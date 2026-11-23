# Stage 3: firmware builds

All builds at `b1043d68b`, from the repository root with the bootstrap venv
active, on 4 cores, with the bootstrap toolchain (`arm-none-eabi-gcc` 13.3.1,
cmake 3.28.3, ninja 1.10.2). No tracked file was changed.

**Verdict:** every preset that was tried builds on Linux, except the
`debug` build of `mini`, which cannot fit a 1 MB chip. A main-board firmware
takes 3.7 to 6.1 minutes from scratch, a sub-board firmware 15 to 30 s, a
one-file rebuild about 37 s. The MINI has 12 to 18 kB of flash left
(1.3 to 2.0 %); every other board has more than 30 % free.

## Command

```bash
python3 utils/build.py --preset <name> --build-type release --bootloader yes
```

- Build directory: `build/<preset>_<build type>_<boot|noboot|emptyboot>`,
  holding `firmware` (ELF with debug info, about 120 MB), `firmware.bin`,
  `firmware.bbf`, `firmware.map` and the generated option headers under
  `include/option/`.
- Products are also copied to `build/products/` as
  `<preset>_<build type>_<boot|noboot>` (ELF), `.bin`, `.bbf`, `.map`.
- `--bootloader` defaults to `empty`; CI always uses `yes` (stage 1).
- The linker prints a "Memory region / Used Size / Region Size" table at
  the end of every build (`-Wl,--print-memory-usage`), so the headroom
  figures below can be read straight from the build output.
- A main-board preset also builds the firmwares it carries in its resources
  as CMake sub-builds (`<build dir>/*-build`): `esp8266-nic` or `esp32-nic`
  for all, plus `dwarf`, `modularbed`, `can`, `htcontroller`,
  `tool_offset_sensor` for `xl`; `modularbed`, `xbuddy_extension` for `ix`;
  `anfc`, `xbuddy_extension` for `coreone` and `coreonel`;
  `indx_head`, `tool_offset_sensor`, `xbuddy_extension` for `coreone_indx`.
- Release flags as seen in `build.ninja`: `-mcpu=cortex-m4 -std=gnu++23 -g
  -Os ... -O3 -DNDEBUG -flto`, link with `--gc-sections` and
  `-Wl,--fatal-warnings`.

## Results: main boards, `release`, `--bootloader yes`

Flash and RAM figures come from the ELF section headers
(`arm-none-eabi-objdump -h`) and the "Memory Configuration" block of
`firmware.map`, using the script at the end of this note. "Flash used" is
the distance from the start of the `FLASH` region to the end of the last
loaded section; it equals the size of `firmware.bin` in every build.

| Preset | Result | Wall time | Ninja steps | Flash used / available | **Flash free** | `.data` + `.bss` / RAM region | RAM left after static data | CCMRAM used / 65 536 | `.bbf` size |
|---|---|---|---|---|---|---|---|---|---|
| `mini` | ok | 222.3 s | 1092 | 898 740 / 916 480 | **17 740 (1.9 %)** | 3 696 + 83 188 / 130 972 | 44 084 | 58 040 | 1 595 720 |
| `mk3.5` | ok | 252.9 s | 1185 | 1 134 392 / 1 965 056 | 830 664 (42.3 %) | 4 120 + 95 344 / 196 508 | 97 040 | 58 364 | 2 834 892 |
| `mk4` | ok | 267.4 s | 1203 | 1 207 512 / 1 965 056 | 757 544 (38.6 %) | 4 648 + 106 600 / 196 508 | 85 256 | 58 364 | 3 061 612 |
| `xl` | ok | 367.1 s | 1893 | 1 301 548 / 1 965 056 | 663 508 (33.8 %) | 5 520 + 117 700 / 196 508 | 73 284 | 61 948 | 4 271 808 |
| `ix` | ok | 253.4 s | 1167 | 1 128 052 / 1 965 056 | 837 004 (42.6 %) | 4 648 + 114 400 / 196 508 | 77 456 | 62 100 | 1 825 032 |
| `coreone` | ok | 337.2 s | 1308 | 1 304 112 / 1 965 056 | 660 944 (33.6 %) | 5 032 + 110 316 / 196 508 | 81 156 | 61 948 | 3 700 932 |
| `coreonel` | ok | 331.3 s | 1319 | 1 306 304 / 1 965 056 | 658 752 (33.5 %) | 4 976 + 111 636 / 196 508 | 79 892 | 61 948 | 3 928 404 |
| `coreone_indx` | ok | 328.7 s | 1913 | 1 312 252 / 1 965 056 | 652 804 (33.2 %) | 5 448 + 113 516 / 196 508 | 77 540 | 61 948 | 3 821 712 |

