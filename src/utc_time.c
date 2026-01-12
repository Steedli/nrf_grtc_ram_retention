/*
 * UTC Time Management using GRTC Timer
 * Copyright (c) 2025
 * 
 * NOTE: On nRF54L15/nRF54H20, GRTC (Global Real-Time Counter) automatically 
 * persists through software reset when CONFIG_NRF_GRTC_TIMER=y is enabled.
 * No manual KEEPRUNNING register configuration is needed.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#include <zephyr/logging/log.h>
#include "utc_time.h"

LOG_MODULE_REGISTER(utc_time, LOG_LEVEL_INF);

/* UTC time offset (microseconds) */
static int64_t utc_offset = 0;
static bool calibrated = false;

/**
 * @brief Calibrate UTC time with external time source
 * 
 * @param utc_timestamp_us UTC timestamp in microseconds
 */
void utc_time_calibrate(uint64_t utc_timestamp_us)
{
	uint64_t grtc_time = z_nrf_grtc_timer_read();
	utc_offset = (int64_t)utc_timestamp_us - (int64_t)grtc_time;
	calibrated = true;
	
	LOG_INF("UTC time calibrated");
	LOG_INF("  GRTC time: %llu us", grtc_time);
	LOG_INF("  UTC time:  %llu us", utc_timestamp_us);
	LOG_INF("  Offset:    %lld us", utc_offset);
}

/**
 * @brief Calibrate UTC time with Unix timestamp (seconds since 1970-01-01)
 * 
 * @param unix_timestamp Unix timestamp in seconds
 */
void utc_time_calibrate_unix(uint64_t unix_timestamp)
{
	uint64_t utc_us = unix_timestamp * 1000000ULL;
	utc_time_calibrate(utc_us);
}

/**
 * @brief Check if UTC time is calibrated
 * 
 * @return true if calibrated, false otherwise
 */
bool utc_time_is_calibrated(void)
{
	return calibrated;
}

/**
 * @brief Get current UTC timestamp in microseconds
 * 
 * @return UTC timestamp in microseconds, or raw GRTC if not calibrated
 */
uint64_t utc_time_get_us(void)
{
	uint64_t grtc_time = z_nrf_grtc_timer_read();
	
	if (!calibrated) {
		LOG_WRN("UTC time not calibrated, returning raw GRTC time");
		return grtc_time;
	}
	
	return grtc_time + utc_offset;
}

/**
 * @brief Get current UTC timestamp in milliseconds
 * 
 * @return UTC timestamp in milliseconds
 */
uint64_t utc_time_get_ms(void)
{
	return utc_time_get_us() / 1000ULL;
}

/**
 * @brief Get current UTC timestamp in seconds
 * 
 * @return UTC timestamp in seconds (Unix timestamp)
 */
uint64_t utc_time_get_sec(void)
{
	return utc_time_get_us() / 1000000ULL;
}

/**
 * @brief Get UTC time as struct
 * 
 * @param time Pointer to utc_time_t structure to fill
 */
void utc_time_get(utc_time_t *time)
{
	if (time == NULL) {
		return;
	}
	
	uint64_t us = utc_time_get_us();
	
	time->microseconds = us;
	time->milliseconds = us / 1000ULL;
	time->seconds = us / 1000000ULL;
	time->calibrated = calibrated;
}

/**
 * @brief Print current UTC time
 */
void utc_time_print(void)
{
	utc_time_t time;
	utc_time_get(&time);
	
	if (time.calibrated) {
		LOG_INF("UTC Time: %llu sec (%llu ms, %llu us)", 
		        time.seconds, time.milliseconds, time.microseconds);
	} else {
		LOG_INF("GRTC Time (not calibrated): %llu us", time.microseconds);
	}
}

/**
 * @brief Get time difference in microseconds
 * 
 * @param time1 First timestamp (microseconds)
 * @param time2 Second timestamp (microseconds)
 * @return Time difference in microseconds (time2 - time1)
 */
int64_t utc_time_diff_us(uint64_t time1, uint64_t time2)
{
	return (int64_t)time2 - (int64_t)time1;
}

/**
 * @brief Convert microseconds to human-readable format
 * 
 * @param us Microseconds
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
int utc_time_format_us(uint64_t us, char *buffer, size_t size)
{
	uint64_t sec = us / 1000000ULL;
	uint64_t ms = (us / 1000ULL) % 1000ULL;
	uint64_t remaining_us = us % 1000ULL;
	
	return snprintf(buffer, size, "%llu.%03llu.%03llu s", sec, ms, remaining_us);
}

/**
 * @brief Format current UTC time to human-readable string
 * 
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
int utc_time_format(char *buffer, size_t size)
{
	uint64_t us = utc_time_get_us();
	return utc_time_format_us(us, buffer, size);
}
