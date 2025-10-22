/*
 * Copyright  2017 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "app.h"
#include "fsl_debug_console.h"
#include "fsl_device_registers.h"
#include "fsl_spi.h"
#include "fsl_spi_dma.h"
#include "fsl_dma.h"
#include "board.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TRANSFER_SIZE 64U /*! Transfer dataSize */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void SPI_SlaveUserCallback(SPI_Type *base, spi_dma_handle_t *handle, status_t status, void *userData);
static void EXAMPLE_SlaveInit(void);
static void EXAMPLE_SlaveDMASetup(void);
static void EXAMPLE_SlaveStartDMATransfer(void);
static void EXAMPLE_TransferDataCheck(void);

static void EXAMPLE_InitBuffers(void);
static void EXAMPLE_SlaveDMASetupAndStartTransfer(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/
uint8_t slaveRxData[TRANSFER_SIZE] = {0};
uint8_t slaveTxData[TRANSFER_SIZE] = {0};

dma_handle_t slaveTxHandle;
dma_handle_t slaveRxHandle;

spi_dma_handle_t slaveHandle;

volatile bool isTransferCompleted = false;
static volatile uint32_t s_transferCount = 0;
#define DMA_LINK_TRANSFER_COUNT 2

static dma_handle_t s_DMA_Handle;
DMA_ALLOCATE_LINK_DESCRIPTORS(s_dma_table, DMA_LINK_TRANSFER_COUNT);

/*******************************************************************************
 * Code
 ******************************************************************************/
static void SPI_SlaveUserCallback(SPI_Type *base, spi_dma_handle_t *handle, status_t status, void *userData)
{
    if (status == kStatus_Success)
    {
        if (++s_transferCount >= DMA_LINK_TRANSFER_COUNT)
        {
            isTransferCompleted = true;
        }
    }
}

void DMA_Callback(dma_handle_t *handle, void *param, bool transferDone, uint32_t tcds)
{
    if (transferDone)
    {
        if (++s_transferCount >= DMA_LINK_TRANSFER_COUNT)
        {
            isTransferCompleted = true;
            DMA_DisableChannel(DMA0, EXAMPLE_SPI_SLAVE_RX_CHANNEL);
        }
    }
}

/*!
 * @brief Main function
 */
int main(void)
{
    /* Initialzie board setting. */
    BOARD_InitHardware();

    /* Print project information. */
    PRINTF("This is SPI DMA transfer slave example(SPI14, PIO1-11/12/13/14).\r\n");
    PRINTF("This example will communicate with another master SPI on the other board.\r\n");
    PRINTF("Slave board is working...!\r\n");

    /* Initialize the SPI slave instance. */
    EXAMPLE_SlaveInit();
    
    EXAMPLE_InitBuffers();

    /* Configure DMA for slave SPI. */
    EXAMPLE_SlaveDMASetup();

    /* Start SPI DMA transfer. */
    EXAMPLE_SlaveStartDMATransfer();
    
    //EXAMPLE_SlaveDMASetupAndStartTransfer();

    /* Waiting for transmission complete and check if all data matched. */
    EXAMPLE_TransferDataCheck();

    /* De-intialzie the DMA instance. */
    DMA_Deinit(EXAMPLE_DMA);

    /* De-intialize the SPI instance. */
    SPI_Deinit(EXAMPLE_SPI_SLAVE);

    while (1)
    {
    }
}

static void EXAMPLE_SlaveInit(void)
{
    spi_slave_config_t slaveConfig;

    /* Get default Slave configuration. */
    SPI_SlaveGetDefaultConfig(&slaveConfig);

    /* Initialize the SPI slave. */
    slaveConfig.sselPol = (spi_spol_t)EXAMPLE_SLAVE_SPI_SPOL;
    SPI_SlaveInit(EXAMPLE_SPI_SLAVE, &slaveConfig);
}

static void EXAMPLE_InitBuffers(void)
{
    uint32_t i = 0U;
    /* Initialzie the transfer data */
    for (i = 0U; i < TRANSFER_SIZE; i++)
    {
        slaveTxData[i] = i % 256U;
        slaveRxData[i] = 0U;
    }
}

static void EXAMPLE_SlaveDMASetup(void)
{
    /* DMA init */
    DMA_Init(EXAMPLE_DMA);

    /* configure channel/priority and create handle for TX and RX. */
    DMA_EnableChannel(EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_TX_CHANNEL);
    DMA_EnableChannel(EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_RX_CHANNEL);
    DMA_SetChannelPriority(EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_TX_CHANNEL, kDMA_ChannelPriority0);
    DMA_SetChannelPriority(EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_RX_CHANNEL, kDMA_ChannelPriority1);
    DMA_CreateHandle(&slaveTxHandle, EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_TX_CHANNEL);
    DMA_CreateHandle(&slaveRxHandle, EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_RX_CHANNEL);
}

static void EXAMPLE_SlaveStartDMATransfer(void)
{
    spi_transfer_t slaveXferPing, slaveXferPong;

    /* Create handle for slave instance. */
    SPI_SlaveTransferCreateHandleDMA(EXAMPLE_SPI_SLAVE, &slaveHandle, SPI_SlaveUserCallback, NULL, &slaveTxHandle,
                                     &slaveRxHandle);

    slaveXferPing.txData   = (uint8_t *)&slaveTxData;
    slaveXferPing.rxData   = (uint8_t *)&slaveRxData;
    slaveXferPing.dataSize = (TRANSFER_SIZE / 2) * sizeof(slaveTxData[0]);
    slaveXferPing.configFlags = kSPI_FrameAssert;

    slaveXferPong.txData   = (uint8_t *)&slaveTxData[TRANSFER_SIZE/2];
    slaveXferPong.rxData   = (uint8_t *)&slaveRxData[TRANSFER_SIZE/2];
    slaveXferPong.dataSize = (TRANSFER_SIZE / 2) * sizeof(slaveTxData[0]);
    slaveXferPong.configFlags = kSPI_FrameAssert;

    /* Start transfer, when transmission complete, the SPI_SlaveUserCallback will be called. */
    if (kStatus_Success != SPI_SlavePingPongTransferDMA(EXAMPLE_SPI_SLAVE, &slaveHandle, &slaveXferPing, &slaveXferPong))
    {
        PRINTF("There is an error when start SPI_SlaveTransferDMA \r\n");
    }
}

static void EXAMPLE_SlaveDMASetupAndStartTransfer(void)
{
    DMA_Init(EXAMPLE_DMA);
    DMA_CreateHandle(&s_DMA_Handle, EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_RX_CHANNEL);
    DMA_EnableChannel(EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_RX_CHANNEL);
    DMA_SetCallback(&s_DMA_Handle, DMA_Callback, NULL);

    DMA_SetupDescriptor(&(s_dma_table[0]),
                        DMA_CHANNEL_XFER(true, false, true, false, 1U, 0,
                                         1, TRANSFER_SIZE/2),
                        (void *)&EXAMPLE_SPI_SLAVE->FIFORD, &slaveRxData[0], &(s_dma_table[1]));

    DMA_SetupDescriptor(&(s_dma_table[1]),
                        DMA_CHANNEL_XFER(true, false, true, false, 1U, 0,
                                         1, TRANSFER_SIZE/2),
                        (void *)&EXAMPLE_SPI_SLAVE->FIFORD, &slaveRxData[TRANSFER_SIZE/2], &(s_dma_table[0]));

    DMA_SubmitChannelDescriptor(&s_DMA_Handle, &(s_dma_table[0]));

    DMA_EnableChannelPeriphRq(EXAMPLE_DMA, EXAMPLE_SPI_SLAVE_RX_CHANNEL);
    SPI_EnableRxDMA(EXAMPLE_SPI_SLAVE, true);
    DMA_StartTransfer(&s_DMA_Handle);
}

static void EXAMPLE_TransferDataCheck(void)
{
    uint32_t i = 0U, errorCount = 0U;

    /* Wait until transfer completed */
    while (!isTransferCompleted)
    {
    }

    PRINTF("\r\nThe received data are:");
    /*Check if the data is right*/
    for (i = 0; i < TRANSFER_SIZE; i++)
    {
        /* Print 16 numbers in a line */
        if ((i & 0x0FU) == 0U)
        {
            PRINTF("\r\n  ");
        }
        PRINTF("  0x%02X", slaveRxData[i]);
        /* Check if data matched. */
        if (slaveTxData[i] != slaveRxData[i])
        {
            errorCount++;
        }
    }
    if (errorCount == 0)
    {
        PRINTF("\r\nSPI transfer all data matched! \r\n");
    }
    else
    {
        PRINTF("\r\nError occurred in SPI transfer ! \r\n");
    }
}
