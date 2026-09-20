#pragma once
#include <QIcon>
#include <QColor>
#include <QHash>
#include <QString>

// 内置 SVG 图标（线性风格，24x24），按名字+颜色即时渲染
namespace Icons {
QIcon get(const QString& name, const QColor& color);
QPixmap pixmap(const QString& name, const QColor& color, int size);
// 等宽字体
QFont monoFont(int pointSize);
} // namespace Icons
