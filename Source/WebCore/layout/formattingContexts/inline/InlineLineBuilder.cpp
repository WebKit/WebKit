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

#include "ComplexTextController.h"
#include "InlineContentAligner.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingUtils.h"
#include "InlineQuirks.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutShape.h"
#include "RenderStyleInlines.h"
#include "RubyFormattingContext.h"
#include "StyleWebKitLineBoxContain.h"
#include "TextUtil.h"
#include "UnicodeBidi.h"
#include <ranges>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

struct LineContent {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(LineContent);

    InlineItemRange range;
    size_t partialTrailingContentLength { 0 };
    std::optional<InlineLayoutUnit> overflowLogicalWidth { };
    HashMap<const Box*, InlineLayoutUnit> rubyBaseAlignmentOffsetList { };
    InlineLayoutUnit rubyAnnotationOffset { 0.f };
    enum class LineBreakReason : uint8_t {
        ForcedLineBreakByBlockContent,
        Other
    };
    LineBreakReason lineBreakReason { LineBreakReason::Other };
};

static bool isContentfulOrHasDecoration(const InlineItem& inlineItem, const InlineFormattingContext& formattingContext)
{
    if (inlineItem.isFloat() || inlineItem.isOpaque())
        return false;
    if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem)) {
        auto wouldProduceEmptyRun = inlineTextItem->isFullyTrimmable() || inlineTextItem->isEmpty() || inlineTextItem->isWordSeparator() || inlineTextItem->isZeroWidthSpaceSeparator() || inlineTextItem->isQuirkNonBreakingSpace();
        return !wouldProduceEmptyRun;
    }

    if (inlineItem.isInlineBoxStart())
        return !!formattingContext.geometryForBox(inlineItem.layoutBox()).marginBorderAndPaddingStart();
    if (inlineItem.isInlineBoxEnd())
        return !!formattingContext.geometryForBox(inlineItem.layoutBox()).marginBorderAndPaddingEnd();
    return inlineItem.isAtomicInlineBox() || inlineItem.isLineBreak();
}

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
    size_t numberOfOpaqueRuns = 0;
    for (size_t i = 0, accumulatedOffset = 0; i < lineRuns.size(); ++i) {
        if (lineRuns[i].bidiLevel() == InlineItem::opaqueBidiLevel) {
            ++accumulatedOffset;
            ++numberOfOpaqueRuns;
            continue;
        }

        // bidiLevels are required to be less than the MAX + 1, otherwise
        // ubidi_reorderVisual will silently fail.
        if (lineRuns[i].bidiLevel() > UBIDI_MAX_EXPLICIT_LEVEL + 1) {
            ASSERT(lineRuns[i].bidiLevel() == UBIDI_DEFAULT_LTR);
            continue;
        }

        runLevels.append(lineRuns[i].bidiLevel());
        runIndexOffsetMap.append(accumulatedOffset);
    }

    auto forceBiDiOnOpaqueLine = [&] {
        if (lineRuns.isEmpty() || numberOfOpaqueRuns != lineRuns.size())
            return;
        // When an RTL line has only opaque items (e.g. [spanning inline box start][inline box end] on <span><div></div></span>)
        // we need to set the bidi level on the spanning inline box as if it was contentful to initiate bidi processing (mainly just RTL direction align).
        if (!lineRuns.first().isLineSpanningInlineBoxStart())
            return;
        runLevels.append(lineRuns.first().layoutBox().parent().writingMode().isBidiLTR() ? UBIDI_LTR : UBIDI_RTL);
        runIndexOffsetMap.append(0);
    };
    forceBiDiOnOpaqueLine();

    visualOrderList.resizeToFit(runLevels.size());
    ubidi_reorderVisual(runLevels.span().data(), runLevels.size(), visualOrderList.mutableSpan().data());
    if (numberOfOpaqueRuns) {
        ASSERT(visualOrderList.size() == runIndexOffsetMap.size());
        for (size_t i = 0; i < runIndexOffsetMap.size(); ++i)
            visualOrderList[i] += runIndexOffsetMap[visualOrderList[i]];
    }
    return visualOrderList;
}

static bool hasTrailingSoftWrapOpportunity(size_t softWrapOpportunityIndex, size_t layoutRangeEnd, std::span<const InlineItem> inlineItemList)
{
    if (!softWrapOpportunityIndex || softWrapOpportunityIndex == layoutRangeEnd) {
        // This candidate inline content ends because the entire content ends and not because there's a soft wrap opportunity.
        return false;
    }
    // See https://www.w3.org/TR/css-text-3/#line-break-details
    auto& trailingInlineItem = inlineItemList[softWrapOpportunityIndex - 1];
    if (trailingInlineItem.isFloat()) {
        // While we stop at floats, they are not considered real soft wrap opportunities.
        return false;
    }
    if (trailingInlineItem.isAtomicInlineBox() || trailingInlineItem.isLineBreak() || trailingInlineItem.isWordBreakOpportunity() || trailingInlineItem.isInlineBoxEnd()) {
        // For Web-compatibility there is a soft wrap opportunity before and after each replaced element or other atomic inline.
        return true;
    }
    if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(trailingInlineItem)) {
        if (inlineTextItem->isWhitespace())
            return true;
        // Now in case of non-whitespace trailing content, we need to check if the actual soft wrap opportunity belongs to the next set.
        // e.g. "this_is_the_trailing_run<span> <-but_this_space_here_is_the_soft_wrap_opportunity"
        // When there's an inline box start(<span>)/end(</span>) between the trailing and the (next)leading run, while we break before the inline box start (<span>)
        // the actual soft wrap position is after the inline box start (<span>) but in terms of line breaking continuity the inline box start (<span>) and the whitespace run belong together.
        RELEASE_ASSERT(layoutRangeEnd <= inlineItemList.size());
        for (auto index = softWrapOpportunityIndex; index < layoutRangeEnd; ++index) {
            if (inlineItemList[index].isInlineBoxStart() || inlineItemList[index].isInlineBoxEnd() || inlineItemList[index].isOpaque())
                continue;
            // FIXME: Check if [non-whitespace][inline-box][no-whitespace] content has rules about it.
            // For now let's say the soft wrap position belongs to the next set of runs when [non-whitespace][inline-box][whitespace], [non-whitespace][inline-box][box] etc.
            auto inlineItemListTextItem = dynamicDowncast<InlineTextItem>(inlineItemList[index]);
            return inlineItemListTextItem && !inlineItemListTextItem->isWhitespace();
        }
        return true;
    }
    if (trailingInlineItem.isInlineBoxStart()) {
        // This is a special case when the inline box's first child is a float box.
        return false;
    }
    if (trailingInlineItem.isOpaque()) {
        for (auto index = softWrapOpportunityIndex; index--;) {
            if (!inlineItemList[index].isOpaque())
                return hasTrailingSoftWrapOpportunity(index + 1, layoutRangeEnd, inlineItemList);
        }
        ASSERT(inlineItemList[softWrapOpportunityIndex].isFloat());
        return false;
    }
    ASSERT_NOT_REACHED();
    return true;
};

static TextDirection inlineBaseDirectionForLineContent(const Line::RunList& runs, const RenderStyle& rootStyle, std::optional<PreviousLine> previousLine)
{
    ASSERT(!runs.isEmpty());
    auto shouldUseBlockDirection = rootStyle.unicodeBidi() != UnicodeBidi::Plaintext;
    if (shouldUseBlockDirection)
        return rootStyle.writingMode().bidiDirection();
    // A previous line ending with a line break (<br> or preserved \n) introduces a new unicode paragraph with its own direction.
    if (previousLine && !previousLine->endsWithLineBreak)
        return previousLine->inlineBaseDirection;
    return TextUtil::directionForTextContent(toString(runs));
}

struct LineCandidate {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(LineCandidate);

    void reset();

    struct InlineContent {
        const InlineContentBreaker::ContinuousContent& continuousContent() const { return m_continuousContent; }
        InlineContentBreaker::ContinuousContent& continuousContent() { return m_continuousContent; }
        const InlineItem* trailingLineBreak() const { return m_trailingLineBreak; }
        const InlineItem* trailingWordBreakOpportunity() const { return m_trailingWordBreakOpportunity; }

        void appendInlineItem(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment = 0);
        void reset();
        bool isEmpty() const { return m_continuousContent.runs().isEmpty() && !trailingWordBreakOpportunity() && !trailingLineBreak(); }

        void setHasTrailingSoftWrapOpportunity(bool hasTrailingSoftWrapOpportunity) { m_hasTrailingSoftWrapOpportunity = hasTrailingSoftWrapOpportunity; }
        bool hasTrailingSoftWrapOpportunity() const { return m_hasTrailingSoftWrapOpportunity; }

        void setTrailingSoftHyphenWidth(InlineLayoutUnit hyphenWidth) { m_continuousContent.setTrailingSoftHyphenWidth(hyphenWidth); }

        void setHangingContentWidth(InlineLayoutUnit logicalWidth) { m_continuousContent.setHangingContentWidth(logicalWidth); }

        void setHasTrailingClonedDecoration(bool hasClonedDecoration) { m_hasTrailingClonedDecoration = hasClonedDecoration; }
        bool hasTrailingClonedDecoration() const { return m_hasTrailingClonedDecoration; }

        void setMinimumRequiredWidth(InlineLayoutUnit minimumRequiredWidth) { m_continuousContent.setMinimumRequiredWidth(minimumRequiredWidth); }

        std::optional<size_t> firstTextRunIndex() const { return m_firstTextRunIndex; }
        std::optional<size_t> lastTextRunIndex() const { return m_lastTextRunIndex; }

        bool isShapingCandidateByContent() const { return m_hasTextContentSpanningBoxes; }

    private:
        InlineContentBreaker::ContinuousContent m_continuousContent;
        const InlineItem* m_trailingLineBreak { nullptr };
        const InlineItem* m_trailingWordBreakOpportunity { nullptr };
        bool m_hasTrailingClonedDecoration { false };
        bool m_hasTrailingSoftWrapOpportunity { false };
        std::optional<size_t> m_firstTextRunIndex { };
        std::optional<size_t> m_lastTextRunIndex { };
        std::optional<size_t> m_lastInlineBoxIndex { };
        bool m_hasTextContentSpanningBoxes { false };
    };

    // Candidate content is a collection of inline content or a float box.
    InlineContent inlineContent;
    const InlineItem* floatItem { nullptr };
    const InlineItem* blockItem { nullptr };
};

inline void LineCandidate::InlineContent::appendInlineItem(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment)
{
    if (inlineItem.isAtomicInlineBox() || inlineItem.isOpaque())
        return m_continuousContent.append(inlineItem, style, logicalWidth, textSpacingAdjustment);

    if (inlineItem.isInlineBoxStartOrEnd()) {
        auto numberOfRuns = m_continuousContent.runs().size();
        m_hasTextContentSpanningBoxes = m_hasTextContentSpanningBoxes || (m_lastTextRunIndex && m_lastTextRunIndex == numberOfRuns - 1);
        m_lastInlineBoxIndex = numberOfRuns;
        m_continuousContent.append(inlineItem, style, logicalWidth, textSpacingAdjustment);
        return;
    }

    if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem)) {
        auto numberOfRuns = m_continuousContent.runs().size();
        m_firstTextRunIndex = m_firstTextRunIndex.value_or(numberOfRuns);
        m_lastTextRunIndex = numberOfRuns;
        m_hasTextContentSpanningBoxes = m_hasTextContentSpanningBoxes || (m_lastInlineBoxIndex && m_lastInlineBoxIndex == numberOfRuns - 1);
        return m_continuousContent.appendTextContent(*inlineTextItem, style, logicalWidth);
    }

    if (inlineItem.isLineBreak()) {
        m_trailingLineBreak = &inlineItem;
        return;
    }

    if (inlineItem.isWordBreakOpportunity()) {
        m_trailingWordBreakOpportunity = &inlineItem;
        return;
    }

    ASSERT_NOT_REACHED();
}

