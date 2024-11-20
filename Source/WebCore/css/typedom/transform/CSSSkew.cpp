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
#include "CSSSkew.h"

#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSPrimitiveNumericTypes+CSSOMConversion.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSSkew);

ExceptionOr<Ref<CSSSkew>> CSSSkew::create(Ref<CSSNumericValue> ax, Ref<CSSNumericValue> ay)
{
    if (!ax->type().matches<CSSNumericBaseType::Angle>()
        || !ay->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };
    return adoptRef(*new CSSSkew(WTFMove(ax), WTFMove(ay)));
}

ExceptionOr<Ref<CSSSkew>> CSSSkew::create(CSS::Skew skew)
{
    auto ax = CSSNumericFactory::reifyNumeric(skew.x);
    if (ax.hasException())
        return ax.releaseException();
    auto ay = skew.y ? CSSNumericFactory::reifyNumeric(*skew.y) : ExceptionOr<Ref<CSSNumericValue>> { CSSUnitValue::create(0.0, CSSUnitType::CSS_DEG) };
    if (ay.hasException())
        return ay.releaseException();

    return adoptRef(*new CSSSkew(ax.releaseReturnValue(), ay.releaseReturnValue()));
}

CSSSkew::CSSSkew(Ref<CSSNumericValue> ax, Ref<CSSNumericValue> ay)
    : CSSTransformComponent(Is2D::Yes)
    , m_ax(WTFMove(ax))
    , m_ay(WTFMove(ay))
{
}

ExceptionOr<void> CSSSkew::setAx(Ref<CSSNumericValue> ax)
{
    if (!ax->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };

    m_ax = WTFMove(ax);
    return { };
}

ExceptionOr<void> CSSSkew::setAy(Ref<CSSNumericValue> ay)
{
    if (!ay->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };

    m_ay = WTFMove(ay);
    return { };
}

void CSSSkew::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssskew
    builder.append("skew("_s);
    m_ax->serialize(builder);
    if (auto* ayUnitValue = dynamicDowncast<CSSUnitValue>(m_ay.get()); !ayUnitValue || ayUnitValue->value()) {
        builder.append(", "_s);
        m_ay->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSSkew::toMatrix()
{
    RefPtr ax = dynamicDowncast<CSSUnitValue>(m_ax);
    RefPtr ay = dynamicDowncast<CSSUnitValue>(m_ay);
    if (!ax || !ay)
        return Exception { ExceptionCode::TypeError };

    auto x = ax->convertTo(CSSUnitType::CSS_DEG);
    auto y = ay->convertTo(CSSUnitType::CSS_DEG);

    if (!x || !y)
        return Exception { ExceptionCode::TypeError };

    TransformationMatrix matrix { };
    matrix.skew(x->value(), y->value());

    return { DOMMatrix::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::Yes) };
}

std::optional<CSS::TransformFunction> CSSSkew::toCSS() const
{
    auto ax = CSS::convertFromCSSOMValue<CSS::Angle<>>(m_ax);
    if (!ax)
        return { };
    auto ay = CSS::convertFromCSSOMValue<CSS::Angle<>>(m_ay);
    if (!ay)
        return { };

    return CSS::TransformFunction { CSS::SkewFunction { { WTFMove(*ax), WTFMove(*ay) } } };
}

} // namespace WebCore
