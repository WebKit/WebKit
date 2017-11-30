//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ImageImpl.h: Defines the rx::ImageImpl class representing the EGLimage object.

#ifndef LIBANGLE_RENDERER_IMAGEIMPL_H_
#define LIBANGLE_RENDERER_IMAGEIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/Error.h"

namespace gl
{
class Context;
}  // namespace gl

namespace egl
{
class ImageSibling;
struct ImageState;
}  // namespace egl

namespace rx
{
class ImageImpl : angle::NonCopyable
{
  public:
    ImageImpl(const egl::ImageState &state) : mState(state) {}
    virtual ~ImageImpl() {}
    virtual egl::Error initialize() = 0;

    virtual gl::Error orphan(const gl::Context *context, egl::ImageSibling *sibling) = 0;

  protected:
    const egl::ImageState &mState;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_IMAGEIMPL_H_
