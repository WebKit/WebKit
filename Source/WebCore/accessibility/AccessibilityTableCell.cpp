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
#include "AccessibilityTableCell.h"

#include "AXObjectCache.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableRow.h"
#include "ElementIterator.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "RenderObject.h"
#include "RenderTableCell.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityTableCell::AccessibilityTableCell(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
    , m_axColIndexFromRow(-1)
{
}

AccessibilityTableCell::~AccessibilityTableCell() = default;

Ref<AccessibilityTableCell> AccessibilityTableCell::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityTableCell(renderer));
}

bool AccessibilityTableCell::computeAccessibilityIsIgnored() const
{
    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;
    
    // Ignore anonymous table cells as long as they're not in a table (ie. when display:table is used).
    RenderObject* renderTable = is<RenderTableCell>(renderer()) ? downcast<RenderTableCell>(*m_renderer).table() : nullptr;
    bool inTable = renderTable && renderTable->node() && (renderTable->node()->hasTagName(tableTag) || nodeHasRole(renderTable->node(), "grid"));
    if (!node() && !inTable)
        return true;
        
    if (!isTableCell())
        return AccessibilityRenderObject::computeAccessibilityIsIgnored();
    
    return false;
}

AccessibilityTable* AccessibilityTableCell::parentTable() const
{
    if (!is<RenderTableCell>(renderer()))
        return nullptr;

    // If the document no longer exists, we might not have an axObjectCache.
    if (!axObjectCache())
        return nullptr;
    
    // Do not use getOrCreate. parentTable() can be called while the render tree is being modified 
    // by javascript, and creating a table element may try to access the render tree while in a bad state.
    // By using only get() implies that the AXTable must be created before AXTableCells. This should
    // always be the case when AT clients access a table.
    // https://bugs.webkit.org/show_bug.cgi?id=42652
    AccessibilityObject* parentTable = axObjectCache()->get(downcast<RenderTableCell>(*m_renderer).table());
    if (!is<AccessibilityTable>(parentTable))
        return nullptr;
    
    // The RenderTableCell's table() object might be anonymous sometimes. We should handle it gracefully
    // by finding the right table.
    if (!parentTable->node()) {
        for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
            // If this is a non-anonymous table object, but not an accessibility table, we should stop because
            // we don't want to choose another ancestor table as this cell's table.
            if (is<AccessibilityTable>(*parent)) {
                auto& parentTable = downcast<AccessibilityTable>(*parent);
                if (parentTable.isExposableThroughAccessibility())
                    return &parentTable;
                if (parentTable.node())
                    break;
            }
        }
        return nullptr;
    }
    
    return downcast<AccessibilityTable>(parentTable);
}
    
bool AccessibilityTableCell::isTableCell() const
{
    // If the parent table is an accessibility table, then we are a table cell.
    // This used to check if the unignoredParent was a row, but that exploded performance if
    // this was in nested tables. This check should be just as good.
    AccessibilityObject* parentTable = this->parentTable();
    return is<AccessibilityTable>(parentTable) && downcast<AccessibilityTable>(*parentTable).isExposableThroughAccessibility();
}
    
AccessibilityRole AccessibilityTableCell::determineAccessibilityRole()
{
    // AccessibilityRenderObject::determineAccessibleRole provides any ARIA-supplied
    // role, falling back on the role to be used if we determine here that the element
    // should not be exposed as a cell. Thus if we already know it's a cell, return that.
    AccessibilityRole defaultRole = AccessibilityRenderObject::determineAccessibilityRole();
    if (defaultRole == AccessibilityRole::ColumnHeader || defaultRole == AccessibilityRole::RowHeader || defaultRole == AccessibilityRole::Cell || defaultRole == AccessibilityRole::GridCell)
        return defaultRole;

    if (!isTableCell())
        return defaultRole;
    if (isColumnHeaderCell())
        return AccessibilityRole::ColumnHeader;
    if (isRowHeaderCell())
        return AccessibilityRole::RowHeader;

    return AccessibilityRole::Cell;
}
    
bool AccessibilityTableCell::isTableHeaderCell() const
{
    return node() && node()->hasTagName(thTag);
}

bool AccessibilityTableCell::isColumnHeaderCell() const
{
    const AtomString& scope = getAttribute(scopeAttr);
    if (scope == "col" || scope == "colgroup")
        return true;
    if (scope == "row" || scope == "rowgroup")
        return false;
    if (!isTableHeaderCell())
        return false;

    // We are in a situation after checking the scope attribute.
    // It is an attempt to resolve the type of th element without support in the specification.
    // Checking tableTag and tbodyTag allows to check the case of direct row placement in the table and lets stop the loop at the table level.
    for (Node* parentNode = node(); parentNode; parentNode = parentNode->parentNode()) {
        if (parentNode->hasTagName(theadTag))
            return true;
        if (parentNode->hasTagName(tfootTag))
            return false;
        if (parentNode->hasTagName(tableTag) || parentNode->hasTagName(tbodyTag)) {
            std::pair<unsigned, unsigned> rowRange;
            rowIndexRange(rowRange);
            if (!rowRange.first)
                return true;
            return false;
        }
    }
    return false;
}

