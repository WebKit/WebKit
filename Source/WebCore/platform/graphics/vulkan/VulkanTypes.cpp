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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VulkanTypes.h"

#if USE(VULKAN)
#include "GLContext.h"
#include "Logging.h"
#include "PlatformDisplay.h"
#include "VulkanUtilities.h"
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <wtf/Scope.h>
#include <wtf/text/CStringView.h>
#include <wtf/text/StringBuilder.h>

#if USE(GBM)
#include "DRMDeviceManager.h"
#include "GBMDevice.h"
#include "PlatformDisplayGBM.h"
#include <fcntl.h>
#include <gbm.h>
#endif

#if USE(LIBDRM)
#include <xf86drm.h>
#endif

// Epoxy does not yet define this macro as of version 1.5.10
#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

namespace WebCore {
namespace Vulkan {

ApplicationInfo::ApplicationInfo(const char* applicationName, uint32_t apiVersion)
{
    value().pApplicationName = applicationName;
    value().apiVersion = apiVersion;
}

InstanceCreateInfo::InstanceCreateInfo(const ApplicationInfo& applicationInfo, std::span<const char* const> enabledLayers, std::span<const char* const> enabledExtensions)
{
    value().pApplicationInfo = applicationInfo.ptr();

    value().enabledLayerCount = enabledLayers.size();
    value().ppEnabledLayerNames = enabledLayers.data();

    value().enabledExtensionCount = enabledExtensions.size();
    value().ppEnabledExtensionNames = enabledExtensions.data();
}

std::span<const uint8_t> PhysicalDeviceIDProperties::deviceUUID() const
{
    return unsafeMakeSpan(value().deviceUUID, VK_UUID_SIZE);
}

std::span<const uint8_t> PhysicalDeviceIDProperties::driverUUID() const
{
    return unsafeMakeSpan(value().driverUUID, VK_UUID_SIZE);
}

void PhysicalDevice::fillProperties(PhysicalDeviceProperties& properties) const
{
    vkGetPhysicalDeviceProperties2(ptr(), properties.ptr());
}

Vector<QueueFamilyProperties> PhysicalDevice::queueFamilies() const
{
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(ptr(), &queueFamilyCount, nullptr);

    Vector<QueueFamilyProperties> queueFamilies(queueFamilyCount);
    static_assert(sizeof(QueueFamilyProperties) == sizeof(VkQueueFamilyProperties));
    auto queueFamiliesSpan = spanReinterpretCast<VkQueueFamilyProperties>(queueFamilies.mutableSpan());
    ASSERT(queueFamiliesSpan.size() == queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ptr(), &queueFamilyCount, queueFamiliesSpan.data());

    return queueFamilies;
}

DeviceQueueCreateInfo::DeviceQueueCreateInfo(uint32_t familyIndex, std::span<const float> queuePriorities)
{
    value().queueFamilyIndex = familyIndex;
    value().queueCount = queuePriorities.size();
    value().pQueuePriorities = queuePriorities.data();
}

DeviceCreateInfo::DeviceCreateInfo(const DeviceQueueCreateInfo& queueCreateInfo, std::span<const char* const> enabledExtensions)
{
    value().queueCreateInfoCount = 1;
    value().pQueueCreateInfos = queueCreateInfo.ptr();

    value().enabledExtensionCount = enabledExtensions.size();
    value().ppEnabledExtensionNames = enabledExtensions.data();
}

Device::Device(VkDevice device)
    : Base(device)
{
    if (VkDevice device = ptr())
        volkLoadDeviceTable(&m_table, device);
}

Device::~Device()
{
    if (VkDevice device = ptr()) {
        ASSERT(m_table.vkDestroyDevice);
        m_table.vkDestroyDevice(device, nullptr);
    }
}

Result<Device> Device::create(PhysicalDevice& deviceInfo, const DeviceCreateInfo& creationInfo)
{
    VkDevice device;
    if (auto result = vkCreateDevice(deviceInfo.ptr(), creationInfo.ptr(), nullptr, &device); result != VK_SUCCESS)
        return makeUnexpected(result);

    return Device(device);
}

Device Device::s_sharedDevice { nullptr };

void Device::setSharedDevice(Device&& device)
{
    RELEASE_ASSERT_WITH_MESSAGE(!s_sharedDevice, "Attempted to reset already initialized Vulkan shared device");
    s_sharedDevice = WTF::move(device);
}

Device* Device::sharedDeviceIfExists()
{
    return s_sharedDevice ? &s_sharedDevice : nullptr;
}

Device& Device::sharedDevice()
{
    RELEASE_ASSERT_WITH_MESSAGE(s_sharedDevice, "Attempted to use Vulkan shared device before its initialization");
    return s_sharedDevice;
}

const Vector<VkLayerProperties>& Instance::availableLayers()
{
    static const auto layerProperties = ([]() -> Vector<VkLayerProperties> {
        uint32_t layerCount;
        if (auto result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr); result != VK_SUCCESS) {
            RELEASE_LOG_ERROR(Vulkan, "Cannot enumerate instance layers: %s", resultString(result));
            return { };
        }

        Vector<VkLayerProperties> result(layerCount);
        auto resultSpan = result.mutableSpan();
        ASSERT(resultSpan.size() == layerCount);

        if (auto result = vkEnumerateInstanceLayerProperties(&layerCount, resultSpan.data()); result != VK_SUCCESS) {
            RELEASE_LOG_ERROR(Vulkan, "Cannot enumerate instance layers: %s", resultString(result));
            return { };
        }

        for (const auto& item : result)
            RELEASE_LOG_DEBUG(Vulkan, "Available layer: %s (spec %" PRIu32 ", version %" PRIu32 ")", item.layerName, item.specVersion, item.implementationVersion);

        return result;
    })();
    return layerProperties;
}

bool Instance::hasLayers(std::span<const char* const> layerNames)
{
    return std::ranges::all_of(layerNames, [](auto* layerName) -> bool {
        return availableLayers().containsIf([layerName](const auto& layer) -> bool {
            return CStringView::unsafeFromUTF8(layerName) == CStringView::unsafeFromUTF8(layer.layerName);
        });
    });
}

bool Instance::hasLayer(const char* const layerName)
{
    std::array<const char* const, 1> layerNames = { layerName };
    return hasLayers(std::span(layerNames));
}

Result<Vector<VkExtensionProperties>> Instance::availableExtensions(const char* layerName)
{
    uint32_t extensionCount;
    if (auto result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr); result != VK_SUCCESS)
        return makeUnexpected(result);

