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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingContext.h"
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
    if (currentContent.is8Bit() && !previousContent.is8Bit()) {
        // FIXME: Remove this workaround when we move over to a better way of handling prior-context with unicode.
        // See the templated CharacterType in nextBreakablePosition for last and lastlast characters. 
        currentContent = String::make16BitFrom8BitSource(currentContent.characters8(), currentContent.length());
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

static inline bool isAtSoftWrapOpportunity(const InlineFormattingContext& inlineFormattingContext, const InlineItem& current, const InlineItem& next)
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
    ASSERT(current.isText() || current.isBox() || current.isFloat());
    ASSERT(next.isText() || next.isBox() || next.isFloat());
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
            // white-space: break-spaces: line breaking opportunity exists after every preserved white space character, but not before.
            return TextUtil::isWrappingAllowed(nextInlineTextItem.style()) && nextInlineTextItem.style().whiteSpace() != WhiteSpace::BreakSpaces;
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
    if (current.isFloat() || next.isFloat()) {
        // While floats are not part of the inline content and they are not supposed to introduce soft wrap opportunities,
        // e.g. [text][float box][float box][text][float box][text] is essentially just [text][text][text]
        // figuring out whether a float (or set of floats) should stay on the line or not (and handle potentially out of order inline items)
        // brings in unnecessary complexity.
        return true;
    }
    if (current.layoutBox().isListMarkerBox() || next.layoutBox().isListMarkerBox())
        return false;
    if (current.isBox() || next.isBox()) {
        auto isImageContent = current.layoutBox().isImage() || next.layoutBox().isImage();
        if (isImageContent && inlineFormattingContext.layoutState().inQuirksMode())
            return inlineFormattingContext.formattingQuirks().hasSoftWrapOpportunityAtImage();
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
        auto isWhitespace = inlineTextItem.isWhitespace();

        auto isHangingContent = isWhitespace && style.whiteSpace() == WhiteSpace::PreWrap;
        if (isHangingContent)
            return m_continuousContent.append(inlineTextItem, style, logicalWidth);

        auto collapsibleWidth = [&]() -> std::optional<InlineLayoutUnit> {
            if (isWhitespace && !InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem)) {
                // Fully collapsible trailing content.
                return logicalWidth;
            }
            // Check for partially collapsible content.
            if (m_ignoreTrailingLetterSpacing)
                return { };
            auto letterSpacing = style.letterSpacing();
            if (letterSpacing <= 0)
                return { };
            ASSERT(logicalWidth > letterSpacing);
            return letterSpacing;
        };
        m_continuousContent.append(inlineTextItem, style, logicalWidth, collapsibleWidth());
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
    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (auto contentWidth = inlineTextItem.width())
            return *contentWidth;
        auto& fontCascade = isFirstLine() ? inlineTextItem.firstLineStyle().fontCascade() : inlineTextItem.style().fontCascade();
        if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return TextUtil::width(inlineTextItem, fontCascade, contentLogicalLeft);
        return TextUtil::width(inlineTextItem, fontCascade, inlineTextItem.start(), inlineTextItem.start() + 1, contentLogicalLeft);
    }

    if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity())
        return { };

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = m_inlineFormattingContext.geometryForBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return boxGeometry.marginBoxWidth();

    if (layoutBox.isReplacedBox())
        return boxGeometry.marginBoxWidth();

    if (inlineItem.isInlineBoxStart()) {
        auto logicalWidth = boxGeometry.marginStart() + boxGeometry.borderStart() + boxGeometry.paddingStart().value_or(0);
#if ENABLE(CSS_BOX_DECORATION_BREAK)
        auto& style = isFirstLine() ? inlineItem.firstLineStyle() : inlineItem.style();
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

LineBuilder::LineBuilder(InlineFormattingContext& inlineFormattingContext, FloatingState& floatingState, HorizontalConstraints rootHorizontalConstraints, const InlineItems& inlineItems, std::optional<IntrinsicWidthMode> intrinsicWidthMode)
    : m_intrinsicWidthMode(intrinsicWidthMode)
    , m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineFormattingState(&inlineFormattingContext.formattingState())
    , m_floatingState(&floatingState)
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

struct UsedConstraints {
    InlineRect logicalRect;
    InlineLayoutUnit marginStart { 0 };
    bool isConstrainedByFloat { false };
};
LineBuilder::LineContent LineBuilder::layoutInlineContent(const InlineItemRange& needsLayoutRange, const InlineRect& lineLogicalRect, const std::optional<PreviousLine>& previousLine)
{
    auto previousLineEndsWithLineBreak = !previousLine ? std::nullopt : std::make_optional(previousLine->endsWithLineBreak);
    initialize(initialConstraintsForLine(lineLogicalRect, previousLineEndsWithLineBreak), needsLayoutRange.start, previousLine);

    auto committedContent = placeInlineContent(needsLayoutRange);
    auto committedRange = close(needsLayoutRange, committedContent);
    auto& lineRuns = m_line.runs();

    auto computedVisualOrder = [&]() -> Vector<int32_t> {
        if (!m_line.contentNeedsBidiReordering())
            return { };

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
    };

    auto inlineBaseDirectionForLineContent = [&] {
        auto& rootStyle = !previousLine ? root().firstLineStyle() : root().style();
        auto shouldUseBlockDirection = rootStyle.unicodeBidi() != UnicodeBidi::Plaintext;
        if (shouldUseBlockDirection)
            return rootStyle.direction();
        // A previous line ending with a line break (<br> or preserved \n) introduces a new unicode paragraph with its own direction.
        if (previousLine && !previousLine->endsWithLineBreak)
            return previousLine->inlineBaseDirection;
        return TextUtil::directionForTextContent(toString(lineRuns));
    };

    auto isLastLine = isLastLineWithInlineContent(committedRange, needsLayoutRange.end, committedContent.partialTrailingContentLength);
    auto partialOverflowingContent = committedContent.partialTrailingContentLength ? std::make_optional<PartialContent>(committedContent.partialTrailingContentLength, committedContent.overflowLogicalWidth) : std::nullopt;
    return LineContent { committedRange
        , partialOverflowingContent
        , partialOverflowingContent ? std::nullopt : committedContent.overflowLogicalWidth
        , m_floats
        , m_contentIsConstrainedByFloat
        , m_lineMarginStart
        , m_lineLogicalRect.topLeft()
        , m_lineLogicalRect.width()
        , m_line.contentLogicalWidth()
        , m_line.contentLogicalRight()
        , m_line.hangingTrailingContentWidth()
        , isLastLine
        , m_line.nonSpanningInlineLevelBoxCount()
        , computedVisualOrder()
        , inlineBaseDirectionForLineContent()
        , lineRuns };
}

LineBuilder::IntrinsicContent LineBuilder::computedIntrinsicWidth(const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>& previousLine)
{
    ASSERT(isInIntrinsicWidthMode());
    auto lineLogicalWidth = *intrinsicWidthMode() == IntrinsicWidthMode::Maximum ? maxInlineLayoutUnit() : 0.f;
    auto previousLineEndsWithLineBreak = !previousLine ? std::nullopt : std::make_optional(previousLine->endsWithLineBreak);
    auto lineConstraints = initialConstraintsForLine({ 0, 0, lineLogicalWidth, 0 }, previousLineEndsWithLineBreak);
    initialize(lineConstraints, needsLayoutRange.start, previousLine);

    auto committedContent = placeInlineContent(needsLayoutRange);
    auto committedRange = close(needsLayoutRange, committedContent);
    auto lineWidth = lineConstraints.logicalRect.left() + lineConstraints.marginStart + m_line.contentLogicalWidth();
    auto overflow = std::optional<PartialContent> { };
    if (committedContent.partialTrailingContentLength)
        overflow = { committedContent.partialTrailingContentLength, committedContent.overflowLogicalWidth };
    return { committedRange, lineWidth, overflow, m_floats };
}

void LineBuilder::initialize(const UsedConstraints& lineConstraints, size_t leadingInlineItemIndex, const std::optional<PreviousLine>& previousLine)
{
    m_previousLine = previousLine;
    m_floats.clear();
    m_lineSpanningInlineBoxes.clear();
    m_wrapOpportunityList.clear();
    m_overflowingLogicalWidth = { };
    m_partialLeadingTextItem = { };

    auto createLineSpanningInlineBoxes = [&] {
        auto isRootLayoutBox = [&](auto& containerBox) {
            return &containerBox == &root();
        };
        // An inline box may not necessarily start on the current line:
        // <span>first line<br>second line<span>with some more embedding<br> forth line</span></span>
        // We need to make sure that there's an [InlineBoxStart] for every inline box that's present on the current line.
        // We only have to do it on the first run as any subsequent inline content is either at the same/higher nesting level.
        auto& firstInlineItem = m_inlineItems[leadingInlineItemIndex];
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
    m_line.initialize(m_lineSpanningInlineBoxes, m_previousLine && !m_previousLine->endsWithLineBreak);

    m_lineMarginStart = lineConstraints.marginStart;
    m_lineLogicalRect = lineConstraints.logicalRect;
    m_lineLogicalRect.moveHorizontally(m_lineMarginStart);
    // While negative margins normally don't expand the available space, preferred width computation gets confused by negative text-indent
    // (shrink the space needed for the content) which we have to balance it here.
    m_lineLogicalRect.expandHorizontally(-m_lineMarginStart);
    m_contentIsConstrainedByFloat = lineConstraints.isConstrainedByFloat;

    if (previousLine) {
        if (previousLine->partialOverflowingContent) {
            // Turn previous line's overflow content length into the next line's leading content partial length.
            // "sp[<-line break->]lit_content" -> overflow length: 11 -> leading partial content length: 11.
            m_partialLeadingTextItem = downcast<InlineTextItem>(m_inlineItems[leadingInlineItemIndex]).right(previousLine->partialOverflowingContent->length, previousLine->partialOverflowingContent->width);
            ASSERT(!previousLine->trailingOverflowingContentWidth);
        }
        m_overflowingLogicalWidth = previousLine->trailingOverflowingContentWidth;
    }
}

LineBuilder::CommittedContent LineBuilder::placeInlineContent(const InlineItemRange& needsLayoutRange)
{
    auto lineCandidate = LineCandidate { layoutState().shouldIgnoreTrailingLetterSpacing() };
    auto inlineContentBreaker = InlineContentBreaker { intrinsicWidthMode() };

    auto currentItemIndex = needsLayoutRange.start;
    size_t committedInlineItemCount = 0;
    while (currentItemIndex < needsLayoutRange.end) {
        // 1. Collect the set of runs that we can commit to the line as one entity e.g. <span>text_and_span_start_span_end</span>.
        // 2. Apply floats and shrink the available horizontal space e.g. <span>intru_<div style="float: left"></div>sive_float</span>.
        // 3. Check if the content fits the line and commit the content accordingly (full, partial or not commit at all).
        // 4. Return if we are at the end of the line either by not being able to fit more content or because of an explicit line break.
        candidateContentForLine(lineCandidate, currentItemIndex, needsLayoutRange, m_line.contentLogicalRight());
        // Now check if we can put this content on the current line.
        auto result = Result { };
        if (lineCandidate.floatItem) {
            ASSERT(lineCandidate.inlineContent.isEmpty());
            handleFloatContent(*lineCandidate.floatItem);
            // Floats never terminate the line.
        } else
            result = handleInlineContent(inlineContentBreaker, needsLayoutRange, lineCandidate);
        auto isEndOfLine = result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes;
        if (!result.committedCount.isRevert) {
            committedInlineItemCount += result.committedCount.value;
            auto& inlineContent = lineCandidate.inlineContent;
            auto inlineContentIsFullyCommitted = inlineContent.continuousContent().runs().size() == result.committedCount.value && !result.partialTrailingContentLength;
            if (inlineContentIsFullyCommitted) {
                if (auto* wordBreakOpportunity = inlineContent.trailingWordBreakOpportunity()) {
                    // <wbr> needs to be on the line as an empty run so that we can construct an inline box and compute basic geometry.
                    ++committedInlineItemCount;
                    m_line.append(*wordBreakOpportunity, wordBreakOpportunity->style(), { });
                }
                if (inlineContent.trailingLineBreak()) {
                    // Fully committed (or empty) content followed by a line break means "end of line".
                    // FIXME: This will put the line break box at the end of the line while in case of some inline boxes, the line break
                    // could very well be at an earlier position. This has no visual implications at this point though (only geometry correctness on the line break box).
                    // e.g. <span style="border-right: 10px solid green">text<br></span> where the <br>'s horizontal position is before the right border and not after.
                    auto& trailingLineBreak = *inlineContent.trailingLineBreak();
                    m_line.append(trailingLineBreak, trailingLineBreak.style(), { });
                    ++committedInlineItemCount;
                    isEndOfLine = true;
                }
            }
        } else
            committedInlineItemCount = result.committedCount.value;

        if (isEndOfLine) {
            // We can't place any more items on the current line.
            return { committedInlineItemCount, result.partialTrailingContentLength, result.overflowLogicalWidth };
        }
        currentItemIndex = needsLayoutRange.start + committedInlineItemCount + m_floats.size();
    }
    // Looks like we've run out of runs.
    return { committedInlineItemCount, { } };
}

LineBuilder::InlineItemRange LineBuilder::close(const InlineItemRange& needsLayoutRange, const CommittedContent& committedContent)
{
    ASSERT(committedContent.inlineItemCount || !m_floats.isEmpty() || m_contentIsConstrainedByFloat);
    auto numberOfCommittedItems = committedContent.inlineItemCount + m_floats.size();
    auto trailingInlineItemIndex = needsLayoutRange.start + numberOfCommittedItems - 1;
    auto lineRange = InlineItemRange { needsLayoutRange.start, trailingInlineItemIndex + 1 };
    ASSERT(lineRange.end <= needsLayoutRange.end);
    if (!committedContent.inlineItemCount) {
        // Line is empty, we only managed to place float boxes.
        return lineRange;
    }
    auto isLastLine = isLastLineWithInlineContent(lineRange, needsLayoutRange.end, committedContent.partialTrailingContentLength);
    auto horizontalAvailableSpace = m_lineLogicalRect.width();
    auto isInIntrinsicWidthMode = this->isInIntrinsicWidthMode();
    // Legacy line layout quirk: keep the trailing whitespace around when it is followed by a line break, unless the content overflows the line.
    // This quirk however should not be applied when running intrinsic width computation.
    // FIXME: webkit.org/b/233261
    auto shouldApplyTrailingWhiteSpaceFollowedByBRQuirk = isInIntrinsicWidthMode || !layoutState().isInlineFormattingContextIntegration()
        ? Line::ShouldApplyTrailingWhiteSpaceFollowedByBRQuirk::No
        : Line::ShouldApplyTrailingWhiteSpaceFollowedByBRQuirk::Yes;
    m_line.removeTrailingTrimmableContent(shouldApplyTrailingWhiteSpaceFollowedByBRQuirk);

    if (isInIntrinsicWidthMode) {
        // When a glyph at the start or end edge of a line hangs, it is not considered when measuring the line’s contents for fit.
        // https://drafts.csswg.org/css-text/#hanging
        if (*intrinsicWidthMode() == IntrinsicWidthMode::Minimum)
            m_line.removeHangingGlyphs();
        else {
            // Glyphs that conditionally hang are not taken into account when computing min-content sizes and any sizes derived thereof, but they are taken into account for max-content sizes and any sizes derived thereof.
            auto isConditionalHanging = isLastLine || (!m_line.runs().isEmpty() && m_line.runs().last().isLineBreak());
            if (!isConditionalHanging)
                m_line.removeHangingGlyphs();
        }
    }

    auto& rootStyle = root().style();
    // On each line, reset the embedding level of any sequence of whitespace characters at the end of the line
    // to the paragraph embedding level
    m_line.resetBidiLevelForTrailingWhitespace(rootStyle.isLeftToRightDirection() ? UBIDI_LTR : UBIDI_RTL);
    auto runsExpandHorizontally = isLastLine ? rootStyle.textAlignLast() == TextAlignLast::Justify
        : rootStyle.textAlign() == TextAlignMode::Justify;
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

std::optional<HorizontalConstraints> LineBuilder::floatConstraints(const InlineRect& lineLogicalRect) const
{
    auto* floatingState = this->floatingState();
    if (!floatingState || floatingState->floats().isEmpty())
        return { };

    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    auto floatingContext = FloatingContext { formattingContext(), *floatingState };
    auto toLogicalFloatPosition = [&] (const auto& constraints) -> FloatingContext::Constraints {
        if (root().style().isLeftToRightDirection())
            return constraints;
        auto logicalConstraints = FloatingContext::Constraints { };
        auto borderBoxWidth = layoutState().geometryForBox(root()).borderBoxWidth();
        if (constraints.left)
            logicalConstraints.right = PointInContextRoot { borderBoxWidth - constraints.left->x, constraints.left->y };
        if (constraints.right)
            logicalConstraints.left = PointInContextRoot { borderBoxWidth - constraints.right->x, constraints.right->y };
        return logicalConstraints;
    };
    auto constraints = toLogicalFloatPosition(floatingContext.constraints(toLayoutUnit(lineLogicalRect.top()), toLayoutUnit(lineLogicalRect.bottom())));

    // Check if these values actually constrain the line.
    if (constraints.left && constraints.left->x <= lineLogicalRect.left())
        constraints.left = { };

    if (constraints.right && constraints.right->x >= lineLogicalRect.right())
        constraints.right = { };

    if (!constraints.left && !constraints.right)
        return { };

    auto lineLogicalLeft = lineLogicalRect.left();
    auto lineLogicalRight = lineLogicalRect.right();
    if (constraints.left && constraints.right) {
        lineLogicalRight = constraints.right->x;
        lineLogicalLeft = constraints.left->x;
    } else if (constraints.left) {
        ASSERT(constraints.left->x >= lineLogicalLeft);
        lineLogicalLeft = constraints.left->x;
    } else if (constraints.right) {
        // Right float boxes may overflow the containing block on the left.
        lineLogicalRight = std::max<InlineLayoutUnit>(lineLogicalLeft, constraints.right->x);
    }
    return HorizontalConstraints { toLayoutUnit(lineLogicalLeft), toLayoutUnit(lineLogicalRight - lineLogicalLeft) };
}

UsedConstraints LineBuilder::initialConstraintsForLine(const InlineRect& initialLineLogicalRect, std::optional<bool> previousLineEndsWithLineBreak) const
{
    auto lineLogicalLeft = initialLineLogicalRect.left();
    auto lineLogicalRight = initialLineLogicalRect.right();
    auto lineIsConstrainedByFloat = false;

    if (auto lineConstraints = floatConstraints(initialLineLogicalRect)) {
        lineLogicalLeft = lineConstraints->logicalLeft;
        lineLogicalRight = lineConstraints->logicalRight();
        lineIsConstrainedByFloat = true;
    }

    auto isIntrinsicWidthMode = isInIntrinsicWidthMode() ? InlineFormattingGeometry::IsIntrinsicWidthMode::Yes : InlineFormattingGeometry::IsIntrinsicWidthMode::No;
    auto textIndent = formattingContext().formattingGeometry().computedTextIndent(isIntrinsicWidthMode, previousLineEndsWithLineBreak, initialLineLogicalRect.width());
    return UsedConstraints { { initialLineLogicalRect.top(), lineLogicalLeft, lineLogicalRight - lineLogicalLeft, initialLineLogicalRect.height() }, textIndent, lineIsConstrainedByFloat };
}

void LineBuilder::candidateContentForLine(LineCandidate& lineCandidate, size_t currentInlineItemIndex, const InlineItemRange& layoutRange, InlineLayoutUnit currentLogicalRight)
{
    ASSERT(currentInlineItemIndex < layoutRange.end);
    lineCandidate.reset();
    // 1. Simply add any overflow content from the previous line to the candidate content. It's always a text content.
    // 2. Find the next soft wrap position or explicit line break.
    // 3. Collect floats between the inline content.
    auto softWrapOpportunityIndex = nextWrapOpportunity(currentInlineItemIndex, layoutRange);
    // softWrapOpportunityIndex == layoutRange.end means we don't have any wrap opportunity in this content.
    ASSERT(softWrapOpportunityIndex <= layoutRange.end);

    auto isLineStart = currentInlineItemIndex == layoutRange.start;
    if (isLineStart && m_partialLeadingTextItem) {
        ASSERT(!m_overflowingLogicalWidth);
        // Handle leading partial content first (overflowing text from the previous line).
        auto itemWidth = inlineItemWidth(*m_partialLeadingTextItem, currentLogicalRight);
        lineCandidate.inlineContent.appendInlineItem(*m_partialLeadingTextItem, m_partialLeadingTextItem->style(), itemWidth);
        currentLogicalRight += itemWidth;
        ++currentInlineItemIndex;
    }

    for (auto index = currentInlineItemIndex; index < softWrapOpportunityIndex; ++index) {
        auto& inlineItem = m_inlineItems[index];
        auto& style = [&] () -> const RenderStyle& {
            return isFirstLine() ? inlineItem.firstLineStyle() : inlineItem.style();
        }();
        if (inlineItem.isFloat()) {
            lineCandidate.floatItem = &inlineItem;
            // This is a soft wrap opportunity, must be the only item in the list.
            ASSERT(currentInlineItemIndex + 1 == softWrapOpportunityIndex);
            continue;
        }
        if (inlineItem.isText() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            auto logicalWidth = m_overflowingLogicalWidth ? *std::exchange(m_overflowingLogicalWidth, std::nullopt) : inlineItemWidth(inlineItem, currentLogicalRight);
            lineCandidate.inlineContent.appendInlineItem(inlineItem, style, logicalWidth);
            currentLogicalRight += logicalWidth;
            if (is<InlineTextItem>(inlineItem) && downcast<InlineTextItem>(inlineItem).isWordSeparator()) {
                // Word spacing does not make the run longer, but it produces an offset instead. See Line::appendTextContent as well.
                currentLogicalRight += style.fontCascade().wordSpacing();
            }
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
    auto inlineContentEndsInSoftWrapOpportunity = [&] {
        if (!softWrapOpportunityIndex || softWrapOpportunityIndex == layoutRange.end) {
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
            RELEASE_ASSERT(layoutRange.end <= m_inlineItems.size());
            for (auto index = softWrapOpportunityIndex; index < layoutRange.end; ++index) {
                if (m_inlineItems[index].isInlineBoxStart() || m_inlineItems[index].isInlineBoxEnd())
                    continue;
                // FIXME: Check if [non-whitespace][inline-box][no-whitespace] content has rules about it.
                // For now let's say the soft wrap position belongs to the next set of runs when [non-whitespace][inline-box][whitespace], [non-whitespace][inline-box][box] etc.
                return m_inlineItems[index].isText() && !downcast<InlineTextItem>(m_inlineItems[index]).isWhitespace();
            }
            return true;
        }
        ASSERT_NOT_REACHED();
        return true;
    };
    lineCandidate.inlineContent.setHasTrailingSoftWrapOpportunity(inlineContentEndsInSoftWrapOpportunity());
}

size_t LineBuilder::nextWrapOpportunity(size_t startIndex, const LineBuilder::InlineItemRange& layoutRange) const
{
    // 1. Find the start candidate by skipping leading non-content items e.g "<span><span>start". Opportunity is after "<span><span>".
    // 2. Find the end candidate by skipping non-content items inbetween e.g. "<span><span>start</span>end". Opportunity is after "</span>".
    // 3. Check if there's a soft wrap opportunity between the 2 candidate inline items and repeat.
    // 4. Any force line break/explicit wrap content inbetween is considered as wrap opportunity.

    // [ex-][inline box start][inline box end][float][ample] (ex-<span></span><div style="float:left"></div>ample). Wrap index is at [ex-].
    // [ex][inline box start][amp-][inline box start][le] (ex<span>amp-<span>ample). Wrap index is at [amp-].
    // [ex-][inline box start][line break][ample] (ex-<span><br>ample). Wrap index is after [br].
    auto previousInlineItemIndex = std::optional<size_t> { };
    for (auto index = startIndex; index < layoutRange.end; ++index) {
        auto& inlineItem = m_inlineItems[index];
        if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity()) {
            // We always stop at explicit wrapping opportunities e.g. <br>. However the wrap position may be at later position.
            // e.g. <span><span><br></span></span> <- wrap position is after the second </span>
            // but in case of <span><br><span></span></span> <- wrap position is right after <br>.
            for (++index; index < layoutRange.end && m_inlineItems[index].isInlineBoxEnd(); ++index) { }
            return index;
        }
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            // Need to see what comes next to decide.
            continue;
        }
        ASSERT(inlineItem.isText() || inlineItem.isBox() || inlineItem.isFloat());
        if (!previousInlineItemIndex) {
            previousInlineItemIndex = index;
            continue;
        }
        // At this point previous and current items are not necessarily adjacent items e.g "previous<span>current</span>"
        auto& previousItem = m_inlineItems[*previousInlineItemIndex];
        auto& currentItem = m_inlineItems[index];
        if (isAtSoftWrapOpportunity(m_inlineFormattingContext, previousItem, currentItem)) {
            if (*previousInlineItemIndex + 1 == index && (!previousItem.isText() || !currentItem.isText())) {
                // We only know the exact soft wrap opportunity index when the previous and current items are next to each other.
                return index;
            }
            // There's a soft wrap opportunity between 'previousInlineItemIndex' and 'index'.
            // Now forward-find from the start position to see where we can actually wrap.
            // [ex-][ample] vs. [ex-][inline box start][inline box end][ample]
            // where [ex-] is startContent and [ample] is the nextContent.
            for (auto candidateIndex = *previousInlineItemIndex + 1; candidateIndex < index; ++candidateIndex) {
                if (m_inlineItems[candidateIndex].isInlineBoxStart()) {
                    // inline content and [inline box start] and [inline box end] form unbreakable content.
                    // ex-<span></span>ample  : wrap opportunity is after "ex-".
                    // ex-</span></span>ample : wrap opportunity is after "ex-</span></span>".
                    // ex-</span><span>ample</span> : wrap opportunity is after "ex-</span>".
                    // ex-<span><span>ample</span></span> : wrap opportunity is after "ex-".
                    return candidateIndex;
                }
            }
            return index;
        }
        previousInlineItemIndex = index;
    }
    return layoutRange.end;
}

void LineBuilder::handleFloatContent(const InlineItem& floatItem)
{
    auto& floatBox = floatItem.layoutBox();
    m_floats.append(&floatBox);

    auto* floatingState = this->floatingState();
    if (!floatingState)
        return;

    ASSERT(formattingState());
    auto& boxGeometry = formattingState()->boxGeometry(floatBox);
    // Set static position first.
    boxGeometry.setLogicalTopLeft(LayoutPoint { m_lineLogicalRect.topLeft() });
    // Float it.
    ASSERT(m_rootHorizontalConstraints);
    auto floatingContext = FloatingContext { formattingContext(), *floatingState };
    auto floatingPosition = floatingContext.positionForFloat(floatBox, *m_rootHorizontalConstraints);
    boxGeometry.setLogicalTopLeft(floatingPosition);
    floatingState->append(floatingContext.toFloatItem(floatBox));
    // Check if this float shrinks the line (they don't get positioned higher than the line).
    if (floatingPosition.y() > m_lineLogicalRect.bottom())
        return;

    m_contentIsConstrainedByFloat = true;
    auto floatBoxWidth = inlineItemWidth(floatItem, { });
    if (floatBox.isLeftFloatingPositioned())
        m_lineLogicalRect.setLeft(m_lineLogicalRect.left() + floatBoxWidth);
    m_lineLogicalRect.expandHorizontally(-floatBoxWidth);
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

LineBuilder::Result LineBuilder::handleInlineContent(InlineContentBreaker& inlineContentBreaker, const InlineItemRange& layoutRange, const LineCandidate& lineCandidate)
{
    auto& inlineContent = lineCandidate.inlineContent;
    if (inlineContent.continuousContent().runs().isEmpty()) {
        ASSERT(inlineContent.trailingLineBreak() || inlineContent.trailingWordBreakOpportunity());
        return { inlineContent.trailingLineBreak() ? InlineContentBreaker::IsEndOfLine::Yes : InlineContentBreaker::IsEndOfLine::No };
    }
    auto shouldDisableHyphenation = [&] {
        auto& style = root().style();
        unsigned limitLines = style.hyphenationLimitLines() == RenderStyle::initialHyphenationLimitLines() ? std::numeric_limits<unsigned>::max() : style.hyphenationLimitLines();
        return m_successiveHyphenatedLineCount >= limitLines;
    };
    if (shouldDisableHyphenation())
        inlineContentBreaker.setHyphenationDisabled();

    auto& continuousInlineContent = inlineContent.continuousContent();
    auto lineLogicalRectForCandidateContent = [&] {
        // Check if the candidate content would stretch the line and whether additional floats are getting in the way.
        if (!floatingState() || !inlineContent.hasInlineLevelBox())
            return m_lineLogicalRect;
        auto maximumLineLogicalHeight = m_lineLogicalRect.height();
        for (auto& run : continuousInlineContent.runs()) {
            // FIXME: Add support for inline boxes too.
            if (!run.inlineItem.isBox())
                continue;
            maximumLineLogicalHeight = std::max(maximumLineLogicalHeight, InlineLayoutUnit { formattingContext().geometryForBox(run.inlineItem.layoutBox()).marginBoxHeight() });
        }
        if (maximumLineLogicalHeight == m_lineLogicalRect.height())
            return m_lineLogicalRect;
        auto adjustedLineLogicalRect = InlineRect { m_lineLogicalRect.top(), m_lineLogicalRect.left(), m_lineLogicalRect.width(), maximumLineLogicalHeight };
        if (auto horizontalConstraints = floatConstraints(adjustedLineLogicalRect)) {
            adjustedLineLogicalRect.setLeft(horizontalConstraints->logicalLeft);
            adjustedLineLogicalRect.setWidth(horizontalConstraints->logicalWidth);
        }
        return adjustedLineLogicalRect;
    }();
    // While the floats are not considered to be on the line, they make the line contentful for line breaking.
    auto availableWidthForNewContent = availableWidth(inlineContent, m_line, lineLogicalRectForCandidateContent.width());
    auto lineHasContent = m_line.hasContent() || m_contentIsConstrainedByFloat;
    auto trailingContentWidthToIgnore = [&] {
        if (auto trimmableWidth = m_line.trimmableTrailingWidth()) {
            ASSERT(!m_line.hangingTrailingContentWidth());
            return trimmableWidth;
        }
        return m_line.hangingTrailingContentWidth();
    };
    auto lineStatus = InlineContentBreaker::LineStatus { m_line.contentLogicalRight()
        , availableWidthForNewContent
        , trailingContentWidthToIgnore()
        , m_line.trailingSoftHyphenWidth()
        , m_line.isTrailingRunFullyTrimmable()
        , lineHasContent
        , !m_wrapOpportunityList.isEmpty()
    };
    auto result = inlineContentBreaker.processInlineContent(continuousInlineContent, lineStatus);
    auto& candidateRuns = continuousInlineContent.runs();
    if (result.action == InlineContentBreaker::Result::Action::Keep) {
        // This continuous content can be fully placed on the current line.
        m_lineLogicalRect = lineLogicalRectForCandidateContent;
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
            auto& parentStyle = isFirstLine() ? layoutBoxParent.firstLineStyle() : layoutBoxParent.style();

            auto isWrapOpportunity = TextUtil::isWrappingAllowed(parentStyle);
            if (!isWrapOpportunity && (trailingInlineItem.isInlineBoxStart() || trailingInlineItem.isInlineBoxEnd()))
                isWrapOpportunity = TextUtil::isWrappingAllowed(trailingRun.style);
            if (isWrapOpportunity)
                m_wrapOpportunityList.append(&trailingInlineItem);
        }
        return { result.isEndOfLine, { candidateRuns.size(), false } };
    }

    auto eligibleOverflowWidthAsLeading = [&] () -> std::optional<InlineLayoutUnit> {
        // FIXME: Add support for other types of continuous content.
        ASSERT(result.action == InlineContentBreaker::Result::Action::Wrap || result.action == InlineContentBreaker::Result::Action::Break);
        if (candidateRuns.size() != 1 || !candidateRuns.first().inlineItem.isText())
            return { };
        auto& inlineTextItem = downcast<InlineTextItem>(candidateRuns.first().inlineItem);
        if (inlineTextItem.isWhitespace())
            return { };
        auto& overflowingRun = candidateRuns.first();
        if (isFirstLine()) {
            auto& usedStyle = overflowingRun.style;
            auto& style = overflowingRun.inlineItem.style();
            if (&usedStyle != &style && usedStyle.fontCascade() != style.fontCascade()) {
                // We may have the incorrect text width when styles differ. Just re-measure the text content when we place it on the next line.
                return { };
            }
        }
        auto logicalWidthForNextLineAsLeading = overflowingRun.logicalWidth;
        if (result.action == InlineContentBreaker::Result::Action::Wrap)
            return logicalWidthForNextLineAsLeading;
        if (result.action == InlineContentBreaker::Result::Action::Break && result.partialTrailingContent->partialRun)
            return logicalWidthForNextLineAsLeading - result.partialTrailingContent->partialRun->logicalWidth;
        return { };
    };

    if (result.action == InlineContentBreaker::Result::Action::Wrap) {
        ASSERT(result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
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
            return { InlineContentBreaker::IsEndOfLine::Yes, { rebuildLine(layoutRange, *m_wrapOpportunityList.last()), true } };
        }
        return { InlineContentBreaker::IsEndOfLine::Yes, { }, { }, eligibleOverflowWidthAsLeading() };
    }
    if (result.action == InlineContentBreaker::Result::Action::WrapWithHyphen) {
        ASSERT(result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        // This continuous content can't be placed on the current line, nothing to commit.
        // However we need to make sure that the current line gains a trailing hyphen.
        ASSERT(m_line.trailingSoftHyphenWidth());
        m_line.addTrailingHyphen(*m_line.trailingSoftHyphenWidth());
        return { InlineContentBreaker::IsEndOfLine::Yes };
    }
    if (result.action == InlineContentBreaker::Result::Action::RevertToLastWrapOpportunity) {
        ASSERT(result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        // Not only this content can't be placed on the current line, but we even need to revert the line back to an earlier position.
        ASSERT(!m_wrapOpportunityList.isEmpty());
        return { InlineContentBreaker::IsEndOfLine::Yes, { rebuildLine(layoutRange, *m_wrapOpportunityList.last()), true } };
    }
    if (result.action == InlineContentBreaker::Result::Action::RevertToLastNonOverflowingWrapOpportunity) {
        ASSERT(result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        ASSERT(!m_wrapOpportunityList.isEmpty());
        if (auto committedCount = rebuildLineForTrailingSoftHyphen(layoutRange))
            return { InlineContentBreaker::IsEndOfLine::Yes, { committedCount, true } };
        return { InlineContentBreaker::IsEndOfLine::Yes };
    }
    if (result.action == InlineContentBreaker::Result::Action::Break) {
        ASSERT(result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        // Commit the combination of full and partial content on the current line.
        ASSERT(result.partialTrailingContent);
        commitPartialContent(candidateRuns, *result.partialTrailingContent);
        // When breaking multiple runs <span style="word-break: break-all">text</span><span>content</span>, we might end up breaking them at run boundary.
        // It simply means we don't really have a partial run. Partial content yes, but not partial run.
        auto trailingRunIndex = result.partialTrailingContent->trailingRunIndex;
        auto committedInlineItemCount = trailingRunIndex + 1;
        if (!result.partialTrailingContent->partialRun)
            return { InlineContentBreaker::IsEndOfLine::Yes, { committedInlineItemCount, false } };

        auto partialRun = *result.partialTrailingContent->partialRun;
        auto& trailingInlineTextItem = downcast<InlineTextItem>(candidateRuns[trailingRunIndex].inlineItem);
        ASSERT(partialRun.length < trailingInlineTextItem.length());
        auto overflowLength = trailingInlineTextItem.length() - partialRun.length;
        return { InlineContentBreaker::IsEndOfLine::Yes, { committedInlineItemCount, false }, overflowLength, eligibleOverflowWidthAsLeading() };
    }
    ASSERT_NOT_REACHED();
    return { InlineContentBreaker::IsEndOfLine::No };
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

size_t LineBuilder::rebuildLine(const InlineItemRange& layoutRange, const InlineItem& lastInlineItemToAdd)
{
    ASSERT(!m_wrapOpportunityList.isEmpty());
    // We might already have added floats. They shrink the available horizontal space for the line.
    // Let's just reuse what the line has at this point.
    m_line.initialize(m_lineSpanningInlineBoxes, m_previousLine && !m_previousLine->endsWithLineBreak);
    auto currentItemIndex = layoutRange.start;
    if (m_partialLeadingTextItem) {
        m_line.append(*m_partialLeadingTextItem, m_partialLeadingTextItem->style(), inlineItemWidth(*m_partialLeadingTextItem, { }));
        if (&m_partialLeadingTextItem.value() == &lastInlineItemToAdd)
            return 1;
        ++currentItemIndex;
    }
    for (; currentItemIndex < layoutRange.end; ++currentItemIndex) {
        auto& inlineItem = m_inlineItems[currentItemIndex];
        auto& style = isFirstLine() ? inlineItem.firstLineStyle() : inlineItem.style();
        m_line.append(inlineItem, style, inlineItemWidth(inlineItem, m_line.contentLogicalRight()));
        if (&inlineItem == &lastInlineItemToAdd)
            return currentItemIndex - layoutRange.start + 1;
    }
    return layoutRange.size();
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
        auto committedCount = rebuildLine(layoutRange, softWrapOpportunityItem);
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
    auto committedCount = rebuildLine(layoutRange, *m_wrapOpportunityList.first());
    if (auto trailingSoftHyphenWidth = m_line.trailingSoftHyphenWidth())
        m_line.addTrailingHyphen(*trailingSoftHyphenWidth);
    return committedCount;
}

bool LineBuilder::isLastLineWithInlineContent(const InlineItemRange& lineRange, size_t lastInlineItemIndex, bool hasPartialTrailingContent) const
{
    if (hasPartialTrailingContent)
        return false;
    if (lineRange.end == lastInlineItemIndex)
        return true;
    // Omit floats to see if this is the last line with inline content.
    for (auto i = lastInlineItemIndex; i--;) {
        if (!m_inlineItems[i].isFloat())
            return i == lineRange.end - 1;
    }
    // There has to be at least one non-float item.
    ASSERT_NOT_REACHED();
    return false;
}

const ContainerBox& LineBuilder::root() const
{
    return formattingContext().root();
}

const LayoutState& LineBuilder::layoutState() const
{
    return formattingContext().layoutState();
}

}
}

#endif
