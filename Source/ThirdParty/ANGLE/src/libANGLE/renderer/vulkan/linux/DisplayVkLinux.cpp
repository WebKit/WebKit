//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkLinux.cpp:
//    Implements the class methods for DisplayVkLinux.
//

#include "libANGLE/renderer/vulkan/linux/DisplayVkLinux.h"

#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/linux/DmaBufImageSiblingVkLinux.h"

namespace rx
{

DisplayVkLinux::DisplayVkLinux(const egl::DisplayState &state) : DisplayVk(state) {}

ExternalImageSiblingImpl *DisplayVkLinux::createExternalImageSibling(
    const gl::Context *context,
    EGLenum target,
    EGLClientBuffer buffer,
    const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_LINUX_DMA_BUF_EXT:
            ASSERT(context == nullptr);
            ASSERT(buffer == nullptr);
            return new DmaBufImageSiblingVkLinux(attribs);

        default:
            return DisplayVk::createExternalImageSibling(context, target, buffer, attribs);
    }
}
}  // namespace rx
