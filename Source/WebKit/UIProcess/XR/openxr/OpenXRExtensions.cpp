/*
 * Copyright (C) 2025 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "OpenXRExtensions.h"

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRUtils.h"
#if USE(LIBEPOXY)
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRExtensions);

OpenXRExtensions& OpenXRExtensions::singleton()
{
    static NeverDestroyed<OpenXRExtensions> sharedExtensions;
    return sharedExtensions;
}

OpenXRExtensions::OpenXRExtensions()
    : m_methods(WTF::makeUnique<OpenXRExtensionMethods>())
{
    uint32_t extensionCount;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);

    if (XR_FAILED(result) || !extensionCount) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties(): no extensions\n");
        return;
    }

    m_extensions.fill(createOpenXRStruct<XrExtensionProperties, XR_TYPE_EXTENSION_PROPERTIES>(), extensionCount);

    result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, m_extensions.mutableSpan().data());
    if (XR_FAILED(result)) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties() failed: %d\n", result);
        return;
    }
}

// Destructor must be explicitly defined here because at this point OpenXRExtensionMethods is already defined.
// If we don't do this, the compiler will try to generate the default destructor for this class the first time
// it finds it which might be too early, in the sense that the struct is not defined yet and thus it will fail.
OpenXRExtensions::~OpenXRExtensions() = default;

bool OpenXRExtensions::loadMethods(XrInstance instance)
{
#if defined(XR_USE_PLATFORM_EGL)
    m_methods->getProcAddressFunc = eglGetProcAddress;
    if (!m_methods->getProcAddressFunc) {
        LOG(XR, "Failed to load eglGetProcAddress");
        return false;
    }
#endif
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrGetOpenGLESGraphicsRequirementsKHR));
    if (!m_methods->xrGetOpenGLESGraphicsRequirementsKHR) {
        LOG(XR, "Failed to load xrGetOpenGLESGraphicsRequirementsKHR");
        return false;
    }
#endif
#if defined(XR_EXT_hand_tracking)
    if (isExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME ""_span)) {
        xrGetInstanceProcAddr(instance, "xrCreateHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrCreateHandTrackerEXT));
        xrGetInstanceProcAddr(instance, "xrDestroyHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrDestroyHandTrackerEXT));
        xrGetInstanceProcAddr(instance, "xrLocateHandJointsEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrLocateHandJointsEXT));
        RELEASE_ASSERT(m_methods->xrCreateHandTrackerEXT);
        RELEASE_ASSERT(m_methods->xrDestroyHandTrackerEXT);
        RELEASE_ASSERT(m_methods->xrLocateHandJointsEXT);
    }
#endif
#if defined(XR_ANDROID_raycast)
    if (isExtensionSupported(XR_ANDROID_RAYCAST_EXTENSION_NAME ""_span)) {
        xrGetInstanceProcAddr(instance, "xrRaycastANDROID", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrRaycastANDROID));
        xrGetInstanceProcAddr(instance, "xrCreateTrackableTrackerANDROID", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrCreateTrackableTrackerANDROID));
        RELEASE_ASSERT(m_methods->xrRaycastANDROID);
        RELEASE_ASSERT(m_methods->xrCreateTrackableTrackerANDROID);
    }
#endif
    return true;
}

bool OpenXRExtensions::isExtensionSupported(std::span<const char> name) const
{
    auto position = m_extensions.findIf([name](auto& property) {
        return equalSpans(unsafeSpan(property.extensionName), name);
    });
    return position != notFound;
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
