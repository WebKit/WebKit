/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineDisplayContentBuilder.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "LayoutBoxGeometry.h"
#include "LayoutInitialContainingBlock.h"
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static inline OptionSet<InlineDisplay::Box::PositionWithinInlineLevelBox> isFirstLastBox(const InlineLevelBox& inlineBox)
{
    auto positionWithinInlineLevelBox = OptionSet<InlineDisplay::Box::PositionWithinInlineLevelBox> { };
    if (inlineBox.isFirstBox())
        positionWithinInlineLevelBox.add(InlineDisplay::Box::PositionWithinInlineLevelBox::First);
    if (inlineBox.isLastBox())
        positionWithinInlineLevelBox.add(InlineDisplay::Box::PositionWithinInlineLevelBox::Last);
    return positionWithinInlineLevelBox;
}

InlineDisplayContentBuilder::InlineDisplayContentBuilder(const ContainerBox& formattingContextRoot, InlineFormattingState& formattingState)
    : m_formattingContextRoot(formattingContextRoot)
    , m_formattingState(formattingState)
{
}

DisplayBoxes InlineDisplayContentBuilder::build(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineRect& lineBoxLogicalRect, const size_t lineIndex)
{
    DisplayBoxes boxes;
    boxes.reserveInitialCapacity(lineContent.runs.size() + lineBox.nonRootInlineLevelBoxes().size() + 1);

    m_lineIndex = lineIndex;
    // Every line starts with a root box, even the empty ones.
    auto rootInlineBoxRect = lineBox.logicalRectForRootInlineBox();
    rootInlineBoxRect.moveBy(lineBoxLogicalRect.topLeft());
    boxes.append({ m_lineIndex, InlineDisplay::Box::Type::RootInlineBox, root(), UBIDI_DEFAULT_LTR, rootInlineBoxRect, rootInlineBoxRect, { }, { }, lineBox.rootInlineBox().hasContent() });

    auto contentNeedsBidiReordering = !lineContent.visualOrderList.isEmpty();
    if (contentNeedsBidiReordering)
        processBidiContent(lineContent, lineBox, lineBoxLogicalRect.topLeft(), boxes);
    else
        processNonBidiContent(lineContent, lineBox, lineBoxLogicalRect.topLeft(), boxes);
    processOverflownRunsForEllipsis(boxes, lineBoxLogicalRect.right());
    collectInkOverflowForInlineBoxes(lineBox, boxes);
    return boxes;
}

static inline void addBoxShadowInkOverflow(const RenderStyle& style, InlineRect& inkOverflow)
{
    auto topBoxShadow = LayoutUnit { };
    auto bottomBoxShadow = LayoutUnit { };
    style.getBoxShadowBlockDirectionExtent(topBoxShadow, bottomBoxShadow);

    auto leftBoxShadow = LayoutUnit { };
    auto rightBoxShadow = LayoutUnit { };
    style.getBoxShadowInlineDirectionExtent(leftBoxShadow, rightBoxShadow);
    inkOverflow.inflate(InlineLayoutUnit { topBoxShadow }, InlineLayoutUnit { rightBoxShadow }, InlineLayoutUnit { bottomBoxShadow }, InlineLayoutUnit { leftBoxShadow });
}

void InlineDisplayContentBuilder::appendTextDisplayBox(const Line::Run& lineRun, const InlineRect& textRunRect, DisplayBoxes& boxes)
{
    ASSERT(lineRun.textContent() && is<InlineTextBox>(lineRun.layoutBox()));

    auto& layoutBox = lineRun.layoutBox();
    auto& style = !m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();

    auto inkOverflow = [&] {
        auto initialContaingBlockSize = RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()
            ? formattingState().layoutState().viewportSize()
            : formattingState().layoutState().geometryForBox(layoutBox.initialContainingBlock()).contentBox().size();
        auto strokeOverflow = ceilf(style.computedStrokeWidth(ceiledIntSize(initialContaingBlockSize)));
        auto inkOverflow = textRunRect;
        inkOverflow.inflate(strokeOverflow);
        auto letterSpacing = style.fontCascade().letterSpacing();
        if (letterSpacing < 0) {
            // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
            inkOverflow.expand(-letterSpacing, { });
        }
        return inkOverflow;
    };
    auto content = downcast<InlineTextBox>(layoutBox).content();
    auto text = lineRun.textContent();
    auto adjustedContentToRender = [&] {
        return text->needsHyphen ? makeString(content.substring(text->start, text->length), style.hyphenString()) : String();
    };
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::Text
        , layoutBox
        , lineRun.bidiLevel()
        , textRunRect
        , inkOverflow()
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text->start, text->length, content, adjustedContentToRender(), text->needsHyphen } });
}

