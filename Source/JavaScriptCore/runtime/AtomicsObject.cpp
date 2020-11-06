/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

const ClassInfo AtomicsObject::s_info = { "Atomics", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AtomicsObject) };

AtomicsObject::AtomicsObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

AtomicsObject* AtomicsObject::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    AtomicsObject* object = new (NotNull, allocateCell<AtomicsObject>(vm.heap)) AtomicsObject(vm, structure);
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
    ASSERT(inherits(vm, info()));
    
#define PUT_DIRECT_NATIVE_FUNC(lowerName, upperName, count) \
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, #lowerName), count, atomicsFunc ## upperName, Atomics ## upperName ## Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    FOR_EACH_ATOMICS_FUNC(PUT_DIRECT_NATIVE_FUNC)
#undef PUT_DIRECT_NATIVE_FUNC

    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

namespace {

template<typename Adaptor, typename Func>
EncodedJSValue atomicReadModifyWriteCase(JSGlobalObject* globalObject, const JSValue* args, ThrowScope& scope, JSArrayBufferView* typedArrayView, unsigned accessIndex, const Func& func)
{
    JSGenericTypedArrayView<Adaptor>* typedArray = jsCast<JSGenericTypedArrayView<Adaptor>*>(typedArrayView);
    
    double extraArgs[Func::numExtraArgs + 1]; // Add 1 to avoid 0 size array error in VS.
    for (unsigned i = 0; i < Func::numExtraArgs; ++i) {
        double value = args[2 + i].toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));
        extraArgs[i] = value;
    }

    if (typedArray->isDetached())
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    return JSValue::encode(func(typedArray->typedVector() + accessIndex, extraArgs));
}

static unsigned validateAtomicAccess(VM& vm, JSGlobalObject* globalObject, JSArrayBufferView* typedArrayView, JSValue accessIndexValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned accessIndex = 0;
    if (LIKELY(accessIndexValue.isUInt32()))
        accessIndex = accessIndexValue.asUInt32();
    else {
        accessIndex = accessIndexValue.toIndex(globalObject, "accessIndex");
        RETURN_IF_EXCEPTION(scope, 0);
    }
    
    ASSERT(typedArrayView->length() <= static_cast<unsigned>(INT_MAX));
    if (accessIndex >= typedArrayView->length()) {
        throwRangeError(globalObject, scope, "Access index out of bounds for atomic access."_s);
        return 0;
    }
    
    return accessIndex;
}

static JSArrayBufferView* validateTypedArray(JSGlobalObject* globalObject, JSValue typedArrayValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!typedArrayValue.isCell()) {
        throwTypeError(globalObject, scope, "Argument needs to be a typed array."_s);
        return nullptr;
    }

    JSCell* typedArrayCell = typedArrayValue.asCell();
    if (!isTypedView(typedArrayCell->classInfo(vm)->typedArrayStorageType)) {
        throwTypeError(globalObject, scope, "Argument needs to be a typed array."_s);
        return nullptr;
    }

    JSArrayBufferView* typedArray = jsCast<JSArrayBufferView*>(typedArrayCell);
    if (typedArray->isDetached()) {
        throwTypeError(globalObject, scope, "Argument typed array is detached."_s);
        return nullptr;
    }
    return typedArray;
}

enum class TypedArrayOperationMode { Read, Write };
template<TypedArrayOperationMode mode>
inline JSArrayBufferView* validateIntegerTypedArray(JSGlobalObject* globalObject, JSValue typedArrayValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBufferView* typedArray = validateTypedArray(globalObject, typedArrayValue);
    RETURN_IF_EXCEPTION(scope, { });

    if constexpr (mode == TypedArrayOperationMode::Write) {
        switch (typedArray->type()) {
        case Int32ArrayType:
            break;
        default:
            throwTypeError(globalObject, scope, "Typed array argument must be an Int32Array."_s);
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
            break;
        default:
            throwTypeError(globalObject, scope, "Typed array argument must be an Int8Array, Int16Array, Int32Array, Uint8Array, Uint16Array, or Uint32Array."_s);
            return { };
        }
    }
    return typedArray;
}

template<typename Func>
EncodedJSValue atomicReadModifyWrite(VM& vm, JSGlobalObject* globalObject, const JSValue* args, const Func& func)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBufferView* typedArrayView = validateIntegerTypedArray<TypedArrayOperationMode::Read>(globalObject, args[0]);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned accessIndex = validateAtomicAccess(vm, globalObject, typedArrayView, args[1]);
    RETURN_IF_EXCEPTION(scope, { });

    switch (typedArrayView->type()) {
    case Int8ArrayType:
        return atomicReadModifyWriteCase<Int8Adaptor>(globalObject, args, scope, typedArrayView, accessIndex, func);
    case Int16ArrayType:
        return atomicReadModifyWriteCase<Int16Adaptor>(globalObject, args, scope, typedArrayView, accessIndex, func);
    case Int32ArrayType:
        return atomicReadModifyWriteCase<Int32Adaptor>(globalObject, args, scope, typedArrayView, accessIndex, func);
    case Uint8ArrayType:
        return atomicReadModifyWriteCase<Uint8Adaptor>(globalObject, args, scope, typedArrayView, accessIndex, func);
    case Uint16ArrayType:
        return atomicReadModifyWriteCase<Uint16Adaptor>(globalObject, args, scope, typedArrayView, accessIndex, func);
    case Uint32ArrayType:
        return atomicReadModifyWriteCase<Uint32Adaptor>(globalObject, args, scope, typedArrayView, accessIndex, func);
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
    return atomicReadModifyWrite(globalObject->vm(), globalObject, args, func);
}

