/*
** $Id: startup.c,v 1.1 2022/10/20 15:38:08 rkujawa Exp $
*/

#define __NOLIBBASE__

#include <proto/dos.h>
#include <proto/exec.h>

struct Library *SysBase, *DOSBase;

__saveds long _main (void)
 {
  long return_value, wb;
  struct Process *process;
  struct Message *wbmessage = NULL;
  
  SysBase = *(struct Library**)4;
  if (DOSBase = (struct Library*)OpenLibrary ("dos.library", 37))
   {
    process = (struct Process*)FindTask (NULL);
    if (process->pr_CLI) wb = FALSE;
    else
     {
        WaitPort (&process->pr_MsgPort);
        wbmessage = GetMsg (&process->pr_MsgPort);
        wb = TRUE;
     }
    return_value = Main (wb);
    CloseLibrary (DOSBase);
    if (wbmessage)
     {
      Forbid ();
      ReplyMsg (wbmessage);
     }
    return (return_value);
   }
  return (10);
 }
