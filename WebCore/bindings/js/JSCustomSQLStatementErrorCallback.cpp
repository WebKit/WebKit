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
#include "JSCustomSQLStatementErrorCallback.h"

#if ENABLE(DATABASE)

#include "Frame.h"
#include "ScriptController.h"
#include "JSSQLError.h"
#include "JSSQLTransaction.h"
#include <runtime/JSLock.h>

namespace WebCore {
    
using namespace JSC;
    
JSCustomSQLStatementErrorCallback::JSCustomSQLStatementErrorCallback(JSObject* callback, Frame* frame)
    : m_callback(callback)
    , m_frame(frame)
{
}
    
bool JSCustomSQLStatementErrorCallback::handleEvent(SQLTransaction* transaction, SQLError* error)
{
    ASSERT(m_callback);
    ASSERT(m_frame);
        
    if (!m_frame->script()->isEnabled())
        return true;
        
    JSGlobalObject* globalObject = m_frame->script()->globalObject();
    ExecState* exec = globalObject->globalExec();
        
    JSC::JSLock lock(false);
        
    JSValue handleEventFunction = m_callback->get(exec, Identifier(exec, "handleEvent"));
    CallData handleEventCallData;
    CallType handleEventCallType = handleEventFunction.getCallData(handleEventCallData);
    CallData callbackCallData;
    CallType callbackCallType = CallTypeNone;

    if (handleEventCallType == CallTypeNone) {
        callbackCallType = m_callback->getCallData(callbackCallData);
        if (callbackCallType == CallTypeNone) {
            // FIXME: Should an exception be thrown here?
            return true;
        }
    }
        
    RefPtr<JSCustomSQLStatementErrorCallback> protect(this);
        
    MarkedArgumentBuffer args;
    args.append(toJS(exec, transaction));
    args.append(toJS(exec, error));
        
    JSValue result;
    globalObject->globalData()->timeoutChecker.start();
    if (handleEventCallType != CallTypeNone)
        result = call(exec, handleEventFunction, handleEventCallType, handleEventCallData, m_callback, args);
    else
        result = call(exec, m_callback, callbackCallType, callbackCallData, m_callback, args);
    globalObject->globalData()->timeoutChecker.stop();
        
    if (exec->hadException()) {
        reportCurrentException(exec);
            
        // The spec says:
        // "If the error callback returns false, then move on to the next statement..."
        // "Otherwise, the error callback did not return false, or there was no error callback"
        // Therefore an exception and returning true are the same thing - so, return true on an exception
        return true;
    }
        
    Document::updateStyleForAllDocuments();

    return result.toBoolean(exec);
}

}

#endif // ENABLE(DATABASE)
