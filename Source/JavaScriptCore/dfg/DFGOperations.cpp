/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#include "JITCodeInlines.h"
#include "JITWorklist.h"
#include "JSArrayInlines.h"
#include "JSArrayIterator.h"
#include "JSAsyncGenerator.h"
#include "JSBigInt.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSImmutableButterfly.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
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
#include "ObjectConstructorInlines.h"
#include "ObjectPrototypeInlines.h"
#include "Operations.h"
#include "ParseInt.h"
#include "RegExpGlobalDataInlines.h"
#include "RegExpMatchesArray.h"
#include "RegExpObjectInlines.h"
#include "Repatch.h"
#include "ScopedArguments.h"
#include "StringConstructor.h"
#include "StringPrototypeInlines.h"
#include "SuperSampler.h"
#include "Symbol.h"
#include "TypeProfilerLog.h"
#include "VMInlines.h"
#include "VMTrapsInlines.h"
#include "WeakMapImplInlines.h"
#include "WeakMapPrototype.h"
#include "WeakSetPrototype.h"

#if ENABLE(JIT)
#if ENABLE(DFG_JIT)

IGNORE_WARNINGS_BEGIN("frame-address")

namespace JSC { namespace DFG {

template<bool strict, bool direct>
static ALWAYS_INLINE void putByVal(JSGlobalObject* globalObject, VM& vm, JSValue baseValue, uint32_t index, JSValue value)
{
    ASSERT(isIndex(index));
    if constexpr (direct) {
        RELEASE_ASSERT(baseValue.isObject());
        asObject(baseValue)->putDirectIndex(globalObject, index, value, 0, strict ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }
    if (baseValue.isObject()) {
        JSObject* object = asObject(baseValue);
        if (object->trySetIndexQuickly(vm, index, value))
            return;

        object->methodTable()->putByIndex(object, globalObject, index, value, strict);
        return;
    }

    baseValue.putByIndex(globalObject, index, value, strict);
}

template<bool strict, bool direct>
ALWAYS_INLINE static void putByValInternal(JSGlobalObject* globalObject, VM& vm, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);
    JSValue value = JSValue::decode(encodedValue);

    if (std::optional<uint32_t> index = property.tryGetAsUint32Index()) {
        scope.release();
        putByVal<strict, direct>(globalObject, vm, baseValue, *index, value);
        return;
    }

    // Don't put to an object if toString throws an exception.
    auto propertyName = property.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    PutPropertySlot slot(baseValue, strict);
    if constexpr (direct) {
        RELEASE_ASSERT(baseValue.isObject());
        JSObject* baseObject = asObject(baseValue);
        if (std::optional<uint32_t> index = parseIndex(propertyName)) {
            scope.release();
            baseObject->putDirectIndex(globalObject, index.value(), value, 0, strict ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
            return;
        }
        scope.release();
        CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, propertyName, value, slot);
        return;
    }
    scope.release();
    baseValue.put(globalObject, propertyName, value, slot);
}

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
        return bitwise_cast<char*>(ViewClass::createWithFastVector(globalObject, structure, unsignedSize, untagArrayPtr(vector, unsignedSize)));

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

    return JSValue::encode(JSValue::decode(encodedOp).toThis(globalObject, ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationToThisStrict, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(JSValue::decode(encodedOp).toThis(globalObject, ECMAMode::strict()));
}

JSC_DEFINE_JIT_OPERATION(operationObjectKeys, JSArray*, (JSGlobalObject* globalObject, EncodedJSValue encodedObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(encodedObject).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    scope.release();
    return ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude, CachedPropertyNamesKind::Keys);
}

JSC_DEFINE_JIT_OPERATION(operationObjectKeysObject, JSArray*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude, CachedPropertyNamesKind::Keys);
}

JSC_DEFINE_JIT_OPERATION(operationObjectGetOwnPropertyNames, JSArray*, (JSGlobalObject* globalObject, EncodedJSValue encodedObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(encodedObject).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    scope.release();
    return ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include, CachedPropertyNamesKind::GetOwnPropertyNames);
}

JSC_DEFINE_JIT_OPERATION(operationObjectGetOwnPropertyNamesObject, JSArray*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include, CachedPropertyNamesKind::GetOwnPropertyNames);
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
        return nullptr;
    }

    if (prototype.isObject())
        RELEASE_AND_RETURN(scope, constructEmptyObject(globalObject, asObject(prototype)));
    RELEASE_AND_RETURN(scope, constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationObjectCreateObject, JSCell*, (JSGlobalObject* globalObject, JSObject* prototype))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return constructEmptyObject(globalObject, prototype);
}

JSC_DEFINE_JIT_OPERATION(operationObjectAssignObject, void, (JSGlobalObject* globalObject, JSObject* target, JSObject* source))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool targetCanPerformFastPut = jsDynamicCast<JSFinalObject*>(target) && target->canPerformFastPutInlineExcludingProto() && target->isStructureExtensible();

    if (targetCanPerformFastPut) {
        Vector<RefPtr<UniquedStringImpl>, 8> properties;
        MarkedArgumentBuffer values;
        if (!source->staticPropertiesReified()) {
            source->reifyAllStaticProperties(globalObject);
            RETURN_IF_EXCEPTION(scope, void());
        }

        // |source| Structure does not have any getters. And target can perform fast put.
        // So enumerating properties and putting properties are non observable.

        // FIXME: It doesn't seem like we should have to do this in two phases, but
        // we're running into crashes where it appears that source is transitioning
        // under us, and even ends up in a state where it has a null butterfly. My
        // leading hypothesis here is that we fire some value replacement watchpoint
        // that ends up transitioning the structure underneath us.
        // https://bugs.webkit.org/show_bug.cgi?id=187837

        // FIXME: This fast path is very similar to ObjectConstructor' one. But extracting it to a function caused performance
        // regression in object-assign-replace. Since the code is small and fast path, we keep both.

        // Do not clear since Vector::clear shrinks the backing store.
        properties.resize(0);
        values.clear();
        bool canUseFastPath = source->fastForEachPropertyWithSideEffectFreeFunctor(vm, [&](const PropertyTableEntry& entry) -> bool {
            if (entry.attributes() & PropertyAttribute::DontEnum)
                return true;

            PropertyName propertyName(entry.key());
            if (propertyName.isPrivateName())
                return true;

            properties.append(entry.key());
            values.appendWithCrashOnOverflow(source->getDirect(entry.offset()));

            return true;
        });

        if (canUseFastPath) {
            for (size_t i = 0; i < properties.size(); ++i) {
                // FIXME: We could put properties in a batching manner to accelerate Object.assign more.
                // https://bugs.webkit.org/show_bug.cgi?id=185358
                PutPropertySlot putPropertySlot(target, true);
                target->putOwnDataProperty(vm, properties[i].get(), values.at(i), putPropertySlot);
            }
            return;
        }
    }

    scope.release();
    objectAssignGeneric(globalObject, vm, target, source);
}

JSC_DEFINE_JIT_OPERATION(operationObjectAssignUntyped, void, (JSGlobalObject* globalObject, JSObject* target, EncodedJSValue encodedSource))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool targetCanPerformFastPut = jsDynamicCast<JSFinalObject*>(target) && target->canPerformFastPutInlineExcludingProto() && target->isStructureExtensible();

    JSValue sourceValue = JSValue::decode(encodedSource);
    if (sourceValue.isUndefinedOrNull())
        return;
    JSObject* source = sourceValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    if (targetCanPerformFastPut) {
        if (!source->staticPropertiesReified()) {
            source->reifyAllStaticProperties(globalObject);
            RETURN_IF_EXCEPTION(scope, void());
        }

        Vector<RefPtr<UniquedStringImpl>, 8> properties;
        MarkedArgumentBuffer values;
        if (objectAssignFast(vm, target, source, properties, values))
            return;
    }

    scope.release();
    objectAssignGeneric(globalObject, vm, target, source);
}

