#include "app.h"
#include <cmath>
#include <cstdio>
#include <ctime>
#include "theme.h"
#include "widgets.h"

namespace ui {
using json = nlohmann::json;

bool App::connect(int port) {
    api.reset(new Api(port));
    return api->alive();
}

void App::loadCatalog() {
    json c = api->get("/api/catalog");
    markers.clear(); mIndex.clear();
    for (auto& e : c) {
        UMarker m;
        m.id = e["id"]; m.group = e["group"]; m.name = e["name"]; m.unit = e["unit"]; m.cluster = e["cluster"];
        m.cluster_name = e["cluster_name"]; m.apparatus = e["apparatus"]; m.avail = e["avail"]; m.freq = e["frequency"];
        m.abs = e["abs"]; m.bad_dir = e["bad_dir"]; m.note_danger = e["note_danger"]; m.note_concern = e["note_concern"];
        m.lo = e["span"][0]; m.hi = e["span"][1];
        const char* tn[4] = {"earth", "space", "concern", "danger"};
        for (int t = 0; t < 4; ++t) {
            m.text[t] = e["text"][tn[t]];
            for (int s = 0; s < 2; ++s)
                for (auto& iv : e["tiers"][tn[t]][s ? "F" : "M"]) m.tier[t][s].push_back({iv[0], iv[1]});
        }
        mIndex[m.id] = (int)markers.size();
        markers.push_back(std::move(m));
    }
    zones = api->get("/api/zones");
}

void App::refreshState() {
    json s = api->get("/api/state");
    commsLost = s.is_null();
    if (commsLost) return;
    int prevId = (sel >= 0 && sel < (int)crew.size()) ? crew[sel].id : -1;
    day = s["day"]; missionDays = s["mission_days"];
    auto& m = s["mission"];
    mission.name = m["name"]; mission.vehicle = m["vehicle"]; mission.type = m["type"]; mission.duration = m["duration"];
    mission.simulated = m["simulated"]; mission.file = m["profile_file"];
    sysNames.clear();
    for (auto& n : s["system_names"]) sysNames.push_back(n);
    crew.clear();
    for (auto& c : s["crew"]) {
        UCrew u;
        u.id = c["id"]; u.name = c["name"]; u.role = c["role"]; u.sex = c["sex"]; u.age = c["age"]; u.height = c["height"]; u.weight = c["weight"];
        u.edu = c["edu"]; u.notes = c["notes"]; u.scenario = c["scenario"];
        u.index = c["index"]; u.status = c["status"]; u.alerts = c["alerts"]; u.danger = c["danger"]; u.concern = c["concern"]; u.total = c["total"];
        for (int i = 0; i < 7; ++i) u.systems[i] = c["systems"][i];
        u.hasData = c["has_data"];
        auto& mo = c["moca"];
        u.moca.has = mo["has"]; u.moca.overdue = mo["overdue"]; u.moca.lastDay = mo["last_day"]; u.moca.lastTotal = mo["last_total"];
        u.moca.daysSince = mo["days_since"]; u.moca.dueIn = mo["due_in"];
        crew.push_back(u);
    }
    events.clear();
    for (auto& e : api->get("/api/events")) events.push_back({e["day"], e["crew"], e["text"], e["sev"]});
    // keep the same crew member selected when the list changes
    sel = 0;
    for (size_t i = 0; i < crew.size(); ++i) if (crew[i].id == prevId) sel = (int)i;
}

void App::refreshProfiles() {
    profiles.clear();
    for (auto& p : api->get("/api/profiles"))
        profiles.push_back({p["file"], p["name"], p["vehicle"], p["type"], p["crew"], p["day"], p["simulated"], p["current"]});
}

void App::refreshCrew() {
    labs.clear(); mind.clear(); pains.clear(); advice.clear(); moca.clear(); report.clear();
    if (crew.empty()) return;
    if (sel >= (int)crew.size()) sel = 0;
    int id = crew[sel].id;
    std::string b = "/api/crew/" + std::to_string(id);
    for (auto& l : api->get(b + "/labs")) {
        auto it = mIndex.find(l["id"].get<std::string>());
        if (it == mIndex.end()) continue;
        ULab u{it->second, l["value"], l["day"], l["sev"], l["earth_abnormal"], {}};
        for (auto& h : l["hist"]) u.hist.push_back({h[0], h[1]});
        labs.push_back(std::move(u));
    }
    for (auto& m : api->get(b + "/mind"))
        mind.push_back({m["day"], m["mood"], m["stress"], m["anxiety"], m["fatigue"], m["isolation"], m["sleep"], m["pvt"], m["score"]});
    for (auto& p : api->get(b + "/pain"))
        pains.push_back({p["id"], p["day"], p["view"], p["zone"], p["kind"], p["note"], p["x"], p["y"], p["intensity"], p["duration_h"], p["active"]});
    for (auto& a : api->get(b + "/advice")) {
        UAdvice u{a["sev"], a["title"], a["system"], {}, {}};
        for (auto& w : a["why"]) u.why.push_back(w);
        for (auto& w : a["actions"]) u.actions.push_back(w);
        advice.push_back(std::move(u));
    }
    json mj = api->get(b + "/moca");
    if (mj.is_object()) {
        for (auto& e : mj["entries"]) {
            UMoca u;
            u.day = e["day"]; u.bonus = e["bonus"]; u.total = e["total"]; u.minutes = e["minutes"]; u.version = e["version"]; u.note = e["note"]; u.cls = e["class"];
            const char* k[7] = {"visuospatial", "naming", "attention", "language", "abstraction", "delayed_recall", "orientation"};
            for (int i = 0; i < 7; ++i) u.d[i] = e[k[i]];
            moca.push_back(u);
        }
        mocaNextVersion = mj["next_version"]; mocaBonus = mj["bonus"];
    }
    report = api->text(b + "/report");
}

const ULab* App::labFor(int mi) const {
    for (auto& l : labs) if (l.mi == mi) return &l;
    return nullptr;
}

int App::overdueCount() const {
    int n = 0;
    for (auto& c : crew) if (c.moca.overdue) ++n;
    return n;
}

// ------------------------------------------------------------------ chrome
static void icon(ImDrawList* dl, int k, ImVec2 c, ImU32 col) {
    float s = 8.f;
    switch (k) {
    case S_OVERVIEW:
        for (int i = 0; i < 4; ++i) {
            ImVec2 o(c.x + (i % 2 ? 1.5f : -s), c.y + (i / 2 ? 1.5f : -s));
            dl->AddRect(o, o + ImVec2(s - 1.5f, s - 1.5f), col, 2.f, 0, 1.6f);
        }
        break;
    case S_VITALS: {
        ImVec2 p[7] = {{-9, 0}, {-4, 0}, {-2, -8}, {1, 8}, {3, -2}, {5, 0}, {9, 0}};
        for (auto& q : p) q = q + c;
        dl->AddPolyline(p, 7, col, 0, 1.8f);
        break; }
    case S_MIND:
        dl->AddCircle(c, 9, col, 20, 1.6f); dl->AddCircle(c, 4.5f, col, 16, 1.6f); dl->AddCircleFilled(c, 1.6f, col);
        break;
    case S_MOCA: {
        dl->AddCircle(c, 9, col, 20, 1.6f);
        ImVec2 p[3] = {c + ImVec2(-4, 0), c + ImVec2(-1, 3.5f), c + ImVec2(5, -4)};
        dl->AddPolyline(p, 3, col, 0, 1.8f);
        break; }
    case S_BODY:
        dl->AddCircle(c + ImVec2(0, -7), 3, col, 12, 1.6f);
        dl->AddLine(c + ImVec2(0, -4), c + ImVec2(0, 3), col, 1.8f);
        dl->AddLine(c + ImVec2(-7, -2), c + ImVec2(7, -2), col, 1.8f);
        dl->AddLine(c + ImVec2(0, 3), c + ImVec2(-5, 10), col, 1.8f);
        dl->AddLine(c + ImVec2(0, 3), c + ImVec2(5, 10), col, 1.8f);
        break;
    case S_ADVISOR: {
        ImVec2 p[3] = {c + ImVec2(0, -9), c + ImVec2(9, 8), c + ImVec2(-9, 8)};
        dl->AddPolyline(p, 3, col, ImDrawFlags_Closed, 1.7f);
        dl->AddLine(c + ImVec2(0, -2), c + ImVec2(0, 3), col, 1.8f);
        dl->AddCircleFilled(c + ImVec2(0, 5.5f), 1.2f, col);
        break; }
    case S_REPORT:
        dl->AddRect(c + ImVec2(-7, -9), c + ImVec2(7, 9), col, 2.f, 0, 1.6f);
        for (int i = 0; i < 3; ++i) dl->AddLine(c + ImVec2(-4, -4.f + i * 4.f), c + ImVec2(4, -4.f + i * 4.f), col, 1.4f);
        break;
    case S_SETUP:
        dl->AddCircle(c + ImVec2(-3, -3), 4.5f, col, 14, 1.6f);
        dl->AddCircle(c + ImVec2(5, 1), 3.5f, col, 14, 1.6f);
        dl->AddLine(c + ImVec2(-9, 9), c + ImVec2(3, 9), col, 1.8f);
        dl->AddLine(c + ImVec2(3, 9), c + ImVec2(9, 9), col, 1.8f);
        break;
    }
}

static void emptyState(App& a, ImVec2 size) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(p + ImVec2(size.x / 2 - 260, size.y / 2 - 110));
    if (BeginCard("empty", ImVec2(520, 220), C::cyan)) {
        SectionTitle("NO CREW IN THIS MISSION");
        ImGui::Dummy(ImVec2(1, 4));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 480);
        TextC(fBody, C::dim, "This profile has no crew members yet. Open Mission & Crew to load a saved profile or add the crew and their details.");
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(1, 8));
        if (NeonButton("OPEN MISSION & CREW", ImVec2(0, 44), C::cyan)) { a.screen = S_SETUP; a.refreshProfiles(); }
    }
    EndCard();
}

