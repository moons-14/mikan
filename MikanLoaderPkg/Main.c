#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>
#include <stdalign.h>
#include "frame_buffer_config.hpp"
#include "elf.hpp"

void Halt(void)
{
    while (1)
        __asm__("hlt");
}

void CheckError(EFI_STATUS status, CHAR16 *message)
{
    if (EFI_ERROR(status))
    {
        Print(L"Bootloader Error: %s, %r\n", message, status);
        Halt();
    }
}

struct MemoryMap
{
    UINTN buffer_size;
    VOID *buffer;
    UINTN map_size;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
};

EFI_STATUS GetMemoryMap(struct MemoryMap *map)
{
    if (map->buffer == NULL)
    {
        return EFI_BUFFER_TOO_SMALL;
    }

    map->map_size = map->buffer_size;

    return gBS->GetMemoryMap(
        &map->map_size,
        (EFI_MEMORY_DESCRIPTOR *)map->buffer,
        &map->map_key,
        &map->descriptor_size,
        &map->descriptor_version);
}

EFI_STATUS OpenRootDir(EFI_HANDLE ImageHandle, EFI_FILE_PROTOCOL **Root)
{
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;

    gBS->OpenProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&loaded_image,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&fs,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    fs->OpenVolume(fs, Root);

    return EFI_SUCCESS;
}

const CHAR16 *GetMemoryTypeUnicode(EFI_MEMORY_TYPE type)
{
    switch (type)
    {
    case EfiReservedMemoryType:
        return L"EfiReservedMemoryType";
    case EfiLoaderCode:
        return L"EfiLoaderCode";
    case EfiLoaderData:
        return L"EfiLoaderData";
    case EfiBootServicesCode:
        return L"EfiBootServicesCode";
    case EfiBootServicesData:
        return L"EfiBootServicesData";
    case EfiRuntimeServicesCode:
        return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData:
        return L"EfiRuntimeServicesData";
    case EfiConventionalMemory:
        return L"EfiConventionalMemory";
    case EfiUnusableMemory:
        return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory:
        return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS:
        return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO:
        return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace:
        return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode:
        return L"EfiPalCode";
    case EfiPersistentMemory:
        return L"EfiPersistentMemory";
    case EfiMaxMemoryType:
        return L"EfiMaxMemoryType";
    default:
        return L"InvalidMemoryType";
    }
}

EFI_STATUS SaveMemoryMap(struct MemoryMap *map, EFI_FILE_PROTOCOL *file)
{
    CHAR8 buf[256];
    UINTN len;

    CHAR8 *header = "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    file->Write(file, &len, header);

    Print(L"map->buffer = %08lx, map->map_size = %08lx\n", map->buffer, map->map_size);

    EFI_PHYSICAL_ADDRESS iter;
    int i;
    for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
         iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
         iter += map->descriptor_size, i++)
    {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)iter;
        len = AsciiSPrint(buf, sizeof(buf),
                          "%u, %x, %-ls, %08lx, %lx, %lx\n",
                          i, desc->Type, GetMemoryTypeUnicode(desc->Type),
                          desc->PhysicalStart, desc->NumberOfPages,
                          desc->Attribute & 0xffffflu);
        file->Write(file, &len, buf);
    }

    return EFI_SUCCESS;
}

void CalcLoadAddressRange(Elf64_Ehdr *ehdr, UINT64 *first, UINT64 *last)
{
    Elf64_Phdr *phdr = (Elf64_Phdr *)((UINT64)ehdr + ehdr->e_phoff);
    *first = MAX_UINT64;
    *last = 0;
    for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        *first = MIN(*first, phdr[i].p_vaddr);
        *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
    }
}

void CopyLoadSegments(Elf64_Ehdr *ehdr)
{
    Elf64_Phdr *phdr = (Elf64_Phdr *)((UINT64)ehdr + ehdr->e_phoff);
    for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
        CopyMem((VOID *)phdr[i].p_vaddr, (VOID *)segm_in_file, phdr[i].p_filesz);

        UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
        SetMem((VOID *)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
    }
}

EFI_STATUS OpenGOP(EFI_HANDLE ImageHandle, EFI_GRAPHICS_OUTPUT_PROTOCOL **gop)
{
    UINTN num_gop_handles = 0;
    EFI_HANDLE *gop_handles = NULL;
    gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &num_gop_handles, &gop_handles);

    gBS->OpenProtocol(
        gop_handles[0],
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **)gop,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    FreePool(gop_handles);
    return EFI_SUCCESS;
}

