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

#include "JSDOMBinding.h"
#include "JSMessageChannelConstructor.h"
#include "JSMessagePort.h"
#include "JSWorkerLocation.h"
#include "WorkerContext.h"
#include "WorkerLocation.h"

using namespace JSC;

namespace WebCore {

JSValue* jsWorkerContextLocation(ExecState*, const Identifier&, const PropertySlot&);
JSValue* jsWorkerContextMessageChannel(ExecState*, const Identifier&, const PropertySlot&);
void setJSWorkerContextMessageChannel(ExecState*, JSObject*, JSValue*);
JSValue* jsWorkerContextMessagePort(ExecState*, const Identifier&, const PropertySlot&);
void setJSWorkerContextMessagePort(ExecState*, JSObject*, JSValue*);
JSValue* jsWorkerContextWorkerLocation(ExecState*, const Identifier&, const PropertySlot&);
void setJSWorkerContextWorkerLocation(ExecState*, JSObject*, JSValue*);

}

/*
@begin JSWorkerContextPrototypeTable
#  close                          jsWorkerContextPrototypeFunctionClose                         DontDelete|Function 0
#  MessagePort methods?
@end
*/

/*
@begin JSWorkerContextTable
  location                      jsWorkerContextLocation            DontDelete|ReadOnly
#  onclose                       jsWorkerContextOnclose             DontDelete
#  onconnect                     jsWorkerContextOnconnect           DontDelete
#  onmessage and other MessagePort attributes?
  MessageChannel                jsWorkerContextMessageChannel                DontDelete
  MessagePort                   jsWorkerContextMessagePort                   DontDelete
  WorkerLocation                jsWorkerContextWorkerLocation                DontDelete
#  XMLHttpRequest                jsWorkerContextXMLHttpRequest                DontDelete
#  Database
@end
*/

#include "JSWorkerContext.lut.h"

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSWorkerContext)

static inline PassRefPtr<StructureID> createJSWorkerContextStructureID(JSGlobalData* globalData)
{
    return JSWorkerContext::createStructureID(new (globalData) JSWorkerContextPrototype(JSWorkerContextPrototype::createStructureID(jsNull())));
}

JSWorkerContext::JSWorkerContext(PassRefPtr<WorkerContext> impl)
    : JSDOMGlobalObject(createJSWorkerContextStructureID(Heap::heap(this)->globalData()), new JSDOMGlobalObjectData, this)
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
}

static const HashTable* getJSWorkerContextTable(ExecState* exec)
{
    return getHashTableForGlobalData(exec->globalData(), &JSWorkerContextTable);
}

const ClassInfo JSWorkerContext::s_info = { "WorkerContext", 0, 0, getJSWorkerContextTable };

JSObject* JSWorkerContext::createPrototype(ExecState* exec)
{
    return new (exec) JSWorkerContextPrototype(JSWorkerLocationPrototype::createStructureID(exec->lexicalGlobalObject()->objectPrototype()));
}

bool JSWorkerContext::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSWorkerContext, Base>(exec, getJSWorkerContextTable(exec), this, propertyName, slot);
}

JSValue* jsWorkerContextLocation(JSC::ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    WorkerContext* imp = static_cast<WorkerContext*>(static_cast<JSWorkerContext*>(asObject(slot.slotBase()))->impl());
    return toJS(exec, imp->location());
}

JSValue* jsWorkerContextMessageChannel(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return getDOMConstructor<JSMessageChannelConstructor>(exec, static_cast<JSWorkerContext*>(asObject(slot.slotBase())));
}

JSValue* jsWorkerContextMessagePort(ExecState* exec, const Identifier&, const PropertySlot&)
{
    return JSMessagePort::getConstructor(exec);
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

void setJSWorkerContextMessageChannel(ExecState*, JSObject*, JSValue*)
{
    // FIXME: Do we need to override put for global constructors, like JSDOMWindowBase does?
    ASSERT_NOT_REACHED();
}

void setJSWorkerContextMessagePort(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void setJSWorkerContextWorkerLocation(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}


} // namespace WebCore

#endif // ENABLE(WORKERS)
