//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShareGroup.h: Defines the egl::ShareGroup class, representing the collection of contexts in a
// share group.

#ifndef LIBANGLE_SHAREGROUP_H_
#define LIBANGLE_SHAREGROUP_H_

#include <mutex>
#include <type_traits>
#include <vector>

#include "common/FastVector.h"
#include "common/SimpleMutex.h"
#include "libANGLE/Context.h"
#include "libANGLE/ObjectMap.h"

namespace gl
{
class Context;
}  // namespace gl

namespace rx
{
class EGLImplFactory;
class ShareGroupImpl;
}  // namespace rx

namespace egl
{
class ThreadSafeDisplay;

// References a Context, so that it stays alive while this does.  When this drops the last reference
// to a destroyed Context, it frees it (see ContextMap).
class [[nodiscard]] ScopedContextRef final : angle::NonCopyable
{
  public:
    ScopedContextRef() = default;
    // |display| is the one of |context|.  The caller must make sure that |context| is alive, e.g.
    // because it is current, or because it is found in the context map, which is locked.
    ScopedContextRef(ThreadSafeDisplay *display, gl::Context *context)
        : mDisplay(display), mContext(context)
    {
        if (mContext != nullptr)
        {
            ASSERT(mDisplay != nullptr);
            mContext->addRef();
        }
    }
    ~ScopedContextRef();

    gl::Context *get() const { return mContext; }

    // Gives up the reference, without dropping it.
    gl::Context *release()
    {
        gl::Context *context = mContext;
        mDisplay             = nullptr;
        mContext             = nullptr;
        return context;
    }

  private:
    ThreadSafeDisplay *mDisplay = nullptr;
    gl::Context *mContext       = nullptr;
};

// A Context is freed once eglDestroyContext() was called on it, and the last reference to it is
// dropped (see gl::Context::addRef()).  Until then, it stays in the map, so its handle remains
// valid, e.g. while it is still current on a thread.  The map's mutex makes looking up and
// referencing a Context, marking it destroyed, and dropping the last reference to it, atomic with
// respect to each other.
class ContextMap final : public priv::ObjectMap<gl::Context, angle::SimpleMutex>
{
  public:
    ScopedContextRef getContextRef(ThreadSafeDisplay *display, gl::ContextID handle) const
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        auto iter = mObjects.find(handle.value);
        return ScopedContextRef(display, iter != mObjects.end() ? iter->second : nullptr);
    }

    // The caller must hold a reference to |context|, which it then drops with releaseRef().
    void markDestroyed(gl::Context *context)
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        ASSERT(context->getRefCount() > 0);
        context->setIsDestroyed();
    }

    // Drops a reference to |context|.  If that was the last one, and |context| is destroyed, this
    // removes |context| from the map, so that it can no longer be looked up, and returns true: the
    // caller must then free it.
    [[nodiscard]] bool releaseRef(gl::Context *context)
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        context->release();
        if (context->getRefCount() > 0 || !context->isDestroyed())
        {
            return false;
        }
        mObjects.erase(context->id().value);
        return true;
    }

    void invalidateContext(gl::Context *context)
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        // Another thread may have referenced the context (e.g. via getContextRef()) since the
        // caller dropped its reference.  In that case, leave it in the map; it will be
        // invalidated by invalidateUnreferenced() once it is no longer referenced.
        if (!context->isDestroyed() && context->getRefCount() == 0)
        {
            auto iter = mObjects.find(context->id().value);
            if (iter != mObjects.end())
            {
                mInvalidContexts.push_back(iter->second);
                mObjects.erase(iter);
            }
        }
    }

    void invalidateUnreferenced()
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        for (auto iter = mObjects.begin(); iter != mObjects.end();)
        {
            // Besides the threads that have the context current, a thread that is releasing it may
            // still reference it.
            if (iter->second->getRefCount() == 0)
            {
                mInvalidContexts.push_back(iter->second);
                mObjects.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    }

    Error destroyAllInvalidContexts(ThreadSafeDisplay *display);

    template <typename ConditionType>
    void notifyDeviceLost(ConditionType *deviceLost) const
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        if (*deviceLost)
        {
            return;
        }
        for (const auto &pair : this->mObjects)
        {
            pair.second->markContextLost(gl::GraphicsResetStatus::UnknownContextReset);
        }
        *deviceLost = true;
    }

    bool isResetNotificationEnabled() const
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        for (const auto &pair : this->mObjects)
        {
            if (pair.second->isResetNotificationEnabled())
            {
                return true;
            }
        }
        return false;
    }

    void onGPUSwitch() const
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        for (const auto &pair : this->mObjects)
        {
            pair.second->onGPUSwitch();
        }
    }

    template <typename Callback>
    void forEach(Callback &&callback) const
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        for (const auto &pair : this->mObjects)
        {
            callback(pair.second);
        }
    }

  private:
    std::vector<gl::Context *> mInvalidContexts;
};

class UnlockedContextMap final : public priv::ObjectMap<gl::Context, angle::NoOpMutex>
{
  public:
    size_t size() const { return mObjects.size(); }
};

using SharedContextMap = UnlockedContextMap;

class ShareGroupState final : angle::NonCopyable
{
  public:
    ShareGroupState();
    ~ShareGroupState();

    const SharedContextMap &getContexts() const { return mContexts; }
    void addSharedContext(gl::Context *context);
    void removeSharedContext(gl::Context *context);

    bool hasAnyContextWithRobustness() const { return mAnyContextWithRobustness; }
    bool hasAnyContextWithDisplayTextureShareGroup() const
    {
        return mAnyContextWithDisplayTextureShareGroup;
    }

  private:
    // The list of contexts within the share group
    SharedContextMap mContexts;

    // Whether any context in the share group has robustness enabled.  If any context in the share
    // group is robust, any program created in any context of the share group must have robustness
    // enabled.  This is because programs are shared between the share group contexts.
    bool mAnyContextWithRobustness;

    // Whether any context in the share group uses display shared textures.  This functionality is
    // provided by ANGLE_display_texture_share_group and allows textures to be shared between
    // contexts that are not in the same share group.
    bool mAnyContextWithDisplayTextureShareGroup;
};

class ShareGroup final : angle::NonCopyable
{
  public:
    ShareGroup(rx::EGLImplFactory *factory);

    void addRef();

    void release(const egl::ThreadSafeDisplay *display);

    rx::ShareGroupImpl *getImplementation() const { return mImplementation; }

    rx::UniqueSerial generateFramebufferSerial() { return mFramebufferSerialFactory.generate(); }

    angle::FrameCaptureShared *getFrameCaptureShared() { return mFrameCaptureShared.get(); }

    void finishAllContexts();

    const SharedContextMap &getContexts() const { return mState.getContexts(); }
    void addSharedContext(gl::Context *context);
    void removeSharedContext(gl::Context *context);

  protected:
    ~ShareGroup();

  private:
    size_t mRefCount;
    rx::ShareGroupImpl *mImplementation;
    rx::UniqueSerialFactory mFramebufferSerialFactory;

    // Note: we use a raw pointer here so we can exclude frame capture sources from the build.
    std::unique_ptr<angle::FrameCaptureShared> mFrameCaptureShared;

    ShareGroupState mState;
};

}  // namespace egl

#endif  // LIBANGLE_SHAREGROUP_H_
