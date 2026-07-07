/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OpenXRGraphicsBindingVulkan.h"

#if ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_VULKAN)

#include "OpenXRExtensions.h"
#include "OpenXRSwapchain.h"
#include <array>
#include <bit>
#include <cstring>
#include <utility>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
#include <drm_fourcc.h>
#endif
#if OS(ANDROID)
#include <android/hardware_buffer.h>
#endif

namespace WebKit {

// Vulkan non-dispatchable handles (VkImage, VkDeviceMemory, ...) are 8 bytes on every architecture (a pointer where pointers
// are 64-bit, a uint64_t otherwise). bit_cast copies the raw bytes and is correct for both representations, unlike a
// reinterpret_cast that would assume a pointer.
static_assert(sizeof(VkImage) == sizeof(uint64_t), "VkImage is expected to be a 64-bit handle");

static inline uint64_t toHandle(VkImage image)
{
    return std::bit_cast<uint64_t>(image);
}

static inline VkImage toVkImage(uint64_t handle)
{
    return std::bit_cast<VkImage>(handle);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRGraphicsBindingVulkan);

std::unique_ptr<OpenXRGraphicsBindingVulkan> OpenXRGraphicsBindingVulkan::create()
{
    return std::unique_ptr<OpenXRGraphicsBindingVulkan>(new OpenXRGraphicsBindingVulkan());
}

OpenXRGraphicsBindingVulkan::OpenXRGraphicsBindingVulkan() = default;

OpenXRGraphicsBindingVulkan::~OpenXRGraphicsBindingVulkan()
{
    ASSERT(m_vkDevice == VK_NULL_HANDLE);
}

Vector<ASCIILiteral> OpenXRGraphicsBindingVulkan::requiredInstanceExtensions() const
{
    Vector<ASCIILiteral> extensions;
    if (OpenXRExtensions::singleton().isExtensionSupported(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ""_span))
        extensions.append(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ""_s);
    return extensions;
}

bool OpenXRGraphicsBindingVulkan::initializeDisplay(bool)
{
    ASSERT(RunLoop::isMain());
    // Unlike the OpenGLES binding, Vulkan must create its instance and device through the OpenXR runtime
    // (xrCreateVulkan{Instance|Device}KHR), which requires the OpenXR instance and system to already exist. That work is
    // deferred to initializeForSession() so there is nothing to set up before instance creation here.
    return true;
}

bool OpenXRGraphicsBindingVulkan::initializeForSession(XrInstance instance, XrSystemId systemId)
{
    ASSERT(!RunLoop::isMain());

    const auto& methods = OpenXRExtensions::singleton().methods();

    auto graphicsRequirements = createOpenXRStruct<XrGraphicsRequirementsVulkanKHR, XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR>();
    CHECK_XRCMD(methods.xrGetVulkanGraphicsRequirements2KHR(instance, systemId, &graphicsRequirements));

    if (volkInitialize() != VK_SUCCESS) {
        LOG(XR, "Failed to initialize Volk for the OpenXR Vulkan binding.");
        return false;
    }

    uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion)
        vkEnumerateInstanceVersion(&loaderVersion);
    uint32_t apiMajor = VK_API_VERSION_MAJOR(loaderVersion);
    uint32_t apiMinor = VK_API_VERSION_MINOR(loaderVersion);

    // maxApiVersionSupported might return non-sensical large values standing for "no maximum". This could wrap into
    // a version below 1.1 and hide extensions whose dependencies are core from 1.1, so protect against that.
    static constexpr uint32_t plausibleMajorLimit = 8;
    auto maxMajor = static_cast<uint32_t>(XR_VERSION_MAJOR(graphicsRequirements.maxApiVersionSupported));
    auto maxMinor = static_cast<uint32_t>(XR_VERSION_MINOR(graphicsRequirements.maxApiVersionSupported));
    if (maxMajor <= plausibleMajorLimit && (maxMajor < apiMajor || (maxMajor == apiMajor && maxMinor < apiMinor))) {
        apiMajor = maxMajor;
        apiMinor = maxMinor;
    }

    auto minMajor = static_cast<uint32_t>(XR_VERSION_MAJOR(graphicsRequirements.minApiVersionSupported));
    auto minMinor = static_cast<uint32_t>(XR_VERSION_MINOR(graphicsRequirements.minApiVersionSupported));
    if (apiMajor < minMajor || (apiMajor == minMajor && apiMinor < minMinor)) {
        LOG(XR, "OpenXR Vulkan: Vulkan %u.%u is available but the runtime requires at least %u.%u", apiMajor, apiMinor, minMajor, minMinor);
        return false;
    }

    const uint32_t apiVersion = VK_MAKE_API_VERSION(0, apiMajor, apiMinor, 0);
    VkApplicationInfo applicationInfo { };
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "WebKitWebXR";
    applicationInfo.apiVersion = apiVersion;

    VkInstanceCreateInfo vkInstanceCreateInfo { };
    vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstanceCreateInfo.pApplicationInfo = &applicationInfo;

