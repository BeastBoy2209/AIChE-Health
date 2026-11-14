#include "catalog.h"
#include <fstream>
#include <algorithm>
#include <cmath>

namespace av {
using json = nlohmann::json;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool Catalog::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    json j = json::parse(f, nullptr, false);
    if (!j.is_array()) return false;
    for (auto& e : j) {
        Marker m;
        m.id = e["id"]; m.group = e["group"]; m.name = e["name"]; m.unit = e["unit"];
        m.cluster = e["cluster"]; m.cluster_name = e["cluster_name"];
        m.apparatus = e["apparatus"]; m.avail = e["avail"]; m.avail_text = e["avail_text"];
        m.frequency = e["frequency"]; m.abs = e["abs"]; m.bad_dir = e["bad_dir"];
        m.note_danger = e["note_danger"]; m.note_concern = e["note_concern"]; m.note_space = e["note_space"];
        const char* tn[4] = {"earth", "space", "concern", "danger"};
        for (int t = 0; t < 4; ++t) {
            m.text[t] = e["text"][tn[t]];
            for (int s = 0; s < 2; ++s)
                for (auto& iv : e["tiers"][tn[t]][s ? "F" : "M"]) m.tier[t][s].push_back({iv[0], iv[1]});
        }
        m.span_lo = e["span"][0]; m.span_hi = e["span"][1]; m.space_mid = e["space_mid"];
        m.key = lower(m.id + " " + m.name + " " + m.group);
        m_.push_back(std::move(m));
    }
    return !m_.empty();
}

const Marker* Catalog::find(const std::string& id) const {
    for (auto& m : m_) if (m.id == id) return &m;
    return nullptr;
}
const Marker* Catalog::search(const std::string& needle) const {
    std::string n = lower(needle);
    for (auto& m : m_) if (m.key.find(n) != std::string::npos) return &m;
    return nullptr;
}

static double dist(double v, const std::vector<Interval>& ivs, double span) {
    double best = 1e18;
    for (auto& iv : ivs) {
        double d = 0;
        if (v < iv.lo) d = iv.lo - v; else if (v > iv.hi) d = v - iv.hi;
        best = std::min(best, d / span);
    }
    return best;
}
static bool inside(double v, const std::vector<Interval>& ivs) { return dist(v, ivs, 1.0) <= 0.0; }

Classification Catalog::classify(const Marker& m, int sex, double v) {
    if (m.abs) v = std::fabs(v);
    sex = sex ? 1 : 0;
    Classification c;
    double span = std::max(m.span_hi - m.span_lo, 1e-9);
    c.earth_abnormal = !inside(v, m.tier[T_EARTH][sex]);
    if (inside(v, m.tier[T_DANGER][sex])) { c.severity = SEV_CRIT; return c; }
    if (inside(v, m.tier[T_CONCERN][sex])) { c.severity = SEV_WATCH; return c; }
    if (inside(v, m.tier[T_SPACE][sex]) || inside(v, m.tier[T_EARTH][sex])) { c.severity = SEV_OK; return c; }
    // gap between published bands: take the nearest band, ties go to the more severe one
    double dd = dist(v, m.tier[T_DANGER][sex], span), dc = dist(v, m.tier[T_CONCERN][sex], span);
    double ds = std::min(dist(v, m.tier[T_SPACE][sex], span), dist(v, m.tier[T_EARTH][sex], span));
    double best = std::min({dd, dc, ds});
    c.severity = (dd <= best) ? SEV_CRIT : (dc <= best ? SEV_WATCH : SEV_OK);
    return c;
}

double Catalog::score(int severity) { return severity == SEV_OK ? 100.0 : (severity == SEV_WATCH ? 62.0 : 22.0); }

double Catalog::tierTarget(const Marker& m, int sex, int tier) {
    sex = sex ? 1 : 0;
    const auto& ivs = m.tier[tier][sex];
    if (ivs.empty()) return m.space_mid;
    bool low = (m.bad_dir == "low");
    double w = (m.span_hi - m.span_lo);
    const Interval* pick = &ivs[0];
    for (auto& iv : ivs) {
        if (!low && iv.lo > pick->lo) pick = &iv;
        if (low && iv.hi < pick->hi) pick = &iv;
    }
    if (pick->hi >= INF) return pick->lo + 0.10 * w;
    if (pick->lo <= -INF) return pick->hi - 0.10 * w;
    return (pick->lo + pick->hi) / 2;
}

json Catalog::toJson() const {
    json out = json::array();
    for (auto& m : m_) {
        json t;
        const char* tn[4] = {"earth", "space", "concern", "danger"};
        for (int i = 0; i < 4; ++i)
            for (int s = 0; s < 2; ++s) {
                json a = json::array();
                for (auto& iv : m.tier[i][s]) a.push_back({iv.lo, iv.hi});
                t[tn[i]][s ? "F" : "M"] = a;
            }
        out.push_back({{"id", m.id}, {"group", m.group}, {"name", m.name}, {"unit", m.unit},
                       {"cluster", m.cluster}, {"cluster_name", m.cluster_name}, {"apparatus", m.apparatus},
                       {"avail", m.avail}, {"avail_text", m.avail_text}, {"frequency", m.frequency},
                       {"abs", m.abs}, {"bad_dir", m.bad_dir},
                       {"text", {{"earth", m.text[0]}, {"space", m.text[1]}, {"concern", m.text[2]}, {"danger", m.text[3]}}},
                       {"note_danger", m.note_danger}, {"note_concern", m.note_concern},
                       {"span", {m.span_lo, m.span_hi}}, {"tiers", t}});
    }
    return out;
}

}  // namespace av
