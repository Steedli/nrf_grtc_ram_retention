# GRTC Retention Test

## Overview

This example demonstrates GRTC (Global Real-Time Counter) retention and RAM retention features on nRF54L15 SoC. It validates that the GRTC counter continues counting through software resets and that retained RAM preserves data across resets.

## Features

### GRTC Retention
- **Hardware counter persistence**: GRTC system counter continues counting through software resets
- **System counter register**: `z_nrf_grtc_timer_read()` reads GRTC system counter register directly
- **Automatic retention**: On nRF54L15, GRTC automatically persists through `SYS_REBOOT_COLD` (no manual configuration needed)
- **1 MHz precision**: 64-bit microsecond resolution
- **Limitation**: GRTC counter does NOT persist through watchdog reset

### RAM Retention
- **Persistent data storage**: Uses retained RAM region to preserve application state
- **CRC32 validation**: Ensures data integrity across resets
- **Tracked metrics**:
  - `boots`: Number of application starts
  - `off_count`: Number of software resets performed
  - `uptime_sum`: Cumulative system uptime across sessions
  - `uptime_latest`: Current session uptime tracking

### Automatic Testing
- Performs 3 automatic software resets
- Displays GRTC counter and retained data before/after each reset
- Validates retention functionality without user intervention
- Optional watchdog reset test (enabled via `WDT_TEST` macro)

## Hardware Requirements

- **Board**: nRF54L15DK (nrf54l15dk/nrf54l15/cpuapp)
- **SDK**: nRF Connect SDK v3.1.0 or later
- **Connection**: USB for programming and serial console

## Building and Running

### Build (Software Reset Test)
```bash
cd nrf_grtc_ram_retention
west build -b nrf54l15dk/nrf54l15/cpuapp
```

### Build (Watchdog Reset Test)
```bash
cd nrf_grtc_ram_retention
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DCONFIG_WATCHDOG=y
# Then uncomment #define WDT_TEST in src/main.c
```

### Flash
```bash
west flash
```

### Monitor Output
Connect to the serial console at 115200 baud to view logs.

## Test Modes

### Mode 1: Software Reset Test (Default)
Tests GRTC and RAM retention through software resets.

**Configuration**: Default build (no changes needed)

**Expected behavior**:
- GRTC counter continues incrementing through resets
- Retained RAM data persists
- Test performs 3 automatic resets

### Mode 2: Watchdog Reset Test
Tests GRTC and RAM retention through watchdog timeout.

**Configuration**:
1. Uncomment `#define WDT_TEST` in `src/main.c` (line 21)
2. Ensure `CONFIG_WATCHDOG=y` in prj.conf

**Expected behavior**:
- Watchdog feeds 5 times successfully
- After stopping feeds, watchdog triggers reset
- GRTC counter resets to 0 (does not persist)
- Retained RAM data still persists
- Boot counter increments

## Expected Output

### First Boot (Cold Start)
```
GRTC Retention Test Starting...
========================================
Retained RAM: INVALID (first boot)
GRTC raw counter: 9409 us (0.009 seconds)
GRTC retention: INACTIVE
>>> GRTC appears to be freshly started (first boot or hard reset)
>>> Counter < 1 second indicates cold boot
>>> Enabling GRTC retention for first time...
Boot count: 0 (max reboots: 3)
========================================

=== AUTO REBOOT TEST ===
Will trigger software reset in 10 seconds...
```

### After Software Reset (Warm Start)
```
GRTC Retention Test Starting...
========================================
Retained RAM: VALID
=== Retained Data ===
  boots:         1
  off_count:     1
  uptime_latest: 0 ticks
  uptime_sum:    12345 ticks (12.345 sec)
  crc:           0x12345678
GRTC raw counter: 15118416 us (15.118 seconds)
GRTC retention: ACTIVE
========================================
>>> SUCCESS: GRTC RETENTION WORKING! <<<
========================================
Counter value: 15118416 us = 15.118 seconds
This proves GRTC has been running continuously through software reset!
```

### Test Complete
After 3 resets:
```
=== REBOOT TEST COMPLETE ===
>>> Maximum reboot count (3) reached
========================================
>>> GRTC RETENTION VALIDATED! <<<
========================================
The GRTC counter has persisted through 3 software resets
Current GRTC value: 45234567 us (45.235 seconds)
```

## Implementation Details

### Device Tree Configuration
- **Retained RAM**: 4KB region at `0x2002e000`
- **CPUAPP SRAM**: Reduced to 184KB to avoid overlap
- **Retention device**: Exposed as `retainedmemdevice` alias

