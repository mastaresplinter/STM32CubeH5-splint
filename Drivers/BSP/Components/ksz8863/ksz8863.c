/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * www.rt-labs.com
 * Copyright 2025 rt-labs AB, Sweden.
 *
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 ********************************************************************/

#include "ksz8863.h"
#include "osal.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define MAX_FRAME_SIZE_EXCLUDING_FCS      1518 /* Including 4 bytes VLAN field */
#define MIN_FRAME_SIZE_EXCLUDING_FCS      60

#define LLDP_MAC_ADDRESS                  {{0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e}}

/* Register addresses */
#define REG_CHIP_ID0                      (0x00)
#define REG_CHIP_ID1                      (0x01)
#define REG_GLOBAL_CONTROL1               (0x03)
#define REG_PORT_PORT_CONTROL12(port)     (0x0C + 0x10 * (port))
#define REG_PORT_PORT_CONTROL13(port)     (0x0D + 0x10 * (port))
#define REG_PORT_STATUS0(port)            (0x0E + 0x10 * (port))
#define REG_PORT_STATUS1(port)            (0x0F + 0x10 * (port))
#define REG_USER_DEFINED_REGISTER1        (0x76)
#define REG_INDIRECT_ACCESS_CONTROL0      (0x79)
#define REG_INDIRECT_ACCESS_CONTROL1      (0x7A)
#define REG_INDIRECT_DATA(i)              (0x83 - (i))

#define MIB_COUNTER_RX_LO_PRIORITY_BYTE(port) (0 + (0x20 * (port - 1)))
#define MIB_COUNTER_TX_LO_PRIORITY_BYTE(port) (0x14 + (0x20 * (port - 1)))

/* Global Control 1 register bits */
#define GLOBAL_CONTROL1_TAIL_TAG          BIT (6)
#define GLOBAL_CONTROL1_TX_FLOW_CONTROL   BIT (5)
#define GLOBAL_CONTROL1_RX_FLOW_CONTROL   BIT (4)
#define GLOBAL_CONTROL1_AGING_ENABLE      BIT (2)

/* Port Control 12 register bits */
#define PORT_CONTROL12_AN_ENABLE              BIT (7)
#define PORT_CONTROL12_FORCE_100              BIT (6)
#define PORT_CONTROL12_FORCE_FULL_DUPLEX      BIT (5)
#define PORT_CONTROL12_ADV_FLOW_CTRL          BIT (4)
#define PORT_CONTROL12_ADV_100BT_FULL_DUPLEX  BIT (3)
#define PORT_CONTROL12_ADV_100BT_HALF_DUPLEX  BIT (2)
#define PORT_CONTROL12_ADV_10BT_FULL_DUPLEX   BIT (1)
#define PORT_CONTROL12_ADV_10BT_HALF_DUPLEX   BIT (0)

/* Port x Control 13 register bits */
#define PORT_CONTROL13_RESTART_AN         BIT (5)
#define PORT_CONTROL13_LOOPBACK           BIT (0)

/* Port x Status 0 register bits */
#define PORT_STATUS0_AN_DONE              BIT (6)
#define PORT_STATUS0_LINK_GOOD            BIT (5)

/* Port x Status 1 register bits */
#define PORT_STATUS1_SPEED_100            BIT (2)
#define PORT_STATUS1_FULL_DUPLEX          BIT (1)

/* Indirect Access Control 0 register bits */
#define INDIRECT_ACCESS_CONTROL0_READ     BIT (4)
#define INDIRECT_ACCESS_CONTROL0_TABLE_SELECT(t) ((t) << 2)
#define INDIRECT_ACCESS_CONTROL0_HIGH_ADDRESS(a) (((a) >> 8) & 0x3)
#define READ_MIB_COUNTER  (INDIRECT_ACCESS_CONTROL0_READ | BIT(3) | BIT(2))
#define MIB_COUNTER_VALID                 BIT (30)
#define MIB_COUNTER_VALUE_MASK            0x3FFFFFFF

/* Indirect Access Control 1 register bits */
#define INDIRECT_ACCESS_CONTROL1_LOW_ADDRESS(a) ((a) & 0xff)

/* Static MAC table bits */
#define STATIC_MAC_TABLE_ENTRY_VALID      (1ULL << 51)
#define STATIC_MAC_TABLE_FWD_TO_PORT_3    (4ULL << 48)

