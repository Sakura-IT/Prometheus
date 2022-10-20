/* little <-> big endian conversion macros for GCC */

/* swap word */


#ifdef __VBCC__


ULONG swapl(__reg("d0")ULONG) = "\trol.w\t#8,d0\n\tswap\td0\n\trol.w\t#8,d0\n";
ULONG swapw(__reg("d0")ULONG) = "\trol.w\t#8,d0\n";

#else


#define swapw(arg)\
 ({short __arg = (arg);\
  asm ("ROL.W #8,%0"\
    :"=d" (__arg)\
    :"0" (__arg)\
    :"cc");\
    __arg;})

/* swap long */

#define swapl(arg)\
 ({long __arg = (arg);\
  asm ("ROL.W #8,%0;\
        SWAP %0;\
        ROL.W #8,%0"\
    :"=d" (__arg)\
    :"0" (__arg)\
    :"cc");\
    __arg;})
#endif
