#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_STL_
// ---------------------------------------------------------
// 1. HTTPS ishlashi uchun (Juda muhim!)
// ---------------------------------------------------------
#define CPPHTTPLIB_OPENSSL_SUPPORT 

#include "crow_all.h"
#include "httplib.h" // 2. httplib.h fayli loyiha papkasida bo'lishi shart!

#include <cstdlib> // Env o'qish uchun
#include <unordered_set>
#include <vector>
#include <mutex>
#include <iostream>
#include <fstream>
#include <sstream>

// Global o'zgaruvchilar
std::string SUPABASE_URL;
std::string SUPABASE_KEY;

// Env o'zgaruvchini olish funksiyasi
std::string get_env_var(std::string const& key) {
    char const* val = std::getenv(key.c_str());
    return val == nullptr ? std::string() : std::string(val);
}

// Bazaga xabar yozish (POST)
void save_message_to_db(const std::string& msg) {
    if (SUPABASE_URL.empty()) return;

    httplib::Client cli(SUPABASE_URL);
    cli.set_bearer_token_auth(SUPABASE_KEY);
    cli.set_header("apikey", SUPABASE_KEY);
    cli.set_header("Content-Type", "application/json");
    // HTTPS ulanish xavfsizligi (sertifikatni tekshirmaslik - oddiylik uchun)
    cli.enable_server_certificate_verification(false);

    crow::json::wvalue json_body;
    json_body["content"] = msg;

    cli.Post("/rest/v1/messages", json_body.dump(), "application/json");
}

// Bazadan tarixni yuklash (GET)
std::vector<std::string> load_history_from_db() {
    std::vector<std::string> history;
    if (SUPABASE_URL.empty()) return history;

    httplib::Client cli(SUPABASE_URL);
    cli.set_bearer_token_auth(SUPABASE_KEY);
    cli.set_header("apikey", SUPABASE_KEY);
    cli.enable_server_certificate_verification(false);

    // Oxirgi 50 ta xabarni olamiz
    auto res = cli.Get("/rest/v1/messages?select=content&order=created_at.asc&limit=50");

    if (res && res->status == 200) {
        try {
            auto data = crow::json::load(res->body);
            if (data) {
                for (const auto& item : data) {
                    history.push_back(item["content"].s());
                }
            }
        }
        catch (...) {
            std::cout << "JSON parse xatosi.\n";
        }
    }
    return history;
}

int main()
{
    // ---------------------------------------------------------
    // 3. Dastur boshida ENV dan kalitlarni o'qiymiz
    // ---------------------------------------------------------
    SUPABASE_URL = get_env_var("SUPABASE_URL");
    SUPABASE_KEY = get_env_var("SUPABASE_KEY");

    if (SUPABASE_URL.empty() || SUPABASE_KEY.empty()) {
        std::cout << "DIQQAT: SUPABASE_URL yoki KEY topilmadi! Baza ishlamaydi.\n";
    }
    else {
        std::cout << "Supabase URL: " << SUPABASE_URL << " ga ulanmoqda...\n";
    }

    crow::SimpleApp app;
    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> users;

    // RAM dagi kesh (tez ishlashi uchun)
    std::vector<std::string> chat_history;

    // Server yonishi bilan bazadan yuklab olamiz
    chat_history = load_history_from_db();
    std::cout << "Bazadan " << chat_history.size() << " ta xabar yuklandi.\n";

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
        std::lock_guard<std::mutex> _(mtx);

        // 1. RAM ga yozish
        chat_history.push_back(data);
        if (chat_history.size() > 50) chat_history.erase(chat_history.begin());

        // 2. Bazaga yuborish (Supabase)
        save_message_to_db(data);

        // 3. Hammaga tarqatish
        for (auto u : users) {
            u->send_text(data);
        }
            });

    CROW_ROUTE(app, "/")
        ([] {
        std::ifstream file("index.html");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return crow::response(200, buffer.str());
        }
        return crow::response(404, "index.html topilmadi!");
            });

    // Render 8080 portni kutadi
    app.port(8080).multithreaded().run();
}