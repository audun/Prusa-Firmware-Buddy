# Stage 2: host unit tests

Followed `tests/unit/README.md`. All commands were run from the repository
root with the bootstrap venv active (`. .venv/bin/activate`), at `b1043d68b`.

**Verdict:** everything builds and passes on Linux. 859 of 859 ctest tests
pass, no flaky test was seen. A cold build takes 3 min 41 s on 4 cores, the
whole suite runs in 67 s (11 s without the three `slow` tests), and the
edit loop for one test executable is about 2 s.

## Prerequisites found

| Requirement | Detail |
|---|---|
| Bootstrap venv | Without it: `python3 utils/build_tests.py --list` → `ModuleNotFoundError: No module named 'tree_sitter_cpp'`. Activate the venv or call `.venv/bin/python utils/build_tests.py`. |
| Host compiler | GCC 13.3.0 (`/usr/bin/cc`, `/usr/bin/c++`), chosen by CMake's default search. Flags seen in `build/tests/build.ninja`: `-std=gnu++23 -Wall ... -g -Os -ffunction-sections -fdata-sections -frtti`, `-D_GLIBCXX_ASSERTIONS -DUNITTESTS`. clang was not tried. |
| cmake / ninja | The script prefers `.dependencies/cmake-3.28.3` and `.dependencies/ninja-1.10.2` over system tools. |
| OS package `gettext` | Needed for `msgfmt` (the translator tests compile `.po` files). It was the only package installed: `apt-get install -y gettext`. Without it (checked by removing the package and rebuilding `translator_tests`) the build stops with the error below. |
| ccache | Looked for by CMake (`-- CCache not found`), not installed, not used. |

```
FAILED: tests/unit/lang/translator/MO/fr.mo build/tests/tests/unit/lang/translator/MO/fr.mo
cd build/tests/tests/unit/lang/translator && msgfmt src/lang/po/fr/Prusa-Firmware-Buddy_fr.po -o build/tests/tests/unit/lang/translator/MO/fr.mo
/bin/sh: 1: msgfmt: not found
ninja: build stopped: subcommand failed.
```

## Build and run: commands and timings

| # | Command | Result | Wall time |
|---|---|---|---|
| 1 | `python3 utils/build_tests.py --list` | configures `build/tests` (board `BUDDY`, default build type) and lists **137 targets** | 6.1 s |
| 2 | `python3 utils/build_tests.py` | cold build of everything, 1840 ninja steps, 39 compiler warnings, exit 0 | **221.2 s** |
| 3 | `python3 utils/build_tests.py --run -- --output-on-failure` (the CI command) | first re-ran CMake and 25 steps (see note), then ctest: **859/859 passed** | 90.0 s (ctest 67.6 s) |
| 4 | `python3 utils/build_tests.py` (nothing changed), twice | `ninja: no work to do.` | 0.3 s |
| 5 | `python3 utils/build_tests.py -t -- -LE slow` | 856/856 passed | **10.8 s** |
| 6 | `python3 utils/build_tests.py -t` | 859/859 passed | 66.7 s |
| 7 | `cd build/tests && ctest -j4` | 859/859 passed | 32.0 s |
| 8 | `cd build/tests && ctest -LE slow --repeat until-fail:5 -j4` | 856/856 passed 5 times each | 14.1 s |
| 9 | `cd build/tests && ctest -L slow` | 3/3 passed | 56.4 s |

Rows 7 to 9 used `.dependencies/cmake-3.28.3/bin/ctest`.

Numbers:

- **Test targets:** 137 are listed by `--list` (members of the `tests` ninja
  target). 130 of them are Catch2 test executables registered with ctest;
  the other 7 are manual programs under `tests/manual/` (`signal2step_test`,
  `sp_agc_test`, `sp_lowpass_test`, `sp_noise_test`,
  `sp_signal_generator_test`, `sp_transfer_test`, `sp_windows_test`) that
  are built but never run by ctest.
- **ctest tests:** 859. Each Catch2 `TEST_CASE` is one ctest test
  (`catch_discover_tests(... ADD_TAGS_AS_LABELS)`), so ctest names are test
  case names and ctest labels are Catch2 tags.
