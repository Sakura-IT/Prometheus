/*
$VER: Prometheus.card 7.601 (08.05.2023) by Dennis Boon with fixes from Mathias Heyer
- Added more S3 cards for DMA to card_s3virge.c
- Bumped version past 7.530 of which source was lost
$VER: Prometheus.card 6.504 (19.08.2002) by Grzegorz Kraszewski
*/

#define __NOLIBBASE__

#include <stdarg.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/utility.h>
#include <proto/expansion.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <exec/execbase.h>
#include <libraries/configvars.h>
#include <hardware/intbits.h>

#include "card.h"

/*--- Functions prototypes -------------------------------------------------*/

struct CardBase *LibOpen (__REGA6(struct CardBase *cb));
LONG LibClose (__REGA6(struct CardBase *cb));
APTR LibExpunge (__REGA6(struct CardBase *cb));
LONG LibReserved (void);
struct CardBase *LibInit (__REGD0(struct CardBase* cb), __REGA0(APTR seglist), __REGA6(struct Library *sysb));

#define VERSION  7
#define REVISION 601

int main(void)
{
    return -1;
}

#ifdef DEBUG
#define __DBG__ "debug "
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
#define __DBG__
#endif


char libid[]   = "\0$VER: Prometheus.card 7.601 " __DBG__ "(10.05.2023)\r\n";
char libname[] = "Prometheus.card\0";

char build[]   = "build date: " __DATE__ ", " __TIME__ "\n";


/*--------------------------------------------------------------------------*/

void *FuncTable[] =
 {
  (APTR)LibOpen,
  (APTR)LibClose,
  (APTR)LibExpunge,
  (APTR)LibReserved,
  (APTR)FindCard,
  (APTR)InitCard,
  (APTR)AllocDMAMemory,
  (APTR)FreeDMAMemory,
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
 (ULONG)               sizeof(struct CardBase),
 (APTR              *) &FuncTable[0],
 (struct MyDataInit *) &DataTab,
 (APTR)                LibInit
};


/* ------------------- ROM Tag ------------------------ */


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

/*-------------------------------------------------------------------------*/
/* Init all library resources. Called from LibOpen() if open counter is 0  */


static BOOL init_resources(struct CardBase *cb)
  {
    struct Library* SysBase = cb->cb_SysBase;
    struct ConfigDev *cd;
    struct Library *ExpansionBase;

    if (!(cb->cb_PrometheusBase = OpenLibrary("prometheus.library", 2)))
      return FALSE;
    if (!(cb->cb_ExpansionBase = OpenLibrary("expansion.library", 39)))
      return FALSE;

    /* determining IO legacy address (first 64 kB of Prometheus addr space) */

    ExpansionBase = cb->cb_ExpansionBase;
    if (cd = FindConfigDev(NULL, VENDOR_MATAY, DEVICE_PROMETHEUS))
      cb->cb_LegacyIOBase = cd->cd_BoardAddr;
    else if (cd = FindConfigDev(NULL, VENDOR_E3B, DEVICE_FIRESTORM))
      cb->cb_LegacyIOBase = (APTR)((ULONG)cd->cd_BoardAddr + 0x1fe00000);
    else return FALSE;

    /* creating memory pool */

    if (!(cb->cb_MemPool = CreatePool(MEMF_CLEAR | MEMF_PUBLIC, 1024, 512)))
      return FALSE;

    /* setting up memory list semaphore */

    if (!(cb->cb_MemSem = AllocPooled(cb->cb_MemPool,
     sizeof(struct SignalSemaphore)))) return FALSE;

//    NewList((struct List*)&cb->cb_MemList);

    InitSemaphore(cb->cb_MemSem);

    return TRUE;
  }

/*-------------------------------------------------------------------------*/
/* Free all library resources. Called:                                     */
/*  1. From LibExpunge().                                                  */
/*  2. From init_resources if any resource failed.                         */

