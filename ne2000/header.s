/*--------------------------------------------------------------------------*/
/* Resident header written for prometheus-rtl8029.device                    */
/* $VER: header.s 1.0 (1.10.2002)                                           */
/*--------------------------------------------------------------------------*/

#include "rev.h"

            .global     _DevInit
            .global     _romtag
            .global     _IdString
            .global     _CopyB

/* Empty function protecting against running library as executable. */

            MOVEQ       #-1,d0
            RTS

/* Resident table. */

_romtag:    .short      0x4AFC
            .long       _romtag
            .long       _tagend
            .byte       0,1,3,0
            .long       _devname
            .long       _IdString
            .long       _DevInit


_tagend:
            .align      2
_devname:   .string     "prm-rtl8029.device"


/* CopyB(APTR function_ptr, APTR dest, APTR src, ULONG len) */

            .align      2

_CopyB:     MOVE.L      a2,-(sp)
            JSR         (a2)
            MOVE.L      (sp)+,a2
            RTS
