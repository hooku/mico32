// copyleft pxj 2014
// mico on 3200 demo
#include "mico32_common.h"
#include "MICO\Demos\COM.MXCHIP.SPP\MicoDefaults.h"
#include "MICO.h"
#include "EasyLink\EasyLink.h"
#include "AirKiss\AirKiss.h"
#include "MICONotificationCenter.h"
#include "MicoSocket.h"

#if 1
// cc3200 only
#define LOOP_FOREVER() while(1)
#endif

#define MAX_TEST_SEM    2
#define MAX_TEST_QUEUE  5

mico_semaphore_t        test_semaphore;
mico_mutex_t            test_mutex;
mico_queue_t            test_queue;

// callback:
void ApListCallback(ScanResult *pApList)
{
    uint8_t i_ap;
    
    UART_PRINT("!!ApListCallback!!\r\n");
    
    for (i_ap = 0; i_ap < pApList->ApNum; i_ap ++)
    {
        // ssid:
        UART_PRINT("SSID: %s\r\n", pApList->ApList[i_ap].ssid);
        
        // rssi(a negative value):
        UART_PRINT("RSSI: %d\r\n", (signed char)pApList->ApList[i_ap].ApPower);
        
        UART_PRINT("\r\n");
    }
    
    UART_PRINT("\r\n");
    
    return ;
}

void ApListAdvCallback(ScanResult_adv *pApAdvList)
{
    uint8_t i_ap;
    
    UART_PRINT("!!ApListAdvCallback!!\r\n");
    
    for (i_ap = 0; i_ap < pApAdvList->ApNum; i_ap ++)
    {
        // ssid:
        UART_PRINT("SSID    : %s\r\n", pApAdvList->ApList[i_ap].ssid);
        
        // bssid:
        UART_PRINT("BSSID   : %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\r\n", 
                   pApAdvList->ApList[i_ap].bssid[0],
                   pApAdvList->ApList[i_ap].bssid[1],
                   pApAdvList->ApList[i_ap].bssid[2],
                   pApAdvList->ApList[i_ap].bssid[3],
                   pApAdvList->ApList[i_ap].bssid[4],
                   pApAdvList->ApList[i_ap].bssid[5]);
        
        // rssi:
        UART_PRINT("RSSI    : %d\r\n", (signed char)pApAdvList->ApList[i_ap].ApPower);
                   
        // channel:
        UART_PRINT("CHANNEL : %d\r\n", pApAdvList->ApList[i_ap].channel);
            
        // security:
        UART_PRINT("SECURITY: ");
        switch (pApAdvList->ApList[i_ap].security)
        {
            case SECURITY_TYPE_NONE:
                UART_PRINT("OPEN");
                break;
            case SECURITY_TYPE_WEP:
                UART_PRINT("WEP");
                break;
            case SECURITY_TYPE_WPA_TKIP:
            case SECURITY_TYPE_WPA_AES:
                UART_PRINT("WPA");
                break;
            case SECURITY_TYPE_WPA2_TKIP:
            case SECURITY_TYPE_WPA2_AES:
            case SECURITY_TYPE_WPA2_MIXED:
                UART_PRINT("WPA2");
                break;
            case SECURITY_TYPE_AUTO:
                UART_PRINT("AUTO");
                break;
            default:
                break;
        }
        UART_PRINT("\r\n");
        
        UART_PRINT("\r\n");
    }
    
    UART_PRINT("\r\n");
    
    return ;
}

void WifiStatusHandler(WiFiEvent status)
{
    UART_PRINT("!!WifiStatusHandler!!\r\n");
  
    switch (status)
    {
      case NOTIFY_STATION_UP:
          // we may inform connection progress that wifi is now connencted..
          UART_PRINT("Station connected\r\n");
          break;
      case NOTIFY_STATION_DOWN:
          break;
      case NOTIFY_AP_UP:
          break;
      case NOTIFY_AP_DOWN:
          break;
      default:
          break;
    }
    
    UART_PRINT("\r\n");
    
    return ;
}

