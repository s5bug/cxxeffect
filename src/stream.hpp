#pragma once

#include <algorithm>
#include <exception>
#include <ios>
#include <iostream>
#include <optional>
#include <type_traits>
#include <vector>

#include "eff.hpp"

namespace eff {

    template<template<typename> typename F, typename O, typename R>
    class pull;

    template<template<typename> typename F, typename O>
    class stream;

    template<template<typename> typename F, typename A, typename B>
    using pipe = std::function<stream<F, B> (stream<F, A>)>;

    template<template<typename> typename F, typename O, typename R>
    class raw_pull {
        public:
        virtual top forcePolymorphic() { return top {}; }
    };

    template<template<typename> typename F, typename O, typename R>
    class raw_pull_action : public raw_pull<F, O, R> {};

    template<template<typename> typename F, typename O>
    class raw_pull_action_output final : public raw_pull_action<F, O, top> {
        std::vector<O> values;
        public:
        raw_pull_action_output(std::vector<O> a) : values(a) {}
    };

    template<template<typename> typename G, template<typename> typename F, typename O>
    class raw_pull_action_translate final : public raw_pull_action<F, O, top> {
        pull<G, O, top> stream;
        functionk<G, F> fk;
        public:
        raw_pull_action_translate(pull<G, O, top> a, functionk<G, F> b) : stream(a), fk(b) {}
    };

    template<template<typename> typename F, typename O, typename P>
    class raw_pull_action_map_output final : public raw_pull_action<F, P, top> {
        pull<F, O, top> stream;
        and_then<O, P> fun;
        public:
        raw_pull_action_map_output(pull<F, O, top> a, and_then<O, P> b) : stream(a), fun(b) {}
    };

    template<template<typename> typename F, typename O, typename P>
    class raw_pull_action_flatmap_output final : public raw_pull_action<F, P, top> {
        pull<F, O, top> stream;
        std::function<pull<F, P, top> (O)> fun;
        public:
        raw_pull_action_flatmap_output(pull<F, O, top> a, std::function<pull<F, P, top> (O)> b) : stream(a), fun(b) {}
    };

    template<template<typename> typename G, typename P, template<typename> typename F, typename O>
    class raw_pull_action_uncons final : public raw_pull_action<G, P, std::optional<std::pair<std::vector<O>, pull<F, O, top>>>> {
        pull<F, O, top> stream;
        public:
        raw_pull_action_uncons(pull<F, O, top> a) : stream(a) {}
    };

    template<template<typename> typename F, typename O, typename R>
    class raw_pull_action_effect : public raw_pull_action<F, O, R> {};

    template<template<typename> typename F, typename O, typename R>
    class raw_pull_action_effect_eval final : public raw_pull_action_effect<F, O, R> {
        F<R> value;
        public:
        raw_pull_action_effect_eval(F<R> a) : value(a) {}
    };

    template<template<typename> typename F, typename O, typename R>
    class raw_pull_terminal : public raw_pull<F, O, R> {};

    template<template<typename> typename F, typename O, typename R>
    class raw_pull_terminal_succeeded final : public raw_pull_terminal<F, O, R> {
        R value;
        public:
        raw_pull_terminal_succeeded(R a) : value(a) {}

        R getValue() {
            return value;
        }
    };

    template<template<typename> typename F, typename O, typename R>
    class raw_pull_terminal_failed final : public raw_pull_terminal<F, O, R> {
        std::exception error;
        public:
        raw_pull_terminal_failed(std::exception a) : error(a) {}
    };

    template<template<typename> typename F, typename O, typename X, typename R>
    class raw_pull_bind final : public raw_pull<F, O, R> {
        pull<F, O, X> step;
        std::function<pull<F, O, R> (std::shared_ptr<raw_pull_terminal<F, O, X>>)> cont;
        public:
        raw_pull_bind(pull<F, O, X> a, std::function<pull<F, O, R> (std::shared_ptr<raw_pull_terminal<F, O, X>>)> b) : step(a), cont(b) {}
    };

    template<template<typename> typename F, typename O, typename R>
    class pull final {
        std::shared_ptr<raw_pull<F, O, R>> internal;
        public:
        pull(std::shared_ptr<raw_pull<F, O, R>> a) : internal(a) {}

