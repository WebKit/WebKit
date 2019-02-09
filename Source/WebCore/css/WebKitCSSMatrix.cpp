/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitCSSMatrix.h"

#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "StyleProperties.h"
#include "TransformFunctions.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

inline WebKitCSSMatrix::WebKitCSSMatrix(const TransformationMatrix& matrix)
    : m_matrix(matrix)
{
}

Ref<WebKitCSSMatrix> WebKitCSSMatrix::create(const TransformationMatrix& matrix)
{
    return adoptRef(*new WebKitCSSMatrix(matrix));
}

ExceptionOr<Ref<WebKitCSSMatrix>> WebKitCSSMatrix::create(const String& string)
{
    auto result = adoptRef(*new WebKitCSSMatrix);
    auto setMatrixValueResult = result->setMatrixValue(string);
    if (setMatrixValueResult.hasException())
        return setMatrixValueResult.releaseException();
    return WTFMove(result);
}

WebKitCSSMatrix::~WebKitCSSMatrix() = default;

ExceptionOr<void> WebKitCSSMatrix::setMatrixValue(const String& string)
{
    if (string.isEmpty())
        return { };

    auto styleDeclaration = MutableStyleProperties::create();
    if (CSSParser::parseValue(styleDeclaration, CSSPropertyTransform, string, true, HTMLStandardMode) == CSSParser::ParseResult::Error)
        return Exception { SyntaxError };

    // Convert to TransformOperations. This can fail if a property requires style (i.e., param uses 'ems' or 'exs')
    auto value = styleDeclaration->getPropertyCSSValue(CSSPropertyTransform);

    // Check for a "none" or empty transform. In these cases we can use the default identity matrix.
    if (!value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).valueID() == CSSValueNone))
        return { };

    TransformOperations operations;
    if (!transformsForValue(*value, CSSToLengthConversionData(), operations))
        return Exception { SyntaxError };

    // Convert transform operations to a TransformationMatrix. This can fail if a parameter has a percentage ('%').
    TransformationMatrix matrix;
    for (auto& operation : operations.operations()) {
        if (operation->apply(matrix, IntSize(0, 0)))
            return Exception { SyntaxError };
    }
    m_matrix = matrix;
    return { };
}

// Perform a concatenation of the matrices (this * secondMatrix)
RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::multiply(WebKitCSSMatrix* secondMatrix) const
{
    if (!secondMatrix)
        return nullptr;

    auto matrix = create(m_matrix);
    matrix->m_matrix.multiply(secondMatrix->m_matrix);
    return WTFMove(matrix);
}

ExceptionOr<Ref<WebKitCSSMatrix>> WebKitCSSMatrix::inverse() const
{
    auto inverse = m_matrix.inverse();
    if (!inverse)
        return Exception { NotSupportedError };
    return create(inverse.value());
}

Ref<WebKitCSSMatrix> WebKitCSSMatrix::translate(double x, double y, double z) const
{
    if (std::isnan(x))
        x = 0;
    if (std::isnan(y))
        y = 0;
    if (std::isnan(z))
        z = 0;

    auto matrix = create(m_matrix);
    matrix->m_matrix.translate3d(x, y, z);
    return matrix;
}

Ref<WebKitCSSMatrix> WebKitCSSMatrix::scale(double scaleX, double scaleY, double scaleZ) const
{
    if (std::isnan(scaleX))
        scaleX = 1;
    if (std::isnan(scaleY))
        scaleY = scaleX;
    if (std::isnan(scaleZ))
        scaleZ = 1;

    auto matrix = create(m_matrix);
    matrix->m_matrix.scale3d(scaleX, scaleY, scaleZ);
    return matrix;
}

Ref<WebKitCSSMatrix> WebKitCSSMatrix::rotate(double rotX, double rotY, double rotZ) const
{
    if (std::isnan(rotX))
        rotX = 0;

    if (std::isnan(rotY) && std::isnan(rotZ)) {
        rotZ = rotX;
        rotX = 0;
        rotY = 0;
    }

    if (std::isnan(rotY))
        rotY = 0;
    if (std::isnan(rotZ))
        rotZ = 0;

    auto matrix = create(m_matrix);
    matrix->m_matrix.rotate3d(rotX, rotY, rotZ);
    return matrix;
}

Ref<WebKitCSSMatrix> WebKitCSSMatrix::rotateAxisAngle(double x, double y, double z, double angle) const
{
    if (std::isnan(x))
        x = 0;
    if (std::isnan(y))
        y = 0;
    if (std::isnan(z))
        z = 0;
    if (std::isnan(angle))
        angle = 0;
    if (x == 0 && y == 0 && z == 0)
        z = 1;

    auto matrix = create(m_matrix);
    matrix->m_matrix.rotate3d(x, y, z, angle);
    return matrix;
}

Ref<WebKitCSSMatrix> WebKitCSSMatrix::skewX(double angle) const
{
    if (std::isnan(angle))
        angle = 0;

    auto matrix = create(m_matrix);
    matrix->m_matrix.skewX(angle);
    return matrix;
}

Ref<WebKitCSSMatrix> WebKitCSSMatrix::skewY(double angle) const
{
    if (std::isnan(angle))
        angle = 0;

    auto matrix = create(m_matrix);
    matrix->m_matrix.skewY(angle);
    return matrix;
}

ExceptionOr<String> WebKitCSSMatrix::toString() const
{
    if (!m_matrix.containsOnlyFiniteValues())
        return Exception { InvalidStateError, "Matrix contains non-finite values"_s };

    StringBuilder builder;
    if (m_matrix.isAffine()) {
        builder.appendLiteral("matrix(");
        builder.appendECMAScriptNumber(m_matrix.a());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.b());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.c());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.d());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.e());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.f());
    } else {
        builder.appendLiteral("matrix3d(");
        builder.appendECMAScriptNumber(m_matrix.m11());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m12());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m13());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m14());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m21());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m22());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m23());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m24());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m31());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m32());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m33());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m34());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m41());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m42());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m43());
        builder.appendLiteral(", ");
        builder.appendECMAScriptNumber(m_matrix.m44());
    }
    builder.append(')');
    return builder.toString();
}

} // namespace WebCore
