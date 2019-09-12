//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceEGL.cpp: EGL implementation of egl::Surface

#include "libANGLE/renderer/gl/egl/SurfaceEGL.h"

#include "common/debug.h"

namespace rx
{

SurfaceEGL::SurfaceEGL(const egl::SurfaceState &state, const FunctionsEGL *egl, EGLConfig config)
    : SurfaceGL(state),
      mEGL(egl),
      mConfig(config),
      mSurface(EGL_NO_SURFACE),
      mHasSwapBuffersWithDamage(mEGL->hasExtension("EGL_KHR_swap_buffers_with_damage"))
{}

SurfaceEGL::~SurfaceEGL()
{
    if (mSurface != EGL_NO_SURFACE)
    {
        EGLBoolean success = mEGL->destroySurface(mSurface);
        ASSERT(success == EGL_TRUE);
    }
}

egl::Error SurfaceEGL::makeCurrent(const gl::Context *context)
{
    // Handling of makeCurrent is done in DisplayEGL
    return egl::NoError();
}

egl::Error SurfaceEGL::swap(const gl::Context *context)
{
    EGLBoolean success = mEGL->swapBuffers(mSurface);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglSwapBuffers failed");
    }
    return egl::NoError();
}

egl::Error SurfaceEGL::swapWithDamage(const gl::Context *context, EGLint *rects, EGLint n_rects)
{
    EGLBoolean success;
    if (mHasSwapBuffersWithDamage)
    {
        success = mEGL->swapBuffersWithDamageKHR(mSurface, rects, n_rects);
    }
    else
    {
        success = mEGL->swapBuffers(mSurface);
    }
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglSwapBuffersWithDamageKHR failed");
    }
    return egl::NoError();
}

egl::Error SurfaceEGL::postSubBuffer(const gl::Context *context,
                                     EGLint x,
                                     EGLint y,
                                     EGLint width,
                                     EGLint height)
{
    UNIMPLEMENTED();
    return egl::EglBadSurface();
}

egl::Error SurfaceEGL::setPresentationTime(EGLnsecsANDROID time)
{
    EGLBoolean success = mEGL->presentationTimeANDROID(mSurface, time);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglPresentationTimeANDROID failed");
    }
    return egl::NoError();
}

egl::Error SurfaceEGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::EglBadSurface();
}

egl::Error SurfaceEGL::bindTexImage(const gl::Context *context, gl::Texture *texture, EGLint buffer)
{
    EGLBoolean success = mEGL->bindTexImage(mSurface, buffer);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglBindTexImage failed");
    }
    return egl::NoError();
}

egl::Error SurfaceEGL::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    EGLBoolean success = mEGL->releaseTexImage(mSurface, buffer);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglReleaseTexImage failed");
    }
    return egl::NoError();
}

void SurfaceEGL::setSwapInterval(EGLint interval)
{
    EGLBoolean success = mEGL->swapInterval(interval);
    if (success == EGL_FALSE)
    {
        ERR() << "eglSwapInterval error " << egl::Error(mEGL->getError());
        ASSERT(false);
    }
}

EGLint SurfaceEGL::getWidth() const
{
    EGLint value;
    EGLBoolean success = mEGL->querySurface(mSurface, EGL_WIDTH, &value);
    ASSERT(success == EGL_TRUE);
    return value;
}

EGLint SurfaceEGL::getHeight() const
{
    EGLint value;
    EGLBoolean success = mEGL->querySurface(mSurface, EGL_HEIGHT, &value);
    ASSERT(success == EGL_TRUE);
    return value;
}

EGLint SurfaceEGL::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return 0;
}

EGLint SurfaceEGL::getSwapBehavior() const
{
    EGLint value;
    EGLBoolean success = mEGL->querySurface(mSurface, EGL_SWAP_BEHAVIOR, &value);
    ASSERT(success == EGL_TRUE);
    return value;
}

EGLSurface SurfaceEGL::getSurface() const
{
    return mSurface;
}

void SurfaceEGL::setTimestampsEnabled(bool enabled)
{
    ASSERT(mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps"));

    EGLBoolean success =
        mEGL->surfaceAttrib(mSurface, EGL_TIMESTAMPS_ANDROID, enabled ? EGL_TRUE : EGL_FALSE);
    if (success == EGL_FALSE)
    {
        ERR() << "eglSurfaceAttribute failed: " << egl::Error(mEGL->getError());
    }
}

egl::SupportedCompositorTimings SurfaceEGL::getSupportedCompositorTimings() const
{
    ASSERT(mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps"));

    egl::SupportedCompositorTimings result;
    for (egl::CompositorTiming name : angle::AllEnums<egl::CompositorTiming>())
    {
        result[name] = mEGL->getCompositorTimingSupportedANDROID(mSurface, egl::ToEGLenum(name));
    }
    return result;
}

egl::Error SurfaceEGL::getCompositorTiming(EGLint numTimestamps,
                                           const EGLint *names,
                                           EGLnsecsANDROID *values) const
{
    ASSERT(mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps"));

    EGLBoolean success = mEGL->getCompositorTimingANDROID(mSurface, numTimestamps, names, values);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglGetCompositorTimingANDROID failed");
    }
    return egl::NoError();
}

egl::Error SurfaceEGL::getNextFrameId(EGLuint64KHR *frameId) const
{
    ASSERT(mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps"));

    EGLBoolean success = mEGL->getNextFrameIdANDROID(mSurface, frameId);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglGetNextFrameId failed");
    }
    return egl::NoError();
}

egl::SupportedTimestamps SurfaceEGL::getSupportedTimestamps() const
{
    ASSERT(mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps"));

    egl::SupportedTimestamps result;
    for (egl::Timestamp timestamp : angle::AllEnums<egl::Timestamp>())
    {
        result[timestamp] =
            mEGL->getFrameTimestampSupportedANDROID(mSurface, egl::ToEGLenum(timestamp));
    }
    return result;
}

egl::Error SurfaceEGL::getFrameTimestamps(EGLuint64KHR frameId,
                                          EGLint numTimestamps,
                                          const EGLint *timestamps,
                                          EGLnsecsANDROID *values) const
{
    ASSERT(mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps"));

    // The driver may return EGL_BAD_ACCESS at any time if the requested frame is no longer stored.
    EGLBoolean success =
        mEGL->getFrameTimestampsANDROID(mSurface, frameId, numTimestamps, timestamps, values);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglGetFrameTimestampsANDROID failed");
    }
    return egl::NoError();
}

}  // namespace rx
