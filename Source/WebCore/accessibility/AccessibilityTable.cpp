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
#include "ElementIterator.h"
#include "HTMLNames.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableSectionElement.h"
#include "RenderObject.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableSection.h"

#include <wtf/Deque.h>

namespace WebCore {

using namespace HTMLNames;

AccessibilityTable::AccessibilityTable(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
    , m_headerContainer(nullptr)
    , m_isExposableThroughAccessibility(true)
{
}

AccessibilityTable::~AccessibilityTable() = default;

void AccessibilityTable::init()
{
    AccessibilityRenderObject::init();
    m_isExposableThroughAccessibility = computeIsTableExposableThroughAccessibility();
}

Ref<AccessibilityTable> AccessibilityTable::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityTable(renderer));
}

bool AccessibilityTable::hasARIARole() const
{
    if (!m_renderer)
        return false;
    
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole != AccessibilityRole::Unknown)
        return true;

    return false;
}

bool AccessibilityTable::isExposableThroughAccessibility() const
{
    if (!m_renderer)
        return false;
    
    return m_isExposableThroughAccessibility;
}

HTMLTableElement* AccessibilityTable::tableElement() const
{
    if (!is<RenderTable>(*m_renderer))
        return nullptr;
    
    RenderTable& table = downcast<RenderTable>(*m_renderer);
    if (is<HTMLTableElement>(table.element()))
        return downcast<HTMLTableElement>(table.element());
    // Try to find the table element, when the AccessibilityTable is mapped to an anonymous table renderer.
    auto* firstChild = table.firstChild();
    if (!firstChild || !firstChild->node())
        return nullptr;
    if (is<HTMLTableElement>(*firstChild->node()))
        return downcast<HTMLTableElement>(firstChild->node());
    // FIXME: This might find an unrelated parent table element.
    return ancestorsOfType<HTMLTableElement>(*(firstChild->node())).first();
}
    
