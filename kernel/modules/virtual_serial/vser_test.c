#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>

#include "vser.h"

int fd;

static void sigio_handler(int signum, siginfo_t *siginfo, void *act)
{
    int ret;
    char buf[32];
    if (signum == SIGIO)
    {
        /* si_band记录着资源是可读还是可写 */
        if (siginfo->si_band & POLLIN)
        {
            printf("FIFO is not empty\n");
            ret = read(fd, buf, sizeof(buf));
            if (ret != -1)
            {
                buf[ret] = '\0';
                puts(buf);
            }
        }
        if (siginfo->si_band & POLLOUT)
        {
            printf("FIFO is not full\n");
        }
    }
}

int main()
{
    int ret;
    int flag;

    struct sigaction newact, oldact;

    /* sigaction比signal更高级，主要是信号阻塞和提供信号信息这两方面 */
    /* 设置信号处理函数中要屏蔽的信号，防止嵌套 */
    sigemptyset(&newact.sa_mask);
    sigaddset(&newact.sa_mask, SIGIO);
    /* 注册信号函数 */
    newact.sa_flags = SA_SIGINFO;
    newact.sa_sigaction = sigio_handler;
    if (sigaction(SIGIO, &newact, &oldact) == -1)
    {
        goto fail;
    }

    fd = open("/dev/vser0", O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
        goto fail;
    }
    /* 设置文件属主，从而驱动可以根据file结构来找到对应的进程 */
    if (fcntl(fd, F_SETOWN, getpid()) == -1)
    {
        goto fail;
    }
    /* F_SETSIG不是POSIX标准，需要在编译时候加上-D_GNU_SOURCE */
    /* 当设备资源可用时，向进程发送SIGIO信号 */
    if (fcntl(fd, F_SETSIG, SIGIO) == -1)
    {
        goto fail;
    }
    /* 使能异步通知机制 */
    flag = fcntl(fd, F_GETFL);
    if (flag == -1)
    {
        goto fail;
    }
    /* 会触发调用驱动中的fasync接口函数 */
    if (fcntl(fd, F_SETFL, flag | FASYNC) == -1)
    {
        goto fail;
    }

    while (1)
    {
        sleep(1);
    }
    exit(EXIT_SUCCESS);
fail:
    perror("fasync test");
    exit(EXIT_FAILURE);
}