//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayImpl.cpp: Implementation methods of egl::Display

#include "libANGLE/renderer/DisplayImpl.h"

#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"

namespace rx
{

DisplayImpl::DisplayImpl(const egl::DisplayState &state)
    : mState(state), mExtensionsInitialized(false), mCapsInitialized(false), mBlobCache(nullptr)
{}

DisplayImpl::~DisplayImpl()
{
    ASSERT(mState.surfaceSet.empty());
}

egl::Error DisplayImpl::prepareForCall()
{
    return egl::NoError();
}

egl::Error DisplayImpl::releaseThread()
{
    return egl::NoError();
}

const egl::DisplayExtensions &DisplayImpl::getExtensions() const
{
    if (!mExtensionsInitialized)
    {
        generateExtensions(&mExtensions);
        mExtensionsInitialized = true;
    }

    return mExtensions;
}

egl::Error DisplayImpl::handleGPUSwitch()
{
    return egl::NoError();
}

egl::Error DisplayImpl::validateClientBuffer(const egl::Config *configuration,
                                             EGLenum buftype,
                                             EGLClientBuffer clientBuffer,
                                             const egl::AttributeMap &attribs) const
{
    UNREACHABLE();
    return egl::EglBadDisplay() << "DisplayImpl::validateClientBuffer unimplemented.";
}

egl::Error DisplayImpl::validateImageClientBuffer(const gl::Context *context,
                                                  EGLenum target,
                                                  EGLClientBuffer clientBuffer,
                                                  const egl::AttributeMap &attribs) const
{
    UNREACHABLE();
    return egl::EglBadDisplay() << "DisplayImpl::validateImageClientBuffer unimplemented.";
}

egl::Error DisplayImpl::validatePixmap(egl::Config *config,
                                       EGLNativePixmapType pixmap,
                                       const egl::AttributeMap &attributes) const
{
    UNREACHABLE();
    return egl::EglBadDisplay() << "DisplayImpl::valdiatePixmap unimplemented.";
}

const egl::Caps &DisplayImpl::getCaps() const
{
    if (!mCapsInitialized)
    {
        generateCaps(&mCaps);
        mCapsInitialized = true;
    }

    return mCaps;
}

}  // namespace rx
