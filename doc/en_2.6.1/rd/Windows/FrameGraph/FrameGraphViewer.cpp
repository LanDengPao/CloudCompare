#include "FrameGraphViewer.h"
#include "ui_FrameGraphViewer.h"
#include "FrameGraphTooltip.h"
#include "FrameGraphDataManage.h"
#include "scene/FrameGraphScene.h"

#include "QGVScene.h"
#include "QGVNode.h"
#include "QGVEdge.h"
#include "QGVSubGraph.h"
#include "frameGraph/FGSimpleEdge.h"
#include "frameGraph/FGSimplePassNode.h"
#include "Widgets/CustomPaintWidget.h"

#include "GraphicsView.hpp"
#include "scene/FrameGraphModel.h"

#include <QWheelEvent>
#include <type_traits>
#include <algorithm>
#include <QStandardPaths>
#include <QApplication>
#include <QSvgGenerator>
#include <QMainWindow>


FrameGraphViewer::FrameGraphViewer(ICaptureContext &ctx, QWidget *parent) 
  : QFrame(parent), 
  ui(new Ui::FrameGraphViewer), 
  m_Ctx(ctx), 
  m_FGScene(Q_NULLPTR), m_FGModel(Q_NULLPTR)
{
    ui->setupUi(this);
    m_dataManage = new FrameGraphDataManage(m_Ctx);
   
    // display init
    m_TexDisplay.backgroundColor = FloatVector();
    m_TexDisplay.backgroundColor.w = 1.0f;
    m_TexDisplay.scale = -1.0f; // auto-fit and center scale
    m_TexDisplay.red = true;
    m_TexDisplay.green = true;
    m_TexDisplay.blue = true;
    m_TexDisplay.alpha = true;

    CustomPaintWidget *thumbnail = new CustomPaintWidget(this);
    thumbnail->SetContext(m_Ctx);
    thumbnail->hide(); 
    
    m_Tooltip = new FrameGraphTooltip(this, thumbnail, m_Ctx);

    SetupUI();
    
    m_Ctx.AddCaptureViewer(this);
}

FrameGraphViewer::~FrameGraphViewer()
{
    m_Ctx.BuiltinWindowClosed(this);
    m_Ctx.RemoveCaptureViewer(this);

    ui->graphicsView->setScene(nullptr);
    delete m_scene;
    delete m_Tooltip;
    delete ui;
}

QWidget *FrameGraphViewer::Widget()
{
    return this;
}

void FrameGraphViewer::SetupUI()
{
  ui->SelectCBox->hide();

  m_scene = new QGVScene(lit("Frame Graph"), this);
  ui->graphicsView->setScene(m_scene);

  connect(m_scene, &QGraphicsScene::selectionChanged, this, &FrameGraphViewer::selectionChanged);

  //for show tooltipWidget
  ui->graphicsView->viewport()->installEventFilter(this);
  ui->stackedWidget->setCurrentIndex(0);
  //auto view = ui->graphicsViewNodes;
  //view->setContextMenuPolicy(Qt::ActionsContextMenu);
  //QAction createNodeAction(QStringLiteral("Create Node"), view);
  //QObject::connect(&createNodeAction, &QAction::triggered, [&]() {
  //  // Mouse position in scene coordinates.
  //  QPointF posView = view->mapToScene(view->mapFromGlobal(QCursor::pos()));

  //  NodeId const newId = _graphModel.addNode();
  //  _graphModel.setNodeData(newId, NodeRole::Position, posView);
  //});

  //view->addAction(&createNodeAction);
  
}

void FrameGraphViewer::OnCaptureLoaded()
{
  if(!m_Tooltip)
    return;

  CustomPaintWidget *thumbnail = m_Tooltip->findChild<CustomPaintWidget*>();
  if(!thumbnail) 
    return;

  WindowingData thumbData = thumbnail->GetWidgetWindowingData();

  m_Ctx.Replay().AsyncInvoke([thumbData, this](IReplayController *r) {
    m_Output = r->CreateOutput(thumbData, ReplayOutputType::Texture);
        
    CustomPaintWidget *thumbnail = m_Tooltip->findChild<CustomPaintWidget*>();
    if(thumbnail)
      thumbnail->SetOutput(m_Output);

    RT_UpdateAndDisplay(r);
  });
  
  ui->exportSvgBtn->show();

  BuildFrameGraph();
}

void FrameGraphViewer::OnCaptureClosed()
{
  m_scene->clear();
  
  m_Tooltip->hide();
  
  ui->exportSvgBtn->hide();
  
  m_dataManage->Clear();

  if(m_FGScene)
  {
    delete m_FGScene;
    m_FGScene = NULL;
  }

}

