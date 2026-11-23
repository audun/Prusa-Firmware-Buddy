# Watchdog symbols in the release firmware ELFs

Question (asked by the architecture investigation): do the built release
firmwares link in and call the hardware (independent) watchdog code, in
particular MK4 and MK3.5?

This note records what the symbols and the disassembly of the built ELFs
show. It does not interpret runtime behaviour, and it says nothing about what
the bootloader does before the firmware starts. No source file was changed.

ELFs examined: the eight `--build-type release --bootloader yes` builds of
stage 3 at `b1043d68b` (`build/<preset>_release_boot/firmware`), built with
the bootstrap toolchain (Arm GNU Toolchain 13.3.Rel1) and LTO.

## Result

| Preset | `wdt_iwdg_init()` symbol present | Init called from | `wdt_iwdg_refresh()` symbol present | Refresh called from | Body of `wdt_iwdg_refresh()` | `hiwdg` object |
|---|---|---|---|---|---|---|
| `mini` | no | no call instruction (see "Further observations") | yes | `Temperature::manage_heater()`, `_bsod`, `crash_dump::before_dump()` | writes `0xAAAA` through `hiwdg` if set (11 lines) | `B` (`.bss`) |
| `mk3.5` | **no** | no call instruction | yes | `crash_dump::before_dump()`, `marlin_server::_server_print_loop()` | **single `bx lr`** | **`R` (read-only data)** |
| `mk4` | **no** | no call instruction | yes | `crash_dump::before_dump()`, `marlin_server::_server_print_loop() [clone .lto_priv.0]` | **single `bx lr`** | **`R` (read-only data)** |
| `xl` | no | no call instruction (see "Further observations") | yes | `Temperature::manage_heater()` (2 sites), `_bsod`, `crash_dump::before_dump()`, `marlin_server::_server_print_loop() [clone .lto_priv.0]` | writes `0xAAAA` (12 lines) | `B` |
| `ix` | no | no call instruction (see "Further observations") | yes | `Temperature::manage_heater()`, `_bsod`, `crash_dump::before_dump()`, `marlin_server::cycle() [clone .part.0]` | writes `0xAAAA` (12 lines) | `B` |
| `coreone` | **yes** (`T`) | `app_run()` (2 sites) | yes | `Temperature::updateTemperaturesFromRawValues()`, `_bsod`, `app_run()` (3 sites), `crash_dump::before_dump()`, `marlin_server::_server_print_loop()` | writes `0xAAAA` (9 lines) | `B` |
| `coreonel` | **yes** (`T`) | `app_run()` (2 sites) | yes | same as `coreone` | writes `0xAAAA` (9 lines) | `B` |
| `coreone_indx` | no | no call instruction (see "Further observations") | yes | `Temperature::updateTemperaturesFromRawValues()`, `_bsod`, `crash_dump::before_dump()`, `marlin_server::_server_print_loop() [clone .lto_priv.0]` | writes `0xAAAA` (12 lines) | `B` |

In every ELF:

- `iwdg_warning_cb()` and the variables `wdt_iwdg_counter`,
  `wdt_iwdg_warning_cb`, `wdt_wwdg_counter` are present.
- There is no `HAL_IWDG_Init`, `HAL_IWDG_Refresh`, `watchdog_init` or
  `watchdog_refresh` symbol, and no branch to a symbol with such a name. All
  eight firmwares are linked with LTO, so a missing symbol can also mean
  "inlined into its caller".
- The remaining matches of the `nm` filter are the ADC watchdog (`awdg::`,
  `ADCWatchdog`) and the GUI menu item `MI_WATCHDOG`; 22 to 29 such lines
  per ELF, listed in the raw file.

## Further observations (constants, not symbols)

Because LTO can inline the init function, the disassembly was also searched
for the constants that IWDG start-up code has to use on STM32F4: the IWDG
peripheral base address `0x40003000` as a literal-pool word, and the key
values `0xCCCC` (start) and `0x5555` (register unlock) as immediates.

| Preset | Function containing the literal `0x40003000` | Function loading `0xCCCC` and `0x5555` | LTO file symbol `stm32f4xx_hal_iwdg.c.<hash>` in `nm` |
|---|---|---|---|
| `mini` | `app_run()` | `app_run()` | yes |
| `mk3.5` | **none in the whole ELF** | **none** | **no** |
| `mk4` | **none in the whole ELF** | **none** | **no** |
| `xl` | `app_run()` | `app_run()` | yes |
| `ix` | `app_run()` | `app_run()` | yes |
| `coreone` | `wdt_iwdg_init()` | `wdt_iwdg_init()` | yes |
| `coreonel` | `wdt_iwdg_init()` | `wdt_iwdg_init()` | yes |
| `coreone_indx` | `app_run()` | `app_run()` | yes |

Also checked and found in none of the eight ELFs: a `movw`/`movt` pair
forming `0x40003000`, and a literal-pool word with the window watchdog base
`0x40002c00`.

