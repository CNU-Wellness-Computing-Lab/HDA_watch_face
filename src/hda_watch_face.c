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
#include <app_control.h>
#include <time.h>

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

int final_report_hour = 0;
int final_report_min = 0;
int final_report_sec = 0;
int final_report_year = 0;
int final_report_month = 0;
int final_report_day = 0;

int alert_postpone_delay_time = 1800;

typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *label;

	Evas_Object *basic_screen;
	Evas_Object *btn_report;
	Evas_Object *text_btn_report;
	Evas_Object *btn_active;
	Evas_Object *text_btn_active;

	Evas_Object *alert_screen;
	Evas_Object *alert;
	Evas_Object *btn_postpone_30;
	Evas_Object *text_btn_postpone_30;
	Evas_Object *btn_postpone_90;
	Evas_Object *text_btn_postpone_90;
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

static void pushed_down_postpone_30(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static void pushed_up_postpone_30(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static Eina_Bool pushed_down_postpone_30_animate(void *user_data);
static Eina_Bool pushed_up_postpone_30_animate(void *user_data);

static void pushed_down_postpone_90(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static void pushed_up_postpone_90(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info);
static Eina_Bool pushed_down_postpone_90_animate(void *user_data);
static Eina_Bool pushed_up_postpone_90_animate(void *user_data);

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

typedef enum sensor_launched_state {
	NONE = 0, REQUEST, LAUNCHED, ALLOWED
} sensor_launched_state_e;

sensor_launched_state_e hrm_launched_state = NONE;
sensor_launched_state_e physics_launched_state = NONE;
sensor_launched_state_e environment_launched_state = NONE;

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
static void _set_alert_visible(void *data, Ecore_Thread *thread, void *msgdata);
static void _encore_thread_check_wear(void*date, Ecore_Thread *thread);
//int GetTimeT(int year, int month, int day, int hour, int minute, int second);

const char *sensor_privilege = "http://tizen.org/privilege/healthinfo";
const char *mediastorage_privilege = "http://tizen.org/privilege/mediastorage";

bool check_and_request_sensor_permission();
bool request_sensor_permission();
bool request_mediastorage_permission();
void request_sensor_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data);
void request_mediastorage_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data);

app_control_h app_controller;

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
				"<align=center>HDA Service<br/>%02d:%02d:%02d</align>", hour24,
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
//	ad->btn_active = evas_object_rectangle_add(ad->basic_screen);
//	evas_object_color_set(ad->btn_active, 0, 0, 0, 255);
//	elm_grid_pack(ad->basic_screen, ad->btn_active, 0, 50, 50, 50);
//	evas_object_show(ad->btn_active);
//	ad->text_btn_active = elm_label_add(ad->basic_screen);
//	evas_object_color_set(ad->text_btn_active, 255, 255, 255, 255);
//	elm_object_text_set(ad->text_btn_active,
//			"<align=center><font_size=30><b>활성화</b></font></align>");
//	elm_grid_pack(ad->basic_screen, ad->text_btn_active, 0, 65, 50, 10);
//	evas_object_show(ad->text_btn_active);
//	evas_object_event_callback_add(ad->btn_active, EVAS_CALLBACK_MOUSE_DOWN,
//			pushed_down_active, ad);
//	evas_object_event_callback_add(ad->btn_active, EVAS_CALLBACK_MOUSE_UP,
//			pushed_up_active, ad);
//	evas_object_event_callback_add(ad->text_btn_active,
//			EVAS_CALLBACK_MOUSE_DOWN, pushed_down_active, ad);
//	evas_object_event_callback_add(ad->text_btn_active, EVAS_CALLBACK_MOUSE_UP,
//			pushed_up_active, ad);

	// 기록 버튼
	ad->btn_report = evas_object_rectangle_add(ad->basic_screen);
	evas_object_color_set(ad->btn_report, 0, 0, 0, 255);
	elm_grid_pack(ad->basic_screen, ad->btn_report, 0, 50, 100, 50);
	evas_object_show(ad->btn_report);
	ad->text_btn_report = elm_label_add(ad->basic_screen);
	evas_object_color_set(ad->text_btn_report, 255, 255, 255, 255);
	elm_object_text_set(ad->text_btn_report,
			"<align=center><font_size=30><b>기록</b></font></align>");
	elm_grid_pack(ad->basic_screen, ad->text_btn_report, 0, 65, 100, 10);
	evas_object_show(ad->text_btn_report);
	evas_object_event_callback_add(ad->btn_report, EVAS_CALLBACK_MOUSE_DOWN,
			pushed_down_report, ad);
	evas_object_event_callback_add(ad->btn_report, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_report, ad);
	evas_object_event_callback_add(ad->text_btn_report,
			EVAS_CALLBACK_MOUSE_DOWN, pushed_down_report, ad);
	evas_object_event_callback_add(ad->text_btn_report, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_report, ad);

	ad->label = elm_label_add(ad->basic_screen);
	elm_grid_pack(ad->basic_screen, ad->label, 0, 20, 100, 30);
	evas_object_show(ad->label);



	ad->alert_screen = elm_grid_add(ad->win);
	elm_object_content_set(ad->win, ad->alert_screen);
	evas_object_show(ad->alert_screen);

	Evas_Object *alert_bg = elm_bg_add(ad->alert_screen);
	elm_bg_color_set(alert_bg, 0, 0, 0);
	elm_grid_pack(ad->alert_screen, alert_bg, 0, 0, 100, 100);
	evas_object_show(alert_bg);

	// 30
	ad->btn_postpone_30 = evas_object_rectangle_add(ad->alert_screen);
	evas_object_color_set(ad->btn_postpone_30, 166, 255, 166, 255);
	elm_grid_pack(ad->alert_screen, ad->btn_postpone_30, 0, 50, 50, 50);
	evas_object_show(ad->btn_postpone_30);
	ad->text_btn_postpone_30 = elm_label_add(ad->alert_screen);
	evas_object_color_set(ad->text_btn_postpone_30, 0, 0, 0, 255);
	elm_object_text_set(ad->text_btn_postpone_30,
			"<align=center><font_size=30><b>30분 후 알림</b></font></align>");
	elm_grid_pack(ad->alert_screen, ad->text_btn_postpone_30, 0, 65, 50, 10);
	evas_object_show(ad->text_btn_postpone_30);
	evas_object_event_callback_add(ad->btn_postpone_30, EVAS_CALLBACK_MOUSE_DOWN,
			pushed_down_postpone_30, ad);
	evas_object_event_callback_add(ad->btn_postpone_30, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_postpone_30, ad);
	evas_object_event_callback_add(ad->text_btn_postpone_30,
			EVAS_CALLBACK_MOUSE_DOWN, pushed_down_postpone_30, ad);
	evas_object_event_callback_add(ad->text_btn_postpone_30, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_postpone_30, ad);

	// 90
	ad->btn_postpone_90 = evas_object_rectangle_add(ad->alert_screen);
	evas_object_color_set(ad->btn_postpone_90, 166, 200, 255, 255);
	elm_grid_pack(ad->alert_screen, ad->btn_postpone_90, 50, 50, 50, 50);
	evas_object_show(ad->btn_postpone_90);
	ad->text_btn_postpone_90 = elm_label_add(ad->alert_screen);
	evas_object_color_set(ad->text_btn_postpone_90, 0, 0, 0, 255);
	elm_object_text_set(ad->text_btn_postpone_90,
			"<align=center><font_size=30><b>90분 후 알림</b></font></align>");
	elm_grid_pack(ad->alert_screen, ad->text_btn_postpone_90, 50, 65, 50, 10);
	evas_object_show(ad->text_btn_postpone_90);
	evas_object_event_callback_add(ad->btn_postpone_90, EVAS_CALLBACK_MOUSE_DOWN,
			pushed_down_postpone_90, ad);
	evas_object_event_callback_add(ad->btn_postpone_90, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_postpone_90, ad);
	evas_object_event_callback_add(ad->text_btn_postpone_90,
			EVAS_CALLBACK_MOUSE_DOWN, pushed_down_postpone_90, ad);
	evas_object_event_callback_add(ad->text_btn_postpone_90, EVAS_CALLBACK_MOUSE_UP,
			pushed_up_postpone_90, ad);

	ad->alert = elm_label_add(ad->alert_screen);
	elm_grid_pack(ad->alert_screen, ad->alert, 0, 20, 100, 30);
	elm_object_text_set(ad->alert, "<align=center><font_size=30><b>시계의 착용 상태를<br>확인해주세요.</b></font></align>");
	evas_object_show(ad->alert);

	evas_object_hide(ad->alert_screen);

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
	ecore_thread_feedback_run(_encore_thread_check_wear, _set_alert_visible, NULL, NULL, ad,
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
	feedback_initialize();
	s_info.smooth_tick = false;

	appdata_s *ad = data;
	if (!check_and_request_sensor_permission()) {
		dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
				"Failed to check if an application has permission to use the sensor privilege.");
	} else
		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
				"Succeeded in checking if an application has permission to use the sensor privilege.");
}

