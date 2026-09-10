//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferWgpu.h:
//    Defines the class interface for RenderbufferWgpu, implementing RenderbufferImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_RENDERBUFFERWGPU_H_
#define LIBANGLE_RENDERER_WGPU_RENDERBUFFERWGPU_H_

#include "libANGLE/renderer/RenderbufferImpl.h"
#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_helpers.h"

namespace rx
{

class RenderbufferWgpu : public RenderbufferImpl, public angle::ObserverInterface
{
  public:
    RenderbufferWgpu(const gl::RenderbufferState &state);
    ~RenderbufferWgpu() override;

    void onDestroy(const gl::Context *context) override;

    angle::Result setStorage(const gl::Context *context,
                             GLenum internalformat,
                             GLsizei width,
                             GLsizei height) override;
    angle::Result setStorageMultisample(const gl::Context *context,
                                        GLsizei samples,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        gl::MultisamplingMode mode) override;

    angle::Result setStorageEGLImageTarget(const gl::Context *context, egl::Image *image) override;

    angle::Result initializeContents(const gl::Context *context,
                                     GLenum binding,
                                     const gl::ImageIndex &imageIndex) override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    void releaseOwnershipOfImage(const gl::Context *context);
    webgpu::ImageHelper *getImage() const { return mImage; }

    // We monitor the ImageHelper and set dirty bits if the ImageHelper changes. This can
    // support changes in the ImageHelper even outside the RenderbufferWgpu class.
    void onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message) override;

  private:
    void setImageHelper(webgpu::ImageHelper *imageHelper, bool ownsImageHelper);

    bool mOwnsImage             = false;
    webgpu::ImageHelper *mImage = nullptr;
    RenderTargetWgpu mRenderTarget;

    angle::ObserverBinding mImageObserverBinding;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_RENDERBUFFERWGPU_H_
