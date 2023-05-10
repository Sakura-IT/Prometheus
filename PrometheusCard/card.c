#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/expansion.h>
#include <proto/prometheus.h>
#include <libraries/prometheus.h>
#include <exec/execbase.h>
#include <hardware/intbits.h>

#include "boardinfo.h"
#include "card.h"

#define HAS_TOOLTYPES (bi->GetVSyncState != NULL)

// we have that one in the BoardInfo structure
// so there's no need to open it again.

typedef void (*dpmsFunc)(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level));
typedef void (*intFunc)(void);

/******************************************************************************************
*                                                                                         *
* This function registers the interrupt server for the board                              *
*                                                                                         *
******************************************************************************************/

void RegisterIntServer(struct CardBase *cb, void *board, struct Interrupt *interrupt)
  {
    struct Library* PrometheusBase = cb->cb_PrometheusBase;
    struct Library* SysBase = cb->cb_SysBase;

    if(PrometheusBase->lib_Version >= 2)
      {
        Prm_AddIntServer(board, interrupt);
      }
    else
      {
        AddIntServer(INTB_PORTS, interrupt);
      }
  }

/******************************************************************************************
*                                                                                         *
* This function registers the owner for the board to prevent it from being used twice    *
*                                                                                         *
******************************************************************************************/

void RegisterOwner(struct CardBase *cb, void *board, struct Node *driver)
  {
    struct Library* PrometheusBase = cb->cb_PrometheusBase;

    if (PrometheusBase->lib_Version >= 2)
      {
         Prm_SetBoardAttrsTags(board, PRM_BoardOwner, (ULONG)driver);
      }
  }

/******************************************************************************************
*                                                                                         *
* This function should set a board switch to let the Amiga signal pass through when       *
* supplied with a 0 in d0 and to show the board signal if a 1 is passed in d0. You should *
* remember the current state of the switch to avoid unneeded switching. You may use the   *
* MoniSwitch field of the BoardInfo structure for buffering the current state.            *
*                                                                                         *
* For PCI bridges we have no integrated monitor switch solution, but there is an external *
* display switch from JAVOSOFT that shows the secondary video input if the primary video  *
* signal is configure for DPMS_OFF.                                                       *
*                                                                                         *
******************************************************************************************/

