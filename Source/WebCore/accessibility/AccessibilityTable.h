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
#include <wtf/Forward.h>

namespace WebCore {

class AccessibilityTableRow;
class HTMLTableElement;

class AccessibilityTable : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityTable> create(AXID, RenderObject&);
    static Ref<AccessibilityTable> create(AXID, Node&);
    virtual ~AccessibilityTable();

    void init() final;

    virtual bool isAriaTable() const { return false; }

    void addChildren() final;
    void clearChildren() final;
    void updateChildrenRoles();

    AccessibilityChildrenVector columns() final;
    AccessibilityChildrenVector rows() final;

    unsigned columnCount() final;
    unsigned rowCount() final;

    String title() const final;

    // all the cells in the table
    AccessibilityChildrenVector cells() final;
    AccessibilityObject* cellForColumnAndRow(unsigned column, unsigned row) final;

    AccessibilityChildrenVector rowHeaders() final;
    AccessibilityChildrenVector visibleRows() final;

    // Returns an object that contains, as children, all the objects that act as headers.
    AccessibilityObject* headerContainer() final;

    bool isTable() const final { return true; }
    // Returns whether it is exposed as an AccessibilityTable to the platform.
    bool isExposable() const final { return m_isExposable; }
    void recomputeIsExposable();

    int axColumnCount() const final;
    int axRowCount() const final;

    // Cell indexes are assigned during child creation, so make sure children are up-to-date.
    void ensureCellIndexesUpToDate() { updateChildrenIfNecessary(); }
    Vector<Vector<Markable<AXID>>> cellSlots() final;
    void setCellSlotsDirty();

protected:
    explicit AccessibilityTable(AXID, RenderObject&);
    explicit AccessibilityTable(AXID, Node&);

    AccessibilityChildrenVector m_rows;
    AccessibilityChildrenVector m_columns;
    // 2D matrix of the cells assigned to each "slot" in this table.
    // ("Slot" as defined here: https://html.spec.whatwg.org/multipage/tables.html#concept-slots)
    Vector<Vector<Markable<AXID>>> m_cellSlots;

    RefPtr<AccessibilityObject> m_headerContainer;
    bool m_isExposable;

    // Used in type checking function is<AccessibilityTable>.
    bool isAccessibilityTableInstance() const final { return true; }

    bool computeIsIgnored() const final;

    void addRow(AccessibilityTableRow&, unsigned, unsigned& maxColumnCount);

private:
    AccessibilityRole determineAccessibilityRole() final;
    virtual bool computeIsTableExposableThroughAccessibility() const { return isDataTable(); }
    void labelText(Vector<AccessibilityText>&) const final;
    HTMLTableElement* tableElement() const;

    // Returns the number of columns the table should have.
    unsigned computeCellSlots();

    void ensureRow(unsigned);
    void ensureRowAndColumn(unsigned /* rowIndex */, unsigned /* columnIndex */);

    bool hasNonTableARIARole() const;
    // isDataTable is whether it is exposed as an AccessibilityTable because the heuristic
    // think this "looks" like a data-based table (instead of a table used for layout).
    bool isDataTable() const;
};

} // namespace WebCore 

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityTable, isAccessibilityTableInstance())
