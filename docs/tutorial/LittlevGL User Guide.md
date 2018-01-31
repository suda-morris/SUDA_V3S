# LittlevGL User Guide

###图形对象---lvgl的基本构建单位

#### Basic Object

> 对象的基本工作机制：父子结构，子随父动，子对象只能在父对象的范围内可见
>
> 基础对象包含了大多数对象需要的属性，比如：坐标，样式，父对象，能否点击，能否拖动等。
>
> 1. 创建对象：`lv_obj_t * lv_type_create(lv_obj_t * parent, lv_obj_t * copy)`
> 2. 删除对象：`void lv_obj_del(lv_obj_t * obj)`(删除本身以及子对象)；`void lv_obj_clean(lv_obj_t * obj)`(删除子对象，保留本身)
> 3. 设置相对于父对象的坐标位置：`lv_obj_set_x(obj, new_x)`，`lv_obj_set_y(obj, new_y)`，`lv_obj_set_pos(obj, new_x, new_y)`
> 4. 设置对象大小：`lv_obj_set_width(obj, new_width)`，`lv_obj_set_height(obj, new_height)`，`lv_obj_set_size(obj, new_width, new_height)`
> 5. 与其余对象对齐：`lv_obj_align(obj1, obj2, LV_ALIGN_TYPE, x_shift, y_shift)`，其中obj2设置为null表示obj1与父对象对齐，第三个参数可以选择`LV_ALIGN_OUT_TOP_MID`，`LV_ALIGN_OUT_BOTTOM_MID`，`LV_ALIGN_CENTER`等
> 6. 设置新的父对象：`lv_obj_set_parent(obj, new_parent)`
> 7. 获取子对象：`lv_obj_get_child(obj, child_prev)`（从最后一个到第一个），`lv_obj_get_child_back(obj, child_prev)`（从第一个到最后一个）
> 8. 创建屏幕：`lv_obj_create(NULL, NULL)`
> 9. 载入屏幕：`lv_scr_load(screen1)`
> 10. 获取当前屏幕：`lv_scr_act()`
> 11. 与屏幕独立的还有两个逻辑层，一个是top layer，一个是system layer，top层在所有的object之上，可以用来设计弹出框等界面；system层在top层之上，可以用来设计鼠标光标。获取top层的方法：`lv_layer_top()`；获取system层的方法：`lv_layer_sys()`
> 12. 为对象设置样式：`lv_obj_set_style(obj, &new_style)`，如果第二个参数是null，表示继承父对象的样式
> 13. 刷新样式：`lv_obj_refresh_style(obj)`（刷新对象的样式）或者`lv_obj_report_style_mod(&style)`（刷新使用该样式的对象），当style参数为null时表示刷新所有对象的样式
> 14. 使能/失能属性：`lv_obj_set_...(obj, true/false)`
>     * **hidden**，如果使能，子对象也会被隐藏
>     * **click**，单击
>     * **top**，如果使能，当被点击时会出现在屏幕最前面
>     * **drag**，拖拽
>     * **drag_throw**，带有冲量效果的拖拽
>     * **drag_parent**，如果使能，父对象将代替子对象被拖拽
> 15. 保护对象不受lvgl库自带的默认动作的影响：`lv_obj_set/clr_protect(obj, LV_PROTECT_...)`：
>     * **LV_PROTECT_NONE**：无保护
>     * **LV_PROTECT_POS**：防止自动布局
>     * **LV_PROTECT_FOLLOW**：防止对象在自动排序中被跟随
>     * **LV_PROTECT_PARENT**：防止跟着父对象自动变化
>     * **LV_PROTECT_CHILD_CHG**：防止改变子对象
> 16. 设置动画：`lv_obj_animate(obj, anim_type, time, delay, callback)`：
>     * **LV_ANIM_FLOAT_TOP** 
>     * **LV_ANIM_FLOAT_LEFT**
>     * **LV_ANIM_FLOAT_BOTTOM**
>     * **LV_ANIM_FLOAT_RIGHT**
>     * **LV_ANIM_GROW_H**
>     * **LV_ANIM_GROW_V**
>     * 上面的枚举变量需要和`ANIM_IN` 或者`ANIM_OUT`进行或操作，再传递给函数



#### Label

