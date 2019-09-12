// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MemoryObjectVk.cpp: Defines the class interface for MemoryObjectVk, implementing
// MemoryObjectImpl.

#include "libANGLE/renderer/vulkan/MemoryObjectVk.h"

#include <vulkan/vulkan.h>

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

#if !defined(ANGLE_PLATFORM_WINDOWS)
#    include <unistd.h>
#else
#    include <io.h>
#endif

namespace rx
{

namespace
{

constexpr int kInvalidFd = -1;

#if defined(ANGLE_PLATFORM_WINDOWS)
int close(int fd)
{
    return _close(fd);
}
#endif

}  // namespace

MemoryObjectVk::MemoryObjectVk() : mSize(0), mFd(kInvalidFd) {}

MemoryObjectVk::~MemoryObjectVk() = default;

void MemoryObjectVk::onDestroy(const gl::Context *context)
{
    if (mFd != kInvalidFd)
    {
        close(mFd);
        mFd = kInvalidFd;
    }
}

angle::Result MemoryObjectVk::importFd(gl::Context *context,
                                       GLuint64 size,
                                       gl::HandleType handleType,
                                       GLint fd)
{
    switch (handleType)
    {
        case gl::HandleType::OpaqueFd:
            return importOpaqueFd(context, size, fd);

        default:
            UNREACHABLE();
            return angle::Result::Stop;
    }
}

angle::Result MemoryObjectVk::importOpaqueFd(gl::Context *context, GLuint64 size, GLint fd)
{
    ASSERT(mFd == kInvalidFd);
    mFd   = fd;
    mSize = size;
    return angle::Result::Continue;
}

angle::Result MemoryObjectVk::createImage(const gl::Context *context,
                                          gl::TextureType type,
                                          size_t levels,
                                          GLenum internalFormat,
                                          const gl::Extents &size,
                                          GLuint64 offset,
                                          vk::ImageHelper *image)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    const vk::Format &vkFormat = renderer->getFormat(internalFormat);

    // All supported usage flags must be specified.
    // See EXT_external_objects issue 13.
    VkImageUsageFlags imageUsageFlags =
        vk::GetMaximalImageUsageFlags(renderer, vkFormat.vkImageFormat);

    VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
    externalMemoryImageCreateInfo.sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    ANGLE_TRY(image->initExternal(contextVk, type, size, vkFormat, 1, imageUsageFlags,
                                  vk::ImageLayout::ExternalPreInitialized,
                                  &externalMemoryImageCreateInfo, levels, 1));

    VkMemoryRequirements externalMemoryRequirements;
    image->getImage().getMemoryRequirements(renderer->getDevice(), &externalMemoryRequirements);

    ASSERT(mFd != -1);
    VkImportMemoryFdInfoKHR importMemoryFdInfo = {};
    importMemoryFdInfo.sType                   = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    importMemoryFdInfo.handleType              = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    importMemoryFdInfo.fd                      = dup(mFd);

    // TODO(jmadill, spang): Memory sub-allocation. http://anglebug.com/2162
    ASSERT(offset == 0);
    ASSERT(externalMemoryRequirements.size == mSize);

    VkMemoryPropertyFlags flags = 0;
    ANGLE_TRY(image->initExternalMemory(contextVk, renderer->getMemoryProperties(),
                                        externalMemoryRequirements, &importMemoryFdInfo,
                                        VK_QUEUE_FAMILY_EXTERNAL, flags));

    return angle::Result::Continue;
}

}  // namespace rx
