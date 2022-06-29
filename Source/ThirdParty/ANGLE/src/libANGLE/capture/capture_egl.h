//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_egl.h:
//   EGL capture functions
//

#ifndef LIBANGLE_CAPTURE_EGL_H_
#define LIBANGLE_CAPTURE_EGL_H_

#include "libANGLE/Context.h"
#include "libANGLE/Thread.h"
#include "libANGLE/capture/FrameCapture.h"

namespace gl
{
enum class GLenumGroup;
}

namespace egl
{

template <typename CaptureFuncT, typename... ArgsT>
void CaptureCallToCaptureEGL(CaptureFuncT captureFunc, egl::Thread *thread, ArgsT... captureParams)
{
    gl::Context *context = thread->getContext();
    angle::FrameCaptureShared *frameCaptureShared =
        context->getShareGroup()->getFrameCaptureShared();
    if (!frameCaptureShared->isCapturing())
    {
        return;
    }

    angle::CallCapture call = captureFunc(context, captureParams...);

    frameCaptureShared->captureCall(context, std::move(call), true);
}

angle::CallCapture CaptureCreateNativeClientBufferANDROID(gl::Context *context,
                                                          const egl::AttributeMap &attribMap,
                                                          EGLClientBuffer eglClientBuffer);
angle::CallCapture CaptureEGLCreateImage(gl::Context *context,
                                         EGLenum target,
                                         EGLClientBuffer buffer,
                                         const egl::AttributeMap &attributes,
                                         egl::Image *image);
angle::CallCapture CaptureEGLDestroyImage(gl::Context *context,
                                          egl::Display *display,
                                          egl::Image *image);

}  // namespace egl

#endif  // LIBANGLE_FRAME_CAPTURE_H_
