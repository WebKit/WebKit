//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VulkanExternalHelper.cpp : Helper for allocating & managing vulkan external objects.

#include "test_utils/VulkanExternalHelper.h"

#include <vulkan/vulkan.h>
#include <vector>

#include "common/bitset_utils.h"
#include "common/debug.h"

namespace angle
{

namespace
{

constexpr VkImageUsageFlags kRequiredImageUsageFlags =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

std::vector<VkExtensionProperties> EnumerateInstanceExtensionProperties(const char *layerName)
{
    uint32_t instanceExtensionCount;
    VkResult result =
        vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount, nullptr);
    ASSERT(result == VK_SUCCESS);
    std::vector<VkExtensionProperties> instanceExtensionProperties(instanceExtensionCount);
    result = vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount,
                                                    instanceExtensionProperties.data());
    ASSERT(result == VK_SUCCESS);
    return instanceExtensionProperties;
}

std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance)
{
    uint32_t physicalDeviceCount;
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    ASSERT(result == VK_SUCCESS);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    return physicalDevices;
}

std::vector<VkExtensionProperties> EnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice,
    const char *layerName)
{
    uint32_t deviceExtensionCount;
    VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName,
                                                           &deviceExtensionCount, nullptr);
    ASSERT(result == VK_SUCCESS);
    std::vector<VkExtensionProperties> deviceExtensionProperties(deviceExtensionCount);
    result = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, &deviceExtensionCount,
                                                  deviceExtensionProperties.data());
    ASSERT(result == VK_SUCCESS);
    return deviceExtensionProperties;
}

std::vector<VkQueueFamilyProperties> GetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice)
{
    uint32_t queueFamilyPropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> physicalDeviceQueueFamilyProperties(
        queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount,
                                             physicalDeviceQueueFamilyProperties.data());
    return physicalDeviceQueueFamilyProperties;
}

bool HasExtension(const std::vector<VkExtensionProperties> instanceExtensions,
                  const char *extensionName)
{
    for (const auto &extensionProperties : instanceExtensions)
    {
        if (!strcmp(extensionProperties.extensionName, extensionName))
            return true;
    }

    return false;
}

bool HasExtension(const std::vector<const char *> enabledExtensions, const char *extensionName)
{
    for (const char *enabledExtension : enabledExtensions)
    {
        if (!strcmp(enabledExtension, extensionName))
            return true;
    }

    return false;
}

uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties &memoryProperties,
                        uint32_t memoryTypeBits,
                        VkMemoryPropertyFlags requiredMemoryPropertyFlags)
{
    for (size_t memoryIndex : angle::BitSet32<32>(memoryTypeBits))
    {
        ASSERT(memoryIndex < memoryProperties.memoryTypeCount);

        if ((memoryProperties.memoryTypes[memoryIndex].propertyFlags &
             requiredMemoryPropertyFlags) == requiredMemoryPropertyFlags)
        {
            return static_cast<uint32_t>(memoryIndex);
        }
    }

    return UINT32_MAX;
}

}  // namespace

VulkanExternalHelper::VulkanExternalHelper() {}

VulkanExternalHelper::~VulkanExternalHelper()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
        vkDestroyDevice(mDevice, nullptr);

        mDevice        = VK_NULL_HANDLE;
        mGraphicsQueue = VK_NULL_HANDLE;
    }

    if (mInstance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(mInstance, nullptr);

        mInstance = VK_NULL_HANDLE;
    }
}

