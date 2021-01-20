//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererEAGL.h: implements createWorkerContext for RendererGL.

#ifndef LIBANGLE_RENDERER_GL_EAGL_RENDEREREAGL_H_
#define LIBANGLE_RENDERER_GL_EAGL_RENDEREREAGL_H_

#include "libANGLE/renderer/gl/RendererGL.h"

namespace rx
{

class DisplayEAGL;

class RendererEAGL : public RendererGL
{
  public:
    RendererEAGL(std::unique_ptr<FunctionsGL> functions,
                 const egl::AttributeMap &attribMap,
                 DisplayEAGL *display);
    ~RendererEAGL() override;

  private:
    WorkerContext *createWorkerContext(std::string *infoLog) override;

    DisplayEAGL *mDisplay;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_GLX_RENDERERGLX_H_
