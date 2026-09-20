#pragma once
class ToolPage;

// 所有工具页面的工厂函数（实现在 src/tools/*.cpp）
namespace pages {
// 网络诊断
ToolPage* createIpInfo();
ToolPage* createDns();
ToolPage* createPing();
ToolPage* createTraceroute();
ToolPage* createPortScan();
ToolPage* createHttpClient();
ToolPage* createSubnet();
ToolPage* createWhois();
ToolPage* createNicInfo();
// 编解码
ToolPage* createBase64();
ToolPage* createUrlCodec();
ToolPage* createRadix();
ToolPage* createUnicode();
ToolPage* createJwt();
ToolPage* createHtmlEntity();
// 加密哈希
ToolPage* createHash();
ToolPage* createAes();
ToolPage* createRsa();
ToolPage* createFileHash();
// 文本/开发
ToolPage* createRandom();
ToolPage* createUuid();
ToolPage* createJson();
ToolPage* createYaml();
ToolPage* createTimestamp();
ToolPage* createRegex();
ToolPage* createDiff();
ToolPage* createCron();
ToolPage* createQrcode();
ToolPage* createTextProc();
ToolPage* createSqlFormat();
// 容器运维
ToolPage* createK8sCmd();
ToolPage* createK8sYaml();
// 系统
ToolPage* createSysInfo();
ToolPage* createUrlParser();
ToolPage* createColor();
} // namespace pages
