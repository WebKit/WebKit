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
#include "JSArrayIterator.h"
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

namespace Inspector {

using namespace JSC;

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

JSValue JSInjectedScriptHost::evaluate(JSGlobalObject* globalObject) const
{
    return globalObject->evalFunction();
}

JSValue JSInjectedScriptHost::savedResultAlias(JSGlobalObject* globalObject) const
{
    auto savedResultAlias = impl().savedResultAlias();
    if (!savedResultAlias)
        return jsUndefined();
    return jsString(globalObject->vm(), savedResultAlias.value());
}

JSValue JSInjectedScriptHost::evaluateWithScopeExtension(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue scriptValue = callFrame->argument(0);
    if (!scriptValue.isString())
        return throwTypeError(globalObject, scope, "InjectedScriptHost.evaluateWithScopeExtension first argument must be a string."_s);

    String program = asString(scriptValue)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    NakedPtr<Exception> exception;
    JSObject* scopeExtension = callFrame->argument(1).getObject();
    JSValue result = JSC::evaluateWithScopeExtension(globalObject, makeSource(program, callFrame->callerSourceOrigin(vm)), scopeExtension, exception);
    if (exception)
        throwException(globalObject, scope, exception);

    return result;
}

JSValue JSInjectedScriptHost::internalConstructorName(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    JSObject* object = jsCast<JSObject*>(callFrame->uncheckedArgument(0).toThis(globalObject, NotStrictMode));
    return jsString(vm, JSObject::calculatedClassName(object));
}

JSValue JSInjectedScriptHost::isHTMLAllCollection(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    JSValue value = callFrame->uncheckedArgument(0);
    return jsBoolean(impl().isHTMLAllCollection(vm, value));
}

JSValue JSInjectedScriptHost::isPromiseRejectedWithNativeGetterTypeError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* promise = jsDynamicCast<JSPromise*>(vm, callFrame->argument(0));
    if (!promise)
        return throwTypeError(globalObject, scope, "InjectedScriptHost.isPromiseRejectedWithNativeGetterTypeError first argument must be a Promise."_s);

    bool result = false;
    if (auto* errorInstance = jsDynamicCast<ErrorInstance*>(vm, promise->result(vm)))
        result = errorInstance->isNativeGetterTypeError();
    return jsBoolean(result);
}

