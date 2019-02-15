/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WasmAirIRGenerator.h"

#if ENABLE(WEBASSEMBLY)

#include "AirCode.h"
#include "AirGenerate.h"
#include "AirOpcodeUtils.h"
#include "AirValidate.h"
#include "AllowMacroScratchRegisterUsageIf.h"
#include "B3CCallValue.h"
#include "B3CheckSpecial.h"
#include "B3CheckValue.h"
#include "B3PatchpointSpecial.h"
#include "B3Procedure.h"
#include "B3ProcedureInlines.h"
#include "BinarySwitch.h"
#include "ScratchRegisterAllocator.h"
#include "VirtualRegister.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmExceptionType.h"
#include "WasmFunctionParser.h"
#include "WasmInstance.h"
#include "WasmMemory.h"
#include "WasmOMGPlan.h"
#include "WasmOpcodeOrigin.h"
#include "WasmSignatureInlines.h"
#include "WasmThunks.h"
#include <limits>
#include <wtf/Box.h>
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

using namespace B3::Air;

struct ConstrainedTmp {
    ConstrainedTmp(Tmp tmp)
        : ConstrainedTmp(tmp, tmp.isReg() ? B3::ValueRep::reg(tmp.reg()) : B3::ValueRep::SomeRegister)
    { }

    ConstrainedTmp(Tmp tmp, B3::ValueRep rep)
        : tmp(tmp)
        , rep(rep)
    {
    }

    Tmp tmp;
    B3::ValueRep rep;
};

class TypedTmp {
public:
    constexpr TypedTmp()
        : m_tmp()
        , m_type(Type::Void)
    { }

    TypedTmp(Tmp tmp, Type type)
        : m_tmp(tmp)
        , m_type(type)
    { }

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

private:

    Tmp m_tmp;
    Type m_type;
};

class AirIRGenerator {
public:
    struct ControlData {
        ControlData(B3::Origin origin, Type returnType, TypedTmp resultTmp, BlockType type, BasicBlock* continuation, BasicBlock* special = nullptr)
            : blockType(type)
            , continuation(continuation)
            , special(special)
            , returnType(returnType)
        {
            UNUSED_PARAM(origin); // FIXME: Use origin.
            if (resultTmp) {
                ASSERT(returnType != Type::Void);
                result.append(resultTmp);
            } else
                ASSERT(returnType == Type::Void);
        }

        ControlData()
        {
        }

        void dump(PrintStream& out) const
        {
            switch (type()) {
            case BlockType::If:
                out.print("If:       ");
                break;
            case BlockType::Block:
                out.print("Block:    ");
                break;
            case BlockType::Loop:
                out.print("Loop:     ");
                break;
            case BlockType::TopLevel:
                out.print("TopLevel: ");
                break;
            }
            out.print("Continuation: ", *continuation, ", Special: ");
            if (special)
                out.print(*special);
            else
                out.print("None");
        }

        BlockType type() const { return blockType; }

        Type signature() const { return returnType; }

        bool hasNonVoidSignature() const { return result.size(); }

        BasicBlock* targetBlockForBranch()
        {
            if (type() == BlockType::Loop)
                return special;
            return continuation;
        }

        void convertIfToBlock()
        {
            ASSERT(type() == BlockType::If);
            blockType = BlockType::Block;
            special = nullptr;
        }

        using ResultList = Vector<TypedTmp, 1>;

        ResultList resultForBranch() const
        {
            if (type() == BlockType::Loop)
                return ResultList();
            return result;
        }

    private:
        friend class AirIRGenerator;
        BlockType blockType;
        BasicBlock* continuation;
        BasicBlock* special;
        ResultList result;
        Type returnType;
    };

    using ExpressionType = TypedTmp;
    using ControlType = ControlData;
    using ExpressionList = Vector<ExpressionType, 1>;
    using ResultList = ControlData::ResultList;
    using ControlEntry = FunctionParser<AirIRGenerator>::ControlEntry;

    static ExpressionType emptyExpression() { return { }; };

    using ErrorType = String;
    using UnexpectedResult = Unexpected<ErrorType>;
    using Result = Expected<std::unique_ptr<InternalFunction>, ErrorType>;
    using PartialResult = Expected<void, ErrorType>;

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module failed compiling: "_s, makeString(args)...));
    }

