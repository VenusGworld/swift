//===--- Stmt.h - Swift Language Statement ASTs -----------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the Stmt class and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_AST_STMT_H
#define SWIFT_AST_STMT_H

#include "swift/AST/LLVM.h"
#include "swift/AST/WalkOrder.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerUnion.h"

namespace swift {
  class ASTContext;
  class Decl;
  class Expr;

enum class StmtKind {
#define STMT(ID, PARENT) ID,
#include "swift/AST/StmtNodes.def"
};

/// Stmt - Base class for all statements in swift.
class Stmt {
  Stmt(const Stmt&) = delete;
  void operator=(const Stmt&) = delete;

  /// Kind - The subclass of Stmt that this is.
  const StmtKind Kind;
  
public:
  Stmt(StmtKind kind) : Kind(kind) {}

  StmtKind getKind() const { return Kind; }

  /// getLocStart - Return the location of the start of the expression.
  /// FIXME: QOI: Need to extend this to do full source ranges like Clang.
  SMLoc getStartLoc() const;
  
  
  /// walk - This recursively walks all of the statements and expressions
  /// contained within a statement and invokes the ExprFn and StmtFn blocks on
  /// each.
  ///
  /// The block pointers are invoked both before and after the children are
  /// visted, with the WalkOrder specifing at each invocation which stage it is.
  /// If the block pointer returns a non-NULL value, then the returned
  /// expression or statement is spliced back into the AST or returned from
  /// 'walk' if at the top-level.
  ///
  /// If block pointer returns NULL from a pre-order invocation, then the
  /// subtree is not visited.  If the block pointer returns NULL from a
  /// post-order invocation, then the walk is terminated and 'walk returns
  /// NULL.
  ///
  Stmt *walk(Expr *(^ExprFn)(Expr *E, WalkOrder Order),
             Stmt *(^StmtFn)(Stmt *E, WalkOrder Order) = 0);
  
  
  void dump() const;
  void print(raw_ostream &OS, unsigned Indent = 0) const;
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const Stmt *) { return true; }

  enum { Alignment = 8 };

  // Only allow allocation of Exprs using the allocator in ASTContext
  // or by doing a placement new.
  void *operator new(size_t Bytes, ASTContext &C,
                     unsigned Alignment = Stmt::Alignment) throw();
  
  // Make placement new and vanilla new/delete illegal for Exprs.
  void *operator new(size_t Bytes) throw() = delete;
  void operator delete(void *Data) throw() = delete;
  void *operator new(size_t Bytes, void *Mem) throw() = delete;

};

/// SemiStmt - A semicolon, the noop statement: ";"
class SemiStmt : public Stmt {
  SMLoc Loc;
  
public:
  SemiStmt(SMLoc Loc) : Stmt(StmtKind::Semi), Loc(Loc) {}

  SMLoc getLoc() const { return Loc; }
  SMLoc getStartLoc() const { return Loc; }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const SemiStmt *) { return true; }
  static bool classof(const Stmt *S) { return S->getKind() == StmtKind::Semi; }
};

/// AssignStmt - A value assignment, like "x = y".
class AssignStmt : public Stmt {
  Expr *Dest;
  Expr *Src;
  SMLoc EqualLoc;

public:  
  AssignStmt(Expr *Dest, SMLoc EqualLoc, Expr *Src)
    : Stmt(StmtKind::Assign), Dest(Dest), Src(Src), EqualLoc(EqualLoc) {}

  Expr *getDest() const { return Dest; }
  void setDest(Expr *e) { Dest = e; }
  Expr *getSrc() const { return Src; }
  void setSrc(Expr *e) { Src = e; }
  
  SMLoc getEqualLoc() const { return EqualLoc; }
  SMLoc getStartLoc() const;
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const AssignStmt *) { return true; }
  static bool classof(const Stmt *S) { return S->getKind() == StmtKind::Assign; }
};


/// BraceStmt - A brace enclosed sequence of expressions, stmts, or decls, like
/// { 4; 5 }.
class BraceStmt : public Stmt {
public:
  typedef llvm::PointerUnion3<Expr*, Stmt*, Decl*> ExprStmtOrDecl;

private:
  unsigned NumElements;
  
  SMLoc LBLoc;
  SMLoc RBLoc;

