#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "widgets.h"

namespace ui {
using json = nlohmann::json;

static ImU32 mocaColor(int t) { return t >= 26 ? C::green : (t >= 18 ? C::amber : C::red); }
static const char* mocaClassName(int t) { return t >= 26 ? "Normal" : (t >= 18 ? "Mild impairment range" : (t >= 10 ? "Moderate impairment range" : "Severe impairment range")); }

struct Dom { const char* name; const char* desc; int max; };
static const Dom DOM[7] = {
    {"Visuospatial / Executive", "Trail-making, cube copy, clock drawing", 5},
    {"Naming", "Naming of three animals", 3},
    {"Attention", "Digit span, tapping, serial subtraction", 6},
    {"Language", "Sentence repetition, verbal fluency", 3},
    {"Abstraction", "Similarity between word pairs", 2},
    {"Delayed recall", "Five words recalled without cues", 5},
    {"Orientation", "Date, month, year, day, place, city", 6},
};
static const char* KEYS[7] = {"visuospatial", "naming", "attention", "language", "abstraction", "delayed_recall", "orientation"};

static void trendChart(ImVec2 p, ImVec2 sz, const std::vector<UMoca>& m) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + sz, IM_COL32(9, 14, 28, 255), 10);
    float top = p.y + 14, bot = p.y + sz.y - 22;
    auto Y = [&](double v) { return bot - (float)((v - 14) / 16.0) * (bot - top); };
    dl->AddLine(ImVec2(p.x + 34, Y(26)), ImVec2(p.x + sz.x - 8, Y(26)), C::alpha(C::green, 0.7f), 1.2f);
    ImGui::PushFont(fSmall);
    dl->AddText(ImVec2(p.x + 6, Y(26) - 8), C::green, "26");
    dl->AddText(ImVec2(p.x + 6, Y(30) - 8), C::faint, "30");
    dl->AddText(ImVec2(p.x + 6, Y(18) - 8), C::faint, "18");
    ImGui::PopFont();
    size_t n = std::min<size_t>(m.size(), 12), from = m.size() - n;
    if (n == 0) { ImGui::PushFont(fSmall); dl->AddText(p + ImVec2(44, sz.y / 2 - 8), C::dim, "No assessments yet"); ImGui::PopFont(); return; }
    float bw = (sz.x - 48) / 12.f;
    for (size_t i = 0; i < n; ++i) {
        const UMoca& e = m[from + i];
        float h = std::max(2.f, bot - Y(std::max(14, e.total)));
        ImVec2 b0(p.x + 40 + i * bw, bot - h), b1(p.x + 40 + i * bw + bw - 6, bot);
        dl->AddRectFilled(b0, b1, C::alpha(mocaColor(e.total), 0.85f), 3);
        ImGui::PushFont(fSmall);
        char t[8]; snprintf(t, sizeof t, "%d", e.total);
        ImVec2 ts = ImGui::CalcTextSize(t);
        dl->AddText(ImVec2((b0.x + b1.x) / 2 - ts.x / 2, b0.y - 15), C::text, t);
        ImGui::PopFont();
        if (ImGui::IsMouseHoveringRect(b0, b1)) ImGui::SetTooltip("day %d: %d/30, v%s", e.day, e.total, e.version.c_str());
    }
}

