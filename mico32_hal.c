// hardware abstract layer
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"
#include "rom_map.h"

// freertos includes:
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "platform_common_config.h"
#include "cc32xx_platform.h"

#include "mico32_common.h"

#include "MICORTOS.h"

#include "MicoDrivers/MICODriverAdc.h"
#include "MicoDrivers/MICODriverFlash.h"
#include "MicoDrivers/MICODriverGpio.h"
#include "MicoDrivers/MICODriverI2c.h"
#include "MicoDrivers/MICODriverPwm.h"
#include "MicoDrivers/MICODriverRng.h"
#include "MicoDrivers/MICODriverRtc.h"
#include "MicoDrivers/MICODriverSpi.h"
#include "MicoDrivers/MICODriverUART.h"
#include "MicoDrivers/MICODriverWdg.h"

#include "simplelink.h"

#include "example/common/uart_if.h"

#include "adc.h"
#include "flash.h"
#include "gpio.h"
#include "i2c.h"
#include "systick.h"
#include "spi.h"
#include "timer.h"
#include "uart.h"
#include "wdt.h"

#if 1
#include "time.h"
#endif

#include "prcm.h"

#include "cc32xx_platform.h"

#include "mico32_common.h"

#define MASTER_LED_SCAN_TIMES   3
#define MAX_FLASH_FILE_LEN      16
#define MICO32_OTA_SIZE         240*1024

typedef struct
{
     uint32_t            rx_size;
} uart_interface_t;

extern mico_mutex_t msg_spawn;

uint8_t gpio_triggered = 0;
void (*gpio_int_handler)();

static uart_interface_t uart_interfaces[NUMBER_OF_UART_INTERFACES];

// adc:
// OSStatus MicoAdcInitialize(mico_adc_t adc, uint32_t sampling_cycle)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoAdcTakeSample(mico_adc_t adc, uint16_t *output)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoAdcTakeSampleStreram(mico_adc_t adc, void* buffer, uint16_t buffer_length)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoAdcFinalize(mico_adc_t adc)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }

// flash:

// this function is an ambigious one, which provides flash file info:
uint32_t mico32_flash_get_offset(mico_flash_t inFlash, uint32_t *flash_size, char *flash_file)
{
    uint32_t file_offset = 0, file_size = 0;
    char file_name[MAX_FLASH_FILE_LEN];
  
    switch (inFlash)
    {
    case MICO_FLASH_FOR_APPLICATION:    // not used
        file_offset = APPLICATION_START_ADDRESS;
        file_size = APPLICATION_FLASH_SIZE;
        strcpy(file_name, MICO32_FLASH_FILE_APPLICATION);
        break;
    case MICO_FLASH_FOR_UPDATE:         // ota
        file_offset = UPDATE_START_ADDRESS;
#if 1
        file_size = MICO32_OTA_SIZE;
#else
        file_size = UPDATE_FLASH_SIZE;
#endif
        strcpy(file_name, MICO32_FLASH_FILE_UPDATE);
        break;
    case MICO_FLASH_FOR_BOOT:           // not used
        file_offset = BOOT_START_ADDRESS;
        file_size = BOOT_FLASH_SIZE;
        strcpy(file_name, MICO32_FLASH_FILE_BOOT);
        break;
    case MICO_FLASH_FOR_PARA:           // parameter
        file_offset = PARA_START_ADDRESS;
        file_size = PARA_FLASH_SIZE;
        strcpy(file_name, MICO32_FLASH_FILE_PARA);
        break;
    case MICO_FLASH_FOR_EX_PARA:        // not used
        file_offset = EX_PARA_START_ADDRESS;
        file_size = EX_PARA_FLASH_SIZE;
        strcpy(file_name, MICO32_FLASH_FILE_EX_PARA);
        break;
    default:
        LOOP_FOREVER();
        break;
    }
    
    if (flash_size != NULL)
    {
        *flash_size = file_size;
    }
    
    if (flash_file != NULL)
    {
        memcpy(flash_file, file_name, MAX_FLASH_FILE_LEN);
    }
    
    return file_offset;
}

