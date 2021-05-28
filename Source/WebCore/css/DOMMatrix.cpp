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

#include "ScriptExecutionContext.h"
#include <cmath>
#include <limits>

namespace WebCore {

// https://drafts.fxtf.org/geometry/#dom-dommatrixreadonly-dommatrixreadonly
ExceptionOr<Ref<DOMMatrix>> DOMMatrix::create(ScriptExecutionContext& scriptExecutionContext, Optional<Variant<String, Vector<double>>>&& init)
{
    if (!init)
        return adoptRef(*new DOMMatrix);

    return WTF::switchOn(init.value(),
        [&scriptExecutionContext](const String& init) -> ExceptionOr<Ref<DOMMatrix>> {
            if (!scriptExecutionContext.isDocument())
                return Exception { TypeError };

            auto parseResult = parseStringIntoAbstractMatrix(init);
            if (parseResult.hasException())
                return parseResult.releaseException();
            
            return adoptRef(*new DOMMatrix(parseResult.returnValue().matrix, parseResult.returnValue().is2D ? Is2D::Yes : Is2D::No));
        },
        [](const Vector<double>& init) -> ExceptionOr<Ref<DOMMatrix>> {
            if (init.size() == 6) {
                return adoptRef(*new DOMMatrix(TransformationMatrix {
                    init[0], init[1], init[2], init[3], init[4], init[5]
                }, Is2D::Yes));
            }
            if (init.size() == 16) {
                return adoptRef(*new DOMMatrix(TransformationMatrix {
                    init[0], init[1], init[2], init[3],
                    init[4], init[5], init[6], init[7],
                    init[8], init[9], init[10], init[11],
                    init[12], init[13], init[14], init[15]
                }, Is2D::No));
            }
            return Exception { TypeError };
        }
    );
}

DOMMatrix::DOMMatrix(const TransformationMatrix& matrix, Is2D is2D)
    : DOMMatrixReadOnly(matrix, is2D)
{
}

DOMMatrix::DOMMatrix(TransformationMatrix&& matrix, Is2D is2D)
    : DOMMatrixReadOnly(WTFMove(matrix), is2D)
{
}

// https://drafts.fxtf.org/geometry/#create-a-dommatrix-from-the-dictionary
ExceptionOr<Ref<DOMMatrix>> DOMMatrix::fromMatrix(DOMMatrixInit&& init)
{
    return fromMatrixHelper<DOMMatrix>(WTFMove(init));
}

ExceptionOr<Ref<DOMMatrix>> DOMMatrix::fromFloat32Array(Ref<Float32Array>&& array32)
{
    if (array32->length() == 6)
        return DOMMatrix::create(TransformationMatrix(array32->item(0), array32->item(1), array32->item(2), array32->item(3), array32->item(4), array32->item(5)), Is2D::Yes);

    if (array32->length() == 16) {
        return DOMMatrix::create(TransformationMatrix(
            array32->item(0), array32->item(1), array32->item(2), array32->item(3),
            array32->item(4), array32->item(5), array32->item(6), array32->item(7),
            array32->item(8), array32->item(9), array32->item(10), array32->item(11),
            array32->item(12), array32->item(13), array32->item(14), array32->item(15)
        ), Is2D::No);
    }

    return Exception { TypeError };
}

ExceptionOr<Ref<DOMMatrix>> DOMMatrix::fromFloat64Array(Ref<Float64Array>&& array64)
{
    if (array64->length() == 6)
        return DOMMatrix::create(TransformationMatrix(array64->item(0), array64->item(1), array64->item(2), array64->item(3), array64->item(4), array64->item(5)), Is2D::Yes);

    if (array64->length() == 16) {
        return DOMMatrix::create(TransformationMatrix(
            array64->item(0), array64->item(1), array64->item(2), array64->item(3),
            array64->item(4), array64->item(5), array64->item(6), array64->item(7),
            array64->item(8), array64->item(9), array64->item(10), array64->item(11),
            array64->item(12), array64->item(13), array64->item(14), array64->item(15)
        ), Is2D::No);
    }

    return Exception { TypeError };
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
Ref<DOMMatrix> DOMMatrix::scaleSelf(double scaleX, Optional<double> scaleY, double scaleZ, double originX, double originY, double originZ)
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
Ref<DOMMatrix> DOMMatrix::rotateSelf(double rotX, Optional<double> rotY, Optional<double> rotZ)
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
    if (inverse)
        m_matrix = *inverse;
    else {
        m_matrix.setMatrix(
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()
        );
        m_is2D = false;
    }
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
