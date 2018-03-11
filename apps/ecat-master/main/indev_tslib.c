/*
 * indev_tslib.c
 *
 *  Created on: Mar 11, 2018
 *      Author: morris
 */
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include "tslib.h"
#include "indev_tslib.h"

static bool thread_running = true;
static bool press_down = false;
static int16_t last_x = 0;
static int16_t last_y = 0;

/* 互斥锁 */
static pthread_mutex_t mutex;

void indev_stop(void) {
	pthread_mutex_lock(&mutex);
	thread_running = false;
	pthread_mutex_unlock(&mutex);
}

/* 线程：用来读取最新的触摸输入 */
static void *handle_input(void *arg) {
	struct ts_sample samp;
	int ret;
	struct tsdev *ts = (struct tsdev*) arg;
	pthread_mutex_lock(&mutex);
	while (thread_running) {
		pthread_mutex_unlock(&mutex);
		ret = ts_read_raw(ts, &samp, 1);
		if (ret < 0) {
			perror("ts_read_raw");
			indev_stop();
			goto read_err;
		}
		if (ret != 1) {
			pthread_mutex_lock(&mutex);
			continue;
		}
		pthread_mutex_lock(&mutex);
		press_down = samp.pressure ? true : false;
		if (press_down) {
			last_x = samp.x;
			last_y = samp.y;
		}
	}
	pthread_mutex_unlock(&mutex);
read_err:
	/* 出错处理*/
	pthread_mutex_destroy(&mutex);
	ts_close(ts);
	pthread_exit(arg);
}

void indev_init(void) {
	int ret;
	pthread_t thread_hdl;
	pthread_attr_t attr;
	struct tsdev *ts;
	/* 打开并配置触摸屏设备 */
	ts = ts_setup(NULL, 0);
	if (!ts) {
		perror("ts_setup");
		exit(1);
	}
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
	ret = pthread_create(&thread_hdl, &attr, handle_input, (void*) ts);
	if (ret) {
		perror("pthread_create");
		goto err;
	}
	/* 释放线程属性对象 */
	pthread_attr_destroy(&attr);
	return;
err:
	/* 出错处理 */
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mutex);
	ts_close(ts);
	exit(ret);
}

/* indev_ts_read在主线程中被调用 */
bool indev_ts_read(lv_indev_data_t *data) {
	pthread_mutex_lock(&mutex);
	data->point.x = last_x;
	data->point.y = last_y;
	data->state = press_down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
	pthread_mutex_unlock(&mutex);
	return false;
}
