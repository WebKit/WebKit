/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "InlineFormattingContext.h"
#include "InlineFormattingGeometry.h"
#include "InlineFormattingQuirks.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutState.h"
#include "TextUtil.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

struct CommittedContent {
    size_t itemCount { 0 };
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

static inline Vector<int32_t> computedVisualOrder(const Line& line)
{
    if (!line.contentNeedsBidiReordering())
        return { };

    auto& lineRuns = line.runs();
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

    Vector<int32_t> visualOrderList(runLevels.size());
    ubidi_reorderVisual(runLevels.data(), runLevels.size(), visualOrderList.data());
    if (hasOpaqueRun) {
        ASSERT(visualOrderList.size() == runIndexOffsetMap.size());
        for (size_t i = 0; i < runIndexOffsetMap.size(); ++i)
            visualOrderList[i] += runIndexOffsetMap[visualOrderList[i]];
    }
    return visualOrderList;
}

static inline bool endsWithSoftWrapOpportunity(const InlineTextItem& currentTextItem, const InlineTextItem& nextInlineTextItem)
{
    ASSERT(!nextInlineTextItem.isWhitespace());
    // We are at the position after a whitespace.
    if (currentTextItem.isWhitespace())
        return true;
    // When both these non-whitespace runs belong to the same layout box with the same bidi level, it's guaranteed that
    // they are split at a soft breaking opportunity. See InlineItemsBuilder::moveToNextBreakablePosition.
    if (&currentTextItem.inlineTextBox() == &nextInlineTextItem.inlineTextBox()) {
        if (currentTextItem.bidiLevel() == nextInlineTextItem.bidiLevel())
            return true;
        // The bidi boundary may or may not be the reason for splitting the inline text box content.
        // FIXME: We could add a "reason flag" to InlineTextItem to tell why the split happened.
        auto& style = currentTextItem.style();
        auto lineBreakIterator = LazyLineBreakIterator { currentTextItem.inlineTextBox().content(), style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()) };
        auto softWrapOpportunityCandidate = nextInlineTextItem.start();
        return TextUtil::findNextBreakablePosition(lineBreakIterator, softWrapOpportunityCandidate, style) == softWrapOpportunityCandidate;
    }
    // Now we need to collect at least 3 adjacent characters to be able to make a decision whether the previous text item ends with breaking opportunity.
    // [ex-][ample] <- second to last[x] last[-] current[a]
    // We need at least 1 character in the current inline text item and 2 more from previous inline items.
    auto previousContent = currentTextItem.inlineTextBox().content();
    auto currentContent = nextInlineTextItem.inlineTextBox().content();
    if (!previousContent.is8Bit()) {
        // FIXME: Remove this workaround when we move over to a better way of handling prior-context with Unicode.
        // See the templated CharacterType in nextBreakablePosition for last and lastlast characters. 
        currentContent.convertTo16Bit();
    }
    auto& style = nextInlineTextItem.style();
    auto lineBreakIterator = LazyLineBreakIterator { currentContent, style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()) };
    auto previousContentLength = previousContent.length();
    // FIXME: We should look into the entire uncommitted content for more text context.
    UChar lastCharacter = previousContentLength ? previousContent[previousContentLength - 1] : 0;
    if (lastCharacter == softHyphen && currentTextItem.style().hyphens() == Hyphens::None)
        return false;
    UChar secondToLastCharacter = previousContentLength > 1 ? previousContent[previousContentLength - 2] : 0;
    lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
    // Now check if we can break right at the inline item boundary.
    // With the [ex-ample], findNextBreakablePosition should return the startPosition (0).
    // FIXME: Check if there's a more correct way of finding breaking opportunities.
    return !TextUtil::findNextBreakablePosition(lineBreakIterator, 0, style);
}

static inline bool isAtSoftWrapOpportunity(const InlineItem& current, const InlineItem& next)
{
    // FIXME: Transition no-wrapping logic from InlineContentBreaker to here where we compute the soft wrap opportunity indexes.
    // "is at" simple means that there's a soft wrap opportunity right after the [current].
    // [text][ ][text][inline box start]... (<div>text content<span>..</div>)
    // soft wrap indexes: 0 and 1 definitely, 2 depends on the content after the [inline box start].

    // https://drafts.csswg.org/css-text-3/#line-break-details
    // Figure out if the new incoming content puts the uncommitted content on a soft wrap opportunity.
    // e.g. [inline box start][prior_continuous_content][inline box end] (<span>prior_continuous_content</span>)
    // An incoming <img> box would enable us to commit the "<span>prior_continuous_content</span>" content
    // but an incoming text content would not necessarily.
    ASSERT(current.isText() || current.isBox());
    ASSERT(next.isText() || next.isBox());
    if (current.isText() && next.isText()) {
        auto& currentInlineTextItem = downcast<InlineTextItem>(current);
        auto& nextInlineTextItem = downcast<InlineTextItem>(next);
        if (currentInlineTextItem.isWhitespace() && nextInlineTextItem.isWhitespace()) {
            // <span> </span><span> </span>. Depending on the styles, there may or may not be a soft wrap opportunity between these 2 whitespace content.
            return TextUtil::isWrappingAllowed(currentInlineTextItem.style()) || TextUtil::isWrappingAllowed(nextInlineTextItem.style());
        }
        if (currentInlineTextItem.isWhitespace()) {
            // " <span>text</span>" : after [whitespace] position is a soft wrap opportunity.
            return TextUtil::isWrappingAllowed(currentInlineTextItem.style());
        }
        if (nextInlineTextItem.isWhitespace()) {
            // "<span>text</span> "
            // 'white-space: break-spaces' and '-webkit-line-break: after-white-space': line breaking opportunity exists after every preserved white space character, but not before.
            auto& style = nextInlineTextItem.style();
            return TextUtil::isWrappingAllowed(style) && style.whiteSpace() != WhiteSpace::BreakSpaces && style.lineBreak() != LineBreak::AfterWhiteSpace;
        }
        if (current.style().lineBreak() == LineBreak::Anywhere || next.style().lineBreak() == LineBreak::Anywhere) {
            // There is a soft wrap opportunity around every typographic character unit, including around any punctuation character
            // or preserved white spaces, or in the middle of words.
            return true;
        }
        // Both current and next items are non-whitespace text.
        // [text][text] : is a continuous content.
        // [text-][text] : after [hyphen] position is a soft wrap opportunity.
        return endsWithSoftWrapOpportunity(currentInlineTextItem, nextInlineTextItem);
    }
    if (current.layoutBox().isListMarkerBox() || next.layoutBox().isListMarkerBox())
        return false;
    if (current.isBox() || next.isBox()) {
        // [text][inline box start][inline box end][inline box] (text<span></span><img>) : there's a soft wrap opportunity between the [text] and [img].
        // The line breaking behavior of a replaced element or other atomic inline is equivalent to an ideographic character.
        return true;
    }
    ASSERT_NOT_REACHED();
    return true;
}

