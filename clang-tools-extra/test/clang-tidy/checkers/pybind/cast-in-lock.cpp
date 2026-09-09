// RUN: %check_clang_tidy -std=c++17-or-later %s pybind-cast-in-lock %t

namespace std {
template <typename T>
class lock_guard {
public:
  explicit lock_guard(T &);
  ~lock_guard();
};

template <typename T>
class unique_lock {
public:
  explicit unique_lock(T &);
  ~unique_lock();
};

template <typename... Ts>
class scoped_lock {
public:
  explicit scoped_lock(Ts &...);
  ~scoped_lock();
};

template <typename Mutex>
class shared_lock {
public:
  explicit shared_lock(Mutex &);
  ~shared_lock();
};

class mutex {};
} // namespace std

namespace absl {
class Mutex {};
class [[clang::scoped_lockable]] MutexLock {
public:
  explicit MutexLock(Mutex *);
  ~MutexLock();
};
class ReaderMutexLock {
public:
  explicit ReaderMutexLock(Mutex *);
  ~ReaderMutexLock();
};
class WriterMutexLock {
public:
  explicit WriterMutexLock(Mutex *);
  ~WriterMutexLock();
};
} // namespace absl

class [[clang::scoped_lockable]] CustomScopedLock {
public:
  CustomScopedLock();
  ~CustomScopedLock();
};

namespace pybind11 {
class handle {
public:
  template <typename T>
  T cast() const;
  explicit operator bool() const;
};

class object : public handle {};

enum class return_value_policy {
  reference,
  take_ownership,
  copy
};

template <typename T>
object cast(T &&, return_value_policy = return_value_policy::copy);

} // namespace pybind11

namespace other {
template <typename T>
int cast(T &&);
} // namespace other

namespace mock::pybind11 {
template <typename T>
int cast(T &&);
} // namespace mock::pybind11

namespace py = pybind11;

std::mutex &get_std_mutex();
absl::Mutex &get_absl_mutex();
void Consume(py::object);

struct ErrorSpace {
  static const ErrorSpace *Find(const char *);
  static const ErrorSpace *CanonicalErrorSpace();
};

