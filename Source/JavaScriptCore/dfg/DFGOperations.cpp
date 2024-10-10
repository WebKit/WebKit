/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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
#include "DFGOperations.h"

#include "ArrayPrototypeInlines.h"
#include "ButterflyInlines.h"
#include "CacheableIdentifierInlines.h"
#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CodeBlockInlines.h"
#include "CommonSlowPathsInlines.h"
#include "DFGDriver.h"
#include "DFGJITCode.h"
#include "DFGToFTLDeferredCompilationCallback.h"
#include "DFGToFTLForOSREntryDeferredCompilationCallback.h"
#include "DateInstance.h"
#include "DefinePropertyAttributes.h"
#include "DirectArguments.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLOSREntry.h"
#include "FrameTracers.h"
#include "HasOwnPropertyCache.h"
#include "Interpreter.h"
#include "InterpreterInlines.h"
#include "IntlCollator.h"
#include "JITCode.h"
#include "JITWorklist.h"
#include "JSArrayInlines.h"
#include "JSArrayIterator.h"
#include "JSAsyncGenerator.h"
#include "JSBigInt.h"
#include "JSBoundFunction.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSImmutableButterfly.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSIteratorHelper.h"
#include "JSLexicalEnvironment.h"
#include "JSMap.h"
#include "JSMapIterator.h"
#include "JSPromiseConstructor.h"
#include "JSPropertyNameEnumerator.h"
#include "JSSet.h"
#include "JSSetIterator.h"
#include "JSWeakMapInlines.h"
#include "JSWeakSet.h"
#include "NumberConstructor.h"
#include "NumberObject.h"
#include "NumberPrototype.h"
#include "ObjectConstructorInlines.h"
#include "ObjectPrototypeInlines.h"
#include "Operations.h"
#include "ParseInt.h"
#include "ReflectObject.h"
#include "RegExpGlobalDataInlines.h"
#include "RegExpMatchesArray.h"
#include "RegExpObjectInlines.h"
#include "Repatch.h"
#include "ResourceExhaustion.h"
#include "ScopedArguments.h"
#include "StringConstructor.h"
#include "StringPrototypeInlines.h"
#include "SuperSampler.h"
#include "Symbol.h"
#include "TypeProfilerLog.h"
#include "VMEntryScopeInlines.h"
#include "VMInlines.h"
#include "VMTrapsInlines.h"
#include "WeakMapImplInlines.h"
#include "WeakMapPrototype.h"
#include "WeakSetPrototype.h"
#include <wtf/text/MakeString.h>

#if ENABLE(JIT)
#if ENABLE(DFG_JIT)

IGNORE_WARNINGS_BEGIN("frame-address")

namespace JSC { namespace DFG {

template<bool strict, bool direct>
ALWAYS_INLINE static void putByValCellInternal(JSGlobalObject* globalObject, VM& vm, JSCell* base, PropertyName propertyName, JSValue value)
{
    PutPropertySlot slot(base, strict);
    if constexpr (direct) {
        RELEASE_ASSERT(base->isObject());
        JSObject* baseObject = asObject(base);
        if (std::optional<uint32_t> index = parseIndex(propertyName)) {
            baseObject->putDirectIndex(globalObject, index.value(), value, 0, strict ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
            return;
        }
        CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, propertyName, value, slot);
        return;
    }
    base->putInline(globalObject, propertyName, value, slot);
}

template<bool strict, bool direct>
ALWAYS_INLINE static void putByValCellStringInternal(JSGlobalObject* globalObject, VM& vm, JSCell* base, JSString* property, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = property->toIdentifier(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    scope.release();
    putByValCellInternal<strict, direct>(globalObject, vm, base, propertyName, value);
}

template<typename ViewClass>
char* newTypedArrayWithSize(JSGlobalObject* globalObject, VM& vm, Structure* structure, intptr_t size, char* vector)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (size < 0) {
        throwException(globalObject, scope, createRangeError(globalObject, "Requested length is negative"_s));
        return nullptr;
    }

    static_assert(std::numeric_limits<intptr_t>::max() <= std::numeric_limits<size_t>::max());
    size_t unsignedSize = static_cast<size_t>(size);

    if (vector)
        return bitwise_cast<char*>(ViewClass::createWithFastVector(globalObject, structure, unsignedSize, vector));

    RELEASE_AND_RETURN(scope, bitwise_cast<char*>(ViewClass::create(globalObject, structure, unsignedSize)));
}

template <bool strict>
static ALWAYS_INLINE void putWithThis(JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, const Identifier& ident)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue thisVal = JSValue::decode(encodedThis);
    JSValue putValue = JSValue::decode(encodedValue);
    PutPropertySlot slot(thisVal, strict);
    baseValue.putInline(globalObject, ident, putValue, slot);
}

static ALWAYS_INLINE EncodedJSValue parseIntResult(double input)
{
    int asInt = static_cast<int>(input);
    if (LIKELY(static_cast<double>(asInt) == input && (asInt || !std::signbit(input))))
        return JSValue::encode(jsNumber(asInt));
    return JSValue::encode(jsNumber(input));
}

ALWAYS_INLINE static JSValue getByValObject(JSGlobalObject* globalObject, VM& vm, JSObject* base, PropertyName propertyName)
{
    Structure& structure = *base->structure();
    if (JSCell::canUseFastGetOwnProperty(structure)) {
        if (JSValue result = base->fastGetOwnProperty(vm, structure, propertyName))
            return result;
    }
    return base->get(globalObject, propertyName);
}

JSC_DEFINE_JIT_OPERATION(operationToThis, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(JSValue::decode(encodedOp).toThis(globalObject, ECMAMode::sloppy())));
}

JSC_DEFINE_JIT_OPERATION(operationToThisStrict, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(JSValue::decode(encodedOp).toThis(globalObject, ECMAMode::strict())));
}

JSC_DEFINE_JIT_OPERATION(operationObjectKeys, JSArray*, (JSGlobalObject* globalObject, EncodedJSValue encodedObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(encodedObject).toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    scope.release();
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude));
}

JSC_DEFINE_JIT_OPERATION(operationObjectKeysObject, JSArray*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude));
}

JSC_DEFINE_JIT_OPERATION(operationObjectGetOwnPropertyNames, JSArray*, (JSGlobalObject* globalObject, EncodedJSValue encodedObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(encodedObject).toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    scope.release();
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include));
}

JSC_DEFINE_JIT_OPERATION(operationObjectGetOwnPropertyNamesObject, JSArray*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include));
}

JSC_DEFINE_JIT_OPERATION(operationObjectGetOwnPropertySymbols, JSArray*, (JSGlobalObject* globalObject, EncodedJSValue encodedObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(encodedObject).toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    scope.release();
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::Symbols, DontEnumPropertiesMode::Include));
}

JSC_DEFINE_JIT_OPERATION(operationObjectGetOwnPropertySymbolsObject, JSArray*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::Symbols, DontEnumPropertiesMode::Include));
}

JSC_DEFINE_JIT_OPERATION(operationObjectCreate, JSCell*, (JSGlobalObject* globalObject, EncodedJSValue encodedPrototype))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue prototype = JSValue::decode(encodedPrototype);

    if (!prototype.isObject() && !prototype.isNull()) {
        throwVMTypeError(globalObject, scope, "Object prototype may only be an Object or null."_s);
        OPERATION_RETURN(scope, nullptr);
    }

    if (prototype.isObject())
        OPERATION_RETURN(scope, constructEmptyObject(globalObject, asObject(prototype)));
    OPERATION_RETURN(scope, constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationObjectCreateObject, JSCell*, (JSGlobalObject* globalObject, JSObject* prototype))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, constructEmptyObject(globalObject, prototype));
}

JSC_DEFINE_JIT_OPERATION(operationObjectAssignObject, void, (JSGlobalObject* globalObject, JSObject* target, JSObject* source))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto* targetObject = jsDynamicCast<JSFinalObject*>(target); targetObject && targetObject->canPerformFastPutInlineExcludingProto() && targetObject->isStructureExtensible()) {
        Vector<RefPtr<UniquedStringImpl>, 8> properties;
        MarkedArgumentBuffer values;
        if (!source->staticPropertiesReified()) {
            source->reifyAllStaticProperties(globalObject);
            OPERATION_RETURN_IF_EXCEPTION(scope);
        }

        // |source| Structure does not have any getters. And target can perform fast put.
        // So enumerating properties and putting properties are non observable.

        // FIXME: It doesn't seem like we should have to do this in two phases, but
        // we're running into crashes where it appears that source is transitioning
        // under us, and even ends up in a state where it has a null butterfly. My
        // leading hypothesis here is that we fire some value replacement watchpoint
        // that ends up transitioning the structure underneath us.
        // https://bugs.webkit.org/show_bug.cgi?id=187837

        // Do not clear since Vector::clear shrinks the backing store.
        bool objectAssignFastSucceeded = objectAssignFast(globalObject, targetObject, source, properties, values);
        OPERATION_RETURN_IF_EXCEPTION(scope);
        if (objectAssignFastSucceeded)
            OPERATION_RETURN(scope);
    }

    scope.release();
    objectAssignGeneric(globalObject, vm, target, source);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationObjectAssignUntyped, void, (JSGlobalObject* globalObject, JSObject* target, EncodedJSValue encodedSource))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue sourceValue = JSValue::decode(encodedSource);
    if (sourceValue.isUndefinedOrNull())
        OPERATION_RETURN(scope);
    JSObject* source = sourceValue.toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);

    if (auto* targetObject = jsDynamicCast<JSFinalObject*>(target); targetObject && targetObject->canPerformFastPutInlineExcludingProto() && targetObject->isStructureExtensible()) {
        if (!source->staticPropertiesReified()) {
            source->reifyAllStaticProperties(globalObject);
            OPERATION_RETURN_IF_EXCEPTION(scope);
        }

        Vector<RefPtr<UniquedStringImpl>, 8> properties;
        MarkedArgumentBuffer values;
        bool objectAssignFastSucceeded = objectAssignFast(globalObject, targetObject, source, properties, values);
        OPERATION_RETURN_IF_EXCEPTION(scope);
        if (objectAssignFastSucceeded)
            OPERATION_RETURN(scope);
    }

    scope.release();
    objectAssignGeneric(globalObject, vm, target, source);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationObjectToStringUntyped, JSString*, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = JSValue::decode(encodedValue).toThis(globalObject, ECMAMode::strict());
    OPERATION_RETURN(scope, objectPrototypeToString(globalObject, thisValue));
}

JSC_DEFINE_JIT_OPERATION(operationObjectToStringObjectSlow, JSString*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = JSValue(object).toThis(globalObject, ECMAMode::strict());
    OPERATION_RETURN(scope, objectPrototypeToString(globalObject, thisValue));
}

JSC_DEFINE_JIT_OPERATION(operationReflectOwnKeys, JSArray*, (JSGlobalObject* globalObject, EncodedJSValue encodedObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(encodedObject).getObject();
    if (UNLIKELY(!object)) {
        throwTypeError(globalObject, scope, ReflectOwnKeysNonObjectArgumentError);
        OPERATION_RETURN(scope, nullptr);
    }

    scope.release();
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::StringsAndSymbols, DontEnumPropertiesMode::Include));
}

JSC_DEFINE_JIT_OPERATION(operationReflectOwnKeysObject, JSArray*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, ownPropertyKeys(globalObject, object, PropertyNameMode::StringsAndSymbols, DontEnumPropertiesMode::Include));
}

JSC_DEFINE_JIT_OPERATION(operationCreateThis, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor, uint32_t inlineCapacity))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (constructor->type() == JSFunctionType && jsCast<JSFunction*>(constructor)->canUseAllocationProfiles()) {
        JSObject* result;
        {
            DeferTermination deferScope(vm);
            auto rareData = jsCast<JSFunction*>(constructor)->ensureRareDataAndObjectAllocationProfile(globalObject, inlineCapacity);
            scope.releaseAssertNoException();
            ObjectAllocationProfileWithPrototype* allocationProfile = rareData->objectAllocationProfile();
            Structure* structure = allocationProfile->structure();
            result = constructEmptyObject(vm, structure);
            if (structure->hasPolyProto()) {
                JSObject* prototype = allocationProfile->prototype();
                ASSERT(prototype == jsCast<JSFunction*>(constructor)->prototypeForConstruction(vm, globalObject));
                result->putDirectOffset(vm, knownPolyProtoOffset, prototype);
                prototype->didBecomePrototype(vm);
                ASSERT_WITH_MESSAGE(!hasIndexedProperties(result->indexingType()), "We rely on JSFinalObject not starting out with an indexing type otherwise we would potentially need to convert to slow put storage");
            }
        }
        OPERATION_RETURN(scope, result);
    }

    JSValue proto = constructor->get(globalObject, vm.propertyNames->prototype);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    if (proto.isObject())
        OPERATION_RETURN(scope, constructEmptyObject(globalObject, asObject(proto)));
    JSGlobalObject* functionGlobalObject = getFunctionRealm(globalObject, constructor);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    OPERATION_RETURN(scope, constructEmptyObject(functionGlobalObject));
}

JSC_DEFINE_JIT_OPERATION(operationCreatePromise, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, promiseStructure, constructor, globalObject->promiseConstructor());
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    OPERATION_RETURN(scope, JSPromise::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCreateInternalPromise, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, internalPromiseStructure, constructor, globalObject->internalPromiseConstructor());
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    OPERATION_RETURN(scope, JSInternalPromise::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCreateGenerator, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = InternalFunction::createSubclassStructure(globalObject, constructor, globalObject->generatorStructure());
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    OPERATION_RETURN(scope, JSGenerator::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCreateAsyncGenerator, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = InternalFunction::createSubclassStructure(globalObject, constructor, globalObject->asyncGeneratorStructure());
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    OPERATION_RETURN(scope, JSAsyncGenerator::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCallObjectConstructor, JSCell*, (JSGlobalObject* globalObject, EncodedJSValue encodedTarget))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedTarget);
    ASSERT(!value.isObject());

    if (value.isUndefinedOrNull())
        OPERATION_RETURN(scope, constructEmptyObject(globalObject, globalObject->objectPrototype()));
    OPERATION_RETURN(scope, value.toObject(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationToObject, JSCell*, (JSGlobalObject* globalObject, EncodedJSValue encodedTarget, UniquedStringImpl* errorMessage))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedTarget);
    ASSERT(!value.isObject());

    if (UNLIKELY(value.isUndefinedOrNull())) {
        if (errorMessage && errorMessage->length()) {
            throwVMTypeError(globalObject, scope, errorMessage);
            OPERATION_RETURN(scope, nullptr);
        }
    }

    OPERATION_RETURN(scope, value.toObject(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationValueMod, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsRemainder(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationInc, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsInc(globalObject, JSValue::decode(encodedOp))));
}

JSC_DEFINE_JIT_OPERATION(operationDec, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsDec(globalObject, JSValue::decode(encodedOp))));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitNot, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsBitwiseNot(globalObject, JSValue::decode(encodedOp))));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitAnd, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsBitwiseAnd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitOr, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsBitwiseOr(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitXor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsBitwiseXor(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitLShift, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsLShift(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitRShift, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsRShift(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitURShift, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsURShift(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddNotNumber, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    OPERATION_RETURN(scope, JSValue::encode(jsAddNonNumber(globalObject, op1, op2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueDiv, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsDiv(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValuePow, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsPow(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationArithAbs, double, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, PNaN);
    OPERATION_RETURN(scope, std::abs(a));
}

JSC_DEFINE_JIT_OPERATION(operationArithClz32, UCPUStrictInt32, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    uint32_t value = op1.toUInt32(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);
    OPERATION_RETURN(scope, toUCPUStrictInt32(clz(value)));
}

JSC_DEFINE_JIT_OPERATION(operationArithFRound, double, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, PNaN);
    OPERATION_RETURN(scope, static_cast<float>(a));
}

JSC_DEFINE_JIT_OPERATION(operationArithF16Round, double, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, PNaN);
    OPERATION_RETURN(scope, static_cast<double>(Float16 { a }));
}

#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
JSC_DEFINE_JIT_OPERATION(operationArith##capitalizedName, double, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1)) \
{ \
    VM& vm = globalObject->vm(); \
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm); \
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame); \
    auto scope = DECLARE_THROW_SCOPE(vm); \
    JSValue op1 = JSValue::decode(encodedOp1); \
    double result = op1.toNumber(globalObject); \
    OPERATION_RETURN_IF_EXCEPTION(scope, PNaN); \
    OPERATION_RETURN(scope, JSC::Math::lowerName(result)); \
}
    FOR_EACH_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY

JSC_DEFINE_JIT_OPERATION(operationArithSqrt, double, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, PNaN);
    OPERATION_RETURN(scope, sqrt(a));
}

JSC_DEFINE_JIT_OPERATION(operationArithRound, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(Math::roundDouble(valueOfArgument))));
}

JSC_DEFINE_JIT_OPERATION(operationArithFloor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(floor(valueOfArgument))));
}