    auto xrInstanceCreateInfo = createOpenXRStruct<XrVulkanInstanceCreateInfoKHR, XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR>();
    xrInstanceCreateInfo.systemId = systemId;
    xrInstanceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrInstanceCreateInfo.vulkanCreateInfo = &vkInstanceCreateInfo;

    VkResult vkResult = VK_SUCCESS;
    CHECK_XRCMD(methods.xrCreateVulkanInstanceKHR(instance, &xrInstanceCreateInfo, &m_vkInstance, &vkResult));
    if (vkResult != VK_SUCCESS || m_vkInstance == VK_NULL_HANDLE) {
        LOG(XR, "xrCreateVulkanInstanceKHR() failed (VkResult %d).", vkResult);
        return false;
    }
    volkLoadInstanceTable(&m_instanceTable, m_vkInstance);

    auto deviceGetInfo = createOpenXRStruct<XrVulkanGraphicsDeviceGetInfoKHR, XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR>();
    deviceGetInfo.systemId = systemId;
    deviceGetInfo.vulkanInstance = m_vkInstance;
    CHECK_XRCMD(methods.xrGetVulkanGraphicsDevice2KHR(instance, &deviceGetInfo, &m_vkPhysicalDevice));
    if (m_vkPhysicalDevice == VK_NULL_HANDLE) {
        LOG(XR, "xrGetVulkanGraphicsDevice2KHR() returned a null physical device.");
        return false;
    }

    uint32_t queueFamilyCount = 0;
    m_instanceTable.vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, nullptr);
    Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    m_instanceTable.vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, queueFamilies.mutableSpan().data());

    std::optional<uint32_t> graphicsFamily;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
            break;
        }
    }
    if (!graphicsFamily) {
        LOG(XR, "No graphics-capable Vulkan queue family found.");
        return false;
    }
    m_queueFamilyIndex = *graphicsFamily;

    // Extensions to hand our own images to the web process, to import the web process' frame fence as a semaphore, and to
    // acquire/release queue-family ownership of the externally-written image (VK_QUEUE_FAMILY_FOREIGN_EXT). The external-memory
    // extension differs per platform: a dma-buf on Linux/GBM, an AHardwareBuffer on Android (its other dependencies — YCbCr
    // conversion, bind-memory2, dedicated allocation — are core from Vulkan 1.1, which we always request). The OpenXR runtime
    // appends its own required extensions on top. If the device cannot provide these, xrCreateVulkanDeviceKHR fails below and no
    // session is created, which is intended: the binding has no usable path without them.
    Vector<const char*> requiredExtensions {
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
    };
#if USE(GBM)
    requiredExtensions.append(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    requiredExtensions.append(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
#elif OS(ANDROID)
    requiredExtensions.append(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
#endif

    // Report which device the runtime selected and whether it advertises the extensions we need, so a
    // VK_ERROR_EXTENSION_NOT_PRESENT from xrCreateVulkanDeviceKHR below points at the actual cause (e.g. the runtime picked a
    // software device that lacks dma-buf export) rather than a guess.
    VkPhysicalDeviceProperties physicalDeviceProperties;
    m_instanceTable.vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &physicalDeviceProperties);
    LOG(XR, "OpenXR Vulkan: runtime selected device '%s'", physicalDeviceProperties.deviceName);
    uint32_t availableExtensionCount = 0;
    m_instanceTable.vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &availableExtensionCount, nullptr);
    Vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    m_instanceTable.vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &availableExtensionCount, availableExtensions.mutableSpan().data());
    auto isExtensionAvailable = [&](const char* name) {
        return availableExtensions.containsIf([&](const auto& extension) {
            return StringView::fromLatin1(extension.extensionName) == StringView::fromLatin1(name);
        });
    };

    Vector<const char*> enabledExtensions;
    for (auto* required : requiredExtensions) {
        if (!isExtensionAvailable(required))
            LOG(XR, "OpenXR Vulkan: required device extension %s not supported by '%s'", required, physicalDeviceProperties.deviceName);
        enabledExtensions.append(required);
    }
