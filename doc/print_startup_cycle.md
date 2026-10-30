# Print startup cycle: from tapping "Print" to the first extruded line

Working map for the "why does a small print take minutes to start on a
preheated printer" investigation. Firmware paths are from this repository;
start G-code is from the PrusaSlicer bundled profiles
(`resources/presets/prusa-research-fff/PrusaResearch/preset-printer-*.yaml`
in prusa3d/PrusaSlicer, unchanged in audun/PrusaSlicer).

Durations marked *est.* are derived from feedrates, distances and timeouts in
the configuration, not measured. Section 7 lists what to measure first.

---

## 1. The short version

On a Buddy printer the firmware itself adds very little between the tap and the
first start-G-code line (well under a second, plus whatever dialogs the user has
to answer). Almost all of the startup time is the **slicer's start G-code**.

What "preheated" means matters. The home-screen Preheat button and a finished
filament Load both leave the nozzle at the filament's **preheat temperature**
(170 °C for PLA), which the firmware table deliberately keeps equal to the
slicer's probing temperature. From that state the start G-code never cools the
nozzle; its cost is homing, the bed hold, nozzle cleanup, mesh probing, the
reheat to print temperature and the purge line. Only when the nozzle sits
**above** the probing temperature (a manual temperature from the Control menu,
a custom filament whose preheat temperature equals its print temperature, a
"keep warm" end G-code) does the start G-code add a passive **thermal round
trip** of 1–2 minutes.

Approximate budget, CORE One+, PLA (215 / 60), small part, bed soaked, nozzle
at 170 via the Preheat button, bed parked low after a previous print (all
*est.*):

| Step (in start-G-code order)              | Owner    | Cost                               |
|-------------------------------------------|----------|------------------------------------|
| Preview, metadata scan, prefetch, dialogs | firmware | < 1 s + user interaction           |
| `M109 R170` nozzle already there          | slicer   | 0 (residency already satisfied)    |
| `G28` full XYZ re-home                    | slicer   | 30–40 s, a third of it Z travel from the low park position |
| `G0 Z40`, `M104 S100` idle, `M190 R60`    | slicer   | ~7 s (`M190` always holds 5 s)     |
| `G29 G` heat soak                         | slicer   | 0 if bed on for > ~3 min, else up to ~1.5 min, skippable |
| `M109 R170` reheat from idle drop         | slicer   | 5–15 s incl. 5 s residency         |
| `G29 P9` nozzle cleanup taps              | slicer   | 5–10 s (3 taps min, up to 32)      |
| `G29 P1` ×2 mesh probing, small part      | slicer   | 25–35 s (9 + 3 points, two 5 mm first descents) |
| `M109 S215` reheat 170 → 215 + 5 s hold   | slicer   | 20–30 s                            |
| Purge line + wipe                         | slicer   | ~10 s                              |
| **Total**                                 |          | **roughly 2–2.5 min**              |

Add 1–2 min of passive cool-down at the top if the nozzle started above
170 °C. The MK4S sequence is the same shape minus the idle-temperature drop
and the heat soak.

---

## 2. Firmware: from the tap to the first start-G-code line

### 2.1 Entry points and what they skip

All entry points land in `marlin_server::print_start()`
(`src/common/marlin_server.cpp`) with a `PreviewSkipIfAble` level
(`src/common/marlin_events.h`):

| Entry point                                   | Skip level     | Effect                                             |
|-----------------------------------------------|----------------|----------------------------------------------------|
| File browser / home screen tap                | `no`           | Thumbnail preview, user must tap Print             |
| "Reprint" button on the printing screen       | `preview`      | No thumbnail; checks still run; tool mapping kept  |
| PrusaConnect / PrusaLink start                | `all`          | No thumbnail, no tool-mapping screen               |
| `/usb/AUTO.GCO` present at boot               | `all`          | Same, auto-started after heaters are ready         |

`print_start()` itself does: cancel any running metadata scan, resolve the
long filename in an async job (blocks the Marlin loop by spinning `idle()`),
store paths in `marlin_vars`, point `GCodeInfo` at the file, start the media
prefetch (lazy when the preview will be shown, eager otherwise), and enter
`State::PrintPreviewInit`.

