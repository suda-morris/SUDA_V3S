#pragma once

struct option
{
    unsigned int datab;
    unsigned int parity;
    unsigned int stopb;
};

#define VS_MAGIC 's'

#define VS_SET_BAUD _IOW(VS_MAGIC, 0, unsigned int)
#define VS_GET_BAUD _IOR(VS_MAGIC, 1, unsigned int)
#define VS_SET_FMT _IOW(VS_MAGIC, 2, struct option)
#define VS_GET_FMT _IOR(VS_MAGIC, 3, struct option)
