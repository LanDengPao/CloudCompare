#pragma once
#include "Code/Interface/QRDInterface.h"
#include "renderdoc_replay.h"
#include "stringise.h"
#include "Code/QRDUtils.h"

#include "3rdparty/QGVCore/QGVScene.h"
#include "3rdparty/QGVCore/QGVNode.h"
#include "3rdparty/QGVCore/QGVEdge.h"
#include "replay_enums.h"

#include "BasicGraphicsScene.hpp"
#include "scene/FrameGraphModel.h"

#include <QFrame>
#include <QGraphicsView>
#include <QGraphicsScene>

namespace Ui
{
class FrameGraphViewer;
};

class FrameGraphScene;
class FrameGraphDataManage;
class FrameGraphTooltip;
class FrameGraphViewer : public QFrame, public IFrameGraphViewer, public ICaptureViewer
{
  Q_OBJECT

  enum ViewType
  {
    SIMPLE = 0,
    DETAILED,
    QTNODES
  };

public:
  explicit FrameGraphViewer(ICaptureContext &ctx, QWidget *parent = 0);
  ~FrameGraphViewer();

   // IFrameGraphViewer
  virtual QWidget *Widget() override;
  void BuildFrameGraph() override;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) {};
  void OnEventChanged(uint32_t eventId) override;

  //toolTip view
  ResourceId updateThumbnail(ResourceId resourceId);
  bool hasThumbnail(ResourceId resourceId);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  bool showItemToolTip(QGVEdge *item, QPoint globelPos);
  bool showItemToolTip(FGSimpleEdge *item, QPoint globelPos);
  bool showItemToolTip(QGVNode *item, QPoint globelPos);

private:
  void SetupUI();
 
  rdcstr GetActionName(const ActionDescription &action);
  rdcstr FormatResourceUsage(ResourceUsage usage);
  
  QString GetToolTipTextureInfo(ResourceId resId, const QString &usage);

  void updateViewForType(ViewType type);
  void updateGraphView();

  void RT_UpdateAndDisplay(IReplayController *r);
  bool IsTextureResource(ResourceId resId);

  bool ExportSceneToSvg(QGraphicsScene *scene, const QString &filename);
  void AddEndPassImage();

private slots:
  void OnExportDot();

  void nodeDoubleClick(QGVNode *node);
  void selectionChanged();

  void on_exportSvgBtn_clicked(bool checked = false);
  void on_viewCBox_activated(int index);
  void on_SelectCBox_activated(int index);
  void on_layoutEdit_textEdited(const QString &);

private:
  Ui::FrameGraphViewer *ui;
  ICaptureContext& m_Ctx;
  FrameGraphDataManage *m_dataManage;

  QString m_layoutAlg = lit("dot");

  float m_progress = 0.0f;

  ViewType m_viewType = ViewType::SIMPLE;

  QGVScene *m_scene;

  FrameGraphScene *m_FGScene;
  FrameGraphModel *m_FGModel;

  uint32_t m_CurSelectedEID;
  // thumbnail view
  FrameGraphTooltip *m_Tooltip = NULL;
  
  TextureDisplay m_TexDisplay;
  IReplayOutput *m_Output = NULL;
};

