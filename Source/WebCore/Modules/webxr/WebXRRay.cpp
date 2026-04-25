/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
 * Copyright (C) 2018 The Chromium Authors
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
#include "WebXRRay.h"

#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBXR_HIT_TEST)

#include "DOMPointReadOnly.h"
#include "WebXRRigidTransform.h"
#include "XRRayDirectionInit.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRRay);

// https://immersive-web.github.io/hit-test/#dom-xrray-xrray
ExceptionOr<Ref<WebXRRay>> WebXRRay::create(const DOMPointInit& origin, const XRRayDirectionInit& direction)
{
    if (!direction.x && !direction.y && !direction.z)
        return Exception { ExceptionCode::TypeError };
    if (direction.w)
        return Exception { ExceptionCode::TypeError };
    if (origin.w != 1)
        return Exception { ExceptionCode::TypeError };
    Ref<DOMPointReadOnly> dpOrigin = DOMPointReadOnly::fromPoint(origin);
    std::optional<Ref<DOMPointReadOnly>> dpDirection;
    double length = std::hypot(direction.x, direction.y, direction.z);
    if (length)
        dpDirection = DOMPointReadOnly::create(direction.x / length, direction.y / length, direction.z / length, 0);
    else
        dpDirection = DOMPointReadOnly::create(0, 0, -1, 0);
    return adoptRef(*new WebXRRay(WTF::move(dpOrigin), *WTF::move(dpDirection)));
}

Ref<WebXRRay> WebXRRay::create(WebXRRigidTransform& transform)
{
    FloatPoint3D origin = transform.rawTransform().mapPoint({ 0, 0, 0 });
    FloatPoint3D z = transform.rawTransform().mapPoint({ 0, 0, -1 });
    FloatPoint3D direction = z - origin;
    Ref dpDirection = DOMPointReadOnly::create(direction.x(), direction.y(), direction.z(), 0);
    return adoptRef(*new WebXRRay(DOMPointReadOnly::fromFloatPoint(origin), WTF::move(dpDirection)));
}

WebXRRay::WebXRRay(Ref<DOMPointReadOnly>&& origin, Ref<DOMPointReadOnly>&& direction)
    : m_origin(WTF::move(origin))
    , m_direction(WTF::move(direction))
{
}

WebXRRay::~WebXRRay() = default;

const DOMPointReadOnly& WebXRRay::origin()
{
    return m_origin;
}

const DOMPointReadOnly& WebXRRay::direction()
{
    return m_direction;
}

// https://immersive-web.github.io/hit-test/#dom-xrray-matrix
const Float32Array& WebXRRay::matrix()
{
    if (m_matrix && !m_matrix->isDetached())
        return *m_matrix;

    TransformationMatrix transform;
    transform.translate3d(m_origin->x(), m_origin->y(), m_origin->z());
    FloatPoint3D z { 0, 0, -1 };
    FloatPoint3D direction(m_direction->x(), m_direction->y(), m_direction->z());
    float cosAngle = z.dot(direction);
    if (cosAngle > 0.9999) {
        // Vectors are co-linear or almost co-linear & face the same direction,
        // no rotation is needed.
    } else if (cosAngle < -0.9999) {
        // Vectors are co-linear or almost co-linear & face the opposite
        // direction, rotation by 180 degrees is needed & can be around any vector
        // perpendicular to (0,0,-1) so let's rotate about the x-axis.
        transform.rotate3d(1, 0, 0, 180);
    } else {
        // Rotation needed - create it from axis-angle.
        FloatPoint3D axis = z.cross(direction);
        transform.rotate3d(axis.x(), axis.y(), axis.z(), rad2deg(std::acos(cosAngle)));
    }

    auto matrixData = transform.toColumnMajorFloatArray();
    m_matrix = Float32Array::create(matrixData.data(), matrixData.size());
    return *m_matrix;
}

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
