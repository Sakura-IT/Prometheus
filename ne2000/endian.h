/* little <-> big endian conversion macros for GCC */

/* swap word */

#define swapw(arg)                                            \
    ({                                                        \
        short __arg = (arg);                                  \
        asm("ROL.W #8,%0" : "=d"(__arg) : "0"(__arg) : "cc"); \
        __arg;                                                \
    })

/* swap long */

#define swapl(arg)          \
    ({                      \
        long __arg = (arg); \
        asm("ROL.W #8,%0;\
        SWAP %0;\
        ROL.W #8,%0"  \
            : "=d"(__arg)   \
            : "0"(__arg)    \
            : "cc");        \
        __arg;              \
    })
