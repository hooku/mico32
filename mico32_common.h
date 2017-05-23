#ifndef __MICO32_COMMON_H__
#define __MICO32_COMMON_H__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "example/common/common.h"

#include "Common.h"

#if DEBUG
//#define MICO32_SOCK_DBG
#endif // DEBUG

#define MICO32_VER      "MICO32 20150423"

//#define MICO32_WIFI_NO_CFG              // enable to apply default ssid/key
#define MICO32_EASYLINK
#define MICO32_AIRKISS

#define DOG_LED

#define NOT_SUPPORT                     0

#define MICO32_GENERIC_STACK_SIZE       1500    // mico app = 1500
#define MICO32_GENERIC_VER_LEN          50

#define DAEMON_TASK_PRIORITY            7
#define AIRKIS_TASK_PRIORITY            7

#define MICO32_AP_SCAN_COUNT            10
#define MICO32_AP_SCAN_DELAY            1000    // ms

#define MICO32_AP_LIST_LEN              33      // byte
#define MICO32_AP_LIST_ADV_LEN          40

#define MICO32_CHANNEL_RANGE            11      // 1-13

#define MAX_KEY_LEN                     64
#define MAX_IP_ADDR_LEN                 16      // strlen("xxx.xxx.xxx.xxx")

#if 0
#define MICO32_DEFAULT_SECURITY_TYPE    SECURITY_TYPE_WPA2_MIXED
#define MICO32_DEFAULT_SSID             "William Xu"
#define MICO32_DEFAULT_KEY              "mx099555"
#else
#define MICO32_DEFAULT_SECURITY_TYPE    SECURITY_TYPE_WEP
#define MICO32_DEFAULT_SSID             "ChinaNet-335x"
#define MICO32_DEFAULT_KEY              "1234567890"    // 64 bit wep
#endif

#define MICO32_DEFAULT_IP_ADDR          "192.168.1.32"
#define MICO32_DEFAULT_MASK             "255.255.255.0"
#define MICO32_DEFAULT_GATEWAY          "192.168.1.1"

#define MICO32_DEFAULT_DNS              "223.5.5.5"

#if 0
#define MICO32_DEFAULT_DHCP_MODE        DHCP_Client     // use dhcp
#else
#define MICO32_DEFAULT_DHCP_MODE        DHCP_Disable
#endif

#define MICO32_DEFAULT_WIFI_RETRY_INT   5               // ms

#define MICO32_WLAN_CONNECT_TIMEOUT     5000            // delay before ip level (dhcp) done

#define MICO32_SOCK_MSS                 1460            // ?max tcp segment size

#if 1
#define MICO32_UART_BAUD_RATE           115200
#define MICO32_SYSCLK                   80000000
#define MICO32_CONSOLE                  UARTA0_BASE
#define MICO32_CONSOLE_PERIPH           PRCM_UARTA0
#endif

#define MICO32_FLASH_FILE_APPLICATION   "/sys/m32app.bin"
#define MICO32_FLASH_FILE_UPDATE        "/sys/m32upd.bin"
#define MICO32_FLASH_FILE_BOOT          "/sys/m32boo.bin"
#define MICO32_FLASH_FILE_PARA          "/sys/m32par.bin"
#define MICO32_FLASH_FILE_EX_PARA       "/sys/m32exp.bin"

#define MICO32_LED_TIMER
#define MICO32_LED_TIMER_TIMEOUT        100

#ifndef SL_MAC_ADDR_LEN
#define SL_MAC_ADDR_LEN                 6
#endif // SL_MAC_ADDR_LEN

typedef enum _Led_Status
{
    LED_000 = 0,
    // -- on --
    LED_ON = 0xF,
    LED_001 = 0xF001,
    LED_010 = 0xF010,
    LED_011 = 0xF011,
    LED_100 = 0xF100,
    LED_101 = 0xF101,
    LED_110 = 0xF110,
    LED_111 = 0xF111,
    // -- blink --
    LED_BLINK = 0xF,
    LED_00B = 0xF00B,
    LED_0B0 = 0xF0B0,
    LED_0BB = 0xF0BB,
    LED_B00 = 0xFB00,
    LED_B0B = 0xFB0B,
    LED_BB0 = 0xFBB0,
    LED_BBB = 0xFBBB,
    // -- on + blink --
    LED_ON_BLINK = 0xFFF,
    LED_01B = 0xF01B,
    LED_10B = 0xF10B,
    LED_11B = 0xF11B,
    LED_0B1 = 0xF0B1,
    LED_1B0 = 0xF1B0,
    LED_1B1 = 0xF1B1,
    LED_1BB = 0xF1BB,
    LED_B01 = 0xFB01,
    LED_B10 = 0xFB10,
    LED_B11 = 0xFB11,
    LED_B1B = 0xFB1B,
    LED_BB1 = 0xFBB1,
} Led_Status;

typedef enum _Led_Mapping
{
    LM_ALL_OFF                  = LED_000,
    LM_ALL_ON                   = LED_111,
    LM_EASY_LINK_HOP_CHANNEL    = LED_00B,
    LM_EASY_LINK_DECRYPT_PKT    = LED_0BB,
    LM_WIFI_CONNECT             = LED_001,
    LM_IP_ACQUIRED              = LED_010,
    LM_KEY2_PRESSED             = LED_011,
    LM_S0S                      = LED_BBB,
} Led_Mapping;

typedef enum _Spawn_Msg
{
    SM_GPIO,
    SM_WIFI_AP_UP,
} Spawn_Msg;

void mico_led_timer_init();
void mico_led_set       (Led_Status led_status);

extern unsigned long g_ulStatus;
extern uint8_t l3_connected;

#endif // __MICO32_COMMON_H__
