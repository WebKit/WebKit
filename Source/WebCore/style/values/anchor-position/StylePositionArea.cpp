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
#include "StylePositionArea.h"

#include "BoxSides.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"
#include "StylePositionTryFallbackTactic.h"
#include "StyleSelfAlignmentData.h"
#include "WritingMode.h"

namespace WebCore {
namespace Style {

[[maybe_unused]] static bool axisIsBlockOrX(PositionAreaAxis axis)
{
    switch (axis) {
    case PositionAreaAxis::Horizontal:
    case PositionAreaAxis::X:
    case PositionAreaAxis::Block:
        return true;

    default:
        return false;
    }
}

[[maybe_unused]] static bool axisIsInlineOrY(PositionAreaAxis axis)
{
    switch (axis) {
    case PositionAreaAxis::Vertical:
    case PositionAreaAxis::Y:
    case PositionAreaAxis::Inline:
        return true;

    default:
        return false;
    }
}

PositionAreaValue::PositionAreaValue(PositionAreaSpan blockOrXAxis, PositionAreaSpan inlineOrYAxis)
    : m_blockOrXAxis(blockOrXAxis)
    , m_inlineOrYAxis(inlineOrYAxis)
{
    ASSERT(axisIsBlockOrX(m_blockOrXAxis.axis()));
    ASSERT(axisIsInlineOrY(m_inlineOrYAxis.axis()));
}

PositionAreaSpan PositionAreaValue::spanForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    bool useSelfWritingMode = m_blockOrXAxis.self() == PositionAreaSelf::Yes;
    auto writingMode = useSelfWritingMode ? selfWritingMode : containerWritingMode;
    return physicalAxis == mapPositionAreaAxisToPhysicalAxis(m_blockOrXAxis.axis(), writingMode)
        ? m_blockOrXAxis : m_inlineOrYAxis;
}

PositionAreaSpan PositionAreaValue::spanForAxis(LogicalBoxAxis logicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    bool useSelfWritingMode = m_blockOrXAxis.self() == PositionAreaSelf::Yes;
    auto writingMode = useSelfWritingMode ? selfWritingMode : containerWritingMode;
    return logicalAxis == mapPositionAreaAxisToLogicalAxis(m_blockOrXAxis.axis(), writingMode)
        ? m_blockOrXAxis : m_inlineOrYAxis;
}

PositionAreaTrack PositionAreaValue::coordMatchedTrackForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    auto relevantSpan = spanForAxis(physicalAxis, containerWritingMode, selfWritingMode);
    auto positionAxis = relevantSpan.axis();
    auto track = relevantSpan.track();

    bool shouldFlip = false;
    if (LogicalBoxAxis::Inline == mapAxisPhysicalToLogical(containerWritingMode, physicalAxis)) {
        if (isPositionAreaDirectionLogical(positionAxis)) {
            shouldFlip = containerWritingMode.isInlineFlipped();
            if (relevantSpan.self() == PositionAreaSelf::Yes
                && !containerWritingMode.isInlineMatchingAny(selfWritingMode))
                shouldFlip = !shouldFlip;
        }
    } else {
        shouldFlip = !isPositionAreaDirectionLogical(positionAxis)
            && containerWritingMode.isBlockFlipped();
        if (relevantSpan.self() == PositionAreaSelf::Yes
            && !containerWritingMode.isBlockMatchingAny(selfWritingMode))
            shouldFlip = !shouldFlip;
    }

    return shouldFlip ? flipPositionAreaTrack(track) : track;
}

static ItemPosition flip(ItemPosition alignment)
{
    return ItemPosition::Start == alignment ? ItemPosition::End : ItemPosition::Start;
};

