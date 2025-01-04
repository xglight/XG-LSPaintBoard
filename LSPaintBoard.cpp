#include <chrono>
#include <thread>
#include <regex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ios>
#include <iostream>
#include <set>
#include <string>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <curl/curl.h>
#include "spdlog/spdlog.h"
#include "winsock2.h"

using json = nlohmann::json;

struct color {
    uint8_t r, g, b;
};

int socket_port = 4500;
std::string socket_server;
std::string smtp_server, smtp_port, username, password, to_address;
std::string img_file, token_file;
std::vector<std::pair<int, std::string>> tokens;
std::vector<std::vector<color>> img;
int token_total = 0;
std::map<int, int> uid_token_map;
std::set<int> free_tokens;

struct SocketClient {
    std::string ip, username, servername;
    bool server_status = false;
    int port, id;
    char recvbuf[1024];
    WSADATA wsaData;
    SOCKADDR_IN addrSrv;
    SOCKET sockClient;
    HANDLE hThread;

    bool init(const std::string &_ip, int _port, const std::string &_username, int _id) {
        ip = _ip, port = _port, username = _username, id = _id;
        return true;
    }

    std::string intToHex(int number) {
        std::stringstream ss;
        ss << std::hex << number;
        return "0x" + ss.str();
    }

    void token_clock(int token_id) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        free_tokens.insert(token_id);
        return;
    }

    void message_server() {
        while (1) {
            memset(recvbuf, 0, sizeof(recvbuf));
            int result = recv(sockClient, recvbuf, sizeof(recvbuf), 0);
            if (result == 0) {
                spdlog::info("id:[{}] - Server[{}] disconnected", id, servername);
                server_status = false;
                closesocket(sockClient);
                // WSACleanup();
                break;
            } else if (result > 0) {
                spdlog::debug("id:[{}] - Received message from server[{}]: {}", id, servername, recvbuf);
                std::string str(recvbuf);
                int uid = std::stoi(str.substr(0, str.find(" "))), token_id = uid_token_map[uid];
                int status = std::stoi(str.substr(str.find(" ") + 1));
                if (status == 0xed) {
                    spdlog::warn("Token[{}] is unavailable, removing from list", uid);
                    free_tokens.erase(token_id);
                    uid_token_map.erase(uid);
                    token_total--;
                } else if (status == 0xee) {
                    continue;
                } else if (status == 0xef) {
                    free_tokens.erase(token_id);
                    std::thread t([this, token_id]() { this->token_clock(token_id); });
                    t.detach();
                } else {
                    std::string hexStr = intToHex(status);
                    spdlog::warn("status code from uid[{}]: {}", uid, hexStr);
                }
            } else {
                spdlog::error("id:[{}] - Recv failed with error: {}", id, WSAGetLastError());
                server_status = false;
                closesocket(sockClient);
                break;
            }
        }
    }

    bool connect_server() {
        spdlog::info("id:[{}] - Connecting to server[{}], username: {}", id, ip, username);
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            spdlog::error("WSAStartup failed with error: {}", WSAGetLastError());
            return 0;
        }

        addrSrv.sin_family = AF_INET;
        addrSrv.sin_port = htons(port);
        addrSrv.sin_addr.S_un.S_addr = inet_addr(ip.c_str());

        sockClient = socket(AF_INET, SOCK_STREAM, 0);

        if (SOCKET_ERROR == sockClient) {
            spdlog::error("id:[{}] - Failed to create socket, error: {}", id, WSAGetLastError());
            return 0;
        }
        if (connect(sockClient, (struct sockaddr *)&addrSrv, sizeof(addrSrv)) == INVALID_SOCKET) {
            spdlog::error("id:[{}] - Failed to connect to server, error: {}", id, WSAGetLastError());
            return 0;
        } else {
            recv(sockClient, recvbuf, sizeof(recvbuf), 0);
            servername = recvbuf;
            spdlog::debug("id:[{}] - Received message from server[{}],IP:[{}]: {}", id, servername, ip, recvbuf);
            spdlog::info("id:[{}] - Connected to server[{}], username: {}", id, servername, username);
        }

        server_status = true;

        do {
            send(sockClient, username.c_str(), username.size(), 0);
            recv(sockClient, recvbuf, sizeof(recvbuf), 0);
            spdlog::debug("id:[{}] - Received message from server[{}]: {}", id, servername, recvbuf);
            break;
        } while (1);

        std::thread t([this]() { this->message_server(); });
        t.detach();

        return 1;
    }

    bool sendmsg(std::string msg) {
        if (server_status == false) {
            spdlog::error("id:[{}] - Server is not connected, cannot send message", id);
            return false;
        }
        int iSend = send(sockClient, msg.c_str(), msg.size(), 0);
        if (iSend == SOCKET_ERROR) {
            spdlog::error("id:[{}] - Send failed with error: {}", id, WSAGetLastError());
            return false;
        }
        spdlog::debug("id:[{}] - Sent message to server[{}]: {}", id, servername, msg);
        return true;
    }
} client[7];

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

