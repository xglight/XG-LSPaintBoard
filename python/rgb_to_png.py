from PIL import Image
import sys
import logging
import colorlog

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

def rgb_to_png(rbg_file,output_file='board.png'):
    try:
        with open(rbg_file, 'r') as f:
            try:
                width, height = map(int, f.readline().split())
            except ValueError:
                logging.error("Invalid file format: {}".format(rbg_file))
                sys.exit()
            except Exception as e:
                logging.error("Unexpected error: {}".format(e))
    except FileNotFoundError:
        logging.error("File not found: {}".format(rbg_file))
        sys.exit()
    except ValueError:
        logging.error("Invalid file format: {}".format(rbg_file))
        sys.exit()
    except Exception as e:
        logging.error("Unexpected error: {}".format(e))

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

if __name__ == '__main__':
    output_file = ""

    logging = get_logger()
    logging.info("Start converting RGB file to PNG file...")
    
    if len(sys.argv) == 2:
        rbg_file = sys.argv[1]
        rgb_to_png(rbg_file)
        logging.info("RGB file: {}".format(rbg_file))
    else:
        logging.error("Please specify the RGB file path")
        sys.exit()
    
    if len(sys.argv) == 3:
        output_file = sys.argv[2]
        rgb_to_png(rbg_file,output_file)
        logging.info("Output file: {}".format(output_file))
    else:
        output_file = "board.png"
        logging.info("Output file: board.png")
    
    rgb_to_png(rbg_file,output_file)

    logging.info("Finish converting")
    

