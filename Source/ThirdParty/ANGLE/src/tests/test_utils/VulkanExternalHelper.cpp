//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VulkanExternalHelper.cpp : Helper for allocating & managing vulkan external objects.

#include "test_utils/VulkanExternalHelper.h"

#include <vector>

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/system_utils.h"
#include "common/vulkan/vulkan_icd.h"

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

void ImageMemoryBarrier(VkCommandBuffer commandBuffer,
                        VkImage image,
                        uint32_t srcQueueFamilyIndex,
                        uint32_t dstQueueFamilyIndex,
                        VkImageLayout oldLayout,
                        VkImageLayout newLayout)
{
    const VkImageMemoryBarrier imageMemoryBarriers[] = {
        /* [0] = */ {/* .sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                     /* .pNext = */ nullptr,
                     /* .srcAccessMask = */ VK_ACCESS_MEMORY_WRITE_BIT,
                     /* .dstAccessMask = */ VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                     /* .oldLayout = */ oldLayout,
                     /* .newLayout = */ newLayout,
                     /* .srcQueueFamilyIndex = */ srcQueueFamilyIndex,
                     /* .dstQueueFamilyIndex = */ dstQueueFamilyIndex,
                     /* .image = */ image,
                     /* .subresourceRange = */
                     {
                         /* .aspectMask = */ VK_IMAGE_ASPECT_COLOR_BIT,
                         /* .basicMiplevel = */ 0,
                         /* .levelCount = */ 1,
                         /* .baseArrayLayer = */ 0,
                         /* .layerCount = */ 1,
                     }}};
    const uint32_t imageMemoryBarrierCount = std::extent<decltype(imageMemoryBarriers)>();

    constexpr VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    constexpr VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkDependencyFlags dependencyFlags     = 0;

    vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0,
                         nullptr, imageMemoryBarrierCount, imageMemoryBarriers);
}

}  // namespace

VulkanExternalHelper::VulkanExternalHelper() {}

