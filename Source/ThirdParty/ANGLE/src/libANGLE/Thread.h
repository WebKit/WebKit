//
// Copyright(c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Thread.h : Defines the Thread class which represents a global EGL thread.

#ifndef LIBANGLE_THREAD_H_
#define LIBANGLE_THREAD_H_

#include <EGL/egl.h>

namespace gl
{
class Context;
}  // namespace gl

namespace egl
{
class Error;
class Display;
class Surface;

class Thread
{
  public:
    Thread();

    void setError(const Error &error);
    EGLint getError() const;

    void setAPI(EGLenum api);
    EGLenum getAPI() const;

    void setCurrent(Display *display,
                    Surface *drawSurface,
                    Surface *readSurface,
                    gl::Context *context);
    Display *getDisplay() const;
    Surface *getDrawSurface() const;
    Surface *getReadSurface() const;
    gl::Context *getContext() const;
    gl::Context *getValidContext() const;

  private:
    EGLint mError;
    EGLenum mAPI;
    egl::Display *mDisplay;
    egl::Surface *mDrawSurface;
    egl::Surface *mReadSurface;
    gl::Context *mContext;
};

}  // namespace egl

#endif  // LIBANGLE_THREAD_H_
