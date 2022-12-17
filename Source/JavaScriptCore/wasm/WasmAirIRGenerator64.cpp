/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#include "WasmAirIRGeneratorBase.h"
#include <cstddef>

#if USE(JSVALUE64) && ENABLE(WEBASSEMBLY_B3JIT)

namespace JSC { namespace Wasm {

////////////////////////////////////////////////////////////////////////////////
// 64-bit AirIRGenerator
////////////////////////////////////////////////////////////////////////////////

class TypedTmp {
public:
    constexpr TypedTmp()
        : m_tmp()
        , m_type(Types::Void)
    {
    }

    TypedTmp(Tmp tmp, Type type)
        : m_tmp(tmp)
        , m_type(type)
    {
    }

    TypedTmp(const TypedTmp&) = default;
    TypedTmp(TypedTmp&&) = default;
    TypedTmp& operator=(TypedTmp&&) = default;
    TypedTmp& operator=(const TypedTmp&) = default;

    bool operator==(const TypedTmp& other) const
    {
        return m_tmp == other.m_tmp && m_type == other.m_type;
    }
    bool operator!=(const TypedTmp& other) const
    {
        return !(*this == other);
    }

    explicit operator bool() const { return !!tmp(); }

    operator Tmp() const { return tmp(); }
    operator Arg() const { return Arg(tmp()); }
    Tmp tmp() const { return m_tmp; }
    Type type() const { return m_type; }

    TypedTmp coerce(Type type) const
    {
        return TypedTmp(m_tmp, type);
    }

    void dump(PrintStream& out) const
    {
        out.print("(", m_tmp, ", ", m_type.kind, ", ", m_type.index, ")");
    }

private:
    Tmp m_tmp;
    Type m_type;
};

class AirIRGenerator64 : public AirIRGeneratorBase<AirIRGenerator64, TypedTmp> {
public:
    friend AirIRGeneratorBase<AirIRGenerator64, TypedTmp>;
    using ExpressionType = TypedTmp;

    static ExpressionType emptyExpression() { return { }; };

    AirIRGenerator64(const ModuleInformation&, B3::Procedure&, InternalFunction*, Vector<UnlinkedWasmToWasmCall>&, MemoryMode, unsigned functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount*, const TypeDefinition&, unsigned& osrEntryScratchBufferSize);

    static constexpr bool tierSupportsSIMD = true;
    static constexpr bool generatesB3OriginData = true;
    static constexpr bool supportsPinnedStateRegisters = true;

    void emitMaterializeConstant(Type, uint64_t value, TypedTmp& dest);
    void emitMaterializeConstant(BasicBlock*, Type, uint64_t value, TypedTmp& dest);

    ExpressionType addConstant(Type, uint64_t);
    ExpressionType addConstant(BasicBlock*, Type, uint64_t);
    ExpressionType addConstant(v128_t);
    ExpressionType addConstantZero(Type);
    ExpressionType addConstantZero(BasicBlock*, Type);

    // This pair of operations is used when we need to call into a JIT operation with
    // some arbitrary wasm value--we need a TypedTmp with a uniform size, and all
    // wasm values can be stuffed into an i64
    //
    // `emitCoerceFromI64` is pretty unsafe--nothing checks that the source tmp makes
    // any sense as the requested type; proceed with caution.
    void emitCoerceToI64(const TypedTmp& src, TypedTmp& result);
    void emitCoerceFromI64(Type, const TypedTmp& src, TypedTmp& result);

    // References
    PartialResult WARN_UNUSED_RETURN addRefIsNull(ExpressionType value, ExpressionType& result);

