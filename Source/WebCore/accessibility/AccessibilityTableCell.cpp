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
#include "HTMLParserIdioms.h"
#include "RenderObject.h"
#include "RenderTableCell.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityTableCell::AccessibilityTableCell(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityTableCell::AccessibilityTableCell(Node& node)
    : AccessibilityRenderObject(node)
{
}

AccessibilityTableCell::~AccessibilityTableCell() = default;

Ref<AccessibilityTableCell> AccessibilityTableCell::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityTableCell(renderer));
}

Ref<AccessibilityTableCell> AccessibilityTableCell::create(Node& node)
{
    return adoptRef(*new AccessibilityTableCell(node));
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
    bool inTable = renderTable && renderTable->node() && (renderTable->node()->hasTagName(tableTag) || nodeHasGridRole(renderTable->node()));
    if (!node() && !inTable)
        return true;
        
    if (!isExposedTableCell())
        return AccessibilityRenderObject::computeAccessibilityIsIgnored();
    
    return false;
}

AccessibilityTable* AccessibilityTableCell::parentTable() const
{
    // If the document no longer exists, we might not have an axObjectCache.
    if (!axObjectCache())
        return nullptr;
    
    // Do not use getOrCreate. parentTable() can be called while the render tree is being modified 
    // by javascript, and creating a table element may try to access the render tree while in a bad state.
    // By using only get() implies that the AXTable must be created before AXTableCells. This should
    // always be the case when AT clients access a table.
    // https://bugs.webkit.org/show_bug.cgi?id=42652
    AccessibilityObject* tableFromRenderTree = nullptr;
    if (auto* renderTableCell = dynamicDowncast<RenderTableCell>(renderer()))
        tableFromRenderTree = axObjectCache()->get(renderTableCell->table());
    else if (node()) {
        return downcast<AccessibilityTable>(Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
            return is<AccessibilityTable>(ancestor);
        }));
    }

    if (!tableFromRenderTree)
        return nullptr;
    
    // The RenderTableCell's table() object might be anonymous sometimes. We should handle it gracefully
    // by finding the right table.
    if (!tableFromRenderTree->node()) {
        for (auto* ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
            // If this is a non-anonymous table object, but not an accessibility table, we should stop because
            // we don't want to choose another ancestor table as this cell's table.
            if (auto* ancestorTable = dynamicDowncast<AccessibilityTable>(ancestor)) {
                if (ancestorTable->isExposable())
                    return ancestorTable;
                if (ancestorTable->node())
                    break;
            }
        }
        return nullptr;
    }
    
    return dynamicDowncast<AccessibilityTable>(tableFromRenderTree);
}
    
bool AccessibilityTableCell::isExposedTableCell() const
{
    // If the parent table is an accessibility table, then we are a table cell.
    // This used to check if the unignoredParent was a row, but that exploded performance if
    // this was in nested tables. This check should be just as good.
    auto* parentTable = this->parentTable();
    return parentTable && parentTable->isExposable();
}
    
AccessibilityRole AccessibilityTableCell::determineAccessibilityRole()
{
    // AccessibilityRenderObject::determineAccessibleRole provides any ARIA-supplied
    // role, falling back on the role to be used if we determine here that the element
    // should not be exposed as a cell. Thus if we already know it's a cell, return that.
    AccessibilityRole defaultRole = AccessibilityRenderObject::determineAccessibilityRole();
    if (defaultRole == AccessibilityRole::ColumnHeader || defaultRole == AccessibilityRole::RowHeader || defaultRole == AccessibilityRole::Cell || defaultRole == AccessibilityRole::GridCell)
        return defaultRole;

    if (!isExposedTableCell())
        return defaultRole;
    if (isColumnHeader())
        return AccessibilityRole::ColumnHeader;
    if (isRowHeader())
        return AccessibilityRole::RowHeader;

    return AccessibilityRole::Cell;
}
    
bool AccessibilityTableCell::isTableHeaderCell() const
{
    auto* node = this->node();
    if (!node)
        return false;

    if (node->hasTagName(thTag))
        return true;

    if (node->hasTagName(tdTag)) {
        auto* current = node->parentNode();
        // i < 2 is used here because in a properly structured table, the thead should be 2 levels away from the td.
        for (int i = 0; i < 2 && current; i++) {
            if (current->hasTagName(theadTag))
                return true;
            current = current->parentNode();
        }
    }
    return false;
}

