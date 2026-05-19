/**
 ******************************************************************************
 * @file    LWIP/Target/ethernetif.h
 * @author  Marcus D Niklasson
 * @brief   This file implements Ethernet network interface drivers for lwIP
 ******************************************************************************
 */

#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__


#include "lwip/err.h"
#include "lwip/netif.h"
#include "cmsis_os.h"
#include "eth_types.h"
#include "osal.h"

struct link_str {
  struct netif *netif;
  osSemaphoreId_t semaphore;
};

/* Exported types ------------------------------------------------------------*/

err_t ethernetif_init(struct netif *netif);

void ethernetif_input(void* argument);
void ethernetif_set_link(void* argument);
void ethernetif_update_config(struct netif *netif);
void ethernetif_notify_conn_changed(struct netif *netif);
void ethernet_link_check_state(struct netif *netif);

phy_mac_address_t ethernetif_get_port_mac_address (
   uint8_t port);

void ethernetif_get_status (
   struct netif * netif,
   eth_status_t * link,
   uint8_t port);

int ethernetif_get_port_statistics (void * arg,
    port_stats_t * port_stats,
    uint8_t port);

void KSZ8863_EnableLoopback_Port1(void);
void KSZ8863_EnableLoopback_Port2(void);

#endif
