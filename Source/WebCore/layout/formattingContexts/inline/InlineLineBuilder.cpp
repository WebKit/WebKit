/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "InlineLineBuilder.h"

#include "CSSLineBoxContainValue.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingGeometry.h"
#include "InlineFormattingQuirks.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyleInlines.h"
#include "Shape.h"
#include "TextUtil.h"
#include "UnicodeBidi.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

struct LineContent {
    InlineItemRange range;
    size_t partialTrailingContentLength { 0 };
    std::optional<InlineLayoutUnit> overflowLogicalWidth { };
};

static inline StringBuilder toString(const Line::RunList& runs)
{
    // FIXME: We could try to reuse the content builder in InlineItemsBuilder if this turns out to be a perf bottleneck.
    StringBuilder lineContentBuilder;
    for (auto& run : runs) {
        if (!run.isText())
            continue;
        auto& textContent = run.textContent();
        lineContentBuilder.append(StringView(downcast<InlineTextBox>(run.layoutBox()).content()).substring(textContent->start, textContent->length));
    }
    return lineContentBuilder;
}

static inline Vector<int32_t> computedVisualOrder(const Line::RunList& lineRuns, Vector<int32_t>& visualOrderList)
{
    Vector<UBiDiLevel> runLevels;
    runLevels.reserveInitialCapacity(lineRuns.size());

    Vector<size_t> runIndexOffsetMap;
    runIndexOffsetMap.reserveInitialCapacity(lineRuns.size());
    auto hasOpaqueRun = false;
    for (size_t i = 0, accumulatedOffset = 0; i < lineRuns.size(); ++i) {
        if (lineRuns[i].bidiLevel() == InlineItem::opaqueBidiLevel) {
            ++accumulatedOffset;
            hasOpaqueRun = true;
            continue;
        }

        // bidiLevels are required to be less than the MAX + 1, otherwise
        // ubidi_reorderVisual will silently fail.
        if (lineRuns[i].bidiLevel() > UBIDI_MAX_EXPLICIT_LEVEL + 1) {
            ASSERT(lineRuns[i].bidiLevel() == UBIDI_DEFAULT_LTR);
            continue;
        }

        runLevels.uncheckedAppend(lineRuns[i].bidiLevel());
        runIndexOffsetMap.uncheckedAppend(accumulatedOffset);
    }

    visualOrderList.resizeToFit(runLevels.size());
    ubidi_reorderVisual(runLevels.data(), runLevels.size(), visualOrderList.data());
    if (hasOpaqueRun) {
        ASSERT(visualOrderList.size() == runIndexOffsetMap.size());
        for (size_t i = 0; i < runIndexOffsetMap.size(); ++i)
            visualOrderList[i] += runIndexOffsetMap[visualOrderList[i]];
    }
    return visualOrderList;
}

static bool isLastLineWithInlineContent(const InlineItemRange& lineRange, const InlineItems& inlineItems, size_t lastInlineItemIndex, bool hasPartialTrailingContent)
{
    if (hasPartialTrailingContent)
        return false;
    if (lineRange.endIndex() == lastInlineItemIndex) {
        // We must have only committed trailing (overconstraining) floats on the line when the range is empty.
        return !lineRange.isEmpty();
    }
    // Omit floats to see if this is the last line with inline content.
    for (auto i = lastInlineItemIndex; i--;) {
        if (!inlineItems[i].isFloat())
            return i == lineRange.endIndex() - 1;
    }
    // There has to be at least one non-float item.
    ASSERT_NOT_REACHED();
    return false;
}

static bool hasTrailingSoftWrapOpportunity(size_t softWrapOpportunityIndex, size_t layoutRangeEnd, const InlineItems& inlineItems)
{
    if (!softWrapOpportunityIndex || softWrapOpportunityIndex == layoutRangeEnd) {
        // This candidate inline content ends because the entire content ends and not because there's a soft wrap opportunity.
        return false;
    }
    // See https://www.w3.org/TR/css-text-3/#line-break-details
    auto& trailingInlineItem = inlineItems[softWrapOpportunityIndex - 1];
    if (trailingInlineItem.isFloat()) {
        // While we stop at floats, they are not considered real soft wrap opportunities.
        return false;
    }
    if (trailingInlineItem.isBox() || trailingInlineItem.isLineBreak() || trailingInlineItem.isWordBreakOpportunity() || trailingInlineItem.isInlineBoxEnd()) {
        // For Web-compatibility there is a soft wrap opportunity before and after each replaced element or other atomic inline.
        return true;
    }
    if (trailingInlineItem.isText()) {
        auto& inlineTextItem = downcast<InlineTextItem>(trailingInlineItem);
        if (inlineTextItem.isWhitespace())
            return true;
        // Now in case of non-whitespace trailing content, we need to check if the actual soft wrap opportunity belongs to the next set.
        // e.g. "this_is_the_trailing_run<span> <-but_this_space_here_is_the_soft_wrap_opportunity"
        // When there's an inline box start(<span>)/end(</span>) between the trailing and the (next)leading run, while we break before the inline box start (<span>)
        // the actual soft wrap position is after the inline box start (<span>) but in terms of line breaking continuity the inline box start (<span>) and the whitespace run belong together.
        RELEASE_ASSERT(layoutRangeEnd <= inlineItems.size());
        for (auto index = softWrapOpportunityIndex; index < layoutRangeEnd; ++index) {
            if (inlineItems[index].isInlineBoxStart() || inlineItems[index].isInlineBoxEnd() || inlineItems[index].isOpaque())
                continue;
            // FIXME: Check if [non-whitespace][inline-box][no-whitespace] content has rules about it.
            // For now let's say the soft wrap position belongs to the next set of runs when [non-whitespace][inline-box][whitespace], [non-whitespace][inline-box][box] etc.
            return inlineItems[index].isText() && !downcast<InlineTextItem>(inlineItems[index]).isWhitespace();
        }
        return true;
    }
    if (trailingInlineItem.isInlineBoxStart()) {
        // This is a special case when the inline box's fist child is a float box.
        return false;
    }
    if (trailingInlineItem.isOpaque()) {
        ASSERT(softWrapOpportunityIndex > 1); // Can't have opaque items only candidate content.
        return hasTrailingSoftWrapOpportunity(softWrapOpportunityIndex - 1, layoutRangeEnd, inlineItems);
    }
    ASSERT_NOT_REACHED();
    return true;
};

static TextDirection inlineBaseDirectionForLineContent(const Line::RunList& runs, const RenderStyle& rootStyle, std::optional<PreviousLine> previousLine)
{
    ASSERT(!runs.isEmpty());
    auto shouldUseBlockDirection = rootStyle.unicodeBidi() != UnicodeBidi::Plaintext;
    if (shouldUseBlockDirection)
        return rootStyle.direction();
    // A previous line ending with a line break (<br> or preserved \n) introduces a new unicode paragraph with its own direction.
    if (previousLine && !previousLine->endsWithLineBreak)
        return previousLine->inlineBaseDirection;
    return TextUtil::directionForTextContent(toString(runs));
}

struct LineCandidate {

    void reset();

    struct InlineContent {
        const InlineContentBreaker::ContinuousContent& continuousContent() const { return m_continuousContent; }
        const InlineItem* trailingLineBreak() const { return m_trailingLineBreak; }
        const InlineItem* trailingWordBreakOpportunity() const { return m_trailingWordBreakOpportunity; }

        void appendInlineItem(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
        void appendTrailingLineBreak(const InlineItem& lineBreakItem) { m_trailingLineBreak = &lineBreakItem; }
        void appendtrailingWordBreakOpportunity(const InlineItem& wordBreakItem) { m_trailingWordBreakOpportunity = &wordBreakItem; }
        void reset();
        bool isEmpty() const { return m_continuousContent.runs().isEmpty() && !trailingWordBreakOpportunity() && !trailingLineBreak(); }

        void setHasTrailingSoftWrapOpportunity(bool hasTrailingSoftWrapOpportunity) { m_hasTrailingSoftWrapOpportunity = hasTrailingSoftWrapOpportunity; }
        bool hasTrailingSoftWrapOpportunity() const { return m_hasTrailingSoftWrapOpportunity; }

        void setHangingContentWidth(InlineLayoutUnit logicalWidth) { m_continuousContent.setHangingContentWidth(logicalWidth); }

        void setAccumulatedClonedDecorationEnd(InlineLayoutUnit accumulatedWidth) { m_accumulatedClonedDecorationEnd = accumulatedWidth; }
        InlineLayoutUnit accumulatedClonedDecorationEnd() const { return m_accumulatedClonedDecorationEnd; }

    private:
        InlineContentBreaker::ContinuousContent m_continuousContent;
        const InlineItem* m_trailingLineBreak { nullptr };
        const InlineItem* m_trailingWordBreakOpportunity { nullptr };
        InlineLayoutUnit m_accumulatedClonedDecorationEnd { 0.f };
        bool m_hasTrailingSoftWrapOpportunity { false };
    };

