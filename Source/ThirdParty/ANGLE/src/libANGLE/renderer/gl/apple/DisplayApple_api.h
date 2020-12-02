//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayApple_api.h:
//    Chooses CGL or EAGL either at compile time or runtime based on the platform.
//

#ifndef LIBANGLE_RENDERER_GL_APPLE_DISPLAYAPPLE_API_H_
#define LIBANGLE_RENDERER_GL_APPLE_DISPLAYAPPLE_API_H_

#include "libANGLE/renderer/DisplayImpl.h"

namespace rx
{

DisplayImpl *CreateDisplayCGLOrEAGL(const egl::DisplayState &state);

}  // namespace rx

#endif /* LIBANGLE_RENDERER_GL_APPLE_DISPLAYAPPLE_API_H_ */
