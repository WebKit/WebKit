/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
#include "JSInjectedScriptHost.h"

#include "DateInstance.h"
#include "DirectArguments.h"
#include "Error.h"
#include "InjectedScriptHost.h"
#include "IteratorOperations.h"
#include "JSArray.h"
#include "JSArrayIterator.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSInjectedScriptHostPrototype.h"
#include "JSMap.h"
#include "JSMapIterator.h"
#include "JSPromise.h"
#include "JSPropertyNameIterator.h"
#include "JSSet.h"
#include "JSSetIterator.h"
#include "JSStringIterator.h"
#include "JSTypedArrays.h"
#include "JSWeakMap.h"
#include "JSWeakSet.h"
#include "ObjectConstructor.h"
#include "ProxyObject.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "SourceCode.h"
#include "TypedArrayInlines.h"
#include "WeakMapData.h"

using namespace JSC;

namespace Inspector {

const ClassInfo JSInjectedScriptHost::s_info = { "InjectedScriptHost", &Base::s_info, 0, CREATE_METHOD_TABLE(JSInjectedScriptHost) };

JSInjectedScriptHost::JSInjectedScriptHost(VM& vm, Structure* structure, Ref<InjectedScriptHost>&& impl)
    : JSDestructibleObject(vm, structure)
    , m_wrapped(WTFMove(impl))
{
}

void JSInjectedScriptHost::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

JSObject* JSInjectedScriptHost::createPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return JSInjectedScriptHostPrototype::create(vm, globalObject, JSInjectedScriptHostPrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
}

void JSInjectedScriptHost::destroy(JSC::JSCell* cell)
{
    JSInjectedScriptHost* thisObject = static_cast<JSInjectedScriptHost*>(cell);
    thisObject->JSInjectedScriptHost::~JSInjectedScriptHost();
}

JSValue JSInjectedScriptHost::evaluate(ExecState* exec) const
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return globalObject->evalFunction();
}

JSValue JSInjectedScriptHost::internalConstructorName(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSObject* object = jsCast<JSObject*>(exec->uncheckedArgument(0).toThis(exec, NotStrictMode));
    return jsString(exec, JSObject::calculatedClassName(object));
}

JSValue JSInjectedScriptHost::isHTMLAllCollection(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    return jsBoolean(impl().isHTMLAllCollection(value));
}

