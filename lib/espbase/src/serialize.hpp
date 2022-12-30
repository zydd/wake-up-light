#pragma once

#include <array>
#include <cstdint>

namespace espbase {

using std::size_t;

inline uint8_t from_hex(const char i) {
    return (i >= 'a') ? i - 'a' + 10 :
           (i >= 'A') ? i - 'A' + 10 :
           (i >= '0') ? i - '0' : 0xff;
}

inline char to_hex(const uint8_t i) {
    return (i >= 16) ? '\0' :
           (i >= 10) ? i - 10 + 'a' :
                       i + '0';
}

inline size_t from_hex(const char *in, char *out) {
    size_t count = 0;
    while (true) {
        while (*in == ' ') in++;
        if (*in == '\0') break;
        uint8_t h = from_hex(*in++);

        if (*in == '\0') return 0;
        uint8_t l = from_hex(*in++);

        if (h > 0xf || l > 0xf)
            return 0;

        *out++ = h << 4 | l;
        count++;
    }

    *out = '\0';
    return count;
}

inline void to_hex(const char *in, const size_t in_size, char *out) {
    out = &out[2 * in_size];
    *out = '\0';

    auto itr = &in[in_size];
    while (--itr >= in) {
        *--out = to_hex(*itr & 0xf);
        *--out = to_hex((uint8_t(*itr) >> 4));
    }
}

inline size_t serialize(const uint32_t in, char *out) {
    out[0] = in >> 24;
    out[1] = in >> 16;
    out[2] = in >> 8;
    out[3] = in;
    return 4;
}

inline size_t serialize(const int32_t in, char *out) {
    out[0] = in >> 24;
    out[1] = in >> 16;
    out[2] = in >> 8;
    out[3] = in;
    return 4;
}

inline size_t serialize(const uint16_t in, char *out) {
    out[0] = in >> 8;
    out[1] = in;
    return 2;
}

inline size_t serialize(const char in, char *out) {
    out[0] = in;
    return 1;
}

inline size_t serialize(const uint8_t in, char *out) {
    out[0] = in;
    return 1;
}

inline size_t serialize(const bool in, char *out) {
    out[0] = in;
    return 1;
}

template<class T>
size_t deserialize(const char *in, T& out);

template<>
inline size_t deserialize(const char *in, uint32_t &out) {
    out = uint8_t(in[0]) << 24 | uint8_t(in[1]) << 16 | uint8_t(in[2]) << 8 | uint8_t(in[3]);
    return 4;
}

template<>
inline size_t deserialize(const char *in, int32_t &out) {
    out = uint8_t(in[0]) << 24 | uint8_t(in[1]) << 16 | uint8_t(in[2]) << 8 | uint8_t(in[3]);
    return 4;
}

template<>
inline size_t deserialize(const char *in, uint16_t &out) {
    out = uint8_t(in[0]) << 8 | uint8_t(in[1]);
    return 2;
}

template<>
inline size_t deserialize(const char *in, char &out) {
    out = in[0];
    return 1;
}

template<>
inline size_t deserialize(const char *in, uint8_t &out) {
    out = uint8_t(in[0]);
    return 1;
}

template<>
inline size_t deserialize(const char *in, bool &out) {
    out = bool(in[0]);
    return 1;
}


template<class T, size_t Size>
inline size_t serialize(const T(&in)[Size], char *out) {
    size_t size = 0;
    for (size_t i = 0; i < Size; ++i)
        size += serialize(in[i], &out[size]);
    return size;
}

template<class T, size_t Size>
inline size_t serialize(std::array<T, Size> const& in, char *out) {
    size_t size = 0;
    for (size_t i = 0; i < Size; ++i)
        size += serialize(in[i], &out[size]);
    return size;
}

template<class T, size_t Size>
inline size_t deserialize(const char *in, T(&out)[Size]) {
    size_t size = 0;
    for (size_t i = 0; i < Size; ++i)
        size += deserialize(&in[size], out[i]);
    return size;
}

template<class T, size_t Size>
inline size_t deserialize(const char *in, std::array<T, Size> &out) {
    size_t size = 0;
    for (size_t i = 0; i < Size; ++i)
        size += deserialize(&in[size], out[i]);
    return size;
}

}
