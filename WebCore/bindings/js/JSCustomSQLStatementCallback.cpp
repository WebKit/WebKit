/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "config.h"
#include "JSCustomSQLStatementCallback.h"

#include "CString.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "ScriptController.h"
#include "JSSQLResultSet.h"
#include "JSSQLTransaction.h"

namespace WebCore {
    
using namespace KJS;
    
JSCustomSQLStatementCallback::JSCustomSQLStatementCallback(JSObject* callback, Frame* frame)
    : m_callback(callback)
    , m_frame(frame)
{
}
    
void JSCustomSQLStatementCallback::handleEvent(SQLTransaction* transaction, SQLResultSet* resultSet, bool& raisedException)
{
    ASSERT(m_callback);
    ASSERT(m_frame);
        
    if (!m_frame->script()->isEnabled())
        return;

    JSGlobalObject* globalObject = m_frame->script()->globalObject();
    ExecState* exec = globalObject->globalExec();
        
    KJS::JSLock lock;
        
    JSValue* function = m_callback->get(exec, Identifier(exec, "handleEvent"));
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone) {
        callType = m_callback->getCallData(callData);
        if (callType == CallTypeNone) {
            // FIXME: Should an exception be thrown here?
            return;
        }
        function = m_callback;
    }
        
    RefPtr<JSCustomSQLStatementCallback> protect(this);
        
    ArgList args;
    args.append(toJS(exec, transaction));
    args.append(toJS(exec, resultSet));
        
    globalObject->startTimeoutCheck();
    call(exec, function, callType, callData, m_callback, args);
    globalObject->stopTimeoutCheck();
        
    if (exec->hadException()) {
        JSObject* exception = exec->exception()->toObject(exec);
        String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
        int lineNumber = exception->get(exec, Identifier(exec, "line"))->toInt32(exec);
        String sourceURL = exception->get(exec, Identifier(exec, "sourceURL"))->toString(exec);
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, lineNumber, sourceURL);
        
        raisedException = true;
        
        exec->clearException();
    }
        
    Document::updateDocumentsRendering();
}
    
}