inline void LineCandidate::InlineContent::reset()
{
    m_continuousContent.reset();
    m_trailingLineBreak = { };
    m_trailingWordBreakOpportunity = { };
    m_hasTrailingClonedDecoration = { };
    m_hasTrailingSoftWrapOpportunity = { };
    m_firstTextRunIndex = { };
    m_lastTextRunIndex = { };
    m_lastInlineBoxIndex = { };
    m_hasTextContentSpanningBoxes = { };
}

inline void LineCandidate::reset()
{
    floatItem = { };
    blockItem = { };
    inlineContent.reset();
}

LineBuilder::LineBuilder(InlineFormattingContext& inlineFormattingContext, HorizontalConstraints rootHorizontalConstraints, const InlineItemList& inlineItemList, TextSpacingContext textSpacingContext)
    : AbstractLineBuilder(inlineFormattingContext, inlineFormattingContext.root(), rootHorizontalConstraints, inlineItemList)
    , m_floatingContext(inlineFormattingContext.floatingContext())
    , m_textSpacingContext(WTFMove(textSpacingContext))
{
}

LineLayoutResult LineBuilder::layoutInlineContent(const LineInput& lineInput, const std::optional<PreviousLine>& previousLine, bool isFirstFormattedLineCandidate)
{
    initialize(lineInput.initialLogicalRect, lineInput.needsLayoutRange, previousLine, isFirstFormattedLineCandidate);
    auto lineContent = placeInlineAndFloatContent(lineInput.needsLayoutRange);
    auto result = m_line.close();
    auto inlineContentEnding = result.isContentful ? InlineFormattingUtils::inlineContentEnding(result) : std::nullopt;

    if (isInIntrinsicWidthMode()) {
        return { lineContent->range
            , WTFMove(result.runs)
            , { WTFMove(m_placedFloats), WTFMove(m_suspendedFloats), { } }
            , { { }, result.contentLogicalWidth, { }, lineContent->overflowLogicalWidth }
            , { m_lineLogicalRect.topLeft() }
            , { }
            , { }
            , { isFirstFormattedLineCandidate && inlineContentEnding.has_value() ? IsFirstFormattedLine::Yes : IsFirstFormattedLine::No, { } }
            , { }
            , inlineContentEnding
            , { }
            , { }
            , { }
            , { }
        };
    }

    auto isLastInlineContent = isLastLineWithInlineContent(lineContent, lineInput.needsLayoutRange.endIndex(), result.runs);
    // Lines with nothing but content trailing out-of-flow boxes should also be considered last line for alignment
    // e.g. <div style="text-align-last: center">last line<br><div style="display: inline; position: absolute"></div></div>
    // Both the inline content ('last line') and the trailing out-of-flow box are supposed to be center aligned.
    auto shouldTreatAsLastLine = isLastInlineContent || lineContent->range.endIndex() == lineInput.needsLayoutRange.endIndex();
    auto inlineBaseDirection = !result.runs.isEmpty() ? inlineBaseDirectionForLineContent(result.runs, rootStyle(), m_previousLine) : TextDirection::LTR;
    auto lineEndsWithForcedLineBreak = lineContent->lineBreakReason == LineContent::LineBreakReason::ForcedLineBreakByBlockContent || Line::hasTrailingForcedLineBreak(result.runs);
    auto isLastLineOrLineEndsWithForcedLineBreak = shouldTreatAsLastLine || lineEndsWithForcedLineBreak;
    auto contentLogicalLeft = !result.runs.isEmpty() ? InlineFormattingUtils::horizontalAlignmentOffset(rootStyle(), result.contentLogicalRight, m_lineLogicalRect.width(), result.hangingTrailingContentWidth, isLastLineOrLineEndsWithForcedLineBreak, inlineBaseDirection) : 0.f;
    Vector<int32_t> visualOrderList;
    if (result.contentNeedsBidiReordering)
        computedVisualOrder(result.runs, visualOrderList);

    return { lineContent->range
        , WTFMove(result.runs)
        , { WTFMove(m_placedFloats), WTFMove(m_suspendedFloats), m_lineIsConstrainedByFloat }
        , { contentLogicalLeft, result.contentLogicalWidth, contentLogicalLeft + result.contentLogicalRight, lineContent->overflowLogicalWidth }
        , { m_lineLogicalRect.topLeft(), m_lineLogicalRect.width(), m_lineInitialLogicalRect.left(), m_initialIntrusiveFloatsWidth, m_initialLetterClearGap }
        , { !result.isHangingTrailingContentWhitespace, result.hangingTrailingContentWidth, result.hangablePunctuationStartWidth }
        , { WTFMove(visualOrderList), inlineBaseDirection }
        , { isFirstFormattedLineCandidate && inlineContentEnding.has_value() ? IsFirstFormattedLine::Yes : IsFirstFormattedLine::No, isLastInlineContent }
        , { WTFMove(lineContent->rubyBaseAlignmentOffsetList), lineContent->rubyAnnotationOffset }
        , inlineContentEnding
        , result.nonSpanningInlineLevelBoxCount
        , { }
        , { }
        , lineContent->range.isEmpty() ? std::make_optional(m_lineLogicalRect.top() + m_candidateContentMaximumHeight) : std::nullopt
    };
}

void LineBuilder::initialize(const InlineRect& initialLineLogicalRect, const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>& previousLine, bool isFirstFormattedLineCandidate)
{
    ASSERT(!needsLayoutRange.isEmpty() || (previousLine && !previousLine->suspendedFloats.isEmpty()));
    reset();

    m_previousLine = previousLine;
    m_isFirstFormattedLineCandidate = isFirstFormattedLineCandidate;
    m_placedFloats.clear();
    m_suspendedFloats.clear();
    m_lineSpanningInlineBoxes.clear();
    m_overflowingLogicalWidth = { };
    m_partialLeadingTextItem = { };
    m_initialLetterClearGap = { };
    m_candidateContentMaximumHeight = { };
    inlineContentBreaker().setHyphenationDisabled(layoutState().isHyphenationDisabled());

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
        auto& firstInlineItem = m_inlineItemList[needsLayoutRange.startIndex()];
        // If the parent is the formatting root, we can stop here. This is root inline box content, there's no nesting inline box from the previous line(s)
        // unless the inline box closing is forced over to the current line.
        // e.g.
        // <span>normally the inline box closing forms a continuous content</span>
        // <span>unless it's forced to the next line<br></span>
        auto& firstLayoutBox = firstInlineItem.layoutBox();
        auto hasLeadingInlineBoxEnd = firstInlineItem.isInlineBoxEnd();

        if (!hasLeadingInlineBoxEnd) {
            if (isRootLayoutBox(firstLayoutBox.parent()))
                return;

            if (isRootLayoutBox(firstLayoutBox.parent().parent())) {
                // In many cases the entire content is wrapped inside a single inline box.
                // e.g. <div><span>wall of text with<br>single, line spanning inline box...</span></div>
                ASSERT(firstLayoutBox.parent().isInlineBox());
                m_lineSpanningInlineBoxes.append({ firstLayoutBox.parent(), InlineItem::Type::InlineBoxStart, InlineItem::opaqueBidiLevel });
                return;
            }
        }

        Vector<const Box*, 2> spanningLayoutBoxList;
        if (hasLeadingInlineBoxEnd)
            spanningLayoutBoxList.append(&firstLayoutBox);

        auto* ancestor = &firstInlineItem.layoutBox().parent();
        while (!isRootLayoutBox(*ancestor)) {
            spanningLayoutBoxList.append(ancestor);
            ancestor = &ancestor->parent();
        }
        // Let's treat these spanning inline items as opaque bidi content. They should not change the bidi levels on adjacent content.
        for (auto* spanningInlineBox : spanningLayoutBoxList | std::views::reverse)
            m_lineSpanningInlineBoxes.append({ *spanningInlineBox, InlineItem::Type::InlineBoxStart, InlineItem::opaqueBidiLevel });
    };
    createLineSpanningInlineBoxes();
    m_line.initialize(m_lineSpanningInlineBoxes, isFirstFormattedLineCandidate);

    m_lineInitialLogicalRect = initialLineLogicalRect;
    auto previousLineEndsWithLineBreak = previousLine ? std::make_optional(previousLine->endsWithLineBreak ? InlineFormattingUtils::LineEndsWithLineBreak::Yes : InlineFormattingUtils::LineEndsWithLineBreak::No) : std::nullopt;
    m_lineMarginStart = formattingContext().formattingUtils().computedTextIndent(isInIntrinsicWidthMode() ? InlineFormattingUtils::IsIntrinsicWidthMode::Yes : InlineFormattingUtils::IsIntrinsicWidthMode::No, isFirstFormattedLineCandidate ? IsFirstFormattedLine::Yes : IsFirstFormattedLine::No, previousLineEndsWithLineBreak, initialLineLogicalRect.width());

    auto constraints = floatAvoidingRect(initialLineLogicalRect, { });
    m_lineLogicalRect = constraints.logicalRect;
    m_lineIsConstrainedByFloat = constraints.constrainedSideSet;
    // This is by how much intrusive floats (coming from parent/sibling FCs) initially offset the line.
    m_initialIntrusiveFloatsWidth = m_lineLogicalRect.left() - initialLineLogicalRect.left();
    m_lineLogicalRect.moveHorizontally(m_lineMarginStart);
    // While negative margins normally don't expand the available space, preferred width computation gets confused by negative text-indent
    // (shrink the space needed for the content) which we have to balance it here.
    m_lineLogicalRect.expandHorizontally(-m_lineMarginStart);

    auto initializeLeadingContentFromOverflow = [&] {
        if (!previousLine || !needsLayoutRange.start.offset)
            return;
        auto overflowingInlineItemPosition = needsLayoutRange.start;
        if (auto* overflowingInlineTextItem = dynamicDowncast<InlineTextItem>(m_inlineItemList[overflowingInlineItemPosition.index])) {
            ASSERT(overflowingInlineItemPosition.offset < overflowingInlineTextItem->length());
            auto overflowingLength = overflowingInlineTextItem->length() - overflowingInlineItemPosition.offset;
            if (overflowingLength) {
                // Turn previous line's overflow content into the next line's leading content.
                // "sp[<-line break->]lit_content" -> break position: 2 -> leading partial content length: 11.
                m_partialLeadingTextItem = overflowingInlineTextItem->right(overflowingLength, previousLine->trailingOverflowingContentWidth);
                return;
            }
        }
        m_overflowingLogicalWidth = previousLine->trailingOverflowingContentWidth;
    };
    initializeLeadingContentFromOverflow();
}

