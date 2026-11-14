#include "api.h"
#include <httplib.h>

namespace ui {
using json = nlohmann::json;

Api::Api(int port) : cli_(new httplib::Client("127.0.0.1", port)) {
    cli_->set_connection_timeout(1, 0);
    cli_->set_read_timeout(5, 0);
}
Api::~Api() = default;

json Api::get(const std::string& path) {
    auto r = cli_->Get(path);
    if (!r || r->status != 200) return json();
    json j = json::parse(r->body, nullptr, false);
    return j.is_discarded() ? json() : j;
}
json Api::post(const std::string& path, const json& body) {
    auto r = cli_->Post(path, body.dump(), "application/json");
    if (!r || r->status != 200) return json();
    json j = json::parse(r->body, nullptr, false);
    return j.is_discarded() ? json() : j;
}
std::string Api::text(const std::string& path) {
    auto r = cli_->Get(path);
    return (r && r->status == 200) ? r->body : std::string("(core unreachable)");
}
bool Api::alive() { return (bool)cli_->Get("/api/state"); }

}  // namespace ui
