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
#include "LayoutTreeBuilder.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "DisplayRun.h"
#include "InlineFormattingState.h"
#include "LayoutBlockContainer.h"
#include "LayoutBox.h"
#include "LayoutChildIterator.h"
#include "LayoutContainer.h"
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "LayoutState.h"
#include "RenderBlock.h"
#include "RenderChildIterator.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderStyle.h"
#include "RenderTable.h"
#include "RenderTableCaption.h"
#include "RenderView.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

static void appendChild(Container& parent, Box& newChild)
{
    if (!parent.hasChild()) {
        parent.setFirstChild(newChild);
        parent.setLastChild(newChild);
        newChild.setParent(parent);
        return;
    }

    auto& lastChild = const_cast<Box&>(*parent.lastChild());
    lastChild.setNextSibling(newChild);
    newChild.setPreviousSibling(lastChild);
    newChild.setParent(parent);
    parent.setLastChild(newChild);
}

std::unique_ptr<Container> TreeBuilder::createLayoutTree(const RenderView& renderView)
{
    auto style = RenderStyle::clone(renderView.style());
    style.setLogicalWidth(Length(renderView.width(), Fixed));
    style.setLogicalHeight(Length(renderView.height(), Fixed));

    std::unique_ptr<Container> initialContainingBlock(new BlockContainer(WTF::nullopt, WTFMove(style)));
    TreeBuilder::createSubTree(renderView, *initialContainingBlock);
    return initialContainingBlock;
}

static Optional<LayoutSize> accumulatedOffsetForInFlowPositionedContinuation(const RenderBox& block)
{
    // FIXE: This is a workaround of the continuation logic when the relatively positioned parent inline box
    // becomes a sibling box of this block and only reachable through the continuation link which we don't have here.
    if (!block.isAnonymous() || !block.isInFlowPositioned() || !block.isContinuation())
        return { };
    return block.relativePositionOffset();
}

std::unique_ptr<Box> TreeBuilder::createLayoutBox(const RenderElement& parentRenderer, const RenderObject& childRenderer)
{
    auto elementAttributes = [] (const RenderElement& renderer) -> Optional<Box::ElementAttributes> {
        if (renderer.isDocumentElementRenderer())
            return Box::ElementAttributes { Box::ElementType::Document };
        if (auto* element = renderer.element()) {
            if (element->hasTagName(HTMLNames::bodyTag))
                return Box::ElementAttributes { Box::ElementType::Body };
            if (element->hasTagName(HTMLNames::colTag))
                return Box::ElementAttributes { Box::ElementType::TableColumn };
            if (element->hasTagName(HTMLNames::trTag))
                return Box::ElementAttributes { Box::ElementType::TableRow };
            if (element->hasTagName(HTMLNames::colgroupTag))
                return Box::ElementAttributes { Box::ElementType::TableColumnGroup };
            if (element->hasTagName(HTMLNames::tbodyTag))
                return Box::ElementAttributes { Box::ElementType::TableRowGroup };
            if (element->hasTagName(HTMLNames::theadTag))
                return Box::ElementAttributes { Box::ElementType::TableHeaderGroup };
            if (element->hasTagName(HTMLNames::tfootTag))
                return Box::ElementAttributes { Box::ElementType::TableFooterGroup };
            if (element->hasTagName(HTMLNames::tfootTag))
                return Box::ElementAttributes { Box::ElementType::TableFooterGroup };
            if (element->hasTagName(HTMLNames::imgTag))
                return Box::ElementAttributes { Box::ElementType::Image };
            if (element->hasTagName(HTMLNames::iframeTag))
                return Box::ElementAttributes { Box::ElementType::IFrame };
            return Box::ElementAttributes { Box::ElementType::GenericElement };
        }
        return WTF::nullopt;
    };

    std::unique_ptr<Box> childLayoutBox;
    if (is<RenderText>(childRenderer)) {
        // FIXME: Clearly there must be a helper function for this.
        if (parentRenderer.style().display() == DisplayType::Inline)
            childLayoutBox = std::make_unique<InlineBox>(Optional<Box::ElementAttributes>(), RenderStyle::clone(parentRenderer.style()));
        else
            childLayoutBox = std::make_unique<InlineBox>(Optional<Box::ElementAttributes>(), RenderStyle::createAnonymousStyleWithDisplay(parentRenderer.style(), DisplayType::Inline));
        downcast<InlineBox>(*childLayoutBox).setTextContent(downcast<RenderText>(childRenderer).originalText());
        return childLayoutBox;
    }

    auto& renderer = downcast<RenderElement>(childRenderer);
    auto displayType = renderer.style().display();
    if (is<RenderLineBreak>(renderer))
        return std::make_unique<LineBreakBox>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));

    if (is<RenderTable>(renderer)) {
        // Construct the principal table wrapper box (and not the table box itself).
        if (displayType == DisplayType::Table)
            childLayoutBox = std::make_unique<BlockContainer>(Box::ElementAttributes { Box::ElementType::TableWrapperBox }, RenderStyle::clone(renderer.style()));
        else {
            ASSERT(displayType == DisplayType::InlineTable);
            childLayoutBox = std::make_unique<InlineContainer>(Box::ElementAttributes { Box::ElementType::TableWrapperBox }, RenderStyle::clone(renderer.style()));
        }
    } else if (is<RenderReplaced>(renderer)) {
        if (displayType == DisplayType::Block)
            childLayoutBox = std::make_unique<Box>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        else
            childLayoutBox = std::make_unique<InlineBox>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        // FIXME: We don't yet support all replaced elements and this is temporary anyway.
        if (childLayoutBox->replaced())
            childLayoutBox->replaced()->setIntrinsicSize(downcast<RenderReplaced>(renderer).intrinsicSize());
        if (is<RenderImage>(renderer)) {
            auto& imageRenderer = downcast<RenderImage>(renderer);
            if (imageRenderer.imageResource().errorOccurred())
                childLayoutBox->replaced()->setIntrinsicRatio(1);
        }
    } else {
        if (displayType == DisplayType::Block) {
            if (auto offset = accumulatedOffsetForInFlowPositionedContinuation(downcast<RenderBox>(renderer))) {
                auto style = RenderStyle::clonePtr(renderer.style());
                style->setTop({ offset->height(), Fixed });
                style->setLeft({ offset->width(), Fixed });
                childLayoutBox = std::make_unique<BlockContainer>(elementAttributes(renderer), WTFMove(*style));
            } else
                childLayoutBox = std::make_unique<BlockContainer>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        } else if (displayType == DisplayType::Inline)
            childLayoutBox = std::make_unique<InlineContainer>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        else if (displayType == DisplayType::InlineBlock)
            childLayoutBox = std::make_unique<InlineContainer>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        else if (displayType == DisplayType::TableCaption || displayType == DisplayType::TableCell) {
            childLayoutBox = std::make_unique<BlockContainer>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        } else if (displayType == DisplayType::TableRowGroup || displayType == DisplayType::TableHeaderGroup || displayType == DisplayType::TableFooterGroup
            || displayType == DisplayType::TableRow || displayType == DisplayType::TableColumnGroup || displayType == DisplayType::TableColumn) {
            childLayoutBox = std::make_unique<Container>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        } else {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
    }

    if (childLayoutBox->isOutOfFlowPositioned()) {
        // Not efficient, but this is temporary anyway.
        // Collect the out-of-flow descendants at the formatting root level (as opposed to at the containing block level, though they might be the same).
        const_cast<Container&>(childLayoutBox->formattingContextRoot()).addOutOfFlowDescendant(*childLayoutBox);
    }
    return childLayoutBox;
}

