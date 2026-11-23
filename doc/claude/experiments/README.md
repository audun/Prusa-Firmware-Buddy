# Linux build and test experiments

Empirical results for firmware 8.0.2 (`master` at `b1043d68b`) on Ubuntu
24.04, 4 cores, 15 GiB RAM, GCC 13.3, Python 3.13. Every number in these
notes was measured there; anything not run is marked "not run". No tracked
source file was changed for any experiment.

## What works on Linux

| Area | Result | Details |
|---|---|---|
| Bootstrap | works: about 4 min, 2.6 GB | [00-environment.md](00-environment.md) |
| CI gate | understood and partly reproduced: format check, one CI-exact `-Werror` build, unit tests, reports | [01-ci-gate.md](01-ci-gate.md) |
| Host unit tests | **859 of 859 pass**, none flaky; build 221 s, run 67 s (11 s without `slow`) | [02-unit-tests.md](02-unit-tests.md) |
| Firmware builds | 8 main-board presets, 8 MINI language presets, 3 sub-board presets and `mk4` noboot all build; 222 to 367 s per main board | [03-firmware-builds.md](03-firmware-builds.md) |
| Flash headroom | MINI: 12 028 to 18 140 bytes free (1.3 to 2.0 %), tightest `mini-en-ja`; all other boards more than 30 % free | [03-firmware-builds.md](03-firmware-builds.md) |
| Simulator | bundled simulator runs headless; **MINI firmware boots and can be driven**; MK4 firmware is rejected | [04-simulator.md](04-simulator.md) |
| Integration tests | **cannot pass**: 22 errors, 6 skipped, 0 passed | [04-simulator.md](04-simulator.md) |
| Option matrix | 157 options × 21 builds | [05-option-matrix.md](05-option-matrix.md), [CSV](05-option-matrix.csv) |
| Watchdog symbols | per-ELF facts for 8 release builds | [06-watchdog-symbols.md](06-watchdog-symbols.md) |

## Fastest reliable commands

All from the repository root after `python3 utils/bootstrap.py`.

**(a) Unit tests of one area** (first full build 221 s, afterwards about
2 s per changed file):

```bash
. .venv/bin/activate
python3 utils/build_tests.py gcode_parser_tests gcode_reader_test   # build only these targets
(cd build/tests/tests/unit/common/gcode && ctest -LE slow)          # run that directory: 59 tests, 4 s
python3 utils/build_tests.py --run -- -LE slow                      # everything fast: 856 tests, 11 s
```

`ctest -R <text>` matches test-case names, not directories, so it is not a
reliable way to select an area; running `ctest` inside the build
subdirectory is.

**(b) One firmware** (`coreone`: 337 s from scratch, 30 to 37 s after a
one-file change, 65 s for a clean rebuild with a warm ccache):

```bash
python3 utils/build.py --preset coreone --build-type release --bootloader yes --skip-bootstrap
.dependencies/ninja-1.10.2/ninja -C build/coreone_release_boot     # same, without the 6 s wrapper overhead
```

Add `-DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"` to match CI. The linker
prints flash and RAM use at the end of every build.

**(c) Simulator** (MINI only, headless, UI reached after about 15 s):

```bash
mkdir -p build/simlibs build/sim-mini/usbdir
ln -sf /usr/lib/x86_64-linux-gnu/libglut.so.3.12 build/simlibs/libglut.so.3
cp .dependencies/bootloader-mini-2.6.0/bootloader.bin build/mini_release_boot/firmware.bbf build/sim-mini/
cp build/mini_release_boot/firmware.bbf build/sim-mini/usbdir/
LD_LIBRARY_PATH=$PWD/build/simlibs python3 doc/claude/experiments/sim_probe.py build/sim-mini 120 --shots 5 \
    --at 75 'encoder-input::Twist(-1)' --at 77 'encoder-input::Push()' \
    -- $PWD/.dependencies/mini404-0.9.10/qemu-system-buddy -machine prusa-mini \
    -kernel {work}/firmware.bbf -global STM32F4xx-usb.disable_sof_interrupt=true -icount 2 \
    -drive id=usbstick,format=raw,file=fat:rw:{work}/usbdir -device usb-storage,drive=usbstick \
    -netdev user,id=mini-eth,hostfwd=tcp::18080-:80 -nographic
```

Screenshots land in `build/sim-mini/shot-NNN.png`, PrusaLink answers on
`http://localhost:18080/`.

## Blockers and traps found

1. **Integration tests are dead at this commit.** The MK4 firmware cannot
   run in the bundled simulator (mini404 0.9.10): the simulator aborts on
   the display's fast-fill command, then the firmware hard-faults in
   `Touchscreen_GT911::handle_read_error()`, then it stops with "BOM ID not
   compatible" on both simulated MK4 boards. CI has the stage commented
   out. Newer simulator releases exist and were not tested.
2. **No simulator for MK3.5, XL, iX or CORE One** was shown to work; only
   the MINI was.
3. **No way to inject G-code into the simulated MINI** was found, and no
   PrusaLink credentials could be set from outside.
4. **The MINI debug build does not fit**: 330 976 bytes over. Only
   `mini_big_flash` (2 MB chip) builds in debug.
5. **`requirements.txt` and `integration-test-requirements.txt` conflict**
   (`pytest`, `numpy`, `setuptools`), and every `utils/build.py` run without
   `--skip-bootstrap` reinstalls the former. Use a separate venv for the
   integration tests.
6. **Python 3.13** works but compiles `numpy==1.26.4` from source (146 s
   instead of 10 s with Python 3.12, which is what CI uses).
7. **OS packages** beyond a compiler: `gettext` (unit tests need `msgfmt`,
   the translations report needs `xgettext`); `libglu1-mesa libglew2.2
   libglut3.12` plus a `libglut.so.3` symlink for the simulator;
   `gdb-multiarch` because the toolchain's `arm-none-eabi-gdb` needs
   `libncursesw.so.5`; `ccache` is optional and picked up automatically.
8. **Every git commit changes the embedded version** (`8.0.2+<commit
   count>`), so the first build after a commit relinks even without source
   changes (42 s for `coreone`).
9. **First rebuild after a cold unit-test build is not a no-op** (CMake
   re-runs, about 22 s once).
10. **CI gates are narrower than they look**: no size check besides the
    linker, the three "pre-release checks" cannot fail the build, and the
    translations report script exits early on a complete translation.
11. Specific to the sandbox these experiments ran in: GitHub source
    archives were blocked, so bootstrap needed a git-based wrapper
    (stage 0). Not expected on a normal machine.