    // Candidate content is a collection of inline content or a float box.
    InlineContent inlineContent;
    const InlineItem* floatItem { nullptr };
};

inline void LineCandidate::InlineContent::appendInlineItem(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    ASSERT(inlineItem.isText() || inlineItem.isBox() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd() || inlineItem.isOpaque());

    if (inlineItem.isBox() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd() || inlineItem.isOpaque())
        return m_continuousContent.append(inlineItem, style, logicalWidth);

    if (inlineItem.isText())
        return m_continuousContent.appendTextContent(downcast<InlineTextItem>(inlineItem), style, logicalWidth);

    ASSERT_NOT_REACHED();
}

inline void LineCandidate::InlineContent::reset()
{
    m_continuousContent.reset();
    m_trailingLineBreak = { };
    m_trailingWordBreakOpportunity = { };
    m_accumulatedClonedDecorationEnd = { };
}

inline void LineCandidate::reset()
{
    floatItem = nullptr;
    inlineContent.reset();
}


LineBuilder::LineBuilder(const InlineFormattingContext& inlineFormattingContext, const InlineLayoutState& inlineLayoutState, FloatingState& floatingState, HorizontalConstraints rootHorizontalConstraints, const InlineItems& inlineItems)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineLayoutState(inlineLayoutState)
    , m_floatingState(floatingState)
    , m_rootHorizontalConstraints(rootHorizontalConstraints)
    , m_line(inlineFormattingContext)
    , m_inlineItems(inlineItems)
{
}

LineLayoutResult LineBuilder::layoutInlineContent(const LineInput& lineInput, const std::optional<PreviousLine>& previousLine)
{
    auto previousLineEndsWithLineBreak = !previousLine ? std::nullopt : std::make_optional(previousLine->endsWithLineBreak);
    initialize(lineInput.initialLogicalRect, initialConstraintsForLine(lineInput.initialLogicalRect, previousLineEndsWithLineBreak), lineInput.needsLayoutRange, previousLine);
    auto lineContent = placeInlineAndFloatContent(lineInput.needsLayoutRange);
    auto result = m_line.close();

    if (isInIntrinsicWidthMode()) {
        return { lineContent.range
            , WTFMove(result.runs)
            , { WTFMove(m_placedFloats), WTFMove(m_suspendedFloats), { } }
            , { { }, result.contentLogicalWidth, { }, lineContent.overflowLogicalWidth }
            , { m_lineLogicalRect.topLeft(), { }, { }, { } }
        };
    }

    auto isLastLine = isLastLineWithInlineContent(lineContent.range, m_inlineItems, lineInput.needsLayoutRange.endIndex(), lineContent.partialTrailingContentLength);
    auto inlineBaseDirection = !result.runs.isEmpty() ? inlineBaseDirectionForLineContent(result.runs, rootStyle(), m_previousLine) : TextDirection::LTR;
    auto contentLogicalLeft = !result.runs.isEmpty() ? InlineFormattingGeometry::horizontalAlignmentOffset(rootStyle(), result.contentLogicalRight, m_lineLogicalRect.width(), result.hangingTrailingContentWidth, result.runs, isLastLine, inlineBaseDirection) : 0.f;
    Vector<int32_t> visualOrderList;
    if (result.contentNeedsBidiReordering)
        computedVisualOrder(result.runs, visualOrderList);

    return { lineContent.range
        , WTFMove(result.runs)
        , { WTFMove(m_placedFloats), WTFMove(m_suspendedFloats), m_lineIsConstrainedByFloat }
        , { contentLogicalLeft, result.contentLogicalWidth, contentLogicalLeft + result.contentLogicalRight, lineContent.overflowLogicalWidth }
        , { m_lineLogicalRect.topLeft(), m_lineLogicalRect.width(), m_lineInitialLogicalRect.left() + m_initialIntrusiveFloatsWidth, m_initialLetterClearGap }
        , { !result.isHangingTrailingContentWhitespace, result.hangingTrailingContentWidth }
        , { WTFMove(visualOrderList), inlineBaseDirection }
        , { isFirstFormattedLine() ? LineLayoutResult::IsFirstLast::FirstFormattedLine::WithinIFC : LineLayoutResult::IsFirstLast::FirstFormattedLine::No, isLastLine }
        , result.nonSpanningInlineLevelBoxCount
        , { }
        , lineContent.range.isEmpty() ? std::make_optional(m_lineLogicalRect.top() + m_candidateInlineContentEnclosingHeight) : std::nullopt
    };
}

void LineBuilder::initialize(const InlineRect& initialLineLogicalRect, const UsedConstraints& lineConstraints, const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>& previousLine)
{
    ASSERT(!needsLayoutRange.isEmpty() || (previousLine && !previousLine->suspendedFloats.isEmpty()));

    m_previousLine = previousLine;
    m_placedFloats.clear();
    m_suspendedFloats.clear();
    m_lineSpanningInlineBoxes.clear();
    m_wrapOpportunityList.clear();
    m_overflowingLogicalWidth = { };
    m_partialLeadingTextItem = { };
    m_initialLetterClearGap = { };
    m_candidateInlineContentEnclosingHeight = { };

    auto createLineSpanningInlineBoxes = [&] {
        auto isRootLayoutBox = [&](auto& elementBox) {
            return &elementBox == &root();
        };
        if (needsLayoutRange.isEmpty())
            return;
        // An inline box may not necessarily start on the current line:
        // <span>first line<br>second line<span>with some more embedding<br> forth line</span></span>
        // We need to make sure that there's an [InlineBoxStart] for every inline box that's present on the current line.
        // We only have to do it on the first run as any subsequent inline content is either at the same/higher nesting level.
        auto& firstInlineItem = m_inlineItems[needsLayoutRange.startIndex()];
        // Let's treat these spanning inline items as opaque bidi content. They should not change the bidi levels on adjacent content.
        auto bidiLevelForOpaqueInlineItem = InlineItem::opaqueBidiLevel;
        // If the parent is the formatting root, we can stop here. This is root inline box content, there's no nesting inline box from the previous line(s)
        // unless the inline box closing is forced over to the current line.
        // e.g.
        // <span>normally the inline box closing forms a continuous content</span>
        // <span>unless it's forced to the next line<br></span>
        auto firstInlineItemIsLineSpanning = firstInlineItem.isInlineBoxEnd();
        if (!firstInlineItemIsLineSpanning && isRootLayoutBox(firstInlineItem.layoutBox().parent()))
            return;
        Vector<const Box*> spanningLayoutBoxList;
        if (firstInlineItemIsLineSpanning)
            spanningLayoutBoxList.append(&firstInlineItem.layoutBox());
        auto* ancestor = &firstInlineItem.layoutBox().parent();
        while (!isRootLayoutBox(*ancestor)) {
            spanningLayoutBoxList.append(ancestor);
            ancestor = &ancestor->parent();
        }
        for (auto* spanningInlineBox : makeReversedRange(spanningLayoutBoxList))
            m_lineSpanningInlineBoxes.append({ *spanningInlineBox, InlineItem::Type::InlineBoxStart, bidiLevelForOpaqueInlineItem });
    };
    createLineSpanningInlineBoxes();
    m_line.initialize(m_lineSpanningInlineBoxes, isFirstFormattedLine());

    m_lineInitialLogicalRect = initialLineLogicalRect;
    m_lineMarginStart = lineConstraints.marginStart;
    m_lineLogicalRect = lineConstraints.logicalRect;
    // This is by how much intrusive floats (coming from parent/sibling FCs) initially offset the line.
    m_initialIntrusiveFloatsWidth = m_lineLogicalRect.left() - initialLineLogicalRect.left();
    m_lineLogicalRect.moveHorizontally(m_lineMarginStart);
    // While negative margins normally don't expand the available space, preferred width computation gets confused by negative text-indent
    // (shrink the space needed for the content) which we have to balance it here.
    m_lineLogicalRect.expandHorizontally(-m_lineMarginStart);
    m_lineIsConstrainedByFloat = lineConstraints.isConstrainedByFloat;

    auto initializeLeadingContentFromOverflow = [&] {
        if (!previousLine || !needsLayoutRange.start.offset)
            return;
        auto overflowingInlineItemPosition = needsLayoutRange.start;
        if (is<InlineTextItem>(m_inlineItems[overflowingInlineItemPosition.index])) {
            auto& overflowingInlineTextItem = downcast<InlineTextItem>(m_inlineItems[overflowingInlineItemPosition.index]);
            ASSERT(overflowingInlineItemPosition.offset < overflowingInlineTextItem.length());
            auto overflowingLength = overflowingInlineTextItem.length() - overflowingInlineItemPosition.offset;
            if (overflowingLength) {
                // Turn previous line's overflow content into the next line's leading content.
                // "sp[<-line break->]lit_content" -> break position: 2 -> leading partial content length: 11.
                m_partialLeadingTextItem = overflowingInlineTextItem.right(overflowingLength, previousLine->trailingOverflowingContentWidth);
                return;
            }
        }
        m_overflowingLogicalWidth = previousLine->trailingOverflowingContentWidth;
    };
    initializeLeadingContentFromOverflow();
}

LineContent LineBuilder::placeInlineAndFloatContent(const InlineItemRange& needsLayoutRange)
{
    size_t resumedFloatCount = 0;
    auto layoutPreviouslySuspendedFloats = [&] {
        if (!m_previousLine)
            return;
        // FIXME: Note that placedInlineItemCount is not incremented here as these floats are already accounted for (at previous line)
        // as LineContent only takes one range -meaning that inline layout may continue while float layout is being suspended
        // and the placed InlineItem range ends at the last inline item placed on the current line.
        resumedFloatCount = m_previousLine->suspendedFloats.size();
        for (auto* suspendedFloat : m_previousLine->suspendedFloats) {
            auto isPlaced = tryPlacingFloatBox(*suspendedFloat, MayOverConstrainLine::Yes);
            ASSERT_UNUSED(isPlaced, isPlaced);
        }
        m_previousLine->suspendedFloats.clear();
    };
    layoutPreviouslySuspendedFloats();

    auto lineContent = LineContent { };
    size_t placedInlineItemCount = 0;

    auto layoutInlineAndFloatContent = [&] {
        auto lineCandidate = LineCandidate { };

        auto currentItemIndex = needsLayoutRange.startIndex();
        while (currentItemIndex < needsLayoutRange.endIndex()) {
            // 1. Collect the set of runs that we can commit to the line as one entity e.g. <span>text_and_span_start_span_end</span>.
            // 2. Apply floats and shrink the available horizontal space e.g. <span>intru_<div style="float: left"></div>sive_float</span>.
            // 3. Check if the content fits the line and commit the content accordingly (full, partial or not commit at all).
            // 4. Return if we are at the end of the line either by not being able to fit more content or because of an explicit line break.
            candidateContentForLine(lineCandidate, currentItemIndex, needsLayoutRange, m_line.contentLogicalRight());
            // Now check if we can put this content on the current line.
            if (auto* floatItem = lineCandidate.floatItem) {
                ASSERT(lineCandidate.inlineContent.isEmpty());
                if (!tryPlacingFloatBox(floatItem->layoutBox(), m_line.runs().isEmpty() ? MayOverConstrainLine::Yes : MayOverConstrainLine::No)) {
                    // This float overconstrains the line (it simply means shrinking the line box by the float would cause inline content overflow.)
                    // At this point we suspend float layout but continue with inline layout.
                    // Such suspended float will be placed at the next available vertical positon when this line "closes".
                    m_suspendedFloats.append(&floatItem->layoutBox());
                }
                ++placedInlineItemCount;
            } else {
                auto result = handleInlineContent(needsLayoutRange, lineCandidate);
                auto isEndOfLine = result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes;
                if (!result.committedCount.isRevert) {
                    placedInlineItemCount += result.committedCount.value;
                    auto& inlineContent = lineCandidate.inlineContent;
                    auto inlineContentIsFullyPlaced = inlineContent.continuousContent().runs().size() == result.committedCount.value && !result.partialTrailingContentLength;
                    if (inlineContentIsFullyPlaced) {
                        if (auto* wordBreakOpportunity = inlineContent.trailingWordBreakOpportunity()) {
                            // <wbr> needs to be on the line as an empty run so that we can construct an inline box and compute basic geometry.
                            ++placedInlineItemCount;
                            m_line.append(*wordBreakOpportunity, wordBreakOpportunity->style(), { });
                        }
                        if (inlineContent.trailingLineBreak()) {
                            // Fully placed (or empty) content followed by a line break means "end of line".
                            // FIXME: This will put the line break box at the end of the line while in case of some inline boxes, the line break
                            // could very well be at an earlier position. This has no visual implications at this point though (only geometry correctness on the line break box).
                            // e.g. <span style="border-right: 10px solid green">text<br></span> where the <br>'s horizontal position is before the right border and not after.
                            auto& trailingLineBreak = *inlineContent.trailingLineBreak();
                            m_line.append(trailingLineBreak, trailingLineBreak.style(), { });
                            ++placedInlineItemCount;
                            isEndOfLine = true;
                        }
                    }
                } else
                    placedInlineItemCount = result.committedCount.value;

                if (isEndOfLine) {
                    lineContent.partialTrailingContentLength = result.partialTrailingContentLength;
                    lineContent.overflowLogicalWidth = result.overflowLogicalWidth;
                    return;
                }
            }
            currentItemIndex = needsLayoutRange.startIndex() + placedInlineItemCount;
        }
        // Looks like we've run out of content.
        ASSERT_UNUSED(resumedFloatCount, placedInlineItemCount || resumedFloatCount);
    };
    layoutInlineAndFloatContent();

    auto comutePlacedInlineItemRange = [&] {
        ASSERT(placedInlineItemCount || !m_placedFloats.isEmpty() || m_lineIsConstrainedByFloat);
        lineContent.range = { needsLayoutRange.start, { needsLayoutRange.startIndex() + placedInlineItemCount, { } } };
        if (!placedInlineItemCount || placedInlineItemCount == m_placedFloats.size() || !lineContent.partialTrailingContentLength)
            return;

        auto trailingInlineItemIndex = lineContent.range.end.index - 1;
        auto overflowingInlineTextItemLength = downcast<InlineTextItem>(m_inlineItems[trailingInlineItemIndex]).length();
        ASSERT(lineContent.partialTrailingContentLength && lineContent.partialTrailingContentLength < overflowingInlineTextItemLength);
        lineContent.range.end = { trailingInlineItemIndex, overflowingInlineTextItemLength - lineContent.partialTrailingContentLength };
    };
    comutePlacedInlineItemRange();

    ASSERT(lineContent.range.endIndex() <= needsLayoutRange.endIndex());

    auto handleLineEnding = [&] {
        auto isLastLine = isLastLineWithInlineContent(lineContent.range, m_inlineItems, needsLayoutRange.endIndex(), lineContent.partialTrailingContentLength);
        auto horizontalAvailableSpace = m_lineLogicalRect.width();
        auto& rootStyle = this->rootStyle();

        auto handleTrailingContent = [&] {
            auto& quirks = formattingContext().formattingQuirks();
            auto lineHasOverflow = [&] {
                return horizontalAvailableSpace < m_line.contentLogicalWidth();
            };
            auto isLineBreakAfterWhitespace = [&] {
                return rootStyle.lineBreak() == LineBreak::AfterWhiteSpace && intrinsicWidthMode() != IntrinsicWidthMode::Minimum && (!isLastLine || lineHasOverflow());
            };
            m_line.handleTrailingTrimmableContent(isLineBreakAfterWhitespace() ? Line::TrailingContentAction::Preserve : Line::TrailingContentAction::Remove);
            if (quirks.trailingNonBreakingSpaceNeedsAdjustment(isInIntrinsicWidthMode(), lineHasOverflow()))
                m_line.handleOverflowingNonBreakingSpace(isLineBreakAfterWhitespace() ? Line::TrailingContentAction::Preserve : Line::TrailingContentAction::Remove, m_line.contentLogicalWidth() - horizontalAvailableSpace);

            m_line.handleTrailingHangingContent(intrinsicWidthMode(), horizontalAvailableSpace, isLastLine);
        };
        handleTrailingContent();

        // On each line, reset the embedding level of any sequence of whitespace characters at the end of the line
        // to the paragraph embedding level
        m_line.resetBidiLevelForTrailingWhitespace(rootStyle.isLeftToRightDirection() ? UBIDI_LTR : UBIDI_RTL);

        auto runsExpandHorizontally = !isInIntrinsicWidthMode() && (isLastLine ? rootStyle.textAlignLast() == TextAlignLast::Justify : rootStyle.textAlign() == TextAlignMode::Justify);
        if (runsExpandHorizontally)
            m_line.applyRunExpansion(horizontalAvailableSpace);
        auto lineEndsWithHyphen = false;
        if (m_line.hasContent()) {
            auto& lastTextContent = m_line.runs().last().textContent();
            lineEndsWithHyphen = lastTextContent && lastTextContent->needsHyphen;
        }
        m_successiveHyphenatedLineCount = lineEndsWithHyphen ? m_successiveHyphenatedLineCount + 1 : 0;
    };
    handleLineEnding();

    return lineContent;
}

FloatingContext::Constraints LineBuilder::floatConstraints(const InlineRect& lineMarginBoxRect) const
{
    if (isInIntrinsicWidthMode() || floatingState().isEmpty())
        return { };

    return formattingContext().formattingGeometry().floatConstraintsForLine(lineMarginBoxRect.top(), lineMarginBoxRect.height(), FloatingContext { formattingContext(), floatingState() });
}

LineBuilder::UsedConstraints LineBuilder::initialConstraintsForLine(const InlineRect& initialLineLogicalRect, std::optional<bool> previousLineEndsWithLineBreak) const
{
    auto adjustedLineLogicalRect = initialLineLogicalRect;
    auto lineConstraints = floatConstraints(initialLineLogicalRect);
    if (lineConstraints.left)
        adjustedLineLogicalRect.shiftLeftTo(std::max<InlineLayoutUnit>(adjustedLineLogicalRect.left(), lineConstraints.left->x));
    if (lineConstraints.right)
        adjustedLineLogicalRect.setRight(std::max(adjustedLineLogicalRect.left(), std::min<InlineLayoutUnit>(adjustedLineLogicalRect.right(), lineConstraints.right->x)));

    auto isIntrinsicWidthMode = isInIntrinsicWidthMode() ? InlineFormattingGeometry::IsIntrinsicWidthMode::Yes : InlineFormattingGeometry::IsIntrinsicWidthMode::No;
    auto textIndent = formattingContext().formattingGeometry().computedTextIndent(isIntrinsicWidthMode, previousLineEndsWithLineBreak, initialLineLogicalRect.width());
    auto lineIsConstrainedByFloat = adjustedLineLogicalRect != initialLineLogicalRect;
    return UsedConstraints { adjustedLineLogicalRect, textIndent, lineIsConstrainedByFloat };
}

InlineLayoutUnit LineBuilder::leadingPunctuationWidthForLineCandiate(size_t firstInlineTextItemIndex, size_t candidateContentStartIndex) const
{
    auto isFirstLineFirstContent = isFirstFormattedLine() && !m_line.hasContent();
    if (!isFirstLineFirstContent)
        return { };

    auto& inlineTextItem = downcast<InlineTextItem>(m_inlineItems[firstInlineTextItemIndex]);
    auto& style = isFirstFormattedLine() ? inlineTextItem.firstLineStyle() : inlineTextItem.style();
    if (!TextUtil::hasHangablePunctuationStart(inlineTextItem, style))
        return { };

    if (firstInlineTextItemIndex) {
        // The text content is not the first in the candidate list. However it may be the first contentful one.
        for (size_t index = firstInlineTextItemIndex; index-- > candidateContentStartIndex;) {
            auto& inlineItem = m_inlineItems[index];
            ASSERT(!inlineItem.isText() && !inlineItem.isLineBreak() && !inlineItem.isWordBreakOpportunity());
            if (inlineItem.isFloat() || inlineItem.isOpaque())
                continue;
            auto isContentful = inlineItem.isBox()
                || (inlineItem.isInlineBoxStart() && formattingContext().geometryForBox(inlineItem.layoutBox()).marginBorderAndPaddingStart())
                || (inlineItem.isInlineBoxEnd() && formattingContext().geometryForBox(inlineItem.layoutBox()).marginBorderAndPaddingEnd());
            if (isContentful)
                return { };
        }
    }
    // This candidate leading content may have hanging punctuation start.
    return TextUtil::hangablePunctuationStartWidth(inlineTextItem, style);
}

InlineLayoutUnit LineBuilder::trailingPunctuationOrStopOrCommaWidthForLineCandiate(size_t lastInlineTextItemIndex, size_t layoutRangeEnd) const
{
    auto& inlineTextItem = downcast<InlineTextItem>(m_inlineItems[lastInlineTextItemIndex]);
    auto& style = isFirstFormattedLine() ? inlineTextItem.firstLineStyle() : inlineTextItem.style();

    if (TextUtil::hasHangableStopOrCommaEnd(inlineTextItem, style)) {
        // Stop or comma does apply to all lines not just the last formatted one.
        return TextUtil::hangableStopOrCommaEndWidth(inlineTextItem, style);
    }

    if (TextUtil::hasHangablePunctuationEnd(inlineTextItem, style)) {
        // FIXME: If this turns out to be problematic (finding out if this is the last formatted line that is), we
        // may have to fallback to a post-process setup, where after finishing laying out the content, we go back and re-layout
        // the last (2?) line(s) when there's trailing hanging punctuation.
        // For now let's probe the content all the way to layoutRangeEnd.
        for (auto index = lastInlineTextItemIndex + 1; index < layoutRangeEnd; ++index) {
            auto isContentfulInlineItem = [&] {
                auto& inlineItem = m_inlineItems[index];
                if (inlineItem.isFloat() || inlineItem.isOpaque())
                    return false;
                if (inlineItem.isText()) {
                    auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
                    if (inlineTextItem.isFullyTrimmable() || inlineTextItem.isEmpty() || inlineTextItem.isWordSeparator() || inlineTextItem.isZeroWidthSpaceSeparator() || inlineTextItem.isQuirkNonBreakingSpace())
                        return false;
                    return true;
                }
                return inlineItem.isBox()
                    || (inlineItem.isInlineBoxStart() && formattingContext().geometryForBox(inlineItem.layoutBox()).marginBorderAndPaddingStart())
                    || (inlineItem.isInlineBoxEnd() && formattingContext().geometryForBox(inlineItem.layoutBox()).marginBorderAndPaddingEnd());
            }();
            if (isContentfulInlineItem)
                return { };
        }
        return TextUtil::hangablePunctuationEndWidth(inlineTextItem, style);
    }

    return { };
}

void LineBuilder::candidateContentForLine(LineCandidate& lineCandidate, size_t currentInlineItemIndex, const InlineItemRange& layoutRange, InlineLayoutUnit currentLogicalRight)
{
    ASSERT(currentInlineItemIndex < layoutRange.endIndex());
    lineCandidate.reset();
    // 1. Simply add any overflow content from the previous line to the candidate content. It's always a text content.
    // 2. Find the next soft wrap position or explicit line break.
    // 3. Collect floats between the inline content.
    auto softWrapOpportunityIndex = formattingContext().formattingGeometry().nextWrapOpportunity(currentInlineItemIndex, layoutRange, m_inlineItems);
    // softWrapOpportunityIndex == layoutRange.end means we don't have any wrap opportunity in this content.
    ASSERT(softWrapOpportunityIndex <= layoutRange.endIndex());

    auto isLineStart = currentInlineItemIndex == layoutRange.startIndex();
    if (isLineStart && m_partialLeadingTextItem) {
        ASSERT(!m_overflowingLogicalWidth);
        // Handle leading partial content first (overflowing text from the previous line).
        auto itemWidth = formattingContext().formattingGeometry().inlineItemWidth(*m_partialLeadingTextItem, currentLogicalRight, isFirstFormattedLine());
        lineCandidate.inlineContent.appendInlineItem(*m_partialLeadingTextItem, m_partialLeadingTextItem->style(), itemWidth);
        currentLogicalRight += itemWidth;
        ++currentInlineItemIndex;
    }

    auto firstInlineTextItemIndex = std::optional<size_t> { };
    auto lastInlineTextItemIndex = std::optional<size_t> { };
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    HashSet<const Box*> inlineBoxListWithClonedDecorationEnd;
    auto accumulatedDecorationEndWidth = InlineLayoutUnit { 0.f };
#endif
    for (auto index = currentInlineItemIndex; index < softWrapOpportunityIndex; ++index) {
        auto& inlineItem = m_inlineItems[index];
        auto& style = isFirstFormattedLine() ? inlineItem.firstLineStyle() : inlineItem.style();

        if (inlineItem.isFloat()) {
            lineCandidate.floatItem = &inlineItem;
            // This is a soft wrap opportunity, must be the only item in the list.
            ASSERT(currentInlineItemIndex + 1 == softWrapOpportunityIndex);
            continue;
        }
        if (inlineItem.isText()) {
            auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
            auto logicalWidth = m_overflowingLogicalWidth ? *std::exchange(m_overflowingLogicalWidth, std::nullopt) : formattingContext().formattingGeometry().inlineItemWidth(inlineTextItem, currentLogicalRight, isFirstFormattedLine());
            lineCandidate.inlineContent.appendInlineItem(inlineTextItem, style, logicalWidth);
            // Word spacing does not make the run longer, but it produces an offset instead. See Line::appendTextContent as well.
            currentLogicalRight += logicalWidth + (inlineTextItem.isWordSeparator() ? style.fontCascade().wordSpacing() : 0.f);
            firstInlineTextItemIndex = firstInlineTextItemIndex.value_or(index);
            lastInlineTextItemIndex = index;
            continue;
        }
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            auto logicalWidth = formattingContext().formattingGeometry().inlineItemWidth(inlineItem, currentLogicalRight, isFirstFormattedLine());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
            if (style.boxDecorationBreak() == BoxDecorationBreak::Clone) {
                auto& layoutBox = inlineItem.layoutBox();
                if (inlineItem.isInlineBoxStart())
                    inlineBoxListWithClonedDecorationEnd.add(&layoutBox);
                else if (inlineBoxListWithClonedDecorationEnd.contains(&layoutBox))
                    accumulatedDecorationEndWidth += logicalWidth;
            }
#endif
            lineCandidate.inlineContent.appendInlineItem(inlineItem, style, logicalWidth);
            currentLogicalRight += logicalWidth;
            continue;
        }
        if (inlineItem.isBox()) {
            auto logicalWidth = formattingContext().formattingGeometry().inlineItemWidth(inlineItem, currentLogicalRight, isFirstFormattedLine());
            // FIXME: While the line breaking related properties for atomic level boxes do not depend on the line index (first line style) it'd be great to figure out the correct style to pass in.
            lineCandidate.inlineContent.appendInlineItem(inlineItem, inlineItem.layoutBox().parent().style(), logicalWidth);
            currentLogicalRight += logicalWidth;
            continue;
        }
        if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity()) {
            // Since both <br> and <wbr> are explicit word break opportunities they have to be trailing items in this candidate run list unless they are embedded in inline boxes.
            // e.g. <span><wbr></span>
#if ASSERT_ENABLED
            for (auto i = index + 1; i < softWrapOpportunityIndex; ++i)
                ASSERT(m_inlineItems[i].isInlineBoxEnd() || m_inlineItems[i].isOpaque());
#endif
            inlineItem.isLineBreak() ? lineCandidate.inlineContent.appendTrailingLineBreak(inlineItem) : lineCandidate.inlineContent.appendtrailingWordBreakOpportunity(inlineItem);
            continue;
        }
        if (inlineItem.isOpaque()) {
            lineCandidate.inlineContent.appendInlineItem(inlineItem, style, { });
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    lineCandidate.inlineContent.setAccumulatedClonedDecorationEnd(accumulatedDecorationEndWidth);

    auto setLeadingAndTrailingHangingPunctuation = [&] {
        auto hangingContentWidth = lineCandidate.inlineContent.continuousContent().hangingContentWidth();
        // Do not even try to check for trailing punctuation when the candidate content already has whitespace type of hanging content.
        if (!hangingContentWidth && lastInlineTextItemIndex)
            hangingContentWidth += trailingPunctuationOrStopOrCommaWidthForLineCandiate(*lastInlineTextItemIndex, layoutRange.endIndex());
        if (firstInlineTextItemIndex)
            hangingContentWidth += leadingPunctuationWidthForLineCandiate(*firstInlineTextItemIndex, currentInlineItemIndex);
        lineCandidate.inlineContent.setHangingContentWidth(hangingContentWidth);
    };
    setLeadingAndTrailingHangingPunctuation();

    lineCandidate.inlineContent.setHasTrailingSoftWrapOpportunity(hasTrailingSoftWrapOpportunity(softWrapOpportunityIndex, layoutRange.endIndex(), m_inlineItems));
}

static bool shouldDisableHyphenation(const RenderStyle& rootStyle, unsigned successiveHyphenatedLineCount)
{
    unsigned limitLines = rootStyle.hyphenationLimitLines() == RenderStyle::initialHyphenationLimitLines() ? std::numeric_limits<unsigned>::max() : rootStyle.hyphenationLimitLines();
    return successiveHyphenatedLineCount >= limitLines;
}

static inline InlineLayoutUnit availableWidth(const LineCandidate::InlineContent& candidateContent, const Line& line, InlineLayoutUnit lineWidth)
{
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
    // 1. Preferred width computation sums up floats while line breaker subtracts them.
    // 2. Available space is inherently a LayoutUnit based value (coming from block/flex etc layout) and it is the result of a floored float.
    // These can all lead to epsilon-scale differences.
    lineWidth += LayoutUnit::epsilon();
#endif
    auto availableWidth = lineWidth - line.contentLogicalRight();
    auto& inlineBoxListWithClonedDecorationEnd = line.inlineBoxListWithClonedDecorationEnd();
    // We may try to commit a inline box end here which already takes up place implicitly through the cloned decoration.
    // Let's not account for its logical width twice.
    if (inlineBoxListWithClonedDecorationEnd.isEmpty())
        return std::isnan(availableWidth) ? maxInlineLayoutUnit() : (availableWidth + candidateContent.accumulatedClonedDecorationEnd());
    for (auto& run : candidateContent.continuousContent().runs()) {
        if (!run.inlineItem.isInlineBoxEnd())
            continue;
        auto decorationEntry = inlineBoxListWithClonedDecorationEnd.find(&run.inlineItem.layoutBox());
        if (decorationEntry == inlineBoxListWithClonedDecorationEnd.end())
            continue;
        availableWidth += decorationEntry->value;
    }
    return std::isnan(availableWidth) ? maxInlineLayoutUnit() : (availableWidth + candidateContent.accumulatedClonedDecorationEnd());
}

static std::optional<InlineLayoutUnit> eligibleOverflowWidthAsLeading(const InlineContentBreaker::ContinuousContent::RunList& candidateRuns, const InlineContentBreaker::Result& lineBreakingResult, bool isFirstFormattedLine)
{
    auto eligibleTrailingRunIndex = [&]() -> std::optional<size_t> {
        ASSERT(lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap || lineBreakingResult.action == InlineContentBreaker::Result::Action::Break);
        if (candidateRuns.size() == 1 && candidateRuns.first().inlineItem.isText()) {
            // A single text run is always a candidate.
            return { 0 };
        }
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break && lineBreakingResult.partialTrailingContent) {
            auto& trailingRun = candidateRuns[lineBreakingResult.partialTrailingContent->trailingRunIndex];
            if (trailingRun.inlineItem.isText())
                return lineBreakingResult.partialTrailingContent->trailingRunIndex;
        }
        return { };
    }();

