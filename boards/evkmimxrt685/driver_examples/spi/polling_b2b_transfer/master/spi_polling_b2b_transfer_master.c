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
#define BUFFER_SIZE (65536)
#define MASTER_TX_FREQ (43000000)
#define MASTER_RX_FREQ (12000000)

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

static uint8_t masterWriteMemory[] = {0x5a, 0xa4, 0x10, 0x00, 0xf0, 0xee, 0x04, 0x01, 0x00, 0x03, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t masterWriteMemoryResp[18];
/*
<5a>
<a4>
<0c 00>
<23 72>
<a0 00 00 02 00 00 00 00 04 00 00 00>
*/

static uint32_t masterWriteMemoryImgHDR[0x10] = { 0 };
static uint8_t masterWriteMemoryImgData[BUFFER_SIZE] = { 0 };

static uint8_t masterSendAck[] = {0x5a, 0xa1};


/*******************************************************************************
 * Code
 ******************************************************************************/

status_t config_fcspi_speed(uint32_t baud)
{
    status_t result = kStatus_Success;
    result = SPI_MasterSetBaud(EXAMPLE_SPI_MASTER, baud, EXAMPLE_SPI_MASTER_CLK_FREQ);
    if (kStatus_Success != result)
    {
        PRINTF("\n\rCannot set SPI baudrate %d\n\r", baud);
    }
    return result;
}

void fill_image_header(void)
{
    masterWriteMemoryImgHDR[0]  = 0x20200000;
    masterWriteMemoryImgHDR[1]  = 0x00082d09;
    // image length - 64KB * 8
    masterWriteMemoryImgHDR[8]  = 0x00080000;
    // image type
    masterWriteMemoryImgHDR[9]  = 0x00000000;
    // Loader address
    masterWriteMemoryImgHDR[13] = 0x00080000;
    
    memset(&masterWriteMemoryImgData[0], 0xF1, sizeof(masterWriteMemoryImgData));
}

void get_ack(uint32_t delayUs, bool isFuncTest, uint32_t pktSize)
{
    config_fcspi_speed(MASTER_RX_FREQ);
    uint32_t retryCnt = 200;

    uint32_t totalDelayCnt0 = 0;
    uint32_t totalDelayCnt1 = 0;
    microseconds_delay(delayUs);
    destBuff[0] = 0;
    spi_transfer_t xfer            = {0};
    xfer.txData      = srcBuff;
    xfer.rxData      = destBuff;
    xfer.dataSize    = 1;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    while (destBuff[0] != 0x5A)
    {
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        microseconds_delay(100);
        totalDelayCnt0++;
        if (totalDelayCnt0 == retryCnt)
        {
            break;
        }
    }

    microseconds_delay(150);

    destBuff[1] = 0;
    xfer.rxData      = &destBuff[1];
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    while (destBuff[1] != 0xA1)
    {
        switch (destBuff[1])
        {
            case 0xA2:
                PRINTF("\n\rGot Nak after delay %dus+%d*100us+150us+%d*50us for packet %d\n\r", delayUs, totalDelayCnt0, totalDelayCnt1, pktSize);
                return;
            case 0xA3:
                PRINTF("\n\rGot AckAbort after delay %dus+%d*100us+150us+%d*50us for packet %d\n\r", delayUs, totalDelayCnt0, totalDelayCnt1, pktSize);
                return;
            default:
                break;
        }
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        microseconds_delay(50);
        totalDelayCnt1++;
        if (totalDelayCnt1 == retryCnt)
        {
            break;
        }
    }
    if (totalDelayCnt0 + totalDelayCnt1 == retryCnt * 2)
    {
        PRINTF("\n\rGot Nothing after delay %dus+%d*100us+150us+%d*50us for packet %d\n\r", delayUs, totalDelayCnt0, totalDelayCnt1, pktSize);
    }
    else if (totalDelayCnt0 || totalDelayCnt1 || isFuncTest)
    {
        // It costs about 3.5ms
        PRINTF("\n\rGot Ack after delay %dus+%d*100us+150us+%d*50us for packet %d\n\r", delayUs, totalDelayCnt0, totalDelayCnt1, pktSize);
    }
}

void send_ack(uint32_t delayUs, bool isFuncTest)
{
    microseconds_delay(delayUs);
    spi_transfer_t xfer            = {0};
    xfer.txData      = masterSendAck;
    xfer.rxData      = destBuff;
    xfer.dataSize    = 2;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    if (isFuncTest)
    {
        // It costs about 8ms
        PRINTF("\n\rSent Ack\n\r");
    }
}

void test_sync(uint32_t delayUs)
{
    spi_transfer_t xfer            = {0};

    xfer.txData      = masterSync;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterSync);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    microseconds_delay(delayUs);

    masterSyncResp[0] = 0x0;
    while (masterSyncResp[0] != 0x5A)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = masterSyncResp;
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        microseconds_delay(delayUs);
    }
    microseconds_delay(delayUs);
    masterSyncResp[1] = 0x0;
    while (masterSyncResp[1] != 0xA7)
    {
        xfer.txData      = srcBuff;
        xfer.rxData      = &masterSyncResp[1];
        xfer.dataSize    = 1;
        SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
        microseconds_delay(delayUs);
    }
    
    xfer.txData      = srcBuff;
    xfer.rxData      = &masterSyncResp[2];
    xfer.dataSize    = sizeof(masterSyncResp) - 2;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    PRINTF("\n\rSync packet done\n\r");
}