/* MIIM registers */
#define REGAD_AUTO_NEG_ADVERTISEMENT      (0x04)

/* Note: The port index is one less than the port number */
#define EXT_PORT_1                        1
#define EXT_PORT_2                        2
#define CPU_PORT                          3
#define PORT_INDEX(port)                  ((port) - 1)
#define CPU_PORT_INDEX                    PORT_INDEX (CPU_PORT)

#define CHIP_ID_FAMILY_ID                 (0x88)

#define PHY_WRITE_SPI(reg, value) pObj->IO.WriteReg (reg, value)

#define PHY_READ_SPI(reg) pObj->IO.ReadReg (reg)

#undef KSZ_DEBUG

#ifdef KSZ_DEBUG
#define DPRINT(...) printf ("ksz8863: "__VA_ARGS__)
#else
#define DPRINT(...)
#endif  /* KSZ_DEBUG */

ksz8863_Object_t * gpObj;

typedef enum ksz8863_table
{
   KSZ8863_TABLE_STATIC_MAC_ADDRESSES = 0,
   KSZ8863_TABLE_VlAN_INFO = 1,
   KSZ8863_TABLE_DYNAMIC_MAC_ADDRESSES = 2,
   KSZ8863_TABLE_MIB_COUNTERS = 3,
} ksz8863_table_t;

int32_t KSZ8863_RegisterBusIO (ksz8863_Object_t * pObj, ksz8863_IOCtx_t * ioctx)
{
   if (!pObj || !ioctx->ReadReg || !ioctx->WriteReg || !ioctx->GetTick)
   {
      return KSZ8863_STATUS_ERROR;
   }

   pObj->IO.Init = ioctx->Init;
   pObj->IO.DeInit = ioctx->DeInit;
   pObj->IO.ReadReg = ioctx->ReadReg;
   pObj->IO.WriteReg = ioctx->WriteReg;
   pObj->IO.GetTick = ioctx->GetTick;

   return KSZ8863_STATUS_OK;
}

static void ksz8863_read_mib_counter
   (ksz8863_Object_t * pObj, const int offset, uint8_t *counter_value)
{
   // Select MIB counter to read from
   PHY_WRITE_SPI (REG_INDIRECT_ACCESS_CONTROL0,
         READ_MIB_COUNTER | INDIRECT_ACCESS_CONTROL0_HIGH_ADDRESS(offset));
   // Trigger read operation
   PHY_WRITE_SPI (REG_INDIRECT_ACCESS_CONTROL1,
         INDIRECT_ACCESS_CONTROL1_LOW_ADDRESS(offset));

   size_t i;
   for (i = 0; i < 4; i++)
   {
      counter_value[i] = PHY_READ_SPI (REG_INDIRECT_DATA(i));
   }
}

static const char * ksz8863_state_description (uint8_t state)
{
   switch (state)
   {
   case PHY_LINK_DOWN:
      return "DOWN";
   case PHY_LINK_OK:
      return "UP, 10Mbps, half duplex";
   case PHY_LINK_OK | PHY_LINK_100MBIT:
      return "UP, 100Mbps, half duplex";
   case PHY_LINK_OK | PHY_LINK_FULL_DUPLEX:
      return "UP, 10Mbps, full duplex";
   case PHY_LINK_OK | PHY_LINK_100MBIT | PHY_LINK_FULL_DUPLEX:
      return "UP, 100Mbps, full duplex";
   default:
      return "invalid state";
   }
}


static phy_mac_address_t byteswapped_mac_address (const phy_mac_address_t input)
{
   phy_mac_address_t output = {{0}};

   for (size_t i = 0; i < NELEMENTS (input.address); i++)
   {
      output.address[i] = input.address[NELEMENTS (input.address) - 1 - i];
   }

   return output;
}

