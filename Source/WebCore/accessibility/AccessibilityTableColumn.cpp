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

#include "AccessibilityTable.h"

namespace WebCore {

AccessibilityTableColumn::AccessibilityTableColumn(AXID axID)
    : AccessibilityMockObject(axID)
{
}

AccessibilityTableColumn::~AccessibilityTableColumn() = default;

Ref<AccessibilityTableColumn> AccessibilityTableColumn::create(AXID axID)
{
    return adoptRef(*new AccessibilityTableColumn(axID));
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
    auto childrenCopy = const_cast<AccessibilityTableColumn*>(this)->unignoredChildren(/* updateChildrenIfNeeded */ false);
    for (const auto& cell : childrenCopy)
        columnRect.unite(cell->elementRect());

    return columnRect;
}

AccessibilityObject* AccessibilityTableColumn::columnHeader()
{
    auto* parentTable = dynamicDowncast<AccessibilityTable>(m_parent.get());
    if (!parentTable || !parentTable->isExposable())
        return nullptr;

    for (const auto& cell : unignoredChildren()) {
        if (cell->roleValue() == AccessibilityRole::ColumnHeader)
            return &downcast<AccessibilityObject>(cell.get());
    }
    return nullptr;
}

void AccessibilityTableColumn::setColumnIndex(unsigned columnIndex)
{
    if (m_columnIndex == columnIndex)
        return;
    m_columnIndex = columnIndex;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto* cache = axObjectCache())
        cache->columnIndexChanged(*this);
#endif
}

bool AccessibilityTableColumn::computeIsIgnored() const
{
#if PLATFORM(IOS_FAMILY) || USE(ATSPI)
    return true;
#endif
    
    return !m_parent || m_parent->isIgnored();
}
    
void AccessibilityTableColumn::addChildren()
{
    ASSERT(!m_childrenInitialized); 
    m_childrenInitialized = true;

    RefPtr parentTable = dynamicDowncast<AccessibilityTable>(m_parent.get());
    if (!parentTable || !parentTable->isExposable())
        return;

    int numRows = parentTable->rowCount();
    for (int i = 0; i < numRows; ++i) {
        RefPtr cell = parentTable->cellForColumnAndRow(m_columnIndex, i);
        if (!cell)
            continue;

        // make sure the last one isn't the same as this one (rowspan cells)
        if (m_children.size() > 0 && m_children.last().ptr() == cell.get())
            continue;

        addChild(*cell);
    }
}
    
} // namespace WebCore
