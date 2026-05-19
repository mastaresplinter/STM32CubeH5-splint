/**
 ******************************************************************************
 * @file    LWIP/Target/eth_types.h
 * @author  Marcus D Niklasson
 * @brief   This file provides macros and types needed for the ksz8863 switch
 * driver. It is based on the rt-kernel phy for stm32
 ******************************************************************************
 */

#ifndef ETH_TYPES_H
#define ETH_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Link state bit settings */
#define PHY_LINK_DOWN 0
#define PHY_LINK_OK BIT(0)
#define PHY_LINK_10MBIT BIT(1)
#define PHY_LINK_100MBIT BIT(2)
#define PHY_LINK_1000MBIT BIT(3)
#define PHY_LINK_FULL_DUPLEX BIT(4)
#define PHY_LINK_FIBER BIT(5)

/* Capabilities advertised to link partner during auto-negotiation.
 * Bit patterns are the same as the Auto-Negotiation Advertisement Register.
 */
#define PHY_CAPABILITY_100_FD BIT(8) /* Can do 100BASE-TX full duplex */
#define PHY_CAPABILITY_100 BIT(7)    /* Can do 100BASE-TX */
#define PHY_CAPABILITY_10_FD BIT(6)  /* Can do 10BASE-T full duplex */
#define PHY_CAPABILITY_10 BIT(5)     /* Can do 10BASE-T */

/**
 * Status of Ethernet link
 */
typedef struct eth_status {
  /** Capabilities advertised to link partner during auto-negotiation
   *
   * This forms a bit pattern. Valid bit values (see driver/eth/phy/phy.h):
   * - PHY_CAPABILITY_100_FD,
   * - PHY_CAPABILITY_100,
   * - PHY_CAPABILITY_10_FD,
   * - PHY_CAPABILITY_10.
   */
  uint16_t capabilities;

  /** Is auto-negotiation process supported by physical layer? */
  bool is_autonegotiation_supported;

  /** Is auto-negotiation process enabled for physical layer? */
  bool is_autonegotiation_enabled;

  /**
   * Is link operational?
   *
   * True if link is up and network interface is administratively up.
   * False if any of those are down.
   */
  bool is_operational;

  /**
   * Link state
   *
   * Bit pattern of the following:
   * - PHY_LINK_OK is set if link is up,
   * - PHY_LINK_10MBIT, PHY_LINK_100MBIT or PHY_LINK_1000MBIT is the speed,
   * - PHY_LINK_FULL_DUPLEX is set if link is full-duplex and cleared if
   *   it is half-duplex.
   */
  uint8_t state;
} eth_status_t;

/**
 * Statistics for Ethernet port
 * (must equal the statistics struct that is used for Profinet)
 */
typedef struct port_stats {
  uint32_t if_in_octets;
  uint32_t if_out_octets;
  uint32_t if_in_discards;
  uint32_t if_out_discards;
  uint32_t if_in_errors;
  uint32_t if_out_errors;
} port_stats_t;

typedef struct phy_mac_address {
  uint8_t address[6];
} phy_mac_address_t;

#ifdef __cplusplus
}
#endif
#endif