### 2.2 The print preview state machine

`PrintPreview::Loop()` in `src/common/marlin_print_preview.cpp` is ticked from
the server loop at most every 50 ms. States, in the order a normal print passes
through them:

1. **`init`** – starts the async metadata scan (`gcode_info_scan::start_scan()`),
   resets progress. With any skip level the job is marked started here.
2. **`loading`** – waits for the scan to report started and for
   `GCodeInfo::is_loaded()`; then waits for the media prefetch buffer to be
   > 90 % full (`MediaPrefetchManager::check_ready_to_start_print()`, buffer is
   8 KiB). Falls into **`download_wait`** if the file is still being
   transferred (Connect/Link uploads).
3. **`preview_wait_user`** – the thumbnail screen. Re-checks file validity
   every second in an async job. Skipped when skip level ≥ `preview`.
4. **`unfinished_selftest_wait_user`** – if selftest/calibration is not
   complete. Blocks until Ignore / Calibrate.
5. **`new_firmware_available_wait_user`** – if `M115 U<version>` in the file
   is newer than the running firmware and the check is not set to Ignore.
   Blocks for up to **30 s** then auto-continues. Easy to overlook as a real
   startup cost for users who are one release behind.
6. **Printer compatibility** (`stateFromPrinterCheck`) – evaluates the
   `M862.x` checks (nozzle diameter, printer model, G-code level, features).
   Reminders and approvals block in `gcode_invalid_wait_user`; fatal ones
   abort.
7. **Wastebin fill check** (INDX printers only).
8. **Filament presence** – with the filament sensor on and the tool used by the
   G-code empty, offers load (`M701`) and waits for it.
9. **Filament type** – if the loaded filament type differs from the G-code,
   offers change (`M1600`) / ignore / abort. Compatibility report lives in
   `src/feature/compatibility_checks/`.
10. **Tool mapping** (XL, MMU, INDX) – the mapping screen, skipped if fully
    compatible and skip level ≥ `tool_mapping`. **This is the only place the
    firmware starts heating ahead of the start G-code**: at `checks_done` on
    tool-mapping printers it sets the bed target from the parsed
    `M140`/`M190` and applies the `M555` print area so bedlets preheat while
    the user maps tools.
11. **`done`** → `Result::Print` → `State::PrintInit`.

### 2.3 Metadata scan (`GCodeInfo::load`, `src/common/gcode/gcode_info.cpp`)

Runs on the async-job thread. Opens the file through `AnyGcodeFormatReader`
(binary `.bgcode` or plain text), builds an index with the three thumbnail
sizes (CRC checking is skipped here on purpose; the comment says it saves
~800 ms), streams the metadata block, then parses the **first 200 G-code
lines** (`search_first_x_gcodes`). From those it extracts, among other things:

- `M862.x` compatibility requirements and `M115 U` firmware version,
- `M555` print area (`bed_preheat_area`),
- `M140`/`M190` → `bed_preheat_temp`, and the max of `M104`/`M109` →
  `hotend_preheat_temp`.

So the firmware already knows the target temperatures before the preview is
shown. Today `hotend_preheat_temp` is only used by the MMU single-tool hack in
`PrintInit`.

### 2.4 `PrintInit` → `Printing`

`_server_print_loop()` in `marlin_server.cpp`: waits for the gcode queue to be
empty, resets feedrate/flow, crash detection, cancel-object, Live-Z offset
(loadcell printers), starts the job timer, swaps the `PrintPreview` FSM for the
`Printing` FSM, enqueues a `T0` for single-tool G-code on toolchangers, then
sets `State::Printing`. From there `media_print_loop()` pulls lines from the
8 KiB prefetch ring into Marlin's command queue (`BUFSIZE 8`, `MAX_CMD_SIZE 96`)
and the start G-code begins executing. Nothing here is slow.

---

## 3. Slicer start G-code, line by line (CORE One, non-MMU)

Source: `preset-printer-coreone.yaml`, `start_gcode`. Firmware handlers noted.
Chamber-temperature branches omitted (not taken for PLA/PETG).

