#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"

char console_buf[sizeof(Console)];
Console *console;

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter *pixel_writer;

const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
};
const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

void operator delete(void *obj) noexcept
{
}

extern "C" void
KernelMain(const FrameBufferConfig &frame_buffer_config)
{
    SetLogLevel(kDebug);
    switch (frame_buffer_config.pixel_format)
    {
    case kPixelRGBResv8BitPerColor:
        pixel_writer = new (pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    case kPixelBGRResv8BitPerColor:
        pixel_writer = new (pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    }

    const int kFrameWidth = frame_buffer_config.horizonal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;
    FillRectangle(*pixel_writer, {0, 0}, {kFrameWidth, kFrameHeight - 50}, kDesktopBGColor);
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth, 50}, {1, 8, 17});
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50}, {80, 80, 80});
    DrawRectangle(*pixel_writer, {10, kFrameHeight - 40}, {30, 30}, {160, 160, 160});

    console = new (console_buf) Console{*pixel_writer, {0, 0, 0}, {255, 255, 255}};

    Log(kInfo, "Hello Mikan OS!\n");
    Log(kDebug, "Horizonal: %d, Vertical: %d\n", frame_buffer_config.horizonal_resolution, frame_buffer_config.vertical_resolution);

    // カーソル表示
    for (int dy = 0; dy < kMouseCursorHeight; ++dy)
    {
        for (int dx = 0; dx < kMouseCursorWidth; ++dx)
        {
            if (mouse_cursor_shape[dy][dx] == '@')
            {
                pixel_writer->Write(200 + dx, 100 + dy, {0, 0, 0});
            }
            else if (mouse_cursor_shape[dy][dx] == '.')
            {
                pixel_writer->Write(200 + dx, 100 + dy, {255, 255, 255});
            }
        }
    }

    auto err = pci::ScanAllBus();
    Log(kDebug, "ScanAllBus: %s\n", err.Name());
    for (int i = 0; i < pci::num_device; ++i)
    {
        const auto &dev = pci::devices[i];
        auto vender_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        Log(kDebug, "%d.%d.%d: vend %04x,class %08x (base %02x, sub %02x, interface %02x),head %02x\n", dev.bus, dev.device, dev.function, vender_id, class_code, dev.class_code.base, dev.class_code.sub, dev.class_code.interface, dev.header_type);
    }

    // xHCを探す intel製を優先
    pci::Device *xhc_dev = nullptr;
    for (int i = 0; i < pci::num_device; ++i)
    {
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u))
        {
            xhc_dev = &pci::devices[i];

            if (0x8086 == pci::ReadVendorId(*xhc_dev))
            {
                break;
            }
        }
    }

    if (xhc_dev)
    {
        Log(kInfo, "xHC has been found: %d.%d.%d\n",
            xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }

    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kDebug, "Read Base Address Register: %s\n", xhc_bar.error.Name());
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf); // 下位4bitを0にする
    Log(kDebug, "xHC Memory Mapped IO Base Address = %08lx\n", xhc_mmio_base);

    while (1)
        __asm__("hlt");
}