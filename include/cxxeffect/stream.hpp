#pragma once

#include <algorithm>
#include <exception>
#include <ios>
#include <iostream>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include <cxxeffect/eff.hpp>

namespace eff {
    // `pull` represents some step in a stream. A `pull` looks like
    //
    //    type Pull f o r =
    //      f ({ result : r } | { head : Vector o, tail : Pull f o r })
    //
    // That is, a `pull` on some effect type `f` represents a computation that
    // either returns a terminating result, or the next step of a stream.
    //
    // `pull` should be considered a "servant" of `stream`. It should be used
    // only in cases where an operation is not purely expressable with `stream`
    // or when it is definitively more performant to express with `pull`. This
    // is because `pull` is much less powerful when `R` != `top`; you cannot
    // assume that any other `R` will have only one possible value, which means
    // there are some functions you cannot write.
    template<template<typename> typename F, typename O, typename R>
    class pull;

    // This is the pair of the head chunk of output elements and the next pull step.
    // Might be called `pull_cons_t`.
    template <template <typename> class F, class O, class R>
    using pull_pair_t = std::pair<std::vector<O>, pull<F, O, R>>;

    // The result of a pull will either be a termination token (R) or a concatenation
    // of some output and a next step (pull_pair_t)
    template <template <typename> class F, class O, class R>
    using pull_variant_t = std::variant<R, pull_pair_t<F, O, R>>;

    // And the internals of a pull are an effectful result
    template <template <typename> class F, class O, class R>
    using pull_task_t = F<pull_variant_t<F, O, R>>;

    // `stream<F, O>` is equivalent to `pull<F, O, top>`. That is, a stream
    // is a pull where the termination token does not store any meaningful
    // data. The difference between `stream` and `pull` is that `stream` is a
    // monad over the output element types, while `pull` is a monad over the
    // result token type. `stream` is much more powerful and able to express
    // more transformations correctly due to `top` being a "terminal object".
    template<template<typename> typename F, typename O>
    class stream;

    // `pipe` isn't useful. It can be replaced with some invoke_result_t
    // wizardry instead.
    //
    // The gist is that you might have something like
    //
    //   pipe<F, uint8_t, bot> writeBytes(some async file handle)
    //
    // and you could do such as `myStreamOfBytes.through(writeBytes(myFileHandle))`
    // The examples page of https://fs2.io does a much better job at showing how
    // beautiful this can end up being.
    template<template<typename> typename F, typename A, typename B>
    using pipe = std::function<stream<F, B> (stream<F, A>)>;

    // I believe that raw_pull can be refactored into a singular
    // class much like task was. Where it just has a singular
    // `pull_task_t` field. But I'm not sure about that.
    template<template<typename> typename F, typename O, typename R>
    class raw_pull {
        public:
        virtual pull_task_t<F, O, R> step() const = 0;
    };

    template<template<typename> typename F, typename O, typename R>
    class raw_pull_eval final : public raw_pull<F, O, R> {
        F<R> fr;
        public:
        raw_pull_eval(F<R> a) : fr(a) {}

        pull_task_t<F, O, R> step() const {
            return fr.map([](R r) {
                return pull_variant_t<F, O, R>{r};
            });
        }
    };

    template<template<typename> typename F, typename O>
    class raw_pull_output final : public raw_pull<F, O, top> {
        std::vector<O> chunk;
        public:
        raw_pull_output(std::vector<O> a) : chunk(a) {}

        pull_task_t<F, O, top> step() const {
            return pull_task_t<F, O, top>::pure(
                pull_variant_t<F, O, top> {
                    std::pair(chunk, pull<F, O, top>::done())
                }
            );
        }
    };

    template<template<typename> typename F, typename O, typename P, typename R>
    class raw_pull_map_output final : public raw_pull<F, P, R> {
        pull<F, O, R> that;
        std::function<std::vector<P> (std::vector<O>)> f;
        public:
        raw_pull_map_output(pull<F, O, R> a, std::function<std::vector<P> (std::vector<O>)> b) : that(a), f(b) {}

