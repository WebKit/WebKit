/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2022 Igalia SL. All rights reserved.
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

#if USE(JSVALUE32_64) && ENABLE(WEBASSEMBLY_B3JIT)

namespace JSC { namespace Wasm {

// This inline namespace is mostly to ensure that the canonical
// namespace of identifiers in this file is *obviously* different from
// those in the 64-bit AirIRGenerator
inline namespace Air32 {

namespace {

#if ASSERT_ENABLED

static bool typeNeedsGPPair(Type type)
{
    switch (type.kind) {
    case TypeKind::I64:
    case TypeKind::Funcref:
    case TypeKind::Externref:
    case TypeKind::RefNull:
    case TypeKind::Ref:
        return true;
    default:
        return false;
    }
}

#endif

}

struct TypedTmp {
    constexpr TypedTmp()
        : m_tmps { Tmp { }, Tmp { } }
        , m_type(Types::Void)
    {
    }

    constexpr TypedTmp(std::array<Tmp, 2> tmps, Type type)
        : m_tmps(tmps)
        , m_type(type)
    {
        ASSERT_IMPLIES(m_tmps[1], typeNeedsGPPair(type));
    }

    constexpr TypedTmp(Tmp tmp, Type type)
        : TypedTmp({ tmp, { } }, type)
    {
        ASSERT(!typeNeedsGPPair(type));
    }

    bool operator==(const TypedTmp& other) const
    {
        return m_tmps[0] == other.m_tmps[0]
            && m_tmps[1] == other.m_tmps[1]
            && m_type == other.m_type;
    }

    bool operator!=(const TypedTmp& other) const
    {
        return !(*this == other);
    }

    Type type() const { return m_type; }

    bool isGPPair() const
    {
        return static_cast<bool>(m_tmps[1]);
    }

    TypedTmp coerce(Type newType) const
    {
        return TypedTmp(m_tmps, newType);
    }

    operator Tmp() const { return tmp(); }
    operator Arg() const { return Arg(tmp()); }

    explicit operator bool() const
    {
        return static_cast<bool>(m_tmps[0]);
    }

    Tmp tmp() const
    {
        ASSERT(!isGPPair());
        return m_tmps[0];
    }

    Tmp lo() const
    {
        ASSERT(isGPPair());
        return m_tmps[0];
    }

    Tmp hi() const
    {
        ASSERT(isGPPair());
        return m_tmps[1];
    }

    void dump(PrintStream& out) const
    {
        if (isGPPair())
            out.print("({", m_tmps[0], ", ", m_tmps[1], "},", m_type.kind, ", ", m_type.index, ")");
        else
            out.print("(", m_tmps[0], ", ", m_type.kind, ", ", m_type.index, ")");
    }

private:
    std::array<Tmp, 2> m_tmps;
    Type m_type;
};

} // inline namespace Air32

class AirIRGenerator32 : public AirIRGeneratorBase<AirIRGenerator32, TypedTmp> {
public:
    friend AirIRGeneratorBase<AirIRGenerator32, TypedTmp>;
    using ExpressionType = TypedTmp;

    AirIRGenerator32(const ModuleInformation& info, B3::Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, unsigned functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount* tierUp, const TypeDefinition& originalSignature, unsigned& osrEntryScratchBufferSize)
        : AirIRGeneratorBase(info, procedure, compilation, unlinkedWasmToWasmCalls, mode, functionIndex, hasExceptionHandlers, tierUp, originalSignature, osrEntryScratchBufferSize)
    {
    }

    static constexpr bool tierSupportsSIMD = false;
    static constexpr bool generatesB3OriginData = false;
    static constexpr bool supportsPinnedStateRegisters = false;

    bool useSignalingMemory() const { return false; }

private:
    TypedTmp gNewWord(Type t) { return TypedTmp({ newTmp(B3::GP), { } }, t); }
    TypedTmp gNewPair(Type t) { return TypedTmp({ newTmp(B3::GP), newTmp(B3::GP) }, t); }
    TypedTmp g32() { return gNewWord(Types::I32); }
    TypedTmp g64() { return gNewPair(Types::I64); }
    TypedTmp gPtr() { return g32(); }
    TypedTmp gExternref() { return gNewPair(Types::Externref); }
    TypedTmp gFuncref() { return gNewPair(Types::Funcref); }
    TypedTmp gRef(Type type) { return gNewPair(type); }
    TypedTmp f32() { return TypedTmp({ newTmp(B3::FP), { } }, Types::F32 ); }
    TypedTmp f64() { return TypedTmp({ newTmp(B3::FP), { } }, Types::F64 ); }
    TypedTmp v128()
    {
        // wasm SIMD not yet supported on 32-bit
        RELEASE_ASSERT_NOT_REACHED();
    }

    static auto constexpr AddPtr = Add32;
    static auto constexpr MulPtr = Mul32;
    static auto constexpr UrshiftPtr = Urshift32;
    static auto constexpr LeaPtr = Lea32;
    static auto constexpr BranchTestPtr = BranchTest32;
    static auto constexpr BranchPtr = Branch32;

    static Arg extractArg(const TypedTmp& tmp) { return tmp.tmp(); }
    static Arg extractArg(const Tmp& tmp) { return Arg(tmp); }
    static Arg extractArg(const Arg& arg) { return arg; }

    Tmp extractJSValuePointer(const TypedTmp& tmp) const { return tmp.lo(); }

    void emitZeroInitialize(ExpressionType value);
    void emitZeroInitialize(BasicBlock*, ExpressionType value);
    template <typename Taken>
    void emitCheckI64Zero(ExpressionType value, Taken&&);
    template<typename Taken>
    void emitCheckForNullReference(const ExpressionType& ref, Taken&&);

    void appendCCallArg(B3::Air::Inst&, const TypedTmp&);

    B3::Type toB3ResultType(BlockSignature);
    static B3::Air::Opcode moveOpForValueType(Type);
    void emitLoad(Tmp base, intptr_t offset, const TypedTmp& result);
    void emitStore(const TypedTmp& value, Tmp base, intptr_t offset);
    void emitMove(const TypedTmp& src, const TypedTmp& dst);
    void emitMove(const ValueLocation&, const TypedTmp& dst);
    void emitMove(const ArgumentLocation&, const TypedTmp& dst);
    void emitMaterializeConstant(Type, uint64_t, const TypedTmp& dst);
    void emitMaterializeConstant(BasicBlock*, Type, uint64_t, const TypedTmp& dst);

    // This pair of operations is used when we need to call into a JIT operation with
    // some arbitrary wasm value--we need a TypedTmp with a uniform size, and all
    // wasm values can be stuffed into an i64
    //
    // `emitCoerceFromI64` is pretty unsafe--nothing checks that the source tmp makes
    // any sense as the requested type; proceed with caution.
    void emitCoerceToI64(const TypedTmp& src, TypedTmp& result);
    void emitCoerceFromI64(Type, const TypedTmp& src, TypedTmp& result);

    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);
    Tmp emitCatchImpl(CatchKind, ControlType&, unsigned exceptionIndex = 0);

    void sanitizeAtomicResult(ExtAtomicOpType, ExpressionType source, ExpressionType dest);
    void sanitizeAtomicResult(ExtAtomicOpType, ExpressionType result);
    template<typename InputType>
    TypedTmp appendGeneralAtomic(ExtAtomicOpType, B3::Air::Opcode, B3::Commutativity, InputType, Arg address, TypedTmp oldValue);
    TypedTmp appendStrongCAS(ExtAtomicOpType, TypedTmp expected, TypedTmp value, Arg addrArg, TypedTmp result);

    template<size_t inlineCapacity>
    PatchpointExceptionHandle preparePatchpointForExceptions(B3::PatchpointValue*, Vector<ConstrainedTmp, inlineCapacity>& args);

public:
    ExpressionType addConstant(Type, uint64_t);
    ExpressionType addConstant(v128_t) { RELEASE_ASSERT_NOT_REACHED(); }
    ExpressionType addConstant(BasicBlock*, Type, uint64_t);
    ExpressionType addConstantZero(Type);
    ExpressionType addConstantZero(BasicBlock*, Type);
    static ExpressionType emptyExpression() { return { }; }

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
    void notifyFunctionUsesSIMD()
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    // Control flow
    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);

    // Calls
    CallPatchpointData WARN_UNUSED_RETURN emitCallPatchpoint(BasicBlock*, B3::Type, const ResultList&, const Vector<TypedTmp>& tmpArgs, const CallInformation&, Vector<ConstrainedTmp> patchArgs = { });
    CallPatchpointData WARN_UNUSED_RETURN emitTailCallPatchpoint(BasicBlock*, const Checked<int32_t>& tailCallStackOffsetFromFP, const Vector<ArgumentLocation>&, const Vector<TypedTmp>& tmpArgs, Vector<ConstrainedTmp> patchArgs = { });

