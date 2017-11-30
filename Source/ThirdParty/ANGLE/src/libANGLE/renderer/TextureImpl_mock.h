//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureImpl_mock.h: Defines a mock of the TextureImpl class.

#ifndef LIBANGLE_RENDERER_TEXTUREIMPLMOCK_H_
#define LIBANGLE_RENDERER_TEXTUREIMPLMOCK_H_

#include "gmock/gmock.h"

#include "libANGLE/renderer/TextureImpl.h"

namespace rx
{

class MockTextureImpl : public TextureImpl
{
  public:
    MockTextureImpl() : TextureImpl(mMockState), mMockState(GL_TEXTURE_2D) {}
    virtual ~MockTextureImpl() { destructor(); }
    MOCK_METHOD9(setImage,
                 gl::Error(const gl::Context *,
                           GLenum,
                           size_t,
                           GLenum,
                           const gl::Extents &,
                           GLenum,
                           GLenum,
                           const gl::PixelUnpackState &,
                           const uint8_t *));
    MOCK_METHOD8(setSubImage,
                 gl::Error(const gl::Context *,
                           GLenum,
                           size_t,
                           const gl::Box &,
                           GLenum,
                           GLenum,
                           const gl::PixelUnpackState &,
                           const uint8_t *));
    MOCK_METHOD8(setCompressedImage,
                 gl::Error(const gl::Context *,
                           GLenum,
                           size_t,
                           GLenum,
                           const gl::Extents &,
                           const gl::PixelUnpackState &,
                           size_t,
                           const uint8_t *));
    MOCK_METHOD8(setCompressedSubImage,
                 gl::Error(const gl::Context *,
                           GLenum,
                           size_t,
                           const gl::Box &,
                           GLenum,
                           const gl::PixelUnpackState &,
                           size_t,
                           const uint8_t *));
    MOCK_METHOD6(copyImage,
                 gl::Error(const gl::Context *,
                           GLenum,
                           size_t,
                           const gl::Rectangle &,
                           GLenum,
                           const gl::Framebuffer *));
    MOCK_METHOD6(copySubImage,
                 gl::Error(const gl::Context *,
                           GLenum,
                           size_t,
                           const gl::Offset &,
                           const gl::Rectangle &,
                           const gl::Framebuffer *));
    MOCK_METHOD10(copyTexture,
                  gl::Error(const gl::Context *,
                            GLenum,
                            size_t,
                            GLenum,
                            GLenum,
                            size_t,
                            bool,
                            bool,
                            bool,
                            const gl::Texture *));
    MOCK_METHOD10(copySubTexture,
                  gl::Error(const gl::Context *,
                            GLenum,
                            size_t,
                            const gl::Offset &,
                            size_t,
                            const gl::Rectangle &,
                            bool,
                            bool,
                            bool,
                            const gl::Texture *));
    MOCK_METHOD2(copyCompressedTexture, gl::Error(const gl::Context *, const gl::Texture *source));
    MOCK_METHOD5(setStorage,
                 gl::Error(const gl::Context *, GLenum, size_t, GLenum, const gl::Extents &));
    MOCK_METHOD4(setImageExternal,
                 gl::Error(const gl::Context *,
                           GLenum,
                           egl::Stream *,
                           const egl::Stream::GLTextureDescription &));
    MOCK_METHOD3(setEGLImageTarget, gl::Error(const gl::Context *, GLenum, egl::Image *));
    MOCK_METHOD1(generateMipmap, gl::Error(const gl::Context *));
    MOCK_METHOD2(bindTexImage, gl::Error(const gl::Context *, egl::Surface *));
    MOCK_METHOD1(releaseTexImage, gl::Error(const gl::Context *));

    MOCK_METHOD4(getAttachmentRenderTarget,
                 gl::Error(const gl::Context *,
                           GLenum,
                           const gl::ImageIndex &,
                           FramebufferAttachmentRenderTarget **));

    MOCK_METHOD6(setStorageMultisample,
                 gl::Error(const gl::Context *, GLenum, GLsizei, GLint, const gl::Extents &, bool));

    MOCK_METHOD2(setBaseLevel, gl::Error(const gl::Context *, GLuint));

    MOCK_METHOD1(syncState, void(const gl::Texture::DirtyBits &));

    MOCK_METHOD0(destructor, void());

  protected:
    gl::TextureState mMockState;
};

}

#endif // LIBANGLE_RENDERER_TEXTUREIMPLMOCK_H_
