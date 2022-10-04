/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "JSArrayBufferPrototype.h"

#include "ArrayPrototypeInlines.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferPrototypeInlines.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(arrayBufferProtoFuncSlice);
static JSC_DECLARE_HOST_FUNCTION(arrayBufferProtoGetterFuncByteLength);
static JSC_DECLARE_HOST_FUNCTION(sharedArrayBufferProtoFuncSlice);
static JSC_DECLARE_HOST_FUNCTION(sharedArrayBufferProtoGetterFuncByteLength);

std::optional<JSValue> arrayBufferSpeciesConstructorSlow(JSGlobalObject* globalObject, JSArrayBuffer* thisObject, ArrayBufferSharingMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool isValid = speciesWatchpointIsValid(thisObject, mode);
    scope.assertNoException();
    if (LIKELY(isValid))
        return std::nullopt;

    JSValue constructor = thisObject->get(globalObject, vm.propertyNames->constructor);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (constructor.isConstructor()) {
        JSObject* constructorObject = jsCast<JSObject*>(constructor);
        JSGlobalObject* globalObjectFromConstructor = constructorObject->globalObject();
        bool isAnyArrayBufferConstructor = constructorObject == globalObjectFromConstructor->arrayBufferConstructor(mode);
        if (isAnyArrayBufferConstructor)
            return std::nullopt;
    }

    if (constructor.isUndefined())
        return std::nullopt;

    if (!constructor.isObject()) {
        throwTypeError(globalObject, scope, "constructor property should not be null"_s);
        return std::nullopt;
    }

    JSValue species = constructor.get(globalObject, vm.propertyNames->speciesSymbol);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    return species.isUndefinedOrNull() ? std::nullopt : std::make_optional(species);
}

static ALWAYS_INLINE std::pair<SpeciesConstructResult, JSArrayBuffer*> speciesConstructArrayBuffer(JSGlobalObject* globalObject, JSArrayBuffer* thisObject, unsigned length, ArrayBufferSharingMode mode)
{
    // This is optimized way of SpeciesConstruct invoked from {ArrayBuffer,SharedArrayBuffer}.prototype.slice.
    // https://tc39.es/ecma262/#sec-arraybuffer.prototype.slice
    // https://tc39.es/ecma262/#sec-sharedarraybuffer.prototype.slice
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr std::pair<SpeciesConstructResult, JSArrayBuffer*> errorResult { SpeciesConstructResult::Exception, nullptr };
    constexpr std::pair<SpeciesConstructResult, JSArrayBuffer*> fastPathResult { SpeciesConstructResult::FastPath, nullptr };

    // Fast path in the normal case where the user has not set an own constructor and the ArrayBuffer.prototype.constructor is normal.
    // We need prototype check for subclasses of ArrayBuffer, which are ArrayBuffer objects but have a different prototype by default.
    std::optional<JSValue> species = arrayBufferSpeciesConstructor(globalObject, thisObject, mode);
    RETURN_IF_EXCEPTION(scope, errorResult);
    if (!species)
        return fastPathResult;

    // 16. Let new be ? Construct(ctor, « 𝔽(newLen) »).
    MarkedArgumentBuffer args;
    args.append(jsNumber(length));
    ASSERT(!args.hasOverflowed());
    JSObject* newObject = construct(globalObject, species.value(), args, "Species construction did not get a valid constructor"_s);
    RETURN_IF_EXCEPTION(scope, errorResult);

    // 17. Perform ? RequireInternalSlot(new, [[ArrayBufferData]]).
    JSArrayBuffer* result = jsDynamicCast<JSArrayBuffer*>(newObject);
    if (UNLIKELY(!result)) {
        throwTypeError(globalObject, scope, "Species construction does not create ArrayBuffer"_s);
        return errorResult;
    }

    if (mode == ArrayBufferSharingMode::Default) {
        // 18. If IsSharedArrayBuffer(new) is true, throw a TypeError exception.
        if (result->isShared()) {
            throwTypeError(globalObject, scope, "ArrayBuffer.prototype.slice creates SharedArrayBuffer"_s);
            return errorResult;
        }
        // 19. If IsDetachedBuffer(new) is true, throw a TypeError exception.
        if (result->impl()->isDetached()) {
            throwVMTypeError(globalObject, scope, "Created ArrayBuffer is detached"_s);
            return errorResult;
        }
    } else {
        // 17. If IsSharedArrayBuffer(new) is false, throw a TypeError exception.
        if (!result->isShared()) {
            throwTypeError(globalObject, scope, "SharedArrayBuffer.prototype.slice creates non-shared ArrayBuffer"_s);
            return errorResult;
        }
    }

    // 20. If SameValue(new, O) is true, throw a TypeError exception.
    if (result == thisObject) {
        throwVMTypeError(globalObject, scope, "Species construction returns same ArrayBuffer to a receiver"_s);
        return errorResult;
    }

    // 21. If new.[[ArrayBufferByteLength]] < newLen, throw a TypeError exception.
    if (result->impl()->byteLength() < length) {
        throwVMTypeError(globalObject, scope, "Species construction returns ArrayBuffer which byteLength is less than requested"_s);
        return errorResult;
    }

    return { SpeciesConstructResult::CreatedObject, result };
}