UniqueRef<LineContent> LineBuilder::placeInlineAndFloatContent(const InlineItemRange& needsLayoutRange)
{
    size_t resumedFloatCount = 0;
    auto layoutPreviouslySuspendedFloats = [&] {
        if (!m_previousLine)
            return true;
        // FIXME: Note that placedInlineItemCount is not incremented here as these floats are already accounted for (at previous line)
        // as LineContent only takes one range -meaning that inline layout may continue while float layout is being suspended
        // and the placed InlineItem range ends at the last inline item placed on the current line.
        for (size_t index = 0; index < m_previousLine->suspendedFloats.size(); ++index) {
            auto& suspendedFloat = *m_previousLine->suspendedFloats[index];
            auto isPlaced = tryPlacingFloatBox(suspendedFloat, !index ? MayOverConstrainLine::OnlyWhenFirstFloatOnLine : MayOverConstrainLine::No);
            if (!isPlaced) {
                // Can't place more floats here. We'll try to place these floats on subsequent lines.
                for (; index < m_previousLine->suspendedFloats.size(); ++index)
                    m_suspendedFloats.append(m_previousLine->suspendedFloats[index]);
                return false;
            }
            ++resumedFloatCount;
        }
        m_previousLine->suspendedFloats.clear();
        return true;
    };

    auto lineContent = makeUniqueRef<LineContent>();

    if (!layoutPreviouslySuspendedFloats()) {
        // Couldn't even manage to place all suspended floats from previous line(s). -which also means we can't fit any inline content at this vertical position.
        lineContent->range = { needsLayoutRange.start, needsLayoutRange.start };
        m_candidateContentMaximumHeight = m_lineLogicalRect.height();
        return lineContent;
    }

    size_t placedInlineItemCount = 0;

    auto layoutInlineAndFloatContent = [&] {
        auto lineCandidate = makeUniqueRef<LineCandidate>();

        auto currentItemIndex = needsLayoutRange.startIndex();
        while (currentItemIndex < needsLayoutRange.endIndex()) {
            // 1. Collect the set of runs that we can commit to the line as one entity e.g. <span>text_and_span_start_span_end</span>.
            // 2. Apply floats and shrink the available horizontal space e.g. <span>intru_<div style="float: left"></div>sive_float</span>.
            // 3. Check if the content fits the line and commit the content accordingly (full, partial or not commit at all).
            // 4. Return if we are at the end of the line either by not being able to fit more content or because of an explicit line break.
            auto canidateStartEndIndex = std::pair<size_t, size_t> { currentItemIndex, formattingContext().formattingUtils().nextWrapOpportunity(currentItemIndex, needsLayoutRange, m_inlineItemList) };
            candidateContentForLine(lineCandidate, canidateStartEndIndex, needsLayoutRange, m_line.contentLogicalRight());
            // Now check if we can put this content on the current line.
            if (auto* floatItem = lineCandidate->floatItem) {
                ASSERT(lineCandidate->inlineContent.isEmpty());
                if (!tryPlacingFloatBox(floatItem->layoutBox(), m_line.runs().isEmpty() ? MayOverConstrainLine::Yes : MayOverConstrainLine::No)) {
                    // This float overconstrains the line (it simply means shrinking the line box by the float would cause inline content overflow.)
                    // At this point we suspend float layout but continue with inline layout.
                    // Such suspended float will be placed at the next available vertical positon when this line "closes".
                    m_suspendedFloats.append(&floatItem->layoutBox());
                }
                ++placedInlineItemCount;
            } else if (auto* blockItem = lineCandidate->blockItem) {
                // We need to break whenever we come across a block level block to ensure it's the only item on the line.
                // This is unlike hard line break as in case of 'text<br>', hard line break stays on the current line.
                if (placedInlineItemCount) {
                    lineContent->lineBreakReason = LineContent::LineBreakReason::ForcedLineBreakByBlockContent;
                    return;
                }

                ASSERT(lineCandidate->inlineContent.isEmpty());
                handleBlockContent(*blockItem);
                ++placedInlineItemCount;
                // It's always end of line before/after a block level box.
                return;
            } else {
                auto result = handleInlineContent(needsLayoutRange, lineCandidate);
                auto isEndOfLine = result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes;
                if (!result.committedCount.isRevert) {
                    placedInlineItemCount += result.committedCount.value;
                    auto& inlineContent = lineCandidate->inlineContent;
                    auto inlineContentIsFullyPlaced = inlineContent.continuousContent().runs().size() == result.committedCount.value && !result.partialTrailingContentLength;
                    if (inlineContentIsFullyPlaced) {
                        if (auto* wordBreakOpportunity = inlineContent.trailingWordBreakOpportunity()) {
                            // <wbr> needs to be on the line as an empty run so that we can construct an inline box and compute basic geometry.
                            ++placedInlineItemCount;
                            m_line.appendWordBreakOpportunity(*wordBreakOpportunity, wordBreakOpportunity->style());
                        }
                        if (inlineContent.trailingLineBreak()) {
                            // Fully placed (or empty) content followed by a line break means "end of line".
                            // FIXME: This will put the line break box at the end of the line while in case of some inline boxes, the line break
                            // could very well be at an earlier position. This has no visual implications at this point though (only geometry correctness on the line break box).
                            // e.g. <span style="border-right: 10px solid green">text<br></span> where the <br>'s horizontal position is before the right border and not after.
                            auto& trailingLineBreak = *inlineContent.trailingLineBreak();
                            m_line.appendLineBreak(trailingLineBreak, trailingLineBreak.style());
                            if (trailingLineBreak.bidiLevel() != UBIDI_DEFAULT_LTR)
                                m_line.setContentNeedsBidiReordering();
                            ++placedInlineItemCount;
                            isEndOfLine = true;
                        }
                    }
                } else
                    placedInlineItemCount = result.committedCount.value;

                if (isEndOfLine) {
                    lineContent->partialTrailingContentLength = result.partialTrailingContentLength;
                    lineContent->overflowLogicalWidth = result.overflowLogicalWidth;
                    return;
                }
            }
            currentItemIndex = needsLayoutRange.startIndex() + placedInlineItemCount;
        }
        // Looks like we've run out of content.
        ASSERT(placedInlineItemCount || resumedFloatCount);
    };
    layoutInlineAndFloatContent();

    auto computePlacedInlineItemRange = [&] {
        lineContent->range = { needsLayoutRange.start, needsLayoutRange.start };

        if (!placedInlineItemCount)
            return;

        // Layout range already includes "suspended" floats from previous line(s). See layoutPreviouslySuspendedFloats above for details.
        ASSERT(m_placedFloats.size() >= resumedFloatCount);
        auto onlyFloatContentPlaced = placedInlineItemCount == m_placedFloats.size() - resumedFloatCount;
        if (onlyFloatContentPlaced || !lineContent->partialTrailingContentLength) {
            lineContent->range.end = { needsLayoutRange.startIndex() + placedInlineItemCount, { } };
            return;
        }

        auto trailingInlineItemIndex = needsLayoutRange.startIndex() + placedInlineItemCount - 1;
        auto overflowingInlineTextItemLength = downcast<InlineTextItem>(m_inlineItemList[trailingInlineItemIndex]).length();
        ASSERT(lineContent->partialTrailingContentLength && lineContent->partialTrailingContentLength < overflowingInlineTextItemLength);
        lineContent->range.end = { trailingInlineItemIndex, overflowingInlineTextItemLength - lineContent->partialTrailingContentLength };
    };
    computePlacedInlineItemRange();

    ASSERT(lineContent->range.endIndex() <= needsLayoutRange.endIndex());

    auto handleLineEnding = [&] {
        auto isLastInlineContent = isLastLineWithInlineContent(lineContent, needsLayoutRange.endIndex(), m_line.runs());
        auto horizontalAvailableSpace = m_lineLogicalRect.width();
        auto& rootStyle = this->rootStyle();

        auto handleTrailingContent = [&] {
            auto& quirks = formattingContext().quirks();
            auto lineHasOverflow = [&] {
                return horizontalAvailableSpace < m_line.contentLogicalWidth() && m_line.hasContentOrListMarker();
            };
            auto isLineBreakAfterWhitespace = [&] {
                return rootStyle.lineBreak() == LineBreak::AfterWhiteSpace && intrinsicWidthMode() != IntrinsicWidthMode::Minimum && (!isLastInlineContent || lineHasOverflow());
            };
            m_line.handleTrailingTrimmableContent(isLineBreakAfterWhitespace() ? Line::TrailingContentAction::Preserve : Line::TrailingContentAction::Remove);
            if (quirks.trailingNonBreakingSpaceNeedsAdjustment(isInIntrinsicWidthMode(), lineHasOverflow()))
                m_line.handleOverflowingNonBreakingSpace(isLineBreakAfterWhitespace() ? Line::TrailingContentAction::Preserve : Line::TrailingContentAction::Remove, m_line.contentLogicalWidth() - horizontalAvailableSpace);

            m_line.handleTrailingHangingContent(intrinsicWidthMode(), horizontalAvailableSpace, isLastInlineContent);

            auto mayNeedOutOfFlowOverflowTrimming = !isInIntrinsicWidthMode() && lineHasOverflow() && !lineContent->partialTrailingContentLength && TextUtil::isWrappingAllowed(rootStyle);
            if (mayNeedOutOfFlowOverflowTrimming) {
                // Overflowing out-of-flow boxes should wrap the to subsequent lines just like any other in-flow content.
                // However since we take a shortcut by not considering out-of-flow content as inflow but instead treating it as an opaque box with zero width and no
                // soft wrap opportunity, any overflowing out-of-flow content would pile up as trailing content.
                // Alternatively we could initiate a two pass layout first with out-of-flow content treated as true inflow and a second without them.
                ASSERT(!lineContent->range.end.offset);
                if (auto* lastRemovedTrailingBox = m_line.removeOverflowingOutOfFlowContent()) {
                    auto lineEndIndex = [&] {
                        for (auto index = lineContent->range.start.index; index < lineContent->range.end.index; ++index) {
                            if (&m_inlineItemList[index].layoutBox() == lastRemovedTrailingBox)
                                return index;
                        }
                        ASSERT_NOT_REACHED();
                        return lineContent->range.end.index;
                    };
                    lineContent->range.end.index = lineEndIndex();
                }
            }
        };
        handleTrailingContent();

        // On each line, reset the embedding level of any sequence of whitespace characters at the end of the line
        // to the paragraph embedding level
        m_line.resetBidiLevelForTrailingWhitespace(rootStyle.writingMode().isBidiLTR() ? UBIDI_LTR : UBIDI_RTL);

        if (m_line.hasContent()) {
            auto applyRunBasedAlignmentIfApplicable = [&] {
                if (isInIntrinsicWidthMode())
                    return;

                auto spaceToDistribute = horizontalAvailableSpace - m_line.contentLogicalWidth() + (m_line.isHangingTrailingContentWhitespace() ? m_line.hangingTrailingContentWidth() : 0.f);
                if (root().isRubyAnnotationBox() && rootStyle.textAlign() == RenderStyle::initialTextAlign()) {
                    lineContent->rubyAnnotationOffset = RubyFormattingContext::applyRubyAlignOnAnnotationBox(m_line, spaceToDistribute, formattingContext());
                    m_line.inflateContentLogicalWidth(spaceToDistribute);
                    m_line.adjustContentRightWithRubyAlign(2 * lineContent->rubyAnnotationOffset);
                    return;
                }
                // Text is justified according to the method specified by the text-justify property,
                // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
                // the last line before a forced break or the end of the block is start-aligned.
                auto hasTextAlignJustify = (isLastInlineContent || m_line.runs().last().isLineBreak()) ? rootStyle.textAlignLast() == Style::TextAlignLast::Justify : rootStyle.textAlign() == Style::TextAlign::Justify;
                if (hasTextAlignJustify) {
                    auto additionalSpaceForAlignedContent = InlineContentAligner::applyTextAlignJustify(m_line.runs(), spaceToDistribute, m_line.hangingTrailingWhitespaceLength());
                    m_line.inflateContentLogicalWidth(additionalSpaceForAlignedContent);
                }
                if (m_line.hasRubyContent())
                    lineContent->rubyBaseAlignmentOffsetList = RubyFormattingContext::applyRubyAlign(m_line, formattingContext());
            };
            applyRunBasedAlignmentIfApplicable();
        }
    };
    handleLineEnding();

    return lineContent;
}