    Vector<VkExtensionProperties> result(extensionCount);
    auto resultSpan = result.mutableSpan();
    ASSERT(resultSpan.size() == extensionCount);

    if (auto result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, resultSpan.data()); result != VK_SUCCESS)
        return makeUnexpected(result);

    return result;
}

bool Instance::hasExtensions(const Vector<VkExtensionProperties>& availableExtensions, std::span<const char* const> extensionNames)
{
    return std::ranges::all_of(extensionNames, [&availableExtensions](auto* extensionName) -> bool {
        return availableExtensions.containsIf([extensionName](const auto& extension) -> bool {
            return CStringView::unsafeFromUTF8(extensionName) == CStringView::unsafeFromUTF8(extension.extensionName);
        });
    });
}

bool Instance::hasExtension(const Vector<VkExtensionProperties>& availableExtensions, const char* const extensionName)
{
    std::array<const char* const, 1> extensionNames = { extensionName };
    return hasExtensions(availableExtensions, std::span(extensionNames));
}

bool Instance::hasExtensions(std::span<const char* const> extensionNames, const char* layerName)
{
    if (auto extensions = availableExtensions(layerName))
        return hasExtensions(*extensions, extensionNames);
    return false;
}

bool Instance::hasExtension(const char* const extensionName, const char *layerName)
{
    std::array<const char* const, 1> extensionNames = { extensionName };
    return hasExtensions(std::span(extensionNames), layerName);
}

Instance Instance::s_sharedInstance { nullptr };