void TreeBuilder::createTableStructure(const RenderTable& tableRenderer, Container& tableWrapperBox)
{
    // Create caption and table box.
    auto* tableChild = tableRenderer.firstChild();
    while (is<RenderTableCaption>(tableChild)) {
        auto& captionRenderer = *tableChild;
        auto captionBox = createLayoutBox(tableRenderer, captionRenderer);
        appendChild(tableWrapperBox, *captionBox);
        auto& captionContainer = downcast<Container>(*captionBox);
        TreeBuilder::createSubTree(downcast<RenderElement>(captionRenderer), captionContainer);
        // Temporary
        captionBox.release();
        tableChild = tableChild->nextSibling();
    }

    auto tableBox = std::make_unique<BlockContainer>(Box::ElementAttributes { Box::ElementType::TableBox }, RenderStyle::clone(tableRenderer.style()));
    appendChild(tableWrapperBox, *tableBox);
    while (tableChild) {
        TreeBuilder::createSubTree(downcast<RenderElement>(*tableChild), *tableBox);
        tableChild = tableChild->nextSibling();
    }
    // Temporary
    tableBox.release();
}

void TreeBuilder::createSubTree(const RenderElement& rootRenderer, Container& rootContainer)
{
    for (auto& childRenderer : childrenOfType<RenderObject>(rootRenderer)) {
        auto childLayoutBox = createLayoutBox(rootRenderer, childRenderer);
        appendChild(rootContainer, *childLayoutBox);
        if (childLayoutBox->isTableWrapperBox())
            createTableStructure(downcast<RenderTable>(childRenderer), downcast<Container>(*childLayoutBox));
        else if (is<Container>(*childLayoutBox))
            createSubTree(downcast<RenderElement>(childRenderer), downcast<Container>(*childLayoutBox));
        // Temporary
        childLayoutBox.release();
    }
}

#if ENABLE(TREE_DEBUGGING)
static void outputInlineRuns(TextStream& stream, const LayoutState& layoutState, const Container& inlineFormattingRoot, unsigned depth)
{
    auto& inlineFormattingState = downcast<InlineFormattingState>(layoutState.establishedFormattingState(inlineFormattingRoot));
    auto& inlineRuns = inlineFormattingState.inlineRuns();
    auto& lineBoxes = inlineFormattingState.lineBoxes();

    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 3)
        stream << " ";

    stream << "lines are -> ";
    for (auto& lineBox : lineBoxes)
        stream << "[" << lineBox.logicalLeft() << "," << lineBox.logicalTop() << " " << lineBox.logicalWidth() << "x" << lineBox.logicalHeight() << "] ";
    stream.nextLine();

    for (auto& inlineRun : inlineRuns) {
        unsigned printedCharacters = 0;
        while (++printedCharacters <= depth * 3)
            stream << " ";
        if (inlineRun->textContext())
            stream << "inline text box";
        else  
            stream << "inline box";
        stream << " at (" << inlineRun->logicalLeft() << "," << inlineRun->logicalTop() << ") size " << inlineRun->logicalWidth() << "x" << inlineRun->logicalHeight();
        if (inlineRun->textContext())
            stream << " run(" << inlineRun->textContext()->start() << ", " << inlineRun->textContext()->end() << ")";
        stream.nextLine();
    }
}

