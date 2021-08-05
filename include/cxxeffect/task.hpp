#pragma once
#include <functional>
#include <future>
#include <type_traits>

#include <conduit/util/concepts.hpp>

namespace eff {
    using std::coroutine_handle;

    // A Task is an awaitable that has a return type specified by return_t
    template <class Task>
    concept task_type = conduit::hard_awaitable<Task, typename Task::return_t>;

    // Task which doesn't suspend and simply returns a value it stores
    template <class Ret>
    struct pure_task : std::suspend_never {
        // A pure task has the value ready immediately, so it
        // never needs to suspend. We use std::suspend_never as a mixin
        using return_t = Ret;
        Ret value;

        using std::suspend_never::await_ready;
        using std::suspend_never::await_suspend;

        return_t await_resume() const {
            return value;
        }
    };

    // Task that obtains it's result from a function invocation
    template <std::invocable Func>
    struct lazy_task : std::suspend_never {
        using return_t = std::invoke_result_t<Func>;
        Func func;

        using std::suspend_never::await_ready;
        using std::suspend_never::await_suspend;

        return_t await_resume() const {
            return func();
        }
    };


    // task that maps a function onto the output of a different task
    template <
        task_type Task,
        // Specifies that Func must accept an input of type Task::return_t
        std::invocable<typename Task::return_t> Func>
    struct map_task : Task {
        Func func;

        using return_t = std::invoke_result_t<Func, typename Task::return_t>;
        using Task::await_ready;
        using Task::await_suspend;

        return_t await_resume() const {
            return func(Task::await_resume());
        }
    };
    template <
        task_type TaskA,
        std::invocable<typename TaskA::return_t> Func>
    struct flatmap_task {
        using TaskB = std::invoke_result_t<Func, typename TaskA::return_t>;

        static_assert(task_type<TaskB>, "The function given to flatmap needs to return a task");
        // return_t is the return type of the task returned by func
        using return_t = typename TaskB::return_t;
        TaskA taskA;
        Func func;

        bool taskA_ready = false;
        bool taskB_ready = false;
        TaskB taskB;
        return_t return_value;

        bool await_ready() {
            if((taskA_ready = taskA.await_ready())) {
                taskB = func(taskA.await_resume());
                if((taskB_ready = taskB.await_ready())) {
                    return_value = taskB.await_resume();
                    return true;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        // To-do: write await_suspend (contains non-trivial logic)
        // await_suspend will transfer control to a coroutine that'll await on
        // A and then B successively before to produce the result value
        // The handle to this coroutine will be returned from await_suspend
        // In order to allow for symmetric transfer
        // When that coroutine suspends, it'll then return control back to
        // the caller

        return_t await_resume() {
            return return_value;
        }
    };


    // This class represents a task that can be executed synchronously or
    // asynchronously, and produces a value
    template <class T>
    class task;

    namespace _internal {
        template <class T>
        struct is_task : std::false_type {};
        template <class T>
        struct is_task<task<T>> : std::true_type {};

        template <class T>
        constexpr bool is_task_v = is_task<T>::value;
    }

    // Concept that matches task<T> forall. T
    template <class Task>
    concept is_task = _internal::is_task_v<Task>;

    // Concept that matches function that matches all F such that
    // F satisfies F: T -> task<T2> for some T2
    template <class F, class T>
    concept task_bind = requires(F func, T value) {
        { func(value) } -> is_task;
    };

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
        template <task_bind<T> F>
        auto flatMap(F func) const -> std::invoke_result_t<F, T> {
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
