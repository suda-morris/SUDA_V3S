/*
 * ecat.c
 *
 *  Created on: Mar 12, 2018
 *      Author: morris
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include "ecat.h"
#include "ecrt.h"

static bool thread_running = true;
/* 互斥锁 */
static pthread_mutex_t mutex;

/* master对象，管理从站，域,IO */
static ec_master_t *master = NULL;
/* master对象的状态 */
static ec_master_state_t master_state = { };

/* 域，管理一组从站的过程数据 */
static ec_domain_t *domain = NULL;
/* 域的状态 */
static ec_domain_state_t domain_state = { };
/* 指向某个域的过程数据 */
static uint8_t *domain_pd = NULL;

static void (*user_work)(void*) = NULL;

/* 过程数据对象中的具体数据的偏移地址 */
static unsigned int off_sensors_in0;
static unsigned int off_leds_out0;
static unsigned int off_sensors_in1;
static unsigned int off_leds_out1;

/* 本地的秒计数器 */
static unsigned int counter = 0;

/* 计数器,用于同步到参考时钟 */
static unsigned int sync_ref_counter = 0;

/* 周期时间 */
const struct timespec cycletime = { 0, PERIOD_NS };

/* 用户数据 */
static ecat_user user_data = { .blink0 = 0, .blink1 = 0, .sensor_temp0 = 0,
		.sensor_temp1 = 0 };

/* 过程数据对象的配置信息，具体到每一条目，可以通过在主站使用命令查看：ethercat cstruct */
/* Master 0, Slave 0
 * Vendor ID:       0x0000034e
 * Product code:    0x00000000
 * Revision number: 0x00000000
 */
static ec_pdo_entry_info_t xmc_pdo_entries[] = {
/* {条目主索引, 条目子索引, 条目大小：单位bit} */
{ 0x7000, 0x01, 1 }, /* LED1 */
{ 0x7000, 0x02, 1 }, /* LED2 */
{ 0x7000, 0x03, 1 }, /* LED3 */
{ 0x7000, 0x04, 1 }, /* LED4 */
{ 0x7000, 0x05, 1 }, /* LED5 */
{ 0x7000, 0x06, 1 }, /* LED6 */
{ 0x7000, 0x07, 1 }, /* LED7 */
{ 0x7000, 0x08, 1 }, /* LED8 */
{ 0x6000, 0x01, 16 }, /* Sensor1 */
};

/* 过程数据对象的配置信息，可以通过在主站使用命令查看：ethercat pdos */
static ec_pdo_info_t xmc_pdos[] = {
/* 过程数据对象的索引, 条目数量, 条目数组 */
{ 0x1600, 8, xmc_pdo_entries + 0 }, /* 代表LED过程数据的映射 */
{ 0x1a00, 1, xmc_pdo_entries + 8 }, /* 代表传感器过程数据的映射 */
};

/* 同步管理单元的配置信息 */
static ec_sync_info_t xmc_syncs[] = {
/* 索引号, 方向, PDO数量, PDO数组, 看门狗模式 */
{ 0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE }, { 1, EC_DIR_INPUT, 0, NULL,
		EC_WD_DISABLE }, { 2, EC_DIR_OUTPUT, 1, xmc_pdos + 0, EC_WD_ENABLE }, {
		3, EC_DIR_INPUT, 1, xmc_pdos + 1, EC_WD_DISABLE }, { 0xff } };

/* 高精度时间相加函数 */
static inline struct timespec timespec_add(struct timespec time1,
		struct timespec time2) {
	struct timespec result;

	if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
		result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
		result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
	} else {
		result.tv_sec = time1.tv_sec + time2.tv_sec;
		result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
	}

	return result;
}

/* 检查域的状态 */
static void check_domain_state(void) {
	ec_domain_state_t ds;

	/* 读取域的状态，能够实时监控到过程数据的交换 */
	ecrt_domain_state(domain, &ds);
	/* 工作计数器是否有改变 */
	if (ds.working_counter != domain_state.working_counter) {
		printf("domain: WC->%u\n", ds.working_counter);
	}
	/* 解释当前的工作状态 */
	if (ds.wc_state != domain_state.wc_state) {
		printf("domain: State->%u\n", ds.wc_state);
	}
	/* 更新状态值 */
	domain_state = ds;
}

/* 检查master对象的状态 */
static void check_master_state(void) {
	ec_master_state_t ms;
	/* 获取当前master的状态 */
	ecrt_master_state(master, &ms);
	/* 检查从站数量是否有变化 */
	if (ms.slaves_responding != master_state.slaves_responding) {
		printf("master: %u slave(s)\n", ms.slaves_responding);
	}
	/* 应用层协议状态 */
	if (ms.al_states != master_state.al_states) {
		printf("master: AL states->%s\n",
				ms.al_states & 0x8 ? "OP" : ms.al_states & 0x4 ? "SAFEOP" :
				ms.al_states & 0x2 ? "PREOP" : "INIT");
	}
	/* 检查是否至少有一个链路已经起来 */
	if (ms.link_up != master_state.link_up) {
		printf("master: Link is %s\n", ms.link_up ? "up" : "down");
	}
	/* 更新状态值 */
	master_state = ms;
}