JSC_DEFINE_JIT_OPERATION(operationObjectToStringUntyped, JSString*, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return objectPrototypeToString(globalObject, JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationObjectToStringObjectSlow, JSString*, (JSGlobalObject* globalObject, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return objectPrototypeToStringSlow(globalObject, object);
}

JSC_DEFINE_JIT_OPERATION(operationCreateThis, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor, uint32_t inlineCapacity))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (constructor->type() == JSFunctionType && jsCast<JSFunction*>(constructor)->canUseAllocationProfile()) {
        DeferTermination deferScope(vm);
        auto rareData = jsCast<JSFunction*>(constructor)->ensureRareDataAndAllocationProfile(globalObject, inlineCapacity);
        scope.releaseAssertNoException();
        ObjectAllocationProfileWithPrototype* allocationProfile = rareData->objectAllocationProfile();
        Structure* structure = allocationProfile->structure();
        JSObject* result = constructEmptyObject(vm, structure);
        if (structure->hasPolyProto()) {
            JSObject* prototype = allocationProfile->prototype();
            ASSERT(prototype == jsCast<JSFunction*>(constructor)->prototypeForConstruction(vm, globalObject));
            result->putDirectOffset(vm, knownPolyProtoOffset, prototype);
            prototype->didBecomePrototype();
            ASSERT_WITH_MESSAGE(!hasIndexedProperties(result->indexingType()), "We rely on JSFinalObject not starting out with an indexing type otherwise we would potentially need to convert to slow put storage");
        }
        return result;
    }

    JSValue proto = constructor->get(globalObject, vm.propertyNames->prototype);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (proto.isObject())
        return constructEmptyObject(globalObject, asObject(proto));
    JSGlobalObject* functionGlobalObject = getFunctionRealm(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return constructEmptyObject(functionGlobalObject);
}

JSC_DEFINE_JIT_OPERATION(operationCreatePromise, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, promiseStructure, constructor, globalObject->promiseConstructor());
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, JSPromise::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCreateInternalPromise, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, internalPromiseStructure, constructor, globalObject->internalPromiseConstructor());
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, JSInternalPromise::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCreateGenerator, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = InternalFunction::createSubclassStructure(globalObject, constructor, globalObject->generatorStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, JSGenerator::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCreateAsyncGenerator, JSCell*, (JSGlobalObject* globalObject, JSObject* constructor))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = InternalFunction::createSubclassStructure(globalObject, constructor, globalObject->asyncGeneratorStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, JSAsyncGenerator::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationCallObjectConstructor, JSCell*, (JSGlobalObject* globalObject, EncodedJSValue encodedTarget))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue value = JSValue::decode(encodedTarget);
    ASSERT(!value.isObject());

    if (value.isUndefinedOrNull())
        return constructEmptyObject(globalObject, globalObject->objectPrototype());
    return value.toObject(globalObject);
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
            return nullptr;
        }
    }

    RELEASE_AND_RETURN(scope, value.toObject(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationValueMod, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsRemainder(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationInc, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsInc(globalObject, JSValue::decode(encodedOp)));
}

JSC_DEFINE_JIT_OPERATION(operationDec, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsDec(globalObject, JSValue::decode(encodedOp)));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitNot, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsBitwiseNot(globalObject, JSValue::decode(encodedOp)));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitAnd, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsBitwiseAnd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitOr, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsBitwiseOr(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitXor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsBitwiseXor(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitLShift, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsLShift(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitRShift, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsRShift(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueBitURShift, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsURShift(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddNotNumber, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    return JSValue::encode(jsAddNonNumber(globalObject, op1, op2));
}

JSC_DEFINE_JIT_OPERATION(operationValueDiv, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsDiv(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValuePow, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsPow(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationArithAbs, double, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, PNaN);
    return fabs(a);
}

JSC_DEFINE_JIT_OPERATION(operationArithClz32, UCPUStrictInt32, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    uint32_t value = op1.toUInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return toUCPUStrictInt32(clz(value));
}

JSC_DEFINE_JIT_OPERATION(operationArithFRound, double, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, PNaN);
    return static_cast<float>(a);
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
    RETURN_IF_EXCEPTION(scope, PNaN); \
    return JSC::Math::lowerName(result); \
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
    RETURN_IF_EXCEPTION(scope, PNaN);
    return sqrt(a);
}

JSC_DEFINE_JIT_OPERATION(operationArithRound, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(Math::roundDouble(valueOfArgument)));
}

JSC_DEFINE_JIT_OPERATION(operationArithFloor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(floor(valueOfArgument)));
}

JSC_DEFINE_JIT_OPERATION(operationArithCeil, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(ceil(valueOfArgument)));
}

JSC_DEFINE_JIT_OPERATION(operationArithTrunc, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double truncatedValueOfArgument = argument.toIntegerPreserveNaN(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(truncatedValueOfArgument));
}

JSC_DEFINE_JIT_OPERATION(operationArithMinMultipleDouble, double, (const double* buffer, unsigned elementCount))
{
    double result = +std::numeric_limits<double>::infinity();
    for (unsigned index = 0; index < elementCount; ++index) {
        double val = buffer[index];
        if (std::isnan(val))
            return PNaN;
        if (val < result || (!val && !result && std::signbit(val)))
            result = val;
    }
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationArithMaxMultipleDouble, double, (const double* buffer, unsigned elementCount))
{
    double result = -std::numeric_limits<double>::infinity();
    for (unsigned index = 0; index < elementCount; ++index) {
        double val = buffer[index];
        if (std::isnan(val))
            return PNaN;
        if (val > result || (!val && !result && !std::signbit(val)))
            result = val;
    }
    return result;
}

ALWAYS_INLINE EncodedJSValue getByValCellInt(JSGlobalObject* globalObject, VM& vm, JSCell* base, int32_t index)
{
    if (index < 0) {
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

    return getByValCellInt(globalObject, vm, base, index);
}

JSC_DEFINE_JIT_OPERATION(operationGetByValStringInt, EncodedJSValue, (JSGlobalObject* globalObject, JSString* base, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return getByValCellInt(globalObject, vm, base, index);
}

JSC_DEFINE_JIT_OPERATION(operationGetByValObjectString, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* base, JSCell* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asString(string)->toIdentifier(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(getByValObject(globalObject, vm, asObject(base), propertyName)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValObjectSymbol, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* base, JSCell* symbol))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto propertyName = asSymbol(symbol)->privateName();
    return JSValue::encode(getByValObject(globalObject, vm, asObject(base), propertyName));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    putByValInternal<true, false>(globalObject, vm, encodedBase, encodedProperty, encodedValue);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValNonStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    putByValInternal<false, false>(globalObject, vm, encodedBase, encodedProperty, encodedValue);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellStringStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    putByValCellStringInternal<true, false>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellStringNonStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    putByValCellStringInternal<false, false>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellSymbolStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<true, false>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValCellSymbolNonStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<false, false>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue), true);
        return;
    }
    
    PutPropertySlot slot(object, true);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValBeyondArrayBoundsNonStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue), false);
        return;
    }
    
    PutPropertySlot slot(object, false);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), jsValue, true);
        return;
    }
    
    PutPropertySlot slot(object, true);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), jsValue, slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValBeyondArrayBoundsNonStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        object->putByIndexInline(globalObject, static_cast<uint32_t>(index), jsValue, false);
        return;
    }
    
    PutPropertySlot slot(object, false);
    object->methodTable()->put(
        object, globalObject, Identifier::from(vm, index), jsValue, slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValDirectBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);

    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), jsValue, 0, PutDirectIndexShouldThrow);
        return;
    }

    PutPropertySlot slot(object, true);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), jsValue, slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutDoubleByValDirectBeyondArrayBoundsNonStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, double value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);

    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), jsValue);
        return;
    }

    PutPropertySlot slot(object, false);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), jsValue, slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    putByValInternal<true, true>(globalObject, vm, encodedBase, encodedProperty, encodedValue);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectNonStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    putByValInternal<false, true>(globalObject, vm, encodedBase, encodedProperty, encodedValue);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellStringStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    putByValCellStringInternal<true, true>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellStringNonStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* string, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    putByValCellStringInternal<false, true>(globalObject, vm, cell, asString(string), JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellSymbolStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<true, true>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectCellSymbolNonStrict, void, (JSGlobalObject* globalObject, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto propertyName = asSymbol(symbol)->privateName();
    putByValCellInternal<false, true>(globalObject, vm, cell, propertyName, JSValue::decode(encodedValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectBeyondArrayBoundsStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue), 0, PutDirectIndexShouldThrow);
        return;
    }
    
    PutPropertySlot slot(object, true);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDirectBeyondArrayBoundsNonStrict, void, (JSGlobalObject* globalObject, JSObject* object, int32_t index, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    if (index >= 0) {
        object->putDirectIndex(globalObject, static_cast<uint32_t>(index), JSValue::decode(encodedValue));
        return;
    }
    
    PutPropertySlot slot(object, false);
    CommonSlowPaths::putDirectWithReify(vm, globalObject, object, Identifier::from(vm, index), JSValue::decode(encodedValue), slot);
}

JSC_DEFINE_JIT_OPERATION(operationArrayPush, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    array->pushInline(globalObject, JSValue::decode(encodedValue));
    return JSValue::encode(jsNumber(array->length()));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPushDouble, EncodedJSValue, (JSGlobalObject* globalObject, double value, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    array->pushInline(globalObject, JSValue(JSValue::EncodeAsDouble, value));
    return JSValue::encode(jsNumber(array->length()));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPushMultiple, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array, void* buffer, int32_t elementCount))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(buffer), elementCount);
    auto scope = DECLARE_THROW_SCOPE(vm);


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
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(jsNumber(array->length()));
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
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(jsNumber(array->length()));
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
        return { };
    }

    for (int32_t i = 0; i < elementCount; ++i) {
        array->pushInline(globalObject, arguments.at(i));
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(jsNumber(array->length()));
}

JSC_DEFINE_JIT_OPERATION(operationArrayPop, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return JSValue::encode(array->pop(globalObject));
}
        
JSC_DEFINE_JIT_OPERATION(operationArrayPopAndRecoverLength, EncodedJSValue, (JSGlobalObject* globalObject, JSArray* array))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    array->butterfly()->setPublicLength(array->butterfly()->publicLength() + 1);
    
    return JSValue::encode(array->pop(globalObject));
}
        
JSC_DEFINE_JIT_OPERATION(operationRegExpExecString, EncodedJSValue, (JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* argument))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return JSValue::encode(regExpObject->execInline(globalObject, argument));
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
        return encodedJSValue();
    RELEASE_AND_RETURN(scope, JSValue::encode(regExpObject->execInline(globalObject, input)));
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
        return throwVMTypeError(globalObject, scope);

    JSString* input = argument.toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        return JSValue::encode(jsUndefined());
    RELEASE_AND_RETURN(scope, JSValue::encode(regexp->exec(globalObject, input)));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpExecNonGlobalOrSticky, EncodedJSValue, (JSGlobalObject* globalObject, RegExp* regExp, JSString* string))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    String input = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned lastIndex = 0;
    MatchResult result;
    JSArray* array = createRegExpMatchesArray(vm, globalObject, string, input, regExp, lastIndex, result);
    RETURN_IF_EXCEPTION(scope, { });
    if (!array)
        return JSValue::encode(jsNull());

    globalObject->regExpGlobalData().recordMatch(vm, globalObject, regExp, string, result);
    return JSValue::encode(array);
}