    if (!eligibleTrailingRunIndex)
        return { };

    auto& overflowingRun = candidateRuns[*eligibleTrailingRunIndex];
    // FIXME: Add support for other types of continuous content.
    ASSERT(is<InlineTextItem>(overflowingRun.inlineItem));
    auto& inlineTextItem = downcast<InlineTextItem>(overflowingRun.inlineItem);
    if (inlineTextItem.isWhitespace())
        return { };
    if (isFirstFormattedLine) {
        auto& usedStyle = overflowingRun.style;
        auto& style = overflowingRun.inlineItem.style();
        if (&usedStyle != &style && usedStyle.fontCascade() != style.fontCascade()) {
            // We may have the incorrect text width when styles differ. Just re-measure the text content when we place it on the next line.
            return { };
        }
    }
    auto logicalWidthForNextLineAsLeading = overflowingRun.logicalWidth;
    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap)
        return logicalWidthForNextLineAsLeading;
    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break && lineBreakingResult.partialTrailingContent->partialRun)
        return logicalWidthForNextLineAsLeading - lineBreakingResult.partialTrailingContent->partialRun->logicalWidth;
    return { };
}

std::tuple<InlineRect, bool> LineBuilder::adjustedLineRectWithCandidateInlineContent(const LineCandidate& lineCandidate) const
{
    // Check if the candidate content would stretch the line and whether additional floats are getting in the way.
    auto& inlineContent = lineCandidate.inlineContent;
    if (isInIntrinsicWidthMode())
        return { m_lineLogicalRect, false };
    auto maximumLineLogicalHeight = m_lineLogicalRect.height();
    // FIXME: Use InlineFormattingGeometry::inlineLevelBoxAffectsLineBox instead.
    auto lineBoxContain = formattingContext().root().style().lineBoxContain();
    for (auto& run : inlineContent.continuousContent().runs()) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isBox() && lineBoxContain.contains(LineBoxContain::Replaced))
            maximumLineLogicalHeight = std::max(maximumLineLogicalHeight, InlineLayoutUnit { formattingContext().geometryForBox(run.inlineItem.layoutBox()).marginBoxHeight() });
        else if (inlineItem.isText()) {
            auto& styleToUse = isFirstFormattedLine() ? inlineItem.firstLineStyle() : inlineItem.style();
            maximumLineLogicalHeight = std::max<InlineLayoutUnit>(maximumLineLogicalHeight, styleToUse.computedLineHeight());
        }
    }
    if (maximumLineLogicalHeight == m_lineLogicalRect.height())
        return { m_lineLogicalRect, false };

    auto adjustedLineMarginBoxRect = InlineRect { m_lineLogicalRect.top(),  m_lineLogicalRect.left() - m_lineMarginStart, m_lineLogicalRect.width() + m_lineMarginStart, maximumLineLogicalHeight };
    auto lineConstraints = floatConstraints(adjustedLineMarginBoxRect);
    if (lineConstraints.left)
        adjustedLineMarginBoxRect.shiftLeftTo(std::max<InlineLayoutUnit>(adjustedLineMarginBoxRect.left(), lineConstraints.left->x));
    if (lineConstraints.right)
        adjustedLineMarginBoxRect.setRight(std::max(adjustedLineMarginBoxRect.left(), std::min<InlineLayoutUnit>(adjustedLineMarginBoxRect.right(), lineConstraints.right->x)));

    auto adjustedLineRect = InlineRect { adjustedLineMarginBoxRect.top(), adjustedLineMarginBoxRect.left() + m_lineMarginStart, adjustedLineMarginBoxRect.width() - m_lineMarginStart, adjustedLineMarginBoxRect.height() };
    return { adjustedLineRect, lineConstraints.left || lineConstraints.right };
}

