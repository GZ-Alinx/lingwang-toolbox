; 灵王工具箱 Windows 安装包脚本（Inno Setup 6.5+）
; 编译：ISCC.exe scripts\installer.iss
; 产物：dist\LingWangToolbox-Setup-<版本>.exe

#define MyAppName "灵王工具箱"
#define MyAppNameEn "LingWangToolbox"
#define MyAppVersion "1.1.1"
#define MyAppPublisher "LingWang"
#define MyAppExeName "lingwtools.exe"

[Setup]
AppId={{F8A0E7D2-3B44-4C5A-9B6E-1C2D3E4F5A6B}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} v{#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoDescription={#MyAppName} 安装程序
; 默认安装到 Program Files（所有用户）或用户目录（仅当前用户），向导中可自行修改路径
DefaultDirName={autopf}\{#MyAppNameEn}
; 关键：显式禁用“自动跳过目录页”。Inno 6.3+ 在检测到旧版本（升级安装）时会
; 自动精简向导流程，导致用户无法选择安装目录——必须显式关掉这一行为
DisableDirPage=no
DisableReadyPage=no
UsePreviousAppDir=yes
DirExistsWarning=no
AppendDefaultDirName=no
PrivilegesRequiredOverridesAllowed=dialog commandline
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableWelcomePage=no
OutputDir=..
OutputBaseFilename=lingwtools-setup-{#MyAppVersion}
SetupIconFile=..\resources\icons\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
; 不用 checkedonce：升级安装时同样展示快捷方式选项
Name: "desktopicon"; Description: "创建桌面快捷方式(&D)"; GroupDescription: "附加任务："
Name: "startmenuicon"; Description: "创建开始菜单快捷方式(&S)"; GroupDescription: "附加任务："; Flags: unchecked

[Files]
Source: "..\dist\lingwtools.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startmenuicon
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"; Tasks: startmenuicon
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即运行 {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 预留：应用未来若产生缓存文件，可在此声明卸载时清理
