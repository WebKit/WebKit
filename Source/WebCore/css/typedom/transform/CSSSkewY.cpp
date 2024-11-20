/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSSkewY.h"

#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSPrimitiveNumericTypes+CSSOMConversion.h"
#include "CSSStyleValueFactory.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSSkewY);

ExceptionOr<Ref<CSSSkewY>> CSSSkewY::create(Ref<CSSNumericValue> ay)
{
    if (!ay->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };
    return adoptRef(*new CSSSkewY(WTFMove(ay)));
}

ExceptionOr<Ref<CSSSkewY>> CSSSkewY::create(CSS::SkewY skewY)
{
    auto ay = CSSNumericFactory::reifyNumeric(skewY.value);
    if (ay.hasException())
        return ay.releaseException();

    return adoptRef(*new CSSSkewY(ay.releaseReturnValue()));
}

CSSSkewY::CSSSkewY(Ref<CSSNumericValue> ay)
    : CSSTransformComponent(Is2D::Yes)
    , m_ay(WTFMove(ay))
{
}

ExceptionOr<void> CSSSkewY::setAy(Ref<CSSNumericValue> ay)
{
    if (!ay->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };

    m_ay = WTFMove(ay);
    return { };
}

void CSSSkewY::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssskewy
    builder.append("skewY("_s);
    m_ay->serialize(builder);
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSSkewY::toMatrix()
{
    RefPtr ay = dynamicDowncast<CSSUnitValue>(m_ay);
    if (!ay)
        return Exception { ExceptionCode::TypeError };

    auto y = ay->convertTo(CSSUnitType::CSS_DEG);
    if (!y)
        return Exception { ExceptionCode::TypeError };

    TransformationMatrix matrix { };
    matrix.skewY(y->value());

    return { DOMMatrix::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::Yes) };
}

std::optional<CSS::TransformFunction> CSSSkewY::toCSS() const
{
    auto ay = CSS::convertFromCSSOMValue<CSS::Angle<>>(m_ay);
    if (!ay)
        return { };

    return CSS::TransformFunction { CSS::SkewYFunction { { WTFMove(*ay) } } };
}

} // namespace WebCore
