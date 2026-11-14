#include <algorithm>
#include <cmath>
#include <cstdio>
#include "widgets.h"

namespace ui {

static void historyChart(ImVec2 p, ImVec2 sz, const UMarker& m, int sex, const std::vector<std::pair<int, double>>& h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + sz, IM_COL32(9, 14, 28, 255), 10);
    double lo = m.lo, hi = m.hi, span = std::max(hi - lo, 1e-9);
    auto Y = [&](double v) { double t = (std::max(lo, std::min(hi, v)) - lo) / span; return p.y + sz.y - 14 - (float)t * (sz.y - 28); };
    auto band = [&](int tier, ImU32 col) {
        for (auto& iv : m.tier[tier][sex]) {
            double a = iv.first <= -1e8 ? lo : iv.first, b = iv.second >= 1e8 ? hi : iv.second;
            float y1 = Y(a), y0 = Y(b);
            if (y1 - y0 < 2) y0 = y1 - 2;
            dl->AddRectFilled(ImVec2(p.x + 1, y0), ImVec2(p.x + sz.x - 1, y1), col);
        }
    };
    band(2, C::alpha(C::amber, 0.16f));
    band(3, C::alpha(C::red, 0.20f));
    band(1, C::alpha(C::green, 0.20f));
    if (h.size() < 2) return;
    int d0 = h.front().first, d1 = h.back().first;
    double dspan = std::max(1, d1 - d0);
    std::vector<ImVec2> pts;
    for (auto& s : h) pts.push_back(ImVec2(p.x + 20 + (float)((s.first - d0) / dspan) * (sz.x - 40), Y(m.abs ? std::fabs(s.second) : s.second)));
    dl->AddPolyline(pts.data(), (int)pts.size(), C::alpha(C::cyan, 0.25f), 0, 6.f);
    dl->AddPolyline(pts.data(), (int)pts.size(), C::cyan, 0, 2.2f);
    for (size_t i = 0; i < pts.size(); ++i) {
        dl->AddCircleFilled(pts[i], i + 1 == pts.size() ? 5.5f : 3.f, i + 1 == pts.size() ? C::text : C::cyan);
        if (ImGui::IsMouseHoveringRect(pts[i] - ImVec2(6, 6), pts[i] + ImVec2(6, 6)))
            ImGui::SetTooltip("day %d: %s %s", h[i].first, Fmt(h[i].second).c_str(), m.unit.c_str());
    }
    ImGui::PushFont(fSmall);
    char b[32]; snprintf(b, sizeof b, "day %d", d0); dl->AddText(p + ImVec2(8, sz.y - 18), C::faint, b);
    snprintf(b, sizeof b, "day %d", d1); ImVec2 ts = ImGui::CalcTextSize(b); dl->AddText(p + ImVec2(sz.x - ts.x - 8, sz.y - 18), C::faint, b);
    ImGui::PopFont();
}

static int effSev(const App& a, const ULab& l) { return l.sev; (void)a; }

