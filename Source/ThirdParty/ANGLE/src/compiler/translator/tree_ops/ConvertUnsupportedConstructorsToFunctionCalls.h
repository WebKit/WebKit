//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TREE_OPS_CONVERTUNSUPPORTEDCONSTRUCTORSTOFUNCTIONCALLS_H_
#define COMPILER_TRANSLATOR_TREE_OPS_CONVERTUNSUPPORTEDCONSTRUCTORSTOFUNCTIONCALLS_H_

#include "compiler/translator/Compiler.h"

namespace sh
{

class TCompiler;
class TInterBlock;
class SymbolEnv;

// Adds explicit type casts into the AST where casting is done implicitly.
ANGLE_NO_DISCARD bool ConvertUnsupportedConstructorsToFunctionCalls(TCompiler &compiler,
                                                                    TIntermBlock &root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREE_OPS_CONVERTUNSUPPORTEDCONSTRUCTORSTOFUNCTIONCALLS_H_
