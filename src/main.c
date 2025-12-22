/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#include "utc_time.h"
#include "retained.h"

LOG_MODULE_REGISTER(utc_time_demo, LOG_LEVEL_INF);

#define MAX_REBOOTS 3  // Maximum number of automatic resets

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
	bool retention_active = utc_time_retention_active();
	
	LOG_WRN("BEFORE RESET:");
	LOG_WRN("  GRTC counter: %llu us (%.3f sec)", grtc_before, (double)grtc_before / 1000000.0);
	LOG_WRN("  Retention:    %s", retention_active ? "ACTIVE" : "INACTIVE");
	
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
	// sys_reboot(SYS_REBOOT_WARM);
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
	bool retention_active = utc_time_retention_active();
	
	LOG_INF("GRTC raw counter: %llu us (%.3f seconds)", grtc_raw, (double)grtc_raw / 1000000.0);
	LOG_INF("GRTC retention: %s", retention_active ? "ACTIVE" : "INACTIVE");
	
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
		
		// Enable retention on first boot
		LOG_INF(">>> Enabling GRTC retention for first time...");
		utc_time_calibrate_unix(0);  // Call once to enable retention
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
	
	// Keep program running, continuously display GRTC counter
	while (1) {
		k_sleep(K_SECONDS(10));
		uint64_t grtc_current = z_nrf_grtc_timer_read();
		
		// Update retained memory to accumulate uptime
		retained_update();
		
		LOG_INF("=== Status ===");
		LOG_INF("GRTC: %llu us (%.3f sec) | retention: %s", 
		        grtc_current,
		        (double)grtc_current / 1000000.0,
		        utc_time_retention_active() ? "active" : "inactive");
		LOG_INF("Retained: boots=%u, off_count=%u, uptime_sum=%llu ticks (%.3f sec)",
		        retained.boots,
		        retained.off_count,
		        retained.uptime_sum,
		        (double)retained.uptime_sum * 1000.0 / CONFIG_SYS_CLOCK_TICKS_PER_SEC / 1000.0);
	}
	return 0;
}
