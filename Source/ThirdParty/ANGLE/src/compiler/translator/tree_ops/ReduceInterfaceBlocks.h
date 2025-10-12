//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_MSL_REDUCEINTERFACEBLOCKS_H_
#define COMPILER_TRANSLATOR_TREEOPS_MSL_REDUCEINTERFACEBLOCKS_H_

#include <functional>

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

namespace sh
{
class TSymbolTable;

using InterfaceBlockInstanceVarNameGen = std::function<ImmutableString()>;

// This rewrites interface block declarations only.
//
// Example:
//  uniform Foo { int x; } foo;
// Becomes:
//  struct Foo { int x; }; uniform Foo x;
//

[[nodiscard]] bool ReduceInterfaceBlocks(TCompiler &compiler,
                                         TIntermBlock &root,
                                         InterfaceBlockInstanceVarNameGen nameGen);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_MSL_REDUCEINTERFACEBLOCKS_H_
