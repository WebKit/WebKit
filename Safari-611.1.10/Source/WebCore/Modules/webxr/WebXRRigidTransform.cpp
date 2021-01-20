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

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRRigidTransform);

Ref<WebXRRigidTransform> WebXRRigidTransform::create()
{
    return adoptRef(*new WebXRRigidTransform({ }, { }));
}

ExceptionOr<Ref<WebXRRigidTransform>> WebXRRigidTransform::create(const DOMPointInit& position, const DOMPointInit& orientation)
{
    // The XRRigidTransform(position, orientation) constructor MUST perform the following steps when invoked:
    //   1. Let transform be a new XRRigidTransform.

    //   2. If position is not a DOMPointInit initialize transform’s position to { x: 0.0, y: 0.0, z: 0.0, w: 1.0 }.
    //   3. If position’s w value is not 1.0, throw a TypeError.
    //   4. Else initialize transform’s position’s x value to position’s x dictionary member, y value to position’s y dictionary member, z value to position’s z dictionary member and w to 1.0.
    if (position.w != 1.0)
        return Exception { TypeError };
    DOMPointInit positionInit { position.x, position.y, position.z, 1 };

    //   5. If orientation is not a DOMPointInit initialize transform’s orientation to { x: 0.0, y: 0.0, z: 0.0, w: 1.0 }.
    //   6. Else initialize transform’s orientation’s x value to orientation’s x dictionary member, y value to orientation’s y dictionary member, z value to orientation’s z dictionary member and w value to orientation’s w dictionary member.
    //   7. Normalize x, y, z, and w components of transform’s orientation.
    DOMPointInit orientationInit { orientation.x, orientation.y, orientation.z, orientation.w };
    {
        double length = std::sqrt(orientationInit.x * orientationInit.x + orientationInit.y * orientationInit.y
            + orientationInit.z * orientationInit.z + orientationInit.w * orientationInit.w);
        if (WTF::areEssentiallyEqual<double>(length, 0))
            return Exception { InvalidStateError };

        orientationInit= {
            orientationInit.x / length, orientationInit.y / length,
            orientationInit.z / length, orientationInit.w / length,
        };
    }

    //   8. Return transform.
    return adoptRef(*new WebXRRigidTransform(positionInit, orientationInit));
}

WebXRRigidTransform::WebXRRigidTransform(const DOMPointInit& position, const DOMPointInit& orientation)
    : m_position(DOMPointReadOnly::create(position))
    , m_orientation(DOMPointReadOnly::create(orientation))
{
    // FIXME: implement properly, per spec.
    TransformationMatrix matrix;
    auto matrixData = matrix.toColumnMajorFloatArray();
    m_matrix = Float32Array::create(matrixData.data(), matrixData.size());
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

Ref<Float32Array>  WebXRRigidTransform::matrix() const
{
    auto matrix = Float32Array::create(16);
    matrix->zeroFill();
    return matrix;
}

Ref<WebXRRigidTransform> WebXRRigidTransform::inverse() const
{
    // FIXME: implement properly.
    return WebXRRigidTransform::create();
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
