/*
 * Copyright (C) 2026 Igalia, S.L.
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(WEBXR_ANCHORS) && USE(OPENXR)

#include <WebCore/PlatformXR.h>
#include <openxr/openxr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebKit {

using CreateAnchorCallback = CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::AnchorIdentifier>)>;

class OpenXRAnchorManager {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRAnchorManager);
    WTF_MAKE_NONCOPYABLE(OpenXRAnchorManager);
public:
    static std::unique_ptr<OpenXRAnchorManager> create(XrInstance, XrSession, XrSpace localSpace);
    ~OpenXRAnchorManager();

    void createAnchor(const PlatformXR::FrameData::Pose&, CreateAnchorCallback&&);
    void deleteAnchor(PlatformXR::AnchorIdentifier);
    HashMap<PlatformXR::AnchorIdentifier, PlatformXR::AnchorState> computeTrackedAnchors(XrTime predictedDisplayTime);

private:
    OpenXRAnchorManager(XrInstance, XrSession, XrSpace localSpace);

    XrInstance m_instance { XR_NULL_HANDLE };
    XrSession m_session { XR_NULL_HANDLE };
    XrSpace m_localSpace { XR_NULL_HANDLE };

#if defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)
    bool ensureSpatialContext();
    void processPendingCreates(XrTime);

    XrFutureEXT m_spatialContextFuture { XR_NULL_FUTURE_EXT };
    XrSpatialContextEXT m_spatialContext { };
    HashMap<PlatformXR::AnchorIdentifier, std::pair<XrSpatialEntityIdEXT, XrSpatialEntityEXT>> m_anchors;
    HashMap<XrSpatialEntityIdEXT, PlatformXR::AnchorIdentifier> m_entityToIdentifier;
    Vector<std::pair<PlatformXR::FrameData::Pose, CreateAnchorCallback>> m_pendingCreates;
#endif
};

} // namespace WebKit

#endif // ENABLE(WEBXR_ANCHORS) && USE(OPENXR)