#if USE(GBM)
    // VK_EXT_image_drm_format_modifier is optional (exportImageAsDMABuf falls back to a linear image without it); its
    // VK_KHR_image_format_list dependency is core from Vulkan 1.2 but must be enabled explicitly on 1.1.
    if (isExtensionAvailable(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME)) {
        enabledExtensions.append(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
        if (isExtensionAvailable(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME))
            enabledExtensions.append(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
        m_supportsDRMModifiers = true;
    }
#endif // USE(GBM)

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo { };
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo vkDeviceCreateInfo { };
    vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkDeviceCreateInfo.queueCreateInfoCount = 1;
    vkDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    vkDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    vkDeviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.span().data();

    auto xrDeviceCreateInfo = createOpenXRStruct<XrVulkanDeviceCreateInfoKHR, XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR>();
    xrDeviceCreateInfo.systemId = systemId;
    xrDeviceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrDeviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
    xrDeviceCreateInfo.vulkanCreateInfo = &vkDeviceCreateInfo;

    vkResult = VK_SUCCESS;
    CHECK_XRCMD(methods.xrCreateVulkanDeviceKHR(instance, &xrDeviceCreateInfo, &m_vkDevice, &vkResult));
    if (vkResult != VK_SUCCESS || m_vkDevice == VK_NULL_HANDLE) {
        LOG(XR, "xrCreateVulkanDeviceKHR() failed (VkResult %d).", vkResult);
        return false;
    }
    volkLoadDeviceTable(&m_deviceTable, m_vkDevice);

    m_deviceTable.vkGetDeviceQueue(m_vkDevice, m_queueFamilyIndex, 0, &m_vkQueue);

    VkCommandPoolCreateInfo poolCreateInfo { };
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // The per-swapchain-image commit command buffers are recorded once and never reset or re-recorded, so no pool flags are
    // needed; they are freed in bulk when the pool is destroyed.
    poolCreateInfo.flags = 0;
    poolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    if (m_deviceTable.vkCreateCommandPool(m_vkDevice, &poolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        LOG(XR, "Failed to create the Vulkan command pool.");
        return false;
    }

    m_graphicsBinding = createOpenXRStruct<XrGraphicsBindingVulkanKHR, XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR>();
    m_graphicsBinding.instance = m_vkInstance;
    m_graphicsBinding.physicalDevice = m_vkPhysicalDevice;
    m_graphicsBinding.device = m_vkDevice;
    m_graphicsBinding.queueFamilyIndex = m_queueFamilyIndex;
    m_graphicsBinding.queueIndex = 0;

    return true;
}

const void* OpenXRGraphicsBindingVulkan::sessionGraphicsBinding() const
{
    return &m_graphicsBinding;
}

int64_t OpenXRGraphicsBindingVulkan::selectColorFormat(const Vector<int64_t>& supportedFormats, bool) const
{
    // OpenXR reports Vulkan swapchain formats as VkFormat values. Prefer 8-bit RGBA; alpha is kept
    // even when not requested, mirroring the OpenGLES binding (the channel is simply ignored).
    //
    // UNORM is preferred over SRGB deliberately. The content arrives already sRGB-encoded from the web process and is copied
    // through verbatim, so an SRGB swapchain would have the compositor decode it on sample and the values would be wrong. The
    // consequence is that the compositor blends and reprojects encoded rather than linear values, which is the same trade-off
    // other WebXR implementations make; changing it would require the web process to hand over linear content.
    static constexpr std::array<int64_t, 4> preferredFormats { {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
    } };
    for (auto format : preferredFormats) {
        if (supportedFormats.contains(format))
            return format;
    }
    return supportedFormats.first();
}

Vector<uint64_t> OpenXRGraphicsBindingVulkan::enumerateSwapchainImages(XrSwapchain swapchain) const
{
    uint32_t imageCount = 0;
    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
    if (!imageCount) {
        LOG(XR, "xrEnumerateSwapchainImages(): no images\n");
        return { };
    }

    Vector<XrSwapchainImageVulkanKHR> imageBuffers(FillWith { }, imageCount, [] {
        return createOpenXRStruct<XrSwapchainImageVulkanKHR, XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR>();
    }());

    Vector<XrSwapchainImageBaseHeader*> imageHeaders = imageBuffers.map([](auto& image) {
        return (XrSwapchainImageBaseHeader*) &image;
    });

    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, imageHeaders[0]));

    return imageBuffers.map([](auto& image) -> uint64_t {
        return toHandle(image.image);
    });
}

