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

#include "Logging.h"
#include <WebCore/PlatformXR.h>
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

template<typename T, XrStructureType StructureType>
T createOpenXRStruct()
{
    T object;
    zeroBytes(object);
    object.type = StructureType;
    object.next = nullptr;
    return object;
}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)

// Macro to generate stringify functions for OpenXR enumerations based data provided in openxr_reflection.h
#define ENUM_CASE_STR(name, val) case name: return #name;
#define MAKE_TO_STRING_FUNC(enumType) \
    inline const char* toString(enumType e) { \
        switch (e) { \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR) \
            default: return "Unknown " #enumType; \
        } \
    }

MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);
MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_TO_STRING_FUNC(XrSessionState);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrFormFactor);

inline XrResult checkXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr)
{
    if (XR_FAILED(res))
        LOG(XR, "OpenXR error: %s (%s) at %s", toString(res), originator ? originator : "unknown", sourceLocation ? sourceLocation : "unknown location");

    return res;
}

#define CHECK_XRCMD(cmd) checkXrResult(cmd, #cmd, FILE_AND_LINE);

#define RETURN_RESULT_IF_FAILED(call, ...) \
{ \
    auto xrResult = call; \
    if (XR_FAILED(xrResult)) { \
        LOG(XR, "%s %s: %s\n", __func__, #call, toString(xrResult)); \
        return xrResult; \
    } \
}

inline PlatformXR::FrameData::Pose XrIdentityPose()
{
    return { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
}

inline PlatformXR::FrameData::Pose XrPosefToPose(XrPosef pose)
{
    return { { pose.position.x, pose.position.y, pose.position.z }, { pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w } };
}

inline PlatformXR::FrameData::View XrViewToView(XrView view)
{
    return { XrPosefToPose(view.pose), PlatformXR::FrameData::Fov { std::abs(view.fov.angleUp), std::abs(view.fov.angleDown), std::abs(view.fov.angleLeft), std::abs(view.fov.angleRight) } };
}

inline ASCIILiteral handednessToString(PlatformXR::XRHandedness handedness)
{
    switch (handedness) {
    case PlatformXR::XRHandedness::Left:
        return "left"_s;
    case PlatformXR::XRHandedness::Right:
        return "right"_s;
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

struct OpenXRSystemProperties {
    bool supportsOrientationTracking { false };
    bool supportsHandTracking { false };
#if ENABLE(WEBXR_LAYERS)
    unsigned maxLayerCount { 1 };
#endif
};

inline OpenXRSystemProperties systemProperties(XrInstance instance, XrSystemId systemId)
{
    XrSystemProperties xrSystemProperties = createOpenXRStruct<XrSystemProperties, XR_TYPE_SYSTEM_PROPERTIES>();

#if defined(XR_EXT_hand_tracking)
    XrSystemHandTrackingPropertiesEXT handTrackingProperties = createOpenXRStruct<XrSystemHandTrackingPropertiesEXT, XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT>();
    handTrackingProperties.supportsHandTracking = XR_FALSE;
    handTrackingProperties.next = xrSystemProperties.next;
    xrSystemProperties.next = &handTrackingProperties;
#endif

    CHECK_XRCMD(xrGetSystemProperties(instance, systemId, &xrSystemProperties));

    return {
        .supportsOrientationTracking = xrSystemProperties.trackingProperties.orientationTracking == XR_TRUE,
#if defined(XR_EXT_hand_tracking)
        .supportsHandTracking = handTrackingProperties.supportsHandTracking == XR_TRUE,
#else
        .supportsHandTracking = false,
#endif
#if ENABLE(WEBXR_LAYERS)
        .maxLayerCount = xrSystemProperties.graphicsProperties.maxLayerCount,
#endif
    };
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
