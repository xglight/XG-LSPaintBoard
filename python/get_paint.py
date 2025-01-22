import requests
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

url = ""

class GetPaint:
    def __init__(self, url):
        self.hearder = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36"
        }
        self.url = url
    
    def get(self):
        cnt = 0
        while cnt < 3:
            cnt += 1
            try:
                r = requests.get(self.url, headers=self.hearder)
            except requests.exceptions.RequestException as e:
                logging.error(e)
            except r.status_code!= 200:
                logging.error("Failed to get paint from server, status code: %d" % r.status_code)
            
            if r.status_code == 200:
                logging.info("Get paint successfully")
                with open("board", "wb") as f:
                    f.write(r.content)
                return 0
        if cnt == 3:
            return 1

def init_parser():
    global url
    parser = argparse.ArgumentParser(description='Get Paint')
    parser.add_argument('-u', '-url', help='url of the paint server', required=True)

    args = parser.parse_args()

    if args.u is not None:
        url = args.u
    else:
        logging.error("Please input the url of the paint server")
        sys.exit(1)

if __name__ == '__main__':
    logging.info("Start to get paint from server")
    logger = get_logger(logging.INFO)
    init_parser()
    paint = GetPaint(url)
    if(paint.get() != 0):
        logging.error("Failed to get paint from server")
        sys.exit(1)
    else:
        logging.info("Finish to get paint from server")
