#!/usr/bin/env python3
"""Generate the CORE One quick start printer profiles for PrusaSlicer.

Reads a PrusaResearch.ini vendor bundle, resolves the stock CORE One printer
presets with full inheritance and writes complete user printer presets that
carry the quick start start and end G-code. Nothing is downloaded; pass the
bundle as an argument.

    python3 -I make_quick_start_ini.py PrusaResearch.ini [-o DIR]
    python3 -I make_quick_start_ini.py PrusaResearch.ini --flags QUICKSTART_FW
    python3 -I make_quick_start_ini.py --print-start-gcode
"""

import argparse
import re
import sys
from pathlib import Path

PRESETS = ('Prusa CORE One 0.4 nozzle', 'Prusa CORE One HF0.4 nozzle')

FLAGS = ('QUICKSTART_FW', 'QUICKSTART_KEEP_WARM', 'QUICKSTART_Z_ALIGN')

MARKER = 'QUICKSTART'

# The G-code tests printer_notes with regexes like /.*QUICKSTART_FW.*/, so
# this line must not contain a flag name verbatim or it would enable it.
FLAGS_COMMENT = ('Optional quick start flags, each on its own line, written '
                 'without the space: QUICKSTART _FW, QUICKSTART _KEEP_WARM, '
                 'QUICKSTART _Z_ALIGN')

START_GCODE = """M17 ; enable steppers
M862.1 P[nozzle_diameter] A{(filament_abrasive[0] ? 1 : 0)} F{(nozzle_high_flow[0] ? 1 : 0)} ; nozzle check
M862.3 P "COREONE" ; printer model check
M862.5 P2 ; g-code level check
M862.6 P"Input shaper" ; FW feature check
{if printer_notes=~/.*QUICKSTART_FW.*/}M862.6 P"Quick start" ; FW feature check
{endif}M115 U6.4.0+11974

M555 X{(min(print_bed_max[0], first_layer_print_min[0] + 32) - 32)} Y{(max(0, first_layer_print_min[1]) - 4)} W{((min(print_bed_max[0], max(first_layer_print_min[0] + 32, first_layer_print_max[0])))) - ((min(print_bed_max[0], first_layer_print_min[0] + 32) - 32))} H{((first_layer_print_max[1])) - ((max(0, first_layer_print_min[1]) - 4))}

G90 ; use absolute coordinates
M83 ; extruder relative mode

{if chamber_minimal_temperature[initial_tool]!=0}
M140 S115 ; set bed temp for chamber heating
{else}
M140 S[first_layer_bed_temperature] ; set bed temp
{endif}

M109 R{((filament_notes[0]=~/.*MBL160.*/) ? 160 : (filament_notes[0]=~/.*HT_MBL10.*/ or filament_type[0] == "PLA" or filament_type[0] == "PETG") ? (first_layer_temperature[0] - 10) : (filament_type[0] == "PC" or filament_type[0] == "PA") ? (first_layer_temperature[0] - 25) : (filament_type[0]=~/(FLEX|TPU|TPE)/) ? 210 : 170)} F ; wait for temp, fan-assisted cooling
M107 ; fan off after fan-assisted cooling

M84 E ; turn off E motor

{if printer_notes=~/.*QUICKSTART_FW.*/}G28 Q ; home only if the pre-print prep left the position trusted{else}G28 ; home all without mesh bed level{endif}

{if chamber_minimal_temperature[initial_tool]!=0}
; Min chamber temp section
M104 S{idle_temperature[initial_tool]} ; set idle temp
G1 Z10 F720 ; set bed position
G1 X242 Y-9 F4800 ; set print head position
M191 S{chamber_minimal_temperature[initial_tool]} ; wait for minimal chamber temp
M141 S{chamber_temperature[initial_tool]} ; set nominal chamber temp
M107
M140 S[first_layer_bed_temperature] ; set bed temp
{else}
M141 S{chamber_temperature[initial_tool]} ; set nominal chamber temp
{endif}

G0 Z40 F10000
{if chamber_minimal_temperature[initial_tool]!=0 or first_layer_bed_temperature[initial_tool] > 60}
; soak expected: drop to idle temp while the bed and frame heat up
{if first_layer_bed_temperature[initial_tool]<=60}M106 S70{endif}
M104 T{initial_tool} S{if is_nil(idle_temperature[initial_tool])}100{else}{idle_temperature[initial_tool]}{endif}
M190 R[first_layer_bed_temperature] ; wait for bed temp
M107

{if printer_notes=~/.*QUICKSTART_FW.*/}
G29 G S{((filament_notes[0]=~/.*MBL160.*/) ? 160 : (filament_notes[0]=~/.*HT_MBL10.*/ or filament_type[0] == "PLA" or filament_type[0] == "PETG") ? (first_layer_temperature[0] - 10) : (filament_type[0] == "PC" or filament_type[0] == "PA") ? (first_layer_temperature[0] - 25) : (filament_type[0]=~/(FLEX|TPU|TPE)/) ? 210 : 170)} ; absorb heat, reheat nozzle near the end of the soak
{else}
G29 G ; absorb heat
{endif}

M109 R{((filament_notes[0]=~/.*MBL160.*/) ? 160 : (filament_notes[0]=~/.*HT_MBL10.*/ or filament_type[0] == "PLA" or filament_type[0] == "PETG") ? (first_layer_temperature[0] - 10) : (filament_type[0] == "PC" or filament_type[0] == "PA") ? (first_layer_temperature[0] - 25) : (filament_type[0]=~/(FLEX|TPU|TPE)/) ? 210 : 170)} ; wait for MBL temp
{else}
; no soak expected: keep the nozzle at probing temperature
M190 R[first_layer_bed_temperature] ; wait for bed temp

G29 G ; absorb heat
{endif}

M302 S155 ; lower cold extrusion limit to 155C

{if filament_type[initial_tool]=="FLEX"}
G1 E-4 F2400 ; retraction
{else}
G1 E-2 F2400 ; retraction
{endif}

M84 E ; turn off E motor

G29 P9 X208 Y-2.5 W32 H4

;
; MBL
;
M84 E ; turn off E motor
{if printer_notes=~/.*QUICKSTART_FW.*/}
G29 P7 ; verify the stored mesh at a few points, re-probe only on mismatch
{else}
G29 P1 ; invalidate mbl & probe print area
G29 P1 X150 Y0 W100 H20 C ; probe near purge place
{endif}
G29 P3.2 ; interpolate mbl probes
G29 P3.13 ; extrapolate mbl outside probe area
G29 A ; activate mbl

; prepare for purge
M104 S{first_layer_temperature[0]}
G0 X249 Y-2.5 Z15 F4800 ; move away and ready for the purge
M109 S{first_layer_temperature[0]}

G92 E0
M569 S0 E ; set spreadcycle mode for extruder

M591 S0 ; disable stuck detection

;
; Extrude purge line (half length)
;
G92 E0 ; reset extruder position
G1 E{(filament_type[0] == "FLEX" ? 4 : 2)} F2400 ; deretraction after the initial one
G0 E2.5 X242 Z0.2 F500 ; purge
G0 X237 E2 F500 ; purge
G0 X232 E2 F650 ; purge
G0 X227 E2 F800 ; purge
G0 X224 Z0.05 F8000 ; wipe, move close to the bed
G0 X221 Z0.2 F8000 ; wipe, move quickly away from the bed

M591 R ; restore stuck detection

G92 E0
M221 S100 ; set flow to 100%"""