std::optional<LineBuilder::InitialLetterOffsets> LineBuilder::adjustLineRectForInitialLetterIfApplicable(const Box& floatBox)
{
    auto drop = floatBox.style().initialLetterDrop();
    auto isInitialLetter = floatBox.isFloatingPositioned() && floatBox.style().styleType() == PseudoId::FirstLetter && drop;
    if (!isInitialLetter)
        return { };

    // Here we try to set the vertical start position for the float in flush with the adjoining text content's cap height.
    // It's a super premature as at this point we don't normally deal with vertical geometry -other than the incoming vertical constraint.
    auto initialLetterCapHeightOffset = formattingContext().formattingQuirks().initialLetterAlignmentOffset(floatBox, rootStyle());
    // While initial-letter based floats do not set their clear property, intrusive floats from sibling IFCs are supposed to be cleared.
    auto intrusiveBottom = blockLayoutState().intrusiveInitialLetterLogicalBottom();
    if (!initialLetterCapHeightOffset && !intrusiveBottom)
        return { };

    auto clearGapBeforeFirstLine = InlineLayoutUnit { };
    if (intrusiveBottom) {
        // When intrusive initial letter is cleared, we introduce a clear gap. This is (with proper floats) normally computed before starting
        // line layout but intrusive initial letters are cleared only when another initial letter shows up. Regular inline content
        // does not need clearance.
        auto intrusiveInitialLetterWidth = std::max(0.f, m_lineLogicalRect.left() - m_lineInitialLogicalRect.left());
        m_lineLogicalRect.setLeft(m_lineInitialLogicalRect.left());
        m_lineLogicalRect.expandHorizontally(intrusiveInitialLetterWidth);
        clearGapBeforeFirstLine = *intrusiveBottom;
    }

    auto sunkenBelowFirstLineOffset = LayoutUnit { };
    auto letterHeight = floatBox.style().initialLetterHeight();
    if (drop < letterHeight) {
        // Sunken/raised initial letter pushes contents of the first line down.
        auto numberOfSunkenLines = letterHeight - drop;
        auto verticalGapForInlineContent = numberOfSunkenLines * rootStyle().computedLineHeight();
        clearGapBeforeFirstLine += verticalGapForInlineContent;
        // And we pull the initial letter up.
        initialLetterCapHeightOffset = -verticalGapForInlineContent + initialLetterCapHeightOffset.value_or(0_lu);
    } else if (drop > letterHeight) {
        // Initial letter is sunken below the first line.
        auto numberOfLinesAboveInitialLetter = drop - letterHeight;
        sunkenBelowFirstLineOffset = numberOfLinesAboveInitialLetter * rootStyle().computedLineHeight();
    }

    m_lineLogicalRect.moveVertically(clearGapBeforeFirstLine);
    // There should never be multiple initial letters.
    ASSERT(!m_initialLetterClearGap);
    m_initialLetterClearGap = clearGapBeforeFirstLine;
    return InitialLetterOffsets { initialLetterCapHeightOffset.value_or(0_lu), sunkenBelowFirstLineOffset };
}

