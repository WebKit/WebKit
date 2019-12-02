//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferVk.h:
//    Defines the class interface for RenderbufferVk, implementing RenderbufferImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_RENDERBUFFERVK_H_
#define LIBANGLE_RENDERER_VULKAN_RENDERBUFFERVK_H_

#include "libANGLE/renderer/RenderbufferImpl.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{

class RenderbufferVk : public RenderbufferImpl
{
  public:
    RenderbufferVk(const gl::RenderbufferState &state);
    ~RenderbufferVk() override;

    void onDestroy(const gl::Context *context) override;

    angle::Result setStorage(const gl::Context *context,
                             GLenum internalformat,
                             size_t width,
                             size_t height) override;
    angle::Result setStorageMultisample(const gl::Context *context,
                                        size_t samples,
                                        GLenum internalformat,
                                        size_t width,
                                        size_t height) override;
    angle::Result setStorageEGLImageTarget(const gl::Context *context, egl::Image *image) override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    angle::Result initializeContents(const gl::Context *context,
                                     const gl::ImageIndex &imageIndex) override;

    vk::ImageHelper *getImage() const { return mImage; }
    void releaseOwnershipOfImage(const gl::Context *context);

    GLenum getColorReadFormat(const gl::Context *context) override;
    GLenum getColorReadType(const gl::Context *context) override;

    angle::Result getRenderbufferImage(const gl::Context *context,
                                       const gl::PixelPackState &packState,
                                       gl::Buffer *packBuffer,
                                       GLenum format,
                                       GLenum type,
                                       void *pixels) override;

  private:
    void releaseAndDeleteImage(ContextVk *contextVk);
    void releaseImage(ContextVk *contextVk);

    angle::Result setStorageImpl(const gl::Context *context,
                                 size_t samples,
                                 GLenum internalformat,
                                 size_t width,
                                 size_t height);

    const gl::InternalFormat &getImplementationSizedFormat() const;

    bool mOwnsImage;
    vk::ImageHelper *mImage;
    vk::ImageViewHelper mImageViews;
    RenderTargetVk mRenderTarget;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERBUFFERVK_H_
