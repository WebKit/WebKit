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
#include "WebXRRigidTransform.h"

#if ENABLE(WEBXR)

#include "DOMPointReadOnly.h"
#include "TransformationMatrix.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

static bool normalizeQuaternion(DOMPointInit& q)
{
    const double length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (WTF::areEssentiallyEqual<double>(length, 0))
        return false;
    q.x /= length;
    q.y /= length;
    q.z /= length;
    q.w /= length;
    return true;
}

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRRigidTransform);

Ref<WebXRRigidTransform> WebXRRigidTransform::create()
{
    return adoptRef(*new WebXRRigidTransform({ }, { }));
}

Ref<WebXRRigidTransform> WebXRRigidTransform::create(const TransformationMatrix& transform)
{
    return adoptRef(*new WebXRRigidTransform(transform));
}


ExceptionOr<Ref<WebXRRigidTransform>> WebXRRigidTransform::create(const DOMPointInit& position, const DOMPointInit& orientation)
{
    // The XRRigidTransform(position, orientation) constructor MUST perform the following steps when invoked:
    //   1. Let transform be a new XRRigidTransform.

    //   2. If position is not a DOMPointInit initialize transform’s position to { x: 0.0, y: 0.0, z: 0.0, w: 1.0 }.
    //   3. If position’s w value is not 1.0, throw a TypeError.
    //   4. Else initialize transform’s position’s x value to position’s x dictionary member, y value to position’s y dictionary member, z value to position’s z dictionary member and w to 1.0.
    if (position.w != 1.0)
        return Exception { ExceptionCode::TypeError };
    DOMPointInit positionInit { position.x, position.y, position.z, 1 };

    //   5. If orientation is not a DOMPointInit initialize transform’s orientation to { x: 0.0, y: 0.0, z: 0.0, w: 1.0 }.
    //   6. Else initialize transform’s orientation’s x value to orientation’s x dictionary member, y value to orientation’s y dictionary member, z value to orientation’s z dictionary member and w value to orientation’s w dictionary member.
    //   7. Normalize x, y, z, and w components of transform’s orientation.
    DOMPointInit orientationInit { orientation.x, orientation.y, orientation.z, orientation.w };
    if (!normalizeQuaternion(orientationInit))
        return Exception { ExceptionCode::InvalidStateError };

    //   8. Return transform.
    return adoptRef(*new WebXRRigidTransform(positionInit, orientationInit));
}

WebXRRigidTransform::WebXRRigidTransform(const DOMPointInit& position, const DOMPointInit& orientation)
    : m_position(DOMPointReadOnly::create(position))
    , m_orientation(DOMPointReadOnly::create(orientation))
{
    TransformationMatrix translation;
    translation.translate3d(position.x, position.y, position.z);
    auto rotation = TransformationMatrix::fromQuaternion({ orientation.x, orientation.y, orientation.z, orientation.w });
    m_rawTransform = translation * rotation;
}

WebXRRigidTransform::WebXRRigidTransform(const TransformationMatrix& transform)
    : m_position(DOMPointReadOnly::create({ }))
    , m_orientation(DOMPointReadOnly::create({ }))
    , m_rawTransform(transform)
{
    if (transform.isIdentity()) {
        // TransformationMatrix::decompose returns a empty quaternion instead of unit quaternion for Identity.
        // WebXR tests expect a unit quaternion for this case.
        return;
    }

    TransformationMatrix::Decomposed4Type decomp = { };
    if (!transform.decompose4(decomp))
        return;

    m_position = DOMPointReadOnly::create(decomp.translateX, decomp.translateY, decomp.translateZ, 1.0f);

    DOMPointInit orientationInit { decomp.quaternion.x, decomp.quaternion.y, decomp.quaternion.z, decomp.quaternion.w };
    normalizeQuaternion(orientationInit);
    m_orientation = DOMPointReadOnly::create(orientationInit);
}

WebXRRigidTransform::~WebXRRigidTransform() = default;

const DOMPointReadOnly& WebXRRigidTransform::position() const
{
    return m_position;
}

const DOMPointReadOnly& WebXRRigidTransform::orientation() const
{
    return m_orientation;
}

const Float32Array& WebXRRigidTransform::matrix()
{
    if (m_matrix && !m_matrix->isDetached())
        return *m_matrix;

    // Lazily create matrix Float32Array.
    auto matrixData = m_rawTransform.toColumnMajorFloatArray();
    m_matrix = Float32Array::create(matrixData.data(), matrixData.size());

    return *m_matrix;
}

const WebXRRigidTransform& WebXRRigidTransform::inverse()
{
    // The inverse of a inverse object should return the original object.
    if (m_parentInverse)
        return *m_parentInverse;

    // Inverse should always return the same object.
    if (m_inverse)
        return *m_inverse;
    
    auto inverseTransform = m_rawTransform.inverse();
    ASSERT(!!inverseTransform);

    m_inverse = WebXRRigidTransform::create(*inverseTransform);
    // The inverse of a inverse object should return the original object.
    m_inverse->m_parentInverse = *this;

    return *m_inverse;
}

const TransformationMatrix& WebXRRigidTransform::rawTransform() const
{
    return m_rawTransform;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
