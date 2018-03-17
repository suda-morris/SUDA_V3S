/*
 * gui.c
 *
 *  Created on: Mar 16, 2018
 *      Author: morris
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "lv_drivers/display/fbdev.h"
#include "indev_tslib.h"
#include "gui.h"
#include "ecat.h"
#include "utils.h"

static lv_obj_t * chart_temp = NULL;
static lv_chart_series_t * ch1ser1 = NULL;
static lv_chart_series_t * ch1ser2 = NULL;
static lv_obj_t * gauge = NULL;

static bool thread_running = true;
/* 互斥锁 */
static pthread_mutex_t mutex;

static void hal_disp_init(void) {
	/* 初始化Linux Frame Buffer设备 */
	fbdev_init();
	/* 注册显示器驱动 */
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = fbdev_flush; /* 将内部图像缓存刷新到显示器上 */
	lv_disp_drv_register(&disp_drv);
}

static void hal_input_init(void) {
	/* 初始化输入设备和tslib中间件 */
	indev_init();
	/* 注册输入设备 */
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read = indev_ts_read;
	lv_indev_drv_register(&indev_drv);
}

static void charts_temp(lv_obj_t* parent) {
	chart_temp = lv_chart_create(parent, NULL);
	lv_obj_set_size(chart_temp, 400, 200);
	lv_obj_align(chart_temp, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_chart_set_type(chart_temp, LV_CHART_TYPE_POINT | LV_CHART_TYPE_LINE);
	lv_chart_set_series_opa(chart_temp, LV_OPA_70);
	lv_chart_set_series_width(chart_temp, 4);
	lv_chart_set_range(chart_temp, 0, 1000);
	/* 向charts中插入数据 */
	ch1ser1 = lv_chart_add_series(chart_temp, LV_COLOR_RED);
	ch1ser2 = lv_chart_add_series(chart_temp, LV_COLOR_BLUE);
}

static void guage_cpu(lv_obj_t* parent) {
	static lv_color_t needle_colors[] = { LV_COLOR_RED };
	gauge = lv_gauge_create(parent, NULL);
	lv_gauge_set_needle_count(gauge,
			sizeof(needle_colors) / sizeof(needle_colors[0]), needle_colors);
	lv_obj_align(gauge, NULL, LV_ALIGN_CENTER, 0, 0);
	/* 设置仪表盘指针的数据 */
	lv_gauge_set_value(gauge, 0, 50);
}

static void control_panel(lv_obj_t* parent) {
	lv_obj_t* led = lv_led_create(parent, NULL);
	lv_obj_align(led, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_led_on(led);
}

static void* gui_tick(void *arg) {
	pthread_mutex_lock(&mutex);
	while (thread_running) {
		pthread_mutex_unlock(&mutex);
		lv_tick_inc(1);
		lv_task_handler();
		usleep(1000);
		pthread_mutex_lock(&mutex);
	}
	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
	pthread_exit(arg);
}

int gui_init(void) {
	int ret;
	pthread_t thread_hdl;
	pthread_attr_t attr;

	/* 初始化LittlevGL库 */
	lv_init();
	/* 初始化显示器底层硬件 */
	hal_disp_init();
	/* 初始化输入设备底层硬件 */
	hal_input_init();

	/* 创建一个屏幕 */
	lv_obj_t *scr = lv_obj_create(NULL, NULL);
	lv_scr_load(scr);

	/* 初始化alien主题 */
	lv_theme_t *th = lv_theme_alien_init(100, NULL);
	lv_theme_set_current(th);

	/* 创建Tab视图容器 */
	lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), NULL);

	/* 创建三个标签选项卡 */
	lv_obj_t *tab1 = lv_tabview_add_tab(tabview, SYMBOL_LOOP);
	lv_obj_t *tab2 = lv_tabview_add_tab(tabview, SYMBOL_HOME);
	lv_obj_t *tab3 = lv_tabview_add_tab(tabview, SYMBOL_SETTINGS);

	/* 创建仪表盘 */
	guage_cpu(tab1);
	/* 创建charts */
	charts_temp(tab2);
	/* 创建控制面板 */
	control_panel(tab3);
	/* 设置默认显示页 */
	lv_tabview_set_tab_act(tabview, 1, false);

	/* 初始化互斥锁 */
	pthread_mutex_init(&mutex, NULL);
	/* 设置线程的分离属性 */
	pthread_attr_init(&attr);
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret) {
		perror("pthread_attr_setdetachstate");
		goto err;
	}
	/* 设置线程的绑定属性 */
	ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	if (ret) {
		perror("pthread_attr_setscope");
		goto err;
	}
	/* 创建线程 */
	ret = pthread_create(&thread_hdl, &attr, gui_tick, NULL);
	if (ret) {
		perror("pthread_create");
		goto err;
	}
	/* 释放线程属性对象 */
	pthread_attr_destroy(&attr);
	return 0;
	err:
	/* 释放线程属性对象 */
	pthread_attr_destroy(&attr);
	/* 释放互斥锁对象 */
	pthread_mutex_destroy(&mutex);
	return ret;
}

void gui_stop(void) {
	indev_stop();
	pthread_mutex_lock(&mutex);
	thread_running = false;
	pthread_mutex_unlock(&mutex);
}

void gui_user_work(void* arg) {
	ecat_user* user_data = (ecat_user*) arg;
	uint8_t usage;
	utils_cpu_usage(&usage);
	lv_gauge_set_value(gauge, 0, usage);
	lv_chart_set_next(chart_temp, ch1ser1, user_data->sensor_temp0);
	lv_chart_set_next(chart_temp, ch1ser2, user_data->sensor_temp1);
	user_data->blink0++;
	user_data->blink1++;
}

