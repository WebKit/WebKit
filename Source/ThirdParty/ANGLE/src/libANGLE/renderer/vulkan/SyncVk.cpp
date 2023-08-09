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

#if !defined(ANGLE_PLATFORM_WINDOWS)
#    include <poll.h>
#    include <unistd.h>
#else
#    include <io.h>
#endif

namespace
{
// Wait for file descriptor to be signaled
VkResult SyncWaitFd(int fd, uint64_t timeoutNs, VkResult timeoutResult = VK_TIMEOUT)
{
#if !defined(ANGLE_PLATFORM_WINDOWS)
    struct pollfd fds;
    int ret;

    // Convert nanoseconds to milliseconds
    int timeoutMs = static_cast<int>(timeoutNs / 1000000);
    // If timeoutNs was non-zero but less than one millisecond, make it a millisecond.
    if (timeoutNs > 0 && timeoutNs < 1000000)
    {
        timeoutMs = 1;
    }

    ASSERT(fd >= 0);

    fds.fd     = fd;
    fds.events = POLLIN;

    do
    {
        ret = poll(&fds, 1, timeoutMs);
        if (ret > 0)
        {
            if (fds.revents & (POLLERR | POLLNVAL))
            {
                return VK_ERROR_UNKNOWN;
            }
            return VK_SUCCESS;
        }
        else if (ret == 0)
        {
            return timeoutResult;
        }
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    return VK_ERROR_UNKNOWN;
#else
    UNREACHABLE();
    return VK_ERROR_UNKNOWN;
#endif
}

}  // anonymous namespace

namespace rx
{
namespace vk
{
SyncHelper::SyncHelper() {}

SyncHelper::~SyncHelper() {}

void SyncHelper::releaseToRenderer(RendererVk *renderer) {}

angle::Result SyncHelper::initialize(ContextVk *contextVk, bool isEGLSyncObject)
{
    ASSERT(!mUse.valid());
    return contextVk->onSyncObjectInit(this, isEGLSyncObject);
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
    ANGLE_TRY(getStatus(context, contextVk, &alreadySignaled));
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

    // Submit commands if requested
    if (flushCommands && contextVk)
    {
        ANGLE_TRY(contextVk->flushCommandsAndEndRenderPassIfDeferredSyncInit(
            RenderPassClosureReason::SyncObjectClientWait));
    }
    // Submit commands if it was deferred on the context that issued the sync object
    ANGLE_TRY(submitSyncIfDeferred(contextVk, RenderPassClosureReason::SyncObjectClientWait));

    VkResult status = VK_SUCCESS;
    ANGLE_TRY(renderer->waitForResourceUseToFinishWithUserTimeout(context, mUse, timeout, &status));

    // Check for errors, but don't consider timeout as such.
    if (status != VK_TIMEOUT)
    {
        ANGLE_VK_TRY(context, status);
    }

    *outResult = status;
    return angle::Result::Continue;
}

angle::Result SyncHelper::serverWait(ContextVk *contextVk)
{
    // If already signaled, no need to wait
    bool alreadySignaled = false;
    ANGLE_TRY(getStatus(contextVk, contextVk, &alreadySignaled));
    if (alreadySignaled)
    {
        return angle::Result::Continue;
    }

    // Every resource already tracks its usage and issues the appropriate barriers, so there's
    // really nothing to do here.  An execution barrier is issued to strictly satisfy what the
    // application asked for.
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer({}, &commandBuffer));
    commandBuffer->pipelineBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 0,
                                   nullptr);
    return angle::Result::Continue;
}

angle::Result SyncHelper::getStatus(Context *context, ContextVk *contextVk, bool *signaledOut)
{
    // Submit commands if it was deferred on the context that issued the sync object
    ANGLE_TRY(submitSyncIfDeferred(contextVk, RenderPassClosureReason::SyncObjectClientWait));
    ASSERT(mUse.valid());
    RendererVk *renderer = context->getRenderer();
    if (renderer->hasResourceUseFinished(mUse))
    {
        *signaledOut = true;
    }
    else
    {
        // Do immediate check in case it actually already finished.
        ANGLE_TRY(renderer->checkCompletedCommands(context));
        *signaledOut = renderer->hasResourceUseFinished(mUse);
    }
    return angle::Result::Continue;
}

angle::Result SyncHelper::submitSyncIfDeferred(ContextVk *contextVk, RenderPassClosureReason reason)
{
    if (contextVk == nullptr)
    {
        // This is EGLSync case. We always immediately call flushImpl.
        return angle::Result::Continue;
    }

    if (contextVk->getRenderer()->hasResourceUseSubmitted(mUse))
    {
        return angle::Result::Continue;
    }

    // The submission of a sync object may be deferred to allow further optimizations to an open
    // render pass before a submission happens for another reason.  If the sync object is being
    // waited on by the current context, the application must have used GL_SYNC_FLUSH_COMMANDS_BIT.
    // However, when waited on by other contexts, the application must have ensured the original
    // context is flushed.  Due to the deferFlushUntilEndRenderPass feature, a glFlush is not
    // sufficient to guarantee this.
    //
    // Deferring the submission is restricted to non-EGL sync objects, so it's sufficient to ensure
    // that the contexts in the share group issue their deferred flushes.
    for (auto context : contextVk->getShareGroup()->getContexts())
    {
        ContextVk *sharedContextVk = vk::GetImpl(context.second);
        if (sharedContextVk->hasUnsubmittedUse(mUse))
        {
            ANGLE_TRY(sharedContextVk->flushCommandsAndEndRenderPassIfDeferredSyncInit(reason));
            break;
        }
    }
    // Note mUse could still be invalid here if it is inserted on a fresh created context, i.e.,
    // fence is tracking nothing and is finished when inserted..
    ASSERT(contextVk->getRenderer()->hasResourceUseSubmitted(mUse));

    return angle::Result::Continue;
}

ExternalFence::ExternalFence()
    : mDevice(VK_NULL_HANDLE), mFenceFdStatus(VK_INCOMPLETE), mFenceFd(kInvalidFenceFd)
{}

ExternalFence::~ExternalFence()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        mFence.destroy(mDevice);
    }

    if (mFenceFd != kInvalidFenceFd)
    {
        close(mFenceFd);
    }
}

