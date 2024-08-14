#pragma once

#include <cstdint>
#include <array>
#include "error.hpp"

namespace pci
{

    // CONFIG_ADDRESSレジスタのIOポートアドレス
    const uint16_t kConfigAddress = 0x0cf8;
    const uint16_t kConfigData = 0x0cfc;

    struct ClassCode
    {
        uint8_t base, sub, interface;

        // ベースクラスが等しい場合に真を返す
        bool Match(uint8_t b) { return b == base; }
        // ベースクラスとサブクラスが等しい場合に真を返す
        bool Match(uint8_t b, uint8_t s) { return Match(b) && s == sub; }
        // ベースクラス、サブクラス、インターフェイスが等しい場合に真を返す
        bool Match(uint8_t b, uint8_t s, uint8_t i) { return Match(b, s) && i == interface; }
    };

    struct Device
    {
        uint8_t bus, device, function, header_type;
        ClassCode class_code;
    };

    // ScanAllBus()で見つかったPCIデバイスの一覧
    inline std::array<Device, 32> devices;
    // devicesの有効な要素の数
    inline int num_device;

    uint32_t MakeAddress(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_addr);
    void WriteAddress(uint32_t address);
    void WriteData(uint32_t value);
    uint32_t ReadData();
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);
    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

    inline uint16_t ReadVendorId(const Device &dev)
    {
        return ReadVendorId(dev.bus, dev.device, dev.function);
    }

    bool IsSingleFunctionDevice(uint8_t header_type);

    Error ScanAllBus();
    Error ScanBus(uint8_t bus);
    Error ScanDevice(uint8_t bus, uint8_t device);
    Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function);

    Error AddDevice(const Device &dev);
};