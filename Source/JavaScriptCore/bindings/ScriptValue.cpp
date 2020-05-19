/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 * Copyright (c) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptValue.h"

#include "JSCInlines.h"
#include "JSLock.h"

namespace Inspector {

using namespace JSC;

static RefPtr<JSON::Value> jsToInspectorValue(JSGlobalObject* globalObject, JSValue value, int maxDepth)
{
    if (!value) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (!maxDepth)
        return nullptr;

    maxDepth--;

    if (value.isUndefinedOrNull())
        return JSON::Value::null();
    if (value.isBoolean())
        return JSON::Value::create(value.asBoolean());
    if (value.isNumber() && value.isDouble())
        return JSON::Value::create(value.asNumber());
    if (value.isNumber() && value.isAnyInt())
        return JSON::Value::create(static_cast<int>(value.asAnyInt()));
    if (value.isString())
        return JSON::Value::create(asString(value)->value(globalObject));

    if (value.isObject()) {
        if (isJSArray(value)) {
            auto inspectorArray = JSON::Array::create();
            auto& array = *asArray(value);
            unsigned length = array.length();
            for (unsigned i = 0; i < length; i++) {
                auto elementValue = jsToInspectorValue(globalObject, array.getIndex(globalObject, i), maxDepth);
                if (!elementValue)
                    return nullptr;
                inspectorArray->pushValue(WTFMove(elementValue));
            }
            return inspectorArray;
        }
        VM& vm = globalObject->vm();
        auto inspectorObject = JSON::Object::create();
        auto& object = *value.getObject();
        PropertyNameArray propertyNames(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        object.methodTable(vm)->getOwnPropertyNames(&object, globalObject, propertyNames, EnumerationMode());
        for (auto& name : propertyNames) {
            auto inspectorValue = jsToInspectorValue(globalObject, object.get(globalObject, name), maxDepth);
            if (!inspectorValue)
                return nullptr;
            inspectorObject->setValue(name.string(), WTFMove(inspectorValue));
        }
        return inspectorObject;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<JSON::Value> toInspectorValue(JSGlobalObject* globalObject, JSValue value)
{
    // FIXME: Maybe we should move the JSLockHolder stuff to the callers since this function takes a JSValue directly.
    // Doing the locking here made sense when we were trying to abstract the difference between multiple JavaScript engines.
    JSLockHolder holder(globalObject);
    return jsToInspectorValue(globalObject, value, JSON::Value::maxDepth);
}

} // namespace Inspector
