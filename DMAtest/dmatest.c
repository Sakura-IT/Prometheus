/* Program for testing DMA functions and DMA memory */

#define __NOLIBBASE__

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/prometheus.h>

extern struct Library *SysBase, *DOSBase;

struct Library *PrometheusBase, *CardBase;

const UBYTE VString[] = "$VER: dmalist 1.2 (6.10.2002) © by Matay 2002";

/* private structures */

struct DMAMemChunk
{
    struct MinNode dmc_Node;    /* for linking into list */
    APTR           dmc_Address; /* logical (CPU) address */
    struct Task *  dmc_Owner;   /* memory owner (NULL if none) */
    ULONG          dmc_Size;    /* chunk size in bytes */
};

struct CardBase
{
    struct Library  cb_Library;
    UBYTE           cb_Flags;
    UBYTE           cb_Pad;
    struct Library *cb_SysBase;
    struct Library *cb_ExpansionBase;
    APTR            cb_SegList;
    STRPTR          cb_Name;

    /* standard fields ends here */

    struct Library *        cb_PrometheusBase;
    BOOL                    cb_DMAMemGranted;
    APTR                    cb_LegacyIOBase;
    APTR                    cb_MemPool; /* for DMA memory management lists */
    struct MinList          cb_MemList;
    struct SignalSemaphore *cb_MemSem;
};

BOOL try_memory(void)
{
    APTR pointer;

    if (pointer = Prm_AllocDMABuffer(4))
    {
        Prm_FreeDMABuffer(pointer, 4);
        return TRUE;
    }
    return FALSE;
}

void dump_list(struct CardBase *cb)
{
    struct DMAMemChunk *dmc;

    PutStr(
        "\nStart      End        Size\tOwner\n"
        "---------------------------------------------------------------\n");

    ObtainSemaphoreShared(cb->cb_MemSem);

    for (dmc = (struct DMAMemChunk *)cb->cb_MemList.mlh_Head; dmc->dmc_Node.mln_Succ;
         dmc = (struct DMAMemChunk *)dmc->dmc_Node.mln_Succ)
    {
        STRPTR owner;

        if (dmc->dmc_Owner)
            owner = dmc->dmc_Owner->tc_Node.ln_Name;
        else
            owner = "FREE";

        Printf("$%08lx  $%08lx  %ld\t%s\n",
               (ULONG)dmc->dmc_Address,
               (ULONG)dmc->dmc_Address + dmc->dmc_Size - 1,
               dmc->dmc_Size,
               (LONG)owner);
    }

    ReleaseSemaphore(cb->cb_MemSem);
    PutStr("---------------------------------------------------------------\n\n");
    return;
}

int main(void)
{
    APTR b;

    Printf("Prometheus DMA memory lister v 1.2\n");
    if (PrometheusBase = OpenLibrary("prometheus.library", 2))
    {
        if (CardBase = OpenLibrary("Prometheus.card", 7))
        {
            if (try_memory())
            {
                Printf("DMA memory detected, block listing...\n");
                dump_list((struct CardBase *)CardBase);
            }
            else
                Printf("No free DMA memory found.\n");
            CloseLibrary(CardBase);
        }
        else
            Printf("Prometheus is not upgraded yet.\n");
        CloseLibrary(PrometheusBase);
    }
    else
        Printf("Can't open prometheus.library v2+\n");
}