void OpenXRGraphicsBindingVulkan::releaseSessionGraphics()
{
    if (m_vkDevice == VK_NULL_HANDLE)
        return;

    m_deviceTable.vkDeviceWaitIdle(m_vkDevice);

    for (auto& exportedImage : m_exportedImages.values())
        destroyExportedImage(exportedImage);
    m_exportedImages.clear();

    m_pendingFenceFD = { };

    if (m_commandPool != VK_NULL_HANDLE) {
        m_deviceTable.vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    m_deviceTable.vkDestroyDevice(m_vkDevice, nullptr);
    m_vkDevice = VK_NULL_HANDLE;
    m_vkQueue = VK_NULL_HANDLE;
    m_vkPhysicalDevice = VK_NULL_HANDLE;

    if (m_vkInstance != VK_NULL_HANDLE) {
        m_instanceTable.vkDestroyInstance(m_vkInstance, nullptr);
        m_vkInstance = VK_NULL_HANDLE;
        m_instanceTable = { };
    }

    m_graphicsBinding = { };
}

void OpenXRGraphicsBindingVulkan::destroyExportedImage(ExportedImage& exportedImage)
{
    ASSERT(m_vkDevice != VK_NULL_HANDLE);
    if (exportedImage.acquireSemaphore != VK_NULL_HANDLE)
        m_deviceTable.vkDestroySemaphore(m_vkDevice, exportedImage.acquireSemaphore, nullptr);
    if (exportedImage.inFlightFence != VK_NULL_HANDLE)
        m_deviceTable.vkDestroyFence(m_vkDevice, exportedImage.inFlightFence, nullptr);
    if (exportedImage.image != VK_NULL_HANDLE)
        m_deviceTable.vkDestroyImage(m_vkDevice, exportedImage.image, nullptr);
    if (exportedImage.memory != VK_NULL_HANDLE)
        m_deviceTable.vkFreeMemory(m_vkDevice, exportedImage.memory, nullptr);
    // The command buffer is owned by m_commandPool and freed when the pool is destroyed.
    exportedImage = { };
}

#if USE(GBM) || OS(ANDROID)
static std::optional<uint32_t> findMemoryType(const VolkInstanceTable& instanceTable, VkPhysicalDevice physicalDevice, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    instanceTable.vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    auto memoryTypes = unsafeMakeSpan(memoryProperties.memoryTypes, memoryProperties.memoryTypeCount);
    uint32_t index = 0;
    for (const auto& memoryType : memoryTypes) {
        if ((memoryTypeBits & (1u << index)) && (memoryType.propertyFlags & properties) == properties)
            return index;
        ++index;
    }
    return std::nullopt;
}
#endif // USE(GBM) || OS(ANDROID)

#if USE(GBM)
static uint32_t drmFourCCForVkFormat(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        return DRM_FORMAT_ABGR8888;
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        return DRM_FORMAT_ARGB8888;
    default:
        return DRM_FORMAT_INVALID;
    }
}
#endif // USE(GBM)

