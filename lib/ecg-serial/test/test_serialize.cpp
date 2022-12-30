#include <cstring>

#include "test.h"

#include "../src/serialize.h"

int main() {
    test("fromHex", []{
        char buf[20];

        assert(fromHex("01900aa00ff0", buf));
        assert(std::strcmp(buf, "\x01\x90\x0a\xa0\x0f\xf0") == 0);

        assert(fromHex("0A B0 0C D0 0E F0", buf));
        assert(std::strcmp(buf, "\x0a\xb0\x0c\xd0\x0e\xf0") == 0);

        assert(fromHex("  AB   CD    EF     ", buf));
        assert(std::strcmp(buf, "\xab\xcd\xef") == 0);

        assert(fromHex("000", buf) == 0);
        assert(fromHex("00 0 0", buf) == 0);
        assert(fromHex("0 000", buf) == 0);
        assert(fromHex("ag", buf) == 0);
        assert(fromHex("", buf) == 0);
        assert(fromHex("   0 ", buf) == 0);
    });
    test("toHex", []{
        char buf[20] ;

        toHex("\x00\x01\x80\x81\xff", 5, buf);
        std::cout << buf << std::endl;
        assert(std::strncmp(buf, "00018081ff", 20) == 0);
    });
    test("uint8_t", []{
        char buf[10 + 1] = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
        uint8_t in = 0xdb;
        uint8_t out = 0x00;
        auto size = serialize(in, buf);
        assert(size == sizeof(in));
        assert(size == deserialize(buf, &out));
        assert(in == out);
        assert(buf[size] == '\xff');
    });
    test("char", []{
        char buf[10 + 1] = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
        char in = 0xdb;
        char out = 0x00;
        auto size = serialize(in, buf);
        assert(size == sizeof(in));
        assert(size == deserialize(buf, &out));
        assert(in == out);
        assert(buf[size] == '\xff');
    });
    test("uint16_t", []{
        char buf[10 + 1] = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
        uint16_t in = 0xcadb;
        uint16_t out = 0x00;
        auto size = serialize(in, buf);
        assert(size == sizeof(in));
        assert(size == deserialize(buf, &out));
        assert(in == out);
        assert(buf[size] == '\xff');
    });
    test("uint8_t[]", []{
        char buf[10 + 1] = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
        uint8_t in[5] = "test";
        uint8_t out[5] = {0xdb, 0xdb, 0xdb, 0xdb, 0xdb};
        auto size = serialize(in, buf);
        assert(size == sizeof(in));
        assert(size == deserialize(buf, out));
        for (unsigned i = 0; i < 5; ++i)
            assert(in[i] == out[i]);
        assert(buf[size] == '\xff');
    });
    test("char[]", []{
        char buf[10 + 1] = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
        char in[5] = "test";
        char out[5] = {'\xdb', '\xdb', '\xdb', '\xdb', '\xdb'};
        auto size = serialize(in, buf);
        assert(size == sizeof(in));
        assert(size == deserialize(buf, out));
        assert(std::strncmp(in, out, sizeof(out)) == 0);
        assert(buf[size] == '\xff');
    });
    test("uint16_t[]", []{
        char buf[10 + 1] = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
        uint16_t in[4] = {0x1234, 0x5678, 0x9abc, 0xdef0};
        uint16_t out[4] = {0xdbdb, 0xdbdb, 0xdbdb, 0xdbdb};
        auto size = serialize(in, buf);
        assert(size == sizeof(in));
        assert(size == deserialize(buf, out));
        for (unsigned i = 0; i < 4; ++i)
            assert(in[i] == out[i]);
        assert(buf[size] == '\xff');
    });

    return 0;
}
