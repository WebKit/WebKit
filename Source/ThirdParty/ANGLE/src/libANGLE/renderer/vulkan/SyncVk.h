//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SyncVk:
//    Defines the class interface for SyncVk, implementing SyncImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_
#define LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_

#include "libANGLE/renderer/EGLSyncImpl.h"
#include "libANGLE/renderer/SyncImpl.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"

namespace egl
{
class AttributeMap;
}

namespace rx
{
namespace vk
{

// Represents an invalid native fence FD.
constexpr int kInvalidFenceFd = EGL_NO_NATIVE_FENCE_FD_ANDROID;

// Implementation of fence types - glFenceSync, and EGLSync(EGL_SYNC_FENCE_KHR).
// The behaviors of SyncVk and EGLFenceSyncVk as fence syncs are currently
// identical for the Vulkan backend, and this class implements both interfaces.
class SyncHelper : public vk::Resource
{
  public:
    SyncHelper();
    ~SyncHelper() override;

    virtual void releaseToRenderer(RendererVk *renderer);

    virtual angle::Result initialize(ContextVk *contextVk);
    virtual angle::Result clientWait(Context *context,
                                     ContextVk *contextVk,
                                     bool flushCommands,
                                     uint64_t timeout,
                                     VkResult *outResult);
    virtual angle::Result serverWait(ContextVk *contextVk);
    virtual angle::Result getStatus(Context *context, bool *signaled) const;
    virtual angle::Result dupNativeFenceFD(Context *context, int *fdOut) const
    {
        return angle::Result::Stop;
    }

  private:
    // The vkEvent that's signaled on `init` and can be waited on in `serverWait`, or queried with
    // `getStatus`.
    Event mEvent;
    // The fence is signaled once the CB including the `init` signal is executed.
    // `clientWait` waits on this fence.
    Shared<Fence> mFence;
};

// Implementation of sync types: EGLSync(EGL_SYNC_ANDROID_NATIVE_FENCE_ANDROID).
class SyncHelperNativeFence : public SyncHelper
{
  public:
    SyncHelperNativeFence();
    ~SyncHelperNativeFence() override;

    void releaseToRenderer(RendererVk *renderer) override;

    angle::Result initializeWithFd(ContextVk *contextVk, int inFd);
    angle::Result clientWait(Context *context,
                             ContextVk *contextVk,
                             bool flushCommands,
                             uint64_t timeout,
                             VkResult *outResult) override;
    angle::Result serverWait(ContextVk *contextVk) override;
    angle::Result getStatus(Context *context, bool *signaled) const override;
    angle::Result dupNativeFenceFD(Context *context, int *fdOut) const override;

  private:
    vk::Fence mFenceWithFd;
    int mNativeFenceFd;
};

}  // namespace vk

// Implementor for glFenceSync.
class SyncVk final : public SyncImpl
{
  public:
    SyncVk();
    ~SyncVk() override;

    void onDestroy(const gl::Context *context) override;

    angle::Result set(const gl::Context *context, GLenum condition, GLbitfield flags) override;
    angle::Result clientWait(const gl::Context *context,
                             GLbitfield flags,
                             GLuint64 timeout,
                             GLenum *outResult) override;
    angle::Result serverWait(const gl::Context *context,
                             GLbitfield flags,
                             GLuint64 timeout) override;
    angle::Result getStatus(const gl::Context *context, GLint *outResult) override;

  private:
    vk::SyncHelper mSyncHelper;
};

// Implementor for EGLSync.
class EGLSyncVk final : public EGLSyncImpl
{
  public:
    EGLSyncVk(const egl::AttributeMap &attribs);
    ~EGLSyncVk() override;

    void onDestroy(const egl::Display *display) override;

    egl::Error initialize(const egl::Display *display,
                          const gl::Context *context,
                          EGLenum type) override;
    egl::Error clientWait(const egl::Display *display,
                          const gl::Context *context,
                          EGLint flags,
                          EGLTime timeout,
                          EGLint *outResult) override;
    egl::Error serverWait(const egl::Display *display,
                          const gl::Context *context,
                          EGLint flags) override;
    egl::Error getStatus(const egl::Display *display, EGLint *outStatus) override;

    egl::Error dupNativeFenceFD(const egl::Display *display, EGLint *fdOut) const override;

  private:
    EGLenum mType;
    vk::SyncHelper *mSyncHelper;  // SyncHelper or SyncHelperNativeFence decided at run-time.
    const egl::AttributeMap &mAttribs;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_