void VulkanExternalHelper::initialize()
{
    ASSERT(mInstance == VK_NULL_HANDLE);

    std::vector<VkExtensionProperties> instanceExtensionProperties =
        EnumerateInstanceExtensionProperties(nullptr);

    std::vector<const char *> requestedInstanceExtensions = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME};

    std::vector<const char *> enabledInstanceExtensions;

    for (const char *extensionName : requestedInstanceExtensions)
    {
        if (HasExtension(instanceExtensionProperties, extensionName))
        {
            enabledInstanceExtensions.push_back(extensionName);
        }
    }

    VkApplicationInfo applicationInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_APPLICATION_INFO,
        /* .pNext = */ nullptr,
        /* .pApplicationName = */ "ANGLE Tests",
        /* .applicationVersion = */ 1,
        /* .pEngineName = */ nullptr,
        /* .engineVersion = */ 0,
        /* .apiVersion = */ VK_API_VERSION_1_0,
    };

    uint32_t enabledInstanceExtensionCount =
        static_cast<uint32_t>(enabledInstanceExtensions.size());

    VkInstanceCreateInfo instanceCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ 0,
        /* .pApplicationInfo = */ &applicationInfo,
        /* .enabledLayerCount = */ 0,
        /* .ppEnabledLayerNames = */ nullptr,
        /* .enabledExtensionCount = */ enabledInstanceExtensionCount,
        /* .ppEnabledExtensionName = */ enabledInstanceExtensions.data(),
    };

    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance);
    ASSERT(result == VK_SUCCESS);
    ASSERT(mInstance != VK_NULL_HANDLE);

    std::vector<VkPhysicalDevice> physicalDevices = EnumeratePhysicalDevices(mInstance);
    ASSERT(physicalDevices.size() > 0);

    mPhysicalDevice = physicalDevices[0];

    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);

    std::vector<VkExtensionProperties> deviceExtensionProperties =
        EnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr);

    std::vector<const char *> requestedDeviceExtensions = {
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    };

    std::vector<const char *> enabledDeviceExtensions;

    for (const char *extensionName : requestedDeviceExtensions)
    {
        if (HasExtension(deviceExtensionProperties, extensionName))
        {
            enabledDeviceExtensions.push_back(extensionName);
        }
    }

    std::vector<VkQueueFamilyProperties> queueFamilyProperties =
        GetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice);

    for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
    {
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            mGraphicsQueueFamilyIndex = i;
        }
    }
    ASSERT(mGraphicsQueueFamilyIndex != UINT32_MAX);

    constexpr uint32_t kQueueCreateInfoCount           = 1;
    constexpr uint32_t kGraphicsQueueCount             = 1;
    float graphicsQueuePriorities[kGraphicsQueueCount] = {0.f};

    VkDeviceQueueCreateInfo queueCreateInfos[kQueueCreateInfoCount] = {
        /* [0] = */ {
            /* .sType = */ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            /* .pNext = */ nullptr,
            /* .flags = */ 0,
            /* .queueFamilyIndex = */ mGraphicsQueueFamilyIndex,
            /* .queueCount = */ 1,
            /* .pQueuePriorities = */ graphicsQueuePriorities,
        },
    };

    uint32_t enabledDeviceExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());

    VkDeviceCreateInfo deviceCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ 0,
        /* .queueCreateInfoCount = */ kQueueCreateInfoCount,
        /* .pQueueCreateInfos = */ queueCreateInfos,
        /* .enabledLayerCount = */ 0,
        /* .ppEnabledLayerNames = */ nullptr,
        /* .enabledExtensionCount = */ enabledDeviceExtensionCount,
        /* .ppEnabledExtensionName = */ enabledDeviceExtensions.data(),
        /* .pEnabledFeatures = */ nullptr,
    };

    result = vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice);
    ASSERT(result == VK_SUCCESS);
    ASSERT(mDevice != VK_NULL_HANDLE);

    constexpr uint32_t kGraphicsQueueIndex = 0;
    static_assert(kGraphicsQueueIndex < kGraphicsQueueCount, "must be in range");
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamilyIndex, kGraphicsQueueIndex, &mGraphicsQueue);
    ASSERT(mGraphicsQueue != VK_NULL_HANDLE);

    mHasExternalMemoryFd =
        HasExtension(enabledDeviceExtensions, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    mHasExternalSemaphoreFd =
        HasExtension(enabledDeviceExtensions, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);

    vkGetPhysicalDeviceImageFormatProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            vkGetInstanceProcAddr(mInstance, "vkGetPhysicalDeviceImageFormatProperties2"));
    vkGetMemoryFdKHR = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetInstanceProcAddr(mInstance, "vkGetMemoryFdKHR"));
}

