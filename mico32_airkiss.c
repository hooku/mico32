#include "mico32_common.h"
#include "MICO.h"
#include "simplelink.h"
#include "external/airkiss/airkiss.h"

#include "example/common/uart_if.h"
#include "example/common/common.h"

#define MICO32_AIRKISS_OFFICIAL_LIB

#define MICO32_AIRKISS_CHANNEL_HOPPING_TIME_OUT         120     // ms
#define MICO32_AIRKISS_ATTEMPT                          999
#define MICO32_AIRKISS_MAX_PKT_SIZE                     1024
#define MICO32_AIRKISS_BROADCAST_ADDR                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#define MICO32_AIRKISS_IPOD_ADDR                        0x00, 0x61, 0x71, 0xE5, 0xB6, 0x67

#define MICO32_AIRKISS_PREAMBLE_COUNT                   4

#define MICO32_MAGIC_CODE_RANGE_MIN                     0
#define MICO32_MAGIC_CODE_RANGE_MAX                     3
#define MICO32_PREFIX_CODE_RANGE_MIN                    4
#define MICO32_PREFIX_CODE_RANGE_MAX                    7
#define MICO32_SEQUENCE_HEADER_MASK                     0x0800  // 0 1XXX XXXX
#define MICO32_DATA_MASK                                0x1000  // 1 XXXX XXXX

#define DO_SWAP_16(x)       (((x) >> 8) | ((x & 0x00FF) << 8))
#define DO_SWAP_32(x)       (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))

#define SWAP_UINT16(x)      DO_SWAP_16(x)
#define SWAP_UINT32(x)      DO_SWAP_32(x)
#define COPY_UINT16(x)      x
#define COPY_UINT32(x)      x

#define MASK_FM             0xFF        // frame type field mask
#define MASK_FM_BASE        0x0F
#define MASK_DS             0x0003      // DS field mask

typedef enum _FRAME_TYPE
{
    /* Management */
    FT_MGT_BASE             = 0x00,
    FT_MGT_ASSOC_REQ        = 0x00,
    FT_MGT_ASSOC_RESP       = 0x10,
    FT_MGT_REASSOC_REQ      = 0x20,
    FT_MGT_REASSOC_RESP     = 0x30,
    FT_MGT_PROBE_REQ        = 0x40,
    FT_MGT_PROBE_RESP       = 0x50,
    FT_MGT_BEACON           = 0x80,
    FT_MGT_ATIM             = 0x90,
    FT_MGT_DISASSOC         = 0xA0,
    FT_MGT_AUTH             = 0xB0,
    FT_MGT_DEAUTH           = 0xC0,
    /* Control */
    FT_CTL_BASE             = 0x04,
    FT_CTL_PSPOOL           = 0xA4,
    FT_CTL_RTS              = 0xB4,
    FT_CTL_CTS              = 0xC4,
    FT_CTL_ACK              = 0xD4,
    FT_CTL_CF_END           = 0xE4,
    FT_CTL_CF_ENDACK        = 0xF4,
    /* Data */
    FT_DATA_BASE            = 0x08,
    FT_DATA                 = 0x08,
    FT_DATA_CF_ACK          = 0x18,
    FT_DATA_CF_POOL         = 0x28,
    FT_DATA_CF_ACKPOOL      = 0x38,
    FT_DATA_NULL            = 0x48,
    FT_DATA_NULL_CF_ACK     = 0x58,
    FT_DATA_NULL_CF_POOL    = 0x68,
    FT_DATA_NULL_CF_ACKPOOL = 0x78,
} FRAME_TYPE;

typedef enum _DS_TYPE
{
    DT_ADHOC    = 0 ,
    DT_TODS         ,
    DT_FROMDS       ,
    DT_WDS          ,
} DS_TYPE;

typedef enum __Air_Kiss_Match_Status_
{
    NO_MATCH                    ,       // XX:XX:XX:XX:XX:XX
    MATCH_MAGIC_CODE            ,       // 01:00:5E:76:00:00
    MATCH_PREFIX_CODE           ,       // 01:00:5E:7E:XX:XX
    MATCH_SEQUENCE_HEADER       ,       // 01:00:5E:76:00:00
    MATCH_DATA                  ,
    MATCH_DONE                  ,
} Air_Kiss_Match_Status;

_i16 g_ak_sock;
airkiss_context_t g_ak_context;

extern _i8 *g_pkt_buffer;
extern SlTransceiverRxOverHead_t       *hdr_rx_overhead;
extern struct ieee80211_header         *hdr_80211;

