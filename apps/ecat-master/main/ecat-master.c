#include <stdio.h>
#include <time.h>
#include "ecrt.h"
#include "ecat-master.h"

static ec_master_t *master = NULL; /* ethercat master对象，管理从站，域以及IOmanage slaves, domains and io */
static ec_master_state_t master_state = { }; /* restore the current state of master */

static ec_domain_t *domain = NULL; /* handles process data of a certain group of slaves */
static ec_domain_state_t domain_state = { }; /* restore the latest state of domain */

static uint8_t *domain_pd = NULL; /* domain process date pointer */

static unsigned int off_sensors_in0; /* offsets for PDO entries: sensor input0 */
static unsigned int off_leds_out0; /* offsets for PDO entries: leds output0 */
static unsigned int off_sensors_in1; /* offsets for PDO entries: sensor input1 */
static unsigned int off_leds_out1; /* offsets for PDO entries: leds output1 */

static unsigned int counter = 0; /* local counter for 1s */
static unsigned int blink = 0; /* led control value */
static unsigned int sync_ref_counter = 0; /* counter for sync to reference clock */
const struct timespec cycletime = { 0, PERIOD_NS }; /* period in linux timespec format */

//////////////////Following parameters are result of command: [ethercat cstruct]//////////////////
/* Master 0, Slave 0
 * Vendor ID:       0x0000034e
 * Product code:    0x00000000
 * Revision number: 0x00000000
 */
/* PDO entry configuration information */
static ec_pdo_entry_info_t xmc_pdo_entries[] = {
/* {entry index, entry subindex, size of entry in bit} */
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

/* PDO configuration information */
static ec_pdo_info_t xmc_pdos[] = {
/* PDO index, number of PDO entries, array of PDO entries */
{ 0x1600, 8, xmc_pdo_entries + 0 }, /* LED process data mapping */
{ 0x1a00, 1, xmc_pdo_entries + 8 }, /* Sensor process data mapping */
};

/* Sync manager configuration information */
static ec_sync_info_t xmc_syncs[] = {
/* sync manager index, sync manager direction, number of PDOs, array of PDOs to assign, watchdog mode */
{ 0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE }, { 1, EC_DIR_INPUT, 0, NULL,
		EC_WD_DISABLE }, { 2, EC_DIR_OUTPUT, 1, xmc_pdos + 0, EC_WD_ENABLE }, {
		3, EC_DIR_INPUT, 1, xmc_pdos + 1, EC_WD_DISABLE }, { 0xff } };

inline struct timespec timespec_add(struct timespec time1,
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

void check_domain_state(void) {
	ec_domain_state_t ds;

	/* read the state of a domain, process data exchange can be monitored in realtime */
	ecrt_domain_state(domain, &ds);
	/* check if change happened in working counter */
	if (ds.working_counter != domain_state.working_counter) {
		printf("domain: WC %u.\n", ds.working_counter);
	}
	/* Working counter interpretation */
	if (ds.wc_state != domain_state.wc_state) {
		printf("domain: State %u.\n", ds.wc_state);
	}
	/* restore the latest domain state */
	domain_state = ds;
}

void check_master_state(void) {
	ec_master_state_t ms;
	/* Read the current master state */
	ecrt_master_state(master, &ms);
	/* check sum of responding slaves on all Ethernet devices */
	if (ms.slaves_responding != master_state.slaves_responding) {
		printf("%u slave(s).\n", ms.slaves_responding);
	}
	/**
	 * check Application-layer states of all slaves. The states are coded in the lower 4 bits.
	 * If a bit is set, it means that at least one slave in the bus is in the corresponding state.
	 * Bit 0: INIT
	 * Bit 1: PREOP
	 * Bit 2: SAFEOP
	 * Bit 3: OP
	 */
	if (ms.al_states != master_state.al_states) {
		printf("AL states: 0x%02X.\n", ms.al_states);
	}
	/* check if at least one link is up */
	if (ms.link_up != master_state.link_up) {
		printf("Link is %s.\n", ms.link_up ? "up" : "down");
	}
	/* restore the current state of master */
	master_state = ms;
}
