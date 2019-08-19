/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSCWrapperMap.h"

#include "APICast.h"
#include "JSAPIWrapperGlobalObject.h"
#include "JSAPIWrapperObject.h"
#include "JSCClassPrivate.h"
#include "JSCContextPrivate.h"
#include "JSCGLibWrapperObject.h"
#include "JSCInlines.h"
#include "JSCValuePrivate.h"
#include "JSCallbackObject.h"

namespace JSC {

WrapperMap::WrapperMap(JSGlobalContextRef jsContext)
    : m_cachedJSWrappers(makeUnique<JSC::WeakGCMap<gpointer, JSC::JSObject>>(toJS(jsContext)->vm()))
{
}

WrapperMap::~WrapperMap()
{
    for (const auto& jscClass : m_classMap.values())
        jscClassInvalidate(jscClass.get());
}

GRefPtr<JSCValue> WrapperMap::gobjectWrapper(JSCContext* jscContext, JSValueRef jsValue)
{
    auto* jsContext = jscContextGetJSContext(jscContext);
    JSC::JSLockHolder locker(toJS(jsContext));
    ASSERT(toJSGlobalObject(jsContext)->wrapperMap() == this);
    GRefPtr<JSCValue> value = m_cachedGObjectWrappers.get(jsValue);
    if (!value) {
        value = adoptGRef(jscValueCreate(jscContext, jsValue));
        m_cachedGObjectWrappers.set(jsValue, value.get());
    }
    return value;
}

void WrapperMap::unwrap(JSValueRef jsValue)
{
    ASSERT(m_cachedGObjectWrappers.contains(jsValue));
    m_cachedGObjectWrappers.remove(jsValue);
}

void WrapperMap::registerClass(JSCClass* jscClass)
{
    auto* jsClass = jscClassGetJSClass(jscClass);
    ASSERT(!m_classMap.contains(jsClass));
    m_classMap.set(jsClass, jscClass);
}

JSCClass* WrapperMap::registeredClass(JSClassRef jsClass) const
{
    return m_classMap.get(jsClass);
}

JSObject* WrapperMap::createJSWrappper(JSGlobalContextRef jsContext, JSClassRef jsClass, JSValueRef prototype, gpointer wrappedObject, GDestroyNotify destroyFunction)
{
    ASSERT(toJSGlobalObject(jsContext)->wrapperMap() == this);
    ExecState* exec = toJS(jsContext);
    VM& vm = exec->vm();
    JSLockHolder locker(vm);
    auto* object = JSC::JSCallbackObject<JSC::JSAPIWrapperObject>::create(exec, exec->lexicalGlobalObject(), exec->lexicalGlobalObject()->glibWrapperObjectStructure(), jsClass, nullptr);
    if (wrappedObject) {
        object->setWrappedObject(new JSC::JSCGLibWrapperObject(wrappedObject, destroyFunction));
        m_cachedJSWrappers->set(wrappedObject, object);
    }
    if (prototype)
        JSObjectSetPrototype(jsContext, toRef(object), prototype);
    else if (auto* jsPrototype = jsClass->prototype(exec))
        object->setPrototypeDirect(vm, jsPrototype);
    return object;
}

JSGlobalContextRef WrapperMap::createContextWithJSWrappper(JSContextGroupRef jsGroup, JSClassRef jsClass, JSValueRef prototype, gpointer wrappedObject, GDestroyNotify destroyFunction)
{
    Ref<VM> vm(*toJS(jsGroup));
    JSLockHolder locker(vm.ptr());
    auto* globalObject = JSCallbackObject<JSAPIWrapperGlobalObject>::create(vm.get(), jsClass, JSCallbackObject<JSAPIWrapperGlobalObject>::createStructure(vm.get(), nullptr, jsNull()));
    if (wrappedObject) {
        globalObject->setWrappedObject(new JSC::JSCGLibWrapperObject(wrappedObject, destroyFunction));
        m_cachedJSWrappers->set(wrappedObject, globalObject);
    }
    ExecState* exec = globalObject->globalExec();
    if (prototype)
        globalObject->resetPrototype(vm.get(), toJS(exec, prototype));
    else if (auto jsPrototype = jsClass->prototype(exec))
        globalObject->resetPrototype(vm.get(), jsPrototype);
    else
        globalObject->resetPrototype(vm.get(), jsNull());

    return JSGlobalContextRetain(toGlobalRef(exec));
}

JSObject* WrapperMap::jsWrapper(gpointer wrappedObject) const
{
    if (!wrappedObject)
        return nullptr;
    return m_cachedJSWrappers->get(wrappedObject);
}

gpointer WrapperMap::wrappedObject(JSGlobalContextRef jsContext, JSObjectRef jsObject) const
{
    ASSERT(toJSGlobalObject(jsContext)->wrapperMap() == this);
    JSLockHolder locker(toJS(jsContext));
    VM& vm = toJS(jsContext)->vm();
    auto* object = toJS(jsObject);
    if (object->inherits(vm, JSC::JSCallbackObject<JSC::JSAPIWrapperObject>::info())) {
        if (auto* wrapper = JSC::jsCast<JSC::JSAPIWrapperObject*>(object)->wrappedObject())
            return static_cast<JSC::JSCGLibWrapperObject*>(wrapper)->object();
    }
    if (object->inherits(vm, JSC::JSCallbackObject<JSC::JSAPIWrapperGlobalObject>::info())) {
        if (auto* wrapper = JSC::jsCast<JSC::JSAPIWrapperGlobalObject*>(object)->wrappedObject())
            return wrapper->object();
    }
    return nullptr;
}

} // namespace JSC
