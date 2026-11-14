#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "widgets.h"

namespace ui {

namespace {
enum PvtState { P_IDLE, P_WAIT, P_GO, P_EARLY, P_DONE };
struct Pvt {
    PvtState st = P_IDLE;
    double tStart = 0, tGo = 0;
    std::vector<double> rts;
    double last = 0;
} pvt;
const int TRIALS = 5;

ImU32 pvtColor(double ms) { return ms <= 280 ? C::green : (ms <= 350 ? C::amber : C::red); }
}

static void lineChart(ImVec2 p, ImVec2 sz, const std::vector<UMind>& m, const bool* show) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + sz, IM_COL32(9, 14, 28, 255), 10);
    for (int g = 1; g <= 10; g += 3) {
        float y = p.y + sz.y - 20 - (g - 1) / 9.f * (sz.y - 40);
        dl->AddLine(ImVec2(p.x + 30, y), ImVec2(p.x + sz.x - 10, y), C::alpha(C::faint, 0.35f));
        char b[8]; snprintf(b, sizeof b, "%d", g);
        ImGui::PushFont(fSmall); dl->AddText(ImVec2(p.x + 8, y - 8), C::faint, b); ImGui::PopFont();
    }
    size_t n = std::min<size_t>(m.size(), 30), from = m.size() - n;
    if (n < 2) return;
    struct S { double UMind::*f; ImU32 c; } ser[5] = {{&UMind::mood, C::green}, {&UMind::stress, C::red}, {&UMind::anxiety, C::amber}, {&UMind::fatigue, C::purple}, {&UMind::isolation, C::blue}};
    for (int s = 0; s < 5; ++s) {
        if (!show[s]) continue;
        std::vector<ImVec2> pts;
        for (size_t i = 0; i < n; ++i) {
            double v = m[from + i].*ser[s].f;
            pts.push_back(ImVec2(p.x + 36 + (sz.x - 56) * i / (n - 1), p.y + sz.y - 20 - (float)((v - 1) / 9.0) * (sz.y - 40)));
        }
        dl->AddPolyline(pts.data(), (int)pts.size(), C::alpha(ser[s].c, 0.2f), 0, 5.f);
        dl->AddPolyline(pts.data(), (int)pts.size(), ser[s].c, 0, 2.f);
        dl->AddCircleFilled(pts.back(), 4.f, ser[s].c);
    }
    ImGui::PushFont(fSmall);
    char b[32]; snprintf(b, sizeof b, "day %d", m[from].day); dl->AddText(p + ImVec2(34, sz.y - 18), C::faint, b);
    snprintf(b, sizeof b, "day %d", m.back().day); ImVec2 ts = ImGui::CalcTextSize(b); dl->AddText(p + ImVec2(sz.x - ts.x - 10, sz.y - 18), C::faint, b);
    ImGui::PopFont();
}

