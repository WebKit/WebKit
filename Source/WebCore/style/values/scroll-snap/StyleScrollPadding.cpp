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

#include "config.h"
#include "StyleScrollPadding.h"

#include "LayoutRect.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

// MARK: - Evaluation

auto Evaluation<ScrollPaddingEdge, LayoutUnit>::operator()(const ScrollPaddingEdge& edge, LayoutUnit referenceLength, ZoomFactor zoom) -> LayoutUnit
{
    return WTF::switchOn(edge,
        [&](const ScrollPaddingEdge::Fixed& fixed) {
            return evaluate<LayoutUnit>(fixed, zoom);
        },
        [&](const ScrollPaddingEdge::Percentage& percentage) {
            return evaluate<LayoutUnit>(percentage, referenceLength);
        },
        [&](const ScrollPaddingEdge::Calc& calculated) {
            return evaluate<LayoutUnit>(calculated, referenceLength, zoom);
        },
        [&](const CSS::Keyword::Auto&) {
            return 0_lu;
        }
    );
}

auto Evaluation<ScrollPaddingEdge, float>::operator()(const ScrollPaddingEdge& edge, float referenceLength, ZoomFactor zoom) -> float
{
    return WTF::switchOn(edge,
        [&](const ScrollPaddingEdge::Fixed& fixed) {
            return evaluate<float>(fixed, zoom);
        },
        [&](const ScrollPaddingEdge::Percentage& percentage) {
            return evaluate<float>(percentage, referenceLength);
        },
        [&](const ScrollPaddingEdge::Calc& calculated) {
            return evaluate<float>(calculated, referenceLength, zoom);
        },
        [&](const CSS::Keyword::Auto&) {
            return 0.0f;
        }
    );
}

LayoutBoxExtent extentForRect(const ScrollPaddingBox& padding, const LayoutRect& rect, ZoomFactor zoom)
{
    return LayoutBoxExtent {
        evaluate<LayoutUnit>(padding.top(), rect.height(), zoom),
        evaluate<LayoutUnit>(padding.right(), rect.width(), zoom),
        evaluate<LayoutUnit>(padding.bottom(), rect.height(), zoom),
        evaluate<LayoutUnit>(padding.left(), rect.width(), zoom),
    };
}

} // namespace Style
} // namespace WebCore
