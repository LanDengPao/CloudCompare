#include "RDGraphicsView.h"
#include "BasicGraphicsScene.hpp"
#include "StyleCollection.hpp"

#include "QGVScene.h"
#include "QGVNode.h"
#include "QGVEdge.h"
#include "QGVSubGraph.h"

#include <QWheelEvent>
#include <qmath.h>
#include <QMessageBox>
#include <QMenu>

using QtNodes::BasicGraphicsScene;
using QtNodes::StyleCollection;
using QtNodes::GraphicsViewStyle;
using QtNodes::NodeStyle;
using QtNodes::ConnectionStyle;

namespace QtNodes
{
   void setStyle()
{
  GraphicsViewStyle::setStyle(lit(
      R"(
  {
    "GraphicsViewStyle": {
      "BackgroundColor": [255, 255, 240],
      "FineGridColor": [245, 245, 230],
      "CoarseGridColor": [235, 235, 220]
    }
  }
  )"));

  NodeStyle::setNodeStyle(
      lit(R"(
  {
    "NodeStyle": {
      "NormalBoundaryColor": "darkgray",
      "SelectedBoundaryColor": "deepskyblue",
      "GradientColor0": "mintcream",
      "GradientColor1": "mintcream",
      "GradientColor2": "mintcream",
      "GradientColor3": "mintcream",
      "ShadowColor": [200, 200, 200],
      "ShadowEnabled": true,
      "FontColor": [10, 10, 10],
      "FontColorFaded": [100, 100, 100],
      "ConnectionPointColor": "white",
      "PenWidth": 2.0,
      "HoveredPenWidth": 2.5,
      "ConnectionPointDiameter": 10.0,
      "Opacity": 1.0
    }
  }
  )"));

  ConnectionStyle::setConnectionStyle(
      lit(R"(
  {
    "ConnectionStyle": {
      "ConstructionColor": "gray",
      "NormalColor": "black",
      "SelectedColor": "gray",
      "SelectedHaloColor": "deepskyblue",
      "HoveredColor": "deepskyblue",

      "LineWidth": 3.0,
      "ConstructionLineWidth": 2.0,
      "PointDiameter": 10.0,

      "UseDataDefinedColors": false
    }
  }
  )"));
}
}

RDGraphicsView::RDGraphicsView(QWidget *parent) 
  : QGraphicsView(parent)
{

  this->setMouseTracking(true);
  this->setDragMode(DragMode::ScrollHandDrag);
  
  this->setRenderHint(QPainter::Antialiasing, true);
  this->setRenderHint(QPainter::SmoothPixmapTransform, true);
  
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

  QtNodes::setStyle();
}

RDGraphicsView::~RDGraphicsView()
{
}

void RDGraphicsView::selectNode(QGVNode *node)
{
  if(!node) return;

  node->setSelected(true);
}

void RDGraphicsView::wheelEvent(QWheelEvent *event)
{
  qreal scaleFactor = qPow(2.0, event->delta() / 240.0);    // How fast we zoom
  qreal factor = transform().scale(scaleFactor, scaleFactor).mapRect(QRectF(0, 0, 1, 1)).width();
  if(0.05 < factor && factor < 10)    // Zoom factor limitation
    scale(scaleFactor, scaleFactor);
}

void RDGraphicsView::mouseMoveEvent(QMouseEvent *e)
{
  if(scene()&& scene()->mouseGrabberItem())
      return;
    // intercept all item move event
  QGraphicsView::mouseMoveEvent(e);
}

void RDGraphicsView::nodeDoubleClick(QGVNode *node)
{
}

void RDGraphicsView::drawBackground(QPainter *painter, const QRectF &r)
{
  QGraphicsView::drawBackground(painter, r);

  auto drawGrid = [&](double gridStep) {
    QRect windowRect = rect();
    QPointF tl = mapToScene(windowRect.topLeft());
    QPointF br = mapToScene(windowRect.bottomRight());

    double left = std::floor(tl.x() / gridStep - 0.5);
    double right = std::floor(br.x() / gridStep + 1.0);
    double bottom = std::floor(tl.y() / gridStep - 0.5);
    double top = std::floor(br.y() / gridStep + 1.0);

    // vertical lines
    for(int xi = int(left); xi <= int(right); ++xi)
    {
      QLineF line(xi * gridStep, bottom * gridStep, xi * gridStep, top * gridStep);

      painter->drawLine(line);
    }

    // horizontal lines
    for(int yi = int(bottom); yi <= int(top); ++yi)
    {
      QLineF line(left * gridStep, yi * gridStep, right * gridStep, yi * gridStep);
      painter->drawLine(line);
    }
  };

  auto const &flowViewStyle = StyleCollection::flowViewStyle();

  QPen pfine(flowViewStyle.FineGridColor, 1.0);

  painter->setPen(pfine);
  drawGrid(15);

  QPen p(flowViewStyle.CoarseGridColor, 1.0);

  painter->setPen(p);
  drawGrid(150);
}

void RDGraphicsView::showEvent(QShowEvent *event)
{
  QGraphicsView::showEvent(event);

  centerScene();
}

BasicGraphicsScene *RDGraphicsView::nodeScene()
{
  return dynamic_cast<BasicGraphicsScene *>(scene());
}

void RDGraphicsView::centerScene()
{
  if(scene())
  {
    scene()->setSceneRect(QRectF());

    QRectF sceneRect = scene()->sceneRect();

    if(sceneRect.width() > this->rect().width() || sceneRect.height() > this->rect().height())
    {
      fitInView(sceneRect, Qt::KeepAspectRatio);
    }

    centerOn(sceneRect.center());
  }
}