/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InlineFormattingUtils.h"

#include "FloatingContext.h"
#include "FontCascade.h"
#include "FormattingContext.h"
#include "InlineDisplayContent.h"
#include "InlineLevelBoxInlines.h"
#include "InlineLineBoxVerticalAligner.h"
#include "InlineQuirks.h"
#include "LayoutElementBox.h"
#include "LengthFunctions.h"
#include "RenderStyleInlines.h"
#include "RubyFormattingContext.h"

namespace WebCore {
namespace Layout {

InlineFormattingUtils::InlineFormattingUtils(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
{
}

InlineLayoutUnit InlineFormattingUtils::logicalTopForNextLine(const LineLayoutResult& lineLayoutResult, const InlineRect& lineLogicalRect, const FloatingContext& floatingContext) const
{
    auto didManageToPlaceInlineContentOrFloat = !lineLayoutResult.inlineItemRange.isEmpty();
    if (didManageToPlaceInlineContentOrFloat) {
        // Normally the next line's logical top is the previous line's logical bottom, but when the line ends
        // with the clear property set, the next line needs to clear the existing floats.
        if (lineLayoutResult.inlineContent.isEmpty())
            return lineLogicalRect.bottom();
        auto& lastRunLayoutBox = lineLayoutResult.inlineContent.last().layoutBox();
        if (!lastRunLayoutBox.hasFloatClear() || lastRunLayoutBox.isOutOfFlowPositioned())
            return lineLogicalRect.bottom();
        auto positionWithClearance = floatingContext.verticalPositionWithClearance(lastRunLayoutBox, formattingContext().geometryForBox(lastRunLayoutBox));
        if (!positionWithClearance)
            return lineLogicalRect.bottom();
        return std::max(lineLogicalRect.bottom(), InlineLayoutUnit(positionWithClearance->position));
    }

    auto intrusiveFloatBottom = [&]() -> std::optional<InlineLayoutUnit> {
        // Floats must have prevented us placing any content on the line.
        // Move next line below the intrusive float(s).
        ASSERT(lineLayoutResult.inlineContent.isEmpty() || lineLayoutResult.inlineContent[0].isLineSpanningInlineBoxStart());
        auto nextLineLogicalTop = [&]() -> LayoutUnit {
            if (auto nextLineLogicalTopCandidate = lineLayoutResult.hintForNextLineTopToAvoidIntrusiveFloat)
                return LayoutUnit { *nextLineLogicalTopCandidate };
            // We have to have a hint when intrusive floats prevented any inline content placement.
            ASSERT_NOT_REACHED();
            return LayoutUnit { lineLogicalRect.top() + formattingContext().root().style().computedLineHeight() };
        };
        auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineLogicalRect.top()), nextLineLogicalTop(), FloatingContext::MayBeAboveLastFloat::Yes);
        if (floatConstraints.left && floatConstraints.right) {
            // In case of left and right constraints, we need to pick the one that's closer to the current line.
            return std::min(floatConstraints.left->y, floatConstraints.right->y);
        }
        if (floatConstraints.left)
            return floatConstraints.left->y;
        if (floatConstraints.right)
            return floatConstraints.right->y;
        // If we didn't manage to place a content on this vertical position due to intrusive floats, we have to have
        // at least one float here.
        ASSERT_NOT_REACHED();
        return { };
    };
    if (auto firstAvailableVerticalPosition = intrusiveFloatBottom())
        return *firstAvailableVerticalPosition;
    // Do not get stuck on the same vertical position even when we find ourselves in this unexpected state.
    return ceil(nextafter(lineLogicalRect.bottom(), std::numeric_limits<float>::max()));
}

ContentWidthAndMargin InlineFormattingUtils::inlineBlockContentWidthAndMargin(const Box&, const HorizontalConstraints&, const OverriddenHorizontalValues&) const
{
    ASSERT_NOT_IMPLEMENTED_YET();
    // 10.3.9 'Inline-block', non-replaced elements in normal flow
    // 10.3.10 'Inline-block', replaced elements in normal flow
    return { };
}

ContentHeightAndMargin InlineFormattingUtils::inlineBlockContentHeightAndMargin(const Box&, const HorizontalConstraints&, const OverriddenVerticalValues&) const
{
    ASSERT_NOT_IMPLEMENTED_YET();
    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    // 10.6.6 Complicated cases
    return { };
}

bool InlineFormattingUtils::inlineLevelBoxAffectsLineBox(const InlineLevelBox& inlineLevelBox) const
{
    if (!inlineLevelBox.mayStretchLineBox())
        return false;

    if (inlineLevelBox.isLineBreakBox())
        return false;
    if (inlineLevelBox.isListMarker())
        return true;
    if (inlineLevelBox.isInlineBox())
        return layoutState().inStandardsMode() ? true : formattingContext().quirks().inlineBoxAffectsLineBox(inlineLevelBox);
    if (inlineLevelBox.isAtomicInlineLevelBox())
        return !inlineLevelBox.layoutBox().isRubyAnnotationBox();
    return false;
}

InlineRect InlineFormattingUtils::flipVisualRectToLogicalForWritingMode(const InlineRect& visualRect, WritingMode writingMode)
{
    switch (writingModeToBlockFlowDirection(writingMode)) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
        return visualRect;
    case BlockFlowDirection::LeftToRight:
    case BlockFlowDirection::RightToLeft: {
        // FIXME: While vertical-lr and vertical-rl modes do differ in the ordering direction of line boxes
        // in a block container (see: https://drafts.csswg.org/css-writing-modes/#block-flow)
        // we ignore it for now as RenderBlock takes care of it for us.
        return InlineRect { visualRect.left(), visualRect.top(), visualRect.height(), visualRect.width() };
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return visualRect;
}

InlineLayoutUnit InlineFormattingUtils::computedTextIndent(IsIntrinsicWidthMode isIntrinsicWidthMode, std::optional<bool> previousLineEndsWithLineBreak, InlineLayoutUnit availableWidth) const
{
    auto& root = formattingContext().root();

    // text-indent property specifies the indentation applied to lines of inline content in a block.
    // The indent is treated as a margin applied to the start edge of the line box.
    // The first formatted line of an element is always indented. For example, the first line of an anonymous block box
    // is only affected if it is the first child of its parent element.
    // If 'each-line' is specified, indentation also applies to all lines where the previous line ends with a hard break.
    // [Integration] root()->parent() would normally produce a valid layout box.
    bool shouldIndent = false;
    if (!previousLineEndsWithLineBreak) {
        shouldIndent = !root.isAnonymous();
        if (root.isAnonymous()) {
            if (!root.isInlineIntegrationRoot())
                shouldIndent = root.parent().firstInFlowChild() == &root;
            else
                shouldIndent = root.isFirstChildForIntegration();
        }
    } else
        shouldIndent = root.style().textIndentLine() == TextIndentLine::EachLine && *previousLineEndsWithLineBreak;

    // Specifying 'hanging' inverts whether the line should be indented or not.
    if (root.style().textIndentType() == TextIndentType::Hanging)
        shouldIndent = !shouldIndent;

    if (!shouldIndent)
        return { };

    auto textIndent = root.style().textIndent();
    if (textIndent == RenderStyle::initialTextIndent())
        return { };
    if (isIntrinsicWidthMode == IsIntrinsicWidthMode::Yes && textIndent.isPercent()) {
        // Percentages must be treated as 0 for the purpose of calculating intrinsic size contributions.
        // https://drafts.csswg.org/css-text/#text-indent-property
        return { };
    }
    return { minimumValueForLength(textIndent, availableWidth) };
}

InlineLayoutUnit InlineFormattingUtils::initialLineHeight(bool isFirstLine) const
{
    if (layoutState().inStandardsMode())
        return isFirstLine ? formattingContext().root().firstLineStyle().computedLineHeight() : formattingContext().root().style().computedLineHeight();
    return formattingContext().quirks().initialLineHeight();
}

FloatingContext::Constraints InlineFormattingUtils::floatConstraintsForLine(InlineLayoutUnit lineLogicalTop, InlineLayoutUnit contentLogicalHeight, const FloatingContext& floatingContext) const
{
    auto logicalTopCandidate = LayoutUnit { lineLogicalTop };
    auto logicalBottomCandidate = LayoutUnit { lineLogicalTop + contentLogicalHeight };
    if (logicalTopCandidate.mightBeSaturated() || logicalBottomCandidate.mightBeSaturated())
        return { };
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    return floatingContext.constraints(logicalTopCandidate, logicalBottomCandidate, FloatingContext::MayBeAboveLastFloat::Yes);
}

InlineLayoutUnit InlineFormattingUtils::horizontalAlignmentOffset(const RenderStyle& rootStyle, InlineLayoutUnit contentLogicalRight, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit hangingTrailingWidth, const Line::RunList& runs, bool isLastLine, std::optional<TextDirection> inlineBaseDirectionOverride)
{
    // Depending on the line’s alignment/justification, the hanging glyph can be placed outside the line box.
    if (hangingTrailingWidth) {
        // If white-space is set to pre-wrap, the UA must (unconditionally) hang this sequence, unless the sequence is followed
        // by a forced line break, in which case it must conditionally hang the sequence is instead.
        // Note that end of last line in a paragraph is considered a forced break.
        auto isConditionalHanging = runs.last().isLineBreak() || isLastLine;
        // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
        if (isConditionalHanging) {
            // FIXME: Conditional hanging needs partial overflow trimming at glyph boundary, one by one until they fit.
            contentLogicalRight = std::min(contentLogicalRight, lineLogicalWidth);
        } else
            contentLogicalRight -= hangingTrailingWidth;
    }

    auto isLastLineOrAfterLineBreak = isLastLine || (!runs.isEmpty() && runs.last().isLineBreak());
    auto horizontalAvailableSpace = lineLogicalWidth - contentLogicalRight;

    if (horizontalAvailableSpace <= 0)
        return { };

    auto isLeftToRightDirection = inlineBaseDirectionOverride.value_or(rootStyle.direction()) == TextDirection::LTR;

    auto computedHorizontalAlignment = [&] {
        auto textAlign = rootStyle.textAlign();
        if (!isLastLineOrAfterLineBreak)
            return textAlign;
        // The last line before a forced break or the end of the block is aligned according to text-align-last.
        switch (rootStyle.textAlignLast()) {
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
    };

    switch (computedHorizontalAlignment()) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        if (!isLeftToRightDirection)
            return horizontalAvailableSpace;
        FALLTHROUGH;
    case TextAlignMode::Start:
        return { };
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        if (!isLeftToRightDirection)
            return { };
        FALLTHROUGH;
    case TextAlignMode::End:
        return horizontalAvailableSpace;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return horizontalAvailableSpace / 2;
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

InlineItemPosition InlineFormattingUtils::leadingInlineItemPositionForNextLine(InlineItemPosition lineContentEnd, std::optional<InlineItemPosition> previousLineContentEnd, bool lineHasIntrusiveFloat, InlineItemPosition layoutRangeEnd)
{
    if (!previousLineContentEnd)
        return lineContentEnd;
    if (previousLineContentEnd->index < lineContentEnd.index || (previousLineContentEnd->index == lineContentEnd.index && previousLineContentEnd->offset < lineContentEnd.offset)) {
        // Either full or partial advancing.
        return lineContentEnd;
    }
    if (lineContentEnd == *previousLineContentEnd && lineHasIntrusiveFloat) {
        // Couldn't manage to put any content on line due to floats.
        return lineContentEnd;
    }
    if (lineContentEnd == layoutRangeEnd) {
        // End of content.
        return layoutRangeEnd;
    }
    // This looks like a partial content and we are stuck. Let's force-move over to the next inline item.
    // We certainly lose some content, but we would be busy looping otherwise.
    ASSERT_NOT_REACHED();
    return { std::min(lineContentEnd.index + 1, layoutRangeEnd.index), { } };
}

InlineLayoutUnit InlineFormattingUtils::inlineItemWidth(const InlineItem& inlineItem, InlineLayoutUnit contentLogicalLeft, bool useFirstLineStyle) const
{
    ASSERT(inlineItem.layoutBox().isInlineLevelBox());
    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (auto contentWidth = inlineTextItem.width())
            return *contentWidth;
        auto& fontCascade = useFirstLineStyle ? inlineTextItem.firstLineStyle().fontCascade() : inlineTextItem.style().fontCascade();
        if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return TextUtil::width(inlineTextItem, fontCascade, contentLogicalLeft);
        return TextUtil::width(inlineTextItem, fontCascade, inlineTextItem.start(), inlineTextItem.start() + 1, contentLogicalLeft);
    }

    if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity())
        return { };

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);

    if (layoutBox.isReplacedBox())
        return boxGeometry.marginBoxWidth();

    if (inlineItem.isInlineBoxStart()) {
        auto logicalWidth = boxGeometry.marginStart() + boxGeometry.borderStart() + boxGeometry.paddingStart();
        auto& style = useFirstLineStyle ? inlineItem.firstLineStyle() : inlineItem.style();
        if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
            logicalWidth += boxGeometry.borderEnd() + boxGeometry.paddingEnd();
        return logicalWidth;
    }

    if (inlineItem.isInlineBoxEnd())
        return boxGeometry.marginEnd() + boxGeometry.borderEnd() + boxGeometry.paddingEnd();

    if (inlineItem.isOpaque())
        return { };

    // Non-replaced inline box (e.g. inline-block)
    return boxGeometry.marginBoxWidth();
}

static inline bool endsWithSoftWrapOpportunity(const InlineTextItem& previousInlineTextItem, const InlineTextItem& nextInlineTextItem)
{
    ASSERT(!nextInlineTextItem.isWhitespace());
    // We are at the position after a whitespace.
    if (previousInlineTextItem.isWhitespace())
        return true;
    // When both these non-whitespace runs belong to the same layout box with the same bidi level, it's guaranteed that
    // they are split at a soft breaking opportunity. See InlineItemsBuilder::moveToNextBreakablePosition.
    if (&previousInlineTextItem.inlineTextBox() == &nextInlineTextItem.inlineTextBox()) {
        if (previousInlineTextItem.bidiLevel() == nextInlineTextItem.bidiLevel())
            return true;
        // The bidi boundary may or may not be the reason for splitting the inline text box content.
        // FIXME: We could add a "reason flag" to InlineTextItem to tell why the split happened.
        auto& style = previousInlineTextItem.style();
        auto lineBreakIteratorFactory = CachedLineBreakIteratorFactory { previousInlineTextItem.inlineTextBox().content(), style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()), TextUtil::contentAnalysis(style.wordBreak()) };
        auto softWrapOpportunityCandidate = nextInlineTextItem.start();
        return TextUtil::findNextBreakablePosition(lineBreakIteratorFactory, softWrapOpportunityCandidate, style) == softWrapOpportunityCandidate;
    }
    return TextUtil::mayBreakInBetween(previousInlineTextItem, nextInlineTextItem);
}