JSC_DEFINE_JIT_OPERATION(operationRegExpMatchFastString, EncodedJSValue, (JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* argument))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    if (!regExpObject->regExp()->global())
        return JSValue::encode(regExpObject->execInline(globalObject, argument));
    return JSValue::encode(regExpObject->matchGlobal(globalObject, argument));
}

JSC_DEFINE_JIT_OPERATION(operationRegExpMatchFastGlobalString, EncodedJSValue, (JSGlobalObject* globalObject, RegExp* regExp, JSString* string))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(regExp->global());

    String s = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (regExp->unicode()) {
        unsigned stringLength = s.length();
        RELEASE_AND_RETURN(scope, JSValue::encode(collectMatches(
            vm, globalObject, string, s, regExp,
            [&] (size_t end) -> size_t {
                return advanceStringUnicode(s, stringLength, end);
            })));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(collectMatches(
        vm, globalObject, string, s, regExp,
        [&] (size_t end) -> size_t {
            return end + 1;
        })));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntNoRadixGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return toStringView(globalObject, JSValue::decode(value), [&] (StringView view) {
        // This version is as if radix was undefined. Hence, undefined.toNumber() === 0.
        return parseIntResult(parseInt(view, 0));
    });
}

JSC_DEFINE_JIT_OPERATION(operationParseIntStringNoRadix, EncodedJSValue, (JSGlobalObject* globalObject, JSString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto viewWithString = string->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // This version is as if radix was undefined. Hence, undefined.toNumber() === 0.
    return parseIntResult(parseInt(viewWithString.view, 0));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntString, EncodedJSValue, (JSGlobalObject* globalObject, JSString* string, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto viewWithString = string->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    return parseIntResult(parseInt(viewWithString.view, radix));
}

JSC_DEFINE_JIT_OPERATION(operationParseIntGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return toStringView(globalObject, JSValue::decode(value), [&] (StringView view) {
        return parseIntResult(parseInt(view, radix));
    });
}
        
JSC_DEFINE_JIT_OPERATION(operationRegExpTestString, size_t, (JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* input))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return regExpObject->testInline(globalObject, input);
}

JSC_DEFINE_JIT_OPERATION(operationRegExpTest, size_t, (JSGlobalObject* globalObject, RegExpObject* regExpObject, EncodedJSValue encodedArgument))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue argument = JSValue::decode(encodedArgument);

    JSString* input = argument.toStringOrNull(globalObject);
    if (!input)
        return false;
    return regExpObject->testInline(globalObject, input);
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
        return false;
    }

    JSString* input = argument.toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        return false;
    RELEASE_AND_RETURN(scope, regexp->test(globalObject, input));
}

