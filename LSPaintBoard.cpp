#include <cpr/cprtypes.h>
#include <cpr/timeout.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <psdk_inc/_socket_types.h>
#include <string>
#include "cpr/cpr.h"
#include "spdlog/spdlog.h"
#include <vector>
#include <curl/curl.h>
#include "winsock2.h"

namespace SocketServer {
int port;
WSADATA wsaData;
SOCKADDR_IN addrSrv, addrClient[1024];
std::string ip[1024], username[1024];
int ic = -1, cnt = 1, len;
std::set<int> id_free, id_have;
std::map<std::string, int> client;
SOCKET sockSrv, sockcli[1024];
HANDLE hThread[1024], cThread[1024];
std::mutex mtx;

DWORD WINAPI client_server(LPVOID lpParamter) {
    if (lpParamter == nullptr) {
        spdlog::error("lpParamter is nullptr");
        return 1;
    }

    char recvBuf[1024];

    int id = client[(char *)lpParamter];

    spdlog::info("Client[{}] server started, id: {}", username[id], id);

    while (1) {
        memset(recvBuf, 0, sizeof(recvBuf));

        int result = recv(sockcli[id], recvBuf, sizeof(recvBuf), 0);
        if (result == 0) {
            spdlog::info("Client[{}] disconnected", username[id]);
            closesocket(sockcli[id]);
            sockcli[id] = INVALID_SOCKET;
            client[(char *)lpParamter] = 0;
            {
                std::lock_guard<std::mutex> lock(mtx);
                id_free.insert(id), id_have.erase(id);
            }
            break;
        } else if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            spdlog::error("Recv failed with error: {}", error);
            if (error == WSAEWOULDBLOCK) {
                continue;
            } else {
                closesocket(sockcli[id]);
                sockcli[id] = INVALID_SOCKET;
                client[(char *)lpParamter] = 0;
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    id_free.insert(id), id_have.erase(id);
                }
                break;
            }
        } else {
            spdlog::debug("Received message from {}: {}", username[id], recvBuf);
        }
    }
    spdlog::info("Client[{}] server stopped", username[id]);
    return 0;
}

DWORD WINAPI connect_client(LPVOID lpParamter) {
    char recvBuf[1024], sendBuf[1024];

    int id = 0;
    for (int i = strlen((char *)lpParamter) - 1; i >= 0; i--)
        id = id * 10 + ((char *)lpParamter)[i] - '0';

    sprintf(sendBuf, "LSPaintboard");
    int iSend = send(sockcli[id], sendBuf, strlen(sendBuf), 0);
    if (iSend == SOCKET_ERROR) {
        spdlog::error("One client connect failed with error: {}", WSAGetLastError());
        return false;
    }
    bool f = 0;
    do {
        memset(recvBuf, 0, sizeof(recvBuf));
        int result = recv(sockcli[id], recvBuf, sizeof(recvBuf), 0);
        if (result == 0) {
            spdlog::info("Client[{}] disconnected", inet_ntoa(addrClient[id].sin_addr));
            closesocket(sockcli[id]);
            client[(char *)recvBuf] = 0;
            {
                std::lock_guard<std::mutex> lock(mtx);
                id_free.insert(id), id_have.erase(id);
            }
            return false;
        } else if (result < 0) {
            spdlog::error("Client[{}] recv failed with error: {}", inet_ntoa(addrClient[id].sin_addr), WSAGetLastError());
            closesocket(sockcli[id]);
            client[(char *)recvBuf] = 0;
            {
                std::lock_guard<std::mutex> lock(mtx);
                id_free.insert(id), id_have.erase(id);
            }
            return false;
        } else {
            client[(char *)recvBuf] = id;
            sprintf(sendBuf, "Welcome to LSPaintboard IP:[%s],%s", inet_ntoa(addrClient[id].sin_addr), (char *)recvBuf);
            spdlog::debug("Send message to client[{}]: {}", (char *)recvBuf, sendBuf);
            send(sockcli[id], sendBuf, strlen(sendBuf), 0);
            break;
        }
    } while (1);
    spdlog::info("Client IP:[{}] connected,id: {},username: {}", inet_ntoa(addrClient[id].sin_addr), id, (char *)recvBuf);
    username[id] = (char *)recvBuf;
    hThread[id] = CreateThread(NULL, 0, client_server, (LPVOID)(username[id].c_str()), 0, NULL);
    CloseHandle(hThread[id]);
    {
        std::lock_guard<std::mutex> lock(mtx);
        id_free.erase(id), id_have.insert(id);
    }
    return 0;
}

