#pragma once

#include "Code/Interface/QRDInterface.h"
#include "renderdoc_replay.h"
#include "Code/QRDUtils.h"

#include <QMap>
#include <QSet>

#include <QAction>
#include <QScreen>
#include <QtWidgets/QApplication>
#include <QPointF>

#include "scene/FrameGraphModel.h"

struct RenderPass
{
  uint32_t id;
  uint32_t start, end;
  ResourceId FBO;
  rdcfixedarray<ResourceId, 8> outputs;
  const ActionDescription *passAction;
  uint32_t effectiveEventId;

  QSet<ResourceId> readAttchment;
  QSet<ResourceId> drawAttchment;

  QSet<ResourceId> dependReadAttchment;
  QSet<ResourceId> dependDrawAttchment;

  RenderPass() : passAction(NULL), start(0), end(0), effectiveEventId(0) {}

  bool IsFrameEnd() const
  {
    if(!passAction)
      return false;
    return (passAction->flags & ActionFlags::Present) == ActionFlags::Present;
  }

  bool operator==(const RenderPass u) const { return start == u.start && end == u.end; }
  bool operator!=(const RenderPass u) const { return start != u.start && end != u.end; }
  bool operator<(const RenderPass u) const { return start < u.start || end < u.end; }
};

struct ResourceDepend
{
  uint32_t fromPassEID;    // effective EID
  uint32_t toPassEID;      // effective EID
  ResourceId resourceId;
  bool isColorTarget;
  EventUsage FromUsage;    // usage.eventId != effective EID
  EventUsage toUsage;      // usage.eventId != effective EID

  ResourceDepend() : fromPassEID(0), toPassEID(0), isColorTarget(true) {}

  bool operator==(const ResourceDepend u) const
  {
    return fromPassEID == u.fromPassEID && toPassEID == toPassEID && resourceId == resourceId;
  }
  bool operator!=(const ResourceDepend u) const
  {
    return fromPassEID != u.fromPassEID && toPassEID != toPassEID && resourceId != resourceId;
  }

  bool operator<(const ResourceDepend u) const
  {
    return fromPassEID < u.fromPassEID && toPassEID <= toPassEID;
  }
};

class FrameGraphDataManage
{

public:
  explicit FrameGraphDataManage(ICaptureContext &ctx);
  virtual ~FrameGraphDataManage();

  void Build();
  void Clear();

  uint32_t effectiveEID(uint32_t eid);

  QString ExportSimpleDot();
  QString ExportPassAndAttachmnetDOT();

  QString GetResourceUsageInfoByPass(uint32_t effectiveEID, ResourceId resId);
  QVector<const RenderPass *> GetEndPass();

  void TransitionGraphModel(FrameGraphModel& model);

private:
  QString GetPassTargetInfo(const RenderPass &pass);
  QString NodePassLabel(const RenderPass &pass);

  void CollectRenderPasses(IReplayController *r);
  void CollectPassResourcesUsages(IReplayController *r);

  void BuildSimpleEdges();

  // If an PS-Texture attachment of a renderPass has multiple sources
  // the most recent renderPass is used as the source.
  bool AddDependEdge(const ResourceDepend &depend);
 

  bool IsOutputUsage(ResourceUsage usage);
  bool IsInputUsage(ResourceUsage usage);
  bool IsColorTarget(ResourceId id, const RenderPass &usedPass);
  EventUsage GetPassUsedResourceUsage(uint32_t usedPassEID, ResourceId id);

  bool beDepended(const RenderPass &pass);
  uint32_t getFrameNumberOfRenderpass(const RenderPass &pass);

  const RenderPass *GetPass(uint32_t effectiveEid);
 
  QString GenerateDetaliedDOT();
  bool IsUsedReourceBetweenPass(const ResourceId &id);


private:

  ICaptureContext& m_Ctx;

  // pass effective EID container
  QVector<uint32_t> m_cachePassRange;
  // <frame number, frame end eid>
  QMap<uint32_t, uint32_t> m_frameIndexInCacheRange;

  // simple view
  QVector<RenderPass> m_renderPass;
  QVector<ResourceDepend> m_dependEdges;
  QMap<uint32_t, rdcarray<EventUsage>> m_renderPassUsages;    //<passEffectiveEID, Pass Resource EventUsage>

  float m_progress = 0.0f;

};

