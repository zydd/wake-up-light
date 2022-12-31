#include <cassert>
#include <cstdarg>

#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiServer.h>

#include "protocol.hpp"

#include "espbase.h"

std::function<void()> espbase::on_wifi_connected;

static bool wifi_connected; 
static WiFiServer server(0);
static WiFiClient tcp;
static std::vector<char> buffer;

Stream *espbase::dbg = nullptr;

using espbase::dbg;


static std::map<std::string, std::function<void(std::vector<std::vector<char>>&&)>> s_command_map;
static protocol s_protocol;
static espbase::config *s_config = nullptr;
static espbase::config::meta *s_config_meta = nullptr;

const char *espbase::device_id() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    static char device_id[] = "0000";
    static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    device_id[0] = base64[mac[3] >> 2];
    device_id[1] = base64[(mac[3] & 0x03) << 4 | mac[4] >> 4];
    device_id[2] = base64[(mac[4] & 0x0f) << 2 | mac[5] >> 6];
    device_id[3] = base64[mac[5] & 0x3f];
    return device_id;
}

espbase::config::config(espbase::config::meta *meta):
    header      {meta, "header",        "espbase"},
    config_port {meta, "config_port",   8266},
    ssid        {meta, "ssid",          {}},
    password    {meta, "password",      {}},
    hostname    {meta, "hostname",      {}},
    wifi_mode   {meta, "wifi_mode",     uint8_t(WiFiMode::WIFI_OFF)},
    debug       {meta, "debug",         false}
{
    assert(s_config == nullptr);
    assert(s_config_meta == nullptr);
    s_config = this;
    s_config_meta = meta;

    std::snprintf(meta->metadata("ssid")->default_value, sizeof(ssid), "esp-%s", espbase::device_id());
    std::snprintf(meta->metadata("hostname")->default_value, sizeof(ssid), "esp-%s", espbase::device_id());
}

void espbase::command(const char *name, std::function<void()>&& callback) {
    command(name, [callback](std::vector<std::vector<char>>&&){ callback(); });
}

void espbase::command(const char *name, std::function<void(std::vector<std::vector<char>>&&)>&& callback) {
    if (s_command_map.find(name) != s_command_map.end())
        print(__FILE__ ": overriding command '%s'\n", name);
    s_command_map[name] = callback;
}

void espbase::parse_command(std::vector<char>&& buf) {
    s_protocol.parse(std::move(buf));
}

static void init_commands() {
    s_protocol.command_callback = [](std::vector<char>&& command, std::vector<std::vector<char>>&& args) {
        if (command.size() <= 1) {
            espbase::print("esp-%s\n", espbase::device_id());
            return;
        }

        auto itr = s_command_map.find(command.data());

        if (itr != s_command_map.end() && itr->second) {
            espbase::print("command: %s\n", itr->first.c_str());
            (itr->second)(std::move(args));
            espbase::print("\n");
        } else {
            espbase::print("unknown: %s\n\n", command.data());
        }
    };

    using espbase::command;

    command("reset-config", []{ s_config_meta->reset_all(); });
    command("config", [](std::vector<std::vector<char>>&& args) {
        if (args.size() == 2) {
            auto& opt = args[0];
            auto& value = args[1];
            opt.push_back('\0');

            auto ex_size = s_config_meta->size(opt.data());

            if (value.size() != ex_size)
                espbase::print("warning: expected %u, got %u\n", ex_size, value.size());

            value.resize(ex_size, '\0');

            if (s_config_meta->set(opt.data(), value.data()))
                espbase::print("set %s\n", opt.data());
            else
                espbase::print("config error: %s\n", opt.data());
        } else {
            for (auto const &itr : s_config_meta->members()) {
                std::vector<char> value(itr.second->type_info->size);
                if (value.size() == 0)
                    continue;

                itr.second->get(value.data());

                espbase::print("config %s ", itr.first.c_str());

                bool is_printable = itr.second->type_info->element_info == espbase::type_info<char>();
                unsigned i = 0;
                for (; i < value.size() && value[i]; ++i)
                    is_printable &= (value[i] >= 0x20) && (value[i] < 0x80);
                for (; i < value.size(); ++i)
                    is_printable &= (value[i] == '\0');

                if (is_printable) { // FIXME: escape
                    value.push_back('\0');
                    espbase::print("'%s'", value.data());
                } else {
                    espbase::print("0x");
                    for (char v : value)
                        espbase::print("%c%c", espbase::to_hex(v >> 4), espbase::to_hex(v & 0xf));
                }
                espbase::print("\n");
            }
        }
    });
    command("commands", []{
        for (auto const &itr : s_command_map)
            espbase::print("%s\n", itr.first.c_str());
    });
    command("device-name", []{ espbase::print("esp-%s\n", espbase::device_id()); });

//     meta->onChange("builtin_led", []{
//         digitalWrite(LED_BUILTIN, config->builtin_led);
//     });
// }

// void EspBase::setup() {
    // command("commit", []{
    //     EEPROM.commit();
    // });
    command("restart", []{
        ESP.restart();
    });
//     command("toggle-led", []{
//         digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
//         return 0;
//     });

    command("wifi", []{
        if (!dbg)
            return;

        dbg->print("isConnected: ");
        dbg->println(WiFi.isConnected());

        dbg->print("localIP: ");
        dbg->println(WiFi.localIP().toString());

        dbg->print("MAC: ");
        dbg->println(WiFi.macAddress());
    });
    command("wifi-scan", []{
        if (!dbg)
            STREAM_READ_RETURNS_INT;

        dbg->println("scanning...");
        int count = WiFi.scanNetworks();
        if (count < 0)
            return;

        for (int i = 0; i < count; ++i) {
            dbg->printf("%2d %2d dBm ", (i + 1), WiFi.RSSI(i));
            switch (WiFi.encryptionType(i)) {
            case ENC_TYPE_WEP:  dbg->print("WEP "); break;
            case ENC_TYPE_TKIP: dbg->print("WPA "); break;
            case ENC_TYPE_CCMP: dbg->print("WPA2"); break;
            case ENC_TYPE_NONE: dbg->print("None"); break;
            case ENC_TYPE_AUTO: dbg->print("Auto"); break;
            }
            dbg->print(' ');
            dbg->println(WiFi.SSID(i));
        }
    });
    command("wifi-start", []{
        espbase::start_wifi();
    });
}


