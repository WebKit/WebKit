/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "DOMMatrixReadOnly.h"

#include "CSSParser.h"
#include "CSSToLengthConversionData.h"
#include "DOMMatrix.h"
#include "DOMPoint.h"
#include "ScriptExecutionContext.h"
#include "StyleProperties.h"
#include "TransformFunctions.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMMatrixReadOnly);

// https://drafts.fxtf.org/geometry/#dom-dommatrixreadonly-dommatrixreadonly
ExceptionOr<Ref<DOMMatrixReadOnly>> DOMMatrixReadOnly::create(ScriptExecutionContext& scriptExecutionContext, std::optional<std::variant<String, Vector<double>>>&& init)
{
    if (!init)
        return adoptRef(*new DOMMatrixReadOnly);

    return WTF::switchOn(init.value(),
        [&scriptExecutionContext](const String& init) -> ExceptionOr<Ref<DOMMatrixReadOnly>> {
            if (!scriptExecutionContext.isDocument())
                return Exception { TypeError };

            auto parseResult = parseStringIntoAbstractMatrix(init);
            if (parseResult.hasException())
                return parseResult.releaseException();
            
            return adoptRef(*new DOMMatrixReadOnly(parseResult.returnValue().matrix, parseResult.returnValue().is2D ? Is2D::Yes : Is2D::No));
        },
        [](const Vector<double>& init) -> ExceptionOr<Ref<DOMMatrixReadOnly>> {
            if (init.size() == 6) {
                return adoptRef(*new DOMMatrixReadOnly(TransformationMatrix {
                    init[0], init[1], init[2], init[3], init[4], init[5]
                }, Is2D::Yes));
            }
            if (init.size() == 16) {
                return adoptRef(*new DOMMatrixReadOnly(TransformationMatrix {
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

DOMMatrixReadOnly::DOMMatrixReadOnly(const TransformationMatrix& matrix, Is2D is2D)
    : m_matrix(matrix)
    , m_is2D(is2D == Is2D::Yes)
{
    ASSERT(!m_is2D || m_matrix.isAffine());
}

DOMMatrixReadOnly::DOMMatrixReadOnly(TransformationMatrix&& matrix, Is2D is2D)
    : m_matrix(WTFMove(matrix))
    , m_is2D(is2D == Is2D::Yes)
{
    ASSERT(!m_is2D || m_matrix.isAffine());
}

Ref<DOMMatrix> DOMMatrixReadOnly::cloneAsDOMMatrix() const
{
    return DOMMatrix::create(m_matrix, m_is2D ? Is2D::Yes : Is2D::No);
}

// https://tc39.github.io/ecma262/#sec-samevaluezero
static bool sameValueZero(double a, double b)
{
    if (std::isnan(a) && std::isnan(b))
        return true;
    return a == b;
}

// https://drafts.fxtf.org/geometry/#matrix-validate-and-fixup
ExceptionOr<void> DOMMatrixReadOnly::validateAndFixup(DOMMatrix2DInit& init)
{
    if (init.a && init.m11 && !sameValueZero(init.a.value(), init.m11.value()))
        return Exception { TypeError, "init.a and init.m11 do not match"_s };
    if (init.b && init.m12 && !sameValueZero(init.b.value(), init.m12.value()))
        return Exception { TypeError, "init.b and init.m12 do not match"_s };
    if (init.c && init.m21 && !sameValueZero(init.c.value(), init.m21.value()))
        return Exception { TypeError, "init.c and init.m21 do not match"_s };
    if (init.d && init.m22 && !sameValueZero(init.d.value(), init.m22.value()))
        return Exception { TypeError, "init.d and init.m22 do not match"_s };
    if (init.e && init.m41 && !sameValueZero(init.e.value(), init.m41.value()))
        return Exception { TypeError, "init.e and init.m41 do not match"_s };
    if (init.f && init.m42 && !sameValueZero(init.f.value(), init.m42.value()))
        return Exception { TypeError, "init.f and init.m42 do not match"_s };

    if (!init.m11)
        init.m11 = init.a.value_or(1);
    if (!init.m12)
        init.m12 = init.b.value_or(0);
    if (!init.m21)
        init.m21 = init.c.value_or(0);
    if (!init.m22)
        init.m22 = init.d.value_or(1);
    if (!init.m41)
        init.m41 = init.e.value_or(0);
    if (!init.m42)
        init.m42 = init.f.value_or(0);

    return { };
}

ExceptionOr<void> DOMMatrixReadOnly::validateAndFixup(DOMMatrixInit& init)
{
    auto validate2D = validateAndFixup(static_cast<DOMMatrix2DInit&>(init));
    if (validate2D.hasException())
        return validate2D.releaseException();

    if (init.is2D && init.is2D.value()) {
        if (init.m13)
            return Exception { TypeError, "m13 should be 0 for a 2D matrix"_s };
        if (init.m14)
            return Exception { TypeError, "m14 should be 0 for a 2D matrix"_s };
        if (init.m23)
            return Exception { TypeError, "m23 should be 0 for a 2D matrix"_s };
        if (init.m24)
            return Exception { TypeError, "m24 should be 0 for a 2D matrix"_s };
        if (init.m31)
            return Exception { TypeError, "m31 should be 0 for a 2D matrix"_s };
        if (init.m32)
            return Exception { TypeError, "m32 should be 0 for a 2D matrix"_s };
        if (init.m34)
            return Exception { TypeError, "m34 should be 0 for a 2D matrix"_s };
        if (init.m43)
            return Exception { TypeError, "m43 should be 0 for a 2D matrix"_s };
        if (init.m33 != 1)
            return Exception { TypeError, "m33 should be 1 for a 2D matrix"_s };
        if (init.m44 != 1)
            return Exception { TypeError, "m44 should be 1 for a 2D matrix"_s };
    }

    if (!init.is2D) {
        if (init.m13 || init.m14 || init.m23 || init.m24 || init.m31 || init.m32 || init.m34 || init.m43 || init.m33 != 1 || init.m44 != 1)
            init.is2D = false;
        else
            init.is2D = true;
    }
    return { };
}

ExceptionOr<Ref<DOMMatrixReadOnly>> DOMMatrixReadOnly::fromMatrix(DOMMatrixInit&& init)
{
    return fromMatrixHelper<DOMMatrixReadOnly>(WTFMove(init));
}

ExceptionOr<Ref<DOMMatrixReadOnly>> DOMMatrixReadOnly::fromFloat32Array(Ref<Float32Array>&& array32)
{
    if (array32->length() == 6)
        return DOMMatrixReadOnly::create(TransformationMatrix(array32->item(0), array32->item(1), array32->item(2), array32->item(3), array32->item(4), array32->item(5)), Is2D::Yes);

    if (array32->length() == 16) {
        return DOMMatrixReadOnly::create(TransformationMatrix(
            array32->item(0), array32->item(1), array32->item(2), array32->item(3),
            array32->item(4), array32->item(5), array32->item(6), array32->item(7),
            array32->item(8), array32->item(9), array32->item(10), array32->item(11),
            array32->item(12), array32->item(13), array32->item(14), array32->item(15)
        ), Is2D::No);
    }

    return Exception { TypeError };
}

ExceptionOr<Ref<DOMMatrixReadOnly>> DOMMatrixReadOnly::fromFloat64Array(Ref<Float64Array>&& array64)
{
    if (array64->length() == 6)
        return DOMMatrixReadOnly::create(TransformationMatrix(array64->item(0), array64->item(1), array64->item(2), array64->item(3), array64->item(4), array64->item(5)), Is2D::Yes);

    if (array64->length() == 16) {
        return DOMMatrixReadOnly::create(TransformationMatrix(
            array64->item(0), array64->item(1), array64->item(2), array64->item(3),
            array64->item(4), array64->item(5), array64->item(6), array64->item(7),
            array64->item(8), array64->item(9), array64->item(10), array64->item(11),
            array64->item(12), array64->item(13), array64->item(14), array64->item(15)
        ), Is2D::No);
    }

    return Exception { TypeError };
}

bool DOMMatrixReadOnly::isIdentity() const
{
    return m_matrix.isIdentity();
}

ExceptionOr<DOMMatrixReadOnly::AbstractMatrix> DOMMatrixReadOnly::parseStringIntoAbstractMatrix(const String& string)
{
    if (string.isEmpty())
        return AbstractMatrix { };

    auto styleDeclaration = MutableStyleProperties::create();
    if (CSSParser::parseValue(styleDeclaration, CSSPropertyTransform, string, true, HTMLStandardMode) == CSSParser::ParseResult::Error)
        return Exception { SyntaxError };

    // Convert to TransformOperations. This can fail if a property requires style (i.e., param uses 'ems' or 'exs')
    auto value = styleDeclaration->getPropertyCSSValue(CSSPropertyTransform);

    // Check for a "none" or empty transform. In these cases we can use the default identity matrix.
    if (!value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).valueID() == CSSValueNone))
        return AbstractMatrix { };

    auto operations = transformsForValue(*value, { });
    if (!operations)
        return Exception { SyntaxError };

    AbstractMatrix matrix;
    for (auto& operation : operations->operations()) {
        if (operation->apply(matrix.matrix, { 0, 0 }))
            return Exception { SyntaxError };
        if (operation->is3DOperation())
            matrix.is2D = false;
    }

    return matrix;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrix-setmatrixvalue
ExceptionOr<void> DOMMatrixReadOnly::setMatrixValue(const String& string)
{
    auto parseResult = parseStringIntoAbstractMatrix(string);
    if (parseResult.hasException())
        return parseResult.releaseException();

    m_is2D = parseResult.returnValue().is2D;
    m_matrix = parseResult.returnValue().matrix;
    return { };
}

Ref<DOMMatrix> DOMMatrixReadOnly::translate(double tx, double ty, double tz)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->translateSelf(tx, ty, tz);
}

// https://drafts.fxtf.org/geometry/#dom-dommatrixreadonly-flipx
Ref<DOMMatrix> DOMMatrixReadOnly::flipX()
{
    auto matrix = cloneAsDOMMatrix();
    matrix->m_matrix.flipX();
    return matrix;
}

// https://drafts.fxtf.org/geometry/#dom-dommatrixreadonly-flipy
Ref<DOMMatrix> DOMMatrixReadOnly::flipY()
{
    auto matrix = cloneAsDOMMatrix();
    matrix->m_matrix.flipY();
    return matrix;
}

ExceptionOr<Ref<DOMMatrix>> DOMMatrixReadOnly::multiply(DOMMatrixInit&& other) const
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->multiplySelf(WTFMove(other));
}

Ref<DOMMatrix> DOMMatrixReadOnly::scale(double scaleX, std::optional<double> scaleY, double scaleZ, double originX, double originY, double originZ)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->scaleSelf(scaleX, scaleY, scaleZ, originX, originY, originZ);
}

Ref<DOMMatrix> DOMMatrixReadOnly::scale3d(double scale, double originX, double originY, double originZ)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->scale3dSelf(scale, originX, originY, originZ);
}

Ref<DOMMatrix> DOMMatrixReadOnly::rotate(double rotX, std::optional<double> rotY, std::optional<double> rotZ)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->rotateSelf(rotX, rotY, rotZ);
}

Ref<DOMMatrix> DOMMatrixReadOnly::rotateFromVector(double x, double y)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->rotateFromVectorSelf(x, y);
}