| Line                                   | Firmware behaviour                                                                                   | Cost / notes |
|----------------------------------------|------------------------------------------------------------------------------------------------------|--------------|
| `M17`, `M862.1/.3/.5/.6`, `M115 U…`    | Steppers on; compat checks (already evaluated in preview; `M862` handlers in `src/marlin_stubs/`)    | negligible |
| `M555 X Y W H`                         | Sets the print area used by `G29 P1` (`PrintArea`). Area is widened 32 mm in X and 4 mm in −Y so the purge/cleanup spot is included | none |
| `G90`, `M83`                           | Modes                                                                                                | none |
| `M140 S<bed>`                          | Bed target, no wait                                                                                  | none |
| `M109 R<probe temp>`                   | `wait_for_hotend` with `no_wait_for_cooling = false`: **waits for cooling too**, no fan assist unless `F` is given. Done when within 1 °C (`TEMP_WINDOW`) for 5 s (`TEMP_RESIDENCY_TIME`, 1 s on INDX); returns at once if that residency is already satisfied. Probe temp is 170 °C for most materials, `first_layer_temperature − 10` if the filament notes contain `HT_MBL10`, −25 for PC/PA, 210 for FLEX, 160 for `MBL160` | 0 after the Preheat button or a Load (nozzle already at 170). **From a 215 °C nozzle: passive cool-down of 45 °C, est. 1–2 min.** |
| `M84 E`                                | E motor off                                                                                          | none |
| `G28`                                  | Bare `G28` ⇒ `force = true`: **all three axes re-homed every print** even if already homed. CORE One: precise CoreXY homing with up to 6 refinement retries (`PRECISE_HOMING_COREXY_RETRIES`), XY at 70 mm/s with 10 mm bumps; Z homed with the loadcell at the safe point (10, 10) at 19 mm/s plus two 2 mm bumps at 1.17 mm/s (`Z_PROBE_SPEED_SLOW`); then **steel-sheet detection** probe at (245, 0) (`HAS_PRINT_SHEET_DETECTION`). Firmware supports `G28 O`/`G28 Q` (home only if not trusted) and `G28 I` (skip precise refinement); the slicer uses neither | est. 20–40 s |
| `M141 S0`                              | Chamber target                                                                                       | none |
| `M106 S70` (bed ≤ 60)                  | Part fan on to help the idle-temp drop                                                               | none |
| `G0 Z40 F10000`                        | Bed down 40 mm                                                                                       | ~2 s |
| `M104 S<idle temp or 100>`             | **Drops nozzle target to 100 °C** (or filament idle temp) while the bed heats                        | sets up a second reheat later |
| `M190 R<bed>`                          | Waits for bed heating **and cooling** (R). Done when within 1 °C for 5 s                             | ~5 s if already there; minutes if cooling from a hotter material |
| `M107`                                 | Fan off                                                                                              | none |
| `G29 G`                                | "Absorb heat": `Temperature::wait_for_frame_heatup()` waits until the bed-frame estimate is within 1 °C of target. Returns immediately if target ≤ room temperature threshold, target < current, or already stable. Skippable from the screen | 0 on a soaked machine; minutes from cold for PETG/ASA |
| `M109 R<probe temp>`                   | Reheat from the idle drop back to 170 °C                                                             | est. 5–15 s + 5 s residency |
| `M302 S155`, `G1 E-2 F2400`, `M84 E`   | Allow cold extrusion, retract 2 mm so the nozzle does not ooze onto the taps                         | ~1 s |
| `G29 P9 X208 Y-2.5 W32 H4`             | Loadcell **nozzle cleanup** (`cleanup_probe`, `lib/Marlin/Marlin/src/module/probe.cpp`): taps a 32×4 mm rectangle on 2 mm pitch (2 rows × 16 columns) until 3 consecutive taps are classified clean. On units with the nozzle-cleaner-lite accessory, runs that instead (heats to cleaning temp, wipes, then rests until the nozzle is only **20 °C** below cleaning temp, `cooldown_temp_diff`) | est. 5–10 s; up to 32 taps if dirty; failure → park + wizard |
| `M84 E`                                | E off                                                                                                | none |
| `G29 P1`                               | Invalidate mesh, probe the **major points** inside the `M555` area widened by one major step (37 × 32 mm). Major grid is 7 × 7 over the bed (indices 1,4,…,19 of a 21 × 21 mesh). Each point: slow descent from 0.3 mm above previous Z at 1.17 mm/s, 150 ms touchdown delay, 0.09 mm back-off at 0.33 mm/s, analysis wait, retried on bad classification; first point descends from 5 mm | **Small part (≤ ~35 mm): 9 points**; 100 × 100 part: 25 points; est. ~2 s/point |
| `G29 P1 X150 Y0 W100 H20 C`            | Extra probing around the purge area, no invalidation (`C`)                                            | 3 more points |
| `G29 P3.2`, `G29 P3.13`, `G29 A`       | Interpolate / extrapolate / activate                                                                 | negligible |
| `M104 S<print temp>`                   | Start reheating                                                                                      | – |
| `G0 X249 Y-2.5 Z15 F4800`              | Move to purge start                                                                                  | ~2 s |
| `M109 S<print temp>`                   | Wait, heating only. 170 → 215 plus 5 s residency                                                     | est. 20–30 s |
| `G92`, `M569 S0 E`, `M591 S0`          | Housekeeping                                                                                         | none |
| `G1 E2 F2400` + 4 purge segments + wipe | ~50 mm purge line at 500–800 mm/min                                                                 | est. ~10 s |
| `M591 R`, `G92 E0`, `M221 S100`        | Housekeeping                                                                                         | none |

