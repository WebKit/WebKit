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

InlineDisplayContentBuilder::InlineDisplayContentBuilder(const ContainerBox& formattingContextRoot, InlineFormattingState& formattingState)
    : m_formattingContextRoot(formattingContextRoot)
    , m_formattingState(formattingState)
{
}

InlineRuns InlineDisplayContentBuilder::build(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineLayoutPoint& lineBoxLogicalTopLeft, const size_t lineIndex)
{
    InlineRuns runs;
    runs.reserveInitialCapacity(lineContent.runs.size() + lineBox.nonRootInlineLevelBoxes().size() + 1);

    // Every line starts with a root run, even the empty ones.
    auto rootInlineBoxRect = lineBox.logicalRectForRootInlineBox();
    rootInlineBoxRect.moveBy(lineBoxLogicalTopLeft);
    runs.append({ lineIndex, Run::Type::RootInlineBox, root(), rootInlineBoxRect, rootInlineBoxRect, { }, { },  lineBox.rootInlineBox().hasContent()});

    createRunsAndUpdateGeometryForLineSpanningInlineBoxes(lineBox, lineBoxLogicalTopLeft, lineIndex, runs);
    createRunsAndUpdateGeometryForLineContent(lineContent, lineBox, lineBoxLogicalTopLeft, lineIndex, runs);
    collectInkOverflowForInlineBoxRuns(lineBox, runs);
    return runs;
}

void InlineDisplayContentBuilder::createRunsAndUpdateGeometryForLineContent(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineLayoutPoint& lineBoxLogicalTopLeft, const size_t lineIndex, InlineRuns& runs)
{
    auto& formattingState = this->formattingState();
    // Create the inline runs on the current line. This is mostly text and atomic inline runs.
    for (auto& lineRun : lineContent.runs) {
        auto& layoutBox = lineRun.layoutBox();
        switch (lineRun.type()) {
        case InlineItem::Type::Text: {
            auto textRunRect = lineBox.logicalRectForTextRun(lineRun);
            textRunRect.moveBy(lineBoxLogicalTopLeft);

            auto& style = layoutBox.style();
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
            runs.append({ lineIndex
                , Run::Type::Text
                , layoutBox
                , textRunRect
                , inkOverflow()
                , lineRun.expansion()
                , Run::Text { text->start, text->length, content, adjustedContentToRender(), text->needsHyphen } });
            break;
        }
        case InlineItem::Type::SoftLineBreak: {
            auto softLineBreakRunRect = lineBox.logicalRectForTextRun(lineRun);
            softLineBreakRunRect.moveBy(lineBoxLogicalTopLeft);

            ASSERT(lineRun.textContent() && is<InlineTextBox>(layoutBox));
            auto& text = lineRun.textContent();
            runs.append({ lineIndex
                , Run::Type::SoftLineBreak
                , layoutBox
                , softLineBreakRunRect
                , softLineBreakRunRect
                , lineRun.expansion()
                , Run::Text { text->start, text->length, downcast<InlineTextBox>(layoutBox).content() } });
            break;
        }
        case InlineItem::Type::HardLineBreak: {
            // Only hard linebreaks have associated layout boxes.
            auto lineBreakBoxRect = lineBox.logicalRectForLineBreakBox(layoutBox);
            lineBreakBoxRect.moveBy(lineBoxLogicalTopLeft);
            runs.append({ lineIndex, Run::Type::LineBreakBox, layoutBox, lineBreakBoxRect, lineBreakBoxRect, lineRun.expansion(), { } });

            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            boxGeometry.setLogicalTopLeft(toLayoutPoint(lineBreakBoxRect.topLeft()));
            boxGeometry.setContentBoxHeight(toLayoutUnit(lineBreakBoxRect.height()));
            break;
        }
        case InlineItem::Type::Box: {
            ASSERT(layoutBox.isAtomicInlineLevelBox());
            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            auto logicalBorderBox = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry);
            logicalBorderBox.moveBy(lineBoxLogicalTopLeft);
            // FIXME: Add ink overflow support for atomic inline level boxes (e.g. box shadow).
            runs.append({ lineIndex, Run::Type::AtomicInlineLevelBox, layoutBox, logicalBorderBox, logicalBorderBox, lineRun.expansion(), { } });

            auto borderBoxLogicalTopLeft = logicalBorderBox.topLeft();
            // Note that inline boxes are relative to the line and their top position can be negative.
            // Atomic inline boxes are all set. Their margin/border/content box geometries are already computed. We just have to position them here.
            boxGeometry.setLogicalTopLeft(toLayoutPoint(borderBoxLogicalTopLeft));

            auto adjustParentInlineBoxInkOverflow = [&] {
                auto& parentInlineBox = layoutBox.parent();
                if (&parentInlineBox == &root()) {
                    // We don't collect ink overflow for the root inline box run.
                    return;
                }
                RELEASE_ASSERT(m_inlineBoxRunIndexMap.contains(&parentInlineBox));
                runs[m_inlineBoxRunIndexMap.get(&parentInlineBox)].adjustInkOverflow(logicalBorderBox);
            };
            adjustParentInlineBoxInkOverflow();
            break;
        }
        case InlineItem::Type::InlineBoxStart: {
            // This inline box showed up first on this line.
            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            auto inlineBoxBorderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
            inlineBoxBorderBox.moveBy(lineBoxLogicalTopLeft);
            if (lineBox.hasContent()) {
                // FIXME: It's expected to not have any runs on empty lines. We should reconsider this.
                m_inlineBoxRunIndexMap.add(&layoutBox, runs.size());
                runs.append({ lineIndex, Run::Type::NonRootInlineBox, layoutBox, inlineBoxBorderBox, inlineBoxBorderBox, { }, { }, lineBox.inlineLevelBoxForLayoutBox(layoutBox).hasContent() });
            }

            auto inlineBoxSize = LayoutSize { LayoutUnit::fromFloatCeil(inlineBoxBorderBox.width()), LayoutUnit::fromFloatCeil(inlineBoxBorderBox.height()) };
            auto logicalRect = Rect { LayoutPoint { inlineBoxBorderBox.topLeft() }, inlineBoxSize };
            boxGeometry.setLogicalTopLeft(logicalRect.topLeft());
            auto contentBoxHeight = logicalRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu));
            boxGeometry.setContentBoxHeight(contentBoxHeight);
            auto contentBoxWidth = logicalRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0_lu));
            boxGeometry.setContentBoxWidth(contentBoxWidth);
            break;
        }
        default:
            ASSERT(lineRun.isInlineBoxEnd() || lineRun.isWordBreakOpportunity());
            break;
        }
    }
}