- **Build directory size:** 2.1 GB (`du -sh build/tests`).
- **Slow tests** (label `slow`, 56 s of the 67 s):

| Executable | Test case | Time |
|---|---|---|
| `gcode_info_test` | `GCodeInfo::is_valid_for_print` | 31.5 s |
| `gcode_reader_test` | `stream restore at offset` | 19.5 s |
| `translator_tests` | `providerFILE::Translations test` | 5.7 s |

  The next slowest test takes 2.1 s (`machine_pos_conv`), everything else is
  under 1 s.

Note on row 3: the first build invocation after a cold build is not a no-op.
Ninja reports `GLOB mismatch!`, re-runs CMake and rebuilds 25 steps around
`autogen_include/openprinttag/autogen/fields.*` (about 22 s). After that,
rebuilds without changes take 0.3 s.

## Failing or flaky tests

None. The full suite was run 3 times (rows 3, 6, 7), the slow tests a fourth
time (row 9) and the fast tests 5 more times in parallel (row 8); every run
was 100 % green. Three tests carry Catch2's `[!mayfail]` / `[!shouldfail]`
tags (in `tests/unit/gui/lazyfilelist/tests.cpp` and
`tests/unit/lang/text_fit/*_display_tests.cpp`), so ctest would stay green
if those assertions failed.

## Edit loop

| Scenario | Commands | Time |
|---|---|---|
| A. Touch a test source | `touch tests/unit/common/ring_allocator/ring_allocator_test.cpp`<br>`python3 utils/build_tests.py ring_allocator_tests` | 2.17 s (1 compile + 1 link) |
| | `(cd build/tests/tests/unit/common/ring_allocator && ctest)` | 0.02 s, 3 tests |
| | or `build/tests/tests/unit/common/ring_allocator/ring_allocator_tests` | 0.01 s, "All tests passed (329200 assertions in 3 test cases)" |
| B. Touch a production source used by one test | `touch lib/Marlin/Marlin/src/feature/phase_stepping/lut.cpp`<br>`python3 utils/build_tests.py Marlin_phase_stepping_tests` | 1.01 s |
| | `(cd build/tests/tests/unit/lib/Marlin/phase_stepping && ctest)` | 0.07 s, 2 tests |
| C. After B, rebuild all tests | `python3 utils/build_tests.py` | 0.28 s (nothing else used that file) |
| D. All fast tests of one area | `(cd build/tests/tests/unit/common/gcode && ctest -LE slow)` | 4.0 s, 59 tests |
| E. Name or label filter | `(cd build/tests && ctest -R gcode -LE slow)` | 0.8 s, 38 tests |
| | `(cd build/tests && ctest -L translator -LE slow)` | 0.04 s, 1 test |

How to select "the tests of one area" reliably:

1. `ctest -R <regex>` matches **test case names**, not directories or
   executables. `-R gcode` ran 38 tests while the `common/gcode` directory
   holds 59 fast ones, so `-R` both misses tests and picks up tests from
   other directories.
2. Running `ctest` **inside the matching build subdirectory**
   (`build/tests/tests/unit/<area>/...`) runs exactly the tests defined in
   that directory and below (scenario D).
3. Running the test executable directly gives Catch2's own output and
   filters (`<executable> "<test case name>"`, `<executable> "[tag]"`).
4. The executable name is the build target name, so
   `python3 utils/build_tests.py <target> [<target> ...]` rebuilds just the
   ones needed.

## Test directories

Summary per top-level directory under `tests/unit/`. "Covers" is read off
the executable names only.

