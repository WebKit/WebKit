//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/wgpu/wgpu_helpers.h"

namespace webgpu
{
ImageHelper::ImageHelper() {}

ImageHelper::~ImageHelper() {}

angle::Result ImageHelper::initImage(wgpu::Device &device,
                                     wgpu::TextureUsage usage,
                                     wgpu::TextureDimension dimension,
                                     wgpu::Extent3D size,
                                     wgpu::TextureFormat format,
                                     std::uint32_t mipLevelCount,
                                     std::uint32_t sampleCount,
                                     std::size_t viewFormatCount)
{
    mTextureDescriptor.usage           = usage;
    mTextureDescriptor.dimension       = dimension;
    mTextureDescriptor.size            = size;
    mTextureDescriptor.format          = format;
    mTextureDescriptor.mipLevelCount   = mipLevelCount;
    mTextureDescriptor.sampleCount     = sampleCount;
    mTextureDescriptor.viewFormatCount = viewFormatCount;

    mTexture = device.CreateTexture(&mTextureDescriptor);

    return angle::Result::Continue;
}

void ImageHelper::flushStagedUpdates(wgpu::Device &device)
{
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ImageCopyTexture dst;
    for (const QueuedDataUpload &src : mBufferQueue)
    {
        if (src.targetLevel < mFirstAllocatedLevel ||
            src.targetLevel >= (mFirstAllocatedLevel + mTextureDescriptor.mipLevelCount))
        {
            continue;
        }
        LevelIndex targetLevelWgpu = toWgpuLevel(src.targetLevel);
        dst.texture                = mTexture;
        dst.mipLevel               = targetLevelWgpu.get();
        encoder.CopyBufferToTexture(&src.copyBuffer, &dst, &mTextureDescriptor.size);
    }
}

LevelIndex ImageHelper::toWgpuLevel(gl::LevelIndex levelIndexGl) const
{
    return gl_wgpu::getLevelIndex(levelIndexGl, mFirstAllocatedLevel);
}

gl::LevelIndex ImageHelper::toGlLevel(LevelIndex levelIndexWgpu) const
{
    return wgpu_gl::getLevelIndex(levelIndexWgpu, mFirstAllocatedLevel);
}

TextureInfo ImageHelper::getWgpuTextureInfo(const gl::ImageIndex &index)
{
    TextureInfo textureInfo;
    textureInfo.dimension     = gl_wgpu::getWgpuTextureDimension(index.getType());
    textureInfo.mipLevelCount = index.getLayerCount();
    return textureInfo;
}
}  // namespace webgpu
