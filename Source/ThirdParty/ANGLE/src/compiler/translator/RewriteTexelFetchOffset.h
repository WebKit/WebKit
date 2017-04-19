//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This mutating tree traversal works around an issue on the translation
// from texelFetchOffset into HLSL function Load on INTEL drivers. It
// works by translating texelFetchOffset into texelFetch:
//
// - From: texelFetchOffset(sampler, Position, lod, offset)
// - To: texelFetch(sampler, Position+offset, lod)
//
// See http://anglebug.com/1469

#ifndef COMPILER_TRANSLATOR_REWRITE_TEXELFETCHOFFSET_H_
#define COMPILER_TRANSLATOR_REWRITE_TEXELFETCHOFFSET_H_

class TIntermNode;
class TSymbolTable;

namespace sh
{

void RewriteTexelFetchOffset(TIntermNode *root, const TSymbolTable &symbolTable, int shaderVersion);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_REWRITE_TEXELFETCHOFFSET_H_