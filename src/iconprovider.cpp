#include "iconprovider.h"
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QFontDatabase>
#include <QGuiApplication>

namespace Icons {

struct IconDef { const char* body; };

static const QHash<QString, QString>& defs() {
    static const QHash<QString, QString> map = {
        // 线性 SVG path 集（feather 风格）
        {QStringLiteral("home"), QStringLiteral("<path d=\"M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z\"/><polyline points=\"9 22 9 12 15 12 15 22\"/>")},
        {QStringLiteral("globe"), QStringLiteral("<circle cx=\"12\" cy=\"12\" r=\"10\"/><line x1=\"2\" y1=\"12\" x2=\"22\" y2=\"12\"/><path d=\"M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z\"/>")},
        {QStringLiteral("code"), QStringLiteral("<polyline points=\"16 18 22 12 16 6\"/><polyline points=\"8 6 2 12 8 18\"/>")},
        {QStringLiteral("lock"), QStringLiteral("<rect x=\"3\" y=\"11\" width=\"18\" height=\"11\" rx=\"2\" ry=\"2\"/><path d=\"M7 11V7a5 5 0 0 1 10 0v4\"/>")},
        {QStringLiteral("file"), QStringLiteral("<path d=\"M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z\"/><polyline points=\"14 2 14 8 20 8\"/><line x1=\"16\" y1=\"13\" x2=\"8\" y2=\"13\"/><line x1=\"16\" y1=\"17\" x2=\"8\" y2=\"17\"/>")},
        {QStringLiteral("cpu"), QStringLiteral("<rect x=\"4\" y=\"4\" width=\"16\" height=\"16\" rx=\"2\" ry=\"2\"/><rect x=\"9\" y=\"9\" width=\"6\" height=\"6\"/><line x1=\"9\" y1=\"1\" x2=\"9\" y2=\"4\"/><line x1=\"15\" y1=\"1\" x2=\"15\" y2=\"4\"/><line x1=\"9\" y1=\"20\" x2=\"9\" y2=\"23\"/><line x1=\"15\" y1=\"20\" x2=\"15\" y2=\"23\"/><line x1=\"20\" y1=\"9\" x2=\"23\" y2=\"9\"/><line x1=\"20\" y1=\"14\" x2=\"23\" y2=\"14\"/><line x1=\"1\" y1=\"9\" x2=\"4\" y2=\"9\"/><line x1=\"1\" y1=\"14\" x2=\"4\" y2=\"14\"/>")},
        {QStringLiteral("zap"), QStringLiteral("<polygon points=\"13 2 3 14 12 14 11 22 21 10 12 10 13 2\"/>")},
        {QStringLiteral("sun"), QStringLiteral("<circle cx=\"12\" cy=\"12\" r=\"5\"/><line x1=\"12\" y1=\"1\" x2=\"12\" y2=\"3\"/><line x1=\"12\" y1=\"21\" x2=\"12\" y2=\"23\"/><line x1=\"4.22\" y1=\"4.22\" x2=\"5.64\" y2=\"5.64\"/><line x1=\"18.36\" y1=\"18.36\" x2=\"19.78\" y2=\"19.78\"/><line x1=\"1\" y1=\"12\" x2=\"3\" y2=\"12\"/><line x1=\"21\" y1=\"12\" x2=\"23\" y2=\"12\"/><line x1=\"4.22\" y1=\"19.78\" x2=\"5.64\" y2=\"18.36\"/><line x1=\"18.36\" y1=\"5.64\" x2=\"19.78\" y2=\"4.22\"/>")},
        {QStringLiteral("moon"), QStringLiteral("<path d=\"M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z\"/>")},
        {QStringLiteral("info"), QStringLiteral("<circle cx=\"12\" cy=\"12\" r=\"10\"/><line x1=\"12\" y1=\"16\" x2=\"12\" y2=\"12\"/><line x1=\"12\" y1=\"8\" x2=\"12.01\" y2=\"8\"/>")},
        {QStringLiteral("wifi"), QStringLiteral("<path d=\"M5 12.55a11 11 0 0 1 14.08 0\"/><path d=\"M1.42 9a16 16 0 0 1 21.16 0\"/><path d=\"M8.53 16.11a6 6 0 0 1 6.95 0\"/><line x1=\"12\" y1=\"20\" x2=\"12.01\" y2=\"20\"/>")},
        {QStringLiteral("clock"), QStringLiteral("<circle cx=\"12\" cy=\"12\" r=\"10\"/><polyline points=\"12 6 12 12 16 14\"/>")},
        {QStringLiteral("hash"), QStringLiteral("<line x1=\"4\" y1=\"9\" x2=\"20\" y2=\"9\"/><line x1=\"4\" y1=\"15\" x2=\"20\" y2=\"15\"/><line x1=\"10\" y1=\"3\" x2=\"8\" y2=\"21\"/><line x1=\"16\" y1=\"3\" x2=\"14\" y2=\"21\"/>")},
        {QStringLiteral("grid"), QStringLiteral("<rect x=\"3\" y=\"3\" width=\"7\" height=\"7\"/><rect x=\"14\" y=\"3\" width=\"7\" height=\"7\"/><rect x=\"14\" y=\"14\" width=\"7\" height=\"7\"/><rect x=\"3\" y=\"14\" width=\"7\" height=\"7\"/>")},
        {QStringLiteral("box"), QStringLiteral("<path d=\"M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z\"/><polyline points=\"3.27 6.96 12 12.01 20.73 6.96\"/><line x1=\"12\" y1=\"22.08\" x2=\"12\" y2=\"12\"/>")},
        {QStringLiteral("chevron-right"), QStringLiteral("<polyline points=\"9 18 15 12 9 6\"/>")},
        {QStringLiteral("chevron-down"), QStringLiteral("<polyline points=\"6 9 12 15 18 9\"/>")},
        {QStringLiteral("search"), QStringLiteral("<circle cx=\"11\" cy=\"11\" r=\"8\"/><line x1=\"21\" y1=\"21\" x2=\"16.65\" y2=\"16.65\"/>")},
        {QStringLiteral("terminal"), QStringLiteral("<polyline points=\"4 17 10 11 4 5\"/><line x1=\"12\" y1=\"19\" x2=\"20\" y2=\"19\"/>")},
    };
    return map;
}

QPixmap pixmap(const QString& name, const QColor& color, int size) {
    const QString body = defs().value(name);
    if (body.isEmpty()) return QPixmap();
    QString svg = QStringLiteral(
                      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" width=\"%1\" height=\"%1\" "
                      "fill=\"none\" stroke=\"%2\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">%3</svg>")
                      .arg(size)
                      .arg(color.name())
                      .arg(body);
    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pm(size * qGuiApp->devicePixelRatio(), size * qGuiApp->devicePixelRatio());
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    renderer.render(&p, QRectF(0, 0, pm.width(), pm.height()));
    pm.setDevicePixelRatio(qGuiApp->devicePixelRatio());
    return pm;
}

QIcon get(const QString& name, const QColor& color) {
    QIcon icon;
    icon.addPixmap(pixmap(name, color, 16));
    icon.addPixmap(pixmap(name, color, 20));
    icon.addPixmap(pixmap(name, color, 24));
    icon.addPixmap(pixmap(name, color, 32));
    return icon;
}

QFont monoFont(int pointSize) {
    QString family = QStringLiteral("Consolas");
#ifdef Q_OS_MAC
    family = QStringLiteral("Menlo");
#endif
    QFont f(family, pointSize);
    if (QFontDatabase::families().contains(family)) {
        f.setStyleHint(QFont::Monospace);
        return f;
    }
    f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(pointSize);
    return f;
}

} // namespace Icons
