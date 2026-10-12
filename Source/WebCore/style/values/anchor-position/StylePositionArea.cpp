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
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle.h"
#include "StylePositionTryFallbackTactic.h"
#include "StyleSelfAlignmentData.h"
#include "WritingMode.h"

namespace WebCore {
namespace Style {

[[maybe_unused]] static bool NODELETE axisIsBlockOrX(PositionAreaAxis axis)
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

[[maybe_unused]] static bool NODELETE axisIsInlineOrY(PositionAreaAxis axis)
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

static ItemPosition NODELETE flip(ItemPosition alignment)
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

static std::optional<PositionAreaAxis> NODELETE positionAreaKeywordToAxis(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueID::Left:
    case CSSValueID::SpanLeft:
    case CSSValueID::Right:
    case CSSValueID::SpanRight:
        return PositionAreaAxis::Horizontal;

    case CSSValueID::Top:
    case CSSValueID::SpanTop:
    case CSSValueID::Bottom:
    case CSSValueID::SpanBottom:
        return PositionAreaAxis::Vertical;

    case CSSValueID::XStart:
    case CSSValueID::SpanXStart:
    case CSSValueID::SelfXStart:
    case CSSValueID::SpanSelfXStart:
    case CSSValueID::XEnd:
    case CSSValueID::SpanXEnd:
    case CSSValueID::SelfXEnd:
    case CSSValueID::SpanSelfXEnd:
        return PositionAreaAxis::X;

    case CSSValueID::YStart:
    case CSSValueID::SpanYStart:
    case CSSValueID::SelfYStart:
    case CSSValueID::SpanSelfYStart:
    case CSSValueID::YEnd:
    case CSSValueID::SpanYEnd:
    case CSSValueID::SelfYEnd:
    case CSSValueID::SpanSelfYEnd:
        return PositionAreaAxis::Y;

    case CSSValueID::BlockStart:
    case CSSValueID::SpanBlockStart:
    case CSSValueID::SelfBlockStart:
    case CSSValueID::SpanSelfBlockStart:
    case CSSValueID::BlockEnd:
    case CSSValueID::SpanBlockEnd:
    case CSSValueID::SelfBlockEnd:
    case CSSValueID::SpanSelfBlockEnd:
        return PositionAreaAxis::Block;

    case CSSValueID::InlineStart:
    case CSSValueID::SpanInlineStart:
    case CSSValueID::SelfInlineStart:
    case CSSValueID::SpanSelfInlineStart:
    case CSSValueID::InlineEnd:
    case CSSValueID::SpanInlineEnd:
    case CSSValueID::SelfInlineEnd:
    case CSSValueID::SpanSelfInlineEnd:
        return PositionAreaAxis::Inline;

    case CSSValueID::Start:
    case CSSValueID::SpanStart:
    case CSSValueID::SelfStart:
    case CSSValueID::SpanSelfStart:
    case CSSValueID::End:
    case CSSValueID::SpanEnd:
    case CSSValueID::SelfEnd:
    case CSSValueID::SpanSelfEnd:
    case CSSValueID::Center:
    case CSSValueID::SpanAll:
        return { };

    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

static PositionAreaTrack NODELETE positionAreaKeywordToTrack(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueID::Left:
    case CSSValueID::Top:
    case CSSValueID::XStart:
    case CSSValueID::SelfXStart:
    case CSSValueID::YStart:
    case CSSValueID::SelfYStart:
    case CSSValueID::BlockStart:
    case CSSValueID::SelfBlockStart:
    case CSSValueID::InlineStart:
    case CSSValueID::SelfInlineStart:
    case CSSValueID::Start:
    case CSSValueID::SelfStart:
        return PositionAreaTrack::Start;

    case CSSValueID::SpanLeft:
    case CSSValueID::SpanTop:
    case CSSValueID::SpanXStart:
    case CSSValueID::SpanSelfXStart:
    case CSSValueID::SpanYStart:
    case CSSValueID::SpanSelfYStart:
    case CSSValueID::SpanBlockStart:
    case CSSValueID::SpanSelfBlockStart:
    case CSSValueID::SpanInlineStart:
    case CSSValueID::SpanSelfInlineStart:
    case CSSValueID::SpanStart:
    case CSSValueID::SpanSelfStart:
        return PositionAreaTrack::SpanStart;

    case CSSValueID::Right:
    case CSSValueID::Bottom:
    case CSSValueID::XEnd:
    case CSSValueID::SelfXEnd:
    case CSSValueID::YEnd:
    case CSSValueID::SelfYEnd:
    case CSSValueID::BlockEnd:
    case CSSValueID::SelfBlockEnd:
    case CSSValueID::InlineEnd:
    case CSSValueID::SelfInlineEnd:
    case CSSValueID::End:
    case CSSValueID::SelfEnd:
        return PositionAreaTrack::End;

    case CSSValueID::SpanRight:
    case CSSValueID::SpanBottom:
    case CSSValueID::SpanXEnd:
    case CSSValueID::SpanSelfXEnd:
    case CSSValueID::SpanYEnd:
    case CSSValueID::SpanSelfYEnd:
    case CSSValueID::SpanBlockEnd:
    case CSSValueID::SpanSelfBlockEnd:
    case CSSValueID::SpanInlineEnd:
    case CSSValueID::SpanSelfInlineEnd:
    case CSSValueID::SpanEnd:
    case CSSValueID::SpanSelfEnd:
        return PositionAreaTrack::SpanEnd;

    case CSSValueID::Center:
        return PositionAreaTrack::Center;
    case CSSValueID::SpanAll:
        return PositionAreaTrack::SpanAll;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaTrack::Start;
    }
}

static PositionAreaSelf NODELETE positionAreaKeywordToSelf(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueID::Left:
    case CSSValueID::SpanLeft:
    case CSSValueID::Right:
    case CSSValueID::SpanRight:
    case CSSValueID::Top:
    case CSSValueID::SpanTop:
    case CSSValueID::Bottom:
    case CSSValueID::SpanBottom:
    case CSSValueID::XStart:
    case CSSValueID::SpanXStart:
    case CSSValueID::XEnd:
    case CSSValueID::SpanXEnd:
    case CSSValueID::YStart:
    case CSSValueID::SpanYStart:
    case CSSValueID::YEnd:
    case CSSValueID::SpanYEnd:
    case CSSValueID::BlockStart:
    case CSSValueID::SpanBlockStart:
    case CSSValueID::BlockEnd:
    case CSSValueID::SpanBlockEnd:
    case CSSValueID::InlineStart:
    case CSSValueID::SpanInlineStart:
    case CSSValueID::InlineEnd:
    case CSSValueID::SpanInlineEnd:
    case CSSValueID::Start:
    case CSSValueID::SpanStart:
    case CSSValueID::End:
    case CSSValueID::SpanEnd:
    case CSSValueID::Center:
    case CSSValueID::SpanAll:
        return PositionAreaSelf::No;

    case CSSValueID::SelfXStart:
    case CSSValueID::SpanSelfXStart:
    case CSSValueID::SelfXEnd:
    case CSSValueID::SpanSelfXEnd:
    case CSSValueID::SelfYStart:
    case CSSValueID::SpanSelfYStart:
    case CSSValueID::SelfYEnd:
    case CSSValueID::SpanSelfYEnd:
    case CSSValueID::SelfBlockStart:
    case CSSValueID::SpanSelfBlockStart:
    case CSSValueID::SelfBlockEnd:
    case CSSValueID::SpanSelfBlockEnd:
    case CSSValueID::SelfInlineStart:
    case CSSValueID::SpanSelfInlineStart:
    case CSSValueID::SelfInlineEnd:
    case CSSValueID::SpanSelfInlineEnd:
    case CSSValueID::SelfStart:
    case CSSValueID::SpanSelfStart:
    case CSSValueID::SelfEnd:
    case CSSValueID::SpanSelfEnd:
        return PositionAreaSelf::Yes;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaSelf::No;
    }
}

// Expand a one keyword position-area to the equivalent keyword pair value.
static std::pair<CSSValueID, CSSValueID> NODELETE positionAreaExpandKeyword(CSSValueID dim)
{
    auto maybeAxis = positionAreaKeywordToAxis(dim);
    if (maybeAxis) {
        // Keyword is axis unambiguous, second keyword is span-all.

        // Y/inline axis keyword goes after in the pair.
        auto axis = *maybeAxis;
        if (axis == PositionAreaAxis::Vertical || axis == PositionAreaAxis::Y || axis == PositionAreaAxis::Inline)
            return { CSSValueID::SpanAll, dim };

        return { dim, CSSValueID::SpanAll };
    }

    // Keyword is axis ambiguous, it's repeated.
    return { dim, dim };
}

// Flip a PositionAreaValue across a logical axis (block or inline), given the current writing mode.
static PositionAreaValue NODELETE flipPositionAreaByLogicalAxis(LogicalBoxAxis flipAxis, PositionAreaValue area, WritingMode writingMode)
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
static PositionAreaValue NODELETE flipPositionAreaByPhysicalAxis(BoxAxis flipAxis, PositionAreaValue area, WritingMode writingMode)
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
static PositionAreaValue NODELETE mirrorPositionAreaAcrossDiagonal(PositionAreaValue area)
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

    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        auto valueID = keywordValue->valueID();
        if (valueID == CSSValueID::None)
            return CSS::Keyword::None { };

