#ifndef _INLINE_PROMETHEUS_CARD_H
#define _INLINE_PROMETHEUS_CARD_H

#ifndef CLIB_PROMETHEUS_CARD_PROTOS_H
#define CLIB_PROMETHEUS_CARD_PROTOS_H
#endif

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef "BOARDINFO_H"
#include "boardinfo.h"
#endif

#ifndef PROMETHEUS_CARD_BASE_NAME
#define PROMETHEUS_CARD_BASE_NAME CardBase
#endif

#define FindCard(bi) \
	LP1(0x1e, BOOL, FindCard, struct BoardInfo *, bi, a0, \
	, PROMETHEUS_CARD_BASE_NAME)

#define InitCard(bi, tooltypes) \
	LP2(0x24, BOOL, InitCard, struct BoardInfo *, bi, a0, STRPTR*, tooltypes, a1, \
	, PROMETHEUS_CARD_BASE_NAME)

#define AllocDMAMem(size) \
	LP1(0x2a, APTR, AllocDMAMem, ULONG, size, d0, \
	, PROMETHEUS_CARD_BASE_NAME)

#define FreeDMAMem(mem, size) \
	LP2NR(0x30, FreeDMAMem, APTR, mem, a0, ULONG, size, d0, \
	, PROMETHEUS_CARD_BASE_NAME)

#endif /*  _INLINE_PROMETHEUS_CARD_H  */