JSC_DEFINE_JIT_OPERATION(operationArithCeil, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(ceil(valueOfArgument))));
}

JSC_DEFINE_JIT_OPERATION(operationArithTrunc, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double truncatedValueOfArgument = argument.toIntegerPreserveNaN(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(truncatedValueOfArgument)));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationArithMinMultipleDouble, double, (const double* buffer, unsigned elementCount))
{
    ASSERT(0 < elementCount);
    double result = buffer[0];
    for (unsigned index = 1; index < elementCount; ++index)
        result = Math::jsMinDouble(result, buffer[index]);
    return result;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationArithMaxMultipleDouble, double, (const double* buffer, unsigned elementCount))
{
    ASSERT(0 < elementCount);
    double result = buffer[0];
    for (unsigned index = 1; index < elementCount; ++index)
        result = Math::jsMaxDouble(result, buffer[index]);
    return result;
}

ALWAYS_INLINE EncodedJSValue getByValCellInt(JSGlobalObject* globalObject, VM& vm, JSCell* base, int32_t index)
{
    if (index < 0) {
        // When index is negative, -1 is the most common case. Let's handle it separately.
        if (LIKELY(index == -1)) {
            if (auto* array = jsDynamicCast<JSArray*>(base); LIKELY(array && array->definitelyNegativeOneMiss()))
                return JSValue::encode(jsUndefined());
            return JSValue::encode(JSValue(base).get(globalObject, vm.propertyNames->negativeOneIdentifier));
        }

        // Go the slowest way possible because negative indices don't use indexed storage.
        return JSValue::encode(JSValue(base).get(globalObject, Identifier::from(vm, index)));
    }

    // Use this since we know that the value is out of bounds.
    return JSValue::encode(JSValue(base).get(globalObject, static_cast<unsigned>(index)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValObjectInt, EncodedJSValue, (JSGlobalObject* globalObject, JSObject* base, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, getByValCellInt(globalObject, vm, base, index));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValStringInt, EncodedJSValue, (JSGlobalObject* globalObject, JSString* base, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, getByValCellInt(globalObject, vm, base, index));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValObjectString, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* base, JSCell* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asString(string)->toIdentifier(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    OPERATION_RETURN(scope, JSValue::encode(getByValObject(globalObject, vm, asObject(base), propertyName)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValObjectSymbol, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* base, JSCell* symbol))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asSymbol(symbol)->privateName();
    OPERATION_RETURN(scope, JSValue::encode(getByValObject(globalObject, vm, asObject(base), propertyName)));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellStringStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    putByValCellStringInternal<true, false>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellStringSloppy, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    putByValCellStringInternal<false, false>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellSymbolStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<true, false>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellSymbolSloppy, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<false, false>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue), true);
        OPERATION_RETURN(scope);
    }
    
    PutPropertySlot slot(object, true);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValBeyondArrayBoundsSloppy, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue), false);
        OPERATION_RETURN(scope);
    }
    
    PutPropertySlot slot(object, false);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), jsValue, true);
        OPERATION_RETURN(scope);
    }
    
    PutPropertySlot slot(object, true);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), jsValue, slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValBeyondArrayBoundsSloppy, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), jsValue, false);
        OPERATION_RETURN(scope);
    }
    
    PutPropertySlot slot(object, false);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), jsValue, slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValDirectBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);

    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), jsValue, 0, PutDirectIndexShouldThrow);
        OPERATION_RETURN(scope);
    }

    PutPropertySlot slot(object, true);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), jsValue, slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValDirectBeyondArrayBoundsSloppy, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);

    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), jsValue);
        OPERATION_RETURN(scope);
    }

    PutPropertySlot slot(object, false);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), jsValue, slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellStringStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    putByValCellStringInternal<true, true>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellStringSloppy, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    putByValCellStringInternal<false, true>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellSymbolStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<true, true>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellSymbolSloppy, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<false, true>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue), 0, PutDirectIndexShouldThrow);
        OPERATION_RETURN(scope);
    }
    
    PutPropertySlot slot(object, true);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectBeyondArrayBoundsSloppy, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue));
        OPERATION_RETURN(scope);
    }
    
    PutPropertySlot slot(object, false);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationArrayPush, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    array->pushInline(globalObject, JSValue::decode(encodedValue));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(array->length())));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPushDouble, EncodedJSValue, (JSGlobalObject* globalObject, double value, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    array->pushInline(globalObject, JSValue(JSValue::EncodeAsDouble, value));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(array->length())));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPushMultiple, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array, void* buffer, int32_t elementCount))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(buffer), elementCount);


    // We assume that multiple JSArray::push calls with ArrayWithInt32/ArrayWithContiguous do not cause JS traps.
    // If it can cause any JS interactions, we can call the caller JS function of this function and overwrite the
    // content of ScratchBuffer. If the IndexingType is now ArrayWithInt32/ArrayWithContiguous, we can ensure
    // that there is no indexed accessors in this object and its prototype chain.
    //
    // ArrayWithArrayStorage is also OK. It can have indexed accessors. But if you define an indexed accessor, the array's length
    // becomes larger than that index. So Array#push never overlaps with this accessor. So accessors are never called unless
    // the IndexingType is ArrayWithSlowPutArrayStorage which could have an indexed accessor in a prototype chain.
    RELEASE_ASSERT(!shouldUseSlowPut(array->indexingType()));

    EncodedJSValue* values = static_cast<EncodedJSValue*>(buffer);
    for (int32_t i = 0; i < elementCount; ++i) {
        array->pushInline(globalObject, JSValue::decode(values[i]));
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(array->length())));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPushDoubleMultiple, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array, void* buffer, int32_t elementCount))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    // Don't need a ActiveScratchBufferScope here because the scratch buffer only contains double values for this call.
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We assume that multiple JSArray::push calls with ArrayWithDouble do not cause JS traps.
    // If it can cause any JS interactions, we can call the caller JS function of this function and overwrite the
    // content of ScratchBuffer. If the IndexingType is now ArrayWithDouble, we can ensure
    // that there is no indexed accessors in this object and its prototype chain.
    ASSERT(array->indexingMode() == ArrayWithDouble);

    double* values = static_cast<double*>(buffer);
    for (int32_t i = 0; i < elementCount; ++i) {
        array->pushInline(globalObject, JSValue(JSValue::EncodeAsDouble, values[i]));
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(array->length())));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPushMultipleSlow, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array, void* buffer, int32_t elementCount))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(buffer), elementCount);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // If JS interaction happens, and if it invokes DFG functions, then scratch buffer can be overridden. Thus we first need to copy all of them into the local buffer.
    MarkedArgumentBuffer arguments;
    arguments.ensureCapacity(elementCount);

    EncodedJSValue* values = static_cast<EncodedJSValue*>(buffer);
    for (int32_t i = 0; i < elementCount; ++i)
        arguments.append(JSValue::decode(values[i]));

    if (UNLIKELY(arguments.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        OPERATION_RETURN(scope, encodedJSValue());
    }

    for (int32_t i = 0; i < elementCount; ++i) {
        array->pushInline(globalObject, arguments.at(i));
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    OPERATION_RETURN(scope, JSValue::encode(jsNumber(array->length())));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPop, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, JSValue::encode(array->pop(globalObject)));
}
        
JSC_DEFINE_JIT_OPERATION(operationArrayPopAndRecoverLength, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    array->butterfly()->setPublicLength(array->butterfly()->publicLength() + 1);
    
    OPERATION_RETURN(scope, JSValue::encode(array->pop(globalObject)));
}

template<bool ignoreResult>
static ALWAYS_INLINE EncodedJSValue arraySpliceImpl(JSGlobalObject* globalObject, JSArray* base, int32_t start, int32_t deleteCount, EncodedJSValue* buffer, unsigned itemCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    MarkedArgumentBuffer insertions;
    insertions.ensureCapacity(itemCount);
    if (UNLIKELY(insertions.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    for (unsigned i = 0; i < itemCount; ++i)
        insertions.appendWithCrashOnOverflow(JSValue::decode(buffer[i]));

    uint64_t length = base->length();
    uint64_t actualStart = 0;
    int64_t startInt64 = start;
    if (startInt64 < 0) {
        startInt64 += length;
        actualStart = startInt64 < 0 ? 0 : static_cast<uint64_t>(startInt64);
    } else
        actualStart = std::min(static_cast<uint64_t>(startInt64), length);

    uint64_t actualDeleteCount = 0;
    if (deleteCount < 0)
        actualDeleteCount = 0;
    else if (deleteCount > static_cast<int64_t>(length - actualStart))
        actualDeleteCount = length - actualStart;
    else
        actualDeleteCount = static_cast<uint64_t>(deleteCount);

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, base, actualDeleteCount);
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (speciesResult.first == SpeciesConstructResult::Exception)
        return { };

    JSValue result;
    if (LIKELY(speciesResult.first == SpeciesConstructResult::FastPath)) {
        // DFG / FTL tells the hint that the result array is not used at all.
        // If this condition is met, we can skip creation of this array completely.
        auto canFastSliceWithoutSideEffect = [](JSGlobalObject* globalObject, JSArray* base, uint64_t count) {
            auto arrayType = base->indexingType() | IsArray;
            switch (arrayType) {
            case ArrayWithDouble:
            case ArrayWithInt32:
            case ArrayWithContiguous: {
                if (count >= MIN_SPARSE_ARRAY_INDEX || base->structure()->holesMustForwardToPrototype(base))
                    return false;

                Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(arrayType);
                if (UNLIKELY(hasAnyArrayStorage(resultStructure->indexingType())))
                    return false;

                return true;
            }
            case ArrayWithArrayStorage: {
                if (count >= MIN_SPARSE_ARRAY_INDEX || base->structure()->holesMustForwardToPrototype(base))
                    return false;

                Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);
                if (UNLIKELY(hasAnyArrayStorage(resultStructure->indexingType())))
                    return false;
                return true;
            }
            case ArrayWithUndecided: {
                return true;
            }
            default:
                return false;
            }
        };

        if (ignoreResult && canFastSliceWithoutSideEffect(globalObject, base, actualDeleteCount))
            result = jsUndefined();
        else {
            result = JSArray::fastSlice(globalObject, base, actualStart, actualDeleteCount);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    if (UNLIKELY(!result)) {
        JSObject* resultObject = nullptr;
        if (speciesResult.first == SpeciesConstructResult::CreatedObject)
            resultObject = speciesResult.second;
        else {
            if (UNLIKELY(actualDeleteCount > std::numeric_limits<uint32_t>::max())) {
                throwRangeError(globalObject, scope, LengthExceededTheMaximumArrayLengthError);
                return { };
            }
            resultObject = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), static_cast<uint32_t>(actualDeleteCount));
            if (UNLIKELY(!resultObject)) {
                throwOutOfMemoryError(globalObject, scope);
                return { };
            }
        }
        for (uint64_t k = 0; k < actualDeleteCount; ++k) {
            JSValue v = getProperty(globalObject, base, k + actualStart);
            RETURN_IF_EXCEPTION(scope, { });
            if (UNLIKELY(!v))
                continue;
            resultObject->putDirectIndex(globalObject, k, v, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, { });
        }
        setLength(globalObject, vm, resultObject, actualDeleteCount);
        RETURN_IF_EXCEPTION(scope, { });
        result = resultObject;
    }

    if (itemCount < actualDeleteCount) {
        shift<JSArray::ShiftCountForSplice>(globalObject, base, actualStart, actualDeleteCount, itemCount, length);
        RETURN_IF_EXCEPTION(scope, { });
    } else if (itemCount > actualDeleteCount) {
        unshift(globalObject, base, actualStart, actualDeleteCount, itemCount, length);
        RETURN_IF_EXCEPTION(scope, { });
    }

    for (unsigned i = 0; i < itemCount; ++i) {
        base->putByIndexInline(globalObject, i + actualStart, insertions.at(i), true);
        RETURN_IF_EXCEPTION(scope, { });
    }

    scope.release();
    setLength(globalObject, vm, base, length - actualDeleteCount + itemCount);
    return JSValue::encode(result);
}

JSC_DEFINE_JIT_OPERATION(operationArraySplice, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* base, int32_t start, int32_t deleteCount, EncodedJSValue* buffer, unsigned itemCount))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ActiveScratchBufferScope activeScratchBufferScope(buffer ? ScratchBuffer::fromData(buffer) : nullptr, itemCount);
    auto scope = DECLARE_THROW_SCOPE(vm);
    EncodedJSValue result = arraySpliceImpl</* ignore result */ false>(globalObject, base, start, deleteCount, buffer, itemCount);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationArraySpliceIgnoreResult, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* base, int32_t start, int32_t deleteCount, EncodedJSValue* buffer, unsigned itemCount))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ActiveScratchBufferScope activeScratchBufferScope(buffer ? ScratchBuffer::fromData(buffer) : nullptr, itemCount);
    auto scope = DECLARE_THROW_SCOPE(vm);
    EncodedJSValue result = arraySpliceImpl</* ignore result */ true>(globalObject, base, start, deleteCount, buffer, itemCount);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationRegExpExecString, EncodedJSValue, (JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* argument))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, JSValue::encode(regExpObject->execInline(globalObject, argument)));
}
        
JSC_DEFINE_JIT_OPERATION(operationRegExpExec, EncodedJSValue, (JSGlobalObject* globalObject, RegExpObject* regExpObject, EncodedJSValue encodedArgument))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue argument = JSValue::decode(encodedArgument);

    JSString* input = argument.toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        OPERATION_RETURN(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(regExpObject->execInline(globalObject, input)));
}
        