bool AccessibilityTable::isDataTable() const
{
    if (!m_renderer)
        return false;

    // Do not consider it a data table is it has an ARIA role.
    if (hasARIARole())
        return false;

    // When a section of the document is contentEditable, all tables should be
    // treated as data tables, otherwise users may not be able to work with rich
    // text editors that allow creating and editing tables.
    if (node() && node()->hasEditableStyle())
        return true;

    if (!is<RenderTable>(*m_renderer))
        return false;

    // This employs a heuristic to determine if this table should appear.
    // Only "data" tables should be exposed as tables.
    // Unfortunately, there is no good way to determine the difference
    // between a "layout" table and a "data" table.
    if (HTMLTableElement* tableElement = this->tableElement()) {
        // If there is a caption element, summary, THEAD, or TFOOT section, it's most certainly a data table.
        if (!tableElement->summary().isEmpty() || tableElement->tHead() || tableElement->tFoot() || tableElement->caption())
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
    int axColumnCount = getAttribute(aria_colcountAttr).toInt();
    if (axColumnCount == -1 || axColumnCount > 0)
        return true;

    int axRowCount = getAttribute(aria_rowcountAttr).toInt();
    if (axRowCount == -1 || axRowCount > 0)
        return true;

    RenderTable& table = downcast<RenderTable>(*m_renderer);
    // go through the cell's and check for tell-tale signs of "data" table status
    // cells have borders, or use attributes like headers, abbr, scope or axis
    table.recalcSectionsIfNeeded();
    RenderTableSection* firstBody = table.firstBody();
    if (!firstBody)
        return false;
    
    int numCols = firstBody->numColumns();
    int numRows = firstBody->numRows();
    
    // If there are at least 20 rows, we'll call it a data table.
    if (numRows >= 20)
        return true;
    
    // Store the background color of the table to check against cell's background colors.
    const RenderStyle& tableStyle = table.style();
    Color tableBGColor = tableStyle.visitedDependentColor(CSSPropertyBackgroundColor);
    
    // check enough of the cells to find if the table matches our criteria
    // Criteria: 
    //   1) must have at least one valid cell (and)
    //   2) at least half of cells have borders (or)
    //   3) at least half of cells have different bg colors than the table, and there is cell spacing (or)
    //   4) the valid cell has an ARIA cell-related property
    unsigned validCellCount = 0;
    unsigned borderedCellCount = 0;
    unsigned backgroundDifferenceCellCount = 0;
    unsigned cellsWithTopBorder = 0;
    unsigned cellsWithBottomBorder = 0;
    unsigned cellsWithLeftBorder = 0;
    unsigned cellsWithRightBorder = 0;
    
    Color alternatingRowColors[5];
    int alternatingRowColorCount = 0;
    
    int headersInFirstColumnCount = 0;
    for (int row = 0; row < numRows; ++row) {
    
        int headersInFirstRowCount = 0;
        for (int col = 0; col < numCols; ++col) {    
            RenderTableCell* cell = firstBody->primaryCellAt(row, col);
            if (!cell)
                continue;

            Element* cellElement = cell->element();
            if (!cellElement)
                continue;
            
            if (cell->width() < 1 || cell->height() < 1)
                continue;
            
            ++validCellCount;
            
            bool isTHCell = cellElement->hasTagName(thTag);
            // If the first row is comprised of all <th> tags, assume it is a data table.
            if (!row && isTHCell)
                ++headersInFirstRowCount;

            // If the first column is comprised of all <th> tags, assume it is a data table.
            if (!col && isTHCell)
                ++headersInFirstColumnCount;
            
            // In this case, the developer explicitly assigned a "data" table attribute.
            if (is<HTMLTableCellElement>(*cellElement)) {
                HTMLTableCellElement& tableCellElement = downcast<HTMLTableCellElement>(*cellElement);
                if (!tableCellElement.headers().isEmpty() || !tableCellElement.abbr().isEmpty()
                    || !tableCellElement.axis().isEmpty() || !tableCellElement.scope().isEmpty())
                    return true;
            }

            // If the author has used ARIA to specify a valid column or row index, assume they want us
            // to treat the table as a data table.
            int axColumnIndex =  cellElement->attributeWithoutSynchronization(aria_colindexAttr).toInt();
            if (axColumnIndex >= 1)
                return true;

            int axRowIndex = cellElement->attributeWithoutSynchronization(aria_rowindexAttr).toInt();
            if (axRowIndex >= 1)
                return true;

            if (auto cellParentElement = cellElement->parentElement()) {
                axRowIndex = cellParentElement->attributeWithoutSynchronization(aria_rowindexAttr).toInt();
                if (axRowIndex >= 1)
                    return true;
            }

            // If the author has used ARIA to specify a column or row span, we're supposed to ignore
            // the value for the purposes of exposing the span. But assume they want us to treat the
            // table as a data table.
            int axColumnSpan = cellElement->attributeWithoutSynchronization(aria_colspanAttr).toInt();
            if (axColumnSpan >= 1)
                return true;

            int axRowSpan = cellElement->attributeWithoutSynchronization(aria_rowspanAttr).toInt();
            if (axRowSpan >= 1)
                return true;

            const RenderStyle& renderStyle = cell->style();

            // If the empty-cells style is set, we'll call it a data table.
            if (renderStyle.emptyCells() == EmptyCell::Hide)
                return true;

            // If a cell has matching bordered sides, call it a (fully) bordered cell.
            if ((cell->borderTop() > 0 && cell->borderBottom() > 0)
                || (cell->borderLeft() > 0 && cell->borderRight() > 0))
                ++borderedCellCount;

            // Also keep track of each individual border, so we can catch tables where most
            // cells have a bottom border, for example.
            if (cell->borderTop() > 0)
                ++cellsWithTopBorder;
            if (cell->borderBottom() > 0)
                ++cellsWithBottomBorder;
            if (cell->borderLeft() > 0)
                ++cellsWithLeftBorder;
            if (cell->borderRight() > 0)
                ++cellsWithRightBorder;
            
            // If the cell has a different color from the table and there is cell spacing,
            // then it is probably a data table cell (spacing and colors take the place of borders).
            Color cellColor = renderStyle.visitedDependentColor(CSSPropertyBackgroundColor);
            if (table.hBorderSpacing() > 0 && table.vBorderSpacing() > 0
                && tableBGColor != cellColor && cellColor.alpha() != 1)
                ++backgroundDifferenceCellCount;
            
            // If we've found 10 "good" cells, we don't need to keep searching.
            if (borderedCellCount >= 10 || backgroundDifferenceCellCount >= 10)
                return true;
            
            // For the first 5 rows, cache the background color so we can check if this table has zebra-striped rows.
            if (row < 5 && row == alternatingRowColorCount) {
                RenderElement* renderRow = cell->parent();
                if (!is<RenderTableRow>(renderRow))
                    continue;
                const RenderStyle& rowRenderStyle = renderRow->style();
                Color rowColor = rowRenderStyle.visitedDependentColor(CSSPropertyBackgroundColor);
                alternatingRowColors[alternatingRowColorCount] = rowColor;
                ++alternatingRowColorCount;
            }
        }
        
        if (!row && headersInFirstRowCount == numCols && numCols > 1)
            return true;
    }

    if (headersInFirstColumnCount == numRows && numRows > 1)
        return true;
    
    // if there is less than two valid cells, it's not a data table
    if (validCellCount <= 1)
        return false;
    
    // half of the cells had borders, it's a data table
    unsigned neededCellCount = validCellCount / 2;
    if (borderedCellCount >= neededCellCount
        || cellsWithTopBorder >= neededCellCount
        || cellsWithBottomBorder >= neededCellCount
        || cellsWithLeftBorder >= neededCellCount
        || cellsWithRightBorder >= neededCellCount)
        return true;
    
    // half had different background colors, it's a data table
    if (backgroundDifferenceCellCount >= neededCellCount)
        return true;

    // Check if there is an alternating row background color indicating a zebra striped style pattern.
    if (alternatingRowColorCount > 2) {
        Color firstColor = alternatingRowColors[0];
        for (int k = 1; k < alternatingRowColorCount; k++) {
            // If an odd row was the same color as the first row, its not alternating.
            if (k % 2 == 1 && alternatingRowColors[k] == firstColor)
                return false;
            // If an even row is not the same as the first row, its not alternating.
            if (!(k % 2) && alternatingRowColors[k] != firstColor)
                return false;
        }
        return true;
    }
    
    return false;
}
    
bool AccessibilityTable::computeIsTableExposableThroughAccessibility() const
{
    // The following is a heuristic used to determine if a
    // <table> should be exposed as an AXTable. The goal
    // is to only show "data" tables.

    if (!m_renderer)
        return false;

    // If the developer assigned an aria role to this, then we
    // shouldn't expose it as a table, unless, of course, the aria
    // role is a table.
    if (hasARIARole())
        return false;

    return isDataTable();
}

void AccessibilityTable::clearChildren()
{
    AccessibilityRenderObject::clearChildren();
    m_rows.clear();
    m_columns.clear();

    if (m_headerContainer) {
        m_headerContainer->detachFromParent();
        m_headerContainer = nullptr;
    }
}

void AccessibilityTable::addChildren()
{
    if (!isExposableThroughAccessibility()) {
        AccessibilityRenderObject::addChildren();
        return;
    }
    
    ASSERT(!m_haveChildren); 
    
    m_haveChildren = true;
    if (!is<RenderTable>(renderer()))
        return;
    
    RenderTable& table = downcast<RenderTable>(*m_renderer);
    // Go through all the available sections to pull out the rows and add them as children.
    table.recalcSectionsIfNeeded();
    
    if (HTMLTableElement* tableElement = this->tableElement()) {
        if (auto caption = tableElement->caption()) {
            AccessibilityObject* axCaption = axObjectCache()->getOrCreate(caption.get());
            if (axCaption && !axCaption->accessibilityIsIgnored())
                m_children.append(axCaption);
        }
    }

    unsigned maxColumnCount = 0;
    RenderTableSection* footer = table.footer();
    
    for (RenderTableSection* tableSection = table.topSection(); tableSection; tableSection = table.sectionBelow(tableSection, SkipEmptySections)) {
        if (tableSection == footer)
            continue;
        addChildrenFromSection(tableSection, maxColumnCount);
    }
    
    // Process the footer last, in case it was ordered earlier in the DOM.
    if (footer)
        addChildrenFromSection(footer, maxColumnCount);
    
    AXObjectCache* axCache = m_renderer->document().axObjectCache();
    // make the columns based on the number of columns in the first body
    unsigned length = maxColumnCount;
    for (unsigned i = 0; i < length; ++i) {
        auto& column = downcast<AccessibilityTableColumn>(*axCache->getOrCreate(AccessibilityRole::Column));
        column.setColumnIndex((int)i);
        column.setParent(this);
        m_columns.append(&column);
        if (!column.accessibilityIsIgnored())
            m_children.append(&column);
    }
    
    AccessibilityObject* headerContainerObject = headerContainer();
    if (headerContainerObject && !headerContainerObject->accessibilityIsIgnored())
        m_children.append(headerContainerObject);

    // Sometimes the cell gets the wrong role initially because it is created before the parent
    // determines whether it is an accessibility table. Iterate all the cells and allow them to
    // update their roles now that the table knows its status.
    // see bug: https://bugs.webkit.org/show_bug.cgi?id=147001
    for (const auto& row : m_rows) {
        for (const auto& cell : row->children())
            cell->updateAccessibilityRole();
    }

}

void AccessibilityTable::addTableCellChild(AccessibilityObject* rowObject, HashSet<AccessibilityObject*>& appendedRows, unsigned& columnCount)
{
    if (!rowObject || !is<AccessibilityTableRow>(*rowObject))
        return;

    auto& row = downcast<AccessibilityTableRow>(*rowObject);
    // We need to check every cell for a new row, because cell spans
    // can cause us to miss rows if we just check the first column.
    if (appendedRows.contains(&row))
        return;
    
    row.setRowIndex(static_cast<int>(m_rows.size()));
    m_rows.append(&row);
    if (!row.accessibilityIsIgnored())
        m_children.append(&row);
    appendedRows.add(&row);
        
    // store the maximum number of columns
    unsigned rowCellCount = row.children().size();
    if (rowCellCount > columnCount)
        columnCount = rowCellCount;
}

void AccessibilityTable::addChildrenFromSection(RenderTableSection* tableSection, unsigned& maxColumnCount)
{
    ASSERT(tableSection);
    if (!tableSection)
        return;
    
    AXObjectCache* axCache = m_renderer->document().axObjectCache();
    HashSet<AccessibilityObject*> appendedRows;
    unsigned numRows = tableSection->numRows();
    for (unsigned rowIndex = 0; rowIndex < numRows; ++rowIndex) {
        
        RenderTableRow* renderRow = tableSection->rowRendererAt(rowIndex);
        if (!renderRow)
            continue;
        
        AccessibilityObject& rowObject = *axCache->getOrCreate(renderRow);
        
        // If the row is anonymous, we should dive deeper into the descendants to try to find a valid row.
        if (renderRow->isAnonymous()) {
            Deque<AccessibilityObject*> queue;
            queue.append(&rowObject);
            
            while (!queue.isEmpty()) {
                AccessibilityObject* obj = queue.takeFirst();
                if (obj->node() && is<AccessibilityTableRow>(*obj)) {
                    addTableCellChild(obj, appendedRows, maxColumnCount);
                    continue;
                }
                for (auto* child = obj->firstChild(); child; child = child->nextSibling())
                    queue.append(child);
            }
        } else
            addTableCellChild(&rowObject, appendedRows, maxColumnCount);
    }
    
    maxColumnCount = std::max(tableSection->numColumns(), maxColumnCount);
}
    
AccessibilityObject* AccessibilityTable::headerContainer()
{
    if (m_headerContainer)
        return m_headerContainer.get();
    
    auto& tableHeader = downcast<AccessibilityMockObject>(*axObjectCache()->getOrCreate(AccessibilityRole::TableHeaderContainer));
    tableHeader.setParent(this);

    m_headerContainer = &tableHeader;
    return m_headerContainer.get();
}

const AccessibilityObject::AccessibilityChildrenVector& AccessibilityTable::columns()
{
    updateChildrenIfNecessary();
        
    return m_columns;
}

const AccessibilityObject::AccessibilityChildrenVector& AccessibilityTable::rows()
{
    updateChildrenIfNecessary();
    
    return m_rows;
}

void AccessibilityTable::columnHeaders(AccessibilityChildrenVector& headers)
{
    if (!m_renderer)
        return;
    
    updateChildrenIfNecessary();
    
    // Sometimes m_columns can be reset during the iteration, we cache it here to be safe.
    AccessibilityChildrenVector columnsCopy = m_columns;
    
    for (const auto& column : columnsCopy) {
        if (AccessibilityObject* header = downcast<AccessibilityTableColumn>(*column).headerObject())
            headers.append(header);
    }
}

void AccessibilityTable::rowHeaders(AccessibilityChildrenVector& headers)
{
    if (!m_renderer)
        return;
    
    updateChildrenIfNecessary();
    
    // Sometimes m_rows can be reset during the iteration, we cache it here to be safe.
    AccessibilityChildrenVector rowsCopy = m_rows;
    
    for (const auto& row : rowsCopy) {
        if (AccessibilityObject* header = downcast<AccessibilityTableRow>(*row).headerObject())
            headers.append(header);
    }
}

void AccessibilityTable::visibleRows(AccessibilityChildrenVector& rows)
{
    if (!m_renderer)
        return;
    
    updateChildrenIfNecessary();
    
    for (const auto& row : m_rows) {
        if (row && !row->isOffScreen())
            rows.append(row);
    }
}

void AccessibilityTable::cells(AccessibilityObject::AccessibilityChildrenVector& cells)
{
    if (!m_renderer)
        return;
    
    updateChildrenIfNecessary();
    
    for (const auto& row : m_rows)
        cells.appendVector(row->children());
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

int AccessibilityTable::tableLevel() const
{
    int level = 0;
    for (AccessibilityObject* obj = static_cast<AccessibilityObject*>(const_cast<AccessibilityTable*>(this)); obj; obj = obj->parentObject()) {
        if (is<AccessibilityTable>(*obj) && downcast<AccessibilityTable>(*obj).isExposableThroughAccessibility())
            ++level;
    }
    
    return level;
}

AccessibilityTableCell* AccessibilityTable::cellForColumnAndRow(unsigned column, unsigned row)
{
    updateChildrenIfNecessary();
    if (column >= columnCount() || row >= rowCount())
        return nullptr;
    
    // Iterate backwards through the rows in case the desired cell has a rowspan and exists in a previous row.
    for (unsigned rowIndexCounter = row + 1; rowIndexCounter > 0; --rowIndexCounter) {
        unsigned rowIndex = rowIndexCounter - 1;
        const auto& children = m_rows[rowIndex]->children();
        // Since some cells may have colspans, we have to check the actual range of each
        // cell to determine which is the right one.
        for (unsigned colIndexCounter = std::min(static_cast<unsigned>(children.size()), column + 1); colIndexCounter > 0; --colIndexCounter) {
            unsigned colIndex = colIndexCounter - 1;
            AccessibilityObject* child = children[colIndex].get();
            ASSERT(is<AccessibilityTableCell>(*child));
            if (!is<AccessibilityTableCell>(*child))
                continue;
            
            std::pair<unsigned, unsigned> columnRange;
            std::pair<unsigned, unsigned> rowRange;
            auto& tableCellChild = downcast<AccessibilityTableCell>(*child);
            tableCellChild.columnIndexRange(columnRange);
            tableCellChild.rowIndexRange(rowRange);
            
            if ((column >= columnRange.first && column < (columnRange.first + columnRange.second))
                && (row >= rowRange.first && row < (rowRange.first + rowRange.second)))
                return &tableCellChild;
        }
    }
    
    return nullptr;
}

AccessibilityRole AccessibilityTable::roleValue() const
{
    if (!isExposableThroughAccessibility())
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
    
    if (!isExposableThroughAccessibility())
        return AccessibilityRenderObject::computeAccessibilityIsIgnored();
        
    return false;
}

void AccessibilityTable::titleElementText(Vector<AccessibilityText>& textOrder) const
{
    String title = this->title();
    if (!title.isEmpty())
        textOrder.append(AccessibilityText(title, AccessibilityTextSource::LabelByElement));
}

String AccessibilityTable::title() const
{
    if (!isExposableThroughAccessibility())
        return AccessibilityRenderObject::title();
    
    String title;
    if (!m_renderer)
        return title;
    
    // see if there is a caption
    Node* tableElement = m_renderer->node();
    if (is<HTMLTableElement>(tableElement)) {
        if (auto caption = downcast<HTMLTableElement>(*tableElement).caption())
            title = caption->innerText();
    }
    
    // try the standard 
    if (title.isEmpty())
        title = AccessibilityRenderObject::title();
    
    return title;
}

int AccessibilityTable::axColumnCount() const
{
    const AtomString& colCountValue = getAttribute(aria_colcountAttr);
    int colCountInt = colCountValue.toInt();
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
    const AtomString& rowCountValue = getAttribute(aria_rowcountAttr);
    int rowCountInt = rowCountValue.toInt();
    // The ARIA spec states, "Authors must set the value of aria-rowcount to an integer equal to the
    // number of rows in the full table. If the total number of rows is unknown, authors must set
    // the value of aria-rowcount to -1 to indicate that the value should not be calculated by the
    // user agent." If we have a valid value, make it available to platforms.
    if (rowCountInt == -1 || rowCountInt >= (int)m_rows.size())
        return rowCountInt;
    
    return 0;
}

} // namespace WebCore