        pull_task_t<F, P, R> step() const {
            std::function<pull_variant_t<F, P, R> (pull_variant_t<F, O, R>)> transformStep =
                [*this](pull_variant_t<F, O, R> stepResult) {
                    if(std::holds_alternative<pull_pair_t<F, O, R>>(stepResult)) {
                        pull_pair_t<F, O, R> p = std::get<pull_pair_t<F, O, R>>(stepResult);
                        std::vector<O> hd = p.first;
                        pull<F, O, R> tl = p.second;

                        std::vector<P> nhd = f(hd);
                        pull<F, P, R> ntl = tl.mapOutput(f);
                        pull_pair_t<F, P, R> np = std::make_pair(nhd, ntl);

                        pull_variant_t<F, P, R> result = np;
                        return result;
                    } else {
                        R r = std::get<R>(stepResult);
                        pull_variant_t<F, P, R> result = r;
                        return result;
                    }
                };
            pull_task_t<F, O, R> thatStep = that.step();
            return thatStep.map(transformStep);
        }
    };

    template<template<typename> typename F, typename O, typename R, typename S>
    class raw_pull_flatmap final : public raw_pull<F, O, S> {
        pull<F, O, R> that;
        std::function<pull<F, O, S> (R)> f;
        public:
        raw_pull_flatmap(pull<F, O, R> a, std::function<pull<F, O, S> (R)> b) : that(a), f(b) {}

        pull_task_t<F, O, S> step() const {
            std::function<pull_task_t<F, O, S> (pull_variant_t<F, O, R>)> transformStep =
                [*this](pull_variant_t<F, O, R> stepResult) {
                    if(std::holds_alternative<pull_pair_t<F, O, R>>(stepResult)) {
                        pull_pair_t<F, O, R> p = std::get<pull_pair_t<F, O, R>>(stepResult);
                        std::vector<O> hd = p.first;
                        pull<F, O, R> tl = p.second;

                        pull_pair_t<F, O, S> np = std::make_pair(hd, tl.flatMap(f));
                        pull_variant_t<F, O, S> v = np;
                        return pull_task_t<F, O, S>::pure(v);
                    } else {
                        R r = std::get<R>(stepResult);
                        return f(r).step();
                    }
                };
            pull_task_t<F, O, R> thatStep = that.step();
            return thatStep.flatMap(transformStep);
        }
    };

    template<template<typename> typename F, typename O, typename P>
    class raw_pull_flatmap_output final : public raw_pull<F, P, top> {
        pull<F, O, top> that;
        std::function<pull<F, P, top> (O)> f;
        public:
        raw_pull_flatmap_output(pull<F, O, top> a, std::function<pull<F, P, top> (O)> b) : that(a), f(b) {}

        pull_task_t<F, P, top> step() const {
            std::function<pull_task_t<F, P, top> (pull_variant_t<F, O, top>)> transformStep =
                [*this](pull_variant_t<F, O, top> stepResult) {
                    if(std::holds_alternative<pull_pair_t<F, O, top>>(stepResult)) {
                        pull_pair_t<F, O, top> p = std::get<pull_pair_t<F, O, top>>(stepResult);
                        std::vector<O> hd = p.first;
                        pull<F, O, top> tl = p.second;

                        std::shared_ptr<std::function<pull<F, P, top> (std::size_t)>> go = std::make_shared<std::function<pull<F, P, top> (std::size_t)>>();
                        *go = [*this, hd, tl, go](std::size_t idx) {
                            if(idx == hd.size()) {
                                return tl.flatMapOutput(f);
                            } else {
                                std::function<pull<F, P, top> (std::size_t)> goI = *go;
                                std::function<pull<F, P, top> (top)> next = [idx, goI](top t) {
                                    return goI(idx + 1);
                                };
                                return f(hd[idx]).flatMap(next);
                            }
                        };
                        std::function<pull<F, P, top> (std::size_t)> goI = *go;
                        return goI(0).step();
                    } else {
                        pull_variant_t<F, P, top> result = std::get<top>(stepResult);
                        return pull_task_t<F, P, top>::pure(result);
                    }
                };
            pull_task_t<F, O, top> thatStep = that.step();
            return thatStep.flatMap(transformStep);
        }
    };

    template<template<typename> typename F, typename O, typename R>
    class pull final {
        std::shared_ptr<raw_pull<F, O, R>> internal;
        public:
        pull(std::shared_ptr<raw_pull<F, O, R>> a) : internal(a) {}

        static pull<F, O, top> output(std::vector<O> os) {
            std::shared_ptr<raw_pull<F, O, top>> inner = std::shared_ptr<raw_pull<F, O, top>>(new raw_pull_output<F, O>(os));
            return pull(inner);
        }

        static pull<F, O, top> output1(O o) {
            std::vector<O> chunk = { o };
            std::shared_ptr<raw_pull<F, O, top>> inner = std::shared_ptr<raw_pull<F, O, top>>(new raw_pull_output<F, O>(chunk));
            return pull(inner);
        }