bool LineBuilder::shouldTryToPlaceFloatBox(const Box& floatBox, LayoutUnit floatBoxMarginBoxWidth, MayOverConstrainLine mayOverConstrainLine) const
{
    if (mayOverConstrainLine == MayOverConstrainLine::Yes) {
        // This is a resumed float from a previous vertical position. Now we need to find a place for it.
        // (which also means that the current line can't have any floats that we couldn't place yet)
        ASSERT(m_suspendedFloats.isEmpty());
        return true;
    }
    auto lineIsConsideredEmpty = !m_line.hasContent() && !m_lineIsConstrainedByFloat;
    if (lineIsConsideredEmpty)
        return true;
    // Non-clear type of floats stack up (horizontally). It's easy to check if there's space for this float at all,
    // while floats with clear needs post-processing to see if they overlap existing line content (and here we just check if they may fit at all).
    auto lineLogicalWidth = floatBox.hasFloatClear() ? m_lineInitialLogicalRect.width() : m_lineLogicalRect.width();
    auto availableWidthForFloat = lineLogicalWidth - m_line.contentLogicalRight() + m_line.trimmableTrailingWidth();
    return availableWidthForFloat >= InlineLayoutUnit { floatBoxMarginBoxWidth };
}

static bool haveEnoughSpaceForFloatWithClear(const LayoutRect& floatBoxMarginBox, bool isLeftPositioned, const InlineRect& lineLogicalRect, InlineLayoutUnit contentLogicalWidth)
{
    auto adjustedLineLogicalLeft = lineLogicalRect.left();
    auto adjustedLineLogicalRight = lineLogicalRect.right();
    if (isLeftPositioned)
        adjustedLineLogicalLeft = std::max<InlineLayoutUnit>(floatBoxMarginBox.maxX(), adjustedLineLogicalLeft);
    else
        adjustedLineLogicalRight = std::min<InlineLayoutUnit>(floatBoxMarginBox.x(), adjustedLineLogicalRight);
    auto availableSpaceForContentWithPlacedFloat = adjustedLineLogicalRight - adjustedLineLogicalLeft;
    return contentLogicalWidth <= availableSpaceForContentWithPlacedFloat;
}