| Area | Executables | ctest tests | Covers (from the names) |
|---|---|---|---|
| (root) | 1 | 1 | `system_tests` (one test case) |
| `common` | 40 | 260 | utility code from `src/common`: buffers, bitsets, JSON, string and path helpers, OTP, hotend detection, automata, end-to-end-encryption decryptor, filament sensor ADC, FreeRTOS queue, G-code parser/reader/info and MeatPack, HTTP, power panic storage, probe analysis, ring allocator, ST25DV64K EEPROM chip, printer model, progress mapping |
| `connect` | 3 | 80 | Prusa Connect client: planner, registration, protocol |
| `feature` | 13 | 41 | individual printer features: cork G-code, filament tracker, G-code exceptions, INDX hotend temperature model, nozzle cleaner, nozzle thermal compensation, OpenPrintTag data/requests/filament usage, ramming, tool index iterator, xBuddy extension |
| `gui` | 11 | 54 | GUI building blocks: colours, rectangles, text input layouts, jogwheel, lazy file list, neopixel, window and layout classes, file list window |
| `hw` | 1 | 1 | XL modular bed fan |
| `lang` | 6 | 18 | translations: language in EEPROM, "print will end" formatting, UTF-8 string views, text fitting on MINI and large displays, translator against the `.po` files |
| `lib` | 8 | 104 | vendored/shared libraries: Marlin pieces (E-stall detector, MMU2 protocol, phase stepping, signal-to-step, time ticks), WUI (HTTP server `nhttp`, lwIP out-of-sequence limit), libsysbase paths |
| `logging` | 2 | 6 | logging framework incl. compile-time filter |
| `marlin` | 3 | 11 | machine position conversion, temperature tables, vector type |
| `media_prefetch` | 1 | 11 | G-code media prefetch buffer |
| `mmu2` | 1 | 2 | MMU2 integration |
| `mmu2-modbus` | 1 | 4 | MMU2 over Modbus |
| `module` | 30 | 195 | reusable modules: base64/COBS coding, FIFO coder, ISO 13239 CRC, Modbus, NFC (NFC-V codec, field manager, OpenPrintTag reader, ST25R39xxB driver), 1-Wire master, OTP, signal processing, TPIS, small utilities (queues, debouncers, rate limiters, timers, ...) |
| `persistent_stores` | 1 | 12 | EEPROM/config store |
| `puppies` | 2 | 11 | main-board side of sub-board communication: time sync, minimal Cyphal node |
| `puppy` | 4 | 33 | sub-board firmware code: Dwarf loadcell deglitcher, xBuddy extension Cyphal application, NFC node, circular buffer |
| `resources` | 1 | 5 | resource tarball handling |
| `transfers` | 1 | 10 | file transfers (download/upload) |
| **total** | **130** | **859** | |

Detail per subdirectory (generated, numbers in parentheses are ctest tests
per executable):

