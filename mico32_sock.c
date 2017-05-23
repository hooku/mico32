// network wrapper
#include "MICO.h"
#include "mico32_common.h"

#include "simplelink.h"

#include "lwip/ip_addr.h"

#include "MicoSocket.h"

#ifdef MICO32_SOCK_DBG
#define SCK_DBG_PRINT(...)      DBG_PRINT(##__VA_ARGS__)
#else // MICO32_SOCK_DBG
#define SCK_DBG_PRINT(...)      {}
#endif // MICO32_SOCK_DBG

// convert table for wifi sec type cc32xx<==>mico
typedef struct __Tab_Sock_Opt_Name_Cc32xx_Mico_
{
    _i16                val_sl;
    _i16                val_mico;
} Tab_Sock_Opt_Name_Cc32xx_Mico;

//   simplelink         , mico , TODO:
const Tab_Sock_Opt_Name_Cc32xx_Mico tab_sock_opt_name_mico32[] = {
    { NOT_SUPPORT               , SO_REUSEADDR          },
    { NOT_SUPPORT               , SO_BROADCAST          },
    { SL_IP_ADD_MEMBERSHIP      , IP_ADD_MEMBERSHIP     },      // TODO:
    { SL_IP_DROP_MEMBERSHIP     , IP_DROP_MEMBERSHIP    },
    { NOT_SUPPORT               , TCP_CONN_NUM          },
    { NOT_SUPPORT               , TCP_MAX_CONN_NUM      },
    { SL_SO_NONBLOCKING         , SO_BLOCKMODE          },
    { NOT_SUPPORT               , SO_SNDTIMEO           },
    { SL_SO_RCVTIMEO            , SO_RCVTIMEO           },
    { NOT_SUPPORT               , SO_ERROR              },
    { NOT_SUPPORT               , SO_TYPE               },
    { NOT_SUPPORT               , SO_NO_CHECK           },
};

typedef enum __Sock_Opt_Convert_Direct_
{
    SL_2_MICO,
    MICO_2_SL,
} Sock_Opt_Convert_Direct;

int socket(int domain, int type, int protocol)
{
    int net_result;
    
    SlSockNonblocking_t enable_option;
    
    SCK_DBG_PRINT("+sck\r\n");
    
    // we should wait l3 connected, then we can create sock..
    while (l3_connected == 0)
    {
        // it is wrong to call sock function before l2 is connected
        vTaskDelay(1);
    }
    net_result = sl_Socket(domain, type, protocol);
    
#if 0
    enable_option.NonblockingEnabled = 1;
    net_result = sl_SetSockOpt(net_result, SL_SOL_SOCKET, SL_SO_NONBLOCKING, (_u8 *)&enable_option, sizeof(SlSockNonblocking_t));
#endif
    
    SCK_DBG_PRINT("-sck\r\n");
    
    return net_result;
}

