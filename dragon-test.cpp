#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <stdint.h>
#include <cstring>
#include <sys/time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <cerrno>
#include <sys/types.h>

#include "../dragon-module/dragon.h"

#define DRAGON_DEV_FILENAME "/dev/dragon0"

#define DRAGON_BUFFER_COUNT 5

#define LOOPS_COUNT 100

// get system time in milliseconds
unsigned long GetTickCount()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (tv.tv_sec*1000+tv.tv_usec/1000);
}



//main function, here main PCIE thread runs
int main(int argc, char** argv)
{
    int DragonDevHandle; //handle for opening PCIE device file
    unsigned int i;
    dragon_buffer buf;
    size_t buf_count = DRAGON_BUFFER_COUNT;
    unsigned char* user_bufs[DRAGON_BUFFER_COUNT];
    unsigned long dtStart, dtEnd;

    //open PCIE device
    DragonDevHandle = open(DRAGON_DEV_FILENAME, O_RDWR);

    if(DragonDevHandle<0)
    {
        puts("Failed to open PCIE device!\n");
        return -1;
    }

    ioctl(DragonDevHandle, DRAGON_SET_ACTIVITY, 0);

    if (!ioctl(DragonDevHandle, DRAGON_REQUEST_BUFFERS, &buf_count) )
    {
        printf("buf_count = %ld\n", buf_count);
    }
    else
    {
        printf("DRAGON_REQUEST_BUFFERS error\n");
        return -1;
    }


    for (i = 0; i < buf_count; ++i)
    {
        buf.idx = i;

        if (ioctl(DragonDevHandle, DRAGON_QUERY_BUFFER, &buf) )
        {
            printf("DRAGON_QUERY_BUFFER %d error\n", i);
            return -1;
        }

        user_bufs[i] = (unsigned char*)mmap(NULL, buf.len,
                                            PROT_READ | PROT_WRITE,
                                            MAP_SHARED, DragonDevHandle,
                                            buf.offset);

        if (!user_bufs[i])
        {
            printf("mmap buffer %d error\n", i);
            return -1;
        }


        if (ioctl(DragonDevHandle, DRAGON_QBUF, &buf) )
        {
            printf("DRAGON_QBUF error\n");
            return -1;
        }
    }

    ioctl(DragonDevHandle, DRAGON_SET_ACTIVITY, 1);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(DragonDevHandle, &fds);

    dtStart = GetTickCount();
    int count = 0;
    for (;;)
    {
        select(DragonDevHandle + 1, &fds, NULL, NULL, NULL);

        //Dequeue buffer

        if (ioctl(DragonDevHandle, DRAGON_DQBUF, &buf) )
        {
            printf("DRAGON_DQBUF error\n");
            continue;
        }

        //Do something with data
        memset(user_bufs[buf.idx], 0, buf.len);

        //Queue buffer
        if (ioctl(DragonDevHandle, DRAGON_QBUF, &buf) )
        {
            printf("DRAGON_QBUF error\n");
            return -1;
        }

        ++count;
        if (count % LOOPS_COUNT == 0)
        {
            dtEnd = GetTickCount();
            double  FPS = 1000*LOOPS_COUNT / (double)(dtEnd - dtStart);
            count = 0;
            dtStart = dtEnd;
            printf("FPS = %lf\n", FPS);
        }
    }




    close(DragonDevHandle);

    return 0;
}

