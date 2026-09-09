```{title} clang-tidy - pybind-cast-in-lock
```

# pybind-cast-in-lock

Finds calls to `pybind11::cast` or `pybind11::handle::cast` executed while holding
a synchronization lock guard (such as `std::lock_guard`, `std::unique_lock`,
`std::scoped_lock`, or `absl::MutexLock`).

In multi-threaded and free-threaded Python environments (PEP 703),
`pybind11::cast` accesses pybind11's type registry and Python's object allocator.
When Python's cyclic garbage collector initiates a Stop-The-World (STW) pause,
it waits for all attached threads to reach a safepoint. A thread holding an
application mutex that enters `pybind11::cast` can park at a safepoint while
holding the mutex. If other threads blocked on that mutex cannot reach a safepoint,
the runtime deadlocks.

To avoid deadlocks, narrow the scope of the lock so it is released before
converting objects to Python:

```cpp
// Incorrect: lock held across py::cast
absl::MutexLock lock(GetMutex());
return py::cast(FindResource(name));

// Correct: lock released before calling py::cast
const Resource* resource = nullptr;
{
  absl::MutexLock lock(GetMutex());
  resource = FindResource(name);
}
return py::cast(resource);
```

## Options

.. option:: LockGuards

   A semicolon-separated list of names of RAII lock guard types.
   Defaults to `::std::lock_guard;::std::unique_lock;::std::scoped_lock;::std::shared_lock;::absl::MutexLock;::absl::ReaderMutexLock;::absl::WriterMutexLock`.
   Types annotated with `[[clang::scoped_lockable]]` are also matched automatically.
