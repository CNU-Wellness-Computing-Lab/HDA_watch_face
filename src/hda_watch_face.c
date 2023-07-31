#include <tizen.h>
#include <privacy_privilege_manager.h>

#include <sensor/hrm_listener.h>
#include <sensor/physics_listener.h>
#include <sensor/environment_listener.h>
#include <tools/sqlite_helper.h>
#include "bluetooth/gatt/server.h"
#include "bluetooth/gatt/service.h"
#include "bluetooth/gatt/characteristic.h"
#include "bluetooth/gatt/descriptor.h"
#include "bluetooth/le/advertiser.h"
#include<device/power.h>
#include <feedback.h>

#include <app.h>
#include <watch_app.h>
#include <watch_app_efl.h>
#include <system_settings.h>
#include <efl_extension.h>
#include <dlog.h>
#include <device/battery.h>

#include "view.h"
#include "data.h"
#include "hda_watch_face.h"

static struct main_info {
	int sec_min_restart;
	int cur_day;
	int cur_month;
	int cur_weekday;
	bool ambient;
	bool low_battery;
	bool smooth_tick;
	int cur_min;
} s_info = { .sec_min_restart = 0, .cur_day = 0, .cur_month = 0, .cur_weekday =
		0, .ambient = false, .low_battery = false, .smooth_tick = false,
		.cur_min = 0 };
int hour = 0;
int min = 0;
int sec = 0;
int year = 0;
int month = 0;
int day = 0;
int day_of_week = 0;
int battery_level = 0;

typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *label;

	Evas_Object *basic_screen;
	Evas_Object *btn_report;
	Evas_Object *text_btn_report;
	Evas_Object *btn_active;
	Evas_Object *text_btn_active;
} appdata_s;

static void pushed_down_active(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static void pushed_up_active(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static Eina_Bool pushed_down_active_animate(void *user_data);
static Eina_Bool pushed_up_active_animate(void *user_data);

static void pushed_down_report(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static void pushed_up_report(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static Eina_Bool pushed_down_report_animate(void *user_data);
static Eina_Bool pushed_up_report_animate(void *user_data);

#define TEXT_BUF_SIZE 256

sensor_type_e sensor_type = SENSOR_HRM;

sensor_h hrm_sensor_handle = 0;
sensor_h hrm_led_green_sensor_handle = 0;

sensor_h accelerometer_sensor_handle = 0;
sensor_h gravity_sensor_handle = 0;
sensor_h gyroscope_rotation_vector_sensor_hanlde = 0;
sensor_h gyroscope_sensor_handle = 0;
sensor_h linear_acceleration_sensor_handle = 0;

sensor_h light_sensor_handle = 0;
sensor_h pedometer_handle = 0;
sensor_h pressure_sensor_handle = 0;
sensor_h sleep_monitor_handle = 0;

bool hrm_is_launched = false;
bool physics_is_launched = false;
bool environment_is_launched = false;

sqlite3 *sql_db;

bool check_hrm_sensor_is_supported();
bool initialize_hrm_sensor();
bool initialize_hrm_led_green_sensor();

bool check_physics_sensor_is_supported();
bool initialize_accelerometer_sensor();
bool initialize_gravity_sensor();
bool initialize_gyroscope_rotation_vector_sensor();
bool initialize_gyroscope_sensor();
bool initialize_linear_acceleration_sensor();

bool check_environment_sensor_is_supported();
bool initialize_light_sensor();
bool initialize_pedometer();
bool initialize_pressure_sensor();
bool initialize_sleep_monitor();

static void _encore_thread_update_date(void *data, Ecore_Thread *thread);

const char *sensor_privilege = "http://tizen.org/privilege/healthinfo";
const char *mediastorage_privilege = "http://tizen.org/privilege/mediastorage";

bool check_and_request_sensor_permission();
bool request_sensor_permission();
void request_health_sensor_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data);
void request_physics_sensor_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data);

static void _set_time(int hour, int min, int sec);
static void _set_date(int day, int month, int day_of_week);
static void _set_battery(int bat);
static Evas_Object *_create_parts(parts_type_e type);
static void _create_base_gui(int width, int height);

void lang_changed(app_event_info_h event_info, void* user_data) {
	/*
	 * Takes necessary actions when language setting is changed
	 */
	char *locale = NULL;

	system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE,
			&locale);
	if (locale == NULL)
		return;

	elm_language_set(locale);
	free(locale);

	return;
}
void region_changed(app_event_info_h event_info, void* user_data) {
	/*
	 * Takes necessary actions when region setting is changed
	 */
}
void low_battery(app_event_info_h event_info, void* user_data) {
	/*
	 * Takes necessary actions when system is running on low battery
	 */
}
void low_memory(app_event_info_h event_info, void* user_data) {
	/*
	 * Takes necessary actions when system is running on low memory
	 */
	watch_app_exit();
}
void device_orientation(app_event_info_h event_info, void* user_data) {
	/*
	 * Takes necessary actions when device orientation is changed
	 */
}

static void update_watch(appdata_s *ad, watch_time_h watch_time, int ambient) {
	char watch_text[TEXT_BUF_SIZE];
	int hour24, minute, second;

	if (watch_time == NULL)
		return;

	watch_time_get_hour24(watch_time, &hour24);
	watch_time_get_minute(watch_time, &minute);
	watch_time_get_second(watch_time, &second);
	if (!ambient) {
		snprintf(watch_text, TEXT_BUF_SIZE,
				"<align=center>Hello Watch<br/>%02d:%02d:%02d</align>", hour24,
				minute, second);
	} else {
		snprintf(watch_text, TEXT_BUF_SIZE,
				"<align=center>Hello Watch<br/>%02d:%02d</align>", hour24,
				minute);
	}

	elm_object_text_set(ad->label, watch_text);
}

static void create_base_gui(appdata_s *ad, int width, int height) {
	int ret;
	watch_time_h watch_time = NULL;

	/* Window */
	ret = watch_app_get_elm_win(&ad->win);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get window. err = %d", ret);
		return;
	}

	evas_object_resize(ad->win, width, height);

	/* Conformant */
	ad->conform = elm_conformant_add(ad->win);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND,
	EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	ad->basic_screen = elm_grid_add(ad->conform);
	elm_object_content_set(ad->conform, ad->basic_screen);
	evas_object_show(ad->basic_screen);

	ad->label = elm_label_add(ad->basic_screen);
	elm_grid_pack(ad->basic_screen, ad->label, 0, 20, 100, 30);
	evas_object_show(ad->label);

	// 활성화 버튼
	ad->btn_active = evas_object_rectangle_add(ad->basic_screen);
	evas_object_color_set(ad->btn_active, 0, 0, 0, 255);
	elm_grid_pack(ad->basic_screen, ad->btn_active, 0, 50, 50, 50);
	evas_object_show(ad->btn_active);
	ad->text_btn_active = elm_label_add(ad->basic_screen);
	evas_object_color_set(ad->text_btn_active, 255, 255, 255, 255);
	elm_object_text_set(ad->text_btn_active,
			"<align=center><font_size=30><b>활성화</b></font></align>");
	elm_grid_pack(ad->basic_screen, ad->text_btn_active, 0, 65, 50, 10);
	evas_object_show(ad->text_btn_active);
	evas_object_event_callback_add(ad->btn_active, EVAS_CALLBACK_MOUSE_DOWN,
			pushed_down_active, ad);
	evas_object_event_callback_add(ad->btn_active, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_active, ad);
	evas_object_event_callback_add(ad->text_btn_active,
			EVAS_CALLBACK_MOUSE_DOWN, pushed_down_active, ad);
	evas_object_event_callback_add(ad->text_btn_active, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_active, ad);

	// 기록 버튼
	ad->btn_report = evas_object_rectangle_add(ad->basic_screen);
	evas_object_color_set(ad->btn_report, 0, 0, 0, 255);
	elm_grid_pack(ad->basic_screen, ad->btn_report, 50, 50, 50, 50);
	evas_object_show(ad->btn_report);
	ad->text_btn_report = elm_label_add(ad->basic_screen);
	evas_object_color_set(ad->text_btn_report, 255, 255, 255, 255);
	elm_object_text_set(ad->text_btn_report,
			"<align=center><font_size=30><b>기록</b></font></align>");
	elm_grid_pack(ad->basic_screen, ad->text_btn_report, 50, 65, 50, 10);
	evas_object_show(ad->text_btn_report);
	evas_object_event_callback_add(ad->btn_report, EVAS_CALLBACK_MOUSE_DOWN,
			pushed_down_report, ad);
	evas_object_event_callback_add(ad->btn_report, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_report, ad);
	evas_object_event_callback_add(ad->text_btn_report,
			EVAS_CALLBACK_MOUSE_DOWN, pushed_down_report, ad);
	evas_object_event_callback_add(ad->text_btn_report, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_report, ad);

//	ad->btn_report = elm_button_add(ad->basic_screen);
//	elm_object_text_set(ad->btn_report, "기록");
//	elm_grid_pack(ad->basic_screen, ad->btn_report, 50, 50, 50, 50);
//	evas_object_show(ad->btn_report);

	ret = watch_time_get_current_time(&watch_time);
	if (ret != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get current time. err = %d",
				ret);

	update_watch(ad, watch_time, 0);
	watch_time_delete(watch_time);

	/* Show window after base gui is set up */
	evas_object_show(ad->win);

	ecore_thread_feedback_run(_encore_thread_update_date, NULL, NULL, NULL, ad,
	EINA_FALSE);
}

