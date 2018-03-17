/*
 * ecat.h
 *
 *  Created on: Mar 12, 2018
 *      Author: morris
 */

#ifndef ECAT_H_
#define ECAT_H_

#include <stdint.h>

/* 运行过程中测量分布式时钟的性能 */
#define MEASURE_TIMING

/* 周期性任务1秒钟运行1000次 */
#define FREQUENCY 		1000
/* 用实时时钟作为参考时钟 */
#define CLOCK_TO_USE 	CLOCK_REALTIME

/* 1s = 10^9ns */
#define NSEC_PER_SEC 	(1000000000L)
/* 周期时间换算为纳秒单位 */
#define PERIOD_NS 		(NSEC_PER_SEC / FREQUENCY)

/* 时间差(B-A)，结果换算成纳秒单位 */
#define DIFF_NS(A, B) 	(((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + (B).tv_nsec - (A).tv_nsec)
/* 将时间单位timespec转换为纳秒单位 */
#define TIMESPEC2NS(T) 	((uint64_t)(T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

/**
 * EtherCAT从站在总线上的位置
 */
#define XMC_Slave0Pos 0, 0 /* slave alias, slave position */
#define XMC_Slave1Pos 0, 1 /* slave alias, slave position */

/**
 * 从站厂商信息与产品信息
 */
#define Infineon_XMC4800 0x0000034E, 0x00000000 /* vendor ID, product code */

typedef struct ecat_user_struct {
	/* 控制从站LED的状态值 */
	unsigned int blink0;
	unsigned int blink1;
	/* 从站上传的传感器数据 */
	uint16_t sensor_temp0;
	uint16_t sensor_temp1;
} ecat_user;

int ecat_init(void (*work_callback)(void*));
void ecat_stop(void);

#endif /* ECAT_H_ */
