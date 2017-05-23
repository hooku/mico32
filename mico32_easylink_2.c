#include "mico32_easylink.h"
#include "mico32_common.h"

#include "MICO.h"
#include "simplelink.h"

#include "example/common/common.h"

#define MICO32_EASYLINK_CHANNEL_HOPPING_TIME_OUT        100         // ms
#define MICO32_EASYLINK_ATTEMPT                         999         // how many hoop attempt
#define MICO32_EASYLINK_MAX_PKT_SIZE                    192         // byte (reduce this value to a lower value could affect efficiency)
#define MICO32_EASYLINK_IGMP_PATTERN                    0x01, 0x00, 0x5E                        // ipv4 multicast

#define MICO32_EASYLINK_SEPERATOR_PATTERN               0x76
#define MICO32_EASYLINK_PAYLOAD_PATTERN                 0x7E

#define MICO32_EASYLINK_2_INDEX_OFFSET                  20          // udp 20B = 0 position

typedef enum _Easy_Link_2_Status
{
    EL2_INIT,
    EL2_MATCH_CHANNEL,
    EL2_MATCH_HEAD,         // 01:00:5E:76:00:00
    EL2_MATCH_BODY,         // 01:00:5E:7E:XX:XX
    EL2_MATCH_DONE,         // 01:00:5E:76:00:00
    EL2_DONE,
} Easy_Link_2_Status;

const unsigned char mico32_igmp_pattern[] = { MICO32_EASYLINK_IGMP_PATTERN };

static uint8_t *el_match_buffer;

static uint8_t easylink_client[SL_MAC_ADDR_LEN];        // mac address of client
uint8_t protocol_overhead;                              // minus overhead to obtain real udp length

static char last_ssid[32], last_key[64], last_extra[64];
static char el_ssid[32], el_key[64], el_extra[64];

static Easy_Link_2_Status el_match_status;

// this function is shard in easylink v2 & v3:
uint16_t mico32_udp_length(const void *frame, unsigned short length)
{
#define QOS_OVERHEAD    2
  
    // calculate udp length from global buffer
    uint16_t udp_length;

    struct ieee80211_header *hdr_80211 = (struct ieee80211_header *)frame;    
    
    FRAME_TYPE fm_type;

    udp_length = length - protocol_overhead;
    
    fm_type = COPY_UINT16(hdr_80211->ctl_frame) & MASK_FM;
    if (fm_type == FT_DATA_QOS)
    {
        udp_length -= QOS_OVERHEAD;     // escape the QoS bytes
    }

    return udp_length;
}

static int8_t mico32_easylink_2_find_channel(const void *frame, unsigned short length)
{
    int8_t wlan_result = -1;
    
    struct ieee80211_header *hdr_80211 = (struct ieee80211_header *)frame;
    
    _i8 i_channel;

    uint32_t            time_limit;
    
    static uint8_t      match_time = 0;
        
    if ((memcmp(hdr_80211->addr_receiver, mico32_igmp_pattern,
                sizeof(mico32_igmp_pattern)) == 0))// find multicast address
    {
        if ((hdr_80211->addr_receiver[3] == MICO32_EASYLINK_SEPERATOR_PATTERN) &&        // check to ensure correct source   
           (match_time++ > 0))
        {
            el_match_buffer = (uint8_t *)malloc(MICO32_EASYLINK_MATCH_BUFFER);
            
            // since the seperator contains udp 20B, we can measure the overhead by len-20
            // call get udp length with 0 protocol overhead to remove qos overhead
            protocol_overhead = mico32_udp_length(frame, length) - MICO32_EASYLINK_2_INDEX_OFFSET;
            
            DBG_PRINT("overhead=%d/%d\r\n", protocol_overhead, length);

            // once the correspond ap is found, we should set rx filter
            // so as to filter out garbage pkts:
            memcpy(easylink_client, hdr_80211->addr_destination, SL_MAC_ADDR_LEN);

            wlan_result = 1;
        }
        else
        {
            wlan_result = 0;    // dwell more time on current channel
        }
    }
done:
    
    return wlan_result;
}