static inline bool isAtSoftWrapOpportunity(const InlineItem& previous, const InlineItem& next)
{
    // FIXME: Transition no-wrapping logic from InlineContentBreaker to here where we compute the soft wrap opportunity indexes.
    // "is at" simple means that there's a soft wrap opportunity right after the [previous].
    // [text][ ][text][inline box start]... (<div>text content<span>..</div>)
    // soft wrap indexes: 0 and 1 definitely, 2 depends on the content after the [inline box start].

    // https://drafts.csswg.org/css-text-3/#line-break-details
    // Figure out if the new incoming content puts the uncommitted content on a soft wrap opportunity.
    // e.g. [inline box start][prior_continuous_content][inline box end] (<span>prior_continuous_content</span>)
    // An incoming <img> box would enable us to commit the "<span>prior_continuous_content</span>" content
    // but an incoming text content would not necessarily.
    ASSERT(previous.isText() || previous.isBox() || previous.layoutBox().isRubyInlineBox());
    ASSERT(next.isText() || next.isBox() || next.layoutBox().isRubyInlineBox());
    if (previous.isText() && next.isText()) {
        auto& currentInlineTextItem = downcast<InlineTextItem>(previous);
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
            return TextUtil::isWrappingAllowed(style) && style.whiteSpaceCollapse() != WhiteSpaceCollapse::BreakSpaces && style.lineBreak() != LineBreak::AfterWhiteSpace;
        }
        if (previous.style().lineBreak() == LineBreak::Anywhere || next.style().lineBreak() == LineBreak::Anywhere) {
            // There is a soft wrap opportunity around every typographic character unit, including around any punctuation character
            // or preserved white spaces, or in the middle of words.
            return true;
        }
        // Both previous and next items are non-whitespace text.
        // [text][text] : is a continuous content.
        // [text-][text] : after [hyphen] position is a soft wrap opportunity.
        auto currentAndNextHaveSameParent = &currentInlineTextItem.layoutBox().parent() == &nextInlineTextItem.layoutBox().parent();
        if (currentAndNextHaveSameParent && !TextUtil::isWrappingAllowed(currentInlineTextItem.style()))
            return false;
        return endsWithSoftWrapOpportunity(currentInlineTextItem, nextInlineTextItem);
    }
    if (previous.layoutBox().isListMarkerBox()) {
        auto& listMarkerBox = downcast<ElementBox>(previous.layoutBox());
        return !listMarkerBox.isListMarkerInsideList() || !listMarkerBox.isListMarkerOutside();
    }
    if (next.layoutBox().isListMarkerBox()) {
        // FIXME: SHould this ever be the case?
        return true;
    }
    if (previous.isBox() || next.isBox()) {
        // [text][inline box start][inline box end][inline box] (text<span></span><img>) : there's a soft wrap opportunity between the [text] and [img].
        // The line breaking behavior of a replaced element or other atomic inline is equivalent to an ideographic character.
        return true;
    }
    if (previous.layoutBox().isRubyInlineBox() || next.layoutBox().isRubyInlineBox())
        return RubyFormattingContext::isAtSoftWrapOpportunity(previous, next);

    ASSERT_NOT_REACHED();
    return true;
}

