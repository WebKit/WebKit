//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorMetalConstantNames:
// Implementation of constant values used in both translator
// backends.

#include <stdio.h>

#include "GLSLANG/ShaderLang.h"

namespace sh
{

namespace mtl
{
/** extern */
const char kCoverageMaskEnabledConstName[]      = "ANGLECoverageMaskEnabled";
const char kRasterizerDiscardEnabledConstName[] = "ANGLERasterizerDisabled";
const char kDepthWriteEnabledConstName[]        = "ANGLEDepthWriteEnabled";
}  // namespace mtl

}  // namespace sh
