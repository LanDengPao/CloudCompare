#pragma once

#include <QtGui/QColor>

#include "Style.hpp"

namespace QtNodes {

class ConnectionStyle : public Style
{
public:
    ConnectionStyle();

    ConnectionStyle(QString jsonText);

    ~ConnectionStyle() = default;

public:
    static void setConnectionStyle(QString jsonText);

public:
    void loadJson(QJsonObject const &json) override;

    QJsonObject toJson() const override;

public:
    QColor constructionColor() const;
    QColor normalColor() const;
    QColor normalColor(QString typeId) const;
    QColor selectedColor() const;
    QColor selectedHaloColor() const;
    QColor hoveredColor() const;

    float lineWidth() const;
    float constructionLineWidth() const;
    float pointDiameter() const;

    bool useDataDefinedColors() const;

private:
    QColor ConstructionColor;
    QColor NormalColor;
    QColor SelectedColor;
    QColor SelectedHaloColor;
    QColor HoveredColor;

    float LineWidth;
    float ConstructionLineWidth;
    float PointDiameter;

    bool UseDataDefinedColors;
};
} // namespace QtNodes
