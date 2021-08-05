#pragma once

#include <tuple>
#include <variant>
#include <vector>
#include <functional>
#include <memory>
#include <optional>

namespace eff {
    using top = std::monostate;

    template<template<typename> typename F, typename O, typename R>
    class pull;

    template <template <typename> class F, class O, class R>
    using pull_pair_t = std::pair<std::vector<O>, pull<F, O, R>>;

    template <template <typename> class F, class O, class R>
    using pull_variant_t = std::variant<R, pull_pair_t<F, O, R>>;

    template <template <typename> class F, class O, class R>
    using pull_task_t = F<pull_variant_t<F, O, R>>;

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

        template<typename P>
        pull<F, P, pull_variant_t<F, O, R>> uncons() const {
            return pull<F, P, pull_variant_t<F, O, R>>::eval(step());
        }

        pull_task_t<F, O, R> step() const {
            return internal->step();
        }
    };

}