static _i16 mico32_airkiss_rcvr_ptk(_i8 channel)
{
    _i16 wlan_result;
    
    static uint16_t     last_seq;
    
    FRAME_TYPE          fm_type;
    DS_TYPE             ds_type;
    
    unsigned char       broadcast_addr[] = { MICO32_AIRKISS_BROADCAST_ADDR };
    
    wlan_result = sl_Recv(g_ak_sock, g_pkt_buffer, MICO32_AIRKISS_MAX_PKT_SIZE, 0);
    
    // reject channel incorrect pkt:
    if (hdr_rx_overhead->channel != channel)
    {
        return 0;
    }
    
    if ((wlan_result <= 0) ||
        (wlan_result <= (sizeof(SlTransceiverRxOverHead_t) + sizeof(struct ieee80211_header))))
    {
        return 0;
    }
    
    // reject duplicated pkt:
    if (hdr_80211->ctl_seq == last_seq)
    {
        return 0;
    }
    last_seq = hdr_80211->ctl_seq;          // filter duplicate len value (TODO: we need figure out why)
    
    // extract fm & ds type:
    fm_type = COPY_UINT16(hdr_80211->ctl_frame) & MASK_FM;          // TODO: is cortex-m4 same as x86?
    //fm_type &= MASK_FM_BASE;
    ds_type = SWAP_UINT16(hdr_80211->ctl_frame) & MASK_DS;
    
#if 1
    if (fm_type != FT_DATA)
#else
    if ((fm_type != FT_DATA) ||                                             // data frame (rigid)
        //(ds_type != DT_FROMDS) ||                                         // relay from ap
        (memcmp(hdr_80211->addr_destination, &broadcast_addr, SL_MAC_ADDR_LEN) != 0))   // broadcast
#endif
    {
        return 0;
    }
    
    return wlan_result;
}

static int8_t mico32_airkiss_find_channel(uint8_t *easylink_client)
{
    _i16 wlan_result;
    
    int ak_result;
    
    _i8 i_channel;
    _u8 i_attempt;
    
    struct SlTimeval_t  time_value;
    uint32_t            time_limit;
    
    uint8_t             preamble_len_array[MICO32_AIRKISS_PREAMBLE_COUNT];

    for (i_attempt = 1; i_attempt <= MICO32_AIRKISS_ATTEMPT; i_attempt ++)
    {
        DBG_PRINT("attempt %d..\r\n", i_attempt);
 
        // hopping:
        for (i_channel = 1; i_channel <= MICO32_CHANNEL_RANGE; i_channel ++)
        {
            g_ak_sock = sl_Socket(SL_AF_RF, SL_SOCK_RAW, i_channel);

            // could cause delay if timeout not configured
            time_value.tv_sec   = 0;
            time_value.tv_usec  = 1000;
            sl_SetSockOpt(g_ak_sock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, (_u8 *)&time_value, sizeof(time_value));       
            
            DBG_PRINT("%d, ", i_channel);
            
            time_limit = mico_get_time() + MICO32_AIRKISS_CHANNEL_HOPPING_TIME_OUT;
            
            memset(preamble_len_array, 0, MICO32_AIRKISS_PREAMBLE_COUNT);
            
            while(mico_get_time() < time_limit)
            {
                wlan_result = mico32_airkiss_rcvr_ptk(i_channel);
                
                if (wlan_result == 0)
                {
                    continue;
                }
                
                // the real "802.11" pkt len:
                wlan_result -= (sizeof(SlTransceiverRxOverHead_t));
                
                ak_result = airkiss_recv(&g_ak_context, hdr_80211, wlan_result);
                
                switch (ak_result)
                {
                case AIRKISS_STATUS_CHANNEL_LOCKED:
                    // this would only happen once
                    memcpy(easylink_client, hdr_80211->addr_transmitter, SL_MAC_ADDR_LEN);
                    sl_Close(g_ak_sock);
                    
                    DBG_PRINT("Channel Locked\r\n");
                    return i_channel;
                case AIRKISS_STATUS_COMPLETE:
                    break;
                case AIRKISS_STATUS_CONTINUE:
                default:
                    break;
                }
            }
            sl_Close(g_ak_sock);
            
            //mico_thread_msleep(MICO32_EASYLINK_CHANNEL_HOPPING_TIME_OUT);
            if (i_attempt > MICO32_AIRKISS_ATTEMPT)
            { // we're time out and we still cannot find sync word, simply return:              
                goto done;
            }
        }
        
        DBG_PRINT("\r\n");
    }

done:
    return -1;
}

