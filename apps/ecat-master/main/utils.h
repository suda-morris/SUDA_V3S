/*
 * utils.h
 *
 *  Created on: Mar 11, 2018
 *      Author: morris
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>

int utils_init(void);
int utils_cpu_usage(uint8_t* arg);
void utils_stop(void);

#endif /* UTILS_H_ */