    PartialResult addShift(Type, B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
    PartialResult addShift64(B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
    PartialResult addRotate64(B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
    PartialResult addIntegerSub(B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addFloatingPointAbs(B3::Air::Opcode, ExpressionType value, ExpressionType& result);
    PartialResult addFloatingPointBinOp(Type, B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addCompare(Type, MacroAssembler::RelationalCondition, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    template<typename IntType>
    void emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    PartialResult addUncheckedFloatingPointTruncation(FloatingPointTruncationKind, ExpressionType arg, ExpressionType out);

    // Other operations with special 32-bit implementation

    PartialResult addI64Add(ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addI64Mul(ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addI64Eqz(ExpressionType arg, ExpressionType& result);
    PartialResult addI64And(ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addI64Or(ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addI64Xor(ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addI64Clz(ExpressionType arg, ExpressionType& result);
    PartialResult addI64Ctz(ExpressionType arg, ExpressionType& result);
    PartialResult addI64Extend8S(ExpressionType arg, ExpressionType& result);
    PartialResult addI64Extend16S(ExpressionType arg, ExpressionType& result);
    PartialResult addI64Extend32S(ExpressionType arg, ExpressionType& result);
    PartialResult addI64ExtendUI32(ExpressionType arg, ExpressionType& result);
    PartialResult addI64ExtendSI32(ExpressionType arg, ExpressionType& result);
    PartialResult addI32WrapI64(ExpressionType arg, ExpressionType& result);
    PartialResult addF64ConvertUI64(ExpressionType arg, ExpressionType& result);
    PartialResult addF32ConvertUI64(ExpressionType arg, ExpressionType& result);
    PartialResult addF64ConvertSI64(ExpressionType arg, ExpressionType& result);
    PartialResult addF32ConvertSI64(ExpressionType arg, ExpressionType& result);
    PartialResult addF32ConvertUI32(ExpressionType arg, ExpressionType& result);
    PartialResult addF64ConvertUI32(ExpressionType arg, ExpressionType& result);
    PartialResult addI64ReinterpretF64(ExpressionType arg, ExpressionType& result);
    PartialResult addF64ReinterpretI64(ExpressionType arg, ExpressionType& result);
    PartialResult addF32Nearest(ExpressionType arg, ExpressionType& result);
    PartialResult addF64Nearest(ExpressionType arg, ExpressionType& result);
    PartialResult addF64Copysign(ExpressionType arg0, ExpressionType arg1, ExpressionType& result);
};

B3::Air::Opcode AirIRGenerator32::moveOpForValueType(Type type)
{
    switch (type.kind) {
    case TypeKind::I32:
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
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void AirIRGenerator32::emitZeroInitialize(ExpressionType value)
{
    emitZeroInitialize(m_currentBlock, value);
}

void AirIRGenerator32::emitZeroInitialize(BasicBlock* block, ExpressionType value)
{
    const auto type = value.type();
    switch (type.kind) {
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull: {
        auto const immValue = JSValue::encode(jsNull());
        append(block, Move, Arg::bigImmLo32(immValue), value.lo());
        append(block, Move, Arg::bigImmHi32(immValue), value.hi());
        break;
    }
    case TypeKind::I32:
        append(block, Move, Arg::imm(0), value);
        break;
    case TypeKind::I64:
        append(block, Move, Arg::imm(0), value.lo());
        append(block, Move, Arg::imm(0), value.hi());
        break;
    case TypeKind::F32: {
        auto temp = g32();
        // IEEE 754 "0" is just int32/64 zero.
        append(block, Move, Arg::imm(0), temp);
        append(block, Move32ToFloat, temp, value);
        break;
    }
    case TypeKind::F64: {
#if CPU(ARM_THUMB2)
        TypedTmp zeroPtr = gPtr();
        static double zeroConstant = 0.;
        append(block, Move, Arg::immPtr(&zeroConstant), zeroPtr);
        append(block, MoveDouble, Arg::addr(zeroPtr), value);
#else
        append(block, MoveZeroToDouble, value);
#endif
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

}

template<typename Taken>
void AirIRGenerator32::emitCheckI64Zero(ExpressionType value, Taken&& taken)
{
    BasicBlock* continuation = nullptr;
    emitCheck([&] {
        BasicBlock* checkLo = m_code.addBlock();
        continuation = m_code.addBlock();
        append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), value.hi(), value.hi());
        m_currentBlock->setSuccessors(continuation, checkLo);
        m_currentBlock = checkLo;
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), value.lo(), value.lo());
    }, std::forward<Taken>(taken));

    ASSERT(continuation);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = continuation;
}

template<typename Taken>
void AirIRGenerator32::emitCheckForNullReference(const ExpressionType& ref, Taken&& taken)
{
    auto tmpForNullTag = g32();
    append(Move, Arg::bigImm(JSValue::NullTag), tmpForNullTag);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::Equal), ref.hi(), tmpForNullTag);
    }, std::forward<Taken>(taken));
}

B3::Type AirIRGenerator32::toB3ResultType(BlockSignature returnType)
{
    auto signature = returnType->as<FunctionSignature>();

    if (signature->returnsVoid())
        return B3::Void;

    // On 32-bit platforms, 64-bit integer types are returned as two 32-bit values
    if (signature->returnCount() == 1 && !signature->returnType(0).isGP64())
        return toB3Type(signature->returnType(0));

    auto result = m_tupleMap.ensure(returnType, [&] {
        Vector<B3::Type> result;
        for (unsigned i = 0; i < signature->returnCount(); ++i) {
            Type type = signature->returnType(i);
            if (type.isGP64()) {
                result.append(B3::Int32);
                result.append(B3::Int32);
                continue;
            }
            result.append(toB3Type(type));
        }
        return m_proc.addTuple(WTFMove(result));
    });
    return result.iterator->value;
}

void AirIRGenerator32::emitLoad(Tmp base, intptr_t offset, const TypedTmp& result)
{
    auto const largestOffsetUsed = result.isGPPair() ? offset + 4 : offset;
    if (!Arg::isValidAddrForm(moveForType(toB3Type(result.type())), largestOffsetUsed, B3::widthForType(toB3Type(result.type())))) {
        auto address = gPtr();
        append(Move, Arg::bigImm(offset), address);
        append(Add32, base, address, address);
        base = address.tmp();
        offset = 0;
    }

    if (result.isGPPair()) {
        append(Move, Arg::addr(base, offset), result.lo());
        append(Move, Arg::addr(base, offset + 4), result.hi());
    } else
        append(moveOpForValueType(result.type()), Arg::addr(base, offset), result);
}

void AirIRGenerator32::emitStore(const TypedTmp& value, Tmp base, intptr_t offset)
{
    auto const largestOffsetUsed = value.isGPPair() ? offset + 4 : offset;
    if (!Arg::isValidAddrForm(moveForType(toB3Type(value.type())), largestOffsetUsed, B3::widthForType(toB3Type(value.type())))) {
        auto address = gPtr();
        append(Move, Arg::bigImm(offset), address);
        append(Add32, base, address, address);
        base = address.tmp();
        offset = 0;
    }

    if (value.isGPPair()) {
        append(Move, value.lo(), Arg::addr(base, offset));
        append(Move, value.hi(), Arg::addr(base, offset + 4));
    } else
        append(moveOpForValueType(value.type()), value, Arg::addr(base, offset));
}

void AirIRGenerator32::emitMove(const TypedTmp& src, const TypedTmp& dst)
{
    if (src == dst)
        return;
    ASSERT(isSubtype(src.type(), dst.type()));
    if (src.isGPPair()) {
        append(Move, src.lo(), dst.lo());
        append(Move, src.hi(), dst.hi());
    } else
        append(moveOpForValueType(src.type()), src, dst);
}

void AirIRGenerator32::emitMove(const ValueLocation& location, const TypedTmp& dst)
{
    if (location.isStack())
        emitLoad(Tmp(GPRInfo::callFrameRegister), location.offsetFromFP(), dst);
    else if (location.isFPR())
        append(moveOpForValueType(dst.type()), Tmp(location.fpr()), dst);
    else if (!dst.isGPPair())
        append(Move, Tmp(location.jsr().payloadGPR()), dst);
    else {
        append(Move, Tmp(location.jsr().payloadGPR()), dst.lo());
        append(Move, Tmp(location.jsr().tagGPR()), dst.hi());
    }
}

void AirIRGenerator32::emitMove(const ArgumentLocation& arg, const TypedTmp& dst)
{
    emitMove(arg.location, dst);
}

void AirIRGenerator32::appendCCallArg(B3::Air::Inst& inst, const TypedTmp& tmp)
{
    if (tmp.isGPPair()) {
        inst.args.append(tmp.lo());
        inst.args.append(tmp.hi());
    } else
        inst.args.append(tmp.tmp());
}

auto AirIRGenerator32::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    ASSERT(value);
    result = tmpForType(Types::I32);
    auto tmp = g32();

    append(Move, Arg::bigImm(JSValue::NullTag), tmp);
    append(Compare32, Arg::relCond(MacroAssembler::Equal), value.hi(), tmp, result);

    return { };
}

auto AirIRGenerator32::emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOperation) -> ExpressionType
{
    auto memoryBase = gPtr();
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfCachedMemory()), memoryBase);

    auto result = gPtr();
    append(Move32, pointer, result);

    switch (m_mode) {
    case MemoryMode::BoundsChecking: {
        // In bound checking mode, while shared wasm memory partially relies on signal handler too, we need to perform bound checking
        // to ensure that no memory access exceeds the current memory size.
        auto boundsCheckingSize = gPtr();
        append(Move, Arg::addr(instanceValue(), Instance::offsetOfCachedBoundsCheckingSize()), boundsCheckingSize);
        ASSERT(sizeOfOperation + offset > offset);

        auto temp = gPtr();
        if (offset) {
            append(Move, Arg::bigImm(offset), temp);
            append(AddPtr, result, temp);
            emitCheck([&] {
                return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::Below), temp, result);
            }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
            });
        }
        if (sizeOfOperation > 1) {
            append(AddPtr, Arg::imm(sizeOfOperation - 1), offset ? temp : result , temp);
            emitCheck([&] {
                return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::Below), temp, result);
            }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
            });
        }
        append(Move, Arg::bigImm(static_cast<uint64_t>(offset) + sizeOfOperation - 1), temp);
        append(AddPtr, result, temp);
        emitCheck([&] {
            return Inst(BranchPtr, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), temp, boundsCheckingSize);
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        break;
    }
    }

    append(AddPtr, memoryBase, result);
    return result;
}
inline bool isFPLoadOp(LoadOpType op)
{
    switch (op) {
    case LoadOpType::F32Load:
    case LoadOpType::F64Load:
        return true;
    default:
        return false;
    }
}
TypedTmp AirIRGenerator32::emitLoadOp(LoadOpType op, ExpressionType pointer, uint32_t uoffset)
{
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    TypedTmp immTmp;
    TypedTmp newPtr;
    TypedTmp result;

    auto getAddr = [&](uint32_t offset) {
        JSC::B3::Air::Opcode opcode = Move;
#if CPU(ARM_THUMB2)
        if (isFPLoadOp(op))
            opcode = MoveDouble;
#endif
        if (Arg::isValidAddrForm(opcode, offset, widthForBytes(sizeOfLoadOp(op))))
            return Arg::addr(pointer, offset);
        immTmp = gPtr();
        newPtr = gPtr();
        append(Move, Arg::bigImm(offset), immTmp);
        append(AddPtr, immTmp, pointer, newPtr);
        return Arg::addr(newPtr);
    };

    Arg addrArg = getAddr(offset);

    switch (op) {
    case LoadOpType::I32Load8S: {
        result = g32();
        appendEffectful(Load8SignedExtendTo32, addrArg, result);
        break;
    }

    case LoadOpType::I64Load8S: {
        result = g64();
        appendEffectful(Load8SignedExtendTo32, addrArg, result.lo());
        append(Rshift32, result.lo(), Arg::imm(31), result.hi());
        break;
    }

    case LoadOpType::I32Load8U: {
        result = g32();
        appendEffectful(Load8, addrArg, result);
        break;
    }

    case LoadOpType::I64Load8U: {
        result = g64();
        appendEffectful(Load8, addrArg, result.lo());
        append(Move, Arg::imm(0), result.hi());
        break;
    }

    case LoadOpType::I32Load16S: {
        result = g32();
        appendEffectful(Load16SignedExtendTo32, addrArg, result);
        break;
    }

    case LoadOpType::I64Load16S: {
        result = g64();
        appendEffectful(Load16SignedExtendTo32, addrArg, result.lo());
        append(Rshift32, result.lo(), Arg::imm(31), result.hi());
        break;
    }

    case LoadOpType::I32Load16U: {
        result = g32();
        appendEffectful(Load16, addrArg, result);
        break;
    }

    case LoadOpType::I64Load16U: {
        result = g64();
        appendEffectful(Load16, addrArg, result.lo());
        append(Move, Arg::imm(0), result.hi());
        break;
    }

    case LoadOpType::I32Load:
        result = g32();
        appendEffectful(Move32, addrArg, result);
        break;

    case LoadOpType::I64Load32U: {
        result = g64();
        appendEffectful(Move32, addrArg, result.lo());
        append(Move, Arg::imm(0), result.hi());
        break;
    }

    case LoadOpType::I64Load32S: {
        result = g64();
        appendEffectful(Move32, addrArg, result.lo());
        append(Rshift32, result.lo(), Arg::imm(31), result.hi());
        break;
    }

    case LoadOpType::I64Load: {
        result = g64();
        // Might be unaligned so can't use LoadPair32
        appendEffectful(Move, addrArg, result.lo());
        appendEffectful(Move, getAddr(offset + 4), result.hi());
        break;
    }

    case LoadOpType::F32Load: {
        result = f32();
#if !CPU(ARM_THUMB2)
        appendEffectful(MoveFloat, addrArg, result);
#else
        // Might be unaligned so can't use MoveFloat
        auto tmp = g32();
        appendEffectful(Move, addrArg, tmp);
        append(Move32ToFloat, tmp, result);
#endif
        break;
    }

    case LoadOpType::F64Load: {
        result = f64();
#if !CPU(ARM_THUMB2)
        appendEffectful(MoveDouble, addrArg, result);
#else
        // Might be unaligned so can't use MoveDouble
        auto tmp = g64();
        appendEffectful(Move, addrArg, tmp.lo());
        appendEffectful(Move, getAddr(offset + 4), tmp.hi());
        append(Move64ToDouble, tmp.hi(), tmp.lo(), result);
#endif
        break;
    }
    }

    return result;
}

