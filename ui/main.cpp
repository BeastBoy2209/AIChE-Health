// aiche-health: native mission-control UI (Dear ImGui + GLFW + OpenGL).
//   aiche-health [--connect] [--port N] [--profile FILE] [--profiles DIR] [--size WxH] [--shot DIR]
// Default: starts the offline core inside this process (127.0.0.1:port) and talks to it over HTTP.
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "app.h"
#include "server.h"
#include "theme.h"

static bool exists(const std::string& p) { std::ifstream f(p, std::ios::binary); return (bool)f; }

static std::string findDir(const char* name, const std::string& probe, const std::string& exeDir) {
    const char* pre[] = {"", "../", "../../", "../../../"};
    for (auto p : pre) if (exists(std::string(p) + name + "/" + probe)) return std::string(p) + name;
    for (auto p : pre) if (exists(exeDir + "/" + p + name + "/" + probe)) return exeDir + "/" + p + name;
    return name;
}

static void savePng(const std::string& path, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    std::vector<unsigned char> fl((size_t)w * h * 4);
    for (int y = 0; y < h; ++y) memcpy(&fl[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    for (size_t i = 3; i < fl.size(); i += 4) fl[i] = 255;
    stbi_write_png(path.c_str(), w, h, 4, fl.data(), w * 4);
}

int main(int argc, char** argv) {
    int port = 8787, W = 1600, H = 940;
    bool connectOnly = false;
    std::string shotDir, profile, profilesArg;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--connect")) connectOnly = true;
        else if (!strcmp(argv[i], "--profile") && i + 1 < argc) profile = argv[++i];
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &W, &H);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shotDir = argv[++i];
        else if (!strcmp(argv[i], "--profiles") && i + 1 < argc) profilesArg = argv[++i];
    }
    std::string exe = argv[0], exeDir = ".";
    auto sl = exe.find_last_of("/\\");
    if (sl != std::string::npos) exeDir = exe.substr(0, sl);
    std::string dataDir = findDir("data", "biomarkers.json", exeDir), assetDir = findDir("assets", "male_front.jpg", exeDir);
    std::string profilesDir = profilesArg.empty() ? findDir("profiles", "demo_expedition.json", exeDir) : profilesArg;

    try { profilesDir = std::filesystem::absolute(profilesDir).string(); } catch (...) {}
    av::Engine engine;
    std::unique_ptr<av::ApiServer> server;
    if (!connectOnly) {
        if (!engine.init(dataDir, profilesDir, profile)) { fprintf(stderr, "core init failed: %s\n", engine.lastError.c_str()); return 1; }
        server.reset(new av::ApiServer(engine));
        if (!server->start(port, false)) { fprintf(stderr, "port %d busy: connecting to the running core instead\n", port); server.reset(); }
    }

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* win = glfwCreateWindow(W, H, "AIChE Health - Crew Health Monitor", nullptr, nullptr);
    if (!win) { fprintf(stderr, "cannot create window\n"); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ui::loadFonts(assetDir, 1.0f);
    ui::applyStyle();

    ui::App app;
    app.assetDir = assetDir;
    app.profilesDir = profilesDir;
    if (!app.connect(port)) { fprintf(stderr, "cannot reach aiche-core on port %d\n", port); return 1; }
    app.loadCatalog();
    app.refreshAll();
    app.refreshProfiles();
    ui::loadBodyTextures(assetDir);

    // screenshot script: "screen:crew" pairs
    struct Shot { int screen, crew; const char* name; };
    std::vector<Shot> shots = {{0, 3, "overview"}, {1, 0, "vitals"}, {2, 0, "mind"}, {3, 1, "moca"}, {4, 1, "body"}, {5, 3, "advisor"}, {6, 0, "report"}, {7, 2, "setup"}};
    size_t shotIdx = 0; int shotFrames = 0;

    double last = glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        double now = glfwGetTime();
        float dt = (float)(now - last); last = now;
        if (!shotDir.empty() && shotFrames == 0 && shotIdx < shots.size()) {
            app.screen = (ui::Screen)shots[shotIdx].screen;
            app.selectCrew(shots[shotIdx].crew);
        }
        app.frame(dt);
        ImGui::Render();
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.02f, 0.03f, 0.06f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!shotDir.empty()) {
            if (++shotFrames >= 45) {
                savePng(shotDir + "/" + shots[shotIdx].name + ".png", fw, fh);
                shotFrames = 0;
                if (++shotIdx >= shots.size()) glfwSetWindowShouldClose(win, 1);
            }
        }
        glfwSwapBuffers(win);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    if (server) server->stop();
    return 0;
}



