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

### 1.1 Projected budget after the round-2 improvements

Same scenario (CORE One+, PLA 215 / 60, small part, preheated for a few
minutes). Improvement set assumed, numbers refer to the backlog in section 6. Each
item has a firmware half and a slicer half; the firmware half of every
item is *landed* (on the branch, documented in section 8), the slicer
half is a start-G-code / filament-profile change that still has to be made
in the PrusaSlicer bundle (`preset-printer-coreone.yaml`,
`preset-filament-*.yaml`):

- **Prep while idle** (18): after Preheat or Ready the user confirms the bed
  is clear; the printer homes, detects the sheet and verifies the mesh (7)
  while still idle, with a Stop button. Firmware: F5, *landed* (`M1706`,
  offered after Preheat and from Control → Prepare for Print). Slicer:
  `G28 Q` instead of `G28` in the start G-code and `G29 P9` dropped; or,
  with the stock lines, the "Trust Prep in Start G-code" setting, which
  makes the firmware treat a bare `G28` as `G28 Q` and skip `G29 P9` while
  prepared.
- **Z alignment at the end of every print** (19), where the bed is already
  at the bottom. Firmware: F4, *landed* (`G162 Z R0`, setting "Z Align
  After Print", default off). Slicer: nothing.
- **Trust the prep** (18): the start G-code issues `G28 Q`; the firmware
  keeps position trust until motors are disabled, a crash or a power cycle.
  Firmware: F5, *landed* (prepared state, steppers kept on). Slicer:
  `G28 Q`.
- **Verify the mesh instead of re-probing** (7): Firmware: F2, *landed*
  (`G29 P11`). Slicer: `G29 P11` in place of the first `G29 P1`.
- **Probe hotter** (1): probing at print temperature − 10, with the firmware
  preheat preset raised to match so Preheat lands there too. Firmware: F6,
  *landed* (PLA 205, PETG 220 on loadcell printers). Slicer: `HT_MBL10` in
  the PLA/PETG filament notes. Until the slicer half ships, a Preheat
  followed by a stock-profile print *regresses* by ~1 min of passive
  cool-down (section 8.7).
- **No idle drop when no soak is pending** (4, 21), cleanup and reheat
  overlapped with the soak when there is one (21). Firmware: F3, *landed*
  (`G29 G S<temp>` raises the nozzle target near the end of the soak).
  Slicer: `G29 G S<probe temp>`; dropping the `M104 S100` idle drop when
  no soak is pending is slicer-only and not done.
- **Bed residency tracked continuously** (23) so `M190` returns at once when
  the bed has already been at target for 5 s. Firmware: F1, *landed*, no
  slicer change.
- **Shorter purge** (10). Slicer-only, not done.
- **Measurement** (section 7): F0, *landed*. Every start-G-code command and
  every status phase is stamped relative to the Print tap (`pstart_*`
  metrics / `pstart` log lines) when metrics are enabled.

Slicer-side lines in one place: `G28 Q`, `G29 P11`, `G29 G S<probe temp>`,
`HT_MBL10` (filament notes), and dropping `G29 P9` for the "prep run"
column (the last two are replaced by the "Trust Prep in Start G-code"
setting when the stock lines stay). None of them is in the stock profiles
yet; the "After" columns of the table assume all of them.

| Step (start-G-code order)                | Today       | After, prep not run | After, prep run | Changed by |
|------------------------------------------|-------------|---------------------|-----------------|------------|
| Preview, scan, prefetch, dialogs         | < 1 s + user| < 1 s + user        | < 1 s + user    | – |
| `M109 R<probe>` nozzle wait              | 0           | 0                   | 0               | Preheat preset = probe temp (1) |
| `G28` / `G28 Q`                          | 30–40 s     | 30–40 s             | ~0              | 18 |
| `G0 Z40`, idle drop, `M190`              | ~7 s        | ~2 s                | ~2 s            | 4, 23 |
| `G29 G` heat soak                        | 0 (soaked)  | 0                   | 0               | – |
| `M109 R<probe>` reheat from idle drop    | 5–15 s      | 0                   | 0               | 4 |
| `G29 P9` cleanup                         | 5–10 s      | 5–10 s              | 0 (done in prep)| 18 |
| Mesh probing                             | 25–35 s     | ~10 s (4 verify taps)| 0 (done in prep)| 7, 18 |
| `M109 S215` reheat + 5 s hold            | 20–30 s     | 10–12 s (from 205)  | 10–12 s         | 1 |
| Purge line + wipe                        | ~10 s       | ~5 s                | ~5 s            | 10 |
| **Total after tapping Print**            | **2–2.5 min** | **~1–1.5 min**    | **~20–30 s**    | |

Work moved out of the start, not removed: ~15 s of Z alignment at print end,
and ~40–50 s of homing, sheet detection and mesh verification during the
preheat, both overlapping with the user removing the part and loading the
next job. With a nozzle that started above the probing temperature, fan
assist (17) halves the extra 1–2 min; the prep cannot remove it.

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
23. **Track bed residency continuously.** `wait_for_bed()`
    (`lib/Marlin/Marlin/src/module/temperature.cpp`) starts its 5 s residency
    timer only when called, so `M190` costs at least 5 s even when the bed has
    been at target for minutes. The hotend already tracks residency
    continuously (`BaseHotend::manage_temp_residency`), which is why `M109`
    returns at once in the same situation. The TODO at the top of
    `wait_for_bed` asks for exactly this. Saves 5 s per `M190`.

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

## 8. Implemented (firmware half, round 2)

Commits on `claude/focused-hamilton-onqmge` after `f1a0ccaa`, in the order
they landed: `9d20ce29` (F0), `a308595d` (F1), `f4a9318a` (F3), `333ff027`
(F6), `518b85ca` (F4), `903926c3` (F2, with fixups `97849020` and
`63947d97`), `58f0e6a5` (F5, with fixup `d6a0ae42`). The fixups are to be
squashed by the maintainer before merging. On-printer steps
for every item are in `doc/print_startup_local_test_plan.md`; the numbers
below are *est.* until that plan has been run and the measured values are
written into the table in section 1.1.

### 8.1 F0 Print startup timeline

**What**

`src/feature/startup_timeline/` records the print startup relative to
`marlin_server::print_start()`: every top-level G-code dequeued from the
Marlin queue (hook in `GcodeSuite::process_next_command()`, so the stamp is
the dequeue time) and every `PrintStatusMessage` phase change, both when a
guard sets a new type and when a guard is destroyed (the type then shown,
usually `none`, is reported). Each event is one custom metric point and one
`MarlinServer` info log line. Metrics: `pstart_gcode` (`t=<ms>i,i=<index>i,
c="<command, max 20 chars>"`), `pstart_phase` (`t=<ms>i,p=<type>i,n="<short
name>"`), `pstart_note` (`t=<ms>i,n="<marker>"`). Log lines: `pstart +0ms
armed`, `pstart +<ms>ms gcode <index> <full command>`, `pstart +<ms>ms phase
<type> <name>`, `pstart +<ms>ms note <marker>`. The short phase names are
untranslated (`homing`, `sheet_detect`, `wait_bed`, `wait_hotend`,
`absorbing_heat`, `nozzle_cleaning`, `probing_bed`, `additional_probing`,
`verifying_mesh`, `none`, …). Sub-commands run through
`process_subcommands_now()` are not listed. `buddy::startup_timeline::note()`
is the marker API: F2 emits `mesh_verify:<result>` from every `G29 P11`
decision and F5 emits `prep:<step>`; both are recorded only while a print
start has armed the timeline, so during an idle preparation the `prep:*`
notes are dropped (the `pre_print_prep` metric carries them instead).

**How to enable**

Nothing to configure beyond metrics: the timeline is armed in
`print_start()` only when `are_metrics_enabled()` is true (Settings →
Network → Metrics & Log → Host set, "Enable Metrics" on, or `M334 <host>
<metrics port> <syslog port>`). It disarms itself after 160 commands or at
the end of the print, so with metrics off the cost is one branch per command
and per status change. The log line is emitted only while armed, so it is
also gated by the metrics switch. The three `pstart_*` metrics are
`METRIC_ENABLED` by default, no `M331` needed.

**How to test on the printer**

`doc/print_startup_local_test_plan.md` section 1. Key signals: `pstart +0ms
armed` right after the Print tap; one `pstart … gcode <i> …` line per
start-G-code line in file order; `pstart … phase <p> homing` →
`sheet_detect` → `none` around `G28`; in the collector the `pstart_gcode` /
`pstart_phase` measurements with fields `t`, `i`/`p`, `c`/`n`. With metrics
disabled: no `pstart` lines at all.

**Expected timeline change**

None. F0 is the instrument; it adds well under a millisecond per command.
The `t` of each phase change from a stock-profile print after a Preheat is
the "Today" column of the table in section 1.1 (rows "`G28` / `G28 Q`" =
`homing` enter → `none` after `sheet_detect`; "`G0 Z40`, idle drop, `M190`"
= `wait_bed` enter → leave; "`G29 P9` cleanup" = `nozzle_cleaning`; "Mesh
probing" = `probing_bed` enter → `additional_probing` leave; "`M109 S215`
reheat" = the second `wait_hotend`).

### 8.2 F1 Continuous bed residency

**What**

`Temperature::wait_for_bed()` used to start its 5 s residency timer only
when called, so every `M190` cost at least `TEMP_BED_RESIDENCY_TIME` (5 s)
even on a bed that had sat at target for minutes. The bed residency now
lives in a `TempResidency` tracker
(`lib/Marlin/Marlin/src/module/temperature/temp_residency.hpp`) advanced
from `manage_heater()`: the timer starts when the bed enters
`TEMP_BED_WINDOW` (1 °C), is cleared when it leaves `TEMP_BED_HYSTERESIS`
(3 °C) and starts again once the bed re-enters the window; the target counts
as reached after 5 s inside the band, or at once when the target is 0. A
target *change* resets it; re-sending the same target (`M140 S60` then
`M190 R60`) does not. `wait_for_bed()` returns on its first iteration when
the residency is already satisfied and logs `Bed residency already
satisfied, not waiting` (`MarlinServer`); otherwise it waits as before (`R`
waits for cooling, `S` returns while cooling, the `MIN_COOLING_SLOPE` abort
is unchanged). `is_bed_temperature_reached()` now reports the residency, so
pause and power-panic resume can wait up to 5 s longer and no longer resume
with the bed up to 3 °C short. The hotend keeps its own residency code.

**How to enable**

Always on, no setting, no G-code change. Nothing in the slicer profile needs
to change; `M190 R<bed>` simply returns at once when the bed has been at
target for > 5 s.

**How to test on the printer**

`doc/print_startup_local_test_plan.md` section 2. Key signals: log line
`Bed residency already satisfied, not waiting` on the second `M190 R60`; no
"Waiting for bed" status flash on the screen; on the F0 timeline the
`wait_bed` phase is absent or < 0.3 s on a preheated bed, versus ~5 s on the
first `M190` after a target change.

**Expected timeline change**

Section 1.1 row "`G0 Z40`, idle drop, `M190`": ~7 s → ~2 s (the 2 s is the
`G0 Z40` move) whenever the bed has been within 1 °C of target for > 5 s
when `M190` runs, i.e. after any Preheat or a previous print. Does not apply
to a bed that is still heating, or when the bed target changed between
`M140` and `M190` (different material, bedlet mask change on XL). Saves 5 s
per `M190`, also for the one in the MK4S profile.

### 8.3 F2 Mesh verification `G29 P11`

**What**

`G29 P11` re-uses the mesh left in RAM by the previous print when a few
check taps agree with it: it taps the outermost major grid points of the
probe area (`N` 3–5, default 4: far-left, far-right, near-right, near-left,
then the centre), optionally the Z homing point (`Z`, expected to read 0),
compares each tap with `z_values` within `O` mm (default 0.05; `T` is taken
by the map type) and, when every tap agrees, keeps the mesh, activates it
if it has no NaN and marks it *fresh*. On a mismatch, an invalid mesh, a
check point without a value (`incomplete`) or no reachable check point
(`unreachable`) it invalidates the mesh and probes the area exactly as
`G29 P1` does (serial: `Mesh verification failed. Probing mesh.`). A fresh
mesh (probed or verified since the last print ended) makes `P11` tap
nothing; `M` forces the taps. New status `Verifying mesh` with `n/N`
progress (`PrintStatusMessage::verifying_mesh`, short name
`verifying_mesh` on the F0 timeline), log lines `Mesh verify tap (x, y):
<measured> expected <stored> diff <d>` per tap and `Mesh verify: <ok|fresh|
invalid|incomplete|mismatch|unreachable|probe_failed>, <n> taps, max diff
<d> mm, tol <o> mm` (`Marlin` component), metric `mesh_verify r="<result>",
n=<taps>,d=<max diff>`, and the F0 timeline note `mesh_verify:<result>`
(`pstart_note`, fixup `63947d97`). Validity tracking: the mesh is trustworthy only
after a complete non-extending `P1` over an area that covers the requested
one, with the same `probe_offset.z` (so Live-Z, sheet profile and `M851`
changes count) and a bed target within 10 °C. Cleared by boot, `G29 P0`,
`G29 P1`, `G29 I`, `G29 Q`, `M502`, crash recovery, Z alignment (`G162 Z`
and the F4 end-of-print alignment, log `Mesh invalidated: Z align`) and
mesh edits (`G29 P5 C`, `G29 P6`, `M420 C`, `M421`); freshness is cleared
at the end of every print. The end-of-print `print_area.reset_bounding_rect()`
no longer zeroes the mesh (UBL printers): leveling is only disabled, so
`G29 A` after a manual `M555` enlargement activates the old mesh. On MK3.5
and MINI the comparison removes the axis correction that `z_values` holds.

**How to enable**

Slicer start G-code: replace the first `G29 P1` with `G29 P11` (keep
`G29 P1 X150 Y0 W100 H20 C`, `G29 P3.2`, `G29 P3.13`, `G29 A` as they are;
the `C` line probes nothing after a successful verify because every point
already has a value). Defaults need no parameters; `G29 P11 N5 O0.1 Z M`
for diagnostics. Stock `G29 P1` behaviour is unchanged. The `mesh_verify`
metric is `METRIC_ENABLED`.

**How to test on the printer**

`doc/print_startup_local_test_plan.md` section 3. Key signals: screen
"Verifying mesh 1/4 … 4/4"; log `Mesh verify: ok, 4 taps, max diff 0.0xx mm,
tol 0.050 mm` and `Mesh verify: fresh, 0 taps …` on the immediate repeat;
metric `mesh_verify r="ok",n=4,d=0.0xx`; in a print also `pstart … note
mesh_verify:ok`; a `mismatch` (paper under a corner) followed by the full
`Probing bed` phase. The "Verifying mesh" wait screen
has no STOP button (same as "Probing bed"); a running verify is stopped
from the printing screen only.

**Expected timeline change**

Section 1.1 row "Mesh probing": 25–35 s (9 + 3 taps, two 5 mm first
descents) → ~10 s (4 taps, one descent) for the second and later prints of a
session with `G29 P11` in the start G-code ("After, prep not run" column);
→ ~0 s when the mesh is fresh, which only F5 produces at print start
("After, prep run" column). Does not apply, and the full probe runs, on the
first print after a power cycle, after a Z alignment (every print when the
F4 toggle is on, see 8.5), after a Live-Z / sheet / `M851` change, after a
crash, when the bed target moved by more than 10 °C (PLA → PETG) or when the
new `M555` area is not covered by the probed one. The purge-area
`G29 P1 … C` points are not re-verified.

### 8.4 F3 `G29 G S<temp>` early hotend target during the heat soak

**What**

`G29 G` (absorb heat, `Temperature::wait_for_frame_heatup()`) takes an
optional `S<temp>`. When the soak is predicted to finish within 20 s
(`early_hotend_lead_s`, prediction from
`lib/Marlin/Marlin/src/module/temperature/bed_frame_soak.hpp`, the same
slew model the estimate uses, EJG2 speed-up included), or at once when there
is nothing to wait for (frame already stable, bed target below the current
estimate, bed target below the soak threshold) or when the user presses
Skip, the hotend target of the picked tool (`Hotend::for_tool`) is set to
`S`, so the `M109 R<temp>` that follows finds the nozzle at or near the
temperature. An aborted wait (`planner.draining()`), a dry run, or no tool
picked (toolchanger/INDX, log `Absorbing heat: no tool picked, hotend target
<t> not set`) leaves the target untouched. Log line: `Absorbing heat:
setting hotend target <t> early (<already stable|cooling|below soak
threshold|soak nearly done|skipped by user|soak done>, predicted <n> s
left)` (`MarlinServer`). `S` outside 0..`HEATER_0_MAXTEMP` is rejected
(`?(S)oak hotend target implausible.`). Without `S`, or without `G`, the
behaviour is byte-for-byte the old one; the upstream UBL `S` (mesh storage
slot) was never compiled in, so the letter is free with `G`.

**How to enable**

Slicer start G-code only: `G29 G` → `G29 G S<probe temp>` with the same
expression the profile uses for `M109 R<probe temp>` (170 today, print
temperature − 10 with `HT_MBL10`). No setting, no default change.

**How to test on the printer**

`doc/print_startup_local_test_plan.md` section 4. Key signals: nozzle target
jumps to `S` while "Absorbing heat" is still on screen, ~20 s before the
soak ends (log `… (soak nearly done, predicted 19 s left)`); on a warm
printer `G29 G S205` returns at once with the target set (log `(already
stable, predicted 0 s left)`); on the F0 timeline the `wait_hotend` phase
after `absorbing_heat` shrinks from 5–15 s to ≤ 2 s.

**Expected timeline change**

Section 1.1 row "`M109 R<probe>` reheat from idle drop": 5–15 s → 0–2 s,
but **only when the soak is non-zero** (Print tapped within the first ~2–3
minutes after the bed reached target on a CORE One+, which is when the
reheat can overlap the soak). When `G29 G` is skipped outright (bed soaked
for minutes, the common Preheat case) the target is raised at the moment
`G29 G` returns and the stock order (`M109 R` immediately after `G29 G`)
still pays the full reheat from 100 °C; the table's "0" for that row
belongs to the slicer-side item 4 (no idle drop when no soak is pending),
not to F3. Nothing changes without `S`. The residency itself is still 5 s
(`TEMP_RESIDENCY_TIME`), tracked continuously by the hotend as before.

### 8.5 F4 `G162 R` and Z alignment after print

**What**

`G162 Z` takes `R<0|1>`: `R0` skips the return move to Z = 50 after the
push into the bottom hard stop (`selftest::calib_Z(false)`), `R1` keeps the
return. Default `R1` on every printer except iX (`R0`, as before); on XL
`R` is ignored because its alignment never returned to Z = 50. Z is left
unhomed either way. New config store item `z_align_after_print` (default
off) with the Settings switch "Z Align After Print" (top-level Settings
list, after "Stuck Filament Detection"; shown on loadcell printers except
XL). When on, `align_z_after_print()` in `src/common/marlin_server.cpp`
runs `calib_Z(false)` in `State::Finishing_ParkHead`, after `lift_head()`
parked the bed at ≥ `Z_MAX_POS − 50` and `pre_finalize_print()` retracted /
docked, only from the outer server loop (`idle_running` guard) and only
while Z is still trusted (XY trust is already dropped there). Skipped for
serial prints and on the Aborting path; a preview abort after the job
started also passes through the finishing states and aligns. The screen
shows the "Calibrating Z" selftest dialog over the printing screen, then
"Printed". The alignment calls `ubl.invalidate_mesh_validity("Z align")`
(F2), so the next print has to probe a full mesh; that is why the default is
off. `fatal_error(ERR_ELECTRO_HOMING_ERROR_Z)` inside `calib_Z` is now
reachable at the end of a print.

**How to enable**

G-code: `G162 Z R0` (bed stays at the bottom), `G162 Z` / `G162 Z R1`
(returns to Z = 50). Menu: Settings → "Z Align After Print" → on; survives
a power cycle. No slicer change. Recommended only together with F5 (the
preparation then absorbs the full re-probe while idle); with the stock
profile it costs nothing extra because `G29 P1` re-probes anyway.

**How to test on the printer**

`doc/print_startup_local_test_plan.md` section 5. Key signals: after the end
G-code and the park, the "Calibrating Z" dialog, the bed pushes into the
bottom stop and backs off 1 mm, then the "Printed" screen; afterwards Z is
unhomed (the next Z move homes first) and the log has `Mesh invalidated: Z
align`; with the toggle off, "Printed" follows the park immediately.

**Expected timeline change**

No row of the start table changes by F4 itself. It adds, at the end of a
finished print with the toggle on, an est. 5–8 s between the park and
"Printed" (≤ 50 mm at 19 mm/s, the 5 mm push at 15 mm/s and the 1 mm
back-off; the spec's earlier "~15 s" assumed the return move, which `R0`
drops). From the top (`G162 Z` from the Calibrations menu) it stays ~30 s.
Interaction with F2: with the toggle on, every print's `G29 P11` finds the
mesh invalid and the "Mesh probing" row is back at 25–35 s unless the F5
preparation ran the full probe while idle; with the toggle off, F4 changes
nothing.

### 8.6 F5 Pre-print preparation (`M1706`, prepared state)

**What**

`src/feature/pre_print_prep/` (printers with `HAS_PRE_PRINT_PREP`: CORE One
and MK4, i.e. loadcell, sheet detection, auto-retract and a known slicer
cleanup rectangle; not the INDX / L variants). `M1706 [ S0 | C0 | Z | M ]`
runs, inside one `ClientFSM::PrePrintPrep` dialog titled "Prepare for
Print": the question "Is the bed clear and the steel sheet in place? …"
(Yes / No; `S0` skips it), then with a Stop button: a full precise `G28`
with steel-sheet detection (`G28_no_parser`, as the slicer's bare `G28`),
the slicer's nozzle-cleanup taps `G29 P9 X208 Y-2.5 W32 H4` (MK4: `G29 P9
X10 Y-4 W32 H4`; `C0` skips; `cleanup_probe()` does its own retract), then
Live-Z reset (`probe_offset.z = 0`, as print init does), the print area
reset to the whole bed and `G29 P11` (`Z`, `M` pass through), `G29 P3.2`,
`G29 P3.13`, `G29 A`. On success the dialog simply closes and the printer
is *prepared*: the stepper inactivity timeout is suspended so XY keep
their position trust, and the F2 mesh stays fresh. The state ends
(`prepared state ended (<reason>)`) on `trust_lost` (any axis below the
precise home level: Disable Motors, a crash, `G162 Z`, a full re-home),
`nozzle_cooled` (idle only: the active nozzle target drops below the
target it had at completion), `expired` (30 min idle), `print_finished`
(`finalize_print`) or `manual` (a new preparation starts); every clear
also calls `ubl.clear_mesh_fresh()`. Nozzle and age rules are suspended
while printing because the start G-code owns temperatures. A preparation
started with a cold nozzle (Control menu, no Preheat) records a 0 target,
so the nozzle rule never fires and the state, with the motors kept on, is
bounded only by the 30 min, trust loss or a print end. Stop quick-stops
through `GCodeExceptionHandler { GCEHandlerExtent::any_move }`; the
handler calls `set_all_unhomed()` and lifts Z by 5 mm (relative park), then
the "Preparation stopped. The position is no longer known; the next print
will home and probe as usual." frame waits for Continue. Failures (homing
or refinement failed, Abort on the "steel sheet not detected" warning,
Abort in the nozzle-cleaning-failed wizard, probing failed, mesh not
fresh afterwards) end in "Preparation did not finish. The next print will
home and probe as usual." with Continue; Ignore in the sheet warning and in
the cleaning wizard continues (the wizard's Ignore now clears
`g29_nozzle_cleaning_failed`). `G28_no_parser` now reports failure when the
user aborts the sheet warning outside a print. Log component
`PrePrintPrep`: `begin +<ms>ms`, `homed +<ms>ms`, `cleaned +<ms>ms`,
`mesh_ok +<ms>ms`, `done +<ms>ms` (or `stopped` / `failed`), `prepared
(nozzle target <N>)`, `prepared state ended (<reason>)`; metric
`pre_print_prep s="<step>",t=<ms>i` and `s="cleared",r="<reason>"`; F0
timeline notes `prep:<step>` (only when a print window is armed, so
normally absent for an idle prep). While prepared and with the "Trust
Prep in Start G-code" setting on, a bare `G28` (no axis, no `O`/`Q`) is
treated as `G28 Q` (log `G28: printer prepared, homing only if needed`)
and `G29 P9` returns before any move when the preparation ran the cleanup
taps (`PreparedState` keeps a `cleanup_done` bit, false after `M1706 C0`
and cleared with the state; log `G29 P9: printer prepared, nozzle cleanup
skipped`). The home-screen
Preheat now sends `M1700 T-1 P1`; the new `M1700 P` flag runs the
preparation (with its question) after a filament was chosen, when
"Prepare After Preheat" is on. Connect reports the FSM as busy; the
question cannot be answered from Connect yet. Config strings: "Prepare For
Print After Preheat" (`pre_print_prep_after_preheat`, default on),
"Prepare For Print Trusts Start Gcode" (`pre_print_prep_trusts_start_gcode`,
default off).

**How to enable**

Defaults: after every home-screen Preheat the question appears (Settings →
Hardware → "Prepare After Preheat", on). On demand: Control → "Prepare for
Print" (below Auto Home; refused with the printer-busy box while printing
or with G-codes queued), or `M1706` / `M1706 S0 C0 Z M` from the console.
To make the start G-code benefit: either edit the slicer profile (`G28`
→ `G28 Q`, drop `G29 P9`) or keep the stock lines and switch Settings →
Hardware → "Trust Prep in Start G-code" on, which makes the firmware home
only if needed and skip the cleanup taps while prepared (off by default: a
gantry bumped while prepared cannot be detected, and the nozzle may ooze
between the prep and the print). `G29 P1` → `G29 P11` (F2) in either case
so the fresh mesh is reused.

**How to test on the printer**

`doc/print_startup_local_test_plan.md` section 6. Key signals: the
"Prepare for Print" question after Preheat; the running frame showing
Homing → Detecting steel sheet → Nozzle cleaning → Verifying mesh n/4
(or Probing bed n/49 on the first run after boot / Z align); log
`PrePrintPrep … done +<ms>ms` and `prepared (nozzle target 205)`; metric
`pre_print_prep s="done"`; motors still energised after > 2 min idle;
`prepared state ended (nozzle_cooled|trust_lost|print_finished|expired)`
when the state is dropped; in a `G28 Q` + `G29 P11` print, no `homing`
phase and `Mesh verify: fresh, 0 taps`.

**Expected timeline change**

Section 1.1 column "After, prep run": row "`G28` / `G28 Q`" 30–40 s → ~0
(`G28 Q` in the profile or the trust setting; with neither, the bare `G28`
re-homes fully, which also ends the prepared state via `trust_lost`, so
the following `G29 P11` taps 4 points instead of reporting `fresh`); row
"`G29 P9` cleanup" 5–10 s → 0 (the trust setting skips it when the prep
ran the cleanup, or the profile drops the line; after `M1706 C0`, or with
the setting off and the line kept, it still costs its 3+ taps); row "Mesh
probing" 25–35 s → ~0 (`fresh`, and the purge-area `G29 P1 … C` probes
nothing). Total after the Print tap est. ~20–30 s. Work moved to idle time:
est. 40–50 s per preparation (home, sheet, cleanup, 4 taps), ~100 s when
the whole bed has to be probed (first prep after a power cycle, after
`G162 Z` / the F4 end-of-print alignment, after a crash, Live-Z/`M851`
change or a bed target change > 10 °C). Does not apply when the state was
dropped before the print: part removed after a previous print
(`print_finished`), Disable Motors, cooldown below the prep-time target,
30 min idle, or a Stop.

### 8.7 F6 Preheat presets at print temperature − 10

**What**

`src/common/filament_presets.cpp`: the PLA and PETG presets'
`nozzle_preheat_temperature` is `215 − 10 = 205` and `230 − 10 = 220` when
`HAS_LOADCELL() && !HAS_INDX()`, 170 otherwise (INDX keeps 170 because its
tool-offset calibration probes Z at the preheat temperature; MINI and MK3.5
are unchanged). The preset preheat is what the home-screen Preheat
(`M1700 T-1`) and a finished Load (`M701`) leave the nozzle at, and it is
meant to equal the slicer's probing temperature so the first
`M109 R<probe>` is free; the PrusaSlicer PLA/PETG profiles are moving to
`HT_MBL10` (first layer temperature − 10). Other readers of the value move
with it: chamber filtration's "hotend is hot" threshold for PLA/PETG
(175 → 210/225), the XL lite nozzle cleaner's cleaning temperature in tool
offset calibration (PLA cleans at 205 and probes at 185, PETG 220/200),
OpenPrintTag tags without a preheat field. Autoload, unload, `M600`, cold
pull and first-layer calibration never read it; user and ad-hoc filaments
keep their stored preheat. Presets are `constexpr`, no config-store
migration.

**How to enable**

Always on after flashing (loadcell, non-INDX printers). The matching slicer
half is the filament profile: `filament_notes` containing `HT_MBL10` for
the PLA/PETG profiles, which makes the start G-code probe at
`first_layer_temperature − 10` (Prusament PLA 215 → 205 = preset; Generic
PETG 230 → 220 = preset; Prusament PETG 240 → 230, 10 °C above the preset,
which costs ~5 s of heating, never a cool-down).

**How to test on the printer**

`doc/print_startup_local_test_plan.md` section 7. Key signals: after Preheat
→ PLA the Temperature screen / `M105` shows a 205 nozzle target (PETG 220);
after a Load finishes the target drops 215 → 205 instead of 170; with an
`HT_MBL10` profile the first `wait_hotend` phase on the F0 timeline is ~0
and the last one (`M109 S215`) is 10–12 s; with the stock profile the first
`wait_hotend` is ~1 min (passive 205 → 170).

**Expected timeline change**

Section 1.1 row "`M109 S215` reheat + 5 s hold": 20–30 s (from 170) →
10–12 s (from 205), *provided* the profile probes at 205 (`HT_MBL10`). Row
"`M109 R<probe>` nozzle wait": 0 before and 0 after with `HT_MBL10`. With
the **stock profile** (`M109 R170`) the same row regresses from 0 to ~1 min
of passive cool-down 205 → 170 after every Preheat or Load; the fix is the
slicer profile (`HT_MBL10`, or `M109 R… F` fan assist), not firmware. Bed,
homing and probing rows are unaffected. More ooze at 205 while idle is
possible; the start G-code's `G1 E-2` retract and `G29 P9` cleanup still
run before probing, and the tap-rejection count is worth watching.

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
