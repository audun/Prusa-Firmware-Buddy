# Print startup: local test plan for the round-2 firmware changes

On-printer checks for the commits on `claude/focused-hamilton-onqmge`
after `f1a0ccaa` (F0 timeline, F1 bed residency, F2 `G29 P11`, F3
`G29 G S`, F4 `G162 R` / Z align after print, F6 preheat presets, F5
pre-print preparation `58f0e6a5` with fixup `d6a0ae42`). What each item does is in
`doc/print_startup_cycle.md` section 8; this file is only the procedure.
Numbers marked *est.* are expectations, the point of the plan is to replace
them with measurements in the results table at the end.

Printer in all sections: CORE One+ (`coreone` preset). G-codes are sent from
the PrusaLink console or a serial terminal; "Settings" / "Control" are the
home-screen buttons.

## Prerequisites

- CORE One+ with the branch firmware flashed (release build of
  `claude/focused-hamilton-onqmge` including the F5 commit), selftest and
  calibrations complete so the preview never stops at the selftest prompt.
- PLA loaded (preset PLA, not a user filament: the F6 preheat value only
  applies to presets). A second spool of PETG is useful for F6 step 7.2 but
  not required.
- A human at the printer for every moving test, finger on the screen's
  STOP / Stop button (see Safety). Bed clear, steel sheet on.
- A terminal for G-code: PrusaLink web console (Printer → Console) or a
  serial terminal on the USB port (`screen /dev/ttyACM0 115200`). The USB
  serial port also carries the firmware log.
- A PC on the same network running the metrics collector and a syslog
  receiver:

  ```bash
  cd utils/metrics
  docker-compose build
  docker-compose up -d          # InfluxDB :8086, Grafana :3000 (admin/admin),
                                # metric-handler listening on UDP 8500
  docker-compose logs -f metric-handler   # shows parsed points as they arrive
  ```

  or without Docker (needs `pip install -r utils/metrics/requirements.txt`
  and a reachable InfluxDB 1.8):

  ```bash
  python utils/metrics/collect.py --host 0.0.0.0 --port 8514 --database buddy --database-host localhost
  ```

  The firmware log is a plain RFC 5424 UDP syslog stream on a second port;
  any receiver works, e.g. `nc -kul 13514 | tee printer.log` (or
  `socat -u UDP-RECV:13514 STDOUT`). `grep pstart printer.log` is the
  timeline.

- On the printer, Settings → Network → Metrics & Log:
  - **Host** = the PC's IP address,
  - **Metrics Port** = 8500 with the Docker setup, 8514 with the plain
    `collect.py` (the firmware default is 8514),
  - **Syslog Port** = 13514 (default) or whatever the receiver listens on,
  - **Enable Metrics** = on. (The same can be done with
    `M334 <ip> <metrics port> <syslog port>`, which asks for confirmation on
    the screen.)
  - Metrics List: all five metrics below are enabled by default; `M333`
    lists them, `M331 <name>` enables one.
- Metric names to watch in the collector: `pstart_phase`, `pstart_gcode`,
  `pstart_note` (F0; notes come from F2 as `mesh_verify:<result>` and from
  F5 as `prep:<step>`, both only inside a print's startup window, so an
  idle preparation produces none), `mesh_verify` (F2), `pre_print_prep`
  (F5). The log component names in the lines quoted below are
  `MarlinServer` (F0, F1, F3), `Marlin` (F2, `G28`) and `PrePrintPrep`
  (F5).