void drawMind(App& a, ImVec2 size) {
    static int v[6] = {7, 3, 3, 4, 3, 7};
    static bool show[5] = {true, true, true, true, false};
    static int lastCrew = -1;
    if (lastCrew != a.sel) {
        lastCrew = a.sel;
        if (!a.mind.empty()) { auto& m = a.mind.back(); v[0] = (int)std::round(m.mood); v[1] = (int)std::round(m.stress); v[2] = (int)std::round(m.anxiety); v[3] = (int)std::round(m.fatigue); v[4] = (int)std::round(m.isolation); v[5] = (int)std::round(m.sleep); }
        pvt = Pvt();
    }
    const UCrew& crew = a.cur();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float gap = 16, w1 = size.x * 0.28f, w2 = size.x * 0.30f, w3 = size.x - w1 - w2 - gap * 2;

    // ---------- check-in
    if (BeginCard("checkin", ImVec2(w1, size.y), C::purple)) {
        SectionTitle("DAILY MENTAL CHECK-IN", C::text);
        TextC(fSmall, C::dim, "Private. Stored only on this device.");
        ImGui::Dummy(ImVec2(1, 6));
        const char* names[6] = {"Mood", "Stress", "Anxiety", "Fatigue", "Isolation / confinement", "Sleep quality"};
        const bool goodHigh[6] = {true, false, false, false, false, true};
        for (int i = 0; i < 6; ++i) {
            float good = goodHigh[i] ? (v[i] - 1) / 9.f : (10 - v[i]) / 9.f;
            ImU32 c = good > 0.6f ? C::green : (good > 0.35f ? C::amber : C::red);
            TextC(fBody, C::text, "%s", names[i]);
            ImGui::SameLine(w1 - 70);
            TextC(fBold, c, "%d / 10", v[i]);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, C::v4(c));
            ImGui::SetNextItemWidth(w1 - 34);
            char id[16]; snprintf(id, sizeof id, "##s%d", i);
            ImGui::SliderInt(id, &v[i], 1, 10, "");
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(1, 8));
        if (NeonButton("SUBMIT CHECK-IN", ImVec2(w1 - 34, 44), C::purple)) {
            a.api->post("/api/crew/" + std::to_string(crew.id) + "/mind",
                        {{"mood", v[0]}, {"stress", v[1]}, {"anxiety", v[2]}, {"fatigue", v[3]}, {"isolation", v[4]}, {"sleep", v[5]}});
            a.refreshAll();
            a.notify("Check-in saved for " + crew.name);
        }
        ImGui::Dummy(ImVec2(1, 8));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + w1 - 34);
        TextC(fSmall, C::dim, "Scores feed the Mind system of the health index and the autonomous advisor (stress, low mood, isolation trends).");
        ImGui::PopTextWrapPos();
    }
    EndCard();

    // ---------- PVT
    ImGui::SameLine(0, gap);
    if (BeginCard("pvt", ImVec2(w2, size.y), C::cyan)) {
        SectionTitle("REACTION TEST (PVT)", C::text);
        TextC(fSmall, C::dim, "Psychomotor vigilance: tap the instant the circle turns green. %d trials.", TRIALS);
        ImGui::Dummy(ImVec2(1, 8));
        double now = ImGui::GetTime();
        if (pvt.st == P_WAIT && now >= pvt.tGo) { pvt.st = P_GO; pvt.tStart = now; }
        float R = std::min((w2 - 34) / 2 - 10, 120.f);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 c = p + ImVec2((w2 - 32) / 2, R + 6);
        ImGui::SetCursorScreenPos(c - ImVec2(R, R));
        bool tapped = ImGui::InvisibleButton("##pvt", ImVec2(R * 2, R * 2));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 col = pvt.st == P_GO ? C::green : (pvt.st == P_EARLY ? C::red : (pvt.st == P_WAIT ? C::amber : C::cyan));
        float pulse = 0.5f + 0.5f * std::sin(a.time * 3.f);
        for (int g = 0; g < 4; ++g) dl->AddCircle(c, R + g * 5 + (pvt.st == P_IDLE ? pulse * 4 : 0), C::alpha(col, 0.12f - g * 0.025f), 64, 3.f);
        dl->AddCircleFilled(c, R, C::alpha(col, pvt.st == P_GO ? 0.55f : 0.14f), 64);
        dl->AddCircle(c, R, col, 64, 2.5f);
        const char* msg = pvt.st == P_IDLE ? "START" : pvt.st == P_WAIT ? "WAIT..." : pvt.st == P_GO ? "TAP!" : pvt.st == P_EARLY ? "TOO EARLY" : "DONE";
        ImGui::PushFont(fTitle);
        ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText(c - ts * 0.5f, C::text, msg);
        ImGui::PopFont();
        if (tapped) {
            if (pvt.st == P_IDLE || pvt.st == P_DONE) { pvt = Pvt(); pvt.st = P_WAIT; pvt.tGo = now + 1.5 + (std::rand() % 2500) / 1000.0; }
            else if (pvt.st == P_WAIT) { pvt.st = P_EARLY; }
            else if (pvt.st == P_EARLY) { pvt.st = P_WAIT; pvt.tGo = now + 1.5 + (std::rand() % 2500) / 1000.0; }
            else if (pvt.st == P_GO) {
                pvt.last = (now - pvt.tStart) * 1000.0;
                pvt.rts.push_back(pvt.last);
                if ((int)pvt.rts.size() >= TRIALS) pvt.st = P_DONE;
                else { pvt.st = P_WAIT; pvt.tGo = now + 1.5 + (std::rand() % 2500) / 1000.0; }
            }
        }
        ImGui::SetCursorScreenPos(p + ImVec2(0, R * 2 + 24));
        if (!pvt.rts.empty()) {
            double sum = 0; for (double r : pvt.rts) sum += r;
            double avg = sum / pvt.rts.size();
            for (size_t i = 0; i < pvt.rts.size(); ++i) {
                TextC(fBody, C::dim, "Trial %zu", i + 1);
                ImGui::SameLine(w2 - 110);
                TextC(fBold, pvtColor(pvt.rts[i]), "%.0f ms", pvt.rts[i]);
            }
            ImGui::Dummy(ImVec2(1, 4));
            TextC(fBold, C::text, "Average");
            ImGui::SameLine(w2 - 110);
            TextC(fBold, pvtColor(avg), "%.0f ms", avg);
            if (pvt.st == P_DONE) {
                ImGui::Dummy(ImVec2(1, 6));
                if (NeonButton("SAVE RESULT", ImVec2(w2 - 34, 40), C::green)) {
                    a.api->post("/api/crew/" + std::to_string(crew.id) + "/mind",
                                {{"mood", v[0]}, {"stress", v[1]}, {"anxiety", v[2]}, {"fatigue", v[3]}, {"isolation", v[4]}, {"sleep", v[5]}, {"pvt", avg}});
                    a.refreshAll();
                    a.notify("PVT saved: " + std::to_string((int)avg) + " ms");
                    pvt = Pvt();
                }
            }
        } else {
            TextC(fSmall, C::dim, "Healthy range 210-260 ms. Over 350 ms is a hazard for EVA.");
        }
    }
    EndCard();

    // ---------- trends
    ImGui::SameLine(0, gap);
    if (BeginCard("trend", ImVec2(w3, size.y), C::blue)) {
        SectionTitle("TRENDS / LAST 30 DAYS", C::text);
        const UMind* last = a.mind.empty() ? nullptr : &a.mind.back();
        TextC(fSmall, C::dim, "Mind score now: %s", last ? std::to_string((int)std::round(last->score)).c_str() : "-");
        ImGui::Dummy(ImVec2(1, 4));
        const char* sn[5] = {"Mood", "Stress", "Anxiety", "Fatigue", "Isolation"};
        ImU32 sc[5] = {C::green, C::red, C::amber, C::purple, C::blue};
        for (int i = 0; i < 5; ++i) { if (Pill(sn[i], show[i], sc[i])) show[i] = !show[i]; ImGui::SameLine(0, 6); }
        ImGui::NewLine();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float ch = (size.y - 190) * 0.62f;
        lineChart(p, ImVec2(w3 - 34, ch), a.mind, show);
        ImGui::Dummy(ImVec2(1, ch + 10));
        TextC(fBold, C::text, "PVT reaction time (ms)");
        ImVec2 q = ImGui::GetCursorScreenPos();
        float ph = size.y - 190 - ch + 10;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(q, q + ImVec2(w3 - 34, ph), IM_COL32(9, 14, 28, 255), 10);
        std::vector<const UMind*> pv;
        for (auto& m : a.mind) if (m.pvt > 0) pv.push_back(&m);
        size_t n = std::min<size_t>(pv.size(), 14), from = pv.size() - n;
        float bw = (w3 - 34 - 24) / 14.f;
        for (size_t i = 0; i < n; ++i) {
            double r = pv[from + i]->pvt;
            float h = (float)std::min(1.0, (r - 180) / 220.0) * (ph - 24);
            ImVec2 b0(q.x + 12 + i * bw, q.y + ph - 10 - h), b1(q.x + 12 + i * bw + bw - 5, q.y + ph - 10);
            dl->AddRectFilled(b0, b1, C::alpha(pvtColor(r), 0.85f), 3);
            if (ImGui::IsMouseHoveringRect(b0, b1)) ImGui::SetTooltip("day %d: %.0f ms", pv[from + i]->day, r);
        }
        ImGui::Dummy(ImVec2(1, ph));
    }
    EndCard();
    (void)o;
}

}  // namespace ui
