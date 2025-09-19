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

LayoutUnit Evaluation<ScrollPaddingEdge>::operator()(const ScrollPaddingEdge& edge, LayoutUnit referenceLength, float zoom)
{
    return WTF::switchOn(edge,
        [&](const ScrollPaddingEdge::Fixed& fixed) {
            return LayoutUnit(fixed.value);
        },
        [&](const ScrollPaddingEdge::Percentage& percentage) {
            return Style::evaluate(percentage, referenceLength, zoom /* FIXME ZOOM EFFECTED? */);
        },
        [&](const ScrollPaddingEdge::Calc& calculated) {
            return Style::evaluate(calculated, referenceLength, zoom /* FIXME FIND ZOOM */);
        },
        [&](const CSS::Keyword::Auto&) {
            return 0_lu;
        }
    );
}

float Evaluation<ScrollPaddingEdge>::operator()(const ScrollPaddingEdge& edge, float referenceLength, float zoom)
{
    return WTF::switchOn(edge,
        [&](const ScrollPaddingEdge::Fixed& fixed) {
            return fixed.value;
        },
        [&](const ScrollPaddingEdge::Percentage& percentage) {
            return Style::evaluate(percentage, referenceLength, zoom /* FIXME ZOOM EFFECTED? */);
        },
        [&](const ScrollPaddingEdge::Calc& calculated) {
            return Style::evaluate(calculated, referenceLength, zoom /* FIXME ZOOM EFFECTED? */);
        },
        [&](const CSS::Keyword::Auto&) {
            return 0.0f;
        }
    );
}

LayoutBoxExtent extentForRect(const ScrollPaddingBox& padding, const LayoutRect& rect, float zoom)
{
    return LayoutBoxExtent {
        Style::evaluate(padding.top(), rect.height(), zoom),
        Style::evaluate(padding.right(), rect.width(), zoom),
        Style::evaluate(padding.bottom(), rect.height(), zoom),
        Style::evaluate(padding.left(), rect.width(), zoom),
    };
}

} // namespace Style
} // namespace WebCore