auto AirIRGenerator32::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
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

inline bool isFPStoreOp(StoreOpType op)
{
    switch (op) {
    case StoreOpType::F32Store:
    case StoreOpType::F64Store:
        return true;
    default:
        return false;
    }
}

inline void AirIRGenerator32::emitStoreOp(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    TypedTmp immTmp;
    TypedTmp newPtr;

    auto const getAddr = [&](uint32_t offset) {
        JSC::B3::Air::Opcode opcode = Move;
#if CPU(ARM_THUMB2)
        if (isFPStoreOp(op))
            opcode = MoveDouble;
#endif
        if (Arg::isValidAddrForm(opcode, offset, widthForBytes(sizeOfStoreOp(op))))
            return Arg::addr(pointer, offset);
        immTmp = gPtr();
        newPtr = gPtr();
        append(Move, Arg::bigImm(offset), immTmp);
        append(AddPtr, immTmp, pointer, newPtr);
        return Arg::addr(newPtr);
    };

    auto const addrArg = getAddr(offset);

    switch (op) {
    case StoreOpType::I64Store8:
        append(Store8, value.lo(), addrArg);
        return;
    case StoreOpType::I32Store8:
        append(Store8, value, addrArg);
        return;
    case StoreOpType::I64Store16:
        append(Store16, value.lo(), addrArg);
        return;
    case StoreOpType::I32Store16:
        append(Store16, value, addrArg);
        return;
    case StoreOpType::I64Store32:
        append(Move32, value.lo(), addrArg);
        return;
    case StoreOpType::I32Store:
        append(Move32, value, addrArg);
        return;
    case StoreOpType::I64Store:
        append(Move, value.lo(), addrArg);
        append(Move, value.hi(), getAddr(offset + 4));
        return;
    case StoreOpType::F32Store: {
#if !CPU(ARM_THUMB2)
        append(MoveFloat, value, addrArg);
#else
        // Might be unaligned so can't use MoveFloat
        auto tmp = g32();
        append(MoveFloatTo32, value, tmp);
        append(Move, tmp, addrArg);
#endif
        return;
    }
    case StoreOpType::F64Store: {
#if !CPU(ARM_THUMB2)
        append(MoveDouble, value, addrArg);
#else
        // Might be unaligned so can't use MoveDouble
        auto tmp = g64();
        append(MoveDoubleTo64, value, tmp.hi(), tmp.lo());
        appendEffectful(Move, tmp.lo(), addrArg);
        appendEffectful(Move, tmp.hi(), getAddr(offset + 4));
#endif
        return;
    }
    }
        RELEASE_ASSERT_NOT_REACHED();
}

