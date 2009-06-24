/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef DataGridColumn_h
#define DataGridColumn_h

#include "AtomicString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DataGridColumnList;

class DataGridColumn : public RefCounted<DataGridColumn> {
public:
    static PassRefPtr<DataGridColumn> create(DataGridColumnList* columns, const String& columnID, const String& label, const String& type, bool primary, unsigned short sortable)
    {
        return new DataGridColumn(columns, columnID, label, type, primary, sortable);
    }

    const AtomicString& id() const { return m_id; }
    void setId(const AtomicString& id) { m_id = id; }

    const AtomicString& label() const { return m_label; }
    void setLabel(const AtomicString& label) { m_label = label; }
    
    const AtomicString& type() const { return m_type; }
    void setType(const AtomicString& type) { m_type = type; }
    
    unsigned short sortable() const { return m_sortable; }
    void setSortable(unsigned short sortable) { m_sortable = sortable; }
    
    unsigned short sortDirection() const { return m_sortDirection; }
    void setSortDirection(unsigned short sortDirection) { m_sortDirection = sortDirection; }
    
    bool primary() const { return m_primary; }
    void setPrimary(bool);

    void detachFromColumnList() { m_columns = 0; }

private:
    DataGridColumn(DataGridColumnList* columns, const String& columnID, const String& label, const String& type, bool primary, unsigned short sortable)
        : m_columns(columns)
        , m_id(columnID)
        , m_label(label)
        , m_type(type)
        , m_primary(primary)
        , m_sortable(sortable)
        , m_sortDirection(0)
    {
    }

    DataGridColumnList* m_columns; // Not refcounted.  The columns list will null out our reference when it goes away.

    AtomicString m_id;
    AtomicString m_label;
    AtomicString m_type;

    bool m_primary;

    unsigned short m_sortable;
    unsigned short m_sortDirection;
};

} // namespace WebCore

#endif // DataGridColumn_h