  BraceStmt(SMLoc lbloc, ArrayRef<ExprStmtOrDecl> elements, SMLoc rbloc);
  ExprStmtOrDecl *getElementsStorage() {
    return reinterpret_cast<ExprStmtOrDecl*>(this + 1);
  }
  const ExprStmtOrDecl *getElementsStorage() const {
    return const_cast<BraceStmt*>(this)->getElementsStorage();
  }

public:
  static BraceStmt *create(ASTContext &ctx, SMLoc lbloc,
                           ArrayRef<ExprStmtOrDecl> elements,
                           SMLoc rbloc);

  SMLoc getLBraceLoc() const { return LBLoc; }
  SMLoc getRBraceLoc() const { return RBLoc; }
  SMLoc getStartLoc() const { return LBLoc; }

  unsigned getNumElements() const { return NumElements; }
  ArrayRef<ExprStmtOrDecl> getElements() const {
    return ArrayRef<ExprStmtOrDecl>(getElementsStorage(), NumElements);
  }
  ExprStmtOrDecl getElement(unsigned i) const {
    assert(i < NumElements && "index out of range!");
    return getElementsStorage()[i];
  }
  void setElement(unsigned i, ExprStmtOrDecl elt) {
    assert(i < NumElements && "index out of range!");
    getElementsStorage()[i] = elt;
  }
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const BraceStmt *) { return true; }
  static bool classof(const Stmt *S) { return S->getKind() == StmtKind::Brace; }
};

/// ReturnStmt - A return statement.  Return statements with no specified
/// subexpression are expanded into a return of the empty tuple in the parser.
///    return 42
class ReturnStmt : public Stmt {
  SMLoc ReturnLoc;
  Expr *Result;
  
public:
  ReturnStmt(SMLoc ReturnLoc, Expr *Result)
    : Stmt(StmtKind::Return), ReturnLoc(ReturnLoc), Result(Result) {}

  SMLoc getStartLoc() const { return ReturnLoc; }
  SMLoc getReturnLoc() const { return ReturnLoc; }

  Expr *getResult() const { return Result; }
  void setResult(Expr *e) { Result = e; }
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const ReturnStmt *) { return true; }
  static bool classof(const Stmt *S) { return S->getKind() == StmtKind::Return; }
};

/// IfStmt - if/then/else statement.  If no 'else' is specified, then the
/// ElseLoc location is not specified and the Else statement is null.  The
/// condition of the 'if' is required to have a __builtin_int1 type.
class IfStmt : public Stmt {
  SMLoc IfLoc;
  SMLoc ElseLoc;
  Expr *Cond;
  Stmt *Then;
  Stmt *Else;
  
public:
  IfStmt(SMLoc IfLoc, Expr *Cond, Stmt *Then, SMLoc ElseLoc,
         Stmt *Else)
  : Stmt(StmtKind::If),
    IfLoc(IfLoc), ElseLoc(ElseLoc), Cond(Cond), Then(Then), Else(Else) {}

  SMLoc getStartLoc() const { return IfLoc; }
  SMLoc getIfLoc() const { return IfLoc; }
  SMLoc getElseLoc() const { return ElseLoc; }

  Expr *getCond() const { return Cond; }
  void setCond(Expr *e) { Cond = e; }

  Stmt *getThenStmt() const { return Then; }
  void setThenStmt(Stmt *s) { Then = s; }

  Stmt *getElseStmt() const { return Else; }
  void setElseStmt(Stmt *s) { Else = s; }
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const IfStmt *) { return true; }
  static bool classof(const Stmt *S) { return S->getKind() == StmtKind::If; }
};

/// WhileStmt - while statement.  The condition is required to have a
/// __builtin_int1 type.
class WhileStmt : public Stmt {
  SMLoc WhileLoc;
  Expr *Cond;
  Stmt *Body;
  
public:
  WhileStmt(SMLoc WhileLoc, Expr *Cond, Stmt *Body)
  : Stmt(StmtKind::While),
    WhileLoc(WhileLoc), Cond(Cond), Body(Body) {}

  SMLoc getStartLoc() const { return WhileLoc; }

  Expr *getCond() const { return Cond; }
  void setCond(Expr *e) { Cond = e; }

  Stmt *getBody() const { return Body; }
  void setBody(Stmt *s) { Body = s; }
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const WhileStmt *) { return true; }
  static bool classof(const Stmt *S) { return S->getKind() == StmtKind::While; }
};

} // end namespace swift

#endif