JSC_DEFINE_JIT_OPERATION(operationRegExpExecGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedArgument))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(encodedBase);
    JSValue argument = JSValue::decode(encodedArgument);
    
    auto* regexp = jsDynamicCast<RegExpObject*>(base);
    if (UNLIKELY(!regexp))
        OPERATION_RETURN(scope, throwVMTypeError(globalObject, scope, "Builtin RegExp exec can only be called on a RegExp object"_s));

    JSString* input = argument.toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        OPERATION_RETURN(scope, JSValue::encode(jsUndefined()));
    OPERATION_RETURN(scope, JSValue::encode(regexp->exec(globalObject, input)));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpExecNonGlobalOrSticky, EncodedJSValue, (JSGlobalObject* globalObject, RegExp* regExp, JSString* string))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto input = string->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned lastIndex = 0;
    MatchResult result;
    JSArray* array = createRegExpMatchesArray(vm, globalObject, string, input, regExp, lastIndex, result);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!array)
        OPERATION_RETURN(scope, JSValue::encode(jsNull()));

    globalObject->regExpGlobalData().recordMatch(vm, globalObject, regExp, string, result);
    OPERATION_RETURN(scope, JSValue::encode(array));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpMatchFastString, EncodedJSValue, (JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* argument))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!regExpObject->regExp()->global())
        OPERATION_RETURN(scope, JSValue::encode(regExpObject->execInline(globalObject, argument)));
    OPERATION_RETURN(scope, JSValue::encode(regExpObject->matchGlobal(globalObject, argument)));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpMatchFastGlobalString, EncodedJSValue, (JSGlobalObject* globalObject, RegExp* regExp, JSString* string))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(regExp->global());

    auto s = string->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (regExp->eitherUnicode()) {
        unsigned stringLength = s->length();
        OPERATION_RETURN(scope, JSValue::encode(collectMatches(
            vm, globalObject, string, s, regExp,
            [&](size_t end) ALWAYS_INLINE_LAMBDA {
                return advanceStringUnicode(s, stringLength, end);
            })));
    }

    OPERATION_RETURN(scope, JSValue::encode(collectMatches(
        vm, globalObject, string, s, regExp,
        [](size_t end) ALWAYS_INLINE_LAMBDA {
            return end + 1;
        })));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntGenericNoRadix, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedValue);
    if (value.isNumber()) {
        if (auto result = parseIntDouble(value.asNumber()))
            OPERATION_RETURN(scope, parseIntResult(result.value()));
    }

    OPERATION_RETURN(scope, toStringView(globalObject, value, [&] (StringView view) {
        // This version is as if radix was undefined. Hence, undefined.toNumber() === 0.
        return parseIntResult(parseInt(view, 0));
    }));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntStringNoRadix, EncodedJSValue, (JSGlobalObject* globalObject, JSString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto view = string->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // This version is as if radix was undefined. Hence, undefined.toNumber() === 0.
    OPERATION_RETURN(scope, parseIntResult(parseInt(view, 0)));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntDoubleNoRadix, EncodedJSValue, (JSGlobalObject* globalObject, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto result = parseIntDouble(value))
        OPERATION_RETURN(scope, parseIntResult(result.value()));

    OPERATION_RETURN(scope, parseIntResult(parseInt(String::number(value), 0)));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntString, EncodedJSValue, (JSGlobalObject* globalObject, JSString* string, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto view = string->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    OPERATION_RETURN(scope, parseIntResult(parseInt(view, radix)));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedValue);
    if (radix == 10 && value.isNumber()) {
        if (auto result = parseIntDouble(value.asNumber()))
            OPERATION_RETURN(scope, parseIntResult(result.value()));
    }

    OPERATION_RETURN(scope, toStringView(globalObject, value, [&] (StringView view) {
        return parseIntResult(parseInt(view, radix));
    }));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntDouble, EncodedJSValue, (JSGlobalObject* globalObject, double value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix == 10) {
        if (auto result = parseIntDouble(value))
            OPERATION_RETURN(scope, parseIntResult(result.value()));
    }

    OPERATION_RETURN(scope, parseIntResult(parseInt(String::number(value), radix)));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntInt32, EncodedJSValue, (JSGlobalObject* globalObject, int32_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix == 10)
        OPERATION_RETURN(scope, JSValue::encode(jsNumber(value)));

    OPERATION_RETURN(scope, parseIntResult(parseInt(String::number(value), radix)));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpTestString, size_t, (JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* input))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, regExpObject->testInline(globalObject, input));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpTest, size_t, (JSGlobalObject* globalObject, RegExpObject* regExpObject, EncodedJSValue encodedArgument))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);

    JSString* input = argument.toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        OPERATION_RETURN(scope, false);
    OPERATION_RETURN(scope, regExpObject->testInline(globalObject, input));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpTestGeneric, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedArgument))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(encodedBase);
    JSValue argument = JSValue::decode(encodedArgument);

    auto* regexp = jsDynamicCast<RegExpObject*>(base);
    if (UNLIKELY(!regexp)) {
        throwTypeError(globalObject, scope);
        OPERATION_RETURN(scope, false);
    }

    JSString* input = argument.toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        OPERATION_RETURN(scope, false);
    OPERATION_RETURN(scope, regexp->test(globalObject, input));
}

JSC_DEFINE_JIT_OPERATION(operationSubHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::sub(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationBitNotHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* operand = jsCast<JSBigInt*>(op1);

    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::bitwiseNot(globalObject, operand)));
}

JSC_DEFINE_JIT_OPERATION(operationMulHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::multiply(globalObject, leftOperand, rightOperand)));
}
    
