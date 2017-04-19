//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetVk:
//   Wrapper around a Vulkan renderable resource, using an ImageView.
//

#ifndef LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_
#define LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_

#include <vulkan/vulkan.h>

#include "libANGLE/FramebufferAttachment.h"

namespace rx
{
class ResourceVk;

namespace vk
{
struct Format;
class Image;
class ImageView;
}

// This is a very light-weight class that does not own to the resources it points to.
// It's meant only to copy across some information from a FramebufferAttachment to the
// business rendering logic.
class RenderTargetVk final : public FramebufferAttachmentRenderTarget
{
  public:
    RenderTargetVk();

    const vk::Format *format;
    vk::Image *image;
    vk::ImageView *imageView;
    gl::Extents extents;
    VkSampleCountFlagBits samples;
    ResourceVk *resource;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERTARGETVK_H_