VulkanExternalHelper::~VulkanExternalHelper()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }

    if (mCommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    }

    if (mDevice != VK_NULL_HANDLE)
    {
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

void VulkanExternalHelper::initialize(bool useSwiftshader, bool enableValidationLayers)
{
    vk::ICD icd = useSwiftshader ? vk::ICD::SwiftShader : vk::ICD::Default;

    vk::ScopedVkLoaderEnvironment scopedEnvironment(enableValidationLayers, icd);

    ASSERT(mInstance == VK_NULL_HANDLE);
    VkResult result = VK_SUCCESS;
#if ANGLE_SHARED_LIBVULKAN
    result = volkInitialize();
    ASSERT(result == VK_SUCCESS);
#endif  // ANGLE_SHARED_LIBVULKAN
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

    std::vector<const char *> enabledLayerNames;
    if (enableValidationLayers)
    {
        enabledLayerNames.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo instanceCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ 0,
        /* .pApplicationInfo = */ &applicationInfo,
        /* .enabledLayerCount = */ enabledLayerNames.size(),
        /* .ppEnabledLayerNames = */ enabledLayerNames.data(),
        /* .enabledExtensionCount = */ enabledInstanceExtensionCount,
        /* .ppEnabledExtensionName = */ enabledInstanceExtensions.data(),
    };

    result = vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance);
    ASSERT(result == VK_SUCCESS);
    ASSERT(mInstance != VK_NULL_HANDLE);
#if ANGLE_SHARED_LIBVULKAN
    volkLoadInstance(mInstance);
#endif  // ANGLE_SHARED_LIBVULKAN

    std::vector<VkPhysicalDevice> physicalDevices = EnumeratePhysicalDevices(mInstance);

    ASSERT(physicalDevices.size() > 0);

    VkPhysicalDeviceProperties physicalDeviceProperties;
    ChoosePhysicalDevice(physicalDevices, icd, &mPhysicalDevice, &physicalDeviceProperties);

    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);

    std::vector<VkExtensionProperties> deviceExtensionProperties =
        EnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr);

    std::vector<const char *> requestedDeviceExtensions = {
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,  VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,     VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME, VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
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
#if ANGLE_SHARED_LIBVULKAN
    volkLoadDevice(mDevice);
#endif  // ANGLE_SHARED_LIBVULKAN

    constexpr uint32_t kGraphicsQueueIndex = 0;
    static_assert(kGraphicsQueueIndex < kGraphicsQueueCount, "must be in range");
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamilyIndex, kGraphicsQueueIndex, &mGraphicsQueue);
    ASSERT(mGraphicsQueue != VK_NULL_HANDLE);

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ 0,
        /* .queueFamilyIndex = */ mGraphicsQueueFamilyIndex,
    };
    result = vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mCommandPool);
    ASSERT(result == VK_SUCCESS);

    mHasExternalMemoryFd =
        HasExtension(enabledDeviceExtensions, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    mHasExternalSemaphoreFd =
        HasExtension(enabledDeviceExtensions, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    mHasExternalMemoryFuchsia =
        HasExtension(enabledDeviceExtensions, VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME);
    mHasExternalSemaphoreFuchsia =
        HasExtension(enabledDeviceExtensions, VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME);

    vkGetPhysicalDeviceImageFormatProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            vkGetInstanceProcAddr(mInstance, "vkGetPhysicalDeviceImageFormatProperties2"));
    vkGetMemoryFdKHR = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetInstanceProcAddr(mInstance, "vkGetMemoryFdKHR"));
    ASSERT(!mHasExternalMemoryFd || vkGetMemoryFdKHR);
    vkGetSemaphoreFdKHR = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        vkGetInstanceProcAddr(mInstance, "vkGetSemaphoreFdKHR"));
    ASSERT(!mHasExternalSemaphoreFd || vkGetSemaphoreFdKHR);
    vkGetPhysicalDeviceExternalSemaphorePropertiesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR>(
            vkGetInstanceProcAddr(mInstance, "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR"));
    vkGetMemoryZirconHandleFUCHSIA = reinterpret_cast<PFN_vkGetMemoryZirconHandleFUCHSIA>(
        vkGetInstanceProcAddr(mInstance, "vkGetMemoryZirconHandleFUCHSIA"));
    ASSERT(!mHasExternalMemoryFuchsia || vkGetMemoryZirconHandleFUCHSIA);
    vkGetSemaphoreZirconHandleFUCHSIA = reinterpret_cast<PFN_vkGetSemaphoreZirconHandleFUCHSIA>(
        vkGetInstanceProcAddr(mInstance, "vkGetSemaphoreZirconHandleFUCHSIA"));
    ASSERT(!mHasExternalSemaphoreFuchsia || vkGetSemaphoreZirconHandleFUCHSIA);
}

bool VulkanExternalHelper::canCreateImageExternal(
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkExternalMemoryHandleTypeFlagBits handleType) const
{
    VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        /* .pNext = */ nullptr,
        /* .handleType = */ handleType,
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

    constexpr VkExternalMemoryFeatureFlags kRequiredFeatures =
        VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
    if ((externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures &
         kRequiredFeatures) != kRequiredFeatures)
    {
        return false;
    }

    return true;
}

VkResult VulkanExternalHelper::createImage2DExternal(VkFormat format,
                                                     VkExtent3D extent,
                                                     VkExternalMemoryHandleTypeFlags handleTypes,
                                                     VkImage *imageOut,
                                                     VkDeviceMemory *deviceMemoryOut,
                                                     VkDeviceSize *deviceMemorySizeOut)
{
    VkExternalMemoryImageCreateInfoKHR externalMemoryImageCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .handleTypes = */ handleTypes,
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
        /* .handleTypes = */ handleTypes,
    };
    VkMemoryDedicatedAllocateInfoKHR memoryDedicatedAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
        /* .pNext = */ &exportMemoryAllocateInfo,
        /* .image = */ image,
    };
    VkMemoryAllocateInfo memoryAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        /* .pNext = */ &memoryDedicatedAllocateInfo,
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