static void app_terminate(void *data) {
	feedback_deinitialize();
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

	app_control_create(&app_controller);
	app_control_set_operation(app_controller, APP_CONTROL_OPERATION_DEFAULT);
	app_control_set_app_id(app_controller, "org.example.hda_report");

	app_control_error_e ret = app_control_send_launch_request(app_controller,
	NULL, NULL);
	if (ret == APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Succeeded to launch a reporting app.");
	} else {
		dlog_print(DLOG_INFO, LOG_TAG,
				"Failed to launch a reporting app. (error=%d)", ret);
	}

	app_control_destroy(app_controller);
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

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static void pushed_down_postpone_30(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	ecore_animator_add(pushed_down_postpone_30_animate, ad);
}
static void pushed_up_postpone_30(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	alert_postpone_delay_time = 1800;

	final_report_year = year;
	final_report_month = month;
	final_report_day = day;
	final_report_hour = hour;
	final_report_min = min;
	final_report_sec = sec;
	dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG, "%d %d %d %d %d %d", year, month,
			day, hour, min, sec);

	evas_object_hide(ad->alert_screen);
	ecore_animator_add(pushed_up_postpone_30_animate, ad);
}
static Eina_Bool pushed_down_postpone_30_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_active, 87, 134, 87, 255);
	return ECORE_CALLBACK_RENEW;
}
static Eina_Bool pushed_up_postpone_30_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_active, 166, 255, 166, 255);
	return ECORE_CALLBACK_RENEW;
}

