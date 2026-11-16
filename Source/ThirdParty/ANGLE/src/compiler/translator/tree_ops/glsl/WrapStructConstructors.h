//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WrapStructConstructors: Replace struct constructor invocations with a call to a helper function,
// so that complex expressions aren't directly used in the struct constructor.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_GLSL_WRAPSTRUCTCONSTRUCTORS_H_
#define COMPILER_TRANSLATOR_TREEOPS_GLSL_WRAPSTRUCTCONSTRUCTORS_H_

#include "common/angleutils.h"

namespace sh
{

class TCompiler;
class TIntermBlock;
class TSymbolTable;

#ifdef ANGLE_ENABLE_GLSL
[[nodiscard]] bool WrapStructConstructors(TCompiler *compiler,
                                          TIntermBlock *root,
                                          TSymbolTable *symbolTable);
#else
[[nodiscard]] bool WrapStructConstructors(TCompiler *compiler,
                                          TIntermBlock *root,
                                          TSymbolTable *symbolTable)
{
    UNREACHABLE();
    return false;
}
#endif

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_GLSL_WRAPSTRUCTCONSTRUCTORS_H_
