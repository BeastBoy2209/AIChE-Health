#pragma once
#include <imgui.h>
#include <string>

namespace ui {

// palette (mission-control dark theme)
namespace C {
extern ImU32 bg0, bg1, panel, panel2, border, text, dim, faint, cyan, green, amber, red, purple, blue;
inline ImU32 alpha(ImU32 c, float a) { return (c & 0x00FFFFFF) | ((ImU32)(a * 255.f) << 24); }
inline ImVec4 v4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }
}  // namespace C

extern ImFont *fBody, *fSmall, *fBold, *fTitle, *fBig, *fMono, *fNum;

// colour for severity 0/1/2
ImU32 sevColor(int sev);
const char* sevLabel(int sev);

void loadFonts(const std::string& assetDir, float scale);
void applyStyle();
void drawBackground(float time);  // starfield + Earth limb glow

}  // namespace ui
