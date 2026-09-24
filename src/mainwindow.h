#pragma once
#include <QMainWindow>
#include <QHash>
#include <QList>

class QStackedWidget;
class QLineEdit;
class QLabel;
class CollapsibleSection;
class QVBoxLayout;
class QToolButton;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void openTool(const QString& id);
    void toggleTheme();
    void showAbout();
    void applySearch(const QString& text);

private:
    QWidget* makeHome();
    QWidget* makeSidebar();
    void applyTheme(bool dark);

    QStackedWidget* m_stack = nullptr;
    QLineEdit* m_search = nullptr;
    QToolButton* m_themeBtn = nullptr;
    QPushButton* m_homeBtn = nullptr;
    QWidget* m_home = nullptr;
    QWidget* m_sidebar = nullptr;
    bool m_dark = true;

    struct SideEntry {
        QPushButton* btn = nullptr;
        QString toolId;      // 工具 id（分类标题为空）
        QString category;    // 所属分类
    };
    QList<SideEntry> m_sideEntries;
    QList<CollapsibleSection*> m_sections;
    QHash<QString, CollapsibleSection*> m_sectionByCat;
    QHash<QString, bool> m_savedExpanded;   // 搜索前的展开状态
    bool m_inSearch = false;
    QHash<QString, QPushButton*> m_toolButtons;   // id -> sidebar button
    QHash<QString, QWidget*> m_homeCards;         // id -> home card
    QList<QLabel*> m_homeCaptions;
    QLabel* m_noResult = nullptr;                 // 搜索无结果提示
};
