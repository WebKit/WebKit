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

#include "CachedImage.h"
#include "DisplayBox.h"
#include "DisplayRun.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableColElement.h"
#include "InlineFormattingState.h"
#include "InvalidationContext.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutChildIterator.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "LayoutDescendantIterator.h"
#include "LayoutPhase.h"
#include "LayoutSize.h"
#include "LayoutState.h"
#include "RenderBlock.h"
#include "RenderBox.h"
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
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutTreeContent);
LayoutTreeContent::LayoutTreeContent(const RenderBox& rootRenderer, std::unique_ptr<Container> rootLayoutBox)
    : m_rootRenderer(rootRenderer)
    , m_rootLayoutBox(WTFMove(rootLayoutBox))
{
}

LayoutTreeContent::~LayoutTreeContent() = default;


void LayoutTreeContent::addLayoutBoxForRenderer(const RenderObject& renderer, Box& layoutBox)
{
    m_renderObjectToLayoutBox.add(&renderer, &layoutBox);
    m_layoutBoxToRenderObject.add(&layoutBox, &renderer);
}

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

static Optional<LayoutSize> accumulatedOffsetForInFlowPositionedContinuation(const RenderBox& block)
{
    // FIXE: This is a workaround of the continuation logic when the relatively positioned parent inline box
    // becomes a sibling box of this block and only reachable through the continuation link which we don't have here.
    if (!block.isAnonymous() || !block.isInFlowPositioned() || !block.isContinuation())
        return { };
    return block.relativePositionOffset();
}

static bool canUseSimplifiedTextMeasuring(const StringView& content, const FontCascade& font, bool whitespaceIsCollapsed)
{
    if (font.codePath(TextRun(content)) == FontCascade::Complex)
        return false;

    if (font.wordSpacing() || font.letterSpacing())
        return false;

    for (unsigned i = 0; i < content.length(); ++i) {
        if ((!whitespaceIsCollapsed && content[i] == '\t') || content[i] == noBreakSpace || content[i] >= HiraganaLetterSmallA)
            return false;
    }
    return true;
}

std::unique_ptr<Layout::LayoutTreeContent> TreeBuilder::buildLayoutTree(const RenderView& renderView)
{
    PhaseScope scope(Phase::Type::TreeBuilding);

    auto style = RenderStyle::clone(renderView.style());
    style.setLogicalWidth(Length(renderView.width(), Fixed));
    style.setLogicalHeight(Length(renderView.height(), Fixed));

    auto layoutTreeContent = makeUnique<LayoutTreeContent>(renderView, makeUnique<Container>(WTF::nullopt, WTFMove(style)));
    TreeBuilder(*layoutTreeContent).buildTree();
    return layoutTreeContent;
}

TreeBuilder::TreeBuilder(LayoutTreeContent& layoutTreeContent)
    : m_layoutTreeContent(layoutTreeContent)
{
}

void TreeBuilder::buildTree()
{
    buildSubTree(m_layoutTreeContent.rootRenderer(), m_layoutTreeContent.rootLayoutBox());
}

Box& TreeBuilder::createBox(Optional<Box::ElementAttributes> elementAttributes, RenderStyle&& style)
{
    auto newBox = makeUnique<Box>(elementAttributes, WTFMove(style));
    auto& box = *newBox;
    m_layoutTreeContent.addBox(WTFMove(newBox));
    return box;
}

Box& TreeBuilder::createTextBox(TextContext&& textContent, RenderStyle&& style)
{
    auto newBox = makeUnique<Box>(WTFMove(textContent), WTFMove(style));
    auto& box = *newBox;
    m_layoutTreeContent.addBox(WTFMove(newBox));
    return box;
}

Container& TreeBuilder::createContainer(Optional<Box::ElementAttributes> elementAttributes, RenderStyle&& style)
{
    auto newContainer = makeUnique<Container>(elementAttributes, WTFMove(style));
    auto& container = *newContainer;
    m_layoutTreeContent.addContainer(WTFMove(newContainer));
    return container;
}

