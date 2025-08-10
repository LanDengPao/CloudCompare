#include "FrameGraphDataManage.h"

#include "QGVNode.h" //NodeType

#include <QMainWindow>

static uint qHash(const ResourceId &key, uint seed = 0)
{
  return qHash(ToQStr(key), seed);
}

#define FAST_BUILD_FRAMEGRAPH
#define CR_EDGEREAD lit("#4CAF50")
#define CR_EDGEDRAW lit("#FF5722")
#define CR_PASSNODE lit("#e69f00")
#define CR_RESOURCENODE lit("#56b4e9")
#define CR_ENDPASS lit("#2ab574")


void FrameGraphDataManage::Clear()
{
  m_renderPass.clear();
  m_dependEdges.clear();
  m_renderPassUsages.clear();
  m_cachePassRange.clear();
}

FrameGraphDataManage::FrameGraphDataManage(ICaptureContext &ctx) 
  : m_Ctx(ctx)
{

}

FrameGraphDataManage::~FrameGraphDataManage()
{

}

void FrameGraphDataManage::Build()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  // convert from the currently open cap to the destination
  LambdaThread *th = new LambdaThread([this]() {
    m_Ctx.Replay().BlockInvoke([this](IReplayController *r) {
      
      Clear();

      CollectRenderPasses(r);

      CollectPassResourcesUsages(r);

      BuildSimpleEdges();

      m_progress = 1;
    });
  });

  th->setName(lit("BuildFrameGraph"));
  th->start();
  // wait a few ms before popping up a progress bar
  th->wait(500);
  if(th->isRunning())
  {
    ShowProgressDialog(
        dynamic_cast<QMainWindow *>(m_Ctx.GetMainWindow()), lit("Build Frame Graph."),
        [th]() { return !th->isRunning(); }, [this]() { return m_progress; });
  }
  th->deleteLater();
}

void FrameGraphDataManage::CollectRenderPasses(IReplayController *r)
{
  uint32_t frameNumber = m_Ctx.FrameInfo().frameNumber;
  const rdcarray<ActionDescription> &rootActions = m_Ctx.CurRootActions();
  if(rootActions.isEmpty())
    return;

  rdcarray<const ActionDescription *> allActions;
  for(const auto &action : rootActions)
  {
    if(action.children.isEmpty())
      allActions.push_back(&action);
    else
      for(const auto &child : action.children)
        allActions.push_back(&child);
  }

  uint passNum = 1;
  size_t len = allActions.size();
  int startIndex = 0, endIndex = 0;
  for(int i = 0; i < len; i++)
  {
    m_progress = (float)i / (float)len * 0.7f;
    bool isFrameEnd =
        len - 1 == i || (ActionFlags::Present == (allActions[i]->flags & ActionFlags::Present));
    bool onePass = allActions[startIndex]->outputs == allActions[i]->outputs &&
                   allActions[startIndex]->depthOut == allActions[i]->depthOut;
    if(onePass && !isFrameEnd)
      continue;

    endIndex = isFrameEnd ? i : i - 1;    // file end must be frameEnd

    RenderPass pass;
    const ActionDescription *action = allActions[endIndex];
    pass.start = allActions[startIndex]->eventId;
    pass.end = action->eventId;
    pass.outputs = action->outputs;
    pass.passAction = action;
    pass.effectiveEventId = pass.end;
    pass.id = passNum;
    pass.FBO = ResourceId();

#ifndef FAST_BUILD_FRAMEGRAPH
    r->SetFrameEvent(pass.end, false);
    if(r->GetAPIProperties().pipelineType == GraphicsAPI::OpenGL)
      pass.FBO = r->GetGLPipelineState()->framebuffer.drawFBO.resourceId;
    else if(r->GetAPIProperties().pipelineType == GraphicsAPI::Vulkan)
      pass.FBO = r->GetVulkanPipelineState()->currentPass.framebuffer.resourceId;
#endif

    m_renderPass.append(pass);

    m_cachePassRange.push_back(pass.effectiveEventId);

    if(isFrameEnd)
    {
      passNum = 1;
      startIndex = i + 1;
      frameNumber++;
      m_frameIndexInCacheRange.insert(frameNumber, pass.effectiveEventId);
    }
    else
    {
      passNum++;
      startIndex = i;
    }
  }

  // for safe
  std::sort(m_renderPass.begin(), m_renderPass.end(),
            [](const RenderPass &o1, const RenderPass &o2) { return o1 < o2; });
}