#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition))                  \
            return fail(__VA_ARGS__);             \
    } while (0)

    AirIRGenerator(const ModuleInformation&, B3::Procedure&, InternalFunction*, Vector<UnlinkedWasmToWasmCall>&, MemoryMode, unsigned functionIndex, TierUpCount*, ThrowWasmException, const Signature&);

    PartialResult WARN_UNUSED_RETURN addArguments(const Signature&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);
    ExpressionType addConstant(BasicBlock*, Type, uint64_t);

    // Locals
    PartialResult WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    // Globals
    PartialResult WARN_UNUSED_RETURN getGlobal(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN setGlobal(uint32_t index, ExpressionType value);

    // Memory
    PartialResult WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN addGrowMemory(ExpressionType delta, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addCurrentMemory(ExpressionType& result);

    // Basic operators
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addTopLevel(Type signature);
    ControlData WARN_UNUSED_RETURN addBlock(Type signature);
    ControlData WARN_UNUSED_RETURN addLoop(Type signature);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType condition, Type signature, ControlData& result);
    PartialResult WARN_UNUSED_RETURN addElse(ControlData&, const ExpressionList&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&);

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const Signature&, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(const Signature&, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addUnreachable();

    PartialResult addShift(Type, B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
    PartialResult addIntegerSub(B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addFloatingPointAbs(B3::Air::Opcode, ExpressionType value, ExpressionType& result);
    PartialResult addFloatingPointBinOp(Type, B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    void dump(const Vector<ControlEntry>& controlStack, const ExpressionList* expressionStack);
    void setParser(FunctionParser<AirIRGenerator>* parser) { m_parser = parser; };

    static Vector<Tmp> toTmpVector(const Vector<TypedTmp>& vector)
    {
        Vector<Tmp> result;
        for (const auto& item : vector)
            result.append(item.tmp());
        return result;
    }

    ALWAYS_INLINE void didKill(const ExpressionType& typedTmp)
    {
        Tmp tmp = typedTmp.tmp();
        if (!tmp)
            return;
        if (tmp.isGP())
            m_freeGPs.append(tmp);
        else
            m_freeFPs.append(tmp);
    }

private:
    ALWAYS_INLINE void validateInst(Inst& inst)
    {
        if (!ASSERT_DISABLED) {
            if (!inst.isValidForm()) {
                dataLogLn(inst);
                CRASH();
            }
        }
    }

    static Arg extractArg(const TypedTmp& tmp) { return tmp.tmp(); }
    static Arg extractArg(const Tmp& tmp) { return Arg(tmp); }
    static Arg extractArg(const Arg& arg) { return arg; }

    template<typename... Arguments>
    void append(BasicBlock* block, Kind kind, Arguments&&... arguments)
    {
        // FIXME: Find a way to use origin here.
        auto& inst = block->append(kind, nullptr, extractArg(arguments)...);
        validateInst(inst);
    }

    template<typename... Arguments>
    void append(Kind kind, Arguments&&... arguments)
    {
        append(m_currentBlock, kind, std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    void appendEffectful(B3::Air::Opcode op, Arguments&&... arguments)
    {
        Kind kind = op;
        kind.effects = true;
        append(m_currentBlock, kind, std::forward<Arguments>(arguments)...);
    }

    Tmp newTmp(B3::Bank bank)
    {
        switch (bank) {
        case B3::GP:
            if (m_freeGPs.size())
                return m_freeGPs.takeLast();
            break;
        case B3::FP:
            if (m_freeFPs.size())
                return m_freeFPs.takeLast();
            break;
        }
        return m_code.newTmp(bank);
    }

    TypedTmp g32() { return { newTmp(B3::GP), Type::I32 }; }
    TypedTmp g64() { return { newTmp(B3::GP), Type::I64 }; }
    TypedTmp f32() { return { newTmp(B3::FP), Type::F32 }; }
    TypedTmp f64() { return { newTmp(B3::FP), Type::F64 }; }

    TypedTmp tmpForType(Type type)
    {
        switch (type) {
        case Type::I32:
            return g32();
        case Type::I64:
            return g64();
        case Type::F32:
            return f32();
        case Type::F64:
            return f64();
        case Type::Void:
            return { };
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    B3::PatchpointValue* addPatchpoint(B3::Type type)
    {
        return m_proc.add<B3::PatchpointValue>(type, B3::Origin());
    }

    template <typename ...Args>
    void emitPatchpoint(B3::PatchpointValue* patch, Tmp result, Args... theArgs)
    {
        emitPatchpoint(m_currentBlock, patch, result, std::forward<Args>(theArgs)...);
    }

    template <typename ...Args>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result, Args... theArgs)
    {
        emitPatchpoint(basicBlock, patch, result, Vector<ConstrainedTmp, sizeof...(Args)>::from(theArgs...));
    }

    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result)
    {
        emitPatchpoint(basicBlock, patch, result, Vector<ConstrainedTmp>());
    }

    template <size_t inlineSize>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result, Vector<ConstrainedTmp, inlineSize>&& args)
    {
        if (!m_patchpointSpecial)
            m_patchpointSpecial = static_cast<B3::PatchpointSpecial*>(m_code.addSpecial(std::make_unique<B3::PatchpointSpecial>()));

        Inst inst(Patch, patch, Arg::special(m_patchpointSpecial));
        Inst resultMov;
        if (result) {
            ASSERT(patch->type() != B3::Void);
            switch (patch->resultConstraint.kind()) {
            case B3::ValueRep::Register:
                inst.args.append(Tmp(patch->resultConstraint.reg()));
                resultMov = Inst(result.isGP() ? Move : MoveDouble, nullptr, Tmp(patch->resultConstraint.reg()), result);
                break;
            case B3::ValueRep::SomeRegister:
                inst.args.append(result);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else
            ASSERT(patch->type() == B3::Void);
        
        for (ConstrainedTmp& tmp : args) {
            // FIXME: This is less than ideal to create dummy values just to satisfy Air's
            // validation. We should abstrcat Patch enough so ValueRep's don't need to be
            // backed by Values.
            // https://bugs.webkit.org/show_bug.cgi?id=194040
            B3::Value* dummyValue = m_proc.addConstant(B3::Origin(), tmp.tmp.isGP() ? B3::Int64 : B3::Double, 0);
            patch->append(dummyValue, tmp.rep);
            switch (tmp.rep.kind()) {
            case B3::ValueRep::SomeRegister:
                inst.args.append(tmp.tmp);
                break;
            case B3::ValueRep::Register:
                patch->earlyClobbered().clear(tmp.rep.reg());
                append(basicBlock, tmp.tmp.isGP() ? Move : MoveDouble, tmp.tmp, tmp.rep.reg());
                inst.args.append(Tmp(tmp.rep.reg()));
                break;
            case B3::ValueRep::StackArgument: {
                auto arg = Arg::callArg(tmp.rep.offsetFromSP());
                append(basicBlock, tmp.tmp.isGP() ? Move : MoveDouble, tmp.tmp, arg);
                inst.args.append(arg);
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        if (patch->resultConstraint.isReg())
            patch->lateClobbered().clear(patch->resultConstraint.reg());
        for (unsigned i = patch->numGPScratchRegisters; i--;)
            inst.args.append(g64().tmp());
        for (unsigned i = patch->numFPScratchRegisters; i--;)
            inst.args.append(f64().tmp());

        validateInst(inst);
        basicBlock->append(WTFMove(inst));
        if (resultMov) {
            validateInst(resultMov);
            basicBlock->append(WTFMove(resultMov));
        }
    }

    template <typename Branch, typename Generator>
    void emitCheck(const Branch& makeBranch, const Generator& generator)
    {
        // We fail along the truthy edge of 'branch'.
        Inst branch = makeBranch();

        // FIXME: Make a hashmap of these.
        B3::CheckSpecial::Key key(branch);
        B3::CheckSpecial* special = static_cast<B3::CheckSpecial*>(m_code.addSpecial(std::make_unique<B3::CheckSpecial>(key)));

        // FIXME: Remove the need for dummy values
        // https://bugs.webkit.org/show_bug.cgi?id=194040
        B3::Value* dummyPredicate = m_proc.addConstant(B3::Origin(), B3::Int32, 42);
        B3::CheckValue* checkValue = m_proc.add<B3::CheckValue>(B3::Check, B3::Origin(), dummyPredicate);
        checkValue->setGenerator(generator);

        Inst inst(Patch, checkValue, Arg::special(special));
        inst.args.appendVector(branch.args);
        m_currentBlock->append(WTFMove(inst));
    }

    template <typename Func, typename ...Args>
    void emitCCall(Func func, TypedTmp result, Args... args)
    {
        emitCCall(m_currentBlock, func, result, std::forward<Args>(args)...);
    }
    template <typename Func, typename ...Args>
    void emitCCall(BasicBlock* block, Func func, TypedTmp result, Args... theArgs)
    {
        B3::Type resultType = B3::Void;
        if (result) {
            switch (result.type()) {
            case Type::I32:
                resultType = B3::Int32;
                break;
            case Type::I64:
                resultType = B3::Int64;
                break;
            case Type::F32:
                resultType = B3::Float;
                break;
            case Type::F64:
                resultType = B3::Double;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        auto makeDummyValue = [&] (Tmp tmp) {
            // FIXME: This is less than ideal to create dummy values just to satisfy Air's
            // validation. We should abstrcat CCall enough so we're not reliant on arguments
            // to the B3::CCallValue.
            // https://bugs.webkit.org/show_bug.cgi?id=194040
            if (tmp.isGP())
                return m_proc.addConstant(B3::Origin(), B3::Int64, 0);
            return m_proc.addConstant(B3::Origin(), B3::Double, 0);
        };

        B3::Value* dummyFunc = m_proc.addConstant(B3::Origin(), B3::Int64, bitwise_cast<uintptr_t>(func));
        B3::Value* origin = m_proc.add<B3::CCallValue>(resultType, B3::Origin(), B3::Effects::none(), dummyFunc, makeDummyValue(theArgs)...);

        Inst inst(CCall, origin);

        Tmp callee = g64();
        append(Move, Arg::immPtr(tagCFunctionPtr<void*>(func, B3CCallPtrTag)), callee);
        inst.args.append(callee);

        if (result)
            inst.args.append(result.tmp());

        for (Tmp tmp : Vector<Tmp, sizeof...(Args)>::from(theArgs.tmp()...))
            inst.args.append(tmp);

        block->append(WTFMove(inst));
    }

    static B3::Air::Opcode moveOpForValueType(Type type)
    {
        switch (type) {
        case Type::I32:
            return Move32;
        case Type::I64:
            return Move;
        case Type::F32:
            return MoveFloat;
        case Type::F64:
            return MoveDouble;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void emitThrowException(CCallHelpers&, ExceptionType);

    void emitTierUpCheck(uint32_t decrementCount, B3::Origin);

    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    void unify(const ExpressionType& dst, const ExpressionType& source);
    void unifyValuesWithBlock(const ExpressionList& resultStack, const ResultList& stack);

    template <typename IntType>
    void emitChecksForModOrDiv(bool isSignedDiv, ExpressionType left, ExpressionType right);

    template <typename IntType>
    void emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    int32_t WARN_UNUSED_RETURN fixupPointerPlusOffset(ExpressionType&, uint32_t);

    void restoreWasmContextInstance(BasicBlock*, TypedTmp);
    enum class RestoreCachedStackLimit { No, Yes };
    void restoreWebAssemblyGlobalState(RestoreCachedStackLimit, const MemoryInformation&, TypedTmp instance, BasicBlock*);

    B3::Origin origin();

    FunctionParser<AirIRGenerator>* m_parser { nullptr };
    const ModuleInformation& m_info;
    const MemoryMode m_mode { MemoryMode::BoundsChecking };
    const unsigned m_functionIndex { UINT_MAX };
    const TierUpCount* m_tierUp { nullptr };

    B3::Procedure& m_proc;
    Code& m_code;
    BasicBlock* m_currentBlock { nullptr };
    BasicBlock* m_rootBlock { nullptr };
    Vector<TypedTmp> m_locals;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    GPRReg m_memoryBaseGPR { InvalidGPRReg };
    GPRReg m_memorySizeGPR { InvalidGPRReg };
    GPRReg m_wasmContextInstanceGPR { InvalidGPRReg };
    bool m_makesCalls { false };

    Vector<Tmp, 8> m_freeGPs;
    Vector<Tmp, 8> m_freeFPs;

    TypedTmp m_instanceValue; // Always use the accessor below to ensure the instance value is materialized when used.
    bool m_usesInstanceValue { false };
    TypedTmp instanceValue()
    {
        m_usesInstanceValue = true;
        return m_instanceValue;
    }

    uint32_t m_maxNumJSCallArguments { 0 };

    B3::PatchpointSpecial* m_patchpointSpecial { nullptr };
};

// Memory accesses in WebAssembly have unsigned 32-bit offsets, whereas they have signed 32-bit offsets in B3.
int32_t AirIRGenerator::fixupPointerPlusOffset(ExpressionType& ptr, uint32_t offset)
{
    if (static_cast<uint64_t>(offset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        auto previousPtr = ptr;
        ptr = g64();
        auto constant = g64();
        append(Move, Arg::bigImm(offset), constant);
        append(Add64, constant, previousPtr, ptr);
        return 0;
    }
    return offset;
}

void AirIRGenerator::restoreWasmContextInstance(BasicBlock* block, TypedTmp instance)
{
    if (Context::useFastTLS()) {
        auto* patchpoint = addPatchpoint(B3::Void);
        if (CCallHelpers::storeWasmContextInstanceNeedsMacroScratchRegister())
            patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsageIf allowScratch(jit, CCallHelpers::storeWasmContextInstanceNeedsMacroScratchRegister());
            jit.storeWasmContextInstance(params[0].gpr());
        });
        emitPatchpoint(block, patchpoint, Tmp(), instance);
        return;
    }

    // FIXME: Because WasmToWasm call clobbers wasmContextInstance register and does not restore it, we need to restore it in the caller side.
    // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
    auto* patchpoint = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.writesPinned = true;
    effects.reads = B3::HeapRange::top();
    patchpoint->effects = effects;
    patchpoint->clobberLate(RegisterSet(m_wasmContextInstanceGPR));
    GPRReg wasmContextInstanceGPR = m_wasmContextInstanceGPR;
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& param) {
        jit.move(param[0].gpr(), wasmContextInstanceGPR);
    });
    emitPatchpoint(block, patchpoint, Tmp(), instance);
}

AirIRGenerator::AirIRGenerator(const ModuleInformation& info, B3::Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, unsigned functionIndex, TierUpCount* tierUp, ThrowWasmException throwWasmException, const Signature& signature)
    : m_info(info)
    , m_mode(mode)
    , m_functionIndex(functionIndex)
    , m_tierUp(tierUp)
    , m_proc(procedure)
    , m_code(m_proc.code())
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
{
    m_currentBlock = m_code.addBlock();
    m_rootBlock = m_currentBlock;

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623
    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();

    m_memoryBaseGPR = pinnedRegs.baseMemoryPointer;
    m_code.pinRegister(m_memoryBaseGPR);

    m_wasmContextInstanceGPR = pinnedRegs.wasmContextInstancePointer;
    if (!Context::useFastTLS())
        m_code.pinRegister(m_wasmContextInstanceGPR);

    if (mode != MemoryMode::Signaling) {
        ASSERT(!pinnedRegs.sizeRegisters[0].sizeOffset);
        m_memorySizeGPR = pinnedRegs.sizeRegisters[0].sizeRegister;
        for (const PinnedSizeRegisterInfo& regInfo : pinnedRegs.sizeRegisters)
            m_code.pinRegister(regInfo.sizeRegister);
    }

    if (throwWasmException)
        Thunks::singleton().setThrowWasmException(throwWasmException);

    if (info.memory) {
        switch (m_mode) {
        case MemoryMode::BoundsChecking:
            break;
        case MemoryMode::Signaling:
            // Most memory accesses in signaling mode don't do an explicit
            // exception check because they can rely on fault handling to detect
            // out-of-bounds accesses. FaultSignalHandler nonetheless needs the
            // thunk to exist so that it can jump to that thunk.
            if (UNLIKELY(!Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator)))
                CRASH();
            break;
        }
    }

    m_code.setNumEntrypoints(1);

    GPRReg contextInstance = Context::useFastTLS() ? wasmCallingConventionAir().prologueScratch(1) : m_wasmContextInstanceGPR;

    Ref<B3::Air::PrologueGenerator> prologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([=] (CCallHelpers& jit, B3::Air::Code& code) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        code.emitDefaultPrologue(jit);

        {
            GPRReg calleeGPR = wasmCallingConventionAir().prologueScratch(0);
            auto moveLocation = jit.moveWithPatch(MacroAssembler::TrustedImmPtr(nullptr), calleeGPR);
            jit.addLinkTask([compilation, moveLocation] (LinkBuffer& linkBuffer) {
                compilation->calleeMoveLocation = linkBuffer.locationOf<WasmEntryPtrTag>(moveLocation);
            });
            jit.emitPutToCallFrameHeader(calleeGPR, CallFrameSlot::callee);
            jit.emitPutToCallFrameHeader(nullptr, CallFrameSlot::codeBlock);
        }

        {
            const Checked<int32_t> wasmFrameSize = m_code.frameSize();
            const unsigned minimumParentCheckSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), 1024);
            const unsigned extraFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), std::max<uint32_t>(
                // This allows us to elide stack checks for functions that are terminal nodes in the call
                // tree, (e.g they don't make any calls) and have a small enough frame size. This works by
                // having any such terminal node have its parent caller include some extra size in its
                // own check for it. The goal here is twofold:
                // 1. Emit less code.
                // 2. Try to speed things up by skipping stack checks.
                minimumParentCheckSize,
                // This allows us to elide stack checks in the Wasm -> Embedder call IC stub. Since these will
                // spill all arguments to the stack, we ensure that a stack check here covers the
                // stack that such a stub would use.
                (Checked<uint32_t>(m_maxNumJSCallArguments) * sizeof(Register) + jscCallingConvention().headerSizeInBytes()).unsafeGet()
            ));
            const int32_t checkSize = m_makesCalls ? (wasmFrameSize + extraFrameSize).unsafeGet() : wasmFrameSize.unsafeGet();
            bool needUnderflowCheck = static_cast<unsigned>(checkSize) > Options::reservedZoneSize();
            bool needsOverflowCheck = m_makesCalls || wasmFrameSize >= minimumParentCheckSize || needUnderflowCheck;

            // This allows leaf functions to not do stack checks if their frame size is within
            // certain limits since their caller would have already done the check.
            if (needsOverflowCheck) {
                GPRReg scratch = wasmCallingConventionAir().prologueScratch(0);

                if (Context::useFastTLS())
                    jit.loadWasmContextInstance(contextInstance);

                jit.addPtr(CCallHelpers::TrustedImm32(-checkSize), GPRInfo::callFrameRegister, scratch);
                MacroAssembler::JumpList overflow;
                if (UNLIKELY(needUnderflowCheck))
                    overflow.append(jit.branchPtr(CCallHelpers::Above, scratch, GPRInfo::callFrameRegister));
                overflow.append(jit.branchPtr(CCallHelpers::Below, scratch, CCallHelpers::Address(contextInstance, Instance::offsetOfCachedStackLimit())));
                jit.addLinkTask([overflow] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(overflow, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
                });
            } else if (m_usesInstanceValue && Context::useFastTLS()) {
                // No overflow check is needed, but the instance values still needs to be correct.
                jit.loadWasmContextInstance(contextInstance);
            }
        }
    });

    m_code.setPrologueForEntrypoint(0, WTFMove(prologueGenerator));

    if (Context::useFastTLS()) {
        m_instanceValue = g64();
        // FIXME: Would be nice to only do this if we use instance value.
        append(Move, Tmp(contextInstance), m_instanceValue);
    } else
        m_instanceValue = { Tmp(contextInstance), Type::I64 };

    ASSERT(!m_locals.size());
    m_locals.grow(signature.argumentCount());
    for (unsigned i = 0; i < signature.argumentCount(); ++i) {
        Type type = signature.argument(i);
        m_locals[i] = tmpForType(type);
    }

    wasmCallingConventionAir().loadArguments(signature, [&] (const Arg& arg, unsigned i) {
        switch (signature.argument(i)) {
        case Type::I32:
            append(Move32, arg, m_locals[i]);
            break;
        case Type::I64:
            append(Move, arg, m_locals[i]);
            break;
        case Type::F32:
            append(MoveFloat, arg, m_locals[i]);
            break;
        case Type::F64:
            append(MoveDouble, arg, m_locals[i]);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    });

    emitTierUpCheck(TierUpCount::functionEntryDecrement(), B3::Origin());
}

void AirIRGenerator::restoreWebAssemblyGlobalState(RestoreCachedStackLimit restoreCachedStackLimit, const MemoryInformation& memory, TypedTmp instance, BasicBlock* block)
{
    restoreWasmContextInstance(block, instance);

    if (restoreCachedStackLimit == RestoreCachedStackLimit::Yes) {
        // The Instance caches the stack limit, but also knows where its canonical location is.
        static_assert(sizeof(decltype(static_cast<Instance*>(nullptr)->cachedStackLimit())) == sizeof(uint64_t), "");

        RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfPointerToActualStackLimit(), B3::Width64));
        RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfCachedStackLimit(), B3::Width64));
        auto temp = g64();
        append(block, Move, Arg::addr(instanceValue(), Instance::offsetOfPointerToActualStackLimit()), temp);
        append(block, Move, Arg::addr(temp), temp);
        append(block, Move, temp, Arg::addr(instanceValue(), Instance::offsetOfCachedStackLimit()));
    }

    if (!!memory) {
        const PinnedRegisterInfo* pinnedRegs = &PinnedRegisterInfo::get();
        RegisterSet clobbers;
        clobbers.set(pinnedRegs->baseMemoryPointer);
        for (auto info : pinnedRegs->sizeRegisters)
            clobbers.set(info.sizeRegister);

        auto* patchpoint = addPatchpoint(B3::Void);
        B3::Effects effects = B3::Effects::none();
        effects.writesPinned = true;
        effects.reads = B3::HeapRange::top();
        patchpoint->effects = effects;
        patchpoint->clobber(clobbers);

        patchpoint->setGenerator([pinnedRegs] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            GPRReg baseMemory = pinnedRegs->baseMemoryPointer;
            const auto& sizeRegs = pinnedRegs->sizeRegisters;
            ASSERT(sizeRegs.size() >= 1);
            ASSERT(!sizeRegs[0].sizeOffset); // The following code assumes we start at 0, and calculates subsequent size registers relative to 0.
            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), Instance::offsetOfCachedMemorySize()), sizeRegs[0].sizeRegister);
            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), Instance::offsetOfCachedMemory()), baseMemory);
            for (unsigned i = 1; i < sizeRegs.size(); ++i)
                jit.add64(CCallHelpers::TrustedImm32(-sizeRegs[i].sizeOffset), sizeRegs[0].sizeRegister, sizeRegs[i].sizeRegister);
        });

        emitPatchpoint(block, patchpoint, Tmp(), instance);
    }
}

