#include "mico32_easylink.h"
#include "mico32_common.h"

#include "simplelink.h"

#define SIMPLE_LINK_RAW_MSS_LIMIT           0x5B8

#define EASY_LINK_PLUS_MAGIC_START_1        0x5AA
#define EASY_LINK_PLUS_MAGIC_START_2        0x5AB
#define EASY_LINK_PLUS_MAGIC_START_3        0x5AC

#define EASY_LINK_PLUS_MAGIC_INDEX_1ST      0x500
#define EASY_LINK_PLUS_MAGIC_INDEX_LO       0x501
#define EASY_LINK_PLUS_MAGIC_INDEX_HI       0x540

#define EASY_LINK_PLUS_MASK_DATA            0xFF

#define EASY_LINK_PLUS_LS_SHIFT             8

#define EASY_LINK_PLUS_DEST                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF

typedef enum _Easy_Link_Plus_Status
{
    ELP_INIT,
    ELP_LOCK_CHANNEL,
    ELP_DONE,
} Easy_Link_Plus_Status;

const unsigned char mico32_elp_dest[] = { EASY_LINK_PLUS_DEST };

static uint8_t *el_match_buffer;

static uint8_t easylink_client[SL_MAC_ADDR_LEN];        // mac address of client

static char el_ssid[32], el_key[64], el_extra[64];

static Easy_Link_Plus_Status el_match_status;

static int8_t mico32_easylink_plus_find_channel(const void *frame, unsigned short length)
{
    int8_t wlan_result = -1;

    struct ieee80211_header *hdr_80211 = (struct ieee80211_header *)frame;

    static uint16_t last_length, last_first_index_length;

    if (length > EASY_LINK_PLUS_MAGIC_INDEX_LO)
    {
        if ((last_length == SIMPLE_LINK_RAW_MSS_LIMIT) && (length != SIMPLE_LINK_RAW_MSS_LIMIT))
        {
            // we met a new simplelink start:
            if (last_first_index_length == length)
            {
                protocol_overhead = mico32_udp_length(frame, length) - EASY_LINK_PLUS_MAGIC_INDEX_LO;
                DBG_PRINT("overhead=%d/%d\r\n", protocol_overhead, length);
              
                el_match_buffer = (uint8_t *)malloc(MICO32_EASYLINK_MATCH_BUFFER);
                
                memcpy(easylink_client, hdr_80211->addr_transmitter, SL_MAC_ADDR_LEN);

                wlan_result = 1;
                
                goto done;
            }
            
            last_first_index_length = length;
        }
        last_length = length;
        
        wlan_result = 0;
    }

done:
    return wlan_result;
}

static uint8_t mico32_easylink_plus_worker(const void *frame, unsigned short length)
{
    int8_t el_result = 0;

    uint16_t udp_length;

    static uint8_t data_ms_index = 0;       // most significant, 00~40
    uint8_t data_ls_index,              // 0~4
            i_el_match_buffer;

    uint8_t length_data;

    uint8_t total_len, ssid_len, key_len, extra_len;

    uint16_t chk_sum = 0;

    udp_length = mico32_udp_length(frame, length);
           
#if 0
    if (isprint(udp_length & 0xFF))
    {
        DBG_PRINT("%c", udp_length & 0xFF);
    }
#endif

    if ((udp_length >= EASY_LINK_PLUS_MAGIC_INDEX_LO) &&
        (udp_length <= EASY_LINK_PLUS_MAGIC_INDEX_HI))
    {
        data_ms_index = udp_length - EASY_LINK_PLUS_MAGIC_INDEX_1ST;
        goto done;
    }
    else if (length == SIMPLE_LINK_RAW_MSS_LIMIT)
    {
        data_ms_index = 0;
        goto done;
    }

    data_ls_index = (udp_length >> EASY_LINK_PLUS_LS_SHIFT) - 1;
    length_data = udp_length & EASY_LINK_PLUS_MASK_DATA;
    i_el_match_buffer = data_ms_index*4 + data_ls_index;

    el_match_buffer[i_el_match_buffer] = length_data;

    total_len   = el_match_buffer[0];
    ssid_len    = el_match_buffer[1];
    key_len     = el_match_buffer[2];
    extra_len   = total_len - 3/*len*/ - ssid_len - key_len - 2/*chksum*/;

    // reach the end of stream
    if ((total_len > 0) && (i_el_match_buffer >= (total_len - 1)))
    {
#if 1
        DBG_PRINT("total_len=%d, ssid_len=%d, key_len=%d, extra_len=%d\r\n",
                  total_len, ssid_len, key_len, extra_len);
#endif
        // calc chksum
        for (i_el_match_buffer = 0; i_el_match_buffer < total_len - 2; i_el_match_buffer ++)
        {
            chk_sum += el_match_buffer[i_el_match_buffer];
        }
#if 1
        DBG_PRINT("chksum=%04X/%02X%02X\r\n", chk_sum, el_match_buffer[total_len - 2], el_match_buffer[total_len - 1]);
#endif
        
        if (((chk_sum >> 8) == el_match_buffer[total_len - 2]) && ((chk_sum & 0xFF) == el_match_buffer[total_len - 1]))
        {
            memcpy(el_ssid  , el_match_buffer + 3, ssid_len);
            memcpy(el_key   , el_match_buffer + 3 + ssid_len, key_len);
            memcpy(el_extra , el_match_buffer + 3 + ssid_len + key_len, extra_len);

            free(el_match_buffer);

            el_result = 1;
        }
    }
    
done:
    return el_result;
}

int easylink_plus_init(El_Context *context)
{
    el_match_status = ELP_INIT;
    
    protocol_overhead = 0;
    
    return 0;
}

int easylink_plus_recv(El_Context *context, const void *frame, unsigned short length)
{
    struct ieee80211_header *hdr_80211 = (struct ieee80211_header *)frame;

    int8_t el_result;

    MICO32_EL_STATUS_TYPE mico32_80211_cfg = MICO32_EL_STATUS_CONTINUE;
    
    if (memcmp(hdr_80211->addr_destination, mico32_elp_dest, MAC_ADDR_LEN) == 0)
    {
        switch (el_match_status)
        {
        case ELP_INIT:
            el_result = mico32_easylink_plus_find_channel(frame, length);
            if (el_result > 0)
            {
                el_match_status = ELP_LOCK_CHANNEL;
                mico32_80211_cfg = MICO32_EL_STATUS_CHANNEL_LOCKED;
            }
            else if (el_result == 0)
            {
                mico32_80211_cfg = MICO32_EL_DWELL_MORE;
            }
            break;
        case ELP_LOCK_CHANNEL:
            if (memcmp(hdr_80211->addr_transmitter, &easylink_client, SL_MAC_ADDR_LEN) != 0)
            {
                break;
            }

            el_result = mico32_easylink_plus_worker(frame, length);
            if (el_result > 0)
            {
                el_match_status = ELP_DONE;
            }
            break;
        case ELP_DONE:
            mico32_80211_cfg = MICO32_EL_STATUS_COMPLETE;
        default:
            break;
        }
    }

    return mico32_80211_cfg;
}

int easylink_plus_get_result(El_Context *context, El_Result *result)
{
    memcpy(result->ssid , el_ssid       , MAXIMAL_SSID_LENGTH);
    memcpy(result->pwd  , el_key        , MAX_KEY_LEN);

    return MICO32_EL_STATUS_COMPLETE;
}
