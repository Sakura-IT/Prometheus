/* 
$VER: driver.c 4.4 (08.05.2023) by Dennis Boon with fixes from Mathias Heyer
- Removed restriction on AllocDMA/FreeDMA - prometheus.card now does the check
- Some S3 cards can now be used for DMA purposes
- Search for Prometheus.card under LIBS:Picasso96/
- Fixed translation of physical to virtual address
- Increased reset time of the Prometheus/Firebird bridge per hints from Mathias Heyer
$VER: driver.c 4.3 (26.01.2023) by Dennis Boon
- Fixed set-up of graphics memory of Prometheus cards with original firmware
$VER: driver.c 4.2 (15.02.2022) by Dennis Boon
- Fixed set-up of bridges beyond bridges
$VER: driver.c 4.1 (04.12.2021) by Dennis Boon
- Fixed set-up of multi-functional PCI cards
- Fixed set-up of PCI(e) bridges
$VER: driver.c 4.0 (08.05.2021) by Dennis Boon
- Added Alloc/FreePCIAddressSpace functions
- Added support for various PPC cards
$VER: driver.c 3.1 (12.04.2010) by Tobias Seiler
- Added support for E3B FireStorm upgraded cards
$VER: driver.c 3.0 (13.03.2005) by Benjamin Vernoux
- Fixed bug in Prm_WriteConfXXXX ULONG data reg and UBYTE offset reg register was reversed.
- Fixed Prm_ReadConfXXXX & Prm_WriteConfXXXX removed offset rounded down now it use REAL offset.
- Added function GetVirtualAddress()
$VER: driver.c 2.5 (19.12.2002) by Grzegorz Kraszewski 
*/

//#define TESTEXE
//#define DEBUG
//void illegal(void)="\tillegal\n";

#define __NOLIBBASE__

#include <stdarg.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/utility.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <exec/nodes.h>
#include <exec/execbase.h>
#include <libraries/configvars.h>
#include <hardware/intbits.h>
#include <utility/tagitem.h>
#include <devices/timer.h>

/* from private include directory */

#define PCIBOARD_TYPEDEF
typedef struct
{
  struct MinNode pn_Node;
  volatile struct PciConfig *pn_ConfigBase;     /* v2 */
  struct TagItem pn_TagList[30];                /* increased from 25 (27 different tags in total in V4) */
} PCIBoard;


#ifdef __VBCC__
#include <inline/prometheuscard_protos.h> //private include
#else
#include <proto/prometheus_card.h>
#endif

#include "prometheus.h"
#include "endian.h"
#include "lists.h"

#define FS_PCI_ADDR_CONFIG0    0x1fc00000
#define FS_PCI_ADDR_CONFIG1    0x1fd00000
#define FS_PCI_ADDR_IO         0x1fe00000

#define PCI_MEMSIZE_4KB        0xfffff000
#define PCI_MEMSIZE_64KB       0xffff0000
#define PCI_MEMSIZE_256KB      0xfffc0000
#define PCI_MEMSIZE_4MB        0xffc00000
#define PCI_MEMSIZE_16MB       0xff000000
#define PCI_MEMSIZE_64MB       0xfc000000
#define PCI_MEMSIZE_128MB      0xf8000000

#define VID_FREESCALE          0x1957
#define VID_3DFX               0x121a
#define VID_MOTOROLA           0x1057
#define VID_TI                 0x104c
#define VID_ATI                0x1002
#define VID_S3                 0x5333

#define DEVID_MPC107           0x0004
#define DEVID_MPC834X          0x0086
#define DEVID_MPC831X          0x00b6
#define DEVID_POWERPLUSIII     0x480b
#define DEVID_PERMEDIA2        0x3d07
#define DEVID_VIRGE3D          0x5631
#define DEVID_VIRGEDX          0x8A01
#define DEVID_VIRGEGX2         0x8A02

struct PrometheusBase
{
  struct Library pb_Lib;
  struct Library *pb_SysBase;
  struct Library *pb_UtilityBase;
  struct Library *pb_DMASuppBase; /* base of the library supporting DMA functions */
  APTR pb_SegList;
  APTR pb_MemPool;
  APTR pb_BaseAddr;
  struct MinList pb_Cards;
  struct List pb_Busses;       /* this list is private used only at startup time */
  BOOL pb_FireStorm;
  UBYTE pb_BridgeCnt;
};

struct PCIBus
{
	struct Node br_Node;
	UBYTE br_BusNr;
	UBYTE br_pBusNr;
	BOOL  br_hasGraphics;
	BOOL  br_hasUpperIO;
	struct PciConfig volatile *br_ConfigBase;
	struct List br_CardRequests;
};

struct SpaceReq
{
  struct Node sr_Node;
  ULONG sr_Size;
  ULONG sr_Type;
  ULONG sr_Flag;
  struct TagItem *sr_Tag;
  ULONG volatile *sr_CfgAddr;                   /* ptr to base register */
};

struct PciConfig
 {
  UWORD pc_Vendor;               /* these two words swapped! */
  UWORD pc_Device;

  UWORD pc_Command;              /* these two words swapped! */
  UWORD pc_Status;

  UBYTE pc_Revision;             /* these four bytes swapped! */
  UBYTE pc_Interface;
  UBYTE pc_SubClass;
  UBYTE pc_Class;

  UBYTE pc_CacheLineSize;        /* these four bytes swapped! */
  UBYTE pc_LatencyTimer;
  UBYTE pc_HeaderType;
  UBYTE pc_BIST;

  union hts {
	  struct ht0 {
		ULONG pc_BaseRegs[6];
		ULONG pc_Cardbus;              /* currently unused */
		UWORD pc_SybsystemVID;
		UWORD pc_SubsystemID;
		ULONG pc_ROM;

		ULONG pc_Reserved[2];

		UBYTE pc_IntLine;
		UBYTE pc_IntPin;
		UBYTE pc_MinGnt;
		UBYTE pc_MaxLat;
	  } t0;

	  struct ht1 {
		ULONG pc_BaseRegs[2];
		UBYTE pc_PrimaryBus;
		UBYTE pc_SecondaryBus;
		UBYTE pc_SubordinateBus;
		UBYTE pc_SecondaryLat;

		UBYTE pc_IOBase;
		UBYTE pc_IOLimit;
		UWORD pc_SecondaryStatus;
		UWORD pc_MemoryBase;
		UWORD pc_MemoryLimit;
		UWORD pc_PrefetchMemBase;
		UWORD pc_PrefetchMemLimit;

		ULONG pc_PrefetchBaseUp;
		ULONG pc_PrefetchLimitUp;

		UWORD pc_IOBaseUp;
		UWORD pc_IOLimitUp;

		ULONG pc_Reserved;
		ULONG pc_ROM;

		UBYTE pc_IntLine;
		UBYTE pc_IntPin;
		UWORD pc_BridgeControl;
	  } t1;
	  UBYTE pc_Unused[192];
  } types;
};

#define BLOCK_MEMORY   0
#define BLOCK_INOUT    1
#define BLOCK_GFXMEM   2
#define BLOCK_USRMEM   3
#define BLOCK_CFGMEM   4

#define VERSION  4
#define REVISION 4

#define CARD_NAME "Picasso96/Prometheus.card"

#ifdef DEBUG
#define __DBG__ "debug "
#else
#define __DBG__
#endif

char libid[]   = "\0$VER: prometheus.library 4.4 " __DBG__ "(08.05.2023)\r\n";
char build[]   = "build date: " __DATE__ ", " __TIME__ "\n";
char libname[] = "prometheus.library\0";

/*--------------------------------------------------------------------------*/

#ifndef TESTEXE

/*--- Functions prototypes -------------------------------------------------*/

