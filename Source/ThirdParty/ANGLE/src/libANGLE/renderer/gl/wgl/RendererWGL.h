//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererWGL.h: Renderer class for GL on Windows.  Owns a WGL context object.

#ifndef LIBANGLE_RENDERER_GL_WGL_RENDERERWGL_H_
#define LIBANGLE_RENDERER_GL_WGL_RENDERERWGL_H_

#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/wgl/FunctionsWGL.h"

namespace rx
{
class DisplayWGL;

class RendererWGL : public RendererGL
{
  public:
    RendererWGL(std::unique_ptr<FunctionsGL> functionsGL,
                const egl::AttributeMap &attribMap,
                DisplayWGL *display,
                HGLRC context,
                HGLRC sharedContext,
                const std::vector<int> workerContextAttribs);
    ~RendererWGL() override;

    HGLRC getContext() const;

  private:
    WorkerContext *createWorkerContext(std::string *infoLog) override;
    DisplayWGL *mDisplay;
    HGLRC mContext;
    HGLRC mSharedContext;
    const std::vector<int> mWorkerContextAttribs;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_WGL_RENDERERWGL_H_
