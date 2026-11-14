# AIChE Health: autonomous crew health monitor

Native (C++17) health monitoring software for crews on **space stations and spacecraft**, designed to work with **no link to Earth**.
Built for the NASA Space Apps Challenge. Full user guide (Russian): [docs/AIChE_Health_Guide.pdf](docs/AIChE_Health_Guide.pdf).

- **Backend `aiche-core`**: offline service. Biomarker threshold engine, health index, rule-based medical advisor, MoCA schedule tracking, local REST API on `127.0.0.1`.
- **Frontend `aiche-health`**: native window (Dear ImGui + GLFW + OpenGL), Linux and Windows.
- No web browser, no cloud, no internet at runtime. All third-party code is vendored in `third_party/`.

## Profiles (no built-in mock data)
The app starts **empty**. All data lives in JSON *profiles* in the `profiles/` folder: mission, crew and every crew member's data (labs, mental check-ins, pain reports, MoCA, event log).
- `profiles/demo_expedition.json`: fictional 4-person demo mission (load it from **Mission & Crew** or with `--profile demo_expedition.json`)
- `profiles/template_empty_mission.json`: minimal template to copy and edit by hand
- Every change is autosaved into the active profile. The last opened profile is reopened on next start.
- Regenerate the demo: `aiche-core --data data --make-demo profiles/demo_expedition.json`

## What it does
| Screen | Purpose |
|---|---|
| Crew Overview | Health index per crew member, critical and MoCA-due banners, system radar, priority alerts, mission log |
| Vitals & Labs | 73 biomarkers with four-tier scales (Earth normal / Space normal / Concerning / Dangerous), history, device and cadence, record new measurements, Earth-vs-Space reference toggle |
| Mind | Daily mental check-in plus an in-app reaction-time (PVT) test and trends |
| MoCA Test | **Mandatory weekly** Montreal Cognitive Assessment: schedule, overdue reminders, 7-domain score entry, versions 8.1/8.2/8.3 rotation, education adjustment, trend vs personal baseline |
| Body Map | Click the front or back body to report pain: zone, intensity, type, duration, note |
| Advisor | Offline rule engine (labs + mind + pain + MoCA) with evidence and recommended actions |
| Mission Report | Text summary per crew member, exported to a downlink queue |
| Mission & Crew | Load/create/save profiles, edit mission parameters, add/edit/remove crew members and their details |

MoCA note: the app records scores only. Administer the test with the official licensed MoCA materials (mocacognition.com); no test stimuli are bundled.

## Build and run

### Windows
Needs Visual Studio 2022+ with the workload "Desktop development with C++" (it includes the compiler and CMake).
The script finds CMake even if it is not on PATH ("cmake is not recognized"):
```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1 -Run
```
Without `-Run` it only builds; start the app with `cd build; .\ui\Release\aiche-health.exe`.
If you prefer the commands by hand, run them in "Developer PowerShell for VS" (Start menu), where `cmake` is available:
`cmake -S . -B build; cmake --build build --config Release`.
### macOS (not yet verified on real hardware)
```bash
chmod +x scripts/build_macos.sh && ./scripts/build_macos.sh
```
Needs Xcode Command Line Tools and CMake (the script installs CMake via Homebrew if present). Uses the system OpenGL 3.2 core profile (deprecated by Apple but still available).

### Linux (Ubuntu/Debian)
```bash
./scripts/build_linux.sh
cd build && ./ui/aiche-health
```
Or with Docker (headless core smoke test): `docker build -t aiche-health . && docker run --rm aiche-health`.

### Options
`aiche-health [--profile FILE] [--profiles DIR] [--connect] [--port N] [--size WxH] [--shot DIR]`
- default: runs the core inside the UI process (still over local HTTP)
- `--connect`: attach to a separately running `aiche-core --data data --profiles profiles`
- `--shot DIR`: render each screen to PNG (used for documentation)

## Data
`data/biomarkers.json` is generated from the protocol spreadsheet:
`python tools/xlsx_to_json.py data/source/Spaceflight_Diagnostic_Biomarker_Values.xlsx data/biomarkers.json`

## Disclaimer
Research prototype for a hackathon. Reference values come from the supplied protocol, not a validated clinical standard. Not a medical device.