        static pull<F, O, top> output(std::vector<O> os) {
            std::shared_ptr<raw_pull<F, O, top>> inner = std::shared_ptr<raw_pull<F, O, top>>(new raw_pull_action_output<F, O>(os));
            return pull(inner);
        }

        static pull<F, O, top> output1(O o) {
            std::vector<O> chunk = { o };
            std::shared_ptr<raw_pull<F, O, top>> inner = std::shared_ptr<raw_pull<F, O, top>>(new raw_pull_action_output<F, O>(chunk));
            return pull(inner);
        }

        static pull<F, O, top> done() {
            std::shared_ptr<raw_pull<F, O, top>> inner = std::shared_ptr<raw_pull<F, O, top>>(new raw_pull_terminal_succeeded<F, O, top>(top {}));
            return pull(inner);
        }

        static pull<F, O, R> eval(F<R> fr) {
            std::shared_ptr<raw_pull<F, O, R>> inner = std::shared_ptr<raw_pull<F, O, R>>(new raw_pull_action_effect_eval<F, O, R>(fr));
            return pull(inner);
        }

        template<typename P>
        static pull<F, P, R> flatMapOutput(pull<F, O, R> stream, std::function<pull<F, P, R> (O)> f) {
            // TODO: Handle this construction more efficiently
            std::shared_ptr<raw_pull<F, P, R>> mapped = std::shared_ptr<raw_pull<F, P, R>>(new raw_pull_action_flatmap_output(stream, f));
            return pull<F, P, R>(mapped);
        }

        static std::function<pull<F, O, top> (R)> loop(std::function<pull<F, O, std::optional<R>> (R)> f) {
            std::function<pull<F, O, top> (R)> l = [f](R r) {
                pull<F, O, std::optional<R>> p = f(r);
                std::function<pull<F, O, top> (std::optional<R>)> pflm = [f](std::optional<R> entry) {
                    if(entry.has_value()) {
                        std::function<pull<F, O, top> (R)> step = loop(f);
                        pull<F, O, top> result = step(entry.value());
                        return result;
                    } else {
                        pull<F, O, top> result = pull<F, O, top>::done();
                        return result;
                    }
                };
                pull<F, O, top> result = p.flatMap(pflm);
                return result;
            };
            return l;
        }

        template<typename P>
        static pull<F, P, R> mapOutput(pull<F, O, R> stream, std::function<P (O)> f) {
            // TODO: Handle this construction more efficiently
            std::shared_ptr<raw_pull<F, P, R>> mapped = std::shared_ptr<raw_pull<F, P, R>>(new raw_pull_action_map_output(stream, and_then<O, P>::of(f)));
            return pull<F, P, R>(mapped);
        }

        static pull<F, O, R> pure(R r) {
            std::shared_ptr<raw_pull<F, O, R>> inner = std::shared_ptr<raw_pull<F, O, R>>(new raw_pull_terminal_succeeded<F, O, R>(r));
            return pull(inner);
        }

        template<typename P>
        static pull<F, P, std::optional<std::pair<std::vector<O>, pull<F, O, top>>>> uncons(pull<F, O, top> s) {
            std::shared_ptr<raw_pull<F, P, std::optional<std::pair<std::vector<O>, pull<F, O, top>>>>> inner =
                std::shared_ptr<raw_pull<F, P, std::optional<std::pair<std::vector<O>, pull<F, O, top>>>>>(new raw_pull_action_uncons<F, P, F, O>(s));
            return pull<F, P, std::optional<std::pair<std::vector<O>, pull<F, O, top>>>>(inner);
        }

        template<typename S>
        pull<F, O, S> as(S s) const {
            std::function<S (R)> constFunction = [s](R r) {
                return s;
            };
            return map(constFunction);
        }