| Directory under `tests/unit/` | Executables | ctest tests | Test executables (ctest tests each) |
|---|---|---|---|
| `.` | 1 | 1 | `system_tests` (1) |
| `common` | 20 | 121 | `algorithm_scale_tests` (1), `circle_buffer_tests` (3), `circular_buffer_tests` (1), `encoded_bitset_tests` (16), `encoded_filament_tests` (6), `exponential_backoff_tests` (1), `filename_tests` (6), `hotend_detect_tests` (26), `json_tests` (21), `lfn_tests` (6), `median_tests` (1), `otp_tests` (9), `path_utils_tests` (6), `percent_encode_tests` (1), `str_utils_tests` (9), `string_builder_tests` (1), `support_utils_tests` (3), `timing_tests` (1), `url_decode_tests` (1), `weighted_modus_tests` (2) |
| `common/automata` | 1 | 29 | `automata_tests` (29) |
| `common/e2ee` | 1 | 11 | `decryptor_tests` (11) |
| `common/filament_sensor` | 1 | 4 | `filament_sensor_adc_tests` (4) |
| `common/freertos` | 1 | 1 | `freertos_queue_test` (1) |
| `common/gcode` | 6 | 61 | `gcode_basic_parser_tests` (1), `gcode_info_test` (3), `gcode_parser_custom_types_tests` (3), `gcode_parser_tests` (3), `gcode_reader_test` (44), `meatpack_test` (7) |
| `common/http` | 1 | 6 | `common_http_tests` (6) |
| `common/power_panic` | 1 | 1 | `power_panic_storage_tests` (1) |
| `common/probe_analysis` | 2 | 8 | `position_lookback` (4), `probe_analysis_tests` (4) |
| `common/ring_allocator` | 1 | 3 | `ring_allocator_tests` (3) |
| `common/st25dv64k` | 1 | 1 | `st25dv64k_tests` (1) |
| `common/utils` | 4 | 14 | `center_approx` (2), `dynamic_index_mapping` (1), `printer_model_tests` (2), `progress_mapper_tests` (9) |
| `connect` | 3 | 80 | `connect_planner_tests` (24), `connect_registration_tests` (1), `connect_tests` (55) |
| `feature/cork` | 1 | 1 | `cork_gcode` (1) |
| `feature/filament_tracker` | 1 | 2 | `filament_tracker_tests` (2) |
| `feature/gcode_exception` | 1 | 6 | `gcode_exception_tests` (6) |
| `feature/indx_hotend_temp_model` | 2 | 5 | `indx_hotend_temp_compensation_tests` (1), `indx_hotend_thermal_model_tests` (4) |
| `feature/nozzle_cleaner_lite` | 1 | 11 | `cleaner_zigzag_tests` (11) |
| `feature/nozzle_thermal_compensation` | 1 | 3 | `nozzle_thermal_compensation_tests` (3) |
| `feature/openprinttag` | 3 | 8 | `buddy_openprinttag_datautils_tests` (4), `buddy_openprinttag_multirequest_tests` (3), `filament_usage_tracker_tests` (1) |
| `feature/ramming` | 1 | 1 | `ramming_sequence_tests` (1) |
| `feature/tool_index` | 1 | 3 | `tool_index_iterator_tests` (3) |
| `feature/xbuddy_extension` | 1 | 1 | `xbuddy_extension_tests` (1) |
| `gui` | 5 | 24 | `color_tests` (1), `gui_tests` (1), `mini_text_input_layout_tests` (1), `rectangle_tests` (20), `std_text_input_layout_tests` (1) |
| `gui/jogwheel` | 1 | 1 | `jogwheel_tests` (1) |
| `gui/lazyfilelist` | 1 | 9 | `lazyfilelist_tests` (9) |
| `gui/neopixel` | 1 | 1 | `neopixel_tests` (1) |
| `gui/window` | 2 | 14 | `layout_tests` (9), `window_tests` (5) |
| `gui/window_file_list` | 1 | 5 | `window_file_list_tests` (5) |
| `hw/xl` | 1 | 1 | `modular_bed_fan_tests` (1) |
| `lang/eeprom` | 1 | 1 | `language_eeprom_tests` (1) |
| `lang/format_print_will_end` | 1 | 3 | `format_print_will_end_tests` (3) |
| `lang/string_view_utf8` | 1 | 10 | `string_view_utf8_tests` (10) |
| `lang/text_fit` | 2 | 2 | `text_fit_tests_large` (1), `text_fit_tests_mini` (1) |
| `lang/translator` | 1 | 2 | `translator_tests` (2) |
| `lib/Marlin` | 5 | 76 | `Marlin_EStallDetector_tests` (9), `Marlin_MMU2_tests` (34), `Marlin_phase_stepping_tests` (2), `signal2step_tests` (18), `time_ticks_tests` (13) |
| `lib/WUI` | 2 | 27 | `nhttp_tests` (21), `ooseq_limit_tests` (6) |
| `lib/libsysbase` | 1 | 1 | `path_tests` (1) |
| `logging` | 2 | 6 | `logging_tests` (5), `logging_tests_compile_time_filter` (1) |
| `marlin` | 3 | 11 | `machine_pos_conv_tests` (1), `marlin_temptable_tests` (3), `vector_tests` (7) |
| `media_prefetch` | 1 | 11 | `media_prefetch_tests` (11) |
| `mmu2` | 1 | 2 | `mmu_tests` (2) |
| `mmu2-modbus` | 1 | 4 | `mmu2-modbus_tests` (4) |
| `module/coding` | 2 | 19 | `base64_tests` (9), `cobs_tests` (10) |
| `module/fifo_coder` | 1 | 7 | `fifo_coder_tests` (7) |
| `module/iso13239` | 1 | 1 | `iso13239_crc_property_tests` (1) |
| `module/modbus` | 1 | 8 | `modbus_tests` (8) |
| `module/nfc` | 9 | 53 | `nfcv_encode_decode_tests` (17), `nfcv_field_manager_tests` (5), `openprinttag_nfcv_tests` (12), `openprinttag_reader_tests` (7), `openprinttag_stub_tests` (1), `test_st25r39xxb_field_up` (2), `test_st25r39xxb_interrupts` (4), `test_st25r39xxb_response_retry` (4), `test_st25r39xxb_spi_interface` (1) |
| `module/onewire` | 1 | 3 | `onewire_master_tests` (3) |
| `module/otp` | 1 | 13 | `otp_module_tests` (13) |
| `module/signal_processing` | 1 | 58 | `signal_processing_tests` (58) |
| `module/tpis` | 1 | 2 | `tpis_tests` (2) |
| `module/utils` | 12 | 31 | `atomic_circular_queue_test` (2), `bitset_utils_tests` (2), `combo_counter_tests` (1), `compact_optional_tests` (2), `debouncer_tests` (11), `latching_debouncer_tests` (1), `leaky_bucket_tests` (1), `rate_limiter_tests` (3), `single_linked_list_tests` (4), `timer_event_period_tracker_tests` (2), `timer_tests` (1), `value_pack_tests` (1) |
| `persistent_stores` | 1 | 12 | `eeprom_unit_tests` (12) |
| `puppies` | 1 | 10 | `time_sync` (10) |
| `puppies/minimal_cyphal_node` | 1 | 1 | `minimal_cyphal_node_test` (1) |
| `puppy/dwarf` | 1 | 7 | `loadcell_deglitcher_tests` (7) |
| `puppy/xbuddy_extension` | 3 | 26 | `cyphal_nfc_node_tests` (7), `xbuddy_extension_cyphal_application_tests` (14), `xbuddy_extension_yet_another_circular_buffer_tests` (5) |
| `resources` | 1 | 5 | `tarball_tests` (5) |
| `transfers` | 1 | 10 | `transfers_tests` (10) |
| **total** | **130** | **859** | |

