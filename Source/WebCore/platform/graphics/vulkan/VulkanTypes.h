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

#pragma once

#if USE(VULKAN)
#include "VulkanHandle.h"
#include "VulkanStructure.h"
#include <expected>
#include <wtf/text/WTFString.h>

namespace WebCore {

class PlatformDisplay;

namespace Vulkan {

template <typename Type>
using Result = std::expected<Type, VkResult>;

struct ApplicationInfo : Structure<VkApplicationInfo, VK_STRUCTURE_TYPE_APPLICATION_INFO> {
    ApplicationInfo(const String& applicationName, uint32_t apiVersion = VK_API_VERSION_1_3)
        : ApplicationInfo(applicationName.utf8().data(), apiVersion)
    {
    }

private:
    ApplicationInfo(const char* applicationName, uint32_t apiVersion);
};

struct InstanceCreateInfo : Structure<VkInstanceCreateInfo, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO> {
    InstanceCreateInfo(const ApplicationInfo& applicationInfo LIFETIME_BOUND, std::span<const char* const> enabledLayers LIFETIME_BOUND = { }, std::span<const char* const> enabledExtensions LIFETIME_BOUND = { });
};

using QueueFamilyProperties = VkQueueFamilyProperties;

struct DeviceQueueCreateInfo : Structure<VkDeviceQueueCreateInfo, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO> {
    DeviceQueueCreateInfo(uint32_t familyIndex, std::span<const float> queuePriorities);
};

struct PhysicalDeviceProperties : Structure<VkPhysicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2> {
    const VkPhysicalDeviceProperties* LIFETIME_BOUND operator->() const { return &ptr()->properties; }
    VkPhysicalDeviceProperties* LIFETIME_BOUND operator->() { return &ptr()->properties; }
};

struct PhysicalDeviceDRMProperties : Structure<VkPhysicalDeviceDrmPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT> {
};

struct PhysicalDeviceIDProperties : Structure<VkPhysicalDeviceIDProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES> {
    std::span<const uint8_t> deviceUUID() const;
    std::span<const uint8_t> driverUUID() const;
};

struct PhysicalDevice : BorrowedHandle<VkPhysicalDevice> {
    void fillProperties(PhysicalDeviceProperties&) const;
    Vector<QueueFamilyProperties> queueFamilies() const;
};

struct DeviceCreateInfo : Structure<VkDeviceCreateInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO> {
    DeviceCreateInfo(const DeviceQueueCreateInfo&, std::span<const char* const> enabledExtensions = { });
};

struct Device : Handle<VkDevice> {
    [[nodiscard]] static Result<Device> create(PhysicalDevice&, const DeviceCreateInfo&);
    ~Device();

    Device(Device&& other) { swap(other); }
    Device& operator=(Device&& other) { swap(other); return *this; }

    void swap(Device& other)
    {
        Base::swap(other);
        std::swap(m_table, other.m_table);
    }

    static void setSharedDevice(Device&&);
    [[nodiscard]] static Device* sharedDeviceIfExists();
    [[nodiscard]] static Device& sharedDevice();

private:
    using Base::Base;

    explicit Device(VkDevice);

    VolkDeviceTable m_table;

    static Device s_sharedDevice;
};

struct Instance : Handle<VkInstance> {
    [[nodiscard]] static const Vector<VkLayerProperties>& availableLayers();
    [[nodiscard]] static bool hasLayers(std::span<const char* const> layerNames);
    [[nodiscard]] static bool hasLayer(const char* const layerName);

    [[nodiscard]] static Result<Vector<VkExtensionProperties>> availableExtensions(const char* layerName = nullptr);
    [[nodiscard]] static bool hasExtensions(const Vector<VkExtensionProperties>&, std::span<const char* const> extensionNames);
    [[nodiscard]] static bool hasExtension(const Vector<VkExtensionProperties>&, const char* const extensionName);
    [[nodiscard]] static bool hasExtensions(std::span<const char* const> extensionNames, const char* layerName = nullptr);
    [[nodiscard]] static bool hasExtension(const char* const extensionName, const char* layerName = nullptr);

    static void setSharedInstance(Instance&&);
    [[nodiscard]] static Instance* sharedInstanceIfExists();
    [[nodiscard]] static Instance& sharedInstance();

    [[nodiscard]] static Result<Instance> create(const InstanceCreateInfo&);
    ~Instance();

    Instance(Instance&& other) { swap(other); }
    Instance& operator=(Instance&& other) { swap(other); return *this; }

#ifdef VK_EXT_debug_utils
    void swap(Instance& other)
    {
        Base::swap(other);
        std::swap(m_debugMessenger, other.m_debugMessenger);
    }
#endif // VK_EXT_debug_utils

    [[nodiscard]] Result<Vector<PhysicalDevice>> availableDevices() const;
    [[nodiscard]] Result<PhysicalDevice> deviceForDisplay(PlatformDisplay&);
    [[nodiscard]] VkResult installDebugMessenger();

private:
    using Base::Base;

    explicit Instance(VkInstance);

    static Instance s_sharedInstance;

#ifdef VK_EXT_debug_utils
    VkDebugUtilsMessengerEXT m_debugMessenger { nullptr };
#endif
};

} // namespace Vulkan
} // namespace WebCore

#endif // USE(VULKAN)