void FrameGraphDataManage::CollectPassResourcesUsages(IReplayController *r)
{
  const rdcarray<TextureDescription> &textures = m_Ctx.GetTextures();
  int i = 0, all = (int)textures.size();

  for(const TextureDescription &tex : textures)
  {
    m_progress = i++ / (float)all * 0.1f + 0.7f;

    ResourceId resId = tex.resourceId;
    rdcarray<EventUsage> allUsages = r->GetUsage(resId);
    rdcarray<EventUsage> usages;
    CombineUsageEvents(m_Ctx, allUsages,
                       [this, &usages, resId](uint32_t startEID, uint32_t endEID, ResourceUsage use) {
                         if(startEID == endEID)
                           usages.push_back(EventUsage(endEID, use, resId));
                         else
                         {
                           usages.push_back(EventUsage(startEID, use, resId));
                           usages.push_back(EventUsage(endEID, use, resId));
                         }
                       });

    for(EventUsage &it : usages)
    {
      // find parent pass
      for(auto &pass : m_renderPass)
      {
        if(it.eventId > pass.end || it.eventId < pass.start)
          continue;

        if(IsInputUsage(it.usage))
          pass.readAttchment.insert(resId);
        else
          pass.drawAttchment.insert(resId);

        rdcarray<EventUsage> value = m_renderPassUsages.value(pass.effectiveEventId);
        value.push_back(it);
        m_renderPassUsages.insert(pass.effectiveEventId, value);
        break;
      }
    }
  }
}

void FrameGraphDataManage::BuildSimpleEdges()
{
  size_t len = m_renderPass.size();

  for(int i = 0; i < len; i++)
  {
    RenderPass &currentPass = m_renderPass[i];
    m_progress = i / (float)len * 0.1f + 0.8f;
    // check next pass
    for(int j = i + 1; j < len; j++)
    {
      RenderPass &nextPass = m_renderPass[j];

      for(const ResourceId &outputRes : currentPass.drawAttchment)
      {
        if(!nextPass.readAttchment.contains(outputRes))
          continue;

        ResourceDepend edge;
        edge.fromPassEID = currentPass.effectiveEventId;
        edge.toPassEID = nextPass.effectiveEventId;
        edge.resourceId = outputRes;
        edge.isColorTarget = IsColorTarget(outputRes, currentPass);
        edge.FromUsage = GetPassUsedResourceUsage(currentPass.effectiveEventId, outputRes);
        edge.toUsage = GetPassUsedResourceUsage(nextPass.effectiveEventId, outputRes);
        
        if(AddDependEdge(edge))
        {
          currentPass.dependDrawAttchment.insert(outputRes);
          nextPass.dependReadAttchment.insert(outputRes);
        }
      }

      // if(nextPass.IsFrameEnd())
      // break;
    }
  }
}

bool FrameGraphDataManage::AddDependEdge(const ResourceDepend &newDepend)
{
  int len = (int)m_dependEdges.size();
  for(int i = len - 1; i >= 0; i--)
  {
    if(m_dependEdges[i].toPassEID != newDepend.toPassEID ||
       m_dependEdges[i].resourceId != newDepend.resourceId)
      continue;

    if(m_dependEdges[i].fromPassEID < newDepend.fromPassEID)
      m_dependEdges.replace(i, newDepend);
    return false;
  }
  m_dependEdges.push_back(newDepend);
  return true;
}

bool FrameGraphDataManage::IsColorTarget(ResourceId id, const RenderPass &usedPass)
{
  auto usage = GetPassUsedResourceUsage(usedPass.effectiveEventId, id).usage;
  return usage == ResourceUsage::ColorTarget || usage == ResourceUsage::CopySrc ||
         usage == ResourceUsage::Copy;
}


EventUsage FrameGraphDataManage::GetPassUsedResourceUsage(uint32_t usedPassEID, ResourceId id)
{
  auto usages = m_renderPassUsages.value(usedPassEID);
  rdcarray<EventUsage> used;
  for(const auto &us : usages)
  {
    if(us.view != id)
      continue;
    used.push_back(us);
  }
  return used.isEmpty() ? EventUsage() : used.back();
};