BOOL SetSwitch(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
  {
    BOOL oldState = (bi->MoniSwitch & 1) ? TRUE : FALSE;
  
    bi->MoniSwitch = (bi->MoniSwitch & ~1) | (state ? 1 : 0);

    if (oldState != (state ? TRUE : FALSE))
      {
        /* when anything changed shut primary video off */
        /* or restore current DPMS level logged by our  */
        /* SetDPMSLevel replacement below               */

        ((dpmsFunc)bi->JSetOldDPMSLevel)(bi, state ? bi->JDPMSLevel : DPMS_OFF);
      }
    return oldState;
  }

/********************************************************************************************
* This function sets the DPMS level supplied. Valid values are (see boardinfo.h):           *
*    DPMS_ON      (normal operation)                                                        *
*    DPMS_STANDBY (Optional state of minimal power reduction)                               *
*    DPMS_SUSPEND (Significant reduction of power consumption)                              *
*    DPMS_OFF     (Monitor off, lowest level of power consumption)                          *
*                                                                                           *
********************************************************************************************/

void SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
  {
    if (level <= DPMS_OFF)
      {
        /* log new level in any case */

        bi->JDPMSLevel = level;

        /* but only do real DPMS if the primary video is visible */

        if(bi->MoniSwitch & 1) ((dpmsFunc)bi->JSetOldDPMSLevel)(bi, level);
      }
  }

/********************************************************************************************
*                                                                                           *
* FindCard is called in the first stage of the board initialisation and configuration and   *
* is used to look if there is a free and unconfigured board of the type the driver is       *
* capable of managing. If it finds one, it immediately reserves it for use by Picasso96,    *
* usually by clearing the CDB_CONFIGME bit in the flags field of the ConfigDev struct of    *
* this expansion card. But this is only a common example, a driver can do whatever it wants *
* to mark this card as used by the driver. This mechanism is intended to ensure that a      *
* board is only configured and used by one driver. FindBoard also usually fills some fields *
* of the BoardInfo struct supplied by the caller, the rtg.library, for example the          *
* MemoryBase, MemorySize and RegisterBase fields.                                           *
*                                                                                           *
* For PCI bridges we abuse this concept to some degree - we just make sure the bridge is    *
* available. To find a board with supported chipset and to enumerate multiple boards we use *
* the InitCard function                                                                     *
*                                                                                           *
********************************************************************************************/

BOOL FindCard(__REGA0(struct BoardInfo *bi), __REGA6(struct CardBase *base))
  {
    /* we can't do very much here as bridge is already initialized */

    return TRUE;
  }

/********************************************************************************************
*                                                                                           *
* This function gets a long work representing all RGBFormats that can coexist               *
* simultaneously on the board with a bitmap of the supplied RGBFormat. This is used when a  *
* bitmap is put to the board in order to check all bitmaps currently on board if they have  *
* to leave. This is for example the case with Cirrus based boards where all chunky and      *
* Hi/TrueColor bitmaps must leave whenever a planar bitmap needs to be put to the board,    *
* because the formats cannot share the board due to restrictions by that VGA chips.         *
*                                                                                           *
********************************************************************************************/

ULONG GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
  {
    return (ULONG)~RGBFF_PLANAR | RGBFF_A8R8G8B8 | RGBFF_R8G8B8A8 | RGBFF_R5G6B5 |
      RGBFF_R5G5B5 | RGBFF_YUV422 | RGBFF_YUV422PA;
  }

/********************************************************************************************
*                                                                                           *
* Dummy interrupt handler that will be used in case an interrupt is caused before the real  *
* handler is registered by the chip driver.                                                 *
*                                                                                           *
********************************************************************************************/

LONG HardInterruptCode(__REGA1(struct BoardInfo *bi))
  {
    return 0;
  }

/********************************************************************************************
*                                                                                           *
* InitCard is called in a second stage after the rtg.library allocated other system         *
* resources like memory. During this call, all fields of the BoardInfo structure that need  *
* to be filled must be initialized.                                                         *
* A driver that consists of two separate parts like a card driver and a chip driver calls   *
* the init function of the chip driver at that time.                                        *
*                                                                                           *
* That's exactly what we do: find any supported board and use the specific chip driver to   *
* initialize the hardware. This way we make the chip driver re-usable for any other PCI or  *
* a ZORRO bridge.                                                                           *
*                                                                                           *
********************************************************************************************/

BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(char **ToolTypes), __REGA6(struct CardBase *cb))
  {
    struct Library* PrometheusBase = cb->cb_PrometheusBase;
    BOOL found = FALSE;
    BOOL forbid_interrupts = FALSE;
    BOOL special_switch = FALSE;
    ULONG dma_size = 0;
    struct Library* UtilityBase = bi->UtilBase;

    /* add dummy handler for safety reasons... */

    bi->HardInterrupt.is_Code = (intFunc)HardInterruptCode;

    if (HAS_TOOLTYPES)
      {
        /* new Picasso96 version (with tooltype support) */

        char *ToolType;

        while (ToolType = *ToolTypes++)
          {
            /* check if JAVOSOFT is specified as switch type */
            /* this is only important if just one monitor is used */
      
            if (bi->Flags & BIF_INDISPLAYCHAIN)
              {
                if (Stricmp(ToolType, "SWITCHTYPE=JAVOSOFT") == 0)
                  {
                    special_switch = TRUE;
                  }
              }

            /* user likes to disable vertical blanking interrupt? */

            if (Stricmp(ToolType, "NOINTERRUPT=Yes") == 0)
              {
                forbid_interrupts = TRUE;
              }

            /* user likes to reserve some memory for DMA? */

            if (Strnicmp(ToolType, "DMASIZE=", 8) == 0)
              {
                if (!cb->cb_DMAMemGranted)
                  {
                    STRPTR cp = &ToolType[8];
                    UBYTE c;
                    ULONG size = 0;

                    while ((c = *cp++) != 0)
                      {
                        if ((c >= '0') && (c <= '9'))
                          {
                            size = size*10 + (c - '0');
                          }
                        else
                          {
                            if ((c == 'k') || (c == 'K')) size = size << 10;
                            else if((c == 'm') || (c == 'M')) size = size << 20;
                          }
                      }

                    if (size > 0)
                      {
                        dma_size = size;
                        bi->Flags |= BIF_GRANTDIRECTACCESS;
                      }
                  }
              }
          }
      }

  /* check Banshee/Voodoo3/4/5 based cards */

  if (!found) found = Init3dfxVoodoo(cb, bi);

  /* check ViRGE based S3 cards */

  if (!found) found = InitS3ViRGE(cb, bi);

  /* check Permedia2 based cards (3DLabs/TI) */

  if(!found) found = Init3DLabsPermedia2(cb, bi);

  if (found)
    {
      bi->BoardName = BOARD_NAME;
      bi->BoardType = BOARD_TYPE;
      if (!(bi->ExecBase->AttnFlags & AFF_68040) && (bi->ExecBase->AttnFlags & AFF_68030))
        {
          /* an 030 Amiga - don't use cache modifications. */

          bi->Flags &= ~BIF_CACHEMODECHANGE;
        }
      if (special_switch)
        {
          /* plug in handler for external monitor switch */

          bi->SetSwitch = SetSwitch;

          /* save chip specific SetDPMSLevel function */

          bi->JSetOldDPMSLevel = (ULONG)bi->SetDPMSLevel;

          /* and overwrite with enhanced implementation */

          bi->SetDPMSLevel = SetDPMSLevel;

          /* set startup value */

          bi->MoniSwitch = (UWORD)0xFFFF;
        }


      if (forbid_interrupts)
        {
          /* just make sure no interrupts are caused */
          
          bi->Flags = bi->Flags & ~(ULONG)BIF_VBLANKINTERRUPT;
        }
      if (PrometheusBase->lib_Version >= 2)
        {
          if ((dma_size > 0) && (dma_size <= bi->MemorySize))
            {
              cb->cb_DMAMemGranted = TRUE;
              bi->MemorySize = bi->MemorySize - dma_size;
              InitDMAMemory(cb, (APTR)((ULONG)bi->MemoryBase + bi->MemorySize), dma_size);
            }
        }
    }
    return found;
  }