void InlineDisplayContentBuilder::appendSoftLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& softLineBreakRunRect, DisplayBoxes& boxes)
{
    ASSERT(lineRun.textContent() && is<InlineTextBox>(lineRun.layoutBox()));

    auto& layoutBox = lineRun.layoutBox();
    auto& text = lineRun.textContent();

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::SoftLineBreak
        , layoutBox
        , lineRun.bidiLevel()
        , softLineBreakRunRect
        , softLineBreakRunRect
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text->start, text->length, downcast<InlineTextBox>(layoutBox).content() } });
}

void InlineDisplayContentBuilder::appendHardLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& lineBreakBoxRect, DisplayBoxes& boxes)
{
    auto& layoutBox = lineRun.layoutBox();

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::LineBreakBox
        , layoutBox
        , lineRun.bidiLevel()
        , lineBreakBoxRect
        , lineBreakBoxRect
        , lineRun.expansion()
        , { } });

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setLogicalTopLeft(toLayoutPoint(lineBreakBoxRect.topLeft()));
    boxGeometry.setContentBoxHeight(toLayoutUnit(lineBreakBoxRect.height()));
}

void InlineDisplayContentBuilder::appendAtomicInlineLevelDisplayBox(const Line::Run& lineRun, const InlineRect& borderBoxRect, DisplayBoxes& boxes)
{
    ASSERT(lineRun.layoutBox().isAtomicInlineLevelBox());

    auto& layoutBox = lineRun.layoutBox();
    // FIXME: Add ink overflow support for atomic inline level boxes (e.g. box shadow).
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::AtomicInlineLevelBox
        , layoutBox
        , lineRun.bidiLevel()
        , borderBoxRect
        , borderBoxRect
        , lineRun.expansion()
        , { } });

    // Note that inline boxes are relative to the line and their top position can be negative.
    // Atomic inline boxes are all set. Their margin/border/content box geometries are already computed. We just have to position them here.
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setLogicalTopLeft(toLayoutPoint(borderBoxRect.topLeft()));

    auto adjustParentInlineBoxInkOverflow = [&] {
        auto& parentInlineBox = layoutBox.parent();
        if (&parentInlineBox == &root()) {
            // We don't collect ink overflow for the root inline box.
            return;
        }
        RELEASE_ASSERT(m_inlineBoxIndexMap.contains(&parentInlineBox));

        auto boxInkOverflow = borderBoxRect;
        addBoxShadowInkOverflow(!m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), boxInkOverflow);
        boxes[m_inlineBoxIndexMap.get(&parentInlineBox)].adjustInkOverflow(boxInkOverflow);
    };
    adjustParentInlineBoxInkOverflow();
}

void InlineDisplayContentBuilder::setInlineBoxGeometry(const Box& layoutBox, const InlineRect& rect, bool isFirstInlineBoxFragment)
{
    auto adjustedSize = LayoutSize { LayoutUnit::fromFloatCeil(rect.width()), LayoutUnit::fromFloatCeil(rect.height()) };
    auto adjustedRect = Rect { LayoutPoint { rect.topLeft() }, adjustedSize };
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    if (!isFirstInlineBoxFragment) {
        auto enclosingBorderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
        enclosingBorderBoxRect.expandToContain(adjustedRect);
        adjustedRect = enclosingBorderBoxRect;
    }
    boxGeometry.setLogicalTopLeft(adjustedRect.topLeft());
    auto contentBoxHeight = adjustedRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu));
    auto contentBoxWidth = adjustedRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0_lu));
    boxGeometry.setContentBoxHeight(contentBoxHeight);
    boxGeometry.setContentBoxWidth(contentBoxWidth);
}

