#ifndef _PROTO_PROMETHEUS_CARD_H
#define _PROTO_PROMETHEUS_CARD_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#if !defined(CLIB_PROMETHEUS_CARD_PROTOS_H) && !defined(__GNUC__)
#include <clib/prometheus_card_protos.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library *CardBase;
#endif

#ifdef __GNUC__
#include <inline/prometheus_card.h>
#elif !defined(__VBCC__)
#include <pragma/prometheus_card_lib.h>
#endif

#endif	/*  _PROTO_PROMETHEUS_CARD_H  */
