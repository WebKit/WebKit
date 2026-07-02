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
#if defined(XR_ANDROID_trackables)
    PFN_xrCreateTrackableTrackerANDROID xrCreateTrackableTrackerANDROID { nullptr };
    PFN_xrDestroyTrackableTrackerANDROID xrDestroyTrackableTrackerANDROID { nullptr };
#endif
#if defined(XR_ANDROID_raycast)
    PFN_xrRaycastANDROID xrRaycastANDROID { nullptr };
    PFN_xrEnumerateRaycastSupportedTrackableTypesANDROID xrEnumerateRaycastSupportedTrackableTypesANDROID { nullptr };
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
