#ifndef __hda_watch_face_H__
#define __hda_watch_face_H__

#include <watch_app.h>
#include <watch_app_efl.h>
#include <Elementary.h>
#include <dlog.h>

#include <sensor.h>
#include <bluetooth.h>
#include <time.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif

//extern struct main_info {
//	int sec_min_restart;
//	int cur_day;
//	int cur_month;
//	int cur_weekday;
//	bool ambient;
//	bool low_battery;
//	bool smooth_tick;
//	int cur_min;
//	int hour;
//	int min;
//	int sec;
//	int year;
//	int month;
//	int day;
//	int day_of_week;
//	int battery_level;
//} s_info = { .sec_min_restart = 0, .cur_day = 0, .cur_month = 0, .cur_weekday =
//		0, .ambient = false, .low_battery = false, .smooth_tick = false,
//		.cur_min = 0, .hour = 0, .min = 0, .sec = 0, .year = 0, .month = 0,
//		.day = 0, .day_of_week = 0, .battery_level = 0 };

extern int hour;
extern int min;
extern int sec;
extern int year;
extern int month;
extern int day;
extern int day_of_week;
extern int battery_level;

extern int final_report_hour;
extern int final_report_min;
extern int final_report_sec;
extern int final_report_year;
extern int final_report_month;
extern int final_report_day;

extern int alert_postpone_delay_time;

/* Angle */
#define HOUR_ANGLE 30
#define MIN_ANGLE 6
#define SEC_ANGLE 6
#define MONTH_ANGLE 30
#define WEEKDAY_ANGLE 51
#define BATTERY_START_ANGLE 195
#define BATTERY_ANGLE 0.6f

/* Layout */
#define BASE_WIDTH 360
#define BASE_HEIGHT 360

#define BATTERY_START_POS_X 147
#define BATTERY_START_POS_Y 105
#define BATTERY_OFFSET_X -25
#define BATTERY_OFFSET_Y 15

#define MODULE_MOONPHASE_SIZE 240
#define MODULE_DAY_NUM_SIZE 100
#define MODULE_DAY_NUM_RIGHT_PADDING 15
#define MODULE_MONTH_SIZE 128
#define MODULE_WEEKDAY_SIZE MODULE_MONTH_SIZE

#define BG_PLATE_WIDTH 120
#define BG_PLATE_HEIGHT 120

#define HANDS_SEC_WIDTH 30
#define HANDS_SEC_HEIGHT 360
#define HANDS_MIN_WIDTH 30
#define HANDS_MIN_HEIGHT 360
#define HANDS_HOUR_WIDTH 30
#define HANDS_HOUR_HEIGHT 360
#define HANDS_BAT_WIDTH 10
#define HANDS_BAT_HEIGHT 117
#define HANDS_MODULE_CALENDAR_WIDTH 20
#define HANDS_MODULE_CALENDAR_HEIGHT 128

#define HANDS_BAT_Y_PADDING 44
#define HANDS_BAT_SHADOW_PADDING 3
#define HANDS_SEC_SHADOW_PADDING 5
#define HANDS_MIN_SHADOW_PADDING 9
#define HANDS_HOUR_SHADOW_PADDING 9

#define LOG_TAG "hda_watch_face"
#define HRM_SENSOR_LOG_TAG "HRM_SENSOR_EVENT"
#define HRM_LED_GREEN_SENSOR_LOG_TAG "HRM_LED_GREEN_SENSOR_EVENT"

#define PHYSICS_SENSOR_LOG_TAG "PHYSICS_SENSOR_EVENT"
#define ACCELEROMETER_SENSOR_LOG_TAG "ACCELEROMETE_SENSOR_EVENT"
#define GRAVITY_SENSOR_LOG_TAG "GRAVITY_SENSOR_EVENT"
#define BLUETOOTH_LOG_TAG "BLUETOOTH_EVENT"
#define GYROSCOPE_ROTATION_VECTOR_SENSOR_LOG_TAG "GYROSCOPE_ROTATION_VECTOR_SENSOR_EVENT"
#define GYROSCOPE_SENSOR_LOG_TAG "GYROSCOPE_SENSOR_EVENT"
#define LINEAR_ACCELERATION_SENSOR_LOG_TAG "LINEAR_ACCLERATION_SENSOR_EVENT"

#define ENVIRONMENT_SENSOR_LOG_TAG "ENVIRONMENT_SENSOR_EVENT"
#define LIGHT_SENSOR_LOG_TAG "LIGHT_SENSOR_EVENT"
#define PEDOMETER_LOG_TAG "PEDOMETER_EVENT" //
#define PRESSURE_SENSOR_LOG_TAG "PRESSURE_SENSOR_EVENT"
#define SLEEP_MONITOR_LOG_TAG "SLEEP_MONITOR_EVENT" //

#define SQLITE3_LOG_TAG "SQLITE3_EVENT"

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "hda_watch_face"

#if !defined(PACKAGE)
#define PACKAGE "org.example.hda_watch_face"
#endif

#endif /* __hda_watch_face_H__ */
