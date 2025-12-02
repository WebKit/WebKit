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

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRExtensions.h"
#include "OpenXRUtils.h"

namespace WebKit {

OpenXRHitTestManager::OpenXRHitTestManager(XrSession session)
    : m_session(session)
{
#if defined(XR_ANDROID_raycast)
    auto createInfo = createOpenXRStruct<XrTrackableTrackerCreateInfoANDROID, XR_TYPE_TRACKABLE_TRACKER_CREATE_INFO_ANDROID>();
    createInfo.trackableType = XR_TRACKABLE_TYPE_PLANE_ANDROID;
    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrCreateTrackableTrackerANDROID(session, &createInfo, &m_trackableTracker));
#endif
}

Vector<PlatformXR::FrameData::HitTestResult> OpenXRHitTestManager::requestHitTest(const PlatformXR::Ray& ray, XrSpace space, XrTime time)
{
#if defined(XR_ANDROID_raycast)
    if (m_trackableTracker == XR_NULL_HANDLE)
        return { };

    constexpr int maxHitTestResults = 2;
    auto raycastInfo = createOpenXRStruct<XrRaycastInfoANDROID, XR_TYPE_RAYCAST_INFO_ANDROID>();
    raycastInfo.maxResults = maxHitTestResults;
    raycastInfo.trackerCount = 1;
    raycastInfo.trackers = &m_trackableTracker;
    raycastInfo.origin = XrVector3f { ray.origin.x(), ray.origin.y(), ray.origin.z() };
    raycastInfo.trajectory = XrVector3f { ray.direction.x(), ray.direction.y(), ray.direction.z() };
    raycastInfo.space = space;
    raycastInfo.time = time;

    XrRaycastHitResultANDROID xrResults[maxHitTestResults];
    auto xrHitResults = createOpenXRStruct<XrRaycastHitResultsANDROID, XR_TYPE_RAYCAST_HIT_RESULTS_ANDROID>();
    xrHitResults.resultsCapacityInput = maxHitTestResults;
    xrHitResults.results = xrResults;

    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrRaycastANDROID(m_session, &raycastInfo, &xrHitResults));

    Vector<PlatformXR::FrameData::HitTestResult> results;
    for (const auto xrResult : xrResults)
        results.append(XrPosefToPose(xrResult.pose));
    return results;
#else
    return { };
#endif
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
