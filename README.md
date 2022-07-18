# cxxeffect

## api goals

| type          | explanation |
|---------------|--|
| `⊥`           | bottom/empty/never type. cannot be created lawfully |
| `⊤`           | top/unit/void type. all instances are equal to `eff::top()` |
| `α + β`       | either `α` or `β` |
| `α × β`       | both `α` and `β` |
| `callback<α>` | `(exception + α) → ⊤`, a callback that accepts a failure or success |
| `outcome<α>`  | `⊤ + exception + α`, either cancelled (`⊤`), errored, or completed |
| `poll`        | a polymorphic `(A : Type) → task<A> → task<A>` |

### `task`

Represents a program with a specific result type.

| function       | type                                                             |
|----------------|------------------------------------------------------------------|
| `async`        | `(callback<α> → task<option<task<⊤>>>) → task<α>`                |
| `async_`       | `(callback<α> → ⊤) → task<α>`                                    |
| `blocking`     | `(⊤ → α) → task<α>`                                              |
| `bracket`      | `task<α> → (α → task<β>) → (α → outcome<β> → task<⊤>) → task<β>` |
| `delay`        | `(⊤ → α) → task<α>`                                              |
| `never`        | `task<⊥>`                                                        |
| `pure`         | `α → task<α>`                                                    |
| `race`         | `task<α> → task<β> → task<(α × fiber<β>) + (fiber<α> × β)>`      |
| `as`           | `task<α> → β → task<β>`                                          |
| `flatMap`      | `task<α> → (α → task<β>) → task<β>`                              |
| `map`          | `task<α> → (α → β) → task<β>`                                    |
| `start`        | `task<α> → task<fiber<α>>`                                       |
| `uncancelable` | `(poll → task<α>) → task<α>`                                     |

### `fiber`

Represents an in-progress computation.

| function | type               |
|----------|--------------------|
| `cancel` | `task<⊤>`          |
| `join`   | `task<outcome<α>>` |

### `resource`

Represents a resource that can be acquired and released.

| function  | type                                            |
|-----------|-------------------------------------------------|
| `flatMap` | `resource<α> → (α → resource<β>) → resource<β>` |
| `map`     | `resource<α> → (α → β) → resource<β>`           |
| `use`     | `resource<α> → (α → task<β>) → task<β>`         |

### `dispatcher`

A way to run pure code / tasks in impure / synchronous environments.

| function                    | type                                                   |
|-----------------------------|--------------------------------------------------------|
| `make_dispatcher`           | `resource<dispatcher>`                                 |
| `unsafeToFutureCancellable` | `dispatcher → task<α> → (future<α> × (⊤ → future<α>))` |
| `unsafeToFuture`            | `dispatcher → task<α> → future<α>`                     |
| `unsafeRunAndForget`        | `dispatcher → task<α> → ⊤`                             |
