#pragma once
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

class QPropertyAnimation;

// 侧边栏可折叠分类区块：标题头（箭头 + 分类名）+ 工具按钮容器
class CollapsibleSection : public QWidget {
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);
    void addButton(QWidget* btn);
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool on, bool animate = true);

signals:
    void expandedChanged(bool on);

private:
    void updateHeaderIcon();

    QPushButton* m_header;
    QWidget* m_content;
    QVBoxLayout* m_contentLay;
    QPropertyAnimation* m_anim = nullptr;
    bool m_expanded = false;
};
