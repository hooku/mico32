#include "MicoWlan.h"
#include "MICONotificationCenter.h"     // WiFiEvent

// external call back handler
extern void ApListCallback(ScanResult *);
extern void ApListAdvCallback(ScanResult_adv *);
extern void WifiStatusHandler(WiFiEvent );
extern void connected_ap_info(apinfo_adv_t *, char *, int );
extern void NetCallback(IPStatusTypedef *);
extern void RptConfigmodeRslt(network_InitTypeDef_st *);
extern void easylink_user_data_result(int , char *);
extern void socket_connected(int );
extern void dns_ip_set(uint8_t *, uint32_t );                   // dns resolve
extern void system_version(char *, int );
extern void sendNotifySYSWillPowerOff(void);
extern void join_fail(OSStatus );
extern void wifi_reboot_event(void);
extern void mico_rtos_stack_overflow(char *);

//#ifdef DEBUG
#if 1
//#define ApListCallback(x,y,z)           {}    // scan finish
//#define ApListAdvCallback(x)            {}
//#define WifiStatusHandler(x)            {}    // 802.11 is connected
//#define connected_ap_info(x,y,z)        {}
//#define NetCallback(x)                  {}    // ip addr retrieved
//#define RptConfigmodeRslt(x)            {}
//#define easylink_user_data_result(x)    {}
//#define socket_connected(x)             {}
#define dns_ip_set(x)                   {}
#define system_version(x)               {}
#define sendNotifySYSWillPowerOff(x)    {}
#define join_fail(x)                    {}
#define wifi_reboot_event(x)            {}
#define mico_rtos_stack_overflow(x)     {}
#endif

extern uint8_t in_spawn_isr;

