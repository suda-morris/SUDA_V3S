#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <linux/input.h>

#include "vser.h"

int main()
{
    int ret;
    struct pollfd fds[2];
    char rbuf[32];
    char wbuf[32];
    struct input_event key;

    fds[0].fd = open("/dev/vser0", O_RDWR | O_NONBLOCK);
    if (fds[0].fd == -1)
    {
        goto fail;
    }
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    /* 通过cat /proc/bus/input/devices来查看/dev/input目录下的event对应的设备 */
    /* 这里选择键盘 */
    fds[1].fd = open("/dev/input/event8", O_RDWR | O_NONBLOCK);
    if (fds[1].fd == -1)
    {
        goto fail;
    }
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    while (1)
    {
        /* 第三个参数是毫秒的超时值，负数表示一直监听，直到被监听的文件描述符集合中的任意一个设备发生了事件才会返回 */
        ret = poll(fds, sizeof(fds), -1);
        if (ret == -1)
        {
            goto fail;
        }
        if (fds[0].revents & POLLIN)
        {
            ret = read(fds[0].fd, rbuf, sizeof(rbuf));
            if (ret < 0)
            {
                goto fail;
            }
            puts(rbuf);
        }
        if (fds[1].revents & POLLIN)
        {
            ret = read(fds[1].fd, &key, sizeof(key));
            if (ret < 0)
            {
                goto fail;
            }
            if (key.type == EV_KEY)
            {
                sprintf(wbuf, "%#x\n", key.code);
                ret = write(fds[0].fd, wbuf, strlen(wbuf) + 1);
                if (ret < 0)
                {
                    goto fail;
                }
            }
        }
    }
    exit(EXIT_SUCCESS);
fail:
    perror("poll test");
    exit(EXIT_FAILURE);
}