static bool app_create(int width, int height, void *data) {
	/* Hook to take necessary actions before main event loop starts
	 Initialize UI resources and application's data
	 If this function returns true, the main loop of application starts
	 If this function returns false, the application is terminated */
	app_event_handler_h handlers[5] = { NULL, };

	/*
	 * Register callbacks for each system event
	 */
	if (watch_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED],
			APP_EVENT_LANGUAGE_CHANGED, lang_changed, NULL) != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG,
				"watch_app_add_event_handler () is failed");

	if (watch_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED],
			APP_EVENT_REGION_FORMAT_CHANGED, region_changed, NULL)
			!= APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG,
				"watch_app_add_event_handler () is failed");

	if (watch_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY],
			APP_EVENT_LOW_BATTERY, low_battery, NULL) != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG,
				"watch_app_add_event_handler () is failed");

	if (watch_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY],
			APP_EVENT_LOW_MEMORY, low_memory, NULL) != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG,
				"watch_app_add_event_handler () is failed");

	if (watch_app_add_event_handler(
			&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED],
			APP_EVENT_DEVICE_ORIENTATION_CHANGED, device_orientation, NULL)
			!= APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG,
				"watch_app_add_event_handler () is failed");

	dlog_print(DLOG_DEBUG, LOG_TAG, "%s", __func__);

	appdata_s *ad = data;
	create_base_gui(ad, width, height);

	return true;
}

static void app_control(app_control_h app_control, void *data) {
	/* Handle the launch request. */
}

static void app_pause(void *data) {
	/* Take necessary actions when application becomes invisible. */
	s_info.smooth_tick = false;
}

static void app_resume(void *data) {
	s_info.smooth_tick = false;

	appdata_s *ad = data;
//	if (!check_and_request_sensor_permission()) {
//		dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
//				"Failed to check if an application has permission to use the sensor privilege.");
////		ui_app_exit();
//	} else
//		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//				"Succeeded in checking if an application has permission to use the sensor privilege.");
}

static void app_terminate(void *data) {
	view_destroy_base_gui();

	int retval;

	if (check_hrm_sensor_listener_is_created()) {
		if (!destroy_hrm_sensor_listener())
			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
					"Failed to release all the resources allocated for a HRM sensor listener.");
		else
			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
					"Succeeded in releasing all the resources allocated for a HRM sensor listener.");
	}

	if (check_physics_sensor_listener_is_created()) {
		if (!destroy_physics_sensor_listener())
			dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
					"Failed to release all the resources allocated for a Physics sensor listener.");
		else
			dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
					"Succeeded in releasing all the resources allocated for a Physics sensor listener.");
	}

	if (check_environment_sensor_listener_is_created()) {
		if (!destroy_environment_sensor_listener())
			dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
					"Failed to release all the resources allocated for a Environment sensor listener.");
		else
			dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
					"Succeeded in releasing all the resources allocated for a Environment sensor listener.");
	}

	// Bluetooth //
	if (!destroy_gatt_service())
		dlog_print(DLOG_ERROR, BLUETOOTH_LOG_TAG,
				"Failed to destroy the GATT handle of service.");
	else
		dlog_print(DLOG_INFO, BLUETOOTH_LOG_TAG,
				"Succeeded in destroying the GATT handle of service.");
	if (!destroy_gatt_server())
		dlog_print(DLOG_ERROR, BLUETOOTH_LOG_TAG,
				"Failed to destroy the GATT server's handle.");
	else
		dlog_print(DLOG_INFO, BLUETOOTH_LOG_TAG,
				"Succeeded in destroying the GATT server's handle.");

	retval = bt_deinitialize();
	if (retval != BT_ERROR_NONE) {
		dlog_print(DLOG_INFO, BLUETOOTH_LOG_TAG,
				"Function bt_deinitialize() return value = %s",
				get_error_message(retval));
		dlog_print(DLOG_INFO, BLUETOOTH_LOG_TAG,
				"Failed to release all resources of the Bluetooth API.");
	} else
		dlog_print(DLOG_INFO, BLUETOOTH_LOG_TAG,
				"Succeeded in releasing all resources of the Bluetooth API.");
}

static void app_time_tick(watch_time_h watch_time, void *data) {
	/* Called at each second while your app is visible. Update watch UI. */
	appdata_s *ad = data;
	update_watch(ad, watch_time, 0);
}

static void app_ambient_tick(watch_time_h watch_time, void *data) {
	/* Called at each minute while the device is in ambient mode. Update watch UI. */
	appdata_s *ad = data;
	update_watch(ad, watch_time, 1);
}

