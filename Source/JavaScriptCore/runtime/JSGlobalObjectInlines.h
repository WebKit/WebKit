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

#include "JSGlobalObject.h"

#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "JSCustomGetterFunction.h"
#include "JSCustomSetterFunction.h"
#include "JSFunction.h"
#include "LinkTimeConstant.h"
#include "ObjectPrototype.h"
#include <wtf/Hasher.h>

namespace JSC {

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
inline GetterSetter* JSGlobalObject::regExpProtoGlobalGetter() const { return bitwise_cast<GetterSetter*>(linkTimeConstant(LinkTimeConstant::regExpProtoGlobalGetter)); }
inline GetterSetter* JSGlobalObject::regExpProtoUnicodeGetter() const { return bitwise_cast<GetterSetter*>(linkTimeConstant(LinkTimeConstant::regExpProtoUnicodeGetter)); }

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

} // namespace JSC
