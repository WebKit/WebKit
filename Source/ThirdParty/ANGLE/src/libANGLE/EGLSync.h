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
    Sync(rx::EGLImplFactory *factory, EGLenum type);
    ~Sync() override;

    void setLabel(EGLLabelKHR label) override;
    EGLLabelKHR getLabel() const override;

    const SyncID &id() const { return mId; }

    void onDestroy(const Display *display);

    Error initialize(const Display *display,
                     const gl::Context *context,
                     const SyncID &id,
                     const AttributeMap &attribs);
    Error clientWait(const Display *display,
                     const gl::Context *context,
                     EGLint flags,
                     EGLTime timeout,
                     EGLint *outResult);
    Error serverWait(const Display *display, const gl::Context *context, EGLint flags);
    Error signal(const Display *display, const gl::Context *context, EGLint mode);
    Error getStatus(const Display *display, EGLint *outStatus) const;

    Error copyMetalSharedEventANGLE(const Display *display, void **result) const;
    Error dupNativeFenceFD(const Display *display, EGLint *result) const;

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

class [[nodiscard]] ScopedSyncRef final
{
  public:
    ScopedSyncRef() = default;
    ScopedSyncRef(Display &display, Sync *sync) : mDisplay(display), mSync(sync)
    {
        // Callers only invoke this with valid Sync* objects; if lookup fails, ScopedSyncRef() is
        // used instead.
        ASSERT(mSync != nullptr);
        mSync->addRef();
    }
    ScopedSyncRef(Display *display, Sync *sync)
        : mDisplay(display ? ScopedDisplayRef(*display) : ScopedDisplayRef()), mSync(sync)
    {
        if (mSync)
        {
            mSync->addRef();
        }
    }
    ~ScopedSyncRef()
    {
        if (mSync)
        {
            mDisplay.get()->releaseSync(mSync);
        }
    }
    ScopedSyncRef(const ScopedSyncRef &other) : mDisplay(other.mDisplay), mSync(other.mSync)
    {
        if (mSync)
        {
            mSync->addRef();
        }
    }
    ScopedSyncRef(ScopedSyncRef &&other) noexcept
        : mDisplay(std::move(other.mDisplay)), mSync(other.mSync)
    {
        other.mSync = nullptr;
    }

    Sync *get() const { return mSync; }

  private:
    ScopedDisplayRef mDisplay;
    Sync *mSync = nullptr;
};

}  // namespace egl

#endif  // LIBANGLE_EGLSYNC_H_
