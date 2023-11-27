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
#include "CSSSkewX.h"

#include "CSSFunctionValue.h"
#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSStyleValueFactory.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSSkewX);

ExceptionOr<Ref<CSSSkewX>> CSSSkewX::create(Ref<CSSNumericValue> ax)
{
    if (!ax->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };
    return adoptRef(*new CSSSkewX(WTFMove(ax)));
}

ExceptionOr<Ref<CSSSkewX>> CSSSkewX::create(CSSFunctionValue& cssFunctionValue)
{
    if (cssFunctionValue.name() != CSSValueSkewX) {
        ASSERT_NOT_REACHED();
        return CSSSkewX::create(CSSNumericFactory::deg(0));
    }

    if (cssFunctionValue.size() != 1 || !cssFunctionValue.item(0)) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::TypeError, "Unexpected number of values."_s };
    }

    auto valueOrException = CSSStyleValueFactory::reifyValue(*cssFunctionValue.item(0), std::nullopt);
    if (valueOrException.hasException())
        return valueOrException.releaseException();
    RefPtr numericValue = dynamicDowncast<CSSNumericValue>(valueOrException.releaseReturnValue());
    if (!numericValue)
        return Exception { ExceptionCode::TypeError, "Expected a CSSNumericValue."_s };
    return CSSSkewX::create(numericValue.releaseNonNull());
}

CSSSkewX::CSSSkewX(Ref<CSSNumericValue> ax)
    : CSSTransformComponent(Is2D::Yes)
    , m_ax(WTFMove(ax))
{
}

ExceptionOr<void> CSSSkewX::setAx(Ref<CSSNumericValue> ax)
{
    if (!ax->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };

    m_ax = WTFMove(ax);
    return { };
}

void CSSSkewX::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssskewx
    builder.append("skewX(");
    m_ax->serialize(builder);
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSSkewX::toMatrix()
{
    RefPtr ax = dynamicDowncast<CSSUnitValue>(m_ax);
    if (!ax)
        return Exception { ExceptionCode::TypeError };

    auto x = ax->convertTo(CSSUnitType::CSS_DEG);
    if (!x)
        return Exception { ExceptionCode::TypeError };

    TransformationMatrix matrix { };
    matrix.skewX(x->value());

    return { DOMMatrix::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::Yes) };
}

RefPtr<CSSValue> CSSSkewX::toCSSValue() const
{
    auto ax = m_ax->toCSSValue();
    if (!ax)
        return nullptr;
    return CSSFunctionValue::create(CSSValueSkewX, ax.releaseNonNull());
}

} // namespace WebCore