All sizes are bytes. No compiler warning appears in any of the eight build
logs (`grep -c 'warning:'` is 0). Each build directory takes 0.8 to 1.2 GB.

Memory layout behind these numbers:

| | MINI (`stm32f407vg_boot.ld`) | all other main boards (`stm32f42x_boot.ld`) |
|---|---|---|
| Flash region with bootloader | origin `0x08020200`, 895 K = 916 480 bytes | origin `0x08020200`, 1919 K = 1 965 056 bytes |
| Flash region without bootloader (`stm32f407vg.ld`, `stm32f42x.ld`) | origin `0x08000000`, 1024 K | origin `0x08000000`, 2048 K |
| RAM | 128 K total: 100 bytes `DXRAM` + 130 972 bytes `RAM` | 192 K total: 100 bytes `DXRAM` + 196 508 bytes `RAM` |
| CCMRAM | 64 K | 64 K |
| BACKUP_RAM | 4 K | 4 K |

Both include `src/device/stm32f4/linker/stm32f4_platform.ld`.

### Heap and stack

- The linker script states `_Min_Heap_Size = 0x800` and
  `_Min_Stack_Size = 0x800` (2048 bytes each). They only size the
  `._user_heap_stack` section (4096 bytes) that makes the link fail when
  less than that is left; they are not the real heap or stack size.
- The real heap is everything between the end of `.bss` (symbol `end`) and
  `_estack`, minus the interrupt stack: `src/common/heap.cpp` implements
  `_sbrk` on that range and reserves `ISR_STACK_LENGTH_BYTES` (2048 on main
  boards, 512 on sub-boards) at the top. So "RAM left after static data"
  minus 2048 is the heap that `malloc` can hand out: about 42 kB on the
  MINI, 71 to 95 kB on the others.
- FreeRTOS does not have a heap of its own here: `pvPortMalloc` and
  `vPortFree` in the ELF are 4-byte functions from `heap.cpp`, no
  `heap_N.c` array (`ucHeap`) is linked, and `configTOTAL_HEAP_SIZE`
  (40960 in `include/stm32f4_hal/FreeRTOSConfig.h`) is therefore unused.
- Task stacks are static. The largest RAM symbol in the `mini` ELF is
  `task_stack`, 27 584 bytes, placed in CCMRAM
  (`arm-none-eabi-nm --size-sort -S`).

## MINI flash headroom

The plain `mini` preset has **every translation disabled**
(`ENABLE_TRANSLATION_*() 0` in its option headers), so it is the
English-only image. The `mini-en-<lang>` presets add one language each.
Translation data goes into the `.bbf` resources, not into the MCU flash, so
the flash cost of a language is small:

| Preset | Flash used | **Flash free of 916 480** | `.bbf` size | Result |
|---|---|---|---|---|
| `mini` (English only) | 898 740 | **17 740** | 1 595 720 | ok |
| `mini-en-cs` | 899 444 | **17 036** | 1 668 104 | ok, built with the exact CI flags (below) |
| `mini-en-de` | 899 396 | **17 084** | 1 668 056 | ok |
| `mini-en-es` | 899 404 | **17 076** | 1 668 064 | ok |
| `mini-en-fr` | 899 404 | **17 076** | 1 668 064 | ok |
| `mini-en-it` | 899 404 | **17 076** | 1 668 064 | ok |
| `mini-en-ja` | 904 452 | **12 028** | 1 683 352 | ok |
| `mini-en-pl` | 899 404 | **17 076** | 1 668 064 | ok |
| `mini-en-uk` | 898 340 | **18 140** | 1 687 480 | ok |

