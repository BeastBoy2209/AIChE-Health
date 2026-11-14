#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <set>

namespace av {
using json = nlohmann::json;
namespace fs = std::filesystem;

static const char* SYS[7] = {"Cardio & Blood", "Musculoskeletal", "Neuro-sensory", "Metabolic & Renal", "Immune & Endocrine", "Mind", "Pain"};
const char* Engine::systemName(int i) { return SYS[i]; }

static const int MOCA_MAX[7] = {5, 3, 6, 3, 2, 5, 6};  // visuospatial/executive, naming, attention, language, abstraction, delayed recall, orientation
static const char* MOCA_KEYS[7] = {"visuospatial", "naming", "attention", "language", "abstraction", "delayed_recall", "orientation"};
static const int MOCA_INTERVAL = 7;  // days: weekly, mandatory

static double clampd(double v, double a, double b) { return std::max(a, std::min(b, v)); }
static std::string fmt(double v) {
    char b[48];
    if (std::fabs(v) >= 100) snprintf(b, sizeof b, "%.0f", v);
    else if (std::fabs(v) >= 10) snprintf(b, sizeof b, "%.1f", v);
    else snprintf(b, sizeof b, "%.2f", v);
    return b;
}
static const char* sevName(int s) { return s == 0 ? "normal" : (s == 1 ? "concerning" : "DANGEROUS"); }
static const char* mocaClass(int t) { return t >= 26 ? "normal" : (t >= 18 ? "mild impairment range" : (t >= 10 ? "moderate impairment range" : "severe impairment range")); }

std::string Engine::safeName(const std::string& s) {
    std::string o;
    for (unsigned char c : s) o += (std::isalnum(c) || c == '-' || c == '_' || c == '.') ? (char)c : (c == ' ' ? '_' : '_');
    if (o.empty()) o = "profile";
    if (o.size() < 5 || o.substr(o.size() - 5) != ".json") o += ".json";
    return o;
}

// ---------------------------------------------------------------- init / schema / store
void Engine::resetStore() {
    db_.reset(new Db(":memory:"));
    createSchema();
    crew_.clear();
    nextCrewId_ = 1;
    day_ = 1;
}

void Engine::createSchema() {
    db_->exec(
        "CREATE TABLE IF NOT EXISTS labs(id INTEGER PRIMARY KEY, crew INT, bio TEXT, value REAL, day INT);"
        "CREATE INDEX IF NOT EXISTS ix_labs ON labs(crew, bio, day);"
        "CREATE TABLE IF NOT EXISTS mind(id INTEGER PRIMARY KEY, crew INT, day INT, mood REAL, stress REAL, anxiety REAL, fatigue REAL, isolation REAL, sleep REAL, pvt REAL);"
        "CREATE TABLE IF NOT EXISTS pain(id INTEGER PRIMARY KEY, crew INT, day INT, view TEXT, zone TEXT, x REAL, y REAL, intensity INT, kind TEXT, duration_h REAL, note TEXT, active INT);"
        "CREATE TABLE IF NOT EXISTS moca(id INTEGER PRIMARY KEY, crew INT, day INT, visuo INT, naming INT, attention INT, language INT, abstraction INT, recall INT, orient INT, bonus INT, total INT, version TEXT, minutes INT, note TEXT);"
        "CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY, day INT, crew INT, sev INT, text TEXT);");
}

bool Engine::init(const std::string& dataDir, const std::string& profilesDir, const std::string& profileFile) {
    try {
        if (!cat_.load(dataDir + "/biomarkers.json")) { lastError = "cannot load biomarkers.json in " + dataDir; return false; }
        std::ifstream zf(dataDir + "/body_zones.json", std::ios::binary);
        if (zf) zones_ = json::parse(zf, nullptr, false);
        profilesDir_ = profilesDir;
        std::error_code ec;
        fs::create_directories(profilesDir_, ec);
        resetStore();
        std::string pf = profileFile;
        if (pf.empty()) {
            std::ifstream lf(profilesDir_ + "/.last");
            std::getline(lf, pf);
        }
        if (!pf.empty()) loadProfile(pf);
        return true;
    } catch (std::exception& e) { lastError = e.what(); return false; }
}

Crew* Engine::crewById(int id) {
    for (auto& c : crew_) if (c.id == id) return &c;
    return nullptr;
}

void Engine::addEvent(int day, int crew, int sev, const std::string& text) {
    db_->prep("INSERT INTO events(day,crew,sev,text) VALUES(?,?,?,?)").i(day).i(crew).i(sev).t(text).run();
}
long long Engine::insertLab(int crew, const std::string& bio, double v, int day) {
    db_->prep("INSERT INTO labs(crew,bio,value,day) VALUES(?,?,?,?)").i(crew).t(bio).d(v).i(day).run();
    return db_->lastId();
}
void Engine::insertPain(int crew, int day, const std::string& view, const std::string& zone, double x, double y,
                        int intensity, const std::string& kind, double dur, const std::string& note) {
    db_->prep("INSERT INTO pain(crew,day,view,zone,x,y,intensity,kind,duration_h,note,active) VALUES(?,?,?,?,?,?,?,?,?,?,1)")
        .i(crew).i(day).t(view).t(zone).d(x).d(y).i(intensity).t(kind).d(dur).t(note).run();
}
void Engine::insertMoca(int crew, int day, const int d[7], int bonus, const std::string& version, int minutes, const std::string& note) {
    int total = std::min(30, d[0] + d[1] + d[2] + d[3] + d[4] + d[5] + d[6] + bonus);
    db_->prep("INSERT INTO moca(crew,day,visuo,naming,attention,language,abstraction,recall,orient,bonus,total,version,minutes,note) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)")
        .i(crew).i(day).i(d[0]).i(d[1]).i(d[2]).i(d[3]).i(d[4]).i(d[5]).i(d[6]).i(bonus).i(total).t(version).i(minutes).t(note).run();
}

bool Engine::zoneCenter(int sex, const std::string& view, const std::string& zone, double& x, double& y) {
    std::string key = std::string(sex ? "female_" : "male_") + view;
    if (!zones_.contains(key)) return false;
    for (auto& z : zones_[key])
        if (z["name"] == zone) {
            x = (double)z["coords"]["x"] + (double)z["coords"]["w"] / 2;
            y = (double)z["coords"]["y"] + (double)z["coords"]["h"] / 2;
            return true;
        }
    return false;
}

// ---------------------------------------------------------------- simulation (demo missions only)
struct Scenario { const char* key; double sev; int start, end; };

static int scenIdx(const std::string& s) {
    if (s == "sans") return 0;
    if (s == "bone_renal") return 1;
    if (s == "thrombosis") return 3;
    return 2;  // nominal / anything else
}

static std::vector<Scenario> scenariosFor(int idx) {
    switch (idx) {
    case 0: return {{"retinal nerve", 1.45, 8, 85}, {"optic nerve sheath", 1.35, 10, 90}, {"spherical equivalent", 1.2, 12, 95},
                    {"intraocular", 0.7, 10, 90}, {"morning salivary", 1.0, 20, 100}, {"sleep duration", 1.0, 20, 100}};
    case 1: return {{"n-telopeptide", 1.55, 5, 70}, {"c-telopeptide", 1.3, 5, 75}, {"urinary calcium", 1.6, 8, 72},
                    {"urinary citrate", 1.25, 8, 75}, {"vitamin d", 1.2, 5, 80}, {"urine output", 1.1, 10, 80},
                    {"supersaturation", 1.1, 10, 80}, {"multifidus", 0.9, 20, 90}, {"creatine kinase", 0.6, 20, 80}};
    case 2: return {{"sleep duration", 0.35, 30, 100}};
    case 3: return {{"peak venous", 2.15, 40, 76}, {"ijv cross", 1.9, 40, 76}, {"d-dimer", 2.1, 50, 76},
                    {"morning salivary", 0.8, 20, 100}, {"interleukin", 0.6, 30, 100}};
    }
    return {};
}

static uint32_t hashStr(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}

void Engine::simulateDay(int day) {
    std::map<std::string, int> prev;
    {
        std::map<std::string, double> last;
        db_->prep("SELECT crew,bio,value FROM labs ORDER BY day,id").each([&](Db::Stmt& s) {
            last[std::to_string(s.gi(0)) + "|" + s.gt(1)] = s.gd(2);
        });
        for (auto& kv : last) {
            auto p = kv.first.find('|');
            const Marker* m = cat_.find(kv.first.substr(p + 1));
            Crew* c = crewById(std::stoi(kv.first.substr(0, p)));
            if (m && c) prev[kv.first] = Catalog::classify(*m, c->sex, kv.second).severity;
        }
    }
    const auto& ms = cat_.all();
    for (const Crew& c : crew_) {
        int ci = c.id - 1, si = scenIdx(c.scenario);
        auto sc = scenariosFor(si);
        for (size_t mi = 0; mi < ms.size(); ++mi) {
            const Marker& m = ms[mi];
            int interval = m.avail == "onboard" ? 7 : (m.avail == "partial" ? 14 : 28);
            int off = (int)((mi * 3 + ci) % interval);
            if (!(day == 1 || (day - 1) % interval == off)) continue;
            const auto& sb = m.tier[T_SPACE][c.sex];
            double lo = sb.empty() ? m.space_mid : sb[0].lo, hi = sb.empty() ? m.space_mid : sb[0].hi;
            if (hi >= INF || lo <= -INF) { lo = hi = m.space_mid; }
            double half = std::max((hi - lo) / 2, 0.02 * (m.span_hi - m.span_lo));
            std::mt19937 rng(hashStr(m.id) * 31u + (uint32_t)ci * 977u + (uint32_t)day * 7919u);
            std::mt19937 rngBase(hashStr(m.id) * 17u + (uint32_t)ci * 131u);
            std::uniform_real_distribution<double> U(-1.0, 1.0);
            double base = (lo + hi) / 2 + U(rngBase) * 0.35 * half;
            double val = base + U(rng) * 0.12 * half;
            for (auto& s : sc) {
                if (m.key.find(s.key) == std::string::npos) continue;
                double p = clampd((double)(day - s.start) / (s.end - s.start), 0.0, 1.0) * s.sev;
                double t1 = Catalog::tierTarget(m, c.sex, T_CONCERN), t2 = Catalog::tierTarget(m, c.sex, T_DANGER);
                if (p <= 1.0) val = base + (t1 - base) * p + U(rng) * 0.04 * half;
                else val = t1 + (t2 - t1) * (p - 1.0) + U(rng) * 0.03 * half;
            }
            if (m.span_lo >= 0 && val < 0) val = std::fabs(val) * 0.2;
            insertLab(c.id, m.id, val, day);
            int sev = Catalog::classify(m, c.sex, val).severity;
            std::string k = std::to_string(c.id) + "|" + m.id;
            int ps = prev.count(k) ? prev[k] : 0;
            if (sev > ps && sev >= 1)
                addEvent(day, c.id, sev, m.name + " " + fmt(val) + " " + m.unit + " -> " + sevName(sev));
            prev[k] = sev;
        }
        simMind(c, day);
        simPain(c, day);
        simMoca(c, day);
    }
}

void Engine::simMind(const Crew& c, int day) {
    struct P { double m0, m1, s0, s1, a0, a1, f0, f1, i0, i1, q0, q1; };
    static const P prof[4] = {
        {7.5, 5.2, 3, 7.0, 2.5, 5.8, 4, 6.4, 3, 6.2, 7.5, 5.4},
        {7.5, 7.0, 3, 4.2, 2.5, 3.2, 4, 6.0, 3, 3.6, 7.5, 6.5},
        {8.0, 7.8, 2.5, 2.8, 2.0, 2.2, 3, 3.4, 2.5, 2.8, 8.0, 7.6},
        {7.5, 6.4, 3, 4.6, 2.5, 3.8, 4, 5.0, 3, 4.0, 7.5, 6.6},
    };
    const P& p = prof[scenIdx(c.scenario)];
    std::mt19937 rng(day * 104729u + c.id * 7919u);
    std::uniform_real_distribution<double> U(-0.7, 0.7);
    double t = clampd((double)day / 74.0, 0.0, 1.6);
    auto lerp = [&](double a, double b) { return clampd(a + (b - a) * t + U(rng), 1.0, 10.0); };
    double fat = lerp(p.f0, p.f1);
    double pvt = 0;
    if (day % 3 == 0) pvt = clampd(236 + 14 * (fat - 4) + U(rng) * 12, 190, 480);
    db_->prep("INSERT INTO mind(crew,day,mood,stress,anxiety,fatigue,isolation,sleep,pvt) VALUES(?,?,?,?,?,?,?,?,?)")
        .i(c.id).i(day).d(lerp(p.m0, p.m1)).d(lerp(p.s0, p.s1)).d(lerp(p.a0, p.a1)).d(fat).d(lerp(p.i0, p.i1)).d(lerp(p.q0, p.q1)).d(pvt).run();
}

void Engine::simPain(const Crew& c, int day) {
    double x, y;
    int s = scenIdx(c.scenario);
    if (s == 1 && day == 52 && zoneCenter(c.sex, "back", "Lower Back", x, y)) {
        insertPain(c.id, day, "back", "Lower Back", x, y, 4, "Dull ache", 36, "after ARED session");
        addEvent(day, c.id, 1, "Pain reported: Lower Back 4/10");
    }
    if (s == 3 && day == 72 && zoneCenter(c.sex, "front", "Neck", x, y)) {
        insertPain(c.id, day, "front", "Neck", x, y, 5, "Pressure / tightness", 20, "right side, swelling feeling");
        addEvent(day, c.id, 1, "Pain reported: Neck 5/10");
    }
    if (s == 0 && day == 77 && zoneCenter(c.sex, "front", "Head", x, y)) {
        insertPain(c.id, day, "front", "Head", x, y, 4, "Pressure / tightness", 8, "morning headache, blurred near vision");
        addEvent(day, c.id, 1, "Pain reported: Head 4/10");
    }
}

void Engine::simMoca(const Crew& c, int day) {
    if (day < 7 || (day - (c.id % 7)) % 7 != 0) return;
    int s = scenIdx(c.scenario);
    if (s == 1 && day > 65) return;  // demo: this crew member skipped this week's MoCA -> reminder fires
    std::mt19937 rng(day * 7121u + c.id * 331u);
    std::uniform_real_distribution<double> U(-0.6, 0.6);
    double base = s == 0 ? 28.5 - 3.4 * clampd(day / 74.0, 0, 1.3) : (s == 2 ? 29.2 : (s == 1 ? 28.2 : 28.4 - 1.0 * clampd(day / 74.0, 0, 1.3)));
    int bonus = c.edu <= 12 ? 1 : 0;
    int target = (int)std::lround(clampd(base + U(rng), 0, 30)) - bonus;
    int loss = std::max(0, std::min(30 - bonus, 30) - target - 0) - 0;
    int d[7];
    for (int i = 0; i < 7; ++i) d[i] = MOCA_MAX[i];
    int order[7] = {5, 2, 0, 3, 4, 1, 6};  // where points are usually lost: delayed recall, attention, visuospatial, ...
    int k = 0;
    int deficit = (30 - bonus) - target; if (deficit < 0) deficit = 0;
    (void)loss;
    while (deficit > 0 && k < 40) { int i = order[k % 7]; if (d[i] > 0) { d[i]--; deficit--; } ++k; }
    static const char* ver[3] = {"8.1", "8.2", "8.3"};
    insertMoca(c.id, day, d, bonus, ver[(day / 7) % 3], 11 + (int)(std::fabs(U(rng)) * 6), "");
}

// ---------------------------------------------------------------- loading
std::vector<LabItem> Engine::loadLabs(int crew) {
    Crew* c = crewById(crew);
    std::map<std::string, LabItem> by;
    db_->prep("SELECT bio,day,value FROM labs WHERE crew=? ORDER BY day,id").i(crew).each([&](Db::Stmt& s) {
        std::string b = s.gt(0);
        const Marker* m = cat_.find(b);
        if (!m) return;
        auto& it = by[b];
        it.m = m;
        it.hist.push_back({(int)s.gi(1), s.gd(2)});
    });
    std::vector<LabItem> out;
    for (auto& kv : by) { kv.second.cls = Catalog::classify(*kv.second.m, c ? c->sex : 0, kv.second.hist.back().second); out.push_back(kv.second); }
    std::sort(out.begin(), out.end(), [](const LabItem& a, const LabItem& b) {
        if (a.m->cluster != b.m->cluster) return a.m->cluster < b.m->cluster;
        return a.m->name < b.m->name;
    });
    return out;
}

std::vector<MindEntry> Engine::loadMind(int crew) {
    std::vector<MindEntry> v;
    db_->prep("SELECT day,mood,stress,anxiety,fatigue,isolation,sleep,pvt FROM mind WHERE crew=? ORDER BY day,id").i(crew).each([&](Db::Stmt& s) {
        v.push_back({(int)s.gi(0), s.gd(1), s.gd(2), s.gd(3), s.gd(4), s.gd(5), s.gd(6), s.gd(7)});
    });
    return v;
}

std::vector<PainEntry> Engine::loadPain(int crew, bool onlyActive) {
    std::vector<PainEntry> v;
    db_->prep(std::string("SELECT id,day,view,zone,x,y,intensity,kind,duration_h,note,active FROM pain WHERE crew=?") +
              (onlyActive ? " AND active=1" : "") + " ORDER BY day DESC,id DESC").i(crew).each([&](Db::Stmt& s) {
        v.push_back({s.gi(0), (int)s.gi(1), s.gt(2), s.gt(3), s.gt(7), s.gt(9), s.gd(4), s.gd(5), (int)s.gi(6), s.gd(8), s.gi(10) != 0});
    });
    return v;
}

std::vector<MocaEntry> Engine::loadMoca(int crew) {
    std::vector<MocaEntry> v;
    db_->prep("SELECT id,day,visuo,naming,attention,language,abstraction,recall,orient,bonus,total,version,note,minutes FROM moca WHERE crew=? ORDER BY day,id").i(crew).each([&](Db::Stmt& s) {
        v.push_back({s.gi(0), (int)s.gi(1), (int)s.gi(2), (int)s.gi(3), (int)s.gi(4), (int)s.gi(5), (int)s.gi(6), (int)s.gi(7), (int)s.gi(8),
                     (int)s.gi(9), (int)s.gi(10), s.gt(11), s.gt(12), (int)s.gi(13)});
    });
    return v;
}

MocaInfo Engine::mocaInfo(const std::vector<MocaEntry>& v) const {
    MocaInfo m;
    if (!v.empty()) {
        m.has = true; m.lastDay = v.back().day; m.lastTotal = v.back().total;
        m.daysSince = day_ - m.lastDay; m.dueIn = MOCA_INTERVAL - m.daysSince; m.overdue = m.daysSince >= MOCA_INTERVAL;
    }
    return m;
}

double Engine::mindScore(const MindEntry& e) {
    double avg = (e.mood + (11 - e.stress) + (11 - e.anxiety) + (11 - e.fatigue) + (11 - e.isolation) + e.sleep) / 6.0;
    return clampd((avg - 1) / 9.0 * 100.0, 0, 100);
}

Health Engine::health(const Crew&, const std::vector<LabItem>& labs, const std::vector<MindEntry>& mind, const std::vector<PainEntry>& pain,
                      const std::vector<MocaEntry>& moca) {
    Health h{};
    double sum[5] = {0}; int cnt[5] = {0};
    for (auto& l : labs) {
        int ci = std::max(0, std::min(4, l.m->cluster - 1));
        sum[ci] += Catalog::score(l.cls.severity); cnt[ci]++;
        h.total++;
        if (l.cls.severity == SEV_CRIT) h.danger++;
        else if (l.cls.severity == SEV_WATCH) h.concern++;
    }
    for (int i = 0; i < 5; ++i) h.systems[i] = cnt[i] ? sum[i] / cnt[i] : 100.0;
    double ms = 100;
    if (!mind.empty()) {
        double a = 0; int n = 0;
        for (int i = (int)mind.size() - 1; i >= 0 && n < 3; --i, ++n) a += mindScore(mind[i]);
        ms = a / n;
    }
    if (!moca.empty()) ms = moca.back().total / 30.0 * 100.0 * 0.3 + ms * 0.7;  // MoCA contributes 30% of the Mind system
    h.systems[5] = ms;
    int maxp = 0, np = 0;
    for (auto& p : pain) if (p.active) { maxp = std::max(maxp, p.intensity); np++; }
    h.systems[6] = np ? clampd(100 - 6.0 * maxp - 2.0 * (np - 1), 0, 100) : 100.0;
    double mean = 0, mn = 100;
    for (double s : h.systems) { mean += s / 7; mn = std::min(mn, s); }
    h.overall = 0.75 * mean + 0.25 * mn;
    h.overall -= 6.0 * std::min(h.danger, 3);
    if (h.danger > 0 || maxp >= 8 || ms < 30) h.status = SEV_CRIT;
    else if (h.concern > 0 || ms < 55 || maxp >= 6 || mocaInfo(moca).overdue) h.status = SEV_WATCH;
    return h;
}

// ---------------------------------------------------------------- advisor
std::vector<Advice> Engine::runAdvisor(const Crew& c, const std::vector<LabItem>& labs, const std::vector<MindEntry>& mind,
                                       const std::vector<PainEntry>& pain, const std::vector<MocaEntry>& moca) {
    std::vector<Advice> out;
    auto L = [&](const char* key) -> const LabItem* {
        const Marker* m = cat_.search(key);
        if (!m) return nullptr;
        for (auto& l : labs) if (l.m == m) return &l;
        return nullptr;
    };
    auto sv = [&](const LabItem* l) { return l ? l->cls.severity : 0; };
    auto line = [&](const LabItem* l) {
        double v = l->hist.back().second;
        return l->m->name + ": " + fmt(v) + " " + l->m->unit + " (" + sevName(l->cls.severity) + ")";
    };
    auto painIn = [&](std::initializer_list<const char*> zs, int minI = 1) -> const PainEntry* {
        for (auto& p : pain) if (p.active && p.intensity >= minI)
            for (auto z : zs) if (p.zone.find(z) != std::string::npos) return &p;
        return nullptr;
    };
    auto gather = [&](std::initializer_list<const char*> keys, std::vector<const LabItem*>& hits, int& mx) {
        mx = 0;
        for (auto k : keys) { auto* l = L(k); if (l && l->cls.severity >= SEV_WATCH) { hits.push_back(l); mx = std::max(mx, l->cls.severity); } }
    };
    auto push = [&](int sev, const char* title, const char* sys, std::vector<const LabItem*>& hits, std::vector<std::string> extraWhy, std::vector<std::string> actions) {
        Advice a; a.sev = sev; a.title = title; a.system = sys;
        for (auto h : hits) a.why.push_back(line(h));
        for (auto& e : extraWhy) a.why.push_back(e);
        a.actions = std::move(actions);
        out.push_back(std::move(a));
    };

    std::vector<const LabItem*> h; int mx;

    h.clear(); gather({"retinal nerve", "optic nerve sheath", "spherical equivalent", "intraocular", "estimated intracranial"}, h, mx);
    if (!h.empty()) {
        std::vector<std::string> extra;
        if (auto* p = painIn({"Head"})) extra.push_back("Active head pain " + std::to_string(p->intensity) + "/10: " + p->note);
        int sev = (h.size() >= 2 || extra.size()) ? std::max(mx, (int)SEV_WATCH) : mx;
        if (h.size() >= 3 && mx >= 1) sev = SEV_CRIT;
        push(sev, "Spaceflight-associated neuro-ocular syndrome (SANS) pattern", SYS[2], h, extra,
             {"Repeat OCT (RNFL) and ONSD ultrasound within 7 days; log Frisen grade.",
              "Schedule LBNP / lower-body negative pressure sessions; limit prolonged head-down sleep posture.",
              "Monitor cabin CO2 (target < 3 mmHg); consider vision-correcting lenses for near-vision shift.",
              "Escalate to flight surgeon at next comm window if RNFL or ONSD keeps rising."});
    }
    h.clear(); gather({"n-telopeptide", "c-telopeptide", "vitamin d", "parathyroid", "ionized calcium"}, h, mx);
    if (sv(L("n-telopeptide")) + sv(L("c-telopeptide")) >= 1) {
        std::vector<std::string> extra;
        if (auto* p = painIn({"Back", "Pelvis"})) extra.push_back("Active " + p->zone + " pain " + std::to_string(p->intensity) + "/10");
        push(std::max(mx, (int)SEV_WATCH), "Accelerated bone resorption", SYS[1], h, extra,
             {"Increase resistive exercise (ARED) to 6 days/week; log load progression.",
              "Ensure vitamin D 800-2000 IU/day and calcium intake per nutrition plan.",
              "Repeat NTX/CTX in 2 weeks; consider anti-resorptive countermeasure per medical protocol."});
    }
    h.clear(); gather({"urine output", "urinary calcium", "urinary citrate", "supersaturation"}, h, mx);
    if (h.size() >= 2) {
        push(std::max(mx, (int)SEV_WATCH), "Kidney-stone risk (urine chemistry)", SYS[3], h, {},
             {"Drink to maintain urine output above 2.0 L/day.", "Reduce sodium; consider potassium citrate per protocol.",
              "Re-check urine chemistry weekly; ultrasound kidneys if flank pain appears."});
    }
    if (sv(L("creatine kinase")) >= 1) {
        h.clear(); h.push_back(L("creatine kinase"));
        int sev = sv(L("creatine kinase"));
        if (sv(L("serum creatinine")) >= 1) { h.push_back(L("serum creatinine")); sev = SEV_CRIT; }
        push(sev, "Muscle damage / rhabdomyolysis screen", SYS[1], h, {},
             {"Pause ARED / high-load exercise for 48 h and hydrate.", "Check creatinine and urine colour; repeat CK in 24 h."});
    }
    h.clear(); gather({"qtc", "premature ventricular", "sdnn", "ejection fraction"}, h, mx);
    if (!h.empty()) {
        std::vector<std::string> extra;
        if (auto* p = painIn({"Chest"})) { extra.push_back("Chest pain " + std::to_string(p->intensity) + "/10"); mx = SEV_CRIT; }
        push(mx, "Cardiac rhythm / function alert", SYS[0], h, extra,
             {"Check electrolytes (ionized Ca, K, Mg) with i-STAT.", "Avoid QT-prolonging medication; 12-lead ECG now.",
              "Hold EVA and heavy exercise until reviewed by Medical Officer."});
    }
    h.clear(); gather({"peak venous", "ijv cross", "d-dimer"}, h, mx);
    if (!h.empty()) {
        std::vector<std::string> extra;
        if (auto* p = painIn({"Neck", "Head"})) { extra.push_back(p->zone + " pain " + std::to_string(p->intensity) + "/10: " + p->note); mx = std::max(mx, (int)SEV_WATCH); }
        int sev = (h.size() >= 2 && mx == SEV_CRIT) ? SEV_CRIT : std::max(mx, (int)SEV_WATCH);
        push(sev, "Possible internal jugular vein thrombosis / venous stasis", SYS[0], h, extra,
             {"URGENT: repeat IJV Doppler now and document flow direction and clot presence.",
              "Follow onboard anticoagulation protocol; do not start without Medical Officer confirmation.",
              "Avoid neck massage and vigorous Valsalva; keep crew member under hourly observation.",
              "Prepare medical evacuation readiness checklist."});
    }
    h.clear(); gather({"hemoglobin", "hematocrit", "exhaled endogenous", "haptoglobin"}, h, mx);
    if (!h.empty()) push(mx, "Space anemia / hemolysis pattern", SYS[0], h, {},
                          {"Repeat CBC in 7 days; review iron (ferritin) and folate status.", "Check for fatigue and exertional dyspnea at exercise."});
    h.clear(); gather({"psychomotor", "sleep duration", "dim-light"}, h, mx);
    {
        std::vector<std::string> extra;
        bool fat = !mind.empty() && mind.back().fatigue >= 7;
        if (fat) extra.push_back("Self-reported fatigue " + fmt(mind.back().fatigue) + "/10");
        if (!h.empty() && (h.size() >= 2 || fat))
            push(std::max(mx, (int)SEV_WATCH), "Fatigue and circadian misalignment risk", SYS[2], h, extra,
                 {"Protect 8 h sleep window; dim blue light 2 h before sleep.", "Postpone EVA / critical operations if PVT > 350 ms.",
                  "Re-run PVT in 24 h."});
    }
    h.clear(); gather({"morning salivary", "interleukin", "herpesvirus", "cd4", "neutrophil-to"}, h, mx);
    if (!h.empty()) {
        std::vector<std::string> extra;
        if (!mind.empty() && mind.back().stress >= 6.5) extra.push_back("Self-reported stress " + fmt(mind.back().stress) + "/10");
        if (h.size() >= 2 || !extra.empty())
            push(std::max(mx, (int)SEV_WATCH), "Chronic stress / immune dysregulation", SYS[4], h, extra,
                 {"Schedule recovery time and exercise; review workload with the Commander.", "Check for rash or latent virus symptoms; re-test in 2 weeks."});
    }
    h.clear(); gather({"h2ax", "8-ohdg"}, h, mx);
    if (!h.empty()) push(mx, "Radiation exposure / DNA damage marker elevated", SYS[4], h, {},
                          {"Check dosimeter log and solar particle event alerts.", "Shelter in storm shelter during SPE; add antioxidant-rich diet."});
    h.clear(); gather({"glucose", "homa", "triglycer", "alanine"}, h, mx);
    if (!h.empty()) push(mx, "Metabolic / liver strain", SYS[3], h, {}, {"Review diet and caloric balance; repeat panel in 2 weeks."});
    h.clear(); gather({"multifidus", "disc height"}, h, mx);
    if (auto* p = painIn({"Back"}); p && !h.empty())
        push(SEV_WATCH, "Spinal deconditioning with back pain", SYS[1], h, {"Back pain " + std::to_string(p->intensity) + "/10, since day " + std::to_string(p->day)},
             {"Core-stabilisation exercise programme; avoid heavy axial loading for 72 h.", "Re-measure multifidus area and disc height in 2 weeks."});
    if (!mind.empty()) {
        int n = (int)mind.size();
        auto avg = [&](int from, int to, double MindEntry::*f) { double s = 0; int k = 0; for (int i = std::max(0, from); i < to && i < n; ++i, ++k) s += mind[i].*f; return k ? s / k : 0.0; };
        double st = avg(n - 3, n, &MindEntry::stress), an = avg(n - 3, n, &MindEntry::anxiety), mo = avg(n - 3, n, &MindEntry::mood), iso = avg(n - 3, n, &MindEntry::isolation);
        double st0 = avg(n - 14, n - 11, &MindEntry::stress);
        std::vector<std::string> why;
        int sev = 0;
        if (st >= 6.5) { why.push_back("Stress " + fmt(st) + "/10 (3-day average)"); sev = 1; }
        if (an >= 6) { why.push_back("Anxiety " + fmt(an) + "/10"); sev = 1; }
        if (mo <= 4.5) { why.push_back("Low mood " + fmt(mo) + "/10"); sev = 1; }
        if (iso >= 6) { why.push_back("Isolation / confinement " + fmt(iso) + "/10"); sev = 1; }
        if (st - st0 >= 2) why.push_back("Stress up " + fmt(st - st0) + " points over two weeks");
        if (st >= 8.5 || mo <= 2.5) sev = 2;
        if (sev) {
            Advice a; a.sev = sev; a.title = "Psychological strain trend"; a.system = SYS[5]; a.why = why;
            a.actions = {"Offer private conversation with Medical Officer / psychological support protocol.",
                         "Schedule personal downtime, family-video slot (stored message) and exercise.", "Re-run Mental Check-in daily for the next 5 days."};
            out.push_back(a);
        }
    }
    // MoCA: weekly cognitive screening is mandatory
    {
        MocaInfo mi = mocaInfo(moca);
        if (mi.overdue) {
            Advice a; a.system = SYS[5];
            a.sev = (mi.daysSince >= 14 && mi.has) ? SEV_CRIT : SEV_WATCH;
            a.title = mi.has ? "Weekly MoCA overdue" : "Baseline MoCA not yet recorded";
            a.why = {mi.has ? ("Last MoCA " + std::to_string(mi.lastTotal) + "/30 on day " + std::to_string(mi.lastDay) + ", " + std::to_string(mi.daysSince) + " days ago")
                            : "No Montreal Cognitive Assessment on file for " + c.name,
                     "Protocol requires one MoCA every 7 days for every crew member"};
            a.actions = {"Open the MoCA screen and complete the assessment today (use the alternate version).",
                         "If the crew member cannot test, record the reason in the mission log."};
            out.push_back(a);
        }
        if (!moca.empty()) {
            int last = moca.back().total;
            std::vector<std::string> why;
            int sev = 0;
            if (last < 26) { why.push_back("Last MoCA " + std::to_string(last) + "/30 is in the " + mocaClass(last)); sev = last < 18 ? SEV_CRIT : SEV_WATCH; }
            if (moca.size() >= 3) {
                int n = (int)moca.size() - 1, from = std::max(0, n - 4);
                std::vector<int> prev;
                for (int i = from; i < n; ++i) prev.push_back(moca[i].total);
                std::sort(prev.begin(), prev.end());
                double base = prev[prev.size() / 2];
                if (base - last >= 3) { why.push_back("Down " + fmt(base - last) + " points from personal baseline (" + fmt(base) + ")"); sev = std::max(sev, (int)SEV_WATCH); }
            }
            if (sev) {
                Advice a; a.sev = sev; a.title = "Cognitive screening decline (MoCA)"; a.system = SYS[5]; a.why = why;
                a.actions = {"Repeat MoCA within 48 h with the alternate version in a quiet setting.",
                             "Rule out sleep loss, elevated cabin CO2, illness and sedating medication; review PVT and sleep markers.",
                             "Limit safety-critical tasks (EVA, docking, robotics) until reviewed by the Medical Officer if the score is below 18."};
                out.push_back(a);
            }
        }
    }
    for (auto& p : pain) if (p.active && p.intensity >= 7) {
        Advice a; a.sev = SEV_CRIT; a.title = "Severe pain: " + p.zone; a.system = SYS[6];
        a.why = {p.zone + " pain " + std::to_string(p.intensity) + "/10 (" + p.kind + "), " + fmt(p.duration_h) + " h"};
        a.actions = {"Medical Officer assessment now.", "Analgesia per onboard formulary; reassess in 1 hour."};
        out.push_back(a);
    }
    if (out.empty()) out.push_back({SEV_OK, "All monitored systems within space-adapted ranges", "All", {"No threshold crossed in the latest samples."}, {"Continue nominal monitoring schedule."}});
    std::sort(out.begin(), out.end(), [](const Advice& a, const Advice& b) { return a.sev > b.sev; });
    return out;
}

// ---------------------------------------------------------------- API json
static json crewJson(const Crew& c) {
    return {{"id", c.id}, {"name", c.name}, {"role", c.role}, {"sex", c.sex}, {"age", c.age}, {"height", c.height}, {"weight", c.weight},
            {"edu", c.edu}, {"notes", c.notes}, {"scenario", c.scenario}};
}

json Engine::state() {
    std::lock_guard<std::mutex> g(mu_);
    json crew = json::array();
    for (auto& c : crew_) {
        auto labs = loadLabs(c.id); auto mind = loadMind(c.id); auto pain = loadPain(c.id, false); auto moca = loadMoca(c.id);
        Health h = health(c, labs, mind, pain, moca);
        auto adv = runAdvisor(c, labs, mind, pain, moca);
        int alerts = 0; for (auto& a : adv) if (a.sev > 0) alerts++;
        MocaInfo mi = mocaInfo(moca);
        json cj = crewJson(c);
        cj["index"] = h.overall; cj["status"] = h.status; cj["alerts"] = alerts; cj["danger"] = h.danger; cj["concern"] = h.concern; cj["total"] = h.total;
        cj["systems"] = std::vector<double>(h.systems, h.systems + 7);
        cj["has_data"] = (h.total > 0 || !mind.empty() || !moca.empty());
        cj["moca"] = {{"has", mi.has}, {"last_day", mi.lastDay}, {"last_total", mi.lastTotal}, {"days_since", mi.has ? mi.daysSince : -1},
                      {"due_in", mi.dueIn}, {"overdue", mi.overdue}};
        crew.push_back(cj);
    }
    json sys = json::array();
    for (auto s : SYS) sys.push_back(s);
    json m = {{"name", mission_.name}, {"vehicle", mission_.vehicle}, {"type", mission_.type}, {"duration", mission_.duration}, {"simulated", mission_.simulated},
              {"profile_file", fs::path(profileFile_).filename().string()}};
    return {{"day", day_}, {"mission_days", mission_.duration}, {"comms", "OFFLINE"}, {"mission", m}, {"crew", crew}, {"system_names", sys}};
}

json Engine::catalogJson() { std::lock_guard<std::mutex> g(mu_); return cat_.toJson(); }

json Engine::labs(int crew) {
    std::lock_guard<std::mutex> g(mu_);
    json out = json::array();
    for (auto& l : loadLabs(crew)) {
        json hist = json::array();
        size_t from = l.hist.size() > 24 ? l.hist.size() - 24 : 0;
        for (size_t i = from; i < l.hist.size(); ++i) hist.push_back({l.hist[i].first, l.hist[i].second});
        out.push_back({{"id", l.m->id}, {"value", l.hist.back().second}, {"day", l.hist.back().first},
                       {"sev", l.cls.severity}, {"earth_abnormal", l.cls.earth_abnormal}, {"hist", hist}});
    }
    return out;
}

json Engine::addLab(int crew, const std::string& bio, double value) {
    std::lock_guard<std::mutex> g(mu_);
    Crew* c = crewById(crew); const Marker* m = cat_.find(bio);
    if (!c || !m) return {{"error", "unknown crew or marker"}};
    insertLab(crew, bio, value, day_);
    auto cl = Catalog::classify(*m, c->sex, value);
    if (cl.severity >= 1) addEvent(day_, crew, cl.severity, c->name + ": " + m->name + " " + fmt(value) + " " + m->unit + " -> " + sevName(cl.severity));
    autosave();
    return {{"sev", cl.severity}, {"earth_abnormal", cl.earth_abnormal}};
}

json Engine::mind(int crew) {
    std::lock_guard<std::mutex> g(mu_);
    json hist = json::array();
    auto v = loadMind(crew);
    size_t from = v.size() > 60 ? v.size() - 60 : 0;
    for (size_t i = from; i < v.size(); ++i) {
        auto& e = v[i];
        hist.push_back({{"day", e.day}, {"mood", e.mood}, {"stress", e.stress}, {"anxiety", e.anxiety}, {"fatigue", e.fatigue},
                        {"isolation", e.isolation}, {"sleep", e.sleep}, {"pvt", e.pvt}, {"score", mindScore(e)}});
    }
    return hist;
}

json Engine::addMind(int crew, const json& b) {
    std::lock_guard<std::mutex> g(mu_);
    if (!crewById(crew)) return {{"error", "unknown crew"}};
    auto num = [&](const char* k, double d) { return b.contains(k) && b[k].is_number() ? clampd(b[k].get<double>(), 0, 1000) : d; };
    db_->prep("INSERT INTO mind(crew,day,mood,stress,anxiety,fatigue,isolation,sleep,pvt) VALUES(?,?,?,?,?,?,?,?,?)")
        .i(crew).i(day_).d(num("mood", 5)).d(num("stress", 5)).d(num("anxiety", 5)).d(num("fatigue", 5)).d(num("isolation", 5)).d(num("sleep", 5)).d(num("pvt", 0)).run();
    autosave();
    return {{"ok", true}};
}

json Engine::pain(int crew) {
    std::lock_guard<std::mutex> g(mu_);
    json out = json::array();
    for (auto& p : loadPain(crew, false))
        out.push_back({{"id", p.id}, {"day", p.day}, {"view", p.view}, {"zone", p.zone}, {"x", p.x}, {"y", p.y}, {"intensity", p.intensity},
                       {"kind", p.kind}, {"duration_h", p.duration_h}, {"note", p.note}, {"active", p.active}});
    return out;
}

json Engine::addPain(int crew, const json& b) {
    std::lock_guard<std::mutex> g(mu_);
    Crew* c = crewById(crew);
    if (!c) return {{"error", "unknown crew"}};
    int inten = (int)clampd(b.value("intensity", 3), 1, 10);
    std::string zone = b.value("zone", "Unknown");
    insertPain(crew, day_, b.value("view", "front"), zone, b.value("x", 50.0), b.value("y", 50.0), inten,
               b.value("kind", "Dull ache"), b.value("duration_h", 1.0), b.value("note", ""));
    addEvent(day_, crew, inten >= 7 ? 2 : 1, c->name + ": pain reported " + zone + " " + std::to_string(inten) + "/10");
    autosave();
    return {{"ok", true}};
}

json Engine::resolvePain(int crew, long long pid) {
    std::lock_guard<std::mutex> g(mu_);
    db_->prep("UPDATE pain SET active=0 WHERE id=? AND crew=?").i(pid).i(crew).run();
    autosave();
    return {{"ok", true}};
}

json Engine::moca(int crew) {
    std::lock_guard<std::mutex> g(mu_);
    json entries = json::array();
    auto v = loadMoca(crew);
    for (auto& e : v)
        entries.push_back({{"id", e.id}, {"day", e.day}, {"visuospatial", e.visuo}, {"naming", e.naming}, {"attention", e.attention}, {"language", e.language},
                           {"abstraction", e.abstraction}, {"delayed_recall", e.recall}, {"orientation", e.orient}, {"bonus", e.bonus}, {"total", e.total},
                           {"version", e.version}, {"minutes", e.minutes}, {"note", e.note}, {"class", mocaClass(e.total)}});
    MocaInfo mi = mocaInfo(v);
    static const char* ver[3] = {"8.1", "8.2", "8.3"};
    Crew* c = crewById(crew);
    json dom = json::array();
    for (int i = 0; i < 7; ++i) dom.push_back({{"key", MOCA_KEYS[i]}, {"max", MOCA_MAX[i]}});
    return {{"entries", entries}, {"domains", dom}, {"next_version", ver[v.size() % 3]}, {"bonus", (c && c->edu <= 12) ? 1 : 0},
            {"interval_days", MOCA_INTERVAL},
            {"schedule", {{"has", mi.has}, {"last_day", mi.lastDay}, {"last_total", mi.lastTotal}, {"days_since", mi.has ? mi.daysSince : -1},
                          {"due_in", mi.dueIn}, {"overdue", mi.overdue}}}};
}

json Engine::addMoca(int crew, const json& b) {
    std::lock_guard<std::mutex> g(mu_);
    Crew* c = crewById(crew);
    if (!c) return {{"error", "unknown crew"}};
    int d[7];
    for (int i = 0; i < 7; ++i) d[i] = (int)clampd(b.value(MOCA_KEYS[i], 0), 0, MOCA_MAX[i]);
    int bonus = c->edu <= 12 ? 1 : 0;
    int prevTotal = -1;
    auto prev = loadMoca(crew);
    if (!prev.empty()) prevTotal = prev.back().total;
    insertMoca(crew, day_, d, bonus, b.value("version", "8.1"), (int)b.value("minutes", 10), b.value("note", ""));
    int total = std::min(30, d[0] + d[1] + d[2] + d[3] + d[4] + d[5] + d[6] + bonus);
    if (total < 26) addEvent(day_, crew, total < 18 ? 2 : 1, c->name + ": MoCA " + std::to_string(total) + "/30 (" + mocaClass(total) + ")");
    else addEvent(day_, crew, 0, c->name + ": MoCA completed " + std::to_string(total) + "/30");
    autosave();
    return {{"total", total}, {"bonus", bonus}, {"class", mocaClass(total)}, {"delta", prevTotal < 0 ? 0 : total - prevTotal}};
}

json Engine::advice(int crew) {
    std::lock_guard<std::mutex> g(mu_);
    Crew* c = crewById(crew);
    json out = json::array();
    if (!c) return out;
    for (auto& a : runAdvisor(*c, loadLabs(crew), loadMind(crew), loadPain(crew, true), loadMoca(crew)))
        out.push_back({{"sev", a.sev}, {"title", a.title}, {"system", a.system}, {"why", a.why}, {"actions", a.actions}});
    return out;
}

json Engine::advance(int days) {
    std::lock_guard<std::mutex> g(mu_);
    db_->begin();
    for (int i = 0; i < days; ++i) {
        ++day_;
        if (mission_.simulated) simulateDay(day_);
    }
    db_->commit();
    autosave();
    return {{"day", day_}, {"simulated", mission_.simulated}};
}

json Engine::events(int limit) {
    std::lock_guard<std::mutex> g(mu_);
    json out = json::array();
    db_->prep("SELECT day,crew,sev,text FROM events ORDER BY id DESC LIMIT ?").i(limit).each([&](Db::Stmt& s) {
        Crew* c = crewById((int)s.gi(1));
        out.push_back({{"day", s.gi(0)}, {"crew", c ? c->name : "?"}, {"sev", s.gi(2)}, {"text", s.gt(3)}});
    });
    return out;
}

std::string Engine::report(int crewId) {
    std::lock_guard<std::mutex> g(mu_);
    Crew* c = crewById(crewId);
    if (!c) return "unknown crew member";
    auto labs = loadLabs(crewId); auto mind = loadMind(crewId); auto pain = loadPain(crewId, true); auto moca = loadMoca(crewId);
    Health h = health(*c, labs, mind, pain, moca);
    MocaInfo mi = mocaInfo(moca);
    std::ostringstream o;
    o << "AIChE HEALTH - MEDICAL SUMMARY (stored locally - transmit at next comm window)\n";
    o << "===========================================================================\n";
    o << "Mission     : " << mission_.name << (mission_.vehicle.empty() ? "" : " / " + mission_.vehicle) << "\n";
    o << "Crew member : " << c->name << " (" << c->role << ", " << (c->sex ? "F" : "M") << ", " << c->age << " y)\n";
    o << "Mission day : " << day_ << " / " << mission_.duration << "\n";
    o << "Health index: " << (int)std::round(h.overall) << " / 100   Status: " << (h.status == 2 ? "CRITICAL" : h.status == 1 ? "WATCH" : "NOMINAL") << "\n\n";
    o << "SYSTEM SCORES\n";
    for (int i = 0; i < 7; ++i) o << "  " << SYS[i] << ": " << (int)std::round(h.systems[i]) << "\n";
    o << "\nMoCA (weekly cognitive screening)\n";
    if (!mi.has) o << "  no assessment on file - OVERDUE\n";
    else o << "  last: " << mi.lastTotal << "/30 (" << mocaClass(mi.lastTotal) << ") on day " << mi.lastDay << ", " << mi.daysSince << " d ago"
           << (mi.overdue ? " - OVERDUE" : "") << "\n";
    o << "\nBIOMARKERS OUT OF SPACE-NORMAL RANGE (" << h.danger << " dangerous, " << h.concern << " concerning of " << h.total << ")\n";
    for (auto& l : labs) if (l.cls.severity > 0)
        o << "  [" << sevName(l.cls.severity) << "] " << l.m->name << " = " << fmt(l.hist.back().second) << " " << l.m->unit << " (day " << l.hist.back().first << ")\n";
    o << "\nACTIVE PAIN REPORTS\n";
    if (pain.empty()) o << "  none\n";
    for (auto& p : pain) o << "  " << p.zone << " " << p.intensity << "/10, " << p.kind << ", since day " << p.day << (p.note.empty() ? "" : " - " + p.note) << "\n";
    o << "\nAUTONOMOUS ADVISOR\n";
    for (auto& a : runAdvisor(*c, labs, mind, pain, moca)) {
        o << "  [" << (a.sev == 2 ? "CRITICAL" : a.sev == 1 ? "WATCH" : "OK") << "] " << a.title << "\n";
        for (auto& w : a.why) o << "      - " << w << "\n";
        for (auto& x : a.actions) o << "      > " << x << "\n";
    }
    o << "\nDecision-support prototype - not a clinical device.\n";
    return o.str();
}

// ---------------------------------------------------------------- mission / crew / profiles
json Engine::mission() {
    std::lock_guard<std::mutex> g(mu_);
    return {{"name", mission_.name}, {"vehicle", mission_.vehicle}, {"type", mission_.type}, {"duration", mission_.duration},
            {"simulated", mission_.simulated}, {"day", day_}, {"profile_file", fs::path(profileFile_).filename().string()}, {"profiles_dir", profilesDir_}};
}

json Engine::updateMission(const json& b) {
    std::lock_guard<std::mutex> g(mu_);
    if (b.contains("name")) mission_.name = b["name"].get<std::string>();
    if (b.contains("vehicle")) mission_.vehicle = b["vehicle"].get<std::string>();
    if (b.contains("type")) mission_.type = b["type"].get<std::string>();
    if (b.contains("duration")) mission_.duration = std::max(1, (int)b["duration"].get<double>());
    if (b.contains("day")) day_ = std::max(1, (int)b["day"].get<double>());
    if (b.contains("simulated")) mission_.simulated = b["simulated"].get<bool>();
    autosave();
    return {{"ok", true}};
}

json Engine::addCrew(const json& b) {
    std::lock_guard<std::mutex> g(mu_);
    Crew c;
    c.id = nextCrewId_++;
    c.name = b.value("name", std::string("New crew member"));
    c.role = b.value("role", std::string("Mission Specialist"));
    c.sex = b.value("sex", 0) ? 1 : 0;
    c.age = b.value("age", 35);
    c.height = b.value("height", 175.0); c.weight = b.value("weight", 75.0); c.edu = b.value("edu", 16);
    c.notes = b.value("notes", std::string()); c.scenario = b.value("scenario", std::string());
    crew_.push_back(c);
    addEvent(day_, c.id, 0, c.name + " added to the crew");
    autosave();
    return {{"id", c.id}};
}

json Engine::updateCrew(int id, const json& b) {
    std::lock_guard<std::mutex> g(mu_);
    Crew* c = crewById(id);
    if (!c) return {{"error", "unknown crew"}};
    if (b.contains("name")) c->name = b["name"].get<std::string>();
    if (b.contains("role")) c->role = b["role"].get<std::string>();
    if (b.contains("sex")) c->sex = b["sex"].get<int>() ? 1 : 0;
    if (b.contains("age")) c->age = (int)clampd(b["age"].get<double>(), 18, 90);
    if (b.contains("height")) c->height = clampd(b["height"].get<double>(), 100, 230);
    if (b.contains("weight")) c->weight = clampd(b["weight"].get<double>(), 30, 200);
    if (b.contains("edu")) c->edu = (int)clampd(b["edu"].get<double>(), 0, 40);
    if (b.contains("notes")) c->notes = b["notes"].get<std::string>();
    if (b.contains("scenario")) c->scenario = b["scenario"].get<std::string>();
    autosave();
    return {{"ok", true}};
}

json Engine::deleteCrew(int id) {
    std::lock_guard<std::mutex> g(mu_);
    Crew* c = crewById(id);
    if (!c) return {{"error", "unknown crew"}};
    for (const char* t : {"labs", "mind", "pain", "moca", "events"})
        db_->prep(std::string("DELETE FROM ") + t + " WHERE crew=?").i(id).run();
    crew_.erase(std::remove_if(crew_.begin(), crew_.end(), [&](const Crew& x) { return x.id == id; }), crew_.end());
    autosave();
    return {{"ok", true}};
}

json Engine::exportProfile() {
    json j;
    j["format"] = "aiche-health-profile";
    j["version"] = 1;
    j["mission"] = {{"name", mission_.name}, {"vehicle", mission_.vehicle}, {"type", mission_.type}, {"duration_days", mission_.duration},
                    {"day", day_}, {"simulated", mission_.simulated}};
    json crew = json::array();
    for (auto& c : crew_) {
        json cj = {{"id", c.id}, {"name", c.name}, {"role", c.role}, {"sex", c.sex ? "F" : "M"}, {"age", c.age}, {"height_cm", c.height},
                   {"weight_kg", c.weight}, {"education_years", c.edu}, {"notes", c.notes}, {"scenario", c.scenario}};
        json labs = json::array(), mind = json::array(), pain = json::array(), moca = json::array();
        db_->prep("SELECT bio,day,value FROM labs WHERE crew=? ORDER BY day,id").i(c.id).each([&](Db::Stmt& s) {
            labs.push_back({{"bio", s.gt(0)}, {"day", s.gi(1)}, {"value", std::round(s.gd(2) * 10000) / 10000}});
        });
        for (auto& e : loadMind(c.id))
            mind.push_back({{"day", e.day}, {"mood", std::round(e.mood * 100) / 100}, {"stress", std::round(e.stress * 100) / 100}, {"anxiety", std::round(e.anxiety * 100) / 100},
                            {"fatigue", std::round(e.fatigue * 100) / 100}, {"isolation", std::round(e.isolation * 100) / 100}, {"sleep", std::round(e.sleep * 100) / 100},
                            {"pvt_ms", std::round(e.pvt)}});
        for (auto& p : loadPain(c.id, false))
            pain.push_back({{"day", p.day}, {"view", p.view}, {"zone", p.zone}, {"x", p.x}, {"y", p.y}, {"intensity", p.intensity}, {"kind", p.kind},
                            {"duration_h", p.duration_h}, {"note", p.note}, {"active", p.active}});
        for (auto& m : loadMoca(c.id))
            moca.push_back({{"day", m.day}, {"visuospatial", m.visuo}, {"naming", m.naming}, {"attention", m.attention}, {"language", m.language},
                            {"abstraction", m.abstraction}, {"delayed_recall", m.recall}, {"orientation", m.orient}, {"version", m.version},
                            {"minutes", m.minutes}, {"note", m.note}});
        cj["data"] = {{"labs", labs}, {"mind", mind}, {"pain", pain}, {"moca", moca}};
        crew.push_back(cj);
    }
    j["crew"] = crew;
    json ev = json::array();
    db_->prep("SELECT day,crew,sev,text FROM events ORDER BY id").each([&](Db::Stmt& s) {
        ev.push_back({{"day", s.gi(0)}, {"crew", s.gi(1)}, {"sev", s.gi(2)}, {"text", s.gt(3)}});
    });
    j["events"] = ev;
    return j;
}

bool Engine::importProfile(const json& j, std::string& err) {
    if (!j.is_object() || !j.contains("crew") || !j["crew"].is_array()) { err = "not an AIChE Health profile (missing crew array)"; return false; }
    resetStore();
    mission_ = Mission();
    if (j.contains("mission") && j["mission"].is_object()) {
        auto& m = j["mission"];
        mission_.name = m.value("name", std::string("Untitled mission"));
        mission_.vehicle = m.value("vehicle", std::string());
        mission_.type = m.value("type", std::string("Space station"));
        mission_.duration = m.value("duration_days", 180);
        mission_.simulated = m.value("simulated", false);
        day_ = std::max(1, m.value("day", 1));
    }
    db_->begin();
    for (auto& cj : j["crew"]) {
        Crew c;
        c.id = cj.value("id", nextCrewId_);
        nextCrewId_ = std::max(nextCrewId_, c.id + 1);
        c.name = cj.value("name", std::string("Crew member"));
        c.role = cj.value("role", std::string());
        c.sex = (cj.value("sex", std::string("M")) == "F") ? 1 : 0;
        c.age = cj.value("age", 35);
        c.height = cj.value("height_cm", 175.0); c.weight = cj.value("weight_kg", 75.0);
        c.edu = cj.value("education_years", 16);
        c.notes = cj.value("notes", std::string()); c.scenario = cj.value("scenario", std::string());
        crew_.push_back(c);
        if (!cj.contains("data") || !cj["data"].is_object()) continue;
        auto& d = cj["data"];
        if (d.contains("labs")) for (auto& l : d["labs"]) if (cat_.find(l.value("bio", std::string()))) insertLab(c.id, l["bio"], l.value("value", 0.0), l.value("day", 1));
        if (d.contains("mind")) for (auto& m : d["mind"])
            db_->prep("INSERT INTO mind(crew,day,mood,stress,anxiety,fatigue,isolation,sleep,pvt) VALUES(?,?,?,?,?,?,?,?,?)")
                .i(c.id).i(m.value("day", 1)).d(m.value("mood", 5.0)).d(m.value("stress", 5.0)).d(m.value("anxiety", 5.0)).d(m.value("fatigue", 5.0))
                .d(m.value("isolation", 5.0)).d(m.value("sleep", 5.0)).d(m.value("pvt_ms", 0.0)).run();
        if (d.contains("pain")) for (auto& p : d["pain"]) {
            insertPain(c.id, p.value("day", 1), p.value("view", std::string("front")), p.value("zone", std::string("Unknown")), p.value("x", 50.0), p.value("y", 50.0),
                       p.value("intensity", 3), p.value("kind", std::string("Dull ache")), p.value("duration_h", 1.0), p.value("note", std::string()));
            if (!p.value("active", true)) db_->prep("UPDATE pain SET active=0 WHERE id=?").i(db_->lastId()).run();
        }
        if (d.contains("moca")) for (auto& m : d["moca"]) {
            int v[7];
            for (int i = 0; i < 7; ++i) v[i] = (int)clampd(m.value(MOCA_KEYS[i], 0), 0, MOCA_MAX[i]);
            insertMoca(c.id, m.value("day", 1), v, c.edu <= 12 ? 1 : 0, m.value("version", std::string("8.1")), m.value("minutes", 10), m.value("note", std::string()));
        }
    }
    if (j.contains("events")) for (auto& e : j["events"]) addEvent(e.value("day", 1), e.value("crew", 0), e.value("sev", 0), e.value("text", std::string()));
    db_->commit();
    return true;
}

bool Engine::writeProfile(const std::string& path) {
    try {
        std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary);
            if (!f) return false;
            f << exportProfile().dump(1);
        }
        std::error_code ec;
        fs::rename(tmp, path, ec);
        if (ec) { fs::remove(path, ec); fs::rename(tmp, path, ec); }
        return !ec;
    } catch (...) { return false; }
}

