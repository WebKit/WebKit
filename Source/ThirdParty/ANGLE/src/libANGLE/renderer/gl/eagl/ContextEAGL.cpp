//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextEAGL:
//   iOS-specific subclass of ContextGL.
//

#import "common/platform.h"

#if defined(ANGLE_ENABLE_EAGL)

#include "libANGLE/renderer/gl/eagl/ContextEAGL.h"

#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/gl/eagl/DisplayEAGL.h"

namespace rx
{

ContextEAGL::ContextEAGL(const gl::State &state,
                         gl::ErrorSet *errorSet,
                         const std::shared_ptr<RendererGL> &renderer)
    : ContextGL(state, errorSet, renderer, RobustnessVideoMemoryPurgeStatus::NOT_REQUESTED)
{}

}  // namespace rx

#endif  // defined(ANGLE_ENABLE_EAGL)
