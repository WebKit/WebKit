/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "JSErrorHandler.h"

#include "Document.h"
#include "ErrorEvent.h"
#include "Event.h"
#include "EventNames.h"
#include "JSEvent.h"
#include "JSMainThreadExecState.h"
#include <runtime/JSLock.h>
#include <runtime/VMEntryScope.h>
#include <wtf/Ref.h>

using namespace JSC;

namespace WebCore {

JSErrorHandler::JSErrorHandler(JSObject* function, JSObject* wrapper, bool isAttribute, DOMWrapperWorld& world)
    : JSEventListener(function, wrapper, isAttribute, world)
{
}

JSErrorHandler::~JSErrorHandler()
{
}

void JSErrorHandler::handleEvent(ScriptExecutionContext* scriptExecutionContext, Event* event)
{

    if (event->eventInterface() != ErrorEventInterfaceType)
        return JSEventListener::handleEvent(scriptExecutionContext, event);

    ASSERT(scriptExecutionContext);
    if (!scriptExecutionContext)
        return;

    ErrorEvent* errorEvent = toErrorEvent(event);

    JSLockHolder lock(scriptExecutionContext->vm());

    JSObject* jsFunction = this->jsFunction(scriptExecutionContext);
    if (!jsFunction)
        return;

    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(scriptExecutionContext, isolatedWorld());
    if (!globalObject)
        return;

    ExecState* exec = globalObject->globalExec();

    CallData callData;
    CallType callType = jsFunction->methodTable()->getCallData(jsFunction, callData);

    if (callType != CallTypeNone) {
        Ref<JSErrorHandler> protectedctor(*this);

        Event* savedEvent = globalObject->currentEvent();
        globalObject->setCurrentEvent(event);

        MarkedArgumentBuffer args;
        args.append(jsStringWithCache(exec, errorEvent->message()));
        args.append(jsStringWithCache(exec, errorEvent->filename()));
        args.append(jsNumber(errorEvent->lineno()));
        args.append(jsNumber(errorEvent->colno()));

        VM& vm = globalObject->vm();
        VMEntryScope entryScope(vm, vm.entryScope ? vm.entryScope->globalObject() : globalObject);

        JSValue returnValue = scriptExecutionContext->isDocument()
            ? JSMainThreadExecState::call(exec, jsFunction, callType, callData, globalObject, args)
            : JSC::call(exec, jsFunction, callType, callData, globalObject, args);

        globalObject->setCurrentEvent(savedEvent);

        if (exec->hadException())
            reportCurrentException(exec);
        else {
            if (returnValue.isTrue())
                event->preventDefault();
        }
    }
}

} // namespace WebCore
