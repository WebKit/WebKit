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
#include "platform/FeaturesVk_autogen.h"

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

    if (context->isWebGL())
    {
        // Only WebGL requires initialization of local variables, others don't.
        // Extra initialization in spirv shader may affect performance.
        compileOptions |= SH_INITIALIZE_UNINITIALIZED_LOCALS;

        // WebGL shaders may contain OOB array accesses which in turn cause undefined behavior,
        // which may result in security issues. See https://crbug.com/1189110.
        compileOptions |= SH_CLAMP_INDIRECT_ARRAY_BOUNDS;

        if (mState.getShaderType() != gl::ShaderType::Compute)
        {
            compileOptions |= SH_INIT_OUTPUT_VARIABLES;
        }
    }

    if (contextVk->getFeatures().clampPointSize.enabled)
    {
        compileOptions |= SH_CLAMP_POINT_SIZE;
    }

    if (contextVk->getFeatures().emulateAdvancedBlendEquations.enabled)
    {
        compileOptions |= SH_ADD_ADVANCED_BLEND_EQUATIONS_EMULATION;
    }

    if (contextVk->emulateSeamfulCubeMapSampling())
    {
        compileOptions |= SH_EMULATE_SEAMFUL_CUBE_MAP_SAMPLING;
    }

    if (!contextVk->getFeatures().enablePrecisionQualifiers.enabled)
    {
        compileOptions |= SH_IGNORE_PRECISION_QUALIFIERS;
    }

    if (contextVk->getFeatures().forceFragmentShaderPrecisionHighpToMediump.enabled)
    {
        compileOptions |= SH_FORCE_SHADER_PRECISION_HIGHP_TO_MEDIUMP;
    }

    // Let compiler use specialized constant for pre-rotation.
    if (!contextVk->getFeatures().preferDriverUniformOverSpecConst.enabled)
    {
        compileOptions |= SH_USE_SPECIALIZATION_CONSTANT;
    }

    if (!contextVk->getFeatures().supportsDepthClipControl.enabled)
    {
        compileOptions |= SH_ADD_VULKAN_DEPTH_CORRECTION;
    }

    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        compileOptions |= SH_ADD_VULKAN_XFB_EXTENSION_SUPPORT_CODE;
    }
    else if (mState.getShaderType() == gl::ShaderType::Vertex &&
             contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        compileOptions |= SH_ADD_VULKAN_XFB_EMULATION_SUPPORT_CODE;
    }

    if (contextVk->getFeatures().generateSPIRVThroughGlslang.enabled)
    {
        compileOptions |= SH_GENERATE_SPIRV_THROUGH_GLSLANG;
    }

    if (contextVk->getFeatures().roundOutputAfterDithering.enabled)
    {
        compileOptions |= SH_ROUND_OUTPUT_AFTER_DITHERING;
    }

    return compileImpl(context, compilerInstance, mState.getSource(), compileOptions | options);
}

std::string ShaderVk::getDebugInfo() const
{
    return mState.getCompiledBinary().empty() ? "" : "<binary blob>";
}

}  // namespace rx
