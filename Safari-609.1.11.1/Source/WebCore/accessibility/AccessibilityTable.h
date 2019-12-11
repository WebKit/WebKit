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

class AccessibilityTableCell;
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
    
    const AccessibilityChildrenVector& columns();
    const AccessibilityChildrenVector& rows();
    
    virtual bool supportsSelectedRows() { return false; }
    unsigned columnCount();
    unsigned rowCount();
    int tableLevel() const final;
    
    String title() const final;
    
    // all the cells in the table
    void cells(AccessibilityChildrenVector&);
    AccessibilityTableCell* cellForColumnAndRow(unsigned column, unsigned row);
    
    void columnHeaders(AccessibilityChildrenVector&);
    void rowHeaders(AccessibilityChildrenVector&);
    void visibleRows(AccessibilityChildrenVector&);
    
    // an object that contains, as children, all the objects that act as headers
    AccessibilityObject* headerContainer();

    // isExposableThroughAccessibility() is whether it is exposed as an AccessibilityTable to the platform.
    bool isExposableThroughAccessibility() const;
    
    int axColumnCount() const;
    int axRowCount() const;

protected:
    explicit AccessibilityTable(RenderObject*);

    AccessibilityChildrenVector m_rows;
    AccessibilityChildrenVector m_columns;

    RefPtr<AccessibilityObject> m_headerContainer;
    bool m_isExposableThroughAccessibility;

    bool hasARIARole() const;

    // isTable is whether it's an AccessibilityTable object.
    bool isTable() const final { return true; }
    // isDataTable is whether it is exposed as an AccessibilityTable because the heuristic
    // think this "looks" like a data-based table (instead of a table used for layout).
    bool isDataTable() const final;
    bool computeAccessibilityIsIgnored() const final;

private:
    virtual bool computeIsTableExposableThroughAccessibility() const;
    void titleElementText(Vector<AccessibilityText>&) const final;
    HTMLTableElement* tableElement() const;
    void addChildrenFromSection(RenderTableSection*, unsigned& maxColumnCount);
    void addTableCellChild(AccessibilityObject*, HashSet<AccessibilityObject*>& appendedRows, unsigned& columnCount);
};

} // namespace WebCore 

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityTable, isTable())
