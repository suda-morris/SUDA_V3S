#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main()
{
    int fd;
    int ret;
    char *start = NULL;
    int i;
    char buf[32]; //用户空间的内存

    fd = open("/dev/vfb0", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    /* 第一个参数是想要映射的起始地址，通常设为NULL，表示由内核来决定起始地址
    ** 第二个参数表示要映射的内存空间的大小
    ** 第三个参数表示映射后的空间是可读可写的
    ** 第四个参数表示该映射是多进程共享的 */
    start = mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (start == MAP_FAILED)
    {
        goto fail;
    }
    /* 对映射后的内存进行操作 */
    for (i = 0; i < 26; i++)
    {
        *(start + i) = 'a' + i;
    }
    *(start + i) = '\0';

    if (lseek(fd, 3, SEEK_SET) == -1)
    {
        goto fail;
    }

    if (read(fd, buf, 10) == -1)
    {
        goto fail;
    }
    buf[10] = '\0';
    puts(buf);

    munmap(start, 32);

    exit(EXIT_SUCCESS);
fail:
    perror("mmap test");
    exit(EXIT_FAILURE);
}