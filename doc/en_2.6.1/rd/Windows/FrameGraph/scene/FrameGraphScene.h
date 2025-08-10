#pragma once


#include "BasicGraphicsScene.hpp"

#include <QMap>
#include <QSet>
#include <QPointF>
#include <QSize>
#include <QDebug>

#define lit(a) QStringLiteral(a)

using QtNodes::ConnectionId;
using QtNodes::NodeId;
using QtNodes::BasicGraphicsScene;
using QtNodes::AbstractGraphModel;
using QtNodes::PortType; 

/*
* be combined with Graphviz dot layout engines
* only compute node`s positon And edge drawPath, graphicsItem can not move
*/

class FrameGraphScene : public BasicGraphicsScene
{
      Q_OBJECT
public:
    FrameGraphScene(AbstractGraphModel &graphModel, QObject *parent = nullptr);

    // Scenes without models are not supported
    FrameGraphScene() = delete;

    ~FrameGraphScene();

    // 公开接口用于手动触发布局计算
    void requestLayoutUpdate() { computeFrameGraphLayout(); }

signals:
    // 布局更新信号
    void layoutUpdateStarted();
    void layoutUpdateFinished();
    void layoutUpdateFailed(const QString& error);

private:
    // 主要布局计算方法
    void computeFrameGraphLayout();
    
    // 应用布局结果到场景
    void applyLayoutResults(
        const QMap<uint32_t, QPointF>& nodePositions,
        const QMap<ConnectionId, QPainterPath>& connectionPaths,
        const QSet<ConnectionId>& allConnections);
    
    // 创建回退路径（当Graphviz路径不可用时）
    QPainterPath createFallbackPath(const ConnectionId& connId);
    
    // 场景几何变化管理
    void prepareSceneGeometryChange();
    void finishSceneGeometryChange();
    
    // 调整场景矩形以适应布局
    void adjustSceneRect();

private:
    //using UniqueConnectionGraphicsObject = std::unique_ptr<FGConnectionGraphObject>;
    //std::unordered_map<ConnectionId, UniqueConnectionGraphicsObject> _connectionGraphicsObjects;

};

#include "gvc.h"
#include "cgraph.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Graphviz坐标转换辅助函数
inline qreal getGraphHeight(Agraph_t *graph) { 
    return GD_bb(graph).UR.y; 
}

inline QPointF convertPoint(pointf p, qreal gheight) { 
    return QPointF(p.x, gheight - p.y); 
}

class DotLayout
{
public:
  static void dot(const QMap<uint32_t, QSize> &inData, 
                  const QMap<uint32_t, QVector<uint32_t>> &inConnection,
                  const QSet<ConnectionId> &realConnections,
                  QMap<uint32_t, QPointF> &outData, 
                  QMap<ConnectionId, QPainterPath>& connectionPaths, 
                  bool isHorizontal = false)
  {
    outData.clear();
    connectionPaths.clear();
    if (inData.isEmpty()) return;

    GVC_t *gvc = gvContext();
    Agraph_t *g = agopen(lit("g").toLocal8Bit().data(), Agdirected, NULL);

    agattr(g, AGRAPH, lit("rankdir").toLocal8Bit().data(),
           lit("%1").arg(isHorizontal ? lit("LR") : lit("TB")).toLocal8Bit().constData());

    char empty[]{""};
    QMap<uint32_t, Agnode_t *> nodes;
    QMap<QPair<uint32_t, uint32_t>, Agedge_t *> edges;

    // 创建节点
    for (auto it = inData.begin(); it != inData.end(); ++it) {
      uint32_t id = it.key();
      QSize size = it.value();

      char name[32];
      snprintf(name, sizeof(name), "%u", id);
      
      Agnode_t *n = agnode(g, name, 1);
      agsafeset(n, lit("width").toLocal8Bit().data(),
                lit("%1").arg(size.width() / 72.0).toLocal8Bit().constData(), empty);
      agsafeset(n, lit("height").toLocal8Bit().data(),
                lit("%1").arg(size.height() / 72.0).toLocal8Bit().constData(), empty);

      nodes[id] = n;
    }

    // 添加边并记录边对象，创建从节点对到实际连接ID的映射
    QMap<QPair<uint32_t, uint32_t>, ConnectionId> edgeToConnectionMap;
    
    // 首先建立连接映射
    for (const ConnectionId& connId : realConnections) {
      QPair<uint32_t, uint32_t> edgeKey(connId.outNodeId, connId.inNodeId);
      edgeToConnectionMap[edgeKey] = connId;
    }
    
    // 添加边到图中
    for (auto it = inConnection.begin(); it != inConnection.end(); ++it) {
      uint32_t sourceId = it.key();
      Agnode_t *source = nodes.value(sourceId);
      if (!source) continue;
      
      for (uint32_t targetId : it.value()) {
        Agnode_t *target = nodes.value(targetId);
        if (target) {
          Agedge_t *e = agedge(g, source, target, NULL, 1);
          edges[QPair<uint32_t, uint32_t>(sourceId, targetId)] = e;
        }
      }
    }

    // 执行布局
    gvLayout(gvc, g, "dot");

    // 获取图形高度用于坐标转换
    qreal gheight = getGraphHeight(g);

    // 获取节点位置
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
      Agnode_t *n = it.value();
      pointf coord = ND_coord(n);
      outData[it.key()] = convertPoint(coord, gheight);
    }

