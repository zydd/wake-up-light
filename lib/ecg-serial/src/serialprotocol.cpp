#include <cmath>

#include "serialprotocol.h"

static const uint8_t rbo[] = {
     0, 16,  8, 24,  4, 20, 12, 28,
     2, 18, 10, 26,  6, 22, 14, 30,
     1, 17,  9, 25,  5, 21, 13, 29,
     3, 19, 11, 27,  7, 23, 15, 31,
    32, 48, 40, 56, 36, 52, 44, 60,
    34, 50, 42, 58, 38, 54, 46, 62,
    33, 49, 41, 57, 37, 53, 45, 61,
    35, 51, 43, 59, 39, 55, 47, 63,
};

static const uint8_t ord1[] = {
     0,  8,  4, 12,  2, 10,  6, 14,
     1,  9,  5, 13,  3, 11,  7, 15,
    16, 24, 20, 28, 18, 26, 22, 30,
    17, 25, 21, 29, 19, 27, 23, 31,
    32, 40, 36, 44, 34, 42, 38, 46,
    33, 41, 37, 45, 35, 43, 39, 47,
    48, 56, 52, 60, 50, 58, 54, 62,
    49, 57, 53, 61, 51, 59, 55, 63,
};

static const uint8_t ord2[] = {
    16, 24, 20, 28, 18, 26, 22, 30,
    17, 25, 21, 29, 19, 27, 23, 31,
     0,  8,  4, 12,  2, 10,  6, 14,
     1,  9,  5, 13,  3, 11,  7, 15,
    48, 56, 52, 60, 50, 58, 54, 62,
    49, 57, 53, 61, 51, 59, 55, 63,
    32, 40, 36, 44, 34, 42, 38, 46,
    33, 41, 37, 45, 35, 43, 39, 47,
};

SerialProtocol::SerialProtocol():
    m_packetNumIn(-1),
    m_packetNumOut(-1),
    m_transmitOrder(0),
    m_reorderBuffer(0),
    m_transmitBuffer(64, Packet(-1))
{
    state = s_init;
}

char *SerialProtocol::beginSample() {
    // only one sample per packet
    auto& prev = m_transmitBuffer[m_packetNumOut & 0x3f];
    prev.setPacketCrc(prev.crc());

    const unsigned i = ++m_packetNumOut & 0x3f;

    if (onPacketReady) {
        unsigned j = i ^ 0x20;
        switch (m_transmitOrder) {
        default:
            onPacketReady(m_transmitBuffer[j]);
            break;
        case 1:
            onPacketReady(m_transmitBuffer[rbo[j]]);
            break;
        case 2:
            onPacketReady(m_transmitBuffer[ord1[j]]);
            onPacketReady(m_transmitBuffer[ord2[j]]);
            break;
        }
    }

    m_transmitBuffer[i] = Packet(m_packetNumOut);
    return m_transmitBuffer[i].data() + Packet::header;
}

inline int SerialProtocol::init() {
    if (next() == '/')
        return s_header;
    else
        return s_init;
}

inline int SerialProtocol::header() {
    switch (next()) {
    case 'c':
        return read_command(16, s_parse_command);
    case 'd':
        config_name(16, s_config_value);
        config_value(64, s_parse_config);
        return s_config_name;
    case 'R':
        read_packet(Packet::size - 2, s_parse_packet);
        read_packet.data.emplace_back('/');
        read_packet.data.emplace_back('R');
        return s_read_packet;
    }
    return s_init;
}

template<int State>
int SerialProtocol::read_word<State>::operator()(size_t count, int next) {
    this->count = count;
    this->next = next;
    this->escape = false;
    data.clear();
    return State;
}

template<int State>
inline int SerialProtocol::read_word<State>::update(FsmBase *fsm) {
    while (count && fsm->hasNext()) {
        if (escape) {
            escape = false;
            data.push_back(fsm->next());
            continue;
        }

        switch (auto c = fsm->next()) {
        case '\0': case ' ': case '\r': case '\n': case '\t':
            if (data.size() == 0)
                continue;
            else
                goto finish;

        case '\\':
            escape = true;
            continue;

        default:
            data.push_back(c);
            count--;
        }
    }

    if (count == 0) {
finish:
        data.push_back('\0');
        return next;
    } else
        return State;
}

int SerialProtocol::update() {
    while (state > lambda || hasNext())
        switch (state) {
        case s_init:            state = init(); break;
        case s_header:          state = header(); break;

        case s_read_packet:     state = read_packet.update(this); break;
        case s_parse_packet:    state = parse_packet(); break;

        case s_config_name:     state = config_name.update(this); break;
        case s_config_value:    state = config_value.update(this); break;
        case s_parse_config:    state = parse_config(); break;

        case s_read_command:    state = read_command.update(this); break;
        case s_parse_command:   state = parse_command(); break;

        default: FsmBase::update(); break;
        }

    return state;
}

int SerialProtocol::parse_packet() {
    Packet packet(std::move(read_packet.data));

    if (! packet.checkCrc())
        return s_init;

    const uint16_t pnum = packet.packetNum();
    const int16_t dpnum = pnum - m_packetNumIn;

    if (!m_reorderBuffer.max_size()) {
        if (onPacketArrived)
            onPacketArrived(packet);
    } else {
        if (dpnum > 0 && uint16_t(dpnum) < m_reorderBuffer.max_size()) { // new packet
            while (!m_reorderBuffer.empty() && m_reorderBuffer.size() + dpnum >= m_reorderBuffer.max_size()) {
                if (onPacketArrived && m_reorderBuffer.front())
                    onPacketArrived(m_reorderBuffer.front().value());

                m_reorderBuffer.front().reset();
                m_reorderBuffer.pop_front();
            }

            m_reorderBuffer.push_back_n(dpnum);
            m_reorderBuffer.back().emplace(std::move(packet));
            m_packetNumIn = pnum;
        } else if (dpnum <= 0 && uint16_t(-dpnum) < m_reorderBuffer.size()) { // retransmission
            size_t pos = m_reorderBuffer.size()-1 + dpnum;
            m_reorderBuffer[pos].emplace(std::move(packet));
        } else if (std::abs(dpnum) >= m_reorderBuffer.max_size()) {
            // transmit buffer
            while (!m_reorderBuffer.empty()) {
                if (onPacketArrived && m_reorderBuffer.front())
                    onPacketArrived(m_reorderBuffer.front().value());

                m_reorderBuffer.front().reset();
                m_reorderBuffer.pop_front();
            }
            m_reorderBuffer.push_back();
            m_reorderBuffer.back().emplace(std::move(packet));
            m_packetNumIn = pnum;
        }

        while (!m_reorderBuffer.empty() && m_reorderBuffer.front()) {
            if (onPacketArrived)
                onPacketArrived(m_reorderBuffer.front().value());

            m_reorderBuffer.front().reset();
            m_reorderBuffer.pop_front();
        }
    }

    return s_init;
}

int SerialProtocol::parse_config() {
    if (config_name.data.size() == 0 || config_value.data.size() == 0)
        return s_init;

    if (onConfig) {
        auto size = fromHex(config_value.data.data(), config_value.data.data());
        if (!size)
            return s_init;

        config_value.data.resize(size);
        onConfig(std::move(config_name.data), std::move(config_value.data));
    }

    return s_init;
}

int SerialProtocol::parse_command() {
    if (read_command.data.size() == 0)
        return s_init;

    if (onCommand)
        onCommand(std::move(read_command.data));

    return s_init;
}
