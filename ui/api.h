// HTTP client for the local astro-core daemon (localhost only).
#pragma once
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

namespace httplib { class Client; }

namespace ui {

class Api {
public:
    explicit Api(int port);
    ~Api();
    nlohmann::json get(const std::string& path);
    nlohmann::json post(const std::string& path, const nlohmann::json& body = nlohmann::json::object());
    std::string text(const std::string& path);
    bool alive();
private:
    std::unique_ptr<httplib::Client> cli_;
};

}  // namespace ui
