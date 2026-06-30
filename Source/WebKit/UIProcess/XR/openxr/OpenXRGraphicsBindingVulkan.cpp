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
#include <drm_fourcc.h>
#include <utility>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unix/UnixFileDescriptor.h>

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

    // Extensions required to export our own images as dma-bufs to the web process, to import the web process' frame fence as a
    // semaphore, and to acquire/release queue-family ownership of the externally-written image (VK_QUEUE_FAMILY_FOREIGN_EXT).
    // The OpenXR runtime appends its own required extensions on top of these. If the device cannot provide them,
    // xrCreateVulkanDeviceKHR fails below and no session is created, which is intended: the binding has no usable path without
    // them.
    static constexpr std::array<const char*, 4> deviceExtensions { {
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
    } };

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
    for (auto* required : deviceExtensions) {
        bool available = availableExtensions.containsIf([&](const auto& extension) {
            return StringView::fromLatin1(extension.extensionName) == StringView::fromLatin1(required);
        });
        if (!available)
            LOG(XR, "OpenXR Vulkan: required device extension %s not supported by '%s'", required, physicalDeviceProperties.deviceName);
    }

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
    vkDeviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    vkDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

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

    VkSemaphoreCreateInfo semaphoreCreateInfo { };
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (m_deviceTable.vkCreateSemaphore(m_vkDevice, &semaphoreCreateInfo, nullptr, &m_acquireSemaphore) != VK_SUCCESS) {
        LOG(XR, "Failed to create the Vulkan frame-fence semaphore.");
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

    if (m_acquireSemaphore != VK_NULL_HANDLE) {
        m_deviceTable.vkDestroySemaphore(m_vkDevice, m_acquireSemaphore, nullptr);
        m_acquireSemaphore = VK_NULL_HANDLE;
    }
    m_acquireSemaphorePending = false;

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
    if (exportedImage.image != VK_NULL_HANDLE)
        m_deviceTable.vkDestroyImage(m_vkDevice, exportedImage.image, nullptr);
    if (exportedImage.memory != VK_NULL_HANDLE)
        m_deviceTable.vkFreeMemory(m_vkDevice, exportedImage.memory, nullptr);
    exportedImage = { };
}

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

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingVulkan::exportTexture(uint64_t image, const OpenXRSwapchain& swapchain, TextureType type, uint32_t width, uint32_t height)
{
    ASSERT(m_vkDevice != VK_NULL_HANDLE);

    switch (type) {
    case TextureType::Texture2D:
        return exportTexture2D(image, swapchain, width, height);
    case TextureType::Cubemap:
        // FIXME: cube layers are not implemented yet for the Vulkan binding.
        break;
    }
    return std::nullopt;
}

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingVulkan::exportTexture2D(uint64_t swapchainImage, const OpenXRSwapchain& swapchain, uint32_t width, uint32_t height)
{
    auto format = static_cast<VkFormat>(swapchain.format());
    auto fourcc = drmFourCCForVkFormat(format);
    if (fourcc == DRM_FORMAT_INVALID) {
        RELEASE_LOG(XR, "OpenXR Vulkan: unsupported swapchain format %d for dma-buf export", format);
        return std::nullopt;
    }

    VkExternalMemoryImageCreateInfo externalMemoryImageInfo { };
    externalMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    // Linear tiling, exported as a DRM_FORMAT_MOD_LINEAR dma-buf. This avoids depending on VK_EXT_image_drm_format_modifier,
    // which not every driver advertises, and is the most widely importable layout on the web process side. We only ever read
    // this image as a transfer source (the web process renders into it via the dma-buf), so TRANSFER_SRC is enough.
    //
    // FIXME: prefer VK_EXT_image_drm_format_modifier when it is available, falling back to linear otherwise. This image is a
    // render target the web process draws into every frame (per swapchain image, at the headset's refresh rate), so a tiled
    // or otherwise optimal layout is worth the added complexity:
    //   a) Performance: linear is the slowest layout for the GPU to render into and sample/copy from; a vendor-tiled modifier
    //      has far better cache locality for those per-frame operations.
    //   b) Bandwidth: some modifiers enable framebuffer compression (e.g. AMD DCC), cutting the memory traffic of both the
    //      web process' rendering and our commitFrame copy.
    //   c) Explicit layout negotiation: producer and consumer agree on the exact memory layout instead of relying on linear
    //      being universally importable.
    // Doing this correctly means creating the image with the intersection of the modifiers our device supports and the ones
    // the consumer (the web process' dma-buf import) can handle, which requires plumbing the consumer's supported-modifier
    // list here; linear stays as the fallback.
    VkImageCreateInfo imageCreateInfo { };
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = &externalMemoryImageInfo;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    ExportedImage exportedImage;
    exportedImage.width = width;
    exportedImage.height = height;
    exportedImage.format = format;

    if (m_deviceTable.vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &exportedImage.image) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkCreateImage failed for exported texture");
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

    // Single-plane linear layout: the row pitch and offset of the colour plane fully describe it.
    VkImageSubresource subresource { };
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSubresourceLayout layout { };
    m_deviceTable.vkGetImageSubresourceLayout(m_vkDevice, exportedImage.image, &subresource, &layout);

    VkMemoryGetFdInfoKHR getFdInfo { };
    getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    getFdInfo.memory = exportedImage.memory;
    getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    int dmaBufFd = -1;
    if (m_deviceTable.vkGetMemoryFdKHR(m_vkDevice, &getFdInfo, &dmaBufFd) != VK_SUCCESS || dmaBufFd < 0) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkGetMemoryFdKHR failed for exported texture");
        return std::nullopt;
    }

    cleanupOnError.release();
    m_exportedImages.set(swapchainImage, WTF::move(exportedImage));

    Vector<WTF::UnixFileDescriptor> fds;
    fds.append(WTF::UnixFileDescriptor { dmaBufFd, WTF::UnixFileDescriptor::Adopt });
    return PlatformXR::FrameData::ExternalTexture {
        .fds = WTF::move(fds),
        .strides = { static_cast<uint32_t>(layout.rowPitch) },
        .offsets = { static_cast<uint32_t>(layout.offset) },
        .fourcc = fourcc,
        .modifier = DRM_FORMAT_MOD_LINEAR,
    };
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

    auto makeBarrier = [](VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED) {
        VkImageMemoryBarrier barrier { };
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = srcQueueFamily;
        barrier.dstQueueFamilyIndex = dstQueueFamily;
        barrier.image = image;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        return barrier;
    };

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
        makeBarrier(exportedImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_FOREIGN_EXT, m_queueFamilyIndex),
        makeBarrier(swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT),
    } };
    m_deviceTable.vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, preBlitBarriers.size(), preBlitBarriers.data());

    // The WebProcess renders into the exported image with OpenGL, whose framebuffer origin is the bottom left, whereas
    // Vulkan (and therefore the runtime's swapchain image) uses the top left. We blit rather than copy so the destination's
    // Y offsets can be inverted, correcting the flip in the same pass (vkCmdCopyImage cannot flip). The transfer is 1:1 in
    // size and format so NEAREST adds no filtering, and the Y inversion itself costs nothing beyond the blit.
    VkImageBlit blitRegion { };
    blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.srcOffsets[0] = { 0, 0, 0 };
    blitRegion.srcOffsets[1] = { static_cast<int32_t>(exportedImage.width), static_cast<int32_t>(exportedImage.height), 1 };
    blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.dstOffsets[0] = { 0, static_cast<int32_t>(exportedImage.height), 0 };
    blitRegion.dstOffsets[1] = { static_cast<int32_t>(exportedImage.width), 0, 1 };
    m_deviceTable.vkCmdBlitImage(commandBuffer, exportedImage.image, VK_IMAGE_LAYOUT_GENERAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

    // Transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL, which the runtime expects when it is released, and release
    // ownership of the exported image back to the foreign (OpenGL) producer so it can render into it again next frame.
    std::array<VkImageMemoryBarrier, 2> postBlitBarriers { {
        makeBarrier(swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
        makeBarrier(exportedImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_READ_BIT, 0, m_queueFamilyIndex, VK_QUEUE_FAMILY_FOREIGN_EXT),
    } };
    m_deviceTable.vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, postBlitBarriers.size(), postBlitBarriers.data());

    m_deviceTable.vkEndCommandBuffer(commandBuffer);

    exportedImage.commandBuffer = commandBuffer;
    return true;
}

