//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DeviceEAGL.cpp: EAGL implementation of egl::Device

#import "common/platform.h"

#if defined(ANGLE_PLATFORM_IOS)

#    include "libANGLE/renderer/gl/eagl/DeviceEAGL.h"

#    include "libANGLE/renderer/gl/eagl/DisplayEAGL.h"

#    include <EGL/eglext.h>

namespace rx
{

DeviceEAGL::DeviceEAGL() {}

DeviceEAGL::~DeviceEAGL() {}

egl::Error DeviceEAGL::initialize()
{
    return egl::NoError();
}

egl::Error DeviceEAGL::getAttribute(const egl::Display *display, EGLint attribute, void **outValue)
{
    DisplayEAGL *displayImpl = GetImplAs<DisplayEAGL>(display);

    switch (attribute)
    {
        case EGL_EAGL_CONTEXT_ANGLE:
            *outValue = displayImpl->getEAGLContext();
            break;
        default:
            return egl::EglBadAttribute();
    }

    return egl::NoError();
}

EGLint DeviceEAGL::getType()
{
    return 0;
}

void DeviceEAGL::generateExtensions(egl::DeviceExtensions *outExtensions) const
{
    outExtensions->deviceEAGL = true;
}

}  // namespace rx

#endif  // defined(ANGLE_PLATFORM_IOS)
