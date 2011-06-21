//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderbuffer.h: Defines the wrapper class gl::Renderbuffer, as well as the
// class hierarchy used to store its contents: RenderbufferStorage, Colorbuffer,
// DepthStencilbuffer, Depthbuffer and Stencilbuffer. Implements GL renderbuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4.3 page 108.

#ifndef LIBGLESV2_RENDERBUFFER_H_
#define LIBGLESV2_RENDERBUFFER_H_

#define GL_APICALL
#include <GLES2/gl2.h>
#include <d3d9.h>

#include "common/angleutils.h"
#include "libGLESv2/RefCountObject.h"

namespace gl
{
class Texture;

// A class derived from RenderbufferStorage is created whenever glRenderbufferStorage
// is called. The specific concrete type depends on whether the internal format is
// colour depth, stencil or packed depth/stencil.
class RenderbufferStorage
{
  public:
    RenderbufferStorage();

    virtual ~RenderbufferStorage() = 0;

    virtual bool isColorbuffer() const;
    virtual bool isDepthbuffer() const;
    virtual bool isStencilbuffer() const;

    virtual IDirect3DSurface9 *getRenderTarget();
    virtual IDirect3DSurface9 *getDepthStencil();

    virtual GLsizei getWidth() const;
    virtual GLsizei getHeight() const;
    virtual GLenum getInternalFormat() const;
    GLuint getRedSize() const;
    GLuint getGreenSize() const;
    GLuint getBlueSize() const;
    GLuint getAlphaSize() const;
    GLuint getDepthSize() const;
    GLuint getStencilSize() const;
    virtual GLsizei getSamples() const;

    virtual D3DFORMAT getD3DFormat() const;

    unsigned int getSerial() const;

  protected:
    GLsizei mWidth;
    GLsizei mHeight;
    GLenum mInternalFormat;
    D3DFORMAT mD3DFormat;
    GLsizei mSamples;

  private:
    DISALLOW_COPY_AND_ASSIGN(RenderbufferStorage);

    static unsigned int issueSerial();

    const unsigned int mSerial;

    static unsigned int mCurrentSerial;
};

// Renderbuffer implements the GL renderbuffer object.
// It's only a proxy for a RenderbufferStorage instance; the internal object
// can change whenever glRenderbufferStorage is called.
class Renderbuffer : public RefCountObject
{
  public:
    Renderbuffer(GLuint id, RenderbufferStorage *storage);

    ~Renderbuffer();

    bool isColorbuffer() const;
    bool isDepthbuffer() const;
    bool isStencilbuffer() const;

    IDirect3DSurface9 *getRenderTarget();
    IDirect3DSurface9 *getDepthStencil();

    GLsizei getWidth() const;
    GLsizei getHeight() const;
    GLenum getInternalFormat() const;
    D3DFORMAT getD3DFormat() const;
    GLuint getRedSize() const;
    GLuint getGreenSize() const;
    GLuint getBlueSize() const;
    GLuint getAlphaSize() const;
    GLuint getDepthSize() const;
    GLuint getStencilSize() const;
    GLsizei getSamples() const;

    unsigned int getSerial() const;

    void setStorage(RenderbufferStorage *newStorage);
    RenderbufferStorage *getStorage() { return mStorage; }

  private:
    DISALLOW_COPY_AND_ASSIGN(Renderbuffer);

    RenderbufferStorage *mStorage;
};

class Colorbuffer : public RenderbufferStorage
{
  public:
    explicit Colorbuffer(IDirect3DSurface9 *renderTarget);
    Colorbuffer(Texture *texture, GLenum target);
    Colorbuffer(GLsizei width, GLsizei height, GLenum format, GLsizei samples);

    virtual ~Colorbuffer();

    virtual bool isColorbuffer() const;

    virtual IDirect3DSurface9 *getRenderTarget();

    virtual GLsizei getWidth() const;
    virtual GLsizei getHeight() const;
    virtual GLenum getInternalFormat() const;
    virtual GLenum getType() const;

    virtual D3DFORMAT getD3DFormat() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Colorbuffer);

    IDirect3DSurface9 *mRenderTarget;
    Texture *mTexture;
    GLenum mTarget;
};

class DepthStencilbuffer : public RenderbufferStorage
{
  public:
    explicit DepthStencilbuffer(IDirect3DSurface9 *depthStencil);
    DepthStencilbuffer(GLsizei width, GLsizei height, GLsizei samples);

    ~DepthStencilbuffer();

    virtual bool isDepthbuffer() const;
    virtual bool isStencilbuffer() const;

    virtual IDirect3DSurface9 *getDepthStencil();

  private:
    DISALLOW_COPY_AND_ASSIGN(DepthStencilbuffer);
    IDirect3DSurface9 *mDepthStencil;
};

class Depthbuffer : public DepthStencilbuffer
{
  public:
    explicit Depthbuffer(IDirect3DSurface9 *depthStencil);
    Depthbuffer(GLsizei width, GLsizei height, GLsizei samples);

    virtual ~Depthbuffer();

    virtual bool isDepthbuffer() const;
    virtual bool isStencilbuffer() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Depthbuffer);
};

class Stencilbuffer : public DepthStencilbuffer
{
  public:
    explicit Stencilbuffer(IDirect3DSurface9 *depthStencil);
    Stencilbuffer(GLsizei width, GLsizei height, GLsizei samples);

    virtual ~Stencilbuffer();

    virtual bool isDepthbuffer() const;
    virtual bool isStencilbuffer() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Stencilbuffer);
};
}

#endif   // LIBGLESV2_RENDERBUFFER_H_
