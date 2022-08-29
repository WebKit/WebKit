//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Thread.h : Defines the Thread class which represents a global EGL thread.

#ifndef LIBANGLE_THREAD_H_
#define LIBANGLE_THREAD_H_

#include <EGL/egl.h>

#include "libANGLE/Debug.h"

#include <atomic>

namespace angle
{
#if defined(ANGLE_USE_ANDROID_TLS_SLOT)
extern bool gUseAndroidOpenGLTlsSlot;
#endif

void PthreadKeyDestructorCallback(void *ptr);
}  // namespace angle

namespace gl
{
class Context;
}  // namespace gl

namespace egl
{
class Error;
class Debug;
class Display;
class Surface;

class Thread : public LabeledObject
{
  public:
    Thread();

    void setLabel(EGLLabelKHR label) override;
    EGLLabelKHR getLabel() const override;

    void setSuccess();

    void setError(EGLint error,
                  const char *command,
                  const LabeledObject *object,
                  const char *message);

    // TODO: Remove egl::Error. http://anglebug.com/3041
    void setError(const Error &error, const char *command, const LabeledObject *object);
    EGLint getError() const;

    void setAPI(EGLenum api);
    EGLenum getAPI() const;

    void setCurrent(gl::Context *context);
    Surface *getCurrentDrawSurface() const;
    Surface *getCurrentReadSurface() const;
    gl::Context *getContext() const;
    Display *getDisplay() const;

  private:
    EGLLabelKHR mLabel;
    EGLint mError;
    EGLenum mAPI;
    gl::Context *mContext;
};

void EnsureDebugAllocated();
void DeallocateDebug();
Debug *GetDebug();

}  // namespace egl

#endif  // LIBANGLE_THREAD_H_
