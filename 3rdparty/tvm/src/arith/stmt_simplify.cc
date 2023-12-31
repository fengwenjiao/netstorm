/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file stmt_simplify.cc
 * \brief Statement simplifier based on analyzer
 */
#include <tvm/tir/expr.h>
#include <tvm/tir/ir_pass.h>
#include <tvm/arith/analyzer.h>
#include <tvm/tir/op.h>
#include <tvm/arith/analyzer.h>
#include "ir_mutator_with_analyzer.h"

namespace tvm {
namespace arith {

using namespace tir;

class StmtSimplifier : public IRMutatorWithAnalyzer {
 public:
  explicit StmtSimplifier(Analyzer* analyzer)
      : IRMutatorWithAnalyzer(analyzer) {}

  using Parent = IRMutatorWithAnalyzer;
  using Parent::VisitStmt;
  using Parent::VisitStmt_;

  PrimExpr VisitExpr(const PrimExpr& expr) final {
    return analyzer_->Simplify(expr);
  }

  Stmt Simplify(Stmt stmt) {
    return operator()(std::move(stmt));
  }

  Stmt VisitStmt_(const ForNode* op) final {
    analyzer_->Bind(op->loop_var, Range::make_by_min_extent(op->min, op->extent));
    With<ConstraintContext> ctx1(analyzer_, op->loop_var >= op->min);
    With<ConstraintContext> ctx2(analyzer_, op->loop_var < op->min + op->extent);
    return Parent::VisitStmt_(op);
  }

  Stmt VisitStmt_(const LetStmtNode* op) {
    PrimExpr value = this->VisitExpr(op->value);
    if (!tir::HasSideEffect(value)) {
      // it is fine to discard the let binding
      // because the call to simplify will always inline the var.
      analyzer_->Bind(op->var, value);
      return this->VisitStmt(op->body);
    }
    Stmt body = this->VisitStmt(op->body);
    if (value.same_as(op->value) &&
        body.same_as(op->body)) {
      return GetRef<Stmt>(op);
    } else {
      auto n = this->CopyOnWrite(op);
      n->value = std::move(value);
      n->body = std::move(body);
      return Stmt(n);
    }
  }

  // eliminate useless stores
  Stmt VisitStmt_(const StoreNode* op) final {
    Stmt stmt = Parent::VisitStmt_(op);
    op = stmt.as<StoreNode>();
    if (const LoadNode* load = op->value.as<LoadNode>()) {
      if (load->buffer_var.same_as(op->buffer_var) &&
          Equal(load->index, op->index)) {
        return EvaluateNode::make(0);
      }
    }
    return GetRef<Stmt>(op);
  }
};

}  // namespace arith

namespace tir {

Stmt CanonicalSimplify(Stmt stmt, Map<Var, Range> vrange) {
  arith::Analyzer analyzer;
  for (auto kv : vrange) {
    analyzer.Bind(kv.first, kv.second);
  }
  return arith::StmtSimplifier(&analyzer).Simplify(std::move(stmt));
}

PrimExpr CanonicalSimplify(PrimExpr expr, Map<Var, Range> vrange) {
  arith::Analyzer analyzer;
  for (auto kv : vrange) {
    analyzer.Bind(kv.first, kv.second);
  }
  return analyzer.canonical_simplify(expr);
}

PrimExpr Simplify(PrimExpr expr, Map<Var, Range> vrange) {
  arith::Analyzer analyzer;
  for (auto kv : vrange) {
    analyzer.Bind(kv.first, kv.second);
  }
  expr = analyzer.Simplify(expr);
  return expr;
}

Stmt Simplify(Stmt stmt, Map<Var, Range> vrange) {
  return CanonicalSimplify(std::move(stmt), vrange);
}
}  // namespace tir
}  // namespace tvm
