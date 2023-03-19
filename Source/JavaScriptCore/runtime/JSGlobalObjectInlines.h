/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "ArrayAllocationProfile.h"
#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "JSClassRef.h"
#include "JSCustomGetterFunction.h"
#include "JSCustomSetterFunction.h"
#include "JSFunction.h"
#include "JSGlobalLexicalEnvironment.h"
#include "JSGlobalObject.h"
#include "JSWeakObjectMapRefInternal.h"
#include "LinkTimeConstant.h"
#include "ObjectPrototype.h"
#include "StrongInlines.h"
#include <wtf/Hasher.h>

namespace JSC {

struct JSGlobalObject::RareData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    unsigned profileGroup { 0 };
    HashMap<OpaqueJSClass*, std::unique_ptr<OpaqueJSClassContextData>> opaqueJSClassData;
};

struct JSGlobalObject::GlobalPropertyInfo {
    GlobalPropertyInfo(const Identifier& i, JSValue v, unsigned a)
        : identifier(i)
        , value(v)
        , attributes(a)
    {
        ASSERT(Thread::current().stack().contains(this));
    }

    const Identifier identifier;
    JSValue value;
    unsigned attributes;
};

ALWAYS_INLINE bool JSGlobalObject::objectPrototypeIsSaneConcurrently(Structure* objectPrototypeStructure)
{
    ASSERT(objectPrototypeStructure->typeInfo().isImmutablePrototypeExoticObject());
    ASSERT(objectPrototypeStructure->storedPrototype().isNull());

    return !hasIndexedProperties(objectPrototypeStructure->indexingType());
}

ALWAYS_INLINE bool JSGlobalObject::arrayPrototypeChainIsSaneConcurrently(Structure* arrayPrototypeStructure, Structure* objectPrototypeStructure)
{
    return !hasIndexedProperties(arrayPrototypeStructure->indexingType())
        && arrayPrototypeStructure->hasMonoProto()
        && arrayPrototypeStructure->storedPrototype() == objectPrototype()
        && objectPrototypeIsSaneConcurrently(objectPrototypeStructure);
}

ALWAYS_INLINE bool JSGlobalObject::stringPrototypeChainIsSaneConcurrently(Structure* stringPrototypeStructure, Structure* objectPrototypeStructure)
{
    return !hasIndexedProperties(stringPrototypeStructure->indexingType())
        && stringPrototypeStructure->hasMonoProto()
        && stringPrototypeStructure->storedPrototype() == objectPrototype()
        && objectPrototypeIsSaneConcurrently(objectPrototypeStructure);
}

ALWAYS_INLINE bool JSGlobalObject::objectPrototypeChainIsSane()
{
    ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
    return m_objectPrototypeChainIsSaneWatchpointSet.isStillValid();
}

ALWAYS_INLINE bool JSGlobalObject::arrayPrototypeChainIsSane()
{
    ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
    return m_arrayPrototypeChainIsSaneWatchpointSet.isStillValid();
}

ALWAYS_INLINE bool JSGlobalObject::stringPrototypeChainIsSane()
{
    ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
    return m_stringPrototypeChainIsSaneWatchpointSet.isStillValid();
}

inline void JSGlobalObject::setUnhandledRejectionCallback(VM& vm, JSObject* function)
{
    m_unhandledRejectionCallback.set(vm, function);
}

ALWAYS_INLINE bool JSGlobalObject::isArrayPrototypeIteratorProtocolFastAndNonObservable()
{
    // We're fast if we don't have any indexed properties on the prototype.
    // We're non-observable if the iteration protocol hasn't changed.
    //
    // Note: it only makes sense to call this from the main thread. If you're
    // trying to prove this behavior on the compiler thread, you'll want to
    // carefully set up watchpoints to have correct ordering while JS code is
    // executing concurrently.

    return arrayIteratorProtocolWatchpointSet().isStillValid() && !isHavingABadTime() && arrayPrototypeChainIsSane();
}