static void* cyclic_task(void* arg) {
	struct timespec wakeupTime, time;
#ifdef MEASURE_TIMING
	struct timespec startTime, endTime, lastStartTime = { };
	uint32_t period_ns = 0, exec_ns = 0, latency_ns = 0, latency_min_ns = 0,
			latency_max_ns = 0, period_min_ns = 0, period_max_ns = 0,
			exec_min_ns = 0, exec_max_ns = 0;
#endif

	/* 获取当前时间 */
	clock_gettime(CLOCK_TO_USE, &wakeupTime);
	while (thread_running) {
		/* 更新理论上的唤醒时间 */
		wakeupTime = timespec_add(wakeupTime, cycletime);
		/* 高精度睡眠函数 */
		clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);

#ifdef MEASURE_TIMING
		/* 更新实际的唤醒时间 */
		clock_gettime(CLOCK_TO_USE, &startTime);
		/* latency_ns=startTime-wakeupTime */
		latency_ns = DIFF_NS(wakeupTime, startTime);
		/* period_ns=startTime-lastStartTime */
		period_ns = DIFF_NS(lastStartTime, startTime);
		/* exec_ns=endTime-lastStartTime */
		exec_ns = DIFF_NS(lastStartTime, endTime);
		/* 更新lastStartTime */
		lastStartTime = startTime;

		if (latency_ns > latency_max_ns) {
			latency_max_ns = latency_ns;
		}
		if (latency_ns < latency_min_ns) {
			latency_min_ns = latency_ns;
		}
		if (period_ns > period_max_ns) {
			period_max_ns = period_ns;
		}
		if (period_ns < period_min_ns) {
			period_min_ns = period_ns;
		}
		if (exec_ns > exec_max_ns) {
			exec_max_ns = exec_ns;
		}
		if (exec_ns < exec_min_ns) {
			exec_min_ns = exec_ns;
		}
#endif

		/* 从硬件获取数据帧 */
		ecrt_master_receive(master);
		/* 处理接收到的数据帧，解析工作计数器，域的工作状态  */
		ecrt_domain_process(domain);
		/* 检查当前域的工作状态 */
		check_domain_state();
		if (counter) {
			counter--;
		} else {
			/* 下面的任务一秒钟做一次 */
			counter = FREQUENCY;
			/* 检查master的状态 */
			check_master_state();

#ifdef MEASURE_TIMING
			/* 打印时间测量结果 */
			printf("period %*u ... %*u\n", 8, period_min_ns, 10, period_max_ns);
			printf("exec %*u ... %*u\n", 10, exec_min_ns, 10, exec_max_ns);
			printf("latency %*u ... %*u\n", 7, latency_min_ns, 10,
					latency_max_ns);
			period_max_ns = 0;
			period_min_ns = 0xffffffff;
			exec_max_ns = 0;
			exec_min_ns = 0xffffffff;
			latency_max_ns = 0;
			latency_min_ns = 0xffffffff;
#endif
			/* 读取输入的过程数据 */
			user_data.sensor_temp0 = EC_READ_U16(domain_pd + off_sensors_in0);
			user_data.sensor_temp1 = EC_READ_U16(domain_pd + off_sensors_in1);
			user_work((void*) &user_data);
			printf("temp from slave0:%d\ttemp from slave1:%d\r\n",
					user_data.sensor_temp0, user_data.sensor_temp1);
			/* 输出过程数据 */
			EC_WRITE_U8(domain_pd + off_leds_out0, user_data.blink0);
			EC_WRITE_U8(domain_pd + off_leds_out1, user_data.blink1);
		}

		clock_gettime(CLOCK_TO_USE, &time);
		/* 设置应用程序的时间(DC需要) */
		ecrt_master_application_time(master, TIMESPEC2NS(time));

		if (sync_ref_counter) {
			sync_ref_counter--;
		} else {
			/* 每两个周期同步一次 */
			sync_ref_counter = 1;
			/* 提交DC参考时钟偏移补偿报文(即修正DC参考用的时钟),这里的参考时钟同步于本地实时时钟 */
			ecrt_master_sync_reference_clock(master);
		}
		/* 提交DC时钟偏移补偿报文，DC时钟同步于DC参考时钟 */
		ecrt_master_sync_slave_clocks(master);

		/* 标记域中需要发送的数据包 */
		ecrt_domain_queue(domain);
		/* 发送数据包 */
		ecrt_master_send(master);

#ifdef MEASURE_TIMING
		clock_gettime(CLOCK_TO_USE, &endTime);
#endif
	}
	/* 释放互斥锁内存 */
	pthread_mutex_destroy(&mutex);
	pthread_exit(arg);
}

