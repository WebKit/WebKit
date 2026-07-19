/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include "CSSKeywordValue.h"
#include "CSSValuePair.h"
#include "RenderStyleConstants.h"
#include "StyleBuilderChecking.h"
#include "StyleKeyword+Mappings.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

template<typename T> requires std::is_enum_v<T> struct CSSValueConversion<T> {
    T operator()(BuilderState&, const CSSKeywordValue& value)
    {
        return fromCSSValueID<T>(value.valueID());
    }
    T operator()(BuilderState& state, const CSSValue& value)
    {
        RefPtr keywordValue = requiredDowncast<CSSKeywordValue>(state, value);
        if (!keywordValue)
            return static_cast<T>(0);
        return fromCSSValueID<T>(keywordValue->valueID());
    }
};

template<> struct CSSValueConversion<FillBox> {
    FillBox operator()(BuilderState&, const CSSKeywordValue& value)
    {
        return fromCSSValueID<FillBox>(value.valueID());
    }
    FillBox operator()(BuilderState& state, const CSSValue& value)
    {
        if (auto* pair = dynamicDowncast<CSSValuePair>(value))
            return isCSSValuePairForBorderAreaText(*pair) ? FillBox::BorderAreaText : FillBox::BorderBox;
        RefPtr keywordValue = requiredDowncast<CSSKeywordValue>(state, value);
        if (!keywordValue)
            return FillBox::BorderBox;
        return fromCSSValueID<FillBox>(keywordValue->valueID());
    }

private:
    static bool isCSSValuePairForBorderAreaText(const CSSValuePair& pair)
    {
        auto* first = dynamicDowncast<CSSKeywordValue>(pair.first());
        auto* second = dynamicDowncast<CSSKeywordValue>(pair.second());
        return first && second
            && first->valueID() == CSSValueBorderArea
            && second->valueID() == CSSValueText;
    }
};

} // namespace Style
} // namespace WebCore
