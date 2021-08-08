/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY_B3JIT)

#include "AirCode.h"
#include "AirGenerate.h"
#include "AirHelpers.h"
#include "AirOpcodeUtils.h"
#include "AllowMacroScratchRegisterUsageIf.h"
#include "B3CheckSpecial.h"
#include "B3CheckValue.h"
#include "B3Commutativity.h"
#include "B3PatchpointSpecial.h"
#include "B3Procedure.h"
#include "B3ProcedureInlines.h"
#include "B3StackmapGenerationParams.h"
#include "BinarySwitch.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "ScratchRegisterAllocator.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmExceptionType.h"
#include "WasmFunctionParser.h"
#include "WasmInstance.h"
#include "WasmMemory.h"
#include "WasmOSREntryData.h"
#include "WasmOpcodeOrigin.h"
#include "WasmOperations.h"
#include "WasmSignatureInlines.h"
#include "WasmThunks.h"
#include <limits>
#include <wtf/Box.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

using namespace B3::Air;

struct ConstrainedTmp {
    ConstrainedTmp() = default;
    ConstrainedTmp(Tmp tmp)
        : ConstrainedTmp(tmp, tmp.isReg() ? B3::ValueRep::reg(tmp.reg()) : B3::ValueRep::SomeRegister)
    { }

    ConstrainedTmp(Tmp tmp, B3::ValueRep rep)
        : tmp(tmp)
        , rep(rep)
    {
    }

    explicit operator bool() const { return !!tmp; }

    Tmp tmp;
    B3::ValueRep rep;
};

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

    void dump(PrintStream& out) const
    {
        out.print("(", m_tmp, ", ", m_type.kind, ", ", m_type.index, ")");
    }

private:

    Tmp m_tmp;
    Type m_type;
};

class AirIRGenerator {
public:
    using ExpressionType = TypedTmp;
    using ResultList = Vector<ExpressionType, 8>;

    struct ControlData {
        ControlData(B3::Origin origin, BlockSignature result, ResultList resultTmps, BlockType type, BasicBlock* continuation, BasicBlock* special = nullptr)
            : controlBlockType(type)
            , continuation(continuation)
            , special(special)
            , results(resultTmps)
            , returnType(result)
        {
            UNUSED_PARAM(origin);
        }

        ControlData()
        {
        }

        static bool isIf(const ControlData& control) { return control.blockType() == BlockType::If; }
        static bool isTopLevel(const ControlData& control) { return control.blockType() == BlockType::TopLevel; }

        void dump(PrintStream& out) const
        {
            switch (blockType()) {
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

            CommaPrinter comma(", ", " Result Tmps: [");
            for (const auto& tmp : results)
                out.print(comma, tmp);
            if (comma.didPrint())
                out.print("]");
        }

        BlockType blockType() const { return controlBlockType; }
        BlockSignature signature() const { return returnType; }

        BasicBlock* targetBlockForBranch()
        {
            if (blockType() == BlockType::Loop)
                return special;
            return continuation;
        }

        void convertIfToBlock()
        {
            ASSERT(blockType() == BlockType::If);
            controlBlockType = BlockType::Block;
            special = nullptr;
        }

        SignatureArgCount branchTargetArity() const
        {
            if (blockType() == BlockType::Loop)
                return returnType->argumentCount();
            return returnType->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (blockType() == BlockType::Loop)
                return returnType->argument(i);
            return returnType->returnType(i);
        }

    private:
        friend class AirIRGenerator;
        BlockType controlBlockType;
        BasicBlock* continuation;
        BasicBlock* special;
        ResultList results;
        BlockSignature returnType;
    };

    using ControlType = ControlData;

    using ControlEntry = FunctionParser<AirIRGenerator>::ControlEntry;
    using ControlStack = FunctionParser<AirIRGenerator>::ControlStack;
    using Stack = FunctionParser<AirIRGenerator>::Stack;
    using TypedExpression = FunctionParser<AirIRGenerator>::TypedExpression;

    using ErrorType = String;
    using UnexpectedResult = Unexpected<ErrorType>;
    using Result = Expected<std::unique_ptr<InternalFunction>, ErrorType>;
    using PartialResult = Expected<void, ErrorType>;

    static_assert(std::is_same_v<ResultList, FunctionParser<AirIRGenerator>::ResultList>);

    static ExpressionType emptyExpression() { return { }; };

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

    AirIRGenerator(const ModuleInformation&, B3::Procedure&, InternalFunction*, Vector<UnlinkedWasmToWasmCall>&, MemoryMode, unsigned functionIndex, TierUpCount*, const Signature&);

    PartialResult WARN_UNUSED_RETURN addArguments(const Signature&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);
    ExpressionType addConstant(BasicBlock*, Type, uint64_t);
    ExpressionType addBottom(BasicBlock*, Type);

