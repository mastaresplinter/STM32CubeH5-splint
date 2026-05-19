/**
 ******************************************************************************
 * @file    LWIP/Target/ethernetif.c
 * @author  Marcus D Niklasson
 * @brief   This file implements Ethernet network interface drivers for lwIP
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "ethernetif.h"
#include "ksz8863.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "main.h"
#include "netif/etharp.h"
#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_def.h"
#include <lwip/snmp.h>
#include <string.h>

#include "app_storage.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

#define ETH_DMA_TRANSMIT_TIMEOUT (20U)

#define ETH_RX_BUFFER_SIZE PBUF_POOL_BUFSIZE
#define ETH_RX_BUFFER_CNT 12U
#define ETH_TX_BUFFER_MAX ((ETH_TX_DESC_CNT) * 2U)

#define INTERFACE_THREAD_STACK_SIZE (2048 * 2)

#define KSZ_CS_GPIO_PORT ETH_SPI_CS_GPIO_Port
#define KSZ_CS_PIN ETH_SPI_CS_Pin
#define MIN_FRAME_SIZE_EXCLUDING_FCS 60

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/*
@Note: This interface is implemented to operate in zero-copy mode only:
        - Rx Buffers will be allocated from LwIP stack Rx memory pool,
          then passed to ETH HAL driver.
        - Tx Buffers will be allocated from LwIP stack memory heap,
          then passed to ETH HAL driver.

@Notes:
  1.a. ETH DMA Rx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_RX_DESC_CNT in ETH GUI (Rx Descriptor
Length) so that updated value will be generated in stm32xxxx_hal_conf.h 1.b. ETH
DMA Tx descriptors must be contiguous, the default count is 4, to customize it
please redefine ETH_TX_DESC_CNT in ETH GUI (Tx Descriptor Length) so that
updated value will be generated in stm32xxxx_hal_conf.h

  2.a. Rx Buffers number: ETH_RX_BUFFER_CNT must be greater than
ETH_RX_DESC_CNT. 2.b. Rx Buffers must have the same size: ETH_RX_BUFFER_SIZE,
this value must passed to ETH DMA in the init field (heth.Init.RxBuffLen)
*/
typedef enum { RX_ALLOC_OK = 0x00, RX_ALLOC_ERROR = 0x01 } RxAllocStatusTypeDef;

typedef struct {
  struct pbuf_custom pbuf_custom;
  uint8_t buff[(ETH_RX_BUFFER_SIZE + 31) & ~31] __ALIGNED(32);
} RxBuff_t;

extern ETH_DMADescTypeDef
    DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
extern ETH_DMADescTypeDef
    DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

/* Memory Pool Declaration */
LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t),
                     "Zero-copy RX PBUF pool");

/* Variable Definitions */
static uint8_t RxAllocStatus;

/* Global Ethernet handle*/
extern ETH_HandleTypeDef heth;
extern ETH_TxPacketConfig TxConfig;
extern SPI_HandleTypeDef hspi6;

osSemaphoreId_t s_xSemaphore = NULL;

/* KSZ8863 Driver Objects */
ksz8863_Object_t KSZ8863;
/* IO Context Prototypes */
uint8_t KSZ8863_IO_Init(void);
uint8_t KSZ8863_IO_DeInit(void);
uint8_t KSZ8863_IO_ReadReg(uint8_t RegAddr);
uint8_t KSZ8863_IO_WriteReg(uint8_t RegAddr, uint8_t RegVal);
uint32_t KSZ8863_IO_GetTick(void);

ksz8863_IOCtx_t KSZ8863_IOCtx = {KSZ8863_IO_Init, KSZ8863_IO_DeInit,
                                 KSZ8863_IO_WriteReg, KSZ8863_IO_ReadReg,
                                 KSZ8863_IO_GetTick};

/* Tail Tag Buffer (1 byte) */
static uint8_t TxTailTagBuffer = 0x00;

