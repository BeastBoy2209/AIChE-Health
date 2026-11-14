#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include "widgets.h"

namespace ui {

static float textH(const std::string& s, float w, ImFont* f) {
    ImGui::PushFont(f);
    float h = ImGui::CalcTextSize(s.c_str(), nullptr, false, w).y;
    ImGui::PopFont();
    return h;
}

void drawAdvisor(App& a, ImVec2 size) {
    static std::map<std::string, bool> done;
    ImVec2 o = ImGui::GetCursorScreenPos();
    TextC(fTitle, C::text, "AUTONOMOUS MEDICAL ADVISOR");
    TextC(fSmall, C::dim, "%s  /  on-board rule engine, 14 clinical rules x 73 biomarkers + mind + pain. No Earth link required.", a.cur().name.c_str());
    ImGui::SetCursorScreenPos(o + ImVec2(0, 62));
    ImGui::BeginChild("advlist", ImVec2(size.x, size.y - 62), 0, 0);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float W = size.x - 14, gap = 14;
    int ci = 0;
    for (auto& ad : a.advice) {
        ImU32 sc = sevColor(ad.sev);
        float tw = W - 60;
        float hTitle = textH(ad.title, tw - 220, fBold);
        float hh = 22 + std::max(hTitle, 24.f) + 14;
        if (!ad.why.empty()) { hh += 24; for (auto& w : ad.why) hh += textH(w, tw, fBody) + 4; }
        hh += 14 + 24;
        for (auto& x : ad.actions) hh += std::max(textH(x, tw - 34, fBody), 22.f) + 6;
        hh += 14;
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, p + ImVec2(W, hh), C::panel, 14);
        dl->AddRectFilledMultiColor(p + ImVec2(1, 1), p + ImVec2(W - 1, 64), C::alpha(sc, 0.10f), C::alpha(sc, 0.10f), 0, 0);
        dl->AddRect(p, p + ImVec2(W, hh), C::alpha(sc, 0.55f), 14);
        dl->AddRectFilled(p + ImVec2(0, 18), p + ImVec2(4, hh - 18), sc, 2);
        float y = 18;
        ImGui::PushFont(fBold);
        dl->AddText(nullptr, 0.f, p + ImVec2(24, y), C::text, ad.title.c_str(), nullptr, tw - 220);
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(p + ImVec2(W - 200, 16));
        float cw = Chip(ad.system.c_str(), C::dim);
        ImGui::SetCursorScreenPos(p + ImVec2(W - 200 + cw + 8, 16));
        Chip(ad.sev == 2 ? "CRITICAL" : (ad.sev == 1 ? "WATCH" : "OK"), sc, ad.sev == 2);
        y += std::max(hTitle, 24.f) + 14;
        if (!ad.why.empty()) {
            ImGui::PushFont(fSmall);
            dl->AddText(p + ImVec2(24, y), C::dim, "EVIDENCE");
            ImGui::PopFont();
            y += 24;
            for (auto& w : ad.why) {
                dl->AddCircleFilled(p + ImVec2(30, y + 10), 3, sc);
                ImGui::PushFont(fBody);
                dl->AddText(nullptr, 0.f, p + ImVec2(44, y), C::text, w.c_str(), nullptr, tw - 20);
                ImGui::PopFont();
                y += textH(w, tw, fBody) + 4;
            }
        }
        y += 14;
        ImGui::PushFont(fSmall);
        dl->AddText(p + ImVec2(24, y), C::cyan, "RECOMMENDED ACTIONS  (tick when done)");
        ImGui::PopFont();
        y += 24;
        int ai = 0;
        for (auto& x : ad.actions) {
            std::string key = a.cur().name + ad.title + std::to_string(ai);
            bool& d = done[key];
            ImGui::SetCursorScreenPos(p + ImVec2(22, y));
            char id[32]; snprintf(id, sizeof id, "##ck%d_%d", ci, ai);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
            ImGui::PushStyleColor(ImGuiCol_Border, C::alpha(C::cyan, 0.6f));
            ImGui::Checkbox(id, &d);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            ImGui::PushFont(fBody);
            dl->AddText(nullptr, 0.f, p + ImVec2(54, y + 1), d ? C::faint : C::text, x.c_str(), nullptr, tw - 34);
            ImGui::PopFont();
            y += std::max(textH(x, tw - 34, fBody), 22.f) + 6;
            ++ai;
        }
        ImGui::SetCursorScreenPos(p + ImVec2(0, hh + gap));
        ++ci;
    }
    ImGui::Dummy(ImVec2(1, 10));
    ImGui::EndChild();
}

void drawReport(App& a, ImVec2 size) {
    static std::vector<std::string> queue;
    ImVec2 o = ImGui::GetCursorScreenPos();
    TextC(fTitle, C::text, "MISSION REPORT");
    TextC(fSmall, C::dim, "Compact summary stored on board. Send to Earth at the next comm window.");
    ImGui::SetCursorScreenPos(o + ImVec2(0, 62));
    float gap = 16, w2 = 330, w1 = size.x - w2 - gap, H = size.y - 62;
    if (BeginCard("rep", ImVec2(w1, H), C::cyan)) {
        ImGui::BeginChild("reptext", ImVec2(0, 0), 0, 0);
        ImGui::PushFont(fMono);
        ImGui::PushStyleColor(ImGuiCol_Text, C::text);
        ImGui::TextUnformatted(a.report.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::EndChild();
    }
    EndCard();
    ImGui::SameLine(0, gap);
    if (BeginCard("repact", ImVec2(w2, H), C::green)) {
        SectionTitle("DOWNLINK QUEUE");
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + w2 - 34);
        TextC(fSmall, C::dim, "Reports are written as text files and wait here until communications return.");
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(1, 8));
        auto exportOne = [&](int idx) {
            const UCrew& c = a.crew[idx];
            std::string txt = a.api->text("/api/crew/" + std::to_string(c.id) + "/report");
            std::string n = c.name;
            for (auto& ch : n) if (ch == ' ') ch = '_';
            std::string fn = "report_" + n + "_day" + std::to_string(a.day) + ".txt";
            std::ofstream f(fn, std::ios::binary);
            f << txt;
            queue.push_back(fn);
        };
        if (NeonButton("EXPORT THIS CREW MEMBER", ImVec2(w2 - 34, 44), C::green)) { exportOne(a.sel); a.notify("Report exported: " + queue.back()); }
        if (NeonButton("EXPORT ALL CREW", ImVec2(w2 - 34, 44), C::cyan)) {
            for (size_t i = 0; i < a.crew.size(); ++i) exportOne((int)i);
            a.notify("Exported reports for " + std::to_string(a.crew.size()) + " crew members");
        }
        if (NeonButton("COPY TO CLIPBOARD", ImVec2(w2 - 34, 44), C::purple)) { ImGui::SetClipboardText(a.report.c_str()); a.notify("Copied to clipboard"); }
        ImGui::Dummy(ImVec2(1, 10));
        TextC(fBold, C::text, "Queued files (%zu)", queue.size());
        ImGui::BeginChild("q", ImVec2(0, 0), 0, 0);
        for (auto it = queue.rbegin(); it != queue.rend(); ++it) TextC(fSmall, C::dim, "%s", it->c_str());
        ImGui::EndChild();
    }
    EndCard();
}

}  // namespace ui