void NetCallback(IPStatusTypedef *pnet)
{
    UART_PRINT("!!NetCallback!!\r\n");
  
    UART_PRINT("IP Address : %s\r\n", pnet->ip          );
    UART_PRINT("Gateway    : %s\r\n", pnet->gate        );
    UART_PRINT("Mask       : %s\r\n", pnet->mask        );
    UART_PRINT("Broadcast  : %s\r\n", pnet->broadcastip );      // related
    
    UART_PRINT("DNS Server : %s\r\n", pnet->dns         );
    
    UART_PRINT("Mac Address: %s\r\n", pnet->mac         );
    
    UART_PRINT("\r\n");
    
    return ;
}

void RptConfigmodeRslt(network_InitTypeDef_st *nwkpara)
{
}

void easylink_user_data_result(int datalen, char*data)
{
}

void socket_connected()
{
}

void connected_ap_info()
{
}

static void mico32_test_timer(void *arg)
{
    UART_PRINT("[TIME]timer callback\r\n");
    
    return ;
}

static void mico32_test_thread(void *arg)
{
    uint32_t message;
  
    // block to get semaphore
    mico_rtos_get_semaphore(&test_semaphore, MICO_WAIT_FOREVER);
    UART_PRINT("[TEST]semaphore got\r\n");
    
    mico_thread_msleep(300);
    
    if (mico_rtos_pop_from_queue(&test_queue, &message, MICO_WAIT_FOREVER) == kNoErr)
    { // success
        UART_PRINT("[TEST]queue data=0x%x\r\n", message);
    }
    
#if 0
    LOOP_FOREVER();
#else
    mico_thread_msleep(100);
#endif
      
    return ;
}

void mico32_test_osal()
{
#define MICO_TEST_TIMER_TIMEOUT         500     // ms
  
    uint32_t message;
    mico_timer_t test_timer;
    
    uint8_t i_time;
    
    UART_PRINT("===OSAL TEST===\r\n");
  
    // this function runs in daemon task
  
    // initalize tasks, semaphore, mutex, queue
    mico_thread_t test_thread;
    
    mico_rtos_init_semaphore(&test_semaphore, MAX_TEST_SEM);
    mico_rtos_init_mutex(&test_mutex);
    mico_rtos_init_queue(&test_queue, NULL, sizeof(uint32_t), MAX_TEST_QUEUE);
    
    mico_rtos_create_thread(&test_thread, MICO_APPLICATION_PRIORITY, "TEST", mico32_test_thread,
                            MICO_DEFAULT_APPLICATION_STACK_SIZE, NULL);
    
    mico_thread_msleep(100);    // wait for thread ready
    
    /* task */
    mico_rtos_suspend_thread(&test_thread);
    mico_rtos_thread_force_awake(&test_thread);
    
    mico_rtos_suspend_all_thread();
    mico_rtos_resume_all_thread();
    
    mico_thread_msleep(250);
    
    /* semaphore */
    mico_rtos_set_semaphore(&test_semaphore);
    UART_PRINT("semaphore set\r\n");
    
    mico_thread_msleep(250);
    
    /* mutex */
    if (mico_rtos_lock_mutex(&test_mutex) == kNoErr)
    { // success
        UART_PRINT("1st lock success\r\n");
    }
    else
    {
        UART_PRINT("1st lock fail\r\n");
    }
    if (mico_rtos_lock_mutex(&test_mutex) == kNoErr)
    { // success
        UART_PRINT("2nd lock success\r\n");
    }
    else
    {
        UART_PRINT("2nd lock fail\r\n");
    }
    
    mico_rtos_unlock_mutex(&test_mutex);
    
    mico_thread_msleep(250);
    
    /* queue */
    if (mico_rtos_is_queue_empty(&test_queue) == kNoErr)
    {
        UART_PRINT("queue empty\r\n");
    }
    else
    {
        UART_PRINT("queue not empty\r\n");
    }
    
    message = 0xDEADBEAF;
    mico_rtos_push_to_queue(&test_queue, &message, MICO_WAIT_FOREVER);
    
    // test if queue empty check function works:
    if (mico_rtos_is_queue_empty(&test_queue) == kNoErr)
    {
        UART_PRINT("queue empty\r\n");
    }
    else
    {
        UART_PRINT("queue not empty\r\n");
    }
    
    mico_thread_msleep(250);
    
    /* timer */
    mico_init_timer(&test_timer, MICO_TEST_TIMER_TIMEOUT, mico32_test_timer, NULL);
    mico_stop_timer(&test_timer);
    mico_reload_timer(&test_timer);
    mico_start_timer(&test_timer);
    
    mico_thread_msleep(100);
    
    if (mico_is_timer_running(&test_timer) == true)
    {
        UART_PRINT("timer running\r\n");
    }
    else
    {
        UART_PRINT("timer not running\r\n");
    }
    
    if (mico_is_timer_running(&test_timer) == true)
    {
        UART_PRINT("timer running\r\n");
    }
    else
    {
        UART_PRINT("timer not running\r\n");
    }
    
    mico_thread_msleep(MICO_TEST_TIMER_TIMEOUT*2);      // wait for test timer timeout
    
    mico_deinit_timer(&test_timer);
    
    for (i_time = 0; i_time < 10; i_time ++)
    {   
        UART_PRINT("T=%d/%d ms\r\n", mico_get_time(), mico_get_time_no_os());
        mico_thread_msleep(75);
    }
    
    // delete tasks, semaphore, mutex, queue
    // we should notify test thread gracefully exit
    mico_rtos_deinit_semaphore(&test_semaphore);
    mico_rtos_deinit_mutex(&test_mutex);
    mico_rtos_deinit_queue(&test_queue);
    
    mico_rtos_delete_thread(&test_thread);
    UART_PRINT("thread exit\r\n");
    
    UART_PRINT("\r\n");
    
    return ;
}