static void free_resources(struct CardBase *cb)
  {
    struct Library* SysBase = cb->cb_SysBase;

    if (cb->cb_MemSem)
      {
        APTR sm = cb->cb_MemSem;

        Forbid();
        ObtainSemaphore(sm);
        ReleaseSemaphore(sm);
        cb->cb_MemSem = NULL;
        Permit();
        FreePooled(cb->cb_MemPool, sm, sizeof(struct SignalSemaphore));
      }
    if (cb->cb_MemPool) DeletePool(cb->cb_MemPool);
    if (cb->cb_PrometheusBase) CloseLibrary (cb->cb_PrometheusBase);
    if (cb->cb_ExpansionBase) CloseLibrary (cb->cb_ExpansionBase);
    return;
  }

/*-------------------------------------------------------------------------*/
/* INIT                                                                    */
/*-------------------------------------------------------------------------*/

struct CardBase *LibInit (__REGD0(struct CardBase* cb), __REGA0(APTR seglist), __REGA6(struct Library *sysb))
  {
    struct ExecBase *SysBase = (struct ExecBase*)sysb;

    D(kprintf("prometheus.card: LibInit()\n"));

    if (!(SysBase->AttnFlags & AFF_68020))
      {
        FreeMem ((APTR)((ULONG)cb - (ULONG)cb->cb_Library.lib_NegSize),
        (LONG)cb->cb_Library.lib_PosSize + (LONG)cb->cb_Library.lib_NegSize);
        return FALSE;
      }
    cb->cb_SysBase        = sysb;
    cb->cb_ExpansionBase  = NULL;
    cb->cb_SegList        = seglist;
    cb->cb_Name           = &libname[0];
    cb->cb_PrometheusBase = NULL;
    cb->cb_DMAMemGranted  = FALSE;
    cb->cb_MemPool        = NULL;

    return cb;
  }


/*-------------------------------------------------------------------------*/
/* OPEN                                                                    */
/*-------------------------------------------------------------------------*/

struct CardBase *LibOpen (__REGA6(struct CardBase *cb))
 {
  struct CardBase *ret = cb;
  BOOL open = TRUE;

  if (cb->cb_Library.lib_OpenCnt == 0) open = init_resources(cb);

  if (open)
    {
      cb->cb_Library.lib_OpenCnt++;
      cb->cb_Library.lib_Flags &= ~LIBF_DELEXP;

      return cb;
    }
  free_resources(cb);
  return NULL;
 }

/*-------------------------------------------------------------------------*/
/* CLOSE                                                                   */
/*-------------------------------------------------------------------------*/

long LibClose (__REGA6(struct CardBase *cb))
 {
  if (!(--cb->cb_Library.lib_OpenCnt))
   {
    if (cb->cb_Library.lib_Flags & LIBF_DELEXP) return ((long)LibExpunge (cb));
   }
  return 0;
 }

/*-------------------------------------------------------------------------*/
/* EXPUNGE                                                                 */
/*-------------------------------------------------------------------------*/

void *LibExpunge (__REGA6(struct CardBase *cb))
 {
  APTR seglist;
  struct Library* SysBase = cb->cb_SysBase;

  if (cb->cb_Library.lib_OpenCnt)
   {
    cb->cb_Library.lib_Flags |= LIBF_DELEXP;
    return 0;
   }
  Remove ((struct Node*)cb);
  free_resources(cb);
  seglist = cb->cb_SegList;
  FreeMem ((APTR)((ULONG)cb - (ULONG)cb->cb_Library.lib_NegSize),
   (LONG)cb->cb_Library.lib_PosSize + (LONG)cb->cb_Library.lib_NegSize);
  return seglist;
 }

/*-------------------------------------------------------------------------*/
/* RESERVED                                                                */
/*-------------------------------------------------------------------------*/

long LibReserved (void)
 {
  return 0;
 }

