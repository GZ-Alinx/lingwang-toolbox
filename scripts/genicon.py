#!/usr/bin/env python3
"""生成灵王工具箱应用图标：app.ico（Windows）+ app.png（macOS iconset 源）"""
import os
from PIL import Image, ImageDraw

SIZE = 1024
BLUE = (61, 109, 216)
BLUE_LIGHT = (94, 152, 255)
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "resources", "icons")


def make_icon() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # 背景圆角方块（垂直渐变蓝）
    radius = int(SIZE * 0.22)
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, SIZE - 1, SIZE - 1], radius=radius, fill=255)
    grad = Image.new("RGBA", (SIZE, SIZE))
    gd = ImageDraw.Draw(grad)
    for y in range(SIZE):
        t = y / SIZE
        color = tuple(int(BLUE_LIGHT[i] + (tuple(map(lambda a, b: a - b, (35, 62, 140), BLUE_LIGHT))[i]) * t * 0.55)
                      for i in range(3)) + (255,)
        gd.line([(0, y), (SIZE, y)], fill=color)
    img.paste(grad, (0, 0), mask)

    # 白色闪电（zap 风格多边形）
    cx, cy = SIZE / 2, SIZE / 2
    s = SIZE / 100
    pts = [
        (cx + 8 * s, cy - 42 * s),
        (cx - 22 * s, cy + 6 * s),
        (cx - 2 * s, cy + 6 * s),
        (cx - 8 * s, cy + 42 * s),
        (cx + 22 * s, cy - 6 * s),
        (cx + 2 * s, cy - 6 * s),
    ]
    d.polygon(pts, fill=(255, 255, 255, 245))
    return img


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    icon = make_icon()
    # Windows .ico（多尺寸）
    ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
    icon.save(os.path.join(OUT_DIR, "app.ico"), format="ICO", sizes=ico_sizes)
    # macOS 源 PNG + 嵌入资源
    for s in (16, 32, 64, 128, 256, 512, 1024):
        icon.resize((s, s), Image.LANCZOS).save(os.path.join(OUT_DIR, f"icon_{s}x{s}.png"))
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(OUT_DIR, "app.png"))
    print("icons written to", os.path.abspath(OUT_DIR))


if __name__ == "__main__":
    main()
