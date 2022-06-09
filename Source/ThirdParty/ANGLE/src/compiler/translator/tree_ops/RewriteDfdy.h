//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This mutating tree traversal flips the output of dFdy() to account for framebuffer flipping.
//
// From: dFdy(p)
// To:   (dFdy(p) * viewportYScale)
//
// See http://anglebug.com/3487

#ifndef COMPILER_TRANSLATOR_TREEOPS_REWRITEDFDY_H_
#define COMPILER_TRANSLATOR_TREEOPS_REWRITEDFDY_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

namespace sh
{

class TCompiler;
class TIntermNode;
class TIntermSymbol;
class TIntermBinary;
class TIntermTyped;
class TSymbolTable;
class TVariable;
class SpecConst;
class DriverUniform;

// If fragRotation = nullptr, no rotation will be applied.
ANGLE_NO_DISCARD bool RewriteDfdy(TCompiler *compiler,
                                  ShCompileOptions compileOptions,
                                  TIntermNode *root,
                                  const TSymbolTable &symbolTable,
                                  int shaderVersion,
                                  SpecConst *specConst,
                                  const DriverUniform *driverUniforms);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_REWRITEDFDY_H_
