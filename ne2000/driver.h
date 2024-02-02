/* Prometheus driver skeleton header file */

/* Main option switches */

#define PDEBUG        1 /* Debug on/off switch (debug off if commented). */
#define USE_TASK      1 /* Uncomment if unit runs own task.              */
#define USE_INTERRUPT 1 /* Uncomment if unit uses interrupt.             */

#define MAX_UNITS 4 /* How many units can device have? */
#define OK        0

/* PCI card ID's - put your board ID's, you can obtain them using PrmScan */

#define PCI_VENDOR 0x10EC
#define PCI_DEVICE 0x8029

/* Macro for registerized parameters (used in some OS functions). */

#define reg(a) __asm(#a)

/* Macro for declaring local libraries bases. */

#define USE(a)   struct Library *##a = dd->dd_##a; /* local base from DevData */
#define USE_U(a) struct Library *##a = ud->ud_##a; /* local base from UnitData */

/* Macros for debug messages. Replace FPrintf() with KPrintf() */
/* if you want traditional serial terminal debug output.       */

#ifdef PDEBUG
    #define USE_D(a)        struct Library *##a = dd->dd_##a;
    #define USE_UD(a)       struct Library *##a = ud->ud_##a;
    #define DBG(a)          FPrintf(dd->debug, a "\n")
    #define DBG_U(a)        FPrintf(ud->debug, a "\n")
    #define DBG_T(a)        FPrintf(ud->tdebug, a "\n")
    #define DBG1(a, b)      FPrintf(dd->debug, a "\n", (LONG)b)
    #define DBG1_U(a, b)    FPrintf(ud->debug, a "\n", (LONG)b)
    #define DBG1_T(a, b)    FPrintf(ud->tdebug, a "\n", (LONG)b)
    #define DBG2(a, b, c)   FPrintf(dd->debug, a "\n", (LONG)b, (LONG)c)
    #define DBG2_U(a, b, c) FPrintf(ud->debug, a "\n", (LONG)b, (LONG)c)
    #define DBG2_T(a, b, c) FPrintf(ud->tdebug, a "\n", (LONG)b, (LONG)c)
#else
    #define USE_D(a)
    #define USE_UD(a)
    #define DBG(a)
    #define DBG_U(a)
    #define DBG_T(a)
    #define DBG1(a, b)
    #define DBG1_U(a, b)
    #define DBG1_T(a, b)
    #define DBG2(a, b, c)
    #define DBG2_U(a, b, c)
    #define DBG2_T(a, b, c)
#endif

/* device structures */

/* Type of request. You can define your own type of request structure or use IOStdReq. */

typedef struct IOSana2Req IOMyReq;

/* Device base */

struct DevData
{
    struct Library   dd_Lib;
    APTR             dd_SegList;
    struct Library * dd_SysBase;
    struct Library * dd_PrometheusBase;
    struct Library * dd_UtilityBase;
    struct Library * dd_DOSBase;
    struct UnitData *dd_Units[MAX_UNITS];

#ifdef PDEBUG
    BPTR debug;
#endif

    /* add your own stuff here */
};

struct UnitData
{
    struct Message  ud_Message;
    struct Library *ud_SysBase;
    struct Library *ud_DOSBase;
    LONG            ud_OpenCnt;
    char *          ud_Name;

#ifdef USE_INTERRUPT
    struct Interrupt *ud_Interrupt;
#endif

#ifdef USE_TASK
    struct Task *   ud_Task;
    struct MsgPort *ud_TaskPort;
    struct MsgPort *ud_LifeTime;
#endif

    APTR ud_Hardware; /* you can change the type to structure */
                      /* describing hardware registers */

#ifdef PDEBUG
    BPTR debug;
    BPTR tdebug;
#endif

    /* add your own stuff here */
};

/* prototypes */

struct DevData *DevInit(void *seglist reg(a0), struct Library *sysb reg(a6));
LONG            DevOpen(IOMyReq *req reg(a1), LONG unit reg(d0), LONG flags reg(d1), struct DevData *dd reg(a6));
APTR            DevClose(IOMyReq *req reg(a1), struct DevData *dd reg(a6));
APTR            DevExpunge(struct DevData *dd reg(a6));
long            DevReserved(void);
void            DevBeginIO(IOMyReq *req reg(a1), struct DevData *dd reg(a6));
ULONG           DevAbortIO(IOMyReq *req reg(a1), struct DevData *dd reg(a6));

void             IoDone(struct UnitData *ud, IOMyReq *req, LONG err);
LONG             OpenDeviceLibraries(struct DevData *dd);
void             CloseDeviceLibraries(struct DevData *dd);
LONG             RunTask(struct DevData *dd, struct UnitData *ud);
struct UnitData *OpenUnit(struct DevData *dd, LONG unit, LONG flags);
struct UnitData *InitializeUnit(struct DevData *dd, LONG unit);
void             CloseUnit(struct DevData *dd, struct UnitData *ud);
void             ExpungeUnit(struct DevData *dd, struct UnitData *ud);

void __saveds CmdNSDQuery(struct UnitData *ud, struct IOStdReq *req);

APTR FindHardware(struct DevData *dd, WORD unit);
LONG InstallInterrupt(struct UnitData *ud);
void RemoveInterrupt(struct UnitData *ud);

void UnitTask(void);
