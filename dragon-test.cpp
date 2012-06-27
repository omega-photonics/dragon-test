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

#define DRAGON_DEV_FILENAME "/dev/dragon"

#define FrameLength 9000
#define FramesPerBuffer 3
#define PacketCount (FrameLength*FramesPerBuffer/90)

#define USE_BUFFERS 4

//main function, here main PCIE thread runs
int main(int argc, char** argv)
{
    int DragonDevHandle; //handle for opening PCIE device file
    unsigned char* PcieInputDataShadows[DRAGON_BUFFER_COUNT]; //here we map kernel memory area which PCIE device fills with data via DMA
    unsigned int i, j, k;

    //open PCIE device
    DragonDevHandle = open(DRAGON_DEV_FILENAME, O_RDWR);
    if(DragonDevHandle<0)
    {
        puts("Failed to open PCIE device!\n");
        return -1;
    }

    for(i=0; i<USE_BUFFERS; i++)
    {
        ioctl(DragonDevHandle, DRAGON_REQUEST_BUFFER_NUMBER, i);
        PcieInputDataShadows[i] = (unsigned char*)mmap(0, DRAGON_BUFFER_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, DragonDevHandle, 0);
    }

    ioctl(DragonDevHandle, DRAGON_SET_FRAME_LENGTH, FrameLength);
    ioctl(DragonDevHandle, DRAGON_SET_FRAME_PER_BUFFER_COUNT, FramesPerBuffer);
    ioctl(DragonDevHandle, DRAGON_START, 0);
    ioctl(DragonDevHandle, DRAGON_START, 1);

    while(true)
    {
//        ioctl(DragonDevHandle, DRAGON_START, 0);
        for(k=0; k<USE_BUFFERS; k++)
            ioctl(DragonDevHandle, DRAGON_QUEUE_BUFFER, k);
//        ioctl(DragonDevHandle, DRAGON_START, 1);

        usleep(1000000);

        for(k=0; k<USE_BUFFERS; k++)
        {
            printf("\n\nbuffer: %d", k);
            for(i=0; i<PacketCount*DRAGON_PACKET_SIZE_BYTES; i+=DRAGON_PACKET_SIZE_BYTES)
            {
                printf("\n%d\tpckt:\t%d\tf:%d,\tsextets: ", i/DRAGON_PACKET_SIZE_BYTES,
                       ((0x0F&PcieInputDataShadows[k][i])<<24) | (PcieInputDataShadows[k][i+1]<<16) |
                       (PcieInputDataShadows[k][i+2]<<8) | PcieInputDataShadows[k][i+3], (PcieInputDataShadows[k][i]&128)>>7);

                for(j=0; j<15; j++)
                    printf("%d\t", ((PcieInputDataShadows[k][i+4+j*8]&31)<<8) | PcieInputDataShadows[k][i+5+j*8]);
            }
        }
        puts("");
    }

    return 0;
}