OSStatus MicoFlashInitialize(mico_flash_t inFlash)
{
    OSStatus os_result = kNoErr;
    _i32 fs_result;
    
    _i32 h_file;
    
    _u32 flash_file_size;
    
    char flash_file[MAX_FLASH_FILE_LEN];
    
    SlFsFileInfo_t file_info;
    
    // nothing to do
#if 0
    fs_result = sl_FsDel(MICO32_USER_FLASH_FILE_NAME, 0);
    
    LOOP_FOREVER();
#endif 
    
    mico32_flash_get_offset(inFlash, &flash_file_size, flash_file);
    
    // test if file exists:
    fs_result = sl_FsGetInfo(flash_file, NULL, &file_info);
    if(fs_result < 0 )
    {
        // file doesn't exist, create one
        fs_result = sl_FsOpen(flash_file,
            FS_MODE_OPEN_CREATE(flash_file_size, _FS_FILE_OPEN_FLAG_COMMIT | _FS_FILE_PUBLIC_READ | _FS_FILE_PUBLIC_WRITE),
            NULL, &h_file);
        // can we open flash for both read & write???
        // _FS_FILE_PUBLIC_WRITE is a secure file parameter

        fs_result = sl_FsClose(h_file, NULL, NULL, 0);
    }
    
done:
    return os_result;
}

OSStatus MicoFlashErase(mico_flash_t inFlash, uint32_t inStartAddress, uint32_t inEndAddress)
{
    OSStatus os_result = kNoErr;

    char *erase_buffer;
    uint16_t erase_buffer_len;
    
    erase_buffer_len = inEndAddress - inStartAddress;
    erase_buffer = (char *)malloc(sizeof(erase_buffer_len));
    
#if 1
    memset(erase_buffer, 0, sizeof(erase_buffer_len));
#else
    memset(erase_buffer, 0xFF, sizeof(erase_buffer_len));
#endif

    // we just call flash write to fill 0xff (to emulate flash erase)
    os_result = MicoFlashWrite(inFlash, &inStartAddress, erase_buffer, erase_buffer_len);
    
    free(erase_buffer);
    
    DBG_PRINT("erased\r\n");
    
    return os_result;
}

OSStatus MicoFlashWrite(mico_flash_t inFlash, volatile uint32_t* inFlashAddress, uint8_t* inBuffer ,uint32_t inBufferLength)
{
    OSStatus os_result = kNoErr;
    _i32 fs_result;

    _i32 h_file;
    
    _u32 file_offset;
    
    char flash_file[MAX_FLASH_FILE_LEN];
    
    file_offset = (*inFlashAddress) - mico32_flash_get_offset(inFlash, NULL, flash_file);
    
    fs_result = sl_FsOpen(flash_file,
        FS_MODE_OPEN_WRITE,
        NULL ,&h_file);
    
    fs_result = sl_FsWrite(h_file, file_offset, inBuffer, inBufferLength);
    
    if (fs_result > 0)
    { // success:
        
    }
    else
    {
        LOOP_FOREVER();
    }
    
    fs_result = sl_FsClose(h_file, NULL, NULL, 0);
    
done:
    return os_result;
}

OSStatus MicoFlashRead(mico_flash_t inFlash, volatile uint32_t* inFlashAddress, uint8_t* outBuffer ,uint32_t inBufferLength)
{
    OSStatus os_result = kNoErr;
    _i32 fs_result;

    _i32 h_file;
    
    _u32 file_offset;
    
    char flash_file[MAX_FLASH_FILE_LEN];
    
    file_offset = (*inFlashAddress) - mico32_flash_get_offset(inFlash, NULL, flash_file);
    
    fs_result = sl_FsOpen(flash_file,
        FS_MODE_OPEN_READ,
        NULL ,&h_file);
    
    fs_result = sl_FsRead(h_file, file_offset, outBuffer, inBufferLength);
    
    if (fs_result >= 0)
    { // success:
        
    }
    else
    {
        //LOOP_FOREVER();
    }
    
    fs_result = sl_FsClose(h_file, NULL, NULL, 0);

done:
    return os_result;
}


