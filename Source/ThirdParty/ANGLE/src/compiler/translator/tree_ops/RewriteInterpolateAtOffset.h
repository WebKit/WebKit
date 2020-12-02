//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This mutating tree traversal flips the 2nd argument of interpolateAtOffset() to account for
// Y-coordinate flipping
//
// From: interpolateAtOffset(float interpolant, vec2 offset);
// To: interpolateAtOffset(float interpolant, vec2(offset * (pre-rotation * viewportYScale)));
//
// See http://anglebug.com/3589

#ifndef COMPILER_TRANSLATOR_TREEOPS_FLIP_INTERPOLATEATOFFSET_H_
#define COMPILER_TRANSLATOR_TREEOPS_FLIP_INTERPOLATEATOFFSET_H_

#include "common/angleutils.h"

namespace sh
{

class TCompiler;
class TIntermNode;
class TIntermBinary;
class TIntermTyped;
class TSymbolTable;

// If fragRotation = nullptr, no rotation will be applied.
ANGLE_NO_DISCARD bool RewriteInterpolateAtOffset(TCompiler *compiler,
                                                 TIntermNode *root,
                                                 const TSymbolTable &symbolTable,
                                                 int shaderVersion,
                                                 TIntermBinary *flipXY,
                                                 TIntermTyped *fragRotation);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_FLIP_INTERPOLATEATOFFSET_H_