bool LineBuilder::tryPlacingFloatBox(const Box& floatBox, MayOverConstrainLine mayOverConstrainLine)
{
    if (isFloatLayoutSuspended())
        return false;

    auto boxGeometry = BoxGeometry { formattingContext().geometryForBox(floatBox) };
    if (!shouldTryToPlaceFloatBox(floatBox, boxGeometry.marginBoxWidth(), mayOverConstrainLine))
        return false;

    auto lineMarginBoxLeft = std::max(0.f, m_lineLogicalRect.left() - m_lineMarginStart);
    auto floatingContext = FloatingContext { formattingContext(), floatingState() };
    auto computeFloatBoxPosition = [&] {
        // Set static position first.
        auto staticPosition = LayoutPoint { lineMarginBoxLeft, m_lineLogicalRect.top() };
        if (auto additionalOffsets = adjustLineRectForInitialLetterIfApplicable(floatBox)) {
            staticPosition.setY(m_lineLogicalRect.top() + additionalOffsets->capHeightOffset);
            boxGeometry.setVerticalMargin({ boxGeometry.marginBefore() + additionalOffsets->sunkenBelowFirstLineOffset, boxGeometry.marginAfter() });
        }
        staticPosition.move(boxGeometry.marginStart(), boxGeometry.marginBefore());
        boxGeometry.setLogicalTopLeft(staticPosition);
        // Compute float position by running float layout.
        auto floatingPosition = floatingContext.positionForFloat(floatBox, boxGeometry, *m_rootHorizontalConstraints);
        boxGeometry.setLogicalTopLeft(floatingPosition);
    };
    computeFloatBoxPosition();

    auto willFloatBoxShrinkLine = [&] {
        // Float boxes don't get positioned higher than the line.
        auto floatBoxMarginBox = BoxGeometry::marginBoxRect(boxGeometry);
        if (floatBoxMarginBox.isEmpty())
            return false;
        if (floatBoxMarginBox.right() <= lineMarginBoxLeft) {
            // Previous floats already constrain the line horizontally more than this one.
            return false;
        }
        // Empty rect case: "line-height: 0px;" line still intersects with intrusive floats.
        return floatBoxMarginBox.top() == m_lineLogicalRect.top() || floatBoxMarginBox.top() < m_lineLogicalRect.bottom();
    }();

    auto willFloatBoxWithClearFit = [&] {
        if (!willFloatBoxShrinkLine)
            return true;
        auto lineIsConsideredEmpty = !m_line.hasContent() && !m_lineIsConstrainedByFloat;
        if (lineIsConsideredEmpty)
            return true;
        // When floats with clear are placed under existing floats, we may find ourselves in an over-constrained state and
        // can't place this float here.
        auto contentLogicalWidth = m_line.contentLogicalWidth() - m_line.trimmableTrailingWidth();
        return haveEnoughSpaceForFloatWithClear(BoxGeometry::marginBoxRect(boxGeometry), floatingContext.isLogicalLeftPositioned(floatBox), m_lineLogicalRect, contentLogicalWidth);
    };
    if (floatBox.hasFloatClear() && !willFloatBoxWithClearFit())
        return false;

    auto placeFloatBox = [&] {
        auto floatItem = floatingContext.toFloatItem(floatBox, boxGeometry);
        // FIXME: Maybe FloatingContext should be able to preserve FloatItems and the caller should mutate the FloatingState instead.
        floatingState().append(floatItem);
        m_placedFloats.append(floatItem);
    };
    placeFloatBox();

    auto adjustLineRectIfNeeded = [&] {
        if (!willFloatBoxShrinkLine) {
            // This float is placed outside the line box. No need to shrink the current line.
            return;
        }

        auto lineConstraints = floatConstraints(m_lineLogicalRect);

        auto adjustedRect = m_lineLogicalRect;
        if (lineConstraints.left)
            adjustedRect.shiftLeftTo(std::max<InlineLayoutUnit>(adjustedRect.left(), lineConstraints.left->x + m_lineMarginStart));
        if (lineConstraints.right)
            adjustedRect.setRight(std::max(adjustedRect.left(), std::min<InlineLayoutUnit>(adjustedRect.right(), lineConstraints.right->x)));

        m_lineIsConstrainedByFloat = m_lineIsConstrainedByFloat || adjustedRect != m_lineLogicalRect;
        m_lineLogicalRect = adjustedRect;
    };
    adjustLineRectIfNeeded();

    return true;
}