ItemPosition PositionAreaValue::defaultAlignmentForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    auto relevantSpan = spanForAxis(physicalAxis, containerWritingMode, selfWritingMode);

    ItemPosition alignment;
    switch (relevantSpan.track()) {
    case PositionAreaTrack::Start:
    case PositionAreaTrack::SpanStart:
        alignment = ItemPosition::End;
        break;
    case PositionAreaTrack::End:
    case PositionAreaTrack::SpanEnd:
        alignment = ItemPosition::Start;
        break;
    case PositionAreaTrack::Center:
        return ItemPosition::Center;
    case PositionAreaTrack::SpanAll:
        return ItemPosition::AnchorCenter;
    }

    // Remap for self alignment.
    auto axis = relevantSpan.axis();
    bool shouldFlip = false;
    if (relevantSpan.self() == PositionAreaSelf::Yes && containerWritingMode != selfWritingMode) {
        auto logicalAxis = mapPositionAreaAxisToLogicalAxis(axis, selfWritingMode);
        if (containerWritingMode.isOrthogonal(selfWritingMode)) {
            if (LogicalBoxAxis::Inline == logicalAxis)
                shouldFlip = !selfWritingMode.isInlineMatchingAny(containerWritingMode);
            else
                shouldFlip = !selfWritingMode.isBlockMatchingAny(containerWritingMode);
        } else if (LogicalBoxAxis::Inline == logicalAxis)
            shouldFlip = selfWritingMode.isInlineOpposing(containerWritingMode);
        else
            shouldFlip = selfWritingMode.isBlockOpposing(containerWritingMode);
    }

    if (isPositionAreaDirectionLogical(axis))
        return shouldFlip ? flip(alignment) : alignment;

    ASSERT(PositionAreaAxis::Horizontal == axis || PositionAreaAxis::Vertical == axis);

    if ((PositionAreaAxis::Horizontal == axis) == containerWritingMode.isHorizontal())
        return containerWritingMode.isInlineFlipped() ? flip(alignment) : alignment;
    return containerWritingMode.isBlockFlipped() ? flip(alignment) : alignment;
}

// MARK: - Conversion

static std::optional<PositionAreaAxis> positionAreaKeywordToAxis(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueSpanLeft:
    case CSSValueRight:
    case CSSValueSpanRight:
        return PositionAreaAxis::Horizontal;

    case CSSValueTop:
    case CSSValueSpanTop:
    case CSSValueBottom:
    case CSSValueSpanBottom:
        return PositionAreaAxis::Vertical;

    case CSSValueXStart:
    case CSSValueSpanXStart:
    case CSSValueSelfXStart:
    case CSSValueSpanSelfXStart:
    case CSSValueXEnd:
    case CSSValueSpanXEnd:
    case CSSValueSelfXEnd:
    case CSSValueSpanSelfXEnd:
        return PositionAreaAxis::X;

    case CSSValueYStart:
    case CSSValueSpanYStart:
    case CSSValueSelfYStart:
    case CSSValueSpanSelfYStart:
    case CSSValueYEnd:
    case CSSValueSpanYEnd:
    case CSSValueSelfYEnd:
    case CSSValueSpanSelfYEnd:
        return PositionAreaAxis::Y;

    case CSSValueBlockStart:
    case CSSValueSpanBlockStart:
    case CSSValueSelfBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueBlockEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueSelfBlockEnd:
    case CSSValueSpanSelfBlockEnd:
        return PositionAreaAxis::Block;

    case CSSValueInlineStart:
    case CSSValueSpanInlineStart:
    case CSSValueSelfInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueInlineEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueSelfInlineEnd:
    case CSSValueSpanSelfInlineEnd:
        return PositionAreaAxis::Inline;

    case CSSValueStart:
    case CSSValueSpanStart:
    case CSSValueSelfStart:
    case CSSValueSpanSelfStart:
    case CSSValueEnd:
    case CSSValueSpanEnd:
    case CSSValueSelfEnd:
    case CSSValueSpanSelfEnd:
    case CSSValueCenter:
    case CSSValueSpanAll:
        return { };

    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

static PositionAreaTrack positionAreaKeywordToTrack(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueTop:
    case CSSValueXStart:
    case CSSValueSelfXStart:
    case CSSValueYStart:
    case CSSValueSelfYStart:
    case CSSValueBlockStart:
    case CSSValueSelfBlockStart:
    case CSSValueInlineStart:
    case CSSValueSelfInlineStart:
    case CSSValueStart:
    case CSSValueSelfStart:
        return PositionAreaTrack::Start;

    case CSSValueSpanLeft:
    case CSSValueSpanTop:
    case CSSValueSpanXStart:
    case CSSValueSpanSelfXStart:
    case CSSValueSpanYStart:
    case CSSValueSpanSelfYStart:
    case CSSValueSpanBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueSpanInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueSpanStart:
    case CSSValueSpanSelfStart:
        return PositionAreaTrack::SpanStart;

    case CSSValueRight:
    case CSSValueBottom:
    case CSSValueXEnd:
    case CSSValueSelfXEnd:
    case CSSValueYEnd:
    case CSSValueSelfYEnd:
    case CSSValueBlockEnd:
    case CSSValueSelfBlockEnd:
    case CSSValueInlineEnd:
    case CSSValueSelfInlineEnd:
    case CSSValueEnd:
    case CSSValueSelfEnd:
        return PositionAreaTrack::End;

    case CSSValueSpanRight:
    case CSSValueSpanBottom:
    case CSSValueSpanXEnd:
    case CSSValueSpanSelfXEnd:
    case CSSValueSpanYEnd:
    case CSSValueSpanSelfYEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueSpanSelfBlockEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueSpanSelfInlineEnd:
    case CSSValueSpanEnd:
    case CSSValueSpanSelfEnd:
        return PositionAreaTrack::SpanEnd;

    case CSSValueCenter:
        return PositionAreaTrack::Center;
    case CSSValueSpanAll:
        return PositionAreaTrack::SpanAll;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaTrack::Start;
    }
}