bool VulkanExternalHelper::canCreateImageOpaqueFd(VkFormat format,
                                                  VkImageType type,
                                                  VkImageTiling tiling) const
{
    if (!mHasExternalMemoryFd)
    {
        return false;
    }

    return canCreateImageExternal(format, type, tiling,
                                  VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
}

VkResult VulkanExternalHelper::createImage2DOpaqueFd(VkFormat format,
                                                     VkExtent3D extent,
                                                     VkImage *imageOut,
                                                     VkDeviceMemory *deviceMemoryOut,
                                                     VkDeviceSize *deviceMemorySizeOut)
{
    return createImage2DExternal(format, extent, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                                 imageOut, deviceMemoryOut, deviceMemorySizeOut);
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

bool VulkanExternalHelper::canCreateImageZirconVmo(VkFormat format,
                                                   VkImageType type,
                                                   VkImageTiling tiling) const
{
    if (!mHasExternalMemoryFuchsia)
    {
        return false;
    }

    return canCreateImageExternal(format, type, tiling,
                                  VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA);
}

VkResult VulkanExternalHelper::createImage2DZirconVmo(VkFormat format,
                                                      VkExtent3D extent,
                                                      VkImage *imageOut,
                                                      VkDeviceMemory *deviceMemoryOut,
                                                      VkDeviceSize *deviceMemorySizeOut)
{
    return createImage2DExternal(format, extent,
                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA,
                                 imageOut, deviceMemoryOut, deviceMemorySizeOut);
}

VkResult VulkanExternalHelper::exportMemoryZirconVmo(VkDeviceMemory deviceMemory, zx_handle_t *vmo)
{
    VkMemoryGetZirconHandleInfoFUCHSIA memoryGetZirconHandleInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_TEMP_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA,
        /* .pNext = */ nullptr,
        /* .memory = */ deviceMemory,
        /* .handleType = */ VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA,
    };

    return vkGetMemoryZirconHandleFUCHSIA(mDevice, &memoryGetZirconHandleInfo, vmo);
}

bool VulkanExternalHelper::canCreateSemaphoreOpaqueFd() const
{
    if (!mHasExternalSemaphoreFd || !vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)
    {
        return false;
    }

    VkPhysicalDeviceExternalSemaphoreInfo externalSemaphoreInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
        /* .pNext = */ nullptr,
        /* .handleType = */ VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
    };

    VkExternalSemaphoreProperties externalSemaphoreProperties = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
    };
    vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(mPhysicalDevice, &externalSemaphoreInfo,
                                                      &externalSemaphoreProperties);

    constexpr VkExternalSemaphoreFeatureFlags kRequiredFeatures =
        VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;

    if ((externalSemaphoreProperties.externalSemaphoreFeatures & kRequiredFeatures) !=
        kRequiredFeatures)
    {
        return false;
    }

    return true;
}

VkResult VulkanExternalHelper::createSemaphoreOpaqueFd(VkSemaphore *semaphore)
{
    VkExportSemaphoreCreateInfo exportSemaphoreCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .handleTypes = */ VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        /* .pNext = */ &exportSemaphoreCreateInfo,
        /* .flags = */ 0,
    };

    return vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, semaphore);
}

VkResult VulkanExternalHelper::exportSemaphoreOpaqueFd(VkSemaphore semaphore, int *fd)
{
    VkSemaphoreGetFdInfoKHR semaphoreGetFdInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
        /* .pNext = */ nullptr,
        /* .semaphore = */ semaphore,
        /* .handleType = */ VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
    };

    return vkGetSemaphoreFdKHR(mDevice, &semaphoreGetFdInfo, fd);
}

bool VulkanExternalHelper::canCreateSemaphoreZirconEvent() const
{
    if (!mHasExternalSemaphoreFuchsia || !vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)
    {
        return false;
    }

    VkPhysicalDeviceExternalSemaphoreInfo externalSemaphoreInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
        /* .pNext = */ nullptr,
        /* .handleType = */ VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA,
    };

    VkExternalSemaphoreProperties externalSemaphoreProperties = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
    };
    vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(mPhysicalDevice, &externalSemaphoreInfo,
                                                      &externalSemaphoreProperties);

    constexpr VkExternalSemaphoreFeatureFlags kRequiredFeatures =
        VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;

    if ((externalSemaphoreProperties.externalSemaphoreFeatures & kRequiredFeatures) !=
        kRequiredFeatures)
    {
        return false;
    }

    return true;
}