void Engine::autosave() {
    if (!profileFile_.empty()) writeProfile(profileFile_);
}

void Engine::rememberLast() {
    std::ofstream f(profilesDir_ + "/.last", std::ios::binary);
    f << fs::path(profileFile_).filename().string();
}

json Engine::profiles() {
    std::lock_guard<std::mutex> g(mu_);
    json out = json::array();
    std::error_code ec;
    for (auto& e : fs::directory_iterator(profilesDir_, ec)) {
        if (e.path().extension() != ".json") continue;
        std::ifstream f(e.path(), std::ios::binary);
        json j = json::parse(f, nullptr, false);
        if (!j.is_object() || !j.contains("crew")) continue;
        json m = (j.contains("mission") && j["mission"].is_object()) ? j["mission"] : json::object();
        out.push_back({{"file", e.path().filename().string()}, {"name", m.value("name", std::string("?"))}, {"vehicle", m.value("vehicle", std::string())},
                       {"type", m.value("type", std::string())}, {"crew", j["crew"].size()}, {"day", m.value("day", 1)}, {"simulated", m.value("simulated", false)},
                       {"current", e.path().filename().string() == fs::path(profileFile_).filename().string()}});
    }
    std::sort(out.begin(), out.end(), [](const json& a, const json& b) { return a["file"].get<std::string>() < b["file"].get<std::string>(); });
    return out;
}

