//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGL.h: Defines the class interface for DisplayGL.

#ifndef LIBANGLE_RENDERER_GL_DISPLAYGL_H_
#define LIBANGLE_RENDERER_GL_DISPLAYGL_H_

#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"

namespace egl
{
class Surface;
}

namespace rx
{

class RendererGL;

class DisplayGL : public DisplayImpl
{
  public:
    DisplayGL(const egl::DisplayState &state);
    ~DisplayGL() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    ImageImpl *createImage(EGLenum target,
                           egl::ImageSibling *buffer,
                           const egl::AttributeMap &attribs) override;

    ContextImpl *createContext(const gl::ContextState &state) override;

    StreamProducerImpl *createStreamProducerD3DTextureNV12(
        egl::Stream::ConsumerType consumerType,
        const egl::AttributeMap &attribs) override;

    egl::Error makeCurrent(egl::Surface *drawSurface, egl::Surface *readSurface, gl::Context *context) override;

    gl::Version getMaxSupportedESVersion() const override;

  protected:
    RendererGL *getRenderer() const { return mRenderer; };

  private:
    virtual const FunctionsGL *getFunctionsGL() const = 0;
    virtual egl::Error makeCurrentSurfaceless(gl::Context *context);

    RendererGL *mRenderer;

    egl::Surface *mCurrentDrawSurface;
};

}

#endif // LIBANGLE_RENDERER_GL_DISPLAYGL_H_
