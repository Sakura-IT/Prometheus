#include <exec/libraries.h>
#include <exec/memory.h>
#include <libraries/prometheus.h>
#include <proto/exec.h>
#include <proto/prometheus.h>

#include "boardinfo.h"
#include "card.h"

#define ALIGNMENT 4096

/* PrometheusBase private fields are only used to extract SysBase, */
/* it is much faster than read it from $00000004.                  */

struct PrometheusBase
{
    struct Library  pb_Lib;
    struct Library *pb_SysBase;

    /* private not used fields here */
};

APTR AllocDMAMemory(__REGD0(ULONG size), __REGA6(struct CardBase *cb))
{
    struct Library *    SysBase  = cb->cb_SysBase;
    ULONG               bestsize = 0xFFFFFFFF;
    struct DMAMemChunk *mem, *best = NULL;
    APTR                memaddr = NULL;

    D(kprintf("prometheus.card: allocDMA(%ld)\n", size));

    if (size == 0)
    {
        D(kprintf("prometheus.card: allocDMA() zero size!\n"));
        return NULL;
    }

    size = (size + ALIGNMENT + 3) & ~3;

    ObtainSemaphore(cb->cb_MemSem);

    for (mem = (struct DMAMemChunk *)cb->cb_MemList.mlh_Head; mem->dmc_Node.mln_Succ;
         mem = (struct DMAMemChunk *)mem->dmc_Node.mln_Succ)
    {
        if ((!mem->dmc_Owner) && (mem->dmc_Size >= size) && (mem->dmc_Size < bestsize))
        {
            bestsize = mem->dmc_Size;
            best     = mem;
            if (mem->dmc_Size == size) break;
        }
    }

    if (best)
    {
        if (best->dmc_Size == size)
        {
            best->dmc_Owner = FindTask(NULL);
            memaddr         = best->dmc_Address;
        }
        else
        {
            if (mem = AllocPooled(cb->cb_MemPool, sizeof(struct DMAMemChunk)))
            {
                D(kprintf("prometheus.card: DMC allocated at $%08lx\n", (LONG)mem));
                mem->dmc_Size    = best->dmc_Size - size;
                mem->dmc_Address = (APTR)((ULONG)best->dmc_Address + size);
                mem->dmc_Owner   = NULL;
                best->dmc_Owner  = FindTask(NULL);
                best->dmc_Size   = size;
                Insert((struct List *)&cb->cb_MemList, (struct Node *)mem, (struct Node *)best);
                memaddr = best->dmc_Address;
            }
        }
        memaddr               = (APTR)(((ULONG)memaddr + (ALIGNMENT - 1)) & (-ALIGNMENT));
        best->dmc_AlignedAddr = memaddr;
    }
    ReleaseSemaphore(cb->cb_MemSem);
    D(kprintf("prometheus.card: Allocated DMA at $%08lx with size %ld\n", (LONG)memaddr, size));
    return memaddr;
}

void FreeDMAMemory(__REGA0(APTR membase), __REGD0(ULONG memsize), __REGA6(struct CardBase *cb))
{
    struct Library *    SysBase = cb->cb_SysBase;
    struct DMAMemChunk *mem, *tmem;

    D(kprintf("prometheus.card: freeDMA($%08lx, %ld)\n", (LONG)membase, memsize));
    if (memsize == 0)
    {
        D(kprintf("prometheus.card: freeDMA() zero size!\n"));
        return;
    }

    if (!membase)
    {
        D(kprintf("prometheus.card: freeDMA() NULL pointer!\n"));
        return;
    }

    ObtainSemaphore(cb->cb_MemSem);

    for (mem = (struct DMAMemChunk *)cb->cb_MemList.mlh_Head; mem->dmc_Node.mln_Succ;
         mem = (struct DMAMemChunk *)mem->dmc_Node.mln_Succ)
    {
        if (mem->dmc_AlignedAddr == membase) break;
    }

    if (!mem->dmc_Node.mln_Succ)
    {
        D(kprintf("prometheus.card: freeDMA() block not found!\n"));
        return;
    }

    if (!mem->dmc_Owner)
    {
        D(kprintf("prometheus.card: freeDMA() freed twice!\n"));
        return;
    }

    mem->dmc_Owner = NULL;

    /* merge with predecessor */

    tmem = (struct DMAMemChunk *)mem->dmc_Node.mln_Pred;
    if ((tmem->dmc_Node.mln_Pred) && (!tmem->dmc_Owner))
    {
        mem->dmc_Address = tmem->dmc_Address;
        mem->dmc_Size += tmem->dmc_Size;
        Remove((struct Node *)tmem);
        D(kprintf("prometheus.card: DMC at $%08lx will be freed\n", (LONG)tmem));
        FreePooled(cb->cb_MemPool, tmem, sizeof(struct DMAMemChunk));
    }

    /* merge with successor */

    tmem = (struct DMAMemChunk *)mem->dmc_Node.mln_Succ;
    if ((tmem->dmc_Node.mln_Succ) && (!tmem->dmc_Owner))
    {
        mem->dmc_Size += tmem->dmc_Size;
        Remove((struct Node *)tmem);
        D(kprintf("prometheus.card: DMC at $%08lx will be freed\n", (LONG)tmem));
        FreePooled(cb->cb_MemPool, tmem, sizeof(struct DMAMemChunk));
    }

    ReleaseSemaphore(cb->cb_MemSem);
    return;
}

VOID InitDMAMemory(struct CardBase *cb, APTR memory, ULONG size)
{
    struct Library *    SysBase        = cb->cb_SysBase;
    struct Library *    PrometheusBase = cb->cb_PrometheusBase;
    struct DMAMemChunk *dmc;

    if ((size > 0) && (memory != NULL))
    {
        ObtainSemaphore(cb->cb_MemSem);

        cb->cb_MemList.mlh_Head     = (struct MinNode *)&cb->cb_MemList.mlh_Tail;
        cb->cb_MemList.mlh_Tail     = NULL;
        cb->cb_MemList.mlh_TailPred = (struct MinNode *)&cb->cb_MemList.mlh_Head;

        if (dmc = AllocPooled(cb->cb_MemPool, sizeof(struct DMAMemChunk)))
        {
            dmc->dmc_Size    = size;
            dmc->dmc_Address = memory;
            dmc->dmc_Owner   = NULL;
            AddHead((struct List *)&cb->cb_MemList, (struct Node *)dmc);
        }
        ReleaseSemaphore(cb->cb_MemSem);
    }
}