static uint8_t mico32_airkiss_worker(char *ak_ssid, char *ak_key, char *ak_extra)
{
    _i16 wlan_result;
    
    int ak_result;
  
    Sl_WlanNetworkEntry_t sl_ap_list[MICO32_AP_SCAN_COUNT] = { 0 };
  
    uint8_t     airkiss_client[SL_MAC_ADDR_LEN];
    
    _i16        airkiss_channel;
    
    struct SlTimeval_t  time_value;
    
    Air_Kiss_Match_Status ak_match_status = NO_MATCH;
    
    uint16_t    udp_len;

    _u8 i_ap;
    uint32_t i_ap_crc_ssid;
    
    airkiss_result_t airkiss_data;
    
    g_pkt_buffer = (_i8 *)malloc(MICO32_AIRKISS_MAX_PKT_SIZE);
    memset(g_pkt_buffer, 0, MICO32_AIRKISS_MAX_PKT_SIZE);
    
    hdr_rx_overhead = (SlTransceiverRxOverHead_t *)g_pkt_buffer;
    hdr_80211 = (struct ieee80211_header *)(g_pkt_buffer + sizeof(SlTransceiverRxOverHead_t));
    
#if 0
    // perform a scan to figure out broadcasting ssid:
    mico_wlan_scan(sl_ap_list);
#endif
    
    mico_led_set(LM_EASY_LINK_HOP_CHANNEL);
    airkiss_channel = mico32_airkiss_find_channel(airkiss_client);
    // why we need to strip out the client mac?
    // because we want to filter out the massive pkt via client mac
    
    if (airkiss_channel <= 0)
    { // airkiss fail
        return -1;
    }
    
    // we must give co-processor a breath
    // or socket create will fail
    mico_thread_msleep(100);
    
    g_ak_sock = sl_Socket(SL_AF_RF, SL_SOCK_RAW, airkiss_channel);
    
    time_value.tv_sec   = 0;
    time_value.tv_usec  = 1000;
    sl_SetSockOpt(g_ak_sock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, (_u8 *)&time_value, sizeof(time_value));
    
    mico_led_set(LM_EASY_LINK_DECRYPT_PKT);
    while (1)
    {
        wlan_result = mico32_airkiss_rcvr_ptk(airkiss_channel);
        
        if (wlan_result == 0)
        {
            continue;
        }
        
        // reject non-source pkt:
        if (memcmp(hdr_80211->addr_transmitter, &airkiss_client, SL_MAC_ADDR_LEN) != 0)
        {
            continue;
        }
        
        wlan_result -= (sizeof(SlTransceiverRxOverHead_t));
        
        ak_result = airkiss_recv(&g_ak_context, hdr_80211, wlan_result);
        
        switch (ak_result)
        {
        case AIRKISS_STATUS_CHANNEL_LOCKED:
            // this would only happen once
            DBG_PRINT("Error, Channel Already Locked\r\n");
            break;
        case AIRKISS_STATUS_COMPLETE:
            airkiss_get_result(&g_ak_context, &airkiss_data);
            
            // upper layer already cleared memory
            
            memcpy(ak_ssid, airkiss_data.ssid, airkiss_data.ssid_length);
            memcpy(ak_key, airkiss_data.pwd, airkiss_data.pwd_length);
            *ak_extra = airkiss_data.random;
            
            DBG_PRINT("ssid=%s, pwd=%s\r\n", ak_ssid, ak_key);
            
            DBG_PRINT("AirKiss Success!!!\r\n");
            
            goto done;
            break;      // no use
        case AIRKISS_STATUS_CONTINUE:
        default:
            break;
        }
    }
    
done:
    
    sl_Close(g_ak_sock);
    
    free(g_pkt_buffer);
    
    return 1;
}

OSStatus mico32_airkiss_entry(char *ak_ssid, char *ak_key, char *ak_extra)
{
    OSStatus os_result = kNoErr;
    
    int ak_result;      // eat tencent return value
    
    const airkiss_config_t ak_conf =
    {
        (airkiss_memset_fn)&memset,
        (airkiss_memcpy_fn)&memcpy,
        (airkiss_memcmp_fn)&memcmp,
        (airkiss_printf_fn)&Report
    };
    
    DBG_PRINT("airkiss enter\r\n");
    
    mico32_easylink_reset();    // call easylink reset wlan api
    
    // call stupid tencent's airkiss initialize function:

    ak_result = airkiss_init(&g_ak_context, &ak_conf);
    if (ak_result < 0)
    {
        LOOP_FOREVER();
    }
    
    DBG_PRINT("%s\r\n", airkiss_version());
    
    // set airkiss encryption here:
    
#if 0
    ak_result = mico32_airkiss_worker(ak_ssid, ak_key, ak_extra);
#else
    
    strcpy(ak_ssid, "ChinaNet");
    strcpy(ak_key, "1234567890");    
    
    mico32_easylink_reset();
    
    DBG_PRINT("airkiss leave\r\n");
    mico_led_set(LM_ALL_OFF);
    
    return os_result;
}
