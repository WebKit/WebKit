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

namespace WebCore {
    
AccessibilityARIAGridCell::AccessibilityARIAGridCell(RenderObject* renderer)
    : AccessibilityTableCell(renderer)
{
}

AccessibilityARIAGridCell::~AccessibilityARIAGridCell()
{
}

Ref<AccessibilityARIAGridCell> AccessibilityARIAGridCell::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityARIAGridCell(renderer));
}

AccessibilityTable* AccessibilityARIAGridCell::parentTable() const
{
    // ARIA gridcells may have multiple levels of unignored ancestors that are not the parent table,
    // including rows and interactive rowgroups. In addition, poorly-formed grids may contain elements
    // which pass the tests for inclusion.
    for (AccessibilityObject* parent = parentObjectUnignored(); parent; parent = parent->parentObjectUnignored()) {
        if (is<AccessibilityTable>(*parent) && downcast<AccessibilityTable>(*parent).isExposableThroughAccessibility())
            return downcast<AccessibilityTable>(parent);
    }

    return nullptr;
}
    
void AccessibilityARIAGridCell::rowIndexRange(std::pair<unsigned, unsigned>& rowRange) const
{
    AccessibilityObject* parent = parentObjectUnignored();
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

    // as far as I can tell, grid cells cannot span rows
    rowRange.second = 1;
}

void AccessibilityARIAGridCell::columnIndexRange(std::pair<unsigned, unsigned>& columnRange) const
{
    AccessibilityObject* parent = parentObjectUnignored();
    if (!parent)
        return;

    if (!is<AccessibilityTableRow>(*parent)
        && !(is<AccessibilityTable>(*parent) && downcast<AccessibilityTable>(*parent).isExposableThroughAccessibility()))
        return;

    const AccessibilityChildrenVector& siblings = parent->children();
    unsigned childrenSize = siblings.size();
    for (unsigned k = 0; k < childrenSize; ++k) {
        if (siblings[k].get() == this) {
            columnRange.first = k;
            break;
        }
    }
    
    // as far as I can tell, grid cells cannot span columns
    columnRange.second = 1;    
}
  
} // namespace WebCore
