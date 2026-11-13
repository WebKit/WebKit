//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLSync.h: Defines the egl::Sync classes, which support the EGL_KHR_fence_sync,
// EGL_KHR_wait_sync and EGL 1.5 sync objects.

#ifndef LIBANGLE_EGLSYNC_H_
#define LIBANGLE_EGLSYNC_H_

#include "libANGLE/Debug.h"
#include "libANGLE/Display.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

#include "common/angleutils.h"

namespace rx
{
class EGLSyncImpl;
class ThreadSafeDisplayImpl;
}  // namespace rx

namespace gl
{
class Context;
}  // namespace gl

namespace egl
{
class Sync final : public LabeledObject
{
  public:
    Sync(rx::ThreadSafeDisplayImpl *factory, EGLenum type);
    ~Sync() override;

    void setLabel(EGLLabelKHR label) override;
    EGLLabelKHR getLabel() const override;

    const SyncID &id() const { return mId; }

    void onDestroy(const ThreadSafeDisplay *display);

    Error initialize(const ThreadSafeDisplay *display,
                     const gl::Context *context,
                     const SyncID &id,
                     const AttributeMap &attribs);
    Error clientWait(const ThreadSafeDisplay *display,
                     const gl::Context *context,
                     EGLint flags,
                     EGLTime timeout,
                     EGLint *outResult);
    Error serverWait(const ThreadSafeDisplay *display, const gl::Context *context, EGLint flags);
    Error signal(const ThreadSafeDisplay *display, const gl::Context *context, EGLint mode);
    Error getStatus(const ThreadSafeDisplay *display, EGLint *outStatus) const;

    Error copyMetalSharedEventANGLE(const ThreadSafeDisplay *display, void **result) const;
    Error dupNativeFenceFD(const ThreadSafeDisplay *display, EGLint *result) const;

    EGLenum getType() const { return mType; }
    const AttributeMap &getAttributeMap() const { return mAttributeMap; }
    EGLint getCondition() const { return mCondition; }
    EGLint getNativeFenceFD() const { return mNativeFenceFD; }

    void addRef() const { mRefCount.fetch_add(1, std::memory_order_relaxed); }
    bool releaseRef() const { return mRefCount.fetch_sub(1, std::memory_order_acq_rel) == 1; }
    uint32_t getRefCount() const { return mRefCount.load(std::memory_order_acquire); }

  private:
    std::unique_ptr<rx::EGLSyncImpl> mFence;

    EGLLabelKHR mLabel;

    SyncID mId;
    EGLenum mType;
    AttributeMap mAttributeMap;
    EGLint mCondition;
    EGLint mNativeFenceFD;

    mutable std::atomic<uint32_t> mRefCount{0};
};

// Holds a reference to a Sync so that it stays alive while in use, and releases it through the
// display when it goes out of scope.
//
// This does not hold a reference to the display.  A ScopedSyncRef only lives while the EGL entry
// point that created it holds (or locks) the display, so the display cannot be terminated before
// the sync is released.  Unlocked tail calls that capture a ScopedSyncRef must therefore run before
// the entry point releases the display; see egl_tail_call_holds_display() in
// scripts/generate_entry_points.py.  This cannot hold a reference to the display of its own, since
// a thread must not take a second reference to the display (see ThreadSafeDisplay::addRef()).
class [[nodiscard]] ScopedSyncRef final
{
  public:
    ScopedSyncRef() = default;
    ScopedSyncRef(ThreadSafeDisplay *display, Sync *sync) : mDisplay(display), mSync(sync)
    {
        if (mSync)
        {
            ASSERT(mDisplay != nullptr);
            mSync->addRef();
        }
    }
    ~ScopedSyncRef()
    {
        if (mSync)
        {
            // See the class comment.  This fires if, for example, an unlocked tail call that
            // captured a ScopedSyncRef runs after its entry point released the display.
            AssertCurrentThreadHoldingOneDisplayRefCount();
            mDisplay->releaseSync(mSync);
        }
    }
    ScopedSyncRef(const ScopedSyncRef &other) : mDisplay(other.mDisplay), mSync(other.mSync)
    {
        if (mSync)
        {
            mSync->addRef();
        }
    }
    ScopedSyncRef(ScopedSyncRef &&other) noexcept : mDisplay(other.mDisplay), mSync(other.mSync)
    {
        other.mDisplay = nullptr;
        other.mSync    = nullptr;
    }
    ScopedSyncRef &operator=(const ScopedSyncRef &other) = delete;
    ScopedSyncRef &operator=(ScopedSyncRef &&other)      = delete;

    Sync *get() const { return mSync; }

  private:
    ThreadSafeDisplay *mDisplay = nullptr;
    Sync *mSync                 = nullptr;
};

class [[nodiscard]] ScopedSyncDestroy final : angle::NonCopyable
{
  public:
    ScopedSyncDestroy() : mDisplay(nullptr), mSync(nullptr) {}
    ScopedSyncDestroy(ThreadSafeDisplay *display, Sync *sync) : mDisplay(display), mSync(sync) {}
    ~ScopedSyncDestroy()
    {
        if (mSync)
        {
            ASSERT(mDisplay != nullptr);
            AssertCurrentThreadHoldingOneDisplayRefCount();
            mDisplay->releaseSync(mSync);
        }
    }

    Sync *get() const { return mSync; }
    Sync *release()
    {
        Sync *sync = mSync;
        mSync      = nullptr;
        return sync;
    }

  private:
    ThreadSafeDisplay *mDisplay;
    Sync *mSync;
};

}  // namespace egl

#endif  // LIBANGLE_EGLSYNC_H_