void App::frame(float dt) {
    time += dt;
    refreshTimer += dt;
    if (toastT > 0) toastT -= dt;
    if (refreshTimer > 6.f) { refreshTimer = 0; refreshAll(); }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 W = io.DisplaySize;
    drawBackground(time);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(W);
    ImGui::Begin("root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float TOP = 68, SIDE = 232;

    // ---- top bar
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W.x, TOP), IM_COL32(6, 10, 20, 235));
    dl->AddLine(ImVec2(0, TOP), ImVec2(W.x, TOP), C::border);
    {
        ImVec2 c(40, TOP / 2);
        dl->AddCircleFilled(c, 11, IM_COL32(30, 90, 190, 255), 24);
        dl->AddCircleFilled(c + ImVec2(-3, -3), 5.5f, IM_COL32(90, 180, 255, 140), 16);
        dl->AddEllipse(c, ImVec2(22, 8), C::alpha(C::cyan, 0.8f), -0.45f, 48, 1.5f);
        float a = time * 1.4f;
        ImVec2 sp(std::cos(a) * 22, std::sin(a) * 8);
        float cs = std::cos(-0.45f), sn = std::sin(-0.45f);
        dl->AddCircleFilled(c + ImVec2(sp.x * cs - sp.y * sn, sp.x * sn + sp.y * cs), 3.2f, C::cyan);
        ImGui::PushFont(fTitle);
        dl->AddText(ImVec2(72, 12), C::text, "AIChE HEALTH");
        ImGui::PopFont();
        ImGui::PushFont(fSmall);
        std::string sub = mission.name.empty() ? "NO MISSION LOADED" : mission.name;
        if (!mission.vehicle.empty()) sub += "  /  " + mission.vehicle;
        dl->AddText(nullptr, 0.f, ImVec2(73, 40), C::dim, sub.c_str(), nullptr, 340.f);
        ImGui::PopFont();
    }
    {
        float cx = W.x * 0.5f + 60, bw = 280;
        ImGui::PushFont(fBold);
        char b[64]; snprintf(b, sizeof b, "MISSION DAY %d / %d", day, missionDays);
        ImVec2 ts = ImGui::CalcTextSize(b);
        dl->AddText(ImVec2(cx - ts.x / 2, 10), C::text, b);
        ImGui::PopFont();
        dl->AddRectFilled(ImVec2(cx - bw / 2, 42), ImVec2(cx + bw / 2, 48), C::alpha(C::faint, 0.45f), 3);
        float f = missionDays ? std::min(1.f, (float)day / missionDays) : 0;
        dl->AddRectFilledMultiColor(ImVec2(cx - bw / 2, 42), ImVec2(cx - bw / 2 + bw * f, 48), C::blue, C::cyan, C::cyan, C::blue);
        dl->AddCircleFilled(ImVec2(cx - bw / 2 + bw * f, 45), 5, C::text);
    }
    {
        float x = W.x - 24;
        ImGui::SetCursorScreenPos(ImVec2(x - 124, 16));
        if (NeonButton(mission.simulated ? "SIM +1 DAY" : "NEXT DAY", ImVec2(124, 36), C::green)) {
            api->post("/api/sim/advance", {{"days", 1}});
            refreshAll();
            notify(mission.simulated ? "Simulated 1 mission day: new samples recorded" : "Mission day advanced");
        }
        x -= 138;
        ImGui::SetCursorScreenPos(ImVec2(x - 188, 18));
        if (Pill(earthNorms ? "REFERENCE: EARTH NORMS" : "REFERENCE: SPACE NORMS", earthNorms, C::amber, 188)) earthNorms = !earthNorms;
        x -= 202;
        float pulse = 0.5f + 0.5f * std::sin(time * 3.f);
        ImGui::PushFont(fSmall);
        const char* ct = commsLost ? "CORE LINK LOST" : "COMMS: OFFLINE";
        ImVec2 ts = ImGui::CalcTextSize(ct);
        ImVec2 p(x - ts.x - 34, 20);
        dl->AddRectFilled(p, p + ImVec2(ts.x + 34, 28), C::alpha(C::red, 0.12f), 14);
        dl->AddRect(p, p + ImVec2(ts.x + 34, 28), C::alpha(C::red, 0.55f), 14);
        dl->AddCircleFilled(p + ImVec2(14, 14), 4, C::alpha(C::red, 0.5f + 0.5f * pulse));
        dl->AddCircle(p + ImVec2(14, 14), 4 + 4 * pulse, C::alpha(C::red, 0.4f * (1 - pulse)), 16, 1.5f);
        dl->AddText(p + ImVec2(26, 6), C::red, ct);
        ImGui::PopFont();
    }

    // ---- sidebar
    dl->AddRectFilled(ImVec2(0, TOP), ImVec2(SIDE, W.y), IM_COL32(7, 11, 22, 215));
    dl->AddLine(ImVec2(SIDE, TOP), ImVec2(SIDE, W.y), C::border);
    static const char* names[S_COUNT] = {"Crew Overview", "Vitals & Labs", "Mind", "MoCA Test", "Body Map", "Advisor", "Mission Report", "Mission & Crew"};
    int badge[S_COUNT] = {0};
    if (!crew.empty()) {
        badge[S_ADVISOR] = cur().alerts;
        badge[S_VITALS] = cur().danger + cur().concern;
        badge[S_MOCA] = overdueCount();
    }
    const float NAVH = 44;
    for (int i = 0; i < S_COUNT; ++i) {
        ImVec2 p(14, TOP + 12 + i * NAVH + (i == S_SETUP ? 10 : 0));
        if (i == S_SETUP) dl->AddLine(ImVec2(24, p.y - 6), ImVec2(SIDE - 24, p.y - 6), C::alpha(C::border, 0.9f));
        ImGui::SetCursorScreenPos(p);
        char id[32]; snprintf(id, sizeof id, "##nav%d", i);
        bool clicked = ImGui::InvisibleButton(id, ImVec2(SIDE - 28, NAVH - 4));
        bool hov = ImGui::IsItemHovered(), act = (screen == i);
        float k = Anim(id, act ? 1.f : (hov ? 0.4f : 0.f));
        dl->AddRectFilled(p, p + ImVec2(SIDE - 28, NAVH - 4), C::alpha(C::cyan, 0.16f * k), 10);
        if (act) dl->AddRectFilled(p + ImVec2(0, 8), p + ImVec2(3, NAVH - 12), C::cyan, 2);
        icon(dl, i, p + ImVec2(26, (NAVH - 4) / 2), act ? C::cyan : (hov ? C::text : C::dim));
        ImGui::PushFont(fBold);
        dl->AddText(p + ImVec2(50, 8), act ? C::text : (hov ? C::text : C::dim), names[i]);
        ImGui::PopFont();
        if (badge[i] > 0) {
            char bb[8]; snprintf(bb, sizeof bb, "%d", badge[i]);
            ImVec2 bp = p + ImVec2(SIDE - 28 - 30, 10);
            dl->AddRectFilled(bp, bp + ImVec2(22, 20), C::alpha((i == S_ADVISOR || i == S_MOCA) ? C::red : C::amber, 0.9f), 10);
            ImGui::PushFont(fSmall);
            ImVec2 ts = ImGui::CalcTextSize(bb);
            dl->AddText(bp + ImVec2(11 - ts.x / 2, 10 - ts.y / 2), C::bg0, bb);
            ImGui::PopFont();
        }
        if (clicked) { screen = (Screen)i; if (i == S_SETUP) refreshProfiles(); }
    }
    // crew selector
    float cy = TOP + 12 + S_COUNT * NAVH + 22;
    ImGui::SetCursorScreenPos(ImVec2(20, cy));
    Label("CREW  (click to switch)");
    int maxRows = std::max(0, (int)((W.y - cy - 36) / 58));
    for (size_t i = 0; i < crew.size() && (int)i < maxRows; ++i) {
        ImVec2 p(14, cy + 26 + i * 58);
        ImGui::SetCursorScreenPos(p);
        char id[32]; snprintf(id, sizeof id, "##crew%zu", i);
        bool clicked = ImGui::InvisibleButton(id, ImVec2(SIDE - 28, 52));
        bool hov = ImGui::IsItemHovered(), act = ((int)i == sel);
        float k = Anim(id, act ? 1.f : (hov ? 0.4f : 0.f));
        ImU32 sc = crew[i].hasData ? sevColor(crew[i].status) : C::faint;
        dl->AddRectFilled(p, p + ImVec2(SIDE - 28, 52), C::alpha(sc, 0.12f * k + 0.02f), 10);
        if (act) dl->AddRect(p, p + ImVec2(SIDE - 28, 52), C::alpha(sc, 0.7f), 10, 0, 1.3f);
        ImVec2 ac = p + ImVec2(28, 26);
        dl->AddCircleFilled(ac, 18, IM_COL32(26, 38, 66, 255), 24);
        dl->AddCircle(ac, 18, sc, 24, 2.f);
        std::string nm = crew[i].name;
        char ini[8] = {0};
        ini[0] = nm.empty() ? '?' : nm[0];
        size_t sp = nm.find(' ');
        if (sp != std::string::npos && sp + 1 < nm.size()) ini[1] = nm[sp + 1];
        ImGui::PushFont(fBold);
        ImVec2 ts = ImGui::CalcTextSize(ini);
        dl->AddText(ac - ts * 0.5f, C::text, ini);
        dl->AddText(nullptr, 0.f, p + ImVec2(54, 6), act ? C::text : C::dim, nm.c_str(), nullptr, SIDE - 28 - 62);
        ImGui::PopFont();
        ImGui::PushFont(fSmall);
        char sub[96]; snprintf(sub, sizeof sub, "%s  /  %d", crew[i].role.c_str(), (int)std::round(crew[i].index));
        dl->AddText(nullptr, 0.f, p + ImVec2(54, 28), C::faint, sub, nullptr, SIDE - 28 - 62);
        ImGui::PopFont();
        if (clicked) selectCrew((int)i);
    }

    // ---- content
    ImVec2 origin(SIDE + 22, TOP + 20);
    ImVec2 csize(W.x - origin.x - 22, W.y - origin.y - 20);
    ImGui::SetCursorScreenPos(origin);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("content", csize, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    if (commsLost) {
        TextC(fBold, C::red, "Cannot reach the AIChE Health core service.");
    } else if (screen == S_SETUP) {
        drawSetup(*this, csize);
    } else if (crew.empty()) {
        emptyState(*this, csize);
    } else switch (screen) {
        case S_OVERVIEW: drawOverview(*this, csize); break;
        case S_VITALS: drawVitals(*this, csize); break;
        case S_MIND: drawMind(*this, csize); break;
        case S_MOCA: drawMoca(*this, csize); break;
        case S_BODY: drawBody(*this, csize); break;
        case S_ADVISOR: drawAdvisor(*this, csize); break;
        case S_REPORT: drawReport(*this, csize); break;
        default: break;
    }
    ImGui::EndChild();

    // ---- toast
    if (toastT > 0) {
        float a = std::min(1.f, toastT * 2.f);
        ImGui::PushFont(fBody);
        ImVec2 ts = ImGui::CalcTextSize(toast.c_str());
        ImVec2 p((W.x + SIDE) / 2 - ts.x / 2 - 20, W.y - 70);
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(p, p + ImVec2(ts.x + 40, ts.y + 20), C::alpha(C::panel2, 0.97f * a), 14);
        fg->AddRect(p, p + ImVec2(ts.x + 40, ts.y + 20), C::alpha(C::cyan, 0.8f * a), 14);
        fg->AddText(p + ImVec2(20, 10), C::alpha(C::text, a), toast.c_str());
        ImGui::PopFont();
    }
    ImGui::End();
}

}  // namespace ui
