//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EmulateMutableFunctionParams: If function parameters are modified
// in the function body, they are replaced with temporary copies, since
// function parameters are always immuable in WGSL.
//
// Example:
//
// vec4 doFoo(Foo foo, float zw)
// {
//     foo.x = foo.y;
//     return vec4(foo.x, foo.y, zw, zw);
// }
//
// Result:
//
// vec4 doFoo(Foo foo, float zw)
// {
//     Foo sbc7 = foo;
//     sbc7.x = sbc7.y;
//     return vec4(sbc7.x, sbc7.y, zw, zw);
// }
//
// NOTE: this an be deleted if WGSL standardized mutable function parameters.
// https://github.com/gpuweb/gpuweb/issues/4113
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_WGSL_EMULATEMUTABLEFUNCTIONPARAMS_H_
#define COMPILER_TRANSLATOR_TREEOPS_WGSL_EMULATEMUTABLEFUNCTIONPARAMS_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

[[nodiscard]] bool EmulateMutableFunctionParams(TCompiler *compiler, TIntermBlock *root);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_WGSL_EMULATEMUTABLEFUNCTIONPARAMS_H_
