#include "mico32_easylink.h"
#include "mico32_common.h"
#include "MICO.h"
#include "simplelink.h"
#include "external/airkiss/airkiss.h"

#include "example/common/uart_if.h"
#include "example/common/common.h"

#define MICO32_EASYLINK_LIB_CHANNEL_HOPPING_TIME        120     // ms
#define MICO32_EASYLINK_LIB_CHANNEL_HOPPING_BREATH      5       // ms
#define MICO32_EASYLINK_LIB_ATTEMPT                     999
#define MICO32_EASYLINK_LIB_MAX_PKT_SIZE                1536    // should be large enough to hold easylink plus's pkt
                                                                // at Present max lenght of 802.11 frame is 2358
#define MICO32_AIRKISS_BROADCAST_ADDR                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#define MICO32_AIRKISS_IPOD_ADDR                        0x00, 0x61, 0x71, 0xE5, 0xB6, 0x67

_i8 *g_pkt_buffer;

_i16 g_el_sock;
El_Context g_el_context;

int (*easylink_lib_recv)(El_Context *context, const void *frame, unsigned short length);
int (*easylink_lib_get_result)(El_Context *context, El_Result *result);

static void mico32_easylink_lib_print_pkt(const uint8_t *frame, uint16_t length)
{
    uint16_t i_buffer;

    DBG_PRINT("\r\n");

    DBG_PRINT("packet:\r\n");

    for (i_buffer = 0; i_buffer < length; i_buffer ++)
    {
        DBG_PRINT("%.2X ", frame[i_buffer]);
        if ((i_buffer & 0xF) == 0xF)
        {
            DBG_PRINT("\r\n");
        }
    }

    DBG_PRINT("\r\n");

    return ;
}

static _i16 mico32_easylink_lib_rcvr_ptk(struct ieee80211_header **frame, unsigned short *length, _i8 channel)
{
    _i16 wlan_result;
    
    SlTransceiverRxOverHead_t   *hdr_rx_overhead;
    struct ieee80211_header     *hdr_80211;

    static uint16_t     last_seq;
    
    FRAME_TYPE          fm_type;
    DS_TYPE             ds_type;
    
    unsigned char       broadcast_addr[] = { MICO32_AIRKISS_BROADCAST_ADDR };
    
    wlan_result = sl_Recv(g_el_sock, g_pkt_buffer, MICO32_EASYLINK_LIB_MAX_PKT_SIZE, 0);

    hdr_rx_overhead = (SlTransceiverRxOverHead_t *)g_pkt_buffer;
    *frame = (struct ieee80211_header *)(g_pkt_buffer + sizeof(SlTransceiverRxOverHead_t));
    hdr_80211 = *frame;
    
    // reject channel incorrect pkt:
    if (hdr_rx_overhead->channel != channel)
    {
        return 0;
    }
    
    if ((wlan_result <= 0) ||
        (wlan_result <= (sizeof(SlTransceiverRxOverHead_t) + sizeof(struct ieee80211_header))) ||
        (wlan_result > MICO32_EASYLINK_LIB_MAX_PKT_SIZE))
    {
        return 0;
    }
    
    // calculate real 802.11 pkt length:
    *length = wlan_result - sizeof(SlTransceiverRxOverHead_t);

    // reject duplicated pkt:
    if (hdr_80211->ctl_seq == last_seq)
    {
        //DBG_PRINT("DUP!%d\r\n", *length);
        return 0;
    }
    last_seq = hdr_80211->ctl_seq;          // filter duplicate len value (TODO: we need figure out why)
    
    // extract fm & ds type:
    fm_type = COPY_UINT16(hdr_80211->ctl_frame) & MASK_FM;          // TODO: is cortex-m4 same as x86?
    //fm_type &= MASK_FM_BASE;
    ds_type = SWAP_UINT16(hdr_80211->ctl_frame) & MASK_DS;
    
#if 1
    if ((fm_type != FT_DATA) && (fm_type != FT_DATA_QOS) && (fm_type != FT_MGT_PROBE_REQ))
    {
        return 0;
    }
#endif

    return wlan_result;
}

void mico32_easylink_lib_reset()
{
    _i16 wlan_result;

    _u8 policy;
    _u16 config;

    _WlanRxFilterOperationCommandBuff_t rx_filter_mask = { 0 };

    // Switch to STA:
    wlan_result = sl_WlanSetMode(ROLE_STA);

    wlan_result = sl_Stop(0xFF);
    wlan_result = sl_Start(0, 0, 0);

    // Set Connection Policy:
    policy = SL_CONNECTION_POLICY(0, 0, 0, 0, 0);
    wlan_result = sl_WlanPolicySet(SL_POLICY_CONNECTION, policy, NULL, 0);

    // Remove All Profiles:
    wlan_result = sl_WlanProfileDel(0xFF);
    wlan_result = sl_WlanDisconnect();

    // Disable Scan:
    policy = SL_SCAN_POLICY(0);
    wlan_result = sl_WlanPolicySet(SL_POLICY_SCAN, policy, NULL, 0);

    // Set Tx Power:
    // 0 = max
    config = 0;
    wlan_result = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER,
        sizeof(config), (_u8 *)&config);

    // Set PM Policy:
    wlan_result = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);

    // Unregister mDNS:
    wlan_result = sl_NetAppMDNSUnRegisterService(0, 0);

    // Remove Filters:
    memset(rx_filter_mask.FilterIdMask, 0xFF, 8);
    wlan_result = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&rx_filter_mask,
        sizeof(_WlanRxFilterOperationCommandBuff_t));

    return ;
}

