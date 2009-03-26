/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "JSDOMGlobalObject.h"

#include "Document.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"
#include "JSWorkerContext.h"
#include "WorkerContext.h"

using namespace JSC;

namespace WebCore {

JSDOMGlobalObject::JSDOMGlobalObjectData::JSDOMGlobalObjectData()
    : evt(0)
{
}

JSDOMGlobalObject::JSDOMGlobalObject(PassRefPtr<Structure> structure, JSDOMGlobalObject::JSDOMGlobalObjectData* data, JSObject* thisValue)
    : JSGlobalObject(structure, data, thisValue)
{
}

JSDOMGlobalObject::~JSDOMGlobalObject()
{
    // Clear any backpointers to the window
    ProtectedListenersMap::iterator i1 = d()->jsProtectedEventListeners.begin();
    ProtectedListenersMap::iterator e1 = d()->jsProtectedEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearGlobalObject();

    i1 = d()->jsProtectedInlineEventListeners.begin();
    e1 = d()->jsProtectedInlineEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearGlobalObject();

    JSListenersMap::iterator i2 = d()->jsEventListeners.begin();
    JSListenersMap::iterator e2 = d()->jsEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearGlobalObject();

    i2 = d()->jsInlineEventListeners.begin();
    e2 = d()->jsInlineEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearGlobalObject();
}

void JSDOMGlobalObject::mark()
{
    Base::mark();

    JSDOMStructureMap::iterator end = structures().end();
    for (JSDOMStructureMap::iterator it = structures().begin(); it != end; ++it)
        it->second->mark();

    JSDOMConstructorMap::iterator end2 = constructors().end();
    for (JSDOMConstructorMap::iterator it2 = constructors().begin(); it2 != end2; ++it2) {
        if (!it2->second->marked())
            it2->second->mark();
    }
}

JSProtectedEventListener* JSDOMGlobalObject::findJSProtectedEventListener(JSValuePtr val, bool isInline)
{
    if (!val.isObject())
        return 0;
    JSObject* object = asObject(val);
    ProtectedListenersMap& listeners = isInline ? d()->jsProtectedInlineEventListeners : d()->jsProtectedEventListeners;
    return listeners.get(object);
}

PassRefPtr<JSProtectedEventListener> JSDOMGlobalObject::findOrCreateJSProtectedEventListener(ExecState*, JSValuePtr val, bool isInline)
{
    if (JSProtectedEventListener* listener = findJSProtectedEventListener(val, isInline))
        return listener;

    if (!val.isObject())
        return 0;

    // The JSProtectedEventListener constructor adds it to our jsProtectedEventListeners map.
    return JSProtectedEventListener::create(asObject(val), this, isInline).get();
}

JSEventListener* JSDOMGlobalObject::findJSEventListener(ExecState*, JSValuePtr val, bool isInline)
{
    if (!val.isObject())
        return 0;

    JSListenersMap& listeners = isInline ? d()->jsInlineEventListeners : d()->jsEventListeners;
    return listeners.get(asObject(val));
}

PassRefPtr<JSEventListener> JSDOMGlobalObject::findOrCreateJSEventListener(ExecState* exec, JSValuePtr val, bool isInline)
{
    if (JSEventListener* listener = findJSEventListener(exec, val, isInline))
        return listener;

    if (!val.isObject())
        return 0;

    // The JSEventListener constructor adds it to our jsEventListeners map.
    return JSEventListener::create(asObject(val), this, isInline).get();
}

JSDOMGlobalObject::ProtectedListenersMap& JSDOMGlobalObject::jsProtectedEventListeners()
{
    return d()->jsProtectedEventListeners;
}

JSDOMGlobalObject::ProtectedListenersMap& JSDOMGlobalObject::jsProtectedInlineEventListeners()
{
    return d()->jsProtectedInlineEventListeners;
}

JSDOMGlobalObject::JSListenersMap& JSDOMGlobalObject::jsEventListeners()
{
    return d()->jsEventListeners;
}

JSDOMGlobalObject::JSListenersMap& JSDOMGlobalObject::jsInlineEventListeners()
{
    return d()->jsInlineEventListeners;
}

void JSDOMGlobalObject::setCurrentEvent(Event* evt)
{
    d()->evt = evt;
}

Event* JSDOMGlobalObject::currentEvent()
{
    return d()->evt;
}

JSDOMGlobalObject* toJSDOMGlobalObject(ScriptExecutionContext* scriptExecutionContext)
{
    if (scriptExecutionContext->isDocument())
        return toJSDOMWindow(static_cast<Document*>(scriptExecutionContext)->frame());

#if ENABLE(WORKERS)
    if (scriptExecutionContext->isWorkerContext())
        return static_cast<WorkerContext*>(scriptExecutionContext)->script()->workerContextWrapper();
#endif

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