void Instance::setSharedInstance(Instance&& instance)
{
    if (s_sharedInstance)
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Attempted to reset already initialized Vulkan shared instance");
    s_sharedInstance = WTF::move(instance);
}

Instance* Instance::sharedInstanceIfExists()
{
    return s_sharedInstance ? &s_sharedInstance : nullptr;
}

Instance& Instance::sharedInstance()
{
    if (!s_sharedInstance) [[unlikely]]
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("Attempted to use Vulkan shared instance before its initialization");
    return s_sharedInstance;
}

Result<Instance> Instance::create(const InstanceCreateInfo& creationInfo)
{
    VkInstance instance;
    if (auto result = vkCreateInstance(creationInfo.ptr(), nullptr, &instance); result != VK_SUCCESS)
        return makeUnexpected(result);

    return Instance(instance);
}

Instance::Instance(VkInstance instance)
    : Base(instance)
{
    if (VkInstance instance = ptr())
        volkLoadInstanceOnly(instance);
}

Instance::~Instance()
{
    if (VkInstance instance = ptr()) {
#ifdef VK_EXT_debug_utils
        if (m_debugMessenger)
            vkDestroyDebugUtilsMessengerEXT(instance, m_debugMessenger, nullptr);
#endif // VK_EXT_debug_utils

        vkDestroyInstance(instance, nullptr);
    }
}

Result<Vector<PhysicalDevice>> Instance::availableDevices() const
{
    uint32_t deviceCount;
    if (auto result = vkEnumeratePhysicalDevices(ptr(), &deviceCount, nullptr); result != VK_SUCCESS)
        return makeUnexpected(result);

    Vector<PhysicalDevice> devices(deviceCount);
    static_assert(sizeof(PhysicalDevice) == sizeof(VkPhysicalDevice));
    auto devicesSpan = spanReinterpretCast<VkPhysicalDevice>(devices.mutableSpan());
    ASSERT(devicesSpan.size() == deviceCount);
    if (auto result = vkEnumeratePhysicalDevices(ptr(), &deviceCount, devicesSpan.data()); result != VK_SUCCESS)
        return makeUnexpected(result);

    return devices;
}

