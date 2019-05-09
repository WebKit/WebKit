//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferGL.cpp: Implements the class methods for RenderbufferGL.

#include "libANGLE/renderer/gl/RenderbufferGL.h"

#include "common/debug.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/gl/BlitGL.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/ImageGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/formatutilsgl.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace rx
{
RenderbufferGL::RenderbufferGL(const gl::RenderbufferState &state,
                               const FunctionsGL *functions,
                               const WorkaroundsGL &workarounds,
                               StateManagerGL *stateManager,
                               BlitGL *blitter,
                               const gl::TextureCapsMap &textureCaps)
    : RenderbufferImpl(state),
      mFunctions(functions),
      mWorkarounds(workarounds),
      mStateManager(stateManager),
      mBlitter(blitter),
      mTextureCaps(textureCaps),
      mRenderbufferID(0)
{
    mFunctions->genRenderbuffers(1, &mRenderbufferID);
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mRenderbufferID);
}

RenderbufferGL::~RenderbufferGL()
{
    mStateManager->deleteRenderbuffer(mRenderbufferID);
    mRenderbufferID = 0;
}

angle::Result RenderbufferGL::setStorage(const gl::Context *context,
                                         GLenum internalformat,
                                         size_t width,
                                         size_t height)
{
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mRenderbufferID);

    nativegl::RenderbufferFormat renderbufferFormat =
        nativegl::GetRenderbufferFormat(mFunctions, mWorkarounds, internalformat);
    mFunctions->renderbufferStorage(GL_RENDERBUFFER, renderbufferFormat.internalFormat,
                                    static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    mNativeInternalFormat = renderbufferFormat.internalFormat;

    return angle::Result::Continue;
}

angle::Result RenderbufferGL::setStorageMultisample(const gl::Context *context,
                                                    size_t samples,
                                                    GLenum internalformat,
                                                    size_t width,
                                                    size_t height)
{
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mRenderbufferID);

    nativegl::RenderbufferFormat renderbufferFormat =
        nativegl::GetRenderbufferFormat(mFunctions, mWorkarounds, internalformat);
    mFunctions->renderbufferStorageMultisample(
        GL_RENDERBUFFER, static_cast<GLsizei>(samples), renderbufferFormat.internalFormat,
        static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    const gl::TextureCaps &formatCaps = mTextureCaps.get(internalformat);
    if (samples > formatCaps.getMaxSamples())
    {
        // Before version 4.2, it is unknown if the specific internal format can support the
        // requested number of samples.  It is expected that GL_OUT_OF_MEMORY is returned if the
        // renderbuffer cannot be created.
        GLenum error = GL_NO_ERROR;
        do
        {
            error = mFunctions->getError();
            ANGLE_CHECK_GL_ALLOC(GetImplAs<ContextGL>(context), error != GL_OUT_OF_MEMORY);
            ASSERT(error == GL_NO_ERROR);
        } while (error != GL_NO_ERROR);
    }

    mNativeInternalFormat = renderbufferFormat.internalFormat;

    return angle::Result::Continue;
}

angle::Result RenderbufferGL::setStorageEGLImageTarget(const gl::Context *context,
                                                       egl::Image *image)
{
    ImageGL *imageGL = GetImplAs<ImageGL>(image);
    return imageGL->setRenderbufferStorage(context, this, &mNativeInternalFormat);
}

GLuint RenderbufferGL::getRenderbufferID() const
{
    return mRenderbufferID;
}

angle::Result RenderbufferGL::initializeContents(const gl::Context *context,
                                                 const gl::ImageIndex &imageIndex)
{
    return mBlitter->clearRenderbuffer(this, mNativeInternalFormat);
}

GLenum RenderbufferGL::getNativeInternalFormat() const
{
    return mNativeInternalFormat;
}

}  // namespace rx
