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

    template<typename A, typename B>
    class and_then;

    template<typename A, typename B>
    class raw_and_then {};

    template<typename A, typename B>
    class raw_and_then_single final : public raw_and_then<A, B> {
        std::function<B (A)> f;
        std::uint32_t index;
        public:
        raw_and_then_single(std::function<B (A)> a, std::uint32_t b) : f(a), index(b) {}
    };

    template<typename A, typename E, typename B>
    class raw_and_then_concat final : public raw_and_then<A, B> {
        and_then<A, E> left;
        and_then<E, B> right;
        public:
        raw_and_then_concat(and_then<A, E> a, and_then<E, B> b) : left(a), right(b) {}
    };

    template<typename A, typename B>
    class and_then : public std::function<B (A)> {
        std::shared_ptr<raw_and_then<A, B>> internal;
        public:
        and_then(std::shared_ptr<raw_and_then<A, B>> a) : internal(a) {}

        static and_then<A, B> of(std::function<B (A)> f) {
            // TODO: don't reconvert things that are already and_thens
            std::shared_ptr<raw_and_then<A, B>> inner = std::shared_ptr<raw_and_then<A, B>>(new raw_and_then_single<A, B>(f, 0));
            return and_then(inner);
        }

        template<typename E>
        static and_then<A, B> concat(and_then<A, E> left, and_then<E, B> right) {
            std::shared_ptr<raw_and_then<A, B>> inner = std::shared_ptr<raw_and_then<A, B>>(new raw_and_then_concat<A, E, B>(left, right));
            return and_then(inner);
        }
    };

}
