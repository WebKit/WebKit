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
#include "CSSTranslate.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSFunctionValue.h"
#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSTranslate);

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(Ref<CSSNumericValue> x, Ref<CSSNumericValue> y, RefPtr<CSSNumericValue> z)
{
    auto is2D = z ? CSSTransformComponent::Is2D::No : CSSTransformComponent::Is2D::Yes;
    if (!z)
        z = CSSUnitValue::create(0.0, CSSUnitType::CSS_PX);

    if (!x->type().matchesTypeOrPercentage<CSSNumericBaseType::Length>()
        || !y->type().matchesTypeOrPercentage<CSSNumericBaseType::Length>()
        || !z->type().matches<CSSNumericBaseType::Length>())
        return Exception { TypeError };

    return adoptRef(*new CSSTranslate(is2D, WTFMove(x), WTFMove(y), z.releaseNonNull()));
}

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(CSSFunctionValue& cssFunctionValue)
{
    auto makeTranslate = [&](const Function<ExceptionOr<Ref<CSSTranslate>>(Vector<Ref<CSSNumericValue>>&&)>& create, size_t minNumberOfComponents, std::optional<size_t> maxNumberOfComponents = std::nullopt) -> ExceptionOr<Ref<CSSTranslate>> {
        Vector<Ref<CSSNumericValue>> components;
        for (auto componentCSSValue : cssFunctionValue) {
            auto valueOrException = CSSStyleValueFactory::reifyValue(componentCSSValue);
            if (valueOrException.hasException())
                return valueOrException.releaseException();
            if (!is<CSSNumericValue>(valueOrException.returnValue()))
                return Exception { TypeError, "Expected a CSSNumericValue."_s };
            components.append(downcast<CSSNumericValue>(valueOrException.releaseReturnValue().get()));
        }
        if (!maxNumberOfComponents)
            maxNumberOfComponents = minNumberOfComponents;
        auto numberOfComponents = components.size();
        if (numberOfComponents < minNumberOfComponents || numberOfComponents > maxNumberOfComponents) {
            ASSERT_NOT_REACHED();
            return Exception { TypeError, "Unexpected number of values."_s };
        }
        return create(WTFMove(components));
    };

    switch (cssFunctionValue.name()) {
    case CSSValueTranslateX:
        return makeTranslate([](Vector<Ref<CSSNumericValue>>&& components) {
            return CSSTranslate::create(components[0], CSSNumericFactory::px(0), nullptr);
        }, 1);
    case CSSValueTranslateY:
        return makeTranslate([](Vector<Ref<CSSNumericValue>>&& components) {
            return CSSTranslate::create(CSSNumericFactory::px(0), components[0], nullptr);
        }, 1);
    case CSSValueTranslateZ:
        return makeTranslate([](Vector<Ref<CSSNumericValue>>&& components) {
            return CSSTranslate::create(CSSNumericFactory::px(0), CSSNumericFactory::px(0), components[0].ptr());
        }, 1);
    case CSSValueTranslate:
        return makeTranslate([](Vector<Ref<CSSNumericValue>>&& components) {
            if (components.size() == 2)
                return CSSTranslate::create(components[0], components[1], nullptr);
            return CSSTranslate::create(components[0], CSSNumericFactory::px(0), nullptr);
        }, 1, 2);
    case CSSValueTranslate3d:
        return makeTranslate([](Vector<Ref<CSSNumericValue>>&& components) {
            return CSSTranslate::create(components[0], components[1], components[2].ptr());
        }, 3);
    default:
        ASSERT_NOT_REACHED();
        return CSSTranslate::create(CSSNumericFactory::px(0), CSSNumericFactory::px(0), nullptr);
    }
}

CSSTranslate::CSSTranslate(CSSTransformComponent::Is2D is2D, Ref<CSSNumericValue> x, Ref<CSSNumericValue> y, Ref<CSSNumericValue> z)
    : CSSTransformComponent(is2D)
    , m_x(WTFMove(x))
    , m_y(WTFMove(y))
    , m_z(WTFMove(z))
{
}

void CSSTranslate::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-csstranslate
    builder.append(is2D() ? "translate(" : "translate3d(");
    m_x->serialize(builder);
    builder.append(", ");
    m_y->serialize(builder);
    if (!is2D()) {
        builder.append(", ");
        m_z->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<void> CSSTranslate::setZ(Ref<CSSNumericValue> z)
{
    if (!z->type().matches<CSSNumericBaseType::Length>())
        return Exception { TypeError };

    m_z = WTFMove(z);
    return { };
}

ExceptionOr<Ref<DOMMatrix>> CSSTranslate::toMatrix()
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-csstransformcomponent-tomatrix
    // As the entries of such a matrix are defined relative to the px unit, if any <length>s
    // in this involved in generating the matrix are not compatible units with px (such as
    // relative lengths or percentages), throw a TypeError.
    if (!is<CSSUnitValue>(m_x) || !is<CSSUnitValue>(m_y) || !is<CSSUnitValue>(m_z))
        return Exception { TypeError };

    auto xPx = downcast<CSSUnitValue>(m_x.get()).convertTo(CSSUnitType::CSS_PX);
    auto yPx = downcast<CSSUnitValue>(m_y.get()).convertTo(CSSUnitType::CSS_PX);
    auto zPx = downcast<CSSUnitValue>(m_z.get()).convertTo(CSSUnitType::CSS_PX);

    if (!xPx || !yPx || !zPx)
        return Exception { TypeError };

    auto x = xPx->value();
    auto y = yPx->value();
    auto z = zPx->value();

    TransformationMatrix matrix { };

    if (is2D())
        matrix.translate(x, y);
    else
        matrix.translate3d(x, y, z);

    return { DOMMatrix::create(WTFMove(matrix), is2D() ? DOMMatrixReadOnly::Is2D::Yes : DOMMatrixReadOnly::Is2D::No) };
}

} // namespace WebCore

#endif