static PositionAreaSelf positionAreaKeywordToSelf(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueSpanLeft:
    case CSSValueRight:
    case CSSValueSpanRight:
    case CSSValueTop:
    case CSSValueSpanTop:
    case CSSValueBottom:
    case CSSValueSpanBottom:
    case CSSValueXStart:
    case CSSValueSpanXStart:
    case CSSValueXEnd:
    case CSSValueSpanXEnd:
    case CSSValueYStart:
    case CSSValueSpanYStart:
    case CSSValueYEnd:
    case CSSValueSpanYEnd:
    case CSSValueBlockStart:
    case CSSValueSpanBlockStart:
    case CSSValueBlockEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueInlineStart:
    case CSSValueSpanInlineStart:
    case CSSValueInlineEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueStart:
    case CSSValueSpanStart:
    case CSSValueEnd:
    case CSSValueSpanEnd:
    case CSSValueCenter:
    case CSSValueSpanAll:
        return PositionAreaSelf::No;

    case CSSValueSelfXStart:
    case CSSValueSpanSelfXStart:
    case CSSValueSelfXEnd:
    case CSSValueSpanSelfXEnd:
    case CSSValueSelfYStart:
    case CSSValueSpanSelfYStart:
    case CSSValueSelfYEnd:
    case CSSValueSpanSelfYEnd:
    case CSSValueSelfBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueSelfBlockEnd:
    case CSSValueSpanSelfBlockEnd:
    case CSSValueSelfInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueSelfInlineEnd:
    case CSSValueSpanSelfInlineEnd:
    case CSSValueSelfStart:
    case CSSValueSpanSelfStart:
    case CSSValueSelfEnd:
    case CSSValueSpanSelfEnd:
        return PositionAreaSelf::Yes;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaSelf::No;
    }
}

// Expand a one keyword position-area to the equivalent keyword pair value.
static std::pair<CSSValueID, CSSValueID> positionAreaExpandKeyword(CSSValueID dim)
{
    auto maybeAxis = positionAreaKeywordToAxis(dim);
    if (maybeAxis) {
        // Keyword is axis unambiguous, second keyword is span-all.

        // Y/inline axis keyword goes after in the pair.
        auto axis = *maybeAxis;
        if (axis == PositionAreaAxis::Vertical || axis == PositionAreaAxis::Y || axis == PositionAreaAxis::Inline)
            return { CSSValueSpanAll, dim };

        return { dim, CSSValueSpanAll };
    }

    // Keyword is axis ambiguous, it's repeated.
    return { dim, dim };
}

// Flip a PositionAreaValue across a logical axis (block or inline), given the current writing mode.
static PositionAreaValue flipPositionAreaByLogicalAxis(LogicalBoxAxis flipAxis, PositionAreaValue area, WritingMode writingMode)
{
    auto blockOrXSpan = area.blockOrXAxis();
    auto inlineOrYSpan = area.inlineOrYAxis();

    // blockOrXSpan is on the flip axis, so flip its track and keep inlineOrYSpan intact.
    if (mapPositionAreaAxisToLogicalAxis(blockOrXSpan.axis(), writingMode) == flipAxis) {
        return {
            { blockOrXSpan.axis(), flipPositionAreaTrack(blockOrXSpan.track()), blockOrXSpan.self() },
            inlineOrYSpan
        };
    }

    // The two spans are orthogonal in axis, so if blockOrXSpan isn't on the flip axis,
    // inlineOrYSpan must be. In this case, flip the track of inlineOrYSpan, and
    // keep blockOrXSpan intact.
    return {
        blockOrXSpan,
        { inlineOrYSpan.axis(), flipPositionAreaTrack(inlineOrYSpan.track()), inlineOrYSpan.self() }
    };
}