static int8_t mico32_easylink_lib_find_channel()
{
    _i16 wlan_result;
    
    int elib_result;
    
    _i8 i_channel;
    _u8 i_attempt;

    struct ieee80211_header *hdr_80211;
    unsigned short          length;
    
    struct SlTimeval_t  time_value;
    uint32_t            time_limit;

    for (i_attempt = 1; i_attempt <= MICO32_EASYLINK_LIB_ATTEMPT; i_attempt ++)
    {
        DBG_PRINT("attempt %d..\r\n", i_attempt);
 
        // hopping:
        for (i_channel = 1; i_channel <= MICO32_CHANNEL_RANGE; i_channel ++)
        {
            while (1)
            {
                g_el_sock = sl_Socket(SL_AF_RF, SL_SOCK_RAW, i_channel);
                if (g_el_sock == SL_EALREADY_ENABLED)   // close in progress..
                {
                    mico_thread_msleep(MICO32_EASYLINK_LIB_CHANNEL_HOPPING_BREATH);
                }
                else
                {
                    break;
                }
            }
            
            // could cause delay if timeout not configured
            time_value.tv_sec   = 0;
            time_value.tv_usec  = 1000;
            sl_SetSockOpt(g_el_sock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, (_u8 *)&time_value, sizeof(time_value));
            
            DBG_PRINT("[%d] ", i_channel);
            
            time_limit = mico_get_time() + MICO32_EASYLINK_LIB_CHANNEL_HOPPING_TIME;
            
            while(mico_get_time() < time_limit)
            {
                wlan_result = mico32_easylink_lib_rcvr_ptk(&hdr_80211, &length, i_channel);
                
                if (wlan_result == 0)
                {
                    continue;
                }
                
                elib_result = (*easylink_lib_recv)(&g_el_context, hdr_80211, length);
                
                switch (elib_result)
                {
                case MICO32_EL_STATUS_CHANNEL_LOCKED:
                    // this would only happen once
                    sl_Close(g_el_sock);
                    
                    DBG_PRINT("Channel Locked\r\n");
                    return i_channel;
                case MICO32_EL_STATUS_COMPLETE:
                    break;
                case MICO32_EL_DWELL_MORE:
                    time_limit += MICO32_EASYLINK_LIB_CHANNEL_HOPPING_TIME;
                    
                    DBG_PRINT("Dwell More\r\n");
                    break;
                case MICO32_EL_STATUS_CONTINUE:
                default:
                    break;
                }
            }

            wlan_result = sl_Close(g_el_sock);

            if (i_attempt > MICO32_EASYLINK_LIB_ATTEMPT)
            { // we're time out and we still cannot find sync word, simply return:
                goto done;
            }
        }
        DBG_PRINT("\r\n");
    }

done:
    return -1;
}

