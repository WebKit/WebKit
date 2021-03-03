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
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutState.h"
#include "TextUtil.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

static inline bool endsWithSoftWrapOpportunity(const InlineTextItem& currentTextItem, const InlineTextItem& nextInlineTextItem)
{
    ASSERT(!nextInlineTextItem.isWhitespace());
    // We are at the position after a whitespace.
    if (currentTextItem.isWhitespace())
        return true;
    // When both these non-whitespace runs belong to the same layout box, it's guaranteed that
    // they are split at a soft breaking opportunity. See InlineTextItem::moveToNextBreakablePosition.
    if (&currentTextItem.inlineTextBox() == &nextInlineTextItem.inlineTextBox())
        return true;
    // Now we need to collect at least 3 adjacent characters to be able to make a decision whether the previous text item ends with breaking opportunity.
    // [ex-][ample] <- second to last[x] last[-] current[a]
    // We need at least 1 character in the current inline text item and 2 more from previous inline items.
    auto previousContent = currentTextItem.inlineTextBox().content();
    auto lineBreakIterator = LazyLineBreakIterator { nextInlineTextItem.inlineTextBox().content() };
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
    return !TextUtil::findNextBreakablePosition(lineBreakIterator, 0, nextInlineTextItem.style());
}

static inline bool isAtSoftWrapOpportunity(const InlineFormattingContext& inlineFormattingContext, const InlineItem& current, const InlineItem& next)
{
    // "is at" simple means that there's a soft wrap opportunity right after the [current].
    // [text][ ][text][container start]... (<div>text content<span>..</div>)
    // soft wrap indexes: 0 and 1 definitely, 2 depends on the content after the [container start].

    // https://drafts.csswg.org/css-text-3/#line-break-details
    // Figure out if the new incoming content puts the uncommitted content on a soft wrap opportunity.
    // e.g. [container start][prior_continuous_content][container end] (<span>prior_continuous_content</span>)
    // An incoming <img> box would enable us to commit the "<span>prior_continuous_content</span>" content
    // but an incoming text content would not necessarily.
    ASSERT(current.isText() || current.isBox() || current.isFloat());
    ASSERT(next.isText() || next.isBox() || next.isFloat());
    if (current.isText() && next.isText()) {
        auto& currentInlineTextItem = downcast<InlineTextItem>(current);
        auto& nextInlineTextItem = downcast<InlineTextItem>(next);
        if (currentInlineTextItem.isWhitespace()) {
            // [ ][text] : after [whitespace] position is a soft wrap opportunity.
            return true;
        }
        if (nextInlineTextItem.isWhitespace()) {
            // [text][ ] (<span>text</span> )
            // white-space: break-spaces: line breaking opportunity exists after every preserved white space character, but not before.
            return nextInlineTextItem.style().whiteSpace() != WhiteSpace::BreakSpaces;
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
    if (current.isBox() || next.isBox()) {
        auto isImageContent = current.layoutBox().isImage() || next.layoutBox().isImage();
        if (isImageContent)
            return inlineFormattingContext.quirks().hasSoftWrapOpportunityAtImage();
        // [text][container start][container end][inline box] (text<span></span><img>) : there's a soft wrap opportunity between the [text] and [img].
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

        void appendInlineItem(const InlineItem&, InlineLayoutUnit logicalWidth);
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

inline void LineCandidate::InlineContent::appendInlineItem(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    ASSERT(inlineItem.isText() || inlineItem.isBox() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd());
    auto collapsibleWidth = [&]() -> Optional<InlineLayoutUnit> {
        if (!inlineItem.isText())
            return { };
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (inlineTextItem.isWhitespace() && !InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem)) {
            // Fully collapsible trailing content.
            return logicalWidth;
        }
        // Check for partially collapsible content.
        if (m_ignoreTrailingLetterSpacing)
            return { };
        auto letterSpacing = inlineItem.style().letterSpacing();
        if (letterSpacing <= 0)
            return { };
        ASSERT(logicalWidth > letterSpacing);
        return letterSpacing;
    };
    m_continuousContent.append(inlineItem, logicalWidth, collapsibleWidth());
    m_hasInlineLevelBox = m_hasInlineLevelBox || inlineItem.isBox() || inlineItem.isInlineBoxStart();
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
        if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return TextUtil::width(inlineTextItem, contentLogicalLeft);
        return TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + 1, contentLogicalLeft);
    }

    if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity())
        return { };

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = m_inlineFormattingContext.geometryForBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return boxGeometry.marginBoxWidth();

    if (layoutBox.isReplacedBox())
        return boxGeometry.marginBoxWidth();

    if (inlineItem.isInlineBoxStart())
        return boxGeometry.marginStart() + boxGeometry.borderLeft() + boxGeometry.paddingLeft().valueOr(0);

    if (inlineItem.isInlineBoxEnd())
        return boxGeometry.marginEnd() + boxGeometry.borderRight() + boxGeometry.paddingRight().valueOr(0);

    // Non-replaced inline box (e.g. inline-block)
    return boxGeometry.marginBoxWidth();
}