// Flip a PositionAreaValue across a physical axis (x or y), given the current writing mode.
static PositionAreaValue flipPositionAreaByPhysicalAxis(BoxAxis flipAxis, PositionAreaValue area, WritingMode writingMode)
{
    auto blockOrXSpan = area.blockOrXAxis();
    auto inlineOrYSpan = area.inlineOrYAxis();

    // blockOrXSpan is on the flip axis, so flip its track and keep inlineOrYSpan intact.
    if (mapPositionAreaAxisToPhysicalAxis(blockOrXSpan.axis(), writingMode) == flipAxis) {
        return {
            { blockOrXSpan.axis(), flipPositionAreaTrack(blockOrXSpan.track()), blockOrXSpan.self() },
            inlineOrYSpan
        };
    }

    // The two spans are orthogonal in axis, so if blockOrXSpan isn't on the flip axis,
    // inlineOrYSpan must be. In this case, flip the track of inlineOrYSpan, and
    // keep blockOrXSpan intact.
    return {
        blockOrXSpan,
        { inlineOrYSpan.axis(), flipPositionAreaTrack(inlineOrYSpan.track()), inlineOrYSpan.self() }
    };
}

// Flip a PositionAreaValue as specified by flip-start tactic.
// Intuitively, this mirrors the PositionAreaValue across a diagonal line drawn from the
// block-start/inline-start corner to the block-end/inline-end corner. This is done
// by flipping the axes of the spans in the PositionAreaValue, while keeping their track
// and self properties intact. Because this turns a block/X span into an inline/Y
// span and vice versa, this function also swaps the order of the spans, so
// that the block/X span goes before the inline/Y span.
static PositionAreaValue mirrorPositionAreaAcrossDiagonal(PositionAreaValue area)
{
    auto blockOrXSpan = area.blockOrXAxis();
    auto inlineOrYSpan = area.inlineOrYAxis();

    return {
        { oppositePositionAreaAxis(inlineOrYSpan.axis()), inlineOrYSpan.track(), inlineOrYSpan.self() },
        { oppositePositionAreaAxis(blockOrXSpan.axis()), blockOrXSpan.track(), blockOrXSpan.self() }
    };
}

auto CSSValueConversion<PositionArea>::operator()(BuilderState& state, const CSSValue& value) -> PositionArea
{
    std::pair<CSSValueID, CSSValueID> dimPair;

    if (value.isValueID()) {
        if (value.valueID() == CSSValueNone)
            return CSS::Keyword::None { };

        dimPair = positionAreaExpandKeyword(value.valueID());
    } else if (RefPtr pair = dynamicDowncast<CSSValuePair>(value)) {
        const auto& first = pair->first();
        const auto& second = pair->second();

        if (!first.isValueID() || !second.isValueID()) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }

        // The parsing logic guarantees the keyword pair is in the correct order
        // (horizontal/x/block axis before vertical/Y/inline axis)

        dimPair = { first.valueID(), second.valueID() };
    } else {
        // value MUST be a single ValueID or a pair of ValueIDs, as returned by the parsing logic.
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    auto dim1Axis = positionAreaKeywordToAxis(dimPair.first);
    auto dim2Axis = positionAreaKeywordToAxis(dimPair.second);

    // If both keyword axes are ambiguous, the first one is block axis and second one
    // is inline axis. If only one keyword axis is ambiguous, its axis is the opposite
    // of the other keyword's axis.
    if (!dim1Axis && !dim2Axis) {
        dim1Axis = PositionAreaAxis::Block;
        dim2Axis = PositionAreaAxis::Inline;
    } else if (!dim1Axis)
        dim1Axis = oppositePositionAreaAxis(*dim2Axis);
    else if (!dim2Axis)
        dim2Axis = oppositePositionAreaAxis(*dim1Axis);

    PositionAreaValue area {
        { *dim1Axis, positionAreaKeywordToTrack(dimPair.first), positionAreaKeywordToSelf(dimPair.first) },
        { *dim2Axis, positionAreaKeywordToTrack(dimPair.second), positionAreaKeywordToSelf(dimPair.second) }
    };

    // Flip according to `position-try-fallbacks`, if specified.
    if (const auto& positionTryFallback = state.positionTryFallback()) {
        auto writingMode = state.style().writingMode();
        for (auto tactic : positionTryFallback->tactics) {
            switch (tactic) {
            case PositionTryFallbackTactic::FlipBlock:
                area = flipPositionAreaByLogicalAxis(LogicalBoxAxis::Block, area, writingMode);
                break;
            case PositionTryFallbackTactic::FlipInline:
                area = flipPositionAreaByLogicalAxis(LogicalBoxAxis::Inline, area, writingMode);
                break;
            case PositionTryFallbackTactic::FlipX:
                area = flipPositionAreaByPhysicalAxis(BoxAxis::Horizontal, area, writingMode);
                break;
            case PositionTryFallbackTactic::FlipY:
                area = flipPositionAreaByPhysicalAxis(BoxAxis::Vertical, area, writingMode);
                break;
            case PositionTryFallbackTactic::FlipStart:
                area = mirrorPositionAreaAcrossDiagonal(area);
                break;
            }
        }
    }

    return area;
}

