#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_STL_

#include "crow_all.h"
#include <unordered_set>
#include <vector> // Tarixni saqlash uchun
#include <mutex>
#include <iostream>
#include <fstream>
#include <sstream>

int main()
{
    crow::SimpleApp app;

    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> users;

    // ---------------------------------------------------------
    // YANGI QISM: Chat tarixini saqlash uchun "Baza" (RAM da)
    // ---------------------------------------------------------
    std::vector<std::string> chat_history;
    const int MAX_HISTORY = 50; // Eng oxirgi 50 ta xabarni saqlaymiz

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection& conn) {
        std::lock_guard<std::mutex> _(mtx);
        users.insert(&conn);

        // -------------------------------------------------
        // YANGI QISM: Yangi kirgan odamga ESKI xabarlarni yuklab beramiz
        // -------------------------------------------------
        for (const auto& msg : chat_history) {
            conn.send_text(msg);
        }

        std::cout << "++ Yangi foydalanuvchi. Jami: " << users.size() << "\n";
            })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
        std::lock_guard<std::mutex> _(mtx);
        users.erase(&conn);
        std::cout << "-- Foydalanuvchi ketdi. Jami: " << users.size() << "\n";
            })
        .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary) {
        std::lock_guard<std::mutex> _(mtx);

        // 1. Xabarni tarixga yozib qo'yamiz
        chat_history.push_back(data);

        // Agar xabarlar ko'payib ketsa, eng eskisini o'chiramiz (RAM to'lib ketmasligi uchun)
        if (chat_history.size() > MAX_HISTORY) {
            chat_history.erase(chat_history.begin());
        }

        // 2. Hamma foydalanuvchilarga xabarni tarqatish
        for (auto u : users) {
            u->send_text(data);
        }
            });

    // Asosiy Sahifa Marshruti (/)
    CROW_ROUTE(app, "/")
        ([] {
        std::ifstream file("index.html");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return crow::response(200, buffer.str());
        }
        else {
            return crow::response(404, "Xatolik: index.html fayli topilmadi!");
        }
            });

    std::cout << "Server ishga tushdi: http://localhost:8080" << std::endl;
    app.port(8080).multithreaded().run();
}