#include <cstdint>

extern "C" void KernelMain(uint64_t frame_buffer_base, uint64_t frame_buffer_size)
{
    uint8_t *frame_bnuffer = reinterpret_cast<uint8_t *>(frame_buffer_base);
    for (int i = 0; i < frame_buffer_size; ++i)
    {
        frame_bnuffer[i] = i % 256;
    }
    while (1)
        __asm__("hlt");
}