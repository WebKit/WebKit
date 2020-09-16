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
#include "DisplayRun.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableColElement.h"
#include "HTMLTableElement.h"
#include "InlineFormattingState.h"
#include "InvalidationContext.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutLineBreakBox.h"
#include "LayoutPhase.h"
#include "LayoutReplacedBox.h"
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
LayoutTreeContent::LayoutTreeContent(const RenderBox& rootRenderer, std::unique_ptr<ContainerBox> rootLayoutBox)
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

static void appendChild(ContainerBox& parent, Box& newChild)
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

    auto layoutTreeContent = makeUnique<LayoutTreeContent>(renderView, makeUnique<InitialContainingBlock>(WTFMove(style)));
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

Box& TreeBuilder::createReplacedBox(Optional<Box::ElementAttributes> elementAttributes, RenderStyle&& style)
{
    auto newBox = makeUnique<ReplacedBox>(elementAttributes, WTFMove(style));
    auto& box = *newBox;
    m_layoutTreeContent.addBox(WTFMove(newBox));
    return box;
}

Box& TreeBuilder::createTextBox(String text, bool canUseSimplifiedTextMeasuring, RenderStyle&& style)
{
    auto newBox = makeUnique<InlineTextBox>(text, canUseSimplifiedTextMeasuring, WTFMove(style));
    auto& box = *newBox;
    m_layoutTreeContent.addBox(WTFMove(newBox));
    return box;
}

Box& TreeBuilder::createLineBreakBox(bool isOptional, RenderStyle&& style)
{
    auto newBox = makeUnique<Layout::LineBreakBox>(isOptional, WTFMove(style));
    auto& box = *newBox;
    m_layoutTreeContent.addBox(WTFMove(newBox));
    return box;
}

ContainerBox& TreeBuilder::createContainer(Optional<Box::ElementAttributes> elementAttributes, RenderStyle&& style)
{
    auto newContainer = makeUnique<ContainerBox>(elementAttributes, WTFMove(style));
    auto& container = *newContainer;
    m_layoutTreeContent.addContainer(WTFMove(newContainer));
    return container;
}

