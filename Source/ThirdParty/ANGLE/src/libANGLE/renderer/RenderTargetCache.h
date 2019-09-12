//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetCache:
// The RenderTargetCache pattern is used in the D3D9, D3D11 and Vulkan back-ends. It is a
// cache of the various back-end objects (RenderTargets) associated with each Framebuffer
// attachment, be they Textures, Renderbuffers, or Surfaces. The cache is updated in Framebuffer's
// syncState method.
//

#ifndef LIBANGLE_RENDERER_RENDER_TARGET_CACHE_H_
#define LIBANGLE_RENDERER_RENDER_TARGET_CACHE_H_

#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"

namespace rx
{

template <typename RenderTargetT>
class RenderTargetCache final : angle::NonCopyable
{
  public:
    RenderTargetCache();
    ~RenderTargetCache();

    // Update all RenderTargets from the dirty bits.
    angle::Result update(const gl::Context *context,
                         const gl::FramebufferState &state,
                         const gl::Framebuffer::DirtyBits &dirtyBits);

    // Update individual RenderTargets.
    angle::Result updateColorRenderTarget(const gl::Context *context,
                                          const gl::FramebufferState &state,
                                          size_t colorIndex);
    angle::Result updateDepthStencilRenderTarget(const gl::Context *context,
                                                 const gl::FramebufferState &state);

    using RenderTargetArray = gl::AttachmentArray<RenderTargetT *>;

    const RenderTargetArray &getColors() const;
    RenderTargetT *getDepthStencil() const;

    RenderTargetT *getColorRead(const gl::FramebufferState &state) const;

  private:
    angle::Result updateCachedRenderTarget(const gl::Context *context,
                                           const gl::FramebufferAttachment *attachment,
                                           RenderTargetT **cachedRenderTarget);

    gl::AttachmentArray<RenderTargetT *> mColorRenderTargets;
    // We only support a single Depth/Stencil RenderTarget currently.
    RenderTargetT *mDepthStencilRenderTarget;
};

template <typename RenderTargetT>
RenderTargetCache<RenderTargetT>::RenderTargetCache()
    : mColorRenderTargets{{nullptr}}, mDepthStencilRenderTarget(nullptr)
{}

template <typename RenderTargetT>
RenderTargetCache<RenderTargetT>::~RenderTargetCache()
{}

template <typename RenderTargetT>
angle::Result RenderTargetCache<RenderTargetT>::update(const gl::Context *context,
                                                       const gl::FramebufferState &state,
                                                       const gl::Framebuffer::DirtyBits &dirtyBits)
{
    for (auto dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
                ANGLE_TRY(updateDepthStencilRenderTarget(context, state));
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
                break;
            default:
            {
                ASSERT(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0 &&
                       dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX);
                size_t colorIndex =
                    static_cast<size_t>(dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                ANGLE_TRY(updateColorRenderTarget(context, state, colorIndex));
                break;
            }
        }
    }

    return angle::Result::Continue;
}

template <typename RenderTargetT>
const gl::AttachmentArray<RenderTargetT *> &RenderTargetCache<RenderTargetT>::getColors() const
{
    return mColorRenderTargets;
}

template <typename RenderTargetT>
RenderTargetT *RenderTargetCache<RenderTargetT>::getDepthStencil() const
{
    return mDepthStencilRenderTarget;
}

template <typename RenderTargetT>
angle::Result RenderTargetCache<RenderTargetT>::updateColorRenderTarget(
    const gl::Context *context,
    const gl::FramebufferState &state,
    size_t colorIndex)
{
    return updateCachedRenderTarget(context, state.getColorAttachment(colorIndex),
                                    &mColorRenderTargets[colorIndex]);
}

template <typename RenderTargetT>
angle::Result RenderTargetCache<RenderTargetT>::updateDepthStencilRenderTarget(
    const gl::Context *context,
    const gl::FramebufferState &state)
{
    return updateCachedRenderTarget(context, state.getDepthOrStencilAttachment(),
                                    &mDepthStencilRenderTarget);
}

template <typename RenderTargetT>
angle::Result RenderTargetCache<RenderTargetT>::updateCachedRenderTarget(
    const gl::Context *context,
    const gl::FramebufferAttachment *attachment,
    RenderTargetT **cachedRenderTarget)
{
    RenderTargetT *newRenderTarget = nullptr;
    if (attachment)
    {
        ASSERT(attachment->isAttached());
        ANGLE_TRY(attachment->getRenderTarget(context, &newRenderTarget));
    }
    *cachedRenderTarget = newRenderTarget;
    return angle::Result::Continue;
}

template <typename RenderTargetT>
RenderTargetT *RenderTargetCache<RenderTargetT>::getColorRead(
    const gl::FramebufferState &state) const
{
    ASSERT(mColorRenderTargets[state.getReadIndex()] &&
           state.getReadIndex() < mColorRenderTargets.size());
    return mColorRenderTargets[state.getReadIndex()];
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_RENDER_TARGET_CACHE_H_