void AirIRGenerator::emitThrowException(CCallHelpers& jit, ExceptionType type)
{
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    auto jumpToExceptionStub = jit.jump();

    jit.addLinkTask([jumpToExceptionStub] (LinkBuffer& linkBuffer) {
        linkBuffer.link(jumpToExceptionStub, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator).code()));
    });
}

auto AirIRGenerator::addLocal(Type type, uint32_t count) -> PartialResult
{
    Checked<uint32_t, RecordOverflow> totalBytesChecked = count;
    totalBytesChecked += m_locals.size();
    uint32_t totalBytes;
    WASM_COMPILE_FAIL_IF((totalBytesChecked.safeGet(totalBytes) == CheckedState::DidOverflow) || !m_locals.tryReserveCapacity(totalBytes), "can't allocate memory for ", totalBytes, " locals");

    for (uint32_t i = 0; i < count; ++i) {
        auto local = tmpForType(type);
        m_locals.uncheckedAppend(local);
        switch (type) {
        case Type::I32:
        case Type::I64: {
            append(Xor64, local, local);
            break;
        }
        case Type::F32:
        case Type::F64: {
            auto temp = g64();
            // IEEE 754 "0" is just int32/64 zero.
            append(Xor64, temp, temp);
            append(type == Type::F32 ? Move32ToFloat : Move64ToDouble, temp, local);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    return { };
}

auto AirIRGenerator::addConstant(Type type, uint64_t value) -> ExpressionType
{
    return addConstant(m_currentBlock, type, value);
}

auto AirIRGenerator::addConstant(BasicBlock* block, Type type, uint64_t value) -> ExpressionType
{
    auto result = tmpForType(type);
    switch (type) {
    case Type::I32:
    case Type::I64:
        append(block, Move, Arg::bigImm(value), result);
        break;
    case Type::F32:
    case Type::F64: {
        auto tmp = g64();
        append(block, Move, Arg::bigImm(value), tmp);
        append(block, type == Type::F32 ? Move32ToFloat : Move64ToDouble, tmp, result);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return result;
}

auto AirIRGenerator::addArguments(const Signature& signature) -> PartialResult
{
    RELEASE_ASSERT(m_locals.size() == signature.argumentCount()); // We handle arguments in the prologue
    return { };
}

auto AirIRGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    ASSERT(m_locals[index].tmp());
    result = tmpForType(m_locals[index].type());
    append(moveOpForValueType(m_locals[index].type()), m_locals[index].tmp(), result);
    return { };
}

auto AirIRGenerator::addUnreachable() -> PartialResult
{
    B3::PatchpointValue* unreachable = addPatchpoint(B3::Void);
    unreachable->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::Unreachable);
    });
    unreachable->effects.terminal = true;
    emitPatchpoint(unreachable, Tmp());
    return { };
}

auto AirIRGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    int32_t (*growMemory)(void*, Instance*, int32_t) = [] (void* callFrame, Instance* instance, int32_t delta) -> int32_t {
        instance->storeTopCallFrame(callFrame);

        if (delta < 0)
            return -1;

        auto grown = instance->memory()->grow(PageCount(delta));
        if (!grown) {
            switch (grown.error()) {
            case Memory::GrowFailReason::InvalidDelta:
            case Memory::GrowFailReason::InvalidGrowSize:
            case Memory::GrowFailReason::WouldExceedMaximum:
            case Memory::GrowFailReason::OutOfMemory:
                return -1;
            }
            RELEASE_ASSERT_NOT_REACHED();
        }

        return grown.value().pageCount();
    };

    result = g32();
    emitCCall(growMemory, result, TypedTmp { Tmp(GPRInfo::callFrameRegister), Type::I64 }, instanceValue(), delta);
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::No, m_info.memory, instanceValue(), m_currentBlock);

    return { };
}

