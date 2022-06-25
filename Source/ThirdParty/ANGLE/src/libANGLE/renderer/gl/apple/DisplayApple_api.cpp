//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayApple_api.cpp:
//    Chooses CGL or EAGL either at compile time or runtime based on the platform.
//

#ifndef LIBANGLE_RENDERER_GL_APPLE_DISPLAYAPPLE_API_H_
#define LIBANGLE_RENDERER_GL_APPLE_DISPLAYAPPLE_API_H_

#include "gpu_info_util/SystemInfo.h"
#include "libANGLE/renderer/DisplayImpl.h"

#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
#    include "libANGLE/renderer/gl/cgl/DisplayCGL.h"
#    if defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64)
#        include "libANGLE/renderer/gl/eagl/DisplayEAGL.h"
#    endif
#elif defined(ANGLE_PLATFORM_IOS)
#    include "libANGLE/renderer/gl/eagl/DisplayEAGL.h"
#endif

namespace rx
{

DisplayImpl *CreateDisplayCGLOrEAGL(const egl::DisplayState &state)
{
#if defined(ANGLE_PLATFORM_MACOS)
    return new rx::DisplayCGL(state);
#elif defined(ANGLE_PLATFORM_MACCATALYST)
#    if defined(ANGLE_CPU_ARM64)
    angle::SystemInfo info;
    if (!angle::GetSystemInfo(&info))
    {
        return nullptr;
    }

    if (info.needsEAGLOnMac)
    {
        return new rx::DisplayEAGL(state);
    }
    else
    {
        return new rx::DisplayCGL(state);
    }
#    else
    return new rx::DisplayCGL(state);
#    endif
#elif defined(ANGLE_PLATFORM_IOS)
    return new rx::DisplayEAGL(state);
#endif
}

}  // namespace rx

#endif /* LIBANGLE_RENDERER_GL_APPLE_DISPLAYAPPLE_API_H_ */
