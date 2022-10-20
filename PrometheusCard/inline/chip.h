#ifndef _INLINE_CHIP_H
#define _INLINE_CHIP_H

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef CHIP_BASE_NAME
#define CHIP_BASE_NAME ChipBase
#endif

#define InitChip(bi) \
	LP1NR(0x1e, InitChip, struct BoardInfo *, bi, a0, \
	, CHIP_BASE_NAME)

#endif /*  _INLINE_CHIP_H  */
