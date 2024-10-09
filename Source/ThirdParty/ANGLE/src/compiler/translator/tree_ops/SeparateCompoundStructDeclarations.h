//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_SEPARATECOMPOUNDSTRUCTDECLARATIONS_H_
#define COMPILER_TRANSLATOR_TREEOPS_SEPARATECOMPOUNDSTRUCTDECLARATIONS_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

#include <functional>

namespace sh
{
class TSymbolTable;

using StructNameGenerator = std::function<ImmutableString()>;

// Example:
//  struct Foo { int x; } foo;
// Becomes:
//  struct Foo {int x; }; Foo foo;
[[nodiscard]] bool SeparateCompoundStructDeclarations(TCompiler &compiler,
                                                      StructNameGenerator nameGenerator,
                                                      TIntermBlock &root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_SEPARATECOMPOUNDSTRUCTDECLARATIONS_H_
