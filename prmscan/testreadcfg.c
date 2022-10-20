/* Test of ReadConfigXxxx() functions */

#define __NOLIBBASE__  /* we do not want to peeking library bases */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/prometheus.h>

struct Library *SysBase, *DOSBase, *PrometheusBase;

const char *VString = "$VER: testreadcfg 1.0 (5.1.2002) © 2002 Matay.\n";


LONG main (void)
 {
  Printf ("\ntestreadcfg 1.0 © 2002 Matay.\n");
  if (PrometheusBase = OpenLibrary ("prometheus.library", 2))
   {
    APTR board = NULL;

    while (board = Prm_FindBoardTags(board,
      PRM_Vendor, 0x10EC,
      PRM_Device, 0x8139,
      TAG_END))
      {
       Printf("$%08lx\n", Prm_ReadConfigLong(board, 0));
       Printf("$%04lx $%04lx\n", Prm_ReadConfigWord(board, 0), Prm_ReadConfigWord(board, 2));
       Printf("$%02lx $%02lx $%02lx $%02lx\n", Prm_ReadConfigByte(board, 0),
         Prm_ReadConfigByte(board, 1), Prm_ReadConfigByte(board, 2),
         Prm_ReadConfigByte(board, 3));
       Printf("-------------------------------------------------\n");
       Printf("command word: %04lx\n", Prm_ReadConfigByte(board, 7));
       Prm_WriteConfigByte(board, 0x00, 7);
       CacheClearU();
       Delay(20);
       Printf("command word: %04lx\n", Prm_ReadConfigByte(board, 7));
       Prm_WriteConfigByte(board, 0x07, 7);
       Delay(20);
       Printf("command word: %04lx\n", Prm_ReadConfigByte(board, 7));
      }
    Printf("\n");
    CloseLibrary(PrometheusBase);
   }
  else Printf ("prometheus.library V2 not found or initialization failed.\n\n");
  return 0;
 }

