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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "HTMLNames.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "RenderObject.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableSection.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

AccessibilityTable::AccessibilityTable(RenderObject* renderer)
    : AccessibilityRenderObject(renderer),
    m_headerContainer(0)
{
#if ACCESSIBILITY_TABLES
    m_isAccessibilityTable = isTableExposableThroughAccessibility();
#else
    m_isAccessibilityTable = false;
#endif
}

AccessibilityTable::~AccessibilityTable()
{
}

PassRefPtr<AccessibilityTable> AccessibilityTable::create(RenderObject* renderer)
{
    return adoptRef(new AccessibilityTable(renderer));
}

bool AccessibilityTable::isTableExposableThroughAccessibility()
{
    // the following is a heuristic used to determine if a
    // <table> should be exposed as an AXTable. The goal
    // is to only show "data" tables
    
    if (!m_renderer || !m_renderer->isTable())
        return false;
    
    // if the developer assigned an aria role to this, then we shouldn't 
    // expose it as a table, unless, of course, the aria role is a table
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole != UnknownRole)
        return false;
    
    RenderTable* table = toRenderTable(m_renderer);
    
    // this employs a heuristic to determine if this table should appear. 
    // Only "data" tables should be exposed as tables. 
    // Unfortunately, there is no good way to determine the difference
    // between a "layout" table and a "data" table
    
    Node* tableNode = table->node();
    if (!tableNode || !tableNode->hasTagName(tableTag))
        return false;
    
    // if there is a caption element, summary, THEAD, or TFOOT section, it's most certainly a data table
    HTMLTableElement* tableElement = static_cast<HTMLTableElement*>(tableNode);
    if (!tableElement->summary().isEmpty() || tableElement->tHead() || tableElement->tFoot() || tableElement->caption())
        return true;
    
    // if someone used "rules" attribute than the table should appear
    if (!tableElement->rules().isEmpty())
        return true;    
    
    // go through the cell's and check for tell-tale signs of "data" table status
    // cells have borders, or use attributes like headers, abbr, scope or axis
    RenderTableSection* firstBody = table->firstBody();
    if (!firstBody)
        return false;
    
    int numCols = firstBody->numColumns();
    int numRows = firstBody->numRows();
    
    // if there's only one cell, it's not a good AXTable candidate
    if (numRows == 1 && numCols == 1)
        return false;
    
    // store the background color of the table to check against cell's background colors
    RenderStyle* tableStyle = table->style();
    if (!tableStyle)
        return false;
    Color tableBGColor = tableStyle->backgroundColor();
    
    // check enough of the cells to find if the table matches our criteria
    // Criteria: 
    //   1) must have at least one valid cell (and)
    //   2) at least half of cells have borders (or)
    //   3) at least half of cells have different bg colors than the table, and there is cell spacing
    unsigned validCellCount = 0;
    unsigned borderedCellCount = 0;
    unsigned backgroundDifferenceCellCount = 0;
    
    for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < numCols; ++col) {    
            RenderTableCell* cell = firstBody->cellAt(row, col).cell;
            if (!cell)
                continue;
            Node* cellNode = cell->node();
            if (!cellNode)
                continue;
            
            if (cell->width() < 1 || cell->height() < 1)
                continue;
            
            validCellCount++;
            
            HTMLTableCellElement* cellElement = static_cast<HTMLTableCellElement*>(cellNode);
            
            // in this case, the developer explicitly assigned a "data" table attribute
            if (!cellElement->headers().isEmpty() || !cellElement->abbr().isEmpty()
                || !cellElement->axis().isEmpty() || !cellElement->scope().isEmpty())
                return true;
            
            RenderStyle* renderStyle = cell->style();
            if (!renderStyle)
                continue;

            // a cell needs to have matching bordered sides, before it can be considered a bordered cell.
            if ((cell->borderTop() > 0 && cell->borderBottom() > 0)
                || (cell->borderLeft() > 0 && cell->borderRight() > 0))
                borderedCellCount++;
            
            // if the cell has a different color from the table and there is cell spacing,
            // then it is probably a data table cell (spacing and colors take the place of borders)
            Color cellColor = renderStyle->backgroundColor();
            if (table->hBorderSpacing() > 0 && table->vBorderSpacing() > 0
                && tableBGColor != cellColor && cellColor.alpha() != 1)
                backgroundDifferenceCellCount++;
            
            // if we've found 10 "good" cells, we don't need to keep searching
            if (borderedCellCount >= 10 || backgroundDifferenceCellCount >= 10)
                return true;
        }
    }

    // if there is less than two valid cells, it's not a data table
    if (validCellCount <= 1)
        return false;
    
    // half of the cells had borders, it's a data table
    unsigned neededCellCount = validCellCount / 2;
    if (borderedCellCount >= neededCellCount)
        return true;
    
    // half had different background colors, it's a data table
    if (backgroundDifferenceCellCount >= neededCellCount)
        return true;

    return false;
}
    
