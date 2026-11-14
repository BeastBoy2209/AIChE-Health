#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "widgets.h"

namespace ui {

static unsigned int g_tex[2][2] = {{0, 0}, {0, 0}};  // [sex][view]
static float g_aspect = 0.462f;

static unsigned int loadHolo(const std::string& path) {
    int w, h, n;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!px) return 0;
    g_aspect = (float)w / h;
    // background colour from the corners
    double br = 0, bg = 0, bb = 0; int cnt = 0;
    for (int cy : {2, h - 3}) for (int cx : {2, w - 3}) { unsigned char* q = px + ((size_t)cy * w + cx) * 4; br += q[0]; bg += q[1]; bb += q[2]; cnt++; }
    br /= cnt; bg /= cnt; bb /= cnt;
    for (size_t i = 0; i < (size_t)w * h; ++i) {
        unsigned char* q = px + i * 4;
        double d = std::max({std::fabs(q[0] - br), std::fabs(q[1] - bg), std::fabs(q[2] - bb)});
        double a = std::min(1.0, d / 60.0);
        a = std::pow(a, 0.75);
        double warm = std::max(0.0, std::min(1.0, (q[0] - q[2]) / 110.0)) * 0.6;
        q[0] = (unsigned char)(70 + (255 - 70) * warm);
        q[1] = (unsigned char)(205 + (120 - 205) * warm);
        q[2] = (unsigned char)(255 + (110 - 255) * warm);
        q[3] = (unsigned char)(a * 235);
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);  // GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    stbi_image_free(px);
    return t;
}

bool loadBodyTextures(const std::string& dir) {
    const char* names[2][2] = {{"male_front.jpg", "male_back.jpg"}, {"female_front.jpg", "female_back.jpg"}};
    bool ok = true;
    for (int s = 0; s < 2; ++s) for (int v = 0; v < 2; ++v) { g_tex[s][v] = loadHolo(dir + "/" + names[s][v]); ok = ok && g_tex[s][v]; }
    return ok;
}

static ImU32 painColor(int i) { return i >= 7 ? C::red : (i >= 4 ? C::amber : C::green); }