JSC_DEFINE_JIT_OPERATION(operationSubHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSValue::encode(JSBigInt::sub(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationBitNotHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSBigInt* operand = jsCast<JSBigInt*>(op1);

    return JSValue::encode(JSBigInt::bitwiseNot(globalObject, operand));
}

JSC_DEFINE_JIT_OPERATION(operationMulHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    return JSValue::encode(JSBigInt::multiply(globalObject, leftOperand, rightOperand));
}
    
JSC_DEFINE_JIT_OPERATION(operationModHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSValue::encode(JSBigInt::remainder(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationDivHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSValue::encode(JSBigInt::divide(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationPowHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSValue::encode(JSBigInt::exponentiate(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationBitAndHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    return JSValue::encode(JSBigInt::bitwiseAnd(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationBitLShiftHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    return JSValue::encode(JSBigInt::leftShift(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationAddHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSValue::encode(JSBigInt::add(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationBitRShiftHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSValue::encode(JSBigInt::signedRightShift(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationBitOrHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSValue::encode(JSBigInt::bitwiseOr(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationBitXorHeapBigInt, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    return JSValue::encode(JSBigInt::bitwiseXor(globalObject, leftOperand, rightOperand));
}

JSC_DEFINE_JIT_OPERATION(operationCompareStrictEqCell, size_t, (JSGlobalObject* globalObject, JSCell* op1, JSCell* op2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return JSValue::strictEqualForCells(globalObject, op1, op2);
}

JSC_DEFINE_JIT_OPERATION(operationSameValue, size_t, (JSGlobalObject* globalObject, EncodedJSValue arg1, EncodedJSValue arg2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return sameValue(globalObject, JSValue::decode(arg1), JSValue::decode(arg2));
}

JSC_DEFINE_JIT_OPERATION(operationToPrimitive, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return JSValue::encode(JSValue::decode(value).toPrimitive(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationToPropertyKey, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(JSValue::decode(value).toPropertyKeyValue(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationToNumber, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsNumber(JSValue::decode(value).toNumber(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationToNumeric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(JSValue::decode(value).toNumeric(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationCallNumberConstructor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedValue);
    JSValue numeric = value.toNumeric(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (numeric.isNumber())
        return JSValue::encode(numeric);
    ASSERT(numeric.isBigInt());
    return JSValue::encode(JSBigInt::toNumber(numeric));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdWithThisStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    putWithThis<true>(globalObject, encodedBase, encodedThis, encodedValue, ident);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdWithThis, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    putWithThis<false>(globalObject, encodedBase, encodedThis, encodedValue, ident);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValWithThisStrict, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier property = JSValue::decode(encodedSubscript).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    putWithThis<true>(globalObject, encodedBase, encodedThis, encodedValue, property);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValWithThis, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier property = JSValue::decode(encodedSubscript).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    putWithThis<false>(globalObject, encodedBase, encodedThis, encodedValue, property);
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
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    defineDataProperty(globalObject, base, propertyName, JSValue::decode(encodedValue), attributes);
}

JSC_DEFINE_JIT_OPERATION(operationDefineDataPropertyString, void, (JSGlobalObject* globalObject, JSObject* base, JSString* property, EncodedJSValue encodedValue, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = property->toIdentifier(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    defineDataProperty(globalObject, base, propertyName, JSValue::decode(encodedValue), attributes);
}

JSC_DEFINE_JIT_OPERATION(operationDefineDataPropertyStringIdent, void, (JSGlobalObject* globalObject, JSObject* base, UniquedStringImpl* property, EncodedJSValue encodedValue, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    defineDataProperty(globalObject, base, Identifier::fromUid(vm, property), JSValue::decode(encodedValue), attributes);
}

JSC_DEFINE_JIT_OPERATION(operationDefineDataPropertySymbol, void, (JSGlobalObject* globalObject, JSObject* base, Symbol* property, EncodedJSValue encodedValue, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    defineDataProperty(globalObject, base, Identifier::fromUid(property->privateName()), JSValue::decode(encodedValue), attributes);
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
    RETURN_IF_EXCEPTION(scope, void());
    defineAccessorProperty(globalObject, base, propertyName, getter, setter, attributes);
}

JSC_DEFINE_JIT_OPERATION(operationDefineAccessorPropertyString, void, (JSGlobalObject* globalObject, JSObject* base, JSString* property, JSObject* getter, JSObject* setter, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = property->toIdentifier(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    defineAccessorProperty(globalObject, base, propertyName, getter, setter, attributes);
}

JSC_DEFINE_JIT_OPERATION(operationDefineAccessorPropertyStringIdent, void, (JSGlobalObject* globalObject, JSObject* base, UniquedStringImpl* property, JSObject* getter, JSObject* setter, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    defineAccessorProperty(globalObject, base, Identifier::fromUid(vm, property), getter, setter, attributes);
}

JSC_DEFINE_JIT_OPERATION(operationDefineAccessorPropertySymbol, void, (JSGlobalObject* globalObject, JSObject* base, Symbol* property, JSObject* getter, JSObject* setter, int32_t attributes))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    defineAccessorProperty(globalObject, base, Identifier::fromUid(property->privateName()), getter, setter, attributes);
}

JSC_DEFINE_JIT_OPERATION(operationNewArray, char*, (JSGlobalObject* globalObject, Structure* arrayStructure, void* buffer, size_t size))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(buffer), size);

    return bitwise_cast<char*>(constructArray(globalObject, arrayStructure, static_cast<JSValue*>(buffer), size));
}

JSC_DEFINE_JIT_OPERATION(operationNewEmptyArray, char*, (VM* vmPointer, Structure* arrayStructure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return bitwise_cast<char*>(JSArray::create(vm, arrayStructure));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSize, char*, (JSGlobalObject* globalObject, Structure* arrayStructure, int32_t size, Butterfly* butterfly))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(size < 0)) {
        throwException(globalObject, scope, createRangeError(globalObject, "Array size is not a small enough positive integer."_s));
        return nullptr;
    }

    JSArray* result;
    if (butterfly)
        result = JSArray::createWithButterfly(vm, nullptr, arrayStructure, butterfly);
    else {
        result = JSArray::tryCreate(vm, arrayStructure, size);
        if (UNLIKELY(!result)) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }
    }
    return bitwise_cast<char*>(result);
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSizeAndHint, char*, (JSGlobalObject* globalObject, Structure* arrayStructure, int32_t size, int32_t vectorLengthHint, Butterfly* butterfly))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(size < 0)) {
        throwException(globalObject, scope, createRangeError(globalObject, "Array size is not a small enough positive integer."_s));
        return nullptr;
    }

    JSArray* result;
    if (butterfly)
        result = JSArray::createWithButterfly(vm, nullptr, arrayStructure, butterfly);
    else {
        result = JSArray::tryCreate(vm, arrayStructure, size, vectorLengthHint);
        if (UNLIKELY(!result)) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }
    }
    return bitwise_cast<char*>(result);
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayBuffer, JSCell*, (VM* vmPointer, Structure* arrayStructure, JSCell* immutableButterflyCell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ASSERT(!arrayStructure->outOfLineCapacity());
    auto* immutableButterfly = jsCast<JSImmutableButterfly*>(immutableButterflyCell);
    ASSERT(arrayStructure->indexingMode() == immutableButterfly->indexingMode() || hasAnyArrayStorage(arrayStructure->indexingMode()));
    auto* result = CommonSlowPaths::allocateNewArrayBuffer(vm, arrayStructure, immutableButterfly);
    ASSERT(result->indexingMode() == result->structure()->indexingMode());
    ASSERT(result->structure() == arrayStructure);
    return result;
}

#define JSC_TYPED_ARRAY_OPERATIONS(type) \
    JSC_DEFINE_JIT_OPERATION(operationNew##type##ArrayWithSize, char*, (JSGlobalObject* globalObject, Structure* structure, intptr_t length, char* vector)) \
    { \
        VM& vm = globalObject->vm(); \
        CallFrame* callFrame = DECLARE_CALL_FRAME(vm); \
        JITOperationPrologueCallFrameTracer tracer(vm, callFrame); \
        return newTypedArrayWithSize<JS##type##Array>(globalObject, vm, structure, length, vector); \
    } \
    \
    JSC_DEFINE_JIT_OPERATION(operationNew##type##ArrayWithOneArgument, char*, (JSGlobalObject* globalObject, EncodedJSValue encodedValue)) \
    { \
        VM& vm = globalObject->vm(); \
        CallFrame* callFrame = DECLARE_CALL_FRAME(vm); \
        JITOperationPrologueCallFrameTracer tracer(vm, callFrame); \
        JSValue firstValue = JSValue::decode(encodedValue); \
        bool isResizableOrGrowableShared = false; \
        if (auto* arrayBuffer = jsDynamicCast<JSArrayBuffer*>(firstValue)) \
            isResizableOrGrowableShared = arrayBuffer->isResizableOrGrowableShared(); \
        Structure* structure = globalObject->typedArrayStructure(Type##type, isResizableOrGrowableShared); \
        return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JS##type##Array>(globalObject, structure, firstValue, 0, std::nullopt)); \
    } \

    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_TYPED_ARRAY_OPERATIONS)
#undef JSC_TYPED_ARRAY_OPERATIONS

JSC_DEFINE_JIT_OPERATION(operationNewArrayIterator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSArrayIterator::createWithInitialValues(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationNewMapIterator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSMapIterator::createWithInitialValues(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationNewSetIterator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSSetIterator::createWithInitialValues(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationCreateActivationDirect, JSCell*, (VM* vmPointer, Structure* structure, JSScope* scope, SymbolTable* table, EncodedJSValue initialValueEncoded))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue initialValue = JSValue::decode(initialValueEncoded);
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    return JSLexicalEnvironment::create(vm, structure, scope, table, initialValue);
}

JSC_DEFINE_JIT_OPERATION(operationCreateDirectArguments, JSCell*, (VM* vmPointer, Structure* structure, uint32_t length, uint32_t minCapacity))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    DirectArguments* result = DirectArguments::create(
        vm, structure, length, std::max(length, minCapacity));
    // The caller will store to this object without barriers. Most likely, at this point, this is
    // still a young object and so no barriers are needed. But it's good to be careful anyway,
    // since the GC should be allowed to do crazy (like pretenuring, for example).
    vm.writeBarrier(result);
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationCreateScopedArguments, JSCell*, (JSGlobalObject* globalObject, Structure* structure, Register* argumentStart, uint32_t length, JSFunction* callee, JSLexicalEnvironment* scope))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    // We could pass the ScopedArgumentsTable* as an argument. We currently don't because I
    // didn't feel like changing the max number of arguments for a slow path call from 6 to 7.
    ScopedArgumentsTable* table = scope->symbolTable()->arguments();
    
    return ScopedArguments::createByCopyingFrom(
        vm, structure, argumentStart, length, callee, table, scope);
}

JSC_DEFINE_JIT_OPERATION(operationCreateClonedArguments, JSCell*, (JSGlobalObject* globalObject, Structure* structure, Register* argumentStart, uint32_t length, JSFunction* callee))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return ClonedArguments::createByCopyingFrom(
        globalObject, structure, argumentStart, length, callee);
}

JSC_DEFINE_JIT_OPERATION(operationCreateArgumentsButterfly, JSCell*, (JSGlobalObject* globalObject, Register* argumentStart, uint32_t argumentCount))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSImmutableButterfly* butterfly = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), argumentCount);
    if (!butterfly) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    for (unsigned index = 0; index < argumentCount; ++index)
        butterfly->setIndex(vm, index, argumentStart[index].jsValue());
    return butterfly;
}

JSC_DEFINE_JIT_OPERATION(operationCreateDirectArgumentsDuringExit, JSCell*, (VM* vmPointer, InlineCallFrame* inlineCallFrame, JSFunction* callee, uint32_t argumentCount))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
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
    
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationCreateClonedArgumentsDuringExit, JSCell*, (VM* vmPointer, InlineCallFrame* inlineCallFrame, JSFunction* callee, uint32_t argumentCount))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    DeferGCForAWhile deferGC(vm);
    
    CodeBlock* codeBlock;
    if (inlineCallFrame)
        codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    else
        codeBlock = callFrame->codeBlock();
    
    unsigned length = argumentCount - 1;
    JSGlobalObject* globalObject = codeBlock->globalObject();
    ClonedArguments* result = ClonedArguments::createEmpty(
        vm, globalObject->clonedArgumentsStructure(), callee, length);
    
    Register* arguments =
        callFrame->registers() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0) +
        CallFrame::argumentOffset(0);
    for (unsigned i = length; i--;)
        result->putDirectIndex(globalObject, i, arguments[i].jsValue());

    
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationCreateRest, JSCell*, (JSGlobalObject* globalObject, Register* argumentStart, unsigned numberOfParamsToSkip, unsigned arraySize))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    Structure* structure = globalObject->restParameterStructure();
    static_assert(sizeof(Register) == sizeof(JSValue), "This is a strong assumption here.");
    JSValue* argumentsToCopyRegion = bitwise_cast<JSValue*>(argumentStart) + numberOfParamsToSkip;
    return constructArray(globalObject, structure, argumentsToCopyRegion, arraySize);
}

JSC_DEFINE_JIT_OPERATION(operationTypeOfIsObject, size_t, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    return jsTypeofIsObject(globalObject, object);
}

JSC_DEFINE_JIT_OPERATION(operationTypeOfIsFunction, size_t, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(jsDynamicCast<JSObject*>(object));

    return jsTypeofIsFunction(globalObject, object);
}

JSC_DEFINE_JIT_OPERATION(operationObjectIsCallable, size_t, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    return object->isCallable();
}

JSC_DEFINE_JIT_OPERATION(operationIsConstructor, size_t, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::decode(value).isConstructor();
}

JSC_DEFINE_JIT_OPERATION(operationTypeOfObject, JSCell*, (JSGlobalObject* globalObject, JSCell* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    if (object->structure()->masqueradesAsUndefined(globalObject))
        return vm.smallStrings.undefinedString();
    if (object->isCallable())
        return vm.smallStrings.functionString();
    return vm.smallStrings.objectString();
}

JSC_DEFINE_JIT_OPERATION(operationAllocateSimplePropertyStorageWithInitialCapacity, char*, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return reinterpret_cast<char*>(
        Butterfly::createUninitialized(vm, nullptr, 0, initialOutOfLineCapacity, false, 0));
}

JSC_DEFINE_JIT_OPERATION(operationAllocateSimplePropertyStorage, char*, (VM* vmPointer, size_t newSize))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return reinterpret_cast<char*>(
        Butterfly::createUninitialized(vm, nullptr, 0, newSize, false, 0));
}

JSC_DEFINE_JIT_OPERATION(operationAllocateComplexPropertyStorageWithInitialCapacity, char*, (VM* vmPointer, JSObject* object))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(!object->structure()->outOfLineCapacity());
    return reinterpret_cast<char*>(
        object->allocateMoreOutOfLineStorage(vm, 0, initialOutOfLineCapacity));
}

JSC_DEFINE_JIT_OPERATION(operationAllocateComplexPropertyStorage, char*, (VM* vmPointer, JSObject* object, size_t newSize))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return reinterpret_cast<char*>(
        object->allocateMoreOutOfLineStorage(vm, object->structure()->outOfLineCapacity(), newSize));
}

JSC_DEFINE_JIT_OPERATION(operationEnsureInt32, char*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    if (!cell->isObject())
        return nullptr;

    auto* result = reinterpret_cast<char*>(asObject(cell)->tryMakeWritableInt32(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasInt32(cell->indexingMode())) || !result);
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationEnsureDouble, char*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    if (!cell->isObject())
        return nullptr;

    ASSERT(Options::allowDoubleShape());
    auto* result = reinterpret_cast<char*>(asObject(cell)->tryMakeWritableDouble(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasDouble(cell->indexingMode())) || !result);
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationEnsureContiguous, char*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    if (!cell->isObject())
        return nullptr;
    
    auto* result = reinterpret_cast<char*>(asObject(cell)->tryMakeWritableContiguous(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasContiguous(cell->indexingMode())) || !result);
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationEnsureArrayStorage, char*, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    if (!cell->isObject())
        return nullptr;

    auto* result = reinterpret_cast<char*>(asObject(cell)->ensureArrayStorage(vm));
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasAnyArrayStorage(cell->indexingMode())) || !result);
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationHasIndexedProperty, size_t, (JSGlobalObject* globalObject, JSCell* baseCell, int32_t subscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSObject* object = baseCell->toObject(globalObject);
    if (UNLIKELY(subscript < 0)) {
        // Go the slowest way possible because negative indices don't use indexed storage.
        return object->hasProperty(globalObject, Identifier::from(vm, subscript));
    }
    return object->hasProperty(globalObject, static_cast<unsigned>(subscript));
}

JSC_DEFINE_JIT_OPERATION(operationHasEnumerableIndexedProperty, size_t, (JSGlobalObject* globalObject, JSCell* baseCell, int32_t subscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSObject* object = baseCell->toObject(globalObject);
    if (UNLIKELY(subscript < 0)) {
        // Go the slowest way possible because negative indices don't use indexed storage.
        return object->hasEnumerableProperty(globalObject, Identifier::from(vm, subscript));
    }
    return object->hasEnumerableProperty(globalObject, subscript);
}

JSC_DEFINE_JIT_OPERATION(operationGetPropertyEnumerator, JSCell*, (JSGlobalObject* globalObject, EncodedJSValue encodedBase))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(encodedBase);
    if (base.isUndefinedOrNull())
        return vm.emptyPropertyNameEnumerator();

    JSObject* baseObject = base.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, propertyNameEnumerator(globalObject, baseObject));
}

JSC_DEFINE_JIT_OPERATION(operationGetPropertyEnumeratorCell, JSCell*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* base = cell->toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, propertyNameEnumerator(globalObject, base));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorNextUpdateIndexAndMode, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue baseValue, uint32_t index, int32_t modeNumber, JSPropertyNameEnumerator* enumerator))
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
        RETURN_IF_EXCEPTION(scope, { });

        constexpr bool shouldAllocateIndexedNameString = false;
        enumerator->computeNext(globalObject, base, index, mode, shouldAllocateIndexedNameString);
        RETURN_IF_EXCEPTION(scope, { });
    }

#if USE(JSVALUE64)
    JSValue result = bitwise_cast<JSValue>(static_cast<uint64_t>(mode) << 32 | index | JSValue::DoubleEncodeOffset);
#else
    JSValue result = JSValue(mode, index);
#endif
    ASSERT(result.isDouble());
    return JSValue::encode(result);
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorNextUpdatePropertyName, JSString*, (JSGlobalObject* globalObject, uint32_t index, int32_t modeNumber, JSPropertyNameEnumerator* enumerator))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    if (modeNumber == JSPropertyNameEnumerator::IndexedMode) {
        if (index < enumerator->indexedLength())
            return jsString(vm, Identifier::from(vm, index).string());
        return vm.smallStrings.sentinelString();
    }

    JSString* result = enumerator->propertyNameAtIndex(index);
    if (!result)
        return vm.smallStrings.sentinelString();

    return result;
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
    RETURN_IF_EXCEPTION(scope, { });
    JSValue base = JSValue::decode(baseValue);
    JSObject* object = base.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(object->get(globalObject, propertyName)));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorInByVal, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue baseValue, EncodedJSValue propertyNameValue, uint32_t index, int32_t modeNumber))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(baseValue);
    if (modeNumber == JSPropertyNameEnumerator::IndexedMode && base.isObject())
        RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(jsCast<JSObject*>(base)->hasProperty(globalObject, index))));

    JSString* propertyName = jsSecureCast<JSString*>(JSValue::decode(propertyNameValue));
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(CommonSlowPaths::opInByVal(globalObject, base, propertyName))));
}

JSC_DEFINE_JIT_OPERATION(operationEnumeratorHasOwnProperty, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue baseValue, EncodedJSValue propertyNameValue, uint32_t index, int32_t modeNumber))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(baseValue);
    if (modeNumber == JSPropertyNameEnumerator::IndexedMode && base.isObject())
        RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(jsCast<JSObject*>(base)->hasOwnProperty(globalObject, index))));

    JSString* propertyName = jsSecureCast<JSString*>(JSValue::decode(propertyNameValue));
    auto identifier = propertyName->toIdentifier(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    JSObject* baseObject = base.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(objectPrototypeHasOwnProperty(globalObject, baseObject, identifier))));
}

JSC_DEFINE_JIT_OPERATION(operationNewRegexpWithLastIndex, JSCell*, (JSGlobalObject* globalObject, JSCell* regexpPtr, EncodedJSValue encodedLastIndex))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    return RegExpObject::create(vm, globalObject->regExpStructure(), regexp, JSValue::decode(encodedLastIndex));
}