JSC_DEFINE_JIT_OPERATION(operationModHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::remainder(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationDivHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::divide(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationPowHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::exponentiate(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationBitAndHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::bitwiseAnd(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationBitLShiftHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::leftShift(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationAddHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::add(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationBitRShiftHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::signedRightShift(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationBitOrHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::bitwiseOr(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_JIT_OPERATION(operationBitXorHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::bitwiseXor(globalObject, leftOperand, rightOperand)));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationCompareStrictEqCell, size_t, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, JSValue::strictEqualForCells(globalObject, op1, op2));
}

JSC_DEFINE_JIT_OPERATION(operationSameValue, size_t, (JSGlobalObject* globalObject, EncodedJSValue arg1, EncodedJSValue arg2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, sameValue(globalObject, JSValue::decode(arg1), JSValue::decode(arg2)));
}

JSC_DEFINE_JIT_OPERATION(operationToPrimitive, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, JSValue::encode(JSValue::decode(value).toPrimitive(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationToPropertyKey, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(JSValue::decode(value).toPropertyKeyValue(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationToPropertyKeyOrNumber, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue jsValue = JSValue::decode(value);
    OPERATION_RETURN(scope, JSValue::encode(jsValue.isNumber() ? jsValue : jsValue.toPropertyKeyValue(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationToNumber, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(jsNumber(JSValue::decode(value).toNumber(globalObject))));
}

JSC_DEFINE_JIT_OPERATION(operationToNumberString, EncodedJSValue, (JSGlobalObject* globalObject, JSString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto view = string->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned size = view->length();
    if (size == 1) {
        UChar c = view[0];
        if (isASCIIDigit(c))
            OPERATION_RETURN(scope, JSValue::encode(jsNumber(static_cast<int32_t>(c - '0'))));
        if (isStrWhiteSpace(c))
            OPERATION_RETURN(scope, JSValue::encode(jsNumber(0)));
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    }

    if (size == 2 && view[0] == '-') {
        UChar c = view[1];
        if (c == '0')
            OPERATION_RETURN(scope, JSValue::encode(jsNumber(-0.0)));
        if (isASCIIDigit(c))
            OPERATION_RETURN(scope, JSValue::encode(jsNumber(-static_cast<int32_t>(c - '0'))));
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    }

    OPERATION_RETURN(scope, JSValue::encode(jsNumber(jsToNumber(view))));
}

JSC_DEFINE_JIT_OPERATION(operationToNumeric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(JSValue::decode(value).toNumeric(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationCallNumberConstructor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedValue);
    JSValue numeric = value.toNumeric(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (numeric.isNumber())
        OPERATION_RETURN(scope, JSValue::encode(numeric));
    ASSERT(numeric.isBigInt());
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::toNumber(numeric)));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdWithThisStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    putWithThis<true>(globalObject, encodedBase, encodedThis, encodedValue, ident);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdWithThis, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    putWithThis<false>(globalObject, encodedBase, encodedThis, encodedValue, ident);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValWithThisStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier property = JSValue::decode(encodedSubscript).toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    scope.release();
    putWithThis<true>(globalObject, encodedBase, encodedThis, encodedValue, property);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValWithThis, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier property = JSValue::decode(encodedSubscript).toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    scope.release();
    putWithThis<false>(globalObject, encodedBase, encodedThis, encodedValue, property);
    OPERATION_RETURN(scope);
}

ALWAYS_INLINE static void defineDataProperty(JSGlobalObject* globalObject, JSObject* base, const Identifier& propertyName, JSValue value, int32_t attributes)
{
    PropertyDescriptor descriptor = toPropertyDescriptor(value, jsUndefined(), jsUndefined(), DefinePropertyAttributes(attributes));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    if (base->methodTable()->defineOwnProperty == JSObject::defineOwnProperty)
        JSObject::defineOwnProperty(base, globalObject, propertyName, descriptor, true);
    else
        base->methodTable()->defineOwnProperty(base, globalObject, propertyName, descriptor, true);
}

JSC_DEFINE_JIT_OPERATION(operationDefineDataProperty, void, (JSGlobalObject* globalObject, JSObject* base, EncodedJSValue encodedProperty, EncodedJSValue encodedValue, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = JSValue::decode(encodedProperty).toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    scope.release();
    defineDataProperty(globalObject, base, propertyName, JSValue::decode(encodedValue), attributes);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDefineDataPropertyString, void, (JSGlobalObject* globalObject, JSObject* base, JSString* property, EncodedJSValue encodedValue, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = property->toIdentifier(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    scope.release();
    defineDataProperty(globalObject, base, propertyName, JSValue::decode(encodedValue), attributes);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDefineDataPropertyStringIdent, void, (JSGlobalObject* globalObject, JSObject* base, UniquedStringImpl* property, EncodedJSValue encodedValue, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    defineDataProperty(globalObject, base, Identifier::fromUid(vm, property), JSValue::decode(encodedValue), attributes);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDefineDataPropertySymbol, void, (JSGlobalObject* globalObject, JSObject* base, Symbol* property, EncodedJSValue encodedValue, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    defineDataProperty(globalObject, base, Identifier::fromUid(property->privateName()), JSValue::decode(encodedValue), attributes);
    OPERATION_RETURN(scope);
}

ALWAYS_INLINE static void defineAccessorProperty(JSGlobalObject* globalObject, JSObject* base, const Identifier& propertyName, JSObject* getter, JSObject* setter, int32_t attributes)
{
    PropertyDescriptor descriptor = toPropertyDescriptor(jsUndefined(), getter, setter, DefinePropertyAttributes(attributes));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    if (base->methodTable()->defineOwnProperty == JSObject::defineOwnProperty)
        JSObject::defineOwnProperty(base, globalObject, propertyName, descriptor, true);
    else
        base->methodTable()->defineOwnProperty(base, globalObject, propertyName, descriptor, true);
}

JSC_DEFINE_JIT_OPERATION(operationDefineAccessorProperty, void, (JSGlobalObject* globalObject, JSObject* base, EncodedJSValue encodedProperty, JSObject* getter, JSObject* setter, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = JSValue::decode(encodedProperty).toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    defineAccessorProperty(globalObject, base, propertyName, getter, setter, attributes);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDefineAccessorPropertyString, void, (JSGlobalObject* globalObject, JSObject* base, JSString* property, JSObject* getter, JSObject* setter, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = property->toIdentifier(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    defineAccessorProperty(globalObject, base, propertyName, getter, setter, attributes);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDefineAccessorPropertyStringIdent, void, (JSGlobalObject* globalObject, JSObject* base, UniquedStringImpl* property, JSObject* getter, JSObject* setter, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    defineAccessorProperty(globalObject, base, Identifier::fromUid(vm, property), getter, setter, attributes);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDefineAccessorPropertySymbol, void, (JSGlobalObject* globalObject, JSObject* base, Symbol* property, JSObject* getter, JSObject* setter, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    defineAccessorProperty(globalObject, base, Identifier::fromUid(property->privateName()), getter, setter, attributes);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationNewArray, char*, (JSGlobalObject* globalObject, Structure* arrayStructure, void* buffer, size_t size))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(buffer), size);

    OPERATION_RETURN(scope, bitwise_cast<char*>(constructArray(globalObject, arrayStructure, static_cast<JSValue*>(buffer), size)));
}

JSC_DEFINE_JIT_OPERATION(operationNewEmptyArray, char*, (VM* vmPointer, Structure* arrayStructure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, bitwise_cast<char*>(JSArray::create(vm, arrayStructure)));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSize, char*, (JSGlobalObject* globalObject, Structure* arrayStructure, int32_t size, Butterfly* butterfly))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(size < 0)) {
        throwException(globalObject, scope, createRangeError(globalObject, ArrayInvalidLengthError));
        OPERATION_RETURN(scope, nullptr);
    }

    JSArray* result;
    if (butterfly)
        result = JSArray::createWithButterfly(vm, nullptr, arrayStructure, butterfly);
    else {
        result = JSArray::tryCreate(vm, arrayStructure, size);
        if (UNLIKELY(!result)) {
            throwOutOfMemoryError(globalObject, scope);
            OPERATION_RETURN(scope, nullptr);
        }
    }
    OPERATION_RETURN(scope, bitwise_cast<char*>(result));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSizeAndHint, char*, (JSGlobalObject* globalObject, Structure* arrayStructure, int32_t size, int32_t vectorLengthHint, Butterfly* butterfly))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(size < 0)) {
        throwException(globalObject, scope, createRangeError(globalObject, ArrayInvalidLengthError));
        OPERATION_RETURN(scope, nullptr);
    }

    JSArray* result;
    if (butterfly)
        result = JSArray::createWithButterfly(vm, nullptr, arrayStructure, butterfly);
    else {
        result = JSArray::tryCreate(vm, arrayStructure, size, vectorLengthHint);
        if (UNLIKELY(!result)) {
            throwOutOfMemoryError(globalObject, scope);
            OPERATION_RETURN(scope, nullptr);
        }
    }
    OPERATION_RETURN(scope, bitwise_cast<char*>(result));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayBuffer, JSCell*, (VM* vmPointer, Structure* arrayStructure, JSCell* immutableButterflyCell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(!arrayStructure->outOfLineCapacity());
    auto* immutableButterfly = jsCast<JSImmutableButterfly*>(immutableButterflyCell);
    ASSERT(arrayStructure->indexingMode() == immutableButterfly->indexingMode() || hasAnyArrayStorage(arrayStructure->indexingMode()));
    auto* result = CommonSlowPaths::allocateNewArrayBuffer(vm, arrayStructure, immutableButterfly);
    ASSERT(result->indexingMode() == result->structure()->indexingMode());
    ASSERT(result->structure() == arrayStructure);
    OPERATION_RETURN(scope, result);
}

#define JSC_TYPED_ARRAY_OPERATIONS(type) \
    JSC_DEFINE_JIT_OPERATION(operationNew##type##ArrayWithSize, char*, (JSGlobalObject* globalObject, Structure* structure, intptr_t length, char* vector)) \
    { \
        VM& vm = globalObject->vm(); \
        CallFrame* callFrame = DECLARE_CALL_FRAME(vm); \
        JITOperationPrologueCallFrameTracer tracer(vm, callFrame); \
        auto scope = DECLARE_THROW_SCOPE(vm); \
        OPERATION_RETURN(scope, newTypedArrayWithSize<JS##type##Array>(globalObject, vm, structure, length, vector)); \
    } \
    \
    JSC_DEFINE_JIT_OPERATION(operationNew##type##ArrayWithOneArgument, char*, (JSGlobalObject* globalObject, EncodedJSValue encodedValue)) \
    { \
        VM& vm = globalObject->vm(); \
        CallFrame* callFrame = DECLARE_CALL_FRAME(vm); \
        JITOperationPrologueCallFrameTracer tracer(vm, callFrame); \
        auto scope = DECLARE_THROW_SCOPE(vm); \
        JSValue firstValue = JSValue::decode(encodedValue); \
        bool isResizableOrGrowableShared = false; \
        if (auto* arrayBuffer = jsDynamicCast<JSArrayBuffer*>(firstValue)) \
            isResizableOrGrowableShared = arrayBuffer->isResizableOrGrowableShared(); \
        Structure* structure = globalObject->typedArrayStructure(Type##type, isResizableOrGrowableShared); \
        OPERATION_RETURN(scope, reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JS##type##Array>(globalObject, structure, firstValue, 0, std::nullopt))); \
    } \

    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_TYPED_ARRAY_OPERATIONS)
#undef JSC_TYPED_ARRAY_OPERATIONS

JSC_DEFINE_JIT_OPERATION(operationNewArrayIterator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSArrayIterator::createWithInitialValues(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewMapIterator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSMapIterator::createWithInitialValues(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewSetIterator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSSetIterator::createWithInitialValues(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewIteratorHelper, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSIteratorHelper::createWithInitialValues(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCreateActivationDirect, JSCell*, (VM* vmPointer, Structure* structure, JSScope* jsScope, SymbolTable* table, EncodedJSValue initialValueEncoded))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue initialValue = JSValue::decode(initialValueEncoded);
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    OPERATION_RETURN(scope, JSLexicalEnvironment::create(vm, structure, jsScope, table, initialValue));
}

JSC_DEFINE_JIT_OPERATION(operationCreateDirectArguments, JSCell*, (VM* vmPointer, Structure* structure, uint32_t length, uint32_t minCapacity))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    DirectArguments* result = DirectArguments::create(
        vm, structure, length, std::max(length, minCapacity));
    // The caller will store to this object without barriers. Most likely, at this point, this is
    // still a young object and so no barriers are needed. But it's good to be careful anyway,
    // since the GC should be allowed to do crazy (like pretenuring, for example).
    vm.writeBarrier(result);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationCreateScopedArguments, JSCell*, (JSGlobalObject* globalObject, Structure* structure, Register* argumentStart, uint32_t length, JSFunction* callee, JSLexicalEnvironment* environment))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    // We could pass the ScopedArgumentsTable* as an argument. We currently don't because I
    // didn't feel like changing the max number of arguments for a slow path call from 6 to 7.
    ScopedArgumentsTable* table = environment->symbolTable()->arguments();
    
    OPERATION_RETURN(scope, ScopedArguments::createByCopyingFrom(vm, structure, argumentStart, length, callee, table, environment));
}

JSC_DEFINE_JIT_OPERATION(operationCreateClonedArguments, JSCell*, (JSGlobalObject* globalObject, Structure* structure, Register* argumentStart, uint32_t length, JSFunction* callee, Butterfly* butterfly))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCell* result = ClonedArguments::createByCopyingFrom(globalObject, structure, argumentStart, length, callee, butterfly);
    EXCEPTION_ASSERT_UNUSED(scope, scope.exception() || result);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationCreateDirectArgumentsDuringExit, JSCell*, (VM* vmPointer, InlineCallFrame* inlineCallFrame, JSFunction* callee, uint32_t argumentCount))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    DeferGCForAWhile deferGC(vm);
    
    CodeBlock* codeBlock;
    if (inlineCallFrame)
        codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    else
        codeBlock = callFrame->codeBlock();
    
    unsigned length = argumentCount - 1;
    unsigned capacity = std::max(length, static_cast<unsigned>(codeBlock->numParameters() - 1));
    DirectArguments* result = DirectArguments::create(
        vm, codeBlock->globalObject()->directArgumentsStructure(), length, capacity);
    
    result->setCallee(vm, callee);
    
    Register* arguments =
        callFrame->registers() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0) +
        CallFrame::argumentOffset(0);
    for (unsigned i = length; i--;)
        result->setIndexQuickly(vm, i, arguments[i].jsValue());
    
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationCreateClonedArgumentsDuringExit, JSCell*, (VM* vmPointer, InlineCallFrame* inlineCallFrame, JSFunction* callee, uint32_t argumentCount))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    DeferGCForAWhile deferGC(vm);
    
    CodeBlock* codeBlock;
    if (inlineCallFrame)
        codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    else
        codeBlock = callFrame->codeBlock();
    
    unsigned length = argumentCount - 1;
    JSGlobalObject* globalObject = codeBlock->globalObject();
    ClonedArguments* result = ClonedArguments::createEmpty(vm, nullptr, globalObject->clonedArgumentsStructure(), callee, length, nullptr);
    RELEASE_ASSERT_RESOURCE_AVAILABLE(result, MemoryExhaustion, "Crash intentionally because memory is exhausted.");

    Register* arguments =
        callFrame->registers() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0) +
        CallFrame::argumentOffset(0);
    for (unsigned i = length; i--;)
        result->putDirectIndex(globalObject, i, arguments[i].jsValue());

    
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationCreateRest, JSCell*, (JSGlobalObject* globalObject, Register* argumentStart, unsigned numberOfParamsToSkip, unsigned arraySize))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = globalObject->restParameterStructure();
    static_assert(sizeof(Register) == sizeof(JSValue), "This is a strong assumption here.");
    JSValue* argumentsToCopyRegion = bitwise_cast<JSValue*>(argumentStart) + numberOfParamsToSkip;
    OPERATION_RETURN(scope, constructArray(globalObject, structure, argumentsToCopyRegion, arraySize));
}

JSC_DEFINE_JIT_OPERATION(operationTypeOfIsObject, size_t, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    OPERATION_RETURN(scope, jsTypeofIsObject(globalObject, object));
}

JSC_DEFINE_JIT_OPERATION(operationTypeOfIsFunction, size_t, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(jsDynamicCast<JSObject*>(object));

    OPERATION_RETURN(scope, jsTypeofIsFunction(globalObject, object));
}

JSC_DEFINE_JIT_OPERATION(operationObjectIsCallable, size_t, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    OPERATION_RETURN(scope, object->isCallable());
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationIsConstructor, size_t, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::decode(value).isConstructor());
}

JSC_DEFINE_JIT_OPERATION(operationTypeOfObject, JSCell*, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    if (object->structure()->masqueradesAsUndefined(globalObject))
        OPERATION_RETURN(scope, vm.smallStrings.undefinedString());
    if (object->isCallable())
        OPERATION_RETURN(scope, vm.smallStrings.functionString());
    OPERATION_RETURN(scope, vm.smallStrings.objectString());
}

JSC_DEFINE_JIT_OPERATION(operationAllocateSimplePropertyStorageWithInitialCapacity, Butterfly*, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, Butterfly::createUninitialized(vm, nullptr, 0, initialOutOfLineCapacity, false, 0));
}

JSC_DEFINE_JIT_OPERATION(operationAllocateSimplePropertyStorage, Butterfly*, (VM* vmPointer, size_t newSize))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, Butterfly::createUninitialized(vm, nullptr, 0, newSize, false, 0));
}

JSC_DEFINE_JIT_OPERATION(operationAllocateComplexPropertyStorageWithInitialCapacity, Butterfly*, (VM* vmPointer, JSObject* object))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!object->structure()->outOfLineCapacity());
    OPERATION_RETURN(scope, object->allocateMoreOutOfLineStorage(vm, 0, initialOutOfLineCapacity));
}

JSC_DEFINE_JIT_OPERATION(operationAllocateComplexPropertyStorage, Butterfly*, (VM* vmPointer, JSObject* object, size_t newSize))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, object->allocateMoreOutOfLineStorage(vm, object->structure()->outOfLineCapacity(), newSize));
}

JSC_DEFINE_JIT_OPERATION(operationEnsureInt32, Butterfly*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!cell->isObject())
        OPERATION_RETURN(scope, nullptr);

    auto* result = reinterpret_cast<Butterfly*>(asObject(cell)->tryMakeWritableInt32(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasInt32(cell->indexingMode())) || !result);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationEnsureDouble, Butterfly*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!cell->isObject())
        OPERATION_RETURN(scope, nullptr);

    ASSERT(Options::allowDoubleShape());
    auto* result = reinterpret_cast<Butterfly*>(asObject(cell)->tryMakeWritableDouble(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasDouble(cell->indexingMode())) || !result);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationEnsureContiguous, Butterfly*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!cell->isObject())
        OPERATION_RETURN(scope, nullptr);
    
    auto* result = reinterpret_cast<Butterfly*>(asObject(cell)->tryMakeWritableContiguous(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasContiguous(cell->indexingMode())) || !result);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationEnsureArrayStorage, Butterfly*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!cell->isObject())
        OPERATION_RETURN(scope, nullptr);

    auto* result = reinterpret_cast<Butterfly*>(asObject(cell)->ensureArrayStorage(vm));
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasAnyArrayStorage(cell->indexingMode())) || !result);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationHasIndexedProperty, size_t, (JSGlobalObject* globalObject, JSCell* baseCell, int32_t subscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = baseCell->toObject(globalObject);
    if (UNLIKELY(subscript < 0)) {
        // When subscript is negative, -1 is the most common case. Let's handle it separately.
        if (LIKELY(subscript == -1)) {
            // what?
            if (auto* array = jsDynamicCast<JSArray*>(object); LIKELY(array && array->definitelyNegativeOneMiss()))
                OPERATION_RETURN(scope, false);
            OPERATION_RETURN(scope, object->hasProperty(globalObject, vm.propertyNames->negativeOneIdentifier));
        }

        // Go the slowest way possible because negative indices don't use indexed storage.
        OPERATION_RETURN(scope, object->hasProperty(globalObject, Identifier::from(vm, subscript)));
    }
    OPERATION_RETURN(scope, object->hasProperty(globalObject, static_cast<unsigned>(subscript)));
}

JSC_DEFINE_JIT_OPERATION(operationHasEnumerableIndexedProperty, size_t, (JSGlobalObject* globalObject, JSCell* baseCell, int32_t subscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = baseCell->toObject(globalObject);
    if (UNLIKELY(subscript < 0)) {
        // When subscript is negative, -1 is the most common case. Let's handle it separately.
        if (LIKELY(subscript == -1)) {
            if (auto* array = jsDynamicCast<JSArray*>(baseCell); LIKELY(array && array->definitelyNegativeOneMiss()))
                OPERATION_RETURN(scope, false);
            OPERATION_RETURN(scope, object->hasProperty(globalObject, vm.propertyNames->negativeOneIdentifier));
        }

        // Go the slowest way possible because negative indices don't use indexed storage.
        OPERATION_RETURN(scope, object->hasEnumerableProperty(globalObject, Identifier::from(vm, subscript)));
    }
    OPERATION_RETURN(scope, object->hasEnumerableProperty(globalObject, subscript));
}

JSC_DEFINE_JIT_OPERATION(operationGetPropertyEnumerator, JSCell*, (JSGlobalObject* globalObject, EncodedJSValue encodedBase))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(encodedBase);
    if (base.isUndefinedOrNull())
        OPERATION_RETURN(scope, vm.emptyPropertyNameEnumerator());

    JSObject* baseObject = base.toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, propertyNameEnumerator(globalObject, baseObject));
}

JSC_DEFINE_JIT_OPERATION(operationGetPropertyEnumeratorCell, JSCell*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* base = cell->toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, propertyNameEnumerator(globalObject, base));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorNextUpdateIndexAndMode, UGPRPair, (JSGlobalObject* globalObject, EncodedJSValue baseValue, uint32_t index, int32_t modeNumber, JSPropertyNameEnumerator* enumerator))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPropertyNameEnumerator::Flag mode = static_cast<JSPropertyNameEnumerator::Flag>(modeNumber);
    if (JSValue::decode(baseValue).isUndefinedOrNull()) {
        ASSERT(mode == JSPropertyNameEnumerator::InitMode);
        ASSERT(!index);
        ASSERT(!enumerator->sizeOfPropertyNames());
        mode = JSPropertyNameEnumerator::GenericMode;
    } else {
        JSObject* base = JSValue::decode(baseValue).toObject(globalObject);
        OPERATION_RETURN_IF_EXCEPTION(scope, UGPRPair { });

        constexpr bool shouldAllocateIndexedNameString = false;
        enumerator->computeNext(globalObject, base, index, mode, shouldAllocateIndexedNameString);
        OPERATION_RETURN_IF_EXCEPTION(scope, UGPRPair { });
    }

    OPERATION_RETURN(scope, makeUGPRPair(index, static_cast<uint32_t>(mode)));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorNextUpdatePropertyName, JSString*, (JSGlobalObject* globalObject, uint32_t index, int32_t modeNumber, JSPropertyNameEnumerator* enumerator))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (modeNumber == JSPropertyNameEnumerator::IndexedMode) {
        if (index < enumerator->indexedLength())
            OPERATION_RETURN(scope, jsString(vm, Identifier::from(vm, index).string()));
        OPERATION_RETURN(scope, vm.smallStrings.sentinelString());
    }

    JSString* result = enumerator->propertyNameAtIndex(index);
    if (!result)
        OPERATION_RETURN(scope, vm.smallStrings.sentinelString());

    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorRecoverNameAndGetByVal, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue baseValue, uint32_t index, JSPropertyNameEnumerator* enumerator))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = enumerator->propertyNameAtIndex(index);
    PropertyName propertyName = string->toIdentifier(globalObject);
    // This should only really return for TerminationException since we know string is backed by a UUID.
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSValue base = JSValue::decode(baseValue);
    JSObject* object = base.toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    OPERATION_RETURN(scope, JSValue::encode(object->get(globalObject, propertyName)));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorInByVal, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue baseValue, EncodedJSValue propertyNameValue, uint32_t index, int32_t modeNumber))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(baseValue);
    if (modeNumber == JSPropertyNameEnumerator::IndexedMode && base.isObject())
        OPERATION_RETURN(scope, JSValue::encode(jsBoolean(jsCast<JSObject*>(base)->hasProperty(globalObject, index))));

    JSString* propertyName = jsSecureCast<JSString*>(JSValue::decode(propertyNameValue));
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(CommonSlowPaths::opInByVal(globalObject, base, propertyName))));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorHasOwnProperty, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue baseValue, EncodedJSValue propertyNameValue, uint32_t index, int32_t modeNumber))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(baseValue);
    if (modeNumber == JSPropertyNameEnumerator::IndexedMode && base.isObject())
        OPERATION_RETURN(scope, JSValue::encode(jsBoolean(jsCast<JSObject*>(base)->hasOwnProperty(globalObject, index))));

    JSString* propertyName = jsSecureCast<JSString*>(JSValue::decode(propertyNameValue));
    auto identifier = propertyName->toIdentifier(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSObject* baseObject = base.toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(objectPrototypeHasOwnProperty(globalObject, baseObject, identifier))));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorRecoverNameAndPutByVal, void, (JSGlobalObject* globalObject, EncodedJSValue baseValue, EncodedJSValue valueValue, bool isStrict, uint32_t index, JSPropertyNameEnumerator* enumerator))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = enumerator->propertyNameAtIndex(index);
    PropertyName propertyName = string->toIdentifier(globalObject);
    // This should only really return for TerminationException since we know string is backed by a UUID.
    OPERATION_RETURN_IF_EXCEPTION(scope);
    JSValue base = JSValue::decode(baseValue);
    JSValue value = JSValue::decode(valueValue);

    scope.release();
    PutPropertySlot slot(base, isStrict);
    base.put(globalObject, propertyName, value, slot);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationNewRegexpWithLastIndex, JSCell*, (JSGlobalObject* globalObject, JSCell* regexpPtr, EncodedJSValue encodedLastIndex))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    OPERATION_RETURN(scope, RegExpObject::create(vm, globalObject->regExpStructure(), regexp, JSValue::decode(encodedLastIndex)));
}

JSC_DEFINE_JIT_OPERATION(operationResolveRope, StringImpl*, (JSGlobalObject* globalObject, JSString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, string->value(globalObject)->impl());
}