        template<typename S>
        pull<F, O, S> flatMap(std::function<pull<F, O, S> (R)> f) const {
            std::function<pull<F, O, S> (std::shared_ptr<raw_pull_terminal<F, O, R>>)> cont = [f](std::shared_ptr<raw_pull_terminal<F, O, R>> e) {
                if(std::shared_ptr<raw_pull_terminal_succeeded<F, O, R>> res = std::dynamic_pointer_cast<raw_pull_terminal_succeeded<F, O, R>>(e)) {
                    R value = res->getValue();
                    try {
                        pull<F, O, S> result = f(value);
                        return result;
                    } catch (const std::exception& e) {
                        std::shared_ptr<raw_pull<F, O, S>> inner = std::shared_ptr<raw_pull<F, O, S>>(new raw_pull_terminal_failed<F, O, S>(e));
                        return pull<F, O, S>(inner);
                    }
                } else {
                    std::shared_ptr<raw_pull<F, O, S>> coerce = std::dynamic_pointer_cast<raw_pull<F, O, S>>(e);
                    return pull<F, O, S>(coerce);
                }
            };
            std::shared_ptr<raw_pull<F, O, S>> inner = std::shared_ptr<raw_pull<F, O, S>>(new raw_pull_bind(*this, cont));
            return pull<F, O, S>(inner);
        }

        template<typename S>
        pull<F, O, S> map(std::function<S (R)> f) const {
            // TODO: Figure out how to handle more efficient `map`s
            std::function<pull<F, O, S> (R)> fThenPure = [f](R r) {
                S s = f(r);
                pull<F, O, S> fos = pull<F, O, S>::pure(s);
                return fos;
            };
            return flatMap(fThenPure);
        }

        pull<F, O, top> terminate() const {
            return as<top>(top {});
        }

        stream<F, O> toStream() const {
            return stream(*this);
        }
    };

    template<template<typename> typename F, typename O>
    class stream_to_pull final {
        stream<F, O> self;
        public:
        stream_to_pull(stream<F, O> a) : self(a) {}

        template<typename P>
        pull<F, P, std::optional<std::pair<std::vector<O>, stream<F, O>>>> uncons() const {
            pull<F, P, std::optional<std::pair<std::vector<O>, pull<F, O, top>>>> pullUncon = pull<F, O, top>::template uncons<P>(self.getUnderlying());
            std::function<std::optional<std::pair<std::vector<O>, stream<F, O>>> (std::optional<std::pair<std::vector<O>, pull<F, O, top>>>)> restream =
                [](std::optional<std::pair<std::vector<O>, pull<F, O, top>>> occ) {
                    std::optional<std::pair<std::vector<O>, stream<F, O>>> result;
                    if(occ.has_value()) {
                        std::pair<std::vector<O>, pull<F, O, top>> pair = occ.value();
                        std::pair<std::vector<O>, stream<F, O>> newPair = std::make_pair(pair.first, stream(pair.second));
                        result = newPair;
                    } else {
                        result = std::nullopt;
                    }
                    return result;
                };
            pull<F, P, std::optional<std::pair<std::vector<O>, stream<F, O>>>> stitch = pullUncon.map(restream);
            return stitch;
        }

