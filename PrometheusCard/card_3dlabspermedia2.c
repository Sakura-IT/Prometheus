#include <proto/exec.h>
#include <proto/prometheus.h>
#include <libraries/prometheus.h>
#include <proto/picasso96_chip.h>

#include "card.h"

struct CardInfo
  {
    ULONG   Device;
    UBYTE  *Memory0;
    UBYTE  *Memory1;
    UBYTE  *Memory2;
    ULONG   Memory1Size;
    ULONG   Memory2Size;
};

static ULONG count = 0;

#define PCI_VENDOR  0x104C  // TexasInstruments
#define CHIP_NAME   "picasso96/3DLabsPermedia2.chip"

BOOL Init3DLabsPermedia2(struct CardBase *cb, struct BoardInfo *bi)
  {
    struct Library* SysBase = cb->cb_SysBase;
    struct Library* PrometheusBase = cb->cb_PrometheusBase;
    APTR board = NULL;
    ULONG current = 0;

    while((board = (APTR)Prm_FindBoardTags(board, PRM_Vendor, PCI_VENDOR, TAG_END)) != NULL)
      {
        struct CardInfo ci;
        BOOL found = FALSE;

        Prm_GetBoardAttrsTags(board,
          PRM_Device, (ULONG)&ci.Device,
          PRM_MemoryAddr0, (ULONG)&ci.Memory0,
          PRM_MemoryAddr1, (ULONG)&ci.Memory1,
          PRM_MemoryAddr2, (ULONG)&ci.Memory2,
          PRM_MemorySize1, (ULONG)&ci.Memory1Size,
          PRM_MemorySize2, (ULONG)&ci.Memory2Size,
        TAG_END);

        switch(ci.Device)
          {
            case 0x3D07:  // Permedia2
              found = TRUE;
            break;

            default:
              found = FALSE;
          }

        if(found)
          {
            struct ChipBase *ChipBase;

            /* check for multiple hits and skip the ones already used */

            if(current++ < count) continue;

            /* we have found one - so mark it as used and */
            /* don't care about possible errors from here on */

            count++;

            if ((ChipBase = (struct ChipBase *)OpenLibrary(CHIP_NAME, 1)) != NULL)
              {
                bi->ChipBase     = ChipBase;
                bi->MemoryBase   = ci.Memory1;
                bi->RegisterBase = (UBYTE *)((ULONG)ci.Memory0 + 0x18000);
                bi->MemoryIOBase = ci.Memory2;
                bi->MemorySize   = ci.Memory1Size;

        // register interrupt server
        RegisterIntServer(cb, board, &bi->HardInterrupt);

        InitChip(bi);

        // enable vertical blanking interrupt
        bi->Flags |= BIF_VBLANKINTERRUPT;

        if (ci.Memory1 + ci.Memory1Size == ci.Memory2)
        {
            bi->MemorySpaceBase = ci.Memory1;
            bi->MemorySpaceSize = ci.Memory1Size + ci.Memory2Size;
        }
        else if (ci.Memory2 + ci.Memory2Size == ci.Memory1)
        {
            bi->MemorySpaceBase = ci.Memory2;
            bi->MemorySpaceSize = ci.Memory1Size + ci.Memory2Size;
        }
        else
        {
            bi->MemorySpaceBase = ci.Memory1;
            bi->MemorySpaceSize = ci.Memory1Size;
        }

        // enable special cache mode settings
        bi->Flags |= BIF_CACHEMODECHANGE;

                Prm_SetBoardAttrsTags(board,
                  PRM_BoardOwner, (ULONG)ChipBase,
                TAG_END);


        return TRUE;
              }
          }
      }
    return FALSE;
  }