- The tightest MINI image at this commit is `mini-en-ja` with 12 028 bytes
  (1.3 %) to spare. The CI comment calls `mini-en-pl` the one with the
  "largest memory requirement"; in flash it is not.
- Static RAM is the same for all MINI variants: 3 696 + 83 232 bytes of
  130 972, CCMRAM 58 040 of 65 536 (88.6 %).
- `mini-en-cs` was built the way CI builds it:
  `python3 utils/build.py --preset mini-en-cs --build-type release
  --bootloader yes --no-store-output --skip-bootstrap
  --version-suffix=-DEV+54.PR0.B0 --version-suffix-short=-DEV+54
  -DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"
  -DDEVELOPMENT_ITEMS_ENABLED:BOOL=yes`. It passes, so `-Werror` is not an
  obstacle with this toolchain.
- The eight `mini-en-*` builds ran while simulator experiments used the
  same machine; their wall times (240 to 300 s) are not clean measurements.

### `mini` in `debug`: does not link

```bash
python3 utils/build.py --preset mini --build-type debug --bootloader yes
```

Fails after 262.9 s at the final link:

```
ld: firmware section `.text' will not fit in region `FLASH'
ld: region `FLASH' overflowed by 330976 bytes
Memory region         Used Size  Region Size  %age Used
           FLASH:     1247456 B       895 KB    136.11%
             RAM:      101372 B     130972 B     77.40%
          CCMRAM:       58444 B        64 KB     89.18%
collect2: error: ld returned 1 exit status
```

A debug image needs 1 247 456 bytes, 36 % more than a MINI has. The preset
list contains `mini_big_flash` (`MCU=STM32F429VI`, `ALLOW_MISSING_OTP=True`)
for this case:

```bash
python3 utils/build.py --preset mini_big_flash --build-type debug --bootloader yes
```

builds in 307.2 s (ccache was installed by then, cold cache) with `-Og` and
links against `stm32f42x_boot.ld`: 1 248 312 of 1 965 056 flash bytes used,
3 852 + 93 416 bytes of static RAM. It targets a board fitted with a 2 MB
STM32F429VI, not a stock MINI, so a debug build of the MINI firmware cannot
be flashed to an unmodified printer.

## Other builds

| Preset, type, bootloader | Result | Wall time | Flash used / available (free) | `.data` + `.bss` (+ other) / RAM (left) | Linker script |
|---|---|---|---|---|---|
| `mk4`, release, `--bootloader no` | ok | 271.0 s | 1 205 856 / 2 097 152 (891 296) | 4 632 + 106 576 / 196 508 (85 296) | `stm32f42x.ld` |
| `xl-dwarf`, release, yes | ok | 29.4 s | 77 352 / 122 752 (45 400, 37.0 %) | 268 + 9 396 + 172 / 36 864 (27 028) | `src/device/stm32g0/linker/stm32g070rb_boot.ld` |
| `xl-modularbed`, release, yes | ok | 15.4 s | 37 992 / 122 752 (84 760, 69.0 %) | 128 + 12 220 + 172 / 36 864 (24 340) | same |
| `coreone-xbuddy_extension`, release, yes | ok | 19.9 s | 62 692 / 122 752 (60 060, 48.9 %) | 8 832 + 18 256 + 612 / 31 744 (4 032) | `src/puppy/xbuddy_extension/stm32h503_boot.ld` |

Notes on the sub-boards:

- Their linker scripts have four regions: `FLASH_BOOTLOADER` (8 K),
  `FLASH` (122 752 bytes), `FW_DESCRIPTOR` (128 bytes) and `RAM`
  (plus a 1 K `ISR_STACK` region on the STM32H503).
- The STM32G0 script pads the image to the end of `FLASH` with a
  `.flash_fill` section, so the linker's own report always says
  `FLASH: 122752 B ... 100.00%` for the Dwarf and the modular bed. The table
  above subtracts that padding.