bool FrameGraphDataManage::IsOutputUsage(ResourceUsage usage)
{
  switch(usage)
  {
    case ResourceUsage::ColorTarget:
    case ResourceUsage::DepthStencilTarget:
    case ResourceUsage::StreamOut:
    case ResourceUsage::CopyDst:
    case ResourceUsage::ResolveDst:
    case ResourceUsage::CS_RWResource:
    case ResourceUsage::PS_RWResource:
    case ResourceUsage::VS_RWResource:
    case ResourceUsage::GS_RWResource:
    case ResourceUsage::HS_RWResource:
    case ResourceUsage::DS_RWResource:
    case ResourceUsage::All_RWResource: return true;
    default: return false;
  }
}

bool FrameGraphDataManage::IsInputUsage(ResourceUsage usage)
{
  switch(usage)
  {
    case ResourceUsage::VertexBuffer:
    case ResourceUsage::IndexBuffer:
    case ResourceUsage::VS_Constants:
    case ResourceUsage::HS_Constants:
    case ResourceUsage::DS_Constants:
    case ResourceUsage::GS_Constants:
    case ResourceUsage::PS_Constants:
    case ResourceUsage::CS_Constants:
    case ResourceUsage::All_Constants:
    case ResourceUsage::VS_Resource:
    case ResourceUsage::HS_Resource:
    case ResourceUsage::DS_Resource:
    case ResourceUsage::GS_Resource:
    case ResourceUsage::PS_Resource:
    case ResourceUsage::CS_Resource:
    case ResourceUsage::All_Resource:
    case ResourceUsage::InputTarget:
    case ResourceUsage::CopySrc:
    case ResourceUsage::ResolveSrc: return true;
    default: return false;
  }
}


QString FrameGraphDataManage::ExportSimpleDot()
{
  QString dot;

  dot += lit("digraph SimpleFrameGraph {\n");
  dot += lit("  rankdir=LR;\n");
  // dot += lit("  concentrate=true;\n");

  dot += lit("  node [fontname=\"Arial\"];\n");
  dot += lit("  edge [fontname=\"Arial\"];\n");
  dot += lit("  bgcolor=\"#FFFFFF\";\n");
  dot += lit("  \n");

  dot += lit("  node [shape=box, style=\"filled\", fillcolor=\"%1\", ").arg(CR_PASSNODE);
  dot += lit("fontcolor=\"black\", fontsize=12];\n");
  dot += lit("  \n");

  uint32_t frameNumber = m_Ctx.FrameInfo().frameNumber;
  if(frameNumber == ~0U)
    frameNumber = 1;

  dot += lit("  subgraph cluster_%1 {\n").arg(frameNumber);
  dot += lit("    style=filled;\n");
  dot += lit("    fillcolor=\"#e1e1e1\";\n");
  dot += lit("    color=grey;\n");
  dot += lit("    fontsize=20;\n");
  dot += lit("    label=\"Frame #%1\";\n").arg(frameNumber);

  // add pass
  for(const RenderPass &pass : m_renderPass)
  {
    bool isEndPass = !beDepended(pass);
    // m_Ctx.GetResource(pass.FBO)->type == ResourceType::SwapchainImage;
    QString nodeId = lit("pass_%1").arg(pass.effectiveEventId);
    QString nodeLabel = NodePassLabel(pass);
    uint32_t eid = pass.effectiveEventId;
    ResourceId firstTexture = *pass.drawAttchment.begin();
    TextureDescription *tex = m_Ctx.GetTexture(firstTexture);
    dot += lit("  %1 [tooltip=\"%1\", label=\"%2\", eid=\"%3\", nodeType=\"%4\", passName=\"%5\", "
               "fillcolor =\"%6\", fixedsize=false, height=%7, width=%8];\n")
               .arg(nodeId)
               .arg(nodeLabel)
               .arg(eid)
               .arg(NodeType::PASS)
               .arg(lit("Pass #%1").arg(pass.id))
               .arg(isEndPass ? CR_ENDPASS : CR_PASSNODE);
               //.arg(tex->height / 128.f)
               //.arg(isEndPass ? tex->width / 128.f + 1 : tex->width / 128.f);

    if(pass.IsFrameEnd() && pass != m_renderPass.back())
    {
      dot += lit("  }\n");
      dot += lit("  subgraph cluster_%1 {\n").arg(++frameNumber);
      dot += lit("    style=filled;\n");
      dot += lit("    fontsize=20;\n");
      dot += lit("    fillcolor=\"#e1e1e1\";\n");
      dot += lit("    color=grey;\n");
      dot += lit("    label=\"Frame #%1\";\n").arg(frameNumber);
    }
  }

  dot += lit("  }\n");

  // add edge
  for(const ResourceDepend &edge : m_dependEdges)
  {
    QString fromNodeId = lit("pass_%1").arg(edge.fromPassEID);
    QString toNodeId = lit("pass_%1").arg(edge.toPassEID);

    QString edgeColor = edge.isColorTarget ? CR_EDGEREAD : CR_EDGEDRAW;

    dot += lit("  %1 -> %2 [color=\"%3\", xlabel=\" \", fontsize=8, fromEid=\"%4\", toEid=\"%5\", ")
               .arg(fromNodeId)
               .arg(toNodeId)
               .arg(edgeColor)
               .arg(edge.fromPassEID)
               .arg(edge.toPassEID);
    QString useInfo = lit("EID %1:%2\n")
                          .arg(edge.FromUsage.eventId)
                          .arg(ToQStr(edge.FromUsage.usage, m_Ctx.APIProps().pipelineType)) +
                      lit("EID %1:%2\n")
                          .arg(edge.FromUsage.eventId)
                          .arg(ToQStr(edge.toUsage.usage, m_Ctx.APIProps().pipelineType));
    dot += lit("resourceId=\"%1\"];\n").arg(ToQStr(edge.resourceId));
  }

  dot += lit("  \n");
  dot += lit("}\n");

  return dot;
}

