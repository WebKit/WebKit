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
#include "AXUtilities.h"
#include "AccessibilityTableRow.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableCellElement.h"
#include "RenderElementInlines.h"
#include "RenderObject.h"
#include "RenderTableCell.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityTableCell::AccessibilityTableCell(AXID axID, RenderObject& renderer, AXObjectCache& cache, bool isARIAGridCell)
    : AccessibilityRenderObject(axID, renderer, cache)
{
    m_isARIAGridCell = isARIAGridCell;
}

AccessibilityTableCell::AccessibilityTableCell(AXID axID, Node& node, AXObjectCache& cache, bool isARIAGridCell)
    : AccessibilityRenderObject(axID, node, cache)
{
    m_isARIAGridCell = isARIAGridCell;
}

AccessibilityTableCell::~AccessibilityTableCell() = default;

Ref<AccessibilityTableCell> AccessibilityTableCell::create(AXID axID, RenderObject& renderer, AXObjectCache& cache, bool isARIAGridCell)
{
    return adoptRef(*new AccessibilityTableCell(axID, renderer, cache, isARIAGridCell));
}

Ref<AccessibilityTableCell> AccessibilityTableCell::create(AXID axID, Node& node, AXObjectCache& cache, bool isARIAGridCell)
{
    return adoptRef(*new AccessibilityTableCell(axID, node, cache, isARIAGridCell));
}

bool AccessibilityTableCell::computeIsIgnored() const
{
    auto decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;

    // Ignore anonymous table cells as long as they're not in a table (ie. when display:table is used).
    RefPtr parentTable = this->parentTable();
    RefPtr parentElement = parentTable ? parentTable->element() : nullptr;
    bool inTable = parentElement && (parentElement->elementName() == ElementName::HTML_table || hasTableRole(*parentElement));
    if (!inTable && !element())
        return true;

    return !isExposedTableCell() && AccessibilityRenderObject::computeIsIgnored();
}

AccessibilityObject* AccessibilityTableCell::parentTable() const
{
    // ARIA gridcells may have multiple levels of unignored ancestors that are not the parent table,
    // including rows and interactive rowgroups. In addition, poorly-formed grids may contain elements
    // which pass the tests for inclusion.
    if (isARIAGridCell()) {
        return Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
            return ancestor.isExposableTable() && !ancestor.isIgnored();
        });
    }

    CheckedPtr cache = axObjectCache();
    // If the document no longer exists, we might not have an axObjectCache.
    if (!cache)
        return nullptr;

    // Do not use getOrCreate. parentTable() can be called while the render tree is being modified
    // by javascript, and creating a table element may try to access the render tree while in a bad state.
    // By using only get() implies that the AXTable must be created before AXTableCells. This should
    // always be the case when AT clients access a table.
    // https://bugs.webkit.org/show_bug.cgi?id=42652
    RefPtr<AccessibilityObject> tableFromRenderTree;
    if (auto* renderTableCell = dynamicDowncast<RenderTableCell>(renderer()))
        tableFromRenderTree = cache->get(renderTableCell->table());

    if (!tableFromRenderTree || !tableFromRenderTree->isTable()) {
        if (node()) {
            return Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
                return ancestor.isTable();
            });
        }
        return nullptr;
    }

    // The RenderTableCell's table() object might be anonymous sometimes. We should handle it gracefully
    // by finding the right table.
    if (!tableFromRenderTree->node()) {
        for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
            // If this is a non-anonymous table object, but not an accessibility table, we should stop because
            // we don't want to choose another ancestor table as this cell's table.
            if (ancestor->isTable()) {
                if (ancestor->isExposableTable())
                    return ancestor.get();
                if (ancestor->node())
                    break;
            }
        }
        return nullptr;
    }

    return tableFromRenderTree.get();
}