struct Paint {
    struct data {
        int uid;
        std::string token;
        int r, g, b, x, y;
    };

    std::vector<data> chunks[7];

    void init() {
        for (int i = 0; i < 7; i++) chunks[i].clear();
        std::thread t([this]() { this->send_data(); });
        t.detach();
    }

    std::string get_merge_data(std::vector<data> &chunk) {
        std::string result = "";

        for (int i = 0; i < chunk.size(); i++) {
            data d = chunk[i];
            result += std::to_string(d.uid) + " " + d.token + " " + std::to_string(d.r) + " " + std::to_string(d.g) + " " + std::to_string(d.b) + " " + std::to_string(d.x) + " " + std::to_string(d.y) + ",";
        }
        chunk.clear();

        return result;
    }

    void paint(int f, int uid, std::string token, int x, int y, int r, int g, int b) {
        spdlog::debug("Painting ({},{},{}) at ({},{}),uid: {},token: {}", r, g, b, x, y, uid, token);
        chunks[f].push_back({uid, token, r, g, b, x, y});
    }

    void send_data() {
        while (1) {
            for (int i = 0; i < 7; i++)
                if (!chunks[i].empty() && client[i].server_status == true)
                    client[i].sendmsg(get_merge_data(chunks[i]));
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

} paint;

void tokens_check() {
    for (int i = 0; i < tokens.size(); i++) {
        paint.paint(0, tokens[i].first, tokens[i].second, 0, 0, 0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    spdlog::info("Checked tokens, total: {}", token_total);
    return;
}

void get_token() {
    std::ifstream f(token_file);
    if (f.fail()) {
        spdlog::error("Failed to open file: {}", token_file);
        return;
    }
    tokens.clear();
    while (!f.eof()) {
        int uid;
        std::string token;
        f >> uid >> token;
        tokens.push_back(std::make_pair(uid, token));
        free_tokens.insert(tokens.size() - 1);
        uid_token_map[uid] = tokens.size() - 1;
    }
    token_total = tokens.size();
    f.close();
    spdlog::info("Read tokens from {},total: {}", token_file, tokens.size());
    tokens_check();
    return;
}

void get_image() {
    std::ifstream f(img_file);
    if (f.fail()) {
        spdlog::error("Failed to open file: {}", img_file);
        return;
    }
    int height, width;
    f >> height >> width;
    img.resize(height);
    for (int i = 0; i < height; i++) {
        img[i].resize(width);
    }
    while (!f.eof()) {
        int y, x;
        uint8_t r, g, b;
        f >> y >> x >> r >> g >> b;
        img[y][x] = {r, g, b};
    }
    f.close();
    spdlog::info("Read image from {}", img_file);
}

void init_client() {
    client[0].init(socket_server, socket_port, "LSPaintBoard", 0);
    client[0].connect_server();
}

void init() {
    system("start LSPaint.exe");

    std::ifstream f("config.json");
    if (f.fail()) {
        spdlog::error("Failed to open file: config.json");
        return;
    }
    json data = json::parse(f);
    f.close();

    spdlog::info("Read config.json");

    smtp_server = data["email"]["server"].get<std::string>();
    smtp_port = data["email"]["port"].get<int>();
    username = data["email"]["username"].get<std::string>();
    password = data["email"]["password"].get<std::string>();
    to_address = data["email"]["to"].get<std::string>();
    if (data["socket"]["port"] != nullptr && data["socket"]["port"].is_number())
        socket_port = data["socket"]["port"].get<int>();
    socket_server = data["socket"]["server"].get<std::string>();
    img_file = data["paint"]["img_file"].get<std::string>();
    token_file = data["paint"]["token_file"].get<std::string>();

    init_client();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paint.init();
    get_token();
    get_image();

    spdlog::info("Ininitialized");
}

GetBoard board;
Email email(smtp_server, smtp_port, username, password);

int main() {
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("LSPaintBoard started");

    init();

    while (true) {
        if (board.getboard() == false) {
            spdlog::error("Failed to get board, retrying in 10 seconds");
            // email.send(to_address, to_address, "LSPaintBoard Error", "Failed to get board, retrying in 10 seconds");
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }
        while (free_tokens.empty());
        for (int i = 0; i < token_total; i++)
            printf("%d %s\n", tokens[i].first, tokens[i].second.c_str());

        std::this_thread::sleep_for(std::chrono::seconds(100));
    }
}