void drawMoca(App& a, ImVec2 size) {
    static int v[7] = {0};
    static int verIdx = 0, minutes = 10, lastCrew = -1;
    static char note[128] = "";
    const UCrew& crew = a.cur();
    if (lastCrew != crew.id) {
        lastCrew = crew.id;
        for (int i = 0; i < 7; ++i) v[i] = 0;
        note[0] = 0;
    }
    static const char* vers[3] = {"8.1", "8.2", "8.3"};
    static std::string lastNext;
    if (lastNext != a.mocaNextVersion + std::to_string(crew.id)) {
        lastNext = a.mocaNextVersion + std::to_string(crew.id);
        for (int i = 0; i < 3; ++i) if (a.mocaNextVersion == vers[i]) verIdx = i;
    }
    ImVec2 o = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const UMocaInfo& mi = crew.moca;
    float y = 0;
    if (mi.overdue) {
        float pulse = 0.5f + 0.5f * std::sin(a.time * 4.f);
        ImVec2 p = o, sz(size.x, 50);
        dl->AddRectFilled(p, p + sz, C::alpha(mi.has && mi.daysSince >= 14 ? C::red : C::amber, 0.12f + 0.05f * pulse), 12);
        dl->AddRect(p, p + sz, C::alpha(mi.has && mi.daysSince >= 14 ? C::red : C::amber, 0.8f), 12, 0, 1.6f);
        ImGui::PushFont(fBold);
        char t[200];
        if (mi.has) snprintf(t, sizeof t, "MANDATORY WEEKLY MoCA OVERDUE for %s: last test %d days ago", crew.name.c_str(), mi.daysSince);
        else snprintf(t, sizeof t, "MANDATORY BASELINE MoCA REQUIRED for %s", crew.name.c_str());
        dl->AddText(p + ImVec2(20, 13), C::text, t);
        ImGui::PopFont();
        y += 50 + 14;
    }
    float gap = 16, w1 = size.x * 0.38f, w2 = size.x - w1 - gap, H = size.y - y;
    float statH = 352;

    // ---------- status + trend
    ImGui::SetCursorScreenPos(o + ImVec2(0, y));
    if (BeginCard("mstat", ImVec2(w1, statH), C::purple)) {
        SectionTitle("MONTREAL COGNITIVE ASSESSMENT");
        TextC(fSmall, C::dim, "%s  /  one test every 7 days is mandatory", crew.name.c_str());
        ImVec2 p = ImGui::GetCursorScreenPos() + ImVec2(0, 8);
        ImVec2 rc = p + ImVec2(62, 66);
        int last = mi.has ? mi.lastTotal : 0;
        ImU32 col = mi.has ? mocaColor(last) : C::faint;
        char rid[24]; snprintf(rid, sizeof rid, "mring%d", crew.id);
        float val = Anim(rid, last / 30.f, 3.f);
        HealthRing(rc, 48, val, col, 9.f);
        ImGui::PushFont(fNum);
        char num[8]; snprintf(num, sizeof num, mi.has ? "%d" : "-", (int)std::round(val * 30));
        ImVec2 ts = ImGui::CalcTextSize(num);
        dl->AddText(rc - ts * 0.5f - ImVec2(0, 4), C::text, num);
        ImGui::PopFont();
        ImGui::PushFont(fSmall);
        dl->AddText(rc + ImVec2(-8, 18), C::dim, "/ 30");
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(p + ImVec2(136, 4));
        if (mi.has) Chip(mocaClassName(last), col);
        else Chip("NOT TESTED", C::faint);
        ImGui::PushFont(fSmall);
        char l1[96], l2[96];
        if (mi.has) {
            snprintf(l1, sizeof l1, "Last test: day %d (%d d ago)", mi.lastDay, mi.daysSince);
            if (mi.overdue) snprintf(l2, sizeof l2, "OVERDUE by %d d", -mi.dueIn);
            else snprintf(l2, sizeof l2, "Next due: day %d (in %d d)", mi.lastDay + 7, mi.dueIn);
        } else { snprintf(l1, sizeof l1, "No assessment on file"); snprintf(l2, sizeof l2, "Baseline required now"); }
        dl->AddText(p + ImVec2(136, 38), C::dim, l1);
        dl->AddText(p + ImVec2(136, 58), mi.overdue ? C::red : C::green, l2);
        char l3[96]; snprintf(l3, sizeof l3, "Next version to use: %s", a.mocaNextVersion.c_str());
        dl->AddText(p + ImVec2(136, 78), C::dim, l3);
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(p + ImVec2(0, 140));
        TextC(fBold, C::text, "Last 12 assessments");
        trendChart(ImGui::GetCursorScreenPos(), ImVec2(w1 - 34, statH - 212), a.moca);
    }
    EndCard();

    // ---------- history
    ImGui::SetCursorScreenPos(o + ImVec2(0, y + statH + gap));
    if (BeginCard("mhist", ImVec2(w1, H - statH - gap), C::blue)) {
        SectionTitle("HISTORY");
        ImGui::BeginChild("mh", ImVec2(0, 0), 0, 0);
        for (auto it = a.moca.rbegin(); it != a.moca.rend(); ++it) {
            ImVec2 q = ImGui::GetCursorScreenPos();
            ImDrawList* d2 = ImGui::GetWindowDrawList();
            d2->AddRectFilled(q, q + ImVec2(w1 - 42, 50), C::alpha(mocaColor(it->total), 0.07f), 8);
            d2->AddRectFilled(q, q + ImVec2(3, 50), mocaColor(it->total), 2);
            ImGui::PushFont(fBold);
            char t[64]; snprintf(t, sizeof t, "Day %d   %d/30", it->day, it->total);
            d2->AddText(q + ImVec2(12, 3), C::text, t);
            ImGui::PopFont();
            ImGui::PushFont(fSmall);
            char s[160];
            snprintf(s, sizeof s, "v%s  /  V%d N%d A%d L%d Ab%d R%d O%d  /  %d min", it->version.c_str(), it->d[0], it->d[1], it->d[2], it->d[3], it->d[4], it->d[5], it->d[6], it->minutes);
            d2->AddText(q + ImVec2(12, 27), C::dim, s);
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 56));
        }
        if (a.moca.empty()) TextC(fSmall, C::dim, "No records.");
        ImGui::EndChild();
    }
    EndCard();

    // ---------- new assessment form
    ImGui::SetCursorScreenPos(o + ImVec2(w1 + gap, y));
    if (BeginCard("mform", ImVec2(w2, H), mi.overdue ? C::amber : C::cyan)) {
        float iw = w2 - 34;
        SectionTitle("NEW ASSESSMENT");
        TextC(fSmall, C::dim, "Enter the scores from the paper or official digital MoCA form administered to %s", crew.name.c_str());
        ImGui::Dummy(ImVec2(1, 4));
        TextC(fBody, C::text, "Version");
        ImGui::SameLine(90);
        ImGui::SetNextItemWidth(110);
        ImGui::Combo("##ver", &verIdx, vers, 3);
        ImGui::SameLine(0, 24);
        TextC(fBody, C::text, "Duration (min)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        ImGui::InputInt("##min", &minutes, 1, 5);
        minutes = std::max(1, std::min(60, minutes));
        ImGui::Dummy(ImVec2(1, 6));
        int sum = 0;
        for (int i = 0; i < 7; ++i) {
            ImVec2 q = ImGui::GetCursorScreenPos();
            ImGui::PushFont(fBold);
            ImGui::TextUnformatted(DOM[i].name);
            ImGui::PopFont();
            TextC(fSmall, C::dim, "%s", DOM[i].desc);
            ImGui::SetCursorScreenPos(ImVec2(q.x + iw * 0.52f, q.y + 4));
            ImGui::SetNextItemWidth(iw * 0.48f);
            char id[16]; snprintf(id, sizeof id, "##d%d", i);
            char f[24]; snprintf(f, sizeof f, "%%d / %d", DOM[i].max);
            ImGui::SliderInt(id, &v[i], 0, DOM[i].max, f);
            ImGui::SetCursorScreenPos(ImVec2(q.x, q.y + 50));
            sum += v[i];
        }
        int total = std::min(30, sum + a.mocaBonus);
        ImGui::Dummy(ImVec2(1, 2));
        ImVec2 q = ImGui::GetCursorScreenPos();
        ImGui::PushFont(fBig);
        char tb[16]; snprintf(tb, sizeof tb, "%d", total);
        dl->AddText(q, mocaColor(total), tb);
        ImVec2 ts = ImGui::CalcTextSize(tb);
        ImGui::PopFont();
        ImGui::PushFont(fBold);
        dl->AddText(q + ImVec2(ts.x + 6, 22), C::dim, "/ 30");
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(q + ImVec2(ts.x + 70, 14));
        Chip(mocaClassName(total), mocaColor(total));
        if (a.mocaBonus) { ImGui::SameLine(); Chip("+1 education adjustment", C::blue); }
        ImGui::SetCursorScreenPos(q + ImVec2(0, ts.y + 4));
        ImGui::SetNextItemWidth(iw);
        ImGui::InputTextWithHint("##mnote", "Notes (environment, interruptions, symptoms)", note, sizeof note);
        if (NeonButton("SAVE ASSESSMENT", ImVec2(iw, 44), C::purple)) {
            json body;
            for (int i = 0; i < 7; ++i) body[KEYS[i]] = v[i];
            body["version"] = vers[verIdx]; body["minutes"] = minutes; body["note"] = std::string(note);
            json r = a.api->post("/api/crew/" + std::to_string(crew.id) + "/moca", body);
            a.refreshAll();
            if (r.is_object() && r.contains("total"))
                a.notify("MoCA saved: " + std::to_string((int)r["total"]) + "/30, " + r["class"].get<std::string>());
            else a.notify("Could not save MoCA");
            for (int i = 0; i < 7; ++i) v[i] = 0;
            note[0] = 0;
        }
        ImGui::Dummy(ImVec2(1, 2));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + iw);
        TextC(fSmall, C::faint, "AIChE Health records scores only. Administer the test with the official licensed MoCA materials (mocacognition.com); no test stimuli are included. Use versions 8.1, 8.2, 8.3 in rotation to limit practice effects. Normal: 26 or more of 30.");
        ImGui::PopTextWrapPos();
    }
    EndCard();
}

}  // namespace ui