    // Memory
    PartialResult WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    // GC
    PartialResult WARN_UNUSED_RETURN addI31New(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetS(ExpressionType ref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetU(ExpressionType ref, ExpressionType& result);

    // SIMD
    void notifyFunctionUsesSIMD() { ASSERT(m_info.isSIMDFunction(m_functionIndex)); }
    PartialResult WARN_UNUSED_RETURN addSIMDLoad(ExpressionType pointer, uint32_t offset, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDStore(ExpressionType value, ExpressionType pointer, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN addSIMDSplat(SIMDLane, ExpressionType scalar, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDShuffle(v128_t imm, ExpressionType a, ExpressionType b, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType v, ExpressionType shift, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadSplat(SIMDLaneOperation, ExpressionType pointer, uint32_t offset, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadLane(SIMDLaneOperation, ExpressionType pointer, ExpressionType vector, uint32_t offset, uint8_t laneIndex, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDStoreLane(SIMDLaneOperation, ExpressionType pointer, ExpressionType vector, uint32_t offset, uint8_t laneIndex);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadExtend(SIMDLaneOperation, ExpressionType pointer, uint32_t offset, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadPad(SIMDLaneOperation, ExpressionType pointer, uint32_t offset, ExpressionType& result);

    // SIMD generated

#define DEFINE_AIR_OP_FOR_SIGNED_OP(OP) \
    ALWAYS_INLINE constexpr B3::Air::Opcode airOpForSIMD##OP(SIMDInfo info) { \
        switch (info.signMode) { \
        case SIMDSignMode::Unsigned: \
            switch (info.lane) { \
                case SIMDLane::i8x16: return B3::Air::Vector##OP##UnsignedInt8; \
                case SIMDLane::i16x8: return B3::Air::Vector##OP##UnsignedInt16; \
                default: break; \
            } \
            break; \
        case SIMDSignMode::Signed: \
            switch (info.lane) { \
                case SIMDLane::i8x16: return B3::Air::Vector##OP##SignedInt8; \
                case SIMDLane::i16x8: return B3::Air::Vector##OP##SignedInt16; \
                default: break; \
            } \
            break; \
        case SIMDSignMode::None: \
            switch (info.lane) { \
                case SIMDLane::i32x4: return B3::Air::Vector##OP##Int32; \
                case SIMDLane::i64x2: return B3::Air::Vector##OP##Int64; \
                case SIMDLane::f32x4: return B3::Air::Vector##OP##Float32; \
                case SIMDLane::f64x2: return B3::Air::Vector##OP##Float64; \
                default: break; \
            } \
            break; \
        } \
        RELEASE_ASSERT_NOT_REACHED(); \
        return Oops; \
    }

#define DEFINE_AIR_OP_FOR_UNSIGNED_OP(OP) \
    ALWAYS_INLINE constexpr B3::Air::Opcode airOpForSIMD##OP(SIMDInfo info) { \
        switch (info.signMode) { \
        case SIMDSignMode::None: \
            switch (info.lane) { \
                case SIMDLane::i8x16: return B3::Air::Vector##OP##Int8; \
                case SIMDLane::i16x8: return B3::Air::Vector##OP##Int16; \
                case SIMDLane::i32x4: return B3::Air::Vector##OP##Int32; \
                case SIMDLane::i64x2: return B3::Air::Vector##OP##Int64; \
                case SIMDLane::f32x4: return B3::Air::Vector##OP##Float32; \
                case SIMDLane::f64x2: return B3::Air::Vector##OP##Float64; \
                default: break; \
            } \
            break; \
        default: break; \
        } \
        RELEASE_ASSERT_NOT_REACHED(); \
        return Oops; \
    }

#define AIR_OP_CASE(OP) \
    else if (op == SIMDLaneOperation::OP) airOp = B3::Air::Vector##OP;

#define AIR_OP_CASES() \
    B3::Air::Opcode airOp = B3::Air::Oops; \
    if (false) { }

    DEFINE_AIR_OP_FOR_SIGNED_OP(ExtractLane)
    auto addExtractLane(SIMDInfo info, uint8_t lane, ExpressionType v, ExpressionType& result) -> PartialResult
    {
        auto airOp = airOpForSIMDExtractLane(info);
        result = tmpForType(simdScalarType(info.lane));
        if (isValidForm(airOp, Arg::Imm, Arg::Tmp, Arg::Tmp)) {
            append(airOp, Arg::imm(lane), v, result);
            return { };
        }
        if (isValidForm(airOp, Arg::Imm, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp)) {
            append(airOp, Arg::imm(lane), Arg::simdInfo(info), v, result);
            return { };
        }
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }

    DEFINE_AIR_OP_FOR_UNSIGNED_OP(ReplaceLane)
    auto addReplaceLane(SIMDInfo info, uint8_t imm, ExpressionType v, ExpressionType s, ExpressionType& result) -> PartialResult
    {
        auto airOp = airOpForSIMDReplaceLane(info);
        result = tmpForType(Types::V128);
        append(MoveVector, v, result);
        append(airOp, Arg::imm(imm), s, result);
        return { };
    }

    auto addSIMDI_V(SIMDLaneOperation op, SIMDInfo info, ExpressionType v, ExpressionType& result) -> PartialResult
    {
        AIR_OP_CASES()
        AIR_OP_CASE(Bitmask)
        AIR_OP_CASE(AnyTrue)
        AIR_OP_CASE(AllTrue)
        result = tmpForType(Types::I32);
        if (isX86() && (op == SIMDLaneOperation::AllTrue || op == SIMDLaneOperation::Bitmask)) {
            append(airOp, Arg::simdInfo(info), v, result, tmpForType(Types::V128));
            return { };
        }
        if (isValidForm(airOp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, v, result);
            return { };
        }
        if (isValidForm(airOp, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp)) {
            append(airOp, Arg::simdInfo(info), v, result);
            return { };
        }
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }

    auto addSIMDV_V(SIMDLaneOperation op, SIMDInfo info, ExpressionType v, ExpressionType& result) -> PartialResult
    {
        AIR_OP_CASES()
        AIR_OP_CASE(Demote)
        AIR_OP_CASE(Promote)
        AIR_OP_CASE(Abs)
        AIR_OP_CASE(Popcnt)
        AIR_OP_CASE(Ceil)
        AIR_OP_CASE(Floor)
        AIR_OP_CASE(Trunc)
        AIR_OP_CASE(Nearest)
        AIR_OP_CASE(Sqrt)
        AIR_OP_CASE(ExtaddPairwise)
        AIR_OP_CASE(Convert)
        AIR_OP_CASE(ConvertLow)
        AIR_OP_CASE(ExtendHigh)
        AIR_OP_CASE(ExtendLow)
        AIR_OP_CASE(TruncSat)
        AIR_OP_CASE(Not)
        AIR_OP_CASE(Neg)

        result = tmpForType(Types::V128);

        if (isX86()) {
            if (airOp == B3::Air::VectorNot) {
                // x86_64 has no vector bitwise NOT instruction, so we expand vxv.not v into vxv.xor -1, v
                // here to give B3/Air a chance to optimize out repeated usage of the mask.
                v128_t mask;
                mask.u64x2[0] = 0xffffffffffffffff;
                mask.u64x2[1] = 0xffffffffffffffff;
                TypedTmp ones = addConstant(mask);
                append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), ones, v, result);
                return { };
            }

            if (airOp == B3::Air::VectorNeg) {
                // x86_64 has no vector negate instruction. For integer vectors, we can replicate negation by
                // subtracting from zero. For floating-point vectors, we need to toggle the sign using packed
                // XOR.
                switch (info.lane) {
                case SIMDLane::i8x16:
                case SIMDLane::i16x8:
                case SIMDLane::i32x4:
                case SIMDLane::i64x2: {
                    TypedTmp zero = addConstant(v128_t());
                    append(VectorSub, Arg::simdInfo(info), zero, v, result);
                    break;
                }
                case SIMDLane::f32x4: {
                    TypedTmp gptmp = tmpForType(Types::I32);
                    TypedTmp fptmp = tmpForType(Types::V128);
                    append(Move, Arg::bigImm(0x80000000), gptmp);
                    append(Move32ToFloat, gptmp, fptmp);
                    append(VectorSplatFloat32, fptmp, fptmp);
                    append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), v, fptmp, result);
                    break;
                }
                case SIMDLane::f64x2: {
                    TypedTmp gptmp = tmpForType(Types::I64);
                    TypedTmp fptmp = tmpForType(Types::V128);
                    append(Move, Arg::bigImm(0x8000000000000000), gptmp);
                    append(Move64ToDouble, gptmp, fptmp);
                    append(VectorSplatFloat64, fptmp, fptmp);
                    append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), v, fptmp, result);
                    break;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                return { };
            }

            if (airOp == B3::Air::VectorAbs && info.lane == SIMDLane::i64x2) {
                append(VectorAbsInt64, v, result, tmpForType(Types::V128));
                return { };
            }

            if (airOp == B3::Air::VectorExtaddPairwise) {
                if (info.lane == SIMDLane::i16x8 && info.signMode == SIMDSignMode::Unsigned)
                    append(VectorExtaddPairwiseUnsignedInt16, v, result, tmpForType(Types::V128));
                else
                    append(airOp, Arg::simdInfo(info), v, result, tmpForType(Types::I64), tmpForType(Types::V128));
                return { };
            }

            if (airOp == B3::Air::VectorConvert && info.signMode == SIMDSignMode::Unsigned) {
                append(VectorConvertUnsigned, v, result, tmpForType(Types::V128));
                return { };
            }

            if (airOp == B3::Air::VectorConvertLow) {
                if (info.signMode == SIMDSignMode::Signed)
                    append(VectorConvertLowSignedInt32, v, result);
                else
                    append(VectorConvertLowUnsignedInt32, v, result, tmpForType(Types::I64), tmpForType(Types::V128));
                return { };
            }

            if (airOp == B3::Air::VectorTruncSat) {
                switch (info.lane) {
                case SIMDLane::f64x2:
                    if (info.signMode == SIMDSignMode::Signed)
                        append(VectorTruncSatSignedFloat64, v, result, tmpForType(Types::I64), tmpForType(Types::V128));
                    else
                        append(VectorTruncSatUnsignedFloat64, v, result, tmpForType(Types::I64), tmpForType(Types::V128));
                    return { };
                case SIMDLane::f32x4:
                    if (info.signMode == SIMDSignMode::Signed)
                        append(airOp, Arg::simdInfo(info), v, result, tmpForType(Types::I64), tmpForType(Types::V128), tmpForType(Types::V128));
                    else
                        append(VectorTruncSatUnsignedFloat32, v, result, tmpForType(Types::I64), tmpForType(Types::V128), tmpForType(Types::V128));
                    return { };
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
        }

        if (isValidForm(airOp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, v, result);
            return { };
        }
        if (isValidForm(airOp, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp)) {
            append(airOp, Arg::simdInfo(info), v, result);
            return { };
        }
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }

    auto addSIMDBitwiseSelect(ExpressionType v1, ExpressionType v2, ExpressionType c, ExpressionType& result) -> PartialResult
    {
        auto airOp = B3::Air::VectorBitwiseSelect;
        result = tmpForType(Types::V128);
        append(MoveVector, c, result);
        append(airOp, v1, v2, result);
        return { };
    }

    auto addSIMDRelOp(SIMDLaneOperation, SIMDInfo info, ExpressionType lhs, ExpressionType rhs, Arg relOp, ExpressionType& result) -> PartialResult
    {
        AIR_OP_CASES()
        else if (scalarTypeIsFloatingPoint(info.lane))
            airOp = B3::Air::CompareFloatingPointVector;
        else if (scalarTypeIsIntegral(info.lane))
            airOp = B3::Air::CompareIntegerVector;
        result = tmpForType(Types::V128);
        if (isValidForm(airOp, Arg::DoubleCond, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, relOp, Arg::simdInfo(info), lhs, rhs, result);
            return { };
        }

        if constexpr (isX86()) {
            if (isValidForm(airOp, Arg::RelCond, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                // On Intel, the best codegen for a bitwise-complement of an integer vector is to
                // XOR with a vector of all ones. This is necessary here since Intel also doesn't
                // directly implement most relational conditions between vectors: the cases below
                // are best emitted as inversions of conditions that are supported.
                v128_t allOnes;
                allOnes.u64x2[0] = 0xffffffffffffffff;
                allOnes.u64x2[1] = 0xffffffffffffffff;
                auto scratch = tmpForType(Types::V128);

                switch (relOp.asRelationalCondition()) {
                case MacroAssembler::NotEqual:
                    append(airOp, Arg::relCond(MacroAssembler::Equal), Arg::simdInfo(info), lhs, rhs, result, scratch);
                    append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), result, addConstant(allOnes), result);
                    break;
                case MacroAssembler::Above:
                    append(airOp, Arg::relCond(MacroAssembler::BelowOrEqual), Arg::simdInfo(info), lhs, rhs, result, scratch);
                    append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), result, addConstant(allOnes), result);
                    break;
                case MacroAssembler::Below:
                    append(airOp, Arg::relCond(MacroAssembler::AboveOrEqual), Arg::simdInfo(info), lhs, rhs, result, scratch);
                    append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), result, addConstant(allOnes), result);
                    break;
                case MacroAssembler::GreaterThanOrEqual:
                    if (info.lane == SIMDLane::i64x2) {
                        append(airOp, Arg::relCond(MacroAssembler::GreaterThan), Arg::simdInfo(info), rhs, lhs, result, scratch);
                        append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), result, addConstant(allOnes), result);
                    } else
                        append(airOp, relOp, Arg::simdInfo(info), lhs, rhs, result, scratch);
                    break;
                case MacroAssembler::LessThanOrEqual:
                    if (info.lane == SIMDLane::i64x2) {
                        append(airOp, Arg::relCond(MacroAssembler::GreaterThan), Arg::simdInfo(info), lhs, rhs, result, scratch);
                        append(VectorXor, Arg::simdInfo({ SIMDLane::v128, SIMDSignMode::None }), result, addConstant(allOnes), result);
                    } else
                        append(airOp, relOp, Arg::simdInfo(info), lhs, rhs, result, scratch);
                    break;
                default:
                    append(airOp, relOp, Arg::simdInfo(info), lhs, rhs, result, scratch);
                }
                return { };
            }
        }

        if (isValidForm(airOp, Arg::RelCond, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, relOp, Arg::simdInfo(info), lhs, rhs, result);
            return { };
        }
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }

    auto addSIMDSwizzleHelperX86(ExpressionType& a, ExpressionType& b, ExpressionType& result) -> PartialResult
    {
        ASSERT(isX86() && result.type() == Types::V128);
        // Let each byte mask be 112 (0x70) then after VectorAddSat
        // each index > 15 would set the saturated index's bit 7 to 1, 
        // whose corresponding byte will be zero cleared in VectorSwizzle.
        // https://github.com/WebAssembly/simd/issues/93
        v128_t mask;
        mask.u64x2[0] = 0x7070707070707070;
        mask.u64x2[1] = 0x7070707070707070;
        auto saturatedIndexes = addConstant(mask);
        append(VectorAddSat, Arg::simdInfo(SIMDInfo { SIMDLane::i8x16, SIMDSignMode::Unsigned }), saturatedIndexes, b, saturatedIndexes);
        append(B3::Air::VectorSwizzle, a, saturatedIndexes, result);
        return { };
    }

    auto addSIMDV_VV(SIMDLaneOperation op, SIMDInfo info, ExpressionType a, ExpressionType b, ExpressionType& result) -> PartialResult
    {
        AIR_OP_CASES()
        AIR_OP_CASE(And)
        AIR_OP_CASE(Andnot)
        AIR_OP_CASE(AvgRound)
        AIR_OP_CASE(DotProduct)
        AIR_OP_CASE(Add)
        AIR_OP_CASE(Mul)
        AIR_OP_CASE(MulSat)
        AIR_OP_CASE(Sub)
        AIR_OP_CASE(Div)
        AIR_OP_CASE(Pmax)
        AIR_OP_CASE(Pmin)
        AIR_OP_CASE(Or)
        AIR_OP_CASE(Swizzle)
        AIR_OP_CASE(Xor)
        AIR_OP_CASE(Narrow)
        AIR_OP_CASE(AddSat)
        AIR_OP_CASE(SubSat)
        AIR_OP_CASE(Max)
        AIR_OP_CASE(Min)
        result = tmpForType(Types::V128);

        if (isX86() && airOp == B3::Air::VectorMulSat) {
            append(airOp, a, b, result, tmpForType(Types::I64), tmpForType(Types::V128));
            return { };
        }

        if (isX86() && airOp == B3::Air::VectorSwizzle) {
            addSIMDSwizzleHelperX86(a, b, result);
            return { };
        }

        if (isValidForm(airOp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, a, b, result);
            return { };
        }
        if (isValidForm(airOp, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, Arg::simdInfo(info), a, b, result);
            return { };
        }
        if (isValidForm(airOp, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, a, b, result, tmpForType(Types::V128));
            return { };
        }
        if (isValidForm(airOp, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp, Arg::simdInfo(info), a, b, result, tmpForType(Types::V128));
            return { };
        }
        ASSERT_NOT_REACHED();
        return { };
    }

