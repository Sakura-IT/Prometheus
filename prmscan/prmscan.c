/* Source code of prmscan - program scanning system for PCI cards plugged   */
/* into Prometheus board. It demonstrates use of prometheus.library.        */

#define __NOLIBBASE__ /* we do not want to peeking library bases */

#include <faststring.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/prometheus.h>

struct Library *SysBase, *DOSBase, *PrometheusBase;

const char *VString = "$VER: prmscan 1.6 (14.9.2002) by Grzegorz Kraszewski.\n";

UWORD HexStrToWord(UBYTE *buffer)
{
    WORD  i;
    UWORD result = 0, ch;

    for (i = 0; i < 4; i++)
    {
        result <<= 4;
        ch = buffer[i];
        if (ch >= '0' && ch <= '9')
            ch -= 48;
        else if (ch >= 'A' && ch <= 'F')
            ch -= 55;
        else if (ch >= 'a' && ch <= 'f')
            ch -= 87;
        else
            return 0xFFFF;
        result += ch;
    }
    return result;
}

void ShiftBufferLeft(UBYTE *buffer, WORD shift)
{
    UBYTE *ptr = buffer;

    while (ptr[shift])
    {
        ptr[0] = ptr[shift];
        ptr++;
    }
}

void StripLeadingSpaces(UBYTE *buffer)
{
    WORD idx = 0;

    while (buffer[idx] == 0x09 || buffer[idx] == 0x20) idx++;
    if (idx) ShiftBufferLeft(buffer, idx);
}

void RemoveEOL(UBYTE *buffer)
{
    UBYTE *ptr = buffer;
    while (*buffer++ != 0x0A)
        ;
    *--buffer = 0x00;
}

LONG GetVendorString(BPTR vendors_file, UWORD vendor, UBYTE *buffer, LONG bufsize)
{
    LONG error;

    if (vendors_file)
    {
        Seek(vendors_file, 0, OFFSET_BEGINNING);
        while (FGets(vendors_file, buffer, bufsize))
        {
            if (*buffer == ';') continue;  /* comment */
            if (*buffer == 0x09) continue; /* device description */
            if (*buffer == 0x20) continue;
            if (HexStrToWord(buffer) == vendor)
            {
                ShiftBufferLeft(buffer, 5);
                StripLeadingSpaces(buffer);
                RemoveEOL(buffer);
                return 0;
            }
        }
        if (error = IoErr()) return error;
    }
    else
        sprintf(buffer, "unknown ($%04lx)", vendor);
    return 0;
}

LONG GetDeviceString(BPTR vendors_file, UWORD device, UBYTE *buffer, LONG bufsize)
{
    LONG error;

    if (device != 0xFFFF)
    {
        if (vendors_file)
        {
            while (FGets(vendors_file, buffer, bufsize))
            {
                if (*buffer == ';') continue;                  /* comment */
                if (*buffer != 0x09 && *buffer != 0x20) break; /* vendor -> dev not found */
                StripLeadingSpaces(buffer);
                if (HexStrToWord(buffer) == device)
                {
                    ShiftBufferLeft(buffer, 5);
                    StripLeadingSpaces(buffer);
                    RemoveEOL(buffer);
                    return 0;
                }
            }
            if (error = IoErr()) return error;
        }
    }
    sprintf(buffer, "unknown ($%04lx)", device);
    return 0;
}

LONG Main(void)
{
    UBYTE vendor_name[80];
    UBYTE device_name[80];

    Printf("\nPrmscan 1.6 by Grzegorz Kraszewski.\n");
    Printf("PCI cards listing:\n-------------------------------------------------\n");
    if (PrometheusBase = OpenLibrary("prometheus.library", 2))
    {
        APTR         board = NULL;
        ULONG        vendor, device, revision, dclass, dsubclass, blkaddr, blksize, slot, function;
        ULONG        romaddr, romsize;
        WORD         blk;
        BPTR         vendors_file;
        struct Node *owner;
        STRPTR       owner_name = "NONE";

        vendors_file = Open("DEVS:PCI/vendors.txt", MODE_OLDFILE);

        while (board = Prm_FindBoardTags(board, TAG_END))
        {
            Prm_GetBoardAttrsTags(board,
                                  PRM_Vendor,
                                  (ULONG)&vendor,
                                  PRM_Device,
                                  (ULONG)&device,
                                  PRM_Revision,
                                  (ULONG)&revision,
                                  PRM_Class,
                                  (ULONG)&dclass,
                                  PRM_SubClass,
                                  (ULONG)&dsubclass,
                                  PRM_SlotNumber,
                                  (ULONG)&slot,
                                  PRM_FunctionNumber,
                                  (ULONG)&function,
                                  PRM_BoardOwner,
                                  (ULONG)&owner,
                                  TAG_END);
            GetVendorString(vendors_file, vendor, vendor_name, 80);
            GetDeviceString(vendors_file, device, device_name, 80);
            Printf("Board in slot %ld, function %ld\n", slot, function);
            Printf("Vendor: \033[1m%s\033[0m\nDevice: \033[1m%s\033[0m\nRevision: %ld.\n",
                   (LONG)vendor_name,
                   (LONG)device_name,
                   revision);
            Printf("Device class %02lx, subclass %02lx.\n", dclass, dsubclass);
            for (blk = 0; blk < 6; blk++)
            {
                Prm_GetBoardAttrsTags(
                    board, PRM_MemoryAddr0 + blk, (ULONG)&blkaddr, PRM_MemorySize0 + blk, (ULONG)&blksize, TAG_END);
                if (blkaddr && blksize)
                {
                    ULONG  normsize = blksize;
                    STRPTR unit     = "B";

                    if (normsize >= 1024)
                    {
                        normsize >>= 10;
                        unit = "kB";
                    }

                    if (normsize >= 1024)
                    {
                        normsize >>= 10;
                        unit = "MB";
                    }

                    Printf("Address range: %08lx - %08lx (%ld %s).\n",
                           blkaddr,
                           blkaddr + blksize - 1,
                           normsize,
                           (LONG)unit);
                }
            }
            Prm_GetBoardAttrsTags(board, PRM_ROM_Address, (ULONG)&romaddr, PRM_ROM_Size, (ULONG)&romsize, TAG_END);
            if (romaddr && romsize)
            {
                Printf("%ld kB of ROM at %08lx - %08lx.\n", romsize >> 10, romaddr, romaddr + romsize - 1);
            }
            if (owner)
            {
                if (owner->ln_Name)
                    owner_name = owner->ln_Name;
                else
                    owner_name = "unknown";
            }
            else
                owner_name = "NONE";
            Printf("Board driver: \033[32m%s\033[0m.\n", (ULONG)owner_name);
            Printf("-------------------------------------------------\n");
        }

        if (vendors_file) Close(vendors_file);

        Printf("\n");
        CloseLibrary(PrometheusBase);
    }
    else
        Printf("Can't open prometheus.library v2.\n\n");
    return 0;
}
