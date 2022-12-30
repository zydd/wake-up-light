#pragma once

#include <tuple>
#include <vector>
#include <cassert>

namespace detail {
    template<class T, class Tuple>
    struct tuple_idx;

    template<class T, class... Types>
    struct tuple_idx<T, std::tuple<T, Types...>> {
        static constexpr std::size_t value = 0;
    };

    template<class T, class U, class... Types>
    struct tuple_idx<T, std::tuple<U, Types...>> {
        static constexpr std::size_t value = 1 + tuple_idx<T, std::tuple<Types...>>::value;
    };

    using fsm_global = struct { };
}

struct tr { };
struct state {
    void on_enter() { }
    void on_exit() { }
};


template<class States, class Global = detail::fsm_global>
class fsm : public Global {
public:
    using state_t = int;

    template<typename... Args> fsm(Args&&... args) {
        using initial = typename std::tuple_element<0, States>::type;
        inst<initial>().on_enter(std::forward<Args...>(args)...);
        // on_enter<initial>();
    }

    template<typename S>
    // static constexpr state_t id = detail::tuple_idx<S, States>::value;
    static constexpr state_t id() { return detail::tuple_idx<S, States>::value; }

    template<typename T>
    inline void update(T tr) { return get_exec<0>::exec(this, tr); }

    inline state_t state() const { return m_state; }
    template<typename S> S& inst() { return std::get<id<S>()>(m_data.back()); }

protected:
    template<typename S> void on_enter(state_t from) { }
    template<typename S> void on_exit(state_t to) { }

    template<typename S1, typename S2, typename... Args>
    void transition(Args&&... args) {
        on_exit<S1>(id<S2>());
        inst<S1>().on_exit();
        inst<S2>().on_enter(std::forward<Args...>(args)...);
        on_enter<S2>(id<S1>());
        m_state = id<S2>();
    }

    template<typename S1, typename S2, typename T, typename... Args>
    void lambda(T tr, Args&&... args) {
        transition<S1, S2>(std::forward<Args...>(args)...);
        exec<S2>(tr);
    }

    template<typename S1, typename S2, typename T, typename... Args>
    void lambda_push(T tr, Args&&... args) {
        push<S1, S2>(std::forward<Args...>(args)...);
        exec<S2>(tr);
    }

    template<typename S, typename T>
    void lambda_pop(T tr) {
        pop<S>();
        get_exec<0>::exec(this, tr);
    }

    template<typename S1, typename S2, typename... Args>
    void push(Args&&... args) {
        m_stack.push_back(id<S1>());
        m_data.emplace_back();
        transition<S1, S2>(std::forward<Args...>(args)...);
    }
    template<typename S>
    void pop() {
        assert(!m_stack.empty());
        get_transition<S, 0>::transition(this, m_stack.back());
        m_data.pop_back();
        m_stack.pop_back();
    }

    template<int N, int M = std::tuple_size<States>::value> struct get_exec {
        template<typename T>
        static void exec(fsm *fsm, T tr) {
            if (fsm->state() == N)
                fsm->fsm::exec<typename std::tuple_element<N, States>::type>(tr);
            else
                get_exec<N+1>::exec(fsm, tr);
        }
    };
    template<int M> struct get_exec<M, M> {
        template<typename T> static void exec(fsm *fsm, T tr) { } };

    template<typename S, int N, int M = std::tuple_size<States>::value> struct get_transition {
        static void transition(fsm *fsm, state_t state) {
            if (state == N)
                fsm->fsm::transition<S, typename std::tuple_element<N, States>::type>();
            else
                get_transition<S, N+1>::transition(fsm, state);
        }
    };
    template<typename S, int M> struct get_transition<S, M, M> {
        static void transition(fsm *fsm, state_t state) { } };

    template<typename S, typename T> void exec(T tr);

private:
    state_t m_state = 0;
    std::vector<States> m_data = {{}};
    std::vector<state_t> m_stack;
};