static uint8_t mico32_easylink_2_process(const void *frame, unsigned short length)
{
    _i16 wlan_result;
    
    struct ieee80211_header *hdr_80211 = (struct ieee80211_header *)frame;

    static int8_t i_el_match = 0;
    
    uint8_t data_len_rev;
    
    if (memcmp(hdr_80211->addr_destination, mico32_igmp_pattern, sizeof(mico32_igmp_pattern)) == 0)
    {
        switch (hdr_80211->addr_destination[3])
        {
            case MICO32_EASYLINK_SEPERATOR_PATTERN: // 0x76
            {
                uint8_t ssid_len, key_len;
                
                if (el_match_status == EL2_MATCH_BODY)
                {
                    // met seperator, match finish:
                    el_match_status = EL2_MATCH_DONE;
                    
                    DBG_PRINT("EasyLink Pkt End\r\n");
                    
                    // let us try to extract ssid & key:
                    
                    // first check length..
                    ssid_len    = el_match_buffer[0];
                    key_len     = el_match_buffer[1];
                    
                    DBG_PRINT("len=%d, ssid_len=%d, key_len=%d\r\n",
                              i_el_match, ssid_len, key_len);
                    
                    // check for valid ssid & key length:
                    if ((ssid_len == 0) || (key_len == 0) ||
                        ((ssid_len + key_len) > i_el_match))
                    {
                        DBG_PRINT("length error!\r\n");
                        return -1;
                    }
                    if ((ssid_len > MAXIMAL_SSID_LENGTH) && (key_len > MAX_KEY_LEN))
                    { 
                        DBG_PRINT("length invalid..\r\n");
                        return -1;
                    }
                    
                    // copy ssid 2 ssid, key 2 key:
                    memcpy(el_ssid , &el_match_buffer[2]           , ssid_len);
                    memcpy(el_key  , &el_match_buffer[2 + ssid_len], key_len);
                    memcpy(el_extra, &el_match_buffer[2 + ssid_len + key_len], i_el_match - ssid_len - key_len);
                                        
                    // check if ssid or key has \0:
                    for (data_len_rev = ssid_len - 1; ((data_len_rev > 0) && (el_ssid[data_len_rev] != 0)); data_len_rev --)
                        ;
                    if (data_len_rev > 0)
                    {
                        DBG_PRINT("ssid data incomplete\r\n");
                        return -1;
                    }
                    
                    for (data_len_rev = key_len - 1; ((data_len_rev > 0) && (el_key[data_len_rev] != 0)); data_len_rev --)
                        ;
                    if (data_len_rev > 0)
                    {
                        DBG_PRINT("key data incomplete\r\n");
                        return -1;
                    }
                    
                    // success:
                    DBG_PRINT("EasyLink Success!!!\r\n");
                    
                    return 1;
                }
                else
                {
                    el_match_status = EL2_MATCH_HEAD;
                    i_el_match = 0;
                }
                break;
            }
            case MICO32_EASYLINK_PAYLOAD_PATTERN: // 0x7E
            {
                if (el_match_status > EL2_INIT)
                {
                    el_match_status = EL2_MATCH_BODY;
                    
                    // enhanced methology:
                    // use 802.11 pkt length to store the information by index
                    // i_el_match store the index of data
                    i_el_match = (mico32_udp_length(frame, length) - MICO32_EASYLINK_2_INDEX_OFFSET)*2;
                      
                    if ((i_el_match >= 0) &&
                        (i_el_match < MICO32_EASYLINK_MATCH_BUFFER))
                    {
                        // copy to buffer:
                        el_match_buffer[i_el_match]     = hdr_80211->addr_destination[4];
                        el_match_buffer[i_el_match + 1] = hdr_80211->addr_destination[5];
                        
#if 1
                        DBG_PRINT(">");
#endif
                    }
                }
                break;
            }
        }
#if 1
        DBG_PRINT("%02X:%02X:%02X:%02X:%02X:%02X-%d\r\n", 
                  hdr_80211->addr_destination[0], hdr_80211->addr_destination[1], 
                  hdr_80211->addr_destination[2], hdr_80211->addr_destination[3], 
                  hdr_80211->addr_destination[4], hdr_80211->addr_destination[5],
                  length);
#endif
    }
    
    return 0;
}

