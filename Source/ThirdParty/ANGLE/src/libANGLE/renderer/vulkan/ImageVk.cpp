//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageVk.cpp:
//    Implements the class methods for ImageVk.
//

#include "libANGLE/renderer/vulkan/ImageVk.h"

#include "common/debug.h"

namespace rx
{

ImageVk::ImageVk(const egl::ImageState &state) : ImageImpl(state)
{
}

ImageVk::~ImageVk()
{
}

egl::Error ImageVk::initialize()
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

gl::Error ImageVk::orphan(const gl::Context *context, egl::ImageSibling *sibling)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

}  // namespace rx
