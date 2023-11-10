/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "WebXRBoundedReferenceSpace.h"

#if ENABLE(WEBXR)

#include "DOMPointReadOnly.h"
#include "Document.h"
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

// https://immersive-web.github.io/webxr/#xrboundedreferencespace-native-bounds-geometry
// It is suggested that points of the native bounds geometry be quantized to the nearest 5cm.
static constexpr float BoundsPrecisionInMeters = 0.05; 
// A valid polygon has at least 3 vertices.
static constexpr int MinimumBoundsVertices = 3; 

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRBoundedReferenceSpace);

Ref<WebXRBoundedReferenceSpace> WebXRBoundedReferenceSpace::create(Document& document, WebXRSession& session, XRReferenceSpaceType type)
{
    return adoptRef(*new WebXRBoundedReferenceSpace(document, session, WebXRRigidTransform::create(), type));
}

Ref<WebXRBoundedReferenceSpace> WebXRBoundedReferenceSpace::create(Document& document, WebXRSession& session, Ref<WebXRRigidTransform>&& offset, XRReferenceSpaceType type)
{
    return adoptRef(*new WebXRBoundedReferenceSpace(document, session, WTFMove(offset), type));
}

WebXRBoundedReferenceSpace::WebXRBoundedReferenceSpace(Document& document, WebXRSession& session, Ref<WebXRRigidTransform>&& offset, XRReferenceSpaceType type)
    : WebXRReferenceSpace(document, session, WTFMove(offset), type)
{
}

WebXRBoundedReferenceSpace::~WebXRBoundedReferenceSpace() = default;

std::optional<TransformationMatrix> WebXRBoundedReferenceSpace::nativeOrigin() const
{
    // https://immersive-web.github.io/webxr/#dom-xrreferencespacetype-bounded-floor.
    // Bounded floor space should be at the same height as local floor space.
    return floorOriginTransform();
}

const Vector<Ref<DOMPointReadOnly>>& WebXRBoundedReferenceSpace::boundsGeometry()
{
    updateIfNeeded();
    return m_boundsGeometry;
}

ExceptionOr<Ref<WebXRReferenceSpace>> WebXRBoundedReferenceSpace::getOffsetReferenceSpace(const WebXRRigidTransform& offsetTransform)
{
    if (!m_session)
        return Exception { ExceptionCode::InvalidStateError };

    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return Exception { ExceptionCode::InvalidStateError };

    // https://immersive-web.github.io/webxr/#dom-xrreferencespace-getoffsetreferencespace
    // Set offsetSpace’s origin offset to the result of multiplying base’s origin offset by originOffset in the relevant realm of base.
    auto offset = WebXRRigidTransform::create(originOffset().rawTransform() * offsetTransform.rawTransform());

    return { create(*document, *m_session.get(), WTFMove(offset), m_type) };
}

// https://immersive-web.github.io/webxr/#dom-xrboundedreferencespace-boundsgeometry
void WebXRBoundedReferenceSpace::updateIfNeeded()
{
    if (!m_session)
        return;

    auto& frameData = m_session->frameData();
    if (frameData.stageParameters.id == m_lastUpdateId)
        return;
    m_lastUpdateId = frameData.stageParameters.id;

    m_boundsGeometry.clear();

    if (frameData.stageParameters.bounds.size() >= MinimumBoundsVertices) {
        // Each point has to multiplied by the inverse of originOffset.
        auto transform = valueOrDefault(originOffset().rawTransform().inverse());
        for (auto& point : frameData.stageParameters.bounds) {
            auto mappedPoint = transform.mapPoint(FloatPoint3D(point.x(), 0.0, point.y()));
            m_boundsGeometry.append(DOMPointReadOnly::create(quantize(mappedPoint.x()), quantize(mappedPoint.y()), quantize(mappedPoint.z()), 1.0));
        }
    }
}

// https://immersive-web.github.io/webxr/#quantization
float WebXRBoundedReferenceSpace::quantize(float value)
{
    // Each point in the native bounds geometry MUST also be quantized sufficiently to prevent fingerprinting.
    // For user’s safety, quantized points values MUST NOT fall outside the bounds reported by the platform.
    return std::floor(value / BoundsPrecisionInMeters) * BoundsPrecisionInMeters;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