InlineLayoutUnit LineBuilder::leadingPunctuationWidthForLineCandiate(const LineCandidate& lineCandidate) const
{
    auto& inlineContent = lineCandidate.inlineContent;
    auto firstTextRunIndex = inlineContent.firstTextRunIndex();
    if (!firstTextRunIndex)
        return { };

    auto isFirstLineFirstContent = isFirstFormattedLineCandidate() && !m_line.hasContent();
    if (!isFirstLineFirstContent)
        return { };

    auto& runs = inlineContent.continuousContent().runs();
    auto* inlineTextItem = dynamicDowncast<InlineTextItem>(runs[*firstTextRunIndex].inlineItem);
    if (!inlineTextItem) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto& style = isFirstFormattedLineCandidate() ? inlineTextItem->firstLineStyle() : inlineTextItem->style();
    if (!TextUtil::hasHangablePunctuationStart(*inlineTextItem, style))
        return { };

    if (*firstTextRunIndex) {
        // The text content is not the first in the candidate list. However it may be the first contentful one.
        for (size_t index = *firstTextRunIndex; index--;) {
            if (isContentfulOrHasDecoration(runs[index].inlineItem, formattingContext()))
                return { };
        }
    }
    // This candidate leading content may have hanging punctuation start.
    return TextUtil::hangablePunctuationStartWidth(*inlineTextItem, style);
}

InlineLayoutUnit LineBuilder::trailingPunctuationOrStopOrCommaWidthForLineCandiate(const LineCandidate& lineCandidate, size_t startIndexAfterCandidateContent,  size_t layoutRangeEnd) const
{
    auto& inlineContent = lineCandidate.inlineContent;
    auto lastTextRunIndex = inlineContent.lastTextRunIndex();
    if (!lastTextRunIndex)
        return { };

    auto& runs = inlineContent.continuousContent().runs();
    auto* inlineTextItem = dynamicDowncast<InlineTextItem>(runs[*lastTextRunIndex].inlineItem);
    if (!inlineTextItem) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& style = isFirstFormattedLineCandidate() ? inlineTextItem->firstLineStyle() : inlineTextItem->style();

    if (TextUtil::hasHangableStopOrCommaEnd(*inlineTextItem, style)) {
        // Stop or comma does apply to all lines not just the last formatted one.
        return TextUtil::hangableStopOrCommaEndWidth(*inlineTextItem, style);
    }

    if (TextUtil::hasHangablePunctuationEnd(*inlineTextItem, style)) {
        // FIXME: If this turns out to be problematic (finding out if this is the last formatted line that is), we
        // may have to fallback to a post-process setup, where after finishing laying out the content, we go back and re-layout
        // the last (2?) line(s) when there's trailing hanging punctuation.
        // For now let's probe the content all the way to layoutRangeEnd.
        for (auto index = startIndexAfterCandidateContent; index < layoutRangeEnd; ++index) {
            if (isContentfulOrHasDecoration(m_inlineItemList[index], formattingContext()))
                return { };
        }
        return TextUtil::hangablePunctuationEndWidth(*inlineTextItem, style);
    }

    return { };
}