static void app_ambient_changed(bool ambient_mode, void *data) {
	/* Update your watch UI to conform to the ambient mode */
}

//static void app_time_tick(watch_time_h watch_time, void *data) {
//	/* Called at each second while your app is visible. Update watch UI. */
//	appdata_s *ad = data;
////	int hour = 0;
////	int min = 0;
////	int sec = 0;
////	int year = 0;
////	int month = 0;
////	int day = 0;
////	int day_of_week = 0;
////	int battery_level = 0;
//
//	watch_time_get_hour(watch_time, &hour);
//	watch_time_get_minute(watch_time, &min);
//	watch_time_get_second(watch_time, &sec);
//	watch_time_get_day(watch_time, &day);
//	watch_time_get_month(watch_time, &month);
//	watch_time_get_year(watch_time, &year);
//	watch_time_get_day_of_week(watch_time, &day_of_week);
//
//	int ret = device_battery_get_percent(&battery_level);
//	if (ret != 0) {
//		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get battery level");
//		return;
//	}
//
////	_set_time(hour, min, sec);
////	_set_date(day, month, day_of_week);
////	_set_battery(battery_level);
//}
//
//static void app_ambient_tick(watch_time_h watch_time, void *data) {
//	/* Called at each minute while the device is in ambient mode. Update watch UI. */
//	appdata_s *ad = data;
//	int hour = 0;
//	int min = 0;
//	int battery_level = 0;
//
//	watch_time_get_hour(watch_time, &hour);
//	watch_time_get_minute(watch_time, &min);
//
//	_set_time(hour, min, 0);
//
//	int ret = device_battery_get_percent(&battery_level);
//	if (ret != 0) {
//		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get battery level");
//		return;
//	}
//
//	_set_battery(battery_level);
//}
//
//static void app_ambient_changed(bool ambient_mode, void *data) {
//	/* Update your watch UI to conform to the ambient mode */
//	s_info.ambient = ambient_mode;
//
//	Evas_Object *bg = NULL;
//	Evas_Object *object = NULL;
//	Evas_Object *hands = NULL;
//
//	bg = view_get_bg();
//	if (bg == NULL) {
//		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get bg");
//		return;
//	}
//
//	if (ambient_mode) // Ambient
//	{
//		// Set Watchface
//		set_object_background_image(bg,
//				(s_info.low_battery ? IMAGE_BG_AMBIENT_LOWBAT : IMAGE_BG_AMBIENT));
//
//		object = view_get_bg_plate();
//		evas_object_hide(object);
//
//		// Set Day
//		object = view_get_module_day_layout();
//		edje_object_signal_emit(object, "set_ambient", "");
//
//		if (s_info.low_battery) {
//			evas_object_hide(object);
//		}
//
//		//Set Battery Hand
//		hands = evas_object_data_get(bg, "__HANDS_BAT__");
//		evas_object_hide(hands);
//		hands = evas_object_data_get(bg, "__HANDS_BAT_SHADOW__");
//		evas_object_hide(hands);
//
//		//Set Minute Hand
//		set_object_background_image(evas_object_data_get(bg, "__HANDS_MIN__"),
//				(s_info.low_battery ?
//				IMAGE_HANDS_MIN_AMBIENT_LOWBAT :
//										IMAGE_HANDS_MIN_AMBIENT));
//
//		hands = evas_object_data_get(bg, "__HANDS_MIN_SHADOW__");
//		evas_object_hide(hands);
//
//		//Set Hour Hand
//		set_object_background_image(evas_object_data_get(bg, "__HANDS_HOUR__"),
//				(s_info.low_battery ?
//				IMAGE_HANDS_HOUR_AMBIENT_LOWBAT :
//										IMAGE_HANDS_HOUR_AMBIENT));
//
//		hands = evas_object_data_get(bg, "__HANDS_HOUR_SHADOW__");
//		evas_object_hide(hands);
//
//		//Set Second Hand
//		object = view_get_module_second_layout();
//		edje_object_signal_emit(object, "second_set_ambient", "");
//		evas_object_hide(object);
//
//		/*TODO: Commented out as smooth tick is re-implemented
//		 //Set Minute Hand
//		 object = view_get_module_minute_layout();
//		 edje_object_signal_emit(object,"minute_set_ambient","");
//		 */
//
//		s_info.smooth_tick = false;
//
//	} else // Non-ambient
//	{
//		// Set Watchface
//		set_object_background_image(bg, IMAGE_BG);
//		object = view_get_bg_plate();
//		evas_object_show(object);
//
//		//Set Day
//		object = view_get_module_day_layout();
//		evas_object_show(object);
//		edje_object_signal_emit(object, "set_default", "");
//
//		//Set Battery Hand
//		hands = evas_object_data_get(bg, "__HANDS_BAT__");
//		evas_object_show(hands);
//		hands = evas_object_data_get(bg, "__HANDS_BAT_SHADOW__");
//		evas_object_show(hands);
//
//		//Set Second Hand
//		hands = view_get_module_second_layout();
//		evas_object_show(hands);
//
//		//Set Minute Hand
//		set_object_background_image(evas_object_data_get(bg, "__HANDS_MIN__"),
//		IMAGE_HANDS_MIN);
//		hands = evas_object_data_get(bg, "__HANDS_MIN_SHADOW__");
//		evas_object_show(hands);
//
//		//Set Hour Hand
//		set_object_background_image(evas_object_data_get(bg, "__HANDS_HOUR__"),
//		IMAGE_HANDS_HOUR);
//		hands = evas_object_data_get(bg, "__HANDS_HOUR_SHADOW__");
//		evas_object_show(hands);
//	}
//}

static void watch_app_lang_changed(app_event_info_h event_info, void *user_data) {
	/*APP_EVENT_LANGUAGE_CHANGED*/
	char *locale = NULL;
	app_event_get_language(event_info, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}

static void watch_app_region_changed(app_event_info_h event_info,
		void *user_data) {
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

int main(int argc, char *argv[]) {
	appdata_s ad = { 0, };
	int ret = 0;

	watch_app_lifecycle_callback_s event_callback = { 0, };
	app_event_handler_h handlers[5] = { NULL, };

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;
	event_callback.time_tick = app_time_tick;
	event_callback.ambient_tick = app_ambient_tick;
	event_callback.ambient_changed = app_ambient_changed;

	watch_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED],
			APP_EVENT_LANGUAGE_CHANGED, watch_app_lang_changed, &ad);
	watch_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED],
			APP_EVENT_REGION_FORMAT_CHANGED, watch_app_region_changed, &ad);

	ret = watch_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "watch_app_main() is failed. err = %d",
				ret);
	}

	return ret;
}

