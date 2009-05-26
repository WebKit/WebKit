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
#include "AccessibilityARIAGrid.h"

#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableHeaderContainer.h"
#include "AccessibilityTableRow.h"
#include "AXObjectCache.h"
#include "RenderObject.h"

using namespace std;

namespace WebCore {

AccessibilityARIAGrid::AccessibilityARIAGrid(RenderObject* renderer)
    : AccessibilityTable(renderer)
{
#if ACCESSIBILITY_TABLES
    m_isAccessibilityTable = true;
#else
    m_isAccessibilityTable = false;
#endif
}

AccessibilityARIAGrid::~AccessibilityARIAGrid()
{
}

PassRefPtr<AccessibilityARIAGrid> AccessibilityARIAGrid::create(RenderObject* renderer)
{
    return adoptRef(new AccessibilityARIAGrid(renderer));
}

void AccessibilityARIAGrid::addChild(AccessibilityObject* child, HashSet<AccessibilityObject*>& appendedRows, unsigned& columnCount)
{
    if (!child || !child->isTableRow() || child->ariaRoleAttribute() != RowRole)
        return;
        
    AccessibilityTableRow* row = static_cast<AccessibilityTableRow*>(child);
    if (appendedRows.contains(row))
        return;
        
    // store the maximum number of columns
    unsigned rowCellCount = row->children().size();
    if (rowCellCount > columnCount)
        columnCount = rowCellCount;
    
    row->setRowIndex((int)m_rows.size());        
    m_rows.append(row);
    m_children.append(row);
    appendedRows.add(row);
}
    
void AccessibilityARIAGrid::addChildren()
{
    ASSERT(!m_haveChildren); 
    
    if (!isDataTable()) {
        AccessibilityRenderObject::addChildren();
        return;
    }
    
    m_haveChildren = true;
    if (!m_renderer)
        return;
    
    AXObjectCache* axCache = m_renderer->document()->axObjectCache();
    
    // add only rows that are labeled as aria rows
    HashSet<AccessibilityObject*> appendedRows;
    unsigned columnCount = 0;
    for (RefPtr<AccessibilityObject> child = firstChild(); child; child = child->nextSibling()) {

        // in case the render tree doesn't match the expected ARIA hierarchy, look at the children
        if (child->accessibilityIsIgnored()) {
            if (!child->hasChildren())
                child->addChildren();
            
            AccessibilityChildrenVector children = child->children();
            unsigned length = children.size();
            for (unsigned i = 0; i < length; ++i)
                addChild(children[i].get(), appendedRows, columnCount);
        } else
            addChild(child.get(), appendedRows, columnCount);            
    }
    
    // make the columns based on the number of columns in the first body
    for (unsigned i = 0; i < columnCount; ++i) {
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
    
AccessibilityTableCell* AccessibilityARIAGrid::cellForColumnAndRow(unsigned column, unsigned row)
{
    if (!m_renderer)
        return 0;
    
    if (!hasChildren())
        addChildren();
    
    if (column >= columnCount() || row >= rowCount())
        return 0;
    
    AccessibilityObject *tableRow = m_rows[row].get();
    if (!tableRow)
        return 0;
    
    AccessibilityChildrenVector children = tableRow->children();
    // in case this row had fewer columns than other rows
    AccessibilityObject* tableCell = 0;
    if (column >= children.size())
        return 0;

    tableCell = children[column].get();
    return static_cast<AccessibilityTableCell*>(tableCell);
}
    
} // namespace WebCore
