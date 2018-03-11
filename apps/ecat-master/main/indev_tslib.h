/*
 * indev_tslib.h
 *
 *  Created on: Mar 11, 2018
 *      Author: morris
 */

#ifndef INDEV_TSLIB_H_
#define INDEV_TSLIB_H_

#include "lvgl/lv_hal/lv_hal_indev.h"

void indev_init(void);
bool indev_ts_read(lv_indev_data_t *data);
void indev_stop(void);

#endif /* INDEV_TSLIB_H_ */
