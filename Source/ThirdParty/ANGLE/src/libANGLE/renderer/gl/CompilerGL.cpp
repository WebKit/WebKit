//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CompilerGL.cpp: Implements the class methods for CompilerGL.

#include "libANGLE/renderer/gl/CompilerGL.h"

#include "common/debug.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Data.h"

namespace rx
{

// Global count of active shader compiler handles. Needed to know when to call ShInitialize and ShFinalize.
static size_t activeCompilerHandles = 0;

CompilerGL::CompilerGL(const gl::Data &data)
    : CompilerImpl(),
      mSpec(data.clientVersion > 2 ? SH_GLES3_SPEC : SH_GLES2_SPEC),
      mOutputType(SH_GLSL_OUTPUT),
      mResources(),
      mFragmentCompiler(nullptr),
      mVertexCompiler(nullptr)
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
    mResources.EXT_shader_texture_lod = extensions.shaderTextureLOD;
    mResources.OES_EGL_image_external = 0; // TODO: disabled until the extension is actually supported.
    mResources.FragmentPrecisionHigh = 1; // TODO: use shader precision caps to determine if high precision is supported?
    mResources.EXT_frag_depth = extensions.fragDepth;

    // GLSL ES 3.0 constants
    mResources.MaxVertexOutputVectors = caps.maxVertexOutputComponents / 4;
    mResources.MaxFragmentInputVectors = caps.maxFragmentInputComponents / 4;
    mResources.MinProgramTexelOffset = caps.minProgramTexelOffset;
    mResources.MaxProgramTexelOffset = caps.maxProgramTexelOffset;
}

CompilerGL::~CompilerGL()
{
    release();
}

gl::Error CompilerGL::release()
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

ShHandle CompilerGL::getCompilerHandle(GLenum type)
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

    if ((*compiler) == nullptr)
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
