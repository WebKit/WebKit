//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanExternalHelper.h : Helper for allocating & managing vulkan external objects.

#ifndef ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_
#define ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_

#include "volk.h"
#include "vulkan/vulkan_fuchsia_ext.h"

namespace angle
{

class VulkanExternalHelper
{
  public:
    VulkanExternalHelper();
    ~VulkanExternalHelper();

    void initialize();

    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    VkDevice getDevice() const { return mDevice; }
    VkQueue getGraphicsQueue() const { return mGraphicsQueue; }

    bool canCreateImageExternal(VkFormat format,
                                VkImageType type,
                                VkImageTiling tiling,
                                VkExternalMemoryHandleTypeFlagBits handleType) const;
    VkResult createImage2DExternal(VkFormat format,
                                   VkExtent3D extent,
                                   VkExternalMemoryHandleTypeFlags handleTypes,
                                   VkImage *imageOut,
                                   VkDeviceMemory *deviceMemoryOut,
                                   VkDeviceSize *deviceMemorySizeOut);

    // VK_KHR_external_memory_fd
    bool canCreateImageOpaqueFd(VkFormat format, VkImageType type, VkImageTiling tiling) const;
    VkResult createImage2DOpaqueFd(VkFormat format,
                                   VkExtent3D extent,
                                   VkImage *imageOut,
                                   VkDeviceMemory *deviceMemoryOut,
                                   VkDeviceSize *deviceMemorySizeOut);
    VkResult exportMemoryOpaqueFd(VkDeviceMemory deviceMemory, int *fd);

    // VK_FUCHSIA_external_memory
    bool canCreateImageZirconVmo(VkFormat format, VkImageType type, VkImageTiling tiling) const;
    VkResult createImage2DZirconVmo(VkFormat format,
                                    VkExtent3D extent,
                                    VkImage *imageOut,
                                    VkDeviceMemory *deviceMemoryOut,
                                    VkDeviceSize *deviceMemorySizeOut);
    VkResult exportMemoryZirconVmo(VkDeviceMemory deviceMemory, zx_handle_t *vmo);

    // VK_KHR_external_semaphore_fd
    bool canCreateSemaphoreOpaqueFd() const;
    VkResult createSemaphoreOpaqueFd(VkSemaphore *semaphore);
    VkResult exportSemaphoreOpaqueFd(VkSemaphore semaphore, int *fd);

    // VK_FUCHSIA_external_semaphore
    bool canCreateSemaphoreZirconEvent() const;
    VkResult createSemaphoreZirconEvent(VkSemaphore *semaphore);
    VkResult exportSemaphoreZirconEvent(VkSemaphore semaphore, zx_handle_t *event);

  private:
    VkInstance mInstance             = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice                 = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue           = VK_NULL_HANDLE;

    VkPhysicalDeviceMemoryProperties mMemoryProperties = {};

    uint32_t mGraphicsQueueFamilyIndex = UINT32_MAX;

    bool mHasExternalMemoryFd         = false;
    bool mHasExternalMemoryFuchsia    = false;
    bool mHasExternalSemaphoreFd      = false;
    bool mHasExternalSemaphoreFuchsia = false;
    PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2 =
        nullptr;
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR       = nullptr;
    PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR = nullptr;
    PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR
        vkGetPhysicalDeviceExternalSemaphorePropertiesKHR             = nullptr;
    PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA = nullptr;
};

}  // namespace angle

#endif  // ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_
