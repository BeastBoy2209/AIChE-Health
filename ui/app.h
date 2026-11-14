#pragma once
#include <imgui.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "api.h"

namespace ui {

struct UMarker {
    std::string id, group, name, unit, cluster_name, apparatus, avail, freq, bad_dir, note_danger, note_concern;
    std::string text[4];
    int cluster = 1;
    bool abs = false;
    double lo = 0, hi = 1;
    std::vector<std::pair<double, double>> tier[4][2];
};
struct ULab { int mi; double value; int day, sev; bool earthAb; std::vector<std::pair<int, double>> hist; };
struct UMocaInfo { bool has = false, overdue = true; int lastDay = 0, lastTotal = 0, daysSince = -1, dueIn = 0; };
struct UCrew {
    int id; std::string name, role; int sex, age; double height, weight; int edu; std::string notes, scenario;
    double index; int status, alerts, danger, concern, total; double systems[7]; bool hasData; UMocaInfo moca;
};
struct UMind { int day; double mood, stress, anxiety, fatigue, isolation, sleep, pvt, score; };
struct UPain { long long id; int day; std::string view, zone, kind, note; double x, y; int intensity; double dur; bool active; };
struct UAdvice { int sev; std::string title, system; std::vector<std::string> why, actions; };
struct UEvent { int day; std::string crew, text; int sev; };
struct UMoca { int day, d[7], bonus, total, minutes; std::string version, note, cls; };
struct UProfile { std::string file, name, vehicle, type; int crew, day; bool simulated, current; };
struct UMission { std::string name, vehicle, type, file; int duration = 180; bool simulated = false; };

enum Screen { S_OVERVIEW, S_VITALS, S_MIND, S_MOCA, S_BODY, S_ADVISOR, S_REPORT, S_SETUP, S_COUNT };

struct App {
    std::unique_ptr<Api> api;
    // mission state
    int day = 0, missionDays = 180;
    UMission mission;
    std::vector<std::string> sysNames;
    std::vector<UCrew> crew;
    std::vector<UEvent> events;
    std::vector<UProfile> profiles;
    // catalog
    std::vector<UMarker> markers;
    std::map<std::string, int> mIndex;
    nlohmann::json zones;
    // selected crew data
    int sel = 0;
    std::vector<ULab> labs;
    std::vector<UMind> mind;
    std::vector<UPain> pains;
    std::vector<UAdvice> advice;
    std::vector<UMoca> moca;
    std::string mocaNextVersion = "8.1";
    int mocaBonus = 0;
    std::string report;
    // ui state
    Screen screen = S_OVERVIEW;
    bool earthNorms = false;
    float time = 0, refreshTimer = 0;
    std::string toast; float toastT = 0;
    std::string assetDir, profilesDir;
    bool commsLost = false;

    bool connect(int port);
    void loadCatalog();
    void refreshState();     // mission + crew + events
    void refreshCrew();      // selected crew details
    void refreshProfiles();
    void refreshAll() { refreshState(); refreshCrew(); }
    void selectCrew(int i) { if (i < 0 || i >= (int)crew.size()) return; sel = i; refreshCrew(); }
    void selectCrewId(int id) { for (size_t i = 0; i < crew.size(); ++i) if (crew[i].id == id) selectCrew((int)i); }
    void notify(const std::string& s) { toast = s; toastT = 3.5f; }
    const UCrew& cur() const { return crew[sel]; }
    const ULab* labFor(int mi) const;
    int overdueCount() const;
    void frame(float dt);
};

// screens
void drawOverview(App& a, ImVec2 size);
void drawVitals(App& a, ImVec2 size);
void drawMind(App& a, ImVec2 size);
void drawMoca(App& a, ImVec2 size);
void drawBody(App& a, ImVec2 size);
void drawAdvisor(App& a, ImVec2 size);
void drawReport(App& a, ImVec2 size);
void drawSetup(App& a, ImVec2 size);
bool loadBodyTextures(const std::string& assetDir);

}  // namespace ui
