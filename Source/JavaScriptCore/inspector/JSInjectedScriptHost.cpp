/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "ArrayPrototype.h"
#include "BuiltinNames.h"
#include "Completion.h"
#include "DateInstance.h"
#include "DeferGC.h"
#include "DirectArguments.h"
#include "Error.h"
#include "FunctionPrototype.h"
#include "HeapAnalyzer.h"
#include "HeapIterationScope.h"
#include "HeapProfiler.h"
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
#include "JSLock.h"
#include "JSMap.h"
#include "JSPromise.h"
#include "JSPromisePrototype.h"
#include "JSSet.h"
#include "JSStringIterator.h"
#include "JSTypedArrays.h"
#include "JSWeakMap.h"
#include "JSWeakSet.h"
#include "JSWithScope.h"
#include "MapIteratorPrototype.h"
#include "MapPrototype.h"
#include "MarkedSpaceInlines.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "PreventCollectionScope.h"
#include "ProxyObject.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "SetIteratorPrototype.h"
#include "SetPrototype.h"
#include "SourceCode.h"
#include "TypedArrayInlines.h"
#include <wtf/Function.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/PrintStream.h>
#include <wtf/text/StringConcatenate.h>

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

JSValue JSInjectedScriptHost::savedResultAlias(ExecState* exec) const
{
    auto savedResultAlias = impl().savedResultAlias();
    if (!savedResultAlias)
        return jsUndefined();
    return jsString(exec->vm(), savedResultAlias.value());
}

JSValue JSInjectedScriptHost::evaluateWithScopeExtension(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue scriptValue = exec->argument(0);
    if (!scriptValue.isString())
        return throwTypeError(exec, scope, "InjectedScriptHost.evaluateWithScopeExtension first argument must be a string."_s);

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

    VM& vm = exec->vm();
    JSObject* object = jsCast<JSObject*>(exec->uncheckedArgument(0).toThis(exec, NotStrictMode));
    return jsString(vm, JSObject::calculatedClassName(object));
}

JSValue JSInjectedScriptHost::isHTMLAllCollection(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    JSValue value = exec->uncheckedArgument(0);
    return jsBoolean(impl().isHTMLAllCollection(vm, value));
}

JSValue JSInjectedScriptHost::isPromiseRejectedWithNativeGetterTypeError(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* promise = jsDynamicCast<JSPromise*>(vm, exec->argument(0));
    if (!promise || promise->status(vm) != JSPromise::Status::Rejected)
        return throwTypeError(exec, scope, "InjectedScriptHost.isPromiseRejectedWithNativeGetterTypeError first argument must be a rejected Promise."_s);

    bool result = false;
    if (auto* errorInstance = jsDynamicCast<ErrorInstance*>(vm, promise->result(vm)))
        result = errorInstance->isNativeGetterTypeError();
    return jsBoolean(result);
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

    if (auto* object = jsDynamicCast<JSObject*>(vm, value)) {
        if (object->isErrorInstance())
            return jsNontrivialString(vm, "error"_s);

        // Consider class constructor functions class objects.
        JSFunction* function = jsDynamicCast<JSFunction*>(vm, value);
        if (function && function->isClassConstructorFunction())
            return jsNontrivialString(vm, "class"_s);

        if (object->inherits<JSArray>(vm))
            return jsNontrivialString(vm, "array"_s);
        if (object->inherits<DirectArguments>(vm) || object->inherits<ScopedArguments>(vm))
            return jsNontrivialString(vm, "array"_s);

        if (object->inherits<DateInstance>(vm))
            return jsNontrivialString(vm, "date"_s);
        if (object->inherits<RegExpObject>(vm))
            return jsNontrivialString(vm, "regexp"_s);
        if (object->inherits<ProxyObject>(vm))
            return jsNontrivialString(vm, "proxy"_s);

        if (object->inherits<JSMap>(vm))
            return jsNontrivialString(vm, "map"_s);
        if (object->inherits<JSSet>(vm))
            return jsNontrivialString(vm, "set"_s);
        if (object->inherits<JSWeakMap>(vm))
            return jsNontrivialString(vm, "weakmap"_s);
        if (object->inherits<JSWeakSet>(vm))
            return jsNontrivialString(vm, "weakset"_s);

        if (object->inherits<JSStringIterator>(vm))
            return jsNontrivialString(vm, "iterator"_s);

        if (object->getDirect(vm, vm.propertyNames->builtinNames().arrayIteratorNextIndexPrivateName())
            || object->getDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName())
            || object->getDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName()))
            return jsNontrivialString(vm, "iterator"_s);

        if (object->inherits<JSInt8Array>(vm)
            || object->inherits<JSInt16Array>(vm)
            || object->inherits<JSInt32Array>(vm)
            || object->inherits<JSUint8Array>(vm)
            || object->inherits<JSUint8ClampedArray>(vm)
            || object->inherits<JSUint16Array>(vm)
            || object->inherits<JSUint32Array>(vm)
            || object->inherits<JSFloat32Array>(vm)
            || object->inherits<JSFloat64Array>(vm))
            return jsNontrivialString(vm, "array"_s);
    }

    return impl().subtype(exec, value);
}

