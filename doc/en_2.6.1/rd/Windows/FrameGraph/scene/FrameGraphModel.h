#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QPointF>
#include <QtCore/QSize>
#include "AbstractGraphModel.hpp"
#include "ConnectionIdUtils.hpp"
#include "StyleCollection.hpp"

using ConnectionId = QtNodes::ConnectionId;
using ConnectionPolicy = QtNodes::ConnectionPolicy;
using NodeFlag = QtNodes::NodeFlag;
using NodeId = QtNodes::NodeId;
using NodeRole = QtNodes::NodeRole;
using PortIndex = QtNodes::PortIndex;
using PortRole = QtNodes::PortRole;
using PortType = QtNodes::PortType;
using StyleCollection = QtNodes::StyleCollection;
using QtNodes::NodeFlags;

using QtNodes::InvalidNodeId;

class CustomPaintWidget;

class FrameGraphModel : public QtNodes::AbstractGraphModel
{
  Q_OBJECT
public:
  struct NodeGeometryData
  {
    QSize size;
    QPointF pos;
  };

  struct PassNodeInfo{
    uint32_t effectiveEID;
    QString name;
    uint32_t width;
    uint32_t height;
    uint32_t colorNumber;
    uint32_t depthStencilNumber;
    QVector<QString> readAtt;
    QVector<QString> drawAtt;

    PassNodeInfo() : effectiveEID(0), width(0), height(0), colorNumber(0), depthStencilNumber(0){}
    bool operator==(const PassNodeInfo& other) const {
      return effectiveEID == other.effectiveEID ; }
    bool operator!=(const PassNodeInfo& other) const {
      return effectiveEID != other.effectiveEID ; }
    bool operator<(const PassNodeInfo& other) const {
      return effectiveEID < other.effectiveEID ; }

  };

struct ConnnectionInfo{
  uint32_t fromEffectiveEID;
  uint32_t toEffectiveEID;
  QString  resource;
  bool isColorTarget;
};

public:
  //FrameGraphModel(std::shared_ptr<NodeDelegateModelRegistry> registry);

  FrameGraphModel() : _nodesLocked(false){};

  ~FrameGraphModel() override;

  void clear();
  std::unordered_set<NodeId> allNodeIds() const override;

  std::unordered_set<ConnectionId> allConnectionIds(NodeId const nodeId) const override;

  std::unordered_set<ConnectionId> connections(NodeId nodeId, PortType portType,
                                               PortIndex portIndex) const override;

  bool connectionExists(ConnectionId const connectionId) const override;

  NodeId addNode(QString const nodeType = QString()) override;
  NodeId addNode(const PassNodeInfo& info);

  /**
   * Connection is possible when graph contains no connectivity data
   * in both directions `Out -> In` and `In -> Out`.
   */
  bool connectionPossible(ConnectionId const connectionId) const override;

  void addConnection(ConnectionId const connectionId) override;

  void addConnection(const ConnnectionInfo &info);

  bool nodeExists(NodeId const nodeId) const override;

  QVariant nodeData(NodeId nodeId, NodeRole role) const override;

  bool setNodeData(NodeId nodeId, NodeRole role, QVariant value) override;

  QVariant portData(NodeId nodeId, PortType portType, PortIndex portIndex,
                    PortRole role) const override;

  bool setPortData(NodeId nodeId, PortType portType, PortIndex portIndex, QVariant const &value,
                   PortRole role = PortRole::Data) override;

  bool deleteConnection(ConnectionId const connectionId) override;

  bool deleteNode(NodeId const nodeId) override;

  QJsonObject saveNode(NodeId const) const override;

  /// @brief Creates a new node based on the informatoin in `nodeJson`.
  /**
   * @param nodeJson conains a `NodeId`, node's position, internal node
   * information.
   */
  void loadNode(QJsonObject const &nodeJson) override;

  NodeId newNodeId() override { return _nextNodeId++; }

  //set lock state
  void setNodesLocked(bool b = true);
  NodeFlags nodeFlags(NodeId nodeId) const override;
  
  bool detachPossible(ConnectionId const) const override { return false; }

  private:

  QMap<NodeId, PassNodeInfo> _nodeIds;
  QMap<uint32_t, NodeId> _effectiveEIDToNodeId;

  std::unordered_set<ConnectionId> _connectivity;
  mutable std::unordered_map<NodeId, NodeGeometryData> _nodeGeometryData;

  QMap<uint32_t, QPixmap> _passColorAttachment;

  /// A convenience variable needed for generating unique node ids.
  NodeId _nextNodeId;

  bool _nodesLocked;
};
