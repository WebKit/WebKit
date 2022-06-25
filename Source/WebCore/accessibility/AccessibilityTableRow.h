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

class AccessibilityTableRow : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityTableRow> create(RenderObject*);
    virtual ~AccessibilityTableRow();

    // retrieves the "row" header (a th tag in the rightmost column)
    virtual AXCoreObject* headerObject();
    virtual AccessibilityTable* parentTable() const;

    void setRowIndex(unsigned rowIndex) { m_rowIndex = rowIndex; }
    unsigned rowIndex() const override { return m_rowIndex; }

    // allows the table to add other children that may not originate
    // in the row, but their col/row spans overlap into it
    void appendChild(AccessibilityObject*);
    
    void addChildren() override;

    int axColumnIndex() const override;
    int axRowIndex() const override;

protected:
    explicit AccessibilityTableRow(RenderObject*);

    AccessibilityRole determineAccessibilityRole() final;

private:
    bool isTableRow() const final;
    AccessibilityObject* observableObject() const final;
    bool computeAccessibilityIsIgnored() const final;

    unsigned m_rowIndex;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityTableRow, isTableRow())
