#include "widgets.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <algorithm>

namespace ui {

float Anim(const char* id, float target, float speed) {
    ImGuiStorage* s = ImGui::GetStateStorage();
    ImGuiID k = ImGui::GetID(id);
    float cur = s->GetFloat(k, target);
    cur += (target - cur) * std::min(1.f, ImGui::GetIO().DeltaTime * speed);
    s->SetFloat(k, cur);
    return cur;
}

bool BeginCard(const char* id, ImVec2 size, ImU32 accent) {
    ImVec2 p = ImGui::GetCursorScreenPos(), av = ImGui::GetContentRegionAvail();
    if (size.x <= 0) size.x = av.x;
    if (size.y <= 0) size.y = av.y;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + size, C::panel, 12.f);
    dl->AddRect(p, p + size, accent ? C::alpha(accent, 0.5f) : C::border, 12.f, 0, 1.f);
    if (accent) {
        dl->AddRectFilled(p + ImVec2(16, 0), p + ImVec2(64, 2.5f), accent, 2.f);
        dl->AddRectFilledMultiColor(p + ImVec2(1, 1), p + ImVec2(size.x - 1, 60), C::alpha(accent, 0.07f), C::alpha(accent, 0.07f), 0, 0);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    bool r = ImGui::BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    return r;
}
void EndCard() { ImGui::EndChild(); }

void TextC(ImFont* f, ImU32 col, const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    ImGui::PushFont(f);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(buf);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}
void Label(const char* s) { TextC(fSmall, C::dim, "%s", s); }

void SectionTitle(const char* s, ImU32 col) { TextC(fBold, col ? col : C::text, "%s", s); }

std::string Fmt(double v) {
    char b[48];
    double a = std::fabs(v);
    if (a >= 1000) snprintf(b, sizeof b, "%.0f", v);
    else if (a >= 100) snprintf(b, sizeof b, "%.0f", v);
    else if (a >= 10) snprintf(b, sizeof b, "%.1f", v);
    else snprintf(b, sizeof b, "%.2f", v);
    return b;
}

float Chip(const char* txt, ImU32 col, bool filled) {
    ImGui::PushFont(fSmall);
    ImVec2 ts = ImGui::CalcTextSize(txt);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 sz(ts.x + 18, ts.y + 6);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + sz, C::alpha(col, filled ? 0.9f : 0.16f), sz.y / 2);
    if (!filled) dl->AddRect(p, p + sz, C::alpha(col, 0.55f), sz.y / 2);
    dl->AddText(p + ImVec2(9, 3), filled ? C::bg0 : col, txt);
    ImGui::Dummy(sz);
    ImGui::PopFont();
    return sz.x;
}

bool Pill(const char* label, bool active, ImU32 col, float minW) {
    if (!col) col = C::cyan;
    ImGui::PushFont(fBody);
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 sz(std::max(ts.x + 26, minW), ts.y + 12);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, sz);
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + sz, active ? C::alpha(col, 0.25f) : (hov ? C::alpha(col, 0.12f) : C::alpha(C::panel2, 0.8f)), sz.y / 2);
    dl->AddRect(p, p + sz, active ? col : C::border, sz.y / 2, 0, active ? 1.5f : 1.f);
    dl->AddText(p + ImVec2((sz.x - ts.x) / 2, 6), active ? col : (hov ? C::text : C::dim), label);
    ImGui::PopFont();
    return clicked;
}

bool NeonButton(const char* label, ImVec2 size, ImU32 col) {
    if (!col) col = C::cyan;
    ImGui::PushFont(fBold);
    ImVec2 ts = ImGui::CalcTextSize(label);
    if (size.x <= 0) size.x = ts.x + 36;
    if (size.y <= 0) size.y = ts.y + 14;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size);
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hov) dl->AddRect(p - ImVec2(2, 2), p + size + ImVec2(2, 2), C::alpha(col, 0.25f), 12, 0, 3.f);
    dl->AddRectFilled(p, p + size, C::alpha(col, act ? 0.55f : (hov ? 0.38f : 0.22f)), 10.f);
    dl->AddRect(p, p + size, col, 10.f, 0, 1.4f);
    dl->AddText(p + ImVec2((size.x - ts.x) / 2, (size.y - ts.y) / 2), C::text, label);
    ImGui::PopFont();
    return clicked;
}

void HealthRing(ImVec2 c, float r, float v, ImU32 col, float th) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float a0 = IM_PI * 0.75f, full = IM_PI * 1.5f;
    dl->PathArcTo(c, r, a0, a0 + full, 64);
    dl->PathStroke(C::alpha(C::faint, 0.35f), 0, th);
    v = std::max(0.f, std::min(1.f, v));
    if (v > 0.01f) {
        for (int g = 0; g < 3; ++g) {
            dl->PathArcTo(c, r, a0, a0 + full * v, 64);
            dl->PathStroke(C::alpha(col, 0.10f), 0, th + (3 - g) * 3.5f);
        }
        dl->PathArcTo(c, r, a0, a0 + full * v, 64);
        dl->PathStroke(col, 0, th);
    }
}