/* Private function prototypes -----------------------------------------------*/
u32_t sys_now(void);

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth) {
  osSemaphoreRelease(s_xSemaphore);
}

/* Private functions ---------------------------------------------------------*/
void pbuf_free_custom(struct pbuf *p);
/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH)
*******************************************************************************/
/**
 * @brief In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif) {

  ETH_MACConfigTypeDef MACConf = {0};
  phy_mac_address_t mac_temp;

  if (netif->state == (void *)3) {
    /* Initialize OS Primitives */
    if (s_xSemaphore == NULL) {
      s_xSemaphore = osSemaphoreNew(1, 1, NULL);
      LWIP_MEMPOOL_INIT(RX_POOL);
    }

    static ksz8863_cfg_t ksz8863_cfg = {
        .address = 3,
        .use_AN[0] = true,
        .use_AN[1] = true,
    };

    /* Get MAC addresses from EEPROM */
    if (app_storage_get_mac_by_index(0, mac_temp.address) == APP_STORAGE_OK) {
      ksz8863_cfg.mac_address_port[0] = mac_temp;
    }
    if (app_storage_get_mac_by_index(1, mac_temp.address) == APP_STORAGE_OK) {
      ksz8863_cfg.mac_address_port[1] = mac_temp;
    }
    if (app_storage_get_mac_by_index(2, mac_temp.address) == APP_STORAGE_OK) {
      ksz8863_cfg.mac_address_port[2] = mac_temp;
    }

    /* Set CPU ETH handle */
    memcpy(heth.Init.MACAddr, &ksz8863_cfg.mac_address_port[2], 6);

    if (HAL_ETH_Init(&heth) != HAL_OK) {
      Error_Handler();
    }

    memset(&TxConfig, 0, sizeof(ETH_TxPacketConfigTypeDef));
    TxConfig.Attributes =
        ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
    TxConfig.SrcAddrCtrl = ETH_SRC_ADDR_CONTROL_DISABLE;

    /* Commuication between switch and CPU is fixed */
    HAL_ETH_GetMACConfig(&heth, &MACConf);
    MACConf.SourceAddrControl = ETH_SOURCEADDRESS_DISABLE;
    MACConf.DuplexMode = ETH_FULLDUPLEX_MODE;
    MACConf.Speed = ETH_SPEED_100M;
    HAL_ETH_SetMACConfig(&heth, &MACConf);

    if (KSZ8863.Is_Initialized == 0) {
      KSZ8863_RegisterBusIO(&KSZ8863, &KSZ8863_IOCtx);
      KSZ8863_Init(&ksz8863_cfg, &KSZ8863);
    }

    /* LwIP MAC Setup */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(netif->hwaddr, heth.Init.MACAddr, ETHARP_HWADDR_LEN);

    heth.Instance->MACPFR |= ETH_MACPFR_PR;

    ksz8863_start(&KSZ8863);

    netif->state = &KSZ8863;

    osThreadAttr_t attributes;
    memset(&attributes, 0x0, sizeof(osThreadAttr_t));
    attributes.name = "EthRcv";
    attributes.stack_size = INTERFACE_THREAD_STACK_SIZE;
    attributes.priority = osPriorityRealtime;
    osThreadNew(ethernetif_input, netif, &attributes);
  }
}

