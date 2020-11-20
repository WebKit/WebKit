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

ShaderVk::ShaderVk(const gl::ShaderState &state) : ShaderImpl(state) {}

ShaderVk::~ShaderVk() {}

std::shared_ptr<WaitableCompileEvent> ShaderVk::compile(const gl::Context *context,
                                                        gl::ShCompilerInstance *compilerInstance,
                                                        ShCompileOptions options)
{
    ShCompileOptions compileOptions = 0;

    ContextVk *contextVk = vk::GetImpl(context);

    bool isWebGL = context->getExtensions().webglCompatibility;

    if (isWebGL)
    {
        // Only webgl requires initialization of local variables, others don't.
        // Extra initialization in spirv shader may affect performance.
        compileOptions |= SH_INITIALIZE_UNINITIALIZED_LOCALS;

        if (mState.getShaderType() != gl::ShaderType::Compute)
        {
            compileOptions |= SH_INIT_OUTPUT_VARIABLES;
        }
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

    if (!contextVk->getFeatures().enablePrecisionQualifiers.enabled)
    {
        compileOptions |= SH_IGNORE_PRECISION_QUALIFIERS;
    }

    // Let compiler detect and emit early fragment test execution mode. We will remove it if
    // context state does not allow it
    compileOptions |= SH_EARLY_FRAGMENT_TESTS_OPTIMIZATION;

    if (contextVk->getFeatures().enablePreRotateSurfaces.enabled ||
        contextVk->getFeatures().emulatedPrerotation90.enabled ||
        contextVk->getFeatures().emulatedPrerotation180.enabled ||
        contextVk->getFeatures().emulatedPrerotation270.enabled)
    {
        // Let compiler insert pre-rotation code.
        compileOptions |= SH_ADD_PRE_ROTATION;
    }

    return compileImpl(context, compilerInstance, mState.getSource(), compileOptions | options);
}

std::string ShaderVk::getDebugInfo() const
{
    return mState.getTranslatedSource();
}

}  // namespace rx
