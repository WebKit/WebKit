/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AXCoreObject.h"
#include "AccessibilityObject.h"
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXObjectRareData);
class AXObjectRareData : public CanMakeCheckedPtr<AXObjectRareData> {
    WTF_MAKE_NONCOPYABLE(AXObjectRareData);
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(AXObjectRareData);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AXObjectRareData);
public:
    AXObjectRareData() = default;

    // Begin table-related methods.
    const AXCoreObject::AccessibilityChildrenVector& tableRows() const { return m_tableRows; }
    const AXCoreObject::AccessibilityChildrenVector& tableColumns() const { return m_tableColumns; }
    unsigned rowCount() const { return m_tableRows.size(); }
    unsigned columnCount() const { return m_tableColumns.size(); }
    void appendColumn(AccessibilityObject& columnObject) { m_tableColumns.append(columnObject); }
    void appendRow(AccessibilityObject& rowObject) { m_tableRows.append(rowObject); }
    bool isExposableTable() const { return m_isExposableTable; }
    void setIsExposableTable(bool newValue) { m_isExposableTable = newValue; }
    AccessibilityObject* tableHeaderContainer() const { return m_tableHeaderContainer.get(); }
    void setTableHeaderContainer(AccessibilityObject& object) { m_tableHeaderContainer = object; }

    const Vector<Vector<Markable<AXID>>>& cellSlots() const { return m_cellSlots; }
    Vector<Vector<Markable<AXID>>>& mutableCellSlots() { return m_cellSlots; }

    void resetChildrenDependentTableFields();
    // End table-related methods.

private:
    // Begin table-related fields.
    AXCoreObject::AccessibilityChildrenVector m_tableRows;
    AXCoreObject::AccessibilityChildrenVector m_tableColumns;
    // 2D matrix of the cells assigned to each "slot" in the table.
    // ("Slot" as defined here: https://html.spec.whatwg.org/multipage/tables.html#concept-slots)
    Vector<Vector<Markable<AXID>>> m_cellSlots;

    RefPtr<AccessibilityObject> m_tableHeaderContainer;
    bool m_isExposableTable { false };
    // End table-related fields.

}; // class AXObjectRareData

} // namespace WebCore