bool init() {
    spdlog::info("Initializing socket server");

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("WSAStartup failed with error: {}", WSAGetLastError());
        return false;
    }

    sockSrv = socket(AF_INET, SOCK_STREAM, 0);
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(port);
    addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    int retVal = bind(sockSrv, (LPSOCKADDR)&addrSrv, sizeof(SOCKADDR_IN));
    if (retVal == SOCKET_ERROR) {
        spdlog::error("Bind failed with error: {}", WSAGetLastError());
        return false;
    }

    if (listen(sockSrv, 10) == SOCKET_ERROR) {
        spdlog::error("Listen failed with error: {}", WSAGetLastError());
        return false;
    }

    char hostname[256];
    gethostname(hostname, 256);
    hostent *temp;
    temp = gethostbyname(hostname);
    for (int i = 0; temp->h_addr_list[i] != NULL && i < 10; i++) {
        ip[i] = inet_ntoa(*(struct in_addr *)temp->h_addr_list[i]);
        ic++;
    }

    len = sizeof(SOCKADDR);
    for (int i = 1; i <= 1000; i++) id_free.insert(i);
    client.clear();

    spdlog::info("Socket server initialized, listening on ip: {}, port: {}", ip[ic], port);

    return true;
}

DWORD WINAPI thread_server(LPVOID lpParamter) {
    spdlog::info("Starting socket server");

    while (1) {
        cnt = *id_free.begin();
        sockcli[cnt] = accept(sockSrv, (SOCKADDR *)&addrClient[cnt], &len);
        if (sockcli[cnt] == SOCKET_ERROR) {
            spdlog::error("Accept failed with error: {}", WSAGetLastError());
            return false;
        }
        char chcnt[1005];
        sprintf(chcnt, "%d", cnt);
        cThread[cnt] = CreateThread(NULL, 0, connect_client, (LPVOID)chcnt, 0, NULL);
        CloseHandle(cThread[cnt]);
        {
            std::lock_guard<std::mutex> lock(mtx);
            id_free.erase(cnt), id_have.insert(cnt);
        }
    }
    return 0;
}

bool start() {
    hThread[0] = CreateThread(NULL, 0, thread_server, NULL, 0, NULL);
    CloseHandle(hThread[0]);
    return true;
}
} // namespace SocketServer

namespace SocketClient {
std::string ip, username, servername;
bool server_status = false;
int port;
char recvbuf[1024];
WSADATA wsaData;
SOCKADDR_IN addrSrv;
SOCKET sockClient;
HANDLE hThread;

bool init(const std::string &ip, int port, const std::string &username) {
    SocketClient::ip = ip;
    SocketClient::port = port;
    SocketClient::username = username;

    return true;
}

DWORD WINAPI message_server(LPVOID lpParam) {
    while (1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        int result = recv(sockClient, recvbuf, sizeof(recvbuf), 0);
        if (result == 0) {
            spdlog::info("Server[{}] disconnected", servername);
            server_status = false;
            closesocket(sockClient);
            // WSACleanup();
            break;
        } else if (result > 0)
            spdlog::debug("Received message from server[{}]: {}", servername, recvbuf);
        else {
            spdlog::error("Recv failed with error: {}", WSAGetLastError());
            server_status = false;
            closesocket(sockClient);
            break;
        }
    }
    return 0;
}

bool connect() {
    spdlog::info("Connecting to server[{}], username: {}", ip, username);
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("WSAStartup failed with error: {}", WSAGetLastError());
        return 0;
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(port);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ip.c_str());

    sockClient = socket(AF_INET, SOCK_STREAM, 0);

    if (SOCKET_ERROR == sockClient) {
        spdlog::error("Failed to create socket, error: {}", WSAGetLastError());
        return 0;
    }
    if (connect(sockClient, (struct sockaddr *)&addrSrv, sizeof(addrSrv)) == INVALID_SOCKET) {
        spdlog::error("Failed to connect to server, error: {}", WSAGetLastError());
        return 0;
    } else {
        recv(sockClient, recvbuf, sizeof(recvbuf), 0);
        servername = recvbuf;
        spdlog::debug("Received message from server[{}],IP:[{}]: {}", servername, ip, recvbuf);
        spdlog::info("Connected to server[{}], username: {}", servername, username);
    }

    server_status = true;

    do {
        send(sockClient, username.c_str(), username.size(), 0);
        recv(sockClient, recvbuf, sizeof(recvbuf), 0);
        spdlog::debug("Received message from server[{}]: {}", servername, recvbuf);
        break;
    } while (1);

    hThread = CreateThread(NULL, 0, message_server, NULL, 0, NULL);
    CloseHandle(hThread);

    return 1;
}

bool sendmsg(std::string msg) {
    if (server_status == false) {
        spdlog::error("Server is not connected, cannot send message");
        return false;
    }
    int iSend = send(sockClient, msg.c_str(), msg.size(), 0);
    if (iSend == SOCKET_ERROR) {
        spdlog::error("Send failed with error: {}", WSAGetLastError());
        return false;
    }
    spdlog::debug("Sent message to server[{}]: {}", servername, msg);
    return true;
}

} // namespace SocketClient

struct Email {
    std::string smtp_server, smtp_port, username, password;

    Email(const std::string &smtpServer, const std::string &smtpPort, const std::string &username, const std::string &password) :
        smtp_server(smtpServer), smtp_port(smtpPort), username(username), password(password) {}

