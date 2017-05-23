#include "mico32_common.h"
#include "mico32_handler.h"

#include "MICORTOS.h"

#include "simplelink.h"

#define MICO_SPAWN_QUEUE_NAME   "m32_spawn_msg"
#define MICO_SPAWN_MSG_SIZE     1       // byte
#define MICO_SPAWN_MSG_COUNT    2

extern apinfo_adv_t g_ap_info;
extern char g_key[MAX_KEY_LEN];
extern int g_key_len;

uint8_t in_spawn_isr = 0;

mico_mutex_t msg_spawn;

static void mico_spawn_task(void *pvParameters)
{
    long gpio_pin = 0;
  
    uint8_t msg;
    
    while (1)
    {
        mico_rtos_pop_from_queue(&msg_spawn, &msg, MICO_WAIT_FOREVER);
      
        switch (msg)
        {
        case SM_GPIO:
            mico_gpio_soft_handler();

            break;
        case SM_WIFI_AP_UP:
            WifiStatusHandler(NOTIFY_STATION_UP);
            
            break;
        default:
            break;
        }
    }
    
    return ;
}

void mico_spawn_init()
{
    mico_rtos_init_queue(&msg_spawn, MICO_SPAWN_QUEUE_NAME, MICO_SPAWN_MSG_SIZE, MICO_SPAWN_MSG_COUNT);
  
    mico_rtos_create_thread(NULL, DAEMON_TASK_PRIORITY, "m32_spawn",
                            mico_spawn_task, MICO32_GENERIC_STACK_SIZE, NULL);
}

void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    WiFiEvent wifi_event;
  
    DBG_PRINT("wl+\r\n");
    
    in_spawn_isr = 1;
    
    if(pWlanEvent == NULL)
    {
        LOOP_FOREVER();
    }
    switch(pWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {       
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            
#if 0
            wifi_event = NOTIFY_STATION_UP;
            // why we not notify station up event?
            // the stupid upper layer would regard 802.11 associated as ip acquired
#endif
                        
            DBG_PRINT("l2 connected\r\n");
            mico_led_set(LM_WIFI_CONNECT);
            
            break;
        }
        case SL_WLAN_DISCONNECT_EVENT:
        {
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);
          
            wifi_event = NOTIFY_STATION_DOWN;
            break;
        }
        case SL_WLAN_STA_CONNECTED_EVENT:
        {
            wifi_event = NOTIFY_AP_UP;
            break;
        }
        case SL_WLAN_STA_DISCONNECTED_EVENT:
        {
            wifi_event = NOTIFY_AP_DOWN;
            break;
        }
        case SL_WLAN_SMART_CONFIG_COMPLETE_EVENT:
        {
            break;
        }
        case SL_WLAN_SMART_CONFIG_STOP_EVENT:
        {
            break;
        }
        default:
        {
            break;
        }
    }
    
    // call mico handler:
    WifiStatusHandler(wifi_event);
    
    in_spawn_isr = 0;
    
    DBG_PRINT("wl-\r\n");
}

void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    IPStatusTypedef ip_status = { 0 };
    
    DBG_PRINT("net+\r\n");
    
    if(pNetAppEvent == NULL)
    {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER();
    }

    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
        {
            SlIpV4AcquiredAsync_t *pEventData = NULL;
            WiFi_Interface wifi_interface;
            uint8_t msg;

            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);
            
#if 1
            inet_ntoa(ip_status.ip      , pNetAppEvent->EventData.ipAcquiredV4.ip       );
            inet_ntoa(ip_status.gate    , pNetAppEvent->EventData.ipAcquiredV4.gateway  );
            // no mask
            inet_ntoa(ip_status.dns     , pNetAppEvent->EventData.ipAcquiredV4.dns      );
            // no mac
            // no broadcast
            DBG_PRINT("l3 acquired\r\n");
            DBG_PRINT("ip=%s\r\n", ip_status.ip);
            mico_led_set(LM_IP_ACQUIRED);
            
            l3_connected = 1;
            
            // use global ap info:
            connected_ap_info(&g_ap_info, g_key, g_key_len); 
            
            msg = SM_WIFI_AP_UP;
            mico_rtos_push_to_queue(&msg_spawn, &msg, 0);
#else
            // we shouldn't call mico function to retrieve ip info, which may get block:
            wifi_interface = Station;
            micoWlanGetIPStatus(&ip_status, Station);