/**
 * @brief This function should do the actual transmission of the packet. The
 * packet is contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and
 * type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p) {
  err_t errval = ERR_OK;
  static ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT + 2];
  static uint8_t paddingBuffer[MIN_FRAME_SIZE_EXCLUDING_FCS];

  memset(Txbuffer, 0, sizeof(Txbuffer));
  memset(paddingBuffer, 0, sizeof(paddingBuffer));

  TxTailTagBuffer = (uint8_t)p->port_num;
  struct pbuf *q;
  int i = 0;
  uint32_t current_total_len = 0;

  for (q = p; q != NULL; q = q->next) {
    if (i >= ETH_TX_DESC_CNT)
      return ERR_IF;

    Txbuffer[i].buffer = q->payload;
    Txbuffer[i].len = q->len;
    current_total_len += q->len;

    Txbuffer[i].next = &Txbuffer[i + 1];
    i++;
  }

  uint32_t padding_len = 0;
  if (current_total_len <
      MIN_FRAME_SIZE_EXCLUDING_FCS) /* If less than 60 Bytes */
  {
    padding_len = MIN_FRAME_SIZE_EXCLUDING_FCS - current_total_len;
  }

  if (padding_len > 0) {
    Txbuffer[i].buffer = paddingBuffer;
    Txbuffer[i].len = padding_len;
    Txbuffer[i].next = &Txbuffer[i + 1];
    i++;
  }

  Txbuffer[i].buffer = &TxTailTagBuffer;
  Txbuffer[i].len = 1;
  Txbuffer[i].next = NULL;

  TxConfig.Length = current_total_len + padding_len + 1;
  TxConfig.TxBuffer = Txbuffer;
  TxConfig.pData = p;
#ifdef TEST_BUILD
  if (current_total_len == 185) {
    printf("[TX COMPLETE DUMP] Frame size %d:\n", current_total_len);

    int byte_count = 0;
    for (int j = 0; j < i + 1; j++) {
      uint8_t *buf = (uint8_t *)Txbuffer[j].buffer;
      for (uint32_t k = 0; k < Txbuffer[j].len; k++) {
        printf("%02X ", buf[k]);
        byte_count++;
        if (byte_count % 16 == 0)
          printf("\n");
      }
    }
    if (byte_count % 16 != 0)
      printf("\n");
    printf("[TX COMPLETE DUMP] Total bytes: %d\n", byte_count);
  }
#endif
  HAL_StatusTypeDef hal_status =
      HAL_ETH_Transmit(&heth, &TxConfig, ETH_DMA_TRANSMIT_TIMEOUT);
  if (hal_status != HAL_OK) {
    errval = ERR_IF;
  }

  return errval;
}

/**
 * @brief Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *low_level_input(struct netif *netif) {
  struct pbuf *p = NULL;

  if (RxAllocStatus == RX_ALLOC_OK) {
    HAL_ETH_ReadData(&heth, (void **)&p);
  }
  return p;
}

#ifdef TEST_BUILD
static uint32_t g_loopback_rx_counter = 0;
#endif

/**
 * @brief This function is the ethernetif_input task, it is processed when a
 * packet is ready to be read from the interface. It uses the function
 * low_level_input() that should handle the actual reception of bytes from the
 * network interface. Then the type of the received packet is determined and the
 * appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void ethernetif_input(void *arg) {
  struct pbuf *p;
  struct netif *netif = (struct netif *)arg;
  for (;;) {
    if (osSemaphoreAcquire(s_xSemaphore, osWaitForever) == osOK) {
      do {
        LOCK_TCPIP_CORE();
        p = low_level_input(netif);
        if (p != NULL) {
#ifdef TEST_BUILD
          /* Check if this is a loopback test frame */
          uint8_t *payload = (uint8_t *)p->payload;
          if (payload[0] == 0x00 && payload[1] == 0x01) { // Test pattern
            g_loopback_rx_counter++;
          }
#endif

          /* Get the receiving port on the Ethernet switch */
          size_t adjusted_size = p->len;

          p->port_num = ksz8863_get_incoming_port(p->payload, &adjusted_size);
          p->tot_len = adjusted_size;
          p->len = adjusted_size;

          if (netif && netif->input(p, netif) != ERR_OK) {
            pbuf_free(p);
          }
        }
        UNLOCK_TCPIP_CORE();
      } while (p != NULL);
    }
  }
}

phy_mac_address_t ethernetif_get_port_mac_address(uint8_t port) {
  ksz8863_Object_t *pObj = ksz8863_get_default_driver();
  assert(pObj != NULL);

  return ksz8863_get_port_mac_address(pObj, port);
}

