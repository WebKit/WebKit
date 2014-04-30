//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderTarget.h: Defines an abstract wrapper class to manage IDirect3DSurface9
// and ID3D11View objects belonging to renderbuffers.

#ifndef LIBGLESV2_RENDERER_RENDERTARGET_H_
#define LIBGLESV2_RENDERER_RENDERTARGET_H_

#include "common/angleutils.h"
#include "libGLESv2/angletypes.h"

namespace rx
{
class RenderTarget
{
  public:
    RenderTarget()
    {
        mWidth = 0;
        mHeight = 0;
        mDepth = 0;
        mInternalFormat = GL_NONE;
        mActualFormat = GL_NONE;
        mSamples = 0;
    }

    virtual ~RenderTarget() {};

    GLsizei getWidth() const { return mWidth; }
    GLsizei getHeight() const { return mHeight; }
    GLsizei getDepth() const { return mDepth; }
    GLenum getInternalFormat() const { return mInternalFormat; }
    GLenum getActualFormat() const { return mActualFormat; }
    GLsizei getSamples() const { return mSamples; }
    gl::Extents getExtents() const { return gl::Extents(mWidth, mHeight, mDepth); }

    virtual void invalidate(GLint x, GLint y, GLsizei width, GLsizei height) = 0;

    struct Desc {
        GLsizei width;
        GLsizei height;
        GLsizei depth;
        GLenum  format;
    };

  protected:
    GLsizei mWidth;
    GLsizei mHeight;
    GLsizei mDepth;
    GLenum mInternalFormat;
    GLenum mActualFormat;
    GLsizei mSamples;

  private:
    DISALLOW_COPY_AND_ASSIGN(RenderTarget);
};

}

#endif // LIBGLESV2_RENDERTARGET_H_
