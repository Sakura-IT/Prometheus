/* Program for testing DMA functions and DMA memory */

#define __NOLIBBASE__

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/prometheus.h>

#define PCI_COMMAND     0x04
#define PCI_STATUS      0x06


extern struct Library *SysBase, *DOSBase;

struct Library *PrometheusBase;

const UBYTE VString[] = "$VER: RTL8139test 1.0 (13.9.2002) © by Matay 2002";

int main (void)
  {
    PCIBoard *b;
    UWORD s;

    Printf("RTL8139 test.\n");
    if (PrometheusBase = OpenLibrary("prometheus.library", 2))
      {
        Printf("prometheus.library v2 base at $%08lx\n", (LONG)PrometheusBase);
        if (b = Prm_FindBoardTags(NULL,
          PRM_Vendor, 0x10EC,
          PRM_Device, 0x8139,
          TAG_END))
          {
            Printf("RTL8139 board at $%08lx.\n", (ULONG)b);
            s = Prm_ReadConfigWord(b, PCI_COMMAND);
            Printf("conf: %04lx\n", s);
            s = Prm_ReadConfigWord(b, PCI_STATUS);
            Printf("stat: %04lx\n", s);
            s = Prm_ReadConfigWord(b, 0);
            Printf("vend: %04lx\n", s);
            s = Prm_ReadConfigWord(b, 2);
            Printf("devc: %04lx\n", s);
            s = Prm_ReadConfigByte(b, 62);
            Printf("intr: %02lx\n", s);
          }
        else Printf("No RTL8139 board found.\n");
        CloseLibrary(PrometheusBase);
      }
    else Printf("Can't open prometheus.library v2+\n");
  }



