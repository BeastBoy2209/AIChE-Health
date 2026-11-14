#include "server.h"
#include <httplib.h>

namespace av {
using json = nlohmann::json;

ApiServer::ApiServer(Engine& e) : eng_(e), srv_(new httplib::Server()) {}
ApiServer::~ApiServer() { stop(); }

static void reply(httplib::Response& res, const json& j) { res.set_content(j.dump(), "application/json"); }
static json body(const httplib::Request& r) { json j = json::parse(r.body, nullptr, false); return j.is_discarded() ? json::object() : j; }
static int cid(const httplib::Request& r) { return std::stoi(r.matches[1]); }

bool ApiServer::start(int port, bool blocking) {
    auto& s = *srv_;
    s.Get("/api/state", [&](const httplib::Request&, httplib::Response& res) { reply(res, eng_.state()); });
    s.Get("/api/catalog", [&](const httplib::Request&, httplib::Response& res) { reply(res, eng_.catalogJson()); });
    s.Get("/api/zones", [&](const httplib::Request&, httplib::Response& res) { reply(res, eng_.zones()); });
    s.Get("/api/events", [&](const httplib::Request&, httplib::Response& res) { reply(res, eng_.events(60)); });
    s.Post("/api/sim/advance", [&](const httplib::Request& r, httplib::Response& res) {
        reply(res, eng_.advance((int)body(r).value("days", 1)));
    });
    // mission, profiles, crew management
    s.Get("/api/mission", [&](const httplib::Request&, httplib::Response& res) { reply(res, eng_.mission()); });
    s.Post("/api/mission", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.updateMission(body(r))); });
    s.Get("/api/profiles", [&](const httplib::Request&, httplib::Response& res) { reply(res, eng_.profiles()); });
    s.Post("/api/profile/load", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.loadProfile(body(r).value("file", ""))); });
    s.Post("/api/profile/new", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.newProfile(body(r))); });
    s.Post("/api/profile/saveas", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.saveProfileAs(body(r).value("file", "profile"))); });
    s.Post("/api/crew", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.addCrew(body(r))); });
    s.Post(R"(/api/crew/(\d+)/update)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.updateCrew(cid(r), body(r))); });
    s.Post(R"(/api/crew/(\d+)/delete)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.deleteCrew(cid(r))); });
    s.Get(R"(/api/crew/(\d+)/moca)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.moca(cid(r))); });
    s.Post(R"(/api/crew/(\d+)/moca)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.addMoca(cid(r), body(r))); });
    s.Get(R"(/api/crew/(\d+)/labs)",[&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.labs(cid(r))); });
    s.Post(R"(/api/crew/(\d+)/labs)", [&](const httplib::Request& r, httplib::Response& res) {
        json b = body(r);
        reply(res, eng_.addLab(cid(r), b.value("bio", ""), b.value("value", 0.0)));
    });
    s.Get(R"(/api/crew/(\d+)/mind)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.mind(cid(r))); });
    s.Post(R"(/api/crew/(\d+)/mind)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.addMind(cid(r), body(r))); });
    s.Get(R"(/api/crew/(\d+)/pain)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.pain(cid(r))); });
    s.Post(R"(/api/crew/(\d+)/pain)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.addPain(cid(r), body(r))); });
    s.Post(R"(/api/crew/(\d+)/pain/(\d+)/resolve)", [&](const httplib::Request& r, httplib::Response& res) {
        reply(res, eng_.resolvePain(cid(r), std::stoll(r.matches[2])));
    });
    s.Get(R"(/api/crew/(\d+)/advice)", [&](const httplib::Request& r, httplib::Response& res) { reply(res, eng_.advice(cid(r))); });
    s.Get(R"(/api/crew/(\d+)/report)", [&](const httplib::Request& r, httplib::Response& res) {
        res.set_content(eng_.report(cid(r)), "text/plain; charset=utf-8");
    });
    if (!s.bind_to_port("127.0.0.1", port)) return false;
    if (blocking) { s.listen_after_bind(); return true; }
    th_ = std::thread([this] { srv_->listen_after_bind(); });
    return true;
}

void ApiServer::stop() {
    if (srv_) srv_->stop();
    if (th_.joinable()) th_.join();
}

}  // namespace av