    // Control flow
    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);

    // Calls
    std::pair<B3::PatchpointValue*, PatchpointExceptionHandle> WARN_UNUSED_RETURN emitCallPatchpoint(BasicBlock*, const TypeDefinition&, const ResultList& results, const Vector<TypedTmp>& args, Vector<ConstrainedTmp> extraArgs = { });

    PartialResult addShift(Type, B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
    PartialResult addIntegerSub(B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addFloatingPointAbs(B3::Air::Opcode, ExpressionType value, ExpressionType& result);
    PartialResult addFloatingPointBinOp(Type, B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addCompare(Type, MacroAssembler::RelationalCondition, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    // Misc. operations that require 64-bit-only patchpoints
    PartialResult WARN_UNUSED_RETURN addF64ConvertUI64(ExpressionType arg, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addF32ConvertUI64(ExpressionType arg, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI64Ctz(ExpressionType arg, ExpressionType& result);

    PartialResult WARN_UNUSED_RETURN addF64ConvertUI32(ExpressionType arg0, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI64And(ExpressionType arg0, ExpressionType arg1, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI64Eqz(ExpressionType arg0, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI64Or(ExpressionType arg0, ExpressionType arg1, ExpressionType& result);

    Tmp emitCatchImpl(CatchKind, ControlType&, unsigned exceptionIndex = 0);
    template <size_t inlineCapacity>
    PatchpointExceptionHandle preparePatchpointForExceptions(B3::PatchpointValue*, Vector<ConstrainedTmp, inlineCapacity>& args);

private:
    TypedTmp g32() { return { newTmp(B3::GP), Types::I32 }; }
    TypedTmp g64() { return { newTmp(B3::GP), Types::I64 }; }
    decltype(auto) gPtr() { return g64(); }
    TypedTmp gExternref() { return { newTmp(B3::GP), Types::Externref }; }
    TypedTmp gFuncref() { return { newTmp(B3::GP), Types::Funcref }; }
    TypedTmp gRef(Type type) { return { newTmp(B3::GP), type }; }
    TypedTmp f32() { return { newTmp(B3::FP), Types::F32 }; }
    TypedTmp f64() { return { newTmp(B3::FP), Types::F64 }; }
    TypedTmp v128() { return { newTmp(B3::FP), Types::V128 }; }

    static auto constexpr AddPtr = Add64;
    static auto constexpr MulPtr = Mul64;
    static auto constexpr UrshiftPtr = Urshift64;
    static auto constexpr LeaPtr = Lea64;
    static auto constexpr BranchTestPtr = BranchTest64;
    static auto constexpr BranchPtr = Branch64;

    static Arg extractArg(const TypedTmp& tmp) { return tmp.tmp(); }
    static Arg extractArg(const Tmp& tmp) { return Arg(tmp); }
    static Arg extractArg(const Arg& arg) { return arg; }

    Tmp extractJSValuePointer(const TypedTmp& tmp) const { return tmp.tmp(); }

    void emitZeroInitialize(ExpressionType);
    void emitZeroInitialize(BasicBlock*, ExpressionType);
    template <typename Taken>
    void emitCheckI64Zero(ExpressionType, Taken&&);
    template<typename Then>
    void emitCheckForNullReference(const ExpressionType& ref, Then&&);

    B3::Type toB3ResultType(BlockSignature);

    static B3::Air::Opcode moveOpForValueType(Type type)
    {
        switch (type.kind) {
        case TypeKind::I32:
            return Move32;
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
            return Move;
        case TypeKind::F32:
            return MoveFloat;
        case TypeKind::F64:
            return MoveDouble;
        case TypeKind::V128:
            return MoveVector;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    B3::Air::Arg materializeAddrArg(Tmp base, size_t offset, Width width)
    {
        if (Arg::isValidAddrForm(offset, width))
            return Arg::addr(base, offset);

        auto temp = g64();
        append(Move, Arg::bigImm(offset), temp);
        append(Add64, temp, base, temp);
        return Arg::addr(temp);
    }

    B3::Air::Arg materializeSimpleAddrArg(Tmp base, size_t offset)
    {
        auto temp = g64();
        append(Move, Arg::bigImm(offset), temp);
        append(Add64, temp, base, temp);
        return Arg::simpleAddr(temp);
    }

    void emitLoad(Tmp base, size_t offset, const TypedTmp& result)
    {
        emitLoad(moveOpForValueType(result.type()), toB3Type(result.type()), base, offset, result.tmp());
    }

    void emitLoad(B3::Air::Opcode op, B3::Type type, Tmp base, size_t offset, Tmp result)
    {
        append(op, materializeAddrArg(base, offset, B3::widthForType(type)), result);
    }

    void emitStore(const TypedTmp& value, Tmp base, size_t offset)
    {
        append(moveOpForValueType(value.type()), value, materializeAddrArg(base, offset, B3::widthForType(toB3Type(value.type()))));
    }

    void appendCCallArg(B3::Air::Inst& inst, const TypedTmp& tmp)
    {
        inst.args.append(tmp.tmp());
    }

    void emitMove(const TypedTmp& src, const TypedTmp& dst)
    {
        if (src == dst)
            return;
        ASSERT(isSubtype(src.type(), dst.type()));
        append(moveOpForValueType(src.type()), src, dst);
    }

    void emitMove(const ValueLocation& location, const TypedTmp& dest)
    {
        if (location.isStack())
            emitLoad(Tmp(GPRInfo::callFrameRegister), location.offsetFromFP(), dest);
        else if (location.isFPR())
            append(moveOpForValueType(dest.type()), Tmp(location.fpr()), dest);
        else
            append(moveOpForValueType(dest.type()), Tmp(location.jsr().gpr()), dest);
    }

    void emitMove(const ArgumentLocation& arg, const TypedTmp& dest)
    {
        emitMove(arg.location, dest);
    }

    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    void sanitizeAtomicResult(ExtAtomicOpType, TypedTmp source, TypedTmp dest);
    void sanitizeAtomicResult(ExtAtomicOpType, TypedTmp result);
    TypedTmp appendGeneralAtomic(ExtAtomicOpType, B3::Air::Opcode nonAtomicOpcode, B3::Commutativity, Arg input, Arg addrArg, TypedTmp result);
    TypedTmp appendStrongCAS(ExtAtomicOpType, TypedTmp expected, TypedTmp value, Arg addrArg, TypedTmp result);

    template <typename IntType>
    void emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    PartialResult addUncheckedFloatingPointTruncation(FloatingPointTruncationKind, ExpressionType arg, ExpressionType out);

    bool useSignalingMemory() const
    {
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
        return m_mode == MemoryMode::Signaling;
#else
        return false;
#endif
    }

};

AirIRGenerator64::AirIRGenerator64(const ModuleInformation& info, B3::Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, unsigned functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount* tierUp, const TypeDefinition& originalSignature, unsigned& osrEntryScratchBufferSize)
    : AirIRGeneratorBase(info, procedure, compilation, unlinkedWasmToWasmCalls, mode, functionIndex, hasExceptionHandlers, tierUp, originalSignature, osrEntryScratchBufferSize)
{
}

void AirIRGenerator64::emitZeroInitialize(ExpressionType value)
{
    emitZeroInitialize(m_currentBlock, value);
}

void AirIRGenerator64::emitZeroInitialize(BasicBlock* block, ExpressionType value)
{
    auto const type = value.type();
    switch (type.kind) {
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
        append(block, Move, Arg::imm(JSValue::encode(jsNull())), value);
        break;
    case TypeKind::I32:
    case TypeKind::I64: {
        append(block, Move, Arg::imm(0), value);
        break;
    }
    case TypeKind::F32:
    case TypeKind::F64: {
        auto temp = g64();
        // IEEE 754 "0" is just int32/64 zero.
        append(block, Move, Arg::imm(0), temp);
        append(block, type.isF32() ? Move32ToFloat : Move64ToDouble, temp, value);
        break;
    }
    case TypeKind::V128: {
        append(block, MoveZeroToVector, value);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<typename Taken>
void AirIRGenerator64::emitCheckI64Zero(ExpressionType value, Taken&& taken)
{
    emitCheck([&] {
        return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::Zero), value, value);
    }, std::forward<Taken>(taken));
}

template<typename Taken>
void AirIRGenerator64::emitCheckForNullReference(const TypedTmp& ref, Taken&& taken)
{
    auto tmpForNull = g64();
    append(Move, Arg::bigImm(JSValue::encode(jsNull())), tmpForNull);
    emitCheck([&] {
        return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::Equal), ref, tmpForNull);
    }, std::forward<Taken>(taken));
}

B3::Type AirIRGenerator64::toB3ResultType(BlockSignature returnType)
{
    auto signature = returnType->as<FunctionSignature>();

    if (signature->returnsVoid())
        return B3::Void;

    if (signature->returnCount() == 1)
        return toB3Type(signature->returnType(0));

    auto result = m_tupleMap.ensure(returnType, [&] {
        Vector<B3::Type> result;
        for (unsigned i = 0; i < signature->returnCount(); ++i) {
            Type type = signature->returnType(i);
            result.append(toB3Type(type));
        }
        return m_proc.addTuple(WTFMove(result));
    });
    return result.iterator->value;
}

void AirIRGenerator64::emitMaterializeConstant(Type type, uint64_t value, TypedTmp& dest)
{
    emitMaterializeConstant(m_currentBlock, type, value, dest);
}

void AirIRGenerator64::emitMaterializeConstant(BasicBlock* block, Type type, uint64_t value, TypedTmp& dest)
{
    switch (type.kind) {
    case TypeKind::I32:
    case TypeKind::I64:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
        append(block, Move, Arg::bigImm(value), dest);
        break;
    case TypeKind::F32:
    case TypeKind::F64: {
        auto tmp = g64();
        append(block, Move, Arg::bigImm(value), tmp);
        append(block, type.isF32() ? Move32ToFloat : Move64ToDouble, tmp, dest);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

auto AirIRGenerator64::addConstant(Type type, uint64_t value) -> ExpressionType
{
    return addConstant(m_currentBlock, type, value);
}

auto AirIRGenerator64::addConstant(BasicBlock* block, Type type, uint64_t value) -> ExpressionType
{
    auto result = tmpForType(type);
    emitMaterializeConstant(block, type, value, result);
    return result;
}

auto AirIRGenerator64::addConstant(v128_t value) -> ExpressionType
{
    auto result = tmpForType(Types::V128);

    if (!value.u64x2[0] && !value.u64x2[1])
        return addConstantZero(Types::V128);

    if (value.u64x2[0] == 0xffffffffffffffff && value.u64x2[1] == 0xffffffffffffffff) {
        if constexpr (isX86())
            append(CompareIntegerVector, Arg::relCond(MacroAssembler::RelationalCondition::Equal), Arg::simdInfo({ SIMDLane::i32x4, SIMDSignMode::None }), result, result, result, tmpForType(Types::V128));
        else
            append(CompareIntegerVector, Arg::relCond(MacroAssembler::RelationalCondition::Equal), Arg::simdInfo({ SIMDLane::i32x4, SIMDSignMode::None }), result, result, result);
        return result;
    }

    // FIXME: this is bad, we should load
    auto a = g64();
    append(Move, Arg::bigImm(value.u64x2[0]), a);
    append(MoveZeroToVector, result);
    append(VectorReplaceLaneInt64, Arg::imm(0), a, result);
    append(Move, Arg::bigImm(value.u64x2[1]), a);
    append(VectorReplaceLaneInt64, Arg::imm(1), a, result);
    return result;
}

auto AirIRGenerator64::addConstantZero(Type type) -> ExpressionType
{
    return addConstantZero(m_currentBlock, type);
}

auto AirIRGenerator64::addConstantZero(BasicBlock* block, Type type) -> ExpressionType
{
    auto result = tmpForType(type);
    emitZeroInitialize(block, result);
    return result;
}

void AirIRGenerator64::emitCoerceToI64(const TypedTmp& src, TypedTmp& result)
{
    switch (src.type().kind) {
    case TypeKind::F32: {
        auto result32 = g32();
        result = g64();
        append(MoveFloatTo32, src, result32);
        append(Move32, result32, result);
    }
        break;
    case TypeKind::F64:
        result = g64();
        append(MoveDoubleTo64, src, result);
        break;
    case TypeKind::I32:
        result = g64();
        append(Move32, src, result);
        break;
    case TypeKind::I64:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
        result = src.coerce(Types::I64);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void AirIRGenerator64::emitCoerceFromI64(Type type, const TypedTmp& src, TypedTmp& result)
{
    switch (type.kind) {
    case TypeKind::I32:
        result = g32();
        append(Move32, src, result);
        break;
    case TypeKind::F32:
        result = f32();
        append(Move32ToFloat, src, result);
        break;
    case TypeKind::F64:
        result = f64();
        append(Move64ToDouble, src, result);
        break;
    case TypeKind::I64:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
        result = src.coerce(type);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

auto AirIRGenerator64::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    ASSERT(value.tmp());
    result = tmpForType(Types::I32);
    auto tmp = g64();

    append(Move, Arg::bigImm(JSValue::encode(jsNull())), tmp);
    append(Compare64, Arg::relCond(MacroAssembler::Equal), value, tmp, result);

    return { };
}

inline AirIRGenerator64::ExpressionType AirIRGenerator64::emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOperation)
{
    ASSERT(m_memoryBaseGPR);

    auto result = g64();
    append(Move32, pointer, result);

    switch (m_mode) {
    case MemoryMode::BoundsChecking: {
        // In bound checking mode, while shared wasm memory partially relies on signal handler too, we need to perform bound checking
        // to ensure that no memory access exceeds the current memory size.
        ASSERT(m_boundsCheckingSizeGPR);
        ASSERT(sizeOfOperation + offset > offset);
        auto temp = g64();
        append(Move, Arg::bigImm(static_cast<uint64_t>(sizeOfOperation) + offset - 1), temp);
        append(Add64, result, temp);

        emitCheck([&] {
            return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), temp, Tmp(m_boundsCheckingSizeGPR));
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        break;
    }

#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    case MemoryMode::Signaling: {
        // We've virtually mapped 4GiB+redzone for this memory. Only the user-allocated pages are addressable, contiguously in range [0, current],
        // and everything above is mapped PROT_NONE. We don't need to perform any explicit bounds check in the 4GiB range because WebAssembly register
        // memory accesses are 32-bit. However WebAssembly register + offset accesses perform the addition in 64-bit which can push an access above
        // the 32-bit limit (the offset is unsigned 32-bit). The redzone will catch most small offsets, and we'll explicitly bounds check any
        // register + large offset access. We don't think this will be generated frequently.
        //
        // We could check that register + large offset doesn't exceed 4GiB+redzone since that's technically the limit we need to avoid overflowing the
        // PROT_NONE region, but it's better if we use a smaller immediate because it can codegens better. We know that anything equal to or greater
        // than the declared 'maximum' will trap, so we can compare against that number. If there was no declared 'maximum' then we still know that
        // any access equal to or greater than 4GiB will trap, no need to add the redzone.
        if (offset >= Memory::fastMappedRedzoneBytes()) {
            uint64_t maximum = m_info.memory.maximum() ? m_info.memory.maximum().bytes() : std::numeric_limits<uint32_t>::max();
            auto temp = g64();
            append(Move, Arg::bigImm(static_cast<uint64_t>(sizeOfOperation) + offset - 1), temp);
            append(Add64, result, temp);
            auto sizeMax = addConstant(Types::I64, maximum);

            emitCheck([&] {
                return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), temp, sizeMax);
            }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
            });
        }
        break;
    }
#endif
    }

    append(Add64, Tmp(m_memoryBaseGPR), result);
    return result;
}

inline TypedTmp AirIRGenerator64::emitLoadOp(LoadOpType op, ExpressionType pointer, uint32_t uoffset)
{
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    TypedTmp result;

    Arg addrArg = materializeAddrArg(pointer, offset, widthForBytes(sizeOfLoadOp(op)));

    switch (op) {
    case LoadOpType::I32Load8S: {
        result = g32();
        appendEffectful(Load8SignedExtendTo32, addrArg, result);
        break;
    }

    case LoadOpType::I64Load8S: {
        result = g64();
        appendEffectful(Load8SignedExtendTo32, addrArg, result);
        append(SignExtend32ToPtr, result, result);
        break;
    }

    case LoadOpType::I32Load8U: {
        result = g32();
        appendEffectful(Load8, addrArg, result);
        break;
    }

    case LoadOpType::I64Load8U: {
        result = g64();
        appendEffectful(Load8, addrArg, result);
        break;
    }

    case LoadOpType::I32Load16S: {
        result = g32();
        appendEffectful(Load16SignedExtendTo32, addrArg, result);
        break;
    }

    case LoadOpType::I64Load16S: {
        result = g64();
        appendEffectful(Load16SignedExtendTo32, addrArg, result);
        append(SignExtend32ToPtr, result, result);
        break;
    }

    case LoadOpType::I32Load16U: {
        result = g32();
        appendEffectful(Load16, addrArg, result);
        break;
    }

    case LoadOpType::I64Load16U: {
        result = g64();
        appendEffectful(Load16, addrArg, result);
        break;
    }

    case LoadOpType::I32Load:
        result = g32();
        appendEffectful(Move32, addrArg, result);
        break;

    case LoadOpType::I64Load32U: {
        result = g64();
        appendEffectful(Move32, addrArg, result);
        break;
    }

    case LoadOpType::I64Load32S: {
        result = g64();
        appendEffectful(Move32, addrArg, result);
        append(SignExtend32ToPtr, result, result);
        break;
    }

    case LoadOpType::I64Load: {
        result = g64();
        appendEffectful(Move, addrArg, result);
        break;
    }

    case LoadOpType::F32Load: {
        result = f32();
        appendEffectful(MoveFloat, addrArg, result);
        break;
    }

    case LoadOpType::F64Load: {
        result = f64();
        appendEffectful(MoveDouble, addrArg, result);
        break;
    }
    }

    return result;
}

auto AirIRGenerator64::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfLoadOp(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* patch = addPatchpoint(B3::Void);
        patch->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(patch, Tmp());

        // We won't reach here, so we just pick a random reg.
        switch (op) {
        case LoadOpType::I32Load8S:
        case LoadOpType::I32Load16S:
        case LoadOpType::I32Load:
        case LoadOpType::I32Load16U:
        case LoadOpType::I32Load8U:
            result = g32();
            break;
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
        case LoadOpType::I64Load16S:
        case LoadOpType::I64Load32U:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load:
        case LoadOpType::I64Load16U:
            result = g64();
            break;
        case LoadOpType::F32Load:
            result = f32();
            break;
        case LoadOpType::F64Load:
            result = f64();
            break;
        }
    } else
        result = emitLoadOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfLoadOp(op)), offset);

    return { };
}

inline void AirIRGenerator64::emitStoreOp(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    Arg addrArg = materializeAddrArg(pointer, offset, widthForBytes(sizeOfStoreOp(op)));

    switch (op) {
    case StoreOpType::I64Store8:
    case StoreOpType::I32Store8:
        append(Store8, value, addrArg);
        return;

    case StoreOpType::I64Store16:
    case StoreOpType::I32Store16:
        append(Store16, value, addrArg);
        return;

    case StoreOpType::I64Store32:
    case StoreOpType::I32Store:
        append(Move32, value, addrArg);
        return;

    case StoreOpType::I64Store:
        append(Move, value, addrArg);
        return;

    case StoreOpType::F32Store:
        append(MoveFloat, value, addrArg);
        return;

    case StoreOpType::F64Store:
        append(MoveDouble, value, addrArg);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto AirIRGenerator64::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfStoreOp(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* throwException = addPatchpoint(B3::Void);
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(throwException, Tmp());
    } else
        emitStoreOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfStoreOp(op)), value, offset);

    return { };
}

void AirIRGenerator64::sanitizeAtomicResult(ExtAtomicOpType op, TypedTmp source, TypedTmp dest)
{
    ASSERT(source.type() == dest.type());
    switch (source.type().kind) {
    case TypeKind::I64: {
        switch (accessWidth(op)) {
        case Width8:
            append(ZeroExtend8To32, source, dest);
            return;
        case Width16:
            append(ZeroExtend16To32, source, dest);
            return;
        case Width32:
            append(Move32, source, dest);
            return;
        case Width64:
            if (source == dest)
                return;
            append(Move, source, dest);
            return;
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
        return;
    }
    case TypeKind::I32:
        switch (accessWidth(op)) {
        case Width8:
            append(ZeroExtend8To32, source, dest);
            return;
        case Width16:
            append(ZeroExtend16To32, source, dest);
            return;
        case Width32:
        case Width64:
            if (source == dest)
                return;
            append(Move, source, dest);
            return;
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
        return;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void AirIRGenerator64::sanitizeAtomicResult(ExtAtomicOpType op, TypedTmp result)
{
    sanitizeAtomicResult(op, result, result);
}

TypedTmp AirIRGenerator64::appendGeneralAtomic(ExtAtomicOpType op, B3::Air::Opcode opcode, B3::Commutativity commutativity, Arg input, Arg address, TypedTmp oldValue)
{
    Width accessWidth = Wasm::accessWidth(op);

    auto newTmp = [&]() {
        if (accessWidth == Width64)
            return g64();
        return g32();
    };

    auto tmp = [&](Arg arg) -> TypedTmp {
        if (arg.isTmp())
            return TypedTmp(arg.tmp(), accessWidth == Width64 ? Types::I64 : Types::I32);
        TypedTmp result = newTmp();
        append(Move, arg, result);
        return result;
    };

    auto imm = [&](Arg arg) {
        if (arg.isImm())
            return arg;
        return Arg();
    };

    auto bitImm = [&](Arg arg) {
        if (arg.isBitImm())
            return arg;
        return Arg();
    };

    Tmp newValue = opcode == B3::Air::Nop ? tmp(input) : newTmp();

    // We need a CAS loop or a LL/SC loop. Using prepare/attempt jargon, we want:
    //
    // Block #reloop:
    //     Prepare
    //     opcode
    //     Attempt
    //   Successors: Then:#done, Else:#reloop
    // Block #done:
    //     Move oldValue, result

    auto* beginBlock = m_currentBlock;
    auto* reloopBlock = m_code.addBlock();
    auto* doneBlock = m_code.addBlock();

    append(B3::Air::Jump);
    beginBlock->setSuccessors(reloopBlock);
    m_currentBlock = reloopBlock;

    B3::Air::Opcode prepareOpcode;
    if (isX86()) {
        switch (accessWidth) {
        case Width8:
            prepareOpcode = Load8SignedExtendTo32;
            break;
        case Width16:
            prepareOpcode = Load16SignedExtendTo32;
            break;
        case Width32:
            prepareOpcode = Move32;
            break;
        case Width64:
            prepareOpcode = Move;
            break;
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
        }
    } else {
        RELEASE_ASSERT(isARM64());
        prepareOpcode = OPCODE_FOR_WIDTH(LoadLinkAcq, accessWidth);
    }
    appendEffectful(prepareOpcode, address, oldValue);

    if (opcode != B3::Air::Nop) {
        // FIXME: If we ever have to write this again, we need to find a way to share the code with
        // appendBinOp.
        // https://bugs.webkit.org/show_bug.cgi?id=169249
        if (commutativity == B3::Commutative && imm(input) && isValidForm(opcode, Arg::Imm, Arg::Tmp, Arg::Tmp))
            append(opcode, imm(input), oldValue, newValue);
        else if (imm(input) && isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Tmp))
            append(opcode, oldValue, imm(input), newValue);
        else if (commutativity == B3::Commutative && bitImm(input) && isValidForm(opcode, Arg::BitImm, Arg::Tmp, Arg::Tmp))
            append(opcode, bitImm(input), oldValue, newValue);
        else if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp))
            append(opcode, oldValue, tmp(input), newValue);
        else {
            append(Move, oldValue, newValue);
            if (imm(input) && isValidForm(opcode, Arg::Imm, Arg::Tmp))
                append(opcode, imm(input), newValue);
            else
                append(opcode, tmp(input), newValue);
        }
    }

    if (isX86()) {
#if CPU(X86) || CPU(X86_64)
        Tmp eax(X86Registers::eax);
        B3::Air::Opcode casOpcode = OPCODE_FOR_WIDTH(BranchAtomicStrongCAS, accessWidth);
        append(Move, oldValue, eax);
        appendEffectful(casOpcode, Arg::statusCond(MacroAssembler::Success), eax, newValue, address);
#endif
    } else {
        RELEASE_ASSERT(isARM64());
        TypedTmp boolResult = newTmp();
        appendEffectful(OPCODE_FOR_WIDTH(StoreCondRel, accessWidth), newValue, address, boolResult);
        append(BranchTest32, Arg::resCond(MacroAssembler::Zero), boolResult, boolResult);
    }
    reloopBlock->setSuccessors(doneBlock, reloopBlock);
    m_currentBlock = doneBlock;
    return oldValue;
}

TypedTmp AirIRGenerator64::appendStrongCAS(ExtAtomicOpType op, TypedTmp expected, TypedTmp value, Arg address, TypedTmp valueResultTmp)
{
    Width accessWidth = Wasm::accessWidth(op);

    auto newTmp = [&]() {
        if (accessWidth == Width64)
            return g64();
        return g32();
    };

    auto tmp = [&](Arg arg) -> TypedTmp {
        if (arg.isTmp())
            return TypedTmp(arg.tmp(), accessWidth == Width64 ? Types::I64 : Types::I32);
        TypedTmp result = newTmp();
        append(Move, arg, result);
        return result;
    };

    Tmp successBoolResultTmp = newTmp();

    Tmp expectedValueTmp = tmp(expected);
    Tmp newValueTmp = tmp(value);

    if (isX86()) {
#if CPU(X86) || CPU(X86_64)
        Tmp eax(X86Registers::eax);
        append(Move, expectedValueTmp, eax);
        appendEffectful(OPCODE_FOR_WIDTH(AtomicStrongCAS, accessWidth), eax, newValueTmp, address);
        append(Move, eax, valueResultTmp);
#endif
        return valueResultTmp;
    }

    if (isARM64E()) {
        append(Move, expectedValueTmp, valueResultTmp);
        appendEffectful(OPCODE_FOR_WIDTH(AtomicStrongCAS, accessWidth), valueResultTmp, newValueTmp, address);
        return valueResultTmp;
    }


    RELEASE_ASSERT(isARM64());
    // We wish to emit:
    //
    // Block #reloop:
    //     LoadLink
    //     Branch NotEqual
    //   Successors: Then:#fail, Else: #store
    // Block #store:
    //     StoreCond
    //     Xor $1, %result    <--- only if !invert
    //     Jump
    //   Successors: #done
    // Block #fail:
    //     Move $invert, %result
    //     Jump
    //   Successors: #done
    // Block #done:

    auto* reloopBlock = m_code.addBlock();
    auto* storeBlock = m_code.addBlock();
    auto* strongFailBlock = m_code.addBlock();
    auto* doneBlock = m_code.addBlock();
    auto* beginBlock = m_currentBlock;

    append(B3::Air::Jump);
    beginBlock->setSuccessors(reloopBlock);

    m_currentBlock = reloopBlock;
    appendEffectful(OPCODE_FOR_WIDTH(LoadLinkAcq, accessWidth), address, valueResultTmp);
    append(OPCODE_FOR_CANONICAL_WIDTH(Branch, accessWidth), Arg::relCond(MacroAssembler::NotEqual), valueResultTmp, expectedValueTmp);
    reloopBlock->setSuccessors(B3::Air::FrequentedBlock(strongFailBlock), storeBlock);

    m_currentBlock = storeBlock;
    appendEffectful(OPCODE_FOR_WIDTH(StoreCondRel, accessWidth), newValueTmp, address, successBoolResultTmp);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), successBoolResultTmp, successBoolResultTmp);
    storeBlock->setSuccessors(doneBlock, reloopBlock);

    m_currentBlock = strongFailBlock;
    {
        TypedTmp tmp = newTmp();
        appendEffectful(OPCODE_FOR_WIDTH(StoreCondRel, accessWidth), valueResultTmp, address, tmp);
        append(BranchTest32, Arg::resCond(MacroAssembler::Zero), tmp, tmp);
    }
    strongFailBlock->setSuccessors(B3::Air::FrequentedBlock(doneBlock), reloopBlock);

    m_currentBlock = doneBlock;
    return valueResultTmp;
}

auto AirIRGenerator64::addI31New(ExpressionType value, ExpressionType& result) -> PartialResult
{
    auto tmp1 = g32();
    result = gRef(Type { TypeKind::Ref, static_cast<TypeIndex>(TypeKind::I31ref) });

    append(Move, Arg::bigImm(0x7fffffff), tmp1);
    append(And32, tmp1, value, tmp1);
    append(Move, Arg::bigImm(JSValue::NumberTag), result);
    append(Or64, result, tmp1, result);

    return { };
}

auto AirIRGenerator64::addI31GetS(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    // Trap on null reference.
    auto tmpForNull = g64();
    append(Move, Arg::bigImm(JSValue::encode(jsNull())), tmpForNull);
    emitCheck([&] {
        return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::Equal), ref, tmpForNull);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::NullI31Get);
    });

    auto tmpForShift = g32();
    result = g32();

    append(Move, Arg::imm(1), tmpForShift);
    append(Move32, ref, result);
    addShift(Types::I32, Lshift32, result, tmpForShift, result);
    addShift(Types::I32, Rshift32, result, tmpForShift, result);

    return { };
}

auto AirIRGenerator64::addI31GetU(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    // Trap on null reference.
    auto tmpForNull = g64();
    append(Move, Arg::bigImm(JSValue::encode(jsNull())), tmpForNull);
    emitCheck([&] {
        return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::Equal), ref, tmpForNull);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::NullI31Get);
    });

    result = g32();
    append(Move32, ref, result);

    return { };
}

