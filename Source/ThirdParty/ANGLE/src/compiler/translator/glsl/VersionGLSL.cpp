//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/glsl/VersionGLSL.h"

namespace sh
{

int ShaderOutputTypeToGLSLVersion(ShShaderOutput output)
{
    switch (output)
    {
        case SH_GLSL_150_CORE_OUTPUT:
            return GLSL_VERSION_150;
        case SH_GLSL_330_CORE_OUTPUT:
            return GLSL_VERSION_330;
        case SH_GLSL_400_CORE_OUTPUT:
            return GLSL_VERSION_400;
        case SH_GLSL_410_CORE_OUTPUT:
            return GLSL_VERSION_410;
        case SH_GLSL_420_CORE_OUTPUT:
            return GLSL_VERSION_420;
        case SH_GLSL_430_CORE_OUTPUT:
            return GLSL_VERSION_430;
        case SH_GLSL_440_CORE_OUTPUT:
            return GLSL_VERSION_440;
        case SH_GLSL_450_CORE_OUTPUT:
            return GLSL_VERSION_450;
        default:
            UNREACHABLE();
            return 0;
    }
}

}  // namespace sh
