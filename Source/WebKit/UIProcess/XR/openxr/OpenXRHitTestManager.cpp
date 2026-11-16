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
#include "OpenXRHitTestManager.h"

#if ENABLE(WEBXR_HIT_TEST) && USE(OPENXR)

#include "OpenXRExtensions.h"
#include "OpenXRUtils.h"

namespace WebKit {

std::unique_ptr<OpenXRHitTestManager> OpenXRHitTestManager::create(XrInstance instance, XrSystemId systemId, XrSession session)
{
#if defined(XR_ANDROID_trackables) && defined(XR_ANDROID_raycast)
    if (!OpenXRExtensions::singleton().methods().xrCreateTrackableTrackerANDROID)
        return nullptr;
    if (!OpenXRExtensions::singleton().methods().xrRaycastANDROID)
        return nullptr;
    auto manager = makeUnique<OpenXRHitTestManager>(instance, systemId, session);
    if (!manager->m_trackableTrackers.size())
        return nullptr;
    return manager;
#else
    return nullptr;
#endif
}

OpenXRHitTestManager::OpenXRHitTestManager(XrInstance instance, XrSystemId systemId, XrSession session)
{
#if defined(XR_ANDROID_trackables) && defined(XR_ANDROID_raycast)
    uint32_t trackableTypeCapacity = 0;
    uint32_t trackableTypeCountOutput = 0;
    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrEnumerateRaycastSupportedTrackableTypesANDROID(instance, systemId, 0, &trackableTypeCapacity, nullptr));
    Vector<XrTrackableTypeANDROID> types(trackableTypeCapacity);
    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrEnumerateRaycastSupportedTrackableTypesANDROID(instance, systemId, trackableTypeCapacity, &trackableTypeCountOutput, types.mutableSpan().data()));
    auto createInfo = createOpenXRStruct<XrTrackableTrackerCreateInfoANDROID, XR_TYPE_TRACKABLE_TRACKER_CREATE_INFO_ANDROID>();
    for (auto type : types) {
        switch (type) {
        case XR_TRACKABLE_TYPE_PLANE_ANDROID:
        case XR_TRACKABLE_TYPE_DEPTH_ANDROID: {
            XrTrackableTrackerANDROID tracker = XR_NULL_HANDLE;
            createInfo.trackableType = type;
            CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrCreateTrackableTrackerANDROID(session, &createInfo, &tracker));
            m_trackableTrackers.append(tracker);
            break;
        }
        default:
            break;
        }
    }
#endif
}

OpenXRHitTestManager::~OpenXRHitTestManager()
{
#if defined(XR_ANDROID_trackables) && defined(XR_ANDROID_raycast)
    for (const auto& tracker : m_trackableTrackers)
        CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrDestroyTrackableTrackerANDROID(tracker));
#endif
}

Vector<PlatformXR::FrameData::HitTestResult> OpenXRHitTestManager::requestHitTest(XrSession session, const PlatformXR::Ray& ray, XrSpace space, XrTime time)
{
#if defined(XR_ANDROID_raycast)
    if (space == XR_NULL_HANDLE)
        return { };

    constexpr int maxHitTestResults = 2;
    auto raycastInfo = createOpenXRStruct<XrRaycastInfoANDROID, XR_TYPE_RAYCAST_INFO_ANDROID>();
    raycastInfo.maxResults = maxHitTestResults;
    raycastInfo.trackerCount = m_trackableTrackers.size();
    raycastInfo.trackers = m_trackableTrackers.span().data();
    raycastInfo.origin = XrVector3f { ray.origin.x(), ray.origin.y(), ray.origin.z() };
    raycastInfo.trajectory = XrVector3f { ray.direction.x(), ray.direction.y(), ray.direction.z() };
    raycastInfo.space = space;
    raycastInfo.time = time;

    auto xrHitResults = createOpenXRStruct<XrRaycastHitResultsANDROID, XR_TYPE_RAYCAST_HIT_RESULTS_ANDROID>();
    xrHitResults.resultsCapacityInput = 0;
    xrHitResults.resultsCountOutput = 0;
    xrHitResults.results = nullptr;

    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrRaycastANDROID(session, &raycastInfo, &xrHitResults));
    if (!xrHitResults.resultsCountOutput)
        return { };

    Vector<XrRaycastHitResultANDROID> xrResults;
    xrResults.resize(xrHitResults.resultsCountOutput);
    xrHitResults.resultsCapacityInput = xrHitResults.resultsCountOutput;
    xrHitResults.results = xrResults.mutableSpan().data();

    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrRaycastANDROID(session, &raycastInfo, &xrHitResults));

    return xrResults.map([](auto& result) -> PlatformXR::FrameData::HitTestResult {
        return { XrPosefToPose(result.pose) };
    });
#else
    return { };
#endif
}

} // namespace WebKit

#endif // ENABLE(WEBXR_HIT_TEST) && USE(OPENXR)
