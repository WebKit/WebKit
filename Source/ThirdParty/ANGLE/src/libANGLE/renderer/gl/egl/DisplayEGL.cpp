//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayEGL.cpp: Common across EGL parts of platform specific egl::Display implementations

#include "libANGLE/renderer/gl/egl/DisplayEGL.h"

#include "libANGLE/renderer/gl/egl/egl_utils.h"

namespace rx
{

#define EGL_NO_CONFIG ((EGLConfig)0)

DisplayEGL::DisplayEGL(const egl::DisplayState &state)
    : DisplayGL(state),
      mEGL(nullptr),
      mConfig(EGL_NO_CONFIG),
      mContext(EGL_NO_CONTEXT),
      mFunctionsGL(nullptr)
{
}

DisplayEGL::~DisplayEGL()
{
}

std::string DisplayEGL::getVendorString() const
{
    const char *vendor = mEGL->queryString(EGL_VENDOR);
    ASSERT(vendor);
    return vendor;
}

egl::Error DisplayEGL::initializeContext(const egl::AttributeMap &eglAttributes)
{
    gl::Version eglVersion(mEGL->majorVersion, mEGL->minorVersion);

    EGLint requestedMajor =
        eglAttributes.getAsInt(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, EGL_DONT_CARE);
    EGLint requestedMinor =
        eglAttributes.getAsInt(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, EGL_DONT_CARE);
    bool initializeRequested = requestedMajor != EGL_DONT_CARE && requestedMinor != EGL_DONT_CARE;

    static_assert(EGL_CONTEXT_MAJOR_VERSION == EGL_CONTEXT_MAJOR_VERSION_KHR,
                  "Major Version define should match");
    static_assert(EGL_CONTEXT_MINOR_VERSION == EGL_CONTEXT_MINOR_VERSION_KHR,
                  "Minor Version define should match");

    std::vector<native_egl::AttributeVector> contextAttribLists;
    if (eglVersion >= gl::Version(1, 5) || mEGL->hasExtension("EGL_KHR_create_context"))
    {
        if (initializeRequested)
        {
            contextAttribLists.push_back({EGL_CONTEXT_MAJOR_VERSION, requestedMajor,
                                          EGL_CONTEXT_MINOR_VERSION, requestedMinor, EGL_NONE});
        }
        else
        {
            // clang-format off
            const gl::Version esVersionsFrom2_0[] = {
                gl::Version(3, 2),
                gl::Version(3, 1),
                gl::Version(3, 0),
                gl::Version(2, 0),
            };
            // clang-format on

            for (const auto &version : esVersionsFrom2_0)
            {
                contextAttribLists.push_back(
                    {EGL_CONTEXT_MAJOR_VERSION, static_cast<EGLint>(version.major),
                     EGL_CONTEXT_MINOR_VERSION, static_cast<EGLint>(version.minor), EGL_NONE});
            }
        }
    }
    else
    {
        if (initializeRequested && (requestedMajor != 2 || requestedMinor != 0))
        {
            return egl::EglBadAttribute() << "Unsupported requested context version";
        }
        contextAttribLists.push_back({EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    }

    for (const auto &attribList : contextAttribLists)
    {
        mContext = mEGL->createContext(mConfig, EGL_NO_CONTEXT, attribList.data());
        if (mContext != EGL_NO_CONTEXT)
        {
            return egl::NoError();
        }
    }

    return egl::Error(mEGL->getError(), "eglCreateContext failed");
}

void DisplayEGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContextRobustness =
        mEGL->hasExtension("EGL_EXT_create_context_robustness");

    outExtensions->postSubBuffer = false;  // Since SurfaceEGL::postSubBuffer is not implemented

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    // Surfaceless contexts are emulated even if there is no native support.
    outExtensions->surfacelessContext = true;

    DisplayGL::generateExtensions(outExtensions);
}

void DisplayEGL::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;  // Since we request GLES >= 2
}

const FunctionsGL *DisplayEGL::getFunctionsGL() const
{
    return mFunctionsGL;
}
}  // namespace rx
