//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderVk.cpp:
//    Implements the class methods for ShaderVk.
//

#include "libANGLE/renderer/vulkan/ShaderVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "platform/FeaturesVk.h"

namespace rx
{

ShaderVk::ShaderVk(const gl::ShaderState &data) : ShaderImpl(data) {}

ShaderVk::~ShaderVk() {}

std::shared_ptr<WaitableCompileEvent> ShaderVk::compile(const gl::Context *context,
                                                        gl::ShCompilerInstance *compilerInstance,
                                                        ShCompileOptions options)
{
    ShCompileOptions compileOptions = SH_INITIALIZE_UNINITIALIZED_LOCALS;

    ContextVk *contextVk = vk::GetImpl(context);

    bool isWebGL = context->getExtensions().webglCompatibility;
    if (isWebGL && mData.getShaderType() != gl::ShaderType::Compute)
    {
        compileOptions |= SH_INIT_OUTPUT_VARIABLES;
    }

    if (contextVk->getFeatures().clampPointSize.enabled)
    {
        compileOptions |= SH_CLAMP_POINT_SIZE;
    }

    if (contextVk->getFeatures().basicGLLineRasterization.enabled)
    {
        compileOptions |= SH_ADD_BRESENHAM_LINE_RASTER_EMULATION;
    }

    if (contextVk->emulateSeamfulCubeMapSampling())
    {
        compileOptions |= SH_EMULATE_SEAMFUL_CUBE_MAP_SAMPLING;
    }

    if (contextVk->useOldRewriteStructSamplers())
    {
        compileOptions |= SH_USE_OLD_REWRITE_STRUCT_SAMPLERS;
    }

    return compileImpl(context, compilerInstance, mData.getSource(), compileOptions | options);
}

std::string ShaderVk::getDebugInfo() const
{
    return mData.getTranslatedSource();
}

}  // namespace rx
