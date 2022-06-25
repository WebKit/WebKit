//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeclarePerVertexBlocks: If gl_PerVertex is not already declared, it is declared and builts are
// turned into references into that I/O block.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_VULKAN_DECLAREPERVERTEXBLOCKS_H_
#define COMPILER_TRANSLATOR_TREEOPS_VULKAN_DECLAREPERVERTEXBLOCKS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

ANGLE_NO_DISCARD bool DeclarePerVertexBlocks(TCompiler *compiler,
                                             TIntermBlock *root,
                                             TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_VULKAN_DECLAREPERVERTEXBLOCKS_H_
