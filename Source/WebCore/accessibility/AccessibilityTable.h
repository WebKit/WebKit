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
#include "AccessibilityTableCell.h"
#include <wtf/Forward.h>

namespace WebCore {

class HTMLTableElement;
class RenderTableSection;
    
class AccessibilityTable : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityTable> create(RenderObject*);
    virtual ~AccessibilityTable();

    void init() final;

    AccessibilityRole roleValue() const final;
    virtual bool isAriaTable() const { return false; }

    void addChildren() override;
    void clearChildren() final;
    void updateChildrenRoles();

    AccessibilityChildrenVector columns() override;
    AccessibilityChildrenVector rows() override;

    unsigned columnCount() override;
    unsigned rowCount() override;
    int tableLevel() const final;

    String title() const final;

    // all the cells in the table
    AccessibilityChildrenVector cells() override;
    AXCoreObject* cellForColumnAndRow(unsigned column, unsigned row) override;

    AccessibilityChildrenVector columnHeaders() override;
    AccessibilityChildrenVector rowHeaders() override;
    AccessibilityChildrenVector visibleRows() override;

    // Returns an object that contains, as children, all the objects that act as headers.
    AXCoreObject* headerContainer() override;

    bool isTable() const override { return true; }
    // Returns whether it is exposed as an AccessibilityTable to the platform.
    bool isExposable() const override;
    void recomputeIsExposable();

    int axColumnCount() const override;
    int axRowCount() const override;

protected:
    explicit AccessibilityTable(RenderObject*);

    AccessibilityChildrenVector m_rows;
    AccessibilityChildrenVector m_columns;

    RefPtr<AccessibilityObject> m_headerContainer;
    bool m_isExposable;

    // Used in type checking function is<AccessibilityTable>.
    bool isAccessibilityTableInstance() const final { return true; }

    bool computeAccessibilityIsIgnored() const final;

private:
    virtual bool computeIsTableExposableThroughAccessibility() const;
    void titleElementText(Vector<AccessibilityText>&) const final;
    HTMLTableElement* tableElement() const;
    void addChildrenFromSection(RenderTableSection*, unsigned& maxColumnCount);
    void addTableCellChild(AccessibilityObject*, HashSet<AccessibilityObject*>& appendedRows, unsigned& columnCount);

    bool hasNonTableARIARole() const;
    // isDataTable is whether it is exposed as an AccessibilityTable because the heuristic
    // think this "looks" like a data-based table (instead of a table used for layout).
    bool isDataTable() const;
};

} // namespace WebCore 

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityTable, isAccessibilityTableInstance())
