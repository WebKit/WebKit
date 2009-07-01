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

#include "config.h"

#if ENABLE(DATAGRID)

#include "AtomicString.h"
#include "DataGridColumnList.h"
#include "HTMLDataGridElement.h"
#include "PlatformString.h"
#include "RenderObject.h"

namespace WebCore {

DataGridColumnList::DataGridColumnList(HTMLDataGridElement* dataGrid)
    : m_dataGrid(dataGrid)
{
}

DataGridColumnList::~DataGridColumnList()
{
    clear();
}

DataGridColumn* DataGridColumnList::itemWithName(const AtomicString& name) const
{
    unsigned length = m_columns.size();
    for (unsigned i = 0; i < length; ++i) {
        if (m_columns[i]->id() == name)
            return m_columns[i].get();
    }
    return 0;
}

void DataGridColumnList::setDataGridNeedsLayout()
{
    // Mark the datagrid as needing layout.
    if (dataGrid() && dataGrid()->renderer()) 
        dataGrid()->renderer()->setNeedsLayout(true);
}

DataGridColumn* DataGridColumnList::add(const String& id, const String& label, const String& type, bool primary, unsigned short sortable)
{
    return add(DataGridColumn::create(id, label, type, primary, sortable).get());
}

DataGridColumn* DataGridColumnList::add(DataGridColumn* column)
{
    if (column->primary())
        m_primaryColumn = column;
    m_columns.append(column);
    column->setColumnList(this);
    setDataGridNeedsLayout();
    return column;
}

void DataGridColumnList::remove(DataGridColumn* col)
{
    size_t index = m_columns.find(col);
    if (index == notFound)
        return;
    m_columns.remove(index);
    if (col == m_primaryColumn)
        m_primaryColumn = 0;
    if (col == m_sortColumn)
        m_sortColumn = 0;
    col->setColumnList(0);
    setDataGridNeedsLayout();
}

void DataGridColumnList::move(DataGridColumn* col, unsigned long index)
{
    size_t colIndex = m_columns.find(col);
    if (colIndex == notFound)
        return;
    m_columns.insert(index, col);
    setDataGridNeedsLayout();
}

void DataGridColumnList::clear()
{
    unsigned length = m_columns.size();
    for (unsigned i = 0; i < length; ++i)
        m_columns[i]->setColumnList(0);
    m_columns.clear();
    m_primaryColumn = 0;
    m_sortColumn = 0;
    setDataGridNeedsLayout();
}

void DataGridColumnList::primaryColumnChanged(DataGridColumn* col)
{
    if (col->primary())
        m_primaryColumn = col;
    else if (m_primaryColumn = col)
        m_primaryColumn = 0;
    
    setDataGridNeedsLayout();
}

} // namespace WebCore

#endif