bool OpenXRGraphicsBindingVulkan::createExportedImageSyncObjects(ExportedImage& exportedImage)
{
    // The fence starts signaled so the first commitFrame() that reuses this image waits on it without a special case.
    VkFenceCreateInfo fenceCreateInfo { };
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (m_deviceTable.vkCreateFence(m_vkDevice, &fenceCreateInfo, nullptr, &exportedImage.inFlightFence) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkCreateFence failed for exported texture");
        return false;
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo { };
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (m_deviceTable.vkCreateSemaphore(m_vkDevice, &semaphoreCreateInfo, nullptr, &exportedImage.acquireSemaphore) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkCreateSemaphore failed for exported texture");
        return false;
    }
    return true;
}

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingVulkan::exportTexture(uint64_t image, const OpenXRSwapchain& swapchain, TextureType type, uint32_t width, uint32_t height)
{
    ASSERT(m_vkDevice != VK_NULL_HANDLE);

    if (type != TextureType::Texture2D) {
        // FIXME: cube layers are not implemented yet for the Vulkan binding.
        return std::nullopt;
    }

#if USE(GBM)
    return exportImageAsDMABuf(image, swapchain, width, height);
#elif OS(ANDROID)
    // Android shares the buffer as an AHardwareBuffer instead of a DMABuf.
    return exportImageAsAHardwareBuffer(image, swapchain, width, height);
#else
    // No external-texture path without GBM (DMABuf) or Android (AHardwareBuffer), so nothing is exported.
    UNUSED_PARAM(image);
    UNUSED_PARAM(swapchain);
    UNUSED_PARAM(type);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    return std::nullopt;
#endif
}

#if USE(GBM)
Vector<VkDrmFormatModifierPropertiesEXT> OpenXRGraphicsBindingVulkan::supportedExportDRMModifiers(VkFormat format, VkImageUsageFlags usage) const
{
    VkDrmFormatModifierPropertiesListEXT modifierList { };
    modifierList.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
    VkFormatProperties2 formatProperties { };
    formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    formatProperties.pNext = &modifierList;
    m_instanceTable.vkGetPhysicalDeviceFormatProperties2(m_vkPhysicalDevice, format, &formatProperties);
    Vector<VkDrmFormatModifierPropertiesEXT> allModifiers(modifierList.drmFormatModifierCount);
    modifierList.pDrmFormatModifierProperties = allModifiers.mutableSpan().data();
    m_instanceTable.vkGetPhysicalDeviceFormatProperties2(m_vkPhysicalDevice, format, &formatProperties);

    VkFormatFeatureFlags requiredFeatures = 0;
    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        requiredFeatures |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        requiredFeatures |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        requiredFeatures |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
        requiredFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    Vector<VkDrmFormatModifierPropertiesEXT> result;
    for (const auto& properties : allModifiers) {
        if ((properties.drmFormatModifierTilingFeatures & requiredFeatures) != requiredFeatures)
            continue;

        VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo { };
        modifierInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
        modifierInfo.drmFormatModifier = properties.drmFormatModifier;
        modifierInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkPhysicalDeviceExternalImageFormatInfo externalInfo { };
        externalInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
        externalInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        externalInfo.pNext = &modifierInfo;

        VkPhysicalDeviceImageFormatInfo2 imageFormatInfo { };
        imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
        imageFormatInfo.pNext = &externalInfo;
        imageFormatInfo.format = format;
        imageFormatInfo.type = VK_IMAGE_TYPE_2D;
        imageFormatInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        imageFormatInfo.usage = usage;

        VkExternalImageFormatProperties externalFormatProperties { };
        externalFormatProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
        VkImageFormatProperties2 imageFormatProperties { };
        imageFormatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
        imageFormatProperties.pNext = &externalFormatProperties;
        if (m_instanceTable.vkGetPhysicalDeviceImageFormatProperties2(m_vkPhysicalDevice, &imageFormatInfo, &imageFormatProperties) != VK_SUCCESS)
            continue;
        if (!(externalFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT))
            continue;
        result.append(properties);
    }
    return result;
}

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingVulkan::exportImageAsDMABuf(uint64_t swapchainImage, const OpenXRSwapchain& swapchain, uint32_t width, uint32_t height)
{
    auto format = static_cast<VkFormat>(swapchain.format());
    auto fourcc = drmFourCCForVkFormat(format);
    if (fourcc == DRM_FORMAT_INVALID) {
        RELEASE_LOG(XR, "OpenXR Vulkan: unsupported swapchain format %d for dma-buf export", format);
        return std::nullopt;
    }

    // Only TRANSFER_SRC, not COLOR_ATTACHMENT: Vulkan merely transfer-reads this image for the commit blit, while the web
    // process renders into the exported DMABuf via GL/EGL, and linear-tiled images generally do not support COLOR_ATTACHMENT.
    // The tiled DRM-modifier path is the validation-clean way to export a DMABuf; the linear fallback is tolerated but not
    // advertised where the modifier extension is missing, so it renders but emits VUID-VkImageCreateInfo-pNext-00990.
    // FIXME: the chosen modifier is one our device supports; it is not yet intersected with the modifiers the web process' EGL
    // import can consume (that needs the consumer's list plumbed across IPC). On a single GPU the sets overlap in practice.
    static constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    auto modifierProperties = m_supportsDRMModifiers ? supportedExportDRMModifiers(format, usage) : Vector<VkDrmFormatModifierPropertiesEXT> { };
    Vector<uint64_t> modifiers = modifierProperties.map([](const auto& properties) {
        return properties.drmFormatModifier;
    });
    bool useModifiers = !modifiers.isEmpty();

    VkExternalMemoryImageCreateInfo externalMemoryImageInfo { };
    externalMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageDrmFormatModifierListCreateInfoEXT modifierListInfo { };
    modifierListInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
    modifierListInfo.pNext = &externalMemoryImageInfo;
    modifierListInfo.drmFormatModifierCount = static_cast<uint32_t>(modifiers.size());
    modifierListInfo.pDrmFormatModifiers = modifiers.span().data();

    VkImageCreateInfo imageCreateInfo { };
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = useModifiers ? static_cast<const void*>(&modifierListInfo) : static_cast<const void*>(&externalMemoryImageInfo);
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = useModifiers ? VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT : VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    ExportedImage exportedImage;
    exportedImage.width = width;
    exportedImage.height = height;
    exportedImage.format = format;

    if (m_deviceTable.vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &exportedImage.image) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkCreateImage failed for exported texture (useModifiers=%d)", useModifiers);
        return std::nullopt;
    }

    auto cleanupOnError = makeScopeExit([&] {
        destroyExportedImage(exportedImage);
    });

    VkMemoryRequirements memoryRequirements;
    m_deviceTable.vkGetImageMemoryRequirements(m_vkDevice, exportedImage.image, &memoryRequirements);

    auto memoryTypeIndex = findMemoryType(m_instanceTable, m_vkPhysicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryTypeIndex) {
        RELEASE_LOG(XR, "OpenXR Vulkan: no suitable memory type for exported texture");
        return std::nullopt;
    }

    VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo { };
    dedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocateInfo.image = exportedImage.image;

    VkExportMemoryAllocateInfo exportAllocateInfo { };
    exportAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    exportAllocateInfo.pNext = &dedicatedAllocateInfo;

    VkMemoryAllocateInfo memoryAllocateInfo { };
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = &exportAllocateInfo;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = *memoryTypeIndex;

    if (m_deviceTable.vkAllocateMemory(m_vkDevice, &memoryAllocateInfo, nullptr, &exportedImage.memory) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkAllocateMemory failed for exported texture");
        return std::nullopt;
    }
    if (m_deviceTable.vkBindImageMemory(m_vkDevice, exportedImage.image, exportedImage.memory, 0) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkBindImageMemory failed for exported texture");
        return std::nullopt;
    }

    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    uint32_t planeCount = 1;
    if (useModifiers) {
        VkImageDrmFormatModifierPropertiesEXT chosenModifier { };
        chosenModifier.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;
        if (m_deviceTable.vkGetImageDrmFormatModifierPropertiesEXT(m_vkDevice, exportedImage.image, &chosenModifier) != VK_SUCCESS) {
            RELEASE_LOG(XR, "OpenXR Vulkan: vkGetImageDrmFormatModifierPropertiesEXT failed for exported texture");
            return std::nullopt;
        }
        modifier = chosenModifier.drmFormatModifier;
        for (const auto& properties : modifierProperties) {
            if (properties.drmFormatModifier == modifier) {
                planeCount = properties.drmFormatModifierPlaneCount;
                break;
            }
        }
        if (!planeCount || planeCount > 4) {
            RELEASE_LOG(XR, "OpenXR Vulkan: unexpected plane count %u for exported texture", planeCount);
            return std::nullopt;
        }
    }

    // All planes share the single dedicated allocation, so one exported fd is duplicated per plane. The planes differ only by offset and stride.
    VkMemoryGetFdInfoKHR getFdInfo { };
    getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    getFdInfo.memory = exportedImage.memory;
    getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    int dmaBufFd = -1;
    if (m_deviceTable.vkGetMemoryFdKHR(m_vkDevice, &getFdInfo, &dmaBufFd) != VK_SUCCESS || dmaBufFd < 0) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkGetMemoryFdKHR failed for exported texture");
        return std::nullopt;
    }
    WTF::UnixFileDescriptor primaryDescriptor { dmaBufFd, WTF::UnixFileDescriptor::Adopt };

    Vector<WTF::UnixFileDescriptor> fds;
    Vector<uint32_t> strides;
    Vector<uint32_t> offsets;
    for (uint32_t plane = 0; plane < planeCount; ++plane) {
        // A modifier image exposes each plane via a MEMORY_PLANE aspect (bits run consecutively from MEMORY_PLANE_0); a linear image has a single colour plane.
        VkImageSubresource subresource { };
        subresource.aspectMask = useModifiers ? static_cast<VkImageAspectFlagBits>(VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << plane) : VK_IMAGE_ASPECT_COLOR_BIT;
        VkSubresourceLayout layout { };
        m_deviceTable.vkGetImageSubresourceLayout(m_vkDevice, exportedImage.image, &subresource, &layout);
        auto duplicated = primaryDescriptor.duplicate();
        if (!duplicated) {
            RELEASE_LOG(XR, "OpenXR Vulkan: failed to duplicate DMABuf fd for plane %u", plane);
            return std::nullopt;
        }
        fds.append(WTF::move(duplicated));
        strides.append(static_cast<uint32_t>(layout.rowPitch));
        offsets.append(static_cast<uint32_t>(layout.offset));
    }

    if (!createExportedImageSyncObjects(exportedImage))
        return std::nullopt;

    cleanupOnError.release();
    m_exportedImages.set(swapchainImage, WTF::move(exportedImage));

    return PlatformXR::FrameData::ExternalTexture {
        .fds = WTF::move(fds),
        .strides = WTF::move(strides),
        .offsets = WTF::move(offsets),
        .fourcc = fourcc,
        .modifier = modifier,
    };
}
#endif // USE(GBM)

