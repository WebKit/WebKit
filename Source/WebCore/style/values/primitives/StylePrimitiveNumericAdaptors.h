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

#include "Length.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// Adaptor for a platform `WebCore::Length` acting as a `<length>`.
struct LengthAdaptor {
    WebCore::Length value;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        using Representation = Length<>;

        switch (value.type()) {
        case WebCore::LengthType::Fixed:
            return functor(Representation { value.value() });
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const LengthAdaptor&) const = default;
};

// Adaptor for a platform `WebCore::Length` acting as a `<length-percentage>`.
struct LengthPercentageAdaptor {
    WebCore::Length value;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        using Representation = LengthPercentage<>;

        switch (value.type()) {
        case WebCore::LengthType::Fixed:
            return functor(Representation::Dimension { value.value() });
        case WebCore::LengthType::Percent:
            return functor(Representation::Percentage { value.value() });
        case WebCore::LengthType::Calculated:
            return functor(Representation::Calc { value.protectedCalculationValue() });
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const LengthPercentageAdaptor&) const = default;
};

// Adaptor for a platform `WebCore::Length` acting as a `<length-percentage> | auto`.
struct LengthPercentageOrAutoAdaptor {
    WebCore::Length value;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        using Representation = LengthPercentage<>;

        switch (value.type()) {
        case WebCore::LengthType::Auto:
            return functor(CSS::Keyword::Auto { });
        case WebCore::LengthType::Fixed:
            return functor(Representation::Dimension { value.value() });
        case WebCore::LengthType::Percent:
            return functor(Representation::Percentage { value.value() });
        case WebCore::LengthType::Calculated:
            return functor(Representation::Calc { value.protectedCalculationValue() });
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const LengthPercentageOrAutoAdaptor&) const = default;
};

// Adaptor for a platform `WebCore::Length` acting as a `<preferred-size>`.
struct PreferredSizeAdaptor {
    WebCore::Length value;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        using Representation = LengthPercentage<>;

        switch (value.type()) {
        case LengthType::Auto:
            return functor(CSS::Keyword::Auto { });
        case LengthType::Content:
            return functor(CSS::Keyword::Content { });
        case LengthType::FillAvailable:
            return functor(CSS::Keyword::WebkitFillAvailable { });
        case LengthType::FitContent:
            return functor(CSS::Keyword::FitContent { });
        case LengthType::Intrinsic:
            return functor(CSS::Keyword::Intrinsic { });
        case LengthType::MinIntrinsic:
            return functor(CSS::Keyword::MinIntrinsic { });
        case LengthType::MinContent:
            return functor(CSS::Keyword::MinContent { });
        case LengthType::MaxContent:
            return functor(CSS::Keyword::MaxContent { });
        case LengthType::Normal:
            return functor(CSS::Keyword::Normal { });
        case WebCore::LengthType::Fixed:
            return functor(Representation::Dimension { value.value() });
        case WebCore::LengthType::Percent:
            return functor(Representation::Percentage { value.value() });
        case WebCore::LengthType::Calculated:
            return functor(Representation::Calc { value.protectedCalculationValue() });
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const PreferredSizeAdaptor&) const = default;
};

// Adaptor for a platform `WebCore::Length` acting as a `<min-size>`.
struct MinimumSizeAdaptor {
    WebCore::Length value;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        using Representation = LengthPercentage<>;

        switch (value.type()) {
        case LengthType::Auto:
            return functor(CSS::Keyword::Auto { });
        case LengthType::Content:
            return functor(CSS::Keyword::Content { });
        case LengthType::FillAvailable:
            return functor(CSS::Keyword::WebkitFillAvailable { });
        case LengthType::FitContent:
            return functor(CSS::Keyword::FitContent { });
        case LengthType::Intrinsic:
            return functor(CSS::Keyword::Intrinsic { });
        case LengthType::MinIntrinsic:
            return functor(CSS::Keyword::MinIntrinsic { });
        case LengthType::MinContent:
            return functor(CSS::Keyword::MinContent { });
        case LengthType::MaxContent:
            return functor(CSS::Keyword::MaxContent { });
        case LengthType::Normal:
            return functor(CSS::Keyword::Normal { });
        case WebCore::LengthType::Fixed:
            return functor(Representation::Dimension { value.value() });
        case WebCore::LengthType::Percent:
            return functor(Representation::Percentage { value.value() });
        case WebCore::LengthType::Calculated:
            return functor(Representation::Calc { value.protectedCalculationValue() });
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const MinimumSizeAdaptor&) const = default;
};

// Adaptor for a platform `WebCore::Length` acting as a `<max-size>`.
struct MaximumSizeAdaptor {
    WebCore::Length value;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        using Representation = LengthPercentage<>;

        switch (value.type()) {
        case LengthType::Undefined:
            return functor(CSS::Keyword::None { });
        case LengthType::Content:
            return functor(CSS::Keyword::Content { });
        case LengthType::FillAvailable:
            return functor(CSS::Keyword::WebkitFillAvailable { });
        case LengthType::FitContent:
            return functor(CSS::Keyword::FitContent { });
        case LengthType::Intrinsic:
            return functor(CSS::Keyword::Intrinsic { });
        case LengthType::MinIntrinsic:
            return functor(CSS::Keyword::MinIntrinsic { });
        case LengthType::MinContent:
            return functor(CSS::Keyword::MinContent { });
        case LengthType::MaxContent:
            return functor(CSS::Keyword::MaxContent { });
        case LengthType::Normal:
            return functor(CSS::Keyword::Normal { });
        case WebCore::LengthType::Fixed:
            return functor(Representation::Dimension { value.value() });
        case WebCore::LengthType::Percent:
            return functor(Representation::Percentage { value.value() });
        case WebCore::LengthType::Calculated:
            return functor(Representation::Calc { value.protectedCalculationValue() });
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const MaximumSizeAdaptor&) const = default;
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::LengthAdaptor> = true;
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::LengthPercentageAdaptor> = true;
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::LengthPercentageOrAutoAdaptor> = true;
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::PreferredSizeAdaptor> = true;
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::MinimumSizeAdaptor> = true;
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::MaximumSizeAdaptor> = true;
