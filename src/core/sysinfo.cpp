#include "sysinfo.h"
#include <QSysInfo>
#include <QThread>
#include <QLocale>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_MAC
#include <sys/sysctl.h>
#endif

namespace sysinfo {

static QString ramTotal() {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st))
        return QStringLiteral("%1 GB").arg(QString::number(static_cast<double>(st.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0), 'f', 1));
#elif defined(Q_OS_MAC)
    quint64 mem = 0;
    size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0)
        return QStringLiteral("%1 GB").arg(QString::number(static_cast<double>(mem) / (1024.0 * 1024.0 * 1024.0), 'f', 1));
#endif
    return QStringLiteral("未知");
}

InfoList collect() {
    InfoList out;
    QSysInfo si;
    out << qMakePair(QStringLiteral("主机名"), si.machineHostName());
    out << qMakePair(QStringLiteral("操作系统"),
                     QStringLiteral("%1 %2").arg(si.prettyProductName(), si.currentCpuArchitecture()));
    out << qMakePair(QStringLiteral("内核版本"), si.kernelType() + QLatin1Char(' ') + si.kernelVersion());
    out << qMakePair(QStringLiteral("CPU 架构（构建）"), si.buildCpuArchitecture());
    out << qMakePair(QStringLiteral("逻辑核心数"), QString::number(QThread::idealThreadCount()));
    out << qMakePair(QStringLiteral("物理内存"), ramTotal());
    out << qMakePair(QStringLiteral("应用版本"), QCoreApplication::applicationVersion());
    out << qMakePair(QStringLiteral("Qt 版本"), QString::fromLatin1(qVersion()));
    out << qMakePair(QStringLiteral("编译器"),
#ifdef Q_OS_WIN
        QStringLiteral("MinGW-w64 / GCC %1").arg(QString::number(__GNUC__) + "." + QString::number(__GNUC_MINOR__))
#else
        QStringLiteral("Clang %1.%2").arg(QString::number(__clang_major__), QString::number(__clang_minor__))
#endif
    );
    out << qMakePair(QStringLiteral("系统区域"), QLocale::system().name());
    return out;
}

} // namespace sysinfo
