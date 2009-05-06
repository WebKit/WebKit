/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSCustomSQLTransactionCallback.h"

#if ENABLE(DATABASE)

#include "Frame.h"
#include "ScriptController.h"
#include "JSSQLTransaction.h"
#include "Page.h"
#include <runtime/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {
    
using namespace JSC;
    
#ifndef NDEBUG
static WTF::RefCountedLeakCounter counter("JSCustomSQLTransactionCallback");
#endif

// We have to clean up the data on the main thread for two reasons:
//
//     1) Can't deref a Frame on a non-main thread.
//     2) Unprotecting the JSObject on a non-main thread would register that thread
//        for JavaScript garbage collection, which could unnecessarily slow things down.

class JSCustomSQLTransactionCallback::Data {
public:
    Data(JSObject* callback, Frame* frame) : m_callback(callback), m_frame(frame) { }
    JSObject* callback() { return m_callback; }
    Frame* frame() { return m_frame.get(); }

private:
    ProtectedPtr<JSObject> m_callback;
    RefPtr<Frame> m_frame;
};

JSCustomSQLTransactionCallback::JSCustomSQLTransactionCallback(JSObject* callback, Frame* frame)
    : m_data(new Data(callback, frame))
{
#ifndef NDEBUG
    counter.increment();
#endif
}

void JSCustomSQLTransactionCallback::deleteData(void* context)
{
    delete static_cast<Data*>(context);
}

JSCustomSQLTransactionCallback::~JSCustomSQLTransactionCallback()
{
    callOnMainThread(deleteData, m_data);
#ifndef NDEBUG
    m_data = 0;
    counter.decrement();
#endif
}

void JSCustomSQLTransactionCallback::handleEvent(SQLTransaction* transaction, bool& raisedException)
{
    ASSERT(m_data);
    ASSERT(m_data->callback());
    ASSERT(m_data->frame());

    if (!m_data->frame()->script()->isEnabled())
        return;
        
    JSGlobalObject* globalObject = m_data->frame()->script()->globalObject();
    ExecState* exec = globalObject->globalExec();
        
    JSC::JSLock lock(false);
        
    JSValue handleEventFunction = m_data->callback()->get(exec, Identifier(exec, "handleEvent"));
    CallData handleEventCallData;
    CallType handleEventCallType = handleEventFunction.getCallData(handleEventCallData);
    CallData callbackCallData;
    CallType callbackCallType = CallTypeNone;

    if (handleEventCallType == CallTypeNone) {
        callbackCallType = m_data->callback()->getCallData(callbackCallData);
        if (callbackCallType == CallTypeNone) {
            // FIXME: Should an exception be thrown here?
            return;
        }
    }
        
    RefPtr<JSCustomSQLTransactionCallback> protect(this);
        
    MarkedArgumentBuffer args;
    args.append(toJS(exec, transaction));

    globalObject->globalData()->timeoutChecker.start();
    if (handleEventCallType != CallTypeNone)
        call(exec, handleEventFunction, handleEventCallType, handleEventCallData, m_data->callback(), args);
    else
        call(exec, m_data->callback(), callbackCallType, callbackCallData, m_data->callback(), args);
    globalObject->globalData()->timeoutChecker.stop();
        
    if (exec->hadException()) {
        reportCurrentException(exec);
        
        raisedException = true;
    }
        
    Document::updateStyleForAllDocuments();
}
    
}

#endif // ENABLE(DATABASE)
