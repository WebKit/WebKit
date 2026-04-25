//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageWgpu.h:
//    Defines the class interface for ImageWgpu, implementing ImageImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_IMAGEWGPU_H_
#define LIBANGLE_RENDERER_WGPU_IMAGEWGPU_H_

#include "libANGLE/AttributeMap.h"
#include "libANGLE/renderer/ImageImpl.h"
#include "libANGLE/renderer/wgpu/wgpu_helpers.h"

namespace rx
{
class ExternalImageSiblingWgpu : public ExternalImageSiblingImpl
{
  public:
    ExternalImageSiblingWgpu() {}
    ~ExternalImageSiblingWgpu() override {}

    virtual webgpu::ImageHelper *getImage() const = 0;
};

class WebGPUTextureImageSiblingWgpu : public ExternalImageSiblingWgpu
{
  public:
    WebGPUTextureImageSiblingWgpu(EGLClientBuffer buffer, const egl::AttributeMap &attribs);
    ~WebGPUTextureImageSiblingWgpu() override;

    egl::Error initialize(const egl::Display *display) override;
    void onDestroy(const egl::Display *display) override;

    // ExternalImageSiblingImpl interface
    gl::Format getFormat() const override;
    bool isRenderable(const gl::Context *context) const override;
    bool isTexturable(const gl::Context *context) const override;
    bool isYUV() const override;
    bool hasFrontBufferUsage() const override;
    bool isCubeMap() const override;
    bool hasProtectedContent() const override;
    gl::Extents getSize() const override;
    size_t getSamples() const override;
    uint32_t getLevelCount() const override;

    webgpu::ImageHelper *getImage() const override;

  private:
    angle::Result initializeImpl(const egl::Display *display);

    EGLClientBuffer mBuffer = nullptr;
    egl::AttributeMap mAttribs;
    webgpu::ImageHelper mImage;
};

class ImageWgpu : public ImageImpl
{
  public:
    ImageWgpu(const egl::ImageState &state, const gl::Context *context);
    ~ImageWgpu() override;
    egl::Error initialize(const egl::Display *display) override;

    angle::Result orphan(const gl::Context *context, egl::ImageSibling *sibling) override;

    webgpu::ImageHelper *getImage() const { return mImage; }

  private:
    bool mOwnsImage             = false;
    webgpu::ImageHelper *mImage = nullptr;

    const gl::Context *mContext = nullptr;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_IMAGEWGPU_H_
