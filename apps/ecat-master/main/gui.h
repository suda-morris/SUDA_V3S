/*
 * gui.h
 *
 *  Created on: Mar 16, 2018
 *      Author: morris
 */

#ifndef GUI_H_
#define GUI_H_

#include "lvgl/lvgl.h"

int gui_init(void);
void gui_stop(void);
void gui_user_work(void* arg);

#endif /* GUI_H_ */
