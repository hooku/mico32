#pragma once

#include "pinmux.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_gpio.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "gpio.h"
#include "timer.h"
#include "prcm.h"

#include "MicoRtos.h"

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

typedef struct
{
    unsigned long gpio_base;
    unsigned long gpio_pin;
    unsigned long gpio_pin_mode;
} platform_pin_mapping_t;

typedef struct
{
    unsigned long pwm_base;
    unsigned long pwm_pin;
    unsigned long pwm_timer;
    unsigned long pwm_config;
} platform_pwm_mapping_t;


/* DMA can be enabled by setting SPI_USE_DMA */
// typedef struct
// {
//     SPI_TypeDef*                  spi_regs;
//     uint8_t                       gpio_af;
//     uint32_t                      peripheral_clock_reg;
//     peripheral_clock_func_t       peripheral_clock_func;
//     const platform_pin_mapping_t* pin_mosi;
//     const platform_pin_mapping_t* pin_miso;
//     const platform_pin_mapping_t* pin_clock;
//     DMA_Stream_TypeDef*           tx_dma_stream;
//     DMA_Stream_TypeDef*           rx_dma_stream;
//     uint32_t                      tx_dma_channel;
//     uint32_t                      rx_dma_channel;
//     uint8_t                       tx_dma_stream_number;
//     uint8_t                       rx_dma_stream_number;
// } platform_spi_mapping_t;

typedef struct
{
    unsigned long uart_base;
    unsigned long uart_tx_pin;
    unsigned long uart_tx_pin_mode;
    unsigned long uart_rx_pin;
    unsigned long uart_rx_pin_mode;
} platform_uart_mapping_t;


// typedef struct
// {
//     I2C_TypeDef*            i2c;
//     const platform_pin_mapping_t* pin_scl;
//     const platform_pin_mapping_t* pin_sda;
//     uint32_t                peripheral_clock_reg;
//     dma_registers_t*              tx_dma;
//     peripheral_clock_t            tx_dma_peripheral_clock;
//     DMA_Stream_TypeDef* tx_dma_stream;
//     DMA_Stream_TypeDef* rx_dma_stream;
//     int                 tx_dma_stream_id;
//     int                 rx_dma_stream_id;
//     uint32_t            tx_dma_channel;
//     uint32_t            rx_dma_channel;
//     uint32_t                      gpio_af;
// } platform_i2c_mapping_t;

/******************************************************
 *                 Global Variables
 ******************************************************/

extern const platform_pin_mapping_t  gpio_mapping[];
// extern const platform_adc_mapping_t  adc_mapping[];
extern const platform_pwm_mapping_t  pwm_mapping[];
// extern const platform_spi_mapping_t  spi_mapping[];
extern const platform_uart_mapping_t uart_mapping[];
// extern const platform_i2c_mapping_t  i2c_mapping[];

/******************************************************
 *               Function Declarations
 ******************************************************/
