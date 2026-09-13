//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteStructSamplers: Extract samplers from structs.
//
// This traverser is designed to strip out samplers from structs. It moves them into separate
// uniform sampler declarations. This allows the struct to be stored in the default uniform block.
// This transformation requires MonomorphizeUnsupportedFunctions to have been run so it
// wouldn't need to deal with functions that are passed such structs.
//
// For example:
//
//   struct S { sampler2D samp; int i; };
//   uniform S uni;
//
// Is rewritten as:
//
//   struct S { int i; };
//   uniform S uni;
//   uniform sampler2D extractedSampler0;
//
// Note that |extractedSamplerN| is given an AngleInternal symbol type so that it cannot collide
// with user names (such as |uni|).  The extracted samplers are declared in DFS order when visiting
// the uniforms; this order must be kept so that they can be mapped back to reflection info
// collected in |ShaderVariable|s.

#ifndef COMPILER_TRANSLATOR_TREEOPS_REWRITESTRUCTSAMPLERS_H_
#define COMPILER_TRANSLATOR_TREEOPS_REWRITESTRUCTSAMPLERS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

bool RewriteStructSamplers(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_REWRITESTRUCTSAMPLERS_H_