- `stm32g070rb_boot.ld` states `_Min_Heap_Size = 0x200` and
  `_Min_Stack_Size = 0x400`.
- The xBuddy extension has only 4 032 bytes of RAM left after static data
  (87 % used).
- Not built: `xl-can`, `ix-modularbed`, `ix-xbuddy_extension`, the `anfc`
  presets, `*-indx_head`, `*-tool_offset_sensor`, `xl-htcontroller`,
  `xl_dev_kit`, `mk4-burst`, `xl-burst`, `xl-minimal`, `coreone_oak`,
  `coreonel_indx`, `mini_big_flash` in release. Several of them are built
  implicitly as sub-builds of the main-board presets above.

## Rebuild times and ccache

Measured on `coreone` (release, with bootloader) on an otherwise idle
machine, right after its full build:

| Scenario | Command | Wall time |
|---|---|---|
| Nothing changed | `python3 utils/build.py --preset coreone --build-type release --bootloader yes` | 7.3 s |
| Nothing changed, no bootstrap check | same with `--skip-bootstrap` | 5.9 s |
| One source touched | `touch src/common/marlin_server.cpp`, then the `--skip-bootstrap` command | **36.6 s** (17 ninja steps: 1 compile, the LTO link, `.bin`, translations and `.bbf` generation) |

Without `--skip-bootstrap`, every `utils/build.py` call runs the bootstrap
first, which re-runs `pip install -r requirements.txt` in `.venv`. That is
harmless for builds but resets packages that other tools changed (see
stage 4).

ccache: `cmake/CCache.cmake` uses ccache automatically when it is on the
`PATH` (separate cache namespace per printer, build type and bootloader
mode) and `-DNO_CCACHE=1` turns it off. ccache was **not installed** for
all measurements above (`-- CCache not found` in the configure output), so
none of those times benefit from a cache.

To see what it would bring, ccache 4.9.1 was installed afterwards
(`apt-get install -y ccache`) and `coreone` was built twice from an empty
build directory (`rm -rf build/coreone_release_boot` before each build,
`ccache -C` before the first):

| Clean build of `coreone` | Wall time | ccache statistics |
|---|---|---|
| without ccache (table at the top) | 337.2 s | n/a |
| ccache installed, empty cache | 387.5 s | 1604 misses, cache 0.3 GiB afterwards |
| ccache installed, warm cache | **64.7 s** | 1604 hits of 1604 |

So ccache costs about 15 % on the first build and makes a repeated clean
build of the same preset about 5 times faster. Because of the per-preset
namespace it does not share objects between presets (not measured).

## Script

`fw_size.py <arm-none-eabi-objdump> <build dir>...` prints one table row
per build directory:

