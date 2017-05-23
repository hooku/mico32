// wlan wrapper
#include "mico32_common.h"
#include "mico32_handler.h"
#include "mico32_easylink.h"

#include "MICORTOS.h"
#include "MicoWlan.h"
#include "simplelink.h"

#define MICO32_SCAN_INTERVAL            20                      // second
#define MICO32_DEFUALT_CHANNEL          6                       // ap channel
#define MICO32_DEFUALT_HIDDEN           0                       // ssid hidden
#define MICO32_DEFUALT_SECURITY_TYPE    SL_SEC_TYPE_WPA_WPA2    // wpa2 tkip?
#define MICO32_DEFAULT_COUNTRY          "JP"                    // japan allows ch1~14
#define MICO32_DEFAULT_TX_POWER         15                      // 0~15 dB

#define MICO32_EASYLINK_EXTRA_LEN       32

// convert table for wifi sec type cc32xx<==>mico
typedef struct _Tab_Sec_Type_Cc32xx_Mico
{
    _u8                 val_sl_gene     ;       // simplelink scan security type differs simplelink security type
    _u8                 val_sl_scan     ;
    SECURITY_TYPE_E     val_mico        ;
} Tab_Sec_Type_Cc32xx_Mico;

//   simplelink gene            , simplelink scan       , mico
const Tab_Sec_Type_Cc32xx_Mico tab_sec_type_mico32[] = {
    { SL_SEC_TYPE_OPEN          , SL_SCAN_SEC_TYPE_OPEN , SECURITY_TYPE_NONE            , },
    { SL_SEC_TYPE_WEP           , SL_SCAN_SEC_TYPE_WEP  , SECURITY_TYPE_WEP             , },
    { SL_SEC_TYPE_WPA           , SL_SCAN_SEC_TYPE_WPA  , SECURITY_TYPE_WPA_TKIP        , },    /* SL_SEC_TYPE_WPA is deprecated */
    { SL_SEC_TYPE_WPA           , SL_SCAN_SEC_TYPE_WPA  , SECURITY_TYPE_WPA_AES         , },
    { SL_SEC_TYPE_WPA_WPA2      , SL_SCAN_SEC_TYPE_WPA2 , SECURITY_TYPE_WPA2_TKIP       , },
    { SL_SEC_TYPE_WPA_WPA2      , SL_SCAN_SEC_TYPE_WPA2 , SECURITY_TYPE_WPA2_AES        , },
    { SL_SEC_TYPE_WPA_WPA2      , SL_SCAN_SEC_TYPE_WPA2 , SECURITY_TYPE_WPA2_MIXED      , },
    { SL_SEC_TYPE_WPA_WPA2      , SL_SCAN_SEC_TYPE_WPA2 , SECURITY_TYPE_AUTO            , },
};

typedef enum __Sec_Type_Convert_Direct_
{
    SL_SCAN_2_MICO,
    MICO_2_SL_GENE,
    // SL_2_MICO,
} Sec_Type_Convert_Direct;

unsigned long   g_ulStatus                      = 0;            // global link(layer2 & layer3) status
//char            g_ssid[MAXIMAL_SSID_LENGTH]     = { 0 };        // global current connected ssid

apinfo_adv_t g_ap_info;                                         // global ap info
char g_key[MAX_KEY_LEN];
int g_key_len;

extern void mico32_easylink_lib_reset();
extern int airkiss_wait_report(char *, char *, char *);

