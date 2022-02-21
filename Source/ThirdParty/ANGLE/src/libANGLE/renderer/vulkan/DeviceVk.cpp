//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeviceVk.cpp:
//    Implements the class methods for DeviceVk.
//

#include "libANGLE/renderer/vulkan/DeviceVk.h"

#include <stdint.h>

#include "common/debug.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

DeviceVk::DeviceVk() = default;

DeviceVk::~DeviceVk() = default;

egl::Error DeviceVk::initialize()
{
    return egl::NoError();
}

egl::Error DeviceVk::getAttribute(const egl::Display *display, EGLint attribute, void **outValue)
{
    RendererVk *renderer =
        static_cast<rx::DisplayVk *>(display->getImplementation())->getRenderer();
    switch (attribute)
    {
        case EGL_VULKAN_VERSION_ANGLE:
        {
            auto version = static_cast<intptr_t>(renderer->getApiVersion());
            *outValue    = reinterpret_cast<void *>(version);
            return egl::NoError();
        }
        case EGL_VULKAN_INSTANCE_ANGLE:
        {
            *outValue = renderer->getInstance();
            return egl::NoError();
        }
        case EGL_VULKAN_DEVICE_ANGLE:
        {
            *outValue = renderer->getDevice();
            return egl::NoError();
        }
        case EGL_VULKAN_PHYSICAL_DEVICE_ANGLE:
        {
            *outValue = renderer->getPhysicalDevice();
            return egl::NoError();
        }
        case EGL_VULKAN_QUEUE_ANGLE:
        {
            // egl::ContextPriority::Medium is the default context priority.
            *outValue = renderer->getQueue(egl::ContextPriority::Medium);
            return egl::NoError();
        }
        case EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE:
        {
            intptr_t index = static_cast<intptr_t>(renderer->getQueueFamilyIndex());
            *outValue      = reinterpret_cast<void *>(index);
            return egl::NoError();
        }
        case EGL_VULKAN_DEVICE_EXTENSIONS_ANGLE:
        {
            char **extensions = const_cast<char **>(renderer->getEnabledDeviceExtensions().data());
            *outValue         = reinterpret_cast<void *>(extensions);
            return egl::NoError();
        }
        case EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE:
        {
            char **extensions =
                const_cast<char **>(renderer->getEnabledInstanceExtensions().data());
            *outValue = reinterpret_cast<void *>(extensions);
            return egl::NoError();
        }
        case EGL_VULKAN_FEATURES_ANGLE:
        {
            const auto *features = &renderer->getEnabledFeatures();
            *outValue            = const_cast<void *>(reinterpret_cast<const void *>(features));
            return egl::NoError();
        }
        case EGL_VULKAN_GET_INSTANCE_PROC_ADDR:
        {
            *outValue = reinterpret_cast<void *>(vkGetInstanceProcAddr);
            return egl::NoError();
        }
        default:
            return egl::EglBadAccess();
    }
}

EGLint DeviceVk::getType()
{
    return EGL_VULKAN_DEVICE_ANGLE;
}

void DeviceVk::generateExtensions(egl::DeviceExtensions *outExtensions) const
{
    outExtensions->deviceVulkan = true;
}

}  // namespace rx