#endif
            
            break;
        }
        case SL_NETAPP_IP_LEASED_EVENT:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
          
            break;
        }
        case SL_NETAPP_IP_RELEASED_EVENT:
        {
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
          
            break;
        }
        default:
        {
            break;
        }
    }
    
    // call mico handler:
    NetCallback(&ip_status);
    
    DBG_PRINT("net-\r\n");
}

void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent, 
                                SlHttpServerResponse_t *pSlHttpServerResponse)
{
/*
    switch (pSlHttpServerEvent->Event)
    {
        case SL_NETAPP_HTTPGETTOKENVALUE_EVENT:
        {
            unsigned char *ptr;

            ptr = pSlHttpServerResponse->ResponseData.token_value.data;
            pSlHttpServerResponse->ResponseData.token_value.len = 0;
         
            if(memcmp(pSlHttpServerEvent->EventData.httpTokenName.data, 
                    GET_token_TEMP, strlen((const char *)GET_token_TEMP)) == 0)
            {
                float fCurrentTemp;
                TMP006DrvGetTemp(&fCurrentTemp);
                char cTemp = (char)fCurrentTemp;
                short sTempLen = itoa(cTemp,(char*)ptr);
                ptr[sTempLen++] = ' ';
                ptr[sTempLen] = 'F';
                pSlHttpServerResponse->ResponseData.token_value.len += sTempLen;

            }

            if(memcmp(pSlHttpServerEvent->EventData.httpTokenName.data, 
                      GET_token_UIC, strlen((const char *)GET_token_UIC)) == 0)
            {
                if(g_iInternetAccess==0)
                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"1");
                else
                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"0");
                pSlHttpServerResponse->ResponseData.token_value.len =  1;
            }

            if(memcmp(pSlHttpServerEvent->EventData.httpTokenName.data, 
                       GET_token_ACC, strlen((const char *)GET_token_ACC)) == 0)
            {

                ReadAccSensor();
                if(g_ucDryerRunning)
                {
                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"Running");
                    pSlHttpServerResponse->ResponseData.token_value.len += strlen("Running");
                }
                else
                {
                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"Stopped");
                    pSlHttpServerResponse->ResponseData.token_value.len += strlen("Stopped");
                }
            }
        }
            break;

        case SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT:
        {
            unsigned char led;
            unsigned char *ptr = pSlHttpServerEvent->EventData.httpPostData.token_name.data;

            //g_ucLEDStatus = 0;
            if(memcmp(ptr, POST_token, strlen((const char *)POST_token)) == 0)
            {
                ptr = pSlHttpServerEvent->EventData.httpPostData.token_value.data;
                if(memcmp(ptr, "LED", 3) != 0)
                    break;
                ptr += 3;
                led = *ptr;
                ptr += 2;
                if(led == '1')
                {
                    if(memcmp(ptr, "ON", 2) == 0)
                    {
                        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                                                g_ucLEDStatus = LED_ON;

                    }
                    else if(memcmp(ptr, "Blink", 5) == 0)
                    {
                        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                        g_ucLEDStatus = LED_BLINK;
                    }
                    else
                    {
                        GPIO_IF_LedOff(MCU_RED_LED_GPIO);
                                                g_ucLEDStatus = LED_OFF;
                    }
                }
                else if(led == '2')
                {
                    if(memcmp(ptr, "ON", 2) == 0)
                    {
                        GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
                    }
                    else if(memcmp(ptr, "Blink", 5) == 0)
                    {
                        GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
                        g_ucLEDStatus = 1;
                    }
                    else
                    {
                        GPIO_IF_LedOff(MCU_ORANGE_LED_GPIO);
                    }
                }

            }
        }
            break;
        default:
            break;
    }
*/
}

void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    DBG_PRINT("general+\r\n");
  
    if(pDevEvent == NULL)
    {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER();
    }

    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
    
    DBG_PRINT("general-\r\n");
}

void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    DBG_PRINT("sock+\r\n");
  
    if(pSock == NULL)
    {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER();
    }
    //
    // This application doesn't work w/ socket - Events are not expected
    //
    switch( pSock->Event )
    {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch( pSock->socketAsyncEvent.SockTxFailData.status )
        {
            case SL_ECLOSE:
                UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                "failed to transmit all queued packets\n\n",
                       pSock->socketAsyncEvent.SockAsyncData.sd);
                break;
            default:
                UART_PRINT("[SOCK ERROR] - TX FAILED : socket %d , reason"
                    "(%d) \n\n",
                    pSock->socketAsyncEvent.SockAsyncData.sd,
                    pSock->socketAsyncEvent.SockTxFailData.status);
        }
        break;
    default:
        UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
    }
    
    DBG_PRINT("sock-\r\n");
}
