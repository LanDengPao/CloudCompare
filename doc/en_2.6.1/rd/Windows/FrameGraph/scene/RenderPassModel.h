#pragma once

#include <QtCore/QObject>

#include "NodeData.hpp"
#include "NodeDelegateModel.hpp"

#include <memory>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::PortIndex;
using QtNodes::PortType;

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
class MyNodeData : public NodeData
{
public:
  NodeDataType type() const override
  {
    return NodeDataType{lit("MyNodeData"), lit("My Node Data")};
  }
};

class SimpleNodeData : public NodeData
{
public:
  NodeDataType type() const override { return NodeDataType{lit("SimpleData"), lit("Simple Data")}; }
};

//------------------------------------------------------------------------------

/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class RenderPassModel : public NodeDelegateModel
{
    Q_OBJECT

public:
    virtual ~RenderPassModel() {}

public:
    QString caption() const override { return lit("Render Pass"); }

    QString name() const override { return lit("RenderPassModel"); }

public:
    unsigned int nPorts(PortType const portType) const override;

    NodeDataType dataType(PortType const portType, PortIndex const portIndex) const override;

    std::shared_ptr<NodeData> outData(PortIndex const port) override;
    void setInData(std::shared_ptr<NodeData>, PortIndex const) override;
    
    QWidget *embeddedWidget() override { return nullptr; }

private:
  
};
