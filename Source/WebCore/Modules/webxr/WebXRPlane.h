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

#if ENABLE(WEBXR_PLANE_DETECTION)

#include "DOMHighResTimeStamp.h"
#include "FloatPoint.h"
#include "PlatformXR.h"
#include "XRPlaneOrientation.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class DOMPointReadOnly;
class WebXRPlaneSpace;
class WebXRSession;
class WebXRSpace;

class WebXRPlane : public RefCounted<WebXRPlane> {
    WTF_MAKE_TZONE_ALLOCATED(WebXRPlane);
public:
    static Ref<WebXRPlane> create(Document&, WebXRSession&, const PlatformXR::FrameData::Plane&, DOMHighResTimeStamp);
    ~WebXRPlane();

    uint64_t id() const { return m_id; }
    WebXRSession* session();

    WebXRSpace& planeSpace();
    const Vector<Ref<DOMPointReadOnly>>& polygon() const LIFETIME_BOUND { return m_polygon; }
    std::optional<XRPlaneOrientation> orientation() const { return m_orientation; }
    DOMHighResTimeStamp lastChangedTime() const { return m_lastChangedTime; }
    String semanticLabel() const;

    void update(const PlatformXR::FrameData::Plane&, DOMHighResTimeStamp);
    void invalidate();

private:
    WebXRPlane(Document&, WebXRSession&, const PlatformXR::FrameData::Plane&, DOMHighResTimeStamp);

    uint64_t m_id { 0 };
    WeakPtr<WebXRSession> m_session;
    const Ref<WebXRPlaneSpace> m_planeSpace;
    Vector<FloatPoint> m_nativePolygon;
    Vector<Ref<DOMPointReadOnly>> m_polygon;
    std::optional<XRPlaneOrientation> m_orientation;
    std::optional<PlatformXR::PlaneSemanticLabel> m_semanticLabel;
    DOMHighResTimeStamp m_lastChangedTime { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEBXR_PLANE_DETECTION)
