#ifndef ECAT_MASTER_H_
#define ECAT_MASTER_H_

#define FREQUENCY 		1000              	/* 周期性任务1秒钟运行1000次 */
#define CLOCK_TO_USE 	CLOCK_REALTIME 		/* 用实时时钟作为参考时钟 */

#define MEASURE_TIMING 	/* 运行过程中测量分布式时钟的性能 */

#define NSEC_PER_SEC 	(10e9)           	/* 1s = 10e9ns */
#define PERIOD_NS 		(NSEC_PER_SEC / FREQUENCY) 	/* 周期时间换算为纳秒单位 */

#define DIFF_NS(A, B) 	(((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + (B).tv_nsec - (A).tv_nsec)	/* 时间差(B-A)，结果换算成纳秒单位 */
#define TIMESPEC2NS(T) 	((uint64_t)(T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)	/* 将时间单位timespec转换为纳秒单位 */

/**
 * EtherCAT从站在总线上的位置
 */
#define XMC_Slave0Pos 0, 0 /* slave alias, slave position */
#define XMC_Slave1Pos 0, 1 /* slave alias, slave position */
/**
 * 厂商标号信息
 */
#define Infineon_XMC4800 0x0000034E, 0x00000000 /* vendor ID, product code */

#endif /* ECAT_MASTER_H_ */