auto AirIRGenerator32::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfStoreOp(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* throwException = addPatchpoint(B3::Void);
        throwException->setGenerator([this](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(throwException, Tmp());
    } else
        emitStoreOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfStoreOp(op)), value, offset);

    return { };
}

template<size_t inlineCapacity>
PatchpointExceptionHandle AirIRGenerator32::preparePatchpointForExceptions(B3::PatchpointValue* patch, Vector<ConstrainedTmp, inlineCapacity>& args)
{
    ++m_callSiteIndex;
    if (!m_tryCatchDepth)
        return { m_hasExceptionHandlers };

    unsigned numLiveValues = 0;
    forEachLiveValue([&](TypedTmp tmp) {
        ++numLiveValues;
        if (tmp.isGPPair()) {
            ++numLiveValues;
            args.append(ConstrainedTmp(TypedTmp(tmp.lo(), Types::I32), B3::ValueRep::LateColdAny));
            args.append(ConstrainedTmp(TypedTmp(tmp.hi(), Types::I32), B3::ValueRep::LateColdAny));
            return;
        }
        args.append(ConstrainedTmp(tmp, B3::ValueRep::LateColdAny));
    });

    patch->effects.exitsSideways = true;

    return { m_hasExceptionHandlers, m_callSiteIndex, numLiveValues };
}

Tmp AirIRGenerator32::emitCatchImpl(CatchKind kind, ControlType& data, unsigned exceptionIndex)
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

    restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_currentBlock);

    unsigned indexInBuffer = 0;
    auto loadFromScratchBuffer = [&] (TypedTmp result) {
        Tmp bufferPtr = Tmp(GPRInfo::argumentGPR0);
        if (result.isGPPair()) {
            emitLoad(bufferPtr, sizeof(uint64_t) * indexInBuffer++, TypedTmp(result.lo(), Types::I32));
            emitLoad(bufferPtr, sizeof(uint64_t) * indexInBuffer++, TypedTmp(result.hi(), Types::I32));
            return;
        }
        emitLoad(bufferPtr, sizeof(uint64_t) * indexInBuffer++, result);
        return;
    };
    forEachLiveValue([&] (TypedTmp tmp) {
        // We set our current ControlEntry's exception below after the patchpoint, it's
        // not in the incoming buffer of live values.
        if (tmp != data.exception())
            loadFromScratchBuffer(tmp);
    });

    Vector<B3::Type> resultTypes { B3::pointerType(), B3::Int32, B3::Int32 };
    B3::PatchpointValue* patch = addPatchpoint(m_proc.addTuple(WTFMove(resultTypes)));
    patch->effects.exitsSideways = true;
    patch->clobber(RegisterSetBuilder::macroClobberedRegisters());
    auto clobberLate = RegisterSetBuilder::registersToSaveForCCall(RegisterSetBuilder::allScalarRegisters());
    clobberLate.add(GPRInfo::argumentGPR0, IgnoreVectors);
    clobberLate.add(GPRInfo::argumentGPR1, IgnoreVectors);
    patch->clobberLate(clobberLate);
    patch->resultConstraints.append(B3::ValueRep::reg(GPRInfo::returnValueGPR));
    patch->resultConstraints.append(B3::ValueRep::SomeRegister);
    patch->resultConstraints.append(B3::ValueRep::SomeRegister); // result Tag
    patch->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        // Returning one EncodedJSValue on the stack
        constexpr int32_t resultSpace = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(static_cast<int32_t>(sizeof(EncodedJSValue)));
        jit.subPtr(CCallHelpers::TrustedImm32(resultSpace), MacroAssembler::stackPointerRegister);
        jit.move(params[3].gpr(), GPRInfo::argumentGPR0);
        jit.move(MacroAssembler::stackPointerRegister, GPRInfo::argumentGPR1);
        CCallHelpers::Call call = jit.call(OperationPtrTag);
        jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
            linkBuffer.link<OperationPtrTag>(call, operationWasmRetrieveAndClearExceptionIfCatchable);
        });
        jit.load32(CCallHelpers::Address(MacroAssembler::stackPointerRegister, TagOffset), params[2].gpr());
        jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister, PayloadOffset), params[1].gpr());
        jit.addPtr(CCallHelpers::TrustedImm32(resultSpace), MacroAssembler::stackPointerRegister);
    });
    TypedTmp buffer = TypedTmp(Tmp(GPRInfo::returnValueGPR), is64Bit() ? Types::I64 : Types::I32);
    TypedTmp exceptionLo = TypedTmp(data.exception().lo(), Types::I32);
    TypedTmp exceptionHi = TypedTmp(data.exception().hi(), Types::I32);
    emitPatchpoint(m_currentBlock, patch, Vector<TypedTmp, 8>::from(buffer, exceptionLo, exceptionHi), Vector<ConstrainedTmp, 1>::from(instanceValue()));
    return buffer;
}

void AirIRGenerator32::emitMaterializeConstant(Type type, uint64_t value, const TypedTmp& dest)
{
    emitMaterializeConstant(m_currentBlock, type, value, dest);
}

void AirIRGenerator32::emitMaterializeConstant(BasicBlock* block, Type type, uint64_t value, const TypedTmp& dest)
{
    switch (type.kind) {
    case TypeKind::I32:
        append(block, Move, Arg::bigImm(value), dest);
        break;
    case TypeKind::I64:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
        append(block, Move, Arg::bigImmLo32(value), dest.lo());
        append(block, Move, Arg::bigImmHi32(value), dest.hi());
        break;
    case TypeKind::F32: {
        auto tmp = g32();
        append(block, Move, Arg::bigImm(value), tmp);
        append(block, Move32ToFloat, tmp, dest);
        break;
    }
    case TypeKind::F64: {
        auto tmp = g64();
        append(block, Move, Arg::bigImmLo32(value), tmp.lo());
        append(block, Move, Arg::bigImmHi32(value), tmp.hi());
        append(block, Move64ToDouble, tmp.hi(), tmp.lo(), dest);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

auto AirIRGenerator32::addConstant(Type type, uint64_t value) -> ExpressionType
{
    return addConstant(m_currentBlock, type, value);
}

auto AirIRGenerator32::addConstant(BasicBlock* block, Type type, uint64_t value) -> ExpressionType
{
    auto result = tmpForType(type);
    emitMaterializeConstant(block, type, value, result);
    return result;
}

auto AirIRGenerator32::addConstantZero(Type type) -> ExpressionType
{
    return addConstantZero(m_currentBlock, type);
}

auto AirIRGenerator32::addConstantZero(BasicBlock* block, Type type) -> ExpressionType
{
    auto result = tmpForType(type);
    emitZeroInitialize(block, result);
    return result;
}

void AirIRGenerator32::emitCoerceToI64(const TypedTmp& src, TypedTmp& result)
{
    result = g64();
    switch (src.type().kind) {
    case TypeKind::F32:
        append(MoveFloatTo32, src, result.lo());
        append(Move, Arg::imm(0), result.hi());
        break;
    case TypeKind::F64:
        append(MoveDoubleTo64, src, result.hi(), result.lo());
        break;
    case TypeKind::I32:
        append(Move, src, result.lo());
        append(Move, Arg::imm(0), result.hi());
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

void AirIRGenerator32::emitCoerceFromI64(Type type, const TypedTmp& src, TypedTmp& result)
{
    switch (type.kind) {
    case TypeKind::F32:
        result = f32();
        append(Move32ToFloat, src.lo(), result);
        break;
    case TypeKind::F64:
        result = f64();
        append(Move64ToDouble, src.hi(), src.lo(), result);
        break;
    case TypeKind::I32:
        result = TypedTmp(src.lo(), Types::I32);
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


auto AirIRGenerator32::addReturn(const ControlData& data, const Stack& returnValues) -> PartialResult
{
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(*data.signature(), CallRole::Callee);
    if (!wasmCallInfo.results.size()) {
        append(RetVoid);
        return { };
    }

    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    patch->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        params.code().emitEpilogue(jit);
    });
    patch->effects.terminal = true;

    ASSERT(returnValues.size() >= wasmCallInfo.results.size());
    unsigned offset = returnValues.size() - wasmCallInfo.results.size();
    Vector<ConstrainedTmp, 8> returnConstraints;
    for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i) {
        B3::ValueRep rep = wasmCallInfo.results[i].location;
        TypedTmp tmp = returnValues[offset + i];

        if (rep.isStack()) {
            emitStore(tmp, Tmp(GPRInfo::callFrameRegister), rep.offsetFromFP());
            continue;
        }

        ASSERT(rep.isReg());
        if (tmp.isGPPair()) {
            JSValueRegs jsr = wasmCallInfo.results[i].location.jsr();
            returnConstraints.append(ConstrainedTmp(TypedTmp(tmp.lo(), Types::I32), B3::ValueRep { jsr.payloadGPR() }));
            returnConstraints.append(ConstrainedTmp(TypedTmp(tmp.hi(), Types::I32), B3::ValueRep { jsr.tagGPR() }));
        } else
            returnConstraints.append(ConstrainedTmp(tmp, rep));
    }

    emitPatchpoint(m_currentBlock, patch, ResultList { }, WTFMove(returnConstraints));
    return { };
}

auto AirIRGenerator32::addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&) -> PartialResult
{
    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    patch->effects.terminal = true;
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters()));

    Vector<ConstrainedTmp, 8> patchArgs;
    patchArgs.append(ConstrainedTmp(instanceValue(), B3::ValueRep::reg(GPRInfo::argumentGPR0)));
    patchArgs.append(ConstrainedTmp(TypedTmp(Tmp(GPRInfo::callFrameRegister), Types::I32), B3::ValueRep::reg(GPRInfo::argumentGPR1)));
    for (unsigned i = 0; i < args.size(); ++i) {
        if (args[i].isGPPair()) {
            patchArgs.append(ConstrainedTmp(TypedTmp(args[i].lo(), Types::I32), B3::ValueRep::stackArgument(i * sizeof(EncodedJSValue) + PayloadOffset)));
            patchArgs.append(ConstrainedTmp(TypedTmp(args[i].hi(), Types::I32), B3::ValueRep::stackArgument(i * sizeof(EncodedJSValue) + TagOffset)));
            continue;
        }
        patchArgs.append(ConstrainedTmp(args[i], B3::ValueRep::stackArgument(i * sizeof(EncodedJSValue))));
    }

    PatchpointExceptionHandle handle = preparePatchpointForExceptions(patch, patchArgs);

    patch->setGenerator([this, exceptionIndex, handle] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        handle.generate(jit, params, this);
        emitThrowImpl(jit, exceptionIndex);
    });

    emitPatchpoint(m_currentBlock, patch, Tmp(), WTFMove(patchArgs));

    return { };
}

