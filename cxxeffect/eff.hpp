//
// Created by Aly Cerruti on 2022-07-15.
//

#ifndef CXXEFFECT_EFF_HPP
#define CXXEFFECT_EFF_HPP

#include <stdexcept>

namespace eff {
    struct top {
        top() = default;
        top(const top&) = default;
        top& operator=(const top&) = default;
        ~top() = default;
    };
    union bot {
        bot() = delete;
        bot(const bot&) = delete;
        bot& operator=(const bot&) = delete;
        void* operator new(std::size_t) = delete;
        void operator delete(void*) = delete;
        ~bot() = default;
    };

    template<typename A>
    A absurd(const bot& bot) {
        throw std::invalid_argument("eff::absurd called");
    }
}

#include "./eff/task.hpp"

#endif //CXXEFFECT_EFF_HPP
