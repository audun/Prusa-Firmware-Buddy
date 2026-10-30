#include <catch2/catch_test_macros.hpp>

#include <filament.hpp>
#include <option/has_loadcell.h>
#include <option/has_indx.h>

TEST_CASE("filament_presets: PLA/PETG preheat matches the HT_MBL10 probing temperature", "[filament]") {
    const auto &pla = preset_filament_parameters[PresetFilamentType::PLA];
    const auto &petg = preset_filament_parameters[PresetFilamentType::PETG];

    CHECK(pla.nozzle_temperature == 215);
    CHECK(petg.nozzle_temperature == 230);

    if constexpr (!HAS_LOADCELL()) {
        CHECK(pla.nozzle_preheat_temperature == 170);
        CHECK(petg.nozzle_preheat_temperature == 170);
    } else if constexpr (HAS_INDX()) {
        // INDX tool-offset calibration probes Z at the preheat temperature
        CHECK(pla.nozzle_preheat_temperature == 170);
        CHECK(petg.nozzle_preheat_temperature == 170);
    } else {
        CHECK(pla.nozzle_preheat_temperature == pla.nozzle_temperature - 10);
        CHECK(petg.nozzle_preheat_temperature == petg.nozzle_temperature - 10);
    }
}

TEST_CASE("filament_presets: preheat never exceeds the print temperature", "[filament]") {
    for (size_t i = 0; i < static_cast<size_t>(PresetFilamentType::_count); i++) {
        const auto &p = preset_filament_parameters[static_cast<PresetFilamentType>(i)];
        CHECK(p.nozzle_preheat_temperature <= p.nozzle_temperature);
    }
}
