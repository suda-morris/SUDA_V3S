#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "indev_tslib.h"
#include "utils.h"

static bool app_running = true;

static lv_obj_t * chart_temp = NULL;
static lv_chart_series_t * ch1ser1 = NULL;
static lv_chart_series_t * ch1ser2 = NULL;
static lv_obj_t * gauge = NULL;

/* 进程的信号处理函数 */
static void signal_handler(int sig) {
	uint8_t usage;
	switch (sig) {
	case SIGALRM:
		lv_chart_set_next(chart_temp, ch1ser1, rand() % 100);
		lv_chart_set_next(chart_temp, ch1ser2, rand() % 100);
		utils_cpu_usage(&usage);
		lv_gauge_set_value(gauge, 0, usage);
		alarm(1);
		break;
	case SIGINT:
		app_running = false;
		break;
	default:
		break;
	}
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
	lv_chart_set_range(chart_temp, 0, 100);
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

	utils_init();
}

static void control_panel(lv_obj_t* parent) {
	lv_obj_t* led = lv_led_create(parent, NULL);
	lv_obj_align(led, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_led_on(led);
}

int main(void) {
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
	lv_obj_t *tab1 = lv_tabview_add_tab(tabview, SYMBOL_HOME);
	lv_obj_t *tab2 = lv_tabview_add_tab(tabview, SYMBOL_IMAGE);
	lv_obj_t *tab3 = lv_tabview_add_tab(tabview, SYMBOL_SETTINGS);

	/* 创建charts */
	charts_temp(tab1);
	/* 创建仪表盘 */
	guage_cpu(tab2);
	/* 创建控制面板 */
	control_panel(tab3);

	/* 注册信号处理函数 */
	signal(SIGINT, signal_handler);
	srand(time(NULL));
	signal(SIGALRM, signal_handler);
	alarm(1);

	while (app_running) {
		lv_tick_inc(1);
		lv_task_handler();
		usleep(1000);
	}
	indev_stop();
	utils_stop();
	printf("ByeBye\r\n");
	return 0;
}

