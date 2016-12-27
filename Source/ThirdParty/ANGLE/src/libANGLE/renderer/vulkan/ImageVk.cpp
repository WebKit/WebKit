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

ImageVk::ImageVk() : ImageImpl()
{
}

ImageVk::~ImageVk()
{
}

egl::Error ImageVk::initialize()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

gl::Error ImageVk::orphan(egl::ImageSibling *sibling)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
