#pragma once

#include <functional>

#include "fsm.hpp"
#include "serialize.hpp"

struct eof : tr { };

struct initial : state { };

struct parse_command {
    std::vector<char> command;
    std::vector<std::vector<char>> args;

    void on_enter() {
        command.clear();
        args.clear();
    }
    void on_exit() { command.push_back('\0'); }
};

struct parse_value : state {
    std::vector<char> value;
    void on_enter() { value.clear(); }
};

struct parse_quoted_value : state {
    bool escape;
    void on_enter() { escape = false; }
};

struct parse_unquoted_value : state {
    bool escape;
    void on_enter() { escape = false; }
};

struct parse_hex_value : state {
    char last;
    uint8_t count;
    void on_enter() { count = 0; }
};

struct eol : state { };

struct parse_error : state { };

struct protocol_global {
    std::function<void(std::vector<char>&& command, std::vector<std::vector<char>>&& args)> command_callback;
};

using protocol_fsm = fsm<
    std::tuple<
        initial, parse_command, eol, parse_value, parse_error,
        parse_quoted_value, parse_unquoted_value, parse_hex_value
    >,
    protocol_global
>;

template<> template<typename S, typename T>
void protocol_fsm::exec(T tr) {
    return lambda<S, initial>(tr);
}

// initial

template<> template<>
void ::protocol_fsm::exec<initial>(char tr) {
    return lambda<initial, parse_command>(tr);
}
template<> template<>
void ::protocol_fsm::exec<initial>(eof) { }

// parse_command

template<> template<>
void protocol_fsm::exec<parse_command>(char tr) {
    switch (tr) {
    case '\0':  case '\t': case ' ':
        return transition<parse_command, parse_value>();

    case '\r': case '\n':
        return transition<parse_command, eol>();

    default:
        inst<parse_command>().command.push_back(tr);
        break;
    }
}
template<> template<>
void protocol_fsm::exec<parse_command>(eof) {
    return transition<parse_command, eol>();
}

// parse_value

template<> template<>
void protocol_fsm::exec<parse_value>(char tr) {
    // if (std::isdigit(tr))
    //     return lambda<parse_value, parse_number_value>(tr);
    // else
    switch (tr) {
        case '\'':
            return transition<parse_value, parse_quoted_value>();
        case '\0': case '\t': case ' ':
            break;
        case '\r': case '\n':
            return transition<parse_value, eol>();
        default:
            return lambda<parse_value, parse_unquoted_value>(tr);
    }
}
template<> template<>
void protocol_fsm::exec<parse_value>(eof) {
    return transition<parse_value, eol>();
}

// parse_unquoted_value

template<> template<>
void protocol_fsm::exec<parse_unquoted_value>(char tr) {
    auto& value = inst<parse_value>().value;

    if (inst<parse_unquoted_value>().escape) {
        inst<parse_unquoted_value>().escape = false;
        value.push_back(tr);
    } else switch (tr) {
    case '\\':
        inst<parse_unquoted_value>().escape = true;
        break;

    case '\0': case '\r': case '\n': case '\t': case ' ':
        inst<parse_command>().args.emplace_back(std::move(inst<parse_value>().value));
        return lambda<parse_unquoted_value, parse_value>(tr);

    case 'x': case 'X':
        if (value.size() == 1 && value[0] == '0') {
            inst<parse_value>().value.clear();
            return transition<parse_unquoted_value, parse_hex_value>();
        }
        /* fall through */

    default:
        value.push_back(tr);
        break;
    }
}
template<> template<>
void protocol_fsm::exec<parse_unquoted_value>(eof) {
    return transition<parse_hex_value, eol>();
}


// parse_hex_value

template<> template<>
void protocol_fsm::exec<parse_hex_value>(char tr) {
    if ((tr >= 'a' && tr <= 'f') || (tr >= 'A' && tr <= 'F') || (tr >= '0' && tr <= '9'))
        inst<parse_value>().value.push_back(tr);
    else switch (tr) {
    case '\0': case '\r': case '\n': case '\t': case ' ':
        if ((inst<parse_value>().value.size() & 1) == 0)
            return lambda<parse_hex_value, parse_value>(tr);
        else
            return lambda<parse_hex_value, parse_error>(tr);

    default:
        return lambda<parse_hex_value, parse_error>(tr);
    }
}
template<> template<>
void protocol_fsm::exec<parse_hex_value>(eof) {
    return transition<parse_hex_value, eol>();
}
template<> template<>
void protocol_fsm::on_exit<parse_hex_value>(protocol_fsm::state_t to) {
    auto& value = inst<parse_value>().value;

    value.push_back('\0');
    auto size = espbase::from_hex(value.data(), value.data());
    value.resize(size);

    inst<parse_command>().args.emplace_back(std::move(value));
}

// parse_quoted_value

template<> template<>
void protocol_fsm::exec<parse_quoted_value>(char tr) {
    if (inst<parse_quoted_value>().escape) {
        inst<parse_quoted_value>().escape = false;
        inst<parse_value>().value.push_back(tr);
    } else switch (tr) {
    case '\\':
        inst<parse_quoted_value>().escape = true;
        break;

    case '\'':
        return transition<parse_quoted_value, parse_value>();

    default:
        inst<parse_value>().value.push_back(tr);
        break;
    }
}
template<> template<>
void protocol_fsm::exec<parse_quoted_value>(eof) {
    return transition<parse_hex_value, eol>();
}
template<> template<>
void protocol_fsm::on_exit<parse_quoted_value>(protocol_fsm::state_t to) {
    inst<parse_command>().args.emplace_back(std::move(inst<parse_value>().value));
}

// eol

template<> template<>
void protocol_fsm::exec<eol>(char tr) {
    return lambda<eol, initial>(tr);
}
template<> template<>
void protocol_fsm::exec<eol>(tr) {
    return transition<eol, initial>();
}
template<> template<>
void protocol_fsm::on_enter<eol>(protocol_fsm::state_t from) {
    command_callback(std::move(inst<parse_command>().command),
                     std::move(inst<parse_command>().args));
}

// parse_error

template<> template<>
void protocol_fsm::on_enter<parse_error>(protocol_fsm::state_t from) {
    // std::printf("parse error\n");
}

template<> template<>
void protocol_fsm::exec<parse_error>(char tr) {
    if (tr == '\r' || tr == '\n')
        return transition<parse_error, initial>();
}

class protocol : public protocol_fsm {
public:
    inline void parse(std::vector<char>&& buf) {
        for (char c : buf)
            update(c);
    }
};
