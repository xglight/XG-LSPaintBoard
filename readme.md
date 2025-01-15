# LSPaintBoard

洛谷冬日画板自动化脚本。

![](https://img.shields.io/badge/Python-3.10.11-blue) ![](https://img.shields.io/badge/C++-Clang_19.1.4-blue)

## 关于冬日画板

绘板地址: [https://paintboard.ayakacraft.com/](https://paintboard.ayakacraft.com/)

Api 文档: [https://www.luogu.com/article/57b4jd3c](https://www.luogu.com/article/57b4jd3c)

介绍: [https://www.luogu.com.cn/article/7yfdaqak](https://www.luogu.com.cn/article/7yfdaqak)

## 脚本介绍

`LSPaintBoard.cpp` 主程序，负责除绘画外的一切处理。

`paint.py` 绘画脚本，负责调用 API 绘制图片。

`rgb_to_png.py` 图片转换脚本，用于将 RGB 格式的图片转换为 PNG 格式。

`config.json` 配置文件，用于设置相关信息。

### C++

-  [CPR](https://github.com/libcpr/cpr) 库，用于网络通信。
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

| 参数名 | 类型  | 默认值 |    说明     |
| :----: | :---: | :----: | :---------: |
|  port  |  int  |  4500  | Socket 端口 |

### Email

> 本脚本支持通过 Email 发送绘画通知。

|  参数名  |  类型  | 默认值 |     说明     |
| :------: | :----: | :----: | :----------: |
|  enable  |  bool  | false  |   是否启用   |
|  server  | string |   ""   | SMTP 服务器  |
|   port   |  int   |   0    |  SMTP 端口   |
| username | string |   ""   | SMTP 用户名  |
| password | string |   ""   |  SMTP 密码   |
|    to    | string |   ""   | 接受邮件邮箱 |

### Paint

|   参数名    |  类型  |   默认值    |         说明         |
| :---------: | :----: | :---------: | :------------------: |
|  img_file   | string |  "img.txt"  |    rgb 图片文件名    |
| token_file  | string | "token.txt" |     token 文件名     |
| value_file  | string | "img.value" |     token 文件名     |
|   start_x   |  int   |      0      | 画图开始位置的横坐标 |
|   start_y   |  int   |      0      | 画图开始位置的纵坐标 |
| token_group |  int   |      5      |  每组 token 的数目   |

说明：

1. `img_file` 应为 RGB 格式的图片，第一行为为高、宽，接下来每一行 `y x r g b`，如果只有图片，用 `png_to_rgb.py` 脚本转换为 RGB 格式。
2. `value_file` 第一行为为高、宽，接下来每行 `y x v`。
3. `token_file` 应为 token 文件，每一行为`uid token`。

## 使用方法

在 `config.json` 中设置相关信息。

运行 `LSPaintBoard.exe` 启动脚本。

