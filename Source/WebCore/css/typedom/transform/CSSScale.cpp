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

#include "CSSMathValue.h"
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

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSScale);

static bool isValidScaleCoord(const CSSNumericValue& coord)
{
    return coord.type().matchesNumber();
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSSNumberish x, CSSNumberish y, std::optional<CSSNumberish>&& z)
{
    auto rectifiedX = CSSNumericValue::rectifyNumberish(WTFMove(x));
    auto rectifiedY = CSSNumericValue::rectifyNumberish(WTFMove(y));
    auto rectifiedZ = z ? CSSNumericValue::rectifyNumberish(WTFMove(*z)) : Ref<CSSNumericValue> { CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER) };

    // https://drafts.css-houdini.org/css-typed-om/#dom-cssscale-cssscale
    if (!isValidScaleCoord(rectifiedX) || !isValidScaleCoord(rectifiedY) || !isValidScaleCoord(rectifiedZ))
        return Exception { ExceptionCode::TypeError };

    return adoptRef(*new CSSScale(z ? Is2D::No : Is2D::Yes, WTFMove(rectifiedX), WTFMove(rectifiedY), WTFMove(rectifiedZ)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyScaleNumber(const CSS::NumberOrPercentageResolvedToNumber& numeric)
{
    // CSSScale only supports "number" types. Convert any raw percentage values ("as if" done at parse) and throw for
    // any percentage calc() values.

    return WTF::switchOn(numeric.value,
        [](const CSS::Number<>& number) -> ExceptionOr<Ref<CSSNumericValue>> {
            return CSSNumericFactory::reifyNumeric(number);
        },
        [](const CSS::Percentage<>& percentage) -> ExceptionOr<Ref<CSSNumericValue>> {
            if (auto raw = percentage.raw())
                return static_reference_cast<CSSNumericValue>(CSSNumericFactory::number(raw->value / 100.0));
            return Exception { ExceptionCode::TypeError };
        }
    );
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSS::Scale3D scale3D)
{
    auto x = reifyScaleNumber(scale3D.x);
    if (x.hasException())
        return x.releaseException();
    auto y = reifyScaleNumber(scale3D.y);
    if (y.hasException())
        return y.releaseException();
    auto z = reifyScaleNumber(scale3D.z);
    if (z.hasException())
        return z.releaseException();

    return adoptRef(*new CSSScale(Is2D::No,
        x.releaseReturnValue(),
        y.releaseReturnValue(),
        z.releaseReturnValue())
    );
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSS::Scale scale)
{
    auto x = reifyScaleNumber(scale.x);
    if (x.hasException())
        return x.releaseException();
    auto y = scale.y ? reifyScaleNumber(*scale.y) : x;
    if (y.hasException())
        return y.releaseException();

    return adoptRef(*new CSSScale(Is2D::Yes,
        x.releaseReturnValue(),
        y.releaseReturnValue(),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER))
    );
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSS::ScaleX scaleX)
{
    auto x = reifyScaleNumber(scaleX.value);
    if (x.hasException())
        return x.releaseException();

    return adoptRef(*new CSSScale(Is2D::Yes,
        x.releaseReturnValue(),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER))
    );
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSS::ScaleY scaleY)
{
    auto y = reifyScaleNumber(scaleY.value);
    if (y.hasException())
        return y.releaseException();

    return adoptRef(*new CSSScale(Is2D::Yes,
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        y.releaseReturnValue(),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER))
    );
}

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSS::ScaleZ scaleZ)
{
    auto z = reifyScaleNumber(scaleZ.value);
    if (z.hasException())
        return z.releaseException();

    return adoptRef(*new CSSScale(Is2D::No,
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER),
        z.releaseReturnValue())
    );
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
    builder.append(is2D() ? "scale("_s : "scale3d("_s);
    m_x->serialize(builder);
    builder.append(", "_s);
    m_y->serialize(builder);
    if (!is2D()) {
        builder.append(", "_s);
        m_z->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSScale::toMatrix()
{
    auto* xUnitValue = dynamicDowncast<CSSUnitValue>(m_x.get());
    auto* yUnitValue = dynamicDowncast<CSSUnitValue>(m_y.get());
    auto* zUnitValue = dynamicDowncast<CSSUnitValue>(m_z.get());
    if (!xUnitValue || !yUnitValue || !zUnitValue)
        return Exception { ExceptionCode::TypeError };

    TransformationMatrix matrix { };

    auto x = xUnitValue->value();
    auto y = yUnitValue->value();
    auto z = zUnitValue->value();

    if (is2D())
        matrix.scaleNonUniform(x, y);
    else
        matrix.scale3d(x, y, z);

    return { DOMMatrix::create(WTFMove(matrix), is2D() ? DOMMatrixReadOnly::Is2D::Yes : DOMMatrixReadOnly::Is2D::No) };
}

std::optional<CSS::TransformFunction> CSSScale::toCSS() const
{
    auto x = CSS::convertFromCSSOMValue<CSS::Number<>>(m_x);
    if (!x)
        return { };
    auto y = CSS::convertFromCSSOMValue<CSS::Number<>>(m_y);
    if (!y)
        return { };

    if (is2D())
        return CSS::TransformFunction { CSS::ScaleFunction { { { WTFMove(*x) }, CSS::NumberOrPercentageResolvedToNumber { WTFMove(*y) } } } };

    auto z = CSS::convertFromCSSOMValue<CSS::Number<>>(m_z);
    if (!z)
        return { };

    return CSS::TransformFunction { CSS::Scale3DFunction { { { WTFMove(*x) }, { WTFMove(*y) }, { WTFMove(*z) } } } };
}

} // namespace WebCore
