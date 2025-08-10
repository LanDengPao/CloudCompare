#include "RenderPassModel.h"

// For some reason CMake could not generate moc-files correctly
// without having a cpp for an QObject from hpp.

unsigned int RenderPassModel::nPorts(PortType const portType) const
{
  uint32_t result = 0;

  switch(portType)
  {
    case PortType::In: 
      break;
    case PortType::Out: 
      break;

    case PortType::None: break;
  }

  return result;
}

NodeDataType RenderPassModel::dataType(PortType const portType, PortIndex const portIndex) const
{
  switch(portType)
  {
    case PortType::In:
      switch(portIndex)
      {
        case 0: return MyNodeData().type();
        case 1: return SimpleNodeData().type();
      }
      break;

    case PortType::Out:
      switch(portIndex)
      {
        case 0: return MyNodeData().type();
        case 1: return SimpleNodeData().type();
      }
      break;

    case PortType::None: break;
  }
  // FIXME: control may reach end of non-void function [-Wreturn-type]
  return NodeDataType();
}

std::shared_ptr<NodeData> RenderPassModel::outData(PortIndex const port)
{
  if(port < 1)
    return std::make_shared<MyNodeData>();

  return std::make_shared<SimpleNodeData>();
}

void RenderPassModel::setInData(std::shared_ptr<NodeData>, PortIndex const)
{
}