static void ksz8863_write_data_to_index_in_table (
   ksz8863_Object_t * pObj,
   const ksz8863_table_t table,
   const int index,
   const void * untyped_data,
   const size_t size)
{
   const uint8_t * data = untyped_data;

   assert (size <= 9);

   /* Configure data to write to entry in table */
   for (size_t i = 0; i < size; i++)
   {
      PHY_WRITE_SPI (REG_INDIRECT_DATA (i), data[i]);
   }

   /* Select entry in the static MAC address table to write to */
   PHY_WRITE_SPI (
      REG_INDIRECT_ACCESS_CONTROL0,
      (INDIRECT_ACCESS_CONTROL0_TABLE_SELECT (table) |
       INDIRECT_ACCESS_CONTROL0_HIGH_ADDRESS (index)));

   /* Trigger write operation */
   PHY_WRITE_SPI (
      REG_INDIRECT_ACCESS_CONTROL1,
      INDIRECT_ACCESS_CONTROL1_LOW_ADDRESS (index));
}

static void ksz8863_write_mac_address_to_index_in_static_table (
   ksz8863_Object_t * pObj,
   const int index,
   phy_mac_address_t mac_address)
{
   uint64_t data = 0;

   mac_address = byteswapped_mac_address (mac_address);
   memcpy (&data, &mac_address, sizeof (mac_address));
   data |= STATIC_MAC_TABLE_ENTRY_VALID | STATIC_MAC_TABLE_FWD_TO_PORT_3;

   ksz8863_write_data_to_index_in_table (
      pObj,
      KSZ8863_TABLE_STATIC_MAC_ADDRESSES,
      index,
      &data,
      sizeof (data));
}

static void ksz8863_log_link_state (int port, uint8_t state)
{
   switch (state)
   {
   case PHY_LINK_DOWN:
      /* Fall-through */
   case PHY_LINK_OK | PHY_LINK_100MBIT | PHY_LINK_FULL_DUPLEX:
      DPRINT ("Link state on port %d changed to %s\n",
            port, ksz8863_state_description (state));
      break;
   default:
      /* Bad speed or duplex mode */
      printf ("Link state on port %d erroneously changed to %s\n",
            port, ksz8863_state_description (state));
      break;
   }
}

static bool ksz8863_autonegotiation_is_configured_on_port (
      ksz8863_Object_t * pObj, const int port)
{
   assert (port == EXT_PORT_1 || port == EXT_PORT_2);

   const uint8_t control12_expected =
         (pObj->use_auto_negotiation[PORT_INDEX (port)]) ?
               PORT_CONTROL12_AN_ENABLE :
               (PORT_CONTROL12_FORCE_100 | PORT_CONTROL12_FORCE_FULL_DUPLEX) |
         PORT_CONTROL12_ADV_FLOW_CTRL |
         PORT_CONTROL12_ADV_100BT_FULL_DUPLEX;

   uint8_t control12 =
         PHY_READ_SPI (REG_PORT_PORT_CONTROL12 (port));

   return (control12 == control12_expected);
}

int32_t KSZ8863_DeInit (ksz8863_Object_t * pObj)
{
   if (pObj->Is_Initialized)
   {
      if (pObj->IO.DeInit != 0)
      {
         pObj->IO.DeInit();
      }
      pObj->Is_Initialized = 0;
   }
   return KSZ8863_STATUS_OK;
}

uint8_t ksz8863_get_port_state (ksz8863_Object_t * pObj, const int port)
{
   uint8_t last_state = pObj->port[PORT_INDEX (port)].last_state;

   if (last_state == 0)
   {
      /* This will update state for all ports */
      last_state = ksz8863_get_link_state (pObj);

      if (port == EXT_PORT_1 || port == EXT_PORT_2)
      {
         return pObj->port[PORT_INDEX (port)].last_state;
      }
   }

   return last_state;
}

int ksz8863_get_port_statistics (ksz8863_Object_t * pObj,
                                        port_stats_t * port_stats,
                                        uint8_t port)
{
   static port_stats_t acc_stats[KSZ8863_NUM_EXT_PORTS];
   int result = -1;
   uint32_t counter_value;

   assert (port == EXT_PORT_1 || port == EXT_PORT_2);

   memset (port_stats, 0, sizeof (port_stats_t));

   ksz8863_read_mib_counter (pObj,
                             MIB_COUNTER_RX_LO_PRIORITY_BYTE (port),
                             (uint8_t *)&counter_value);

   if ((counter_value & MIB_COUNTER_VALID) == MIB_COUNTER_VALID)
   {
      counter_value &= MIB_COUNTER_VALUE_MASK;
      acc_stats[PORT_INDEX(port)].if_in_octets += counter_value;

      ksz8863_read_mib_counter (pObj,
                                MIB_COUNTER_TX_LO_PRIORITY_BYTE (port),
                                (uint8_t *)&counter_value);

      if ((counter_value & MIB_COUNTER_VALID) == MIB_COUNTER_VALID)
      {
         counter_value &= MIB_COUNTER_VALUE_MASK;
         acc_stats[PORT_INDEX(port)].if_out_octets += counter_value;

         port_stats->if_in_octets = acc_stats[PORT_INDEX(port)].if_in_octets;
         port_stats->if_out_octets = acc_stats[PORT_INDEX(port)].if_out_octets;

         result = 0;
      }
   }

   return result;
}

