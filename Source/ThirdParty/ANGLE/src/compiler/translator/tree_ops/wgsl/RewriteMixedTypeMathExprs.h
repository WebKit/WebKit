//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteMixedTypeMathExprs: Some mixed-type arithmetic is legal in GLSL but not
// WGSL. Generate code to perform the arithmetic as specified in GLSL.
//
// Example:
// uvec2 x;
// uint y;
// x &= y;
// Is transformed into:
// x &= uvec(y);
//
// Also,
// mat2 x;
// int y;
// x += y;
// Is transformed into:
// x += mat2(float(y), float(y), float(y), float(y))
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_WGSL_REWRITEMIXEDTYPEMATHEXPRS_H_
#define COMPILER_TRANSLATOR_TREEOPS_WGSL_REWRITEMIXEDTYPEMATHEXPRS_H_

#include "compiler/translator/Compiler.h"

namespace sh
{
class TIntermBlock;

[[nodiscard]] bool RewriteMixedTypeMathExprs(TCompiler *compiler, TIntermBlock *root);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_WGSL_REWRITEMIXEDTYPEMATHEXPRS_H_
