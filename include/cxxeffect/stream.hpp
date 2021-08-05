#pragma once

#include <algorithm>
#include <exception>
#include <ios>
#include <iostream>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include <cxxeffect/pull.hpp>

namespace eff {
    template<template<typename> typename F, typename O>
    class stream;

    template<template<typename> typename F, typename A, typename B>
    using pipe = std::function<stream<F, B> (stream<F, A>)>;

    // ???? Why is there an empty union here????
    // (This was moved here from eff.hpp b/c stream needed it
    // and eff.hpp doesn't use it)
    union bot {};

    template<template<typename> typename F, typename O>
    class stream_compile_ops final {
        stream<F, O> internal;
        public:
        stream_compile_ops(stream<F, O> a) : internal(a) {}

        F<top> drain() {
            std::function<top (top, std::vector<O>)> discardChunks = [](top t, std::vector<O> chunk) { return t; };
            return foldChunks<top>(top {}, discardChunks);
        }

        template<typename B>
        F<B> foldChunks(B init, std::function<B (B, std::vector<O>)> f) {
            std::shared_ptr<std::function<F<B> (B, pull_variant_t<F, O, top>)>> loop =
                std::make_shared<std::function<F<B> (B, pull_variant_t<F, O, top>)>>();

            *loop = [loop, f](B bstep, pull_variant_t<F, O, top> step) {
                if(std::holds_alternative<top>(step)) {
                    F<B> done = F<B>::pure(bstep);
                    return done;
                } else {
                    pull_pair_t<F, O, top> p = std::get<pull_pair_t<F, O, top>>(step);
                    std::vector<O> hd = p.first;
                    pull<F, O, top> tl = p.second;

                    B newBstep = f(bstep, hd);
                    pull_task_t<F, O, top> newStep = tl.step();

                    std::function<F<B> (pull_variant_t<F, O, top>)> next =
                        [loop, newBstep](pull_variant_t<F, O, top> nextStep) {
                            return (*loop)(newBstep, nextStep);
                        };

                    return newStep.flatMap(next);
                }
            };

            std::function<F<B> (pull_variant_t<F, O, top>)> go =
                [loop, init](pull_variant_t<F, O, top> initialStep) {
                    return (*loop)(init, initialStep);
                };

            pull<F, O, top> usPull = internal.toPull();
            pull_task_t<F, O, top> usStep = usPull.step();
            return usStep.flatMap(go);
        }
    };

    template<template<typename> typename F, typename O>
    class stream final {
        pull<F, O, top> underlying;
        public:
        stream(pull<F, O, top> a) : underlying(a) {}

        static stream<F, O> chunk(std::vector<O> os) {
            return stream(pull<F, O, top>::output(os));
        }

        static stream<F, O> emit(O o) {
            return stream(pull<F, O, top>::output1(o));
        }

        static stream<F, O> empty() {
            return stream(pull<F, O, top>::done());
        }

        static stream<F, O> eval(F<O> fo) {
            pull<F, O, O> pEval = pull<F, O, O>::eval(fo);
            std::function<pull<F, O, top> (O)> emitResult = [](O o) { return pull<F, O, top>::output1(o); };
            pull<F, O, top> pEmit = pEval.flatMap(emitResult);
            return stream(pEmit);
        }

        static stream<F, bot> exec(F<top> ft) {
            return stream(pull<F, bot, top>::eval(ft));
        }

        static stream<F, O> repeatEval(F<O> fo) {
            return stream<F, O>::eval(fo).repeat();
        }

        stream<F, O> append(std::function<stream<F, O> ()> other) const {
            std::function<pull<F, O, top> (top)> asNextPull = [other](top t) { return other().underlying; };
            return stream<F, O>(underlying.flatMap(asNextPull));
        }

        stream_compile_ops<F, O> compile() const {
            return stream_compile_ops<F, O>(*this);
        }

        stream<F, std::vector<O>> chunks() const {
            std::function<std::vector<std::vector<O>> (std::vector<O>)> wrapChunk = [](std::vector<O> chunk) {
                std::vector<std::vector<O>> wrapped = { chunk };
                return wrapped;
            };
            return mapChunks(wrapChunk);
        }

        stream<F, O> cons(std::vector<O> chunk) const {
            if(chunk.empty()) {
                return *this;
            } else {
                stream<F, O> chunkStream = stream<F, O>::chunk(chunk);
                std::function<stream<F, O> ()> thenThis = [*this]() { return *this; };
                return chunkStream.append(thenThis);
            }
        }

        template<typename P>
        stream<F, P> evalMap(std::function<F<P> (O)> f) const {
            std::function<stream<F, P> (O)> evalSingular = [f](O o) { return stream<F, P>::eval(f(o)); };
            return flatMap(evalSingular);
        }

        template<typename P>
        stream<F, P> flatMap(std::function<stream<F, P> (O)> f) const {
            std::function<pull<F, P, top> (O)> pullForElement = [f](O o) {
                stream<F, P> streamForThisElement = f(o);
                pull<F, P, top> pullForThisElement = streamForThisElement.toPull();
                return pullForThisElement;
            };
            pull<F, O, top> usPull = toPull();
            pull<F, P, top> concatAllOutput = usPull.flatMapOutput(pullForElement);
            return stream<F, P>(concatAllOutput);
        }

