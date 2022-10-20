#ifndef _PROTO_CHIP_H
#define _PROTO_CHIP_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef CLIB_CHIP_PROTOS_H
#include <clib/chip_protos.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library *ChipBase;
#endif

#ifdef __GNUC__
#include <inline/chip.h>
#elif defined(__VBCC__)
#include <inline/chip_protos.h>
#endif

#endif	/*  _PROTO_CHIP_H  */