The detail table was generated with this script
(`(cd build/tests && ctest --show-only=json-v1) | python3 unit_test_table.py`):

```python
#!/usr/bin/env python3
"""Summarize the host unit tests per directory from ctest's JSON listing.

Usage: (cd build/tests && ctest --show-only=json-v1) | unit_test_table.py
"""
import collections
import json
import sys

data = json.load(sys.stdin)
per_dir = collections.defaultdict(lambda: collections.defaultdict(int))
slow = []
for test in data['tests']:
    command = test.get('command')
    if not command:
        continue
    executable = command[0]
    relative = executable.split('/build/tests/', 1)[1]
    parts = relative.split('/')
    target = parts[-1]
    # tests/unit/<area>/<subdir>/... -> area/subdir
    directory = '/'.join(parts[2:-1][:2]) or '.'
    per_dir[directory][target] += 1
    labels = []
    for prop in test.get('properties', []):
        if prop['name'] == 'LABELS':
            labels = prop['value']
    if 'slow' in labels:
        slow.append((target, test['name']))

total_targets = sum(len(t) for t in per_dir.values())
total_tests = sum(sum(t.values()) for t in per_dir.values())
print(f'| Directory under `tests/unit/` | Executables | ctest tests | Test executables (ctest tests each) |')
print('|---|---|---|---|')
for directory in sorted(per_dir):
    targets = per_dir[directory]
    names = ', '.join(f'`{name}` ({count})'
                      for name, count in sorted(targets.items()))
    print(f'| `{directory}` | {len(targets)} | {sum(targets.values())} | {names} |')
print(f'| **total** | **{total_targets}** | **{total_tests}** | |')
print()
for target, name in slow:
    print(f'slow: {target}: {name}')
```

## Not run

- Coverage build (`python3 utils/build_tests.py --coverage`).
- `--debug` and `--release` test build types.
- Building the tests with clang.