    // References
    PartialResult WARN_UNUSED_RETURN addRefIsNull(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addRefFunc(uint32_t index, ExpressionType& result);

    // Tables
    PartialResult WARN_UNUSED_RETURN addTableGet(unsigned, ExpressionType index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableSet(unsigned, ExpressionType index, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addTableInit(unsigned, unsigned, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length);
    PartialResult WARN_UNUSED_RETURN addElemDrop(unsigned);
    PartialResult WARN_UNUSED_RETURN addTableSize(unsigned, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableGrow(unsigned, ExpressionType fill, ExpressionType delta, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableFill(unsigned, ExpressionType offset, ExpressionType fill, ExpressionType count);
    PartialResult WARN_UNUSED_RETURN addTableCopy(unsigned, unsigned, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length);

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
    PartialResult WARN_UNUSED_RETURN addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count);
    PartialResult WARN_UNUSED_RETURN addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count);
    PartialResult WARN_UNUSED_RETURN addMemoryInit(unsigned, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length);
    PartialResult WARN_UNUSED_RETURN addDataDrop(unsigned);

    // Atomics
    PartialResult WARN_UNUSED_RETURN atomicLoad(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicStore(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicWait(ExtAtomicOpType, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicNotify(ExtAtomicOpType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicFence(ExtAtomicOpType, uint8_t flags);

    // Saturated truncation.
    PartialResult WARN_UNUSED_RETURN truncSaturated(Ext1OpType, ExpressionType operand, ExpressionType& result, Type returnType, Type operandType);

    // Basic operators
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addTopLevel(BlockSignature);
    PartialResult WARN_UNUSED_RETURN addBlock(BlockSignature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addLoop(BlockSignature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType condition, BlockSignature, Stack& enclosingStack, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addElse(ControlData&, const Stack&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& expressionStack = { });

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&) { return { }; }

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallRef(const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN emitIndirectCall(TypedTmp calleeInstance, ExpressionType calleeCode, const Signature&, const Vector<ExpressionType>& args, ResultList&);
    B3::PatchpointValue* WARN_UNUSED_RETURN emitCallPatchpoint(BasicBlock*, const Signature&, const ResultList& results, const Vector<TypedTmp>& args, Vector<ConstrainedTmp>&& extraArgs = { });

    PartialResult addShift(Type, B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
    PartialResult addIntegerSub(B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addFloatingPointAbs(B3::Air::Opcode, ExpressionType value, ExpressionType& result);
    PartialResult addFloatingPointBinOp(Type, B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    void dump(const ControlStack&, const Stack* expressionStack);
    void setParser(FunctionParser<AirIRGenerator>* parser) { m_parser = parser; };
    void didFinishParsingLocals() { }
    void didPopValueFromStack() { }

    const Bag<B3::PatchpointValue*>& patchpoints() const
    {
        return m_patchpoints;
    }

private:
    B3::Type toB3ResultType(BlockSignature returnType);
    ALWAYS_INLINE void validateInst(Inst& inst)
    {
        if (ASSERT_ENABLED) {
            if (!inst.isValidForm()) {
                dataLogLn("Inst validation failed:");
                dataLogLn(inst, "\n");
                if (inst.origin)
                    dataLogLn(deepDump(inst.origin), "\n");
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

    template<typename... Arguments>
    void appendEffectful(BasicBlock* block, B3::Air::Opcode op, Arguments&&... arguments)
    {
        Kind kind = op;
        kind.effects = true;
        append(block, kind, std::forward<Arguments>(arguments)...);
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

    TypedTmp g32() { return { newTmp(B3::GP), Types::I32 }; }
    TypedTmp g64() { return { newTmp(B3::GP), Types::I64 }; }
    TypedTmp gExternref() { return { newTmp(B3::GP), Types::Externref }; }
    TypedTmp gFuncref() { return { newTmp(B3::GP), Types::Funcref }; }
    TypedTmp gTypeIdx(Type type) { return { newTmp(B3::GP), type }; }
    TypedTmp f32() { return { newTmp(B3::FP), Types::F32 }; }
    TypedTmp f64() { return { newTmp(B3::FP), Types::F64 }; }

    TypedTmp tmpForType(Type type)
    {
        switch (type.kind) {
        case TypeKind::I32:
            return g32();
        case TypeKind::I64:
            return g64();
        case TypeKind::Funcref:
            return gFuncref();
        case TypeKind::TypeIdx:
            return gTypeIdx(type);
        case TypeKind::Externref:
            return gExternref();
        case TypeKind::F32:
            return f32();
        case TypeKind::F64:
            return f64();
        case TypeKind::Void:
            return { };
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    ResultList tmpsForSignature(BlockSignature signature)
    {
        ResultList result(signature->returnCount());
        for (unsigned i = 0; i < signature->returnCount(); ++i)
            result[i] = tmpForType(signature->returnType(i));
        return result;
    }

    B3::PatchpointValue* addPatchpoint(B3::Type type)
    {
        auto* result = m_proc.add<B3::PatchpointValue>(type, B3::Origin());
        if (UNLIKELY(shouldDumpIRAtEachPhase(B3::AirMode)))
            m_patchpoints.add(result);
        return result;
    }

    template <typename ...Args>
    void emitPatchpoint(B3::PatchpointValue* patch, Tmp result, Args... theArgs)
    {
        emitPatchpoint(m_currentBlock, patch, result, std::forward<Args>(theArgs)...);
    }

    template <typename ...Args>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result, Args... theArgs)
    {
        emitPatchpoint(basicBlock, patch, Vector<Tmp, 8> { result }, Vector<ConstrainedTmp, sizeof...(Args)>::from(theArgs...));
    }

    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result)
    {
        emitPatchpoint(basicBlock, patch, Vector<Tmp, 8> { result }, Vector<ConstrainedTmp>());
    }

    template <typename ResultTmpType, size_t inlineSize>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, const Vector<ResultTmpType, 8>&  results, Vector<ConstrainedTmp, inlineSize>&& args)
    {
        if (!m_patchpointSpecial)
            m_patchpointSpecial = static_cast<B3::PatchpointSpecial*>(m_code.addSpecial(makeUnique<B3::PatchpointSpecial>()));

        auto toTmp = [&] (ResultTmpType tmp) {
            if constexpr (std::is_same_v<ResultTmpType, Tmp>)
                return tmp;
            else
                return tmp.tmp();
        };

        Inst inst(Patch, patch, Arg::special(m_patchpointSpecial));
        Vector<Inst, 1> resultMovs;
        switch (patch->type().kind()) {
        case B3::Void:
            break;
        default: {
            ASSERT(results.size());
            for (unsigned i = 0; i < results.size(); ++i) {
                switch (patch->resultConstraints[i].kind()) {
                case B3::ValueRep::StackArgument: {
                    Arg arg = Arg::callArg(patch->resultConstraints[i].offsetFromSP());
                    inst.args.append(arg);
                    resultMovs.append(Inst(B3::Air::moveForType(m_proc.typeAtOffset(patch->type(), i)), nullptr, arg, toTmp(results[i])));
                    break;
                }
                case B3::ValueRep::Register: {
                    inst.args.append(Tmp(patch->resultConstraints[i].reg()));
                    resultMovs.append(Inst(B3::Air::relaxedMoveForType(m_proc.typeAtOffset(patch->type(), i)), nullptr, Tmp(patch->resultConstraints[i].reg()), toTmp(results[i])));
                    break;
                }
                case B3::ValueRep::SomeRegister: {
                    inst.args.append(toTmp(results[i]));
                    break;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
        }
        }

        for (unsigned i = 0; i < args.size(); ++i) {
            ConstrainedTmp& tmp = args[i];
            // FIXME: This is less than ideal to create dummy values just to satisfy Air's
            // validation. We should abstrcat Patch enough so ValueRep's don't need to be
            // backed by Values.
            // https://bugs.webkit.org/show_bug.cgi?id=194040
            B3::Value* dummyValue = m_proc.addConstant(B3::Origin(), tmp.tmp.isGP() ? B3::Int64 : B3::Double, 0);
            patch->append(dummyValue, tmp.rep);
            switch (tmp.rep.kind()) {
            case B3::ValueRep::ColdAny: // B3::Value propagates ColdAny information and later Air will allocate appropriate stack.
            case B3::ValueRep::SomeRegister:
                inst.args.append(tmp.tmp);
                break;
            case B3::ValueRep::Register:
                patch->earlyClobbered().clear(tmp.rep.reg());
                append(basicBlock, tmp.tmp.isGP() ? Move : MoveDouble, tmp.tmp, tmp.rep.reg());
                inst.args.append(Tmp(tmp.rep.reg()));
                break;
            case B3::ValueRep::StackArgument: {
                ASSERT(!patch->effects.terminal);
                Arg arg = Arg::callArg(tmp.rep.offsetFromSP());
                append(basicBlock, tmp.tmp.isGP() ? Move : MoveDouble, tmp.tmp, arg);
                ASSERT(arg.canRepresent(patch->child(i)->type()));
                inst.args.append(arg);
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        for (auto valueRep : patch->resultConstraints) {
            if (valueRep.isReg())
                patch->lateClobbered().clear(valueRep.reg());
        }
        for (unsigned i = patch->numGPScratchRegisters; i--;)
            inst.args.append(g64().tmp());
        for (unsigned i = patch->numFPScratchRegisters; i--;)
            inst.args.append(f64().tmp());

        validateInst(inst);
        basicBlock->append(WTFMove(inst));
        for (Inst result : resultMovs) {
            validateInst(result);
            basicBlock->append(WTFMove(result));
        }
    }

    template <typename Branch, typename Generator>
    void emitCheck(const Branch& makeBranch, const Generator& generator)
    {
        // We fail along the truthy edge of 'branch'.
        Inst branch = makeBranch();

        // FIXME: Make a hashmap of these.
        B3::CheckSpecial::Key key(branch);
        B3::CheckSpecial* special = static_cast<B3::CheckSpecial*>(m_code.addSpecial(makeUnique<B3::CheckSpecial>(key)));

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
            switch (result.type().kind) {
            case TypeKind::I32:
                resultType = B3::Int32;
                break;
            case TypeKind::I64:
            case TypeKind::Externref:
            case TypeKind::Funcref:
            case TypeKind::TypeIdx:
                resultType = B3::Int64;
                break;
            case TypeKind::F32:
                resultType = B3::Float;
                break;
            case TypeKind::F64:
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
        append(block, Move, Arg::immPtr(tagCFunctionPtr<void*, OperationPtrTag>(func)), callee);
        inst.args.append(callee);

        if (result)
            inst.args.append(result.tmp());

        for (Tmp tmp : Vector<Tmp, sizeof...(Args)>::from(theArgs.tmp()...))
            inst.args.append(tmp);

        block->append(WTFMove(inst));
    }

    static B3::Air::Opcode moveOpForValueType(Type type)
    {
        switch (type.kind) {
        case TypeKind::I32:
            return Move32;
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::TypeIdx:
            return Move;
        case TypeKind::F32:
            return MoveFloat;
        case TypeKind::F64:
            return MoveDouble;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void emitThrowException(CCallHelpers&, ExceptionType);

    void emitEntryTierUpCheck();
    void emitLoopTierUpCheck(uint32_t loopIndex, const Stack& enclosingStack, const Stack& newStack);

    void emitWriteBarrierForJSWrapper();
    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicLoadOp(ExtAtomicOpType, Type, ExpressionType pointer, uint32_t offset);
    void emitAtomicStoreOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicBinaryRMWOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicCompareExchange(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t offset);

    void sanitizeAtomicResult(ExtAtomicOpType, Type, Tmp source, Tmp dest);
    void sanitizeAtomicResult(ExtAtomicOpType, Type, Tmp result);
    TypedTmp appendGeneralAtomic(ExtAtomicOpType, B3::Air::Opcode nonAtomicOpcode, B3::Commutativity, Arg input, Arg addrArg, TypedTmp result);
    TypedTmp appendStrongCAS(ExtAtomicOpType, TypedTmp expected, TypedTmp value, Arg addrArg, TypedTmp result);

    void unify(const ExpressionType dst, const ExpressionType source);
    void unifyValuesWithBlock(const Stack& resultStack, const ResultList& stack);

    template <typename IntType>
    void emitChecksForModOrDiv(bool isSignedDiv, ExpressionType left, ExpressionType right);

    template <typename IntType>
    void emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    enum class MinOrMax { Min, Max };

    PartialResult addFloatingPointMinOrMax(Type, MinOrMax, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    int32_t WARN_UNUSED_RETURN fixupPointerPlusOffset(ExpressionType&, uint32_t);
    ExpressionType WARN_UNUSED_RETURN fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType, ExpressionType, uint32_t);

    void restoreWasmContextInstance(BasicBlock*, TypedTmp);
    enum class RestoreCachedStackLimit { No, Yes };
    void restoreWebAssemblyGlobalState(RestoreCachedStackLimit, const MemoryInformation&, TypedTmp instance, BasicBlock*);

    B3::Origin origin();

    uint32_t outerLoopIndex() const
    {
        if (m_outerLoops.isEmpty())
            return UINT32_MAX;
        return m_outerLoops.last();
    }

    FunctionParser<AirIRGenerator>* m_parser { nullptr };
    const ModuleInformation& m_info;
    const MemoryMode m_mode { MemoryMode::BoundsChecking };
    const unsigned m_functionIndex { UINT_MAX };
    TierUpCount* m_tierUp { nullptr };

    B3::Procedure& m_proc;
    Code& m_code;
    Vector<uint32_t> m_outerLoops;
    BasicBlock* m_currentBlock { nullptr };
    BasicBlock* m_rootBlock { nullptr };
    Vector<TypedTmp> m_locals;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    GPRReg m_memoryBaseGPR { InvalidGPRReg };
    GPRReg m_boundsCheckingSizeGPR { InvalidGPRReg };
    GPRReg m_wasmContextInstanceGPR { InvalidGPRReg };
    bool m_makesCalls { false };

    Vector<Tmp, 8> m_freeGPs;
    Vector<Tmp, 8> m_freeFPs;

    HashMap<BlockSignature, B3::Type> m_tupleMap;
    // This is only filled if we are dumping IR.
    Bag<B3::PatchpointValue*> m_patchpoints;

    TypedTmp m_instanceValue; // Always use the accessor below to ensure the instance value is materialized when used.
    bool m_usesInstanceValue { false };
    TypedTmp instanceValue()
    {
        m_usesInstanceValue = true;
        return m_instanceValue;
    }

    uint32_t m_maxNumJSCallArguments { 0 };
    unsigned m_numImportFunctions;

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

AirIRGenerator::AirIRGenerator(const ModuleInformation& info, B3::Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, unsigned functionIndex, TierUpCount* tierUp, const Signature& signature)
    : m_info(info)
    , m_mode(mode)
    , m_functionIndex(functionIndex)
    , m_tierUp(tierUp)
    , m_proc(procedure)
    , m_code(m_proc.code())
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_numImportFunctions(info.importFunctionCount())
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
        m_boundsCheckingSizeGPR = pinnedRegs.boundsCheckingSizeRegister;
        m_code.pinRegister(m_boundsCheckingSizeGPR);
    }

    m_code.setNumEntrypoints(1);

    GPRReg contextInstance = Context::useFastTLS() ? wasmCallingConvention().prologueScratchGPRs[1] : m_wasmContextInstanceGPR;

    Ref<B3::Air::PrologueGenerator> prologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([=] (CCallHelpers& jit, B3::Air::Code& code) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        code.emitDefaultPrologue(jit);

        {
            GPRReg calleeGPR = wasmCallingConvention().prologueScratchGPRs[0];
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
                Checked<uint32_t>(m_maxNumJSCallArguments) * sizeof(Register) + jsCallingConvention().headerSizeInBytes
            ));
            const int32_t checkSize = m_makesCalls ? (wasmFrameSize + extraFrameSize).value() : wasmFrameSize.value();
            bool needUnderflowCheck = static_cast<unsigned>(checkSize) > Options::reservedZoneSize();
            bool needsOverflowCheck = m_makesCalls || wasmFrameSize >= static_cast<int32_t>(minimumParentCheckSize) || needUnderflowCheck;

            // This allows leaf functions to not do stack checks if their frame size is within
            // certain limits since their caller would have already done the check.
            if (needsOverflowCheck) {
                GPRReg scratch = wasmCallingConvention().prologueScratchGPRs[0];

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
        m_instanceValue = { Tmp(contextInstance), Types::I64 };

    ASSERT(!m_locals.size());
    m_locals.grow(signature.argumentCount());
    for (unsigned i = 0; i < signature.argumentCount(); ++i) {
        Type type = signature.argument(i);
        m_locals[i] = tmpForType(type);
    }

    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);

    for (unsigned i = 0; i < wasmCallInfo.params.size(); ++i) {
        B3::ValueRep location = wasmCallInfo.params[i];
        Arg arg = location.isReg() ? Arg(Tmp(location.reg())) : Arg::addr(Tmp(GPRInfo::callFrameRegister), location.offsetFromFP());
        switch (signature.argument(i).kind) {
        case TypeKind::I32:
            append(Move32, arg, m_locals[i]);
            break;
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::TypeIdx:
            append(Move, arg, m_locals[i]);
            break;
        case TypeKind::F32:
            append(MoveFloat, arg, m_locals[i]);
            break;
        case TypeKind::F64:
            append(MoveDouble, arg, m_locals[i]);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    emitEntryTierUpCheck();
}

B3::Type AirIRGenerator::toB3ResultType(BlockSignature returnType)
{
    if (returnType->returnsVoid())
        return B3::Void;

    if (returnType->returnCount() == 1)
        return toB3Type(returnType->returnType(0));

    auto result = m_tupleMap.ensure(returnType, [&] {
        Vector<B3::Type> result;
        for (unsigned i = 0; i < returnType->returnCount(); ++i)
            result.append(toB3Type(returnType->returnType(i)));
        return m_proc.addTuple(WTFMove(result));
    });
    return result.iterator->value;
}

void AirIRGenerator::restoreWebAssemblyGlobalState(RestoreCachedStackLimit restoreCachedStackLimit, const MemoryInformation& memory, TypedTmp instance, BasicBlock* block)
{
    restoreWasmContextInstance(block, instance);

    if (restoreCachedStackLimit == RestoreCachedStackLimit::Yes) {
        // The Instance caches the stack limit, but also knows where its canonical location is.
        static_assert(sizeof(std::declval<Instance*>()->cachedStackLimit()) == sizeof(uint64_t), "codegen relies on this size");

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
        clobbers.set(pinnedRegs->boundsCheckingSizeRegister);
        clobbers.set(RegisterSet::macroScratchRegisters());

        auto* patchpoint = addPatchpoint(B3::Void);
        B3::Effects effects = B3::Effects::none();
        effects.writesPinned = true;
        effects.reads = B3::HeapRange::top();
        patchpoint->effects = effects;
        patchpoint->clobber(clobbers);
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->setGenerator([pinnedRegs] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg baseMemory = pinnedRegs->baseMemoryPointer;
            GPRReg scratch = params.gpScratch(0);

            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), Instance::offsetOfCachedBoundsCheckingSize()), pinnedRegs->boundsCheckingSizeRegister);
            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), Instance::offsetOfCachedMemory()), baseMemory);

            jit.cageConditionallyAndUntag(Gigacage::Primitive, baseMemory, pinnedRegs->boundsCheckingSizeRegister, scratch);
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
    size_t newSize = m_locals.size() + count;
    ASSERT(!(CheckedUint32(count) + m_locals.size()).hasOverflowed());
    ASSERT(newSize <= maxFunctionLocals);
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(newSize), "can't allocate memory for ", newSize, " locals");

    for (uint32_t i = 0; i < count; ++i) {
        auto local = tmpForType(type);
        m_locals.uncheckedAppend(local);
        switch (type.kind) {
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::TypeIdx:
            append(Move, Arg::imm(JSValue::encode(jsNull())), local);
            break;
        case TypeKind::I32:
        case TypeKind::I64: {
            append(Xor64, local, local);
            break;
        }
        case TypeKind::F32:
        case TypeKind::F64: {
            auto temp = g64();
            // IEEE 754 "0" is just int32/64 zero.
            append(Xor64, temp, temp);
            append(type.isF32() ? Move32ToFloat : Move64ToDouble, temp, local);
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
    switch (type.kind) {
    case TypeKind::I32:
    case TypeKind::I64:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::TypeIdx:
        append(block, Move, Arg::bigImm(value), result);
        break;
    case TypeKind::F32:
    case TypeKind::F64: {
        auto tmp = g64();
        append(block, Move, Arg::bigImm(value), tmp);
        append(block, type.isF32() ? Move32ToFloat : Move64ToDouble, tmp, result);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return result;
}

auto AirIRGenerator::addBottom(BasicBlock* block, Type type) -> ExpressionType
{
    append(block, B3::Air::Oops);
    return addConstant(type, 0);
}

auto AirIRGenerator::addArguments(const Signature& signature) -> PartialResult
{
    RELEASE_ASSERT(m_locals.size() == signature.argumentCount()); // We handle arguments in the prologue
    return { };
}

auto AirIRGenerator::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    ASSERT(value.tmp());
    result = tmpForType(Types::I32);
    auto tmp = g64();

    append(Move, Arg::bigImm(JSValue::encode(jsNull())), tmp);
    append(Compare64, Arg::relCond(MacroAssembler::Equal), value, tmp, result);

    return { };
}

auto AirIRGenerator::addRefFunc(uint32_t index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    if (Options::useWebAssemblyTypedFunctionReferences()) {
        SignatureIndex signatureIndex = m_info.signatureIndexFromFunctionIndexSpace(index);
        result = tmpForType(Type { TypeKind::TypeIdx, Nullable::No, signatureIndex });
    } else
        result = tmpForType(Types::Funcref);
    emitCCall(&operationWasmRefFunc, result, instanceValue(), addConstant(Types::I32, index));

    return { };
}

auto AirIRGenerator::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index.tmp());

    ASSERT(index.type().isI32());
    result = tmpForType(m_info.tables[tableIndex].wasmType());

    emitCCall(&operationGetWasmTableElement, result, instanceValue(), addConstant(Types::I32, tableIndex), index);
    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addTableSet(unsigned tableIndex, ExpressionType index, ExpressionType value) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index.tmp());
    ASSERT(index.type().isI32());
    ASSERT(value.tmp());

    auto shouldThrow = g32();
    emitCCall(&operationSetWasmTableElement, shouldThrow, instanceValue(), addConstant(Types::I32, tableIndex), index, value);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), shouldThrow, shouldThrow);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    ASSERT(dstOffset.tmp());
    ASSERT(dstOffset.type().isI32());

    ASSERT(srcOffset.tmp());
    ASSERT(srcOffset.type().isI32());

    ASSERT(length.tmp());
    ASSERT(length.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmTableInit, result, instanceValue(),
        addConstant(Types::I32, elementIndex),
        addConstant(Types::I32, tableIndex),
        dstOffset, srcOffset, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addElemDrop(unsigned elementIndex) -> PartialResult
{
    emitCCall(&operationWasmElemDrop, TypedTmp(), instanceValue(), addConstant(Types::I32, elementIndex));
    return { };
}

auto AirIRGenerator::addTableSize(unsigned tableIndex, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    result = tmpForType(Types::I32);

    emitCCall(&operationGetWasmTableSize, result, instanceValue(), addConstant(Types::I32, tableIndex));

    return { };
}

auto AirIRGenerator::addTableGrow(unsigned tableIndex, ExpressionType fill, ExpressionType delta, ExpressionType& result) -> PartialResult
{
    ASSERT(fill.tmp());
    ASSERT(isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()));
    ASSERT(delta.tmp());
    ASSERT(delta.type().isI32());
    result = tmpForType(Types::I32);

    emitCCall(&operationWasmTableGrow, result, instanceValue(), addConstant(Types::I32, tableIndex), fill, delta);

    return { };
}

auto AirIRGenerator::addTableFill(unsigned tableIndex, ExpressionType offset, ExpressionType fill, ExpressionType count) -> PartialResult
{
    ASSERT(fill.tmp());
    ASSERT(isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()));
    ASSERT(offset.tmp());
    ASSERT(offset.type().isI32());
    ASSERT(count.tmp());
    ASSERT(count.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(&operationWasmTableFill, result, instanceValue(), addConstant(Types::I32, tableIndex), offset, fill, count);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    ASSERT(dstOffset.tmp());
    ASSERT(dstOffset.type().isI32());

    ASSERT(srcOffset.tmp());
    ASSERT(srcOffset.type().isI32());

    ASSERT(length.tmp());
    ASSERT(length.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmTableCopy, result, instanceValue(),
        addConstant(Types::I32, dstTableIndex),
        addConstant(Types::I32, srcTableIndex),
        dstOffset, srcOffset, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

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
    result = g32();
    emitCCall(&operationGrowMemory, result, TypedTmp { Tmp(GPRInfo::callFrameRegister), Types::I64 }, instanceValue(), delta);
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::No, m_info.memory, instanceValue(), m_currentBlock);

    return { };
}

auto AirIRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    static_assert(sizeof(std::declval<Memory*>()->size()) == sizeof(uint64_t), "codegen relies on this size");

    auto temp1 = g64();
    auto temp2 = g64();

    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfMemory(), B3::Width64));
    RELEASE_ASSERT(Arg::isValidAddrForm(Memory::offsetOfHandle(), B3::Width64));
    RELEASE_ASSERT(Arg::isValidAddrForm(MemoryHandle::offsetOfSize(), B3::Width64));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfMemory()), temp1);
    append(Move, Arg::addr(temp1, Memory::offsetOfHandle()), temp1);
    append(Move, Arg::addr(temp1, MemoryHandle::offsetOfSize()), temp1);
    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    append(Move, Arg::imm(16), temp2);
    addShift(Types::I32, Urshift64, temp1, temp2, result);
    append(Move32, result, result);

    return { };
}

auto AirIRGenerator::addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count) -> PartialResult
{
    ASSERT(dstAddress.tmp());
    ASSERT(dstAddress.type().isI32());

    ASSERT(targetValue.tmp());
    ASSERT(targetValue.type().isI32());

    ASSERT(count.tmp());
    ASSERT(count.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmMemoryFill, result, instanceValue(),
        dstAddress, targetValue, count);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count) -> PartialResult
{
    ASSERT(dstAddress.tmp());
    ASSERT(dstAddress.type().isI32());

    ASSERT(srcAddress.tmp());
    ASSERT(srcAddress.type().isI32());

    ASSERT(count.tmp());
    ASSERT(count.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmMemoryCopy, result, instanceValue(),
        dstAddress, srcAddress, count);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addMemoryInit(unsigned dataSegmentIndex, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length) -> PartialResult
{
    ASSERT(dstAddress.tmp());
    ASSERT(dstAddress.type().isI32());

    ASSERT(srcAddress.tmp());
    ASSERT(srcAddress.type().isI32());

    ASSERT(length.tmp());
    ASSERT(length.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmMemoryInit, result, instanceValue(),
        addConstant(Types::I32, dataSegmentIndex),
        dstAddress, srcAddress, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addDataDrop(unsigned dataSegmentIndex) -> PartialResult
{
    emitCCall(&operationWasmDataDrop, TypedTmp(), instanceValue(), addConstant(Types::I32, dataSegmentIndex));
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
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    result = tmpForType(type);

    auto temp = g64();

    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfGlobals(), B3::Width64));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfGlobals()), temp);

    int32_t offset = safeCast<int32_t>(index * sizeof(Register));
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        if (Arg::isValidAddrForm(offset, B3::widthForType(toB3Type(type))))
            append(moveOpForValueType(type), Arg::addr(temp, offset), result);
        else {
            auto temp2 = g64();
            append(Move, Arg::bigImm(offset), temp2);
            append(Add64, temp2, temp, temp);
            append(moveOpForValueType(type), Arg::addr(temp), result);
        }
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        ASSERT(global.mutability == Wasm::GlobalInformation::Mutability::Mutable);
        if (Arg::isValidAddrForm(offset, B3::Width64))
            append(Move, Arg::addr(temp, offset), temp);
        else {
            auto temp2 = g64();
            append(Move, Arg::bigImm(offset), temp2);
            append(Add64, temp2, temp, temp);
            append(Move, Arg::addr(temp), temp);
        }
        append(moveOpForValueType(type), Arg::addr(temp), result);
        break;
    }
    return { };
}

auto AirIRGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    auto temp = g64();

    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfGlobals(), B3::Width64));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfGlobals()), temp);

    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    int32_t offset = safeCast<int32_t>(index * sizeof(Register));
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        if (Arg::isValidAddrForm(offset, B3::widthForType(toB3Type(type))))
            append(moveOpForValueType(type), value, Arg::addr(temp, offset));
        else {
            auto temp2 = g64();
            append(Move, Arg::bigImm(offset), temp2);
            append(Add64, temp2, temp, temp);
            append(moveOpForValueType(type), value, Arg::addr(temp));
        }
        if (isRefType(type))
            emitWriteBarrierForJSWrapper();
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        ASSERT(global.mutability == Wasm::GlobalInformation::Mutability::Mutable);
        if (Arg::isValidAddrForm(offset, B3::Width64))
            append(Move, Arg::addr(temp, offset), temp);
        else {
            auto temp2 = g64();
            append(Move, Arg::bigImm(offset), temp2);
            append(Add64, temp2, temp, temp);
            append(Move, Arg::addr(temp), temp);
        }
        append(moveOpForValueType(type), value, Arg::addr(temp));
        // We emit a write-barrier onto JSWebAssemblyGlobal, not JSWebAssemblyInstance.
        if (isRefType(type)) {
            auto cell = g64();
            auto vm = g64();
            auto cellState = g32();
            auto threshold = g32();

            BasicBlock* fenceCheckPath = m_code.addBlock();
            BasicBlock* fencePath = m_code.addBlock();
            BasicBlock* doSlowPath = m_code.addBlock();
            BasicBlock* continuation = m_code.addBlock();

            append(Move, Arg::addr(instanceValue(), Instance::offsetOfOwner()), cell);
            append(Move, Arg::addr(cell, JSWebAssemblyInstance::offsetOfVM()), vm);

            append(Move, Arg::addr(temp, Wasm::Global::offsetOfOwner() - Wasm::Global::offsetOfValue()), cell);
            append(Load8, Arg::addr(cell, JSCell::cellStateOffset()), cellState);
            append(Move32, Arg::addr(vm, VM::offsetOfHeapBarrierThreshold()), threshold);

            append(Branch32, Arg::relCond(MacroAssembler::Above), cellState, threshold);
            m_currentBlock->setSuccessors(continuation, fenceCheckPath);
            m_currentBlock = fenceCheckPath;

            append(Load8, Arg::addr(vm, VM::offsetOfHeapMutatorShouldBeFenced()), threshold);
            append(BranchTest32, Arg::resCond(MacroAssembler::Zero), threshold, threshold);
            m_currentBlock->setSuccessors(doSlowPath, fencePath);
            m_currentBlock = fencePath;

            auto* doFence = addPatchpoint(B3::Void);
            doFence->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                jit.memoryFence();
            });
            emitPatchpoint(doFence, Tmp());

            append(Load8, Arg::addr(cell, JSCell::cellStateOffset()), cellState);
            append(Branch32, Arg::relCond(MacroAssembler::Above), cellState, Arg::imm(blackThreshold));
            m_currentBlock->setSuccessors(continuation, doSlowPath);
            m_currentBlock = doSlowPath;

            emitCCall(&operationWasmWriteBarrierSlowPath, TypedTmp(), cell, vm);
            append(Jump);
            m_currentBlock->setSuccessors(continuation);
            m_currentBlock = continuation;
        }
        break;
    }

    return { };
}

inline void AirIRGenerator::emitWriteBarrierForJSWrapper()
{
    auto cell = g64();
    auto vm = g64();
    auto cellState = g32();
    auto threshold = g32();

    BasicBlock* fenceCheckPath = m_code.addBlock();
    BasicBlock* fencePath = m_code.addBlock();
    BasicBlock* doSlowPath = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(Move, Arg::addr(instanceValue(), Instance::offsetOfOwner()), cell);
    append(Move, Arg::addr(cell, JSWebAssemblyInstance::offsetOfVM()), vm);
    append(Load8, Arg::addr(cell, JSCell::cellStateOffset()), cellState);
    append(Move32, Arg::addr(vm, VM::offsetOfHeapBarrierThreshold()), threshold);

    append(Branch32, Arg::relCond(MacroAssembler::Above), cellState, threshold);
    m_currentBlock->setSuccessors(continuation, fenceCheckPath);
    m_currentBlock = fenceCheckPath;

    append(Load8, Arg::addr(vm, VM::offsetOfHeapMutatorShouldBeFenced()), threshold);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), threshold, threshold);
    m_currentBlock->setSuccessors(doSlowPath, fencePath);
    m_currentBlock = fencePath;

    auto* doFence = addPatchpoint(B3::Void);
    doFence->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        jit.memoryFence();
    });
    emitPatchpoint(doFence, Tmp());

