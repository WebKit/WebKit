/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef V8DataGridDataSource_h
#define V8DataGridDataSource_h

#if ENABLE(DATAGRID)

#include "DataGridDataSource.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Frame;
class HTMLDataGridElement;

class V8DataGridDataSource : public DataGridDataSource {
public:
    static PassRefPtr<V8DataGridDataSource> create(v8::Handle<v8::Value> dataSource, Frame* frame)
    {
        return adoptRef(new V8DataGridDataSource(dataSource, frame));
    }

    virtual ~V8DataGridDataSource();

    virtual bool isJSDataGridDataSource() const { return true; }
    v8::Handle<v8::Value> jsDataSource() const { return m_dataSource; }

private:
    V8DataGridDataSource(v8::Handle<v8::Value>, Frame*);

    v8::Persistent<v8::Value> m_dataSource;
    RefPtr<Frame> m_frame;
};

inline V8DataGridDataSource* asV8DataGridDataSource(DataGridDataSource* dataSource)
{
    ASSERT(dataSource->isJSDataGridDataSource());
    return static_cast<V8DataGridDataSource*>(dataSource);
}

inline const V8DataGridDataSource* asV8DataGridDataSource(const DataGridDataSource* dataSource)
{
    ASSERT(dataSource->isJSDataGridDataSource());
    return static_cast<const V8DataGridDataSource*>(dataSource);
}

} // namespace WebCore

#endif // ENABLE(DATAGRID)
#endif // V8DataGridDataSource_h