int ecat_init(void (*work_callback)(void*)) {
	int ret;
	pthread_t thread_hdl;
	pthread_attr_t attr;
	/* 保存回调函数 */
	user_work = work_callback;

	/* 将进程使用的部分或者全部的地址空间锁定在物理内存中，防止其被交换到swap空间 */
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
		perror("mlockall failed");
		ret = -1;
		goto mlock_err;
	}
	/* 申请一个master对象 */
	master = ecrt_request_master(0);
	if (!master) {
		perror("ecrt_request_master failed");
		ret = -1;
		goto request_master_err;
	}
	/* 申请一个域来管理过程数据 */
	domain = ecrt_master_create_domain(master);
	if (!domain) {
		perror("create_domain failed");
		ret = -1;
		goto ecat_err;
	}
	printf("Configuring PDOs...\n");
	/* 获取指定从站的配置信息 */
	ec_slave_config_t* sc_xmc = ecrt_master_slave_config(master, XMC_Slave0Pos,
	Infineon_XMC4800);
	if (!sc_xmc) {
		perror("get slave configuration failed");
		ret = -1;
		goto ecat_err;
	}
	/* 初始化一个完整的PDO配置 */
	if (ecrt_slave_config_pdos(sc_xmc, EC_END, xmc_syncs)) {
		perror("config pdos failed");
		ret = -1;
		goto ecat_err;
	}
	/* 向域中注册PDO条目，这样才能实现过程数据的交换 */
	off_leds_out0 = ecrt_slave_config_reg_pdo_entry(sc_xmc, 0x7000, 1, domain,
	NULL);
	if (off_leds_out0 < 0) {
		perror("reg_pdo_entry_err failed");
		ret = -1;
		goto ecat_err;
	}
	off_sensors_in0 = ecrt_slave_config_reg_pdo_entry(sc_xmc, 0x6000, 1, domain,
	NULL);
	if (off_sensors_in0 < 0) {
		perror("reg_pdo_entry_err failed");
		ret = -1;
		goto ecat_err;
	}
	/* 配置分步式时钟，查阅从站描述文件Device->Dc->AssignActivate */
	ecrt_slave_config_dc(sc_xmc, 0x300, PERIOD_NS, 4400000, 0, 0);

	/* 获取指定从站的配置信息 */
	sc_xmc = ecrt_master_slave_config(master, XMC_Slave1Pos, Infineon_XMC4800);
	if (!sc_xmc) {
		perror("get slave configuration failed");
		ret = -1;
		goto ecat_err;
	}
	/* 初始化一个完整的PDO配置 */
	if (ecrt_slave_config_pdos(sc_xmc, EC_END, xmc_syncs)) {
		perror("config pdos failed");
		ret = -1;
		goto ecat_err;
	}
	/* 向域中注册PDO条目，这样才能实现过程数据的交换 */
	off_leds_out1 = ecrt_slave_config_reg_pdo_entry(sc_xmc, 0x7000, 1, domain,
	NULL);
	if (off_leds_out1 < 0) {
		perror("reg_pdo_entry_err failed");
		ret = -1;
		goto ecat_err;
	}
	off_sensors_in1 = ecrt_slave_config_reg_pdo_entry(sc_xmc, 0x6000, 1, domain,
	NULL);
	if (off_sensors_in1 < 0) {
		perror("reg_pdo_entry_err failed");
		ret = -1;
		goto ecat_err;
	}
	/* 配置分步式时钟，查阅从站描述文件Device->Dc->AssignActivate */
	ecrt_slave_config_dc(sc_xmc, 0x300, PERIOD_NS, 4400000, 0, 0);

	printf("Activating master...\n");
	/* 激活主站 */
	if (ecrt_master_activate(master)) {
		perror("master_activate failed");
		ret = -1;
		goto ecat_err;
	}
	/* 获取过程数据的映射地址 */
	if (!(domain_pd = ecrt_domain_data(domain))) {
		perror("get mapped domain data failed");
		ret = -1;
		goto ecat_err;
	}
	/* 设置本进程的优先执行权 */
	pid_t pid = getpid();
	if (setpriority(PRIO_PROCESS, pid, -19)) {
		perror("Failed to set priority");
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
	ret = pthread_create(&thread_hdl, &attr, cyclic_task, NULL);
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
	ecat_err:
	/* 释放master对象 */
	ecrt_release_master(master);
	request_master_err:
	/* 解除对物理内存的锁定 */
	munlockall();
	mlock_err:
	/* 返回错误代码 */
	return ret;
}

void ecat_stop(void) {
	pthread_mutex_lock(&mutex);
	thread_running = false;
	pthread_mutex_unlock(&mutex);
}