    append(Load8, Arg::addr(cell, JSCell::cellStateOffset()), cellState);
    append(Branch32, Arg::relCond(MacroAssembler::Above), cellState, Arg::imm(blackThreshold));
    m_currentBlock->setSuccessors(continuation, doSlowPath);
    m_currentBlock = doSlowPath;

    emitCCall(&operationWasmWriteBarrierSlowPath, TypedTmp(), cell, vm);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = continuation;
}

inline AirIRGenerator::ExpressionType AirIRGenerator::emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOperation)
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
            auto sizeMax = addConstant(Types::I64, maximum);

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

#define OPCODE_FOR_WIDTH(opcode, width) ( \
    (width) == B3::Width8 ? B3::Air::opcode ## 8 : \
    (width) == B3::Width16 ? B3::Air::opcode ## 16 :    \
    (width) == B3::Width32 ? B3::Air::opcode ## 32 :    \
    B3::Air::opcode ## 64)
#define OPCODE_FOR_CANONICAL_WIDTH(opcode, width) ( \
    (width) == B3::Width64 ? B3::Air::opcode ## 64 : B3::Air::opcode ## 32)

inline B3::Width accessWidth(ExtAtomicOpType op)
{
    return static_cast<B3::Width>(memoryLog2Alignment(op));
}

