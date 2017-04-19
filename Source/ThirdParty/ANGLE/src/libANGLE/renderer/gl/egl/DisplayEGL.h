//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayEGL.h: Common across EGL parts of platform specific egl::Display implementations

#ifndef LIBANGLE_RENDERER_GL_EGL_DISPLAYEGL_H_
#define LIBANGLE_RENDERER_GL_EGL_DISPLAYEGL_H_

#include "libANGLE/renderer/gl/DisplayGL.h"
#include "libANGLE/renderer/gl/egl/FunctionsEGL.h"

namespace rx
{

class DisplayEGL : public DisplayGL
{
  public:
    DisplayEGL(const egl::DisplayState &state);
    ~DisplayEGL() override;

    std::string getVendorString() const override;

  protected:
    egl::Error initializeContext(const egl::AttributeMap &eglAttributes);

    FunctionsEGL *mEGL;
    EGLConfig mConfig;
    EGLContext mContext;
    FunctionsGL *mFunctionsGL;

  private:
    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    const FunctionsGL *getFunctionsGL() const override;
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_GL_EGL_DISPLAYEGL_H_ */