struct LineCandidate {
    LineCandidate(bool ignoreTrailingLetterSpacing);

    void reset();

    struct InlineContent {
        InlineContent(bool ignoreTrailingLetterSpacing);

        const InlineContentBreaker::ContinuousContent& continuousContent() const { return m_continuousContent; }
        const InlineItem* trailingLineBreak() const { return m_trailingLineBreak; }
        const InlineItem* trailingWordBreakOpportunity() const { return m_trailingWordBreakOpportunity; }

        void appendInlineItem(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
        void appendTrailingLineBreak(const InlineItem& lineBreakItem) { m_trailingLineBreak = &lineBreakItem; }
        void appendtrailingWordBreakOpportunity(const InlineItem& wordBreakItem) { m_trailingWordBreakOpportunity = &wordBreakItem; }
        void reset();
        bool isEmpty() const { return m_continuousContent.runs().isEmpty() && !trailingWordBreakOpportunity() && !trailingLineBreak(); }
        bool hasInlineLevelBox() const { return m_hasInlineLevelBox; }

        void setHasTrailingSoftWrapOpportunity(bool hasTrailingSoftWrapOpportunity) { m_hasTrailingSoftWrapOpportunity = hasTrailingSoftWrapOpportunity; }
        bool hasTrailingSoftWrapOpportunity() const { return m_hasTrailingSoftWrapOpportunity; }

        void setHangingContentWidth(InlineLayoutUnit logicalWidth) { m_continuousContent.setHangingContentWidth(logicalWidth); }

    private:
        bool m_ignoreTrailingLetterSpacing { false };

        InlineContentBreaker::ContinuousContent m_continuousContent;
        const InlineItem* m_trailingLineBreak { nullptr };
        const InlineItem* m_trailingWordBreakOpportunity { nullptr };
        bool m_hasInlineLevelBox { false };
        bool m_hasTrailingSoftWrapOpportunity { false };
    };

    // Candidate content is a collection of inline content or a float box.
    InlineContent inlineContent;
    const InlineItem* floatItem { nullptr };
};

LineCandidate::LineCandidate(bool ignoreTrailingLetterSpacing)
    : inlineContent(ignoreTrailingLetterSpacing)
{
}

LineCandidate::InlineContent::InlineContent(bool ignoreTrailingLetterSpacing)
    : m_ignoreTrailingLetterSpacing(ignoreTrailingLetterSpacing)
{
}

inline void LineCandidate::InlineContent::appendInlineItem(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    ASSERT(inlineItem.isText() || inlineItem.isBox() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd());
    m_hasInlineLevelBox = m_hasInlineLevelBox || inlineItem.isBox() || inlineItem.isInlineBoxStart();

    if (inlineItem.isBox() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd())
        return m_continuousContent.append(inlineItem, style, logicalWidth);

    if (inlineItem.isText()) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto isTrailingHangingContent = inlineTextItem.isWhitespace() && style.whiteSpace() == WhiteSpace::PreWrap;
        auto trimmableWidth = [&]() -> std::optional<InlineLayoutUnit> {
            if (isTrailingHangingContent)
                return { };
            if (inlineTextItem.isFullyTrimmable() || inlineTextItem.isQuirkNonBreakingSpace()) {
                // Fully trimmable trailing content.
                return logicalWidth;
            }
            // Check for partially trimmable content.
            if (m_ignoreTrailingLetterSpacing)
                return { };
            auto letterSpacing = style.letterSpacing();
            if (letterSpacing <= 0)
                return { };
            ASSERT(logicalWidth > letterSpacing);
            return letterSpacing;
        };
        m_continuousContent.appendTextContent(inlineTextItem, style, logicalWidth, trimmableWidth());
        // FIXME: Should reset this hanging content when not trailing anymore (probably never happens though).
        if (isTrailingHangingContent)
            m_continuousContent.setHangingContentWidth(logicalWidth);
        return;
    }
    ASSERT_NOT_REACHED();
}

inline void LineCandidate::InlineContent::reset()
{
    m_continuousContent.reset();
    m_trailingLineBreak = { };
    m_trailingWordBreakOpportunity = { };
    m_hasInlineLevelBox = false;
}

inline void LineCandidate::reset()
{
    floatItem = nullptr;
    inlineContent.reset();
}

InlineLayoutUnit LineBuilder::inlineItemWidth(const InlineItem& inlineItem, InlineLayoutUnit contentLogicalLeft) const
{
    ASSERT(inlineItem.layoutBox().isInlineLevelBox());
    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (auto contentWidth = inlineTextItem.width())
            return *contentWidth;
        auto& fontCascade = isFirstFormattedLine() ? inlineTextItem.firstLineStyle().fontCascade() : inlineTextItem.style().fontCascade();
        if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return TextUtil::width(inlineTextItem, fontCascade, contentLogicalLeft);
        return TextUtil::width(inlineTextItem, fontCascade, inlineTextItem.start(), inlineTextItem.start() + 1, contentLogicalLeft);
    }

    if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity())
        return { };

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = m_inlineFormattingContext.geometryForBox(layoutBox);

    if (layoutBox.isReplacedBox())
        return boxGeometry.marginBoxWidth();

    if (inlineItem.isInlineBoxStart()) {
        auto logicalWidth = boxGeometry.marginStart() + boxGeometry.borderStart() + boxGeometry.paddingStart().value_or(0);
#if ENABLE(CSS_BOX_DECORATION_BREAK)
        auto& style = isFirstFormattedLine() ? inlineItem.firstLineStyle() : inlineItem.style();
        if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
            logicalWidth += boxGeometry.borderEnd() + boxGeometry.paddingEnd().value_or(0_lu);
#endif
        return logicalWidth;
    }

    if (inlineItem.isInlineBoxEnd())
        return boxGeometry.marginEnd() + boxGeometry.borderEnd() + boxGeometry.paddingEnd().value_or(0);

    // Non-replaced inline box (e.g. inline-block)
    return boxGeometry.marginBoxWidth();
}

