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

#include "config.h"
#include "JSDataGridDataSource.h"

#include "Document.h"
#include "Frame.h"
#include "HTMLDataGridElement.h"
#include "JSHTMLDataGridElement.h"
#include "JSDOMWindowBase.h"
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

JSDataGridDataSource::JSDataGridDataSource(JSC::JSValue dataSource, Frame* frame)
    : m_dataSource(dataSource)
    , m_frame(frame)
{
}

JSDataGridDataSource::~JSDataGridDataSource()
{
}

void JSDataGridDataSource::initialize(HTMLDataGridElement* datagrid)
{
    if (!m_frame->script()->isEnabled())
        return;

    JSLock lock(false);
    RefPtr<JSDataGridDataSource> protect(this);

    ExecState* exec = m_frame->script()->globalObject()->globalExec();

    if (!jsDataSource().isObject())
        return;

    JSValue initializeFunction = jsDataSource().get(exec, Identifier(exec, "initialize"));
    CallData callData;
    CallType callType = initializeFunction.getCallData(callData);
    if (callType == CallTypeNone)
        return;

    MarkedArgumentBuffer args;
    args.append(toJS(exec, datagrid));

    JSDOMWindowBase::commonJSGlobalData()->timeoutChecker.start();
    call(exec, initializeFunction, callType, callData, m_dataSource, args);
    JSDOMWindowBase::commonJSGlobalData()->timeoutChecker.stop();

    if (exec->hadException())
        reportCurrentException(exec);
}

} // namespace WebCore