void FrameGraphViewer::OnEventChanged(uint32_t eventId)
{
  uint32_t EID = m_dataManage->effectiveEID(eventId);

  if(m_CurSelectedEID == EID) 
    return;

  auto node = m_scene->findNodeForEid(EID);
  if(!node)
    return;

  m_CurSelectedEID = EID;
  m_scene->clearSelection();
  node->setSelected(true);
  ui->graphicsView->ensureVisible(node);
}

void FrameGraphViewer::BuildFrameGraph()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  m_dataManage->Build();

  // Data => View::FrameGraphModel
  if(m_FGScene)
    delete m_FGScene;
 
  m_FGModel = new FrameGraphModel();

  m_dataManage->TransitionGraphModel(*m_FGModel);
  
  m_FGScene = new FrameGraphScene(*m_FGModel, this);

  m_FGModel->setNodesLocked(true);


  updateViewForType(m_viewType);

}

void FrameGraphViewer::updateViewForType(ViewType type)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  GUIInvoke::call(this, [this, type]() {
    ui->stackedWidget->setCurrentIndex(0);
    ui->graphicsView->setScene(m_scene);
    if(ViewType::DETAILED == type)
    {
      m_scene->loadLayout(m_dataManage->ExportPassAndAttachmnetDOT(),
                          m_layoutAlg.toLocal8Bit().constData());
    }
    else if(ViewType::SIMPLE == type)
    {
      m_scene->loadSimpleLayout(m_dataManage->ExportSimpleDot(),
                                m_layoutAlg.toLocal8Bit().constData());
    }
    else if(ViewType::QTNODES == type)
    {
      ui->graphicsView->setScene(m_FGScene);
    }

    AddEndPassImage();

    uint32_t eid = m_CurSelectedEID;
    m_CurSelectedEID = 0;
    OnEventChanged(eid);
  });
}

void FrameGraphViewer::updateGraphView()
{
  
}

QString FrameGraphViewer::GetToolTipTextureInfo(ResourceId resId, const QString &usage)
{
  if(resId == ResourceId())
    return tr("Empty Resource");

  TextureDescription *tex = m_Ctx.GetTexture(resId);
  if(!tex)
    return tr("Unknown Resource");

  QString format = tex->format.Name();

  // 添加输出端的usage信息
  QString info;
  if(!usage.isEmpty())
    info += tr("%1\n").arg(usage);

  info += tr("Format: %1\n").arg(format);

  // 如果是depth/stencil纹理，添加特殊标记
  if(tex->format.type == ResourceFormatType::D16S8 ||
     tex->format.type == ResourceFormatType::D24S8 || tex->format.type == ResourceFormatType::D32S8)
    info += tr("Depth-Stencil Format\n");
  else if(tex->format.compType == CompType::Depth)
    info += tr("Depth Format\n");

  // SRGB format
  if(tex->format.SRGBCorrected())
    info += tr("SRGB Format\n");

  // 多重采样信息
  if(tex->msSamp > 1)
    info += tr("MSAA Samples: %1\n").arg(tex->msSamp);

  return info.trimmed();
}

rdcstr FrameGraphViewer::GetActionName(const ActionDescription &action)
{
  const SDFile &sdfile = m_Ctx.GetStructuredFile();
  return action.GetName(sdfile);
}

rdcstr FrameGraphViewer::FormatResourceUsage(ResourceUsage usage)
{
    return ToQStr(usage);
}

void FrameGraphViewer::OnExportDot()
{
  QString fileName = QFileDialog::getOpenFileName(this,
    lit("Export Dot"),
    lit("%1").arg(QStandardPaths::DesktopLocation));

  QFile file(fileName);
  if(file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QTextStream out(&file);
    m_viewType == DETAILED 
      ? out << m_dataManage->ExportPassAndAttachmnetDOT()
      : out << m_dataManage->ExportSimpleDot();
    file.close();
  }
}

