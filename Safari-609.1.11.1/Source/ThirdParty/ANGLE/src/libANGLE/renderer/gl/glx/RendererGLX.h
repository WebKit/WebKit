//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererGLX.h: implements createWorkerContext for RendererGL.

#ifndef LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_
#define LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_

#include "libANGLE/renderer/gl/RendererGL.h"

namespace rx
{

class DisplayGLX;

class RendererGLX : public RendererGL
{
  public:
    RendererGLX(std::unique_ptr<FunctionsGL> functions,
                const egl::AttributeMap &attribMap,
                DisplayGLX *display);
    ~RendererGLX() override;

  private:
    WorkerContext *createWorkerContext(std::string *infoLog) override;

    DisplayGLX *mDisplay;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_
