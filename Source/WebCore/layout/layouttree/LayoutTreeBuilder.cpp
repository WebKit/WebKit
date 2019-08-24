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
#include "HTMLTableCellElement.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "LayoutChildIterator.h"
#include "LayoutContainer.h"
#include "LayoutDescendantIterator.h"
#include "LayoutPhase.h"
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
#include "RenderTableCell.h"
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
    PhaseScope scope(Phase::Type::TreeBuilding);

    auto style = RenderStyle::clone(renderView.style());
    style.setLogicalWidth(Length(renderView.width(), Fixed));
    style.setLogicalHeight(Length(renderView.height(), Fixed));

    std::unique_ptr<Container> initialContainingBlock(new Container(WTF::nullopt, WTFMove(style)));
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
            if (element->hasTagName(HTMLNames::theadTag))
                return Box::ElementAttributes { Box::ElementType::TableHeaderGroup };
            if (element->hasTagName(HTMLNames::tbodyTag))
                return Box::ElementAttributes { Box::ElementType::TableBodyGroup };
            if (element->hasTagName(HTMLNames::tfootTag))
                return Box::ElementAttributes { Box::ElementType::TableFooterGroup };
            if (element->hasTagName(HTMLNames::imgTag))
                return Box::ElementAttributes { Box::ElementType::Image };
            if (element->hasTagName(HTMLNames::iframeTag))
                return Box::ElementAttributes { Box::ElementType::IFrame };
            // FIXME wbr should not be considered as hard linebreak.
            if (element->hasTagName(HTMLNames::brTag) || element->hasTagName(HTMLNames::wbrTag))
                return Box::ElementAttributes { Box::ElementType::HardLineBreak };
            return Box::ElementAttributes { Box::ElementType::GenericElement };
        }
        return WTF::nullopt;
    };

    std::unique_ptr<Box> childLayoutBox;
    if (is<RenderText>(childRenderer)) {
        // FIXME: Clearly there must be a helper function for this.
        if (parentRenderer.style().display() == DisplayType::Inline)
            childLayoutBox = makeUnique<Box>(downcast<RenderText>(childRenderer).originalText(), RenderStyle::clone(parentRenderer.style()));
        else
            childLayoutBox = makeUnique<Box>(downcast<RenderText>(childRenderer).originalText(), RenderStyle::createAnonymousStyleWithDisplay(parentRenderer.style(), DisplayType::Inline));
        childLayoutBox->setIsAnonymous();
        return childLayoutBox;
    }

    auto& renderer = downcast<RenderElement>(childRenderer);
    auto displayType = renderer.style().display();
    if (is<RenderLineBreak>(renderer))
        return makeUnique<Box>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));

    if (is<RenderTable>(renderer)) {
        // Construct the principal table wrapper box (and not the table box itself).
        childLayoutBox = makeUnique<Container>(Box::ElementAttributes { Box::ElementType::TableWrapperBox }, RenderStyle::clone(renderer.style()));
        childLayoutBox->setIsAnonymous();
    } else if (is<RenderReplaced>(renderer)) {
        if (displayType == DisplayType::Block)
            childLayoutBox = makeUnique<Box>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        else
            childLayoutBox = makeUnique<Box>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
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
                childLayoutBox = makeUnique<Container>(elementAttributes(renderer), WTFMove(*style));
            } else
                childLayoutBox = makeUnique<Container>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        } else if (displayType == DisplayType::Inline)
            childLayoutBox = makeUnique<Container>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        else if (displayType == DisplayType::InlineBlock)
            childLayoutBox = makeUnique<Container>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        else if (displayType == DisplayType::TableCaption || displayType == DisplayType::TableCell) {
            childLayoutBox = makeUnique<Container>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        } else if (displayType == DisplayType::TableRowGroup || displayType == DisplayType::TableHeaderGroup || displayType == DisplayType::TableFooterGroup
            || displayType == DisplayType::TableRow || displayType == DisplayType::TableColumnGroup || displayType == DisplayType::TableColumn) {
            childLayoutBox = makeUnique<Container>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
        } else {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
    }

    if (is<RenderTableCell>(renderer)) {
        auto& cellElement = downcast<HTMLTableCellElement>(*renderer.element());
        auto rowSpan = cellElement.rowSpan();
        if (rowSpan > 1)
            childLayoutBox->setRowSpan(rowSpan);

        auto columnSpan = cellElement.colSpan();
        if (columnSpan > 1)
            childLayoutBox->setColumnSpan(columnSpan);
    }

    if (childRenderer.isAnonymous())
        childLayoutBox->setIsAnonymous();

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

    auto tableBox = makeUnique<Container>(Box::ElementAttributes { Box::ElementType::TableBox }, RenderStyle::clone(tableRenderer.style()));
    appendChild(tableWrapperBox, *tableBox);
    auto* sectionRenderer = tableChild;
    while (sectionRenderer) {
        auto sectionBox = createLayoutBox(tableRenderer, *sectionRenderer);
        appendChild(*tableBox, *sectionBox);
        auto& sectionContainer = downcast<Container>(*sectionBox);
        TreeBuilder::createSubTree(downcast<RenderElement>(*sectionRenderer), sectionContainer);
        sectionBox.release();
        sectionRenderer = sectionRenderer->nextSibling();
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

    if (layoutBox.isInitialContainingBlock())
        stream << "Initial containing block";
    else if (layoutBox.isDocumentBox())
        stream << "HTML";
    else if (layoutBox.isBodyBox())
        stream << "BODY";
    else if (layoutBox.isTableWrapperBox())
        stream << "TABLE principal";
    else if (layoutBox.isTableBox())
        stream << "TABLE";
    else if (layoutBox.isTableCaption())
        stream << "CAPTION";
    else if (layoutBox.isTableHeader())
        stream << "THEAD";
    else if (layoutBox.isTableBody())
        stream << "TBODY";
    else if (layoutBox.isTableFooter())
        stream << "TFOOT";
    else if (layoutBox.isTableCell())
        stream << "TD";
    else if (layoutBox.isTableRow())
        stream << "TR";
    else if (layoutBox.isInlineBlockBox())
        stream << "Inline-block container";
    else if (layoutBox.isInlineLevelBox()) {
        if (layoutBox.isInlineContainer())
            stream << "SPAN inline container";
        else if (layoutBox.replaced())
            stream << "IMG replaced inline box";
        else if (layoutBox.isAnonymous())
            stream << "anonymous inline box";
        else if (layoutBox.isLineBreakBox())
            stream << "BR line break";
        else
            stream << "inline box";
    } else if (layoutBox.isBlockLevelBox())
        stream << "block box";
    else
        stream << "unknown box";

    // FIXME: Inline text runs don't create display boxes yet.
    if (displayBox)
        stream << " at (" << displayBox->left() << "," << displayBox->top() << ") size " << displayBox->width() << "x" << displayBox->height();
    stream << " layout box->(" << &layoutBox << ")";
    if (layoutBox.isInlineLevelBox() && layoutBox.isAnonymous())
        stream << " text content [\"" << layoutBox.textContent().utf8().data() << "\"]";

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
        auto layoutState = makeUnique<Layout::LayoutState>(*initialContainingBlock);
        layoutState->setQuirksMode(renderView.document().inLimitedQuirksMode() ? LayoutState::QuirksMode::Limited : (renderView.document().inQuirksMode() ? LayoutState::QuirksMode::Yes : LayoutState::QuirksMode::No));
        layoutState->updateLayout();
        showLayoutTree(*initialContainingBlock, layoutState.get());
    }
}
#endif

}
}

#endif
