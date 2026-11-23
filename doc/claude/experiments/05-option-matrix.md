# Stage 5: option matrix

Every firmware build generates one header per build option under
`<build dir>/include/option/<name>.h`. Each header defines the macro
`<NAME>()` (and `option::<name>` for C++); the value is fixed at configure
time from the preset and the CMake logic. This note tabulates those values
for the builds of stage 3 (commit `b1043d68b`).

- 157 options exist, the same set in every build directory, including the
  sub-board builds.
- [`05-option-matrix.csv`](05-option-matrix.csv) has one row per option and
  one column for each of 21 builds: the 8 main-board presets, 3 sub-board
  presets, `mk4` without bootloader, `mini_big_flash` in debug and the 8
  `mini-en-*` language presets. All are `release` with `--bootloader yes`
  unless the column name says otherwise.
- The tables below show the first 11 columns (main boards and sub-boards).
  Among the 8 main boards, 120 of the 157 options differ; 27 options have
  the same value in all 11 builds.
- Values are `0`/`1` except for five multi-valued options, which are shown
  by their symbolic name: `mcu`, `filament_sensor`, `power_panic_storage`,
  `xbuddy_extension_variant`, `tool_offset_sensor_geometry`.
- In the sub-board columns many `has_*` options are 1 because the header
  set is generated for the printer the sub-board belongs to; read them as
  "configured for this preset", not as "implemented in that sub-board
  firmware".

## How the remaining columns differ from their base preset

Computed from the CSV:

| Build | Compared with | Options that differ |
|---|---|---|
| `mk4`, `--bootloader no` | `mk4` | `bootloader` 1→0, `bootloader_update` 1→0 |
| `mini-en-<lang>` | `mini` | only `enable_translation_<lang>` 0→1 (one language per preset) |
| `mini_big_flash`, debug | `mini` | `mcu` STM32F407VG→STM32F429VI, `allow_missing_otp` 0→1, `disable_watchdog` 0→1, `sysdebug` 0→1, `has_extra_experimental_settings` 0→1, `networking_benchmark_enabled` 0→1, `rtt_metrics_enabled` 0→1 |

Differences between some neighbouring main-board presets, for orientation:
`coreone` and `coreonel` differ in 9 options, `mk4` and `mk3.5` in 17,
`coreone` and `coreone_indx` in 35.

## Options that differ between these builds (130 of 157)