void FrameGraphViewer::selectionChanged()
{
  auto items = m_scene->selectedItems();
  if(items.isEmpty())
    return;

  if(items[0]->type() == QGVNode::Type 
    || items[0]->type() == FGSimplePassNode::Type)    // node
  {
    QGVNode *node = dynamic_cast<QGVNode *>(items[0]);
    if(!node) return;
    
    if(NodeType::PASS == node->nodeType())
    {
      auto eid = node->EID();
      if(m_CurSelectedEID == eid) return;
      m_Ctx.SetEventID({this}, eid, eid);
      m_CurSelectedEID = eid;
    }
    else if(NodeType::RESOURCE == node->nodeType())
    {
      // ResourceInspector view
      QString resourceIdStr = node->getAttribute(lit("resourceId"));
      if(resourceIdStr.isEmpty())
        return;
      ResourceId resId;
      if(resourceIdStr.startsWith(lit("ResourceId::")))
      {
        qulonglong num = resourceIdStr.mid(sizeof("ResourceId::") - 1).toULongLong();
        memcpy(&resId, &num, sizeof(num));
      }

      ITextureViewer *viewer = m_Ctx.GetTextureViewer();
      viewer->ViewTexture(resId, CompType::Typeless, true);
      return;
    }
  }
  else if(items[0]->type() == QGVEdge::Type)    // edge
  {
    // resource viewer
    QGVEdge *edge = dynamic_cast<QGVEdge *>(items[0]);
    if(!edge) return;
    QString resourceIdStr = edge->getAttribute(lit("resourceId"));
    if(resourceIdStr.isEmpty())
      return;
    ResourceId resId;
    if(resourceIdStr.startsWith(lit("ResourceId::")))
    {
      qulonglong num = resourceIdStr.mid(sizeof("ResourceId::") - 1).toULongLong();
      memcpy(&resId, &num, sizeof(num));
    }

    auto resIns = m_Ctx.GetResourceInspector();
    resIns->Inspect(resId);
    return;
  }
  else if(items[0]->type() == FGSimpleEdge::Type)    // edge
  {
    // texure viewer and set event
    FGSimpleEdge *edge = dynamic_cast<FGSimpleEdge *>(items[0]);
    if(!edge)
      return;

    QString resourceIdStr = edge->getAttribute(lit("resourceId"));
    if(resourceIdStr.isEmpty())
      return;
    ResourceId resId;
    if(resourceIdStr.startsWith(lit("ResourceId::")))
    {
      qulonglong num = resourceIdStr.mid(sizeof("ResourceId::") - 1).toULongLong();
      memcpy(&resId, &num, sizeof(num));
    }

    ITextureViewer *viewer = m_Ctx.GetTextureViewer();
    viewer->ViewTexture(resId, CompType::Typeless, false);

    auto resIns = m_Ctx.GetResourceInspector();
    resIns->Inspect(resId);

    uint32_t eid = edge->getAttribute(lit("fromEid")).toUInt();
    
    auto node = m_scene->findNodeForEid(eid);
    if(node)
      node->setSelected(true);

    m_Ctx.SetEventID({this}, eid, eid);
    return;

  }

}
void FrameGraphViewer::nodeDoubleClick(QGVNode *node)
{
  if(!node) return;
  if(NodeType::PASS == node->nodeType() || FGSimplePassNode::Type == node->type())
  {
    //showNodeInformation(node);
  }
}

void FrameGraphViewer::on_exportSvgBtn_clicked(bool checked)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  QString filename =
      QFileDialog::getSaveFileName(nullptr, lit("Save SVG"), lit(""), lit("SVG files (*.svg)"));
  if(!filename.isEmpty())
  {
    if(ExportSceneToSvg(m_scene, filename))
      QMessageBox::information(nullptr, lit("Saved"), lit("Scene saved to SVG file successfully!"));
    else
      QMessageBox::information(nullptr, lit("Save Failed"),
                               lit("Frame Graph saved to SVG file Failed!"));
  }
}

void FrameGraphViewer::on_viewCBox_activated(int index)
{
  switch(index)
  {
    case 0: m_viewType = ViewType::SIMPLE; break;
    case 1: m_viewType = ViewType::DETAILED; break;
    default: m_viewType = ViewType::QTNODES; break;
  }

  updateViewForType(m_viewType);
}

void FrameGraphViewer::on_SelectCBox_activated(int index)
{
  m_scene->updateSelectType(index);
}

void FrameGraphViewer::on_layoutEdit_textEdited(const QString& text)
{
  m_layoutAlg = text;
  updateViewForType(m_viewType);
}

void FrameGraphViewer::RT_UpdateAndDisplay(IReplayController *r)
{
  if(m_Output != NULL)
  {
    m_Output->SetTextureDisplay(m_TexDisplay);

    GUIInvoke::call(this, [this]() { 
      if(m_Tooltip)
      {
        CustomPaintWidget *thumbnail = m_Tooltip->findChild<CustomPaintWidget*>();
        if(thumbnail)
          thumbnail->update(); 
      }
    });
  }
}

