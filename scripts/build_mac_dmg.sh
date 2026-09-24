#!/bin/bash
# 本地打包 macOS DMG（与 CI 产物同构：macdeployqt + icns + ad-hoc 签名 + hdiutil）
# 用法：scripts/build_mac_dmg.sh [版本号]   （默认读 CMakeLists 的 project VERSION）
# 产物：dist/lingwtools-macos-arm64.dmg —— 可直接 gh release upload
set -euo pipefail
cd "$(dirname "$0")/.."

VER="${1:-$(grep -o 'project(lingwtools VERSION [0-9.]*' CMakeLists.txt | awk '{print $3}')}"
QT_BIN="$(brew --prefix qt)/bin"
export PATH="$QT_BIN:$PATH"

echo "[1/5] 配置与编译（arm64 Release）…"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" >/dev/null
cmake --build build --target lingwtools

APP=build/lingwtools.app
echo "[2/5] 部署 Qt 框架（macdeployqt）…"
macdeployqt "$APP" -always-overwrite

echo "[3/5] 图标…"
mkdir -p appicon.iconset
for s in 16 32 64 128 256 512; do
  cp resources/icons/icon_${s}x${s}.png appicon.iconset/icon_${s}x${s}.png 2>/dev/null || true
  cp resources/icons/icon_$((s*2))x$((s*2)).png appicon.iconset/icon_${s}x${s}@2x.png 2>/dev/null || true
done
iconutil -c icns appicon.iconset -o lingwtools.icns
mkdir -p "$APP/Contents/Resources"
cp lingwtools.icns "$APP/Contents/Resources/"
/usr/libexec/PlistBuddy -c "Set :CFBundleIconFile lingwtools" "$APP/Contents/Info.plist"

echo "[4/5] ad-hoc 签名…"
codesign --force --deep -s - "$APP"

echo "[5/5] 生成 DMG（v${VER}）…"
rm -rf dmgstage dist && mkdir -p dmgstage dist
cp -R "$APP" dmgstage/
ln -s /Applications dmgstage/Applications
hdiutil create -volname "灵王工具箱" -srcfolder dmgstage -ov -format UDZO \
  "dist/lingwtools-macos-arm64.dmg" >/dev/null

echo "完成：dist/lingwtools-macos-arm64.dmg（v${VER}）"
echo "上传：gh release upload v${VER} dist/lingwtools-macos-arm64.dmg --clobber"