static void pushed_down_postpone_90(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	ecore_animator_add(pushed_down_postpone_90_animate, ad);
}
static void pushed_up_postpone_90(void *user_data, Evas* e, Evas_Object *obj,
		void *event_info) {
	appdata_s *ad = user_data;
	alert_postpone_delay_time = 5400;

	final_report_year = year;
	final_report_month = month;
	final_report_day = day;
	final_report_hour = hour;
	final_report_min = min;
	final_report_sec = sec;
	dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG, "%d %d %d %d %d %d", year, month,
			day, hour, min, sec);

	evas_object_hide(ad->alert_screen);
	ecore_animator_add(pushed_up_postpone_90_animate, ad);
}
static Eina_Bool pushed_down_postpone_90_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_report, 87, 105, 134, 255);
	return ECORE_CALLBACK_RENEW;
}
static Eina_Bool pushed_up_postpone_90_animate(void *user_data) {
	appdata_s *ad = user_data;
	evas_object_color_set(ad->btn_report, 166, 200, 255, 255);
	return ECORE_CALLBACK_RENEW;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static void _encore_thread_update_date(void *data, Ecore_Thread *thread) {
	appdata_s *ad = data;

	while (1) {
		int ret;
		watch_time_h watch_time = NULL;
		ret = watch_time_get_current_time(&watch_time);
		if (ret != APP_ERROR_NONE)
			dlog_print(DLOG_ERROR, LOG_TAG,
					"failed to get current time. err = %d", ret);
		watch_time_get_hour24(watch_time, &hour);
		watch_time_get_minute(watch_time, &min);
		watch_time_get_second(watch_time, &sec);
		watch_time_get_day(watch_time, &day);
		watch_time_get_month(watch_time, &month);
		watch_time_get_year(watch_time, &year);
		watch_time_get_day_of_week(watch_time, &day_of_week);

		sleep(1);
	}
}

static void _set_alert_visible(void *data, Ecore_Thread *thread, void *msgdata) {
	appdata_s *ad = data;
	evas_object_show(ad->alert_screen);
}
static void _encore_thread_check_wear(void *data, Ecore_Thread *thread) {
	appdata_s *ad = data;

	while (1) {
		if(final_report_year == 0 || final_report_month == 0 || final_report_day == 0){
			sleep(1);
			continue;
		}

		unsigned long final_report_ts = (((((final_report_year * 12 + final_report_month) * 30) + final_report_day) * 24 + final_report_hour) * 60 + final_report_min) * 60 + final_report_sec;
		unsigned long current_ts = (((((year * 12 + month) * 30) + day) * 24 + hour) * 60 + min) * 60 + sec;

		if (final_report_ts <= current_ts - alert_postpone_delay_time - 600) {
			ecore_thread_feedback(thread, (void*) (uintptr_t) final_report_ts);
		}
		else if(final_report_ts <= current_ts - alert_postpone_delay_time){
			ecore_thread_feedback(thread, (void*) (uintptr_t) final_report_ts);
//			evas_object_show(ad->alert_screen);
			feedback_play(FEEDBACK_PATTERN_VIBRATION_ON);
		}
		else{
			evas_object_hide(ad->alert_screen);
		}
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

	if (hrm_launched_state == NONE || hrm_launched_state == ALLOWED) {
		if (health_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
			switch (health_result) {
			case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ALLOW:
				/* Update UI and start accessing protected functionality */
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"The application has permission to use a sensor privilege.");

				hrm_launched_state = LAUNCHED;
				if (!check_hrm_sensor_listener_is_created()) {
					if (!initialize_hrm_sensor()
							|| !initialize_hrm_led_green_sensor()) {
						dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
								"Failed to get the handle for the default sensor of a HRM sensor.");
						health_usable = false;
						hrm_launched_state = NONE;
					} else
						dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
								"Succeeded in getting the handle for the default sensor of a HRM sensor.");

					if (!create_hrm_sensor_listener(hrm_sensor_handle,
							hrm_led_green_sensor_handle)) {
						dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
								"Failed to create a HRM sensor listener.");
						health_usable = false;
						hrm_launched_state = NONE;
					} else
						dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
								"Succeeded in creating a HRM sensor listener.");

					if (!start_hrm_sensor_listener()) {
						dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
								"Failed to start observing the sensor events regarding a HRM sensor listener.");
						health_usable = false;
						hrm_launched_state = NONE;
					} else
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
				hrm_launched_state = NONE;
				break;
			case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ASK:
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"The user has to be asked whether to grant permission to use a sensor privilege.");

				hrm_launched_state = REQUEST;
				if (!request_sensor_permission()) {
					dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
							"Failed to request a user's response to obtain permission for using the sensor privilege.");
					health_usable = false;
					hrm_launched_state = NONE;
				} else {
					dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
							"Succeeded in requesting a user's response to obtain permission for using the sensor privilege.");
					health_usable = true;
					hrm_launched_state = ALLOWED;
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
			hrm_launched_state = NONE;
		}
	}

	if ((physics_launched_state == NONE || physics_launched_state == ALLOWED)
			&& (environment_launched_state == NONE
					|| environment_launched_state == ALLOWED)) {
		if (mediastorage_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
			if (mediastorage_result
					== PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ALLOW) {
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"The application has permission to use a storage privilege.");

				physics_launched_state = LAUNCHED;
				if (!check_physics_sensor_listener_is_created()) {
					if (!initialize_accelerometer_sensor()
							|| !initialize_gravity_sensor()
							|| !initialize_gyroscope_rotation_vector_sensor()
							|| !initialize_gyroscope_sensor()
							|| !initialize_linear_acceleration_sensor()) {
						dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
								"Failed to get the handle for the default sensor of a Physics sensor.");
						physics_usable = false;
						physics_launched_state = NONE;
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
						physics_launched_state = NONE;
					} else
						dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
								"Succeeded in creating a Physics sensor listener.");

					if (!start_physics_sensor_listener()) {
						dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
								"Failed to start observing the sensor events regarding a Physics sensor listener.");
						physics_usable = false;
						physics_launched_state = NONE;
					} else
						dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
								"Succeeded in starting observing the sensor events regarding a Physics sensor listener.");
				}

				environment_launched_state = LAUNCHED;
				if (!check_environment_sensor_listener_is_created()) {
					if (!initialize_light_sensor() || !initialize_pedometer()
							|| !initialize_pressure_sensor()
							|| !initialize_sleep_monitor()) {
						dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
								"Failed to get the handle for the default sensor of a Environment sensor.");
						environment_usable = false;
						environment_launched_state = NONE;
					} else
						dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
								"Succeeded in getting the handle for the default sensor of a Environment sensor.");

					if (!create_environment_sensor_listener(light_sensor_handle,
							pedometer_handle, pressure_sensor_handle,
							sleep_monitor_handle)) {
						dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
								"Failed to create a Environment sensor listener.");
						environment_usable = false;
						environment_launched_state = NONE;
					} else
						dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
								"Succeeded in creating a Environment sensor listener.");

					if (!start_environment_sensor_listener()) {
						dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
								"Failed to start observing the sensor events regarding a Environment sensor listener.");
						environment_usable = false;
						environment_launched_state = NONE;
					} else
						dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
								"Succeeded in starting observing the sensor events regarding a Environment sensor listener.");
				}
			} else if (mediastorage_result
					== PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ASK) {
				physics_launched_state = REQUEST;
				environment_launched_state = REQUEST;

				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"The user has to be asked whether to grant permission to use a sensor privilege.");

				if (!request_mediastorage_permission()) {
					dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
							"Failed to request a user's response to obtain permission for using the sensor privilege.");
					physics_usable = false;
					environment_usable = false;
					physics_launched_state = NONE;
					environment_launched_state = NONE;
				} else {
					dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
							"Succeeded in requesting a user's response to obtain permission for using the sensor privilege.");
					physics_usable = true;
					environment_usable = true;
					physics_launched_state = ALLOWED;
					environment_launched_state = ALLOWED;
				}
			} else {
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"Function ppm_check_permission() output result = PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_DENY");
				dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
						"The application doesn't have permission to use a sensor privilege.");
				physics_usable = false;
				environment_usable = false;
				physics_launched_state = NONE;
				environment_launched_state = NONE;
			}
		} else {
			/* retval != PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE */
			/* Handle errors */
			dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
					"Function ppm_check_permission() return %s",
					get_error_message(mediastorage_retval));
			physics_usable = false;
			environment_usable = false;
			physics_launched_state = NONE;
			environment_launched_state = NONE;
		}
	}

	return health_usable && physics_usable && environment_usable;
}

