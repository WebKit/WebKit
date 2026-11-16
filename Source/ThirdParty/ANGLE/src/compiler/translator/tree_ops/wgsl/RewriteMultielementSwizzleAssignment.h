//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteMultielementSwizzleAssignment: WGSL does not support assignment to multielement swizzles.
// This splits them into mutliple assignments to single-element swizzles.
//
// For example:
//   vec3 v1 = ...;
//   vec3 v2 = ...;
//   v1.xy = v2.yz;
// is converted to:
//   vec3 v1 = ...;
//   vec3 v2 = ...;
//   vec2 sbbc = v2.yz;
//   v1.x = v2.x;
//   v1.y = v2.y;
//
// Note that temporaries are used in order to avoid duplicated side effects in the RHS.
//
// One special case is multiplication-by-a-matrix assignment:
//   vec.xy *= mat;
// is converted to something like
//   vec.x = (vec.xy * mat).x;
//   vec.y = (vec.xy * mat).y;
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_WGSL_REWRITE_MULTIELEMENTSWIZZLEASSIGNMENT_H_
#define COMPILER_TRANSLATOR_TREEOPS_WGSL_REWRITE_MULTIELEMENTSWIZZLEASSIGNMENT_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

// Can only be called if CanRewriteMultiElementSwizzleAssignmentEasily() returns true for all
// multi-element swizzles assignments in the tree.
[[nodiscard]] bool RewriteMultielementSwizzleAssignment(TCompiler *compiler, TIntermBlock *root);

[[nodiscard]] bool IsMultielementSwizzleAssignment(TOperator op, TIntermTyped *assignedNode);

[[nodiscard]] bool CanRewriteMultiElementSwizzleAssignmentEasily(
    TIntermBinary *multielementSwizzleAssignment,
    TIntermNode *parent);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_WGSL_REWRITE_MULTIELEMENTSWIZZLEASSIGNMENT_H_