json Engine::loadProfile(const std::string& file) {
    std::lock_guard<std::mutex> g(mu_);
    std::string path = file;
    if (!fs::exists(path)) path = profilesDir_ + "/" + fs::path(file).filename().string();
    std::ifstream f(path, std::ios::binary);
    if (!f) return {{"error", "profile not found: " + file}};
    json j = json::parse(f, nullptr, false);
    if (j.is_discarded()) return {{"error", "invalid JSON in " + file}};
    std::string err;
    if (!importProfile(j, err)) return {{"error", err}};
    profileFile_ = path;
    rememberLast();
    return {{"ok", true}, {"name", mission_.name}};
}

json Engine::newProfile(const json& b) {
    std::lock_guard<std::mutex> g(mu_);
    std::string name = b.value("name", std::string("New mission"));
    std::string fn = safeName(name);
    int k = 2;
    while (fs::exists(profilesDir_ + "/" + fn)) fn = safeName(name + "_" + std::to_string(k++));
    resetStore();
    mission_ = Mission();
    mission_.name = name;
    mission_.vehicle = b.value("vehicle", std::string());
    mission_.type = b.value("type", std::string("Space station"));
    mission_.duration = b.value("duration", 180);
    profileFile_ = profilesDir_ + "/" + fn;
    autosave();
    rememberLast();
    return {{"ok", true}, {"file", fn}};
}

