//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteRowMajorMatrices: Change row-major matrices to column-major in uniform and storage
// buffers.

#ifndef COMPILER_TRANSLATOR_TREEOPS_APPLE_REWRITEROWMAJORMATRICES_H_
#define COMPILER_TRANSLATOR_TREEOPS_APPLE_REWRITEROWMAJORMATRICES_H_

#include "common/angleutils.h"
#include "common/debug.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

#if (defined(ANGLE_ENABLE_GLSL) || defined(ANGLE_ENABLE_METAL)) && \
    defined(ANGLE_ENABLE_APPLE_WORKAROUNDS)
[[nodiscard]] bool RewriteRowMajorMatrices(TCompiler *compiler,
                                           TIntermBlock *root,
                                           TSymbolTable *symbolTable);
#else
[[nodiscard]] ANGLE_INLINE bool RewriteRowMajorMatrices(TCompiler *compiler,
                                                        TIntermBlock *root,
                                                        TSymbolTable *symbolTable)
{
    UNREACHABLE();
    return false;
}
#endif

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_APPLE_REWRITEROWMAJORMATRICES_H_
