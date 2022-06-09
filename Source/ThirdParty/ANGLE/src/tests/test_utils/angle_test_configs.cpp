//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/angle_test_configs.h"

#include "common/platform.h"
#include "util/util_gl.h"

namespace angle
{

PlatformParameters::PlatformParameters() : PlatformParameters(2, 0, GLESDriverType::AngleEGL) {}

PlatformParameters::PlatformParameters(EGLint majorVersion,
                                       EGLint minorVersion,
                                       const EGLPlatformParameters &eglPlatformParameters)
    : driver(GLESDriverType::AngleEGL),
      noFixture(false),
      eglParameters(eglPlatformParameters),
      majorVersion(majorVersion),
      minorVersion(minorVersion)
{
    initDefaultParameters();
}

PlatformParameters::PlatformParameters(EGLint majorVersion,
                                       EGLint minorVersion,
                                       GLESDriverType driver)
    : driver(driver), noFixture(false), majorVersion(majorVersion), minorVersion(minorVersion)
{
    initDefaultParameters();
}

EGLint PlatformParameters::getRenderer() const
{
    return eglParameters.renderer;
}

EGLint PlatformParameters::getDeviceType() const
{
    return eglParameters.deviceType;
}

bool PlatformParameters::isSwiftshader() const
{
    return eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE;
}

bool PlatformParameters::isVulkan() const
{
    return eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
}

bool PlatformParameters::isANGLE() const
{
    return driver == GLESDriverType::AngleEGL;
}

EGLint PlatformParameters::getAllocateNonZeroMemoryFeature() const
{
    return eglParameters.allocateNonZeroMemoryFeature;
}

void PlatformParameters::initDefaultParameters()
{
    // Default debug layers to enabled in tests.
    eglParameters.debugLayersEnabled = EGL_TRUE;
}

bool operator<(const PlatformParameters &a, const PlatformParameters &b)
{
    return a.tie() < b.tie();
}

bool operator==(const PlatformParameters &a, const PlatformParameters &b)
{
    return a.tie() == b.tie();
}

bool operator!=(const PlatformParameters &a, const PlatformParameters &b)
{
    return a.tie() != b.tie();
}

const char *GetRendererName(EGLint renderer)
{
    switch (renderer)
    {
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            return "Default";
        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
            return "D3D9";
        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
            return "D3D11";
        case EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE:
            return "Metal";
        case EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE:
            return "Null";
        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
            return "OpenGL";
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
            return "OpenGLES";
        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
            return "Vulkan";
        default:
            return "Undefined";
    }
}

std::ostream &operator<<(std::ostream &stream, const PlatformParameters &pp)
{
    stream << "ES" << pp.majorVersion << "_";
    if (pp.minorVersion != 0)
    {
        stream << pp.minorVersion << "_";
    }

    switch (pp.driver)
    {
        case GLESDriverType::AngleEGL:
            stream << GetRendererName(pp.eglParameters.renderer);
            break;
        case GLESDriverType::SystemWGL:
            stream << "WGL";
            break;
        case GLESDriverType::SystemEGL:
            stream << "EGL";
            break;
        default:
            stream << "Error";
            break;
    }

    if (pp.eglParameters.majorVersion != EGL_DONT_CARE)
    {
        stream << "_" << pp.eglParameters.majorVersion;
    }

    if (pp.eglParameters.minorVersion != EGL_DONT_CARE)
    {
        stream << "_" << pp.eglParameters.minorVersion;
    }

    switch (pp.eglParameters.deviceType)
    {
        case EGL_DONT_CARE:
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
            // default
            break;

        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
            stream << "_Null";
            break;

        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE:
            stream << "_Reference";
            break;

        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE:
            stream << "_Warp";
            break;

        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE:
            stream << "_SwiftShader";
            break;

        default:
            stream << "_Error";
            break;
    }

    switch (pp.eglParameters.presentPath)
    {
        case EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE:
            stream << "_PresentPathCopy";
            break;

        case EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE:
            stream << "_PresentPathFast";
            break;

        case EGL_DONT_CARE:
            // default
            break;

        default:
            stream << "_Error";
            break;
    }

    if (pp.noFixture)
    {
        stream << "_NoFixture";
    }

    if (pp.eglParameters.transformFeedbackFeature == EGL_FALSE)
    {
        stream << "_NoTransformFeedback";
    }
    else if (pp.eglParameters.transformFeedbackFeature == EGL_TRUE)
    {
        stream << "_TransformFeedback";
    }

    if (pp.eglParameters.allocateNonZeroMemoryFeature == EGL_FALSE)
    {
        stream << "_NoAllocateNonZeroMemory";
    }
    else if (pp.eglParameters.allocateNonZeroMemoryFeature == EGL_TRUE)
    {
        stream << "_AllocateNonZeroMemory";
    }

    if (pp.eglParameters.emulateCopyTexImage2DFromRenderbuffers == EGL_TRUE)
    {
        stream << "_EmulateCopyTexImage2DFromRenderbuffers";
    }

    if (pp.eglParameters.shaderStencilOutputFeature == EGL_FALSE)
    {
        stream << "_NoStencilOutput";
    }

    if (pp.eglParameters.genMultipleMipsPerPassFeature == EGL_FALSE)
    {
        stream << "_NoGenMultipleMipsPerPass";
    }

    switch (pp.eglParameters.emulatedPrerotation)
    {
        case 90:
            stream << "_PreRotate90";
            break;
        case 180:
            stream << "_PreRotate180";
            break;
        case 270:
            stream << "_PreRotate270";
            break;
        default:
            break;
    }

    if (pp.eglParameters.asyncCommandQueueFeatureVulkan == EGL_TRUE)
    {
        stream << "_AsyncQueue";
    }

    if (pp.eglParameters.hasExplicitMemBarrierFeatureMtl == EGL_FALSE)
    {
        stream << "_NoMetalExplicitMemoryBarrier";
    }

    if (pp.eglParameters.hasCheapRenderPassFeatureMtl == EGL_FALSE)
    {
        stream << "_NoMetalCheapRenderPass";
    }

    if (pp.eglParameters.forceBufferGPUStorageFeatureMtl == EGL_TRUE)
    {
        stream << "_ForceMetalBufferGPUStorage";
    }

    if (pp.eglParameters.supportsVulkanViewportFlip == EGL_TRUE)
    {
        stream << "_VulkanViewportFlip";
    }
    else if (pp.eglParameters.supportsVulkanViewportFlip == EGL_FALSE)
    {
        stream << "_NoVulkanViewportFlip";
    }

    if (pp.eglParameters.supportsVulkanMultiDrawIndirect == EGL_TRUE)
    {
        stream << "_VulkanMultiDrawIndirect";
    }
    else if (pp.eglParameters.supportsVulkanMultiDrawIndirect == EGL_FALSE)
    {
        stream << "_VulkanNoMultiDrawIndirect";
    }

    if (pp.eglParameters.WithVulkanPreferCPUForBufferSubData == EGL_TRUE)
    {
        stream << "_VulkanPreferCPUForBufferSubData";
    }
    else if (pp.eglParameters.WithVulkanPreferCPUForBufferSubData == EGL_FALSE)
    {
        stream << "_VulkanNoPreferCPUForBufferSubData";
    }

    if (pp.eglParameters.emulatedVAOs == EGL_TRUE)
    {
        stream << "_EmulatedVAOs";
    }

    if (pp.eglParameters.generateSPIRVThroughGlslang == EGL_TRUE)
    {
        stream << "_Glslang";
    }

    if (pp.eglParameters.directMetalGeneration == EGL_TRUE)
    {
        stream << "_DirectMetalGen";
    }

    if (pp.eglParameters.forceInitShaderVariables == EGL_TRUE)
    {
        stream << "_InitShaderVars";
    }

    if (pp.eglParameters.forceVulkanFallbackFormat == EGL_TRUE)
    {
        stream << "_FallbackFormat";
    }

    if (pp.eglParameters.displayPowerPreference == EGL_LOW_POWER_ANGLE)
    {
        stream << "_LowPowerGPU";
    }

    if (pp.eglParameters.displayPowerPreference == EGL_HIGH_POWER_ANGLE)
    {
        stream << "_HighPowerGPU";
    }

    if (pp.eglParameters.forceSubmitImmutableTextureUpdates == EGL_TRUE)
    {
        stream << "_VulkanForceSubmitImmutableTextureUpdates";
    }

    if (pp.eglParameters.createPipelineDuringLink == EGL_TRUE)
    {
        stream << "_CreatePipelineDuringLink";
    }

    return stream;
}

// EGL platforms
namespace egl_platform
{

EGLPlatformParameters DEFAULT()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
}

EGLPlatformParameters DEFAULT_NULL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE, EGL_DONT_CARE,
                                 EGL_DONT_CARE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
}

