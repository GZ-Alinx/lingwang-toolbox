#pragma once
#include <QLayout>
#include <QList>
#include <QStyle>

// 自适应换行的流式布局（用于首页工具卡片网格）
class FlowLayout : public QLayout {
    Q_OBJECT
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 12, int vSpacing = 12);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int horizontalSpacing() const { return m_hSpace; }
    int verticalSpacing() const { return m_vSpace; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    int count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int index) const override { return m_items.value(index); }
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override { return minimumSize(); }
    QLayoutItem* takeAt(int index) override;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> m_items;
    int m_hSpace = 0;
    int m_vSpace = 0;
};