void espbase::start_wifi() {
    switch (::WiFiMode(s_config->wifi_mode.value)) {
    default:
    case ::WiFiMode::WIFI_OFF:
        WiFi.mode(::WiFiMode::WIFI_OFF);
        break;
    case ::WiFiMode::WIFI_STA:
        WiFi.mode(::WiFiMode::WIFI_STA);
        WiFi.begin(s_config->ssid, s_config->password);
        break;
    case ::WiFiMode::WIFI_AP:
        WiFi.mode(::WiFiMode::WIFI_AP);
        //192.168.1.1 , 192.168.1.1 , 255.255.255.0
        WiFi.softAPConfig(0x0101A8C0, 0x0101A8C0, 0x00FFFFFF);
        if (dbg)
            dbg->printf("wifi: %s %s\n", s_config->ssid.value, s_config->password.value);
        WiFi.softAP(s_config->ssid, s_config->password);
        break;
    }
}

static void connect_wifi() {
    if (WiFi.isConnected()) {
        if (!wifi_connected) {
            wifi_connected = true;

            if (espbase::on_wifi_connected)
                espbase::on_wifi_connected();
        }
    } else {
        if (wifi_connected)
            wifi_connected = false;
    }
}

static void read_tcp() {
    if (server.hasClient()) {
        tcp.stop();
        tcp = server.available();
    }

    while (tcp.connected()) {
        int length = tcp.available();
        if (length <= 0)
            break;

        buffer.resize(length);
        length = tcp.read(reinterpret_cast<uint8_t *>(buffer.data()), buffer.size());

        if (length > 0) {
            if (dbg != &tcp)
                dbg = &tcp;

            s_protocol.update(eof{});
            s_protocol.parse(std::move(buffer));
            s_protocol.update(eof{});
        }
    }
}

static int vprint(const char *format, std::va_list argp) {
    int size = 0;

    std::va_list argc;
    va_copy(argc, argp);
    size = std::vsnprintf(nullptr, 0, format, argc);
    va_end(argc);

    std::vector<char> print_buffer;

    if (size > 0) {
        auto start = print_buffer.size();
        print_buffer.resize(print_buffer.size() + size + 1);

        size = std::vsnprintf(print_buffer.data() + start, std::size_t(size) + 1, format, argp);
        print_buffer.resize(start + size);
    }

    dbg->write(print_buffer.data(), print_buffer.size());
    dbg->flush();

    return size;
}

int espbase::print(const char *format, ...) {
    int size = 0;

    std::va_list argp;
    va_start(argp, format);
    size = vprint(format, argp);
    va_end(argp);

    return size;
}


void espbase::setup() {
    assert(s_config);

    init_commands();
    start_wifi();

    ArduinoOTA.setPort(40000);    
    ArduinoOTA.begin();

    server.begin(s_config->config_port);
}


void espbase::loop() {
    connect_wifi();
    read_tcp();

    ArduinoOTA.handle();
}