#if USE(GBM)
static UnixFileDescriptor drmFileDescriptorForGBMDisplay()
{
    auto& deviceManager = DRMDeviceManager::singleton();
    if (!deviceManager.isInitialized())
        return { };

    // Same logic used in WebProcessGLib::initializePlatformDisplayIfNeeded() to obtain the gbm_device.
    if (auto device = deviceManager.mainGBMDevice(DRMDeviceManager::NodeType::Render))
        return { gbm_device_get_fd(device->device()), UnixFileDescriptor::Borrow };

    const auto& device = deviceManager.mainDevice();
    const CString& deviceNode = device.renderNode.isNull() ? device.primaryNode : device.renderNode;
    return { open(deviceNode.data(), O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
}
#endif // USE(GBM)

#if USE(LIBDRM)
static UnixFileDescriptor drmFileDescriptorForDisplay(const PlatformDisplay& display)
{
    auto eglDisplay = display.eglDisplay();
    if (eglDisplay == EGL_NO_DISPLAY)
        return { };

    if (!GLContext::isExtensionSupported(eglQueryString(nullptr, EGL_EXTENSIONS), "EGL_EXT_device_query"))
        return { };

    EGLDeviceEXT eglDevice;
    if (!eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice)))
        return { };

    const char* deviceExtensionsString = eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS);
    if (!GLContext::isExtensionSupported(deviceExtensionsString, "EGL_EXT_device_drm"))
        return { };

    // Prefer the render node path, if available; use the main device node otherwise.
    const char* devicePath = nullptr;
    if (GLContext::isExtensionSupported(deviceExtensionsString, "EGL_EXT_device_drm_render_node"))
        devicePath = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_RENDER_NODE_FILE_EXT);

    if (!devicePath || !*devicePath)
        devicePath = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_DEVICE_FILE_EXT);

    if (!devicePath || !*devicePath)
        return { };

    return { open(devicePath, O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
}
#endif // USE(LIBDRM)

Result<PhysicalDevice> Instance::deviceForDisplay(PlatformDisplay& display)
{
    auto devices = availableDevices();
    if (!devices)
        return makeUnexpected(devices.error());

    //
    // The best (exact!) match can be found by comparing the unique device and driver
    // identifiers provided by the GL_EXT_memory_object extension, because both OpenGL
    // and Vulkan will report the same values.
    //
    if (auto* context = display.sharingGLContext()) {
        GLContext::ScopedGLContextCurrent scopedContext(*context);
        RELEASE_LOG_DEBUG(Vulkan, "deviceForDisplay: GL_EXT_memory_object = %s", context->glExtensions().EXT_memory_object ? "yes" : "no");
        if (context->glExtensions().EXT_memory_object) {
            GLint deviceUUIDCount = 0;
            glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &deviceUUIDCount);
            RELEASE_LOG_DEBUG(Vulkan, "deviceForDisplay: found %d device UUIDs", deviceUUIDCount);
            if (deviceUUIDCount == 1) {
                std::array<GLubyte, GL_UUID_SIZE_EXT> deviceUUID;
                glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT, 0, deviceUUID.data());

                std::array<GLubyte, GL_UUID_SIZE_EXT> driverUUID;
                glGetUnsignedBytei_vEXT(GL_DRIVER_UUID_EXT, 0, driverUUID.data());

                auto index = devices->findIf([&deviceUUID, &driverUUID](const auto& deviceInfo) {
                    PhysicalDeviceProperties properties;
                    auto idProperties = properties.next<PhysicalDeviceIDProperties>();
                    deviceInfo.fillProperties(properties);
                    return equalSpans(std::span(deviceUUID), idProperties.deviceUUID())
                        && equalSpans(std::span(driverUUID), idProperties.driverUUID());
                });
                if (index != notFound) {
                    RELEASE_LOG_DEBUG(Vulkan, "deviceForDisplay: matched device/driver UUIDs");
                    return devices->at(index);
                }
            }
        }
    }

    //
    // The next best option is to obtain the PCI bus vendor and device identifiers,
    // and match those to the identifiers reported by Vulkan.
    //
    UnixFileDescriptor drmFileDescriptor;
#if USE(GBM)
    if (is<PlatformDisplayGBM>(display))
        drmFileDescriptor = drmFileDescriptorForGBMDisplay();
#endif

    if (!drmFileDescriptor)
        drmFileDescriptor = drmFileDescriptorForDisplay(display);

#if USE(LIBDRM)
    if (drmFileDescriptor) {
        drmDevice* device { nullptr };
        auto deviceScope = makeScopeExit([&device]() {
            if (device) {
                drmFreeDevice(&device);
                device = nullptr;
            }
        });
        if (!drmGetDevice(drmFileDescriptor.value(), &device) && device->bustype == DRM_BUS_PCI) {
            auto index = devices->findIf([pciInfo = device->deviceinfo.pci](const auto& deviceInfo) {
                PhysicalDeviceProperties properties;
                deviceInfo.fillProperties(properties);
                return pciInfo->vendor_id == properties->vendorID && pciInfo->device_id == properties->deviceID;
            });
            if (index != notFound) {
                RELEASE_LOG_DEBUG(Vulkan, "deviceForDisplay: matched PCI device %04x:%04x",
                    device->deviceinfo.pci->vendor_id, device->deviceinfo.pci->device_id);
                return devices->at(index);
            }
        }
    }