LineBuilder::Result LineBuilder::handleInlineContent(const InlineItemRange& layoutRange, const LineCandidate& lineCandidate)
{
    auto& inlineContent = lineCandidate.inlineContent;
    auto& continuousInlineContent = inlineContent.continuousContent();

    if (continuousInlineContent.runs().isEmpty()) {
        ASSERT(inlineContent.trailingLineBreak() || inlineContent.trailingWordBreakOpportunity());
        return { inlineContent.trailingLineBreak() ? InlineContentBreaker::IsEndOfLine::Yes : InlineContentBreaker::IsEndOfLine::No };
    }

    // While the floats are not considered to be on the line, they make the line contentful for line breaking.
    auto [lineRectAdjutedWithCandidateContent, candidateContentIsConstrainedByFloat] = adjustedLineRectWithCandidateInlineContent(lineCandidate);
    // Note that adjusted line height never shrinks.
    m_candidateInlineContentEnclosingHeight = lineRectAdjutedWithCandidateContent.height();
    auto toLineBuilderResult = [&](auto& lineBreakingResult) -> LineBuilder::Result {
        auto& candidateRuns = continuousInlineContent.runs();
    
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Keep) {
            // This continuous content can be fully placed on the current line.
            for (auto& run : candidateRuns)
                m_line.append(run.inlineItem, run.style, run.logicalWidth);
            // We are keeping this content on the line but we need to check if we could have wrapped here
            // in order to be able to revert back to this position if needed.
            // Let's just ignore cases like collapsed leading whitespace for now.
            if (lineCandidate.inlineContent.hasTrailingSoftWrapOpportunity() && m_line.hasContentOrListMarker()) {
                auto& trailingRun = candidateRuns.last();
                auto& trailingInlineItem = trailingRun.inlineItem;

                // Note that wrapping here could be driven both by the style of the parent and the inline item itself.
                // e.g inline boxes set the wrapping rules for their content and not for themselves.
                auto& layoutBoxParent = trailingInlineItem.layoutBox().parent();

                // Need to ensure we use the correct style here, so the content breaker and line builder remain in sync.
                auto& parentStyle = isFirstFormattedLine() ? layoutBoxParent.firstLineStyle() : layoutBoxParent.style();

                auto isWrapOpportunity = TextUtil::isWrappingAllowed(parentStyle);
                if (!isWrapOpportunity && (trailingInlineItem.isInlineBoxStart() || trailingInlineItem.isInlineBoxEnd()))
                    isWrapOpportunity = TextUtil::isWrappingAllowed(trailingRun.style);
                if (isWrapOpportunity)
                    m_wrapOpportunityList.append(&trailingInlineItem);
            }
            return { lineBreakingResult.isEndOfLine, { candidateRuns.size(), false } };
        }

        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap) {
            ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
            // This continuous content can't be placed on the current line. Nothing to commit at this time.
            // However there are cases when, due to whitespace collapsing, this overflowing content should not be separated from
            // the content on the line.
            // <div>X <span> X</span></div>
            // If the second 'X' overflows the line, the trailing whitespace gets trimmed which introduces a stray inline box
            // on the first line ('X <span>' and 'X</span>' first and second line respectively).
            // In such cases we need to revert the content on the line to a previous wrapping opportunity to keep such content together.
            auto needsRevert = m_line.trimmableTrailingWidth() && !m_line.runs().isEmpty() && m_line.runs().last().isInlineBoxStart();
            if (needsRevert && m_wrapOpportunityList.size() > 1) {
                m_wrapOpportunityList.removeLast();
                return { InlineContentBreaker::IsEndOfLine::Yes, { rebuildLineWithInlineContent(layoutRange, *m_wrapOpportunityList.last()), true } };
            }
            return { InlineContentBreaker::IsEndOfLine::Yes, { }, { }, eligibleOverflowWidthAsLeading(candidateRuns, lineBreakingResult, isFirstFormattedLine()) };
        }
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::WrapWithHyphen) {
            ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
            // This continuous content can't be placed on the current line, nothing to commit.
            // However we need to make sure that the current line gains a trailing hyphen.
            ASSERT(m_line.trailingSoftHyphenWidth());
            m_line.addTrailingHyphen(*m_line.trailingSoftHyphenWidth());
            return { InlineContentBreaker::IsEndOfLine::Yes };
        }
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::RevertToLastWrapOpportunity) {
            ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
            // Not only this content can't be placed on the current line, but we even need to revert the line back to an earlier position.
            ASSERT(!m_wrapOpportunityList.isEmpty());
            return { InlineContentBreaker::IsEndOfLine::Yes, { rebuildLineWithInlineContent(layoutRange, *m_wrapOpportunityList.last()), true } };
        }
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::RevertToLastNonOverflowingWrapOpportunity) {
            ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
            ASSERT(!m_wrapOpportunityList.isEmpty());
            if (auto committedCount = rebuildLineForTrailingSoftHyphen(layoutRange))
                return { InlineContentBreaker::IsEndOfLine::Yes, { committedCount, true } };
            return { InlineContentBreaker::IsEndOfLine::Yes };
        }
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break) {
            ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
            // Commit the combination of full and partial content on the current line.
            ASSERT(lineBreakingResult.partialTrailingContent);
            commitPartialContent(candidateRuns, *lineBreakingResult.partialTrailingContent);
            // When breaking multiple runs <span style="word-break: break-all">text</span><span>content</span>, we might end up breaking them at run boundary.
            // It simply means we don't really have a partial run. Partial content yes, but not partial run.
            auto trailingRunIndex = lineBreakingResult.partialTrailingContent->trailingRunIndex;
            auto committedInlineItemCount = trailingRunIndex + 1;
            if (!lineBreakingResult.partialTrailingContent->partialRun)
                return { InlineContentBreaker::IsEndOfLine::Yes, { committedInlineItemCount, false } };

            auto partialRun = *lineBreakingResult.partialTrailingContent->partialRun;
            auto& trailingInlineTextItem = downcast<InlineTextItem>(candidateRuns[trailingRunIndex].inlineItem);
            ASSERT(partialRun.length < trailingInlineTextItem.length());
            auto overflowLength = trailingInlineTextItem.length() - partialRun.length;
            return { InlineContentBreaker::IsEndOfLine::Yes, { committedInlineItemCount, false }, overflowLength, eligibleOverflowWidthAsLeading(candidateRuns, lineBreakingResult, isFirstFormattedLine()) };
        }
        ASSERT_NOT_REACHED();
        return { InlineContentBreaker::IsEndOfLine::No };
    };

    // If width constraint overrides exist, modify the available width accordingly.
    auto lineIndex = m_previousLine ? (m_previousLine->lineIndex + 1) : 0lu;
    const auto& availableLineWidthOverride = m_inlineLayoutState.availableLineWidthOverride();
    auto widthOverride = availableLineWidthOverride.availableLineWidthOverrideForLine(lineIndex);
    auto availableTotalWidthForContent = widthOverride ? InlineLayoutUnit { widthOverride.value() } - m_lineMarginStart : lineRectAdjutedWithCandidateContent.width();
    auto availableWidthForCandidateContent = availableWidth(inlineContent, m_line, availableTotalWidthForContent);

    auto lineBreakingResult = InlineContentBreaker::Result { InlineContentBreaker::Result::Action::Keep, InlineContentBreaker::IsEndOfLine::No, { }, { } };
    if (continuousInlineContent.logicalWidth() > availableWidthForCandidateContent) {
        auto lineIsConsideredContentful = m_line.hasContentOrListMarker() || m_lineIsConstrainedByFloat || candidateContentIsConstrainedByFloat;
        auto lineStatus = InlineContentBreaker::LineStatus {
            m_line.contentLogicalRight(),
            availableWidthForCandidateContent,
            m_line.trimmableTrailingWidth(),
            m_line.trailingSoftHyphenWidth(),
            m_line.isTrailingRunFullyTrimmable(),
            lineIsConsideredContentful,
            !m_wrapOpportunityList.isEmpty()
        };
        m_inlineContentBreaker.setHyphenationDisabled(shouldDisableHyphenation(root().style(), m_successiveHyphenatedLineCount));
        m_inlineContentBreaker.setIsMinimumInIntrinsicWidthMode(intrinsicWidthMode() == IntrinsicWidthMode::Minimum);
        lineBreakingResult = m_inlineContentBreaker.processInlineContent(continuousInlineContent, lineStatus);
    }
    auto lineGainsNewContent = lineBreakingResult.action == InlineContentBreaker::Result::Action::Keep || lineBreakingResult.action == InlineContentBreaker::Result::Action::Break; 
    if (lineGainsNewContent) {
        // Sometimes in order to put this content on the line, we have to avoid additional float boxes (when the new content is taller than any previous content and we have vertically stacked floats on this line)
        // which means we need to adjust the line rect to accommodate such new constraints.
        m_lineLogicalRect = lineRectAdjutedWithCandidateContent;
    }
    m_lineIsConstrainedByFloat = m_lineIsConstrainedByFloat || candidateContentIsConstrainedByFloat;
    return toLineBuilderResult(lineBreakingResult);
}