ALWAYS_INLINE bool JSGlobalObject::isArgumentsPrototypeIteratorProtocolFastAndNonObservable()
{
    // Since Arguments iteration uses ArrayIterator, we need to check the state of ArrayIteratorProtocolWatchpointSet.
    // But we do not need to check isHavingABadTime() and array prototype's chain.
    if (!arrayIteratorProtocolWatchpointSet().isStillValid())
        return false;

    if (isHavingABadTime())
        return false;

    // Since [[Prototype]] of arguments is Object.prototype, we do not need to check Array.prototype.
    if (!objectPrototypeChainIsSane())
        return false;

    return true;
}

ALWAYS_INLINE bool JSGlobalObject::isTypedArrayPrototypeIteratorProtocolFastAndNonObservable(TypedArrayType typedArrayType)
{
    ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());

    typedArrayPrototype(typedArrayType); // Materialize WatchpointSet.

    // Since TypedArray iteration uses ArrayIterator, we need to check the state of ArrayIteratorProtocolWatchpointSet.
    // But we do not need to check isHavingABadTime() and array prototype's chain.
    if (!arrayIteratorProtocolWatchpointSet().isStillValid())
        return false;

    // This WatchpointSet ensures that
    //     1. "length" getter is absent on derived TypedArray prototype (e.g. Uint8Array.prototype).
    //     2. @@iterator function is absent on derived TypedArray prototype.
    //     3. derived TypedArray prototype's [[Prototype]] is TypedArray.prototype.
    if (typedArrayIteratorProtocolWatchpointSet(typedArrayType).state() != IsWatched)
        return false;

    // This WatchpointSet ensures that TypedArray.prototype has default "length" getter and @@iterator function.
    if (typedArrayPrototypeIteratorProtocolWatchpointSet().state() != IsWatched)
        return false;

    return true;
}

// We're non-observable if the iteration protocol hasn't changed.
//
// Note: it only makes sense to call this from the main thread. If you're
// trying to prove this behavior on the compiler thread, you'll want to
// carefully set up watchpoints to have correct ordering while JS code is
// executing concurrently.
ALWAYS_INLINE bool JSGlobalObject::isMapPrototypeIteratorProtocolFastAndNonObservable()
{
    return mapIteratorProtocolWatchpointSet().isStillValid();
}

ALWAYS_INLINE bool JSGlobalObject::isSetPrototypeIteratorProtocolFastAndNonObservable()
{
    return setIteratorProtocolWatchpointSet().isStillValid();
}

ALWAYS_INLINE bool JSGlobalObject::isStringPrototypeIteratorProtocolFastAndNonObservable()
{
    return stringIteratorProtocolWatchpointSet().isStillValid();
}

ALWAYS_INLINE bool JSGlobalObject::isMapPrototypeSetFastAndNonObservable()
{
    return mapSetWatchpointSet().isStillValid();
}

ALWAYS_INLINE bool JSGlobalObject::isSetPrototypeAddFastAndNonObservable()
{
    return setAddWatchpointSet().isStillValid();
}

ALWAYS_INLINE Structure* JSGlobalObject::arrayStructureForIndexingTypeDuringAllocation(JSGlobalObject* globalObject, IndexingType indexingType, JSValue newTarget) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!newTarget || newTarget == globalObject->arrayConstructor())
        return globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType);
    auto* functionGlobalObject = getFunctionRealm(globalObject, asObject(newTarget));
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, InternalFunction::createSubclassStructure(globalObject, asObject(newTarget), functionGlobalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType)));
}

