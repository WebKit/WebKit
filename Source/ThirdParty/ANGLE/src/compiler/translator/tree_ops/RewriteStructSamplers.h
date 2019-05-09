//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteStructSamplers: Extract structs from samplers.
//
// This traverser is designed to strip out samplers from structs. It moves them into
// separate uniform sampler declarations. This allows the struct to be stored in the
// default uniform block. It also requires that we rewrite any functions that take the
// struct as an argument. The struct is split into two arguments.
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

namespace sh
{
class TIntermBlock;
class TSymbolTable;
int RewriteStructSamplers(TIntermBlock *root, TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_REWRITESTRUCTSAMPLERS_H_
