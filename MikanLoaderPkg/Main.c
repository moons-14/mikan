#include <Uefi.h>
#include <Library/UefiLib.h>

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    Print(L"Hello, Mikan World!\n");

    while (1)
        ;
    return EFI_SUCCESS;
}