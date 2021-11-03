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

DisplayBoxes InlineDisplayContentBuilder::build(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineLayoutPoint& lineBoxLogicalTopLeft, const size_t lineIndex)
{
    DisplayBoxes boxes;
    boxes.reserveInitialCapacity(lineContent.runs.size() + lineBox.nonRootInlineLevelBoxes().size() + 1);

    // Every line starts with a root box, even the empty ones.
    auto rootInlineBoxRect = lineBox.logicalRectForRootInlineBox();
    rootInlineBoxRect.moveBy(lineBoxLogicalTopLeft);
    boxes.append({ lineIndex, InlineDisplay::Box::Type::RootInlineBox, root(), UBIDI_DEFAULT_LTR, rootInlineBoxRect, rootInlineBoxRect, { }, { }, lineBox.rootInlineBox().hasContent() });

    createBoxesAndUpdateGeometryForLineContent(lineContent, lineBox, lineBoxLogicalTopLeft, lineIndex, boxes);
    collectInkOverflowForInlineBoxes(lineBox, boxes);
    return boxes;
}

void InlineDisplayContentBuilder::createBoxesAndUpdateGeometryForLineContent(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineLayoutPoint& lineBoxLogicalTopLeft, const size_t lineIndex, DisplayBoxes& boxes)
{
    // Create the inline boxes on the current line. This is mostly text and atomic inline boxes.
    auto& formattingState = this->formattingState();
    auto& runs = lineContent.runs;
    auto contentNeedsBidiReordering = !lineContent.visualOrderList.isEmpty();
    ASSERT(!contentNeedsBidiReordering || lineContent.visualOrderList.size() == runs.size());

    auto contentRightInVisualOrder = InlineLayoutUnit { };
    for (size_t i = 0; i < runs.size(); ++i) {
        auto visualIndex = contentNeedsBidiReordering ? lineContent.visualOrderList[i] : i;
        auto& lineRun = runs[visualIndex];
        auto& layoutBox = lineRun.layoutBox();
        auto& style = [&] () -> const RenderStyle& {
            return !lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();
        }();

        auto displayBoxRect = [&] {
            auto logicalRect = InlineRect { };

            if (lineRun.isText() || lineRun.isSoftLineBreak())
                logicalRect = lineBox.logicalRectForTextRun(lineRun);
            else if (lineRun.isHardLineBreak())
                logicalRect = lineBox.logicalRectForLineBreakBox(layoutBox);
            else {
                auto& boxGeometry = formattingState.boxGeometry(layoutBox);
                if (lineRun.isBox())
                    logicalRect = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry);
                else if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart())
                    logicalRect = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
                else
                    ASSERT_NOT_REACHED();
            }
            if (!contentNeedsBidiReordering) {
                logicalRect.moveBy(lineBoxLogicalTopLeft);
                return logicalRect;
            }
            logicalRect.moveVertically(lineBoxLogicalTopLeft.y());
            // Use the distance from the logical previous run to place the display box horizontally in visual terms.
            auto* logicalPreviousRun = visualIndex ? &runs[visualIndex - 1] : nullptr;
            // Certain css properties (e.g. word-spacing) may introduce a gap between runs.
            auto distanceFromLogicalPreviousRun = logicalPreviousRun ? lineRun.logicalLeft() - logicalPreviousRun->logicalRight() : lineRun.logicalLeft();
            auto visualOrderRect = logicalRect;
            auto contentLeft = contentRightInVisualOrder + distanceFromLogicalPreviousRun;
            if (!i) {
                // First visual run. Initial content position depends on the block's inline direction.
                contentLeft += lineBoxLogicalTopLeft.x();

                auto rootInlineBox = boxes[0];
                ASSERT(rootInlineBox.isRootInlineBox());
                if (!rootInlineBox.style().isLeftToRightDirection())
                    contentLeft += lineContent.lineLogicalWidth - rootInlineBox.logicalWidth();
            }
            visualOrderRect.setLeft(contentLeft);
            // The inline box right edge includes its content as well as the inline box end (padding-right etc).
            // What we need here is the inline box start run's width.
            contentRightInVisualOrder = lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()
                ? visualOrderRect.left() + lineRun.logicalWidth()
                : visualOrderRect.right();
            return visualOrderRect;
        };

        if (lineRun.isText()) {
            auto textRunRect = displayBoxRect();
            auto inkOverflow = [&] {
                auto initialContaingBlockSize = RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()
                    ? formattingState.layoutState().viewportSize()
                    : formattingState.layoutState().geometryForBox(layoutBox.initialContainingBlock()).contentBox().size();
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
            ASSERT(lineRun.textContent() && is<InlineTextBox>(layoutBox));
            auto content = downcast<InlineTextBox>(layoutBox).content();
            auto text = lineRun.textContent();
            auto adjustedContentToRender = [&] {
                return text->needsHyphen ? makeString(content.substring(text->start, text->length), style.hyphenString()) : String();
            };
            boxes.append({ lineIndex
                , InlineDisplay::Box::Type::Text
                , layoutBox
                , lineRun.bidiLevel()
                , textRunRect
                , inkOverflow()
                , lineRun.expansion()
                , InlineDisplay::Box::Text { text->start, text->length, content, adjustedContentToRender(), text->needsHyphen } });
            continue;
        }
        if (lineRun.isSoftLineBreak()) {
            ASSERT(lineRun.textContent() && is<InlineTextBox>(layoutBox));
            auto softLineBreakRunRect = displayBoxRect();
            auto& text = lineRun.textContent();
            boxes.append({ lineIndex
                , InlineDisplay::Box::Type::SoftLineBreak
                , layoutBox
                , lineRun.bidiLevel()
                , softLineBreakRunRect
                , softLineBreakRunRect
                , lineRun.expansion()
                , InlineDisplay::Box::Text { text->start, text->length, downcast<InlineTextBox>(layoutBox).content() } });
            break;
        }
        if (lineRun.isHardLineBreak()) {
            // Only hard linebreaks have associated layout boxes.
            auto lineBreakBoxRect = displayBoxRect();
            boxes.append({ lineIndex, InlineDisplay::Box::Type::LineBreakBox, layoutBox, lineRun.bidiLevel(), lineBreakBoxRect, lineBreakBoxRect, lineRun.expansion(), { } });

            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            boxGeometry.setLogicalTopLeft(toLayoutPoint(lineBreakBoxRect.topLeft()));
            boxGeometry.setContentBoxHeight(toLayoutUnit(lineBreakBoxRect.height()));
            continue;
        }
        if (lineRun.isBox()) {
            ASSERT(layoutBox.isAtomicInlineLevelBox());
            auto borderBoxRect = displayBoxRect();
            // FIXME: Add ink overflow support for atomic inline level boxes (e.g. box shadow).
            boxes.append({ lineIndex, InlineDisplay::Box::Type::AtomicInlineLevelBox, layoutBox, lineRun.bidiLevel(), borderBoxRect, borderBoxRect, lineRun.expansion(), { } });

            // Note that inline boxes are relative to the line and their top position can be negative.
            // Atomic inline boxes are all set. Their margin/border/content box geometries are already computed. We just have to position them here.
            formattingState.boxGeometry(layoutBox).setLogicalTopLeft(toLayoutPoint(borderBoxRect.topLeft()));

            auto adjustParentInlineBoxInkOverflow = [&] {
                auto& parentInlineBox = layoutBox.parent();
                if (&parentInlineBox == &root()) {
                    // We don't collect ink overflow for the root inline box.
                    return;
                }
                RELEASE_ASSERT(m_inlineBoxIndexMap.contains(&parentInlineBox));
                boxes[m_inlineBoxIndexMap.get(&parentInlineBox)].adjustInkOverflow(borderBoxRect);
            };
            adjustParentInlineBoxInkOverflow();
            continue;
        }
        if (lineRun.isInlineBoxStart()) {
            // This inline box showed up first on this line.
            auto inlineBoxBorderBox = displayBoxRect();
            if (lineBox.hasContent()) {
                // FIXME: It's expected to not have any boxes on empty lines. We should reconsider this.
                m_inlineBoxIndexMap.add(&layoutBox, boxes.size());

                auto& inlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox);
                ASSERT(inlineBox.isInlineBox());
                ASSERT(inlineBox.isFirstBox());
                boxes.append({ lineIndex, InlineDisplay::Box::Type::NonRootInlineBox, layoutBox, lineRun.bidiLevel(), inlineBoxBorderBox, inlineBoxBorderBox, { }, { }, inlineBox.hasContent(), isFirstLastBox(inlineBox) });
            }

            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            auto inlineBoxSize = LayoutSize { LayoutUnit::fromFloatCeil(inlineBoxBorderBox.width()), LayoutUnit::fromFloatCeil(inlineBoxBorderBox.height()) };
            auto logicalRect = Rect { LayoutPoint { inlineBoxBorderBox.topLeft() }, inlineBoxSize };
            boxGeometry.setLogicalTopLeft(logicalRect.topLeft());
            auto contentBoxHeight = logicalRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu));
            boxGeometry.setContentBoxHeight(contentBoxHeight);
            auto contentBoxWidth = logicalRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0_lu));
            boxGeometry.setContentBoxWidth(contentBoxWidth);
            continue;
        }
        if (lineRun.isLineSpanningInlineBoxStart()) {
            if (!lineBox.hasContent()) {
                // When a spanning inline box (e.g. <div>text<span><br></span></div>) lands on an empty line
                // (empty here means no content at all including line breaks, not just visually empty) then we
                // don't extend the spanning line box over to this line -also there is no next line in cases like this.
                continue;
            }
            m_inlineBoxIndexMap.add(&layoutBox, boxes.size());

            auto& inlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox);
            auto inlineBoxBorderBox = displayBoxRect();
            ASSERT(!inlineBox.isFirstBox());
            boxes.append({ lineIndex, InlineDisplay::Box::Type::NonRootInlineBox, layoutBox, lineRun.bidiLevel(), inlineBoxBorderBox, inlineBoxBorderBox, { }, { }, inlineBox.hasContent(), isFirstLastBox(inlineBox) });

            auto inlineBoxSize = LayoutSize { LayoutUnit::fromFloatCeil(inlineBoxBorderBox.width()), LayoutUnit::fromFloatCeil(inlineBoxBorderBox.height()) };
            auto logicalRect = Rect { LayoutPoint { inlineBoxBorderBox.topLeft() }, inlineBoxSize };
            // Middle or end of the inline box. Let's stretch the box as needed.
            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            auto enclosingBorderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
            enclosingBorderBoxRect.expandToContain(logicalRect);
            boxGeometry.setLogicalLeft(enclosingBorderBoxRect.left());

            boxGeometry.setContentBoxHeight(enclosingBorderBoxRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu)));
            boxGeometry.setContentBoxWidth(enclosingBorderBoxRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0_lu)));
            continue;
        }
        ASSERT(lineRun.isInlineBoxEnd() || lineRun.isWordBreakOpportunity());
    }
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