size_t InlineFormattingUtils::nextWrapOpportunity(size_t startIndex, const InlineItemRange& layoutRange, const InlineItemList& inlineItemList)
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
        auto& currentItem = inlineItemList[index];
        if (currentItem.isLineBreak() || currentItem.isWordBreakOpportunity()) {
            // We always stop at explicit wrapping opportunities e.g. <br>. However the wrap position may be at later position.
            // e.g. <span><span><br></span></span> <- wrap position is after the second </span>
            // but in case of <span><br><span></span></span> <- wrap position is right after <br>.
            for (++index; index < layoutRange.endIndex() && inlineItemList[index].isInlineBoxEnd(); ++index) { }
            return index;
        }
        auto isNonRubyInlineBox = (currentItem.isInlineBoxStart() || currentItem.isInlineBoxEnd()) && !currentItem.layoutBox().isRubyInlineBox();
        if (isNonRubyInlineBox) {
            // Need to see what comes next to decide.
            continue;
        }
        if (currentItem.isOpaque()) {
            // This item is invisible to line breaking. Need to pretend it's not here.
            continue;
        }
        ASSERT(currentItem.isText() || currentItem.isBox() || currentItem.isFloat() || currentItem.layoutBox().isRubyInlineBox());
        if (currentItem.isFloat()) {
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
        auto& previousItem = inlineItemList[*previousInlineItemIndex];
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
                auto& inlineItem = inlineItemList[candidateIndex];
                ASSERT(inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd() || inlineItem.isOpaque());
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

std::pair<InlineLayoutUnit, InlineLayoutUnit> InlineFormattingUtils::textEmphasisForInlineBox(const Box& layoutBox, const ElementBox& rootBox)
{
    // Generic, non-inline box inline-level content (e.g. replaced elements) can't have text-emphasis annotations.
    ASSERT(layoutBox.isInlineBox() || &layoutBox == &rootBox);

    auto& style = layoutBox.style();
    auto hasTextEmphasis =  style.textEmphasisMark() != TextEmphasisMark::None;
    if (!hasTextEmphasis)
        return { };
    auto emphasisPosition = style.textEmphasisPosition();
    // Normally we resolve visual -> logical values at pre-layout time, but emphaisis values are not part of the general box geometry.
    auto hasAboveTextEmphasis = false;
    auto hasUnderTextEmphasis = false;
    if (style.isHorizontalWritingMode()) {
        hasAboveTextEmphasis = emphasisPosition.contains(TextEmphasisPosition::Over);
        hasUnderTextEmphasis = !hasAboveTextEmphasis && emphasisPosition.contains(TextEmphasisPosition::Under);
    } else {
        hasAboveTextEmphasis = emphasisPosition.contains(TextEmphasisPosition::Right) || emphasisPosition == TextEmphasisPosition::Over;
        hasUnderTextEmphasis = !hasAboveTextEmphasis && (emphasisPosition.contains(TextEmphasisPosition::Left) || emphasisPosition == TextEmphasisPosition::Under);
    }

    if (!hasAboveTextEmphasis && !hasUnderTextEmphasis)
        return { };

    auto enclosingRubyBase = [&]() -> const ElementBox* {
        if (&layoutBox == &rootBox)
            return nullptr;
        for (auto* ancestor = &layoutBox.parent(); ancestor != &rootBox; ancestor = &ancestor->parent()) {
            if (ancestor->isRubyBase())
                return ancestor;
        }
        return nullptr;
    };
    if (auto* rubyBase = enclosingRubyBase(); rubyBase && RubyFormattingContext::hasInterlinearAnnotation(*rubyBase)) {
        auto annotationPosition = rubyBase->style().rubyPosition();
        if ((hasAboveTextEmphasis && annotationPosition == RubyPosition::Before) || (hasUnderTextEmphasis && annotationPosition == RubyPosition::After)) {
            // FIXME: Check if annotation box has content.
            return { };
        }
    }
    InlineLayoutUnit annotationSize = roundToInt(style.fontCascade().floatEmphasisMarkHeight(style.textEmphasisMarkString()));
    return { hasAboveTextEmphasis ? annotationSize : 0.f, hasAboveTextEmphasis ? 0.f : annotationSize };
}

LineEndingEllipsisPolicy InlineFormattingUtils::lineEndingEllipsisPolicy(const RenderStyle& rootStyle, size_t numberOfLinesWithInlineContent, std::optional<size_t> numberOfVisibleLinesAllowed)
{
    if (numberOfVisibleLinesAllowed && *numberOfVisibleLinesAllowed == numberOfLinesWithInlineContent)
        return LineEndingEllipsisPolicy::WhenContentOverflowsInBlockDirection;

    // Truncation is in effect when the block container has overflow other than visible.
    if (rootStyle.overflowX() != Overflow::Visible && rootStyle.textOverflow() == TextOverflow::Ellipsis)
        return LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection;
    return LineEndingEllipsisPolicy::NoEllipsis;
}

const InlineLayoutState& InlineFormattingUtils::layoutState() const
{
    return formattingContext().layoutState();
}

}
}

