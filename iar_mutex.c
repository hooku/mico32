//#include "stm32f2xx.h"

typedef void *__iar_Rmtx;

void __iar_system_Mtxinit(__iar_Rmtx *mutex)    // Initialize a system lock
{
}
void __iar_system_Mtxdst(__iar_Rmtx *mutex)     // Destroy a system lock
{


}
void __iar_system_Mtxlock(__iar_Rmtx *mutex)    // Lock a system lock
{

 vTaskSuspendAll();
}
void __iar_system_Mtxunlock(__iar_Rmtx *mutex)  // Unlock a system lock
{
  
  xTaskResumeAll();
}


void __iar_file_Mtxinit(__iar_Rmtx *mutex)    // Initialize a file lock
{
}
void __iar_file_Mtxdst(__iar_Rmtx *mutex)     // Destroy a file lock
{
}
void __iar_file_Mtxlock(__iar_Rmtx *mutex)    // Lock a file lock
{
  vTaskSuspendAll();
}
void __iar_file_Mtxunlock(__iar_Rmtx *mutex)  // Unlock a file lock
{
  xTaskResumeAll();
}