auto AirIRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    static_assert(sizeof(decltype(static_cast<Memory*>(nullptr)->size())) == sizeof(uint64_t), "codegen relies on this size");

    auto temp1 = g64();
    auto temp2 = g64();

    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfCachedMemorySize(), B3::Width64));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfCachedMemorySize()), temp1);
    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    append(Move, Arg::imm(16), temp2);
    addShift(Type::I32, Urshift64, temp1, temp2, result);
    append(Move32, result, result);

    return { };
}

auto AirIRGenerator::setLocal(uint32_t index, ExpressionType value) -> PartialResult
{
    ASSERT(m_locals[index].tmp());
    append(moveOpForValueType(m_locals[index].type()), value, m_locals[index].tmp());
    return { };
}

auto AirIRGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    Type type = m_info.globals[index].type;

    result = tmpForType(type);

    auto temp = g64();

    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfGlobals(), B3::Width64));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfGlobals()), temp);

    int32_t offset = safeCast<int32_t>(index * sizeof(Register));
    if (Arg::isValidAddrForm(offset, B3::widthForType(toB3Type(type))))
        append(moveOpForValueType(type), Arg::addr(temp, offset), result);
    else {
        auto temp2 = g64();
        append(Move, Arg::bigImm(offset), temp2);
        append(Add64, temp2, temp, temp);
        append(moveOpForValueType(type), Arg::addr(temp), result);
    }
    return { };
}

auto AirIRGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    auto temp = g64();

    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfGlobals(), B3::Width64));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfGlobals()), temp);

    Type type = m_info.globals[index].type;

    int32_t offset = safeCast<int32_t>(index * sizeof(Register));
    if (Arg::isValidAddrForm(offset, B3::widthForType(toB3Type(type))))
        append(moveOpForValueType(type), value, Arg::addr(temp, offset));
    else {
        auto temp2 = g64();
        append(Move, Arg::bigImm(offset), temp2);
        append(Add64, temp2, temp, temp);
        append(moveOpForValueType(type), value, Arg::addr(temp));
    }

    return { };
}

inline AirIRGenerator::ExpressionType AirIRGenerator::emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOperation)
{
    ASSERT(m_memoryBaseGPR);

    auto result = g64();
    append(Move32, pointer, result);

    switch (m_mode) {
    case MemoryMode::BoundsChecking: {
        // We're not using signal handling at all, we must therefore check that no memory access exceeds the current memory size.
        ASSERT(m_memorySizeGPR);
        ASSERT(sizeOfOperation + offset > offset);
        auto temp = g64();
        append(Move, Arg::bigImm(static_cast<uint64_t>(sizeOfOperation) + offset - 1), temp);
        append(Add64, result, temp);

        emitCheck([&] {
            return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), temp, Tmp(m_memorySizeGPR));
        }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        break;
    }

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
            auto sizeMax = addConstant(Type::I64, maximum);

            emitCheck([&] {
                return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), temp, sizeMax);
            }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
            });
        }
        break;
    }
    }

    append(Add64, Tmp(m_memoryBaseGPR), result);
    return result;
}

inline uint32_t sizeOfLoadOp(LoadOpType op)
{
    switch (op) {
    case LoadOpType::I32Load8S:
    case LoadOpType::I32Load8U:
    case LoadOpType::I64Load8S:
    case LoadOpType::I64Load8U:
        return 1;
    case LoadOpType::I32Load16S:
    case LoadOpType::I64Load16S:
    case LoadOpType::I32Load16U:
    case LoadOpType::I64Load16U:
        return 2;
    case LoadOpType::I32Load:
    case LoadOpType::I64Load32S:
    case LoadOpType::I64Load32U:
    case LoadOpType::F32Load:
        return 4;
    case LoadOpType::I64Load:
    case LoadOpType::F64Load:
        return 8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline TypedTmp AirIRGenerator::emitLoadOp(LoadOpType op, ExpressionType pointer, uint32_t uoffset)
{
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    TypedTmp immTmp;
    TypedTmp newPtr;
    TypedTmp result;

    Arg addrArg;
    if (Arg::isValidAddrForm(offset, B3::widthForBytes(sizeOfLoadOp(op))))
        addrArg = Arg::addr(pointer, offset);
    else {
        immTmp = g64();
        newPtr = g64();
        append(Move, Arg::bigImm(offset), immTmp);
        append(Add64, immTmp, pointer, newPtr);
        addrArg = Arg::addr(newPtr);
    }

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

auto AirIRGenerator::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
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

inline uint32_t sizeOfStoreOp(StoreOpType op)
{
    switch (op) {
    case StoreOpType::I32Store8:
    case StoreOpType::I64Store8:
        return 1;
    case StoreOpType::I32Store16:
    case StoreOpType::I64Store16:
        return 2;
    case StoreOpType::I32Store:
    case StoreOpType::I64Store32:
    case StoreOpType::F32Store:
        return 4;
    case StoreOpType::I64Store:
    case StoreOpType::F64Store:
        return 8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}


inline void AirIRGenerator::emitStoreOp(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    TypedTmp immTmp;
    TypedTmp newPtr;

    Arg addrArg;
    if (Arg::isValidAddrForm(offset, B3::widthForBytes(sizeOfStoreOp(op))))
        addrArg = Arg::addr(pointer, offset);
    else {
        immTmp = g64();
        newPtr = g64();
        append(Move, Arg::bigImm(offset), immTmp);
        append(Add64, immTmp, pointer, newPtr);
        addrArg = Arg::addr(newPtr);
    }

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

auto AirIRGenerator::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
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

auto AirIRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    ASSERT(nonZero.type() == zero.type());
    result = tmpForType(nonZero.type());
    append(moveOpForValueType(nonZero.type()), nonZero, result);

    BasicBlock* isZero = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), condition, condition);
    m_currentBlock->setSuccessors(isZero, continuation);

    append(isZero, moveOpForValueType(zero.type()), zero, result);
    append(isZero, Jump);
    isZero->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

void AirIRGenerator::emitTierUpCheck(uint32_t decrementCount, B3::Origin origin)
{
    UNUSED_PARAM(origin);

    if (!m_tierUp)
        return;

    auto countdownPtr = g64();
    auto oldCountdown = g64();
    auto newCountdown = g64();

    append(Move, Arg::bigImm(reinterpret_cast<uint64_t>(m_tierUp)), countdownPtr);
    append(Move32, Arg::addr(countdownPtr), oldCountdown);

    RELEASE_ASSERT(Arg::isValidImmForm(decrementCount));
    append(Move32, oldCountdown, newCountdown);
    append(Sub32, Arg::imm(decrementCount), newCountdown);
    append(Move32, newCountdown, Arg::addr(countdownPtr));

    auto* patch = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    patch->effects = effects;

    patch->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        MacroAssembler::Jump tierUp = jit.branch32(MacroAssembler::Above, params[0].gpr(), params[1].gpr());
        MacroAssembler::Label tierUpResume = jit.label();

        params.addLatePath([=] (CCallHelpers& jit) {
            tierUp.link(&jit);

            const unsigned extraPaddingBytes = 0;
            RegisterSet registersToSpill = { };
            registersToSpill.add(GPRInfo::argumentGPR1);
            unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

            jit.move(MacroAssembler::TrustedImm32(m_functionIndex), GPRInfo::argumentGPR1);
            MacroAssembler::Call call = jit.nearCall();

            ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToSpill, RegisterSet(), numberOfStackBytesUsedForRegisterPreservation, extraPaddingBytes);
            jit.jump(tierUpResume);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                MacroAssembler::repatchNearCall(linkBuffer.locationOfNearCall<NoPtrTag>(call), CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(triggerOMGTierUpThunkGenerator).code()));

            });
        });
    });

    emitPatchpoint(patch, Tmp(), newCountdown, oldCountdown);
}