/* Determine link state for the switch's internal port (port 3).
 *
 * The processor is directly connected over RMII to a MAC on switch port 3.
 * Being directly connected, the link to this port is always up and never down.
 * Reporting the link as always up is not very convenient, as it says nothing
 * about whether network communication over one of the two external ports
 * (port 1 or 2) is actually possible as both could both be down.
 * Instead, we re-interpret link state of "up" to mean that network
 * communication is possible over an external switch port.
 *
 * As a hack we also report link as down if link state for an external port
 * has changed. This is to force DHCP re-negotiation.
 */
uint8_t ksz8863_get_link_state (ksz8863_Object_t * pObj)
{
   uint8_t state_port[KSZ8863_NUM_PORTS];
   bool link_changed_on_external_port = false;
   bool link_up_on_external_port = false;

   for (int i = 0; i < KSZ8863_NUM_PORTS; i++)
   {
      state_port[i] = ksz8863_link_state_for_port (pObj, i + 1);
   }

   /* Determine what link state to report for internal port */
   for (int i = 0; i < KSZ8863_NUM_EXT_PORTS; i++)
   {
      if (state_port[i] & PHY_LINK_OK)
      {
         link_up_on_external_port = true;
      }

      if (state_port[i] != pObj->port[i].last_state)
      {
         link_changed_on_external_port = true;
      }
   }

   if (!link_up_on_external_port)
   {
      /* Report link as down as network communication is not possible */
      state_port[CPU_PORT_INDEX] = PHY_LINK_DOWN;
   }
   else if (link_changed_on_external_port)
   {
      /* Report link as down as to force DHCP re-negotiation */
      state_port[CPU_PORT_INDEX] = PHY_LINK_DOWN;
   }

   /* Log state changes */
   for (int i = 0; i < KSZ8863_NUM_PORTS; i++)
   {
      if (state_port[i] != pObj->port[i].last_state)
      {
         ksz8863_log_link_state (i + 1, state_port[i]);
      }
      /* Save status for next call */
      pObj->port[i].last_state = state_port[i];
   }

   return state_port[CPU_PORT_INDEX];
}

uint8_t ksz8863_link_state_for_port (ksz8863_Object_t * pObj, const int port)
{
   uint8_t state;

   assert (port == EXT_PORT_1 || port == EXT_PORT_2 || port == CPU_PORT);

   /* Check if link is up */
   if (port == CPU_PORT)
   {
      /* Link for the internal port is always up */
      state = PHY_LINK_OK;
   }
   else
   {
      uint8_t status0 = PHY_READ_SPI (REG_PORT_STATUS0 (port));
      bool good_link = status0 & PORT_STATUS0_LINK_GOOD;
      bool link_up;

      if (pObj->use_auto_negotiation[PORT_INDEX (port)])
      {
         bool autonegotiation_completed = status0 & PORT_STATUS0_AN_DONE;
         link_up = autonegotiation_completed && good_link;
      }
      else
      {
         link_up = good_link;
      }

      state = link_up ? PHY_LINK_OK : PHY_LINK_DOWN;
   }

   /* Check speed and duplex mode */
   if (state & PHY_LINK_OK)
   {
      uint8_t status1 = PHY_READ_SPI (REG_PORT_STATUS1 (port));

      if (status1 & PORT_STATUS1_SPEED_100)
      {
         state |= PHY_LINK_100MBIT;
      }

      if (status1 & PORT_STATUS1_FULL_DUPLEX)
      {
         state |= PHY_LINK_FULL_DUPLEX;
      }
   }

   return state;
}

