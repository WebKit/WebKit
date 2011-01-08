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

#include "config.h"

#if ENABLE(DATAGRID)

#include "V8DataGridDataSource.h"

#include "Document.h"
#include "Frame.h"
#include "HTMLDataGridElement.h"
#include "V8HTMLDataGridElement.h"


namespace WebCore {

V8DataGridDataSource::V8DataGridDataSource(v8::Handle<v8::Value> dataSource, Frame* frame)
    : m_dataSource(v8::Persistent<v8::Value>::New(dataSource))
    , m_frame(frame)
{
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(DATASOURCE, this, m_dataSource);
#endif
}

V8DataGridDataSource::~V8DataGridDataSource()
{
#ifndef NDEBUG
    V8GCController::unregisterGlobalHandle(this, m_dataSource);
#endif
    m_dataSource.Dispose();
    m_dataSource.Clear();
}

} // namespace WebCore

#endif // ENABLE(DATAGRID)