AirIRGenerator::ControlData AirIRGenerator::addLoop(Type signature)
{
    BasicBlock* body = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(Jump);
    m_currentBlock->setSuccessors(body);

    m_currentBlock = body;
    emitTierUpCheck(TierUpCount::loopDecrement(), origin());

    return ControlData(origin(), signature, tmpForType(signature), BlockType::Loop, continuation, body);
}

AirIRGenerator::ControlData AirIRGenerator::addTopLevel(Type signature)
{
    return ControlData(B3::Origin(), signature, tmpForType(signature), BlockType::TopLevel, m_code.addBlock());
}

AirIRGenerator::ControlData AirIRGenerator::addBlock(Type signature)
{
    return ControlData(origin(), signature, tmpForType(signature), BlockType::Block, m_code.addBlock());
}

auto AirIRGenerator::addIf(ExpressionType condition, Type signature, ControlType& result) -> PartialResult
{
    BasicBlock* taken = m_code.addBlock();
    BasicBlock* notTaken = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();
    
    // Wasm bools are i32.
    append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), condition, condition);
    m_currentBlock->setSuccessors(taken, notTaken);

    m_currentBlock = taken;
    result = ControlData(origin(), signature, tmpForType(signature), BlockType::If, continuation, notTaken);
    return { };
}

auto AirIRGenerator::addElse(ControlData& data, const ExpressionList& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.result);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);
    return addElseToUnreachable(data);
}

auto AirIRGenerator::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.type() == BlockType::If);
    m_currentBlock = data.special;
    data.convertIfToBlock();
    return { };
}

auto AirIRGenerator::addReturn(const ControlData& data, const ExpressionList& returnValues) -> PartialResult
{
    ASSERT(returnValues.size() <= 1);
    if (returnValues.size()) {
        Tmp returnValueGPR = Tmp(GPRInfo::returnValueGPR);
        Tmp returnValueFPR = Tmp(FPRInfo::returnValueFPR);
        switch (data.signature()) {
        case Type::I32:
            append(Move32, returnValues[0], returnValueGPR);
            append(Ret32, returnValueGPR);
            break;
        case Type::I64:
            append(Move, returnValues[0], returnValueGPR);
            append(Ret64, returnValueGPR);
            break;
        case Type::F32:
            append(MoveFloat, returnValues[0], returnValueFPR);
            append(RetFloat, returnValueFPR);
            break;
        case Type::F64:
            append(MoveDouble, returnValues[0], returnValueFPR);
            append(RetFloat, returnValueFPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    } else
        append(RetVoid);
    return { };
}

// NOTE: All branches in Wasm are on 32-bit ints

auto AirIRGenerator::addBranch(ControlData& data, ExpressionType condition, const ExpressionList& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data.resultForBranch());

    BasicBlock* target = data.targetBlockForBranch();
    if (condition) {
        BasicBlock* continuation = m_code.addBlock();
        append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), condition, condition);
        m_currentBlock->setSuccessors(target, continuation);
        m_currentBlock = continuation;
    } else {
        append(Jump);
        m_currentBlock->setSuccessors(target);
    }

    return { };
}

auto AirIRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const ExpressionList& expressionStack) -> PartialResult
{
    auto& successors = m_currentBlock->successors();
    ASSERT(successors.isEmpty());
    for (const auto& target : targets) {
        unifyValuesWithBlock(expressionStack, target->resultForBranch());
        successors.append(target->targetBlockForBranch());
    }
    unifyValuesWithBlock(expressionStack, defaultTarget.resultForBranch());
    successors.append(defaultTarget.targetBlockForBranch());

    ASSERT(condition.type() == Type::I32);

    // FIXME: We should consider dynamically switching between a jump table
    // and a binary switch depending on the number of successors.
    // https://bugs.webkit.org/show_bug.cgi?id=194477

    size_t numTargets = targets.size();

    auto* patchpoint = addPatchpoint(B3::Void);
    patchpoint->effects = B3::Effects::none();
    patchpoint->effects.terminal = true;
    patchpoint->clobber(RegisterSet::macroScratchRegisters());

    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);

        Vector<int64_t> cases;
        cases.reserveInitialCapacity(numTargets);
        for (size_t i = 0; i < numTargets; ++i)
            cases.uncheckedAppend(i);

        GPRReg valueReg = params[0].gpr();
        BinarySwitch binarySwitch(valueReg, cases, BinarySwitch::Int32);

        Vector<CCallHelpers::Jump> caseJumps;
        caseJumps.resize(numTargets);

        while (binarySwitch.advance(jit)) {
            unsigned value = binarySwitch.caseValue();
            unsigned index = binarySwitch.caseIndex();
            ASSERT_UNUSED(value, value == index);
            ASSERT(index < numTargets);
            caseJumps[index] = jit.jump();
        }

        CCallHelpers::JumpList fallThrough = binarySwitch.fallThrough();

        Vector<Box<CCallHelpers::Label>> successorLabels = params.successorLabels();
        ASSERT(successorLabels.size() == caseJumps.size() + 1);

        params.addLatePath([=, caseJumps = WTFMove(caseJumps), successorLabels = WTFMove(successorLabels)] (CCallHelpers& jit) {
            for (size_t i = 0; i < numTargets; ++i)
                caseJumps[i].linkTo(*successorLabels[i], &jit);                
            fallThrough.linkTo(*successorLabels[numTargets], &jit);
        });
    });

    emitPatchpoint(patchpoint, TypedTmp(), condition);

    return { };
}

auto AirIRGenerator::endBlock(ControlEntry& entry, ExpressionList& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    unifyValuesWithBlock(expressionStack, data.result);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);

    return addEndToUnreachable(entry);
}


auto AirIRGenerator::addEndToUnreachable(ControlEntry& entry) -> PartialResult
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;

    if (data.type() == BlockType::If) {
        append(data.special, Jump);
        data.special->setSuccessors(m_currentBlock);
    }

    for (const auto& result : data.result)
        entry.enclosedExpressionStack.append(result);

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.type() == BlockType::TopLevel)
        return addReturn(data, entry.enclosedExpressionStack);

    return { };
}

auto AirIRGenerator::addCall(uint32_t functionIndex, const Signature& signature, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    ASSERT(signature.argumentCount() == args.size());

    m_makesCalls = true;

    Type returnType = signature.returnType();
    if (returnType != Type::Void)
        result = tmpForType(returnType);

    Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
        m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

        auto currentInstance = g64();
        append(Move, instanceValue(), currentInstance);

        auto targetInstance = g64();

        // FIXME: We should have better isel here.
        // https://bugs.webkit.org/show_bug.cgi?id=193999
        append(Move, Arg::bigImm(Instance::offsetOfTargetInstance(functionIndex)), targetInstance);
        append(Add64, instanceValue(), targetInstance);
        append(Move, Arg::addr(targetInstance), targetInstance);

        BasicBlock* isWasmBlock = m_code.addBlock();
        BasicBlock* isEmbedderBlock = m_code.addBlock();
        BasicBlock* continuation = m_code.addBlock();

        append(BranchTest64, Arg::resCond(MacroAssembler::NonZero), targetInstance, targetInstance);
        m_currentBlock->setSuccessors(isWasmBlock, isEmbedderBlock);

        {
            auto* patchpoint = addPatchpoint(toB3Type(returnType));
            patchpoint->effects.writesPinned = true;
            patchpoint->effects.readsPinned = true;
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

            Vector<ConstrainedTmp> patchArgs;
            wasmCallingConventionAir().setupCall(m_code, returnType, patchpoint, toTmpVector(args), [&] (Tmp tmp, B3::ValueRep rep) {
                patchArgs.append({ tmp, rep });
            });

            patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CCallHelpers::Call call = jit.threadSafePatchableNearCall();
                jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                    unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
                });
            });

            emitPatchpoint(isWasmBlock, patchpoint, result, WTFMove(patchArgs));
            append(isWasmBlock, Jump);
            isWasmBlock->setSuccessors(continuation);
        }

        {
            auto jumpDestination = g64();
            append(isEmbedderBlock, Move, Arg::bigImm(Instance::offsetOfWasmToEmbedderStub(functionIndex)), jumpDestination);
            append(isEmbedderBlock, Add64, instanceValue(), jumpDestination);
            append(isEmbedderBlock, Move, Arg::addr(jumpDestination), jumpDestination);

            auto* patchpoint = addPatchpoint(toB3Type(returnType));
            patchpoint->effects.writesPinned = true;
            patchpoint->effects.readsPinned = true;
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

            Vector<ConstrainedTmp> patchArgs;
            patchArgs.append(jumpDestination);

            wasmCallingConventionAir().setupCall(m_code, returnType, patchpoint, toTmpVector(args), [&] (Tmp tmp, B3::ValueRep rep) {
                patchArgs.append({ tmp, rep });
            });

            patchpoint->setGenerator([returnType] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                jit.call(params[returnType == Void ? 0 : 1].gpr(), WasmEntryPtrTag);
            });

            emitPatchpoint(isEmbedderBlock, patchpoint, result, WTFMove(patchArgs));
            append(isEmbedderBlock, Jump);
            isEmbedderBlock->setSuccessors(continuation);
        }

        m_currentBlock = continuation;
        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, currentInstance, continuation);
    } else {
        auto* patchpoint = addPatchpoint(toB3Type(returnType));
        patchpoint->effects.writesPinned = true;
        patchpoint->effects.readsPinned = true;

        Vector<ConstrainedTmp> patchArgs;
        wasmCallingConventionAir().setupCall(m_code, returnType, patchpoint, toTmpVector(args), [&] (Tmp tmp, B3::ValueRep rep) {
            patchArgs.append({ tmp, rep });
        });

        patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CCallHelpers::Call call = jit.threadSafePatchableNearCall();
            jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
            });
        });

        emitPatchpoint(m_currentBlock, patchpoint, result, WTFMove(patchArgs));
    }

    return { };
}

