/* header file for NE2000 driver for Prometheus PCI bridge */
/* $Id: ne2000.h,v 1.1 2022/10/20 15:38:08 rkujawa Exp $ */

#include <exec/types.h>

#ifndef PRMNE2000_NE2000_H
#define PRMNE2000_NE2000_H

/* some register constans */

#define COMMAND_PAGE0   0x00
#define COMMAND_PAGE1   0x40
#define COMMAND_PAGE2   0x80
#define COMMAND_READ    0x08
#define COMMAND_WRITE   0x10
#define COMMAND_SEND    0x18
#define COMMAND_ABORT   0x20
#define COMMAND_TXP     0x04
#define COMMAND_START   0x02
#define COMMAND_STOP    0x01

#define TXCFG_COLOFFSET 0x10
#define TXCFG_ATDIS     0x08
#define TXCFG_LOOP_MASK 0x06
#define TXCFG_LOOP_NONE 0x00
#define TXCFG_LOOP_INT  0x02
#define TXCFG_LOOP_EXT  0x04
#define TXCFG_CRC       0x01

#define RXCFG_MONITOR   0x20
#define RXCFG_PROM      0x10
#define RXCFG_MCAST     0x08
#define RXCFG_BCAST     0x04
#define RXCFG_SHORTPKT  0x02
#define RXCFG_RXERR     0x01

#define DTCFG_FIFOTRSH  0x60
#define DTCFG_FIFO_8    0x40
#define DTCFG_AUTORM    0x10
#define DTCFG_LOOPSEL   0x08
#define DTCFG_LAS       0x04
#define DTCFG_BYTESWAP  0x02
#define DTCFG_WIDE      0x01

#define INT_RESET       0x80
#define INT_DMAFINISH   0x40
#define INT_COUNTEROVF  0x20
#define INT_OVERFLOW    0x10
#define INT_TXERROR     0x08
#define INT_RXERROR     0x04
#define INT_TXPACKET    0x02
#define INT_RXPACKET    0x01

#define TXS_TRANSMITTED_OK    0x01

#define NE2000_COMMAND             0
#define NE2000_PAGE_START          1
#define NE2000_PHYSICAL_ADDR0      1      /* register page 1 */
#define NE2000_PAGE_STOP           2
#define NE2000_BOUNDARY            3
#define NE2000_TX_PAGE_START       4      /* write */
#define NE2000_TX_STATUS           4      /* read */
#define NE2000_TX_COUNTER0         5
#define NE2000_TX_COUNTER1         6
#define NE2000_CURRENT_PAGE        7      /* register page 1 */
#define NE2000_INT_STATUS          7
#define NE2000_DMA_START_ADDR0     8
#define NE2000_DMA_START_ADDR1     9
#define NE2000_DMA_COUNTER0       10
#define NE2000_DMA_COUNTER1       11
#define NE2000_RX_CONFIG          12
#define NE2000_TX_CONFIG          13
#define NE2000_DATA_CONFIG        14
#define NE2000_INT_MASK           15

struct Ne2000
 {
  UBYTE regs[16];
  UWORD DMAPort;
  UWORD pad0;
  UWORD pad1;
  UWORD pad2;
  UWORD ResetPort;
 };


struct EthFrame
 {
  UBYTE ef_DestAddr[6];
  UBYTE ef_SrcAddr[6];
  UWORD ef_Type;
  UBYTE ef_Data[46];       /* shortest possible */
 };

#endif