#if OS(ANDROID)
std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingVulkan::exportImageAsAHardwareBuffer(uint64_t swapchainImage, const OpenXRSwapchain& swapchain, uint32_t width, uint32_t height)
{
    auto format = static_cast<VkFormat>(swapchain.format());

    // The WebProcess renders into the AHardwareBuffer as a colour target and we transfer read it at commit, so it needs
    // COLOR_ATTACHMENT | TRANSFER_SRC. COLOR_ATTACHMENT is what makes the exported buffer carry the GPU_FRAMEBUFFER usage
    // that the GL consumer needs.
    static constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkExternalMemoryImageCreateInfo externalMemoryImageInfo { };
    externalMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkImageCreateInfo imageCreateInfo { };
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = &externalMemoryImageInfo;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    ExportedImage exportedImage;
    exportedImage.width = width;
    exportedImage.height = height;
    exportedImage.format = format;
    if (m_deviceTable.vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &exportedImage.image) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkCreateImage failed for exported AHardwareBuffer texture");
        return std::nullopt;
    }

    auto cleanupOnError = makeScopeExit([&] {
        destroyExportedImage(exportedImage);
    });

    VkMemoryRequirements memoryRequirements;
    m_deviceTable.vkGetImageMemoryRequirements(m_vkDevice, exportedImage.image, &memoryRequirements);
    auto memoryTypeIndex = findMemoryType(m_instanceTable, m_vkPhysicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryTypeIndex) {
        RELEASE_LOG(XR, "OpenXR Vulkan: no suitable memory type for exported AHardwareBuffer texture");
        return std::nullopt;
    }

    // AHardwareBuffer export requires a dedicated allocation.
    VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo { };
    dedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocateInfo.image = exportedImage.image;

    VkExportMemoryAllocateInfo exportAllocateInfo { };
    exportAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportAllocateInfo.pNext = &dedicatedAllocateInfo;
    exportAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkMemoryAllocateInfo memoryAllocateInfo { };
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = &exportAllocateInfo;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = *memoryTypeIndex;
    if (m_deviceTable.vkAllocateMemory(m_vkDevice, &memoryAllocateInfo, nullptr, &exportedImage.memory) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkAllocateMemory failed for exported AHardwareBuffer texture");
        return std::nullopt;
    }
    if (m_deviceTable.vkBindImageMemory(m_vkDevice, exportedImage.image, exportedImage.memory, 0) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkBindImageMemory failed for exported AHardwareBuffer texture");
        return std::nullopt;
    }

    // Pull the AHardwareBuffer out of the memory; it comes with one acquired reference that adoptRef transfers to the caller.
    // Resolve the function via vkGetDeviceProcAddr rather than the device table so this does not depend on volk having been built
    // with VK_USE_PLATFORM_ANDROID_KHR.
    auto vkGetMemoryAndroidHardwareBuffer = reinterpret_cast<PFN_vkGetMemoryAndroidHardwareBufferANDROID>(m_instanceTable.vkGetDeviceProcAddr(m_vkDevice, "vkGetMemoryAndroidHardwareBufferANDROID"));
    if (!vkGetMemoryAndroidHardwareBuffer) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkGetMemoryAndroidHardwareBufferANDROID is not available");
        return std::nullopt;
    }
    VkMemoryGetAndroidHardwareBufferInfoANDROID getInfo { };
    getInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    getInfo.memory = exportedImage.memory;
    AHardwareBuffer* buffer = nullptr;
    if (vkGetMemoryAndroidHardwareBuffer(m_vkDevice, &getInfo, &buffer) != VK_SUCCESS || !buffer) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkGetMemoryAndroidHardwareBufferANDROID failed for exported texture");
        return std::nullopt;
    }
    // Adopt the acquired reference now so it is released automatically on any early return below.
    RefPtr<AHardwareBuffer> hardwareBuffer = adoptRef(buffer);

    if (!createExportedImageSyncObjects(exportedImage))
        return std::nullopt;

    cleanupOnError.release();
    m_exportedImages.set(swapchainImage, WTF::move(exportedImage));
    return hardwareBuffer;
}
#endif // OS(ANDROID)