JSC_DEFINE_JIT_OPERATION(operationResolveRope, StringImpl*, (JSGlobalObject* globalObject, JSString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return string->value(globalObject).impl();
}

JSC_DEFINE_JIT_OPERATION(operationResolveRopeString, JSString*, (JSGlobalObject* globalObject, JSRopeString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    string->resolveRope(globalObject);
    return string;
}

JSC_DEFINE_JIT_OPERATION(operationStringValueOf, JSString*, (JSGlobalObject* globalObject, EncodedJSValue encodedArgument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);

    if (argument.isString())
        return asString(argument);

    if (auto* stringObject = jsDynamicCast<StringObject*>(argument))
        return stringObject->internalValue();

    throwVMTypeError(globalObject, scope);
    return nullptr;
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringString, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = stringCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String search = searchCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String replacement = replacementCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::Yes, StringReplaceUseTable::No, BoyerMooreHorspoolTable<uint8_t>>(globalObject, stringCell, WTFMove(string), WTFMove(search), WTFMove(replacement), nullptr)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringStringWithoutSubstitution, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = stringCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String search = searchCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String replacement = replacementCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::No, StringReplaceUseTable::No, BoyerMooreHorspoolTable<uint8_t>>(globalObject, stringCell, WTFMove(string), WTFMove(search), WTFMove(replacement), nullptr)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringEmptyString, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = stringCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String search = searchCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    size_t matchStart = string.find(search);
    if (matchStart == notFound)
        return stringCell;

    // Because replacement string is empty, it cannot include backreferences.
    size_t searchLength = search.length();
    size_t matchEnd = matchStart + searchLength;
    auto result = tryMakeString(StringView(string).substring(0, matchStart), StringView(string).substring(matchEnd, string.length() - matchEnd));
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    return jsString(vm, WTFMove(result));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringStringWithTable8, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell, const BoyerMooreHorspoolTable<uint8_t>* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = stringCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String search = searchCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String replacement = replacementCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::Yes, StringReplaceUseTable::Yes>(globalObject, stringCell, WTFMove(string), WTFMove(search), WTFMove(replacement), table)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringStringWithoutSubstitutionWithTable8, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, JSString* replacementCell, const BoyerMooreHorspoolTable<uint8_t>* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = stringCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String search = searchCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String replacement = replacementCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::No, StringReplaceUseTable::Yes>(globalObject, stringCell, WTFMove(string), WTFMove(search), WTFMove(replacement), table)));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringEmptyStringWithTable8, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, const BoyerMooreHorspoolTable<uint8_t>* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = stringCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String search = searchCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    size_t matchStart = table->find(string, search);
    if (matchStart == notFound)
        return stringCell;

    // Because replacement string is empty, it cannot include backreferences.
    size_t searchLength = search.length();
    size_t matchEnd = matchStart + searchLength;
    auto result = tryMakeString(StringView(string).substring(0, matchStart), StringView(string).substring(matchEnd, string.length() - matchEnd));
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    return jsString(vm, WTFMove(result));
}

JSC_DEFINE_JIT_OPERATION(operationStringReplaceStringGeneric, JSString*, (JSGlobalObject* globalObject, JSString* stringCell, JSString* searchCell, EncodedJSValue encodedReplaceValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = stringCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String search = searchCell->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSValue replaceValue = JSValue::decode(encodedReplaceValue);

    RELEASE_AND_RETURN(scope, replaceUsingStringSearch(vm, globalObject, stringCell, WTFMove(string), WTFMove(search), replaceValue, StringReplaceMode::Single));
}

JSC_DEFINE_JIT_OPERATION(operationStringSubstr, JSCell*, (JSGlobalObject* globalObject, JSCell* cell, int32_t from, int32_t span))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return jsSubstring(vm, globalObject, jsCast<JSString*>(cell), from, span);
}

JSC_DEFINE_JIT_OPERATION(operationStringSlice, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    static_assert(static_cast<uint64_t>(JSString::MaxLength) <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max()));
    return stringSlice<int32_t>(globalObject, vm, string, string->length(), start, std::nullopt);
}

JSC_DEFINE_JIT_OPERATION(operationStringSliceWithEnd, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start, int32_t end))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    static_assert(static_cast<uint64_t>(JSString::MaxLength) <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max()));
    return stringSlice<int32_t>(globalObject, vm, string, string->length(), start, end);
}

JSC_DEFINE_JIT_OPERATION(operationStringSubstring, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return stringSubstring(globalObject, string, start, std::nullopt);
}