END_GCODE = """{if layer_z < max_print_height}G1 Z{z_offset+min(max_layer_z+1, max_print_height)} F720 ; Move print head up{endif}
{if printer_notes=~/.*QUICKSTART_KEEP_WARM.*/}
M104 S{if is_nil(idle_temperature[initial_tool])}170{else}{idle_temperature[initial_tool]}{endif} ; keep nozzle warm for the next iteration (safety timer turns it off)
{else}
M104 S0 ; turn off temperature
{endif}
M140 S0 ; turn off heatbed
M141 S0 ; disable chamber control
M107 ; turn off fan
G1 X242 Y211 F10200 ; park
G4 ; wait
{if printer_notes=~/.*QUICKSTART_FW.*/ and printer_notes=~/.*QUICKSTART_Z_ALIGN.*/}
G162 Z R0 ; align Z at print end, stay low
{endif}
M572 S0 ; reset PA
M84 E ; turn off E motor; XY trust is left to the firmware
; max_layer_z = [max_layer_z]"""


def parse_ini(path):
    """Parse a PrusaSlicer ini file into {section: {key: raw value}}.

    Keys before the first section header land in section ''. Values are kept
    in their escaped form. configparser is not used because it mangles the
    vendor bundle (duplicate keys, '%' interpolation, case folding).
    """
    sections = {'': {}}
    current = sections['']
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\r\n')
            stripped = line.strip()
            if not stripped or stripped[0] in '#;':
                continue
            header = re.fullmatch(r'\[(.*)\]', stripped)
            if header:
                current = sections.setdefault(header.group(1), {})
                continue
            key, sep, value = line.partition('=')
            if sep:
                current[key.strip()] = value.strip()
    return sections


