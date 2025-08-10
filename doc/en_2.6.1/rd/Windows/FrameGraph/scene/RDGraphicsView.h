#pragma once

#include <QGraphicsView>
class QGVNode;
class QGVScene;

namespace QtNodes
{
    class BasicGraphicsScene;
}

class RDGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    RDGraphicsView(QWidget *parent);
    ~RDGraphicsView();

    void selectNode(QGVNode *node);

    void centerScene();

protected:
  virtual void wheelEvent(QWheelEvent *event) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void drawBackground(QPainter *painter, const QRectF &r) override;
  void showEvent(QShowEvent *event) override;

  QtNodes::BasicGraphicsScene *nodeScene();
private:

private slots:
  void nodeDoubleClick(QGVNode *node);
  
private:

};

