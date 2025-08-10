#pragma once

#include <QFrame>

class FrameGraphViewer;
class CustomPaintWidget;

class FrameGraphTooltip : public QFrame
{
  Q_OBJECT

public:
  explicit FrameGraphTooltip(FrameGraphViewer *parent, CustomPaintWidget *thumbnail,
                             ICaptureContext &ctx);

  void hideTip();
  QSize configureTip(ResourceId resourceId, QString text);
  void showTip(QPoint pos);
  bool hasThumbnail(ResourceId resourceId);
  void update() ;


protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

private:
  FrameGraphViewer *m_FrameGraphViewer = NULL;
  QLabel *title = NULL;
  QLabel *label = NULL;
  ICaptureContext &m_Ctx;
};
