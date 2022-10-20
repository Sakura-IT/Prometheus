#ifndef _INCLUDE_PRAGMA_PROMETHEUS_CARD_LIB_H
#define _INCLUDE_PRAGMA_PROMETHEUS_CARD_LIB_H

#ifndef CLIB_PROMETHEUS_CARD_PROTOS_H
#include <clib/prometheus_card_protos.h>
#endif

#if defined(AZTEC_C) || defined(__MAXON__) || defined(__STORM__)
#pragma amicall(CardBase,0x01e,FindCard(a0))
#pragma amicall(CardBase,0x024,InitCard(a0,a1))
#pragma amicall(CardBase,0x02a,AllocDMAMem(d0))
#pragma amicall(CardBase,0x030,FreeDMAMem(a0,d0))
#endif
#if defined(_DCC) || defined(__SASC)
#pragma  libcall CardBase FindCard               01e 801
#pragma  libcall CardBase InitCard               024 9802
#pragma  libcall CardBase AllocDMAMem            02a 001
#pragma  libcall CardBase FreeDMAMem             030 0802
#endif

#endif	/*  _INCLUDE_PRAGMA_PROMETHEUS_CARD_LIB_H  */