inline uint32_t sizeOfAtomicOpMemoryAccess(ExtAtomicOpType op)
{
    return bytesForWidth(accessWidth(op));
}

auto AirIRGenerator::fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType op, ExpressionType pointer, uint32_t uoffset) -> ExpressionType
{
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);
    if (Arg::isValidAddrForm(offset, B3::widthForBytes(sizeOfAtomicOpMemoryAccess(op)))) {
        if (offset == 0)
            return pointer;
        TypedTmp newPtr = g64();
        append(Lea64, Arg::addr(pointer, offset), newPtr);
        return newPtr;
    }
    TypedTmp newPtr = g64();
    append(Move, Arg::bigImm(offset), newPtr);
    append(Add64, pointer, newPtr);
    return newPtr;
}

void AirIRGenerator::sanitizeAtomicResult(ExtAtomicOpType op, Type valueType, Tmp source, Tmp dest)
{
    switch (valueType.kind) {
    case TypeKind::I64: {
        switch (accessWidth(op)) {
        case B3::Width8:
            append(ZeroExtend8To32, source, dest);
            return;
        case B3::Width16:
            append(ZeroExtend16To32, source, dest);
            return;
        case B3::Width32:
            append(Move32, source, dest);
            return;
        case B3::Width64:
            if (source == dest)
                return;
            append(Move, source, dest);
            return;
        }
        return;
    }
    case TypeKind::I32:
        switch (accessWidth(op)) {
        case B3::Width8:
            append(ZeroExtend8To32, source, dest);
            return;
        case B3::Width16:
            append(ZeroExtend16To32, source, dest);
            return;
        case B3::Width32:
        case B3::Width64:
            if (source == dest)
                return;
            append(Move, source, dest);
            return;
        }
        return;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void AirIRGenerator::sanitizeAtomicResult(ExtAtomicOpType op, Type valueType, Tmp result)
{
    sanitizeAtomicResult(op, valueType, result, result);
}

TypedTmp AirIRGenerator::appendGeneralAtomic(ExtAtomicOpType op, B3::Air::Opcode opcode, B3::Commutativity commutativity, Arg input, Arg address, TypedTmp oldValue)
{
    B3::Width accessWidth = Wasm::accessWidth(op);

    auto newTmp = [&]() {
        if (accessWidth == B3::Width64)
            return g64();
        return g32();
    };

    auto tmp = [&](Arg arg) -> TypedTmp {
        if (arg.isTmp())
            return TypedTmp(arg.tmp(), accessWidth == B3::Width64 ? Types::I64 : Types::I32);
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
        case B3::Width8:
            prepareOpcode = Load8SignedExtendTo32;
            break;
        case B3::Width16:
            prepareOpcode = Load16SignedExtendTo32;
            break;
        case B3::Width32:
            prepareOpcode = Move32;
            break;
        case B3::Width64:
            prepareOpcode = Move;
            break;
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

TypedTmp AirIRGenerator::appendStrongCAS(ExtAtomicOpType op, TypedTmp expected, TypedTmp value, Arg address, TypedTmp valueResultTmp)
{
    B3::Width accessWidth = Wasm::accessWidth(op);

    auto newTmp = [&]() {
        if (accessWidth == B3::Width64)
            return g64();
        return g32();
    };

    auto tmp = [&](Arg arg) -> TypedTmp {
        if (arg.isTmp())
            return TypedTmp(arg.tmp(), accessWidth == B3::Width64 ? Types::I64 : Types::I32);
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

inline TypedTmp AirIRGenerator::emitAtomicLoadOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, uint32_t uoffset)
{
    TypedTmp newPtr = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);

    if (accessWidth(op) != B3::Width8) {
        emitCheck([&] {
            return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    std::optional<B3::Air::Opcode> opcode;
    if (isX86() || isARM64E())
        opcode = OPCODE_FOR_WIDTH(AtomicXchgAdd, accessWidth(op));
    B3::Air::Opcode nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Add, accessWidth(op));

    TypedTmp result = valueType.isI64() ? g64() : g32();

    if (opcode) {
        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind(), Arg::Tmp)) {
            append(Move, Arg::imm(0), result);
            appendEffectful(opcode.value(), result, addrArg, result);
            sanitizeAtomicResult(op, valueType, result);
            return result;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            append(Move, Arg::imm(0), result);
            appendEffectful(opcode.value(), result, addrArg);
            sanitizeAtomicResult(op, valueType, result);
            return result;
        }
    }

    appendGeneralAtomic(op, nonAtomicOpcode, B3::Commutative, Arg::imm(0), addrArg, result);
    sanitizeAtomicResult(op, valueType, result);
    return result;
}

auto AirIRGenerator::atomicLoad(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* patch = addPatchpoint(B3::Void);
        patch->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(patch, Tmp());

        // We won't reach here, so we just pick a random reg.
        switch (valueType.kind) {
        case TypeKind::I32:
            result = g32();
            break;
        case TypeKind::I64:
            result = g64();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = emitAtomicLoadOp(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), offset);

    return { };
}

inline void AirIRGenerator::emitAtomicStoreOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    TypedTmp newPtr = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);

    if (accessWidth(op) != B3::Width8) {
        emitCheck([&] {
            return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    std::optional<B3::Air::Opcode> opcode;
    if (isX86() || isARM64E())
        opcode = OPCODE_FOR_WIDTH(AtomicXchg, accessWidth(op));
    B3::Air::Opcode nonAtomicOpcode = B3::Air::Nop;

    if (opcode) {
        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind(), Arg::Tmp)) {
            TypedTmp result = valueType.isI64() ? g64() : g32();
            appendEffectful(opcode.value(), value, addrArg, result);
            return;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            TypedTmp result = valueType.isI64() ? g64() : g32();
            append(Move, value, result);
            appendEffectful(opcode.value(), result, addrArg);
            return;
        }
    }

    appendGeneralAtomic(op, nonAtomicOpcode, B3::Commutative, value, addrArg, valueType.isI64() ? g64() : g32());
}

auto AirIRGenerator::atomicStore(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* throwException = addPatchpoint(B3::Void);
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(throwException, Tmp());
    } else
        emitAtomicStoreOp(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), value, offset);

    return { };
}

TypedTmp AirIRGenerator::emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    TypedTmp newPtr = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);

    if (accessWidth(op) != B3::Width8) {
        emitCheck([&] {
            return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    std::optional<B3::Air::Opcode> opcode;
    B3::Air::Opcode nonAtomicOpcode = B3::Air::Nop;
    B3::Commutativity commutativity = B3::NotCommutative;
    switch (op) {
    case ExtAtomicOpType::I32AtomicRmw8AddU:
    case ExtAtomicOpType::I32AtomicRmw16AddU:
    case ExtAtomicOpType::I32AtomicRmwAdd:
    case ExtAtomicOpType::I64AtomicRmw8AddU:
    case ExtAtomicOpType::I64AtomicRmw16AddU:
    case ExtAtomicOpType::I64AtomicRmw32AddU:
    case ExtAtomicOpType::I64AtomicRmwAdd:
        if (isX86() || isARM64E())
            opcode = OPCODE_FOR_WIDTH(AtomicXchgAdd, accessWidth(op));
        nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Add, accessWidth(op));
        commutativity = B3::Commutative;
        break;
    case ExtAtomicOpType::I32AtomicRmw8SubU:
    case ExtAtomicOpType::I32AtomicRmw16SubU:
    case ExtAtomicOpType::I32AtomicRmwSub:
    case ExtAtomicOpType::I64AtomicRmw8SubU:
    case ExtAtomicOpType::I64AtomicRmw16SubU:
    case ExtAtomicOpType::I64AtomicRmw32SubU:
    case ExtAtomicOpType::I64AtomicRmwSub:
        if (isX86() || isARM64E()) {
            TypedTmp newValue;
            if (valueType.isI64()) {
                newValue = g64();
                append(Move, value, newValue);
                append(Neg64, newValue);
            } else {
                newValue = g32();
                append(Move, value, newValue);
                append(Neg32, newValue);
            }
            value = newValue;
            opcode = OPCODE_FOR_WIDTH(AtomicXchgAdd, accessWidth(op));
            nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Add, accessWidth(op));
            commutativity = B3::Commutative;
        } else {
            nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Sub, accessWidth(op));
            commutativity = B3::NotCommutative;
        }
        break;
    case ExtAtomicOpType::I32AtomicRmw8AndU:
    case ExtAtomicOpType::I32AtomicRmw16AndU:
    case ExtAtomicOpType::I32AtomicRmwAnd:
    case ExtAtomicOpType::I64AtomicRmw8AndU:
    case ExtAtomicOpType::I64AtomicRmw16AndU:
    case ExtAtomicOpType::I64AtomicRmw32AndU:
    case ExtAtomicOpType::I64AtomicRmwAnd:
        if (isARM64E()) {
            TypedTmp newValue;
            if (valueType.isI64()) {
                newValue = g64();
                append(Not64, value, newValue);
            } else {
                newValue = g32();
                append(Not32, value, newValue);
            }
            value = newValue;
            opcode = OPCODE_FOR_WIDTH(AtomicXchgClear, accessWidth(op));
        }
        nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(And, accessWidth(op));
        commutativity = B3::Commutative;
        break;
    case ExtAtomicOpType::I32AtomicRmw8OrU:
    case ExtAtomicOpType::I32AtomicRmw16OrU:
    case ExtAtomicOpType::I32AtomicRmwOr:
    case ExtAtomicOpType::I64AtomicRmw8OrU:
    case ExtAtomicOpType::I64AtomicRmw16OrU:
    case ExtAtomicOpType::I64AtomicRmw32OrU:
    case ExtAtomicOpType::I64AtomicRmwOr:
        if (isARM64E())
            opcode = OPCODE_FOR_WIDTH(AtomicXchgOr, accessWidth(op));
        nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Or, accessWidth(op));
        commutativity = B3::Commutative;
        break;
    case ExtAtomicOpType::I32AtomicRmw8XorU:
    case ExtAtomicOpType::I32AtomicRmw16XorU:
    case ExtAtomicOpType::I32AtomicRmwXor:
    case ExtAtomicOpType::I64AtomicRmw8XorU:
    case ExtAtomicOpType::I64AtomicRmw16XorU:
    case ExtAtomicOpType::I64AtomicRmw32XorU:
    case ExtAtomicOpType::I64AtomicRmwXor:
        if (isARM64E())
            opcode = OPCODE_FOR_WIDTH(AtomicXchgXor, accessWidth(op));
        nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Xor, accessWidth(op));
        commutativity = B3::Commutative;
        break;
    case ExtAtomicOpType::I32AtomicRmw8XchgU:
    case ExtAtomicOpType::I32AtomicRmw16XchgU:
    case ExtAtomicOpType::I32AtomicRmwXchg:
    case ExtAtomicOpType::I64AtomicRmw8XchgU:
    case ExtAtomicOpType::I64AtomicRmw16XchgU:
    case ExtAtomicOpType::I64AtomicRmw32XchgU:
    case ExtAtomicOpType::I64AtomicRmwXchg:
        if (isX86() || isARM64E())
            opcode = OPCODE_FOR_WIDTH(AtomicXchg, accessWidth(op));
        nonAtomicOpcode = B3::Air::Nop;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    TypedTmp result = valueType.isI64() ? g64() : g32();

    if (opcode) {
        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind(), Arg::Tmp)) {
            appendEffectful(opcode.value(), value, addrArg, result);
            sanitizeAtomicResult(op, valueType, result);
            return result;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            append(Move, value, result);
            appendEffectful(opcode.value(), result, addrArg);
            sanitizeAtomicResult(op, valueType, result);
            return result;
        }
    }

    appendGeneralAtomic(op, nonAtomicOpcode, commutativity, value, addrArg, result);
    sanitizeAtomicResult(op, valueType, result);
    return result;
}

