//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkLinux.h:
//    Defines the class interface for DisplayVkLinux, which is the base of DisplayVkSimple,
//    DisplayVkHeadless and DisplayVkXcb.  This base class implements the common functionality of
//    handling Linux dma-bufs.
//

#ifndef LIBANGLE_RENDERER_VULKAN_DISPLAY_DISPLAYVKLINUX_H_
#define LIBANGLE_RENDERER_VULKAN_DISPLAY_DISPLAYVKLINUX_H_

#include "libANGLE/renderer/vulkan/DisplayVk.h"

namespace rx
{
class DisplayVkLinux : public DisplayVk
{
  public:
    DisplayVkLinux(const egl::DisplayState &state);

    ExternalImageSiblingImpl *createExternalImageSibling(const gl::Context *context,
                                                         EGLenum target,
                                                         EGLClientBuffer buffer,
                                                         const egl::AttributeMap &attribs) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_DISPLAY_DISPLAYVKLINUX_H_