static EncodedJSValue arrayBufferSlice(JSGlobalObject* globalObject, JSValue arrayBufferValue, JSValue startValue, JSValue endValue, ArrayBufferSharingMode mode)
{
    // https://tc39.es/ecma262/#sec-arraybuffer.prototype.slice
    // https://tc39.es/ecma262/#sec-sharedarraybuffer.prototype.slice

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 2. Perform ? RequireInternalSlot(O, [[ArrayBufferData]]).
    // 3. If IsSharedArrayBuffer(O) is true, throw a TypeError exception.
    JSArrayBuffer* thisObject = jsDynamicCast<JSArrayBuffer*>(arrayBufferValue);
    if (!thisObject || (mode != thisObject->impl()->sharingMode()))
        return throwVMTypeError(globalObject, scope, makeString("Receiver must be "_s, mode == ArrayBufferSharingMode::Default ? "ArrayBuffer"_s : "SharedArrayBuffer"_s));

    // 4. If IsDetachedBuffer(O) is true, throw a TypeError exception.
    if (mode == ArrayBufferSharingMode::Default && thisObject->impl()->isDetached())
        return throwVMTypeError(globalObject, scope, "Receiver is detached"_s);

    // 5. Let len be O.[[ArrayBufferByteLength]].
    unsigned byteLength = thisObject->impl()->byteLength();
    unsigned firstIndex = 0;
    double relativeStart = startValue.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (relativeStart < 0)
        firstIndex = static_cast<unsigned>(std::max<double>(byteLength + relativeStart, 0));
    else
        firstIndex = static_cast<unsigned>(std::min<double>(relativeStart, byteLength));
    ASSERT(firstIndex <= byteLength);

    unsigned finalIndex = 0;
    if (!endValue.isUndefined()) {
        double relativeEnd = endValue.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (relativeEnd < 0)
            finalIndex = static_cast<unsigned>(std::max<double>(byteLength + relativeEnd, 0));
        else
            finalIndex = static_cast<unsigned>(std::min<double>(relativeEnd, byteLength));
    } else
        finalIndex = thisObject->impl()->byteLength();
    ASSERT(finalIndex <= byteLength);

    // 14. Let newLen be max(final - first, 0).
    unsigned newLength = (finalIndex >= firstIndex) ? finalIndex - firstIndex : 0;

    // 15. Let ctor be ? SpeciesConstructor(O, %ArrayBuffer%).
    auto speciesResult = speciesConstructArrayBuffer(globalObject, thisObject, newLength, mode);
    // We can only get an exception if we call some user function.
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
        return { };

    // 23. If IsDetachedBuffer(O) is true, throw a TypeError exception.
    if (mode == ArrayBufferSharingMode::Default && thisObject->impl()->isDetached())
        return throwVMTypeError(globalObject, scope, "Receiver is detached"_s);

    if (LIKELY(speciesResult.first == SpeciesConstructResult::FastPath)) {
        ASSERT(!thisObject->impl()->isDetached());
        auto newBuffer = thisObject->impl()->sliceWithClampedIndex(firstIndex, finalIndex);
        if (!newBuffer)
            return JSValue::encode(throwOutOfMemoryError(globalObject, scope));
        Structure* structure = globalObject->arrayBufferStructure(newBuffer->sharingMode());
        JSArrayBuffer* result = JSArrayBuffer::create(vm, structure, WTFMove(newBuffer));
        return JSValue::encode(result);
    }

    // 24. Let fromBuf be O.[[ArrayBufferData]].
    // 25. Let toBuf be new.[[ArrayBufferData]].
    // 26. Perform CopyDataBlockBytes(toBuf, 0, fromBuf, first, newLen).
    JSArrayBuffer* newObject = speciesResult.second;
    ASSERT(!thisObject->impl()->isDetached());
    ASSERT(!newObject->impl()->isDetached());
    ASSERT(newObject->impl()->byteLength() >= newLength);
    memcpy(newObject->impl()->data(), static_cast<const char*>(thisObject->impl()->data()) + firstIndex, newLength);
    return JSValue::encode(newObject);
}

