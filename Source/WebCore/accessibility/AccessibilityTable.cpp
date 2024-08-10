/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityTable.h"

#include "AXObjectCache.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableHeaderContainer.h"
#include "AccessibilityTableRow.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "NodeRenderStyle.h"
#include "RenderObject.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include <wtf/Scope.h>
#include <wtf/WeakRef.h>

#include <queue>

namespace WebCore {

using namespace HTMLNames;

AccessibilityTable::AccessibilityTable(RenderObject& renderer)
    : AccessibilityRenderObject(renderer)
    , m_headerContainer(nullptr)
    , m_isExposable(true)
{
}

AccessibilityTable::AccessibilityTable(Node& node)
    : AccessibilityRenderObject(node)
    , m_headerContainer(nullptr)
    , m_isExposable(true)
{
}

AccessibilityTable::~AccessibilityTable() = default;

void AccessibilityTable::init()
{
    AccessibilityRenderObject::init();
    m_isExposable = computeIsTableExposableThroughAccessibility();
}

Ref<AccessibilityTable> AccessibilityTable::create(RenderObject& renderer)
{
    return adoptRef(*new AccessibilityTable(renderer));
}

Ref<AccessibilityTable> AccessibilityTable::create(Node& node)
{
    return adoptRef(*new AccessibilityTable(node));
}

bool AccessibilityTable::hasNonTableARIARole() const
{
    switch (ariaRoleAttribute()) {
    case AccessibilityRole::Unknown: // No role attribute specified.
    case AccessibilityRole::Table:
    case AccessibilityRole::Grid:
    case AccessibilityRole::TreeGrid:
        return false;
    default:
        return true;
    }
}

HTMLTableElement* AccessibilityTable::tableElement() const
{
    if (auto* tableElement = dynamicDowncast<HTMLTableElement>(node()))
        return tableElement;

    auto* renderTable = dynamicDowncast<RenderTable>(renderer());
    if (!renderTable)
        return nullptr;

    if (auto* tableElement = dynamicDowncast<HTMLTableElement>(renderTable->element()))
        return tableElement;
    // Try to find the table element, when the AccessibilityTable is mapped to an anonymous table renderer.
    auto* firstChild = renderTable->firstChild();
    if (!firstChild || !firstChild->node())
        return nullptr;
    if (auto* childTable = dynamicDowncast<HTMLTableElement>(firstChild->node()))
        return childTable;
    // FIXME: This might find an unrelated parent table element.
    return ancestorsOfType<HTMLTableElement>(*(firstChild->node())).first();
}

static const RenderStyle* styleFrom(Element& element)
{
    if (auto* renderStyle = element.renderStyle())
        return renderStyle;
    return element.existingComputedStyle();
}

bool AccessibilityTable::isDataTable() const
{
    WeakPtr cache = axObjectCache();
    if (!cache)
        return false;

    // Do not consider it a data table if it has a non-table ARIA role.
    if (hasNonTableARIARole())
        return false;

    // When a section of the document is contentEditable, all tables should be
    // treated as data tables, otherwise users may not be able to work with rich
    // text editors that allow creating and editing tables.
    if (node() && node()->hasEditableStyle())
        return true;

    // This employs a heuristic to determine if this table should appear.
    // Only "data" tables should be exposed as tables.
    // Unfortunately, there is no good way to determine the difference
    // between a "layout" table and a "data" table.
    if (WeakPtr tableElement = this->tableElement()) {
        // If there is a caption element, summary, THEAD, or TFOOT section, it's most certainly a data table.
        if (!tableElement->summary().isEmpty()
            || (tableElement->tHead() && tableElement->tHead()->renderer())
            || (tableElement->tFoot() && tableElement->tFoot()->renderer())
            || tableElement->caption())
            return true;
        
        // If someone used "rules" attribute than the table should appear.
        if (!tableElement->rules().isEmpty())
            return true;

        // If there's a colgroup or col element, it's probably a data table.
        for (const auto& child : childrenOfType<HTMLElement>(*tableElement)) {
            if (child.hasTagName(colTag) || child.hasTagName(colgroupTag))
                return true;
        }
    }
    
    // The following checks should only apply if this is a real <table> element.
    if (!hasTagName(tableTag))
        return false;
    
    // If the author has used ARIA to specify a valid column or row count, assume they
    // want us to treat the table as a data table.
    auto ariaRowOrColCountIsSet = [this] (const QualifiedName& attribute) {
        int result = getIntegralAttribute(attribute);
        return result == -1 || result > 0;
    };
    if (ariaRowOrColCountIsSet(aria_colcountAttr) || ariaRowOrColCountIsSet(aria_rowcountAttr))
        return true;

    bool didTopSectionCheck = false;
    auto topSectionIndicatesLayoutTable = [&] (HTMLTableSectionElement* tableSectionElement) {
        if (didTopSectionCheck || !tableSectionElement)
            return false;
        didTopSectionCheck = true;

        // If the top section has any non-group role, then don't make this a data table. The author probably wants to use the role on the section.
        if (auto* axTableSection = cache->getOrCreate(*tableSectionElement)) {
            auto role = axTableSection->roleValue();
            if (!axTableSection->isGroup() && role != AccessibilityRole::Unknown && role != AccessibilityRole::Ignored)
                return true;
        }
        return false;
    };

    // Store the background color of the table to check against cell's background colors.
    const auto* tableStyle = this->style();
    Color tableBackgroundColor = tableStyle ? tableStyle->visitedDependentColor(CSSPropertyBackgroundColor) : Color::white;
    unsigned tableHorizontalBorderSpacing = tableStyle ? tableStyle->horizontalBorderSpacing() : 0;
    unsigned tableVerticalBorderSpacing = tableStyle ? tableStyle->verticalBorderSpacing() : 0;

    unsigned cellCount = 0;
    unsigned borderedCellCount = 0;
    unsigned backgroundDifferenceCellCount = 0;
    unsigned cellsWithTopBorder = 0;
    unsigned cellsWithBottomBorder = 0;
    unsigned cellsWithLeftBorder = 0;
    unsigned cellsWithRightBorder = 0;

    HashMap<Node*, unsigned> cellCountForEachRow;
    Color alternatingRowColors[5];
    int alternatingRowColorCount = 0;
    unsigned rowCount = 0;
    unsigned maxColumnCount = 0;

    auto isDataTableBasedOnRowColumnCount = [&] () {
        // If there are at least 20 rows, we'll call it a data table.
        return (rowCount >= 20 && maxColumnCount >= 2) || (rowCount >= 2 && maxColumnCount >= 20);
    };

    bool firstColumnHasAllHeaderCells = true;
    WeakPtr<HTMLTableRowElement, WeakPtrImplWithEventTargetData> firstRow;
    WeakPtr<HTMLTableSectionElement, WeakPtrImplWithEventTargetData> firstBody;
    WeakPtr<HTMLTableSectionElement, WeakPtrImplWithEventTargetData> firstFoot;

    // Do a breadth-first search to determine if this is a data table.
    std::queue<WeakPtr<Element, WeakPtrImplWithEventTargetData>> elementsToVisit;
    elementsToVisit.push(dynamicDowncast<Element>(node()));
    while (!elementsToVisit.empty()) {
        WeakPtr currentParent = elementsToVisit.front();
        elementsToVisit.pop();
        bool rowIsAllTableHeaderCells = true;
        for (RefPtr currentElement = currentParent ? currentParent->firstElementChild() : nullptr; currentElement; currentElement = currentElement->nextElementSibling()) {
            if (auto* tableSectionElement = dynamicDowncast<HTMLTableSectionElement>(currentElement.get())) {
                if (tableSectionElement->hasTagName(theadTag)) {
                    if (topSectionIndicatesLayoutTable(tableSectionElement))
                        return false;
                } else if (tableSectionElement->hasTagName(tbodyTag))
                    firstBody = firstBody ? firstBody : tableSectionElement;
                else {
                    ASSERT_WITH_MESSAGE(tableSectionElement->hasTagName(tfootTag), "table section elements should always have either thead, tbody, or tfoot tag");
                    firstFoot = firstFoot ? firstFoot : tableSectionElement;
                }
            } else if (auto* tableRow = dynamicDowncast<HTMLTableRowElement>(currentElement.get())) {
                firstRow = firstRow ? firstRow : tableRow;

                rowCount += 1;
                if (isDataTableBasedOnRowColumnCount())
                    return true;

                if (tableRow->getIntegralAttribute(aria_rowindexAttr) >= 1 || tableRow->getIntegralAttribute(aria_colindexAttr) || nodeHasRole(tableRow, "row"_s))
                    return true;

                // For the first 5 rows, cache the background color so we can check if this table has zebra-striped rows.
                if (alternatingRowColorCount < 5) {
                    if (const auto* rowStyle = styleFrom(*tableRow)) {
                        alternatingRowColors[alternatingRowColorCount] = rowStyle->visitedDependentColor(CSSPropertyBackgroundColor);
                        alternatingRowColorCount++;
                    }
                }
            } else if (auto* cell = dynamicDowncast<HTMLTableCellElement>(currentElement.get())) {
                cellCount++;

                bool isTHCell = cell->hasTagName(thTag);
                if (!isTHCell && rowIsAllTableHeaderCells)
                    rowIsAllTableHeaderCells = false;
                if (auto* parentNode = cell->parentNode()) {
                    auto cellCountForRowIterator = cellCountForEachRow.ensure(parentNode, [&] {
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
                if (!cell->headers().isEmpty() || !cell->abbr().isEmpty() || !cell->axis().isEmpty() || !cell->scope().isEmpty() || nodeHasCellRole(cell))
                    return true;

                // If the author has used ARIA to specify a valid column or row index, assume they want us
                // to treat the table as a data table.
                if (cell->getIntegralAttribute(aria_colindexAttr) >= 1 || cell->getIntegralAttribute(aria_rowindexAttr) >= 1)
                    return true;

                // If the author has used ARIA to specify a column or row span, we're supposed to ignore
                // the value for the purposes of exposing the span. But assume they want us to treat the
                // table as a data table.
                if (cell->getIntegralAttribute(aria_colspanAttr) >= 1 || cell->getIntegralAttribute(aria_rowspanAttr) >= 1)
                    return true;

                const auto* cellStyle = styleFrom(*cell);
                // If the empty-cells style is set, we'll call it a data table.
                if (cellStyle && cellStyle->emptyCells() == EmptyCell::Hide)
                    return true;

                if (auto* cellRenderer = dynamicDowncast<RenderBlock>(cell->renderer())) {
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
                Color cellColor = cellStyle ? cellStyle->visitedDependentColor(CSSPropertyBackgroundColor) : Color::white;
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

// The following is a heuristic used to determine if a <table> should be exposed as an AXTable.
// The goal is to only show "data" tables.
bool AccessibilityTable::computeIsTableExposableThroughAccessibility() const
{
    // If it has a non-table ARIA role, it shouldn't be exposed as a table.
    if (hasNonTableARIARole())
        return false;

    return isDataTable();
}

void AccessibilityTable::recomputeIsExposable()
{
    // Make sure children are up-to-date, because if we do end up changing is-exposed state, we want to make
    // sure updateChildrenRoles iterates over those children before they change.
    updateChildrenIfNecessary();

    bool previouslyExposable = m_isExposable;
    m_isExposable = computeIsTableExposableThroughAccessibility();
    if (previouslyExposable != m_isExposable) {
        // A table's role value is dependent on whether it's exposed, so notify the cache this has changed.
        if (auto* cache = axObjectCache())
            cache->handleRoleChanged(*this);

        // Before resetting our existing children, possibly losing references to them, ensure we update their role (since a table cell's role is dependent on whether its parent table is exposable).
        updateChildrenRoles();

        m_childrenDirty = true;
    }
}

Vector<Vector<AXID>> AccessibilityTable::cellSlots()
{
    updateChildrenIfNecessary();
    return m_cellSlots;
}

void AccessibilityTable::setCellSlotsDirty()
{
    // Because the cell-slots grid is (necessarily) computed in conjunction with children, mark
    // the children as dirty by clearing them.
    //
    // It's necessary to compute the cell-slots grid together with children because they are both
    // influenced by the same factors. For example, if `setCellSlotsDirty` is called because
    // a child increased in column span, that may also result in more column children being
    // added if that column span change increased the "width" of the table.
    clearChildren();
}

void AccessibilityTable::updateChildrenRoles()
{
    for (const auto& row : m_rows) {
        downcast<AccessibilityObject>(*row).updateRole();
        for (const auto& cell : row->children())
            downcast<AccessibilityObject>(*cell).updateRole();
    }
}

void AccessibilityTable::clearChildren()
{
    AccessibilityRenderObject::clearChildren();
    m_rows.clear();
    m_columns.clear();
    m_cellSlots.clear();

    if (m_headerContainer) {
        m_headerContainer->detachFromParent();
        m_headerContainer = nullptr;
    }
}

void AccessibilityTable::addChildren()
{
    if (!isExposable()) {
        AccessibilityRenderObject::addChildren();
        return;
    }
    auto clearDirtySubtree = makeScopeExit([&] {
        m_subtreeDirty = false;
    });

    ASSERT(!m_childrenInitialized);
    m_childrenInitialized = true;
    WeakPtr cache = axObjectCache();
    if (!cache)
        return;

    // This function implements the "forming a table" algorithm for determining
    // the correct cell positions and spans (and storing those in m_cellSlots for later use).
    // https://html.spec.whatwg.org/multipage/tables.html#forming-a-table

    // Step 1.
    unsigned xWidth = 0;
    // Step 2.
    unsigned yHeight = 0;
    // Step 3: Let pending tfoot elements be a list of tfoot elements, initially empty.
    Vector<Ref<Element>> pendingTfootElements;
    // Step 10.
    unsigned yCurrent = 0;
    bool didAddCaption = false;

    struct DownwardGrowingCell {
        WeakRef<AccessibilityTableCell> axObject;
        // The column the cell starts in.
        unsigned x;
        // The number of columns the cell spans (called "width" in the spec).
        unsigned colSpan;
        unsigned remainingRowsToSpan;
    };
    Vector<DownwardGrowingCell> downwardGrowingCells;

    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-growing-downward-growing-cells
    auto growDownwardsCells = [&] () {
        // ...for growing downward-growing cells, the user agent must, for each {cell, cellX, width} tuple in the list
        // of downward-growing cells, extend the cell so that it also covers the slots with coordinates (x, yCurrent), where cellX ≤ x < cellX+width.
        for (auto& cell : downwardGrowingCells) {
            if (!cell.remainingRowsToSpan)
                continue;
            --cell.remainingRowsToSpan;
            cell.axObject->incrementEffectiveRowSpan();

            for (unsigned column = cell.x; column < cell.x + cell.colSpan; column++) {
                ensureRowAndColumn(yCurrent, column);
                m_cellSlots[yCurrent][column] = cell.axObject->objectID();
            }
        }
    };

    HashSet<AccessibilityObject*> processedRows;
    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-processing-rows
    auto processRow = [&] (AccessibilityTableRow* row) {
        if (!row || processedRows.contains(row))
            return;
        processedRows.add(row);

        if (row->roleValue() != AccessibilityRole::Unknown && row->accessibilityIsIgnored()) {
            // Skip ignored rows (except for those ignored because they have an unknown role, which will happen after a table has become un-exposed but is potentially becoming re-exposed).
            // This is an addition on top of the HTML algorithm because the computed AX table has extra restrictions (e.g. cannot contain aria-hidden or role="presentation" rows).
            return;
        }

        // Step 1: If yheight is equal to ycurrent, then increase yheight by 1. (ycurrent must never be greater than yheight.)
        if (yHeight <= yCurrent)
            yHeight = yCurrent + 1;

        // Step 2.
        unsigned xCurrent = 0;
        // Step 3: Run the algorithm for growing downward-growing cells.
        growDownwardsCells();

        // Step 4: If the tr element being processed has no td or th element children, then increase ycurrent by 1, abort this set of steps, and return to the algorithm above.
        for (const auto& child : row->children()) {
            RefPtr currentCell = dynamicDowncast<AccessibilityTableCell>(child.get());
            if (!currentCell)
                continue;
            // (Not specified): As part of beginning to process this cell, reset its effective rowspan in case it had a non-default value set from a previous call to AccessibilityTable::addChildren().
            currentCell->resetEffectiveRowSpan();

            // Step 6: While the slot with coordinate (xcurrent, ycurrent) already has a cell assigned to it, increase xcurrent by 1.
            ensureRowAndColumn(yCurrent, xCurrent);
            while (m_cellSlots[yCurrent][xCurrent].isValid()) {
                xCurrent += 1;
                ensureRowAndColumn(yCurrent, xCurrent);
            }
            // Step 7: If xcurrent is equal to xwidth, increase xwidth by 1. (xcurrent is never greater than xwidth.)
            if (xCurrent >= xWidth)
                xWidth = xCurrent + 1;
            // Step 8: If the current cell has a colspan attribute, then parse that attribute's value, and let colspan be the result.
            unsigned colSpan = currentCell->colSpan();
            // Step 9: If the current cell has a rowspan attribute, then parse that attribute's value, and let rowspan be the result.
            unsigned rowSpan = currentCell->rowSpan();

            // Step 10: If rowspan is zero and the table element's node document is not set to quirks mode, then let
            // cell grows downward be true, and set rowspan to 1. Otherwise, let cell grows downward be false.
            // NOTE: We intentionally don't implement this step because the rendering code doesn't, so implementing it
            // would cause AX to not match the visual state of the page.

            // Step 11: If xwidth < xcurrent+colspan, then let xwidth be xcurrent+colspan.
            if (xWidth < xCurrent + colSpan)
                xWidth = xCurrent + colSpan;

            // Step 12: If yheight < ycurrent+rowspan, then let yheight be ycurrent+rowspan.
            // NOTE: An explicit choice is made not to follow this part of the spec, because rowspan
            // can be some arbitrarily large number (up to 65535) that will not actually reflect how
            // many rows the cell spans in the final table. Taking it as-provided will cause incorrect
            // results in many scenarios. Instead, only check for yHeight < yCurrent.
            if (yHeight < yCurrent)
                yHeight = yCurrent;

            // Step 13: Let the slots with coordinates (x, y) such that xcurrent ≤ x < xcurrent+colspan and
            // ycurrent ≤ y < ycurrent+rowspan be covered by a new cell c, anchored at (xcurrent, ycurrent),
            // which has width colspan and height rowspan, corresponding to the current cell element.
            // NOTE: We don't implement this exactly, instead using the downward-growing cell algorithm to accurately
            // handle rowspan cells. This makes it easy to avoid extending cells outside their rowgroup.
            currentCell->setRowIndex(yCurrent);
            currentCell->setColumnIndex(xCurrent);
            for (unsigned x = xCurrent; x < xCurrent + colSpan; x++) {
                ensureRowAndColumn(yCurrent, x);
                m_cellSlots[yCurrent][x] = currentCell->objectID();
            }

            // Step 14: If cell grows downward is true, then add the tuple {c, xcurrent, colspan} to the
            // list of downward-growing cells.
            // NOTE: We use the downward-growing cell algorithm to expand rowspanned cells.
            if (rowSpan > 1) {
                downwardGrowingCells.append({
                    *currentCell,
                    xCurrent,
                    colSpan,
                    rowSpan - 1
                });
            }

            // Step 15.
            xCurrent += colSpan;

            // Step 16 handled below.
            // Step 17 and 18: Let current cell be the next td or th element child in the tr element being processed. (This is implemented by allowing the loop to continue above).
        }

        // Not specified: update some internal data structures.
        m_rows.append(row);
        row->setRowIndex(yCurrent);
        addChild(row);

        // Step 16: If current cell is the last td or th element child in the tr element being processed, then increase ycurrent by 1, abort this set of steps, and return to the algorithm above.
        yCurrent += 1;
    };
    auto needsToDescend = [&processedRows] (AXCoreObject& axObject) {
        return (!axObject.isTableRow() || !axObject.node())
            && !processedRows.contains(dynamicDowncast<AccessibilityObject>(axObject));
    };
    std::function<void(AXCoreObject*)> processRowDescendingIfNeeded = [&] (AXCoreObject* axObject) {
        if (!axObject)
            return;
        // Descend past anonymous renderers and non-rows.
        if (needsToDescend(*axObject)) {
            for (const auto& child : axObject->children())
                processRowDescendingIfNeeded(child.get());
        } else
            processRow(dynamicDowncast<AccessibilityTableRow>(axObject));
    };
    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-ending-a-row-group
    auto endRowGroup = [&] () {
        // 1. While yCurrent is less than yHeight, follow these steps:
        while (yCurrent < yHeight) {
            // 1a. Run the algorithm for growing downward-growing cells.
            growDownwardsCells();
            // 1b. Increase yCurrent by 1.
            ++yCurrent;
        }
        // 2. Empty the list of downward-growing cells.
        downwardGrowingCells.clear();
    };
    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-processing-row-groups
    auto processRowGroup = [&] (Element& sectionElement) {
        // Step 1: Let ystart have the value of yheight. Not implemented because it's only useful for step 3, which we skip.

        // Step 2: For each tr element that is a child of the element being processed,
        // in tree order, run the algorithm for processing rows.
        if (RefPtr tableSection = dynamicDowncast<HTMLTableSectionElement>(sectionElement)) {
            for (Ref row : childrenOfType<HTMLTableRowElement>(*tableSection)) {
                RefPtr tableRow = dynamicDowncast<AccessibilityTableRow>(cache->getOrCreate(row));
                processRow(tableRow.get());
            }
        } else if (RefPtr sectionAxObject = cache->getOrCreate(sectionElement)) {
            ASSERT_WITH_MESSAGE(nodeHasRole(&sectionElement, "rowgroup"_s), "processRowGroup should only be called with native table section elements, or role=rowgroup elements");
            for (const auto& child : sectionAxObject->children())
                processRowDescendingIfNeeded(child.get());
        }
        // Step 3: If yheight > ystart, then let all the last rows in the table from y=ystart to y=yheight-1
        // form a new row group, anchored at the slot with coordinate (0, ystart), with height yheight-ystart,
        // corresponding to the element being processed. Not implemented.

        // Step 4: Run the algorithm for ending a row group.
        endRowGroup();
    };

    // Step 4: Let the table be the table represented by the table element.
    RefPtr tableElement = this->node();
    // `isAriaTable()` will return true for table-like ARIA structures (grid, treegrid, table).
    if (!is<HTMLTableElement>(tableElement.get()) && !isAriaTable())
        return;

    bool withinImplicitRowGroup = false;
    std::function<void(Node*)> processTableDescendant = [&] (Node* node) {
        // Step 8: While the current element is not one of the following elements, advance the
        // current element to the next child of the table.
        if (auto* caption = dynamicDowncast<HTMLTableCaptionElement>(node)) {
            // Step 6: Associate the first caption element child of the table element with the table.
            if (!didAddCaption) {
                if (RefPtr axCaption = cache->getOrCreate(*caption)) {
                    addChild(axCaption.get(), DescendIfIgnored::No);
                    didAddCaption = true;
                }
            }
            return;
        }

        auto* element = dynamicDowncast<Element>(node);
        bool descendantIsRow = element && (element->hasTagName(trTag) || nodeHasRole(element, "row"_s));
        bool descendantIsRowGroup = element && !descendantIsRow && (element->hasTagName(theadTag) || element->hasTagName(tbodyTag) || element->hasTagName(tfootTag) || nodeHasRole(element, "rowgroup"_s));

        if (descendantIsRowGroup)
            withinImplicitRowGroup = false;
        else {
            // (Not specified): For ARIA tables, we need to track implicit rowgroups (allowed by the ARIA spec)
            // in order to properly perform the downward-growing cell algorithm.
            withinImplicitRowGroup = isAriaTable();
        }

        // Step 9: Handle the colgroup element. Not implemented.
        // Step 10: Handled above.
        // Step 11: Let the list of downward-growing cells be an empty list.
        if (!withinImplicitRowGroup)
            downwardGrowingCells.clear();
        // Step 12: While the current element is not one of the following elements, advance the current element to the next child of the table
        if (!descendantIsRow && !descendantIsRowGroup) {
            if (isAriaTable()) {
                // We are forgiving with ARIA grid markup, descending past disallowed elements to build the grid structure (this is not specified, but consistent with other browsers).
                if (RefPtr axObject = cache->getOrCreate(node); axObject && needsToDescend(*axObject)) {
                    for (const auto& child : axObject->children())
                        processTableDescendant(child ? child->node() : nullptr);
                }
            }
            return;
        }

        // Step 13: If the current element is a tr, then run the algorithm for processing rows,
        // advance the current element to the next child of the table, and return to the step labeled rows.
        if (descendantIsRow)
            processRow(dynamicDowncast<AccessibilityTableRow>(cache->getOrCreate(element)));

        // Step 14: Run the algorithm for ending a row group.
        if (!withinImplicitRowGroup)
            endRowGroup();

        // Step 15: If the current element is a tfoot...
        if (element->hasTagName(tfootTag)) {
            // ...then add that element to the list of pending tfoot elements
            pendingTfootElements.append(*element);
            // ...advance the current element to the next child of the table.
            return;
        }

        // Step 16: If the current element is either a thead or a tbody, run the algorithm for processing row groups. (Not specified: include role="rowgroups").
        if (descendantIsRowGroup)
            processRowGroup(*element);
    };
    // Step 7: Let the current element be the first element child of the table element.
    for (RefPtr currentElement = tableElement->firstChild(); currentElement; currentElement = currentElement->nextSibling()) {
        processTableDescendant(currentElement.get());
        // Step 17 + 18: Advance the current element to the next child of the table.
    }

    // Step 19: For each tfoot element in the list of pending tfoot elements, in tree order,
    // run the algorithm for processing row groups.
    for (const auto& tfootElement : pendingTfootElements)
        processRowGroup(tfootElement.get());

    // The remainder of this function is unspecified updating of internal data structures.
    for (unsigned i = 0; i < xWidth; ++i) {
        RefPtr column = downcast<AccessibilityTableColumn>(cache->create(AccessibilityRole::Column));
        column->setColumnIndex(i);
        column->setParent(this);
        m_columns.append(column.get());
        addChild(column.get(), DescendIfIgnored::No);
    }
    addChild(headerContainer(), DescendIfIgnored::No);

    m_subtreeDirty = false;
    // Sometimes the cell gets the wrong role initially because it is created before the parent
    // determines whether it is an accessibility table. Iterate all the cells and allow them to
    // update their roles now that the table knows its status.
    // see bug: https://bugs.webkit.org/show_bug.cgi?id=147001
    updateChildrenRoles();
}

AXCoreObject* AccessibilityTable::headerContainer()
{
    if (m_headerContainer)
        return m_headerContainer.get();

    auto* cache = axObjectCache();
    if (!cache)
        return nullptr;

    auto* tableHeader = downcast<AccessibilityMockObject>(cache->create(AccessibilityRole::TableHeaderContainer));
    tableHeader->setParent(this);

    m_headerContainer = tableHeader;
    return m_headerContainer.get();
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTable::columns()
{
    updateChildrenIfNecessary();

    return m_columns;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTable::rows()
{
    updateChildrenIfNecessary();
    
    return m_rows;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTable::columnHeaders()
{
    updateChildrenIfNecessary();

    AccessibilityChildrenVector headers;
    // Sometimes m_columns can be reset during the iteration, we cache it here to be safe.
    AccessibilityChildrenVector columnsCopy = m_columns;
    for (const auto& column : columnsCopy) {
        if (auto* header = column->columnHeader())
            headers.append(header);
    }

    return headers;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTable::rowHeaders()
{
    updateChildrenIfNecessary();

    AccessibilityChildrenVector headers;
    // Sometimes m_rows can be reset during the iteration, we cache it here to be safe.
    AccessibilityChildrenVector rowsCopy = m_rows;
    for (const auto& row : rowsCopy) {
        if (auto* header = downcast<AccessibilityTableRow>(*row).rowHeader())
            headers.append(header);
    }

    return headers;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTable::visibleRows()
{
    updateChildrenIfNecessary();

    AccessibilityChildrenVector rows;
    for (const auto& row : m_rows) {
        if (row && !row->isOffScreen())
            rows.append(row);
    }

    return rows;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTable::cells()
{
    updateChildrenIfNecessary();

    AccessibilityChildrenVector cells;
    for (const auto& row : m_rows)
        cells.appendVector(row->children());
    return cells;
}
    
unsigned AccessibilityTable::columnCount()
{
    updateChildrenIfNecessary();
    
    return m_columns.size();
}
    
unsigned AccessibilityTable::rowCount()
{
    updateChildrenIfNecessary();
    
    return m_rows.size();
}

AccessibilityObject* AccessibilityTable::cellForColumnAndRow(unsigned column, unsigned row)
{
    updateChildrenIfNecessary();
    if (row >= m_cellSlots.size() || column >= m_cellSlots[row].size())
        return nullptr;

    if (AXID cellID = m_cellSlots[row][column])
        return axObjectCache()->objectForID(cellID);
    return nullptr;
}

bool AccessibilityTable::hasGridAriaRole() const
{
    auto ariaRole = ariaRoleAttribute();
    // Notably, this excludes role="table".
    return ariaRole == AccessibilityRole::Grid || ariaRole == AccessibilityRole::TreeGrid;
}

AccessibilityRole AccessibilityTable::roleValue() const
{
    if (!isExposable())
        return AccessibilityRenderObject::roleValue();

    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole == AccessibilityRole::Grid || ariaRole == AccessibilityRole::TreeGrid)
        return ariaRole;

    return AccessibilityRole::Table;
}
    
bool AccessibilityTable::computeAccessibilityIsIgnored() const
{
    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;
    
    if (!isExposable())
        return AccessibilityRenderObject::computeAccessibilityIsIgnored();

    return false;
}

void AccessibilityTable::labelText(Vector<AccessibilityText>& textOrder) const
{
    String title = this->title();
    if (!title.isEmpty())
        textOrder.append(AccessibilityText(title, AccessibilityTextSource::LabelByElement));
}

String AccessibilityTable::title() const
{
    if (!isExposable())
        return AccessibilityRenderObject::title();
    
    String title;
    // Prefer the table caption if present.
    if (auto* tableElement = dynamicDowncast<HTMLTableElement>(node())) {
        if (RefPtr caption = tableElement->caption())
            title = caption->innerText();
    }
    
    // Fall back to standard title computation.
    if (title.isEmpty())
        title = AccessibilityRenderObject::title();
    return title;
}

int AccessibilityTable::axColumnCount() const
{
    int colCountInt = getIntegralAttribute(aria_colcountAttr);
    // The ARIA spec states, "Authors must set the value of aria-colcount to an integer equal to the
    // number of columns in the full table. If the total number of columns is unknown, authors must
    // set the value of aria-colcount to -1 to indicate that the value should not be calculated by
    // the user agent." If we have a valid value, make it available to platforms.
    if (colCountInt == -1 || colCountInt >= (int)m_columns.size())
        return colCountInt;
    
    return 0;
}

int AccessibilityTable::axRowCount() const
{
    int rowCountInt = getIntegralAttribute(aria_rowcountAttr);
    // The ARIA spec states, "Authors must set the value of aria-rowcount to an integer equal to the
    // number of rows in the full table. If the total number of rows is unknown, authors must set
    // the value of aria-rowcount to -1 to indicate that the value should not be calculated by the
    // user agent." If we have a valid value, make it available to platforms.
    if (rowCountInt == -1 || rowCountInt >= (int)m_rows.size())
        return rowCountInt;
    
    return 0;
}

void AccessibilityTable::ensureRow(unsigned index)
{
    if (m_cellSlots.size() < index + 1)
        m_cellSlots.grow(index + 1);
}

void AccessibilityTable::ensureRowAndColumn(unsigned rowIndex, unsigned columnIndex)
{
    ensureRow(rowIndex);
    if (m_cellSlots[rowIndex].size() < columnIndex + 1)
        m_cellSlots[rowIndex].grow(columnIndex + 1);
}

} // namespace WebCore