LineBuilder::LineBuilder(InlineFormattingContext& inlineFormattingContext, FloatingState& floatingState, HorizontalConstraints rootHorizontalConstraints, const InlineItems& inlineItems)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineFormattingState(&inlineFormattingContext.formattingState())
    , m_floatingState(&floatingState)
    , m_rootHorizontalConstraints(rootHorizontalConstraints)
    , m_line(inlineFormattingContext)
    , m_inlineItems(inlineItems)
{
}

LineBuilder::LineBuilder(const InlineFormattingContext& inlineFormattingContext, const InlineItems& inlineItems)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_line(inlineFormattingContext)
    , m_inlineItems(inlineItems)
{
}

LineBuilder::LineContent LineBuilder::layoutInlineContent(const InlineItemRange& needsLayoutRange, size_t partialLeadingContentLength, Optional<InlineLayoutUnit> overflowLogicalWidth, const InlineRect& initialLineLogicalRect, bool isFirstLine)
{
    initialize(initialConstraintsForLine(initialLineLogicalRect, isFirstLine));

    auto committedContent = placeInlineContent(needsLayoutRange, partialLeadingContentLength, overflowLogicalWidth);
    auto committedRange = close(needsLayoutRange, committedContent);

    auto isLastLine = isLastLineWithInlineContent(committedRange, needsLayoutRange.end, committedContent.partialTrailingContentLength);
    return LineContent { committedRange, committedContent.partialTrailingContentLength, committedContent.overflowLogicalWidth, m_floats, m_contentIsConstrainedByFloat
        , m_lineLogicalRect.topLeft()
        , m_lineLogicalRect.width()
        , m_line.contentLogicalWidth()
        , isLastLine
        , m_line.runs()};
}

LineBuilder::IntrinsicContent LineBuilder::computedIntrinsicWidth(const InlineItemRange& needsLayoutRange, InlineLayoutUnit availableWidth)
{
    initialize({ { { }, { availableWidth, maxInlineLayoutUnit() } }, false });
    auto committedContent = placeInlineContent(needsLayoutRange, { }, { });
    auto committedRange = close(needsLayoutRange, committedContent);
    return { committedRange, m_line.contentLogicalWidth(), m_floats };
}

void LineBuilder::initialize(const UsedConstraints& lineConstraints)
{
    m_floats.clear();
    m_partialLeadingTextItem = { };
    m_wrapOpportunityList.clear();

    m_line.initialize();
    m_lineLogicalRect = lineConstraints.logicalRect;
    m_contentIsConstrainedByFloat = lineConstraints.isConstrainedByFloat;
}

