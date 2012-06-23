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

#define START_PULSE_WIDTH 5 // (127 max)

#define DRAGON_DEFAULT_FRAME_LENGTH 8192	// (8192 max) - real frame point count per channel is 6x larger, but device just counts by 6 points
#define DRAGON_DEFAULT_FRAME_COUNT 10000
#define DRAGON_DEFAULT_DAC_DATA 0xFFFFFFFF  //each byte for one of four 8-bit DACs

uint16_t FrameLength = DRAGON_DEFAULT_FRAME_LENGTH; //how many points in one input (and ouput) frame we have
uint32_t FrameCount =  DRAGON_DEFAULT_FRAME_COUNT; //how many frames we want to sum to get output data
uint32_t PcieDacData = DRAGON_DEFAULT_DAC_DATA; //DAC data to be set on PCIE device

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
    unsigned char* PcieInputDataShadows[DRAGON_BUFFER_COUNT]; //here we map kernel memory area which PCIE device fills with data via DMA
    unsigned int i, j, k;
    uint32_t CONFIG_REG_1 = (1<<21) | ((START_PULSE_WIDTH<<13) & 127) | ((FrameLength-1) & 8191);

    //open PCIE device
    DragonDevHandle = open(DRAGON_DEV_FILENAME, O_RDWR);
    if(DragonDevHandle<0)
    {
        puts("Failed to open PCIE device!\n");
        return -1;
    }

    for(i=0; i<DRAGON_BUFFER_COUNT; i++)
    {
        ioctl(DragonDevHandle, DRAGON_REQUEST_BUFFER_NUMBER, i);
        PcieInputDataShadows[i] = (unsigned char*)mmap(0, DRAGON_BUFFER_SIZE, PROT_READ, MAP_SHARED, DragonDevHandle, 0);
        ioctl(DragonDevHandle, DRAGON_QUEUE_BUFFER, i);
    }

    ioctl(DragonDevHandle, DRAGON_SET_DAC, PcieDacData);

    ioctl(DragonDevHandle, DRAGON_SET_REG1, CONFIG_REG_1);
    ioctl(DragonDevHandle, DRAGON_SET_REG2, 1);
    ioctl(DragonDevHandle, DRAGON_START, 1);


    while(true)
    {
        for(k=0; k<DRAGON_BUFFER_COUNT; k++)
            ioctl(DragonDevHandle, DRAGON_QUEUE_BUFFER, k);

        usleep(1000000);

        for(k=0; k<DRAGON_BUFFER_COUNT; k++)
        {
            printf("\n\nbuffer: %d", k);
            i=0;
            printf("\nfirst pckt: %d,\tsextets: ",
                   ((0x0F&PcieInputDataShadows[k][i])<<24) | (PcieInputDataShadows[k][i+1]<<16) |
                   (PcieInputDataShadows[k][i+2]<<8) | PcieInputDataShadows[k][i+3]);

            for(j=0; j<15; j++)
                printf("%d\t", ((PcieInputDataShadows[k][i+4+j*8]&31)<<8) | PcieInputDataShadows[k][i+5+j*8]);

            i=DRAGON_BUFFER_SIZE-DRAGON_PACKET_SIZE_BYTES;
            printf("\nlast  pckt: %d \t sextets: ",
                   ((0x0F&PcieInputDataShadows[k][i])<<24) | (PcieInputDataShadows[k][i+1]<<16) |
                   (PcieInputDataShadows[k][i+2]<<8) | PcieInputDataShadows[k][i+3]);

            for(j=0; j<15; j++)
                printf("%d\t", ((PcieInputDataShadows[k][i+4+j*8]&31)<<8) | PcieInputDataShadows[k][i+5+j*8]);
        }
    }

    return 0;
}