auto AirIRGenerator64::addSIMDLoad(ExpressionType pointer, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    result = v128();
    auto offset = fixupPointerPlusOffset(pointer, uoffset);
    Arg addrArg = materializeAddrArg(emitCheckAndPreparePointer(pointer, offset, bytesForWidth(Width128)), offset, Width128);
    appendEffectful(MoveVector, addrArg, result);

    return { };
}

auto AirIRGenerator64::addSIMDStore(ExpressionType value, ExpressionType pointer, uint32_t uoffset) -> PartialResult
{
    auto offset = fixupPointerPlusOffset(pointer, uoffset);
    Arg addrArg = materializeAddrArg(emitCheckAndPreparePointer(pointer, offset, bytesForWidth(Width128)), offset, Width128);
    appendEffectful(MoveVector, value, addrArg);

    return { };
}

auto AirIRGenerator64::addSIMDSplat(SIMDLane lane, ExpressionType scalar, ExpressionType& result) -> PartialResult
{
    B3::Air::Opcode op;

    switch (lane) {
    case SIMDLane::i8x16:
        op = VectorSplatInt8;
        break;
    case SIMDLane::i16x8:
        op = VectorSplatInt16;
        break;
    case SIMDLane::i32x4:
        op = VectorSplatInt32;
        break;
    case SIMDLane::i64x2:
        op = VectorSplatInt64;
        break;
    case SIMDLane::f32x4:
        op = VectorSplatFloat32;
        break;
    case SIMDLane::f64x2:
        op = VectorSplatFloat64;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    result = v128();
    append(op, scalar, result.tmp());
    return { };
}

auto AirIRGenerator64::addSIMDShift(SIMDLaneOperation op, SIMDInfo info, ExpressionType v, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    result = v128();
    int32_t mask = (elementByteSize(info.lane) * CHAR_BIT) - 1;

    if (isARM64()) {
        Tmp shiftAmount = newTmp(B3::GP);
        Tmp shiftVector = newTmp(B3::FP);
        append(And32, Arg::bitImm(mask), shift.tmp(), shiftAmount);
        if (op == SIMDLaneOperation::Shr) {
            // ARM64 doesn't have a version of this instruction for right shift. Instead, if the input to
            // left shift is negative, it's a right shift by the absolute value of that amount.
            append(Neg32, shiftAmount);
        }
        append(VectorSplatInt8, shiftAmount, shiftVector);
        append(info.signMode == SIMDSignMode::Signed ? VectorSshl : VectorUshl, Arg::simdInfo(info), v.tmp(), shiftVector, result.tmp());

        return { };
    } else if (isX86()) {
        Tmp shiftAmount = newTmp(B3::GP);
        Tmp shiftVector = newTmp(B3::FP);
        append(Move32, shift.tmp(), shiftAmount);
        append(And32, Arg::bitImm(mask), shift.tmp(), shiftAmount);
        append(VectorSplatInt8, shiftAmount, shiftVector);

        if (op == SIMDLaneOperation::Shl)
            append(VectorUshl, Arg::simdInfo(info), v.tmp(), shiftVector, result.tmp());
        else
            append(info.signMode == SIMDSignMode::Signed ? VectorSshr : VectorUshr, Arg::simdInfo(info), v.tmp(), shiftVector, result.tmp());
        return { };
    }

    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

auto AirIRGenerator64::addSIMDExtmul(SIMDLaneOperation op, SIMDInfo info, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(info.signMode != SIMDSignMode::None);

    result = v128();

    auto lhsTmp = newTmp(B3::FP);
    auto rhsTmp = newTmp(B3::FP);

    auto extOp = op == SIMDLaneOperation::ExtmulLow ? VectorExtendLow : VectorExtendHigh;
    append(extOp, Arg::simdInfo(info), lhs.tmp(), lhsTmp);
    append(extOp, Arg::simdInfo(info), rhs.tmp(), rhsTmp);
    append(VectorMul, Arg::simdInfo(info), lhsTmp, rhsTmp, result.tmp());

    return { };
}

auto AirIRGenerator64::addSIMDShuffle(v128_t imm, ExpressionType a, ExpressionType b, ExpressionType& result) -> PartialResult
{
    result = v128();

    if (isX86()) {
        // Store each byte (w/ index < 16) of `a` to result
        // and zero clear each byte (w/ index > 15) in result.
        auto indexes = addConstant(imm);
        addSIMDSwizzleHelperX86(a, indexes, result);

        // Store each byte (w/ index - 16 >= 0) of `b` to result2
        // and zero clear each byte (w/ index - 16 < 0) in result2.
        auto result2 = v128();
        v128_t mask;
        mask.u64x2[0] = 0x1010101010101010;
        mask.u64x2[1] = 0x1010101010101010;
        append(VectorSub, Arg::simdInfo(SIMDInfo { SIMDLane::i8x16, SIMDSignMode::None }), indexes, addConstant(mask), indexes);
        append(B3::Air::VectorSwizzle, b, indexes, result2);

        // Since each index in [0, 31], we can return result2 VectorOr result.
        append(VectorOr, Arg::simdInfo(SIMDInfo { SIMDLane::v128, SIMDSignMode::None }), result, result2, result);
        return { };
    }

    append(VectorShuffle, Arg::bigImm(imm.u64x2[0]), Arg::bigImm(imm.u64x2[1]), a, b, result);

    return { };
}

auto AirIRGenerator64::addSIMDLoadSplat(SIMDLaneOperation op, ExpressionType pointer, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    B3::Air::Opcode opcode;
    Width width;
    switch (op) {
    case SIMDLaneOperation::LoadSplat8:
        opcode = VectorLoad8Splat;
        width = Width8;
        break;
    case SIMDLaneOperation::LoadSplat16:
        opcode = VectorLoad16Splat;
        width = Width16;
        break;
    case SIMDLaneOperation::LoadSplat32:
        opcode = VectorLoad32Splat;
        width = Width32;
        break;
    case SIMDLaneOperation::LoadSplat64:
        opcode = VectorLoad64Splat;
        width = Width64;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    result = v128();

    auto offset = fixupPointerPlusOffset(pointer, uoffset);
    Arg addrArg = materializeSimpleAddrArg(emitCheckAndPreparePointer(pointer, offset, bytesForWidth(width)), offset);
    if (isX86() && op == SIMDLaneOperation::LoadSplat8)
        appendEffectful(opcode, addrArg, result, tmpForType(Types::V128));
    else
        appendEffectful(opcode, addrArg, result);

    return { };
}

auto AirIRGenerator64::addSIMDLoadLane(SIMDLaneOperation op, ExpressionType pointer, ExpressionType vector, uint32_t uoffset, uint8_t laneIndex, ExpressionType& result) -> PartialResult
{
    B3::Air::Opcode opcode;
    Width width;
    switch (op) {
    case SIMDLaneOperation::LoadLane8:
        opcode = VectorLoad8Lane;
        width = Width8;
        break;
    case SIMDLaneOperation::LoadLane16:
        opcode = VectorLoad16Lane;
        width = Width16;
        break;
    case SIMDLaneOperation::LoadLane32:
        opcode = VectorLoad32Lane;
        width = Width32;
        break;
    case SIMDLaneOperation::LoadLane64:
        opcode = VectorLoad64Lane;
        width = Width64;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    result = v128();

    auto offset = fixupPointerPlusOffset(pointer, uoffset);
    Arg addrArg = materializeSimpleAddrArg(emitCheckAndPreparePointer(pointer, offset, bytesForWidth(width)), offset);
    append(MoveVector, vector, result);
    appendEffectful(opcode, addrArg, Arg::imm(laneIndex), result);

    return { };
}

auto AirIRGenerator64::addSIMDStoreLane(SIMDLaneOperation op, ExpressionType pointer, ExpressionType vector, uint32_t uoffset, uint8_t laneIndex) -> PartialResult
{
    B3::Air::Opcode opcode;
    Width width;
    switch (op) {
    case SIMDLaneOperation::StoreLane8:
        opcode = VectorStore8Lane;
        width = Width8;
        break;
    case SIMDLaneOperation::StoreLane16:
        opcode = VectorStore16Lane;
        width = Width16;
        break;
    case SIMDLaneOperation::StoreLane32:
        opcode = VectorStore32Lane;
        width = Width32;
        break;
    case SIMDLaneOperation::StoreLane64:
        opcode = VectorStore64Lane;
        width = Width64;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    auto offset = fixupPointerPlusOffset(pointer, uoffset);
    Arg addrArg = materializeSimpleAddrArg(emitCheckAndPreparePointer(pointer, offset, bytesForWidth(width)), offset);
    appendEffectful(opcode, vector, addrArg, Arg::imm(laneIndex));

    return { };
}

auto AirIRGenerator64::addSIMDLoadExtend(SIMDLaneOperation op, ExpressionType pointer, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    SIMDLane lane;
    SIMDSignMode signMode;

    switch (op) {
    case SIMDLaneOperation::LoadExtend8U:
        lane = SIMDLane::i16x8;
        signMode = SIMDSignMode::Unsigned;
        break;
    case SIMDLaneOperation::LoadExtend8S:
        lane = SIMDLane::i16x8;
        signMode = SIMDSignMode::Signed;
        break;
    case SIMDLaneOperation::LoadExtend16U:
        lane = SIMDLane::i32x4;
        signMode = SIMDSignMode::Unsigned;
        break;
    case SIMDLaneOperation::LoadExtend16S:
        lane = SIMDLane::i32x4;
        signMode = SIMDSignMode::Signed;
        break;
    case SIMDLaneOperation::LoadExtend32U:
        lane = SIMDLane::i64x2;
        signMode = SIMDSignMode::Unsigned;
        break;
    case SIMDLaneOperation::LoadExtend32S:
        lane = SIMDLane::i64x2;
        signMode = SIMDSignMode::Signed;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    result = v128();

    auto offset = fixupPointerPlusOffset(pointer, uoffset);
    Arg addrArg = materializeAddrArg(emitCheckAndPreparePointer(pointer, offset, bytesForWidth(Width64)), offset, Width64);
    appendEffectful(MoveDouble, addrArg, result);
    append(VectorExtendLow, Arg::simdInfo({ lane, signMode }), result, result);

    return { };
}

auto AirIRGenerator64::addSIMDLoadPad(SIMDLaneOperation op, ExpressionType pointer, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    B3::Air::Opcode airOp;
    Width width;
    switch (op) {
    case SIMDLaneOperation::LoadPad32:
        width = Width32;
        airOp = MoveFloat;
        break;
    case SIMDLaneOperation::LoadPad64:
        width = Width64;
        airOp = MoveDouble;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    result = v128();

    auto offset = fixupPointerPlusOffset(pointer, uoffset);
    Arg addrArg = materializeAddrArg(emitCheckAndPreparePointer(pointer, offset, bytesForWidth(Width64)), offset, width);
    appendEffectful(airOp, addrArg, result);

    return { };
}

Tmp AirIRGenerator64::emitCatchImpl(CatchKind kind, ControlType& data, unsigned exceptionIndex)
{
    m_currentBlock = m_code.addBlock();
    m_catchEntrypoints.append(m_currentBlock);

    if (ControlType::isTry(data)) {
        if (kind == CatchKind::Catch)
            data.convertTryToCatch(++m_callSiteIndex, g64());
        else
            data.convertTryToCatchAll(++m_callSiteIndex, g64());
    }
    // We convert from "try" to "catch" ControlType above. This doesn't
    // happen if ControlType is already a "catch". This can happen when
    // we have multiple catches like "try {} catch(A){} catch(B){}...CatchAll(E){}".
    // We just convert the first ControlType to a catch, then the others will
    // use its fields.
    ASSERT(ControlType::isAnyCatch(data));

    HandlerType handlerType = kind == CatchKind::Catch ? HandlerType::Catch : HandlerType::CatchAll;
    m_exceptionHandlers.append({ handlerType, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, exceptionIndex });

    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, instanceValue(), m_currentBlock);

    unsigned indexInBuffer = 0;
    auto loadFromScratchBuffer = [&] (TypedTmp result) {
        size_t offset = sizeof(uint64_t) * indexInBuffer;
        ++indexInBuffer;
        Tmp bufferPtr = Tmp(GPRInfo::argumentGPR0);
        emitLoad(bufferPtr, offset, result);
    };
    forEachLiveValue([&] (TypedTmp tmp) {
        // We set our current ControlEntry's exception below after the patchpoint, it's
        // not in the incoming buffer of live values.
        auto toIgnore = data.exception();
        if (tmp.tmp() != toIgnore.tmp())
            loadFromScratchBuffer(tmp);
    });

    B3::PatchpointValue* patch = addPatchpoint(m_proc.addTuple({ B3::pointerType(), B3::pointerType() }));
    patch->effects.exitsSideways = true;
    patch->clobber(RegisterSetBuilder::macroClobberedRegisters());
    auto clobberLate = RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters());
    clobberLate.add(GPRInfo::argumentGPR0, IgnoreVectors);
    patch->clobberLate(clobberLate);
    patch->resultConstraints.append(B3::ValueRep::reg(GPRInfo::returnValueGPR));
    patch->resultConstraints.append(B3::ValueRep::reg(GPRInfo::returnValueGPR2));
    patch->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.move(params[2].gpr(), GPRInfo::argumentGPR0);
        CCallHelpers::Call call = jit.call(OperationPtrTag);
        jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
            linkBuffer.link<OperationPtrTag>(call, operationWasmRetrieveAndClearExceptionIfCatchable);
        });
    });

    Tmp exception = Tmp(GPRInfo::returnValueGPR);
    Tmp buffer = Tmp(GPRInfo::returnValueGPR2);
    emitPatchpoint(m_currentBlock, patch, Vector<Tmp, 8>::from(exception, buffer), Vector<ConstrainedTmp, 1>::from(instanceValue()));
    append(Move, exception, data.exception());

    return buffer;
}

auto AirIRGenerator64::addReturn(const ControlData& data, const Stack& returnValues) -> PartialResult
{
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(*data.signature(), CallRole::Callee);
    if (!wasmCallInfo.results.size()) {
        append(RetVoid);
        return { };
    }

    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    patch->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        auto calleeSaves = params.code().calleeSaveRegisterAtOffsetList();
        jit.emitRestore(calleeSaves);
        jit.emitFunctionEpilogue();
        jit.ret();
    });
    patch->effects.terminal = true;

    ASSERT(returnValues.size() >= wasmCallInfo.results.size());
    unsigned offset = returnValues.size() - wasmCallInfo.results.size();
    Vector<ConstrainedTmp, 8> returnConstraints;
    for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i) {
        B3::ValueRep rep = wasmCallInfo.results[i].location;
        TypedTmp tmp = returnValues[offset + i];

        if (rep.isStack()) {
            append(moveForType(toB3Type(tmp.type())), tmp, Arg::addr(Tmp(GPRInfo::callFrameRegister), rep.offsetFromFP()));
            continue;
        }

        ASSERT(rep.isReg());
        if (data.signature()->as<FunctionSignature>()->returnType(i).isI32())
            append(Move32, tmp, tmp);
        returnConstraints.append(ConstrainedTmp(tmp, wasmCallInfo.results[i].location));
    }

    emitPatchpoint(m_currentBlock, patch, ResultList { }, WTFMove(returnConstraints));
    return { };
}

