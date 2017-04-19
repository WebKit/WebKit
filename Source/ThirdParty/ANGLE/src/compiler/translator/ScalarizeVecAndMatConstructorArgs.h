//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Scalarize vector and matrix constructor args, so that vectors built from components don't have
// matrix arguments, and matrices built from components don't have vector arguments. This avoids
// driver bugs around vector and matrix constructors.
//

#ifndef COMPILER_TRANSLATOR_SCALARIZEVECANDMATCONSTRUCTORARGS_H_
#define COMPILER_TRANSLATOR_SCALARIZEVECANDMATCONSTRUCTORARGS_H_

#include "GLSLANG/ShaderLang.h"

namespace sh
{
class TIntermBlock;

void ScalarizeVecAndMatConstructorArgs(TIntermBlock *root,
                                       sh::GLenum shaderType,
                                       bool fragmentPrecisionHigh,
                                       unsigned int *temporaryIndex);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_SCALARIZEVECANDMATCONSTRUCTORARGS_H_
