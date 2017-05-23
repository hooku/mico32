#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"
#include "pin.h"
#include "rom_map.h"

#include "gpio.h"
#include "timer.h"
#include "prcm.h"

#include "example/common/timer_if.h"

#include "mico32_common.h"

#include "MICO.h"
#include "MicoPlatform.h"
#include "MICODefine.h"

// mem info:
#include "FreeRTOS.h"

#include "simplelink.h"

#define PAD_MODE_MASK           0x0000000F
#define PAD_STRENGTH_MASK       0x000000E0
#define PAD_TYPE_MASK           0x00000310

#define REG_PAD_CONFIG_26       0x4402E108      // antenna sel1
#define REG_PAD_CONFIG_27       0x4402E10C      // antenna sel2

char mico_ver[] = { "0.1.0" };

mico_mutex_t        stdio_tx_mutex;

//SemaphoreHandle_t x_l3_connected;
uint8_t l3_connected;

static Led_Status g_led_status;

#ifdef DOG_LED
void mico_dog_led_init();
#endif // DOG_LED

int MicoGetRfVer(char *outVersion, uint8_t inLength)
{    
    _i32 misc_result;
    
    SlVersionFull cfg_val = { 0 };
    _u8 cfg_option;
    _u8 cfg_len;
    
    cfg_option = SL_DEVICE_GENERAL_VERSION;
    cfg_len = sizeof(SlVersionFull);
  
    misc_result = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &cfg_option, 
                            &cfg_len, (_u8 *)(&cfg_val));
    
    if (misc_result == 0)
    { // success
        snprintf(outVersion, inLength, "%d.%d.%d.%d",
                 cfg_val.ChipFwAndPhyVersion.FwVersion[0],
                 cfg_val.ChipFwAndPhyVersion.FwVersion[1],
                 cfg_val.ChipFwAndPhyVersion.FwVersion[2],
                 cfg_val.ChipFwAndPhyVersion.FwVersion[3]);
    }
    else
    {
        
    }
  
    return 0;
}

char *MicoGetVer(void)
{
    return mico_ver;
}

void MicoInit(void)
{
    // initialize simplelink stack:
    _i16 wlan_result;
    
    static uint8_t mico_initialized = 0;
    
    long lRetVal = -1;
    
    char version[MICO32_GENERIC_VER_LEN] = { 0 };
    
    if (mico_initialized == 0)
    {
        mico_initialized = 1;
    }
    else
    {
        return ;
    }
    
    mico_rtos_init_mutex(&stdio_tx_mutex);      // terminal io mutex
    
    // create l3 connected mutex
    //*mutex = (mico_mutex_t)xSemaphoreCreateMutex();
    l3_connected = 0;
    
    // after os has been scheduled, we can initialize simplelink with os:
    lRetVal = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
    
    wlan_result = sl_Start(NULL, NULL, NULL);
    
    switch (wlan_result)
    {
    case ROLE_STA:
    case ROLE_AP:
    case ROLE_P2P:
        break;
    default:
        break;
    }
    
    // we need disable net app to prevent unexpected call back (like mdns):
    sl_NetAppStop(SL_NET_APP_MDNS_ID);
    
#if 0
    _i32 fs_result = -1;
    
    _i32 h_file;
    
    _u32 flash_file_size;
    
    flash_file_size = 2048*1024;
      
    sl_FsDel("/test.bin" , 0);
    sl_FsDel("/sys/test.bin" , 0);
    sl_FsDel("/tmp/test.bin" , 0);
    sl_FsDel("/cert/test.bin" , 0);
    sl_FsDel("www/test.bin" , 0);
    
    while (fs_result < 0)
    {
        // test flash available size:
        fs_result = sl_FsOpen("/test.bin",
            FS_MODE_OPEN_CREATE(flash_file_size, _FS_FILE_OPEN_FLAG_COMMIT | _FS_FILE_PUBLIC_READ | _FS_FILE_PUBLIC_WRITE),
            NULL, &h_file);
        
        UART_PRINT("f=%d, %d\r\n", flash_file_size, fs_result);

        flash_file_size -= 256;
    }
    
    LOOP_FOREVER();
#endif
    
    // print version info:
    UART_PRINT("MICO=%s, ", MicoGetVer());          // os ver
    MicoGetRfVer(version, MICO32_GENERIC_VER_LEN);      // rf ver
    UART_PRINT("RF=%s\r\n", version);
    
#ifdef DOG_LED
    mico_dog_led_init();
    DBG_PRINT("Dog Led\r\n");
#endif // DOG_LED
    
    return ;
}