#endif // USE(LIBDRM)

    //
    // Try to match the major+minor device numbers.
    //
    struct stat statBuffer;
    if (drmFileDescriptor && !fstat(drmFileDescriptor.value(), &statBuffer)) {
        auto index = devices->findIf([deviceID = statBuffer.st_dev](const auto& deviceInfo) {
            PhysicalDeviceProperties properties;
            auto drmProperties = properties.next<PhysicalDeviceDRMProperties>();
            deviceInfo.fillProperties(properties);
            return (major(deviceID) == drmProperties->renderMajor && minor(deviceID) == drmProperties->renderMinor)
                || (major(deviceID) == drmProperties->primaryMajor && minor(deviceID) == drmProperties->primaryMinor);
        });
        if (index != notFound) {
            RELEASE_LOG_DEBUG(Vulkan, "deviceForDisplay: matched device major=%d, minor=%d", major(statBuffer.st_dev), minor(statBuffer.st_dev));
            return devices->at(index);
        }
    }

    //
    // As a last resort, try to split the list of devices in software renderers (CPU based)
    // and hardware ones (the rest). If the resulting set has only one device of the same
    // type as the GL display, that one must be the device to use.
    //
    // Note that it is better to NOT pick a device (and avoid using Vulkan) if it is not
    // possible to be completely sure that both OpenGL and Vulkan will be using the same
    // device. Therefore, a choice is only made if there is only a single Vulkan device.
    // Splitting the list of devices in software-based and actual GPUs increases
    // chances of covering single-GPU setups where swrast is also installed.
    //
    Vector<PhysicalDevice, 2> softwareDevices;
    Vector<PhysicalDevice, 2> hardwareDevices;
    for (auto& deviceInfo : *devices) {
        PhysicalDeviceProperties properties;
        deviceInfo.fillProperties(properties);
        if (properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
            softwareDevices.append(deviceInfo);
        else
            hardwareDevices.append(deviceInfo);
    }

    if (display.glDisplay().isSoftwareRendered()) {
        if (softwareDevices.size() == 1) {
            RELEASE_LOG_DEBUG(Vulkan, "deviceForDisplay: matched single software-based device");
            return softwareDevices[0];
        }
    } else {
        if (hardwareDevices.size() == 1) {
            RELEASE_LOG_DEBUG(Vulkan, "deviceForDisplay: matched single hardware-based device");
            return hardwareDevices[0];
        }
    }

    return makeUnexpected(VK_ERROR_DEVICE_LOST);
}

#ifdef VK_EXT_debug_utils
struct DebugUtilsMessengerCreateInfo : Structure<VkDebugUtilsMessengerCreateInfoEXT, VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT> {
};

static VkBool32 debugUtilsMessengerHandleMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    ASSERT(data->pMessage);

    const WTFLogLevel logLevel = [messageSeverity] {
        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return WTFLogLevel::Error;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return WTFLogLevel::Info;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return WTFLogLevel::Warning;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return WTFLogLevel::Debug;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }();

    StringBuilder messageTypeString;
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        messageTypeString.append("General, "_s);
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        messageTypeString.append("Validation, "_s);
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        messageTypeString.append("Performance, "_s);
    RELEASE_ASSERT(messageTypeString.length() > 2);
    messageTypeString.shrink(messageTypeString.length() - 2);

    if (data->pMessageIdName)
        RELEASE_LOG_WITH_LEVEL(Vulkan, logLevel, "[%s: %s] %s", data->pMessageIdName, messageTypeString.toString().utf8().data(), data->pMessage);
    else
        RELEASE_LOG_WITH_LEVEL(Vulkan, logLevel, "[%s] %s", messageTypeString.toString().utf8().data(), data->pMessage);

    return VK_FALSE;
}

VkResult Instance::installDebugMessenger()
{
    if (m_debugMessenger)
        return VK_SUCCESS;

    DebugUtilsMessengerCreateInfo createInfo;
    createInfo->pfnUserCallback = debugUtilsMessengerHandleMessage;
    createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    if (LOG_CHANNEL(Vulkan).level >= WTFLogLevel::Info)
        createInfo->messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    if (LOG_CHANNEL(Vulkan).level >= WTFLogLevel::Warning)
        createInfo->messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    if (LOG_CHANNEL(Vulkan).level >= WTFLogLevel::Debug)
        createInfo->messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

    return vkCreateDebugUtilsMessengerEXT(ptr(), createInfo.ptr(), nullptr, &m_debugMessenger);
}
#else
VkResult Instance::installDebugMessenger() const
{
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
#endif // VK_EXT_debug_utils

} // namespace Vulkan
} // namespace WebCore

#endif // USE(VULKAN)
