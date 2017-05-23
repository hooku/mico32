#include "mico32_easylink.h"
#include "mico32_common.h"

#include "simplelink.h"

typedef enum _Easy_Link_3_Status
{
    EL3_INIT,
    EL3_LOCK_CHANNEL,
    EL3_DONE,
} Easy_Link_3_Status;

static char el_ssid[32], el_key[64], el_extra[64];

static Easy_Link_3_Status el_match_status;

int easylink_3_init(El_Context *context)
{
    el_match_status = EL3_INIT;
    
    return 0;
}

int easylink_3_recv(El_Context *context, const void *frame, unsigned short length)
{
    MICO32_EL_STATUS_TYPE mico32_80211_cfg = MICO32_EL_STATUS_CONTINUE;
    
    struct ieee80211_header *hdr_80211 = (struct ieee80211_header *)frame;
    
    struct ieee80211_mgmt_tag_header *hdr_80211_mgmt_tag = 
      (struct ieee80211_mgmt_tag_header *)((char *)hdr_80211 + sizeof(struct ieee80211_header));

    FRAME_TYPE fm_type;
    
    char *ssid_buffer;
    
    if (el_match_status == EL3_INIT)
    {
        mico32_80211_cfg = MICO32_EL_STATUS_CHANNEL_LOCKED;
        
        el_match_status = EL3_LOCK_CHANNEL;
    }
    
    fm_type = COPY_UINT16(hdr_80211->ctl_frame) & MASK_FM;

    if ((fm_type == FT_MGT_PROBE_REQ) && (hdr_80211_mgmt_tag->tag_len > 0))
    {
        // ssid is id 0, and we just regard it is the first item: (let other guys to fix this)
        ssid_buffer = &hdr_80211_mgmt_tag->tag_entity;
        
        // insert a \0 at the tail
        ssid_buffer[hdr_80211_mgmt_tag->tag_len] = '\0';
        
        DBG_PRINT("Probe:%s\r\n", ssid_buffer);
        
        //strcpy(ssid_buffer, "C8120001#wow2@password");
        sscanf(ssid_buffer, "%[^'#']#%[^'@']@%s", el_extra, el_ssid, el_key);
        
        if ((strlen(el_ssid) > 0) && (strlen(el_key) > 0))
        {
            mico32_80211_cfg = MICO32_EL_STATUS_COMPLETE;
        }
    }

done:
    return mico32_80211_cfg;
}
                       
int easylink_3_get_result(El_Context *context, El_Result *result)
{
    memcpy(result->ssid , el_ssid       , MAXIMAL_SSID_LENGTH);
    memcpy(result->pwd  , el_key        , MAX_KEY_LEN);
    
    return 0;
}
