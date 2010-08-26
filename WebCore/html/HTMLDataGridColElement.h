/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTMLDataGridColElement_h
#define HTMLDataGridColElement_h

#if ENABLE(DATAGRID)

#include "DataGridColumn.h"
#include "HTMLElement.h"

namespace WebCore {

class HTMLDataGridElement;

class HTMLDataGridColElement : public HTMLElement {
public:
    static PassRefPtr<HTMLDataGridColElement> create(const QualifiedName&, Document*);

    String label() const;
    void setLabel(const String&);
    
    String type() const;
    void setType(const String&);
    
    unsigned short sortable() const;
    void setSortable(unsigned short);
    
    unsigned short sortDirection() const;
    void setSortDirection(unsigned short);
    
    bool primary() const;
    void setPrimary(bool);
    
    DataGridColumn* column() const { return m_column.get(); }
    void setColumn(PassRefPtr<DataGridColumn> col) { m_column = col; }

private:
    HTMLDataGridColElement(const QualifiedName&, Document*);

    virtual void insertedIntoTree(bool /*deep*/);
    virtual void removedFromTree(bool /*deep*/);
    virtual void parseMappedAttribute(Attribute*);

    HTMLDataGridElement* dataGrid() const { return m_dataGrid; }
    HTMLDataGridElement* findDataGridAncestor() const;
    void ensureColumn();

    RefPtr<DataGridColumn> m_column;
    HTMLDataGridElement* m_dataGrid; // Not refcounted. We will null out our reference if we get removed from the grid.
};

} // namespace WebCore

#endif

#endif // HTMLDataGridColElement_h