| Option | `mini` | `mk3.5` | `mk4` | `xl` | `ix` | `coreone` | `coreonel` | `coreone_indx` | `xl-dwarf` | `xl-modularbed` | `coreone-xbuddy_extension` |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `board_is_master_board` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `bootloader_update` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 1 |
| `buddy_enable_connect` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `enable_translation_cs` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `enable_translation_de` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `enable_translation_es` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `enable_translation_fr` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `enable_translation_it` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `enable_translation_ja` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `enable_translation_pl` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `enable_translation_uk` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `filament_sensor` | BINARY | BINARY | ADC | ADC | ADC | ADC | ADC | NO | NO | NO | NO |
| `has_15gt_belts` | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_ac_controller` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `has_accelerometer` | 0 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 |
| `has_adc_side_fsensor` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | 1 | 0 |
| `has_advanced_power` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 |
| `has_anfc` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 1 |
| `has_attachable_accelerometer` | 0 | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 |
| `has_auto_retract` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_bed_fan` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `has_bed_level_correction` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_bowden` | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_ceiling_clearance` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_chamber_api` | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_chamber_filtration_api` | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_chamber_vents` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_coldpull` | 0 | 1 | 1 | 1 | 0 | 1 | 1 | 0 | 1 | 1 | 1 |
| `has_cpu_fan` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_crash_detection` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_cyphal` | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_door_sensor` | 0 | 0 | 1 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_door_sensor_calibration` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_dwarf` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_e2ee_support` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_embedded_esp32` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | 1 | 0 |
| `has_emergency_stop` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_esp` | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_expansion_joints_gen_2` | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 1 |
| `has_extruder_fsensor` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 | 0 |
| `has_filament_base_preset_param` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_filament_heatbreak_param` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_filament_sensors_menu` | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_filament_tracker` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_gantry_squareness_check` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_gcode_compatibility` | 0 | 1 | 1 | 1 | 0 | 1 | 1 | 0 | 1 | 1 | 1 |
| `has_gearbox_alignment` | 0 | 0 | 1 | 1 | 0 | 1 | 1 | 0 | 0 | 0 | 0 |
| `has_gui` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_heatbed_screws_during_transport` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `has_heatbreak_temp` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 | 0 |
| `has_heaters_selftest_bed_sheet_retry` | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_heaters_selftest_gcode` | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_heaters_selftest_revise` | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_hotend_type_support` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 | 0 |
| `has_ht_hotend` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 1 |
| `has_htcontroller` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | 1 | 0 |
| `has_human_interactions` | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_i2c_expander` | 0 | 1 | 1 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_indx` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_indx_head` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_input_shaper_calibration` | 0 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_internal_storage_flash` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_leds` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_lights_menu` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_loadcell` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_loadcell_hx717` | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 0 | 1 | 0 | 0 |
| `has_local_accelerometer` | 0 | 1 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 0 | 1 |
| `has_local_bed` | 1 | 1 | 1 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 |
| `has_love_board` | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 0 | 1 |
| `has_manual_belt_tuning` | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_mmu2` | 0 | 1 | 1 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 1 |
| `has_mmu2_over_uart` | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_modular_bed` | 0 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_motor_current_profiles` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_nextruder` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 | 0 |
| `has_nfc` | 0 | 1 | 1 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_nozzle_cleaner` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_nozzle_cleaner_lite` | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 0 | 1 | 1 | 1 |
| `has_nozzle_thermal_compensation` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_pause` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_per_tool_temperatures` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_phase_stepping` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_phase_stepping_calibration` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_phase_stepping_selftest` | 0 | 0 | 0 | 1 | 1 | 0 | 1 | 1 | 0 | 0 | 0 |
| `has_planner` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_power_panic` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_precise_homing` | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_precise_homing_corexy` | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_print_fan_type` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_print_sheet_detection` | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_psu_fan` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `has_puppies` | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_puppies_bootloader` | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_puppy_bootstrap` | 0 | 0 | 0 | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_puppy_modularbed` | 0 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_remote_accelerometer` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 1 | 1 | 0 |
| `has_remote_bed` | 0 | 0 | 0 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| `has_selftest_dependencies` | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_sheet_profiles` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_sheet_support` | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_side_fsensor` | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_side_fsensor_invertible` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_side_fsensor_remap` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_side_leds` | 0 | 0 | 0 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_spool_join` | 0 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_switchable_auto_retract` | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 1 |
| `has_switchable_homing_calibration` | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 1 |
| `has_switched_fan_test` | 0 | 1 | 1 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 1 |
| `has_tmc_uart` | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_tool_crash_recovery` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_tool_mapping` | 0 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_tool_offset_pin_calibration` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_tool_offset_sensor` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 1 | 1 | 0 |
| `has_toolchanger` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_touch` | 0 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_translations` | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `has_uneven_bed_detection` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 0 | 0 | 1 |
| `has_usb_device` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_wastebin` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_wastebin_fill_tracking` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `has_xbuddy_extension` | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `has_xl_can` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `has_xlcd` | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `mcu` | STM32F407VG | STM32F427ZI | STM32F427ZI | STM32F427ZI | STM32F427ZI | STM32F427ZI | STM32F427ZI | STM32F427ZI | STM32G070RBT6 | STM32G070RBT6 | STM32H503CBU7 |
| `puppy_flash_fw` | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `resources` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |
| `tool_offset_sensor_geometry` | NONE | NONE | NONE | DUAL_COIL | NONE | NONE | NONE | SINGLE_COIL | DUAL_COIL | DUAL_COIL | NONE |
| `websocket` | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| `xbuddy_extension_variant` | NONE | NONE | NONE | NONE | iX | STANDARD | STANDARD | STANDARD | NONE | NONE | NONE |
| `xl_enclosure_support` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Options with the same value in all of these builds (27)

| Option | Value |
|---|---|
| `allow_missing_otp` | 0 |
| `bootloader` | 1 |
| `buddy_enable_wui` | 1 |
| `cyphal_can_stats` | 0 |
| `debug_with_beeps` | 0 |
| `developer_mode` | 0 |
| `development_items` | 1 |
| `disable_watchdog` | 0 |
| `has_anfc_for_multiple_tools` | 0 |
| `has_burst_stepping` | 0 |
| `has_cancel_object` | 1 |
| `has_config_store_wo_backend` | 0 |
| `has_cyphal_logging` | 0 |
| `has_cyphal_metrics` | 0 |
| `has_cyphal_timesync` | 0 |
| `has_extra_experimental_settings` | 0 |
| `has_fsensor_invertible` | 0 |
| `has_selftest` | 1 |
| `has_serial_print` | 1 |
| `has_trinamic` | 1 |
| `heap_instrumentation_enabled` | 0 |
| `mdns` | 1 |
| `networking_benchmark_enabled` | 0 |
| `power_panic_storage` | FLASH |
| `rtt_metrics_enabled` | 0 |
| `signature_oak` | 0 |
| `sysdebug` | 0 |

