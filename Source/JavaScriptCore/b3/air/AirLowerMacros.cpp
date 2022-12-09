/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "AirLowerMacros.h"

#if ENABLE(B3_JIT)

#include "AirCCallingConvention.h"
#include "AirCode.h"
#include "AirEmitShuffle.h"
#include "AirInsertionSet.h"
#include "AirPhaseScope.h"
#include "B3CCallValue.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 { namespace Air {

void lowerMacros(Code& code)
{
    PhaseScope phaseScope(code, "Air::lowerMacros");

    InsertionSet insertionSet(code);
    for (BasicBlock* block : code) {
        for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
            Inst& inst = block->at(instIndex);

            auto handleCall = [&] () {
                CCallValue* value = inst.origin->as<CCallValue>();
                Kind oldKind = inst.kind;

                Vector<Arg> destinations = computeCCallingConvention(code, value);

                unsigned offset = value->type() == Void ? 0 : 1;
                Vector<ShufflePair, 16> shufflePairs;
                bool hasRegisterSource = false;
                for (unsigned i = 1; i < destinations.size(); ++i) {
                    Value* child = value->child(i);
                    ShufflePair pair(inst.args[offset + i], destinations[i], widthForType(child->type()));
                    shufflePairs.append(pair);
                    hasRegisterSource |= pair.src().isReg();
                }

                if (UNLIKELY(hasRegisterSource))
                    insertionSet.insertInst(instIndex, createShuffle(inst.origin, Vector<ShufflePair>(shufflePairs)));
                else {
                    // If none of the inputs are registers, then we can efficiently lower this
                    // shuffle before register allocation. First we lower all of the moves to
                    // memory, in the hopes that this is the last use of the operands. This
                    // avoids creating interference between argument registers and arguments
                    // that don't go into argument registers.
                    for (ShufflePair& pair : shufflePairs) {
                        if (pair.dst().isMemory())
                            insertionSet.insertInsts(instIndex, pair.insts(code, inst.origin));
                    }

                    // Fill the argument registers by starting with the first one. This avoids
                    // creating interference between things passed to low-numbered argument
                    // registers and high-numbered argument registers. The assumption here is
                    // that lower-numbered argument registers are more likely to be
                    // incidentally clobbered.
                    for (ShufflePair& pair : shufflePairs) {
                        if (!pair.dst().isMemory())
                            insertionSet.insertInsts(instIndex, pair.insts(code, inst.origin));
                    }
                }

                // Indicate that we're using our original callee argument.
                destinations[0] = inst.args[0];

                // Save where the original instruction put its result.
                Arg resultDst = value->type() == Void ? Arg() : inst.args[1];

                inst = buildCCall(code, inst.origin, destinations);
                if (oldKind.effects)
                    inst.kind.effects = true;

                Tmp result = cCallResult(value->type());
                switch (value->type().kind()) {
                case Void:
                case Tuple:
                    break;
                case Float:
                    insertionSet.insert(instIndex + 1, MoveFloat, value, result, resultDst);
                    break;
                case Double:
                    insertionSet.insert(instIndex + 1, MoveDouble, value, result, resultDst);
                    break;
                case Int32:
                    insertionSet.insert(instIndex + 1, Move32, value, result, resultDst);
                    break;
                case Int64:
                    insertionSet.insert(instIndex + 1, Move, value, result, resultDst);
                    break;
                case V128:
                    insertionSet.insert(instIndex + 1, MoveVector, value, result, resultDst);
                    break;
                }
            };

            auto handleVectorBitwiseSelect = [&] {
                if (!isX86())
                    return;

                Tmp lhs = inst.args[1].tmp();
                Tmp rhs = inst.args[2].tmp();
                Tmp dst = inst.args[3].tmp(); // Bitmask is passed via dst.
                auto* origin = inst.origin;

                Tmp scratch = code.newTmp(FP);

                insertionSet.insert(instIndex, VectorAnd, origin, lhs, dst, scratch);
                insertionSet.insert(instIndex, VectorAndnot, origin, rhs, dst, dst);
                insertionSet.insert(instIndex, VectorOr, origin, scratch, dst, dst);

                inst = Inst();
            };

            auto handleVectorMul = [&] {
                if (inst.args[0].simdInfo().lane != SIMDLane::i64x2)
                    return;

                Tmp lhs = inst.args[1].tmp();
                Tmp rhs = inst.args[2].tmp();
                Tmp dst = inst.args[3].tmp();
                auto* origin = inst.origin;

                Tmp lhsLower = code.newTmp(GP);
                Tmp lhsUpper = code.newTmp(GP);
                Tmp rhsLower = code.newTmp(GP);
                Tmp rhsUpper = code.newTmp(GP);

                Tmp tmp = code.newTmp(FP);

                insertionSet.insert(instIndex, VectorExtractLaneInt64, origin, Arg::imm(0), lhs, lhsLower);
                insertionSet.insert(instIndex, VectorExtractLaneInt64, origin, Arg::imm(1), lhs, lhsUpper);
                insertionSet.insert(instIndex, VectorExtractLaneInt64, origin, Arg::imm(0), rhs, rhsLower);
                insertionSet.insert(instIndex, VectorExtractLaneInt64, origin, Arg::imm(1), rhs, rhsUpper);

                insertionSet.insert(instIndex, Mul64, origin, lhsLower, rhsLower);
                insertionSet.insert(instIndex, Mul64, origin, lhsUpper, rhsUpper);
                insertionSet.insert(instIndex, MoveZeroToVector, origin, tmp);
                insertionSet.insert(instIndex, VectorReplaceLaneInt64, origin, Arg::imm(0), rhsLower, tmp);
                insertionSet.insert(instIndex, VectorReplaceLaneInt64, origin, Arg::imm(1), rhsUpper, tmp);
                insertionSet.insert(instIndex, MoveVector, origin, tmp, dst);

                inst = Inst();
            };

            auto handleVectorAllTrue = [&] {
                if (!isARM64())
                    return;
                SIMDInfo simdInfo = inst.args[0].simdInfo();
                Tmp vec = inst.args[1].tmp();
                Tmp dst = inst.args[2].tmp();
                auto* origin = inst.origin;

                Tmp vtmp = code.newTmp(FP);

                ASSERT(scalarTypeIsIntegral(simdInfo.lane));
                switch (simdInfo.lane) {
                case SIMDLane::i64x2:
                    insertionSet.insert(instIndex, CompareIntegerVectorWithZero, origin, Arg::relCond(MacroAssembler::NotEqual), Arg::simdInfo(simdInfo), vec, vtmp);
                    insertionSet.insert(instIndex, VectorUnsignedMin, origin, Arg::simdInfo({ SIMDLane::i32x4, SIMDSignMode::None }), vtmp, vtmp);
                    break;
                case SIMDLane::i32x4:
                case SIMDLane::i16x8:
                case SIMDLane::i8x16:
                    insertionSet.insert(instIndex, VectorUnsignedMin, origin, Arg::simdInfo(simdInfo), vec, vtmp);
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }

                Tmp tmp = code.newTmp(GP);

                insertionSet.insert(instIndex, MoveFloatTo32, origin, vtmp, tmp);
                insertionSet.insert(instIndex, Compare32, origin, Arg::relCond(MacroAssembler::NotEqual), tmp, Arg::imm(0), dst);

                inst = Inst();
            };

            auto handleVectorAnyTrue = [&] {
                if (!isARM64())
                    return;

                Tmp vec = inst.args[0].tmp();
                Tmp dst = inst.args[1].tmp();
                auto* origin = inst.origin;

                Tmp tmp = code.newTmp(GP);

                insertionSet.insert(instIndex, VectorUnsignedMax, origin, Arg::simdInfo({ SIMDLane::i32x4, SIMDSignMode::None }), vec, vec);
                insertionSet.insert(instIndex, MoveFloatTo32, origin, vec, tmp);
                insertionSet.insert(instIndex, Compare32, origin, Arg::relCond(MacroAssembler::NotEqual), tmp, Arg::imm(0), dst);

                inst = Inst();
            };

            auto handleVectorShuffle = [&] {
                if (!isARM64())
                    return;

#if CPU(ARM64)
                Tmp n(ARM64Registers::q0);
                Tmp n2(ARM64Registers::q1);
#else
                Tmp n;
                Tmp n2;
#endif

                v128_t imm;
                imm.u64x2[0] = inst.args[0].value();
                imm.u64x2[1] = inst.args[1].value();
                Tmp a = inst.args[2].tmp();
                Tmp b = inst.args[3].tmp();
                Tmp dst = inst.args[4].tmp();
                auto* origin = inst.origin;

                auto control = code.newTmp(FP);
                insertionSet.insert(instIndex, MoveVector, origin, a, n);
                insertionSet.insert(instIndex, MoveVector, origin, b, n2);

                // FIXME: this is bad, we should load
                auto gpTmp = code.newTmp(GP);
                insertionSet.insert(instIndex, Move, origin, Arg::bigImm(imm.u64x2[0]), gpTmp);
                insertionSet.insert(instIndex, MoveZeroToVector, origin, control);
                insertionSet.insert(instIndex, VectorReplaceLaneInt64, origin, Arg::imm(0), gpTmp, control);
                insertionSet.insert(instIndex, Move, origin, Arg::bigImm(imm.u64x2[1]), gpTmp);
                insertionSet.insert(instIndex, VectorReplaceLaneInt64, origin, Arg::imm(1), gpTmp, control);

                insertionSet.insert(instIndex, VectorSwizzle2, origin, n, n2, control, dst);
                inst = Inst();
            };

            auto handleVectorBitmask = [&] {
                if (!isARM64())
                    return;

                SIMDInfo simdInfo = inst.args[0].simdInfo();
                Tmp vector = inst.args[1].tmp();
                Tmp dst = inst.args[2].tmp();
                auto* origin = inst.origin;

                if (simdInfo.lane == SIMDLane::i64x2) {
                    Tmp vectorTmp = code.newTmp(FP);
                    Tmp gpTmp = code.newTmp(GP);
                    // This might look bad, but remember: every bit of information we destroy contributes to the heat death of the universe.
                    insertionSet.insert(instIndex, VectorSshr8, origin, Arg::simdInfo({ SIMDLane::i64x2, SIMDSignMode::None }), vector, Arg::imm(63), vectorTmp);
                    insertionSet.insert(instIndex, VectorUnzipEven, origin, Arg::simdInfo({ SIMDLane::i8x16, SIMDSignMode::None }), vectorTmp, vectorTmp, vectorTmp);
                    insertionSet.insert(instIndex, MoveDoubleTo64, origin, vectorTmp, gpTmp);
                    insertionSet.insert(instIndex, Rshift64, origin, gpTmp, Arg::imm(31), gpTmp);
                    insertionSet.insert(instIndex, And32, origin, Arg::bitImm(0b11), gpTmp, dst);
                    inst = Inst();
                    return;
                }

                Tmp maskTmp = code.newTmp(FP);

                {
                    v128_t towerOfPower { };
                    switch (simdInfo.lane) {
                    case SIMDLane::i32x4:
                        for (unsigned i = 0; i < 4; ++i)
                            towerOfPower.u32x4[i] = 1 << i;
                        break;
                    case SIMDLane::i16x8:
                        for (unsigned i = 0; i < 8; ++i)
                            towerOfPower.u16x8[i] = 1 << i;
                        break;
                    case SIMDLane::i8x16:
                        for (unsigned i = 0; i < 8; ++i)
                            towerOfPower.u8x16[i] = 1 << i;
                        for (unsigned i = 0; i < 8; ++i)
                            towerOfPower.u8x16[i + 8] = 1 << i;
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }

                    // FIXME: this is bad, we should load
                    auto gpTmp = code.newTmp(GP);
                    insertionSet.insert(instIndex, Move, origin, Arg::bigImm(towerOfPower.u64x2[0]), gpTmp);
                    insertionSet.insert(instIndex, MoveZeroToVector, origin, maskTmp);
                    insertionSet.insert(instIndex, VectorReplaceLaneInt64, origin, Arg::imm(0), gpTmp, maskTmp);
                    insertionSet.insert(instIndex, Move, origin, Arg::bigImm(towerOfPower.u64x2[1]), gpTmp);
                    insertionSet.insert(instIndex, VectorReplaceLaneInt64, origin, Arg::imm(1), gpTmp, maskTmp);
                }

                Tmp vectorTmp = code.newTmp(FP);

                insertionSet.insert(instIndex, VectorSshr8, origin, Arg::simdInfo(simdInfo), vector, Arg::imm(elementByteSize(simdInfo.lane) * 8 - 1), vectorTmp);
                insertionSet.insert(instIndex, VectorAnd, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), vectorTmp, maskTmp, vectorTmp);

                if (simdInfo.lane == SIMDLane::i8x16) {
                    Tmp maskedHi = code.newTmp(FP);
                    insertionSet.insert(instIndex, VectorExtractPair, origin, Arg::simdInfo({ SIMDLane::i8x16, SIMDSignMode::None }), Arg::imm(8), vectorTmp, vectorTmp, maskedHi);
                    insertionSet.insert(instIndex, VectorZipUpper, origin, Arg::simdInfo({ SIMDLane::i8x16, SIMDSignMode::None }), vectorTmp, maskedHi, vectorTmp);
                    simdInfo.lane = SIMDLane::i16x8;
                }
                insertionSet.insert(instIndex, VectorHorizontalAdd, origin, Arg::simdInfo(simdInfo), vectorTmp, vectorTmp);
                insertionSet.insert(instIndex, MoveFloatTo32, origin, vectorTmp, dst);

                inst = Inst();
            };

            switch (inst.kind.opcode) {
            case ColdCCall:
                if (code.optLevel() < 2)
                    handleCall();
                break;
            case CCall:
                handleCall();
                break;
            case VectorAllTrue:
                handleVectorAllTrue();
                break;
            case VectorAnyTrue:
                handleVectorAnyTrue();
                break;
            case VectorMul:
                handleVectorMul();
                break;
            case VectorBitmask:
                handleVectorBitmask();
                break;
            case VectorShuffle:
                handleVectorShuffle();
                break;
            case VectorBitwiseSelect:
                handleVectorBitwiseSelect();
                break;
            default:
                break;
            }
        }
        insertionSet.execute(block);
    }
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

