/*
 * UTC Time Management using GRTC Timer - Header File
 */

#ifndef UTC_TIME_H
#define UTC_TIME_H

#include <zephyr/types.h>
#include <stdbool.h>

/**
 * @brief UTC time structure
 */
typedef struct {
	uint64_t microseconds;  /**< Time in microseconds */
	uint64_t milliseconds;  /**< Time in milliseconds */
	uint64_t seconds;       /**< Time in seconds (Unix timestamp) */
	bool calibrated;        /**< Whether time is calibrated */
} utc_time_t;

/**
 * @brief Calibrate UTC time with external time source
 * 
 * @param utc_timestamp_us UTC timestamp in microseconds
 */
void utc_time_calibrate(uint64_t utc_timestamp_us);

/**
 * @brief Calibrate UTC time with Unix timestamp (seconds since 1970-01-01)
 * 
 * @param unix_timestamp Unix timestamp in seconds
 */
void utc_time_calibrate_unix(uint64_t unix_timestamp);

/**
 * @brief Check if UTC time is calibrated
 * 
 * @return true if calibrated, false otherwise
 */
bool utc_time_is_calibrated(void);

/**
 * @brief Get current UTC timestamp in microseconds
 * 
 * @return UTC timestamp in microseconds, or raw GRTC if not calibrated
 */
uint64_t utc_time_get_us(void);

/**
 * @brief Get current UTC timestamp in milliseconds
 * 
 * @return UTC timestamp in milliseconds
 */
uint64_t utc_time_get_ms(void);

/**
 * @brief Get current UTC timestamp in seconds
 * 
 * @return UTC timestamp in seconds (Unix timestamp)
 */
uint64_t utc_time_get_sec(void);

/**
 * @brief Get UTC time as struct
 * 
 * @param time Pointer to utc_time_t structure to fill
 */
void utc_time_get(utc_time_t *time);

/**
 * @brief Print current UTC time
 */
void utc_time_print(void);

/**
 * @brief Get time difference in microseconds
 * 
 * @param time1 First timestamp (microseconds)
 * @param time2 Second timestamp (microseconds)
 * @return Time difference in microseconds (time2 - time1)
 */
int64_t utc_time_diff_us(uint64_t time1, uint64_t time2);

/**
 * @brief Convert microseconds to human-readable format
 * 
 * @param us Microseconds
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
int utc_time_format_us(uint64_t us, char *buffer, size_t size);

/**
 * @brief Format current UTC time to human-readable string
 * 
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
int utc_time_format(char *buffer, size_t size);

/**
 * @brief Enable GRTC retention mode
 * 
 * When enabled, GRTC counter continues running through software reset,
 * allowing time to persist without NVS storage.
 * This is automatically called during calibration.
 */
void utc_time_enable_retention(void);

/**
 * @brief Check if GRTC retention is active
 * 
 * @return true if GRTC will persist through reset, false otherwise
 */
bool utc_time_retention_active(void);

#endif /* UTC_TIME_H */
