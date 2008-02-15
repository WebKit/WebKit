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

#include "CString.h"
#include "Frame.h"
#include "Logging.h"
#include "kjs_proxy.h"
#include "JSSQLTransaction.h"
#include "Page.h"

namespace WebCore {
    
using namespace KJS;
    
#ifndef NDEBUG

WTFLogChannel LogWebCoreSQLLeaks = { 0x00000000, "", WTFLogChannelOn };

struct JSCustomSQLTransactionCallbackCounter { 
    static int count; 
    ~JSCustomSQLTransactionCallbackCounter() 
    { 
        if (count)
            LOG(WebCoreSQLLeaks, "LEAK: %d JSCustomSQLTransactionCallback\n", count);
    }
};

int JSCustomSQLTransactionCallbackCounter::count = 0;
static JSCustomSQLTransactionCallbackCounter counter;

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
    ++JSCustomSQLTransactionCallbackCounter::count;
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
    --JSCustomSQLTransactionCallbackCounter::count;
#endif
}

void JSCustomSQLTransactionCallback::handleEvent(SQLTransaction* transaction, bool& raisedException)
{
    ASSERT(m_data);
    ASSERT(m_data->callback());
    ASSERT(m_data->frame());

    if (!m_data->frame()->scriptProxy()->isEnabled())
        return;
        
    JSGlobalObject* globalObject = m_data->frame()->scriptProxy()->globalObject();
    ExecState* exec = globalObject->globalExec();
        
    KJS::JSLock lock;
        
    JSValue* handleEventFuncValue = m_data->callback()->get(exec, "handleEvent");
    JSObject* handleEventFunc = 0;
    if (handleEventFuncValue->isObject()) {
        handleEventFunc = static_cast<JSObject*>(handleEventFuncValue);
        if (!handleEventFunc->implementsCall())
            handleEventFunc = 0;
    }
        
    if (!handleEventFunc && !m_data->callback()->implementsCall()) {
        // FIXME: Should an exception be thrown here?
        return;
    }
        
    RefPtr<JSCustomSQLTransactionCallback> protect(this);
        
    List args;
    args.append(toJS(exec, transaction));

    globalObject->startTimeoutCheck();
    if (handleEventFunc)
        handleEventFunc->call(exec, m_data->callback(), args);
    else
        m_data->callback()->call(exec, m_data->callback(), args);
    globalObject->stopTimeoutCheck();
        
    if (exec->hadException()) {
        JSObject* exception = exec->exception()->toObject(exec);
        String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
        int lineNumber = exception->get(exec, "line")->toInt32(exec);
        String sourceURL = exception->get(exec, "sourceURL")->toString(exec);
        if (Interpreter::shouldPrintExceptions())
            printf("SQLTransactionCallback: %s\n", message.utf8().data());
        if (Page* page = m_data->frame()->page())
            page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, lineNumber, sourceURL);
        exec->clearException();
        
        raisedException = true;
    }
        
    Document::updateDocumentsRendering();
}
    
}
