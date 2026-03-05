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
#include "CSSPerspective.h"

#include "CSSFunctionValue.h"
#include "CSSKeywordValue.h"
#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSPerspective);

static ExceptionOr<CSSPerspectiveValue> checkLength(CSSPerspectiveValue length)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssperspective-cssperspective
    auto checkKeywordValue = [](Ref<CSSKeywordValue> value) -> ExceptionOr<CSSPerspectiveValue> {
        if (!equalLettersIgnoringASCIICase(value->value(), "none"_s))
            return Exception { ExceptionCode::TypeError };
        return { WTF::move(value) };
    };
    return WTF::switchOn(WTF::move(length),
        [](Ref<CSSNumericValue>&& value) -> ExceptionOr<CSSPerspectiveValue> {
            if (!value->type().matches<CSSNumericBaseType::Length>())
                return Exception { ExceptionCode::TypeError };
            return { WTF::move(value) };
        },
        [&](String&& value) {
            return checkKeywordValue(CSSKeywordValue::rectifyKeywordish(WTF::move(value)));
        },
        checkKeywordValue
    );
}

ExceptionOr<Ref<CSSPerspective>> CSSPerspective::create(CSSPerspectiveValue length)
{
    auto checkedLength = checkLength(WTF::move(length));
    if (checkedLength.hasException())
        return checkedLength.releaseException();
    return adoptRef(*new CSSPerspective(checkedLength.releaseReturnValue()));
}

ExceptionOr<Ref<CSSPerspective>> CSSPerspective::create(Ref<const CSSFunctionValue> cssFunctionValue, Document& document)
{
    if (cssFunctionValue->name() != CSSValuePerspective) {
        ASSERT_NOT_REACHED();
        return CSSPerspective::create("none"_s);
    }

    if (cssFunctionValue->size() != 1 || !cssFunctionValue->item(0)) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::TypeError, "Unexpected number of values."_s };
    }

    auto keywordOrNumeric = CSSStyleValueFactory::reifyValue(document, *cssFunctionValue->item(0), std::nullopt);
    if (keywordOrNumeric.hasException())
        return keywordOrNumeric.releaseException();
    Ref keywordOrNumericValue = keywordOrNumeric.returnValue();
    return [&] -> ExceptionOr<Ref<CSSPerspective>> {
        if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(keywordOrNumericValue))
            return CSSPerspective::create(keywordValue.releaseNonNull());
        if (RefPtr numericValue = dynamicDowncast<CSSNumericValue>(keywordOrNumericValue))
            return CSSPerspective::create(numericValue.releaseNonNull());
        return Exception { ExceptionCode::TypeError, "Expected a CSSNumericValue."_s };
    }();
}

CSSPerspective::CSSPerspective(CSSPerspectiveValue length)
    : CSSTransformComponent(Is2D::No)
    , m_length(WTF::move(length))
{
}

CSSPerspective::~CSSPerspective() = default;

ExceptionOr<void> CSSPerspective::setLength(CSSPerspectiveValue length)
{
    auto checkedLength = checkLength(WTF::move(length));
    if (checkedLength.hasException())
        return checkedLength.releaseException();
    m_length = checkedLength.releaseReturnValue();
    return { };
}

void CSSPerspective::setIs2D(bool)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssperspective-is2d says to do nothing here.
}

void CSSPerspective::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssperspective
    builder.append("perspective("_s);
    WTF::switchOn(m_length,
        [&](const Ref<CSSNumericValue>& value) {
            if (RefPtr unitValue = dynamicDowncast<CSSUnitValue>(value); unitValue && unitValue->value() < 0.0) {
                builder.append("calc("_s);
                value->serialize(builder);
                builder.append(')');
                return;
            }
            value->serialize(builder);
        },
        [&](const String& value) {
            builder.append(value);
        },
        [&](const Ref<CSSKeywordValue>& value) {
            value->serialize(builder);
        }
    );
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSPerspective::toMatrix()
{
    if (!std::holds_alternative<Ref<CSSNumericValue>>(m_length))
        return { DOMMatrix::create({ }, DOMMatrixReadOnly::Is2D::Yes) };

    RefPtr length = dynamicDowncast<CSSUnitValue>(std::get<Ref<CSSNumericValue>>(m_length));
    if (!length)
        return Exception { ExceptionCode::TypeError };

    auto valuePx = length->convertTo(CSSUnitType::CSS_PX);
    if (!valuePx)
        return Exception { ExceptionCode::TypeError, "Length unit is not compatible with 'px'"_s };

    TransformationMatrix matrix { };
    matrix.applyPerspective(valuePx->value());

    return { DOMMatrix::create(WTF::move(matrix), DOMMatrixReadOnly::Is2D::No) };
}

RefPtr<CSSValue> CSSPerspective::toCSSValue() const
{
    RefPtr length = switchOn(m_length,
        [](const Ref<CSSNumericValue>& numericValue) -> RefPtr<CSSValue> {
            return numericValue->toCSSValue();
        },
        [](const String&) -> RefPtr<CSSValue> {
            // FIXME: Implement this.
            return nullptr;
        },
        [](const Ref<CSSKeywordValue>& keywordValue) -> RefPtr<CSSValue> {
            return keywordValue->toCSSValue();
        }
    );
    if (!length)
        return nullptr;

    return CSSFunctionValue::create(CSSValuePerspective, length.releaseNonNull());
}

} // namespace WebCore