// convert wlan security type mico<->simplelink
static void mico_wlan_sec_convert(SECURITY_TYPE_E *mico_sec_type, _u8 *sl_sec_type, Sec_Type_Convert_Direct direction)
{
    _u8 i_sec_type;
    
    // lookup sec type in table:
    for (i_sec_type = 0;
         i_sec_type < (sizeof(tab_sec_type_mico32)/sizeof(Tab_Sec_Type_Cc32xx_Mico));
         i_sec_type ++)
    {
        // direction:
        switch (direction)
        {
            case MICO_2_SL_GENE:
            {
                if (tab_sec_type_mico32[i_sec_type].val_mico == (*mico_sec_type))
                {
                    (*sl_sec_type) = tab_sec_type_mico32[i_sec_type].val_sl_gene;
                    return ;    // simply exit sub
                }
                break;
            }
            case SL_SCAN_2_MICO:
            {
                if (tab_sec_type_mico32[i_sec_type].val_sl_scan == (*sl_sec_type))
                {              
                    (*mico_sec_type) = tab_sec_type_mico32[i_sec_type].val_mico;
                    return ;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

OSStatus micoWlanStart(network_InitTypeDef_st *inNetworkInitPara)
{
    OSStatus os_result = kNoErr;
    
    _i16 wlan_result;

    _u8 channel, hidden, secure_type, tx_power;
    char country[2] = { MICO32_DEFAULT_COUNTRY };
    
    network_InitTypeDef_adv_st nw_init_adv_setting = { 0 };
    
    uint8_t wifi_key_len;
    
    // only non-adv mode support ap:
    if (inNetworkInitPara->wifi_mode == Soft_AP)
    {
        wlan_result = sl_WlanSetMode(ROLE_AP);
        
        wlan_result = sl_Stop(SL_STOP_TIMEOUT);
        wlan_result = sl_Start(NULL,NULL,NULL);
        
        // set ssid:
        wlan_result = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, strlen(inNetworkInitPara->wifi_ssid),
                                 inNetworkInitPara->wifi_ssid);
        
        // set channel:
        channel = MICO32_DEFUALT_CHANNEL;
        wlan_result = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_CHANNEL, 1, &channel);

        // set hidden:
        hidden = MICO32_DEFUALT_HIDDEN;
        sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_HIDDEN_SSID, 1, &hidden);

        // set secure type:
        // simple connect doesn't have security type, so we use wpa/wpa2 here:
        secure_type = MICO32_DEFUALT_SECURITY_TYPE;
        wlan_result = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SECURITY_TYPE, 1, &secure_type);

        // set password:
        wlan_result = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_PASSWORD, strlen(inNetworkInitPara->wifi_key), inNetworkInitPara->wifi_key);

        // set country code:
        wlan_result = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE, 2, country);

        // set tx power:
        tx_power = MICO32_DEFAULT_TX_POWER;
        wlan_result = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, &tx_power);
    }
    else // Station
    {
        wlan_result = sl_WlanSetMode(ROLE_STA);
        
        wlan_result = sl_Stop(SL_STOP_TIMEOUT);
        wlan_result = sl_Start(NULL,NULL,NULL);
      
        // copy to adv setting:
        memcpy(nw_init_adv_setting.ap_info.ssid, inNetworkInitPara->wifi_ssid, MAXIMAL_SSID_LENGTH);
        // nw_adv_setting.ap_info.bssid     = { 0 };
        // nw_adv_setting.ap_info.channel   = 0;
        wifi_key_len = strlen(inNetworkInitPara->wifi_key);
        if (wifi_key_len == 0)
        {
            nw_init_adv_setting.ap_info.security = SECURITY_TYPE_NONE;
        }
        else if (wifi_key_len == 5)
        {
            nw_init_adv_setting.ap_info.security = SECURITY_TYPE_WEP;
        }
        else
        {
            // TODO: missing wep, we should use "auto" mode, which use the result from scan
            nw_init_adv_setting.ap_info.security = SECURITY_TYPE_WPA2_MIXED;
        }
        
        memcpy(nw_init_adv_setting.key, inNetworkInitPara->wifi_key, MAX_KEY_LEN);
        nw_init_adv_setting.key_len = strlen(inNetworkInitPara->wifi_key);  // MAX_KEY_LEN is not correct
        
        memcpy(nw_init_adv_setting.local_ip_addr            , inNetworkInitPara->local_ip_addr      ,
               MAX_IP_ADDR_LEN);
        memcpy(nw_init_adv_setting.net_mask                 , inNetworkInitPara->net_mask           ,
               MAX_IP_ADDR_LEN);
        memcpy(nw_init_adv_setting.gateway_ip_addr          , inNetworkInitPara->gateway_ip_addr    ,
               MAX_IP_ADDR_LEN);
        memcpy(nw_init_adv_setting.dnsServer_ip_addr        , inNetworkInitPara->dnsServer_ip_addr  ,
               MAX_IP_ADDR_LEN);
        nw_init_adv_setting.dhcpMode             = inNetworkInitPara->dhcpMode;
        
        nw_init_adv_setting.wifi_retry_interval  = inNetworkInitPara->wifi_retry_interval;
        
        os_result = micoWlanStartAdv(&nw_init_adv_setting);
    }
    
    return os_result;
}