LineBuilder::LineBuilder(InlineFormattingContext& inlineFormattingContext, BlockLayoutState& blockLayoutState, HorizontalConstraints rootHorizontalConstraints, const InlineItems& inlineItems, std::optional<IntrinsicWidthMode> intrinsicWidthMode)
    : m_intrinsicWidthMode(intrinsicWidthMode)
    , m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineFormattingState(&inlineFormattingContext.formattingState())
    , m_blockLayoutState(&blockLayoutState)
    , m_rootHorizontalConstraints(rootHorizontalConstraints)
    , m_line(inlineFormattingContext)
    , m_inlineItems(inlineItems)
{
}

LineBuilder::LineBuilder(const InlineFormattingContext& inlineFormattingContext, const InlineItems& inlineItems, std::optional<IntrinsicWidthMode> intrinsicWidthMode)
    : m_intrinsicWidthMode(intrinsicWidthMode)
    , m_inlineFormattingContext(inlineFormattingContext)
    , m_line(inlineFormattingContext)
    , m_inlineItems(inlineItems)
{
}

LineBuilder::LineContent LineBuilder::layoutInlineContent(const LineInput& lineInput, const std::optional<PreviousLine>& previousLine)
{
    auto previousLineEndsWithLineBreak = !previousLine ? std::nullopt : std::make_optional(previousLine->endsWithLineBreak);
    initialize(lineInput.initialLogicalRect, initialConstraintsForLine(lineInput.initialLogicalRect, previousLineEndsWithLineBreak), lineInput.needsLayoutRange, previousLine);

    auto committedContent = placeInlineContent(lineInput.needsLayoutRange);
    auto committedRange = close(lineInput.needsLayoutRange, committedContent);

    auto isLastLine = isLastLineWithInlineContent(committedRange, lineInput.needsLayoutRange.endIndex(), committedContent.partialTrailingContentLength);
    auto inlineBaseDirection = m_line.runs().isEmpty() ? TextDirection::LTR : inlineBaseDirectionForLineContent();
    auto contentLogicalLeft = horizontalAlignmentOffset(isLastLine);

    return { committedRange
        , committedContent.overflowLogicalWidth
        , WTFMove(m_placedFloats)
        , WTFMove(m_overflowingFloats)
        , m_lineIsConstrainedByFloat
        , m_lineInitialLogicalLeft + m_initialIntrusiveFloatsWidth
        , m_lineLogicalRect.topLeft()
        , m_lineLogicalRect.width()
        , contentLogicalLeft
        , m_line.contentLogicalWidth()
        , contentLogicalLeft + m_line.contentLogicalRight()
        , { !m_line.isHangingTrailingContentWhitespace(), m_line.hangingTrailingContentWidth() }
        , isFirstFormattedLine() ? LineContent::FirstFormattedLine::WithinIFC : LineContent::FirstFormattedLine::No
        , isLastLine
        , m_line.nonSpanningInlineLevelBoxCount()
        , computedVisualOrder(m_line)
        , inlineBaseDirection
        , m_line.runs() };
}

LineBuilder::IntrinsicContent LineBuilder::computedIntrinsicWidth(const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>& previousLine)
{
    ASSERT(isInIntrinsicWidthMode());
    auto lineLogicalWidth = *intrinsicWidthMode() == IntrinsicWidthMode::Maximum ? maxInlineLayoutUnit() : 0.f;
    auto previousLineEndsWithLineBreak = !previousLine ? std::nullopt : std::make_optional(previousLine->endsWithLineBreak);
    auto initialRect = InlineRect { 0, 0, lineLogicalWidth, 0 };
    auto lineConstraints = initialConstraintsForLine(initialRect, previousLineEndsWithLineBreak);
    initialize(initialRect, lineConstraints, needsLayoutRange, previousLine);

    auto committedContent = placeInlineContent(needsLayoutRange);
    auto committedRange = close(needsLayoutRange, committedContent);
    auto contentWidth = lineConstraints.logicalRect.left() + lineConstraints.marginStart + m_line.contentLogicalWidth();
    return { committedRange, committedContent.overflowLogicalWidth, contentWidth, WTFMove(m_placedFloats) };
}