JSValue JSInjectedScriptHost::subtype(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    JSValue value = callFrame->uncheckedArgument(0);
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

        if (object->inherits<JSArrayIterator>(vm)
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

    return impl().subtype(globalObject, value);
}

JSValue JSInjectedScriptHost::functionDetails(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    JSValue value = callFrame->uncheckedArgument(0);
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
    JSObject* location = constructEmptyObject(globalObject);
    location->putDirect(vm, Identifier::fromString(vm, "scriptId"), jsString(vm, scriptID));
    location->putDirect(vm, Identifier::fromString(vm, "lineNumber"), jsNumber(lineNumber));
    location->putDirect(vm, Identifier::fromString(vm, "columnNumber"), jsNumber(columnNumber));

    JSObject* result = constructEmptyObject(globalObject);
    result->putDirect(vm, Identifier::fromString(vm, "location"), location);

    String name = function->name(vm);
    if (!name.isEmpty())
        result->putDirect(vm, Identifier::fromString(vm, "name"), jsString(vm, name));

    String displayName = function->displayName(vm);
    if (!displayName.isEmpty())
        result->putDirect(vm, Identifier::fromString(vm, "displayName"), jsString(vm, displayName));

    return result;
}

static JSObject* constructInternalProperty(JSGlobalObject* globalObject, const String& name, JSValue value)
{
    VM& vm = globalObject->vm();
    JSObject* result = constructEmptyObject(globalObject);
    result->putDirect(vm, Identifier::fromString(vm, "name"), jsString(vm, name));
    result->putDirect(vm, Identifier::fromString(vm, "value"), value);
    return result;
}

JSValue JSInjectedScriptHost::getInternalProperties(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = callFrame->uncheckedArgument(0);

    JSValue internalProperties = impl().getInternalProperties(vm, globalObject, value);
    if (internalProperties)
        return internalProperties;

    if (JSPromise* promise = jsDynamicCast<JSPromise*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(globalObject, nullptr);
        RETURN_IF_EXCEPTION(scope, JSValue());
        switch (promise->status(vm)) {
        case JSPromise::Status::Pending:
            scope.release();
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "status"_s, jsNontrivialString(vm, "pending"_s)));
            return array;
        case JSPromise::Status::Fulfilled:
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "status"_s, jsNontrivialString(vm, "resolved"_s)));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "result"_s, promise->result(vm)));
            return array;
        case JSPromise::Status::Rejected:
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "status"_s, jsNontrivialString(vm, "rejected"_s)));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "result"_s, promise->result(vm)));
            return array;
        }
        // FIXME: <https://webkit.org/b/141664> Web Inspector: ES6: Improved Support for Promises - Promise Reactions
        RELEASE_ASSERT_NOT_REACHED();
    }

    if (JSBoundFunction* boundFunction = jsDynamicCast<JSBoundFunction*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(globalObject, nullptr);
        RETURN_IF_EXCEPTION(scope, JSValue());
        array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "targetFunction", boundFunction->targetFunction()));
        RETURN_IF_EXCEPTION(scope, JSValue());
        array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "boundThis", boundFunction->boundThis()));
        RETURN_IF_EXCEPTION(scope, JSValue());
        if (boundFunction->boundArgs()) {
            scope.release();
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "boundArgs", boundFunction->boundArgsCopy(globalObject)));
            return array;
        }
        return array;
    }

    if (ProxyObject* proxy = jsDynamicCast<ProxyObject*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(globalObject, nullptr, 2);
        RETURN_IF_EXCEPTION(scope, JSValue());
        array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "target"_s, proxy->target()));
        RETURN_IF_EXCEPTION(scope, JSValue());
        scope.release();
        array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "handler"_s, proxy->handler()));
        return array;
    }

    if (JSObject* iteratorObject = jsDynamicCast<JSObject*>(vm, value)) {
        auto toString = [&] (IterationKind kind) {
            switch (kind) {
            case IterationKind::Keys:
                return jsNontrivialString(vm, "keys"_s);
            case IterationKind::Values:
                return jsNontrivialString(vm, "values"_s);
            case IterationKind::Entries:
                return jsNontrivialString(vm, "entries"_s);
            }
            return jsNontrivialString(vm, ""_s);
        };
        
        if (auto* arrayIterator = jsDynamicCast<JSArrayIterator*>(vm, iteratorObject)) {
            JSValue iteratedValue = arrayIterator->iteratedObject();
            IterationKind kind = arrayIterator->kind();

            unsigned index = 0;
            JSArray* array = constructEmptyArray(globalObject, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "array", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "kind", toString(kind)));
            return array;
        }

        if (iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName())) {
            JSValue iteratedValue = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
            IterationKind kind = static_cast<IterationKind>(iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapIteratorKindPrivateName()).asInt32());

            unsigned index = 0;
            JSArray* array = constructEmptyArray(globalObject, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "map", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "kind", toString(kind)));
            return array;
        }

        if (iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName())) {
            JSValue iteratedValue = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
            IterationKind kind = static_cast<IterationKind>(iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setIteratorKindPrivateName()).asInt32());

            unsigned index = 0;
            JSArray* array = constructEmptyArray(globalObject, nullptr, 2);
            RETURN_IF_EXCEPTION(scope, JSValue());
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "set", iteratedValue));
            RETURN_IF_EXCEPTION(scope, JSValue());
            scope.release();
            array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "kind", toString(kind)));
            return array;
        }
    }

    if (JSStringIterator* stringIterator = jsDynamicCast<JSStringIterator*>(vm, value)) {
        unsigned index = 0;
        JSArray* array = constructEmptyArray(globalObject, nullptr, 1);
        RETURN_IF_EXCEPTION(scope, JSValue());
        scope.release();
        array->putDirectIndex(globalObject, index++, constructInternalProperty(globalObject, "string", stringIterator->iteratedString()));
        return array;
    }

    return jsUndefined();
}