OSStatus micoWlanStartAdv(network_InitTypeDef_adv_st *inNetworkInitParaAdv)
{
    OSStatus os_result = kNoErr;
    
    _i16 wlan_result;
    
    SlSecParams_t sec_para;
    
    static char sec_wep_key[5] = { 0 };
    char *wep_key_tail;
    uint32_t wep_key_long;
    
    // try disconnect first:
    wlan_result = sl_WlanDisconnect();
    
//    mico32_easylink_reset();
    
    sec_para.Key    = inNetworkInitParaAdv->key;
    sec_para.KeyLen = inNetworkInitParaAdv->key_len;
    
    // try to guess if key is "WEP"
    // and convert "WEP" ascii to real "WEP" key,
    // e.g. "1234567890" actually = 0x12, 0x34, 0x56, 0x78, 0x90
    // however, "1234567890" could also be a "WPA2" key..
    // but in this scenario we just make it "WEP", and ignore the fault,
    // we could also make some discipline in "MICO" documentation to state that
    // only "WPA" or "WPA2" is supported, since "WEP" itself is insecure
    
    if (strlen(inNetworkInitParaAdv->key) == 10) // maybe WEP key
    {
        // test if key is pure number, then we regard it as "WEP"
        // however, 0xdeadbeef could be a issue..
        wep_key_long = strtol(inNetworkInitParaAdv->key, &wep_key_tail, 10);
        if ((wep_key_tail - inNetworkInitParaAdv->key) == 10)
        { // is "WEP"
            inNetworkInitParaAdv->ap_info.security = SECURITY_TYPE_WEP;
          
            // convert to hex:
            sscanf(inNetworkInitParaAdv->key, "%2x%2x%2x%2x%2x",
                   &sec_wep_key[0], &sec_wep_key[1], &sec_wep_key[2], &sec_wep_key[3], &sec_wep_key[4]);
            
            DBG_PRINT("WEP Key = %2x%2x%2x%2x%2x\r\n",
                      sec_wep_key[0], sec_wep_key[1], sec_wep_key[2], sec_wep_key[3], sec_wep_key[4]);
            
            sec_para.Key        = sec_wep_key;
            sec_para.KeyLen     = 5;
        }
    }
    
    // lookup sec type in table mico->simplelink:
    mico_wlan_sec_convert(&inNetworkInitParaAdv->ap_info.security, &sec_para.Type, MICO_2_SL_GENE);
    
    // save a copy of ap info for later use (e.g. connection status query):
    memcpy(&g_ap_info, &inNetworkInitParaAdv->ap_info, sizeof(apinfo_adv_t));
    memcpy(g_key, inNetworkInitParaAdv->key, inNetworkInitParaAdv->key_len);
    g_key_len = inNetworkInitParaAdv->key_len;
    
    // disable mdns:
    // why disable mdns, the bonjour use the same 5353 port
    sl_NetAppMDNSUnRegisterService(0, 0);
    sl_NetAppStop(SL_NET_APP_MDNS_ID);
    
    wlan_result = sl_WlanConnect(inNetworkInitParaAdv->ap_info.ssid,
                                 strlen(inNetworkInitParaAdv->ap_info.ssid), NULL,
                                 &sec_para, NULL);

    if (wlan_result == 0)
    { // success
        os_result = kNoErr;
    }
    else
    {
        os_result = kNoErr;
    }

    return os_result;
}

