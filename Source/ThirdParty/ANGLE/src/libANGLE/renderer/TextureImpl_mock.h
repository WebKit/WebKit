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
                 gl::Error(ContextImpl *,
                           GLenum,
                           size_t,
                           GLenum,
                           const gl::Extents &,
                           GLenum,
                           GLenum,
                           const gl::PixelUnpackState &,
                           const uint8_t *));
    MOCK_METHOD8(setSubImage,
                 gl::Error(ContextImpl *,
                           GLenum,
                           size_t,
                           const gl::Box &,
                           GLenum,
                           GLenum,
                           const gl::PixelUnpackState &,
                           const uint8_t *));
    MOCK_METHOD8(setCompressedImage,
                 gl::Error(ContextImpl *,
                           GLenum,
                           size_t,
                           GLenum,
                           const gl::Extents &,
                           const gl::PixelUnpackState &,
                           size_t,
                           const uint8_t *));
    MOCK_METHOD8(setCompressedSubImage,
                 gl::Error(ContextImpl *,
                           GLenum,
                           size_t,
                           const gl::Box &,
                           GLenum,
                           const gl::PixelUnpackState &,
                           size_t,
                           const uint8_t *));
    MOCK_METHOD6(copyImage,
                 gl::Error(ContextImpl *,
                           GLenum,
                           size_t,
                           const gl::Rectangle &,
                           GLenum,
                           const gl::Framebuffer *));
    MOCK_METHOD6(copySubImage,
                 gl::Error(ContextImpl *,
                           GLenum,
                           size_t,
                           const gl::Offset &,
                           const gl::Rectangle &,
                           const gl::Framebuffer *));
    MOCK_METHOD10(copyTexture,
                  gl::Error(ContextImpl *,
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
                  gl::Error(ContextImpl *,
                            GLenum,
                            size_t,
                            const gl::Offset &,
                            size_t,
                            const gl::Rectangle &,
                            bool,
                            bool,
                            bool,
                            const gl::Texture *));
    MOCK_METHOD2(copyCompressedTexture, gl::Error(ContextImpl *, const gl::Texture *source));
    MOCK_METHOD5(setStorage, gl::Error(ContextImpl *, GLenum, size_t, GLenum, const gl::Extents &));
    MOCK_METHOD3(setImageExternal,
                 gl::Error(GLenum, egl::Stream *, const egl::Stream::GLTextureDescription &));
    MOCK_METHOD2(setEGLImageTarget, gl::Error(GLenum, egl::Image *));
    MOCK_METHOD1(generateMipmap, gl::Error(ContextImpl *));
    MOCK_METHOD1(bindTexImage, void(egl::Surface *));
    MOCK_METHOD0(releaseTexImage, void(void));

    MOCK_METHOD2(getAttachmentRenderTarget, gl::Error(const gl::FramebufferAttachment::Target &, FramebufferAttachmentRenderTarget **));

    MOCK_METHOD6(setStorageMultisample,
                 gl::Error(ContextImpl *, GLenum, GLsizei, GLint, const gl::Extents &, GLboolean));

    MOCK_METHOD1(setBaseLevel, void(GLuint));

    MOCK_METHOD1(syncState, void(const gl::Texture::DirtyBits &));

    MOCK_METHOD0(destructor, void());

  protected:
    gl::TextureState mMockState;
};

}

#endif // LIBANGLE_RENDERER_TEXTUREIMPLMOCK_H_
