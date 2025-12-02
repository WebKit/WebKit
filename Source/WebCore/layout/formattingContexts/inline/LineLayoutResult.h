/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/InlineLine.h>
#include <WebCore/InlineLineTypes.h>
#include <WebCore/LayoutUnits.h>
#include <WebCore/PlacedFloats.h>

namespace WebCore {
namespace Layout {

struct LineLayoutResult {
    using PlacedFloatList = PlacedFloats::List;
    using SuspendedFloatList = Vector<const Box*>;

    InlineItemRange inlineItemRange;
    Line::RunList runs;

    struct FloatContent {
        PlacedFloatList placedFloats;
        SuspendedFloatList suspendedFloats;
        OptionSet<UsedFloat> hasIntrusiveFloat { };
    };
    FloatContent floatContent { };

    struct ContentGeometry {
        InlineLayoutUnit logicalLeft { 0.f };
        InlineLayoutUnit logicalWidth { 0.f };
        InlineLayoutUnit logicalRightIncludingNegativeMargin { 0.f }; // Note that with negative horizontal margin value, contentLogicalLeft + contentLogicalWidth is not necessarily contentLogicalRight.
        std::optional<InlineLayoutUnit> trailingOverflowingContentWidth { };
    };
    ContentGeometry contentGeometry { };

    struct LineGeometry {
        InlineLayoutPoint logicalTopLeft;
        InlineLayoutUnit logicalWidth { 0.f };
        InlineLayoutUnit initialLogicalLeft { 0.f };
        InlineLayoutUnit intrusiveFloatsOffset { 0.f }; // Inherited floats from parent formatting context offseting line box.
        std::optional<InlineLayoutUnit> initialLetterClearGap { };
    };
    LineGeometry lineGeometry { };

    struct HangingContent {
        bool shouldContributeToScrollableOverflow { false };
        InlineLayoutUnit logicalWidth { 0.f };
        InlineLayoutUnit hangablePunctuationStartWidth { 0.f };
    };
    HangingContent hangingContent { };

    struct Directionality {
        Vector<int32_t> visualOrderList;
        TextDirection inlineBaseDirection { TextDirection::LTR };
    };
    Directionality directionality { };

    struct IsFirstLast {
        IsFirstFormattedLine isFirstFormattedLine { IsFirstFormattedLine::Yes };
        bool isLastLineWithInlineContent { true };
    };
    IsFirstLast isFirstLast { };

    struct Ruby {
        HashMap<const Box*, InlineLayoutUnit> baseAlignmentOffsetList { };
        InlineLayoutUnit annotationAlignmentOffset { 0.f };
    };
    Ruby ruby { };

    // Misc
    enum InlineContentEnding : uint8_t { Generic, Hyphen, LineBreak };
    std::optional<InlineContentEnding> inlineContentEnding { }; // No value means line does not have any inline content (either float, out-of-flow or block inside inline)

    enum class InflowContentType : uint8_t { Inline, Block };
    std::optional<InflowContentType> inflowContentType() const
    {
        if (inlineContentEnding.has_value())
            return InflowContentType::Inline;
        if (!runs.isEmpty() && runs.last().isBlock())
            return InflowContentType::Block;
        return { };
    }
    bool hasInlineContent() const { return inflowContentType().value_or(InflowContentType::Block) == InflowContentType::Inline; }
    bool hasBlockContent() const { return inflowContentType().value_or(InflowContentType::Inline) == InflowContentType::Block; }
    bool hasInflowContent() const { return inflowContentType().has_value(); }
    bool endsWithHyphen() const { return inlineContentEnding && *inlineContentEnding == InlineContentEnding::Hyphen; }
    bool endsWithLineBreak() const { return inlineContentEnding && *inlineContentEnding == InlineContentEnding::LineBreak; }

    size_t nonSpanningInlineLevelBoxCount { 0 };
    InlineLayoutUnit trimmedTrailingWhitespaceWidth { 0.f }; // only used for line-break: after-white-space currently
    InlineLayoutUnit firstLineStartTrim { 0.f }; // This is how much text-box-trim: start adjusts the first line box. We only need it to adjust the initial letter float position (which will not be needed once we drop the float behavior)
    std::optional<InlineLayoutUnit> hintForNextLineTopToAvoidIntrusiveFloat { }; // This is only used for cases when intrusive floats prevent any content placement at current vertical position.
};

}
}
