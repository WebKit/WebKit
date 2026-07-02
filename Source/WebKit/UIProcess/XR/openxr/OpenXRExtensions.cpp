/*
 * Copyright (C) 2025-2026 Igalia, S.L.
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
#if defined(XR_ANDROID_trackables)
    if (isExtensionSupported(XR_ANDROID_TRACKABLES_EXTENSION_NAME ""_span)) {
        xrGetInstanceProcAddr(instance, "xrCreateTrackableTrackerANDROID", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrCreateTrackableTrackerANDROID));
        xrGetInstanceProcAddr(instance, "xrDestroyTrackableTrackerANDROID", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrDestroyTrackableTrackerANDROID));
        RELEASE_ASSERT(m_methods->xrCreateTrackableTrackerANDROID);
        RELEASE_ASSERT(m_methods->xrDestroyTrackableTrackerANDROID);
    }
#endif
#if defined(XR_ANDROID_raycast)
    if (isExtensionSupported(XR_ANDROID_RAYCAST_EXTENSION_NAME ""_span)) {
        xrGetInstanceProcAddr(instance, "xrRaycastANDROID", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrRaycastANDROID));
        xrGetInstanceProcAddr(instance, "xrEnumerateRaycastSupportedTrackableTypesANDROID", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrEnumerateRaycastSupportedTrackableTypesANDROID));
        RELEASE_ASSERT(m_methods->xrRaycastANDROID);
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
