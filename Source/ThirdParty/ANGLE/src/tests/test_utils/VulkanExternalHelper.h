//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanExternalHelper.h : Helper for allocating & managing vulkan external objects.

#ifndef ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_
#define ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_

#include "common/vulkan/vk_headers.h"
#include "vulkan/vulkan_fuchsia_ext.h"

namespace angle
{

class VulkanExternalHelper
{
  public:
    VulkanExternalHelper();
    ~VulkanExternalHelper();

    void initialize(bool useSwiftshader, bool enableValidationLayers);

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

    // Performs a queue ownership transfer to VK_QUEUE_FAMILY_EXTERNAL on an
    // image owned by our instance. The current image layout must be |oldLayout|
    // and will be in |newLayout| after the memory barrier. |semaphore|
    // will be signaled upon completion of the release operation.
    void releaseImageAndSignalSemaphore(VkImage image,
                                        VkImageLayout oldLayout,
                                        VkImageLayout newLayout,
                                        VkSemaphore semaphore);

    // Performs a queue ownership transfer from VK_QUEUE_FAMILY_EXTERNAL on an
    // image owned by an external instance. The current image layout must be
    // |oldLayout| and will be in |newLayout| after the memory barrier. The
    // barrier will wait for |semaphore|.
    void waitSemaphoreAndAcquireImage(VkImage image,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout,
                                      VkSemaphore semaphore);

    // Copies pixels out of an image. Currently only VK_FORMAT_R8G8B8A8_UNORM
    // and VK_FORMAT_B8G8R8A8_UNORM formats are supported.
    void readPixels(VkImage srcImage,
                    VkImageLayout srcImageLayout,
                    VkFormat srcImageFormat,
                    VkOffset3D imageOffset,
                    VkExtent3D imageExtent,
                    void *pixels,
                    size_t pixelsSize);

  private:
    VkInstance mInstance             = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice                 = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue           = VK_NULL_HANDLE;
    VkCommandPool mCommandPool       = VK_NULL_HANDLE;

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
        vkGetPhysicalDeviceExternalSemaphorePropertiesKHR                   = nullptr;
    PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA       = nullptr;
    PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA = nullptr;
};

}  // namespace angle

#endif  // ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_
