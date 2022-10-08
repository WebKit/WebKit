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
#include "CSSRotate.h"

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

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSRotate);

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSSNumberish x, CSSNumberish y, CSSNumberish z, Ref<CSSNumericValue> angle)
{
    if (!angle->type().matches<CSSNumericBaseType::Angle>())
        return Exception { TypeError };

    auto rectifiedX = CSSNumericValue::rectifyNumberish(WTFMove(x));
    auto rectifiedY = CSSNumericValue::rectifyNumberish(WTFMove(y));
    auto rectifiedZ = CSSNumericValue::rectifyNumberish(WTFMove(z));

    if (!rectifiedX->type().matchesNumber()
        || !rectifiedY->type().matchesNumber()
        || !rectifiedZ->type().matchesNumber())
        return Exception { TypeError };

    return adoptRef(*new CSSRotate(Is2D::No, WTFMove(rectifiedX), WTFMove(rectifiedY), WTFMove(rectifiedZ), WTFMove(angle)));
}

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(Ref<CSSNumericValue> angle)
{
    if (!angle->type().matches<CSSNumericBaseType::Angle>())
        return Exception { TypeError };
    return adoptRef(*new CSSRotate(Is2D::Yes,
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        WTFMove(angle)));
}

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSSFunctionValue& cssFunctionValue)
{
    auto makeRotate = [&](const Function<ExceptionOr<Ref<CSSRotate>>(Vector<RefPtr<CSSNumericValue>>&&)>& create, size_t expectedNumberOfComponents) -> ExceptionOr<Ref<CSSRotate>> {
        Vector<RefPtr<CSSNumericValue>> components;
        for (auto componentCSSValue : cssFunctionValue) {
            auto valueOrException = CSSStyleValueFactory::reifyValue(componentCSSValue);
            if (valueOrException.hasException())
                return valueOrException.releaseException();
            if (!is<CSSNumericValue>(valueOrException.returnValue()))
                return Exception { TypeError, "Expected a CSSNumericValue."_s };
            components.append(downcast<CSSNumericValue>(valueOrException.releaseReturnValue().ptr()));
        }
        if (components.size() != expectedNumberOfComponents) {
            ASSERT_NOT_REACHED();
            return Exception { TypeError, "Unexpected number of values."_s };
        }
        return create(WTFMove(components));
    };

    switch (cssFunctionValue.name()) {
    case CSSValueRotateX:
        return makeRotate([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSRotate::create(CSSNumericFactory::number(1), CSSNumericFactory::number(0), CSSNumericFactory::number(0), *components[0]);
        }, 1);
    case CSSValueRotateY:
        return makeRotate([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSRotate::create(CSSNumericFactory::number(0), CSSNumericFactory::number(1), CSSNumericFactory::number(0), *components[0]);
        }, 1);
    case CSSValueRotateZ:
        return makeRotate([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSRotate::create(CSSNumericFactory::number(0), CSSNumericFactory::number(0), CSSNumericFactory::number(1), *components[0]);
        }, 1);
    case CSSValueRotate:
        return makeRotate([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSRotate::create(*components[0]);
        }, 1);
    case CSSValueRotate3d:
        return makeRotate([](Vector<RefPtr<CSSNumericValue>>&& components) {
            return CSSRotate::create(components[0], components[1], components[2], *components[3]);
        }, 4);
    default:
        ASSERT_NOT_REACHED();
        return CSSRotate::create(CSSNumericFactory::deg(0));
    }
}

CSSRotate::CSSRotate(CSSTransformComponent::Is2D is2D, Ref<CSSNumericValue> x, Ref<CSSNumericValue> y, Ref<CSSNumericValue> z, Ref<CSSNumericValue> angle)
    : CSSTransformComponent(is2D)
    , m_x(WTFMove(x))
    , m_y(WTFMove(y))
    , m_z(WTFMove(z))
    , m_angle(WTFMove(angle))
{
}

ExceptionOr<void> CSSRotate::setX(CSSNumberish x)
{
    auto rectified = CSSNumericValue::rectifyNumberish(WTFMove(x));
    if (!rectified->type().matchesNumber())
        return Exception { TypeError };
    m_x = WTFMove(rectified);
    return { };
}

ExceptionOr<void> CSSRotate::setY(CSSNumberish y)
{
    auto rectified = CSSNumericValue::rectifyNumberish(WTFMove(y));
    if (!rectified->type().matchesNumber())
        return Exception { TypeError };
    m_y = WTFMove(rectified);
    return { };
}

ExceptionOr<void> CSSRotate::setZ(CSSNumberish z)
{
    auto rectified = CSSNumericValue::rectifyNumberish(WTFMove(z));
    if (!rectified->type().matchesNumber())
        return Exception { TypeError };
    m_z = WTFMove(rectified);
    return { };
}

ExceptionOr<void> CSSRotate::setAngle(Ref<CSSNumericValue> angle)
{
    if (!angle->type().matches<CSSNumericBaseType::Angle>())
        return Exception { TypeError };
    m_angle = WTFMove(angle);
    return { };
}

void CSSRotate::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssrotate
    builder.append(is2D() ? "rotate(" : "rotate3d(");
    if (!is2D()) {
        m_x->serialize(builder);
        builder.append(", ");
        m_y->serialize(builder);
        builder.append(", ");
        m_z->serialize(builder);
        builder.append(", ");
    }
    m_angle->serialize(builder);
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSRotate::toMatrix()
{
    if (!is<CSSUnitValue>(m_angle) || !is<CSSUnitValue>(m_x) || !is<CSSUnitValue>(m_y) || !is<CSSUnitValue>(m_z))
        return Exception { TypeError };

    auto angle = downcast<CSSUnitValue>(m_angle.get()).convertTo(CSSUnitType::CSS_DEG);
    if (!angle)
        return Exception { TypeError };

    TransformationMatrix matrix { };

    if (is2D())
        matrix.rotate(angle->value());
    else {
        auto x = downcast<CSSUnitValue>(m_x.get()).value();
        auto y = downcast<CSSUnitValue>(m_y.get()).value();
        auto z = downcast<CSSUnitValue>(m_z.get()).value();

        matrix.rotate3d(x, y, z, angle->value());
    }

    return { DOMMatrix::create(WTFMove(matrix), is2D() ? DOMMatrixReadOnly::Is2D::Yes : DOMMatrixReadOnly::Is2D::No) };
}

} // namespace WebCore

#endif
