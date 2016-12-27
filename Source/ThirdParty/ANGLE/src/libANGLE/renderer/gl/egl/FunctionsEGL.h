//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsEGL.h: Defines the FunctionsEGL class to load functions and data from EGL

#ifndef LIBANGLE_RENDERER_GL_CROS_FUNCTIONSEGL_H_
#define LIBANGLE_RENDERER_GL_CROS_FUNCTIONSEGL_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <string>
#include <vector>

#include "libANGLE/Error.h"

namespace rx
{

class FunctionsGL;

class FunctionsEGL
{
  public:
    FunctionsEGL();
    virtual ~FunctionsEGL();

    int majorVersion;
    int minorVersion;

    egl::Error initialize(EGLNativeDisplayType nativeDisplay);
    egl::Error terminate();

    virtual void *getProcAddress(const char *name) const = 0;

    FunctionsGL *makeFunctionsGL() const;
    bool hasExtension(const char *extension) const;
    EGLDisplay getDisplay() const;
    EGLint getError() const;

    EGLBoolean chooseConfig(EGLint const *attrib_list,
                            EGLConfig *configs,
                            EGLint config_size,
                            EGLint *num_config) const;
    EGLBoolean getConfigAttrib(EGLConfig config, EGLint attribute, EGLint *value) const;
    EGLContext createContext(EGLConfig config,
                             EGLContext share_context,
                             EGLint const *attrib_list) const;
    EGLSurface createPbufferSurface(EGLConfig config, const EGLint *attrib_list) const;
    EGLSurface createWindowSurface(EGLConfig config,
                                   EGLNativeWindowType win,
                                   const EGLint *attrib_list) const;
    EGLBoolean destroyContext(EGLContext context) const;
    EGLBoolean destroySurface(EGLSurface surface) const;
    EGLBoolean makeCurrent(EGLSurface surface, EGLContext context) const;
    const char *queryString(EGLint name) const;
    EGLBoolean querySurface(EGLSurface surface, EGLint attribute, EGLint *value) const;
    EGLBoolean swapBuffers(EGLSurface surface) const;

    EGLBoolean bindTexImage(EGLSurface surface, EGLint buffer) const;
    EGLBoolean releaseTexImage(EGLSurface surface, EGLint buffer) const;
    EGLBoolean swapInterval(EGLint interval) const;

    EGLImageKHR createImageKHR(EGLContext context,
                               EGLenum target,
                               EGLClientBuffer buffer,
                               const EGLint *attrib_list) const;
    EGLBoolean destroyImageKHR(EGLImageKHR image) const;

    EGLSyncKHR createSyncKHR(EGLenum type, const EGLint *attrib_list);
    EGLBoolean destroySyncKHR(EGLSyncKHR sync);
    EGLint clientWaitSyncKHR(EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
    EGLBoolean getSyncAttribKHR(EGLSyncKHR sync, EGLint attribute, EGLint *value);

  private:
    // So as to isolate from angle we do not include angleutils.h and cannot
    // use angle::NonCopyable so we replicated it here instead.
    FunctionsEGL(const FunctionsEGL &) = delete;
    void operator=(const FunctionsEGL &) = delete;

    struct EGLDispatchTable;
    EGLDispatchTable *mFnPtrs;
    EGLDisplay mEGLDisplay;
    std::vector<std::string> mExtensions;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_CROS_FUNCTIONSEGL_H_
