#include "test.h"

#include "../src/ringbuffer.h"

int main() {
    test("fill_buffer", []{
        const size_t len = 10000;
        ring_buffer<size_t> b(len);

        assert(b.max_size() == len);
        assert(b.size() == 0);

        for (size_t i = 0; i < len; ++i)
            b.push_back(i);

        assert(b.size() == b.max_size());

        for (size_t i = 0; i < b.size(); ++i)
            assert(b[i] == i);
    });

    test("erase_front_push_back", []{
        const size_t len = 10000;
        ring_buffer<size_t> b(len);

        for (size_t i = 0; i < len; ++i)
            b.push_back(i);

        for (size_t i = 0; i < len; ++i) {
            b.pop_front();
            b.push_back(len + i);
        }

        assert(b.size() == b.max_size());

        for (size_t i = 0; i < b.size(); ++i)
            assert(b[i] == len + i);
    });

    test("push_back_on_full", []{
        const size_t len = 10000;
        ring_buffer<size_t> b(len);

        for (size_t i = 0; i < len; ++i)
            b.push_back(i);

        assert(b.size() == len);

        for (size_t i = 0; i < len; ++i) {
            assert(b.front() == i);
            b.push_back(len + i);
        }

        assert(b.size() == len);

        for (size_t i = 0; i < len; ++i)
            assert(b[i] == len + i);
    });
    
    return 0;
}
