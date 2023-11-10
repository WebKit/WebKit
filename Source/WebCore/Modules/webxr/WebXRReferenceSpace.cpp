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
#include "WebXRReferenceSpace.h"

#if ENABLE(WEBXR)

#include "Document.h"
#include "WebXRFrame.h"
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

static constexpr double DefaultUserHeightInMeters = 1.65;

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRReferenceSpace);

Ref<WebXRReferenceSpace> WebXRReferenceSpace::create(Document& document, WebXRSession& session, XRReferenceSpaceType type)
{
    // https://immersive-web.github.io/webxr/#xrspace-native-origin
    // The transform from the effective space to the native origin's space is
    // defined by an origin offset, which is an XRRigidTransform initially set
    // to an identity transform.
    return adoptRef(*new WebXRReferenceSpace(document, session, WebXRRigidTransform::create(), type));
}

Ref<WebXRReferenceSpace> WebXRReferenceSpace::create(Document& document, WebXRSession& session, Ref<WebXRRigidTransform>&& offset, XRReferenceSpaceType type)
{
    return adoptRef(*new WebXRReferenceSpace(document, session, WTFMove(offset), type));
}

WebXRReferenceSpace::WebXRReferenceSpace(Document& document, WebXRSession& session, Ref<WebXRRigidTransform>&& offset, XRReferenceSpaceType type)
    : WebXRSpace(document, WTFMove(offset))
    , m_session(session)
    , m_type(type)
{
}

WebXRReferenceSpace::~WebXRReferenceSpace() = default;


std::optional<TransformationMatrix> WebXRReferenceSpace::nativeOrigin() const
{
    if (!m_session)
        return std::nullopt;

    TransformationMatrix identity;

    // We assume that poses got from the devices are in local space.
    // This will require more complex logic if we add ports with different default coordinates.
    switch (m_type) {
    case XRReferenceSpaceType::Viewer: {
        // Return the current pose. Content rendered in viewer pose will stay in fixed point on HMDs.
        auto& data = m_session->frameData();
        return WebXRFrame::matrixFromPose(data.origin);
    }
    case XRReferenceSpaceType::Local:
        // Data from the device is already in local, use the identity matrix.
        return identity;
    case XRReferenceSpaceType::Unbounded:
        // Local and unbounded use the same device space, use the identity matrix.
        return identity;
    case XRReferenceSpaceType::LocalFloor: {
        // Use the floor transform provided by the device or fallback to a default height.
        return floorOriginTransform();
    }
    case XRReferenceSpaceType::BoundedFloor:
    default:
        // BoundedFloor is handled by WebXRBoundedReferenceSpace subclass
        RELEASE_ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

ExceptionOr<Ref<WebXRReferenceSpace>> WebXRReferenceSpace::getOffsetReferenceSpace(const WebXRRigidTransform& offsetTransform)
{
    if (!m_session)
        return Exception { ExceptionCode::InvalidStateError };

    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return Exception { ExceptionCode::InvalidStateError };

    // https://immersive-web.github.io/webxr/#dom-xrreferencespace-getoffsetreferencespace
    // Set offsetSpace’s origin offset to the result of multiplying base’s origin offset by originOffset in the relevant realm of base.
    auto offset = WebXRRigidTransform::create(originOffset().rawTransform() * offsetTransform.rawTransform());

    return create(*document, *m_session.get(), WTFMove(offset), m_type);
}

std::optional<TransformationMatrix> WebXRReferenceSpace::floorOriginTransform() const
{
    if (!m_session)
        return std::nullopt;

    auto& data = m_session->frameData();
    if (!data.floorTransform) {
        TransformationMatrix defautTransform;
        defautTransform.translate3d(0.0, -DefaultUserHeightInMeters, 0.0);
        return defautTransform;
    }

    // https://immersive-web.github.io/webxr/#dom-xrreferencespacetype-local-floor
    // Get floor estimation from the device
    // FIXME: Round to nearest 1cm to prevent fingerprinting
    return WebXRFrame::matrixFromPose(*data.floorTransform);
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
