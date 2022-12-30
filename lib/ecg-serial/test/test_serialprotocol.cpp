#include <cstring>

#include "test.h"

#include "../src/serialprotocol.cpp"
#include "../src/crc.cpp"

int main() {
    test("transmission", []{
        SerialProtocol pro;
        int pre = 1;
        uint16_t count = 0xffff;
        pro.onPacketArrived =[&](Packet const& packet) {
            // std::cout << "arr " << packet.packetNum() << std::endl;
            if (pre > 0) pre--;
            assert(pre > 0 || packet.packetNum() == count++);
        };
        pro.onPacketReady = [&](Packet const& packet) {
            pro.parseBuffer(std::vector<char>(packet.buffer));
        };

        for (unsigned i = 0; i < 6600; ++i) {
            auto buffer = pro.beginSample();
            for (char s = 0; s < 3*9; ++s) {
                *buffer++ = s;
            }
        }
    });

    test("reordering", []{
        SerialProtocol pro;
        pro.setTransmitOrder(2);
        pro.setReorderBufferSize(32);
        uint16_t count = 0;
        pro.onPacketArrived = [&](Packet const& packet) {
            // std::cout << "arr " << packet.packetNum() << std::endl;
            assert(packet.packetNum() == count++);
        };
        pro.onPacketReady = [&](Packet const& packet) {
            pro.parseBuffer(std::vector<char>(packet.buffer));
        };

        for (unsigned i = 0; i < 66000; ++i) {
            auto buffer = pro.beginSample();
            for (char s = 0; s < 3 * 9; ++s)
                *buffer++ = s;
        }
    });

    test("config", []{
        const char buffer[] = "/d cfg-name 6768696a\n";

        SerialProtocol pro;
        bool callback = false;

        pro.onConfig = [&](std::vector<char>&& name, std::vector<char>&& value) {
            assert(!name.empty());
            assert(!value.empty());
            callback = true;
            assert(std::strncmp(name.data(), "cfg-name", sizeof(buffer)) == 0);
            assert(std::strncmp(value.data(), "ghij", sizeof(buffer)) == 0);
        };

        pro.parseBuffer(std::vector<char>(buffer, buffer + sizeof(buffer)));

        assert(callback);
    });

    test("config escaped", []{
        const char buffer[] = "/d \0\r\n\t cfg\\ name \0\r\n\t 41424344";

        SerialProtocol pro;
        bool callback = false;

        pro.onConfig = [&](std::vector<char>&& name, std::vector<char>&& value) {
            assert(!name.empty());
            assert(!value.empty());
            callback = true;
            assert(std::strncmp(name.data(), "cfg name", sizeof(buffer)) == 0);
            assert(std::strncmp(value.data(), "ABCD", sizeof(buffer)) == 0);
        };

        pro.parseBuffer(std::vector<char>(buffer, buffer + sizeof(buffer)));

        assert(callback);
    });

    test("command", []{
        const char buffer[] = "/c1234567890123456-";

        SerialProtocol pro;
        bool callback = false;

        pro.onCommand = [&](std::vector<char>&& cmd) {
            assert(!cmd.empty());
            callback = true;
            assert(std::strncmp(cmd.data(), "1234567890123456", sizeof(buffer)) == 0);
        };

        pro.parseBuffer(std::vector<char>(buffer, buffer + sizeof(buffer)));

        assert(callback);
    });

    test("command escaped", []{
        const char buffer[] = "/c\\12345678901234567-";

        SerialProtocol pro;
        bool callback = false;

        pro.onCommand = [&](std::vector<char>&& cmd) {
            assert(!cmd.empty());
            callback = true;
            assert(std::strncmp(cmd.data(), "12345678901234567", sizeof(buffer)) == 0);
        };

        pro.parseBuffer(std::vector<char>(buffer, buffer + sizeof(buffer)));

        assert(callback);
    });

    return 0;
}
