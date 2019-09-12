//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayEGL.cpp: Common across EGL parts of platform specific egl::Display implementations

#include "libANGLE/renderer/gl/egl/DisplayEGL.h"

#include "libANGLE/renderer/gl/egl/ImageEGL.h"
#include "libANGLE/renderer/gl/egl/SyncEGL.h"

namespace rx
{

#define EGL_NO_CONFIG ((EGLConfig)0)

DisplayEGL::DisplayEGL(const egl::DisplayState &state)
    : DisplayGL(state), mEGL(nullptr), mConfig(EGL_NO_CONFIG)
{}

DisplayEGL::~DisplayEGL() {}

ImageImpl *DisplayEGL::createImage(const egl::ImageState &state,
                                   const gl::Context *context,
                                   EGLenum target,
                                   const egl::AttributeMap &attribs)
{
    return new ImageEGL(state, context, target, attribs, mEGL);
}

EGLSyncImpl *DisplayEGL::createSync(const egl::AttributeMap &attribs)
{
    return new SyncEGL(attribs, mEGL);
}

std::string DisplayEGL::getVendorString() const
{
    const char *vendor = mEGL->queryString(EGL_VENDOR);
    ASSERT(vendor);
    return vendor;
}

egl::Error DisplayEGL::initializeContext(EGLContext shareContext,
                                         const egl::AttributeMap &eglAttributes,
                                         EGLContext *outContext,
                                         native_egl::AttributeVector *outAttribs) const
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

    EGLContext context = EGL_NO_CONTEXT;
    for (const auto &attribList : contextAttribLists)
    {
        context = mEGL->createContext(mConfig, shareContext, attribList.data());
        if (context != EGL_NO_CONTEXT)
        {
            *outContext = context;
            *outAttribs = attribList;
            return egl::NoError();
        }
    }

    return egl::Error(mEGL->getError(), "eglCreateContext failed");
}

void DisplayEGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    gl::Version eglVersion(mEGL->majorVersion, mEGL->minorVersion);

    outExtensions->createContextRobustness =
        mEGL->hasExtension("EGL_EXT_create_context_robustness");

    outExtensions->postSubBuffer    = false;  // Since SurfaceEGL::postSubBuffer is not implemented
    outExtensions->presentationTime = mEGL->hasExtension("EGL_ANDROID_presentation_time");

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    // We will fallback to regular swap if swapBuffersWithDamage isn't
    // supported, so indicate support here to keep validation happy.
    outExtensions->swapBuffersWithDamage = true;

    outExtensions->image     = mEGL->hasExtension("EGL_KHR_image");
    outExtensions->imageBase = mEGL->hasExtension("EGL_KHR_image_base");
    // Pixmaps are not supported in ANGLE's EGL implementation.
    // outExtensions->imagePixmap = mEGL->hasExtension("EGL_KHR_image_pixmap");
    outExtensions->glTexture2DImage      = mEGL->hasExtension("EGL_KHR_gl_texture_2D_image");
    outExtensions->glTextureCubemapImage = mEGL->hasExtension("EGL_KHR_gl_texture_cubemap_image");
    outExtensions->glTexture3DImage      = mEGL->hasExtension("EGL_KHR_gl_texture_3D_image");
    outExtensions->glRenderbufferImage   = mEGL->hasExtension("EGL_KHR_gl_renderbuffer_image");

    outExtensions->imageNativeBuffer = mEGL->hasExtension("EGL_ANDROID_image_native_buffer");

    outExtensions->getFrameTimestamps = mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps");

    outExtensions->fenceSync =
        eglVersion >= gl::Version(1, 5) || mEGL->hasExtension("EGL_KHR_fence_sync");
    outExtensions->waitSync =
        eglVersion >= gl::Version(1, 5) || mEGL->hasExtension("EGL_KHR_wait_sync");

    DisplayGL::generateExtensions(outExtensions);
}

void DisplayEGL::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;  // Since we request GLES >= 2
}

void DisplayEGL::setBlobCacheFuncs(EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get)
{
    if (mEGL->hasExtension("EGL_ANDROID_blob_cache"))
    {
        mEGL->setBlobCacheFuncsANDROID(set, get);
    }
}

}  // namespace rx
