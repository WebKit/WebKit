//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
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
    useFBO         = false;
}

DrawCallPerfParams::~DrawCallPerfParams() = default;

std::string DrawCallPerfParams::suffix() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::suffix();

    if (numTris == 0)
    {
        strstr << "_validation_only";
    }

    if (useFBO)
    {
        strstr << "_render_to_texture";
    }

    if (eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
    {
        strstr << "_null";
    }

    return strstr.str();
}

using namespace angle::egl_platform;

DrawCallPerfParams DrawCallPerfD3D11Params(bool useNullDevice, bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = useNullDevice ? D3D11_NULL() : D3D11();
    params.useFBO        = renderToTexture;
    return params;
}

DrawCallPerfParams DrawCallPerfD3D9Params(bool useNullDevice, bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = useNullDevice ? D3D9_NULL() : D3D9();
    params.useFBO        = renderToTexture;
    return params;
}

DrawCallPerfParams DrawCallPerfOpenGLOrGLESParams(bool useNullDevice, bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = OPENGL_OR_GLES(useNullDevice);
    params.useFBO        = renderToTexture;
    return params;
}

DrawCallPerfParams DrawCallPerfValidationOnly()
{
    DrawCallPerfParams params;
    params.eglParameters  = DEFAULT();
    params.numTris        = 0;
    params.runTimeSeconds = 5.0;
    return params;
}

DrawCallPerfParams DrawCallPerfVulkanParams(bool useNullDevice, bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = useNullDevice ? VULKAN_NULL() : VULKAN();
    params.useFBO        = renderToTexture;
    return params;
}

DrawCallPerfParams DrawCallPerfWGLParams(bool renderToTexture)
{
    DrawCallPerfParams params;
    params.useFBO = renderToTexture;
    params.driver = angle::GLESDriverType::SystemWGL;
    return params;
}
