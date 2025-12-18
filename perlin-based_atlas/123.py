from PIL import Image, ImageDraw, ImageFont
import os

W = H = 1024
GRID = 16
CELL = W // GRID

img = Image.new("RGBA", (W, H), (0,0,0,0))
draw = ImageDraw.Draw(img)

# 換成你想要的字型檔，例如 "C:/Windows/Fonts/consola.ttf"
font = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 48)

for code in range(256):
    col = code % GRID
    row = code // GRID
    x = col * CELL + 8
    y = row * CELL + 6
    ch = chr(code)
    draw.text((x, y), ch, font=font, fill=(255,255,255,255))

os.makedirs("assets", exist_ok=True)
img.save("assets/fonts.png")
print("saved assets/fonts.png")