> 用来展示文本，对文本大小没有限制，文本内容中可以使用`\n`进行换行
>
> 1. 创建label：`lv_label_create(lv_obj_t * par, lv_obj_t * copy)`
> 2. 修改文本内容：`lv_label_set_text()`
> 3. 设置文本过长时候的模式：`lv_label_set_long_mode(label, long_mode)`
>    * LV_LABEL_LONG_EXPAND
>    * LV_LABEL_LONG_BREAK
>    * LV_LABEL_LONG_DOTS
>    * LV_LABEL_LONG_SCROLL
>    * LV_LABEL_LONG_ROLL
> 4. 从静态数组中加载文本：`lv_label_set_static_text(label, char_array)`
> 5. 从字符数组中加载文本：`lv_label_set_array_text(label, char_array)`
> 6. 设置文本对齐方式：`lv_label_set_align(label, LV_LABEL_ALIGN_LEFT/CENTER)`
> 7. 设置背景颜色：`lv_label_set_body_draw(label, draw)`
> 8. 使能重彩模式：`lv_label_set_recolor()`，然后文本中可以使用命令来加重某些文字的效果，比如`"Write a #ff0000 red# word"`
> 9. 能够使用的样式：
>    1. style.text中的所有样式
>    2. style.body可以用来描述背景色

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

static void hal_disp_init(void) {
	/* 初始化Linux Frame Buffer设备 */
	fbdev_init();
	/* 注册显示器驱动 */
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = fbdev_flush; /* 将内部图像缓存刷新到显示器上 */
	lv_disp_drv_register(&disp_drv);
}