EGLPlatformParameters D3D9()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
}

EGLPlatformParameters D3D9_NULL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
}

EGLPlatformParameters D3D9_REFERENCE()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE);
}

EGLPlatformParameters D3D11()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
}

EGLPlatformParameters D3D11_PRESENT_PATH_FAST()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
                                 EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE);
}

EGLPlatformParameters D3D11_FL11_1()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 11, 1,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
}

EGLPlatformParameters D3D11_FL11_0()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 11, 0,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
}

EGLPlatformParameters D3D11_FL10_1()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 10, 1,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
}

EGLPlatformParameters D3D11_FL10_0()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 10, 0,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
}

EGLPlatformParameters D3D11_NULL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
}

EGLPlatformParameters D3D11_WARP()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE);
}

EGLPlatformParameters D3D11_FL11_1_WARP()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 11, 1,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE);
}

EGLPlatformParameters D3D11_FL11_0_WARP()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 11, 0,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE);
}

EGLPlatformParameters D3D11_FL10_1_WARP()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 10, 1,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE);
}

EGLPlatformParameters D3D11_FL10_0_WARP()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 10, 0,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE);
}

EGLPlatformParameters D3D11_REFERENCE()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE);
}

EGLPlatformParameters D3D11_FL11_1_REFERENCE()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 11, 1,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE);
}

