// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SemaphoreVk.cpp: Defines the class interface for SemaphoreVk, implementing
// SemaphoreImpl.

#include "libANGLE/renderer/vulkan/SemaphoreVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

SemaphoreVk::SemaphoreVk() = default;

SemaphoreVk::~SemaphoreVk() = default;

void SemaphoreVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    contextVk->addGarbage(&mSemaphore);
}

angle::Result SemaphoreVk::importFd(gl::Context *context, gl::HandleType handleType, GLint fd)
{
    switch (handleType)
    {
        case gl::HandleType::OpaqueFd:
            return importOpaqueFd(context, fd);

        default:
            ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
            return angle::Result::Stop;
    }
}

angle::Result SemaphoreVk::wait(gl::Context *context,
                                const gl::BufferBarrierVector &bufferBarriers,
                                const gl::TextureBarrierVector &textureBarriers)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (!bufferBarriers.empty())
    {
        // Buffers in external memory are not implemented yet.
        UNIMPLEMENTED();
    }

    if (!textureBarriers.empty())
    {
        // Texture barriers are not implemented yet.
        UNIMPLEMENTED();
    }

    contextVk->insertWaitSemaphore(&mSemaphore);
    return angle::Result::Continue;
}

angle::Result SemaphoreVk::signal(gl::Context *context,
                                  const gl::BufferBarrierVector &bufferBarriers,
                                  const gl::TextureBarrierVector &textureBarriers)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (!bufferBarriers.empty())
    {
        // Buffers in external memory are not implemented yet.
        UNIMPLEMENTED();
    }

    if (!textureBarriers.empty())
    {
        // Texture barriers are not implemented yet.
        UNIMPLEMENTED();
    }

    return contextVk->flushImpl(&mSemaphore);
}

angle::Result SemaphoreVk::importOpaqueFd(gl::Context *context, GLint fd)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    if (!mSemaphore.valid())
    {
        mSemaphore.init(renderer->getDevice());
    }

    ASSERT(mSemaphore.valid());

    VkImportSemaphoreFdInfoKHR importSemaphoreFdInfo = {};
    importSemaphoreFdInfo.sType      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    importSemaphoreFdInfo.semaphore  = mSemaphore.getHandle();
    importSemaphoreFdInfo.flags      = 0;
    importSemaphoreFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    importSemaphoreFdInfo.fd         = fd;

    ANGLE_VK_TRY(contextVk, vkImportSemaphoreFdKHR(renderer->getDevice(), &importSemaphoreFdInfo));

    return angle::Result::Continue;
}

}  // namespace rx