JSC_DEFINE_JIT_OPERATION(operationResolveRopeString, JSString*, (JSGlobalObject* globalObject, JSRopeString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    string->resolveRope(globalObject);
    OPERATION_RETURN(scope, string);
}

JSC_DEFINE_JIT_OPERATION(operationStringValueOf, JSString*, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);

    if (argument.isString())
        OPERATION_RETURN(scope,  asString(argument));

    if (auto* stringObject = jsDynamicCast<StringObject*>(argument))
        OPERATION_RETURN(scope,  stringObject->internalValue());

    throwVMTypeError(globalObject, scope);
    OPERATION_RETURN(scope, nullptr);
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringString, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (JSString* result = tryReplaceOneCharUsingString<DollarCheck::Yes>(globalObject, stringCell, searchCell, replacementCell))
        OPERATION_RETURN(scope, result);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto string = stringCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto search = searchCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto replacement = replacementCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::Yes, StringReplaceUseTable::No, BoyerMooreHorspoolTable<uint8_t>>(globalObject, stringCell, string, search, replacement, nullptr)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringStringWithoutSubstitution, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (JSString* result = tryReplaceOneCharUsingString<DollarCheck::No>(globalObject, stringCell, searchCell, replacementCell))
        OPERATION_RETURN(scope, result);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto string = stringCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto search = searchCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto replacement = replacementCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::No, StringReplaceUseTable::No, BoyerMooreHorspoolTable<uint8_t>>(globalObject, stringCell, string, search, replacement, nullptr)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringEmptyString, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (JSString* result = tryReplaceOneCharUsingString<DollarCheck::No>(globalObject, stringCell, searchCell, jsEmptyString(vm)))
        OPERATION_RETURN(scope, result);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto string = stringCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto search = searchCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    size_t matchStart = StringView(string).find(vm.adaptiveStringSearcherTables(), StringView(search));
    if (matchStart == notFound)
        OPERATION_RETURN(scope,  stringCell);

    // Because replacement string is empty, it cannot include backreferences.
    size_t searchLength = search->length();
    size_t matchEnd = matchStart + searchLength;
    auto result = tryMakeString(StringView(string).substring(0, matchStart), StringView(string).substring(matchEnd, string->length() - matchEnd));
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        OPERATION_RETURN(scope,  nullptr);
    }

    OPERATION_RETURN(scope,  jsString(vm, WTFMove(result)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringStringWithTable8, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell, const BoyerMooreHorspoolTable<uint8_t>* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = stringCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto search = searchCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto replacement = replacementCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::Yes, StringReplaceUseTable::Yes>(globalObject, stringCell, string, search, replacement, table)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringStringWithoutSubstitutionWithTable8, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell, const BoyerMooreHorspoolTable<uint8_t>* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = stringCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto search = searchCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto replacement = replacementCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::No, StringReplaceUseTable::Yes>(globalObject, stringCell, string, search, replacement, table)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringEmptyStringWithTable8, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, const BoyerMooreHorspoolTable<uint8_t>* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = stringCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto search = searchCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    size_t matchStart = table->find(string, search);
    if (matchStart == notFound)
        OPERATION_RETURN(scope, stringCell);

    // Because replacement string is empty, it cannot include backreferences.
    size_t searchLength = search->length();
    size_t matchEnd = matchStart + searchLength;
    auto result = tryMakeString(StringView(string).substring(0, matchStart), StringView(string).substring(matchEnd, string->length() - matchEnd));
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        OPERATION_RETURN(scope, nullptr);
    }

    OPERATION_RETURN(scope, jsString(vm, WTFMove(result)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringGeneric, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, EncodedJSValue encodedReplaceValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue replaceValue = JSValue::decode(encodedReplaceValue);
    if (replaceValue.isString()) {
        if (JSString* result = tryReplaceOneCharUsingString<DollarCheck::Yes>(globalObject, stringCell, searchCell, asString(replaceValue)))
            OPERATION_RETURN(scope, result);
        OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    }

    auto string = stringCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    auto search = searchCell->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, replaceUsingStringSearch(vm, globalObject, stringCell, string, search, replaceValue, StringReplaceMode::Single));
}

JSC_DEFINE_JIT_OPERATION(operationStringSubstr, JSCell*, (JSGlobalObject* globalObject, JSCell* cell, int32_t from, int32_t span))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsSubstring(vm, globalObject, jsCast<JSString*>(cell), from, span));
}

JSC_DEFINE_JIT_OPERATION(operationStringSlice, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    static_assert(static_cast<uint64_t>(JSString::MaxLength) <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max()));
    OPERATION_RETURN(scope, stringSlice<int32_t>(globalObject, vm, string, string->length(), start, std::nullopt));
}

JSC_DEFINE_JIT_OPERATION(operationStringSliceWithEnd, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start, int32_t end))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    static_assert(static_cast<uint64_t>(JSString::MaxLength) <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max()));
    OPERATION_RETURN(scope, stringSlice<int32_t>(globalObject, vm, string, string->length(), start, end));
}

JSC_DEFINE_JIT_OPERATION(operationStringSubstring, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, stringSubstring(globalObject, string, start, std::nullopt));
}

JSC_DEFINE_JIT_OPERATION(operationStringSubstringWithEnd, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start, int32_t end))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, stringSubstring(globalObject, string, start, end));
}

JSC_DEFINE_JIT_OPERATION(operationToLowerCase, JSString*, (JSGlobalObject* globalObject, JSString* string, uint32_t failingIndex))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto inputString = string->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    if (!inputString->length())
        OPERATION_RETURN(scope, vm.smallStrings.emptyString());

    String lowercasedString = inputString->is8Bit() ? inputString->convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(failingIndex) : inputString->convertToLowercaseWithoutLocale();
    if (lowercasedString.impl() == inputString->impl())
        OPERATION_RETURN(scope, string);
    OPERATION_RETURN(scope, jsString(vm, WTFMove(lowercasedString)));
}

JSC_DEFINE_JIT_OPERATION(operationStringLocaleCompare, UCPUStrictInt32, (JSGlobalObject* globalObject, JSString* base, JSString* argument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = base->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    auto that = argument->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    auto* collator = globalObject->defaultCollator();

    OPERATION_RETURN(scope, toUCPUStrictInt32(collator->compareStrings(globalObject, string, that)));
}

JSC_DEFINE_JIT_OPERATION(operationStringIndexOf, UCPUStrictInt32, (JSGlobalObject* globalObject, JSString* base, JSString* argument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto thisView = base->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    auto otherView = argument->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    size_t result = thisView->find(vm.adaptiveStringSearcherTables(), otherView);
    if (result == notFound)
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));
    OPERATION_RETURN(scope, toUCPUStrictInt32(result));
}

JSC_DEFINE_JIT_OPERATION(operationStringIndexOfWithOneChar, UCPUStrictInt32, (JSGlobalObject* globalObject, JSString* base, int32_t character))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto thisView = base->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    size_t result = thisView->find(static_cast<UChar>(character));
    if (result == notFound)
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));
    OPERATION_RETURN(scope, toUCPUStrictInt32(result));
}

JSC_DEFINE_JIT_OPERATION(operationStringIndexOfWithIndex, UCPUStrictInt32, (JSGlobalObject* globalObject, JSString* base, JSString* argument, int32_t position))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto thisView = base->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    auto otherView = argument->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    int32_t length = thisView->length();
    unsigned pos = 0;
    if (position >= 0)
        pos = std::min<uint32_t>(position, length);

    if (static_cast<unsigned>(length) < otherView->length() + pos)
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));

    size_t result = thisView->find(vm.adaptiveStringSearcherTables(), otherView, pos);
    if (result == notFound)
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));
    OPERATION_RETURN(scope, toUCPUStrictInt32(result));
}

JSC_DEFINE_JIT_OPERATION(operationStringIndexOfWithIndexWithOneChar, UCPUStrictInt32, (JSGlobalObject* globalObject, JSString* base, int32_t position, int32_t character))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto thisView = base->view(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, 0);

    int32_t length = thisView->length();
    unsigned pos = 0;
    if (position >= 0)
        pos = std::min<uint32_t>(position, length);

    if (static_cast<unsigned>(length) < 1 + pos)
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));

    size_t result = thisView->find(static_cast<UChar>(character), pos);
    if (result == notFound)
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));
    OPERATION_RETURN(scope, toUCPUStrictInt32(result));
}

JSC_DEFINE_JIT_OPERATION(operationInt32ToString, char*, (JSGlobalObject* globalObject, int32_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(globalObject, scope, createRangeError(globalObject, "toString() radix argument must be between 2 and 36"_s));
        OPERATION_RETURN(scope, nullptr);
    }

    OPERATION_RETURN(scope, reinterpret_cast<char*>(int32ToString(vm, value, radix)));
}

JSC_DEFINE_JIT_OPERATION(operationInt52ToString, char*, (JSGlobalObject* globalObject, int64_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(globalObject, scope, createRangeError(globalObject, "toString() radix argument must be between 2 and 36"_s));
        OPERATION_RETURN(scope, nullptr);
    }

    OPERATION_RETURN(scope, reinterpret_cast<char*>(int52ToString(vm, value, radix)));
}

JSC_DEFINE_JIT_OPERATION(operationDoubleToString, char*, (JSGlobalObject* globalObject, double value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(globalObject, scope, createRangeError(globalObject, "toString() radix argument must be between 2 and 36"_s));
        OPERATION_RETURN(scope, nullptr);
    }

    OPERATION_RETURN(scope, reinterpret_cast<char*>(numberToString(vm, value, radix)));
}

JSC_DEFINE_JIT_OPERATION(operationInt32ToStringWithValidRadix, char*, (JSGlobalObject* globalObject, int32_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, reinterpret_cast<char*>(int32ToString(vm, value, radix)));
}

JSC_DEFINE_JIT_OPERATION(operationInt52ToStringWithValidRadix, char*, (JSGlobalObject* globalObject, int64_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, reinterpret_cast<char*>(int52ToString(vm, value, radix)));
}

JSC_DEFINE_JIT_OPERATION(operationDoubleToStringWithValidRadix, char*, (JSGlobalObject* globalObject, double value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, reinterpret_cast<char*>(numberToString(vm, value, radix)));
}

JSC_DEFINE_JIT_OPERATION(operationFunctionToString, JSString*, (JSGlobalObject* globalObject, JSFunction* function))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, function->toString(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationFunctionBind, JSBoundFunction*, (JSGlobalObject* globalObject, JSObject* target, EncodedJSValue boundThisValue, EncodedJSValue arg0Value, EncodedJSValue arg1Value, EncodedJSValue arg2Value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!target->isCallable())) {
        throwTypeError(globalObject, scope, "|this| is not a function inside Function.prototype.bind"_s);
        OPERATION_RETURN(scope, nullptr);
    }

    unsigned boundArgsLength = 0;
    boundArgsLength += !!(JSValue::decode(arg0Value));
    boundArgsLength += !!(JSValue::decode(arg1Value));
    boundArgsLength += !!(JSValue::decode(arg2Value));

    JSValue boundThis = JSValue::decode(boundThisValue);
    EncodedJSValue arguments[JSBoundFunction::maxEmbeddedArgs] {
        arg0Value,
        arg1Value,
        arg2Value,
    };
    ArgList boundArgs { };
    if (boundArgsLength >= 1)
        boundArgs = ArgList(arguments, boundArgsLength);

    double length = 0;
    JSString* name = nullptr;
    JSFunction* function = jsDynamicCast<JSFunction*>(target);
    if (LIKELY(function && function->canAssumeNameAndLengthAreOriginal(vm))) {
        // Do nothing! 'length' and 'name' computation are lazily done.
        // And this is totally OK since we know that wrapped functions have canAssumeNameAndLengthAreOriginal condition
        // at the time of creation of JSBoundFunction.
        length = PNaN; // Defer computation.
    } else {
        bool found = target->hasOwnProperty(globalObject, vm.propertyNames->length);
        OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
        if (found) {
            JSValue lengthValue = target->get(globalObject, vm.propertyNames->length);
            OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
            length = lengthValue.toIntegerOrInfinity(globalObject);
            OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
            if (length > boundArgsLength)
                length -= boundArgsLength;
            else
                length = 0;
        }
        JSValue nameValue = target->get(globalObject, vm.propertyNames->name);
        OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
        if (nameValue.isString())
            name = asString(nameValue);
        else
            name = jsEmptyString(vm);
    }

    OPERATION_RETURN(scope, JSBoundFunction::create(vm, globalObject, target, boundThis, boundArgs, length, name));
}

JSC_DEFINE_JIT_OPERATION(operationNewBoundFunction, JSBoundFunction*, (JSGlobalObject* globalObject, JSFunction* function, EncodedJSValue boundThisValue, EncodedJSValue arg0Value, EncodedJSValue arg1Value, EncodedJSValue arg2Value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue boundThis = JSValue::decode(boundThisValue);
    JSValue arg0 = JSValue::decode(arg0Value);
    JSValue arg1 = JSValue::decode(arg1Value);
    JSValue arg2 = JSValue::decode(arg2Value);
    unsigned boundArgsLength = 0;
    boundArgsLength += !!(arg0);
    boundArgsLength += !!(arg1);
    boundArgsLength += !!(arg2);
    OPERATION_RETURN(scope, JSBoundFunction::createRaw(vm, globalObject, function, boundArgsLength, boundThis, arg0, arg1, arg2));
}

JSC_DEFINE_JIT_OPERATION(operationSingleCharacterString, JSString*, (VM* vmPointer, int32_t character))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, jsSingleCharacterString(vm, static_cast<UChar>(character)));
}

JSC_DEFINE_JIT_OPERATION(operationNewSymbol, Symbol*, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, Symbol::create(vm));
}

JSC_DEFINE_JIT_OPERATION(operationNewSymbolWithStringDescription, Symbol*, (JSGlobalObject* globalObject, JSString* description))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = description->value(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, Symbol::createWithDescription(vm, WTFMove(string)));
}

JSC_DEFINE_JIT_OPERATION(operationNewSymbolWithDescription, Symbol*, (JSGlobalObject* globalObject, EncodedJSValue encodedDescription))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue description = JSValue::decode(encodedDescription);
    if (description.isUndefined())
        OPERATION_RETURN(scope, Symbol::create(vm));

    String string = description.toWTFString(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, Symbol::createWithDescription(vm, string));
}

JSC_DEFINE_JIT_OPERATION(operationNewStringObject, JSCell*, (VM* vmPointer, JSString* string, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, StringObject::create(vm, structure, string));
}

JSC_DEFINE_JIT_OPERATION(operationToStringOnCell, JSString*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, JSValue(cell).toString(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationToString, JSString*, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::decode(value).toString(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationCallStringConstructorOnCell, JSString*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, stringConstructor(globalObject, cell));
}

JSC_DEFINE_JIT_OPERATION(operationCallStringConstructor, JSString*, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, stringConstructor(globalObject, JSValue::decode(value)));
}

JSC_DEFINE_JIT_OPERATION(operationMakeRope2, JSString*, (JSGlobalObject* globalObject, JSString* left, JSString* right))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsString(globalObject, left, right));
}

JSC_DEFINE_JIT_OPERATION(operationMakeRope3, JSString*, (JSGlobalObject* globalObject, JSString* a, JSString* b, JSString* c))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsString(globalObject, a, b, c));
}

JSC_DEFINE_JIT_OPERATION(operationStrCat2, JSString*, (JSGlobalObject* globalObject, EncodedJSValue a, EncodedJSValue b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!JSValue::decode(a).isSymbol());
    ASSERT(!JSValue::decode(b).isSymbol());
    JSString* str1 = JSValue::decode(a).toString(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr); // OOM exception can happen from ToString(BigInt).
    JSString* str2 = JSValue::decode(b).toString(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, jsString(globalObject, str1, str2));
}
    
JSC_DEFINE_JIT_OPERATION(operationStrCat3, JSString*, (JSGlobalObject* globalObject, EncodedJSValue a, EncodedJSValue b, EncodedJSValue c))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!JSValue::decode(a).isSymbol());
    ASSERT(!JSValue::decode(b).isSymbol());
    ASSERT(!JSValue::decode(c).isSymbol());
    JSString* str1 = JSValue::decode(a).toString(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr); // OOM exception can happen from ToString(BigInt).
    JSString* str2 = JSValue::decode(b).toString(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    JSString* str3 = JSValue::decode(c).toString(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    OPERATION_RETURN(scope, jsString(globalObject, str1, str2, str3));
}

JSC_DEFINE_JIT_OPERATION(operationMakeAtomString1, JSString*, (JSGlobalObject* globalObject, JSString* a))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsAtomString(globalObject, vm, a));
}

JSC_DEFINE_JIT_OPERATION(operationMakeAtomString2, JSString*, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsAtomString(globalObject, vm, a, b));
}

JSC_DEFINE_JIT_OPERATION(operationMakeAtomString3, JSString*, (JSGlobalObject* globalObject, JSString* a, JSString* b, JSString* c))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsAtomString(globalObject, vm, a, b, c));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationFindSwitchImmTargetForDouble, char*, (VM* vmPointer, EncodedJSValue encodedValue, size_t tableIndex, int32_t min))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    CodeBlock* codeBlock = callFrame->codeBlock();
    const SimpleJumpTable& linkedTable = codeBlock->dfgSwitchJumpTable(tableIndex);
    JSValue value = JSValue::decode(encodedValue);
    ASSERT(value.isDouble());
    double asDouble = value.asDouble();
    int32_t asInt32 = static_cast<int32_t>(asDouble);
    if (asDouble == asInt32)
        OPERATION_RETURN(scope, linkedTable.ctiForValue(min, asInt32).taggedPtr<char*>());
    OPERATION_RETURN(scope, linkedTable.m_ctiDefault.taggedPtr<char*>());
}