LineBuilder::CommittedContent LineBuilder::placeInlineContent(const InlineItemRange& needsLayoutRange, size_t partialLeadingContentLength, Optional<InlineLayoutUnit> leadingLogicalWidth)
{
    auto lineCandidate = LineCandidate { layoutState().shouldIgnoreTrailingLetterSpacing() };
    auto inlineContentBreaker = InlineContentBreaker { };

    auto currentItemIndex = needsLayoutRange.start;
    size_t committedInlineItemCount = 0;
    while (currentItemIndex < needsLayoutRange.end) {
        // 1. Collect the set of runs that we can commit to the line as one entity e.g. <span>text_and_span_start_span_end</span>.
        // 2. Apply floats and shrink the available horizontal space e.g. <span>intru_<div style="float: left"></div>sive_float</span>.
        // 3. Check if the content fits the line and commit the content accordingly (full, partial or not commit at all).
        // 4. Return if we are at the end of the line either by not being able to fit more content or because of an explicit line break.
        candidateContentForLine(lineCandidate, currentItemIndex, needsLayoutRange, partialLeadingContentLength, std::exchange(leadingLogicalWidth, WTF::nullopt), m_line.contentLogicalRight());
        // Now check if we can put this content on the current line.
        auto result = Result { };
        if (lineCandidate.floatItem) {
            ASSERT(lineCandidate.inlineContent.isEmpty());
            handleFloatContent(*lineCandidate.floatItem);
            // Floats never terminate the line.
        } else
            result = handleInlineContent(inlineContentBreaker, needsLayoutRange, lineCandidate);
        committedInlineItemCount = result.committedCount.isRevert ? result.committedCount.value : committedInlineItemCount + result.committedCount.value;
        auto& inlineContent = lineCandidate.inlineContent;
        auto inlineContentIsFullyCommitted = inlineContent.continuousContent().runs().size() == result.committedCount.value && !result.partialTrailingContentLength;
        auto isEndOfLine = result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes;

        if (inlineContentIsFullyCommitted) {
            if (auto* wordBreakOpportunity = inlineContent.trailingWordBreakOpportunity()) {
                // <wbr> needs to be on the line as an empty run so that we can construct an inline box and compute basic geometry.
                ++committedInlineItemCount;
                m_line.append(*wordBreakOpportunity, { });
            }
            if (inlineContent.trailingLineBreak()) {
                // Fully committed (or empty) content followed by a line break means "end of line".
                // FIXME: This will put the line break box at the end of the line while in case of some inline boxes, the line break
                // could very well be at an earlier position. This has no visual implications at this point though (only geometry correctness on the line break box).
                // e.g. <span style="border-right: 10px solid green">text<br></span> where the <br>'s horizontal position is before the right border and not after.
                m_line.append(*inlineContent.trailingLineBreak(), { });
                ++committedInlineItemCount;
                isEndOfLine = true;
            }
        }
        if (isEndOfLine) {
            // We can't place any more items on the current line.
            return { committedInlineItemCount, result.partialTrailingContentLength, result.overflowLogicalWidth };
        }
        currentItemIndex = needsLayoutRange.start + committedInlineItemCount + m_floats.size();
        partialLeadingContentLength = { };
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
    auto availableWidth = m_lineLogicalRect.width() - m_line.contentLogicalRight();
    m_line.removeCollapsibleContent(availableWidth);
    auto horizontalAlignment = root().style().textAlign();
    auto runsExpandHorizontally = horizontalAlignment == TextAlignMode::Justify && !isLastLineWithInlineContent(lineRange, needsLayoutRange.end, committedContent.partialTrailingContentLength);
    if (runsExpandHorizontally)
        m_line.applyRunExpansion(m_lineLogicalRect.width() - m_line.contentLogicalRight());
    auto lineEndsWithHyphen = false;
    if (!m_line.runs().isEmpty()) {
        auto& lastTextContent = m_line.runs().last().textContent();
        lineEndsWithHyphen = lastTextContent && lastTextContent->needsHyphen();
    }
    m_successiveHyphenatedLineCount = lineEndsWithHyphen ? m_successiveHyphenatedLineCount + 1 : 0;
    return lineRange;
}

Optional<HorizontalConstraints> LineBuilder::floatConstraints(const InlineRect& lineLogicalRect) const
{
    auto* floatingState = this->floatingState();
    if (!floatingState || floatingState->floats().isEmpty())
        return { };

    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    auto floatingContext = FloatingContext { formattingContext(), *floatingState };
    auto constraints = floatingContext.constraints(toLayoutUnit(lineLogicalRect.top()), toLayoutUnit(lineLogicalRect.bottom()));
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

LineBuilder::UsedConstraints LineBuilder::initialConstraintsForLine(const InlineRect& initialLineLogicalRect, bool isFirstLine) const
{
    auto lineLogicalLeft = initialLineLogicalRect.left();
    auto lineLogicalRight = initialLineLogicalRect.right();
    auto lineIsConstrainedByFloat = false;

    if (auto lineConstraints = floatConstraints(initialLineLogicalRect)) {
        lineLogicalLeft = lineConstraints->logicalLeft;
        lineLogicalRight = lineConstraints->logicalRight();
        lineIsConstrainedByFloat = true;
    }

    auto computedTextIndent = [&]() -> InlineLayoutUnit {
        // text-indent property specifies the indentation applied to lines of inline content in a block.
        // The indent is treated as a margin applied to the start edge of the line box.
        // Unless otherwise specified, only lines that are the first formatted line of an element are affected.
        // For example, the first line of an anonymous block box is only affected if it is the first child of its parent element.
        // FIXME: Add support for each-line.
        // [Integration] root()->parent() would normally produce a valid layout box.
        auto& root = this->root();
        auto isFormattingContextRootCandidateToTextIndent = !root.isAnonymous();
        if (root.isAnonymous()) {
            // Unless otherwise specified by the each-line and/or hanging keywords, only lines that are the first formatted line
            // of an element are affected.
            // For example, the first line of an anonymous block box is only affected if it is the first child of its parent element.
            auto isIntegratedRootBoxFirstChild = layoutState().isIntegratedRootBoxFirstChild();
            if (isIntegratedRootBoxFirstChild == LayoutState::IsIntegratedRootBoxFirstChild::NotApplicable)
                isFormattingContextRootCandidateToTextIndent = root.parent().firstInFlowChild() == &root;
            else
                isFormattingContextRootCandidateToTextIndent = isIntegratedRootBoxFirstChild == LayoutState::IsIntegratedRootBoxFirstChild::Yes;
        }
        if (!isFormattingContextRootCandidateToTextIndent)
            return { };
        auto invertLineRange = false;
#if ENABLE(CSS3_TEXT)
        invertLineRange = root.style().textIndentType() == TextIndentType::Hanging;
#endif
        // text-indent: hanging inverts which lines are affected.
        // inverted line range -> all the lines except the first one.
        // !inverted line range -> first line gets the indent.
        auto shouldIndent = invertLineRange != isFirstLine;
        if (!shouldIndent)
            return { };

        auto textIndent = root.style().textIndent();
        if (textIndent == RenderStyle::initialTextIndent())
            return { };
        return { minimumValueForLength(textIndent, initialLineLogicalRect.width()) };
    };
    lineLogicalLeft += computedTextIndent();
    return UsedConstraints { { initialLineLogicalRect.top(), lineLogicalLeft, lineLogicalRight - lineLogicalLeft, initialLineLogicalRect.height() }, lineIsConstrainedByFloat };
}

void LineBuilder::candidateContentForLine(LineCandidate& lineCandidate, size_t currentInlineItemIndex, const InlineItemRange& layoutRange, size_t partialLeadingContentLength, Optional<InlineLayoutUnit> leadingLogicalWidth, InlineLayoutUnit currentLogicalRight)
{
    ASSERT(currentInlineItemIndex < layoutRange.end);
    lineCandidate.reset();
    // 1. Simply add any overflow content from the previous line to the candidate content. It's always a text content.
    // 2. Find the next soft wrap position or explicit line break.
    // 3. Collect floats between the inline content.
    auto softWrapOpportunityIndex = nextWrapOpportunity(currentInlineItemIndex, layoutRange);
    // softWrapOpportunityIndex == layoutRange.end means we don't have any wrap opportunity in this content.
    ASSERT(softWrapOpportunityIndex <= layoutRange.end);

    if (partialLeadingContentLength) {
        // Handle leading partial content first (overflowing text from the previous line).
        // Construct a partial leading inline item.
        m_partialLeadingTextItem = downcast<InlineTextItem>(m_inlineItems[currentInlineItemIndex]).right(partialLeadingContentLength);
        auto itemWidth = leadingLogicalWidth ? *std::exchange(leadingLogicalWidth, WTF::nullopt) : inlineItemWidth(*m_partialLeadingTextItem, currentLogicalRight);
        lineCandidate.inlineContent.appendInlineItem(*m_partialLeadingTextItem, itemWidth);
        currentLogicalRight += itemWidth;
        ++currentInlineItemIndex;
    }

    for (auto index = currentInlineItemIndex; index < softWrapOpportunityIndex; ++index) {
        auto& inlineItem = m_inlineItems[index];
        if (inlineItem.isFloat()) {
            lineCandidate.floatItem = &inlineItem;
            // This is a soft wrap opportunity, must be the only item in the list.
            ASSERT(currentInlineItemIndex + 1 == softWrapOpportunityIndex);
            continue;
        }
        if (inlineItem.isText() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd() || inlineItem.isBox()) {
            ASSERT(!leadingLogicalWidth || inlineItem.isText());
            auto logicalWidth = leadingLogicalWidth ? *std::exchange(leadingLogicalWidth, WTF::nullopt) : inlineItemWidth(inlineItem, currentLogicalRight);
            lineCandidate.inlineContent.appendInlineItem(inlineItem, logicalWidth);
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

    // [ex-][container start][container end][float][ample] (ex-<span></span><div style="float:left"></div>ample). Wrap index is at [ex-].
    // [ex][container start][amp-][container start][le] (ex<span>amp-<span>ample). Wrap index is at [amp-].
    // [ex-][container start][line break][ample] (ex-<span><br>ample). Wrap index is after [br].
    auto previousInlineItemIndex = Optional<size_t> { };
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
            // [ex-][ample] vs. [ex-][container start][container end][ample]
            // where [ex-] is startContent and [ample] is the nextContent.
            for (auto candidateIndex = *previousInlineItemIndex + 1; candidateIndex < index; ++candidateIndex) {
                if (m_inlineItems[candidateIndex].isInlineBoxStart()) {
                    // inline content and [container start] and [container end] form unbreakable content.
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

    auto& continuousInlineContent = lineCandidate.inlineContent.continuousContent();
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
    auto availableWidth = lineLogicalRectForCandidateContent.width() - m_line.contentLogicalRight();
    // While the floats are not considered to be on the line, they make the line contentful for line breaking.
    auto lineHasContent = !m_line.runs().isEmpty() || m_contentIsConstrainedByFloat;
    auto lineStatus = InlineContentBreaker::LineStatus { m_line.contentLogicalRight(), availableWidth, m_line.trimmableTrailingWidth(), m_line.trailingSoftHyphenWidth(), m_line.isTrailingRunFullyTrimmable(), lineHasContent, !m_wrapOpportunityList.isEmpty() };
    auto result = inlineContentBreaker.processInlineContent(continuousInlineContent, lineStatus);
    auto& candidateRuns = continuousInlineContent.runs();
    if (result.action == InlineContentBreaker::Result::Action::Keep) {
        // This continuous content can be fully placed on the current line.
        m_lineLogicalRect = lineLogicalRectForCandidateContent;
        for (auto& run : candidateRuns)
            m_line.append(run.inlineItem, run.logicalWidth);
        if (lineCandidate.inlineContent.hasTrailingSoftWrapOpportunity()) {
            // Check if we are allowed to wrap at this position.
            auto& trailingItem = candidateRuns.last().inlineItem;
            // FIXME: There must be a way to decide if the trailing run actually ended up on the line.
            // Let's just deal with collapsed leading whitespace for now.
            if (!m_line.runs().isEmpty() && InlineContentBreaker::isWrappingAllowed(trailingItem))
                m_wrapOpportunityList.append(&trailingItem);
        }
        return { result.isEndOfLine, { candidateRuns.size(), false } };
    }

    auto eligibleOverflowWidthAsLeading = [&] () -> Optional<InlineLayoutUnit> {
        // FIXME: Add support for other types of continuous content.
        ASSERT(result.action == InlineContentBreaker::Result::Action::Wrap || result.action == InlineContentBreaker::Result::Action::Break);
        if (candidateRuns.size() != 1 || !candidateRuns.first().inlineItem.isText())
            return { };
        auto& inlineTextItem = downcast<InlineTextItem>(candidateRuns.first().inlineItem);
        if (inlineTextItem.isWhitespace())
            return { };
        if (result.action == InlineContentBreaker::Result::Action::Wrap)
            return candidateRuns.first().logicalWidth;
        if (result.action == InlineContentBreaker::Result::Action::Break && result.partialTrailingContent->partialRun)
            return candidateRuns.first().logicalWidth - result.partialTrailingContent->partialRun->logicalWidth;
        return { };
    };

    if (result.action == InlineContentBreaker::Result::Action::Wrap) {
        ASSERT(result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);
        // This continuous content can't be placed on the current line. Nothing to commit at this time.
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
        return { InlineContentBreaker::IsEndOfLine::Yes, { rebuildLineForTrailingSoftHyphen(layoutRange), true } };
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
                m_line.append(partialTrailingTextItem, partialRun->logicalWidth);
                if (auto hyphenWidth = partialRun->hyphenWidth)
                    m_line.addTrailingHyphen(*hyphenWidth);
                return;
            }
            // The partial run is the last content to commit.
            m_line.append(run.inlineItem, run.logicalWidth);
            return;
        }
        m_line.append(run.inlineItem, run.logicalWidth);
    }
}

size_t LineBuilder::rebuildLine(const InlineItemRange& layoutRange, const InlineItem& lastInlineItemToAdd)
{
    ASSERT(!m_wrapOpportunityList.isEmpty());
    // We might already have added floats. They shrink the available horizontal space for the line.
    // Let's just reuse what the line has at this point.
    m_line.initialize();
    auto currentItemIndex = layoutRange.start;
    if (m_partialLeadingTextItem) {
        m_line.append(*m_partialLeadingTextItem, inlineItemWidth(*m_partialLeadingTextItem, { }));
        if (&m_partialLeadingTextItem.value() == &lastInlineItemToAdd)
            return 1;
        ++currentItemIndex;
    }
    for (; currentItemIndex < layoutRange.end; ++currentItemIndex) {
        auto& inlineItem = m_inlineItems[currentItemIndex];
        m_line.append(inlineItem, inlineItemWidth(inlineItem, m_line.contentLogicalRight()));
        if (&inlineItem == &lastInlineItemToAdd)
            return currentItemIndex - layoutRange.start + 1;
    }
    return layoutRange.size();
}

size_t LineBuilder::rebuildLineForTrailingSoftHyphen(const InlineItemRange& layoutRange)
{
    ASSERT(!m_wrapOpportunityList.isEmpty());
    // Revert all the way back to a wrap opportunity when either a soft hyphen fits or no hyphen is required.
    for (auto i = m_wrapOpportunityList.size(); i-- > 1;) {
        auto& softWrapOpportunityItem = *m_wrapOpportunityList[i];
        // FIXME: If this turns out to be a perf issue, we could also traverse the wrap list and keep adding the items
        // while watching the available width very closely.
        auto committedCount = rebuildLine(layoutRange, softWrapOpportunityItem);
        auto availableWidth = m_lineLogicalRect.width() - m_line.contentLogicalRight();
        auto trailingSoftHyphenWidth = m_line.trailingSoftHyphenWidth();
        // Check if the trailing hyphen now fits the line (or we don't need hyhen anymore).
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
