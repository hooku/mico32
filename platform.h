#include "platform_common_config.h"

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
  
#define HARDWARE_REVISION   "CC3200-LAUNCHXL_1"
#define DEFAULT_NAME        "CC3200-LAUNCHXL"
#define MODEL               "CC3200-LAUNCHXL"

   
/******************************************************
 *                   Enumerations
 ******************************************************/
typedef enum
{
    MICO_GPIO_0 = MICO_COMMON_GPIO_MAX,
    MICO_GPIO_1,
    MICO_GPIO_2,
    //MICO_GPIO_3,
    MICO_GPIO_MAX, /* Denotes the total number of GPIO port aliases. Not a valid GPIO alias */
} mico_gpio_t;

typedef enum
{
    MICO_SPI_1,
    MICO_SPI_MAX, /* Denotes the total number of SPI port aliases. Not a valid SPI alias */
} mico_spi_t;

typedef enum
{
    MICO_I2C_1,
    MICO_I2C_MAX, /* Denotes the total number of I2C port aliases. Not a valid I2C alias */
} mico_i2c_t;

typedef enum
{
    MICO_PWM_1 = MICO_COMMON_PWM_MAX,
    MICO_PWM_2,
    MICO_PWM_3,
    MICO_PWM_MAX, /* Denotes the total number of PWM port aliases. Not a valid PWM alias */
} mico_pwm_t;

typedef enum
{
    MICO_ADC_1,
    MICO_ADC_2,
    MICO_ADC_3,
    MICO_ADC_MAX, /* Denotes the total number of ADC port aliases. Not a valid ADC alias */
} mico_adc_t;

typedef enum
{
    MICO_UART_UNUSED = -1,
    MICO_UART_1,
    MICO_UART_2,
    MICO_UART_MAX, /* Denotes the total number of UART port aliases. Not a valid UART alias */
} mico_uart_t;

typedef enum
{
  MICO_INTERNAL_FLASH,
  MICO_FILE_FLASH_0,
  MICO_FILE_FLASH_1,
  MICO_FILE_FLASH_2,
  MICO_FILE_FLASH_3,
  MICO_FILE_FLASH_4,
  MICO_FLASH_MAX,
} mico_flash_t;

#define STM32_UART_1 MICO_UART_1  /*Not used here, define to avoid warning*/
#define STM32_UART_2 MICO_UART_1
#define STM32_UART_6 MICO_UART_2

/* Components connected to external I/Os*/


/* I/O connection <-> Peripheral Connections */
//#define MICO_I2C_CP         (MICO_I2C_1)



#define RestoreDefault_TimeOut          3000  /**< Restore default and start easylink after 
                                                   press down EasyLink button for 3 seconds. */

#ifdef __cplusplus
} /*extern "C" */
#endif

