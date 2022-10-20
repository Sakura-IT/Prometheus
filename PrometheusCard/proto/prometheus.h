#ifndef _PROTO_PROMETHEUS_H
#define _PROTO_PROMETHEUS_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#if !defined(CLIB_PROMETHEUS_PROTOS_H) && !defined(__GNUC__)
#include <clib/prometheus_protos.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library *PrometheusBase;
#endif

#ifdef __GNUC__
#include <inline/prometheus.h>
#elif defined(__VBCC__)
#if defined(__MORPHOS__) || !defined(__PPC__)
#include <inline/prometheus_protos.h>
#endif
#else
#include <pragma/prometheus_lib.h>
#endif

#endif	/*  _PROTO_PROMETHEUS_H  */
