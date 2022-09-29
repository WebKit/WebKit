//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/angle_test_configs.h"

#include "common/debug.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "util/util_gl.h"

#include <algorithm>
#include <cctype>

namespace angle
{
namespace
{
void AppendCapitalizedFeature(std::ostream &stream, Feature feature)
{
    const char *name = GetFeatureName(feature);

    if (name == nullptr)
    {
        stream << "InternalError";
        return;
    }

    const std::string camelCase = angle::ToCamelCase(name);

    stream << static_cast<char>(std::toupper(camelCase[0])) << (camelCase.c_str() + 1);
}

bool HasFeatureOverride(const std::vector<Feature> &overrides, Feature feature)
{
    return std::find(overrides.begin(), overrides.end(), feature) != overrides.end();
}
}  // namespace

PlatformParameters::PlatformParameters()
    : PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, GLESDriverType::AngleEGL)
{}

PlatformParameters::PlatformParameters(EGLenum clientType,
                                       EGLint majorVersion,
                                       EGLint minorVersion,
                                       EGLint profileMask,
                                       const EGLPlatformParameters &eglPlatformParameters)
    : driver(GLESDriverType::AngleEGL),
      noFixture(false),
      eglParameters(eglPlatformParameters),
      clientType(clientType),
      majorVersion(majorVersion),
      minorVersion(minorVersion),
      profileMask(profileMask)
{
    initDefaultParameters();
}

PlatformParameters::PlatformParameters(EGLenum clientType,
                                       EGLint majorVersion,
                                       EGLint minorVersion,
                                       EGLint profileMask,
                                       GLESDriverType driver)
    : driver(driver),
      noFixture(false),
      clientType(clientType),
      majorVersion(majorVersion),
      minorVersion(minorVersion),
      profileMask(profileMask)
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

bool PlatformParameters::isMetal() const
{
    return eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE;
}

bool PlatformParameters::isVulkan() const
{
    return eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
}

bool PlatformParameters::isANGLE() const
{
    return driver == GLESDriverType::AngleEGL;
}

bool PlatformParameters::isDesktopOpenGLFrontend() const
{
    return clientType == EGL_OPENGL_API;
}

void PlatformParameters::initDefaultParameters()
{
    // Default debug layers to enabled in tests.
    eglParameters.debugLayersEnabled = EGL_TRUE;
}

bool PlatformParameters::isEnableRequested(Feature feature) const
{
    return HasFeatureOverride(eglParameters.enabledFeatureOverrides, feature);
}

bool PlatformParameters::isDisableRequested(Feature feature) const
{
    return HasFeatureOverride(eglParameters.disabledFeatureOverrides, feature);
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
    switch (pp.clientType)
    {
        case EGL_OPENGL_ES_API:
            stream << "ES";
            break;

        case EGL_OPENGL_API:
            stream << "GL";
            break;

        case EGL_OPENVG_API:
            stream << "VG";
            break;

        default:
            UNREACHABLE();
            stream << "Error";
            break;
    }

    stream << pp.majorVersion << "_";
    if (pp.minorVersion != 0)
    {
        stream << pp.minorVersion << "_";
    }

    if (pp.clientType == EGL_OPENGL_API)
    {
        if ((pp.profileMask & EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT) != 0)
        {
            stream << "Core_";
        }
        if ((pp.profileMask & EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT) != 0)
        {
            stream << "Compatibility_";
        }
    }
    else
    {
        // Profile mask is only valid for desktop OpenGL contexts.
        ASSERT(pp.profileMask == 0);
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

    if (pp.eglParameters.displayPowerPreference == EGL_LOW_POWER_ANGLE)
    {
        stream << "_LowPowerGPU";
    }

    if (pp.eglParameters.displayPowerPreference == EGL_HIGH_POWER_ANGLE)
    {
        stream << "_HighPowerGPU";
    }

    for (Feature feature : pp.eglParameters.enabledFeatureOverrides)
    {
        stream << "_";
        AppendCapitalizedFeature(stream, feature);
    }
    for (Feature feature : pp.eglParameters.disabledFeatureOverrides)
    {
        stream << "_No";
        AppendCapitalizedFeature(stream, feature);
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
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::D3D9());
}

PlatformParameters ES2_D3D9()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D9());
}

PlatformParameters ES1_D3D11()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::D3D11());
}

PlatformParameters ES2_D3D11()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11());
}

PlatformParameters ES2_D3D11_PRESENT_PATH_FAST()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_PRESENT_PATH_FAST());
}

PlatformParameters ES2_D3D11_FL11_0()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL11_0());
}

PlatformParameters ES2_D3D11_FL10_1()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL10_1());
}

PlatformParameters ES2_D3D11_FL10_0()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL10_0());
}

PlatformParameters ES2_D3D11_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_WARP());
}

PlatformParameters ES2_D3D11_FL11_0_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL11_0_WARP());
}

