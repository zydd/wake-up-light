#ifndef CRC_H
#define CRC_H

#include <stdint.h>

extern const uint16_t crc_ccitt_lut[256];

inline uint16_t crc16_update(const uint16_t crc, const char data) {
    return crc_ccitt_lut[uint8_t(data) ^ (crc >> 8)] ^ (crc << 8);
}

inline uint16_t crc16(const char *data, unsigned length, uint16_t crc = 0x0000) {
    while (length--)
        crc = crc16_update(crc, *data++);
    return crc;
}

#endif // CRC_H