auto AirIRGenerator::atomicBinaryRMW(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* patch = addPatchpoint(B3::Void);
        patch->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(patch, Tmp());

        switch (valueType.kind) {
        case TypeKind::I32:
            result = g32();
            break;
        case TypeKind::I64:
            result = g64();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = emitAtomicBinaryRMWOp(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), value, offset);

    return { };
}

TypedTmp AirIRGenerator::emitAtomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t uoffset)
{
    TypedTmp newPtr = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);
    B3::Width valueWidth = widthForType(toB3Type(valueType));
    B3::Width accessWidth = Wasm::accessWidth(op);

    if (accessWidth != B3::Width8) {
        emitCheck([&] {
            return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    TypedTmp result = valueType.isI64() ? g64() : g32();

    if (valueWidth == accessWidth) {
        appendStrongCAS(op, expected, value, addrArg, result);
        sanitizeAtomicResult(op, valueType, result);
        return result;
    }

    BasicBlock* failureCase = m_code.addBlock();
    BasicBlock* successCase = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    TypedTmp truncatedExpected = valueType.isI64() ? g64() : g32();
    sanitizeAtomicResult(op, valueType, expected, truncatedExpected);

    append(OPCODE_FOR_CANONICAL_WIDTH(Branch, valueWidth), Arg::relCond(MacroAssembler::NotEqual), expected, truncatedExpected);
    m_currentBlock->setSuccessors(B3::Air::FrequentedBlock(failureCase, B3::FrequencyClass::Rare), successCase);

    m_currentBlock = successCase;
    appendStrongCAS(op, expected, value, addrArg, result);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);

    m_currentBlock = failureCase;
    ([&] {
        std::optional<B3::Air::Opcode> opcode;
        if (isX86() || isARM64E())
            opcode = OPCODE_FOR_WIDTH(AtomicXchgAdd, accessWidth);
        B3::Air::Opcode nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Add, accessWidth);

        if (opcode) {
            if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind(), Arg::Tmp)) {
                append(Move, Arg::imm(0), result);
                appendEffectful(opcode.value(), result, addrArg, result);
                return;
            }

            if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
                append(Move, Arg::imm(0), result);
                appendEffectful(opcode.value(), result, addrArg);
                return;
            }
        }
        appendGeneralAtomic(op, nonAtomicOpcode, B3::Commutative, Arg::imm(0), addrArg, result);
    })();
    append(Jump);
    m_currentBlock->setSuccessors(continuation);

    m_currentBlock = continuation;
    sanitizeAtomicResult(op, valueType, result);
    return result;
}

auto AirIRGenerator::atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* patch = addPatchpoint(B3::Void);
        patch->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(patch, Tmp());

        // We won't reach here, so we just pick a random reg.
        switch (valueType.kind) {
        case TypeKind::I32:
            result = g32();
            break;
        case TypeKind::I64:
            result = g64();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    } else
        result = emitAtomicCompareExchange(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), expected, value, offset);

    return { };
}

auto AirIRGenerator::atomicWait(ExtAtomicOpType op, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = g32();

    if (op == ExtAtomicOpType::MemoryAtomicWait32)
        emitCCall(&operationMemoryAtomicWait32, result, instanceValue(), pointer, addConstant(Types::I32, offset), value, timeout);
    else
        emitCCall(&operationMemoryAtomicWait64, result, instanceValue(), pointer, addConstant(Types::I32, offset), value, timeout);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::LessThan), result, Arg::imm(0));
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
    });

    return { };
}