bool AccessibilityTableCell::isColumnHeader() const
{
    const AtomString& scope = getAttribute(scopeAttr);
    if (scope == "col"_s || scope == "colgroup"_s)
        return true;
    if (scope == "row"_s || scope == "rowgroup"_s)
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
            auto rowRange = rowIndexRange();
            if (!rowRange.first)
                return true;
            return false;
        }
    }
    return false;
}

bool AccessibilityTableCell::isRowHeader() const
{
    const AtomString& scope = getAttribute(scopeAttr);
    if (scope == "row"_s || scope == "rowgroup"_s)
        return true;
    if (scope == "col"_s || scope == "colgroup"_s)
        return false;
    if (!isTableHeaderCell())
        return false;

    // We are in a situation after checking the scope attribute.
    // It is an attempt to resolve the type of th element without support in the specification.
    // Checking tableTag allows to check the case of direct row placement in the table and lets stop the loop at the table level.
    for (Node* parentNode = node(); parentNode; parentNode = parentNode->parentNode()) {
        if (parentNode->hasTagName(tfootTag) || parentNode->hasTagName(tbodyTag) || parentNode->hasTagName(tableTag)) {
            auto colRange = columnIndexRange();
            if (!colRange.first)
                return true;
            return false;
        }
        if (parentNode->hasTagName(theadTag))
            return false;
    }
    return false;
}
    
AXID AccessibilityTableCell::rowGroupAncestorID() const
{
    auto* rowGroup = Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
        return ancestor.hasTagName(theadTag) || ancestor.hasTagName(tbodyTag) || ancestor.hasTagName(tfootTag);
    });
    if (!rowGroup)
        return { };

    return rowGroup->objectID();
}
    
String AccessibilityTableCell::expandedTextValue() const
{
    return getAttribute(abbrAttr);
}
    
