//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteStructSamplers: Extract structs from samplers.
//
// This traverser is designed to strip out samplers from structs. It moves them into separate
// uniform sampler declarations. This allows the struct to be stored in the default uniform block.
// This transformation requires MonomorphizeUnsupportedFunctions to have been run so it
// wouldn't need to deal with functions that are passed such structs.
//
// For example:
//   struct S { sampler2D samp; int i; };
//   uniform S uni;
// Is rewritten as:
//   struct S { int i; };
//   uniform S uni;
//   uniform sampler2D uni_i;

#ifndef COMPILER_TRANSLATOR_TREEOPS_REWRITESTRUCTSAMPLERS_H_
#define COMPILER_TRANSLATOR_TREEOPS_REWRITESTRUCTSAMPLERS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

ANGLE_NO_DISCARD bool RewriteStructSamplers(TCompiler *compiler,
                                            TIntermBlock *root,
                                            TSymbolTable *symbolTable,
                                            int *removedUniformsCountOut);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_VULKAN_REWRITESTRUCTSAMPLERS_H_