void drawBody(App& a, ImVec2 size) {
    static int selView = -1, selInt = 4, selType = 0, selDur = 2;
    static std::string selZone;
    static float selX = 0, selY = 0;
    static char note[128] = "";
    static int lastCrew = -1;
    if (lastCrew != a.sel) { lastCrew = a.sel; selView = -1; selZone.clear(); }
    const UCrew& crew = a.cur();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float gap = 16;
    float imgH = size.y - 92, imgW = imgH * g_aspect;
    float w1 = imgW * 2 + 70, w2 = size.x - w1 - gap;
    const char* views[2] = {"front", "back"};

    if (BeginCard("bodycard", ImVec2(w1, size.y), C::cyan)) {
        SectionTitle("BODY MAP");
        ImGui::SameLine(0, 14);
        TextC(fSmall, C::dim, "%s  /  click where it hurts", crew.name.c_str());
        ImVec2 base = ImGui::GetCursorScreenPos() + ImVec2(0, 8);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int v = 0; v < 2; ++v) {
            ImVec2 p = base + ImVec2(v * (imgW + 38), 0);
            ImVec2 q = p + ImVec2(imgW, imgH);
            dl->AddRectFilled(p, q, IM_COL32(8, 14, 30, 255), 12);
            dl->AddRect(p, q, C::alpha(C::cyan, 0.25f), 12);
            // grid + scanline
            for (int g = 1; g < 10; ++g) {
                dl->AddLine(ImVec2(p.x, p.y + imgH * g / 10), ImVec2(q.x, p.y + imgH * g / 10), C::alpha(C::cyan, 0.05f));
                dl->AddLine(ImVec2(p.x + imgW * g / 10, p.y), ImVec2(p.x + imgW * g / 10, q.y), C::alpha(C::cyan, 0.05f));
            }
            if (g_tex[crew.sex][v]) dl->AddImage((ImTextureID)(intptr_t)g_tex[crew.sex][v], p, q);
            float sy = p.y + std::fmod(a.time * 70.f + v * 90.f, imgH);
            dl->AddRectFilledMultiColor(ImVec2(p.x + 2, sy - 18), ImVec2(q.x - 2, sy), 0, 0, C::alpha(C::cyan, 0.22f), C::alpha(C::cyan, 0.22f));
            dl->AddLine(ImVec2(p.x + 2, sy), ImVec2(q.x - 2, sy), C::alpha(C::cyan, 0.7f), 1.5f);
            ImGui::PushFont(fSmall);
            dl->AddText(p + ImVec2(12, 10), C::cyan, v ? "BACK" : "FRONT");
            ImGui::PopFont();

            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(v);
            bool clicked = ImGui::InvisibleButton("##body", ImVec2(imgW, imgH));
            ImGui::PopID();
            bool hov = ImGui::IsItemHovered();
            ImVec2 mp = ImGui::GetIO().MousePos;
            float px = (mp.x - p.x) / imgW * 100.f, py = (mp.y - p.y) / imgH * 100.f;
            std::string key = std::string(crew.sex ? "female_" : "male_") + views[v];
            const nlohmann::json* best = nullptr; double bestArea = 1e9;
            if (a.zones.contains(key))
                for (auto& z : a.zones[key]) {
                    double x = z["coords"]["x"], y = z["coords"]["y"], w = z["coords"]["w"], h = z["coords"]["h"];
                    if (px >= x && px <= x + w && py >= y && py <= y + h && w * h < bestArea) { best = &z; bestArea = w * h; }
                }
            if (hov && best) {
                auto& c = (*best)["coords"];
                ImVec2 z0 = p + ImVec2((float)c["x"] / 100.f * imgW, (float)c["y"] / 100.f * imgH);
                ImVec2 z1 = z0 + ImVec2((float)c["w"] / 100.f * imgW, (float)c["h"] / 100.f * imgH);
                dl->AddRectFilled(z0, z1, C::alpha(C::cyan, 0.16f), 6);
                dl->AddRect(z0, z1, C::alpha(C::cyan, 0.8f), 6, 0, 1.4f);
                ImGui::SetTooltip("%s", (*best)["name"].get<std::string>().c_str());
            }
            if (hov && clicked && best) {
                selView = v; selZone = (*best)["name"]; selX = px; selY = py;
            }
            // existing pains
            for (auto& pn : a.pains) {
                if (pn.view != views[v]) continue;
                ImVec2 c = p + ImVec2((float)pn.x / 100.f * imgW, (float)pn.y / 100.f * imgH);
                if (!pn.active) { dl->AddCircleFilled(c, 4, C::alpha(C::dim, 0.5f)); continue; }
                ImU32 col = painColor(pn.intensity);
                float pulse = 0.5f + 0.5f * std::sin(a.time * 3.5f + pn.id);
                dl->AddCircleFilled(c, 16 + 6 * pulse, C::alpha(col, 0.14f));
                dl->AddCircle(c, 12 + 10 * pulse, C::alpha(col, 0.5f * (1 - pulse)), 24, 2.f);
                dl->AddCircleFilled(c, 11, col);
                dl->AddCircle(c, 11, IM_COL32(255, 255, 255, 230), 20, 1.6f);
                char b[8]; snprintf(b, sizeof b, "%d", pn.intensity);
                ImGui::PushFont(fSmall);
                ImVec2 ts = ImGui::CalcTextSize(b);
                dl->AddText(c - ts * 0.5f, C::bg0, b);
                ImGui::PopFont();
            }
            if (selView == v) {
                ImVec2 c = p + ImVec2(selX / 100.f * imgW, selY / 100.f * imgH);
                float r = 14 + 3 * std::sin(a.time * 6.f);
                dl->AddCircle(c, r, C::text, 28, 2.f);
                dl->AddLine(c - ImVec2(r + 8, 0), c - ImVec2(r - 3, 0), C::text, 2.f);
                dl->AddLine(c + ImVec2(r + 8, 0), c + ImVec2(r - 3, 0), C::text, 2.f);
                dl->AddLine(c - ImVec2(0, r + 8), c - ImVec2(0, r - 3), C::text, 2.f);
                dl->AddLine(c + ImVec2(0, r + 8), c + ImVec2(0, r - 3), C::text, 2.f);
            }
        }
        ImGui::SetCursorScreenPos(base + ImVec2(0, imgH + 4));
    }
    EndCard();

    ImGui::SameLine(0, gap);
    if (BeginCard("painform", ImVec2(w2, size.y), C::red)) {
        float iw = w2 - 34;
        SectionTitle("REPORT PAIN");
        if (selZone.empty()) {
            TextC(fSmall, C::dim, "Select a body area on the figure. Front and back views are both clickable.");
        } else {
            ImGui::SameLine(0, 12);
            Chip((selZone + " / " + views[selView]).c_str(), C::red);
            ImGui::Dummy(ImVec2(1, 4));
            TextC(fBody, C::text, "Intensity");
            ImGui::SameLine(iw - 60);
            TextC(fBold, painColor(selInt), "%d / 10", selInt);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, C::v4(painColor(selInt)));
            ImGui::SetNextItemWidth(iw);
            ImGui::SliderInt("##int", &selInt, 1, 10, "");
            ImGui::PopStyleColor();
            static const char* types[] = {"Dull ache", "Sharp / stabbing", "Burning", "Throbbing", "Pressure / tightness", "Numbness / tingling"};
            ImGui::SetNextItemWidth(iw);
            ImGui::Combo("##type", &selType, types, 6);
            ImGui::SetNextItemWidth(iw);
            ImGui::SliderInt("##dur", &selDur, 1, 72, "lasting %d h");
            ImGui::SetNextItemWidth(iw);
            ImGui::InputTextWithHint("##note", "Notes (what triggered it?)", note, sizeof note);
            if (NeonButton("SUBMIT PAIN REPORT", ImVec2(iw, 42), C::red)) {
                a.api->post("/api/crew/" + std::to_string(crew.id) + "/pain",
                            {{"view", views[selView]}, {"zone", selZone}, {"x", selX}, {"y", selY}, {"intensity", selInt},
                             {"kind", types[selType]}, {"duration_h", selDur}, {"note", std::string(note)}});
                a.refreshAll();
                a.notify("Pain recorded: " + selZone + " " + std::to_string(selInt) + "/10");
                selZone.clear(); selView = -1; note[0] = 0;
            }
        }
        ImGui::Dummy(ImVec2(1, 10));
        int active = 0; for (auto& p : a.pains) active += p.active;
        SectionTitle("ACTIVE REPORTS");
        ImGui::SameLine(0, 10);
        Chip(std::to_string(active).c_str(), active ? C::red : C::green);
        ImGui::BeginChild("plist", ImVec2(0, 0), 0, 0);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (auto& p : a.pains) {
            ImVec2 q = ImGui::GetCursorScreenPos();
            float hh = 74;
            ImU32 col = p.active ? painColor(p.intensity) : C::faint;
            dl->AddRectFilled(q, q + ImVec2(iw - 4, hh), C::alpha(col, 0.08f), 10);
            dl->AddRectFilled(q, q + ImVec2(4, hh), col, 2);
            ImGui::PushFont(fBold);
            dl->AddText(q + ImVec2(14, 6), C::text, (p.zone + "  " + std::to_string(p.intensity) + "/10").c_str());
            ImGui::PopFont();
            ImGui::PushFont(fSmall);
            char sub[160]; snprintf(sub, sizeof sub, "day %d / %s / %.0f h%s%s", p.day, p.kind.c_str(), p.dur, p.note.empty() ? "" : " / ", p.note.c_str());
            dl->AddText(nullptr, 0.f, q + ImVec2(14, 32), C::dim, sub, nullptr, iw - 120);
            ImGui::PopFont();
            if (p.active) {
                ImGui::SetCursorScreenPos(q + ImVec2(iw - 110, 40));
                ImGui::PushID((int)p.id);
                if (Pill("Resolved", false, C::green)) {
                    a.api->post("/api/crew/" + std::to_string(crew.id) + "/pain/" + std::to_string(p.id) + "/resolve");
                    a.refreshAll();
                }
                ImGui::PopID();
            } else {
                dl->AddText(q + ImVec2(iw - 80, 8), C::faint, "resolved");
            }
            ImGui::SetCursorScreenPos(q + ImVec2(0, hh + 8));
        }
        if (a.pains.empty()) TextC(fSmall, C::dim, "No pain reported.");
        ImGui::EndChild();
    }
    EndCard();
    (void)o;
}

}  // namespace ui
