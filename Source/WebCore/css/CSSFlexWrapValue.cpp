/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSFlexWrapValue.h"

#include "CSSKeywordValueInlines.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {

Ref<CSSFlexWrapValue> CSSFlexWrapValue::create(Ref<CSSValue> wrap, Ref<CSSValue> balance)
{
    auto flexWrap = [&] -> CSS::FlexWrap {
        if (valueID(balance.get()) == CSSValueBalance) {
            switch (valueID(wrap.get())) {
            case CSSValueWrap:
                return { CSS::Keyword::Wrap { }, CSS::Keyword::Balance { } };
            case CSSValueWrapReverse:
                return { CSS::Keyword::WrapReverse { }, CSS::Keyword::Balance { } };
            default:
                break;
            }
        }

        ASSERT_NOT_REACHED();
        return CSS::Keyword::Balance { };
    }();

    return adoptRef(*new CSSFlexWrapValue(WTF::move(flexWrap)));
}

CSSFlexWrapValue::CSSFlexWrapValue(CSS::FlexWrap&& flexWrap)
    : CSSValue(ClassType::FlexWrap)
    , m_flexWrap(WTF::move(flexWrap))
{
}

String CSSFlexWrapValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_flexWrap);
}

bool CSSFlexWrapValue::equals(const CSSFlexWrapValue& other) const
{
    return m_flexWrap == other.m_flexWrap;
}

} // namespace WebCore
