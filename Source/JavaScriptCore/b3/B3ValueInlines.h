/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3ArgumentRegValue.h"
#include "B3AtomicValue.h"
#include "B3BottomTupleValue.h"
#include "B3CCallValue.h"
#include "B3CheckValue.h"
#include "B3Const128Value.h"
#include "B3Const32Value.h"
#include "B3Const64Value.h"
#include "B3ConstDoubleValue.h"
#include "B3ConstFloatValue.h"
#include "B3ExtractValue.h"
#include "B3FenceValue.h"
#include "B3MemoryValue.h"
#include "B3PatchpointValue.h"
#include "B3PhiChildren.h"
#include "B3Procedure.h"
#include "B3SIMDValue.h"
#include "B3SlotBaseValue.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3Value.h"
#include "B3VariableValue.h"
#include "B3WasmAddressValue.h"
#include "B3WasmBoundsCheckValue.h"
#include <wtf/GraphNodeWorklist.h>

namespace JSC { namespace B3 {

#define DISPATCH_ON_KIND(MACRO) \
    switch (kind().opcode()) { \
    case FramePointer: \
    case Nop: \
    case Phi: \
    case Jump: \
    case Oops: \
    case EntrySwitch: \
    case Return: \
    case Identity: \
    case Opaque: \
    case Neg: \
    case Clz: \
    case Abs: \
    case Ceil: \
    case Floor: \
    case Sqrt: \
    case SExt8: \
    case SExt16: \
    case Trunc: \
    case SExt32: \
    case ZExt32: \
    case FloatToDouble: \
    case IToD: \
    case DoubleToFloat: \
    case IToF: \
    case BitwiseCast: \
    case Branch: \
    case Depend: \
    case Add: \
    case Sub: \
    case Mul: \
    case Div: \
    case UDiv: \
    case Mod: \
    case UMod: \
    case FMax: \
    case FMin: \
    case BitAnd: \
    case BitOr: \
    case BitXor: \
    case Shl: \
    case SShr: \
    case ZShr: \
    case RotR: \
    case RotL: \
    case Equal: \
    case NotEqual: \
    case LessThan: \
    case GreaterThan: \
    case LessEqual: \
    case GreaterEqual: \
    case Above: \
    case Below: \
    case AboveEqual: \
    case BelowEqual: \
    case EqualOrUnordered: \
    case Select: \
        return MACRO(Value); \
    case ArgumentReg: \
        return MACRO(ArgumentRegValue); \
    case Const32: \
        return MACRO(Const32Value); \
    case Const64: \
        return MACRO(Const64Value); \
    case Const128: \
        return MACRO(Const128Value); \
    case ConstFloat: \
        return MACRO(ConstFloatValue); \
    case ConstDouble: \
        return MACRO(ConstDoubleValue); \
    case BottomTuple: \
        return MACRO(BottomTupleValue); \
    case Fence: \
        return MACRO(FenceValue); \
    case SlotBase: \
        return MACRO(SlotBaseValue); \
    case Get: \
    case Set: \
        return MACRO(VariableValue); \
    case Load8Z: \
    case Load8S: \
    case Load16Z: \
    case Load16S: \
    case Load: \
    case Store8: \
    case Store16: \
    case Store: \
        return MACRO(MemoryValue); \
    case Switch: \
        return MACRO(SwitchValue); \
    case Upsilon: \
        return MACRO(UpsilonValue); \
    case Extract: \
        return MACRO(ExtractValue); \
    case WasmAddress: \
        return MACRO(WasmAddressValue); \
    case WasmBoundsCheck: \
        return MACRO(WasmBoundsCheckValue); \
    case AtomicXchgAdd: \
    case AtomicXchgAnd: \
    case AtomicXchgOr: \
    case AtomicXchgSub: \
    case AtomicXchgXor: \
    case AtomicXchg: \
    case AtomicWeakCAS: \
    case AtomicStrongCAS: \
        return MACRO(AtomicValue); \
    case CCall: \
        return MACRO(CCallValue); \
    case Check: \
    case CheckAdd: \
    case CheckSub: \
    case CheckMul: \
        return MACRO(CheckValue); \
    case Patchpoint: \
        return MACRO(PatchpointValue); \
    case VectorExtractLane: \
    case VectorReplaceLane: \
    case VectorEqual: \
    case VectorNotEqual: \
    case VectorLessThan: \
    case VectorLessThanOrEqual: \
    case VectorBelow: \
    case VectorBelowOrEqual: \
    case VectorGreaterThan: \
    case VectorGreaterThanOrEqual: \
    case VectorAbove: \
    case VectorAboveOrEqual: \
    case VectorAdd: \
    case VectorSub: \
    case VectorAddSat: \
    case VectorSubSat: \
    case VectorMul: \
    case VectorDotProduct: \
    case VectorDiv: \
    case VectorMin: \
    case VectorMax: \
    case VectorPmin: \
    case VectorPmax: \
    case VectorNarrow: \
    case VectorNot: \
    case VectorAnd: \
    case VectorAndnot: \
    case VectorOr: \
    case VectorXor: \
    case VectorShl: \
    case VectorShr: \
    case VectorAbs: \
    case VectorNeg: \
    case VectorPopcnt: \
    case VectorCeil: \
    case VectorFloor: \
    case VectorTrunc: \
    case VectorTruncSat: \
    case VectorConvert: \
    case VectorConvertLow: \
    case VectorNearest: \
    case VectorSqrt: \
    case VectorExtendLow: \
    case VectorExtendHigh: \
    case VectorPromote: \
    case VectorDemote: \
    case VectorSplat: \
    case VectorAnyTrue: \
    case VectorAllTrue: \
    case VectorAvgRound: \
    case VectorBitmask: \
    case VectorBitwiseSelect: \
    case VectorExtaddPairwise: \
    case VectorMulSat: \
    case VectorSwizzle: \
    case VectorShuffle: \
        return MACRO(SIMDValue); \
    default: \
        RELEASE_ASSERT_NOT_REACHED(); \
    }

ALWAYS_INLINE size_t Value::adjacencyListOffset() const
{
#define VALUE_TYPE_SIZE(ValueType) sizeof(ValueType)
    DISPATCH_ON_KIND(VALUE_TYPE_SIZE);
#undef VALUE_TYPE_SIZE
}

ALWAYS_INLINE Value* Value::cloneImpl() const
{
#define VALUE_TYPE_CLONE(ValueType) allocate<ValueType>(*static_cast<const ValueType*>(this))
    DISPATCH_ON_KIND(VALUE_TYPE_CLONE);
#undef VALUE_TYPE_CLONE
}

template<typename BottomProvider>
void Value::replaceWithBottom(const BottomProvider& bottomProvider)
{
    if (m_type == Void) {
        replaceWithNop();
        return;
    }
    
    if (isConstant())
        return;
    
    replaceWithIdentity(bottomProvider(m_origin, m_type));
}

template<typename T>
inline T* Value::as()
{
    if (T::accepts(kind()))
        return static_cast<T*>(this);
    return nullptr;
}

template<typename T>
inline const T* Value::as() const
{
    return const_cast<Value*>(this)->as<T>();
}

inline bool Value::isConstant() const
{
    return B3::isConstant(opcode());
}

inline bool Value::isInteger() const
{
    return type() == Int32 || type() == Int64;
}

inline bool Value::hasInt32() const
{
    return !!as<Const32Value>();
}

inline int32_t Value::asInt32() const
{
    return as<Const32Value>()->value();
}

inline bool Value::isInt32(int32_t value) const
{
    return hasInt32() && asInt32() == value;
}

inline bool Value::hasInt64() const
{
    return !!as<Const64Value>();
}

inline int64_t Value::asInt64() const
{
    return as<Const64Value>()->value();
}

inline bool Value::isInt64(int64_t value) const
{
    return hasInt64() && asInt64() == value;
}

inline bool Value::hasInt() const
{
    return hasInt32() || hasInt64();
}

inline int64_t Value::asInt() const
{
    return hasInt32() ? asInt32() : asInt64();
}

inline bool Value::isInt(int64_t value) const
{
    return hasInt() && asInt() == value;
}

inline bool Value::hasIntPtr() const
{
    if (is64Bit())
        return hasInt64();
    return hasInt32();
}

inline intptr_t Value::asIntPtr() const
{
    if (is64Bit())
        return asInt64();
    return asInt32();
}

inline bool Value::isIntPtr(intptr_t value) const
{
    return hasIntPtr() && asIntPtr() == value;
}

inline bool Value::hasDouble() const
{
    return !!as<ConstDoubleValue>();
}

inline double Value::asDouble() const
{
    return as<ConstDoubleValue>()->value();
}

inline bool Value::isEqualToDouble(double value) const
{
    return hasDouble() && asDouble() == value;
}

inline bool Value::hasFloat() const
{
    return !!as<ConstFloatValue>();
}

inline float Value::asFloat() const
{
    return as<ConstFloatValue>()->value();
}

inline bool Value::hasV128() const
{
    return !!as<Const128Value>();
}

inline v128_t Value::asV128() const
{
    return as<Const128Value>()->value();
}

inline bool Value::hasNumber() const
{
    return hasInt() || hasDouble() || hasFloat();
}

inline bool Value::isNegativeZero() const
{
    if (hasDouble()) {
        double value = asDouble();
        return !value && std::signbit(value);
    }
    if (hasFloat()) {
        float value = asFloat();
        return !value && std::signbit(value);
    }
    return false;
}

template<typename T>
inline bool Value::isRepresentableAs() const
{
    switch (opcode()) {
    case Const32:
        return B3::isRepresentableAs<T>(asInt32());
    case Const64:
        return B3::isRepresentableAs<T>(asInt64());
    case ConstDouble:
        return B3::isRepresentableAs<T>(asDouble());
    case ConstFloat:
        return B3::isRepresentableAs<T>(asFloat());
    default:
        return false;
    }
}

template<typename T>
inline T Value::asNumber() const
{
    switch (opcode()) {
    case Const32:
        return static_cast<T>(asInt32());
    case Const64:
        return static_cast<T>(asInt64());
    case ConstDouble:
        return static_cast<T>(asDouble());
    case ConstFloat:
        return static_cast<T>(asFloat());
    default:
        return T();
    }
}

template<typename Functor>
void Value::walk(const Functor& functor, PhiChildren* phiChildren)
{
    GraphNodeWorklist<Value*> worklist;
    worklist.push(this);
    while (Value* value = worklist.pop()) {
        WalkStatus status = functor(value);
        switch (status) {
        case Continue:
            if (value->opcode() == Phi) {
                if (phiChildren)
                    worklist.pushAll(phiChildren->at(value).values());
            } else
                worklist.pushAll(value->children());
            break;
        case IgnoreChildren:
            break;
        case Stop:
            return;
        }
    }
}

inline bool Value::isSIMDValue() const
{
    return !!as<SIMDValue>();
}

inline SIMDValue* Value::asSIMDValue()
{
    return as<SIMDValue>();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