auto AirIRGenerator::addCallIndirect(const Signature& signature, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();
    ASSERT(signature.argumentCount() == args.size());

    m_makesCalls = true;
    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    auto currentInstance = g64();
    append(Move, instanceValue(), currentInstance);

    ExpressionType callableFunctionBuffer = g64();
    ExpressionType instancesBuffer = g64();
    ExpressionType callableFunctionBufferLength = g64();
    {
        RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfTable(), B3::Width64));
        RELEASE_ASSERT(Arg::isValidAddrForm(Table::offsetOfFunctions(), B3::Width64));
        RELEASE_ASSERT(Arg::isValidAddrForm(Table::offsetOfInstances(), B3::Width64));
        RELEASE_ASSERT(Arg::isValidAddrForm(Table::offsetOfLength(), B3::Width64));

        append(Move, Arg::addr(instanceValue(), Instance::offsetOfTable()), callableFunctionBufferLength);
        append(Move, Arg::addr(callableFunctionBufferLength, Table::offsetOfFunctions()), callableFunctionBuffer);
        append(Move, Arg::addr(callableFunctionBufferLength, Table::offsetOfInstances()), instancesBuffer);
        append(Move32, Arg::addr(callableFunctionBufferLength, Table::offsetOfLength()), callableFunctionBufferLength);
    }

    append(Move32, calleeIndex, calleeIndex);

    // Check the index we are looking for is valid.
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), calleeIndex, callableFunctionBufferLength);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsCallIndirect);
    });

    ExpressionType calleeCode = g64();
    {
        ExpressionType calleeSignatureIndex = g64();
        // Compute the offset in the table index space we are looking for.
        append(Move, Arg::imm(sizeof(WasmToWasmImportableFunction)), calleeSignatureIndex);
        append(Mul64, calleeIndex, calleeSignatureIndex);
        append(Add64, callableFunctionBuffer, calleeSignatureIndex);
        
        append(Move, Arg::addr(calleeSignatureIndex, WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation()), calleeCode); // Pointer to callee code.

        // Check that the WasmToWasmImportableFunction is initialized. We trap if it isn't. An "invalid" SignatureIndex indicates it's not initialized.
        // FIXME: when we have trap handlers, we can just let the call fail because Signature::invalidIndex is 0. https://bugs.webkit.org/show_bug.cgi?id=177210
        static_assert(sizeof(WasmToWasmImportableFunction::signatureIndex) == sizeof(uint64_t), "Load codegen assumes i64");

        // FIXME: This seems dumb to do two checks just for a nicer error message.
        // We should move just to use a single branch and then figure out what
        // error to use in the exception handler.

        append(Move, Arg::addr(calleeSignatureIndex, WasmToWasmImportableFunction::offsetOfSignatureIndex()), calleeSignatureIndex);

        emitCheck([&] {
            static_assert(Signature::invalidIndex == 0, "");
            return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::Zero), calleeSignatureIndex, calleeSignatureIndex);
        }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::NullTableEntry);
        });

        ExpressionType expectedSignatureIndex = g64();
        append(Move, Arg::bigImm(SignatureInformation::get(signature)), expectedSignatureIndex);
        emitCheck([&] {
            return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::NotEqual), calleeSignatureIndex, expectedSignatureIndex);
        }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::BadSignature);
        });
    }

    // Do a context switch if needed.
    {
        auto newContextInstance = g64();
        append(Move, Arg::index(instancesBuffer, calleeIndex, 8, 0), newContextInstance);

        BasicBlock* doContextSwitch = m_code.addBlock();
        BasicBlock* continuation = m_code.addBlock();

        append(Branch64, Arg::relCond(MacroAssembler::Equal), newContextInstance, instanceValue());
        m_currentBlock->setSuccessors(continuation, doContextSwitch);

        auto* patchpoint = addPatchpoint(B3::Void);
        patchpoint->effects.writesPinned = true;
        // We pessimistically assume we're calling something with BoundsChecking memory.
        // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
        patchpoint->clobber(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg newContextInstance = params[0].gpr();
            GPRReg oldContextInstance = params[1].gpr();
            const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
            const auto& sizeRegs = pinnedRegs.sizeRegisters;
            GPRReg baseMemory = pinnedRegs.baseMemoryPointer;
            ASSERT(newContextInstance != baseMemory);
            jit.loadPtr(CCallHelpers::Address(oldContextInstance, Instance::offsetOfCachedStackLimit()), baseMemory);
            jit.storePtr(baseMemory, CCallHelpers::Address(newContextInstance, Instance::offsetOfCachedStackLimit()));
            jit.storeWasmContextInstance(newContextInstance);
            ASSERT(sizeRegs[0].sizeRegister != baseMemory);
            // FIXME: We should support more than one memory size register
            //   see: https://bugs.webkit.org/show_bug.cgi?id=162952
            ASSERT(sizeRegs.size() == 1);
            ASSERT(sizeRegs[0].sizeRegister != newContextInstance);
            ASSERT(!sizeRegs[0].sizeOffset);
            jit.loadPtr(CCallHelpers::Address(newContextInstance, Instance::offsetOfCachedMemorySize()), sizeRegs[0].sizeRegister); // Memory size.
            jit.loadPtr(CCallHelpers::Address(newContextInstance, Instance::offsetOfCachedMemory()), baseMemory); // Memory::void*.
        });

        emitPatchpoint(doContextSwitch, patchpoint, Tmp(), newContextInstance, instanceValue());
        append(doContextSwitch, Jump);
        doContextSwitch->setSuccessors(continuation);

        m_currentBlock = continuation;
    }

    append(Move, Arg::addr(calleeCode), calleeCode);

    Type returnType = signature.returnType();
    if (returnType != Type::Void)
        result = tmpForType(returnType);

    auto* patch = addPatchpoint(toB3Type(returnType));
    patch->effects.writesPinned = true;
    patch->effects.readsPinned = true;
    // We need to clobber all potential pinned registers since we might be leaving the instance.
    // We pessimistically assume we're always calling something that is bounds checking so
    // because the wasm->wasm thunk unconditionally overrides the size registers.
    // FIXME: We should not have to do this, but the wasm->wasm stub assumes it can
    // use all the pinned registers as scratch: https://bugs.webkit.org/show_bug.cgi?id=172181
    patch->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

    Vector<ConstrainedTmp> emitArgs;
    emitArgs.append(calleeCode);
    wasmCallingConventionAir().setupCall(m_code, returnType, patch, toTmpVector(args), [&] (Tmp tmp, B3::ValueRep rep) {
        emitArgs.append({ tmp, rep });
    });
    patch->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.call(params[returnType == Void ? 0 : 1].gpr(), WasmEntryPtrTag);
    });

    emitPatchpoint(m_currentBlock, patch, result, WTFMove(emitArgs));

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, currentInstance, m_currentBlock);

    return { };
}

void AirIRGenerator::unify(const ExpressionType& dst, const ExpressionType& source)
{
    ASSERT(dst.type() == source.type());
    append(moveOpForValueType(dst.type()), source, dst);
}

void AirIRGenerator::unifyValuesWithBlock(const ExpressionList& resultStack, const ResultList& result)
{
    ASSERT(result.size() <= resultStack.size());

    for (size_t i = 0; i < result.size(); ++i)
        unify(result[result.size() - 1 - i], resultStack[resultStack.size() - 1 - i]);
}

void AirIRGenerator::dump(const Vector<ControlEntry>&, const ExpressionList*)
{
}