    // 提取边的路径信息并生成QPainterPath
    for (auto edgeIt = edges.begin(); edgeIt != edges.end(); ++edgeIt) {
      QPair<uint32_t, uint32_t> edgeKey = edgeIt.key();
      Agedge_t *edge = edgeIt.value();
      
      // 查找对应的真实连接ID
      if (edgeToConnectionMap.contains(edgeKey)) {
        ConnectionId realConnId = edgeToConnectionMap[edgeKey];
        
        QPainterPath path = extractEdgePath(edge, gheight);
        if (!path.isEmpty()) {
          connectionPaths[realConnId] = path;
        }
      }
    }

    gvFreeLayout(gvc, g);
    agclose(g);
    gvFreeContext(gvc);
  }

private:
  // 从样条线生成QPainterPath
  static QPainterPath createPathFromSplines(const splines *spl, qreal gheight)
  {
    QPainterPath path;
    if (spl && (spl->list != 0) && (spl->list->size % 3 == 1)) {
      bezier bez = spl->list[0];
      
      if (bez.sflag) {
        path.moveTo(convertPoint(bez.sp, gheight));
        path.lineTo(convertPoint(bez.list[0], gheight));
      } else {
        path.moveTo(convertPoint(bez.list[0], gheight));
      }
      
      for (int i = 1; i < bez.size; i += 3) {
        path.cubicTo(convertPoint(bez.list[i], gheight),
                    convertPoint(bez.list[i + 1], gheight),
                    convertPoint(bez.list[i + 2], gheight));
      }
      
      if (bez.eflag) {
        path.lineTo(convertPoint(bez.ep, gheight));
      }
    }
    return path;
  }
  
  static QPolygonF createArrow(const QLineF &line)
  {
    QLineF n = line.normalVector();
    QPointF o(n.dx() / 3.0, n.dy() / 3.0);
    
    QPolygonF polygon;
    polygon.append(line.p1() + o);
    polygon.append(line.p2());
    polygon.append(line.p1() - o);
    
    return polygon;
  }
  
  // 将箭头多边形添加到路径中
  static void addArrowToPath(QPainterPath &path, const QPolygonF &arrow)
  {
    if (arrow.isEmpty()) return;
    
    QPointF currentPos = path.currentPosition();
    
    path.addPolygon(arrow);
    
    path.moveTo(currentPos);
  }
  
  // 从Graphviz边对象中提取路径信息并添加箭头
  static QPainterPath extractEdgePath(Agedge_t *edge, qreal gheight)
  {
    QPainterPath path;
    
    if (!edge) return path;
    
    // 使用ED_spl宏获取样条线信息
    const splines* spl = ED_spl(edge);
    
    if (spl && (spl->list != 0) && (spl->list->size % 3 == 1)) {
      // 从样条线创建路径
      path = createPathFromSplines(spl, gheight);
      
      bezier bez = spl->list[0];
      
      // 添加箭头（如果存在）
      if (bez.eflag) {
        // 创建从最后一个控制点到终点的线段来生成箭头
        QPointF lastPoint = convertPoint(bez.list[bez.size - 1], gheight);
        QPointF endPoint = convertPoint(bez.ep, gheight);
        QLineF arrowLine(lastPoint, endPoint);
        
        QPolygonF arrow = createArrow(arrowLine);
        addArrowToPath(path, arrow);
      }
      
      // 添加尾部箭头（如果存在）
      if (bez.sflag) {
        QPointF startPoint = convertPoint(bez.sp, gheight);
        QPointF firstPoint = convertPoint(bez.list[0], gheight);
        QLineF tailArrowLine(firstPoint, startPoint);
        
        QPolygonF tailArrow = createArrow(tailArrowLine);
        addArrowToPath(path, tailArrow);
      }
    } else {
      // 如果无法获取样条线信息，创建简单的直线连接
      Agnode_t *tail = agtail(edge);
      Agnode_t *head = aghead(edge);
      
      if (tail && head) {
        pointf tailCoord = ND_coord(tail);
        pointf headCoord = ND_coord(head);
        
        QPointF startPoint = convertPoint(tailCoord, gheight);
        QPointF endPoint = convertPoint(headCoord, gheight);
        
        path.moveTo(startPoint);
        path.lineTo(endPoint);
        
        // 为直线添加箭头
        QLineF line(startPoint, endPoint);
        QPolygonF arrow = createArrow(line);
        addArrowToPath(path, arrow);
      }
    }
    
    return path;
  }
};