Box* TreeBuilder::createLayoutBox(const ContainerBox& parentContainer, const RenderObject& childRenderer)
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
            return Box::ElementAttributes { Box::ElementType::GenericElement };
        }
        return WTF::nullopt;
    };

    Box* childLayoutBox = nullptr;
    if (is<RenderText>(childRenderer)) {
        auto& textRenderer = downcast<RenderText>(childRenderer);
        // RenderText::text() has already applied text-transform and text-security properties.
        String text = textRenderer.text();
        auto useSimplifiedTextMeasuring = canUseSimplifiedTextMeasuring(text, parentContainer.style().fontCascade(), parentContainer.style().collapseWhiteSpace());
        if (parentContainer.style().display() == DisplayType::Inline)
            childLayoutBox = &createTextBox(text, useSimplifiedTextMeasuring, RenderStyle::clone(parentContainer.style()));
        else
            childLayoutBox = &createTextBox(text, useSimplifiedTextMeasuring, RenderStyle::createAnonymousStyleWithDisplay(parentContainer.style(), DisplayType::Inline));
    } else {
        auto& renderer = downcast<RenderElement>(childRenderer);
        auto displayType = renderer.style().display();

        auto clonedStyle = RenderStyle::clone(renderer.style());

        if (is<RenderLineBreak>(renderer)) {
            clonedStyle.setDisplay(DisplayType::Inline);
            clonedStyle.setFloating(Float::No);
            childLayoutBox = &createLineBreakBox(downcast<RenderLineBreak>(childRenderer).isWBR(), WTFMove(clonedStyle));
        } else if (is<RenderTable>(renderer)) {
            // Construct the principal table wrapper box (and not the table box itself).
            // The computed values of properties 'position', 'float', 'margin-*', 'top', 'right', 'bottom', and 'left' on the table element
            // are used on the table wrapper box and not the table box; all other values of non-inheritable properties are used
            // on the table box and not the table wrapper box.
            auto tableWrapperBoxStyle = RenderStyle::createAnonymousStyleWithDisplay(parentContainer.style(), renderer.style().display() == DisplayType::Table ? DisplayType::Block : DisplayType::Inline);
            tableWrapperBoxStyle.setPosition(renderer.style().position());
            tableWrapperBoxStyle.setFloating(renderer.style().floating());

            tableWrapperBoxStyle.setTop(Length { renderer.style().top() });
            tableWrapperBoxStyle.setLeft(Length { renderer.style().left() });
            tableWrapperBoxStyle.setBottom(Length { renderer.style().bottom() });
            tableWrapperBoxStyle.setRight(Length { renderer.style().right() });

            tableWrapperBoxStyle.setMarginTop(Length { renderer.style().marginTop() });
            tableWrapperBoxStyle.setMarginLeft(Length { renderer.style().marginLeft() });
            tableWrapperBoxStyle.setMarginBottom(Length { renderer.style().marginBottom() });
            tableWrapperBoxStyle.setMarginRight(Length { renderer.style().marginRight() });

            childLayoutBox = &createContainer(Box::ElementAttributes { Box::ElementType::TableWrapperBox }, WTFMove(tableWrapperBoxStyle));
            childLayoutBox->setIsAnonymous();
        } else if (is<RenderReplaced>(renderer)) {
            childLayoutBox = &createReplacedBox(elementAttributes(renderer), WTFMove(clonedStyle));
            // FIXME: We don't yet support all replaced elements and this is temporary anyway.
            downcast<ReplacedBox>(*childLayoutBox).setIntrinsicSize(downcast<RenderReplaced>(renderer).intrinsicSize());
            if (is<RenderImage>(renderer)) {
                auto& imageRenderer = downcast<RenderImage>(renderer);
                if (imageRenderer.shouldDisplayBrokenImageIcon())
                    downcast<ReplacedBox>(*childLayoutBox).setIntrinsicRatio(1);
                if (imageRenderer.cachedImage())
                    downcast<ReplacedBox>(*childLayoutBox).setCachedImage(*imageRenderer.cachedImage());
            }
        } else {
            if (displayType == DisplayType::Block) {
                if (auto offset = accumulatedOffsetForInFlowPositionedContinuation(downcast<RenderBox>(renderer))) {
                    clonedStyle.setTop({ offset->height(), Fixed });
                    clonedStyle.setLeft({ offset->width(), Fixed });
                    childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
                } else
                    childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            } else if (displayType == DisplayType::Flex)
                childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            else if (displayType == DisplayType::Inline)
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
                // Let's fall back to a regular block level container when the renderer type is not yet supported.
                clonedStyle.setDisplay(DisplayType::Block);
                childLayoutBox = &createContainer(elementAttributes(renderer), WTFMove(clonedStyle));
            }
        }

        if (is<RenderTableCell>(renderer)) {
            auto* tableCellElement = renderer.element();
            if (is<HTMLTableCellElement>(tableCellElement)) {
                auto& cellElement = downcast<HTMLTableCellElement>(*tableCellElement);
                auto rowSpan = cellElement.rowSpan();
                if (rowSpan > 1)
                    childLayoutBox->setRowSpan(rowSpan);

                auto columnSpan = cellElement.colSpan();
                if (columnSpan > 1)
                    childLayoutBox->setColumnSpan(columnSpan);
            }
        }

        if (childRenderer.isAnonymous())
            childLayoutBox->setIsAnonymous();
    }
    m_layoutTreeContent.addLayoutBoxForRenderer(childRenderer, *childLayoutBox);
    return childLayoutBox;
}

