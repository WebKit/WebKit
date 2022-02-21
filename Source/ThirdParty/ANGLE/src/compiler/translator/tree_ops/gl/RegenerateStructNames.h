//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_GL_REGENERATESTRUCTNAMES_H_
#define COMPILER_TRANSLATOR_TREEOPS_GL_REGENERATESTRUCTNAMES_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

#if defined(ANGLE_ENABLE_GLSL)
ANGLE_NO_DISCARD bool RegenerateStructNames(TCompiler *compiler,
                                            TIntermBlock *root,
                                            TSymbolTable *symbolTable);
#else
ANGLE_NO_DISCARD ANGLE_INLINE bool RegenerateStructNames(TCompiler *compiler,
                                                         TIntermBlock *root,
                                                         TSymbolTable *symbolTable)
{
    UNREACHABLE();
    return false;
}
#endif

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_GL_REGENERATESTRUCTNAMES_H_
