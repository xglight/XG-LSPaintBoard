#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ios>
#include <iostream>
#include <set>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <regex>
#include <curl/curl.h>
#include "spdlog/spdlog.h"
#include "winsock2.h"

using json = nlohmann::json;

// paint
std::mutex board_mutex;
std::string api_url;
struct color {
    uint8_t r, g, b;
};
std::map<int, int> group_id;
std::map<int, double> paint_status;
int success_total = 0, fail_total = 0;
int thread_num = 0;
double time_limit;

// socket
int socket_port;
std::string socket_server;

// email
bool enable_mail = false;
std::string smtp_server, smtp_port, username, password, to_address;

// filename
std::string img_file, token_file, value_file;

// token
std::vector<std::pair<int, std::string>> tokens;
std::map<int, bool> token_used;
std::vector<int> free_tokens[10005];
std::map<int, int> uid_token_map;
int token_total = 0, token_group, group = 0;

// img
int img_w, img_h;
std::vector<std::vector<color>> img;
int start_x, start_y, end_x, end_y;

// value
std::vector<std::vector<int>> values;

double nowtime() {
    return std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now().time_since_epoch()).count();
} // get time now

struct GetBoard {
    const int width = 1000, height = 600;
    const int retry_count = 3, retry_delay = 1000, timeout = 10000;

    std::vector<std::vector<color>> board;

    GetBoard() {
        board.resize(height);
        for (int i = 0; i < height; i++) {
            board[i].resize(width);
        }
    }

    void read_byte_rgb(std::string filename) {
        FILE *fp = fopen(filename.c_str(), "rb");

        if (fp == NULL) {
            spdlog::error("Failed to open file: {}", filename);
            return;
        }

        std::vector<uint8_t> byteArray(width * height * 3);

        fread(byteArray.data(), 1, width * height * 3, fp);

        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                uint8_t r = byteArray[y * width * 3 + x * 3];
                uint8_t g = byteArray[y * width * 3 + x * 3 + 1];
                uint8_t b = byteArray[y * width * 3 + x * 3 + 2];

                board[y][x] = {r, g, b};
            }

        fclose(fp);

        spdlog::info("Read image from {}", filename);
    } // read the png from rgbfile

    bool getboard() {
        std::string command = "GetPaint.exe -u " + api_url;
        system(command.c_str());
        read_byte_rgb("board");
        return true;
    } // get LSPaintBoard now

    void saveimage_rgb(std::string filename) {
        FILE *fp = fopen(filename.c_str(), "w");

        if (fp == NULL) {
            spdlog::error("Failed to open file: {}", filename);
            return;
        }

        fprintf(fp, "%d %d\n", height, width);

        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                color c = board[y][x];

                fprintf(fp, "%d %d %d %d %d\n", y, x, c.r, c.g, c.b);
            }

        fclose(fp);

        spdlog::info("Saved image_rgb to {}", filename);
    } // save the png to rgb

} board;

struct SocketClient {
    std::string ip, username, servername;
    bool server_status = false;
    int port, id;
    char recvbuf[102400];
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