static uint8_t masterWriteMemoryImgData_65535[6] = {0x5a, 0xa5, 0xff, 0xff, 0xa3, 0x09 };
static uint8_t masterWriteMemoryImgData_64512[6] = {0x5a, 0xa5, 0x00, 0xfc, 0xc4, 0x27 };
static uint8_t masterWriteMemoryImgData_32768[6] = {0x5a, 0xa5, 0x00, 0x80, 0xb8, 0x2e };
static uint8_t masterWriteMemoryImgData_16384[6] = {0x5a, 0xa5, 0x00, 0x40, 0x8c, 0x6e };
static uint8_t masterWriteMemoryImgData_8192[6]  = {0x5a, 0xa5, 0x00, 0x20, 0x52, 0x38 };
static uint8_t masterWriteMemoryImgData_4096[6]  = {0x5a, 0xa5, 0x00, 0x10, 0xff, 0x8e };
static uint8_t masterWriteMemoryImgData_4000[6]  = {0x5a, 0xa5, 0xa0, 0x0f, 0xcb, 0x44 };
static uint8_t masterWriteMemoryImgData_2048[6]  = {0x5a, 0xa5, 0x00, 0x08, 0x73, 0xf8 };
static uint8_t masterWriteMemoryImgData_1500[6]  = {0x5a, 0xa5, 0xdc, 0x05, 0xbd, 0xee };
static uint8_t masterWriteMemoryImgData_1024[6]  = {0x5a, 0xa5, 0x00, 0x04, 0x78, 0xff };
static uint8_t masterWriteMemoryImgData_1016[6]  = {0x5a, 0xa5, 0xf8, 0x03, 0x88, 0x81 };

