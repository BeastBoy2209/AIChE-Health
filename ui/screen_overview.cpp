#include <cmath>
#include <cstdio>
#include "widgets.h"

namespace ui {

static void crewCard(App& a, int i, ImVec2 p, ImVec2 sz) {
    const UCrew& c = a.crew[i];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(p);
    char id[24]; snprintf(id, sizeof id, "##cc%d", i);
    bool clicked = ImGui::InvisibleButton(id, sz);
    bool hov = ImGui::IsItemHovered(), sel = (i == a.sel);
    float k = Anim(id, hov ? 1.f : 0.f);
    ImU32 sc = sevColor(c.status);
    float pulse = c.status == 2 ? 0.5f + 0.5f * std::sin(a.time * 4.f) : 0.f;
    dl->AddRectFilled(p, p + sz, C::panel, 14);
    dl->AddRectFilledMultiColor(p + ImVec2(1, 1), p + ImVec2(sz.x - 1, 70), C::alpha(sc, 0.10f), C::alpha(sc, 0.10f), 0, 0);
    if (c.status == 2) dl->AddRect(p - ImVec2(2, 2), p + sz + ImVec2(2, 2), C::alpha(C::red, 0.25f + 0.3f * pulse), 16, 0, 3.f);
    dl->AddRect(p, p + sz, sel ? C::alpha(sc, 0.9f) : C::alpha(C::border, 0.9f + 0.1f * k), 14, 0, sel ? 1.8f : 1.f);
    // ring
    ImVec2 rc = p + ImVec2(66, 88);
    float v = Anim(id, 0, 0) ;  // keep storage touched
    (void)v;
    char rid[32]; snprintf(rid, sizeof rid, "ring%d", i);
    float val = Anim(rid, (float)c.index / 100.f, 3.f);
    HealthRing(rc, 44, val, sc, 9.f);
    char num[8]; snprintf(num, sizeof num, "%d", (int)std::round(val * 100));
    ImGui::PushFont(fNum);
    ImVec2 ts = ImGui::CalcTextSize(num);
    dl->AddText(rc - ts * 0.5f - ImVec2(0, 4), C::text, num);
    ImGui::PopFont();
    ImGui::PushFont(fSmall);
    ImVec2 ls = ImGui::CalcTextSize("INDEX");
    dl->AddText(rc + ImVec2(-ls.x / 2, 18), C::dim, "INDEX");
    ImGui::PopFont();
    // texts
    ImGui::PushFont(fBold);
    dl->AddText(p + ImVec2(130, 18), C::text, c.name.c_str());
    ImGui::PopFont();
    ImGui::PushFont(fSmall);
    char sub[96]; snprintf(sub, sizeof sub, "%s  /  %s, %d y", c.role.c_str(), c.sex ? "F" : "M", c.age);
    dl->AddText(p + ImVec2(130, 44), C::dim, sub);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(p + ImVec2(130, 66));
    if (c.hasData) Chip(sevLabel(c.status), sc, c.status == 2);
    else Chip("NO DATA YET", C::faint);
    ImGui::PushFont(fSmall);
    char al[96]; snprintf(al, sizeof al, "%d alerts  /  %d dangerous  /  %d watch", c.alerts, c.danger, c.concern);
    dl->AddText(p + ImVec2(130, 96), c.alerts ? C::text : C::dim, al);
    char mo[96];
    if (!c.moca.has) snprintf(mo, sizeof mo, "MoCA: not tested - REQUIRED");
    else if (c.moca.overdue) snprintf(mo, sizeof mo, "MoCA %d/30: OVERDUE by %d d", c.moca.lastTotal, -c.moca.dueIn);
    else snprintf(mo, sizeof mo, "MoCA %d/30: next in %d d", c.moca.lastTotal, c.moca.dueIn);
    dl->AddText(p + ImVec2(130, 113), c.moca.overdue ? C::amber : C::dim, mo);
    ImGui::PopFont();
    // 7 system bars
    float bx = p.x + 130, by = p.y + sz.y - 12, bw = (sz.x - 130 - 22) / 7.f;
    for (int s = 0; s < 7; ++s) {
        float h = (float)c.systems[s] / 100.f * 24.f;
        ImU32 col = c.systems[s] < 50 ? C::red : (c.systems[s] < 78 ? C::amber : C::green);
        ImVec2 b0(bx + s * bw, by - 24), b1(bx + s * bw + bw - 6, by);
        dl->AddRectFilled(b0, b1, C::alpha(C::faint, 0.3f), 3);
        dl->AddRectFilled(ImVec2(b0.x, by - h), b1, C::alpha(col, 0.9f), 3);
        if (ImGui::IsMouseHoveringRect(b0, b1) && hov) {
            char t[96]; snprintf(t, sizeof t, "%s: %d", a.sysNames.size() > (size_t)s ? a.sysNames[s].c_str() : "?", (int)std::round(c.systems[s]));
            ImGui::SetTooltip("%s", t);
        }
    }
    if (clicked) a.selectCrew(i);
}

void drawOverview(App& a, ImVec2 size) {
    ImVec2 o = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float y = 0;
    // critical banner
    int crit = -1;
    for (size_t i = 0; i < a.crew.size(); ++i) if (a.crew[i].status == 2) { crit = (int)i; break; }
    if (crit >= 0) {
        float pulse = 0.5f + 0.5f * std::sin(a.time * 4.f);
        ImVec2 p = o, sz(size.x, 54);
        ImGui::SetCursorScreenPos(p);
        bool clicked = ImGui::InvisibleButton("##banner", sz);
        dl->AddRectFilled(p, p + sz, C::alpha(C::red, 0.12f + 0.06f * pulse), 12);
        dl->AddRect(p, p + sz, C::alpha(C::red, 0.6f + 0.3f * pulse), 12, 0, 1.6f);
        ImVec2 tri[3] = {p + ImVec2(34, 12), p + ImVec2(50, 42), p + ImVec2(18, 42)};
        dl->AddTriangleFilled(tri[0], tri[1], tri[2], C::red);
        dl->AddLine(p + ImVec2(34, 22), p + ImVec2(34, 32), C::bg0, 2.2f);
        dl->AddCircleFilled(p + ImVec2(34, 37), 1.6f, C::bg0);
        ImGui::PushFont(fBold);
        char t[200]; snprintf(t, sizeof t, "CRITICAL: %s needs medical attention", a.crew[crit].name.c_str());
        dl->AddText(p + ImVec2(66, 8), C::text, t);
        ImGui::PopFont();
        ImGui::PushFont(fSmall);
        dl->AddText(p + ImVec2(66, 33), C::dim, "Autonomous advisor has prepared a response protocol. Click to open the Advisor.");
        ImGui::PopFont();
        if (clicked) { a.selectCrew(crit); a.screen = S_ADVISOR; }
        y += 54 + 16;
    }
    // MoCA reminder banner (weekly test is mandatory)
    {
        int n = a.overdueCount();
        if (n > 0) {
            ImVec2 p = o + ImVec2(0, y), sz(size.x, 44);
            ImGui::SetCursorScreenPos(p);
            bool clicked = ImGui::InvisibleButton("##mocabanner", sz);
            dl->AddRectFilled(p, p + sz, C::alpha(C::amber, ImGui::IsItemHovered() ? 0.2f : 0.12f), 12);
            dl->AddRect(p, p + sz, C::alpha(C::amber, 0.7f), 12, 0, 1.4f);
            std::string names;
            int first = -1;
            for (size_t i = 0; i < a.crew.size(); ++i) if (a.crew[i].moca.overdue) { if (first < 0) first = (int)i; if (!names.empty()) names += ", "; names += a.crew[i].name; }
            ImGui::PushFont(fBold);
            std::string t = "WEEKLY MoCA DUE: " + names;
            dl->AddText(nullptr, 0.f, p + ImVec2(18, 10), C::text, t.c_str(), nullptr, size.x - 220);
            ImGui::PopFont();
            ImGui::PushFont(fSmall);
            dl->AddText(p + ImVec2(size.x - 170, 14), C::amber, "Click to start the test");
            ImGui::PopFont();
            if (clicked) { a.selectCrew(first); a.screen = S_MOCA; }
            y += 44 + 14;
        }
    }
    // crew cards (up to two rows of four)
    float gap = 16, cw = (size.x - gap * 3) / 4.f;
    int shown = std::min<int>((int)a.crew.size(), 8);
    int rows = (shown + 3) / 4;
    float ch = rows > 1 ? 172.f : 196.f;
    for (int i = 0; i < shown; ++i) crewCard(a, i, o + ImVec2((i % 4) * (cw + gap), y + (i / 4) * (ch + gap)), ImVec2(cw, ch));
    y += rows * ch + (rows - 1) * gap + 16;
    // lower row
    float rh = size.y - y;
    float w1 = size.x * 0.34f, w3 = size.x * 0.30f, w2 = size.x - w1 - w3 - gap * 2;
    ImGui::SetCursorScreenPos(o + ImVec2(0, y));
    if (BeginCard("radar", ImVec2(w1, rh), C::cyan)) {
        const UCrew& c = a.cur();
        SectionTitle("SYSTEM BALANCE");
        TextC(fSmall, C::dim, "%s  /  %s", c.name.c_str(), c.role.c_str());
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float r = std::min((w1 - 270) / 2, (rh - 150) / 2);
        Radar(cp + ImVec2((w1 - 32) / 2, r + 36), r, c.systems, 7, a.sysNames, C::cyan);
        ImGui::Dummy(ImVec2(1, r * 2 + 60));
        TextC(fSmall, C::dim, "Each axis = average health of that system's latest samples (100 = nominal).");
    }
    EndCard();
    ImGui::SameLine(0, gap);
    if (BeginCard("alerts", ImVec2(w2, rh), C::amber)) {
        SectionTitle("PRIORITY ALERTS");
        TextC(fSmall, C::dim, "Offline advisor / %s", a.cur().name.c_str());
        ImGui::Dummy(ImVec2(1, 4));
        int shown = 0;
        for (auto& ad : a.advice) {
            if (shown >= 4) break;
            ImVec2 p = ImGui::GetCursorScreenPos();
            float cwid = w2 - 34;
            ImGui::PushTextWrapPos(p.x + cwid - 16);
            ImGui::PushFont(fBold);
            ImVec2 t1 = ImGui::CalcTextSize(ad.title.c_str(), nullptr, false, cwid - 30);
            ImGui::PopFont();
            ImGui::PopTextWrapPos();
            float hh = t1.y + 52;
            ImGui::InvisibleButton(("##al" + std::to_string(shown)).c_str(), ImVec2(cwid, hh));
            if (ImGui::IsItemClicked()) a.screen = S_ADVISOR;
            ImDrawList* d2 = ImGui::GetWindowDrawList();
            ImU32 sc = sevColor(ad.sev);
            d2->AddRectFilled(p, p + ImVec2(cwid, hh), C::alpha(sc, ImGui::IsItemHovered() ? 0.14f : 0.07f), 10);
            d2->AddRectFilled(p, p + ImVec2(4, hh), sc, 2);
            ImGui::PushFont(fBold);
            d2->AddText(nullptr, 0.f, p + ImVec2(14, 6), C::text, ad.title.c_str(), nullptr, cwid - 30);
            ImGui::PopFont();
            ImGui::PushFont(fSmall);
            std::string w = ad.why.empty() ? "" : ad.why[0];
            d2->AddText(nullptr, 0.f, p + ImVec2(14, t1.y + 12), C::dim, w.c_str(), nullptr, cwid - 30);
            d2->AddText(p + ImVec2(14, t1.y + 30), sc, ad.system.c_str());
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 2));
            ++shown;
        }
        // out-of-range biomarkers (compact list)
        ImGui::Dummy(ImVec2(1, 8));
        TextC(fBold, C::text, "OUT OF SPACE-NORMAL RANGE");
        int listed = 0;
        for (int sv = 2; sv >= 1; --sv)
            for (auto& l : a.labs) {
                if (l.sev != sv || listed >= 7) continue;
                const UMarker& m = a.markers[l.mi];
                ImVec2 q = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddCircleFilled(q + ImVec2(5, 10), 4, sevColor(sv));
                ImGui::SetCursorScreenPos(q + ImVec2(16, 0));
                TextC(fSmall, C::text, "%s", m.name.c_str());
                ImGui::SameLine(w2 - 150);
                TextC(fSmall, sevColor(sv), "%s %s", Fmt(l.value).c_str(), m.unit.c_str());
                ++listed;
            }
        if (!listed) TextC(fSmall, C::green, "All biomarkers within range.");
    }
    EndCard();
    ImGui::SameLine(0, gap);
    if (BeginCard("log", ImVec2(w3, rh), C::blue)) {
        SectionTitle("MISSION LOG");
        TextC(fSmall, C::dim, "Stored locally, ready for next downlink");
        ImGui::Dummy(ImVec2(1, 4));
        ImGui::BeginChild("feed", ImVec2(0, 0), 0, 0);
        for (auto& e : a.events) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImU32 sc = e.sev >= 2 ? C::red : (e.sev == 1 ? C::amber : C::green);
            ImGui::GetWindowDrawList()->AddCircleFilled(p + ImVec2(5, 9), 3.5f, sc);
            ImGui::SetCursorScreenPos(p + ImVec2(18, 0));
            ImGui::PushFont(fSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, C::dim);
            ImGui::Text("DAY %d  %s", e.day, e.crew.c_str());
            ImGui::PopStyleColor();
            ImGui::SetCursorScreenPos(p + ImVec2(18, 18));
            ImGui::PushStyleColor(ImGuiCol_Text, C::text);
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + w3 - 70);
            ImGui::TextUnformatted(e.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 4));
        }
        ImGui::EndChild();
    }
    EndCard();
}

}  // namespace ui