static void _set_time(int hour, int min, int sec) {
	Evas_Object *bg = NULL;
	Evas_Object *hands = NULL;
	Evas_Object *hands_shadow = NULL;
	double degree = 0.0f;

	bg = view_get_bg();
	if (bg == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get bg");
		return;
	}

	/*
	 * Rotate hands at the watch
	 */
	if (!s_info.ambient && !s_info.smooth_tick) {
		s_info.smooth_tick = true;
		degree = sec * SEC_ANGLE;
		hands = view_get_module_second_layout();
		view_rotate_hand(hands, degree, (BASE_WIDTH / 2), (BASE_HEIGHT / 2));
		edje_object_signal_emit(hands, "second_start_tick", "");
	}

	degree = (min * MIN_ANGLE) + data_get_minute_plus_angle(sec);
	hands = evas_object_data_get(bg, "__HANDS_MIN__");
	view_rotate_hand(hands, degree, (BASE_WIDTH / 2), (BASE_HEIGHT / 2));
	hands_shadow = evas_object_data_get(bg, "__HANDS_MIN_SHADOW__");
	view_rotate_hand(hands_shadow, degree, (BASE_WIDTH / 2),
			(BASE_HEIGHT / 2) + HANDS_MIN_SHADOW_PADDING);

	if (s_info.cur_min != min) {
		s_info.cur_min = min;

		degree = (hour * HOUR_ANGLE) + data_get_hour_plus_angle(min, sec);
		hands = evas_object_data_get(bg, "__HANDS_HOUR__");
		view_rotate_hand(hands, degree, (BASE_WIDTH / 2), (BASE_HEIGHT / 2));
		hands_shadow = evas_object_data_get(bg, "__HANDS_HOUR_SHADOW__");
		view_rotate_hand(hands_shadow, degree, (BASE_WIDTH / 2),
				(BASE_HEIGHT / 2) + HANDS_HOUR_SHADOW_PADDING);
	}
}

/**
 * @brief Set battery level of the watch.
 * @pram[in] bat The battery level
 */
static void _set_battery(int bat) {
	Evas_Object *bg = NULL;
	Evas_Object *hands = NULL;
	Evas_Object *hands_shadow = NULL;
	double degree = 0.0f;

	bg = view_get_bg();
	if (bg == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get bg");
		return;
	}

	/*
	 *
	 *  Adjust battery indicator status
	 */

	// Low Battery
	if (bat <= LOW_BATTERY_LEVEL && !s_info.low_battery) {
		s_info.low_battery = true;

		set_object_background_image(evas_object_data_get(bg, "__HANDS_BAT__"),
		IMAGE_HANDS_BAT_LOWBAT);

		if (s_info.ambient) {
			set_object_background_image(bg, IMAGE_BG_AMBIENT_LOWBAT);
			evas_object_hide(view_get_module_day_layout());
			set_object_background_image(
					evas_object_data_get(bg, "__HANDS_MIN__"),
					IMAGE_HANDS_MIN_AMBIENT_LOWBAT);
			set_object_background_image(
					evas_object_data_get(bg, "__HANDS_HOUR__"),
					IMAGE_HANDS_HOUR_AMBIENT_LOWBAT);
		}
	}
	// Regular Battery
	else if (bat > LOW_BATTERY_LEVEL && s_info.low_battery) {
		s_info.low_battery = false;

		set_object_background_image(evas_object_data_get(bg, "__HANDS_BAT__"),
		IMAGE_HANDS_BAT);

		if (s_info.ambient) {
			set_object_background_image(bg, IMAGE_BG_AMBIENT);
			evas_object_show(view_get_module_day_layout());
			set_object_background_image(
					evas_object_data_get(bg, "__HANDS_MIN__"),
					IMAGE_HANDS_MIN_AMBIENT);
			set_object_background_image(
					evas_object_data_get(bg, "__HANDS_HOUR__"),
					IMAGE_HANDS_HOUR_AMBIENT);
		}
	}

	// Rotate battery hand
	if (!s_info.ambient) {
		degree = BATTERY_START_ANGLE + (bat * BATTERY_ANGLE);
		hands = evas_object_data_get(bg, "__HANDS_BAT__");
		view_rotate_hand(hands, degree, (BASE_WIDTH / 2), (BASE_HEIGHT / 2));
		hands_shadow = evas_object_data_get(bg, "__HANDS_BAT_SHADOW__");
		view_rotate_hand(hands_shadow, degree, (BASE_WIDTH / 2),
				(BASE_HEIGHT / 2) + HANDS_BAT_SHADOW_PADDING);
	}
}

/**
 * @brief Set date at the watch.
 * @pram[in] day The day number
 * @pram[in] month The month number
 * @pram[in] day_of_week The day of week number
 */
static void _set_date(int day, int month, int day_of_week) {
	Evas_Object *bg = NULL;
	Evas_Object *module_layout = NULL;
	char txt_day_num[32] = { 0, };
	char txt_day_txt[4] = { 0, };

	/*
	 * Set day at the watch
	 */
	if (s_info.cur_day != day) {
		module_layout = view_get_module_day_layout();

		snprintf(txt_day_num, sizeof(txt_day_num), "%d", day);
		view_set_text(module_layout, "txt.day.num", txt_day_num);

		snprintf(txt_day_txt, sizeof(txt_day_txt), "%s",
				get_day_of_week(day_of_week));
		view_set_text(module_layout, "txt.day.txt", txt_day_txt);

		s_info.cur_day = day;
	}

	bg = view_get_bg();
	if (bg == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get bg");
		return;
	}
}

/**
 * @brief Create parts of watch.
 * @param[in] type Parts type
 */
static Evas_Object *_create_parts(parts_type_e type) {
	Evas_Object *parts = NULL;
	Evas_Object *bg = NULL;
	char *parts_image_path = NULL;
	int x = 0, y = 0, w = 0, h = 0;

	/*
	 * Get the BG
	 */
	bg = view_get_bg();

	/*
	 * Get the information about the part
	 */
	parts_image_path = data_get_parts_image_path(type);
	data_get_parts_position(type, &x, &y);
	w = data_get_parts_width_size(type);
	h = data_get_parts_height_size(type);

	/*
	 * Create the part object
	 */
	parts = view_create_parts(bg, parts_image_path, x, y, w, h);
	if (parts == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create parts : %d", type);
	}

	free(parts_image_path);

	/*
	 * Set opacity to shadow hands
	 */
	if (type == PARTS_TYPE_HANDS_HOUR_SHADOW
			|| type == PARTS_TYPE_HANDS_MIN_SHADOW
			|| type == PARTS_TYPE_HANDS_SEC_SHADOW) {
		view_set_opacity_to_parts(parts);
	}

	return parts;
}

/**
 * @brief Create base GUI for the watch.
 * @param[in] width The width size of the watch
 * @param[in] height The height size of the watch
 */