JSValue JSInjectedScriptHost::functionDetails(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    JSValue value = exec->uncheckedArgument(0);
    auto* function = jsDynamicCast<JSFunction*>(vm, value);
    if (!function)
        return jsUndefined();

    // FIXME: <https://webkit.org/b/87192> Web Inspector: Expose function scope / closure data

    // FIXME: This should provide better details for JSBoundFunctions.

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
    location->putDirect(vm, Identifier::fromString(vm, "scriptId"), jsString(vm, scriptID));
    location->putDirect(vm, Identifier::fromString(vm, "lineNumber"), jsNumber(lineNumber));
    location->putDirect(vm, Identifier::fromString(vm, "columnNumber"), jsNumber(columnNumber));

    JSObject* result = constructEmptyObject(exec);
    result->putDirect(vm, Identifier::fromString(vm, "location"), location);

    String name = function->name(vm);
    if (!name.isEmpty())
        result->putDirect(vm, Identifier::fromString(vm, "name"), jsString(vm, name));

    String displayName = function->displayName(vm);
    if (!displayName.isEmpty())
        result->putDirect(vm, Identifier::fromString(vm, "displayName"), jsString(vm, displayName));

    return result;
}

static JSObject* constructInternalProperty(ExecState* exec, const String& name, JSValue value)
{
    VM& vm = exec->vm();
    JSObject* result = constructEmptyObject(exec);
    result->putDirect(vm, Identifier::fromString(vm, "name"), jsString(vm, name));
    result->putDirect(vm, Identifier::fromString(vm, "value"), value);
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
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "status"_s, jsNontrivialString(vm, "pending"_s)));
            return array;
        case JSPromise::Status::Fulfilled:
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "status"_s, jsNontrivialString(vm, "resolved"_s)));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "result"_s, promise->result(vm)));
            return array;
        case JSPromise::Status::Rejected:
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "status"_s, jsNontrivialString(vm, "rejected"_s)));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "result"_s, promise->result(vm)));
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
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "target"_s, proxy->target()));
        RETURN_IF_EXCEPTION(scope, JSValue());
        scope.release();
        array->putDirectIndex(exec, index++, constructInternalProperty(exec, "handler"_s, proxy->handler()));
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
                kind = "key"_s;
                break;
            case IterateValue:
                kind = "value"_s;
                break;
            case IterateKeyValue:
                kind = "key+value"_s;
                break;
            }
            unsigned index = 0;
            JSArray* array = constructEmptyArray(exec, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "map", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", jsNontrivialString(vm, kind)));
            return array;
        }

        if (iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName())) {
            JSValue iteratedValue = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
            String kind;
            switch (static_cast<IterationKind>(iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setIteratorKindPrivateName()).asInt32())) {
            case IterateKey:
                kind = "key"_s;
                break;
            case IterateValue:
                kind = "value"_s;
                break;
            case IterateKeyValue:
                kind = "key+value"_s;
                break;
            }
            unsigned index = 0;
            JSArray* array = constructEmptyArray(exec, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "set", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(exec, index++, constructInternalProperty(exec, "kind", jsNontrivialString(vm, kind)));
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
        entry->putDirect(vm, Identifier::fromString(vm, "key"), buffer.at(index));
        entry->putDirect(vm, Identifier::fromString(vm, "value"), buffer.at(index + 1));
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
        entry->putDirect(vm, Identifier::fromString(vm, "value"), buffer.at(index));
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
        entry->putDirect(vm, Identifier::fromString(vm, "value"), nextValue);
        array->putDirectIndex(exec, i, entry);
        if (UNLIKELY(scope.exception())) {
            scope.release();
            iteratorClose(exec, iterationRecord);
            break;
        }
    }

    return array;
}

