//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SyncVk.cpp:
//    Implements the class methods for SyncVk.
//

#include "libANGLE/renderer/vulkan/SyncVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"

namespace rx
{
namespace vk
{
SyncHelper::SyncHelper()
{
    mUse.init();
}

SyncHelper::~SyncHelper()
{
    mUse.release();
}

void SyncHelper::releaseToRenderer(RendererVk *renderer)
{
    renderer->collectGarbageAndReinit(&mUse, &mEvent);
    mFence.reset(renderer->getDevice());
}

angle::Result SyncHelper::initialize(ContextVk *contextVk)
{
    ASSERT(!mEvent.valid());

    RendererVk *renderer = contextVk->getRenderer();
    VkDevice device      = renderer->getDevice();

    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType             = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    eventCreateInfo.flags             = 0;

    DeviceScoped<Event> event(device);
    ANGLE_VK_TRY(contextVk, event.get().init(device, eventCreateInfo));
    ANGLE_TRY(contextVk->getNextSubmitFence(&mFence));

    mEvent = event.release();

    CommandGraph *commandGraph = contextVk->getCommandGraph();
    commandGraph->setFenceSync(mEvent);
    contextVk->getResourceUseList().add(mUse);

    return angle::Result::Continue;
}

angle::Result SyncHelper::clientWait(Context *context,
                                     ContextVk *contextVk,
                                     bool flushCommands,
                                     uint64_t timeout,
                                     VkResult *outResult)
{
    RendererVk *renderer = context->getRenderer();

    // If the event is already set, don't wait
    bool alreadySignaled = false;
    ANGLE_TRY(getStatus(context, &alreadySignaled));
    if (alreadySignaled)
    {
        *outResult = VK_EVENT_SET;
        return angle::Result::Continue;
    }

    // If timeout is zero, there's no need to wait, so return timeout already.
    if (timeout == 0)
    {
        *outResult = VK_TIMEOUT;
        return angle::Result::Continue;
    }

    if (flushCommands && contextVk)
    {
        ANGLE_TRY(contextVk->flushImpl(nullptr));
    }

    // Wait on the fence that's expected to be signaled on the first vkQueueSubmit after
    // `initialize` was called. The first fence is the fence created to signal this sync.
    ASSERT(mFence.get().valid());
    VkResult status = mFence.get().wait(renderer->getDevice(), timeout);

    // Check for errors, but don't consider timeout as such.
    if (status != VK_TIMEOUT)
    {
        ANGLE_VK_TRY(context, status);
    }

    *outResult = status;
    return angle::Result::Continue;
}

void SyncHelper::serverWait(ContextVk *contextVk)
{
    CommandGraph *commandGraph = contextVk->getCommandGraph();
    commandGraph->waitFenceSync(mEvent);
    contextVk->getResourceUseList().add(mUse);
}

angle::Result SyncHelper::getStatus(Context *context, bool *signaled)
{
    VkResult result = mEvent.getStatus(context->getDevice());
    if (result != VK_EVENT_SET && result != VK_EVENT_RESET)
    {
        ANGLE_VK_TRY(context, result);
    }
    *signaled = result == VK_EVENT_SET;
    return angle::Result::Continue;
}
}  // namespace vk

SyncVk::SyncVk() : SyncImpl() {}

SyncVk::~SyncVk() {}

void SyncVk::onDestroy(const gl::Context *context)
{
    mFenceSync.releaseToRenderer(vk::GetImpl(context)->getRenderer());
}

angle::Result SyncVk::set(const gl::Context *context, GLenum condition, GLbitfield flags)
{
    ASSERT(condition == GL_SYNC_GPU_COMMANDS_COMPLETE);
    ASSERT(flags == 0);

    return mFenceSync.initialize(vk::GetImpl(context));
}

angle::Result SyncVk::clientWait(const gl::Context *context,
                                 GLbitfield flags,
                                 GLuint64 timeout,
                                 GLenum *outResult)
{
    ContextVk *contextVk = vk::GetImpl(context);

    ASSERT((flags & ~GL_SYNC_FLUSH_COMMANDS_BIT) == 0);

    bool flush = (flags & GL_SYNC_FLUSH_COMMANDS_BIT) != 0;
    VkResult result;

    ANGLE_TRY(mFenceSync.clientWait(contextVk, contextVk, flush, static_cast<uint64_t>(timeout),
                                    &result));

    switch (result)
    {
        case VK_EVENT_SET:
            *outResult = GL_ALREADY_SIGNALED;
            return angle::Result::Continue;

        case VK_SUCCESS:
            *outResult = GL_CONDITION_SATISFIED;
            return angle::Result::Continue;

        case VK_TIMEOUT:
            *outResult = GL_TIMEOUT_EXPIRED;
            return angle::Result::Incomplete;

        default:
            UNREACHABLE();
            *outResult = GL_WAIT_FAILED;
            return angle::Result::Stop;
    }
}

angle::Result SyncVk::serverWait(const gl::Context *context, GLbitfield flags, GLuint64 timeout)
{
    ASSERT(flags == 0);
    ASSERT(timeout == GL_TIMEOUT_IGNORED);

    ContextVk *contextVk = vk::GetImpl(context);
    mFenceSync.serverWait(contextVk);
    return angle::Result::Continue;
}

angle::Result SyncVk::getStatus(const gl::Context *context, GLint *outResult)
{
    bool signaled = false;
    ANGLE_TRY(mFenceSync.getStatus(vk::GetImpl(context), &signaled));

    *outResult = signaled ? GL_SIGNALED : GL_UNSIGNALED;
    return angle::Result::Continue;
}

EGLSyncVk::EGLSyncVk(const egl::AttributeMap &attribs) : EGLSyncImpl()
{
    ASSERT(attribs.isEmpty());
}

EGLSyncVk::~EGLSyncVk() {}

void EGLSyncVk::onDestroy(const egl::Display *display)
{
    mFenceSync.releaseToRenderer(vk::GetImpl(display)->getRenderer());
}

egl::Error EGLSyncVk::initialize(const egl::Display *display,
                                 const gl::Context *context,
                                 EGLenum type)
{
    ASSERT(type == EGL_SYNC_FENCE_KHR);
    ASSERT(context != nullptr);

    if (mFenceSync.initialize(vk::GetImpl(context)) == angle::Result::Stop)
    {
        return egl::Error(EGL_BAD_ALLOC, "eglCreateSyncKHR failed to create sync object");
    }

    return egl::NoError();
}

egl::Error EGLSyncVk::clientWait(const egl::Display *display,
                                 const gl::Context *context,
                                 EGLint flags,
                                 EGLTime timeout,
                                 EGLint *outResult)
{
    ASSERT((flags & ~EGL_SYNC_FLUSH_COMMANDS_BIT_KHR) == 0);

    bool flush = (flags & EGL_SYNC_FLUSH_COMMANDS_BIT_KHR) != 0;
    VkResult result;

    ContextVk *contextVk = context ? vk::GetImpl(context) : nullptr;
    if (mFenceSync.clientWait(vk::GetImpl(display), contextVk, flush,
                              static_cast<uint64_t>(timeout), &result) == angle::Result::Stop)
    {
        return egl::Error(EGL_BAD_ALLOC);
    }

    switch (result)
    {
        case VK_EVENT_SET:
            // fall through.  EGL doesn't differentiate between event being already set, or set
            // before timeout.
        case VK_SUCCESS:
            *outResult = EGL_CONDITION_SATISFIED_KHR;
            return egl::NoError();

        case VK_TIMEOUT:
            *outResult = EGL_TIMEOUT_EXPIRED_KHR;
            return egl::NoError();

        default:
            UNREACHABLE();
            *outResult = EGL_FALSE;
            return egl::Error(EGL_BAD_ALLOC);
    }
}

egl::Error EGLSyncVk::serverWait(const egl::Display *display,
                                 const gl::Context *context,
                                 EGLint flags)
{
    // Server wait requires a valid bound context.
    ASSERT(context);

    // No flags are currently implemented.
    ASSERT(flags == 0);

    ContextVk *contextVk = vk::GetImpl(context);
    mFenceSync.serverWait(contextVk);
    return egl::NoError();
}

egl::Error EGLSyncVk::getStatus(const egl::Display *display, EGLint *outStatus)
{
    bool signaled = false;
    if (mFenceSync.getStatus(vk::GetImpl(display), &signaled) == angle::Result::Stop)
    {
        return egl::Error(EGL_BAD_ALLOC);
    }

    *outStatus = signaled ? EGL_SIGNALED_KHR : EGL_UNSIGNALED_KHR;
    return egl::NoError();
}

egl::Error EGLSyncVk::dupNativeFenceFD(const egl::Display *display, EGLint *result) const
{
    UNREACHABLE();
    return egl::EglBadDisplay();
}

}  // namespace rx