static void _create_base_gui(int width, int height) {
	Evas_Object *win = NULL;
	Evas_Object *bg = NULL;
	Evas_Object *bg_plate = NULL;
	Evas_Object *module_day_layout = NULL;
	Evas_Object *module_sec_layout = NULL;
	Evas_Object *hands_min = NULL;
	Evas_Object *hands_min_shadow = NULL;
	Evas_Object *hands_hour = NULL;
	Evas_Object *hands_hour_shadow = NULL;
	Evas_Object *hands_bat = NULL;
	Evas_Object *hands_bat_shadow = NULL;
	char bg_path[PATH_MAX] = { 0, };
	char bg_plate_path[PATH_MAX] = { 0, };
	char edj_path[PATH_MAX] = { 0, };
	int ret = 0;

	/*
	 * Get window object
	 */
	ret = watch_app_get_elm_win(&win);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get window. err = %d", ret);
		return;
	}
	evas_object_resize(win, width, height);
	evas_object_show(win);

	/*
	 * Get background image file path
	 */
	data_get_resource_path(IMAGE_BG, bg_path, sizeof(bg_path));

	/*
	 * Create BG
	 */
	bg = view_create_bg(win, bg_path, width, height);
	if (bg == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create a bg");
		return;
	}

	/*
	 * Create hands & shadow hands to display the battery level
	 */

	hands_bat_shadow = _create_parts(PARTS_TYPE_HANDS_BAT_SHADOW);
	evas_object_data_set(bg, "__HANDS_BAT_SHADOW__", hands_bat_shadow);
	hands_bat = _create_parts(PARTS_TYPE_HANDS_BAT);
	evas_object_data_set(bg, "__HANDS_BAT__", hands_bat);

	/*
	 * Get background plate image file path
	 */
	data_get_resource_path(IMAGE_BG_PLATE, bg_plate_path,
			sizeof(bg_plate_path));

	/*
	 * Create BG Plate
	 */
	bg_plate = view_create_bg_plate(bg, bg_plate_path, BG_PLATE_WIDTH,
	BG_PLATE_HEIGHT);
	if (bg_plate == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create bg plate");
		return;
	}

	/*
	 * Get edje file path
	 */
	data_get_resource_path(EDJ_FILE, edj_path, sizeof(edj_path));

	/*
	 * Create layout to display day number at the watch
	 */
	module_day_layout = view_create_module_layout(bg, edj_path,
			"layout_module_day");
	if (module_day_layout) {
		view_set_module_property(module_day_layout,
		BASE_WIDTH - MODULE_DAY_NUM_SIZE - MODULE_DAY_NUM_RIGHT_PADDING,
				(BASE_HEIGHT / 2) - (MODULE_DAY_NUM_SIZE / 2),
				MODULE_DAY_NUM_SIZE, MODULE_DAY_NUM_SIZE);
		view_set_module_day_layout(module_day_layout);
	}

	/*
	 * Create hands & shadow hands to display at the watch
	 */

	hands_min_shadow = _create_parts(PARTS_TYPE_HANDS_MIN_SHADOW);
	evas_object_data_set(bg, "__HANDS_MIN_SHADOW__", hands_min_shadow);
	hands_min = _create_parts(PARTS_TYPE_HANDS_MIN);
	evas_object_data_set(bg, "__HANDS_MIN__", hands_min);

	hands_hour_shadow = _create_parts(PARTS_TYPE_HANDS_HOUR_SHADOW);
	evas_object_data_set(bg, "__HANDS_HOUR_SHADOW__", hands_hour_shadow);
	hands_hour = _create_parts(PARTS_TYPE_HANDS_HOUR);
	evas_object_data_set(bg, "__HANDS_HOUR__", hands_hour);

	/*
	 * Create layout to display second hand on the watch
	 */
	module_sec_layout = view_create_module_layout(bg, edj_path,
			"layout_module_second");
	if (module_sec_layout) {
		view_set_module_property(module_sec_layout, 0, 0, 360, 360);
		view_set_module_second_layout(module_sec_layout);
	}
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static void pushed_down_active(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	ecore_animator_add(pushed_down_active_animate, ad);
}
static void pushed_up_active(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	ecore_animator_add(pushed_up_active_animate, ad);
}
static Eina_Bool pushed_down_active_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_active, 60, 60, 60, 255);
	return ECORE_CALLBACK_RENEW;
}
static Eina_Bool pushed_up_active_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_active, 0, 0, 0, 255);
	return ECORE_CALLBACK_RENEW;
}

static void pushed_down_report(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	ecore_animator_add(pushed_down_report_animate, ad);
}
static void pushed_up_report(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	ecore_animator_add(pushed_up_report_animate, ad);
}
static Eina_Bool pushed_down_report_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_report, 60, 60, 60, 255);
	return ECORE_CALLBACK_RENEW;
}
static Eina_Bool pushed_up_report_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_report, 0, 0, 0, 255);
	return ECORE_CALLBACK_RENEW;
}

static void _encore_thread_update_date(void *data, Ecore_Thread *thread) {
	appdata_s *ad = data;

	while (1) {
		int ret;
		watch_time_h watch_time = NULL;
		ret = watch_time_get_current_time(&watch_time);
		if (ret != APP_ERROR_NONE)
			dlog_print(DLOG_ERROR, LOG_TAG,
					"failed to get current time. err = %d", ret);

		watch_time_get_hour(watch_time, &hour);
		watch_time_get_minute(watch_time, &min);
		watch_time_get_second(watch_time, &sec);
		watch_time_get_day(watch_time, &day);
		watch_time_get_month(watch_time, &month);
		watch_time_get_year(watch_time, &year);
		watch_time_get_day_of_week(watch_time, &day_of_week);

		sleep(1);
	}
}

//sensor_is_supported()
bool check_hrm_sensor_is_supported() {
	int hrm_retval;
	bool hrm_supported = false;
	hrm_retval = sensor_is_supported(SENSOR_HRM, &hrm_supported);

	if (hrm_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
				"Function sensor_is_supported() return value = %s",
				get_error_message(hrm_retval));
		dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
				"Failed to checks whether a HRM sensor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
				"Succeeded in checking whether a HRM sensor is supported in the current device.");

	if (!hrm_supported) {
		dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
				"Function sensor_is_supported() output supported = %d",
				hrm_supported);
		return false;
	} else
		return true;
}

