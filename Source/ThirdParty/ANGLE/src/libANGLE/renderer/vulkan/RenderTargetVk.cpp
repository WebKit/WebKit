//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetVk:
//   Wrapper around a Vulkan renderable resource, using an ImageView.
//

#include "libANGLE/renderer/vulkan/RenderTargetVk.h"

namespace rx
{

RenderTargetVk::RenderTargetVk()
    : format(nullptr),
      image(nullptr),
      imageView(nullptr),
      extents(),
      samples(VK_SAMPLE_COUNT_1_BIT),
      resource(nullptr)
{
}

}  // namespace rx