void InlineDisplayContentBuilder::appendInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, bool linehasContent, DisplayBoxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());

    auto& layoutBox = lineRun.layoutBox();
    if (linehasContent) {
        auto inkOverflow = [&] {
            auto inkOverflow = inlineBoxBorderBox;
            addBoxShadowInkOverflow(!m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow);
            return inkOverflow;
        };
        // FIXME: It's expected to not have any boxes on empty lines. We should reconsider this.
        m_inlineBoxIndexMap.add(&layoutBox, boxes.size());

        ASSERT(inlineBox.isInlineBox());
        ASSERT(inlineBox.isFirstBox());
        boxes.append({ m_lineIndex
            , InlineDisplay::Box::Type::NonRootInlineBox
            , layoutBox
            , lineRun.bidiLevel()
            , inlineBoxBorderBox
            , inkOverflow()
            , { }
            , { }
            , inlineBox.hasContent()
            , isFirstLastBox(inlineBox) });
    }

    // This inline box showed up first on this line.
    setInlineBoxGeometry(layoutBox, inlineBoxBorderBox, true);
}

void InlineDisplayContentBuilder::appendSpanningInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, DisplayBoxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());

    auto& layoutBox = lineRun.layoutBox();
    m_inlineBoxIndexMap.add(&layoutBox, boxes.size());

    auto inkOverflow = [&] {
        auto inkOverflow = inlineBoxBorderBox;
        addBoxShadowInkOverflow(!m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow);
        return inkOverflow;
    };
    ASSERT(!inlineBox.isFirstBox());
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , lineRun.bidiLevel()
        , inlineBoxBorderBox
        , inkOverflow()
        , { }
        , { }
        , inlineBox.hasContent()
        , isFirstLastBox(inlineBox) });

    // Middle or end of the inline box. Let's stretch the box as needed.
    setInlineBoxGeometry(layoutBox, inlineBoxBorderBox, false);
}

void InlineDisplayContentBuilder::appendInlineBoxDisplayBoxForBidiBoundary(const Box& layoutBox, const InlineRect& inlineBoxRect, DisplayBoxes& boxes)
{
    UNUSED_PARAM(layoutBox);
    UNUSED_PARAM(inlineBoxRect);
    UNUSED_PARAM(boxes);
}

void InlineDisplayContentBuilder::adjustInlineBoxDisplayBoxForBidiBoundary(InlineDisplay::Box& displayBox, const InlineRect& inlineBoxRect)
{
    UNUSED_PARAM(displayBox);
    UNUSED_PARAM(inlineBoxRect);
}

void InlineDisplayContentBuilder::processNonBidiContent(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineLayoutPoint& lineBoxLogicalTopLeft, DisplayBoxes& boxes)
{
    // Create the inline boxes on the current line. This is mostly text and atomic inline boxes.
    for (auto& lineRun : lineContent.runs) {

        auto displayBoxRect = [&] {
            auto& layoutBox = lineRun.layoutBox();
            auto logicalRect = InlineRect { };

            if (lineRun.isText() || lineRun.isSoftLineBreak())
                logicalRect = lineBox.logicalRectForTextRun(lineRun);
            else if (lineRun.isHardLineBreak())
                logicalRect = lineBox.logicalRectForLineBreakBox(layoutBox);
            else if (lineRun.isBox())
                logicalRect = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, formattingState().boxGeometry(layoutBox));
            else if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart())
                logicalRect = lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingState().boxGeometry(layoutBox));
            else
                ASSERT_NOT_REACHED();
            logicalRect.moveBy(lineBoxLogicalTopLeft);
            return logicalRect;
        };

        if (lineRun.isText()) {
            appendTextDisplayBox(lineRun, displayBoxRect(), boxes);
            continue;
        }
        if (lineRun.isSoftLineBreak()) {
            appendSoftLineBreakDisplayBox(lineRun, displayBoxRect(), boxes);
            continue;
        }
        if (lineRun.isHardLineBreak()) {
            appendHardLineBreakDisplayBox(lineRun, displayBoxRect(), boxes);
            continue;
        }
        if (lineRun.isBox()) {
            appendAtomicInlineLevelDisplayBox(lineRun, displayBoxRect(), boxes);
            continue;
        }
        if (lineRun.isInlineBoxStart()) {
            appendInlineBoxDisplayBox(lineRun, lineBox.inlineLevelBoxForLayoutBox(lineRun.layoutBox()), displayBoxRect(), lineBox.hasContent(), boxes);
            continue;
        }
        if (lineRun.isLineSpanningInlineBoxStart()) {
            if (!lineBox.hasContent()) {
                // When a spanning inline box (e.g. <div>text<span><br></span></div>) lands on an empty line
                // (empty here means no content at all including line breaks, not just visually empty) then we
                // don't extend the spanning line box over to this line -also there is no next line in cases like this.
                continue;
            }
            appendSpanningInlineBoxDisplayBox(lineRun, lineBox.inlineLevelBoxForLayoutBox(lineRun.layoutBox()), displayBoxRect(), boxes);
            continue;
        }
        ASSERT(lineRun.isInlineBoxEnd() || lineRun.isWordBreakOpportunity());
    }
}

