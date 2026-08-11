/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TimelineRangeValue.h"

#include "CSSPropertyParserConsumer+Timeline.h"
#include "Document.h"
#include "StylePrimitiveNumericTypes+TypedCSSOMConversions.h"
#include "StyleSingleAnimationRange.h"
#include "TimelineRangeOffset.h"

namespace WebCore {

static std::optional<Style::SingleAnimationRangeEdgeOffset> convertToOffset(Ref<CSSNumericValue>&& numericValue)
{
    return Style::toAbsoluteStyleFromCSSNumericValue<Style::SingleAnimationRangeEdgeOffset>(WTF::move(numericValue));
}

template<typename RangeEdge>
static std::optional<RangeEdge> convertToRangeEdge(TimelineRangeOffset&& rangeOffset)
{
    if (!rangeOffset.rangeName.isNull()) {
        auto rangeName = CSSPropertyParserHelpers::parseTimelineRangeNameRaw(rangeOffset.rangeName);
        if (!rangeName)
            return std::nullopt;
        if (RefPtr offset = WTF::move(rangeOffset.offset)) {
            auto rangeOffset = convertToOffset(offset.releaseNonNull());
            if (!rangeOffset)
                return std::nullopt;
            return RangeEdge { *rangeName, WTF::move(*rangeOffset) };
        }
        return RangeEdge { *rangeName };
    }
    if (RefPtr offset = WTF::move(rangeOffset.offset)) {
        auto rangeOffset = convertToOffset(offset.releaseNonNull());
        if (!rangeOffset)
            return std::nullopt;
        return RangeEdge { WTF::move(*rangeOffset) };
    }
    return std::nullopt;
}

template<typename RangeEdge>
static std::optional<RangeEdge> convertToRangeEdge(Ref<CSSOMKeywordValue>&& rangeKeyword)
{
    auto rangeName = CSSPropertyParserHelpers::parseTimelineRangeNameOrNormalRaw(rangeKeyword->value());
    if (!rangeName)
        return std::nullopt;
    return RangeEdge { *rangeName };
}

template<typename RangeEdge>
static std::optional<RangeEdge> convertToRangeEdge(Ref<CSSNumericValue>&& rangeValue)
{
    auto rangeOffset = convertToOffset(WTF::move(rangeValue));
    if (!rangeOffset)
        return std::nullopt;
    return RangeEdge { WTF::move(*rangeOffset) };
}

std::optional<Style::SingleAnimationRangeStart> validateTimelineRangeStart(TimelineRangeValue&& value, const Document& document)
{
    return WTF::switchOn(WTF::move(value),
        [&](String&& rangeString) -> std::optional<Style::SingleAnimationRangeStart> {
            return CSSPropertyParserHelpers::parseAbsoluteSingleAnimationRangeStartRaw(rangeString, document.cssParserContext(), document);
        },
        [](auto&& value) -> std::optional<Style::SingleAnimationRangeStart> {
            return convertToRangeEdge<Style::SingleAnimationRangeStart>(WTF::move(value));
        }
    );
}

std::optional<Style::SingleAnimationRangeEnd> validateTimelineRangeEnd(TimelineRangeValue&& value, const Document& document)
{
    return WTF::switchOn(WTF::move(value),
        [&](String&& rangeString) -> std::optional<Style::SingleAnimationRangeEnd> {
            return CSSPropertyParserHelpers::parseAbsoluteSingleAnimationRangeEndRaw(rangeString, document.cssParserContext(), document);
        },
        [](auto&& value) -> std::optional<Style::SingleAnimationRangeEnd> {
            return convertToRangeEdge<Style::SingleAnimationRangeEnd>(WTF::move(value));
        }
    );
}

} // namespace WebCore