Vector<std::pair<size_t, size_t>> LineBuilder::collectShapeRanges(const LineCandidate& lineCandidate) const
{
    // Normally candidate content is inline items between 2 soft wraping opportunities e.g.
    // <div>some text<span>more text</span></div>
    // where candidate contents are as follows: [some] [ ] [text<span>more] [ ] [text</span>]
    // However when white space is preserved and/or no wrapping is allowed the entire content is
    // one candidate content with all sorts of inline level content.

    // Let's find shaping ranges by filtering out content that are not relevant to shaping,
    // followed by processing this compressed list of [content , break, joint ] where
    // 'content' means shapable content (text)
    // 'break' means shape breaking gap (e.g. whitespace between 2 words)
    // 'keep' means box that keeps adjacent inline items in the same shaping context ("text<span>more" <- inline box start)
    auto& runs = lineCandidate.inlineContent.continuousContent().runs();

    auto isFirstFormattedLineCandidate = this->isFirstFormattedLineCandidate();
    enum class ShapingType : uint8_t { Content, Break, Keep };
    struct Content {
        ShapingType type { ShapingType::Break };
        size_t index { 0 };
    };
    Vector<Content> contentList;
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& inlineItem = runs[index].inlineItem;

        auto type = std::optional<ShapingType> { };
        switch (inlineItem.type()) {
        case InlineItem::Type::Text:
            type = downcast<InlineTextItem>(inlineItem).isWhitespace() ? ShapingType::Break : ShapingType::Content;
            break;
        case InlineItem::Type::AtomicInlineBox:
            type = ShapingType::Break;
            break;
        case InlineItem::Type::InlineBoxStart: {
            [[fallthrough]];
        case InlineItem::Type::InlineBoxEnd:
            auto& boxGeometry = formattingContext().geometryForBox(inlineItem.layoutBox());
            auto& style = isFirstFormattedLineCandidate ? inlineItem.firstLineStyle() : inlineItem.style();
            auto hasDecoration = [&] {
                // Note that this depends on the content being RTL (inline-box-end vs. start decoration matching visual order -visual matching).
                auto shouldCheckLogicalStart = style.writingMode().bidiDirection() == TextDirection::LTR ? inlineItem.type() == InlineItem::Type::InlineBoxEnd : inlineItem.type() == InlineItem::Type::InlineBoxStart;
                return shouldCheckLogicalStart ? boxGeometry.marginStart() || boxGeometry.borderStart() || boxGeometry.paddingStart() : boxGeometry.marginEnd() || boxGeometry.borderEnd() || boxGeometry.paddingEnd();
            };
            auto hasBidiIsolation = isIsolated(style.unicodeBidi());
            type = hasDecoration() || hasBidiIsolation ? ShapingType::Break : ShapingType::Keep;
            break;
        }
        case InlineItem::Type::HardLineBreak:
        case InlineItem::Type::SoftLineBreak:
        case InlineItem::Type::WordBreakOpportunity:
        case InlineItem::Type::Float:
        case InlineItem::Type::Opaque:
        case InlineItem::Type::Block:
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        auto shouldIgnore = [&] {
            if (!type)
                return true;
            if (*type == ShapingType::Content)
                return false;
            return contentList.isEmpty() || *type == contentList.last().type;
        };
        if (!shouldIgnore())
            contentList.append(Content { *type, index });
    }

    // Trailing non-content entries should just be ignored.
    while (!contentList.isEmpty()) {
        if (contentList.last().type == ShapingType::Content)
            break;
        contentList.removeLast();
    }

    if (contentList.isEmpty())
        return { };

    ASSERT(contentList.first().type == ShapingType::Content && contentList.last().type == ShapingType::Content);
    Vector<std::pair<size_t, size_t>> ranges;

    CheckedPtr lastFontCascade = &rootStyle().fontCascade();
    auto leadingContentRunIndex = std::optional<size_t> { };
    auto trailingContentRunIndex = std::optional<size_t> { };
    auto hasBoundaryBetween = false;

    auto resetCandidateRange = [&] {
        leadingContentRunIndex = { };
        trailingContentRunIndex = { };
        hasBoundaryBetween = false;
    };
    auto commitIfHasContentAndReset = [&] {
        if (leadingContentRunIndex && trailingContentRunIndex && hasBoundaryBetween)
            ranges.append({ *leadingContentRunIndex, *trailingContentRunIndex });
        resetCandidateRange();
    };

    for (auto entry : contentList) {
        switch (entry.type) {
        case ShapingType::Break:
            commitIfHasContentAndReset();
            break;
        case ShapingType::Keep:
            if (hasBoundaryBetween) {
                // Nested inline boxes e.g. <span>content<span>more<span>and some more
                ASSERT(leadingContentRunIndex);
                break;
            }
            if (leadingContentRunIndex)
                hasBoundaryBetween = true;
            break;
        case ShapingType::Content: {
            auto& inlineTextItem = downcast<InlineTextItem>(runs[entry.index].inlineItem);
            auto& styleToUse = isFirstFormattedLineCandidate ? inlineTextItem.firstLineStyle() : inlineTextItem.style();
            auto& inlineTextBox = inlineTextItem.inlineTextBox();
            auto isEligibleText = !inlineTextBox.canUseSimpleFontCodePath() && !inlineTextBox.isCombined() && inlineTextItem.direction() == TextDirection::RTL;

            if (!leadingContentRunIndex) {
                if (isEligibleText)
                    leadingContentRunIndex = entry.index;
                lastFontCascade = &styleToUse.fontCascade();
            } else if (hasBoundaryBetween) {
                auto hasMatchingFontCascade = *lastFontCascade.get() == styleToUse.fontCascade();
                if (isEligibleText && hasMatchingFontCascade)
                    trailingContentRunIndex = entry.index;
                else
                    resetCandidateRange();
            } else if (!isEligibleText)
                resetCandidateRange();
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
    commitIfHasContentAndReset();
    return ranges;
}

void LineBuilder::applyShapingOnRunRange(LineCandidate& lineCandidate, std::pair<size_t, size_t> range) const
{
    auto& inlineContent = lineCandidate.inlineContent;
    auto& runs = inlineContent.continuousContent().runs();
    if (range.first >= range.second || range.first >= runs.size() || range.second >= runs.size()) {
        ASSERT_NOT_REACHED();
        return;
    }
    runs[range.first].shapingBoundary = InlineContentBreaker::ContinuousContent::Run::ShapingBoundary::Start;
    runs[range.second].shapingBoundary = InlineContentBreaker::ContinuousContent::Run::ShapingBoundary::End;

    StringBuilder textContent;
    for (auto index = range.first; index <= range.second; ++index) {
        if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(runs[index].inlineItem))
            textContent.append(inlineTextItem->content());
    }

    ASSERT(!textContent.isEmpty());
    auto characterScanForCodePath = true;
    auto& style = isFirstFormattedLineCandidate() ? runs[range.first].inlineItem.firstLineStyle() : runs[range.first].inlineItem.style();
    auto textRun = TextRun { textContent, m_lineLogicalRect.left(), { }, ExpansionBehavior::defaultBehavior(), TextDirection::RTL, style.rtlOrdering() == Order::Visual, characterScanForCodePath };
    auto glyphAdvances = ComplexTextController::glyphAdvancesForTextRun(style.fontCascade(), textRun);

    if (glyphAdvances.size() != textRun.length()) {
        ASSERT_NOT_REACHED();
        return;
    }

    size_t glyphIndex = 0;
    auto shapedContentWidth = InlineLayoutUnit { };
    for (auto index = range.first; index <= range.second; ++index) {
        auto& run = runs[index];
        auto* inlineTextItem = dynamicDowncast<InlineTextItem>(run.inlineItem);
        if (!inlineTextItem) {
            ASSERT(run.inlineItem.isInlineBoxStartOrEnd());
            continue;
        }
        auto runWidth = InlineLayoutUnit { };
        for (size_t i = 0; i < inlineTextItem->length(); ++i) {
            runWidth += std::max(0.f, glyphAdvances[glyphIndex]);
            ++glyphIndex;
        }
        run.adjustContentWidth(runWidth);
        shapedContentWidth += runWidth;
    }
    inlineContent.continuousContent().adjustLogicalWidth(shapedContentWidth);
    inlineContent.continuousContent().setHasShapedContent();
}

void LineBuilder::applyShapingIfNeeded(LineCandidate& lineCandidate)
{
    if (!layoutState().shouldShapeTextAcrossInlineBoxes())
        return;

    if (!lineCandidate.inlineContent.isShapingCandidateByContent())
        return;

    for (auto range : collectShapeRanges(lineCandidate))
        applyShapingOnRunRange(lineCandidate, range);
}

void LineBuilder::shapePartialLineCandidate(LineCandidate& lineCandidate, size_t trailingRunIndex) const
{
    auto& inlineContent = lineCandidate.inlineContent;
    auto& runs = inlineContent.continuousContent().runs();

    if (trailingRunIndex >= runs.size()) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Find the shaping boundary end to see if we need to reshape the candidate text.
    for (auto index = trailingRunIndex + 1; index < runs.size(); ++index) {
        auto shapingBoundary = runs[index].shapingBoundary;
        if (!shapingBoundary)
            continue;
        if (*shapingBoundary == InlineContentBreaker::ContinuousContent::Run::ShapingBoundary::Start) {
            // Trailing content is a new shaping boundary, no need to reshape leading content.
            return;
        }
        ASSERT(*shapingBoundary == InlineContentBreaker::ContinuousContent::Run::ShapingBoundary::End);
        auto endPosition = std::optional<size_t> { };
        for (auto i = trailingRunIndex + 1; i--;) {
            auto& run = runs[i];
            if (!endPosition && run.inlineItem.isText())
                endPosition = i;

            auto shapingBoundary = run.shapingBoundary;
            if (shapingBoundary && *shapingBoundary == InlineContentBreaker::ContinuousContent::Run::ShapingBoundary::Start) {
                if (!endPosition) {
                    ASSERT_NOT_REACHED();
                    return;
                }
                if (*endPosition == i) {
                    // No shaping is needed when content does not cross multiple boxes.
                    run.shapingBoundary = { };
                    if (i < trailingRunIndex)
                        run.adjustContentWidth(formattingContext().formattingUtils().inlineItemWidth(run.inlineItem, { }, isFirstFormattedLineCandidate()));
                    return;
                }

                applyShapingOnRunRange(lineCandidate, { i, *endPosition });
                return;
            }
        }
        // We should always find a start when there's an end.
        ASSERT_NOT_REACHED();
    }
}

void LineBuilder::candidateContentForLine(LineCandidate& lineCandidate, std::pair<size_t, size_t> startEndIndex, const InlineItemRange& layoutRange, InlineLayoutUnit currentLogicalRight, SkipFloats skipFloats)
{
    ASSERT(startEndIndex.first < layoutRange.endIndex());
    ASSERT(startEndIndex.second <= layoutRange.endIndex());

    auto isFirstFormattedLineCandidate = this->isFirstFormattedLineCandidate();
    lineCandidate.reset();

    auto isLeadingPartiaContent = startEndIndex.first == layoutRange.startIndex() && m_partialLeadingTextItem;
    if (isLeadingPartiaContent) {
        ASSERT(!m_overflowingLogicalWidth);
        // Handle leading partial content first (overflowing text from the previous line).
        auto itemWidth = formattingContext().formattingUtils().inlineItemWidth(*m_partialLeadingTextItem, currentLogicalRight, isFirstFormattedLineCandidate);
        lineCandidate.inlineContent.appendInlineItem(*m_partialLeadingTextItem, m_partialLeadingTextItem->style(), itemWidth);
        currentLogicalRight += itemWidth;
        ++startEndIndex.first;
    }

    auto trailingSoftHyphenInlineTextItemIndex = std::optional<size_t> { };
    auto textSpacingAdjustment = InlineLayoutUnit { };
    auto contentHasInlineItemsWithDecorationClone = !m_line.inlineBoxListWithClonedDecorationEnd().isEmpty();

    for (auto index = startEndIndex.first; index < startEndIndex.second; ++index) {
        auto& inlineItem = m_inlineItemList[index];
        auto& style = isFirstFormattedLineCandidate ? inlineItem.firstLineStyle() : inlineItem.style();
        if (inlineItem.isInlineBoxStart()) {
            if (auto inlineBoxBoundaryTextSpacing = m_textSpacingContext.inlineBoxBoundaryTextSpacings.find(index); inlineBoxBoundaryTextSpacing != m_textSpacingContext.inlineBoxBoundaryTextSpacings.end())
                textSpacingAdjustment = inlineBoxBoundaryTextSpacing->value;
        }

        auto needsLayout = inlineItem.isFloat() || inlineItem.isAtomicInlineBox() || (inlineItem.isOpaque() && inlineItem.layoutBox().isRubyAnnotationBox());
        if (needsLayout) {
            // FIXME: Intrinsic width mode should call into the intrinsic width codepath. Currently we only get here when box has fixed width (meaning no need to run intrinsic width on the box).
            if (!isInIntrinsicWidthMode())
                formattingContext().integrationUtils().layoutWithFormattingContextForBox(downcast<ElementBox>(inlineItem.layoutBox()));
        }

        if (inlineItem.isFloat()) {
            if (skipFloats == SkipFloats::Yes)
                continue;
            lineCandidate.floatItem = &inlineItem;
            // This is a soft wrap opportunity, must be the only item in the list.
            ASSERT(startEndIndex.first + 1 == startEndIndex.second);
            continue;
        }
        if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem)) {
            auto logicalWidth = m_overflowingLogicalWidth ? *std::exchange(m_overflowingLogicalWidth, std::nullopt) : formattingContext().formattingUtils().inlineItemWidth(*inlineTextItem, currentLogicalRight, isFirstFormattedLineCandidate);
            if (!currentLogicalRight) {
                if (auto trimmableSpacing = m_textSpacingContext.trimmableTextSpacings.find(index); trimmableSpacing != m_textSpacingContext.trimmableTextSpacings.end())
                    logicalWidth -= trimmableSpacing->value;
            }
            lineCandidate.inlineContent.appendInlineItem(*inlineTextItem, style, logicalWidth);
            // Word spacing does not make the run longer, but it produces an offset instead. See Line::appendTextContent as well.
            currentLogicalRight += logicalWidth + (inlineTextItem->isWordSeparator() ? style.fontCascade().wordSpacing() : 0.f);
            trailingSoftHyphenInlineTextItemIndex = inlineTextItem->hasTrailingSoftHyphen() ? std::make_optional(index) : std::nullopt;
            continue;
        }
        if (inlineItem.isInlineBoxStartOrEnd()) {
            auto& layoutBox = inlineItem.layoutBox();
            auto logicalWidth = formattingContext().formattingUtils().inlineItemWidth(inlineItem, currentLogicalRight, isFirstFormattedLineCandidate);
            if (layoutBox.isRubyBase()) {
                if (inlineItem.isInlineBoxStart()) {
                    // There should only be one ruby base per/annotation candidate content as we allow line breaking between bases unless some special characters between ruby bases prevent us from doing so (see RubyFormattingContext::canBreakAtCharacter)
                    if (auto marginBoxWidth = RubyFormattingContext::annotationBoxLogicalWidth(layoutBox, formattingContext()); marginBoxWidth > 0) {
                        auto& inlineContent = lineCandidate.inlineContent;
                        inlineContent.setMinimumRequiredWidth(inlineContent.continuousContent().minimumRequiredWidth().value_or(InlineLayoutUnit { }) + marginBoxWidth);
                    }
                } else
                    logicalWidth += RubyFormattingContext::baseEndAdditionalLogicalWidth(layoutBox, m_line.runs(), lineCandidate.inlineContent.continuousContent().runs(), formattingContext());
            }

            contentHasInlineItemsWithDecorationClone |= inlineItem.isInlineBoxStart() && style.boxDecorationBreak() == BoxDecorationBreak::Clone;
            lineCandidate.inlineContent.appendInlineItem(inlineItem, style, logicalWidth, textSpacingAdjustment);
            currentLogicalRight += logicalWidth;
            continue;
        }
        if (inlineItem.isAtomicInlineBox()) {
            auto logicalWidth = formattingContext().formattingUtils().inlineItemWidth(inlineItem, currentLogicalRight, isFirstFormattedLineCandidate);
            // FIXME: While the line breaking related properties for atomic level boxes do not depend on the line index (first line style) it'd be great to figure out the correct style to pass in.
            lineCandidate.inlineContent.appendInlineItem(inlineItem, inlineItem.layoutBox().parent().style(), logicalWidth);
            currentLogicalRight += logicalWidth;
            continue;
        }
        if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity()) {
#if ASSERT_ENABLED
            // Since both <br> and <wbr> are explicit word break opportunities they have to be trailing items in this candidate run list unless they are embedded in inline boxes.
            // e.g. <span><wbr></span>
            for (auto i = index + 1; i < startEndIndex.second; ++i)
                ASSERT(m_inlineItemList[i].isInlineBoxEnd() || m_inlineItemList[i].isOpaque());
#endif
            lineCandidate.inlineContent.appendInlineItem(inlineItem, style, { });
            continue;
        }
        if (inlineItem.isOpaque()) {
            lineCandidate.inlineContent.appendInlineItem(inlineItem, style, { });
            continue;
        }
        if (inlineItem.isBlock()) {
            // Blocks must be the only items in the list.
            lineCandidate.blockItem = &inlineItem;
            ASSERT(startEndIndex.first + 1 == startEndIndex.second);
            continue;
        }
        ASSERT_NOT_REACHED();
    }

    if (lineCandidate.floatItem || lineCandidate.blockItem)
        return;

    auto setupTrailingContent = [&] {
        lineCandidate.inlineContent.setHasTrailingClonedDecoration(contentHasInlineItemsWithDecorationClone);

        auto setLeadingAndTrailingHangingPunctuation = [&] {
            auto& inlineContent = lineCandidate.inlineContent;
            auto hangingContentWidth = inlineContent.continuousContent().hangingContentWidth();
            // Do not even try to check for trailing punctuation when the candidate content already has whitespace type of hanging content.
            if (!hangingContentWidth)
                hangingContentWidth += trailingPunctuationOrStopOrCommaWidthForLineCandiate(lineCandidate, startEndIndex.second, layoutRange.endIndex());
            hangingContentWidth += leadingPunctuationWidthForLineCandiate(lineCandidate);
            if (hangingContentWidth)
                lineCandidate.inlineContent.setHangingContentWidth(hangingContentWidth);
        };
        setLeadingAndTrailingHangingPunctuation();

        auto setTrailingSoftHyphenWidth = [&] {
            if (!trailingSoftHyphenInlineTextItemIndex)
                return;
            for (auto index = *trailingSoftHyphenInlineTextItemIndex; index < startEndIndex.second; ++index) {
                if (!is<InlineTextItem>(m_inlineItemList[index]))
                    return;
            }
            auto& trailingInlineTextItem = m_inlineItemList[*trailingSoftHyphenInlineTextItemIndex];
            auto& style = isFirstFormattedLineCandidate ? trailingInlineTextItem.firstLineStyle() : trailingInlineTextItem.style();
            lineCandidate.inlineContent.setTrailingSoftHyphenWidth(TextUtil::hyphenWidth(style));
        };
        setTrailingSoftHyphenWidth();
        lineCandidate.inlineContent.setHasTrailingSoftWrapOpportunity(hasTrailingSoftWrapOpportunity(startEndIndex.second, layoutRange.endIndex(), m_inlineItemList));
    };
    setupTrailingContent();
    applyShapingIfNeeded(lineCandidate);
}