inline JSFunction* JSGlobalObject::throwTypeErrorFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::throwTypeErrorFunction)); }
inline JSFunction* JSGlobalObject::iteratorProtocolFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performIteration)); }
inline JSFunction* JSGlobalObject::newPromiseCapabilityFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::newPromiseCapability)); }
inline JSFunction* JSGlobalObject::resolvePromiseFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::resolvePromise)); }
inline JSFunction* JSGlobalObject::rejectPromiseFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::rejectPromise)); }
inline JSFunction* JSGlobalObject::promiseProtoThenFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::defaultPromiseThen)); }
inline JSFunction* JSGlobalObject::performPromiseThenFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performPromiseThen)); }
inline JSFunction* JSGlobalObject::regExpProtoExecFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::regExpBuiltinExec)); }
inline JSFunction* JSGlobalObject::stringProtoSubstringFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::stringSubstring)); }
inline JSFunction* JSGlobalObject::performProxyObjectHasFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectHas)); }
inline JSFunction* JSGlobalObject::performProxyObjectGetFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectGet)); }
inline JSFunction* JSGlobalObject::performProxyObjectGetFunctionConcurrently() const { return linkTimeConstantConcurrently<JSFunction*>(LinkTimeConstant::performProxyObjectGet); }
inline JSFunction* JSGlobalObject::performProxyObjectSetSloppyFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectSetSloppy)); }
inline JSFunction* JSGlobalObject::performProxyObjectSetSloppyFunctionConcurrently() const { return linkTimeConstantConcurrently<JSFunction*>(LinkTimeConstant::performProxyObjectSetSloppy); }
inline JSFunction* JSGlobalObject::performProxyObjectSetStrictFunction() const { return jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectSetStrict)); }
inline JSFunction* JSGlobalObject::performProxyObjectSetStrictFunctionConcurrently() const { return linkTimeConstantConcurrently<JSFunction*>(LinkTimeConstant::performProxyObjectSetStrict); }
inline GetterSetter* JSGlobalObject::regExpProtoGlobalGetter() const { return bitwise_cast<GetterSetter*>(linkTimeConstant(LinkTimeConstant::regExpProtoGlobalGetter)); }
inline GetterSetter* JSGlobalObject::regExpProtoUnicodeGetter() const { return bitwise_cast<GetterSetter*>(linkTimeConstant(LinkTimeConstant::regExpProtoUnicodeGetter)); }
inline GetterSetter* JSGlobalObject::regExpProtoUnicodeSetsGetter() const { return bitwise_cast<GetterSetter*>(linkTimeConstant(LinkTimeConstant::regExpProtoUnicodeSetsGetter)); }

ALWAYS_INLINE VM& getVM(JSGlobalObject* globalObject)
{
    return globalObject->vm();
}

template<typename T>
inline unsigned JSGlobalObject::WeakCustomGetterOrSetterHash<T>::hash(const Weak<T>& value)
{
    if (!value)
        return 0;
    return hash(value->propertyName(), value->customFunctionPointer(), value->slotBaseClassInfoIfExists());
}

template<typename T>
inline bool JSGlobalObject::WeakCustomGetterOrSetterHash<T>::equal(const Weak<T>& a, const Weak<T>& b)
{
    if (!a || !b)
        return false;
    return a == b;
}

template<typename T>
inline unsigned JSGlobalObject::WeakCustomGetterOrSetterHash<T>::hash(const PropertyName& propertyName, typename T::CustomFunctionPointer functionPointer, const ClassInfo* classInfo)
{
    if (!propertyName.isNull())
        return WTF::computeHash(functionPointer, propertyName.uid()->existingSymbolAwareHash(), classInfo);
    return WTF::computeHash(functionPointer, classInfo);
}

inline JSArray* constructEmptyArray(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, unsigned initialLength = 0, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure;
    if (initialLength >= MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)
        structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(globalObject, ArrayWithArrayStorage, newTarget);
    else
        structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSArray* result = JSArray::tryCreate(vm, structure, initialLength);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    return ArrayAllocationProfile::updateLastAllocationFor(profile, result);
}

inline JSArray* constructArray(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const ArgList& values, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArray(globalObject, structure, values));
}

inline JSArray* constructArray(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const JSValue* values, unsigned length, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArray(globalObject, structure, values, length));
}

inline JSArray* constructArrayNegativeIndexed(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const JSValue* values, unsigned length, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArrayNegativeIndexed(globalObject, structure, values, length));
}

inline OptionSet<CodeGenerationMode> JSGlobalObject::defaultCodeGenerationMode() const
{
    OptionSet<CodeGenerationMode> codeGenerationMode;
    if (hasInteractiveDebugger() || Options::forceDebuggerBytecodeGeneration() || Options::debuggerTriggersBreakpointException())
        codeGenerationMode.add(CodeGenerationMode::Debugger);
    if (vm().typeProfiler())
        codeGenerationMode.add(CodeGenerationMode::TypeProfiler);
    if (vm().controlFlowProfiler())
        codeGenerationMode.add(CodeGenerationMode::ControlFlowProfiler);
    return codeGenerationMode;
}