micoMemInfo_t *MicoGetMemoryInfo(void)
{
    static micoMemInfo_t mem_info;
    
    memset(&mem_info, 0, sizeof(micoMemInfo_t));
    
    mem_info.total_memory       = configTOTAL_HEAP_SIZE;        // not including some overhead
    
    mem_info.free_memory        = xPortGetFreeHeapSize();
    mem_info.allocted_memory    = mem_info.total_memory - mem_info.free_memory;
    
    mem_info.num_of_chunks      = xPortGetMinimumEverFreeHeapSize();
  
    return &mem_info;
}

// it is appropriate to settle down system control api in misc section:
void MicoSystemReboot(void)
{
    // blink the 3 led several times to indicate soft-reset
    mico_led_set(LM_S0S);
    
    // delay a while:
    mico_thread_sleep(1);
  
#if 1
    PRCMMCUReset(true);     // not including wlan cpu
#else
    PRCMSOCReset();
#endif
    
    return ;
}

void MicoSystemStandBy(uint32_t secondsToWakeup)
{
#if 1
    PRCMSleepEnter();
#else
    PRCMHibernateEnter();
#endif

    return ;
}

void MicoMcuPowerSaveConfig(int enable)
{
    // not supported on cc3200

    return ;
}

int mfg_test(char *log)
{
    return 0;
}

int MicoCliInit(void)
{
    return 0;
}

void mico_mfg_test(mico_Context_t * const inContext)
{
    return ;
}

static void mico_led_timer_cb(void)
{
    static uint8_t blink_state = 0;
    uint8_t led_alphabet,       // could be 0, 1, B
            led_state;          // could be 0, 1
    
    uint8_t i_led, led_num;
    
    unsigned long timer_int;
    
    timer_int = MAP_TimerIntStatus(TIMERA3_BASE, true);
    MAP_TimerIntClear(TIMERA3_BASE, timer_int);
    
    for (i_led = MICO_POWER_LED; i_led <= MICO_RF_LED; i_led ++)
    {
        led_num = i_led - MICO_POWER_LED;       // could be 0, 1, 2
        
        led_alphabet = (g_led_status & (0xF << (led_num*4))) >> (led_num*4);
        led_state = ((led_alphabet == 0xB) ? blink_state : led_alphabet);
        
        if (led_state == 1)
        { // on:
            MicoGpioOutputHigh(i_led);
        }
        else
        { // off:
            MicoGpioOutputLow(i_led);
        }
    }
    
    blink_state = 1 - blink_state;
}

