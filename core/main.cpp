// aiche-core: standalone offline health daemon.
//   aiche-core [--data DIR] [--profiles DIR] [--profile FILE] [--port N] [--make-demo FILE]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "server.h"

int main(int argc, char** argv) {
    std::string data = "data", profiles = "profiles", profile, demo;
    int port = 8787;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--data") && i + 1 < argc) data = argv[++i];
        else if (!strcmp(argv[i], "--profiles") && i + 1 < argc) profiles = argv[++i];
        else if (!strcmp(argv[i], "--profile") && i + 1 < argc) profile = argv[++i];
        else if (!strcmp(argv[i], "--make-demo") && i + 1 < argc) demo = argv[++i];
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
    }
    av::Engine eng;
    if (!eng.init(data, profiles, demo.empty() ? profile : "")) { fprintf(stderr, "init failed: %s\n", eng.lastError.c_str()); return 1; }
    if (!demo.empty()) {
        bool ok = eng.buildDemo(demo);
        printf(ok ? "demo profile written: %s\n" : "cannot write %s\n", demo.c_str());
        return ok ? 0 : 1;
    }
    av::ApiServer srv(eng);
    printf("aiche-core listening on 127.0.0.1:%d (offline, no external network)\n", port);
    fflush(stdout);
    if (!srv.start(port, true)) { fprintf(stderr, "cannot bind port %d\n", port); return 1; }
    return 0;
}