auto AirIRGenerator::atomicNotify(ExtAtomicOpType, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = g32();

    emitCCall(&operationMemoryAtomicNotify, result, instanceValue(), pointer, addConstant(Types::I32, offset), count);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::LessThan), result, Arg::imm(0));
    }, [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
    });

    return { };
}

auto AirIRGenerator::atomicFence(ExtAtomicOpType, uint8_t) -> PartialResult
{
    append(MemoryFence);
    return { };
}

auto AirIRGenerator::truncSaturated(Ext1OpType op, ExpressionType arg, ExpressionType& result, Type returnType, Type operandType) -> PartialResult
{
    TypedTmp maxFloat;
    TypedTmp minFloat;
    TypedTmp signBitConstant;
    bool requiresMacroScratchRegisters = false;
    switch (op) {
    case Ext1OpType::I32TruncSatF32S:
        maxFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
        minFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));
        break;
    case Ext1OpType::I32TruncSatF32U:
        maxFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
        minFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
        break;
    case Ext1OpType::I32TruncSatF64S:
        maxFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
        minFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));
        break;
    case Ext1OpType::I32TruncSatF64U:
        maxFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
        minFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(-1.0));
        break;
    case Ext1OpType::I64TruncSatF32S:
        maxFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
        minFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));
        break;
    case Ext1OpType::I64TruncSatF32U:
        maxFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
        minFloat = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
        if (isX86())
            signBitConstant = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
        requiresMacroScratchRegisters = true;
        break;
    case Ext1OpType::I64TruncSatF64S:
        maxFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
        minFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));
        break;
    case Ext1OpType::I64TruncSatF64U:
        maxFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
        minFloat = addConstant(Types::F64, bitwise_cast<uint64_t>(-1.0));
        if (isX86())
            signBitConstant = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
        requiresMacroScratchRegisters = true;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    uint64_t minResult = 0;
    uint64_t maxResult = 0;
    switch (op) {
    case Ext1OpType::I32TruncSatF32S:
    case Ext1OpType::I32TruncSatF64S:
        maxResult = bitwise_cast<uint32_t>(INT32_MAX);
        minResult = bitwise_cast<uint32_t>(INT32_MIN);
        break;
    case Ext1OpType::I32TruncSatF32U:
    case Ext1OpType::I32TruncSatF64U:
        maxResult = bitwise_cast<uint32_t>(UINT32_MAX);
        minResult = bitwise_cast<uint32_t>(0U);
        break;
    case Ext1OpType::I64TruncSatF32S:
    case Ext1OpType::I64TruncSatF64S:
        maxResult = bitwise_cast<uint64_t>(INT64_MAX);
        minResult = bitwise_cast<uint64_t>(INT64_MIN);
        break;
    case Ext1OpType::I64TruncSatF32U:
    case Ext1OpType::I64TruncSatF64U:
        maxResult = bitwise_cast<uint64_t>(UINT64_MAX);
        minResult = bitwise_cast<uint64_t>(0ULL);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    result = tmpForType(returnType);

    BasicBlock* minCase = m_code.addBlock();
    BasicBlock* maxCheckCase = m_code.addBlock();
    BasicBlock* maxCase = m_code.addBlock();
    BasicBlock* inBoundsCase = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    auto branchOp = operandType == Types::F32 ? BranchFloat : BranchDouble;
    append(m_currentBlock, branchOp, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualOrUnordered), arg, minFloat);
    m_currentBlock->setSuccessors(minCase, maxCheckCase);

    append(maxCheckCase, branchOp, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered), arg, maxFloat);
    maxCheckCase->setSuccessors(maxCase, inBoundsCase);

    if (!minResult) {
        append(minCase, Move, Arg::bigImm(minResult), result);
        append(minCase, Jump);
        minCase->setSuccessors(continuation);
    } else {
        BasicBlock* minMaterializeCase = m_code.addBlock();
        BasicBlock* nanCase = m_code.addBlock();
        append(minCase, branchOp, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg, arg);
        minCase->setSuccessors(minMaterializeCase, nanCase);

        append(minMaterializeCase, Move, Arg::bigImm(minResult), result);
        append(minMaterializeCase, Jump);
        minMaterializeCase->setSuccessors(continuation);

        append(nanCase, Move, Arg::bigImm(0), result);
        append(nanCase, Jump);
        nanCase->setSuccessors(continuation);
    }

    append(maxCase, Move, Arg::bigImm(maxResult), result);
    append(maxCase, Jump);
    maxCase->setSuccessors(continuation);

    Vector<ConstrainedTmp, 2> args;
    auto* patchpoint = addPatchpoint(toB3Type(returnType));
    patchpoint->effects = B3::Effects::none();
    args.append(arg);
    if (requiresMacroScratchRegisters) {
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        if (isX86()) {
            args.append(signBitConstant);
            patchpoint->numFPScratchRegisters = 1;
        }
    }

    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        switch (op) {
        case Ext1OpType::I32TruncSatF32S:
            jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I32TruncSatF32U:
            jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I32TruncSatF64S:
            jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I32TruncSatF64U:
            jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I64TruncSatF32S:
            jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I64TruncSatF32U: {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            ASSERT(requiresMacroScratchRegisters);
            FPRReg scratch = InvalidFPRReg;
            FPRReg constant = InvalidFPRReg;
            if (isX86()) {
                scratch = params.fpScratch(0);
                constant = params[2].fpr();
            }
            jit.truncateFloatToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
            break;
        }
        case Ext1OpType::I64TruncSatF64S:
            jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I64TruncSatF64U: {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            ASSERT(requiresMacroScratchRegisters);
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

    emitPatchpoint(inBoundsCase, patchpoint, Vector<TypedTmp, 8> { result }, WTFMove(args));
    append(inBoundsCase, Jump);
    inBoundsCase->setSuccessors(continuation);

    m_currentBlock = continuation;

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

void AirIRGenerator::emitEntryTierUpCheck()
{
    if (!m_tierUp)
        return;

    auto countdownPtr = g64();
    append(Move, Arg::bigImm(bitwise_cast<uintptr_t>(&m_tierUp->m_counter)), countdownPtr);

    auto* patch = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    patch->effects = effects;
    patch->clobber(RegisterSet::macroScratchRegisters());

    patch->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);

        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::functionEntryIncrement()), CCallHelpers::Address(params[0].gpr()));
        CCallHelpers::Label tierUpResume = jit.label();

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
                MacroAssembler::repatchNearCall(linkBuffer.locationOfNearCall<NoPtrTag>(call), CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(triggerOMGEntryTierUpThunkGenerator).code()));
            });
        });
    });

    emitPatchpoint(patch, Tmp(), countdownPtr);
}

void AirIRGenerator::emitLoopTierUpCheck(uint32_t loopIndex, const Stack& enclosingStack, const Stack& newStack)
{
    uint32_t outerLoopIndex = this->outerLoopIndex();
    m_outerLoops.append(loopIndex);

    if (!m_tierUp)
        return;

    ASSERT(m_tierUp->osrEntryTriggers().size() == loopIndex);
    m_tierUp->osrEntryTriggers().append(TierUpCount::TriggerReason::DontTrigger);
    m_tierUp->outerLoops().append(outerLoopIndex);

    auto countdownPtr = g64();
    append(Move, Arg::bigImm(bitwise_cast<uintptr_t>(&m_tierUp->m_counter)), countdownPtr);

    auto* patch = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    effects.exitsSideways = true;
    patch->effects = effects;

    patch->clobber(RegisterSet::macroScratchRegisters());
    RegisterSet clobberLate;
    clobberLate.add(GPRInfo::argumentGPR0);
    patch->clobberLate(clobberLate);

    Vector<ConstrainedTmp> patchArgs;
    patchArgs.append(countdownPtr);

    for (auto& local : m_locals)
        patchArgs.append(ConstrainedTmp(local, B3::ValueRep::ColdAny));
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        Stack& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (TypedExpression value : expressionStack)
            patchArgs.append(ConstrainedTmp(value.value(), B3::ValueRep::ColdAny));
    }
    for (TypedExpression value : enclosingStack)
        patchArgs.append(ConstrainedTmp(value.value(), B3::ValueRep::ColdAny));
    for (TypedExpression value : newStack)
        patchArgs.append(ConstrainedTmp(value.value(), B3::ValueRep::ColdAny));

    TierUpCount::TriggerReason* forceEntryTrigger = &(m_tierUp->osrEntryTriggers().last());
    static_assert(!static_cast<uint8_t>(TierUpCount::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
    static_assert(sizeof(TierUpCount::TriggerReason) == 1, "branchTest8 assumes this size");
    patch->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        CCallHelpers::Jump forceOSREntry = jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::AbsoluteAddress(forceEntryTrigger));
        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::loopIncrement()), CCallHelpers::Address(params[0].gpr()));
        MacroAssembler::Label tierUpResume = jit.label();

        OSREntryData& osrEntryData = m_tierUp->addOSREntryData(m_functionIndex, loopIndex);
        // First argument is the countdown location.
        for (unsigned index = 1; index < params.value()->numChildren(); ++index)
            osrEntryData.values().constructAndAppend(params[index], params.value()->child(index)->type());
        OSREntryData* osrEntryDataPtr = &osrEntryData;

        params.addLatePath([=] (CCallHelpers& jit) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            forceOSREntry.link(&jit);
            tierUp.link(&jit);

            jit.probe(tagCFunction<JITProbePtrTag>(operationWasmTriggerOSREntryNow), osrEntryDataPtr);
            jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::argumentGPR0).linkTo(tierUpResume, &jit);
            jit.farJump(GPRInfo::argumentGPR1, WasmEntryPtrTag);
        });
    });

    emitPatchpoint(m_currentBlock, patch, ResultList { }, WTFMove(patchArgs));
}

AirIRGenerator::ControlData AirIRGenerator::addTopLevel(BlockSignature signature)
{
    return ControlData(B3::Origin(), signature, tmpsForSignature(signature), BlockType::TopLevel, m_code.addBlock());
}

auto AirIRGenerator::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex) -> PartialResult
{
    BasicBlock* body = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    splitStack(signature, enclosingStack, newStack);
    ResultList results;
    results.reserveInitialCapacity(newStack.size());
    for (auto item : newStack)
        results.uncheckedAppend(item);
    block = ControlData(origin(), signature, WTFMove(results), BlockType::Loop, continuation, body);

    append(Jump);
    m_currentBlock->setSuccessors(body);

    m_currentBlock = body;
    emitLoopTierUpCheck(loopIndex, enclosingStack, newStack);

    return { };
}