void TreeBuilder::buildTableStructure(const RenderTable& tableRenderer, ContainerBox& tableWrapperBox)
{
    // Create caption and table box.
    auto* tableChild = tableRenderer.firstChild();
    while (is<RenderTableCaption>(tableChild)) {
        auto& captionRenderer = *tableChild;
        auto* captionBox = createLayoutBox(tableWrapperBox, captionRenderer);
        appendChild(tableWrapperBox, *captionBox);
        auto& captionContainer = downcast<ContainerBox>(*captionBox);
        buildSubTree(downcast<RenderElement>(captionRenderer), captionContainer);
        tableChild = tableChild->nextSibling();
    }

    auto tableBoxStyle = RenderStyle::clone(tableRenderer.style());
    tableBoxStyle.setPosition(PositionType::Static);
    tableBoxStyle.setFloating(Float::No);
    tableBoxStyle.resetMargin();
    // FIXME: Figure out where the spec says table width is like box-sizing: border-box;
    if (is<HTMLTableElement>(tableRenderer.element()))
        tableBoxStyle.setBoxSizing(BoxSizing::BorderBox);
    auto& tableBox = createContainer(Box::ElementAttributes { Box::ElementType::TableBox }, WTFMove(tableBoxStyle));
    appendChild(tableWrapperBox, tableBox);
    auto* sectionRenderer = tableChild;
    while (sectionRenderer) {
        auto* sectionBox = createLayoutBox(tableBox, *sectionRenderer);
        appendChild(tableBox, *sectionBox);
        auto& sectionContainer = downcast<ContainerBox>(*sectionBox);
        buildSubTree(downcast<RenderElement>(*sectionRenderer), sectionContainer);
        sectionRenderer = sectionRenderer->nextSibling();
    }
    auto addMissingTableCells = [&] (auto& tableBody) {
        // A "missing cell" is a cell in the row/column grid that is not occupied by an element or pseudo-element.
        // Missing cells are rendered as if an anonymous table-cell box occupied their position in the grid.

        // Find the max number of columns and fill in the gaps.
        size_t maximumColumns = 0;
        size_t currentRow = 0;
        Vector<size_t> numberOfCellsPerRow;
        for (auto& rowBox : childrenOfType<ContainerBox>(tableBody)) {
            if (numberOfCellsPerRow.size() <= currentRow) {
                // Ensure we always have a vector entry for the current row -even when the row is empty.
                numberOfCellsPerRow.append({ });
            }
            for (auto& cellBox : childrenOfType<ContainerBox>(rowBox)) {
                auto numberOfSpannedColumns = cellBox.columnSpan();
                for (size_t rowSpan = 0; rowSpan < cellBox.rowSpan(); ++rowSpan) {
                    auto rowIndexWithSpan = currentRow + rowSpan;
                    if (numberOfCellsPerRow.size() <= rowIndexWithSpan) {
                        // This is where we advance from the current row by having a row spanner.
                        numberOfCellsPerRow.append(numberOfSpannedColumns);
                        continue;
                    }
                    numberOfCellsPerRow[rowIndexWithSpan] += numberOfSpannedColumns;
                }
            }
            maximumColumns = std::max(maximumColumns, numberOfCellsPerRow[currentRow]);
            ++currentRow;
        }
        // Fill in the gaps.
        size_t rowIndex = 0;
        for (auto& rowBox : childrenOfType<ContainerBox>(tableBody)) {
            ASSERT(maximumColumns >= numberOfCellsPerRow[rowIndex]);
            auto numberOfMissingCells = maximumColumns - numberOfCellsPerRow[rowIndex++];
            for (size_t i = 0; i < numberOfMissingCells; ++i)
                appendChild(const_cast<ContainerBox&>(rowBox), createContainer({ }, RenderStyle::createAnonymousStyleWithDisplay(rowBox.style(), DisplayType::TableCell)));
        }
    };

    for (auto& section : childrenOfType<ContainerBox>(tableBox)) {
        // FIXME: Check if headers and footers need the same treatment.
        if (!section.isTableBody())
            continue;
        addMissingTableCells(section);
    }
}

void TreeBuilder::buildSubTree(const RenderElement& parentRenderer, ContainerBox& parentContainer)
{
    for (auto& childRenderer : childrenOfType<RenderObject>(parentRenderer)) {
        auto* childLayoutBox = createLayoutBox(parentContainer, childRenderer);
        appendChild(parentContainer, *childLayoutBox);
        if (childLayoutBox->isTableWrapperBox())
            buildTableStructure(downcast<RenderTable>(childRenderer), downcast<ContainerBox>(*childLayoutBox));
        else if (is<ContainerBox>(*childLayoutBox))
            buildSubTree(downcast<RenderElement>(childRenderer), downcast<ContainerBox>(*childLayoutBox));
    }
}