void ethernetif_get_status(struct netif *netif, eth_status_t *link,
                           uint8_t port) {
  link->is_autonegotiation_supported = true;
  link->is_autonegotiation_enabled = true;
  link->capabilities = ksz8863_get_port_capabilities(&KSZ8863, port);
  link->state = ksz8863_get_port_state(&KSZ8863, port);
  link->is_operational = ((link->state & PHY_LINK_OK) != 0);
}
int ethernetif_get_port_statistics(void *arg, port_stats_t *port_stats,
                                   uint8_t port) {
  ksz8863_Object_t *pObj = (ksz8863_Object_t *)arg;
  return ksz8863_get_port_statistics(pObj, port_stats, port);
}

/**
 * @brief Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif) {
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;
  netif->mtu = 1500;
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
  MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 0);

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

/**
 * @brief  Custom Rx pbuf free callback
 * @param  pbuf: pbuf to be freed
 * @retval None
 */
void pbuf_free_custom(struct pbuf *p) {
  struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;
  LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
  /* If the Rx Buffer Pool was exhausted, signal the ethernetif_input task to
   * call HAL_ETH_GetRxDataBuffer to rebuild the Rx descriptors. */
  if (RxAllocStatus == RX_ALLOC_ERROR) {
    RxAllocStatus = RX_ALLOC_OK;
  }
}

/**
 * @brief  Returns the current time in milliseconds
 *         when LWIP_TIMERS == 1 and NO_SYS == 1
 * @param  None
 * @retval Current Time value
 */
u32_t sys_now(void) { return HAL_GetTick(); }

/*******************************************************************************
                        KSZ8863 SPI IO Functions
*******************************************************************************/

uint8_t KSZ8863_IO_Init(void) {
  /* Assumes hspi6 is already initialized by main.c/MX_SPI6_Init */
  return 0;
}

uint8_t KSZ8863_IO_DeInit(void) { return 0; }

/* SPI Write: Command (0x02) + Address + Data */
uint8_t KSZ8863_IO_WriteReg(uint8_t RegAddr, uint8_t RegVal) {
  uint8_t txData[3];
  txData[0] = 0x02; /* Write Command */
  txData[1] = RegAddr;
  txData[2] = RegVal;

  HAL_GPIO_WritePin(KSZ_CS_GPIO_PORT, KSZ_CS_PIN, GPIO_PIN_RESET);
  /* SPI6 Handle used here */
  HAL_SPI_Transmit(&hspi6, txData, 3, 100);
  HAL_GPIO_WritePin(KSZ_CS_GPIO_PORT, KSZ_CS_PIN, GPIO_PIN_SET);

  return 0;
}