static void ksz8863_restart_autonegotiation_on_port (
      ksz8863_Object_t * pObj, int port)
{
   assert (port == EXT_PORT_1 || port == EXT_PORT_2);

   if (pObj->use_auto_negotiation[PORT_INDEX (port)])
   {
      /* Enable auto-negotiation with 100Mbit/full-duplex capability advertised.
       * Fall back on 10Mbit/half-duplex if auto-negotiation fails.
       */
      PHY_WRITE_SPI (
            REG_PORT_PORT_CONTROL12 (port),
            (PORT_CONTROL12_AN_ENABLE |
             PORT_CONTROL12_ADV_FLOW_CTRL |
             PORT_CONTROL12_ADV_100BT_FULL_DUPLEX));

      /* Restart auto-negotiation as to force settings to take effect */
      PHY_WRITE_SPI (
            REG_PORT_PORT_CONTROL13 (port),
            PORT_CONTROL13_RESTART_AN);
   }
   else
   {
      /* Force 100 Mbps speed and full duplex */
      PHY_WRITE_SPI (
            REG_PORT_PORT_CONTROL12 (port),
            (PORT_CONTROL12_FORCE_100 |
             PORT_CONTROL12_FORCE_FULL_DUPLEX |
             PORT_CONTROL12_ADV_FLOW_CTRL |
             PORT_CONTROL12_ADV_100BT_FULL_DUPLEX));
   }
}

uint8_t ksz8863_get_incoming_port (uint8_t * frame, size_t * const size)
{
   unsigned port_index;

   assert (frame != NULL);
   assert (size != NULL);
   assert (*size > 0);
   assert (*size <= MAX_FRAME_SIZE_EXCLUDING_FCS + 1);

   /* Remove tail-tag byte and determine which port the frame was received on */
   (*size)--;
   port_index = frame[*size] & BIT (0);

   return (port_index + 1);
}

void ksz8863_start (ksz8863_Object_t * pObj)
{
   for (int port = EXT_PORT_1; port <= EXT_PORT_2; port++)
   {
      if (ksz8863_autonegotiation_is_configured_on_port (pObj, port))
      {
         /* Port is already configured for auto-negotiation, so no need to
          * configure it again.
          */
      }
      else
      {
         ksz8863_restart_autonegotiation_on_port (pObj, port);
      }
   }
}

static void ksz8863_reset (ksz8863_Object_t * pObj)
{
   /* Test SMI communication by writing User Defined Register 1 */
   PHY_WRITE_SPI (REG_USER_DEFINED_REGISTER1, 0xAA);
   assert (PHY_READ_SPI (REG_USER_DEFINED_REGISTER1) == 0xAA);

#ifdef DUMP_REGS
   uint16_t v1, v2, v3;

   printf ("\n");

   v1 = PHY_READ_SPI (0); /* 88h */
   v2 = PHY_READ_SPI (1); /* 31h */
   printf ("%02x %02x\n", v1 & 0xFF, v2 & 0xFF);

   printf ("\n");

   for (int i = 0; i < 0x10; i++)
   {
      v1 = PHY_READ_SPI (i);
      printf ("%02x: %02x\n", i, v1 & 0xFF);
   }

   for (int i = 0x10; i < 0x20; i++)
   {
      v1 = PHY_READ_SPI (i);
      v2 = PHY_READ_SPI (i + 0x10);
      v3 = PHY_READ_SPI (i + 0x20);
      printf ("%02x: %02x %02x %02x\n", i, v1 & 0xFF, v2 & 0xFF, v3 & 0xFF);
   }

   for (int i = 0x43; i < 0x44; i++)
   {
      v1 = PHY_READ_SPI (i);
      printf ("%02x: %02x\n", i, v1 & 0xFF);
   }

   for (int i = 96; i < 199; i++)
   {
      v1 = PHY_READ_SPI (i);
      printf ("%02x: %02x\n", i, v1 & 0xFF);
   }
#endif

   for (int i = 0; i < KSZ8863_NUM_PORTS; i++)
   {
      pObj->port[i].last_state = PHY_LINK_DOWN;
   }

   /* Skip hardware reset as we don't want to break communication
    * between port 1 and 2 unless strictly needed.
    */
}


