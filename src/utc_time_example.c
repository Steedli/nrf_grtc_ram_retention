/*
 * UTC Time Example - Demonstrates GRTC-based UTC time management
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "utc_time.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("=== UTC Time Management Example ===");
	LOG_INF("Using GRTC Timer for high-precision timekeeping");
	
	/* Wait a bit for system to stabilize */
	k_sleep(K_MSEC(100));
	
	/* Example 1: Use raw GRTC time (not calibrated) */
	LOG_INF("\n--- Example 1: Raw GRTC Time ---");
	utc_time_print();
	
	/* Example 2: Calibrate with Unix timestamp */
	LOG_INF("\n--- Example 2: Calibrate UTC Time ---");
	/* December 11, 2025, 00:00:00 UTC = 1765411200 seconds */
	uint64_t unix_timestamp = 1765411200ULL;
	utc_time_calibrate_unix(unix_timestamp);
	
	/* Example 3: Get UTC time in different formats */
	LOG_INF("\n--- Example 3: Get UTC Time ---");
	uint64_t utc_us = utc_time_get_us();
	uint64_t utc_ms = utc_time_get_ms();
	uint64_t utc_sec = utc_time_get_sec();
	
	LOG_INF("UTC time (microseconds): %llu", utc_us);
	LOG_INF("UTC time (milliseconds): %llu", utc_ms);
	LOG_INF("UTC time (seconds):      %llu", utc_sec);
	
	/* Example 4: Use UTC time structure */
	LOG_INF("\n--- Example 4: UTC Time Structure ---");
	utc_time_t time;
	utc_time_get(&time);
	LOG_INF("Calibrated: %s", time.calibrated ? "Yes" : "No");
	LOG_INF("Seconds:    %llu", time.seconds);
	LOG_INF("Millisec:   %llu", time.milliseconds);
	LOG_INF("Microsec:   %llu", time.microseconds);
	
	/* Example 5: Measure time intervals */
	LOG_INF("\n--- Example 5: Measure Time Intervals ---");
	uint64_t start_time = utc_time_get_us();
	k_sleep(K_MSEC(500)); /* Sleep for 500ms */
	uint64_t end_time = utc_time_get_us();
	
	int64_t diff_us = utc_time_diff_us(start_time, end_time);
	LOG_INF("Time interval: %lld us (%.3f ms)", diff_us, diff_us / 1000.0);
	
	/* Example 6: Format time as string */
	LOG_INF("\n--- Example 6: Format Time ---");
	char time_str[64];
	utc_time_format_us(end_time, time_str, sizeof(time_str));
	LOG_INF("Formatted time: %s", time_str);
	
	/* Example 7: Continuous monitoring */
	LOG_INF("\n--- Example 7: Continuous Monitoring ---");
	LOG_INF("Printing UTC time every second (press Ctrl+C to stop)...\n");
	
	int count = 0;
	while (1) {
		utc_time_print();
		
		/* Also demonstrate precision timing */
		uint64_t now = utc_time_get_us();
		uint64_t ms_part = (now / 1000ULL) % 1000ULL;
		uint64_t us_part = now % 1000ULL;
		LOG_INF("  -> Precision: .%03llu.%03llu", ms_part, us_part);
		
		k_sleep(K_SECONDS(1));
		
		count++;
		if (count >= 10) {
			LOG_INF("\nExample completed after 10 seconds");
			break;
		}
	}
	
	LOG_INF("\n=== UTC Time Example Finished ===");
	
	return 0;
}