JSValue JSInjectedScriptHost::proxyTargetValue(VM& vm, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    JSValue value = callFrame->uncheckedArgument(0);
    ProxyObject* proxy = jsDynamicCast<ProxyObject*>(vm, value);
    if (!proxy)
        return jsUndefined();

    JSObject* target = proxy->target();
    while (ProxyObject* proxy = jsDynamicCast<ProxyObject*>(vm, target))
        target = proxy->target();

    return target;
}

JSValue JSInjectedScriptHost::weakMapSize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    JSValue value = callFrame->uncheckedArgument(0);
    JSWeakMap* weakMap = jsDynamicCast<JSWeakMap*>(vm, value);
    if (!weakMap)
        return jsUndefined();

    return jsNumber(weakMap->size());
}

JSValue JSInjectedScriptHost::weakMapEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* weakMap = jsDynamicCast<JSWeakMap*>(vm, callFrame->uncheckedArgument(0));
    if (!weakMap)
        return jsUndefined();

    MarkedArgumentBuffer buffer;
    auto fetchCount = callFrame->argument(1).toInteger(globalObject);
    weakMap->takeSnapshot(buffer, fetchCount >= 0 ? static_cast<unsigned>(fetchCount) : 0);
    ASSERT(!buffer.hasOverflowed());

    JSArray* array = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValue());

    for (unsigned index = 0; index < buffer.size(); index += 2) {
        JSObject* entry = constructEmptyObject(globalObject);
        entry->putDirect(vm, Identifier::fromString(vm, "key"), buffer.at(index));
        entry->putDirect(vm, Identifier::fromString(vm, "value"), buffer.at(index + 1));
        array->putDirectIndex(globalObject, index / 2, entry);
        RETURN_IF_EXCEPTION(scope, JSValue());
    }

    return array;
}

JSValue JSInjectedScriptHost::weakSetSize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    JSValue value = callFrame->uncheckedArgument(0);
    JSWeakSet* weakSet = jsDynamicCast<JSWeakSet*>(vm, value);
    if (!weakSet)
        return jsUndefined();

    return jsNumber(weakSet->size());
}

JSValue JSInjectedScriptHost::weakSetEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* weakSet = jsDynamicCast<JSWeakSet*>(vm, callFrame->uncheckedArgument(0));
    if (!weakSet)
        return jsUndefined();

    MarkedArgumentBuffer buffer;
    auto fetchCount = callFrame->argument(1).toInteger(globalObject);
    weakSet->takeSnapshot(buffer, fetchCount >= 0 ? static_cast<unsigned>(fetchCount) : 0);
    ASSERT(!buffer.hasOverflowed());

    JSArray* array = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValue());

    for (unsigned index = 0; index < buffer.size(); ++index) {
        JSObject* entry = constructEmptyObject(globalObject);
        entry->putDirect(vm, Identifier::fromString(vm, "value"), buffer.at(index));
        array->putDirectIndex(globalObject, index, entry);
        RETURN_IF_EXCEPTION(scope, JSValue());
    }

    return array;
}

static JSObject* cloneArrayIteratorObject(JSGlobalObject* globalObject, VM& vm, JSArrayIterator* iteratorObject)
{
    JSArrayIterator* clone = JSArrayIterator::create(vm, globalObject->arrayIteratorStructure(), iteratorObject->iteratedObject(), iteratorObject->internalField(JSArrayIterator::Field::Kind).get());
    clone->internalField(JSArrayIterator::Field::Index).set(vm, clone, iteratorObject->internalField(JSArrayIterator::Field::Index).get());
    return clone;
}

static JSObject* cloneMapIteratorObject(JSGlobalObject* globalObject, VM& vm, JSObject* iteratorObject, JSValue mapBucket, JSValue iteratedObject)
{
    ASSERT(iteratorObject->type() == FinalObjectType);
    JSObject* clone = constructEmptyObject(globalObject, MapIteratorPrototype::create(vm, globalObject, MapIteratorPrototype::createStructure(vm, globalObject, globalObject->iteratorPrototype())));
    clone->putDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName(), iteratedObject);
    clone->putDirect(vm, vm.propertyNames->builtinNames().mapIteratorKindPrivateName(), iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapIteratorKindPrivateName()));
    clone->putDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName(), mapBucket);
    return clone;
}