        stream<F, bot> foreach(std::function<F<top> (O)> f) const {
            std::function<stream<F, bot> (O)> execElement = [f](O o) {
                return stream<F, bot>::exec(f(o));
            };
            return flatMap(execElement);
        }

        template<typename P>
        stream<F, P> map(std::function<P (O)> f) const {
            std::function<std::vector<P> (std::vector<O>)> chunkMapper = [f](std::vector<O> v) {
                std::vector<P> r(v.size());

                for(std::size_t i = 0; i < v.size(); i++) {
                    r[i] = f(v[i]);
                }

                return r;
            };
            return stream<F, P>(underlying.mapOutput(chunkMapper));
        }

        template<typename P>
        stream<F, P> mapChunks(std::function<std::vector<P> (std::vector<O>)> f) const {
            return stream<F, P>(underlying.mapOutput(f));
        }

        stream<F, O> repeat() const {
            std::function<stream<F, O> ()> here = [*this]() {
                return this->repeat();
            };
            return append(here);
        }

        stream<F, O> takeWhile(std::function<bool (O)> predicate, bool takeFailure = false) const {
            return stream<F, O>(toPull().takeWhile(predicate, takeFailure));
        }

        template<typename P>
        stream<F, P> through(pipe<F, O, P> p) const {
            return p(*this);
        }

        template<typename P>
        pull<F, P, std::optional<std::pair<std::vector<O>, stream<F, O>>>> uncons() const {
            std::function<std::optional<std::pair<std::vector<O>, stream<F, O>>> (pull_variant_t<F, O, top>)> stitch =
                [](pull_variant_t<F, O, top> pullUncons) {
                    if(std::holds_alternative<pull_pair_t<F, O, top>>(pullUncons)) {
                        pull_pair_t<F, O, top> p = std::get<pull_pair_t<F, O, top>>(pullUncons);
                        std::optional<pull_pair_t<F, O, top>> result = p;
                        return p;
                    } else {
                        std::optional<pull_pair_t<F, O, top>> result = std::nullopt;
                        return result;
                    }
                };
            pull<F, P, pull_variant_t<F, O, top>> pullUncons = underlying.uncons();
            pull<F, P, std::optional<std::pair<std::vector<O>, stream<F, O>>>> result = pullUncons.map(stitch);
            return result;
        }

        pull<F, O, top> toPull() const {
            return underlying;
        }
    };

    namespace io {

        // TODO: figure out Stream.bracket and closeAfterUse
        template<template<typename> typename F>
        stream<F, char> istream(std::istream& in, std::streamsize chunkSize) {
            std::function<std::optional<std::vector<char>> ()> readChunkThunk = [&in, chunkSize]() {
                if(in.eof()) {
                    std::optional<std::vector<char>> result = std::nullopt;
                    return result;
                } else {
                    std::vector<char> cv(chunkSize);
                    // TODO: I swear there was a read function that actually played nicely with cin
                    in.getline(cv.data(), cv.size());
                    std::streamsize newSize = in.gcount();
                    cv.resize(newSize);
                    cv.push_back('\n');
                    std::optional<std::vector<char>> result = cv;
                    return result;
                }
            };
            F<std::optional<std::vector<char>>> readChunk = F<std::optional<std::vector<char>>>::delay(readChunkThunk);
            stream<F, std::optional<std::vector<char>>> readChunks = stream<F, std::optional<std::vector<char>>>::repeatEval(readChunk);

            std::function<bool (std::optional<std::vector<char>>)> chunkExistsPredicate = [](std::optional<std::vector<char>> ov) {
                return ov.has_value();
            };
            stream<F, std::optional<std::vector<char>>> takeUntilEmptyChunk = readChunks.takeWhile(chunkExistsPredicate);

            std::function<std::vector<char> (std::optional<std::vector<char>>)> unwrapFullOptions = [](std::optional<std::vector<char>> ov) {
                return ov.value();
            };
            stream<F, std::vector<char>> nonEmptyChunks = takeUntilEmptyChunk.map(unwrapFullOptions);

            std::function<stream<F, char> (std::vector<char>)> emitVectorChunks = [](std::vector<char> v) {
                return stream<F, char>::chunk(v);
            };
            stream<F, char> result = nonEmptyChunks.flatMap(emitVectorChunks);

            return result;
        }

        template<template<typename> typename F>
        stream<F, char> cin(std::streamsize chunkSize) {
            return istream<F>(std::cin, chunkSize);
        }

        // TODO: figure out Stream.bracket and closeAfterUse
        template<template<typename> typename F>
        pipe<F, char, bot> ostream(std::ostream& out) {
            std::function<F<top> (std::vector<char>)> writeChunk = [&out](std::vector<char> chunk) {
                std::function<top ()> writeChunkThunk = [&out, chunk]() {
                    out.write(chunk.data(), chunk.size());
                    return top {};
                };
                return F<top>::delay(writeChunkThunk);
            };
            std::function<stream<F, bot> (stream<F, char>)> p = [&out, writeChunk](stream<F, char> in) {
                stream<F, std::vector<char>> inChunks = in.chunks();
                stream<F, bot> eachPrint = inChunks.foreach(writeChunk);
                return eachPrint;
            };
            return p;
        }

        template<template<typename> typename F>
        pipe<F, char, bot> cout() {
            return ostream<F>(std::cout);
        }

    };

}
