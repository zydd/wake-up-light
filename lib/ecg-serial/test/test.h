#ifndef TEST_H
#define TEST_H

#include <cassert>
#include <iostream>

template<class Func>
void test(const char* name, Func func) {
    std::cout << name << std::endl;
    func();
}

#endif
