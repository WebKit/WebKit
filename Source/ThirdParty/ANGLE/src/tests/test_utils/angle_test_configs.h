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

// The GLES driver type determines what shared object we use to load the GLES entry points.
// AngleEGL loads from ANGLE's version of libEGL, libGLESv2, and libGLESv1_CM.
// SystemEGL uses the system copies of libEGL, libGLESv2, and libGLESv1_CM.
// SystemWGL loads Windows GL with the GLES compatiblity extensions. See util/WGLWindow.h.
enum class GLESDriverType
{
    AngleEGL,
    SystemEGL,
    SystemWGL,
};

struct PlatformParameters
{
    PlatformParameters();
    PlatformParameters(EGLint majorVersion,
                       EGLint minorVersion,
                       const EGLPlatformParameters &eglPlatformParameters);
    PlatformParameters(EGLint majorVersion, EGLint minorVersion, GLESDriverType driver);

    EGLint getRenderer() const;
    EGLint getDeviceType() const;

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
EGLPlatformParameters D3D11_FL9_3();

EGLPlatformParameters D3D11_NULL();

EGLPlatformParameters D3D11_WARP();
EGLPlatformParameters D3D11_FL11_1_WARP();
EGLPlatformParameters D3D11_FL11_0_WARP();
EGLPlatformParameters D3D11_FL10_1_WARP();
EGLPlatformParameters D3D11_FL10_0_WARP();
EGLPlatformParameters D3D11_FL9_3_WARP();

EGLPlatformParameters D3D11_REFERENCE();
EGLPlatformParameters D3D11_FL11_1_REFERENCE();
EGLPlatformParameters D3D11_FL11_0_REFERENCE();
EGLPlatformParameters D3D11_FL10_1_REFERENCE();
EGLPlatformParameters D3D11_FL10_0_REFERENCE();
EGLPlatformParameters D3D11_FL9_3_REFERENCE();

EGLPlatformParameters OPENGL();
EGLPlatformParameters OPENGL(EGLint major, EGLint minor);
EGLPlatformParameters OPENGL_NULL();

EGLPlatformParameters OPENGLES();
EGLPlatformParameters OPENGLES(EGLint major, EGLint minor);
EGLPlatformParameters OPENGLES_NULL();

EGLPlatformParameters OPENGL_OR_GLES();
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
PlatformParameters ES2_D3D11_FL9_3();

PlatformParameters ES2_D3D11_WARP();
PlatformParameters ES2_D3D11_FL11_0_WARP();
PlatformParameters ES2_D3D11_FL10_1_WARP();
PlatformParameters ES2_D3D11_FL10_0_WARP();
PlatformParameters ES2_D3D11_FL9_3_WARP();

PlatformParameters ES2_D3D11_REFERENCE();
PlatformParameters ES2_D3D11_FL11_0_REFERENCE();
PlatformParameters ES2_D3D11_FL10_1_REFERENCE();
PlatformParameters ES2_D3D11_FL10_0_REFERENCE();
PlatformParameters ES2_D3D11_FL9_3_REFERENCE();

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

PlatformParameters ES1_METAL();
PlatformParameters ES2_METAL();
PlatformParameters ES3_METAL();

PlatformParameters ES2_WGL();
PlatformParameters ES3_WGL();

inline PlatformParameters WithNoVirtualContexts(const PlatformParameters &params)
{
    PlatformParameters withNoVirtualContexts                  = params;
    withNoVirtualContexts.eglParameters.contextVirtualization = EGL_FALSE;
    return withNoVirtualContexts;
}

inline PlatformParameters WithNoFixture(const PlatformParameters &params)
{
    PlatformParameters withNoFixture = params;
    withNoFixture.noFixture          = true;
    return withNoFixture;
}

inline PlatformParameters WithNoCommandGraph(const PlatformParameters &params)
{
    PlatformParameters withNoCommandGraph                = params;
    withNoCommandGraph.eglParameters.commandGraphFeature = EGL_FALSE;
    return withNoCommandGraph;
}

inline PlatformParameters WithNoTransformFeedback(const PlatformParameters &params)
{
    PlatformParameters withNoTransformFeedback                     = params;
    withNoTransformFeedback.eglParameters.transformFeedbackFeature = EGL_FALSE;
    return withNoTransformFeedback;
}

}  // namespace angle

#endif  // ANGLE_TEST_CONFIGS_H_
