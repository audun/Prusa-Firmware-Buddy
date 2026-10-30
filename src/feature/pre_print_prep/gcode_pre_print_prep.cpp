#include <marlin_stubs/PrusaGcodeSuite.hpp>
#include <common/gcode/gcode_parser.hpp>

#include "pre_print_prep.hpp"

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1706: Pre-print preparation
 *
 * Internal GCode
 *
 * not meant to be used during print
 *
 * Asks whether the bed is clear and the steel sheet is in place, then homes
 * all axes (with sheet detection), runs the nozzle-cleanup taps and verifies
 * the bed mesh over the whole bed (G29 P11, full probe when the mesh is not
 * valid), all inside one dialog with a Stop button. On success the printer
 * stays "prepared": the steppers are kept enabled and the position and mesh
 * stay trusted until the position is lost, the nozzle target is lowered,
 * 30 minutes pass or a print ends. Live-Z is reset, as at the start of every
 * print. While prepared and with the "Trust Prep for G28" setting on, a bare
 * G28 behaves as G28 Q.
 *
 *#### Usage
 *
 *    M1706 [ S0 | C0 | Z | M ]
 *
 *#### Parameters
 *
 * - `S0` - Do not ask the "bed clear" question (the caller already did)
 * - `C0` - Skip the nozzle-cleanup taps
 * - `Z` - Also tap the Z homing point when verifying the mesh (G29 P11 Z)
 * - `M` - Tap the mesh check points even when the mesh is fresh (G29 P11 M)
 *
 *#### Examples
 *
 *    M1706    ; ask, home, clean, verify
 *    M1706 S0 ; the same without the question
 */
void PrusaGcodeSuite::M1706() {
    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    // Parse everything first, the preparation runs further G-codes through the shared parser
    buddy::pre_print_prep::RunArgs args;
    p.store_option_if_present('S', args.ask_bed_clear);
    p.store_option_if_present('C', args.cleanup);
    p.store_option_if_present('Z', args.include_home_point);
    p.store_option_if_present('M', args.force_measure);

    buddy::pre_print_prep::run(args);
}

/** @}*/