    bool send(const std::string &from, const std::string &to, const std::string &subject, const std::string &body) {
        spdlog::info("Sending email");
        CURL *curl;
        CURLcode res;

        curl = curl_easy_init();
        if (curl) {
            // Set the email sender and server
            std::string smtpServer = "smtp://" + smtp_server + ":" + smtp_port;
            curl_easy_setopt(curl, CURLOPT_URL, smtpServer.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

            // set email's recipients
            struct curl_slist *recipients = curl_slist_append(recipients, to.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL); // use SSL

            // set email's subject and body
            const char *payload_text[] = {
                ("To: " + to + "\r\n").c_str(),
                ("From: " + from + "\r\n").c_str(),
                ("Subject: " + subject + "\r\n").c_str(),
                "\r\n",
                body.c_str(),
                nullptr};

            curl_easy_setopt(curl, CURLOPT_READFUNCTION, nullptr);
            curl_easy_setopt(curl, CURLOPT_READDATA, payload_text);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

            res = curl_easy_perform(curl); // send the email

            if (res != CURLE_OK) {
                spdlog::error("Failed to send email, error: {}", curl_easy_strerror(res));
                return false;
            }

            curl_slist_free_all(recipients);
            curl_easy_cleanup(curl);

            return true;
        } else {
            spdlog::error("Failed to initialize curl");
            return false;
        }
    }
};

struct GetBoard {
    const int width = 1000, height = 600;
    const int retry_count = 3, retry_delay = 1000, timeout = 10000;

    struct color {
        uint8_t r, g, b;
    };

    std::vector<std::vector<color>> board;

    GetBoard() {
        board.resize(height);
        for (int i = 0; i < height; i++) {
            board[i].resize(width);
        }
    }

    bool getboard() {
        std::string url = "https://api.paintboard.ayakacraft.com:32767/api/paintboard/getboard";
        cpr::Header hearder = {{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36"}};

        spdlog::info("Getting board from {}", url);

        std::vector<uint8_t> byteArray(width * height * 3);

        int cnt = 0;
        while (cnt < retry_count) {
            try {
                cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Header{hearder}, cpr::Timeout{timeout});
                if (r.status_code == 200) {
                    std::copy(r.text.begin(), r.text.end(), byteArray.begin());
                    break;
                } else
                    spdlog::error("Failed to get board, status code: {}", r.status_code);
            } catch (const cpr::Error &e) {
                spdlog::error("Failed to get board, error: {}", e.message);
            } catch (...) {
                spdlog::error("Failed to get board, unknown error");
            }
            if (cnt == retry_count - 1) {
                spdlog::error("Failed to get board after {} attempts, giving up", retry_count);
                return false;
            }
            cnt++;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay));
            spdlog::info("Retrying to get board, count: {}", cnt);
        }

        spdlog::info("Got board, size: {}x{}", width, height);

        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                uint8_t r = byteArray[y * width * 3 + x * 3];
                uint8_t g = byteArray[y * width * 3 + x * 3 + 1];
                uint8_t b = byteArray[y * width * 3 + x * 3 + 2];
                board[y][x] = {r, g, b};
            }

        return true;
    }

    void saveimage_rgb(std::string filename) {
        FILE *fp = fopen(filename.c_str(), "w");
        if (fp == NULL) {
            spdlog::error("Failed to open file: {}", filename);
            return;
        }
        fprintf(fp, "%d %d\n", height, width);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                color c = board[y][x];
                fprintf(fp, "%d %d %d %d %d\n", y, x, c.r, c.g, c.b);
            }
        }
        fclose(fp);
        spdlog::info("Saved image_rgb to {}", filename);
    }

    void read_byte_rgb(std::string filename) {
        FILE *fp = fopen(filename.c_str(), "rb");
        if (fp == NULL) {
            spdlog::error("Failed to open file: {}", filename);
            return;
        }
        std::vector<uint8_t> byteArray(width * height * 3);
        fread(byteArray.data(), 1, width * height * 3, fp);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint8_t r = byteArray[y * width * 3 + x * 3];
                uint8_t g = byteArray[y * width * 3 + x * 3 + 1];
                uint8_t b = byteArray[y * width * 3 + x * 3 + 2];
                board[y][x] = {r, g, b};
            }
        }
        fclose(fp);
        spdlog::info("Read image from {}", filename);
    }
};

GetBoard board;
Email email("smtp.gmail.com", "587", "your_email_address", "your_email_password");

int main() {
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("LSPaintBoard started");

    SocketClient::init("10.0.3.113", 4500, "LSPaintBoard");
    SocketClient::connect();

    while (true) {
        // if (board.getboard() == false) {
        //     spdlog::error("Failed to get board, retrying in 10 seconds");

        //     // email.send("your_email_address", "your_email_address", "LSPaintBoard Error", "Failed to get board, retrying in 10 seconds");

        //     std::this_thread::sleep_for(std::chrono::seconds(100));
        //     continue;
        // }
        // board.saveimage_rgb("board_rgb.txt");
        std::this_thread::sleep_for(std::chrono::seconds(100));
    }
}