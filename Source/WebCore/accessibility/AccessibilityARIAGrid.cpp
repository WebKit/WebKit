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
#include "AccessibilityARIAGrid.h"

#include "AXObjectCache.h"
#include "AccessibilityARIAGridRow.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableHeaderContainer.h"
#include "RenderObject.h"
#include "RenderTableSection.h"

namespace WebCore {

AccessibilityARIAGrid::AccessibilityARIAGrid(RenderObject* renderer)
    : AccessibilityTable(renderer)
{
}

AccessibilityARIAGrid::~AccessibilityARIAGrid() = default;

Ref<AccessibilityARIAGrid> AccessibilityARIAGrid::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityARIAGrid(renderer));
}

bool AccessibilityARIAGrid::addTableCellChild(AXCoreObject* child, HashSet<AccessibilityObject*>& appendedRows, unsigned& columnCount)
{
    if (!child || (!is<AccessibilityTableRow>(*child) && !is<AccessibilityARIAGridRow>(*child)))
        return false;
        
    auto& row = downcast<AccessibilityTableRow>(*child);
    if (appendedRows.contains(&row))
        return false;
        
    // store the maximum number of columns
    unsigned rowCellCount = row.children().size();
    if (rowCellCount > columnCount)
        columnCount = rowCellCount;
    
    row.setRowIndex((int)m_rows.size());
    m_rows.append(&row);

    // Try adding the row if it's not ignoring accessibility,
    // otherwise add its children (the cells) as the grid's children.
    if (!row.accessibilityIsIgnored())
        m_children.append(&row);
    else
        m_children.appendVector(row.children());

    appendedRows.add(&row);
    return true;
}

bool AccessibilityARIAGrid::isMultiSelectable() const
{
    const AtomString& ariaMultiSelectable = getAttribute(HTMLNames::aria_multiselectableAttr);
    return !equalLettersIgnoringASCIICase(ariaMultiSelectable, "false");
}

void AccessibilityARIAGrid::addRowDescendant(AXCoreObject* rowChild, HashSet<AccessibilityObject*>& appendedRows, unsigned& columnCount)
{
    if (!rowChild)
        return;

    if (!rowChild->isTableRow() || !rowChild->node()) {
        // Although a "grid" should have rows as its direct descendants, if this is not a table row,
        // or this row is anonymous, dive deeper into the descendants to try to find a valid row.
        for (const auto& child : rowChild->children())
            addRowDescendant(child.get(), appendedRows, columnCount);
    } else
        addTableCellChild(rowChild, appendedRows, columnCount);
}

void AccessibilityARIAGrid::addChildren()
{
    ASSERT(!m_haveChildren); 
    
    if (!isExposable()) {
        AccessibilityRenderObject::addChildren();
        return;
    }
    
    m_haveChildren = true;
    if (!m_renderer)
        return;
    
    AXObjectCache* axCache = m_renderer->document().axObjectCache();
    
    // Add the children rows but be mindful in case there are footer sections in this table.
    HashSet<AccessibilityObject*> appendedRows;
    unsigned columnCount = 0;
    AccessibilityChildrenVector footerSections;
    for (RefPtr<AccessibilityObject> child = firstChild(); child; child = child->nextSibling()) {
        bool footerSection = false;
        if (RenderObject* childRenderer = child->renderer()) {
            if (is<RenderTableSection>(*childRenderer)) {
                RenderTableSection& childSection = downcast<RenderTableSection>(*childRenderer);
                if (&childSection == childSection.table()->footer()) {
                    footerSections.append(child);
                    footerSection = true;
                }
            }
        }
        if (!footerSection)
            addRowDescendant(child.get(), appendedRows, columnCount);
    }
    
    for (const auto& footerSection : footerSections)
        addRowDescendant(footerSection.get(), appendedRows, columnCount);
    
    // make the columns based on the number of columns in the first body
    for (unsigned i = 0; i < columnCount; ++i) {
        auto& column = downcast<AccessibilityTableColumn>(*axCache->create(AccessibilityRole::Column));
        column.setColumnIndex(i);
        column.setParent(this);
        m_columns.append(&column);
        if (!column.accessibilityIsIgnored())
            m_children.append(&column);
    }

    auto* headerContainerObject = headerContainer();
    if (headerContainerObject && !headerContainerObject->accessibilityIsIgnored())
        m_children.append(headerContainerObject);
}
    
} // namespace WebCore
