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

std::ostream &operator<<(std::ostream &os, const DrawCallPerfParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

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
    params.iterations     = 10000;
    params.numTris        = 0;
    params.runTimeSeconds = 5.0;
    return params;
}

DrawCallPerfParams DrawCallPerfVulkanParams(bool renderToTexture)
{
    DrawCallPerfParams params;
    params.eglParameters = VULKAN();
    params.useFBO        = renderToTexture;
    return params;
}
