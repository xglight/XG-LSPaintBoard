#include <cpr/cprtypes.h>
#include <cpr/timeout.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
std::string ip[1024];
int ic = -1, cnt = 1, len;
std::set<int> id_free, id_have;
std::map<std::string, int> client;
SOCKET sockSrv, sockcli[1024];
HANDLE hThread[1024], cThread[1024];

DWORD WINAPI client_server(LPVOID lpParamter) {
    char recvBuf[1024], username[1024];
    snprintf(username, sizeof(username), "%s", (char *)lpParamter);
    int id = client[(char *)lpParamter];
    spdlog::info("Client {} server started,id: {}", username, id);

    while (1) {
        memset(recvBuf, 0, sizeof(recvBuf));

        int result = recv(sockcli[id], recvBuf, sizeof(recvBuf), 0);
        if (result == 0) {
            spdlog::info("Client {} disconnected", username);
            closesocket(sockcli[id]);
            client[(char *)lpParamter] = 0;
            id_free.insert(id), id_have.erase(id);
            // WSACleanup();
            break;
        } else if (result > 0) {
            spdlog::info("Received message from {}: {}", username, recvBuf);
        } else {
            spdlog::error("Recv failed with error: {}", WSAGetLastError());
            closesocket(sockcli[id]);
            client[(char *)lpParamter] = 0;
            id_free.insert(id), id_have.erase(id);
            break;
        }
    }
    return 0;
}

DWORD WINAPI connect_client(LPVOID lpParamter) {
    char recvBuf[1024], sendBuf[1024];

    int id = 0;
    for (int i = strlen((char *)lpParamter) - 1; i >= 0; i--)
        id = id * 10 + ((char *)lpParamter)[i] - '0';

    sprintf(sendBuf, "try to connect IP:[%s]", ip[ic].c_str());
    int iSend = send(sockcli[id], sendBuf, sizeof(sendBuf), 0);
    if (iSend == SOCKET_ERROR) {
        spdlog::error("One client connect failed with error: {}", WSAGetLastError());
        return false;
    }
    bool f = 0;
    do {
        memset(recvBuf, 0, sizeof(recvBuf));
        int result = recv(sockcli[id], recvBuf, sizeof(recvBuf), 0);
        if (result == 0) {
            spdlog::info("Client IP:[{}] disconnected", inet_ntoa(addrClient[id].sin_addr));
            closesocket(sockcli[id]);
            client[(char *)recvBuf] = 0;
            id_free.insert(id), id_have.erase(id);
            return false;
        } else if (result < 0) {
            spdlog::error("client IP:[{}] recv failed with error: {}", inet_ntoa(addrClient[id].sin_addr), WSAGetLastError());
            closesocket(sockcli[id]);
            client[(char *)recvBuf] = 0;
            id_free.insert(id), id_have.erase(id);
            return false;
        } else {
            client[(char *)recvBuf] = id;
            sprintf(sendBuf, "Welcome to Paintboar IP:[%s],%s", inet_ntoa(addrClient[id].sin_addr), (char *)recvBuf);
            spdlog::info("Client IP:[{}] send message: {}", inet_ntoa(addrClient[id].sin_addr), sendBuf);
            send(sockcli[id], sendBuf, sizeof(sendBuf), 0);
            break;
        }
    } while (1);
    spdlog::info("Client IP:[{}] connected,id:{}", inet_ntoa(addrClient[id].sin_addr), id);
    try {
        hThread[id] = CreateThread(NULL, 0, client_server, (LPVOID)recvBuf, 0, NULL);
    } catch (...) {
        spdlog::error("CreateThread failed with error: {}", GetLastError());
        return false;
    }
    CloseHandle(hThread[id]);
    id_free.erase(id), id_have.insert(id);
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
        id_free.erase(cnt), id_have.insert(cnt);
    }
    return 0;
}

bool start() {
    hThread[0] = CreateThread(NULL, 0, thread_server, NULL, 0, NULL);
    CloseHandle(hThread[0]);
    return true;
}
} // namespace SocketServer

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
    const int retry_count = 3, retry_delay = 1000, timeout = 5000;

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

    /*
    void speedtest() {
        std::string url = "https://www.baidu.com/";
        cpr::Header hearder = {{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36"}};

        spdlog::info("Testing connection to {}", url);

        cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Header{hearder}, cpr::Timeout{timeout});

        spdlog::info("Connection test result: {}", r.status_code);
    }
    */

    bool getboard() {
        std::string url = "https://paintboard.ayakacraft.com/";
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

        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                uint8_t r = byteArray[y * width * 3 + x * 3];
                uint8_t g = byteArray[y * width * 3 + x * 3 + 1];
                uint8_t b = byteArray[y * width * 3 + x * 3 + 2];
                board[y][x] = {r, g, b};
            }

        return true;
    }

    bool getimage(std::string filename) {
        std::string url = "https://paintboard.ayakacraft.com/";
        cpr::Header hearder = {{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36"}};

        spdlog::info("Getting image from {}", url);

        int cnt = 0;
        while (cnt < retry_count) {
            try {
                cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Header{hearder});
                if (r.status_code == 200) {
                    freopen(filename.c_str(), "wb", stdout);
                    fwrite(r.text.data(), 1, r.text.size(), stdout);
                    fclose(stdout);
                    break;
                } else {
                    spdlog::error("Failed to get image, status code: {}", r.status_code);
                }
            } catch (const cpr::Error &e) {
                spdlog::error("Failed to get image, error: {}", e.message);
            } catch (...) {
                spdlog::error("Failed to get image, unknown error");
            }
            if (cnt == retry_count - 1) {
                spdlog::error("Failed to get image after {} attempts, giving up", retry_count);
                return false;
            }
            cnt++;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay));
            spdlog::info("Retrying to get image, count: {}", cnt);
        }

        return true;
    }
};

GetBoard board;
Email email("smtp.gmail.com", "587", "your_email_address", "your_email_password");

int main() {
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("LSPaintBoard started");

    SocketServer::port = 8080;
    if (SocketServer::init() == false) {
        spdlog::error("Failed to initialize socket server, exiting");
        return 1;
    }

    SocketServer::start();

    while (true) {
        if (board.getboard() == false) {
            spdlog::error("Failed to get board, retrying in 10 seconds");

            // email.send("your_email_address", "your_email_address", "LSPaintBoard Error", "Failed to get board, retrying in 10 seconds");

            std::this_thread::sleep_for(std::chrono::seconds(100));
            continue;
        }
    }
}