static uint8_t mico32_easylink_lib_worker(char *el_ssid, char *el_key, char *el_extra)
{
    _i16 wlan_result;
    
    int elib_result;

    struct ieee80211_header *hdr_80211;
    unsigned short          length;

    _i16        easylink_lib_channel;
    
    struct SlTimeval_t  time_value;
    SlSockWinsize_t     win_size = { 0 };
    _u16 size_win_size;
    
    uint16_t    udp_len;

    _u8 i_ap;
    uint32_t i_ap_crc_ssid;
    
    El_Result easylink_lib_data;
    
    g_pkt_buffer = (_i8 *)malloc(MICO32_EASYLINK_LIB_MAX_PKT_SIZE); // share buffer with find channel
    
    mico_led_set(LM_EASY_LINK_HOP_CHANNEL);
#if 1
    easylink_lib_channel = mico32_easylink_lib_find_channel();
#else
    easylink_lib_channel = 1;
#endif
    // why we need to strip out the client mac?
    // because we want to filter out the massive pkt via client mac
    
    if (easylink_lib_channel <= 0)
    {
        return -1;
    }
    
    while (1)
    {
        g_el_sock = sl_Socket(SL_AF_RF, SL_SOCK_RAW, easylink_lib_channel);
        if (g_el_sock == SL_EALREADY_ENABLED)   // close in progress..
        {
            mico_thread_msleep(MICO32_EASYLINK_LIB_CHANNEL_HOPPING_BREATH);
        }
        else
        {
            break;
        }
    }
    
    time_value.tv_sec   = 0;
    time_value.tv_usec  = 10000;
    sl_SetSockOpt(g_el_sock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, (_u8 *)&time_value, sizeof(struct SlTimeval_t));
    
#if 0
    size_win_size = sizeof(SlSockWinsize_t);
    sl_GetSockOpt(g_el_sock, SL_SOL_SOCKET, SL_SO_RCVBUF, (_u8 *)&win_size, &size_win_size);
    
    win_size.Winsize = MICO32_EASYLINK_LIB_MAX_PKT_SIZE;
    sl_SetSockOpt(g_el_sock, SL_SOL_SOCKET, SL_SO_RCVBUF, (_u8 *)&win_size, size_win_size);
#endif
    
    mico_led_set(LM_EASY_LINK_DECRYPT_PKT);
    while (1)
    {
        wlan_result = mico32_easylink_lib_rcvr_ptk(&hdr_80211, &length, easylink_lib_channel);
        
        if (wlan_result == 0)
        {
            continue;
        }
        
        elib_result = (*easylink_lib_recv)(&g_el_context, hdr_80211, length);
        
        switch (elib_result)
        {
        case MICO32_EL_STATUS_CHANNEL_LOCKED:
            // this would only happen once
            DBG_PRINT("Error, Channel Already Locked\r\n");
            break;
        case MICO32_EL_STATUS_COMPLETE:
            // upper layer already cleared memory
            easylink_lib_data.ssid      = el_ssid;   // ssid is only a ptr
            easylink_lib_data.pwd       = el_key;
            
            // note: the airkiss library itself auto malloc the memory for ssid and pwd
          
            (*easylink_lib_get_result)(&g_el_context, &easylink_lib_data);
            //memcpy(el_extra , easylink_lib_data.random, 32);    // TODO: check range
            *el_extra = easylink_lib_data.random;
            
            DBG_PRINT("ssid=%s, pwd=%s\r\n", easylink_lib_data.ssid, easylink_lib_data.pwd);
            
            DBG_PRINT("EasyLink Lib Success!!!\r\n");
            
            goto done;
            break;      // no use
        case MICO32_EL_STATUS_CONTINUE:
        default:
            break;
        }
    }
    
done:
    
    sl_Close(g_el_sock);
    
    free(g_pkt_buffer);
    
    return 1;
}

OSStatus mico32_easylink_lib_entry(char *el_ssid, char *el_key, char *el_extra, MICO32_CFG_TYPE cfg_type)
{
    OSStatus os_result = kNoErr;
    
    int elib_result = 0;
    
    const airkiss_config_t ak_conf =
    {
        (airkiss_memset_fn)&memset,
        (airkiss_memcpy_fn)&memcpy,
        (airkiss_memcmp_fn)&memcmp,
        (airkiss_printf_fn)&Report
    };
    
    DBG_PRINT("easylink enter\r\n");
    
    mico32_easylink_lib_reset();    // call easylink reset wlan api
    
    // call corresponding initialize function:
    switch (cfg_type)
    {
    case MICO32_CFG_EASYLINK_1:
    case MICO32_CFG_EASYLINK_2:
        DBG_PRINT("easylink v2\r\n");
        // install ptr function
        easylink_lib_recv = easylink_2_recv;
        easylink_lib_get_result = easylink_2_get_result;
        elib_result = easylink_2_init(&g_el_context);
        break;
    case MICO32_CFG_EASYLINK_3:
        DBG_PRINT("easylink v3\r\n");
        easylink_lib_recv = easylink_3_recv;
        easylink_lib_get_result = easylink_3_get_result;
        elib_result = easylink_3_init(&g_el_context);
        break;
    case MICO32_CFG_EASYLINK_PLUS:
        DBG_PRINT("easylink plus\r\n");
        easylink_lib_recv = easylink_plus_recv;
        easylink_lib_get_result = easylink_plus_get_result;
        elib_result = easylink_plus_init(&g_el_context);
        break;
    case MICO32_CFG_AIRKISS:
        DBG_PRINT("%s\r\n", airkiss_version());
        easylink_lib_recv = airkiss_recv;
        easylink_lib_get_result = airkiss_get_result;
        // call stupid tencent's library:
        elib_result = airkiss_init(&g_el_context, &ak_conf);
        break;
    }
    
    if (elib_result < 0)
    {
        LOOP_FOREVER();
    }
    
#ifdef MICO32_WIFI_NO_CFG
    strcpy(el_ssid, MICO32_DEFAULT_SSID);
    strcpy(el_key, MICO32_DEFAULT_KEY);
#else // MICO32_WIFI_NO_CFG
    elib_result = mico32_easylink_lib_worker(el_ssid, el_key, el_extra);
#endif // MICO32_WIFI_NO_CFG
    
    mico32_easylink_lib_reset();
    
    DBG_PRINT("Easylink Lib leave\r\n");
    mico_led_set(LM_ALL_OFF);
    
    return os_result;
}
