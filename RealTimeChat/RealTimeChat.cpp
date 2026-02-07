#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_STL_
// HTTPS (Supabase) ishlashi uchun shart
#define CPPHTTPLIB_OPENSSL_SUPPORT 

#include "crow_all.h"
#include "httplib.h"

#include <cstdlib>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <thread> // <--- YANGI: Multithreading uchun
#include <iostream>
#include <fstream>
#include <sstream>

// Global o'zgaruvchilar
std::string SUPABASE_URL;
std::string SUPABASE_KEY;

// Environment Variable olish
std::string get_env_var(std::string const& key) {
    char const* val = std::getenv(key.c_str());
    return val == nullptr ? std::string() : std::string(val);
}

// ---------------------------------------------------------
// 1. BAZAGA YOZISH (Bu funksiya endi orqa fonda ishlaydi)
// ---------------------------------------------------------
void save_message_to_db(std::string username, std::string content) {
    if (SUPABASE_URL.empty()) return;

    // Har bir thread o'zining shaxsiy Client obyektini ochadi (Thread-safe)
    httplib::Client cli(SUPABASE_URL);
    cli.set_bearer_token_auth(SUPABASE_KEY);
    cli.set_default_headers({
        {"apikey", SUPABASE_KEY},
        {"Content-Type", "application/json"}
        });
    cli.enable_server_certificate_verification(false);

    crow::json::wvalue json_body;
    json_body["username"] = username;
    json_body["content"] = content;

    // So'rov yuborish (bu 0.5-1 sekund vaqt olishi mumkin, lekin endi chatni qotirmaydi)
    auto res = cli.Post("/rest/v1/messages", json_body.dump(), "application/json");

    if (res && res->status != 201) {
        std::cout << "DB Error: " << res->status << "\n";
    }
}

// ---------------------------------------------------------
// 2. TARIXNI YUKLASH (Faqat startda ishlaydi)
// ---------------------------------------------------------
std::vector<std::string> load_history_from_db() {
    std::vector<std::string> history;
    if (SUPABASE_URL.empty()) return history;

    httplib::Client cli(SUPABASE_URL);
    cli.set_bearer_token_auth(SUPABASE_KEY);
    cli.set_default_headers({ {"apikey", SUPABASE_KEY} });
    cli.enable_server_certificate_verification(false);

    // Oxirgi 50 ta xabarni vaqt bo'yicha olish
    auto res = cli.Get("/rest/v1/messages?select=username,content&order=created_at.asc&limit=50");

    if (res && res->status == 200) {
        try {
            auto data = crow::json::load(res->body);
            if (data) {
                for (const auto& item : data) {
                    crow::json::wvalue msg;
                    msg["username"] = item["username"].s();
                    msg["content"] = item["content"].s();
                    history.push_back(msg.dump());
                }
            }
        }
        catch (...) {
            std::cout << "JSON parse error during history load\n";
        }
    }
    return history;
}

int main() {
    // 1. Konfiguratsiyani yuklash
    SUPABASE_URL = get_env_var("SUPABASE_URL");
    SUPABASE_KEY = get_env_var("SUPABASE_KEY");

    if (SUPABASE_URL.empty() || SUPABASE_KEY.empty()) {
        std::cout << "DIQQAT: Supabase kalitlari topilmadi!\n";
    }

    crow::SimpleApp app;
    std::mutex mtx; // Resurslarni himoya qilish uchun
    std::unordered_set<crow::websocket::connection*> users;

    // Server yonishi bilan tarixni yuklaymiz
    std::vector<std::string> chat_history = load_history_from_db();
    std::cout << "Tarix yuklandi: " << chat_history.size() << " ta xabar.\n";

    // WebSocket Marshruti
    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection& conn) {
        std::lock_guard<std::mutex> _(mtx);
        users.insert(&conn);

        // Yangi kirgan odamga tarixni beramiz
        for (const auto& msg : chat_history) {
            conn.send_text(msg);
        }
            })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
        std::lock_guard<std::mutex> _(mtx);
        users.erase(&conn);
            })
        .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary) {
        // JSONni tekshiramiz
        std::string user_name, user_content;
        try {
            auto received_json = crow::json::load(data);
            if (!received_json) return;
            user_name = received_json["username"].s();
            user_content = received_json["content"].s();
        }
        catch (...) {
            return; // Xato ma'lumot kelsa, indamaymiz
        }

        // -----------------------------------------------------
        // 1. RAM bilan ishlash (Juda tez - LOCK kerak)
        // -----------------------------------------------------
        {
            std::lock_guard<std::mutex> _(mtx);

            // Tarixga qo'shish
            chat_history.push_back(data);
            if (chat_history.size() > 50) chat_history.erase(chat_history.begin());

            // Hammaga darhol tarqatish!
            for (auto u : users) {
                u->send_text(data);
            }
        }
        // Lock shu yerda ochiladi. Server endi bo'sh.

        // -----------------------------------------------------
        // 2. BAZAGA YOZISH (MULTITHREADING - ASINXRON)
        // -----------------------------------------------------
        // Biz yangi "ishchi" (thread) yollaymiz va unga "borib bazaga yozib kel" deymiz.
        // Asosiy server esa keyingi xabarni qabul qilishga tayyor bo'ladi.

        std::thread([user_name, user_content]() {
            save_message_to_db(user_name, user_content);
            }).detach(); // .detach() - "Ortinga qarama, ishlayver" degani.
            });

    // HTML sahifa
    CROW_ROUTE(app, "/")([] {
        std::ifstream file("index.html");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return crow::response(200, buffer.str());
        }
        return crow::response(404, "Index fayl topilmadi");
        });

    // Serverni ishga tushirish (Multithreaded rejimda)
    std::cout << "Server 8080 portda ishga tushdi...\n";
    app.port(8080).multithreaded().run();
}