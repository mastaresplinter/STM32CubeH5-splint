
/**
 ******************************************************************************
 * @file    LWIP/Target/lwipopts.h
 * @author  Marcus D Niklasson
 * @brief   This file overrides LwIP stack default configuration
  *                      done in opt.h file.
 ******************************************************************************
 */

/* Define to prevent recursive inclusion --------------------------------------*/
#ifndef __LWIPOPTS__H__
#define __LWIPOPTS__H__

#include "main.h"

/*-----------------------------------------------------------------------------*/
/* Current version of LwIP supported by CubeMx: 2.1.2 -*/
/*-----------------------------------------------------------------------------*/

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */
#define PROFINET_INFO_SIZE          0
#define PBUF_FCS_SIZE               4
/* USER CODE END 0 */

#ifdef __cplusplus
 extern "C" {
#endif

/* STM32CubeMX Specific Parameters (not defined in opt.h) ---------------------*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- WITH_RTOS enabled (Since FREERTOS is set) -----*/
#define WITH_RTOS 1
/*----- CHECKSUM_BY_HARDWARE enabled -----*/
#define CHECKSUM_BY_HARDWARE 1
/*-----------------------------------------------------------------------------*/

/* LwIP Stack Parameters (modified compared to initialization value in opt.h) -*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- Value in opt.h for LWIP_DHCP: 0 -----*/
#define LWIP_DHCP 0
/*----- Default Value for LWIP_TCPIP_CORE_LOCKING: 0 ---*/
#define LWIP_TCPIP_CORE_LOCKING 1
/*----- Value in opt.h for MEM_ALIGNMENT: 1 -----*/
#define MEM_ALIGNMENT 4
/*----- Value in opt.h for MEMP_NUM_SYS_TIMEOUT: (LWIP_TCP + IP_REASSEMBLY + LWIP_ARP + (2*LWIP_DHCP) + LWIP_AUTOIP + LWIP_IGMP + LWIP_DNS + (PPP_SUPPORT*6*MEMP_NUM_PPP_PCB) + (LWIP_IPV6 ? (1 + LWIP_IPV6_REASS + LWIP_IPV6_MLD) : 0)) -*/
#define MEMP_NUM_SYS_TIMEOUT 5
/*----- Default Value for PBUF_POOL_BUFSIZE: 592 ---*/
#define PBUF_POOL_BUFSIZE LWIP_MEM_ALIGN_SIZE(1518 + PBUF_FCS_SIZE  + PROFINET_INFO_SIZE)
/*----- Value in opt.h for LWIP_ETHERNET: LWIP_ARP || PPPOE_SUPPORT -*/
#define LWIP_ETHERNET 1
/*----- Value in opt.h for LWIP_DNS_SECURE: (LWIP_DNS_SECURE_RAND_XID | LWIP_DNS_SECURE_NO_MULTIPLE_OUTSTANDING | LWIP_DNS_SECURE_RAND_SRC_PORT) -*/
#define LWIP_DNS_SECURE 7
/*----- Value in opt.h for TCP_SND_QUEUELEN: (4*TCP_SND_BUF + (TCP_MSS - 1))/TCP_MSS -----*/
#define TCP_SND_QUEUELEN 9
/*----- Value in opt.h for TCP_SNDLOWAT: LWIP_MIN(LWIP_MAX(((TCP_SND_BUF)/2), (2 * TCP_MSS) + 1), (TCP_SND_BUF) - 1) -*/
#define TCP_SNDLOWAT 1071
/*----- Value in opt.h for TCP_SNDQUEUELOWAT: LWIP_MAX(TCP_SND_QUEUELEN)/2, 5) -*/
#define TCP_SNDQUEUELOWAT 5
/*----- Value in opt.h for TCP_WND_UPDATE_THRESHOLD: LWIP_MIN(TCP_WND/4, TCP_MSS*4) -----*/
#define TCP_WND_UPDATE_THRESHOLD 536
/*----- Default Value for LWIP_NETIF_HOSTNAME: 0 ---*/
#define LWIP_NETIF_HOSTNAME 1
/*----- Value in opt.h for LWIP_NETIF_LINK_CALLBACK: 0 -----*/
#define LWIP_NETIF_LINK_CALLBACK 1
/*----- Default Value for TCPIP_THREAD_NAME: "tcpip_thread" ---*/
#define TCPIP_THREAD_NAME "tcpip_thread11"
/*----- Value in opt.h for TCPIP_THREAD_STACKSIZE: 0 -----*/
#define TCPIP_THREAD_STACKSIZE 1768
/*----- Value in opt.h for TCPIP_THREAD_PRIO: 1 -----*/
#define TCPIP_THREAD_PRIO osPriorityNormal
/*----- Value in opt.h for TCPIP_MBOX_SIZE: 0 -----*/
#define TCPIP_MBOX_SIZE 128
/*----- Value in opt.h for SLIPIF_THREAD_STACKSIZE: 0 -----*/
#define SLIPIF_THREAD_STACKSIZE 1024
/*----- Value in opt.h for SLIPIF_THREAD_PRIO: 1 -----*/
#define SLIPIF_THREAD_PRIO 3
/*----- Value in opt.h for DEFAULT_THREAD_STACKSIZE: 0 -----*/
#define DEFAULT_THREAD_STACKSIZE 1024
/*----- Value in opt.h for DEFAULT_THREAD_PRIO: 1 -----*/
#define DEFAULT_THREAD_PRIO 3
/*----- Value in opt.h for DEFAULT_UDP_RECVMBOX_SIZE: 0 -----*/
#define DEFAULT_UDP_RECVMBOX_SIZE 128
/*----- Value in opt.h for DEFAULT_TCP_RECVMBOX_SIZE: 0 -----*/
#define DEFAULT_TCP_RECVMBOX_SIZE 128
/*----- Value in opt.h for DEFAULT_ACCEPTMBOX_SIZE: 0 -----*/
#define DEFAULT_ACCEPTMBOX_SIZE 5
/*----- Value in opt.h for LWIP_COMPAT_SOCKETS: 1 -----*/
#define LWIP_COMPAT_SOCKETS 2
/*----- Value in opt.h for RECV_BUFSIZE_DEFAULT: INT_MAX -----*/
#define RECV_BUFSIZE_DEFAULT 2000000000
/*----- Default Value for SO_REUSE: 0 ---*/
#define SO_REUSE 1
/*----- Default Value for LWIP_SNMP: 0 ---*/
#define LWIP_SNMP 1
/*----- Default Value for SNMP_USE_NETCONN: 0 ---*/
#define SNMP_USE_NETCONN 1
/*----- Default Value for SNMP_USE_RAW: 1 ---*/
#define SNMP_USE_RAW 0
/*----- Value in opt.h for MIB2_STATS: 0 or SNMP_LWIP_MIB2 -----*/
#define MIB2_STATS 1
/*----- Value in opt.h for CHECKSUM_GEN_IP: 1 -----*/
#define CHECKSUM_GEN_IP 0
/*----- Value in opt.h for CHECKSUM_GEN_UDP: 1 -----*/
#define CHECKSUM_GEN_UDP 0
/*----- Value in opt.h for CHECKSUM_GEN_TCP: 1 -----*/
#define CHECKSUM_GEN_TCP 0
/*----- Value in opt.h for CHECKSUM_GEN_ICMP: 1 -----*/
#define CHECKSUM_GEN_ICMP 0
/*----- Value in opt.h for CHECKSUM_GEN_ICMP6: 1 -----*/
#define CHECKSUM_GEN_ICMP6 0
/*----- Value in opt.h for CHECKSUM_CHECK_IP: 1 -----*/
#define CHECKSUM_CHECK_IP 0
/*----- Value in opt.h for CHECKSUM_CHECK_UDP: 1 -----*/
#define CHECKSUM_CHECK_UDP 0
/*----- Value in opt.h for CHECKSUM_CHECK_TCP: 1 -----*/
#define CHECKSUM_CHECK_TCP 0
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP: 1 -----*/
#define CHECKSUM_CHECK_ICMP 0
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP6: 1 -----*/
#define CHECKSUM_CHECK_ICMP6 0
/*-----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1
#define LWIP_NETIF_API                1
#define ETHARP_SUPPORT_VLAN           1

#define LWIP_HOOK_FILENAME "lwip_hooks.h"
#define LWIP_HOOK_UNKNOWN_ETH_PROTOCOL lwip_hook_unknown_eth_protocol

/**
 * LWIP_DHCP_AUTOIP_COOP_TRIES: Set to the number of DHCP DISCOVER probes
 * that should be sent before falling back on AUTOIP. This can be set
 * as low as 1 to get an AutoIP address very quickly, but you should
 * be prepared to handle a changing IP address when DHCP overrides
 * AutoIP.
 */
#define LWIP_DHCP_AUTOIP_COOP_TRIES 2

/* Supported services. Should be 78 for Profinet. See IETF RFC 3418. */
#define SNMP_SYSSERVICES ((1 << 6) | (1 << 3) | (1 << 2) | (1 << 1))

/* Various memory size options */
#define MEMP_MEM_INIT               1
#define MEM_SIZE                    (16 * 1024)
#define MEMP_NUM_UDP_PCB            6
#define PBUF_POOL_SIZE              (8 + 3)
#define TCP_MSS                     1460 /* Should be 1460 for Ethernet IPv4 */
#define TCP_SND_BUF                 (2 * TCP_MSS)
#define SNMP_MAX_OBJ_ID_LEN         32

/* Stack sizes */
#define DEFAULT_THREAD_STACKSIZE    1024
#define TCPIP_THREAD_STACKSIZE      1768
#define SNMP_STACK_SIZE             4000

/* SNMP server thread priority */
#define SNMP_THREAD_PRIO            osPriorityBelowNormal

/* Mailbox sizes */
#define DEFAULT_RAW_RECVMBOX_SIZE   5

/* TCP options */
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_NETCONN            16

#define LWIP_PBUF_CUSTOM_DATA       \
      struct timespec timestamp;    \
      void * gptp_pkt;              \
      u8_t port_num;
#define LWIP_PBUF_CUSTOM_DATA_INIT  \
      p->timestamp.tv_sec  = 0;     \
      p->timestamp.tv_nsec = 0;     \
      p->gptp_pkt = NULL;           \
      p->port_num = 0;

/* Global debugging options
 *
 * LWIP_DEBUG - #define this to enable debug messages (LWIP_DEBUGF()).
 *
 * LWIP_DBG_MIN_LEVEL - enable debug messages based on severity of event:
 *    LWIP_DBG_LEVEL_ALL      - all messages
 *    LWIP_DBG_LEVEL_WARNING  - warning, serious and severe messages
 *    LWIP_DBG_LEVEL_SERIOUS  - serious and severe messages
 *    LWIP_DBG_LEVEL_SEVERE   - severe messages
 *
 * LWIP_DBG_TYPES_ON - enable debug messages based on type of event:
 *    LWIP_DBG_ON             - all messages
 *    LWIP_DBG_TRACE          - tracing messages (to follow program flow)
 *    LWIP_DBG_STATE          - state debug messages (to follow module states)
 */
//#define LWIP_DEBUG 1
#define LWIP_DBG_MIN_LEVEL          LWIP_DBG_LEVEL_ALL
#define LWIP_DBG_TYPES_ON           LWIP_DBG_ON

/* Module specific debugging options
 *
 * Set the module-specific debug macro (e.g. DHCP_DEBUG) to one of these:
 *    LWIP_DBG_OFF
 *    LWIP_DBG_ON
 */
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define IGMP_DEBUG                  LWIP_DBG_OFF
#define TCPIP_DEBUG                 LWIP_DBG_OFF
#define AUTOIP_DEBUG                LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define TCP_RTO_DEBUG               LWIP_DBG_OFF
#define TCP_CWND_DEBUG              LWIP_DBG_OFF
#define TCP_WND_DEBUG               LWIP_DBG_OFF
#define TCP_FR_DEBUG                LWIP_DBG_OFF
#define TCP_QLEN_DEBUG              LWIP_DBG_OFF
#define TCP_RST_DEBUG               LWIP_DBG_OFF
#define SNMP_DEBUG                  LWIP_DBG_OFF
#define SNMP_MIB_DEBUG              LWIP_DBG_OFF

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /*__LWIPOPTS__H__ */

/************************* (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