void InlineDisplayContentBuilder::processBidiContent(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineLayoutPoint& lineBoxLogicalTopLeft, DisplayBoxes& boxes)
{
    ASSERT(lineContent.visualOrderList.size() == lineContent.runs.size());

    auto createDisplayBoxesInVisualOrderForContentRuns = [&] {
        auto rootInlineBoxRect = lineBox.logicalRectForRootInlineBox();
        auto contentRightInVisualOrder = InlineLayoutUnit { };
        // First visual run's initial content position depends on the block's inline direction.
        if (!root().style().isLeftToRightDirection()) {
            // FIXME: This needs the block end position instead of the lineLogicalWidth.
            contentRightInVisualOrder += lineContent.lineLogicalWidth - rootInlineBoxRect.width();
        }
        // Adjust the content start position with the (text)alignment offset (root inline box has the alignment offset and not the individual runs).
        contentRightInVisualOrder += rootInlineBoxRect.left();

        auto& runs = lineContent.runs;
        for (size_t i = 0; i < runs.size(); ++i) {
            auto visualIndex = lineContent.visualOrderList[i];
            auto& lineRun = runs[visualIndex];

            auto isContentRun = !lineRun.isInlineBoxStart() && !lineRun.isLineSpanningInlineBoxStart() && !lineRun.isInlineBoxEnd();
            if (!isContentRun) {
                // FIXME: Add support for inline boxes.
                continue;
            }

            auto visualRectRelativeToRoot = [&](auto logicallRect) {
                logicallRect.setLeft(contentRightInVisualOrder);
                logicallRect.moveBy(lineBoxLogicalTopLeft);
                return logicallRect;
            };

            if (lineRun.isText()) {
                auto visualRect = visualRectRelativeToRoot(lineBox.logicalRectForTextRun(lineRun));
                appendTextDisplayBox(lineRun, visualRect, boxes);
                contentRightInVisualOrder += visualRect.width();
                continue;
            }
            if (lineRun.isSoftLineBreak()) {
                ASSERT(!visualRectRelativeToRoot(lineBox.logicalRectForTextRun(lineRun)).width());
                appendSoftLineBreakDisplayBox(lineRun, visualRectRelativeToRoot(lineBox.logicalRectForTextRun(lineRun)), boxes);
                continue;
            }
            if (lineRun.isHardLineBreak()) {
                ASSERT(!visualRectRelativeToRoot(lineBox.logicalRectForLineBreakBox(lineRun.layoutBox())).width());
                appendHardLineBreakDisplayBox(lineRun, visualRectRelativeToRoot(lineBox.logicalRectForLineBreakBox(lineRun.layoutBox())), boxes);
                continue;
            }
            if (lineRun.isBox()) {
                auto& layoutBox = lineRun.layoutBox();
                auto& boxGeometry = formattingState().boxGeometry(layoutBox);
                auto visualRect = visualRectRelativeToRoot(lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry));
                visualRect.moveHorizontally(boxGeometry.marginStart());
                appendAtomicInlineLevelDisplayBox(lineRun, visualRect, boxes);
                contentRightInVisualOrder += boxGeometry.marginStart() + visualRect.width() + boxGeometry.marginEnd();
                continue;
            }
            ASSERT(lineRun.isWordBreakOpportunity());
        }
    };
    createDisplayBoxesInVisualOrderForContentRuns();
}

