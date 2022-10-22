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

#include "CSSFunctionValue.h"
#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSSkew);

ExceptionOr<Ref<CSSSkew>> CSSSkew::create(Ref<CSSNumericValue> ax, Ref<CSSNumericValue> ay)
{
    if (!ax->type().matches<CSSNumericBaseType::Angle>()
        || !ay->type().matches<CSSNumericBaseType::Angle>())
        return Exception { TypeError };
    return adoptRef(*new CSSSkew(WTFMove(ax), WTFMove(ay)));
}

ExceptionOr<Ref<CSSSkew>> CSSSkew::create(CSSFunctionValue& cssFunctionValue)
{
    if (cssFunctionValue.name() != CSSValueSkew) {
        ASSERT_NOT_REACHED();
        return CSSSkew::create(CSSNumericFactory::deg(0), CSSNumericFactory::deg(0));
    }

    Vector<Ref<CSSNumericValue>> components;
    for (auto componentCSSValue : cssFunctionValue) {
        auto valueOrException = CSSStyleValueFactory::reifyValue(componentCSSValue);
        if (valueOrException.hasException())
            return valueOrException.releaseException();
        if (!is<CSSNumericValue>(valueOrException.returnValue()))
            return Exception { TypeError, "Expected a CSSNumericValue."_s };
        components.append(downcast<CSSNumericValue>(valueOrException.releaseReturnValue().get()));
    }

    auto numberOfComponents = components.size();
    if (numberOfComponents < 1 || numberOfComponents > 2) {
        ASSERT_NOT_REACHED();
        return Exception { TypeError, "Unexpected number of values."_s };
    }

    if (components.size() == 2)
        return CSSSkew::create(components[0], components[1]);
    return CSSSkew::create(components[0], CSSNumericFactory::deg(0));
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
        return Exception { TypeError };

    m_ax = WTFMove(ax);
    return { };
}

ExceptionOr<void> CSSSkew::setAy(Ref<CSSNumericValue> ay)
{
    if (!ay->type().matches<CSSNumericBaseType::Angle>())
        return Exception { TypeError };

    m_ay = WTFMove(ay);
    return { };
}

void CSSSkew::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssskew
    builder.append("skew(");
    m_ax->serialize(builder);
    if (!is<CSSUnitValue>(m_ay) || downcast<CSSUnitValue>(m_ay.get()).value()) {
        builder.append(", ");
        m_ay->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSSkew::toMatrix()
{
    if (!is<CSSUnitValue>(m_ax) || !is<CSSUnitValue>(m_ay))
        return Exception { TypeError };

    auto x = downcast<CSSUnitValue>(m_ax.get()).convertTo(CSSUnitType::CSS_DEG);
    auto y = downcast<CSSUnitValue>(m_ay.get()).convertTo(CSSUnitType::CSS_DEG);

    if (!x || !y)
        return Exception { TypeError };

    TransformationMatrix matrix { };
    matrix.skew(x->value(), y->value());

    return { DOMMatrix::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::Yes) };
}

} // namespace WebCore
