/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "JSRemoteDOMWindow.h"

#include "JSDOMWindowCustom.h"
#include "WebCoreJSClientData.h"

namespace WebCore {
using namespace JSC;

bool JSRemoteDOMWindow::getOwnPropertySlot(JSObject* object, ExecState* state, PropertyName propertyName, PropertySlot& slot)
{
    if (std::optional<unsigned> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(object, state, index.value(), slot);

    auto* thisObject = jsCast<JSRemoteDOMWindow*>(object);
    return jsDOMWindowGetOwnPropertySlotRestrictedAccess<DOMWindowType::Remote>(thisObject, thisObject->wrapped(), *state, propertyName, slot, String());
}

bool JSRemoteDOMWindow::getOwnPropertySlotByIndex(JSObject* object, ExecState* state, unsigned index, PropertySlot& slot)
{
    auto* thisObject = jsCast<JSRemoteDOMWindow*>(object);

    // Indexed getters take precendence over regular properties, so caching would be invalid.
    slot.disableCaching();

    // FIXME: Add support for indexed properties.

    return jsDOMWindowGetOwnPropertySlotRestrictedAccess<DOMWindowType::Remote>(thisObject, thisObject->wrapped(), *state, Identifier::from(state, index), slot, String());
}

bool JSRemoteDOMWindow::put(JSCell* cell, ExecState* state, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsCast<JSRemoteDOMWindow*>(cell);
    if (!thisObject->wrapped().frame())
        return false;

    String errorMessage;

    // We only allow setting "location" attribute cross-origin.
    if (propertyName == static_cast<JSVMClientData*>(vm.clientData)->builtinNames().locationPublicName()) {
        bool putResult = false;
        if (lookupPut(state, propertyName, thisObject, value, *s_info.staticPropHashTable, slot, putResult))
            return putResult;
        return false;
    }
    throwSecurityError(*state, scope, errorMessage);
    return false;
}

bool JSRemoteDOMWindow::putByIndex(JSCell*, ExecState*, unsigned, JSValue, bool)
{
    return false;
}

bool JSRemoteDOMWindow::deleteProperty(JSCell*, ExecState* state, PropertyName)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwSecurityError(*state, scope, String());
    return false;
}

bool JSRemoteDOMWindow::deletePropertyByIndex(JSCell*, ExecState* state, unsigned)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwSecurityError(*state, scope, String());
    return false;
}

void JSRemoteDOMWindow::getOwnPropertyNames(JSObject*, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    // FIXME: Add scoped children indexes.

    if (mode.includeDontEnumProperties())
        addCrossOriginOwnPropertyNames<CrossOriginObject::Window>(*exec, propertyNames);
}

bool JSRemoteDOMWindow::defineOwnProperty(JSC::JSObject*, JSC::ExecState* state, JSC::PropertyName, const JSC::PropertyDescriptor&, bool)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwSecurityError(*state, scope, String());
    return false;
}

JSValue JSRemoteDOMWindow::getPrototype(JSObject*, ExecState*)
{
    return jsNull();
}

bool JSRemoteDOMWindow::preventExtensions(JSObject*, ExecState* exec)
{
    auto scope = DECLARE_THROW_SCOPE(exec->vm());
    throwTypeError(exec, scope, "Cannot prevent extensions on this object"_s);
    return false;
}

String JSRemoteDOMWindow::toStringName(const JSObject*, ExecState*)
{
    return "Object"_s;
}

} // namepace WebCore
