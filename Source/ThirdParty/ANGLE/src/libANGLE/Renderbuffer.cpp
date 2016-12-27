//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderbuffer.cpp: Implements the renderer-agnostic gl::Renderbuffer class,
// GL renderbuffer objects and related functionality.
// [OpenGL ES 2.0.24] section 4.4.3 page 108.

#include "libANGLE/Renderbuffer.h"

#include "common/utilities.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Image.h"
#include "libANGLE/Texture.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"

namespace gl
{
Renderbuffer::Renderbuffer(rx::RenderbufferImpl *impl, GLuint id)
    : egl::ImageSibling(id),
      mRenderbuffer(impl),
      mLabel(),
      mWidth(0),
      mHeight(0),
      mFormat(GL_RGBA4),
      mSamples(0)
{
}

Renderbuffer::~Renderbuffer()
{
    SafeDelete(mRenderbuffer);
}

void Renderbuffer::setLabel(const std::string &label)
{
    mLabel = label;
}

const std::string &Renderbuffer::getLabel() const
{
    return mLabel;
}

Error Renderbuffer::setStorage(GLenum internalformat, size_t width, size_t height)
{
    orphanImages();

    ANGLE_TRY(mRenderbuffer->setStorage(internalformat, width, height));

    mWidth          = static_cast<GLsizei>(width);
    mHeight         = static_cast<GLsizei>(height);
    mFormat         = Format(internalformat);
    mSamples = 0;

    mDirtyChannel.signal();

    return NoError();
}

Error Renderbuffer::setStorageMultisample(size_t samples, GLenum internalformat, size_t width, size_t height)
{
    orphanImages();

    ANGLE_TRY(mRenderbuffer->setStorageMultisample(samples, internalformat, width, height));

    mWidth          = static_cast<GLsizei>(width);
    mHeight         = static_cast<GLsizei>(height);
    mFormat         = Format(internalformat);
    mSamples        = static_cast<GLsizei>(samples);

    mDirtyChannel.signal();

    return NoError();
}

Error Renderbuffer::setStorageEGLImageTarget(egl::Image *image)
{
    orphanImages();

    ANGLE_TRY(mRenderbuffer->setStorageEGLImageTarget(image));

    setTargetImage(image);

    mWidth          = static_cast<GLsizei>(image->getWidth());
    mHeight         = static_cast<GLsizei>(image->getHeight());
    mFormat         = Format(image->getFormat());
    mSamples        = 0;

    mDirtyChannel.signal();

    return NoError();
}

rx::RenderbufferImpl *Renderbuffer::getImplementation() const
{
    ASSERT(mRenderbuffer);
    return mRenderbuffer;
}

GLsizei Renderbuffer::getWidth() const
{
    return mWidth;
}

GLsizei Renderbuffer::getHeight() const
{
    return mHeight;
}

const Format &Renderbuffer::getFormat() const
{
    return mFormat;
}

GLsizei Renderbuffer::getSamples() const
{
    return mSamples;
}

GLuint Renderbuffer::getRedSize() const
{
    return mFormat.info->redBits;
}

GLuint Renderbuffer::getGreenSize() const
{
    return mFormat.info->greenBits;
}

GLuint Renderbuffer::getBlueSize() const
{
    return mFormat.info->blueBits;
}

GLuint Renderbuffer::getAlphaSize() const
{
    return mFormat.info->alphaBits;
}

GLuint Renderbuffer::getDepthSize() const
{
    return mFormat.info->depthBits;
}

GLuint Renderbuffer::getStencilSize() const
{
    return mFormat.info->stencilBits;
}

void Renderbuffer::onAttach()
{
    addRef();
}

void Renderbuffer::onDetach()
{
    release();
}

GLuint Renderbuffer::getId() const
{
    return id();
}

Extents Renderbuffer::getAttachmentSize(const FramebufferAttachment::Target & /*target*/) const
{
    return Extents(mWidth, mHeight, 1);
}
}  // namespace gl
