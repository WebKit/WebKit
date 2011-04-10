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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSDataGridDataSource_h
#define JSDataGridDataSource_h

#if ENABLE(DATAGRID)

#include "DataGridDataSource.h"
#include <heap/Strong.h>
#include <runtime/JSValue.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Frame;
class HTMLDataGridElement;

class JSDataGridDataSource : public DataGridDataSource {
public:
    static PassRefPtr<JSDataGridDataSource> create(JSC::JSValue dataSource, Frame* frame)
    {
        return adoptRef(new JSDataGridDataSource(dataSource, frame));
    }

    virtual ~JSDataGridDataSource();

    virtual bool isJSDataGridDataSource() const { return true; }
    JSC::JSValue jsDataSource() const { return m_dataSource.get(); }

private:
    JSDataGridDataSource(JSC::JSValue, Frame*);

    JSC::ProtectedJSValue m_dataSource;
    RefPtr<Frame> m_frame;
};

inline JSDataGridDataSource* asJSDataGridDataSource(DataGridDataSource* dataSource)
{
    ASSERT(dataSource->isJSDataGridDataSource());
    return static_cast<JSDataGridDataSource*>(dataSource);
}

inline const JSDataGridDataSource* asJSDataGridDataSource(const DataGridDataSource* dataSource)
{
    ASSERT(dataSource->isJSDataGridDataSource());
    return static_cast<const JSDataGridDataSource*>(dataSource);
}

} // namespace WebCore

#endif // ENABLE(DATAGRID)
#endif // JSDataGridDataSource_h
