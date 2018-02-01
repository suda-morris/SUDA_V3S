#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "sqlite3.h"
#include "ecrt.h"
#include "ecat-master.h"

static lv_obj_t * chart_temp = NULL;
static lv_chart_series_t * ch1ser1 = NULL;
static lv_chart_series_t * ch1ser2 = NULL;

void sigalrm_fn(int sig) {
	lv_chart_set_next(chart_temp, ch1ser1, rand() % 100);
	lv_chart_set_next(chart_temp, ch1ser2, rand() % 100);
	alarm(1);
}

static void hal_disp_init(void) {
	/* 初始化Linux Frame Buffer设备 */
	fbdev_init();
	/* 注册显示器驱动 */
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = fbdev_flush; /* 将内部图像缓存刷新到显示器上 */
	lv_disp_drv_register(&disp_drv);
}

int main(void) {
	/* 初始化LittlevGL库 */
	lv_init();
	/* 初始化显示器底层硬件 */
	hal_disp_init();

	/* 创建一个屏幕 */
	lv_obj_t *scr = lv_obj_create(NULL, NULL);
	lv_scr_load(scr);

	/* 初始化alien主题 */
	lv_theme_t *th = lv_theme_alien_init(100, NULL);
	lv_theme_set_current(th);

	/*Create a Tab view object*/
	lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), NULL);

	/*Add 3 tabs (the tabs are page (lv_page) and can be scrolled*/
	lv_obj_t *tab1 = lv_tabview_add_tab(tabview, SYMBOL_HOME);
	lv_obj_t *tab2 = lv_tabview_add_tab(tabview, SYMBOL_IMAGE);
	lv_obj_t *tab3 = lv_tabview_add_tab(tabview, SYMBOL_SETTINGS);

	/* 创建charts */
	chart_temp = lv_chart_create(tab1, NULL);
	lv_obj_set_size(chart_temp, 400, 200);
	lv_obj_align(chart_temp, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_chart_set_type(chart_temp, LV_CHART_TYPE_POINT | LV_CHART_TYPE_LINE);
	lv_chart_set_series_opa(chart_temp, LV_OPA_70);
	lv_chart_set_series_width(chart_temp, 4);
	lv_chart_set_range(chart_temp, 0, 100);
	/* 向charts中插入数据 */
	ch1ser1 = lv_chart_add_series(chart_temp, LV_COLOR_RED);
	ch1ser2 = lv_chart_add_series(chart_temp, LV_COLOR_BLUE);

	/* 创建仪表盘 */
	static lv_color_t needle_colors[] = { LV_COLOR_RED, LV_COLOR_BLUE };
	lv_obj_t * gauge = lv_gauge_create(tab2, NULL);
	lv_gauge_set_needle_count(gauge,
			sizeof(needle_colors) / sizeof(needle_colors[0]), needle_colors);
	lv_obj_align(gauge, NULL, LV_ALIGN_CENTER, 0, 0);
	/* 设置仪表盘指针的数据 */
	lv_gauge_set_value(gauge, 0, 10);
	lv_gauge_set_value(gauge, 1, 20);

	lv_tabview_set_tab_act(tabview, 1, false);

	srand(time(NULL));
	signal(SIGALRM, sigalrm_fn);
	alarm(1);

	while (1) {
		lv_tick_inc(1);
		lv_task_handler();
		sleep(1);
	}

	return 0;
}