OSStatus MicoFlashFinalize(mico_flash_t inFlash)
{
    OSStatus os_result = kNoErr;
    _i32 fs_result;

    // nothing to do
    
    return os_result;
}

// gpio:

static void mico_master()
{
    uint8_t i_scan;
  
#if 0
    // disable all interrupts:
    IntMasterDisable(); // should we do this? the stupid simplelink will use spi
#endif
  
    // suspend all threads:
    
    // kill all user updated files:
    sl_FsDel(MICO32_FLASH_FILE_APPLICATION      , 0);
    sl_FsDel(MICO32_FLASH_FILE_UPDATE           , 0);
    sl_FsDel(MICO32_FLASH_FILE_BOOT             , 0);
    sl_FsDel(MICO32_FLASH_FILE_PARA             , 0);
    sl_FsDel(MICO32_FLASH_FILE_EX_PARA          , 0);
    
    // blink led:
    for (i_scan = 0; i_scan < MASTER_LED_SCAN_TIMES; i_scan ++)
    {
        mico_led_set(LED_100);
        mico_thread_msleep(100);
        mico_led_set(LED_010);
        mico_thread_msleep(100);
        mico_led_set(LED_001);
        mico_thread_msleep(100);
        mico_led_set(LED_010);
        mico_thread_msleep(100);
    }
    
    // do reset:
    MicoSystemReboot();
}

void mico_gpio_soft_handler()
{
    long gpio_pin = 0;
  
    // only 2 button down will trigger master:        
    gpio_pin = MAP_GPIOPinRead(gpio_mapping[WakeUp_BUTTON].gpio_base, gpio_mapping[WakeUp_BUTTON].gpio_pin);

    if (gpio_pin == gpio_mapping[WakeUp_BUTTON].gpio_pin)
    {
        mico_master();
    }
    
    // call real gpio handler here:
    if (gpio_int_handler != NULL)
    {
        (*gpio_int_handler)();
    }
}

void mico_gpio_int_handler()
{
    uint8_t msg;
    
    BaseType_t higher_priority_task_woken;
    
    MAP_GPIOIntClear(gpio_mapping[EasyLink_BUTTON].gpio_base, gpio_mapping[EasyLink_BUTTON].gpio_pin);
   
    mico_led_set(LM_KEY2_PRESSED);
        
#if 0
    DBG_PRINT("el button\r\n");
#endif

    
#if 1
    higher_priority_task_woken = pdTRUE;
    // we shouldn't call any simplelink api from isr, just trigger a semaphore:
    
    if (msg_spawn != NULL)
    {
        msg = SM_GPIO;
        
        if (xQueueSendFromISR(msg_spawn, &msg, &higher_priority_task_woken) != pdPASS)
        {
            LOOP_FOREVER();
        }
    }
#else
    xSemaphoreGiveFromISR(hal_spawn, &higher_priority_task_woken);
#endif
        
#if 0        
    // call cb function
    (*gpio_int_handler)();
#endif
  
    return ;
}

OSStatus MicoGpioInitialize(mico_gpio_t gpio, mico_gpio_config_t configuration)
{
    OSStatus os_result = kNoErr;

    if (gpio == EasyLink_BUTTON)
    {    
        //MAP_PRCMPeripheralReset(PRCM_GPIOA1);
        //MAP_PRCMPeripheralClkDisable(PRCM_GPIOA1, PRCM_RUN_MODE_CLK);
        //MAP_PRCMPeripheralClkEnable(PRCM_GPIOA1, PRCM_RUN_MODE_CLK);
          
        MAP_GPIOIntClear(gpio_mapping[EasyLink_BUTTON].gpio_base, 0xFF);
        
        MAP_IntPendClear(INT_GPIOA1);
        MAP_IntEnable(INT_GPIOA1);
    }
    
    return os_result;
}

OSStatus MicoGpioFinalize(mico_gpio_t gpio)
{
    OSStatus os_result = kNoErr;
    
    // nothiong to do
    
    return os_result;
}

