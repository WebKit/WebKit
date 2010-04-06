/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if ENABLE(WORKERS)

#include "JSWorkerContextErrorHandler.h"

#include "ErrorEvent.h"
#include "Event.h"
#include "JSEvent.h"
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

JSWorkerContextErrorHandler::JSWorkerContextErrorHandler(JSObject* function, JSObject* wrapper, bool isAttribute, DOMWrapperWorld* isolatedWorld)
    : JSEventListener(function, wrapper, isAttribute, isolatedWorld)
{
}

JSWorkerContextErrorHandler::~JSWorkerContextErrorHandler()
{
}

void JSWorkerContextErrorHandler::handleEvent(ScriptExecutionContext* scriptExecutionContext, Event* event)
{
    ASSERT(scriptExecutionContext);
    if (!scriptExecutionContext)
        return;

    JSLock lock(SilenceAssertionsOnly);

    JSObject* jsFunction = this->jsFunction(scriptExecutionContext);
    if (!jsFunction)
        return;

    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(scriptExecutionContext, isolatedWorld());
    if (!globalObject)
        return;

    ExecState* exec = globalObject->globalExec();

    CallData callData;
    CallType callType = jsFunction->getCallData(callData);

    if (callType != CallTypeNone) {

        ref();

        Event* savedEvent = globalObject->currentEvent();
        globalObject->setCurrentEvent(event);

        ASSERT(event->isErrorEvent());
        ErrorEvent* errorEvent = static_cast<ErrorEvent*>(event);

        MarkedArgumentBuffer args;
        args.append(jsString(exec, errorEvent->message()));
        args.append(jsString(exec, errorEvent->filename()));
        args.append(jsNumber(exec, errorEvent->lineno()));

        JSGlobalData* globalData = globalObject->globalData();
        DynamicGlobalObjectScope globalObjectScope(exec, globalData->dynamicGlobalObject ? globalData->dynamicGlobalObject : globalObject);    

        JSValue thisValue = globalObject->toThisObject(exec);

        globalData->timeoutChecker.start();
        JSValue returnValue = JSC::call(exec, jsFunction, callType, callData, thisValue, args);
        globalData->timeoutChecker.stop();

        globalObject->setCurrentEvent(savedEvent);

        if (exec->hadException())
            reportCurrentException(exec);
        else {
            bool retvalbool;
            if (returnValue.getBoolean(retvalbool) && !retvalbool)
                event->preventDefault();
        }

        deref();
    }
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