VkResult ExternalFence::init(VkDevice device, const VkFenceCreateInfo &createInfo)
{
    ASSERT(device != VK_NULL_HANDLE);
    ASSERT(mFenceFdStatus == VK_INCOMPLETE && mFenceFd == kInvalidFenceFd);
    ASSERT(mDevice == VK_NULL_HANDLE);
    mDevice = device;
    return mFence.init(device, createInfo);
}

void ExternalFence::init(int fenceFd)
{
    ASSERT(fenceFd != kInvalidFenceFd);
    ASSERT(mFenceFdStatus == VK_INCOMPLETE && mFenceFd == kInvalidFenceFd);
    mFenceFdStatus = VK_SUCCESS;
    mFenceFd       = fenceFd;
}

VkResult ExternalFence::getStatus(VkDevice device) const
{
    if (mFenceFdStatus == VK_SUCCESS)
    {
        return SyncWaitFd(mFenceFd, 0, VK_NOT_READY);
    }
    return mFence.getStatus(device);
}

VkResult ExternalFence::wait(VkDevice device, uint64_t timeout) const
{
    if (mFenceFdStatus == VK_SUCCESS)
    {
        return SyncWaitFd(mFenceFd, timeout);
    }
    return mFence.wait(device, timeout);
}

void ExternalFence::exportFd(VkDevice device, const VkFenceGetFdInfoKHR &fenceGetFdInfo)
{
    ASSERT(mFenceFdStatus == VK_INCOMPLETE && mFenceFd == kInvalidFenceFd);
    mFenceFdStatus = mFence.exportFd(device, fenceGetFdInfo, &mFenceFd);
    ASSERT(mFenceFdStatus != VK_INCOMPLETE);
}

SyncHelperNativeFence::SyncHelperNativeFence()
{
    mExternalFence = std::make_shared<ExternalFence>();
}

SyncHelperNativeFence::~SyncHelperNativeFence() {}

void SyncHelperNativeFence::releaseToRenderer(RendererVk *renderer)
{
    mExternalFence.reset();
}