void mico32_test_wlan()
{
    network_InitTypeDef_st      nw_init_setting         = { 0 };
    network_InitTypeDef_adv_st  nw_init_adv_setting     = { 0 };
    IPStatusTypedef             ip_status               = { 0 };
    LinkStatusTypeDef           link_status             = { 0 };
  
    UART_PRINT("===WLAN TEST===\r\n");
  
    /* power on */
    micoWlanPowerOn();
  
#if 0
    /* scan */
    UART_PRINT("scan result:\r\n");
    micoWlanStartScan();
#endif
    
#if 0
    /* advanced scan */
    UART_PRINT("adv scan result:\r\n");
    micoWlanStartScanAdv();
#endif
    
#if 0
    /* connect (as ap) */
    // we'd connect wifi first:
    nw_init_setting.wifi_mode   = Soft_AP;
    strcpy(nw_init_setting.wifi_ssid            , MICO32_DEFAULT_SSID           );
    strcpy(nw_init_setting.wifi_key             , MICO32_DEFAULT_KEY            );
    strcpy(nw_init_setting.local_ip_addr        , MICO32_DEFAULT_IP_ADDR        );
    strcpy(nw_init_setting.net_mask             , MICO32_DEFAULT_MASK           );
    strcpy(nw_init_setting.gateway_ip_addr      , MICO32_DEFAULT_GATEWAY        );
    strcpy(nw_init_setting.dnsServer_ip_addr    , MICO32_DEFAULT_DNS            );
    nw_init_setting.dhcpMode            = MICO32_DEFAULT_DHCP_MODE      ;
    nw_init_setting.wifi_retry_interval = MICO32_DEFAULT_WIFI_RETRY_INT ;
    
    micoWlanStart(&nw_init_setting);    // see mico32_test_sock() for simple connect as station
    
    UART_PRINT("ap mode started\r\n");
    UART_PRINT("ssid=%s, key=%s\r\n", nw_init_setting.wifi_ssid, nw_init_setting.wifi_key);
    
    LOOP_FOREVER();
#endif
    
#if 0
    /* advanced connect (as station) */
    strcpy(nw_init_adv_setting.ap_info.ssid     , MICO32_DEFAULT_SSID           );
    // bssid    = { 0 }
    // channel  = 0
    nw_init_adv_setting.ap_info.security = MICO32_DEFAULT_SECURITY_TYPE;
    
    strcpy(nw_init_adv_setting.key              , MICO32_DEFAULT_KEY);
    nw_init_adv_setting.key_len                 = strlen(MICO32_DEFAULT_KEY);
    
    strcpy(nw_init_adv_setting.local_ip_addr    , MICO32_DEFAULT_IP_ADDR        );
    strcpy(nw_init_adv_setting.net_mask         , MICO32_DEFAULT_MASK           );
    strcpy(nw_init_adv_setting.gateway_ip_addr  , MICO32_DEFAULT_GATEWAY        );
    strcpy(nw_init_adv_setting.dnsServer_ip_addr, MICO32_DEFAULT_DNS            );
    
    nw_init_adv_setting.dhcpMode                = MICO32_DEFAULT_DHCP_MODE      ;
    nw_init_adv_setting.wifi_retry_interval     = MICO32_DEFAULT_WIFI_RETRY_INT ;
     
    micoWlanStartAdv(&nw_init_adv_setting);
    
    // wait several seconds before connection done:
    mico_thread_msleep(MICO32_WLAN_CONNECT_TIMEOUT);
#endif

#if 0
    /* ip status */
    micoWlanGetIPStatus(&ip_status, Station);
    switch (ip_status.dhcp)
    {
        case DHCP_Disable:
            UART_PRINT("DHCP Off");
            break;
        case DHCP_Client:
            UART_PRINT("DHCP Client");
            break;
        case DHCP_Server:
            UART_PRINT("DHCP Server");
            break;
        default:
            break;
    }
    UART_PRINT("\r\n");
    
    UART_PRINT("IP Address : %s\r\n", ip_status.ip              );
    UART_PRINT("Gateway    : %s\r\n", ip_status.gate            );
    UART_PRINT("Mask       : %s\r\n", ip_status.mask            );
    UART_PRINT("Broadcast  : %s\r\n", ip_status.broadcastip     );      // related
    
    UART_PRINT("DNS Server : %s\r\n", ip_status.dns             );
    
    UART_PRINT("Mac Address: %s\r\n", ip_status.mac             );
#endif
    
#if 0
    /* link status */
    micoWlanGetLinkStatus(&link_status);
    if (link_status.is_connected == 1)
    {
        UART_PRINT("Already connected to \"%s\"\r\n", link_status.ssid);
    }
    else
    {
        UART_PRINT("Wlan Disconnected\r\n");
    }
#endif
    
#if 0
    /* suspend/disconnect */
    micoWlanSuspend();
#endif
    
#if 1
    /* easylink */
    micoWlanStartEasyLink(EasyLink_TimeOut);
//    LOOP_FOREVER();
    
//    micoWlanStopEasyLink();
#endif
    
#if 1
    micoWlanStartEasyLinkPlus(EasyLink_TimeOut);
#endif
    
#if 1
    /* airkiss */
    micoWlanStartAirkiss(Airkiss_TimeOut);
    
//    micoWlanStopAirkiss();
#endif

#if 0
    /* wps */
    micoWlanStartWPS(EasyLink_TimeOut);
    micoWlanStopWPS();
#endif
    
#if 0
    /* power save */
    micoWlanEnablePowerSave();
    micoWlanDisablePowerSave();
#endif
  
#if 0
    /* power off */
    micoWlanPowerOff();
#endif
    
    UART_PRINT("\r\n");
    
    return ;
}