static VkImageMemoryBarrier makeImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t layerCount = 1)
{
    VkImageMemoryBarrier barrier { };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = srcQueueFamily;
    barrier.dstQueueFamilyIndex = dstQueueFamily;
    barrier.image = image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layerCount };
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    return barrier;
}

bool OpenXRGraphicsBindingVulkan::recordBlitCommandBuffer(ExportedImage& exportedImage, VkImage swapchainImage)
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo { };
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (m_deviceTable.vkAllocateCommandBuffers(m_vkDevice, &commandBufferAllocateInfo, &commandBuffer) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: failed to allocate commit command buffer");
        return false;
    }

    VkCommandBufferBeginInfo beginInfo { };
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // No VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: this buffer is recorded once and resubmitted on every commit.
    m_deviceTable.vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // The exported image was written by the WebProcess through an external (OpenGL) DMABuf import, so acquire queue family
    // ownership of it from VK_QUEUE_FAMILY_FOREIGN_EXT before reading. We keep it in VK_IMAGE_LAYOUT_GENERAL throughout: GENERAL
    // preserves the foreign contents (UNDEFINED would let the driver discard them) and imposes no Vulkan-specific tiling, which
    // matches the externally-written linear DMABuf. The swapchain image is owned by our own device and fully overwritten by the
    // blit, so it can start from UNDEFINED with no ownership transfer.
    std::array<VkImageMemoryBarrier, 2> preBlitBarriers { {
        makeImageBarrier(exportedImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_FOREIGN_EXT, m_queueFamilyIndex),
        makeImageBarrier(swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT),
    } };
    m_deviceTable.vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, preBlitBarriers.size(), preBlitBarriers.data());

    // The WebProcess renders into the exported image with OpenGL, whose framebuffer origin is the bottom left, whereas
    // Vulkan (and therefore the runtime's swapchain image) uses the top left. We blit rather than copy so the destination's
    // Y offsets can be inverted, correcting the flip in the same pass (vkCmdCopyImage cannot flip). The transfer is 1:1 in
    // size and format so NEAREST adds no filtering, and the Y inversion itself costs nothing beyond the blit.
    VkImageBlit blitRegion {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(exportedImage.width), static_cast<int32_t>(exportedImage.height), 1 } },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { { 0, static_cast<int32_t>(exportedImage.height), 0 }, { static_cast<int32_t>(exportedImage.width), 0, 1 } },
    };
    m_deviceTable.vkCmdBlitImage(commandBuffer, exportedImage.image, VK_IMAGE_LAYOUT_GENERAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

    // Transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL, which the runtime expects when it is released, and release
    // ownership of the exported image back to the foreign (OpenGL) producer so it can render into it again next frame.
    std::array<VkImageMemoryBarrier, 2> postBlitBarriers { {
        makeImageBarrier(swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
        makeImageBarrier(exportedImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_READ_BIT, 0, m_queueFamilyIndex, VK_QUEUE_FAMILY_FOREIGN_EXT),
    } };
    m_deviceTable.vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, postBlitBarriers.size(), postBlitBarriers.data());

    m_deviceTable.vkEndCommandBuffer(commandBuffer);

    exportedImage.commandBuffer = commandBuffer;
    return true;
}