void AccessibilityTable::clearChildren()
{
    m_children.clear();
    m_rows.clear();
    m_columns.clear();
    m_haveChildren = false;
}

void AccessibilityTable::addChildren()
{
    if (!isDataTable()) {
        AccessibilityRenderObject::addChildren();
        return;
    }
    
    ASSERT(!m_haveChildren); 
    
    m_haveChildren = true;
    if (!m_renderer || !m_renderer->isTable())
        return;
    
    RenderTable* table = toRenderTable(m_renderer);
    AXObjectCache* axCache = m_renderer->document()->axObjectCache();

    // go through all the available sections to pull out the rows
    // and add them as children
    RenderTableSection* tableSection = table->header();
    if (!tableSection)
        tableSection = table->firstBody();
    
    if (!tableSection)
        return;
    
    RenderTableSection* initialTableSection = tableSection;
    
    while (tableSection) {
        
        HashSet<AccessibilityObject*> appendedRows;

        unsigned numRows = tableSection->numRows();
        unsigned numCols = tableSection->numColumns();
        for (unsigned rowIndex = 0; rowIndex < numRows; ++rowIndex) {
            for (unsigned colIndex = 0; colIndex < numCols; ++colIndex) {
                
                RenderTableCell* cell = tableSection->cellAt(rowIndex, colIndex).cell;
                if (!cell)
                    continue;
                
                AccessibilityObject* rowObject = axCache->getOrCreate(cell->parent());
                if (!rowObject->isTableRow())
                    continue;
                
                AccessibilityTableRow* row = static_cast<AccessibilityTableRow*>(rowObject);
                // we need to check every cell for a new row, because cell spans
                // can cause us to mess rows if we just check the first column
                if (appendedRows.contains(row))
                    continue;
                
                row->setRowIndex((int)m_rows.size());        
                m_rows.append(row);
                m_children.append(row);
                appendedRows.add(row);
            }
        }
        
        tableSection = table->sectionBelow(tableSection, true);
    }
    
    // make the columns based on the number of columns in the first body
    unsigned length = initialTableSection->numColumns();
    for (unsigned i = 0; i < length; ++i) {
        AccessibilityTableColumn* column = static_cast<AccessibilityTableColumn*>(axCache->getOrCreate(ColumnRole));
        column->setColumnIndex((int)i);
        column->setParentTable(this);
        m_columns.append(column);
        m_children.append(column);
    }
    
    AccessibilityObject* headerContainerObject = headerContainer();
    if (headerContainerObject)
        m_children.append(headerContainerObject);
}
    
AccessibilityObject* AccessibilityTable::headerContainer()
{
    if (m_headerContainer)
        return m_headerContainer;
    
    m_headerContainer = static_cast<AccessibilityTableHeaderContainer*>(axObjectCache()->getOrCreate(TableHeaderContainerRole));
    m_headerContainer->setParentTable(this);
    
    return m_headerContainer;
}

AccessibilityObject::AccessibilityChildrenVector& AccessibilityTable::columns()
{
    if (!hasChildren())
        addChildren();
        
    return m_columns;
}

AccessibilityObject::AccessibilityChildrenVector& AccessibilityTable::rows()
{
    if (!hasChildren())
        addChildren();

    return m_rows;
}
    
void AccessibilityTable::rowHeaders(AccessibilityChildrenVector& headers)
{
    if (!m_renderer)
        return;
    
    if (!hasChildren())
        addChildren();
    
    unsigned rowCount = m_rows.size();
    for (unsigned k = 0; k < rowCount; ++k) {
        AccessibilityObject* header = static_cast<AccessibilityTableRow*>(m_rows[k].get())->headerObject();
        if (!header)
            continue;
        headers.append(header);
    }
}