ResourceId FrameGraphViewer::updateThumbnail(ResourceId resourceId)
{
  if(resourceId == ResourceId())
    return ResourceId();

  TextureDescription *tex = m_Ctx.GetTexture(resourceId);

  if(tex)
  {
    m_TexDisplay.resourceId = resourceId;
    
    m_Ctx.Replay().AsyncInvoke(lit("framegraph_thumbnail"), [this](IReplayController *r) {
      RT_UpdateAndDisplay(r);
    });

    float aspect = (float)tex->width / (float)qMax(1U, tex->height);

    if(m_Tooltip)
    {
      CustomPaintWidget *thumbnail = m_Tooltip->findChild<CustomPaintWidget*>();
      if(thumbnail)
      {
        thumbnail->setFixedSize((int)qBound(100.0f, aspect * 100.0f, (21.0f / 9.0f) * 100.0f), 100);
        thumbnail->show();
      }
    }
  }
  else
  {
    if(m_Tooltip)
    {
      CustomPaintWidget *thumbnail = m_Tooltip->findChild<CustomPaintWidget*>();
      if(thumbnail)
        thumbnail->hide();
    }
  }

  return resourceId;
}

bool FrameGraphViewer::hasThumbnail(ResourceId resourceId)
{
  if(resourceId == ResourceId())
    return false;

  TextureDescription *tex = m_Ctx.GetTexture(resourceId);
  if(!tex)
    return false;

  if(m_Ctx.APIProps().remoteReplay)
    return true;

  return m_Output != nullptr;
}

bool FrameGraphViewer::eventFilter(QObject *obj, QEvent *event)
{
  if(event->type() == QEvent::MouseMove)
  {
    if(obj != ui->graphicsView->viewport() && !m_scene)
    {
    eventFilterQuit:
      m_Tooltip->hideTip();
      QFrame::eventFilter(obj, event);
      return false;
    }
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    
    QPointF scenePos = ui->graphicsView->mapToScene(mouseEvent->pos());
    QGraphicsItem *item = m_scene->itemAt(scenePos, ui->graphicsView->transform());
    if(!item)
      goto eventFilterQuit;

    bool ret = false;
    QPoint globalPos = ui->graphicsView->mapToGlobal(mouseEvent->pos());
    if(item->type() == FGSimpleEdge::Type)
    {
      FGSimpleEdge *edge = dynamic_cast<FGSimpleEdge *>(item);
      ret = showItemToolTip(edge, globalPos);
    }
    else if(item->type() == QGVEdge::Type)
    {
      QGVEdge *edge = dynamic_cast<QGVEdge *>(item);
      ret = showItemToolTip(edge, globalPos);
    }
    else if(item->type() == QGVNode::Type)
    {
      QGVNode *node = dynamic_cast<QGVNode *>(item);
      ret = showItemToolTip(node, globalPos);
    }

    if(!ret)
      goto eventFilterQuit;

    return true;
  }

  return QFrame::eventFilter(obj, event);
}

bool FrameGraphViewer::showItemToolTip(QGVEdge *edge, QPoint globelPos)
{
  if(!edge)
    return false;

  QString resourceIdStr = edge->getAttribute(lit("resourceId"));
  if(resourceIdStr.isEmpty())
    return false;
  // ResourceId format: "ResourceId::Number"
  ResourceId resId;
  if(resourceIdStr.startsWith(lit("ResourceId::")))
  {
    qulonglong num = resourceIdStr.mid(sizeof("ResourceId::") - 1).toULongLong();
    memcpy(&resId, &num, sizeof(num));
  }

  QString tooltipText;
  if(m_Tooltip->hasThumbnail(resId))
  {
    uint32_t f = edge->getAttribute(lit("fromEid")).toUInt();
    uint32_t to = edge->getAttribute(lit("toEid")).toUInt();
    QString outputUsage =
        m_dataManage->GetResourceUsageInfoByPass(f, resId) + 
      lit("\n") + m_dataManage->GetResourceUsageInfoByPass(to, resId);

    tooltipText = GetToolTipTextureInfo(resId, outputUsage);
  }
  else
  {
    tooltipText = QString{lit("No Thumbnail")};
  }
  QSize tipSize = m_Tooltip->configureTip(resId, tooltipText);

  globelPos.setY(globelPos.y() - tipSize.height() - 10);

  m_Tooltip->showTip(globelPos);

  return true;
}