def resolve(sections, kind, name, chain=()):
    """Return the fully inherited key set of a bundle preset, without
    'inherits'. Multiple parents separated by ';' apply left to right."""
    section = f'{kind}:{name}'
    if section not in sections:
        raise KeyError(f'[{section}] not found in the bundle')
    if section in chain:
        raise ValueError(f'inheritance cycle through [{section}]')
    own = sections[section]
    out = {}
    for parent in own.get('inherits', '').split(';'):
        if parent.strip():
            out.update(
                resolve(sections, kind, parent.strip(), chain + (section, )))
    out.update((k, v) for k, v in own.items() if k != 'inherits')
    return out


ESCAPES = {'\\': '\\\\', '\r': '\\r', '\n': '\\n'}
UNESCAPES = {'n': '\n', 'r': '\r'}


def encode(text):
    """Escape a multi-line string the way PrusaSlicer serializes it."""
    return ''.join(ESCAPES.get(c, c) for c in text)


def decode(value):
    """Inverse of encode(). Like PrusaSlicer, any other escaped character
    stands for itself."""
    return re.sub(r'\\(.)', lambda m: UNESCAPES.get(m.group(1), m.group(1)),
                  value)


def with_stock_m115(start_gcode, stock):
    """Keep the firmware version the stock preset announces with M115 U."""
    match = re.search(r'M115 U\S+', decode(stock.get('start_gcode', '')))
    if not match:
        return start_gcode
    return re.sub(r'M115 U\S+', match.group(0), start_gcode, count=1)


def build(stock, parent, stem, flags):
    """Turn a resolved stock preset into the quick start user preset."""
    cfg = dict(stock)
    cfg['inherits'] = parent
    cfg['printer_settings_id'] = stem
    cfg['start_gcode'] = encode(with_stock_m115(START_GCODE, stock))
    cfg['end_gcode'] = encode(END_GCODE)
    notes = decode(stock.get('printer_notes', ''))
    cfg['printer_notes'] = encode('\n'.join([notes, MARKER, FLAGS_COMMENT] +
                                            list(flags)))
    return cfg


def write_preset(path, cfg, source):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'# generated by make_quick_start_ini.py from {source}\n')
        for key in sorted(cfg):
            # No trailing blank for empty values; the whitespace hook would
            # strip it anyway and PrusaSlicer trims values when loading.
            f.write(f'{key} = {cfg[key]}'.rstrip() + '\n')


def parse_flags(text):
    flags = [f.strip() for f in text.split(',') if f.strip()]
    unknown = [f for f in flags if f not in FLAGS]
    if unknown:
        raise argparse.ArgumentTypeError(
            f'unknown flag(s) {", ".join(unknown)}; known: {", ".join(FLAGS)}')
    return flags


def main():
    parser = argparse.ArgumentParser(
        description='Generate CORE One quick start PrusaSlicer printer '
        'presets from a PrusaResearch.ini vendor bundle.')
    parser.add_argument('bundle',
                        nargs='?',
                        type=Path,
                        help='path to PrusaResearch.ini')
    parser.add_argument('-o',
                        '--output-dir',
                        type=Path,
                        default=Path(__file__).resolve().parent,
                        help='where to write the .ini files '
                        '(default: the directory of this script)')
    parser.add_argument('--flags',
                        type=parse_flags,
                        default=[],
                        help='comma separated flags to enable for a test '
                        'variant; appended to printer_notes and to the file '
                        f'name. Known: {",".join(FLAGS)}')
    parser.add_argument('--print-start-gcode',
                        action='store_true',
                        help='print the start G-code and exit')
    parser.add_argument('--print-end-gcode',
                        action='store_true',
                        help='print the end G-code and exit')
    args = parser.parse_args()

    if args.print_start_gcode or args.print_end_gcode:
        if args.print_start_gcode:
            print(START_GCODE)
        if args.print_end_gcode:
            print(END_GCODE)
        return 0
    if args.bundle is None:
        parser.error('the bundle argument is required')

    sections = parse_ini(args.bundle)
    version = sections.get('vendor', {}).get('config_version', 'unknown')
    source = f'{args.bundle.name} config_version {version}'
    for name in PRESETS:
        stem = f'{name} - quick start'
        if args.flags:
            stem += f' - {",".join(args.flags)}'
        cfg = build(resolve(sections, 'printer', name), name, stem, args.flags)
        path = args.output_dir / f'{stem}.ini'
        write_preset(path, cfg, source)
        print(f'wrote {path} ({len(cfg)} keys)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
