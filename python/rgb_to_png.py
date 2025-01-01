from PIL import Image

rbg_file = "board_rgb.txt"

# 读入图片的宽度和高度
with open(rbg_file, 'r') as f:
    height, width = map(int, f.readline().split())

# 读入 RGB 值
rgb_values = []
with open(rbg_file, 'r') as f:
    f.readline()  # 跳过第一行
    # 一行五个值，前两个代表坐标，后三代表 RGB 值
    for line in f:
        y, x, r, g, b = map(int, line.split())
        rgb_values.append((x, y, (r, g, b)))

# 创建一个新的图片对象
image = Image.new('RGB', (width, height))

# 填充颜色
for x, y, (r, g, b) in rgb_values:
    image.putpixel((x, y), (r, g, b))
    

# 保存图片
image.save('board.png')

# 显示图片
# image.show()
