//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteArrayOfArrayOfOpaqueUniforms: Flatten array of array of opaque uniforms.  Requires
// MonomorphizeUnsupportedFunctions and RewriteStructSamplers to have been run.

#ifndef COMPILER_TRANSLATOR_TREEOPS_REWRITEARRAYOFARRAYOFOPAQUEUNIFORMS_H_
#define COMPILER_TRANSLATOR_TREEOPS_REWRITEARRAYOFARRAYOFOPAQUEUNIFORMS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

ANGLE_NO_DISCARD bool RewriteArrayOfArrayOfOpaqueUniforms(TCompiler *compiler,
                                                          TIntermBlock *root,
                                                          TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_REWRITEARRAYOFARRAYOFOPAQUEUNIFORMS_H_
