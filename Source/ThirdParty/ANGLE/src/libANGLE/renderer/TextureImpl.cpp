//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureImpl.cpp: Defines the abstract rx::TextureImpl classes.

#include "libANGLE/renderer/TextureImpl.h"

namespace rx
{

TextureImpl::TextureImpl(const gl::TextureState &state) : mState(state)
{
}

TextureImpl::~TextureImpl()
{
}

gl::Error TextureImpl::copyTexture(GLenum internalFormat,
                                   GLenum type,
                                   bool unpackFlipY,
                                   bool unpackPremultiplyAlpha,
                                   bool unpackUnmultiplyAlpha,
                                   const gl::Texture *source)
{
    UNREACHABLE();
    return gl::Error(GL_INVALID_OPERATION, "CHROMIUM_copy_texture exposed but not implemented.");
}

gl::Error TextureImpl::copySubTexture(const gl::Offset &destOffset,
                                      const gl::Rectangle &sourceArea,
                                      bool unpackFlipY,
                                      bool unpackPremultiplyAlpha,
                                      bool unpackUnmultiplyAlpha,
                                      const gl::Texture *source)
{
    UNREACHABLE();
    return gl::Error(GL_INVALID_OPERATION, "CHROMIUM_copy_texture exposed but not implemented.");
}

gl::Error TextureImpl::copyCompressedTexture(const gl::Texture *source)
{
    UNREACHABLE();
    return gl::Error(GL_INVALID_OPERATION,
                     "CHROMIUM_copy_compressed_texture exposed but not implemented.");
}
}
