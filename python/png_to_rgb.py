from PIL import Image

img_file = "head.png"

img = Image.open(img_file)

with open("head.rgb", "w") as f:
    f.write(str(img.size[1]) + ' ' + str(img.size[0]) + '\n')

    for i in range(img.size[0]):
        for j in range(img.size[1]):
            r, g, b = img.getpixel((i, j))
            f.write(str(j) + ' '+str(i) +' ' + str(r) +' ' + str(g) +' ' + str(b) + '\n')