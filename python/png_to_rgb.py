from PIL import Image
import logging
import colorlog
import sys

def get_logger(level=logging.INFO):
    # 创建logger对象
    logger = logging.getLogger()
    logging.basicConfig(level=logging.DEBUG,
                        format='%(asctime)s - %(levelname)s: %(message)s')
    logger.setLevel(level)
    # 创建控制台日志处理器
    console_handler = logging.StreamHandler()
    console_handler.setLevel(level)
    # 定义颜色输出格式
    color_formatter = colorlog.ColoredFormatter(
        '[%(asctime)s] [%(log_color)s%(levelname)s%(reset)s] %(message)s',
        log_colors={
            'DEBUG': 'cyan',
            'INFO': 'green',
            'WARNING': 'yellow',
            'ERROR': 'red',
            'CRITICAL': 'red,bg_white',
        }
    )
    # 将颜色输出格式添加到控制台日志处理器
    console_handler.setFormatter(color_formatter)
    # 移除默认的handler
    for handler in logger.handlers:
        logger.removeHandler(handler)
    # 将控制台日志处理器添加到logger对象
    logger.addHandler(console_handler)
    return logger

img_file = ""

def convert_to_rgb(img_file, output_file="img.rgb"):
    img = Image.open(img_file)
    try:
        with open(output_file, "w") as f:
            f.write(str(img.size[1]) + ' ' + str(img.size[0]) + '\n')

            for i in range(img.size[0]):
                for j in range(img.size[1]):
                    r, g, b = img.getpixel((i, j))
                    f.write(str(j) + ' '+str(i) +' ' + str(r) +' ' + str(g) +' ' + str(b) + '\n')
    except Exception as e:
        logging.error("Error: " + str(e))

if __name__ == '__main__':
    output_file = ""

    logging = get_logger()
    logging.info("Start converting")

    if len(sys.argv) > 1:
        img_file = sys.argv[1]
        logging.info("Image file path: " + img_file)
    else:
        logging.error("Please input image file path")
    sys.exit()
    
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
        logging.info("Output file path: " + output_file)
    else:
        output_file = "img.rgb"
        logging.info("Output file path: " + output_file)

    convert_to_rgb(img_file, output_file)

    logging.info("Finish converting")
    