bool FrameGraphViewer::showItemToolTip(FGSimpleEdge *edge, QPoint globelPos)
{
  if(!edge)
    return false;

  QString resourceIdStr = edge->getAttribute(lit("resourceId"));
  if(resourceIdStr.isEmpty())
    return false;
  // ResourceId format: "ResourceId::Number"
  ResourceId resId;
  if(resourceIdStr.startsWith(lit("ResourceId::")))
  {
    qulonglong num = resourceIdStr.mid(sizeof("ResourceId::") - 1).toULongLong();
    memcpy(&resId, &num, sizeof(num));
  }

  QString tooltipText;
  if(m_Tooltip->hasThumbnail(resId))
  {
    uint32_t f = edge->getAttribute(lit("fromEid")).toUInt();
    uint32_t to = edge->getAttribute(lit("toEid")).toUInt();
    QString outputUsage = m_dataManage->GetResourceUsageInfoByPass(f, resId) + lit("\n") +
                          m_dataManage->GetResourceUsageInfoByPass(to, resId);
   
    tooltipText = GetToolTipTextureInfo(resId, outputUsage);
  }
  else
  {
    tooltipText = QString{lit("No Thumbnail")};
  }
  QSize tipSize = m_Tooltip->configureTip(resId, tooltipText);

  globelPos.setY(globelPos.y() - tipSize.height() - 10);

  m_Tooltip->showTip(globelPos);

  return true;
}

bool FrameGraphViewer::showItemToolTip(QGVNode *node, QPoint globelPos)
{
  if(!node || node->nodeType() != NodeType::RESOURCE)
    return false;

  QString resourceIdStr = node->getAttribute(lit("resourceId"));
  if(resourceIdStr.isEmpty())
    return false;
  // ResourceId format: "ResourceId::Number"
  ResourceId resId;
  if(resourceIdStr.startsWith(lit("ResourceId::")))
  {
    qulonglong num = resourceIdStr.mid(sizeof("ResourceId::") - 1).toULongLong();
    memcpy(&resId, &num, sizeof(num));
  }

  QString tooltipText;
  if(m_Tooltip->hasThumbnail(resId))
  {
    QString usageInfo = node->getAttribute(lit("usage"));
    tooltipText = GetToolTipTextureInfo(resId, usageInfo);
  }
  else
  {
    tooltipText = QString{lit("No Thumbnail")};
  }

  QSize tipSize = m_Tooltip->configureTip(resId, tooltipText);

  globelPos.setY(globelPos.y() - tipSize.height() - 10);

  m_Tooltip->showTip(globelPos);

  return true;
}

bool FrameGraphViewer::IsTextureResource(ResourceId resId)
{
  if(resId == ResourceId())
    return false;
    
  TextureDescription *tex = m_Ctx.GetTexture(resId);
  return tex != nullptr;
}

bool FrameGraphViewer::ExportSceneToSvg(QGraphicsScene *scene, const QString &filename)
{
  if(!scene) return false;
  scene->clearSelection();
  QSvgGenerator generator;
  generator.setFileName(filename);
  generator.setSize(scene->sceneRect().size().toSize());
  generator.setViewBox(scene->sceneRect());
  generator.setTitle(lit("SVG Generated using QGraphicsScene"));
  generator.setDescription(lit("This SVG file was generated from a QGraphicsScene"));

  QPainter painter;
  painter.begin(&generator);
  scene->render(&painter);
  painter.end();

  return true;
}

void FrameGraphViewer::AddEndPassImage()
{
  auto endpass = m_dataManage->GetEndPass();
  // add pass
  for(const RenderPass* &pass : endpass)
  {
    auto node = m_scene->findNodeForEid(pass->effectiveEventId);
    if(!node)
      continue;
    ResourceId id = *pass->outputs.begin();
    TextureDescription *tex = m_Ctx.GetTexture(id);
    if(!tex) continue;

    Subresource sub = {tex->mips, 0, ~0U};
    CompType typeCast = tex->format.compType;
    QSize s{(int)tex->width, (int)tex->height};
    
    m_Ctx
      .Replay()
      .AsyncInvoke(
        [this, node, s, sub, id, typeCast](IReplayController *) 
        {
          m_TexDisplay.resourceId = id;
          m_Output->SetTextureDisplay(m_TexDisplay);

          bytebuf data = m_Output->DrawThumbnail(s.width(), s.height(), id,
                                                sub, typeCast);
          // new and swap to move the data into the lambda
          bytebuf *copy = new bytebuf;
          copy->swap(data);
          GUIInvoke::call(this, [s, node, copy]() {
            QImage thumb(copy->data(), s.width(), s.height(),
                        s.width() * 3, QImage::Format_RGB888);
            node->setIcon(thumb);
            delete copy;
          });
        });
  }
}