static EncodedJSValue arrayBufferByteLength(JSGlobalObject* globalObject, JSValue arrayBufferValue, ArrayBufferSharingMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsDynamicCast<JSArrayBuffer*>(arrayBufferValue);
    if (!thisObject || (mode != thisObject->impl()->sharingMode()))
        return throwVMTypeError(globalObject, scope, makeString("Receiver must be "_s, mode == ArrayBufferSharingMode::Default ? "ArrayBuffer"_s : "SharedArrayBuffer"_s));

    if (mode == ArrayBufferSharingMode::Default && thisObject->impl()->isDetached())
        return JSValue::encode(jsNumber(0));

    return JSValue::encode(jsNumber(thisObject->impl()->byteLength()));
}

JSC_DEFINE_HOST_FUNCTION(arrayBufferProtoFuncSlice, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return arrayBufferSlice(globalObject, callFrame->thisValue(), callFrame->argument(0), callFrame->argument(1), ArrayBufferSharingMode::Default);
}

// http://tc39.github.io/ecmascript_sharedmem/shmem.html#sec-get-arraybuffer.prototype.bytelength
JSC_DEFINE_HOST_FUNCTION(arrayBufferProtoGetterFuncByteLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return arrayBufferByteLength(globalObject, callFrame->thisValue(), ArrayBufferSharingMode::Default);
}

JSC_DEFINE_HOST_FUNCTION(sharedArrayBufferProtoFuncSlice, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return arrayBufferSlice(globalObject, callFrame->thisValue(), callFrame->argument(0), callFrame->argument(1), ArrayBufferSharingMode::Shared);
}

// http://tc39.github.io/ecmascript_sharedmem/shmem.html#StructuredData.SharedArrayBuffer.prototype.get_byteLength
JSC_DEFINE_HOST_FUNCTION(sharedArrayBufferProtoGetterFuncByteLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return arrayBufferByteLength(globalObject, callFrame->thisValue(), ArrayBufferSharingMode::Shared);
}

const ClassInfo JSArrayBufferPrototype::s_info = {
    "ArrayBuffer"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSArrayBufferPrototype)
};

JSArrayBufferPrototype::JSArrayBufferPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSArrayBufferPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject, ArrayBufferSharingMode sharingMode)
{
    Base::finishCreation(vm);
    
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsNontrivialString(vm, arrayBufferSharingModeName(sharingMode)), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    if (sharingMode == ArrayBufferSharingMode::Default) {
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, arrayBufferProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
        JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteLength, arrayBufferProtoGetterFuncByteLength, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    } else {
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, sharedArrayBufferProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
        JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteLength, sharedArrayBufferProtoGetterFuncByteLength, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    }
}

JSArrayBufferPrototype* JSArrayBufferPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, ArrayBufferSharingMode sharingMode)
{
    JSArrayBufferPrototype* prototype =
        new (NotNull, allocateCell<JSArrayBufferPrototype>(vm))
        JSArrayBufferPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject, sharingMode);
    return prototype;
}

Structure* JSArrayBufferPrototype::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC

