//
#include "mico32_common.h"

// operating system abstract layer
#include "Mico.h"
#include "Common.h"
#include "MICORTOS.h"
#include "platform.h"

// freertos includes:
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include "portmacro.h"

#include "mico32_handler.h"

#include "hw_types.h"

#include "prcm.h"
#include "utils.h"
#include "rom_map.h"

mico_mutex_t idle_reclaim;
uint8_t need_idle_reclaim = 0;

extern int application_start(void);

// mico32 daemon task:
static void mico_daemon_task(void *pvParameters)
{
    mico_led_set(LM_ALL_ON);
  
    UART_PRINT(MICO32_VER "\r\n");
    
    mico_rtos_init_mutex(&idle_reclaim);
    
    // why we call mico init here?
    // because the stupid micoentrance.c read flash without intialize it,
    // but simplelink require initialize its os stack first, then access fs
    MicoInit();
    
    // initialize flash for user app:
    MicoFlashInitialize(MICO_FLASH_FOR_PARA);   // must call after simplelink init
    
    // initialize led status indicator: 
    mico_led_timer_init();
    mico_led_set(LM_ALL_OFF);
    
#if 0
    mico_thread_sleep(1000);
#endif

    mico_spawn_init();          // create a self-own isr spawn task
    
#if 1
    application_start();        // start user application
#endif
    
    LOOP_FOREVER();
    
    return ;
}

void mico_rtos_entry(void)
{
    TaskHandle_t x_mico32_daemon;
  
    // we call freertos raw apis here to enter mico
    // create mico32 daemon task, which would initialize mico main thread:
    if(xTaskCreate(mico_daemon_task, "m32_daemon", MICO32_GENERIC_STACK_SIZE,
                   NULL, DAEMON_TASK_PRIORITY, &x_mico32_daemon) != pdPASS)
    {
        // error occurred
        vTaskDelete(x_mico32_daemon);
    }

    // start task scheduler:
    vTaskStartScheduler();
}

OSStatus mico_rtos_create_thread(mico_thread_t *thread, uint8_t priority, const char *name,
                                  mico_thread_function_t function, uint32_t stack_size, void *arg)
{
    OSStatus os_result = kNoErr;

    if (xTaskCreate(function, name, stack_size, arg, (unsigned portBASE_TYPE)priority, 
                    (TaskHandle_t *)thread) != pdPASS)
    {
        os_result = kGeneralErr;
    }
  
    return os_result;
}

OSStatus mico_rtos_delete_thread(mico_thread_t *thread)
{
    OSStatus os_result = kNoErr;
     
    need_idle_reclaim = 1;
    if (thread == NULL)
    {
        vTaskDelete(NULL);
    }
    else
    {
        vTaskDelete((TaskHandle_t)*thread);
    }
    
    // actually code after here wouldn't be executed,
    // the scheduler change the function return routine
    return os_result;
}

void mico_rtos_suspend_thread(mico_thread_t *thread)
{
    vTaskSuspend((TaskHandle_t)*thread);
    
    return ;
}

#if 0
// already alias in macro
void mico_rtos_suspend_all_thread(void)
{
    vTaskSuspendAll();
  
    return ;
}

long mico_rtos_resume_all_thread(void)
{
    vTaskResumeAll();
  
    return 0;
}
#endif

OSStatus mico_rtos_thread_join(mico_thread_t *thread)
{
    OSStatus os_result = kNoErr;
  
    return os_result;
}

OSStatus mico_rtos_thread_force_awake(mico_thread_t *thread)
{
    OSStatus os_result = kNoErr;
    
    vTaskResume((TaskHandle_t)*thread);

    return os_result;
}

bool mico_rtos_is_current_thread(mico_thread_t *thread)
{
    bool is_current_thread = false;
  
    return is_current_thread;
}

void mico_thread_sleep(uint32_t seconds)
{
    int milliseconds;
    
    milliseconds = seconds*1000;
    mico_thread_msleep(milliseconds);
    
    return ;
}

void mico_thread_msleep(uint32_t milliseconds)
{
    TickType_t x_tick;
    
    x_tick = milliseconds/portTICK_PERIOD_MS;
    vTaskDelay(x_tick);
  
    return ;
}

void mico_thread_msleep_no_os(volatile uint32_t milliseconds)
{
    unsigned long delay;
  
    delay = milliseconds*(configCPU_CLOCK_HZ/1000);
    MAP_UtilsDelay(delay);
  
    return ;
}