EGLPlatformParameters D3D11_FL11_0_REFERENCE()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 11, 0,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE);
}

EGLPlatformParameters D3D11_FL10_1_REFERENCE()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 10, 1,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE);
}

EGLPlatformParameters D3D11_FL10_0_REFERENCE()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, 10, 0,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE);
}

EGLPlatformParameters OPENGL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE);
}

EGLPlatformParameters OPENGL(EGLint major, EGLint minor)
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE, major, minor, EGL_DONT_CARE);
}

EGLPlatformParameters OPENGL_NULL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
}

EGLPlatformParameters OPENGLES()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE);
}

EGLPlatformParameters OPENGLES(EGLint major, EGLint minor)
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE, major, minor,
                                 EGL_DONT_CARE);
}

EGLPlatformParameters OPENGLES_NULL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE, EGL_DONT_CARE,
                                 EGL_DONT_CARE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
}

EGLPlatformParameters OPENGL_OR_GLES()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return OPENGLES();
#else
    return OPENGL();
#endif
}

EGLPlatformParameters OPENGL_OR_GLES(EGLint major, EGLint minor)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return OPENGLES(major, minor);
#else
    return OPENGL(major, minor);
#endif
}

EGLPlatformParameters OPENGL_OR_GLES_NULL()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return OPENGLES_NULL();
#else
    return OPENGL_NULL();
#endif
}

EGLPlatformParameters VULKAN()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE);
}

EGLPlatformParameters VULKAN_NULL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
}

EGLPlatformParameters VULKAN_SWIFTSHADER()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, EGL_DONT_CARE, EGL_DONT_CARE,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE);
}
EGLPlatformParameters METAL()
{
    return EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE);
}

}  // namespace egl_platform