auto AirIRGenerator32::addRethrow(unsigned, ControlType& data) -> PartialResult
{
    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters()));
    patch->effects.terminal = true;

    Vector<ConstrainedTmp, 3> patchArgs;
    patchArgs.append(ConstrainedTmp(instanceValue(), B3::ValueRep::reg(GPRInfo::argumentGPR0)));
    patchArgs.append(ConstrainedTmp(TypedTmp(Tmp(GPRInfo::callFrameRegister), Types::I32), B3::ValueRep::reg(GPRInfo::argumentGPR1)));
    patchArgs.append(ConstrainedTmp(TypedTmp(data.exception().lo(), Types::I32), B3::ValueRep::reg(GPRInfo::argumentGPR2)));
    patchArgs.append(ConstrainedTmp(TypedTmp(data.exception().hi(), Types::I32), B3::ValueRep::reg(GPRInfo::argumentGPR3)));

    PatchpointExceptionHandle handle = preparePatchpointForExceptions(patch, patchArgs);
    patch->setGenerator([this, handle] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        handle.generate(jit, params, this);
        emitRethrowImpl(jit);
    });

    emitPatchpoint(m_currentBlock, patch, Tmp(), WTFMove(patchArgs));

    return { };
}

auto AirIRGenerator32::emitCallPatchpoint(BasicBlock*block, B3::Type returnType, const ResultList& results, const Vector<TypedTmp>& tmpArgs, const CallInformation& wasmCalleeInfo, Vector<ConstrainedTmp> patchArgs) -> CallPatchpointData
{
    auto* patchpoint = addPatchpoint(returnType);
    patchpoint->effects.writesPinned = true;
    patchpoint->effects.readsPinned = true;
    patchpoint->clobberEarly(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->clobberLate(RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters()));

    ASSERT(wasmCalleeInfo.params.size() == tmpArgs.size());
    ASSERT(wasmCalleeInfo.results.size() == results.size());

    // On 32-bit platforms, 64-bit integer types are passed as two 32-bit values
    size_t offset = patchArgs.size();
    size_t passedArgs = tmpArgs.size();
    for (unsigned i = 0; i < tmpArgs.size(); ++i) {
        if (tmpArgs[i].isGPPair())
            ++passedArgs;
    }
    Checked<size_t> newSize = checkedSum<size_t>(patchArgs.size(), passedArgs);
    RELEASE_ASSERT(!newSize.hasOverflowed());
    patchArgs.grow(newSize);
    unsigned j = 0;
    for (unsigned i = 0; i < tmpArgs.size(); ++i) {
        const TypedTmp& arg = tmpArgs[i];
        auto& loc = wasmCalleeInfo.params[i].location;
        if (arg.isGPPair()) {
            B3::ValueRep valueRepLo = loc.isGPR() ? B3::ValueRep(loc.jsr().payloadGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP());
            B3::ValueRep valueRepHi = loc.isGPR() ? B3::ValueRep(loc.jsr().tagGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP() + 4);
            patchArgs[offset + j++] = ConstrainedTmp(TypedTmp(arg.lo(), Types::I32), valueRepLo);
            patchArgs[offset + j++] = ConstrainedTmp(TypedTmp(arg.hi(), Types::I32), valueRepHi);
            continue;
        }
        patchArgs[offset + j++] = ConstrainedTmp(arg, loc);
    }
    ASSERT(j == passedArgs);

    // On 32-bit platforms, 64-bit integer types are returned as two 32-bit values
    ResultList patchResults;
    if (patchpoint->type() != B3::Void) {
        Vector<B3::ValueRep, 1> resultConstraints;
        for (unsigned i = 0; i < results.size(); ++i) {
            const TypedTmp& result = results[i];
            auto& loc = wasmCalleeInfo.results[i].location;
            if (result.isGPPair()) {
                patchResults.append(TypedTmp { result.lo(), Types::I32 });
                patchResults.append(TypedTmp { result.hi(), Types::I32 });
                resultConstraints.append(loc.isGPR() ? B3::ValueRep(loc.jsr().payloadGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP()));
                resultConstraints.append(loc.isGPR() ? B3::ValueRep(loc.jsr().tagGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP() + 4));
                continue;
            }
            patchResults.append(result);
            resultConstraints.append(B3::ValueRep(loc));
        }
        patchpoint->resultConstraints = WTFMove(resultConstraints);
    } else
        ASSERT(!results.size());
    PatchpointExceptionHandle exceptionHandle = preparePatchpointForExceptions(patchpoint, patchArgs);
    emitPatchpoint(block, patchpoint, patchResults, WTFMove(patchArgs));
    return { patchpoint, exceptionHandle };
}

auto AirIRGenerator32::emitTailCallPatchpoint(BasicBlock* block, const Checked<int32_t>& tailCallStackOffsetFromFP, const Vector<ArgumentLocation>& constrainedArgLocations, const Vector<TypedTmp>& tmpArgs, Vector<ConstrainedTmp> patchArgs) -> CallPatchpointData
{
    UNUSED_PARAM(block);
    UNUSED_PARAM(tailCallStackOffsetFromFP);
    UNUSED_PARAM(constrainedArgLocations);
    UNUSED_PARAM(tmpArgs);
    UNUSED_PARAM(patchArgs);
    UNREACHABLE_FOR_PLATFORM();
}