void OpenXRGraphicsBindingVulkan::commitFrame(uint64_t keyImage, const OpenXRSwapchain& swapchain, TextureType type, const Vector<uint64_t>&)
{
    // Take the fence stashed by waitFrameFence() for this commit. Snapshot it up front so the early returns below don't leave a
    // stale fd waiting on the next layer's commit (the UnixFileDescriptor closes it if we bail out).
    auto pendingFenceFD = std::exchange(m_pendingFenceFD, WTF::UnixFileDescriptor { });

    if (type != TextureType::Texture2D)
        return;

    ASSERT(m_vkDevice != VK_NULL_HANDLE);

    auto exportedImageIterator = m_exportedImages.find(keyImage);
    if (exportedImageIterator == m_exportedImages.end())
        return;
    auto& exportedImage = exportedImageIterator->value;

    VkImage swapchainImage = toVkImage(swapchain.acquiredTexture());
    ASSERT(swapchainImage != VK_NULL_HANDLE);

    // The blit is identical every time this swapchain image is acquired so its command buffer is recorded once and resubmitted on every commit.
    if (exportedImage.commandBuffer == VK_NULL_HANDLE && !recordBlitCommandBuffer(exportedImage, swapchainImage))
        return;

    // Wait for this image's previous submission to retire before reusing its command buffer and acquire semaphore, then reset
    // the fence for this frame's submission. The fence was created signaled and the image was last used a full swapchain cycle
    // ago, so this almost always returns immediately; it only blocks if the GPU has fallen behind by the swapchain depth. This
    // is what lets us drop the per-frame vkQueueWaitIdle and overlap CPU and GPU work.
    m_deviceTable.vkWaitForFences(m_vkDevice, 1, &exportedImage.inFlightFence, VK_TRUE, UINT64_MAX);
    m_deviceTable.vkResetFences(m_vkDevice, 1, &exportedImage.inFlightFence);

    // Import the WebProcess render-completion fence (stashed by waitFrameFence()) into this image's semaphore so the copy can
    // wait on it on the GPU. SYNC_FD payloads are temporary and consumed by the wait, and the fence wait above guarantees the
    // previous wait on this semaphore has already executed, so re-importing here is safe.
    bool waitOnAcquireFence = false;
    if (pendingFenceFD) {
        VkImportSemaphoreFdInfoKHR importInfo { };
        importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
        importInfo.semaphore = exportedImage.acquireSemaphore;
        importInfo.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
        importInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
        importInfo.fd = pendingFenceFD.value();
        if (m_deviceTable.vkImportSemaphoreFdKHR(m_vkDevice, &importInfo) == VK_SUCCESS) {
            // Vulkan owns the fd on a successful SYNC_FD import, disown it (discarding the result) to avoid a double close.
            (void)pendingFenceFD.release();
            waitOnAcquireFence = true;
        } else
            RELEASE_LOG(XR, "OpenXR Vulkan: vkImportSemaphoreFdKHR failed; frame fence ignored");
    }

    // The blit waits on the GPU for the WebProcess to finish rendering into the exported image (TRANSFER stage, where the
    // blit reads), and signals inFlightFence on completion so the next reuse of this image can wait on it.
    VkPipelineStageFlags acquireWaitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo { };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    if (waitOnAcquireFence) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &exportedImage.acquireSemaphore;
        submitInfo.pWaitDstStageMask = &acquireWaitStage;
    }
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &exportedImage.commandBuffer;
    if (auto submitResult = m_deviceTable.vkQueueSubmit(m_vkQueue, 1, &submitInfo, exportedImage.inFlightFence); submitResult != VK_SUCCESS)
        RELEASE_LOG(XR, "OpenXR Vulkan: vkQueueSubmit failed (%d)", submitResult);
}

void OpenXRGraphicsBindingVulkan::waitFrameFence(WTF::UnixFileDescriptor&& fenceFD)
{
    // The fence is the WebProcess render completion sync_file. waitFrameFence() does not know which swapchain image the
    // following endFrame()/commitFrame() will target, so just stash the fd here. Then commitFrame() imports it into that image's
    // acquireSemaphore and the blit submission waits on it, mirroring the OpenGLES GLFence::importFD()/serverWait() path.
    // The caller (OpenXRCoordinator::endFrame) always pairs a waitFrameFence() with the following layer's commitFrame(), which
    // consumes and clears m_pendingFenceFD up front, so there must be no unconsumed fd here. (The move assignment would still close
    // a previous fd rather than leak it, but a stash without an intervening consume means the pairing contract was broken.)
    ASSERT(!m_pendingFenceFD);
    m_pendingFenceFD = WTF::move(fenceFD);
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_VULKAN)