void mico_led_timer_init()
{
#ifdef MICO32_LED_TIMER
    mico_led_set(LM_S0S);
  
     // init timer:
    Timer_IF_Init(PRCM_TIMERA3, TIMERA3_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(TIMERA3_BASE, TIMER_A, mico_led_timer_cb);
    Timer_IF_Start(TIMERA3_BASE, TIMER_A, MICO32_LED_TIMER_TIMEOUT); // ms
#endif // MICO32_LED_TIMER
}

void mico_led_set(Led_Status led_status)
{
//#ifdef MICO32_LED_TIMER
    g_led_status = led_status;
    
    mico_led_timer_cb();
//#endif // MICO32_LED_TIMER
}

#ifdef DOG_LED

unsigned char g_pwm_r, g_pwm_g, g_pwm_b;

void mico_dog_led_cb()
{
    static unsigned char i_pwm;
    
    unsigned long timer_int;
    long gpio_val;
    
    //
    // Clear all pending interrupts from the timer we are
    // currently using.
    //
    timer_int = MAP_TimerIntStatus(TIMERA0_BASE, true);
    MAP_TimerIntClear(TIMERA0_BASE, timer_int);
    
    // set correspond pwm, exit AFAP:
    if (i_pwm == 0x0)
    {
        // turn on all led:
        gpio_val = 0x0;
        MAP_GPIOPinWrite(GPIOA3_BASE, 0xC       , gpio_val);
        MAP_GPIOPinWrite(GPIOA3_BASE, 0x10      , gpio_val);
    }
    if (i_pwm == g_pwm_r)
    {
        // turn off red:
        gpio_val = MAP_GPIOPinRead(GPIOA3_BASE, 0xC);
        gpio_val |= 0x8;
        MAP_GPIOPinWrite(GPIOA3_BASE, 0xC, gpio_val);

    }
    if (i_pwm == g_pwm_g)
    {
        gpio_val = 0x10;
        MAP_GPIOPinWrite(GPIOA3_BASE, 0x10, gpio_val);
    }
    if (i_pwm == g_pwm_b)
    {
        gpio_val = MAP_GPIOPinRead(GPIOA3_BASE, 0xC);
        gpio_val |= 0x4;
        MAP_GPIOPinWrite(GPIOA3_BASE, 0xC, gpio_val);
    }

done:
    i_pwm ++;
    i_pwm &= 0xF;
    
    return ;
}

// call ti stock function to set antenna gpio:
static void SetAntennaSelectionGPIOs(void)
{

    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA3, PRCM_RUN_MODE_CLK);
    MAP_GPIODirModeSet(GPIOA3_BASE,0xC,GPIO_DIR_MODE_OUT);
    
    //
    // Configure PIN_29 for GPIOOutput
    //    
    HWREG(REG_PAD_CONFIG_26) = ((HWREG(REG_PAD_CONFIG_26) & ~(PAD_STRENGTH_MASK 
                                | PAD_TYPE_MASK)) | (0x00000020 | 0x00000000 ));
    
    //
    // Set the mode.
    //
    HWREG(REG_PAD_CONFIG_26) = (((HWREG(REG_PAD_CONFIG_26) & ~PAD_MODE_MASK) |  
                                  0x00000000) & ~(3<<10));
    
    //
    // Set the direction
    //
    HWREG(REG_PAD_CONFIG_26) = ((HWREG(REG_PAD_CONFIG_26) & ~0xC00) | 0x00000800);
    
    
     //
    // Configure PIN_30 for GPIOOutput
    //
    HWREG(REG_PAD_CONFIG_27) = ((HWREG(REG_PAD_CONFIG_27) & ~(PAD_STRENGTH_MASK
                                | PAD_TYPE_MASK)) | (0x00000020 | 0x00000000 ));
    
    //
    // Set the mode.
    //
    HWREG(REG_PAD_CONFIG_27) = (((HWREG(REG_PAD_CONFIG_27) & ~PAD_MODE_MASK) |  
                                  0x00000000) & ~(3<<10));

    //
    // Set the direction
    //
    HWREG(REG_PAD_CONFIG_26) = ((HWREG(REG_PAD_CONFIG_27) & ~0xC00) | 0x00000800);
    
    
    // set gpio 28
    PinTypeGPIO(PIN_18, PIN_MODE_0, false);
    GPIODirModeSet(GPIOA3_BASE, 0x10, GPIO_DIR_MODE_OUT);
}

void mico_dog_led_init()
{
#define LED_PWM_PERIOD          0x10000

    // init 3 gpio led:
    SetAntennaSelectionGPIOs(); // pin 29 & 30

    g_pwm_r = 0x7;
    g_pwm_g = 0x0;
    g_pwm_b = 0x0; 
    
    // init timer:
    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, mico_dog_led_cb);
    Timer_IF_Start(TIMERA0_BASE, TIMER_A, 1); // 16*50=800Hz
    
    return ;
}

void mico_dog_led_set(unsigned char pwm_r, unsigned char pwm_g, unsigned char pwm_b)
{
    g_pwm_r = (pwm_r >> 4);
    g_pwm_g = (pwm_g >> 4);
    g_pwm_b = (pwm_b >> 4);
    
    DBG_PRINT("led=%1X,%1X,%1X\r\n", g_pwm_r, g_pwm_g, g_pwm_b);
    
    return ;
}

#endif // DOG_LED