int main(void) {
	/* 初始化LittlevGL库 */
	lv_init();
	/* 初始化显示器底层硬件 */
	hal_disp_init();

	/* 初始化alien主题 */
	lv_theme_t *th = lv_theme_alien_init(350, NULL);

	/* 创建一个屏幕 */
	lv_obj_t *scr = lv_obj_create(NULL, NULL);
	lv_scr_load(scr);

	lv_obj_t * title = lv_label_create(lv_scr_act(), NULL);
	lv_label_set_text(title, SYMBOL_LIST"Title Label");
	lv_obj_align(title, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);

	lv_obj_t * txt = lv_label_create(lv_scr_act(), NULL);
	lv_obj_set_style(txt, th->label.prim); /*Set the style*/
	lv_label_set_long_mode(txt, LV_LABEL_LONG_BREAK); /*Break the long lines*/
	lv_label_set_recolor(txt, true); /*Enable re-coloring by commands in the text*/
	lv_label_set_align(txt, LV_LABEL_ALIGN_CENTER); /*Center aligned lines*/
	lv_label_set_text(txt,
			"Align lines to the middle\n"SYMBOL_FILE"\n"
			"#000080 Re-color# #0000ff words of# #6666ff the text#\n"SYMBOL_GPS"\n"
			"If a line become too long it can be automatically broken into multiple lines");
	lv_obj_set_width(txt, 300); /*Set a width*/
	lv_obj_align(txt, NULL, LV_ALIGN_CENTER, 0, 20);

	while (1) {
		lv_tick_inc(1);
		lv_task_handler();
		usleep(1000);
	}

	return 0;
}
```



### Image

> 它是渲染图片的最基本对象，图片的源文件可以来自内存文件系统（uFS）或者是外部存储设备（比如SD卡）。图片可以在运行的过程中，根据像素的亮度被重新渲染（re-color），这一属性可以通过设置样式中的img.intense来设置。
>
> 1. 创建图片对象 lv_obj_t * lv_img_create(lv_obj_t * par, lv_obj_t * copy);
> 2. 根据图片数据在内存文件系统中创建图片文件lv_fs_res_t lv_img_create_file(const char * fn, const lv_color_int_t * data);
> 3. 给图片设置文件来源void lv_img_set_file(lv_obj_t * img, const char * fn);
> 4. 设置图片自动调整大小 void lv_img_set_auto_size(lv_obj_t * img, bool autosize_en);
> 5.  防止因为“防锯齿”效果导致图片缩小一倍void lv_img_set_upscale(lv_obj_t * img, bool en);
> 6. 设置图片样式 void lv_img_set_style(lv_obj_t *img, lv_style_t *style)
> 7. 获取图片的名字 const char * lv_img_get_file_name(lv_obj_t * img)

1. 图片资源转c文件和bin文件`python img_conv.py -f my_image.png -t`，讲生成的c文件保存在项目工程中
   * img_conv.py的-b参数可以指定生成的bin文件的颜色深度，c文件会生成1位，8位，16位，24位颜色深度的数据
2. 在需要使用该图片的地方生命变量`LV_IMG_DECLARE(img_my_image)`
3. 为该图片在UFS文件系统中创建文件`lv_img_create_file("file_name", img_my_image)`
4. 创建img容器，用来展示图片`lv_obj_t *img = lv_img_create(lv_scr_act(), NULL);`
5. 向img容器中添加图片文件`lv_imag_set_file(img, "U:/file_name");`

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

static void hal_disp_init(void) {
	/* 初始化Linux Frame Buffer设备 */
	fbdev_init();
	/* 注册显示器驱动 */
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = fbdev_flush; /* 将内部图像缓存刷新到显示器上 */
	lv_disp_drv_register(&disp_drv);
}

/* 声明要使用的图片 */
LV_IMG_DECLARE(img_cw);

int main(void) {
	/* 初始化LittlevGL库 */
	lv_init();
	/* 初始化显示器底层硬件 */
	hal_disp_init();

	/* 创建一个屏幕 */
	lv_obj_t *scr = lv_obj_create(NULL, NULL);
	lv_scr_load(scr);

	/* 在内存文件系统中创建文件 */
	lv_img_create_file("cogwheel", img_cw);

	/***************************************
	 * Create three images and re-color them
	 ***************************************/

	/*Create the first image without re-color*/
	lv_obj_t * img1 = lv_img_create(lv_scr_act(), NULL);
	lv_img_set_file(img1, "U:/cogwheel");
	lv_obj_align(img1, NULL, LV_ALIGN_IN_TOP_LEFT, 20, 40);
	lv_img_set_upscale(img1, true);

	/*Create style to re-color with light orange*/
	static lv_style_t style_img2;
	lv_style_copy(&style_img2, &lv_style_plain);
	style_img2.image.color = LV_COLOR_HEX(0xFF8700);
	style_img2.image.intense = LV_OPA_50;

	/*Create an image with the light orange style*/
	lv_obj_t * img2 = lv_img_create(lv_scr_act(), img1);
	lv_obj_set_style(img2, &style_img2);
	lv_obj_align(img2, NULL, LV_ALIGN_IN_TOP_MID, 0, 40);
	lv_img_set_upscale(img2, true);

	/*Create style to re-color with dark orange*/
	static lv_style_t style_img3;
	lv_style_copy(&style_img3, &lv_style_plain);
	style_img3.image.color = LV_COLOR_HEX(0xFF8700);
	style_img3.image.intense = LV_OPA_90;

	/*Create an image with the dark orange style*/
	lv_obj_t * img3 = lv_img_create(lv_scr_act(), img2);
	lv_obj_set_style(img3, &style_img3);
	lv_obj_align(img3, NULL, LV_ALIGN_IN_TOP_RIGHT, -20, 40);
	lv_img_set_upscale(img3, true);

	/**************************************
	 * Create an image with symbols
	 **************************************/

	/*Create a string from symbols*/
	char buf[32];
	sprintf(buf, "%s%s%s%s%s%s%s",
	SYMBOL_DRIVE, SYMBOL_FILE, SYMBOL_DIRECTORY, SYMBOL_SETTINGS,
	SYMBOL_POWER, SYMBOL_GPS, SYMBOL_BLUETOOTH);

	/*Create style with a symbol font*/
	static lv_style_t style_sym;
	lv_style_copy(&style_sym, &lv_style_plain);
	style_sym.text.font = &lv_font_dejavu_40;
	style_sym.text.letter_space = 10;

	/*Create an image and use the string as file name*/
	lv_obj_t * img_sym = lv_img_create(lv_scr_act(), NULL);
	lv_img_set_file(img_sym, buf);
	lv_img_set_style(img_sym, &style_sym);
	lv_obj_align(img_sym, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -30);

	/*Create description labels*/
	lv_obj_t * label = lv_label_create(lv_scr_act(), NULL);
	lv_label_set_text(label, "Re-color the images in run time");
	lv_obj_align(label, NULL, LV_ALIGN_IN_TOP_MID, 0, 15);

	label = lv_label_create(lv_scr_act(), NULL);
	lv_label_set_text(label, "Use symbols from fonts as images");
	lv_obj_align(label, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -80);

	while (1) {
		lv_tick_inc(1);
		lv_task_handler();
		usleep(1000);
	}

	return 0;
}
```



