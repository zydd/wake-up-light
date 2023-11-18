#include <vector>

#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <button_fcn.h>
#include <espbase.h>

#include <NTPClient.h>

#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (86400UL)
#define DAYS_PER_WEEK (7UL)
#define SECS_PER_WEEK (SECS_PER_DAY * DAYS_PER_WEEK)
#define SECS_PER_YEAR (SECS_PER_WEEK * 52UL)


uint32_t timer_int_handle_time = 0;
uint32_t loop_interval = 0;

const int NUM_LEDS = 60;
static uint16_t led_color[NUM_LEDS][3];  // RGB


static double t = 0;
static bool is_running;

static const unsigned reset_time = 500000 * 80 / 1000;
static const unsigned t0_hi = 400 * 80 / 1000;
static const unsigned t0_lo = 850 * 80 / 1000;

static const unsigned t1_hi = 800 * 80 / 1000;
static const unsigned t1_lo = 450 * 80 / 1000;


#define d3_hi() (GPOS = 1 << D3)
#define d3_lo() (GPOC = 1 << D3)
#define led_hi() (GPOS = LED_BUILTIN << D3)
#define led_lo() (GPOC = LED_BUILTIN << D3)


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);


static espbase::config_base::meta config_meta;
struct Config : espbase::config {
    Config() : espbase::config(&config_meta) {}
    member<uint8_t>     led_brightness  = {&config_meta, "led_brightness",  255};
    member<uint8_t>     led_count       = {&config_meta, "led_count",       60};
    member<bool>        builtin_led     = {&config_meta, "builtin_led",     false};
    member<uint16_t>    animation_speed = {&config_meta, "animation_speed", 0x0100};
    member<uint8_t>     led_dither_max  = {&config_meta, "led_dither_max",  0x00};
    member<uint16_t>    duration        = {&config_meta, "duration",        90 * 60};
    member<uint16_t>    variation       = {&config_meta, "variation",       0x0100};
    member<uint8_t>     max_progression_night   = {&config_meta, "max_progression_night",   0x40};  // 0x40 / 0xff ~= 0.25
    member<uint16_t>    led_update_freq = {&config_meta, "led_update_freq", 400};
};


static Config& config = *[]{
    EEPROM.begin(sizeof(config));
    return new(EEPROM.getDataPtr()) Config();
}();


__always_inline static uint32_t __clock_cycles() {
    uint32_t cycles;
    __asm__ __volatile__ ("rsr %0,ccount":"=a" (cycles));
    return cycles;
}


std::vector<char> buffer;
void read_serial() {
    while (int length = Serial.available()) {
        buffer.resize(length);
        length = Serial.read(buffer.data(), buffer.size());
        Serial.write(buffer.data(), buffer.size());

        if (length > 0) {
            espbase::dbg = &Serial;
            espbase::parse_command(std::move(buffer));
        }
    }
}


IRAM_ATTR static inline void write_led(uint32_t c) {
    uint32_t clk;

    for (unsigned i = 0; i < 24; ++i) {
        d3_hi();
        clk = __clock_cycles();
        clk += (c & 0x800000) ? t1_hi : t0_hi;
        clk -= 12;
        while (__clock_cycles() < clk);

        d3_lo();
        clk += (c & 0x800000) ? t1_lo : t0_lo;
        clk -= 12;
        c <<= 1;
        while (__clock_cycles() < clk);
    }
}


void reset_strip_protocol() {
    d3_lo();
    auto clk = __clock_cycles();
    clk += reset_time;
    while (__clock_cycles() < clk);
}


