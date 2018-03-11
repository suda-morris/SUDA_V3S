/*
 * utils.c
 *
 *  Created on: Mar 11, 2018
 *      Author: morris
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "utils.h"

static bool thread_running = true;
static uint8_t usage = 0;

/* 互斥锁 */
static pthread_mutex_t mutex;

void utils_stop(void) {
	pthread_mutex_lock(&mutex);
	thread_running = false;
	pthread_mutex_unlock(&mutex);
}

static void* handle_cpu_usage(void* arg) {
	FILE* fp = NULL;
	uint32_t cpu_user = 0, cpu_nice = 0, cpu_system = 0, cpu_idle = 0,
			cpu_iowait = 0, cpu_irq = 0, cpu_softirq = 0, cpu_stealstolen = 0,
			cpu_guest = 0;
	uint32_t all0 = 0, all1 = 0, idle0 = 0, idle1 = 0;
	char cpu[8];
	pthread_mutex_lock(&mutex);
	while (thread_running) {
		pthread_mutex_unlock(&mutex);
		fp = fopen("/proc/stat", "r");
		fscanf(fp, "%s%d%d%d%d%d%d%d%d%d", cpu, &cpu_user, &cpu_nice,
				&cpu_system, &cpu_idle, &cpu_iowait, &cpu_irq, &cpu_softirq,
				&cpu_stealstolen, &cpu_guest);
		fclose(fp);
		all1 = cpu_user + cpu_nice + cpu_system + cpu_idle + cpu_iowait
				+ cpu_irq + cpu_softirq + cpu_stealstolen + cpu_guest;
		idle1 = cpu_idle;
		pthread_mutex_lock(&mutex);
		usage = (uint8_t) ((float) (all1 - all0 - (idle1 - idle0))
				/ (all1 - all0) * 100);
		pthread_mutex_unlock(&mutex);
		all0 = all1;
		idle0 = idle1;
		sleep(1);
		pthread_mutex_lock(&mutex);
	}
	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
	pthread_exit(arg);
}

void utils_init(void) {
	int ret;
	pthread_t thread_hdl;
	pthread_attr_t attr;

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
	ret = pthread_create(&thread_hdl, &attr, handle_cpu_usage, NULL);
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
	exit(ret);
}

int utils_cpu_usage(uint8_t* arg) {
	pthread_mutex_lock(&mutex);
	*arg = usage;
	pthread_mutex_unlock(&mutex);
	return 0;
}
