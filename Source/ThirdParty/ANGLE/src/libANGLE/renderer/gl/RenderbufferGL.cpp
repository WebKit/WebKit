//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferGL.cpp: Implements the class methods for RenderbufferGL.

#include "libANGLE/renderer/gl/RenderbufferGL.h"

#include "common/debug.h"
#include "libANGLE/Caps.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace rx
{

RenderbufferGL::RenderbufferGL(const FunctionsGL *functions, StateManagerGL *stateManager, const gl::TextureCapsMap &textureCaps)
    : RenderbufferImpl(),
      mFunctions(functions),
      mStateManager(stateManager),
      mTextureCaps(textureCaps),
      mRenderbufferID(0)
{
    mFunctions->genRenderbuffers(1, &mRenderbufferID);
}

RenderbufferGL::~RenderbufferGL()
{
    if (mRenderbufferID != 0)
    {
        mFunctions->deleteRenderbuffers(1, &mRenderbufferID);
        mRenderbufferID = 0;
    }
}

gl::Error RenderbufferGL::setStorage(GLenum internalformat, size_t width, size_t height)
{
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mRenderbufferID);
    mFunctions->renderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);
    return gl::Error(GL_NO_ERROR);
}

gl::Error RenderbufferGL::setStorageMultisample(size_t samples, GLenum internalformat, size_t width, size_t height)
{
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mRenderbufferID);
    mFunctions->renderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalformat, width, height);

    const gl::TextureCaps &formatCaps = mTextureCaps.get(internalformat);
    if (samples > formatCaps.getMaxSamples())
    {
        // Before version 4.2, it is unknown if the specific internal format can support the requested number
        // of samples.  It is expected that GL_OUT_OF_MEMORY is returned if the renderbuffer cannot be created.
        GLenum error = GL_NO_ERROR;
        do
        {
            error = mFunctions->getError();
            if (error == GL_OUT_OF_MEMORY)
            {
                return gl::Error(GL_OUT_OF_MEMORY);
            }

            ASSERT(error == GL_NO_ERROR);
        } while (error != GL_NO_ERROR);
    }

    return gl::Error(GL_NO_ERROR);
}

GLuint RenderbufferGL::getRenderbufferID() const
{
    return mRenderbufferID;
}

}
