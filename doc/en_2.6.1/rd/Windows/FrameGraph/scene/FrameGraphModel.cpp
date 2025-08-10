#include "FrameGraphModel.h"


FrameGraphModel::~FrameGraphModel()
{
  //
}

void FrameGraphModel::clear()
{
  _nodeIds.clear();
  _nodeGeometryData.clear();
  _connectivity.clear();
}

std::unordered_set<NodeId> FrameGraphModel::allNodeIds() const
{
  std::unordered_set<NodeId> ret;
  auto it = _nodeIds.begin();
  while(it != _nodeIds.end())
    ret.insert(it++.key());

  return ret;
}

std::unordered_set<ConnectionId> FrameGraphModel::allConnectionIds(NodeId const nodeId) const
{
  std::unordered_set<ConnectionId> result;

  std::copy_if(_connectivity.begin(), _connectivity.end(), std::inserter(result, std::end(result)),
               [&nodeId](ConnectionId const &cid) {
                 return cid.inNodeId == nodeId || cid.outNodeId == nodeId;
               });

  return result;
}

std::unordered_set<ConnectionId> FrameGraphModel::connections(NodeId nodeId, PortType portType,
                                                               PortIndex portIndex) const
{
  std::unordered_set<ConnectionId> result;

  std::copy_if(
      _connectivity.begin(), _connectivity.end(), std::inserter(result, std::end(result)),
      [&portType, &portIndex, &nodeId](ConnectionId const &cid) {
        return (getNodeId(portType, cid) == nodeId && getPortIndex(portType, cid) == portIndex);
      });

  return result;
}

bool FrameGraphModel::connectionExists(ConnectionId const connectionId) const
{
  return (_connectivity.find(connectionId) != _connectivity.end());
}

NodeId FrameGraphModel::addNode(const PassNodeInfo& info)
{
  NodeId newId = newNodeId();

  //设置port caption
  for(int i = 0; i < info.readAtt.size(); i++)
  {
    setPortData(newId, PortType::In, i, info.readAtt[i]);
  }
  for(int i = 0; i < info.drawAtt.size(); i++)
  {
    setPortData(newId, PortType::Out, i, info.drawAtt[i]);
  }

  // Create new node.
  _nodeIds.insert(newId, info);
  _effectiveEIDToNodeId.insert(info.effectiveEID, newId);

  Q_EMIT nodeCreated(newId);
  return newId;
}

NodeId FrameGraphModel::addNode(QString const nodeType)
{
  NodeId newId = newNodeId();
  // Create new node.
  //_nodeIds.insert(newId);

  Q_EMIT nodeCreated(newId);

  return newId;
}


bool FrameGraphModel::connectionPossible(ConnectionId const connectionId) const
{
  return _connectivity.find(connectionId) == _connectivity.end();
}

void FrameGraphModel::addConnection(ConnectionId const connectionId)
{
  _connectivity.insert(connectionId);

  Q_EMIT connectionCreated(connectionId);
}

void FrameGraphModel::addConnection(const ConnnectionInfo &info)
{
  //effective eid
  NodeId fromNodeId = _effectiveEIDToNodeId[info.fromEffectiveEID];
  NodeId toNodeId = _effectiveEIDToNodeId[info.toEffectiveEID];
  PortIndex outPortIndex = _nodeIds[fromNodeId].drawAtt.indexOf(info.resource);
  PortIndex inPortIndex = _nodeIds[toNodeId].readAtt.indexOf(info.resource);

  addConnection(ConnectionId{fromNodeId, outPortIndex, toNodeId, inPortIndex});
}

bool FrameGraphModel::nodeExists(NodeId const nodeId) const
{
  return (_nodeIds.find(nodeId) != _nodeIds.end());
}

QVariant FrameGraphModel::nodeData(NodeId nodeId, NodeRole role) const
{
  Q_UNUSED(nodeId);

  QVariant result;

  switch(role)
  {
    case NodeRole::Type: result = lit("Default Node Type"); break;

    case NodeRole::Position: result = _nodeGeometryData[nodeId].pos; break;

    case NodeRole::Size: result = _nodeGeometryData[nodeId].size; break;

    case NodeRole::CaptionVisible: result = true; break;

    case NodeRole::Caption: result = _nodeIds[nodeId].name; break;

    case NodeRole::Style:
    {
      auto style = StyleCollection::nodeStyle();
      result = style.toJson().toVariantMap();
    }
    break;

    case NodeRole::InternalData: break;

    case NodeRole::InPortCount: result = _nodeIds[nodeId].readAtt.size(); break;

    case NodeRole::OutPortCount: result = _nodeIds[nodeId].drawAtt.size(); break;

    case NodeRole::Widget: result = QVariant(); break;
  }

  return result;
}