void Sparkline(ImVec2 p, ImVec2 sz, const std::vector<float>& v, ImU32 col, float lo, float hi) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (v.size() < 2) return;
    if (hi - lo < 1e-9f) hi = lo + 1;
    std::vector<ImVec2> pts;
    for (size_t i = 0; i < v.size(); ++i) {
        float x = p.x + sz.x * i / (v.size() - 1);
        float t = (std::max(lo, std::min(hi, v[i])) - lo) / (hi - lo);
        pts.push_back(ImVec2(x, p.y + sz.y - t * sz.y));
    }
    dl->AddPolyline(pts.data(), (int)pts.size(), C::alpha(col, 0.28f), 0, 4.f);
    dl->AddPolyline(pts.data(), (int)pts.size(), col, 0, 1.6f);
    dl->AddCircleFilled(pts.back(), 3.2f, col);
    dl->AddCircle(pts.back(), 5.5f, C::alpha(col, 0.35f), 16, 1.5f);
}

void RangeGauge(ImVec2 p, float w, float h, const UMarker& m, int sex, double v, int sev) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + ImVec2(w, h), IM_COL32(28, 38, 62, 255), h / 2);
    double span = std::max(m.hi - m.lo, 1e-9);
    auto X = [&](double x) { double t = (std::max(m.lo, std::min(m.hi, x)) - m.lo) / span; return p.x + (float)t * w; };
    auto band = [&](int tier, ImU32 col) {
        for (auto& iv : m.tier[tier][sex]) {
            double a = iv.first <= -1e8 ? m.lo : iv.first, b = iv.second >= 1e8 ? m.hi : iv.second;
            float x0 = X(a), x1 = X(b);
            if (x1 - x0 < 3) x1 = x0 + 3;
            dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x1, p.y + h), col, 2.f);
        }
    };
    band(0, C::alpha(C::green, 0.22f));
    band(2, C::alpha(C::amber, 0.65f));
    band(3, C::alpha(C::red, 0.75f));
    band(1, C::alpha(C::green, 0.80f));
    float x = X(m.abs ? std::fabs(v) : v);
    ImU32 col = sevColor(sev);
    dl->AddLine(ImVec2(x, p.y - 3), ImVec2(x, p.y + h + 3), IM_COL32(255, 255, 255, 230), 2.f);
    dl->AddCircleFilled(ImVec2(x, p.y + h / 2), h * 0.9f, C::alpha(col, 0.25f));
    dl->AddCircleFilled(ImVec2(x, p.y + h / 2), h * 0.55f, col);
    dl->AddCircle(ImVec2(x, p.y + h / 2), h * 0.55f, IM_COL32(255, 255, 255, 220), 16, 1.5f);
}

void Radar(ImVec2 c, float r, const double* v, int n, const std::vector<std::string>& labels, ImU32 col) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto pt = [&](int i, float k) {
        float a = -IM_PI / 2 + 2 * IM_PI * i / n;
        return ImVec2(c.x + std::cos(a) * r * k, c.y + std::sin(a) * r * k);
    };
    for (int ring = 1; ring <= 4; ++ring) {
        std::vector<ImVec2> pts;
        for (int i = 0; i < n; ++i) pts.push_back(pt(i, ring / 4.f));
        dl->AddPolyline(pts.data(), n, ring == 4 ? C::alpha(C::faint, 0.9f) : C::alpha(C::faint, 0.45f), ImDrawFlags_Closed, 1.f);
    }
    for (int i = 0; i < n; ++i) dl->AddLine(c, pt(i, 1.f), C::alpha(C::faint, 0.4f));
    std::vector<ImVec2> poly;
    for (int i = 0; i < n; ++i) poly.push_back(pt(i, (float)std::max(0.06, std::min(1.0, v[i] / 100.0))));
    dl->AddConvexPolyFilled(poly.data(), n, C::alpha(col, 0.20f));
    dl->AddPolyline(poly.data(), n, col, ImDrawFlags_Closed, 2.2f);
    ImGui::PushFont(fSmall);
    for (int i = 0; i < n; ++i) {
        ImU32 pc = v[i] < 50 ? C::red : (v[i] < 78 ? C::amber : C::green);
        dl->AddCircleFilled(poly[i], 4.5f, pc);
        dl->AddCircle(poly[i], 4.5f, IM_COL32(255, 255, 255, 200), 12, 1.2f);
        if (i < (int)labels.size()) {
            ImVec2 lp = pt(i, 1.14f), ts = ImGui::CalcTextSize(labels[i].c_str());
            float ang = -IM_PI / 2 + 2 * IM_PI * i / n, cx = std::cos(ang);
            lp.x -= ts.x * (0.5f - 0.5f * cx);  // left-align when pointing right, right-align when pointing left
            lp.y -= ts.y / 2;
            dl->AddText(lp, C::dim, labels[i].c_str());
        }
    }
    ImGui::PopFont();
}

}  // namespace ui
