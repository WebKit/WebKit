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

                unsigned resultCount = cCallResultCount(value);
                ASSERT_IMPLIES(is64Bit(), resultCount <= 1);
                
                Vector<ShufflePair, 16> shufflePairs;
                bool hasRegisterSource = false;
                unsigned offset = 1;
                auto addNextPair = [&](Width width) {
                    ShufflePair pair(inst.args[offset + resultCount], destinations[offset], width);
                    shufflePairs.append(pair);
                    hasRegisterSource |= pair.src().isReg();
                    ++offset;
                };
                for (unsigned i = 1; i < value->numChildren(); ++i) {
                    Value* child = value->child(i);
                    for (unsigned j = 0; j < cCallArgumentRegisterCount(child); j++)
                        addNextPair(cCallArgumentRegisterWidth(child));
                }
                ASSERT(offset = inst.args.size());
                
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
                Arg resultDst0 = resultCount >= 1 ? inst.args[1] : Arg();
#if USE(JSVALUE32_64)
                Arg resultDst1 = resultCount >= 2 ? inst.args[2] : Arg();
#endif

                inst = buildCCall(code, inst.origin, destinations);
                if (oldKind.effects)
                    inst.kind.effects = true;

                switch (value->type().kind()) {
                case Void:
                case Tuple:
                    break;
                case Float:
                    insertionSet.insert(instIndex + 1, MoveFloat, value, cCallResult(value, 0), resultDst0);
                    break;
                case Double:
                    insertionSet.insert(instIndex + 1, MoveDouble, value, cCallResult(value, 0), resultDst0);
                    break;
                case Int32:
                    insertionSet.insert(instIndex + 1, Move32, value, cCallResult(value, 0), resultDst0);
                    break;
                case Int64:
                    insertionSet.insert(instIndex + 1, Move, value, cCallResult(value, 0), resultDst0);
#if USE(JSVALUE32_64)
                    insertionSet.insert(instIndex + 1, Move, value, cCallResult(value, 1), resultDst1);
#endif
                    break;
                case V128:
                    ASSERT(is64Bit());
                    insertionSet.insert(instIndex + 1, MoveVector, value, cCallResult(value, 0), resultDst0);
                    break;
                }
            };

            auto handleVectorBitwiseSelect = [&] {
                if (!isX86())
                    return;

                Tmp lhs = inst.args[0].tmp();
                Tmp rhs = inst.args[1].tmp();
                Tmp dst = inst.args[2].tmp(); // Bitmask is passed via dst.
                auto* origin = inst.origin;

                Tmp scratch = code.newTmp(FP);

                insertionSet.insert(instIndex, VectorAnd, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), lhs, dst, scratch);
                insertionSet.insert(instIndex, VectorAndnot, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), rhs, dst, dst);
                insertionSet.insert(instIndex, VectorOr, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), scratch, dst, dst);

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

            auto handleVectorMin = [&] {
                if (!isX86() || !scalarTypeIsFloatingPoint(inst.args[0].simdInfo().lane))
                    return;

                // Intel's vectorized minimum instruction has slightly different semantics to the WebAssembly vectorized
                // minimum instruction, namely in terms of signed zero values and propagating NaNs. VectorPmin implements
                // a fast version of this instruction that compiles down to a single op, without conforming to the exact
                // semantics. In order to precisely implement VectorMin, we need to do extra work on Intel to check for
                // the necessary edge cases.

                SIMDInfo simdInfo = inst.args[0].simdInfo();
                Tmp lhs = inst.args[1].tmp();
                Tmp rhs = inst.args[2].tmp();
                Tmp dst = inst.args[3].tmp();
                auto* origin = inst.origin;

                Tmp tmp = code.newTmp(FP);

                // Compute result in both directions.
                insertionSet.insert(instIndex, VectorPmin, origin, Arg::simdInfo(simdInfo), rhs, lhs, tmp);
                insertionSet.insert(instIndex, VectorPmin, origin, Arg::simdInfo(simdInfo), lhs, rhs, dst);

                // OR results, propagating the sign bit for negative zeroes, and NaNs.
                insertionSet.insert(instIndex, VectorOr, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), tmp, dst, tmp);

                // Canonicalize NaNs by checking for unordered values and clearing payload if necessary.
                insertionSet.insert(instIndex, CompareFloatingPointVectorUnordered, origin, Arg::simdInfo(simdInfo), dst, tmp, dst);
                insertionSet.insert(instIndex, VectorOr, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), tmp, dst, tmp);
                SIMDLane equivalentIntegerLane = simdInfo.lane == SIMDLane::f32x4 ? SIMDLane::i32x4 : SIMDLane::i64x2;
                insertionSet.insert(instIndex, VectorUshr8, origin, Arg::simdInfo({ equivalentIntegerLane, SIMDSignMode::None }), dst, Arg::imm(simdInfo.lane == SIMDLane::f32x4 ? 10 : 13), dst);
                insertionSet.insert(instIndex, VectorAndnot, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), tmp, dst, dst);

                inst = Inst();
            };

            auto handleVectorMax = [&] {
                if (!isX86() || !scalarTypeIsFloatingPoint(inst.args[0].simdInfo().lane))
                    return;

                // Intel's vectorized maximum instruction has slightly different semantics to the WebAssembly vectorized
                // minimum instruction, namely in terms of signed zero values and propagating NaNs. VectorPmax implements
                // a fast version of this instruction that compiles down to a single op, without conforming to the exact
                // semantics. In order to precisely implement VectorMax, we need to do extra work on Intel to check for
                // the necessary edge cases.

                SIMDInfo simdInfo = inst.args[0].simdInfo();
                Tmp lhs = inst.args[1].tmp();
                Tmp rhs = inst.args[2].tmp();
                Tmp dst = inst.args[3].tmp();
                auto* origin = inst.origin;

                Tmp tmp = code.newTmp(FP);

                // Compute result in both directions.
                insertionSet.insert(instIndex, VectorPmax, origin, Arg::simdInfo(simdInfo), rhs, lhs, tmp);
                insertionSet.insert(instIndex, VectorPmax, origin, Arg::simdInfo(simdInfo), lhs, rhs, dst);

                // Check for discrepancies by XORing the two results together.
                insertionSet.insert(instIndex, VectorXor, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), tmp, dst, dst);

                // OR results, propagating the sign bit for negative zeroes, and NaNs.
                insertionSet.insert(instIndex, VectorOr, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), tmp, dst, tmp);

                // Propagate discrepancies in the sign bit.
                insertionSet.insert(instIndex, VectorSub, origin, Arg::simdInfo(simdInfo), tmp, dst, tmp);

                // Canonicalize NaNs by checking for unordered values and clearing payload if necessary.
                insertionSet.insert(instIndex, CompareFloatingPointVectorUnordered, origin, Arg::simdInfo(simdInfo), dst, tmp, dst);
                SIMDLane equivalentIntegerLane = simdInfo.lane == SIMDLane::f32x4 ? SIMDLane::i32x4 : SIMDLane::i64x2;
                insertionSet.insert(instIndex, VectorUshr8, origin, Arg::simdInfo({ equivalentIntegerLane, SIMDSignMode::None }), dst, Arg::imm(simdInfo.lane == SIMDLane::f32x4 ? 10 : 13), dst);
                insertionSet.insert(instIndex, VectorAndnot, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), tmp, dst, dst);

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

            auto handleVectorAbs = [&] {
                SIMDInfo simdInfo = inst.args[0].simdInfo();

                if (!isX86() || !scalarTypeIsFloatingPoint(simdInfo.lane))
                    return;

                // Intel doesn't have a vector absolute-value instruction for floats, so we have to manually
                // set the sign bit.

                Tmp vec = inst.args[1].tmp();
                Tmp dst = inst.args[2].tmp();
                auto* origin = inst.origin;

                Tmp fptmp = code.newTmp(FP);
                Tmp gptmp = code.newTmp(GP);

                if (simdInfo.lane == SIMDLane::f32x4) {
                    insertionSet.insert(instIndex, Move, origin, Arg::imm(0x7fffffff), gptmp);
                    insertionSet.insert(instIndex, Move32ToFloat, origin, gptmp, fptmp);
                    insertionSet.insert(instIndex, VectorSplatFloat32, origin, fptmp, fptmp);
                } else {
                    insertionSet.insert(instIndex, Move, origin, Arg::bigImm(0x7fffffffffffffff), gptmp);
                    insertionSet.insert(instIndex, Move64ToDouble, origin, gptmp, fptmp);
                    insertionSet.insert(instIndex, VectorSplatFloat64, origin, fptmp, fptmp);
                }
                insertionSet.insert(instIndex, VectorAnd, origin, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), vec, fptmp, dst);

                inst = Inst();
            };

            auto handleVectorExtaddPairwise = [&] {
                SIMDInfo simdInfo = inst.args[0].simdInfo();

                if (!isX86() || simdInfo.lane != SIMDLane::i16x8 || simdInfo.signMode != SIMDSignMode::Unsigned)
                    return;

                Tmp vec = inst.args[1].tmp();
                Tmp dst = inst.args[2].tmp();
                auto* origin = inst.origin;

                insertionSet.insert(instIndex, VectorExtaddPairwiseUnsignedInt16, origin, vec, dst, code.newTmp(FP));
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
            case VectorAbs:
                handleVectorAbs();
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
            case VectorMin:
                handleVectorMin();
                break;
            case VectorMax:
                handleVectorMax();
                break;
            case VectorExtaddPairwise:
                handleVectorExtaddPairwise();
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

