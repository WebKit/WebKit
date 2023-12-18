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

#pragma once

#include "AccessibilityRenderObject.h"

namespace WebCore {
    
class AccessibilityTable;
class AccessibilityTableRow;

class AccessibilityTableCell : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityTableCell> create(RenderObject*);
    static Ref<AccessibilityTableCell> create(Node&);
    virtual ~AccessibilityTableCell();
    bool isTableCell() const final { return true; }

    bool isExposedTableCell() const final;
    bool isTableHeaderCell() const;
    bool isColumnHeader() const override;
    bool isRowHeader() const override;

    AXID rowGroupAncestorID() const final;

    virtual AccessibilityTable* parentTable() const;

    // Returns the start location and row span of the cell.
    std::pair<unsigned, unsigned> rowIndexRange() const final;
    // Returns the start location and column span of the cell.
    std::pair<unsigned, unsigned> columnIndexRange() const final;

    AccessibilityChildrenVector columnHeaders() override;
    AccessibilityChildrenVector rowHeaders() override;

    int axColumnIndex() const override;
    int axRowIndex() const override;
    unsigned colSpan() const;
    unsigned rowSpan() const;
    void incrementEffectiveRowSpan() { ++m_effectiveRowSpan; }
    void resetEffectiveRowSpan() { m_effectiveRowSpan = 1; }
    void setAXColIndexFromRow(int index) { m_axColIndexFromRow = index; }

    void setRowIndex(unsigned index) { m_rowIndex = index; }
    void setColumnIndex(unsigned index) { m_columnIndex = index; }

#if USE(ATSPI)
    int axColumnSpan() const;
    int axRowSpan() const;
#endif

protected:
    explicit AccessibilityTableCell(RenderObject*);
    explicit AccessibilityTableCell(Node&);

    AccessibilityTableRow* parentRow() const;
    AccessibilityRole determineAccessibilityRole() final;
    AccessibilityObject* parentObjectUnignored() const override;

private:
    // If a table cell is not exposed as a table cell, a TH element can serve as its title UI element.
    AccessibilityObject* titleUIElement() const final;
    bool computeAccessibilityIsIgnored() const final;
    String expandedTextValue() const final;
    bool supportsExpandedTextValue() const final;
    AccessibilityTableRow* ariaOwnedByParent() const;
    void ensureIndexesUpToDate() const;

    unsigned m_rowIndex { 0 };
    unsigned m_columnIndex { 0 };
    int m_axColIndexFromRow { -1 };

    // How many rows does this cell actually span?
    // This differs from rowSpan(), which can be an author-specified number all the way up 65535 that doesn't actually
    // reflect how many rows the cell spans in the rendered table.
    // Default to 1, as the cell should span at least the row it starts in.
    unsigned m_effectiveRowSpan { 1 };
};

} // namespace WebCore 

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityTableCell, isTableCell())