OSStatus micoWlanGetIPStatus(IPStatusTypedef *outNetpara, WiFi_Interface inInterface)
{
    OSStatus os_result = kNoErr;
        
    _u8 dhcp_status;
    _u8 cfg_len;
    
    /* we should be aware if we're in "SPAWN" task,
       once in "SPAWN" task, we should return stocked ip info rather than
       call simplelink api
    */
    
    if (in_spawn_isr == 1) // call freertos api directly
    { // we're in spawn task:
        goto done;
    }
    else
    { // not in spawn task:
        SlNetCfgIpV4Args_t ip_args = { 0 };
        _u8 mac_addr[SL_MAC_ADDR_LEN];
        _u32 ip_broadcast;
        
        cfg_len = sizeof(SlNetCfgIpV4Args_t);
        
        sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO, &dhcp_status, &cfg_len, (_u8 *)&ip_args);
        
        // convert simplelink ip uint32 to mico char[16]
        
        // 1. get ip addr:
        outNetpara->dhcp = dhcp_status;
        inet_ntoa(outNetpara->ip    , ip_args.ipV4          );
        inet_ntoa(outNetpara->gate  , ip_args.ipV4Gateway   );
        inet_ntoa(outNetpara->mask  , ip_args.ipV4Mask      );
        inet_ntoa(outNetpara->dns   , ip_args.ipV4DnsServer );
        
        // 2. get mac addr:    
        sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &cfg_len, (_u8 *)mac_addr);
        sprintf(outNetpara->mac, "%.2X%.2X%.2X%.2X%.2X%.2X",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

        // 3. calculate boradcast ip addr:
        ip_broadcast = (ip_args.ipV4 | (~ip_args.ipV4Mask));
        inet_ntoa(outNetpara->broadcastip, ip_broadcast);        
    }

done:
    
    return os_result;
}

OSStatus micoWlanGetLinkStatus(LinkStatusTypeDef *outStatus)
{
    OSStatus os_result = kNoErr;
    
    _i16 wlan_result;
    
    // we shouldn't call sl_WlanRxStatGet to retreive the current connected rssi value
    // since it does not tell apart regular pkts/connected ap pkts
    // instead, we should perform a normal scan, and extract the rssi value of the connected ap
    
    // TODO: let's wait for ti's next release of sdk
    
    // connected:
    outStatus->is_connected = GET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
    
    if (outStatus->is_connected == 1)
    {
        // rssi:
        outStatus->wifi_strength = -60;     // fake value
        
        // ssid:
        memcpy(outStatus->ssid, g_ap_info.ssid, MAXIMAL_SSID_LENGTH);
        
        // bssid:
        // we just untouch bssid, since it is rarely used
    }
    
    return os_result;
}

_u8 mico_wlan_scan(Sl_WlanNetworkEntry_t *sl_ap_list)
{
    _u8 policy;
    _u32 interval_sec;
    
    _u8 ap_count;
    
    policy = SL_SCAN_POLICY(1);         // SL_SCAN_ENABLE
    interval_sec = MICO32_SCAN_INTERVAL;
    
    // http://www.ti.com/lit/ug/swru368/swru368.pdf
    // after the scan interval is set, an immediate scan is activated
    sl_WlanPolicySet(SL_POLICY_SCAN, policy, (_u8 *)&interval_sec, sizeof(interval_sec));
    
    // http://e2e.ti.com/support/wireless_connectivity/f/968/t/354686.aspx
    // it takes about 900 msec to scan all the channels
    
    // we just block 1000 ms to wait for scan finish, then we can retrieve scan results
    
    mico_thread_msleep(MICO32_AP_SCAN_DELAY);
    
    // result hardlimit: 20 ssid
    ap_count = sl_WlanGetNetworkList(0, MICO32_AP_SCAN_COUNT, sl_ap_list);

    // disable scan
    policy = SL_SCAN_POLICY(0);         // SL_SCAN_DISABLE
    sl_WlanPolicySet(SL_POLICY_SCAN, policy, 0, 0);
  
    return ap_count;
}

void micoWlanStartScan(void)
{
#define AP_LIST_BUFFER_LEN      MICO32_AP_SCAN_COUNT*MICO32_AP_LIST_LEN
    // difference between normal scan/adv scan: result different
    // adv scan contains bssid & channel
    Sl_WlanNetworkEntry_t sl_ap_list[MICO32_AP_SCAN_COUNT] = { 0 };
  
    ScanResult scan_result;
    
    _u8 i_ap;
        
    // scan result is pretty large, may corrupt stack (20*33 = 660 Byte!)
    // so we use heap space to store..
    scan_result.ApList = malloc(AP_LIST_BUFFER_LEN);
    memset(scan_result.ApList, 0, AP_LIST_BUFFER_LEN);
    
    scan_result.ApNum = mico_wlan_scan(sl_ap_list);
    
    // copy entity to mico scan result:
    for (i_ap = 0; i_ap < MICO32_AP_SCAN_COUNT; i_ap ++)
    {
        // copy ssid 
        memcpy(scan_result.ApList[i_ap].ssid, sl_ap_list[i_ap].ssid, sl_ap_list[i_ap].ssid_len);
        
        // copy power
        // TODO: convert rssi -> power
        scan_result.ApList[i_ap].ApPower = sl_ap_list[i_ap].rssi;
    }
    
    ApListCallback(&scan_result);
    
    // free ap list
    free(scan_result.ApList);
    
    return ;
}