Box* TreeBuilder::createLayoutBox(const Container& parentContainer, const RenderObject& childRenderer)
{
    auto elementAttributes = [] (const RenderElement& renderer) -> Optional<Box::ElementAttributes> {
        if (renderer.isDocumentElementRenderer())
            return Box::ElementAttributes { Box::ElementType::Document };
        if (auto* element = renderer.element()) {
            if (element->hasTagName(HTMLNames::bodyTag))
                return Box::ElementAttributes { Box::ElementType::Body };
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

    Box* childLayoutBox = nullptr;
    if (is<RenderText>(childRenderer)) {
        auto& textRenderer = downcast<RenderText>(childRenderer);
        // RenderText::text() has already applied text-transform and text-security properties.
        String text = textRenderer.text();
        auto textContent = TextContext { text, canUseSimplifiedTextMeasuring(text, parentContainer.style().fontCascade(), parentContainer.style().collapseWhiteSpace()) };
        if (parentContainer.style().display() == DisplayType::Inline)
            childLayoutBox = &createTextBox(WTFMove(textContent), RenderStyle::clone(parentContainer.style()));
        else
            childLayoutBox = &createTextBox(WTFMove(textContent), RenderStyle::createAnonymousStyleWithDisplay(parentContainer.style(), DisplayType::Inline));
        childLayoutBox->setIsAnonymous();
    } else {
        auto& renderer = downcast<RenderElement>(childRenderer);
        auto displayType = renderer.style().display();

        auto clonedStyle = RenderStyle::clone(renderer.style());

        if (is<RenderLineBreak>(renderer)) {
            clonedStyle.setDisplay(DisplayType::Inline);
            clonedStyle.setFloating(Float::No);
            childLayoutBox = &createBox(elementAttributes(renderer), WTFMove(clonedStyle));
        } else if (is<RenderTable>(renderer)) {
            // Construct the principal table wrapper box (and not the table box itself).
            childLayoutBox = &createContainer(Box::ElementAttributes { Box::ElementType::TableWrapperBox }, WTFMove(clonedStyle));
            childLayoutBox->setIsAnonymous();
        } else if (is<RenderReplaced>(renderer)) {
            if (displayType == DisplayType::Block)
                childLayoutBox = &createBox(elementAttributes(renderer), WTFMove(clonedStyle));
            else
                childLayoutBox = &createBox(elementAttributes(renderer), WTFMove(clonedStyle));
            // FIXME: We don't yet support all replaced elements and this is temporary anyway.
            if (childLayoutBox->replaced())
                childLayoutBox->replaced()->setIntrinsicSize(downcast<RenderReplaced>(renderer).intrinsicSize());
            if (is<RenderImage>(renderer)) {
                auto& imageRenderer = downcast<RenderImage>(renderer);
                if (imageRenderer.shouldDisplayBrokenImageIcon())
                    childLayoutBox->replaced()->setIntrinsicRatio(1);
                if (imageRenderer.cachedImage())
                    childLayoutBox->replaced()->setCachedImage(*imageRenderer.cachedImage());
            }
        } else {
            if (displayType == DisplayType::Block) {
                if (auto offset = accumulatedOffsetForInFlowPositionedContinuation(downcast<RenderBox>(renderer))) {
                    clonedStyle.setTop({ offset->height(), Fixed });
                    clonedStyle.setLeft({ offset->width(), Fixed });
                    childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
                } else
                    childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            } else if (displayType == DisplayType::Inline)
                childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            else if (displayType == DisplayType::InlineBlock)
                childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            else if (displayType == DisplayType::TableCaption || displayType == DisplayType::TableCell) {
                childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            } else if (displayType == DisplayType::TableRowGroup || displayType == DisplayType::TableHeaderGroup || displayType == DisplayType::TableFooterGroup
                || displayType == DisplayType::TableRow || displayType == DisplayType::TableColumnGroup) {
                childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            } else if (displayType == DisplayType::TableColumn) {
                childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
                auto& tableColElement = static_cast<HTMLTableColElement&>(*renderer.element());
                auto columnWidth = tableColElement.width();
                if (!columnWidth.isEmpty())
                    childLayoutBox->setColumnWidth(columnWidth.toInt());
                if (tableColElement.span() > 1)
                    childLayoutBox->setColumnSpan(tableColElement.span());
            } else {
                ASSERT_NOT_IMPLEMENTED_YET();
                return nullptr;
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
    }
    m_layoutTreeContent.addLayoutBoxForRenderer(childRenderer, *childLayoutBox);
    return childLayoutBox;
}

void TreeBuilder::buildTableStructure(const RenderTable& tableRenderer, Container& tableWrapperBox)
{
    // Create caption and table box.
    auto* tableChild = tableRenderer.firstChild();
    while (is<RenderTableCaption>(tableChild)) {
        auto& captionRenderer = *tableChild;
        auto* captionBox = createLayoutBox(tableWrapperBox, captionRenderer);
        appendChild(tableWrapperBox, *captionBox);
        auto& captionContainer = downcast<Container>(*captionBox);
        buildSubTree(downcast<RenderElement>(captionRenderer), captionContainer);
        tableChild = tableChild->nextSibling();
    }

    auto& tableBox = createContainer(Box::ElementAttributes { Box::ElementType::TableBox }, RenderStyle::clone(tableRenderer.style()));
    appendChild(tableWrapperBox, tableBox);
    auto* sectionRenderer = tableChild;
    while (sectionRenderer) {
        auto* sectionBox = createLayoutBox(tableBox, *sectionRenderer);
        appendChild(tableBox, *sectionBox);
        auto& sectionContainer = downcast<Container>(*sectionBox);
        buildSubTree(downcast<RenderElement>(*sectionRenderer), sectionContainer);
        sectionRenderer = sectionRenderer->nextSibling();
    }
}

void TreeBuilder::buildSubTree(const RenderElement& parentRenderer, Container& parentContainer)
{
    for (auto& childRenderer : childrenOfType<RenderObject>(parentRenderer)) {
        auto* childLayoutBox = createLayoutBox(parentContainer, childRenderer);
        appendChild(parentContainer, *childLayoutBox);
        if (childLayoutBox->isTableWrapperBox())
            buildTableStructure(downcast<RenderTable>(childRenderer), downcast<Container>(*childLayoutBox));
        else if (is<Container>(*childLayoutBox))
            buildSubTree(downcast<RenderElement>(childRenderer), downcast<Container>(*childLayoutBox));
    }
}

#if ENABLE(TREE_DEBUGGING)
static void outputInlineRuns(TextStream& stream, const LayoutState& layoutState, const Container& inlineFormattingRoot, unsigned depth)
{
    auto& inlineFormattingState = layoutState.establishedInlineFormattingState(inlineFormattingRoot);
    auto* displayInlineContent = inlineFormattingState.displayInlineContent();
    if (!displayInlineContent)
        return;

    auto& displayRuns = displayInlineContent->runs;
    auto& lineBoxes = displayInlineContent->lineBoxes;

    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";
    stream << "  ";

    stream << "lines are -> ";
    for (auto& lineBox : lineBoxes)
        stream << "[" << lineBox.left() << "," << lineBox.top() << " " << lineBox.width() << "x" << lineBox.height() << "] ";
    stream.nextLine();

    for (auto& displayRun : displayRuns) {
        unsigned printedCharacters = 0;
        while (++printedCharacters <= depth * 2)
            stream << " ";
        stream << "  ";
        if (displayRun.textContext())
            stream << "inline text box";
        else
            stream << "inline box";
        stream << " at (" << displayRun.left() << "," << displayRun.top() << ") size " << displayRun.width() << "x" << displayRun.height();
        if (displayRun.textContext())
            stream << " run(" << displayRun.textContext()->start() << ", " << displayRun.textContext()->end() << ")";
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
    else if (layoutBox.isTableColumnGroup())
        stream << "COL GROUP";
    else if (layoutBox.isTableColumn())
        stream << "COL";
    else if (layoutBox.isTableCell())
        stream << "TD";
    else if (layoutBox.isTableRow())
        stream << "TR";
    else if (layoutBox.isInlineBlockBox())
        stream << "Inline-block";
    else if (layoutBox.isInlineLevelBox()) {
        if (layoutBox.isInlineBox())
            stream << "SPAN inline box";
        else if (layoutBox.replaced())
            stream << "IMG replaced inline box";
        else if (layoutBox.isAnonymous())
            stream << "anonymous inline box";
        else if (layoutBox.isLineBreakBox())
            stream << "BR line break";
        else
            stream << "other inline level box";
    } else if (layoutBox.isBlockLevelBox())
        stream << "block box";
    else
        stream << "unknown box";

    // FIXME: Inline text runs don't create display boxes yet.
    if (displayBox)
        stream << " at (" << displayBox->left() << "," << displayBox->top() << ") size " << displayBox->width() << "x" << displayBox->height();
    stream << " layout box->(" << &layoutBox << ")";
    if (layoutBox.isInlineLevelBox() && layoutBox.isAnonymous())
        stream << " text content [\"" << layoutBox.textContext()->content.utf8().data() << "\"]";

    stream.nextLine();
}

static void outputLayoutTree(const LayoutState* layoutState, TextStream& stream, const Container& rootContainer, unsigned depth)
{
    for (auto& child : childrenOfType<Box>(rootContainer)) {
        if (layoutState) {
            // Not all boxes generate display boxes.
            if (layoutState->hasDisplayBox(child))
                outputLayoutBox(stream, child, &layoutState->displayBoxForLayoutBox(child), depth);
            if (child.establishesInlineFormattingContext())
                outputInlineRuns(stream, *layoutState, downcast<Container>(child), depth + 1);
        } else
            outputLayoutBox(stream, child, nullptr, depth);

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
        auto layoutTreeContent = TreeBuilder::buildLayoutTree(renderView);
        auto layoutState = LayoutState { *document, layoutTreeContent->rootLayoutBox() };

        auto& layoutRoot = layoutState.root();
        auto invalidationState = InvalidationState { };

        LayoutContext(layoutState).layout(renderView.size(), invalidationState);
        showLayoutTree(layoutRoot, &layoutState);
    }
}
#endif

}
}

#endif
