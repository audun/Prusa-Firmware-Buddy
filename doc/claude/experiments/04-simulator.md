# Stage 4: simulator and integration tests

All experiments at `b1043d68b` on the machine of stage 0 (Ubuntu 24.04, no
display, no GPU), with the firmware built in stage 3. No tracked file was
changed; `sim_probe.py` next to this note is the helper used for all manual
runs.

**Verdict**

- The simulator that bootstrap downloads (mini404 0.9.10, a QEMU 7.0.0
  fork) **runs headless on Linux** after installing three small OS packages
  and adding one library symlink.
- The **MINI firmware boots and is usable** in it: screenshots, encoder
  input, sensor readback, HTTP to PrusaLink and a USB stick directory all
  work. First UI screen about 15 s after start on an idle machine (40 to
  75 s while firmware builds were running in parallel).
- The **MK4 firmware does not run** in it. Three independent problems were
  found, the last one cannot be worked around from outside: firmware 8.0.2
  rejects the board revisions the simulator emulates ("BOM ID not
  compatible").
- Therefore the **integration tests (MK4 only) cannot pass here**: 22
  errors, 6 skipped, 0 passed in 6.6 minutes.

## How the firmware runs without hardware

| Piece | What it is |
|---|---|
| `.dependencies/mini404-0.9.10/qemu-system-buddy` | the simulator binary (`--version`: "QEMU emulator version 7.0.0"), with `assets/` (3D models for its optional GL view) and `pc-bios/` |
| `utils/simulator/` | Python package: `python utils/simulator [BUILD_DIR]` launcher (`__main__.py`), `Simulator` class used by the tests (`simulator.py`), machine names (`printer.py`) |
| `tests/integration/` | pytest suite that starts the simulator per test, reads the screen with OCR (`easyocr`) and talks to PrusaLink over HTTP; README says only "MK4, noboot variant" is supported |
| `integration-test-requirements.txt` | `aiohttp~=3.8 click~=8.1.3 easyocr~=1.7 pytest-asyncio~=0.21`, installed by `python3 utils/bootstrap.py --include-integration` |

Machines the binary offers (`qemu-system-buddy -machine help`):
`prusa-mini`, `prusa-mini-014`, `prusa-mk4-034`, `prusa-mk4-027c`,
`prusa-mk3-39`, `prusa-mk3-35`, `prusa-xl-040`, `prusa-xl-050`,
`prusa-xl-bed-060`, `prusa-xl-bed-070`, `prusa-xl-extruder-040-0..4`,
`prusa-xl-extruder-060-0..4`, `prusabuddy` (deprecated). There is no CORE
One, CORE One L or iX machine.

What `utils/simulator` covers: `--printer MINI|MK4|XL`; XL raises
`NotImplementedError('XL simulator isn't implemented yet')`; the tests use
`prusa-mk4-027c` only.

## Getting the binary to start on Ubuntu 24.04

```
$ .dependencies/mini404-0.9.10/qemu-system-buddy --version
error while loading shared libraries: libglut.so.3: cannot open shared object file
```

`ldd` reports three missing libraries: `libGLU.so.1`, `libglut.so.3`,
`libGLEW.so.2.2`.

```bash
apt-get install -y libglu1-mesa libglew2.2 libglut3.12
```

provides the first and the third. Ubuntu 24.04 ships freeglut only as
`libglut.so.3.12`, and no package provides the old name `libglut.so.3`
(the CI image for the tests is Ubuntu 22.04, where `freeglut3` has it).
Workaround that was used for everything below, without touching system
directories:

```bash
mkdir -p build/simlibs
ln -sf /usr/lib/x86_64-linux-gnu/libglut.so.3.12 build/simlibs/libglut.so.3
export LD_LIBRARY_PATH=$PWD/build/simlibs
```

Headless operation: with `-nographic` the binary needs no X server
(`DISPLAY` was unset in all runs). Without it the launcher ends with
`gtk initialization failed`. `xvfb-run` is installed on this machine but
was not needed and not tried.

## The launcher: `python utils/simulator`

| Command | Result |
|---|---|
| `python utils/simulator --help` in a venv with only `requirements.txt` | `ModuleNotFoundError: No module named 'click'` (click comes from the integration requirements) |
| `python utils/simulator build/mini_release_boot` | prepares `build/mini_release_boot/simulator-mini/` (`bootloader.bin`, `firmware.bbf`, `usbdir/`), starts QEMU, which exits with `gtk initialization failed` on a machine without a display |
| `python utils/simulator build/mini_release_boot --buddy-extra-opt=-nographic` | runs (still running when stopped after 45 s); creates `Prusa_Mini_eeprom.bin`, `Prusa_Mini_eeprom_sys.bin`, `Prusa_Mini_xflash.bin` in the simulator directory |
| same with the short form `-X=-nographic` | QEMU receives the literal argument `=-nographic` and fails |

The launcher does not open the script console, so a headless run started
this way cannot be observed; that is what `sim_probe.py` adds.

## What can be observed and driven (verified on the MINI)

Firmware: `build/mini_release_boot/firmware.bbf` (stage 3) with
`.dependencies/bootloader-mini-2.6.0/bootloader.bin`. A `.bbf` kernel needs
`bootloader.bin` in the working directory, otherwise:
`No bootloader.bin file found. It is required to use a .bbf file!`

| Capability | How | Result |
|---|---|---|
| Screen capture | script console command `generic-spi-display::Screenshot(<file>.png)` | works: 240×320 PNG (480×320 on MK4). Seen in order: bootloader 2.6.0 screen, "Installing" (first boot only), "SELECT LANGUAGE", setup wizard greeting, "PRINTER SETUP" menu, "PRINTHEAD" menu |
| UI input | `encoder-input::Twist(1)`, `encoder-input::Twist(-1)`, `encoder-input::Push()` | works: selected the language, confirmed the wizard, entered menus and started editing "Nozzle Diameter" |
| Sensor and actuator readback | `thermistor::GetTemp()`, `thermistor2::GetTemp()`, `fan::GetRPM()`, `X::GetPosFloat()` | replies `18.000000`, `25.000000`, `6600`, `0.000000` |
| Fault injection | `thermistor*::Set(float)/Short()/Disconnect()`, `heater*::Runaway()/Open()`, `fan*::Stall()`, `ir-sensor::Set(bool)`, `encoder-input::Reset()`, `KeyCtl::Key(string)` | listed by the binary, **not run** |
| Network | `-netdev user,id=mini-eth,hostfwd=tcp::18080-:80` | `GET http://localhost:18080/` → 200, 7211 bytes (PrusaLink page); the ethernet icon appears in the header |
| PrusaLink API | `GET /api/version` | 401 with `WWW-Authenticate: Digest realm="Printer API"`; no working credentials were obtained (see below) |
| USB flash drive | `-drive id=usbstick,format=raw,file=fat:rw:<dir> -device usb-storage,drive=usbstick` | works: the firmware installs its resources from `firmware.bbf` in that directory and shows the USB icon. Printing a G-code file from it was **not run** |
| Persistent storage | files in the working directory (`Prusa_Mini_xflash.bin`, `Prusa_Mini_eeprom.bin`, `Prusa_Mini_eeprom_sys.bin`), or explicit `-drive if=mtd,...` and two `-drive if=pflash,...` | state survives: a second run in the same directory skipped "Installing" and continued after the language choice |
| G-code injection over USB CDC | `-chardev socket,id=stm32usbfscdc,port=15555,host=localhost,server=on,wait=off`, then `M115` / `M105` over TCP | **no reply and no output** in two runs (connected before and after the firmware switched USB to device mode) |
| Debugger | `-gdb tcp::12345`, then `gdb-multiarch build/<dir>/firmware -ex 'target remote localhost:12345'` (`apt-get install -y gdb-multiarch`) | works (breakpoints, registers, memory; used on MK4 below). The toolchain's own `arm-none-eabi-gdb` does not start on Ubuntu 24.04: `libncursesw.so.5: cannot open shared object file` |
| QEMU event log | `-d int,cpu_reset,guest_errors -D <file>` | works; shows exceptions and resets of the guest |

The full list of script console commands comes from running the binary
with `-append scripthelp`. For `prusa-mini` the contexts are `E`, `X`, `Y`,
`Z`, `KeyCtl`, `ScriptHost`, `encoder-input`, `fan`, `fan1`,
`generic-spi-display`, `heater`, `heater1`, `ir-sensor`, `pinda`,
`thermistor` and `thermistor1..4`.

Not achieved on the MINI:

- A known PrusaLink password. Seeding the config store through an EEPROM
  image the way the MK4 tests do (items `Run Selftest`, `PrusaLink
  Password`, ... generated with `utils/persistent_stores/eeprom.py` and
  passed as two `-drive if=pflash` files) had no visible effect on
  `prusa-mini`: the wizard still started and `X-Api-Key` was rejected with
  401. Not investigated further; the password can presumably be set through
  the menus with the encoder (not run).
- Any way to send G-code. The remaining candidate is a `.gcode` file in the
  USB directory started through the UI (not run).

## Integration tests on MK4

Firmware: `python3 utils/build.py --preset mk4 --build-type release
--bootloader no` (271 s, stage 3).

### 1. Python environment

| Step | Result |
|---|---|
| `python3 utils/bootstrap.py --include-integration` | exit 0 after 353 s; `.venv` grows from 226 MB to 5.8 GB (torch 2.15.0 with its CUDA libraries, torchvision, opencv, scipy, ...). It also **upgrades** `numpy` 1.26.4 → 2.5.5, `pytest` 7.3.2 → 8.4.2 and `setuptools` 70.1.1 → 84.2.1, against the pins in `requirements.txt` |
| any later `python3 utils/build.py ...` without `--skip-bootstrap` | re-runs `pip install -r requirements.txt` and puts `numpy`, `pytest` and `setuptools` back, leaving `pytest-asyncio` 0.26.0, `scipy` 1.18.1 and `opencv-python-headless` 5.1 with unmet requirements (`pip check` lists them) |
| `pytest tests/integration --firmware build/mk4_release_noboot/firmware.bin -x` in that state | fails in 1 s before collecting: `ImportError: cannot import name 'FixtureDef' from 'pytest'` (pytest-asyncio 0.26.0 needs pytest ≥ 8.2) |

So the two requirement files cannot both be satisfied in one venv today,
and every build without `--skip-bootstrap` silently breaks the test
environment again. This comes from the unpinned upper ends in
`integration-test-requirements.txt`, not from the Python version.

What was used instead: a second venv with the same two pip steps as
bootstrap, and `BUDDY_NO_VIRTUALENV=1` so that `utils/simulator` does not
re-execute itself with `.venv`:

```bash
python3 -m venv <scratch>/venv-it
<scratch>/venv-it/bin/pip install -r requirements.txt
<scratch>/venv-it/bin/pip install -r integration-test-requirements.txt
```

On first use easyocr downloads its models (94 MB, two GitHub release
assets) into `~/.EasyOCR`.

### 2. Test run

```bash
BUDDY_NO_VIRTUALENV=1 LD_LIBRARY_PATH=$PWD/build/simlibs \
    <scratch>/venv-it/bin/pytest tests/integration \
    --firmware build/mk4_release_noboot/firmware.bin -o cache_dir=<scratch>/pytest-cache
```

(`-o cache_dir=...` only keeps `.pytest_cache`, which is not git-ignored,
out of the repository. Disabling the cache plugin instead breaks the
fixtures, which use `config.cache`.)

Result: **28 collected, 22 errors, 6 skipped, 0 passed, 396 s.** With `-x`
the run stops at the first error after 23 s.

| File | Tests | Outcome |
|---|---|---|
| `test_basic_examples.py` | 2 | 2 errors |
| `test_safety.py` | 10 | 9 errors, 1 skipped |
| `test_prusa_link.py` | 16 | 11 errors, 5 skipped |

The skips are `@pytest.mark.skip()` in the sources. All 22 errors happen in
fixture setup, while the suite prepares the external flash by booting the
firmware once (`prepare_xflash_content`): 21 times
`AssertionError: simulator is not running`, once a `FileNotFoundError` for
a screenshot that the dying simulator no longer wrote. The simulator
process ends about 15 s after start.

### 3. Why the simulator dies, and what is behind it

Each step was reproduced outside pytest with `sim_probe.py` and the same
QEMU arguments as the tests.

**Problem 1: the simulator aborts on a display command.** QEMU prints
`spi_display: COLMOD 61 not implemented.` and exits with SIGABRT. Backtrace
(`gdb -batch -ex run -ex bt --args qemu-system-buddy ...`):

```
#4  __GI_abort ()
#5  spi_display_transfer (dev=<optimized out>, data=97) at ../hw/arm/prusa/parts/spi_display.c:239
#6  ssi_transfer (bus=<optimized out>, val=val@entry=97) at ../hw/ssi/ssi.c:124
#7  stm32_common_spi_write (..., data=97, size=1) at ../hw/arm/prusa/stm32_common/stm32_spi.c:298
```

The firmware side is `ili9488_fill_rect_color_fast()` in
`src/guiapi/src/ili9488.cpp`, which switches the ILI9488 to a 3-bit pixel
format (`TRIBIT_COLMOD = 0x61`) for black and white fills. It is used when
the config store item "Fast Draw Enabled" is true, and that item defaults to
`DEVELOPMENT_ITEMS()`, which is on in a default local build and in CI's
non-release builds.

**Problem 2: with fast draw off, the firmware hard-faults.** The item was
switched off through the simulated EEPROM, without changing the firmware:

```python
# run from utils/, writes the two pflash images the tests also use
import struct
from persistent_stores import eeprom
items = [('Run Selftest', b'\0'), ('Run XYZ Calibration', b'\0'),
         ('Run First Layer', b'\0'), ('FSensor Enabled', b'\0'),
         ('Selftest Result Gears', 21 * b'\xaa'),
         ('Fast Draw Enabled', b'\0')]
bank = eeprom.generate_bank(sequence_id=1, version=1, items=items)
open('eeprom1.bin', 'wb').write(bank.ljust(8192, b'\xff'))
open('eeprom2.bin', 'wb').write(b'\xff' * 8192)
```

QEMU then keeps running, but the screen stays black and the QEMU log
(`-d int,cpu_reset,guest_errors`) shows, about 22 s after start:

```
Taking exception 4 [Data Abort] on CPU 0
...at fault address 0x0
...with CFSR.DACCVIOL and MMFAR 0x0
...taking pending nonsecure exception 3
```

followed by the crash-dump code reading the SCB registers and a reset from
`sys_reset()`. With `gdb-multiarch` stopped at `HardFault_Handler`, the
stacked frame gives `pc = 0x0800cca8`, `lr = 0x080d22e5`;
`arm-none-eabi-addr2line -f -C -i -e build/mk4_release_noboot/firmware`
resolves them to `Touchscreen_GT911::handle_read_error()` and
`xQueueSemaphoreTake`. The faulting instruction is `ldr r2, [r3, #0]` with
`r3 = 0`, right after constructing an `i2c::ChannelMutex`. After that reset
the simulator's clock model reports `CPUCLOCK set to 0 Hz` and nothing
further happens. (The simulator has no GT911 touch controller; whether the
same null access can happen on real hardware when the touch controller
does not answer was not examined.)

**Problem 3: with touch also off, the firmware refuses the board.** Adding
`('Touch Enabled', b'\0')` to the EEPROM items avoids the fault. The
firmware then draws a blue screen:

```
Fallback BSOD
(possibly BSODception)
8.0.2+55.LOCAL
main.cpp:408
BOM ID not compatible
```

on `prusa-mk4-027c` and equally on `prusa-mk4-034`.
`Configuration::check_bom_compatible()` in
`src/hw/xbuddy/hw_configuration.cpp` accepts board BOM IDs 37 and above
(and 12 or 14 for early units), so the two MK4 boards this simulator
emulates (0.2.7c and 0.3.4) are both rejected. This is in the firmware's
start-up path and no setting was found that avoids it.

Conclusion for MK4: firmware 8.0.2 and mini404 0.9.10 are out of step. That
matches stage 1: the integration stage and the `mk4` `noboot` build are
commented out in the CI pipeline ("No integration tests anymore").

Not tested: newer simulator releases. `git ls-remote --tags` on the
simulator's repository shows `v0.9.11` and `v0.9.12` after the pinned
`v0.9.10`, and their Linux archives are reachable. Downloading and running
a binary that is not among the project's pinned dependencies was refused by
the permission policy of the session that ran these experiments, so
whether a newer release accepts this firmware is unknown. It would be the
first thing to try (`pytest ... --simulator <path>` exists for that).

Also not run: the MK3.5 machines (`prusa-mk3-35`, `prusa-mk3-39`; the
launcher has no MK3.5 option and the same BOM check applies to that
firmware), the XL machines, the graphical mode.

## Recipe: check a firmware change in the simulator (MINI only)

This is the sequence that was run last, on an idle machine, exactly as
written. It is limited to the MINI firmware; for MK4, MK3.5, XL, iX and
CORE One there is no working simulator set-up here.

```bash
# once, as root: libraries for the simulator binary (Ubuntu 24.04)
apt-get install -y libglu1-mesa libglew2.2 libglut3.12
mkdir -p build/simlibs
ln -sf /usr/lib/x86_64-linux-gnu/libglut.so.3.12 build/simlibs/libglut.so.3

# build (222 s from scratch, about 37 s after a one-file change)
python3 utils/build.py --preset mini --build-type release --bootloader yes

# fresh simulator state
W=build/sim-mini
rm -rf $W && mkdir -p $W/usbdir
cp .dependencies/bootloader-mini-2.6.0/bootloader.bin build/mini_release_boot/firmware.bbf $W/
cp build/mini_release_boot/firmware.bbf $W/usbdir/

# run 120 s headless: a screenshot every 5 s, choose the language at 75 s,
# read a thermistor at 100 s, PrusaLink on http://localhost:18080/
LD_LIBRARY_PATH=$PWD/build/simlibs python3 doc/claude/experiments/sim_probe.py $W 120 --shots 5 \
    --at 75 'encoder-input::Twist(-1)' --at 77 'encoder-input::Push()' \
    --at 100 'thermistor::GetTemp()' \
    -- $PWD/.dependencies/mini404-0.9.10/qemu-system-buddy -machine prusa-mini \
    -kernel {work}/firmware.bbf -global STM32F4xx-usb.disable_sof_interrupt=true -icount 2 \
    -drive id=usbstick,format=raw,file=fat:rw:{work}/usbdir -device usb-storage,drive=usbstick \
    -netdev user,id=mini-eth,hostfwd=tcp::18080-:80 -nographic
```

What that run produced:

- `build/sim-mini/shot-NNN.png`: "Installing" at 10 s, "SELECT LANGUAGE"
  from 15 s, "English" focused at 75 s, the setup wizard from 80 s.
- `curl http://localhost:18080/` at 95 s → 200, 7211 bytes.
- `thermistor::GetTemp()` → `18.000000`.
- Exit code 0 when the helper stopped QEMU after 120 s.

To continue from the reached state, run the last command again without
recreating `$W`: the EEPROM and flash images in it are kept. To look at a
specific screen, extend the `--at` list with more `Twist`/`Push` steps and
read the screenshots; the schedule is in wall-clock seconds, so leave a few
seconds between steps.

Limits of this recipe: it exercises the GUI, the menus, the network stack
and the start-up path of the MINI firmware. It does not print anything
(no G-code path was found), and the PrusaLink API stays behind a password
that was not set.