bool check_physics_sensor_is_supported() {
	int accelerometer_retval;
	bool accelerometer_supported = false;
	accelerometer_retval = sensor_is_supported(SENSOR_ACCELEROMETER,
			&accelerometer_supported);

	if (accelerometer_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, ACCELEROMETER_SENSOR_LOG_TAG,
				"Accelerometer sensor_is_supported() return value = %s",
				get_error_message(accelerometer_retval));
		dlog_print(DLOG_ERROR, ACCELEROMETER_SENSOR_LOG_TAG,
				"Failed to checks whether a Accelerometer sensor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, ACCELEROMETER_SENSOR_LOG_TAG,
				"Succeeded in checking whether a Accelerometer sensor is supported in the current device.");

	if (!accelerometer_supported) {
		dlog_print(DLOG_ERROR, ACCELEROMETER_SENSOR_LOG_TAG,
				"Accelerometer sensor_is_supported() output supported = %d",
				accelerometer_supported);
		return false;
	}

	int gravity_retval;
	bool gravity_supported = false;
	gravity_retval = sensor_is_supported(SENSOR_GRAVITY, &gravity_supported);

	if (gravity_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, GRAVITY_SENSOR_LOG_TAG,
				"Gravity sensor_is_supported() return value = %s",
				get_error_message(gravity_retval));
		dlog_print(DLOG_ERROR, GRAVITY_SENSOR_LOG_TAG,
				"Failed to checks whether a Gravity sensor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, GRAVITY_SENSOR_LOG_TAG,
				"Succeeded in checking whether a Gravity sensor is supported in the current device.");

	if (!gravity_supported) {
		dlog_print(DLOG_ERROR, GRAVITY_SENSOR_LOG_TAG,
				"Gravity sensor_is_supported() output supported = %d",
				gravity_supported);
		return false;
	}

	int gyroscope_rotation_vector_retval;
	bool gyroscope_rotation_vector_supported = false;
	gyroscope_rotation_vector_retval = sensor_is_supported(SENSOR_GYROSCOPE,
			&gyroscope_rotation_vector_supported);

	if (gyroscope_rotation_vector_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, GYROSCOPE_ROTATION_VECTOR_SENSOR_LOG_TAG,
				"Gyroscope Rotation Vector sensor_is_supported() return value = %s",
				get_error_message(gyroscope_rotation_vector_retval));
		dlog_print(DLOG_ERROR, GYROSCOPE_ROTATION_VECTOR_SENSOR_LOG_TAG,
				"Failed to checks whether a Gyroscope Rotation Vector is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, GYROSCOPE_ROTATION_VECTOR_SENSOR_LOG_TAG,
				"Succeeded in checking whether a Gyroscope Rotation Vector sensor is supported in the current device.");

	if (!gyroscope_rotation_vector_supported) {
		dlog_print(DLOG_ERROR, GYROSCOPE_ROTATION_VECTOR_SENSOR_LOG_TAG,
				"Gyroscope sensor_is_supported() output supported = %d",
				gyroscope_rotation_vector_supported);
		return false;
	}

	int gyroscope_retval;
	bool gyroscope_supported = false;
	gyroscope_retval = sensor_is_supported(SENSOR_GYROSCOPE,
			&gyroscope_supported);

	if (gyroscope_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, GYROSCOPE_SENSOR_LOG_TAG,
				"Gyroscope sensor_is_supported() return value = %s",
				get_error_message(gyroscope_retval));
		dlog_print(DLOG_ERROR, GYROSCOPE_SENSOR_LOG_TAG,
				"Failed to checks whether a Gyroscope sensor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, GYROSCOPE_SENSOR_LOG_TAG,
				"Succeeded in checking whether a Gyroscope sensor is supported in the current device.");

	if (!gyroscope_supported) {
		dlog_print(DLOG_ERROR, GYROSCOPE_SENSOR_LOG_TAG,
				"Gyroscope sensor_is_supported() output supported = %d",
				gyroscope_supported);
		return false;
	}

	int linear_acceleration_retval;
	bool linear_acceleration_supported = false;
	linear_acceleration_retval = sensor_is_supported(SENSOR_LINEAR_ACCELERATION,
			&linear_acceleration_supported);

	if (linear_acceleration_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LINEAR_ACCELERATION_SENSOR_LOG_TAG,
				"linear_acceleration sensor_is_supported() return value = %s",
				get_error_message(linear_acceleration_retval));
		dlog_print(DLOG_ERROR, LINEAR_ACCELERATION_SENSOR_LOG_TAG,
				"Failed to checks whether a linear_acceleration sensor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, LINEAR_ACCELERATION_SENSOR_LOG_TAG,
				"Succeeded in checking whether a linear_acceleration sensor is supported in the current device.");

	if (!linear_acceleration_supported) {
		dlog_print(DLOG_ERROR, LINEAR_ACCELERATION_SENSOR_LOG_TAG,
				"linear_acceleration sensor_is_supported() output supported = %d",
				linear_acceleration_supported);
		return false;
	}

	return true;
}

bool check_environment_sensor_is_supported() {
	int light_retval;
	bool light_supported = false;
	light_retval = sensor_is_supported(SENSOR_LIGHT, &light_supported);

	if (light_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LIGHT_SENSOR_LOG_TAG,
				"Function sensor_is_supported() return value = %s",
				get_error_message(light_retval));
		dlog_print(DLOG_ERROR, LIGHT_SENSOR_LOG_TAG,
				"Failed to checks whether a Light sensor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, LIGHT_SENSOR_LOG_TAG,
				"Succeeded in checking whether a Light sensor is supported in the current device.");

	if (!light_supported) {
		dlog_print(DLOG_ERROR, LIGHT_SENSOR_LOG_TAG,
				"Function sensor_is_supported() output supported = %d",
				light_supported);
		return false;
	}

	int pedometer_retval;
	bool pedometer_supported = false;
	pedometer_retval = sensor_is_supported(SENSOR_HUMAN_PEDOMETER,
			&pedometer_supported);

	if (pedometer_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, PEDOMETER_LOG_TAG,
				"Function sensor_is_supported() return value = %s",
				get_error_message(pedometer_retval));
		dlog_print(DLOG_ERROR, PEDOMETER_LOG_TAG,
				"Failed to checks whether a Pedometer is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, PEDOMETER_LOG_TAG,
				"Succeeded in checking whether a Pedometer is supported in the current device.");

	if (!pedometer_supported) {
		dlog_print(DLOG_ERROR, PEDOMETER_LOG_TAG,
				"Function sensor_is_supported() output supported = %d",
				pedometer_supported);
		return false;
	}

	int pressure_retval;
	bool pressure_supported = false;
	pressure_retval = sensor_is_supported(SENSOR_PRESSURE, &pressure_supported);

	if (pressure_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, PRESSURE_SENSOR_LOG_TAG,
				"Function sensor_is_supported() return value = %s",
				get_error_message(pressure_retval));
		dlog_print(DLOG_ERROR, PRESSURE_SENSOR_LOG_TAG,
				"Failed to checks whether a Pressure sensor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, PRESSURE_SENSOR_LOG_TAG,
				"Succeeded in checking whether a Pressure sensor is supported in the current device.");

	if (!pressure_supported) {
		dlog_print(DLOG_ERROR, PRESSURE_SENSOR_LOG_TAG,
				"Function sensor_is_supported() output supported = %d",
				pressure_supported);
		return false;
	}

	int sleep_monitor_retval;
	bool sleep_monitor_supported = false;
	sleep_monitor_retval = sensor_is_supported(SENSOR_HUMAN_SLEEP_MONITOR,
			&sleep_monitor_supported);

	if (sleep_monitor_retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, SLEEP_MONITOR_LOG_TAG,
				"Function sensor_is_supported() return value = %s",
				get_error_message(sleep_monitor_retval));
		dlog_print(DLOG_ERROR, SLEEP_MONITOR_LOG_TAG,
				"Failed to checks whether a sleep_monitor is supported in the current device.");
		return false;
	} else
		dlog_print(DLOG_INFO, SLEEP_MONITOR_LOG_TAG,
				"Succeeded in checking whether a sleep_monitor sensor is supported in the current device.");

	if (!sleep_monitor_supported) {
		dlog_print(DLOG_ERROR, SLEEP_MONITOR_LOG_TAG,
				"Function sensor_is_supported() output supported = %d",
				sleep_monitor_supported);
		return false;
	}
	return true;
}

