// Local-only REST API over the Engine (127.0.0.1, no external network).
#pragma once
#include <memory>
#include <string>
#include <thread>
#include "engine.h"

namespace httplib { class Server; }

namespace av {

class ApiServer {
public:
    explicit ApiServer(Engine& e);
    ~ApiServer();
    bool start(int port, bool blocking);  // non-blocking => runs on a thread
    void stop();
private:
    Engine& eng_;
    std::unique_ptr<httplib::Server> srv_;
    std::thread th_;
};

}  // namespace av
