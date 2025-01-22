import argparse
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

height = 10
width = 10
default_weight = 1

def init_argparse():
    global height
    global width
    global default_weight
    parser = argparse.ArgumentParser(description='Generate valuefile for LSPaintBoard')
    parser.add_argument('-height',type = int, default=10, help='height of the img')
    parser.add_argument('-width',type = int, default=10, help='width of the img')
    parser.add_argument('-default','-d',type = int, default=1, help='default value of the weight')

    args = parser.parse_args()

    if args.height is not None:
        height = args.height
    if args.width is not None:
        width = args.width
    if args.default is not None:
        default_weight = args.default

if __name__ == '__main__':
    logger = get_logger(logging.INFO)
    init_argparse()
    logger.info("height:{}, width:{}, default_weight:{}".format(height, width, default_weight))

    with open('img.value', 'w') as f:
        for j in range(width):
            for i in range(height):
                f.write(str(i) + ' ' + str(j) + ' ' + str(default_weight) + '\n')
    