bool FrameGraphModel::setNodeData(NodeId nodeId, NodeRole role, QVariant value)
{
  bool result = false;

  switch(role)
  {
    case NodeRole::Type: break;
    case NodeRole::Position:
    {
      _nodeGeometryData[nodeId].pos = value.value<QPointF>();

      Q_EMIT nodePositionUpdated(nodeId);

      result = true;
    }
    break;

    case NodeRole::Size:
    {
      _nodeGeometryData[nodeId].size = value.value<QSize>();
      result = true;
    }
    break;

    case NodeRole::CaptionVisible: break;

    case NodeRole::Caption: break;

    case NodeRole::Style: break;

    case NodeRole::InternalData: break;

    case NodeRole::InPortCount: break;

    case NodeRole::OutPortCount: break;

    case NodeRole::Widget: break;
  }

  return result;
}

QVariant FrameGraphModel::portData(NodeId nodeId, PortType portType, PortIndex portIndex,
                                    PortRole role) const
{
  switch(role)
  {
    case PortRole::Data: return QVariant(); break;

    case PortRole::DataType: return QVariant(); break;

    case PortRole::ConnectionPolicyRole: return QVariant::fromValue(ConnectionPolicy::One); break;

    case PortRole::CaptionVisible: return true; break;

    case PortRole::Caption:
      if(portType == PortType::In)
        return portIndex < (uint32_t)_nodeIds[nodeId].readAtt.size() ? _nodeIds[nodeId].readAtt[portIndex] : lit("Port In");
      else
        return portIndex < (uint32_t)_nodeIds[nodeId].drawAtt.size() ? _nodeIds[nodeId].drawAtt[portIndex] : lit("Port Out");

      break;
  }

  return QVariant();
}

bool FrameGraphModel::setPortData(NodeId nodeId, PortType portType, PortIndex portIndex,
                                   QVariant const &value, PortRole role)
{
  Q_UNUSED(nodeId);
  Q_UNUSED(portType);
  Q_UNUSED(portIndex);
  Q_UNUSED(value);
  Q_UNUSED(role);

  return false;
}

bool FrameGraphModel::deleteConnection(ConnectionId const connectionId)
{
  bool disconnected = false;

  auto it = _connectivity.find(connectionId);

  if(it != _connectivity.end())
  {
    disconnected = true;

    _connectivity.erase(it);
  }

  if(disconnected)
    Q_EMIT connectionDeleted(connectionId);

  return disconnected;
}

bool FrameGraphModel::deleteNode(NodeId const nodeId)
{
  // Delete connections to this node first.
  auto connectionIds = allConnectionIds(nodeId);

  for(auto &cId : connectionIds)
  {
    deleteConnection(cId);
  }
  _passColorAttachment.remove(_nodeIds[nodeId].effectiveEID);
  _effectiveEIDToNodeId.remove(_nodeIds[nodeId].effectiveEID);
  _nodeIds.remove(nodeId);
  _nodeGeometryData.erase(nodeId);

  Q_EMIT nodeDeleted(nodeId);

  return true;
}

QJsonObject FrameGraphModel::saveNode(NodeId const nodeId) const
{
  QJsonObject nodeJson;

  nodeJson[lit("id")] = static_cast<qint64>(nodeId);

  {
    QPointF const pos = nodeData(nodeId, NodeRole::Position).value<QPointF>();

    QJsonObject posJson;
    posJson[lit("x")] = pos.x();
    posJson[lit("y")] = pos.y();
    nodeJson[lit("position")] = posJson;
  }

  return nodeJson;
}

void FrameGraphModel::loadNode(QJsonObject const &nodeJson)
{
  NodeId restoredNodeId = static_cast<NodeId>(nodeJson[lit("id")].toInt());

  // Next NodeId must be larger that any id existing in the graph
  _nextNodeId = std::max(_nextNodeId, restoredNodeId + 1);

  // Create new node.
  //_nodeIds.insert(restoredNodeId);

  Q_EMIT nodeCreated(restoredNodeId);

  {
    QJsonObject posJson = nodeJson[lit("position")].toObject();
    QPointF const pos(posJson[lit("x")].toDouble(), posJson[lit("y")].toDouble());

    setNodeData(restoredNodeId, NodeRole::Position, pos);
  }
}

void FrameGraphModel::setNodesLocked(bool b)
{
  _nodesLocked = b;

  for(NodeId nodeId : allNodeIds())
  {
    Q_EMIT nodeFlagsUpdated(nodeId);
  }
}

NodeFlags FrameGraphModel::nodeFlags(NodeId nodeId) const
{
  auto basicFlags = AbstractGraphModel::nodeFlags(nodeId);

  if(_nodesLocked)
    basicFlags |= NodeFlag::Locked;

  return basicFlags;
}