        dimPair = positionAreaExpandKeyword(valueID);
    } else if (auto* pair = dynamicDowncast<CSSValuePair>(value)) {
        RefPtr first = dynamicDowncast<CSSKeywordValue>(pair->first());
        RefPtr second = dynamicDowncast<CSSKeywordValue>(pair->second());

        if (!first || !second) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }

        // The parsing logic guarantees the keyword pair is in the correct order
        // (horizontal/x/block axis before vertical/Y/inline axis)

        dimPair = { first->valueID(), second->valueID() };
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

static CSSValueID NODELETE keywordForPositionAreaSpan(PositionAreaSpan span)
{
    auto axis = span.axis();
    auto track = span.track();
    auto self = span.self();

    switch (axis) {
    case PositionAreaAxis::Horizontal:
        ASSERT(self == PositionAreaSelf::No);
        switch (track) {
        case PositionAreaTrack::Start:
            return CSSValueID::Left;
        case PositionAreaTrack::SpanStart:
            return CSSValueID::SpanLeft;
        case PositionAreaTrack::End:
            return CSSValueID::Right;
        case PositionAreaTrack::SpanEnd:
            return CSSValueID::SpanRight;
        case PositionAreaTrack::Center:
            return CSSValueID::Center;
        case PositionAreaTrack::SpanAll:
            return CSSValueID::SpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueID::Left;
        }

    case PositionAreaAxis::Vertical:
        ASSERT(self == PositionAreaSelf::No);
        switch (track) {
        case PositionAreaTrack::Start:
            return CSSValueID::Top;
        case PositionAreaTrack::SpanStart:
            return CSSValueID::SpanTop;
        case PositionAreaTrack::End:
            return CSSValueID::Bottom;
        case PositionAreaTrack::SpanEnd:
            return CSSValueID::SpanBottom;
        case PositionAreaTrack::Center:
            return CSSValueID::Center;
        case PositionAreaTrack::SpanAll:
            return CSSValueID::SpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueID::Top;
        }

    case PositionAreaAxis::X:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueID::XStart : CSSValueID::SelfXStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueID::SpanXStart : CSSValueID::SpanSelfXStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueID::XEnd : CSSValueID::SelfXEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueID::SpanXEnd : CSSValueID::SpanSelfXEnd;
        case PositionAreaTrack::Center:
            return CSSValueID::Center;
        case PositionAreaTrack::SpanAll:
            return CSSValueID::SpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueID::XStart;
        }

    case PositionAreaAxis::Y:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueID::YStart : CSSValueID::SelfYStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueID::SpanYStart : CSSValueID::SpanSelfYStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueID::YEnd : CSSValueID::SelfYEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueID::SpanYEnd : CSSValueID::SpanSelfYEnd;
        case PositionAreaTrack::Center:
            return CSSValueID::Center;
        case PositionAreaTrack::SpanAll:
            return CSSValueID::SpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueID::YStart;
        }

    case PositionAreaAxis::Block:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueID::BlockStart : CSSValueID::SelfBlockStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueID::SpanBlockStart : CSSValueID::SpanSelfBlockStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueID::BlockEnd : CSSValueID::SelfBlockEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueID::SpanBlockEnd : CSSValueID::SpanSelfBlockEnd;
        case PositionAreaTrack::Center:
            return CSSValueID::Center;
        case PositionAreaTrack::SpanAll:
            return CSSValueID::SpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueID::BlockStart;
        }

    case PositionAreaAxis::Inline:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueID::InlineStart : CSSValueID::SelfInlineStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueID::SpanInlineStart : CSSValueID::SpanSelfInlineStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueID::InlineEnd : CSSValueID::SelfInlineEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueID::SpanInlineEnd : CSSValueID::SpanSelfInlineEnd;
        case PositionAreaTrack::Center:
            return CSSValueID::Center;
        case PositionAreaTrack::SpanAll:
            return CSSValueID::SpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueID::InlineStart;
        }
    }

    ASSERT_NOT_REACHED();
    return CSSValueID::Left;
}

Ref<CSSValue> CSSValueCreation<PositionAreaValue>::operator()(CSSValuePool&, const Style::ComputedStyle&, const PositionAreaValue& value)
{
    auto blockOrXAxisKeyword = keywordForPositionAreaSpan(value.blockOrXAxis());
    auto inlineOrYAxisKeyword = keywordForPositionAreaSpan(value.inlineOrYAxis());

    return CSSPropertyParserHelpers::valueForPositionArea(blockOrXAxisKeyword, inlineOrYAxisKeyword, CSSPropertyParserHelpers::ValueType::Computed).releaseNonNull();
}

// MARK: - Serialization

void Serialize<PositionAreaValue>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const Style::ComputedStyle&, const PositionAreaValue& value)
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
