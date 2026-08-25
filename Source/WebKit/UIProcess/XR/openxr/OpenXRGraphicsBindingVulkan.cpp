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
#include <cstdlib>
#include <cstring>
#include <span>
#include <utility>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringView.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebKit {

// Vulkan non-dispatchable handles (VkImage, VkDeviceMemory, ...) are 8 bytes on every architecture (a pointer where pointers are 64-bit, a uint64_t otherwise).
// bit_cast copies the raw bytes so is correct for both representations (reinterpret_cast would assume a pointer).
static_assert(sizeof(VkImage) == sizeof(uint64_t), "VkImage is expected to be a 64-bit handle");

static inline uint64_t toHandle(VkImage image)
{
    return std::bit_cast<uint64_t>(image);
}

static inline VkImage toVkImage(uint64_t handle)
{
    return std::bit_cast<VkImage>(handle);
}

template<typename Handle> static inline uint64_t toDebugHandle(Handle handle)
{
    static_assert(sizeof(Handle) == sizeof(uint64_t), "Vulkan non-dispatchable handles are expected to be 64-bit");
    return std::bit_cast<uint64_t>(handle);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
{
    if (!callbackData || !callbackData->pMessage)
        return VK_FALSE;

    [[maybe_unused]] const char* severityLabel = "info";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severityLabel = "error";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severityLabel = "warning";

    [[maybe_unused]] const char* typeLabel = "general";
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        typeLabel = "validation";
    else if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        typeLabel = "performance";

    LOG(XR, "OpenXR Vulkan [%s/%s] %s", severityLabel, typeLabel, callbackData->pMessage);

    // Always VK_FALSE because returning VK_TRUE aborts the offending call, which would turn a diagnostic into a behaviour change.
    return VK_FALSE;
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
    // Unlike the OpenGLES binding, Vulkan must create its instance and device through the OpenXR runtime (xrCreateVulkan{Instance|Device}KHR), which requires the OpenXR instance
    // and system to already exist. That work is deferred to initializeForSession() so there is nothing to set up before instance creation here.
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

    // maxApiVersionSupported might return non-sensical large values standing for "no maximum" (as in the case of Monado for desktop). This could wrap into a version
    // below 1.1 and hide extensions whose dependencies are core from 1.1, so protect against that.
    static constexpr uint32_t plausibleMajorLimit = 8;
    auto maxMajor = static_cast<uint32_t>(XR_VERSION_MAJOR(graphicsRequirements.maxApiVersionSupported));
    auto maxMinor = static_cast<uint32_t>(XR_VERSION_MINOR(graphicsRequirements.maxApiVersionSupported));
    if (maxMajor <= plausibleMajorLimit && (maxMajor < apiMajor || (maxMajor == apiMajor && maxMinor < apiMinor))) {
        apiMajor = maxMajor;
        apiMinor = maxMinor;
    }

    // The binding calls Vulkan 1.1 core entry points unconditionally, vkGetPhysicalDeviceProperties2 for the device and driver UUIDs and
    // VkMemoryDedicatedAllocateInfo for the export, so below that there is nothing to fallback to and the instance would be unusable.
    if (apiMajor < 1 || (apiMajor == 1 && !apiMinor)) {
        LOG(XR, "OpenXR Vulkan: Vulkan %u.%u is available but this binding requires at least 1.1", apiMajor, apiMinor);
        return false;
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
    applicationInfo.pApplicationName = "WebKit";
    applicationInfo.apiVersion = apiVersion;

    Vector<const char*> instanceLayers;
    Vector<const char*> instanceExtensions;
    VkValidationFeaturesEXT validationFeatures { };
    bool validationEnabled = configureValidation(instanceLayers, instanceExtensions, validationFeatures);

    VkInstanceCreateInfo vkInstanceCreateInfo { };
    vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstanceCreateInfo.pApplicationInfo = &applicationInfo;
    if (validationFeatures.sType)
        vkInstanceCreateInfo.pNext = &validationFeatures;
    vkInstanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
    vkInstanceCreateInfo.ppEnabledLayerNames = instanceLayers.span().data();
    vkInstanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    vkInstanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.span().data();

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

    // After volkLoadInstanceTable which resolves vkCreateDebugUtilsMessengerEXT. Messages from instance creation itself are reported directly by the layer.
    if (validationEnabled)
        createDebugMessenger();

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

    // Extensions to hand our own images to the WebProcess and to import its frame fence as a semaphore. Everything else the sharing needs (external memory and semaphores
    // in general, dedicated allocation, bind-memory2...) is core from Vulkan 1.1, which the instance created above is required to be.
    Vector<const char*> requiredVulkanExtensions {
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    };

    // Report which device the runtime selected and whether it advertises the extensions we need, so a VK_ERROR_EXTENSION_NOT_PRESENT from xrCreateVulkanDeviceKHR below points
    // at the actual cause (e.g. the runtime picked a software device that lacks DMABuf export) rather than a guess.
    VkPhysicalDeviceProperties physicalDeviceProperties;
    m_instanceTable.vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &physicalDeviceProperties);
    LOG(XR, "OpenXR Vulkan: runtime selected device '%s'", physicalDeviceProperties.deviceName);
    readDeviceAndDriverUUIDs(physicalDeviceProperties.deviceName);
    uint32_t availableExtensionCount = 0;
    m_instanceTable.vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &availableExtensionCount, nullptr);
    Vector<VkExtensionProperties> availableVulkanExtensions(availableExtensionCount);
    m_instanceTable.vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &availableExtensionCount, availableVulkanExtensions.mutableSpan().data());
    auto isVulkanExtensionAvailable = [&](const char* name) {
        return availableVulkanExtensions.containsIf([&](const auto& extension) {
            return StringView::fromLatin1(extension.extensionName) == StringView::fromLatin1(name);
        });
    };

    Vector<const char*> enabledVulkanExtensions;
    for (auto* required : requiredVulkanExtensions) {
        if (!isVulkanExtensionAvailable(required))
            LOG(XR, "OpenXR Vulkan: required device extension %s not supported by '%s'", required, physicalDeviceProperties.deviceName);
        enabledVulkanExtensions.append(required);
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
    vkDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledVulkanExtensions.size());
    vkDeviceCreateInfo.ppEnabledExtensionNames = enabledVulkanExtensions.span().data();

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
    // The per-swapchain-image commit command buffers are recorded once and never reset or re-recorded, so no pool flags are needed. They are freed in bulk when the pool is destroyed.
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

