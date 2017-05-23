#include "external/airkiss/airkiss.h"
#include "MicoWlan.h"

#define DO_SWAP_16(x)       (((x) >> 8) | ((x & 0x00FF) << 8))
#define DO_SWAP_32(x)       (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))

#define SWAP_UINT16(x)      DO_SWAP_16(x)
#define SWAP_UINT32(x)      DO_SWAP_32(x)
#define COPY_UINT16(x)      x
#define COPY_UINT32(x)      x

#define MASK_FM             0xFF        // frame type field mask
#define MASK_FM_BASE        0x0F
#define MASK_DS             0x0003      // DS field mask
#define MASK_PROTECTED      0x40

#define MAC_ADDR_LEN        6
#define El_Context          airkiss_context_t
#define El_Result           airkiss_result_t

#define MICO32_EASYLINK_MATCH_BUFFER                128     // enough?

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
    FT_DATA_QOS             = 0x88,
} FRAME_TYPE;

typedef enum _DS_TYPE
{
    DT_ADHOC    = 0 ,
    DT_TODS         ,
    DT_FROMDS       ,
    DT_WDS          ,
} DS_TYPE;

typedef enum _MICO32_EL_STATUS_TYPE
{
    MICO32_EL_STATUS_CONTINUE           = AIRKISS_STATUS_CONTINUE,
    MICO32_EL_STATUS_CHANNEL_LOCKED     = AIRKISS_STATUS_CHANNEL_LOCKED,
    MICO32_EL_STATUS_COMPLETE           = AIRKISS_STATUS_COMPLETE,
    MICO32_EL_DWELL_MORE,       // dwell more time on current channel
} MICO32_EL_STATUS_TYPE;

typedef enum _MICO32_CFG_TYPE
{
    MICO32_CFG_EASYLINK_1,
    MICO32_CFG_EASYLINK_2,      // multicast
    MICO32_CFG_EASYLINK_3,      // probe request
    MICO32_CFG_EASYLINK_PLUS,   // broadcast
    MICO32_CFG_AIRKISS,
} MICO32_CFG_TYPE;

struct ieee80211_header
{
    uint16_t    ctl_frame;
    uint16_t    id_duration;
    uint8_t     addr_receiver[MAC_ADDR_LEN];
    uint8_t     addr_transmitter[MAC_ADDR_LEN];
    uint8_t     addr_destination[MAC_ADDR_LEN];
    uint16_t    ctl_seq;
    uint8_t     extra[0];
};

struct ieee80211_mgmt_tag_header
{
    unsigned char tag_num;
    unsigned char tag_len;
    unsigned char tag_entity;
};

const char* easylink_version(void);

extern uint8_t protocol_overhead;

uint16_t mico32_udp_length(const void *frame, unsigned short length);

int easylink_2_init(El_Context *context);
int easylink_2_recv(El_Context *context, const void *frame, unsigned short length);
int easylink_2_get_result(El_Context *context, El_Result *result);

int easylink_3_init(El_Context *context);
int easylink_3_recv(El_Context *context, const void *frame, unsigned short length);
int easylink_3_get_result(El_Context *context, El_Result *result);

int easylink_plus_init(El_Context *context);
int easylink_plus_recv(El_Context *context, const void* frame, unsigned short length);
int easylink_plus_get_result(El_Context *context, El_Result *result);