VkResult VulkanExternalHelper::createSemaphoreZirconEvent(VkSemaphore *semaphore)
{
    VkExportSemaphoreCreateInfo exportSemaphoreCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .handleTypes = */ VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        /* .pNext = */ &exportSemaphoreCreateInfo,
        /* .flags = */ 0,
    };

    return vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, semaphore);
}

VkResult VulkanExternalHelper::exportSemaphoreZirconEvent(VkSemaphore semaphore, zx_handle_t *event)
{
    VkSemaphoreGetZirconHandleInfoFUCHSIA semaphoreGetZirconHandleInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_TEMP_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA,
        /* .pNext = */ nullptr,
        /* .semaphore = */ semaphore,
        /* .handleType = */ VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA,
    };

    return vkGetSemaphoreZirconHandleFUCHSIA(mDevice, &semaphoreGetZirconHandleInfo, event);
}

void VulkanExternalHelper::releaseImageAndSignalSemaphore(VkImage image,
                                                          VkImageLayout oldLayout,
                                                          VkImageLayout newLayout,
                                                          VkSemaphore semaphore)
{
    VkResult result;

    VkCommandBuffer commandBuffers[]                      = {VK_NULL_HANDLE};
    constexpr uint32_t commandBufferCount                 = std::extent<decltype(commandBuffers)>();
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        /* .pNext = */ nullptr,
        /* .commandPool = */ mCommandPool,
        /* .level = */ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        /* .commandBufferCount = */ commandBufferCount,
    };

    result = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, commandBuffers);
    ASSERT(result == VK_SUCCESS);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        /* .pInheritanceInfo = */ nullptr,
    };
    result = vkBeginCommandBuffer(commandBuffers[0], &commandBufferBeginInfo);
    ASSERT(result == VK_SUCCESS);

    ImageMemoryBarrier(commandBuffers[0], image, mGraphicsQueueFamilyIndex,
                       VK_QUEUE_FAMILY_EXTERNAL, oldLayout, newLayout);

    result = vkEndCommandBuffer(commandBuffers[0]);
    ASSERT(result == VK_SUCCESS);

    const VkSemaphore signalSemaphores[] = {
        semaphore,
    };
    constexpr uint32_t signalSemaphoreCount = std::extent<decltype(signalSemaphores)>();

    const VkSubmitInfo submits[] = {
        /* [0] = */ {
            /* .sType */ VK_STRUCTURE_TYPE_SUBMIT_INFO,
            /* .pNext = */ nullptr,
            /* .waitSemaphoreCount = */ 0,
            /* .pWaitSemaphores = */ nullptr,
            /* .pWaitDstStageMask = */ nullptr,
            /* .commandBufferCount = */ commandBufferCount,
            /* .pCommandBuffers = */ commandBuffers,
            /* .signalSemaphoreCount = */ signalSemaphoreCount,
            /* .pSignalSemaphores = */ signalSemaphores,
        },
    };
    constexpr uint32_t submitCount = std::extent<decltype(submits)>();

    const VkFence fence = VK_NULL_HANDLE;
    result              = vkQueueSubmit(mGraphicsQueue, submitCount, submits, fence);
    ASSERT(result == VK_SUCCESS);
}

