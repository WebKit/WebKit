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
#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "platform/FeaturesVk_autogen.h"

namespace rx
{

ShaderVk::ShaderVk(const gl::ShaderState &state) : ShaderImpl(state) {}

ShaderVk::~ShaderVk() {}

std::shared_ptr<WaitableCompileEvent> ShaderVk::compile(const gl::Context *context,
                                                        gl::ShCompilerInstance *compilerInstance,
                                                        ShCompileOptions *options)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (context->isWebGL())
    {
        // Only WebGL requires initialization of local variables, others don't.
        // Extra initialization in spirv shader may affect performance.
        options->initializeUninitializedLocals = true;

        // WebGL shaders may contain OOB array accesses which in turn cause undefined behavior,
        // which may result in security issues. See https://crbug.com/1189110.
        options->clampIndirectArrayBounds = true;

        if (mState.getShaderType() != gl::ShaderType::Compute)
        {
            options->initOutputVariables = true;
        }
    }

    // robustBufferAccess on Vulkan doesn't support bound check on shader local variables
    // but the GL_EXT_robustness does support.
    // Enable the flag clampIndirectArrayBounds to ensure out of bounds local variable writes in
    // shaders are protected when the context has GL_EXT_robustness enabled
    if (contextVk->getShareGroup()->hasAnyContextWithRobustness())
    {
        options->clampIndirectArrayBounds = true;
    }

    if (contextVk->getFeatures().clampPointSize.enabled)
    {
        options->clampPointSize = true;
    }

    if (contextVk->getFeatures().emulateAdvancedBlendEquations.enabled)
    {
        options->addAdvancedBlendEquationsEmulation = true;
    }

    if (contextVk->emulateSeamfulCubeMapSampling())
    {
        options->emulateSeamfulCubeMapSampling = true;
    }

    if (!contextVk->getFeatures().enablePrecisionQualifiers.enabled)
    {
        options->ignorePrecisionQualifiers = true;
    }

    if (contextVk->getFeatures().forceFragmentShaderPrecisionHighpToMediump.enabled)
    {
        options->forceShaderPrecisionHighpToMediump = true;
    }

    // Let compiler use specialized constant for pre-rotation.
    if (!contextVk->getFeatures().preferDriverUniformOverSpecConst.enabled)
    {
        options->useSpecializationConstant = true;
    }

    if (!contextVk->getFeatures().supportsDepthClipControl.enabled)
    {
        options->addVulkanDepthCorrection = true;
    }

    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        options->addVulkanXfbExtensionSupportCode = true;
    }
    else if (mState.getShaderType() == gl::ShaderType::Vertex &&
             contextVk->getFeatures().emulateTransformFeedback.enabled)
    {
        options->addVulkanXfbEmulationSupportCode = true;
    }

    if (contextVk->getFeatures().generateSPIRVThroughGlslang.enabled)
    {
        options->generateSpirvThroughGlslang = true;
    }

    if (contextVk->getFeatures().roundOutputAfterDithering.enabled)
    {
        options->roundOutputAfterDithering = true;
    }

    if (contextVk->getFeatures().precisionSafeDivision.enabled)
    {
        options->precisionSafeDivision = true;
    }

    if (contextVk->getExtensions().shaderPixelLocalStorageANGLE)
    {
        options->pls.type = contextVk->getNativePixelLocalStorageType();
        if (contextVk->getExtensions().shaderPixelLocalStorageCoherentANGLE)
        {
            ASSERT(contextVk->getFeatures().supportsFragmentShaderPixelInterlock.enabled);
            // GL_ARB_fragment_shader_interlock compiles to SPV_EXT_fragment_shader_interlock in
            // both Vulkan Glslang and our own backend.
            options->pls.fragmentSynchronizationType =
                ShFragmentSynchronizationType::FragmentShaderInterlock_ARB_GL;
        }
    }

    return compileImpl(context, compilerInstance, mState.getSource(), options);
}

std::string ShaderVk::getDebugInfo() const
{
    return mState.getCompiledBinary().empty() ? "" : "<binary blob>";
}

}  // namespace rx
