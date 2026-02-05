//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Bridge to IR operations and code generation in Rust.
//

#ifndef COMPILER_TRANSLATOR_IR_SRC_COMPILE_H_
#define COMPILER_TRANSLATOR_IR_SRC_COMPILE_H_

#include <GLSLANG/ShaderLang.h>

#ifdef ANGLE_IR
#    include "compiler/translator/ir/src/compile.rs.h"
#endif

namespace sh
{
class TType;
class TVariable;
class TIntermNode;
class TIntermTyped;
class TIntermBlock;
class TCompiler;

namespace ir
{
struct Output
{
    TIntermBlock *root = nullptr;
    std::vector<ShaderVariable> TODOvariables;
};

#ifdef ANGLE_IR
using IR = rust::Box<ffi::IR>;

Output GenerateAST(IR ir, TCompiler *compiler, const ShCompileOptions &options);
#else
using IR = void *;

Output GenerateAST(IR ir, TCompiler *compiler, const ShCompileOptions &options)
{
    return {};
}
#endif
}  // namespace ir
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_IR_SRC_COMPILE_H_