void mico32_test_sock()
{
#define MICO_TEST_SERVER_HOST   "www.baidu.com"
#define MICO_TEST_BUFFER_SIZE   1024
  
    /*
      the test_sock demo demonstates mico's capability to act as a http client
    */
    int mico_result = 0;
  
    network_InitTypeDef_st nw_init_setting      = { 0 };
    IPStatusTypedef ip_status                   = { 0 };
    
    int max_err_num, keep_alive_seconds;
    
    const char  mico_test_server_host_address[]                 = { MICO_TEST_SERVER_HOST };
    char        mico_test_server_ip_address[MAX_IP_ADDR_LEN]    = { 0 };
    
    int test_sock_fd;
    
    struct sockaddr_t test_sock_addr;
    int test_sock_addr_size;
    
    struct timeval_t time_val;
    socklen_t opt_len;
    
    const char mico_test_get_string[] = "GET / HTTP/1.1\r\n"
      "Accept: */*\r\n"
      "User-Agent: CC3200\r\n"
      "Host: www.baidu.com\r\n"
      "\r\n";
    
    size_t mico_test_get_string_len;
    
    char mico_test_buffer[MICO_TEST_BUFFER_SIZE] = { 0 };
    
    UART_PRINT("===SOCK TEST===\r\n");
  
    // since sock layer is the alias of simplelink bsd sock
    // we just test the sock via http client to baidu
    
    // we'd connect wifi first:
    nw_init_setting.wifi_mode   = Station;
    strcpy(nw_init_setting.wifi_ssid            , MICO32_DEFAULT_SSID           );
    strcpy(nw_init_setting.wifi_key             , MICO32_DEFAULT_KEY            );
    strcpy(nw_init_setting.local_ip_addr        , MICO32_DEFAULT_IP_ADDR        );
    strcpy(nw_init_setting.net_mask             , MICO32_DEFAULT_MASK           );
    strcpy(nw_init_setting.gateway_ip_addr      , MICO32_DEFAULT_GATEWAY        );
    strcpy(nw_init_setting.dnsServer_ip_addr    , MICO32_DEFAULT_DNS            );
    nw_init_setting.dhcpMode            = MICO32_DEFAULT_DHCP_MODE      ;
    nw_init_setting.wifi_retry_interval = MICO32_DEFAULT_WIFI_RETRY_INT ;
    
    UART_PRINT("Connect to \"%s\"...\r\n", nw_init_setting.wifi_ssid);
#if 1    
    micoWlanStart(&nw_init_setting);
#endif
    
    // correct strategy should wait wifi status callback:
    mico_thread_msleep(MICO32_WLAN_CONNECT_TIMEOUT);
    
    micoWlanGetIPStatus(&ip_status, Station);
    UART_PRINT("IP Address : %s\r\n", ip_status.ip);
    
    //SlSockAddrIn_t s_addr;
    
#if 1
    /* keep alive */
    // the tcp keepalive option on cc3200 is either on or off
    set_tcp_keepalive(0, 5);
    get_tcp_keepalive(&max_err_num, &keep_alive_seconds);
#endif

#if 1
    /* dns resolve */
    gethostbyname(mico_test_server_host_address, mico_test_server_ip_address, MAX_IP_ADDR_LEN);
    UART_PRINT("Server Host Name : %s\r\n", mico_test_server_host_address       );
    UART_PRINT("Server IP Address: %s\r\n", mico_test_server_ip_address         );
#endif

#if 1
    /* socket */
    test_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    /* set socket option */
    time_val.tv_sec     = 1;    // sec
    time_val.tv_usec    = 0;    // = 10000 ms?
    opt_len = sizeof(struct timeval_t);
    
    mico_result = setsockopt(test_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &time_val, opt_len);
    
    /* connect */
    //test_sock_addr.s_type       = SL_AF_INET;           // ipv4 (ignored in mico)
    test_sock_addr.s_port       = 80;
    test_sock_addr.s_ip         = inet_addr(mico_test_server_ip_address);
    //test_sock_addr.s_ip         = inet_addr("192.168.1.26");
    
    test_sock_addr_size = sizeof(struct sockaddr_t);
    
    mico_result = connect(test_sock_fd, &test_sock_addr, test_sock_addr_size);
    
    if (mico_result >= 0)
    {
        UART_PRINT("Socket Id: %d was created\r\n", mico_result);
        
        /* send */
        mico_test_get_string_len = sizeof(mico_test_get_string);
        mico_result = send(test_sock_fd, mico_test_get_string, mico_test_get_string_len, 0);
        UART_PRINT("Send:\r\n");
        UART_PRINT("%s\r\n", mico_test_get_string);
        
        /* receive */
        mico_result = recv(test_sock_fd, mico_test_buffer, MICO_TEST_BUFFER_SIZE, 0);
        
        UART_PRINT("Receive:\r\n");
        UART_PRINT("%s\r\n", mico_test_buffer);
        
        mico_result = close(test_sock_fd);
    }
    else
    {
        UART_PRINT("Connect Failed\r\n");
    }
#endif
  
    UART_PRINT("\r\n");
    
    return ;
}