static void outputLayoutBox(TextStream& stream, const Box& layoutBox, const Display::Box* displayBox, unsigned depth)
{
    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";

    if (layoutBox.isFloatingPositioned())
        stream << "[float] ";

    if (is<Container>(layoutBox)) {
        if (layoutBox.isTableWrapperBox())
            stream << "TABLE principal";
        else if (is<InlineContainer>(layoutBox)) {
            // FIXME: fix names
            if (layoutBox.isInlineBlockBox())
                stream << "DIV inline-block container";
            else
                stream << "SPAN inline container";
        } else if (is<BlockContainer>(layoutBox)) {
            if (layoutBox.isInitialContainingBlock())
                stream << "Initial containing block";
            else if (layoutBox.isDocumentBox())
                stream << "HTML";
            else if (layoutBox.isBodyBox())
                stream << "BODY";
            else if (layoutBox.isTableBox())
                stream << "TABLE";
            else if (layoutBox.isTableCaption())
                stream << "CAPTION";
            else if (layoutBox.isTableCell())
                stream << "TD";
            else {
                // FIXME
                stream << "DIV";
            }
        } else {
            if (layoutBox.isTableRow())
                stream << "TR";
            else
                stream << "unknown container";
        }
    } else if (layoutBox.isInlineLevelBox()) {
        if (layoutBox.replaced())
            stream << "IMG replaced inline box";
        else if (layoutBox.isAnonymous())
            stream << "anonymous inline box";
        else if (is<LineBreakBox>(layoutBox))
            stream << "BR line break";
        else
            stream << "inline box";
    } else if (layoutBox.isBlockLevelBox())
        stream << "block box";
    else
        stream << "box";

    // FIXME: Inline text runs don't create display boxes yet.
    if (displayBox)
        stream << " at (" << displayBox->left() << "," << displayBox->top() << ") size " << displayBox->width() << "x" << displayBox->height();
    stream << " layout box->(" << &layoutBox << ")";
    if (is<InlineBox>(layoutBox) && downcast<InlineBox>(layoutBox).hasTextContent())
        stream << " text content [\"" << downcast<InlineBox>(layoutBox).textContent().utf8().data() << "\"]";

    stream.nextLine();
}

static void outputLayoutTree(const LayoutState* layoutState, TextStream& stream, const Container& rootContainer, unsigned depth)
{
    for (auto& child : childrenOfType<Box>(rootContainer)) {
        Display::Box* displayBox = nullptr;
        // Not all boxes generate display boxes.
        if (layoutState && layoutState->hasDisplayBox(child))
            displayBox = &layoutState->displayBoxForLayoutBox(child);

        outputLayoutBox(stream, child, displayBox, depth);
        if (layoutState && child.establishesInlineFormattingContext())
            outputInlineRuns(stream, *layoutState, downcast<Container>(child), depth + 1);

        if (is<Container>(child))
            outputLayoutTree(layoutState, stream, downcast<Container>(child), depth + 1);
    }
}

void showLayoutTree(const Box& layoutBox, const LayoutState* layoutState)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    auto& initialContainingBlock = layoutBox.initialContainingBlock();
    outputLayoutBox(stream, initialContainingBlock, layoutState ? &layoutState->displayBoxForLayoutBox(initialContainingBlock) : nullptr, 0);
    outputLayoutTree(layoutState, stream, initialContainingBlock, 1);
    WTFLogAlways("%s", stream.release().utf8().data());
}

void showLayoutTree(const Box& layoutBox)
{
    showLayoutTree(layoutBox, nullptr);
}

void printLayoutTreeForLiveDocuments()
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isMainFrame())
            fprintf(stderr, "----------------------main frame--------------------------\n");
        fprintf(stderr, "%s\n", document->url().string().utf8().data());
        // FIXME: Need to find a way to output geometry without layout context.
        auto& renderView = *document->renderView();
        auto initialContainingBlock = TreeBuilder::createLayoutTree(renderView);
        auto layoutState = std::make_unique<Layout::LayoutState>(*initialContainingBlock);
        layoutState->setQuirksMode(renderView.document().inLimitedQuirksMode() ? LayoutState::QuirksMode::Limited : (renderView.document().inQuirksMode() ? LayoutState::QuirksMode::Yes : LayoutState::QuirksMode::No));
        layoutState->updateLayout();
        showLayoutTree(*initialContainingBlock, layoutState.get());
    }
}
#endif

}
}

#endif
