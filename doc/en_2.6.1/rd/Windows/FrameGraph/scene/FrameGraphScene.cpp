#include "FrameGraphScene.h"
#include <QSet>
#include <QDebug>
#include <exception>
#include "NodeGraphicsObject.hpp"
#include "ConnectionGraphicsObject.hpp"

FrameGraphScene::FrameGraphScene(AbstractGraphModel &graphModel, QObject *parent) 
  : BasicGraphicsScene(graphModel, parent)
{
    computeFrameGraphLayout();

    //disconnect(&BasicGraphicsScene::graphModel(), &AbstractGraphModel::connectionCreated, this,
    //    &BasicGraphicsScene::onConnectionCreated);
    //disconnect(&BasicGraphicsScene::graphModel(), &AbstractGraphModel::connectionCreated, this,
    //           &FrameGraphScene::onConnectionCreated);
}

FrameGraphScene::~FrameGraphScene()
{
    clearScene();
}


void FrameGraphScene::computeFrameGraphLayout()
{
  AbstractGraphModel &model = graphModel();
  auto &nodeGeo = nodeGeometry();
  auto nodes = model.allNodeIds();

  // 检查是否有节点需要布局
  if (nodes.empty()) {
    qDebug() << "No nodes to layout";
    return;
  }

  // 收集所有节点尺寸信息
  QMap<uint32_t, QSize> nodeSize;
  // 收集节点间的连接关系
  QMap<uint32_t, QVector<uint32_t>> nodeConnection;
  // 保存所有连接ID用于后续路径设置
  QSet<ConnectionId> allConnections;
  
  for(auto nodeId : nodes)
  {
    QSize size = nodeGeo.size(nodeId);
    nodeSize[nodeId] = size;

    // 获取该节点的所有连接
    auto connections = model.allConnectionIds(nodeId);
    for(auto conn : connections)
    {
      // 只处理出边，避免重复添加
      if(conn.outNodeId == nodeId)
      {
        nodeConnection[nodeId].append(conn.inNodeId);
        allConnections.insert(conn);
      }
    }
  }

  // 验证连接的完整性
  for(auto it = nodeConnection.begin(); it != nodeConnection.end(); ++it)
  {
    uint32_t sourceId = it.key();
    for(uint32_t targetId : it.value())
    {
      if(!nodeSize.contains(targetId))
      {
        qWarning() << "Invalid connection: target node" << targetId << "not found";
        continue;
      }
    }
  }

  // 计算节点位置和连接路径
  QMap<uint32_t, QPointF> nodePos;
  QMap<ConnectionId, QPainterPath> connectionPaintPath;
  
  try {
    DotLayout::dot(nodeSize, nodeConnection, allConnections, nodePos, connectionPaintPath, true);
  } catch (const std::exception& e) {
    qCritical() << "Layout computation failed:" << e.what();
    emit layoutUpdateFailed(QString::fromStdString(e.what()));
    return;
  }

  // 应用布局结果
  applyLayoutResults(nodePos, connectionPaintPath, allConnections);
}

void FrameGraphScene::applyLayoutResults(
    const QMap<uint32_t, QPointF>& nodePositions,
    const QMap<ConnectionId, QPainterPath>& connectionPaths,
    const QSet<ConnectionId>& allConnections)
{
  AbstractGraphModel &model = graphModel();
  
  // 通知场景即将进行大量几何变化
  prepareSceneGeometryChange();
  
  // 设置节点位置
  for(auto it = nodePositions.begin(); it != nodePositions.end(); ++it)
  {
    uint32_t nodeId = it.key();
    QPointF position = it.value();
    
    // 设置节点位置
    model.setNodeData(nodeId, QtNodes::NodeRole::Position, position);
    
    // 通知对应的图形对象更新位置
    auto ngo = nodeGraphicsObject(nodeId);
    if(ngo)
    {
      ngo->setPos(position);
    }
  }

  // 设置连接路径
  int pathsSet = 0;
  for(const ConnectionId& connId : allConnections)
  {
    auto cgo = connectionGraphicsObject(connId);
    if(cgo)
    {
      // 查找对应的路径
      if(connectionPaths.contains(connId))
      {
        cgo->setPaintPath(connectionPaths[connId]);
        pathsSet++;
      }
      else
      {
        // 如果没有找到对应路径，生成简单的直线路径
        QPainterPath fallbackPath = createFallbackPath(connId);
        cgo->setPaintPath(fallbackPath);
        qDebug() << "Using fallback path for connection" << connId.outNodeId << "->" << connId.inNodeId;
      }
      
      // 触发连接图形对象的更新
      cgo->update();
    }
  }
  
  qDebug() << "Layout applied:" << nodePositions.size() << "nodes," 
           << pathsSet << "/" << allConnections.size() << "connection paths set";
  
  // 完成场景几何变化
  finishSceneGeometryChange();
}

QPainterPath FrameGraphScene::createFallbackPath(const ConnectionId& connId)
{
  QPainterPath path;
  AbstractGraphModel &model = graphModel();
  
  // 获取源节点和目标节点的位置
  QVariant outPosVar = model.nodeData(connId.outNodeId, QtNodes::NodeRole::Position);
  QVariant inPosVar = model.nodeData(connId.inNodeId, QtNodes::NodeRole::Position);
  
  if(outPosVar.isValid() && inPosVar.isValid())
  {
    QPointF outPos = outPosVar.toPointF();
    QPointF inPos = inPosVar.toPointF();
    
    // 创建简单的直线路径
    path.moveTo(outPos);
    path.lineTo(inPos);
  }
  
  return path;
}

void FrameGraphScene::prepareSceneGeometryChange()
{
  // 通知即将开始布局更新
  emit layoutUpdateStarted();
}

void FrameGraphScene::finishSceneGeometryChange()
{
  // 更新整个场景
  update();
  
  // 调整场景矩形以适应新的布局
  adjustSceneRect();
  
  emit layoutUpdateFinished();
}

void FrameGraphScene::adjustSceneRect()
{
  AbstractGraphModel &model = graphModel();
  auto &nodeGeo = nodeGeometry();
  auto nodes = model.allNodeIds();
  
  if (nodes.empty()) {
    return;
  }
  
  // 计算所有节点的边界矩形
  QRectF boundingRect;
  bool first = true;
  
  for (auto nodeId : nodes) {
    QVariant posVar = model.nodeData(nodeId, QtNodes::NodeRole::Position);
    if (posVar.isValid()) {
      QPointF pos = posVar.toPointF();
      QSize size = nodeGeo.size(nodeId);
      
      // 计算节点的实际矩形（中心点 + 半尺寸）
      QRectF nodeRect(pos.x() - size.width() / 2.0,
                      pos.y() - size.height() / 2.0,
                      size.width(),
                      size.height());
      
      if (first) {
        boundingRect = nodeRect;
        first = false;
      } else {
        boundingRect = boundingRect.united(nodeRect);
      }
    }
  }
  
  // 添加边距
  const qreal margin = 50.0;
  boundingRect.adjust(-margin, -margin, margin, margin);
  
  // 设置场景矩形
  setSceneRect(boundingRect);
  
  qDebug() << "Scene rect adjusted to:" << boundingRect;
}