static JSObject* cloneSetIteratorObject(JSGlobalObject* globalObject, VM& vm, JSObject* iteratorObject, JSValue setBucket, JSValue iteratedObject)
{
    ASSERT(iteratorObject->type() == FinalObjectType);
    JSObject* clone = constructEmptyObject(globalObject, SetIteratorPrototype::create(vm, globalObject, SetIteratorPrototype::createStructure(vm, globalObject, globalObject->iteratorPrototype())));
    clone->putDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName(), iteratedObject);
    clone->putDirect(vm, vm.propertyNames->builtinNames().setIteratorKindPrivateName(), iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setIteratorKindPrivateName()));
    clone->putDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName(), setBucket);
    return clone;
}

JSValue JSInjectedScriptHost::iteratorEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue iterator;
    JSValue value = callFrame->uncheckedArgument(0);
    if (JSStringIterator* stringIterator = jsDynamicCast<JSStringIterator*>(vm, value)) {
        if (globalObject->isStringPrototypeIteratorProtocolFastAndNonObservable())
            iterator = stringIterator->clone(globalObject);
    } else if (JSObject* iteratorObject = jsDynamicCast<JSObject*>(vm, value)) {
        if (auto* arrayIterator = jsDynamicCast<JSArrayIterator*>(vm, iteratorObject)) {
            JSObject* iteratedObject = arrayIterator->iteratedObject();
            if (isJSArray(iteratedObject)) {
                JSArray* array = jsCast<JSArray*>(iteratedObject);
                if (array->isIteratorProtocolFastAndNonObservable())
                    iterator = cloneArrayIteratorObject(globalObject, vm, arrayIterator);
            } else if (TypeInfo::isArgumentsType(iteratedObject->type())) {
                if (globalObject->isArrayPrototypeIteratorProtocolFastAndNonObservable())
                    iterator = cloneArrayIteratorObject(globalObject, vm, arrayIterator);
            }
        } else {
            JSValue iteratedObject = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().iteratedObjectPrivateName());
            if (JSValue mapBucket = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().mapBucketPrivateName())) {
                if (jsCast<JSMap*>(iteratedObject)->isIteratorProtocolFastAndNonObservable())
                    iterator = cloneMapIteratorObject(globalObject, vm, iteratorObject, mapBucket, iteratedObject);
            } else if (JSValue setBucket = iteratorObject->getDirect(vm, vm.propertyNames->builtinNames().setBucketPrivateName())) {
                if (jsCast<JSSet*>(iteratedObject)->isIteratorProtocolFastAndNonObservable())
                    iterator = cloneSetIteratorObject(globalObject, vm, iteratorObject, setBucket, iteratedObject);
            }
        }
    }
    RETURN_IF_EXCEPTION(scope, { });
    if (!iterator)
        return jsUndefined();

    IterationRecord iterationRecord = { iterator, iterator.get(globalObject, vm.propertyNames->next) };

    unsigned numberToFetch = 5;
    JSValue numberToFetchArg = callFrame->argument(1);
    double fetchDouble = numberToFetchArg.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (fetchDouble >= 0)
        numberToFetch = static_cast<unsigned>(fetchDouble);

    JSArray* array = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    for (unsigned i = 0; i < numberToFetch; ++i) {
        JSValue next = iteratorStep(globalObject, iterationRecord);
        if (UNLIKELY(scope.exception()) || next.isFalse())
            break;

        JSValue nextValue = iteratorValue(globalObject, next);
        RETURN_IF_EXCEPTION(scope, { });

        JSObject* entry = constructEmptyObject(globalObject);
        entry->putDirect(vm, Identifier::fromString(vm, "value"), nextValue);
        array->putDirectIndex(globalObject, i, entry);
        if (UNLIKELY(scope.exception())) {
            scope.release();
            iteratorClose(globalObject, iterationRecord);
            break;
        }
    }

    return array;
}

static bool checkForbiddenPrototype(JSGlobalObject* globalObject, JSValue value, JSValue proto)
{
    if (value == proto)
        return true;

    // Check that the prototype chain of proto hasn't been modified to include value.
    return JSObject::defaultHasInstance(globalObject, proto, value);
}

