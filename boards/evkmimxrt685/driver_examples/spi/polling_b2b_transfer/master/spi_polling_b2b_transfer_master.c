/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_spi.h"
#include "board.h"
#include "app.h"
#include "fsl_debug_console.h"
#include "microseconds.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
#define BUFFER_SIZE (4096)
static uint8_t srcBuff[BUFFER_SIZE];
static uint8_t destBuff[BUFFER_SIZE];

static uint8_t masterSync[] = {0x5a, 0xa6};
static uint8_t masterSyncResp[10];
/*
<5a>
<a7>
<00 03 01 50 00 00 fb 40>
*/

static uint8_t masterGetProperty1[] = {0x5a, 0xa4, 0x0c, 0x00, 0x4b, 0x33, 0x07, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t masterGetProperty1Resp[18];
/*
<5a>
<a4>
<0c 00>
<65 1c>
<a7 00 00 02 00 00 00 00 00 00 03 4b>
*/

static uint8_t masterGetProperty11[] = {0x5a, 0xa4, 0x0c, 0x00, 0x37, 0xa2, 0x07, 0x00, 0x00, 0x02, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t masterGetProperty11Resp[18];
/*
<5a>
<a4>
<0c 00>
<f9 de>
<a7 00 00 02 00 00 00 00 00 02 00 00>
*/

static uint8_t masterWriteMemory[] = {0x5a, 0xa4, 0x10, 0x00, 0xf2, 0x68, 0x04, 0x01, 0x00, 0x03, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t masterWriteMemoryResp[18];
/*
<5a>
<a4>
<0c 00>
<23 72>
<a0 00 00 02 00 00 00 00 04 00 00 00>
*/

static uint32_t masterWriteMemoryImgHDR[0x10] = { 0 };
static uint8_t masterWriteMemoryImgData[0x400] = { 0 };

static uint8_t masterSendAck[] = {0x5a, 0xa1};


/*******************************************************************************
 * Code
 ******************************************************************************/

void fill_image_header(void)
{
    masterWriteMemoryImgHDR[0]  = 0x20200000;
    masterWriteMemoryImgHDR[1]  = 0x00082d09;
    // image length
    masterWriteMemoryImgHDR[8]  = 0x00020000;
    // image type
    masterWriteMemoryImgHDR[9]  = 0x00000000;
    // Loader address
    masterWriteMemoryImgHDR[13] = 0x00080000;
}

uint8_t s_ackData[100] = {0};
uint32_t s_ackBytes = 0;
void get_ack(uint32_t delayUs, bool needToSave)
{
    microseconds_delay(delayUs);
    if (needToSave)
    {
        s_ackBytes = 0;
    }
    spi_transfer_t xfer            = {0};
    destBuff[0] = 0;
    while (destBuff[0] != 0x5A)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = destBuff;
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        if (needToSave)
        {
            s_ackData[s_ackBytes++] = destBuff[0];
        }
        microseconds_delay(100);
    }

    destBuff[1] = 0;
    while (destBuff[1] != 0xA1)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = &destBuff[1];
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        if (needToSave)
        {
            s_ackData[s_ackBytes++] = destBuff[1];
        }
        microseconds_delay(50);
    }
}

void send_ack(uint32_t delayUs)
{
    SDK_DelayAtLeastUs(delayUs, SystemCoreClock);
    spi_transfer_t xfer            = {0};
    xfer.txData      = masterSendAck;
    xfer.rxData      = destBuff;
    xfer.dataSize    = 2;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
}

void test_sync(void)
{
    spi_transfer_t xfer            = {0};

    xfer.txData      = masterSync;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterSync);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    masterSyncResp[0] = 0x0;
    while (masterSyncResp[0] != 0x5A)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = masterSyncResp;
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        SDK_DelayAtLeastUs(1000, SystemCoreClock);
    }
    masterSyncResp[1] = 0x0;
    while (masterSyncResp[1] != 0xA7)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = &masterSyncResp[1];
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        SDK_DelayAtLeastUs(1000, SystemCoreClock);
    }
    
    xfer.txData      = srcBuff;
    xfer.rxData      = &masterSyncResp[2];
    xfer.dataSize    = sizeof(masterSyncResp) - 2;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
}

static uint8_t masterWriteMemoryImgData_4096[6] = {0x5a, 0xa5, 0x00, 0x10, 0xbf, 0x75 };
static uint8_t masterWriteMemoryImgData_4000[6] = {0x5a, 0xa5, 0xa0, 0x0f, 0xa2, 0xb2 };
static uint8_t masterWriteMemoryImgData_2048[6] = {0x5a, 0xa5, 0x00, 0x08, 0x2d, 0x7b };
static uint8_t masterWriteMemoryImgData_1500[6] = {0x5a, 0xa5, 0xdc, 0x05, 0x65, 0xd1 };
static uint8_t masterWriteMemoryImgData_1024[6] = {0x5a, 0xa5, 0x00, 0x04, 0x6f, 0xc0 };

