#ifndef PACKET_H
#define PACKET_H

#include <vector>

#include "crc.h"
#include "serialize.h"

struct Packet {
    static constexpr int header = 4;
    static constexpr int channels = 9;
    static constexpr int samples = 1;
    static constexpr int precision = 3;
    static constexpr int tail = 2;
    static constexpr int size = header + channels * samples * precision + tail;

    std::vector<char> buffer;

    inline Packet(): buffer(std::vector<char>(size)) {
        buffer[0] = '/'; buffer[1] = 'R'; }

    inline Packet(uint16_t num): Packet() {
        serialize(num, &buffer[2]); }

    inline Packet(std::vector<char>&& data): buffer(std::move(data)) { }

    inline char *data() {
        return buffer.data(); }

    inline const char *data() const {
        return buffer.data(); }

    inline uint16_t packetNum() const {
        return (uint8_t(buffer[2]) << 8) | uint8_t(buffer[3]); }

    inline void setPacketNum(uint16_t num) {
        serialize(num, &buffer[2]); }

    inline uint16_t packetCrc() const {
        return uint8_t(buffer[size - 2]) << 8 | uint8_t(buffer[size - 1]); }

    inline void setPacketCrc(uint16_t crc) {
        serialize(crc, &buffer[buffer.size() - 2]); }

    inline bool checkCrc() const {
        return packetCrc() == crc(); }

    inline uint16_t crc() const {
        return crc16(buffer.data(), buffer.size() - 2); }

    inline int32_t rawSample(unsigned channel, unsigned t);
    inline void setRawSample(unsigned channel, unsigned t, uint32_t sample);
    inline double sample(unsigned channel, unsigned t);
    inline void setSample(unsigned channel, unsigned t, double normVal);
};

inline int32_t Packet::rawSample(unsigned channel, unsigned t) {
    const unsigned pos = header + (channels * t + channel) * precision;
    int32_t sample = 0;

    for (unsigned i = 0; i < precision; ++i)
        sample = (sample << 8) | uint8_t(buffer[pos + i]);

    if (sample >> (precision * 8 - 1))
        sample |= 0xffffffff << (precision * 8);

    return sample;
}

inline void Packet::setRawSample(unsigned channel, unsigned t, uint32_t sample) {
    const unsigned pos = header + (channels * t + channel) * precision;
    for (unsigned i = 0; i < precision; ++i)
        buffer[pos + i] = (sample >> ((precision - 1 - i) * 8)) & 0xff;
}

inline double Packet::sample(unsigned channel, unsigned t) {
    auto sample = rawSample(channel, t);
    return double(sample) / (1 << (precision * 8 - 1));
}

inline void Packet::setSample(unsigned channel, unsigned t, double normVal) {
    auto sample = int32_t(normVal * ((1 << (precision * 8 - 1)) - 1));
    setRawSample(channel, t, sample);
}

#endif // PACKET_H
