//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReswizzleYUVOps: Adjusts swizzles for YUV channel order difference between
//   GLES and Vulkan
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_SPIRV_RESWIZZLEYUVOPS_H_
#define COMPILER_TRANSLATOR_TREEOPS_SPIRV_RESWIZZLEYUVOPS_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

[[nodiscard]] bool AdjustYUVOutput(TCompiler *compiler,
                                   TIntermBlock *root,
                                   TSymbolTable *symbolTable,
                                   const TIntermSymbol &yuvOutput);

[[nodiscard]] bool ReswizzleYUVTextureAccess(TCompiler *compiler,
                                             TIntermBlock *root,
                                             TSymbolTable *symbolTable);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_SPIRV_RESWIZZLEYUVOPS_H_
