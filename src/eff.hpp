#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <variant>

namespace eff {

    using top = std::monostate;
    union bot {};

    template<typename A>
    using pure = A;

    template<template<typename> typename F, template<typename> typename G>
    class functionk {
        public:
        template<typename A>
        G<A> operator()(F<A> fa) const;
    };

}