JSValue JSInjectedScriptHost::subtype(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    if (value.isString())
        return exec->vm().smallStrings.stringString();
    if (value.isBoolean())
        return exec->vm().smallStrings.booleanString();
    if (value.isNumber())
        return exec->vm().smallStrings.numberString();
    if (value.isSymbol())
        return exec->vm().smallStrings.symbolString();

    JSObject* object = asObject(value);
    if (object) {
        if (object->isErrorInstance())
            return jsNontrivialString(exec, ASCIILiteral("error"));

        // Consider class constructor functions class objects.
        JSFunction* function = jsDynamicCast<JSFunction*>(value);
        if (function && function->isClassConstructorFunction())
            return jsNontrivialString(exec, ASCIILiteral("class"));
    }

    if (value.inherits(JSArray::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(DirectArguments::info()) || value.inherits(ScopedArguments::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));

    if (value.inherits(DateInstance::info()))
        return jsNontrivialString(exec, ASCIILiteral("date"));
    if (value.inherits(RegExpObject::info()))
        return jsNontrivialString(exec, ASCIILiteral("regexp"));

    if (value.inherits(JSMap::info()))
        return jsNontrivialString(exec, ASCIILiteral("map"));
    if (value.inherits(JSSet::info()))
        return jsNontrivialString(exec, ASCIILiteral("set"));
    if (value.inherits(JSWeakMap::info()))
        return jsNontrivialString(exec, ASCIILiteral("weakmap"));
    if (value.inherits(JSWeakSet::info()))
        return jsNontrivialString(exec, ASCIILiteral("weakset"));

    if (value.inherits(JSArrayIterator::info())
        || value.inherits(JSMapIterator::info())
        || value.inherits(JSSetIterator::info())
        || value.inherits(JSStringIterator::info())
        || value.inherits(JSPropertyNameIterator::info()))
        return jsNontrivialString(exec, ASCIILiteral("iterator"));

    if (value.inherits(JSInt8Array::info()) || value.inherits(JSInt16Array::info()) || value.inherits(JSInt32Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSUint8Array::info()) || value.inherits(JSUint16Array::info()) || value.inherits(JSUint32Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSFloat32Array::info()) || value.inherits(JSFloat64Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));

    return impl().subtype(exec, value);
}

JSValue JSInjectedScriptHost::functionDetails(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    if (!value.asCell()->inherits(JSFunction::info()))
        return jsUndefined();

    // FIXME: This should provide better details for JSBoundFunctions.

    JSFunction* function = jsCast<JSFunction*>(value);
    const SourceCode* sourceCode = function->sourceCode();
    if (!sourceCode)
        return jsUndefined();

    // In the inspector protocol all positions are 0-based while in SourceCode they are 1-based
    int lineNumber = sourceCode->firstLine();
    if (lineNumber)
        lineNumber -= 1;
    int columnNumber = sourceCode->startColumn();
    if (columnNumber)
        columnNumber -= 1;

    String scriptID = String::number(sourceCode->provider()->asID());
    JSObject* location = constructEmptyObject(exec);
    location->putDirect(exec->vm(), Identifier::fromString(exec, "scriptId"), jsString(exec, scriptID));
    location->putDirect(exec->vm(), Identifier::fromString(exec, "lineNumber"), jsNumber(lineNumber));
    location->putDirect(exec->vm(), Identifier::fromString(exec, "columnNumber"), jsNumber(columnNumber));

    JSObject* result = constructEmptyObject(exec);
    result->putDirect(exec->vm(), Identifier::fromString(exec, "location"), location);

    String name = function->name(exec);
    if (!name.isEmpty())
        result->putDirect(exec->vm(), Identifier::fromString(exec, "name"), jsString(exec, name));

    String displayName = function->displayName(exec);
    if (!displayName.isEmpty())
        result->putDirect(exec->vm(), Identifier::fromString(exec, "displayName"), jsString(exec, displayName));

    // FIXME: provide function scope data in "scopesRaw" property when JSC supports it.
    // <https://webkit.org/b/87192> [JSC] expose function (closure) inner context to debugger

    return result;
}

static JSObject* constructInternalProperty(ExecState* exec, const String& name, JSValue value)
{
    JSObject* result = constructEmptyObject(exec);
    result->putDirect(exec->vm(), Identifier::fromString(exec, "name"), jsString(exec, name));
    result->putDirect(exec->vm(), Identifier::fromString(exec, "value"), value);
    return result;
}

JSValue JSInjectedScriptHost::getInternalProperties(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);

    if (JSPromise* promise = jsDynamicCast<JSPromise*>(value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr);
        switch (promise->status(exec->vm())) {
        case JSPromise::Status::Pending:
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("status"), jsNontrivialString(exec, ASCIILiteral("pending"))));
            break;
        case JSPromise::Status::Fulfilled:
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("status"), jsNontrivialString(exec, ASCIILiteral("resolved"))));
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("result"), promise->result(exec->vm())));
            break;
        case JSPromise::Status::Rejected:
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("status"), jsNontrivialString(exec, ASCIILiteral("rejected"))));
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("result"), promise->result(exec->vm())));
            break;
        }
        // FIXME: <https://webkit.org/b/141664> Web Inspector: ES6: Improved Support for Promises - Promise Reactions
        return array;
    }

    if (JSBoundFunction* boundFunction = jsDynamicCast<JSBoundFunction*>(value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 3);
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "targetFunction", boundFunction->targetFunction()));
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "boundThis", boundFunction->boundThis()));
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "boundArgs", boundFunction->boundArgs()));
        return array;
    }

    if (ProxyObject* proxy = jsDynamicCast<ProxyObject*>(value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 2);
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("target"), proxy->target()));
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("handler"), proxy->handler()));
        return array;
    }

    if (JSArrayIterator* arrayIterator = jsDynamicCast<JSArrayIterator*>(value)) {
        String kind;
        switch (arrayIterator->kind(exec)) {
        case ArrayIterateKey:
            kind = ASCIILiteral("key");
            break;
        case ArrayIterateValue:
            kind = ASCIILiteral("value");
            break;
        case ArrayIterateKeyValue:
            kind = ASCIILiteral("key+value");
            break;
        }
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 2);
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "array", arrayIterator->iteratedValue(exec)));
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", jsNontrivialString(exec, kind)));
        return array;
    }

    if (JSMapIterator* mapIterator = jsDynamicCast<JSMapIterator*>(value)) {
        String kind;
        switch (mapIterator->kind()) {
        case MapIterateKey:
            kind = ASCIILiteral("key");
            break;
        case MapIterateValue:
            kind = ASCIILiteral("value");
            break;
        case MapIterateKeyValue:
            kind = ASCIILiteral("key+value");
            break;
        }
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 2);
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "map", mapIterator->iteratedValue()));
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", jsNontrivialString(exec, kind)));
        return array;
    }
        
    if (JSSetIterator* setIterator = jsDynamicCast<JSSetIterator*>(value)) {
        String kind;
        switch (setIterator->kind()) {
        case SetIterateKey:
            kind = ASCIILiteral("key");
            break;
        case SetIterateValue:
            kind = ASCIILiteral("value");
            break;
        case SetIterateKeyValue:
            kind = ASCIILiteral("key+value");
            break;
        }
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 2);
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "set", setIterator->iteratedValue()));
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", jsNontrivialString(exec, kind)));
        return array;
    }

    if (JSStringIterator* stringIterator = jsDynamicCast<JSStringIterator*>(value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 1);
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "string", stringIterator->iteratedValue(exec)));
        return array;
    }

    if (JSPropertyNameIterator* propertyNameIterator = jsDynamicCast<JSPropertyNameIterator*>(value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 1);
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "object", propertyNameIterator->iteratedValue()));
        return array;
    }

    return jsUndefined();
}