auto AirIRGenerator64::addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&) -> PartialResult
{
    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    patch->effects.terminal = true;
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()));

    Vector<ConstrainedTmp, 8> patchArgs;
    patchArgs.append(ConstrainedTmp(instanceValue(), B3::ValueRep::reg(GPRInfo::argumentGPR0)));
    patchArgs.append(ConstrainedTmp(TypedTmp(Tmp(GPRInfo::callFrameRegister), Types::I64), B3::ValueRep::reg(GPRInfo::argumentGPR1)));
    for (unsigned i = 0; i < args.size(); ++i)
        patchArgs.append(ConstrainedTmp(args[i], B3::ValueRep::stackArgument(i * sizeof(EncodedJSValue))));

    PatchpointExceptionHandle handle = preparePatchpointForExceptions(patch, patchArgs);

    patch->setGenerator([this, exceptionIndex, handle] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        handle.generate(jit, params, this);
        emitThrowImpl(jit, exceptionIndex); 
    });

    emitPatchpoint(m_currentBlock, patch, Tmp(), WTFMove(patchArgs));

    return { };
}

auto AirIRGenerator64::addRethrow(unsigned, ControlType& data) -> PartialResult
{
    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()));
    patch->effects.terminal = true;

    Vector<ConstrainedTmp, 3> patchArgs;
    patchArgs.append(ConstrainedTmp(instanceValue(), B3::ValueRep::reg(GPRInfo::argumentGPR0)));
    patchArgs.append(ConstrainedTmp(TypedTmp(Tmp(GPRInfo::callFrameRegister), Types::I64), B3::ValueRep::reg(GPRInfo::argumentGPR1)));
    patchArgs.append(ConstrainedTmp(data.exception(), B3::ValueRep::reg(GPRInfo::argumentGPR2)));

    PatchpointExceptionHandle handle = preparePatchpointForExceptions(patch, patchArgs);
    patch->setGenerator([this, handle] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        handle.generate(jit, params, this);
        emitRethrowImpl(jit);
    });

    emitPatchpoint(m_currentBlock, patch, Tmp(), WTFMove(patchArgs));

    return { };
}