bool AccessibilityTableCell::supportsExpandedTextValue() const
{
    return isTableHeaderCell() && hasAttribute(abbrAttr);
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTableCell::columnHeaders()
{
    auto* parent = parentTable();
    if (!parent)
        return { };

    // Choose columnHeaders as the place where the "headers" attribute is reported.
    auto headers = relatedObjects(AXRelationType::Headers);
    // If the headers attribute returned valid values, then do not further search for column headers.
    if (!headers.isEmpty())
        return headers;

    auto rowRange = rowIndexRange();
    auto colRange = columnIndexRange();

    for (unsigned row = 0; row < rowRange.first; row++) {
        auto* tableCell = parent->cellForColumnAndRow(colRange.first, row);
        if (!tableCell || tableCell == this || headers.contains(tableCell))
            continue;

        ASSERT(is<AccessibilityObject>(tableCell));
        if (tableCell->cellScope() == "colgroup"_s && isTableCellInSameColGroup(tableCell))
            headers.append(tableCell);
        else if (tableCell->isColumnHeader())
            headers.append(tableCell);
    }

    return headers;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTableCell::rowHeaders()
{
    AccessibilityChildrenVector headers;
    AccessibilityTable* parent = parentTable();
    if (!parent)
        return headers;

    auto rowRange = rowIndexRange();
    auto colRange = columnIndexRange();

    for (unsigned column = 0; column < colRange.first; column++) {
        auto* tableCell = parent->cellForColumnAndRow(column, rowRange.first);
        if (!tableCell || tableCell == this || headers.contains(tableCell))
            continue;

        if (tableCell->cellScope() == "rowgroup"_s && isTableCellInSameRowGroup(tableCell))
            headers.append(tableCell);
        else if (tableCell->isRowHeader())
            headers.append(tableCell);
    }

    return headers;
}

AccessibilityTableRow* AccessibilityTableCell::ariaOwnedByParent() const
{
    auto owners = this->owners();
    if (owners.size() == 1 && owners[0]->isTableRow())
        return downcast<AccessibilityTableRow>(owners[0].get());
    return nullptr;
}

AccessibilityObject* AccessibilityTableCell::parentObjectUnignored() const
{
    if (auto ownerParent = ariaOwnedByParent())
        return ownerParent;
    return AccessibilityRenderObject::parentObjectUnignored();
}

AccessibilityTableRow* AccessibilityTableCell::parentRow() const
{
    if (auto ownerParent = ariaOwnedByParent())
        return ownerParent;
    return dynamicDowncast<AccessibilityTableRow>(parentObjectUnignored());
}

void AccessibilityTableCell::ensureIndexesUpToDate() const
{
    if (auto* parentTable = this->parentTable())
        parentTable->ensureCellIndexesUpToDate();
}

std::pair<unsigned, unsigned> AccessibilityTableCell::rowIndexRange() const
{
    ensureIndexesUpToDate();
    return { m_rowIndex, m_effectiveRowSpan };
}
    
std::pair<unsigned, unsigned> AccessibilityTableCell::columnIndexRange() const
{
    ensureIndexesUpToDate();
    return { m_columnIndex, colSpan() };
}

AccessibilityObject* AccessibilityTableCell::titleUIElement() const
{
    // Try to find if the first cell in this row is a <th>. If it is,
    // then it can act as the title ui element. (This is only in the
    // case when the table is not appearing as an AXTable.)
    if (isExposedTableCell() || !is<RenderTableCell>(renderer()))
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
    if (int value = getIntegralAttribute(aria_colindexAttr); value >= 1)
        return value;

    // "ARIA 1.1: If the set of columns which is present in the DOM is contiguous, and if there are no cells which span more than one row
    // or column in that set, then authors may place aria-colindex on each row, setting the value to the index of the first column of the set."
    // Here, we let its parent row to set its index beforehand, so we don't have to go through the siblings to calculate the index.
    if (m_axColIndexFromRow != -1 && parentRow())
        return m_axColIndexFromRow;

    return -1;
}
    
int AccessibilityTableCell::axRowIndex() const
{
    // ARIA 1.1: Authors should place aria-rowindex on each row. Authors may also place
    // aria-rowindex on all of the children or owned elements of each row.
    if (int value = getIntegralAttribute(aria_rowindexAttr); value >= 1)
        return value;

    if (AccessibilityTableRow* parentRow = this->parentRow())
        return parentRow->axRowIndex();

    return -1;
}

unsigned AccessibilityTableCell::rowSpan() const
{
    // According to the ARIA spec, "If aria-rowspan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-rowspan."
    if (auto rowSpan = parseHTMLInteger(getAttribute(rowspanAttr)); rowSpan && *rowSpan >= 1) {
        // https://html.spec.whatwg.org/multipage/tables.html
        // If rowspan is greater than 65534, let it be 65534 instead.
        return std::min(std::max(*rowSpan, 1), 65534);
    }
    if (auto ariaRowSpan = parseHTMLInteger(getAttribute(aria_rowspanAttr)); ariaRowSpan && *ariaRowSpan >= 1)
        return std::min(std::max(*ariaRowSpan, 1), 65534);
    return 1;
}

unsigned AccessibilityTableCell::colSpan() const
{
    if (auto colSpan = parseHTMLInteger(getAttribute(colspanAttr)); colSpan && *colSpan >= 1) {
        // https://html.spec.whatwg.org/multipage/tables.html
        // If colspan is greater than 1000, let it be 1000 instead.
        return std::min(std::max(*colSpan, 1), 1000);
    }
    if (auto ariaColSpan = parseHTMLInteger(getAttribute(aria_colspanAttr)); ariaColSpan && *ariaColSpan >= 1)
        return std::min(std::max(*ariaColSpan, 1), 1000);
    return 1;
}

#if USE(ATSPI)
int AccessibilityTableCell::axColumnSpan() const
{
    // According to the ARIA spec, "If aria-colpan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-colspan."
    if (hasAttribute(colspanAttr))
        return -1;

    // ARIA 1.1: Authors must set the value of aria-colspan to an integer greater than or equal to 1.
    if (int value = getIntegralAttribute(aria_colspanAttr); value >= 1)
        return value;

    return -1;
}

int AccessibilityTableCell::axRowSpan() const
{
    // According to the ARIA spec, "If aria-rowspan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-rowspan."
    if (hasAttribute(rowspanAttr))
        return -1;

    // ARIA 1.1: Authors must set the value of aria-rowspan to an integer greater than or equal to 0.
    // Setting the value to 0 indicates that the cell or gridcell is to span all the remaining rows in the row group.
    if (getAttribute(aria_rowspanAttr) == "0"_s)
        return 0;
    if (int value = getIntegralAttribute(aria_rowspanAttr); value >= 1)
        return value;

    return -1;
}
#endif // USE(ATSPI)

} // namespace WebCore
