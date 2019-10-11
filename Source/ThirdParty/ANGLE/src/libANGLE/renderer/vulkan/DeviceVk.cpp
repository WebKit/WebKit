//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeviceVk.cpp:
//    Implements the class methods for DeviceVk.
//

#include "libANGLE/renderer/vulkan/DeviceVk.h"

#include "common/debug.h"

namespace rx
{

DeviceVk::DeviceVk() : DeviceImpl() {}

DeviceVk::~DeviceVk() {}

egl::Error DeviceVk::initialize()
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error DeviceVk::getAttribute(const egl::Display *display, EGLint attribute, void **outValue)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

EGLint DeviceVk::getType()
{
    UNIMPLEMENTED();
    return EGLint();
}

void DeviceVk::generateExtensions(egl::DeviceExtensions *outExtensions) const
{
    UNIMPLEMENTED();
}

}  // namespace rx
