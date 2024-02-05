#include <libraries/prometheus.h>
#include <proto/exec.h>
#include <proto/picasso96_chip.h>
#include <proto/prometheus.h>

#include "card.h"

struct CardInfo
{
    ULONG  Device;
    UBYTE *Memory0;
    UBYTE *Memory1;
    UBYTE *Memory2;
    ULONG  Memory1Size;
    UBYTE *ROM;
};

static ULONG count = 0;
#define PCI_VENDOR 0x121A
#define CHIP_NAME  "picasso96/3dfxVoodoo.chip"

#define ROMBase  ChipData[14]
#define DeviceID ChipData[15]

BOOL Init3dfxVoodoo(struct CardBase *cb, struct BoardInfo *bi)
{
    struct Library *PrometheusBase = cb->cb_PrometheusBase;
    struct Library *SysBase        = cb->cb_SysBase;

    APTR  board   = NULL;
    ULONG current = 0;

#ifdef DBG
    KPrintf("prometheus.card: Init3dfxVoodoo()\n");
#endif

    while ((board = (APTR)Prm_FindBoardTags(board, PRM_Vendor, PCI_VENDOR, TAG_END)) != NULL)
    {
        struct CardInfo ci;
        BOOL            found = FALSE;

#ifdef DBG
        KPrintf("  Voodoo board found on PCI [$%08lx]\n", (ULONG)board);
#endif

        Prm_GetBoardAttrsTags(board,
                              PRM_Device,
                              (ULONG)&ci.Device,
                              PRM_MemoryAddr0,
                              (ULONG)&ci.Memory0,
                              PRM_MemoryAddr1,
                              (ULONG)&ci.Memory1,
                              PRM_MemoryAddr2,
                              (ULONG)&ci.Memory2,
                              PRM_MemorySize1,
                              (ULONG)&ci.Memory1Size,
                              PRM_ROM_Address,
                              (ULONG)&ci.ROM,
                              TAG_END);

        switch (ci.Device)
        {
        case 3:  // Banshee
        case 5:  // Avenger
        case 9:  // Rampage
            found = TRUE;
            break;
        default:
            found = FALSE;
        }

        if (found)
        {
            struct ChipBase *ChipBase;

#ifdef DBG
            KPrintf("  card attrs read (device %ld)\n", (ULONG)ci.Device);
#endif

            // check for multiple hits and skip the ones already used
            if (current++ < count) continue;

            // we have found one - so mark it as used and
            // don't care about possible errors from here on
            count++;

            if ((ChipBase = (struct ChipBase *)OpenLibrary(CHIP_NAME, 7)) != NULL)
            {
#ifdef DBG
                KPrintf("  chip driver opened [$%08lx]\n", (ULONG)ChipBase);
#endif

                bi->ChipBase = ChipBase;

                bi->DeviceID     = ci.Device;
                bi->MemoryIOBase = ci.Memory0;
                bi->MemoryBase   = ci.Memory1;
                bi->RegisterBase = ci.Memory2;
                bi->MemorySize   = ci.Memory1Size;
                bi->ROMBase      = (ULONG)ci.ROM;

                // register interrupt server
                RegisterIntServer(cb, board, &bi->HardInterrupt);

                InitChip(bi);

                // enable vertical blanking interrupt
                bi->Flags |= BIF_VBLANKINTERRUPT;

                Prm_SetBoardAttrsTags(board, PRM_BoardOwner, (ULONG)ChipBase, TAG_END);

                return TRUE;
            }
        }
    }  // while
    return FALSE;
}