#### line

> 线对象根据点数组来绘制，所有的点保存在 `lv_point_t` 类型的数组中。
>
> 1. 创建line对象lv_obj_t * lv_line_create(lv_obj_t * par, lv_obj_t * copy);
> 2. 设置需要绘制在线上的点 void lv_line_set_points(lv_obj_t * line, const lv_point_t * point_a, uint16_t point_num);
> 3. 设置对象自动调整大小以适应线的大小void lv_line_set_auto_size(lv_obj_t * line, bool autosize_en);
> 4. y轴是否反序 void lv_line_set_y_invert(lv_obj_t * line, bool yinv_en);
> 5. 设置样式 void lv_line_set_style(lv_obj_t *line, lv_style_t *style)

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

static void hal_disp_init(void) {
	/* 初始化Linux Frame Buffer设备 */
	fbdev_init();
	/* 注册显示器驱动 */
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = fbdev_flush; /* 将内部图像缓存刷新到显示器上 */
	lv_disp_drv_register(&disp_drv);
}

int main(void) {
	/* 初始化LittlevGL库 */
	lv_init();
	/* 初始化显示器底层硬件 */
	hal_disp_init();

	/* 创建一个屏幕 */
	lv_obj_t *scr = lv_obj_create(NULL, NULL);
	lv_scr_load(scr);

	/*Create an array for the points of the line*/
	static lv_point_t line_points[] = { { 5, 5 }, { 70, 70 }, { 120, 10 }, {
			180, 60 }, { 240, 10 } };

	/*Create line with default style*/
	lv_obj_t * line1;
	line1 = lv_line_create(lv_scr_act(), NULL);
	lv_line_set_points(line1, line_points, 5); /*Set the points*/
	lv_obj_align(line1, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);

	/*Create new style (thin light blue)*/
	static lv_style_t style_line2;
	lv_style_copy(&style_line2, &lv_style_plain);
	style_line2.line.color = LV_COLOR_MAKE(0xEE, 0x00, 0xFF);
	style_line2.line.width = 2;

	/*Copy the previous line and apply the new style*/
	lv_obj_t * line2 = lv_line_create(lv_scr_act(), line1);
	lv_line_set_style(line2, &style_line2);
	lv_obj_align(line2, line1, LV_ALIGN_OUT_BOTTOM_MID, 0, -20);

	/*Create new style (thick dark blue)*/
	static lv_style_t style_line3;
	lv_style_copy(&style_line3, &lv_style_plain);
	style_line3.line.color = LV_COLOR_MAKE(0x00, 0x3b, 0x75);
	style_line3.line.width = 5;

	/*Copy the previous line and apply the new style*/
	lv_obj_t * line3 = lv_line_create(lv_scr_act(), line1);
	lv_line_set_style(line3, &style_line3);
	lv_obj_align(line3, line2, LV_ALIGN_OUT_BOTTOM_MID, 0, -20);

	while (1) {
		lv_tick_inc(1);
		lv_task_handler();
		usleep(1000);
	}

	return 0;
}
```



#### container

> container是一个长方形的容器，可以对其设置layout布局，来更好的组织其囊括的子对象。支持的样式有：style.body
>
> 1. 窗件一个容器 lv_obj_t * lv_cont_create(lv_obj_t * par, lv_obj_t * copy);
> 2. 设置布局void lv_cont_set_layout(lv_obj_t * cont, lv_layout_t layout);
> 3. 设置大小自适应 void lv_cont_set_fit(lv_obj_t * cont, bool hor_en, bool ver_en);
> 4. 设置样式 void lv_cont_set_style(lv_obj_t *cont, lv_style_t * style)

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

static void hal_disp_init(void) {
	/* 初始化Linux Frame Buffer设备 */
	fbdev_init();
	/* 注册显示器驱动 */
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = fbdev_flush; /* 将内部图像缓存刷新到显示器上 */
	lv_disp_drv_register(&disp_drv);
}

int main(void) {
	/* 初始化LittlevGL库 */
	lv_init();
	/* 初始化显示器底层硬件 */
	hal_disp_init();

	/* 创建一个屏幕 */
	lv_obj_t *scr = lv_obj_create(NULL, NULL);
	lv_scr_load(scr);

	/* 创建一个容器 */
	lv_obj_t * box1;
	box1 = lv_cont_create(lv_scr_act(), NULL);
	lv_obj_set_style(box1, &lv_style_pretty);
	lv_cont_set_fit(box1, true, true);

	/*Add a text to the container*/
	lv_obj_t * txt = lv_label_create(box1, NULL);
	lv_label_set_text(txt, "Lorem ipsum dolor\n"
			"sit amet, consectetur\n"
			"adipiscing elit, sed do\n"
			"eiusmod tempor incididunt\n"
			"ut labore et dolore\n"
			"magna aliqua.");

	lv_obj_align(box1, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10); /*Align the container*/

	/*Create a style*/
	static lv_style_t style;
	lv_style_copy(&style, &lv_style_pretty_color);
	style.body.shadow.width = 6;
	style.body.padding.hor = 5; /*Set a great horizontal padding*/

	/*Create an other container*/
	lv_obj_t * box2;
	box2 = lv_cont_create(lv_scr_act(), NULL);
	lv_obj_set_style(box2, &style); /*Set the new style*/
	lv_cont_set_fit(box2, true, false); /*Do not enable the vertical fit */
	lv_obj_set_height(box2, 55); /*Set a fix height*/

	/*Add a text to the new container*/
	lv_obj_t * txt2 = lv_label_create(box2, NULL);
	lv_label_set_text(txt2, "No vertical fit 1...\n"
			"No vertical fit 2...\n"
			"No vertical fit 3...\n"
			"No vertical fit 4...");

	/*Align the container to the bottom of the previous*/
	lv_obj_align(box2, box1, LV_ALIGN_OUT_BOTTOM_MID, 30, -30);
	while (1) {
		lv_tick_inc(1);
		lv_task_handler();
		usleep(1000);
	}

	return 0;
}
```



