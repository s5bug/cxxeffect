#pragma once
#include <functional>
#include <future>
#include <type_traits>

namespace eff {
    // This class represents a task that can be executed synchronously or
    // asynchronously, and produces a value
    template <class T>
    class task {
        // An internal representation of the task itself
        std::function<T()> thunk;
       public:
        // The type returned by this task, when evaluated
        using type = T;

        task() = default;
        task(task const&) = default;
        task(task&&) = default;

        // Constructs task from std::function which represents thunk.
        // This is equivilant to task<T>::delay(thunk)
        task(std::function<T()> thunk) : thunk(std::move(thunk)) {}

        // Creates a task which simply returns a value
        static task pure(T value) {
            return task([=]() { return value; });
        }

        // Creates a task that executes some function when run
        static auto delay(std::function<T()> thunk) {
            return task(thunk);
        }

        // Maps a function over a task
        template <class F>
        auto map(F func) const -> task<std::invoke_result_t<F, T>> {
            return {
                    [f = std::move(func), input = *this]() {
                    return f(input.unsafeRunSync());
                }
            };
        }

        // Takes a function which takes an input of type T, and produces a task
        // of type U. This is Equivilant to task T -> (T -> task U) -> task U
        template <class F>
        auto flatMap(F func) const
            -> task<typename std::invoke_result_t<F, T>::type> {
            return {
                [f = func, ta = *this]() {
                    return f(ta.unsafeRunSync()).unsafeRunSync();
                }
            };
        }

        // Evaluates the thunk represented by this task
        T unsafeRunSync() const {
            return thunk();
        }

        // Runs task asynchronously
        std::future<T> unsafeRunAsync() const {
            return std::async(std::launch::async, thunk);
        }
    };

    // Allows the type of a task to be deduced
    // from the thunk passed to the constructor
    template <class F>
    task(F func) -> task<decltype(func())()>;
}
