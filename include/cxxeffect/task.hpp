#pragma once
#include <functional>
#include <future>

namespace eff {
    template <class F, class T>
    using flatmap_t = decltype(std::declval<F>()(std::declval<T>()).unsafeRunSync());
    template <class F, class T>
    using map_t = decltype(std::declval<F>()(std::declval<T>()));
    template <class T>
    class task {
        std::function<T()> thunk;
       public:
        task() = default;
        task(task const&) = default;
        task(task&&) = default;
        task(std::function<T()> thunk) : thunk(std::move(thunk)) {}

        static task pure(T value) {
            return task([=]() { return value; });
        }
        static auto delay(std::function<T()> thunk) {
            return task(thunk);
        }

        template <class F>
        auto map(F func) const -> task<map_t<F, T>> {
            return {
                    [f = std::move(func), input = *this]() {
                    return f(input.unsafeRunSync());
                }
            };
        }

        template <class F>
        auto flatMap(F func) const -> task<flatmap_t<F, T>> {
            return {
                [f = func, ta = *this]() {
                    return f(ta.unsafeRunSync()).unsafeRunSync();
                }
            };
        }

        T unsafeRunSync() const {
            return thunk();
        }
        std::future<T> unsafeRunAsync() const {
            return std::async(std::launch::async, thunk);
        }
    };
    template <class F>
    task(F func) -> task<decltype(func())()>;
}
