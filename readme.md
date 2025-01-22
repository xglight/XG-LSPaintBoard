# LSPaintBoard

洛谷冬日画板自动化脚本。

![](https://img.shields.io/badge/Python-3.10.11-blue) ![](https://img.shields.io/badge/C++-Clang_19.1.4-blue)

## 关于冬日画板

绘板地址: [https://paintboard.ayakacraft.com/](https://paintboard.ayakacraft.com/)

Api 文档: [https://www.luogu.com/article/57b4jd3c](https://www.luogu.com/article/57b4jd3c)

介绍: [https://www.luogu.com.cn/article/7yfdaqak](https://www.luogu.com.cn/article/7yfdaqak)

## 脚本介绍

使用 Python 进行网络交互，C++ 进行图片处理。

其中程序使用 Socket 通信。

如需调试，可以使用 [https://github.com/xglight/LSPaintBoard-Fake-Server](https://github.com/xglight/LSPaintBoard-Fake-Server) 项目。

### C++

-  [spdlog](https://github.com/gabime/spdlog) 库，用于日志输出。
-  [json](https://github.com/nlohmann/json) 库，用于配置文件读取。
-  wsock32.dll，用于 Socket 通信。

### Python

- [Pillow](https://github.com/python-pillow/Pillow) 库，用于图片处理。
- [websocket-client](https://github.com/websocket-client/websocket-client) 库，用于 Socket 通信。
- [logging](https://docs.python.org/3/library/logging.html) 库，用于日志输出。
- [socket](https://docs.python.org/3/library/socket.html) 库，用于 Socket 通信。

## 参数说明

### Socket

> 本脚本使用 Socket 通信，需要设置 Socket 可用的 UDP 端口。

| 参数名 |  类型  |   默认值    |    说明     |
| :----: | :----: | :---------: | :---------: |
|  host  | string | "localhost" | Socket 地址 |
|  port  | int[]  |     []      | Socket 端口 |

### Email

> 本脚本支持通过 Email 发送绘画通知。

|  参数名  |  类型  | 默认值 |     说明     |
| :------: | :----: | :----: | :----------: |
|  enable  |  bool  | false  |   是否启用   |
|   host   | string |   ""   | SMTP 服务器  |
|   port   |  int   |   0    |  SMTP 端口   |
| username | string |   ""   | SMTP 用户名  |
| password | string |   ""   |  SMTP 密码   |
|    to    | string |   ""   | 接受邮件邮箱 |

### Paint

|   参数名    |  类型  |                               默认值                                |                  说明                   |
| :---------: | :----: | :-----------------------------------------------------------------: | :-------------------------------------: |
|   ws_url    | string |     wss://api.paintboard.ayakacraft.com:32767/api/paintboard/ws     |             WebSocket 地址              |
|   api_url   | string | https://api.paintboard.ayakacraft.com:32767/api/paintboard/getboard |                API 地址                 |
|  img_file   | string |                              "img.rgb"                              |             rgb 图片文件名              |
| token_file  | string |                             "token.txt"                             |              token 文件名               |
| value_file  | string |                             "img.value"                             |              token 文件名               |
|   start_x   |  int   |                                  0                                  |          画图开始位置的横坐标           |
|   start_y   |  int   |                                  0                                  |          画图开始位置的纵坐标           |
| token_group |  int   |                                  5                                  |            每组 token 的数目            |
| time_limit  |  int   |                                 30                                  | token 的冷却时间，用于调试脚本，单位:秒 |
| thread_num  |  int   |                                  1                                  |              线程数 [1,7]               |

说明：

1. `img_file` 应为 RGB 格式的图片，第一行为为高、宽，接下来每一行 `y x r g b`，如果只有图片，用 `png_to_rgb.py` 脚本转换为 RGB 格式。
2. `value_file` 为图片每个像素点的权值，越高代表该像素点越容易被涂画，第一行为为高、宽，接下来每行 `y x v`。
3. `token_file` 应为 token 文件，每一行为`uid token`。

## 使用方法

在 `config.json` 中设置相关信息。

运行 `LSPaintBoard.exe` 启动脚本。

## 手动编译

> 如果 Python 未使用虚拟环境，可以把 `buildpython.ps1` 脚本中的第一行注释掉。

```bash
git clone https://github.com/xglight/LSPaintBoard.git
cd LSPaintBoard
mkdir build
.\buildpython.ps1
cd build
cmake ..
cmake --build .
```

