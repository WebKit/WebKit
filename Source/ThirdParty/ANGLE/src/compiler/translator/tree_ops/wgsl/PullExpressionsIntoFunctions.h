//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PullExpressionsIntoFunctions: Certain GLSL expressions are not translatable as single
// expressions. Those that need to be translated into multiple expressions, or into one or more
// statements, are pulled into functions and replaced by a function call.
//
// This works by pulling all temporaries used inside of ternaries into global variables, which is
// fine because recursion is not allowed in GLSL. Function parameters are much more difficult to
// pull into globals, so they are just all passed to the new function.
//
// This makes all arbitrary ternaries, comma operators, outparams, and multielement swizzle
// assignments translatable into WGSL.

#ifndef COMPILER_TRANSLATOR_TREEOPS_WGSL_PULLEXPRESSIONSINTOFUNCTIONS_H_
#define COMPILER_TRANSLATOR_TREEOPS_WGSL_PULLEXPRESSIONSINTOFUNCTIONS_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

[[nodiscard]] bool PullExpressionsIntoFunctions(TCompiler *compiler, TIntermBlock *root);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_WGSL_PULLEXPRESSIONSINTOFUNCTIONS_H_