void test_std_lock_guard(int x) {
  std::lock_guard<std::mutex> lock(get_std_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  py::cast(x);
  // CHECK-MESSAGES: :[[@LINE-3]]:31: note: lock 'lock' acquired here
}

void test_std_unique_lock(int x) {
  std::unique_lock<std::mutex> u_lock(get_std_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:14: warning: do not call 'pybind11::cast' while holding lock 'u_lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  auto res = py::cast(x);
  // CHECK-MESSAGES: :[[@LINE-3]]:32: note: lock 'u_lock' acquired here
}

void test_std_scoped_lock(int x) {
  std::scoped_lock<std::mutex> s_lock(get_std_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: do not call 'pybind11::cast' while holding lock 's_lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  pybind11::cast(x);
  // CHECK-MESSAGES: :[[@LINE-3]]:32: note: lock 's_lock' acquired here
}

void test_std_shared_lock(int x) {
  std::shared_lock<std::mutex> s_lock(get_std_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: do not call 'pybind11::cast' while holding lock 's_lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  py::cast(x);
  // CHECK-MESSAGES: :[[@LINE-3]]:32: note: lock 's_lock' acquired here
}

py::object test_absl_mutex_lock(const char *name) {
  absl::MutexLock lock(&get_absl_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:10: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  return py::cast(ErrorSpace::Find(name), py::return_value_policy::reference);
  // CHECK-MESSAGES: :[[@LINE-3]]:19: note: lock 'lock' acquired here
}

py::object test_absl_reader_mutex_lock(const char *name) {
  absl::ReaderMutexLock rlock(&get_absl_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:10: warning: do not call 'pybind11::cast' while holding lock 'rlock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  return py::cast(ErrorSpace::Find(name), py::return_value_policy::reference);
  // CHECK-MESSAGES: :[[@LINE-3]]:25: note: lock 'rlock' acquired here
}

py::object test_absl_writer_mutex_lock(const char *name) {
  absl::WriterMutexLock wlock(&get_absl_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:10: warning: do not call 'pybind11::cast' while holding lock 'wlock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  return py::cast(ErrorSpace::Find(name), py::return_value_policy::reference);
  // CHECK-MESSAGES: :[[@LINE-3]]:25: note: lock 'wlock' acquired here
}

void test_member_cast(py::handle h) {
  std::lock_guard<std::mutex> lock(get_std_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:5: warning: do not call 'pybind11::handle::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  h.cast<int>();
  // CHECK-MESSAGES: :[[@LINE-3]]:31: note: lock 'lock' acquired here
}

void test_reference_to_lock_guard(std::lock_guard<std::mutex> &lock, int x) {
  std::lock_guard<std::mutex> &ref = lock;
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: do not call 'pybind11::cast' while holding lock 'ref'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  py::cast(x);
  // CHECK-MESSAGES: :[[@LINE-3]]:32: note: lock 'ref' acquired here
}

void test_custom_scoped_lockable(int x) {
  CustomScopedLock custom_lock;
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: do not call 'pybind11::cast' while holding lock 'custom_lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  py::cast(x);
  // CHECK-MESSAGES: :[[@LINE-3]]:20: note: lock 'custom_lock' acquired here
}

void test_if_init_cond(int x) {
  if (std::lock_guard<std::mutex> lock(get_std_mutex()); py::cast(x)) {
    // CHECK-MESSAGES: :[[@LINE-1]]:58: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
    // CHECK-MESSAGES: :[[@LINE-2]]:35: note: lock 'lock' acquired here
  }
}

void test_if_init_then(int x) {
  if (std::lock_guard<std::mutex> lock(get_std_mutex()); x > 0) {
    // CHECK-MESSAGES: :[[@LINE+1]]:5: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
    py::cast(x);
    // CHECK-MESSAGES: :[[@LINE-3]]:35: note: lock 'lock' acquired here
  }
}

void test_if_init_else(int x) {
  if (std::lock_guard<std::mutex> lock(get_std_mutex()); x > 0) {
  } else {
    // CHECK-MESSAGES: :[[@LINE+1]]:5: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
    py::cast(x);
    // CHECK-MESSAGES: :[[@LINE-4]]:35: note: lock 'lock' acquired here
  }
}

void test_switch_init(int x) {
  switch (std::lock_guard<std::mutex> lock(get_std_mutex()); x) {
  case 1:
    // CHECK-MESSAGES: :[[@LINE+1]]:5: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
    py::cast(x);
    // CHECK-MESSAGES: :[[@LINE-4]]:39: note: lock 'lock' acquired here
    break;
  default:
    break;
  }
}

void test_for_init(int x) {
  for (std::lock_guard<std::mutex> lock(get_std_mutex()); x < 10; ++x) {
    // CHECK-MESSAGES: :[[@LINE+1]]:5: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
    py::cast(x);
    // CHECK-MESSAGES: :[[@LINE-3]]:36: note: lock 'lock' acquired here
  }
}

void test_lambda_capture_init(int x) {
  std::lock_guard<std::mutex> lock(get_std_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:19: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  auto l = [val = py::cast(x)]() {};
  // CHECK-MESSAGES: :[[@LINE-3]]:31: note: lock 'lock' acquired here
}

void test_lambda_body(int x) {
  std::lock_guard<std::mutex> lock(get_std_mutex());
  auto l = [x]() {
    py::cast(x);
  };
}

void test_nested_pybind11_namespace(int x) {
  std::lock_guard<std::mutex> lock(get_std_mutex());
  mock::pybind11::cast(x);
}

void test_sub_expression(int x) {
  std::lock_guard<std::mutex> lock(get_std_mutex());
  // CHECK-MESSAGES: :[[@LINE+1]]:11: warning: do not call 'pybind11::cast' while holding lock 'lock'; release the lock before converting Python objects to avoid deadlocks with the Python garbage collector [pybind-cast-in-lock]
  Consume(py::cast(x));
  // CHECK-MESSAGES: :[[@LINE-3]]:31: note: lock 'lock' acquired here
}

// Recommended fix pattern: lock released before cast
py::object test_scoped_lock_released_first(const char *name) {
  const ErrorSpace *space = nullptr;
  {
    absl::MutexLock lock(&get_absl_mutex());
    space = ErrorSpace::Find(name);
  }
  return py::cast(space, py::return_value_policy::reference);
}

// Valid cases that should not warn
void test_valid_usages(int x, py::handle h) {
  py::cast(x);
  h.cast<int>();

  std::lock_guard<std::mutex> lock(get_std_mutex());
  other::cast(x);
}
