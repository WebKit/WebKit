//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferNULL.h:
//    Defines the class interface for RenderbufferNULL, implementing RenderbufferImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_RENDERBUFFERNULL_H_
#define LIBANGLE_RENDERER_NULL_RENDERBUFFERNULL_H_

#include "libANGLE/renderer/RenderbufferImpl.h"

namespace rx
{

class RenderbufferNULL : public RenderbufferImpl
{
  public:
    RenderbufferNULL();
    ~RenderbufferNULL() override;

    gl::Error setStorage(GLenum internalformat, size_t width, size_t height) override;
    gl::Error setStorageMultisample(size_t samples,
                                    GLenum internalformat,
                                    size_t width,
                                    size_t height) override;
    gl::Error setStorageEGLImageTarget(egl::Image *image) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_RENDERBUFFERNULL_H_