void AirIRGenerator32::sanitizeAtomicResult(ExtAtomicOpType op, TypedTmp source, TypedTmp dest)
{
    ASSERT(source.type() == dest.type());
    switch (source.type().kind) {
    case TypeKind::I64: {
        switch (accessWidth(op)) {
        case Width8:
            append(ZeroExtend8To32, source.lo(), dest.lo());
            append(Move, Arg::imm(0), dest.hi());
            return;
        case Width16:
            append(ZeroExtend16To32, source.lo(), dest.lo());
            append(Move, Arg::imm(0), dest.hi());
            return;
        case Width32:
            append(Move, source.lo(), dest.lo());
            append(Move, Arg::imm(0), dest.hi());
            return;
        case Width64:
            emitMove(source, dest);
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
            emitMove(source, dest);
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

void AirIRGenerator32::sanitizeAtomicResult(ExtAtomicOpType op, TypedTmp result)
{
    sanitizeAtomicResult(op, result, result);
}

template<typename InputType>
TypedTmp AirIRGenerator32::appendGeneralAtomic(ExtAtomicOpType op, B3::Air::Opcode opcode, B3::Commutativity commutativity, InputType input, Arg address, TypedTmp oldValue)
{
    static_assert(std::is_same_v<InputType, TypedTmp> || std::is_same_v<InputType, Arg>);

    Width accessWidth = Wasm::accessWidth(op);

    auto newTmp = [&]() {
        if (accessWidth == Width64)
            return g64();
        return g32();
    };

    auto tmp = [&]() -> TypedTmp {
        if constexpr (std::is_same_v<InputType, Arg>) {
            TypedTmp result = newTmp();
            if (result.isGPPair()) {
                append(Move, input, result.lo());
                append(Move, Arg::imm(0), result.hi());
                return result;
            }
            append(Move, input, result);
            return result;
        } else
            return input;
    };

    auto imm = [&]() -> Arg {
        if constexpr (std::is_same_v<InputType, Arg>) {
            if (input.isImm())
                return input;
        }
        return Arg();
    };

    auto bitImm = [&]() -> Arg {
        if constexpr (std::is_same_v<InputType, Arg>) {
            if (input.isBitImm())
                return input;
        }
        return Arg();
    };

    TypedTmp newValue = opcode == B3::Air::Nop ? tmp() : newTmp();

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

    appendEffectful(MemoryFence);
    append(B3::Air::Jump);
    beginBlock->setSuccessors(reloopBlock);
    m_currentBlock = reloopBlock;

    RELEASE_ASSERT(isARM());

    if (accessWidth == Width64)
        appendEffectful(LoadLinkPair32, address, oldValue.lo(), oldValue.hi());
    else if (oldValue.isGPPair())
        appendEffectful(OPCODE_FOR_WIDTH(LoadLink, accessWidth), address, oldValue.lo());
    else
        appendEffectful(OPCODE_FOR_WIDTH(LoadLink, accessWidth), address, oldValue);

    if (opcode != B3::Air::Nop) {
        // FIXME: see note in WasmAirGenerator64::appendGeneralAtomic also
        TypedTmp inTmp = tmp();
        if (accessWidth == Width64) {
            switch (opcode) {
            case Add64:
                append(Add64, oldValue.hi(), oldValue.lo(), inTmp.hi(), inTmp.lo(), newValue.hi(), newValue.lo());
                break;
            case Sub64:
                append(Sub64, oldValue.hi(), oldValue.lo(), inTmp.hi(), inTmp.lo(), newValue.hi(), newValue.lo());
                break;
            case And64:
                append(And32, oldValue.hi(), inTmp.hi(), newValue.hi());
                append(And32, oldValue.lo(), inTmp.lo(), newValue.lo());
                break;
            case Or64:
                append(Or32, oldValue.hi(), inTmp.hi(), newValue.hi());
                append(Or32, oldValue.lo(), inTmp.lo(), newValue.lo());
                break;
            case Xor64:
                append(Xor32, oldValue.hi(), inTmp.hi(), newValue.hi());
                append(Xor32, oldValue.lo(), inTmp.lo(), newValue.lo());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        } else {
            Tmp oldVal = oldValue.isGPPair() ? oldValue.lo() : oldValue.tmp();
            Tmp newVal = newValue.isGPPair() ? newValue.lo() : newValue.tmp();
            Tmp inVal = inTmp.isGPPair() ? inTmp.lo() : inTmp.tmp();
            if (commutativity == B3::Commutative && imm() && isValidForm(opcode, Arg::Imm, Arg::Tmp, Arg::Tmp))
                append(opcode, imm(), oldVal, newVal);
            else if (imm() && isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Tmp))
                append(opcode, oldVal, imm(), newVal);
            else if (commutativity == B3::Commutative && bitImm() && isValidForm(opcode, Arg::BitImm, Arg::Tmp, Arg::Tmp))
                append(opcode, bitImm(), oldVal, newVal);
            else if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp))
                append(opcode, oldVal, inVal, newVal);
            else {
                append(Move, oldVal, newVal);
                if (imm() && isValidForm(opcode, Arg::Imm, Arg::Tmp))
                    append(opcode, imm(), newVal);
                else
                    append(opcode, inVal, newVal);
            }
        }
    }

    TypedTmp boolResult = g32();
    if (accessWidth == Width64)
        appendEffectful(StoreCondPair32, newValue.lo(), newValue.hi(), address, boolResult);
    else if (newValue.isGPPair())
        appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), newValue.lo(), address, boolResult);
    else
        appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), newValue, address, boolResult);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), boolResult, boolResult);
    reloopBlock->setSuccessors(doneBlock, reloopBlock);
    m_currentBlock = doneBlock;
    appendEffectful(MemoryFence);
    return oldValue;
}

TypedTmp AirIRGenerator32::appendStrongCAS(ExtAtomicOpType op, TypedTmp expected, TypedTmp newValue, Arg address, TypedTmp oldValue)
{
    Width accessWidth = Wasm::accessWidth(op);

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

    appendEffectful(MemoryFence);
    append(B3::Air::Jump);
    beginBlock->setSuccessors(reloopBlock);

    m_currentBlock = reloopBlock;
    if (accessWidth == Width64) {
        appendEffectful(LoadLinkPair32, address, oldValue.lo(), oldValue.hi());
        append(Branch32, Arg::relCond(MacroAssembler::NotEqual), oldValue.lo(), expected.lo());
        auto* checkHiBlock = m_code.addBlock();
        reloopBlock->setSuccessors(B3::Air::FrequentedBlock(strongFailBlock), checkHiBlock);
        m_currentBlock = checkHiBlock;
        append(Branch32, Arg::relCond(MacroAssembler::NotEqual), oldValue.hi(), expected.hi());
    } else if (oldValue.isGPPair()) {
        appendEffectful(OPCODE_FOR_WIDTH(LoadLink, accessWidth), address, oldValue.lo());
        append(OPCODE_FOR_CANONICAL_WIDTH(Branch, accessWidth), Arg::relCond(MacroAssembler::NotEqual), oldValue.lo(), expected.lo());
    } else {
        appendEffectful(OPCODE_FOR_WIDTH(LoadLink, accessWidth), address, oldValue);
        append(OPCODE_FOR_CANONICAL_WIDTH(Branch, accessWidth), Arg::relCond(MacroAssembler::NotEqual), oldValue, expected);
    }
    reloopBlock->setSuccessors(B3::Air::FrequentedBlock(strongFailBlock), storeBlock);

    m_currentBlock = storeBlock;
    {
        TypedTmp tmp = g32();
        if (accessWidth == Width64)
            appendEffectful(StoreCondPair32, newValue.lo(), newValue.hi(), address, tmp);
        else if (newValue.isGPPair())
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), newValue.lo(), address, tmp);
        else
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), newValue, address, tmp);
        append(BranchTest32, Arg::resCond(MacroAssembler::Zero), tmp, tmp);
    }
    storeBlock->setSuccessors(doneBlock, reloopBlock);

    m_currentBlock = strongFailBlock;
    {
        TypedTmp tmp = g32();
        if (accessWidth == Width64)
            appendEffectful(StoreCondPair32, oldValue.lo(), oldValue.hi(), address, tmp);
        else if (oldValue.isGPPair())
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), oldValue.lo(), address, tmp);
        else
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), oldValue, address, tmp);
        append(BranchTest32, Arg::resCond(MacroAssembler::Zero), tmp, tmp);
    }
    strongFailBlock->setSuccessors(B3::Air::FrequentedBlock(doneBlock), reloopBlock);

    m_currentBlock = doneBlock;
    appendEffectful(MemoryFence);
    return oldValue;
}

auto AirIRGenerator32::addShift(Type type, B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isI64() || type.isI32());

    if (type.isI64()) {
        if (op == RotateRight64 || op == RotateLeft64)
            return addRotate64(op, value, shift, result);
        return addShift64(op, value, shift, result);
    }

    result = tmpForType(type);

    if (isValidForm(op, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
        append(op, value, shift, result);
        return { };
    }

    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

auto AirIRGenerator32::addShift64(B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    ASSERT(op == Rshift64 || op == Urshift64 || op == Lshift64);

    result = g64();

    BasicBlock* check = m_code.addBlock();
    BasicBlock* aboveOrEquals32 = m_code.addBlock();
    BasicBlock* below32 = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    auto tmpShift = g32();
    append(Move, shift.lo(), tmpShift);
    append(Move, value.hi(), result.hi());
    append(Move, value.lo(), result.lo());
    append(And32, Arg::imm(63), tmpShift, tmpShift);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), tmpShift, tmpShift);
    m_currentBlock->setSuccessors(continuation, check);
    m_currentBlock = check;

    append(Branch32, Arg::relCond(MacroAssembler::Below), tmpShift, Arg::imm(32));
    m_currentBlock->setSuccessors(below32, aboveOrEquals32);
    m_currentBlock = aboveOrEquals32;

    append(Sub32, tmpShift, Arg::imm(32), tmpShift);
    if (op == Rshift64) {
        append(Rshift32, value.hi(), tmpShift, result.lo());
        append(Rshift32, value.hi(), Arg::imm(31), result.hi());
    } else if (op == Urshift64) {
        append(Urshift32, value.hi(), tmpShift, result.lo());
        append(Move, Arg::imm(0), result.hi());
    } else { // Lshift64
        append(Lshift32, value.lo(), tmpShift, result.hi());
        append(Move, Arg::imm(0), result.lo());
    }
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = below32;

    auto tmpComplementShift = g32();
    append(Move, Arg::imm(32), tmpComplementShift);
    append(Sub32, tmpShift, tmpComplementShift);
    if (op == Rshift64) {
        append(Urshift32, value.lo(), tmpShift, result.lo());
        append(Lshift32, value.hi(), tmpComplementShift, tmpComplementShift);
        append(Or32, tmpComplementShift, result.lo());
        append(Rshift32, value.hi(), tmpShift, result.hi());
    } else if (op == Urshift64) {
        append(Urshift32, value.lo(), tmpShift, result.lo());
        append(Lshift32, value.hi(), tmpComplementShift, tmpComplementShift);
        append(Or32, tmpComplementShift, result.lo());
        append(Urshift32, value.hi(), tmpShift, result.hi());
    } else { // Lshift64
        append(Lshift32, value.hi(), tmpShift, result.hi());
        append(Urshift32, value.lo(), tmpComplementShift, tmpComplementShift);
        append(Or32, tmpComplementShift, result.hi());
        append(Lshift32, value.lo(), tmpShift, result.lo());
    }
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = continuation;

    return { };
}

