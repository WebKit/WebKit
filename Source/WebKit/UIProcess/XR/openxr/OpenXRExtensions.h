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

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRUtils.h"

typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef unsigned EGLenum;
#if defined(XR_USE_PLATFORM_EGL)
typedef void (*(*PFNEGLGETPROCADDRESSPROC)(const char *))(void);
#endif

// The JNI types need to be defined before including openxr_platform.h
#if OS(ANDROID)
#include <jni.h>
#endif

#include <openxr/openxr_platform.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebKit {

struct OpenXRExtensionMethods {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(OpenXRExtensionMethods);
public:
#if defined(XR_USE_PLATFORM_EGL)
    PFNEGLGETPROCADDRESSPROC getProcAddressFunc { nullptr };
#endif
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR { nullptr };
#endif
#if defined(XR_EXT_hand_tracking)
    PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT { nullptr };
    PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT { nullptr };
    PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT { nullptr };
#endif
#if defined(XR_ANDROID_raycast)
    PFN_xrRaycastANDROID xrRaycastANDROID { nullptr };
    PFN_xrCreateTrackableTrackerANDROID xrCreateTrackableTrackerANDROID { nullptr };
#endif
};

class OpenXRExtensions final {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRExtensions);
    WTF_MAKE_NONCOPYABLE(OpenXRExtensions);
public:
    static OpenXRExtensions& singleton();

    ~OpenXRExtensions();

    bool loadMethods(XrInstance);
    bool isExtensionSupported(std::span<const char>) const;
    const OpenXRExtensionMethods& methods() const { return *m_methods; }

private:
    friend class NeverDestroyed<OpenXRExtensions>;
    OpenXRExtensions();
    Vector<XrExtensionProperties> m_extensions;
    std::unique_ptr<OpenXRExtensionMethods> m_methods;
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