auto AirIRGenerator::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> PartialResult
{
    splitStack(signature, enclosingStack, newStack);
    newBlock = ControlData(origin(), signature, tmpsForSignature(signature), BlockType::Block, m_code.addBlock());
    return { };
}

auto AirIRGenerator::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    BasicBlock* taken = m_code.addBlock();
    BasicBlock* notTaken = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();
    
    // Wasm bools are i32.
    append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), condition, condition);
    m_currentBlock->setSuccessors(taken, notTaken);

    m_currentBlock = taken;
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(origin(), signature, tmpsForSignature(signature), BlockType::If, continuation, notTaken);
    return { };
}

auto AirIRGenerator::addElse(ControlData& data, const Stack& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);
    return addElseToUnreachable(data);
}

auto AirIRGenerator::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.blockType() == BlockType::If);
    m_currentBlock = data.special;
    data.convertIfToBlock();
    return { };
}

auto AirIRGenerator::addReturn(const ControlData& data, const Stack& returnValues) -> PartialResult
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
        B3::ValueRep rep = wasmCallInfo.results[i];
        TypedTmp tmp = returnValues[offset + i];

        if (rep.isStack()) {
            append(moveForType(toB3Type(tmp.type())), tmp, Arg::addr(Tmp(GPRInfo::callFrameRegister), rep.offsetFromFP()));
            continue;
        }

        ASSERT(rep.isReg());
        if (data.signature()->returnType(i).isI32())
            append(Move32, tmp, tmp);
        returnConstraints.append(ConstrainedTmp(tmp, wasmCallInfo.results[i]));
    }

    emitPatchpoint(m_currentBlock, patch, ResultList { }, WTFMove(returnConstraints));
    return { };
}

// NOTE: All branches in Wasm are on 32-bit ints

auto AirIRGenerator::addBranch(ControlData& data, ExpressionType condition, const Stack& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data.results);

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

auto AirIRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const Stack& expressionStack) -> PartialResult
{
    auto& successors = m_currentBlock->successors();
    ASSERT(successors.isEmpty());
    for (const auto& target : targets) {
        unifyValuesWithBlock(expressionStack, target->results);
        successors.append(target->targetBlockForBranch());
    }
    unifyValuesWithBlock(expressionStack, defaultTarget.results);
    successors.append(defaultTarget.targetBlockForBranch());

    ASSERT(condition.type().isI32());

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

auto AirIRGenerator::endBlock(ControlEntry& entry, Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    if (data.blockType() != BlockType::Loop)
        unifyValuesWithBlock(expressionStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);

    return addEndToUnreachable(entry, expressionStack);
}


auto AirIRGenerator::addEndToUnreachable(ControlEntry& entry, const Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;

    if (data.blockType() == BlockType::If) {
        append(data.special, Jump);
        data.special->setSuccessors(m_currentBlock);
    }

    if (data.blockType() == BlockType::Loop) {
        m_outerLoops.removeLast();
        for (unsigned i = 0; i < data.signature()->returnCount(); ++i) {
            if (i < expressionStack.size())
                entry.enclosedExpressionStack.append(expressionStack[i]);
            else {
                Type type = data.signature()->returnType(i);
                entry.enclosedExpressionStack.constructAndAppend(type, addBottom(m_currentBlock, type));
            }
        }
    } else {
        for (unsigned i = 0; i < data.signature()->returnCount(); ++i)
            entry.enclosedExpressionStack.constructAndAppend(data.signature()->returnType(i), data.results[i]);
    }

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.blockType() == BlockType::TopLevel)
        return addReturn(data, entry.enclosedExpressionStack);

    return { };
}

B3::PatchpointValue* AirIRGenerator::emitCallPatchpoint(BasicBlock* block, const Signature& signature, const ResultList& results, const Vector<TypedTmp>& args, Vector<ConstrainedTmp>&& patchArgs)
{
    auto* patchpoint = addPatchpoint(toB3ResultType(&signature));
    patchpoint->effects.writesPinned = true;
    patchpoint->effects.readsPinned = true;
    patchpoint->clobberEarly(RegisterSet::macroScratchRegisters());
    patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());

    CallInformation locations = wasmCallingConvention().callInformationFor(signature);
    m_code.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), locations.headerAndArgumentStackSizeInBytes));

    size_t offset = patchArgs.size();
    Checked<size_t> newSize = checkedSum<size_t>(patchArgs.size(), args.size());
    RELEASE_ASSERT(!newSize.hasOverflowed());

    patchArgs.grow(newSize);
    for (unsigned i = 0; i < args.size(); ++i)
        patchArgs[i + offset] = ConstrainedTmp(args[i], locations.params[i]);

    if (patchpoint->type() != B3::Void) {
        Vector<B3::ValueRep, 1> resultConstraints;
        for (auto valueLocation : locations.results)
            resultConstraints.append(B3::ValueRep(valueLocation));
        patchpoint->resultConstraints = WTFMove(resultConstraints);
    }
    emitPatchpoint(block, patchpoint, results, WTFMove(patchArgs));
    return patchpoint;
}

auto AirIRGenerator::addCall(uint32_t functionIndex, const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ASSERT(signature.argumentCount() == args.size());

    m_makesCalls = true;

    for (unsigned i = 0; i < signature.returnCount(); ++i)
        results.append(tmpForType(signature.returnType(i)));

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
            auto* patchpoint = emitCallPatchpoint(isWasmBlock, signature, results, args);
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

            patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CCallHelpers::Call call = jit.threadSafePatchableNearCall();
                jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                    unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
                });
            });

            append(isWasmBlock, Jump);
            isWasmBlock->setSuccessors(continuation);
        }

        {
            auto jumpDestination = g64();
            append(isEmbedderBlock, Move, Arg::bigImm(Instance::offsetOfWasmToEmbedderStub(functionIndex)), jumpDestination);
            append(isEmbedderBlock, Add64, instanceValue(), jumpDestination);
            append(isEmbedderBlock, Move, Arg::addr(jumpDestination), jumpDestination);

            Vector<ConstrainedTmp> jumpArgs;
            jumpArgs.append({ jumpDestination, B3::ValueRep::SomeRegister });
            auto* patchpoint = emitCallPatchpoint(isEmbedderBlock, signature, results, args, WTFMove(jumpArgs));
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
            patchpoint->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                jit.call(params[params.proc().resultCount(params.value()->type())].gpr(), WasmEntryPtrTag);
            });

            append(isEmbedderBlock, Jump);
            isEmbedderBlock->setSuccessors(continuation);
        }

        m_currentBlock = continuation;
        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, currentInstance, continuation);
    } else {
        auto* patchpoint = emitCallPatchpoint(m_currentBlock, signature, results, args);
        // We need to clobber the size register since the LLInt always bounds checks
        if (m_mode == MemoryMode::Signaling || m_info.memory.isShared())
            patchpoint->clobberLate(RegisterSet { PinnedRegisterInfo::get().boundsCheckingSizeRegister });
        patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CCallHelpers::Call call = jit.threadSafePatchableNearCall();
            jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
            });
        });
    }

    return { };
}

auto AirIRGenerator::addCallIndirect(unsigned tableIndex, const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();
    ASSERT(signature.argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    m_makesCalls = true;
    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    ExpressionType callableFunctionBuffer = g64();
    ExpressionType instancesBuffer = g64();
    ExpressionType callableFunctionBufferLength = g64();
    {
        RELEASE_ASSERT(Arg::isValidAddrForm(FuncRefTable::offsetOfFunctions(), B3::Width64));
        RELEASE_ASSERT(Arg::isValidAddrForm(FuncRefTable::offsetOfInstances(), B3::Width64));
        RELEASE_ASSERT(Arg::isValidAddrForm(FuncRefTable::offsetOfLength(), B3::Width64));

        if (UNLIKELY(!Arg::isValidAddrForm(Instance::offsetOfTablePtr(m_numImportFunctions, tableIndex), B3::Width64))) {
            append(Move, Arg::bigImm(Instance::offsetOfTablePtr(m_numImportFunctions, tableIndex)), callableFunctionBufferLength);
            append(Add64, instanceValue(), callableFunctionBufferLength);
            append(Move, Arg::addr(callableFunctionBufferLength), callableFunctionBufferLength);
        } else
            append(Move, Arg::addr(instanceValue(), Instance::offsetOfTablePtr(m_numImportFunctions, tableIndex)), callableFunctionBufferLength);
        append(Move, Arg::addr(callableFunctionBufferLength, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
        append(Move, Arg::addr(callableFunctionBufferLength, FuncRefTable::offsetOfInstances()), instancesBuffer);
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

        // FIXME: This seems wasteful to do two checks just for a nicer error message.
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

    auto calleeInstance = g64();
    append(Move, Arg::index(instancesBuffer, calleeIndex, 8, 0), calleeInstance);

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

auto AirIRGenerator::addCallRef(const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    m_makesCalls = true;
    // Note: call ref can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    ExpressionType calleeFunction = args.takeLast();
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    ExpressionType calleeCode = g64();
    append(Move, Arg::addr(calleeFunction, WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation()), calleeCode); // Pointer to callee code.

    auto calleeInstance = g64();
    append(Move, Arg::addr(calleeFunction, WebAssemblyFunctionBase::offsetOfInstance()), calleeInstance);
    append(Move, Arg::addr(calleeInstance, JSWebAssemblyInstance::offsetOfInstance()), calleeInstance);

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

auto AirIRGenerator::emitIndirectCall(TypedTmp calleeInstance, ExpressionType calleeCode, const Signature& signature, const Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    auto currentInstance = g64();
    append(Move, instanceValue(), currentInstance);

    // Do a context switch if needed.
    {
        BasicBlock* doContextSwitch = m_code.addBlock();
        BasicBlock* continuation = m_code.addBlock();

        append(Branch64, Arg::relCond(MacroAssembler::Equal), calleeInstance, currentInstance);
        m_currentBlock->setSuccessors(continuation, doContextSwitch);

        auto* patchpoint = addPatchpoint(B3::Void);
        patchpoint->effects.writesPinned = true;
        // We pessimistically assume we're calling something with BoundsChecking memory.
        // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
        patchpoint->clobber(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg calleeInstance = params[0].gpr();
            GPRReg oldContextInstance = params[1].gpr();
            const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
            GPRReg baseMemory = pinnedRegs.baseMemoryPointer;
            ASSERT(calleeInstance != baseMemory);
            jit.loadPtr(CCallHelpers::Address(oldContextInstance, Instance::offsetOfCachedStackLimit()), baseMemory);
            jit.storePtr(baseMemory, CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedStackLimit()));
            jit.storeWasmContextInstance(calleeInstance);
            // FIXME: We should support more than one memory size register
            //   see: https://bugs.webkit.org/show_bug.cgi?id=162952
            ASSERT(pinnedRegs.boundsCheckingSizeRegister != calleeInstance);
            GPRReg scratch = params.gpScratch(0);

            jit.loadPtr(CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedBoundsCheckingSize()), pinnedRegs.boundsCheckingSizeRegister); // Bound checking size.
            jit.loadPtr(CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedMemory()), baseMemory); // Memory::void*.

            jit.cageConditionallyAndUntag(Gigacage::Primitive, baseMemory, pinnedRegs.boundsCheckingSizeRegister, scratch);
        });

        emitPatchpoint(doContextSwitch, patchpoint, Tmp(), calleeInstance, currentInstance);
        append(doContextSwitch, Jump);
        doContextSwitch->setSuccessors(continuation);

        m_currentBlock = continuation;
    }

    append(Move, Arg::addr(calleeCode), calleeCode);

    Vector<ConstrainedTmp> extraArgs;
    extraArgs.append(calleeCode);

    for (unsigned i = 0; i < signature.returnCount(); ++i)
        results.append(tmpForType(signature.returnType(i)));

    auto* patchpoint = emitCallPatchpoint(m_currentBlock, signature, results, args, WTFMove(extraArgs));

    // We need to clobber all potential pinned registers since we might be leaving the instance.
    // We pessimistically assume we're always calling something that is bounds checking so
    // because the wasm->wasm thunk unconditionally overrides the size registers.
    // FIXME: We should not have to do this, but the wasm->wasm stub assumes it can
    // use all the pinned registers as scratch: https://bugs.webkit.org/show_bug.cgi?id=172181

    patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.call(params[params.proc().resultCount(params.value()->type())].gpr(), WasmEntryPtrTag);
    });

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, currentInstance, m_currentBlock);

    return { };
}