void test_one_packet_data(uint32_t packetSize, uint32_t delayUs)
{
    spi_transfer_t xfer            = {0};
    assert(packetSize >= 64+8);

    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    switch(packetSize)
    {
        case 4096:
            xfer.txData      = masterWriteMemoryImgData_4096;
            break;
        case 4000:
            xfer.txData      = masterWriteMemoryImgData_4000;
            break;
        case 2048:
            xfer.txData      = masterWriteMemoryImgData_2048;
            break;
        case 1500:
            xfer.txData      = masterWriteMemoryImgData_1500;
            break;
        case 1024:
            xfer.txData      = masterWriteMemoryImgData_1024;
            break;
        default:
            break;
    }
    xfer.rxData      = destBuff;
    xfer.dataSize    = 6;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    xfer.txData      = (uint8_t *)&masterWriteMemoryImgHDR[0];
    xfer.dataSize    = 64;
    packetSize      -= 64;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    memset(&masterWriteMemoryImgData[0], 0xF1, 1024);
    xfer.txData      = &masterWriteMemoryImgData[0];
    
    if (packetSize > 1024-64-8)
    {
        xfer.dataSize = 1024-64-8;
        packetSize -= 1024-64-8;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    }
    
    while(packetSize)
    {
        if (packetSize >= 1024)
        {
            xfer.dataSize = 1024;
            packetSize -= 1024;
        }
        else
        {
            xfer.dataSize = packetSize;
            packetSize = 0;
        }
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    }

    get_ack(delayUs, true);
}

void test_blhost(void)
{
    spi_transfer_t xfer            = {0};
    
    test_sync();

    xfer.txData      = masterGetProperty1;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterGetProperty1);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    get_ack(20000, false);
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = srcBuff;
    xfer.rxData      = masterGetProperty1Resp;
    xfer.dataSize    = sizeof(masterGetProperty1Resp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack(20000);
    
    /////////////////////////////////////////////////////////
    
    test_sync();

    xfer.txData      = masterGetProperty11;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterGetProperty11);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    get_ack(20000, false);
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = srcBuff;
    xfer.rxData      = masterGetProperty11Resp;
    xfer.dataSize    = sizeof(masterGetProperty11Resp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack(20000);

    //////////////////////////////////////////////////////////    
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = masterWriteMemory;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterWriteMemory);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    get_ack(20000, false);
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = srcBuff;
    xfer.rxData      = masterWriteMemoryResp;
    xfer.dataSize    = sizeof(masterWriteMemoryResp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack(20000);
    
    fill_image_header();
    uint32_t loop = 10;
    while (loop--)
    {
        test_one_packet_data(1024, 500);
    }

    while(1);
}

int main(void)
{
    spi_master_config_t userConfig = {0};
    uint32_t srcFreq               = 0;
    uint32_t i                     = 0;
    uint32_t err                   = 0;
    spi_transfer_t xfer            = {0};

    BOARD_InitHardware();
    microseconds_init();
    PRINTF("\n\rMaster Start...\n\r");
    /*
     * userConfig.enableLoopback = false;
     * userConfig.enableMaster = true;
     * userConfig.polarity = kSPI_ClockPolarityActiveHigh;
     * userConfig.phase = kSPI_ClockPhaseFirstEdge;
     * userConfig.direction = kSPI_MsbFirst;
     * userConfig.baudRate_Bps = 500000U;
     */
    SPI_MasterGetDefaultConfig(&userConfig);
    
    userConfig.polarity = kSPI_ClockPolarityActiveLow;
    userConfig.phase = kSPI_ClockPhaseSecondEdge;
    
    srcFreq            = EXAMPLE_SPI_MASTER_CLK_FREQ;
    userConfig.sselNum = (spi_ssel_t)EXAMPLE_SPI_SSEL;
    userConfig.sselPol = (spi_spol_t)EXAMPLE_SPI_SPOL;
    SPI_MasterInit(EXAMPLE_SPI_MASTER, &userConfig, srcFreq);
    
    
    test_blhost();

    /* Init Buffer*/
    for (i = 0; i < BUFFER_SIZE; i++)
    {
        srcBuff[i] = i;
    }

    /*Start Transfer*/
    xfer.txData      = srcBuff;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(destBuff);
    xfer.configFlags = kSPI_FrameAssert;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    /*Check if the data is right*/
    for (i = 0; i < BUFFER_SIZE; i++)
    {
        if (srcBuff[i] != destBuff[i])
        {
            err++;
            PRINTF("The %d is wrong! data is %d\n\r", i, destBuff[i]);
        }
    }
    if (err == 0)
    {
        PRINTF("Succeed!\n\r");
    }

    while (1)
    {
    }
}