- A small sliced object (≤ 35 mm footprint, PLA, stock "Original Prusa CORE
  One" profile, 0.4 nozzle) as `stock.bgcode`, and copies with the start
  G-code edited: `p11.bgcode` (`G29 P1` → `G29 P11`), `soak.bgcode`
  (`G29 G` → `G29 G S170`), and `htmbl.bgcode` sliced with `HT_MBL10` in
  the PLA filament notes (probes at 205), and `prep.bgcode` (`G28` →
  `G28 Q`, `G29 P1` → `G29 P11`, `G29 P9 …` line removed, `HT_MBL10`
  profile) for F5. Binary G-code can be re-exported
  from the slicer after editing the printer profile; the files only need
  the start G-code to differ.

## Safety

- STOP conventions: during a print (including every start-G-code step) use
  the printing screen's Stop and confirm the dialog; the quick stop drains
  the planner within a fraction of a second. A `G28`/`G29` sent from the
  console shows the "Homing" / "Verifying mesh" / "Probing bed" wait screen
  **without any button**; the only ways to interrupt it are the power
  switch or, for `G29 G`, the Skip button of the "Absorbing heat" screen.
  So run every test that needs a controlled stop as a print (`p11.bgcode`,
  `soak.bgcode`), not from the console. `G162 Z` and the end-of-print
  alignment have no STOP either; they take a few seconds. Keep hands clear
  of the bed while it drives into the bottom hard stop.
- A probe tap that looks wrong (nozzle pressing visibly into the sheet, bed
  not stopping on contact, a tap aimed off the sheet): Stop from the
  printing screen if printing, otherwise power off. Afterwards `G28` from
  the Control menu (Auto Home) and watch that the Z home taps at (10, 10)
  stop on contact before sending any further `G29`.
- After `G162 Z R0` and after the F4 end-of-print alignment the bed is at
  the bottom and **Z is unhomed**: any Z move from the Move menu, a filament
  load park or a print start will home Z first. That is expected; do not
  send `G1 Z…` from the console without `G28 Z` before it.
- F6 raises the idle nozzle to 205 °C after Preheat; a Preheat left alone is
  switched off by the 30 min safety timer. Expect a little ooze on the taps
  in the first `G29 P9` of a print.
- Paper under the sheet corner (F2 mismatch test): ordinary 80 g paper
  (≈0.1 mm), one layer, well inside the sheet so the sheet still lies flat
  on the magnets; remove it before any print.

## 1. F0 Print startup timeline

Steps

1. Metrics enabled (Prerequisites). Home → Preheat → PLA; wait ~1 min.
2. Print `stock.bgcode` from the file browser, tap Print.
3. Let the start G-code run through to the purge line, then let the print
   run ~1 min and stop it (or let it finish, it is small).
4. From the printing screen tap Reprint; stop it during homing.
5. Settings → Network → Metrics & Log → Enable Metrics off. Print
   `stock.bgcode` again, stop after `G28`. Enable metrics again.
6. Optional: during the start window of yet another print send
   `M117 "quoted"` from the console.

Observe

- Log right after the Print tap: `pstart +0ms armed`, then one
  `pstart +<ms>ms gcode <i> <command>` per start-G-code line in file order
  (`M17`, `M862.1 P0.4`, …, `M109 R170`, `M84 E`, `G28`, …, `G29 P9 X208
  Y-2.5 W32 H4`, `G29 P1`, `G29 P1 X150 Y0 W100 H20 C`, …, `M109 S215`).
- Phase pairs around them: `phase <p> wait_hotend` → `phase 0 none`
  (short) at `M109 R170`; `homing` → `sheet_detect` → `none` at `G28`; `wait_bed` →
  `none` at `M190 R60`; `absorbing_heat` only if the soak is non-zero;
  `nozzle_cleaning` → `none` at `G29 P9`; `probing_bed` →
  `additional_probing` → `none` at the two `G29 P1`; `wait_hotend` → `none`
  at `M109 S215`. (`p` is the COREONE enum value and shifted by one for
  everything after `additional_probing` when F2 added `verifying_mesh`;
  group by `n`, not by `p`.)
- Collector: `pstart_gcode` points with `t`, `i`, `c` (command cut to 20
  chars, `"` replaced by `'`), `pstart_phase` with `t`, `p`, `n`.
- Step 3: the last `gcode` index before the purge line is < 160 and no
  `pstart` line appears later in the print. Step 4: a new `pstart +0ms
  armed`. Step 5: no `pstart` lines at all. Step 6: `c="M117 'quoted'"`.

Pass

- Every start-G-code line present, in order, with monotonically increasing
  `t`; the phases above present; nothing after command 160; nothing with
  metrics off; no crash on abort during homing and the next print arms.

Record (this is the "Today" baseline for every other section)

- `t` at: `gcode … G28` → `phase … none` after `sheet_detect` (= `G28`
  row); `wait_bed` enter → leave (`M190` row); `absorbing_heat` enter →
  leave (soak row, 0 if absent); second `wait_hotend` enter → leave
  (reheat-from-idle row); `nozzle_cleaning` enter → leave (`G29 P9` row);
  `probing_bed` enter → `additional_probing` leave (mesh row); last
  `wait_hotend` enter → leave (`M109 S215` row); `t` of the first `G1 E2
  F2400` purge line (total). Write them into the results table as the
  "before" of the matching sections.

## 2. F1 Continuous bed residency

All times from the F0 log (`phase … wait_bed`) or a stopwatch. Bed starts
cold (target 0), nozzle off, nothing printing.

Steps and expectations

1. `M190 R60` — waits for heating, then ~5 s residency, returns. No
   `Bed residency already satisfied` line in the log.
2. Immediately `M190 R60` again — returns within one thermal tick (< 0.3 s);
   log `Bed residency already satisfied, not waiting`; no "Waiting for bed"
   flash on the screen.
3. `M140 S60` then `M190 R60` — immediate return (same target does not reset
   the residency).
4. `M190 R70` — waits for 70 plus ~5 s.
5. `M190 S50` (bed at 70) — returns at once (`S` never waits for cooling);
   target is now 50.
6. `M190 R50` — waits for the bed to cool within 1 °C of 50, then ~5 s. The
   cooling-slope abort must not fire (a bare bed loses more than 1.5 °C per
   minute between 70 and 50).
7. `M140 S0` then `M190 R0` — returns at once (target 0 counts as reached;
   before this change `M190 R0` sat in the cooling-slope guard).
8. Pause a print with the bed at target, wait 30 s, Resume — resumes
   normally; if the bed drifted > 1 °C during the pause the resume waits
   for it to come back plus 5 s (new, intended).
9. Full start G-code: Preheat → PLA, wait 1 min, print `stock.bgcode`. On
   the F0 timeline `wait_bed` is absent or < 0.3 s.

Pass

- Steps 2, 3, 7 return in < 0.5 s with the log line; steps 1, 4, 6 still
  wait for the temperature plus residency; step 6 does not abort early.

Record

- Step 1 duration vs step 2 duration (before/after of the "`M190`" row,
  est. ~5 s → ~0 s); step 9 `wait_bed` duration.

## 3. F2 Mesh verification `G29 P11`

Watch the `Marlin` log lines and the `mesh_verify` metric. Bed target 60
(`M140 S60`) throughout unless stated; nozzle at 170 (`M104 S170`, wait)
so the taps are as during a print. Every `G29 P11` that falls back to a
full probe shows "Probing bed" after "Verifying mesh".

Steps and expectations

1. Baseline probe: `M555 X80 Y80 W60 H60`, `G28`, `G29 P1`, `G29 P3.2`,
   `G29 P3.13`, `G29 A`, `G29 T` (keep the printed map). Full probe of
   9 major points as today.
2. Verify, same area: `G29 P11 M` (`M` forces taps although the mesh is
   fresh). 4 taps at the corners of the probed major rectangle, in the
   order far-left, far-right, near-right, near-left; screen "Verifying mesh
   1/4 … 4/4"; log one `Mesh verify tap (x, y): … diff 0.0xx` per tap and
   `Mesh verify: ok, 4 taps, max diff 0.0xx mm, tol 0.050 mm`; metric
   `mesh_verify r="ok",n=4,d=0.0xx`. Time it from the `G29 P11` echo to
   the end of the last tap: est. ~10 s including the 5 mm first descent.
3. Fresh skip: `G29 P11` immediately again — no movement, log `Mesh verify:
   fresh, 0 taps, max diff 0.000 mm, tol 0.050 mm`, metric `r="fresh"`.
4. End of print clears fresh: print `stock.bgcode` (its `G29 P1` is fine),
   let it finish; then `M555 X80 Y80 W60 H60`, `G28`, `G29 P11` (no `M`) —
   taps (not fresh), result `ok`.
5. Mismatch fallback: slide one layer of paper under the sheet at one of
   the four corner points tapped in step 2, `G29 P11 M` — log `Mesh verify:
   mismatch, …` (diff ≈ 0.1), serial `Mesh verification failed. Probing
   mesh.`, then the full "Probing bed" of the 9 points follows by itself;
   `G29 T` shows the bump.
6. Tolerance: paper still there, `G29 P11 M O0.3` — `ok` (0.1 < 0.3).
   Remove the paper. Then `G29 P1` to get a clean mesh again (`G29 P3.2`,
   `G29 P3.13`, `G29 A`).
7. Points and home point: `G29 P11 M N5 Z` — 5 taps (4 corners + centre)
   plus one at the Z home point (10, 10) (240, 10 on the mirrored variant);
   6 tap lines, the home tap `expected 0.000`, diff ≈ 0; screen counts to
   6/6.
8. Area coverage: `M555 X20 Y20 W200 H180`, `G29 P11` — log `Mesh verify:
   invalid, 0 taps …` then the full probe of the large area (25+ points).
   Then `M555 X80 Y80 W60 H60`, `G29 P11 M` — `ok` (small inside large).
9. Bed target delta: bed was 60 for step 8; `M140 S75`, `G29 P11 M` —
   `invalid` + full probe; `M140 S65`, `G29 P11 M` — taps, `ok` (within
   10 °C of 75). Back to `M140 S60` (then the next `G29 P11` is `invalid`
   again, 15 °C from 75: expected).
10. Live-Z / offset: `M851 Z-0.05`, `G29 P11 M` — `invalid` + full probe.
    `M851 Z0`. (Print init resets `probe_offset.z` to 0 on loadcell
    printers, so a Live-Z tweak during a print does not invalidate the
    next print's verify; a `M851` left non-zero does.)
11. Z alignment: `G162 Z` (bed returns to Z = 50), log `Mesh invalidated: Z
    align`; `G28`, `G29 P11 M` — `invalid` + full probe.
12. Crash recovery: start `stock.bgcode`, during the first layer nudge the
    gantry enough to trigger crash detection, let it recover, stop the
    print; `G28`, `M555 X80 Y80 W60 H60`, `G29 P11 M` — `invalid` (log
    `Mesh invalidated: crash recovery` at the crash).
13. Power cycle: reboot, `M140 S60`, `G28`, `M555 X80 Y80 W60 H60`,
    `G29 P11` — `invalid` + full probe.
14. STOP: the "Verifying mesh" wait screen has no button, so do this as a
    print: start `p11.bgcode` (mesh valid from a previous `stock.bgcode`
    print of the same file, F4 toggle off) and press Stop on the printing
    screen while the check taps run — the taps end, no probing-failed
    prompt, the printer parks and is usable (`G28` works); log shows no
    `Mesh verify:` result line for that run (the stopped verify reports
    nothing).
15. Stock flow unchanged, then the real thing: print `stock.bgcode` after a
    Preheat and compare the F0 timeline with section 1: no difference.
    Then (F4 toggle off) print `p11.bgcode` twice in a row after a
    Preheat: the first run
    verifies the mesh left by the previous `stock.bgcode` print if the
    `M555` area is covered (same object: yes) — `ok`, ~10 s; the second run
    the same. In both, `G29 P1 X150 Y0 W100 H20 C` must move and probe
    nothing (no `additional_probing` taps; the phase may still flash).

Pass

- Steps 2–3, 5–11, 13 give exactly the result named; step 14 stops
  cleanly; step 15 first-print timeline equals section 1 and the `p11`
  prints show `verifying_mesh` of ~10 s instead of `probing_bed` of 25–35 s.

Record

- Step 2 duration (verify, 4 taps) vs section 1 mesh row (before, 9 + 3
  taps); step 15 `verifying_mesh` enter → `none` (after) vs `probing_bed`
  enter → `additional_probing` leave (before); the max diff of every `ok`
  (mesh repeatability between prints, sizes the 0.05 mm tolerance).

## 4. F3 `G29 G S<temp>` early hotend target

Nozzle target is read from the Temperature screen (Control → Temperature)
or `M105`. The soak on a CORE One+ (non-EJG2) runs for up to ~150 s only
when the frame estimate is cold, i.e. within the first minutes after the
bed heats up from room temperature; after that `G29 G` is skipped.

Steps and expectations

1. No `S`, no change: `M104 S100`, `M140 S60`, `M190 S60`, `G29 G` — the
   soak runs (or is skipped) exactly as before, nozzle target stays 100.
2. Cold bed, soak runs: power-cycle (cold frame estimate), bed cold.
   `M104 S100`, `M140 S60`, `M190 S60`, `G29 G S205`. "Absorbing heat" with
   progress on screen; when roughly 20 s remain the nozzle target jumps to
   205, log `Absorbing heat: setting hotend target 205 early (soak nearly
   done, predicted <n> s left)` with n ≤ 20; `G29 G` finishes ~20 s later;
   then `M109 R205` returns within a few seconds. Note the time between
   `absorbing_heat` leave and `wait_hotend` leave on the F0 log (the
   timeline is armed only in a print, so use `soak.bgcode` for the timed
   run, see 8; here a stopwatch is fine).
3. Warm printer, soak skipped: bed at 60 for > 5 min, `M104 S100`,
   `G29 G S205` — returns at once, target 205, log `(already stable,
   predicted 0 s left)`.
4. Below threshold: needs a cold frame estimate, so right after a power
   cycle with the bed cold: `M104 S100`, `M140 S25`, `G29 G S205` —
   immediate return, target 205, log `(below soak threshold, predicted 0 s
   left)`. (With a warm frame the same command reports `cooling`, which is
   checked first.)
5. Cooling: bed at 60 with a settled estimate, `M140 S40`, `G29 G S205` —
   immediate return, target 205, log `(cooling, predicted 0 s left)`.
6. User skip: as step 2 but press Skip on the "Absorbing heat" screen early
   — target 205 at the moment of the skip, log `(skipped by user, …)`,
   `G29` returns.
7. Abort: as step 2 inside a print (`soak.bgcode`, see 8), press Stop on
   the printing screen during the soak — no `setting hotend target` line
   from G29, no error (the abort sets its own targets).
8. Start-G-code integration: `soak.bgcode` (`G29 G S170`). Heat the bed
   from cold with `M140 S60` and tap Print as soon as `M190` would pass
   (bed just reached 60, frame estimate still low): the soak is non-zero,
   the target is raised ~20 s before it ends, and on the F0 timeline the
   `wait_hotend` phase after `absorbing_heat` is ≤ 2 s instead of 5–15 s.
   Then the same file after a 5 min Preheat: the soak is skipped, the
   target is set at once, and the following `wait_hotend` still shows the
   full 100 → 170 reheat (expected: F3 only helps when there is a soak to
   overlap; the stock order has `M109 R` right after `G29 G`).
9. Bad value: `G29 G S999` — serial `?(S)oak hotend target implausible.`,
   nothing moves, target unchanged.

Pass

- Step 1 unchanged; steps 2–6 set the target with the named reason; step 7
  does not; step 8 first run shows the shortened `wait_hotend`; step 9
  rejected.

Record

- Step 8, run 1: `absorbing_heat` enter → leave, and the following
  `wait_hotend` enter → leave (after); compare with the same file minus
  `S` (`stock.bgcode` under the same cold-frame condition) for the before.
  Step 2: seconds between the target jump and the end of the soak (should
  be ≈ 20).

## 5. F4 `G162 R` and Z alignment after print

Steps and expectations

1. `G28`, `G1 Z20 F600`, then `G162 Z R0` — "Calibrating Z" selftest dialog;
   the bed drives down to the hard stop, pushes 5 mm, backs off 1 mm and
   **stays there**. Afterwards the Info screen / `M114` shows Z unhomed; a
   `G28 Z` is needed before any Z move. Log `Mesh invalidated: Z align` if a
   valid mesh existed.
2. `G28`, `G1 Z20 F600`, `G162 Z` and then again with `G162 Z R1` —
   unchanged: after the push the bed returns to Z = 50.
3. Settings → "Z Align After Print" → on (top-level Settings list, after
   "Stuck Filament Detection"). Print `stock.bgcode` to the end. After the
   end G-code and the park (head at the print-end position, bed ≥ Z_MAX −
   50 i.e. ≥ 225): the "Calibrating Z" dialog appears over the printing
   screen, the bed moves down the remaining ≤ 50 mm, pushes, backs off 1 mm;
   then the "Printed" screen. Stopwatch from the park finishing to
   "Printed": est. 5–8 s. Z unhomed afterwards; the next print's `G28` (or
   Control → Auto Home) homes it normally.
4. Toggle off, same print: "Printed" immediately after the park.
5. Toggle on, Stop a print from the screen mid-way: the Aborting path runs,
   **no** alignment, bed parks as today.
6. Toggle on, tap Print on the preview and Cancel/Back after the job has
   started (checks done, e.g. on the filament-mismatch dialog choose
   Abort): the preview abort passes through the finishing states and
   aligns too (expected per the commit; note whether the bed was already
   low).
7. Toggle survives a power cycle.
8. Timeline: with the toggle on the alignment is in the finishing phase of
   the print, not in the next print's `pstart` window; the next
   `p11.bgcode` print logs `Mesh verify: invalid` and probes fully (F2
   interaction, expected).

Pass

- Step 1 stays at the bottom and leaves Z unhomed; step 2 returns to 50;
  steps 3/4 differ only by the alignment; step 5 does not align; step 7
  persists; step 8 shows the full re-probe with the toggle on.

Record

- Step 3 park-end → "Printed" with the toggle on (after) vs step 4 (before,
  ≈ 0); step 1 whole `G162 Z R0` from Z = 20 for reference; step 8 mesh
  phase duration of the following `p11` print (toggle on) vs section 3
  step 15 (toggle off).

## 6. F5 Pre-print preparation (`M1706`, prepared state)

Names below are from commit `58f0e6a5` plus fixup `d6a0ae42` (Stop lifts
Z 5 mm, Ignore in the nozzle-cleaning wizard continues, the "Trust Prep in
Start G-code" setting also skips `G29 P9`). The dialog: header
"PREPARE FOR PRINT"; question frame titled "Prepare for Print" with the
text "Is the bed clear and the steel sheet in place? The printer will
home, detect the sheet, clean the nozzle and check the bed mesh so that
the next print can start right away." and buttons **Yes / No**; running
frame with the live status text (Homing, Detecting steel sheet, Nozzle
cleaning, Verifying mesh n/N or Probing bed n/N) or, between G-codes, the
step text Homing / Nozzle cleaning / Checking bed mesh, and one button
**Stop**; "Preparation stopped. The position is no longer known; the next
print will home and probe as usual." with **Continue**; "Preparation did
not finish. The next print will home and probe as usual." with
**Continue**. A successful preparation closes the dialog without any
message. Warnings from the sub-steps (steel sheet not detected, homing
refinement failed, nozzle-cleaning-failed wizard, probing failed) overlay
the running frame with their own buttons, as during a print.

Preconditions: F0–F4 and F6 flashed together with F5; PLA loaded; metrics
on; Settings → Hardware → "Prepare After Preheat" ON and "Trust Prep in
Start G-code" OFF (the defaults); F4 "Z Align After Print" OFF unless a
step says
otherwise. Watch the `PrePrintPrep` log lines (`begin +0ms`, `homed +…`,
`cleaned +…`, `mesh_ok +…`, `done +…`, `prepared (nozzle target N)`,
`prepared state ended (<reason>)`) and the `pre_print_prep` metric
(`s="begin|homed|cleaned|mesh_ok|done|stopped|failed",t=<ms>i` and
`s="cleared",r="<reason>"`).

Steps and expectations

1. Answer No: Home → Preheat → PLA. After the preheat dialog closes the
   "Prepare for Print" question appears. **No** — nothing moves, home
   screen; no `PrePrintPrep` line.
2. Full run from a cold mesh (first after power-on): Preheat → PLA →
   **Yes**. Status texts in order: Homing (XY, then Z at the front-left),
   Detecting steel sheet (tap at (245, 0)), Nozzle cleaning (taps in the
   32 × 4 mm strip at X 208–240, Y −2.5), then Probing bed n/49 over the
   **whole bed** (mesh invalid after boot; est. ~100 s), no "Verifying
   mesh". The dialog closes by itself. Log `begin +0ms` … `homed`,
   `cleaned`, `mesh_ok`, `done +<ms>ms`, `prepared (nozzle target 205)`;
   metric points `s="begin"` … `s="done"`. Record the `done` time.
3. Prepared state holds: wait > 2 min 30 s idle. The XY motors stay
   energised (gently push the gantry in X: it resists; before this change
   the motors released after 2 min). No `prepared state ended` line.
4. Second run verifies instead of probing: Control → "Prepare for Print" →
   Yes. Log `prepared state ended (manual)` first (the old state is void
   once the printer moves), then the same steps, but the mesh phase is
   "Verifying mesh 1/4 … 4/4" (~10 s), log `Mesh verify: ok …`, then
   `mesh_ok`, `done`. Record the `done` time (est. 40–50 s).
5. Stop during homing: start prep again, press **Stop** while XY are
   homing. Motion stops within a fraction of a second, the bed/head lift
   5 mm in Z, the "Preparation stopped …" frame with Continue appears, log
   `stopped +<ms>ms`, metric `s="stopped"`, no `prepared` line. Press
   Continue. Control → Auto Home works normally afterwards.
6. Stop during the taps: same, press Stop mid-tap during Nozzle cleaning or
   Verifying mesh. The nozzle lifts 5 mm off the sheet, same frame. Z is
   unhomed afterwards (the next Z move homes first).
7. Cooldown ends it: run prep to completion after a Preheat (target 205),
   then Control → Temperature → nozzle 0 (or Preheat → Cooldown). Within a
   second: log `prepared state ended (nozzle_cooled)`, metric
   `s="cleared",r="nozzle_cooled"`; the motors release ~2 min later.
   Variant: Control → Prepare for Print with the nozzle cold (target 0):
   `prepared (nozzle target 0)`; a later cooldown changes nothing (0 is not
   below 0), so the state and the energised motors last until trust loss,
   a print end or the 30 min expiry — note it as expected.
8. Trust loss ends it: prepared, then Control → Disable Motors → log
   `prepared state ended (trust_lost)`. Prepared again, then `G162 Z` (or
   Calibrations → Z alignment) → `(trust_lost)` plus F2's `Mesh
   invalidated: Z align`; the next prep probes the whole bed again
   (~100 s).
9. Safety-timer bound: prepared after a Preheat, leave the printer idle
   30 min: `(expired)` at 30 min, or `(nozzle_cooled)` when the 30 min
   heater safety timer fires first (same moment, either reason passes).
   Motors release 2 min later.
10. Sheet missing: remove the sheet, prep → Yes. After three detection
    tries the "steel sheet not detected" warning appears with the bed
    lowered; put the sheet on, **Retry** → prep continues to `done`.
    Repeat and press **Abort** → "Preparation did not finish …", Continue;
    log `failed +<ms>ms`, no `prepared` line. (Ignore would continue
    without a sheet; do not use it here.)
11. Live-Z + print start (the real thing): during a `stock.bgcode` print
    adjust Live-Z by −0.05; after it finishes (`prepared state ended
    (print_finished)` if a state existed) Preheat → PLA → Yes. Then print
    `prep.bgcode` (`G28 Q`, `G29 P11`, no `G29 P9`, `HT_MBL10`). F0
    timeline: no `homing` / `sheet_detect` phase, no `nozzle_cleaning`,
    `Mesh verify: fresh, 0 taps …` with `pstart … note mesh_verify:fresh`,
    `additional_probing` probes nothing, first `wait_hotend` ≈ 0, first
    purge line within est. 20–30 s of the Print tap. The Live-Z of the
    previous print was reset at prep time (verify says `fresh`, not
    `invalid`).
12. Bare `G28`, `G29 P9` and the trust option: prepared (after a step
    4-style prep that ran the cleanup); print `stock.bgcode` (bare `G28`,
    `G29 P9`, `G29 P1`) with "Trust Prep in Start G-code" **OFF**: full
    homing runs and, because homing drops the home level first, log
    `prepared state ended (trust_lost)` during `G28`; the cleanup taps run
    as usual; a `G29 P11` in that position would tap 4 points (not
    `fresh`). Then Settings → Hardware → "Trust Prep in Start G-code"
    **ON**, prepare again, print `p11.bgcode` (bare `G28`, `G29 P9`,
    `G29 P11`): log `G28: printer prepared, homing only if needed` and
    `G29 P9: printer prepared, nozzle cleanup skipped`, no homing moves and
    no cleanup taps on the F0 timeline (no `homing`, `sheet_detect` or
    `nozzle_cleaning` phase), `Mesh verify: fresh`. Variant: option still
    ON, `M1706 S0 C0` from the console (prep without cleanup), then the
    same print: `G28` is skipped but `G29 P9` taps as usual (no cleanup
    bit, no `nozzle cleanup skipped` line). Turn the option OFF again
    afterwards.
13. End of print clears: after any print finishes, log `prepared state
    ended (print_finished)`; the next Preheat offers prep again; a `G29
    P11` sent without a prep taps 4 points (section 3 step 4).
14. Bumped gantry (documented risk): prepared, nozzle high, bed low,
    shove the gantry hard enough to skip steps (the motors are on, it takes
    real force). Nothing detects it unless crash detection fires (then
    `(trust_lost)`). This is the risk behind the trust option's default
    OFF; record what happened.
15. Connect: with the printer linked, run prep from Control; Connect shows
    Busy during the prep and Idle afterwards. The question cannot be
    answered from Connect (expected in this commit).
16. Nozzle-cleaning-failed path: leave a filament blob on the cleanup
    strip; prep → the nozzle-cleaning-failed wizard appears over the prep
    screen. **Retry** re-runs the taps; **Ignore** continues to the mesh
    step and ends in `done` (Ignore clears the failure); **Abort** →
    "Preparation did not finish …", Continue, log `failed`.
17. Parameters: `M1706 S0` from the console (no question, starts at once);
    `M1706 S0 C0` (no cleanup; no `cleaned` line); `M1706 S0 M Z` (taps
    even when fresh, 4 corners + Z home point, "Verifying mesh n/5").
    Control → Prepare for Print while a print runs: the printer-busy box,
    nothing enqueued.

Pass

- Steps 1, 5, 6, 10, 16 end in the named frame with the named buttons;
  steps 2 and 4 end with `done` and `prepared`, step 4 with 4 taps; step 3
  keeps the motors on; steps 7, 8, 9, 13 log the named reason; step 11
  starts the purge line without homing, cleaning or probing; step 12
  behaves as described in both halves.

Record

- Step 2 `done` time (whole-bed prep) and step 4 `done` time (verify prep)
  — work moved to idle time; step 11 Print tap → first purge line (after,
  est. 20–30 s) vs section 1 total (before, 2–2.5 min); per row: `G28`
  30–40 s → ~0, `G29 P9` 5–10 s → 0 (line dropped), mesh 25–35 s → 0,
  first `wait_hotend` → 0; step 12 second half: the same total with the
  stock lines and the trust option on (`G28` and `G29 P9` skipped by
  firmware), and the first half's time of `trust_lost` within `G28`.

## 7. F6 Preheat presets at print temperature − 10

Steps and expectations

1. Preheat button: Home → Preheat → PLA — nozzle target **205**, bed 60
   (Control → Temperature, or `M105` → `T:… /205`). Cooldown, then Preheat
   → PETG — 220 / 85.
2. Load: Filament → Load → PLA. Loading heats to 215; after the dialog's
   Done the target drops to **205** (was 170).
3. Unload: Filament → Unload — heats to 215 as before; no change.
4. Autoload: insert filament with the sensor on; after autoload the target
   stays at the load temperature as before (no preheat read on this path).
5. Cooldown: Preheat → Cooldown — targets 0; unchanged.
6. Print after Preheat, `HT_MBL10` profile (`htmbl.bgcode`): tap Print right
   after step 1. Its `M109 R205` returns immediately (no "Waiting for
   hotend" countdown, first `wait_hotend` on the F0 timeline ≈ 0); the
   final `M109 S215` heats only 10 °C (last `wait_hotend` est. 10–12 s
   including the 5 s residency). Count the `G29 P9` taps (log
   `nozzle_cleaning` duration, probe-analysis rejections) against section
   1: ooze at 205 may add retries.
7. Print after Preheat, **stock** profile (`stock.bgcode`): expected
   regression, verify once — `M109 R170` waits for a passive cool-down 205
   → 170 (first `wait_hotend` est. ~1 min). Confirms the commit-body
   statement; the fix is the slicer profile (`HT_MBL10`), not firmware.
8. Filament detail: Filament → PLA → detail shows Preheat 205 (read-only).
   A user filament still shows its stored value.
9. Before/after of the reheat without reflashing: `M104 S170`, wait for
   170, then time `M109 S215` (before, est. 20–30 s); `M104 S205`, wait,
   time `M109 S215` (after, est. 10–12 s).

Pass

- Steps 1, 2, 8 show 205/220; steps 3–5 unchanged; step 6 first
  `wait_hotend` ≈ 0 and last 10–12 s; step 7 shows the ~1 min cool-down
  (documented regression, not a failure).

Record

- Step 9 both timings (`M109 S215` row before/after); step 6 first and last
  `wait_hotend` durations; step 7 first `wait_hotend` duration (stock
  profile regression); `G29 P9` tap count at 205 vs 170 (section 1).

## Results table

One row per measured step. "Expected" is the *est.* value from the section;
"Measured" comes from the F0 log (`t` differences) or a stopwatch; note
which. Add the firmware build id (Settings → System → Version info) and the
date at the top of the filled table.

| Item | Test step | Signal measured | Expected (est.) | Measured before | Measured after | Pass/fail | Notes |
|------|-----------|-----------------|-----------------|-----------------|----------------|-----------|-------|
| F0 | 1.3 | `pstart` lines present, in order, < 160, none with metrics off | all | – | – | | |
| F0 | 1 record | `G28` row: `homing` enter → `none` after `sheet_detect` | 30–40 s | | – | | baseline |
| F0 | 1 record | `M190` row: `wait_bed` enter → leave | ~5 s | | – | | baseline |
| F0 | 1 record | `G29 P9` row: `nozzle_cleaning` enter → leave | 5–10 s | | – | | baseline |
| F0 | 1 record | mesh row: `probing_bed` enter → `additional_probing` leave | 25–35 s | | – | | baseline |
| F0 | 1 record | `M109 S215` row: last `wait_hotend` enter → leave | 20–30 s | | – | | baseline |
| F0 | 1 record | total: Print tap → first `G1 E2 F2400` | 2–2.5 min | | – | | baseline |
| F1 | 2.1 / 2.2 | first `M190 R60` vs repeated `M190 R60` | ~5 s → < 0.3 s | | | | log line seen? |
| F1 | 2.9 | `wait_bed` in a preheated print | < 0.3 s | (F0 baseline) | | | |
| F2 | 3.2 | `G29 P11 M` 4 taps, `ok` | ~10 s | (F0 mesh row) | | | max diff |
| F2 | 3.3 | `G29 P11` fresh | 0 s, no motion | – | | | |
| F2 | 3.5 | paper → `mismatch` + full probe | fallback runs | – | | | |
| F2 | 3.8–3.13 | each invalidation reason | `invalid` | – | | | list reasons seen |
| F2 | 3.15 | `p11.bgcode` `verifying_mesh` enter → `none` | ~10 s | (F0 mesh row) | | | `P1 … C` probed nothing? |
| F3 | 4.2 | target jump → end of soak | ≈ 20 s | – | | | predicted n |
| F3 | 4.8 run 1 | `wait_hotend` after `absorbing_heat` | ≤ 2 s | 5–15 s (no `S`) | | | soak was non-zero? |
| F3 | 4.8 run 2 | same, soak skipped | unchanged reheat | | | | expected no gain |
| F4 | 5.1 | `G162 Z R0` from Z = 20, bed stays low, Z unhomed | yes | – | | | |
| F4 | 5.3 / 5.4 | park end → "Printed", toggle on / off | 5–8 s / ~0 | | | | |
| F4 | 5.8 | next `p11` print mesh phase with toggle on | full probe 25–35 s | (3.15) | | | F2 interaction |
| F5 | 6.2 | first prep after boot, `begin` → `done`, whole bed | ~100 s | – | | | idle time |
| F5 | 6.4 | second prep, `begin` → `done`, 4 verify taps | 40–50 s | – | | | idle time |
| F5 | 6.11 | Print tap → first purge line, prep run, `prep.bgcode` | 20–30 s | (F0 total) | | | no homing / cleaning / probing phases? |
| F5 | 6.12 | stock lines, trust OFF → `trust_lost`; trust ON → no homing, no cleanup taps; after `M1706 C0` P9 taps | as described | – | | | Print tap → purge with trust ON |
| F6 | 7.1 / 7.2 | target after Preheat / after Load | 205 / 205 | 170 | | | PETG 220 |
| F6 | 7.9 | `M109 S215` from 170 vs from 205 | 20–30 s → 10–12 s | | | | |
| F6 | 7.6 | `htmbl.bgcode` first / last `wait_hotend` | ≈ 0 / 10–12 s | (F0 baseline) | | | `G29 P9` taps |
| F6 | 7.7 | `stock.bgcode` first `wait_hotend` after Preheat | ~1 min | 0 (F0 baseline) | | | documented regression |