```python
#!/usr/bin/env python3
"""Report flash and RAM use of Buddy firmware builds against the linker
memory regions.

Usage: fw_size.py <objdump> <build dir>...

<objdump> is arm-none-eabi-objdump from the bootstrap toolchain. Every build
directory must contain `firmware` (ELF) and `firmware.map`.
"""
import re
import subprocess
import sys
from pathlib import Path

SECTION = re.compile(
    r'^\s*\d+\s+(\S+)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+'
    r'[0-9a-f]{8}\s+2\*\*\d+\s*\n\s+([A-Z, _]+)$', re.MULTILINE)
REGION = re.compile(r'^(\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\S*$')
SYMBOL = re.compile(r'^\s+0x([0-9a-f]+)\s+(_Min_Heap_Size|_Min_Stack_Size|'
                    r'_estack)\s+=', re.MULTILINE)


def memory_regions(map_text):
    lines = map_text.split('\n')
    start = lines.index('Memory Configuration')
    regions = {}
    for line in lines[start + 3:]:
        match = REGION.match(line)
        if not match:
            break
        name, origin, length = match.groups()
        if name != '*default*':
            regions[name] = (int(origin, 16), int(length, 16))
    return regions


def sections(objdump, elf):
    out = subprocess.run([objdump, '-h', str(elf)],
                         check=True,
                         capture_output=True,
                         text=True).stdout
    for name, size, vma, lma, flags in SECTION.findall(out):
        flags = {flag.strip() for flag in flags.split(',')}
        if 'ALLOC' in flags and int(size, 16):
            yield name, int(size, 16), int(vma, 16), int(lma, 16), flags


def inside(address, region):
    origin, length = region
    return origin <= address < origin + length


def analyse(objdump, build_dir):
    map_text = (build_dir / 'firmware.map').read_text(errors='replace')
    regions = memory_regions(map_text)
    symbols = {name: int(value, 16) for value, name in SYMBOL.findall(map_text)}
    secs = list(sections(objdump, build_dir / 'firmware'))
    result = {'build': build_dir.name}

    flash_name = 'FLASH' if 'FLASH' in regions else next(
        name for name in regions if 'FLASH' in name.upper())
    flash = regions[flash_name]
    # Sub-board linker scripts pad the image up to the end of the region
    # with a `.flash_fill` section; that padding is free space.
    loaded = [
        s for s in secs if 'LOAD' in s[4] and inside(s[3], flash)
        and not s[0].endswith('_fill')
    ]
    flash_used = max(lma + size for _, size, _, lma, _ in loaded) - flash[0]
    result.update(flash_origin=flash[0],
                  flash_size=flash[1],
                  flash_used=flash_used,
                  flash_free=flash[1] - flash_used)

    ram = regions['RAM']
    in_ram = [s for s in secs if inside(s[2], ram)]
    reserved = sum(size for name, size, *_ in in_ram
                   if name == '._user_heap_stack')
    static = [s for s in in_ram if s[0] != '._user_heap_stack']
    static_end = max(vma + size for _, size, vma, _, _ in static)
    data = sum(size for name, size, *_ in static if name == '.data')
    bss = sum(size for name, size, *_ in static if name == '.bss')
    other = sum(size for name, size, *_ in static
                if name not in ('.data', '.bss'))
    result.update(ram_size=ram[1],
                  data=data,
                  bss=bss,
                  ram_other=other,
                  ram_other_names=','.join(
                      name for name, *_ in static
                      if name not in ('.data', '.bss')),
                  ram_free=ram[0] + ram[1] - static_end,
                  min_heap_stack=reserved)

    for name, region in regions.items():
        if name in (flash_name, 'RAM'):
            continue
        used = sum(size for _, size, vma, _, _ in secs if inside(vma, region))
        result[f'region_{name}'] = (used, region[1])
    result['symbols'] = symbols
    bin_file = build_dir / 'firmware.bin'
    bbf_file = build_dir / 'firmware.bbf'
    result['bin'] = bin_file.stat().st_size if bin_file.exists() else None
    result['bbf'] = bbf_file.stat().st_size if bbf_file.exists() else None
    return result


def main():
    objdump = sys.argv[1]
    print('| Build dir | Flash used / region (free) | `.data` + `.bss` / RAM '
          'region (left for heap + ISR stack) | Other regions used / size | '
          '`firmware.bin` | `firmware.bbf` |')
    print('|---|---|---|---|---|---|')
    for arg in sys.argv[2:]:
        r = analyse(objdump, Path(arg))
        others = ', '.join(f'{key[7:]} {value[0]} / {value[1]}'
                           for key, value in r.items()
                           if key.startswith('region_'))
        ram_other = (f' + {r["ram_other"]} ({r["ram_other_names"]})'
                     if r['ram_other'] else '')
        print(f'| `{r["build"]}` '
              f'| {r["flash_used"]} / {r["flash_size"]} '
              f'(**{r["flash_free"]}**, '
              f'{100 * r["flash_free"] / r["flash_size"]:.1f} %) '
              f'| {r["data"]} + {r["bss"]}{ram_other} / {r["ram_size"]} '
              f'({r["ram_free"]}) '
              f'| {others} | {r["bin"]} | {r["bbf"]} |')
        print(f'<!-- {r["build"]}: flash origin {r["flash_origin"]:#x}, '
              f'min heap+stack section {r["min_heap_stack"]}, '
              f'symbols {r["symbols"]} -->')


if __name__ == '__main__':
    main()
```
