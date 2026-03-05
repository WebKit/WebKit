/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "AXTableHelpers.h"

#include "AXCoreObject.h"
#include "AXObjectCache.h"
#include "AXUtilities.h"
#include "ContainerNodeInlines.h"
#include "Color.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "NodeRenderStyle.h"
#include "RenderElementInlines.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <queue>

namespace WebCore {

namespace AXTableHelpers {

using namespace HTMLNames;

bool appendCaptionTextIfNecessary(Element& element, Vector<AccessibilityText>& textOrder)
{
    if (RefPtr tableElement = dynamicDowncast<HTMLTableElement>(element)) {
        RefPtr caption = tableElement->caption();
        if (String captionText = caption ? caption->innerText() : emptyString(); !captionText.isEmpty()) {
            textOrder.append(AccessibilityText(WTF::move(captionText), AccessibilityTextSource::LabelByElement));
            return true;
        }
    }
    return false;
}

bool isTableRole(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Table:
    case AccessibilityRole::Grid:
    case AccessibilityRole::TreeGrid:
        return true;
    default:
        return false;
    }
}

bool hasRowRole(Element& element)
{
    return hasRole(element, "row"_s);
}

bool isTableRowElement(Element& element)
{
    if (hasRowRole(element))
        return true;

    if (!hasRole(element, nullAtom())) {
        // This has a non-row role, so it shouldn't be considered a row.
        return false;
    }

    bool isAnonymous = false;
    CheckedPtr renderer = element.renderer();
#if USE(ATSPI)
    isAnonymous = renderer && renderer->isAnonymous();
#endif

    if (is<RenderTableRow>(renderer.get()) && !isAnonymous)
        return true;

    return is<HTMLTableRowElement>(element);
}

bool isTableCellElement(Element& element)
{
    if (hasCellARIARole(element))
        return true;

    if (is<HTMLTableCellElement>(element) && hasRole(element, nullAtom()))
        return true;

    bool isAnonymous = false;
    CheckedPtr renderer = element.renderer();
#if USE(ATSPI)
    isAnonymous = renderer && renderer->isAnonymous();
#endif
    return is<RenderTableCell>(renderer) && !isAnonymous;
}

HTMLTableElement* tableElementIncludingAncestors(Node* node, RenderObject* renderer)
{
    if (auto* tableElement = dynamicDowncast<HTMLTableElement>(node))
        return tableElement;

    auto* renderTable = dynamicDowncast<RenderTable>(renderer);
    if (!renderTable)
        return nullptr;

    if (auto* tableElement = dynamicDowncast<HTMLTableElement>(renderTable->element()))
        return tableElement;
    // Try to find the table element when the object is mapped to an anonymous table renderer.
    CheckedPtr firstChild = renderTable->firstChild();
    if (!firstChild || !firstChild->node())
        return nullptr;
    if (auto* childTable = dynamicDowncast<HTMLTableElement>(firstChild->node()))
        return childTable;
    // FIXME: This might find an unrelated parent table element.
    return ancestorsOfType<HTMLTableElement>(*(firstChild->node())).first();
}

bool tableElementIndicatesAccessibleTable(HTMLTableElement& tableElement)
{
    // If there is a caption element, summary, THEAD, or TFOOT section, it's most certainly a data table.
    if (!tableElement.summary().isEmpty()
        || (tableElement.tHead() && tableElement.tHead()->renderer())
        || (tableElement.tFoot() && tableElement.tFoot()->renderer())
        || tableElement.caption())
        return true;

    // If someone used "rules" attribute than the table should appear.
    if (!tableElement.rules().isEmpty())
        return true;

    // If there's a colgroup or col element, it's probably a data table.
    for (const Ref child : childrenOfType<HTMLElement>(tableElement)) {
        auto elementName = child->elementName();
        if (elementName == ElementName::HTML_col || elementName == ElementName::HTML_colgroup)
            return true;
    }
    return false;
}

bool tableSectionIndicatesAccessibleTable(HTMLTableSectionElement& sectionElement, AXObjectCache& cache)
{
    // Use the presence of any non-group role as a sign that the author wants this to be an accessibility table (rather
    // than a layout table).
    if (RefPtr axTableSection = cache.getOrCreate(sectionElement)) {
        auto role = axTableSection->role();
        if (!axTableSection->isGroup() && role != AccessibilityRole::Unknown && role != AccessibilityRole::Ignored)
            return true;
    }
    return false;
}

static const RenderStyle* styleFrom(Element& element)
{
    if (auto* renderStyle = element.renderStyle())
        return renderStyle;
    return element.existingComputedStyle();
}

bool isDataTableWithTraversal(HTMLTableElement& tableElement, AXObjectCache& cache)
{
    bool didTopSectionCheck = false;
    auto topSectionIndicatesLayoutTable = [&] (HTMLTableSectionElement* tableSectionElement) {
        if (didTopSectionCheck || !tableSectionElement)
            return false;
        didTopSectionCheck = true;
        return tableSectionIndicatesAccessibleTable(*tableSectionElement, cache);
    };

    // Store the background color of the table to check against cell's background colors.
    Color tableBackgroundColor = Color::white;
    unsigned tableHorizontalBorderSpacing = 0;
    unsigned tableVerticalBorderSpacing = 0;
    if (CheckedPtr<const RenderStyle> tableStyle = safeStyleFrom(tableElement)) {
        tableBackgroundColor = tableStyle->visitedDependentBackgroundColor();
        tableHorizontalBorderSpacing = tableStyle->borderHorizontalSpacing().resolveZoom(tableStyle->usedZoomForLength());
        tableVerticalBorderSpacing = tableStyle->borderVerticalSpacing().resolveZoom(tableStyle->usedZoomForLength());

    }

    unsigned cellCount = 0;
    unsigned borderedCellCount = 0;
    unsigned backgroundDifferenceCellCount = 0;
    unsigned cellsWithTopBorder = 0;
    unsigned cellsWithBottomBorder = 0;
    unsigned cellsWithLeftBorder = 0;
    unsigned cellsWithRightBorder = 0;

    HashMap<Node*, unsigned> cellCountForEachRow;
    std::array<Color, 5> alternatingRowColors;
    int alternatingRowColorCount = 0;
    unsigned rowCount = 0;
    unsigned maxColumnCount = 0;

    auto isDataTableBasedOnRowColumnCount = [&] () {
        // If there are at least 20 rows, we'll call it a data table.
        return (rowCount >= 20 && maxColumnCount >= 2) || (rowCount >= 2 && maxColumnCount >= 20);
    };

    bool firstColumnHasAllHeaderCells = true;
    RefPtr<HTMLTableRowElement> firstRow;
    RefPtr<HTMLTableSectionElement> firstBody;
    RefPtr<HTMLTableSectionElement> firstFoot;

    // Do a breadth-first search to determine if this is a data table.
    std::queue<RefPtr<Element>> elementsToVisit;
    elementsToVisit.push(tableElement);
    while (!elementsToVisit.empty()) {
        RefPtr currentParent = elementsToVisit.front();
        elementsToVisit.pop();
        bool rowIsAllTableHeaderCells = true;
        for (RefPtr currentElement = currentParent ? currentParent->firstElementChild() : nullptr; currentElement; currentElement = currentElement->nextElementSibling()) {
            if (auto* tableSectionElement = dynamicDowncast<HTMLTableSectionElement>(currentElement.get())) {
                auto elementName = tableSectionElement->elementName();
                if (elementName == ElementName::HTML_thead) {
                    if (topSectionIndicatesLayoutTable(tableSectionElement))
                        return false;
                } else if (elementName == ElementName::HTML_tbody)
                    firstBody = firstBody ? firstBody : RefPtr { tableSectionElement };
                else {
                    ASSERT_WITH_MESSAGE(elementName == ElementName::HTML_tfoot, "table section elements should always have either thead, tbody, or tfoot tag");
                    firstFoot = firstFoot ? firstFoot : RefPtr { tableSectionElement };
                }
            } else if (auto* tableRow = dynamicDowncast<HTMLTableRowElement>(currentElement.get())) {
                firstRow = firstRow ? firstRow : RefPtr { tableRow };

                rowCount += 1;
                if (isDataTableBasedOnRowColumnCount())
                    return true;

                if (tableRow->integralAttribute(aria_rowindexAttr) >= 1 || tableRow->integralAttribute(aria_colindexAttr) || !tableRow->getAttribute(aria_rowindextextAttr).isEmpty() || hasRole(*tableRow, "row"_s))
                    return true;

                // For the first 5 rows, cache the background color so we can check if this table has zebra-striped rows.
                if (alternatingRowColorCount < 5) {
                    if (CheckedPtr<const RenderStyle> rowStyle = styleFrom(*tableRow)) {
                        alternatingRowColors[alternatingRowColorCount] = rowStyle->visitedDependentBackgroundColor();
                        alternatingRowColorCount++;
                    }
                }
            } else if (auto* cell = dynamicDowncast<HTMLTableCellElement>(currentElement.get())) {
                cellCount++;

                bool isTHCell = cell->elementName() == ElementName::HTML_th;
                if (!isTHCell && rowIsAllTableHeaderCells)
                    rowIsAllTableHeaderCells = false;
                if (RefPtr parentNode = cell->parentNode()) {
                    auto cellCountForRowIterator = cellCountForEachRow.ensure(parentNode.get(), [&] {
                        // If we don't have an entry for this parent yet, it must be the first column.
                        if (!isTHCell && firstColumnHasAllHeaderCells)
                            firstColumnHasAllHeaderCells = false;
                        return 0;
                    }).iterator;
                    cellCountForRowIterator->value += 1;
                    maxColumnCount = std::max(cellCountForRowIterator->value, maxColumnCount);
                    if (isDataTableBasedOnRowColumnCount())
                        return true;
                }

                // In this case, the developer explicitly assigned a "data" table attribute.
                if (!cell->headers().isEmpty() || !cell->abbr().isEmpty() || !cell->axis().isEmpty() || !cell->scope().isEmpty() || hasCellARIARole(*cell))
                    return true;

                // If the author has used ARIA to specify a valid column or row index or index text, assume they want us
                // to treat the table as a data table.
                if (cell->integralAttribute(aria_colindexAttr) >= 1 || cell->integralAttribute(aria_rowindexAttr) >= 1 || !cell->getAttribute(aria_colindextextAttr).isEmpty() || !cell->getAttribute(aria_rowindextextAttr).isEmpty())
                    return true;

                // If the author has used ARIA to specify a column or row span, we're supposed to ignore
                // the value for the purposes of exposing the span. But assume they want us to treat the
                // table as a data table.
                if (cell->integralAttribute(aria_colspanAttr) >= 1 || cell->integralAttribute(aria_rowspanAttr) >= 1)
                    return true;

                Color cellColor = Color::white;
                if (CheckedPtr<const RenderStyle> cellStyle = styleFrom(*cell)) {
                    if (cellStyle->emptyCells() == EmptyCell::Hide) {
                        // If the empty-cells style is set, we'll call it a data table.
                        return true;
                    }
                    cellColor = cellStyle->visitedDependentBackgroundColor();
                }

                if (CheckedPtr cellRenderer = dynamicDowncast<RenderBlock>(cell->renderer())) {
                    bool hasBorderTop = cellRenderer->borderTop() > 0;
                    bool hasBorderBottom = cellRenderer->borderBottom() > 0;
                    bool hasBorderLeft = cellRenderer->borderLeft() > 0;
                    bool hasBorderRight = cellRenderer->borderRight() > 0;
                    // If a cell has matching bordered sides, call it a (fully) bordered cell.
                    if ((hasBorderTop && hasBorderBottom) || (hasBorderLeft && hasBorderRight))
                        borderedCellCount++;

                    // Also keep track of each individual border, so we can catch tables where most
                    // cells have a bottom border, for example.
                    if (hasBorderTop)
                        cellsWithTopBorder++;
                    if (hasBorderBottom)
                        cellsWithBottomBorder++;
                    if (hasBorderLeft)
                        cellsWithLeftBorder++;
                    if (hasBorderRight)
                        cellsWithRightBorder++;
                }

                // If the cell has a different color from the table and there is cell spacing,
                // then it is probably a data table cell (spacing and colors take the place of borders).
                if (tableHorizontalBorderSpacing > 0 && tableVerticalBorderSpacing > 0 && tableBackgroundColor != cellColor && !cellColor.isOpaque())
                    backgroundDifferenceCellCount++;

                // If we've found 10 "good" cells, we don't need to keep searching.
                if (borderedCellCount >= 10 || backgroundDifferenceCellCount >= 10)
                    return true;
            } else if (is<HTMLTableElement>(currentElement)) {
                // Do not descend into nested tables. (Implemented by continuing before pushing the current element into the BFS elementsToVisit queue)
                continue;
            }
            elementsToVisit.push(currentElement);
        }

        // If the first row of a multi-row table is comprised of all <th> tags, assume it is a data table.
        if (firstRow && currentParent == firstRow && rowIsAllTableHeaderCells && cellCountForEachRow.get(currentParent.get()) >= 1 && rowCount >= 2)
            return true;
    }

    // If there is less than two valid cells, it's not a data table.
    if (cellCount <= 1)
        return false;

    if (topSectionIndicatesLayoutTable(firstBody.get()) || topSectionIndicatesLayoutTable(firstFoot.get()))
        return false;

    if (firstColumnHasAllHeaderCells && rowCount >= 2)
        return true;

    // At least half of the cells had borders, it's a data table.
    unsigned neededCellCount = cellCount / 2;
    if (borderedCellCount >= neededCellCount
        || cellsWithTopBorder >= neededCellCount
        || cellsWithBottomBorder >= neededCellCount
        || cellsWithLeftBorder >= neededCellCount
        || cellsWithRightBorder >= neededCellCount)
        return true;

    // At least half of the cells had different background colors, it's a data table.
    if (backgroundDifferenceCellCount >= neededCellCount)
        return true;

    if (isDataTableBasedOnRowColumnCount())
        return true;

    // Check if there is an alternating row background color indicating a zebra striped style pattern.
    if (alternatingRowColorCount > 2) {
        Color firstColor = alternatingRowColors[0];
        for (int k = 1; k < alternatingRowColorCount; k++) {
            // If an odd row was the same color as the first row, it's not alternating.
            if (k % 2 == 1 && alternatingRowColors[k] == firstColor)
                return false;
            // If an even row is not the same as the first row, it's not alternating.
            if (!(k % 2) && alternatingRowColors[k] != firstColor)
                return false;
        }
        return true;
    }
    return false;
}

} // namespace AXTableHelpers

} // namespace WebCore