void micoWlanStartScanAdv(void)
{
#define AP_LIST_ADV_BUFFER_LEN          MICO32_AP_SCAN_COUNT*MICO32_AP_LIST_ADV_LEN

    Sl_WlanNetworkEntry_t sl_ap_list[MICO32_AP_SCAN_COUNT] = { 0 };
    ScanResult_adv scan_result_adv;
    
    _u8 i_ap;
    
    // malloc ap list buffer from heap
    scan_result_adv.ApList = malloc(AP_LIST_ADV_BUFFER_LEN);
    memset(scan_result_adv.ApList, 0, AP_LIST_ADV_BUFFER_LEN);
    
    scan_result_adv.ApNum = mico_wlan_scan(sl_ap_list);
    
    for (i_ap = 0; i_ap < MICO32_AP_SCAN_COUNT; i_ap ++)
    {
        // ssid
        memcpy(scan_result_adv.ApList[i_ap].ssid, sl_ap_list[i_ap].ssid, sl_ap_list[i_ap].ssid_len);
        
        // power
        scan_result_adv.ApList[i_ap].ApPower = sl_ap_list[i_ap].rssi;
        
        // bssid
        memcpy(scan_result_adv.ApList[i_ap].bssid, sl_ap_list[i_ap].bssid, SL_BSSID_LENGTH);
        
        // channel
        scan_result_adv.ApList[i_ap].channel = 0;       // not supported by simplelink
        
        // security
        mico_wlan_sec_convert(&scan_result_adv.ApList[i_ap].security, &sl_ap_list[i_ap].sec_type, SL_SCAN_2_MICO);
    }
    
    ApListAdvCallback(&scan_result_adv);
    
    free(scan_result_adv.ApList);

    return ;
}

OSStatus micoWlanPowerOff(void)
{
    OSStatus os_result = kNoErr;
    
    sl_DeviceDisable();
    
    return os_result;
}

OSStatus micoWlanPowerOn(void)
{
    OSStatus os_result = kNoErr;
    
    sl_DeviceEnable();
    
    return os_result;
}

OSStatus micoWlanSuspend(void)
{
    OSStatus os_result = kNoErr;
    
    _i16 wlan_result;
    
    // we simply call disconnect from wifi
    wlan_result = sl_WlanDisconnect();
    
    return os_result;
}

OSStatus micoWlanSuspendStation(void)
{
    OSStatus os_result = kNoErr;
    
    micoWlanSuspend();
    
    return os_result;
}

OSStatus micoWlanSuspendSoftAP(void)
{
    OSStatus os_result = kNoErr;
    
    micoWlanSuspend();
    
    return os_result;
}

