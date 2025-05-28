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

#include "StylePrimitiveNumericAdaptors.h"
#include "TranslateTransformOperation.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

// <'translate'> = none | <length-percentage> [ <length-percentage> <length>? ]?
// https://drafts.csswg.org/css-transforms-2/#propdef-translate
struct Translate {
    Translate(CSS::Keyword::None)
        : m_value { }
    {
    }

    Translate(Ref<TranslateTransformOperation> value)
        : m_value { WTFMove(value) }
    {
    }

    bool isNone() const { return !m_value; }

    TranslateTransformOperation* operation() const { return m_value.get(); }
    TranslateTransformOperation* operator->() const { return operation(); }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        RefPtr value = m_value;
        if (!value)
            return functor(CSS::Keyword::None { });

        if (!value->z().isZero())
            return functor(SpaceSeparatedTuple { LengthPercentageAdaptor { value->x() }, LengthPercentageAdaptor { value->y() }, LengthAdaptor { value->z() } });
        if (!value->y().isZero() || value->y().isPercent())
            return functor(SpaceSeparatedTuple { LengthPercentageAdaptor { value->x() }, LengthPercentageAdaptor { value->y() } });
        if (!value->x().isUndefined() && !value->x().isEmptyValue())
            return functor(SpaceSeparatedTuple { LengthPercentageAdaptor { value->x() } });

        return functor(CSS::Keyword::None { });
    }

    bool operator==(const Translate& other) const
    {
        return arePointingToEqualData(m_value, other.m_value);
    }

private:
    RefPtr<TranslateTransformOperation> m_value;
};

template<> struct Blending<Translate> {
    constexpr auto canBlend(const Translate&, const Translate&) -> bool { return true; }
    auto blend(const Translate&, const Translate&, const BlendingContext&) -> Translate;
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::Translate> = true;
