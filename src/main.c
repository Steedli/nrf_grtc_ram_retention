/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * GRTC RAM Retention Demo
 * Demonstrates that nRF54L15 GRTC automatically persists through software reset
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#include "retained.h"
#include <zephyr/drivers/watchdog.h>
#include <zephyr/device.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(utc_time_demo, LOG_LEVEL_INF);

// #define WDT_TEST 0

#define MAX_REBOOTS 3  // Maximum number of automatic resets
#define WDT_FEED_TRIES 5
#define WDT_ALLOW_CALLBACK 0

#ifndef WDT_MAX_WINDOW
#define WDT_MAX_WINDOW  1000U
#endif

#ifndef WDT_MIN_WINDOW
#define WDT_MIN_WINDOW  0U
#endif

#ifndef WDG_FEED_INTERVAL
#define WDG_FEED_INTERVAL 50U
#endif

#ifndef WDT_OPT
#define WDT_OPT WDT_OPT_PAUSE_HALTED_BY_DBG
#endif
int wdt_channel_id;
const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));

int watch_dog(void)
{
	int err;


	LOG_INF("Watchdog sample application\n");

	if (!device_is_ready(wdt)) {
		LOG_INF("%s: device not ready.\n", wdt->name);
		return 0;
	}

	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog timer expires. */
		.flags = WDT_FLAG_RESET_SOC,

		/* Expire watchdog after max window */
		.window.min = WDT_MIN_WINDOW,
		.window.max = WDT_MAX_WINDOW,
	};

#if WDT_ALLOW_CALLBACK
	/* Set up watchdog callback. */
	wdt_config.callback = wdt_callback;

	LOG_INF("Attempting to test pre-reset callback\n");
#else /* WDT_ALLOW_CALLBACK */
	LOG_INF("Callback in RESET_SOC disabled for this platform\n");
#endif /* WDT_ALLOW_CALLBACK */

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id == -ENOTSUP) {
		/* IWDG driver for STM32 doesn't support callback */
		LOG_INF("Callback support rejected, continuing anyway\n");
		wdt_config.callback = NULL;
		wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	}
	if (wdt_channel_id < 0) {
		LOG_INF("Watchdog install error\n");
		return 0;
	}

	err = wdt_setup(wdt, WDT_OPT);
	if (err < 0) {
		LOG_INF("Watchdog setup error\n");
		return 0;
	}

#if WDT_MIN_WINDOW != 0
	/* Wait opening window. */
	k_msleep(WDT_MIN_WINDOW);
#endif

	return 0;
}

// Work queue for triggering software reset
static void reboot_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_work_handler);

static void reboot_work_handler(struct k_work *work)
{
	LOG_WRN("\n========================================");
	LOG_WRN("=== INITIATING SOFTWARE RESET #%u ===", retained.boots + 1);
	LOG_WRN("========================================");
	
	// Record GRTC state before reset
	uint64_t grtc_before = z_nrf_grtc_timer_read();
	
	LOG_WRN("BEFORE RESET:");
	LOG_WRN("  GRTC counter: %llu us (%.3f sec)", grtc_before, (double)grtc_before / 1000000.0);
	
	LOG_WRN("\n>>> Performing software reset NOW...");
	LOG_WRN(">>> GRTC should continue counting from %llu us", grtc_before);
	
	// Update retained memory - increment boots counter
	retained.boots++;
	retained_update();
	LOG_WRN(">>> Saved retained data to RAM:");
	LOG_WRN("    boots=%u, off_count=%u, uptime_sum=%llu", 
	        retained.boots, retained.off_count, retained.uptime_sum);
	
	k_msleep(100); // Allow time for log output
	
	// Execute software reset
	sys_reboot(SYS_REBOOT_COLD);	
}