JSValue JSInjectedScriptHost::weakMapSize(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    JSWeakMap* weakMap = jsDynamicCast<JSWeakMap*>(value);
    if (!weakMap)
        return jsUndefined();

    return jsNumber(weakMap->weakMapData()->size());
}

JSValue JSInjectedScriptHost::weakMapEntries(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    JSWeakMap* weakMap = jsDynamicCast<JSWeakMap*>(value);
    if (!weakMap)
        return jsUndefined();

    unsigned fetched = 0;
    unsigned numberToFetch = 100;

    JSValue numberToFetchArg = exec->argument(1);
    double fetchDouble = numberToFetchArg.toInteger(exec);
    if (fetchDouble >= 0)
        numberToFetch = static_cast<unsigned>(fetchDouble);

    JSArray* array = constructEmptyArray(exec, nullptr);
    for (auto it = weakMap->weakMapData()->begin(); it != weakMap->weakMapData()->end(); ++it) {
        JSObject* entry = constructEmptyObject(exec);
        entry->putDirect(exec->vm(), Identifier::fromString(exec, "key"), it->key);
        entry->putDirect(exec->vm(), Identifier::fromString(exec, "value"), it->value.get());
        array->putDirectIndex(exec, fetched++, entry);
        if (numberToFetch && fetched >= numberToFetch)
            break;
    }

    return array;
}

JSValue JSInjectedScriptHost::weakSetSize(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    JSWeakSet* weakSet = jsDynamicCast<JSWeakSet*>(value);
    if (!weakSet)
        return jsUndefined();

    return jsNumber(weakSet->weakMapData()->size());
}

JSValue JSInjectedScriptHost::weakSetEntries(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    JSWeakSet* weakSet = jsDynamicCast<JSWeakSet*>(value);
    if (!weakSet)
        return jsUndefined();

    unsigned fetched = 0;
    unsigned numberToFetch = 100;

    JSValue numberToFetchArg = exec->argument(1);
    double fetchDouble = numberToFetchArg.toInteger(exec);
    if (fetchDouble >= 0)
        numberToFetch = static_cast<unsigned>(fetchDouble);

    JSArray* array = constructEmptyArray(exec, nullptr);
    for (auto it = weakSet->weakMapData()->begin(); it != weakSet->weakMapData()->end(); ++it) {
        JSObject* entry = constructEmptyObject(exec);
        entry->putDirect(exec->vm(), Identifier::fromString(exec, "value"), it->key);
        array->putDirectIndex(exec, fetched++, entry);
        if (numberToFetch && fetched >= numberToFetch)
            break;
    }

    return array;
}

JSValue JSInjectedScriptHost::iteratorEntries(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue iterator;
    JSValue value = exec->uncheckedArgument(0);
    if (JSArrayIterator* arrayIterator = jsDynamicCast<JSArrayIterator*>(value))
        iterator = arrayIterator->clone(exec);
    else if (JSMapIterator* mapIterator = jsDynamicCast<JSMapIterator*>(value))
        iterator = mapIterator->clone(exec);
    else if (JSSetIterator* setIterator = jsDynamicCast<JSSetIterator*>(value))
        iterator = setIterator->clone(exec);
    else if (JSStringIterator* stringIterator = jsDynamicCast<JSStringIterator*>(value))
        iterator = stringIterator->clone(exec);
    else if (JSPropertyNameIterator* propertyNameIterator = jsDynamicCast<JSPropertyNameIterator*>(value))
        iterator = propertyNameIterator->clone(exec);
    else
        return jsUndefined();

    unsigned numberToFetch = 5;
    JSValue numberToFetchArg = exec->argument(1);
    double fetchDouble = numberToFetchArg.toInteger(exec);
    if (fetchDouble >= 0)
        numberToFetch = static_cast<unsigned>(fetchDouble);

    JSArray* array = constructEmptyArray(exec, nullptr);

    for (unsigned i = 0; i < numberToFetch; ++i) {
        JSValue next = iteratorStep(exec, iterator);
        if (exec->hadException())
            break;
        if (next.isFalse())
            break;

        JSValue nextValue = iteratorValue(exec, next);
        if (exec->hadException())
            break;

        JSObject* entry = constructEmptyObject(exec);
        entry->putDirect(exec->vm(), Identifier::fromString(exec, "value"), nextValue);
        array->putDirectIndex(exec, i, entry);
    }

    iteratorClose(exec, iterator);

    return array;
}

} // namespace Inspector