angle::Result SyncHelperNativeFence::initializeWithFd(ContextVk *contextVk, int inFd)
{
    ASSERT(inFd >= kInvalidFenceFd);

    // If valid FD provided by application - import it to fence.
    if (inFd > kInvalidFenceFd)
    {
        // File descriptor ownership: EGL_ANDROID_native_fence_sync
        // Whenever a file descriptor is passed into or returned from an
        // EGL call in this extension, ownership of that file descriptor is
        // transferred. The recipient of the file descriptor must close it when it is
        // no longer needed, and the provider of the file descriptor must dup it
        // before providing it if they require continued use of the native fence.
        mExternalFence->init(inFd);
        return angle::Result::Continue;
    }

    RendererVk *renderer = contextVk->getRenderer();
    VkDevice device      = renderer->getDevice();

    VkExportFenceCreateInfo exportCreateInfo = {};
    exportCreateInfo.sType                   = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO;
    exportCreateInfo.pNext                   = nullptr;
    exportCreateInfo.handleTypes             = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

    // Create fenceInfo base.
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags             = 0;
    fenceCreateInfo.pNext             = &exportCreateInfo;

    // Initialize/create a VkFence handle
    ANGLE_VK_TRY(contextVk, mExternalFence->init(device, fenceCreateInfo));

    // invalid FD provided by application - create one with fence.
    /*
      Spec: "When a fence sync object is created or when an EGL native fence sync
      object is created with the EGL_SYNC_NATIVE_FENCE_FD_ANDROID attribute set to
      EGL_NO_NATIVE_FENCE_FD_ANDROID, eglCreateSyncKHR also inserts a fence command
      into the command stream of the bound client API's current context and associates it
      with the newly created sync object.
    */
    // Flush current pending set of commands providing the fence...
    ANGLE_TRY(contextVk->flushImpl(nullptr, &mExternalFence,
                                   RenderPassClosureReason::SyncObjectWithFdInit));
    QueueSerial submitSerial = contextVk->getLastSubmittedQueueSerial();

    // exportFd is exporting VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR type handle which
    // obeys copy semantics. This means that the fence must already be signaled or the work to
    // signal it is in the graphics pipeline at the time we export the fd. Thus we need to
    // call waitForQueueSerialToBeSubmittedToDevice() here.
    ANGLE_TRY(renderer->waitForQueueSerialToBeSubmittedToDevice(contextVk, submitSerial));

    ANGLE_VK_TRY(contextVk, mExternalFence->getFenceFdStatus());

    return angle::Result::Continue;
}

angle::Result SyncHelperNativeFence::clientWait(Context *context,
                                                ContextVk *contextVk,
                                                bool flushCommands,
                                                uint64_t timeout,
                                                VkResult *outResult)
{
    RendererVk *renderer = context->getRenderer();

    // If already signaled, don't wait
    bool alreadySignaled = false;
    ANGLE_TRY(getStatus(context, contextVk, &alreadySignaled));
    if (alreadySignaled)
    {
        *outResult = VK_SUCCESS;
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
        ANGLE_TRY(
            contextVk->flushImpl(nullptr, nullptr, RenderPassClosureReason::SyncObjectClientWait));
    }

    VkResult status = mExternalFence->wait(renderer->getDevice(), timeout);
    if (status != VK_TIMEOUT)
    {
        ANGLE_VK_TRY(contextVk, status);
    }

    *outResult = status;
    return angle::Result::Continue;
}

angle::Result SyncHelperNativeFence::serverWait(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    // If already signaled, no need to wait
    bool alreadySignaled = false;
    ANGLE_TRY(getStatus(contextVk, contextVk, &alreadySignaled));
    if (alreadySignaled)
    {
        return angle::Result::Continue;
    }

    VkDevice device = renderer->getDevice();
    DeviceScoped<Semaphore> waitSemaphore(device);
    // Wait semaphore for next vkQueueSubmit().
    // Create a Semaphore with imported fenceFd.
    ANGLE_VK_TRY(contextVk, waitSemaphore.get().init(device));

    VkImportSemaphoreFdInfoKHR importFdInfo = {};
    importFdInfo.sType                      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    importFdInfo.semaphore                  = waitSemaphore.get().getHandle();
    importFdInfo.flags                      = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR;
    importFdInfo.handleType                 = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR;
    importFdInfo.fd                         = dup(mExternalFence->getFenceFd());
    ANGLE_VK_TRY(contextVk, waitSemaphore.get().importFd(device, importFdInfo));

    // Add semaphore to next submit job.
    contextVk->addWaitSemaphore(waitSemaphore.get().getHandle(),
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    contextVk->addGarbage(&waitSemaphore.get());  // This releases the handle.
    return angle::Result::Continue;
}

angle::Result SyncHelperNativeFence::getStatus(Context *context,
                                               ContextVk *contextVk,
                                               bool *signaledOut)
{
    VkResult result = mExternalFence->getStatus(context->getDevice());
    if (result != VK_NOT_READY)
    {
        ANGLE_VK_TRY(context, result);
    }
    *signaledOut = (result == VK_SUCCESS);
    return angle::Result::Continue;
}

angle::Result SyncHelperNativeFence::dupNativeFenceFD(Context *context, int *fdOut) const
{
    if (mExternalFence->getFenceFd() == kInvalidFenceFd)
    {
        return angle::Result::Stop;
    }

    *fdOut = dup(mExternalFence->getFenceFd());

    return angle::Result::Continue;
}

}  // namespace vk