`wdt_iwdg_refresh()` as built, shortest and longest form:

```
mk4, mk3.5:
    bx   lr

mini:
    ldr  r3, [pc, #20]      @ .word 0x20004e1c  (address of hiwdg)
    ldr  r3, [r3, #0]
    cbz  r3, <return>
    movw r2, #43690         @ 0xaaaa
    str  r2, [r3, #0]
    ldr  r3, [pc, #12]
    movs r2, #0
    str.w r2, [r3, #184]
    bx   lr
```

## What this does and does not show

- In the `mk4` and `mk3.5` release ELFs the refresh function is an empty
  function, the `hiwdg` handle sits in read-only data, and no instruction or
  literal in the ELF refers to the IWDG peripheral address or its start and
  unlock keys.
- In the other six ELFs the IWDG address and both keys appear in exactly one
  function: `wdt_iwdg_init()` where that symbol exists (`coreone`,
  `coreonel`), otherwise `app_run()`.
- For reference, `grep -n '#define USE_WATCHDOG' include/marlin/Configuration_*_adv.h`
  at this commit matches the `COREONE`, `COREONEL`, `MINI`, `XL`,
  `XL_DEV_KIT`, `XL_Dwarf` and `iX` files and no `MK4` or `MK3.5` file.
- Not examined: whether the bootloader starts the watchdog, what happens at
  run time, the sub-board firmwares, debug builds and `--bootloader no`
  builds.

## Commands

`<nm>` and `<objdump>` are `arm-none-eabi-nm` and `arm-none-eabi-objdump`
from `.dependencies/gcc-arm-none-eabi-13.3.1/bin/`. The complete, unfiltered
output of every command for every ELF is in
[`06-watchdog-symbols-raw.txt`](06-watchdog-symbols-raw.txt).

```bash
# 1. symbols
<nm> -C build/<preset>_release_boot/firmware | grep -i -E 'watchdog|iwdg|wdt'

# 2. call sites, as suggested
<objdump> -d -C build/<preset>_release_boot/firmware \
    | grep -i -E 'bl.*(watchdog_init|HAL_IWDG_Init|wdt_)'

# 3. call sites with the name of the calling function, plus peripheral
#    base literals
<objdump> -d -C build/<preset>_release_boot/firmware \
    | awk -f callers.awk | sort | uniq -c

# 4. key constants
<objdump> -d -C build/<preset>_release_boot/firmware \
    | awk -f keys.awk | sort | uniq -c

# 5. body of the refresh function
<objdump> -d -C --disassemble='wdt_iwdg_refresh()' \
    build/<preset>_release_boot/firmware | grep -E '^\s+[0-9a-f]+:'
```

`callers.awk`:

```awk
# Print every branch to a watchdog-named symbol together with the function
# that contains it, and every literal-pool word that is the IWDG or WWDG
# peripheral base address (STM32F4: 0x40003000 / 0x40002c00).
/^[0-9a-f]+ <.*>:$/ { fn = $0; sub(/^[0-9a-f]+ </, "", fn); sub(/>:$/, "", fn) }
/\tb[a-z.]*\t[0-9a-f]+ </ {
    target = $0; sub(/^.*\t[0-9a-f]+ </, "", target); sub(/>$/, "", target)
    low = tolower(target)
    if (low ~ /watchdog|iwdg|wdt/ && low !~ /awdg|adcwatchdog|mi_watchdog/) {
        insn = $0; sub(/^[^\t]*\t[^\t]*\t/, "", insn); sub(/\t.*$/, "", insn)
        print "CALL\t" fn "\t" insn "\t" target
    }
}
/\.word\t0x40003000/ { print "IWDG_BASE_LITERAL\t" fn }
/\.word\t0x40002c00/ { print "WWDG_BASE_LITERAL\t" fn }
```

`keys.awk`:

```awk
# Print functions that load the IWDG key constants 0xCCCC (start) or 0x5555
# (register unlock) as immediates, and movw/movt pairs forming 0x40003000.
/^[0-9a-f]+ <.*>:$/ { fn = $0; sub(/^[0-9a-f]+ </, "", fn); sub(/>:$/, "", fn) }
/\t(movw|mov\.w|movs?)\t[a-z0-9]+, #52428\t/ { print "KEY_0xCCCC\t" fn }
/\t(movw|mov\.w|movs?)\t[a-z0-9]+, #21845\t/ { print "KEY_0x5555\t" fn }
/\tmovw\t[a-z0-9]+, #12288\t/ { pending = NR }
/\tmovt\t[a-z0-9]+, #16384\t/ { if (NR - pending <= 2) print "MOVW_MOVT_0x40003000\t" fn }
```

In the output of command 3, lines such as
`CALL wdt_iwdg_init() bls.n wdt_iwdg_init()+0x2a` and
`CALL iwdg_warning_cb() b.n iwdg_warning_cb()+0x18` are branches inside the
named function, not calls.
