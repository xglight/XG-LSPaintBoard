from PIL import Image
import sys
import logging
import colorlog
import argparse


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


rbg_file = "board.rgb"


def rgb_to_png(rbg_file, output_file='board.png'):
    try:
        with open(rbg_file, 'r') as f:
            try:
                width, height = map(int, f.readline().split())
            except ValueError:
                logging.error("Invalid file format: {}".format(rbg_file))
                sys.exit(1)
            except Exception as e:
                logging.error("Unexpected error: {}".format(e))
                sys.exit(1)
    except FileNotFoundError:
        logging.error("File not found: {}".format(rbg_file))
        sys.exit(1)
    except ValueError:
        logging.error("Invalid file format: {}".format(rbg_file))
        sys.exit(1)
    except Exception as e:
        logging.error("Unexpected error: {}".format(e))
        sys.exit(1)

    rgb_values = []

    with open(rbg_file, 'r') as f:
        f.readline()
        for line in f:
            try:
                y, x, r, g, b = map(int, line.split())
            except ValueError:
                logging.error("Invalid file format: {}".format(rbg_file))
                sys.exit()
            except Exception as e:
                logging.error("Unexpected error: {}".format(e))
            rgb_values.append((x, y, (r, g, b)))

    image = Image.new('RGB', (width, height))

    for x, y, (r, g, b) in rgb_values:
        image.putpixel((x, y), (r, g, b))

    try:
        image.save(output_file)
    except Exception as e:
        logging.error("Unexpected error: {}".format(e))
        sys.exit(1)


def init_argparse():
    global rbg_file
    global output_file
    parser = argparse.ArgumentParser(
        description='Convert RGB file to PNG file')
    parser.add_argument('-file', '-f', type=str, help='RGB file path')
    parser.add_argument('-output', '-o', type=str,
                        default='img.png', help='Output file path')

    args = parser.parse_args()

    if args.file is not None:
        rbg_file = args.file
    else:
        logging.error("Please input RGB file path")
        sys.exit(1)

    if args.output is not None:
        output_file = args.output


if __name__ == '__main__':
    output_file = ""

    logging = get_logger()
    init_argparse()
    logging.info("Start converting RGB file to PNG file...")

    rgb_to_png(rbg_file, output_file)

    logging.info("Finish converting")