json Engine::saveProfileAs(const std::string& file) {
    std::lock_guard<std::mutex> g(mu_);
    std::string fn = safeName(file);
    std::string path = profilesDir_ + "/" + fn;
    if (!writeProfile(path)) return {{"error", "cannot write " + path}};
    profileFile_ = path;
    rememberLast();
    return {{"ok", true}, {"file", fn}};
}

bool Engine::buildDemo(const std::string& path) {
    std::lock_guard<std::mutex> g(mu_);
    resetStore();
    mission_ = Mission();
    mission_.name = "ISS Expedition (demo)";
    mission_.vehicle = "Orbital station";
    mission_.type = "Space station";
    mission_.duration = 180;
    mission_.simulated = true;
    auto add = [&](const char* n, const char* role, int sex, int age, double h, double w, int edu, const char* scen, const char* notes) {
        Crew c; c.id = nextCrewId_++; c.name = n; c.role = role; c.sex = sex; c.age = age; c.height = h; c.weight = w; c.edu = edu; c.scenario = scen; c.notes = notes;
        crew_.push_back(c);
    };
    add("Elena Vasquez", "Commander", 1, 41, 168, 61, 18, "sans", "Demo crew member (fictional): SANS storyline.");
    add("Arjun Mehta", "Flight Engineer", 0, 38, 178, 78, 20, "bone_renal", "Demo crew member (fictional): bone and kidney storyline.");
    add("Yuki Tanaka", "Mission Specialist", 1, 34, 162, 54, 16, "nominal", "Demo crew member (fictional): nominal reference.");
    add("Marcus Okafor", "Medical Officer", 0, 45, 182, 84, 22, "thrombosis", "Demo crew member (fictional): jugular vein storyline.");
    db_->begin();
    for (int d = 1; d <= 74; ++d) simulateDay(d);
    db_->commit();
    day_ = 74;
    return writeProfile(path);
}

}  // namespace av