auto AirIRGenerator32::addRotate64(B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    ASSERT(op == RotateRight64 || op == RotateLeft64);

    result = g64();

    BasicBlock* swap = m_code.addBlock();
    BasicBlock* check = m_code.addBlock();
    BasicBlock* rotate = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    auto tmpArgLo = g32();
    auto tmpArgHi = g32();
    auto tmpShift = g32();
    append(Move, value.lo(), tmpArgLo);
    append(Move, value.hi(), tmpArgHi);
    append(Move, shift.lo(), tmpShift);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), tmpShift, Arg::imm(0x20));
    m_currentBlock->setSuccessors(check, swap);
    m_currentBlock = swap;

    append(Shuffle, tmpArgLo, tmpArgHi, Arg::widthArg(Width32), tmpArgHi, tmpArgLo, Arg::widthArg(Width32));
    append(Jump);
    m_currentBlock->setSuccessors(check);
    m_currentBlock = check;

    append(Move, tmpArgLo, result.lo());
    append(Move, tmpArgHi, result.hi());
    append(And32, Arg::imm(31), tmpShift, tmpShift);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), tmpShift, tmpShift);
    m_currentBlock->setSuccessors(continuation, rotate);
    m_currentBlock = rotate;

    auto tmpComplementShift = g32();
    auto tmpResLo = g32();
    auto tmpResHi = g32();
    append(Move, Arg::imm(32), tmpComplementShift);
    append(Sub32, tmpShift, tmpComplementShift);
    if (op == RotateRight64) {
        append(Urshift32, tmpArgLo, tmpShift, result.lo());
        append(Urshift32, tmpArgHi, tmpShift, result.hi());
        append(Lshift32, tmpArgLo, tmpComplementShift, tmpResHi);
        append(Lshift32, tmpArgHi, tmpComplementShift, tmpResLo);
    } else { // RotateLeft64
        append(Lshift32, tmpArgLo, tmpShift, result.lo());
        append(Lshift32, tmpArgHi, tmpShift, result.hi());
        append(Urshift32, tmpArgLo, tmpComplementShift, tmpResHi);
        append(Urshift32, tmpArgHi, tmpComplementShift, tmpResLo);
    }
    append(Or32, tmpResLo, result.lo());
    append(Or32, tmpResHi, result.hi());
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = continuation;

    return { };
}

auto AirIRGenerator32::addIntegerSub(B3::Air::Opcode op, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(op == Sub32 || op == Sub64);

    result = op == Sub32 ? g32() : g64();

    if (op == Sub64) {
        append(op, lhs.hi(), lhs.lo(), rhs.hi(), rhs.lo(), result.hi(), result.lo());
        return { };
    }

    if (isValidForm(op, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
        append(op, lhs, rhs, result);
        return { };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto AirIRGenerator32::addFloatingPointAbs(B3::Air::Opcode op, ExpressionType value, ExpressionType& result) -> PartialResult
{
    RELEASE_ASSERT(op == AbsFloat || op == AbsDouble);

    result = op == AbsFloat ? f32() : f64();

    if (isValidForm(op, Arg::Tmp, Arg::Tmp)) {
        append(op, value, result);
        return { };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto AirIRGenerator32::addFloatingPointBinOp(Type type, B3::Air::Opcode op, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isF32() || type.isF64());
    result = tmpForType(type);

    if (isValidForm(op, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
        append(op, lhs, rhs, result);
        return { };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto AirIRGenerator32::addCompare(Type type, MacroAssembler::RelationalCondition cond, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isI32() || type.isI64());

    result = g32();

    if (type.isI32()) {
        append(Compare32, Arg::relCond(cond), lhs, rhs, result);
        return { };
    }

    BasicBlock* continuation = m_code.addBlock();
    BasicBlock* compareLo = m_code.addBlock();

    if (cond == MacroAssembler::Equal || cond == MacroAssembler::NotEqual) {
        append(Move, Arg::imm(cond == MacroAssembler::NotEqual), result);
        append(Branch32, Arg::relCond(MacroAssembler::NotEqual), lhs.hi(), rhs.hi());
        m_currentBlock->setSuccessors(continuation, compareLo);
        m_currentBlock = compareLo;

        append(Compare32, Arg::relCond(cond), lhs.lo(), rhs.lo(), result);
        append(Jump);
        m_currentBlock->setSuccessors(continuation);
        m_currentBlock = continuation;
        return { };
    }

    BasicBlock* compareHi = m_code.addBlock();

    append(Branch32, Arg::relCond(MacroAssembler::Equal), lhs.hi(), rhs.hi());
    m_currentBlock->setSuccessors(compareLo, compareHi);
    m_currentBlock = compareHi;

    append(Compare32, Arg::relCond(cond), lhs.hi(), rhs.hi(), result);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = compareLo;

    // Signed to unsigned, leave the rest alone
    switch (cond) {
    case MacroAssembler::GreaterThan:
        cond = MacroAssembler::Above;
        break;
    case MacroAssembler::LessThan:
        cond = MacroAssembler::Below;
        break;
    case MacroAssembler::GreaterThanOrEqual:
        cond = MacroAssembler::AboveOrEqual;
        break;
    case MacroAssembler::LessThanOrEqual:
        cond = MacroAssembler::BelowOrEqual;
        break;
    default:
        break;
    }
    append(Compare32, Arg::relCond(cond), lhs.lo(), rhs.lo(), result);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = continuation;

    return { };
}

namespace {
template<typename IntType>
using BinaryFuncPtr = IntType (*)(IntType, IntType);
template<typename IntType> BinaryFuncPtr<IntType> getSoftDiv() { return nullptr; }
template<typename IntType> BinaryFuncPtr<IntType> getSoftMod() { return nullptr; }
template<> BinaryFuncPtr<int32_t> getSoftDiv<int32_t>() { return Math::i32_div_s; }
template<> BinaryFuncPtr<uint32_t> getSoftDiv<uint32_t>() { return Math::i32_div_u; }
template<> BinaryFuncPtr<int64_t> getSoftDiv<int64_t>() { return Math::i64_div_s; }
template<> BinaryFuncPtr<uint64_t> getSoftDiv<uint64_t>() { return Math::i64_div_u; }
template<> BinaryFuncPtr<int32_t> getSoftMod<int32_t>() { return Math::i32_rem_s; }
template<> BinaryFuncPtr<uint32_t> getSoftMod<uint32_t>() { return Math::i32_rem_u; }
template<> BinaryFuncPtr<int64_t> getSoftMod<int64_t>() { return Math::i64_rem_s; }
template<> BinaryFuncPtr<uint64_t> getSoftMod<uint64_t>() { return Math::i64_rem_u; }
}

template <typename IntType>
void AirIRGenerator32::emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8);

    result = sizeof(IntType) == 4 ? g32() : g64();

    if (isARM()) {
        // FIXME: use ARMv7 sdiv/udiv if available
        if (isDiv)
            emitCCall(getSoftDiv<IntType>(), result, lhs, rhs);
        else
            emitCCall(getSoftMod<IntType>(), result, lhs, rhs);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto AirIRGenerator32::addI64Add(ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Add64, lhs.hi(), lhs.lo(), rhs.hi(), rhs.lo(), result.hi(), result.lo());
    return { };
}

auto AirIRGenerator32::addI64Mul(ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    result = g64();
    auto tmpHiLo = g32();
    auto tmpLoHi = g32();
    append(Mul32, lhs.hi(), rhs.lo(), tmpHiLo);
    append(Mul32, lhs.lo(), rhs.hi(), tmpLoHi);
    append(UMull32, lhs.lo(), rhs.lo(), result.hi(), result.lo());
    append(Add32, tmpHiLo, result.hi());
    append(Add32, tmpLoHi, result.hi());
    return { };
}

auto AirIRGenerator32::addI64Eqz(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    BasicBlock* checkLo = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(Move, Arg::imm(0), result);
    append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), arg0.hi(), arg0.hi());
    m_currentBlock->setSuccessors(continuation, checkLo);
    m_currentBlock = checkLo;

    append(Test32, Arg::resCond(MacroAssembler::Zero), arg0.lo(), arg0.lo(), result);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = continuation;
    return { };
}

auto AirIRGenerator32::addI64And(ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(And32, lhs.lo(), rhs.lo(), result.lo());
    append(And32, lhs.hi(), rhs.hi(), result.hi());
    return { };
}

auto AirIRGenerator32::addI64Or(ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Or32, lhs.lo(), rhs.lo(), result.lo());
    append(Or32, lhs.hi(), rhs.hi(), result.hi());
    return { };
}

auto AirIRGenerator32::addI64Xor(ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Xor32, lhs.lo(), rhs.lo(), result.lo());
    append(Xor32, lhs.hi(), rhs.hi(), result.hi());
    return { };
}

auto AirIRGenerator32::addI64Clz(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        auto countLo = jit.branchTest32(MacroAssembler::Zero, params[2].gpr(), params[2].gpr());
        jit.countLeadingZeros32(params[2].gpr(), params[0].gpr());
        auto continuation = jit.jump();
        countLo.link(&jit);
        jit.countLeadingZeros32(params[1].gpr(), params[0].gpr());
        jit.add32(CCallHelpers::TrustedImm32(32), params[0].gpr());
        continuation.link(&jit);
    });
    emitPatchpoint(patchpoint, TypedTmp { result.lo(), Types::I32 }, TypedTmp { arg.lo(), Types::I32 }, TypedTmp { arg.hi(), Types::I32 });
    append(Move, Arg::imm(0), result.hi());
    return { };
}

