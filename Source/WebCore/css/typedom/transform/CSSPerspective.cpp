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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSPerspective);

static ExceptionOr<CSSPerspectiveValue> checkLength(CSSPerspectiveValue length)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssperspective-cssperspective
    auto checkKeywordValue = [] (RefPtr<CSSKeywordValue> value) -> ExceptionOr<CSSPerspectiveValue> {
        RELEASE_ASSERT(value);
        if (!equalLettersIgnoringASCIICase(value->value(), "none"_s))
            return Exception { TypeError };
        return { WTFMove(value) };
    };
    return WTF::switchOn(WTFMove(length),
        [] (RefPtr<CSSNumericValue> value) -> ExceptionOr<CSSPerspectiveValue> {
            if (value && !value->type().matches<CSSNumericBaseType::Length>())
                return Exception { TypeError };
            return { WTFMove(value) };
        }, [&] (String value) {
            return checkKeywordValue(CSSKeywordValue::rectifyKeywordish(WTFMove(value)));
        }, checkKeywordValue);
}

ExceptionOr<Ref<CSSPerspective>> CSSPerspective::create(CSSPerspectiveValue length)
{
    auto checkedLength = checkLength(WTFMove(length));
    if (checkedLength.hasException())
        return checkedLength.releaseException();
    return adoptRef(*new CSSPerspective(checkedLength.releaseReturnValue()));
}

ExceptionOr<Ref<CSSPerspective>> CSSPerspective::create(CSSFunctionValue& cssFunctionValue)
{
    if (cssFunctionValue.name() != CSSValuePerspective) {
        ASSERT_NOT_REACHED();
        return CSSPerspective::create("none"_s);
    }

    if (cssFunctionValue.size() != 1 || !cssFunctionValue.item(0)) {
        ASSERT_NOT_REACHED();
        return Exception { TypeError, "Unexpected number of values."_s };
    }

    auto keywordOrNumeric = CSSStyleValueFactory::reifyValue(*cssFunctionValue.item(0));
    if (keywordOrNumeric.hasException())
        return keywordOrNumeric.releaseException();
    auto& keywordOrNumericValue = keywordOrNumeric.returnValue().get();
    return [&]() -> ExceptionOr<Ref<CSSPerspective>> {
        if (is<CSSKeywordValue>(keywordOrNumericValue))
            return CSSPerspective::create(&downcast<CSSKeywordValue>(keywordOrNumericValue));
        if (is<CSSNumericValue>(keywordOrNumericValue))
            return CSSPerspective::create(&downcast<CSSNumericValue>(keywordOrNumericValue));
        return Exception { TypeError, "Expected a CSSNumericValue."_s };
    }();
}

CSSPerspective::CSSPerspective(CSSPerspectiveValue length)
    : CSSTransformComponent(Is2D::No)
    , m_length(WTFMove(length))
{
}

ExceptionOr<void> CSSPerspective::setLength(CSSPerspectiveValue length)
{
    auto checkedLength = checkLength(WTFMove(length));
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
    builder.append("perspective(");
    WTF::switchOn(m_length,
        [&] (const RefPtr<CSSNumericValue>& value) {
            if (auto* unitValue = dynamicDowncast<CSSUnitValue>(value.get()); unitValue && unitValue->value() < 0.0) {
                builder.append("calc(");
                value->serialize(builder);
                builder.append(')');
                return;
            }
            if (value)
                value->serialize(builder);
        }, [&] (const String& value) {
            builder.append(value);
        }, [&] (const RefPtr<CSSKeywordValue>& value) {
            if (CSSStyleValue* styleValue = value.get())
                styleValue->serialize(builder);
        });
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSPerspective::toMatrix()
{
    if (!std::holds_alternative<RefPtr<CSSNumericValue>>(m_length))
        return { DOMMatrix::create({ }, DOMMatrixReadOnly::Is2D::Yes) };

    auto length = std::get<RefPtr<CSSNumericValue>>(m_length);
    if (!is<CSSUnitValue>(length))
        return Exception { TypeError };

    auto valuePx = downcast<CSSUnitValue>(*length).convertTo(CSSUnitType::CSS_PX);
    if (!valuePx)
        return Exception { TypeError, "Length unit is not compatible with 'px'"_s };

    TransformationMatrix matrix { };
    matrix.applyPerspective(valuePx->value());

    return { DOMMatrix::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::No) };
}

RefPtr<CSSValue> CSSPerspective::toCSSValue() const
{
    RefPtr<CSSValue> length;
    switchOn(m_length, [&](const RefPtr<CSSNumericValue>& numericValue) {
        length = numericValue->toCSSValue();
    }, [&](const String&) {
        // FIXME: Implement this.
    }, [&](const RefPtr<CSSKeywordValue>& keywordValue) {
        length = keywordValue->toCSSValue();
    });
    if (!length)
        return nullptr;

    auto result = CSSFunctionValue::create(CSSValuePerspective);
    result->append(length.releaseNonNull());
    return result;
}

} // namespace WebCore