bool AccessibilityTableCell::isRowHeaderCell() const
{
    const AtomString& scope = getAttribute(scopeAttr);
    if (scope == "row" || scope == "rowgroup")
        return true;
    if (scope == "col" || scope == "colgroup")
        return false;
    if (!isTableHeaderCell())
        return false;

    // We are in a situation after checking the scope attribute.
    // It is an attempt to resolve the type of th element without support in the specification.
    // Checking tableTag allows to check the case of direct row placement in the table and lets stop the loop at the table level.
    for (Node* parentNode = node(); parentNode; parentNode = parentNode->parentNode()) {
        if (parentNode->hasTagName(tfootTag) || parentNode->hasTagName(tbodyTag) || parentNode->hasTagName(tableTag)) {
            std::pair<unsigned, unsigned> colRange;
            columnIndexRange(colRange);
            if (!colRange.first)
                return true;
            return false;
        }
        if (parentNode->hasTagName(theadTag))
            return false;
    }
    return false;
}

bool AccessibilityTableCell::isTableCellInSameRowGroup(AccessibilityTableCell* otherTableCell)
{
    Node* parentNode = node();
    for ( ; parentNode; parentNode = parentNode->parentNode()) {
        if (parentNode->hasTagName(theadTag) || parentNode->hasTagName(tbodyTag) || parentNode->hasTagName(tfootTag))
            break;
    }
    
    Node* otherParentNode = otherTableCell->node();
    for ( ; otherParentNode; otherParentNode = otherParentNode->parentNode()) {
        if (otherParentNode->hasTagName(theadTag) || otherParentNode->hasTagName(tbodyTag) || otherParentNode->hasTagName(tfootTag))
            break;
    }
    
    return otherParentNode == parentNode;
}


bool AccessibilityTableCell::isTableCellInSameColGroup(AccessibilityTableCell* tableCell)
{
    std::pair<unsigned, unsigned> colRange;
    columnIndexRange(colRange);
    
    std::pair<unsigned, unsigned> otherColRange;
    tableCell->columnIndexRange(otherColRange);
    
    if (colRange.first <= (otherColRange.first + otherColRange.second))
        return true;
    return false;
}
    
String AccessibilityTableCell::expandedTextValue() const
{
    return getAttribute(abbrAttr);
}
    
bool AccessibilityTableCell::supportsExpandedTextValue() const
{
    return isTableHeaderCell() && hasAttribute(abbrAttr);
}
    
void AccessibilityTableCell::columnHeaders(AccessibilityChildrenVector& headers)
{
    AccessibilityTable* parent = parentTable();
    if (!parent)
        return;

    // Choose columnHeaders as the place where the "headers" attribute is reported.
    ariaElementsFromAttribute(headers, headersAttr);
    // If the headers attribute returned valid values, then do not further search for column headers.
    if (!headers.isEmpty())
        return;
    
    std::pair<unsigned, unsigned> rowRange;
    rowIndexRange(rowRange);
    
    std::pair<unsigned, unsigned> colRange;
    columnIndexRange(colRange);
    
    for (unsigned row = 0; row < rowRange.first; row++) {
        AccessibilityTableCell* tableCell = parent->cellForColumnAndRow(colRange.first, row);
        if (!tableCell || tableCell == this || headers.contains(tableCell))
            continue;

        std::pair<unsigned, unsigned> childRowRange;
        tableCell->rowIndexRange(childRowRange);
            
        const AtomString& scope = tableCell->getAttribute(scopeAttr);
        if (scope == "colgroup" && isTableCellInSameColGroup(tableCell))
            headers.append(tableCell);
        else if (tableCell->isColumnHeaderCell())
            headers.append(tableCell);
    }
}
    
void AccessibilityTableCell::rowHeaders(AccessibilityChildrenVector& headers)
{
    AccessibilityTable* parent = parentTable();
    if (!parent)
        return;

    std::pair<unsigned, unsigned> rowRange;
    rowIndexRange(rowRange);

    std::pair<unsigned, unsigned> colRange;
    columnIndexRange(colRange);

    for (unsigned column = 0; column < colRange.first; column++) {
        AccessibilityTableCell* tableCell = parent->cellForColumnAndRow(column, rowRange.first);
        if (!tableCell || tableCell == this || headers.contains(tableCell))
            continue;
        
        const AtomString& scope = tableCell->getAttribute(scopeAttr);
        if (scope == "rowgroup" && isTableCellInSameRowGroup(tableCell))
            headers.append(tableCell);
        else if (tableCell->isRowHeaderCell())
            headers.append(tableCell);
    }
}

AccessibilityTableRow* AccessibilityTableCell::parentRow() const
{
    AccessibilityObject* parent = parentObjectUnignored();
    if (!is<AccessibilityTableRow>(*parent))
        return nullptr;
    return downcast<AccessibilityTableRow>(parent);
}

