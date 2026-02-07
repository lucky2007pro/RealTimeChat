#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_STL_
#define CPPHTTPLIB_OPENSSL_SUPPORT 

#include "crow_all.h"
#include "httplib.h"

#include <cstdlib>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <iostream>
#include <fstream>
#include <sstream>

std::string SUPABASE_URL;
std::string SUPABASE_KEY;

std::string get_env_var(std::string const& key) {
    char const* val = std::getenv(key.c_str());
    return val == nullptr ? std::string() : std::string(val);
}

// Bazaga yozish
void save_message_to_db(const std::string& msg) {
    if (SUPABASE_URL.empty()) return;

    httplib::Client cli(SUPABASE_URL);
    cli.set_bearer_token_auth(SUPABASE_KEY);
    
    // O'ZGARISH: set_header O'RNIGA set_default_headers ishlatildi
    cli.set_default_headers({
        { "apikey", SUPABASE_KEY },
        { "Content-Type", "application/json" }
    });
    
    cli.enable_server_certificate_verification(false);
    crow::json::wvalue json_body;
    json_body["content"] = msg;
    cli.Post("/rest/v1/messages", json_body.dump(), "application/json");
}

// Bazadan o'qish
std::vector<std::string> load_history_from_db() {
    std::vector<std::string> history;
    if (SUPABASE_URL.empty()) return history;

    httplib::Client cli(SUPABASE_URL);
    cli.set_bearer_token_auth(SUPABASE_KEY);
    
    // O'ZGARISH: Bu yerda ham o'zgardi
    cli.set_default_headers({
        { "apikey", SUPABASE_KEY }
    });

    cli.enable_server_certificate_verification(false);
    auto res = cli.Get("/rest/v1/messages?select=content&order=created_at.asc&limit=50");

    if (res && res->status == 200) {
        try {
            auto data = crow::json::load(res->body);
            if (data) {
                for (const auto& item : data) {
                    history.push_back(item["content"].s());
                }
            }
        } catch (...) {}
    }
    return history;
}

int main()
{
    SUPABASE_URL = get_env_var("SUPABASE_URL");
    SUPABASE_KEY = get_env_var("SUPABASE_KEY");

    crow::SimpleApp app;
    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> users;
    std::vector<std::string> chat_history = load_history_from_db();
    
    std::cout << "Bazadan " << chat_history.size() << " ta xabar yuklandi.\n";

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection& conn) {
            std::lock_guard<std::mutex> _(mtx);
            users.insert(&conn);
            for (const auto& msg : chat_history) conn.send_text(msg);
        })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
            std::lock_guard<std::mutex> _(mtx);
            users.erase(&conn);
        })
        .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary) {
            std::lock_guard<std::mutex> _(mtx);
            chat_history.push_back(data);
            if (chat_history.size() > 50) chat_history.erase(chat_history.begin());
            
            save_message_to_db(data); // Bazaga saqlash

            for (auto u : users) u->send_text(data);
        });

    CROW_ROUTE(app, "/")
    ([] {
        std::ifstream file("index.html");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return crow::response(200, buffer.str());
        }
        return crow::response(404, "Index fayl topilmadi");
    });

    app.port(8080).multithreaded().run();
}