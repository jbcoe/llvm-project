//===--- CastInLockCheck.h - clang-tidy -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_PYBIND_CASTINLOCKCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_PYBIND_CASTINLOCKCHECK_H

#include "../ClangTidyCheck.h"
#include <string>
#include <vector>

namespace clang::tidy::pybind {

/// Finds calls to `pybind11::cast` made while holding a lock.
///
/// In multi-threaded and free-threaded Python environments (PEP 703),
/// `pybind11::cast` interacts with pybind11's internal type registry and
/// Python's object allocator. If Python's cyclic garbage collector triggers a
/// Stop-The-World (STW) pause, a thread executing `pybind11::cast` while
/// holding a mutex can be parked at a safepoint. Any other threads attempting
/// to acquire the same mutex cannot reach a safepoint, leading to a circular
/// deadlock between the garbage collector and the application mutex.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/pybind/cast-in-lock.html
class CastInLockCheck : public ClangTidyCheck {
public:
  CastInLockCheck(StringRef Name, ClangTidyContext *Context);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.CPlusPlus;
  }

private:
  const std::string RawLockGuards;
  std::vector<std::string> LockGuardsList;
};

} // namespace clang::tidy::pybind

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_PYBIND_CASTINLOCKCHECK_H
