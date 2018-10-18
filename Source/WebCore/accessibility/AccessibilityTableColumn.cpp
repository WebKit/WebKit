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
#include "AccessibilityTableColumn.h"

#include "AXObjectCache.h"
#include "AccessibilityTableCell.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableSection.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityTableColumn::AccessibilityTableColumn() = default;

AccessibilityTableColumn::~AccessibilityTableColumn() = default;

Ref<AccessibilityTableColumn> AccessibilityTableColumn::create()
{
    return adoptRef(*new AccessibilityTableColumn());
}

void AccessibilityTableColumn::setParent(AccessibilityObject* parent)
{
    AccessibilityMockObject::setParent(parent);
    
    clearChildren();
}
    
LayoutRect AccessibilityTableColumn::elementRect() const
{
    // This used to be cached during the call to addChildren(), but calling elementRect()
    // can invalidate elements, so its better to ask for this on demand.
    LayoutRect columnRect;
    AccessibilityChildrenVector childrenCopy = m_children;
    for (const auto& cell : childrenCopy)
        columnRect.unite(cell->elementRect());

    return columnRect;
}

AccessibilityObject* AccessibilityTableColumn::headerObject()
{
    if (!m_parent)
        return nullptr;
    
    RenderObject* renderer = m_parent->renderer();
    if (!renderer)
        return nullptr;
    if (!is<AccessibilityTable>(*m_parent))
        return nullptr;

    auto& parentTable = downcast<AccessibilityTable>(*m_parent);
    if (!parentTable.isExposableThroughAccessibility())
        return nullptr;
    
    if (parentTable.isAriaTable()) {
        for (const auto& cell : children()) {
            if (cell->ariaRoleAttribute() == AccessibilityRole::ColumnHeader)
                return cell.get();
        }
        
        return nullptr;
    }

    if (!is<RenderTable>(*renderer))
        return nullptr;
    
    RenderTable& table = downcast<RenderTable>(*renderer);

    // try the <thead> section first. this doesn't require th tags
    if (auto* headerObject = headerObjectForSection(table.header(), false))
        return headerObject;
    
    RenderTableSection* bodySection = table.firstBody();
    while (bodySection && bodySection->isAnonymous())
        bodySection = table.sectionBelow(bodySection, SkipEmptySections);
    
    // now try for <th> tags in the first body. If the first body is 
    return headerObjectForSection(bodySection, true);
}

AccessibilityObject* AccessibilityTableColumn::headerObjectForSection(RenderTableSection* section, bool thTagRequired)
{
    if (!section)
        return nullptr;
    
    unsigned numCols = section->numColumns();
    if (m_columnIndex >= numCols)
        return nullptr;
    
    if (!section->numRows())
        return nullptr;
    
    RenderTableCell* cell = nullptr;
    // also account for cells that have a span
    for (int testCol = m_columnIndex; testCol >= 0; --testCol) {
        
        // Run down the rows in case initial rows are invalid (like when a <caption> is used).
        unsigned rowCount = section->numRows();
        for (unsigned testRow = 0; testRow < rowCount; testRow++) {
            RenderTableCell* testCell = section->primaryCellAt(testRow, testCol);
            // No cell at this index, keep checking more rows and columns.
            if (!testCell)
                continue;
            
            // If we've reached a cell that doesn't even overlap our column it can't be the header.
            if ((testCell->col() + (testCell->colSpan()-1)) < m_columnIndex)
                break;
            
            Node* testCellNode = testCell->element();
            // If the RenderTableCell doesn't have an element because its anonymous,
            // try to see if we can find the original cell element to check if it has a <th> tag.
            if (!testCellNode && testCell->isAnonymous()) {
                if (Element* parentElement = testCell->parent() ? testCell->parent()->element() : nullptr) {
                    if (parentElement->hasTagName(trTag) && testCol < static_cast<int>(parentElement->countChildNodes()))
                        testCellNode = parentElement->traverseToChildAt(testCol);
                }
            }

            if (!testCellNode)
                continue;
            
            // If th is required, but we found an element that doesn't have a th tag, we can stop looking.
            if (thTagRequired && !testCellNode->hasTagName(thTag))
                break;
            
            cell = testCell;
            break;
        }
    }
    
    if (!cell)
        return nullptr;

    auto* cellObject = axObjectCache()->getOrCreate(cell);
    if (!cellObject || cellObject->accessibilityIsIgnored())
        return nullptr;
        
    return cellObject;
}
    
bool AccessibilityTableColumn::computeAccessibilityIsIgnored() const
{
    if (!m_parent)
        return true;
    
#if PLATFORM(IOS_FAMILY) || PLATFORM(GTK)
    return true;
#endif
    
    return m_parent->accessibilityIsIgnored();
}
    
void AccessibilityTableColumn::addChildren()
{
    ASSERT(!m_haveChildren); 
    
    m_haveChildren = true;
    if (!is<AccessibilityTable>(m_parent))
        return;

    auto& parentTable = downcast<AccessibilityTable>(*m_parent);
    if (!parentTable.isExposableThroughAccessibility())
        return;
    
    int numRows = parentTable.rowCount();
    
    for (int i = 0; i < numRows; ++i) {
        AccessibilityTableCell* cell = parentTable.cellForColumnAndRow(m_columnIndex, i);
        if (!cell)
            continue;
        
        // make sure the last one isn't the same as this one (rowspan cells)
        if (m_children.size() > 0 && m_children.last() == cell)
            continue;
            
        m_children.append(cell);
    }
}
    
} // namespace WebCore
