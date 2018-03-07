#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "led.h"

int main()
{
    int fd;
    int ret;
    int num = LED_RED;

    fd = open("/dev/led", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    while (1)
    {
        ret = ioctl(fd, LED_ON, num);
        if (ret == -1)
        {
            goto fail;
        }
        usleep(500000);
        ret = ioctl(fd, LED_OFF, num);
        if (ret == -1)
        {
            goto fail;
        }
        usleep(500000);

        num = (num + 1) % 3;
    }
    exit(EXIT_SUCCESS);
fail:
    perror("led test");
    exit(EXIT_FAILURE);
}