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
    : m_session(session)
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
#else
    UNUSED_VARIABLE(m_session);
#endif
}

OpenXRHitTestManager::~OpenXRHitTestManager()
{
#if defined(XR_ANDROID_trackables) && defined(XR_ANDROID_raycast)
    for (const auto& tracker : m_trackableTrackers)
        CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrDestroyTrackableTrackerANDROID(tracker));
#endif
}

Vector<PlatformXR::FrameData::HitTestResult> OpenXRHitTestManager::requestHitTest(const PlatformXR::Ray& ray, XrSpace space, XrTime time)
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

    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrRaycastANDROID(m_session, &raycastInfo, &xrHitResults));
    if (!xrHitResults.resultsCountOutput)
        return { };

    Vector<XrRaycastHitResultANDROID> xrResults;
    xrResults.resize(xrHitResults.resultsCountOutput);
    xrHitResults.resultsCapacityInput = xrHitResults.resultsCountOutput;
    xrHitResults.results = xrResults.mutableSpan().data();

    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrRaycastANDROID(m_session, &raycastInfo, &xrHitResults));

    return xrResults.map([](auto& result) -> PlatformXR::FrameData::HitTestResult {
        return { XrPosefToPose(result.pose) };
    });
#else
    return { };
#endif
}

} // namespace WebKit

#endif // ENABLE(WEBXR_HIT_TEST) && USE(OPENXR)
