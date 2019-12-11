/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "AccessibilityARIAGridCell.h"

#include "AccessibilityObject.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableRow.h"
#include "HTMLNames.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityARIAGridCell::AccessibilityARIAGridCell(RenderObject* renderer)
    : AccessibilityTableCell(renderer)
{
}

AccessibilityARIAGridCell::~AccessibilityARIAGridCell() = default;

Ref<AccessibilityARIAGridCell> AccessibilityARIAGridCell::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityARIAGridCell(renderer));
}

AccessibilityTable* AccessibilityARIAGridCell::parentTable() const
{
    // ARIA gridcells may have multiple levels of unignored ancestors that are not the parent table,
    // including rows and interactive rowgroups. In addition, poorly-formed grids may contain elements
    // which pass the tests for inclusion.
    for (auto* parent = parentObjectUnignored(); parent; parent = parent->parentObjectUnignored()) {
        if (is<AccessibilityTable>(*parent) && downcast<AccessibilityTable>(*parent).isExposableThroughAccessibility())
            return downcast<AccessibilityTable>(parent);
    }

    return nullptr;
}
    
void AccessibilityARIAGridCell::rowIndexRange(std::pair<unsigned, unsigned>& rowRange) const
{
    AXCoreObject* parent = parentObjectUnignored();
    if (!parent)
        return;

    if (is<AccessibilityTableRow>(*parent)) {
        // We already got a table row, use its API.
        rowRange.first = downcast<AccessibilityTableRow>(*parent).rowIndex();
    } else if (is<AccessibilityTable>(*parent) && downcast<AccessibilityTable>(*parent).isExposableThroughAccessibility()) {
        // We reached the parent table, so we need to inspect its
        // children to determine the row index for the cell in it.
        unsigned columnCount = downcast<AccessibilityTable>(*parent).columnCount();
        if (!columnCount)
            return;

        const auto& siblings = parent->children();
        unsigned childrenSize = siblings.size();
        for (unsigned k = 0; k < childrenSize; ++k) {
            if (siblings[k].get() == this) {
                rowRange.first = k / columnCount;
                break;
            }
        }
    }

    // ARIA 1.1, aria-rowspan attribute is intended for cells and gridcells which are not contained in a native table.
    // So we should check for that attribute here.
    rowRange.second = axRowSpanWithRowIndex(rowRange.first);
}

unsigned AccessibilityARIAGridCell::axRowSpanWithRowIndex(unsigned rowIndex) const
{
    int rowSpan = AccessibilityTableCell::axRowSpan();
    if (rowSpan == -1) {
        std::pair<unsigned, unsigned> range;
        AccessibilityTableCell::rowIndexRange(range);
        return std::max(static_cast<int>(range.second), 1);
    }

    AXCoreObject* parent = parentObjectUnignored();
    if (!parent)
        return 1;
    
    // Setting the value to 0 indicates that the cell or gridcell is to span all the remaining rows in the row group.
    if (!rowSpan) {
        // rowSpan defaults to 1.
        rowSpan = 1;
        if (AccessibilityObject* parentRowGroup = this->parentRowGroup()) {
            // If the row group is the parent table, we use total row count to calculate the span.
            if (is<AccessibilityTable>(*parentRowGroup))
                rowSpan = downcast<AccessibilityTable>(*parentRowGroup).rowCount() - rowIndex;
            // Otherwise, we have to get the index for the current row within the parent row group.
            else if (is<AccessibilityTableRow>(*parent)) {
                const auto& siblings = parentRowGroup->children();
                unsigned rowCount = siblings.size();
                for (unsigned k = 0; k < rowCount; ++k) {
                    if (siblings[k].get() == parent) {
                        rowSpan = rowCount - k;
                        break;
                    }
                }
            }
        }
    }

    return rowSpan;
}

void AccessibilityARIAGridCell::columnIndexRange(std::pair<unsigned, unsigned>& columnRange) const
{
    AXCoreObject* parent = parentObjectUnignored();
    if (!parent)
        return;

    if (!is<AccessibilityTableRow>(*parent)
        && !(is<AccessibilityTable>(*parent) && downcast<AccessibilityTable>(*parent).isExposableThroughAccessibility()))
        return;

    const AccessibilityChildrenVector& siblings = parent->children();
    unsigned childrenSize = siblings.size();
    unsigned indexWithSpan = 0;
    for (unsigned k = 0; k < childrenSize; ++k) {
        auto child = siblings[k].get();
        if (child == this) {
            columnRange.first = indexWithSpan;
            break;
        }
        indexWithSpan += is<AccessibilityTableCell>(*child) ? std::max(downcast<AccessibilityTableCell>(*child).axColumnSpan(), 1) : 1;
    }
    
    // ARIA 1.1, aria-colspan attribute is intended for cells and gridcells which are not contained in a native table.
    // So we should check for that attribute here.
    int columnSpan = AccessibilityTableCell::axColumnSpan();
    if (columnSpan == -1) {
        std::pair<unsigned, unsigned> range;
        AccessibilityTableCell::columnIndexRange(range);
        columnSpan = range.second;
    }

    columnRange.second = std::max(columnSpan, 1);
}

AccessibilityObject* AccessibilityARIAGridCell::parentRowGroup() const
{
    for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        if (parent->hasTagName(theadTag) || parent->hasTagName(tbodyTag) || parent->hasTagName(tfootTag) || parent->roleValue() == AccessibilityRole::RowGroup)
            return parent;
    }
    
    // If there's no row group found, we use the parent table as the row group.
    return parentTable();
}

String AccessibilityARIAGridCell::readOnlyValue() const
{
    if (hasAttribute(aria_readonlyAttr))
        return getAttribute(aria_readonlyAttr).string().convertToASCIILowercase();

    // ARIA 1.1 requires user agents to propagate the grid's aria-readonly value to all
    // gridcell elements if the property is not present on the gridcell element itelf.
    if (AccessibilityObject* parent = parentTable())
        return parent->readOnlyValue();

    return String();
}
  
} // namespace WebCore