void AccessibilityTableCell::rowIndexRange(std::pair<unsigned, unsigned>& rowRange) const
{
    if (!is<RenderTableCell>(renderer()))
        return;
    
    RenderTableCell& renderCell = downcast<RenderTableCell>(*m_renderer);

    // ARIA 1.1's aria-rowspan attribute is intended for cells and gridcells which are not contained
    // in a native table. But if we have a valid author-provided value and do not have an explicit
    // native host language value for the rowspan, expose the ARIA value.
    rowRange.second = axRowSpan();
    if (static_cast<int>(rowRange.second) == -1)
        rowRange.second = renderCell.rowSpan();
    
    if (AccessibilityTableRow* parentRow = this->parentRow())
        rowRange.first = parentRow->rowIndex();
}
    
void AccessibilityTableCell::columnIndexRange(std::pair<unsigned, unsigned>& columnRange) const
{
    if (!is<RenderTableCell>(renderer()))
        return;
    
    const RenderTableCell& cell = downcast<RenderTableCell>(*m_renderer);
    columnRange.first = cell.table()->colToEffCol(cell.col());

    // ARIA 1.1's aria-colspan attribute is intended for cells and gridcells which are not contained
    // in a native table. But if we have a valid author-provided value and do not have an explicit
    // native host language value for the colspan, expose the ARIA value.
    columnRange.second = axColumnSpan();
    if (static_cast<int>(columnRange.second) != -1)
        return;

    columnRange.second = cell.table()->colToEffCol(cell.col() + cell.colSpan()) - columnRange.first;
}
    
AccessibilityObject* AccessibilityTableCell::titleUIElement() const
{
    // Try to find if the first cell in this row is a <th>. If it is,
    // then it can act as the title ui element. (This is only in the
    // case when the table is not appearing as an AXTable.)
    if (isTableCell() || !is<RenderTableCell>(renderer()))
        return nullptr;

    // Table cells that are th cannot have title ui elements, since by definition
    // they are title ui elements
    Node* node = m_renderer->node();
    if (node && node->hasTagName(thTag))
        return nullptr;
    
    RenderTableCell& renderCell = downcast<RenderTableCell>(*m_renderer);

    // If this cell is in the first column, there is no need to continue.
    int col = renderCell.col();
    if (!col)
        return nullptr;

    int row = renderCell.rowIndex();

    RenderTableSection* section = renderCell.section();
    if (!section)
        return nullptr;
    
    RenderTableCell* headerCell = section->primaryCellAt(row, 0);
    if (!headerCell || headerCell == &renderCell)
        return nullptr;

    if (!headerCell->element() || !headerCell->element()->hasTagName(thTag))
        return nullptr;
    
    return axObjectCache()->getOrCreate(headerCell);
}
    
int AccessibilityTableCell::axColumnIndex() const
{
    const AtomString& colIndexValue = getAttribute(aria_colindexAttr);
    if (colIndexValue.toInt() >= 1)
        return colIndexValue.toInt();

    // "ARIA 1.1: If the set of columns which is present in the DOM is contiguous, and if there are no cells which span more than one row
    // or column in that set, then authors may place aria-colindex on each row, setting the value to the index of the first column of the set."
    // Here, we let its parent row to set its index beforehand, so we don't have to go through the siblings to calculate the index.
    AccessibilityTableRow* parentRow = this->parentRow();
    if (parentRow && m_axColIndexFromRow != -1)
        return m_axColIndexFromRow;
    
    return -1;
}
    
int AccessibilityTableCell::axRowIndex() const
{
    // ARIA 1.1: Authors should place aria-rowindex on each row. Authors may also place
    // aria-rowindex on all of the children or owned elements of each row.
    const AtomString& rowIndexValue = getAttribute(aria_rowindexAttr);
    if (rowIndexValue.toInt() >= 1)
        return rowIndexValue.toInt();
    
    if (AccessibilityTableRow* parentRow = this->parentRow())
        return parentRow->axRowIndex();
    
    return -1;
}

int AccessibilityTableCell::axColumnSpan() const
{
    // According to the ARIA spec, "If aria-colpan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-colspan."
    if (hasAttribute(colspanAttr))
        return -1;

    const AtomString& colSpanValue = getAttribute(aria_colspanAttr);
    // ARIA 1.1: Authors must set the value of aria-colspan to an integer greater than or equal to 1.
    if (colSpanValue.toInt() >= 1)
        return colSpanValue.toInt();
    
    return -1;
}

int AccessibilityTableCell::axRowSpan() const
{
    // According to the ARIA spec, "If aria-rowspan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-rowspan."
    if (hasAttribute(rowspanAttr))
        return -1;

    const AtomString& rowSpanValue = getAttribute(aria_rowspanAttr);
    
    // ARIA 1.1: Authors must set the value of aria-rowspan to an integer greater than or equal to 0.
    // Setting the value to 0 indicates that the cell or gridcell is to span all the remaining rows in the row group.
    if (rowSpanValue == "0")
        return 0;
    if (rowSpanValue.toInt() >= 1)
        return rowSpanValue.toInt();
    
    return -1;
}
    
} // namespace WebCore