void OpenXRGraphicsBindingVulkan::commitFrame(uint64_t keyImage, const OpenXRSwapchain& swapchain, TextureType type, const Vector<uint64_t>&)
{
    // Consume any fence imported by waitFrameFence() for this commit. Snapshot it up front so the
    // early returns below don't leave a stale payload waiting on the next layer's commit.
    bool waitOnAcquireFence = std::exchange(m_acquireSemaphorePending, false);

    if (type != TextureType::Texture2D)
        return;

    ASSERT(m_vkDevice != VK_NULL_HANDLE);

    auto exportedImageIterator = m_exportedImages.find(keyImage);
    if (exportedImageIterator == m_exportedImages.end())
        return;
    auto& exportedImage = exportedImageIterator->value;

    VkImage swapchainImage = toVkImage(swapchain.acquiredTexture());
    ASSERT(swapchainImage != VK_NULL_HANDLE);

    // The blit is identical every time this swapchain image is acquired (fixed source/destination images and region), so its
    // command buffer is recorded once and resubmitted on every commit. Reuse is safe because the per-frame vkQueueWaitIdle below
    // guarantees the previous submission completed before this one is resubmitted.
    if (exportedImage.commandBuffer == VK_NULL_HANDLE && !recordBlitCommandBuffer(exportedImage, swapchainImage))
        return;

    // Make the blit wait, on the GPU, for the web process to finish rendering into the exported image
    // (the fence imported by waitFrameFence()). TRANSFER stage because the blit reads at transfer time.
    VkPipelineStageFlags acquireWaitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo { };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    if (waitOnAcquireFence) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_acquireSemaphore;
        submitInfo.pWaitDstStageMask = &acquireWaitStage;
    }
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &exportedImage.commandBuffer;
    m_deviceTable.vkQueueSubmit(m_vkQueue, 1, &submitInfo, VK_NULL_HANDLE);

    // FIXME: perf — a per-frame queue wait serializes CPU and GPU. The eventual path should let the runtime synchronize on
    // the queue instead of blocking here. Note this idle also currently guarantees m_acquireSemaphore's temporary payload is
    // consumed before the next frame re-imports into it, so removing it needs per-frame semaphores or a tracking fence.
    m_deviceTable.vkQueueWaitIdle(m_vkQueue);
}

