#ifndef _VBCCINLINE_CHIP_H
#define _VBCCINLINE_CHIP_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

void __InitChip(__reg("a6") struct ChipBase *, __reg("a0") struct BoardInfo * bi)="\tjsr\t-30(a6)";
#define InitChip(bi) __InitChip(ChipBase, (bi))

#endif /*  _VBCCINLINE_CHIP_H  */
