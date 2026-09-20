#include "pages.h"
#include "widgets/toolpage.h"
#include "widgets/highlighters.h"
#include "iconprovider.h"

#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QScrollArea>
#include <QTimer>

#include <yaml-cpp/yaml.h>

#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHash>
#include <functional>

namespace {

// ---------------- 字段定义 ----------------
struct FieldDef {
    enum Ty { Combo, Check, Line, Spin };
    Ty ty = Line;
    QString key, label, hint;
    QStringList choices;
    QString def;   // Combo: 默认项；Check: "1"=勾选；Spin: "值|最小|最大"
};

using GenFn = std::function<QStringList(const QHash<QString, QString>&)>;

struct ScenarioDef {
    QString name;   // 场景标题（下拉项）
    QString desc;   // 一句话说明
    QList<FieldDef> fields;
    GenFn gen;
};

static QString V(const QHash<QString, QString>& v, const char* k) {
    return v.value(QString::fromLatin1(k));
}
static QString nsArg(const QString& ns) {
    return ns.isEmpty() ? QString() : QStringLiteral(" -n ") + ns;
}

static FieldDef combo(const char* key, const char* label, const QStringList& choices, const QString& def) {
    FieldDef f;
    f.ty = FieldDef::Combo; f.key = QLatin1String(key); f.label = QString::fromUtf8(label);
    f.choices = choices; f.def = def;
    return f;
}
static FieldDef line(const char* key, const char* label, const QString& def = QString(), const char* hint = "") {
    FieldDef f;
    f.ty = FieldDef::Line; f.key = QLatin1String(key); f.label = QString::fromUtf8(label);
    f.def = def; f.hint = QString::fromUtf8(hint);
    return f;
}
static FieldDef check(const char* key, const char* label, bool on = false) {
    FieldDef f;
    f.ty = FieldDef::Check; f.key = QLatin1String(key); f.label = QString::fromUtf8(label);
    f.def = on ? QStringLiteral("1") : QString();
    return f;
}
static FieldDef spin(const char* key, const char* label, int def, int mn, int mx) {
    FieldDef f;
    f.ty = FieldDef::Spin; f.key = QLatin1String(key); f.label = QString::fromUtf8(label);
    f.def = QStringLiteral("%1|%2|%3").arg(def).arg(mn).arg(mx);
    return f;
}

static const QStringList kRes = {
    QStringLiteral("pods"), QStringLiteral("deployments"), QStringLiteral("services"), QStringLiteral("ingresses"),
    QStringLiteral("configmaps"), QStringLiteral("secrets"), QStringLiteral("persistentvolumeclaims"), QStringLiteral("namespaces"),
    QStringLiteral("nodes"), QStringLiteral("serviceaccounts"), QStringLiteral("roles"), QStringLiteral("rolebindings"),
    QStringLiteral("statefulsets"), QStringLiteral("daemonsets"), QStringLiteral("jobs"), QStringLiteral("cronjobs"),
    QStringLiteral("horizontalpodautoscalers"), QStringLiteral("replicasets"), QStringLiteral("customresourcedefinitions"), QStringLiteral("all")};

static const QStringList kOutFmt = {QString(), QStringLiteral("-o wide"), QStringLiteral("-o yaml"),
                                    QStringLiteral("-o json"), QStringLiteral("-o name")};

static const QList<ScenarioDef>& scenarios() {
    static const QList<ScenarioDef> list = [] {
        QList<ScenarioDef> s;

        // ---- 查看 · 资源列表 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("查看 · 资源列表 (get)");
            sc.desc = QStringLiteral("列出资源；常用 -o wide / -o yaml，-l 按标签过滤，-A 全命名空间");
            sc.fields = {
                combo("res", "资源类型", kRes, QStringLiteral("pods")),
                line("name", "名称（可空）", QString(), "留空=列出全部"),
                line("ns", "命名空间", QStringLiteral("default"), "留空=当前上下文"),
                line("selector", "标签选择器", QString(), "如 app=nginx"),
                check("allns", "所有命名空间 (-A)"),
                combo("out", "输出格式", kOutFmt, QStringLiteral("-o wide"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl get %1").arg(V(v, "res"));
                if (!V(v, "name").isEmpty()) cmd += QStringLiteral(" ") + V(v, "name");
                if (!V(v, "selector").isEmpty()) cmd += QStringLiteral(" -l ") + V(v, "selector");
                if (V(v, "allns") == QLatin1String("1")) cmd += QStringLiteral(" -A");
                else cmd += nsArg(V(v, "ns"));
                if (!V(v, "out").isEmpty()) cmd += QStringLiteral(" ") + V(v, "out");
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- 查看 · 详细信息 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("查看 · 详细信息 (describe)");
            sc.desc = QStringLiteral("查看资源详情，含事件（Event）与最近状态");
            sc.fields = {
                combo("res", "资源类型", kRes, QStringLiteral("pods")),
                line("name", "名称"),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl describe %1 %2%3")
                                       .arg(V(v, "res"), V(v, "name"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 日志 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("日志 · Pod 日志 (logs)");
            sc.desc = QStringLiteral("查看容器日志；--tail 限制行数，--since 时间窗口，-p 上次崩溃的容器");
            sc.fields = {
                line("pod", "Pod 名称"),
                line("container", "容器（可空）", QString(), "多容器时指定"),
                line("ns", "命名空间", QStringLiteral("default")),
                check("follow", "持续跟踪 (-f)", true),
                spin("tail", "最新 N 行（0=全部）", 100, 0, 100000),
                line("since", "时间窗口（可空）", QString(), "如 30s / 10m / 1h"),
                check("timestamps", "显示时间戳"),
                check("previous", "上次崩溃容器 (-p)")};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl logs %1").arg(V(v, "pod"));
                cmd += nsArg(V(v, "ns"));
                if (!V(v, "container").isEmpty()) cmd += QStringLiteral(" -c ") + V(v, "container");
                if (V(v, "follow") == QLatin1String("1")) cmd += QStringLiteral(" -f");
                if (V(v, "tail") != QLatin1String("0")) cmd += QStringLiteral(" --tail=") + V(v, "tail");
                if (!V(v, "since").isEmpty()) cmd += QStringLiteral(" --since=") + V(v, "since");
                if (V(v, "timestamps") == QLatin1String("1")) cmd += QStringLiteral(" --timestamps");
                if (V(v, "previous") == QLatin1String("1")) cmd += QStringLiteral(" -p");
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- 事件 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("事件 · 排查异常 (events)");
            sc.desc = QStringLiteral("命名空间事件流；可按资源名/类型过滤，按时间排序，Warning 优先排查");
            sc.fields = {
                line("ns", "命名空间", QStringLiteral("default")),
                line("obj", "资源名称（可空）", QString(), "只看该资源相关事件"),
                combo("type", "事件类型", {QString(), QStringLiteral("Warning"), QStringLiteral("Normal")}, QString()),
                check("sort", "按时间排序", true),
                check("watch", "持续监听 (--watch)")};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl get events") + nsArg(V(v, "ns"));
                QStringList fs;
                if (!V(v, "obj").isEmpty()) fs << QStringLiteral("involvedObject.name=") + V(v, "obj");
                if (!V(v, "type").isEmpty()) fs << QStringLiteral("type=") + V(v, "type");
                if (!fs.isEmpty()) cmd += QStringLiteral(" --field-selector ") + fs.join(QLatin1Char(','));
                if (V(v, "sort") == QLatin1String("1")) cmd += QStringLiteral(" --sort-by=.lastTimestamp");
                if (V(v, "watch") == QLatin1String("1")) cmd += QStringLiteral(" --watch");
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- 进入容器 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("调试 · 进入容器 (exec)");
            sc.desc = QStringLiteral("在容器内执行命令或打开交互 shell");
            sc.fields = {
                line("pod", "Pod 名称"),
                line("container", "容器（可空）"),
                line("ns", "命名空间", QStringLiteral("default")),
                check("it", "交互终端 (-it)", true),
                line("cmd", "命令", QStringLiteral("sh"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl exec");
                if (V(v, "it") == QLatin1String("1")) cmd += QStringLiteral(" -it");
                cmd += QStringLiteral(" %1").arg(V(v, "pod")) + nsArg(V(v, "ns"));
                if (!V(v, "container").isEmpty()) cmd += QStringLiteral(" -c ") + V(v, "container");
                cmd += QStringLiteral(" -- ") + V(v, "cmd");
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- 文件复制 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("调试 · 文件传输 (cp)");
            sc.desc = QStringLiteral("本地与容器之间复制文件/目录（目录自动加 -r 提示）");
            sc.fields = {
                line("pod", "Pod 名称"),
                line("ns", "命名空间", QStringLiteral("default")),
                combo("dir", "方向", {QStringLiteral("下载到本地"), QStringLiteral("上传到容器")}, QStringLiteral("下载到本地")),
                line("remote", "容器内路径", QStringLiteral("/tmp/log.txt")),
                line("local", "本地路径", QStringLiteral("./log.txt"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                if (V(v, "dir") == QLatin1String("下载到本地"))
                    return QStringList{QStringLiteral("kubectl cp %1/%2:%3 %4")
                                           .arg(V(v, "ns"), V(v, "pod"), V(v, "remote"), V(v, "local"))};
                return QStringList{QStringLiteral("kubectl cp %1 %2/%3:%4")
                                       .arg(V(v, "local"), V(v, "ns"), V(v, "pod"), V(v, "remote"))};
            };
            s << sc;
        }
        // ---- 端口转发 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("调试 · 端口转发 (port-forward)");
            sc.desc = QStringLiteral("把集群内端口映射到本地，浏览器/调试器直连 Pod 或 Service");
            sc.fields = {
                line("target", "目标", QStringLiteral("pods/my-pod"), "pods/名称 或 deployments/名称 或 services/名称"),
                spin("lport", "本地端口", 8080, 1, 65535),
                spin("rport", "集群端口", 80, 1, 65535),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl port-forward %1 %2:%3%4")
                                       .arg(V(v, "target"), V(v, "lport"), V(v, "rport"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 资源占用 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("调试 · 资源占用 (top)");
            sc.desc = QStringLiteral("实时 CPU / 内存占用（需 metrics-server）");
            sc.fields = {
                combo("kind", "对象", {QStringLiteral("pod"), QStringLiteral("node")}, QStringLiteral("pod")),
                line("name", "名称（可空）", QString(), "留空=全部"),
                line("ns", "命名空间", QStringLiteral("default")),
                check("containers", "按容器细分")};
            sc.gen = [](const QHash<QString, QString>& v) {
                if (V(v, "kind") == QLatin1String("node"))
                    return QStringList{QStringLiteral("kubectl top nodes")};
                QString cmd = QStringLiteral("kubectl top pods");
                if (!V(v, "name").isEmpty()) cmd += QStringLiteral(" ") + V(v, "name");
                cmd += nsArg(V(v, "ns"));
                if (V(v, "containers") == QLatin1String("1")) cmd += QStringLiteral(" --containers");
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- 编辑 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("修改 · 在线编辑 (edit)");
            sc.desc = QStringLiteral("直接编辑资源的存活配置（临时生效，建议改 YAML 源）");
            sc.fields = {
                combo("res", "资源类型", kRes, QStringLiteral("deployments")),
                line("name", "名称"),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl edit %1 %2%3").arg(V(v, "res"), V(v, "name"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 标签/注解 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("修改 · 标签/注解 (label/annotate)");
            sc.desc = QStringLiteral("打标签或注解；--overwrite 覆盖已有值");
            sc.fields = {
                combo("mode", "操作", {QStringLiteral("label"), QStringLiteral("annotate")}, QStringLiteral("label")),
                combo("res", "资源类型", kRes, QStringLiteral("deployments")),
                line("name", "名称"),
                line("kv", "键值", QStringLiteral("tier=frontend"), "label 用 k=v；annotate 建议 k: v"),
                check("overwrite", "覆盖已有值 (--overwrite)"),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl %1 %2 %3 %4")
                                  .arg(V(v, "mode"), V(v, "res"), V(v, "name"), V(v, "kv"));
                if (V(v, "overwrite") == QLatin1String("1")) cmd += QStringLiteral(" --overwrite");
                cmd += nsArg(V(v, "ns"));
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- 补丁 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("修改 · JSON/YAML 补丁 (patch)");
            sc.desc = QStringLiteral("脚本化修改字段；merge 最常用，strategic 支持列表合并");
            sc.fields = {
                combo("res", "资源类型", kRes, QStringLiteral("deployments")),
                line("name", "名称"),
                combo("ptype", "补丁类型", {QStringLiteral("strategic"), QStringLiteral("merge"), QStringLiteral("json")},
                      QStringLiteral("merge")),
                line("patch", "补丁内容", QStringLiteral("{\"spec\":{\"replicas\":2}}")),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl patch %1 %2%3 --type=%4 --patch '%5'")
                                       .arg(V(v, "res"), V(v, "name"), nsArg(V(v, "ns")), V(v, "ptype"), V(v, "patch"))};
            };
            s << sc;
        }
        // ---- 扩缩容 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("伸缩 · 手动扩缩容 (scale)");
            sc.desc = QStringLiteral("修改副本数，秒级生效");
            sc.fields = {
                combo("res", "资源类型", {QStringLiteral("deployments"), QStringLiteral("statefulsets"), QStringLiteral("replicasets")}, QStringLiteral("deployments")),
                line("name", "名称"),
                spin("replicas", "目标副本数", 3, 0, 1000),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl scale %1 %2 --replicas=%3%4")
                                       .arg(V(v, "res"), V(v, "name"), V(v, "replicas"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 自动伸缩 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("伸缩 · 自动伸缩 (autoscale)");
            sc.desc = QStringLiteral("创建 HPA：按 CPU 使用率在 min~max 间自动调节副本");
            sc.fields = {
                line("name", "Deployment 名称"),
                spin("min", "最小副本", 2, 0, 1000),
                spin("max", "最大副本", 10, 1, 1000),
                spin("cpu", "目标 CPU 百分比", 80, 1, 100),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl autoscale deployment %1 --min=%2 --max=%3 --cpu-percent=%4%5")
                                       .arg(V(v, "name"), V(v, "min"), V(v, "max"), V(v, "cpu"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- rollout ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("发布 · 重启/回滚 (rollout)");
            sc.desc = QStringLiteral("滚动重启（重新拉镜像/重载配置）、回滚版本、查看发布历史与状态");
            sc.fields = {
                combo("sub", "操作", {QStringLiteral("restart"), QStringLiteral("undo"), QStringLiteral("status"),
                                      QStringLiteral("history"), QStringLiteral("pause"), QStringLiteral("resume")},
                      QStringLiteral("restart")),
                line("res", "资源", QStringLiteral("deployments/my-app"), "如 deployments/名称"),
                line("torev", "回滚到版本（可空）", QString(), "undo 时填，如 3"),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl rollout %1 %2").arg(V(v, "sub"), V(v, "res"));
                if (V(v, "sub") == QLatin1String("undo") && !V(v, "torev").isEmpty())
                    cmd += QStringLiteral(" --to-revision=") + V(v, "torev");
                cmd += nsArg(V(v, "ns"));
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- set image ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("发布 · 更新镜像 (set image)");
            sc.desc = QStringLiteral("滚动更新容器镜像，触发重新发布");
            sc.fields = {
                line("res", "资源", QStringLiteral("deployments/my-app")),
                line("container", "容器名", QStringLiteral("my-app")),
                line("image", "新镜像", QStringLiteral("nginx:1.25")),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl set image %1 %2=%3%4")
                                       .arg(V(v, "res"), V(v, "container"), V(v, "image"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 创建 deployment ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("创建 · 无状态部署 (deployment)");
            sc.desc = QStringLiteral("快速创建 Deployment；生产建议用 YAML 模板（资源限额/探针）");
            sc.fields = {
                line("name", "名称", QStringLiteral("my-app")),
                line("image", "镜像", QStringLiteral("nginx:1.25")),
                spin("replicas", "副本数", 2, 1, 1000),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl create deployment %1 --image=%2 --replicas=%3%4")
                                       .arg(V(v, "name"), V(v, "image"), V(v, "replicas"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 命名空间 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("创建 · 命名空间 (namespace)");
            sc.desc = QStringLiteral("新建命名空间用于环境/项目隔离");
            sc.fields = {line("name", "名称", QStringLiteral("dev"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl create namespace %1").arg(V(v, "name"))};
            };
            s << sc;
        }
        // ---- 配置/密钥 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("创建 · 配置/密钥 (configmap/secret)");
            sc.desc = QStringLiteral("创建 ConfigMap 或 Secret；生产敏感信息建议 --from-file 或密封工具");
            sc.fields = {
                combo("kind", "类型", {QStringLiteral("configmap"), QStringLiteral("secret")}, QStringLiteral("configmap")),
                combo("stype", "Secret 类型", {QStringLiteral("generic"), QStringLiteral("tls"), QStringLiteral("docker-registry")},
                      QStringLiteral("generic")),
                line("name", "名称", QStringLiteral("app-config")),
                line("lit", "键值对", QStringLiteral("LOG_LEVEL=info"), "k1=v1,k2=v2"),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                if (V(v, "kind") == QLatin1String("configmap"))
                    return QStringList{QStringLiteral("kubectl create configmap %1 --from-literal=%2%3")
                                           .arg(V(v, "name"), V(v, "lit"), nsArg(V(v, "ns")))};
                return QStringList{QStringLiteral("kubectl create secret %1 %2 --from-literal=%3%4")
                                       .arg(V(v, "stype"), V(v, "name"), V(v, "lit"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- expose ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("创建 · 服务暴露 (expose)");
            sc.desc = QStringLiteral("为工作负载创建 Service；对集群外选 NodePort/LoadBalancer");
            sc.fields = {
                line("res", "资源", QStringLiteral("deployments/my-app")),
                spin("port", "Service 端口", 80, 1, 65535),
                spin("tport", "容器端口 (targetPort)", 8080, 1, 65535),
                combo("type", "类型", {QStringLiteral("ClusterIP"), QStringLiteral("NodePort"), QStringLiteral("LoadBalancer")},
                      QStringLiteral("ClusterIP")),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl expose %1 --port=%2 --target-port=%3 --type=%4%5")
                                       .arg(V(v, "res"), V(v, "port"), V(v, "tport"), V(v, "type"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 临时 Pod ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("创建 · 临时调试 Pod (run)");
            sc.desc = QStringLiteral("跑一次性调试容器（网络探测/工具箱），退出即删");
            sc.fields = {
                line("name", "名称", QStringLiteral("tmp-debug")),
                line("image", "镜像", QStringLiteral("busybox")),
                check("rm", "退出后删除 (--rm)", true),
                check("it", "交互终端 (-it)", true),
                line("cmd", "命令", QStringLiteral("sh")),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl run %1 --image=%2").arg(V(v, "name"), V(v, "image"));
                if (V(v, "rm") == QLatin1String("1")) cmd += QStringLiteral(" --rm");
                if (V(v, "it") == QLatin1String("1")) cmd += QStringLiteral(" -it");
                cmd += nsArg(V(v, "ns"));
                if (!V(v, "cmd").isEmpty()) cmd += QStringLiteral(" -- ") + V(v, "cmd");
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- apply ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("创建 · 应用清单 (apply)");
            sc.desc = QStringLiteral("应用 YAML 清单（声明式，可重复执行）；-k kustomize 目录也支持");
            sc.fields = {line("file", "文件/目录", QStringLiteral("deploy.yaml"), "yaml 文件或目录")};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl apply -f %1").arg(V(v, "file"))};
            };
            s << sc;
        }
        // ---- 删除 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("删除 · 资源删除 (delete)");
            sc.desc = QStringLiteral("删除资源；--all 删同类型全部；强制删除仅用于终结卡死的资源");
            sc.fields = {
                combo("res", "资源类型", kRes, QStringLiteral("pods")),
                line("name", "名称", QString(), "配合 --all 可留空"),
                check("all", "删除全部 (--all)"),
                spin("grace", "宽限期(秒)", 30, 0, 3600),
                check("force", "强制 (--force --grace-period=0)"),
                combo("cascade", "级联策略", {QString(), QStringLiteral("orphan"), QStringLiteral("background"), QStringLiteral("foreground")},
                      QString()),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString cmd = QStringLiteral("kubectl delete %1").arg(V(v, "res"));
                if (V(v, "all") == QLatin1String("1")) cmd += QStringLiteral(" --all");
                else if (!V(v, "name").isEmpty()) cmd += QStringLiteral(" ") + V(v, "name");
                if (V(v, "force") == QLatin1String("1")) cmd += QStringLiteral(" --grace-period=0 --force");
                else if (V(v, "grace") != QLatin1String("30")) cmd += QStringLiteral(" --grace-period=") + V(v, "grace");
                if (!V(v, "cascade").isEmpty()) cmd += QStringLiteral(" --cascade=") + V(v, "cascade");
                cmd += nsArg(V(v, "ns"));
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- RBAC · can-i ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("权限 · 能力自查 (auth can-i)");
            sc.desc = QStringLiteral("检查某用户/SA 在命名空间的权限；* 表示全部");
            sc.fields = {
                line("verbs", "动词", QStringLiteral("get,list"), "get,list,watch,create,update,delete,*"),
                line("resources", "资源", QStringLiteral("pods"), "pods,deployments,services,*"),
                line("as", "模拟身份（可空）", QString(), "如 system:serviceaccount:dev:ci-bot"),
                line("ns", "命名空间", QStringLiteral("default"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                QString verbs = V(v, "verbs"); verbs.replace(QLatin1Char(','), QLatin1Char(' '));
                QString res = V(v, "resources"); res.replace(QLatin1Char(','), QLatin1Char(' '));
                QString cmd = QStringLiteral("kubectl auth can-i %1 %2").arg(verbs, res);
                cmd += nsArg(V(v, "ns"));
                if (!V(v, "as").isEmpty()) cmd += QStringLiteral(" --as=") + V(v, "as");
                return QStringList{cmd};
            };
            s << sc;
        }
        // ---- RBAC · 命名空间角色 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("权限 · 命名空间角色 (role+binding)");
            sc.desc = QStringLiteral("一键生成 Role + RoleBinding（含可选 Token），命名空间内授权标准姿势");
            sc.fields = {
                line("role", "角色名", QStringLiteral("dev-viewer")),
                line("verbs", "允许的动词", QStringLiteral("get,list,watch")),
                line("resources", "允许的资源", QStringLiteral("pods,services,configmaps")),
                line("sa", "服务账号", QStringLiteral("ci-bot")),
                line("ns", "命名空间", QStringLiteral("dev")),
                check("token", "同时生成 Token 命令 (1.24+)")};
            sc.gen = [](const QHash<QString, QString>& v) {
                QStringList out;
                const QString role = V(v, "role"), sa = V(v, "sa"), ns = V(v, "ns");
                out << QStringLiteral("kubectl create role %1 --verb=%2 --resource=%3 -n %4")
                           .arg(role, V(v, "verbs"), V(v, "resources"), ns);
                out << QStringLiteral("kubectl create rolebinding %1-binding --role=%1 --serviceaccount=%2:%3 -n %4")
                           .arg(role, ns, sa, ns);
                if (V(v, "token") == QLatin1String("1"))
                    out << QStringLiteral("kubectl create token %1 -n %2   # K8s 1.24+").arg(sa, ns);
                return out;
            };
            s << sc;
        }
        // ---- RBAC · 集群角色 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("权限 · 集群角色 (clusterrole)");
            sc.desc = QStringLiteral("生成 ClusterRole + ClusterRoleBinding，跨命名空间授权（谨慎使用）");
            sc.fields = {
                line("role", "角色名", QStringLiteral("node-reader")),
                line("verbs", "允许的动词", QStringLiteral("get,list")),
                line("resources", "允许的资源", QStringLiteral("nodes,pods")),
                line("sa", "服务账号（含命名空间）", QStringLiteral("dev:ci-bot"), "格式 命名空间:账号"),
                check("token", "同时生成 Token 命令 (1.24+)")};
            sc.gen = [](const QHash<QString, QString>& v) {
                QStringList out;
                const QString role = V(v, "role");
                out << QStringLiteral("kubectl create clusterrole %1 --verb=%2 --resource=%3")
                           .arg(role, V(v, "verbs"), V(v, "resources"));
                out << QStringLiteral("kubectl create clusterrolebinding %1-binding --clusterrole=%1 --serviceaccount=%2")
                           .arg(role, V(v, "sa"));
                if (V(v, "token") == QLatin1String("1")) {
                    const QStringList parts = V(v, "sa").split(QLatin1Char(':'));
                    out << QStringLiteral("kubectl create token %1 -n %2   # K8s 1.24+")
                               .arg(parts.value(1), parts.value(0));
                }
                return out;
            };
            s << sc;
        }
        // ---- SA ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("权限 · 服务账号 (serviceaccount)");
            sc.desc = QStringLiteral("创建服务账号，配合角色授权场景使用");
            sc.fields = {
                line("name", "账号名", QStringLiteral("ci-bot")),
                line("ns", "命名空间", QStringLiteral("dev"))};
            sc.gen = [](const QHash<QString, QString>& v) {
                return QStringList{QStringLiteral("kubectl create serviceaccount %1%2").arg(V(v, "name"), nsArg(V(v, "ns")))};
            };
            s << sc;
        }
        // ---- 节点管理 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("集群 · 节点管理 (cordon/drain/taint)");
            sc.desc = QStringLiteral("节点维护三板斧：封锁 → 驱逐 → 上线；污点控制调度");
            sc.fields = {
                combo("sub", "操作", {QStringLiteral("cordon"), QStringLiteral("uncordon"), QStringLiteral("drain"),
                                      QStringLiteral("taint"), QStringLiteral("delete")},
                      QStringLiteral("drain")),
                line("node", "节点名", QStringLiteral("node-1")),
                line("taint", "污点（taint 时填）", QStringLiteral("key1=value1:NoSchedule"), "去除污点在末尾加 -")};
            sc.gen = [](const QHash<QString, QString>& v) {
                const QString sub = V(v, "sub"), node = V(v, "node");
                if (sub == QLatin1String("drain"))
                    return QStringList{QStringLiteral("kubectl drain %1 --ignore-daemonsets --delete-emptydir-data").arg(node)};
                if (sub == QLatin1String("taint"))
                    return QStringList{QStringLiteral("kubectl taint nodes %1 %2").arg(node, V(v, "taint"))};
                return QStringList{QStringLiteral("kubectl %1 %2").arg(sub, node)};
            };
            s << sc;
        }
        // ---- 集群信息 ----
        {
            ScenarioDef sc;
            sc.name = QStringLiteral("集群 · 信息与版本");
            sc.desc = QStringLiteral("集群概况、版本、API 资源清单、节点状态");
            sc.fields = {
                combo("sub", "操作", {QStringLiteral("cluster-info"), QStringLiteral("version"), QStringLiteral("api-resources"),
                                      QStringLiteral("nodes"), QStringLiteral("top-nodes")},
                      QStringLiteral("cluster-info")),
                line("filter", "api-resources 过滤（可空）", QString(), "如 deployment")};
            sc.gen = [](const QHash<QString, QString>& v) {
                const QString sub = V(v, "sub");
                if (sub == QLatin1String("api-resources")) {
                    QString cmd = QStringLiteral("kubectl api-resources");
                    if (!V(v, "filter").isEmpty()) cmd += QStringLiteral(" | grep %1").arg(V(v, "filter"));
                    return QStringList{cmd};
                }
                if (sub == QLatin1String("nodes")) return QStringList{QStringLiteral("kubectl get nodes -o wide")};
                if (sub == QLatin1String("top-nodes")) return QStringList{QStringLiteral("kubectl top nodes")};
                if (sub == QLatin1String("version")) return QStringList{QStringLiteral("kubectl version")};
                return QStringList{QStringLiteral("kubectl cluster-info")};
            };
            s << sc;
        }
        return s;
    }();
    return list;
}

// ---------------- K8s 命令生成器页面 ----------------
class K8sCmdPage final : public ToolPage {
    Q_OBJECT
public:
    K8sCmdPage() {
        ToolPage::setMeta(QStringLiteral("box"), QStringLiteral("K8s 命令生成器"),
                          QStringLiteral("kubectl 可视化生成：查看 / 日志 / 事件 / 调试 / 发布 / RBAC / 集群"));
        // 整页内容放入垂直滚动区：窗口缩小后可上下滚动填写参数/查看输出
        auto* pageHost = new QWidget;
        auto* pageLay = new QVBoxLayout(pageHost);
        pageLay->setContentsMargins(0, 0, 6, 0);
        pageLay->setSpacing(12);

        auto* top = new QWidget;
        auto* topLay = new QVBoxLayout(top);
        topLay->setContentsMargins(0, 0, 0, 0);
        topLay->setSpacing(8);

        auto* row1 = new QWidget;
        auto* r1 = new QHBoxLayout(row1);
        r1->setContentsMargins(0, 0, 0, 0);
        r1->setSpacing(8);
        m_scenario = new QComboBox;
        for (const ScenarioDef& sc : scenarios()) m_scenario->addItem(sc.name);
        r1->addWidget(new QLabel(QStringLiteral("场景:")));
        r1->addWidget(m_scenario, 1);

        auto* row2 = new QWidget;
        auto* r2 = new QHBoxLayout(row2);
        r2->setContentsMargins(0, 0, 0, 0);
        r2->setSpacing(8);
        m_ctx = new QComboBox;                       // 集群上下文（kubeconfig）
        m_ns = new QComboBox;                        // 命名空间（可选可输）
        m_ns->setEditable(true);
        m_kubectlLbl = new QLabel;
        r2->addWidget(new QLabel(QStringLiteral("集群:")));
        r2->addWidget(m_ctx, 1);
        r2->addWidget(new QLabel(QStringLiteral("命名空间:")));
        r2->addWidget(m_ns, 1);

        r2->addWidget(m_kubectlLbl);
        m_installBtn = ui::button(QStringLiteral("一键安装 kubectl"));
        connect(m_installBtn, &QPushButton::clicked, this, [this] { installKubectl(); });
        r2->addWidget(m_installBtn);
        topLay->addWidget(row1);
        topLay->addWidget(row2);
        m_desc = new QLabel;
        m_desc->setObjectName(QStringLiteral("pageDesc"));
        m_desc->setWordWrap(true);
        topLay->addWidget(m_desc);
        pageLay->addWidget(ui::card(QStringLiteral("场景与集群（自动读取 ~/.kube/config）"), top));

        loadKubeconfig();
        m_regenTimer.setSingleShot(true);
        m_regenTimer.setInterval(30);
        connect(&m_regenTimer, &QTimer::timeout, this, &K8sCmdPage::regen);
        connect(m_ctx, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            applyCtxNamespace();
            scheduleRegen();
        });
        connect(m_ns, &QComboBox::currentTextChanged, this, [this](const QString&) { scheduleRegen(); });
        connect(m_ns, &QComboBox::editTextChanged, this, [this](const QString&) { scheduleRegen(); });

        m_formHost = new QWidget;
        m_form = new QFormLayout(m_formHost);
        m_form->setLabelAlignment(Qt::AlignRight);
        m_form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
        m_form->setSpacing(8);
        auto* formScroll = new QScrollArea;
        formScroll->setWidgetResizable(true);
        formScroll->setFrameShape(QFrame::NoFrame);
        formScroll->setWidget(m_formHost);
        formScroll->setMinimumHeight(160);
        pageLay->addWidget(ui::card(QStringLiteral("参数"), formScroll, nullptr, true), 1);

        m_out = new QPlainTextEdit;
        m_out->setObjectName(QStringLiteral("mono"));
        m_out->setReadOnly(true);
        m_out->setMinimumHeight(100);
        m_out->setPlaceholderText(QStringLiteral("命令将实时生成…"));
        new ShellHighlighter(m_out->document());   // kubectl 命令着色
        m_copyBtn = ui::button(QStringLiteral("复制全部"), "primary");
        connect(m_copyBtn, &QPushButton::clicked, this, [this] {
            ui::copyText(m_out->toPlainText());
            m_copyBtn->setText(QStringLiteral("✓ 已复制"));
            QTimer::singleShot(1200, this, [this] { m_copyBtn->setText(QStringLiteral("复制全部")); });
        });
        pageLay->addWidget(ui::card(QStringLiteral("命令 · 参数变化实时更新（复制到终端执行）"), m_out, m_copyBtn));

        m_out->setMinimumHeight(200);   // 命令区兼任执行输出控制台

        detectKubectl();

        auto* pageScroll = new QScrollArea;
        pageScroll->setWidgetResizable(true);
        pageScroll->setFrameShape(QFrame::NoFrame);
        pageScroll->setWidget(pageHost);
        body()->addWidget(pageScroll, 1);

        connect(m_scenario, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &K8sCmdPage::buildForm);
        buildForm(0);
    }

private slots:
    void buildForm(int idx) {
        while (m_form->count()) {
            QLayoutItem* it = m_form->takeAt(0);
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        m_widgets.clear();
        const ScenarioDef& sc = scenarios().at(idx);
        for (const FieldDef& f : sc.fields) {
            QWidget* w = createWidget(f);
            if (!w) continue;
            if (f.ty == FieldDef::Check)
                m_form->addRow(QString(), w);
            else
                m_form->addRow(f.label + QStringLiteral(":"), w);
            m_widgets.insert(f.key, w);   // 注册控件映射（regen 读取值的唯一来源）
        }
        m_desc->setText(sc.desc);
        m_lastCmd.clear();   // 切场景强制重刷
        regen();
    }
    // 防抖统一驱动：任何控件变化 → 30ms 后重生成（合并高频输入，保证可靠刷新）
    void scheduleRegen() { m_regenTimer.start(); }
    void regen() {
        if (m_scenario->currentIndex() < 0) return;
        const ScenarioDef& sc = scenarios().at(m_scenario->currentIndex());
        QHash<QString, QString> v;
        for (const FieldDef& f : sc.fields) {
            QWidget* w = m_widgets.value(f.key);
            if (!w) continue;
            if (auto* c = qobject_cast<QComboBox*>(w)) v.insert(f.key, c->currentText());
            else if (auto* cb = qobject_cast<QCheckBox*>(w)) v.insert(f.key, cb->isChecked() ? QStringLiteral("1") : QString());
            else if (auto* s = qobject_cast<QSpinBox*>(w)) v.insert(f.key, QString::number(s->value()));
            else if (auto* e = qobject_cast<QLineEdit*>(w)) v.insert(f.key, e->text().trimmed());
        }
        // 注入全局集群上下文：ns 统一由「集群」卡片控制（场景内的 ns 字段已隐藏）
        v.insert(QStringLiteral("ns"), m_ns ? m_ns->currentText().trimmed() : QString());
        // --context 用 context 名称（currentText）；「（不指定）/（未找到」占位项不注入
        QString ctx = m_ctx ? m_ctx->currentText().trimmed() : QString();
        if (ctx.startsWith(QLatin1Char('(')) || ctx.startsWith(QChar(0xFF08)))
            ctx.clear();
        QStringList cmds = sc.gen(v);
        if (!ctx.isEmpty()) {
            for (QString& c : cmds) {
                if (c.startsWith(QLatin1String("kubectl")))
                    c += QStringLiteral(" --context ") + ctx;
            }
        }
        const QString next = cmds.join(QLatin1Char('\n'));
        m_out->setPlainText(next);                 // 无条件重刷，绝不依赖信号时序
        if (next != m_lastCmd) {
            m_lastCmd = next;
            flashOutput();
        }
    }
    // 检测本机 kubectl（执行能力的前提）
    void detectKubectl() {
        QProcess p;
        p.start(QStringLiteral("kubectl"), {QStringLiteral("version"), QStringLiteral("--client=true")});
        QString ver;
        if (p.waitForFinished(3000) && p.exitCode() == 0) {
            const QString out = QString::fromUtf8(p.readAllStandardOutput());
            // 兼容两种输出：旧版 GitVersion:"v1.x" 与新版（yaml）gitVersion: v1.x
            static const QRegularExpression re(QStringLiteral("[gG]itVersion:?\"?\s*v([0-9.]+)\"?"));
            const auto m = re.match(out);
            ver = m.hasMatch() ? m.captured(1) : QStringLiteral("?");
        }
        m_kubectlOk = !ver.isEmpty();
        m_kubectlLbl->setText(m_kubectlOk
                                  ? QStringLiteral("kubectl v%1 ✓").arg(ver)
                                  : QStringLiteral("kubectl 未安装"));
        m_kubectlLbl->setStyleSheet(m_kubectlOk
                                        ? QStringLiteral("color:#3FB950;")
                                        : QStringLiteral("color:#F85149;"));
        m_installBtn->setVisible(!m_kubectlOk);
    }
    // 一键下载安装 kubectl（dl.k8s.io 稳定版，放入程序目录）
    void installKubectl() {
        m_installBtn->setEnabled(false);
        m_installBtn->setText(QStringLiteral("获取版本号…"));
        QNetworkRequest req{QUrl(QStringLiteral("https://dl.k8s.io/release/stable.txt"))};
        req.setTransferTimeout(15000);
        auto* reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            const QString ver = QString::fromUtf8(reply->readAll()).trimmed();
            if (reply->error() != QNetworkReply::NoError || !ver.startsWith(QLatin1Char('v'))) {
                m_installBtn->setEnabled(true);
                m_installBtn->setText(QStringLiteral("安装失败，点击重试"));
                m_kubectlLbl->setText(QStringLiteral("下载失败（网络原因），也可手动安装"));
                return;
            }
#ifdef Q_OS_WIN
            const QString url = QStringLiteral("https://dl.k8s.io/release/%1/bin/windows/amd64/kubectl.exe").arg(ver);
#elif defined(Q_OS_MAC)
            const QString url = QStringLiteral("https://dl.k8s.io/release/%1/bin/darwin/arm64/kubectl").arg(ver);
#else
            const QString url = QStringLiteral("https://dl.k8s.io/release/%1/bin/linux/amd64/kubectl").arg(ver);
#endif
            m_installBtn->setText(QStringLiteral("下载中…"));
            QNetworkRequest r2{QUrl(url)};
            r2.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            auto* dl = m_nam.get(r2);
            connect(dl, &QNetworkReply::downloadProgress, this, [this](qint64 done, qint64 total) {
                if (total > 0)
                    m_installBtn->setText(QStringLiteral("下载中 %1%…").arg(done * 100 / total));
            });
            connect(dl, &QNetworkReply::finished, this, [this, dl, ver] {
                dl->deleteLater();
                const QByteArray data = dl->readAll();
                const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/kubectl");
#ifdef Q_OS_WIN
                const QString finalPath = path + QStringLiteral(".exe");
#else
                const QString finalPath = path;
#endif
                if (dl->error() != QNetworkReply::NoError || data.size() < 1000000) {
                    m_installBtn->setEnabled(true);
                    m_installBtn->setText(QStringLiteral("安装失败，点击重试"));
                    return;
                }
                QFile f(finalPath);
                if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
                    m_installBtn->setEnabled(true);
                    m_installBtn->setText(QStringLiteral("写入失败，点击重试"));
                    return;
                }
                f.close();
                m_installBtn->setText(QStringLiteral("已下载 %1").arg(ver));
                m_installBtn->setEnabled(false);
                m_out->appendPlainText(QStringLiteral("\n# kubectl 已安装到程序目录；若执行报找不到命令，请重启工具箱"));
                detectKubectl();
            });
        });
    }
    // 读取 ~/.kube/config：列出全部集群 context 及其默认命名空间
    void loadKubeconfig() {
        QString home = qEnvironmentVariable("USERPROFILE");
        if (home.isEmpty()) home = qEnvironmentVariable("HOME");
        const QString path = home + QStringLiteral("/.kube/config");
        m_ctx->blockSignals(true);
        m_ctx->clear();
        m_ctx->addItem(QStringLiteral("（不指定）"), QString());
        QString current;
        QStringList nsList{QStringLiteral("default"), QStringLiteral("kube-system"), QStringLiteral("kube-public")};
        bool loaded = false;
        try {
            YAML::Node cfg = YAML::LoadFile(path.toStdString());
            if (cfg["current-context"])
                current = QString::fromStdString(cfg["current-context"].as<std::string>());
            if (cfg["contexts"]) {
                for (const auto& c : cfg["contexts"]) {
                    if (!c["name"]) continue;
                    const QString name = QString::fromStdString(c["name"].as<std::string>());
                    QString ns = QStringLiteral("default");
                    if (c["context"] && c["context"]["namespace"])
                        ns = QString::fromStdString(c["context"]["namespace"].as<std::string>());
                    m_ctx->addItem(name, ns);
                    if (!nsList.contains(ns)) nsList << ns;
                    loaded = true;
                }
            }
        } catch (...) {}
        if (!loaded) {
            m_ctx->addItem(QStringLiteral("（未找到 ~/.kube/config，可手填参数）"), QString());
        } else {
            const int idx = m_ctx->findText(current);
            if (idx >= 0) m_ctx->setCurrentIndex(idx);
        }
        m_ctx->blockSignals(false);
        m_ns->blockSignals(true);
        m_ns->clear();
        m_ns->addItems(nsList);
        m_ns->blockSignals(false);
        applyCtxNamespace();
    }
    // 切换集群时把命名空间带成该 context 的默认值
    void applyCtxNamespace() {
        if (!m_ctx) return;
        const QString ns = m_ctx->currentData().toString();
        if (!ns.isEmpty() && m_ns) {
            m_ns->blockSignals(true);
            m_ns->setCurrentText(ns);
            m_ns->blockSignals(false);
        }
    }
    void flashOutput() {
        if (!m_out) return;
        m_out->setStyleSheet(QStringLiteral(
            "border: 2px solid #4F8CFF; border-radius: 6px; background: rgba(79,140,255,0.07);"));
        QTimer::singleShot(450, this, [this] { m_out->setStyleSheet(QString()); });
    }

private:
    QWidget* createWidget(const FieldDef& f) {
        if (f.key == QLatin1String("ns"))
            return nullptr;   // 命名空间由「集群」卡片统一控制
        switch (f.ty) {
            case FieldDef::Combo: {
                auto* c = new QComboBox;
                c->addItems(f.choices);
                if (!f.def.isEmpty()) c->setCurrentText(f.def);
                connect(c, &QComboBox::currentTextChanged, this, [this](const QString&) { scheduleRegen(); });
                return c;
            }
            case FieldDef::Check: {
                auto* cb = new QCheckBox(f.label);
                cb->setChecked(f.def == QLatin1String("1"));
                connect(cb, &QCheckBox::toggled, this, [this](bool) { scheduleRegen(); });
                return cb;
            }
            case FieldDef::Spin: {
                auto* s = new QSpinBox;
                const QStringList parts = f.def.split(QLatin1Char('|'));
                s->setRange(parts.value(1).toInt(), parts.value(2).toInt());
                s->setValue(parts.value(0).toInt());
                connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { scheduleRegen(); });
                return s;
            }
            case FieldDef::Line:
            default: {
                auto* e = new QLineEdit;
                if (!f.def.isEmpty()) e->setText(f.def);
                if (!f.hint.isEmpty()) e->setPlaceholderText(f.hint);
                connect(e, &QLineEdit::textChanged, this, [this](const QString&) { scheduleRegen(); });
                return e;
            }
        }
    }

    QComboBox* m_scenario = nullptr;
    QComboBox* m_ctx = nullptr;      // 集群上下文（读取 kubeconfig）
    QComboBox* m_ns = nullptr;       // 命名空间（全局）
    QLabel* m_kubectlLbl = nullptr;
    QPushButton* m_installBtn = nullptr;
    QNetworkAccessManager m_nam;
    bool m_kubectlOk = false;
    QTimer m_regenTimer;
    QLabel* m_desc = nullptr;
    QFormLayout* m_form = nullptr;
    QWidget* m_formHost = nullptr;
    QPlainTextEdit* m_out = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QString m_lastCmd;
    QHash<QString, QWidget*> m_widgets;
};

// ---------------- K8s YAML 模板页面 ----------------
struct YamlTmpl {
    QString name, desc, body;
};

static const QList<YamlTmpl>& yamlTmpls() {
    static const QList<YamlTmpl> list = {
        {QStringLiteral("Deployment 无状态部署"),
         QStringLiteral("最常用工作负载：副本管理/滚动更新/探针/资源限额"),
         QStringLiteral(R"YAML(apiVersion: apps/v1
kind: Deployment
metadata:
  name: my-app                    # 应用名
  namespace: default              # 命名空间
  labels: {app: my-app}
spec:
  replicas: 3                     # 副本数
  selector:
    matchLabels: {app: my-app}
  template:
    metadata:
      labels: {app: my-app}
    spec:
      containers:
        - name: my-app
          image: nginx:1.25       # 镜像
          ports: [{containerPort: 80}]
          resources:              # 资源限额（生产建议必填）
            requests: {cpu: 100m, memory: 128Mi}
            limits: {cpu: 500m, memory: 256Mi}
          livenessProbe:          # 存活探针：失败重启容器
            httpGet: {path: /healthz, port: 80}
            initialDelaySeconds: 10
            periodSeconds: 10
          readinessProbe:         # 就绪探针：未就绪不接流量
            httpGet: {path: /ready, port: 80}
            initialDelaySeconds: 5
)YAML")},
        {QStringLiteral("Service 服务"),
         QStringLiteral("ClusterIP 集群内访问 / NodePort 节点端口 / LoadBalancer 云负载均衡"),
         QStringLiteral(R"YAML(apiVersion: v1
kind: Service
metadata:
  name: my-app-svc
  namespace: default
spec:
  type: ClusterIP                 # ClusterIP | NodePort | LoadBalancer
  selector: {app: my-app}         # 匹配 Pod 标签
  ports:
    - name: http
      port: 80                    # Service 端口
      targetPort: 8080            # 容器端口
      # nodePort: 30080           # NodePort 类型时指定（30000-32767）
)YAML")},
        {QStringLiteral("Ingress 入口"),
         QStringLiteral("七层路由：域名/路径转发，需集群已装 Ingress Controller"),
         QStringLiteral(R"YAML(apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: my-app-ingress
  namespace: default
  annotations:
    nginx.ingress.kubernetes.io/ssl-redirect: "true"
spec:
  ingressClassName: nginx
  tls:
    - hosts: [app.example.com]
      secretName: app-tls         # TLS 证书 Secret
  rules:
    - host: app.example.com
      http:
        paths:
          - path: /
            pathType: Prefix
            backend:
              service:
                name: my-app-svc
                port: {number: 80}
)YAML")},
        {QStringLiteral("ConfigMap 配置"),
         QStringLiteral("非敏感配置：环境变量或挂载文件"),
         QStringLiteral(R"YAML(apiVersion: v1
kind: ConfigMap
metadata:
  name: my-app-config
  namespace: default
data:
  APP_MODE: production            # 键值对（环境变量）
  nginx.conf: |                   # 或整文件（挂载用）
    server {
      listen 80;
      server_name localhost;
    }
)YAML")},
        {QStringLiteral("Secret 密钥"),
         QStringLiteral("敏感信息（base64 编码存储，非加密！生产建议 sealed-secrets/Vault）"),
         QStringLiteral(R"YAML(apiVersion: v1
kind: Secret
metadata:
  name: my-app-secret
  namespace: default
type: Opaque
stringData:                       # stringData 明文写入，自动编码
  DB_PASSWORD: "change-me"
  API_KEY: "sk-xxx"
)YAML")},
        {QStringLiteral("PVC 持久存储"),
         QStringLiteral("声明持久卷；需集群有 StorageClass 动态供给"),
         QStringLiteral(R"YAML(apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: my-app-data
  namespace: default
spec:
  accessModes: [ReadWriteOnce]    # RWO 单节点读写 | ROX 只读 | RWX 多节点
  storageClassName: standard      # 集群 StorageClass 名
  resources:
    requests: {storage: 10Gi}
)YAML")},
        {QStringLiteral("ServiceAccount 服务账号"),
         QStringLiteral("给 Pod/CI 用的身份，配合 Role/RoleBinding 授权"),
         QStringLiteral(R"YAML(apiVersion: v1
kind: ServiceAccount
metadata:
  name: ci-bot
  namespace: default
# 配套绑定见 Role/RoleBinding 模板
)YAML")},
        {QStringLiteral("Role 命名空间角色"),
         QStringLiteral("命名空间内权限：verbs × resources 白名单"),
         QStringLiteral(R"YAML(apiVersion: rbac.authorization.k8s.io/v1
kind: Role
metadata:
  name: dev-viewer
  namespace: default
rules:
  - apiGroups: [""]               # "" = 核心组（pods/svc等）
    resources: ["pods", "pods/log", "services", "configmaps"]
    verbs: ["get", "list", "watch"]
  - apiGroups: ["apps"]
    resources: ["deployments"]
    verbs: ["get", "list", "watch", "update"]   # 可加 create/delete
)YAML")},
        {QStringLiteral("RoleBinding 角色绑定"),
         QStringLiteral("把 Role 绑到用户/组/服务账号（含 Role 模板可一并 apply）"),
         QStringLiteral(R"YAML(apiVersion: rbac.authorization.k8s.io/v1
kind: RoleBinding
metadata:
  name: dev-viewer-binding
  namespace: default
subjects:
  - kind: ServiceAccount
    name: ci-bot
    namespace: default
roleRef:                          # roleRef 创建后不可改
  kind: Role
  name: dev-viewer
  apiGroup: rbac.authorization.k8s.io
)YAML")},
        {QStringLiteral("HPA 自动伸缩"),
         QStringLiteral("按 CPU/内存或自定义指标自动调节副本数"),
         QStringLiteral(R"YAML(apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: my-app-hpa
  namespace: default
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: my-app
  minReplicas: 2
  maxReplicas: 10
  metrics:
    - type: Resource
      resource:
        name: cpu
        target: {type: Utilization, averageUtilization: 80}
  behavior:                       # 稳定性（可选）
    scaleDown:
      stabilizationWindowSeconds: 300
)YAML")},
        {QStringLiteral("CronJob 定时任务"),
         QStringLiteral("周期任务：备份/报表/清理，格式同 crontab"),
         QStringLiteral(R"YAML(apiVersion: batch/v1
kind: CronJob
metadata:
  name: db-backup
  namespace: default
spec:
  schedule: "0 3 * * *"           # 每天 3 点
  concurrencyPolicy: Forbid       # 禁止重叠
  successfulJobsHistoryLimit: 3
  jobTemplate:
    spec:
      backoffLimit: 2
      template:
        spec:
          restartPolicy: OnFailure
          containers:
            - name: backup
              image: postgres:16
              command: ["sh", "-c", "pg_dump ... > /backup/db.sql"]
)YAML")},
        {QStringLiteral("Namespace 命名空间"),
         QStringLiteral("环境/项目隔离，可带资源配额"),
         QStringLiteral(R"YAML(apiVersion: v1
kind: Namespace
metadata:
  name: dev
  labels: {team: backend}
)YAML")},
        {QStringLiteral("DaemonSet 守护集"),
         QStringLiteral("每节点跑一个：日志采集/监控 Agent"),
         QStringLiteral(R"YAML(apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: log-agent
  namespace: kube-system
spec:
  selector:
    matchLabels: {app: log-agent}
  template:
    metadata:
      labels: {app: log-agent}
    spec:
      tolerations:                # 容忍控制面污点，全节点部署
        - key: node-role.kubernetes.io/control-plane
          effect: NoSchedule
      containers:
        - name: agent
          image: fluent/fluentd:latest
          resources: {requests: {cpu: 100m, memory: 128Mi}}
)YAML")},
    };
    return list;
}

class K8sYamlPage final : public ToolPage {
    Q_OBJECT
public:
    K8sYamlPage() {
        ToolPage::setMeta(QStringLiteral("file"), QStringLiteral("K8s YAML 模板"),
                          QStringLiteral("常用资源清单模板，可直接编辑后复制 / kubectl apply"));
        auto* top = new QWidget;
        auto* topLay = new QHBoxLayout(top);
        topLay->setContentsMargins(0, 0, 0, 0);
        topLay->setSpacing(8);
        m_kind = new QComboBox;
        for (const YamlTmpl& t : yamlTmpls()) m_kind->addItem(t.name);
        m_desc = new QLabel;
        m_desc->setObjectName(QStringLiteral("pageDesc"));
        m_desc->setWordWrap(true);
        topLay->addWidget(new QLabel(QStringLiteral("模板:")));
        topLay->addWidget(m_kind, 1);
        body()->addWidget(ui::card(QStringLiteral("模板"), top));

        m_descLbl = m_desc;
        QWidget* descWrap = new QWidget;
        auto* dl = new QVBoxLayout(descWrap);
        dl->setContentsMargins(0, 0, 0, 0);
        dl->addWidget(m_desc);
        body()->addWidget(descWrap);

        m_out = new QPlainTextEdit;
        m_out->setObjectName(QStringLiteral("mono"));
        m_out->setPlaceholderText(QStringLiteral("选择模板后在此生成，可直接编辑…"));
        new YamlHighlighter(m_out->document());   // kubectl 命令着色
        auto* copyBtn = ui::button(QStringLiteral("复制"), "primary");
        connect(copyBtn, &QPushButton::clicked, this, [this] { ui::copyText(m_out->toPlainText()); });
        auto* resetBtn = ui::button(QStringLiteral("重置模板"));
        connect(resetBtn, &QPushButton::clicked, this, [this] { fill(); });
        auto* btns = new QWidget;
        auto* bl = new QHBoxLayout(btns);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(8);
        bl->addWidget(copyBtn);
        bl->addWidget(resetBtn);
        bl->addStretch();
        body()->addWidget(ui::card(QStringLiteral("清单（可编辑）"), m_out, btns, true), 1);

        connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { fill(); });
        fill();
    }

private slots:
    void fill() {
        if (m_kind->currentIndex() < 0) return;
        const YamlTmpl& t = yamlTmpls().at(m_kind->currentIndex());
        m_out->setPlainText(t.body);
        m_desc->setText(QStringLiteral("💡 ") + t.desc);
    }
private:
    QComboBox* m_kind = nullptr;
    QLabel* m_desc = nullptr;
    QLabel* m_descLbl = nullptr;
    QPlainTextEdit* m_out = nullptr;
};

} // namespace

namespace pages {
ToolPage* createK8sCmd() { return new K8sCmdPage; }
ToolPage* createK8sYaml() { return new K8sYamlPage; }
} // namespace pages

#include "k8stoolspages.moc"