String AccessibilityTableCell::readOnlyValue() const
{
    const AtomString& readOnlyValue = getAttribute(aria_readonlyAttr);
    if (readOnlyValue != nullAtom())
        return readOnlyValue.string().convertToASCIILowercase();

    // ARIA 1.1 requires user agents to propagate the grid's aria-readonly value to all
    // gridcell elements if the property is not present on the gridcell element itself.
    if (RefPtr parent = parentTable())
        return parent->readOnlyValue();

    return String();
}

bool AccessibilityTableCell::isExposedTableCell() const
{
    // If the parent table is an accessibility table, then we are a table cell.
    // This used to check if the unignoredParent was a row, but that exploded performance if
    // this was in nested tables. This check should be just as good.
    RefPtr parentTable = this->parentTable();
    return parentTable && parentTable->isExposableTable();
}

AccessibilityRole AccessibilityTableCell::determineAccessibilityRole()
{
    // AccessibilityRenderObject::determineAccessibleRole provides any ARIA-supplied
    // role, falling back on the role to be used if we determine here that the element
    // should not be exposed as a cell. Thus if we already know it's a cell, return that.
    AccessibilityRole defaultRole = AccessibilityRenderObject::determineAccessibilityRole();
    if (defaultRole == AccessibilityRole::ColumnHeader || defaultRole == AccessibilityRole::RowHeader || defaultRole == AccessibilityRole::Cell || defaultRole == AccessibilityRole::GridCell)
        return defaultRole;

    // This matches the logic of `isExposedTableCell()`, but allows us to keep the pointer to the parentTable
    // for use at the bottom of this method.
    RefPtr parentTable = this->parentTable();
    if (!parentTable || !parentTable->isExposableTable())
        return defaultRole;
    return parentTable->hasGridRole() ? AccessibilityRole::GridCell : AccessibilityRole::Cell;
}

bool AccessibilityTableCell::isTableHeaderCell() const
{
    RefPtr node = this->node();
    if (!node)
        return false;

    auto elementName = WebCore::elementName(*node);
    if (elementName == ElementName::HTML_th)
        return true;

    if (elementName == ElementName::HTML_td) {
        RefPtr current = node->parentNode();
        // i < 2 is used here because in a properly structured table, the thead should be 2 levels away from the td.
        for (int i = 0; i < 2 && current; i++) {
            if (WebCore::elementName(*current) == ElementName::HTML_thead)
                return true;
            current = current->parentNode();
        }
    }
    return false;
}

bool AccessibilityTableCell::isColumnHeader() const
{
    if (role() == AccessibilityRole::ColumnHeader)
        return true;
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
    for (RefPtr ancestor = node()->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        auto elementName = WebCore::elementName(*ancestor);
        if (elementName == ElementName::HTML_thead)
            return true;
        if (elementName == ElementName::HTML_tfoot)
            return false;
        if (elementName == ElementName::HTML_table || elementName == ElementName::HTML_tbody) {
            // If we're in the first row, we're a column header.
            if (!rowIndexRange().first)
                return true;
            return false;
        }
    }
    return false;
}