// convert socket option mico<->simplelink
static void mico_sock_opt_convert(_i16 *mico_sock_opt_name, _i16 *sl_sock_opt_name, Sock_Opt_Convert_Direct direction)
{
    _u8 i_sock_opt_name;
  
// lookup socket option in table:
    for (i_sock_opt_name = 0;
         i_sock_opt_name < (sizeof(tab_sock_opt_name_mico32)/sizeof(Tab_Sock_Opt_Name_Cc32xx_Mico));
         i_sock_opt_name ++)
    {
        // direction:
        switch (direction)
        {
            case MICO_2_SL:
            {
                if (tab_sock_opt_name_mico32[i_sock_opt_name].val_mico == (*mico_sock_opt_name))
                {
                    (*sl_sock_opt_name) = tab_sock_opt_name_mico32[i_sock_opt_name].val_sl;
                    return ;    // simply exit sub
                }
                break;
            }
            case SL_2_MICO:
            {
                if (tab_sock_opt_name_mico32[i_sock_opt_name].val_sl == (*sl_sock_opt_name))
                {              
                    (*mico_sock_opt_name) = tab_sock_opt_name_mico32[i_sock_opt_name].val_mico;
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

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    volatile int net_result;
    
    _i16 mico_sock_opt_name, sl_sock_opt_name;
    
    SlSockNonblocking_t enable_option;
    
    struct SlTimeval_t time_val = { 0 };
    
    int time_in;
    
    level = SL_SOL_SOCKET;
    mico_sock_opt_name = optname;
    
    sl_sock_opt_name = 0;
    mico_sock_opt_convert(&mico_sock_opt_name, &sl_sock_opt_name, MICO_2_SL);
    
    if (sl_sock_opt_name == 0)
    {
        // regard as success
        net_result = 0;
    }
    else
    {      
        if (sl_sock_opt_name == SL_SO_RCVTIMEO)
        {
#if 1
            _u16 opt_len;
            int opt = 1;
            net_result = sl_SetSockOpt(sockfd, SL_SOL_SOCKET, SL_SO_NONBLOCKING, &opt, 4);
            net_result = sl_GetSockOpt(sockfd, SL_SOL_SOCKET, SL_SO_NONBLOCKING, &opt, &opt_len);
            
            // set socket as non-blocking, otherwise timeout value won't take effect
            enable_option.NonblockingEnabled = 1;
            
            optlen = sizeof(SlSockNonblocking_t);
            
            
            mico_thread_msleep(100);
            
            enable_option.NonblockingEnabled = 0;
            net_result = sl_GetSockOpt(sockfd, SL_SOL_SOCKET, SL_SO_NONBLOCKING, (_u8 *)&enable_option, &opt_len);
            
#endif
          
            // the mico use int32 as socket timeout value,
            // however, simplelink use two int32 value
            time_in = *((int *)optval);
          
            time_val.tv_sec     = time_in/1000;                 // second 
            time_val.tv_usec    = (time_in*1000) % 1000000;     // u second
            
            optlen = sizeof(struct SlTimeval_t);
            net_result = sl_SetSockOpt(sockfd, level, sl_sock_opt_name, (unsigned char*)&time_val, optlen);
        }
        else
        {
            // execute the real operation:
            net_result = sl_SetSockOpt(sockfd, level, sl_sock_opt_name, optval, optlen);
        }
    }
    
    return net_result;
}

int getsockopt(int sockfd, int level, int optname, const void *optval, socklen_t *optlen)
{
    int net_result;
  
    SlSocklen_t sock_len;
    
    _i16 mico_sock_opt_name, sl_sock_opt_name;
    
    mico_sock_opt_name = optname;
    mico_sock_opt_convert(&mico_sock_opt_name, &sl_sock_opt_name, MICO_2_SL);
    
    sock_len = *optlen;
    
    net_result = sl_GetSockOpt(sockfd, level, sl_sock_opt_name, (void *)optval, &sock_len);
    
    // TODO: we may need some hard convertion for optval
    
    return net_result;
}

int bind(int sockfd, const struct sockaddr_t *addr, socklen_t addrlen)
{
    int net_result;
    
    SCK_DBG_PRINT("+b");
    
    SlSockAddrIn_t sl_sock_addr = { 0 };
    
    sl_sock_addr.sin_family             = SL_AF_INET;
    sl_sock_addr.sin_port               = sl_Htons(addr->s_port);
    sl_sock_addr.sin_addr.s_addr        = sl_Htonl(addr->s_ip);
    //sl_sock_addr.sin_zero is not used
    
    addrlen = sizeof(SlSockAddrIn_t);
    
    net_result = sl_Bind(sockfd, (SlSockAddr_t *)&sl_sock_addr, addrlen);
    
#ifdef MICO32_DEBUG
    char ip_addr[MAX_IP_ADDR_LEN] = { 0 };
    inet_ntoa(ip_addr, addr->s_ip);
    SCK_DBG_PRINT("%s:%d\r\n", ip_addr, addr->s_port);
#endif // MICO32_DEBUG
 
    SCK_DBG_PRINT("-b\r\n");
    
    return net_result;   
}

int connect(int sockfd, const struct sockaddr_t *addr, socklen_t addrlen)
{
    int net_result;
    
    SCK_DBG_PRINT("+c");
    
    SlSockAddrIn_t sl_sock_addr = { 0 };
    
    char ip_addr[MAX_IP_ADDR_LEN] = { 0 };

    
    sl_sock_addr.sin_family             = SL_AF_INET;
    sl_sock_addr.sin_port               = sl_Htons(addr->s_port);
    sl_sock_addr.sin_addr.s_addr        = sl_Htonl(addr->s_ip);
    
    addrlen = sizeof(SlSockAddrIn_t);          
    
    do
    {
        net_result = sl_Connect(sockfd, (SlSockAddr_t *)&sl_sock_addr, addrlen);
        mico_thread_msleep(10); // wait for connection stablize
    } while (net_result == SL_EALREADY);
    
    if (net_result == SL_EALREADY)
    {
        net_result = 0;
    }
        
    inet_ntoa(ip_addr, addr->s_ip);
    SCK_DBG_PRINT("%s:%d\r\n", ip_addr, addr->s_port);
    
    SCK_DBG_PRINT("-c\r\n");
    
    return net_result;
}

int listen(int sockfd, int backlog)
{
    int net_result;
    
    net_result = sl_Listen(sockfd, backlog);
    
    return net_result;
}

int accept(int sockfd, struct sockaddr_t *addr, socklen_t *addrlen)
{
    int net_result;
    
    SlSockAddrIn_t      sl_sock_addr    = { 0 };
    SlSocklen_t         sl_sock_len     = 0;
    
    SCK_DBG_PRINT("+a");
    
    sl_sock_addr.sin_family             = SL_AF_INET;
    sl_sock_addr.sin_port               = sl_Htons(addr->s_port);
    sl_sock_addr.sin_addr.s_addr        = sl_Htonl(addr->s_ip);
    
    sl_sock_len = sizeof(SlSockAddrIn_t);
    
    net_result = sl_Accept(sockfd, (SlSockAddr_t *)&sl_sock_addr, &sl_sock_len);
    
#ifdef MICO32_DEBUG
    char ip_addr[MAX_IP_ADDR_LEN] = { 0 };
    inet_ntoa(ip_addr, addr->s_ip);
    SCK_DBG_PRINT("%s:%d\r\n", ip_addr, addr->s_port);
#endif // MICO32_DEBUG
    
    SCK_DBG_PRINT("-a\r\n");
    
    return net_result;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval_t *timeout)
{
    int net_result;
    
    SlFdSet_t sl_readfds, sl_writefds, sl_exceptfds;
    uint8_t min_fds_size;
    
    SCK_DBG_PRINT("+sel\r\n");
    
    min_fds_size = (sizeof(fd_set) < sizeof(SlFdSet_t)) ? sizeof(fd_set) : sizeof(SlFdSet_t);
    
    // copy fds:
    if (readfds != NULL)
    {
        memcpy(&sl_readfds, readfds, min_fds_size);
    }
    if (writefds != NULL)
    {
        memcpy(&sl_writefds, writefds, min_fds_size);
    }
    if (exceptfds != NULL)
    {
        memcpy(&sl_exceptfds, exceptfds, min_fds_size);
    }
    
#if 0
    // TODO: warning: mico fds has different size with simplelink, caution buffer overrun
    net_result = sl_Select(nfds, &sl_readfds, &sl_writefds, &sl_exceptfds, (struct SlTimeval_t *)timeout);
#else
    net_result = 1;
#endif
    
    SCK_DBG_PRINT("-sel\r\n");
    
    return net_result;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    int net_result;
    
    uint8_t *send_buffer;
    _i16 send_len;
    
    SCK_DBG_PRINT("+sen\r\n");
    
    send_buffer = (uint8_t *)buf;
    
    while (len > 0)
    {
        if (len > MICO32_SOCK_MSS)
        {
            send_len = MICO32_SOCK_MSS;
        }
        else
        {
            send_len = len;
        }
        
        net_result = sl_Send(sockfd, send_buffer, send_len, flags);
        
        send_buffer += send_len;        // move ptr
        len -= send_len;
    }
    
    SCK_DBG_PRINT("-sen\r\n");
    
    return net_result;
}

int write(int sockfd, void *buf, size_t len)
{
    int net_result;
    
    net_result = send(sockfd, buf, len, 0);

    return net_result;
}

ssize_t sendto(int  sockfd, const void *buf, size_t len, int flags, 
              const struct sockaddr_t *dest_addr, socklen_t addrlen)
{
    int net_result;
    
    SlSockAddrIn_t      sl_sock_to_addr       = { 0 };
    SlSocklen_t         sl_sock_to_len        = 0;
    
    SCK_DBG_PRINT("+sen2\r\n");
    
    sl_sock_to_addr.sin_family        = SL_AF_INET;
    sl_sock_to_addr.sin_port          = sl_Htons(dest_addr->s_port);
    sl_sock_to_addr.sin_addr.s_addr   = sl_Htonl(dest_addr->s_ip);
    
    sl_sock_to_len = sizeof(SlSockAddrIn_t);
    
    net_result = sl_SendTo(sockfd, buf, len, flags, (SlSockAddr_t *)&sl_sock_to_addr, sl_sock_to_len);
    
    SCK_DBG_PRINT("-sen2\r\n");
    
    return net_result;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    int net_result;
    
    SCK_DBG_PRINT("+rcv\r\n");
    
    do
    {
        net_result = sl_Recv(sockfd, buf, len, flags);
    } while (net_result == SL_EAGAIN);
    
#if 1
    static uint8_t led_on_off = 0;
    char *buff = buf;
    
    if (buff[0] == 0xAA)
    {
        if (led_on_off == 0)
        {
            mico_led_set(LM_ALL_OFF);
        }
        else
        {
            mico_led_set(LM_ALL_ON);
        }
        
        led_on_off = 1 - led_on_off;
    }
#endif
    
    SCK_DBG_PRINT("+-rcv\r\n");
    
    return net_result;
}

int read(int sockfd, void *buf, size_t len)
{
    int net_result;
    
    net_result = recv(sockfd, buf, len, 0);
    
    return net_result;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, 
              struct sockaddr_t *src_addr, socklen_t *addrlen)
{
    int net_result;
    
    SlSockAddrIn_t      sl_sock_from_addr       = { 0 };
    SlSocklen_t         sl_sock_from_len        = 0;
    
    sl_sock_from_addr.sin_family        = SL_AF_INET;
    sl_sock_from_addr.sin_port          = sl_Htons(src_addr->s_port);
    sl_sock_from_addr.sin_addr.s_addr   = sl_Htonl(src_addr->s_ip);
    
    sl_sock_from_len = sizeof(SlSockAddrIn_t);
    
    net_result = sl_RecvFrom(sockfd, buf, len, flags, (SlSockAddr_t *)&sl_sock_from_addr, &sl_sock_from_len);
    
    return net_result;
}

int close(int fd)
{
    int net_result;
  
    SCK_DBG_PRINT("+clz\r\n");
    net_result = sl_Close(fd);
    SCK_DBG_PRINT("-clz\r\n");
    
    return net_result;
}

#if 1
// function name conflicted in lwip, use ours instead

uint32_t inet_addr(char *s)
{
    uint32_t x_ip;
    
    x_ip = ipaddr_addr(s);
    x_ip = sl_Htonl(x_ip);
    
    return x_ip;
}

char *inet_ntoa(char *s, uint32_t x)
{
    char *s_ip; // ip in string
    
    ip_addr_t lwip_x;   // lwip's ipaddr_ntoa use big endian??
    
    //lwip_x.addr = ENDIAN_SWAP_32(x);
    lwip_x.addr = sl_Htonl(x);
    s_ip = ipaddr_ntoa(&lwip_x);
    
    strcpy(s, s_ip);
    
    return s;
}
#endif

int gethostbyname(const char *name, uint8_t *addr, uint8_t addrLen)
{
    int net_result;
    
    _u32 ip_addr;
    
    char host_name[NETAPP_MAX_SERVICE_HOST_NAME_SIZE];
    unsigned int host_name_len;
    
    SCK_DBG_PRINT("+dns\r\n");
    
    host_name_len = strlen(name);
    if (host_name_len <= NETAPP_MAX_SERVICE_HOST_NAME_SIZE)
    {
        memcpy(host_name, name, host_name_len);
    }
    
    net_result = sl_NetAppDnsGetHostByName(host_name, host_name_len, &ip_addr, SL_AF_INET);
    
    // convert back to ip string
    // TODO: warning, length not checked.. may corrupt buffer
    inet_ntoa(addr, ip_addr);
    
    SCK_DBG_PRINT("-dns\r\n");
    
    return net_result;
}

void set_tcp_keepalive(int inMaxErrNum, int inSeconds)
{
    // simplelink doesn't support configurable keep-alive timer
    SlSockKeepalive_t enable_option;

    // if inSecond>0, we just enable keep alive
    if (inSeconds > 0)
    {
        enable_option.KeepaliveEnabled = 1;
    }
    else
    {
        enable_option.KeepaliveEnabled = 0;
    }
    
    sl_SetSockOpt(0, SL_SOL_SOCKET,SL_SO_KEEPALIVE,
        (_u8 *)&enable_option, sizeof(SlSockKeepalive_t));

    return ;
}

void get_tcp_keepalive(int *outMaxErrNum, int *outSeconds)
{
    SlSockKeepalive_t enable_option;
    
    SlSocklen_t opt_len;
    
    opt_len = sizeof(SlSockKeepalive_t);
    sl_GetSockOpt(0, SL_SOL_SOCKET,SL_SO_KEEPALIVE,
        (_u8 *)&enable_option, &opt_len);
    
    if (enable_option.KeepaliveEnabled > 0)
    {
        *outSeconds = 5;
    }
    else
    {
        *outSeconds = 0;
    }

    return ;
}
