#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include "gui.h"
#include "utils.h"
#include "ecat.h"

static bool app_running = true;

/* 进程的信号处理函数 */
static void signal_handler(int sig) {
	switch (sig) {
	case SIGINT:
		app_running = false;
		break;
	default:
		break;
	}
}

int main(void) {
	/* 注册信号处理函数 */
	signal(SIGINT, signal_handler);

	/* 初始化GUI组件 */
	gui_init();
	/* 初始化工具线程，计算CPU使用率 */
	utils_init();
	/* 启动ethercat主站，注册回调函数 */
	ecat_init(gui_user_work);

	while (app_running) {
		sleep(1);
	}
	ecat_stop();
	utils_stop();
	gui_stop();
	printf("ByeBye\r\n");
	return 0;
}