OSStatus mico32_easylink_wrapper(MICO32_CFG_TYPE cfg_type)
{
    OSStatus os_result = kNoErr;
    
    _i16 wlan_result;

#ifdef MICO32_EASYLINK    
    network_InitTypeDef_st nw_init_setting = { 0 };
    
    char el_extra[MICO32_EASYLINK_EXTRA_LEN] = { 0 };
    char i_el_extra, el_extra_len;

    // enter mxchip easylink's domain
    wlan_result = mico32_easylink_lib_entry(nw_init_setting.wifi_ssid, nw_init_setting.wifi_key, el_extra, cfg_type);
    
    nw_init_setting.wifi_mode           = Station;
    nw_init_setting.dhcpMode            = MICO32_DEFAULT_DHCP_MODE      ;
    nw_init_setting.wifi_retry_interval = MICO32_DEFAULT_WIFI_RETRY_INT ;

    if (wlan_result == 0)
    {
        // notify upper layer to process extra data:
        
        // we should preprocess "easylink" extra data to feed uplayer met the
        // stupid requirements that either index = 5 or 25
        for (i_el_extra = 0; i_el_extra < MICO32_EASYLINK_EXTRA_LEN; i_el_extra ++)
        {
            if (el_extra[i_el_extra] == '#')
            {
                el_extra_len = i_el_extra + 5;
                break;
            }
        }
        // call framework handler: process extra data
        easylink_user_data_result(el_extra_len, el_extra);      // upgrade to accommodate more data
        
        // call framework handler: save configuration for connection:
        RptConfigmodeRslt(&nw_init_setting);
    }

#else // MICO32_EASYLINK
    // use ti's smartconfig instead
    wlan_result = sl_WlanSmartConfigStart(7 ,   //group ID's (BIT_0 | BIT_1 | BIT_2)
                                          1 ,   //decrypt key by AES method
                                          16,   //decryption key length for group ID0
                                          16,   //decryption key length for group ID1
                                          16,   //decryption key length for group ID2
                                          "Key0Key0Key0Key0",   //decryption key for group ID0
                                          "Key1Key1Key1Key1",   //decryption key for group ID1
                                          "Key2Key2Key2Key2"    //decryption key for group ID2
                                          );
    
    if (wlan_result == 0)
    { // smart config started successfully
        os_result = kNoErr;
    }
    else
    {
        os_result = kNoErr;
    }
#endif // MICO32_EASYLINK
}

OSStatus micoWlanStartEasyLink(int inTimeout)
{
    OSStatus os_result = kNoErr;
   
    os_result = mico32_easylink_wrapper(MICO32_CFG_EASYLINK_2);
   
    return os_result;
}

OSStatus micoWlanStartEasyLinkPlus(int inTimeout)
{
    OSStatus os_result = kNoErr;

    os_result = mico32_easylink_wrapper(MICO32_CFG_EASYLINK_PLUS);

    return os_result;
}

OSStatus micoWlanStopEasyLink(void)
{
    OSStatus os_result = kNoErr;

    _i16 wlan_result;
    
    wlan_result = sl_WlanSmartConfigStop();
    
    if (wlan_result == 0)
    { // smart config is about to be executed without errors
        os_result = kNoErr;
    }
    else
    {
        os_result = kNoErr;
    }
    
    return os_result;
}

OSStatus micoWlanStartWPS(int inTimeout)
{
    OSStatus os_result = kNoErr;
    
    _i16 wlan_result;
    
    SlSecParams_t sec_para = { 0 };
    
    sec_para.Type = SL_SEC_TYPE_WPS_PBC;
    
    wlan_result = sl_WlanConnect(NULL, 0, NULL, &sec_para, NULL);

    if (wlan_result == 0)
    { // success
        os_result = kNoErr;
    }
    else
    {
        os_result = kNoErr;
    }
    
    return os_result;
}

OSStatus micoWlanStopWPS(void)
{
    OSStatus os_result = kNoErr;
    
    _i16 wlan_result;
    
    wlan_result = sl_WlanDisconnect();
    
    if (wlan_result == 0)
    { // disconnected done
        os_result = kNoErr;
    }
    else
    { // already disconnected
        os_result = kNoErr;
    }
    
    return os_result;
}

OSStatus micoWlanStartAirkiss(int inTimeout)
{
    OSStatus os_result = kNoErr;
    
    os_result = mico32_easylink_wrapper(MICO32_CFG_AIRKISS);
    
    return os_result;
}

OSStatus micoWlanStopAirkiss(void)
{
    OSStatus os_result = kNoErr;
    
    return os_result;
}

void micoWlanEnablePowerSave(void)
{
    // ref: http://processors.wiki.ti.com/index.php/CC31xx_NWP_Power_Policies
    _i16 wlan_result;
    wlan_result = sl_WlanPolicySet(SL_POLICY_PM, SL_LOW_POWER_POLICY, NULL, 0);
    
    if (wlan_result == 0)
    { // success
    }
    else
    {
    }
  
    return ;
}

void micoWlanDisablePowerSave(void)
{
    _i16 wlan_result;
    // SL_NORMAL_POLICY(default) also makes cc3200 enter ps mode, use
    // SL_ALWAYS_ON_POLICY to make cc3200 always online
    wlan_result = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    
    if (wlan_result == 0)
    { // success
    }
    else
    {
    }
    
    return ;
}

