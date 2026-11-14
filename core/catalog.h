// Biomarker catalog + threshold classification (works fully offline).
#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace av {

constexpr double INF = 1e9;
enum Tier { T_EARTH = 0, T_SPACE = 1, T_CONCERN = 2, T_DANGER = 3 };
enum Sev { SEV_OK = 0, SEV_WATCH = 1, SEV_CRIT = 2 };

struct Interval { double lo, hi; };

struct Marker {
    std::string id, group, name, unit, cluster_name, apparatus, avail, avail_text, frequency;
    std::string text[4];
    std::string note_danger, note_concern, note_space, bad_dir;
    int cluster = 0;
    bool abs = false;
    std::vector<Interval> tier[4][2];  // [tier][0=M,1=F]
    double span_lo = 0, span_hi = 1, space_mid = 0;
    std::string key;  // lowercase searchable text
};

struct Classification {
    int severity = SEV_OK;      // 0 normal, 1 concerning, 2 dangerous
    bool earth_abnormal = false; // would be flagged using Earth reference only
};

class Catalog {
public:
    bool load(const std::string& path);
    const std::vector<Marker>& all() const { return m_; }
    const Marker* find(const std::string& id) const;
    // first marker whose id/name/group contains `needle` (lowercase)
    const Marker* search(const std::string& needle) const;
    static Classification classify(const Marker& m, int sex, double v);
    static double score(int severity);
    // "typical" value of a tier for simulation (towards bad direction)
    static double tierTarget(const Marker& m, int sex, int tier);
    nlohmann::json toJson() const;
private:
    std::vector<Marker> m_;
};

}  // namespace av
