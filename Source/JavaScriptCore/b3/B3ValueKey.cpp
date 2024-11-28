/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "B3ValueKey.h"

#if ENABLE(B3_JIT)

#include "B3ArgumentRegValue.h"
#include "B3ProcedureInlines.h"
#include "B3SlotBaseValue.h"
#include "B3ValueInlines.h"
#include "B3ValueKeyInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace B3 {

ValueKey ValueKey::intConstant(Type type, int64_t value)
{
    switch (type.kind()) {
    case Int32:
        return ValueKey(Const32, Int32, value);
    case Int64:
        return ValueKey(Const64, Int64, value);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return ValueKey();
    }
}

void ValueKey::dump(PrintStream& out) const
{
    out.print(m_type, " ", m_kind, "(", u.indices[0], ", ", u.indices[1], ", ", u.indices[2], ")");
}

Value* ValueKey::materialize(Procedure& proc, Origin origin) const
{
    // NOTE: We sometimes cannot return a Value* for some key, like for Check and friends. That's because
    // though those nodes have side exit effects. It would be weird to materialize anything that has a side
    // exit. We can't possibly know enough about a side exit to know where it would be safe to emit one.
    switch (opcode()) {
    case FramePointer:
        return proc.add<Value>(kind(), type(), origin);
    case Identity:
    case Opaque:
    case Abs:
    case Floor:
    case Ceil:
    case Sqrt:
    case Neg:
    case Depend:
    case SExt8:
    case SExt16:
    case SExt8To64:
    case SExt16To64:
    case SExt32:
    case ZExt32:
    case Clz:
    case Trunc:
    case IToD:
    case IToF:
    case FloatToDouble:
    case DoubleToFloat:
        return proc.add<Value>(kind(), type(), origin, child(proc, 0));
    case Add:
    case Sub:
    case Mul:
    case Div:
    case UDiv:
    case Mod:
    case UMod:
    case FMax:
    case FMin:
    case BitAnd:
    case BitOr:
    case BitXor:
    case Shl:
    case SShr:
    case ZShr:
    case RotR:
    case RotL:
    case Equal:
    case NotEqual:
    case LessThan:
    case GreaterThan:
    case Above:
    case Below:
    case AboveEqual:
    case BelowEqual:
    case EqualOrUnordered:
        return proc.add<Value>(kind(), type(), origin, child(proc, 0), child(proc, 1));
    case Select:
        return proc.add<Value>(kind(), type(), origin, child(proc, 0), child(proc, 1), child(proc, 2));
    case Const32:
        return proc.add<Const32Value>(origin, static_cast<int32_t>(value()));
    case Const64:
        return proc.add<Const64Value>(origin, value());
    case Const128:
        return proc.add<Const128Value>(origin, vectorValue());
    case ConstDouble:
        return proc.add<ConstDoubleValue>(origin, doubleValue());
    case ConstFloat:
        return proc.add<ConstFloatValue>(origin, floatValue());
    case BottomTuple:
        return proc.add<BottomTupleValue>(origin, type());
    case ArgumentReg:
        return proc.add<ArgumentRegValue>(origin, Reg::fromIndex(static_cast<unsigned>(value())));
    case SlotBase:
        return proc.add<SlotBaseValue>(origin, proc.stackSlots()[value()]);
    case VectorNot:
    case VectorSplat:
    case VectorAbs:
    case VectorNeg:
    case VectorPopcnt:
    case VectorCeil:
    case VectorFloor:
    case VectorTrunc:
    case VectorTruncSat:
    case VectorRelaxedTruncSat:
    case VectorConvert:
    case VectorConvertLow:
    case VectorNearest:
    case VectorSqrt:
    case VectorExtendLow:
    case VectorExtendHigh:
    case VectorPromote:
    case VectorDemote:
    case VectorBitmask:
    case VectorAnyTrue:
    case VectorAllTrue:
    case VectorExtaddPairwise:
        return proc.add<SIMDValue>(origin, kind(), type(), simdInfo(), child(proc, 0));
    case VectorExtractLane:
    case VectorDupElement:
        return proc.add<SIMDValue>(origin, kind(), type(), simdInfo(), static_cast<uint8_t>(u.indices[1]), child(proc, 0));
    case VectorEqual:
    case VectorNotEqual:
    case VectorLessThan:
    case VectorLessThanOrEqual:
    case VectorBelow:
    case VectorBelowOrEqual:
    case VectorGreaterThan:
    case VectorGreaterThanOrEqual:
    case VectorAbove:
    case VectorAboveOrEqual:
    case VectorAdd:
    case VectorSub:
    case VectorAddSat:
    case VectorSubSat:
    case VectorMul:
    case VectorDotProduct:
    case VectorDiv:
    case VectorMin:
    case VectorMax:
    case VectorPmin:
    case VectorPmax:
    case VectorNarrow:
    case VectorAnd:
    case VectorAndnot:
    case VectorOr:
    case VectorXor:
    case VectorShl:
    case VectorShr:
    case VectorMulSat:
    case VectorAvgRound:
    case VectorShiftByVector:
    case VectorRelaxedSwizzle:
        return proc.add<SIMDValue>(origin, kind(), type(), simdInfo(), child(proc, 0), child(proc, 1));
    case VectorReplaceLane:
    case VectorMulByElement:
        return proc.add<SIMDValue>(origin, kind(), type(), simdInfo(), static_cast<uint8_t>(u.indices[2]), child(proc, 0), child(proc, 1));
    case VectorRelaxedMAdd:
    case VectorRelaxedNMAdd:
    case VectorBitwiseSelect:
        return proc.add<SIMDValue>(origin, kind(), type(), simdInfo(), child(proc, 0), child(proc, 1), child(proc, 2));
    case VectorSwizzle:
        if (u.indices[2] == UINT32_MAX)
            return proc.add<SIMDValue>(origin, kind(), type(), simdInfo(), child(proc, 0), child(proc, 1));
        return proc.add<SIMDValue>(origin, kind(), type(), simdInfo(), child(proc, 0), child(proc, 1), child(proc, 2));
    default:
        return nullptr;
    }
}

} } // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