void calc_colors() {
    auto hour = timeClient.getHours();
    auto max_brightness = (hour >= 4 && hour <= 16) ? config.led_brightness.value : 10;
    float max_progression = (hour >= 4 && hour <= 16) ? 1.0f : float(config.max_progression_night) / 0xff;

    float st = t * config.animation_speed.value / 0x0100;
    float _, t10 = min(modff(t / config.duration, &_) * 2.0f, max_progression);

    for (unsigned i = 0; i < config.led_count; ++i) {
        float x = float(i) / config.led_count.value * config.variation / 0x0100;

        float color = 0;

        // float f1 = sqrtf(sinf(2 * PI * (x * 2 - st * 0.5)) * 0.5 + 0.5);
        // f1 = 1;
        float f2 = sqrtf(sinf(2 * PI * (x * 5 - st * 0.04)) * 0.5 + 0.5);
        float f3 = sqrtf(sinf(2 * PI * (x * 11 + st * 0.1)) * 0.5 + 0.5);

        // color = min(min(f1, f2), f3);
        color = min(f2, f3);
        // color = i / 59.0;

        if (color < 0)
            color = 0;
        else if (color > 1)
            color = 1;

        float r = max_brightness * color * min(2.5f * t10, 1.0f);
        float g = max_brightness * color * t10;
        float b = max_brightness * color * min(t10 < 0.5f ? 0.5f * t10 : 1.5f * t10 - 0.5f, 1.0f);

        led_color[i][0] = r * 0x100;
        led_color[i][1] = g * 0x100;
        led_color[i][2] = b * 0x100;
    }

    for (unsigned i = config.led_count; i < NUM_LEDS; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
            led_color[i][j] = 0x0000;
        }
    }
}


void clear_strip() {
    for (unsigned i = 0; i < NUM_LEDS; ++i)
        write_led(0x000000);
}


void update_strip() {
    static uint32_t leds[NUM_LEDS] = {0};
    static int8_t dither[NUM_LEDS][3] = {0};

    for (unsigned i = 0; i < NUM_LEDS; ++i) {
        static uint8_t rgb[3];

        for (unsigned j = 0; j < 3; ++j) {
            rgb[j] = led_color[i][j] >> 8;
            uint8_t fract = uint16_t(led_color[i][j] & 0xff) * config.led_dither_max >> 8;

            if (dither[i][j] <= 0) {
                dither[i][j] += fract;
            } else {
                dither[i][j] -= config.led_dither_max - fract;

                if (rgb[j] < 0xff && fract > 0)
                    rgb[j] += 1;
            }
        }

        leds[i] = rgb[0] << 8 | rgb[1] << 16 | rgb[2];
    }

    for (unsigned i = 0; i < NUM_LEDS; ++i) {
        write_led(leds[i]);
    }
}


IRAM_ATTR void timer_int_handler(void *, void *) {
    T1I = 0;  // clear interrupt flag

    // led.toggle();

    {
        ETS_INTR_LOCK();

        if (is_running) {
            update_strip();
        } else {
            clear_strip();
        }

        ETS_INTR_UNLOCK();
    }

    timer_int_handle_time = T1L - T1V;
}


void update_timer_interval() {
    uint32_t interval = F_CPU / max<uint32_t>(config.led_update_freq, 1);

    {
        static const int bits = 23;
        uint32_t max_interval = (1 << (bits - 1)) - 1;

        interval = min(interval, max_interval);
    }

    {
        uint32_t min_interval;

        if (timer_int_handle_time) {
            min_interval = timer_int_handle_time;
        } else {
            auto clk = __clock_cycles();
            timer_int_handler(nullptr, nullptr);
            min_interval = __clock_cycles() - clk;
        }

        min_interval = min_interval * 1.2;

        interval = max(interval, min_interval);
    }

    config.led_update_freq = round(double(F_CPU) / interval);
    timer1_write(interval);
}


#define enable_timer()  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP)

void setup_timer() {
    ETS_FRC_TIMER1_INTR_ATTACH(timer_int_handler, NULL);
    ETS_FRC1_INTR_ENABLE();

    config_meta.on_change("led_update_freq", update_timer_interval);

    enable_timer();

    ArduinoOTA.onStart([]{
        timer1_disable();
    });
}