JSC_DEFINE_JIT_OPERATION(operationStringSubstringWithEnd, JSString*, (JSGlobalObject* globalObject, JSString* string, int32_t start, int32_t end))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return stringSubstring(globalObject, string, start, end);
}

JSC_DEFINE_JIT_OPERATION(operationToLowerCase, JSString*, (JSGlobalObject* globalObject, JSString* string, uint32_t failingIndex))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    String inputString = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!inputString.length())
        return vm.smallStrings.emptyString();

    String lowercasedString = inputString.is8Bit() ? inputString.convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(failingIndex) : inputString.convertToLowercaseWithoutLocale();
    if (lowercasedString.impl() == inputString.impl())
        return string;
    RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(lowercasedString)));
}

JSC_DEFINE_JIT_OPERATION(operationStringLocaleCompare, UCPUStrictInt32, (JSGlobalObject* globalObject, JSString* base, JSString* argument))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = base->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    String that = argument->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto* collator = globalObject->defaultCollator();

    RELEASE_AND_RETURN(scope, toUCPUStrictInt32(collator->compareStrings(globalObject, string, that)));
}

JSC_DEFINE_JIT_OPERATION(operationInt32ToString, char*, (JSGlobalObject* globalObject, int32_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(globalObject, scope, createRangeError(globalObject, "toString() radix argument must be between 2 and 36"_s));
        return nullptr;
    }

    return reinterpret_cast<char*>(int32ToString(vm, value, radix));
}

JSC_DEFINE_JIT_OPERATION(operationInt52ToString, char*, (JSGlobalObject* globalObject, int64_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(globalObject, scope, createRangeError(globalObject, "toString() radix argument must be between 2 and 36"_s));
        return nullptr;
    }

    return reinterpret_cast<char*>(int52ToString(vm, value, radix));
}

JSC_DEFINE_JIT_OPERATION(operationDoubleToString, char*, (JSGlobalObject* globalObject, double value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(globalObject, scope, createRangeError(globalObject, "toString() radix argument must be between 2 and 36"_s));
        return nullptr;
    }

    return reinterpret_cast<char*>(numberToString(vm, value, radix));
}

JSC_DEFINE_JIT_OPERATION(operationInt32ToStringWithValidRadix, char*, (JSGlobalObject* globalObject, int32_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return reinterpret_cast<char*>(int32ToString(vm, value, radix));
}

JSC_DEFINE_JIT_OPERATION(operationInt52ToStringWithValidRadix, char*, (JSGlobalObject* globalObject, int64_t value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return reinterpret_cast<char*>(int52ToString(vm, value, radix));
}

JSC_DEFINE_JIT_OPERATION(operationDoubleToStringWithValidRadix, char*, (JSGlobalObject* globalObject, double value, int32_t radix))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return reinterpret_cast<char*>(numberToString(vm, value, radix));
}

JSC_DEFINE_JIT_OPERATION(operationFunctionToString, JSString*, (JSGlobalObject* globalObject, JSFunction* function))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return function->toString(globalObject);
}

JSC_DEFINE_JIT_OPERATION(operationSingleCharacterString, JSString*, (VM* vmPointer, int32_t character))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return jsSingleCharacterString(vm, static_cast<UChar>(character));
}

JSC_DEFINE_JIT_OPERATION(operationNewSymbol, Symbol*, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return Symbol::create(vm);
}

JSC_DEFINE_JIT_OPERATION(operationNewSymbolWithStringDescription, Symbol*, (JSGlobalObject* globalObject, JSString* description))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = description->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return Symbol::createWithDescription(vm, WTFMove(string));
}

JSC_DEFINE_JIT_OPERATION(operationNewSymbolWithDescription, Symbol*, (JSGlobalObject* globalObject, EncodedJSValue encodedDescription))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue description = JSValue::decode(encodedDescription);
    if (description.isUndefined())
        return Symbol::create(vm);

    String string = description.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return Symbol::createWithDescription(vm, string);
}

JSC_DEFINE_JIT_OPERATION(operationNewStringObject, JSCell*, (VM* vmPointer, JSString* string, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return StringObject::create(vm, structure, string);
}

JSC_DEFINE_JIT_OPERATION(operationToStringOnCell, JSString*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return JSValue(cell).toString(globalObject);
}

JSC_DEFINE_JIT_OPERATION(operationToString, JSString*, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::decode(value).toString(globalObject);
}

JSC_DEFINE_JIT_OPERATION(operationCallStringConstructorOnCell, JSString*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return stringConstructor(globalObject, cell);
}

JSC_DEFINE_JIT_OPERATION(operationCallStringConstructor, JSString*, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return stringConstructor(globalObject, JSValue::decode(value));
}

JSC_DEFINE_JIT_OPERATION(operationMakeRope2, JSString*, (JSGlobalObject* globalObject, JSString* left, JSString* right))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return jsString(globalObject, left, right);
}

JSC_DEFINE_JIT_OPERATION(operationMakeRope3, JSString*, (JSGlobalObject* globalObject, JSString* a, JSString* b, JSString* c))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return jsString(globalObject, a, b, c);
}

JSC_DEFINE_JIT_OPERATION(operationStrCat2, JSString*, (JSGlobalObject* globalObject, EncodedJSValue a, EncodedJSValue b))
{
    VM& vm = globalObject->vm();
    DeferTermination deferScope(vm);
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!JSValue::decode(a).isSymbol());
    ASSERT(!JSValue::decode(b).isSymbol());
    JSString* str1 = JSValue::decode(a).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr); // OOM exception can happen from ToString(BigInt).
    JSString* str2 = JSValue::decode(b).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, jsString(globalObject, str1, str2));
}
    
JSC_DEFINE_JIT_OPERATION(operationStrCat3, JSString*, (JSGlobalObject* globalObject, EncodedJSValue a, EncodedJSValue b, EncodedJSValue c))
{
    VM& vm = globalObject->vm();
    DeferTermination deferScope(vm);
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!JSValue::decode(a).isSymbol());
    ASSERT(!JSValue::decode(b).isSymbol());
    ASSERT(!JSValue::decode(c).isSymbol());
    JSString* str1 = JSValue::decode(a).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr); // OOM exception can happen from ToString(BigInt).
    JSString* str2 = JSValue::decode(b).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSString* str3 = JSValue::decode(c).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, jsString(globalObject, str1, str2, str3));
}

JSC_DEFINE_JIT_OPERATION(operationFindSwitchImmTargetForDouble, char*, (VM* vmPointer, EncodedJSValue encodedValue, size_t tableIndex, int32_t min))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CodeBlock* codeBlock = callFrame->codeBlock();
    const SimpleJumpTable& linkedTable = codeBlock->dfgSwitchJumpTable(tableIndex);
    JSValue value = JSValue::decode(encodedValue);
    ASSERT(value.isDouble());
    double asDouble = value.asDouble();
    int32_t asInt32 = static_cast<int32_t>(asDouble);
    if (asDouble == asInt32)
        return linkedTable.ctiForValue(min, asInt32).taggedPtr<char*>();
    return linkedTable.m_ctiDefault.taggedPtr<char*>();
}

