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

class AccessibilityTableRow : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityTableRow> create(AXID, RenderObject&, AXObjectCache&, bool isARIAGridRow = false);
    static Ref<AccessibilityTableRow> create(AXID, Node&, AXObjectCache&, bool isARIAGridRow = false);
    virtual ~AccessibilityTableRow();

    AccessibilityObject* parentTable() const;

    void setRowIndex(unsigned);
    unsigned rowIndex() const override { return m_rowIndex; }

    // allows the table to add other children that may not originate
    // in the row, but their col/row spans overlap into it
    void appendChild(AccessibilityObject*);

    void addChildren() final;

    std::optional<unsigned> axColumnIndex() const final;
    std::optional<unsigned> axRowIndex() const final;
    String axRowIndexText() const final;
    // aria-colindextext is not allowed on rows

    AccessibilityChildrenVector disclosedRows() override;
    AccessibilityObject* disclosedByRow() const override;

protected:
    explicit AccessibilityTableRow(AXID, RenderObject&, AXObjectCache&, bool isARIAGridRow = false);
    explicit AccessibilityTableRow(AXID, Node&, AXObjectCache&, bool isARIAGridRow = false);

    AccessibilityRole determineAccessibilityRole() final;

private:
    // FIXME: This implementation of isTableRow() causes us to do an ancestry traversal every time is<AccessibilityTableRow>
    // is called. Can we replace this with a simpler check? And this function should then maybe be called isExposedTableRow()?
    bool isTableRow() const final;
    AccessibilityObject* observableObject() const final;
    bool computeIsIgnored() const final;

    bool isARIAGridRow() const final { return m_isARIAGridRow; }
    bool isARIATreeGridRow() const final;

    unsigned m_rowIndex;
    bool m_isARIAGridRow { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityTableRow, isTableRow())
