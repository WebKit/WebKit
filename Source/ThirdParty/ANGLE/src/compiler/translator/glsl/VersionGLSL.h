//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_GLSL_VERSIONGLSL_H_
#define COMPILER_TRANSLATOR_GLSL_VERSIONGLSL_H_

#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

static const int GLSL_VERSION_150 = 150;
static const int GLSL_VERSION_330 = 330;
static const int GLSL_VERSION_400 = 400;
static const int GLSL_VERSION_410 = 410;
static const int GLSL_VERSION_420 = 420;
static const int GLSL_VERSION_430 = 430;
static const int GLSL_VERSION_440 = 440;
static const int GLSL_VERSION_450 = 450;

int ShaderOutputTypeToGLSLVersion(ShShaderOutput output);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_GLSL_VERSIONGLSL_H_
