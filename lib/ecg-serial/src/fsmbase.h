#ifndef FSMBASE_H
#define FSMBASE_H

#include <functional>
#include <vector>

class FsmBase {
public:
    enum { s_final = -1 };

    int state = s_final;

    inline int update();
    inline void setBuffer(std::vector<char>&& buf);

    inline char next() {
        return buffer[bufferPos++]; }

    inline char peek() {
        return buffer[bufferPos]; }

    inline bool hasNext() {
        return bufferPos < buffer.size(); }

protected:
    template<int State>
    struct read_n {
        size_t count;
        std::vector<char> data;
        int next;

        int operator()(const size_t count, const int next);
        int update(FsmBase *fsm);
    };

private:
    std::vector<char> buffer;
    size_t bufferPos = 0;
};


inline int FsmBase::update() {
    while (hasNext()) next();
    return s_final;
}

inline void FsmBase::setBuffer(std::vector<char>&& buf) {
    buffer = std::move(buf);
    bufferPos = 0;
}

template<int State>
int FsmBase::read_n<State>::operator()(const size_t count, const int next) {
    this->count = count;
    this->next = next;
    data.clear();
    return State;
}

template<int State>
inline int FsmBase::read_n<State>::update(FsmBase *fsm) {
    while (count && fsm->hasNext()) {
        data.push_back(fsm->next());
        count--;
    }

    if (count == 0)
        return next;
    else
        return State;
}

#endif // FSMBASE_H
