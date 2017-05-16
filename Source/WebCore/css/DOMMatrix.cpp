/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "DOMMatrix.h"

#include "ExceptionCode.h"
#include <cmath>
#include <limits>

namespace WebCore {

DOMMatrix::DOMMatrix(const TransformationMatrix& matrix, Is2D is2D)
    : DOMMatrixReadOnly(matrix, is2D)
{
}

// https://drafts.fxtf.org/geometry/#create-a-dommatrix-from-the-dictionary
ExceptionOr<Ref<DOMMatrix>> DOMMatrix::fromMatrix(DOMMatrixInit&& init)
{
    return fromMatrixHelper<DOMMatrix>(WTFMove(init));
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-multiplyself
ExceptionOr<Ref<DOMMatrix>> DOMMatrix::multiplySelf(DOMMatrixInit&& other)
{
    auto fromMatrixResult = DOMMatrix::fromMatrix(WTFMove(other));
    if (fromMatrixResult.hasException())
        return fromMatrixResult.releaseException();
    auto otherObject = fromMatrixResult.releaseReturnValue();
    m_matrix.multiply(otherObject->m_matrix);
    if (!otherObject->is2D())
        m_is2D = false;
    return Ref<DOMMatrix> { *this };
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-premultiplyself
ExceptionOr<Ref<DOMMatrix>> DOMMatrix::preMultiplySelf(DOMMatrixInit&& other)
{
    auto fromMatrixResult = DOMMatrix::fromMatrix(WTFMove(other));
    if (fromMatrixResult.hasException())
        return fromMatrixResult.releaseException();
    auto otherObject = fromMatrixResult.releaseReturnValue();
    m_matrix = otherObject->m_matrix * m_matrix;
    if (!otherObject->is2D())
        m_is2D = false;
    return Ref<DOMMatrix> { *this };
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-translateself
Ref<DOMMatrix> DOMMatrix::translateSelf(double tx, double ty, double tz)
{
    m_matrix.translate3d(tx, ty, tz);
    if (tz)
        m_is2D = false;
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-scaleself
Ref<DOMMatrix> DOMMatrix::scaleSelf(double scaleX, std::optional<double> scaleY, double scaleZ, double originX, double originY, double originZ)
{
    if (!scaleY)
        scaleY = scaleX;
    translateSelf(originX, originY, originZ);
    // Post-multiply a non-uniform scale transformation on the current matrix.
    // The 3D scale matrix is described in CSS Transforms with sx = scaleX, sy = scaleY and sz = scaleZ.
    m_matrix.scale3d(scaleX, scaleY.value(), scaleZ);
    translateSelf(-originX, -originY, -originZ);
    if (scaleZ != 1 || originZ)
        m_is2D = false;
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-scale3dself
Ref<DOMMatrix> DOMMatrix::scale3dSelf(double scale, double originX, double originY, double originZ)
{
    translateSelf(originX, originY, originZ);
    // Post-multiply a uniform 3D scale transformation (m11 = m22 = m33 = scale) on the current matrix.
    // The 3D scale matrix is described in CSS Transforms with sx = sy = sz = scale. [CSS3-TRANSFORMS]
    m_matrix.scale3d(scale, scale, scale);
    translateSelf(-originX, -originY, -originZ);
    if (scale != 1)
        m_is2D = false;
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-rotateself
Ref<DOMMatrix> DOMMatrix::rotateSelf(double rotX, std::optional<double> rotY, std::optional<double> rotZ)
{
    if (!rotY && !rotZ) {
        rotZ = rotX;
        rotX = 0;
        rotY = 0;
    }
    m_matrix.rotate3d(rotX, rotY.value_or(0), rotZ.value_or(0));
    if (rotX || rotY.value_or(0))
        m_is2D = false;
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-rotatefromvectorself
Ref<DOMMatrix> DOMMatrix::rotateFromVectorSelf(double x, double y)
{
    m_matrix.rotateFromVector(x, y);
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-rotateaxisangleself
Ref<DOMMatrix> DOMMatrix::rotateAxisAngleSelf(double x, double y, double z, double angle)
{
    m_matrix.rotate3d(x, y, z, angle);
    if (x || y)
        m_is2D = false;
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-skewxself
Ref<DOMMatrix> DOMMatrix::skewXSelf(double sx)
{
    m_matrix.skewX(sx);
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-skewyself
Ref<DOMMatrix> DOMMatrix::skewYSelf(double sy)
{
    m_matrix.skewY(sy);
    return *this;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-invertself
Ref<DOMMatrix> DOMMatrix::invertSelf()
{
    auto inverse = m_matrix.inverse();
    if (!inverse) {
        m_matrix.setMatrix(
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()
        );
        m_is2D = false;
    }
    m_matrix = inverse.value();
    return Ref<DOMMatrix> { *this };
}

ExceptionOr<Ref<DOMMatrix>> DOMMatrix::setMatrixValueForBindings(const String& string)
{
    auto result = setMatrixValue(string);
    if (result.hasException())
        return result.releaseException();
    return Ref<DOMMatrix> { *this };
}

} // namespace WebCore