static bool checkForbiddenPrototype(ExecState* exec, JSValue value, JSValue proto)
{
    if (value == proto)
        return true;

    // Check that the prototype chain of proto hasn't been modified to include value.
    return JSObject::defaultHasInstance(exec, proto, value);
}

JSValue JSInjectedScriptHost::queryInstances(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue prototypeOrConstructor = exec->uncheckedArgument(0);
    if (!prototypeOrConstructor.isObject())
        return throwTypeError(exec, scope, "queryInstances first argument must be an object."_s);

    JSObject* object = asObject(prototypeOrConstructor);
    if (object->inherits<ProxyObject>(vm))
        return throwTypeError(exec, scope, "queryInstances cannot be called with a Proxy."_s);

    JSValue prototype = object;

    PropertySlot prototypeSlot(object, PropertySlot::InternalMethodType::VMInquiry);
    if (object->getPropertySlot(exec, vm.propertyNames->prototype, prototypeSlot)) {
        RETURN_IF_EXCEPTION(scope, { });
        if (prototypeSlot.isValue()) {
            JSValue prototypeValue = prototypeSlot.getValue(exec, vm.propertyNames->prototype);
            if (prototypeValue.isObject()) {
                prototype = prototypeValue;
                object = asObject(prototype);
            }
        }
    }

    if (object->inherits<ProxyObject>(vm) || prototype.inherits<ProxyObject>(vm))
        return throwTypeError(exec, scope, "queryInstances cannot be called with a Proxy."_s);

    // FIXME: implement a way of distinguishing between internal and user-created objects.
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    if (checkForbiddenPrototype(exec, object, lexicalGlobalObject->objectPrototype()))
        return throwTypeError(exec, scope, "queryInstances cannot be called with Object."_s);
    if (checkForbiddenPrototype(exec, object, lexicalGlobalObject->functionPrototype()))
        return throwTypeError(exec, scope, "queryInstances cannot be called with Function."_s);
    if (checkForbiddenPrototype(exec, object, lexicalGlobalObject->arrayPrototype()))
        return throwTypeError(exec, scope, "queryInstances cannot be called with Array."_s);
    if (checkForbiddenPrototype(exec, object, lexicalGlobalObject->mapPrototype()))
        return throwTypeError(exec, scope, "queryInstances cannot be called with Map."_s);
    if (checkForbiddenPrototype(exec, object, lexicalGlobalObject->jsSetPrototype()))
        return throwTypeError(exec, scope, "queryInstances cannot be called with Set."_s);
    if (checkForbiddenPrototype(exec, object, lexicalGlobalObject->promisePrototype()))
        return throwTypeError(exec, scope, "queryInstances cannot be called with Promise."_s);

    sanitizeStackForVM(vm);
    vm.heap.collectNow(Sync, CollectionScope::Full);

    JSArray* array = constructEmptyArray(exec, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    {
        HeapIterationScope iterationScope(vm.heap);
        vm.heap.objectSpace().forEachLiveCell(iterationScope, [&] (HeapCell* cell, HeapCell::Kind kind) {
            if (!isJSCellKind(kind))
                return IterationStatus::Continue;

            JSValue value(static_cast<JSCell*>(cell));
            if (value.inherits<ProxyObject>(vm))
                return IterationStatus::Continue;

            if (JSObject::defaultHasInstance(exec, value, prototype))
                array->putDirectIndex(exec, array->length(), value);

            return IterationStatus::Continue;
        });
    }

    return array;
}

class HeapHolderFinder final : public HeapAnalyzer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    HeapHolderFinder(HeapProfiler& profiler, JSCell* target)
        : HeapAnalyzer()
        , m_target(target)
    {
        ASSERT(!profiler.activeHeapAnalyzer());
        profiler.setActiveHeapAnalyzer(this);
        profiler.vm().heap.collectNow(Sync, CollectionScope::Full);
        profiler.setActiveHeapAnalyzer(nullptr);

        HashSet<JSCell*> queue;

        // Filter `m_holders` based on whether they're reachable from a non-Debugger root.
        HashSet<JSCell*> visited;
        for (auto* root : m_rootsToInclude)
            queue.add(root);
        while (auto* from = queue.takeAny()) {
            if (m_rootsToIgnore.contains(from))
                continue;
            if (!visited.add(from).isNewEntry)
                continue;
            for (auto* to : m_successors.get(from))
                queue.add(to);
        }

        // If a known holder is not an object, also consider all of the holder's holders.
        for (auto* holder : m_holders)
            queue.add(holder);
        while (auto* holder = queue.takeAny()) {
            if (holder->isObject())
                continue;

            for (auto* from : m_predecessors.get(holder)) {
                if (!m_holders.contains(from)) {
                    m_holders.add(from);
                    queue.add(from);
                }
            }
        }

        m_holders.removeIf([&] (auto* holder) {
            return !holder->isObject() || !visited.contains(holder);
        });
    }

    HashSet<JSCell*>& holders() { return m_holders; }

    void analyzeEdge(JSCell* from, JSCell* to, SlotVisitor::RootMarkReason reason)
    {
        ASSERT(to);
        ASSERT(to->vm().heapProfiler()->activeHeapAnalyzer() == this);

        auto locker = holdLock(m_mutex);

        if (from && from != to) {
            m_successors.ensure(from, [] {
                return HashSet<JSCell*>();
            }).iterator->value.add(to);

            m_predecessors.ensure(to, [] {
                return HashSet<JSCell*>();
            }).iterator->value.add(from);

            if (to == m_target)
                m_holders.add(from);
        }

        if (reason == SlotVisitor::RootMarkReason::Debugger)
            m_rootsToIgnore.add(to);
        else if (!from || reason != SlotVisitor::RootMarkReason::None)
            m_rootsToInclude.add(to);
    }
    void analyzePropertyNameEdge(JSCell* from, JSCell* to, UniquedStringImpl*) { analyzeEdge(from, to, SlotVisitor::RootMarkReason::None); }
    void analyzeVariableNameEdge(JSCell* from, JSCell* to, UniquedStringImpl*) { analyzeEdge(from, to, SlotVisitor::RootMarkReason::None); }
    void analyzeIndexEdge(JSCell* from, JSCell* to, uint32_t) { analyzeEdge(from, to, SlotVisitor::RootMarkReason::None); }

    void analyzeNode(JSCell*) { }
    void setOpaqueRootReachabilityReasonForCell(JSCell*, const char*) { }
    void setWrappedObjectForCell(JSCell*, void*) { }
    void setLabelForCell(JSCell*, const String&) { }

#ifndef NDEBUG
    void dump(PrintStream& out) const
    {
        Indentation<4> indent;

        HashSet<JSCell*> visited;

        Function<void(JSCell*)> visit = [&] (auto* from) {
            auto isFirstVisit = visited.add(from).isNewEntry;

            out.print(makeString(indent));

            out.print("[ "_s);
            if (from == m_target)
                out.print("T "_s);
            if (m_holders.contains(from))
                out.print("H "_s);
            if (m_rootsToIgnore.contains(from))
                out.print("- "_s);
            else if (m_rootsToInclude.contains(from))
                out.print("+ "_s);
            if (!isFirstVisit)
                out.print("V "_s);
            out.print("] "_s);

            from->dump(out);

            out.println();

            if (isFirstVisit) {
                IndentationScope<4> scope(indent);
                for (auto* to : m_successors.get(from))
                    visit(to);
            }
        };

        for (auto* from : m_rootsToInclude)
            visit(from);
    }
#endif

private:
    Lock m_mutex;
    HashMap<JSCell*, HashSet<JSCell*>> m_predecessors;
    HashMap<JSCell*, HashSet<JSCell*>> m_successors;
    HashSet<JSCell*> m_rootsToInclude;
    HashSet<JSCell*> m_rootsToIgnore;
    HashSet<JSCell*> m_holders;
    const JSCell* m_target;
};

JSValue JSInjectedScriptHost::queryHolders(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = exec->uncheckedArgument(0);
    if (!target.isObject())
        return throwTypeError(exec, scope, "queryHolders first argument must be an object."_s);

    JSArray* result = constructEmptyArray(exec, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    {
        DeferGC deferGC(vm.heap);
        PreventCollectionScope preventCollectionScope(vm.heap);
        sanitizeStackForVM(vm);

        HeapHolderFinder holderFinder(vm.ensureHeapProfiler(), target.asCell());

        auto holders = copyToVector(holderFinder.holders());
        std::sort(holders.begin(), holders.end());
        for (auto* holder : holders)
            result->putDirectIndex(exec, result->length(), holder);
    }

    return result;
}

} // namespace Inspector
