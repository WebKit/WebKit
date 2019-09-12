//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DrawCallPerfParams.cpp:
//   Parametrization for performance tests for ANGLE draw call overhead.
//

#include "DrawCallPerfParams.h"

#include <sstream>

DrawCallPerfParams::DrawCallPerfParams()
{
    majorVersion = 2;
    minorVersion = 0;
    windowWidth  = 64;
    windowHeight = 64;

// Lower the iteration count in debug.
#if !defined(NDEBUG)
    iterationsPerStep = 100;
#else
    iterationsPerStep = 20000;
#endif
    runTimeSeconds = 10.0;
    numTris        = 1;
    offscreen      = false;
}

DrawCallPerfParams::~DrawCallPerfParams() = default;

std::string DrawCallPerfParams::story() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::story();

    if (numTris == 0)
    {
        strstr << "_validation_only";
    }

    if (offscreen)
    {
        strstr << "_offscreen";
    }

    return strstr.str();
}

using namespace angle::egl_platform;

namespace params
{
DrawCallPerfParams DrawCallD3D11()
{
    DrawCallPerfParams params;
    params.eglParameters = D3D11();
    return params;
}

DrawCallPerfParams DrawCallD3D9()
{
    DrawCallPerfParams params;
    params.eglParameters = D3D9();
    return params;
}

DrawCallPerfParams DrawCallOpenGL()
{
    DrawCallPerfParams params;
    params.eglParameters = OPENGL_OR_GLES();
    return params;
}

DrawCallPerfParams DrawCallValidation()
{
    DrawCallPerfParams params;
    params.eglParameters  = DEFAULT();
    params.numTris        = 0;
    params.runTimeSeconds = 5.0;
    return params;
}

DrawCallPerfParams DrawCallVulkan()
{
    DrawCallPerfParams params;
    params.eglParameters = VULKAN();
    return params;
}

DrawCallPerfParams DrawCallWGL()
{
    DrawCallPerfParams params;
    params.driver = angle::GLESDriverType::SystemWGL;
    return params;
}
}  // namespace params
