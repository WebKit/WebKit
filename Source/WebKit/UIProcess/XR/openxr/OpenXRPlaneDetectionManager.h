/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR_PLANE_DETECTION) && USE(OPENXR)

#include "OpenXRUtils.h"
#include <WebCore/PlatformXR.h>
#include <openxr/openxr.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebKit {

// Plane tracking through XR_EXT_spatial_entity and XR_EXT_spatial_plane_tracking.
class OpenXRPlaneDetectionManager {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRPlaneDetectionManager);
    WTF_MAKE_NONCOPYABLE(OpenXRPlaneDetectionManager);
public:
    static bool isSupported(XrInstance, XrSystemId);
    static std::unique_ptr<OpenXRPlaneDetectionManager> create(XrInstance, XrSystemId, XrSession);
    ~OpenXRPlaneDetectionManager();

    // Returns the planes of a newly completed discovery snapshot, with poses relative to baseSpace, or nullopt if nothing new landed since the last call.
    std::optional<Vector<PlatformXR::FrameData::Plane>> update(XrSpace baseSpace, XrTime);

#if defined(XR_EXT_spatial_plane_tracking)
    OpenXRPlaneDetectionManager(XrInstance, XrSession, Vector<XrSpatialComponentTypeEXT>&&);
#endif

private:
#if defined(XR_EXT_spatial_plane_tracking)
    bool isFutureReady(XrFutureEXT) const;
    void createSpatialContextAsync();
    void createSpatialDiscoverySnapshotAsync(XrSpace baseSpace, XrTime);
    void collectPlanes(XrSpatialSnapshotEXT);
    bool hasComponent(XrSpatialComponentTypeEXT) const;

    XrInstance m_instance { XR_NULL_HANDLE };
    XrSession m_session { XR_NULL_HANDLE };
    Vector<XrSpatialComponentTypeEXT> m_components;
    XrSpatialContextEXT m_context { XR_NULL_HANDLE };
    XrFutureEXT m_contextFuture { XR_NULL_FUTURE_EXT };
    XrFutureEXT m_discoveryFuture { XR_NULL_FUTURE_EXT };
    MonotonicTime m_lastDiscoveryTime;
    bool m_initializationFailed { false };
    bool m_hasNewPlanes { false };
#endif
    Vector<PlatformXR::FrameData::Plane> m_planes;
};

} // namespace WebKit

#endif // ENABLE(WEBXR_PLANE_DETECTION) && USE(OPENXR)