struct PrometheusBase *LibInit (__reg("d0") struct PrometheusBase *pb, __reg("a0") void *seglist, __reg("a6") struct Library *sysb);
struct PrometheusBase *LibOpen (__reg("a6") struct PrometheusBase *pb);
LONG LibClose (__reg("a6") struct PrometheusBase *pb);
APTR LibExpunge (__reg("a6") struct PrometheusBase *pb);
long LibReserved (void);
PCIBoard* FindBoardTagList (__reg("a6") struct PrometheusBase *pb , __reg("a0") PCIBoard *node, __reg("a1") struct TagItem *taglist);
ULONG GetBoardAttrsTagList (__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("a1") struct TagItem *taglist);
ULONG ReadConfigLong(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE offset);
UWORD ReadConfigWord(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE offset);
UBYTE ReadConfigByte(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE offset);
void WriteConfigLong(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") ULONG data, __reg("d1") UBYTE offset);
void WriteConfigWord(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UWORD data, __reg("d1") UBYTE offset);
void WriteConfigByte(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE data, __reg("d1") UBYTE offset);
LONG SetBoardAttrsTagList(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("a1") struct TagItem *taglist);
BOOL AddIntServer_(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("a1") struct Interrupt *intr);
void RemIntServer_(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("a1") struct Interrupt *intr);
APTR AllocDMABuffer(__reg("a6") struct PrometheusBase *pb, __reg("d0") ULONG size);
void FreeDMABuffer(__reg("a6") struct PrometheusBase *pb, __reg("a0") APTR buffer, __reg("d0") ULONG size);
APTR GetPhysicalAddress(__reg("a6") struct PrometheusBase *pb, __reg("d0") APTR addr);
APTR GetVirtualAddress(__reg("a6") struct PrometheusBase *pb, __reg("d0") APTR addr);
APTR AllocPCIAddressSpace(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") ULONG size, __reg("d1") ULONG bar);
void FreePCIAddressSpace(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") ULONG bar);

void *FuncTable[] =
 {
  (APTR)LibOpen,
  (APTR)LibClose,
  (APTR)LibExpunge,
  (APTR)LibReserved,
  (APTR)FindBoardTagList,
  (APTR)GetBoardAttrsTagList,
  (APTR)ReadConfigLong,
  (APTR)ReadConfigWord,
  (APTR)ReadConfigByte,
  (APTR)WriteConfigLong,
  (APTR)WriteConfigWord,
  (APTR)WriteConfigByte,
  (APTR)SetBoardAttrsTagList,
  (APTR)AddIntServer_,
  (APTR)RemIntServer_,
  (APTR)AllocDMABuffer,
  (APTR)FreeDMABuffer,
  (APTR)GetPhysicalAddress,
  (APTR)GetVirtualAddress,
  (APTR)AllocPCIAddressSpace,
  (APTR)FreePCIAddressSpace,
  (APTR)-1
 };

struct MyDataInit                      /* do not change */
{
 UWORD ln_Type_Init;      UWORD ln_Type_Offset;      UWORD ln_Type_Content;
 UBYTE ln_Name_Init;      UBYTE ln_Name_Offset;      ULONG ln_Name_Content;
 UWORD lib_Flags_Init;    UWORD lib_Flags_Offset;    UWORD lib_Flags_Content;
 UWORD lib_Version_Init;  UWORD lib_Version_Offset;  UWORD lib_Version_Content;
 UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
 UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
 ULONG ENDMARK;
} DataTab =
{
        0xe000,8,NT_LIBRARY,
        0x0080,10,(ULONG) &libname[0],
        0xe000,14,LIBF_SUMUSED|LIBF_CHANGED,
        0xd000,20,VERSION,
        0xd000,22,REVISION,
        0x80,24,(ULONG) &libid[0],
        (ULONG) 0
};

struct InitTable                       /* do not change */
{
 ULONG              LibBaseSize;
 APTR              *FunctionTable;
 struct MyDataInit *DataTable;
 APTR               InitLibTable;
} InitTab =
{
 (ULONG)               sizeof(struct PrometheusBase),
 (APTR              *) &FuncTable[0],
 (struct MyDataInit *) &DataTab,
 (APTR)                LibInit
};


/*--------------------------------------------------------------------------*/
/*	 Our Resident struct.                                                   */
/*--------------------------------------------------------------------------*/

const struct Resident ROMTag =     /* do not change */
{
	RTC_MATCHWORD,
	(APTR)&ROMTag,
	&ROMTag.rt_Init + 1,
	RTF_AUTOINIT,
	VERSION,
	NT_LIBRARY,
	0,
	&libname[0],
	&libid[0],
	&InitTab
};

/*--------------------------------------------------------------------------*/
/*   Fake entry.                                                            */
/*--------------------------------------------------------------------------*/

int main (void)
{
    return -1;
}

#endif

/*--------------------------------------------------------------------------*/
/*   Debug stuff.                                                           */
/*--------------------------------------------------------------------------*/

#ifdef TESTEXE
extern struct Library *SysBase;
#define D(x) x
#define kprintf printf
#define CARDDELAY 100000
#else
#ifdef DEBUG
#define D(x) x
#define CARDDELAY 100000

APTR __DRawPutChar(__reg("a6") void *, __reg("d0") UBYTE MyChar)="\tjsr\t-516(a6)";

#define DRawPutChar(MyChar) __DRawPutChar(SysBase, (MyChar))

void DPutChProc(__reg("d0") UBYTE mychar, __reg("a3") APTR PutChData)
{
    struct ExecBase* SysBase = (struct ExecBase*)PutChData;
    DRawPutChar(mychar);
    return;
}

void kprintf(STRPTR format, ...)
{
    if (format)
    {
        struct ExecBase* SysBase = *(struct ExecBase **)4L;
        va_list args;
        va_start(args, format);
        RawDoFmt(format, (APTR)args, &DPutChProc, (APTR)SysBase);
        va_end(args);
    }
    return;
}
#else
void kprintf(const char *,...);
#define D(x)
#define CARDDELAY 10000 /* 10ms enough? */
#endif
#endif

/*****************************************************************************

    NAME */

void arosEnqueue(struct List * list, struct Node * node)

/*  FUNCTION
	Sort a node into a list. The sort-key is the field node->ln_Pri.
	The node will be inserted into the list before the first node
	with lower priority. This creates a FIFO queue for nodes with
	the same priority.

    INPUTS
	list - Insert into this list. The list has to be in descending
		order in respect to the field ln_Pri of all nodes.
	node - This node is to be inserted. Note that this has to
		be a complete node and not a MinNode !

    RESULT
	The new node will be inserted before nodes with lower
	priority.

    NOTES
	The list has to be in descending order in respect to the field
	ln_Pri of all nodes.

    EXAMPLE
	struct List * list;
	struct Node * node;

	node->ln_Pri = 5;

	// Sort the node at the correct place into the list
	Enqueue (list, node);

    BUGS

    SEE ALSO

    INTERNALS

******************************************************************************/
{
    struct Node * next;

    /* Look through the list */
    ForeachNode (list, next)
    {
	/*
	    Look for the first node with a lower pri as the node
	    we have to insert into the list.
	*/
	if (node->ln_Pri > next->ln_Pri)
	    break;
    }
    
    //D(kprintf("nodepri %ld\n", node->ln_Pri));

    /* Insert the node before(!) next. The situation looks like this:

	    A<->next<->B *<-node->*

	ie. next->ln_Pred points to A, A->ln_Succ points to next,
	next->ln_Succ points to B, B->ln_Pred points to next.
	ln_Succ and ln_Pred of node contain illegal pointers.
    */


    /* Let node point to A: A<-node */
    node->ln_Pred	   = next->ln_Pred;

    /* Make node point to next: A<->node->next<->B */
    node->ln_Succ	   = next;

    /* Let A point to node: A->node */
    next->ln_Pred->ln_Succ = node;

    /* Make next point to node: A<->node<-next<->B */
    next->ln_Pred	   = node;

} /* Enqueue */

/*--------------------------------------------------------------------------*/
/* PrmTimeDelay() Used to create a delay between writing to config bits     */
/*--------------------------------------------------------------------------*/

BOOL PrmTimeDelay(struct PrometheusBase *pb, LONG unit, ULONG secs, ULONG microsecs)
{
  struct PortIO {
    struct MsgPort     port;
    struct timerequest treq;
  } *portio;

  LONG ret = TRUE;

  struct Library *SysBase = pb->pb_SysBase;

  if ((portio = (struct PortIO*)AllocMem(sizeof(*portio), MEMF_CLEAR | MEMF_PUBLIC)))
  {
    if ((BYTE)(portio->port.mp_SigBit = AllocSignal(-1)) >= 0)
    {
      portio->port.mp_Node.ln_Type = NT_MSGPORT;
      portio->port.mp_SigTask = FindTask(NULL);
      NewList(&portio->port.mp_MsgList);
      portio->treq.tr_node.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
      portio->treq.tr_node.io_Message.mn_ReplyPort = &portio->port;

      if (!(OpenDevice("timer.device", unit, &portio->treq.tr_node, 0)))
      {
        portio->treq.tr_node.io_Command = TR_ADDREQUEST;
        portio->treq.tr_time.tv_secs = secs;
        portio->treq.tr_time.tv_micro = microsecs;

        if (!(DoIO(&portio->treq.tr_node)))
        {
          ret = FALSE;
        }

        CloseDevice(&portio->treq.tr_node);
      }
      FreeSignal(portio->port.mp_SigBit);
    }
    FreeMem(portio, sizeof(*portio));
  }
  return ret;
}

/*--------------------------------------------------------------------------*/
/* SizeToPri() calculates 2-based logarithm of size.                        */
/*--------------------------------------------------------------------------*/

BYTE SizeToPri(ULONG size)
 {
  BYTE pri;

  for (pri = 0; !(size & 0x00000001); size >>= 1) pri++;
  return pri;
 }

/*--------------------------------------------------------------------------*/
/* AddRequest() adds memory or I/O space request to pb_Requests list.       */
/*--------------------------------------------------------------------------*/

void AddRequest(struct PrometheusBase *pb, struct PCIBus *pcibus, ULONG size, ULONG type, struct TagItem *tag, ULONG volatile *cfgreg, ULONG flag)
 {
  struct Library *SysBase = pb->pb_SysBase;
  struct SpaceReq *srq;
  BYTE Pri = SizeToPri(size);
  
  //D(kprintf("[AddRequest] pb 0x%lx, pbus 0x%lx, size 0x%lx, pri %ld, type %ld, tag 0x%lx, reg 0x%lx\n", pb, pcibus, size, SizeToPri(size), type, tag, cfgreg));
  
  switch (type)
  {
    case BLOCK_MEMORY:
    {
        Pri = -Pri;
        break;
    }
    case BLOCK_GFXMEM:
    {
        Pri = 127;
        break;
    }
    case BLOCK_CFGMEM:
    {
        Pri = 126;
        type = BLOCK_MEMORY;
        break;
    }
  }
  
  if (srq = AllocPooled(pb->pb_MemPool, sizeof(struct SpaceReq)))
  {
    srq->sr_Size = size;
    srq->sr_Type = type;
    srq->sr_Flag = flag;
    srq->sr_Tag = tag;
    srq->sr_CfgAddr = cfgreg;
    srq->sr_Node.ln_Type = 0xFE;
    srq->sr_Node.ln_Name = "PCICard";
    srq->sr_Node.ln_Pri = Pri;


    arosEnqueue(&pcibus->br_CardRequests, &srq->sr_Node);
    //D(kprintf("Jo AR 0x%lx\n", &pcibus->br_CardRequests));
	
  }
  //D(kprintf("AddRequest end\n"));

  return;
 }

/*--------------------------------------------------------------------------*/
/* AddBus() adds PCIBusses to pb_Busses list.       */
/*--------------------------------------------------------------------------*/

struct PCIBus* AddBus(struct PrometheusBase *pb, UBYTE busnr, UBYTE pbusnr)
 {
  struct Library *SysBase = pb->pb_SysBase;
  struct PCIBus *brq;

  D(kprintf("[AddBus] busnr: %ld, parentbusnr: %ld\n", busnr, pbusnr));

  if (brq = AllocPooled(pb->pb_MemPool, sizeof(struct PCIBus)))
  {
    brq->br_BusNr  = busnr;
    brq->br_pBusNr = pbusnr;
    brq->br_hasGraphics = FALSE;
    brq->br_hasUpperIO  = FALSE;
    brq->br_Node.ln_Type = NT_USER;
    brq->br_Node.ln_Pri  = 0;
    brq->br_Node.ln_Name = "PCIBus";
    NewList(&brq->br_CardRequests);
	
    AddTail((struct List*)&pb->pb_Busses, (struct Node*)&brq->br_Node);
  }
  pb->pb_BridgeCnt += 1;

  //D(kprintf("AddBus end %ld\n", pb->pb_BridgeCnt));

  return brq;
 }

/*--------------------------------------------------------------------------*/
/* QueryCard() asks card about memory requests and adds them to pb_Requests */
/* list. It also generates card's taglist.                                  */
/*--------------------------------------------------------------------------*/

void QueryCard (struct PrometheusBase *pb, struct PCIBus *pcibus, volatile struct PciConfig *conf, struct ConfigDev *cdev)
{
  struct Library *SysBase = pb->pb_SysBase;
  UWORD basereg, vendor, device;
  ULONG memsize, memtype, flag;
  UBYTE Class, SubClass;
  ULONG bus = 0, slot = 0;
  WORD tagindex = 0;
  PCIBoard *pcinode;
  BOOL Bridge = FALSE;

  D(kprintf("[QueryCard] 0x%lx\n", conf));

  PrmTimeDelay(pb, 0, 0, CARDDELAY); //Seems like bridges give trouble without this.

  if (conf->pc_Vendor != 0xFFFF)
  {
    if (pcinode = AllocPooled (pb->pb_MemPool, sizeof (PCIBoard)))
    {
      AddTail((struct List*)&pb->pb_Cards, (struct Node*)pcinode);

      /* v2: save pointer to PCI config space for ReadConfigXxxx() and */
      /* WriteConfigXxxx().                                            */

      pcinode->pn_ConfigBase = conf;

      /* NOTE: do not depend on tag order in taglist in custom code! */
      /* It WILL change. Use utility.library taglist interface.      */

      pcinode->pn_TagList[tagindex].ti_Tag = PRM_Vendor;
      CacheClearU();
      pcinode->pn_TagList[tagindex++].ti_Data = vendor = swapw(conf->pc_Vendor) & 0xFFFF;
      pcinode->pn_TagList[tagindex].ti_Tag = PRM_Device;
      CacheClearU();
      pcinode->pn_TagList[tagindex++].ti_Data = device = swapw(conf->pc_Device) & 0xFFFF;
      pcinode->pn_TagList[tagindex].ti_Tag = PRM_Revision;
      CacheClearU();
      pcinode->pn_TagList[tagindex++].ti_Data = conf->pc_Revision;
      pcinode->pn_TagList[tagindex].ti_Tag = PRM_Class;
      CacheClearU();
      pcinode->pn_TagList[tagindex++].ti_Data = Class = conf->pc_Class;
      pcinode->pn_TagList[tagindex].ti_Tag = PRM_SubClass;
      CacheClearU();
      pcinode->pn_TagList[tagindex++].ti_Data = SubClass = conf->pc_SubClass;
      pcinode->pn_TagList[tagindex].ti_Tag = PRM_Interface;
      CacheClearU();
      pcinode->pn_TagList[tagindex++].ti_Data = conf->pc_Interface;
      pcinode->pn_TagList[tagindex].ti_Tag = PRM_HeaderType;
      CacheClearU();
      pcinode->pn_TagList[tagindex++].ti_Data = conf->pc_HeaderType;

      /* up to six base registers */

      if(Class == 0x6 && SubClass == 0x4)
      {
		Bridge = TRUE;
        CacheClearU();
        conf->types.t1.pc_ROM = 0xFEFFFFFF;
        CacheClearU();
        memsize = swapl(conf->types.t1.pc_ROM);
      }
	  else
      {
		pcinode->pn_TagList[tagindex].ti_Tag = PRM_SubsysVendor;
		CacheClearU();
		pcinode->pn_TagList[tagindex++].ti_Data = swapw(conf->types.t0.pc_SybsystemVID);
		pcinode->pn_TagList[tagindex].ti_Tag = PRM_SubsysID;
		CacheClearU();
		pcinode->pn_TagList[tagindex++].ti_Data = swapw(conf->types.t0.pc_SubsystemID);

        for (basereg = 0; basereg < 6; basereg++)
        {
          flag = 0;
          CacheClearU();
          conf->types.t0.pc_BaseRegs[basereg] = 0xFFFFFFFF;
          CacheClearU();
          memsize = swapl(conf->types.t0.pc_BaseRegs[basereg]);

          if (vendor == VID_FREESCALE) //only do BAR0/1 for K1/M1 and BAR0/2 for
          {
            if (basereg == 1)
            {
              if (device == DEVID_MPC834X)
              {
                memsize = PCI_MEMSIZE_64MB;
              }
              else
              {
                memsize = PCI_MEMSIZE_128MB; /* Flash ? */
              }
            }
            else if ((basereg == 2) && (device == DEVID_MPC831X))
            {
              memsize = PCI_MEMSIZE_16MB; /* Boot ROM? */
              flag = 1;
            }
            else if (basereg != 0)
            {
              memsize = -1;
            }
          }
          else if ((vendor == VID_MOTOROLA) && (device == DEVID_POWERPLUSIII))
          {
            if (basereg == 0)
              memsize = PCI_MEMSIZE_4KB;
            else if (basereg == 1)
              memsize = PCI_MEMSIZE_4KB;
            else if (basereg == 3)
              memsize = PCI_MEMSIZE_256KB;
          }
          else if ((vendor == VID_MOTOROLA) && (device == DEVID_MPC107))
          {
            if (basereg == 0)
              memsize = -1;
          }
          if (memsize == 0xFFFFFFFF) memsize = 0;       /* board doesn't respond */

          if (memsize)
          {
            if (memsize & 0x00000001)
            {
              memtype = BLOCK_INOUT;
              memsize = -(memsize & 0xFFFFFFFC);
              D(kprintf("[QueryCard]  io size: 0x%08lx\n", memsize));
            }
            else
            {
              // 3dfx cards have their memory at basereg 1 so map this first for dma access
              if((vendor == VID_3DFX || (vendor == VID_TI && device == DEVID_PERMEDIA2)) && Class == 0x3 && basereg == 1)
				memtype = BLOCK_GFXMEM;
              else if(vendor == VID_ATI && Class == 0x3 && basereg == 0)
				memtype = BLOCK_GFXMEM;
              else if((vendor == VID_TI && device == DEVID_PERMEDIA2) && Class == 0x3 && basereg == 2)
				memtype = BLOCK_GFXMEM;
              else if((vendor == VID_3DFX) && (basereg == 0))
                memtype = BLOCK_CFGMEM;
              else if((vendor == VID_S3) && (basereg == 0) && (device == DEVID_VIRGE3D || device == DEVID_VIRGEDX || device == DEVID_VIRGEGX2))
                memtype = BLOCK_GFXMEM;
              else
                memtype = BLOCK_MEMORY;
              memsize = -(memsize & 0xFFFFFFF0);
              D(kprintf("[QueryCard] mem size: 0x%08lx\n", memsize));
            }            
            pcinode->pn_TagList[tagindex].ti_Tag = PRM_MemorySize0 + basereg;
            pcinode->pn_TagList[tagindex++].ti_Data = memsize;
            pcinode->pn_TagList[tagindex].ti_Tag = PRM_MemoryAddr0 + basereg;
            CacheClearU();
            AddRequest (pb, pcibus, memsize, memtype, &pcinode->pn_TagList[tagindex++], &conf->types.t0.pc_BaseRegs[basereg], flag);
          }
          else
          {
            pcinode->pn_TagList[tagindex++].ti_Tag = PRM_MemorySize0 + basereg;
            pcinode->pn_TagList[tagindex++].ti_Tag = PRM_MemoryAddr0 + basereg;
            CacheClearU();
          }
        }

        /* query board ROM size */
      CacheClearU();
      conf->types.t0.pc_ROM = 0xFEFFFFFF;
      CacheClearU();
      memsize = swapl(conf->types.t0.pc_ROM);

      }
      if (memsize == 0xFFFFFFFF) memsize = 0;         /* board doesn't respond */

      /* add board rom space request */
      if (memsize)
      {
        memtype = BLOCK_MEMORY;
        memsize = -(memsize & 0xFFFFF800);
        pcinode->pn_TagList[tagindex].ti_Tag = PRM_ROM_Size;
        pcinode->pn_TagList[tagindex++].ti_Data = memsize;
        pcinode->pn_TagList[tagindex].ti_Tag = PRM_ROM_Address;
        AddRequest (pb, pcibus, memsize, memtype, &pcinode->pn_TagList[tagindex++], Bridge ? &conf->types.t1.pc_ROM : &conf->types.t0.pc_ROM, 0 );
      }
    }

    /* V2: set up PRM_BoardOwner tag (initialized to NULL) */

    pcinode->pn_TagList[tagindex].ti_Tag = PRM_BoardOwner;
    if (!(Bridge))
    {
        pcinode->pn_TagList[tagindex++].ti_Data = NULL;
    }
    else
    {
        pcinode->pn_TagList[tagindex++].ti_Data = (ULONG)pb;
    }

    /* V2: set up PRM_SlotNumber tag */

	bus = pcibus->br_BusNr;

	if(pb->pb_FireStorm == TRUE)
	{
		if(bus > 0)
		{
			slot = (((ULONG)conf & 0x0000F800) >> 11);
		}
		else
		{
			slot = ((ULONG)conf & 0xF0000) >> 17;
			if(slot == 4) slot = 3;
		}
	}
	else
	{
		slot = ((ULONG)conf & 0xFFFF) >> 13;
	}

	D(kprintf("[QueryCard] conf: 0x%08lx bus: %lx, slot: %lx\n", (ULONG)conf, bus, slot));
	
    pcinode->pn_TagList[tagindex].ti_Tag = PRM_SlotNumber;
    pcinode->pn_TagList[tagindex++].ti_Data = (ULONG)slot;

    /* V2: set up PRM_FunctionNumber tag - currently multifunction devices */
    /* are not supported. New for V2.4 - function number is supported and  */
    /* derived from config area address. */

    pcinode->pn_TagList[tagindex].ti_Tag = PRM_FunctionNumber;
    pcinode->pn_TagList[tagindex++].ti_Data = ((ULONG)conf & 0x00000700) >> 8;
    pcinode->pn_TagList[tagindex].ti_Tag = PRM_BusNumber;
    pcinode->pn_TagList[tagindex++].ti_Data = (ULONG)bus;
    pcinode->pn_TagList[tagindex].ti_Tag = TAG_DONE;

    //D(kprintf("QueryCard end\n"));

    //D(kprintf("tagindex: %lx\n", tagindex));
  }
}

/*--------------------------------------------------------------------------*/
/* WriteAddresses() writes base adresses to all base registers (including    */
/* ROM base registers).                                                     */
/*--------------------------------------------------------------------------*/

void WriteAddresses(struct PrometheusBase *pb, struct ConfigDev *cdev)
{
  struct Library *SysBase = pb->pb_SysBase;
  struct SpaceReq *srq, *srqn;
  struct PCIBus *pbus;
  ULONG io_highaddr = 0;
  ULONG mem_highaddr = 0; //0x100000;
  ULONG mask = ~(-cdev->cd_BoardSize);
  ULONG fs_size_mask = 0;
  UBYTE count;
  ULONG memlimit, iolimit;

  //D(kprintf("WriteAddresses\n"));

  if (pb->pb_FireStorm == FALSE)
  {
    mem_highaddr = (ULONG)cdev->cd_BoardAddr + cdev->cd_BoardSize;
    io_highaddr = (ULONG)cdev->cd_BoardAddr + 0xF0000;
  }
  else io_highaddr = 0x1000;			/* leave some space for hardcoded ISA io addresses */

  ForeachNode(&pb->pb_Busses, pbus)		/* find the graphic card first for dma mem */
  {
    if(pbus->br_hasGraphics == TRUE && (struct PCIBus *)pb->pb_Busses.lh_Head != pbus)
	{
		Remove((struct Node *)pbus);	/* move the bus with graphics card to list head */
		AddHead(&pb->pb_Busses, (struct Node *)pbus);
		break;
	}
  }
  
  /* walk through the found PCI Busses for bridge address window alignment reasons */

  ForeachNode(&pb->pb_Busses, pbus)
  {
      ListLength(&pbus->br_CardRequests, count);
	  D(kprintf("[WriteAddresses] Bus %ld count %ld\n", pbus->br_BusNr, count));
	  
	  if(pbus->br_BusNr > 0)
	  {
		  /* we need to align the first used address for PCI Bridge windows here */
		  if(mem_highaddr & 0x000FFFFF)
		  {
			mem_highaddr &= 0xFFF00000;		// only upper three digits
			mem_highaddr += 0x100000;		// align on next 1MB window
		  }
		  if(io_highaddr & 0x0fff)
		  {
			io_highaddr = (io_highaddr & 0xf000) + 0x1000;
		  }

		  pbus->br_ConfigBase->types.t1.pc_MemoryBase = swapw((mem_highaddr & 0xFFF00000) >> 16);
		  pbus->br_ConfigBase->types.t1.pc_PrefetchMemBase = swapw((mem_highaddr & 0xFFF00000) >> 16);
		  D(kprintf("[WriteAddresses] bus: %lx, Bus MemBase: 0x%08lx\n", pbus->br_BusNr, pbus->br_ConfigBase->types.t1.pc_MemoryBase));

		  pbus->br_ConfigBase->types.t1.pc_IOBase = (io_highaddr >> 12);
		  D(kprintf("[WriteAddresses] bus: %lx, Bus IOBase: 0x%08lx\n", pbus->br_BusNr, pbus->br_ConfigBase->types.t1.pc_IOBase));

		  CacheClearU();
	  }
#if 0
	  for (srq = (struct SpaceReq*)pbus->br_CardRequests.lh_Head;
			srq->sr_Node.ln_Succ;
			srq = (struct SpaceReq*)srq->sr_Node.ln_Succ);
#endif
	  ForeachNodeSafe(&pbus->br_CardRequests, srq, srqn)
	  {
		if (pb->pb_FireStorm == TRUE)
		{
		  /* check if free space is not exhausted */
		  if (mem_highaddr >= 0x1FC00000) break; /* 508 MB */
		  if (io_highaddr >= 0x1fffff) break;

		  fs_size_mask = ~(-srq->sr_Size);

		  switch (srq->sr_Type)
		  {
			  case BLOCK_GFXMEM:
			  case BLOCK_MEMORY:
			  if(( mem_highaddr & fs_size_mask) != 0) mem_highaddr = (mem_highaddr | fs_size_mask) + 1;

			  *srq->sr_CfgAddr = swapl((mem_highaddr | 1) & mask);
			  srq->sr_Tag->ti_Data = (ULONG)cdev->cd_BoardAddr + mem_highaddr;

			  if (srq->sr_Flag == 1) *(srq->sr_CfgAddr + 1) = 0; /* Clear BigFoot Upper (64 bit) Boot ROM address */
			  D(kprintf("[WriteAddresses] mem Pri: %03ld, addr: 0x%08lx, bus: %ld\n", srq->sr_Node.ln_Pri, mem_highaddr, pbus->br_BusNr));
			  mem_highaddr += srq->sr_Size;
			  break;

			  case BLOCK_INOUT:
			  if(( io_highaddr & fs_size_mask) != 0) io_highaddr = (io_highaddr | fs_size_mask) + 1;

			  *srq->sr_CfgAddr = swapl(io_highaddr & mask);
			  srq->sr_Tag->ti_Data = (ULONG)cdev->cd_BoardAddr + FS_PCI_ADDR_IO + io_highaddr;
			  D(kprintf("[WriteAddresses] io  Pri: %03ld, addr: 0x%08lx, bus: %ld\n", srq->sr_Node.ln_Pri, io_highaddr, pbus->br_BusNr));
			  io_highaddr += srq->sr_Size;
			  break;
		  }
		  
		}
		else
		{
		  switch (srq->sr_Type)
		  {
			case BLOCK_GFXMEM:
            case BLOCK_MEMORY:    mem_highaddr -= srq->sr_Size; break;
			case BLOCK_INOUT:     io_highaddr -= srq->sr_Size;  break;
		  }

		  if (mem_highaddr <= (ULONG)cdev->cd_BoardAddr + 0x00100000) break;
		  if (io_highaddr <= (ULONG)cdev->cd_BoardAddr + 0x00010000) break;

		  /* write assigned address to the card ('1' in memory registers enables */
		  /* ROM decoding and has no meaning in ordinary base registers. Write   */
		  /* the address to the taglist also.                                    */

		  switch (srq->sr_Type)
		  {
			  case BLOCK_GFXMEM:
              case BLOCK_MEMORY:
			  *srq->sr_CfgAddr = swapl((mem_highaddr | 1) & mask);
			  srq->sr_Tag->ti_Data = mem_highaddr;
			  D(kprintf("[WriteAddresses] Pri: %03ld, addr: 0x%08lx, bus: %ld\n", srq->sr_Node.ln_Pri, mem_highaddr, pbus->br_BusNr));
			  break;
			  
			  case BLOCK_INOUT:
			  *srq->sr_CfgAddr = swapl(io_highaddr & mask);
			  srq->sr_Tag->ti_Data = io_highaddr;
			  D(kprintf("[WriteAddresses] Pri: %03ld, addr: 0x%08lx, bus: %ld\n", srq->sr_Node.ln_Pri, io_highaddr, pbus->br_BusNr));
			  break;
		  }
		}
		/* calculate new address space top address */
		if(pbus->br_BusNr > 0)
		{
			memlimit = (mem_highaddr + 0x000FFFFF) & 0xFFF00000;
			pbus->br_ConfigBase->types.t1.pc_MemoryLimit = swapw((memlimit >> 16) - 1);
			pbus->br_ConfigBase->types.t1.pc_PrefetchMemLimit = swapw((memlimit >> 16) - 1);
			D(kprintf("[WriteAddresses] bus: %lx, Adjusted Bus MemLimit: 0x%08lx\n", pbus->br_BusNr, pbus->br_ConfigBase->types.t1.pc_MemoryLimit));


			iolimit = (io_highaddr + 0x0FFF) & 0xF000;
			pbus->br_ConfigBase->types.t1.pc_IOLimit = ((iolimit >> 12) - 1);
			D(kprintf("[WriteAddresses] bus: %lx, Adjusted Bus IOLimit: 0x%08lx\n", pbus->br_BusNr, pbus->br_ConfigBase->types.t1.pc_IOLimit));
		}
		CacheClearU();
	  }
  }
  return;
}

UBYTE ScanBus(struct PrometheusBase *pb, struct PCIBus *pBus, struct ConfigDev *cdev)
{
  struct Library *SysBase = pb->pb_SysBase;
  volatile struct PciConfig *pci;
  WORD function, funmax;                    /* introduced in V2.4 */
  WORD headertype, slot;

  APTR cfspace = (APTR)((ULONG)cdev->cd_BoardAddr + FS_PCI_ADDR_CONFIG1 + ((ULONG)(pBus->br_BusNr) << 16));
  UBYTE busdepth = pBus->br_BusNr;
  
  for (slot = 0; slot <= 0x1f; slot++)
  {
	pci = cfspace;
	headertype = pci->pc_HeaderType;

	D(kprintf("[ScanBus] scanning bus slot: %02ld, cfspace: 0x%08lx ht: 0x%02lx v: 0x%04lx d: 0x%04lx\n"
	           ,slot, cfspace, headertype, pci->pc_Vendor, pci->pc_Device));

	/* What todo if we got a PCI-PCI Bridge */
   	if((headertype & 0x7f) == 0x01)
	{
		struct PCIBus *pbusnew;

		D(kprintf("[ScanBus] Found a PCI-PCI Bridge.\n"));

		pci->types.t1.pc_PrimaryBus = pBus->br_pBusNr;				/* we are given the new bus data */
		pci->types.t1.pc_SecondaryBus = pBus->br_BusNr;
		pci->types.t1.pc_SubordinateBus = 0xFF;
		pbusnew = AddBus(pb, pBus->br_BusNr+1, pBus->br_BusNr);
		pbusnew->br_ConfigBase = cfspace;							/* save for WriteAddresses access */

		if((pci->types.t1.pc_IOBase & 0xf) == 0x01)
		{
			pbusnew->br_hasUpperIO = TRUE;
			D(kprintf("[ScanBus] Bridge has upper IO support!\n"));
		}

		busdepth = ScanBus(pb, pbusnew, cdev);
		pci->types.t1.pc_SubordinateBus = busdepth;
		CacheClearU();
	}

	if(pci->pc_Class == 0x3)
	{
		D(kprintf("[ScanBus] Found a graphic card!\n"));
		pBus->br_hasGraphics = TRUE;
	}

	if(pci->pc_Vendor != 0xFFFF && pci->pc_Device != 0xFFFF)
	{
	/* determine if a device is multifunction (bit 7 in pc_HeaderType).        */
	/* 'function' loop counts from 0 to 7 if it is, is performed only once if  */
	/* not.                                                                    */

		if((headertype & 0x80) && (pci->pc_Vendor != swapw(VID_ATI))) /* save 128MB of second aperture */
		  funmax = 8; else funmax = 1;

		for (function = 0; function < funmax; function++)
		{
			QueryCard(pb, pBus, &pci[function<<8], cdev);
			pci[function<<8].pc_LatencyTimer = 0x80;   /* no latency because of Zorro III design (v 2.5) */
			if(((pci->pc_Vendor == swapw(VID_MOTOROLA)) && (pci->pc_Device == swapw(DEVID_MPC107)) && (pci->pc_Revision == 0x13)))
			{
				pci[function<<8].pc_Command = swapw(0x003);     /* do not enable busmaster for MPC107 rev 19*/
			}
			else if(pci->pc_Vendor == swapw(VID_ATI))
			{
				pci[function<<8].pc_LatencyTimer = 0x80;
				pci[function<<8].pc_Command = swapw(0x207);
			}
			else
			{
				pci[function<<8].pc_Command = swapw(0x007);     /* enable I/O, memory space and busmaster */
			}
		}
		CacheClearU();
	}
    cfspace = (APTR)(((ULONG)cfspace + 0x800));
  }

  return busdepth;
}
/*--------------------------------------------------------------------------*/
/* ConfigurePrometheus() configures all cards inserted in Prometheus slots. */
/*--------------------------------------------------------------------------*/

void ConfigurePrometheus(struct PrometheusBase *pb, struct ConfigDev *cdev)
{
  struct Library *SysBase = pb->pb_SysBase;
  WORD headertype, slot;
  WORD function, funmax;                    /* introduced in V2.4 */
  ULONG fs_csreg = 0x10000;                 /* slot configspace offset */
  volatile APTR cfspace;                    /* ptr to cards config space */
  volatile ULONG *fs_cfreg;                 /* ptr to configreg */
  volatile struct PciConfig *pci;
  volatile APTR mfptr;
  struct PCIBus *pbus;
  UBYTE busdepth;

  /* initialize pb_Busses list */

  NewList(&pb->pb_Busses);
  pb->pb_BaseAddr = cdev->cd_BoardAddr;
  
  pbus = AddBus(pb, 0, 0);					/* add 'RootBus' to busses list */

  if (pb->pb_FireStorm == TRUE)
  {
    cfspace = (APTR)((ULONG)cdev->cd_BoardAddr + FS_PCI_ADDR_CONFIG0);
    fs_cfreg = (ULONG*)((ULONG)cfspace + 0x8000);
    *fs_cfreg |= 0x80000000; 				/* disable reset */
    PrmTimeDelay(pb, 0, 0, 5000);
    *fs_cfreg &= 0x7fffffff; 				/* reset */
    PrmTimeDelay(pb, 0, 0, 5000);
    *fs_cfreg |= 0xc0000000; 				/* disable reset, enable ints */
    PrmTimeDelay(pb, 0, 0, 5000);
    cfspace = (APTR)((ULONG)cfspace + fs_csreg);
  }
  else
  {
    cfspace = (APTR)((ULONG)cdev->cd_BoardAddr + 0x000F0000);
  }

  for (slot = 0; slot < 4; slot++)
  {
	busdepth = 0;
	pci = cfspace;
	headertype = pci->pc_HeaderType;

	D(kprintf("[ConfigurePrometheus] scanning bus slot: %ld, cfspace: 0x%08lx ht: 0x%02lx v: 0x%04lx d: 0x%04lx\n"
               , slot, cfspace, headertype, pci->pc_Vendor, pci->pc_Device));

	/* What todo if we got a PCI-PCI Bridge */
	/* bridges only work with e3b upgraded hardware */

	if(((headertype & 0x7f) == 0x01) && (pb->pb_FireStorm == TRUE)) {
		struct PCIBus *pbusnew;
		D(kprintf("[ConfigurePrometheus] Found a PCI-PCI Bridge. BusNr: %ld\n", pb->pb_BridgeCnt));
		pci->types.t1.pc_PrimaryBus = 0x00;
		pci->types.t1.pc_SecondaryBus = pb->pb_BridgeCnt;
		pci->types.t1.pc_SubordinateBus = 0xFF;
		pbusnew = AddBus(pb, pb->pb_BridgeCnt, 0);			/* add first secondary bus */
		pbusnew->br_ConfigBase = cfspace;				/* save for WriteAddresses access */

		D(kprintf("[ConfigurePrometheus] pbusnr: %ld, busnr: %ld\n", pbusnew->br_pBusNr, pbusnew->br_BusNr));
		if((pci->types.t1.pc_IOBase & 0xf) == 0x01)
			pbusnew->br_hasUpperIO = TRUE;
		
		busdepth = ScanBus(pb, pbusnew, cdev);
		
		pci->types.t1.pc_SubordinateBus = busdepth;
		CacheClearU();
	}

	//D(kprintf("[ConfigurePrometheus] pb_BridgeCnt: %ld\n", pb->pb_BridgeCnt));

	if(pci->pc_Class == 0x3)
	{
		D(kprintf("We found a graphic card!\n"));
		pbus->br_hasGraphics = TRUE;
	}

	if(pci->pc_Vendor != 0xFFFF && pci->pc_Device != 0xFFFF) {
		
	/* determine if a device is multifunction (bit 7 in pc_HeaderType).        */
	/* 'function' loop counts from 0 to 7 if it is, is performed only once if  */
	/* not.                                                                    */

	if ((headertype & 0x80) && (pci->pc_Vendor != swapw(VID_ATI))) /* save 128MB of second aperture */
	  funmax = 8; else funmax = 1;

	for (function = 0; function < funmax; function++)
	{
		if (pb->pb_FireStorm == TRUE)
		{
			mfptr = (APTR)(((ULONG)cfspace) + (function<<8));
			pci = mfptr;
			QueryCard(pb, pbus, pci, cdev);
			pci->pc_LatencyTimer = 0x00;          /* no latency because of Zorro III design (v 2.5) */

			if(((pci->pc_Vendor == swapw(VID_MOTOROLA)) && (pci->pc_Device == swapw(DEVID_MPC107)) && (pci->pc_Revision == 0x13)))
			{
				pci->pc_Command = swapw(0x003);      /* do not enable busmaster for MPC107 rev 19*/
			}
			else if(pci->pc_Vendor == swapw(VID_ATI))
			{
				pci->pc_LatencyTimer = 0x80;
				pci->pc_Command = swapw(0x207);
			}
			else
			{
				pci->pc_Command = swapw(0x007);      /* enable I/O, memory space and busmaster */
			}
		}
		else
		{
			QueryCard(pb, pbus, &pci[function], cdev);
			pci[function].pc_Command = 0x0700;      /* enable I/O, memory space and busmaster */
			pci[function].pc_LatencyTimer = 0x00;   /* no latency because of Zorro III design (v 2.5) */
		}
	}
    }
    if (pb->pb_FireStorm == TRUE)
    {
      fs_csreg <<= 1;  // 0x20000, 0x40000, 0x80000
      cfspace = (APTR)(((ULONG)cfspace & 0xfff0ffff) + fs_csreg);
    }
    else
    {
      cfspace = (APTR)((ULONG)cfspace + 0x2000);
    }
  }

  WriteAddresses(pb, cdev);

  cdev->cd_Driver = pb;
  cdev->cd_Flags &= ~CDF_CONFIGME;
  return;
}

#ifdef TESTEXE
int main(void)
{
  struct PrometheusBase *pb;
  struct Library *UtilityBase, *ExpansionBase;
  struct ConfigDev *driver = NULL;
  UBYTE cards = 0;

  if (pb = (struct PrometheusBase*)AllocVec(sizeof (struct PrometheusBase), MEMF_ANY | MEMF_CLEAR))
   {
    pb->pb_SysBase = SysBase;
    pb->pb_Cards.mlh_Head = (struct MinNode*)&pb->pb_Cards.mlh_Tail;
    pb->pb_Cards.mlh_Tail = NULL;
    pb->pb_Cards.mlh_TailPred = (struct MinNode*)&pb->pb_Cards.mlh_Head;
    pb->pb_BridgeCnt = 0;
	printf("TestExe\n");
    if (pb->pb_MemPool = CreatePool(MEMF_ANY | MEMF_CLEAR, 4 * sizeof(PCIBoard), 4 * sizeof(PCIBoard)))
     {
      if (UtilityBase = OpenLibrary ("utility.library", 36))
       {
		printf("TestExe\n");
        pb->pb_UtilityBase = UtilityBase;
        if (ExpansionBase = OpenLibrary ("expansion.library", 36))
         {
		printf("TestExe\n");

           while (driver = FindConfigDev(driver, 0xAD47, 1))     // find Matay board
           {
            pb->pb_FireStorm = FALSE;
            ConfigurePrometheus (pb, driver);
           }
           while (driver = FindConfigDev(driver, 0x0e3b, 0xc8))  // find e3b Firestorm board
           {
            pb->pb_FireStorm = TRUE;
            ConfigurePrometheus (pb, driver);
           }
          CloseLibrary (ExpansionBase);
         }
       }
     }
   }
}

#else
/*-------------------------------------------------------------------------*/
/* INIT                                                                    */
/*-------------------------------------------------------------------------*/

struct PrometheusBase *LibInit(__reg("d0") struct PrometheusBase *pb, __reg("a0") APTR seglist, __reg("a6") struct Library *sysb)
{
  struct PrometheusBase *rval = NULL;
  struct ExecBase *SysBase = (struct ExecBase*)sysb;
  struct Library *UtilityBase, *ExpansionBase;
  struct ConfigDev *driver = NULL;
  UBYTE cards = 0;

  if (!(SysBase->AttnFlags & AFF_68020))
      {
        FreeMem ((APTR)((ULONG)pb - (ULONG)pb->pb_Lib.lib_NegSize),
        (LONG)pb->pb_Lib.lib_PosSize + (LONG)pb->pb_Lib.lib_NegSize);
        return FALSE;
      }

  pb->pb_Lib.lib_OpenCnt = 1;
  pb->pb_SegList = seglist;
  pb->pb_SysBase = sysb;
  pb->pb_DMASuppBase = NULL;
  pb->pb_Cards.mlh_Head = (struct MinNode*)&pb->pb_Cards.mlh_Tail;
  pb->pb_Cards.mlh_Tail = NULL;
  pb->pb_Cards.mlh_TailPred = (struct MinNode*)&pb->pb_Cards.mlh_Head;
  pb->pb_BridgeCnt = 0;

  if (pb->pb_MemPool = CreatePool(MEMF_ANY | MEMF_CLEAR, 4 * sizeof(PCIBoard), 4 * sizeof(PCIBoard)))
   {
    if (UtilityBase = OpenLibrary ("utility.library", 36))
     {
      pb->pb_UtilityBase = UtilityBase;
      if (ExpansionBase = OpenLibrary ("expansion.library", 36))
       {
         while (driver = FindConfigDev(driver, 0xAD47, 1))     // find Matay board
         {
          pb->pb_FireStorm = FALSE;
          ConfigurePrometheus (pb, driver);
         }
         while (driver = FindConfigDev(driver, 0x0e3b, 0xc8))  // find e3b Firestorm board
         {
          pb->pb_FireStorm = TRUE;
          ConfigurePrometheus (pb, driver);
         }
        rval = pb;
        CloseLibrary (ExpansionBase);
       }
      if (!rval)
       {
        CloseLibrary (UtilityBase);
        pb->pb_UtilityBase = NULL;
       }
     }
   }
  return rval;
}


/*-------------------------------------------------------------------------*/
/* OPEN                                                                    */
/*-------------------------------------------------------------------------*/

struct PrometheusBase *LibOpen ( __reg("a6") struct PrometheusBase *pb)
{
  struct PrometheusBase *ret = pb;
  
  pb->pb_Lib.lib_OpenCnt++;
  pb->pb_Lib.lib_Flags &= ~LIBF_DELEXP;
  return ret;
}

/*-------------------------------------------------------------------------*/
/* CLOSE                                                                   */
/*-------------------------------------------------------------------------*/

long LibClose ( __reg("a6") struct PrometheusBase *pb)
{
  if (!(--pb->pb_Lib.lib_OpenCnt))
   {
    if (pb->pb_Lib.lib_Flags & LIBF_DELEXP) return ((long)LibExpunge (pb));
   }
  return 0;
}

/*-------------------------------------------------------------------------*/
/* EXPUNGE                                                                 */
/*-------------------------------------------------------------------------*/

void *LibExpunge ( __reg("a6") struct PrometheusBase *pb)
{
  APTR seglist;
  struct Library *SysBase = pb->pb_SysBase;

  if (pb->pb_Lib.lib_OpenCnt)
   {
    pb->pb_Lib.lib_Flags |= LIBF_DELEXP;
    return 0;
   }
  Remove ((struct Node*)pb);
  if (pb->pb_MemPool) DeletePool (pb->pb_MemPool);
  if (pb->pb_UtilityBase) CloseLibrary (pb->pb_UtilityBase);
  if (pb->pb_DMASuppBase) CloseLibrary (pb->pb_DMASuppBase);
  seglist = pb->pb_SegList;
  FreeMem ((APTR)((ULONG)pb - pb->pb_Lib.lib_NegSize), (LONG)pb->pb_Lib.lib_PosSize + (LONG)pb->pb_Lib.lib_NegSize);
  return seglist;
}

/*-------------------------------------------------------------------------*/
/* RESERVED                                                                */
/*-------------------------------------------------------------------------*/

long LibReserved (void)
{
  return 0;
}

/****** prometheus.library/Prm_FindBoardTagList *****************************
*
*   NAME
*       Prm_FindBoardTagList -- Finds PCI board with attributes given in the
*               taglist.
*       Prm_FindBoardTags -- Varargs stub for Prm_FindBoardTagList.
*
*   SYNOPSIS
*       Board = Prm_FindBoardTagList (Previous, TagList)
*                                     A0        A1
*
*       PCIBoard *Prm_FindBoardTagList (PCIBoard*, struct TagItem*);
*
*       Board = Prm_FindBoardTags (Previous, Tag1, ...)
*
*       PCIBoard *Prm_FindBoardTags (PCIBoard*, Tag, ...);
*
*   FUNCTION
*       Finds all PCI boards connected to any Prometheus PCI bridge in the
*       system, matching the tags in the TagList. First call returns first
*       matching board found. To find all the matching boards call the
*       function in loop until it returns NULL.
*
*   INPUTS
*       Previous  - pointer to a "blackbox" PCI board structure. The function
*                   will search board list from the *next* board after
*                   Previous. NULL value means "start search from the
*                   beginning of internal list".
*       TagList   - list of tags all boards will be checked against.
*                   Empty taglist (TagList points to LONG 0), or NULL
*                   here means all the boards will be returned in turn.
*
*   RESULT
*       Board     - a pointer to a "blackbox" PCI board structure. Don't try
*                   to peeking and pokeing it, use only as a parameter in
*                   prometheus.library calls. Function can return NULL if no
*                   [more] matching boards can be found.
*
*   EXAMPLE
*       Let's check if any Voodoo3 2000 board is plugged in:
*
*       PCIBoard *board = NULL;
*
*       while (board = Prm_FindBoardTags (board, PRM_Vendor,
*        0x121A, PRM_Device, 5, TAG_END))
*        {
*         \* do something with 'board' *\
*        }
*
*   NOTES
*       Don't give random values as Previous board. It should be only pointer
*       returned by Prm_FindBoardTagList() call or NULL.
*
*   BUGS
*
*   SEE ALSO
*       Prm_GetBoardAttrsTagList()
*
*****************************************************************************
*
*/

PCIBoard *FindBoardTagList ( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *node, __reg("a1") struct TagItem *taglist)
{
   struct TagItem *tag, *tagbase, *tagptr;
   struct Library *UtilityBase = pb->pb_UtilityBase;

   if (!node) node = (PCIBoard*)pb->pb_Cards.mlh_Head;
   else node = (PCIBoard*)node->pn_Node.mln_Succ;

   while (node->pn_Node.mln_Succ)
    {
     /* NULL taglist? No filtering, just return next card. */

     if (!(tagptr = taglist)) return node;

     while (tag = NextTagItem(&tagptr))
      {
       if (tagbase = FindTagItem (tag->ti_Tag, node->pn_TagList))
         if (tagbase->ti_Data != tag->ti_Data) break;
      }
     if (!tag) return node;
     node = (PCIBoard*)node->pn_Node.mln_Succ;
    }
   return NULL;
}


/****** prometheus.library/Prm_GetBoardAttrsTagList *************************
*
*   NAME
*       Prm_GetBoardAttrsTagList -- reads information about PCI board.
*       Prm_GetBoardAttrsTags -- Varargs stub for Prm_GetBoardAttrsTagList.
*
*   SYNOPSIS
*       TagsRead = Prm_GetBoardAttrsTagList (Board, TagList)
*                                 A0     A1
*
*       ULONG Prm_GetBoardAttrsTagList (PCIBoard*, struct TagItem*);
*
*       TagsRead = Prm_GetBoardAttrsTags (Board, Tag1, ...)
*
*       ULONG Prm_GetBoardAttrsTags (PCIBoard*, Tag, ...);
*
*   FUNCTION
*       Reads information from board internal structure and writes it
*       according to given taglist. Function looks for every passed tag value
*       in database and if found writes its value at address given in ti_Data
*       in passed TagList.
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList(). It can be NULL, function returns
*                 immediately in the case, no data is written. Don't pass
*                 random values here.
*       TagList - Function will search in internal database for every ti_Tag
*                 in the TagList. If tag is found, ti_Data field is used as a
*                 pointer to ULONG and the information is written at the
*                 address. Unrecognized tags are skipped and 0 is written to
*                 the given ti_Data location. TagList can be NULL,
*                 function returns immediately at the case, no data are
*                 written. If ti_Data field of any tag is NULL, no data is
*                 written for this tag. Following tags are recognized:
*
*       PRM_Vendor      - Board vendor number assigned by PCISIG.
*       PRM_Device      - Device number assigned by manufacturer.
*       PRM_Revision    - Device revision number assigned by manufacturer.
*       PRM_Class       - Device class as defined in PCI specification.
*       PRM_SubClass    - Device subclass as defined in PCI specification.
*       PRM_MemoryAddrX - (where X can be from '0' to '5'), PCI board can
*           have up to six memory blocks allocated. These tags contains base
*           adresses of blocks assigned by prometheus.library in init code.
*           NULL value means a block is not used.  You can depend on fact
*           that the last nybble of PRM_MemoryAddrX is equal to X. It is
*           guarranted in future releases. So the code below is valid:
*
*       \* read all possible base adresses *\
*
*       ULONG baseaddr;
*
*       for (i = 0; i < 6; i++)
*        {
*         Prm_GetBoardAttrsTags (any_board, PRM_MemoryAddr0 + i,
*          (ULONG)&baseaddr, TAG_END);
*        }
*
*       PRM_MemorySizeX - (where X can be from '0' to '5'), PCI board can
*           have up to six memory blocks allocated. These tags contains sizes
*           of blocks assigned by prometheus.library in init code. NULL value
*           means a block is not used.  You can depend on fact that the last
*           nybble of PRM_MemorySizeX is equal to X. It is guarranted in
*           future releases (see code above).
*       PRM_ROM_Address - Address of PCI on-board ROM (if found).
*       PRM_ROM_Size - Size of PCI on-board ROM (if found).
*       PRM_BoardOwner (V2) - an address of driver which claimed the board.
*           Typically it points to struct Node, and driver name can be read
*           from ln_Name field.
*       PRM_SlotNumber (V2) - number of the physical PCI slot the board is
*           inserted in. Range from 0 to 3 for primary PCI bus.
*       PRM_FunctionNumber (V2) - number of device function. For single-
*           -function devices always 0, for multifunction from 0 to 7. Every
*           function of a multifunction board is reported by prometheus.library
*           as a separate "board".
*
*   RESULT
*       TagsRead - Number of succesfully read tags.
*
*   EXAMPLE
*       Get first base address and block size of the card:
*
*       APTR baseaddress = NULL;
*       ULONG blocksize = 0;
*
*       Prm_GetBoardAttrsTags (any_board,
*         PRM_MemoryAddr0, (ULONG)&baseaddress,
*         PRM_MemorySize0, (ULONG)&blocksize,
*         TAG_END);
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_SetBoardAttrsTagList()
*
*****************************************************************************
*
*/

ULONG GetBoardAttrsTagList ( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("a1") struct TagItem *taglist)
 {
  struct TagItem *tagptr, *tag, *tagbase;
  struct Library *UtilityBase = pb->pb_UtilityBase;
  ULONG cnt = 0;

  if (board && taglist)
   {
    tagptr = taglist;
    while (tag = NextTagItem (&tagptr))
     {
      if (tag->ti_Data)
       {
        if (tagbase = FindTagItem (tag->ti_Tag, board->pn_TagList))
         {
          *(ULONG*)tag->ti_Data = tagbase->ti_Data;
          cnt++;
         }
        else *(ULONG*)tag->ti_Data = 0;
       }
     }
   }
  return cnt;
 }

/****** prometheus.library/Prm_ReadConfigLong *******************************
*
*   NAME
*       Prm_ReadConfigLong -- reads a longword (32 bits) from config
*                             space of given PCI board. (V2)
*
*   SYNOPSIS
*       Value = Prm_ReadConfigLong (Board, Offset)
*                                   A0     D0:8
*
*       ULONG Prm_ReadConfigLong (PCIBoard*, UBYTE);
*
*   FUNCTION
*       This function allows for direct reading of PCI board config space.
*       Takes care of data endianness, data are always returned as big
*       endian.
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Offset  - Config space offset in bytes to read from. Offset will be
*                 rounded down to 4 bytes boundary.
*
*   RESULT
*       Value   - Longword read from PCI board.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_ReadConfigWord(), Prm_ReadConfigByte(), Prm_WriteConfigLong()
*
*****************************************************************************
*
*/

ULONG ReadConfigLong( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE offset)
{
//    volatile ULONG *cfgl = (ULONG*)board->pn_ConfigBase;
	ULONG cfgl = (ULONG)board->pn_ConfigBase;
    struct Library *SysBase = pb->pb_SysBase;
    ULONG tmp;

    CacheClearU();
    //return (swapl(cfgl[offset >> 2]));
    tmp = swapl( *(ULONG*)(cfgl+(ULONG)offset));
	D(kprintf("Prm_ReadConfigLong offset 0x%lx result 0x%08lx\n", offset, tmp));
	return tmp;
}

/****** prometheus.library/Prm_ReadConfigWord *******************************
*
*   NAME
*       Prm_ReadConfigWord -- reads a word (16 bits) from config space of
*                             given PCI board. (V2)
*
*   SYNOPSIS
*       Value = Prm_ReadConfigWord (Board, Offset)
*                                   A0     D0:8
*
*       UWORD Prm_ReadConfigWord (PCIBoard*, UBYTE);
*
*   FUNCTION
*       This function allows for direct reading of PCI board config space.
*       Takes care of data endianness, data are always returned as big
*       endian. For example if VendorID is $5678 and DeviceID is $1234 then:
*       Prm_ReadConfigWord(board, 0) returns $1234,
*       Prm_ReadConfigWord(board, 2) returns $5678,
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Offset  - Config space offset in bytes to read from. Offset will be
*                 rounded down to 2 bytes boundary.
*
*   RESULT
*       Value   - Word read from PCI board.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_ReadConfigLong(), Prm_ReadConfigByte(), Prm_WriteConfigWord()
*
*****************************************************************************
*
*/

UWORD ReadConfigWord( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE offset)
{
//    volatile UWORD *cfgw = (UWORD*)board->pn_ConfigBase;
	ULONG cfgw = (ULONG)board->pn_ConfigBase;
    struct Library *SysBase = pb->pb_SysBase;
	UWORD tmp;
	
    CacheClearU();
    //return (swapw(cfgw[(offset >> 1) ^ 1]));
    tmp = swapw( *(UWORD*)(cfgw+(ULONG)offset));
	D(kprintf("Prm_ReadConfigWord offset 0x%lx result 0x%04lx\n", offset, tmp));
    return tmp;
}

/****** prometheus.library/Prm_ReadConfigByte *******************************
*
*   NAME
*       Prm_ReadConfigByte -- reads a byte (8 bits) from config space of
*                             given PCI board. (V2)
*
*   SYNOPSIS
*       Value = Prm_ReadConfigByte (Board, Offset)
*                                   A0     D0:8
*
*       UBYTE Prm_ReadConfigByte (PCIBoard*, UBYTE);
*
*   FUNCTION
*       This function allows for direct reading of PCI board config space.
*       Takes care of data endianness. Data are always returned as big
*       endian. For example if VendorID is $5678 and DeviceID is $1234 then:
*       Prm_ReadConfigByte(board, 0) returns $12,
*       Prm_ReadConfigByte(board, 1) returns $34,
*       Prm_ReadConfigByte(board, 2) returns $56,
*       Prm_ReadConfigByte(board, 3) returns $78,
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Offset  - Config space offset in bytes to read from.
*
*   RESULT
*       Value   - Value read from PCI board.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_ReadConfigLong(), Prm_ReadConfigWord(), Prm_WriteConfigByte()
*
*****************************************************************************
*
*/

UBYTE ReadConfigByte( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE offset)
{
    volatile UBYTE *cfgb = (UBYTE*)board->pn_ConfigBase;
    struct Library *SysBase = pb->pb_SysBase;
    UBYTE tmp;

    CacheClearU();
    //return (cfgb[offset ^ 3]);
    tmp = cfgb[offset];
	D(kprintf("Prm_ReadConfigByte offset 0x%lx result 0x%02lx\n", offset, tmp));
    return tmp;
}

/****** prometheus.library/Prm_WriteConfigLong ******************************
*
*   NAME
*       Prm_WriteConfigLong -- writes a longword (32 bits) to config space of
*                              given PCI board. (V2)
*
*   SYNOPSIS
*       Prm_WriteConfigLong (Board, Data, Offset)
*                            A0     D0    D1:8
*
*       VOID Prm_WriteConfigLong (PCIBoard*, ULONG, UBYTE);
*
*   FUNCTION
*       This function allows for direct writing to PCI board config space.
*       Takes care of data endianness, you should specify data in big-endian
*       mode.
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Data    - Data to be written to PCI board.
*       Offset  - Config space offset in bytes to write to. Offset will be
*                 rounded down to nearest 4 byte boundary.
*
*   RESULT
*       None.
*
*   NOTES
*       Offsets from 0 to $40 are configured at boot time and must not be
*       changed by any application.
*
*   BUGS
*
*   SEE ALSO
*       Prm_WriteConfigWord(), Prm_WriteConfigByte(), Prm_ReadConfigLong()
*
*****************************************************************************
*
*/

void WriteConfigLong( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") ULONG data, __reg("d1") UBYTE offset)
{
//    volatile ULONG *cfgl = (ULONG*)board->pn_ConfigBase;
	ULONG cfgl = (ULONG)board->pn_ConfigBase;
    struct Library *SysBase = pb->pb_SysBase;

	D(kprintf("Prm_WriteConfigLong offset 0x%lx data 0x%08lx\n", offset, data));

//    cfgl[offset >> 2] = swapl(data);
	*(ULONG*)(cfgl+(ULONG)offset) = swapl(data);
    CacheClearU();
    return;
}

/****** prometheus.library/Prm_WriteConfigWord ******************************
*
*   NAME
*       Prm_WriteConfigWord -- writes a word (16 bits) to config space of
*                              given PCI board. (V2)
*
*   SYNOPSIS
*       Prm_WriteConfigWord (Board, Data, Offset)
*                            A0     D0:16 D1:8
*
*       VOID Prm_WriteConfigWord (PCIBoard*, UWORD, UBYTE);
*
*   FUNCTION
*       This function allows for direct writing to PCI board config space.
*       Takes care of data endianness, you should specify data in big-endian
*       mode. Words will be writen as follows:
*
*              offset 0                 offset 2
*
*       bits 15-8    bits 7-0     bits 15-8    bits 7-0
*
*                  configuration longword
*
*       bits 31-24   bits 23-16   bits 15-8    bits 7-0
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Data    - Data to be written to PCI board.
*       Offset  - Config space offset in bytes to write to. Offset will be
*                 rounded down to nearest 2 byte boundary.
*
*   RESULT
*       None.
*
*   NOTES
*       Offsets from 0 to $40 are configured at boot time and must not be
*       changed by any application.
*
*   BUGS
*
*   SEE ALSO
*       Prm_WriteConfigLong(), Prm_WriteConfigByte(), Prm_ReadConfigWord()
*
*****************************************************************************
*
*/

void WriteConfigWord(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UWORD data, __reg("d1") UBYTE offset)
{
//    volatile UWORD *cfgw = (UWORD*)board->pn_ConfigBase;
	ULONG cfgw = (ULONG)board->pn_ConfigBase;
    struct Library *SysBase = pb->pb_SysBase;

	D(kprintf("Prm_WriteConfigWord offset 0x%lx data 0x%04lx\n", offset, data));

//    cfgw[(offset >> 1) ^ 1] = swapw(data);
    *(UWORD*)(cfgw+(ULONG)offset) = swapw(data);
    CacheClearU();
    return;
}

/****** prometheus.library/Prm_WriteConfigByte **************************
*
*   NAME
*       Prm_WriteConfigByte -- writes a byte (8 bits) to config space of
*                              given PCI board. (V2)
*
*   SYNOPSIS
*       Prm_WriteConfigByte (Board, Data, Offset)
*                            A0     D0:8  D1:8
*
*       VOID Prm_WriteConfigWord (PCIBoard*, UBYTE, UBYTE);
*
*   FUNCTION
*       This function allows for direct writing to PCI board config space.
*       Takes care of data endianness, you should specify data in big-endian
*       mode. Words will be writen as follows:
*
*       offset 0     offset 1     offset 2     offset 3
*
*       bits 7-0     bits 7-0     bits 7-0     bits 7-0
*
*                  configuration longword
*
*       bits 31-24   bits 23-16   bits 15-8    bits 7-0
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Data    - Data to be written to PCI board.
*       Offset  - Config space offset in bytes to write to.
*
*   RESULT
*       None.
*
*   NOTES
*       Offsets from 0 to $40 are configured at boot time and must not be
*       changed by any application.
*
*   BUGS
*
*   SEE ALSO
*       Prm_WriteConfigWord(), Prm_WriteConfigLong(), Prm_ReadConfigByte()
*
*****************************************************************************
*
*/

void WriteConfigByte( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") UBYTE data, __reg("d1") UBYTE offset)
{
    volatile UBYTE *cfgb = (UBYTE*)board->pn_ConfigBase;
    struct Library *SysBase = pb->pb_SysBase;

	D(kprintf("Prm_WriteConfigByte offset 0x%lx data 0x%02lx\n", offset, data));

//    cfgb[offset ^ 3] = data;
	cfgb[offset] = data;
    CacheClearU();
    return;
}

/****** prometheus.library/Prm_SetBoardAttrsTagList *************************
*
*   NAME
*       Prm_SetBoardAttrsTagList -- Sets a PCI board attributes. (V2)
*       Prm_SetBoardAttrsTags -- Varargs stub for Prm_SetBoardAttrsTagList.
*
*   SYNOPSIS
*       Attrs = Prm_SetBoardAttrsTagList (Board, TagList)
*                                         A0     A1
*
*       ULONG Prm_SetBoardAttrsTagList (PCIBoard*, struct TagItem*);
*
*       Attrs = Prm_SetBoardAttrsTags (Board, Tag1, ...)
*
*       ULONG Prm_SetBoardAttrsTags (PCIBoard*, Tag, ...);
*
*   FUNCTION
*       Sets an attribute of a PCI board. Note that not all attributes are
*       settable (this is defined in 'libraries/prometheus.h' include file).
*       Function looks for every passed tag value in TagList and if found in
*       the data base and writing the attribute is permitted new value is
*       recorded in the database.
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList(). It can be NULL, function returns
*                 immediately in the case, no data is written, function
*                 returns 0. Don't pass random values here.
*       TagList - Function will search in internal database for every ti_Tag
*                 in the TagList. If tag is found, ti_Data field is used as a
*                 new value in the database if setting the attibute is
*                 permitted. Unrecognized tags are skipped. TagList can be
*                 NULL, function returns immediately at the case, no data are
*                 changed, function returns FALSE. Following tags are
*                 recognized and allowed for setting:
*
*           PRM_BoardOwner - Contains an address of driver which claimed the
*                 board. Only device drivers should set it. If this attribute
*                 was NULL before setting, it will be set succesfully and the
*                 function returns TRUE. If the attribute was set already to
*                 non NULL value by someone else and the new value is non
*                 NULL (setting the attribute to NULL means "unlocking"
*                 hardware) the function fails, it means old value stays
*                 unchanged and the function returns FALSE. This way a driver
*                 can "lock" its hardware. You can always set this attribute
*                 to an address of your Library or Device structure. If your
*                 driver is neither library nor device (which is strongly
*                 discouraged) set the attribute to a Node structure with
*                 valid ln_Name field.
*
*   EXAMPLE
*       Try to acquire hardware inside the driver:
*
*       if (Prm_SetBoardAttrsTags(board,
*         PRM_BoardOwner, (LONG)MyDeviceBase,
*         TAG_END))
*         {
*           \* proceed with the hardware *\
*         }
*       else
*         {
*           \* somebody already claimed the hadrware *\
*           \* inform the user and quit              *\
*         }
*
*   RESULT
*       Number of attributes succesfully set. In current (V2) library it can
*       be 0 or 1 (for PRM_BoardOwner).
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_GetBoardAttrsTagList()
*
*****************************************************************************
*
*/

const LONG SettableTags[] = {PRM_BoardOwner, TAG_END};

LONG SetBoardAttrsTagList ( __reg("a6") struct PrometheusBase *pb, PCIBoard __reg("a0") *board, __reg("a1") struct TagItem *taglist)
  {
    LONG attr_count = 0;
    struct TagItem *item, *dbasetag, *tagptr = taglist;
    struct Library *UtilityBase = pb->pb_UtilityBase;

    if (board)
      {
        if (taglist)
          {
            while (item = NextTagItem(&tagptr))
              {
                if (TagInArray(item->ti_Tag, (Tag*)SettableTags))
                  {
                    if (dbasetag = FindTagItem(PRM_BoardOwner, board->pn_TagList))
                      {
                        switch (item->ti_Tag)
                          {
                            case PRM_BoardOwner:
                              if ((dbasetag->ti_Data == NULL) || (item->ti_Data == NULL))
                                {
                                  dbasetag->ti_Data = item->ti_Data;
                                  attr_count++;
                                }
                            break;
                         }
                      }
                  }
              }
          }
      }
    return attr_count;
  }

/****** prometheus.library/Prm_AddIntServer *********************************
*
*   NAME
*       Prm_AddIntServer -- Adds PCI interrupt service routine to the
*                           system. (V2)
*
*   SYNOPSIS
*       Success = Prm_AddIntServer (Board, Interrupt)
*                                   A0     A1
*
*       BOOL Prm_AddIntServer (PCIBoard*, struct Interrupt*);
*
*   FUNCTION
*       Adds an interrupt server to a system interrupt server chain
*       responsible for interrupts generated by PCI boards in Prometheus.
*       With current hardware all these interrupt servers will be added
*       to INTB_PORTS chain, but this will change with G4 board.
*
*   INPUTS
*       Board    -  "Blackbox" structure pointer returned by
*                   Prm_FindBoardTagList(). It can be NULL, function returns
*                   immediately in the case with FALSE value. Don't pass
*                   random values here.
*       Interrupt-  Properly initialized Interrupt structure, exactly like
*                   for exec AddIntServer() call. Interrupt code have to take
*                   into account that PCI interrupts are shared, that means
*                   every interrupt server should check if interrupt was
*                   generated by 'its' board or another one. If int server
*                   claims the interrupt it returns 1, zero otherwise.
*
*   RESULT
*       TRUE if interrupt was successfully installed, FALSE otherwise.
*
*   NOTES
*       Current exec.library requires that interrupt server claiming an
*       interrupt exits with zero CPU flag cleared. Server should set
*       the flag if this interrupt was not handled by it. You have to ensure
*       CPU flags are set properly at server routine exit. It can be done
*       using __interrupt attribute for server routine, which cause setting
*       processor flags according to return value:
*
*       __interrupt IntCode(userdata reg(a1))
*         {
*           if (check_int())
*             {
*               \* handle the interrupt *\
*               return (1);
*             }
*           else return (0);
*         }
*
*      If your compiler does not support __interrupt attribute you have to
*      use assembler stub:
*
*      _IntStub:  LEA     _IntCode,a0
*                 JSR     (a0)
*                 TST.L   d0        ;sets CPU flags according to return value
*                 RTS
*
*       and put IntStub() into is_Code field of struct Interrupt.
*
*   BUGS
*
*   SEE ALSO
*       Prm_RemIntServer(), exec.library/AddIntServer()
*
*****************************************************************************
*
*/

BOOL AddIntServer_( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("a1") struct Interrupt *intr)
  {
    struct Library *UtilityBase = pb->pb_UtilityBase;
    struct Library *SysBase = pb->pb_SysBase;

	D(kprintf("Prm_AddIntServer\n"));

    if (!board) return FALSE;

    AddIntServer(INTB_PORTS, intr);
    return TRUE;
  }

/****** prometheus.library/Prm_RemIntServer *********************************
*
*   NAME
*       Prm_RemIntServer -- Removes PCI interrupt service routine from the
*                           system. (V2)
*
*   SYNOPSIS
*       Prm_RemIntServer (Board, Interrupt)
*                         A0     A1
*
*       VOID Prm_RemIntServer (PCIBoard*, struct Interrupt*);
*
*   FUNCTION
*       Removes a PCI interrupt service routine previously added by
*       Prm_AddIntServer() call.
*
*   INPUTS
*       Board    -  "Blackbox" structure pointer returned by
*                   Prm_FindBoardTagList(). It can be NULL, function returns
*                   immediately in the case. Don't pass random values here.
*       Interrupt - Interrupt structure given as a parameter for
*                   Prm_AddIntServer().
*
*   RESULT
*       None.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_AddIntServer()
*
*****************************************************************************
*
*/

void RemIntServer_(__reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("a1") struct Interrupt *intr)
  {
    struct Library *SysBase = pb->pb_SysBase;

	D(kprintf("Prm_RemIntServer\n"));

    if (!board) return;
    if (intr) RemIntServer(INTB_PORTS, intr);
    return;
  }

/****** prometheus.library/Prm_AllocDMABuffer *******************************
*
*   NAME
*       Prm_AllocDMABuffer -- Allocates memory buffer DMA-able on PCI bus.
*                             (V2)
*
*   SYNOPSIS
*       Buffer = Prm_AllocDMABuffer (Size)
*                                    D0
*
*       APTR Prm_AllocDMABuffer (ULONG);
*
*   FUNCTION
*       Allocates a memory buffer accessible for PCI boards using DMA in
*       busmaster mode to fetch and store data.
*
*   INPUTS
*       Size    -  Size of requested memory block in bytes. It will be
*                  rounded up to nearest longword boundary. Returned block
*                  is always longword aligned. Zero size is an no-op with
*                  NULL pointer returned.
*   RESULT
*       Buffer  -  Address of memory block allocated or NULL if there is no
*                  more free DMA-able memory. Note well - this is logical
*                  address as seen by CPU. To obtain physical PCI bus address
*                  (used e.g. when programming board's DMA controllers) use
*                  Prm_GetPhysicalAddress() function.
*
*   NOTES
*       Function must not be called from an interrupt.
*
*   BUGS
*
*   SEE ALSO
*       Prm_FreeDMABuffer(), Prm_GetPhysicalAddress()
*
*****************************************************************************
*
*/

APTR AllocDMABuffer(__reg("a6") struct PrometheusBase *pb, __reg("d0") ULONG size)
  {
    struct Library *SysBase = pb->pb_SysBase;
    struct Library *CardBase;
	APTR mem;

    D(kprintf("Prm_AllocDMABuffer size 0x%08lx\n", size));

      if (!pb->pb_DMASuppBase)
      {
        pb->pb_DMASuppBase = OpenLibrary(CARD_NAME, 7);
      }

      if (CardBase = pb->pb_DMASuppBase)
      {
	    mem = (APTR)AllocDMAMem(size);
  	    D(kprintf("Prm_AllocDMABuffer ptr 0x%08lx\n", mem));
        return mem;
      }
    return NULL;
  }

/****** prometheus.library/Prm_FreeDMABuffer ********************************
*
*   NAME
*       Prm_FreeDMABuffer -- Frees DMA-able memory buffer. (V2)
*
*   SYNOPSIS
*       Prm_FreeDMABuffer (Buffer, Size)
*                          A0      D0
*
*       VOID Prm_FreeDMABuffer (APTR, ULONG);
*
*   FUNCTION
*       Frees a memory buffer allocated by Prm_AllocDMABuffer() call.
*
*   INPUTS
*       Buffer  -  Address of allocated memory block as returned by
*                  Prm_AllocDMABuffer(). NULL address is a no-op.
*       Size    -  Size of allocated block. Must match size given for
*                  Prm_AllocDMABuffer(). Zero size is a no-op.
*   RESULT
*       None.
*
*   NOTES
*       Function must not be called from an interrupt.
*
*   BUGS
*
*   SEE ALSO
*       Prm_AllocDMABuffer(), Prm_GetPhysicalAddress()
*
*****************************************************************************
*
*/

void FreeDMABuffer( __reg("a6") struct PrometheusBase *pb, __reg("a0") APTR buffer, __reg("d0") ULONG size)
  {
    struct Library *SysBase = pb->pb_SysBase;
    struct Library *CardBase;

	D(kprintf("Prm_FreeDMABuffer 0x%08lx 0x%08lx\n", buffer, size));

    {
      if (!pb->pb_DMASuppBase)
      {
        pb->pb_DMASuppBase = OpenLibrary(CARD_NAME, 7);
      }

      if (CardBase = pb->pb_DMASuppBase)
      {
        FreeDMAMem(buffer, size);
      }
    }
    return;
  }

/****** prometheus.library/Prm_GetPhysicalAddress ***************************
*
*   NAME
*       Prm_GetPhysicalAddress -- Converts logical address in Prometheus
*         address space to physical address on PCI bus. (V2)
*
*   SYNOPSIS
*       PhyAddr = Prm_GetPhysicalAddress (Address)
*                                         D0
*
*       APTR Prm_GetPhysicalAddress (APTR);
*
*   FUNCTION
*       Calculates physical PCI bus address of any address in Prometheus
*       address space. This physical address is especially useful for
*       programming DMA buffer address in PCI boards' DMA controllers.
*
*   INPUTS
*       Address -  Logical address as seen by CPU. Address is checked if it
*                  belongs to Prometheus PCI Memory space.
*   RESULT
*       PhyAddr -  The same address as seen by PCI boards. If given parameter
*                  is not a valid logical PCI address in Memory space
*                  function returns NULL.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_AllocDMABuffer()
*
*****************************************************************************
*
*/

APTR GetPhysicalAddress( __reg("a6") struct PrometheusBase *pb, __reg("d0") APTR addr)
{
	D(kprintf("Prm_GetPhysicalAddress 0x%08lx\n", addr));
	
	if(pb->pb_FireStorm == TRUE) {
		if (((ULONG)addr < (ULONG)pb->pb_BaseAddr) ||
			((ULONG)addr >= (ULONG)pb->pb_BaseAddr + FS_PCI_ADDR_CONFIG0)) return NULL;
		else return (APTR)((ULONG)addr & 0x1FFFFFFF);
	}
	else
	{
		if (((ULONG)addr < (ULONG)pb->pb_BaseAddr + 0x00100000) ||
			((ULONG)addr > (ULONG)pb->pb_BaseAddr + 0x1FFFFFFF)) return NULL;
		else return (APTR)((ULONG)addr & 0x1FFFFFFF);
	}
}

/****** prometheus.library/Prm_GetVirtualAddress ***************************
*
*   NAME
*       Prm_GetPhysicalAddress -- Converts physical address on PCI bus
*       to logical address in Prometheus address space (V3)
*
*   SYNOPSIS
*       PhyAddr = Prm_GetVirtualAddress (Address)
*                                         D0
*
*       APTR Prm_GetVirtualAddress (APTR);
*
*   FUNCTION
*       Calculates physical address of any logical address in Prometheus
* 		PCI bus address space.
*
*   INPUTS
*       Address -  physical address as seen by PCI bus. Address is checked if it
*                  belongs to Prometheus PCI Memory space.
*   RESULT
*       PhyAddr -  The same address as seen by the CPU. If given parameter
*                  is not a valid physikal PCI address in Memory space
*                  function returns NULL.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_AllocDMABuffer()
*
*****************************************************************************
*
*/

APTR GetVirtualAddress( __reg("a6") struct PrometheusBase *pb, __reg("d0") APTR addr)
{
	D(kprintf("Prm_GetVirtualAddress 0x%08lx\n", addr));

	if(pb->pb_FireStorm == TRUE) {
		if ( (ULONG)addr >= FS_PCI_ADDR_CONFIG0 ) return NULL;
		else return (APTR)((ULONG)addr + (ULONG)pb->pb_BaseAddr);
	} else {
		if ( ((ULONG)addr < 0x00100000) || ((ULONG)addr > 0x1FFFFFFF) ) return NULL;
		else return (APTR)((ULONG)addr + (ULONG)pb->pb_BaseAddr);
	}
}

/****** prometheus.library/Prm_AllocPCIAddressSpace ******************************
*
*   NAME
*       Prm_AllocPCIAddressSpace -- Allocates an address range in PCI memory space
*                                   for the indicated PCI BAR. (V4)
*
*   SYNOPSIS
*       PciAddr = Prm_AllocPCIAddressSpace (Board, Size, Bar)
*                                            A0     D0    D1
*
*       APTR Prm_AllocPCIAddressSpace (PCIBoard*, ULONG, ULONG);
*
*   FUNCTION
*       This function allows for pci address space to be reserved for certain devices
*       especially PPC cards.
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Data    - Size of the PCI address space you want to reserve
*       Bar     - The P{I BAR for which this reservation is intended
*
*   RESULT
*       PciAddr - Logical address as seen by the CPU in Prometheus PCI bus
*       address space.
*
*   NOTES
*       This only reserves the address in the PCI address space. The user needs
*       to write the address to the intended configuration register himself.
*
*   BUGS
*
*   SEE ALSO
*       Prm_FreePCIAddressSpace()
*
*****************************************************************************
*
*/

APTR AllocPCIAddressSpace( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") ULONG size, __reg("d1") ULONG bar)
{
    ULONG memsizeTag, memaddrTag, memLower;
    struct Library *UtilityBase;
    struct TagItem* memsizetagPtr;
    struct TagItem* memaddrtagPtr;
    struct TagItem* pcibustagPtr;
    struct PCIBus* curpciBus;
    struct PCIBus* nxtpciBus;
    struct SpaceReq* cursrq;
    struct SpaceReq* nxtsrq;

    UtilityBase = pb->pb_UtilityBase;

    D(kprintf("Prm_AllocPCIAddressSpace bar %ld size 0x%08lx\n", bar, size));

    memLower = 0;
    memsizeTag = PRM_MemorySize0 + bar;
    memaddrTag = PRM_MemoryAddr0 + bar;

    memsizetagPtr = FindTagItem(memsizeTag, board->pn_TagList);
    memaddrtagPtr = FindTagItem(memaddrTag, board->pn_TagList);
    pcibustagPtr = FindTagItem(PRM_BusNumber, board->pn_TagList);

    if ((!(memsizetagPtr->ti_Data)) && (!(memaddrtagPtr->ti_Data)))
    {
        memLower = (ULONG)pb->pb_BaseAddr;
        curpciBus = (struct PCIBus*)pb->pb_Busses.lh_Head;

        while (nxtpciBus = (struct PCIBus*)curpciBus->br_Node.ln_Succ)
        {
            if (curpciBus->br_BusNr == pcibustagPtr->ti_Data) break;
            curpciBus = nxtpciBus;
        }

        cursrq = (struct SpaceReq*)curpciBus->br_CardRequests.lh_Head;

        while (nxtsrq = (struct SpaceReq*)cursrq->sr_Node.ln_Succ)
        {
            if (cursrq->sr_Type == BLOCK_INOUT)
            {
                cursrq = nxtsrq;
            }
            else if (((memLower <= cursrq->sr_Tag->ti_Data) && (cursrq->sr_Tag->ti_Data < (memLower + size))) ||
                    ((cursrq->sr_Tag->ti_Data <= memLower) && (memLower < (cursrq->sr_Tag->ti_Data + cursrq->sr_Size))))
            {
                memLower = memLower + cursrq->sr_Size;
                if (memLower & ~(-size))
                {
                    memLower = ((memLower + size) & (-size));
                }
                cursrq = (struct SpaceReq*)curpciBus->br_CardRequests.lh_Head;
            }
            else
            {
                cursrq = nxtsrq;
            }
            if ((memLower > ((ULONG)pb->pb_BaseAddr) + FS_PCI_ADDR_CONFIG0) || (memLower + size > ((ULONG)pb->pb_BaseAddr) + 0x20000000))
            {
                memLower = 0;
                break;
            }
        }
        if (memLower)
        {
            memsizetagPtr->ti_Data = size;
            memaddrtagPtr->ti_Data = memLower;
            AddRequest(pb, curpciBus, size, BLOCK_USRMEM, memaddrtagPtr, NULL, 0);
        }
    }
    return (APTR)memLower;
}

/****** prometheus.library/Prm_FreePCIAddressSpace ******************************
*
*   NAME
*       Prm_FreePCIAddressSpace -- Frees an address range in PCI memory space
*                                  for the indicated PCI BAR previously reserved
*                                  by Prm_AllocPCIAddressSpace(). (V4)
*
*   SYNOPSIS
*       Prm_FreePCIAddressSpace (Board, Bar)
*                                 A0     D0
*
*       VOID Prm_FreePCIAddressSpace (PCIBoard*, ULONG);
*
*   FUNCTION
*       This function frees up reserved PCI address space previously allocated
*       by AllocPCIAddressSpace(). PCI address space reserved during initialization
*       of the prometheus.library cannot be freed.
*
*   INPUTS
*       Board   - "Blackbox" structure pointer returned by
*                 Prm_FindBoardTagList().
*       Bar     - The P{I BAR for which this reservation is intended
*
*   RESULT
*       None.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       Prm_AllocPCIAddressSpace()
*
*****************************************************************************
*
*/

void FreePCIAddressSpace( __reg("a6") struct PrometheusBase *pb, __reg("a0") PCIBoard *board, __reg("d0") ULONG bar)
{
    ULONG memsizeTag, memaddrTag;
    struct Library *UtilityBase;
    struct Library *SysBase;
    struct TagItem* memsizetagPtr;
    struct TagItem* memaddrtagPtr;
    struct TagItem* pcibustagPtr;
    struct PCIBus* curpciBus;
    struct PCIBus* nxtpciBus;
    struct SpaceReq* cursrq;
    struct SpaceReq* nxtsrq;

    SysBase = pb->pb_SysBase;
    UtilityBase = pb->pb_UtilityBase;

    D(kprintf("Prm_FreePCIAddressSpace bar %ld\n", bar));

    memsizeTag = PRM_MemorySize0 + bar;
    memaddrTag = PRM_MemoryAddr0 + bar;

    memsizetagPtr = FindTagItem(memsizeTag, board->pn_TagList);
    memaddrtagPtr = FindTagItem(memaddrTag, board->pn_TagList);
    pcibustagPtr = FindTagItem(PRM_BusNumber, board->pn_TagList);

    if ((memsizetagPtr->ti_Data) && (memaddrtagPtr->ti_Data))
    {
        curpciBus = (struct PCIBus*)pb->pb_Busses.lh_Head;

        while (nxtpciBus = (struct PCIBus*)curpciBus->br_Node.ln_Succ)
        {
            if (curpciBus->br_BusNr == pcibustagPtr->ti_Data) break;
            curpciBus = nxtpciBus;
        }
        cursrq = (struct SpaceReq*)curpciBus->br_CardRequests.lh_Head;

        while (nxtsrq = (struct SpaceReq*)cursrq->sr_Node.ln_Succ)
        {
            if ((cursrq->sr_Type == BLOCK_USRMEM) && (cursrq->sr_Tag->ti_Data == memaddrtagPtr->ti_Data))
            {
                Remove((struct Node*)cursrq);
                FreePooled(pb->pb_MemPool, (APTR)cursrq, sizeof(struct SpaceReq));
                memsizetagPtr->ti_Data = 0;
                memaddrtagPtr->ti_Data = NULL;
                break;
            }
            cursrq = nxtsrq;
        }
    }
    return;
}


#endif
