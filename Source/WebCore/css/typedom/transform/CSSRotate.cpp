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

#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSPrimitiveNumericTypes+CSSOMConversion.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSRotate);

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSSNumberish x, CSSNumberish y, CSSNumberish z, Ref<CSSNumericValue> angle)
{
    if (!angle->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };

    auto rectifiedX = CSSNumericValue::rectifyNumberish(WTFMove(x));
    auto rectifiedY = CSSNumericValue::rectifyNumberish(WTFMove(y));
    auto rectifiedZ = CSSNumericValue::rectifyNumberish(WTFMove(z));

    if (!rectifiedX->type().matchesNumber()
        || !rectifiedY->type().matchesNumber()
        || !rectifiedZ->type().matchesNumber())
        return Exception { ExceptionCode::TypeError };

    return adoptRef(*new CSSRotate(Is2D::No, WTFMove(rectifiedX), WTFMove(rectifiedY), WTFMove(rectifiedZ), WTFMove(angle)));
}

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(Ref<CSSNumericValue> angle)
{
    if (!angle->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };
    return adoptRef(*new CSSRotate(Is2D::Yes,
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        WTFMove(angle)));
}

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSS::Rotate3D rotate3D)
{
    auto x = CSSNumericFactory::reifyNumeric(rotate3D.x);
    if (x.hasException())
        return x.releaseException();
    auto y = CSSNumericFactory::reifyNumeric(rotate3D.y);
    if (y.hasException())
        return y.releaseException();
    auto z = CSSNumericFactory::reifyNumeric(rotate3D.z);
    if (z.hasException())
        return z.releaseException();
    auto angle = CSSNumericFactory::reifyNumeric(rotate3D.angle);
    if (angle.hasException())
        return angle.releaseException();

    return adoptRef(*new CSSRotate(Is2D::No,
        x.releaseReturnValue(),
        y.releaseReturnValue(),
        z.releaseReturnValue(),
        angle.releaseReturnValue())
    );
}

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSS::Rotate rotate)
{
    auto angle = CSSNumericFactory::reifyNumeric(rotate.value);
    if (angle.hasException())
        return angle.releaseException();

    return adoptRef(*new CSSRotate(Is2D::Yes,
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        angle.releaseReturnValue())
    );
}

ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSS::RotateX rotateX)
{
    auto angle = CSSNumericFactory::reifyNumeric(rotateX.value);
    if (angle.hasException())
        return angle.releaseException();

    return adoptRef(*new CSSRotate(Is2D::No,
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        angle.releaseReturnValue())
    );
}
ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSS::RotateY rotateY)
{
    auto angle = CSSNumericFactory::reifyNumeric(rotateY.value);
    if (angle.hasException())
        return angle.releaseException();

    return adoptRef(*new CSSRotate(Is2D::No,
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        angle.releaseReturnValue())
    );
}
ExceptionOr<Ref<CSSRotate>> CSSRotate::create(CSS::RotateZ rotateZ)
{
    auto angle = CSSNumericFactory::reifyNumeric(rotateZ.value);
    if (angle.hasException())
        return angle.releaseException();

    return adoptRef(*new CSSRotate(Is2D::No,
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        angle.releaseReturnValue())
    );
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
        return Exception { ExceptionCode::TypeError };
    m_x = WTFMove(rectified);
    return { };
}

ExceptionOr<void> CSSRotate::setY(CSSNumberish y)
{
    auto rectified = CSSNumericValue::rectifyNumberish(WTFMove(y));
    if (!rectified->type().matchesNumber())
        return Exception { ExceptionCode::TypeError };
    m_y = WTFMove(rectified);
    return { };
}

ExceptionOr<void> CSSRotate::setZ(CSSNumberish z)
{
    auto rectified = CSSNumericValue::rectifyNumberish(WTFMove(z));
    if (!rectified->type().matchesNumber())
        return Exception { ExceptionCode::TypeError };
    m_z = WTFMove(rectified);
    return { };
}

ExceptionOr<void> CSSRotate::setAngle(Ref<CSSNumericValue> angle)
{
    if (!angle->type().matches<CSSNumericBaseType::Angle>())
        return Exception { ExceptionCode::TypeError };
    m_angle = WTFMove(angle);
    return { };
}

void CSSRotate::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssrotate
    builder.append(is2D() ? "rotate("_s : "rotate3d("_s);
    if (!is2D()) {
        m_x->serialize(builder);
        builder.append(", "_s);
        m_y->serialize(builder);
        builder.append(", "_s);
        m_z->serialize(builder);
        builder.append(", "_s);
    }
    m_angle->serialize(builder);
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSRotate::toMatrix()
{
    RefPtr angleUnitValue = dynamicDowncast<CSSUnitValue>(m_angle);
    RefPtr xUnitValue = dynamicDowncast<CSSUnitValue>(m_x);
    RefPtr yUnitValue = dynamicDowncast<CSSUnitValue>(m_y);
    RefPtr zUnitValue = dynamicDowncast<CSSUnitValue>(m_z);
    if (!angleUnitValue || !xUnitValue || !yUnitValue || !zUnitValue)
        return Exception { ExceptionCode::TypeError };

    auto angle = angleUnitValue->convertTo(CSSUnitType::CSS_DEG);
    if (!angle)
        return Exception { ExceptionCode::TypeError };

    TransformationMatrix matrix { };

    if (is2D())
        matrix.rotate(angle->value());
    else {
        auto x = xUnitValue->value();
        auto y = yUnitValue->value();
        auto z = zUnitValue->value();

        matrix.rotate3d(x, y, z, angle->value());
    }

    return { DOMMatrix::create(WTFMove(matrix), is2D() ? DOMMatrixReadOnly::Is2D::Yes : DOMMatrixReadOnly::Is2D::No) };
}

std::optional<CSS::TransformFunction> CSSRotate::toCSS() const
{
    auto angle = CSS::convertFromCSSOMValue<CSS::Angle<>>(m_angle);
    if (!angle)
        return { };

    if (is2D())
        return CSS::TransformFunction { CSS::RotateFunction { { WTFMove(*angle) } } };

    auto x = CSS::convertFromCSSOMValue<CSS::Number<>>(m_x);
    if (!x)
        return { };
    auto y = CSS::convertFromCSSOMValue<CSS::Number<>>(m_y);
    if (!y)
        return { };
    auto z = CSS::convertFromCSSOMValue<CSS::Number<>>(m_z);
    if (!z)
        return { };

    return CSS::TransformFunction { CSS::Rotate3DFunction { { WTFMove(*x), WTFMove(*y), WTFMove(*z), WTFMove(*angle) } } };
}

} // namespace WebCore