//initialize_sensor()
bool initialize_hrm_sensor() {
	int retval;

	retval = sensor_get_default_sensor(SENSOR_HRM, &hrm_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
				"Function sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else
		return true;
}

bool initialize_hrm_led_green_sensor() {
	int retval;

	retval = sensor_get_default_sensor(SENSOR_HRM_LED_GREEN,
			&hrm_led_green_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_INFO, HRM_LED_GREEN_SENSOR_LOG_TAG,
				"HRM LED green sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else
		return true;
}

bool initialize_accelerometer_sensor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_ACCELEROMETER,
			&accelerometer_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, ACCELEROMETER_SENSOR_LOG_TAG,
				"Accelerometer sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, ACCELEROMETER_SENSOR_LOG_TAG,
				"Accelerometer initialized.");
		return true;
	}
}

bool initialize_gravity_sensor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_GRAVITY, &gravity_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, GRAVITY_SENSOR_LOG_TAG,
				"Gravity sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, GRAVITY_SENSOR_LOG_TAG, "Gravity initialized.");
		return true;
	}
}

bool initialize_gyroscope_rotation_vector_sensor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_GYROSCOPE_ROTATION_VECTOR,
			&gyroscope_rotation_vector_sensor_hanlde);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, GYROSCOPE_ROTATION_VECTOR_SENSOR_LOG_TAG,
				"Gyroscope rotation sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, GYROSCOPE_ROTATION_VECTOR_SENSOR_LOG_TAG,
				"Gyroscope rotation initialized.");
		return true;
	}
}

bool initialize_gyroscope_sensor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_GYROSCOPE,
			&gyroscope_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, GYROSCOPE_SENSOR_LOG_TAG,
				"Gyroscope sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, GYROSCOPE_SENSOR_LOG_TAG,
				"Gyroscope initialized.");
		return true;
	}
}

bool initialize_linear_acceleration_sensor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_LINEAR_ACCELERATION,
			&linear_acceleration_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LINEAR_ACCELERATION_SENSOR_LOG_TAG,
				"linear_acceleration sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, LINEAR_ACCELERATION_SENSOR_LOG_TAG,
				"linear_acceleration initialized.");
		return true;
	}
}

bool initialize_light_sensor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_LIGHT, &light_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LIGHT_SENSOR_LOG_TAG,
				"Light sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, LIGHT_SENSOR_LOG_TAG, "Light initialized.");
		return true;
	}
}

bool initialize_pedometer() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_HUMAN_PEDOMETER,
			&pedometer_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, PEDOMETER_LOG_TAG,
				"Pedometer sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, PEDOMETER_LOG_TAG, "Pedometer initialized.");
		return true;
	}
}

bool initialize_pressure_sensor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_PRESSURE,
			&pressure_sensor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, PRESSURE_SENSOR_LOG_TAG,
				"pressure sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, PRESSURE_SENSOR_LOG_TAG,
				"pressure_sensor initialized.");
		return true;
	}
}

bool initialize_sleep_monitor() {
	int retval;
	retval = sensor_get_default_sensor(SENSOR_HUMAN_SLEEP_MONITOR,
			&sleep_monitor_handle);

	if (retval != SENSOR_ERROR_NONE) {
		dlog_print(DLOG_ERROR, SLEEP_MONITOR_LOG_TAG,
				"sleep_monitor sensor_get_default_sensor() return value = %s",
				get_error_message(retval));
		return false;
	} else {
		dlog_print(DLOG_INFO, SLEEP_MONITOR_LOG_TAG,
				"sleep_monitor initialized.");
		return true;
	}
}

bool check_and_request_sensor_permission() {
	bool health_usable = true;
	bool physics_usable = true;
	bool environment_usable = true;

	int health_retval;
	int mediastorage_retval;
	ppm_check_result_e health_result;
	ppm_check_result_e mediastorage_result;

	health_retval = ppm_check_permission(sensor_privilege, &health_result);
	mediastorage_retval = ppm_check_permission(mediastorage_privilege,
			&mediastorage_result);

	if (hrm_is_launched != true) {
		if (health_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
			switch (health_result) {
			case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ALLOW:
				/* Update UI and start accessing protected functionality */
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"The application has permission to use a sensor privilege.");

				if (!check_hrm_sensor_listener_is_created()) {
					if (!initialize_hrm_sensor()
							|| !initialize_hrm_led_green_sensor()) {
						dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
								"Failed to get the handle for the default sensor of a HRM sensor.");
						health_usable = false;
					} else
						dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
								"Succeeded in getting the handle for the default sensor of a HRM sensor.");

					if (!create_hrm_sensor_listener(hrm_sensor_handle,
							hrm_led_green_sensor_handle)) {
						dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
								"Failed to create a HRM sensor listener.");
						health_usable = false;
					} else
						dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
								"Succeeded in creating a HRM sensor listener.");

					if (!start_hrm_sensor_listener())
						dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
								"Failed to start observing the sensor events regarding a HRM sensor listener.");
					else
						dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
								"Succeeded in starting observing the sensor events regarding a HRM sensor listener.");
				}
				break;
			case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_DENY:
				/* Show a message and terminate the application */
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"Function ppm_check_permission() output result = PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_DENY");
				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
						"The application doesn't have permission to use a sensor privilege.");
				health_usable = false;
				break;
			case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ASK:
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"The user has to be asked whether to grant permission to use a sensor privilege.");

				if (!request_sensor_permission()) {
					dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
							"Failed to request a user's response to obtain permission for using the sensor privilege.");
					health_usable = false;
				} else {
					dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
							"Succeeded in requesting a user's response to obtain permission for using the sensor privilege.");
					health_usable = true;
				}
				break;
			}
		} else {
			/* retval != PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE */
			/* Handle errors */
			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
					"Function ppm_check_permission() return %s",
					get_error_message(health_retval));
			health_usable = false;
		}
	}

	if (physics_is_launched != true && environment_is_launched != true) {
		if (mediastorage_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
			if (mediastorage_result
					== PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ALLOW) {
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"The application has permission to use a storage privilege.");

				if (!check_physics_sensor_listener_is_created()) {
					if (!initialize_accelerometer_sensor()
							|| !initialize_gravity_sensor()
							|| !initialize_gyroscope_rotation_vector_sensor()
							|| !initialize_gyroscope_sensor()
							|| !initialize_linear_acceleration_sensor()) {
						dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
								"Failed to get the handle for the default sensor of a Physics sensor.");
						physics_usable = false;
					} else
						dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
								"Succeeded in getting the handle for the default sensor of a Physics sensor.");

					if (!create_physics_sensor_listener(
							accelerometer_sensor_handle, gravity_sensor_handle,
							gyroscope_rotation_vector_sensor_hanlde,
							gyroscope_sensor_handle,
							linear_acceleration_sensor_handle)) {
						dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
								"Failed to create a Physics sensor listener.");
						physics_usable = false;
					} else
						dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
								"Succeeded in creating a Physics sensor listener.");

					if (!start_physics_sensor_listener())
						dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
								"Failed to start observing the sensor events regarding a Physics sensor listener.");
					else
						dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
								"Succeeded in starting observing the sensor events regarding a Physics sensor listener.");
				}

				if (!check_environment_sensor_listener_is_created()) {
					if (!initialize_light_sensor() || !initialize_pedometer()
							|| !initialize_pressure_sensor()
							|| !initialize_sleep_monitor()) {
						dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
								"Failed to get the handle for the default sensor of a Environment sensor.");
						environment_usable = false;
					} else
						dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
								"Succeeded in getting the handle for the default sensor of a Environment sensor.");

					if (!create_environment_sensor_listener(light_sensor_handle,
							pedometer_handle, pressure_sensor_handle,
							sleep_monitor_handle)) {
						dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
								"Failed to create a Environment sensor listener.");
						environment_usable = false;
					} else
						dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
								"Succeeded in creating a Environment sensor listener.");

					if (!start_environment_sensor_listener())
						dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
								"Failed to start observing the sensor events regarding a Environment sensor listener.");
					else
						dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
								"Succeeded in starting observing the sensor events regarding a Environment sensor listener.");
				}
			} else if (mediastorage_result
					== PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ASK) {
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"The user has to be asked whether to grant permission to use a sensor privilege.");

				if (!request_sensor_permission()) {
					dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
							"Failed to request a user's response to obtain permission for using the sensor privilege.");
					physics_usable = false;
					environment_usable = false;
				} else {
					dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
							"Succeeded in requesting a user's response to obtain permission for using the sensor privilege.");
					physics_usable = true;
					environment_usable = true;
				}
			} else {
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"Function ppm_check_permission() output result = PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_DENY");
				dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
						"The application doesn't have permission to use a sensor privilege.");
				physics_usable = false;
				environment_usable = false;
			}
		} else {
			/* retval != PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE */
			/* Handle errors */
			dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
					"Function ppm_check_permission() return %s",
					get_error_message(mediastorage_retval));
			physics_usable = false;
			environment_usable = false;
		}
	}

	return health_usable && physics_usable && environment_usable;
}

