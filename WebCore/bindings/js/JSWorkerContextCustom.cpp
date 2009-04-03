/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#if ENABLE(WORKERS)

#include "JSWorkerContext.h"

#include "JSDOMBinding.h"
#include "JSEventListener.h"
#include "ScheduledAction.h"
#include "WorkerContext.h"
#include <interpreter/Interpreter.h>

using namespace JSC;

namespace WebCore {

bool JSWorkerContext::customGetOwnPropertySlot(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertySlot& slot)
{
    // Look for overrides before looking at any of our own properties.
    if (JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot))
        return true;
    return false;
}

void JSWorkerContext::mark()
{
    Base::mark();

    markActiveObjectsForContext(*globalData(), scriptExecutionContext());

    markIfNotNull(impl()->onmessage());

    typedef WorkerContext::EventListenersMap EventListenersMap;
    typedef WorkerContext::ListenerVector ListenerVector;
    EventListenersMap& eventListeners = impl()->eventListeners();
    for (EventListenersMap::iterator mapIter = eventListeners.begin(); mapIter != eventListeners.end(); ++mapIter) {
        for (ListenerVector::iterator vecIter = mapIter->second.begin(); vecIter != mapIter->second.end(); ++vecIter)
            (*vecIter)->mark();
    }
}

JSValuePtr JSWorkerContext::self(ExecState*) const
{
    return JSValuePtr(this);
}

void JSWorkerContext::setSelf(ExecState* exec, JSValuePtr value)
{
    putDirect(Identifier(exec, "self"), value);
}

JSValuePtr JSWorkerContext::importScripts(ExecState* exec, const ArgList& args)
{
    if (!args.size())
        return jsUndefined();

    Vector<String> urls;
    for (unsigned i = 0; i < args.size(); i++) {
        urls.append(args.at(exec, i).toString(exec));
        if (exec->hadException())
            return jsUndefined();
    }
    ExceptionCode ec = 0;
    int signedLineNumber;
    intptr_t sourceID;
    UString sourceURL;
    JSValuePtr function;
    exec->interpreter()->retrieveLastCaller(exec, signedLineNumber, sourceID, sourceURL, function);

    impl()->importScripts(urls, sourceURL, signedLineNumber >= 0 ? signedLineNumber : 0, ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValuePtr JSWorkerContext::addEventListener(ExecState* exec, const ArgList& args)
{
    RefPtr<JSEventListener> listener = findOrCreateJSEventListener(args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args.at(exec, 0).toString(exec), listener.release(), args.at(exec, 2).toBoolean(exec));
    return jsUndefined();
}

JSValuePtr JSWorkerContext::removeEventListener(ExecState* exec, const ArgList& args)
{
    JSEventListener* listener = findJSEventListener(args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args.at(exec, 0).toString(exec), listener, args.at(exec, 2).toBoolean(exec));
    return jsUndefined();
}

static JSValuePtr setTimeoutOrInterval(ExecState* exec, WorkerContext* workerContext, const ArgList& args, bool singleShot)
{
    JSValuePtr v = args.at(exec, 0);
    int delay = args.at(exec, 1).toInt32(exec);
    if (v.isString())
        return jsNumber(exec, workerContext->installTimeout(new ScheduledAction(asString(v)->value()), delay, singleShot));
    CallData callData;
    if (v.getCallData(callData) == CallTypeNone)
        return jsUndefined();
    ArgList argsTail;
    args.getSlice(2, argsTail);
    return jsNumber(exec, workerContext->installTimeout(new ScheduledAction(exec, v, argsTail), delay, singleShot));
}

JSValuePtr JSWorkerContext::setTimeout(ExecState* exec, const ArgList& args)
{
    return setTimeoutOrInterval(exec, impl(), args, true);
}

JSValuePtr JSWorkerContext::clearTimeout(ExecState* exec, const ArgList& args)
{
    impl()->removeTimeout(args.at(exec, 0).toInt32(exec));
    return jsUndefined();
}

JSValuePtr JSWorkerContext::setInterval(ExecState* exec, const ArgList& args)
{
    return setTimeoutOrInterval(exec, impl(), args, false);
}

JSValuePtr JSWorkerContext::clearInterval(ExecState* exec, const ArgList& args)
{
    impl()->removeTimeout(args.at(exec, 0).toInt32(exec));
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
