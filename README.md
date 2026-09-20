# 灵王工具箱 (LingWang Toolbox)

开发 · 运维 · 网络一站式桌面工具箱。C++17 / Qt 6 编写，原生窗口客户端，双击即用、完全离线（除 IP 归属地等联网查询）。

![平台](https://img.shields.io/badge/平台-Windows%20x64%20%7C%20macOS%20Apple%20Silicon-blue)
![License](https://img.shields.io/badge/License-MIT-green)
[![Release](https://img.shields.io/badge/下载-Releases-4F8CFF)](../../releases/latest)

## ✨ 功能（v1.5 共 36 个工具）

### 🌐 网络诊断
| 工具 | 说明 |
|---|---|
| IP 查询 | 本机内网 IP、公网 IP 与归属地（多接口容灾） |
| DNS 解析 | A / AAAA / CNAME / MX / TXT / NS / SRV，支持自定义 DNS 服务器 |
| Ping | 连通性测试，丢包率 / 最小平均最大延迟统计 |
| 路由追踪 | Traceroute，逐跳流式显示 |
| 端口扫描 | TCP 端口并发扫描（64+ 并发、可取消） |
| HTTP 请求 | 简化版 Postman：方法 / 请求头 / 请求体 / 响应 / 耗时 |
| 子网计算器 | CIDR ↔ 掩码、网段 / 广播 / 可用主机范围 |
| Whois 查询 | 域名注册信息（IANA + 注册商两级查询） |
| 网卡信息 | 网卡 / MAC / IPv4 / IPv6 一览 |

### 🐳 容器运维（v1.1 新增）
| 工具 | 说明 |
|---|---|
| K8s 命令生成器 | kubectl 可视化生成：查看/日志/事件/调试(exec·cp·端口转发·top)/修改/扩缩容/发布(rollout·set image)/创建(含 secret tls·docker-registry·from-file·job)/删除/RBAC 权限/节点管理/集群信息，30+ 场景实时生成；自动读取 ~/.kube/config 选集群带 --context |
| K8s YAML 模板 | Deployment / Service / Ingress / ConfigMap / Secret / PVC / SA / Role / RoleBinding / HPA / CronJob / Namespace / DaemonSet 模板，带中文注释可编辑 |

### 🔐 编解码
Base64（文本 + 文件 + data URI）· URL 编解码 · 进制转换（2/8/10/16）· Unicode（\uXXXX）· JWT 解析（含过期时间）· HTML 实体

### 🔑 加密哈希
哈希计算（MD5 / SHA1 / SHA256 / SHA512 / SHA3）· 文件校验（大文件流式 + 比对）· AES-256 加解密（GCM / CBC / ECB，mbedTLS）· RSA 密钥对生成（PEM）· **SSL 证书查看**（PEM 解析：主题/颁发者/SAN/有效期，到期预警）

### 📝 文本开发
随机字符串 / 密码生成 · UUID 批量 · JSON 格式化 / 压缩 / 校验 / 转义 · YAML ↔ JSON · 时间戳转换（实时走秒）· 正则测试（分组捕获）· 文本 Diff（差异高亮）· Cron 解析（未来 5 次执行时间）· 二维码生成 · 文本批处理（去重 / 排序 / 统计）· SQL 格式化

### ⚙️ 系统信息
系统信息 · URL 解析器 · 颜色转换（HEX / RGB / HSL / CMYK）

## 🎨 界面

- Fluent 风格暗色 / 浅色双主题（顶栏一键切换，自动记忆）
- 左侧分类导航 + 顶栏全局搜索（支持中文 / 英文关键词，`Ctrl+K` 聚焦）
- 首页工具卡片网格；工具页统一「输入 → 执行 → 输出」布局，一键复制 / 输出回输
- Ping / 路由追踪 / 端口扫描流式输出，随时停止

## 📦 下载使用

**从 GitHub Releases 下载最新版**：[Releases · latest](../../releases/latest)

| 平台 | 产物 | 说明 |
|---|---|---|
| Windows x64 | `lingwtools-setup-1.5.0.exe` | **安装向导**：可选安装路径、开始菜单/桌面快捷方式，含卸载器 |
| Windows x64 | `lingwtools-windows-x64-portable.zip` | **绿色便携版**：解压双击即用，不写注册表 |
| macOS Apple Silicon | `lingwtools-macos-arm64.dmg` | 打开后将 `灵王工具箱.app` 拖入 Applications |

安装版与便携版功能完全一致；升级时直接运行新版 Setup 覆盖安装即可。

**macOS 首次打开**（应用未做公证）：

```bash
# 方式一：右键点击 app → 打开 → 再点“打开”
# 方式二：终端执行
xattr -cr /Applications/灵王工具箱.app
```

## 🛠️ 从源码构建

依赖：CMake ≥ 3.21、Qt 6.5+（Widgets / Network / Svg / Concurrent）、C++17 编译器。
第三方库（yaml-cpp、libqrencode、mbedTLS）已内置于 `3rdparty/`，无需额外安装。

**Windows 一键构建**（编译 + 部署运行时 + Inno Setup 安装包）：

```bat
scripts\build_all.bat
```

**手动构建**：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build

# Windows 生成便携包（进程名 lingwtools.exe；替换为你的 Qt 路径）
windeployqt --release --no-translations --compiler-runtime build/lingwtools.exe
# 生成安装向导（需 Inno Setup 6.5+）
ISCC scripts/installer.iss
```

### 自动出双平台包

推送到 GitHub 后打 tag 即可：

```bash
git tag v1.0.0 && git push origin v1.0.0
```

GitHub Actions（`.github/workflows/release.yml`）会在 `windows-latest` 与 `macos-14`（Apple Silicon）上构建，自动产出 **Windows 安装向导 + Windows 便携包 + macOS DMG** 三个产物并创建 Release。

> 注：macOS GUI 应用无法从 Windows 交叉编译，必须经由 CI 或真机构建。macOS 构建首次运行如遇小问题（如 ICMP 权限、Qt 模块差异），按提示微调即可。

## 📁 目录结构

```
lingwang-toolbox/
├── CMakeLists.txt            # 构建脚本（含第三方库集成）
├── theme.qrc                 # 主题 / 图标资源
├── src/
│   ├── main.cpp              # 入口
│   ├── mainwindow.*          # 主窗口：侧边栏 / 搜索 / 主题 / 首页
│   ├── registry.*            # 33 个工具的注册表
│   ├── iconprovider.*        # 内置 SVG 图标渲染
│   ├── widgets/              # ToolPage / TextToolPage / StreamPage / FlowLayout
│   ├── core/                 # 与 UI 解耦的工具逻辑
│   │   ├── icmp.*            #   原生 ICMP（Win: iphlpapi / macOS: SOCK_DGRAM）
│   │   ├── nettools.*        #   子网 / 端口扫描 / Whois
│   │   ├── codecs.*          #   Base64 / URL / JWT / Unicode / HTML
│   │   ├── texttools.*       #   JSON / YAML / Cron / Diff / SQL
│   │   ├── crypto.*          #   AES / RSA（mbedTLS）
│   │   └── sysinfo.*
│   ├── tools/                # 各工具页面
│   └── theme/                # dark.qss / light.qss
├── 3rdparty/                 # yaml-cpp / libqrencode / mbedtls（内置源码）
├── scripts/genicon.py        # 应用图标生成脚本
└── .github/workflows/release.yml
```

## ⚖️ 许可

本项目以 [MIT License](LICENSE) 开源，欢迎自由使用、修改与分发。

内置的第三方组件遵循其原始许可：
- Qt — LGPLv3（以动态链接方式使用）
- yaml-cpp — MIT
- libqrencode — LGPL-2.1+
- mbedTLS — Apache-2.0
