//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanExternalHelper.h : Helper for allocating & managing vulkan external objects.

#ifndef ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_
#define ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_

#include <vulkan/vulkan.h>

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

    // VK_KHR_external_memory_fd
    bool canCreateImageOpaqueFd(VkFormat format, VkImageType type, VkImageTiling tiling) const;
    VkResult createImage2DOpaqueFd(VkFormat format,
                                   VkExtent3D extent,
                                   VkImage *imageOut,
                                   VkDeviceMemory *deviceMemoryOut,
                                   VkDeviceSize *deviceMemorySizeOut);
    VkResult exportMemoryOpaqueFd(VkDeviceMemory deviceMemory, int *fd);

  private:
    VkInstance mInstance             = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice                 = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue           = VK_NULL_HANDLE;

    VkPhysicalDeviceMemoryProperties mMemoryProperties = {};

    uint32_t mGraphicsQueueFamilyIndex = UINT32_MAX;

    bool mHasExternalMemoryFd    = false;
    bool mHasExternalSemaphoreFd = false;
    PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2 =
        nullptr;
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR = nullptr;
};

}  // namespace angle

#endif  // ANGLE_TESTS_TESTUTILS_VULKANEXTERNALHELPER_H_
