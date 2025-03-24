#ifndef TOOLS_H
#define TOOLS_H
#include <cstdint>
#include <vector>

const uint8_t STX = 0x7E; // start byte
const uint8_t ETX = 0x7F; // end byte

struct SCIPacket {
    uint8_t destSrc;      // Байт Dest/Src
    uint8_t cmd;          // Команда (извлекается из cmdLen)
    uint8_t len;          // Длина данных (извлекается из cmdLen)
    std::vector<uint8_t> data; // Данные пакета
    uint8_t crc;          // CRC
};

#endif // TOOLS_H