JSC_DEFINE_JIT_OPERATION(operationSwitchString, char*, (JSGlobalObject* globalObject, size_t tableIndex, const UnlinkedStringJumpTable* unlinkedTable, JSString* string))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    StringImpl* strImpl = string->value(globalObject).impl();

    RETURN_IF_EXCEPTION(throwScope, nullptr);
    CodeBlock* codeBlock = callFrame->codeBlock();
    const StringJumpTable& linkedTable = codeBlock->dfgStringSwitchJumpTable(tableIndex);
    return linkedTable.ctiForValue(*unlinkedTable, strImpl).taggedPtr<char*>();
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringImplLess, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) < 0;
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringImplLessEq, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) <= 0;
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringImplGreater, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) > 0;
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringImplGreaterEq, uintptr_t, (StringImpl* a, StringImpl* b))
{
    return codePointCompare(a, b) >= 0;
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringLess, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return codePointCompareLessThan(asString(a)->value(globalObject), asString(b)->value(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringLessEq, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return !codePointCompareLessThan(asString(b)->value(globalObject), asString(a)->value(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringGreater, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return codePointCompareLessThan(asString(b)->value(globalObject), asString(a)->value(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationCompareStringGreaterEq, uintptr_t, (JSGlobalObject* globalObject, JSString* a, JSString* b))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return !codePointCompareLessThan(asString(a)->value(globalObject), asString(b)->value(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationNotifyWrite, void, (VM* vmPointer, WatchpointSet* set))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    set->touch(vm, "Executed NotifyWrite");
}

JSC_DEFINE_JIT_OPERATION(operationThrowStackOverflowForVarargs, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwStackOverflowError(globalObject, scope);
}

JSC_DEFINE_JIT_OPERATION(operationSizeOfVarargs, UCPUStrictInt32, (JSGlobalObject* globalObject, EncodedJSValue encodedArguments, uint32_t firstVarArgOffset))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    return toUCPUStrictInt32(sizeOfVarargs(globalObject, arguments, firstVarArgOffset));
}

JSC_DEFINE_JIT_OPERATION(operationHasOwnProperty, size_t, (JSGlobalObject* globalObject, JSObject* thisObject, EncodedJSValue encodedKey))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue key = JSValue::decode(encodedKey);
    Identifier propertyName = key.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);
    bool result = thisObject->hasOwnProperty(globalObject, propertyName.impl(), slot);
    RETURN_IF_EXCEPTION(scope, false);

    HasOwnPropertyCache* hasOwnPropertyCache = vm.hasOwnPropertyCache();
    ASSERT(hasOwnPropertyCache);
    hasOwnPropertyCache->tryAdd(slot, thisObject, propertyName.impl(), result);
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationNumberIsInteger, size_t, (JSGlobalObject* globalObject, EncodedJSValue value))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return NumberConstructor::isIntegerImpl(JSValue::decode(value));
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

    return arrayIndexOfString(globalObject, butterfly, searchElement, index);
}

JSC_DEFINE_JIT_OPERATION(operationArrayIndexOfValueInt32OrContiguous, UCPUStrictInt32, (JSGlobalObject* globalObject, Butterfly* butterfly, EncodedJSValue encodedValue, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue searchElement = JSValue::decode(encodedValue);

    if (searchElement.isString())
        RELEASE_AND_RETURN(scope, arrayIndexOfString(globalObject, butterfly, asString(searchElement), index));

    int32_t length = butterfly->publicLength();
    auto data = butterfly->contiguous().data();

    if (index >= length)
        return toUCPUStrictInt32(-1);

    if (searchElement.isObject()) {
        auto* result = bitwise_cast<const WriteBarrier<Unknown>*>(WTF::find64(bitwise_cast<const uint64_t*>(data + index), encodedValue, length - index));
        if (result)
            return toUCPUStrictInt32(result - data);
        return toUCPUStrictInt32(-1);
    }

    for (; index < length; ++index) {
        JSValue value = data[index].get();
        if (!value)
            continue;
        bool isEqual = JSValue::strictEqual(globalObject, searchElement, value);
        RETURN_IF_EXCEPTION(scope, { });
        if (isEqual)
            return toUCPUStrictInt32(index);
    }
    return toUCPUStrictInt32(-1);
}

JSC_DEFINE_JIT_OPERATION(operationArrayIndexOfValueDouble, UCPUStrictInt32, (JSGlobalObject* globalObject, Butterfly* butterfly, EncodedJSValue encodedValue, int32_t index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

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

JSC_DEFINE_JIT_OPERATION(operationLoadVarargs, void, (JSGlobalObject* globalObject, int32_t firstElementDest, EncodedJSValue encodedArguments, uint32_t offset, uint32_t lengthIncludingThis, uint32_t mandatoryMinimum))
{
    VirtualRegister firstElement { firstElementDest };
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    loadVarargs(globalObject, bitwise_cast<JSValue*>(&callFrame->r(firstElement)), arguments, offset, lengthIncludingThis - 1);
    
    for (uint32_t i = lengthIncludingThis - 1; i < mandatoryMinimum; ++i)
        callFrame->r(firstElement + i) = jsUndefined();
}

JSC_DEFINE_JIT_OPERATION(operationFModOnInts, double, (int32_t a, int32_t b))
{
    return fmod(a, b);
}

#if USE(JSVALUE32_64)
JSC_DEFINE_JIT_OPERATION(operationRandom, double, (JSGlobalObject* globalObject))
{
    return globalObject->weakRandomNumber();
}
#endif

JSC_DEFINE_JIT_OPERATION(operationStringFromCharCode, JSCell*, (JSGlobalObject* globalObject, int32_t op1))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSC::stringFromCharCode(globalObject, op1);
}

JSC_DEFINE_JIT_OPERATION(operationStringFromCharCodeUntyped, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue charValue = JSValue::decode(encodedValue);
    int32_t chInt = charValue.toUInt32(globalObject);
    return JSValue::encode(JSC::stringFromCharCode(globalObject, chInt));
}

JSC_DEFINE_JIT_OPERATION(operationConvertBoxedDoubleToInt52, int64_t, (EncodedJSValue encodedValue))
{
    JSValue value = JSValue::decode(encodedValue);
    if (!value.isDouble())
        return JSValue::notInt52;
    return tryConvertToInt52(value.asDouble());
}

JSC_DEFINE_JIT_OPERATION(operationConvertDoubleToInt52, int64_t, (double value))
{
    return tryConvertToInt52(value);
}

JSC_DEFINE_JIT_OPERATION(operationNewRawObject, char*, (VM* vmPointer, Structure* structure, int32_t length, Butterfly* butterfly))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

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
        return bitwise_cast<char*>(JSArray::createWithButterfly(vm, nullptr, structure, butterfly));
    return bitwise_cast<char*>(JSFinalObject::createWithButterfly(vm, structure, butterfly));
}

JSC_DEFINE_JIT_OPERATION(operationNewObjectWithButterfly, JSCell*, (VM* vmPointer, Structure* structure, Butterfly* butterfly))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    if (!butterfly) {
        butterfly = Butterfly::create(
            vm, nullptr, 0, structure->outOfLineCapacity(), false, IndexingHeader(), 0);
    }
    
    if (structure->typeInfo().type() == JSType::ArrayType)
        return JSArray::createWithButterfly(vm, nullptr, structure, butterfly);
    return JSFinalObject::createWithButterfly(vm, structure, butterfly);
}

JSC_DEFINE_JIT_OPERATION(operationNewObjectWithButterflyWithIndexingHeaderAndVectorLength, JSCell*, (VM* vmPointer, Structure* structure, unsigned length, Butterfly* butterfly))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

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
        return JSArray::createWithButterfly(vm, nullptr, structure, butterfly);
    return JSFinalObject::createWithButterfly(vm, structure, butterfly);
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
        return nullptr;
    }

    unsigned length = checkedLength;
    if (UNLIKELY(length >= MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);

    JSArray* result = JSArray::tryCreate(vm, structure, length);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned index = 0;
    for (unsigned i = 0; i < numItems; i++) {
        JSValue value = JSValue::decode(values[i]);
        if (JSImmutableButterfly* array = jsDynamicCast<JSImmutableButterfly*>(value)) {
            // We are spreading.
            for (unsigned i = 0; i < array->publicLength(); i++) {
                result->putDirectIndex(globalObject, index, array->get(i));
                RETURN_IF_EXCEPTION(scope, nullptr);
                ++index;
            }
        } else {
            // We are not spreading.
            result->putDirectIndex(globalObject, index, value);
            RETURN_IF_EXCEPTION(scope, nullptr);
            ++index;
        }
    }

    return result;
}

JSC_DEFINE_JIT_OPERATION(operationCreateImmutableButterfly, JSCell*, (JSGlobalObject* globalObject, unsigned length))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (JSImmutableButterfly* result = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), length))
        return result;

    throwOutOfMemoryError(globalObject, scope);
    return nullptr;
}

JSC_DEFINE_JIT_OPERATION(operationSpreadGeneric, JSCell*, (JSGlobalObject* globalObject, JSCell* iterable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto* result = CommonSlowPaths::trySpreadFast(globalObject, iterable);
    RETURN_IF_EXCEPTION(throwScope, nullptr);
    if (result)
        return result;

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
        RETURN_IF_EXCEPTION(throwScope, nullptr);
        array = jsCast<JSArray*>(arrayResult);
    }

    RELEASE_AND_RETURN(throwScope, JSImmutableButterfly::createFromArray(globalObject, vm, array));
}

JSC_DEFINE_JIT_OPERATION(operationSpreadFastArray, JSCell*, (JSGlobalObject* globalObject, JSCell* cell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(isJSArray(cell));
    JSArray* array = jsCast<JSArray*>(cell);
    ASSERT(array->isIteratorProtocolFastAndNonObservable());

    return JSImmutableButterfly::createFromArray(globalObject, vm, array);
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
            throwRangeError(globalObject, scope, "Array size is not a small enough positive integer."_s);
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

    return newArrayWithSpeciesImpl(globalObject, length, array, indexingType);
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSpecies, JSObject*, (JSGlobalObject* globalObject, EncodedJSValue encodedLength, JSObject* array, IndexingType indexingType))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    uint64_t length = static_cast<uint64_t>(JSValue::decode(encodedLength).asNumber());
    return newArrayWithSpeciesImpl(globalObject, length, array, indexingType);
}


JSC_DEFINE_JIT_OPERATION(operationProcessTypeProfilerLogDFG, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    vm.typeProfilerLog()->processLogEntries(vm, "Log Full, called from inside DFG."_s);
}

JSC_DEFINE_JIT_OPERATION(operationResolveScopeForHoistingFuncDeclInEval, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* scope, UniquedStringImpl* impl))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
        
    JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(globalObject, scope, Identifier::fromUid(vm, impl));
    return JSValue::encode(resolvedScope);
}
    
JSC_DEFINE_JIT_OPERATION(operationResolveScope, JSCell*, (JSGlobalObject* globalObject, JSScope* scope, UniquedStringImpl* impl))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSObject* resolvedScope = JSScope::resolve(globalObject, scope, Identifier::fromUid(vm, impl));
    return resolvedScope;
}