JSC_DEFINE_JIT_OPERATION(operationSwitchString, char*, (JSGlobalObject* globalObject, size_t tableIndex, const UnlinkedStringJumpTable* unlinkedTable, JSString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto str = string->value(globalObject);

    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    CodeBlock* codeBlock = callFrame->codeBlock();
    const StringJumpTable& linkedTable = codeBlock->dfgStringSwitchJumpTable(tableIndex);
    OPERATION_RETURN(scope, linkedTable.ctiForValue(*unlinkedTable, str->impl()).taggedPtr<char*>());
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplLess, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) < 0;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplLessEq, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) <= 0;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplGreater, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) > 0;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplGreaterEq, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) >= 0;
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringLess, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, codePointCompareLessThan(asString(a)->value(globalObject), asString(b)->value(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringLessEq, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, !codePointCompareLessThan(asString(b)->value(globalObject), asString(a)->value(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringGreater, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, codePointCompareLessThan(asString(b)->value(globalObject), asString(a)->value(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringGreaterEq, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, !codePointCompareLessThan(asString(a)->value(globalObject), asString(b)->value(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationNotifyWrite, void, (VM* vmPointer, WatchpointSet* set))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    set->touch(vm, "Executed NotifyWrite");
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationThrowStackOverflowForVarargs, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwStackOverflowError(globalObject, scope);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationSizeOfVarargs, UCPUStrictInt32, (JSGlobalObject* globalObject, EncodedJSValue encodedArguments, uint32_t firstVarArgOffset))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    OPERATION_RETURN(scope, toUCPUStrictInt32(sizeOfVarargs(globalObject, arguments, firstVarArgOffset)));
}

JSC_DEFINE_JIT_OPERATION(operationHasOwnProperty, size_t, (JSGlobalObject* globalObject, JSObject* thisObject, EncodedJSValue encodedKey))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue key = JSValue::decode(encodedKey);
    Identifier propertyName = key.toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, false);

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);
    bool result = thisObject->hasOwnProperty(globalObject, propertyName.impl(), slot);
    OPERATION_RETURN_IF_EXCEPTION(scope, false);

    HasOwnPropertyCache* hasOwnPropertyCache = vm.hasOwnPropertyCache();
    ASSERT(hasOwnPropertyCache);
    hasOwnPropertyCache->tryAdd(slot, thisObject, propertyName.impl(), result);
    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationNumberIsInteger, size_t, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, NumberConstructor::isIntegerImpl(JSValue::decode(value)));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationNumberIsNaN, UCPUStrictInt32, (EncodedJSValue value))
{
    JSValue argument = JSValue::decode(value);
    if (!argument.isNumber())
        return toUCPUStrictInt32(0);
    return toUCPUStrictInt32(!!std::isnan(argument.asNumber()));
}

JSC_DEFINE_JIT_OPERATION(operationIsNaN, UCPUStrictInt32, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = JSValue::decode(value);
    OPERATION_RETURN(scope, toUCPUStrictInt32(std::isnan(argument.toNumber(globalObject))));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationToIntegerOrInfinityDouble, EncodedJSValue, (double d))
{
    return JSValue::encode(jsNumber(trunc(std::isnan(d) ? 0.0 : d + 0.0)));
}

JSC_DEFINE_JIT_OPERATION(operationToIntegerOrInfinityUntyped, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = JSValue::decode(encodedValue);
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(value.toIntegerOrInfinity(globalObject))));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationToLengthDouble, EncodedJSValue, (double argument))
{
    double d = trunc(std::isnan(argument) ? 0.0 : argument + 0.0);
    if (d <= 0)
        return JSValue::encode(jsNumber(0));
    return JSValue::encode(jsNumber(std::min(d, maxSafeInteger())));
}

JSC_DEFINE_JIT_OPERATION(operationToLengthUntyped, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = JSValue::decode(encodedValue);
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(value.toLength(globalObject))));
}

static ALWAYS_INLINE UCPUStrictInt32 arrayIndexOfString(JSGlobalObject* globalObject, Butterfly* butterfly, JSString* searchElement, int32_t index)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t length = butterfly->publicLength();
    auto data = butterfly->contiguous().data();
    for (; index < length; ++index) {
        JSValue value = data[index].get();
        if (!value || !value.isString())
            continue;
        auto* string = asString(value);
        if (string == searchElement)
            return toUCPUStrictInt32(index);
        if (string->equalInline(globalObject, searchElement)) {
            scope.assertNoExceptionExceptTermination();
            return toUCPUStrictInt32(index);
        }
        RETURN_IF_EXCEPTION(scope, { });
    }
    return toUCPUStrictInt32(-1);
}

JSC_DEFINE_JIT_OPERATION(operationArrayIndexOfString, UCPUStrictInt32, (JSGlobalObject* globalObject, Butterfly* butterfly, JSString* searchElement, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, arrayIndexOfString(globalObject, butterfly, searchElement, index));
}

JSC_DEFINE_JIT_OPERATION(operationArrayIndexOfValueInt32OrContiguous, UCPUStrictInt32, (JSGlobalObject* globalObject, Butterfly* butterfly, EncodedJSValue encodedValue, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue searchElement = JSValue::decode(encodedValue);

    if (searchElement.isString())
        OPERATION_RETURN(scope, arrayIndexOfString(globalObject, butterfly, asString(searchElement), index));

    int32_t length = butterfly->publicLength();
    auto data = butterfly->contiguous().data();

    if (index >= length)
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));

    if (searchElement.isObject()) {
        auto* result = bitwise_cast<const WriteBarrier<Unknown>*>(WTF::find64(bitwise_cast<const uint64_t*>(data + index), encodedValue, length - index));
        if (result)
            OPERATION_RETURN(scope, toUCPUStrictInt32(result - data));
        OPERATION_RETURN(scope, toUCPUStrictInt32(-1));
    }

    for (; index < length; ++index) {
        JSValue value = data[index].get();
        if (!value)
            continue;
        bool isEqual = JSValue::strictEqual(globalObject, searchElement, value);
        OPERATION_RETURN_IF_EXCEPTION(scope, 0);
        if (isEqual)
            OPERATION_RETURN(scope, toUCPUStrictInt32(index));
    }
    OPERATION_RETURN(scope, toUCPUStrictInt32(-1));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationArrayIndexOfValueDouble, UCPUStrictInt32, (Butterfly* butterfly, EncodedJSValue encodedValue, int32_t index))
{
    // We do not cause any exceptions, thus we do not need FrameTracers.
    JSValue searchElement = JSValue::decode(encodedValue);

    if (!searchElement.isNumber())
        return toUCPUStrictInt32(-1);
    double number = searchElement.asNumber();

    int32_t length = butterfly->publicLength();
    const double* data = butterfly->contiguousDouble().data();
    for (; index < length; ++index) {
        // This comparison ignores NaN.
        if (data[index] == number)
            return toUCPUStrictInt32(index);
    }
    return toUCPUStrictInt32(-1);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationArrayIndexOfNonStringIdentityValueContiguous, UCPUStrictInt32, (Butterfly* butterfly, EncodedJSValue searchElement, int32_t index))
{
    // We do not cause any exceptions, thus we do not need FrameTracers.
    int32_t length = butterfly->publicLength();
    auto data = butterfly->contiguous().data();

    if (index >= length)
        return toUCPUStrictInt32(-1);

    auto* result = bitwise_cast<const WriteBarrier<Unknown>*>(WTF::find64(bitwise_cast<const uint64_t*>(data + index), searchElement, length - index));
    if (result)
        return toUCPUStrictInt32(result - data);
    return toUCPUStrictInt32(-1);
}

JSC_DEFINE_JIT_OPERATION(operationLoadVarargs, void, (JSGlobalObject* globalObject, int32_t firstElementDest, EncodedJSValue encodedArguments, uint32_t offset, uint32_t lengthIncludingThis, uint32_t mandatoryMinimum))
{
    VirtualRegister firstElement { firstElementDest };
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    loadVarargs(globalObject, bitwise_cast<JSValue*>(&callFrame->r(firstElement)), arguments, offset, lengthIncludingThis - 1);
    
    for (uint32_t i = lengthIncludingThis - 1; i < mandatoryMinimum; ++i)
        callFrame->r(firstElement + i) = jsUndefined();
    OPERATION_RETURN(scope);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationFModOnInts, double, (int32_t a, int32_t b))
{
    return fmod(a, b);
}

#if USE(JSVALUE32_64)
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationRandom, double, (JSGlobalObject* globalObject))
{
    return globalObject->weakRandomNumber();
}
#endif

JSC_DEFINE_JIT_OPERATION(operationStringFromCharCode, JSCell*, (JSGlobalObject* globalObject, int32_t op1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSC::stringFromCharCode(globalObject, op1));
}

JSC_DEFINE_JIT_OPERATION(operationStringFromCharCodeUntyped, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue charValue = JSValue::decode(encodedValue);
    int32_t chInt = charValue.toUInt32(globalObject);
    OPERATION_RETURN(scope, JSValue::encode(JSC::stringFromCharCode(globalObject, chInt)));
}

JSC_DEFINE_JIT_OPERATION(operationNewRawObject, char*, (VM* vmPointer, Structure* structure, int32_t length, Butterfly* butterfly))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!butterfly
        && (structure->outOfLineCapacity() || hasIndexedProperties(structure->indexingType()))) {
        IndexingHeader header;
        header.setVectorLength(length);
        header.setPublicLength(0);
        
        butterfly = Butterfly::create(
            vm, nullptr, 0, structure->outOfLineCapacity(),
            hasIndexedProperties(structure->indexingType()), header,
            length * sizeof(EncodedJSValue));
    }

    if (structure->typeInfo().type() == JSType::ArrayType)
        OPERATION_RETURN(scope, bitwise_cast<char*>(JSArray::createWithButterfly(vm, nullptr, structure, butterfly)));
    OPERATION_RETURN(scope, bitwise_cast<char*>(JSFinalObject::createWithButterfly(vm, structure, butterfly)));
}

JSC_DEFINE_JIT_OPERATION(operationNewObjectWithButterfly, JSCell*, (VM* vmPointer, Structure* structure, Butterfly* butterfly))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!butterfly) {
        butterfly = Butterfly::create(
            vm, nullptr, 0, structure->outOfLineCapacity(), false, IndexingHeader(), 0);
    }
    
    if (structure->typeInfo().type() == JSType::ArrayType)
        OPERATION_RETURN(scope, JSArray::createWithButterfly(vm, nullptr, structure, butterfly));
    OPERATION_RETURN(scope, JSFinalObject::createWithButterfly(vm, structure, butterfly));
}

JSC_DEFINE_JIT_OPERATION(operationNewObjectWithButterflyWithIndexingHeaderAndVectorLength, JSCell*, (VM* vmPointer, Structure* structure, unsigned length, Butterfly* butterfly))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    IndexingHeader header;
    header.setVectorLength(length);
    header.setPublicLength(0);
    if (butterfly)
        *butterfly->indexingHeader() = header;
    else {
        butterfly = Butterfly::create(
            vm, nullptr, 0, structure->outOfLineCapacity(), true, header,
            sizeof(EncodedJSValue) * length);
    }
    
    if (structure->typeInfo().type() == JSType::ArrayType)
        OPERATION_RETURN(scope, JSArray::createWithButterfly(vm, nullptr, structure, butterfly));
    OPERATION_RETURN(scope, JSFinalObject::createWithButterfly(vm, structure, butterfly));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSpreadSlow, JSCell*, (JSGlobalObject* globalObject, void* buffer, uint32_t numItems))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(buffer), numItems);
    auto scope = DECLARE_THROW_SCOPE(vm);

    EncodedJSValue* values = static_cast<EncodedJSValue*>(buffer);
    CheckedUint32 checkedLength = 0;
    for (unsigned i = 0; i < numItems; i++) {
        JSValue value = JSValue::decode(values[i]);
        if (JSImmutableButterfly* array = jsDynamicCast<JSImmutableButterfly*>(value))
            checkedLength += array->publicLength();
        else
            ++checkedLength;
    }

    if (UNLIKELY(checkedLength.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        OPERATION_RETURN(scope, nullptr);
    }

    unsigned length = checkedLength;
    if (UNLIKELY(length >= MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)) {
        throwOutOfMemoryError(globalObject, scope);
        OPERATION_RETURN(scope, nullptr);
    }

    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);

    JSArray* result = JSArray::tryCreate(vm, structure, length);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        OPERATION_RETURN(scope, nullptr);
    }
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned index = 0;
    for (unsigned i = 0; i < numItems; i++) {
        JSValue value = JSValue::decode(values[i]);
        if (JSImmutableButterfly* array = jsDynamicCast<JSImmutableButterfly*>(value)) {
            // We are spreading.
            for (unsigned i = 0; i < array->publicLength(); i++) {
                result->putDirectIndex(globalObject, index, array->get(i));
                OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
                ++index;
            }
        } else {
            // We are not spreading.
            result->putDirectIndex(globalObject, index, value);
            OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
            ++index;
        }
    }

    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_JIT_OPERATION(operationCreateImmutableButterfly, JSCell*, (JSGlobalObject* globalObject, unsigned length))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (JSImmutableButterfly* result = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), length))
        OPERATION_RETURN(scope, result);

    throwOutOfMemoryError(globalObject, scope);
    OPERATION_RETURN(scope, nullptr);
}

JSC_DEFINE_JIT_OPERATION(operationSpreadGeneric, JSCell*, (JSGlobalObject* globalObject, JSCell* iterable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* result = CommonSlowPaths::trySpreadFast(globalObject, iterable);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
    if (result)
        OPERATION_RETURN(scope, result);

    // FIXME: we can probably make this path faster by having our caller JS code call directly into
    // the iteration protocol builtin: https://bugs.webkit.org/show_bug.cgi?id=164520

    JSArray* array;
    {
        JSFunction* iterationFunction = globalObject->iteratorProtocolFunction();
        auto callData = JSC::getCallData(iterationFunction);
        ASSERT(callData.type != CallData::Type::None);

        MarkedArgumentBuffer arguments;
        arguments.append(iterable);
        ASSERT(!arguments.hasOverflowed());
        JSValue arrayResult = call(globalObject, iterationFunction, callData, jsNull(), arguments);
        OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
        array = jsCast<JSArray*>(arrayResult);
    }

    OPERATION_RETURN(scope, JSImmutableButterfly::createFromArray(globalObject, vm, array));
}

JSC_DEFINE_JIT_OPERATION(operationSpreadFastArray, JSCell*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(isJSArray(cell));
    JSArray* array = jsCast<JSArray*>(cell);
    ASSERT(array->isIteratorProtocolFastAndNonObservable());

    OPERATION_RETURN(scope, JSImmutableButterfly::createFromArray(globalObject, vm, array));
}

static ALWAYS_INLINE JSObject* newArrayWithSpeciesImpl(JSGlobalObject* globalObject, uint64_t length, JSObject* array, IndexingType indexingType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, array, length);
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));

    if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
        return { };

    if (LIKELY(speciesResult.first == SpeciesConstructResult::FastPath)) {
        if (UNLIKELY(length > std::numeric_limits<unsigned>::max())) {
            throwRangeError(globalObject, scope, ArrayInvalidLengthError);
            return nullptr;
        }

        Structure* structure = nullptr;
        if (length >= MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)
            structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage);
        else
            structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType);
        JSArray* result = JSArray::tryCreate(vm, structure, length);
        if (UNLIKELY(!result)) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }
        return result;
    }

    ASSERT(speciesResult.first == SpeciesConstructResult::CreatedObject);
    return speciesResult.second;
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSpeciesInt32, JSObject*, (JSGlobalObject* globalObject, int32_t length, JSObject* array, IndexingType indexingType))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, newArrayWithSpeciesImpl(globalObject, length, array, indexingType));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSpecies, JSObject*, (JSGlobalObject* globalObject, EncodedJSValue encodedLength, JSObject* array, IndexingType indexingType))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint64_t length = static_cast<uint64_t>(JSValue::decode(encodedLength).asNumber());
    OPERATION_RETURN(scope, newArrayWithSpeciesImpl(globalObject, length, array, indexingType));
}