void InlineDisplayContentBuilder::createRunsAndUpdateGeometryForLineSpanningInlineBoxes(const LineBox& lineBox, const InlineLayoutPoint& lineBoxLogicalTopLeft, const size_t lineIndex, InlineRuns& runs)
{
    if (!lineBox.hasContent()) {
        // When a spanning inline box (e.g. <div>text<span><br></span></div>) lands on an empty line
        // (empty here means no content at all including line breaks, not just visually empty) then we
        // don't extend the spanning line box over to this line -also there is no next line in cases like this.
        return;
    }

    auto& formattingState = this->formattingState();
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        if (!inlineLevelBox.isLineSpanningInlineBox())
            continue;
        auto& layoutBox = inlineLevelBox.layoutBox();
        auto& boxGeometry = formattingState.boxGeometry(layoutBox);
        // Inline boxes may or may not be wrapped and have runs on multiple lines (e.g. <span>first line<br>second line<br>third line</span>)
        auto inlineBoxBorderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
        inlineBoxBorderBox.moveBy(lineBoxLogicalTopLeft);

        m_inlineBoxRunIndexMap.add(&layoutBox, runs.size());
        runs.append({ lineIndex, Run::Type::NonRootInlineBox, layoutBox, inlineBoxBorderBox, inlineBoxBorderBox, { }, { }, inlineLevelBox.hasContent() });

        auto inlineBoxSize = LayoutSize { LayoutUnit::fromFloatCeil(inlineBoxBorderBox.width()), LayoutUnit::fromFloatCeil(inlineBoxBorderBox.height()) };
        auto logicalRect = Rect { LayoutPoint { inlineBoxBorderBox.topLeft() }, inlineBoxSize };
        // Middle or end of the inline box. Let's stretch the box as needed.
        auto enclosingBorderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
        enclosingBorderBoxRect.expandToContain(logicalRect);
        boxGeometry.setLogicalLeft(enclosingBorderBoxRect.left());

        boxGeometry.setContentBoxHeight(enclosingBorderBoxRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu)));
        boxGeometry.setContentBoxWidth(enclosingBorderBoxRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0_lu)));
    }
}

void InlineDisplayContentBuilder::collectInkOverflowForInlineBoxRuns(const LineBox& lineBox, InlineRuns& runs)
{
    if (m_inlineBoxRunIndexMap.isEmpty() || !lineBox.hasContent()) {
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
        RELEASE_ASSERT(m_inlineBoxRunIndexMap.contains(&inlineBox) && m_inlineBoxRunIndexMap.contains(&parentInlineBox));
        auto& inkOverflow = runs[m_inlineBoxRunIndexMap.get(&inlineBox)].inkOverflow();
        auto& parentInlineBoxRun = runs[m_inlineBoxRunIndexMap.get(&parentInlineBox)];
        parentInlineBoxRun.adjustInkOverflow(inkOverflow);
    }
}

}
}

#endif