auto AirIRGenerator32::addI64Ctz(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        auto countHi = jit.branchTest32(MacroAssembler::Zero, params[1].gpr(), params[1].gpr());
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
        auto continuation = jit.jump();
        countHi.link(&jit);
        jit.countTrailingZeros32(params[2].gpr(), params[0].gpr());
        jit.add32(CCallHelpers::TrustedImm32(32), params[0].gpr());
        continuation.link(&jit);
    });
    emitPatchpoint(patchpoint, TypedTmp { result.lo(), Types::I32 }, TypedTmp { arg.lo(), Types::I32 }, TypedTmp { arg.hi(), Types::I32 });
    append(Move, Arg::imm(0), result.hi());
    return { };
}

auto AirIRGenerator32::addI64Extend8S(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(SignExtend8To32, arg.lo(), result.lo());
    append(Rshift32, result.lo(), Arg::imm(31), result.hi());
    return { };
}

auto AirIRGenerator32::addI64Extend16S(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(SignExtend16To32, arg.lo(), result.lo());
    append(Rshift32, result.lo(), Arg::imm(31), result.hi());
    return { };
}

auto AirIRGenerator32::addI64Extend32S(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Move, arg.lo(), result.lo());
    append(Rshift32, arg.lo(), Arg::imm(31), result.hi());
    return { };
}

auto AirIRGenerator32::addI64ExtendUI32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Move, arg.tmp(), result.lo());
    append(Move, Arg::imm(0), result.hi());
    return { };
}

auto AirIRGenerator32::addI64ExtendSI32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Move, arg.tmp(), result.lo());
    append(Rshift32, arg.tmp(), Arg::imm(31), result.hi());
    return { };
}

auto AirIRGenerator32::addI32WrapI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Move, arg.lo(), result);
    return { };
}

auto AirIRGenerator32::addF64ConvertUI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f64();
    emitCCall(Math::f64_convert_u_i64, result, arg);
    return { };
}

auto AirIRGenerator32::addF64ConvertSI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f64();
    emitCCall(Math::f64_convert_s_i64, result, arg);
    return { };
}

auto AirIRGenerator32::addF32ConvertUI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f32();
    emitCCall(Math::f32_convert_u_i64, result, arg);
    return { };
}

auto AirIRGenerator32::addF32ConvertSI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f32();
    emitCCall(Math::f32_convert_s_i64, result, arg);
    return { };
}

auto AirIRGenerator32::addF32ConvertUI32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f32();
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.convertUInt32ToFloat(params[1].gpr(), params[0].fpr());
    });
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

auto AirIRGenerator32::addF64ConvertUI32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f64();
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.convertUInt32ToDouble(params[1].gpr(), params[0].fpr());
    });
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

auto AirIRGenerator32::addI64ReinterpretF64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(MoveDoubleTo64, arg, result.hi(), result.lo());
    return { };
}

auto AirIRGenerator32::addF64ReinterpretI64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(Move64ToDouble, arg.hi(), arg.lo(), result);
    return { };
}

auto AirIRGenerator32::addF32Nearest(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f32();
    emitCCall(&Math::f32_nearest, result, arg);
    return { };
}

auto AirIRGenerator32::addF64Nearest(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f64();
    emitCCall(&Math::f64_nearest, result, arg);
    return { };
}

auto AirIRGenerator32::addF64Copysign(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f64();
    auto tempA = g32();
    auto tempB = g32();
    auto sign = g32();
    auto value = g64();

    append(MoveDoubleHiTo32, arg1, sign);
    append(Move, Arg::bigImm(0x80000000), tempA);
    append(And32, tempA, sign, sign);

    append(MoveDoubleTo64, arg0, value.hi(), value.lo());
    append(Move, Arg::bigImm(0x7fffffff), tempB);
    append(And32, tempB, value.hi(), value.hi());

    append(Or32, sign, value.hi(), value.hi());
    append(Move64ToDouble, value.hi(), value.lo(), result);
    return { };
}

auto AirIRGenerator32::addUncheckedFloatingPointTruncation(FloatingPointTruncationKind kind, ExpressionType arg, ExpressionType result) -> PartialResult
{
    switch (kind) {
    case FloatingPointTruncationKind::F32ToI64:
        emitCCall(Math::i64_trunc_s_f32, result, arg);
        return { };
    case FloatingPointTruncationKind::F32ToU64:
        emitCCall(Math::i64_trunc_u_f32, result, arg);
        return { };
    case FloatingPointTruncationKind::F64ToI64:
        emitCCall(Math::i64_trunc_s_f64, result, arg);
        return { };
    case FloatingPointTruncationKind::F64ToU64:
        emitCCall(Math::i64_trunc_u_f64, result, arg);
        return { };
    default:
        break;
    }

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
        case FloatingPointTruncationKind::F64ToI32:
            jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F64ToU32:
            jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
            break;
        case FloatingPointTruncationKind::F32ToI64:
        case FloatingPointTruncationKind::F32ToU64:
        case FloatingPointTruncationKind::F64ToI64:
        case FloatingPointTruncationKind::F64ToU64:
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    });

    emitPatchpoint(patchpoint, result, arg);
    return { };
}

////////////////////////////////////////////////////////////////////////////////
// i31 manipulation

auto AirIRGenerator32::addI31New(ExpressionType value, ExpressionType& result) -> PartialResult
{
    result = gRef(Type { TypeKind::Ref, static_cast<TypeIndex>(TypeKind::I31ref) });

    auto tmp1 = g32();
    append(Move, Arg::bigImm(0x7fffffff), tmp1);
    append(And32, tmp1, value, result.lo());
    append(Move, Arg::bigImm(JSValue::Int32Tag), result.hi());

    return { };
}

auto AirIRGenerator32::addI31GetS(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    emitThrowOnNullReference(ref, ExceptionType::NullI31Get);

    auto tmpForShift = g32();
    result = g32();

    append(Move, Arg::imm(1), tmpForShift);
    append(Move, ref.lo(), result);
    addShift(Types::I32, Lshift32, result, tmpForShift, result);
    addShift(Types::I32, Rshift32, result, tmpForShift, result);

    return { };
}

auto AirIRGenerator32::addI31GetU(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    emitThrowOnNullReference(ref, ExceptionType::NullI31Get);

    result = g32();
    append(Move, ref.lo(), result);

    return { };
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileAir(CompilationContext& compilationContext, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount* tierUp)
{
    return parseAndCompileAirImpl<AirIRGenerator32>(compilationContext, function, signature, unlinkedWasmToWasmCalls, info, mode, functionIndex, hasExceptionHandlers, tierUp);
}

}
} // namespace JSC::Wasm

#endif