void mico32_test_hal()
{
#define MICO_FLASH_TEST_DATA    "www.emlab.net"
#define MICO_RTC_TEST_TIME      57, 59, 23, 0, 31, 12, 2014-1900
#define MICO_UART_TEST_DATA     "serial_port\r\n"

    char flash_test_area[]              = { MICO_FLASH_TEST_DATA        };
    uint32_t flash_addr;
    
    mico_rtc_time_t rtc_time            = { MICO_RTC_TEST_TIME          };
    
    mico_uart_config_t uart_config      = { MICO32_UART_BAUD_RATE       ,
                                            DATA_WIDTH_8BIT             ,
                                            NO_PARITY                   ,
                                            STOP_BITS_1                 ,
                                            FLOW_CONTROL_DISABLED       ,
                                            UART_WAKEUP_DISABLE         ,
                                            };
    char uart_test_area[]               = { MICO_UART_TEST_DATA         };
    
    uint8_t i_hal_test;
    
    UART_PRINT("===HAL TEST===\r\n");
  
#if 1
    /* flash */
    MicoFlashInitialize(MICO_FLASH_FOR_PARA);
    
    flash_addr = PARA_START_ADDRESS;
    
    // read:
    memset(flash_test_area, 0, sizeof(flash_test_area));
    MicoFlashRead(MICO_FLASH_FOR_PARA, &flash_addr, (uint8_t *)flash_test_area, sizeof(flash_test_area));
    UART_PRINT("1Read: %s\r\n", flash_test_area);
    
    // write:
    memcpy(flash_test_area, MICO_FLASH_TEST_DATA,  sizeof(flash_test_area));
    UART_PRINT("2Write: %s\r\n", flash_test_area);
    MicoFlashWrite(MICO_FLASH_FOR_PARA, &flash_addr, (uint8_t *)flash_test_area, sizeof(flash_test_area));

    MicoFlashFinalize(MICO_FLASH_FOR_PARA);
    MicoFlashInitialize(MICO_FLASH_FOR_PARA);
    
    // read:
    memset(flash_test_area, 0, sizeof(flash_test_area));
    MicoFlashRead(MICO_FLASH_FOR_PARA, &flash_addr, (uint8_t *)flash_test_area, sizeof(flash_test_area));
    UART_PRINT("3Read: %s\r\n", flash_test_area);
    
    // erase:
    MicoFlashErase(MICO_FLASH_FOR_PARA, flash_addr, flash_addr + sizeof(flash_test_area));
    UART_PRINT("4Erased\r\n");
    
    // read:
    memset(flash_test_area, 0, sizeof(flash_test_area));
    MicoFlashRead(MICO_FLASH_FOR_PARA, &flash_addr, (uint8_t *)flash_test_area, sizeof(flash_test_area));
    UART_PRINT("5Read: %s\r\n", flash_test_area);

    MicoFlashFinalize(MICO_FLASH_FOR_PARA);
#endif
    
#if 0
    /* gpio */
    for (i_hal_test = 0; i_hal_test < 3; i_hal_test ++)
    {
        MicoSysLed      (false);
        MicoRfLed       (true);
        mico_thread_msleep(500);
        
        MicoSysLed      (true);
        MicoRfLed       (false);
        mico_thread_msleep(500);
    }
#endif
    
#if 0
    /* uart */
    MicoUartInitialize(MICO_UART_1, &uart_config, NULL);
    
    MicoUartSend(MICO_UART_1, uart_test_area, sizeof(uart_test_area));
    
    UART_PRINT("Press a\r\n");
    while (uart_test_area[0] != 'a')
    {
        MicoUartRecv(MICO_UART_1, uart_test_area, sizeof(uart_test_area), UART_RECV_TIMEOUT);
    }
    
    // MicoUartFinalize();
#endif
    
#if 0
    /* rtc */
    MicoRtcInitialize();
    
    MicoRtcSetTime(&rtc_time);
    
    for (i_hal_test = 0; i_hal_test < 5; i_hal_test ++)
    {
        MicoRtcGetTime(&rtc_time);
        UART_PRINT("%d/%d/%d %02d:%02d:%02d\r\n", rtc_time.year + 1900, rtc_time.month, rtc_time.date,
                   rtc_time.hr, rtc_time.min, rtc_time.sec);
        
        mico_thread_sleep(1);
    }
#endif
    
#if 1
    /* pwm */
    MicoPwmInitialize((mico_pwm_t)MICO_POWER_LED, 10000, 0);
    MicoPwmInitialize((mico_pwm_t)MICO_SYS_LED  , 10000, 0);
    MicoPwmInitialize((mico_pwm_t)MICO_RF_LED   , 10000, 0);
    
    MicoPwmInitialize((mico_pwm_t)MICO_POWER_LED, 10000, 40);
    MicoPwmStart((mico_pwm_t)MICO_POWER_LED     );
    
    MicoPwmInitialize((mico_pwm_t)MICO_SYS_LED  , 10000, 50);
    MicoPwmStart((mico_pwm_t)MICO_SYS_LED       );
    
    MicoPwmInitialize((mico_pwm_t)MICO_RF_LED   , 10000, 60);
    MicoPwmStart((mico_pwm_t)MICO_RF_LED        );
    
    for (i_hal_test = 0; i_hal_test < 100; i_hal_test ++)
    {
        MicoPwmInitialize((mico_pwm_t)MICO_POWER_LED, 10000, i_hal_test);
        MicoPwmInitialize((mico_pwm_t)MICO_SYS_LED  , 10000, i_hal_test);
        MicoPwmInitialize((mico_pwm_t)MICO_RF_LED   , 10000, i_hal_test);
        mico_thread_msleep(100);
    }

    MicoPwmStop((mico_pwm_t)MICO_POWER_LED      );
    MicoPwmStop((mico_pwm_t)MICO_SYS_LED        );
    MicoPwmStop((mico_pwm_t)MICO_RF_LED         );
#endif
    
    return ;
}