Ref<DOMMatrix> DOMMatrixReadOnly::rotateAxisAngle(double x, double y, double z, double angle)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->rotateAxisAngleSelf(x, y, z, angle);
}

Ref<DOMMatrix> DOMMatrixReadOnly::skewX(double sx)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->skewXSelf(sx);
}

Ref<DOMMatrix> DOMMatrixReadOnly::skewY(double sy)
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->skewYSelf(sy);
}

Ref<DOMMatrix> DOMMatrixReadOnly::inverse() const
{
    auto matrix = cloneAsDOMMatrix();
    return matrix->invertSelf();
}

// https://drafts.fxtf.org/geometry/#dom-dommatrixreadonly-transformpoint
Ref<DOMPoint> DOMMatrixReadOnly::transformPoint(DOMPointInit&& pointInit)
{
    m_matrix.map4ComponentPoint(pointInit.x, pointInit.y, pointInit.z, pointInit.w);
    return DOMPoint::create(pointInit.x, pointInit.y, pointInit.z, pointInit.w);
}

ExceptionOr<Ref<Float32Array>> DOMMatrixReadOnly::toFloat32Array() const
{
    auto array32 = Float32Array::tryCreateUninitialized(16);
    if (!array32)
        return Exception { UnknownError, "Out of memory"_s };

    unsigned index = 0;
    array32->set(index++, m_matrix.m11());
    array32->set(index++, m_matrix.m12());
    array32->set(index++, m_matrix.m13());
    array32->set(index++, m_matrix.m14());
    array32->set(index++, m_matrix.m21());
    array32->set(index++, m_matrix.m22());
    array32->set(index++, m_matrix.m23());
    array32->set(index++, m_matrix.m24());
    array32->set(index++, m_matrix.m31());
    array32->set(index++, m_matrix.m32());
    array32->set(index++, m_matrix.m33());
    array32->set(index++, m_matrix.m34());
    array32->set(index++, m_matrix.m41());
    array32->set(index++, m_matrix.m42());
    array32->set(index++, m_matrix.m43());
    array32->set(index, m_matrix.m44());
    return array32.releaseNonNull();
}

