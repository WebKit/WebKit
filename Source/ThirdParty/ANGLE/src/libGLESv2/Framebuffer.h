//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.h: Defines the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#ifndef LIBGLESV2_FRAMEBUFFER_H_
#define LIBGLESV2_FRAMEBUFFER_H_

#define GL_APICALL
#include <GLES2/gl2.h>
#include <d3d9.h>

#include "common/angleutils.h"
#include "common/RefCountObject.h"

namespace gl
{
class Renderbuffer;
class Colorbuffer;
class Depthbuffer;
class Stencilbuffer;
class DepthStencilbuffer;

class Framebuffer
{
  public:
    Framebuffer();

    virtual ~Framebuffer();

    void setColorbuffer(GLenum type, GLuint colorbuffer);
    void setDepthbuffer(GLenum type, GLuint depthbuffer);
    void setStencilbuffer(GLenum type, GLuint stencilbuffer);

    void detachTexture(GLuint texture);
    void detachRenderbuffer(GLuint renderbuffer);

    IDirect3DSurface9 *getRenderTarget();
    IDirect3DSurface9 *getDepthStencil();

    unsigned int getRenderTargetSerial();
    unsigned int getDepthbufferSerial();
    unsigned int getStencilbufferSerial();

    Renderbuffer *getColorbuffer();
    Renderbuffer *getDepthbuffer();
    Renderbuffer *getStencilbuffer();

    GLenum getColorbufferType();
    GLenum getDepthbufferType();
    GLenum getStencilbufferType();

    GLuint getColorbufferHandle();
    GLuint getDepthbufferHandle();
    GLuint getStencilbufferHandle();

    bool hasStencil();
    int getSamples();

    virtual GLenum completeness();

  protected:
    GLenum mColorbufferType;
    BindingPointer<Renderbuffer> mColorbufferPointer;

    GLenum mDepthbufferType;
    BindingPointer<Renderbuffer> mDepthbufferPointer;

    GLenum mStencilbufferType;
    BindingPointer<Renderbuffer> mStencilbufferPointer;

  private:
    DISALLOW_COPY_AND_ASSIGN(Framebuffer);

    Renderbuffer *lookupRenderbuffer(GLenum type, GLuint handle) const;
};

class DefaultFramebuffer : public Framebuffer
{
  public:
    DefaultFramebuffer(Colorbuffer *colorbuffer, DepthStencilbuffer *depthStencil);

    virtual GLenum completeness();

  private:
    DISALLOW_COPY_AND_ASSIGN(DefaultFramebuffer);
};

}

#endif   // LIBGLESV2_FRAMEBUFFER_H_