void VulkanExternalHelper::waitSemaphoreAndAcquireImage(VkImage image,
                                                        VkImageLayout oldLayout,
                                                        VkImageLayout newLayout,
                                                        VkSemaphore semaphore)
{
    VkResult result;

    VkCommandBuffer commandBuffers[]                      = {VK_NULL_HANDLE};
    constexpr uint32_t commandBufferCount                 = std::extent<decltype(commandBuffers)>();
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        /* .pNext = */ nullptr,
        /* .commandPool = */ mCommandPool,
        /* .level = */ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        /* .commandBufferCount = */ commandBufferCount,
    };

    result = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, commandBuffers);
    ASSERT(result == VK_SUCCESS);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        /* .pInheritanceInfo = */ nullptr,
    };
    result = vkBeginCommandBuffer(commandBuffers[0], &commandBufferBeginInfo);
    ASSERT(result == VK_SUCCESS);

    ImageMemoryBarrier(commandBuffers[0], image, VK_QUEUE_FAMILY_EXTERNAL,
                       mGraphicsQueueFamilyIndex, oldLayout, newLayout);

    result = vkEndCommandBuffer(commandBuffers[0]);
    ASSERT(result == VK_SUCCESS);

    const VkSemaphore waitSemaphores[] = {
        semaphore,
    };
    const VkPipelineStageFlags waitDstStageMasks[] = {
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    };
    constexpr uint32_t waitSemaphoreCount    = std::extent<decltype(waitSemaphores)>();
    constexpr uint32_t waitDstStageMaskCount = std::extent<decltype(waitDstStageMasks)>();
    static_assert(waitSemaphoreCount == waitDstStageMaskCount,
                  "waitSemaphores and waitDstStageMasks must be the same length");

    const VkSubmitInfo submits[] = {
        /* [0] = */ {
            /* .sType */ VK_STRUCTURE_TYPE_SUBMIT_INFO,
            /* .pNext = */ nullptr,
            /* .waitSemaphoreCount = */ waitSemaphoreCount,
            /* .pWaitSemaphores = */ waitSemaphores,
            /* .pWaitDstStageMask = */ waitDstStageMasks,
            /* .commandBufferCount = */ commandBufferCount,
            /* .pCommandBuffers = */ commandBuffers,
            /* .signalSemaphoreCount = */ 0,
            /* .pSignalSemaphores = */ nullptr,
        },
    };
    constexpr uint32_t submitCount = std::extent<decltype(submits)>();

    const VkFence fence = VK_NULL_HANDLE;
    result              = vkQueueSubmit(mGraphicsQueue, submitCount, submits, fence);
    ASSERT(result == VK_SUCCESS);
}