void LineBuilder::initialize(const InlineRect& initialLineLogicalRect, const UsedConstraints& lineConstraints, const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>& previousLine)
{
    ASSERT(!needsLayoutRange.isEmpty() || (previousLine && !previousLine->overflowingFloats.isEmpty()));

    m_previousLine = previousLine;
    m_placedFloats.clear();
    m_overflowingFloats.clear();
    m_lineSpanningInlineBoxes.clear();
    m_wrapOpportunityList.clear();
    m_overflowingLogicalWidth = { };
    m_partialLeadingTextItem = { };

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

    m_lineInitialLogicalLeft = initialLineLogicalRect.left();
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

CommittedContent LineBuilder::placeInlineContent(const InlineItemRange& needsLayoutRange)
{
    size_t overflowingFloatCount = 0;
    auto placeOverflowingFloatsFromPreviousLine = [&] {
        if (!m_previousLine)
            return;
        while (!m_previousLine->overflowingFloats.isEmpty()) {
            auto isPlaced = tryPlacingFloatBox(*m_previousLine->overflowingFloats[0], LineBoxConstraintApplies::No);
            ASSERT_UNUSED(isPlaced, isPlaced);
            ++overflowingFloatCount;
            m_previousLine->overflowingFloats.remove(0);
        }
    };
    placeOverflowingFloatsFromPreviousLine();

    auto lineCandidate = LineCandidate { layoutState().shouldIgnoreTrailingLetterSpacing() };
    auto inlineContentBreaker = InlineContentBreaker { intrinsicWidthMode() };

    auto currentItemIndex = needsLayoutRange.startIndex();
    size_t committedItemCount = 0;
    while (currentItemIndex < needsLayoutRange.endIndex()) {
        // 1. Collect the set of runs that we can commit to the line as one entity e.g. <span>text_and_span_start_span_end</span>.
        // 2. Apply floats and shrink the available horizontal space e.g. <span>intru_<div style="float: left"></div>sive_float</span>.
        // 3. Check if the content fits the line and commit the content accordingly (full, partial or not commit at all).
        // 4. Return if we are at the end of the line either by not being able to fit more content or because of an explicit line break.
        candidateContentForLine(lineCandidate, currentItemIndex, needsLayoutRange, m_line.contentLogicalRight());
        // Now check if we can put this content on the current line.
        if (auto* floatItem = lineCandidate.floatItem) {
            ASSERT(lineCandidate.inlineContent.isEmpty());
            auto evenOverflowingFloatShouldBePlaced = m_line.runs().isEmpty();
            if (!tryPlacingFloatBox(*floatItem, evenOverflowingFloatShouldBePlaced ? LineBoxConstraintApplies::No : LineBoxConstraintApplies::Yes))
                m_overflowingFloats.append(floatItem);
            ++committedItemCount;
        } else {
            auto result = handleInlineContent(inlineContentBreaker, needsLayoutRange, lineCandidate);
            auto isEndOfLine = result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes;
            if (!result.committedCount.isRevert) {
                committedItemCount += result.committedCount.value;
                auto& inlineContent = lineCandidate.inlineContent;
                auto inlineContentIsFullyCommitted = inlineContent.continuousContent().runs().size() == result.committedCount.value && !result.partialTrailingContentLength;
                if (inlineContentIsFullyCommitted) {
                    if (auto* wordBreakOpportunity = inlineContent.trailingWordBreakOpportunity()) {
                        // <wbr> needs to be on the line as an empty run so that we can construct an inline box and compute basic geometry.
                        ++committedItemCount;
                        m_line.append(*wordBreakOpportunity, wordBreakOpportunity->style(), { });
                    }
                    if (inlineContent.trailingLineBreak()) {
                        // Fully committed (or empty) content followed by a line break means "end of line".
                        // FIXME: This will put the line break box at the end of the line while in case of some inline boxes, the line break
                        // could very well be at an earlier position. This has no visual implications at this point though (only geometry correctness on the line break box).
                        // e.g. <span style="border-right: 10px solid green">text<br></span> where the <br>'s horizontal position is before the right border and not after.
                        auto& trailingLineBreak = *inlineContent.trailingLineBreak();
                        m_line.append(trailingLineBreak, trailingLineBreak.style(), { });
                        ++committedItemCount;
                        isEndOfLine = true;
                    }
                }
            } else
                committedItemCount = result.committedCount.value;

            if (isEndOfLine) {
                // We can't place any more items on the current line.
                return { committedItemCount, result.partialTrailingContentLength, result.overflowLogicalWidth };
            }
        }
        currentItemIndex = needsLayoutRange.startIndex() + committedItemCount;
    }
    // Looks like we've run out of runs.
    ASSERT_UNUSED(overflowingFloatCount, committedItemCount || overflowingFloatCount);
    return { committedItemCount, { } };
}

InlineItemRange LineBuilder::close(const InlineItemRange& needsLayoutRange, const CommittedContent& committedContent)
{
    ASSERT(committedContent.itemCount || !m_placedFloats.isEmpty() || m_lineIsConstrainedByFloat);
    auto lineRange = InlineItemRange { needsLayoutRange.start, { needsLayoutRange.startIndex() + committedContent.itemCount, { } } };
    if (!committedContent.itemCount || committedContent.itemCount == m_placedFloats.size()) {
        // Line is empty, we only managed to place float boxes.
        return lineRange;
    }

    if (committedContent.partialTrailingContentLength) {
        auto trailingInlineItemIndex = lineRange.end.index - 1;
        auto overflowingInlineTextItemLength = downcast<InlineTextItem>(m_inlineItems[trailingInlineItemIndex]).length();
        ASSERT(committedContent.partialTrailingContentLength && committedContent.partialTrailingContentLength < overflowingInlineTextItemLength);
        lineRange.end = { trailingInlineItemIndex, overflowingInlineTextItemLength - committedContent.partialTrailingContentLength };
    }
    ASSERT(lineRange.endIndex() <= needsLayoutRange.endIndex());

    auto isLastLine = isLastLineWithInlineContent(lineRange, needsLayoutRange.endIndex(), committedContent.partialTrailingContentLength);
    auto horizontalAvailableSpace = m_lineLogicalRect.width();
    auto& rootStyle = this->rootStyle();

    auto handleTrailingContent = [&] {
        auto& quirks = m_inlineFormattingContext.formattingQuirks();
        auto lineHasOverflow = [&] {
            return horizontalAvailableSpace < m_line.contentLogicalWidth();
        };
        auto lineEndsWithLineBreak = [&] {
            return !m_line.runs().isEmpty() && m_line.runs().last().isLineBreak();
        };
        auto isLineBreakAfterWhitespace = [&] {
            return (!isLastLine || lineHasOverflow()) && rootStyle.lineBreak() == LineBreak::AfterWhiteSpace;
        };
        auto shouldApplyPreserveTrailingWhitespaceQuirk = [&] {
            return quirks.shouldPreserveTrailingWhitespace(isInIntrinsicWidthMode(), m_line.contentNeedsBidiReordering(), horizontalAvailableSpace < m_line.contentLogicalWidth(), lineEndsWithLineBreak());
        };

        m_line.handleTrailingTrimmableContent(shouldApplyPreserveTrailingWhitespaceQuirk() || isLineBreakAfterWhitespace() ? Line::TrailingContentAction::Preserve : Line::TrailingContentAction::Remove);
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
    return lineRange;
}

FloatingContext::Constraints LineBuilder::floatConstraints(const InlineRect& lineMarginBoxRect) const
{
    if (isInIntrinsicWidthMode() || floatingState()->isEmpty())
        return { };

    return formattingContext().formattingGeometry().floatConstraintsForLine(lineMarginBoxRect.top(), lineMarginBoxRect.height(), FloatingContext { formattingContext(), *floatingState() });
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
            if (inlineItem.isFloat())
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
                if (inlineItem.isFloat())
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
    auto softWrapOpportunityIndex = nextWrapOpportunity(currentInlineItemIndex, layoutRange);
    // softWrapOpportunityIndex == layoutRange.end means we don't have any wrap opportunity in this content.
    ASSERT(softWrapOpportunityIndex <= layoutRange.endIndex());

    auto isLineStart = currentInlineItemIndex == layoutRange.startIndex();
    if (isLineStart && m_partialLeadingTextItem) {
        ASSERT(!m_overflowingLogicalWidth);
        // Handle leading partial content first (overflowing text from the previous line).
        auto itemWidth = inlineItemWidth(*m_partialLeadingTextItem, currentLogicalRight);
        lineCandidate.inlineContent.appendInlineItem(*m_partialLeadingTextItem, m_partialLeadingTextItem->style(), itemWidth);
        currentLogicalRight += itemWidth;
        ++currentInlineItemIndex;
    }

    auto firstInlineTextItemIndex = std::optional<size_t> { };
    auto lastInlineTextItemIndex = std::optional<size_t> { };
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
            auto logicalWidth = m_overflowingLogicalWidth ? *std::exchange(m_overflowingLogicalWidth, std::nullopt) : inlineItemWidth(inlineTextItem, currentLogicalRight);
            lineCandidate.inlineContent.appendInlineItem(inlineTextItem, style, logicalWidth);
            // Word spacing does not make the run longer, but it produces an offset instead. See Line::appendTextContent as well.
            currentLogicalRight += logicalWidth + (inlineTextItem.isWordSeparator() ? style.fontCascade().wordSpacing() : 0.f);
            firstInlineTextItemIndex = firstInlineTextItemIndex.value_or(index);
            lastInlineTextItemIndex = index;
            continue;
        }
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            auto logicalWidth = inlineItemWidth(inlineItem, currentLogicalRight);
            lineCandidate.inlineContent.appendInlineItem(inlineItem, style, logicalWidth);
            currentLogicalRight += logicalWidth;
            continue;
        }
        if (inlineItem.isBox()) {
            auto logicalWidth = inlineItemWidth(inlineItem, currentLogicalRight);
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
                ASSERT(m_inlineItems[i].isInlineBoxEnd());
#endif
            inlineItem.isLineBreak() ? lineCandidate.inlineContent.appendTrailingLineBreak(inlineItem) : lineCandidate.inlineContent.appendtrailingWordBreakOpportunity(inlineItem);
            continue;
        }
        ASSERT_NOT_REACHED();
    }

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

    auto inlineContentEndsInSoftWrapOpportunity = [&] {
        if (!softWrapOpportunityIndex || softWrapOpportunityIndex == layoutRange.endIndex()) {
            // This candidate inline content ends because the entire content ends and not because there's a soft wrap opportunity.
            return false;
        }
        if (m_inlineItems[softWrapOpportunityIndex - 1].isFloat()) {
            // While we stop at floats, they are not considered real soft wrap opportunities. 
            return false;
        }
        // See https://www.w3.org/TR/css-text-3/#line-break-details
        auto& trailingInlineItem = m_inlineItems[softWrapOpportunityIndex - 1];
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
            RELEASE_ASSERT(layoutRange.endIndex() <= m_inlineItems.size());
            for (auto index = softWrapOpportunityIndex; index < layoutRange.endIndex(); ++index) {
                if (m_inlineItems[index].isInlineBoxStart() || m_inlineItems[index].isInlineBoxEnd())
                    continue;
                // FIXME: Check if [non-whitespace][inline-box][no-whitespace] content has rules about it.
                // For now let's say the soft wrap position belongs to the next set of runs when [non-whitespace][inline-box][whitespace], [non-whitespace][inline-box][box] etc.
                return m_inlineItems[index].isText() && !downcast<InlineTextItem>(m_inlineItems[index]).isWhitespace();
            }
            return true;
        }
        if (trailingInlineItem.isInlineBoxStart()) {
            // This is a special case when the inline box's fist child is a float box.
            return false;
        }
        ASSERT_NOT_REACHED();
        return true;
    };
    lineCandidate.inlineContent.setHasTrailingSoftWrapOpportunity(inlineContentEndsInSoftWrapOpportunity());
}