bool VulkanExternalHelper::canCreateImageOpaqueFd(VkFormat format,
                                                  VkImageType type,
                                                  VkImageTiling tiling) const
{
    if (!mHasExternalMemoryFd)
    {
        return false;
    }

    VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        /* .pNext = */ nullptr,
        /* .handleType = */ VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        /* .pNext = */ &externalImageFormatInfo,
        /* .format = */ format,
        /* .type = */ type,
        /* .tiling = */ tiling,
        /* .usage = */ kRequiredImageUsageFlags,
        /* .flags = */ 0,
    };

    VkExternalImageFormatProperties externalImageFormatProperties = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
        /* .pNext = */ nullptr,
    };

    VkImageFormatProperties2 imageFormatProperties = {
        /* .sType = */ VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        /* .pNext = */ &externalImageFormatProperties};

    VkResult result = vkGetPhysicalDeviceImageFormatProperties2(mPhysicalDevice, &imageFormatInfo,
                                                                &imageFormatProperties);
    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        return false;
    }

    ASSERT(result == VK_SUCCESS);

    constexpr VkExternalMemoryFeatureFlags kUnsupportedFeatures =
        VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT;
    constexpr VkExternalMemoryFeatureFlags kRequiredFeatures =
        VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
    if (externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures &
        kUnsupportedFeatures)
    {
        return false;
    }
    if (!(externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures &
          kRequiredFeatures))
    {
        return false;
    }

    return true;
}

VkResult VulkanExternalHelper::createImage2DOpaqueFd(VkFormat format,
                                                     VkExtent3D extent,
                                                     VkImage *imageOut,
                                                     VkDeviceMemory *deviceMemoryOut,
                                                     VkDeviceSize *deviceMemorySizeOut)
{
    VkExternalMemoryImageCreateInfoKHR externalMemoryImageCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .handleTypes = */ VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };

    VkImageCreateInfo imageCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        /* .pNext = */ &externalMemoryImageCreateInfo,
        /* .flags = */ 0,
        /* .imageType = */ VK_IMAGE_TYPE_2D,
        /* .format = */ format,
        /* .extent = */ extent,
        /* .mipLevels = */ 1,
        /* .arrayLayers = */ 1,
        /* .samples = */ VK_SAMPLE_COUNT_1_BIT,
        /* .tiling = */ VK_IMAGE_TILING_OPTIMAL,
        /* .usage = */ kRequiredImageUsageFlags,
        /* .sharingMode = */ VK_SHARING_MODE_EXCLUSIVE,
        /* .queueFamilyIndexCount = */ 0,
        /* .pQueueFamilyIndices = */ nullptr,
        /* initialLayout = */ VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage image   = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(mDevice, &imageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkMemoryPropertyFlags requestedMemoryPropertyFlags = 0;
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(mDevice, image, &memoryRequirements);
    uint32_t memoryTypeIndex = FindMemoryType(mMemoryProperties, memoryRequirements.memoryTypeBits,
                                              requestedMemoryPropertyFlags);
    ASSERT(memoryTypeIndex != UINT32_MAX);
    VkDeviceSize deviceMemorySize = memoryRequirements.size;

    VkExportMemoryAllocateInfo exportMemoryAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        /* .pNext = */ nullptr,
        /* .handleTypes = */ VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };

    VkMemoryAllocateInfo memoryAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        /* .pNext = */ &exportMemoryAllocateInfo,
        /* .allocationSize = */ deviceMemorySize,
        /* .memoryTypeIndex = */ memoryTypeIndex,
    };

    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    result = vkAllocateMemory(mDevice, &memoryAllocateInfo, nullptr, &deviceMemory);
    if (result != VK_SUCCESS)
    {
        vkDestroyImage(mDevice, image, nullptr);
        return result;
    }

    VkDeviceSize memoryOffset = 0;
    result                    = vkBindImageMemory(mDevice, image, deviceMemory, memoryOffset);
    if (result != VK_SUCCESS)
    {
        vkFreeMemory(mDevice, deviceMemory, nullptr);
        vkDestroyImage(mDevice, image, nullptr);
        return result;
    }

    *imageOut            = image;
    *deviceMemoryOut     = deviceMemory;
    *deviceMemorySizeOut = deviceMemorySize;

    return VK_SUCCESS;
}

VkResult VulkanExternalHelper::exportMemoryOpaqueFd(VkDeviceMemory deviceMemory, int *fd)
{
    VkMemoryGetFdInfoKHR memoryGetFdInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        /* .pNext = */ nullptr,
        /* .memory = */ deviceMemory,
        /* .handleType = */ VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };

    return vkGetMemoryFdKHR(mDevice, &memoryGetFdInfo, fd);
}

}  // namespace angle