bool request_sensor_permission() {
	int health_retval;
	int mediastorage_retval;

	health_retval = ppm_request_permission(sensor_privilege,
			request_health_sensor_permission_response_callback, NULL);
	mediastorage_retval = ppm_request_permission(mediastorage_privilege,
			request_physics_sensor_permission_response_callback, NULL);

	if (health_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE
			&& mediastorage_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
		return true;
	} else if (health_retval
			== PRIVACY_PRIVILEGE_MANAGER_ERROR_ALREADY_IN_PROGRESS
			&& mediastorage_retval
					== PRIVACY_PRIVILEGE_MANAGER_ERROR_ALREADY_IN_PROGRESS) {
		return true;
	} else {
		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
				"Function ppm_request_permission() return value = %s",
				get_error_message(health_retval));
		dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
				"Function ppm_request_permission() return value = %s",
				get_error_message(mediastorage_retval));
		return false;
	}
}

void request_health_sensor_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data) {
	if (cause == PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR) {
		/* Log and handle errors */
		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
				"Function request_sensor_permission_response_callback() output cause = PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR");
		dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
				"Function request_sensor_permission_response_callback() was called because of an error.");
	} else {
		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
				"Function request_sensor_permission_response_callback() was called with a valid answer.");

		switch (result) {
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_ALLOW_FOREVER:
			/* Update UI and start accessing protected functionality */
			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
					"The user granted permission to use a sensor privilege for an indefinite period of time.");

			if (!initialize_hrm_sensor()
					|| !initialize_hrm_led_green_sensor()) {
				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
						"Failed to get the handle for the default sensor of a HRM sensor.");
//				ui_app_exit();
			} else
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"Succeeded in getting the handle for the default sensor of a HRM sensor.");

			if (!create_hrm_sensor_listener(hrm_sensor_handle,
					hrm_led_green_sensor_handle)) {
				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
						"Failed to create a HRM sensor listener.");
//				ui_app_exit();
			} else
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"Succeeded in creating a HRM sensor listener.");

			if (!start_hrm_sensor_listener()) {
				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
						"Failed to start observing the sensor events regarding a HRM sensor listener.");
//				ui_app_exit();
			} else
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"Succeeded in starting observing the sensor events regarding a HRM sensor listener.");
			hrm_is_launched = true;
			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
			/* Show a message and terminate the application */
			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER");
			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege for an indefinite period of time.");
//			ui_app_exit();
			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
			/* Show a message with explanation */
			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE");
			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege once.");
//			ui_app_exit();
			break;
		}
	}
}

void request_physics_sensor_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data) {
	if (cause == PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR) {
		/* Log and handle errors */
		dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
				"Function request_sensor_permission_response_callback() output cause = PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR");
		dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
				"Function request_sensor_permission_response_callback() was called because of an error.");
	} else {
		dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
				"Function request_sensor_permission_response_callback() was called with a valid answer.");

		switch (result) {
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_ALLOW_FOREVER:
			/* Update UI and start accessing protected functionality */
			dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
					"The user granted permission to use a sensor privilege for an indefinite period of time.");

			if (!initialize_accelerometer_sensor()
					|| !initialize_gravity_sensor()
					|| !initialize_gyroscope_rotation_vector_sensor()
					|| !initialize_gyroscope_sensor()
					|| !initialize_linear_acceleration_sensor()) {
				dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
						"Failed to get the handle for the default sensor of a Physics sensor.");
			} else
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"Succeeded in getting the handle for the default sensor of a Physics sensor.");

			if (!create_physics_sensor_listener(accelerometer_sensor_handle,
					gravity_sensor_handle,
					gyroscope_rotation_vector_sensor_hanlde,
					gyroscope_sensor_handle,
					linear_acceleration_sensor_handle)) {
				dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
						"Failed to create a Physics sensor listener.");
			} else
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"Succeeded in creating a Physics sensor listener.");

			if (!start_physics_sensor_listener())
				dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
						"Failed to start observing the sensor events regarding a Physics sensor listener.");
			else
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"Succeeded in starting observing the sensor events regarding a Physics sensor listener.");

			if (!initialize_light_sensor() || !initialize_pedometer()
					|| !initialize_pressure_sensor()
					|| !initialize_sleep_monitor()) {
				dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
						"Failed to get the handle for the default sensor of a Environment sensor.");
			} else
				dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
						"Succeeded in getting the handle for the default sensor of a Environment sensor.");

			if (!create_environment_sensor_listener(light_sensor_handle,
					pedometer_handle, pressure_sensor_handle,
					sleep_monitor_handle)) {
				dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
						"Failed to create a Environment sensor listener.");
			} else
				dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
						"Succeeded in creating a Environment sensor listener.");

			if (!start_environment_sensor_listener())
				dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
						"Failed to start observing the sensor events regarding a Environment sensor listener.");
			else
				dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
						"Succeeded in starting observing the sensor events regarding a Environment sensor listener.");

			physics_is_launched = true;
			environment_is_launched = true;
			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
			/* Show a message and terminate the application */
			dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER");
			dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege for an indefinite period of time.");
//			ui_app_exit();
			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
			/* Show a message with explanation */
			dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE");
			dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege once.");
//			ui_app_exit();
			break;
		}
	}
}
