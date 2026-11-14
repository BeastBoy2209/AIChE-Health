#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include "app.h"
#include "theme.h"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

namespace ui {

inline ImVec2 operator+(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
inline ImVec2 operator-(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }
inline ImVec2 operator*(ImVec2 a, float k) { return ImVec2(a.x * k, a.y * k); }

float Anim(const char* id, float target, float speed = 8.f);   // smooth value keyed by id
bool BeginCard(const char* id, ImVec2 size, ImU32 accent = 0);   // size.x/-y == -1 => fill
void EndCard();
void TextC(ImFont* f, ImU32 col, const char* fmt, ...);
void Label(const char* s);                                       // dim small caption
float Chip(const char* txt, ImU32 col, bool filled = false);     // inline chip at cursor, returns width
bool Pill(const char* label, bool active, ImU32 col = 0, float minW = 0);
void HealthRing(ImVec2 c, float r, float value01, ImU32 col, float thick);
void Sparkline(ImVec2 p, ImVec2 sz, const std::vector<float>& v, ImU32 col, float lo, float hi);
void RangeGauge(ImVec2 p, float w, float h, const UMarker& m, int sex, double value, int sev);
void Radar(ImVec2 c, float r, const double* v, int n, const std::vector<std::string>& labels, ImU32 col);
void SectionTitle(const char* s, ImU32 col = 0);
std::string Fmt(double v);
bool NeonButton(const char* label, ImVec2 size, ImU32 col = 0);

}  // namespace ui