// ANGLE tests platforms
PlatformParameters ES1_D3D9()
{
    return PlatformParameters(1, 0, egl_platform::D3D9());
}

PlatformParameters ES2_D3D9()
{
    return PlatformParameters(2, 0, egl_platform::D3D9());
}

PlatformParameters ES1_D3D11()
{
    return PlatformParameters(1, 0, egl_platform::D3D11());
}

PlatformParameters ES2_D3D11()
{
    return PlatformParameters(2, 0, egl_platform::D3D11());
}

PlatformParameters ES2_D3D11_PRESENT_PATH_FAST()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_PRESENT_PATH_FAST());
}

PlatformParameters ES2_D3D11_FL11_0()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL11_0());
}

PlatformParameters ES2_D3D11_FL10_1()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL10_1());
}

PlatformParameters ES2_D3D11_FL10_0()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL10_0());
}

PlatformParameters ES2_D3D11_WARP()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_WARP());
}

PlatformParameters ES2_D3D11_FL11_0_WARP()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL11_0_WARP());
}

PlatformParameters ES2_D3D11_FL10_1_WARP()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL10_1_WARP());
}

PlatformParameters ES2_D3D11_FL10_0_WARP()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL10_0_WARP());
}

PlatformParameters ES2_D3D11_REFERENCE()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_REFERENCE());
}

PlatformParameters ES2_D3D11_FL11_0_REFERENCE()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL11_0_REFERENCE());
}

PlatformParameters ES2_D3D11_FL10_1_REFERENCE()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL10_1_REFERENCE());
}

PlatformParameters ES2_D3D11_FL10_0_REFERENCE()
{
    return PlatformParameters(2, 0, egl_platform::D3D11_FL10_0_REFERENCE());
}

PlatformParameters ES3_D3D11()
{
    return PlatformParameters(3, 0, egl_platform::D3D11());
}

PlatformParameters ES3_D3D11_FL11_1()
{
    return PlatformParameters(3, 0, egl_platform::D3D11_FL11_1());
}

PlatformParameters ES3_D3D11_FL11_0()
{
    return PlatformParameters(3, 0, egl_platform::D3D11_FL11_0());
}

PlatformParameters ES3_D3D11_FL10_1()
{
    return PlatformParameters(3, 0, egl_platform::D3D11_FL10_1());
}

PlatformParameters ES31_D3D11()
{
    return PlatformParameters(3, 1, egl_platform::D3D11());
}

PlatformParameters ES31_D3D11_FL11_1()
{
    return PlatformParameters(3, 1, egl_platform::D3D11_FL11_1());
}

PlatformParameters ES31_D3D11_FL11_0()
{
    return PlatformParameters(3, 1, egl_platform::D3D11_FL11_0());
}

PlatformParameters ES3_D3D11_WARP()
{
    return PlatformParameters(3, 0, egl_platform::D3D11_WARP());
}

PlatformParameters ES3_D3D11_FL11_1_WARP()
{
    return PlatformParameters(3, 0, egl_platform::D3D11_FL11_1_WARP());
}

PlatformParameters ES3_D3D11_FL11_0_WARP()
{
    return PlatformParameters(3, 0, egl_platform::D3D11_FL11_0_WARP());
}

PlatformParameters ES3_D3D11_FL10_1_WARP()
{
    return PlatformParameters(3, 0, egl_platform::D3D11_FL10_1_WARP());
}

PlatformParameters ES1_OPENGLES()
{
    return PlatformParameters(1, 0, egl_platform::OPENGLES());
}

PlatformParameters ES2_OPENGLES()
{
    return PlatformParameters(2, 0, egl_platform::OPENGLES());
}

PlatformParameters ES2_OPENGLES(EGLint major, EGLint minor)
{
    return PlatformParameters(2, 0, egl_platform::OPENGLES(major, minor));
}

PlatformParameters ES3_OPENGLES()
{
    return PlatformParameters(3, 0, egl_platform::OPENGLES());
}

