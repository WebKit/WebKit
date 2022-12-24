/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "AtomicsObject.h"

#include "FrameTracers.h"
#include "JSCInlines.h"
#include "JSTypedArrays.h"
#include "ReleaseHeapAccessScope.h"
#include "TypedArrayController.h"
#include "WaiterListManager.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(AtomicsObject);

#define FOR_EACH_ATOMICS_FUNC(macro)                                    \
    macro(add, Add, 3)                                                  \
    macro(and, And, 3)                                                  \
    macro(compareExchange, CompareExchange, 4)                          \
    macro(exchange, Exchange, 3)                                        \
    macro(isLockFree, IsLockFree, 1)                                    \
    macro(load, Load, 2)                                                \
    macro(notify, Notify, 3)                                            \
    macro(or, Or, 3)                                                    \
    macro(store, Store, 3)                                              \
    macro(sub, Sub, 3)                                                  \
    macro(wait, Wait, 4)                                                \
    macro(xor, Xor, 3)

#define DECLARE_FUNC_PROTO(lowerName, upperName, count)  \
    static JSC_DECLARE_HOST_FUNCTION(atomicsFunc ## upperName);
FOR_EACH_ATOMICS_FUNC(DECLARE_FUNC_PROTO)
#undef DECLARE_FUNC_PROTO

static JSC_DECLARE_HOST_FUNCTION(atomicsFuncWaitAsync);

const ClassInfo AtomicsObject::s_info = { "Atomics"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AtomicsObject) };

AtomicsObject::AtomicsObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

AtomicsObject* AtomicsObject::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    AtomicsObject* object = new (NotNull, allocateCell<AtomicsObject>(vm)) AtomicsObject(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* AtomicsObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void AtomicsObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    
#define PUT_DIRECT_NATIVE_FUNC(lowerName, upperName, count) \
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, #lowerName ""_s), count, atomicsFunc ## upperName, ImplementationVisibility::Public, Atomics ## upperName ## Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    FOR_EACH_ATOMICS_FUNC(PUT_DIRECT_NATIVE_FUNC)
#undef PUT_DIRECT_NATIVE_FUNC

    if (Options::useAtomicsWaitAsync() && vm.vmType == VM::Default)
        putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "waitAsync"_s), 4, atomicsFuncWaitAsync, ImplementationVisibility::Public, AtomicsWaitAsyncIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

namespace {

template<typename Adaptor, typename Func>
EncodedJSValue atomicReadModifyWriteCase(JSGlobalObject* globalObject, VM& vm, const JSValue* args, JSArrayBufferView* typedArrayView, unsigned accessIndex, const Func& func)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGenericTypedArrayView<Adaptor>* typedArray = jsCast<JSGenericTypedArrayView<Adaptor>*>(typedArrayView);
    
    typename Adaptor::Type extraArgs[Func::numExtraArgs + 1]; // Add 1 to avoid 0 size array error in VS.
    for (unsigned i = 0; i < Func::numExtraArgs; ++i) {
        auto value = toNativeFromValue<Adaptor>(globalObject, args[2 + i]);
        RETURN_IF_EXCEPTION(scope, { });
        extraArgs[i] = value;
    }

    if (typedArray->isDetached() || !typedArray->inBounds(accessIndex))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    auto result = func(typedArray->typedVector() + accessIndex, extraArgs);
    RELEASE_AND_RETURN(scope, JSValue::encode(Adaptor::toJSValue(globalObject, result)));
}

static unsigned validateAtomicAccess(JSGlobalObject* globalObject, VM& vm, JSArrayBufferView* typedArrayView, JSValue accessIndexValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned accessIndex = 0;
    size_t length = typedArrayView->length();
    if (LIKELY(accessIndexValue.isUInt32()))
        accessIndex = accessIndexValue.asUInt32();
    else {
        accessIndex = accessIndexValue.toIndex(globalObject, "accessIndex");
        RETURN_IF_EXCEPTION(scope, 0);
    }

    if (accessIndex >= length) {
        throwRangeError(globalObject, scope, "Access index out of bounds for atomic access."_s);
        return 0;
    }
    
    return accessIndex;
}

