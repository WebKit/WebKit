//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CompilerD3D.cpp: Implementation of the rx::CompilerD3D class.

#include "libANGLE/renderer/d3d/CompilerD3D.h"

#include "libANGLE/Caps.h"
#include "libANGLE/Data.h"

#include "common/debug.h"

namespace rx
{

// Global count of active shader compiler handles. Needed to know when to call ShInitialize and ShFinalize.
static size_t activeCompilerHandles = 0;

CompilerD3D::CompilerD3D(const gl::Data &data, ShShaderOutput outputType)
    : mSpec(data.clientVersion > 2 ? SH_GLES3_SPEC : SH_GLES2_SPEC),
      mOutputType(outputType),
      mResources(),
      mFragmentCompiler(NULL),
      mVertexCompiler(NULL)
{
    ASSERT(data.clientVersion == 2 || data.clientVersion == 3);

    const gl::Caps &caps = *data.caps;
    const gl::Extensions &extensions = *data.extensions;

    ShInitBuiltInResources(&mResources);
    mResources.MaxVertexAttribs = caps.maxVertexAttributes;
    mResources.MaxVertexUniformVectors = caps.maxVertexUniformVectors;
    mResources.MaxVaryingVectors = caps.maxVaryingVectors;
    mResources.MaxVertexTextureImageUnits = caps.maxVertexTextureImageUnits;
    mResources.MaxCombinedTextureImageUnits = caps.maxCombinedTextureImageUnits;
    mResources.MaxTextureImageUnits = caps.maxTextureImageUnits;
    mResources.MaxFragmentUniformVectors = caps.maxFragmentUniformVectors;
    mResources.MaxDrawBuffers = caps.maxDrawBuffers;
    mResources.OES_standard_derivatives = extensions.standardDerivatives;
    mResources.EXT_draw_buffers = extensions.drawBuffers;
    mResources.EXT_shader_texture_lod = 1;
    // resources.OES_EGL_image_external = mRenderer->getShareHandleSupport() ? 1 : 0; // TODO: commented out until the extension is actually supported.
    mResources.FragmentPrecisionHigh = 1;   // Shader Model 2+ always supports FP24 (s16e7) which corresponds to highp
    mResources.EXT_frag_depth = 1; // Shader Model 2+ always supports explicit depth output

    // GLSL ES 3.0 constants
    mResources.MaxVertexOutputVectors = caps.maxVertexOutputComponents / 4;
    mResources.MaxFragmentInputVectors = caps.maxFragmentInputComponents / 4;
    mResources.MinProgramTexelOffset = caps.minProgramTexelOffset;
    mResources.MaxProgramTexelOffset = caps.maxProgramTexelOffset;
}

CompilerD3D::~CompilerD3D()
{
    release();
}

gl::Error CompilerD3D::release()
{
    if (mFragmentCompiler)
    {
        ShDestruct(mFragmentCompiler);
        mFragmentCompiler = NULL;

        ASSERT(activeCompilerHandles > 0);
        activeCompilerHandles--;
    }

    if (mVertexCompiler)
    {
        ShDestruct(mVertexCompiler);
        mVertexCompiler = NULL;

        ASSERT(activeCompilerHandles > 0);
        activeCompilerHandles--;
    }

    if (activeCompilerHandles == 0)
    {
        ShFinalize();
    }

    return gl::Error(GL_NO_ERROR);
}

ShHandle CompilerD3D::getCompilerHandle(GLenum type)
{
    ShHandle *compiler = NULL;
    switch (type)
    {
      case GL_VERTEX_SHADER:
        compiler = &mVertexCompiler;
        break;

      case GL_FRAGMENT_SHADER:
        compiler = &mFragmentCompiler;
        break;

      default:
        UNREACHABLE();
        return NULL;
    }

    if (!(*compiler))
    {
        if (activeCompilerHandles == 0)
        {
            ShInitialize();
        }

        *compiler = ShConstructCompiler(type, mSpec, mOutputType, &mResources);
        activeCompilerHandles++;
    }

    return *compiler;
}

}