static inline InlineLayoutUnit availableWidth(const Line& line, InlineLayoutUnit lineWidth, std::optional<IntrinsicWidthMode> intrinsicWidthMode)
{
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
    // 1. Preferred width computation sums up floats while line breaker subtracts them.
    // 2. Available space is inherently a LayoutUnit based value (coming from block/flex etc layout) and it is the result of a floored float.
    // These can all lead to epsilon-scale differences.
    if (!intrinsicWidthMode || *intrinsicWidthMode == IntrinsicWidthMode::Maximum)
        lineWidth += LayoutUnit::epsilon();
#endif
    auto availableWidth = lineWidth - line.contentLogicalRight();
    return std::isnan(availableWidth) ? maxInlineLayoutUnit() : availableWidth;
}

LineBuilder::RectAndFloatConstraints LineBuilder::floatAvoidingRect(const InlineRect& logicalRect, InlineLayoutUnit lineMarginStart) const
{
    auto constraints = [&]() -> LineBuilder::RectAndFloatConstraints {
        if (isInIntrinsicWidthMode() || floatingContext().isEmpty())
            return { logicalRect, { } };

        auto constraints = formattingContext().formattingUtils().floatConstraintsForLine(logicalRect.top(), logicalRect.height(), floatingContext());
        if (!constraints.start && !constraints.end)
            return { logicalRect, { } };

        auto constrainedSideSet = OptionSet<UsedFloat> { };
        // text-indent acts as (start)margin on the line. When looking for intrusive floats we need to check against the line's _margin_ box.
        auto marginBoxRect = InlineRect { logicalRect.top(), logicalRect.left() - lineMarginStart, logicalRect.width() + lineMarginStart, logicalRect.height() };

        if (constraints.start && constraints.start->x > marginBoxRect.left()) {
            marginBoxRect.shiftLeftTo(constraints.start->x);
            constrainedSideSet.add(UsedFloat::Left);
        }
        if (constraints.end && constraints.end->x < marginBoxRect.right()) {
            marginBoxRect.setRight(std::max<InlineLayoutUnit>(marginBoxRect.left(), constraints.end->x));
            constrainedSideSet.add(UsedFloat::Right);
        }

        auto lineLogicalRect = InlineRect { marginBoxRect.top(), marginBoxRect.left() + lineMarginStart, marginBoxRect.width() - lineMarginStart, marginBoxRect.height() };
        return { lineLogicalRect, constrainedSideSet };
    }();

    if (auto adjustedRect = formattingContext().quirks().adjustedRectForLineGridLineAlign(constraints.logicalRect))
        constraints.logicalRect = *adjustedRect;

    return constraints;
}

LineBuilder::RectAndFloatConstraints LineBuilder::adjustedLineRectWithCandidateInlineContent(const LineCandidate& lineCandidate) const
{
    // Check if the candidate content would stretch the line and whether additional floats are getting in the way.
    auto& inlineContent = lineCandidate.inlineContent;
    if (isInIntrinsicWidthMode())
        return { m_lineLogicalRect };
    // FIXME: Use InlineFormattingUtils::inlineLevelBoxAffectsLineBox instead.
    auto candidateContentHeight = InlineLayoutUnit { };
    auto lineBoxContain = rootStyle().lineBoxContain();
    for (auto& run : inlineContent.continuousContent().runs()) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isText()) {
            auto& styleToUse = isFirstFormattedLineCandidate() ? inlineItem.firstLineStyle() : inlineItem.style();
            candidateContentHeight = std::max<InlineLayoutUnit>(candidateContentHeight, styleToUse.computedLineHeight());
        } else if (inlineItem.isAtomicInlineBox() && lineBoxContain.contains(Style::WebkitLineBoxContainValue::Replaced))
            candidateContentHeight = std::max(candidateContentHeight, InlineLayoutUnit { formattingContext().geometryForBox(inlineItem.layoutBox()).marginBoxHeight() });
    }
    if (candidateContentHeight <= m_lineLogicalRect.height())
        return { m_lineLogicalRect };

    return floatAvoidingRect({ m_lineLogicalRect.topLeft(), m_lineLogicalRect.width(), candidateContentHeight }, m_lineMarginStart);
}