enum class TypedArrayOperationMode { ReadWrite, Wait };
template<TypedArrayOperationMode mode>
inline JSArrayBufferView* validateIntegerTypedArray(JSGlobalObject* globalObject, JSValue typedArrayValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBufferView* typedArray = validateTypedArray(globalObject, typedArrayValue);
    RETURN_IF_EXCEPTION(scope, { });

    if constexpr (mode == TypedArrayOperationMode::Wait) {
        switch (typedArray->type()) {
        case Int32ArrayType:
        case BigInt64ArrayType:
            break;
        default:
            throwTypeError(globalObject, scope, "Typed array argument must be an Int32Array or BigInt64Array."_s);
            return { };
        }
    } else {
        switch (typedArray->type()) {
        case Int8ArrayType:
        case Int16ArrayType:
        case Int32ArrayType:
        case Uint8ArrayType:
        case Uint16ArrayType:
        case Uint32ArrayType:
        case BigInt64ArrayType:
        case BigUint64ArrayType:
            break;
        default:
            throwTypeError(globalObject, scope, "Typed array argument must be an Int8Array, Int16Array, Int32Array, Uint8Array, Uint16Array, Uint32Array, BigInt64Array, or BigUint64Array."_s);
            return { };
        }
    }
    return typedArray;
}

template<typename Func>
EncodedJSValue atomicReadModifyWrite(JSGlobalObject* globalObject, VM& vm, const JSValue* args, const Func& func)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBufferView* typedArrayView = validateIntegerTypedArray<TypedArrayOperationMode::ReadWrite>(globalObject, args[0]);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned accessIndex = validateAtomicAccess(globalObject, vm, typedArrayView, args[1]);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    switch (typedArrayView->type()) {
    case Int8ArrayType:
        return atomicReadModifyWriteCase<Int8Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    case Int16ArrayType:
        return atomicReadModifyWriteCase<Int16Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    case Int32ArrayType:
        return atomicReadModifyWriteCase<Int32Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    case Uint8ArrayType:
        return atomicReadModifyWriteCase<Uint8Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    case Uint16ArrayType:
        return atomicReadModifyWriteCase<Uint16Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    case Uint32ArrayType:
        return atomicReadModifyWriteCase<Uint32Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    case BigInt64ArrayType:
        return atomicReadModifyWriteCase<BigInt64Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    case BigUint64ArrayType:
        return atomicReadModifyWriteCase<BigUint64Adaptor>(globalObject, vm, args, typedArrayView, accessIndex, func);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return JSValue::encode(jsUndefined());
    }
}

template<typename Func>
EncodedJSValue atomicReadModifyWrite(JSGlobalObject* globalObject, CallFrame* callFrame, const Func& func)
{
    JSValue args[2 + Func::numExtraArgs];
    for (unsigned i = 2 + Func::numExtraArgs; i--;)
        args[i] = callFrame->argument(i);
    return atomicReadModifyWrite(globalObject, globalObject->vm(), args, func);
}

struct AddFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    T operator()(T* ptr, const T* args) const
    {
        return WTF::atomicExchangeAdd(ptr, args[0]);
    }
};

struct AndFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    T operator()(T* ptr, const T* args) const
    {
        return WTF::atomicExchangeAnd(ptr, args[0]);
    }
};

struct CompareExchangeFunc {
    static constexpr unsigned numExtraArgs = 2;
    
    template<typename T>
    T operator()(T* ptr, const T* args) const
    {
        T expected = args[0];
        T newValue = args[1];
        return WTF::atomicCompareExchangeStrong(ptr, expected, newValue);
    }
};

struct ExchangeFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    T operator()(T* ptr, const T* args) const
    {
        return WTF::atomicExchange(ptr, args[0]);
    }
};

struct LoadFunc {
    static constexpr unsigned numExtraArgs = 0;
    
    template<typename T>
    T operator()(T* ptr, const T*) const
    {
        return WTF::atomicLoadFullyFenced(ptr);
    }
};

struct OrFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    T operator()(T* ptr, const T* args) const
    {
        return WTF::atomicExchangeOr(ptr, args[0]);
    }
};

struct SubFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    T operator()(T* ptr, const T* args) const
    {
        return WTF::atomicExchangeSub(ptr, args[0]);
    }
};

struct XorFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    T operator()(T* ptr, const T* args) const
    {
        return WTF::atomicExchangeXor(ptr, args[0]);
    }
};

EncodedJSValue isLockFree(JSGlobalObject* globalObject, JSValue arg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t size = arg.toInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));
    
    bool result;
    switch (size) {
    case 1:
    case 2:
    case 4:
    case 8:
        result = true;
        break;
    default:
        result = false;
        break;
    }
    return JSValue::encode(jsBoolean(result));
}

