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

#ifndef HTMLDataGridElement_h
#define HTMLDataGridElement_h

#if ENABLE(DATAGRID)

#include "DataGridColumnList.h"
#include "DataGridDataSource.h"
#include "HTMLElement.h"
#include "Timer.h"

namespace WebCore {

class HTMLDataGridElement : public HTMLElement {
public:
    HTMLDataGridElement(const QualifiedName&, Document*);
    virtual ~HTMLDataGridElement();

    virtual int tagPriority() const { return 6; } // Same as <select>s
    virtual bool checkDTD(const Node*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    bool autofocus() const;
    void setAutofocus(bool);

    bool disabled() const;
    void setDisabled(bool);

    bool multiple() const;
    void setMultiple(bool);

    void setDataSource(PassRefPtr<DataGridDataSource>);
    DataGridDataSource* dataSource() const;

    DataGridColumnList* columns() const { return m_columns.get(); }

private:
    RefPtr<DataGridDataSource> m_dataSource;
    RefPtr<DataGridColumnList> m_columns;
};

} // namespace WebCore

#endif

#endif // HTMLDataGridElement_h
