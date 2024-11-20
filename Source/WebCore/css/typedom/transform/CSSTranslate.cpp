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

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSTranslate);

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(Ref<CSSNumericValue> x, Ref<CSSNumericValue> y, RefPtr<CSSNumericValue> z)
{
    auto is2D = z ? CSSTransformComponent::Is2D::No : CSSTransformComponent::Is2D::Yes;
    if (!z)
        z = CSSUnitValue::create(0.0, CSSUnitType::CSS_PX);

    if (!x->type().matchesTypeOrPercentage<CSSNumericBaseType::Length>()
        || !y->type().matchesTypeOrPercentage<CSSNumericBaseType::Length>()
        || !z->type().matches<CSSNumericBaseType::Length>())
        return Exception { ExceptionCode::TypeError };

    return adoptRef(*new CSSTranslate(is2D, WTFMove(x), WTFMove(y), z.releaseNonNull()));
}

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(CSS::Translate3D translate3D)
{
    auto x = CSSNumericFactory::reifyNumeric(translate3D.x);
    if (x.hasException())
        return x.releaseException();
    auto y = CSSNumericFactory::reifyNumeric(translate3D.y);
    if (y.hasException())
        return y.releaseException();
    auto z = CSSNumericFactory::reifyNumeric(translate3D.z);
    if (z.hasException())
        return z.releaseException();

    return adoptRef(*new CSSTranslate(Is2D::No,
        x.releaseReturnValue(),
        y.releaseReturnValue(),
        z.releaseReturnValue())
    );
}

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(CSS::Translate translate)
{
    auto x = CSSNumericFactory::reifyNumeric(translate.x);
    if (x.hasException())
        return x.releaseException();

    auto y = translate.y ? CSSNumericFactory::reifyNumeric(*translate.y) : ExceptionOr<Ref<CSSNumericValue>> { CSSUnitValue::create(0.0, CSSUnitType::CSS_PX) };
    if (y.hasException())
        return y.releaseException();

    return adoptRef(*new CSSTranslate(Is2D::Yes,
        x.releaseReturnValue(),
        y.releaseReturnValue(),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_PX))
    );
}

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(CSS::TranslateX translateX)
{
    auto x = CSSNumericFactory::reifyNumeric(translateX.value);
    if (x.hasException())
        return x.releaseException();

    return adoptRef(*new CSSTranslate(Is2D::Yes,
        x.releaseReturnValue(),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_PX),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_PX))
    );
}

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(CSS::TranslateY translateY)
{
    auto y = CSSNumericFactory::reifyNumeric(translateY.value);
    if (y.hasException())
        return y.releaseException();

    return adoptRef(*new CSSTranslate(Is2D::Yes,
        CSSUnitValue::create(0.0, CSSUnitType::CSS_PX),
        y.releaseReturnValue(),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_PX))
    );
}

ExceptionOr<Ref<CSSTranslate>> CSSTranslate::create(CSS::TranslateZ translateZ)
{
    auto z = CSSNumericFactory::reifyNumeric(translateZ.value);
    if (z.hasException())
        return z.releaseException();

    return adoptRef(*new CSSTranslate(Is2D::No,
        CSSUnitValue::create(0.0, CSSUnitType::CSS_PX),
        CSSUnitValue::create(0.0, CSSUnitType::CSS_PX),
        z.releaseReturnValue())
    );
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
    builder.append(is2D() ? "translate("_s : "translate3d("_s);
    m_x->serialize(builder);
    builder.append(", "_s);
    m_y->serialize(builder);
    if (!is2D()) {
        builder.append(", "_s);
        m_z->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<void> CSSTranslate::setZ(Ref<CSSNumericValue> z)
{
    if (!z->type().matches<CSSNumericBaseType::Length>())
        return Exception { ExceptionCode::TypeError };

    m_z = WTFMove(z);
    return { };
}

ExceptionOr<Ref<DOMMatrix>> CSSTranslate::toMatrix()
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-csstransformcomponent-tomatrix
    // As the entries of such a matrix are defined relative to the px unit, if any <length>s
    // in this involved in generating the matrix are not compatible units with px (such as
    // relative lengths or percentages), throw a TypeError.
    RefPtr xUnitValue = dynamicDowncast<CSSUnitValue>(m_x);
    RefPtr yUnitValue = dynamicDowncast<CSSUnitValue>(m_y);
    RefPtr zUnitValue = dynamicDowncast<CSSUnitValue>(m_z);
    if (!xUnitValue || !yUnitValue || !zUnitValue)
        return Exception { ExceptionCode::TypeError };

    auto xPx = xUnitValue->convertTo(CSSUnitType::CSS_PX);
    auto yPx = yUnitValue->convertTo(CSSUnitType::CSS_PX);
    auto zPx = zUnitValue->convertTo(CSSUnitType::CSS_PX);

    if (!xPx || !yPx || !zPx)
        return Exception { ExceptionCode::TypeError };

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

std::optional<CSS::TransformFunction> CSSTranslate::toCSS() const
{
    auto x = CSS::convertFromCSSOMValue<CSS::LengthPercentage<>>(m_x);
    if (!x)
        return { };
    auto y = CSS::convertFromCSSOMValue<CSS::LengthPercentage<>>(m_y);
    if (!y)
        return { };

    if (is2D())
        return CSS::TransformFunction { CSS::TranslateFunction { { WTFMove(*x), WTFMove(*y) } } };

    auto z = CSS::convertFromCSSOMValue<CSS::Length<>>(m_z);
    if (!z)
        return { };

    return CSS::TransformFunction { CSS::Translate3DFunction { { WTFMove(*x), WTFMove(*y), WTFMove(*z) } } };
}

} // namespace WebCore