JSValue JSInjectedScriptHost::queryInstances(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue prototypeOrConstructor = callFrame->uncheckedArgument(0);
    if (!prototypeOrConstructor.isObject())
        return throwTypeError(globalObject, scope, "queryInstances first argument must be an object."_s);

    JSObject* object = asObject(prototypeOrConstructor);
    if (object->inherits<ProxyObject>(vm))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with a Proxy."_s);

    JSValue prototype = object;

    PropertySlot prototypeSlot(object, PropertySlot::InternalMethodType::VMInquiry);
    if (object->getPropertySlot(globalObject, vm.propertyNames->prototype, prototypeSlot)) {
        RETURN_IF_EXCEPTION(scope, { });
        if (prototypeSlot.isValue()) {
            JSValue prototypeValue = prototypeSlot.getValue(globalObject, vm.propertyNames->prototype);
            if (prototypeValue.isObject()) {
                prototype = prototypeValue;
                object = asObject(prototype);
            }
        }
    }

    if (object->inherits<ProxyObject>(vm) || prototype.inherits<ProxyObject>(vm))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with a Proxy."_s);

    // FIXME: implement a way of distinguishing between internal and user-created objects.
    if (checkForbiddenPrototype(globalObject, object, globalObject->objectPrototype()))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with Object."_s);
    if (checkForbiddenPrototype(globalObject, object, globalObject->functionPrototype()))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with Function."_s);
    if (checkForbiddenPrototype(globalObject, object, globalObject->arrayPrototype()))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with Array."_s);
    if (checkForbiddenPrototype(globalObject, object, globalObject->mapPrototype()))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with Map."_s);
    if (checkForbiddenPrototype(globalObject, object, globalObject->jsSetPrototype()))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with Set."_s);
    if (checkForbiddenPrototype(globalObject, object, globalObject->promisePrototype()))
        return throwTypeError(globalObject, scope, "queryInstances cannot be called with Promise."_s);

    sanitizeStackForVM(vm);
    vm.heap.collectNow(Sync, CollectionScope::Full);

    JSArray* array = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    {
        HeapIterationScope iterationScope(vm.heap);
        vm.heap.objectSpace().forEachLiveCell(iterationScope, [&] (HeapCell* cell, HeapCell::Kind kind) {
            if (!isJSCellKind(kind))
                return IterationStatus::Continue;

            JSValue value(static_cast<JSCell*>(cell));
            if (value.inherits<ProxyObject>(vm))
                return IterationStatus::Continue;

            if (JSObject::defaultHasInstance(globalObject, value, prototype))
                array->putDirectIndex(globalObject, array->length(), value);

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

    void analyzeEdge(JSCell* from, JSCell* to, SlotVisitor::RootMarkReason reason) override
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
    void analyzePropertyNameEdge(JSCell* from, JSCell* to, UniquedStringImpl*) override { analyzeEdge(from, to, SlotVisitor::RootMarkReason::None); }
    void analyzeVariableNameEdge(JSCell* from, JSCell* to, UniquedStringImpl*) override { analyzeEdge(from, to, SlotVisitor::RootMarkReason::None); }
    void analyzeIndexEdge(JSCell* from, JSCell* to, uint32_t) override { analyzeEdge(from, to, SlotVisitor::RootMarkReason::None); }

    void analyzeNode(JSCell*) override { }
    void setOpaqueRootReachabilityReasonForCell(JSCell*, const char*) override { }
    void setWrappedObjectForCell(JSCell*, void*) override { }
    void setLabelForCell(JSCell*, const String&) override { }

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

JSValue JSInjectedScriptHost::queryHolders(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return jsUndefined();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->uncheckedArgument(0);
    if (!target.isObject())
        return throwTypeError(globalObject, scope, "queryHolders first argument must be an object."_s);

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    {
        DeferGC deferGC(vm.heap);
        PreventCollectionScope preventCollectionScope(vm.heap);
        sanitizeStackForVM(vm);

        HeapHolderFinder holderFinder(vm.ensureHeapProfiler(), target.asCell());

        auto holders = copyToVector(holderFinder.holders());
        std::sort(holders.begin(), holders.end());
        for (auto* holder : holders)
            result->putDirectIndex(globalObject, result->length(), holder);
    }

    return result;
}

} // namespace Inspector
