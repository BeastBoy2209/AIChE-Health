"""Convert Spaceflight_Diagnostic_Biomarker_Values.xlsx -> data/biomarkers.json

Each tier cell is parsed into numeric intervals (per sex) so the C++ core can
classify a measured value as Normal / Concerning / Dangerous without any
network access or Earth-side lookup.
usage: python -I tools/xlsx_to_json.py <xlsx> <out.json>
"""
import json, re, sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dump_xlsx import read_rows  # noqa: E402

INF = 1e9
NUM = r"[+-]?\d[\d,]*(?:\.\d+)?"
CMP_RE = re.compile(r"^\s*(>=|<=|≥|≤|>|<)\s*(" + NUM + r")")
RANGE_RE = re.compile(r"(" + NUM + r")\s*[%A-Za-zµ²/ ]*?\s*(?:–|—|\bto\b)\s*(" + NUM + r")")
POINT_RE = re.compile(r"^\s*(" + NUM + r")")


def fnum(s):
    return float(s.replace(",", ""))


def clean(cell):
    s = re.sub(r"\((or [^)]*)\)", r" \1", cell, flags=re.I)  # keep "(or ...)" clauses
    s = re.sub(r"\([^)]*\)", " ", s)
    return s


def parse_clause(c):
    c = c.strip()
    m = CMP_RE.match(c)
    if m:
        op, v = m.group(1), fnum(m.group(2))
        rest = c[m.end():]
        if re.match(r"-[A-Za-z]", rest):  # e.g. ">=3-beat VT run"
            return None
        if op in (">", ">=", "≥"):
            return [v, INF]
        return [-INF, v]
    c2 = c.replace("±", "")
    m = RANGE_RE.search(c2)
    if m:
        a, b = fnum(m.group(1)), fnum(m.group(2))
        return [min(a, b), max(a, b)]
    m = POINT_RE.match(c2)
    if m:
        v = fnum(m.group(1))
        return [v, v]
    return None


def parse_intervals(text):
    out = []
    for cl in re.split(r"\s+or\s+", clean(text), flags=re.I):
        iv = parse_clause(cl)
        if iv:
            out.append(iv)
    return out


def parse_tier(cell):
    lines = [l.strip() for l in cell.split("\n") if l.strip()]
    has_sex = any(re.search(r"\((M|F)\)", l) for l in lines)
    if has_sex:
        res = {"M": [], "F": []}
        for l in lines:
            sx = re.search(r"\((M|F)\)", l)
            if sx:
                res[sx.group(1)] += parse_intervals(l)
        return res
    iv = parse_intervals(lines[0] if lines else cell)  # first line only (e.g. supine ICP)
    return {"M": iv, "F": iv}


def first_note(cell):
    m = re.findall(r"\(([^)]*)\)", cell.replace("\n", " "))
    notes = [n for n in m if not re.match(r"^[MF]$", n.strip()) and not n.strip().lower().startswith("or ")]
    return notes[-1] if notes else ""


def avail_of(s):
    s = s.lower()
    if s.startswith("onboard") or "now onboard" in s:
        return "onboard"
    if s.startswith("partially"):
        return "partial"
    return "missing"


def slug(s):
    return re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")[:40]


# Hand-fixed rows where the sheet text is ambiguous (drop vs. signed value, abs shifts).
OVERRIDES = {
    "lbnp": {"earth": [[5, 10]], "space": [[10, 15]], "concern": [[16, 25]], "danger": [[25.0001, INF]], "unit": "mmHg drop"},
}


def main(xlsx, out):
    rows = read_rows(xlsx)
    cluster = 0
    cluster_name = ""
    res = []
    i = 3
    while i < len(rows):
        r = rows[i]
        a = r.get("A", "")
        i += 1
        if a.startswith("CLUSTER"):
            cluster += 1
            cluster_name = a.split(":", 1)[1].strip().title().replace("&", "&")
            continue
        # xlsx rows with multi-line cells may be split by blank cells; we only keep rows with a name and tiers
        if not a or "E" not in r:
            continue
        group, _, rest = a.partition(" — ")
        name_full = rest or a
        um = re.search(r"\(([^()]*)\)\s*(\[[^\]]*\])?\s*$", name_full)
        unit = um.group(1) if um else ""
        short = re.sub(r"\s*\([^()]*\)\s*(\[[^\]]*\])?\s*$", "", name_full).strip()
        cells = {k: r.get(k, "") for k in "EFGH"}
        tiers = {t: parse_tier(cells[k]) for t, k in zip(["earth", "space", "concern", "danger"], "EFGH")}
        m = {
            "id": slug(group + "_" + short),
            "group": group.strip(),
            "name": short,
            "unit": unit,
            "cluster": cluster,
            "cluster_name": cluster_name,
            "apparatus": r.get("B", ""),
            "avail": avail_of(r.get("C", "")),
            "avail_text": r.get("C", ""),
            "frequency": r.get("D", ""),
            "abs": "±" in cells["E"] or "±" in cells["F"],
            "text": {t: cells[k].replace("\n", " / ") for t, k in zip(["earth", "space", "concern", "danger"], "EFGH")},
            "note_danger": first_note(cells["H"]),
            "note_concern": first_note(cells["G"]),
            "note_space": first_note(cells["F"]),
            "tiers": tiers,
        }
        if "LBNP" in group:
            ov = OVERRIDES["lbnp"]
            for t in ["earth", "space", "concern", "danger"]:
                m["tiers"][t] = {"M": ov[t], "F": ov[t]}
            m["unit"] = ov["unit"]
        # direction in which values get worse
        d = tiers["danger"]["M"] + m["tiers"]["concern"]["M"]
        sp = m["tiers"]["space"]["M"]
        lo_bad = any(iv[0] <= -INF for iv in d) or any(iv[1] < sp[0][0] for iv in d if sp)
        hi_bad = any(iv[1] >= INF for iv in d) or any(iv[0] > sp[0][1] for iv in d if sp)
        m["bad_dir"] = "both" if (lo_bad and hi_bad) else ("low" if lo_bad else "high")
        if m["abs"]:
            m["bad_dir"] = "high"
        # display span
        nums = []
        for t in tiers.values():
            for sx in t.values():
                for iv in sx:
                    for v in iv:
                        if abs(v) < INF:
                            nums.append(v)
        lo, hi = min(nums), max(nums)
        pad = max((hi - lo) * 0.25, 0.1 * max(abs(hi), abs(lo), 1))
        m["span"] = [lo - pad if (lo - pad >= 0 or lo < 0) else 0, hi + pad]
        if lo >= 0 and m["span"][0] < 0:
            m["span"][0] = 0
        # Healthy-center target for the simulator: midpoint of space-normal band (male)
        sv = tiers["space"]["M"] or tiers["earth"]["M"]
        m["space_mid"] = (sv[0][0] + sv[0][1]) / 2 if sv else (lo + hi) / 2
        res.append(m)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=1)
    print(f"wrote {len(res)} biomarkers, {cluster} clusters -> {out}")
    bad = [m["name"] for m in res if not all(m["tiers"][t]["M"] for t in m["tiers"])]
    if bad:
        print("WARNING empty tiers:", bad)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main(sys.argv[1], sys.argv[2])
