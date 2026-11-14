// AIChE Health core engine: profiles (JSON), scoring, MoCA tracking, offline medical advisor.
#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include "catalog.h"
#include "db.h"

namespace av {

struct Mission {
    std::string name = "Untitled mission", vehicle, type = "Space station";
    int duration = 180;
    bool simulated = false;  // demo data generator drives "next day"
};

struct Crew {
    int id = 0; std::string name, role; int sex = 0; int age = 35;  // sex: 0 male, 1 female
    double height = 175, weight = 75; int edu = 16;                   // education years (MoCA +1 point if <= 12)
    std::string notes, scenario;                                      // scenario only used by simulated demo missions
};

struct LabItem {
    const Marker* m;
    std::vector<std::pair<int, double>> hist;  // (day, value)
    Classification cls;
};

struct MindEntry { int day; double mood, stress, anxiety, fatigue, isolation, sleep, pvt; };
struct PainEntry { long long id; int day; std::string view, zone, kind, note; double x, y; int intensity; double duration_h; bool active; };
struct MocaEntry {
    long long id; int day; int visuo, naming, attention, language, abstraction, recall, orient, bonus, total;
    std::string version, note; int minutes;
};
struct MocaInfo { bool has = false; int lastDay = 0, lastTotal = 0, daysSince = 9999, dueIn = 0; bool overdue = true; };

struct Advice { int sev; std::string title, system; std::vector<std::string> why, actions; };

struct Health {
    double systems[7]; double overall;
    int danger = 0, concern = 0, total = 0;
    int status = 0;  // 0 nominal, 1 watch, 2 critical
};

class Engine {
public:
    // dataDir: biomarkers.json + body_zones.json. profilesDir: *.json mission profiles.
    // profileFile: profile to open ("" = last used one, or an empty workspace).
    bool init(const std::string& dataDir, const std::string& profilesDir, const std::string& profileFile);
    std::string lastError;

    nlohmann::json state();
    nlohmann::json catalogJson();
    nlohmann::json labs(int crew);
    nlohmann::json addLab(int crew, const std::string& bio, double value);
    nlohmann::json mind(int crew);
    nlohmann::json addMind(int crew, const nlohmann::json& body);
    nlohmann::json pain(int crew);
    nlohmann::json addPain(int crew, const nlohmann::json& body);
    nlohmann::json resolvePain(int crew, long long pid);
    nlohmann::json moca(int crew);
    nlohmann::json addMoca(int crew, const nlohmann::json& body);
    nlohmann::json advice(int crew);
    nlohmann::json advance(int days);
    nlohmann::json events(int limit);
    nlohmann::json zones() { return zones_; }
    std::string report(int crew);
    static const char* systemName(int i);

    // mission / profiles / crew management
    nlohmann::json mission();
    nlohmann::json updateMission(const nlohmann::json& b);
    nlohmann::json profiles();
    nlohmann::json loadProfile(const std::string& file);
    nlohmann::json newProfile(const nlohmann::json& b);
    nlohmann::json saveProfileAs(const std::string& file);
    nlohmann::json addCrew(const nlohmann::json& b);
    nlohmann::json updateCrew(int id, const nlohmann::json& b);
    nlohmann::json deleteCrew(int id);
    bool buildDemo(const std::string& path);  // generate the sample profile file

private:
    std::mutex mu_;
    Catalog cat_;
    std::unique_ptr<Db> db_;
    std::vector<Crew> crew_;
    Mission mission_;
    nlohmann::json zones_;
    std::string profilesDir_, profileFile_;  // file name inside profilesDir_
    int day_ = 1, nextCrewId_ = 1;

    void createSchema();
    void resetStore();
    void simulateDay(int day);
    void simMind(const Crew& c, int day);
    void simPain(const Crew& c, int day);
    void simMoca(const Crew& c, int day);
    void addEvent(int day, int crew, int sev, const std::string& text);
    std::vector<LabItem> loadLabs(int crew);
    std::vector<MindEntry> loadMind(int crew);
    std::vector<PainEntry> loadPain(int crew, bool onlyActive);
    std::vector<MocaEntry> loadMoca(int crew);
    Health health(const Crew& c, const std::vector<LabItem>& labs, const std::vector<MindEntry>& mind, const std::vector<PainEntry>& pain,
                  const std::vector<MocaEntry>& moca);
    std::vector<Advice> runAdvisor(const Crew& c, const std::vector<LabItem>& labs, const std::vector<MindEntry>& mind,
                                   const std::vector<PainEntry>& pain, const std::vector<MocaEntry>& moca);
    long long insertLab(int crew, const std::string& bio, double v, int day);
    void insertPain(int crew, int day, const std::string& view, const std::string& zone, double x, double y, int intensity, const std::string& kind, double dur, const std::string& note);
    void insertMoca(int crew, int day, const int d[7], int bonus, const std::string& version, int minutes, const std::string& note);
    bool zoneCenter(int sex, const std::string& view, const std::string& zone, double& x, double& y);
    Crew* crewById(int id);
    MocaInfo mocaInfo(const std::vector<MocaEntry>& v) const;
    nlohmann::json exportProfile();
    bool importProfile(const nlohmann::json& j, std::string& err);
    bool writeProfile(const std::string& path);
    void autosave();
    void rememberLast();
    static double mindScore(const MindEntry& e);
    static std::string safeName(const std::string& s);
};

}  // namespace av
