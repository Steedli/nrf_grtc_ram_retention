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

## Hardware Requirements

- **Board**: nRF54L15DK (nrf54l15dk/nrf54l15/cpuapp)
- **SDK**: nRF Connect SDK v3.1.0 or later
- **Connection**: USB for programming and serial console

## Building and Running

### Build
```bash
cd nrf_grtc_timer
west build -b nrf54l15dk/nrf54l15/cpuapp
```

### Flash
```bash
west flash
```

### Monitor Output
Connect to the serial console at 115200 baud to view logs.

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

**GRTC Counter Test**:
- Cold boot: Counter starts near 0 (< 1 second)
- After reset: Counter shows accumulated time (> previous value)
- Increment: ~100ms (reset duration) between measurements

**RAM Data Test**:
- CRC validation passes after reset
- Boot counter increments correctly
- Uptime accumulates across sessions

## File Structure

```
nrf_grtc_timer/
├── CMakeLists.txt
├── prj.conf
├── README.md                          # This file
├── GRTC_RETENTION_README.md          # Technical details
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay
└── src/
    ├── main.c                         # Main application
    ├── utc_time.c/h                  # GRTC retention management
    └── retained.c/h                  # RAM retention implementation
```

## Troubleshooting

### GRTC Not Retaining
- Verify `CONFIG_NRF_GRTC_TIMER=y` is set
- Check device tree overlay is correct
- Ensure using warm reset (`sys_reboot(SYS_REBOOT_WARM)`)

### Retained RAM Invalid
- Verify memory region doesn't overlap with main SRAM
- Check `CONFIG_RETAINED_MEM=y` is enabled
- Ensure CRC calculation matches (use `CONFIG_CRC=y`)

### Build Errors
- Clean build: `west build -b nrf54l15dk/nrf54l15/cpuapp --pristine`
- Verify SDK version: `west --version` (should be NCS v3.1.0+)

## References

- [nRF54L15 Product Specification](https://infocenter.nordicsemi.com/topic/struct_nrf54l15/struct/nrf54l15.html)
- [GRTC Technical Documentation](https://docs.nordicsemi.com/)
- [Zephyr Retained Memory Driver](https://docs.zephyrproject.org/latest/hardware/peripherals/retained_mem.html)

## License

Copyright (c) 2024 Nordic Semiconductor ASA
SPDX-License-Identifier: Apache-2.0
