#include <cassert>
#include <cstring>
#include <iostream>

#include "../src/configbase.h"
#include "../src/configbase.cpp"

ConfigBase::meta config_meta;
struct Config : ConfigBase {
    member<uint16_t> u16_1234   = {&config_meta, "u16_1234", 0x1234};
    member<uint8_t>  u8_ef      = {&config_meta, "u8_ef",      0xef};
    member<uint16_t> u16_abcd   = {&config_meta, "u16_abcd", 0xabcd};
};
struct Config2 : ConfigBase {
    Config2():
        test{&config_meta, "u16_1234", 0x1234}
    { }
    member<uint16_t> test = {&config_meta, "u16_1234", 0x1234};
};

int main() {
    char eeprom[sizeof(Config)];
    std::memset(eeprom, 0xdb, sizeof(eeprom));

    Config& config = *new(eeprom) Config;
    assert((void *) eeprom == (void *) &config);
    assert(sizeof(config) <= 6);

    int count_in = 0, count_out = 0;
    config_meta.onChange("u16_1234", [&]{ ++count_in; });
    config_meta.onChange("u16_abcd", [&]{ ++count_in; });
    config_meta.onChange("u8_ef", [&]{ ++count_in; });

    config_meta.notifyAll(); count_out += 3;

    // uninitialized
    assert(config.u16_1234 == 0xdbdb);
    assert(config.u16_abcd == 0xdbdb);
    assert(config.u8_ef == 0xdb);

    config_meta.reset("u16_1234"); ++count_out;
    config_meta.reset("u16_abcd"); ++count_out;
    config_meta.reset("u8_ef"); ++count_out;
    assert(config.u16_1234 == 0x1234);
    assert(config.u16_abcd == 0xabcd);
    assert(config.u8_ef == 0xef);

    char buf[2];
    uint16_t u16;
    uint8_t u8;

    config_meta.get("u16_1234", buf);
    u16 = 0; deserialize(buf, &u16);
    assert(u16 == 0x1234);

    config_meta.get("u16_abcd", buf);
    u16 = 0; deserialize(buf, &u16);
    assert(u16 == 0xabcd);

    config_meta.get("u8_ef", buf);
    u8 = 0; deserialize(buf, &u8);
    assert(u8 == 0xef);

    config_meta.set("u16_1234", "\x43\x21"); ++count_out;
    config_meta.set("u16_abcd", "\xdc\xba"); ++count_out;
    config_meta.set("u8_ef", "\xfe"); ++count_out;
    assert(config.u16_1234 == 0x4321);
    assert(config.u16_abcd == 0xdcba);
    assert(config.u8_ef == 0xfe);

    config_meta.get("u16_1234", buf);
    u16 = 0; deserialize(buf, &u16);
    assert(u16 == 0x4321);

    config_meta.get("u16_abcd", buf);
    u16 = 0; deserialize(buf, &u16);
    assert(u16 == 0xdcba);

    config_meta.get("u8_ef", buf);
    u8 = 0; deserialize(buf, &u8);
    assert(u8 == 0xfe);

    assert(count_in == count_out);

    std::memset(eeprom, 0xdb, sizeof(eeprom));
    Config2& config2 = *new(eeprom) Config2();
    assert((void *) eeprom == (void *) &config2);
    assert(config2.test == 0xdbdb);

    std::cout << "success" << std::endl;

    return 0;
}
