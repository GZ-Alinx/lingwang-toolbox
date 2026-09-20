#include "collapsiblesection.h"
#include "iconprovider.h"

#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QEasingCurve>

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_header = new QPushButton(QStringLiteral("  ") + title);
    m_header->setObjectName(QStringLiteral("navSection"));
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->setToolTip(QStringLiteral("点击展开 / 收起"));
    lay->addWidget(m_header);

    m_content = new QWidget;
    m_content->setVisible(false);
    m_content->setMaximumHeight(0);
    m_contentLay = new QVBoxLayout(m_content);
    m_contentLay->setContentsMargins(8, 2, 0, 4);
    m_contentLay->setSpacing(2);
    lay->addWidget(m_content);

    m_anim = new QPropertyAnimation(m_content, "maximumHeight", this);
    m_anim->setDuration(160);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QPropertyAnimation::finished, this, [this] {
        if (m_expanded)
            m_content->setMaximumHeight(QWIDGETSIZE_MAX);  // 展开后解除高度限制，适应窗口变化
        else {
            m_content->setMaximumHeight(0);
            m_content->setVisible(false);
        }
    });

    connect(m_header, &QPushButton::clicked, this, [this] { setExpanded(!m_expanded); });
    updateHeaderIcon();
}

void CollapsibleSection::addButton(QWidget* btn) {
    m_contentLay->addWidget(btn);
}

void CollapsibleSection::setExpanded(bool on, bool animate) {
    if (on == m_expanded) return;
    m_expanded = on;
    updateHeaderIcon();
    m_anim->stop();
    if (animate) {
        m_content->setVisible(true);
        const int h = qMax(1, m_content->sizeHint().height());
        m_anim->setStartValue(on ? 0 : h);
        m_anim->setEndValue(on ? h : 0);
        m_anim->start();
    } else {
        m_content->setVisible(on);
        m_content->setMaximumHeight(on ? QWIDGETSIZE_MAX : 0);
    }
    emit expandedChanged(on);
}

void CollapsibleSection::updateHeaderIcon() {
    m_header->setIcon(Icons::get(m_expanded ? QStringLiteral("chevron-down")
                                            : QStringLiteral("chevron-right"),
                                 QColor(0x8A, 0x92, 0xA0)));
}