/* SPI Read: Command (0x03) + Address -> Receive Data */
uint8_t KSZ8863_IO_ReadReg(uint8_t RegAddr) {
  uint8_t txData[3];
  uint8_t rxData[3] = {0xFF};

  txData[0] = 0x03; /* Read Command */
  txData[1] = RegAddr;
  txData[2] = 0xFF; /* Dummy Byte for Read */

  HAL_GPIO_WritePin(KSZ_CS_GPIO_PORT, KSZ_CS_PIN, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(&hspi6, txData, rxData, 3, HAL_MAX_DELAY);
  HAL_GPIO_WritePin(KSZ_CS_GPIO_PORT, KSZ_CS_PIN, GPIO_PIN_SET);

  return rxData[2];
}

uint32_t KSZ8863_IO_GetTick(void) { return HAL_GetTick(); }

/**
 * @brief  Link monitoring thread.
 * Polls the KSZ8863 to determine if we have connectivity.
 */
void ethernetif_set_link(void *argument) {
  struct netif *netif = (struct netif *)argument;
  uint8_t current_phy_state;
  uint8_t prev_link_up = 0xFF;

  for (;;) {
    /* Get combined link state of external ports (Port 1 | Port 2) */
    current_phy_state = ksz8863_get_link_state(netif->state);
    uint8_t is_link_up = (current_phy_state & PHY_LINK_OK) ? 1 : 0;

    if (is_link_up != prev_link_up) {
      if (is_link_up) {
        /* Physical Link came UP */
        netif_set_link_up(netif);
      } else {
        /* Physical Link went DOWN */
        netif_set_link_down(netif);
      }

      prev_link_up = is_link_up;
    }

    osDelay(50);
  }
}

void ethernetif_update_config(struct netif *netif) {

  if (netif_is_link_up(netif)) {
    if (heth.gState == HAL_ETH_STATE_READY) {
      HAL_ETH_Start_IT(&heth);
    }
    netif_set_up(netif);
#ifdef TEST_BUILD
    RunCompleteLoopbackTestTHREAD(netif);
    while (1) {
    }
#endif
  } else {
    netif_set_down(netif);
    HAL_ETH_Stop(&heth);
  }
}

void HAL_ETH_RxAllocateCallback(uint8_t **buff) {
  struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
  if (p) {
    /* Get the buff from the struct pbuf address. */
    *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
    p->custom_free_function = pbuf_free_custom;
    /* Initialize the struct pbuf.
     * This must be performed whenever a buffer's allocated because it may be
     * changed by lwIP or the app, e.g., pbuf_free decrements ref. */
    pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
  } else {
    RxAllocStatus = RX_ALLOC_ERROR;
    *buff = NULL;
  }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff,
                            uint16_t Length) {
  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd = (struct pbuf **)pEnd;
  struct pbuf *p = NULL;

  /* Get the struct pbuf from the buff address. */
  p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
  p->next = NULL;
  p->tot_len = 0;
  p->len = Length;

  /* Chain the buffer. */
  if (!*ppStart) {
    /* The first buffer of the packet. */
    *ppStart = p;
  } else {
    /* Chain the buffer to the end of the packet. */
    (*ppEnd)->next = p;
  }
  *ppEnd = p;

  /* Update the total length of all the buffers of the chain. Each pbuf in the
   * chain should have its tot_len set to its own length, plus the length of all
   * the following pbufs in the chain. */
  for (p = *ppStart; p != NULL; p = p->next) {
    p->tot_len += Length;
  }
}

void HAL_ETH_TxFreeCallback(uint32_t *buff) { pbuf_free((struct pbuf *)buff); }

/*****TESTS******* */

#ifdef TEST_BUILD
typedef struct {
  uint32_t frames_sent;
  uint32_t frames_received;
  uint32_t frames_with_padding;
  uint32_t test_passed;
} LoopbackTestResults_t;

/* Test frame sizes that require padding */
static const uint16_t test_frame_sizes[] = {
    20, // Very small - needs heavy padding
    40, // Small - needs padding
    50, // Just under minimum - needs padding
    58, // Just needs 1 byte padding
    59, // No padding needed (59 + 1 tag = 60)
    100 // Normal size - no padding needed
};

void EnableSTM32_MACLoopback(void) {
  /* Enable MAC loopback mode by setting the LM bit in MACCR */
  heth.Instance->MACCR |= ETH_MACCR_LM;

  printf("STM32 MAC Loopback enabled\n");
}

void DisableSTM32_MACLoopback(void) {
  /* Disable MAC loopback mode */
  heth.Instance->MACCR &= ~ETH_MACCR_LM;

  printf("STM32 MAC Loopback disabled\n");
}

LoopbackTestResults_t STM32_RunMACLoopbackTest(struct netif *netif,
                                               uint16_t frame_size) {
  LoopbackTestResults_t results = {0};
  struct pbuf *p;
  static uint8_t test_data[200];
  err_t err;

  printf("  [DEBUG] MAC Loopback test for frame size %d\n", frame_size);

  /* Prepare test data with recognizable pattern */
  for (int i = 0; i < frame_size; i++) {
    test_data[i] = (uint8_t)(i & 0xFF);
  }

  /* Enable MAC loopback */
  EnableSTM32_MACLoopback();
  osDelay(100); // Allow loopback mode to stabilize

  /* Reset RX counter */
  g_loopback_rx_counter = 0;

  /* Allocate pbuf */
  p = pbuf_alloc(PBUF_RAW, frame_size, PBUF_POOL);
  if (p == NULL) {
    printf("  [ERROR] Failed to allocate pbuf\n");
    DisableSTM32_MACLoopback();
    return results;
  }

  /* Copy test data */
  pbuf_take(p, test_data, frame_size);

  /* Set port_num (not used in MAC loopback, but keeps code consistent) */
  p->port_num = 0x01;

  /* Track padding */
  if (frame_size < 59) {
    results.frames_with_padding = 1;
  }

  /* Send frame */
  err = low_level_output(netif, p);

  if (err == ERR_OK) {
    results.frames_sent = 1;
    printf("  [DEBUG] Frame transmitted\n");

    /* Wait for loopback reception */
    osDelay(200);

    /* Check if frame was received */
    results.frames_received = g_loopback_rx_counter;

    if (results.frames_received > 0) {
      results.test_passed = 1;
      printf("  [PASS] Frame looped back successfully\n");
    } else {
      printf("  [FAIL] Frame not received\n");
    }
  } else {
    printf("  [ERROR] Transmission failed: %d\n", err);
    pbuf_free(p);
  }

  /* Disable MAC loopback */
  DisableSTM32_MACLoopback();

  return results;
}

void VerifyETHStatus(void) {
  HAL_ETH_StateTypeDef eth_state = HAL_ETH_GetState(&heth);
  printf("ETH State: %d (HAL_ETH_STATE_READY=%d)\n", eth_state,
         HAL_ETH_STATE_READY);

  // Check if link is up
  uint32_t macpfr = heth.Instance->MACPFR;
  printf("MACPFR: 0x%08X\n", macpfr);

  // Check DMA status
  uint32_t dmasr = heth.Instance->DMACSR;
  printf("DMACSR: 0x%08X\n", dmasr);
}

/* Enable Far-end Loopback for Port 1 */
void KSZ8863_EnableLoopback_Port1(void) {
  uint8_t reg_value;

  /* Read register 29, bit [0] enables far-end loopback for Port 1 */
  reg_value = KSZ8863_IO_ReadReg(29);
  reg_value |= 0x01; // Set bit 0
  KSZ8863_IO_WriteReg(29, reg_value);

  /* Alternatively, use MII Management register 0, bit [14] */
  // reg_value = KSZ8863_IO_ReadReg(0);
  // reg_value |= (1 << 14);
  // KSZ8863_IO_WriteReg(0, reg_value);
}

/* Enable Far-end Loopback for Port 2 */
void KSZ8863_EnableLoopback_Port2(void) {
  uint8_t reg_value;

  /* Read register 45, bit [0] enables far-end loopback for Port 2 */
  reg_value = KSZ8863_IO_ReadReg(45);
  reg_value |= 0x01; // Set bit 0
  KSZ8863_IO_WriteReg(45, reg_value);
}

LoopbackTestResults_t KSZ8863_RunLoopbackTest(struct netif *netif,
                                              uint8_t test_port,
                                              uint16_t frame_size) {
  LoopbackTestResults_t results = {0};
  struct pbuf *p;
  uint8_t test_data[200];
  err_t err;

  printf("  [DEBUG] Starting test for frame size %d\n", frame_size);

  /* Prepare test data with recognizable pattern */
  for (int i = 0; i < frame_size; i++) {
    test_data[i] = (uint8_t)(i & 0xFF);
  }

  /* Enable loopback on the test port */
  if (test_port == 1) {
    KSZ8863_EnableLoopback_Port1();
    printf("  [DEBUG] Loopback enabled on Port 1\n");
  } else if (test_port == 2) {
    KSZ8863_EnableLoopback_Port2();
    printf("  [DEBUG] Loopback enabled on Port 2\n");
  }

  /* Wait for loopback mode to stabilize */
  osDelay(100);

  /* Allocate pbuf for test frame */
  p = pbuf_alloc(PBUF_RAW, frame_size, PBUF_RAM);
  if (p == NULL) {
    printf("  [ERROR] Failed to allocate pbuf\n");
    return results;
  }
  printf("  [DEBUG] pbuf allocated successfully\n");

  /* Copy test data to pbuf */
  memcpy(p->payload, test_data, frame_size);

  /* Set port number for tail tagging */
  p->port_num = (1 << (test_port - 1)); // 0x01 for Port 1, 0x02 for Port 2
  printf("  [DEBUG] port_num set to 0x%02X\n", p->port_num);

  /* Track if padding will be applied */
  if (frame_size < 59) {
    results.frames_with_padding = 1;
  }

  /* Send the frame */
  err = low_level_output(netif, p);
  printf("  [DEBUG] low_level_output returned: %d (ERR_OK=%d)\n", err, ERR_OK);

  if (err == ERR_OK) {
    results.frames_sent = 1;
    printf("  [DEBUG] Frame sent successfully\n");
  } else {
    printf("  [ERROR] Frame send failed with error: %d\n", err);
  }

  /* Wait for loopback reception */
  osDelay(50);

  /* Cleanup */
  pbuf_free(p);

  /* Disable loopback */
  if (test_port == 1) {
    uint8_t reg_value = KSZ8863_IO_ReadReg(29);
    reg_value &= ~0x01;
    KSZ8863_IO_WriteReg(29, reg_value);
  } else if (test_port == 2) {
    uint8_t reg_value = KSZ8863_IO_ReadReg(45);
    reg_value &= ~0x01;
    KSZ8863_IO_WriteReg(45, reg_value);
  }

  return results;
}

void RunCompleteLoopbackTest(struct netif *netif) {
  printf("Starting KSZ8863 Loopback Test...\n");

  for (int i = 0; i < sizeof(test_frame_sizes) / sizeof(test_frame_sizes[0]);
       i++) {
    uint16_t size = test_frame_sizes[i];

    VerifyETHStatus();

    g_loopback_rx_counter = 0; // Reset counter

    LoopbackTestResults_t results = KSZ8863_RunLoopbackTest(netif, 1, size);

    printf("Frame size %d: Sent=%d, Received=%d, Padded=%d\n", size,
           results.frames_sent, g_loopback_rx_counter,
           results.frames_with_padding);

    osDelay(200); // Delay between tests
  }

  printf("Loopback Test Complete\n");
}

void RunMACLoopbackTest(struct netif *netif) {
  printf("\n=== Starting STM32 MAC Loopback Test ===\n");

  for (int i = 0; i < sizeof(test_frame_sizes) / sizeof(test_frame_sizes[0]);
       i++) {
    uint16_t size = test_frame_sizes[i];

    LoopbackTestResults_t results = STM32_RunMACLoopbackTest(netif, size);

    printf("Frame size %3d: Sent=%d, Received=%d, Padded=%d, Result=%s\n", size,
           results.frames_sent, results.frames_received,
           results.frames_with_padding, results.test_passed ? "PASS" : "FAIL");

    osDelay(300); // Delay between tests
  }

  printf("=== MAC Loopback Test Complete ===\n\n");
}

void RunCompleteLoopbackTestTHREAD(struct netif *netif) {
  osThreadAttr_t attributes;
  memset(&attributes, 0x0, sizeof(osThreadAttr_t));
  attributes.name = "LoopTest";
  attributes.stack_size = 4096;
  attributes.priority = osPriorityNormal;

  osThreadNew(RunMACLoopbackTest, netif, &attributes);
}
#endif