const CHAR16 *GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT format)
{
    switch (format)
    {
    case PixelRedGreenBlueReserved8BitPerColor:
        return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
        return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
        return L"PixelBitMask";
    case PixelBltOnly:
        return L"PixelBltOnly";
    case PixelFormatMax:
        return L"PixelFormatMax";
    default:
        return L"InvalidPixelFormat";
    }
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    Print(L"Hello, Mikan World!\n");

    EFI_STATUS status;

    // メモリマップ保存の処理
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    GetMemoryMap(&memmap);

    EFI_FILE_PROTOCOL *root_dir;
    status = OpenRootDir(ImageHandle, &root_dir);
    CheckError(status, L"OpenRootDir");

    EFI_FILE_PROTOCOL *memmap_file;
    status = root_dir->Open(root_dir, &memmap_file, L"\\memmap", EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    CheckError(status, L"Open(memmap)");

    status = SaveMemoryMap(&memmap, memmap_file);
    CheckError(status, L"SaveMemoryMap");
    memmap_file->Close(memmap_file);

    // 画面への描画の処理
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    status = OpenGOP(ImageHandle, &gop);
    CheckError(status, L"OpenGOP");
    Print(L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n",
          gop->Mode->Info->HorizontalResolution,
          gop->Mode->Info->VerticalResolution,
          GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
          gop->Mode->Info->PixelsPerScanLine);
    Print(L"Frame Buffer: 0x%0lx - 0x%0lx, Size: %lu bytes\n",
          gop->Mode->FrameBufferBase,
          gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
          gop->Mode->FrameBufferSize);

    UINT8 *frame_buffer = (UINT8 *)gop->Mode->FrameBufferBase;
    for (UINTN i = 0; i < gop->Mode->FrameBufferSize; ++i)
    {
        frame_buffer[i] = 255;
    }

    // カーネル読み込みの処理
    EFI_FILE_PROTOCOL *kernel_file;
    status = root_dir->Open(root_dir, &kernel_file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
    CheckError(status, L"Open(kernel.elf)");

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
    alignas(alignof(EFI_FILE_INFO)) UINT8 file_info_buffer[file_info_size];
    kernel_file->GetInfo(kernel_file, &gEfiFileInfoGuid, &file_info_size, file_info_buffer);

    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *)file_info_buffer;
    UINTN kernel_file_size = file_info->FileSize;

    VOID *kernel_buff;
    status = gBS->AllocatePool(EfiLoaderData, kernel_file_size, &kernel_buff);
    CheckError(status, L"AllocatePool");
    status = kernel_file->Read(kernel_file, &kernel_file_size, kernel_buff);
    CheckError(status, L"Read(kernel.elf)");

    Elf64_Ehdr *kernel_ehdr = (Elf64_Ehdr *)kernel_buff;
    UINT64 kernel_first_addr, kernel_last_addr;
    CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

    UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, num_pages, &kernel_first_addr);
    CheckError(status, L"AllocatePages");

    CopyLoadSegments(kernel_ehdr);
    Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

    status = gBS->FreePool(kernel_buff);
    CheckError(status, L"FreePool(kernel_buff)");

    // UEFI BIOSを止める処理
    status = gBS->ExitBootServices(ImageHandle, memmap.map_key);
    if (EFI_ERROR(status))
    {
        status = GetMemoryMap(&memmap);
        CheckError(status, L"GetMemoryMap");

        status = gBS->ExitBootServices(ImageHandle, memmap.map_key);
        CheckError(status, L"ExitBootServices");
    }

    // 画面描画の情報をカーネルに渡す
    struct FrameBufferConfig config =
        {
            (UINT8 *)gop->Mode->FrameBufferBase,
            gop->Mode->Info->PixelsPerScanLine,
            gop->Mode->Info->HorizontalResolution,
            gop->Mode->Info->VerticalResolution,
            0,
        };
    switch (gop->Mode->Info->PixelFormat)
    {
    case PixelRedGreenBlueReserved8BitPerColor:
        config.pixel_format = kPixelRGBResv8BitPerColor;
        break;
    case PixelBlueGreenRedReserved8BitPerColor:
        config.pixel_format = kPixelBGRResv8BitPerColor;
        break;
    default:
        Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
        Halt();
    }

    // カーネルを起動
    UINT64 entry_addr = *(UINT64 *)(kernel_first_addr + 24);
    typedef void EntryPointType(const struct FrameBufferConfig *);
    EntryPointType *entry_point = (EntryPointType *)entry_addr;
    entry_point(&config);

    while (1)
        ;
    return EFI_SUCCESS;
}