std::pair<B3::PatchpointValue*, PatchpointExceptionHandle> AirIRGenerator64::emitCallPatchpoint(BasicBlock* block, const TypeDefinition& signature, const ResultList& results, const Vector<TypedTmp>& args, Vector<ConstrainedTmp> patchArgs)
{
    auto* patchpoint = addPatchpoint(toB3ResultType(&signature));
    patchpoint->effects.writesPinned = true;
    patchpoint->effects.readsPinned = true;
    patchpoint->clobberEarly(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->clobberLate(RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()));

    CallInformation locations = wasmCallingConvention().callInformationFor(signature);
    m_code.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), locations.headerAndArgumentStackSizeInBytes));

    size_t offset = patchArgs.size();
    Checked<size_t> newSize = checkedSum<size_t>(patchArgs.size(), args.size());
    RELEASE_ASSERT(!newSize.hasOverflowed());

    patchArgs.grow(newSize);
    for (unsigned i = 0; i < args.size(); ++i)
        patchArgs[i + offset] = ConstrainedTmp(args[i], locations.params[i].location);

    if (patchpoint->type() != B3::Void) {
        Vector<B3::ValueRep, 1> resultConstraints;
        for (auto valueLocation : locations.results)
            resultConstraints.append(B3::ValueRep(valueLocation.location));
        patchpoint->resultConstraints = WTFMove(resultConstraints);
    }
    PatchpointExceptionHandle exceptionHandle = preparePatchpointForExceptions(patchpoint, patchArgs);
    emitPatchpoint(block, patchpoint, results, WTFMove(patchArgs));
    return { patchpoint, exceptionHandle };
}