void InlineDisplayContentBuilder::processOverflownRunsForEllipsis(DisplayBoxes& boxes, InlineLayoutUnit lineBoxLogicalRight)
{
    if (root().style().textOverflow() != TextOverflow::Ellipsis)
        return;
    auto& rootInlineBox = boxes[0];
    ASSERT(rootInlineBox.isRootInlineBox());

    auto rootInlineBoxRect = rootInlineBox.logicalRect();
    if (rootInlineBoxRect.right() <= lineBoxLogicalRight) {
        ASSERT(boxes.last().logicalRight() <= lineBoxLogicalRight);
        return;
    }

    static MainThreadNeverDestroyed<const AtomString> ellipsisStr(&horizontalEllipsis, 1);
    auto ellipsisRun = WebCore::TextRun { ellipsisStr->string() };
    auto ellipsisWidth = !m_lineIndex ? root().firstLineStyle().fontCascade().width(ellipsisRun) : root().style().fontCascade().width(ellipsisRun);
    auto firstTruncatedBoxIndex = boxes.size();

    for (auto index = boxes.size(); index--;) {
        auto& displayBox = boxes[index];

        if (displayBox.logicalLeft() >= lineBoxLogicalRight) {
            // Fully overflown boxes are collapsed.
            displayBox.truncate();
            continue;
        }

        // We keep truncating content until after we can accommodate the ellipsis content
        // 1. fully truncate in case of inline level boxes (ie non-text content) or if ellipsis content is wider than the overflowing one.
        // 2. partially truncated to make room for the ellipsis box.
        auto availableRoomForEllipsis = lineBoxLogicalRight - displayBox.logicalLeft();
        if (availableRoomForEllipsis <= ellipsisWidth) {
            // Can't accommodate the ellipsis content here. We need to truncate non-overflowing boxes too.
            displayBox.truncate();
            continue;
        }

        auto truncatedWidth = InlineLayoutUnit { };
        if (displayBox.isText()) {
            auto text = *displayBox.text();
            // FIXME: Check if it needs adjustment for RTL direction.
            truncatedWidth = TextUtil::breakWord(downcast<InlineTextBox>(displayBox.layoutBox()), text.start(), text.length(), displayBox.logicalWidth(), availableRoomForEllipsis - ellipsisWidth, { }, displayBox.style().fontCascade()).logicalWidth;
        }
        displayBox.truncate(truncatedWidth);
        firstTruncatedBoxIndex = index;
        break;
    }
    ASSERT(firstTruncatedBoxIndex < boxes.size());
    // Collapse truncated runs.
    auto contentRight = boxes[firstTruncatedBoxIndex].logicalRight();
    for (auto index = firstTruncatedBoxIndex + 1; index < boxes.size(); ++index)
        boxes[index].moveHorizontally(contentRight - boxes[index].logicalLeft());
    // And append the ellipsis box as the trailing item.
    auto ellispisBoxRect = InlineRect { rootInlineBoxRect.top(), contentRight, ellipsisWidth, rootInlineBoxRect.height() };
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::Ellipsis
        , root()
        , UBIDI_DEFAULT_LTR
        , ellispisBoxRect
        , ellispisBoxRect
        , { }
        , InlineDisplay::Box::Text { 0, 1, ellipsisStr->string() } });
}

void InlineDisplayContentBuilder::collectInkOverflowForInlineBoxes(const LineBox& lineBox, DisplayBoxes& boxes)
{
    if (m_inlineBoxIndexMap.isEmpty() || !lineBox.hasContent()) {
        // This line has no inline box (only root, but we don't collect ink overflow for the root inline box atm)
        return;
    }

    auto& nonRootInlineLevelBoxes = lineBox.nonRootInlineLevelBoxes();
    // Visit the inline boxes and propagate ink overflow to their parents -except to the root inline box.
    // (e.g. <span style="font-size: 10px;">Small font size<span style="font-size: 300px;">Larger font size. This overflows the top most span.</span></span>).
    for (size_t index = nonRootInlineLevelBoxes.size(); index--;) {
        if (!nonRootInlineLevelBoxes[index].isInlineBox())
            continue;
        auto& inlineBox = nonRootInlineLevelBoxes[index].layoutBox();
        auto& parentInlineBox = inlineBox.parent();
        if (&parentInlineBox == &root())
            continue;
        RELEASE_ASSERT(m_inlineBoxIndexMap.contains(&inlineBox) && m_inlineBoxIndexMap.contains(&parentInlineBox));
        auto& inkOverflow = boxes[m_inlineBoxIndexMap.get(&inlineBox)].inkOverflow();
        auto& parentDisplayBox = boxes[m_inlineBoxIndexMap.get(&parentInlineBox)];
        parentDisplayBox.adjustInkOverflow(inkOverflow);
    }
}

}
}

#endif
