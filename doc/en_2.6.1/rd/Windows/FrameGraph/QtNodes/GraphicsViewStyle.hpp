#pragma once

#include <QtGui/QColor>

#include "Style.hpp"

namespace QtNodes {

class GraphicsViewStyle : public Style
{
public:
    GraphicsViewStyle();

    GraphicsViewStyle(QString jsonText);

    ~GraphicsViewStyle() = default;

public:
    static void setStyle(QString jsonText);

private:
    void loadJson(QJsonObject const &json) override;

    QJsonObject toJson() const override;

public:
    QColor BackgroundColor;
    QColor FineGridColor;
    QColor CoarseGridColor;
};
} // namespace QtNodes