template <typename IntType>
void AirIRGenerator64::emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8);

    result = sizeof(IntType) == 4 ? g32() : g64();

    bool isSigned = std::is_signed<IntType>::value;

    if (isARM64()) {
        B3::Air::Opcode div;
        switch (sizeof(IntType)) {
        case 4:
            div = isSigned ? Div32 : UDiv32;
            break;
        case 8:
            div = isSigned ? Div64 : UDiv64;
            break;
        }

        append(div, lhs, rhs, result);

        if (!isDiv) {
            append(sizeof(IntType) == 4 ? Mul32 : Mul64, result, rhs, result);
            append(sizeof(IntType) == 4 ? Sub32 : Sub64, lhs, result, result);
        }

        return;
    }

#if CPU(X86_64)
    Tmp eax(X86Registers::eax);
    Tmp edx(X86Registers::edx);

    if (isSigned) {
        B3::Air::Opcode convertToDoubleWord;
        B3::Air::Opcode div;
        switch (sizeof(IntType)) {
        case 4:
            convertToDoubleWord = X86ConvertToDoubleWord32;
            div = X86Div32;
            break;
        case 8:
            convertToDoubleWord = X86ConvertToQuadWord64;
            div = X86Div64;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        // We implement "res = Div<Chill>/Mod<Chill>(num, den)" as follows:
        //
        //     if (den + 1 <=_unsigned 1) {
        //         if (!den) {
        //             res = 0;
        //             goto done;
        //         }
        //         if (num == -2147483648) {
        //             res = isDiv ? num : 0;
        //             goto done;
        //         }
        //     }
        //     res = num (/ or %) dev;
        // done:

        BasicBlock* denIsGood = m_code.addBlock();
        BasicBlock* denMayBeBad = m_code.addBlock();
        BasicBlock* denNotZero = m_code.addBlock();
        BasicBlock* continuation = m_code.addBlock();

        auto temp = sizeof(IntType) == 4 ? g32() : g64();
        auto one = addConstant(sizeof(IntType) == 4 ? Types::I32 : Types::I64, 1);

        append(sizeof(IntType) == 4 ? Add32 : Add64, rhs, one, temp);
        append(sizeof(IntType) == 4 ? Branch32 : Branch64, Arg::relCond(MacroAssembler::Above), temp, one);
        m_currentBlock->setSuccessors(denIsGood, denMayBeBad);

        append(denMayBeBad, Xor64, result, result);
        append(denMayBeBad, sizeof(IntType) == 4 ? BranchTest32 : BranchTest64, Arg::resCond(MacroAssembler::Zero), rhs, rhs);
        denMayBeBad->setSuccessors(continuation, denNotZero);

        auto min = addConstant(denNotZero, sizeof(IntType) == 4 ? Types::I32 : Types::I64, std::numeric_limits<IntType>::min());
        if (isDiv)
            append(denNotZero, sizeof(IntType) == 4 ? Move32 : Move, min, result);
        // Otherwise, result is zero, as set above...
        append(denNotZero, sizeof(IntType) == 4 ? Branch32 : Branch64, Arg::relCond(MacroAssembler::Equal), lhs, min);
        denNotZero->setSuccessors(continuation, denIsGood);

        auto divResult = isDiv ? eax : edx;
        append(denIsGood, Move, lhs, eax);
        append(denIsGood, convertToDoubleWord, eax, edx);
        append(denIsGood, div, eax, edx, rhs);
        append(denIsGood, sizeof(IntType) == 4 ? Move32 : Move, divResult, result);
        append(denIsGood, Jump);
        denIsGood->setSuccessors(continuation);

        m_currentBlock = continuation;
        return;
    }

    B3::Air::Opcode div = sizeof(IntType) == 4 ? X86UDiv32 : X86UDiv64;

    Tmp divResult = isDiv ? eax : edx;

    append(Move, lhs, eax);
    append(Xor64, edx, edx);
    append(div, eax, edx, rhs);
    append(sizeof(IntType) == 4 ? Move32 : Move, divResult, result);
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

auto AirIRGenerator64::addShift(Type type, B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isI64() || type.isI32());
    result = tmpForType(type);

    if (isValidForm(op, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
        append(op, value, shift, result);
        return { };
    }
    
#if CPU(X86_64)
    Tmp ecx = Tmp(X86Registers::ecx);
    append(Move, value, result);
    append(Move, shift, ecx);
    append(op, ecx, result);
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif
    return { };
}

auto AirIRGenerator64::addIntegerSub(B3::Air::Opcode op, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(op == Sub32 || op == Sub64);

    result = op == Sub32 ? g32() : g64();

    if (isValidForm(op, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
        append(op, lhs, rhs, result);
        return { };
    }

    RELEASE_ASSERT(isX86());
    // Sub a, b
    // means
    // b = b Sub a
    append(Move, lhs, result);
    append(op, rhs, result);
    return { };
}

auto AirIRGenerator64::addFloatingPointAbs(B3::Air::Opcode op, ExpressionType value, ExpressionType& result) -> PartialResult
{
    RELEASE_ASSERT(op == AbsFloat || op == AbsDouble);

    result = op == AbsFloat ? f32() : f64();

    if (isValidForm(op, Arg::Tmp, Arg::Tmp)) {
        append(op, value, result);
        return { };
    }

    RELEASE_ASSERT(isX86());

    if (op == AbsFloat) {
        auto constant = g32();
        append(Move, Arg::imm(static_cast<uint32_t>(~(1ull << 31))), constant);
        append(Move32ToFloat, constant, result);
        append(AndFloat, value, result);
    } else {
        auto constant = g64();
        append(Move, Arg::bigImm(~(1ull << 63)), constant);
        append(Move64ToDouble, constant, result);
        append(AndDouble, value, result);
    }
    return { };
}

auto AirIRGenerator64::addFloatingPointBinOp(Type type, B3::Air::Opcode op, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isF32() || type.isF64());
    result = tmpForType(type);

    if (isValidForm(op, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
        append(op, lhs, rhs, result);
        return { };
    }

    RELEASE_ASSERT(isX86());

    // Op a, b
    // means
    // b = b Op a
    append(moveOpForValueType(type), lhs, result);
    append(op, rhs, result);
    return { };
}

auto AirIRGenerator64::addCompare(Type type, MacroAssembler::RelationalCondition cond, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isI32() || type.isI64());

    result = g32();

    if (type.isI32()) {
        append(Compare32, Arg::relCond(cond), lhs, rhs, result);
        return { };
    }

    append(Compare64, Arg::relCond(cond), lhs, rhs, result);
    return { };
}


template <size_t inlineCapacity>
PatchpointExceptionHandle AirIRGenerator64::preparePatchpointForExceptions(B3::PatchpointValue* patch, Vector<ConstrainedTmp, inlineCapacity>& args)
{
    ++m_callSiteIndex;
    if (!m_tryCatchDepth)
        return { m_hasExceptionHandlers };

    unsigned numLiveValues = 0;
    forEachLiveValue([&] (auto tmp) {
        ++numLiveValues;
        args.append(ConstrainedTmp(tmp, B3::ValueRep::LateColdAny));
    });

    patch->effects.exitsSideways = true;

    return { m_hasExceptionHandlers, m_callSiteIndex, numLiveValues };
}

auto AirIRGenerator64::addI64Ctz(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=](auto& jit, const B3::StackmapGenerationParams& params) {
        jit.countTrailingZeros64(params[1].gpr(), params[0].gpr());
    });
    result = g64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

