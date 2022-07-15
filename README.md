# cxxeffect

## api goals

| type          | explanation                                         |
|---------------|-----------------------------------------------------|
| `⊥`           | bottom/empty/never type. cannot be created lawfully |
| `⊤`           | top/unit/void type. all instances are equal         |
| `callback<α>` | `α → ⊤`                                             |

### `task`

Represents a program with a specific result type.

| function   | type                                              |
|------------|---------------------------------------------------|
| `async`    | `(callback<α> → task<option<task<⊤>>>) → task<α>` |
| `async_`   | `(callback<α> → ⊤) → task<α>`                     |
| `blocking` | `(⊤ → α) → task<α>`                               |
| `never`    | `task<⊥>`                                         |
| `pure`     | `α → task<α>`                                     |
| `sync`     | `(⊤ → α) → task<α>`                               |
| `as`       | `task<α> → β → task<β>`                           |
| `flatMap`  | `task<α> → (α → task<β>) → task<β>`               |
| `map`      | `task<α> → (α → β) → task<β>`                     |
| `start`    | `task<α> → task<fiber<α>>`                        |

### `fiber`

Represents an in-progress computation.

| function | type      |
|----------|-----------|
| `cancel` | `task<⊤>` |
| `join`   | `task<α>` |

### `resource`

Represents a resource that can be acquired and released.

| function | type                                    |
|----------|-----------------------------------------|
| `use`    | `resource<α> → (α → task<β>) → task<β>` |

### `dispatcher`

A way to run pure code / tasks in impure / synchronous environments.

| function                    | type                                                    |
|-----------------------------|---------------------------------------------------------|
| `make_dispatcher`           | `resource<dispatcher>`                                  |
| `unsafeToFutureCancellable` | `dispatcher → task<α> → pair<future<α>, ⊤ → future<α>>` |
| `unsafeToFuture`            | `dispatcher → task<α> → future<α>`                      |
| `unsafeRunAndForget`        | `dispatcher → task<α> → ⊤`                              |