inline Structure* JSGlobalObject::arrayStructureForProfileDuringAllocation(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, JSValue newTarget) const
{
    return arrayStructureForIndexingTypeDuringAllocation(globalObject, ArrayAllocationProfile::selectIndexingTypeFor(profile), newTarget);
}

inline InlineWatchpointSet& JSGlobalObject::arrayBufferSpeciesWatchpointSet(ArrayBufferSharingMode sharingMode)
{
    switch (sharingMode) {
    case ArrayBufferSharingMode::Default:
        return m_arrayBufferSpeciesWatchpointSet;
    case ArrayBufferSharingMode::Shared:
        return m_sharedArrayBufferSpeciesWatchpointSet;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_arrayBufferSpeciesWatchpointSet;
}

inline JSObject* JSGlobalObject::arrayBufferPrototype(ArrayBufferSharingMode sharingMode) const
{
    switch (sharingMode) {
    case ArrayBufferSharingMode::Default:
        return m_arrayBufferStructure.prototype(this);
    case ArrayBufferSharingMode::Shared:
        return m_sharedArrayBufferStructure.prototype(this);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

inline Structure* JSGlobalObject::arrayBufferStructure(ArrayBufferSharingMode sharingMode) const
{
    switch (sharingMode) {
    case ArrayBufferSharingMode::Default:
        return m_arrayBufferStructure.get(this);
    case ArrayBufferSharingMode::Shared:
        return m_sharedArrayBufferStructure.get(this);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

inline JSObject* JSGlobalObject::arrayBufferConstructor(ArrayBufferSharingMode sharingMode) const
{
    switch (sharingMode) {
    case ArrayBufferSharingMode::Default:
        return m_arrayBufferStructure.constructor(this);
    case ArrayBufferSharingMode::Shared:
        return m_sharedArrayBufferStructure.constructor(this);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

inline void JSGlobalObject::createRareDataIfNeeded()
{
    if (m_rareData)
        return;
    m_rareData = makeUnique<RareData>();
}

inline void JSGlobalObject::setProfileGroup(unsigned value)
{
    createRareDataIfNeeded();
    m_rareData->profileGroup = value;
}

inline unsigned JSGlobalObject::profileGroup() const
{
    if (!m_rareData)
        return 0;
    return m_rareData->profileGroup;
}

inline void JSGlobalObject::registerWeakMap(OpaqueJSWeakObjectMap* map)
{
    // FIXME: This used to keep a set, but not clear why that was done.
    map->ref();
}

inline std::unique_ptr<OpaqueJSClassContextData>& JSGlobalObject::contextData(OpaqueJSClass* key)
{
    createRareDataIfNeeded();
    return m_rareData->opaqueJSClassData.add(key, nullptr).iterator->value;
}

inline Structure* JSGlobalObject::errorStructure(ErrorType errorType) const
{
    switch (errorType) {
    case ErrorType::Error:
        return errorStructure();
    case ErrorType::EvalError:
        return m_evalErrorStructure.get(this);
    case ErrorType::RangeError:
        return m_rangeErrorStructure.get(this);
    case ErrorType::ReferenceError:
        return m_referenceErrorStructure.get(this);
    case ErrorType::SyntaxError:
        return m_syntaxErrorStructure.get(this);
    case ErrorType::TypeError:
        return m_typeErrorStructure.get(this);
    case ErrorType::URIError:
        return m_URIErrorStructure.get(this);
    case ErrorType::AggregateError:
        return m_aggregateErrorStructure.get(this);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

inline JSScope* JSGlobalObject::globalScope()
{
    return m_globalLexicalEnvironment.get();
}

inline void JSGlobalObject::addVar(JSGlobalObject* globalObject, const Identifier& propertyName)
{
    if (!hasOwnProperty(globalObject, propertyName))
        addGlobalVar(propertyName);
}

inline InlineWatchpointSet& JSGlobalObject::typedArraySpeciesWatchpointSet(TypedArrayType type)
{
    switch (type) {
    case NotTypedArray:
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8SpeciesWatchpointSet;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## SpeciesWatchpointSet;
        FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_typedArrayInt8SpeciesWatchpointSet;
}

inline InlineWatchpointSet& JSGlobalObject::typedArrayIteratorProtocolWatchpointSet(TypedArrayType type)
{
    switch (type) {
    case NotTypedArray:
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8IteratorProtocolWatchpointSet;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## IteratorProtocolWatchpointSet;
        FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_typedArrayInt8IteratorProtocolWatchpointSet;
}

inline LazyClassStructure& JSGlobalObject::lazyTypedArrayStructure(TypedArrayType type)
{
    switch (type) {
    case NotTypedArray:
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name;
        FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_typedArrayInt8;
}

inline const LazyClassStructure& JSGlobalObject::lazyTypedArrayStructure(TypedArrayType type) const
{
    return const_cast<const LazyClassStructure&>(const_cast<JSGlobalObject*>(this)->lazyTypedArrayStructure(type));
}

inline LazyProperty<JSGlobalObject, Structure>& JSGlobalObject::lazyResizableOrGrowableSharedTypedArrayStructure(TypedArrayType type)
{
    switch (type) {
    case NotTypedArray:
        RELEASE_ASSERT_NOT_REACHED();
        return m_resizableOrGrowableSharedTypedArrayInt8Structure;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_resizableOrGrowableSharedTypedArray ## name ## Structure;
        FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_resizableOrGrowableSharedTypedArrayInt8Structure;
}

inline const LazyProperty<JSGlobalObject, Structure>& JSGlobalObject::lazyResizableOrGrowableSharedTypedArrayStructure(TypedArrayType type) const
{
    return const_cast<const LazyProperty<JSGlobalObject, Structure>&>(const_cast<JSGlobalObject*>(this)->lazyResizableOrGrowableSharedTypedArrayStructure(type));
}

inline Structure* JSGlobalObject::typedArrayStructure(TypedArrayType type, bool isResizableOrGrowableShared) const
{
    if (isResizableOrGrowableShared)
        return lazyResizableOrGrowableSharedTypedArrayStructure(type).get(this);
    return lazyTypedArrayStructure(type).get(this);
}

inline Structure* JSGlobalObject::typedArrayStructureConcurrently(TypedArrayType type, bool isResizableOrGrowableShared) const
{
    if (isResizableOrGrowableShared)
        return lazyResizableOrGrowableSharedTypedArrayStructure(type).getConcurrently();
    return lazyTypedArrayStructure(type).getConcurrently();
}

inline bool JSGlobalObject::isOriginalTypedArrayStructure(Structure* structure, bool isResizableOrGrowableShared)
{
    TypedArrayType type = typedArrayType(structure->typeInfo().type());
    if (type == NotTypedArray)
        return false;
    return typedArrayStructureConcurrently(type, isResizableOrGrowableShared) == structure;
}

inline JSGlobalObject* JSGlobalObject::deriveShadowRealmGlobalObject(JSGlobalObject* globalObject)
{
    auto& vm = globalObject->vm();
    JSGlobalObject* result = createWithCustomMethodTable(vm, createStructureForShadowRealm(vm, jsNull()), globalObject->globalObjectMethodTable());
    return result;
}

inline Structure* JSGlobalObject::createStructure(VM& vm, JSValue prototype)
{
    Structure* result = Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    result->setTransitionWatchpointIsLikelyToBeFired(true);
    return result;
}

inline Structure* JSGlobalObject::createStructureForShadowRealm(VM& vm, JSValue prototype)
{
    Structure* result = Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags & ~IsImmutablePrototypeExoticObject), info());
    result->setTransitionWatchpointIsLikelyToBeFired(true);
    return result;
}

inline JSObject* JSGlobalObject::typedArrayConstructor(TypedArrayType type) const
{
    return lazyTypedArrayStructure(type).constructor(this);
}

inline JSObject* JSGlobalObject::typedArrayPrototype(TypedArrayType type) const
{
    return lazyTypedArrayStructure(type).prototype(this);
}

inline JSCell* JSGlobalObject::linkTimeConstant(LinkTimeConstant value) const
{
    JSCell* result = m_linkTimeConstants[static_cast<unsigned>(value)].getInitializedOnMainThread(this);
    ASSERT(result);
    return result;
}

template<typename Type> inline Type JSGlobalObject::linkTimeConstantConcurrently(LinkTimeConstant value) const
{
    JSCell* result = m_linkTimeConstants[static_cast<unsigned>(value)].getConcurrently();
    if (!result)
        return nullptr;
    return jsCast<Type>(result);
}

} // namespace JSC