auto AirIRGenerator64::addF64ConvertUI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Double);
    patchpoint->effects = B3::Effects::none();
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
#if CPU(X86_64)
        jit.convertUInt64ToDouble(params[1].gpr(), params[0].fpr(), params.gpScratch(0));
#else
        jit.convertUInt64ToDouble(params[1].gpr(), params[0].fpr());
#endif
    });
    result = f64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

auto AirIRGenerator64::addF32ConvertUI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
#if CPU(X86_64)
        jit.convertUInt64ToFloat(params[1].gpr(), params[0].fpr(), params.gpScratch(0));
#else
        jit.convertUInt64ToFloat(params[1].gpr(), params[0].fpr());
#endif
    });
    result = f32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

auto AirIRGenerator64::addUncheckedFloatingPointTruncation(FloatingPointTruncationKind kind, ExpressionType arg, ExpressionType result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(toB3Type(result.type()));
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        switch (kind) {
        case FloatingPointTruncationKind::F32ToI32:
            jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F32ToU32:
            jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F32ToI64:
            jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F32ToU64: {
            AllowMacroScratchRegisterUsageIf allowScratch(jit, isX86());
            FPRReg scratch = InvalidFPRReg;
            FPRReg constant = InvalidFPRReg;
            if (isX86()) {
                scratch = params.fpScratch(0);
                constant = params[2].fpr();
            }
            jit.truncateFloatToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
            break;
        }
        case FloatingPointTruncationKind::F64ToI32:
            jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F64ToU32:
            jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F64ToI64:
            jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F64ToU64: {
            AllowMacroScratchRegisterUsageIf allowScratch(jit, isX86());
            FPRReg scratch = InvalidFPRReg;
            FPRReg constant = InvalidFPRReg;
            if (isX86()) {
                scratch = params.fpScratch(0);
                constant = params[2].fpr();
            }
            jit.truncateDoubleToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    });

    if (isX86()) {
        switch (kind) {
        case FloatingPointTruncationKind::F32ToU64: {
            auto signBitConstant = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
            patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
            patchpoint->numFPScratchRegisters = 1;
            emitPatchpoint(
                m_currentBlock, patchpoint,
                Vector<TypedTmp, 8>::from(result),
                Vector<ConstrainedTmp, 2>::from(arg, signBitConstant)
            );
            return { };
        }
        case FloatingPointTruncationKind::F64ToU64: {
            auto signBitConstant = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
            patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
            patchpoint->numFPScratchRegisters = 1;
            emitPatchpoint(
                m_currentBlock, patchpoint,
                Vector<TypedTmp, 8>::from(result),
                Vector<ConstrainedTmp, 2>::from(arg, signBitConstant)
            );
            return { };
        }
        default:
            break;
        }
    }

    emitPatchpoint(patchpoint, result, arg);
    return { };
}

auto AirIRGenerator64::addF64ConvertUI32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    auto temp = g64();
    append(Move32, arg0, temp);
    append(ConvertInt64ToDouble, temp, result);
    return { };
}

auto AirIRGenerator64::addI64And(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(And64, arg0, arg1, result);
    return { };
}

auto AirIRGenerator64::addI64Eqz(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Test64, Arg::resCond(MacroAssembler::Zero), arg0, arg0, result);
    return { };
}

auto AirIRGenerator64::addI64Or(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Or64, arg0, arg1, result);
    return { };
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileAir(CompilationContext& compilationContext, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount* tierUp)
{
    return parseAndCompileAirImpl<AirIRGenerator64>(compilationContext, function, signature, unlinkedWasmToWasmCalls, info, mode, functionIndex, hasExceptionHandlers, tierUp);
}

} } // namespace JSC::Wasm

#endif // USE(JSVALUE64)