PlatformParameters ES2_D3D11_FL10_1_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL10_1_WARP());
}

PlatformParameters ES2_D3D11_FL10_0_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL10_0_WARP());
}

PlatformParameters ES2_D3D11_REFERENCE()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_REFERENCE());
}

PlatformParameters ES2_D3D11_FL11_0_REFERENCE()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL11_0_REFERENCE());
}

PlatformParameters ES2_D3D11_FL10_1_REFERENCE()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL10_1_REFERENCE());
}

PlatformParameters ES2_D3D11_FL10_0_REFERENCE()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::D3D11_FL10_0_REFERENCE());
}

PlatformParameters ES3_D3D11()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11());
}

PlatformParameters ES3_D3D11_FL11_1()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11_FL11_1());
}

PlatformParameters ES3_D3D11_FL11_0()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11_FL11_0());
}

PlatformParameters ES3_D3D11_FL10_1()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11_FL10_1());
}

PlatformParameters ES31_D3D11()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::D3D11());
}

PlatformParameters ES31_D3D11_FL11_1()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::D3D11_FL11_1());
}

PlatformParameters ES31_D3D11_FL11_0()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::D3D11_FL11_0());
}

PlatformParameters ES3_D3D11_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11_WARP());
}

PlatformParameters ES3_D3D11_FL11_1_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11_FL11_1_WARP());
}

PlatformParameters ES3_D3D11_FL11_0_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11_FL11_0_WARP());
}

PlatformParameters ES3_D3D11_FL10_1_WARP()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::D3D11_FL10_1_WARP());
}

PlatformParameters ES1_OPENGLES()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::OPENGLES());
}

PlatformParameters ES2_OPENGLES()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::OPENGLES());
}

PlatformParameters ES2_OPENGLES(EGLint major, EGLint minor)
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::OPENGLES(major, minor));
}

PlatformParameters ES3_OPENGLES()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::OPENGLES());
}

PlatformParameters ES3_OPENGLES(EGLint major, EGLint minor)
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::OPENGLES(major, minor));
}

PlatformParameters ES31_OPENGLES()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::OPENGLES());
}

PlatformParameters ES31_OPENGLES(EGLint major, EGLint minor)
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::OPENGLES(major, minor));
}

PlatformParameters ES1_OPENGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::OPENGL());
}

PlatformParameters ES2_OPENGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::OPENGL());
}

PlatformParameters ES2_OPENGL(EGLint major, EGLint minor)
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::OPENGL(major, minor));
}

PlatformParameters ES3_OPENGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::OPENGL());
}

PlatformParameters ES3_OPENGL(EGLint major, EGLint minor)
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::OPENGL(major, minor));
}

PlatformParameters ES31_OPENGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::OPENGL());
}

PlatformParameters ES31_OPENGL(EGLint major, EGLint minor)
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::OPENGL(major, minor));
}

PlatformParameters ES1_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0,
                              EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES2_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0,
                              EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES3_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0,
                              EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES31_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0,
                              EGLPlatformParameters(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE));
}

PlatformParameters ES1_VULKAN()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::VULKAN());
}

PlatformParameters ES1_VULKAN_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES1_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES2_VULKAN()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::VULKAN());
}

PlatformParameters ES2_VULKAN_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES2_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES3_VULKAN()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::VULKAN());
}

PlatformParameters ES3_VULKAN_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES3_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES31_VULKAN()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::VULKAN());
}

PlatformParameters ES31_VULKAN_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES31_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 1, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES32_VULKAN()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 2, 0, egl_platform::VULKAN());
}

PlatformParameters ES32_VULKAN_NULL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 2, 0, egl_platform::VULKAN_NULL());
}

PlatformParameters ES32_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 2, 0, egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters GL32_CORE_VULKAN()
{
    return PlatformParameters(EGL_OPENGL_API, 3, 2, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                              egl_platform::VULKAN());
}

PlatformParameters GL32_CORE_VULKAN_SWIFTSHADER()
{
    return PlatformParameters(EGL_OPENGL_API, 3, 2, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                              egl_platform::VULKAN_SWIFTSHADER());
}

PlatformParameters ES1_METAL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, egl_platform::METAL());
}

PlatformParameters ES2_METAL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, egl_platform::METAL());
}

PlatformParameters ES3_METAL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, egl_platform::METAL());
}

PlatformParameters ES2_WGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, GLESDriverType::SystemWGL);
}

PlatformParameters ES3_WGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, GLESDriverType::SystemWGL);
}

PlatformParameters ES1_EGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 1, 0, 0, GLESDriverType::SystemEGL);
}

PlatformParameters ES2_EGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 2, 0, 0, GLESDriverType::SystemEGL);
}

PlatformParameters ES3_EGL()
{
    return PlatformParameters(EGL_OPENGL_ES_API, 3, 0, 0, GLESDriverType::SystemEGL);
}
}  // namespace angle
