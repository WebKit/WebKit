/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "config.h"

#include "VoidCallback.h"

#include "CString.h"
#include "Frame.h"
#include "Page.h"
#include "kjs_proxy.h"
#include "kjs_window.h"

using namespace WebCore;
using namespace KJS;

VoidCallback::VoidCallback(JSValue* func)
{
    m_func = func;
}

VoidCallback::~VoidCallback()
{
}

void VoidCallback::handleEvent()
{
}

void VoidCallback::execute(Frame* frame)
{
    if (!frame)
        return;
    
    if (!m_func)
        return;
    
    KJS::JSObject* window = Window::retrieveWindow(frame);
    if (!window)
        return;
    
    KJSProxy* scriptProxy = frame->scriptProxy();
    if (!scriptProxy)
        return;
    
    RefPtr<ScriptInterpreter> interpreter = scriptProxy->interpreter();
    
    JSLock lock;
    ExecState* exec = interpreter->globalExec();
    ASSERT(window == interpreter->globalObject());
    interpreter->startTimeoutCheck();
    static_cast<JSObject*>(m_func)->call(exec, window, List());
    interpreter->stopTimeoutCheck();
    if (exec->hadException()) {
        JSObject* exception = exec->exception()->toObject(exec);
        exec->clearException();
        String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
        int lineNumber = exception->get(exec, "line")->toInt32(exec);
        if (Interpreter::shouldPrintExceptions())
            printf("(VoidCallback):%s\n", message.utf8().data());
        if (Page* page = frame->page())
            page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, lineNumber, String());
    }
}

bool VoidCallback::operator==(const VoidCallback& o) const
{
    return m_func == o.m_func;
}

VoidCallback* WebCore::toVoidCallback(JSValue* func)
{
    if (!func->isObject() || !static_cast<JSObject*>(func)->implementsCall())
        return new VoidCallback(0);
    return new VoidCallback(func);
}

