//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextEAGL:
//   iOS-specific subclass of ContextGL.
//

#ifndef LIBANGLE_RENDERER_GL_EAGL_CONTEXTEAGL_H_
#define LIBANGLE_RENDERER_GL_EAGL_CONTEXTEAGL_H_

#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"

namespace rx
{

class ContextEAGL : public ContextGL
{
  public:
    ContextEAGL(const gl::State &state,
                gl::ErrorSet *errorSet,
                const std::shared_ptr<RendererGL> &renderer);
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EAGL_CONTEXTEAGL_H_
