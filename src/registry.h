#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QColor>
#include <functional>

class ToolPage;

struct ToolMeta {
    QString id;          // 唯一标识
    QString name;        // 中文名
    QString desc;        // 一句话描述
    QString category;    // 分类
    QString icon;        // 图标名
    QStringList keywords; // 搜索关键词（含英文）
    std::function<ToolPage*()> create;
};

class ToolRegistry {
public:
    static QList<ToolMeta>& all();
    static const ToolMeta* find(const QString& id);
    static QStringList categories();
    static QColor categoryColor(const QString& category);
};
