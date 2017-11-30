//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageNULL.cpp:
//    Implements the class methods for ImageNULL.
//

#include "libANGLE/renderer/null/ImageNULL.h"

#include "common/debug.h"

namespace rx
{

ImageNULL::ImageNULL(const egl::ImageState &state) : ImageImpl(state)
{
}

ImageNULL::~ImageNULL()
{
}

egl::Error ImageNULL::initialize()
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

gl::Error ImageNULL::orphan(const gl::Context *context, egl::ImageSibling *sibling)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

}  // namespace rx