size_t LineBuilder::nextWrapOpportunity(size_t startIndex, const InlineItemRange& layoutRange) const
{
    // 1. Find the start candidate by skipping leading non-content items e.g "<span><span>start". Opportunity is after "<span><span>".
    // 2. Find the end candidate by skipping non-content items inbetween e.g. "<span><span>start</span>end". Opportunity is after "</span>".
    // 3. Check if there's a soft wrap opportunity between the 2 candidate inline items and repeat.
    // 4. Any force line break/explicit wrap content inbetween is considered as wrap opportunity.

    // [ex-][inline box start][inline box end][float][ample] (ex-<span></span><div style="float:left"></div>ample). Wrap index is at [ex-].
    // [ex][inline box start][amp-][inline box start][le] (ex<span>amp-<span>ample). Wrap index is at [amp-].
    // [ex-][inline box start][line break][ample] (ex-<span><br>ample). Wrap index is after [br].
    auto previousInlineItemIndex = std::optional<size_t> { };
    for (auto index = startIndex; index < layoutRange.endIndex(); ++index) {
        auto& inlineItem = m_inlineItems[index];
        if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity()) {
            // We always stop at explicit wrapping opportunities e.g. <br>. However the wrap position may be at later position.
            // e.g. <span><span><br></span></span> <- wrap position is after the second </span>
            // but in case of <span><br><span></span></span> <- wrap position is right after <br>.
            for (++index; index < layoutRange.endIndex() && m_inlineItems[index].isInlineBoxEnd(); ++index) { }
            return index;
        }
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            // Need to see what comes next to decide.
            continue;
        }
        ASSERT(inlineItem.isText() || inlineItem.isBox() || inlineItem.isFloat());
        if (inlineItem.isFloat()) {
            // While floats are not part of the inline content and they are not supposed to introduce soft wrap opportunities,
            // e.g. [text][float box][float box][text][float box][text] is essentially just [text][text][text]
            // figuring out whether a float (or set of floats) should stay on the line or not (and handle potentially out of order inline items)
            // brings in unnecessary complexity.
            // For now let's always treat a float as a soft wrap opportunity.
            auto wrappingPosition = index == startIndex ? std::min(index + 1, layoutRange.endIndex()) : index;
            return wrappingPosition;
        }
        if (!previousInlineItemIndex) {
            previousInlineItemIndex = index;
            continue;
        }
        // At this point previous and current items are not necessarily adjacent items e.g "previous<span>current</span>"
        auto& previousItem = m_inlineItems[*previousInlineItemIndex];
        auto& currentItem = m_inlineItems[index];
        if (isAtSoftWrapOpportunity(previousItem, currentItem)) {
            if (*previousInlineItemIndex + 1 == index && (!previousItem.isText() || !currentItem.isText())) {
                // We only know the exact soft wrap opportunity index when the previous and current items are next to each other.
                return index;
            }
            // There's a soft wrap opportunity between 'previousInlineItemIndex' and 'index'.
            // Now forward-find from the start position to see where we can actually wrap.
            // [ex-][ample] vs. [ex-][inline box start][inline box end][ample]
            // where [ex-] is previousInlineItemIndex and [ample] is index.

            // inline content and their inline boxes form unbreakable content.
            // ex-<span></span>ample               : wrap opportunity is after "ex-<span></span>".
            // ex-<span>ample                      : wrap opportunity is after "ex-".
            // ex-<span><span></span></span>ample  : wrap opportunity is after "ex-<span><span></span></span>".
            // ex-</span></span>ample              : wrap opportunity is after "ex-</span></span>".
            // ex-</span><span>ample               : wrap opportunity is after "ex-</span>".
            // ex-<span><span>ample                : wrap opportunity is after "ex-".
            struct InlineBoxPosition {
                const Box* inlineBox { nullptr };
                size_t index { 0 };
            };
            Vector<InlineBoxPosition> inlineBoxStack;
            auto start = *previousInlineItemIndex;
            auto end = index;
            // Soft wrap opportunity is at the first inline box that encloses the trailing content.
            for (auto candidateIndex = start + 1; candidateIndex < end; ++candidateIndex) {
                auto& inlineItem = m_inlineItems[candidateIndex];
                ASSERT(inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd());
                if (inlineItem.isInlineBoxStart())
                    inlineBoxStack.append({ &inlineItem.layoutBox(), candidateIndex });
                else if (inlineItem.isInlineBoxEnd() && !inlineBoxStack.isEmpty())
                    inlineBoxStack.removeLast();
            }
            return inlineBoxStack.isEmpty() ? index : inlineBoxStack.first().index;
        }
        previousInlineItemIndex = index;
    }
    return layoutRange.endIndex();
}