void OpenXRGraphicsBindingVulkan::waitFrameFence(WTF::UnixFileDescriptor&& fenceFD)
{
    // The fence is the web process' render-completion sync_file. Import it into m_acquireSemaphore as a temporary SYNC_FD
    // payload; commitFrame() then makes its blit submission wait on that semaphore so the copy observes the finished writes
    // on the GPU without stalling the CPU. This mirrors the OpenGLES GLFence::importFD()/serverWait() path. SYNC_FD payloads
    // are always temporary and are consumed by the wait, so the semaphore is reusable across frames.
    if (!fenceFD)
        return;

    ASSERT(m_acquireSemaphore != VK_NULL_HANDLE);

    VkImportSemaphoreFdInfoKHR importInfo { };
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    importInfo.semaphore = m_acquireSemaphore;
    importInfo.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
    importInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
    importInfo.fd = fenceFD.value();

    if (m_deviceTable.vkImportSemaphoreFdKHR(m_vkDevice, &importInfo) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkImportSemaphoreFdKHR failed; frame fence ignored");
        return;
    }

    // On a successful SYNC_FD import Vulkan takes ownership of the fd, so disown it from the descriptor (discarding the
    // returned value) to avoid a double close.
    (void)fenceFD.release();
    m_acquireSemaphorePending = true;
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR) && defined(XR_USE_GRAPHICS_API_VULKAN)