#### chart

> 用来展示基本的图表，包括折线图，柱状图，散点图。
>
> 1. 创建图表对象 lv_obj_t * lv_chart_create(lv_obj_t * par, lv_obj_t * copy);
> 2. 向图表中增加一组数据lv_chart_series_t * lv_chart_add_series(lv_obj_t * chart, lv_color_t color);
> 3. 设置网格线的数目 void lv_chart_set_div_line_count(lv_obj_t * chart, uint8_t hdiv, uint8_t vdiv);
> 4. 设置图表数据的范围 void lv_chart_set_range(lv_obj_t * chart, lv_coord_t ymin, lv_coord_t ymax);
> 5. 设置图表的类型 void lv_chart_set_type(lv_obj_t * chart, lv_chart_type_t type);
>    * LV_CHART_TYPE_NONE
>    * LV_CHART_TYPE_LINE
>    * LV_CHART_TYPE_COL
>    * LV_CHART_TYPE_POINT
> 6. 设置图表展示的数据个数 void lv_chart_set_point_count(lv_obj_t * chart, uint16_t point_cnt);
> 7. 设置透明度 void lv_chart_set_series_opa(lv_obj_t * chart, lv_opa_t opa);
> 8. 设置数据线宽度 void lv_chart_set_series_width(lv_obj_t * chart, lv_coord_t width);
> 9. 设置暗影效果void lv_chart_set_series_darking(lv_obj_t * chart, lv_opa_t dark_eff);
> 10. 初始化所有数据为相同的值void lv_chart_init_points(lv_obj_t * chart, lv_chart_series_t * ser, lv_coord_t y);
> 11. 从数组中加载数据进行显示 void lv_chart_set_points(lv_obj_t * chart, lv_chart_series_t * ser, lv_coord_t * y_array);
> 12. 加入一个新数据，旧的数据向左移动一格 void lv_chart_set_next(lv_obj_t * chart, lv_chart_series_t * ser, lv_coord_t y);
> 13. 设置样式 void lv_chart_set_style(lv_obj_t *chart, lv_style_t *style)
> 14. 刷新数据界面 void lv_chart_refresh(lv_obj_t * chart);

```c

```





