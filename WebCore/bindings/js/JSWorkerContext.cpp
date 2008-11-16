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

#if ENABLE(WORKERS)

#include "JSWorkerContext.h"

#include "Event.h"
#include "JSDOMBinding.h"
#include "JSEventListener.h"
#include "JSMessageChannelConstructor.h"
#include "JSMessageEvent.h"
#include "JSMessagePort.h"
#include "JSWorkerLocation.h"
#include "WorkerContext.h"
#include "WorkerLocation.h"

using namespace JSC;

namespace WebCore {

static JSValue* jsWorkerContextPrototypeFunctionPostMessage(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsWorkerContextPrototypeFunctionAddEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsWorkerContextPrototypeFunctionRemoveEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsWorkerContextPrototypeFunctionDispatchEvent(ExecState*, JSObject*, JSValue*, const ArgList&);
JSValue* jsWorkerContextLocation(ExecState*, const Identifier&, const PropertySlot&);
JSValue* jsWorkerContextOnmessage(ExecState*, const Identifier&, const PropertySlot&);
void setJSWorkerContextOnmessage(ExecState*, JSObject*, JSValue*);
void setJSWorkerContextMessageEvent(ExecState*, JSObject*, JSValue*);
JSValue* jsWorkerContextMessageEvent(ExecState*, const Identifier&, const PropertySlot&);
JSValue* jsWorkerContextWorkerLocation(ExecState*, const Identifier&, const PropertySlot&);
void setJSWorkerContextWorkerLocation(ExecState*, JSObject*, JSValue*);

}

/*
@begin JSWorkerContextPrototypeTable
  addEventListener               jsWorkerContextPrototypeFunctionAddEventListener              DontDelete|Function 3
  removeEventListener            jsWorkerContextPrototypeFunctionRemoveEventListener           DontDelete|Function 3
  dispatchEvent                  jsWorkerContextPrototypeFunctionDispatchEvent                 DontDelete|Function 2
  postMessage                    jsWorkerContextPrototypeFunctionPostMessage                  DontDelete|Function 2
@end
*/

/*
@begin JSWorkerContextTable
  location                      jsWorkerContextLocation            DontDelete|ReadOnly
  onmessage                     jsWorkerContextOnmessage                     DontDelete
  MessageEvent                  jsWorkerContextMessageEvent                  DontDelete
  WorkerLocation                jsWorkerContextWorkerLocation                DontDelete
@end
*/

#include "JSWorkerContext.lut.h"

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSWorkerContext)

static inline PassRefPtr<Structure> createJSWorkerContextStructure(JSGlobalData* globalData)
{
    return JSWorkerContext::createStructure(new (globalData) JSWorkerContextPrototype(JSWorkerContextPrototype::createStructure(jsNull())));
}

JSWorkerContext::JSWorkerContext(PassRefPtr<WorkerContext> impl)
    : JSDOMGlobalObject(createJSWorkerContextStructure(Heap::heap(this)->globalData()), new JSDOMGlobalObjectData, this)
    , m_impl(impl)
{
    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(globalExec(), "self"), this, DontDelete | ReadOnly),
    };
    
    addStaticGlobals(staticGlobals, sizeof(staticGlobals) / sizeof(GlobalPropertyInfo));

}

JSWorkerContext::~JSWorkerContext()
{
}

ScriptExecutionContext* JSWorkerContext::scriptExecutionContext() const
{
    return m_impl.get();
}

void JSWorkerContext::mark()
{
    Base::mark();

    markActiveObjectsForContext(*globalData(), scriptExecutionContext());

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onmessage()))
        listener->mark();

    typedef WorkerContext::EventListenersMap EventListenersMap;
    typedef WorkerContext::ListenerVector ListenerVector;
    EventListenersMap& eventListeners = m_impl->eventListeners();
    for (EventListenersMap::iterator mapIter = eventListeners.begin(); mapIter != eventListeners.end(); ++mapIter) {
        for (ListenerVector::iterator vecIter = mapIter->second.begin(); vecIter != mapIter->second.end(); ++vecIter) {
            JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(vecIter->get());
            listener->mark();
        }
    }
}

static const HashTable* getJSWorkerContextTable(ExecState* exec)
{
    return getHashTableForGlobalData(exec->globalData(), &JSWorkerContextTable);
}

const ClassInfo JSWorkerContext::s_info = { "WorkerContext", 0, 0, getJSWorkerContextTable };