#if ENABLE(TREE_DEBUGGING)
static void outputInlineRuns(TextStream& stream, const LayoutState& layoutState, const ContainerBox& inlineFormattingRoot, unsigned depth)
{
    auto& inlineFormattingState = layoutState.establishedInlineFormattingState(inlineFormattingRoot);
    auto* displayInlineContent = inlineFormattingState.displayInlineContent();
    if (!displayInlineContent)
        return;

    auto& displayRuns = displayInlineContent->runs;
    auto& lines = displayInlineContent->lines;

    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";
    stream << "  ";

    stream << "lines are -> ";
    for (auto& line : lines)
        stream << "[" << line.left() << "," << line.top() << " " << line.width() << "x" << line.height() << "][baseline: " << line.baseline() << "]";
    stream.nextLine();

    for (auto& displayRun : displayRuns) {
        unsigned printedCharacters = 0;
        while (++printedCharacters <= depth * 2)
            stream << " ";
        stream << "  ";
        if (displayRun.textContent())
            stream << "inline text box";
        else
            stream << "inline box";
        stream << " at (" << displayRun.left() << "," << displayRun.top() << ") size " << displayRun.width() << "x" << displayRun.height();
        if (displayRun.textContent())
            stream << " run(" << displayRun.textContent()->start() << ", " << displayRun.textContent()->end() << ")";
        stream.nextLine();
    }
}

static void outputLayoutBox(TextStream& stream, const Box& layoutBox, const BoxGeometry* boxGeometry, unsigned depth)
{
    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";

    if (layoutBox.isFloatingPositioned())
        stream << "[float] ";

    if (is<InitialContainingBlock>(layoutBox))
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
    else if (layoutBox.isInlineLevelBox()) {
        if (layoutBox.isAnonymous())
            stream << "anonymous inline box";
        else if (layoutBox.isInlineBlockBox())
            stream << "inline-block box";
        else if (layoutBox.isInlineBox())
            stream << "inline box";
        else if (layoutBox.isAtomicInlineLevelBox())
            stream << "atomic inline level box";
        else if (layoutBox.isReplacedBox())
            stream << "replaced inline box";
        else if (layoutBox.isLineBreakBox())
            stream << "line break";
        else
            stream << "other inline level box";
    } else if (layoutBox.isBlockLevelBox())
        stream << "block box";
    else
        stream << "unknown box";

    if (boxGeometry)
        stream << " at (" << boxGeometry->left() << "," << boxGeometry->top() << ") size " << boxGeometry->width() << "x" << boxGeometry->height();
    stream << " (" << &layoutBox << ")";
    if (is<InlineTextBox>(layoutBox)) {
        auto textContent = downcast<InlineTextBox>(layoutBox).content();
        stream << " length->(" << textContent.length() << ")";

        textContent.replaceWithLiteral('\\', "\\\\");
        textContent.replaceWithLiteral('\n', "\\n");

        const size_t maxPrintedLength = 80;
        if (textContent.length() > maxPrintedLength) {
            auto substring = textContent.substring(0, maxPrintedLength);
            stream << " \"" << substring.utf8().data() << "\"...";
        } else
            stream << " \"" << textContent.utf8().data() << "\"";
    }
    stream.nextLine();
}

static void outputLayoutTree(const LayoutState* layoutState, TextStream& stream, const ContainerBox& rootContainer, unsigned depth)
{
    for (auto& child : childrenOfType<Box>(rootContainer)) {
        if (layoutState) {
            // Not all boxes generate display boxes.
            if (layoutState->hasBoxGeometry(child))
                outputLayoutBox(stream, child, &layoutState->geometryForBox(child), depth);
            else
                outputLayoutBox(stream, child, nullptr, depth);
            if (child.establishesInlineFormattingContext())
                outputInlineRuns(stream, *layoutState, downcast<ContainerBox>(child), depth + 1);
        } else
            outputLayoutBox(stream, child, nullptr, depth);

        if (is<ContainerBox>(child))
            outputLayoutTree(layoutState, stream, downcast<ContainerBox>(child), depth + 1);
    }
}

void showLayoutTree(const Box& layoutBox, const LayoutState* layoutState)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    auto& initialContainingBlock = layoutBox.initialContainingBlock();
    outputLayoutBox(stream, initialContainingBlock, layoutState ? &layoutState->geometryForBox(initialContainingBlock) : nullptr, 0);
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