auto AirIRGenerator::origin() -> B3::Origin
{
    // FIXME: We should implement a way to give Inst's an origin.
    return B3::Origin();
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileAir(CompilationContext& compilationContext, const uint8_t* functionStart, size_t functionLength, const Signature& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, uint32_t functionIndex, TierUpCount* tierUp, ThrowWasmException throwWasmException)
{
    auto result = std::make_unique<InternalFunction>();

    compilationContext.embedderEntrypointJIT = std::make_unique<CCallHelpers>();
    compilationContext.wasmEntrypointJIT = std::make_unique<CCallHelpers>();

    B3::Procedure procedure;
    Code& code = procedure.code();

    procedure.setOriginPrinter([] (PrintStream& out, B3::Origin origin) {
        if (origin.data())
            out.print("Wasm: ", bitwise_cast<OpcodeOrigin>(origin));
    });
    
    // This means we cannot use either StackmapGenerationParams::usedRegisters() or
    // StackmapGenerationParams::unavailableRegisters(). In exchange for this concession, we
    // don't strictly need to run Air::reportUsedRegisters(), which saves a bit of CPU time at
    // optLevel=1.
    procedure.setNeedsUsedRegisters(false);
    
    procedure.setOptLevel(Options::webAssemblyBBQOptimizationLevel());

    AirIRGenerator irGenerator(info, procedure, result.get(), unlinkedWasmToWasmCalls, mode, functionIndex, tierUp, throwWasmException, signature);
    FunctionParser<AirIRGenerator> parser(irGenerator, functionStart, functionLength, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());


    for (BasicBlock* block : code) {
        for (size_t i = 0; i < block->numSuccessors(); ++i)
            block->successorBlock(i)->addPredecessor(block);
    }

    {
        B3::Air::prepareForGeneration(code);
        B3::Air::generate(code, *compilationContext.wasmEntrypointJIT);
        compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
        result->entrypoint.calleeSaveRegisters = code.calleeSaveRegisterAtOffsetList();
    }

    return WTFMove(result);
}

template <typename IntType>
void AirIRGenerator::emitChecksForModOrDiv(bool isSignedDiv, ExpressionType left, ExpressionType right)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8, "");

    emitCheck([&] {
        return Inst(sizeof(IntType) == 4 ? BranchTest32 : BranchTest64, nullptr, Arg::resCond(MacroAssembler::Zero), right, right);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::DivisionByZero);
    });

    if (isSignedDiv) {
        ASSERT(std::is_signed<IntType>::value);
        IntType min = std::numeric_limits<IntType>::min();

        // FIXME: Better isel for compare with imms here.
        // https://bugs.webkit.org/show_bug.cgi?id=193999
        auto minTmp = sizeof(IntType) == 4 ? g32() : g64();
        auto negOne = sizeof(IntType) == 4 ? g32() : g64();

        B3::Air::Opcode op = sizeof(IntType) == 4 ? Compare32 : Compare64;
        append(Move, Arg::bigImm(static_cast<uint64_t>(min)), minTmp);
        append(op, Arg::relCond(MacroAssembler::Equal), left, minTmp, minTmp);

        append(Move, Arg::imm(-1), negOne);
        append(op, Arg::relCond(MacroAssembler::Equal), right, negOne, negOne);

        emitCheck([&] {
            return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), minTmp, negOne);
        },
        [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::IntegerOverflow);
        });
    }
}

template <typename IntType>
void AirIRGenerator::emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8, "");

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

#if CPU(X86) || CPU(X86_64)
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
        auto one = addConstant(sizeof(IntType) == 4 ? Type::I32 : Type::I64, 1);

        append(sizeof(IntType) == 4 ? Add32 : Add64, rhs, one, temp);
        append(sizeof(IntType) == 4 ? Branch32 : Branch64, Arg::relCond(MacroAssembler::Above), temp, one);
        m_currentBlock->setSuccessors(denIsGood, denMayBeBad);

        append(denMayBeBad, Xor64, result, result);
        append(denMayBeBad, sizeof(IntType) == 4 ? BranchTest32 : BranchTest64, Arg::resCond(MacroAssembler::Zero), rhs, rhs);
        denMayBeBad->setSuccessors(continuation, denNotZero);

        auto min = addConstant(denNotZero, sizeof(IntType) == 4 ? Type::I32 : Type::I64, std::numeric_limits<IntType>::min());
        if (isDiv)
            append(denNotZero, sizeof(IntType) == 4 ? Move32 : Move, min, result);
        else {
            // Result is zero, as set above...
        }
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

template<>
auto AirIRGenerator::addOp<OpType::I32DivS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<int32_t>(true, left, right);
    emitModOrDiv<int32_t>(true, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32RemS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<int32_t>(false, left, right);
    emitModOrDiv<int32_t>(false, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32DivU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<uint32_t>(false, left, right);
    emitModOrDiv<uint32_t>(true, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32RemU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<uint32_t>(false, left, right);
    emitModOrDiv<uint32_t>(false, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64DivS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<int64_t>(true, left, right);
    emitModOrDiv<int64_t>(true, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64RemS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<int64_t>(false, left, right);
    emitModOrDiv<int64_t>(false, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64DivU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<uint64_t>(false, left, right);
    emitModOrDiv<uint64_t>(true, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64RemU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    emitChecksForModOrDiv<uint64_t>(false, left, right);
    emitModOrDiv<uint64_t>(false, left, right, result);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32Ctz>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    result = g32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64Ctz>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.countTrailingZeros64(params[1].gpr(), params[0].gpr());
    });
    result = g64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32Popcnt>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g32();

#if CPU(X86_64)
    if (MacroAssembler::supportsCountPopulation()) {
        auto* patchpoint = addPatchpoint(B3::Int32);
        patchpoint->effects = B3::Effects::none();
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            jit.countPopulation32(params[1].gpr(), params[0].gpr());
        });
        emitPatchpoint(patchpoint, result, arg);
        return { };
    }
#endif

    uint32_t (*popcount)(int32_t) = [] (int32_t value) -> uint32_t { return __builtin_popcount(value); };
    emitCCall(popcount, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64Popcnt>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();

#if CPU(X86_64)
    if (MacroAssembler::supportsCountPopulation()) {
        auto* patchpoint = addPatchpoint(B3::Int64);
        patchpoint->effects = B3::Effects::none();
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            jit.countPopulation64(params[1].gpr(), params[0].gpr());
        });
        emitPatchpoint(patchpoint, result, arg);
        return { };
    }
#endif

    uint64_t (*popcount)(int64_t) = [] (int64_t value) -> uint64_t { return __builtin_popcountll(value); };
    emitCCall(popcount, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<F64ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Double);
    patchpoint->effects = B3::Effects::none();
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
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

template<>
auto AirIRGenerator::addOp<OpType::F32ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
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

template<>
auto AirIRGenerator::addOp<OpType::F64Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Double);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.roundTowardNearestIntDouble(params[1].fpr(), params[0].fpr());
    });
    result = f64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F32Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.roundTowardNearestIntFloat(params[1].fpr(), params[0].fpr());
    });
    result = f32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F64Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Double);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
    });
    result = f64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F32Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.roundTowardZeroFloat(params[1].fpr(), params[0].fpr());
    });
    result = f32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32TruncSF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
    auto min = addConstant(Type::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min())));

    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanOrUnordered), arg, min, temp1);
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
    });
    result = g32();
    emitPatchpoint(patchpoint, result, arg);

    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
    auto min = addConstant(Type::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));

    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanOrUnordered), arg, min, temp1);
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
    });
    result = g32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}


template<>
auto AirIRGenerator::addOp<OpType::I32TruncUF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
    auto min = addConstant(Type::F64, bitwise_cast<uint64_t>(-1.0));

    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualOrUnordered), arg, min, temp1);
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
    });
    result = g32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32TruncUF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
    auto min = addConstant(Type::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));

    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualOrUnordered), arg, min, temp1);
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
    });
    result = g32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64TruncSF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
    auto min = addConstant(Type::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));

    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanOrUnordered), arg, min, temp1);
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
    });

    result = g64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64TruncUF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
    auto min = addConstant(Type::F64, bitwise_cast<uint64_t>(-1.0));
    
    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualOrUnordered), arg, min, temp1);
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    TypedTmp signBitConstant;
    if (isX86())
        signBitConstant = addConstant(Type::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));

    Vector<ConstrainedTmp> args;
    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    args.append(arg);
    if (isX86()) {
        args.append(signBitConstant);
        patchpoint->numFPScratchRegisters = 1;
    }
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        FPRReg scratch = InvalidFPRReg;
        FPRReg constant = InvalidFPRReg;
        if (isX86()) {
            scratch = params.fpScratch(0);
            constant = params[2].fpr();
        }
        jit.truncateDoubleToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
    });

    result = g64();
    emitPatchpoint(m_currentBlock, patchpoint, result, WTFMove(args));
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
    auto min = addConstant(Type::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));

    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanOrUnordered), arg, min, temp1);
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
    });
    result = g64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64TruncUF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Type::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
    auto min = addConstant(Type::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
    
    auto temp1 = g32();
    auto temp2 = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualOrUnordered), arg, min, temp1);
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, max, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    TypedTmp signBitConstant;
    if (isX86())
        signBitConstant = addConstant(Type::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));

    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    Vector<ConstrainedTmp> args;
    args.append(arg);
    if (isX86()) {
        args.append(signBitConstant);
        patchpoint->numFPScratchRegisters = 1;
    }
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        FPRReg scratch = InvalidFPRReg;
        FPRReg constant = InvalidFPRReg;
        if (isX86()) {
            scratch = params.fpScratch(0);
            constant = params[2].fpr();
        }
        jit.truncateFloatToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
    });

    result = g64();
    emitPatchpoint(m_currentBlock, patchpoint, result, WTFMove(args));

    return { };
}

