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
#include "JSCustomSQLTransactionErrorCallback.h"

#if ENABLE(DATABASE)

#include "Frame.h"
#include "ScriptController.h"
#include "JSSQLError.h"
#include <runtime/JSLock.h>

namespace WebCore {
    
using namespace JSC;
    
JSCustomSQLTransactionErrorCallback::JSCustomSQLTransactionErrorCallback(JSObject* callback, JSDOMGlobalObject* globalObject)
    : m_callback(callback)
    , m_globalObject(globalObject)
{
}
    
void JSCustomSQLTransactionErrorCallback::handleEvent(SQLError* error)
{
    ASSERT(m_callback);
    ASSERT(m_globalObject);

    ExecState* exec = m_globalObject->globalExec();

    JSC::JSLock lock(SilenceAssertionsOnly);

    JSValue function = m_callback->get(exec, Identifier(exec, "handleEvent"));
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone) {
        callType = m_callback->getCallData(callData);
        if (callType == CallTypeNone) {
            // FIXME: Should an exception be thrown here?
            return;
        }
        function = m_callback;
    }

    RefPtr<JSCustomSQLTransactionErrorCallback> protect(this);

    MarkedArgumentBuffer args;
    args.append(toJS(exec, deprecatedGlobalObjectForPrototype(exec), error));

    m_globalObject->globalData()->timeoutChecker.start();
    call(exec, function, callType, callData, m_callback, args);
    m_globalObject->globalData()->timeoutChecker.stop();

    if (exec->hadException())
        reportCurrentException(exec);

    Document::updateStyleForAllDocuments();
}

}

#endif // ENABLE(DATABASE)
