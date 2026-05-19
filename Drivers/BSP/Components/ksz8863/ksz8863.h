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

#ifndef KSZ8863_H
#define KSZ8863_H

#ifdef __cplusplus
extern "C" {
#endif

#include "eth_types.h"
#include "osal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KSZ8863_STATUS_ERROR               ((int32_t)-1)
#define KSZ8863_STATUS_OK                  ((int32_t)0)

#define KSZ8863_NUM_EXT_PORTS 2
#define KSZ8863_NUM_PORTS     (KSZ8863_NUM_EXT_PORTS + 1)

typedef uint8_t (*ksz8863_Init_Func) (void);
typedef uint8_t (*ksz8863_DeInit_Func) (void);
typedef uint8_t (*ksz8863_ReadReg_Func) (uint8_t);
typedef uint8_t (*ksz8863_WriteReg_Func) (uint8_t, uint8_t);
typedef uint32_t (*ksz8863_GetTick_Func) (void);

typedef struct
{
   ksz8863_Init_Func Init;
   ksz8863_DeInit_Func DeInit;
   ksz8863_WriteReg_Func WriteReg;
   ksz8863_ReadReg_Func ReadReg;
   ksz8863_GetTick_Func GetTick;
} ksz8863_IOCtx_t;

typedef struct
{
   uint8_t last_state;
   phy_mac_address_t mac_address;
} ksz8863_port_t;

typedef struct
{
   uint32_t DevAddr;
   uint32_t Is_Initialized;
   ksz8863_IOCtx_t IO;
   void * pData;

   ksz8863_port_t port[KSZ8863_NUM_PORTS];
   bool use_auto_negotiation[KSZ8863_NUM_PORTS];
} ksz8863_Object_t;

typedef struct ksz8863_cfg
{
   uint8_t address;

   phy_mac_address_t mac_address_port[KSZ8863_NUM_PORTS];
   bool use_AN[KSZ8863_NUM_EXT_PORTS];
} ksz8863_cfg_t;

int32_t KSZ8863_RegisterBusIO (ksz8863_Object_t * pObj, ksz8863_IOCtx_t * ioctx);

int32_t KSZ8863_Init (
   const ksz8863_cfg_t * ksz8863_cfg,
   ksz8863_Object_t * pObj);

   int32_t KSZ8863_DeInit (ksz8863_Object_t * pObj);

uint8_t ksz8863_get_port_state (ksz8863_Object_t * pObj, const int port);

uint8_t ksz8863_get_link_state (ksz8863_Object_t * pObj);

uint8_t ksz8863_link_state_for_port (ksz8863_Object_t * pObj, const int port);

uint8_t ksz8863_get_incoming_port (uint8_t * frame, size_t * const size);

phy_mac_address_t ksz8863_get_port_mac_address (
   ksz8863_Object_t * pObj,
   uint8_t port);

   ksz8863_Object_t * ksz8863_get_default_driver(void);

uint16_t ksz8863_get_port_capabilities (ksz8863_Object_t * pObj,
uint8_t port);

uint16_t ksz8863_get_link_capabilities (ksz8863_Object_t * pObj);

void ksz8863_start (ksz8863_Object_t * pObj);

int ksz8863_get_port_statistics (ksz8863_Object_t * pObj,
                                        port_stats_t * port_stats,
                                        uint8_t port);

#ifdef __cplusplus
}
#endif
#endif
