import socket
import logging
import colorlog
import threading
import websocket
import re
import struct
import time
import json

config_path = "config.json"
socket_ip = ""
socket_port = 0
socket_ser = None

class Socket_server:
    server_ip = ''
    port = 0
    server_name = ""
    client_socket = None

    def __init__(self, server_ip, port, server_name):
        self.server_ip = server_ip
        self.port = port
        self.server_name = server_name

    def start_server(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
            server_socket.bind((self.server_ip, self.port))
            server_socket.listen()
            logging.info(
                f"socket server is starting,listening on {self.server_ip}:{self.port}")

            while True:
                self.client_socket, client_address = server_socket.accept()
                logging.info(f"Client connected: {client_address}")
                self.client_socket.sendall(self.server_name.encode('utf-8'))
                try:
                    data = self.client_socket.recv(1024)
                except Exception as e:
                    logging.error(f"Failed to receive data: {e}")

                if not data:
                    logging.warning("No data received from client.")
                    continue

                client_name = data.decode('utf-8')
                self.client_socket.sendall(
                    (f"Welcome to the {self.server_name}!").encode('utf-8'))
                logging.info(f"Client[{client_name}] connected")

                with self.client_socket:
                    while True:
                        try:
                            data = self.client_socket.recv(1024)
                        except Exception as e:
                            logging.error(f"Failed to receive data: {e}")
                            exit(1)

                        if not data:
                            logging.info(f"Client[{client_name}] disconnected.")
                            break
                        data = data.decode('utf-8')
                        logging.debug(
                            f"Received data from {client_name}: {data}")
                        
                        chunks = data.split(",")
                        
                        for chunk in chunks:
                            if not chunk:
                                continue
                            
                            parts = chunk.split(" ")
                            if len(parts) != 7:
                                logging.warning(f"Invalid data format: {chunk}")
                                continue

                            uid, token, r, g, b, x, y = parts
                            try:
                                x = int(x)
                                y = int(y)
                                r = int(r)
                                g = int(g)
                                b = int(b)
                            except ValueError:
                                logging.warning(f"Invalid number format: {chunk}")
                                continue

                            lspaint.paint(uid, token, r, g, b, x, y)
    
    def sendmsg(self, message):
        try:
            logging.debug(f"Sent message to client: {message}")
            self.client_socket.sendall(message.encode('utf-8'))
        except Exception as e:
            logging.error(f"Failed to send message: {e}")
                        

class LSPaint:
    ws = None
    connected = False
    id_uid_map = {}

    def on_message(self, ws, event):
        global socket_ser
        ls = 0
        while(ls < len(event)):
            type = struct.unpack_from('B', event, ls)[0]
            ls += 1
            # logging.debug(f"event type: {type}")
            if type == 0xfa:
                x = struct.unpack_from('<H', event, ls)[0]
                y = struct.unpack_from('<H', event, ls+2)[0]
                r = struct.unpack_from('B', event, ls+4)[0]
                g = struct.unpack_from('B', event, ls+5)[0]
                b = struct.unpack_from('B', event, ls+6)[0]
                ls += 7
                # logging.info(f"({x},{y}) is painted with color ({r},{g},{b})")
            elif type == 0xfc:
                logging.debug("ping-pong")
                self.ws.send_bytes(bytes([0xfb]))
            elif type == 0xff:
                id = struct.unpack_from('<I', event, ls)[0]
                code = struct.unpack_from('B', event, ls+4)[0]
                logging.debug(f"painting finished,id:{id},code:{hex(code)}")
                ls += 5
                socket_ser.sendmsg(f"{self.id_uid_map[id]} {code}")
            else:
                logging.warning(f"Unknown event type: {type}")


    def on_open(self, ws):
        self.connected = True
        logging.info("Connected to server")

    def on_close(self, ws, close_status_code, close_msg):
        self.connected = False
        logging.warning(f"Connection closed: {close_status_code} - {close_msg}")

    def connect(self):
        ws_url = "wss://api.paintboard.ayakacraft.com:32767/api/paintboard/ws"
        # websocket.enableTrace(True)
        self.ws = websocket.WebSocketApp(
            ws_url,
            on_open=self.on_open,
            on_message=self.on_message,
            on_close=self.on_close,
            on_error=lambda ws, error: logging.error(error)
        )
        logging.info("Connecting to server...")

        while True:
            try:
                self.ws.run_forever(http_proxy_port=3116)
            except Exception as e:
                logging.error(f"WebSocket connection error: {e}")
            finally:
                # 连接失败时的处理
                time.sleep(1)  # 延时重试
    
    paintid = 0
    chunks = []
    total_size = 0
    
    def append_data(self, paintdata):
        logging.debug("append_data,len(paintdata): %d", len(paintdata))
        self.chunks.append(paintdata)
        self.total_size += len(paintdata)
    
    def get_merage_data(self):
        result = bytearray(self.total_size)
        i = 0
        for chunk in self.chunks:
            result[i:i + len(chunk)] = chunk
            i += len(chunk)
        self.total_size = 0
        self.chunks.clear()
        return result

    def uintToUint8Array(self,uint, bytes):
        uint = int(uint)
        array = bytearray(bytes)
        for i in range(bytes):
            array[i] = uint & (0xff)
            uint = uint >> 8
        return array

    def paint(self,uid,token,r,g,b,x,y):
        id = (self.paintid) % 4294967296
        logging.debug(f"({x},{y}) is painted with color ({r},{g},{b}),uid:{uid},token:{token},id:{id}")
        self.paintid += 1

        token_cleaned = token.replace("-", "")
        bytes_list = re.findall(".{2}", token_cleaned)
        tokenBytes = [int(byte, 16) for byte in bytes_list]

        paintData = bytearray([
            0xfe,
            *self.uintToUint8Array(x, 2),
            *self.uintToUint8Array(y, 2),
            r, g, b,
            *self.uintToUint8Array(uid, 3),
            *tokenBytes,
            *self.uintToUint8Array(id, 4)
        ])
        
        self.id_uid_map[id] = uid

        self.append_data(paintData)

    def send_data(self):
        while True:
            # 检查是否有包需要发送，以及 WebSocket 连接是否已打开
            if self.total_size > 0 and self.connected:
                logging.debug("Start sending data,len(chunks): %d", len(self.chunks))
                self.ws.send_bytes(self.get_merage_data())
            time.sleep(0.2)

def init():
    global socket_ip, socket_port
    logging.info("Intializing...")

    with open(config_path, "r") as f:
        config = json.load(f)
        logging.debug(config)
    
    socket_ip = config["socket"]["server"]
    socket_port = config["socket"]["port"]

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

if __name__ == "__main__":
    logging = get_logger(logging.DEBUG)
    logging.info("Starting")

    init()

    socket_ser = Socket_server(socket_ip, socket_port, "Paint")
    socket_server_thread = threading.Thread(target=socket_ser.start_server)
    socket_server_thread.start()

    lspaint = LSPaint()
    logging.info("Start sending data")
    send_data_thread = threading.Thread(target=lspaint.send_data)
    send_data_thread.start()

    lspaint.connect()