QString FrameGraphDataManage::ExportPassAndAttachmnetDOT()
{
  QString dot;

  dot += lit("digraph SimpleFrameGraph {\n");
  dot += lit("  rankdir=LR;\n");
  dot += lit("  node [fontname=\"Arial\"];\n");
  dot += lit("  edge [fontname=\"Arial\"];\n");
  dot += lit("  bgcolor=\"#FFFFFF\";\n");
  dot += lit("  \n");

  dot += lit("  // Pass�ڵ���ʽ\n");
  dot += lit("  node [shape=box, style=\"rounded,filled\", fillcolor=\"%1\", ").arg(CR_PASSNODE);
  dot += lit("fontcolor=\"black\", fontsize=12];\n");
  dot += lit("  \n");

  uint32_t frameNumber = m_Ctx.FrameInfo().frameNumber;
  if(frameNumber == ~0U)
    frameNumber = 1;

  // add pass
  for(const RenderPass &pass : m_renderPass)
  {
    QString nodeId = lit("pass_%1").arg(pass.effectiveEventId);
    QString nodeLabel = NodePassLabel(pass);
    uint32_t eid = pass.effectiveEventId;
    dot += lit("  %1 [label=\"%2\", eid=\"%3\", nodeType=\"%4\", tooltip=\"%1\",passName=\"%5\", "
               "fillcolor=\"%6\"];\n")
               .arg(nodeId)
               .arg(nodeLabel)
               .arg(eid)
               .arg(NodeType::PASS)
               .arg(lit("Pass #%1").arg(pass.id))
               .arg(beDepended(pass) ? CR_PASSNODE : CR_ENDPASS);
  }

  // add attachment and edge
  for(const ResourceDepend &edge : m_dependEdges)
  {
    // resource
    auto res = m_Ctx.GetResource(edge.resourceId);
    QString displayName = QString::fromUtf8(res->name.c_str());
    QString num = QString(ToStr(res->resourceId)).split(lit("::")).last();
    QString resId = lit("resource_") + num;
    dot += lit("  %1 [label=\"%2\", tooltip=\"%1\", nodeType=\"%3\", resourceId=\"%4\", ")
               .arg(resId)
               .arg(displayName)
               .arg(NodeType::RESOURCE)
               .arg(ToQStr(res->resourceId));

    QString useInfo = GetResourceUsageInfoByPass(edge.fromPassEID, res->resourceId) + lit("\n") +
                      GetResourceUsageInfoByPass(edge.toPassEID, res->resourceId);
    dot += lit(" usage=\"%1\", shape = ellipse, style = filled, fillcolor =\"%2\"];\n")
               .arg(useInfo)
               .arg(CR_RESOURCENODE);

    QString fromNodeId = lit("pass_%1").arg(edge.fromPassEID);
    QString toNodeId = lit("pass_%1").arg(edge.toPassEID);
    QString edgeColor = edge.isColorTarget
                            ? CR_EDGEREAD
                            : CR_EDGEDRAW;    // ��ɫ��colorTarget����ɫ��depth-stencil target
    // write
    dot += lit("  %1 -> %2 [color=\"%3\", xlabel=\" \", fontsize=8, fromEid=\"%4\", toEid=\"%5\", ")
               .arg(fromNodeId)
               .arg(resId)
               .arg(edgeColor)
               .arg(edge.fromPassEID)
               .arg(edge.toPassEID);
    dot += lit("resourceId=\"%1\"];\n").arg(ToQStr(edge.resourceId));

    // read
    dot += lit("  %1 -> %2 [color=\"%3\", xlabel=\" \", fontsize=8, fromEid=\"%4\", toEid=\"%5\", ")
               .arg(resId)
               .arg(toNodeId)
               .arg(edgeColor)
               .arg(edge.fromPassEID)
               .arg(edge.toPassEID);
    dot += lit("resourceId=\"%1\"];\n").arg(ToQStr(edge.resourceId));
  }

  dot += lit("  \n");
  dot += lit("}\n");

  return dot;
}


