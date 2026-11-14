"""Dump the biomarker xlsx as TSV-like rows (no third-party deps)."""
import sys, zipfile, re
import xml.etree.ElementTree as ET

NS = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}


def read_rows(path):
    z = zipfile.ZipFile(path)
    sst = []
    if "xl/sharedStrings.xml" in z.namelist():
        root = ET.fromstring(z.read("xl/sharedStrings.xml"))
        for si in root.findall("m:si", NS):
            sst.append("".join(t.text or "" for t in si.iter("{%s}t" % NS["m"])))
    sheet = ET.fromstring(z.read("xl/worksheets/sheet1.xml"))
    rows = []
    for r in sheet.iter("{%s}row" % NS["m"]):
        cells = {}
        for c in r.findall("m:c", NS):
            ref = c.get("r")
            col = re.match(r"[A-Z]+", ref).group(0)
            v = c.find("m:v", NS)
            t = c.get("t")
            if t == "s" and v is not None:
                val = sst[int(v.text)]
            elif t == "inlineStr":
                val = "".join(x.text or "" for x in c.iter("{%s}t" % NS["m"]))
            else:
                val = v.text if v is not None else ""
            cells[col] = val
        rows.append(cells)
    return rows


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    for i, r in enumerate(read_rows(sys.argv[1])):
        print(i, "|".join(f"{k}={v}" for k, v in sorted(r.items())))
