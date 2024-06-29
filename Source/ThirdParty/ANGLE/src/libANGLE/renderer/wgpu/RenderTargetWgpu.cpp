//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetWgpu.cpp:
//    Implements the class methods for RenderTargetWgpu.
//

#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"

namespace rx
{
RenderTargetWgpu::RenderTargetWgpu() {}

RenderTargetWgpu::~RenderTargetWgpu()
{
    reset();
}

RenderTargetWgpu::RenderTargetWgpu(RenderTargetWgpu &&other)
    : mImage(other.mImage),
      mTexture(std::move(other.mTexture)),
      mLevelIndex(other.mLevelIndex),
      mLayerIndex(other.mLayerIndex),
      mFormat(other.mFormat)
{}

void RenderTargetWgpu::set(webgpu::ImageHelper *image,
                           const wgpu::TextureView &texture,
                           const webgpu::LevelIndex level,
                           uint32_t layer,
                           const wgpu::TextureFormat &format)
{
    mImage      = image;
    mTexture    = texture;
    mLevelIndex = level;
    mLayerIndex = layer;
    mFormat     = &format;
}

void RenderTargetWgpu::setTexture(const wgpu::TextureView &texture)
{
    mTexture = texture;
}

void RenderTargetWgpu::reset()
{
    mTexture    = nullptr;
    mLevelIndex = webgpu::LevelIndex(0);
    mLayerIndex = 0;
    mFormat     = nullptr;
}
}  // namespace rx