## How it was generated

```bash
python3 option_matrix.py doc/claude/experiments/05-option-matrix.csv tables.md 11 \
    mini=build/mini_release_boot mk3.5=build/mk3.5_release_boot \
    mk4=build/mk4_release_boot xl=build/xl_release_boot \
    ix=build/ix_release_boot coreone=build/coreone_release_boot \
    coreonel=build/coreonel_release_boot \
    coreone_indx=build/coreone_indx_release_boot \
    xl-dwarf=build/xl-dwarf_release_boot \
    xl-modularbed=build/xl-modularbed_release_boot \
    coreone-xbuddy_extension=build/coreone-xbuddy_extension_release_boot \
    mk4-noboot=build/mk4_release_noboot \
    mini_big_flash-debug=build/mini_big_flash_debug_boot \
    mini-en-cs=build/mini-en-cs_release_boot \
    mini-en-de=build/mini-en-de_release_boot \
    mini-en-es=build/mini-en-es_release_boot \
    mini-en-fr=build/mini-en-fr_release_boot \
    mini-en-it=build/mini-en-it_release_boot \
    mini-en-ja=build/mini-en-ja_release_boot \
    mini-en-pl=build/mini-en-pl_release_boot \
    mini-en-uk=build/mini-en-uk_release_boot
```

The two tables above are `tables.md` as written by the script.
`option_matrix.py`:

```python
#!/usr/bin/env python3
"""Build a matrix of generated option values from firmware build directories.

Usage: option_matrix.py <output.csv> <output.md> <md columns>
                        <label>=<build dir>...

Every `<build dir>/include/option/<name>.h` defines one option as the macro
`<NAME>()`. Multi-valued options also define `<NAME>_<SYMBOL> <number>`
constants; those are mapped back to the symbol. All builds go to the CSV,
the first <md columns> of them to the Markdown tables.
"""
import csv
import re
import sys
from pathlib import Path

DEFINE = re.compile(r'^#define\s+(\w+)(\(\))?\s+(.+?)\s*$', re.MULTILINE)


def read_option(header: Path):
    macro = header.stem.upper()
    value = None
    symbols = {}
    for name, call, definition in DEFINE.findall(header.read_text()):
        if name == macro and call:
            value = definition
        elif name.startswith(macro + '_') and not call:
            symbols[definition] = name[len(macro) + 1:]
    if value is None:
        raise ValueError(f'{header}: no {macro}() macro')
    return symbols.get(value, value) if symbols else value


def table(f, labels, rows):
    f.write('| Option | ' + ' | '.join(f'`{label}`' for label in labels) +
            ' |\n')
    f.write('|---|' + '---|' * len(labels) + '\n')
    for row in rows:
        f.write(f'| `{row[0]}` | ' + ' | '.join(row[1:]) + ' |\n')


def main():
    csv_path, md_path, md_columns, *builds = sys.argv[1:]
    md_columns = int(md_columns)
    labels = []
    values = {}
    for build in builds:
        label, directory = build.split('=', 1)
        labels.append(label)
        for header in sorted(Path(directory, 'include', 'option').glob('*.h')):
            values.setdefault(header.stem, {})[label] = read_option(header)

    rows = [[option] + [values[option].get(label, '') for label in labels]
            for option in sorted(values)]
    with open(csv_path, 'w', newline='') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(['option'] + labels)
        writer.writerows(rows)

    shown = [row[:md_columns + 1] for row in rows]
    differing = [row for row in shown if len(set(row[1:])) > 1]
    same = [row for row in shown if len(set(row[1:])) == 1]
    with open(md_path, 'w') as f:
        f.write(f'## Options that differ between these builds '
                f'({len(differing)} of {len(shown)})\n\n')
        table(f, labels[:md_columns], differing)
        f.write(f'\n## Options with the same value in all of these builds '
                f'({len(same)})\n\n')
        f.write('| Option | Value |\n|---|---|\n')
        for row in same:
            f.write(f'| `{row[0]}` | {row[1]} |\n')


if __name__ == '__main__':
    main()
```