JSC_DEFINE_JIT_OPERATION(operationProcessTypeProfilerLogDFG, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    vm.typeProfilerLog()->processLogEntries(vm, "Log Full, called from inside DFG."_s);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationResolveScopeForHoistingFuncDeclInEval, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* jsScope, UniquedStringImpl* impl))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
        
    JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(globalObject, jsScope, Identifier::fromUid(vm, impl));
    OPERATION_RETURN(scope, JSValue::encode(resolvedScope));
}
    
JSC_DEFINE_JIT_OPERATION(operationResolveScope, JSCell*, (JSGlobalObject* globalObject, JSScope* jsScope, UniquedStringImpl* impl))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* resolvedScope = JSScope::resolve(globalObject, jsScope, Identifier::fromUid(vm, impl));
    OPERATION_RETURN(scope, resolvedScope);
}

JSC_DEFINE_JIT_OPERATION(operationGetDynamicVar, EncodedJSValue, (JSGlobalObject* globalObject, JSObject* jsScope, UniquedStringImpl* impl, unsigned getPutInfoBits))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier ident = Identifier::fromUid(vm, impl);
    OPERATION_RETURN(scope, JSValue::encode(jsScope->getPropertySlot(globalObject, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            GetPutInfo getPutInfo(getPutInfoBits);
            if (getPutInfo.resolveMode() == ThrowIfNotFound)
                throwException(globalObject, scope, createUndefinedVariableError(globalObject, ident));
            return jsUndefined();
        }

        if (jsScope->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            JSValue result = slot.getValue(globalObject, ident);
            if (result == jsTDZValue()) {
                throwException(globalObject, scope, createTDZError(globalObject));
                return jsUndefined();
            }
            return result;
        }

        return slot.getValue(globalObject, ident);
    })));
}

ALWAYS_INLINE static void putDynamicVar(JSGlobalObject* globalObject, VM& vm, JSObject* scope, EncodedJSValue value, UniquedStringImpl* impl, unsigned getPutInfoBits, bool isStrictMode)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    const Identifier& ident = Identifier::fromUid(vm, impl);
    GetPutInfo getPutInfo(getPutInfoBits);
    bool hasProperty = scope->hasProperty(globalObject, ident);
    RETURN_IF_EXCEPTION(throwScope, void());
    if (hasProperty
        && scope->isGlobalLexicalEnvironment()
        && !isInitialization(getPutInfo.initializationMode())) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(scope, globalObject, ident, slot);
        if (slot.getValue(globalObject, ident) == jsTDZValue()) {
            throwException(globalObject, throwScope, createTDZError(globalObject));
            return;
        }
    }

    if (getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty) {
        throwException(globalObject, throwScope, createUndefinedVariableError(globalObject, ident));
        return;
    }

    PutPropertySlot slot(scope, isStrictMode, PutPropertySlot::UnknownContext, isInitialization(getPutInfo.initializationMode()));
    throwScope.release();
    scope->methodTable()->put(scope, globalObject, ident, JSValue::decode(value), slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutDynamicVarStrict, void, (JSGlobalObject* globalObject, JSObject* jsScope, EncodedJSValue value, UniquedStringImpl* impl, unsigned getPutInfoBits))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isStrictMode = true;
    putDynamicVar(globalObject, vm, jsScope, value, impl, getPutInfoBits, isStrictMode);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutDynamicVarSloppy, void, (JSGlobalObject* globalObject, JSObject* jsScope, EncodedJSValue value, UniquedStringImpl* impl, unsigned getPutInfoBits))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isStrictMode = false;
    putDynamicVar(globalObject, vm, jsScope, value, impl, getPutInfoBits, isStrictMode);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationNormalizeMapKeyHeapBigInt, EncodedJSValue, (VM* vmPointer, JSBigInt* input))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(normalizeMapKey(input)));
}

JSC_DEFINE_JIT_OPERATION(operationMapHash, UCPUStrictInt32, (JSGlobalObject* globalObject, EncodedJSValue input))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, toUCPUStrictInt32(jsMapHash(globalObject, vm, JSValue::decode(input))));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMapHashHeapBigInt, UCPUStrictInt32, (VM* vmPointer, JSBigInt* input))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, toUCPUStrictInt32(jsMapHash(input)));
}

JSC_DEFINE_JIT_OPERATION(operationMapGet, JSValue*, (JSGlobalObject* globalObject, JSCell* cell, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSMap* map = jsCast<JSMap*>(cell);
    JSValue* keySlot = map->getKeySlot(globalObject, JSValue::decode(key), hash);
    OPERATION_RETURN(scope, keySlot);
}
JSC_DEFINE_JIT_OPERATION(operationSetGet, JSValue*, (JSGlobalObject* globalObject, JSCell* cell, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSSet* set = jsCast<JSSet*>(cell);
    JSValue* keySlot = set->getKeySlot(globalObject, JSValue::decode(key), hash);
    OPERATION_RETURN(scope, keySlot);
}

JSC_DEFINE_JIT_OPERATION(operationMapStorage, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsCast<JSMap*>(cell)->storageOrSentinel(vm)));
}
JSC_DEFINE_JIT_OPERATION(operationSetStorage, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsCast<JSSet*>(cell)->storageOrSentinel(vm)));
}

JSC_DEFINE_JIT_OPERATION(operationMapIterationNext, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (cell == vm.orderedHashTableSentinel())
        OPERATION_RETURN(scope, JSValue::encode(vm.orderedHashTableSentinel()));

    JSMap::Storage& storage = *jsCast<JSMap::Storage*>(cell);
    OPERATION_RETURN(scope, JSValue::encode(JSMap::Helper::nextAndUpdateIterationEntry(vm, storage, index)));
}
JSC_DEFINE_JIT_OPERATION(operationMapIterationEntry, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(cell != vm.orderedHashTableSentinel());
    JSMap::Storage& storage = *jsCast<JSMap::Storage*>(cell);
    OPERATION_RETURN(scope, JSValue::encode(JSMap::Helper::getIterationEntry(storage)));
}
JSC_DEFINE_JIT_OPERATION(operationMapIterationEntryKey, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(cell != vm.orderedHashTableSentinel());
    JSMap::Storage& storage = *jsCast<JSMap::Storage*>(cell);
    OPERATION_RETURN(scope, JSValue::encode(JSMap::Helper::getIterationEntryKey(storage)));
}
JSC_DEFINE_JIT_OPERATION(operationMapIterationEntryValue, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(cell != vm.orderedHashTableSentinel());
    JSMap::Storage& storage = *jsCast<JSMap::Storage*>(cell);
    OPERATION_RETURN(scope, JSValue::encode(JSMap::Helper::getIterationEntryValue(storage)));
}

JSC_DEFINE_JIT_OPERATION(operationSetIterationNext, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (cell == vm.orderedHashTableSentinel())
        OPERATION_RETURN(scope, JSValue::encode(vm.orderedHashTableSentinel()));

    JSSet::Storage& storage = *jsCast<JSSet::Storage*>(cell);
    OPERATION_RETURN(scope, JSValue::encode(JSSet::Helper::nextAndUpdateIterationEntry(vm, storage, index)));
}
JSC_DEFINE_JIT_OPERATION(operationSetIterationEntry, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(cell != vm.orderedHashTableSentinel());
    JSSet::Storage& storage = *jsCast<JSSet::Storage*>(cell);
    OPERATION_RETURN(scope, JSValue::encode(JSSet::Helper::getIterationEntry(storage)));
}
JSC_DEFINE_JIT_OPERATION(operationSetIterationEntryKey, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(cell != vm.orderedHashTableSentinel());
    JSSet::Storage& storage = *jsCast<JSSet::Storage*>(cell);
    OPERATION_RETURN(scope, JSValue::encode(JSSet::Helper::getIterationEntryKey(storage)));
}

JSC_DEFINE_JIT_OPERATION(operationMapIteratorNext, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsCast<JSMapIterator*>(cell)->next(vm)));
}
JSC_DEFINE_JIT_OPERATION(operationMapIteratorKey, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsCast<JSMapIterator*>(cell)->nextKey(vm)));
}
JSC_DEFINE_JIT_OPERATION(operationMapIteratorValue, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsCast<JSMapIterator*>(cell)->nextValue(vm)));
}

JSC_DEFINE_JIT_OPERATION(operationSetIteratorNext, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsCast<JSSetIterator*>(cell)->next(vm)));
}
JSC_DEFINE_JIT_OPERATION(operationSetIteratorKey, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsCast<JSSetIterator*>(cell)->nextKey(vm)));
}

JSC_DEFINE_JIT_OPERATION(operationSetAdd, void, (JSGlobalObject* globalObject, JSCell* set, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    jsCast<JSSet*>(set)->addNormalized(globalObject, JSValue::decode(key), JSValue(), hash);
    OPERATION_RETURN(scope);
}
JSC_DEFINE_JIT_OPERATION(operationMapSet, void, (JSGlobalObject* globalObject, JSCell* map, EncodedJSValue key, EncodedJSValue value, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    jsCast<JSMap*>(map)->addNormalized(globalObject, JSValue::decode(key), JSValue::decode(value), hash);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationSetDelete, size_t, (JSGlobalObject* globalObject, JSCell* set, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, jsCast<JSSet*>(set)->removeNormalized(globalObject, JSValue::decode(key), hash));
}

JSC_DEFINE_JIT_OPERATION(operationMapDelete, size_t, (JSGlobalObject* globalObject, JSCell* map, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, jsCast<JSMap*>(map)->removeNormalized(globalObject, JSValue::decode(key), hash));
}

JSC_DEFINE_JIT_OPERATION(operationNewMap, JSMap*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSMap::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewSet, JSSet*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSSet::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationWeakSetAdd, void, (JSGlobalObject* globalObject, JSCell* set, JSCell* key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!canBeHeldWeakly(key))) {
        throwTypeError(globalObject, scope, WeakSetInvalidValueError);
        OPERATION_RETURN(scope);
    }

    jsCast<JSWeakSet*>(set)->add(vm, key, JSValue(), hash);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationWeakMapSet, void, (JSGlobalObject* globalObject, JSCell* map, JSCell* key, EncodedJSValue value, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!canBeHeldWeakly(key))) {
        throwTypeError(globalObject, scope, WeakMapInvalidKeyError);
        OPERATION_RETURN(scope);
    }

    jsCast<JSWeakMap*>(map)->add(vm, key, JSValue::decode(value), hash);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationGetPrototypeOfObject, EncodedJSValue, (JSGlobalObject* globalObject, JSObject* thisObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(thisObject->getPrototype(vm, globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrototypeOf, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = JSValue::decode(encodedValue);
    OPERATION_RETURN(scope, JSValue::encode(value.getPrototype(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetFullYear, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->year())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCFullYear, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->year())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetMonth, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->month())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCMonth, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->month())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetDate, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->monthDay())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCDate, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->monthDay())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetDay, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->weekDay())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCDay, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->weekDay())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetHours, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->hour())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCHours, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->hour())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetMinutes, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->minute())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCMinutes, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->minute())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetSeconds, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->second())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCSeconds, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->second())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetTimezoneOffset, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(-gregorianDateTime->utcOffsetInMinute())));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetYear, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        OPERATION_RETURN(scope, JSValue::encode(jsNaN()));
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(gregorianDateTime->year() - 1900)));
}

JSC_DEFINE_JIT_OPERATION(operationInt64ToBigInt, EncodedJSValue, (JSGlobalObject* globalObject, int64_t value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, value)));
}

JSC_DEFINE_JIT_OPERATION(operationThrowDFG, void, (JSGlobalObject* globalObject, EncodedJSValue valueToThrow))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    scope.throwException(globalObject, JSValue::decode(valueToThrow));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationThrowStaticError, void, (JSGlobalObject* globalObject, JSString* message, uint32_t errorType))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto errorMessage = message->value(globalObject);
    scope.throwException(globalObject, createError(globalObject, static_cast<ErrorTypeWithExtension>(errorType), errorMessage));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationLinkDirectCall, void, (DirectCallLinkInfo* callLinkInfo, JSFunction* callee))
{
    JSGlobalObject* globalObject = callee->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CodeSpecializationKind kind = callLinkInfo->specializationKind();

    JSScope* jsScope = callee->scopeUnchecked();
    ExecutableBase* executable = callee->executable();

    // FIXME: Support wasm IC.
    // https://bugs.webkit.org/show_bug.cgi?id=220339
    CodePtr<JSEntryPtrTag> codePtr;
    CodeBlock* codeBlock = nullptr;
    DeferTraps deferTraps(vm); // We can't jettison this code if we're about to link to it.

    if (executable->isHostFunction())
        codePtr = executable->entrypointFor(kind, MustCheckArity);
    else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        RELEASE_ASSERT(isCall(kind) || functionExecutable->constructAbility() != ConstructAbility::CannotConstruct);

        functionExecutable->prepareForExecution<FunctionExecutable>(vm, callee, jsScope, kind, codeBlock);
        OPERATION_RETURN_IF_EXCEPTION(scope);

        unsigned argumentStackSlots = callLinkInfo->maxArgumentCountIncludingThis();
        if (argumentStackSlots < static_cast<size_t>(codeBlock->numParameters()))
            codePtr = functionExecutable->entrypointFor(kind, MustCheckArity);
        else
            codePtr = functionExecutable->entrypointFor(kind, ArityCheckNotRequired);
    }

    linkDirectCall(*callLinkInfo, codeBlock, codePtr);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationTriggerReoptimizationNow, void, (CodeBlock* codeBlock, CodeBlock* optimizedCodeBlock, OSRExitBase* exit))
{
    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(codeBlock->vm());
    
    sanitizeStackForVM(codeBlock->vm());

    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Entered reoptimize");
    // We must be called with the baseline code block.
    ASSERT(JITCode::isBaselineCode(codeBlock->jitType()));

    // If I am my own replacement, then reoptimization has already been triggered.
    // This can happen in recursive functions.
    //
    // Note that even if optimizedCodeBlock is an FTLForOSREntry style CodeBlock, this condition is a
    // sure bet that we don't have anything else left to do.
    CodeBlock* replacement = codeBlock->replacement();
    if (!replacement || replacement == codeBlock) {
        dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Not reoptimizing because we've already been jettisoned.");
        return;
    }
    
    // Otherwise, the replacement must be optimized code. Use this as an opportunity
    // to check our logic.
    ASSERT(codeBlock->hasOptimizedReplacement());
    ASSERT(JSC::JITCode::isOptimizingJIT(optimizedCodeBlock->jitType()));
    
    bool didTryToEnterIntoInlinedLoops = false;
    for (InlineCallFrame* inlineCallFrame = exit->m_codeOrigin.inlineCallFrame(); inlineCallFrame; inlineCallFrame = inlineCallFrame->directCaller.inlineCallFrame()) {
        if (inlineCallFrame->baselineCodeBlock->ownerExecutable()->didTryToEnterInLoop()) {
            didTryToEnterIntoInlinedLoops = true;
            break;
        }
    }

    // In order to trigger reoptimization, one of two things must have happened:
    // 1) We exited more than some number of times.
    // 2) We exited and got stuck in a loop, and now we're exiting again.
    bool didExitABunch = optimizedCodeBlock->shouldReoptimizeNow();
    bool didGetStuckInLoop =
        (codeBlock->checkIfOptimizationThresholdReached() || didTryToEnterIntoInlinedLoops)
        && optimizedCodeBlock->shouldReoptimizeFromLoopNow();
    
    if (!didExitABunch && !didGetStuckInLoop) {
        dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Not reoptimizing ", *optimizedCodeBlock, " because it either didn't exit enough or didn't loop enough after exit.");
        codeBlock->optimizeAfterLongWarmUp();
        return;
    }
    
    optimizedCodeBlock->jettison(Profiler::JettisonDueToOSRExit, CountReoptimization);
}

