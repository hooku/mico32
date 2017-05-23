// mico32 app entry
// (copyleft) pxj 2014
// the mico32 is a expermintal project,
// users who use it shoud use it at his/her own risk

// standard includes:
#include "mico32_common.h"
#include "simplelink.h"

// driverlib includes:
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "interrupt.h"
#include "rom.h"
#include "rom_map.h"
#include "uart.h"
#include "prcm.h"
#include "utils.h"

#include "pinmux.h"

#include "MICO.h"

extern uVectorEntry __vector_table;

extern void init_platform(void);

extern void mico_rtos_entry(void);

static void fault_isr_handler(void)
{
    // 1. save a copy of stack pointer:
    asm volatile(
        " movs r0,#4            \n"
        " movs r1, lr           \n"
        " tst r0, r1            \n"     // test whether msp/psp
        " beq.n _MSP            \n"
        " mrs r0, psp           \n"
        " b.n _HALT             \n"
      "_MSP:                    \n"
        " mrs r0, msp           \n"
      "_HALT:                   \n"
        " ldr r1, [r0, #0x14]   \n"     // sp+6
        " bkpt #0               \n"     // breakpoint
          );
  
    // 2. print out error:
    UART_PRINT("HARD FAULT:\r\n");
    //UART_PRINT("%x", );
    
    // 3. halt (like BSOD):
    LOOP_FOREVER();
    
    return ;
}

static void board_init(void)
{
#if 0    
    volatile unsigned long priority_group;

    // configure interrupt priority bits:
    priority_group = MAP_IntPriorityGroupingGet();
    MAP_IntPriorityGroupingSet(priority_group);
#endif
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
    
    // the stupid cc3200 only support 3bit in interrupt priority:
    MAP_IntPrioritySet(INT_GPIOA0, INT_PRIORITY_LVL_1);
    MAP_IntPrioritySet(INT_GPIOA1, INT_PRIORITY_LVL_1);
    MAP_IntPrioritySet(INT_GPIOA2, INT_PRIORITY_LVL_1);
    MAP_IntPrioritySet(INT_GPIOA3, INT_PRIORITY_LVL_1);
    
    // enable processor:
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    sl_DeviceDisable();
    sl_DeviceEnable();
    
    PRCMCC3200MCUInit();
}

// alias of startApplication
int main()
{
    // initialize board:
    board_init();
    PinMuxConfig();
    
#if 1
    // depricated, uart hal should handle these:
    // initialize terminal:
    InitTerm();
    ClearTerm();
    
    // install fault isr hooker to print some dbg info: 
    IntRegister(FAULT_HARD, fault_isr_handler);
#endif
  
    init_platform();            // register gpio isr for mico
    
    // never returns:
    mico_rtos_entry();
    
    return 0;
}
