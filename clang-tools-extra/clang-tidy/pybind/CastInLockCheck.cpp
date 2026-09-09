//===--- CastInLockCheck.cpp - clang-tidy ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CastInLockCheck.h"
#include "../utils/OptionsUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/DiagnosticIDs.h"

using namespace clang::ast_matchers;

namespace clang::tidy::pybind {

namespace {

const char DefaultLockGuards[] = "::std::lock_guard;"
                                 "::std::unique_lock;"
                                 "::std::scoped_lock;"
                                 "::std::shared_lock;"
                                 "::absl::MutexLock;"
                                 "::absl::ReaderMutexLock;"
                                 "::absl::WriterMutexLock";

bool isInPybind11Namespace(const Decl *D) {
  if (!D)
    return false;
  for (const DeclContext *DC = D->getDeclContext(); DC; DC = DC->getParent()) {
    if (const auto *ND = dyn_cast<NamespaceDecl>(DC)) {
      if (ND->getName() == "pybind11" && ND->getParent() &&
          ND->getParent()->getRedeclContext()->isTranslationUnit())
        return true;
    }
  }
  return false;
}

bool isLockGuardType(QualType QT, ArrayRef<std::string> LockGuardsList) {
  if (QT.isNull())
    return false;
  const CXXRecordDecl *RD =
      QT.getNonReferenceType().getCanonicalType()->getAsCXXRecordDecl();
  if (!RD)
    return false;
  if (RD->hasAttr<ScopedLockableAttr>())
    return true;

  std::string QualifiedName = RD->getQualifiedNameAsString();
  StringRef Name = RD->getName();

  for (const std::string &Item : LockGuardsList) {
    StringRef Trimmed = Item;
    bool MatchQualified = Trimmed.consume_front("::");
    if (MatchQualified) {
      if (QualifiedName == Trimmed)
        return true;
    } else {
      if (Name == Trimmed || QualifiedName == Trimmed)
        return true;
    }
  }
  return false;
}

const Stmt *getInitStmt(const Stmt *S) {
  if (!S)
    return nullptr;
  if (const auto *IS = dyn_cast<IfStmt>(S))
    return IS->getInit();
  if (const auto *SS = dyn_cast<SwitchStmt>(S))
    return SS->getInit();
  if (const auto *FS = dyn_cast<ForStmt>(S))
    return FS->getInit();
  if (const auto *FRS = dyn_cast<CXXForRangeStmt>(S))
    return FRS->getInit();
  return nullptr;
}

const VarDecl *findLockInDeclStmt(const Stmt *S,
                                  ArrayRef<std::string> LockGuardsList) {
  if (const auto *DS = dyn_cast_or_null<DeclStmt>(S)) {
    for (const Decl *D : DS->decls()) {
      if (const auto *VD = dyn_cast<VarDecl>(D)) {
        if (isLockGuardType(VD->getType(), LockGuardsList))
          return VD;
      }
    }
  }
  return nullptr;
}

const VarDecl *findActiveLock(const Stmt *TargetStmt, ASTContext &Context,
                              ArrayRef<std::string> LockGuardsList) {
  DynTypedNode CurrentNode = DynTypedNode::create(*TargetStmt);
  while (true) {
    const auto Parents = Context.getParents(CurrentNode);
    if (Parents.empty())
      break;

    const DynTypedNode &ParentNode = Parents[0];
    if (ParentNode.get<FunctionDecl>())
      break;

    if (const auto *CS = ParentNode.get<CompoundStmt>()) {
      const Stmt *ChildStmt = CurrentNode.get<Stmt>();
      for (const Stmt *Sibling : CS->children()) {
        if (Sibling == ChildStmt)
          break;
        if (const VarDecl *VD = findLockInDeclStmt(Sibling, LockGuardsList))
          return VD;
      }
    } else if (const auto *PS = ParentNode.get<Stmt>()) {
      if (const Stmt *Init = getInitStmt(PS)) {
        if (CurrentNode.get<Stmt>() != Init) {
          if (const VarDecl *VD = findLockInDeclStmt(Init, LockGuardsList))
            return VD;
        }
      }
    }

    CurrentNode = ParentNode;
  }
  return nullptr;
}

} // namespace

CastInLockCheck::CastInLockCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      RawLockGuards(Options.get("LockGuards", DefaultLockGuards)) {
  for (StringRef S : utils::options::parseStringList(RawLockGuards))
    LockGuardsList.push_back(S.str());
}

void CastInLockCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "LockGuards", RawLockGuards);
}

void CastInLockCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(callExpr(unless(cxxMemberCallExpr()),
                              callee(functionDecl(hasName("cast"))))
                         .bind("free_cast"),
                     this);
  Finder->addMatcher(cxxMemberCallExpr(callee(cxxMethodDecl(hasName("cast"))))
                         .bind("member_cast"),
                     this);
}

void CastInLockCheck::check(const MatchFinder::MatchResult &Result) {
  const Expr *CastExpr = nullptr;
  std::string CastCalleeName;

  if (const auto *CE = Result.Nodes.getNodeAs<CallExpr>("free_cast")) {
    const FunctionDecl *FD = CE->getDirectCallee();
    if (!FD || !isInPybind11Namespace(FD))
      return;
    CastExpr = CE;
    CastCalleeName = "pybind11::cast";
  } else if (const auto *MCE =
                 Result.Nodes.getNodeAs<CXXMemberCallExpr>("member_cast")) {
    const CXXMethodDecl *MD = MCE->getMethodDecl();
    if (!MD || !isInPybind11Namespace(MD))
      return;
    CastExpr = MCE;
    CastCalleeName = MD->getQualifiedNameAsString();
  }

  if (!CastExpr)
    return;

  if (const VarDecl *LockDecl =
          findActiveLock(CastExpr, *Result.Context, LockGuardsList)) {
    diag(CastExpr->getExprLoc(),
         "do not call '%0' while holding lock %1; release the lock before "
         "converting Python objects to avoid deadlocks with the Python "
         "garbage collector")
        << CastCalleeName << LockDecl;
    diag(LockDecl->getLocation(), "lock %0 acquired here", DiagnosticIDs::Note)
        << LockDecl;
  }
}

} // namespace clang::tidy::pybind