std::optional<LineBuilder::InitialLetterOffsets> LineBuilder::adjustLineRectForInitialLetterIfApplicable(const Box& floatBox)
{
    auto drop = floatBox.style().initialLetter().drop();
    auto isInitialLetter = floatBox.isFloatingPositioned() && floatBox.style().pseudoElementType() == PseudoElementType::FirstLetter && drop;
    if (!isInitialLetter)
        return { };

    // Here we try to set the vertical start position for the float in flush with the adjoining text content's cap height.
    // It's a super premature as at this point we don't normally deal with vertical geometry -other than the incoming vertical constraint.
    auto initialLetterCapHeightOffset = formattingContext().quirks().initialLetterAlignmentOffset(floatBox, rootStyle());
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
    auto letterHeight = floatBox.style().initialLetter().height();
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
    switch (mayOverConstrainLine) {
    case MayOverConstrainLine::Yes:
        return true;
    case MayOverConstrainLine::OnlyWhenFirstFloatOnLine:
        // This is a resumed float from a previous line. Now we need to find a place for it.
        // (which also means that the current line can't have any floats that we couldn't place yet)
        ASSERT(m_suspendedFloats.isEmpty());
        if (!isLineConstrainedByFloat())
            return true;
        [[fallthrough]];
    case MayOverConstrainLine::No: {
        auto lineIsConsideredEmpty = !m_line.hasContent() && !isLineConstrainedByFloat();
        if (lineIsConsideredEmpty)
            return true;
        // Non-clear type of floats stack up (horizontally). It's easy to check if there's space for this float at all,
        // while floats with clear needs post-processing to see if they overlap existing line content (and here we just check if they may fit at all).
        auto lineLogicalWidth = floatBox.hasFloatClear() ? m_lineInitialLogicalRect.width() : m_lineLogicalRect.width();
        auto availableWidthForFloat = lineLogicalWidth - m_line.contentLogicalRight() + m_line.trimmableTrailingWidth();
        return availableWidthForFloat >= InlineLayoutUnit { floatBoxMarginBoxWidth };
    }
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
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

    auto& floatingContext = this->floatingContext();
    auto& boxGeometry = formattingContext().geometryForBox(floatBox);
    if (!shouldTryToPlaceFloatBox(floatBox, boxGeometry.marginBoxWidth(), mayOverConstrainLine))
        return false;

    auto lineMarginBoxLeft = std::max(0.f, m_lineLogicalRect.left() - m_lineMarginStart);
    auto computeFloatBoxPosition = [&] {
        // Set static position first.
        auto staticPosition = LayoutPoint { lineMarginBoxLeft, m_lineLogicalRect.top() };
        if (auto additionalOffsets = adjustLineRectForInitialLetterIfApplicable(floatBox)) {
            staticPosition.setY(m_lineLogicalRect.top() + additionalOffsets->capHeightOffset);
            boxGeometry.setVerticalMargin({ boxGeometry.marginBefore() + additionalOffsets->sunkenBelowFirstLineOffset, boxGeometry.marginAfter() });
        }
        staticPosition.move(boxGeometry.marginStart(), boxGeometry.marginBefore());
        boxGeometry.setTopLeft(staticPosition);
        // Compute float position by running float layout.
        auto floatingPosition = floatingContext.positionForFloat(floatBox, boxGeometry, rootHorizontalConstraints());
        boxGeometry.setTopLeft(floatingPosition);
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
        auto lineIsConsideredEmpty = !m_line.hasContent() && !isLineConstrainedByFloat();
        if (lineIsConsideredEmpty)
            return true;
        // When floats with clear are placed under existing floats, we may find ourselves in an over-constrained state and
        // can't place this float here.
        auto contentLogicalWidth = m_line.contentLogicalWidth() - m_line.trimmableTrailingWidth();
        return haveEnoughSpaceForFloatWithClear(BoxGeometry::marginBoxRect(boxGeometry), floatingContext.isStartPositioned(floatBox), m_lineLogicalRect, contentLogicalWidth);
    };
    if (floatBox.hasFloatClear() && !willFloatBoxWithClearFit())
        return false;

    auto placeFloatBox = [&] {
        auto lineIndex = m_previousLine ? (m_previousLine->lineIndex + 1) : 0lu;
        auto floatItem = floatingContext.makeFloatItem(floatBox, boxGeometry, lineIndex);
        layoutState().placedFloats().add(floatItem);
        m_placedFloats.append(floatItem);
    };
    placeFloatBox();

    auto adjustLineRectIfNeeded = [&] {
        if (!willFloatBoxShrinkLine) {
            // This float is placed outside the line box. No need to shrink the current line.
            return;
        }
        auto constraints = floatAvoidingRect(m_lineLogicalRect, m_lineMarginStart);
        m_lineLogicalRect = constraints.logicalRect;
        m_lineIsConstrainedByFloat.add(constraints.constrainedSideSet);
    };
    adjustLineRectIfNeeded();

    return true;
}

void LineBuilder::handleBlockContent(const InlineItem& blockItem)
{
    ASSERT(blockItem.isBlock());
    // Blocks are always the only content on the line.
    ASSERT(!m_line.hasContentOrListMarker());
    if (!isInIntrinsicWidthMode())
        formattingContext().integrationUtils().layoutWithFormattingContextForBlockInInline(downcast<ElementBox>(blockItem.layoutBox()), LayoutPoint { m_lineLogicalRect.topLeft() }, layoutState());
    auto marginBoxLogicalWidth = formattingContext().formattingUtils().inlineItemWidth(blockItem, { }, false);
    m_line.appendBlock(blockItem, marginBoxLogicalWidth);
    if (rootStyle().writingMode().isBidiRTL())
        m_line.setContentNeedsBidiReordering();
}

LineBuilder::Result LineBuilder::handleInlineContent(const InlineItemRange& layoutRange, LineCandidate& lineCandidate)
{
    auto result = LineBuilder::Result { };
    auto& inlineContent = lineCandidate.inlineContent;

    auto& continuousInlineContent = inlineContent.continuousContent();
    if (continuousInlineContent.runs().isEmpty()) {
        ASSERT(inlineContent.trailingLineBreak() || inlineContent.trailingWordBreakOpportunity());
        result = { inlineContent.trailingLineBreak() ? InlineContentBreaker::IsEndOfLine::Yes : InlineContentBreaker::IsEndOfLine::No };
        return result;
    }

    auto constraints = adjustedLineRectWithCandidateInlineContent(lineCandidate);
    auto availableWidthForCandidateContent = [&] {
        auto lineIndex = m_previousLine ? (m_previousLine->lineIndex + 1) : 0lu;
        // If width constraint overrides exist (e.g. text-wrap: balance), modify the available width accordingly.
        const auto& availableLineWidthOverride = layoutState().availableLineWidthOverride();
        auto widthOverride = availableLineWidthOverride.availableLineWidthOverrideForLine(lineIndex);
        auto availableTotalWidthForContent = widthOverride ? InlineLayoutUnit { widthOverride.value() } - m_lineMarginStart : constraints.logicalRect.width();
        return availableWidth(m_line, availableTotalWidthForContent, intrinsicWidthMode());
    }();

    auto lineHasContent = m_line.hasContentOrListMarker();
    auto verticalPositionHasFloatOrInlineContent = lineHasContent || isLineConstrainedByFloat() || !constraints.constrainedSideSet.isEmpty();
    auto lineBreakingResult = InlineContentBreaker::Result { InlineContentBreaker::Result::Action::Keep, InlineContentBreaker::IsEndOfLine::No, { }, { } };

    if (auto minimumRequiredWidth = continuousInlineContent.minimumRequiredWidth(); minimumRequiredWidth && *minimumRequiredWidth > availableWidthForCandidateContent) {
        if (verticalPositionHasFloatOrInlineContent)
            lineBreakingResult = InlineContentBreaker::Result { InlineContentBreaker::Result::Action::Wrap, InlineContentBreaker::IsEndOfLine::Yes, { }, { } };
    } else {
        auto lineStatus = InlineContentBreaker::LineStatus { m_line.contentLogicalRight(), availableWidthForCandidateContent, m_line.trimmableTrailingWidth(), m_line.trailingSoftHyphenWidth(), m_line.isTrailingRunFullyTrimmable(), verticalPositionHasFloatOrInlineContent, !m_wrapOpportunityList.isEmpty() };
        auto needsClonedDecorationHandling = inlineContent.hasTrailingClonedDecoration() || !m_line.inlineBoxListWithClonedDecorationEnd().isEmpty();
        if (needsClonedDecorationHandling)
            lineBreakingResult = handleInlineContentWithClonedDecoration(lineCandidate, lineStatus);
        else if (continuousInlineContent.logicalWidth() > availableWidthForCandidateContent)
            lineBreakingResult = inlineContentBreaker().processInlineContent(continuousInlineContent, lineStatus);
    }
    result = processLineBreakingResult(lineCandidate, layoutRange, lineBreakingResult);

    auto lineGainsNewContent = lineBreakingResult.action == InlineContentBreaker::Result::Action::Keep || lineBreakingResult.action == InlineContentBreaker::Result::Action::Break;
    if (lineGainsNewContent || !lineHasContent) {
        // In some cases in order to put this content on the line, we have to avoid float boxes that didn't constrain the line initially.
        // (e.g. when this new content is taller than any previous content and there are vertically stacked floats)
        // In some other cases we can't put any content on the line due to such newly discovered floats (e.g. shape-outside floats with gaps in-between them in vertical axis)
        m_lineLogicalRect = constraints.logicalRect;
        m_lineIsConstrainedByFloat.add(constraints.constrainedSideSet);
    }
    m_candidateContentMaximumHeight = constraints.logicalRect.height();
    return result;
}

static inline InlineLayoutUnit lineBreakingResultContentWidth(const InlineContentBreaker::ContinuousContent::RunList& runs, const InlineContentBreaker::Result::PartialTrailingContent& trailingContent)
{
    if (trailingContent.trailingRunIndex >= runs.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto contentWidth = InlineLayoutUnit { };
    for (size_t index = 0; index < trailingContent.trailingRunIndex; ++index)
        contentWidth += runs[index].contentWidth();

    if (auto partialTrailingRun = trailingContent.partialRun)
        return contentWidth + partialTrailingRun->logicalWidth + partialTrailingRun->hyphenWidth.value_or(0.f);

    auto& trailingRun = runs[trailingContent.trailingRunIndex];
    return contentWidth + trailingRun.contentWidth() + trailingContent.hyphenWidth.value_or(0.f);
}

InlineLayoutUnit LineBuilder::placedClonedDecorationWidth(const InlineContentBreaker::ContinuousContent::RunList& runs) const
{
    // Collect already placed, not yet closed inline boxes on the line (minus what we are about to close with the candidate runs)
    // e.g. <div><span>1 <span>2 3 4</span></span></div>
    // At [3] we've got 2 inline boxes placed on the line and they may have space taking (cloned) decoration ends.
    // At [4</span></span>] all inline boxes are closed.
    auto& formattingContext = this->formattingContext();

    HashSet<const Box*> clonedInlineBoxes;
    auto clonedDecorationEndWidth = InlineLayoutUnit { };
    for (auto* box : m_line.inlineBoxListWithClonedDecorationEnd()) {
        clonedDecorationEndWidth += formattingContext.geometryForBox(*box).borderAndPaddingEnd();
        clonedInlineBoxes.add(box);
    }

    for (size_t index = 0; index < runs.size(); ++index) {
        auto& inlineItem = runs[index].inlineItem;
        if (inlineItem.isInlineBoxEnd() && clonedInlineBoxes.contains(&inlineItem.layoutBox()))
            clonedDecorationEndWidth -= formattingContext.geometryForBox(inlineItem.layoutBox()).borderAndPaddingEnd();
    }

    return clonedDecorationEndWidth;
}

InlineLayoutUnit LineBuilder::clonedDecorationAtBreakingPosition(const InlineContentBreaker::ContinuousContent::RunList& runs, const InlineContentBreaker::Result::PartialTrailingContent& trailingContent) const
{
    // Compute how much decoration end we have to put as trailing content if we were to break the line at this position.
    // Collect already committed, but not yet closed inline boxes in addition to these new ones, coming with the candidate content.
    // e.g. <div><span>1 <span>2 3 4</span></span></div>
    // At [<span>2], we have to account for the leading inline box (provided it has cloned decoration) and the inline box (again, if it has cloned decoration) in the candidate content.
    if (trailingContent.trailingRunIndex >= runs.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& formattingContext = this->formattingContext();
    auto clonedDecorationWidth = InlineLayoutUnit { };

    for (auto* box : m_line.inlineBoxListWithClonedDecorationEnd())
        clonedDecorationWidth += formattingContext.geometryForBox(*box).borderAndPaddingEnd();

    for (size_t index = 0; index <= trailingContent.trailingRunIndex; ++index) {
        auto& inlineItem = runs[index].inlineItem;
        if (!inlineItem.isInlineBoxStartOrEnd() || inlineItem.style().boxDecorationBreak() != BoxDecorationBreak::Clone)
            continue;

        auto& inlineBoxGeometry = formattingContext.geometryForBox(inlineItem.layoutBox());
        if (inlineItem.isInlineBoxStart()) {
            clonedDecorationWidth += inlineBoxGeometry.borderAndPaddingEnd();
            continue;
        }
        if (inlineItem.isInlineBoxEnd()) {
            clonedDecorationWidth -= inlineBoxGeometry.borderAndPaddingEnd();
            continue;
        }
    }
    ASSERT(clonedDecorationWidth >= 0);
    return std::max(0.f, clonedDecorationWidth);
}

InlineContentBreaker::Result LineBuilder::handleInlineContentWithClonedDecoration(const LineCandidate& lineCandidate, InlineContentBreaker::LineStatus lineStatus)
{
    // 1. call content breaker to see whether the candidate content fits or not
    // 2. when content breaker tells us that this continuous content needs to be broken up, we have to check whether the partial content we are planning to put on the line has cloned decoration and whether it also fits
    // 3. traverse the candidate content up to the breaking position and compute the width of the cloned decoration(s)
    // 4. check if there's enough space for both content and its cloned decoration(s)
    // 5. if not, let's try again (go to #1) with reduced available space
    // At some point we either manage to fit the content + its cloned decoration(s) or we run out of available space
    // e.g.
    // <div style="width: 30px; word-break: break-all">ab<span style="-webkit-box-decoration-break: clone; padding-right: 20px">cd</span>ef</div>
    // (where each character is 10px wide)
    // [ab<span>cd</span>ef] is the continous content (there's no soft wrap opportunity in-between)
    // The breaking position is between [c] and [d]. We are going to put [abc] on the line which means we have to have space
    // for the enclosing inline box's (cloned) decoration end (20px) too, 50px altogether. -but we only have 30px space here.
    // And now we are at step (5); let's probe line breaking with reduced available space, go to step (1) until we find a valid breaking position (which is after [b]).
    ASSERT(lineCandidate.inlineContent.hasTrailingClonedDecoration() || !m_line.inlineBoxListWithClonedDecorationEnd().isEmpty());

    auto& inlineContent = lineCandidate.inlineContent;
    auto& continuousInlineContent = inlineContent.continuousContent();
    auto& runs = continuousInlineContent.runs();
    auto initialAvailableWidth = lineStatus.availableWidth;

    lineStatus.availableWidth -= placedClonedDecorationWidth(runs);

    if (continuousInlineContent.logicalWidth() <= lineStatus.availableWidth)
        return { InlineContentBreaker::Result::Action::Keep, InlineContentBreaker::IsEndOfLine::No, { }, { } };

    while (lineStatus.availableWidth) {
        auto lineBreakingResult = inlineContentBreaker().processInlineContent(continuousInlineContent, lineStatus);
        if (lineBreakingResult.action != InlineContentBreaker::Result::Action::Break)
            return lineBreakingResult;

        if (!lineBreakingResult.partialTrailingContent) {
            ASSERT_NOT_REACHED();
            return lineBreakingResult;
        }

        auto contentWidth = lineBreakingResultContentWidth(runs, *lineBreakingResult.partialTrailingContent);
        auto clonedDecorationWidth = clonedDecorationAtBreakingPosition(runs, *lineBreakingResult.partialTrailingContent);

        if (contentWidth + clonedDecorationWidth <= initialAvailableWidth)
            return lineBreakingResult;
        lineStatus.availableWidth = std::max(0.f, std::min(lineStatus.availableWidth, contentWidth) - 1.f);
    }

    // In case of this unlikely scenario where we couldn't find a fitting setup, let's just go with the last result -this will most likely produce decoration overflow which may be correct in some cases (e.g. 0px available space)
    return inlineContentBreaker().processInlineContent(continuousInlineContent, lineStatus);
}

void LineBuilder::commitCandidateContent(LineCandidate& lineCandidate, std::optional<InlineContentBreaker::Result::PartialTrailingContent> partialTrailingContent)
{
    auto& inlineContent = lineCandidate.inlineContent;
    auto& runs = inlineContent.continuousContent().runs();
    if (runs.isEmpty()) {
        ASSERT(!partialTrailingContent);
        return;
    }

    auto shapingBoundaryStart = std::optional<size_t> { };
    auto appendRun = [&](auto& index) {
        auto& run = runs[index];
        auto& inlineItem = run.inlineItem;

        if (inlineItem.bidiLevel() != UBIDI_DEFAULT_LTR)
            m_line.setContentNeedsBidiReordering();

        if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem)) {
            auto shapingBoundary = [&]() -> std::optional<Line::ShapingBoundary> {
                if (!layoutState().shouldShapeTextAcrossInlineBoxes())
                    return { };

                // Special case trailing partial run as shaping end.
                if (shapingBoundaryStart && partialTrailingContent && partialTrailingContent->trailingRunIndex == index)
                    return { Line::ShapingBoundary::End };

                if (run.shapingBoundary == InlineContentBreaker::ContinuousContent::Run::ShapingBoundary::Start) {
                    ASSERT(!shapingBoundaryStart);
                    shapingBoundaryStart = index;
                    return { Line::ShapingBoundary::Start };
                }

                if (run.shapingBoundary == InlineContentBreaker::ContinuousContent::Run::ShapingBoundary::End) {
                    ASSERT(shapingBoundaryStart);
                    shapingBoundaryStart = { };
                    return { Line::ShapingBoundary::End };
                }

                if (shapingBoundaryStart)
                    return { Line::ShapingBoundary::Middle };

                return { };
            };
            m_line.appendText(*inlineTextItem, run.style, run.contentWidth(), shapingBoundary());
            return;
        }

        if (inlineItem.isLineBreak()) {
            m_line.appendLineBreak(inlineItem, run.style);
            return;
        }

        if (inlineItem.isWordBreakOpportunity()) {
            m_line.appendWordBreakOpportunity(inlineItem, run.style);
            return;
        }

        if (inlineItem.isInlineBoxStart()) {
            m_line.appendInlineBoxStart(inlineItem, run.style, run.contentWidth(), run.textSpacingAdjustment);
            return;
        }

        if (inlineItem.isInlineBoxEnd()) {
            m_line.appendInlineBoxEnd(inlineItem, run.style, run.contentWidth());
            return;
        }

        if (inlineItem.isAtomicInlineBox()) {
            m_line.appendAtomicInlineBox(inlineItem, run.style, run.contentWidth());
            return;
        }

        if (inlineItem.isOpaque()) {
            ASSERT(!run.contentWidth());
            m_line.appendOpaqueBox(inlineItem, run.style);
            return;
        }

        ASSERT_NOT_REACHED();
    };

    if (partialTrailingContent && inlineContent.continuousContent().hasShapedContent())
        shapePartialLineCandidate(lineCandidate, partialTrailingContent->trailingRunIndex);

    ASSERT(!partialTrailingContent || partialTrailingContent->trailingRunIndex <= runs.size());
    auto endOfNonPartialContent = (partialTrailingContent ? std::min(partialTrailingContent->trailingRunIndex, runs.size()) : runs.size());
    for (size_t index = 0; index < endOfNonPartialContent; ++index)
        appendRun(index);

    if (partialTrailingContent) {
        auto trailingRunIndex = partialTrailingContent->trailingRunIndex;
        if (trailingRunIndex >= runs.size()) {
            ASSERT_NOT_REACHED();
            return;
        }

        if (auto partialRun = partialTrailingContent->partialRun) {
            // Create and commit partial trailing item.
            if (auto* trailingInlineTextItem = dynamicDowncast<InlineTextItem>(runs[trailingRunIndex].inlineItem)) {
                auto partialTrailingTextItem = trailingInlineTextItem->left(partialRun->length);
                m_line.appendText(partialTrailingTextItem, trailingInlineTextItem->style(), partialRun->logicalWidth, shapingBoundaryStart ? std::make_optional(Line::ShapingBoundary::End) : std::nullopt);
                if (trailingInlineTextItem->bidiLevel() != UBIDI_DEFAULT_LTR)
                    m_line.setContentNeedsBidiReordering();
            } else
                ASSERT_NOT_REACHED();

            if (auto hyphenWidth = partialRun->hyphenWidth)
                m_line.addTrailingHyphen(*hyphenWidth);
        } else {
            appendRun(trailingRunIndex);
            if (auto hyphenWidth = partialTrailingContent->hyphenWidth)
                m_line.addTrailingHyphen(*hyphenWidth);
        }
    }
}

LineBuilder::Result LineBuilder::processLineBreakingResult(LineCandidate& lineCandidate, const InlineItemRange& layoutRange, const InlineContentBreaker::Result& lineBreakingResult)
{
    auto& candidateRuns = lineCandidate.inlineContent.continuousContent().runs();

    switch (lineBreakingResult.action) {
    case InlineContentBreaker::Result::Action::Keep: {
        // This continuous content can be fully placed on the current line.
        commitCandidateContent(lineCandidate, lineBreakingResult.partialTrailingContent);
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
            auto& parentStyle = isFirstFormattedLineCandidate() ? layoutBoxParent.firstLineStyle() : layoutBoxParent.style();

            auto isWrapOpportunity = TextUtil::isWrappingAllowed(parentStyle);
            if (!isWrapOpportunity && trailingInlineItem.isInlineBoxStartOrEnd())
                isWrapOpportunity = TextUtil::isWrappingAllowed(trailingRun.style);
            if (isWrapOpportunity)
                m_wrapOpportunityList.append(&trailingInlineItem);
        }
        return { lineBreakingResult.isEndOfLine, { candidateRuns.size(), false } };
    }
    case InlineContentBreaker::Result::Action::Wrap: {
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
        return { InlineContentBreaker::IsEndOfLine::Yes, { }, { }, overflowWidthAsLeadingForNextLine(candidateRuns, lineBreakingResult) };
    }
    case InlineContentBreaker::Result::Action::WrapWithHyphen:
        ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        // This continuous content can't be placed on the current line, nothing to commit.
        // However we need to make sure that the current line gains a trailing hyphen.
        ASSERT(m_line.trailingSoftHyphenWidth());
        m_line.addTrailingHyphen(*m_line.trailingSoftHyphenWidth());
        return { InlineContentBreaker::IsEndOfLine::Yes };
    case InlineContentBreaker::Result::Action::RevertToLastWrapOpportunity:
        ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        // Not only this content can't be placed on the current line, but we even need to revert the line back to an earlier position.
        ASSERT(!m_wrapOpportunityList.isEmpty());
        return { InlineContentBreaker::IsEndOfLine::Yes, { rebuildLineWithInlineContent(layoutRange, *m_wrapOpportunityList.last()), true } };
    case InlineContentBreaker::Result::Action::RevertToLastNonOverflowingWrapOpportunity:
        ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        ASSERT(!m_wrapOpportunityList.isEmpty());
        if (auto committedCount = rebuildLineForTrailingSoftHyphen(layoutRange))
            return { InlineContentBreaker::IsEndOfLine::Yes, { committedCount, true } };
        return { InlineContentBreaker::IsEndOfLine::Yes };
    case InlineContentBreaker::Result::Action::Break: {
        ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        // Commit the combination of full and partial content on the current line.
        ASSERT(lineBreakingResult.partialTrailingContent);
        commitCandidateContent(lineCandidate, lineBreakingResult.partialTrailingContent);
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
        return { InlineContentBreaker::IsEndOfLine::Yes, { committedInlineItemCount, false }, overflowLength, overflowWidthAsLeadingForNextLine(candidateRuns, lineBreakingResult) };
    }
    }
    ASSERT_NOT_REACHED();
    return { InlineContentBreaker::IsEndOfLine::No };
}

