#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "vser.h"

int main()
{
    int fd;
    int ret;
    unsigned int baud = 9600;
    struct option opt = {8, 1, 1};

    fd = open("/dev/vser0", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    ret = ioctl(fd, VS_SET_BAUD, &baud);
    if (ret == -1)
    {
        goto fail;
    }
    ret = ioctl(fd, VS_SET_FMT, &opt);
    if (ret == -1)
    {
        goto fail;
    }
    baud = 0;
    opt.datab = 0;
    opt.stopb = 0;
    opt.parity = 0;
    ret = ioctl(fd, VS_GET_BAUD, &baud);
    if (ret == -1)
    {
        goto fail;
    }
    ret = ioctl(fd, VS_GET_FMT, &opt);
    if (ret == -1)
    {
        goto fail;
    }

    printf("baud rate:%d\n", baud);
    printf("frame format:%d%c%d\n", opt.datab, opt.parity == 0 ? 'N' : opt.parity == 1 ? 'O' : 'E', opt.stopb);

    close(fd);
    exit(EXIT_SUCCESS);
fail:
    exit(EXIT_FAILURE);
}