#pragma once
#include <QString>
#include <QList>
#include <QPair>

namespace sysinfo {

using InfoList = QList<QPair<QString, QString>>;
InfoList collect();

} // namespace sysinfo