uint16_t ksz8863_get_port_capabilities (ksz8863_Object_t * pObj,
uint8_t port)
{
   assert (port == EXT_PORT_1 || port == EXT_PORT_2);

   uint16_t value = 0;
   uint8_t regval;

   regval = PHY_READ_SPI (REG_PORT_PORT_CONTROL12(port));

   if (regval & PORT_CONTROL12_ADV_100BT_FULL_DUPLEX)
   {
      value |= PHY_CAPABILITY_100_FD;
   }
   if (regval & PORT_CONTROL12_ADV_100BT_HALF_DUPLEX)
   {
      value |= PHY_CAPABILITY_100;
   }
   if (regval & PORT_CONTROL12_ADV_10BT_FULL_DUPLEX)
   {
      value |= PHY_CAPABILITY_10_FD;
   }
   if (regval & PORT_CONTROL12_ADV_10BT_HALF_DUPLEX)
   {
      value |= PHY_CAPABILITY_10;
   }

   return value;
}

uint16_t ksz8863_get_link_capabilities (ksz8863_Object_t * pObj)
{
   /* Capabilities should be the same for both ports, so return one of them */
  return ksz8863_get_port_capabilities (pObj, EXT_PORT_1);
}

phy_mac_address_t ksz8863_get_port_mac_address (
   ksz8863_Object_t * pObj,
   uint8_t port)
{
   /* The CPU port may also be addressed as port 0 */
   if (port == 0)
   {
      port = CPU_PORT;
   }

   assert (port == EXT_PORT_1 || port == EXT_PORT_2 || port == CPU_PORT);

   return pObj->port[PORT_INDEX (port)].mac_address;
}

int32_t KSZ8863_Init (const ksz8863_cfg_t * ksz8863_cfg, ksz8863_Object_t * pObj)
{
   assert (ksz8863_cfg->address == 3);

   uint8_t regvalue = 0;

   if (pObj->Is_Initialized == 0)
   {
      if (pObj->IO.Init != 0)
      {
         pObj->IO.Init();
      }

      for (int i = 0; i < KSZ8863_NUM_PORTS; i++)
      {
         pObj->port[i].last_state = PHY_LINK_DOWN;
         memcpy (
            &pObj->port[i].mac_address,
            &ksz8863_cfg->mac_address_port[i],
            sizeof (phy_mac_address_t));

         if (i < KSZ8863_NUM_EXT_PORTS)
         {
            pObj->use_auto_negotiation[i] = ksz8863_cfg->use_AN[i];
         }
      }

      if (pObj->IO.ReadReg != NULL)
      {

         regvalue = PHY_READ_SPI (REG_CHIP_ID0);

         if (regvalue != CHIP_ID_FAMILY_ID)
         {
            return -1;
         }

         regvalue = GLOBAL_CONTROL1_TAIL_TAG | GLOBAL_CONTROL1_TX_FLOW_CONTROL |
                    GLOBAL_CONTROL1_RX_FLOW_CONTROL | GLOBAL_CONTROL1_AGING_ENABLE;

         PHY_WRITE_SPI (REG_GLOBAL_CONTROL1, regvalue);

         for (int i = 0; i < KSZ8863_NUM_EXT_PORTS; i++)
         {
            if (pObj->use_auto_negotiation[i])
            {
               regvalue = PHY_READ_SPI (REG_PORT_PORT_CONTROL12 (i + 1));
               regvalue |= PORT_CONTROL12_AN_ENABLE |
                           PORT_CONTROL12_ADV_FLOW_CTRL;
               PHY_WRITE_SPI (REG_PORT_PORT_CONTROL12 (i + 1), regvalue);
            }
            else
            {
               regvalue = PHY_READ_SPI (REG_PORT_PORT_CONTROL12 (i + 1));
               regvalue |= PORT_CONTROL12_FORCE_100 | PORT_CONTROL12_FORCE_FULL_DUPLEX;
               PHY_WRITE_SPI (REG_PORT_PORT_CONTROL12 (i + 1), regvalue);
            }
         }
      }
      pObj->Is_Initialized = 1;
      gpObj = pObj;
   }

   /* Don't forward incoming LLDP packets between external ports */
   ksz8863_write_mac_address_to_index_in_static_table (
      pObj,
      0,
      (phy_mac_address_t)LLDP_MAC_ADDRESS);

   return KSZ8863_STATUS_OK;
}

ksz8863_Object_t * ksz8863_get_default_driver (void)
{
   return gpObj;
}