PlatformParameters ES3_OPENGLES(EGLint major, EGLint minor)
{
    return PlatformParameters(3, 0, egl_platform::OPENGLES(major, minor));
}

PlatformParameters ES31_OPENGLES()
{
    return PlatformParameters(3, 1, egl_platform::OPENGLES());
}

PlatformParameters ES31_OPENGLES(EGLint major, EGLint minor)
{
    return PlatformParameters(3, 1, egl_platform::OPENGLES(major, minor));
}

PlatformParameters ES1_OPENGL()
{
    return PlatformParameters(1, 0, egl_platform::OPENGL());
}

PlatformParameters ES2_OPENGL()
{
    return PlatformParameters(2, 0, egl_platform::OPENGL());
}

PlatformParameters ES2_OPENGL(EGLint major, EGLint minor)
{
    return PlatformParameters(2, 0, egl_platform::OPENGL(major, minor));
}

PlatformParameters ES3_OPENGL()
{
    return PlatformParameters(3, 0, egl_platform::OPENGL());
}

PlatformParameters ES3_OPENGL(EGLint major, EGLint minor)
{
    return PlatformParameters(3, 0, egl_platform::OPENGL(major, minor));
}

PlatformParameters ES31_OPENGL()
{
    return PlatformParameters(3, 1, egl_platform::OPENGL());
}

PlatformParameters ES31_OPENGL(EGLint major, EGLint minor)
{
    return PlatformParameters(3, 1, egl_platform::OPENGL(major, minor));
}

PlatformParameters ES1_NULL()
{
    return PlatformParameters(1, 0, EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES2_NULL()
{
    return PlatformParameters(2, 0, EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES3_NULL()
{
    return PlatformParameters(3, 0, EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES31_NULL()
{
    return PlatformParameters(3, 1, EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES1_VULKAN()
{
    return PlatformParameters(1, 0, egl_platform::VULKAN());
}

PlatformParameters ES1_VULKAN_NULL()
{
    return PlatformParameters(1, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES1_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(1, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES2_VULKAN()
{
    return PlatformParameters(2, 0, egl_platform::VULKAN());
}

PlatformParameters ES2_VULKAN_NULL()
{
    return PlatformParameters(2, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES2_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(2, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES3_VULKAN()
{
    return PlatformParameters(3, 0, egl_platform::VULKAN());
}

PlatformParameters ES3_VULKAN_NULL()
{
    return PlatformParameters(3, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES3_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(3, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES31_VULKAN()
{
    return PlatformParameters(3, 1, egl_platform::VULKAN());
}

PlatformParameters ES31_VULKAN_NULL()
{
    return PlatformParameters(3, 1, egl_platform::VULKAN_NULL());
}

PlatformParameters ES31_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(3, 1, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES32_VULKAN()
{
    return PlatformParameters(3, 2, egl_platform::VULKAN());
}

PlatformParameters ES32_VULKAN_NULL()
{
    return PlatformParameters(3, 2, egl_platform::VULKAN_NULL());
}

PlatformParameters ES32_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(3, 2, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES1_METAL()
{
    return PlatformParameters(1, 0, egl_platform::METAL());
}

PlatformParameters ES2_METAL()
{
    return PlatformParameters(2, 0, egl_platform::METAL());
}

PlatformParameters ES3_METAL()
{
    return PlatformParameters(3, 0, egl_platform::METAL());
}

PlatformParameters ES2_WGL()
{
    return PlatformParameters(2, 0, GLESDriverType::SystemWGL);
}

PlatformParameters ES3_WGL()
{
    return PlatformParameters(3, 0, GLESDriverType::SystemWGL);
}

PlatformParameters ES2_EGL()
{
    return PlatformParameters(2, 0, GLESDriverType::SystemEGL);
}

PlatformParameters ES3_EGL()
{
    return PlatformParameters(3, 0, GLESDriverType::SystemEGL);
}
}  // namespace angle
