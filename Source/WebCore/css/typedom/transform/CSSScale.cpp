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
#include "CSSScale.h"

#include "CSSFunctionValue.h"
#include "CSSMathValue.h"
#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSScale);

static bool isValidScaleCoord(const CSSNumericValue& coord)
{
    if (auto* mathValue = dynamicDowncast<CSSMathValue>(coord)) {
        auto node = mathValue->toCalcExpressionNode();
        if (!node)
            return false;
        auto resolvedType = node->primitiveType();
        return resolvedType == CSSUnitType::CSS_NUMBER || resolvedType == CSSUnitType::CSS_INTEGER;
    }
    return coord.type().matchesNumber();
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSSNumberish x, CSSNumberish y, std::optional<CSSNumberish>&& z)
{
    auto rectifiedX = CSSNumericValue::rectifyNumberish(WTFMove(x));
    auto rectifiedY = CSSNumericValue::rectifyNumberish(WTFMove(y));
    auto rectifiedZ = z ? CSSNumericValue::rectifyNumberish(WTFMove(*z)) : Ref<CSSNumericValue> { CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER) };

    // https://drafts.css-houdini.org/css-typed-om/#dom-cssscale-cssscale
    if (!isValidScaleCoord(rectifiedX) || !isValidScaleCoord(rectifiedY) || !isValidScaleCoord(rectifiedZ))
        return Exception { TypeError };

    return adoptRef(*new CSSScale(z ? Is2D::No : Is2D::Yes, WTFMove(rectifiedX), WTFMove(rectifiedY), WTFMove(rectifiedZ)));
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSSFunctionValue& cssFunctionValue)
{
    auto makeScale = [&](const Function<ExceptionOr<Ref<CSSScale>>(Vector<RefPtr<CSSNumericValue>>&&)>& create, size_t minNumberOfComponents, std::optional<size_t> maxNumberOfComponents = std::nullopt) -> ExceptionOr<Ref<CSSScale>> {
        Vector<RefPtr<CSSNumericValue>> components;
        for (auto componentCSSValue : cssFunctionValue) {
            auto valueOrException = CSSStyleValueFactory::reifyValue(componentCSSValue, std::nullopt);
            if (valueOrException.hasException())
                return valueOrException.releaseException();
            if (!is<CSSNumericValue>(valueOrException.returnValue()))
                return Exception { TypeError, "Expected a CSSNumericValue."_s };
            components.append(downcast<CSSNumericValue>(valueOrException.releaseReturnValue().ptr()));
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
    case CSSValueScaleX:
        return makeScale([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSScale::create(components[0], CSSNumericFactory::number(1), std::nullopt);
        }, 1);
    case CSSValueScaleY:
        return makeScale([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSScale::create(CSSNumericFactory::number(1), components[0], std::nullopt);
        }, 1);
    case CSSValueScaleZ:
        return makeScale([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSScale::create(CSSNumericFactory::number(1), CSSNumericFactory::number(1), components[0]);
        }, 1);
    case CSSValueScale:
        return makeScale([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSScale::create(components[0], components.size() == 2 ? components[1] : components[0], std::nullopt);
        }, 1, 2);
    case CSSValueScale3d:
        return makeScale([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSScale::create(components[0], components[1], components[2]);
        }, 3);
    default:
        ASSERT_NOT_REACHED();
        return CSSScale::create(CSSNumericFactory::number(1), CSSNumericFactory::number(1), std::nullopt);
    }
}

CSSScale::CSSScale(CSSTransformComponent::Is2D is2D, Ref<CSSNumericValue> x, Ref<CSSNumericValue> y, Ref<CSSNumericValue> z)
    : CSSTransformComponent(is2D)
    , m_x(WTFMove(x))
    , m_y(WTFMove(y))
    , m_z(WTFMove(z))
{
}

void CSSScale::setX(CSSNumberish x)
{
    m_x = CSSNumericValue::rectifyNumberish(WTFMove(x));
}

void CSSScale::setY(CSSNumberish y)
{
    m_y = CSSNumericValue::rectifyNumberish(WTFMove(y));
}

void CSSScale::setZ(CSSNumberish z)
{
    m_z = CSSNumericValue::rectifyNumberish(WTFMove(z));
}

void CSSScale::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssscale
    builder.append(is2D() ? "scale(" : "scale3d(");
    m_x->serialize(builder);
    builder.append(", ");
    m_y->serialize(builder);
    if (!is2D()) {
        builder.append(", ");
        m_z->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSScale::toMatrix()
{
    if (!is<CSSUnitValue>(m_x) || !is<CSSUnitValue>(m_y) || !is<CSSUnitValue>(m_z))
        return Exception { TypeError };

    TransformationMatrix matrix { };

    auto x = downcast<CSSUnitValue>(m_x.get()).value();
    auto y = downcast<CSSUnitValue>(m_y.get()).value();
    auto z = downcast<CSSUnitValue>(m_z.get()).value();

    if (is2D())
        matrix.scaleNonUniform(x, y);
    else
        matrix.scale3d(x, y, z);

    return { DOMMatrix::create(WTFMove(matrix), is2D() ? DOMMatrixReadOnly::Is2D::Yes : DOMMatrixReadOnly::Is2D::No) };
}

RefPtr<CSSValue> CSSScale::toCSSValue() const
{
    auto x = m_x->toCSSValue();
    auto y = m_y->toCSSValue();
    if (!x || !y)
        return nullptr;

    auto result = CSSFunctionValue::create(is2D() ? CSSValueScale : CSSValueScale3d);
    result->append(x.releaseNonNull());
    result->append(y.releaseNonNull());
    if (!is2D()) {
        auto z = m_z->toCSSValue();
        if (!z)
            return nullptr;
        result->append(z.releaseNonNull());
    }
    return result;
}

} // namespace WebCore
