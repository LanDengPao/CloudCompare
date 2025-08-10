#pragma once

#include <QPainter>

class QPainter;

namespace QtNodes {

class NodeGraphicsObject;
class NodeDataModel;

/// Class enables custom painting.
class  AbstractNodePainter
{
public:
    virtual ~AbstractNodePainter() = default;

    /**
   * Reimplement this function in order to have a custom painting.
   *
   * Useful functions:
   * `NodeGraphicsObject::nodeScene()->nodeGeometry()`
   * `NodeGraphicsObject::graphModel()`
   */
    virtual void paint(QPainter *painter, NodeGraphicsObject &ngo) const = 0;
};
} // namespace QtNodes