### 3.1 MK4S differences (`preset-printer-mk4.yaml`, "Original Prusa MK4S")

Order: `M140`, `M104 T0 S<probe temp>`, `M109 T0 R<probe temp>`, `G28`, move to
(42, −4, 5), retract, `G29 P9 X10 Y-4 W32 H4`, `G0 Z40`, `M190 S<bed>` (S, so no
wait for bed cooling), `G29 P1` + `G29 P1 X0 Y0 W50 H20 C` (2 extra points),
reheat, purge at X 15–48. No idle-temperature drop and no `G29 G` heat soak.
MK4S homing uses Cartesian precise homing (`PRECISE_HOMING_TRIES 12` per axis,
Z bump 3 mm). Same thermal round trip 215 → 170 → 215.

### 3.2 XL and MINI

XL: homes XY, waits for the probing temp, homes Z, idle temp 70 °C during bed
wait, `G29 G` soak, cleanup `G29 P9` at a position computed from the print
area, mesh near the purge area with a 50 × 20 window; per-tool offsets make
the tool pick part of `G28`. MINI: `M109 R170` → `M190` → `G28` → bare `G29`
(the firmware expands this to `G29 P1 X0 Y0`, i.e. the **whole bed**, with the
MINI's inductive probe), then `M109`. No cleanup step.

---

## 4. Why the thermal round trip exists, and what the firmware already offers

The loadcell probe is the nozzle. Molten filament oozing out during a tap is
classified as a bad tap (`loadcell.analysis.Analyse`) and the point is
retried; a blob left on the sheet corrupts the mesh. The 170 °C probing
temperature, the 2 mm retract, and the `G29 P9` cleanup all exist to keep the
taps clean. Everything downstream (purge line) exists to undo the retract and
the cooldown.

Relevant knobs that already exist, mostly unused by the stock profiles:

- **`HT_MBL10` filament note** – the stock start G-code probes at
  `first_layer_temperature − 10` when this string is in the filament notes.
  Zero-code experiment: a PLA profile with `HT_MBL10` would cool 215 → 205
  instead of 215 → 170 and reheat only 10 °C.
- **`M109 … F`** – `autotemp` flag passed as `fan_cooling` to
  `wait_for_hotend`: runs the part fan at 100 % while cooling and restores it
  afterwards. Would turn the passive 1–2 min cool-down into a forced one.
- **`M109 C<temp>`** – early return at a threshold without changing the target
  (overlap heating with the next move).
- **`G28 O` / `G28 Q` / `G28 I`** – home only if position is not trusted /
  skip precise refinement. The firmware tracks `axes_home_level` per axis
  (`not_homed` / `imprecise` / `full`). Note the stock end G-code ends with
  `M84 X Y E` (motors off), which drops position trust in Marlin, so this is
  only useful together with an end-G-code change or a firmware-side notion of
  "trusted since last print".
- **The previous mesh is still in RAM.** Upstream UBL's `G29 S`/`G29 L` slot
  storage is **not** compiled into Buddy (the only reference to
  `settings.load_mesh` is inside `UBL_DEVEL_DEBUGGING` dead code), so there is
  no persistent mesh. But `z_values` keeps the last print's mesh until the next
  `G29 P1` invalidates it, so "verify a few points against the previous mesh,
  re-probe only on change" needs no EEPROM work as long as the printer was not
  power-cycled.
- **Nozzle-cleaner-lite precedent** – on units with the accessory the firmware
  probes the touchpoint only 20 °C below the cleaning temperature, which is a
  data point that probing much closer to print temperature is workable when
  the nozzle has just been wiped.
- **Pre-parsed temperatures** – `GCodeInfo::get_hotend_preheat_temp()` and
  `get_bed_preheat_temp()` are available during the preview; only the XL
  tool-mapping screen uses the bed one today.

---

## 5. End of print → next print (the prototyping loop)

- Stock end G-code: `M104 S0`, `M140 S0`, park at (242, 211), `M84 X Y E`.
  The next print starts **cold** unless the user preheats.
- **Preheat button** (home screen) runs `M1700 T-1`
  (`src/marlin_stubs/pause/M70X_preheat.cpp`): nozzle to the filament's
  `nozzle_preheat_temperature`, bed to its `heatbed_temperature`. The firmware
  presets (`src/common/filament_presets.cpp`) mirror the slicer's probing
  temperatures: PLA/PETG 170, FLEX 210, PC/PA print temperature − 25. A
  finished Load ends in the same state (`DoneHasFilament` sets the preheat
  temperature). Preheating this way therefore costs nothing extra at print
  start; the `M104 S100` idle drop in the start G-code then throws ~10 s of it
  away.
- **End-of-print park** lowers the bed to at least `Z_MAX_POS − 50`
  (`Z_NOZZLE_PARK_POINT_MIN`, `park_head()` in `marlin_server.cpp`) so the part
  can be removed. The next `G28` pays that back as ~12 s of Z travel at
  19 mm/s before the Z taps and sheet detection even start. That, more than
  XY, is what makes the CORE One `G28` feel slow.
- Firmware finishing states (`marlin_server.cpp`): `Finishing_WaitIdle` →
  `retract_and_lift()` + `park_head()` (`PARK_HEAD_ON_PRINT_FINISH`) →
  `Finishing_UnloadFilament` → `Finishing_ParkHead` → `Finished` →
  `finalize_print()` (odometer, input shaper reset, print area reset, job
  result). Cheap.
- "Reprint" (`screen_printing_reprint()`) calls `print_start` with
  `PreviewSkipIfAble::preview` and `ResetToolMapping::no`: skips only the
  thumbnail screen, re-runs all checks and the full start G-code.
- Heaters left on while idle are shut off by the safety timer (the comment in
  `print_start()` says 30 minutes), which is the backstop for any "keep warm
  between iterations" idea.

---

## 6. Idea backlog

Seeded from the map above. Each entry: expected saving (est.), where the change
lives, and the main risk.

### 6.1 Attack the thermal round trip (largest win on a preheated machine)

1. **Probe closer to print temperature.** Slicer-only first step: `HT_MBL10` in
   the PLA filament notes (−10 °C instead of −45 °C). Always saves ~15–20 s of
   the final reheat; also removes most of the cool-down in the cases where
   the nozzle starts hot. The firmware preheat temperature for the filament
   should be raised to match so the Preheat button lands on the new probing
   temperature. Risk: more taps rejected by the analysis, more `G29 P9`
   retries, blobs. Needs a tap-quality measurement at 195–205 °C.
2. **Fan-assisted cool-down** via `M109 R… F`. Saves maybe half of the
   cool-down when probing at 170 °C is kept. Risk: none obvious; verify the
   fan restore path and heatbreak fan behaviour during `G29`.
3. **Retract-and-probe-hot** mode in firmware: a `G29` variant that trusts
   the retract + cleanup and does not require a probing temperature at all.
   Saves the entire round trip. Risk: analysis robustness; needs data.
4. **Skip the idle-temperature drop** on CORE One when the bed is already at
   target (the drop exists to limit ooze during a long bed/heat-soak wait).
   Slicer conditional on nothing available today, so firmware would need to
   expose "bed already at target", or the slicer could use `M104.1`-style
   early heating semantics. Saves 5–15 s.

### 6.2 Homing

5. **`G28 Q` instead of `G28`** plus an end G-code that leaves XY motors on
   (or a firmware "trusted since last finished print" flag). Saves 15–30 s on
   a reprint. Risk: user bumps the gantry while removing the part → first layer
   at the wrong XY. Mitigation: keep the Z home (it is the mesh reference and
   cheap), only skip precise XY refinement, or re-home XY imprecisely
   (`G28 I`) which is a few seconds.
6. **Skip sheet detection on reprint** when the previous print ended normally.
   Saves a few seconds. Risk: sheet removed to pop the part is the common
   case, so probably not worth it.

### 6.3 Mesh

7. **Reuse the previous mesh after a cheap verification.** The last mesh is
   still in RAM (section 4). A new `G29` mode would tap a handful of check
   points (the outermost major points of the print area, or the four sheet
   corners), compare with the stored values within a tolerance (~0.05 mm),
   keep the mesh if they agree and fall back to a full `G29 P1` otherwise.
   Saves ~15–20 s on a small part (12 taps → 4, one 5 mm first descent
   instead of two) and 40 s or more on a large one. Invalidate on power
   cycle, crash, Z alignment, sheet change. Risk: debris between check
   points; frame drift between prints at a different bed temperature.
8. **Fewer points for small parts**: the print area is already honoured, and a
   ≤ 35 mm part probes 9 + 3 points. The 3-point purge-area probing could be
   dropped when the purge line is shortened or moved (see 10).

### 6.4 Overlap and shave

9. **Preheat during the preview.** Set bed and nozzle (to the probing temp, or
   the idle temp) as soon as `GCodeInfo` is loaded, for all printers, not only
   on the XL tool-mapping screen. Revert on Back. Overlaps 20–60 s of heating
   with the user's confirmation and the dialogs. Risk: heating a printer the
   user decided not to print on; the safety timer bounds it.
10. **Shorter purge line / purge into the wastebin** on INDX printers. Saves
    ~5–8 s. Slicer-only.
11. **Residency times**: 5 s nozzle + 5 s bed + 5 s nozzle again = 15 s of
    pure waiting per print. INDX already uses 1 s for the nozzle. Risk: first
    layer temperature overshoot/undershoot.
12. **`M190 S` instead of `M190 R`** on CORE One so a hotter bed from a
    previous material does not block the start (MK4S already uses S).
13. **New-firmware dialog** (30 s auto-timeout) and selftest reminders are
    startup blockers for real users even though they are not "time in
    G-code". Consider shortening the timeout or defaulting to Ignore after one
    dismissal.

### 6.5 Workflow

14. **"Quick iteration" printer profile in the slicer**: a start G-code variant
    with `HT_MBL10`, `M109 … F`, `G28 Q`, mesh reuse and a short purge, and an
    end G-code that keeps the nozzle at idle temperature and the motors on.
    Most of the saving with no firmware change, at the cost of safety margins
    the stock profile deliberately keeps.
15. **Keep-warm after print** as a printer setting (end G-code `M104 S0`
    → idle temp), with the safety timer as the backstop.

### 6.6 Round 2: CORE One+ focus

16. **"Why does preheat go to 215 and not 170?"** It does not, when the
    Preheat button or a Load is used (section 5). If 215 shows up after a
    preheat on your unit, the likely causes are a custom filament profile
    whose preheat temperature equals its print temperature, or a temperature
    set from Control → Temperature. Worth checking on the printer before any
    code change; the fix would be in the filament profile, not in firmware.
17. **Fan-assisted cool-down (`M109 R… F`).** Slicer-only, and it only matters
    when the nozzle is above the probing temperature, so after the Preheat
    button it changes nothing. Where it applies it should roughly halve the
    1–2 min. Two things to verify: the fan is held at 100 % until the 5 s
    residency is met (`wants_to_cool` is only evaluated when the target
    changes), so the heater has to hold 170 °C against full fan; and the
    probing temperature's purpose is a dry nozzle, which fan cooling does not
    change. The stock profile already runs `M106 S70` during the idle drop
    when the bed is ≤ 60 °C, so the pattern is accepted.
18. **"Is the printer ready?" pre-print preparation with a STOP.** Today
    "Ready" exists only for Connect (`MarlinPrinter::set_printer_ready` in
    `src/connect/marlin_printer.cpp`): it changes the reported `DeviceState`
    so the queue may start the next job, and moves nothing. Proposal: when the
    user preheats or sets Ready, ask once whether the bed is clear, then run
    homing, sheet detection and optionally the Z alignment and the mesh
    verification in the background, in its own FSM with an abort button
    (`marlin_server::FSM_Holder` pattern, as `PhasesSelftest::CalibZ` uses).
    For the start G-code to benefit, bare `G28` must become `G28 Q` (slicer)
    or the firmware must treat a bare `G28` as `Q` while the pre-print trust
    is fresh. Position trust is dropped by `disable_X/Y/Z`
    (`lib/Marlin/Marlin/src/module/stepper/indirection.h`), by a crash and by
    the toolchanger, so "fresh" has a natural definition. Saves most of the
    30–40 s `G28` including the Z travel, overlapped with the user's own
    actions.
19. **Keep XY homing, add Z alignment around every print.** Z alignment is
    `selftest::calib_Z()` (`src/marlin_stubs/G162.cpp`, exposed as `G162 Z`):
    it drives the bed to the bottom hard stop at 19 mm/s, pushes 5 mm past it
    at 15 mm/s so both motors sync, backs off 1 mm, then returns to Z = 50 and
    leaves Z unhomed. From the top that is ~30 s; from the end-of-print park
    position (bed already within 50 mm of the bottom) only the push and the
    return remain, ~15 s, and the return can be dropped when the bed is left
    low for part removal anyway. So "align after every print" fits at the end
    of the finishing sequence or in the end G-code. A "little tap before" is
    what the Z home at (10, 10) already is; comparing one more tap on the far
    side of the bed against the previous mesh would catch a Z screw that
    moved and trigger the alignment only when needed (same mechanism as 7).
20. **Cursory check instead of full probing.** See idea 7, now sized for the
    RAM mesh. For a small part the full probing is already only 9 + 3 taps;
    the perceived "forever" is the sum of P9 cleanup taps, two 5 mm first
    descents, the reheat residency and the purge, so idea 7 alone buys
    ~15–20 s on small parts.
21. **Run cleanup and the nozzle reheat during the heat soak.** On the
    CORE One+ (`PrinterVariant::plus`, `src/common/printer_variant/coreone.cpp`)
    `ejg2_installed` is false, so the soak threshold is room temperature and
    PLA at 60 °C does soak when the frame estimate is not settled (the model
    in `updateTemperaturesFromRawValues` reaches 60 °C in ~150 s from cold).
    After a Preheat of a few minutes it is zero. Only the Gen2 edition sets
    `ejg2`, which skips the soak up to 85 °C and doubles the model's rate.
    Where the soak is non-zero, the slicer order could be `M109 R170` →
    `G29 P9` → `M104 S100` → `G29 G` → `M109 R170` → MBL, overlapping the
    cleanup with the soak; the reheat cannot overlap unless the firmware
    raises the nozzle target when soak progress passes ~80 %. The idle drop
    to 100 °C exists to avoid cooking filament during long soaks, so skipping
    it when the soak is predicted short is also fair game. The soak is already
    skippable from the screen (`SkippableGCode`).
22. **Does the slicer's time estimate include this?** No. PrusaSlicer's time
    estimator only simulates motion; `M109`/`M190`/`G28`/`G29` waits are not
    modelled, and the printer's remaining time comes from the slicer's `M73`.

---

## 7. Things to measure before changing anything

- A real **startup timeline** from a preheated CORE One and MK4S: timestamp
  each `PrintStatusMessage` phase change (`homing`, `probing_bed`,
  `nozzle_cleaning`, `waiting_for_hotend_temp`, `absorbing_heat`, …,
  `src/feature/print_status_message/print_status_message.hpp`) and each
  start-G-code line as it leaves the queue. The metrics system
  (`METRIC_DEF`, e.g. `probe_analysis` in `probe.cpp`) is the natural sink.
- **Nozzle cool-down curve** 215 → 170 with and without the part fan.
- **Tap rejection rate vs. nozzle temperature** (170 / 190 / 205 / 215 °C) with
  the stock 2 mm retract, to size ideas 1 and 3.
- **Mesh drift between consecutive prints** at the same bed temperature, to
  size idea 7.
- How often users actually hit the new-firmware and selftest dialogs.
- What the nozzle target on your CORE One+ actually is right after Preheat
  and right after a Load (expected: the filament's preheat temperature, 170
  for PLA) and after a manual temperature change.
- How long `G29 G` actually waits on the CORE One+ when Print is tapped right
  after the bed reaches 60 °C, versus after a few minutes of preheat.

---

## Appendix: file map

| Topic                                   | Location |
|-----------------------------------------|----------|
| Print start request, states             | `src/common/marlin_server.cpp` (`print_start`, `_server_print_loop`, `media_print_loop`, `finalize_print`) |
| Preview state machine and checks        | `src/common/marlin_print_preview.cpp`, `.hpp` |
| Skip levels                             | `src/common/marlin_events.h` (`PreviewSkipIfAble`) |
| Metadata scan                           | `src/common/gcode/gcode_info_scan.cpp`, `src/common/gcode/gcode_info.cpp` |
| Media prefetch                          | `src/common/media_prefetch/media_prefetch.cpp` |
| GUI entry points                        | `src/gui/screen_filebrowser.cpp`, `src/gui/screen_home.cpp`, `src/gui/screen_printing.cpp` (`screen_printing_reprint`), `src/gui/screen/screen_print_preview.cpp` |
| Connect/Link entry                      | `src/connect/marlin_printer.cpp` |
| Autostart                               | `src/common/print_utils.cpp` |
| Homing                                  | `lib/Marlin/Marlin/src/gcode/calibrate/G28.cpp`, `lib/Marlin/Marlin/src/module/motion.cpp` (`homeaxis`) |
| Mesh leveling, cleanup, heat soak       | `lib/Marlin/Marlin/src/gcode/bedlevel/ubl/G29.cpp`, `lib/Marlin/Marlin/src/feature/bedlevel/ubl/ubl_G29.cpp` (`probe_major_points`), `lib/Marlin/Marlin/src/module/probe.cpp` (`run_z_probe`, `cleanup_probe`) |
| Heat-and-wait                           | `lib/Marlin/Marlin/src/gcode/temperature/M104_M109.cpp`, `M140_M190.cpp`, `lib/Marlin/Marlin/src/module/temperature.cpp` (`wait_for_hotend`, `wait_for_bed`, `wait_for_frame_heatup`) |
| Residency / window constants            | `include/marlin/Configuration_<PRINTER>.h` (`TEMP_RESIDENCY_TIME`, `TEMP_WINDOW`, probe speeds, mesh grid, homing feedrates) |
| Nozzle cleaner lite                     | `src/feature/nozzle_cleaner_lite/` |
| Preheat button, load/unload temps       | `src/marlin_stubs/pause/M70X_preheat.cpp`, `src/marlin_stubs/pause/M701_2.cpp`, `src/common/filament_presets.cpp` |
| Z alignment                             | `src/marlin_stubs/G162.cpp` (`selftest::calib_Z`) |
| CORE One editions (EJG2, cleaner lite)  | `src/common/printer_variant/coreone.cpp` |
| Ready mode for Connect                  | `src/connect/marlin_printer.cpp`, `src/state/printer_state.cpp` |
| Idle heater safety timer (30 min)       | `src/feature/safety_timer/safety_timer.hpp` |
| Status phases shown on screen           | `src/feature/print_status_message/print_status_message.hpp` |
| Slicer start/end G-code                 | PrusaSlicer `resources/presets/prusa-research-fff/PrusaResearch/preset-printer-{coreone,mk4,xl,mini,coreone-indx}.yaml` |