void VulkanExternalHelper::readPixels(VkImage srcImage,
                                      VkImageLayout srcImageLayout,
                                      VkFormat srcImageFormat,
                                      VkOffset3D imageOffset,
                                      VkExtent3D imageExtent,
                                      void *pixels,
                                      size_t pixelsSize)
{
    ASSERT(srcImageFormat == VK_FORMAT_B8G8R8A8_UNORM ||
           srcImageFormat == VK_FORMAT_R8G8B8A8_UNORM);
    ASSERT(imageExtent.depth == 1);
    ASSERT(pixelsSize == 4 * imageExtent.width * imageExtent.height);

    VkBufferCreateInfo bufferCreateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ 0,
        /* .size = */ pixelsSize,
        /* .usage = */ VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        /* .sharingMode = */ VK_SHARING_MODE_EXCLUSIVE,
        /* .queueFamilyIndexCount = */ 0,
        /* .pQueueFamilyIndices = */ nullptr,
    };
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkResult result        = vkCreateBuffer(mDevice, &bufferCreateInfo, nullptr, &stagingBuffer);
    ASSERT(result == VK_SUCCESS);

    VkMemoryPropertyFlags requestedMemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(mDevice, stagingBuffer, &memoryRequirements);
    uint32_t memoryTypeIndex = FindMemoryType(mMemoryProperties, memoryRequirements.memoryTypeBits,
                                              requestedMemoryPropertyFlags);
    ASSERT(memoryTypeIndex != UINT32_MAX);
    VkDeviceSize deviceMemorySize = memoryRequirements.size;

    VkMemoryDedicatedAllocateInfoKHR memoryDedicatedAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
        /* .pNext = */ nullptr,
        /* .image = */ VK_NULL_HANDLE,
        /* .buffer = */ stagingBuffer,
    };
    VkMemoryAllocateInfo memoryAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        /* .pNext = */ &memoryDedicatedAllocateInfo,
        /* .allocationSize = */ deviceMemorySize,
        /* .memoryTypeIndex = */ memoryTypeIndex,
    };

    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    result = vkAllocateMemory(mDevice, &memoryAllocateInfo, nullptr, &deviceMemory);
    ASSERT(result == VK_SUCCESS);

    result = vkBindBufferMemory(mDevice, stagingBuffer, deviceMemory, 0 /* memoryOffset */);
    ASSERT(result == VK_SUCCESS);

    VkCommandBuffer commandBuffers[]                      = {VK_NULL_HANDLE};
    constexpr uint32_t commandBufferCount                 = std::extent<decltype(commandBuffers)>();
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        /* .pNext = */ nullptr,
        /* .commandPool = */ mCommandPool,
        /* .level = */ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        /* .commandBufferCount = */ commandBufferCount,
    };

    result = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, commandBuffers);
    ASSERT(result == VK_SUCCESS);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        /* .sType = */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        /* .pNext = */ nullptr,
        /* .flags = */ VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        /* .pInheritanceInfo = */ nullptr,
    };
    result = vkBeginCommandBuffer(commandBuffers[0], &commandBufferBeginInfo);
    ASSERT(result == VK_SUCCESS);

    VkBufferImageCopy bufferImageCopies[] = {
        /* [0] = */ {
            /* .bufferOffset = */ 0,
            /* .bufferRowLength = */ 0,
            /* .bufferImageHeight = */ 0,
            /* .imageSubresources = */
            {
                /* .aspectMask = */ VK_IMAGE_ASPECT_COLOR_BIT,
                /* .mipLevel = */ 0,
                /* .baseArrayLayer = */ 0,
                /* .layerCount = */ 1,
            },
            /* .imageOffset = */ imageOffset,
            /* .imageExtent = */ imageExtent,
        },
    };
    constexpr uint32_t bufferImageCopyCount = std::extent<decltype(bufferImageCopies)>();

    vkCmdCopyImageToBuffer(commandBuffers[0], srcImage, srcImageLayout, stagingBuffer,
                           bufferImageCopyCount, bufferImageCopies);

    VkMemoryBarrier memoryBarriers[] = {
        /* [0] = */ {/* .sType = */ VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                     /* .pNext = */ nullptr,
                     /* .srcAccessMask = */ VK_ACCESS_MEMORY_WRITE_BIT,
                     /* .dstAccessMask = */ VK_ACCESS_HOST_READ_BIT},
    };
    constexpr uint32_t memoryBarrierCount = std::extent<decltype(memoryBarriers)>();
    vkCmdPipelineBarrier(commandBuffers[0], VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0 /* dependencyFlags */, memoryBarrierCount,
                         memoryBarriers, 0, nullptr, 0, nullptr);

    result = vkEndCommandBuffer(commandBuffers[0]);
    ASSERT(result == VK_SUCCESS);

    const VkSubmitInfo submits[] = {
        /* [0] = */ {
            /* .sType */ VK_STRUCTURE_TYPE_SUBMIT_INFO,
            /* .pNext = */ nullptr,
            /* .waitSemaphoreCount = */ 0,
            /* .pWaitSemaphores = */ nullptr,
            /* .pWaitDstStageMask = */ nullptr,
            /* .commandBufferCount = */ commandBufferCount,
            /* .pCommandBuffers = */ commandBuffers,
            /* .signalSemaphoreCount = */ 0,
            /* .pSignalSemaphores = */ nullptr,
        },
    };
    constexpr uint32_t submitCount = std::extent<decltype(submits)>();

    const VkFence fence = VK_NULL_HANDLE;
    result              = vkQueueSubmit(mGraphicsQueue, submitCount, submits, fence);
    ASSERT(result == VK_SUCCESS);

    result = vkQueueWaitIdle(mGraphicsQueue);
    ASSERT(result == VK_SUCCESS);

    vkFreeCommandBuffers(mDevice, mCommandPool, commandBufferCount, commandBuffers);

    void *stagingMemory = nullptr;
    result = vkMapMemory(mDevice, deviceMemory, 0 /* offset */, pixelsSize, 0 /* flags */,
                         &stagingMemory);
    ASSERT(result == VK_SUCCESS);

    VkMappedMemoryRange memoryRanges[] = {
        /* [0] = */ {
            /* .sType = */ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            /* .pNext = */ nullptr,
            /* .memory = */ deviceMemory,
            /* .offset = */ 0,
            /* .size = */ pixelsSize,
        },
    };
    constexpr uint32_t memoryRangeCount = std::extent<decltype(memoryRanges)>();

    result = vkInvalidateMappedMemoryRanges(mDevice, memoryRangeCount, memoryRanges);
    ASSERT(result == VK_SUCCESS);

    memcpy(pixels, stagingMemory, pixelsSize);

    vkUnmapMemory(mDevice, deviceMemory);
    vkFreeMemory(mDevice, deviceMemory, nullptr);
    vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
}

}  // namespace angle