bool FrameGraphDataManage::beDepended(const RenderPass &pass)
{
  uint32_t EID = pass.effectiveEventId;
  bool sameFrame = false;
  uint32_t frameEndEID = m_frameIndexInCacheRange.value(getFrameNumberOfRenderpass(pass));

  for(const auto &edge : m_dependEdges)
  {
    if(EID == edge.fromPassEID && EID < frameEndEID)
      return true;
  }

  return false;
}

uint32_t FrameGraphDataManage::getFrameNumberOfRenderpass(const RenderPass &pass)
{
  auto it = m_frameIndexInCacheRange.begin();
  while(it != m_frameIndexInCacheRange.end())
  {
    if(it.value() >= pass.effectiveEventId)
      return it.key();
    it++;
  }
  return 0;
}

QString FrameGraphDataManage::GetPassTargetInfo(const RenderPass &pass)
{
  uint colorTarget = 0;
  uint depthTarget = pass.passAction->depthOut == ResourceId() ? 0 : 1;
  for(const auto &resId : pass.outputs)
    if(resId != ResourceId())
      colorTarget++;

  if(!depthTarget && !colorTarget)
    return QString(lit("No Targets"));
  else if(depthTarget && colorTarget)
    return QString(lit("%1 Targets + Depth").arg(colorTarget).arg(depthTarget));
  else if(!depthTarget && colorTarget)
    return QString(lit("%1 Targets").arg(colorTarget));
  else
    return QString(lit("Depth"));
}

QString FrameGraphDataManage::NodePassLabel(const RenderPass &pass)
{
  QString label = lit("Pass %1\n").arg(pass.id);
  // +lit("\n EID: %1-%2").arg(pass.start).arg(pass.end);
  QString targetInfo = GetPassTargetInfo(pass);
  // get Texture Resolution
  QString targetResolution, format;
  if(!pass.drawAttchment.isEmpty())
  {
    ResourceId firstTexture = *pass.drawAttchment.begin();
    TextureDescription *tex = m_Ctx.GetTexture(firstTexture);
    if(tex)
    {
      targetResolution = lit("%1 x %2").arg(tex->width).arg(tex->height);
      // format = tex->format.Name();
    }
  }

  QString fbo = m_Ctx.GetResourceName(pass.FBO);

  /* Node info
  Pass #1 EID[1-10]
  FBO: framebuffer 2
  1 Targets + Depth
  1920 x 1080
  Format
  */
  QString nodeLabel;
  nodeLabel += lit("\n ") + label + lit("\n");
  if(pass.FBO != ResourceId() && !fbo.isEmpty())
    nodeLabel += lit("\n FBO: ") + fbo + lit("\n");
  if(!targetInfo.isEmpty())
    nodeLabel += lit("\n ") + targetInfo + lit("\n");
  if(!targetResolution.isEmpty())
    nodeLabel += lit("\n  ") + targetResolution + lit("\n");
  if(!format.isEmpty())
    nodeLabel += lit("\n Format: ") + format + lit("\n  ");

  return nodeLabel;
}