void AirIRGenerator::unify(const ExpressionType dst, const ExpressionType source)
{
    ASSERT(isSubtype(source.type(), dst.type()));
    append(moveOpForValueType(dst.type()), source, dst);
}

void AirIRGenerator::unifyValuesWithBlock(const Stack& resultStack, const ResultList& result)
{
    ASSERT(result.size() <= resultStack.size());

    for (size_t i = 0; i < result.size(); ++i)
        unify(result[result.size() - 1 - i], resultStack[resultStack.size() - 1 - i]);
}

static void dumpExpressionStack(const CommaPrinter& comma, const AirIRGenerator::Stack& expressionStack)
{
    dataLog(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, expression.value());
}

void AirIRGenerator::dump(const ControlStack& controlStack, const Stack* stack)
{
    dataLogLn("Processing Graph:");
    dataLog(m_code);
    dataLogLn("With current block:", *m_currentBlock);
    dataLogLn("Control stack:");
    for (size_t i = controlStack.size(); i--;) {
        dataLog("  ", controlStack[i].controlData, ": ");
        CommaPrinter comma(", ", "");
        dumpExpressionStack(comma, *stack);
        stack = &controlStack[i].enclosedExpressionStack;
        dataLogLn();
    }
    dataLogLn("\n");
}

auto AirIRGenerator::origin() -> B3::Origin
{
    // FIXME: We should implement a way to give Inst's an origin.
    return B3::Origin();
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileAir(CompilationContext& compilationContext, const FunctionData& function, const Signature& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, uint32_t functionIndex, TierUpCount* tierUp)
{
    auto result = makeUnique<InternalFunction>();

    compilationContext.embedderEntrypointJIT = makeUnique<CCallHelpers>();
    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();

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
    
    procedure.setOptLevel(Options::webAssemblyBBQAirOptimizationLevel());

    AirIRGenerator irGenerator(info, procedure, result.get(), unlinkedWasmToWasmCalls, mode, functionIndex, tierUp, signature);
    FunctionParser<AirIRGenerator> parser(irGenerator, function.data.data(), function.data.size(), signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());


    for (BasicBlock* block : code) {
        for (size_t i = 0; i < block->numSuccessors(); ++i)
            block->successorBlock(i)->addPredecessor(block);
    }

    {
        if (UNLIKELY(shouldDumpIRAtEachPhase(B3::AirMode))) {
            dataLogLn("Generated patchpoints");
            for (B3::PatchpointValue** patch : irGenerator.patchpoints())
                dataLogLn(deepDump(procedure, *patch));
        }

        B3::Air::prepareForGeneration(code);
        B3::Air::generate(code, *compilationContext.wasmEntrypointJIT);
        compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
        result->entrypoint.calleeSaveRegisters = code.calleeSaveRegisterAtOffsetList();
    }

    return result;
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

        append(Move, Arg::isValidImmForm(-1) ? Arg::imm(-1) : Arg::bigImm(-1) , negOne);
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

    emitCCall(&operationPopcount32, result, arg);
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

    emitCCall(&operationPopcount64, result, arg);
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
    auto max = addConstant(Types::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
    auto min = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));

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
        jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
    });
    result = g32();
    emitPatchpoint(patchpoint, result, arg);

    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I32TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Types::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
    auto min = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));

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
    auto max = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
    auto min = addConstant(Types::F64, bitwise_cast<uint64_t>(-1.0));

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
    auto max = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
    auto min = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));

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
    auto max = addConstant(Types::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
    auto min = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));

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
    auto max = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
    auto min = addConstant(Types::F64, bitwise_cast<uint64_t>(-1.0));
    
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
        signBitConstant = addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));

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
    emitPatchpoint(m_currentBlock, patchpoint, Vector<TypedTmp, 8> { result }, WTFMove(args));
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::I64TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto max = addConstant(Types::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
    auto min = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));

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
    auto max = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
    auto min = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
    
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
        signBitConstant = addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));

    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    Vector<ConstrainedTmp, 2> args;
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
    emitPatchpoint(m_currentBlock, patchpoint, Vector<TypedTmp, 8> { result }, WTFMove(args));

    return { };
}

auto AirIRGenerator::addShift(Type type, B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
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
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualAndOrdered), arg0, arg1, result);
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
    return addFloatingPointMinOrMax(Types::F32, MinOrMax::Min, arg0, arg1, result);
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
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanAndOrdered), arg0, arg1, result);
    return { };
}

auto AirIRGenerator::addFloatingPointMinOrMax(Type floatType, MinOrMax minOrMax, ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    ASSERT(floatType.isF32() || floatType.isF64());
    result = tmpForType(floatType);

    BasicBlock* isEqual = m_code.addBlock();
    BasicBlock* notEqual = m_code.addBlock();
    BasicBlock* isLessThan = m_code.addBlock();
    BasicBlock* notLessThan = m_code.addBlock();
    BasicBlock* isGreaterThan = m_code.addBlock();
    BasicBlock* isNaN = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    auto branchOp = floatType.isF32() ? BranchFloat : BranchDouble;
    append(m_currentBlock, branchOp, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg0, arg1);
    m_currentBlock->setSuccessors(isEqual, notEqual);

    append(notEqual, branchOp, Arg::doubleCond(MacroAssembler::DoubleLessThanAndOrdered), arg0, arg1);
    notEqual->setSuccessors(isLessThan, notLessThan);

    append(notLessThan, branchOp, Arg::doubleCond(MacroAssembler::DoubleGreaterThanAndOrdered), arg0, arg1);
    notLessThan->setSuccessors(isGreaterThan, isNaN);

    auto andOp = floatType.isF32() ? AndFloat : AndDouble;
    auto orOp = floatType.isF32() ? OrFloat : OrDouble;
    append(isEqual, minOrMax == MinOrMax::Max ? andOp : orOp, arg0, arg1, result);
    append(isEqual, Jump);
    isEqual->setSuccessors(continuation);

    auto isLessThanResult = minOrMax == MinOrMax::Max ? arg1 : arg0;
    append(isLessThan, moveOpForValueType(floatType), isLessThanResult, result);
    append(isLessThan, Jump);
    isLessThan->setSuccessors(continuation);

    auto isGreaterThanResult = minOrMax == MinOrMax::Max ? arg0 : arg1;
    append(isGreaterThan, moveOpForValueType(floatType), isGreaterThanResult, result);
    append(isGreaterThan, Jump);
    isGreaterThan->setSuccessors(continuation);

    auto addOp = floatType.isF32() ? AddFloat : AddDouble;
    append(isNaN, addOp, arg0, arg1, result);
    append(isNaN, Jump);
    isNaN->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Max>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F32, MinOrMax::Max, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Mul>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointBinOp(Types::F64, MulDouble, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Div>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointBinOp(Types::F32, DivFloat, arg0, arg1, result);
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
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanAndOrdered), arg0, arg1, result);
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
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualAndOrdered), arg0, arg1, result);
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
    return addFloatingPointBinOp(Types::F64, DivDouble, arg0, arg1, result);
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
        auto constant = addConstant(Types::I32, bitwise_cast<uint32_t>(static_cast<float>(-0.0)));
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
    return addShift(Types::I64, RotateRight64, arg0, arg1, result);
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
        return addShift(Types::I64, RotateRight64, arg0, newShift, result);
    } else
        return addShift(Types::I64, RotateLeft64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Lt>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanAndOrdered), arg0, arg1, result);
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
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Le>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32Ge>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32ShrU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Types::I32, Urshift32, arg0, arg1, result);
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
    return addShift(Types::I32, Rshift32, arg0, arg1, result);
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
    return addShift(Types::I32, Lshift32, arg0, arg1, result);
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
    return addFloatingPointMinOrMax(Types::F64, MinOrMax::Min, arg0, arg1, result);
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
    return addFloatingPointBinOp(Types::F64, SubDouble, arg0, arg1, result);
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
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg0, arg1, result);
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
    return addShift(Types::I64, Rshift64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64ShrU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Types::I64, Urshift64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Sqrt>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    append(SqrtDouble, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Shl>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Types::I64, Lshift64, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Gt>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanAndOrdered), arg0, arg1, result);
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
        return addShift(Types::I32, RotateRight32, arg0, newShift, result);
    } else
        return addShift(Types::I32, RotateLeft32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32Rotr>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Types::I32, RotateRight32, arg0, arg1, result);
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

template<> auto AirIRGenerator::addOp<OpType::I32Extend8S>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(SignExtend8To32, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Extend16S>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
    append(SignExtend16To32, arg0, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Extend8S>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
    auto temp = g32();
    append(Move32, arg0, temp);
    append(SignExtend8To32, temp, temp);
    append(SignExtend32ToPtr, temp, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Extend16S>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
    auto temp = g32();
    append(Move32, arg0, temp);
    append(SignExtend16To32, temp, temp);
    append(SignExtend32ToPtr, temp, result);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Extend32S>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
    auto temp = g32();
    append(Move32, arg0, temp);
    append(SignExtend32ToPtr, temp, result);
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
        auto constant = addConstant(Types::I64, bitwise_cast<uint64_t>(static_cast<double>(-0.0)));
        auto temp = g64();
        append(MoveDoubleTo64, arg0, temp);
        append(Xor64, constant, temp);
        append(Move64ToDouble, temp, result);
    }
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64Max>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F64, MinOrMax::Max, arg0, arg1, result);
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

#endif // ENABLE(WEBASSEMBLY_B3JIT)