void drawVitals(App& a, ImVec2 size) {
    static int cluster = 0, selM = -1;
    static bool onboardOnly = false;
    static double inputVal = 0; static int inputFor = -1;
    const UCrew& crew = a.cur();
    ImVec2 o = ImGui::GetCursorScreenPos();

    // summary + filters
    int n0 = 0, n1 = 0, n2 = 0, falseAlarms = 0;
    for (auto& l : a.labs) { (l.sev == 0 ? n0 : l.sev == 1 ? n1 : n2)++; if (l.earthAb && l.sev == 0) falseAlarms++; }
    ImGui::SetCursorScreenPos(o);
    TextC(fTitle, C::text, "VITALS & LABS");
    ImGui::SameLine(0, 18);
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, o.y + 2));
    Chip(("OK " + std::to_string(n0)).c_str(), C::green);
    ImGui::SameLine(); ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, o.y + 2));
    Chip(("WATCH " + std::to_string(n1)).c_str(), C::amber);
    ImGui::SameLine(); ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, o.y + 2));
    Chip(("DANGER " + std::to_string(n2)).c_str(), C::red);
    ImGui::SameLine(); ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, o.y + 2));
    Chip((std::to_string(falseAlarms) + " values look abnormal by Earth norms but are normal in space").c_str(), C::amber);
    ImGui::SetCursorScreenPos(o + ImVec2(0, 44));
    const char* cl[6] = {"All systems", "Cardio & Blood", "Musculoskeletal", "Neuro-sensory", "Metabolic & Renal", "Immune & Endocrine"};
    for (int i = 0; i < 6; ++i) {
        if (Pill(cl[i], cluster == i)) cluster = i;
        ImGui::SameLine(0, 8);
    }
    if (Pill(onboardOnly ? "Onboard-capable only" : "Show all analyses", onboardOnly, C::green)) onboardOnly = !onboardOnly;

    float top = 92, gap = 16, listW = size.x * 0.64f, detW = size.x - listW - gap, H = size.y - top;
    // build row list
    std::vector<const ULab*> rows;
    for (auto& l : a.labs) {
        const UMarker& m = a.markers[l.mi];
        if (cluster && m.cluster != cluster) continue;
        if (onboardOnly && m.avail == "missing") continue;
        rows.push_back(&l);
    }
    std::stable_sort(rows.begin(), rows.end(), [&](const ULab* x, const ULab* y) { return effSev(a, *x) > effSev(a, *y); });
    if (selM < 0 || !a.labFor(selM)) selM = rows.empty() ? -1 : rows[0]->mi;
    bool selVisible = false;
    for (auto r : rows) if (r->mi == selM) selVisible = true;
    if (!selVisible && !rows.empty()) selM = rows[0]->mi;

    ImGui::SetCursorScreenPos(o + ImVec2(0, top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("list", ImVec2(listW, H), 0, 0);
    ImGui::PopStyleVar();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < rows.size(); ++i) {
        const ULab& l = *rows[i];
        const UMarker& m = a.markers[l.mi];
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz(listW - 14, 78);
        ImGui::PushID((int)i);
        bool clicked = ImGui::InvisibleButton("row", sz);
        bool hov = ImGui::IsItemHovered(), act = (l.mi == selM);
        ImGui::PopID();
        if (clicked) selM = l.mi;
        ImU32 sc = sevColor(l.sev);
        bool falseAlarm = a.earthNorms && l.earthAb && l.sev == 0;
        dl->AddRectFilled(p, p + sz, act ? C::alpha(sc, 0.12f) : (hov ? C::alpha(C::panel2, 0.95f) : C::panel), 12);
        dl->AddRect(p, p + sz, act ? C::alpha(sc, 0.85f) : C::border, 12, 0, act ? 1.6f : 1.f);
        dl->AddRectFilled(p + ImVec2(0, 14), p + ImVec2(4, sz.y - 14), falseAlarm ? C::amber : sc, 2);
        ImGui::PushFont(fBold);
        dl->AddText(nullptr, 0.f, p + ImVec2(16, 8), C::text, m.name.c_str(), nullptr, listW * 0.36f);
        ImGui::PopFont();
        ImGui::PushFont(fSmall);
        dl->AddText(nullptr, 0.f, p + ImVec2(16, 34), C::dim, m.group.c_str(), nullptr, listW * 0.36f);
        const char* av = m.avail == "onboard" ? "ONBOARD" : (m.avail == "partial" ? "LIMITED" : "NEEDS KIT");
        ImU32 ac = m.avail == "onboard" ? C::green : (m.avail == "partial" ? C::amber : C::faint);
        ImGui::SetCursorScreenPos(p + ImVec2(16, 53));
        ImGui::PopFont();
        Chip(av, ac);
        // value
        float vx = p.x + listW * 0.40f;
        ImGui::PushFont(fNum);
        std::string vs = Fmt(l.value);
        dl->AddText(ImVec2(vx, p.y + 8), C::text, vs.c_str());
        ImGui::PopFont();
        ImGui::PushFont(fSmall);
        dl->AddText(ImVec2(vx, p.y + 42), C::dim, m.unit.c_str());
        ImGui::PopFont();
        // gauge
        float gx = vx + 120, gw = listW * 0.22f;
        RangeGauge(ImVec2(gx, p.y + 30), gw, 12, m, crew.sex, l.value, l.sev);
        // sparkline
        std::vector<float> sv;
        for (auto& h : l.hist) sv.push_back((float)(m.abs ? std::fabs(h.second) : h.second));
        Sparkline(ImVec2(gx + gw + 18, p.y + 38), ImVec2(listW * 0.10f, 28), sv, sc, (float)m.lo, (float)m.hi);
        // status chip
        ImGui::SetCursorScreenPos(ImVec2(p.x + sz.x - 108, p.y + 12));
        if (falseAlarm) Chip("EARTH FLAG", C::amber);
        else Chip(l.sev == 0 ? "NORMAL" : (l.sev == 1 ? "CONCERNING" : "DANGEROUS"), sc, l.sev == 2);
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + sz.y + 8));
    }
    if (rows.empty()) TextC(fBody, C::dim, "No analyses for this filter.");
    ImGui::EndChild();

    // detail panel
    ImGui::SetCursorScreenPos(o + ImVec2(listW + gap, top));
    const ULab* sl = selM >= 0 ? a.labFor(selM) : nullptr;
    if (BeginCard("detail", ImVec2(detW, H), sl ? sevColor(sl->sev) : C::cyan) && sl) {
        const UMarker& m = a.markers[sl->mi];
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + detW - 34);
        TextC(fBold, C::text, "%s", m.name.c_str());
        ImGui::PopTextWrapPos();
        TextC(fSmall, C::dim, "%s", m.group.c_str());
        ImGui::Dummy(ImVec2(1, 2));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushFont(fBig);
        std::string vs = Fmt(sl->value);
        ImGui::GetWindowDrawList()->AddText(p, sevColor(sl->sev), vs.c_str());
        ImVec2 ts = ImGui::CalcTextSize(vs.c_str());
        ImGui::PopFont();
        ImGui::GetWindowDrawList()->AddText(p + ImVec2(ts.x + 10, 22), C::dim, m.unit.c_str());
        ImGui::Dummy(ImVec2(1, ts.y + 4));
        historyChart(ImGui::GetCursorScreenPos(), ImVec2(detW - 34, 150), m, crew.sex, sl->hist);
        ImGui::Dummy(ImVec2(1, 158));
        struct Row { const char* n; int t; ImU32 c; } tr[4] = {{"Earth normal", 0, C::faint}, {"Space normal", 1, C::green}, {"Concerning", 2, C::amber}, {"Dangerous", 3, C::red}};
        for (auto& r : tr) {
            ImVec2 q = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(q + ImVec2(5, 10), 4.5f, r.c);
            ImGui::SetCursorScreenPos(q + ImVec2(16, 0));
            TextC(fSmall, r.c == C::faint ? C::dim : r.c, "%s", r.n);
            ImGui::SetCursorScreenPos(q + ImVec2(110, 0));
            ImGui::PushFont(fSmall);
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + detW - 150);
            ImGui::TextUnformatted(m.text[r.t].c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 4));
        }
        ImGui::Dummy(ImVec2(1, 4));
        TextC(fSmall, C::dim, "Device: %s", m.apparatus.c_str());
        TextC(fSmall, C::dim, "Cadence: %s", m.freq.c_str());
        if (sl->earthAb && sl->sev == 0)
            TextC(fSmall, C::amber, "Outside Earth range but inside space adaptation range.");
        ImGui::Dummy(ImVec2(1, 6));
        // record measurement
        if (inputFor != selM) { inputFor = selM; inputVal = sl->value; }
        ImGui::PushFont(fBody);
        ImGui::SetNextItemWidth(detW - 190);
        ImGui::InputDouble("##val", &inputVal, 0, 0, "%.3f");
        ImGui::PopFont();
        ImGui::SameLine();
        if (NeonButton("RECORD", ImVec2(130, 0), C::cyan)) {
            auto r = a.api->post("/api/crew/" + std::to_string(crew.id) + "/labs", {{"bio", m.id}, {"value", inputVal}});
            a.refreshAll();
            a.notify(r.is_null() ? "Could not store measurement" : (std::string("Stored. Classified as ") + (r["sev"] == 0 ? "NORMAL" : r["sev"] == 1 ? "CONCERNING" : "DANGEROUS")));
        }
    }
    EndCard();
}

}  // namespace ui
