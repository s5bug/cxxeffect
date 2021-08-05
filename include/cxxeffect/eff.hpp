#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <variant>

namespace eff {

    // Moved top and bot to stream.hpp and pull.hpp respectively
    // Is there any difference between them?
    // Theyre both empty types
    // Can this file be deleted?

    template<typename A>
    using pure = A;

    template<template<typename> typename F, template<typename> typename G>
    class functionk {
        public:
        template<typename A>
        G<A> operator()(F<A> fa) const;
    };

}