void OpenXRGraphicsBindingVulkan::readDeviceAndDriverUUIDs(const char* deviceName)
{
    // The device the runtime dictates has to be the one the WebProcess imports on, and for handle types that are driver private (the opaque fds) the driver
    // must match too, not just the device. EXT_external_objects makes that comparison the defined legality test, so these travel with every exported handle and
    // the WebProcess refuses anything it cannot match.
    VkPhysicalDeviceIDProperties idProperties { };
    idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceProperties2 properties2 { };
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &idProperties;
    m_instanceTable.vkGetPhysicalDeviceProperties2(m_vkPhysicalDevice, &properties2);

    auto pack = [](std::span<const uint8_t, VK_UUID_SIZE> uuid) {
        std::array<uint64_t, 2> packed { };
        static_assert(sizeof(packed) == VK_UUID_SIZE);
        memcpySpan(asMutableByteSpan(std::span<uint64_t, 2> { packed }), uuid);
        return packed;
    };
    m_deviceUUID = pack(std::span<const uint8_t, VK_UUID_SIZE> { idProperties.deviceUUID });
    m_driverUUID = pack(std::span<const uint8_t, VK_UUID_SIZE> { idProperties.driverUUID });

    [[maybe_unused]] auto toHex = [](std::span<const uint8_t, VK_UUID_SIZE> uuid) {
        static constexpr std::array<char, 16> hexDigits { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
        std::array<char, VK_UUID_SIZE * 2 + 1> text { };
        for (size_t i = 0; i < VK_UUID_SIZE; ++i) {
            text[i * 2] = hexDigits[(uuid[i] >> 4) & 0xf];
            text[i * 2 + 1] = hexDigits[uuid[i] & 0xf];
        }
        return text;
    };

    LOG(XR, "OpenXR Vulkan: device '%s' deviceUUID %s driverUUID %s", deviceName,
        toHex(std::span<const uint8_t, VK_UUID_SIZE> { idProperties.deviceUUID }).data(),
        toHex(std::span<const uint8_t, VK_UUID_SIZE> { idProperties.driverUUID }).data());
}

// Optional diagnostics, opt-in via WEBKIT_WEBXR_VULKAN_VALIDATION. Requires the validation layer and VK_EXT_debug_utils to be enabled at instance creation.
bool OpenXRGraphicsBindingVulkan::configureValidation(Vector<const char*>& layers, Vector<const char*>& extensions, VkValidationFeaturesEXT& features)
{
    auto value = StringView::fromLatin1(getenv("WEBKIT_WEBXR_VULKAN_VALIDATION"));
    const bool shouldEnableVulkanValidation = !value.isEmpty() && value != "0"_s;
    if (!shouldEnableVulkanValidation)
        return false;

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    Vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.mutableSpan().data());
    bool hasValidationLayer = availableLayers.containsIf([](const auto& layer) {
        return StringView::fromLatin1(layer.layerName) == "VK_LAYER_KHRONOS_validation"_s;
    });

    uint32_t instanceExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
    Vector<VkExtensionProperties> availableInstanceExtensions(instanceExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, availableInstanceExtensions.mutableSpan().data());
    bool hasDebugUtils = availableInstanceExtensions.containsIf([](const auto& extension) {
        return StringView::fromLatin1(extension.extensionName) == VK_EXT_DEBUG_UTILS_EXTENSION_NAME ""_s;
    });

    if (!hasValidationLayer || !hasDebugUtils) {
        LOG(XR, "OpenXR Vulkan: WEBKIT_WEBXR_VULKAN_VALIDATION is set but %s is unavailable; continuing without validation",
            hasValidationLayer ? VK_EXT_DEBUG_UTILS_EXTENSION_NAME : "VK_LAYER_KHRONOS_validation");
        return false;
    }

    layers.append("VK_LAYER_KHRONOS_validation");
    extensions.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // VK_EXT_validation_features is provided by the layer, not the driver, so it is absent from the unfiltered instance extension list and must be queried
    // against the layer. Enabling it when absent fails instance creation, and chaining VkValidationFeaturesEXT without enabling it is invalid, so require it first.
    uint32_t layerExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &layerExtensionCount, nullptr);
    Vector<VkExtensionProperties> layerExtensions(layerExtensionCount);
    vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &layerExtensionCount, layerExtensions.mutableSpan().data());
    if (!layerExtensions.containsIf([](const auto& extension) { return StringView::fromLatin1(extension.extensionName) == VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME ""_s; })) {
        LOG(XR, "OpenXR Vulkan: %s unavailable; core validation is enabled, synchronization validation is not", VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        return true;
    }

    static constexpr std::array<VkValidationFeatureEnableEXT, 1> featureEnables { {
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    } };
    extensions.append(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.enabledValidationFeatureCount = static_cast<uint32_t>(featureEnables.size());
    features.pEnabledValidationFeatures = featureEnables.data();
    return true;
}

void OpenXRGraphicsBindingVulkan::createDebugMessenger()
{
    if (!m_instanceTable.vkCreateDebugUtilsMessengerEXT) {
        LOG(XR, "OpenXR Vulkan: vkCreateDebugUtilsMessengerEXT unavailable; validation messages will not be routed and objects will not be named");
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo { };
    messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerCreateInfo.pfnUserCallback = vulkanDebugUtilsCallback;
    if (m_instanceTable.vkCreateDebugUtilsMessengerEXT(m_vkInstance, &messengerCreateInfo, nullptr, &m_debugMessenger) == VK_SUCCESS)
        LOG(XR, "OpenXR Vulkan: validation enabled, debug messenger active");
    else
        LOG(XR, "OpenXR Vulkan: vkCreateDebugUtilsMessengerEXT failed; validation messages will not be routed and objects will not be named");
}

void OpenXRGraphicsBindingVulkan::setDebugName(VkObjectType objectType, uint64_t objectHandle, const char* name)
{
    if (!objectHandle || m_debugMessenger == VK_NULL_HANDLE || !m_instanceTable.vkSetDebugUtilsObjectNameEXT)
        return;

    VkDebugUtilsObjectNameInfoEXT nameInfo { };
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name;
    m_instanceTable.vkSetDebugUtilsObjectNameEXT(m_vkDevice, &nameInfo);
}

const void* OpenXRGraphicsBindingVulkan::sessionGraphicsBinding() const
{
    return &m_graphicsBinding;
}

int64_t OpenXRGraphicsBindingVulkan::selectColorFormat(const Vector<int64_t>& supportedFormats, bool) const
{
    // OpenXR reports Vulkan swapchain formats as VkFormat values. Prefer 8-bit RGBA. alpha is kept even when not requested, mirroring the OpenGLES binding.
    // UNORM is preferred over SRGB because the exported image shares the swapchain's format and the copy between them has to stay verbatim: the web process hands
    // over sRGB-encoded bytes in a plain RGBA8 texture, and an sRGB format would have GL convert on access. The cost is that the runtime's compositor treats the
    // contents as linear, so any blending or reprojection it does happens on encoded values.
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
    if (m_vkDevice == VK_NULL_HANDLE && m_vkInstance == VK_NULL_HANDLE)
        return;

    m_pendingFenceFD = { };

    if (m_vkDevice != VK_NULL_HANDLE) {
        m_deviceTable.vkDeviceWaitIdle(m_vkDevice);

        for (auto& exportedImage : m_exportedImages.values())
            destroyExportedImage(exportedImage);
        m_exportedImages.clear();

        if (m_commandPool != VK_NULL_HANDLE) {
            m_deviceTable.vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        m_deviceTable.vkDestroyDevice(m_vkDevice, nullptr);
        m_vkDevice = VK_NULL_HANDLE;
        m_deviceTable = { };
    }
    m_vkQueue = VK_NULL_HANDLE;
    m_vkPhysicalDevice = VK_NULL_HANDLE;

    if (m_debugMessenger != VK_NULL_HANDLE) {
        if (m_instanceTable.vkDestroyDebugUtilsMessengerEXT)
            m_instanceTable.vkDestroyDebugUtilsMessengerEXT(m_vkInstance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

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
    if (exportedImage.renderFinishedSemaphore != VK_NULL_HANDLE)
        m_deviceTable.vkDestroySemaphore(m_vkDevice, exportedImage.renderFinishedSemaphore, nullptr);
    if (exportedImage.inFlightFence != VK_NULL_HANDLE)
        m_deviceTable.vkDestroyFence(m_vkDevice, exportedImage.inFlightFence, nullptr);
    if (exportedImage.image != VK_NULL_HANDLE)
        m_deviceTable.vkDestroyImage(m_vkDevice, exportedImage.image, nullptr);
    if (exportedImage.memory != VK_NULL_HANDLE)
        m_deviceTable.vkFreeMemory(m_vkDevice, exportedImage.memory, nullptr);
    // The command buffer is owned by m_commandPool and freed when the pool is destroyed.
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

static constexpr uint32_t glRGBA8 = 0x8058;
static uint32_t glInternalFormatForVkFormat(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return glRGBA8;
    default:
        return 0;
    }
}

// An image shared with GL must be created with every usage its format supports: GL cannot state which usages it assumed, and the layout a driver picks for
// VK_IMAGE_TILING_OPTIMAL depends on them, so anything less leaves the two sides agreeing on the layout by coincidence. The GL_EXT_memory_object spec puts it as
// "all supported usage flags must be specified". This mirrors the same derivation in ANGLE's vk::GetMaximalImageUsageFlags().
VkImageUsageFlags OpenXRGraphicsBindingVulkan::maximalImageUsageFlags(VkFormat format) const
{
    VkFormatProperties2 formatProperties { };
    formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    m_instanceTable.vkGetPhysicalDeviceFormatProperties2(m_vkPhysicalDevice, format, &formatProperties);
    auto features = formatProperties.formatProperties.optimalTilingFeatures;

    VkImageUsageFlags usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return usage;
}

bool OpenXRGraphicsBindingVulkan::supportsOpaqueFDSharing(VkFormat format, VkImageUsageFlags usage) const
{
    VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo { };
    externalImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalImageFormatInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo { };
    imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    imageFormatInfo.pNext = &externalImageFormatInfo;
    imageFormatInfo.format = format;
    imageFormatInfo.type = VK_IMAGE_TYPE_2D;
    imageFormatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageFormatInfo.usage = usage;

    VkExternalImageFormatProperties externalImageFormatProperties { };
    externalImageFormatProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 imageFormatProperties { };
    imageFormatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    imageFormatProperties.pNext = &externalImageFormatProperties;

    if (m_instanceTable.vkGetPhysicalDeviceImageFormatProperties2(m_vkPhysicalDevice, &imageFormatInfo, &imageFormatProperties) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: format %d with usage 0x%x is not creatable at all", format, usage);
        return false;
    }
    if (!(externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT)) {
        RELEASE_LOG(XR, "OpenXR Vulkan: format %d with usage 0x%x is not exportable as an opaque fd", format, usage);
        return false;
    }

    // The semaphores must cross the process boundary in both directions, so unlike the memory they have to be importable too.
    VkPhysicalDeviceExternalSemaphoreInfo semaphoreInfo { };
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
    semaphoreInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkExternalSemaphoreProperties semaphoreProperties { };
    semaphoreProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
    m_instanceTable.vkGetPhysicalDeviceExternalSemaphoreProperties(m_vkPhysicalDevice, &semaphoreInfo, &semaphoreProperties);

    static constexpr VkExternalSemaphoreFeatureFlags requiredSemaphoreFeatures = VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
    if ((semaphoreProperties.externalSemaphoreFeatures & requiredSemaphoreFeatures) != requiredSemaphoreFeatures) {
        RELEASE_LOG(XR, "OpenXR Vulkan: opaque fd semaphores are not both exportable and importable");
        return false;
    }

    return true;
}

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

    setDebugName(VK_OBJECT_TYPE_FENCE, toDebugHandle(exportedImage.inFlightFence), "WebXR exported image in-flight fence");
    setDebugName(VK_OBJECT_TYPE_SEMAPHORE, toDebugHandle(exportedImage.acquireSemaphore), "WebXR exported image acquire semaphore");
    return true;
}

// Both directions of the handover are ordinary binary semaphores, only their exportability is special. GL_EXT_semaphore_fd cannot be exported, so the WebProcess imports these
// instead of creating its own.
bool OpenXRGraphicsBindingVulkan::createExportedImageSharingSemaphores(ExportedImage& exportedImage)
{
    VkExportSemaphoreCreateInfo exportSemaphoreInfo { };
    exportSemaphoreInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    exportSemaphoreInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo { };
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &exportSemaphoreInfo;

    if (m_deviceTable.vkCreateSemaphore(m_vkDevice, &semaphoreCreateInfo, nullptr, &exportedImage.renderFinishedSemaphore) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkCreateSemaphore failed for the exported image's render-finished semaphore");
        return false;
    }

    setDebugName(VK_OBJECT_TYPE_SEMAPHORE, toDebugHandle(exportedImage.renderFinishedSemaphore), "WebXR exported image render-finished semaphore");
    return true;
}

// Shares the image purely in Vulkan terms, its memory is exported as an opaque fd and the WebOrocess imports it with GL_EXT_memory_object_fd (no DMABuf, GBM, AHardwareBuffer...).
// Three things make this legal: 1. the two drivers must report the same device and driver UUIDs (enforced on the WebProcess), the image must carry every usage its format supports (see
// maximalImageUsageFlags() above), and the format must have an exact GL counterpart.
std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRGraphicsBindingVulkan::exportTexture(uint64_t swapchainImage, const OpenXRSwapchain& swapchain, TextureType type, uint32_t width, uint32_t height)
{
    ASSERT(m_vkDevice != VK_NULL_HANDLE);

    if (type != TextureType::Texture2D) {
        // FIXME: cube layers are not implemented yet for the Vulkan binding.
        return std::nullopt;
    }

    auto format = static_cast<VkFormat>(swapchain.format());
    auto glInternalFormat = glInternalFormatForVkFormat(format);
    if (!glInternalFormat) {
        RELEASE_LOG(XR, "OpenXR Vulkan: swapchain format %d has no exact GL counterpart, refusing to share it as an opaque fd", format);
        return std::nullopt;
    }

    auto usage = maximalImageUsageFlags(format);
    if (!supportsOpaqueFDSharing(format, usage))
        return std::nullopt;

    VkExternalMemoryImageCreateInfo externalMemoryImageInfo { };
    externalMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

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
        RELEASE_LOG(XR, "OpenXR Vulkan: vkCreateImage failed for the opaque fd exported texture");
        return std::nullopt;
    }
    setDebugName(VK_OBJECT_TYPE_IMAGE, toHandle(exportedImage.image), "WebXR exported image");

    auto cleanupOnError = makeScopeExit([&] {
        destroyExportedImage(exportedImage);
    });

    VkMemoryRequirements memoryRequirements;
    m_deviceTable.vkGetImageMemoryRequirements(m_vkDevice, exportedImage.image, &memoryRequirements);

    auto memoryTypeIndex = findMemoryType(m_instanceTable, m_vkPhysicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryTypeIndex) {
        RELEASE_LOG(XR, "OpenXR Vulkan: no suitable memory type for the opaque fd exported texture");
        return std::nullopt;
    }

    // Dedicated and the consumer sets GL_DEDICATED_MEMORY_OBJECT_EXT to match. Both sides have to agree on whether the allocation belongs to this one image or the import is invalid.
    VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo { };
    dedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocateInfo.image = exportedImage.image;

    VkExportMemoryAllocateInfo exportAllocateInfo { };
    exportAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportAllocateInfo.pNext = &dedicatedAllocateInfo;
    exportAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkMemoryAllocateInfo allocateInfo { };
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = &exportAllocateInfo;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = *memoryTypeIndex;

    if (m_deviceTable.vkAllocateMemory(m_vkDevice, &allocateInfo, nullptr, &exportedImage.memory) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkAllocateMemory failed for the opaque fd exported texture");
        return std::nullopt;
    }
    setDebugName(VK_OBJECT_TYPE_DEVICE_MEMORY, toDebugHandle(exportedImage.memory), "WebXR exported image memory");

    if (m_deviceTable.vkBindImageMemory(m_vkDevice, exportedImage.image, exportedImage.memory, 0) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkBindImageMemory failed for the opaque fd exported texture");
        return std::nullopt;
    }

    VkMemoryGetFdInfoKHR memoryGetFdInfo { };
    memoryGetFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    memoryGetFdInfo.memory = exportedImage.memory;
    memoryGetFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    int memoryFd = -1;
    if (m_deviceTable.vkGetMemoryFdKHR(m_vkDevice, &memoryGetFdInfo, &memoryFd) != VK_SUCCESS) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkGetMemoryFdKHR failed for the opaque fd exported texture");
        return std::nullopt;
    }
    WTF::UnixFileDescriptor memoryDescriptor { memoryFd, WTF::UnixFileDescriptor::Adopt };

    if (!createExportedImageSyncObjects(exportedImage) || !createExportedImageSharingSemaphores(exportedImage))
        return std::nullopt;

    auto exportSemaphore = [&](VkSemaphore semaphore) -> WTF::UnixFileDescriptor {
        VkSemaphoreGetFdInfoKHR semaphoreGetFdInfo { };
        semaphoreGetFdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        semaphoreGetFdInfo.semaphore = semaphore;
        semaphoreGetFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        int fd = -1;
        if (m_deviceTable.vkGetSemaphoreFdKHR(m_vkDevice, &semaphoreGetFdInfo, &fd) != VK_SUCCESS)
            return { };
        return WTF::UnixFileDescriptor { fd, WTF::UnixFileDescriptor::Adopt };
    };

    auto renderFinishedDescriptor = exportSemaphore(exportedImage.renderFinishedSemaphore);
    if (!renderFinishedDescriptor) {
        RELEASE_LOG(XR, "OpenXR Vulkan: vkGetSemaphoreFdKHR failed for the exported image's render-finished semaphore");
        return std::nullopt;
    }

    cleanupOnError.release();
    m_exportedImages.set(swapchainImage, WTF::move(exportedImage));

    return PlatformXR::FrameData::ExternalTexture {
        .memory = WTF::move(memoryDescriptor),
        .allocationSize = memoryRequirements.size,
        .glInternalFormat = glInternalFormat,
        .renderFinishedSemaphore = WTF::move(renderFinishedDescriptor),
        .deviceUUID = m_deviceUUID,
        .driverUUID = m_driverUUID,
    };
}

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

// Vulkan has none of GL's implicit ordering: the GPU may overlap and reorder work, its caches are not coherent between stages, and an image is stored in a layout
// chosen for how it is about to be used. So every access here has to be stated, which is what the barriers below do. One VkImageMemoryBarrier covers three separate
// things at once, and each of the three is why one of its arguments exists:
//   1. Execution. The stage masks passed to vkCmdPipelineBarrier order the work recorded before it against the work recorded after it.
//   2. Memory. srcAccessMask flushes the writer's caches and dstAccessMask invalidates the reader's, because ordering alone does not make the bytes visible.
//   3. Layout. The driver may physically rearrange the image between oldLayout and newLayout. GL has no such concept, which is why the consumer has to name the
//      layout it leaves the shared image in when it signals its semaphore, and why that layout is repeated on this side rather than inferred.
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
    // No VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT as this buffer is recorded once and resubmitted on every commit.
    m_deviceTable.vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Neither barrier transfers queue family ownership, as both images are only ever touched by drivers on this device. The exported image is already in
    // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL because the consumer left it there, spelling that layout GL_LAYOUT_TRANSFER_SRC_EXT since GL has none of its own, so this barrier
    // does not transition it and exists only to make its writes visible to the transfer read. The swapchain image is ours and the blit overwrites all of it, so it starts from UNDEFINED.
    std::array<VkImageMemoryBarrier, 2> preBlitBarriers { {
        makeImageBarrier(exportedImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, VK_ACCESS_TRANSFER_READ_BIT),
        makeImageBarrier(swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT),
    } };
    m_deviceTable.vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, preBlitBarriers.size(), preBlitBarriers.data());

    // The WebProcess renders into the exported image with OpenGL, whose framebuffer origin is the bottom left, whereas Vulkan (and therefore the runtime's swapchain image)
    // uses the top left. We blit rather than copy so the destination's Y offsets can be inverted, correcting the flip in the same pass (vkCmdCopyImage cannot flip).
    // The transfer is 1:1 in size and format so NEAREST adds no filtering, and the Y inversion itself costs nothing beyond the blit.
    VkImageBlit blitRegion {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(exportedImage.width), static_cast<int32_t>(exportedImage.height), 1 } },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { { 0, static_cast<int32_t>(exportedImage.height), 0 }, { static_cast<int32_t>(exportedImage.width), 0, 1 } },
    };
    m_deviceTable.vkCmdBlitImage(commandBuffer, exportedImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

    // Transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL, which the runtime expects when it is released. The exported image
    // needs nothing, it stays in TRANSFER_SRC_OPTIMAL, which is what the consumer leaves it in and what the next blit reads it as.
    std::array<VkImageMemoryBarrier, 1> postBlitBarriers { {
        makeImageBarrier(swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
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

    // No fence means the consumer did not release this image, so there is no renderFinished signal outstanding and nothing was rendered.
    // The two are produced together on purpose, waiting on an unsignalled binary semaphore would wedge the queue, and this is what
    // lets us know a signal exists without a separate flag over IPC.
    if (!pendingFenceFD) {
        RELEASE_LOG(XR, "OpenXR Vulkan: no frame fence for this commit, the consumer did not hand the image back");
        return;
    }

    VkImage swapchainImage = toVkImage(swapchain.acquiredTexture());
    ASSERT(swapchainImage != VK_NULL_HANDLE);

    // The blit is identical every time this swapchain image is acquired so its command buffer is recorded once and resubmitted on every commit.
    if (exportedImage.commandBuffer == VK_NULL_HANDLE && !recordBlitCommandBuffer(exportedImage, swapchainImage))
        return;

    // Wait for this image's previous submission to retire before reusing its command buffer and acquire semaphore, then reset
    // the fence for this frame's submission. The fence was created signaled and the image was last used a full swapchain cycle
    // ago, so this almost always returns immediately; it only blocks if the GPU has fallen behind by the swapchain depth.
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

    // The blit waits for the consumer on two counts: renderFinishedSemaphore is the other half of its glSignalSemaphoreEXT, and
    // acquireSemaphore carries the render completion fence. Waiting on both is redundant in the common case but neither implies the
    // other, and the wait is free once the work is done. inFlightFence is signalled so the next reuse of this image can tell the
    // previous submission has retired. Nothing has to be signalled for the consumer: xrWaitSwapchainImage will not let it render into
    // this image again until the runtime has finished with the swapchain image this blit writes, which cannot happen before the blit
    // completes.
    std::array<VkSemaphore, 2> waitSemaphores { exportedImage.renderFinishedSemaphore, exportedImage.acquireSemaphore };
    std::array<VkPipelineStageFlags, 2> waitStages { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };
    VkSubmitInfo submitInfo { };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitOnAcquireFence ? 2 : 1;
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
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