bool request_sensor_permission() {
	int health_retval;
	health_retval = ppm_request_permission(sensor_privilege,
			request_sensor_permission_response_callback, NULL);

	if (health_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
		return true;
	} else if (health_retval
			== PRIVACY_PRIVILEGE_MANAGER_ERROR_ALREADY_IN_PROGRESS) {
		return true;
	} else {
		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
				"Function ppm_request_permission() return value = %s",
				get_error_message(health_retval));
		return false;
	}
}
bool request_mediastorage_permission() {
	int mediastorage_retval;
	mediastorage_retval = ppm_request_permission(mediastorage_privilege,
			request_mediastorage_permission_response_callback, NULL);

	if (mediastorage_retval == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
		return true;
	} else if (mediastorage_retval
			== PRIVACY_PRIVILEGE_MANAGER_ERROR_ALREADY_IN_PROGRESS) {
		return true;
	} else {
		dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
				"Function ppm_request_permission() return value = %s",
				get_error_message(mediastorage_retval));
		return false;
	}
}

void request_sensor_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data) {
	dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
			"request_health_sensor_permission_response_callback");
//	if (cause == PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR) {
//		/* Log and handle errors */
//		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//				"Function request_sensor_permission_response_callback() output cause = PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR");
//		dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
//				"Function request_sensor_permission_response_callback() was called because of an error.");
//	} else {
//		dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//				"Function request_sensor_permission_response_callback() was called with a valid answer.");
//
//		switch (result) {
//		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_ALLOW_FOREVER:
//			/* Update UI and start accessing protected functionality */
//			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//					"The user granted permission to use a sensor privilege for an indefinite period of time.");
//
//			hrm_launched_state = LAUNCHED;
//			if (!initialize_hrm_sensor()
//					|| !initialize_hrm_led_green_sensor()) {
//				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
//						"Failed to get the handle for the default sensor of a HRM sensor.");
//				hrm_launched_state = NONE;
////				ui_app_exit();
//			} else
//				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//						"Succeeded in getting the handle for the default sensor of a HRM sensor.");
//
//			if (!create_hrm_sensor_listener(hrm_sensor_handle,
//					hrm_led_green_sensor_handle)) {
//				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
//						"Failed to create a HRM sensor listener.");
//				hrm_launched_state = NONE;
////				ui_app_exit();
//			} else
//				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//						"Succeeded in creating a HRM sensor listener.");
//
//			if (!start_hrm_sensor_listener()) {
//				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
//						"Failed to start observing the sensor events regarding a HRM sensor listener.");
//				hrm_launched_state = NONE;
////				ui_app_exit();
//			} else
//				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//						"Succeeded in starting observing the sensor events regarding a HRM sensor listener.");
//			break;
//		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
//			/* Show a message and terminate the application */
//			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER");
//			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
//					"The user denied granting permission to use a sensor privilege for an indefinite period of time.");
//			hrm_launched_state = NONE;
////			ui_app_exit();
//			break;
//		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
//			/* Show a message with explanation */
//			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
//					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE");
//			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
//					"The user denied granting permission to use a sensor privilege once.");
//			hrm_launched_state = NONE;
////			ui_app_exit();
//			break;
//		}
//	}
}

