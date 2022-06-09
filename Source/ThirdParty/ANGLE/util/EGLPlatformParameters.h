//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLPlatformParameters: Basic description of an EGL device.

#ifndef UTIL_EGLPLATFORMPARAMETERS_H_
#define UTIL_EGLPLATFORMPARAMETERS_H_

#include "util/util_gl.h"

#include <tuple>

namespace angle
{
struct PlatformMethods;

// The GLES driver type determines what shared object we use to load the GLES entry points.
// AngleEGL loads from ANGLE's version of libEGL, libGLESv2, and libGLESv1_CM.
// SystemEGL uses the system copies of libEGL, libGLESv2, and libGLESv1_CM.
// SystemWGL loads Windows GL with the GLES compatibility extensions. See util/WGLWindow.h.
enum class GLESDriverType
{
    AngleEGL,
    SystemEGL,
    SystemWGL,
};
}  // namespace angle

struct EGLPlatformParameters
{
    EGLPlatformParameters() = default;

    explicit EGLPlatformParameters(EGLint renderer) : renderer(renderer) {}

    EGLPlatformParameters(EGLint renderer,
                          EGLint majorVersion,
                          EGLint minorVersion,
                          EGLint deviceType)
        : renderer(renderer),
          majorVersion(majorVersion),
          minorVersion(minorVersion),
          deviceType(deviceType)
    {}

    EGLPlatformParameters(EGLint renderer,
                          EGLint majorVersion,
                          EGLint minorVersion,
                          EGLint deviceType,
                          EGLint presentPath)
        : renderer(renderer),
          majorVersion(majorVersion),
          minorVersion(minorVersion),
          deviceType(deviceType),
          presentPath(presentPath)
    {}

    auto tie() const
    {
        return std::tie(renderer, majorVersion, minorVersion, deviceType, presentPath,
                        debugLayersEnabled, transformFeedbackFeature, allocateNonZeroMemoryFeature,
                        emulateCopyTexImage2DFromRenderbuffers, shaderStencilOutputFeature,
                        genMultipleMipsPerPassFeature, platformMethods, robustness,
                        emulatedPrerotation, asyncCommandQueueFeatureVulkan,
                        hasExplicitMemBarrierFeatureMtl, hasCheapRenderPassFeatureMtl,
                        forceBufferGPUStorageFeatureMtl, supportsVulkanViewportFlip,
                        supportsVulkanMultiDrawIndirect, WithVulkanPreferCPUForBufferSubData,
                        emulatedVAOs, generateSPIRVThroughGlslang, captureLimits,
                        forceRobustResourceInit, directMetalGeneration, forceInitShaderVariables,
                        forceVulkanFallbackFormat, displayPowerPreference,
                        forceSubmitImmutableTextureUpdates, createPipelineDuringLink);
    }

    EGLint renderer                               = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
    EGLint majorVersion                           = EGL_DONT_CARE;
    EGLint minorVersion                           = EGL_DONT_CARE;
    EGLint deviceType                             = EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
    EGLint presentPath                            = EGL_DONT_CARE;
    EGLint debugLayersEnabled                     = EGL_DONT_CARE;
    EGLint robustness                             = EGL_DONT_CARE;
    EGLint transformFeedbackFeature               = EGL_DONT_CARE;
    EGLint allocateNonZeroMemoryFeature           = EGL_DONT_CARE;
    EGLint emulateCopyTexImage2DFromRenderbuffers = EGL_DONT_CARE;
    EGLint shaderStencilOutputFeature             = EGL_DONT_CARE;
    EGLint genMultipleMipsPerPassFeature          = EGL_DONT_CARE;
    uint32_t emulatedPrerotation                  = 0;  // Can be 0, 90, 180 or 270
    EGLint asyncCommandQueueFeatureVulkan         = EGL_DONT_CARE;
    EGLint hasExplicitMemBarrierFeatureMtl        = EGL_DONT_CARE;
    EGLint hasCheapRenderPassFeatureMtl           = EGL_DONT_CARE;
    EGLint forceBufferGPUStorageFeatureMtl        = EGL_DONT_CARE;
    EGLint supportsVulkanViewportFlip             = EGL_DONT_CARE;
    EGLint supportsVulkanMultiDrawIndirect        = EGL_DONT_CARE;
    EGLint WithVulkanPreferCPUForBufferSubData    = EGL_DONT_CARE;
    EGLint emulatedVAOs                           = EGL_DONT_CARE;
    EGLint generateSPIRVThroughGlslang            = EGL_DONT_CARE;
    EGLint captureLimits                          = EGL_DONT_CARE;
    EGLint forceRobustResourceInit                = EGL_DONT_CARE;
    EGLint directMetalGeneration                  = EGL_DONT_CARE;
    EGLint forceInitShaderVariables               = EGL_DONT_CARE;
    EGLint forceVulkanFallbackFormat              = EGL_DONT_CARE;
    EGLint displayPowerPreference                 = EGL_DONT_CARE;
    EGLint forceSubmitImmutableTextureUpdates     = EGL_DONT_CARE;
    EGLint createPipelineDuringLink               = EGL_DONT_CARE;

    angle::PlatformMethods *platformMethods = nullptr;
};

inline bool operator<(const EGLPlatformParameters &a, const EGLPlatformParameters &b)
{
    return a.tie() < b.tie();
}

inline bool operator==(const EGLPlatformParameters &a, const EGLPlatformParameters &b)
{
    return a.tie() == b.tie();
}

inline bool operator!=(const EGLPlatformParameters &a, const EGLPlatformParameters &b)
{
    return a.tie() != b.tie();
}

#endif  // UTIL_EGLPLATFORMPARAMETERS_H_
