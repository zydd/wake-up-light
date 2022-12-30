#pragma once

#include <vector>

#include "configbase.hpp"

namespace espbase {

struct config : config_base {
    // member<uint16_t> config_http;
    member<char[8]>  header;
    member<uint16_t> config_port;
    member<char[32]> ssid;
    member<char[64]> password;
    member<char[32]> hostname;
    member<uint8_t>  wifi_mode;
    member<bool>     debug;

    config(meta *meta);

    static config *instance();
};


void setup();
void loop();
int print(const char *format, ...);
void command(const char *name, std::function<void()>&& callback);
void command(const char *name, std::function<void(std::vector<std::vector<char>>&&)>&& callback);
void parse_command(std::vector<char>&& buf);
void start_wifi();
const char *device_id();

extern Stream *dbg;

extern std::function<void()> on_wifi_connected;

}
