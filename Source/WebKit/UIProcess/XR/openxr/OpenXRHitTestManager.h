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

#include <WebCore/PlatformXR.h>
#include <openxr/openxr.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class OpenXRHitTestManager {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRHitTestManager);
    WTF_MAKE_NONCOPYABLE(OpenXRHitTestManager);
public:
    OpenXRHitTestManager(XrSession);
    Vector<PlatformXR::FrameData::HitTestResult> requestHitTest(const PlatformXR::Ray&, XrSpace, XrTime);

private:
    XrSession m_session { XR_NULL_HANDLE };
#if defined(XR_ANDROID_raycast)
    XrTrackableTrackerANDROID m_trackableTracker { XR_NULL_HANDLE };
#endif
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