void test_one_packet_data(uint32_t packetSize, uint32_t delayUs, bool isFuncTest)
{
    spi_transfer_t xfer            = {0};
    assert(packetSize >= 64+8);
    uint32_t oriPktSize = packetSize;
    config_fcspi_speed(MASTER_TX_FREQ);

    microseconds_delay(delayUs);
    if (isFuncTest)
    {
        // It costs about 3.5ms
        PRINTF("\n\rSending one packet data (%d)\n\r", packetSize);
    }
    switch(packetSize)
    {
        case 65535:
            xfer.txData      = masterWriteMemoryImgData_65535;
            break;
        case 64512:
            xfer.txData      = masterWriteMemoryImgData_64512;
            break;
        case 32768:
            xfer.txData      = masterWriteMemoryImgData_32768;
            break;
        case 16384:
            xfer.txData      = masterWriteMemoryImgData_16384;
            break;
        case 8192:
            xfer.txData      = masterWriteMemoryImgData_8192;
            break;
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
        case 1016:
            xfer.txData      = masterWriteMemoryImgData_1016;
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

    xfer.txData      = &masterWriteMemoryImgData[0];
#if 1
    xfer.dataSize = packetSize;
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
#else
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
#endif

    get_ack(delayUs, isFuncTest, oriPktSize);
}

void test_blhost(bool isFuncTest)
{
    uint32_t cmdDelay = 400;

    config_fcspi_speed(MASTER_RX_FREQ);

    fill_image_header();

    test_sync(cmdDelay);

    spi_transfer_t xfer            = {0};

    if (isFuncTest)
    {
        PRINTF("\n\rSending get-property 1\n\r");
    }
    else
    {
        microseconds_delay(cmdDelay);
    }
    xfer.txData      = masterGetProperty1;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterGetProperty1);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    get_ack(cmdDelay, isFuncTest, 0);
    
    if (isFuncTest)
    {
        PRINTF("\n\rReceiving get-property 1 resp\n\r");
    }
    else
    {
        microseconds_delay(cmdDelay);
    }
    xfer.txData      = srcBuff;
    xfer.rxData      = masterGetProperty1Resp;
    xfer.dataSize    = sizeof(masterGetProperty1Resp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack(cmdDelay, isFuncTest);
    
    /////////////////////////////////////////////////////////
    
    test_sync(cmdDelay);

    if (isFuncTest)
    {
        PRINTF("\n\rSending get-property 11\n\r");
    }
    else
    {
        microseconds_delay(cmdDelay);
    }
    xfer.txData      = masterGetProperty11;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterGetProperty11);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);

    get_ack(cmdDelay, isFuncTest, 0);
    
    if (isFuncTest)
    {
        PRINTF("\n\rReceiving get-property 11 resp\n\r");
    }
    else
    {
        microseconds_delay(cmdDelay);
    }
    xfer.txData      = srcBuff;
    xfer.rxData      = masterGetProperty11Resp;
    xfer.dataSize    = sizeof(masterGetProperty11Resp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack(cmdDelay, isFuncTest);

    //////////////////////////////////////////////////////////    

    if (isFuncTest)
    {
        PRINTF("\n\rSending write memory\n\r");
    }
    else
    {
        microseconds_delay(cmdDelay);
    }
    xfer.txData      = masterWriteMemory;
    xfer.rxData      = destBuff;
    xfer.dataSize    = sizeof(masterWriteMemory);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    get_ack(cmdDelay, isFuncTest, 0);
    
    if (isFuncTest)
    {
        PRINTF("\n\rReceiving write memory resp\n\r");
    }
    else
    {
        microseconds_delay(cmdDelay);
    }
    xfer.txData      = srcBuff;
    xfer.rxData      = masterWriteMemoryResp;
    xfer.dataSize    = sizeof(masterWriteMemoryResp);
    SPI_MasterTransferBlocking(EXAMPLE_SPI_MASTER, &xfer);
    
    send_ack(cmdDelay, isFuncTest);

    uint32_t loop = 8;
    // >=250us for 500KHz (240us failed)
    // >=350us for 4-16MHz (340us failed)
    // >=350us for 4-16MHz (340us failed)
    uint32_t pktDelay = 400;
    while (loop--)
    {
        // First packet is for image header handling
        test_one_packet_data(1016, pktDelay, isFuncTest);

        ////////////////////////////////////////////////////
        //test_one_packet_data(1024, pktDelay, isFuncTest);
        //test_one_packet_data(1500, pktDelay, isFuncTest);
        //test_one_packet_data(2048, pktDelay, isFuncTest);
        //test_one_packet_data(4000, pktDelay, isFuncTest);
        //test_one_packet_data(4096, pktDelay, isFuncTest);
        //test_one_packet_data(8192, pktDelay, isFuncTest);
        //test_one_packet_data(16384, pktDelay, isFuncTest);
        //test_one_packet_data(32768, pktDelay, isFuncTest);
        //test_one_packet_data(64512, pktDelay, isFuncTest);
        test_one_packet_data(65535, pktDelay, isFuncTest);
        __NOP();
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
    userConfig.baudRate_Bps = MASTER_RX_FREQ;
    
    userConfig.polarity = kSPI_ClockPolarityActiveLow;
    userConfig.phase = kSPI_ClockPhaseSecondEdge;
    
    srcFreq            = EXAMPLE_SPI_MASTER_CLK_FREQ;
    userConfig.sselNum = (spi_ssel_t)EXAMPLE_SPI_SSEL;
    userConfig.sselPol = (spi_spol_t)EXAMPLE_SPI_SPOL;
    status_t result = SPI_MasterInit(EXAMPLE_SPI_MASTER, &userConfig, srcFreq);
    if (kStatus_Success != result)
    {
        PRINTF("\n\rCannot init SPI module %d\n\r");
        while (1);
    }
    PRINTF("\n\rInitial SPI Clock freq = %dHz...\n\r", userConfig.baudRate_Bps);

    test_blhost(false);

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
