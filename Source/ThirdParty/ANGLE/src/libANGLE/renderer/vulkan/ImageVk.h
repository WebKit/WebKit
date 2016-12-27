//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageVk.h:
//    Defines the class interface for ImageVk, implementing ImageImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_IMAGEVK_H_
#define LIBANGLE_RENDERER_VULKAN_IMAGEVK_H_

#include "libANGLE/renderer/ImageImpl.h"

namespace rx
{

class ImageVk : public ImageImpl
{
  public:
    ImageVk();
    ~ImageVk() override;
    egl::Error initialize() override;

    gl::Error orphan(egl::ImageSibling *sibling) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_IMAGEVK_H_