### Configuration Options
```kconfig
CONFIG_NRF_GRTC_TIMER=y       # Enable GRTC timer
CONFIG_RETAINED_MEM=y         # Enable retained memory driver
CONFIG_REBOOT=y               # Enable system reboot support
CONFIG_CRC=y                  # Enable CRC32 for data validation
```

### Key Components

#### GRTC Retention (utc_time.c)
```c
// GRTC system counter is read via Zephyr API
uint64_t grtc_time = z_nrf_grtc_timer_read();

// On nRF54L15, GRTC automatically persists through software reset
// when CONFIG_NRF_GRTC_TIMER=y is enabled.
// No manual KEEPRUNNING register configuration is needed.
```

**Implementation Notes**:
- `z_nrf_grtc_timer_read()` directly reads the hardware GRTC system counter register
- The system counter is in the always-on power domain
- Software reset (`SYS_REBOOT_COLD`) does not clear this register
- Watchdog reset DOES clear the counter (counter resets to 0)

#### RAM Retention (retained.c)
- Uses Zephyr retained memory driver
- Implements CRC32 validation (residue: `0x2144df1c`)
- Automatically updates uptime tracking
- Validates data integrity on boot

### Retention Verification

**GRTC Counter Test - Software Reset**:
- Cold boot: Counter starts near 0 (< 1 second)
- After software reset: Counter shows accumulated time (> previous value)
- Increment: ~100ms (reset duration) between measurements
- ✅ **Result**: GRTC persists through `SYS_REBOOT_COLD`

**GRTC Counter Test - Watchdog Reset**:
- Enable watchdog test: Define `WDT_TEST` macro
- Watchdog configured for 1000ms timeout
- Feed watchdog 5 times, then stop feeding
- After watchdog reset: Counter resets to near 0
- ❌ **Result**: GRTC does NOT persist through watchdog reset

**RAM Data Test**:
- CRC validation passes after software reset
- Boot counter increments correctly
- Uptime accumulates across sessions
- ✅ **Result**: Retained RAM persists through both software and watchdog resets

## File Structure

```
nrf_grtc_ram_retention/
├── CMakeLists.txt
├── prj.conf
├── README.md                          # This file
├── README_detailed.md                 # Technical details (legacy)
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay
└── src/
    ├── main.c                         # Main application (with WDT test option)
    ├── utc_time.c/h                   # GRTC time utilities (simplified)
    └── retained.c/h                   # RAM retention implementation
```

## Key Findings

### Reset Type Behavior Summary

| Reset Type | GRTC Counter | Retained RAM | Trigger Method |
|------------|--------------|--------------|----------------|
| Software Reset (`SYS_REBOOT_COLD`) | ✅ Persists | ✅ Persists | `sys_reboot()` |
| Watchdog Reset | ❌ Resets to 0 | ✅ Persists | WDT timeout |
| Hard Reset (Power cycle) | ❌ Resets to 0 | ❌ Lost | Power off/on |

### Technical Details

**Why GRTC Persists Through Software Reset:**
- The `z_nrf_grtc_timer_read()` function reads the hardware GRTC system counter register directly
- This register is located in the always-on power domain
- Software reset does not affect the always-on domain, so the counter continues running
- `CONFIG_NRF_GRTC_TIMER=y` configures the GRTC driver but does not need manual KEEPRUNNING register setup on nRF54L15

**Why GRTC Does NOT Persist Through Watchdog Reset:**
- Watchdog reset is a hardware-level reset that affects more subsystems than software reset
- The GRTC system counter is cleared during watchdog reset sequence
- This is expected behavior and documented in nRF54L15 hardware specifications

## Troubleshooting

### GRTC Not Retaining
- Verify `CONFIG_NRF_GRTC_TIMER=y` is set
- Check device tree overlay is correct
- Ensure using software reset (`sys_reboot(SYS_REBOOT_COLD)`)
- Note: Watchdog reset will clear GRTC counter (expected behavior)

### Retained RAM Invalid
- Verify memory region doesn't overlap with main SRAM
- Check `CONFIG_RETAINED_MEM=y` is enabled
- Ensure CRC calculation matches (use `CONFIG_CRC=y`)

### Build Errors
- Clean build: `west build -b nrf54l15dk/nrf54l15/cpuapp --pristine`


## References

- [nRF54L15 Product Specification](https://infocenter.nordicsemi.com/topic/struct_nrf54l15/struct/nrf54l15.html)
- [GRTC Technical Documentation](https://docs.nordicsemi.com/)
- [Zephyr Retained Memory Driver](https://docs.zephyrproject.org/latest/hardware/peripherals/retained_mem.html)

## License

Copyright (c) 2024 Nordic Semiconductor ASA
SPDX-License-Identifier: Apache-2.0