SyncVk::SyncVk() : SyncImpl() {}

SyncVk::~SyncVk() {}

void SyncVk::onDestroy(const gl::Context *context)
{
    mSyncHelper.releaseToRenderer(vk::GetImpl(context)->getRenderer());
}

angle::Result SyncVk::set(const gl::Context *context, GLenum condition, GLbitfield flags)
{
    ASSERT(condition == GL_SYNC_GPU_COMMANDS_COMPLETE);
    ASSERT(flags == 0);

    return mSyncHelper.initialize(vk::GetImpl(context), false);
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

    ANGLE_TRY(mSyncHelper.clientWait(contextVk, contextVk, flush, static_cast<uint64_t>(timeout),
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
    return mSyncHelper.serverWait(contextVk);
}

angle::Result SyncVk::getStatus(const gl::Context *context, GLint *outResult)
{
    ContextVk *contextVk = vk::GetImpl(context);
    bool signaled        = false;
    ANGLE_TRY(mSyncHelper.getStatus(contextVk, contextVk, &signaled));

    *outResult = signaled ? GL_SIGNALED : GL_UNSIGNALED;
    return angle::Result::Continue;
}

EGLSyncVk::EGLSyncVk(const egl::AttributeMap &attribs)
    : EGLSyncImpl(),
      mSyncHelper(nullptr),
      mNativeFenceFD(
          attribs.getAsInt(EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID))
{}

EGLSyncVk::~EGLSyncVk()
{
    SafeDelete(mSyncHelper);
}

void EGLSyncVk::onDestroy(const egl::Display *display)
{
    mSyncHelper->releaseToRenderer(vk::GetImpl(display)->getRenderer());
}

egl::Error EGLSyncVk::initialize(const egl::Display *display,
                                 const gl::Context *context,
                                 EGLenum type)
{
    ASSERT(context != nullptr);
    mType = type;

    switch (type)
    {
        case EGL_SYNC_FENCE_KHR:
        {
            vk::SyncHelper *syncHelper = new vk::SyncHelper();
            mSyncHelper                = syncHelper;
            if (syncHelper->initialize(vk::GetImpl(context), true) == angle::Result::Stop)
            {
                return egl::Error(EGL_BAD_ALLOC, "eglCreateSyncKHR failed to create sync object");
            }
            return egl::NoError();
        }
        case EGL_SYNC_NATIVE_FENCE_ANDROID:
        {
            vk::SyncHelperNativeFence *syncHelper = new vk::SyncHelperNativeFence();
            mSyncHelper                           = syncHelper;
            return angle::ToEGL(syncHelper->initializeWithFd(vk::GetImpl(context), mNativeFenceFD),
                                EGL_BAD_ALLOC);
        }
        default:
            UNREACHABLE();
            return egl::Error(EGL_BAD_ALLOC);
    }
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
    if (mSyncHelper->clientWait(vk::GetImpl(display), contextVk, flush,
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
    return angle::ToEGL(mSyncHelper->serverWait(contextVk), EGL_BAD_ALLOC);
}

egl::Error EGLSyncVk::getStatus(const egl::Display *display, EGLint *outStatus)
{
    bool signaled = false;
    if (mSyncHelper->getStatus(vk::GetImpl(display), nullptr, &signaled) == angle::Result::Stop)
    {
        return egl::Error(EGL_BAD_ALLOC);
    }

    *outStatus = signaled ? EGL_SIGNALED_KHR : EGL_UNSIGNALED_KHR;
    return egl::NoError();
}

egl::Error EGLSyncVk::dupNativeFenceFD(const egl::Display *display, EGLint *fdOut) const
{
    switch (mType)
    {
        case EGL_SYNC_NATIVE_FENCE_ANDROID:
            return angle::ToEGL(mSyncHelper->dupNativeFenceFD(vk::GetImpl(display), fdOut),
                                EGL_BAD_PARAMETER);
        default:
            return egl::EglBadDisplay();
    }
}

}  // namespace rx
