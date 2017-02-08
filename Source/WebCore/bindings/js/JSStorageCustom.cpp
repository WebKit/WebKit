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
#include "JSStorage.h"

#include "JSDOMConvertStrings.h"
#include "JSDOMExceptionHandling.h"
#include <runtime/JSCInlines.h>
#include <runtime/PropertyNameArray.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

bool JSStorage::deleteProperty(JSCell* cell, ExecState* state, PropertyName propertyName)
{
    auto& thisObject = *jsCast<JSStorage*>(cell);

    // Only perform the custom delete if the object doesn't have a native property by this name.
    // Since hasProperty() would end up calling canGetItemsForName() and be fooled, we need to check
    // the native property slots manually.
    PropertySlot slot(&thisObject, PropertySlot::InternalMethodType::GetOwnProperty);

    JSValue prototype = thisObject.getPrototypeDirect();
    if (prototype.isObject() && asObject(prototype)->getPropertySlot(state, propertyName, slot))
        return Base::deleteProperty(&thisObject, state, propertyName);

    if (propertyName.isSymbol())
        return Base::deleteProperty(&thisObject, state, propertyName);

    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    propagateException(*state, scope, thisObject.wrapped().removeItem(propertyNameToString(propertyName)));
    return true;
}

bool JSStorage::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    return deleteProperty(cell, exec, Identifier::from(exec, propertyName));
}

void JSStorage::getOwnPropertyNames(JSObject* object, ExecState* state, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& thisObject = *jsCast<JSStorage*>(object);
    auto lengthResult = thisObject.wrapped().length();
    if (lengthResult.hasException()) {
        propagateException(*state, scope, lengthResult.releaseException());
        return;
    }
    unsigned length = lengthResult.releaseReturnValue();
    for (unsigned i = 0; i < length; ++i) {
        auto keyResult = thisObject.wrapped().key(i);
        if (keyResult.hasException()) {
            propagateException(*state, scope, lengthResult.releaseException());
            return;
        }
        propertyNames.add(Identifier::fromString(state, keyResult.releaseReturnValue()));
    }
        
    Base::getOwnPropertyNames(&thisObject, state, propertyNames, mode);
}

bool JSStorage::putDelegate(ExecState* state, PropertyName propertyName, JSValue value, PutPropertySlot&, bool& putResult)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Only perform the custom put if the object doesn't have a native property by this name.
    // Since hasProperty() would end up calling canGetItemsForName() and be fooled, we need to check
    // the native property slots manually.
    PropertySlot slot { this, PropertySlot::InternalMethodType::GetOwnProperty };

    JSValue prototype = this->getPrototypeDirect();
    if (prototype.isObject() && asObject(prototype)->getPropertySlot(state, propertyName, slot))
        return false;

    if (propertyName.isSymbol())
        return false;

    String stringValue = value.toWTFString(state);
    RETURN_IF_EXCEPTION(scope, true);

    auto setItemResult = wrapped().setItem(propertyNameToString(propertyName), stringValue);
    if (setItemResult.hasException()) {
        propagateException(*state, scope, setItemResult.releaseException());
        return true;
    }

    putResult = true;
    return true;
}

} // namespace WebCore