#if ENABLE(FTL_JIT)
static bool shouldTriggerFTLCompile(CodeBlock* codeBlock, JITCode* jitCode)
{
    if (codeBlock->baselineVersion()->m_didFailFTLCompilation) {
        CODEBLOCK_LOG_EVENT(codeBlock, "abortFTLCompile", ());
        dataLogLnIf(Options::verboseOSR(), "Deferring FTL-optimization of ", *codeBlock, " indefinitely because there was an FTL failure.");
        jitCode->dontOptimizeAnytimeSoon(codeBlock);
        return false;
    }

    if (!codeBlock->hasOptimizedReplacement() && !jitCode->checkIfOptimizationThresholdReached(codeBlock)) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("counter = ", codeBlock->dfgJITData()->tierUpCounter()));
        dataLogLnIf(Options::verboseOSR(), "Choosing not to FTL-optimize ", *codeBlock, " yet.");
        return false;
    }
    return true;
}

static void triggerFTLReplacementCompile(VM& vm, CodeBlock* codeBlock, JITCode* jitCode)
{
    if (codeBlock->codeType() == GlobalCode) {
        // Global code runs once, so we don't want to do anything. We don't want to defer indefinitely,
        // since this may have been spuriously called from tier-up initiated in a loop, and that loop may
        // later want to run faster code. Deferring for warm-up seems safest.
        jitCode->optimizeAfterWarmUp(codeBlock);
        return;
    }
    
    JITWorklist::State worklistState = JITWorklist::ensureGlobalWorklist().completeAllReadyPlansForVM(
        vm, JITCompilationKey(codeBlock->baselineVersion(), JITCompilationMode::FTL));
    
    if (worklistState == JITWorklist::Compiling) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("still compiling"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return;
    }
    
    if (codeBlock->hasOptimizedReplacement()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("has replacement"));
        // That's great, we've compiled the code - next time we call this function,
        // we'll enter that replacement.
        jitCode->optimizeSoon(codeBlock);
        return;
    }
    
    if (worklistState == JITWorklist::Compiled) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("compiled and failed"));
        // This means that we finished compiling, but failed somehow; in that case the
        // thresholds will be set appropriately.
        dataLogLnIf(Options::verboseOSR(), "Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.");
        return;
    }

    CODEBLOCK_LOG_EVENT(codeBlock, "triggerFTLReplacement", ());
    // We need to compile the code.
    compile(
        vm, codeBlock->newReplacement(), codeBlock, JITCompilationMode::FTL, BytecodeIndex(),
        Operands<std::optional<JSValue>>(), ToFTLDeferredCompilationCallback::create());

    // If we reached here, the counter has not be reset. Do that now.
    jitCode->setOptimizationThresholdBasedOnCompilationResult(
        codeBlock, CompilationDeferred);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationTriggerTierUpNow, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    DeferGCForAWhile deferGC(vm);
    CodeBlock* codeBlock = callFrame->codeBlock();
    
    sanitizeStackForVM(vm);

    if (codeBlock->jitType() != JITType::DFGJIT) {
        dataLog("Unexpected code block in DFG->FTL tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    JITCode* jitCode = codeBlock->jitCode()->dfg();
    
    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Entered triggerTierUpNow with executeCounter = ", codeBlock->dfgJITData()->tierUpCounter());

    if (shouldTriggerFTLCompile(codeBlock, jitCode))
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    if (codeBlock->hasOptimizedReplacement()) {
        if (jitCode->tierUpEntryTriggers.isEmpty()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("replacement in place, delaying indefinitely"));
            // There is nothing more we can do, the only way this will be entered
            // is through the function entry point.
            jitCode->dontOptimizeAnytimeSoon(codeBlock);
            return;
        }
        if (jitCode->osrEntryBlock() && jitCode->tierUpEntryTriggers.size() == 1) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("trigger in place, delaying indefinitely"));
            // There is only one outer loop and its trigger must have been set
            // when the plan completed.
            // Exiting the inner loop is useless, we can ignore the counter and leave
            // the trigger do its job.
            jitCode->dontOptimizeAnytimeSoon(codeBlock);
            return;
        }
    }
}

static char* tierUpCommon(VM& vm, CallFrame* callFrame, BytecodeIndex originBytecodeIndex, bool canOSREnterHere)
{
    CodeBlock* codeBlock = callFrame->codeBlock();

    // Resolve any pending plan for OSR Enter on this function.
    JITWorklist::State worklistState = JITWorklist::ensureGlobalWorklist().completeAllReadyPlansForVM(
        vm, JITCompilationKey(codeBlock->baselineVersion(), JITCompilationMode::FTLForOSREntry));

    JITCode* jitCode = codeBlock->jitCode()->dfg();
    
    bool triggeredSlowPathToStartCompilation = false;
    auto tierUpEntryTriggers = jitCode->tierUpEntryTriggers.find(originBytecodeIndex);
    if (tierUpEntryTriggers != jitCode->tierUpEntryTriggers.end()) {
        switch (tierUpEntryTriggers->value) {
        case JITCode::TriggerReason::DontTrigger:
            // The trigger isn't set, we entered because the counter reached its
            // threshold.
            break;

        case JITCode::TriggerReason::CompilationDone:
            // The trigger was set because compilation completed. Don't unset it
            // so that further DFG executions OSR enter as well.
            break;

        case JITCode::TriggerReason::StartCompilation:
            // We were asked to enter as soon as possible and start compiling an
            // entry for the current bytecode location. Unset this trigger so we
            // don't continually enter.
            tierUpEntryTriggers->value = JITCode::TriggerReason::DontTrigger;
            triggeredSlowPathToStartCompilation = true;
            break;
        }
    }

    if (worklistState == JITWorklist::Compiling) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("still compiling"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
        return nullptr;
    }

    auto failedOSREntry = [&] (JITCode* jitCode) {
        CodeBlock* entryBlock = jitCode->osrEntryBlock();
        if (!entryBlock) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR entry code is already invalidated"));
            codeBlock->baselineVersion()->countReoptimization();
            // clearOSREntryBlockAndResetThresholds is already called in FTL::prepareOSREntry and because of that,
            // jitCode->osrEntryBlock() is nullptr.
            return nullptr;
        }

        FTL::ForOSREntryJITCode* entryCode = entryBlock->jitCode()->ftlForOSREntry();
        entryCode->countEntryFailure();
        if (entryCode->entryFailureCount() <
            Options::ftlOSREntryFailureCountForReoptimization()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR entry failed"));
            jitCode->setOptimizationThresholdBasedOnCompilationResult(
                codeBlock, CompilationDeferred);
            return nullptr;
        }

        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR entry failed too many times"));
        codeBlock->baselineVersion()->countReoptimization();
        jitCode->clearOSREntryBlockAndResetThresholds(codeBlock);
        return nullptr;
    };

    // If we can OSR Enter, do it right away.
    if (canOSREnterHere) {
        auto iter = jitCode->bytecodeIndexToStreamIndex.find(originBytecodeIndex);
        if (iter != jitCode->bytecodeIndexToStreamIndex.end()) {
            unsigned streamIndex = iter->value;
            if (CodeBlock* entryBlock = jitCode->osrEntryBlock()) {
                dataLogLnIf(Options::verboseOSR(), "OSR entry: From ", RawPointer(jitCode), " got entry block ", RawPointer(entryBlock));
                // If FTL::prepareOSREntry returns address, we must perform OSR entry since it already set up environment for FTL.
                ASSERT(canOSREnterHere);
                if (void* address = FTL::prepareOSREntry(vm, callFrame, codeBlock, entryBlock, originBytecodeIndex, streamIndex)) {
                    CODEBLOCK_LOG_EVENT(entryBlock, "osrEntry", ("at ", originBytecodeIndex));
                    return tagCodePtrWithStackPointerForJITCall(untagCodePtr<char*, JSEntryPtrTag>(address), callFrame);
                }

                return failedOSREntry(jitCode);
            }
        }
    }

    if (worklistState == JITWorklist::Compiled) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("compiled and failed"));
        // This means that compilation failed and we already set the thresholds.
        dataLogLnIf(Options::verboseOSR(), "Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.");
        return nullptr;
    }

    // - If we don't have an FTL code block, then try to compile one.
    // - If we do have an FTL code block, then try to enter for a while.
    // - If we couldn't enter for a while, then trigger OSR entry.

    if (!shouldTriggerFTLCompile(codeBlock, jitCode) && !triggeredSlowPathToStartCompilation)
        return nullptr;

    if (!codeBlock->dfgJITData()->neverExecutedEntry() && !triggeredSlowPathToStartCompilation) {
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

        if (!codeBlock->hasOptimizedReplacement())
            return nullptr;

        if (jitCode->osrEntryRetry < Options::ftlOSREntryRetryThreshold()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("avoiding OSR entry compile"));
            jitCode->osrEntryRetry++;
            return nullptr;
        }
    } else
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("avoiding replacement compile"));

    if (jitCode->osrEntryBlock()) {
        if (jitCode->osrEntryRetry < Options::ftlOSREntryRetryThreshold()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR entry failed, OSR entry threshold not met"));
            jitCode->osrEntryRetry++;
            jitCode->setOptimizationThresholdBasedOnCompilationResult(
                codeBlock, CompilationDeferred);
            return nullptr;
        }

        return failedOSREntry(jitCode);
    }

    // It's time to try to compile code for OSR entry.

    if (!triggeredSlowPathToStartCompilation) {

        // An inner loop didn't specifically ask for us to kick off a compilation. This means the counter
        // crossed its threshold. We either fall through and kick off a compile for originBytecodeIndex,
        // or we flag an outer loop to immediately try to compile itself. If there are outer loops,
        // we first try to make them compile themselves. But we will eventually fall back to compiling
        // a progressively inner loop if it takes too long for control to reach an outer loop.

        auto tryTriggerOuterLoopToCompile = [&] {
            auto tierUpHierarchyEntry = jitCode->tierUpInLoopHierarchy.find(originBytecodeIndex);
            if (tierUpHierarchyEntry == jitCode->tierUpInLoopHierarchy.end())
                return false;

            // This vector is ordered from innermost to outermost loop. Every bytecode entry in this vector is
            // allowed to do OSR entry. We start with the outermost loop and make our way inwards (hence why we
            // iterate the vector in reverse). Our policy is that we will trigger an outer loop to compile
            // immediately when program control reaches it. If program control is taking too long to reach that
            // outer loop, we progressively move inwards, meaning, we'll eventually trigger some loop that is
            // executing to compile. We start with trying to compile outer loops since we believe outer loop
            // compilations reveal the best opportunities for optimizing code.
            for (auto iter = tierUpHierarchyEntry->value.rbegin(), end = tierUpHierarchyEntry->value.rend(); iter != end; ++iter) {
                BytecodeIndex osrEntryCandidate = *iter;

                if (jitCode->tierUpEntryTriggers.get(osrEntryCandidate) == JITCode::TriggerReason::StartCompilation) {
                    // This means that we already asked this loop to compile. If we've reached here, it
                    // means program control has not yet reached that loop. So it's taking too long to compile.
                    // So we move on to asking the inner loop of this loop to compile itself.
                    continue;
                }

                // This is where we ask the outer to loop to immediately compile itself if program
                // control reaches it.
                dataLogLnIf(Options::verboseOSR(), "Inner-loop ", originBytecodeIndex, " in ", *codeBlock, " setting parent loop ", osrEntryCandidate, "'s trigger and backing off.");
                jitCode->tierUpEntryTriggers.set(osrEntryCandidate, JITCode::TriggerReason::StartCompilation);
                return true;
            }

            return false;
        };

        if (tryTriggerOuterLoopToCompile()) {
            jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
            return nullptr;
        }
    }

    if (!canOSREnterHere) {
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
        return nullptr;
    }

    // We aren't compiling and haven't compiled anything for OSR entry. So, try to compile
    // something.

    auto triggerIterator = jitCode->tierUpEntryTriggers.find(originBytecodeIndex);
    if (triggerIterator == jitCode->tierUpEntryTriggers.end()) {
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
        return nullptr;
    }

    JITCode::TriggerReason* triggerAddress = &(triggerIterator->value);

    Operands<std::optional<JSValue>> mustHandleValues;
    unsigned streamIndex = jitCode->bytecodeIndexToStreamIndex.get(originBytecodeIndex);
    jitCode->reconstruct(callFrame, codeBlock, CodeOrigin(originBytecodeIndex), streamIndex, mustHandleValues);
    CodeBlock* replacementCodeBlock = codeBlock->newReplacement();

    CODEBLOCK_LOG_EVENT(codeBlock, "triggerFTLOSR", ());
    CompilationResult forEntryResult = compile(
        vm, replacementCodeBlock, codeBlock, JITCompilationMode::FTLForOSREntry, originBytecodeIndex,
        WTFMove(mustHandleValues), ToFTLForOSREntryDeferredCompilationCallback::create(triggerAddress));

    if (codeBlock->dfgJITData()->neverExecutedEntry())
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    if (forEntryResult != CompilationSuccessful) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR ecompilation not successful"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return nullptr;
    }
    
    CODEBLOCK_LOG_EVENT(jitCode->osrEntryBlock(), "osrEntry", ("at ", originBytecodeIndex));
    // It's possible that the for-entry compile already succeeded. In that case OSR
    // entry will succeed unless we ran out of stack. It's not clear what we should do.
    // We signal to try again after a while if that happens.
    dataLogLnIf(Options::verboseOSR(), "Immediate OSR entry: From ", RawPointer(jitCode), " got entry block ", RawPointer(jitCode->osrEntryBlock()));

    // If FTL::prepareOSREntry returns address, we must perform OSR entry since it already set up environment for FTL.
    ASSERT(canOSREnterHere);
    void* address = FTL::prepareOSREntry(vm, callFrame, codeBlock, jitCode->osrEntryBlock(), originBytecodeIndex, streamIndex);
    if (!address)
        return failedOSREntry(jitCode);
    return tagCodePtrWithStackPointerForJITCall(untagCodePtr<char*, JSEntryPtrTag>(address), callFrame);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationTriggerTierUpNowInLoop, void, (VM* vmPointer, unsigned bytecodeIndexBits))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    DeferGCForAWhile deferGC(vm);
    CodeBlock* codeBlock = callFrame->codeBlock();
    BytecodeIndex bytecodeIndex = BytecodeIndex::fromBits(bytecodeIndexBits);

    sanitizeStackForVM(vm);

    if (codeBlock->jitType() != JITType::DFGJIT) {
        dataLogLn("Unexpected code block in DFG->FTL trigger tier up now in loop: ", *codeBlock);
        RELEASE_ASSERT_NOT_REACHED();
    }

    JITCode* jitCode = codeBlock->jitCode()->dfg();

    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Entered triggerTierUpNowInLoop with executeCounter = ", codeBlock->dfgJITData()->tierUpCounter());

    if (jitCode->tierUpInLoopHierarchy.contains(bytecodeIndex))
        tierUpCommon(vm, callFrame, bytecodeIndex, false);
    else if (shouldTriggerFTLCompile(codeBlock, jitCode))
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    // Since we cannot OSR Enter here, the default "optimizeSoon()" is not useful.
    if (codeBlock->hasOptimizedReplacement()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR in loop failed, deferring"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
    }
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationTriggerOSREntryNow, char*, (VM* vmPointer, unsigned bytecodeIndexBits))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    DeferGCForAWhile deferGC(vm);
    CodeBlock* codeBlock = callFrame->codeBlock();
    BytecodeIndex bytecodeIndex = BytecodeIndex::fromBits(bytecodeIndexBits);

    sanitizeStackForVM(vm);

    if (codeBlock->jitType() != JITType::DFGJIT) {
        dataLog("Unexpected code block in DFG->FTL tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Entered triggerOSREntryNow with executeCounter = ", codeBlock->dfgJITData()->tierUpCounter());

    return tierUpCommon(vm, callFrame, bytecodeIndex, true);
}

#endif // ENABLE(FTL_JIT)

} } // namespace JSC::DFG

IGNORE_WARNINGS_END

#endif // ENABLE(DFG_JIT)

#endif // ENABLE(JIT)