OSStatus MicoGpioOutputHigh(mico_gpio_t gpio)
{
    OSStatus os_result = kNoErr;
    
    long gpio_val;
    
    gpio_val = MAP_GPIOPinRead(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
    gpio_val |= gpio_mapping[gpio].gpio_pin;
    
    MAP_GPIOPinWrite(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin, gpio_val);
    
    return os_result;
}

OSStatus MicoGpioOutputLow(mico_gpio_t gpio)
{
    OSStatus os_result = kNoErr;
    
    long gpio_val;
    
    gpio_val = MAP_GPIOPinRead(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
    gpio_val &= (~gpio_mapping[gpio].gpio_pin);
    
    MAP_GPIOPinWrite(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin, gpio_val);
    
    return os_result;
}

OSStatus MicoGpioOutputTrigger(mico_gpio_t gpio)
{
    OSStatus os_result = kNoErr;
    
    long gpio_val;
    
    gpio_val = MAP_GPIOPinRead(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
    MAP_GPIOPinWrite(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin, gpio_val);
    
    return os_result;
}

bool MicoGpioInputGet(mico_gpio_t gpio)
{
    bool hal_result = false;
  
    long gpio_val;
    
    mico_common_gpio_t common_gpio = gpio;
    
    if (common_gpio > MICO_GPIO_UNUSED)
    {
        gpio_val = MAP_GPIOPinRead(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
        
        if (gpio_val == gpio_mapping[gpio].gpio_pin)
        {
            hal_result = true;
        }
        else
        {
            hal_result = false;
        }
    }
    
    return hal_result;
}

OSStatus MicoGpioEnableIRQ(mico_gpio_t gpio, mico_gpio_irq_trigger_t trigger, mico_gpio_irq_handler_t handler, void *arg)
{
    OSStatus os_result = kNoErr;
    
    if (gpio == EasyLink_BUTTON)
    {
        // save callback handler:
        gpio_int_handler = handler;
        
        MAP_GPIOIntTypeSet(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin, GPIO_BOTH_EDGES);
        
        MAP_GPIOIntRegister(gpio_mapping[gpio].gpio_base, &mico_gpio_int_handler);
        
//        MAP_GPIOIntDisable(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
        MAP_GPIOIntClear(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
        MAP_GPIOIntEnable(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);   // should be refined
#if 0
        long gpio_val;
        
        while (1)
        {
            gpio_val = MAP_GPIOPinRead(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
            
            if (gpio_val == gpio_mapping[gpio].gpio_pin)
            {
                while (1)
                    ;
            }
        }
#endif
    }
    
    return os_result;
}

OSStatus MicoGpioDisableIRQ(mico_gpio_t gpio)
{
    OSStatus os_result = kNoErr;
    
    MAP_GPIOIntDisable(gpio_mapping[gpio].gpio_base, gpio_mapping[gpio].gpio_pin);
    
    return os_result;
}

// i2c
// OSStatus MicoI2cInitialize(mico_i2c_device_t* device)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }0r
// 
// bool MicoI2cProbeDevice(mico_i2c_device_t* device, int retries)
// {
//     // i2c bus scanner?
//     bool hal_result = false;
//     
//     return hal_result;
// }
// 
// OSStatus MicoI2cBuildTxMessage(mico_i2c_message_t* message, const void* tx_buffer, uint16_t  tx_buffer_length, uint16_t retries)
// {
//     OSStatus os_result = kNoErr;
//     
//     
//     return os_result;
// }
// 
// OSStatus MicoI2cBuildRxMessage(mico_i2c_message_t* message, void* rx_buffer, uint16_t rx_buffer_length, uint16_t retries)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoI2cBuildCombinedMessage(mico_i2c_message_t* message, const void* tx_buffer, void* rx_buffer, uint16_t tx_buffer_length, uint16_t rx_buffer_length, uint16_t retries)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoI2cTransfer(mico_i2c_device_t* device, mico_i2c_message_t* message, uint16_t number_of_messages)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoI2cFinalize(mico_i2c_device_t* device)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }

#define TIMER_TICK_FREQUENCY            80000000        // 80 MHz
#define TIMER_INTERVAL_RELOAD           // 400*(1~100)
#define DUTYCYCLE_GRANULARITY           400

// pwm:
OSStatus MicoPwmInitialize(mico_pwm_t pwm, uint32_t frequency, float duty_cycle)
{
    OSStatus os_result = kNoErr;
    
    unsigned long pin_mode;
    
    unsigned long timer_interval_reload, timer_duty_cycle;
    float timer_duty_cycle_100p;
    
    // reconfigure pin as pwm timer:
    pin_mode = PinModeGet(pwm_mapping[pwm].pwm_pin);
    if (pin_mode != PIN_MODE_3)
    {
        // why we want to skip the pwm initialize function here?
        // the init configuration could corrupt the neighbour pwm value
      
        MAP_PinTypeTimer(pwm_mapping[pwm].pwm_pin, PIN_MODE_3);

        MAP_TimerConfigure(pwm_mapping[pwm].pwm_base, TIMER_CFG_SPLIT_PAIR | pwm_mapping[pwm].pwm_config);
        MAP_TimerPrescaleSet(pwm_mapping[pwm].pwm_base, pwm_mapping[pwm].pwm_timer, 0);
        
        MAP_TimerControlLevel(pwm_mapping[pwm].pwm_base, pwm_mapping[pwm].pwm_timer, 1);
    }
    
    timer_interval_reload       = TIMER_TICK_FREQUENCY/frequency;
    timer_duty_cycle_100p       = timer_interval_reload*duty_cycle/100;
    timer_duty_cycle            = timer_duty_cycle_100p;
    
    DBG_PRINT("%d/%d\r\n", timer_duty_cycle, timer_interval_reload);
        
    MAP_TimerLoadSet(pwm_mapping[pwm].pwm_base, pwm_mapping[pwm].pwm_timer, timer_interval_reload);
    MAP_TimerMatchSet(pwm_mapping[pwm].pwm_base, pwm_mapping[pwm].pwm_timer, timer_duty_cycle);
    
    return os_result;
}

OSStatus MicoPwmStart(mico_pwm_t pwm)
{
    OSStatus os_result = kNoErr;
    
    MAP_TimerEnable(pwm_mapping[pwm].pwm_base, pwm_mapping[pwm].pwm_timer);
    
    return os_result;
}

OSStatus MicoPwmStop(mico_pwm_t pwm)
{
    OSStatus os_result = kNoErr;
    
    MAP_TimerDisable(pwm_mapping[pwm].pwm_base, pwm_mapping[pwm].pwm_timer);
    
    // should we shutdown timer peripheral clock?
    
    return os_result;
}

// rng:
// OSStatus MicoRandomNumberRead(void *inBuffer, int inByteCount)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }

// rtc:
void MicoRtcInitialize(void)
{
    MAP_PRCMRTCInUseSet();

    return ;
}

OSStatus MicoRtcGetTime(mico_rtc_time_t* time)
{
    OSStatus os_result = kNoErr;

    unsigned long   sec;
    unsigned short  msec;

    time_t raw_time;
    struct tm *time_info;

    MAP_PRCMRTCGet(&sec, &msec);

    raw_time = sec;

    time_info = localtime(&raw_time);

    time->sec       = time_info->tm_sec         ;
    time->min       = time_info->tm_min         ;
    time->hr        = time_info->tm_hour        ;
    time->weekday   = time_info->tm_wday        ;
    time->date      = time_info->tm_mday        ;
    time->month     = time_info->tm_mon + 1     ;
    time->year      = time_info->tm_year        ;

    return os_result;
}

OSStatus MicoRtcSetTime(mico_rtc_time_t* time)
{
    OSStatus os_result = kNoErr;

    unsigned long   sec;
    unsigned short  msec;

    time_t raw_time;
    struct tm time_info = { 0 };

    time_info.tm_sec    = time->sec             ;
    time_info.tm_min    = time->min             ;
    time_info.tm_hour   = time->hr              ;
    time_info.tm_wday   = time->weekday         ;
    time_info.tm_mday   = time->date            ;
    time_info.tm_mon    = time->month - 1       ;
    time_info.tm_year   = time->year            ;

    raw_time = mktime(&time_info);

    sec     = raw_time;
    msec    = 0;

    MAP_PRCMRTCSet(sec, msec);

    return os_result;
}

// spi:
// OSStatus MicoSpiInitialize(const mico_spi_device_t* spi)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoSpiTransfer(const mico_spi_device_t* spi, mico_spi_message_segment_t* segments, uint16_t number_of_segments)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }
// 
// OSStatus MicoSpiFinalize(const mico_spi_device_t* spi)
// {
//     OSStatus os_result = kNoErr;
//     
//     return os_result;
// }

// uart:
OSStatus MicoUartInitialize(mico_uart_t uart, const mico_uart_config_t *config, ring_buffer_t *optional_rx_buffer)
{
    OSStatus os_result = kNoErr;
    
    //uart_interfaces[uart]

    unsigned long uart_config = 0;

// include "uart_if.h" please
    // data length:
    uart_config |= UART_CONFIG_WLEN_8;      // always 8 (yr guys should add more scenario)
    // stop bit:
    uart_config |= UART_CONFIG_STOP_ONE;    // one stop bit
    // parity:
    uart_config |= UART_CONFIG_PAR_NONE;

    MAP_UARTConfigSetExpClk(CONSOLE, MAP_PRCMPeripheralClockGet(CONSOLE_PERIPH),
        UART_BAUD_RATE, uart_config);

    // enable fifo, so we can burst read?
    //MAP_UARTFIFOEnable(CONSOLE);

    MAP_UARTEnable(CONSOLE);
    
    return os_result;
}

OSStatus MicoStdioUartInitialize(const mico_uart_config_t *config, ring_buffer_t *optional_rx_buffer)
{
    OSStatus os_result = kNoErr;
    
    MicoUartInitialize(STDIO_UART, config, optional_rx_buffer);
    
    return os_result;
}

OSStatus MicoUartFinalize(mico_uart_t uart)
{
    OSStatus os_result = kNoErr;
    
    MAP_UARTDisable(CONSOLE);
    
    return os_result;
}

OSStatus MicoUartSend(mico_uart_t uart, const void *data, uint32_t size)
{
    OSStatus os_result = kNoErr;
    
    char *data_ptr = (char *)data;
    
    MAP_IntMasterDisable();
    
    while (size--)
    {
        MAP_UARTCharPut(CONSOLE, *data_ptr);
        data_ptr ++;
    }
    
    MAP_IntMasterEnable();
    
    return os_result;
}

OSStatus MicoUartRecv(mico_uart_t uart, void *data, uint32_t size, uint32_t timeout)
{
    OSStatus os_result = kNoErr;
    
    char *data_ptr = (char *)data;

    uint32_t expire_time;
    
    int32_t len = 0;

#if 1
    expire_time = mico_get_time() + timeout;
#else
    expire_time = mico_get_time_no_os() + timeout;
#endif

#if 1
    while ((mico_get_time() < expire_time) && (size > 0))
#else
    while ((mico_get_time_no_os() < expire_time) && (size > 0))
#endif
    {
        // try to get a char, until timeout
        // we use os tick to measure timeout
        mico_thread_msleep(100);
        
        if (MAP_UARTCharsAvail(CONSOLE))
        {
            *data_ptr = MAP_UARTCharGetNonBlocking(CONSOLE);
            MAP_UARTCharPutNonBlocking(CONSOLE, *data_ptr);
            data_ptr ++;
            size --;
            len ++;
        }
    }
#if 1
    return len;
#else
    return os_result;
#endif
}

uint32_t MicoUartGetLengthInBuffer(mico_uart_t uart)
{
    uint32_t hal_result;
    
    if (UARTCharsAvail(CONSOLE) == true)
    {
        hal_result = 1;
    }
    else
    {
        hal_result = 0;
    }

    return hal_result;
}

// wdg:
OSStatus MicoWdgInitialize(uint32_t timeout)
{
    OSStatus os_result = kNoErr;
    
    return os_result;
}

void MicoWdgReload(void)
{
    return ;
}

OSStatus MicoWdgFinalize(void)
{
    OSStatus os_result = kNoErr;
    
    return os_result;
}