auto AirIRGenerator::addShift(Type type, B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    ASSERT(type == Type::I64 || type == Type::I32);
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

auto AirIRGenerator::addIntegerSub(B3::Air::Opcode op, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
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

auto AirIRGenerator::addFloatingPointAbs(B3::Air::Opcode op, ExpressionType value, ExpressionType& result) -> PartialResult
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

auto AirIRGenerator::addFloatingPointBinOp(Type type, B3::Air::Opcode op, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(type == Type::F32 || type == Type::F64);
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

template<> auto AirIRGenerator::addOp<OpType::F32Ceil>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(CeilFloat, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Mul>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Mul32, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Sub>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addIntegerSub(Sub32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Le>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32DemoteF64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(ConvertDoubleToFloat, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Min>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f32();

    BasicBlock* isEqual = m_code.addBlock();
    BasicBlock* notEqual = m_code.addBlock();
    BasicBlock* greaterThanOrEqual = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(m_currentBlock, BranchFloat, Arg::doubleCond(MacroAssembler::DoubleEqual), arg0, arg1);
    m_currentBlock->setSuccessors(isEqual, notEqual);

    append(isEqual, OrFloat, arg0, arg1, result);
    append(isEqual, Jump);
    isEqual->setSuccessors(continuation);

    append(notEqual, MoveFloat, arg0, result);
    append(notEqual, BranchFloat, Arg::doubleCond(MacroAssembler::DoubleLessThan), arg0, arg1);
    notEqual->setSuccessors(continuation, greaterThanOrEqual);

    append(greaterThanOrEqual, MoveFloat, arg1, result);
    append(greaterThanOrEqual, Jump);
    greaterThanOrEqual->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Ne>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleNotEqualOrUnordered), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Lt>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Max>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f32();

    BasicBlock* isEqual = m_code.addBlock();
    BasicBlock* notEqual = m_code.addBlock();
    BasicBlock* lessThan = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(m_currentBlock, BranchFloat, Arg::doubleCond(MacroAssembler::DoubleEqual), arg0, arg1);
    m_currentBlock->setSuccessors(isEqual, notEqual);

    append(isEqual, AndFloat, arg0, arg1, result);
    append(isEqual, Jump);
    isEqual->setSuccessors(continuation);

    append(notEqual, MoveFloat, arg0, result);
    append(notEqual, BranchFloat, Arg::doubleCond(MacroAssembler::DoubleLessThan), arg0, arg1);
    notEqual->setSuccessors(lessThan, continuation);

    append(lessThan, MoveFloat, arg1, result);
    append(lessThan, Jump);
    lessThan->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Mul>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointBinOp(Type::F64, MulDouble, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Div>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointBinOp(Type::F32, DivFloat, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32Clz>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CountLeadingZeros32, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Copysign>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    // FIXME: We can have better codegen here for the imms and two operand forms on x86
    // https://bugs.webkit.org/show_bug.cgi?id=193999
    result = f32();
    auto temp1 = g32();
    auto sign = g32();
    auto value = g32();

    // FIXME: Try to use Imm where possible:
    // https://bugs.webkit.org/show_bug.cgi?id=193999
    append(MoveFloatTo32, arg1, temp1);
    append(Move, Arg::bigImm(0x80000000), sign);
    append(And32, temp1, sign, sign);

    append(MoveDoubleTo64, arg0, temp1);
    append(Move, Arg::bigImm(0x7fffffff), value);
    append(And32, temp1, value, value);

    append(Or32, sign, value, value);
    append(Move32ToFloat, value, result);

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64ConvertUI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    auto temp = g64();
    append(Move32, arg0, temp);
    append(ConvertInt64ToDouble, temp, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32ReinterpretI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(Move32ToFloat, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64And>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(And64, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Ne>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleNotEqualOrUnordered), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Gt>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Sqrt>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(SqrtFloat, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Ge>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64GtS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::GreaterThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64GtU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::Above), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Eqz>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Test64, Arg::resCond(MacroAssembler::Zero), arg0, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Div>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointBinOp(Type::F64, DivDouble, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Add>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(AddFloat, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Or>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Or64, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32LeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::BelowOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32LeS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::LessThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Ne>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::NotEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Clz>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(CountLeadingZeros64, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Neg>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    if (isValidForm(NegateFloat, Arg::Tmp, Arg::Tmp))
        append(NegateFloat, arg0, result);
    else {
        auto constant = addConstant(Type::I32, bitwise_cast<uint32_t>(static_cast<float>(-0.0)));
        auto temp = g32();
        append(MoveFloatTo32, arg0, temp);
        append(Xor32, constant, temp);
        append(Move32ToFloat, temp, result);
    }
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32And>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(And32, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32LtU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::Below), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Rotr>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I64, RotateRight64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Abs>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    return addFloatingPointAbs(AbsDouble, arg0, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32LtS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::LessThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Eq>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::Equal), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Copysign>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    // FIXME: We can have better codegen here for the imms and two operand forms on x86
    // https://bugs.webkit.org/show_bug.cgi?id=193999
    result = f64();
    auto temp1 = g64();
    auto sign = g64();
    auto value = g64();

    append(MoveDoubleTo64, arg1, temp1);
    append(Move, Arg::bigImm(0x8000000000000000), sign);
    append(And64, temp1, sign, sign);

    append(MoveDoubleTo64, arg0, temp1);
    append(Move, Arg::bigImm(0x7fffffffffffffff), value);
    append(And64, temp1, value, value);

    append(Or64, sign, value, value);
    append(Move64ToDouble, value, result);

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32ConvertSI64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(ConvertInt64ToFloat, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Rotl>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    if (isARM64()) {
        // ARM64 doesn't have a rotate left.
        auto newShift = g64();
        append(Move, arg1, newShift);
        append(Neg64, newShift);
        return addShift(Type::I64, RotateRight64, arg0, newShift, result);
    } else
        return addShift(Type::I64, RotateLeft64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Lt>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64ConvertSI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(ConvertInt32ToDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Eq>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Le>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Ge>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32ShrU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I32, Urshift32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32ConvertUI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    auto temp = g64();
    append(Move32, arg0, temp);
    append(ConvertInt64ToFloat, temp, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32ShrS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I32, Rshift32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32GeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::AboveOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Ceil>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(CeilDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32GeS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::GreaterThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Shl>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I32, Lshift32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Floor>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(FloorDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Xor>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Xor32, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Abs>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    return addFloatingPointAbs(AbsFloat, arg0, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Min>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f64();

    BasicBlock* isEqual = m_code.addBlock();
    BasicBlock* notEqual = m_code.addBlock();
    BasicBlock* greaterThanOrEqual = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(m_currentBlock, BranchDouble, Arg::doubleCond(MacroAssembler::DoubleEqual), arg0, arg1);
    m_currentBlock->setSuccessors(isEqual, notEqual);

    append(isEqual, OrDouble, arg0, arg1, result);
    append(isEqual, Jump);
    isEqual->setSuccessors(continuation);

    append(notEqual, MoveDouble, arg0, result);
    append(notEqual, BranchDouble, Arg::doubleCond(MacroAssembler::DoubleLessThan), arg0, arg1);
    notEqual->setSuccessors(continuation, greaterThanOrEqual);

    append(greaterThanOrEqual, MoveDouble, arg1, result);
    append(greaterThanOrEqual, Jump);
    greaterThanOrEqual->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Mul>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(MulFloat, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Sub>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addIntegerSub(Sub64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32ReinterpretF32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(MoveFloatTo32, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Add>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Add32, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Sub>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointBinOp(Type::F64, SubDouble, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32Or>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Or32, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64LtU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::Below), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64LtS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::LessThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64ConvertSI64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(ConvertInt64ToDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Xor>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Xor64, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64GeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::AboveOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Mul>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Mul64, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Sub>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f32();
    if (isValidForm(SubFloat, Arg::Tmp, Arg::Tmp, Arg::Tmp))
        append(SubFloat, arg0, arg1, result);
    else {
        RELEASE_ASSERT(isX86());
        append(MoveFloat, arg0, result);
        append(SubFloat, arg1, result);
    }
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64PromoteF32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(ConvertFloatToDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Add>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(AddDouble, arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64GeS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::GreaterThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64ExtendUI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Move32, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Ne>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    RELEASE_ASSERT(arg0 && arg1);
    append(Compare32, Arg::relCond(MacroAssembler::NotEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64ReinterpretI64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(Move64ToDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Eq>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Eq>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::Equal), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Floor>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(FloorFloat, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32ConvertSI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    append(ConvertInt32ToFloat, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Eqz>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Test32, Arg::resCond(MacroAssembler::Zero), arg0, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64ReinterpretF64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(MoveDoubleTo64, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64ShrS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I64, Rshift64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64ShrU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I64, Urshift64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Sqrt>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(SqrtDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Shl>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I64, Lshift64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Gt>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32WrapI64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Move32, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Rotl>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    if (isARM64()) {
        // ARM64 doesn't have a rotate left.
        auto newShift = g64();
        append(Move, arg1, newShift);
        append(Neg64, newShift);
        return addShift(Type::I32, RotateRight32, arg0, newShift, result);
    } else
        return addShift(Type::I32, RotateLeft32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32Rotr>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Type::I32, RotateRight32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32GtU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::Above), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64ExtendSI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(SignExtend32ToPtr, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32GtS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare32, Arg::relCond(MacroAssembler::GreaterThan), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Neg>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    if (isValidForm(NegateDouble, Arg::Tmp, Arg::Tmp))
        append(NegateDouble, arg0, result);
    else {
        auto constant = addConstant(Type::I64, bitwise_cast<uint64_t>(static_cast<double>(-0.0)));
        auto temp = g64();
        append(MoveDoubleTo64, arg0, temp);
        append(Xor64, constant, temp);
        append(Move64ToDouble, temp, result);
    }
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Max>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = f64();

    BasicBlock* isEqual = m_code.addBlock();
    BasicBlock* notEqual = m_code.addBlock();
    BasicBlock* lessThan = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(m_currentBlock, BranchDouble, Arg::doubleCond(MacroAssembler::DoubleEqual), arg0, arg1);
    m_currentBlock->setSuccessors(isEqual, notEqual);

    append(isEqual, AndDouble, arg0, arg1, result);
    append(isEqual, Jump);
    isEqual->setSuccessors(continuation);

    append(notEqual, MoveDouble, arg0, result);
    append(notEqual, BranchDouble, Arg::doubleCond(MacroAssembler::DoubleLessThan), arg0, arg1);
    notEqual->setSuccessors(lessThan, continuation);

    append(lessThan, MoveDouble, arg1, result);
    append(lessThan, Jump);
    lessThan->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64LeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::BelowOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64LeS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(Compare64, Arg::relCond(MacroAssembler::LessThanOrEqual), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Add>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
    append(Add64, arg0, arg1, result);
    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
