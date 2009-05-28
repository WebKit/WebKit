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

#if ENABLE(WORKERS)
#include "JSWorkerContext.h"
#include "WorkerContext.h"
#endif

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
    JSListenersMap::iterator it = d()->jsEventListeners.begin();
    JSListenersMap::iterator end = d()->jsEventListeners.end();
    for (; it != end; ++it)
        it->second->clearGlobalObject();
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

JSEventListener* JSDOMGlobalObject::findJSEventListener(JSValue val)
{
    if (!val.isObject())
        return 0;

    return d()->jsEventListeners.get(asObject(val));
}

PassRefPtr<JSEventListener> JSDOMGlobalObject::findOrCreateJSEventListener(JSValue val)
{
    if (JSEventListener* listener = findJSEventListener(val))
        return listener;

    if (!val.isObject())
        return 0;

    // The JSEventListener constructor adds it to our jsEventListeners map.
    return JSEventListener::create(asObject(val), this, false).get();
}

PassRefPtr<JSEventListener> JSDOMGlobalObject::createJSAttributeEventListener(JSValue val)
{
    if (!val.isObject())
        return 0;

    return JSEventListener::create(asObject(val), this, true).get();
}

JSDOMGlobalObject::JSListenersMap& JSDOMGlobalObject::jsEventListeners()
{
    return d()->jsEventListeners;
}

void JSDOMGlobalObject::setCurrentEvent(Event* evt)
{
    d()->evt = evt;
}

Event* JSDOMGlobalObject::currentEvent() const
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
