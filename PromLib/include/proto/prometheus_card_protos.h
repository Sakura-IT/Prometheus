#ifndef _VBCCINLINE_PROMETHEUS_CARD_H
#define _VBCCINLINE_PROMETHEUS_CARD_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

BOOL __FindCard(__reg("a6") struct Library *, __reg("a0") struct BoardInfo * bi)="\tjsr\t-30(a6)";
#define FindCard(bi) __FindCard(CardBase, (bi))

BOOL __InitCard(__reg("a6") struct Library *, __reg("a0") struct BoardInfo * bi, __reg("a1") STRPTR* tooltypes)="\tjsr\t-36(a6)";
#define InitCard(bi, tooltypes) __InitCard(CardBase, (bi), (tooltypes))

APTR __AllocDMAMem(__reg("a6") struct Library *, __reg("d0") ULONG size)="\tjsr\t-42(a6)";
#define AllocDMAMem(size) __AllocDMAMem(CardBase, (size))

VOID __FreeDMAMem(__reg("a6") struct Library *, __reg("a0") APTR mem, __reg("d0") ULONG size)="\tjsr\t-48(a6)";
#define FreeDMAMem(mem, size) __FreeDMAMem(CardBase, (mem), (size))

#endif /*  _VBCCINLINE_PROMETHEUS_CARD_H  */