size_t LineBuilder::rebuildLineWithInlineContent(const InlineItemRange& layoutRange, const InlineItem& lastInlineItemToAdd)
{
    ASSERT(!m_wrapOpportunityList.isEmpty());
    m_line.initialize(m_lineSpanningInlineBoxes, isFirstFormattedLineCandidate());

    size_t numberOfFloatsInRange = 0;
    auto endOfCandidateContent = layoutRange.startIndex();
    for (; endOfCandidateContent < layoutRange.endIndex(); ++endOfCandidateContent) {
        if (m_inlineItemList[endOfCandidateContent].isFloat())
            ++numberOfFloatsInRange;
        if (&m_inlineItemList[endOfCandidateContent] == &lastInlineItemToAdd) {
            ++endOfCandidateContent;
            break;
        }
    }
    ASSERT(endOfCandidateContent < layoutRange.endIndex());

    LineCandidate lineCandidate;
    auto canidateStartEndIndex = std::pair<size_t, size_t> { layoutRange.startIndex(), endOfCandidateContent };
    // We might already have added floats. They shrink the available horizontal space for the line.
    // Let's just reuse what the line has at this point.
    candidateContentForLine(lineCandidate, canidateStartEndIndex, layoutRange, m_line.contentLogicalRight(), SkipFloats::Yes);
    auto result = processLineBreakingResult(lineCandidate, layoutRange, { InlineContentBreaker::Result::Action::Keep, InlineContentBreaker::IsEndOfLine::Yes, { }, { } });

    // Remove floats that are outside of this "rebuild" range to ensure we don't add them twice.
    auto unplaceFloatBox = [&](const Box& floatBox) -> bool {
        m_placedFloats.removeFirstMatching([&floatBox](auto& placedFloatItem) {
            return placedFloatItem.layoutBox() == &floatBox;
        });
        return layoutState().placedFloats().remove(floatBox);
    };
    for (auto index = endOfCandidateContent; index < layoutRange.endIndex(); ++index) {
        auto& inlineItem = m_inlineItemList[index];
        if (inlineItem.isFloat() && unplaceFloatBox(inlineItem.layoutBox()))
            break;
    }

    return result.committedCount.value + numberOfFloatsInRange;
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

bool LineBuilder::isLastLineWithInlineContent(const LineContent& lineContent, size_t needsLayoutEnd, const Line::RunList& lineRuns) const
{
    if (lineContent.partialTrailingContentLength)
        return false;
    // FIXME: This needs work with partial layout.
    auto& formattingContext = this->formattingContext();
    if (lineContent.range.endIndex() == needsLayoutEnd) {
        if (!lineContent.range.start) {
            // This is both the first and the last line.
            return true;
        }
        for (auto& lineRun : lineRuns | std::views::reverse) {
            if (Line::Run::isContentfulOrHasDecoration(lineRun, formattingContext))
                return true;
        }
        return false;
    }
    // Look ahead to see if there's more inline type of inline items.
    for (auto i = lineContent.range.endIndex(); i < needsLayoutEnd && i < m_inlineItemList.size(); ++i) {
        if (isContentfulOrHasDecoration(m_inlineItemList[i], formattingContext))
            return false;
    }
    return true;
}

}
}