void LineBuilder::commitPartialContent(const InlineContentBreaker::ContinuousContent::RunList& runs, const InlineContentBreaker::Result::PartialTrailingContent& partialTrailingContent)
{
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& run = runs[index];
        if (partialTrailingContent.trailingRunIndex == index) {
            // Create and commit partial trailing item.
            if (auto partialRun = partialTrailingContent.partialRun) {
                ASSERT(run.inlineItem.isText());
                auto& trailingInlineTextItem = downcast<InlineTextItem>(runs[partialTrailingContent.trailingRunIndex].inlineItem);
                auto partialTrailingTextItem = trailingInlineTextItem.left(partialRun->length);
                m_line.append(partialTrailingTextItem, trailingInlineTextItem.style(), partialRun->logicalWidth);
                if (auto hyphenWidth = partialRun->hyphenWidth)
                    m_line.addTrailingHyphen(*hyphenWidth);
                return;
            }
            // The partial run is the last content to commit.
            m_line.append(run.inlineItem, run.style, run.logicalWidth);
            if (auto hyphenWidth = partialTrailingContent.hyphenWidth)
                m_line.addTrailingHyphen(*hyphenWidth);
            return;
        }
        m_line.append(run.inlineItem, run.style, run.logicalWidth);
    }
}

size_t LineBuilder::rebuildLineWithInlineContent(const InlineItemRange& layoutRange, const InlineItem& lastInlineItemToAdd)
{
    ASSERT(!m_wrapOpportunityList.isEmpty());
    size_t numberOfInlineItemsOnLine = 0;
    // FIXME: Remove floats that are outside of this "rebuild" range to ensure we don't add them twice.
    size_t numberOfFloatsInRange = 0;
    // We might already have added floats. They shrink the available horizontal space for the line.
    // Let's just reuse what the line has at this point.
    m_line.initialize(m_lineSpanningInlineBoxes, isFirstFormattedLine());
    if (m_partialLeadingTextItem) {
        m_line.append(*m_partialLeadingTextItem, m_partialLeadingTextItem->style(), formattingContext().formattingGeometry().inlineItemWidth(*m_partialLeadingTextItem, { }, isFirstFormattedLine()));
        ++numberOfInlineItemsOnLine;
        if (&m_partialLeadingTextItem.value() == &lastInlineItemToAdd)
            return 1;
    }
    for (size_t index = layoutRange.startIndex() + numberOfInlineItemsOnLine; index < layoutRange.endIndex(); ++index) {
        auto& inlineItem = m_inlineItems[index];
        if (inlineItem.isFloat()) {
            ++numberOfFloatsInRange;
            continue;
        }
        auto& style = isFirstFormattedLine() ? inlineItem.firstLineStyle() : inlineItem.style();
        m_line.append(inlineItem, style, formattingContext().formattingGeometry().inlineItemWidth(inlineItem, m_line.contentLogicalRight(), isFirstFormattedLine()));
        ++numberOfInlineItemsOnLine;
        if (&inlineItem == &lastInlineItemToAdd)
            break;
    }
    return numberOfInlineItemsOnLine + numberOfFloatsInRange;
}

size_t LineBuilder::rebuildLineForTrailingSoftHyphen(const InlineItemRange& layoutRange)
{
    if (m_wrapOpportunityList.isEmpty()) {
        // We are supposed to have a wrapping opportunity on the current line at this point.
        ASSERT_NOT_REACHED();
        return { };
    }
    // Revert all the way back to a wrap opportunity when either a soft hyphen fits or no hyphen is required.
    for (auto i = m_wrapOpportunityList.size(); i-- > 1;) {
        auto& softWrapOpportunityItem = *m_wrapOpportunityList[i];
        // FIXME: If this turns out to be a perf issue, we could also traverse the wrap list and keep adding the items
        // while watching the available width very closely.
        auto committedCount = rebuildLineWithInlineContent(layoutRange, softWrapOpportunityItem);
        auto availableWidth = m_lineLogicalRect.width() - m_line.contentLogicalRight();
        auto trailingSoftHyphenWidth = m_line.trailingSoftHyphenWidth();
        // Check if the trailing hyphen now fits the line (or we don't need hyphen anymore).
        if (!trailingSoftHyphenWidth || trailingSoftHyphenWidth <= availableWidth) {
            if (trailingSoftHyphenWidth)
                m_line.addTrailingHyphen(*trailingSoftHyphenWidth);
            return committedCount;
        }
    }
    // Have at least some content on the line.
    auto committedCount = rebuildLineWithInlineContent(layoutRange, *m_wrapOpportunityList.first());
    if (auto trailingSoftHyphenWidth = m_line.trailingSoftHyphenWidth())
        m_line.addTrailingHyphen(*trailingSoftHyphenWidth);
    return committedCount;
}

const ElementBox& LineBuilder::root() const
{
    return formattingContext().root();
}

const RenderStyle& LineBuilder::rootStyle() const
{
    return isFirstFormattedLine() ? root().firstLineStyle() : root().style();
}

}
}

