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

#include "JSDOMBinding.h"
#include <runtime/IdentifierInlines.h>
#include <runtime/PropertyNameArray.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

bool JSStorage::nameGetter(ExecState* exec, PropertyName propertyName, JSValue& value)
{
    if (propertyName.isSymbol())
        return false;

    ExceptionCode ec = 0;
    String item = wrapped().getItem(propertyNameToString(propertyName), ec);
    setDOMException(exec, ec);

    if (item.isNull())
        return false;

    value = jsStringWithCache(exec, item);
    return true;
}

bool JSStorage::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSStorage* thisObject = jsCast<JSStorage*>(cell);
    // Only perform the custom delete if the object doesn't have a native property by this name.
    // Since hasProperty() would end up calling canGetItemsForName() and be fooled, we need to check
    // the native property slots manually.
    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);

    static_assert(!hasStaticPropertyTable, "This function does not handle static instance properties");

    JSValue prototype = thisObject->prototype();
    if (prototype.isObject() && asObject(prototype)->getPropertySlot(exec, propertyName, slot))
        return Base::deleteProperty(thisObject, exec, propertyName);

    if (propertyName.isSymbol())
        return Base::deleteProperty(thisObject, exec, propertyName);

    ExceptionCode ec = 0;
    thisObject->wrapped().removeItem(propertyNameToString(propertyName), ec);
    setDOMException(exec, ec);
    return true;
}

bool JSStorage::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    static_assert(!hasStaticPropertyTable, "This function does not handle static instance properties");
    return deleteProperty(cell, exec, Identifier::from(exec, propertyName));
}

void JSStorage::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSStorage* thisObject = jsCast<JSStorage*>(object);
    ExceptionCode ec = 0;
    unsigned length = thisObject->wrapped().length(ec);
    setDOMException(exec, ec);
    if (exec->hadException())
        return;
    for (unsigned i = 0; i < length; ++i) {
        propertyNames.add(Identifier::fromString(exec, thisObject->wrapped().key(i, ec)));
        setDOMException(exec, ec);
        if (exec->hadException())
            return;
    }
        
    Base::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

bool JSStorage::putDelegate(ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot&)
{
    // Only perform the custom put if the object doesn't have a native property by this name.
    // Since hasProperty() would end up calling canGetItemsForName() and be fooled, we need to check
    // the native property slots manually.
    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
    static_assert(!hasStaticPropertyTable, "This function does not handle static instance properties");

    JSValue prototype = this->prototype();
    if (prototype.isObject() && asObject(prototype)->getPropertySlot(exec, propertyName, slot))
        return false;

    if (propertyName.isSymbol())
        return false;

    String stringValue = value.toString(exec)->value(exec);
    if (exec->hadException())
        return true;

    ExceptionCode ec = 0;
    wrapped().setItem(propertyNameToString(propertyName), stringValue, ec);
    setDOMException(exec, ec);

    return true;
}

} // namespace WebCore
