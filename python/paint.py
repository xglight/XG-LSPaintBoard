import socket
import logging
import threading
import websocket
import re
import struct
import time
import asyncio


class Socket_server:
    server_ip = ''
    port = 0
    server_name = ""

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
                client_socket, client_address = server_socket.accept()
                client_socket.sendall(server_name.encode('utf-8'))
                data = client_socket.recv(1024)
                client_name = data.decode('utf-8')
                client_socket.sendall(
                    (f"Welcome to the {server_name}!").encode('utf-8'))
                logging.info(f"Client[{client_name}] connected")

                with client_socket:
                    while True:
                        data = client_socket.recv(1024).decode('utf-8')
                        if not data:
                            break
                        logging.debug(
                            f"Received data from {client_name}: {data}")
                        
                        uid, token, r, g, b, x, y = data.split(" ")
                        x = int(x)
                        y = int(y)
                        r = int(r)
                        g = int(g)
                        b = int(b)

                        paint.paint(uid, token, r, g, b, x, y)
                        

class Socket_client:
    server_ip = ''
    port = 0
    client_name = ""
    client_socket = None

    def __init__(self, server_ip, port, client_name):
        self.server_ip = server_ip
        self.port = port
        self.client_name = client_name

    def start_client(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as self.client_socket:
            self.client_socket.connect((self.server_ip, self.port))
            self.client_socket.sendall(self.client_name.encode('utf-8'))
            data = self.client_socket.recv(1024)
            server_name = data.decode('utf-8')
            logging.info(f"Connected to server[{server_name}]")
            self.client_socket.sendall(self.client_name.encode('utf-8'))
            data = self.client_socket.recv(1024)
            logging.debug(f"Received data from server: {data.decode('utf-8')}")

    def send_data(self, data):
        self.client_socket.sendall(data.encode('utf-8'))
        logging.debug(f"Sent data to server: {data}")


class Paint:
    ws = None
    connected = False
    def on_message(self, status, event):
        offset = 0
        while(offset < len(event)):
            type = struct.unpack_from('B', event, offset)[0]
            offset += 1
            # logging.debug(f"event type: {type}")
            if type == 0xfa:
                x = struct.unpack_from('<H', event, offset)[0]
                y = struct.unpack_from('<H', event, offset+2)[0]
                r = struct.unpack_from('B', event, offset+4)[0]
                g = struct.unpack_from('B', event, offset+5)[0]
                b = struct.unpack_from('B', event, offset+6)[0]
                offset += 7
                # logging.info(f"({x},{y}) is painted with color ({r},{g},{b})")
            elif type == 0xfc:
                logging.debug("ping-pong")
                self.ws.send_bytes(bytes([0xfb]))
            elif type == 0xff:
                id = struct.unpack_from('<I', event, offset)[0]
                code = struct.unpack_from('B', event, offset+4)[0]
                logging.info(f"painting finished,id:{id},code:{hex(code)}")
                offset += 5
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
            self.ws.run_forever()
            try:
                self.ws.close()
            except:
                pass
    
    paintid = 0
    chunks = []
    total_size = 0
    
    def append_data(self, paintdata):
        logging.debug("append_data,len(paintdata): %d", len(paintdata))
        self.chunks.append(paintdata)
        self.total_size += len(paintdata)
    
    def get_merage_data(self):
        result = bytearray(self.total_size)
        offest = 0
        for chunk in self.chunks:
            offset = offest
            result[offset:offset + len(chunk)] = chunk
            offset += len(chunk)
        self.total_size = 0
        self.chunks = []
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

        self.append_data(paintData)

    def send_data(self):
        while True:
            # 检查是否有包需要发送，以及 WebSocket 连接是否已打开
            if self.total_size > 0 and self.connected:
                logging.debug("Start sending data,len(chunks): %d", len(self.chunks))
                self.ws.send_bytes(self.get_merage_data())
            time.sleep(0.2)

if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG,
                        format='%(asctime)s - %(levelname)s - %(message)s')

    server_name = "Paint"
    socket_server = Socket_server('10.0.3.113', 4500, server_name)
    socket_server_thread = threading.Thread(target=socket_server.start_server)
    socket_server_thread.start()

    paint = Paint()
    logging.info("Start sending data")
    send_data_thread = threading.Thread(target=paint.send_data)
    send_data_thread.start()

    paint.connect()


