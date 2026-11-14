#include "theme.h"
#include <cmath>
#include <vector>

namespace ui {

namespace C {
ImU32 bg0 = IM_COL32(5, 8, 16, 255), bg1 = IM_COL32(10, 15, 28, 255), panel = IM_COL32(15, 22, 40, 235),
      panel2 = IM_COL32(22, 32, 56, 255), border = IM_COL32(38, 54, 88, 255), text = IM_COL32(228, 236, 250, 255),
      dim = IM_COL32(130, 150, 182, 255), faint = IM_COL32(70, 88, 122, 255), cyan = IM_COL32(56, 225, 255, 255),
      green = IM_COL32(61, 220, 151, 255), amber = IM_COL32(255, 184, 77, 255), red = IM_COL32(255, 92, 108, 255),
      purple = IM_COL32(167, 139, 250, 255), blue = IM_COL32(90, 140, 255, 255);
}

ImFont *fBody, *fSmall, *fBold, *fTitle, *fBig, *fMono, *fNum;

ImU32 sevColor(int sev) { return sev == 0 ? C::green : (sev == 1 ? C::amber : C::red); }
const char* sevLabel(int sev) { return sev == 0 ? "NOMINAL" : (sev == 1 ? "WATCH" : "CRITICAL"); }

void loadFonts(const std::string& dir, float s) {
    ImGuiIO& io = ImGui::GetIO();
    static const ImWchar ranges[] = {0x20, 0x17F, 0x370, 0x3FF, 0x400, 0x52F, 0x2010, 0x2022, 0x2190, 0x2193, 0x2264, 0x2265, 0x2212, 0x2212, 0x25CF, 0x25CF, 0};
    static const ImWchar cyr[] = {0x400, 0x52F, 0};
    auto load = [&](const char* f, float px) {
        ImFont* r = io.Fonts->AddFontFromFileTTF((dir + "/fonts/" + f).c_str(), px * s, nullptr, ranges);
        if (r && std::string(f) != "Inter.ttf") {  // headline fonts lack Cyrillic: borrow it from Inter so crew names always render
            ImFontConfig cfg; cfg.MergeMode = true;
            io.Fonts->AddFontFromFileTTF((dir + "/fonts/Inter.ttf").c_str(), px * s * 0.82f, &cfg, cyr);
        }
        return r;
    };
    fBody = load("Inter.ttf", 16);
    fSmall = load("Inter.ttf", 13);
    fBold = load("Rajdhani-Bold.ttf", 22);
    fTitle = load("Orbitron.ttf", 20);
    fBig = load("Rajdhani-Bold.ttf", 50);
    fNum = load("Rajdhani-Bold.ttf", 34);
    fMono = load("RobotoMono.ttf", 15);
    if (!fBody) fBody = io.Fonts->AddFontDefault();
    if (!fSmall) fSmall = fBody;
    if (!fBold) fBold = fBody;
    if (!fTitle) fTitle = fBody;
    if (!fBig) fBig = fBody;
    if (!fNum) fNum = fBody;
    if (!fMono) fMono = fBody;
    io.FontDefault = fBody;
}

void applyStyle() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 0; st.ChildRounding = 12; st.FrameRounding = 8; st.PopupRounding = 10; st.GrabRounding = 8;
    st.ScrollbarRounding = 8; st.ScrollbarSize = 8; st.TabRounding = 8;
    st.WindowPadding = ImVec2(0, 0); st.FramePadding = ImVec2(12, 8); st.ItemSpacing = ImVec2(10, 10);
    st.GrabMinSize = 18;
    st.WindowBorderSize = 0; st.ChildBorderSize = 0; st.FrameBorderSize = 0; st.PopupBorderSize = 1;
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = C::v4(C::text);
    c[ImGuiCol_TextDisabled] = C::v4(C::dim);
    c[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = C::v4(C::panel2);
    c[ImGuiCol_Border] = C::v4(C::border);
    c[ImGuiCol_FrameBg] = C::v4(IM_COL32(11, 17, 32, 255));
    c[ImGuiCol_FrameBgHovered] = C::v4(IM_COL32(20, 30, 54, 255));
    c[ImGuiCol_FrameBgActive] = C::v4(IM_COL32(26, 38, 66, 255));
    c[ImGuiCol_Button] = C::v4(C::alpha(C::cyan, 0.16f));
    c[ImGuiCol_ButtonHovered] = C::v4(C::alpha(C::cyan, 0.30f));
    c[ImGuiCol_ButtonActive] = C::v4(C::alpha(C::cyan, 0.45f));
    c[ImGuiCol_SliderGrab] = C::v4(C::cyan);
    c[ImGuiCol_SliderGrabActive] = C::v4(IM_COL32(150, 240, 255, 255));
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = C::v4(C::border);
    c[ImGuiCol_Header] = C::v4(C::alpha(C::cyan, 0.15f));
    c[ImGuiCol_HeaderHovered] = C::v4(C::alpha(C::cyan, 0.28f));
    c[ImGuiCol_HeaderActive] = C::v4(C::alpha(C::cyan, 0.40f));
    c[ImGuiCol_CheckMark] = C::v4(C::cyan);
    c[ImGuiCol_Separator] = C::v4(C::border);
}

void drawBackground(float t) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 sz = ImGui::GetIO().DisplaySize;
    dl->AddRectFilledMultiColor(ImVec2(0, 0), sz, C::bg0, C::bg0, C::bg1, C::bg1);
    // distant Earth limb glow at the bottom
    float R = sz.x * 0.95f;
    ImVec2 cen(sz.x * 0.62f, sz.y + R * 0.90f);
    for (int i = 0; i < 14; ++i) {
        float k = i / 14.f;
        dl->AddCircle(cen, R + 40.f * (1 - k), IM_COL32(40, 120, 255, (int)(34 * k * k)), 128, 6.f * (1 - k) + 1.f);
    }
    dl->AddCircleFilled(cen, R, IM_COL32(8, 18, 40, 255), 160);
    dl->AddCircle(cen, R, IM_COL32(90, 180, 255, 120), 160, 2.f);
    // stars
    static std::vector<float> st;
    if (st.empty()) {
        unsigned s = 12345;
        auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.f; };
        for (int i = 0; i < 260; ++i) { st.push_back(rnd()); st.push_back(rnd()); st.push_back(rnd()); st.push_back(rnd()); }
    }
    for (size_t i = 0; i + 3 < st.size(); i += 4) {
        float depth = 0.2f + st[i + 2] * 0.8f;
        float x = std::fmod(st[i] * sz.x - t * 3.f * depth + sz.x * 4.f, sz.x);
        float y = st[i + 1] * sz.y;
        float tw = 0.55f + 0.45f * std::sin(t * (0.6f + st[i + 3] * 1.8f) + st[i + 3] * 20.f);
        dl->AddCircleFilled(ImVec2(x, y), 0.6f + depth * 1.1f, IM_COL32(200, 225, 255, (int)(150 * depth * tw)));
    }
}

}  // namespace ui