template<typename Adaptor>
EncodedJSValue atomicStoreCase(JSGlobalObject* globalObject, VM& vm, JSValue operand, JSArrayBufferView* typedArrayView, unsigned accessIndex)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGenericTypedArrayView<Adaptor>* typedArray = jsCast<JSGenericTypedArrayView<Adaptor>*>(typedArrayView);

    typename Adaptor::Type extraArg;
    JSValue value;
    if constexpr (std::is_same_v<Adaptor, BigInt64Adaptor> || std::is_same_v<Adaptor, BigUint64Adaptor>) {
        value = operand.toBigInt(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        extraArg = toNativeFromValue<Adaptor>(globalObject, value);
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        value = jsNumber(operand.toIntegerOrInfinity(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
        extraArg = toNativeFromValue<Adaptor>(globalObject, value);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (typedArray->isDetached() || !typedArray->inBounds(accessIndex))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    WTF::atomicStoreFullyFenced(typedArray->typedVector() + accessIndex, extraArg);
    RELEASE_AND_RETURN(scope, JSValue::encode(value));
}

EncodedJSValue atomicStore(JSGlobalObject* globalObject, VM& vm, JSValue base, JSValue index, JSValue operand)
{
    // https://tc39.es/ecma262/#sec-atomics.store

    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBufferView* typedArrayView = validateIntegerTypedArray<TypedArrayOperationMode::ReadWrite>(globalObject, base);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned accessIndex = validateAtomicAccess(globalObject, vm, typedArrayView, index);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    switch (typedArrayView->type()) {
    case Int8ArrayType:
        return atomicStoreCase<Int8Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    case Int16ArrayType:
        return atomicStoreCase<Int16Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    case Int32ArrayType:
        return atomicStoreCase<Int32Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    case Uint8ArrayType:
        return atomicStoreCase<Uint8Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    case Uint16ArrayType:
        return atomicStoreCase<Uint16Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    case Uint32ArrayType:
        return atomicStoreCase<Uint32Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    case BigInt64ArrayType:
        return atomicStoreCase<BigInt64Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    case BigUint64ArrayType:
        return atomicStoreCase<BigUint64Adaptor>(globalObject, vm, operand, typedArrayView, accessIndex);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }
}

} // anonymous namespace

JSC_DEFINE_HOST_FUNCTION(atomicsFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, AddFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncAnd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, AndFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncCompareExchange, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, CompareExchangeFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncExchange, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, ExchangeFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncIsLockFree, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return isLockFree(globalObject, callFrame->argument(0));
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncLoad, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, LoadFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncOr, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, OrFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncStore, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicStore(globalObject, globalObject->vm(), callFrame->argument(0), callFrame->argument(1), callFrame->argument(2));
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncSub, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, SubFunc());
}


template<typename ValueType, typename JSArrayType>
JSValue atomicsWaitImpl(JSGlobalObject* globalObject, JSArrayType* typedArray, unsigned accessIndex, ValueType expectedValue, JSValue timeoutValue, AtomicsWaitType type)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ValueType* ptr = typedArray->typedVector() + accessIndex;

    double timeoutInMilliseconds = timeoutValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    Seconds timeout = Seconds::infinity();
    if (!std::isnan(timeoutInMilliseconds))
        timeout = std::max(Seconds::fromMilliseconds(timeoutInMilliseconds), 0_s);

    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread()) {
        throwTypeError(globalObject, scope, makeString("Atomics."_s,
            (type == AtomicsWaitType::Async ? "waitAsync"_s : "wait"_s), " cannot be called from the current thread."_s));
        return { };
    }

    if (type == AtomicsWaitType::Async)
        return WaiterListManager::singleton().waitAsync(globalObject, vm, ptr, expectedValue, timeout);

    auto result = WaiterListManager::singleton().waitSync(vm, ptr, expectedValue, timeout);
    switch (result) {
    case WaiterListManager::WaitSyncResult::OK:
        return vm.smallStrings.okString();
    case WaiterListManager::WaitSyncResult::NotEqual:
        return vm.smallStrings.notEqualString();
    case WaiterListManager::WaitSyncResult::TimedOut:
        return vm.smallStrings.timedOutString();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncWait, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* typedArrayView = validateIntegerTypedArray<TypedArrayOperationMode::Wait>(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    if (!typedArrayView->isShared())
        return throwVMTypeError(globalObject, scope, "Typed array for wait/waitAsync/notify must wrap a SharedArrayBuffer."_s);

    unsigned accessIndex = validateAtomicAccess(globalObject, vm, typedArrayView, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    switch (typedArrayView->type()) {
    case Int32ArrayType: {
        int32_t expectedValue = callFrame->argument(2).toInt32(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, JSValue::encode(atomicsWaitImpl<int32_t>(globalObject, jsCast<JSInt32Array*>(typedArrayView), accessIndex, expectedValue, callFrame->argument(3), AtomicsWaitType::Sync)));
    }
    case BigInt64ArrayType: {
        int64_t expectedValue = callFrame->argument(2).toBigInt64(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, JSValue::encode(atomicsWaitImpl<int64_t>(globalObject, jsCast<JSBigInt64Array*>(typedArrayView), accessIndex, expectedValue, callFrame->argument(3), AtomicsWaitType::Sync)));
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncWaitAsync, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* typedArrayView = validateIntegerTypedArray<TypedArrayOperationMode::Wait>(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    if (!typedArrayView->isShared())
        return throwVMTypeError(globalObject, scope, "Typed array for wait/waitAsync/notify must wrap a SharedArrayBuffer."_s);

    unsigned accessIndex = validateAtomicAccess(globalObject, vm, typedArrayView, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    switch (typedArrayView->type()) {
    case Int32ArrayType: {
        int32_t expectedValue = callFrame->argument(2).toInt32(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, JSValue::encode(atomicsWaitImpl<int32_t>(globalObject, jsCast<JSInt32Array*>(typedArrayView), accessIndex, expectedValue, callFrame->argument(3), AtomicsWaitType::Async)));
    }
    case BigInt64ArrayType: {
        int64_t expectedValue = callFrame->argument(2).toBigInt64(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, JSValue::encode(atomicsWaitImpl<int64_t>(globalObject, jsCast<JSBigInt64Array*>(typedArrayView), accessIndex, expectedValue, callFrame->argument(3), AtomicsWaitType::Async)));
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

EncodedJSValue getWaiterListSize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* typedArrayView = validateIntegerTypedArray<TypedArrayOperationMode::Wait>(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    if (!typedArrayView->isShared())
        return throwVMTypeError(globalObject, scope, "Typed array for waiterListSize must wrap a SharedArrayBuffer."_s);

    unsigned accessIndex = validateAtomicAccess(globalObject, vm, typedArrayView, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    switch (typedArrayView->type()) {
    case Int32ArrayType: {
        auto ptr = jsCast<JSInt32Array*>(typedArrayView)->typedVector() + accessIndex;
        RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(WaiterListManager::singleton().waiterListSize(ptr))));
    }
    case BigInt64ArrayType: {
        auto ptr = jsCast<JSBigInt64Array*>(typedArrayView)->typedVector() + accessIndex;
        RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(WaiterListManager::singleton().waiterListSize(ptr))));
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncNotify, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* typedArrayView = validateIntegerTypedArray<TypedArrayOperationMode::Wait>(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    unsigned accessIndex = validateAtomicAccess(globalObject, vm, typedArrayView, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    JSValue countValue = callFrame->argument(2);
    unsigned count = UINT_MAX;
    if (!countValue.isUndefined()) {
        double countInt = countValue.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        double countDouble = std::max(0.0, countInt);
        if (countDouble < UINT_MAX)
            count = static_cast<unsigned>(countDouble);
    }

    if (!typedArrayView->isShared())
        return JSValue::encode(jsNumber(0));

    switch (typedArrayView->type()) {
    case Int32ArrayType: {
        int32_t* ptr = jsCast<JSInt32Array*>(typedArrayView)->typedVector() + accessIndex;
        return JSValue::encode(jsNumber(WaiterListManager::singleton().notifyWaiter(ptr, count)));
    }
    case BigInt64ArrayType: {
        int64_t* ptr = jsCast<JSBigInt64Array*>(typedArrayView)->typedVector() + accessIndex;
        return JSValue::encode(jsNumber(WaiterListManager::singleton().notifyWaiter(ptr, count)));
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncXor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, XorFunc());
}

IGNORE_WARNINGS_BEGIN("frame-address")

JSC_DEFINE_JIT_OPERATION(operationAtomicsAdd, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(globalObject, vm, args, AddFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsAnd, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(globalObject, vm, args, AndFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsCompareExchange, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue expected, EncodedJSValue newValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(expected), JSValue::decode(newValue)};
    return atomicReadModifyWrite(globalObject, vm, args, CompareExchangeFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsExchange, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(globalObject, vm, args, ExchangeFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsIsLockFree, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue size))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return isLockFree(globalObject, JSValue::decode(size));
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsLoad, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index)};
    return atomicReadModifyWrite(globalObject, vm, args, LoadFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsOr, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(globalObject, vm, args, OrFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsStore, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return atomicStore(globalObject, vm, JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand));
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsSub, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(globalObject, vm, args, SubFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsXor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(globalObject, vm, args, XorFunc());
}

IGNORE_WARNINGS_END

} // namespace JSC