#define MAX_SEMAPHORE_COUNT     4

OSStatus mico_rtos_init_semaphore(mico_semaphore_t *semaphore, int count)
{
    OSStatus os_result = kNoErr;

    *semaphore = (mico_semaphore_t)xSemaphoreCreateCounting(count, 0);

    return os_result;
}

// mico: set=give, idiot..
OSStatus mico_rtos_set_semaphore(mico_semaphore_t *semaphore)
{
    OSStatus os_result = kNoErr;

#if 0
    BaseType_t higher_priority_task_woken;
    
    higher_priority_task_woken = pdFALSE;
    
    if (xSemaphoreGiveFromISR(*semaphore, &higher_priority_task_woken) == pdTRUE)
#else
    if (xSemaphoreGive(*semaphore) == pdTRUE)
#endif
    {
        os_result = kNoErr;
    }
    else
    {
        os_result = kGeneralErr;
    }

    return os_result;
}

OSStatus mico_rtos_get_semaphore(mico_semaphore_t *semaphore, uint32_t timeout_ms)
{
    OSStatus os_result = kNoErr;

    if(xSemaphoreTake(*semaphore, (TickType_t)timeout_ms) == pdTRUE)
    {
        os_result = kNoErr;
    }
    else
    {
        os_result = kGeneralErr;
    }

    return os_result;
}

OSStatus mico_rtos_deinit_semaphore(mico_semaphore_t *semaphore)
{
    OSStatus os_result = kNoErr;

    vSemaphoreDelete(*semaphore);

    return os_result;
}

OSStatus mico_rtos_init_mutex(mico_mutex_t *mutex)
{
    OSStatus os_result = kNoErr;

    *mutex = (mico_mutex_t)xSemaphoreCreateBinary();
    
    // binary semaphore is created in the "empty" state,
    // so we should unlock it before use
    // (the stupid MICO want to create semaphore as empty):
    mico_rtos_unlock_mutex(mutex);

    return os_result;
}

OSStatus mico_rtos_lock_mutex(mico_mutex_t *mutex)
{
    OSStatus os_result = kNoErr;

    if(xSemaphoreTake(*mutex, portMAX_DELAY) == pdTRUE)     // mutex doesn't block
    {
        os_result = kNoErr;
    }
    else
    {
        os_result = kGeneralErr;
    }

    return os_result;
}

OSStatus mico_rtos_unlock_mutex(mico_mutex_t *mutex)
{
    OSStatus os_result = kNoErr;

    if(xSemaphoreGive(*mutex) == pdTRUE)
    {
        os_result = kNoErr;
    }
    else
    {
        os_result = kGeneralErr;
    }

    return os_result;
}

OSStatus mico_rtos_deinit_mutex(mico_mutex_t *mutex)
{
    OSStatus os_result = kNoErr;

    vSemaphoreDelete(*mutex);

    return os_result;
}

OSStatus mico_rtos_init_queue(mico_queue_t *queue, const char *name, uint32_t message_size, uint32_t number_of_messages)
{
    OSStatus os_result = kNoErr;

    *queue = xQueueCreate(number_of_messages, message_size);

    return os_result;
}

OSStatus mico_rtos_push_to_queue(mico_queue_t *queue, void *message, uint32_t timeout_ms)
{
    OSStatus os_result = kNoErr;

    if (xQueueSend(*queue, message, timeout_ms) != pdPASS)
    {
        os_result = kGeneralErr;
    }

    return os_result;
}

OSStatus mico_rtos_pop_from_queue(mico_queue_t *queue, void *message, uint32_t timeout_ms)
{
    OSStatus os_result = kNoErr;

    if (xQueueReceive(*queue, message, timeout_ms) != pdPASS)
    {
        os_result = kGeneralErr;
    }

    return os_result;
}

OSStatus mico_rtos_deinit_queue(mico_queue_t *queue)
{
    OSStatus os_result = kNoErr;

    vQueueDelete(*queue);

    return os_result;
}