QString FrameGraphDataManage::GetResourceUsageInfoByPass(uint32_t effectiveEID, ResourceId resId)
{
  // ��ȡ��Pass�����һ����Դʹ����Ϣ
  auto usages = m_renderPassUsages.value(effectiveEID);
  QStringList usageStrs;

  for(int i = (int)usages.size(); i >= 0; i--)
  {
    if(usages[i].view != resId)
      continue;

    QString usageType = ToQStr(usages[i].usage, m_Ctx.APIProps().pipelineType);
    usageStrs << lit("EID %1:").arg(usages[i].eventId) + usageType;
    break;    // only last
  }

  if(usageStrs.isEmpty())
    return lit("No Usage");

  return usageStrs.join(lit("\n"));
}

QVector<const RenderPass *> FrameGraphDataManage::GetEndPass()
{
  QVector<const RenderPass*> ret;

  for(const RenderPass &pass : m_renderPass)
  {
    if(beDepended(pass))
      continue;

    ret.push_back(&pass);
  }

  return ret;
}

void FrameGraphDataManage::TransitionGraphModel(FrameGraphModel &model)
{
  //pass node
  for(const RenderPass& pass : m_renderPass)
  {
    FrameGraphModel::PassNodeInfo info;

    ResourceId firstTexture = *pass.drawAttchment.begin();
    TextureDescription *tex = m_Ctx.GetTexture(firstTexture);

    uint32_t colorTarget = 0;
    for(const auto &resId : pass.outputs)
      if(resId != ResourceId())
        colorTarget++;

    info.name = lit("Pass %1").arg(pass.id);
    info.effectiveEID = pass.effectiveEventId;
    info.width = tex? tex->width : 1;
    info.height = tex ? tex->height : 1;
    info.colorNumber = colorTarget;
    info.depthStencilNumber = pass.passAction->depthOut == ResourceId() ? 0 : 1;

    for(const auto &resId : pass.dependReadAttchment)
      info.readAtt.push_back(m_Ctx.GetResourceName(resId));
    for(const auto &resId : pass.dependDrawAttchment)
      info.drawAtt.push_back(m_Ctx.GetResourceName(resId));
 
    NodeId id = model.addNode(info);
  }
  
  //connection
  for(const auto &edge : m_dependEdges)
  {
    FrameGraphModel::ConnnectionInfo cnn;
    cnn.fromEffectiveEID = edge.fromPassEID;
    cnn.toEffectiveEID = edge.toPassEID;
    cnn.resource = m_Ctx.GetResourceName(edge.resourceId);

    model.addConnection(cnn);
  }
}


uint32_t FrameGraphDataManage::effectiveEID(uint32_t eid)
{
  // if cache
  size_t len = m_cachePassRange.size();
  for(uint32_t i = 0; i < len; i++)
  {
    if(eid <= m_cachePassRange[i])
      return m_cachePassRange[i];
  }

  // else nearest action eid
  auto action = m_Ctx.GetAction(eid);
  if(!action)
    return 0;

  if(action->parent)
    return action->parent->children.back().eventId;

  if(!action->children.isEmpty())
    return action->children.back().eventId;

  auto nextAction = action->next;
  while(nextAction)
  {
    if(action->outputs != nextAction->outputs)
      break;

    action = nextAction;
    nextAction = nextAction->next;
  }

  return action->eventId;
}

const RenderPass *FrameGraphDataManage::GetPass(uint32_t effectiveEid)
{
  for(const auto &pass : m_renderPass)
    if(pass.effectiveEventId == effectiveEid)
      return &pass;

  return NULL;
}


