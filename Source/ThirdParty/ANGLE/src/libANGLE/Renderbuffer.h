//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderbuffer.h: Defines the renderer-agnostic container class gl::Renderbuffer.
// Implements GL renderbuffer objects and related functionality.
// [OpenGL ES 2.0.24] section 4.4.3 page 108.

#ifndef LIBANGLE_RENDERBUFFER_H_
#define LIBANGLE_RENDERBUFFER_H_

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Image.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/RenderbufferImpl.h"

namespace gl
{
// A GL renderbuffer object is usually used as a depth or stencil buffer attachment
// for a framebuffer object. The renderbuffer itself is a distinct GL object, see
// FramebufferAttachment and Framebuffer for how they are applied to an FBO via an
// attachment point.

class Renderbuffer final : public egl::ImageSibling,
                           public gl::FramebufferAttachmentObject,
                           public LabeledObject
{
  public:
    Renderbuffer(rx::RenderbufferImpl *impl, GLuint id);
    virtual ~Renderbuffer();

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    Error setStorage(GLenum internalformat, size_t width, size_t height);
    Error setStorageMultisample(size_t samples, GLenum internalformat, size_t width, size_t height);
    Error setStorageEGLImageTarget(egl::Image *imageTarget);

    rx::RenderbufferImpl *getImplementation() const;

    GLsizei getWidth() const;
    GLsizei getHeight() const;
    const Format &getFormat() const;
    GLsizei getSamples() const;
    GLuint getRedSize() const;
    GLuint getGreenSize() const;
    GLuint getBlueSize() const;
    GLuint getAlphaSize() const;
    GLuint getDepthSize() const;
    GLuint getStencilSize() const;

    // FramebufferAttachmentObject Impl
    Extents getAttachmentSize(const FramebufferAttachment::Target &target) const override;
    const Format &getAttachmentFormat(
        const FramebufferAttachment::Target & /*target*/) const override
    {
        return getFormat();
    }
    GLsizei getAttachmentSamples(const FramebufferAttachment::Target &/*target*/) const override { return getSamples(); }

    void onAttach() override;
    void onDetach() override;
    GLuint getId() const override;

  private:
    rx::FramebufferAttachmentObjectImpl *getAttachmentImpl() const override { return mRenderbuffer; }

    rx::RenderbufferImpl *mRenderbuffer;

    std::string mLabel;

    GLsizei mWidth;
    GLsizei mHeight;
    Format mFormat;
    GLsizei mSamples;
};

}

#endif   // LIBANGLE_RENDERBUFFER_H_