static bool shouldDisableHyphenation(const RenderStyle& rootStyle, unsigned successiveHyphenatedLineCount)
{
    unsigned limitLines = rootStyle.hyphenationLimitLines() == RenderStyle::initialHyphenationLimitLines() ? std::numeric_limits<unsigned>::max() : rootStyle.hyphenationLimitLines();
    return successiveHyphenatedLineCount >= limitLines;
}

static inline InlineLayoutUnit availableWidth(const LineCandidate::InlineContent& candidateContent, const Line& line, InlineLayoutUnit availableWidthForContent)
{
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
    // 1. Preferred width computation sums up floats while line breaker subtracts them.
    // 2. Available space is inherently a LayoutUnit based value (coming from block/flex etc layout) and it is the result of a floored float.
    // These can all lead to epsilon-scale differences.
    availableWidthForContent += LayoutUnit::epsilon();
#endif
    auto availableWidth = availableWidthForContent - line.contentLogicalRight();
    auto& inlineBoxListWithClonedDecorationEnd = line.inlineBoxListWithClonedDecorationEnd();
    if (candidateContent.hasInlineLevelBox() && !inlineBoxListWithClonedDecorationEnd.isEmpty()) {
        // We may try to commit a inline box end here which already takes up place implicitly.
        // Let's not account for its logical width twice.
        for (auto& run : candidateContent.continuousContent().runs()) {
            if (run.inlineItem.isInlineBoxEnd())
                availableWidth += inlineBoxListWithClonedDecorationEnd.get(&run.inlineItem.layoutBox());
        }
    }
    return std::isnan(availableWidth) ? maxInlineLayoutUnit() : availableWidth;
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

std::tuple<InlineRect, bool> LineBuilder::lineBoxForCandidateInlineContent(const LineCandidate& lineCandidate) const
{
    auto& inlineContent = lineCandidate.inlineContent;
    // Check if the candidate content would stretch the line and whether additional floats are getting in the way.
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

LayoutUnit LineBuilder::adjustGeometryForInitialLetterIfNeeded(const Box& floatBox)
{
    auto drop = floatBox.style().initialLetterDrop();
    auto isInitialLetter = floatBox.isFloatingPositioned() && floatBox.style().styleType() == PseudoId::FirstLetter && drop;
    if (!isInitialLetter) {
        formattingState()->setClearGapBeforeFirstLine({ });
        return { };
    }
    // Here we try to set the vertical start position for the float in flush with the adjoining text content's cap height.
    // It's a super premature as at this point we don't normally deal with vertical geometry -other than the incoming vertical constraint.
    auto initialLetterCapHeightOffset = formattingContext().formattingQuirks().initialLetterAlignmentOffset(floatBox, rootStyle());
    // While initial-letter based floats do not set their clear property, intrusive floats from sibling IFCs are supposed to be cleared.
    auto intrusiveBottom = blockLayoutState()->intrusiveInitialLetterLogicalBottom();
    if (!initialLetterCapHeightOffset && !intrusiveBottom) {
        formattingState()->setClearGapBeforeFirstLine({ });
        return { };
    }

    auto clearGapBeforeFirstLine = InlineLayoutUnit { };
    if (intrusiveBottom) {
        // When intrusive initial letter is cleared, we introduce a clear gap. This is (with proper floats) normally computed before starting
        // line layout but intrusive initial letters are cleared only when another initial letter shows up. Regular inline content
        // does not need clearance.
        auto intrusiveInitialLetterWidth = std::max(0.f, m_lineLogicalRect.left() - m_lineInitialLogicalLeft);
        m_lineLogicalRect.setLeft(m_lineInitialLogicalLeft);
        m_lineLogicalRect.expandHorizontally(intrusiveInitialLetterWidth);
        clearGapBeforeFirstLine = *intrusiveBottom;
    }

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
        auto additialMarginBefore = numberOfLinesAboveInitialLetter * rootStyle().computedLineHeight();
        auto& floatBoxGeometry = formattingState()->boxGeometry(floatBox);
        floatBoxGeometry.setVerticalMargin({ floatBoxGeometry.marginBefore() + additialMarginBefore, floatBoxGeometry.marginAfter() });
    }

    m_lineLogicalRect.moveVertically(clearGapBeforeFirstLine);
    formattingState()->setClearGapBeforeFirstLine(clearGapBeforeFirstLine);
    return initialLetterCapHeightOffset.value_or(0_lu);
}

bool LineBuilder::tryPlacingFloatBox(const InlineItem& floatItem, LineBoxConstraintApplies lineBoxConstraintApplies)
{
    if (isInIntrinsicWidthMode()) {
        ASSERT_NOT_REACHED();
        // Just ignore floats by pretending they have been placed.
        return true;
    }
    auto& floatBox = floatItem.layoutBox();
    ASSERT(formattingState());
    auto& boxGeometry = formattingState()->boxGeometry(floatBox);
    auto shouldBePlaced = [&] {
        // Floats never terminate the line. If a float does not fit the current line
        // we can still continue placing inline content on the line, but we have to save all the upcoming floats for subsequent lines.
        auto lineHasSkippedFloats = !m_overflowingFloats.isEmpty();
        if (lineHasSkippedFloats) {
            // Overflowing floats must have been constrained by the current line box.
            ASSERT(lineBoxConstraintApplies == LineBoxConstraintApplies::Yes);
            return false;
        }
        if (lineBoxConstraintApplies == LineBoxConstraintApplies::No) {
            // This is an overflowing float from previous line. Now we need to find a place for it.
            // (which also means that the current line can't have any floats that we couldn't place yet i.e. overflown)
            ASSERT(m_overflowingFloats.isEmpty());
            return true;
        }
        auto lineIsConsideredEmpty = !m_line.hasContent() && !m_lineIsConstrainedByFloat;
        if (lineIsConsideredEmpty)
            return true;
        auto availableWidthForFloat = m_lineLogicalRect.width() - m_line.contentLogicalRight() + m_line.trimmableTrailingWidth();
        return availableWidthForFloat >= boxGeometry.marginBoxWidth();
    };
    if (!shouldBePlaced()) {
        // This float needs to go somewhere else on a subsequent line.
        return false;
    }

    auto additionalOffset = adjustGeometryForInitialLetterIfNeeded(floatBox);
    // Set static position first.
    auto lineMarginBoxLeft = std::max(0.f, m_lineLogicalRect.left() - m_lineMarginStart);
    auto staticPosition = LayoutPoint { lineMarginBoxLeft, m_lineLogicalRect.top() + additionalOffset };
    staticPosition.move(boxGeometry.marginStart(), boxGeometry.marginBefore());
    boxGeometry.setLogicalTopLeft(staticPosition);
    // Float it.
    ASSERT(m_rootHorizontalConstraints);
    auto floatingContext = FloatingContext { formattingContext(), *floatingState() };
    auto floatingPosition = floatingContext.positionForFloat(floatBox, *m_rootHorizontalConstraints);
    boxGeometry.setLogicalTopLeft(floatingPosition);
    auto floatBoxItem = floatingContext.toFloatItem(floatBox);
    auto isLogicalLeftPositionedInFloatingState = floatBoxItem.isLeftPositioned();
    floatingState()->append(floatBoxItem);
    m_placedFloats.append(&floatItem);

    auto intersects = [&] {
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
    };
    if (!intersects()) {
        // This float is placed outside the line. No need to shrink the current line.
        return true;
    }

    // Shrink the line box with the intrusive float box's margin box.
    m_lineIsConstrainedByFloat = true;
    auto shouldAdjustLineLogicalLeft = [&] {
        auto matchingInlineDirection = floatingState()->isLeftToRightDirection() == formattingContext().root().style().isLeftToRightDirection();
        // Floating state inherited from the parent BFC with mismatching inline direction (ltr vs. rtl) puts
        // a right float (float: right in direction: ltr but parent direction: rtl) to logical left.
        return (matchingInlineDirection && isLogicalLeftPositionedInFloatingState) || (!matchingInlineDirection && !isLogicalLeftPositionedInFloatingState);
    };

    // FIXME: In quirks mode some content may sneak above this float.
    if (shouldAdjustLineLogicalLeft()) {
        auto floatLogicalRight = InlineLayoutUnit { floatBoxItem.rectWithMargin().right() };
        auto lineMarginLogicalLeft = m_lineLogicalRect.left() - m_lineMarginStart;
        m_lineLogicalRect.shiftLeftTo(m_lineMarginStart + std::max(lineMarginLogicalLeft, floatLogicalRight));
    } else {
        auto floatLogicalLeft = InlineLayoutUnit { floatBoxItem.rectWithMargin().left() };
        auto shrinkLineBy = m_lineLogicalRect.right() - floatLogicalLeft;
        m_lineLogicalRect.expandHorizontally(std::min(0.f, -shrinkLineBy));
    }
    return true;
}

LineBuilder::Result LineBuilder::handleInlineContent(InlineContentBreaker& inlineContentBreaker, const InlineItemRange& layoutRange, const LineCandidate& lineCandidate)
{
    auto& inlineContent = lineCandidate.inlineContent;
    auto& continuousInlineContent = inlineContent.continuousContent();

    if (continuousInlineContent.runs().isEmpty()) {
        ASSERT(inlineContent.trailingLineBreak() || inlineContent.trailingWordBreakOpportunity());
        return { inlineContent.trailingLineBreak() ? InlineContentBreaker::IsEndOfLine::Yes : InlineContentBreaker::IsEndOfLine::No };
    }
    if (shouldDisableHyphenation(root().style(), m_successiveHyphenatedLineCount))
        inlineContentBreaker.setHyphenationDisabled();

    // While the floats are not considered to be on the line, they make the line contentful for line breaking.
    auto [adjustedLineForCandidateContent, candidateContentIsConstrainedByFloat] = lineBoxForCandidateInlineContent(lineCandidate);
    auto availableWidthForCandidateContent = availableWidth(inlineContent, m_line, adjustedLineForCandidateContent.width());
    auto lineIsConsideredContentful = m_line.hasContent() || m_lineIsConstrainedByFloat || candidateContentIsConstrainedByFloat;
    auto lineStatus = InlineContentBreaker::LineStatus {
        m_line.contentLogicalRight(),
        availableWidthForCandidateContent,
        m_line.trimmableTrailingWidth(),
        m_line.trailingSoftHyphenWidth(),
        m_line.isTrailingRunFullyTrimmable(),
        lineIsConsideredContentful,
        !m_wrapOpportunityList.isEmpty()
    };
    auto toLineBuilderResult = [&](auto& lineBreakingResult) -> LineBuilder::Result {
        auto& candidateRuns = continuousInlineContent.runs();
    
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Keep) {
            // This continuous content can be fully placed on the current line.
            for (auto& run : candidateRuns)
                m_line.append(run.inlineItem, run.style, run.logicalWidth);
            // We are keeping this content on the line but we need to check if we could have wrapped here
            // in order to be able to revert back to this position if needed.
            // Let's just ignore cases like collapsed leading whitespace for now.
            if (lineCandidate.inlineContent.hasTrailingSoftWrapOpportunity() && m_line.hasContent()) {
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

    auto lineBreakingResult = inlineContentBreaker.processInlineContent(continuousInlineContent, lineStatus);
    auto lineGainsNewContent = lineBreakingResult.action == InlineContentBreaker::Result::Action::Keep || lineBreakingResult.action == InlineContentBreaker::Result::Action::Break; 
    if (lineGainsNewContent) {
        // Sometimes in order to put this content on the line, we have to avoid additional float boxes (when the new content is taller than any previous content and we have vertically stacked floats on this line)
        // which means we need to adjust the line rect to accommodate such new constraints.
        m_lineLogicalRect = adjustedLineForCandidateContent;
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
        m_line.append(*m_partialLeadingTextItem, m_partialLeadingTextItem->style(), inlineItemWidth(*m_partialLeadingTextItem, { }));
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
        m_line.append(inlineItem, style, inlineItemWidth(inlineItem, m_line.contentLogicalRight()));
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

bool LineBuilder::isLastLineWithInlineContent(const InlineItemRange& lineRange, size_t lastInlineItemIndex, bool hasPartialTrailingContent) const
{
    if (hasPartialTrailingContent)
        return false;
    if (lineRange.endIndex() == lastInlineItemIndex) {
        // We must have only committed trailing overflowing floats on the line when the range is empty.
        return !lineRange.isEmpty();
    }
    // Omit floats to see if this is the last line with inline content.
    for (auto i = lastInlineItemIndex; i--;) {
        if (!m_inlineItems[i].isFloat())
            return i == lineRange.endIndex() - 1;
    }
    // There has to be at least one non-float item.
    ASSERT_NOT_REACHED();
    return false;
}

TextDirection LineBuilder::inlineBaseDirectionForLineContent() const
{
    ASSERT(!m_line.runs().isEmpty());
    auto shouldUseBlockDirection = rootStyle().unicodeBidi() != UnicodeBidi::Plaintext;
    if (shouldUseBlockDirection)
        return rootStyle().direction();
    // A previous line ending with a line break (<br> or preserved \n) introduces a new unicode paragraph with its own direction.
    if (m_previousLine && !m_previousLine->endsWithLineBreak)
        return m_previousLine->inlineBaseDirection;
    return TextUtil::directionForTextContent(toString(m_line.runs()));
}

InlineLayoutUnit LineBuilder::horizontalAlignmentOffset(bool isLastLine) const
{
    if (m_line.runs().isEmpty())
        return { };

    auto& rootStyle = this->rootStyle();
    auto textAlign = rootStyle.textAlign();
    auto textAlignLast = rootStyle.textAlignLast();
    auto isLeftToRightDirection = inlineBaseDirectionForLineContent() == TextDirection::LTR;

    // Depending on the line’s alignment/justification, the hanging glyph can be placed outside the line box.
    auto& runs = m_line.runs();
    auto contentLogicalRight = m_line.contentLogicalRight();
    auto lineLogicalRight = m_lineLogicalRect.width();

    if (auto hangingTrailingWidth = m_line.hangingTrailingContentWidth()) {
        ASSERT(!runs.isEmpty());
        // If white-space is set to pre-wrap, the UA must (unconditionally) hang this sequence, unless the sequence is followed
        // by a forced line break, in which case it must conditionally hang the sequence is instead.
        // Note that end of last line in a paragraph is considered a forced break.
        auto isConditionalHanging = runs.last().isLineBreak() || isLastLine;
        // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
        if (isConditionalHanging) {
            // FIXME: Conditional hanging needs partial overflow trimming at glyph boundary, one by one until they fit.
            contentLogicalRight = std::min(contentLogicalRight, lineLogicalRight);
        } else
            contentLogicalRight -= hangingTrailingWidth;
    }
    auto extraHorizontalSpace = lineLogicalRight - contentLogicalRight;
    if (extraHorizontalSpace <= 0)
        return { };

    auto computedHorizontalAlignment = [&] {
        // The last line before a forced break or the end of the block is aligned according to
        // text-align-last.
        if (isLastLine || (!runs.isEmpty() && runs.last().isLineBreak())) {
            switch (textAlignLast) {
            case TextAlignLast::Auto:
                if (textAlign == TextAlignMode::Justify)
                    return TextAlignMode::Start;
                return textAlign;
            case TextAlignLast::Start:
                return TextAlignMode::Start;
            case TextAlignLast::End:
                return TextAlignMode::End;
            case TextAlignLast::Left:
                return TextAlignMode::Left;
            case TextAlignLast::Right:
                return TextAlignMode::Right;
            case TextAlignLast::Center:
                return TextAlignMode::Center;
            case TextAlignLast::Justify:
                return TextAlignMode::Justify;
            default:
                ASSERT_NOT_REACHED();
                return TextAlignMode::Start;
            }
        }

        // All other lines are aligned according to text-align.
        return textAlign;
    };

    switch (computedHorizontalAlignment()) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        if (!isLeftToRightDirection)
            return extraHorizontalSpace;
        FALLTHROUGH;
    case TextAlignMode::Start:
        return { };
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        if (!isLeftToRightDirection)
            return { };
        FALLTHROUGH;
    case TextAlignMode::End:
        return extraHorizontalSpace;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return extraHorizontalSpace / 2;
    case TextAlignMode::Justify:
        // TextAlignMode::Justify is a run alignment (and we only do inline box alignment here)
        return { };
    default:
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

const ElementBox& LineBuilder::root() const
{
    return formattingContext().root();
}

const LayoutState& LineBuilder::layoutState() const
{
    return formattingContext().layoutState();
}

const RenderStyle& LineBuilder::rootStyle() const
{
    return isFirstFormattedLine() ? root().firstLineStyle() : root().style();
}

}
}