    std::vector<std::string> split(const std::string &s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
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
                spdlog::debug("id:[{}] - Received message from server[{}],len:{}", id, servername, result);
                std::vector<uint8_t> bytemsg;
                bytemsg.clear();
                for (int i = 0; i < result; i++)
                    bytemsg.push_back(recvbuf[i]);
                int last = 0;
                while (last < bytemsg.size()) {
                    uint8_t type = bytemsg[last];
                    last++;
                    if (type == 0xfa) {
                        uint16_t x = (bytemsg[last + 1] << 8) | bytemsg[last];
                        uint16_t y = (bytemsg[last + 3] << 8) | bytemsg[last + 2];
                        uint8_t r = bytemsg[last + 4];
                        uint8_t g = bytemsg[last + 5];
                        uint8_t b = bytemsg[last + 6];
                        last += 7;
                        spdlog::debug("Server:paint ({},{},{}) at ({},{})", r, g, b, x, y);
                        std::lock_guard<std::mutex> lock(board_mutex);
                        board.board[y][x] = {r, g, b};
                    } else if (type == 0xff) {
                        int id = (bytemsg[last + 3] << 24) | (bytemsg[last + 2] << 16) | (bytemsg[last + 1] << 8) | bytemsg[last];
                        int uid = (bytemsg[last + 6] << 16) | (bytemsg[last + 5] << 8) | bytemsg[last + 4];
                        int code = bytemsg[last + 7];
                        last += 8;

                        std::string hexstr = intToHex(code);
                        spdlog::debug("Server:tast {} is {},uid:{}", id, hexstr, uid);

                        int token_id = uid_token_map[uid];

                        if (code == 0xef) { // success
                            success_total++;
                            paint_status[id] = nowtime();
                        } else if (code == 0xee) { // cooling
                            fail_total++;
                        } else if (code == 0xed) { // failed
                            fail_total++;
                            spdlog::warn("Token[{}] is failed,remove from the list", uid);
                            token_used[token_id] = true;
                            uid_token_map.erase(uid);
                            token_total--;
                        } else { // other
                            spdlog::warn("uid:{} get other type:{}", uid, hexstr);
                            fail_total++;
                        }
                    } else
                        spdlog::error("Unkown type:{}", id, type);
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

struct Paint {
    std::mutex paint_mutex;

    std::vector<std::vector<uint8_t>> chunks[7];

    void init() {
        for (int i = 0; i < 7; i++) chunks[i].clear();

        std::thread t([this]() { this->send_data(); });
        t.detach();
    }

    std::string get_merge_data(std::vector<std::vector<uint8_t>> chunk) {
        std::vector<uint8_t> result;
        result.clear();

        for (int i = 0; i < chunk.size(); i++)
            result.insert(result.end(), chunk[i].begin(), chunk[i].end());

        std::lock_guard<std::mutex> lock(paint_mutex);
        chunk.clear();

        return std::string(result.begin(), result.end());
    } // merge datas

    std::vector<uint8_t> uintToUint8Array(uint32_t uint, size_t bytes) {
        std::vector<uint8_t> array(bytes);
        for (size_t i = 0; i < bytes; ++i) {
            array[i] = uint & 0xff;
            uint = uint >> 8;
        }
        return array;
    }

    void paint(int f, int uid, std::string token, int x, int y, int r, int g, int b, int id) {
        spdlog::debug("Painting ({},{},{}) at ({},{}),uid: {},token: {}", r, g, b, x, y, uid, token);
        spdlog::info("Painting ({},{},{}) at ({},{})", r, g, b, x, y);

        std::string token_cleaned = token;
        token_cleaned.erase(std::remove(token_cleaned.begin(), token_cleaned.end(), '-'), token_cleaned.end());
        std::regex pattern(".{2}");
        auto words_begin = std::sregex_iterator(token_cleaned.begin(), token_cleaned.end(), pattern);
        auto words_end = std::sregex_iterator();
        std::vector<uint8_t> tokenBytes;
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string match_str = match.str();
            tokenBytes.push_back(std::stoi(match_str, nullptr, 16));
        }

        std::vector<uint8_t> tmpx = uintToUint8Array(x, 2);
        std::vector<uint8_t> tmpy = uintToUint8Array(y, 2);
        std::vector<uint8_t> tmpuid = uintToUint8Array(uid, 3);

        std::vector<uint8_t> paint_data;
        paint_data.clear();
        paint_data.push_back(0xfe);
        paint_data.insert(paint_data.end(), tmpx.begin(), tmpx.end());
        paint_data.insert(paint_data.end(), tmpy.begin(), tmpy.end());
        paint_data.push_back(r);
        paint_data.push_back(g);
        paint_data.push_back(b);
        paint_data.insert(paint_data.end(), tmpuid.begin(), tmpuid.end());
        paint_data.insert(paint_data.end(), tokenBytes.begin(), tokenBytes.end());
        chunks[f].push_back(paint_data);
    } // paint (r,g,b) at (x,y)

    void send_data() {
        while (1) {
            for (int i = 0; i < 7; i++)
                if (!chunks[i].empty() && client[i].server_status == true)
                    client[i].sendmsg(get_merge_data(chunks[i]).c_str());

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    } // send paint's datas to LSPaint

} paint;

struct Init {
    bool get_token() {
        std::ifstream f(token_file);

        if (f.fail()) {
            spdlog::error("Failed to open file: {}", token_file);
            return false;
        }

        tokens.clear();

        int cnt = 0;
        while (!f.eof()) {
            int uid;
            std::string token;

            f >> uid >> token;

            tokens.push_back(std::make_pair(uid, token));
            free_tokens[group].push_back(tokens.size() - 1);
            uid_token_map[uid] = tokens.size() - 1;

            cnt++;

            if (cnt == token_group) group++, cnt = 0;
        }

        token_total = tokens.size();

        f.close();

        spdlog::info("Read tokens from {},total: {},group: {}", token_file, tokens.size(), group + 1);

        return true;
    } // get token file

    bool get_image() {
        std::map<std::pair<int, int>, bool> vis;
        vis.clear();

        FILE *fp = fopen(img_file.c_str(), "r");

        if (fp == NULL) {
            spdlog::error("Failed to open file: {}", img_file);
            return false;
        }

        int height, width;

        fscanf(fp, "%d %d", &height, &width);

        img_w = width, img_h = height;

        img.resize(height);
        for (int i = 0; i < height; i++) {
            img[i].resize(width);
        }

        int cnt = 0;
        int y = 0, x = 0;
        uint8_t r = 0, g = 0, b = 0;

        while (fscanf(fp, "%d %d %hhu %hhu %hhu", &y, &x, &r, &g, &b) == 5) {
            if (y >= height || x >= width || r > 255 || g > 255 || b > 255 || y < 0 || x < 0 || r < 0 || g < 0 || b < 0) {
                spdlog::error("Invalid image data, y: {}, x: {}, r: {}, g: {}, b: {}", y, x, r, g, b);
                continue;
            }

            if (!vis[std::make_pair(y, x)]) vis[std::make_pair(y, x)] = true, cnt++;

            img[y][x] = {r, g, b};
        }

        fclose(fp);

        if (cnt != img_w * img_h) spdlog::warn("Invalid image data, total pixel: {}, expected: {}", cnt, img_w * img_h);

        spdlog::info("Read image from {},image size: {}x{}", img_file, img_w, img_h);
        return true;
    } // get image file

    bool get_value() {
        std::map<std::pair<int, int>, int> vis;
        vis.clear();
        FILE *fp = fopen(value_file.c_str(), "r");
        if (fp == NULL) {
            spdlog::error("Failed to open file: {}", value_file);
            return false;
        }
        int height, width;
        fscanf(fp, "%d %d", &height, &width);
        values.resize(height);
        for (int i = 0; i < height; i++) {
            values[i].resize(width);
        }
        int cnt = 0;
        int y, x;
        int value;
        while (fscanf(fp, "%d %d %d", &y, &x, &value) == 3) {
            if (y >= height || x >= width || value < 0 || y < 0 || x < 0 || value > 10 || value < 0) {
                spdlog::error("Invalid value data, y: {}, x: {}, value: {}", y, x, value);
                continue;
            }
            if (!vis[std::make_pair(y, x)]) vis[std::make_pair(y, x)] = true, cnt++;
            values[y][x] = value;
        }
        fclose(fp);
        if (cnt != img_w * img_h) spdlog::warn("Invalid value data, total pixel: {}, expected: {}", cnt, img_w * img_h);
        spdlog::info("Read values from {}", value_file);
        return true;
    } // get value file

    bool init_client() {
        if (thread_num > 7 || thread_num < 1) {
            spdlog::error("Invalid thread_num, should be between 1 and 7");
            return false;
        }
        for (int i = 0; i < thread_num; i++) {
            system("");
            client[i].init(socket_server, socket_port, "LSPaintBoard", i);
            client[i].connect_server();
        }
        return true;
    } // init socket client

    bool init() {
        // start paintboard
        system("start LSPaint.exe");

        // read config.json
        spdlog::info("Read config.json");
        std::ifstream f("config.json");
        if (f.fail()) {
            spdlog::error("Failed to open file: config.json");
            return false;
        }
        json data = json::parse(f);
        f.close();

        // email
        enable_mail = data["email"]["enable"].get<bool>();
        if (enable_mail == true) {
            spdlog::info("Email enabled");
            smtp_server = data["email"]["server"].get<std::string>();
            smtp_port = data["email"]["port"].get<int>();
            username = data["email"]["username"].get<std::string>();
            password = data["email"]["password"].get<std::string>();
            to_address = data["email"]["to"].get<std::string>();
        }

        // socket
        if (data["socket"]["port"] != nullptr && data["socket"]["port"].is_number())
            socket_port = data["socket"]["port"].get<int>();
        socket_server = data["socket"]["server"].get<std::string>();

        // paint
        api_url = data["paint"]["api_url"].get<std::string>();
        img_file = data["paint"]["img_file"].get<std::string>();
        token_file = data["paint"]["token_file"].get<std::string>();
        value_file = data["paint"]["value_file"].get<std::string>();
        start_x = data["paint"]["start_x"].get<int>();
        start_y = data["paint"]["start_y"].get<int>();
        token_group = data["paint"]["token_group"].get<int>();
        time_limit = data["paint"]["time_limit"].get<double>();
        thread_num = data["paint"]["thread_num"].get<int>();

        // init others
        if (init_client() == false) return false;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        paint.init();
        if (get_token() == false) return false;
        if (get_image() == false) return false;
        if (get_value() == false) return false;
        if (board.getboard() == false) return false;

        spdlog::info("Ininitialized");
        return true;
    }
} init;

Email email(smtp_server, smtp_port, username, password);
// Ininitialize email

struct Work {
    struct work_node {
        int x, y;
        double v;
        work_node(int x, int y, double v) :
            x(x), y(y), v(v) {}
        bool operator<(const work_node &other) const {
            return v > other.v;
        }
    };

    int cnt = 0;
    std::multiset<work_node> st; // points' values

    double calc(color c1, color c2) {
        double r = c1.r - c2.r, g = c1.g - c2.g, b = c1.b - c2.b;
        return sqrt(r * r + g * g + b * b);
    } // calculate color difference

    void work() {
        int send_total = 0;
        spdlog::info("Start working");
        st.clear();

        board_mutex.lock();
        for (int i = start_y, ii = 0; ii < img_h; i++, ii++) {
            for (int j = start_x, jj = 0; jj < img_w; j++, jj++) {
                double diff = calc(img[ii][jj], board.board[i][j]);
                double v = diff * values[ii][jj]; // calculate value
                if (v < 0.0001) continue;
                st.insert(work_node(j, i, v));
            }
        }
        board_mutex.unlock();

        auto s = st.begin();
        while (s != st.end()) {
            for (int i = 0; i <= group; i++) {
                if (nowtime() - paint_status[group_id[i]] < time_limit) continue; // cooling

                if (s == st.end()) break;

                if (free_tokens[i].empty()) continue;

                paint_status.erase(group_id[i]);

                int id = (++cnt) % 4294967296; // get id, should be consistent with the id of LSPaint
                group_id[i] = id;              // update id
                paint_status[id] = nowtime();  // update paint time

                for (auto it : free_tokens[i]) {
                    if (token_used[it] == true) continue; // this token is error

                    if (s == st.end()) break;

                    int token_id = it;
                    int x = s->x, y = s->y;
                    int uid = tokens[token_id].first;

                    std::string token = tokens[token_id].second;

                    int _x = x - start_x, _y = y - start_y;

                    paint.paint(0, uid, token, x, y, img[_y][_x].r, img[_y][_x].g, img[_y][_x].b, id);

                    send_total++, s++;

                    if (send_total % 128 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(800)); // link limit
                }

                if (s == st.end()) break;

                s++;
            }
            break;
        }

        spdlog::info("End working, send total: {},success: {},failed: {}", send_total, success_total, fail_total);
        send_total = 0, success_total = 0, fail_total = 0;
    }
} work;

int main() {
    spdlog::set_level(spdlog::level::info);

    spdlog::info("LSPaintBoard started");

    if (init.init() == false) {
        spdlog::error("Failed to initialize");
        return 1;
    }

    while (true) {
        while (true) {
            bool f = false;
            for (int i = 0; i <= group; i++)
                if (nowtime() - paint_status[group_id[i]] > time_limit) {
                    f = true;
                    break;
                }
            if (f) break;
        } // determine token availability

        // if (board.getboard() == false) {
        //     spdlog::error("Failed to get board, retrying in 10 seconds");
        //     // email.send(to_address, to_address, "LSPaintBoard Error", "Failed to get board, retrying in 10 seconds");
        //     std::this_thread::sleep_for(std::chrono::seconds(10));
        //     continue;
        // }

        work.work();
    }
}