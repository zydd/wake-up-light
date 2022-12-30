#ifndef SERIALPROTOCOL_H
#define SERIALPROTOCOL_H

#include "fsmbase.h"
#include "packet.h"
#include "ringbuffer.h"

#include <utility>

template<class T>
struct maybe : private std::pair<bool, T> {
    inline maybe():
        std::pair<bool, T>(false, T())
    { }

    inline maybe(T&& value):
        std::pair<bool, T>(true, value)
    { }

    inline T& value() {
        return this->second; }

    inline const T& value() const {
        return this->second; }

    inline T&& take_value() {
        this->first = false;
        return std::move(this->second); }

    inline explicit operator bool() const {
        return this->first; }

    inline void reset() {
        this->first = false; }

    inline T& emplace(T&& val) {
        this->first = true;
        this->second = std::move(val);
        return this->second;
    }
};

class SerialProtocol : public FsmBase {
public:
    using buffer = std::vector<char>;

    enum {
        s_init,
        s_header,

        s_read_packet,

        s_config_name,
        s_config_value,
        s_read_command,

        lambda,

        s_parse_packet,
        s_parse_config,
        s_parse_command
    };

    std::function<void(Packet const&)> onPacketArrived;
    std::function<void(Packet const&)> onPacketReady;
    std::function<void(buffer&&, buffer&&)> onConfig;
    std::function<void(buffer&&)> onCommand;

    SerialProtocol();

    char *beginSample();

    inline void parseBuffer(buffer&& buffer) {
        setBuffer(std::move(buffer)); update(); }

    inline void setTransmitOrder(int order = 0) {
        m_transmitOrder = order; }

    inline void setReorderBufferSize(unsigned size) {
        m_reorderBuffer = ring_buffer<maybe<Packet>>(size); }

private:
    using FsmBase::setBuffer;

    uint16_t m_packetNumIn;
    uint16_t m_packetNumOut;
    int m_transmitOrder;
    ring_buffer<maybe<Packet>> m_reorderBuffer;
    std::vector<Packet> m_transmitBuffer;

    template<int State>
    struct read_word {
        size_t count;
        buffer data;
        int next;
        bool escape;

        int operator()(size_t count, int next);
        int update(FsmBase *fsm);
    };

    read_n<s_read_packet> read_packet;
    read_word<s_config_name> config_name;
    read_word<s_config_value> config_value;
    read_word<s_read_command> read_command;

    int update();
    int init();
    int header();
    int parse_packet();
    int parse_config();
    int parse_command();
};

#endif // SERIALPROTOCOL_H
