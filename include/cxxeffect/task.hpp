#pragma once
#include <functional>
#include <future>
#include <type_traits>

#include <conduit/util/concepts.hpp>

namespace eff {
    using std::coroutine_handle;
    using std::suspend_never;
    using std::suspend_always;
    using std::invoke_result_t;
    using conduit::hard_awaitable;
    using conduit::invocable;
    using conduit::same_as;

    // Tasks have a task category that determines whether they're immediate,
    // or asynchronousy. If a task's type can't be determined at compile time,
    // it needs to by handled as async
    enum class task_category_t : bool {
        // Immediate tasks return a value synchronously
        // (e.g pure task or lazy_task)
        immediate,
        // async tasks return a value asynchronously
        async
    };

    // A Task is an awaitable that has a return type specified by return_t
    template <class Task>
    concept task_type = hard_awaitable<Task, typename Task::return_t> && requires() {
        { Task::task_category } -> same_as<task_category_t>;
    };


    // Combining tasks (e.g, via flatmap) results in an immediate task
    // if all the tasks in the set are immediate, but if any task is async
    // it results in an async task
    template <task_type... Tasks>
    constexpr task_category_t deduce_task_category =
        ((Tasks::task_category == task_category_t::immediate) && ...)
        ? task_category_t::immediate
        : task_category_t::async;

    // Task which doesn't suspend and simply returns a value it stores
    template <class Ret>
    struct pure_task {

        using return_t = Ret;

        Ret value;

        // It's an immediate task since control is never transferred
        constexpr static auto task_category = task_category_t::immediate;
        // No task switch needs to occur. The value is always ready.
        constexpr bool await_ready() const noexcept { return true; }
        // await_suspend is a no-op
        constexpr void await_suspend(coroutine_handle<>) const noexcept {}

        return_t await_resume() const {
            return value;
        }
    };
    template <class Value>
    pure_task(Value value) -> pure_task<Value>;

    // Task that obtains it's result from a function invocation
    template <invocable Func>
    struct lazy_task {
        Func func;

        // The return type of a lazy task is the same as the type returned by func()
        using return_t = invoke_result_t<Func>;

        // It's an immediate task since control is never transferred
        constexpr static auto task_category = task_category_t::immediate;
        // No task switch needs to occur. The value is always ready.
        constexpr bool await_ready() const noexcept { return true; }
        // await_suspend is a no-op
        constexpr void await_suspend(coroutine_handle<>) const noexcept {}

        return_t await_resume() const {
            return func();
        }
    };
    template <invocable Func>
    lazy_task(Func func) -> lazy_task<Func>;


    // task that maps a function onto the output of a different task
    template <
        task_type Task,
        // Specifies that Func must accept an input of type Task::return_t
        invocable<typename Task::return_t> Func>
    struct map_task : Task {
        Func func;

        using return_t = invoke_result_t<Func, typename Task::return_t>;
        using Task::await_ready;
        using Task::await_suspend;

        // When you map a function on a task, it's category is the same as
        // the category of the task it's derived from
        using Task::task_category;

        return_t await_resume() const {
            return func(Task::await_resume());
        }
    };
    template <task_type Task, invocable<typename Task::return_t> Func>
    map_task(Task task, Func func) -> map_task<Task, Func>;

    template <
        class TaskA,
        class Func,
        task_category_t = deduce_task_category<TaskA, std::invoke_result_t<Func, typename TaskA::return_type>>>
    struct flatmap_impl;

    template <class TaskA, class Func>
    struct flatmap_impl<TaskA, Func, task_category_t::immediate> {
        // It's an immediate task since control is never transferred
        constexpr static auto task_category = task_category_t::immediate;
        // No task switch needs to occur. The value is always ready.
        constexpr bool await_ready() const noexcept { return true; }
        // await_suspend is a no-op
        constexpr void await_suspend(coroutine_handle<>) const noexcept {}

        using TaskB = std::invoke_result_t<Func, typename TaskA::return_type>;
        using return_t = typename TaskB::return_t;
        TaskA taskA;
        Func func;

        // Because it's an immediate task, we can obtain the value directly
        // simply by calling await_resume on taskA, passing that to func to
        // generate taskB, and then calling await_resume() on task B
        return_t await_resume() const {
            return func(taskA.await_resume()).await_resume();
        }
    };
    template <class TaskA, class Func>
    struct flatmap_impl<TaskA, Func, task_category_t::async> {
        constexpr static task_category_t task_category = task_category_t::async;
        using TaskB = std::invoke_result_t<Func, typename TaskA::return_type>;
        using return_t = typename TaskB::return_t;
        TaskA taskA;
        Func func;

        return_t result;
        // We always suspend the calling coroutine for async tasks
        constexpr bool await_ready() const noexcept {
            return false;
        }

        // To-do: write await_suspend (contains non-trivial logic)
        // await_suspend will transfer control to a coroutine that'll await on
        // A and then B successively before to produce the result value
        // The handle to this coroutine will be returned from await_suspend
        // In order to allow for symmetric transfer
        // When that coroutine suspends, it'll then return control back to
        // the caller

        return_t await_resume() {
            return result;
        }
    };

    template <
        task_type TaskA,
        invocable<typename TaskA::return_t> Func>
    struct flatmap_task : flatmap_impl<TaskA, Func> {
        using base = flatmap_impl<TaskA, Func>;

        using base::await_ready;
        using base::await_suspend;
        using base::await_resume;
        using base::return_t;
        using base::task_category;
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
        auto map(F func) const -> task<invoke_result_t<F, T>> {
            return {
                    [f = std::move(func), input = *this]() {
                    return f(input.unsafeRunSync());
                }
            };
        }

        // Takes a function which takes an input of type T, and produces a task
        // of type U. This is Equivilant to task T -> (T -> task U) -> task U
        template <task_bind<T> F>
        auto flatMap(F func) const -> invoke_result_t<F, T> {
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