void setup() {
    Serial.begin(8 * 115200);
    espbase::dbg = &Serial;

    espbase::setup();

    if (config.led_count > NUM_LEDS)
        config.led_count = NUM_LEDS;

    espbase::on_wifi_connected = []{
        // snprintf(device_name, sizeof(device_name), "wul-%s", espbase::device_id());
        // MDNS.begin(device_name);
        // MDNS.addService("wul-config", "tcp", 1444);
    };

    timeClient.begin();
    timeClient.setUpdateInterval(1024 * 1000);

    pinMode(LED_BUILTIN, OUTPUT);
    config_meta.on_change("builtin_led", []{ digitalWrite(LED_BUILTIN, !config.builtin_led); });

    pinMode(D3, OUTPUT);
    d3_lo();

    espbase::command("off", []{ is_running = false; });
    espbase::command("on", []{
        is_running = true;
        t = 0;
    });
    espbase::command("d3_lo", []{ d3_lo(); });
    espbase::command("d3_hi", []{ d3_hi(); });

    espbase::command("time", []{
        espbase::print("time: %s\n", timeClient.getFormattedTime().c_str());
    });

    espbase::command("d", []{
        uint32_t free; uint16_t max; uint8_t frag;
        ESP.getHeapStats(&free, &max, &frag);

        espbase::print("\nRAM:\n"
                        "    free:          %.3lf kB\n"
                        "    largest block: %.3lf kB\n"
                        "    fragmentation: %u %\n\n",
                        free / 1000.0, max / 1000.0, frag);

        espbase::print("Reset info:\n    %s\n\n", ESP.getResetInfo().c_str());

        espbase::print("timer_int_handle_time: %u cycles (max %.2lf Hz)\n",
                       timer_int_handle_time, double(F_CPU) / timer_int_handle_time);
        espbase::print("loop_interval: %u cycles (%.2lf Hz)\n",
                       loop_interval, double(F_CPU) / loop_interval);
        espbase::print("led_update_freq: %u Hz\n\n", config.led_update_freq.value);

        espbase::print("time: %s\n", timeClient.getFormattedTime().c_str());
        espbase::print("T1L: %u (%.2lf Hz)\n", T1L, double(F_CPU) / T1L);
        espbase::print("T1V: %u\n\n", T1V);

        espbase::print("t: %lf\n", t);
        espbase::print("is_running: %u\n", is_running);
    });

    espbase::command("t0", [](std::vector<std::vector<char>>&& args){
        if (args.size() == 1) {
            args[0].push_back('\n');
            t = atoi(args[0].data());
        } else {
            t = 0;
        }
    });

    espbase::command("disable-timer", [](std::vector<std::vector<char>>&& args){
        timer1_disable();
    });

    espbase::command("enable-timer", [](std::vector<std::vector<char>>&& args){
        enable_timer();
    });

    espbase::command("commit", [](std::vector<std::vector<char>>&& args){
        timer1_disable();
        espbase::print("commit config notmr\n");
        delay(20);
        EEPROM.commit();
        enable_timer();
    });

    setup_timer();

    config_meta.notify_all();
}


void loop() {
    espbase::loop();
    read_serial();

    // MDNS.update();
    timeClient.update();

    {
        static uint32_t clk_prev;
        auto clk = __clock_cycles();
        loop_interval = clk - clk_prev;
        t += double(loop_interval) / F_CPU;
        clk_prev = clk;

        if (t > config.duration)
            is_running = false;
    }

    if (!is_running) {
        time_t now = timeClient.getEpochTime();
        time_t midnight = (now / SECS_PER_DAY) * SECS_PER_DAY;

        time_t morning = midnight + 7 * SECS_PER_HOUR + 30 * SECS_PER_MIN;
        time_t evening = midnight + 23 * SECS_PER_HOUR + 30 * SECS_PER_MIN;

        if ((morning <= now) && (now - morning < config.duration)) {
            t = now - morning;
            is_running = true;
        }

        if ((evening <= now) && (now - evening < config.duration)) {
            t = now - evening;
            is_running = true;
        }
    }

    calc_colors();
}
