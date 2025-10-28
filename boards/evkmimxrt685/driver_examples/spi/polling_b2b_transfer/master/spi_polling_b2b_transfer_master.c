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

static uint8_t masterWriteMemory[] = {0x5a, 0xa4, 0x10, 0x00, 0x5e, 0x3e, 0x04, 0x01, 0x00, 0x03, 0x00, 0x00, 0x08, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t masterWriteMemoryResp[18];
/*
<5a>
<a4>
<0c 00>
<23 72>
<a0 00 00 02 00 00 00 00 04 00 00 00>
*/

static uint8_t masterWriteMemoryData[0x1000+6] = {0x5a, 0xa5, 0x00, 0x10, 0xd6, 0x4b };


static uint8_t masterSendAck[] = {0x5a, 0xa1};


/*******************************************************************************
 * Code
 ******************************************************************************/

void get_ack(void)
{
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    spi_transfer_t xfer            = {0};
    destBuff[0] = 0;
    while (destBuff[0] != 0x5A)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = destBuff;
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        SDK_DelayAtLeastUs(300, SystemCoreClock);
    }

    destBuff[1] = 0;
    while (destBuff[1] != 0xA1)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = &destBuff[1];
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        SDK_DelayAtLeastUs(300, SystemCoreClock);
    }
}

void send_ack(void)
{
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
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

void test_one_packet_data(void)
{
    spi_transfer_t xfer            = {0};

    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = masterWriteMemoryData;
    xfer.rxData      = destBuff;
    xfer.dataSize    = 6;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    xfer.txData      = &masterWriteMemoryData[6];
    xfer.rxData      = destBuff;
    memset(&masterWriteMemoryData[6], 0xF1, 1024);
    xfer.dataSize    = 1024-9;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    xfer.dataSize    = 1;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    memset(&masterWriteMemoryData[6], 0xF2, 1024);
    xfer.dataSize    = 1024;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    memset(&masterWriteMemoryData[6], 0xF3, 1024);
    xfer.dataSize    = 1024;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    memset(&masterWriteMemoryData[6], 0xF4, 1024);
    xfer.dataSize    = 1024;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    xfer.dataSize    = 8;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    get_ack();
}

void test_blhost(void)
{
    spi_transfer_t xfer            = {0};
    
    test_sync();

    xfer.txData      = masterGetProperty1;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterGetProperty1);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    get_ack();
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = srcBuff;
    xfer.rxData      = masterGetProperty1Resp;
    xfer.dataSize    = sizeof(masterGetProperty1Resp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack();
    
    /////////////////////////////////////////////////////////
    
    test_sync();

    xfer.txData      = masterGetProperty11;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterGetProperty11);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    get_ack();
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = srcBuff;
    xfer.rxData      = masterGetProperty11Resp;
    xfer.dataSize    = sizeof(masterGetProperty11Resp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack();

    //////////////////////////////////////////////////////////    
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = masterWriteMemory;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterWriteMemory);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    get_ack();
    
    SDK_DelayAtLeastUs(20000, SystemCoreClock);
    xfer.txData      = srcBuff;
    xfer.rxData      = masterWriteMemoryResp;
    xfer.dataSize    = sizeof(masterWriteMemoryResp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack();
    
    test_one_packet_data();
    test_one_packet_data();
    test_one_packet_data();

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
