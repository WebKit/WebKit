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

    virtual int getWidth() const;
    virtual int getHeight() const;
    virtual GLenum getFormat() const;
    virtual bool isFloatingPoint() const;
    D3DFORMAT getD3DFormat() const;
    GLsizei getSamples() const;
    unsigned int getSerial() const;

    static unsigned int issueSerial();

  protected:
    void setSize(int width, int height);
    GLenum mFormat;
    D3DFORMAT mD3DFormat;
    GLsizei mSamples;
    const unsigned int mSerial;

  private:
    DISALLOW_COPY_AND_ASSIGN(RenderbufferStorage);

    static unsigned int mCurrentSerial;

    int mWidth;
    int mHeight;
};

// Renderbuffer implements the GL renderbuffer object.
// It's only a wrapper for a RenderbufferStorage, but the internal object
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

    int getWidth() const;
    int getHeight() const;
    GLenum getFormat() const;
    D3DFORMAT getD3DFormat() const;
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
    explicit Colorbuffer(const Texture* texture);
    Colorbuffer(int width, int height, GLenum format, GLsizei samples);

    ~Colorbuffer();

    bool isColorbuffer() const;

    GLuint getRedSize() const;
    GLuint getGreenSize() const;
    GLuint getBlueSize() const;
    GLuint getAlphaSize() const;

    IDirect3DSurface9 *getRenderTarget();

  protected:
    IDirect3DSurface9 *mRenderTarget;

  private:
    DISALLOW_COPY_AND_ASSIGN(Colorbuffer);
};

class DepthStencilbuffer : public RenderbufferStorage
{
  public:
    explicit DepthStencilbuffer(IDirect3DSurface9 *depthStencil);
    DepthStencilbuffer(int width, int height, GLsizei samples);

    ~DepthStencilbuffer();

    virtual bool isDepthbuffer() const;
    virtual bool isStencilbuffer() const;

    GLuint getDepthSize() const;
    GLuint getStencilSize() const;

    IDirect3DSurface9 *getDepthStencil();

  private:
    DISALLOW_COPY_AND_ASSIGN(DepthStencilbuffer);
    IDirect3DSurface9 *mDepthStencil;
};

class Depthbuffer : public DepthStencilbuffer
{
  public:
    explicit Depthbuffer(IDirect3DSurface9 *depthStencil);
    Depthbuffer(int width, int height, GLsizei samples);

    ~Depthbuffer();

    bool isDepthbuffer() const;
    bool isStencilbuffer() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Depthbuffer);
};

class Stencilbuffer : public DepthStencilbuffer
{
  public:
    explicit Stencilbuffer(IDirect3DSurface9 *depthStencil);
    Stencilbuffer(int width, int height, GLsizei samples);

    ~Stencilbuffer();

    bool isDepthbuffer() const;
    bool isStencilbuffer() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Stencilbuffer);
};
}

#endif   // LIBGLESV2_RENDERBUFFER_H_
