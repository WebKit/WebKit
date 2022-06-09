//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef ANGLE_TEST_CONFIGS_H_
#define ANGLE_TEST_CONFIGS_H_

// On Linux EGL/egl.h includes X.h which does defines for some very common
// names that are used by gtest (like None and Bool) and causes a lot of
// compilation errors. To work around this, even if this file doesn't use it,
// we include gtest before EGL so that it compiles fine in other files that
// want to use gtest.
#include <gtest/gtest.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "angle_test_instantiate.h"
#include "util/EGLPlatformParameters.h"

namespace angle
{

struct PlatformParameters
{
    PlatformParameters();
    PlatformParameters(EGLint majorVersion,
                       EGLint minorVersion,
                       const EGLPlatformParameters &eglPlatformParameters);
    PlatformParameters(EGLint majorVersion, EGLint minorVersion, GLESDriverType driver);

    EGLint getRenderer() const;
    EGLint getDeviceType() const;
    bool isSwiftshader() const;
    bool isVulkan() const;
    bool isANGLE() const;
    EGLint getAllocateNonZeroMemoryFeature() const;

    void initDefaultParameters();

    auto tie() const
    {
        return std::tie(driver, noFixture, eglParameters, majorVersion, minorVersion);
    }

    GLESDriverType driver;
    bool noFixture;
    EGLPlatformParameters eglParameters;
    EGLint majorVersion;
    EGLint minorVersion;
};

const char *GetRendererName(EGLint renderer);

bool operator<(const PlatformParameters &a, const PlatformParameters &b);
bool operator==(const PlatformParameters &a, const PlatformParameters &b);
std::ostream &operator<<(std::ostream &stream, const PlatformParameters &pp);

// EGL platforms
namespace egl_platform
{

EGLPlatformParameters DEFAULT();
EGLPlatformParameters DEFAULT_NULL();

EGLPlatformParameters D3D9();
EGLPlatformParameters D3D9_NULL();
EGLPlatformParameters D3D9_REFERENCE();

EGLPlatformParameters D3D11();
EGLPlatformParameters D3D11_PRESENT_PATH_FAST();
EGLPlatformParameters D3D11_FL11_1();
EGLPlatformParameters D3D11_FL11_0();
EGLPlatformParameters D3D11_FL10_1();
EGLPlatformParameters D3D11_FL10_0();

EGLPlatformParameters D3D11_NULL();

EGLPlatformParameters D3D11_WARP();
EGLPlatformParameters D3D11_FL11_1_WARP();
EGLPlatformParameters D3D11_FL11_0_WARP();
EGLPlatformParameters D3D11_FL10_1_WARP();
EGLPlatformParameters D3D11_FL10_0_WARP();

EGLPlatformParameters D3D11_REFERENCE();
EGLPlatformParameters D3D11_FL11_1_REFERENCE();
EGLPlatformParameters D3D11_FL11_0_REFERENCE();
EGLPlatformParameters D3D11_FL10_1_REFERENCE();
EGLPlatformParameters D3D11_FL10_0_REFERENCE();

EGLPlatformParameters OPENGL();
EGLPlatformParameters OPENGL(EGLint major, EGLint minor);
EGLPlatformParameters OPENGL_NULL();

EGLPlatformParameters OPENGLES();
EGLPlatformParameters OPENGLES(EGLint major, EGLint minor);
EGLPlatformParameters OPENGLES_NULL();

EGLPlatformParameters OPENGL_OR_GLES();
EGLPlatformParameters OPENGL_OR_GLES(EGLint major, EGLint minor);
EGLPlatformParameters OPENGL_OR_GLES_NULL();

EGLPlatformParameters VULKAN();
EGLPlatformParameters VULKAN_NULL();
EGLPlatformParameters VULKAN_SWIFTSHADER();

}  // namespace egl_platform

// ANGLE tests platforms
PlatformParameters ES1_D3D9();
PlatformParameters ES2_D3D9();

PlatformParameters ES1_D3D11();
PlatformParameters ES2_D3D11();
PlatformParameters ES2_D3D11_PRESENT_PATH_FAST();
PlatformParameters ES2_D3D11_FL11_0();
PlatformParameters ES2_D3D11_FL10_1();
PlatformParameters ES2_D3D11_FL10_0();

PlatformParameters ES2_D3D11_WARP();
PlatformParameters ES2_D3D11_FL11_0_WARP();
PlatformParameters ES2_D3D11_FL10_1_WARP();
PlatformParameters ES2_D3D11_FL10_0_WARP();

PlatformParameters ES2_D3D11_REFERENCE();
PlatformParameters ES2_D3D11_FL11_0_REFERENCE();
PlatformParameters ES2_D3D11_FL10_1_REFERENCE();
PlatformParameters ES2_D3D11_FL10_0_REFERENCE();

PlatformParameters ES3_D3D11();
PlatformParameters ES3_D3D11_FL11_1();
PlatformParameters ES3_D3D11_FL11_0();
PlatformParameters ES3_D3D11_FL10_1();
PlatformParameters ES31_D3D11();
PlatformParameters ES31_D3D11_FL11_1();
PlatformParameters ES31_D3D11_FL11_0();

PlatformParameters ES3_D3D11_WARP();
PlatformParameters ES3_D3D11_FL11_1_WARP();
PlatformParameters ES3_D3D11_FL11_0_WARP();
PlatformParameters ES3_D3D11_FL10_1_WARP();

PlatformParameters ES1_OPENGL();
PlatformParameters ES2_OPENGL();
PlatformParameters ES2_OPENGL(EGLint major, EGLint minor);
PlatformParameters ES3_OPENGL();
PlatformParameters ES3_OPENGL(EGLint major, EGLint minor);
PlatformParameters ES31_OPENGL();
PlatformParameters ES31_OPENGL(EGLint major, EGLint minor);

PlatformParameters ES1_OPENGLES();
PlatformParameters ES2_OPENGLES();
PlatformParameters ES2_OPENGLES(EGLint major, EGLint minor);
PlatformParameters ES3_OPENGLES();
PlatformParameters ES3_OPENGLES(EGLint major, EGLint minor);
PlatformParameters ES31_OPENGLES();
PlatformParameters ES31_OPENGLES(EGLint major, EGLint minor);

PlatformParameters ES1_NULL();
PlatformParameters ES2_NULL();
PlatformParameters ES3_NULL();
PlatformParameters ES31_NULL();

PlatformParameters ES1_VULKAN();
PlatformParameters ES1_VULKAN_NULL();
PlatformParameters ES1_VULKAN_SWIFTSHADER();
PlatformParameters ES2_VULKAN();
PlatformParameters ES2_VULKAN_NULL();
PlatformParameters ES2_VULKAN_SWIFTSHADER();
PlatformParameters ES3_VULKAN();
PlatformParameters ES3_VULKAN_NULL();
PlatformParameters ES3_VULKAN_SWIFTSHADER();
PlatformParameters ES31_VULKAN();
PlatformParameters ES31_VULKAN_NULL();
PlatformParameters ES31_VULKAN_SWIFTSHADER();
PlatformParameters ES32_VULKAN();
PlatformParameters ES32_VULKAN_NULL();
PlatformParameters ES32_VULKAN_SWIFTSHADER();

PlatformParameters ES1_METAL();
PlatformParameters ES2_METAL();
PlatformParameters ES3_METAL();

PlatformParameters ES2_WGL();
PlatformParameters ES3_WGL();

PlatformParameters ES2_EGL();
PlatformParameters ES3_EGL();

const char *GetNativeEGLLibraryNameWithExtension();

inline PlatformParameters WithNoFixture(const PlatformParameters &params)
{
    PlatformParameters withNoFixture = params;
    withNoFixture.noFixture          = true;
    return withNoFixture;
}

inline PlatformParameters WithNoTransformFeedback(const PlatformParameters &params)
{
    PlatformParameters withNoTransformFeedback                     = params;
    withNoTransformFeedback.eglParameters.transformFeedbackFeature = EGL_FALSE;
    return withNoTransformFeedback;
}

inline PlatformParameters WithAllocateNonZeroMemory(const PlatformParameters &params)
{
    PlatformParameters allocateNonZero                         = params;
    allocateNonZero.eglParameters.allocateNonZeroMemoryFeature = EGL_TRUE;
    return allocateNonZero;
}

inline PlatformParameters WithEmulateCopyTexImage2DFromRenderbuffers(
    const PlatformParameters &params)
{
    PlatformParameters p                                   = params;
    p.eglParameters.emulateCopyTexImage2DFromRenderbuffers = EGL_TRUE;
    return p;
}

inline PlatformParameters WithNoShaderStencilOutput(const PlatformParameters &params)
{
    PlatformParameters re                       = params;
    re.eglParameters.shaderStencilOutputFeature = EGL_FALSE;
    return re;
}

inline PlatformParameters WithNoGenMultipleMipsPerPass(const PlatformParameters &params)
{
    PlatformParameters re                          = params;
    re.eglParameters.genMultipleMipsPerPassFeature = EGL_FALSE;
    return re;
}

inline PlatformParameters WithMetalMemoryBarrierAndCheapRenderPass(const PlatformParameters &params,
                                                                   bool hasBarrier,
                                                                   bool cheapRenderPass)
{
    PlatformParameters re                            = params;
    re.eglParameters.hasExplicitMemBarrierFeatureMtl = hasBarrier ? EGL_TRUE : EGL_FALSE;
    re.eglParameters.hasCheapRenderPassFeatureMtl    = cheapRenderPass ? EGL_TRUE : EGL_FALSE;
    return re;
}

inline PlatformParameters WithMetalForcedBufferGPUStorage(const PlatformParameters &params)
{
    PlatformParameters re                            = params;
    re.eglParameters.forceBufferGPUStorageFeatureMtl = EGL_TRUE;
    return re;
}

inline PlatformParameters WithRobustness(const PlatformParameters &params)
{
    PlatformParameters withRobustness       = params;
    withRobustness.eglParameters.robustness = EGL_TRUE;
    return withRobustness;
}

inline PlatformParameters WithEmulatedPrerotation(const PlatformParameters &params, EGLint rotation)
{
    PlatformParameters prerotation                = params;
    prerotation.eglParameters.emulatedPrerotation = rotation;
    return prerotation;
}

inline PlatformParameters WithAsyncCommandQueueFeatureVulkan(const PlatformParameters &params)
{
    PlatformParameters withAsyncCommandQueue                           = params;
    withAsyncCommandQueue.eglParameters.asyncCommandQueueFeatureVulkan = EGL_TRUE;
    return withAsyncCommandQueue;
}

inline PlatformParameters WithNoVulkanViewportFlip(const PlatformParameters &params)
{
    PlatformParameters withoutVulkanViewportFlip                       = params;
    withoutVulkanViewportFlip.eglParameters.supportsVulkanViewportFlip = EGL_FALSE;
    return withoutVulkanViewportFlip;
}

inline PlatformParameters WithNoVulkanMultiDrawIndirect(const PlatformParameters &params)
{
    PlatformParameters withoutVulkanMultiDrawIndirectSupport                            = params;
    withoutVulkanMultiDrawIndirectSupport.eglParameters.supportsVulkanMultiDrawIndirect = EGL_FALSE;
    return withoutVulkanMultiDrawIndirectSupport;
}

inline PlatformParameters WithEmulatedVAOs(const PlatformParameters &params)
{
    PlatformParameters emualtedVAOParams         = params;
    emualtedVAOParams.eglParameters.emulatedVAOs = EGL_TRUE;
    return emualtedVAOParams;
}

inline PlatformParameters WithGlslang(const PlatformParameters &params)
{
    PlatformParameters generateSPIRVThroughGlslang                        = params;
    generateSPIRVThroughGlslang.eglParameters.generateSPIRVThroughGlslang = EGL_TRUE;
    return generateSPIRVThroughGlslang;
}

inline PlatformParameters WithDirectMetalGeneration(const PlatformParameters &params)
{
    PlatformParameters directMetalGeneration                  = params;
    directMetalGeneration.eglParameters.directMetalGeneration = EGL_TRUE;
    return directMetalGeneration;
}

inline PlatformParameters WithInitShaderVariables(const PlatformParameters &params)
{
    PlatformParameters initShaderVariables                     = params;
    initShaderVariables.eglParameters.forceInitShaderVariables = EGL_TRUE;
    return initShaderVariables;
}

inline PlatformParameters WithForceVulkanFallbackFormat(const PlatformParameters &paramsIn)
{
    PlatformParameters paramsOut                      = paramsIn;
    paramsOut.eglParameters.forceVulkanFallbackFormat = EGL_TRUE;
    return paramsOut;
}

inline PlatformParameters WithLowPowerGPU(const PlatformParameters &paramsIn)
{
    PlatformParameters paramsOut                   = paramsIn;
    paramsOut.eglParameters.displayPowerPreference = EGL_LOW_POWER_ANGLE;
    return paramsOut;
}

inline PlatformParameters WithHighPowerGPU(const PlatformParameters &paramsIn)
{
    PlatformParameters paramsOut                   = paramsIn;
    paramsOut.eglParameters.displayPowerPreference = EGL_HIGH_POWER_ANGLE;
    return paramsOut;
}

inline PlatformParameters WithVulkanPreferCPUForBufferSubData(const PlatformParameters &paramsIn)
{
    PlatformParameters paramsOut                                = paramsIn;
    paramsOut.eglParameters.WithVulkanPreferCPUForBufferSubData = EGL_TRUE;
    return paramsOut;
}

inline PlatformParameters WithForceSubmitImmutableTextureUpdates(const PlatformParameters &params)
{
    PlatformParameters withForceSubmitImmutableTextureUpdates = params;
    withForceSubmitImmutableTextureUpdates.eglParameters.forceSubmitImmutableTextureUpdates =
        EGL_TRUE;
    return withForceSubmitImmutableTextureUpdates;
}

inline PlatformParameters WithCreateVulkanPipelineDuringLink(const PlatformParameters &params)
{
    PlatformParameters withCreateVulkanPipelineDuringLink                     = params;
    withCreateVulkanPipelineDuringLink.eglParameters.createPipelineDuringLink = EGL_TRUE;
    return withCreateVulkanPipelineDuringLink;
}
}  // namespace angle

#endif  // ANGLE_TEST_CONFIGS_H_
