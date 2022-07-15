//
// Created by Aly Cerruti on 2022-07-15.
//

#ifndef CXXEFFECT_TASK_HPP
#define CXXEFFECT_TASK_HPP

#include <functional>
#include <future>
#include <optional>
#include <variant>

namespace eff {
    template<typename A> class task;
    template<typename A> class fiber;
    template<typename A> class resource;
    class dispatcher;
}

namespace eff {

    template<typename A>
    using callback = std::function<void (std::variant<std::exception, A>)>;

    template<typename A>
    class task final {

    public:
        template<typename B>
        task<B> map(auto f) requires
            std::regular_invocable<decltype(f), A> &&
            std::convertible_to<std::invoke_result_t<decltype(f), A>, B> {
            // TODO
        }

        template<typename B>
        task<B> as(B b) {
            // FIXME
            return map<B>([b](A) { return b; });
        }

        template<typename B>
        task<B> flatMap(auto f) requires
            std::regular_invocable<decltype(f), A> &&
            std::convertible_to<std::invoke_result_t<decltype(f), A>, task<B>> {
            // TODO
        }

        task<fiber<A>> start() {
            // TODO
        }

        template<typename B>
        task<std::variant<std::pair<A, fiber<B>>, std::pair<fiber<A>, B>>> race(task<B>) {
            // TODO
        }

    };

    template<typename A>
    task<A> pure(A a) {
        // TODO
    }

    template<typename A>
    task<A> delay(auto f) requires
        std::regular_invocable<decltype(f)> &&
        std::convertible_to<std::invoke_result_t<decltype(f)>, A> {
        // TODO
    }

    template<typename A>
    task<A> blocking(auto f) requires
        std::regular_invocable<decltype(f)> &&
        std::convertible_to<std::invoke_result_t<decltype(f)>, A> {
        // TODO
    }

    template<typename A>
    task<A> async(auto f) requires
        std::regular_invocable<decltype(f), callback<A>> &&
        std::convertible_to<std::invoke_result_t<decltype(f), callback<A>>, task<std::optional<task<top>>>> {
        // TODO
    }

    template<typename A>
    task<A> async_(auto f) requires
        std::regular_invocable<decltype(f), callback<A>> &&
        std::convertible_to<std::invoke_result_t<decltype(f), callback<A>>, void> {
        // FIXME
        return async<A>([f](callback<A> cb) {
            task<top> fire = delay<top>([f, cb]() {
                f(cb);
                return top();
            });
            return fire.as<std::optional<task<top>>>(std::nullopt);
        });
    }

    task<bot> never() {
        return async_<bot>([](const callback<bot>&) {});
    }

    template<typename A>
    class fiber final {

    public:
        task<top> cancel() {
            // TODO
        }

        task<A> join() {
            // TODO
        }

    };

    template<typename A>
    class resource final {

    public:
        template<typename B>
        task<B> use(auto f) requires
            std::regular_invocable<decltype(f), A> &&
            std::convertible_to<std::invoke_result_t<decltype(f), A>, task<B>> {
            // TODO
        }

        template<typename B>
        resource<B> map(auto f) requires
            std::regular_invocable<decltype(f), A> &&
            std::convertible_to<std::invoke_result_t<decltype(f), A>, B> {
            // TODO
        }

        template<typename B>
        resource<B> flatMap(auto f) requires
            std::regular_invocable<decltype(f), A> &&
            std::convertible_to<std::invoke_result_t<decltype(f), A>, resource<B>> {
            // TODO
        }
    };

    class dispatcher final {

    public:
        template<typename A>
        std::pair<std::future<A>, std::function<std::future<A> ()>> unsafeToFutureCancelable(task<A> fa) {
            // TODO
        }

        template<typename A>
        std::future<A> unsafeToFuture(task<A> fa) {
            return unsafeToFutureCancelable<A>(fa).first;
        }

        template<typename A>
        void unsafeRunAndForget(task<A> fa) {
            (void) unsafeToFutureCancelable(fa);
        }

    };

    resource<dispatcher> make_dispatcher() {
        // TODO
    }

}

#endif //CXXEFFECT_TASK_HPP
