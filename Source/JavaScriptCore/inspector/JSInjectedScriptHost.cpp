/*
 * Copyright (C) 2013, 2015-2016 Apple Inc. All rights reserved.
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

#include "ArrayIteratorPrototype.h"
#include "BuiltinNames.h"
#include "Completion.h"
#include "DateInstance.h"
#include "DirectArguments.h"
#include "Error.h"
#include "InjectedScriptHost.h"
#include "IterationKind.h"
#include "IteratorOperations.h"
#include "IteratorPrototype.h"
#include "JSArray.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInjectedScriptHostPrototype.h"
#include "JSMap.h"
#include "JSPromise.h"
#include "JSSet.h"
#include "JSStringIterator.h"
#include "JSTypedArrays.h"
#include "JSWeakMap.h"
#include "JSWeakSet.h"
#include "JSWithScope.h"
#include "MapIteratorPrototype.h"
#include "ObjectConstructor.h"
#include "ProxyObject.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "SetIteratorPrototype.h"
#include "SourceCode.h"
#include "TypedArrayInlines.h"

using namespace JSC;

namespace Inspector {

const ClassInfo JSInjectedScriptHost::s_info = { "InjectedScriptHost", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSInjectedScriptHost) };

JSInjectedScriptHost::JSInjectedScriptHost(VM& vm, Structure* structure, Ref<InjectedScriptHost>&& impl)
    : JSDestructibleObject(vm, structure)
    , m_wrapped(WTFMove(impl))
{
}

void JSInjectedScriptHost::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
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

JSValue JSInjectedScriptHost::evaluateWithScopeExtension(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue scriptValue = exec->argument(0);
    if (!scriptValue.isString())
        return throwTypeError(exec, scope, ASCIILiteral("InjectedScriptHost.evaluateWithScopeExtension first argument must be a string."));

    String program = asString(scriptValue)->value(exec);
    RETURN_IF_EXCEPTION(scope, JSValue());

    NakedPtr<Exception> exception;
    JSObject* scopeExtension = exec->argument(1).getObject();
    JSValue result = JSC::evaluateWithScopeExtension(exec, makeSource(program, exec->callerSourceOrigin()), scopeExtension, exception);
    if (exception)
        throwException(exec, scope, exception);

    return result;
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

    VM& vm = exec->vm();
    JSValue value = exec->uncheckedArgument(0);
    return jsBoolean(impl().isHTMLAllCollection(vm, value));
}

JSValue JSInjectedScriptHost::subtype(ExecState* exec)
{
    VM& vm = exec->vm();
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    if (value.isString())
        return vm.smallStrings.stringString();
    if (value.isBoolean())
        return vm.smallStrings.booleanString();
    if (value.isNumber())
        return vm.smallStrings.numberString();
    if (value.isSymbol())
        return vm.smallStrings.symbolString();

    JSObject* object = asObject(value);
    if (object) {
        if (object->isErrorInstance())
            return jsNontrivialString(exec, ASCIILiteral("error"));

        // Consider class constructor functions class objects.
        JSFunction* function = jsDynamicCast<JSFunction*>(vm, value);
        if (function && function->isClassConstructorFunction())
            return jsNontrivialString(exec, ASCIILiteral("class"));
    }

    if (value.inherits(vm, JSArray::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(vm, DirectArguments::info()) || value.inherits(vm, ScopedArguments::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));

    if (value.inherits(vm, DateInstance::info()))
        return jsNontrivialString(exec, ASCIILiteral("date"));
    if (value.inherits(vm, RegExpObject::info()))
        return jsNontrivialString(exec, ASCIILiteral("regexp"));
    if (value.inherits(vm, ProxyObject::info()))
        return jsNontrivialString(exec, ASCIILiteral("proxy"));

    if (value.inherits(vm, JSMap::info()))
        return jsNontrivialString(exec, ASCIILiteral("map"));
    if (value.inherits(vm, JSSet::info()))
        return jsNontrivialString(exec, ASCIILiteral("set"));
    if (value.inherits(vm, JSWeakMap::info()))
        return jsNontrivialString(exec, ASCIILiteral("weakmap"));
    if (value.inherits(vm, JSWeakSet::info()))
        return jsNontrivialString(exec, ASCIILiteral("weakset"));

    if (value.inherits(vm, JSStringIterator::info()))
        return jsNontrivialString(exec, ASCIILiteral("iterator"));

    if (object) {
        if (object->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorNextIndexPrivateName())
            || object->getDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName())
            || object->getDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName()))
            return jsNontrivialString(exec, ASCIILiteral("iterator"));
    }

    if (value.inherits(vm, JSInt8Array::info())
        || value.inherits(vm, JSInt16Array::info())
        || value.inherits(vm, JSInt32Array::info())
        || value.inherits(vm, JSUint8Array::info())
        || value.inherits(vm, JSUint8ClampedArray::info())
        || value.inherits(vm, JSUint16Array::info())
        || value.inherits(vm, JSUint32Array::info())
        || value.inherits(vm, JSFloat32Array::info())
        || value.inherits(vm, JSFloat64Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));

    return impl().subtype(exec, value);
}

JSValue JSInjectedScriptHost::functionDetails(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    JSValue value = exec->uncheckedArgument(0);
    if (!value.asCell()->inherits(vm, JSFunction::info()))
        return jsUndefined();

    // FIXME: <https://webkit.org/b/87192> Web Inspector: Expose function scope / closure data

    // FIXME: This should provide better details for JSBoundFunctions.

    JSFunction* function = jsCast<JSFunction*>(value);
    const SourceCode* sourceCode = function->sourceCode();
    if (!sourceCode)
        return jsUndefined();

    // In the inspector protocol all positions are 0-based while in SourceCode they are 1-based
    int lineNumber = sourceCode->firstLine().oneBasedInt();
    if (lineNumber)
        lineNumber -= 1;
    int columnNumber = sourceCode->startColumn().oneBasedInt();
    if (columnNumber)
        columnNumber -= 1;

    String scriptID = String::number(sourceCode->provider()->asID());
    JSObject* location = constructEmptyObject(exec);
    location->putDirect(vm, Identifier::fromString(exec, "scriptId"), jsString(exec, scriptID));
    location->putDirect(vm, Identifier::fromString(exec, "lineNumber"), jsNumber(lineNumber));
    location->putDirect(vm, Identifier::fromString(exec, "columnNumber"), jsNumber(columnNumber));

    JSObject* result = constructEmptyObject(exec);
    result->putDirect(vm, Identifier::fromString(exec, "location"), location);

    String name = function->name(vm);
    if (!name.isEmpty())
        result->putDirect(vm, Identifier::fromString(exec, "name"), jsString(exec, name));

    String displayName = function->displayName(vm);
    if (!displayName.isEmpty())
        result->putDirect(vm, Identifier::fromString(exec, "displayName"), jsString(exec, displayName));

    return result;
}

static JSObject* constructInternalProperty(ExecState* exec, const String& name, JSValue value)
{
    VM& vm = exec->vm();
    JSObject* result = constructEmptyObject(exec);
    result->putDirect(vm, Identifier::fromString(exec, "name"), jsString(exec, name));
    result->putDirect(vm, Identifier::fromString(exec, "value"), value);
    return result;
}

JSValue JSInjectedScriptHost::getInternalProperties(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = exec->uncheckedArgument(0);

    JSValue internalProperties = impl().getInternalProperties(vm, exec, value);
    if (internalProperties)
        return internalProperties;

    if (JSPromise* promise = jsDynamicCast<JSPromise*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr);
        RETURN_IF_EXCEPTION(scope, JSValue());
        switch (promise->status(vm)) {
        case JSPromise::Status::Pending:
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("status"), jsNontrivialString(exec, ASCIILiteral("pending"))));
            return array;
        case JSPromise::Status::Fulfilled:
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("status"), jsNontrivialString(exec, ASCIILiteral("resolved"))));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("result"), promise->result(vm)));
            return array;
        case JSPromise::Status::Rejected:
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("status"), jsNontrivialString(exec, ASCIILiteral("rejected"))));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("result"), promise->result(vm)));
            return array;
        }
        // FIXME: <https://webkit.org/b/141664> Web Inspector: ES6: Improved Support for Promises - Promise Reactions
        RELEASE_ASSERT_NOT_REACHED();
    }

    if (JSBoundFunction* boundFunction = jsDynamicCast<JSBoundFunction*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr);
        RETURN_IF_EXCEPTION(scope, JSValue());
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "targetFunction", boundFunction->targetFunction()));
        RETURN_IF_EXCEPTION(scope, JSValue());
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "boundThis", boundFunction->boundThis()));
        RETURN_IF_EXCEPTION(scope, JSValue());
        if (boundFunction->boundArgs()) {
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "boundArgs", boundFunction->boundArgsCopy(exec)));
            return array;
        }
        return array;
    }

    if (ProxyObject* proxy = jsDynamicCast<ProxyObject*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 2);
        RETURN_IF_EXCEPTION(scope, JSValue());
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("target"), proxy->target()));
        RETURN_IF_EXCEPTION(scope, JSValue());
        scope.release();
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, ASCIILiteral("handler"), proxy->handler()));
        return array;
    }

    if (JSObject* iteratorObject = jsDynamicCast<JSObject*>(vm, value)) {
        if (iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorNextIndexPrivateName())) {
            JSValue iteratedValue = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
            JSValue kind = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorKindPrivateName());

            unsigned index = 0;
            JSArray* array = constructEmptyArray(exec, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "array", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", kind));
            return array;
        }

        if (iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName())) {
            JSValue iteratedValue = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
            String kind;
            switch (static_cast<IterationKind>(iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapIteratorKindPrivateName()).asInt32())) {
            case IterateKey:
                kind = ASCIILiteral("key");
                break;
            case IterateValue:
                kind = ASCIILiteral("value");
                break;
            case IterateKeyValue:
                kind = ASCIILiteral("key+value");
                break;
            }
            unsigned index = 0;
            JSArray* array = constructEmptyArray(exec, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "map", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", jsNontrivialString(exec, kind)));
            return array;
        }

        if (iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName())) {
            JSValue iteratedValue = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
            String kind;
            switch (static_cast<IterationKind>(iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setIteratorKindPrivateName()).asInt32())) {
            case IterateKey:
                kind = ASCIILiteral("key");
                break;
            case IterateValue:
                kind = ASCIILiteral("value");
                break;
            case IterateKeyValue:
                kind = ASCIILiteral("key+value");
                break;
            }
            unsigned index = 0;
            JSArray* array = constructEmptyArray(exec, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "set", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", jsNontrivialString(exec, kind)));
            return array;
        }
    }

    if (JSStringIterator* stringIterator = jsDynamicCast<JSStringIterator*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(exec, nullptr, 1);
        RETURN_IF_EXCEPTION(scope, JSValue());
        scope.release();
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "string", stringIterator->iteratedValue(exec)));
        return array;
    }

    return jsUndefined();
}

JSValue JSInjectedScriptHost::proxyTargetValue(ExecState *exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    JSValue value = exec->uncheckedArgument(0);
    ProxyObject* proxy = jsDynamicCast<ProxyObject*>(vm, value);
    if (!proxy)
        return jsUndefined();

    JSObject* target = proxy->target();
    while (ProxyObject* proxy = jsDynamicCast<ProxyObject*>(vm, target))
        target = proxy->target();

    return target;
}

JSValue JSInjectedScriptHost::weakMapSize(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    JSValue value = exec->uncheckedArgument(0);
    JSWeakMap* weakMap = jsDynamicCast<JSWeakMap*>(vm, value);
    if (!weakMap)
        return jsUndefined();

    return jsNumber(weakMap->size());
}

JSValue JSInjectedScriptHost::weakMapEntries(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = exec->uncheckedArgument(0);
    JSWeakMap* weakMap = jsDynamicCast<JSWeakMap*>(vm, value);
    if (!weakMap)
        return jsUndefined();

    unsigned numberToFetch = 100;

    JSValue numberToFetchArg = exec->argument(1);
    double fetchDouble = numberToFetchArg.toInteger(exec);
    if (fetchDouble >= 0)
        numberToFetch = static_cast<unsigned>(fetchDouble);

    JSArray* array = constructEmptyArray(exec, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValue());

    MarkedArgumentBuffer buffer;
    weakMap->takeSnapshot(buffer, numberToFetch);

    for (unsigned index = 0; index < buffer.size(); index += 2) {
        JSObject* entry = constructEmptyObject(exec);
        entry->putDirect(vm, Identifier::fromString(exec, "key"), buffer.at(index));
        entry->putDirect(vm, Identifier::fromString(exec, "value"), buffer.at(index + 1));
        array->putDirectIndex(exec, index / 2, entry);
        RETURN_IF_EXCEPTION(scope, JSValue());
    }

    return array;
}

JSValue JSInjectedScriptHost::weakSetSize(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    JSValue value = exec->uncheckedArgument(0);
    JSWeakSet* weakSet = jsDynamicCast<JSWeakSet*>(vm, value);
    if (!weakSet)
        return jsUndefined();

    return jsNumber(weakSet->size());
}

JSValue JSInjectedScriptHost::weakSetEntries(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = exec->uncheckedArgument(0);
    JSWeakSet* weakSet = jsDynamicCast<JSWeakSet*>(vm, value);
    if (!weakSet)
        return jsUndefined();

    unsigned numberToFetch = 100;

    JSValue numberToFetchArg = exec->argument(1);
    double fetchDouble = numberToFetchArg.toInteger(exec);
    if (fetchDouble >= 0)
        numberToFetch = static_cast<unsigned>(fetchDouble);

    JSArray* array = constructEmptyArray(exec, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValue());

    MarkedArgumentBuffer buffer;
    weakSet->takeSnapshot(buffer, numberToFetch);

    for (unsigned index = 0; index < buffer.size(); ++index) {
        JSObject* entry = constructEmptyObject(exec);
        entry->putDirect(vm, Identifier::fromString(exec, "value"), buffer.at(index));
        array->putDirectIndex(exec, index, entry);
        RETURN_IF_EXCEPTION(scope, JSValue());
    }

    return array;
}

static JSObject* cloneArrayIteratorObject(ExecState* exec, VM& vm, JSObject* iteratorObject, JSGlobalObject* globalObject, JSValue nextIndex, JSValue iteratedObject)
{
    ASSERT(iteratorObject->type() == FinalObjectType);
    JSObject* clone = constructEmptyObject(exec, ArrayIteratorPrototype::create(vm, globalObject, ArrayIteratorPrototype::createStructure(vm, globalObject, globalObject->iteratorPrototype())));
    clone->putDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName(), iteratedObject);
    clone->putDirect(vm, vm.propertyNames->builtinNames().arrayIteratorKindPrivateName(), iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorKindPrivateName()));
    clone->putDirect(vm, vm.propertyNames->builtinNames().arrayIteratorNextIndexPrivateName(), nextIndex);
    clone->putDirect(vm, vm.propertyNames->builtinNames().arrayIteratorNextPrivateName(), iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorNextPrivateName()));
    clone->putDirect(vm, vm.propertyNames->builtinNames().arrayIteratorIsDonePrivateName(), iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorIsDonePrivateName()));
    return clone;
}

static JSObject* cloneMapIteratorObject(ExecState* exec, VM& vm, JSObject* iteratorObject, JSGlobalObject* globalObject, JSValue mapBucket, JSValue iteratedObject)
{
    ASSERT(iteratorObject->type() == FinalObjectType);
    JSObject* clone = constructEmptyObject(exec, MapIteratorPrototype::create(vm, globalObject, MapIteratorPrototype::createStructure(vm, globalObject, globalObject->iteratorPrototype())));
    clone->putDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName(), iteratedObject);
    clone->putDirect(vm, vm.propertyNames->builtinNames().mapIteratorKindPrivateName(), iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapIteratorKindPrivateName()));
    clone->putDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName(), mapBucket);
    return clone;
}

static JSObject* cloneSetIteratorObject(ExecState* exec, VM& vm, JSObject* iteratorObject, JSGlobalObject* globalObject, JSValue setBucket, JSValue iteratedObject)
{
    ASSERT(iteratorObject->type() == FinalObjectType);
    JSObject* clone = constructEmptyObject(exec, SetIteratorPrototype::create(vm, globalObject, SetIteratorPrototype::createStructure(vm, globalObject, globalObject->iteratorPrototype())));
    clone->putDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName(), iteratedObject);
    clone->putDirect(vm, vm.propertyNames->builtinNames().setIteratorKindPrivateName(), iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setIteratorKindPrivateName()));
    clone->putDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName(), setBucket);
    return clone;
}

JSValue JSInjectedScriptHost::iteratorEntries(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue iterator;
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    JSValue value = exec->uncheckedArgument(0);
    if (JSStringIterator* stringIterator = jsDynamicCast<JSStringIterator*>(vm, value)) {
        if (globalObject->isStringPrototypeIteratorProtocolFastAndNonObservable())
            iterator = stringIterator->clone(exec);
    } else if (JSObject* iteratorObject = jsDynamicCast<JSObject*>(vm, value)) {
        // Detect an ArrayIterator by checking for one of its unique private properties.
        JSValue iteratedObject = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
        if (JSValue nextIndex = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorNextIndexPrivateName())) {
            if (isJSArray(iteratedObject)) {
                JSArray* array = jsCast<JSArray*>(iteratedObject);
                if (array->isIteratorProtocolFastAndNonObservable())
                    iterator = cloneArrayIteratorObject(exec, vm, iteratorObject, globalObject, nextIndex, iteratedObject);
            } else if (iteratedObject.isObject() && TypeInfo::isArgumentsType(asObject(iteratedObject)->type())) {
                if (globalObject->isArrayPrototypeIteratorProtocolFastAndNonObservable())
                    iterator = cloneArrayIteratorObject(exec, vm, iteratorObject, globalObject, nextIndex, iteratedObject);
            }
        } else if (JSValue mapBucket = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName())) {
            if (jsCast<JSMap*>(iteratedObject)->isIteratorProtocolFastAndNonObservable())
                iterator = cloneMapIteratorObject(exec, vm, iteratorObject, globalObject, mapBucket, iteratedObject);
        } else if (JSValue setBucket = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName())) {
            if (jsCast<JSSet*>(iteratedObject)->isIteratorProtocolFastAndNonObservable())
                iterator = cloneSetIteratorObject(exec, vm, iteratorObject, globalObject, setBucket, iteratedObject);
        }
    }
    RETURN_IF_EXCEPTION(scope, { });
    if (!iterator)
        return jsUndefined();

    IterationRecord iterationRecord = { iterator, iterator.get(exec, vm.propertyNames->next) };

    unsigned numberToFetch = 5;
    JSValue numberToFetchArg = exec->argument(1);
    double fetchDouble = numberToFetchArg.toInteger(exec);
    RETURN_IF_EXCEPTION(scope, { });
    if (fetchDouble >= 0)
        numberToFetch = static_cast<unsigned>(fetchDouble);

    JSArray* array = constructEmptyArray(exec, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    for (unsigned i = 0; i < numberToFetch; ++i) {
        JSValue next = iteratorStep(exec, iterationRecord);
        if (UNLIKELY(scope.exception()) || next.isFalse())
            break;

        JSValue nextValue = iteratorValue(exec, next);
        RETURN_IF_EXCEPTION(scope, { });

        JSObject* entry = constructEmptyObject(exec);
        entry->putDirect(vm, Identifier::fromString(exec, "value"), nextValue);
        array->putDirectIndex(exec, i, entry);
        if (UNLIKELY(scope.exception())) {
            scope.release();
            iteratorClose(exec, iterationRecord);
            break;
        }
    }

    return array;
}

} // namespace Inspector
