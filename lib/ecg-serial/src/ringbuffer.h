#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <vector>

template<typename T>
class ring_buffer {
public:
    ring_buffer(size_t maxSize = 0) :
        m_data(maxSize),
        m_max_size(maxSize)
    {
        clear();
    }

    inline size_t size() const { return m_size; }
    inline size_t available() const { return m_max_size - m_size; }
    inline bool empty() const { return m_size == 0; }
    inline size_t max_size() const { return m_max_size; }
    inline size_t begin_index() const { return m_begin; }
    inline size_t end_index() const { return m_end; }

    inline void push_back_n(size_t n) {
        m_end = (m_end + n) % m_max_size;
        m_size += n;

        adjust();
    }

    inline void push_back() {
        m_end = (m_end + 1) % m_max_size;
        m_size++;

        adjust();
    }

    inline void push_back(const T& value) {
        m_data[m_end] = value;
        m_end = (m_end + 1) % m_max_size;
        m_size++;

        adjust();
    }

    template<class... Args>
    void emplace_back(Args&&... args) {
        push_back(T(std::forward<Args>(args)...));
    }

    inline void pop_front(size_t n = 1) {
        if (n > m_size)
            n = m_size;

        m_begin = (m_begin + n) % m_max_size;
        m_size -= n;
    }

    inline void clear() {
        m_begin = 0;
        m_end = 0;
        m_size = 0;
    }

    inline void resize(size_t size, T value = T()) {
        if (size > m_max_size)
            size = m_max_size;

        while (m_size < m_max_size) {
            m_size++;
            m_data[m_end] = value;
            m_end = (m_end + 1) % m_max_size;
        }
    }

    inline T& operator[] (size_t i) {
        return m_data[(m_begin + i) % m_max_size];
    }

    inline const T& operator[] (size_t i) const {
        return m_data[(m_begin + i) % m_max_size];
    }

    inline T& front() {
        return m_data[m_begin];
    }

    inline T const& front() const {
        return m_data[m_begin];
    }

    inline T& back() {
        return m_data[(m_end + m_max_size - 1) % m_max_size];
    }

private:
    std::vector<T> m_data;
    size_t m_max_size;
    size_t m_size;
    size_t m_begin;
    size_t m_end;

    inline void adjust() {
        if (m_size > m_max_size) {
            m_begin = (m_begin + m_size - m_max_size) % m_max_size;
            m_size = m_max_size;
        }
    }
};

#endif // RINGBUFFER_H
