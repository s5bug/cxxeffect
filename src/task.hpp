#pragma once

#include <exception>
#include <functional>
#include <memory>

namespace eff {

    template<typename A>
    class task;

    template<typename A>
    class raw_task {
        public:
        virtual A unsafeRunSync() const = 0;
    };

    template<typename A>
    class raw_task_pure final : public raw_task<A> {
        A value;
        public:
        raw_task_pure(A a) : value(a) {}

        A unsafeRunSync() const {
            return value;
        }
    };

    template<typename A>
    class raw_task_error final : public raw_task<A> {
        std::exception error;
        public:
        raw_task_error(std::exception a) : error(a) {}

        A unsafeRunSync() const {
            throw error;
        }
    };

    template<typename A>
    class raw_task_delay final : public raw_task<A> {
        std::function<A ()> thunk;
        public:
        raw_task_delay(std::function<A ()> a) : thunk(a) {}

        A unsafeRunSync() const {
            return thunk();
        }
    };

    template<typename A, typename B>
    class raw_task_flatmap final : public raw_task<B> {
        task<A> ta;
        std::function<task<B> (A)> f;
        public:
        raw_task_flatmap(task<A> a, std::function<task<B> (A)> b) : ta(a), f(b) {}

        B unsafeRunSync() const {
            A va = ta.unsafeRunSync();
            task<B> tb = f(va);
            B vb = tb.unsafeRunSync();
            return vb;
        }
    };

    template<typename A, typename B>
    class raw_task_map final : public raw_task<B> {
        task<A> ta;
        std::function<B (A)> f;
        public:
        raw_task_map(task<A> a, std::function<B (A)> b) : ta(a), f(b) {}

        B unsafeRunSync() const {
            A va = ta.unsafeRunSync();
            B vb = f(va);
            return vb;
        }
    };

    template<typename A>
    class task {
        std::shared_ptr<raw_task<A>> internal;

        public:
        task(std::shared_ptr<raw_task<A>> a) : internal(a) {}

        static task<A> pure(A a) {
            const auto inner = std::shared_ptr<raw_task<A>>(new raw_task_pure<A>(a));
            return task<A>(inner);
        }
        static task<A> error(std::exception error) {
            const auto inner = std::shared_ptr<raw_task<A>>(new raw_task_error<A>(error));
            return task<A>(inner);
        }
        static task<A> delay(std::function<A ()> thunk) {
            const auto inner = std::shared_ptr<raw_task<A>>(new raw_task_delay<A>(thunk));
            return task<A>(inner);
        }

        template<typename B>
        task<B> flatMap(std::function<task<B> (A)> f) const {
            const auto inner = std::shared_ptr<raw_task<B>>(new raw_task_flatmap<A, B>(*this, f));
            return task<B>(inner);
        }
        template<typename B>
        task<B> map(std::function<B (A)> f) const {
            const auto inner = std::shared_ptr<raw_task<B>>(new raw_task_map<A, B>(*this, f));
            return task<B>(inner);
        }

        A unsafeRunSync() const {
            return internal->unsafeRunSync();
        }
    };

}