struct AddFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        return jsNumber(WTF::atomicExchangeAdd(ptr, toInt32(args[0])));
    }
};

struct AndFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        return jsNumber(WTF::atomicExchangeAnd(ptr, toInt32(args[0])));
    }
};

struct CompareExchangeFunc {
    static constexpr unsigned numExtraArgs = 2;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        T expected = static_cast<T>(toInt32(args[0]));
        T newValue = static_cast<T>(toInt32(args[1]));
        return jsNumber(WTF::atomicCompareExchangeStrong(ptr, expected, newValue));
    }
};

struct ExchangeFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        return jsNumber(WTF::atomicExchange(ptr, static_cast<T>(toInt32(args[0]))));
    }
};

struct LoadFunc {
    static constexpr unsigned numExtraArgs = 0;
    
    template<typename T>
    JSValue operator()(T* ptr, const double*) const
    {
        return jsNumber(WTF::atomicLoadFullyFenced(ptr));
    }
};

struct OrFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        return jsNumber(WTF::atomicExchangeOr(ptr, toInt32(args[0])));
    }
};

struct StoreFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        double valueAsInt = args[0];
        T valueAsT = static_cast<T>(toInt32(valueAsInt));
        WTF::atomicStoreFullyFenced(ptr, valueAsT);
        return jsNumber(valueAsInt);
    }
};

struct SubFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        return jsNumber(WTF::atomicExchangeSub(ptr, toInt32(args[0])));
    }
};

struct XorFunc {
    static constexpr unsigned numExtraArgs = 1;
    
    template<typename T>
    JSValue operator()(T* ptr, const double* args) const
    {
        return jsNumber(WTF::atomicExchangeXor(ptr, toInt32(args[0])));
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
        result = true;
        break;
    default:
        result = false;
        break;
    }
    return JSValue::encode(jsBoolean(result));
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
    return atomicReadModifyWrite(globalObject, callFrame, StoreFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncSub, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return atomicReadModifyWrite(globalObject, callFrame, SubFunc());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncWait, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* typedArrayBuffer = validateIntegerTypedArray<TypedArrayOperationMode::Write>(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    auto* typedArray = jsCast<JSInt32Array*>(typedArrayBuffer);

    if (!typedArray->isShared()) {
        throwTypeError(globalObject, scope, "Typed array for wait/notify must wrap a SharedArrayBuffer."_s);
        return JSValue::encode(jsUndefined());
    }

    unsigned accessIndex = validateAtomicAccess(vm, globalObject, typedArray, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    int32_t* ptr = typedArray->typedVector() + accessIndex;

    int32_t expectedValue = callFrame->argument(2).toInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));

    double timeoutInMilliseconds = callFrame->argument(3).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));
    Seconds timeout = Seconds::infinity();
    if (!std::isnan(timeoutInMilliseconds))
        timeout = std::max(Seconds::fromMilliseconds(timeoutInMilliseconds), 0_s);

    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread()) {
        throwTypeError(globalObject, scope, "Atomics.wait cannot be called from the current thread."_s);
        return JSValue::encode(jsUndefined());
    }

    bool didPassValidation = false;
    ParkingLot::ParkResult result;
    {
        ReleaseHeapAccessScope releaseHeapAccessScope(vm.heap);
        result = ParkingLot::parkConditionally(
            ptr,
            [&] () -> bool {
                didPassValidation = WTF::atomicLoad(ptr) == expectedValue;
                return didPassValidation;
            },
            [] () { },
            MonotonicTime::now() + timeout);
    }
    if (!didPassValidation)
        return JSValue::encode(vm.smallStrings.notEqualString());
    if (!result.wasUnparked)
        return JSValue::encode(vm.smallStrings.timedOutString());
    return JSValue::encode(vm.smallStrings.okString());
}

JSC_DEFINE_HOST_FUNCTION(atomicsFuncNotify, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* typedArrayBuffer = validateIntegerTypedArray<TypedArrayOperationMode::Write>(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    auto* typedArray = jsCast<JSInt32Array*>(typedArrayBuffer);

    unsigned accessIndex = validateAtomicAccess(vm, globalObject, typedArray, callFrame->argument(1));
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

    if (!typedArray->isShared())
        return JSValue::encode(jsNumber(0));

    int32_t* ptr = typedArray->typedVector() + accessIndex;
    return JSValue::encode(jsNumber(ParkingLot::unparkCount(ptr, count)));
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
    return atomicReadModifyWrite(vm, globalObject, args, AddFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsAnd, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(vm, globalObject, args, AndFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsCompareExchange, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue expected, EncodedJSValue newValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(expected), JSValue::decode(newValue)};
    return atomicReadModifyWrite(vm, globalObject, args, CompareExchangeFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsExchange, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(vm, globalObject, args, ExchangeFunc());
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
    return atomicReadModifyWrite(vm, globalObject, args, LoadFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsOr, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(vm, globalObject, args, OrFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsStore, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(vm, globalObject, args, StoreFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsSub, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(vm, globalObject, args, SubFunc());
}

JSC_DEFINE_JIT_OPERATION(operationAtomicsXor, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue args[] = {JSValue::decode(base), JSValue::decode(index), JSValue::decode(operand)};
    return atomicReadModifyWrite(vm, globalObject, args, XorFunc());
}

IGNORE_WARNINGS_END

} // namespace JSC