static int8_t mico32_easylink_2_worker(const void *frame, unsigned short length)
{
    _i16 wlan_result;
    
    int8_t el_result = 0;
      
    // not sure why static char array not works, so we defined it as global variable
    //static char last_ssid[32], last_key[64], last_extra[64];

    memset(el_ssid      , 0     , MAXIMAL_SSID_LENGTH  );
    memset(el_key       , 0     , MAX_KEY_LEN          );
    
    // we call process several time, to validate the 2 consecutive result are the same,
    // so as to get rid of unprecedent packet loss situation:
    wlan_result = mico32_easylink_2_process(frame, length);
    
    if (wlan_result == 1)
    {
        if ((memcmp(last_ssid       , el_ssid       , MAXIMAL_SSID_LENGTH) == 0) &&
            (memcmp(last_key        , el_key        , MAX_KEY_LEN) == 0) &&
            (memcmp(last_extra      , el_extra      , MAX_KEY_LEN) == 0))
        {
#if 0
            // try to figure out if stripped data has "non-continous" zero
            // e.g.: China\00\00et335x\00\00
            data_len = strlen(last_ssid);
            for (data_len_rev = MAXIMAL_SSID_LENGTH - 1; ((data_len_rev > 0) && (el_ssid[data_len_rev] == 0)); data_len_rev --)
                ;
            if (data_len != data_len_rev + 1)
                continue;
                
            data_len = strlen(last_key);
            for (data_len_rev = MAX_KEY_LEN - 1; ((data_len_rev > 0) && (el_key[data_len_rev] == 0)); data_len_rev --)
                ;
            if (data_len != data_len_rev + 1)
                continue;
#endif
            free(el_match_buffer);      // TODO:
            
            el_result = 1;
        }
    
        memcpy(last_ssid    , el_ssid       , 32);
        memcpy(last_key     , el_key        , 64);
        memcpy(last_extra   , el_extra      , 64);
    }
    
    return el_result;
}

int easylink_2_init(El_Context *context)
{
    el_match_status = EL2_INIT;
    
    protocol_overhead = 0;
    
    return 0;
}

int easylink_2_recv(El_Context *context, const void *frame, unsigned short length)
{
    struct ieee80211_header *hdr_80211 = (struct ieee80211_header *)frame;
    
    int8_t el_result;

    MICO32_EL_STATUS_TYPE mico32_80211_cfg = MICO32_EL_STATUS_CONTINUE;
    
    switch (el_match_status)
    {
        case EL2_INIT:
            // try to lock the channel:
            el_result = mico32_easylink_2_find_channel(frame, length);
            if (el_result > 0)
            {
                // we've found the channel, switch to next state, and return "CHANNEL_LOCKED"
                el_match_status = EL2_MATCH_CHANNEL;
                mico32_80211_cfg = MICO32_EL_STATUS_CHANNEL_LOCKED;
            }
            else if (el_result == 0)
            {
                mico32_80211_cfg = MICO32_EL_DWELL_MORE;
            }
            break;
        case EL2_MATCH_CHANNEL:
        case EL2_MATCH_HEAD:
        case EL2_MATCH_BODY:
        case EL2_MATCH_DONE:
             // reject non-source pkt:
            if (memcmp(hdr_80211->addr_transmitter, &easylink_client, SL_MAC_ADDR_LEN) != 0)
            {
                break;
            }

            el_result = mico32_easylink_2_worker(frame, length);
            if (el_result > 0)
            {
                el_match_status = EL2_DONE;
            }
            else if (el_result < 0)
            {
                el_match_status = EL2_MATCH_CHANNEL;        // reset status machine
            }
            else
            {
                // continue;
            }
            
            break;
        case EL2_DONE:
            mico32_80211_cfg = MICO32_EL_STATUS_COMPLETE;
            break;
        default:
            break;
    }

    return mico32_80211_cfg;
}

int easylink_2_get_result(El_Context *context, El_Result *result)
{
    memcpy(result->ssid , el_ssid       , MAXIMAL_SSID_LENGTH);
    memcpy(result->pwd  , el_key        , MAX_KEY_LEN);
    //memcpy(result->
    
    return 0;
}