JSObject* JSWorkerContext::createPrototype(ExecState* exec)
{
    return new (exec) JSWorkerContextPrototype(JSWorkerContextPrototype::createStructure(exec->lexicalGlobalObject()->objectPrototype()));
}

void JSWorkerContext::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    lookupPut<JSWorkerContext, Base>(exec, propertyName, value, getJSWorkerContextTable(exec), this, slot);
}

bool JSWorkerContext::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSWorkerContext, Base>(exec, getJSWorkerContextTable(exec), this, propertyName, slot);
}

JSValue* jsWorkerContextPrototypeFunctionPostMessage(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSWorkerContext::s_info))
        return throwError(exec, TypeError);
    JSWorkerContext* workerContext = static_cast<JSWorkerContext*>(asObject(thisValue));
    const UString& message = args.at(exec, 0)->toString(exec);

    workerContext->impl()->postMessage(message);
    return jsUndefined();
}

JSValue* jsWorkerContextPrototypeFunctionAddEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSWorkerContext::s_info))
        return throwError(exec, TypeError);
    JSWorkerContext* workerContext = static_cast<JSWorkerContext*>(asObject(thisValue));
    RefPtr<JSUnprotectedEventListener> listener = workerContext->findOrCreateJSUnprotectedEventListener(exec, args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    workerContext->impl()->addEventListener(args.at(exec, 0)->toString(exec), listener.release(), args.at(exec, 2)->toBoolean(exec));
    return jsUndefined();
}

JSValue* jsWorkerContextPrototypeFunctionRemoveEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSWorkerContext::s_info))
        return throwError(exec, TypeError);
    JSWorkerContext* workerContext = static_cast<JSWorkerContext*>(asObject(thisValue));
    JSUnprotectedEventListener* listener = workerContext->findJSUnprotectedEventListener(exec, args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    workerContext->impl()->removeEventListener(args.at(exec, 0)->toString(exec), listener, args.at(exec, 2)->toBoolean(exec));
    return jsUndefined();
}

JSValue* jsWorkerContextPrototypeFunctionDispatchEvent(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSWorkerContext::s_info))
        return throwError(exec, TypeError);
    JSWorkerContext* workerContext = static_cast<JSWorkerContext*>(asObject(thisValue));
    Event* evt = toEvent(args.at(exec, 0));

    ExceptionCode ec = 0;
    JSValue* result = jsBoolean(workerContext->impl()->dispatchEvent(evt, ec));
    setDOMException(exec, ec);

    return result;
}

JSValue* jsWorkerContextLocation(JSC::ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    WorkerContext* imp = static_cast<WorkerContext*>(static_cast<JSWorkerContext*>(asObject(slot.slotBase()))->impl());
    return toJS(exec, imp->location());
}

JSValue* jsWorkerContextOnmessage(JSC::ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    WorkerContext* imp = static_cast<WorkerContext*>(static_cast<JSWorkerContext*>(asObject(slot.slotBase()))->impl());
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(imp->onmessage())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void setJSWorkerContextOnmessage(ExecState* exec, JSObject* thisObject, JSValue* value)
{
    WorkerContext* imp = static_cast<WorkerContext*>(static_cast<JSWorkerContext*>(thisObject)->impl());
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(imp->scriptExecutionContext());
    if (!globalObject)
        return;
    imp->setOnmessage(globalObject->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* jsWorkerContextMessageEvent(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return JSMessageEvent::getConstructor(exec);
}

JSValue* jsWorkerContextWorkerLocation(ExecState* exec, const Identifier&, const PropertySlot&)
{
    return JSWorkerLocation::getConstructor(exec);
}

static const HashTable* getJSWorkerContextPrototypeTable(ExecState* exec)
{
    return getHashTableForGlobalData(exec->globalData(), &JSWorkerContextPrototypeTable);
}

const ClassInfo JSWorkerContextPrototype::s_info = { "WorkerContextPrototype", 0, 0, getJSWorkerContextPrototypeTable };

JSObject* JSWorkerContextPrototype::self(ExecState* exec)
{
    return getDOMPrototype<JSWorkerContext>(exec);
}

bool JSWorkerContextPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, getJSWorkerContextPrototypeTable(exec), this, propertyName, slot);
}

void setJSWorkerContextMessageEvent(ExecState*, JSObject*, JSValue*)
{
    // FIXME: Do we need to override put for global constructors, like JSDOMWindowBase does?
    ASSERT_NOT_REACHED();
}

void setJSWorkerContextWorkerLocation(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}


} // namespace WebCore

#endif // ENABLE(WORKERS)
