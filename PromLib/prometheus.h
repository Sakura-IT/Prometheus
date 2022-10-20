#ifndef LIBRARIES_PROMETHEUS_H
#define LIBRARIES_PROMETHEUS_H

/* $VER: prometheus.h 2.6 (01.03.2010) © e3b. */

#define PROMETHEUSNAME "prometheus.library"
#define PROMETHEUSVERSION 2
#define PROMETHEUSMINVERSION 6
#define PROMETHEUSVERSIONSTRING "\0$VER: prometheus.library 2.6 (01.03.2010)\r\n"

#ifndef PCIBOARD_TYPEDEF
#define PCIBOARD_TYPEDEF
typedef VOID PCIBoard;
#endif /* PCIBOARD_TYPEDEF */

/* Tags for Prm_FindBoardTagList(), Prm_GetBoardAttrsTagList() */
/* and Prm_SetBoardAttrsTagList().                             */
/* 'S' - the attribute is settable.                            */
/* 'G' - the attribute is gettable.                            */

#define PRM_Vendor            0x6EDA0000    /* [.G] */
#define PRM_Device            0x6EDA0001    /* [.G] */
#define PRM_Revision          0x6EDA0002    /* [.G] */
#define PRM_Class             0x6EDA0003    /* [.G] */
#define PRM_SubClass          0x6EDA0004    /* [.G] */

/* You can depend on fact that the last nybble of PRM_MemoryAddrX and  */
/* PRM_MemorySizeX is equal to X. It is guarranted in future releases. */

#define PRM_MemoryAddr0       0x6EDA0010    /* [.G] */
#define PRM_MemoryAddr1       0x6EDA0011    /* [.G] */
#define PRM_MemoryAddr2       0x6EDA0012    /* [.G] */
#define PRM_MemoryAddr3       0x6EDA0013    /* [.G] */
#define PRM_MemoryAddr4       0x6EDA0014    /* [.G] */
#define PRM_MemoryAddr5       0x6EDA0015    /* [.G] */
#define PRM_ROM_Address       0x6EDA0016    /* [.G] */

#define PRM_MemorySize0       0x6EDA0020    /* [.G] */
#define PRM_MemorySize1       0x6EDA0021    /* [.G] */
#define PRM_MemorySize2       0x6EDA0022    /* [.G] */
#define PRM_MemorySize3       0x6EDA0023    /* [.G] */
#define PRM_MemorySize4       0x6EDA0024    /* [.G] */
#define PRM_MemorySize5       0x6EDA0025    /* [.G] */
#define PRM_ROM_Size          0x6EDA0026    /* [.G] */

/* Tags added in V2 of library */

#define PRM_BoardOwner        0x6EDA0005    /* [SG] */
#define PRM_SlotNumber        0x6EDA0006    /* [.G] */
#define PRM_FunctionNumber    0x6EDA0007    /* [.G] */
#define PRM_BusNumber	      0x6EDA0008    /* [.G] */

/* unknown tags */

#define PRM_HeaderType        0x6EDA0009
#define PRM_SubsysVendor      0x6EDA000A
#define PRM_SubsysID          0x6EDA000B
#define PRM_Interface         0x6EDA000C

#endif /* LIBRARIES_PROMETHEUS_H */


