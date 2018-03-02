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
    char rbuf[32] = {0};
    char wbuf[32] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

    fd = open("/dev/vser0", O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
        goto fail;
    }

    ret = read(fd, rbuf, sizeof(rbuf));
    if (ret < 0)
    {
        perror("read");
    }
    ret = write(fd, wbuf, sizeof(wbuf));
    if (ret < 0)
    {
        perror("first write");
    }
    ret = write(fd, wbuf, sizeof(wbuf));
    if (ret < 0)
    {
        perror("second write");
    }

    close(fd);
    exit(EXIT_SUCCESS);
fail:
    exit(EXIT_FAILURE);
}