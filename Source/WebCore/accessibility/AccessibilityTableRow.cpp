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
#include "AccessibilityTableRow.h"

#include "AXObjectCache.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "HTMLNames.h"
#include "RenderObject.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityTableRow::AccessibilityTableRow(RenderObject& renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityTableRow::AccessibilityTableRow(Node& node)
    : AccessibilityRenderObject(node)
{
}

AccessibilityTableRow::~AccessibilityTableRow() = default;

Ref<AccessibilityTableRow> AccessibilityTableRow::create(RenderObject& renderer)
{
    return adoptRef(*new AccessibilityTableRow(renderer));
}

Ref<AccessibilityTableRow> AccessibilityTableRow::create(Node& node)
{
    return adoptRef(*new AccessibilityTableRow(node));
}

AccessibilityRole AccessibilityTableRow::determineAccessibilityRole()
{
    if (!isTableRow())
        return AccessibilityRenderObject::determineAccessibilityRole();

    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown)
        return m_ariaRole;

    return AccessibilityRole::Row;
}

bool AccessibilityTableRow::isTableRow() const
{
    auto* table = parentTable();
    return table && table->isExposable();
}
    
AccessibilityObject* AccessibilityTableRow::observableObject() const
{
    // This allows the table to be the one who sends notifications about tables.
    return parentTable();
}
    
bool AccessibilityTableRow::computeAccessibilityIsIgnored() const
{    
    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;
    
    if (!isTableRow())
        return AccessibilityRenderObject::computeAccessibilityIsIgnored();

    return isDOMHidden() || ignoredFromPresentationalRole();
}
    
AccessibilityTable* AccessibilityTableRow::parentTable() const
{
    // The parent table might not be the direct ancestor of the row unfortunately. ARIA states that role="grid" should
    // only have "row" elements, but if not, we still should handle it gracefully by finding the right table.
    for (RefPtr parent = parentObject(); parent; parent = parent->parentObject()) {
        // If this is a non-anonymous table object, but not an accessibility table, we should stop because we don't want to
        // choose another ancestor table as this row's table.
        if (auto* parentTable = dynamicDowncast<AccessibilityTable>(*parent)) {
            if (parentTable->isExposable())
                return parentTable;
            if (parentTable->node())
                break;
        }
    }
    return nullptr;
}

void AccessibilityTableRow::setRowIndex(unsigned rowIndex)
{
    if (m_rowIndex == rowIndex)
        return;
    m_rowIndex = rowIndex;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto* cache = axObjectCache())
        cache->rowIndexChanged(*this);
#endif
}

AXCoreObject* AccessibilityTableRow::rowHeader()
{
    const auto& rowChildren = children();
    if (rowChildren.isEmpty())
        return nullptr;
    
    RefPtr firstCell = rowChildren[0].get();
    if (!firstCell || !firstCell->node() || !firstCell->node()->hasTagName(thTag))
        return nullptr;

    // Verify that the row header is not part of an entire row of headers.
    // In that case, it is unlikely this is a row header.
    for (const auto& child : rowChildren) {
        // We found a non-header cell, so this is not an entire row of headers -- return the original header cell.
        if (child->node() && !child->node()->hasTagName(thTag))
            return firstCell.get();
    }
    return nullptr;
}

void AccessibilityTableRow::addChildren()
{
    // If the element specifies its cells through aria-owns, return that first.
    auto ownedObjects = this->ownedObjects();
    if (ownedObjects.size()) {
        for (auto& object : ownedObjects)
            addChild(object.get(), DescendIfIgnored::No);
        m_childrenInitialized = true;
        m_subtreeDirty = false;
    }
    else
        AccessibilityRenderObject::addChildren();

    // "ARIA 1.1, If the set of columns which is present in the DOM is contiguous, and if there are no cells which span more than one row or
    // column in that set, then authors may place aria-colindex on each row, setting the value to the index of the first column of the set."
    // Update child cells' axColIndex if there's an aria-colindex value set for the row. So the cell doesn't have to go through the siblings
    // to calculate the index.
    int colIndex = axColumnIndex();
    if (colIndex == -1)
        return;

    unsigned index = 0;
    for (const auto& cell : children()) {
        if (auto* tableCell = dynamicDowncast<AccessibilityTableCell>(cell.get()))
            tableCell->setAXColIndexFromRow(colIndex + index);
        index++;
    }
}

int AccessibilityTableRow::axColumnIndex() const
{
    int value = getIntegralAttribute(aria_colindexAttr);
    return value >= 1 ? value : -1;
}

int AccessibilityTableRow::axRowIndex() const
{
    int value = getIntegralAttribute(aria_rowindexAttr);
    return value >= 1 ? value : -1;
}
    
} // namespace WebCore