void mico32_test_misc()
{
    md5_context md5_ctx = { 0 };
    unsigned char test_text[] = { "123" };
    unsigned char text_hash[16];
  
    micoMemInfo_t *p_mem_info;
    
    UART_PRINT("===MISC TEST===\r\n");
    
    /* md5 */
    InitMd5(&md5_ctx);
    Md5Update(&md5_ctx, test_text, 3);
    Md5Final(&md5_ctx, text_hash);
    UART_PRINT("str=%s\r\n", test_text);
    UART_PRINT("md5=%X%X%X%X%X%X%X%X\r\n",
               text_hash[0], text_hash[1], text_hash[2], text_hash[3],
               text_hash[4], text_hash[5], text_hash[6], text_hash[7]);
    
    /* memory info */
    p_mem_info = MicoGetMemoryInfo();   // static pointer
  
    UART_PRINT("Total     : %d\r\n",    p_mem_info->total_memory        );
    UART_PRINT("Free      : %d\r\n",    p_mem_info->free_memory         );
    UART_PRINT("Allocated : %d\r\n",    p_mem_info->allocted_memory     );
    UART_PRINT("Chunk Free: %d\r\n",    p_mem_info->num_of_chunks       );
  
    /* led */
    MicoSysLed  (true);
    MicoRfLed   (true);
    mico_thread_msleep(500);
    MicoSysLed  (false);
    MicoRfLed   (false);
    
    UART_PRINT("\r\n");
    
    return ;
}

int application_start(void)
{  
    // init mico rtos:
    MicoInit();
    
    printf("mico demo\r\n");
    
#if 0
    mico32_test_osal();
#endif
    
#if 1
    mico32_test_wlan();
#endif
    
#if 0
    mico32_test_sock();
#endif
    
#if 0
    mico32_test_hal();
#endif
    
#if 0
    mico32_test_misc();
#endif
    
    return kNoErr;
}
