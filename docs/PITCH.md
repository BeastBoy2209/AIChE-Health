# AIChE Health: pitch

## Problem
Earth medicine uses Earth reference ranges. In microgravity many values shift: venous area in the neck, optic nerve sheath, hemoglobin, bone markers, urinary calcium. Standard thresholds raise **false alarms** (47 of 73 monitored values in our demo crew) and also miss real danger. Far from Earth (Moon, Mars, deep space) there is a communication delay of minutes to hours, or no link at all. The crew medical officer needs help *on board*.

## Solution
A native, fully offline application that runs on the station or ship computer:
1. **Space-specific reference ranges** (Earth, Space-normal, Concerning, Dangerous) for 73 analyses that can be taken or evaluated on board.
2. **Crew-entered data**: lab devices, mental check-ins, reaction test, a body map for pain, and a **mandatory weekly MoCA** cognitive screening with automatic reminders.
3. **Autonomous advisor**: combines markers, mental state and pain into explainable alerts (e.g. SANS pattern, bone loss + kidney stones, jugular thrombosis, fatigue/EVA risk) with a concrete action list.
4. **Store and forward**: compact reports queue up and are sent when communications return.

## Profiles, not mock data
Missions are JSON profiles (mission, vehicle, crew, every crew member's data). Switch flight or crew by loading another profile; add, edit or remove crew in the app. The app starts empty.

## Why native and offline
- Runs on a Linux flight computer with no browser, no cloud, no internet.
- Core and UI are separate processes, so the core can run headless on a medical server and several displays can attach.
- Data stays local (privacy for mental-health data).

## Demo script (2 minutes)
1. Overview: red banner, Marcus Okafor is critical. Show the four health rings.
2. Open the Advisor: possible jugular vein thrombosis, evidence from ultrasound, D-dimer, neck pain, actions.
3. Vitals: switch the reference to Earth norms and show the false alarms; open a marker to see the four-tier scale and history.
4. Body Map: click a zone, report pain 8/10 in the chest, watch the alert appear.
5. Mind: run the reaction test, submit a check-in, see the trend.
6. MoCA: show the overdue reminder and record a test. `SIM +1 DAY`: simulate a day, events appear in the mission log. Export reports to the downlink queue.

## Data sources
NASA Human Research Program and CSA literature on spaceflight biomarkers; our protocol document "Spaceflight Medical Diagnostics Protocol" and the extracted threshold table (`data/source/`).

## Next steps
Validation with flight surgeons, connection to real devices (i-STAT, ultrasound exports), multilingual UI, signed storage, ML trend prediction, sync with ground systems when a link exists.