void AccessibilityTable::columnHeaders(AccessibilityChildrenVector& headers)
{
    if (!m_renderer)
        return;
    
    if (!hasChildren())
        addChildren();
    
    unsigned colCount = m_columns.size();
    for (unsigned k = 0; k < colCount; ++k) {
        AccessibilityObject* header = static_cast<AccessibilityTableColumn*>(m_columns[k].get())->headerObject();
        if (!header)
            continue;
        headers.append(header);
    }
}
    
void AccessibilityTable::cells(AccessibilityObject::AccessibilityChildrenVector& cells)
{
    if (!m_renderer)
        return;
    
    if (!hasChildren())
        addChildren();
    
    int numRows = m_rows.size();
    for (int row = 0; row < numRows; ++row) {
        AccessibilityChildrenVector rowChildren = m_rows[row]->children();
        cells.append(rowChildren);
    }
}
    
unsigned AccessibilityTable::columnCount()
{
    if (!hasChildren())
        addChildren();
    
    return m_columns.size();    
}
    
unsigned AccessibilityTable::rowCount()
{
    if (!hasChildren())
        addChildren();
    
    return m_rows.size();
}
    
AccessibilityTableCell* AccessibilityTable::cellForColumnAndRow(unsigned column, unsigned row)
{
    if (!m_renderer || !m_renderer->isTable())
        return 0;
    
    if (!hasChildren())
        addChildren();
    
    RenderTable* table = toRenderTable(m_renderer);
    RenderTableSection* tableSection = table->header();
    if (!tableSection)
        tableSection = table->firstBody();
    
    RenderTableCell* cell = 0;
    unsigned rowCount = 0;
    unsigned rowOffset = 0;
    while (tableSection) {
        
        unsigned numRows = tableSection->numRows();
        unsigned numCols = tableSection->numColumns();
        
        rowCount += numRows;
        
        unsigned sectionSpecificRow = row - rowOffset;            
        if (row < rowCount && column < numCols && sectionSpecificRow < numRows) {
            cell = tableSection->cellAt(sectionSpecificRow, column).cell;
            
            // we didn't find the cell, which means there's spanning happening
            // search backwards to find the spanning cell
            if (!cell) {
                
                // first try rows
                for (int testRow = sectionSpecificRow-1; testRow >= 0; --testRow) {
                    cell = tableSection->cellAt(testRow, column).cell;
                    // cell overlapped. use this one
                    if (cell && ((cell->row() + (cell->rowSpan()-1)) >= (int)sectionSpecificRow))
                        break;
                    cell = 0;
                }
                
                if (!cell) {
                    // try cols
                    for (int testCol = column-1; testCol >= 0; --testCol) {
                        cell = tableSection->cellAt(sectionSpecificRow, testCol).cell;
                        // cell overlapped. use this one
                        if (cell && ((cell->col() + (cell->colSpan()-1)) >= (int)column))
                            break;
                        cell = 0;
                    }
                }
            }
        }
        
        if (cell)
            break;
        
        rowOffset += numRows;
        // we didn't find anything between the rows we should have
        if (row < rowCount)
            break;
        tableSection = table->sectionBelow(tableSection, true);        
    }
    
    if (!cell)
        return 0;
    
    AccessibilityObject* cellObject = axObjectCache()->getOrCreate(cell);
    ASSERT(cellObject->isTableCell());
    
    return static_cast<AccessibilityTableCell*>(cellObject);
}

AccessibilityRole AccessibilityTable::roleValue() const
{
    if (!isDataTable())
        return AccessibilityRenderObject::roleValue();

    return TableRole;
}
    
bool AccessibilityTable::accessibilityIsIgnored() const
{
    if (!isDataTable())
        return AccessibilityRenderObject::accessibilityIsIgnored();
    
    return false;
}
    
String AccessibilityTable::title() const
{
    if (!isDataTable())
        return AccessibilityRenderObject::title();
    
    String title;
    if (!m_renderer)
        return title;
    
    // see if there is a caption
    Node* tableElement = m_renderer->node();
    if (tableElement && tableElement->hasTagName(tableTag)) {
        HTMLTableCaptionElement* caption = static_cast<HTMLTableElement*>(tableElement)->caption();
        if (caption)
            title = caption->innerText();
    }
    
    // try the standard 
    if (title.isEmpty())
        title = AccessibilityRenderObject::title();
    
    return title;
}

bool AccessibilityTable::isDataTable() const
{
    if (!m_renderer)
        return false;
    
    return m_isAccessibilityTable;
}

} // namespace WebCore
