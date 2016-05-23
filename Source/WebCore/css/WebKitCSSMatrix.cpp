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
#include "CSSPropertyNames.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "ExceptionCode.h"
#include "StyleProperties.h"
#include "TransformFunctions.h"
#include <wtf/MathExtras.h>

namespace WebCore {

WebKitCSSMatrix::WebKitCSSMatrix(const TransformationMatrix& m)
    : m_matrix(m)
{
}

WebKitCSSMatrix::WebKitCSSMatrix(const String& s, ExceptionCode& ec)
{
    setMatrixValue(s, ec);
}

WebKitCSSMatrix::~WebKitCSSMatrix()
{
}

void WebKitCSSMatrix::setMatrixValue(const String& string, ExceptionCode& ec)
{
    if (string.isEmpty())
        return;

    auto styleDeclaration = MutableStyleProperties::create();
    if (CSSParser::parseValue(styleDeclaration, CSSPropertyTransform, string, true, CSSStrictMode, nullptr) != CSSParser::ParseResult::Error) {
        // Convert to TransformOperations. This can fail if a property
        // requires style (i.e., param uses 'ems' or 'exs')
        RefPtr<CSSValue> value = styleDeclaration->getPropertyCSSValue(CSSPropertyTransform);

        // Check for a "none" or empty transform. In these cases we can use the default identity matrix.
        if (!value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).getValueID() == CSSValueNone))
            return;

        TransformOperations operations;
        if (!transformsForValue(*value, CSSToLengthConversionData(), operations)) {
            ec = SYNTAX_ERR;
            return;
        }

        // Convert transform operations to a TransformationMatrix. This can fail
        // if a param has a percentage ('%')
        TransformationMatrix t;
        for (unsigned i = 0; i < operations.operations().size(); ++i) {
            if (operations.operations()[i].get()->apply(t, IntSize(0, 0))) {
                ec = SYNTAX_ERR;
                return;
            }
        }

        // set the matrix
        m_matrix = t;
    } else // There is something there but parsing failed.
        ec = SYNTAX_ERR;
}

// Perform a concatenation of the matrices (this * secondMatrix)
RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::multiply(WebKitCSSMatrix* secondMatrix) const
{
    if (!secondMatrix)
        return nullptr;

    RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(m_matrix);
    matrix->m_matrix.multiply(secondMatrix->m_matrix);
    return matrix;
}

RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::inverse(ExceptionCode& ec) const
{
    if (auto inverse = m_matrix.inverse())
        return WebKitCSSMatrix::create(inverse.value());
    
    ec = NOT_SUPPORTED_ERR;
    return nullptr;
}

RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::translate(double x, double y, double z) const
{
    if (std::isnan(x))
        x = 0;
    if (std::isnan(y))
        y = 0;
    if (std::isnan(z))
        z = 0;

    RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(m_matrix);
    matrix->m_matrix.translate3d(x, y, z);
    return matrix;
}

RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::scale(double scaleX, double scaleY, double scaleZ) const
{
    if (std::isnan(scaleX))
        scaleX = 1;
    if (std::isnan(scaleY))
        scaleY = scaleX;
    if (std::isnan(scaleZ))
        scaleZ = 1;

    RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(m_matrix);
    matrix->m_matrix.scale3d(scaleX, scaleY, scaleZ);
    return matrix;
}

RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::rotate(double rotX, double rotY, double rotZ) const
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

    RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(m_matrix);
    matrix->m_matrix.rotate3d(rotX, rotY, rotZ);
    return matrix;
}

RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::rotateAxisAngle(double x, double y, double z, double angle) const
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

    RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(m_matrix);
    matrix->m_matrix.rotate3d(x, y, z, angle);
    return matrix;
}

RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::skewX(double angle) const
{
    if (std::isnan(angle))
        angle = 0;

    RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(m_matrix);
    matrix->m_matrix.skewX(angle);
    return matrix;
}

RefPtr<WebKitCSSMatrix> WebKitCSSMatrix::skewY(double angle) const
{
    if (std::isnan(angle))
        angle = 0;

    RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(m_matrix);
    matrix->m_matrix.skewY(angle);
    return matrix;
}

String WebKitCSSMatrix::toString() const
{
    // FIXME - Need to ensure valid CSS floating point values (https://bugs.webkit.org/show_bug.cgi?id=20674)
    if (m_matrix.isAffine())
        return String::format("matrix(%f, %f, %f, %f, %f, %f)",
                                m_matrix.a(), m_matrix.b(), m_matrix.c(), m_matrix.d(), m_matrix.e(), m_matrix.f());
    return String::format("matrix3d(%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f)",
                            m_matrix.m11(), m_matrix.m12(), m_matrix.m13(), m_matrix.m14(),
                            m_matrix.m21(), m_matrix.m22(), m_matrix.m23(), m_matrix.m24(),
                            m_matrix.m31(), m_matrix.m32(), m_matrix.m33(), m_matrix.m34(),
                            m_matrix.m41(), m_matrix.m42(), m_matrix.m43(), m_matrix.m44());
}

} // namespace WebCore