        pull<F, O, std::optional<stream<F, O>>> takeWhile(std::function<bool (O)> predicate, bool takeFailure = false) const {
            pull<F, O, std::optional<std::pair<std::vector<O>, stream<F, O>>>> uncon = uncons<O>();
            std::function<pull<F, O, std::optional<stream<F, O>>> (std::optional<std::pair<std::vector<O>, stream<F, O>>>)> handleTermination =
                [predicate, takeFailure](std::optional<std::pair<std::vector<O>, stream<F, O>>> uc) {
                    if(uc.has_value()) {
                        std::vector<O> hd = uc.value().first;
                        stream<F, O> tl = uc.value().second;

                        typename std::vector<O>::iterator it = std::find_if_not(hd.begin(), hd.end(), predicate);

                        typename std::vector<O>::size_type hds = hd.size();
                        typename std::vector<O>::size_type hdi = it - hd.begin();
                        if(hdi != hds) {
                            if(takeFailure) hdi++;
                            std::vector<O> newChunk(hd.begin(), hd.begin() + hdi);

                            pull<F, O, top> outputSuccesses = pull<F, O, top>::output(newChunk);
                            std::function<pull<F, O, std::optional<stream<F, O>>> (top)> thenEmitRemaining = [it, hd, hdi, hds, tl](top t) {
                                std::vector<O> remainingInChunk(hd.begin() + hdi, hd.begin() + hds);

                                stream<F, O> tlWithRemanining = tl.cons(remainingInChunk);
                                std::optional<stream<F, O>> tlo = tlWithRemanining;
                                pull<F, O, std::optional<stream<F, O>>> result = pull<F, O, std::optional<stream<F, O>>>::pure(tlo);
                                return result;
                            };
                            return outputSuccesses.flatMap(thenEmitRemaining);
                        } else {
                            pull<F, O, top> outputSuccesses = pull<F, O, top>::output(hd);
                            std::function<pull<F, O, std::optional<stream<F, O>>> (top)> thenContinue = [tl, predicate, takeFailure](top t) {
                                return tl.toPull().takeWhile(predicate, takeFailure);
                            };
                            return outputSuccesses.flatMap(thenContinue);
                        }
                    } else {
                        std::optional<stream<F, O>> result = std::nullopt;
                        return pull<F, O, std::optional<stream<F, O>>>::pure(result);
                    }
                };
            return uncon.flatMap(handleTermination);
        }
    };

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
            throw std::logic_error("Not implemented! I'm tired.");
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
            std::function<pull<F, std::vector<O>, std::optional<stream<F, O>>> (stream_to_pull<F, O>)> repeater = [](stream_to_pull<F, O> stp) {
                pull<F, std::vector<O>, std::optional<std::pair<std::vector<O>, stream<F, O>>>> uc = stp.template uncons<std::vector<O>>();
                std::function<pull<F, std::vector<O>, std::optional<stream<F, O>>> (std::optional<std::pair<std::vector<O>, stream<F, O>>>)> emitChunks =
                    [](std::optional<std::pair<std::vector<O>, stream<F, O>>> maybeChunk) {
                        if(maybeChunk.has_value()) {
                            std::pair<std::vector<O>, stream<F, O>> pair = maybeChunk.value();
                            pull<F, std::vector<O>, top> outputChunk = pull<F, std::vector<O>, top>::output1(pair.first);
                            std::optional<stream<F, O>> definedTail = pair.second;
                            pull<F, std::vector<O>, std::optional<stream<F, O>>> resultTail = outputChunk.as(definedTail);
                            return resultTail;
                        } else {
                            std::optional<stream<F, O>> result = std::nullopt;
                            pull<F, std::vector<O>, std::optional<stream<F, O>>> single = pull<F, std::vector<O>, std::optional<stream<F, O>>>::pure(result);
                            return single;
                        }
                    };
                return uc.flatMap(emitChunks);
            };
            return repeatPull(repeater);
        }

        stream<F, O> cons(std::vector<O> chunk) const {
            if(chunk.empty()) {
                return *this;
            } else {
                stream<F, O> chunkStream = stream<F, O>::chunk(chunk);
                std::function<stream<F, O> ()> thenThis = [this]() { return *this; };
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
            std::function<pull<F, P, top> (O)> otherUnderlying = [f](O o) {
                return f(o).getUnderlying();
            };
            pull<F, P, top> flatMapUnderlying = pull<F, O, top>::flatMapOutput(underlying, otherUnderlying);
            return stream<F, P>(flatMapUnderlying);
        }

        stream<F, bot> foreach(std::function<F<top> (O)> f) const {
            std::function<stream<F, bot> (O)> execElement = [f](O o) {
                return stream<F, bot>::exec(f(o));
            };
            return flatMap(execElement);
        }

        template<typename P>
        stream<F, P> map(std::function<P (O)> f) const {
            return pull<F, O, top>::mapOutput(underlying, f).toStream();
        }

        stream<F, O> repeat() const {
            std::function<stream<F, O> ()> here = [this]() {
                return this->repeat();
            };
            return append(here);
        }

        template<typename P>
        stream<F, P> repeatPull(std::function<pull<F, P, std::optional<stream<F, O>>> (stream_to_pull<F, O>)> f) const {
            std::function<pull<F, P, std::optional<stream_to_pull<F, O>>> (stream_to_pull<F, O>)> looper = [f](stream_to_pull<F, O> p) {
                pull<F, P, std::optional<stream<F, O>>> step = f(p);
                std::function<std::optional<stream_to_pull<F, O>> (std::optional<stream<F, O>>)> mapInside = [](std::optional<stream<F, O>> so) {
                    std::optional<stream_to_pull<F, O>> result;
                    if(so.has_value()) {
                        stream_to_pull<F, O> ep = so.value().toPull();
                        result = ep;
                    } else {
                        result = std::nullopt;
                    }
                    return result;
                };
                return step.map(mapInside);
            };
            std::function<pull<F, P, top> (stream_to_pull<F, O>)> looped = pull<F, P, stream_to_pull<F, O>>::loop(looper);
            return looped(toPull()).toStream();
        }

        stream<F, O> takeWhile(std::function<bool (O)> predicate, bool takeFailure = false) const {
            return toPull().takeWhile(predicate, takeFailure).terminate().toStream();
        }

        template<typename P>
        stream<F, P> through(pipe<F, O, P> p) const {
            return p(*this);
        }

        stream_to_pull<F, O> toPull() const {
            return stream_to_pull(*this);
        }

        pull<F, O, top> getUnderlying() const {
            return underlying;
        }
    };

    namespace io {

        // TODO: figure out Stream.bracket and closeAfterUse
        template<template<typename> typename F>
        stream<F, char> istream(std::istream& in, std::streamsize chunkSize) {
            std::function<std::optional<std::vector<char>> ()> readThunk = [&in, chunkSize]() {
                std::optional<std::vector<char>> result;
                if(in.eof()) {
                    result = std::nullopt;
                } else {
                    std::vector<char> out(chunkSize, 0);
                    in.read(out.data(), out.size());
                    out.resize(in.gcount());
                    result = out;
                }
                return result;
            };
            // TODO: blocking instead of delay here
            F<std::optional<std::vector<char>>> readF = F<std::optional<std::vector<char>>>::delay(readThunk);
            stream<F, std::optional<std::vector<char>>> readS = stream<F, std::optional<std::vector<char>>>::repeatEval(readF);
            std::function<bool (std::optional<std::vector<char>>)> hasInput = [](std::optional<std::vector<char>> ov) { return ov.has_value(); };
            stream<F, std::optional<std::vector<char>>> readW = readS.takeWhile(hasInput);
            std::function<std::vector<char> (std::optional<std::vector<char>>)> unwrap = [](std::optional<std::vector<char>> ov) { return ov.value(); };
            stream<F, std::vector<char>> readV = readW.map(unwrap);
            std::function<stream<F, char> (std::vector<char>)> emitVector = [](std::vector<char> v) { return stream<F, char>::chunk(v); };
            stream<F, char> unchunked = readV.flatMap(emitVector);
            return unchunked;
        }

        template<template<typename> typename F>
        stream<F, char> cin(std::streamsize chunkSize) {
            return istream<F>(std::cin, chunkSize);
        }

        // TODO: figure out Stream.bracket and closeAfterUse
        template<template<typename> typename F>
        pipe<F, char, bot> ostream(std::ostream& out) {
            std::function<F<top> (std::vector<char>)> writeF = [&out](std::vector<char> vec) {
                std::function<top ()> thunk = [&out, vec]() {
                    out.write(vec.data(), vec.size());
                    return top {};
                };
                // TODO: blocking instead of delay here
                return F<top>::delay(thunk);
            };
            std::function<top ()> flushThunk = [&out]() {
                out.flush();
                return top {};
            };
            F<top> flushF = F<top>::delay(flushThunk);
            std::function<stream<F, bot> (stream<F, char>)> putChunks = [&out, writeF, flushF](stream<F, char> chars) {
                stream<F, std::vector<char>> chunks = chars.chunks();
                stream<F, bot> writeEach = chunks.foreach(writeF);
                std::function<stream<F, bot> ()> thenFlush = [flushF]() {
                    return stream<F, bot>::exec(flushF);
                };
                return writeEach.append(thenFlush);
            };
            return putChunks;
        }

        template<template<typename> typename F>
        pipe<F, char, bot> cout() {
            return ostream<F>(std::cout);
        }

    };

}
