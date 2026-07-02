// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ExpandFragmentOutputsToVec4.h: Rewrite outputs that are not a vec4 to be vec4s.

#ifndef COMPILER_TRANSLATOR_TREEOPS_GLSL_EXPANDFRAGMENTOUTPUTSTOVEC4_H_
#define COMPILER_TRANSLATOR_TREEOPS_GLSL_EXPANDFRAGMENTOUTPUTSTOVEC4_H_

#include "common/angleutils.h"
#include "common/debug.h"

namespace sh
{

class TCompiler;
class TIntermBlock;
class TSymbolTable;

#ifdef ANGLE_ENABLE_GLSL
[[nodiscard]] bool ExpandFragmentOutputsToVec4(TCompiler *compiler,
                                               TIntermBlock *root,
                                               TSymbolTable *symbolTable);
#else
[[nodiscard]] ANGLE_INLINE bool ExpandFragmentOutputsToVec4(TCompiler *compiler,
                                                            TIntermBlock *root,
                                                            TSymbolTable *symbolTable)
{
    UNREACHABLE();
    return false;
}
#endif

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_GLSL_EXPANDFRAGMENTOUTPUTSTOVEC4_H_