bool mico_rtos_is_queue_empty(mico_queue_t *queue)
{
    bool is_queue_empty;

#if 1
    signed portBASE_TYPE is_x_queue_empty;

    // warning: this function shouldn't call from main thread
    is_x_queue_empty = xQueueIsQueueEmptyFromISR(*queue);

    if (is_x_queue_empty == pdFALSE)
    { // not empty
        is_queue_empty = kGeneralErr;
    }
    else
    {
        is_queue_empty = kNoErr;
    }
#endif

    return is_queue_empty;
}

OSStatus mico_rtos_is_queue_full(mico_queue_t* queue)
{
    OSStatus os_result = kNoErr;

#if 1
    signed portBASE_TYPE is_x_queue_full;
    
    is_x_queue_full = xQueueIsQueueFullFromISR(queue);

    if (is_x_queue_full == pdFALSE)
    { // not full
        os_result = kGeneralErr;
    }
    else
    {
        os_result = kNoErr;
    }
#else
    UBaseType_t space_available;

    space_available = uxQueueSpacesAvailable(queue);

    if (space_available == 0)
    {
        os_result = kNoErr;         // full
    }
    else
    {
        os_result = kGeneralErr;    // stupid, confusing
    }
#endif

    return os_result;
}

uint32_t mico_get_time(void)
{
    uint32_t os_time;
    TickType_t x_tick;
    
    if (need_idle_reclaim == 1)
    {
        // we should yeild to idle task to ensure it has chance to recycle memory
        mico_rtos_lock_mutex(&idle_reclaim);
        
        need_idle_reclaim = 0;
    }
    
    x_tick = xTaskGetTickCount();
    os_time = x_tick*portTICK_PERIOD_MS;
  
    return os_time;
}

uint32_t mico_get_time_no_os(void)
{
    uint32_t no_os_time;
    
    unsigned long second;
    unsigned short millisecond;
    
    // return rtc value (or systick value?)
    MAP_PRCMRTCGet(&second, &millisecond);
    
    no_os_time = millisecond;
    
    return no_os_time;
}

OSStatus mico_init_timer(mico_timer_t *timer, uint32_t time_ms, timer_handler_t function, void *arg)
{
    OSStatus os_result = kNoErr;
  
    TickType_t x_tick;
    
    // convert ms to ticks:
    x_tick = time_ms/portTICK_PERIOD_MS;
    
    timer->handle = xTimerCreate(NULL, x_tick, pdFALSE, arg, function);
    timer->function = function;
    timer->arg = arg;
    
    return os_result;
}

OSStatus mico_start_timer(mico_timer_t *timer)
{
    OSStatus os_result = kNoErr;
    
    if (xTimerStart(timer->handle, 0) != pdPASS)
    {
        os_result = kGeneralErr;
    }
    return os_result;
}

OSStatus mico_stop_timer(mico_timer_t *timer)
{
    OSStatus os_result = kNoErr;
    
    if (xTimerStop(timer->handle, 0) != pdPASS)
    {
        os_result = kGeneralErr;
    }
    return os_result;
}

OSStatus mico_reload_timer(mico_timer_t *timer)
{
    OSStatus os_result = kNoErr;
    
    if (xTimerReset(timer->handle, 0) != pdPASS)
    {
        os_result = kGeneralErr;
    }
    return os_result;
}

OSStatus mico_deinit_timer(mico_timer_t *timer)
{
    OSStatus os_result = kNoErr;
    
    if (xTimerDelete(timer->handle, 0) != pdPASS)
    {
        os_result = kGeneralErr;
    }
    return os_result;
}

bool mico_is_timer_running(mico_timer_t *timer)
{
    bool is_timer_running;
    portBASE_TYPE is_x_timer_active;
    
    is_x_timer_active = xTimerIsTimerActive(timer->handle);
    
    if (is_x_timer_active != pdFALSE)
    {
        is_timer_running = true;
    }
    else
    {
        is_timer_running = false;
    }
    
    return is_timer_running;
}

// freertos hooked functions:
void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    DBG_PRINT("%s, Line %d", pcFile, ulLine);
    asm volatile("bkpt #0");
  
    LOOP_FOREVER();
}

void vApplicationIdleHook(void)
{
#if DEBUG
    static uint8_t a;
    a ++;
#endif
    
    mico_rtos_unlock_mutex(&idle_reclaim);
}

void vApplicationMallocFailedHook()
{
    asm volatile("bkpt #0");
  
    LOOP_FOREVER();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    mico_rtos_stack_overflow(pcTaskName);       // callback
    
    LOOP_FOREVER();
}
