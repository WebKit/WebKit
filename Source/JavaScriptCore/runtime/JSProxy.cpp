/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSProxy.h"

#include "JSGlobalObject.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSProxy);

const ClassInfo JSProxy::s_info = { "JSProxy", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSProxy) };

template<typename Visitor>
void JSProxy::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSProxy* thisObject = jsCast<JSProxy*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_target);
}

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE, JSProxy);

void JSProxy::setTarget(VM& vm, JSGlobalObject* globalObject)
{
    m_target.set(vm, this, globalObject);
    setPrototypeDirect(vm, globalObject->getPrototypeDirect(vm));
}

bool JSProxy::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    return thisObject->target()->methodTable(globalObject->vm())->getOwnPropertySlot(thisObject->target(), globalObject, propertyName, slot);
}

bool JSProxy::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    return thisObject->target()->methodTable(globalObject->vm())->getOwnPropertySlotByIndex(thisObject->target(), globalObject, propertyName, slot);
}

bool JSProxy::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSProxy* thisObject = jsCast<JSProxy*>(cell);
    return thisObject->target()->methodTable(globalObject->vm())->put(thisObject->target(), globalObject, propertyName, value, slot);
}

bool JSProxy::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    JSProxy* thisObject = jsCast<JSProxy*>(cell);
    return thisObject->target()->methodTable(globalObject->vm())->putByIndex(thisObject->target(), globalObject, propertyName, value, shouldThrow);
}

bool JSProxy::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    return thisObject->target()->methodTable(globalObject->vm())->defineOwnProperty(thisObject->target(), globalObject, propertyName, descriptor, shouldThrow);
}

bool JSProxy::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    JSProxy* thisObject = jsCast<JSProxy*>(cell);
    return thisObject->target()->methodTable(globalObject->vm())->deleteProperty(thisObject->target(), globalObject, propertyName, slot);
}

bool JSProxy::isExtensible(JSObject* object, JSGlobalObject* globalObject)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    return thisObject->target()->methodTable(globalObject->vm())->isExtensible(thisObject->target(), globalObject);
}

bool JSProxy::preventExtensions(JSObject* object, JSGlobalObject* globalObject)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    return thisObject->target()->methodTable(globalObject->vm())->preventExtensions(thisObject->target(), globalObject);
}

bool JSProxy::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName)
{
    JSProxy* thisObject = jsCast<JSProxy*>(cell);
    return thisObject->target()->methodTable(globalObject->vm())->deletePropertyByIndex(thisObject->target(), globalObject, propertyName);
}

void JSProxy::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    thisObject->target()->methodTable(globalObject->vm())->getOwnPropertyNames(thisObject->target(), globalObject, propertyNames, mode);
}

bool JSProxy::setPrototype(JSObject* object, JSGlobalObject* globalObject, JSValue prototype, bool shouldThrowIfCantSet)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    return thisObject->target()->methodTable(globalObject->vm())->setPrototype(thisObject->target(), globalObject, prototype, shouldThrowIfCantSet);
}

JSValue JSProxy::getPrototype(JSObject* object, JSGlobalObject* globalObject)
{
    JSProxy* thisObject = jsCast<JSProxy*>(object);
    return thisObject->target()->methodTable(globalObject->vm())->getPrototype(thisObject->target(), globalObject);
}

} // namespace JSC
