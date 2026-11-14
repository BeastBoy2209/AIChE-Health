#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "widgets.h"

namespace ui {
using json = nlohmann::json;

static const char* TYPES[] = {"Space station", "Spacecraft", "Lunar base", "Mars transit", "Mars base"};
static const char* ROLES[] = {"Commander", "Pilot", "Flight Engineer", "Mission Specialist", "Medical Officer", "Scientist", "Payload Specialist"};
static const char* SCEN_ID[] = {"", "nominal", "sans", "bone_renal", "thrombosis"};
static const char* SCEN_NAME[] = {"(none)", "Nominal", "SANS (eyes)", "Bone and kidney", "Jugular thrombosis"};

namespace {
struct CrewForm {
    int id = -1;
    char name[64] = "", role[48] = "", notes[256] = "";
    int sex = 0, age = 35, edu = 16, scen = 0;
    float height = 175, weight = 75;
    void load(const UCrew& c) {
        id = c.id;
        snprintf(name, sizeof name, "%s", c.name.c_str());
        snprintf(role, sizeof role, "%s", c.role.c_str());
        snprintf(notes, sizeof notes, "%s", c.notes.c_str());
        sex = c.sex; age = c.age; edu = c.edu; height = (float)c.height; weight = (float)c.weight;
        scen = 0;
        for (int i = 0; i < 5; ++i) if (c.scenario == SCEN_ID[i]) scen = i;
    }
};
}

void drawSetup(App& a, ImVec2 size) {
    static CrewForm form;
    static int formCrewId = -2;
    static char newName[64] = "New mission", newVeh[64] = "", saveAs[64] = "";
    static int newType = 0, newDur = 180;
    static char mName[64] = "", mVeh[64] = "";
    static int mType = 0, mDur = 180, mDay = 1;
    static bool mSim = false;
    static std::string mFile;
    static float deleteArm = 0;
    static bool showNew = false;

    ImVec2 o = ImGui::GetCursorScreenPos();
    float gap = 16, w1 = size.x * 0.28f, w2 = size.x * 0.28f, w3 = size.x - w1 - w2 - gap * 2;

    // keep mission form in sync when the loaded profile changes
    std::string mk = a.mission.file + "|" + a.mission.name + std::to_string(a.day);
    if (mFile != mk) {
        mFile = mk;
        snprintf(mName, sizeof mName, "%s", a.mission.name.c_str());
        snprintf(mVeh, sizeof mVeh, "%s", a.mission.vehicle.c_str());
        mType = 0; for (int i = 0; i < 5; ++i) if (a.mission.type == TYPES[i]) mType = i;
        mDur = a.mission.duration; mDay = a.day; mSim = a.mission.simulated;
    }

    // ================= profiles
    if (BeginCard("profiles", ImVec2(w1, size.y), C::cyan)) {
        float iw = w1 - 34;
        SectionTitle("MISSION PROFILES");
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + iw);
        TextC(fSmall, C::dim, "A profile is a JSON file with the mission, crew and all their data. Changes are saved automatically.");
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(1, 4));
        float listH = size.y - 330 - (showNew ? 190 : 0);
        ImGui::BeginChild("plist", ImVec2(0, std::max(120.f, listH)), 0, 0);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (size_t i = 0; i < a.profiles.size(); ++i) {
            auto& p = a.profiles[i];
            ImVec2 q = ImGui::GetCursorScreenPos();
            float hh = 74;
            ImU32 col = p.current ? C::cyan : C::border;
            dl->AddRectFilled(q, q + ImVec2(iw - 4, hh), p.current ? C::alpha(C::cyan, 0.10f) : C::panel2, 10);
            dl->AddRect(q, q + ImVec2(iw - 4, hh), col, 10, 0, p.current ? 1.5f : 1.f);
            ImGui::PushFont(fBold);
            dl->AddText(nullptr, 0.f, q + ImVec2(12, 6), C::text, p.name.c_str(), nullptr, iw - 30);
            ImGui::PopFont();
            ImGui::PushFont(fSmall);
            char s[160]; snprintf(s, sizeof s, "%s  /  %d crew  /  day %d%s", p.vehicle.empty() ? p.type.c_str() : p.vehicle.c_str(), p.crew, p.day, p.simulated ? "  /  SIM" : "");
            dl->AddText(nullptr, 0.f, q + ImVec2(12, 30), C::dim, s, nullptr, iw - 30);
            dl->AddText(q + ImVec2(12, 52), C::faint, p.file.c_str());
            ImGui::PopFont();
            if (!p.current) {
                ImGui::SetCursorScreenPos(q + ImVec2(iw - 4 - 84, hh - 34));
                ImGui::PushID((int)i);
                if (Pill("LOAD", false, C::cyan, 70)) {
                    json r = a.api->post("/api/profile/load", {{"file", p.file}});
                    if (r.contains("error")) a.notify(r["error"].get<std::string>());
                    else { a.sel = 0; a.refreshAll(); a.refreshProfiles(); a.notify("Loaded profile: " + r["name"].get<std::string>()); formCrewId = -2; }
                    ImGui::PopID();
                    ImGui::EndChild();
                    EndCard();
                    return;
                }
                ImGui::PopID();
            } else {
                dl->AddText(q + ImVec2(iw - 4 - 70, hh - 24), C::cyan, "ACTIVE");
            }
            ImGui::SetCursorScreenPos(q + ImVec2(0, hh + 8));
        }
        if (a.profiles.empty()) TextC(fSmall, C::dim, "No profiles found in the profiles folder.");
        ImGui::EndChild();
        ImGui::Dummy(ImVec2(1, 4));
        if (!showNew) {
            if (NeonButton("NEW BLANK MISSION", ImVec2(iw, 40), C::green)) showNew = true;
        } else {
            ImGui::SetNextItemWidth(iw); ImGui::InputTextWithHint("##nn", "Mission name", newName, sizeof newName);
            ImGui::SetNextItemWidth(iw); ImGui::InputTextWithHint("##nv", "Vehicle / station", newVeh, sizeof newVeh);
            ImGui::SetNextItemWidth(iw * 0.55f); ImGui::Combo("##nt", &newType, TYPES, 5);
            ImGui::SameLine(); ImGui::SetNextItemWidth(iw * 0.4f); ImGui::InputInt("##nd", &newDur, 0, 0);
            if (NeonButton("CREATE", ImVec2(iw * 0.5f - 5, 38), C::green)) {
                json r = a.api->post("/api/profile/new", {{"name", std::string(newName)}, {"vehicle", std::string(newVeh)}, {"type", TYPES[newType]}, {"duration", std::max(1, newDur)}});
                a.sel = 0; a.refreshAll(); a.refreshProfiles();
                a.notify("Created empty mission. Add your crew on the right.");
                showNew = false; formCrewId = -2;
            }
            ImGui::SameLine();
            if (Pill("Cancel", false, C::dim, iw * 0.5f - 5)) showNew = false;
        }
        ImGui::Dummy(ImVec2(1, 6));
        ImGui::SetNextItemWidth(iw - 110);
        ImGui::InputTextWithHint("##sa", "Save copy as...", saveAs, sizeof saveAs);
        ImGui::SameLine();
        if (NeonButton("SAVE", ImVec2(100, 0), C::blue) && saveAs[0]) {
            json r = a.api->post("/api/profile/saveas", {{"file", std::string(saveAs)}});
            a.refreshProfiles(); a.refreshState();
            a.notify(r.contains("file") ? "Saved as " + r["file"].get<std::string>() : "Could not save");
            saveAs[0] = 0;
        }
    }
    EndCard();

    // ================= mission
    ImGui::SameLine(0, gap);
    if (BeginCard("mission", ImVec2(w2, size.y), C::blue)) {
        float iw = w2 - 34;
        SectionTitle("MISSION");
        TextC(fSmall, C::dim, "File: %s", a.mission.file.empty() ? "(unsaved)" : a.mission.file.c_str());
        ImGui::Dummy(ImVec2(1, 6));
        Label("Mission name"); ImGui::SetNextItemWidth(iw); ImGui::InputText("##mn", mName, sizeof mName);
        Label("Vehicle / station"); ImGui::SetNextItemWidth(iw); ImGui::InputText("##mv", mVeh, sizeof mVeh);
        Label("Flight type"); ImGui::SetNextItemWidth(iw); ImGui::Combo("##mt", &mType, TYPES, 5);
        Label("Planned duration (days)"); ImGui::SetNextItemWidth(iw); ImGui::InputInt("##md", &mDur, 1, 10);
        Label("Current mission day"); ImGui::SetNextItemWidth(iw); ImGui::InputInt("##mdd", &mDay, 1, 10);
        ImGui::Checkbox("Simulated mission (demo data generator)", &mSim);
        ImGui::Dummy(ImVec2(1, 4));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + iw);
        TextC(fSmall, C::faint, mSim ? "NEXT DAY generates fictional measurements for crew with a demo scenario." : "NEXT DAY only advances the mission day. Data comes from real measurements entered by the crew.");
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(1, 6));
        if (NeonButton("APPLY MISSION CHANGES", ImVec2(iw, 42), C::blue)) {
            a.api->post("/api/mission", {{"name", std::string(mName)}, {"vehicle", std::string(mVeh)}, {"type", TYPES[mType]},
                                         {"duration", std::max(1, mDur)}, {"day", std::max(1, mDay)}, {"simulated", mSim}});
            a.refreshAll(); a.refreshProfiles();
            a.notify("Mission updated");
        }
        ImGui::Dummy(ImVec2(1, 10));
        SectionTitle("PROFILE FOLDER");
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + iw);
        TextC(fSmall, C::dim, "Copy any *.json profile into this folder to make it appear in the list:");
        TextC(fMono, C::text, "%s", a.profilesDir.c_str());
        ImGui::PopTextWrapPos();
    }
    EndCard();

    // ================= crew
    ImGui::SameLine(0, gap);
    if (BeginCard("crewed", ImVec2(w3, size.y), C::purple)) {
        float iw = w3 - 34;
        SectionTitle("CREW");
        ImGui::SameLine(iw - 150);
        if (NeonButton("+ ADD MEMBER", ImVec2(150, 34), C::green)) {
            json r = a.api->post("/api/crew", {{"name", "New crew member"}});
            a.refreshAll();
            if (r.contains("id")) { a.selectCrewId(r["id"]); formCrewId = -2; }
            a.notify("Crew member added. Fill in the details below.");
        }
        ImGui::Dummy(ImVec2(1, 4));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // crew chips row
        float cx = 0;
        for (size_t i = 0; i < a.crew.size(); ++i) {
            const UCrew& c = a.crew[i];
            bool act = (int)i == a.sel;
            std::string lab = c.name;
            ImGui::PushID((int)i);
            if (cx > 0 && cx + 150 < iw) ImGui::SameLine(0, 8);
            if (Pill(lab.c_str(), act, c.hasData ? sevColor(c.status) : C::cyan)) { a.selectCrew((int)i); formCrewId = -2; }
            ImGui::PopID();
            cx += 150;
            if (cx + 150 > iw) cx = 0;
        }
        if (a.crew.empty()) TextC(fSmall, C::dim, "No crew yet. Press + ADD MEMBER.");
        ImGui::Dummy(ImVec2(1, 8));
        if (!a.crew.empty()) {
            const UCrew& c = a.cur();
            if (formCrewId != c.id) { form.load(c); formCrewId = c.id; deleteArm = 0; }
            ImGui::Separator();
            ImGui::Dummy(ImVec2(1, 6));
            float half = (iw - 10) / 2;
            Label("Full name"); ImGui::SetNextItemWidth(iw); ImGui::InputText("##cn", form.name, sizeof form.name);
            Label("Role");
            ImGui::SetNextItemWidth(iw * 0.6f); ImGui::InputText("##cr", form.role, sizeof form.role);
            ImGui::SameLine(0, 6);
            ImGui::SetNextItemWidth(iw * 0.4f - 6);
            if (ImGui::BeginCombo("##crp", "presets")) {
                for (auto r : ROLES) if (ImGui::Selectable(r)) snprintf(form.role, sizeof form.role, "%s", r);
                ImGui::EndCombo();
            }
            ImGui::BeginGroup(); Label("Sex (reference ranges)"); ImGui::SetNextItemWidth(half);
            const char* sx[] = {"Male", "Female"}; ImGui::Combo("##cs", &form.sex, sx, 2); ImGui::EndGroup();
            ImGui::SameLine(0, 10);
            ImGui::BeginGroup(); Label("Age"); ImGui::SetNextItemWidth(half); ImGui::InputInt("##ca", &form.age, 1, 5); ImGui::EndGroup();
            ImGui::BeginGroup(); Label("Height, cm"); ImGui::SetNextItemWidth(half); ImGui::InputFloat("##ch", &form.height, 1, 5, "%.0f"); ImGui::EndGroup();
            ImGui::SameLine(0, 10);
            ImGui::BeginGroup(); Label("Weight, kg"); ImGui::SetNextItemWidth(half); ImGui::InputFloat("##cw", &form.weight, 1, 5, "%.0f"); ImGui::EndGroup();
            Label("Education, years (12 or fewer adds +1 point to MoCA)"); ImGui::SetNextItemWidth(iw); ImGui::InputInt("##ce", &form.edu, 1, 2);
            Label("Medical notes, allergies, medications"); ImGui::InputTextMultiline("##cno", form.notes, sizeof form.notes, ImVec2(iw, 70));
            if (a.mission.simulated) {
                Label("Demo scenario (simulated missions only)"); ImGui::SetNextItemWidth(iw); ImGui::Combo("##csc", &form.scen, SCEN_NAME, 5);
            }
            ImGui::Dummy(ImVec2(1, 6));
            if (NeonButton("SAVE CHANGES", ImVec2(iw * 0.6f, 42), C::purple)) {
                a.api->post("/api/crew/" + std::to_string(c.id) + "/update",
                            {{"name", std::string(form.name)}, {"role", std::string(form.role)}, {"sex", form.sex}, {"age", form.age}, {"height", form.height},
                             {"weight", form.weight}, {"edu", form.edu}, {"notes", std::string(form.notes)}, {"scenario", std::string(SCEN_ID[form.scen])}});
                int keep = c.id;
                a.refreshAll(); a.selectCrewId(keep); formCrewId = -2;
                a.notify("Crew member saved");
            }
            ImGui::SameLine(0, 10);
            bool armed = deleteArm > 0;
            if (armed) deleteArm -= ImGui::GetIO().DeltaTime;
            if (NeonButton(armed ? "CONFIRM DELETE" : "DELETE", ImVec2(iw * 0.4f - 10, 42), C::red)) {
                if (armed) {
                    a.api->post("/api/crew/" + std::to_string(c.id) + "/delete");
                    a.sel = 0; a.refreshAll(); formCrewId = -2; deleteArm = 0;
                    a.notify("Crew member and all their data deleted");
                } else deleteArm = 4.f;
            }
            if (armed) TextC(fSmall, C::red, "This deletes the person and all their measurements. Click again to confirm.");
        }
        (void)dl;
    }
    EndCard();
}

}  // namespace ui