void request_mediastorage_permission_response_callback(ppm_call_cause_e cause,
		ppm_request_result_e result, const char *privilege, void *user_data) {
	dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
			"request_physics_sensor_permission_response_callback");
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

			hrm_launched_state = LAUNCHED;
			if (!initialize_hrm_sensor()
					|| !initialize_hrm_led_green_sensor()) {
				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
						"Failed to get the handle for the default sensor of a HRM sensor.");
				hrm_launched_state = NONE;
				//				ui_app_exit();
			} else
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"Succeeded in getting the handle for the default sensor of a HRM sensor.");

			if (!create_hrm_sensor_listener(hrm_sensor_handle,
					hrm_led_green_sensor_handle)) {
				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
						"Failed to create a HRM sensor listener.");
				hrm_launched_state = NONE;
				//				ui_app_exit();
			} else
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"Succeeded in creating a HRM sensor listener.");

			if (!start_hrm_sensor_listener()) {
				dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
						"Failed to start observing the sensor events regarding a HRM sensor listener.");
				hrm_launched_state = NONE;
				//				ui_app_exit();
			} else
				dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
						"Succeeded in starting observing the sensor events regarding a HRM sensor listener.");
			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
			/* Show a message and terminate the application */
			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER");
			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege for an indefinite period of time.");
			hrm_launched_state = NONE;
			//			ui_app_exit();
			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
			/* Show a message with explanation */
			dlog_print(DLOG_INFO, HRM_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE");
			dlog_print(DLOG_ERROR, HRM_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege once.");
			hrm_launched_state = NONE;
			//			ui_app_exit();
			break;
		}
	}

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
			physics_launched_state = LAUNCHED;
			environment_launched_state = LAUNCHED;

			dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
					"The user granted permission to use a sensor privilege for an indefinite period of time.");

			if (!initialize_accelerometer_sensor()
					|| !initialize_gravity_sensor()
					|| !initialize_gyroscope_rotation_vector_sensor()
					|| !initialize_gyroscope_sensor()
					|| !initialize_linear_acceleration_sensor()) {
				dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
						"Failed to get the handle for the default sensor of a Physics sensor.");
				physics_launched_state = NONE;
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
				physics_launched_state = NONE;
			} else
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"Succeeded in creating a Physics sensor listener.");

			if (!start_physics_sensor_listener()) {
				dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
						"Failed to start observing the sensor events regarding a Physics sensor listener.");
				physics_launched_state = NONE;
			} else
				dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
						"Succeeded in starting observing the sensor events regarding a Physics sensor listener.");

			if (!initialize_light_sensor() || !initialize_pedometer()
					|| !initialize_pressure_sensor()
					|| !initialize_sleep_monitor()) {
				dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
						"Failed to get the handle for the default sensor of a Environment sensor.");
				environment_launched_state = NONE;
			} else
				dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
						"Succeeded in getting the handle for the default sensor of a Environment sensor.");

			if (!create_environment_sensor_listener(light_sensor_handle,
					pedometer_handle, pressure_sensor_handle,
					sleep_monitor_handle)) {
				dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
						"Failed to create a Environment sensor listener.");
				environment_launched_state = NONE;
			} else
				dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
						"Succeeded in creating a Environment sensor listener.");

			if (!start_environment_sensor_listener()) {
				dlog_print(DLOG_ERROR, ENVIRONMENT_SENSOR_LOG_TAG,
						"Failed to start observing the sensor events regarding a Environment sensor listener.");
				environment_launched_state = NONE;
			} else
				dlog_print(DLOG_INFO, ENVIRONMENT_SENSOR_LOG_TAG,
						"Succeeded in starting observing the sensor events regarding a Environment sensor listener.");

			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
			/* Show a message and terminate the application */
			dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER");
			dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege for an indefinite period of time.");
			physics_launched_state = NONE;
			environment_launched_state = NONE;
//			ui_app_exit();
			break;
		case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
			/* Show a message with explanation */
			dlog_print(DLOG_INFO, PHYSICS_SENSOR_LOG_TAG,
					"Function request_sensor_permission_response_callback() output result = PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE");
			dlog_print(DLOG_ERROR, PHYSICS_SENSOR_LOG_TAG,
					"The user denied granting permission to use a sensor privilege once.");
			physics_launched_state = NONE;
			environment_launched_state = NONE;
//			ui_app_exit();
			break;
		}
	}
}