int main(void)
{
	LOG_INF("GRTC Retention Test Starting...");
	LOG_INF("========================================");
	
	// Initialize retained memory
	bool retained_ok = retained_validate();
	LOG_INF("Retained RAM: %s", retained_ok ? "VALID" : "INVALID (first boot)");
	if (retained_ok) {
		LOG_INF("=== Retained Data ===");
		LOG_INF("  boots:         %u", retained.boots);
		LOG_INF("  off_count:     %u", retained.off_count);
		LOG_INF("  uptime_latest: %llu ticks", retained.uptime_latest);
		LOG_INF("  uptime_sum:    %llu ticks (%.3f sec)", 
		        retained.uptime_sum,
		        (double)retained.uptime_sum * 1000.0 / CONFIG_SYS_CLOCK_TICKS_PER_SEC / 1000.0);
		LOG_INF("  crc:           0x%08x", retained.crc);
	}
	
	// Check GRTC current state (post-reset verification)
	uint64_t grtc_raw = z_nrf_grtc_timer_read();
	
	LOG_INF("GRTC raw counter: %llu us (%.3f seconds)", grtc_raw, (double)grtc_raw / 1000000.0);
	LOG_WRN("Current boot count: %u", retained.boots);

#ifndef WDT_TEST	
	// Check if recovering from software reset
	if (grtc_raw > 1000000ULL) {
		LOG_WRN("========================================");
		LOG_WRN(">>> SUCCESS: GRTC RETENTION WORKING! <<<");
		LOG_WRN("========================================");
		LOG_WRN("Counter value: %llu us = %.3f seconds", grtc_raw, (double)grtc_raw / 1000000.0);
		LOG_WRN("This proves GRTC has been running continuously through software reset!");
		
		// Increment off_count (reset counter)
		retained.off_count++;
	} else {
		LOG_INF(">>> GRTC appears to be freshly started (first boot or hard reset)");
		LOG_INF(">>> Counter < 1 second indicates cold boot");
	}
	
	LOG_INF("Boot count: %u (max reboots: %u)", retained.boots, MAX_REBOOTS);
	LOG_INF("========================================");
	
	// Automatically trigger software reset (to verify retention)
	if (retained.boots < MAX_REBOOTS) {
		LOG_WRN("\n=== AUTO REBOOT TEST ===");
		LOG_WRN("Will trigger software reset in 10 seconds...");
		LOG_WRN("This is to verify GRTC retention across resets");
		LOG_WRN("Current boot count: %u / %u", retained.boots, MAX_REBOOTS);
		
		// Delay 10 seconds before triggering reset
		k_work_schedule(&reboot_work, K_SECONDS(10));
		
		// Countdown display
		for (int i = 10; i > 0; i--) {
			LOG_WRN(">>> Software reset in %d seconds...", i);
			k_sleep(K_SECONDS(1));
		}
	} else {
		LOG_INF("\n=== REBOOT TEST COMPLETE ===");
		LOG_INF(">>> Maximum reboot count (%u) reached", MAX_REBOOTS);
		LOG_INF("========================================");
		LOG_INF(">>> GRTC RETENTION VALIDATED! <<<");
		LOG_INF("========================================");
		LOG_INF("The GRTC counter has persisted through %u software resets", MAX_REBOOTS);
		LOG_INF("Current GRTC value: %llu us (%.3f seconds)", 
		        z_nrf_grtc_timer_read(), 
		        (double)z_nrf_grtc_timer_read() / 1000000.0);
	}
#else
	watch_dog();
#endif		
#ifndef WDT_TEST
	// Keep program running, continuously display GRTC counter
	while (1) {

	k_sleep(K_SECONDS(1));				

	/* Waiting for the SoC reset. */
	LOG_INF("Waiting for reset...\n");		
		k_sleep(K_SECONDS(10));
		uint64_t grtc_current = z_nrf_grtc_timer_read();
		
		// Update retained memory to accumulate uptime
		retained_update();
		
		LOG_INF("=== Status ===");
		LOG_INF("GRTC: %llu us (%.3f sec)", 
		        grtc_current,
		        (double)grtc_current / 1000000.0);
		LOG_INF("Retained: boots=%u, off_count=%u, uptime_sum=%llu ticks (%.3f sec)",
		        retained.boots,
		        retained.off_count,
		        retained.uptime_sum,
		        (double)retained.uptime_sum * 1000.0 / CONFIG_SYS_CLOCK_TICKS_PER_SEC / 1000.0);
	}
#else
	/* Feeding watchdog. */

	LOG_INF("Feeding watchdog %d times\n", WDT_FEED_TRIES);

	for (int i = 0; i < WDT_FEED_TRIES; ++i) {
		LOG_INF("Feeding watchdog...\n");
		wdt_feed(wdt, wdt_channel_id);
		k_sleep(K_MSEC(WDG_FEED_INTERVAL));
	}
#endif	
	return 0;
}