        static pull<F, O, top> done() {
            return pull<F, O, top>::pure(top {});
        }

        static pull<F, O, R> eval(F<R> fr) {
            std::shared_ptr<raw_pull<F, O, R>> inner = std::shared_ptr<raw_pull<F, O, R>>(new raw_pull_eval<F, O, R>(fr));
            return pull(inner);
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

        static pull<F, O, R> pure(R r) {
            F<R> fr = F<R>::pure(r);
            return pull<F, O, R>::eval(fr);
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
            std::shared_ptr<raw_pull<F, O, S>> inner = std::shared_ptr<raw_pull<F, O, S>>(new raw_pull_flatmap<F, O, R, S>(*this, f));
            return pull<F, O, S>(inner);
        }

        template<typename P>
        pull<F, P, top> flatMapOutput(std::function<pull<F, P, top> (O)> f) const {
            std::shared_ptr<raw_pull<F, P, top>> inner = std::shared_ptr<raw_pull<F, P, top>>(new raw_pull_flatmap_output<F, O, P>(*this, f));
            return pull<F, P, top>(inner);
        }

        template<typename S>
        pull<F, O, S> map(std::function<S (R)> f) const {
            std::function<pull<F, O, S> (R)> fThenPure = [f](R r) {
                S s = f(r);
                pull<F, O, S> fos = pull<F, O, S>::pure(s);
                return fos;
            };
            return flatMap(fThenPure);
        }

        template<typename P>
        pull<F, P, R> mapOutput(std::function<std::vector<P> (std::vector<O>)> f) const {
            std::shared_ptr<raw_pull<F, P, R>> inner = std::shared_ptr<raw_pull<F, P, R>>(new raw_pull_map_output<F, O, P, R>(*this, f));
            return pull<F, P, R>(inner);
        }

        pull<F, O, top> discard() const {
            return as<top>(top {});
        }

        // TODO: return a pull<F, O, ???<pull<F, O, ???>>> for the rest of the stream
        pull<F, O, top> takeWhile(std::function<bool (O)> predicate, bool takeFailure = false) const {
            std::function<pull<F, O, top> (pull_variant_t<F, O, R>)> transformUc =
                [predicate, takeFailure](pull_variant_t<F, O, R> step) {
                    if(std::holds_alternative<pull_pair_t<F, O, R>>(step)) {
                        pull_pair_t<F, O, R> p = std::get<pull_pair_t<F, O, R>>(step);
                        std::vector<O> hd = p.first;
                        pull<F, O, R> tl = p.second;

                        std::size_t size = hd.size();
                        std::size_t idx = std::find_if_not(hd.begin(), hd.end(), predicate) - hd.begin();

                        if(idx == size) {
                            pull<F, O, top> emitHd = pull<F, O, top>::output(hd);
                            std::function<pull<F, O, top> (top)> takeWhileTl = [tl, predicate, takeFailure](top t) {
                                return tl.takeWhile(predicate, takeFailure);
                            };
                            return emitHd.flatMap(takeWhileTl);
                        } else {
                            if(takeFailure) idx++;

                            std::vector<O> toEmit(hd.begin(), hd.begin() + idx);

                            // TODO: emit tail pull
                            return pull<F, O, top>::output(toEmit);
                        }
                    } else {
                        pull<F, O, top> result = pull<F, O, top>::pure(top {});
                        return result;
                    }
                };
            pull<F, O, pull_variant_t<F, O, R>> uc = uncons<O>();
            return uc.flatMap(transformUc);
        }

        stream<F, O> toStream() const {
            return stream(*this);
        }

        template<typename P>
        pull<F, P, pull_variant_t<F, O, R>> uncons() const {
            return pull<F, P, pull_variant_t<F, O, R>>::eval(step());
        }

        pull_task_t<F, O, R> step() const {
            return internal->step();
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
            return concatAllOutput.toStream();
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
            return underlying.mapOutput(chunkMapper).toStream();
        }

        template<typename P>
        stream<F, P> mapChunks(std::function<std::vector<P> (std::vector<O>)> f) const {
            return underlying.mapOutput(f).toStream();
        }

        stream<F, O> repeat() const {
            std::function<stream<F, O> ()> here = [*this]() {
                return this->repeat();
            };
            return append(here);
        }

        stream<F, O> takeWhile(std::function<bool (O)> predicate, bool takeFailure = false) const {
            return toPull().takeWhile(predicate, takeFailure).toStream();
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
