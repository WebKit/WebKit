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
#include "VulkanUtilities.h"

#if USE(VULKAN)
#include "Logging.h"
#include "PlatformDisplay.h"
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/CStringView.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if USE(GLIB)
#include "ApplicationGLib.h"
#endif

#ifndef VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME
#define VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME "VK_KHR_external_memory_capabilities"
#endif

namespace WebCore {
namespace Vulkan {

const char* resultString(VkResult result)
{
    switch (result) {
    case VK_SUCCESS:
        return "SUCCESS";
    case VK_NOT_READY:
        return "NOT_READY";
    case VK_TIMEOUT:
        return "TIMEOUT";
    case VK_EVENT_SET:
        return "EVENT_SET";
    case VK_EVENT_RESET:
        return "EVENT_RESET";
    case VK_INCOMPLETE:
        return "INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_FRAGMENTATION:
        return "ERROR_FRAGMENTATION";
    case VK_PIPELINE_COMPILE_REQUIRED:
        return "PIPELINE_COMPILE_REQUIRED";
#ifdef VK_EXT_global_priority
    case VK_ERROR_NOT_PERMITTED_EXT:
        return "ERROR_NOT_PERMITTED_EXT";
#endif
#ifdef VK_KHR_surface
    case VK_ERROR_SURFACE_LOST_KHR:
        return "ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
#endif
#ifdef VK_KHR_swapchain
    case VK_SUBOPTIMAL_KHR:
        return "SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "ERROR_OUT_OF_DATE_KHR";
#endif
#ifdef VK_KHR_display_swapchain
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
#endif
#ifdef VK_NV_glsl_shader
    case VK_ERROR_INVALID_SHADER_NV:
        return "ERROR_INVALID_SHADER_NV";
#endif
#ifdef VK_KHR_video_queue
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
        return "ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
        return "ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
        return "ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
        return "ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
        return "ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
        return "ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
#endif
#ifdef VK_EXT_image_drm_format_modifier
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
#endif
#ifdef VK_EXT_full_screen_exclusive
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
#endif
#ifdef VK_KHR_deferred_host_operations
    case VK_THREAD_IDLE_KHR:
        return "THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:
        return "THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:
        return "OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return "OPERATION_NOT_DEFERRED_KHR";
#endif
#ifdef VK_KHR_video_encode_queue
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
        return "ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
#endif
#ifdef VK_EXT_image_compression_control
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
        return "ERROR_COMPRESSION_EXHAUSTED_EXT";
#endif
#ifdef VK_EXT_shader_object
    case VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT:
        return "ERROR_INCOMPATIBLE_SHADER_BINARY_EXT";
#endif
#ifdef VK_EXT_debug_report
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "ERROR_VALIDATION_FAILED_EXT";
#endif
    default:
        return "UNKNOWN";
    }
}

void initializeIfNeeded()
{
    if (Instance::sharedInstanceIfExists())
        return;

    if (auto result = volkInitialize(); result != VK_SUCCESS) {
        RELEASE_LOG(Vulkan, "Vulkan initialization failed: %s (runtime libraries missing?)", resultString(result));
        return;
    }

    if (!Instance::hasExtension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME)) {
        RELEASE_LOG_ERROR(Vulkan, "Required extension %s not present", VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
        return;
    }

    Vector<const char*> layerNames;
    Vector<const char*> extensionNames {
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    };

#ifdef VK_EXT_debug_utils
    bool shouldInstallDebugMessenger = false;
#endif // VK_EXT_debug_utils

    if (LOG_CHANNEL(Vulkan).state != WTFLogChannelState::Off) [[unlikely]] {
        static constexpr auto validationLayerName = "VK_LAYER_KHRONOS_validation";
        if (Instance::hasLayer(validationLayerName)) {
            layerNames.append(validationLayerName);

#ifdef VK_EXT_debug_utils
            if ((shouldInstallDebugMessenger = Instance::hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)))
                extensionNames.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            else
                RELEASE_LOG_ERROR(Vulkan, "VK_EXT_debug_utils not available, logging may be incomplete");
#endif // VK_EXT_debug_utils
        } else
            RELEASE_LOG_ERROR(Vulkan, "Cannot enable debug/logging support: validation layers not present");

        StringBuilder formattedLayerList;
        if (layerNames.size()) {
            formattedLayerList.append(":"_s);
            for (const auto* name : layerNames)
                formattedLayerList.append(" "_s, CStringView::unsafeFromUTF8(name));
        }
        RELEASE_LOG_DEBUG(Vulkan, "Requesting %zu layers%s", layerNames.size(), formattedLayerList.toString().utf8().data());

        StringBuilder formattedExtensionList;
        if (extensionNames.size()) {
            formattedExtensionList.append(":"_s);
            for (const auto* name : extensionNames)
                formattedExtensionList.append(" "_s, CStringView::unsafeFromUTF8(name));
        }
        RELEASE_LOG_DEBUG(Vulkan, "Requesting %zu extensions%s", extensionNames.size(), formattedExtensionList.toString().utf8().data());
    }

#if USE(GLIB)
    const String applicationName = makeString(getApplicationID(), '.', processTypeDescription(processType()));
#else
    const String applicationName = makeString("WebKit."_s, processTypeDescription(processType()));
#endif

    auto instance = Instance::create({ { applicationName }, layerNames.span(), extensionNames.span() });
    if (!instance) {
        RELEASE_LOG_ERROR(Vulkan, "Cannot create instance: %s (%d)", resultString(instance), instance.error());
        return;
    }

#ifdef VK_EXT_debug_utils
    if (shouldInstallDebugMessenger) {
        if (auto result = instance->installDebugMessenger(); result != VK_SUCCESS)
            RELEASE_LOG_ERROR(Vulkan, "Cannot create debug messenger, logging may be incomplete: %s (%d)", resultString(result), result);
    }
#endif // VK_EXT_debug_utils

    auto deviceInfo = instance->deviceForDisplay(PlatformDisplay::sharedDisplay());
    if (!deviceInfo) {
        RELEASE_LOG_ERROR(Vulkan, "Cannot find device for EGL display: %s (%d)", Vulkan::resultString(deviceInfo), deviceInfo.error());
        return;
    }

    PhysicalDeviceProperties properties;
    deviceInfo->fillProperties(properties);
    RELEASE_LOG(Vulkan, "Found device for EGL display: %s", properties->deviceName);

    const auto queueFamilies = deviceInfo->queueFamilies();
    auto queueIndex = queueFamilies.findIf([](const auto& queueProperties) {
        return queueProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    });
    if (queueIndex == notFound) {
        RELEASE_LOG_ERROR(Vulkan, "Cannot find graphics queue family");
        return;
    }

    RELEASE_ASSERT(queueIndex <= std::numeric_limits<uint32_t>::max());
    static constexpr float queuePriorities[] = { 0.0f };
    DeviceQueueCreateInfo queueCreateInfo { static_cast<uint32_t>(queueIndex), std::span(queuePriorities) };

    Vector<const char*, 2> deviceExtensions = {
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    };
    auto device = Device::create(*deviceInfo, { queueCreateInfo, deviceExtensions.span() });
    if (!device) {
        RELEASE_LOG_ERROR(Vulkan, "Cannot instantiate device: %s (%d)", Vulkan::resultString(device), device.error());
        return;
    }

    RELEASE_LOG(Vulkan, "Instantiated device %p", device->ptr());

    Instance::setSharedInstance(WTF::move(*instance));
    Device::setSharedDevice(WTF::move(*device));
}

} // namespace Vulkan
} // namespace WebCore

#endif // USE(VULKAN)
