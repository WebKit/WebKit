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

#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleSizing.h"

namespace WebCore {
namespace Style {

template<typename SizeType, typename Reference>
auto evaluateForSizeType(const SizeType& size, Reference referenceLength) -> Reference
{
    return WTF::switchOn(size,
        [&](const typename SizeType::Fixed& fixed) -> Reference {
            return evaluate(fixed, referenceLength);
        },
        [&](const typename SizeType::Percentage& percentage) -> Reference {
            return evaluate(percentage, referenceLength);
        },
        [&](const typename SizeType::Calc& calc) -> Reference {
            return evaluate(calc, referenceLength);
        },
        [&](const CSS::Keyword::Auto&) -> Reference {
            return referenceLength;
        },
        [](const CSS::Keyword::None&) -> Reference {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::Content&) -> Reference {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::MinContent&) -> Reference {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::MaxContent&) -> Reference {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::FitContent&) -> Reference {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::Intrinsic&) -> Reference {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::MinIntrinsic&) -> Reference {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> Reference {
            return referenceLength;
        }
    );
}

template<typename SizeType>
auto evaluateMinimumForSizeType(const SizeType& size, LayoutUnit referenceLength) -> LayoutUnit
{
    return WTF::switchOn(size,
        [&](const typename SizeType::Fixed& fixed) -> LayoutUnit {
            return LayoutUnit(fixed.value);
        },
        [&](const typename SizeType::Percentage& percentage) -> LayoutUnit {
            // Don't remove the extra cast to float. It is needed for rounding on 32-bit Intel machines that use the FPU stack.
            return LayoutUnit(static_cast<float>(referenceLength * percentage.value / 100.0f));
        },
        [&](const typename SizeType::Calc& calc) -> LayoutUnit {
            return Style::evaluate(calc, referenceLength);
        },
        [](const CSS::Keyword::Auto&) -> LayoutUnit {
            return 0;
        },
        [](const CSS::Keyword::None&) -> LayoutUnit {
            return 0;
        },
        [](const CSS::Keyword::Content&) -> LayoutUnit {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::MinContent&) -> LayoutUnit {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::MaxContent&) -> LayoutUnit {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::FitContent&) -> LayoutUnit {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::Intrinsic&) -> LayoutUnit {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::MinIntrinsic&) -> LayoutUnit {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const CSS::Keyword::WebkitFillAvailable&) -> LayoutUnit {
            return 0;
        }
    );
}


} // namespace Style
} // namespace WebCore