// dicard
QString FrameGraphDataManage::GenerateDetaliedDOT()
{
  QString dot;

  dot += lit("digraph SimpleFrameGraph {\n");
  dot += lit("  rankdir=LR;\n");
  dot += lit("  node [fontname=\"Arial\"];\n");
  dot += lit("  edge [fontname=\"Arial\"];\n");
  dot += lit("  bgcolor=\"#FFFFFF\";\n");
  dot += lit("  \n");

  dot += lit("  // Pass�ڵ���ʽ\n");
  dot += lit("  node [shape=box, style=\"rounded,filled\", fillcolor=\"%1\", ").arg(CR_PASSNODE);
  dot += lit("fontcolor=\"black\", fontsize=12];\n");
  dot += lit("  \n");

  const SDFile &sdfile = m_Ctx.GetStructuredFile();

  for(const RenderPass &pass : m_renderPass)
  {
    QString passNodeId = lit("pass_%1").arg(pass.effectiveEventId);
    QString nodeLabel = NodePassLabel(pass);

    uint32_t eid = pass.effectiveEventId;
    dot +=
        lit("  %1 [label=\"%2\", eid=\"%3\", nodeType=\"%4\", tooltip=\"%1\", passName=\"%5\"];\n")
            .arg(passNodeId)
            .arg(nodeLabel)
            .arg(eid)
            .arg(NodeType::PASS)
            .arg(lit("Pass #%1").arg(pass.id));

    for(const auto &tex : pass.readAttchment)
    {
      const auto &res = m_Ctx.GetResource(tex);
      const auto &eventUsage = GetPassUsedResourceUsage(pass.effectiveEventId, tex);
      if(!IsUsedReourceBetweenPass(tex))
        continue;
      QString displayName = QString::fromUtf8(res->name.c_str());

      QString resId = QString(ToStr(res->resourceId)).split(lit("::")).last();
      QString nodeId = lit("resource_") + resId;
      // resource
      dot += lit("  %1 [label=\"%2\", tooltip=\"%1\", nodeType=\"%3\", resourceId=\"%4\", "
                 "shape=ellipse, style=filled, fillcolor=\"%5\"];\n")
                 .arg(nodeId)
                 .arg(displayName)
                 .arg(NodeType::RESOURCE)
                 .arg(ToQStr(res->resourceId))
                 .arg(CR_RESOURCENODE);

      QString usageLabel = ToQStr(eventUsage.usage, m_Ctx.APIProps().pipelineType);
      QString tooltipInfo;    //= GetTextureInfo(eventUsage.view, pass.effectiveEventId);
      dot += lit("  %1 -> %2 [color=\"%7\", xlabel=\"%3\", fontsize=6, tooltip=\"%4\", "
                 "resourceId=\"%5\", eid=\"%6\"];\n")
                 .arg(nodeId)
                 .arg(passNodeId)
                 .arg(usageLabel)
                 .arg(tooltipInfo)
                 .arg(ToQStr(eventUsage.view))
                 .arg(pass.effectiveEventId)
                 .arg(CR_EDGEREAD);
    }

    for(const auto &tex : pass.drawAttchment)
    {
      const auto &res = m_Ctx.GetResource(tex);
      const auto &eventUsage = GetPassUsedResourceUsage(pass.effectiveEventId, tex);
      if(!IsUsedReourceBetweenPass(tex))
        continue;

      QString displayName = QString::fromUtf8(res->name.c_str());
      QString resId = QString(ToStr(res->resourceId)).split(lit("::")).last();
      QString nodeId = lit("resource_") + resId;

      dot += lit("  %1 [label=\"%2\", tooltip=\"%1\", nodeType=\"%3\", resourceId=\"%4\", "
                 "shape=ellipse, style=filled, fillcolor=\"%5\"];\n")
                 .arg(nodeId)
                 .arg(displayName)
                 .arg(NodeType::RESOURCE)
                 .arg(ToQStr(res->resourceId))
                 .arg(CR_RESOURCENODE);

      QString usageLabel = ToQStr(eventUsage.usage, m_Ctx.APIProps().pipelineType);
      QString tooltipInfo;// = GetTextureInfo(eventUsage.view, pass.effectiveEventId);
      dot += lit("  %1 -> %2 [color=\"%7\", xlabel=\"%3\", fontsize=6, tooltip=\"%4\", "
                 "resourceId=\"%5\", eid=\"%6\"];\n")
                 .arg(passNodeId)
                 .arg(nodeId)
                 .arg(usageLabel)
                 .arg(tooltipInfo)
                 .arg(ToQStr(eventUsage.view))
                 .arg(pass.effectiveEventId)
                 .arg(CR_EDGEDRAW);
    }
  }

  dot += lit("  \n");

  dot += lit("}\n");

  return dot;
}


bool FrameGraphDataManage::IsUsedReourceBetweenPass(const ResourceId &id)
{
  for(const auto &edge : m_dependEdges)
  {
    if(edge.resourceId == id)
      return true;
  }

  return false;
}

