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

angle::Result ImageHelper::initImage(wgpu::TextureUsage usage,
                                     wgpu::TextureDimension dimension,
                                     wgpu::Extent3D size,
                                     wgpu::TextureFormat format,
                                     std::uint32_t mipLevelCount,
                                     std::uint32_t sampleCount,
                                     std::size_t viewFormatCount)
{

    mUsage           = usage;
    mDimension       = dimension;
    mSize            = size;
    mFormat          = format;
    mMipLevelCount   = mipLevelCount;
    mSampleCount     = sampleCount;
    mViewFormatCount = viewFormatCount;

    return angle::Result::Continue;
}

void ImageHelper::flushStagedUpdates(wgpu::Device device)
{
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ImageCopyTexture dst;
    for (const QueuedDataUpload &src : mBufferQueue)
    {
        if (src.targetLevel < mFirstAllocatedLevel ||
            src.targetLevel >= (mFirstAllocatedLevel + mMipLevelCount))
        {
            continue;
        }
        LevelIndex targetLevelWgpu = toWgpuLevel(src.targetLevel);
        dst.texture                = mTexture;
        dst.mipLevel               = targetLevelWgpu.get();
        encoder.CopyBufferToTexture(&src.copyBuffer, &dst, &mSize);
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
}  // namespace webgpu

namespace wgpu_gl
{
gl::LevelIndex getLevelIndex(webgpu::LevelIndex levelWgpu, gl::LevelIndex baseLevel)
{
    return gl::LevelIndex(levelWgpu.get() + baseLevel.get());
}
}  // namespace wgpu_gl

namespace gl_wgpu
{
webgpu::LevelIndex getLevelIndex(gl::LevelIndex levelGl, gl::LevelIndex baseLevel)
{
    ASSERT(baseLevel <= levelGl);
    return webgpu::LevelIndex(levelGl.get() - baseLevel.get());
}
}  // namespace gl_wgpu