static CSSValueID keywordForPositionAreaSpan(PositionAreaSpan span)
{
    auto axis = span.axis();
    auto track = span.track();
    auto self = span.self();

    switch (axis) {
    case PositionAreaAxis::Horizontal:
        ASSERT(self == PositionAreaSelf::No);
        switch (track) {
        case PositionAreaTrack::Start:
            return CSSValueLeft;
        case PositionAreaTrack::SpanStart:
            return CSSValueSpanLeft;
        case PositionAreaTrack::End:
            return CSSValueRight;
        case PositionAreaTrack::SpanEnd:
            return CSSValueSpanRight;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueLeft;
        }

    case PositionAreaAxis::Vertical:
        ASSERT(self == PositionAreaSelf::No);
        switch (track) {
        case PositionAreaTrack::Start:
            return CSSValueTop;
        case PositionAreaTrack::SpanStart:
            return CSSValueSpanTop;
        case PositionAreaTrack::End:
            return CSSValueBottom;
        case PositionAreaTrack::SpanEnd:
            return CSSValueSpanBottom;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueTop;
        }

    case PositionAreaAxis::X:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueXStart : CSSValueSelfXStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanXStart : CSSValueSpanSelfXStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueXEnd : CSSValueSelfXEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanXEnd : CSSValueSpanSelfXEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueXStart;
        }

    case PositionAreaAxis::Y:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueYStart : CSSValueSelfYStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanYStart : CSSValueSpanSelfYStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueYEnd : CSSValueSelfYEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanYEnd : CSSValueSpanSelfYEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueYStart;
        }

    case PositionAreaAxis::Block:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueBlockStart : CSSValueSelfBlockStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanBlockStart : CSSValueSpanSelfBlockStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueBlockEnd : CSSValueSelfBlockEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanBlockEnd : CSSValueSpanSelfBlockEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueBlockStart;
        }

    case PositionAreaAxis::Inline:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueInlineStart : CSSValueSelfInlineStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanInlineStart : CSSValueSpanSelfInlineStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueInlineEnd : CSSValueSelfInlineEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanInlineEnd : CSSValueSpanSelfInlineEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueInlineStart;
        }
    }

    ASSERT_NOT_REACHED();
    return CSSValueLeft;
}

Ref<CSSValue> CSSValueCreation<PositionAreaValue>::operator()(CSSValuePool&, const RenderStyle&, const PositionAreaValue& value)
{
    auto blockOrXAxisKeyword = keywordForPositionAreaSpan(value.blockOrXAxis());
    auto inlineOrYAxisKeyword = keywordForPositionAreaSpan(value.inlineOrYAxis());

    return CSSPropertyParserHelpers::valueForPositionArea(blockOrXAxisKeyword, inlineOrYAxisKeyword, CSSPropertyParserHelpers::ValueType::Computed).releaseNonNull();
}

// MARK: - Serialization

void Serialize<PositionAreaValue>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle&, const PositionAreaValue& value)
{
    auto blockOrXAxisKeyword = keywordForPositionAreaSpan(value.blockOrXAxis());
    auto inlineOrYAxisKeyword = keywordForPositionAreaSpan(value.inlineOrYAxis());

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(CSSPropertyParserHelpers::valueForPositionArea(blockOrXAxisKeyword, inlineOrYAxisKeyword, CSSPropertyParserHelpers::ValueType::Computed)->cssText(context));
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, PositionAreaValue value)
{
    return ts << "{ span1: "_s << value.blockOrXAxis() << ", span2: "_s << value.inlineOrYAxis() << " }"_s;
}

} // namespace Style
} // namespace WebCore