bool AccessibilityTableCell::isRowHeader() const
{
    if (role() == AccessibilityRole::RowHeader)
        return true;
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
    for (RefPtr ancestor = node()->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        auto elementName = WebCore::elementName(*ancestor);
        if (elementName == ElementName::HTML_tfoot || elementName == ElementName::HTML_tbody || elementName == ElementName::HTML_table) {
            // If we're in the first column, we're a row header.
            if (!columnIndexRange().first)
                return true;
            return false;
        }

        if (elementName == ElementName::HTML_thead)
            return false;
    }
    return false;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityTableCell::rowHeaders()
{
    AccessibilityChildrenVector headers;
    RefPtr parent = parentTable();
    if (!parent)
        return headers;

    auto rowRange = rowIndexRange();
    auto colRange = columnIndexRange();

    for (unsigned column = 0; column < colRange.first; column++) {
        RefPtr tableCell = parent->cellForColumnAndRow(column, rowRange.first);
        if (!tableCell || tableCell == this || headers.contains(Ref { *tableCell }))
            continue;

        if (tableCell->cellScope() == "rowgroup"_s && isTableCellInSameRowGroup(*tableCell))
            headers.append(tableCell.releaseNonNull());
        else if (tableCell->isRowHeader())
            headers.append(tableCell.releaseNonNull());
    }

    return headers;
}

AccessibilityTableRow* AccessibilityTableCell::parentRow() const
{
    return dynamicDowncast<AccessibilityTableRow>(parentObjectUnignored());
}

void AccessibilityTableCell::ensureIndexesUpToDate() const
{
    if (RefPtr parentTable = this->parentTable())
        parentTable->ensureCellIndexesUpToDate();
}

void AccessibilityTableCell::setRowIndex(unsigned index)
{
    if (m_rowIndex == index)
        return;
    m_rowIndex = index;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto* cache = axObjectCache())
        cache->rowIndexChanged(*this);
#endif
}

void AccessibilityTableCell::setColumnIndex(unsigned index)
{
    if (m_columnIndex == index)
        return;
    m_columnIndex = index;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto* cache = axObjectCache())
        cache->columnIndexChanged(*this);
#endif
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
    if (WebCore::elementName(node()) == ElementName::HTML_th)
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

    RefPtr element = headerCell->element();
    if (!element || element->elementName() != ElementName::HTML_th)
        return nullptr;

    return axObjectCache()->getOrCreate(*headerCell);
}

std::optional<unsigned> AccessibilityTableCell::axColumnIndex() const
{
    if (int value = integralAttribute(aria_colindexAttr); value >= 1)
        return value;

    // "ARIA 1.1: If the set of columns which is present in the DOM is contiguous, and if there are no cells which span more than one row
    // or column in that set, then authors may place aria-colindex on each row, setting the value to the index of the first column of the set."
    // Here, we let its parent row to set its index beforehand, so we don't have to go through the siblings to calculate the index.
    if (m_axColIndexFromRow != -1 && parentRow())
        return m_axColIndexFromRow;

    return { };
}

std::optional<unsigned> AccessibilityTableCell::axRowIndex() const
{
    // ARIA 1.1: Authors should place aria-rowindex on each row. Authors may also place
    // aria-rowindex on all of the children or owned elements of each row.
    if (int value = integralAttribute(aria_rowindexAttr); value >= 1)
        return value;

    if (RefPtr parentRow = this->parentRow())
        return parentRow->axRowIndex();

    return { };
}

String AccessibilityTableCell::axColumnIndexText() const
{
    return getAttribute(aria_colindextextAttr);
}

String AccessibilityTableCell::axRowIndexText() const
{
    if (String text = getAttribute(aria_rowindextextAttr); !text.isNull())
        return text;

    if (RefPtr parentRow = this->parentRow())
        return parentRow->axRowIndexText();

    return { };
}

unsigned AccessibilityTableCell::rowSpan() const
{
    // According to the ARIA spec, "If aria-rowspan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-rowspan."
    if (auto rowSpan = parseHTMLInteger(getAttribute(rowspanAttr))) {
        if (*rowSpan < 0)
            return 1;
        return std::min(static_cast<unsigned>(*rowSpan), HTMLTableCellElement::maxRowspan);
    }

    if (auto ariaRowSpan = parseHTMLInteger(getAttribute(aria_rowspanAttr))) {
        if (*ariaRowSpan < 0)
            return 1;
        return std::min(static_cast<unsigned>(*ariaRowSpan), HTMLTableCellElement::maxRowspan);
    }

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
    if (int value = integralAttribute(aria_colspanAttr); value >= 1)
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
    if (int value = integralAttribute(aria_rowspanAttr); value >= 1)
        return value;

    return -1;
}
#endif // USE(ATSPI)

} // namespace WebCore
