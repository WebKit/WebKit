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

#include "config.h"
#include "WebXRPlane.h"

#if ENABLE(WEBXR_PLANE_DETECTION)

#include "DOMPointReadOnly.h"
#include "WebXRPlaneSpace.h"
#include "WebXRSession.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRPlane);

static std::optional<XRPlaneOrientation> toXRPlaneOrientation(std::optional<PlatformXR::PlaneOrientation> orientation)
{
    if (!orientation)
        return std::nullopt;
    switch (*orientation) {
    case PlatformXR::PlaneOrientation::Horizontal:
        return XRPlaneOrientation::Horizontal;
    case PlatformXR::PlaneOrientation::Vertical:
        return XRPlaneOrientation::Vertical;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static Vector<Ref<DOMPointReadOnly>> toDOMPolygon(const Vector<FloatPoint>& polygon)
{
    return polygon.map([](const FloatPoint& point) {
        return DOMPointReadOnly::create(point.x(), 0, point.y(), 1);
    });
}

Ref<WebXRPlane> WebXRPlane::create(Document& document, WebXRSession& session, const PlatformXR::FrameData::Plane& plane, DOMHighResTimeStamp time)
{
    return adoptRef(*new WebXRPlane(document, session, plane, time));
}

WebXRPlane::WebXRPlane(Document& document, WebXRSession& session, const PlatformXR::FrameData::Plane& plane, DOMHighResTimeStamp time)
    : m_id(plane.id)
    , m_session(session)
    , m_planeSpace(WebXRPlaneSpace::create(document, session, plane.pose))
    , m_nativePolygon(plane.polygon)
    , m_polygon(toDOMPolygon(plane.polygon))
    , m_orientation(toXRPlaneOrientation(plane.orientation))
    , m_semanticLabel(plane.semanticLabel)
    , m_lastChangedTime(time)
{
}

WebXRPlane::~WebXRPlane() = default;

WebXRSession* WebXRPlane::session()
{
    return m_session.get();
}

WebXRSpace& WebXRPlane::planeSpace()
{
    return m_planeSpace;
}

String WebXRPlane::semanticLabel() const
{
    if (!m_semanticLabel)
        return String();
    switch (*m_semanticLabel) {
    case PlatformXR::PlaneSemanticLabel::Floor:
        return "floor"_s;
    case PlatformXR::PlaneSemanticLabel::Wall:
        return "wall"_s;
    case PlatformXR::PlaneSemanticLabel::Ceiling:
        return "ceiling"_s;
    case PlatformXR::PlaneSemanticLabel::Table:
        return "table"_s;
    }
    ASSERT_NOT_REACHED();
    return String();
}

// https://immersive-web.github.io/plane-detection/#update-plane-object
void WebXRPlane::update(const PlatformXR::FrameData::Plane& plane, DOMHighResTimeStamp time)
{
    m_planeSpace->setPose(plane.pose);
    m_planeSpace->setTracked(true);

    auto orientation = toXRPlaneOrientation(plane.orientation);
    if (m_nativePolygon == plane.polygon && m_orientation == orientation && m_semanticLabel == plane.semanticLabel)
        return;

    m_nativePolygon = plane.polygon;
    m_polygon = toDOMPolygon(plane.polygon);
    m_orientation = orientation;
    m_semanticLabel = plane.semanticLabel;
    m_lastChangedTime = time;
}

void WebXRPlane::invalidate()
{
    m_planeSpace->setTracked(false);
}

} // namespace WebCore

#endif // ENABLE(WEBXR_PLANE_DETECTION)