ExceptionOr<Ref<Float64Array>> DOMMatrixReadOnly::toFloat64Array() const
{
    auto array64 = Float64Array::tryCreateUninitialized(16);
    if (!array64)
        return Exception { UnknownError, "Out of memory"_s };

    unsigned index = 0;
    array64->set(index++, m_matrix.m11());
    array64->set(index++, m_matrix.m12());
    array64->set(index++, m_matrix.m13());
    array64->set(index++, m_matrix.m14());
    array64->set(index++, m_matrix.m21());
    array64->set(index++, m_matrix.m22());
    array64->set(index++, m_matrix.m23());
    array64->set(index++, m_matrix.m24());
    array64->set(index++, m_matrix.m31());
    array64->set(index++, m_matrix.m32());
    array64->set(index++, m_matrix.m33());
    array64->set(index++, m_matrix.m34());
    array64->set(index++, m_matrix.m41());
    array64->set(index++, m_matrix.m42());
    array64->set(index++, m_matrix.m43());
    array64->set(index, m_matrix.m44());
    return array64.releaseNonNull();
}

// https://drafts.fxtf.org/geometry/#dom-dommatrixreadonly-stringifier
ExceptionOr<String> DOMMatrixReadOnly::toString() const
{
    if (!m_matrix.containsOnlyFiniteValues())
        return Exception { InvalidStateError, "Matrix contains non-finite values"_s };

    if (is2D())
        return makeString("matrix(", m_matrix.a(), ", ", m_matrix.b(), ", ", m_matrix.c(), ", ", m_matrix.d(), ", ", m_matrix.e(), ", ", m_matrix.f(), ')');

    return makeString("matrix3d(", m_matrix.m11(), ", ", m_matrix.m12(), ", ", m_matrix.m13(), ", ", m_matrix.m14(), ", ", m_matrix.m21(), ", ", m_matrix.m22(), ", ", m_matrix.m23(), ", ", m_matrix.m24(), ", ", m_matrix.m31(), ", ", m_matrix.m32(), ", ", m_matrix.m33(), ", ", m_matrix.m34(), ", ", m_matrix.m41(), ", ", m_matrix.m42(), ", ", m_matrix.m43(), ", ", m_matrix.m44(), ')');
}

} // namespace WebCore