JSC_DEFINE_JIT_OPERATION(operationGetDynamicVar, EncodedJSValue, (JSGlobalObject* globalObject, JSObject* scope, UniquedStringImpl* impl, unsigned getPutInfoBits))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    Identifier ident = Identifier::fromUid(vm, impl);
    RELEASE_AND_RETURN(throwScope, JSValue::encode(scope->getPropertySlot(globalObject, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            GetPutInfo getPutInfo(getPutInfoBits);
            if (getPutInfo.resolveMode() == ThrowIfNotFound)
                throwException(globalObject, throwScope, createUndefinedVariableError(globalObject, ident));
            return jsUndefined();
        }

        if (scope->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            JSValue result = slot.getValue(globalObject, ident);
            if (result == jsTDZValue()) {
                throwException(globalObject, throwScope, createTDZError(globalObject));
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

JSC_DEFINE_JIT_OPERATION(operationPutDynamicVarStrict, void, (JSGlobalObject* globalObject, JSObject* scope, EncodedJSValue value, UniquedStringImpl* impl, unsigned getPutInfoBits))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    constexpr bool isStrictMode = true;
    return putDynamicVar(globalObject, vm, scope, value, impl, getPutInfoBits, isStrictMode);
}

JSC_DEFINE_JIT_OPERATION(operationPutDynamicVarNonStrict, void, (JSGlobalObject* globalObject, JSObject* scope, EncodedJSValue value, UniquedStringImpl* impl, unsigned getPutInfoBits))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    constexpr bool isStrictMode = false;
    return putDynamicVar(globalObject, vm, scope, value, impl, getPutInfoBits, isStrictMode);
}

JSC_DEFINE_JIT_OPERATION(operationNormalizeMapKeyHeapBigInt, EncodedJSValue, (VM* vmPointer, JSBigInt* input))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(normalizeMapKey(input));
}

JSC_DEFINE_JIT_OPERATION(operationMapHash, UCPUStrictInt32, (JSGlobalObject* globalObject, EncodedJSValue input))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return toUCPUStrictInt32(jsMapHash(globalObject, vm, JSValue::decode(input)));
}

JSC_DEFINE_JIT_OPERATION(operationMapHashHeapBigInt, UCPUStrictInt32, (VM* vmPointer, JSBigInt* input))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return toUCPUStrictInt32(jsMapHash(input));
}

JSC_DEFINE_JIT_OPERATION(operationJSMapFindBucket, JSCell*, (JSGlobalObject* globalObject, JSCell* map, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSMap::BucketType** bucket = jsCast<JSMap*>(map)->findBucket(globalObject, JSValue::decode(key), hash);
    if (!bucket)
        return vm.sentinelMapBucket();
    return *bucket;
}

JSC_DEFINE_JIT_OPERATION(operationJSSetFindBucket, JSCell*, (JSGlobalObject* globalObject, JSCell* map, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSSet::BucketType** bucket = jsCast<JSSet*>(map)->findBucket(globalObject, JSValue::decode(key), hash);
    if (!bucket)
        return vm.sentinelSetBucket();
    return *bucket;
}

JSC_DEFINE_JIT_OPERATION(operationSetAdd, JSCell*, (JSGlobalObject* globalObject, JSCell* set, EncodedJSValue key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto* bucket = jsCast<JSSet*>(set)->addNormalized(globalObject, JSValue::decode(key), JSValue(), hash);
    if (!bucket)
        return vm.sentinelSetBucket();
    return bucket;
}

JSC_DEFINE_JIT_OPERATION(operationMapSet, JSCell*, (JSGlobalObject* globalObject, JSCell* map, EncodedJSValue key, EncodedJSValue value, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto* bucket = jsCast<JSMap*>(map)->addNormalized(globalObject, JSValue::decode(key), JSValue::decode(value), hash);
    if (!bucket)
        return vm.sentinelMapBucket();
    return bucket;
}

JSC_DEFINE_JIT_OPERATION(operationWeakSetAdd, void, (JSGlobalObject* globalObject, JSCell* set, JSCell* key, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    if (UNLIKELY(!canBeHeldWeakly(key))) {
        auto scope = DECLARE_THROW_SCOPE(vm); // Intentionally putting it here since we would like to make object key case GC-free.
        throwTypeError(globalObject, scope, WeakSetInvalidValueError);
        return;
    }

    jsCast<JSWeakSet*>(set)->add(vm, key, JSValue(), hash);
}

JSC_DEFINE_JIT_OPERATION(operationWeakMapSet, void, (JSGlobalObject* globalObject, JSCell* map, JSCell* key, EncodedJSValue value, int32_t hash))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    if (UNLIKELY(!canBeHeldWeakly(key))) {
        auto scope = DECLARE_THROW_SCOPE(vm); // Intentionally putting it here since we would like to make object key case GC-free.
        throwTypeError(globalObject, scope, WeakMapInvalidKeyError);
        return;
    }

    jsCast<JSWeakMap*>(map)->add(vm, key, JSValue::decode(value), hash);
}

JSC_DEFINE_JIT_OPERATION(operationGetPrototypeOfObject, EncodedJSValue, (JSGlobalObject* globalObject, JSObject* thisObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(thisObject->getPrototype(vm, globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrototypeOf, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue value = JSValue::decode(encodedValue);
    return JSValue::encode(value.getPrototype(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetFullYear, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->year()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCFullYear, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->year()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetMonth, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->month()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCMonth, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->month()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetDate, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->monthDay()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCDate, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->monthDay()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetDay, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->weekDay()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCDay, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->weekDay()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetHours, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->hour()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCHours, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->hour()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetMinutes, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->minute()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCMinutes, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->minute()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetSeconds, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->second()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetUTCSeconds, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->second()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetTimezoneOffset, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(-gregorianDateTime->utcOffsetInMinute()));
}

JSC_DEFINE_JIT_OPERATION(operationDateGetYear, EncodedJSValue, (VM* vmPointer, DateInstance* date))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    const GregorianDateTime* gregorianDateTime = date->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->year() - 1900));
}

JSC_DEFINE_JIT_OPERATION(operationThrowDFG, void, (JSGlobalObject* globalObject, EncodedJSValue valueToThrow))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    scope.throwException(globalObject, JSValue::decode(valueToThrow));
}

JSC_DEFINE_JIT_OPERATION(operationThrowStaticError, void, (JSGlobalObject* globalObject, JSString* message, uint32_t errorType))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    String errorMessage = message->value(globalObject);
    scope.throwException(globalObject, createError(globalObject, static_cast<ErrorTypeWithExtension>(errorType), errorMessage));
}

JSC_DEFINE_JIT_OPERATION(operationLinkDirectCall, void, (OptimizingCallLinkInfo* callLinkInfo, JSFunction* callee))
{
    JSGlobalObject* globalObject = callee->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeSpecializationKind kind = callLinkInfo->specializationKind();
    
    RELEASE_ASSERT(callLinkInfo->isDirect());
    
    // This would happen if the executable died during GC but the CodeBlock did not die. That should
    // not happen because the CodeBlock should have a weak reference to any executable it uses for
    // this purpose.
    RELEASE_ASSERT(callLinkInfo->executable());
    
    // Having a CodeBlock indicates that this is linked. We shouldn't be taking this path if it's
    // linked.
    RELEASE_ASSERT(!callLinkInfo->codeBlock());
    
    // We just don't support this yet.
    RELEASE_ASSERT(!callLinkInfo->isVarargs());
    
    ExecutableBase* executable = callLinkInfo->executable();
    RELEASE_ASSERT(callee->executable() == callLinkInfo->executable());

    JSScope* scope = callee->scopeUnchecked();

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

        functionExecutable->prepareForExecution<FunctionExecutable>(vm, callee, scope, kind, codeBlock);
        RETURN_IF_EXCEPTION(throwScope, void());

        unsigned argumentStackSlots = callLinkInfo->maxArgumentCountIncludingThisForDirectCall();
        if (argumentStackSlots < static_cast<size_t>(codeBlock->numParameters()))
            codePtr = functionExecutable->entrypointFor(kind, MustCheckArity);
        else
            codePtr = functionExecutable->entrypointFor(kind, ArityCheckNotRequired);
    }
    
    linkDirectCall(callFrame, *callLinkInfo, codeBlock, codePtr);
}

JSC_DEFINE_JIT_OPERATION(operationTriggerReoptimizationNow, void, (CodeBlock* codeBlock, CodeBlock* optimizedCodeBlock, OSRExitBase* exit))
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
    ASSERT(JITCode::isOptimizingJIT(optimizedCodeBlock->jitType()));
    
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

    if (!codeBlock->hasOptimizedReplacement()
        && !jitCode->checkIfOptimizationThresholdReached(codeBlock)) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("counter = ", jitCode->tierUpCounter));
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

JSC_DEFINE_JIT_OPERATION(operationTriggerTierUpNow, void, (VM* vmPointer))
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
    
    dataLogLnIf(Options::verboseOSR(),
        *codeBlock, ": Entered triggerTierUpNow with executeCounter = ", jitCode->tierUpCounter);

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

    if (!jitCode->neverExecutedEntry && !triggeredSlowPathToStartCompilation) {
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
        mustHandleValues, ToFTLForOSREntryDeferredCompilationCallback::create(triggerAddress));

    if (jitCode->neverExecutedEntry)
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

JSC_DEFINE_JIT_OPERATION(operationTriggerTierUpNowInLoop, void, (VM* vmPointer, unsigned bytecodeIndexBits))
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

    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Entered triggerTierUpNowInLoop with executeCounter = ", jitCode->tierUpCounter);

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

JSC_DEFINE_JIT_OPERATION(operationTriggerOSREntryNow, char*, (VM* vmPointer, unsigned bytecodeIndexBits))
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

    JITCode* jitCode = codeBlock->jitCode()->dfg();

    dataLogLnIf(Options::verboseOSR(), *codeBlock, ": Entered triggerOSREntryNow with executeCounter = ", jitCode->tierUpCounter);

    return tierUpCommon(vm, callFrame, bytecodeIndex, true);
}

#endif // ENABLE(FTL_JIT)

} } // namespace JSC::DFG

IGNORE_WARNINGS_END

#endif // ENABLE(DFG_JIT)

#endif // ENABLE(JIT)
