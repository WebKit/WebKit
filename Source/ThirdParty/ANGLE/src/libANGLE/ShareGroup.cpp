//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShareGroup.h: Defines the egl::ShareGroup class, representing the collection of contexts in a
// share group.

#include "libANGLE/ShareGroup.h"

#include <algorithm>
#include <iterator>

#include <EGL/eglext.h>
#include <platform/PlatformMethods.h>

#include "common/debug.h"
#include "common/platform_helpers.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/capture/FrameCapture.h"
#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/ShareGroupImpl.h"

namespace egl
{
// ScopedContextRef
ScopedContextRef::~ScopedContextRef()
{
    if (mContext != nullptr)
    {
        Error error = mDisplay->releaseContextRef(mContext);
        if (error.isError())
        {
            ERR() << "Failed to destroy an unreferenced context: " << error;
        }
    }
}

// ShareGroupState
ShareGroupState::ShareGroupState()
    : mAnyContextWithRobustness(false), mAnyContextWithDisplayTextureShareGroup(false)
{}
ShareGroupState::~ShareGroupState() = default;

void ShareGroupState::addSharedContext(gl::Context *context)
{
    mContexts.insert(context->id(), context);

    if (context->isRobustnessEnabled())
    {
        mAnyContextWithRobustness = true;
    }
    if (context->getState().hasDisplayTextureShareGroup())
    {
        mAnyContextWithDisplayTextureShareGroup = true;
    }
}

void ShareGroupState::removeSharedContext(gl::Context *context)
{
    mContexts.erase(context->id());
}

// ShareGroup
ShareGroup::ShareGroup(rx::EGLImplFactory *factory)
    : mRefCount(1),
      mImplementation(factory->createShareGroup(mState)),
      mFrameCaptureShared(new angle::FrameCaptureShared)
{}

ShareGroup::~ShareGroup()
{
    SafeDelete(mImplementation);
}

void ShareGroup::addRef()
{
    // This is protected by global lock, so no atomic is required
    mRefCount++;
}

void ShareGroup::release(const ThreadSafeDisplay *display)
{
    if (--mRefCount == 0)
    {
        if (mImplementation)
        {
            mImplementation->onDestroy(display);
        }
        delete this;
    }
}

void ShareGroup::finishAllContexts()
{
    mState.getContexts().forEach([](gl::Context *shareContext) {
        if (shareContext->hasBeenCurrent() && !shareContext->isDestroyed())
        {
            shareContext->finish();
        }
    });
}

void ShareGroup::addSharedContext(gl::Context *context)
{
    mState.addSharedContext(context);
    mImplementation->onContextAdd();
}

void ShareGroup::removeSharedContext(gl::Context *context)
{
    mState.removeSharedContext(context);
}

// ContextMap
Error ContextMap::destroyAllInvalidContexts(ThreadSafeDisplay *display)
{
    std::vector<gl::Context *> invalidContexts;
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        invalidContexts = std::move(mInvalidContexts);
    }

    for (gl::Context *context : invalidContexts)
    {
        ASSERT(context->getRefCount() == 0);
        // eglReleaseThread() may call to this method when there are still Contexts, that may
        // potentially access shared state of the "context".
        // Need AddRefLock because there may be ContextMutex destruction.
        ScopedContextMutexAddRefLock lock(context->getContextMutex());
        context->setIsDestroyed();
        ANGLE_TRY(display->destroyContextImpl(context));
    }

    return NoError();
}
}  // namespace egl
