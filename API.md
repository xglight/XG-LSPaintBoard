冬日绘版 2025 API 文档及其实现@Federico2903

专栏地址：[Here](https://www.luogu.com.cn/article/57b4jd3c)。

# HTTP API

## 获取版面

|     类型 | 操作                           |
| -------: | :----------------------------- |
|     请求 | `GET /api/paintboard/getboard` |
| 响应主体 | `application/octet-stream`     |

Javascript 解码响应内容：

```javascript
for (let y = 0; y < 600; y++) {
    for (let x = 0; x < 1000; x++) {
        // (y, x) 的颜色编号：
        const color = ('00000' +
            (
                byteArray[y * 1000 * 3 + x * 3] * 0x10000 +
                byteArray[y * 1000 * 3 + x * 3 + 1] * 0x100 +
                byteArray[y * 1000 * 3 + x * 3 + 2]
            ).toString(16)
        ).substr(-6);
    }
}
```

## 获取 Token

|     类型 | 操作                                                   |
| -------: | :----------------------------------------------------- |
|     请求 | `POST /api/auth/gettoken`                              |
|     参数 | `application/json`（`{ uid: number, paste: string }`） |
| 响应主体 | `application/json`（`DataResponse<Token>`）            |

`uid`：需要获取 Token 的 uid。

`paste`：对应的剪贴板。

`Token`：`{ token?: string, errorType?: string }`。

`Token.errorType`：

- `PASTE_NOT_FOUND` 找不到剪贴板。
- `UID_MISMATCH` uid 不匹配。
- `CONTENT_MISMATCH` 内容不匹配。
- `SERVER_ERROR` 服务器错误。
- `BAD_REQUEST` 请求格式错误。

# WebSocket API

绘版后端已经改用粘性发包，其机制即为将所有的二进制信息拼接。

可以参见后文“粘包发送”。

下方介绍的均为一个包单元，请自行拆包。

操作码的定义：每个包的第一个字节的内容。

## 服务端侧（S2C）

### Heartbeat (Ping)

| 数据位置 | 操作码 |
| :------: | :----: |
| 数据内容 |  0xfc  |

当你收到该操作时，应当立即应答，参见“客户端侧 Heartbeat (Pong)”。

### 绘画消息

| 数据位置 | 操作码 |    Uint16     |    Uint16     |    Uint8    |    Uint8    |    Uint8    |
| :------: | :----: | :-----------: | :-----------: | :---------: | :---------: | :---------: |
| 数据内容 |  0xfa  | 事件 $x$ 坐标 | 事件 $y$ 坐标 | 新的 $R$ 值 | 新的 $G$ 值 | 新的 $B$ 值 |

### 绘画结果

| 数据位置 | 操作码 |   Uint32   | Uint8  |
| :------: | :----: | :--------: | :----: |
| 数据内容 |  0xff  | 绘图识别码 | 状态码 |

状态码解释：

- `0xef` 成功。
- `0xee` 正在冷却。
- `0xed` Token 无效。
- `0xec` 请求格式错误。
- `0xeb` 无权限。
- `0xea` 服务器错误。

## 客户端侧（C2S）

### Heartbeat (Pong)

| 数据位置 | 操作码 |
| :------: | :----: |
| 数据内容 |  0xfb  |

当收到“服务端侧 Heartbeat (Ping)”时立即进行此操作。

### 绘画操作

前 $8$ 字节：

| 数据位置 | 操作码 |     Uint16      |     Uint16      | Uint8  | Uint8  | Uint8  |
| :------: | :----: | :-------------: | :-------------: | :----: | :----: | :----: |
| 数据内容 |  0xfe  | 绘画的 $x$ 坐标 | 绘画的 $y$ 坐标 | $R$ 值 | $G$ 值 | $B$ 值 |

后 $21$ 字节：

| 数据位置 |         Uint24 (Uint8 * 3)          |      Uint128       |   Uint32   |
| :------: | :---------------------------------: | :----------------: | :--------: |
| 数据内容 | Token 的 uid，拆分成三个 Uint8 发送 | Token，本质是 UUID | 绘图识别码 |

理论上识别码需要唯一，至少在服务器返回信息前是唯一的。

## API 限制

服务端侧作出如下限制：

- 每秒钟每个 WebSocket 连接可以发送至多 $128$ 个包。
- 每个 IP 地址最多可以建立 $7$ 个 WebSocket 连接。

违反上述限制可能会导致 IP 被暂时封禁，WebSocket 尝试 Upgrade 时返回状态码 $429$，或连接时返回 $1008^{[1]}$。

## 切断连接

服务端可能会出于某些原因主动切断你的 WebSocket 连接，并返回状态码和原因。

状态码解释如下：

- $\text{1001 Ping timeout}$（心跳超时）
- $\text{1002 Protocol violation}^{[2]}$（协议错误）
- $\text{1008 IP connection limit exceeded}$（连接数超限）
- $\text{1011 Server error processing message}$（服务端错误）

$[1]$：后端代码中的写法是在 Upgrade 前即返回，然而其 WebSocket 逻辑中也进行了判定，此时返回 $\text{1008\color{blue} IP is banned}$。

$[2]$：后端代码中其有三个返回信息，分别是：

- $\text{Protocol violation: unexpected pong}$  
  错误的心跳，在服务端未请求心跳时进行心跳。
- $\text{Protocol violation: unknown packet type}$  
  错误的包单元操作码。
- $\text{Protocol violation: duplicate ping state}$  
  服务端在上一个心跳超时时长中请求了下一个心跳，一般不会出现此错误，出现了此错误请上报。

# NodeJS 实现

## WebSocket 连接

NodeJS 中存在 `WebSocket` 对象，从 ws 中导入即可。

通过将函数绑定到 `onopen`，`onclose`，`onerror` 和 `onmessage` 上即可进行事件处理。

```javascript
import WebSocket from 'ws';
const WS_URL = "wss://api.paintboard.ayakacraft.com:32767/api/paintboard/ws";

ws = new WebSocket(WS_URL);
ws.binaryType = "arraybuffer";
ws.onopen = () => {
	console.log("WebSocket 连接已打开。");
};

ws.onmessage = (event) => {
	const buffer = event.data;
	const dataView = new DataView(buffer);
	// 处理你的数据
	// 我建议使用 DataView 处理
};

ws.onerror = (err) => {
	console.error(`WebSocket 出错：${err.message}。`);
};

ws.onclose = (err) => {
	const reason = err.reason ? err.reason : "Unknown";
	console.log(`WebSocket 已经关闭 (${err.code}: ${reason})。`);
};
```

## 数据处理

按照上文的操作码对包单元分别处理：

```javascript
let offset = 0;
while (offset < buffer.byteLength) {
	const type = dataView.getUint8(offset);
	offset += 1;
	switch (type) {
		case 0xfa: {
			const x = dataView.getUint16(offset, true);
			const y = dataView.getUint16(offset + 2, true);
			const colorR = dataView.getUint8(offset + 4);
			const colorG = dataView.getUint8(offset + 5);
			const colorB = dataView.getUint8(offset + 6);
			offset += 7;
			// 此时在 (x, y) 进行了一次颜色为 (R, G, B) 的绘画
			break;
		}
		case 0xfc: {
			ws.send(new Uint8Array([0xfb]));
			break;
		}
		case 0xff: {
			const id = dataView.getUint32(offset, true);
			const code = dataView.getUint8(offset + 4);
			offset += 5;
			// 绘画任务返回
			// 可以使用存储回调函数的方式实现
			break;
		}
		default:
			console.log(`未知的消息类型：${type}`);
	}
}
```

## 粘包发送

生成粘包的方式非常简单，直接拼接即可。

为了优化性能，我们使用一个队列把所有的包单元存储起来一起拼接。

```javascript
let chunks = [];
let totalSize = 0;

function appendData(paintData) {
    chunks.push(paintData);
    totalSize += paintData.length;
}

function getMergedData() {
    let result = new Uint8Array(totalSize);
    let offset = 0;
    for (let chunk of chunks) {
        result.set(chunk, offset);
        offset += chunk.length;
    }
	totalSize = 0;
	chunks = [];
    return result;
}
```

## 绘画操作

所有的数字都用小端序存储。

请不要忘记这一点，可以参考下方的 `uintToUint8Array`。

切记使用粘包发送，否则可能会被暂时封禁 IP。

```javascript
let paintId = 0;

function uintToUint8Array(uint, bytes) {
	const array = newUint8Array(bytes);
	for (let i = 0; i < bytes; i++) {
		array[i] = uint & 0xff;
		uint = uint >> 8;
	}
	return array;
}

async function paint(uid, token, r, g, b, nowX, nowY) {
	const id = (paintId++) % 4294967296;
	paintCnt++;
	const tokenBytes = new Uint8Array(16);
	token.replace(/-/g, '').match(/.{2}/g).map((byte, i) =>
		tokenBytes[i] = parseInt(byte, 16));

	const paintData = new Uint8Array([
		0xfe,
		...uintToUint8Array(nowX, 2),
		...uintToUint8Array(nowY, 2),
		r, g, b,
		...uintToUint8Array(uid, 3),
		...tokenBytes,
		...uintToUint8Array(id, 4)
	]);

	appendData(paintData);
}

setInterval(() => {
  // 检查是否有包需要发送，以及 WebSocket 连接是否已打开
	if (chunks.length > 0 && ws.readyState === WebSocket.OPEN) {
		ws.send(getMergedData());
	}
}, 20);
// 20 毫秒发送一次，每秒 50 个包
```

## 图像处理

有大量的第三方库可以用来处理图像，我使用的是 sharp。

直接从 sharp 中导入 `sharp` 即可：

```javascript
import sharp from 'sharp';
```

### 读取图像

调用 sharp 的构造函数并传入地址即可：

```javascript
const image = sharp('/path/to/image');
```

这样就可以创建一个包含图像数据的 sharp 对象。

### 图像元数据

调用**异步**成员函数 `metadata` 可以获取图像的元数据，包括但不限于宽高，通道数。

```javascript
const metadata = await image.metadata();
const { width, height, channels } = metadata;
// width 表示图像的宽，height 表示图像的高，channels 是图像的通道数
```

请注意有的图像是四通道（RGBA），而有的图像是三通道（RGB），如果通道数处理不善会出现像素错误。

### 图像像素数据

调用**异步**函数 `raw().toBuffer` 可以获取图像的像素信息，其返回一个数组。

每相邻的**通道数个元素**表示一个像素点的信息。

例如四通道的图片的四维信息（RGBA）分别在 $i, i + 1, i + 2, i + 3$ 的位置，三通道的三维信息（RGB）分别在 $i, i + 1, i + 2$ 的位置。

我们只需要提取 RGB 信息即可：

```javascript
const pixels = await image.raw().toBuffer();
const pixelData = [];
for (let i = 0; i < pixels.length; i += channels) {
	const r = pixels[i];
	const g = pixels[i + 1];
	const b = pixels[i + 2];
	pixelData.push({ r, g, b });
}
```