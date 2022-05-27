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
#include "MathCommon.h"
#include "ScratchRegisterAllocator.h"
#include "WasmBranchHints.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmExceptionType.h"
#include "WasmFunctionParser.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmInstance.h"
#include "WasmMemory.h"
#include "WasmOSREntryData.h"
#include "WasmOpcodeOrigin.h"
#include "WasmOperations.h"
#include "WasmThunks.h"
#include "WasmTypeDefinitionInlines.h"
#include <limits>
#include <wtf/Box.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

using namespace B3::Air;

namespace {
static constexpr B3::Air::Opcode AddPtr = is64Bit() ? Add64 : Add32;
static constexpr B3::Air::Opcode MulPtr = is64Bit() ? Mul64 : Mul32;
static constexpr B3::Air::Opcode LeaPtr = is64Bit() ? Lea64 : Lea32;
static constexpr B3::Air::Opcode BranchPtr = is64Bit() ? Branch64 : Branch32;
static constexpr B3::Air::Opcode BranchTestPtr = is64Bit() ? BranchTest64 : BranchTest32;
}

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
        : m_type(Types::Void)
    {
    }

    TypedTmp(Tmp tmp, Type type)
        : m_tmp(tmp)
        , m_type(type)
    {
        ASSERT(type != Types::Void);
        ASSERT(!isGPPair());
    }

#if USE(JSVALUE32_64)
    TypedTmp(Tmp tmpLo, Tmp tmpHi, Type type)
        : m_tmp(tmpLo)
        , m_tmpHi(tmpHi)
        , m_type(type)
    {
        ASSERT(!!tmpLo);
        ASSERT(!!tmpHi);
        ASSERT(isGPPair());
    }
#endif

    TypedTmp(const TypedTmp&) = default;
    TypedTmp(TypedTmp&&) = default;
    TypedTmp& operator=(TypedTmp&&) = default;
    TypedTmp& operator=(const TypedTmp&) = default;

    bool operator==(const TypedTmp& other) const
    {
        bool same = m_tmp == other.m_tmp && m_type == other.m_type;
#if USE(JSVALUE32_64)
        same = same && m_tmpHi == other.m_tmpHi;
#endif
        return same;
    }

    bool operator!=(const TypedTmp& other) const
    {
        return !(*this == other);
    }

    explicit operator bool() const { return !!m_tmp; }

    operator Tmp() const
    {
        ASSERT(!isGPPair());
        return tmp();
    }

    operator Arg() const
    {
        ASSERT(!isGPPair());
        return Arg(tmp());
    }

    bool isGPPair() const { return is32Bit() && type().isGP64(); }

    Tmp tmp() const
    {
        ASSERT(!isGPPair()); // Use lo/hi instead
#if USE(JSVALUE32_64)
        ASSERT(!m_tmpHi);
#endif
        ASSERT(m_tmp);
        return m_tmp;
    }
    Tmp lo() const
    {
#if USE(JSVALUE64)
        UNREACHABLE_FOR_PLATFORM();
        return Tmp();
#elif USE(JSVALUE32_64)
        ASSERT(isGPPair()); // Use tmp instead
        ASSERT(m_tmp);
        return m_tmp;
#endif
    }
    Tmp hi() const
    {
#if USE(JSVALUE64)
        UNREACHABLE_FOR_PLATFORM();
        return Tmp();
#elif USE(JSVALUE32_64)
        ASSERT(isGPPair()); // Use tmp instead
        ASSERT(m_tmpHi);
        return m_tmpHi;
#endif
    }

    Type type() const { return m_type; }

    void dump(PrintStream& out) const
    {
#if USE(JSVALUE32_64)
        if (isGPPair()) {
            out.print("(", m_tmpHi, "/", m_tmp, ", ", m_type.kind, ", ", m_type.index, ")");
            return;
        }
#endif
        out.print("(", m_tmp, ", ", m_type.kind, ", ", m_type.index, ")");
    }

private:

    Tmp m_tmp { };
#if USE(JSVALUE32_64)
    Tmp m_tmpHi { }; // More significant 32-bits of I64 value
#endif
    Type m_type;
};

class AirIRGenerator {
public:
    using ExpressionType = TypedTmp;
    using ResultList = Vector<ExpressionType, 8>;

    struct ControlData {
        ControlData(B3::Origin, BlockSignature result, ResultList resultTmps, BlockType type, BasicBlock* continuation, BasicBlock* special = nullptr)
            : controlBlockType(type)
            , continuation(continuation)
            , special(special)
            , results(resultTmps)
            , returnType(result)
        {
        }

        ControlData(B3::Origin, BlockSignature result, ResultList resultTmps, BlockType type, BasicBlock* continuation, unsigned tryStart, unsigned tryDepth)
            : controlBlockType(type)
            , continuation(continuation)
            , special(nullptr)
            , results(resultTmps)
            , returnType(result)
            , m_tryStart(tryStart)
            , m_tryCatchDepth(tryDepth)
        {
        }

        ControlData()
        {
        }

        static bool isIf(const ControlData& control) { return control.blockType() == BlockType::If; }
        static bool isTry(const ControlData& control) { return control.blockType() == BlockType::Try; }
        static bool isAnyCatch(const ControlData& control) { return control.blockType() == BlockType::Catch; }
        static bool isCatch(const ControlData& control) { return isAnyCatch(control) && control.catchKind() == CatchKind::Catch; }
        static bool isTopLevel(const ControlData& control) { return control.blockType() == BlockType::TopLevel; }
        static bool isLoop(const ControlData& control) { return control.blockType() == BlockType::Loop; }
        static bool isBlock(const ControlData& control) { return control.blockType() == BlockType::Block; }

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
            case BlockType::Try:
                out.print("Try: ");
                break;
            case BlockType::Catch:
                out.print("Catch: ");
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

        FunctionArgCount branchTargetArity() const
        {
            if (blockType() == BlockType::Loop)
                return returnType->as<FunctionSignature>()->argumentCount();
            return returnType->as<FunctionSignature>()->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (blockType() == BlockType::Loop)
                return returnType->as<FunctionSignature>()->argumentType(i);
            return returnType->as<FunctionSignature>()->returnType(i);
        }

        void convertTryToCatch(unsigned tryEndCallSiteIndex, TypedTmp exception)
        {
            ASSERT(blockType() == BlockType::Try);
            controlBlockType = BlockType::Catch;
            m_catchKind = CatchKind::Catch;
            m_tryEnd = tryEndCallSiteIndex;
            m_exception = exception;
        }

        void convertTryToCatchAll(unsigned tryEndCallSiteIndex, TypedTmp exception)
        {
            ASSERT(blockType() == BlockType::Try);
            controlBlockType = BlockType::Catch;
            m_catchKind = CatchKind::CatchAll;
            m_tryEnd = tryEndCallSiteIndex;
            m_exception = exception;
        }

        unsigned tryStart() const
        {
            ASSERT(controlBlockType == BlockType::Try || controlBlockType == BlockType::Catch);
            return m_tryStart;
        }

        unsigned tryEnd() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_tryEnd;
        }

        unsigned tryDepth() const
        {
            ASSERT(controlBlockType == BlockType::Try || controlBlockType == BlockType::Catch);
            return m_tryCatchDepth;
        }

        CatchKind catchKind() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_catchKind;
        }

        TypedTmp exception() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_exception;
        }

    private:
        friend class AirIRGenerator;
        BlockType controlBlockType;
        BasicBlock* continuation;
        BasicBlock* special;
        ResultList results;
        BlockSignature returnType;
        unsigned m_tryStart;
        unsigned m_tryEnd;
        unsigned m_tryCatchDepth;
        CatchKind m_catchKind;
        TypedTmp m_exception;
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

    AirIRGenerator(const ModuleInformation&, B3::Procedure&, InternalFunction*, Vector<UnlinkedWasmToWasmCall>&, MemoryMode, unsigned functionIndex, TierUpCount*, const TypeDefinition&, unsigned& osrEntryScratchBufferSize);

    void finalizeEntrypoints();

    PartialResult WARN_UNUSED_RETURN addArguments(const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);
    ExpressionType addConstant(BasicBlock*, Type, uint64_t);
    void materializeConstant(BasicBlock*, uint64_t, ExpressionType result);
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

    // GC
    PartialResult WARN_UNUSED_RETURN addRttCanon(uint32_t typeIndex, ExpressionType& result);

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

    PartialResult WARN_UNUSED_RETURN addTry(BlockSignature, Stack& enclosingStack, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned exceptionIndex, const TypeDefinition&, Stack&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& expressionStack = { });

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&) { return { }; }

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN emitIndirectCall(TypedTmp calleeInstance, ExpressionType calleeCode, const TypeDefinition&, const Vector<ExpressionType>& args, ResultList&);
    std::pair<B3::PatchpointValue*, PatchpointExceptionHandle> WARN_UNUSED_RETURN emitCallPatchpoint(BasicBlock*, const TypeDefinition&, const ResultList& results, const Vector<TypedTmp>& args, Vector<ConstrainedTmp> extraArgs = { });

    PartialResult addShift(Type, B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
#if USE(JSVALUE32_64)
    PartialResult addShift64(B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
    PartialResult addRotate64(B3::Air::Opcode, ExpressionType value, ExpressionType shift, ExpressionType& result);
#endif
    PartialResult addCompare(Type, MacroAssembler::RelationalCondition, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addIntegerSub(B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);
    PartialResult addFloatingPointAbs(B3::Air::Opcode, ExpressionType value, ExpressionType& result);
    PartialResult addFloatingPointBinOp(Type, B3::Air::Opcode, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    void dump(const ControlStack&, const Stack* expressionStack);
    void setParser(FunctionParser<AirIRGenerator>* parser) { m_parser = parser; };
    void didFinishParsingLocals() { }
    void didPopValueFromStack() { }

    Tmp emitCatchImpl(CatchKind, ControlType&, unsigned exceptionIndex = 0);
    template <size_t inlineCapacity>
    PatchpointExceptionHandle preparePatchpointForExceptions(B3::PatchpointValue*, Vector<ConstrainedTmp, inlineCapacity>& args);

    const Bag<B3::PatchpointValue*>& patchpoints() const
    {
        return m_patchpoints;
    }

    void addStackMap(unsigned callSiteIndex, StackMap&& stackmap)
    {
        m_stackmaps.add(CallSiteIndex(callSiteIndex), WTFMove(stackmap));
    }

    StackMaps&& takeStackmaps()
    {
        return WTFMove(m_stackmaps);
    }

    Vector<UnlinkedHandlerInfo>&& takeExceptionHandlers()
    {
        return WTFMove(m_exceptionHandlers);
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
        return m_code.newTmp(bank);
    }

    TypedTmp gSome64(Type type)
    {
#if USE(JSVALUE64)
        return { newTmp(B3::GP), type };
#elif USE(JSVALUE32_64)
        return { newTmp(B3::GP), newTmp(B3::GP), type };
#endif
    }

    TypedTmp g32() { return { newTmp(B3::GP), Types::I32 }; }
    TypedTmp g64() { return gSome64(Types::I64); }
    TypedTmp gPtr() { return is64Bit() ? g64() : g32(); }
    TypedTmp gRef(Type type) { return gSome64(type); }
    TypedTmp gRtt() { return gSome64(Types::Rtt); }
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
        case TypeKind::Ref:
        case TypeKind::RefNull:
        case TypeKind::Externref:
            return gRef(type);
        case TypeKind::Rtt:
            return gRtt();
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
        ResultList result(signature->as<FunctionSignature>()->returnCount());
        for (unsigned i = 0; i < signature->as<FunctionSignature>()->returnCount(); ++i)
            result[i] = tmpForType(signature->as<FunctionSignature>()->returnType(i));
        return result;
    }

    B3::PatchpointValue* addPatchpoint(B3::Type type)
    {
#if USE(JSVALUE32_64) && ASSERT_ENABLED
        for (B3::Type kind : (type.isTuple() ? m_proc.tupleForType(type) : Vector<B3::Type> { type }))
            ASSERT(kind != B3::Int64);
#endif
        auto* result = m_proc.add<B3::PatchpointValue>(type, B3::Origin());
        if (UNLIKELY(shouldDumpIRAtEachPhase(B3::AirMode)))
            m_patchpoints.add(result);
        return result;
    }

    template <typename ...Args>
    void emitPatchpoint(B3::PatchpointValue* patch, ExpressionType result, Args... theArgs)
    {
        emitPatchpoint(m_currentBlock, patch, result, std::forward<Args>(theArgs)...);
    }

    template <typename ...Args>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, ExpressionType result, Args... theArgs)
    {
        emitPatchpoint(basicBlock, patch, Vector<ExpressionType, 8> { result }, Vector<ConstrainedTmp, sizeof...(Args)>::from(theArgs...));
    }

    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, ExpressionType result)
    {
        emitPatchpoint(basicBlock, patch, Vector<ExpressionType, 8> { result }, Vector<ConstrainedTmp>());
    }

    template <size_t inlineSize>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, ExpressionType result, Vector<ConstrainedTmp, inlineSize>&& args)
    {
        emitPatchpoint(basicBlock, patch, Vector<ExpressionType, 8> { result }, WTFMove(args));
    }

    template <size_t inlineSize>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, const Vector<ExpressionType, 8>& results, Vector<ConstrainedTmp, inlineSize>&& args)
    {
        if (!m_patchpointSpecial)
            m_patchpointSpecial = static_cast<B3::PatchpointSpecial*>(m_code.addSpecial(makeUnique<B3::PatchpointSpecial>()));

        Inst inst(Patch, patch, Arg::special(m_patchpointSpecial));
        Vector<Inst, 1> resultMovs;
        switch (patch->type().kind()) {
        case B3::Void:
            break;
        default: {
            ASSERT(results.size());
            ASSERT(results.size() == patch->resultConstraints.size());
            for (unsigned i = 0; i < results.size(); ++i) {
                ASSERT(!results[i].isGPPair()); // GP Pair types should have been broken down
                switch (patch->resultConstraints[i].kind()) {
                case B3::ValueRep::StackArgument: {
                    Arg arg = Arg::callArg(patch->resultConstraints[i].offsetFromSP());
                    inst.args.append(arg);
                    resultMovs.append(Inst(B3::Air::moveForType(m_proc.typeAtOffset(patch->type(), i)), nullptr, arg, results[i].tmp()));
                    break;
                }
                case B3::ValueRep::Register: {
                    inst.args.append(Tmp(patch->resultConstraints[i].reg()));
                    resultMovs.append(Inst(B3::Air::relaxedMoveForType(m_proc.typeAtOffset(patch->type(), i)), nullptr, Tmp(patch->resultConstraints[i].reg()), results[i].tmp()));
                    break;
                }
                case B3::ValueRep::SomeRegister: {
                    inst.args.append(results[i].tmp());
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
            B3::Value* dummyValue = m_proc.addConstant(B3::Origin(), tmp.tmp.isGP() ? B3::pointerType() : B3::Double, 0);
            patch->append(dummyValue, tmp.rep);
            switch (tmp.rep.kind()) {
            // B3::Value propagates (Late)ColdAny information and later Air will allocate appropriate stack.
            case B3::ValueRep::ColdAny: 
            case B3::ValueRep::LateColdAny:
            case B3::ValueRep::SomeRegister:
                inst.args.append(tmp.tmp);
                break;
            case B3::ValueRep::Register:
                patch->earlyClobbered().clear(tmp.rep.reg());
                append(basicBlock, tmp.tmp.isGP() ? Move : MoveDouble, tmp.tmp, tmp.rep.reg());
                inst.args.append(Tmp(tmp.rep.reg()));
                break;
            case B3::ValueRep::StackArgument: {
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
            inst.args.append(gPtr().tmp());
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
        B3::Type resultType = toB3Type(result.type());

        auto makeDummyValue = [&] (TypedTmp tmp) {
            // FIXME: This is less than ideal to create dummy values just to satisfy Air's
            // validation. We should abstract CCall enough so we're not reliant on arguments
            // to the B3::CCallValue.
            // https://bugs.webkit.org/show_bug.cgi?id=194040
            return m_proc.addConstant(B3::Origin(), toB3Type(tmp.type()), 0);
        };

        B3::Value* dummyFunc = m_proc.addConstant(B3::Origin(), B3::pointerType(), bitwise_cast<uintptr_t>(func));
        B3::Value* origin = m_proc.add<B3::CCallValue>(resultType, B3::Origin(), B3::Effects::none(), dummyFunc, makeDummyValue(theArgs)...);

        Inst inst(CCall, origin);

        Tmp callee = gPtr();
        append(block, Move, Arg::immPtr(tagCFunctionPtr<void*, OperationPtrTag>(func)), callee);
        inst.args.append(callee);

        if (result) {
            if (result.isGPPair()) {
                inst.args.append(result.lo());
                inst.args.append(result.hi());
            } else
                inst.args.append(result.tmp());
        }

        for (TypedTmp& arg : Vector<TypedTmp, sizeof...(Args)>::from(theArgs...)) {
            if (arg.isGPPair()) {
                inst.args.append(arg.lo());
                inst.args.append(arg.hi());
                continue;
            }
            inst.args.append(arg.tmp());
        }

        validateInst(inst);
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

    void emitLoad(Tmp base, size_t offset, const TypedTmp& result)
    {
        if (!Arg::isValidAddrForm(offset, B3::widthForType(toB3Type(result.type())))) {
            auto temp = gPtr();
            append(Move, Arg::bigImm(offset), temp);
            append(AddPtr, temp, base, temp);
            base = temp.tmp();
            offset = 0;
        }

        if (result.isGPPair()) {
            append(Move, Arg::addr(base, offset), result.lo());
            append(Move, Arg::addr(base, offset + 4), result.hi());
            return;
        }

        append(moveOpForValueType(result.type()), Arg::addr(base, offset), result);
    }

    void emitStore(const TypedTmp& value, Tmp base, size_t offset)
    {
        if (!Arg::isValidAddrForm(offset, B3::widthForType(toB3Type(value.type())))) {
            auto temp = gPtr();
            append(Move, Arg::bigImm(offset), temp);
            append(AddPtr, temp, base, temp);
            base = temp.tmp();
            offset = 0;
        }

        if (value.isGPPair()) {
            append(Move, value.lo(), Arg::addr(base, offset));
            append(Move, value.hi(), Arg::addr(base, offset + 4));
            return;
        }

        append(moveOpForValueType(value.type()), value, Arg::addr(base, offset));
    }

    void emitMove(const TypedTmp& src, const TypedTmp& dst)
    {
        if (src == dst)
            return;
        ASSERT(isSubtype(src.type(), dst.type()));

        if (src.isGPPair()) {
            append(Move, src.lo(), dst.lo());
            append(Move, src.hi(), dst.hi());
            return;
        }

        append(moveOpForValueType(src.type()), src, dst);
    }

    void emitThrowException(CCallHelpers&, ExceptionType);

    void emitEntryTierUpCheck();
    void emitLoopTierUpCheck(uint32_t loopIndex, const Vector<TypedTmp>& liveValues);

    void emitWriteBarrierForJSWrapper();
    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    ExpressionType emitLoadOp(LoadOpType, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicLoadOp(ExtAtomicOpType, Type, ExpressionType pointer, uint32_t offset);
    void emitAtomicStoreOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicBinaryRMWOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicCompareExchange(ExtAtomicOpType, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t offset);

    void sanitizeAtomicResult(ExtAtomicOpType, TypedTmp source, TypedTmp dest);
    void sanitizeAtomicResult(ExtAtomicOpType, TypedTmp result);
    template <typename InputType>
    TypedTmp appendGeneralAtomic(ExtAtomicOpType, B3::Air::Opcode nonAtomicOpcode, B3::Commutativity, InputType, Arg addrArg, TypedTmp result);
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

    template <typename Function>
    void forEachLiveValue(Function);

    bool useSignalingMemory() const
    {
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
        return m_mode == MemoryMode::Signaling;
#else
        return false;
#endif
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
    BasicBlock* m_mainEntrypointStart { nullptr };
    Vector<TypedTmp> m_locals;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    GPRReg m_memoryBaseGPR { InvalidGPRReg };
    GPRReg m_boundsCheckingSizeGPR { InvalidGPRReg };
    GPRReg m_wasmContextInstanceGPR { InvalidGPRReg };
    GPRReg m_prologueWasmContextGPR { InvalidGPRReg };
    bool m_makesCalls { false };

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

    RefPtr<B3::Air::PrologueGenerator> m_prologueGenerator;

    Vector<BasicBlock*> m_catchEntrypoints;

    Checked<unsigned> m_tryCatchDepth { 0 };
    Checked<unsigned> m_callSiteIndex { 0 };
    StackMaps m_stackmaps;
    Vector<UnlinkedHandlerInfo> m_exceptionHandlers;

    Vector<std::pair<BasicBlock*, Vector<TypedTmp>>> m_loopEntryVariableData;
    unsigned& m_osrEntryScratchBufferSize;
};

// Memory accesses in WebAssembly have unsigned 32-bit offsets, whereas they have signed 32-bit offsets in B3.
int32_t AirIRGenerator::fixupPointerPlusOffset(ExpressionType& ptr, uint32_t offset)
{
    if (static_cast<uint64_t>(offset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        auto previousPtr = ptr;
        ptr = gPtr();
        auto constant = gPtr();
        append(Move, Arg::bigImm(offset), constant);
        append(AddPtr, constant, previousPtr, ptr);
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
        emitPatchpoint(block, patchpoint, TypedTmp(), instance);
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
    emitPatchpoint(block, patchpoint, TypedTmp(), instance);
}

AirIRGenerator::AirIRGenerator(const ModuleInformation& info, B3::Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, unsigned functionIndex, TierUpCount* tierUp, const TypeDefinition& signature, unsigned& osrEntryScratchBufferSize)
    : m_info(info)
    , m_mode(mode)
    , m_functionIndex(functionIndex)
    , m_tierUp(tierUp)
    , m_proc(procedure)
    , m_code(m_proc.code())
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_numImportFunctions(info.importFunctionCount())
    , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
{
    m_currentBlock = m_code.addBlock();
    m_rootBlock = m_currentBlock;

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623
    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();

    m_memoryBaseGPR = pinnedRegs.baseMemoryPointer;
    if (m_memoryBaseGPR != InvalidGPRReg)
        m_code.pinRegister(m_memoryBaseGPR);

    m_wasmContextInstanceGPR = pinnedRegs.wasmContextInstancePointer;
    if (!Context::useFastTLS())
        m_code.pinRegister(m_wasmContextInstanceGPR);

    if (mode == MemoryMode::BoundsChecking) {
        m_boundsCheckingSizeGPR = pinnedRegs.boundsCheckingSizeRegister;
        if (m_boundsCheckingSizeGPR != InvalidGPRReg)
            m_code.pinRegister(m_boundsCheckingSizeGPR);
    }

    m_prologueWasmContextGPR = Context::useFastTLS() ? wasmCallingConvention().prologueScratchGPRs[1] : m_wasmContextInstanceGPR;

    m_prologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([=, this] (CCallHelpers& jit, B3::Air::Code& code) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        code.emitDefaultPrologue(jit);

        {
            GPRReg calleeGPR = wasmCallingConvention().prologueScratchGPRs[0];
            auto moveLocation = jit.moveWithPatch(MacroAssembler::TrustedImmPtr(nullptr), calleeGPR);
            jit.addLinkTask([compilation, moveLocation] (LinkBuffer& linkBuffer) {
                compilation->calleeMoveLocations.append(linkBuffer.locationOf<WasmEntryPtrTag>(moveLocation));
            });
            CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
            jit.storePtr(calleeGPR, calleeSlot.withOffset(PayloadOffset));
#if USE(JSVALUE32_64)
            jit.store32(CCallHelpers::TrustedImm32(JSValue::WasmTag), calleeSlot.withOffset(TagOffset));
#endif
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
                    jit.loadWasmContextInstance(m_prologueWasmContextGPR);

                jit.addPtr(CCallHelpers::TrustedImm32(-checkSize), GPRInfo::callFrameRegister, scratch);
                MacroAssembler::JumpList overflow;
                if (UNLIKELY(needUnderflowCheck))
                    overflow.append(jit.branchPtr(CCallHelpers::Above, scratch, GPRInfo::callFrameRegister));
                overflow.append(jit.branchPtr(CCallHelpers::Below, scratch, CCallHelpers::Address(m_prologueWasmContextGPR, Instance::offsetOfCachedStackLimit())));
                jit.addLinkTask([overflow] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(overflow, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
                });
            } else if (m_usesInstanceValue && Context::useFastTLS()) {
                // No overflow check is needed, but the instance values still needs to be correct.
                jit.loadWasmContextInstance(m_prologueWasmContextGPR);
            }

            if (m_catchEntrypoints.size()) {
                GPRReg scratch = wasmCallingConvention().prologueScratchGPRs[0];
                jit.loadPtr(CCallHelpers::Address(m_prologueWasmContextGPR, Instance::offsetOfOwner()), scratch);
                jit.storeCell(scratch, CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::thisArgument * sizeof(Register)));
            }
        }
    });

    if (Context::useFastTLS()) {
        m_instanceValue = gPtr();
        // FIXME: Would be nice to only do this if we use instance value.
        append(Move, Tmp(m_prologueWasmContextGPR), m_instanceValue);
    } else
        m_instanceValue = { Tmp(m_prologueWasmContextGPR), Types::IPtr };

    append(EntrySwitch);
    m_mainEntrypointStart = m_code.addBlock();
    m_currentBlock = m_mainEntrypointStart;

    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);
    ASSERT(wasmCallInfo.params.size() == signature.as<FunctionSignature>()->argumentCount());
    ASSERT(!m_locals.size());
    m_locals.grow(wasmCallInfo.params.size());
    for (unsigned i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        Type type = signature.as<FunctionSignature>()->argumentType(i);
        ValueLocation location = wasmCallInfo.params[i];
        m_locals[i] = tmpForType(type);
        if (location.isStack())
            emitLoad(Tmp(GPRInfo::callFrameRegister), location.offsetFromFP(), m_locals[i]);
        else if (location.isFPR())
            append(type.kind == TypeKind::F32 ? MoveFloat : MoveDouble, Tmp(location.fpr()), m_locals[i].tmp());
        else if (type.kind == TypeKind::I32)
            append(Move32, Tmp(location.jsr().payloadGPR()), m_locals[i].tmp());
        else {
#if USE(JSVALUE64)
            append(Move, Tmp(location.jsr().payloadGPR()), m_locals[i].tmp());
#elif USE(JSVALUE32_64)
            append(Move, Tmp(location.jsr().payloadGPR()), m_locals[i].lo());
            append(Move, Tmp(location.jsr().tagGPR()), m_locals[i].hi());
#endif
        }
    }

    emitEntryTierUpCheck();
}

void AirIRGenerator::finalizeEntrypoints()
{
    unsigned numEntrypoints = Checked<unsigned>(1) + m_catchEntrypoints.size() + m_loopEntryVariableData.size();
    m_proc.setNumEntrypoints(numEntrypoints);
    m_code.setPrologueForEntrypoint(0, Ref<B3::Air::PrologueGenerator>(*m_prologueGenerator));
    for (unsigned i = 1 + m_catchEntrypoints.size(); i < numEntrypoints; ++i)
        m_code.setPrologueForEntrypoint(i, Ref<B3::Air::PrologueGenerator>(*m_prologueGenerator));

    if (m_catchEntrypoints.size()) {
        Ref<B3::Air::PrologueGenerator> catchPrologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([this] (CCallHelpers& jit, B3::Air::Code& code) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            emitCatchPrologueShared(code, jit);

            if (Context::useFastTLS()) {
                // Shared prologue expects this in this register when entering the function using fast TLS.
                jit.loadWasmContextInstance(m_prologueWasmContextGPR);
            }
        });

        for (unsigned i = 0; i < m_catchEntrypoints.size(); ++i)
            m_code.setPrologueForEntrypoint(1 + i, catchPrologueGenerator.copyRef());
    }

    BasicBlock::SuccessorList successors;
    successors.append(m_mainEntrypointStart);
    successors.appendVector(m_catchEntrypoints);

    for (auto& pair : m_loopEntryVariableData) {
        BasicBlock* loopBody = pair.first;
        BasicBlock* entry = m_code.addBlock();
        successors.append(entry);
        m_currentBlock = entry;

        auto& temps = pair.second;
        m_osrEntryScratchBufferSize = std::max(m_osrEntryScratchBufferSize, static_cast<unsigned>(temps.size()));
        Tmp basePtr = Tmp(GPRInfo::argumentGPR0);

        for (size_t i = 0; i < temps.size(); ++i) {
            size_t offset = static_cast<size_t>(i) * sizeof(uint64_t);
            emitLoad(basePtr, offset, temps[i]);
        }

        append(Jump);
        entry->setSuccessors(loopBody);
    }

    RELEASE_ASSERT(numEntrypoints == successors.size());
    m_rootBlock->successors() = successors;
}

B3::Type AirIRGenerator::toB3ResultType(BlockSignature returnType)
{
    auto signature = returnType->as<FunctionSignature>();

    if (signature->returnsVoid())
        return B3::Void;

    // On 32-bit platforms, 64-bit integer types are returned as two 32-bit values
    if (signature->returnCount() == 1 && (is64Bit() || !signature->returnType(0).isGP64()))
        return toB3Type(signature->returnType(0));

    auto result = m_tupleMap.ensure(returnType, [&] {
        Vector<B3::Type> result;
        for (unsigned i = 0; i < signature->returnCount(); ++i) {
            Type type = signature->returnType(i);
            if (is32Bit() && type.isGP64()) {
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

void AirIRGenerator::restoreWebAssemblyGlobalState(RestoreCachedStackLimit restoreCachedStackLimit, const MemoryInformation& memory, TypedTmp instance, BasicBlock* block)
{
    restoreWasmContextInstance(block, instance);

    if (restoreCachedStackLimit == RestoreCachedStackLimit::Yes) {
        // The Instance caches the stack limit, but also knows where its canonical location is.
        RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfPointerToActualStackLimit(), B3::pointerWidth()));
        RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfCachedStackLimit(), B3::pointerWidth()));
        auto temp = gPtr();
        append(block, Move, Arg::addr(instanceValue(), Instance::offsetOfPointerToActualStackLimit()), temp);
        append(block, Move, Arg::addr(temp), temp);
        append(block, Move, temp, Arg::addr(instanceValue(), Instance::offsetOfCachedStackLimit()));
    }

    if (!!memory && !isARM()) {
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

        emitPatchpoint(block, patchpoint, TypedTmp(), instance);
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

template <typename Function>
void AirIRGenerator::forEachLiveValue(Function function)
{
    for (const auto& local : m_locals)
        function(local);
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        ControlData& data = m_parser->controlStack()[controlIndex].controlData;
        Stack& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (const auto& tmp : expressionStack)
            function(tmp.value());
        if (ControlType::isAnyCatch(data))
            function(data.exception());
    }
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
        case TypeKind::Ref:
        case TypeKind::RefNull:
#if USE(JSVALUE64)
            append(Move, Arg::imm(JSValue::encode(jsNull())), local);
#elif USE(JSVALUE32_64)
            append(Move, Arg::bigImmLo32(JSValue::encode(jsNull())), local.lo());
            append(Move, Arg::bigImmHi32(JSValue::encode(jsNull())), local.hi());
#endif
            break;
#if USE(JSVALUE64)
        case TypeKind::I32:
        case TypeKind::I64:
            append(Xor64, local, local);
            break;
#elif USE(JSVALUE32_64)
        case TypeKind::I32:
            append(Move, Arg::imm(0), local);
            break;
        case TypeKind::I64:
            append(Move, Arg::imm(0), local.lo());
            append(Move, Arg::imm(0), local.hi());
            break;
#endif
        case TypeKind::F32: {
            auto temp = g32();
            // IEEE 754 "0" is just int32/64 zero.
            if constexpr (isX86())
                append(Xor32, temp, temp);
            else
                append(Move, Arg::imm(0), temp);
            append(Move32ToFloat, temp, local);
            break;
        }
        case TypeKind::F64:
            append(MoveZeroToDouble, local);
            break;
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
    materializeConstant(block, value, result);
    return result;
}

void AirIRGenerator::materializeConstant(BasicBlock*block, uint64_t value, ExpressionType result)
{
    switch (result.type().kind) {
    case TypeKind::I32:
        append(block, Move, Arg::bigImm(value), result);
        break;
    case TypeKind::I64:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
#if USE(JSVALUE64)
        append(block, Move, Arg::bigImm(value), result);
#elif USE(JSVALUE32_64)
        append(block, Move, Arg::bigImmLo32(value), result.lo());
        append(block, Move, Arg::bigImmHi32(value), result.hi());
#endif
        break;
    case TypeKind::F32: {
        auto tmp = g32();
        append(block, Move, Arg::bigImm(value), tmp);
        append(block, Move32ToFloat, tmp, result);
        break;
    }
    case TypeKind::F64: {
        auto tmp = g64();
#if USE(JSVALUE64)
        append(block, Move, Arg::bigImm(value), tmp);
        append(block, Move64ToDouble, tmp, result);
#elif USE(JSVALUE32_64)
        append(block, Move, Arg::bigImmLo32(value), tmp.lo());
        append(block, Move, Arg::bigImmHi32(value), tmp.hi());
        append(block, Move64ToDouble, tmp.hi(), tmp.lo(), result);
#endif
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

auto AirIRGenerator::addBottom(BasicBlock* block, Type type) -> ExpressionType
{
    append(block, B3::Air::Oops);
    return addConstant(type, 0);
}

auto AirIRGenerator::addArguments(const TypeDefinition& signature) -> PartialResult
{
    RELEASE_ASSERT(m_locals.size() == signature.as<FunctionSignature>()->argumentCount()); // We handle arguments in the prologue
    return { };
}

auto AirIRGenerator::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    ASSERT(value);
    result = tmpForType(Types::I32);
#if USE(JSVALUE64)
    auto tmp = g64();
    append(Move, Arg::bigImm(JSValue::encode(jsNull())), tmp);
    append(Compare64, Arg::relCond(MacroAssembler::Equal), value, tmp, result);
#elif USE(JSVALUE32_64)
    auto tmp = g32();
    append(Move, Arg::bigImm(JSValue::NullTag), tmp);
    append(Compare32, Arg::relCond(MacroAssembler::Equal), value.hi(), tmp, result);
#endif
    return { };
}

auto AirIRGenerator::addRefFunc(uint32_t index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    if (Options::useWebAssemblyTypedFunctionReferences()) {
        TypeIndex typeIndex = m_info.typeIndexFromFunctionIndexSpace(index);
        result = tmpForType(Type { TypeKind::Ref, Nullable::No, typeIndex });
    } else
        result = tmpForType(Types::Funcref);
    emitCCall(&operationWasmRefFunc, result, instanceValue(), addConstant(Types::I32, index));

    return { };
}

auto AirIRGenerator::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index);

    ASSERT(index.type().isI32());
    result = tmpForType(m_info.tables[tableIndex].wasmType());
    ASSERT(widthForType(toB3Type(result.type())) == B3::Width64);

    emitCCall(&operationGetWasmTableElement, result, instanceValue(), addConstant(Types::I32, tableIndex), index);

    {
        BasicBlock* continuation = nullptr;
        emitCheck([&] {
#if USE(JSVALUE64)
            return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
#elif USE(JSVALUE32_64)
            BasicBlock* checkLo = m_code.addBlock();
            continuation = m_code.addBlock();
            append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), result.hi(), result.hi());
            m_currentBlock->setSuccessors(continuation, checkLo);
            m_currentBlock = checkLo;
            return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result.lo(), result.lo());
#endif
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
        });
        if (continuation) {
            append(Jump);
            m_currentBlock->setSuccessors(continuation);
            m_currentBlock = continuation;
        }
    }

    return { };
}

auto AirIRGenerator::addTableSet(unsigned tableIndex, ExpressionType index, ExpressionType value) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index);
    ASSERT(index.type().isI32());
    ASSERT(value);

    auto shouldThrow = g32();
    emitCCall(&operationSetWasmTableElement, shouldThrow, instanceValue(), addConstant(Types::I32, tableIndex), index, value);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), shouldThrow, shouldThrow);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    ASSERT(dstOffset);
    ASSERT(dstOffset.type().isI32());

    ASSERT(srcOffset);
    ASSERT(srcOffset.type().isI32());

    ASSERT(length);
    ASSERT(length.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmTableInit, result, instanceValue(),
        addConstant(Types::I32, elementIndex),
        addConstant(Types::I32, tableIndex),
        dstOffset, srcOffset, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
    ASSERT(fill);
    ASSERT(isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()));
    ASSERT(delta);
    ASSERT(delta.type().isI32());
    result = tmpForType(Types::I32);

    emitCCall(&operationWasmTableGrow, result, instanceValue(), addConstant(Types::I32, tableIndex), fill, delta);

    return { };
}

auto AirIRGenerator::addTableFill(unsigned tableIndex, ExpressionType offset, ExpressionType fill, ExpressionType count) -> PartialResult
{
    ASSERT(fill);
    ASSERT(isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()));
    ASSERT(offset);
    ASSERT(offset.type().isI32());
    ASSERT(count);
    ASSERT(count.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(&operationWasmTableFill, result, instanceValue(), addConstant(Types::I32, tableIndex), offset, fill, count);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    ASSERT(dstOffset);
    ASSERT(dstOffset.type().isI32());

    ASSERT(srcOffset);
    ASSERT(srcOffset.type().isI32());

    ASSERT(length);
    ASSERT(length.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmTableCopy, result, instanceValue(),
        addConstant(Types::I32, dstTableIndex),
        addConstant(Types::I32, srcTableIndex),
        dstOffset, srcOffset, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

auto AirIRGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    TypedTmp local = m_locals[index];
    ASSERT(local);
    result = tmpForType(local.type());
    emitMove(local, result);
    return { };
}

auto AirIRGenerator::addUnreachable() -> PartialResult
{
    B3::PatchpointValue* unreachable = addPatchpoint(B3::Void);
    unreachable->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::Unreachable);
    });
    unreachable->effects.terminal = true;
    emitPatchpoint(unreachable, TypedTmp());
    return { };
}

auto AirIRGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = g32();
    emitCCall(&operationGrowMemory, result, TypedTmp { Tmp(GPRInfo::callFrameRegister), Types::IPtr }, instanceValue(), delta);
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::No, m_info.memory, instanceValue(), m_currentBlock);

    return { };
}

auto AirIRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    auto temp1 = gPtr();
    auto temp2 = gPtr();

    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfMemory(), B3::pointerWidth()));
    RELEASE_ASSERT(Arg::isValidAddrForm(Memory::offsetOfHandle(), B3::pointerWidth()));
    RELEASE_ASSERT(Arg::isValidAddrForm(MemoryHandle::offsetOfSize(), B3::pointerWidth()));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfMemory()), temp1);
    append(Move, Arg::addr(temp1, Memory::offsetOfHandle()), temp1);
    append(Move, Arg::addr(temp1, MemoryHandle::offsetOfSize()), temp1);
    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    append(Move, Arg::imm(16), temp2);
    addShift(Types::I32, is64Bit() ? Urshift64 : Urshift32, temp1, temp2, result);
    append(Move32, result, result);

    return { };
}

auto AirIRGenerator::addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count) -> PartialResult
{
    ASSERT(dstAddress);
    ASSERT(dstAddress.type().isI32());

    ASSERT(targetValue);
    ASSERT(targetValue.type().isI32());

    ASSERT(count);
    ASSERT(count.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmMemoryFill, result, instanceValue(),
        dstAddress, targetValue, count);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
    });

    return { };
}

auto AirIRGenerator::addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count) -> PartialResult
{
    ASSERT(dstAddress);
    ASSERT(dstAddress.type().isI32());

    ASSERT(srcAddress);
    ASSERT(srcAddress.type().isI32());

    ASSERT(count);
    ASSERT(count.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmMemoryCopy, result, instanceValue(),
        dstAddress, srcAddress, count);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
    });

    return { };
}

auto AirIRGenerator::addMemoryInit(unsigned dataSegmentIndex, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length) -> PartialResult
{
    ASSERT(dstAddress);
    ASSERT(dstAddress.type().isI32());

    ASSERT(srcAddress);
    ASSERT(srcAddress.type().isI32());

    ASSERT(length);
    ASSERT(length.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(
        &operationWasmMemoryInit, result, instanceValue(),
        addConstant(Types::I32, dataSegmentIndex),
        dstAddress, srcAddress, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
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
    TypedTmp local = m_locals[index];
    ASSERT(local);
    emitMove(value, local);
    return { };
}

auto AirIRGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    auto temp = gPtr();
    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfGlobals(), B3::pointerWidth()));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfGlobals()), temp);
    int32_t offset = safeCast<int32_t>(index * sizeof(Register));

    if (global.bindingMode == Wasm::GlobalInformation::BindingMode::Portable) {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        if (Arg::isValidAddrForm(offset, B3::pointerWidth()))
            append(Move, Arg::addr(temp, offset), temp);
        else {
            auto temp2 = gPtr();
            append(Move, Arg::bigImm(offset), temp2);
            append(AddPtr, temp2, temp, temp);
            append(Move, Arg::addr(temp), temp);
        }
        offset = 0;
    } else
        ASSERT(global.bindingMode == Wasm::GlobalInformation::BindingMode::EmbeddedInInstance);

    result = tmpForType(type);
    emitLoad(temp, offset, result);

    return { };
}

auto AirIRGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    auto temp = gPtr();
    RELEASE_ASSERT(Arg::isValidAddrForm(Instance::offsetOfGlobals(), B3::pointerWidth()));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfGlobals()), temp);
    int32_t offset = safeCast<int32_t>(index * sizeof(Register));

    if (global.bindingMode == Wasm::GlobalInformation::BindingMode::Portable) {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        if (Arg::isValidAddrForm(offset, B3::pointerWidth()))
            append(Move, Arg::addr(temp, offset), temp);
        else {
            auto temp2 = gPtr();
            append(Move, Arg::bigImm(offset), temp2);
            append(AddPtr, temp2, temp, temp);
            append(Move, Arg::addr(temp), temp);
        }
        offset = 0;
    } else
        ASSERT(global.bindingMode == Wasm::GlobalInformation::BindingMode::EmbeddedInInstance);


    emitStore(value, temp, offset);

    if (isRefType(type)) {
        switch (global.bindingMode) {
        case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
            emitWriteBarrierForJSWrapper();
            break;
        case Wasm::GlobalInformation::BindingMode::Portable:
            auto cell = gPtr();
            auto vm = gPtr();
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
            doFence->setGenerator([](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                jit.memoryFence();
            });
            emitPatchpoint(doFence, TypedTmp());

            append(Load8, Arg::addr(cell, JSCell::cellStateOffset()), cellState);
            append(Branch32, Arg::relCond(MacroAssembler::Above), cellState, Arg::imm(blackThreshold));
            m_currentBlock->setSuccessors(continuation, doSlowPath);
            m_currentBlock = doSlowPath;

            emitCCall(&operationWasmWriteBarrierSlowPath, TypedTmp(), cell, vm);
            append(Jump);
            m_currentBlock->setSuccessors(continuation);
            m_currentBlock = continuation;
            break;
        }
    }

    return { };
}

inline void AirIRGenerator::emitWriteBarrierForJSWrapper()
{
    auto cell = gPtr();
    auto vm = gPtr();
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
    emitPatchpoint(doFence, TypedTmp());

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
#if CPU(ARM_THUMB2)
    auto memoryBase = gPtr();
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfCachedMemory()), memoryBase);
#else
    ASSERT(m_memoryBaseGPR);
    Tmp memoryBase { m_memoryBaseGPR };
#endif

    auto result = gPtr();
    append(Move32, pointer, result);

    switch (m_mode) {
    case MemoryMode::BoundsChecking: {
        // In bound checking mode, while shared wasm memory partially relies on signal handler too, we need to perform bound checking
        // to ensure that no memory access exceeds the current memory size.
#if CPU(ARM_THUMB2)
        auto boundsCheckingSize = gPtr();
        append(Move, Arg::addr(instanceValue(), Instance::offsetOfCachedBoundsCheckingSize()), boundsCheckingSize);
#else
        ASSERT(m_boundsCheckingSizeGPR);
        Tmp boundsCheckingSize { m_boundsCheckingSizeGPR };
#endif
        ASSERT(sizeOfOperation + offset > offset);

        auto temp = gPtr();
#if USE(JSVALUE32_64)
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
#endif
        append(Move, Arg::bigImm(static_cast<uint64_t>(offset) + sizeOfOperation - 1), temp);
        append(AddPtr, result, temp);
        emitCheck([&] {
            return Inst(BranchPtr, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), temp, boundsCheckingSize);
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

    append(AddPtr, memoryBase, result);
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

    auto getAddr = [&](uint32_t offset) {
        if (Arg::isValidAddrForm(offset, B3::widthForBytes(sizeOfLoadOp(op))))
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
#if USE(JSVALUE64)
        appendEffectful(Load8SignedExtendTo32, addrArg, result);
        append(SignExtend32ToPtr, result, result);
#elif USE(JSVALUE32_64)
        appendEffectful(Load8SignedExtendTo32, addrArg, result.lo());
        append(Rshift32, result.lo(), Arg::imm(31), result.hi());
#endif
        break;
    }

    case LoadOpType::I32Load8U: {
        result = g32();
        appendEffectful(Load8, addrArg, result);
        break;
    }

    case LoadOpType::I64Load8U: {
        result = g64();
#if USE(JSVALUE64)
        appendEffectful(Load8, addrArg, result);
#elif USE(JSVALUE32_64)
        appendEffectful(Load8, addrArg, result.lo());
        append(Move, Arg::imm(0), result.hi());
#endif
        break;
    }

    case LoadOpType::I32Load16S: {
        result = g32();
        appendEffectful(Load16SignedExtendTo32, addrArg, result);
        break;
    }

    case LoadOpType::I64Load16S: {
        result = g64();
#if USE(JSVALUE64)
        appendEffectful(Load16SignedExtendTo32, addrArg, result);
        append(SignExtend32ToPtr, result, result);
#elif USE(JSVALUE32_64)
        appendEffectful(Load16SignedExtendTo32, addrArg, result.lo());
        append(Rshift32, result.lo(), Arg::imm(31), result.hi());
#endif
        break;
    }

    case LoadOpType::I32Load16U: {
        result = g32();
        appendEffectful(Load16, addrArg, result);
        break;
    }

    case LoadOpType::I64Load16U: {
        result = g64();
#if USE(JSVALUE64)
        appendEffectful(Load16, addrArg, result);
#elif USE(JSVALUE32_64)
        appendEffectful(Load16, addrArg, result.lo());
        append(Move, Arg::imm(0), result.hi());
#endif
        break;
    }

    case LoadOpType::I32Load:
        result = g32();
        appendEffectful(Move32, addrArg, result);
        break;

    case LoadOpType::I64Load32U: {
        result = g64();
#if USE(JSVALUE64)
        appendEffectful(Move32, addrArg, result);
#elif USE(JSVALUE32_64)
        appendEffectful(Move, addrArg, result.lo());
        append(Move, Arg::imm(0), result.hi());
#endif
        break;
    }

    case LoadOpType::I64Load32S: {
        result = g64();
#if USE(JSVALUE64)
        appendEffectful(Move32, addrArg, result);
        append(SignExtend32ToPtr, result, result);
#elif USE(JSVALUE32_64)
        appendEffectful(Move32, addrArg, result.lo());
        append(Rshift32, result.lo(), Arg::imm(31), result.hi());
#endif
        break;
    }

    case LoadOpType::I64Load: {
        result = g64();
#if USE(JSVALUE64)
        appendEffectful(Move, addrArg, result);
#elif USE(JSVALUE32_64)
        // Might be unaligned so can't use LoadPair32
        appendEffectful(Move, addrArg, result.lo());
        appendEffectful(Move, getAddr(offset + 4), result.hi());
#endif
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
        emitPatchpoint(patch, TypedTmp());

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

    auto getAddr = [&](uint32_t offset) {
        if (Arg::isValidAddrForm(offset, B3::widthForBytes(sizeOfStoreOp(op))))
            return Arg::addr(pointer, offset);
        immTmp = gPtr();
        newPtr = gPtr();
        append(Move, Arg::bigImm(offset), immTmp);
        append(AddPtr, immTmp, pointer, newPtr);
        return Arg::addr(newPtr);
    };

    Arg addrArg = getAddr(offset);

    switch (op) {
    case StoreOpType::I64Store8:
#if USE(JSVALUE32_64)
        append(Store8, value.lo(), addrArg);
        return;
#endif
    case StoreOpType::I32Store8:
        append(Store8, value, addrArg);
        return;

    case StoreOpType::I64Store16:
#if USE(JSVALUE32_64)
        append(Store16, value.lo(), addrArg);
        return;
#endif
    case StoreOpType::I32Store16:
        append(Store16, value, addrArg);
        return;

    case StoreOpType::I64Store32:
#if USE(JSVALUE32_64)
        append(Move, value.lo(), addrArg);
        return;
#endif
    case StoreOpType::I32Store:
        append(Move32, value, addrArg);
        return;

    case StoreOpType::I64Store:
#if USE(JSVALUE64)
        append(Move, value, addrArg);
#elif USE(JSVALUE32_64)
        // Might be unaligned so can't use StorePair32
        append(Move, value.lo(), addrArg);
        append(Move, value.hi(), getAddr(offset + 4));
#endif
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
        emitPatchpoint(throwException, TypedTmp());
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
        TypedTmp newPtr = gPtr();
        append(LeaPtr, Arg::addr(pointer, offset), newPtr);
        return newPtr;
    }
    TypedTmp newPtr = gPtr();
    append(Move, Arg::bigImm(offset), newPtr);
    append(AddPtr, pointer, newPtr);
    return newPtr;
}

void AirIRGenerator::sanitizeAtomicResult(ExtAtomicOpType op, TypedTmp source, TypedTmp dest)
{
    ASSERT(source.type() == dest.type());
    switch (source.type().kind) {
    case TypeKind::I64: {
        switch (accessWidth(op)) {
        case B3::Width8:
#if USE(JSVALUE64)
            append(ZeroExtend8To32, source, dest);
#elif USE(JSVALUE32_64)
            append(ZeroExtend8To32, source.lo(), dest.lo());
            append(Move, Arg::imm(0), dest.hi());
#endif
            return;
        case B3::Width16:
#if USE(JSVALUE64)
            append(ZeroExtend16To32, source, dest);
#elif USE(JSVALUE32_64)
            append(ZeroExtend16To32, source.lo(), dest.lo());
            append(Move, Arg::imm(0), dest.hi());
#endif
            return;
        case B3::Width32:
#if USE(JSVALUE64)
            append(Move32, source, dest);
#elif USE(JSVALUE32_64)
            append(Move, source.lo(), dest.lo());
            append(Move, Arg::imm(0), dest.hi());
#endif
            return;
        case B3::Width64:
            emitMove(source, dest);
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
            emitMove(source, dest);
            return;
        }
        return;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void AirIRGenerator::sanitizeAtomicResult(ExtAtomicOpType op, TypedTmp result)
{
    sanitizeAtomicResult(op, result, result);
}

template<typename InputType>
TypedTmp AirIRGenerator::appendGeneralAtomic(ExtAtomicOpType op, B3::Air::Opcode opcode, B3::Commutativity commutativity, InputType input, Arg address, TypedTmp oldValue)
{
    static_assert(std::is_same_v<InputType, TypedTmp> || std::is_same_v<InputType, Arg>);

    B3::Width accessWidth = Wasm::accessWidth(op);

    auto newTmp = [&]() {
        if (accessWidth == B3::Width64)
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

    if (isARM())
        appendEffectful(MemoryFence);
    append(B3::Air::Jump);
    beginBlock->setSuccessors(reloopBlock);
    m_currentBlock = reloopBlock;

    if (isX86()) {
        B3::Air::Opcode prepareOpcode;
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
        appendEffectful(prepareOpcode, address, oldValue);
    } else if constexpr (isARM64()) {
        appendEffectful(OPCODE_FOR_WIDTH(LoadLinkAcq, accessWidth), address, oldValue);
    } else {
        RELEASE_ASSERT(isARM());
        if (accessWidth == B3::Width64)
            appendEffectful(LoadLinkPair32, address, oldValue.lo(), oldValue.hi());
        else if (oldValue.isGPPair())
            appendEffectful(OPCODE_FOR_WIDTH(LoadLink, accessWidth), address, oldValue.lo());
        else
            appendEffectful(OPCODE_FOR_WIDTH(LoadLink, accessWidth), address, oldValue);

    }

    if (opcode != B3::Air::Nop) {
        // FIXME: If we ever have to write this again, we need to find a way to share the code with
        // appendBinOp.
        // https://bugs.webkit.org/show_bug.cgi?id=169249
        TypedTmp inTmp = tmp();
        if (is32Bit() && accessWidth == B3::Width64) {
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

    if (isX86()) {
#if CPU(X86) || CPU(X86_64)
        Tmp eax(X86Registers::eax);
        B3::Air::Opcode casOpcode = OPCODE_FOR_WIDTH(BranchAtomicStrongCAS, accessWidth);
        append(Move, oldValue, eax);
        appendEffectful(casOpcode, Arg::statusCond(MacroAssembler::Success), eax, newValue, address);
#endif
    } else if constexpr (isARM64()) {
        TypedTmp boolResult = newTmp();
        appendEffectful(OPCODE_FOR_WIDTH(StoreCondRel, accessWidth), newValue, address, boolResult);
        append(BranchTest32, Arg::resCond(MacroAssembler::Zero), boolResult, boolResult);
    } else {
        RELEASE_ASSERT(isARM());
        TypedTmp boolResult = g32();
        if (accessWidth == B3::Width64)
            appendEffectful(StoreCondPair32, newValue.lo(), newValue.hi(), address, boolResult);
        else if (newValue.isGPPair())
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), newValue.lo(), address, boolResult);
        else
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), newValue, address, boolResult);
        append(BranchTest32, Arg::resCond(MacroAssembler::Zero), boolResult, boolResult);
    }
    reloopBlock->setSuccessors(doneBlock, reloopBlock);
    m_currentBlock = doneBlock;
    if (isARM())
        appendEffectful(MemoryFence);
    return oldValue;
}

TypedTmp AirIRGenerator::appendStrongCAS(ExtAtomicOpType op, TypedTmp expected, TypedTmp newValue, Arg address, TypedTmp oldValue)
{
    B3::Width accessWidth = Wasm::accessWidth(op);

    if (isX86()) {
#if CPU(X86) || CPU(X86_64)
        Tmp eax(X86Registers::eax);
        append(Move, expected, eax);
        appendEffectful(OPCODE_FOR_WIDTH(AtomicStrongCAS, accessWidth), eax, newValue, address);
        append(Move, eax, oldValue);
#endif
        return oldValue;
    }

    if (isARM64E()) {
        append(Move, expected, oldValue);
        appendEffectful(OPCODE_FOR_WIDTH(AtomicStrongCAS, accessWidth), oldValue, newValue, address);
        return oldValue;
    }

    RELEASE_ASSERT(isARM64() || isARM());
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

    if (isARM())
        appendEffectful(MemoryFence);
    append(B3::Air::Jump);
    beginBlock->setSuccessors(reloopBlock);

    m_currentBlock = reloopBlock;
    if constexpr (isARM64()) {
        appendEffectful(OPCODE_FOR_WIDTH(LoadLinkAcq, accessWidth), address, oldValue);
        append(OPCODE_FOR_CANONICAL_WIDTH(Branch, accessWidth), Arg::relCond(MacroAssembler::NotEqual), oldValue, expected);
    }   else if (accessWidth == B3::Width64) {
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
        if constexpr (isARM64())
            appendEffectful(OPCODE_FOR_WIDTH(StoreCondRel, accessWidth), newValue, address, tmp);
        else if (accessWidth == B3::Width64)
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
        if constexpr (isARM64())
            appendEffectful(OPCODE_FOR_WIDTH(StoreCondRel, accessWidth), oldValue, address, tmp);
        else if (accessWidth == B3::Width64)
            appendEffectful(StoreCondPair32, oldValue.lo(), oldValue.hi(), address, tmp);
        else if (oldValue.isGPPair())
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), oldValue.lo(), address, tmp);
        else
            appendEffectful(OPCODE_FOR_WIDTH(StoreCond, accessWidth), oldValue, address, tmp);
        append(BranchTest32, Arg::resCond(MacroAssembler::Zero), tmp, tmp);
    }
    strongFailBlock->setSuccessors(B3::Air::FrequentedBlock(doneBlock), reloopBlock);

    m_currentBlock = doneBlock;
    if (isARM())
        appendEffectful(MemoryFence);
    return oldValue;
}

inline TypedTmp AirIRGenerator::emitAtomicLoadOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, uint32_t uoffset)
{
    TypedTmp newPtr = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);

    if (accessWidth(op) != B3::Width8) {
        emitCheck([&] {
            return Inst(BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() || is32Bit() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
            sanitizeAtomicResult(op, result);
            return result;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            append(Move, Arg::imm(0), result);
            appendEffectful(opcode.value(), result, addrArg);
            sanitizeAtomicResult(op, result);
            return result;
        }
    }

    appendGeneralAtomic(op, nonAtomicOpcode, B3::Commutative, Arg::imm(0), addrArg, result);
    sanitizeAtomicResult(op, result);
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
        emitPatchpoint(patch, TypedTmp());

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
            return Inst(BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() || is32Bit() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
        emitPatchpoint(throwException, TypedTmp());
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
            return Inst(BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() || is32Bit() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
            sanitizeAtomicResult(op, result);
            return result;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            append(Move, value, result);
            appendEffectful(opcode.value(), result, addrArg);
            sanitizeAtomicResult(op, result);
            return result;
        }
    }

    appendGeneralAtomic(op, nonAtomicOpcode, commutativity, value, addrArg, result);
    sanitizeAtomicResult(op, result);
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
        emitPatchpoint(patch, TypedTmp());

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

TypedTmp AirIRGenerator::emitAtomicCompareExchange(ExtAtomicOpType op, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t uoffset)
{
    TypedTmp newPtr = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);
    B3::Width valueWidth = widthForType(toB3Type(value.type()));
    B3::Width accessWidth = Wasm::accessWidth(op);

    if (accessWidth != B3::Width8) {
        emitCheck([&] {
            return Inst(BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() || is32Bit()  ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    TypedTmp result = expected.type().isI64() ? g64() : g32();

    if (valueWidth == accessWidth) {
        appendStrongCAS(op, expected, value, addrArg, result);
        sanitizeAtomicResult(op, result);
        return result;
    }

    BasicBlock* failureCase = m_code.addBlock();
    BasicBlock* successCase = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    TypedTmp truncatedExpected = expected.type().isI64() ? g64() : g32();
    sanitizeAtomicResult(op, expected, truncatedExpected);

    if (is32Bit() && valueWidth == B3::Width64) {
        append(Branch32, Arg::relCond(MacroAssembler::NotEqual), expected.lo(), truncatedExpected.lo());
        auto* checkHiBlock = m_code.addBlock();
        m_currentBlock->setSuccessors(B3::Air::FrequentedBlock(failureCase), checkHiBlock);
        m_currentBlock = checkHiBlock;
        append(Branch32, Arg::relCond(MacroAssembler::NotEqual), expected.hi(), truncatedExpected.hi());
    } else
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
    sanitizeAtomicResult(op, result);
    return result;
}

auto AirIRGenerator::atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());
    ASSERT(expected.type() == valueType);
    ASSERT(value.type() == valueType);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* patch = addPatchpoint(B3::Void);
        patch->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(patch, TypedTmp());

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
        result = emitAtomicCompareExchange(op, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), expected, value, offset);

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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
        materializeConstant(minCase, minResult, result);
        append(minCase, Jump);
        minCase->setSuccessors(continuation);
    } else {
        BasicBlock* minMaterializeCase = m_code.addBlock();
        BasicBlock* nanCase = m_code.addBlock();
        append(minCase, branchOp, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg, arg);
        minCase->setSuccessors(minMaterializeCase, nanCase);

        materializeConstant(minMaterializeCase, minResult, result);
        append(minMaterializeCase, Jump);
        minMaterializeCase->setSuccessors(continuation);

        materializeConstant(nanCase, 0, result);
        append(nanCase, Jump);
        nanCase->setSuccessors(continuation);
    }

    materializeConstant(maxCase, maxResult, result);
    append(maxCase, Jump);
    maxCase->setSuccessors(continuation);

    if (is64Bit() || result.type().isI32()) {
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
#if USE(JSVALUE64)
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
#endif
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        });

        emitPatchpoint(inBoundsCase, patchpoint, Vector<TypedTmp, 8> { result }, WTFMove(args));
    } else {
        switch (op) {
#if USE(JSVALUE32_64)
        case Ext1OpType::I64TruncSatF32S:
            emitCCall(inBoundsCase, Math::i64_trunc_s_f32, result, arg);
            break;
        case Ext1OpType::I64TruncSatF32U:
            emitCCall(inBoundsCase, Math::i64_trunc_u_f32, result, arg);
            break;
        case Ext1OpType::I64TruncSatF64S:
            emitCCall(inBoundsCase, Math::i64_trunc_s_f64, result, arg);
            break;
        case Ext1OpType::I64TruncSatF64U:
            emitCCall(inBoundsCase, Math::i64_trunc_u_f64, result, arg);
            break;
#endif
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    append(inBoundsCase, Jump);
    inBoundsCase->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

auto AirIRGenerator::addRttCanon(uint32_t typeIndex, ExpressionType& result) -> PartialResult
{
    result = gRtt();
    emitCCall(&operationWasmRttCanon, result, instanceValue(), addConstant(Types::I32, typeIndex));

    return { };
}

auto AirIRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    ASSERT(nonZero.type() == zero.type());
    result = tmpForType(nonZero.type());

    BasicBlock* isZero = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    emitMove(nonZero, result);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), condition, condition);
    m_currentBlock->setSuccessors(isZero, continuation);
    m_currentBlock = isZero;

    emitMove(zero, result);
    append(Jump);
    isZero->setSuccessors(continuation);
    m_currentBlock = continuation;

    return { };
}

void AirIRGenerator::emitEntryTierUpCheck()
{
    if (!m_tierUp)
        return;

    auto countdownPtr = gPtr();
    append(Move, Arg::bigImm(bitwise_cast<uintptr_t>(&m_tierUp->m_counter)), countdownPtr);

    auto* patch = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    patch->effects = effects;
    patch->clobber(RegisterSet::macroScratchRegisters());

    patch->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);

        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::functionEntryIncrement()), CCallHelpers::Address(params[0].gpr()));
        CCallHelpers::Label tierUpResume = jit.label();

        params.addLatePath([=, this] (CCallHelpers& jit) {
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

    emitPatchpoint(patch, TypedTmp(), countdownPtr);
}

void AirIRGenerator::emitLoopTierUpCheck(uint32_t loopIndex, const Vector<TypedTmp>& liveValues)
{
    uint32_t outerLoopIndex = this->outerLoopIndex();
    m_outerLoops.append(loopIndex);

    if (!m_tierUp)
        return;

    ASSERT(m_tierUp->osrEntryTriggers().size() == loopIndex);
    m_tierUp->osrEntryTriggers().append(TierUpCount::TriggerReason::DontTrigger);
    m_tierUp->outerLoops().append(outerLoopIndex);

    auto countdownPtr = gPtr();
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
    for (const TypedTmp& tmp : liveValues)
        patchArgs.append(ConstrainedTmp(tmp.tmp(), B3::ValueRep::ColdAny));

    TierUpCount::TriggerReason* forceEntryTrigger = &(m_tierUp->osrEntryTriggers().last());
    static_assert(!static_cast<uint8_t>(TierUpCount::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
    static_assert(sizeof(TierUpCount::TriggerReason) == 1, "branchTest8 assumes this size");
    patch->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        CCallHelpers::Jump forceOSREntry = jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::AbsoluteAddress(forceEntryTrigger));
        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::loopIncrement()), CCallHelpers::Address(params[0].gpr()));
        MacroAssembler::Label tierUpResume = jit.label();

        // First argument is the countdown location.
        ASSERT(params.value()->numChildren() >= 1);
        StackMap values(params.value()->numChildren() - 1);
        for (unsigned i = 1; i < params.value()->numChildren(); ++i)
            values[i - 1] = OSREntryValue(params[i], params.value()->child(i)->type());

        OSREntryData& osrEntryData = m_tierUp->addOSREntryData(m_functionIndex, loopIndex, WTFMove(values));
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
    RELEASE_ASSERT(loopIndex == m_loopEntryVariableData.size());

    BasicBlock* body = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    splitStack(signature, enclosingStack, newStack);

    Vector<TypedTmp> liveValues;
    forEachLiveValue([&] (TypedTmp tmp) {
        liveValues.append(tmp);
    });
    for (auto variable : enclosingStack)
        liveValues.append(variable);
    for (auto variable : newStack)
        liveValues.append(variable);

    ResultList results;
    results.reserveInitialCapacity(newStack.size());
    for (auto item : newStack)
        results.uncheckedAppend(item);
    block = ControlData(origin(), signature, WTFMove(results), BlockType::Loop, continuation, body);

    append(Jump);
    m_currentBlock->setSuccessors(body);

    m_currentBlock = body;
    emitLoopTierUpCheck(loopIndex, liveValues);

    m_loopEntryVariableData.append(std::pair<BasicBlock*, Vector<TypedTmp>>(body, WTFMove(liveValues)));

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
    B3::FrequencyClass takenFrequency = B3::FrequencyClass::Normal;
    B3::FrequencyClass notTakenFrequency= B3::FrequencyClass::Normal;

    if (Options::useWebAssemblyBranchHints()) {
        BranchHint hint = m_info.getBranchHint(m_functionIndex, m_parser->currentOpcodeStartingOffset());

        switch (hint) {
        case BranchHint::Unlikely:
            takenFrequency = B3::FrequencyClass::Rare;
            break;
        case BranchHint::Likely:
            notTakenFrequency = B3::FrequencyClass::Rare;
            break;
        case BranchHint::Invalid:
            break;
        }
    }

    // Wasm bools are i32.
    append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), condition, condition);
    m_currentBlock->setSuccessors(FrequentedBlock(taken, takenFrequency), FrequentedBlock(notTaken, notTakenFrequency));

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

auto AirIRGenerator::addTry(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    ++m_tryCatchDepth;

    BasicBlock* continuation = m_code.addBlock();
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(origin(), signature, tmpsForSignature(signature), BlockType::Try, continuation, ++m_callSiteIndex, m_tryCatchDepth);
    return { };
}

auto AirIRGenerator::addCatch(unsigned exceptionIndex, const TypeDefinition& signature, Stack& currentStack, ControlType& data, ResultList& results) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);
    return addCatchToUnreachable(exceptionIndex, signature, data, results);
}

auto AirIRGenerator::addCatchAll(Stack& currentStack, ControlType& data) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);
    return addCatchAllToUnreachable(data);
}

auto AirIRGenerator::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& signature, ControlType& data, ResultList& results) -> PartialResult
{
    Tmp buffer = emitCatchImpl(CatchKind::Catch, data, exceptionIndex);
    for (unsigned i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        Type type = signature.as<FunctionSignature>()->argumentType(i);
        TypedTmp tmp = tmpForType(type);
        emitLoad(buffer, i * sizeof(uint64_t), tmp);
        results.append(tmp);
    }
    return { };
}

auto AirIRGenerator::addCatchAllToUnreachable(ControlType& data) -> PartialResult
{
    emitCatchImpl(CatchKind::CatchAll, data);
    return { };
}

Tmp AirIRGenerator::emitCatchImpl(CatchKind kind, ControlType& data, unsigned exceptionIndex)
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

    Vector<B3::Type> resultTypes {
        B3::pointerType(),
#if USE(JSVALUE64)
        B3::Int64
#elif USE(JSVALUE32_64)
        B3::Int32, B3::Int32
#endif
    };
    B3::PatchpointValue* patch = addPatchpoint(m_proc.addTuple(WTFMove(resultTypes)));
    patch->effects.exitsSideways = true;
    patch->clobber(RegisterSet::macroScratchRegisters());
    RegisterSet clobberLate = RegisterSet::volatileRegistersForCCall();
    clobberLate.add(GPRInfo::argumentGPR0);
    clobberLate.add(GPRInfo::argumentGPR1);
    patch->clobberLate(clobberLate);
    patch->resultConstraints.append(B3::ValueRep::reg(GPRInfo::returnValueGPR));
    patch->resultConstraints.append(B3::ValueRep::SomeRegister);
#if USE(JSVALUE32_64)
    patch->resultConstraints.append(B3::ValueRep::SomeRegister); // result Tag
#endif
    patch->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        // Returning one EncodedJSValue on the stack
        constexpr int32_t resultSpace = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(static_cast<int32_t>(sizeof(EncodedJSValue)));
        jit.subPtr(CCallHelpers::TrustedImm32(resultSpace), MacroAssembler::stackPointerRegister);
        jit.move(params[is64Bit() ? 2 : 3].gpr(), GPRInfo::argumentGPR0);
        jit.move(MacroAssembler::stackPointerRegister, GPRInfo::argumentGPR1);
        CCallHelpers::Call call = jit.call(OperationPtrTag);
        jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationWasmRetrieveAndClearExceptionIfCatchable));
        });
#if USE(JSVALUE32_64)
        jit.load32(CCallHelpers::Address(MacroAssembler::stackPointerRegister, TagOffset), params[2].gpr());
#endif
        jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister, PayloadOffset), params[1].gpr());
        jit.addPtr(CCallHelpers::TrustedImm32(resultSpace), MacroAssembler::stackPointerRegister);
    });
    TypedTmp buffer = TypedTmp(Tmp(GPRInfo::returnValueGPR), is64Bit() ? Types::I64 : Types::I32);
#if USE(JSVALUE64)
    emitPatchpoint(m_currentBlock, patch, Vector<TypedTmp, 8>::from(buffer, data.exception()), Vector<ConstrainedTmp, 1>::from(instanceValue()));
#elif USE(JSVALUE32_64)
    TypedTmp exceptionLo = TypedTmp(data.exception().lo(), Types::I32);
    TypedTmp exceptionHi = TypedTmp(data.exception().hi(), Types::I32);
    emitPatchpoint(m_currentBlock, patch, Vector<TypedTmp, 8>::from(buffer, exceptionLo, exceptionHi), Vector<ConstrainedTmp, 1>::from(instanceValue()));
#endif
    return buffer;
}

auto AirIRGenerator::addDelegate(ControlType& target, ControlType& data) -> PartialResult
{
    return addDelegateToUnreachable(target, data);
}

auto AirIRGenerator::addDelegateToUnreachable(ControlType& target, ControlType& data) -> PartialResult
{
    unsigned targetDepth = 0;
    if (ControlType::isTry(target))
        targetDepth = target.tryDepth();

    m_exceptionHandlers.append({ HandlerType::Delegate, data.tryStart(), ++m_callSiteIndex, 0, m_tryCatchDepth, targetDepth });
    return { };
}

auto AirIRGenerator::addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&) -> PartialResult
{
    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    patch->effects.terminal = true;
    RegisterSet clobber = RegisterSet::volatileRegistersForCCall();
    clobber.add(GPRInfo::nonPreservedNonArgumentGPR0);
    patch->clobber(clobber);

    Vector<ConstrainedTmp, 8> patchArgs;
    patchArgs.append(ConstrainedTmp(instanceValue(), B3::ValueRep::reg(GPRInfo::argumentGPR0)));
    patchArgs.append(ConstrainedTmp(Tmp(GPRInfo::callFrameRegister), B3::ValueRep::reg(GPRInfo::argumentGPR1)));
    for (unsigned i = 0; i < args.size(); ++i) {
        if (args[i].isGPPair()) {
            patchArgs.append(ConstrainedTmp(args[i].lo(), B3::ValueRep::stackArgument(i * sizeof(EncodedJSValue) + PayloadOffset)));
            patchArgs.append(ConstrainedTmp(args[i].hi(), B3::ValueRep::stackArgument(i * sizeof(EncodedJSValue) + TagOffset)));
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

    emitPatchpoint(m_currentBlock, patch, TypedTmp(), WTFMove(patchArgs));

    return { };
}

auto AirIRGenerator::addRethrow(unsigned, ControlType& data) -> PartialResult
{
    B3::PatchpointValue* patch = addPatchpoint(B3::Void);
    RegisterSet clobber = RegisterSet::volatileRegistersForCCall();
    clobber.add(GPRInfo::nonPreservedNonArgumentGPR0);
    patch->clobber(clobber);
    patch->effects.terminal = true;

    Vector<ConstrainedTmp, 3> patchArgs;
    patchArgs.append(ConstrainedTmp(instanceValue(), B3::ValueRep::reg(GPRInfo::argumentGPR0)));
    patchArgs.append(ConstrainedTmp(Tmp(GPRInfo::callFrameRegister), B3::ValueRep::reg(GPRInfo::argumentGPR1)));
    if (data.exception().isGPPair()) {
        RELEASE_ASSERT(is32Bit());
        patchArgs.append(ConstrainedTmp(data.exception().lo(), B3::ValueRep::reg(GPRInfo::argumentGPR2)));
        patchArgs.append(ConstrainedTmp(data.exception().hi(), B3::ValueRep::reg(GPRInfo::argumentGPR3)));
    } else {
        RELEASE_ASSERT(is64Bit());
        patchArgs.append(ConstrainedTmp(data.exception().tmp(), B3::ValueRep::reg(GPRInfo::argumentGPR2)));
    }

    PatchpointExceptionHandle handle = preparePatchpointForExceptions(patch, patchArgs);
    patch->setGenerator([this, handle] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        handle.generate(jit, params, this);
        emitRethrowImpl(jit);
    });

    emitPatchpoint(m_currentBlock, patch, TypedTmp(), WTFMove(patchArgs));

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
            emitStore(tmp, Tmp(GPRInfo::callFrameRegister), rep.offsetFromFP());
            continue;
        }

        ASSERT(rep.isReg());
#if USE(JSVALUE64)
        if (data.signature()->as<FunctionSignature>()->returnType(i).isI32())
            append(Move32, tmp, tmp);
        returnConstraints.append(ConstrainedTmp(tmp, rep));
#elif USE(JSVALUE32_64)
        if (tmp.isGPPair()) {
            JSValueRegs jsr = wasmCallInfo.results[i].jsr();
            returnConstraints.append(ConstrainedTmp(tmp.lo(), B3::ValueRep { jsr.payloadGPR() }));
            returnConstraints.append(ConstrainedTmp(tmp.hi(), B3::ValueRep { jsr.tagGPR() }));
        } else
            returnConstraints.append(ConstrainedTmp(tmp, rep));
#endif
    }

    emitPatchpoint(m_currentBlock, patch, ResultList { }, WTFMove(returnConstraints));
    return { };
}

// NOTE: All branches in Wasm are on 32-bit ints

auto AirIRGenerator::addBranch(ControlData& data, ExpressionType condition, const Stack& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data.results);

    BasicBlock* target = data.targetBlockForBranch();
    B3::FrequencyClass targetFrequency = B3::FrequencyClass::Normal;
    B3::FrequencyClass continuationFrequency = B3::FrequencyClass::Normal;

    if (Options::useWebAssemblyBranchHints()) {
        BranchHint hint = m_info.getBranchHint(m_functionIndex, m_parser->currentOpcodeStartingOffset());

        switch (hint) {
        case BranchHint::Unlikely:
            targetFrequency = B3::FrequencyClass::Rare;
            break;
        case BranchHint::Likely:
            continuationFrequency = B3::FrequencyClass::Rare;
            break;
        case BranchHint::Invalid:
            break;
        }
    }

    if (condition) {
        BasicBlock* continuation = m_code.addBlock();
        append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), condition, condition);
        m_currentBlock->setSuccessors(FrequentedBlock(target, targetFrequency), FrequentedBlock(continuation, continuationFrequency));
        m_currentBlock = continuation;
    } else {
        append(Jump);
        m_currentBlock->setSuccessors(FrequentedBlock(target, targetFrequency));
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
    } else if (data.blockType() == BlockType::Try || data.blockType() == BlockType::Catch)
        --m_tryCatchDepth;

    if (data.blockType() == BlockType::Loop) {
        m_outerLoops.removeLast();
        for (unsigned i = 0; i < data.signature()->as<FunctionSignature>()->returnCount(); ++i) {
            if (i < expressionStack.size())
                entry.enclosedExpressionStack.append(expressionStack[i]);
            else {
                Type type = data.signature()->as<FunctionSignature>()->returnType(i);
                entry.enclosedExpressionStack.constructAndAppend(type, addBottom(m_currentBlock, type));
            }
        }
    } else {
        for (unsigned i = 0; i < data.signature()->as<FunctionSignature>()->returnCount(); ++i)
            entry.enclosedExpressionStack.constructAndAppend(data.signature()->as<FunctionSignature>()->returnType(i), data.results[i]);
    }

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.blockType() == BlockType::TopLevel)
        return addReturn(data, entry.enclosedExpressionStack);

    return { };
}

std::pair<B3::PatchpointValue*, PatchpointExceptionHandle> AirIRGenerator::emitCallPatchpoint(BasicBlock* block, const TypeDefinition& signature, const ResultList& results, const Vector<TypedTmp>& args, Vector<ConstrainedTmp> patchArgs)
{
    auto* patchpoint = addPatchpoint(toB3ResultType(&signature));
    patchpoint->effects.writesPinned = true;
    patchpoint->effects.readsPinned = true;
    patchpoint->clobberEarly(RegisterSet::macroScratchRegisters());
    patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());

    CallInformation locations = wasmCallingConvention().callInformationFor(signature);
    m_code.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), locations.headerAndArgumentStackSizeInBytes));

    ASSERT(locations.params.size() == args.size());
    ASSERT(locations.results.size() == results.size());

    // On 32-bit platforms, 64-bit integer types are passed as two 32-bit values
    size_t offset = patchArgs.size();
    size_t passedArgs = args.size();
#if USE(JSVALUE32_64)
    for (unsigned i = 0; i < args.size(); ++i) {
        if (args[i].isGPPair())
            ++passedArgs;
    }
#endif
    Checked<size_t> newSize = checkedSum<size_t>(patchArgs.size(), passedArgs);
    RELEASE_ASSERT(!newSize.hasOverflowed());
    patchArgs.grow(newSize);
    unsigned j = 0;
    for (unsigned i = 0; i < args.size(); ++i) {
        const TypedTmp& arg = args[i];
        ValueLocation& loc = locations.params[i];
#if USE(JSVALUE32_64)
        if (arg.isGPPair()) {
            B3::ValueRep valueRepLo = loc.isGPR() ? B3::ValueRep(loc.jsr().payloadGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP());
            B3::ValueRep valueRepHi = loc.isGPR() ? B3::ValueRep(loc.jsr().tagGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP() + 4);
            patchArgs[offset + j++] = ConstrainedTmp(arg.lo(), valueRepLo);
            patchArgs[offset + j++] = ConstrainedTmp(arg.hi(), valueRepHi);
            continue;
        }
#endif
        patchArgs[offset + j++] = ConstrainedTmp(arg, loc);
    }
    ASSERT(j == passedArgs);

    // On 32-bit platforms, 64-bit integer types are returned as two 32-bit values
    ResultList patchResults;
    if (patchpoint->type() != B3::Void) {
        Vector<B3::ValueRep, 1> resultConstraints;
        for (unsigned i = 0; i < results.size(); ++i) {
            const TypedTmp& result = results[i];
            ValueLocation& loc = locations.results[i];
#if USE(JSVALUE32_64)
            if (result.isGPPair()) {
                patchResults.append(TypedTmp { result.lo(), Types::I32 });
                patchResults.append(TypedTmp { result.hi(), Types::I32 });
                resultConstraints.append(loc.isGPR() ? B3::ValueRep(loc.jsr().payloadGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP()));
                resultConstraints.append(loc.isGPR() ? B3::ValueRep(loc.jsr().tagGPR()) : B3::ValueRep::stackArgument(loc.offsetFromSP() + 4));
                continue;
            }
#endif
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

auto AirIRGenerator::addCall(uint32_t functionIndex, const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    m_makesCalls = true;

    for (unsigned i = 0; i < signature.as<FunctionSignature>()->returnCount(); ++i)
        results.append(tmpForType(signature.as<FunctionSignature>()->returnType(i)));

    Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
        m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

        auto currentInstance = gPtr();
        append(Move, instanceValue(), currentInstance);

        auto targetInstance = gPtr();

        // FIXME: We should have better isel here.
        // https://bugs.webkit.org/show_bug.cgi?id=193999
        append(Move, Arg::bigImm(Instance::offsetOfTargetInstance(functionIndex)), targetInstance);
        append(AddPtr, instanceValue(), targetInstance);
        append(Move, Arg::addr(targetInstance), targetInstance);

        BasicBlock* isWasmBlock = m_code.addBlock();
        BasicBlock* isEmbedderBlock = m_code.addBlock();
        BasicBlock* continuation = m_code.addBlock();

        append(BranchTestPtr, Arg::resCond(MacroAssembler::NonZero), targetInstance, targetInstance);
        m_currentBlock->setSuccessors(isWasmBlock, isEmbedderBlock);

        {
            auto pair = emitCallPatchpoint(isWasmBlock, signature, results, args);
            auto* patchpoint = pair.first;
            auto exceptionHandle = pair.second;
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

            patchpoint->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                exceptionHandle.generate(jit, params, this);
                CCallHelpers::Call call = jit.threadSafePatchableNearCall();
                jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                    unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
                });
            });

            append(isWasmBlock, Jump);
            isWasmBlock->setSuccessors(continuation);
        }

        {
            auto jumpDestination = gPtr();
            append(isEmbedderBlock, Move, Arg::bigImm(Instance::offsetOfWasmToEmbedderStub(functionIndex)), jumpDestination);
            append(isEmbedderBlock, AddPtr, instanceValue(), jumpDestination);
            append(isEmbedderBlock, Move, Arg::addr(jumpDestination), jumpDestination);

            Vector<ConstrainedTmp> jumpArgs;
            jumpArgs.append({ jumpDestination, B3::ValueRep::SomeRegister });
            auto pair = emitCallPatchpoint(isEmbedderBlock, signature, results, args, WTFMove(jumpArgs));
            auto* patchpoint = pair.first;
            auto exceptionHandle = pair.second;

            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
            patchpoint->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                exceptionHandle.generate(jit, params, this);
                jit.call(params[params.proc().resultCount(params.value()->type())].gpr(), WasmEntryPtrTag);
            });

            append(isEmbedderBlock, Jump);
            isEmbedderBlock->setSuccessors(continuation);
        }

        m_currentBlock = continuation;
        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, currentInstance, continuation);
    } else {
        auto pair = emitCallPatchpoint(m_currentBlock, signature, results, args);
        auto* patchpoint = pair.first;
        auto exceptionHandle = pair.second;
        // We need to clobber the size register since the LLInt always bounds checks
        if (useSignalingMemory() || m_info.memory.isShared())
            patchpoint->clobberLate(RegisterSet { PinnedRegisterInfo::get().boundsCheckingSizeRegister });
        patchpoint->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            exceptionHandle.generate(jit, params, this);
            CCallHelpers::Call call = jit.threadSafePatchableNearCall();
            jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
            });
        });
    }

    return { };
}

auto AirIRGenerator::addCallIndirect(unsigned tableIndex, const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    m_makesCalls = true;
    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    ExpressionType callableFunctionBuffer = gPtr();
    ExpressionType instancesBuffer = gPtr();
    ExpressionType callableFunctionBufferLength = gPtr();
    {
        RELEASE_ASSERT(Arg::isValidAddrForm(FuncRefTable::offsetOfFunctions(), B3::pointerWidth()));
        RELEASE_ASSERT(Arg::isValidAddrForm(FuncRefTable::offsetOfInstances(), B3::pointerWidth()));
        RELEASE_ASSERT(Arg::isValidAddrForm(FuncRefTable::offsetOfLength(), B3::pointerWidth()));

        emitLoad(instanceValue().tmp(), Instance::offsetOfTablePtr(m_numImportFunctions, tableIndex), callableFunctionBufferLength);
        append(Move, Arg::addr(callableFunctionBufferLength, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
        append(Move, Arg::addr(callableFunctionBufferLength, FuncRefTable::offsetOfInstances()), instancesBuffer);
        append(Move32, Arg::addr(callableFunctionBufferLength, Table::offsetOfLength()), callableFunctionBufferLength);
    }

    append(Move32, calleeIndex, calleeIndex);

    // Check the index we are looking for is valid.
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), calleeIndex, callableFunctionBufferLength);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsCallIndirect);
    });

    ExpressionType calleeCode = gPtr();
    {
        static_assert(sizeof(TypeIndex) == sizeof(void*));
        ExpressionType calleeSignatureIndex = gPtr();
        // Compute the offset in the table index space we are looking for.
        append(Move, Arg::imm(sizeof(WasmToWasmImportableFunction)), calleeSignatureIndex);
        append(MulPtr, calleeIndex, calleeSignatureIndex);
        append(AddPtr, callableFunctionBuffer, calleeSignatureIndex);
        
        append(Move, Arg::addr(calleeSignatureIndex, WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation()), calleeCode); // Pointer to callee code.

        // FIXME: This seems wasteful to do two checks just for a nicer error message.
        // We should move just to use a single branch and then figure out what
        // error to use in the exception handler.

        append(Move, Arg::addr(calleeSignatureIndex, WasmToWasmImportableFunction::offsetOfSignatureIndex()), calleeSignatureIndex);

        emitCheck([&] {
            static_assert(!TypeDefinition::invalidIndex, "");
            return Inst(BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::Zero), calleeSignatureIndex, calleeSignatureIndex);
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::NullTableEntry);
        });

        ExpressionType expectedSignatureIndex = gPtr();
        append(Move, Arg::bigImm(TypeInformation::get(signature)), expectedSignatureIndex);
        emitCheck([&] {
            return Inst(BranchPtr, nullptr, Arg::relCond(MacroAssembler::NotEqual), calleeSignatureIndex, expectedSignatureIndex);
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::BadSignature);
        });
    }

    auto calleeInstance = gPtr();
    append(Move, Arg::index(instancesBuffer, calleeIndex, sizeof(void*), 0), calleeInstance);

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

auto AirIRGenerator::addCallRef(const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    m_makesCalls = true;
    // Note: call ref can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    ExpressionType calleeFunction = args.takeLast();
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    // Check the target reference for null.
#if USE(JSVALUE64)
    auto tmpForNull = g64();
    append(Move, Arg::bigImm(JSValue::encode(jsNull())), tmpForNull);
    emitCheck([&] {
        return Inst(Branch64, nullptr, Arg::relCond(MacroAssembler::Equal), calleeFunction, tmpForNull);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::NullReference);
    });
#elif USE(JSVALUE32_64)
    auto tmpForNull = g32();
    append(Move, Arg::bigImm(JSValue::NullTag), tmpForNull);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::Equal), calleeFunction.hi(), tmpForNull);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::NullReference);
    });
#endif

    auto extractPtr = [](const TypedTmp& tmp) {
#if USE(JSVALUE64)
        return tmp.tmp();
#elif USE(JSVALUE32_64)
        return tmp.lo();
#endif
    };

    ExpressionType calleeCode = gPtr();
    append(Move, Arg::addr(extractPtr(calleeFunction), WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation()), calleeCode); // Pointer to callee code.

    auto calleeInstance = gPtr();
    append(Move, Arg::addr(extractPtr(calleeFunction), WebAssemblyFunctionBase::offsetOfInstance()), calleeInstance);
    append(Move, Arg::addr(calleeInstance, JSWebAssemblyInstance::offsetOfInstance()), calleeInstance);

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

auto AirIRGenerator::emitIndirectCall(TypedTmp calleeInstance, ExpressionType calleeCode, const TypeDefinition& signature, const Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    auto currentInstance = gPtr();
    append(Move, instanceValue(), currentInstance);

    // Do a context switch if needed.
    {
        BasicBlock* doContextSwitch = m_code.addBlock();
        BasicBlock* continuation = m_code.addBlock();

        append(BranchPtr, Arg::relCond(MacroAssembler::Equal), calleeInstance, currentInstance);
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
            GPRReg scratch = params.gpScratch(0);
            ASSERT(scratch != calleeInstance);
            jit.loadPtr(CCallHelpers::Address(oldContextInstance, Instance::offsetOfCachedStackLimit()), scratch);
            jit.storePtr(scratch, CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedStackLimit()));
            jit.storeWasmContextInstance(calleeInstance);

#if !CPU(ARM_THUMB2)
            const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
            // FIXME: We should support more than one memory size register
            //   see: https://bugs.webkit.org/show_bug.cgi?id=162952
            ASSERT(pinnedRegs.boundsCheckingSizeRegister != calleeInstance);
            ASSERT(pinnedRegs.baseMemoryPointer != calleeInstance);
            jit.loadPtr(CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedBoundsCheckingSize()), pinnedRegs.boundsCheckingSizeRegister); // Bound checking size.
            jit.loadPtr(CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedMemory()), pinnedRegs.baseMemoryPointer); // Memory::void*.
            jit.cageConditionallyAndUntag(Gigacage::Primitive, pinnedRegs.baseMemoryPointer, pinnedRegs.boundsCheckingSizeRegister, scratch);
#endif
        });

        emitPatchpoint(doContextSwitch, patchpoint, TypedTmp(), calleeInstance, currentInstance);
        append(doContextSwitch, Jump);
        doContextSwitch->setSuccessors(continuation);

        m_currentBlock = continuation;
    }

    append(Move, Arg::addr(calleeCode), calleeCode);

    Vector<ConstrainedTmp> extraArgs;
    extraArgs.append(calleeCode);

    for (unsigned i = 0; i < signature.as<FunctionSignature>()->returnCount(); ++i)
        results.append(tmpForType(signature.as<FunctionSignature>()->returnType(i)));

    auto pair = emitCallPatchpoint(m_currentBlock, signature, results, args, WTFMove(extraArgs));
    auto* patchpoint = pair.first;
    auto exceptionHandle = pair.second;

    // We need to clobber all potential pinned registers since we might be leaving the instance.
    // We pessimistically assume we're always calling something that is bounds checking so
    // because the wasm->wasm thunk unconditionally overrides the size registers.
    // FIXME: We should not have to do this, but the wasm->wasm stub assumes it can
    // use all the pinned registers as scratch: https://bugs.webkit.org/show_bug.cgi?id=172181

    patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

    patchpoint->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        exceptionHandle.generate(jit, params, this);
        jit.call(params[params.proc().resultCount(params.value()->type())].gpr(), WasmEntryPtrTag);
    });

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, currentInstance, m_currentBlock);

    return { };
}

void AirIRGenerator::unify(const ExpressionType dst, const ExpressionType source)
{
    emitMove(source, dst);
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
    // FIXME: We should implement a way to give Inst's an origin, and pipe that
    // information into the sampling profiler: https://bugs.webkit.org/show_bug.cgi?id=234182
    return B3::Origin();
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileAir(CompilationContext& compilationContext, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, uint32_t functionIndex, TierUpCount* tierUp)
{
    auto result = makeUnique<InternalFunction>();

    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();

    compilationContext.procedure = makeUnique<B3::Procedure>();
    auto& procedure = *compilationContext.procedure;
    Code& code = procedure.code();

    procedure.setOriginPrinter([] (PrintStream& out, B3::Origin origin) {
        if (origin.data()) {
#if USE(JSVALUE64)
            out.print("Wasm: ", OpcodeOrigin(origin));
#elif USE(JSVALUE32_64)
            UNUSED_PARAM(out);
            UNREACHABLE_FOR_PLATFORM(); // Currently origin.data() is always nullptr.
#endif
        }
    });
    
    // This means we cannot use either StackmapGenerationParams::usedRegisters() or
    // StackmapGenerationParams::unavailableRegisters(). In exchange for this concession, we
    // don't strictly need to run Air::reportUsedRegisters(), which saves a bit of CPU time at
    // optLevel=1.
    procedure.setNeedsUsedRegisters(false);
    
    procedure.setOptLevel(Options::webAssemblyBBQAirOptimizationLevel());

    AirIRGenerator irGenerator(info, procedure, result.get(), unlinkedWasmToWasmCalls, mode, functionIndex, tierUp, signature, result->osrEntryScratchBufferSize);
    FunctionParser<AirIRGenerator> parser(irGenerator, function.data.data(), function.data.size(), signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    irGenerator.finalizeEntrypoints();

    for (BasicBlock* block : code) {
        for (size_t i = 0; i < block->numSuccessors(); ++i)
            block->successorBlock(i)->addPredecessor(block);
    }

    if (UNLIKELY(shouldDumpIRAtEachPhase(B3::AirMode))) {
        dataLogLn("Generated patchpoints");
        for (B3::PatchpointValue** patch : irGenerator.patchpoints())
            dataLogLn(deepDump(procedure, *patch));
    }

    B3::Air::prepareForGeneration(code);
    B3::Air::generate(code, *compilationContext.wasmEntrypointJIT);

    compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
    result->entrypoint.calleeSaveRegisters = code.calleeSaveRegisterAtOffsetList();
    result->stackmaps = irGenerator.takeStackmaps();
    result->exceptionHandlers = irGenerator.takeExceptionHandlers();

    return result;
}

template <typename IntType>
void AirIRGenerator::emitChecksForModOrDiv(bool isSignedDiv, ExpressionType left, ExpressionType right)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8);

    {
        BasicBlock* continuation = nullptr;
        emitCheck([&] {
            if constexpr (sizeof(IntType) == 4)
                return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), right, right);
#if USE(JSVALUE64)
            return Inst(BranchTest64, nullptr, Arg::resCond(MacroAssembler::Zero), right, right);
#elif USE(JSVALUE32_64)
            BasicBlock* checkLo = m_code.addBlock();
            continuation = m_code.addBlock();
            append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), right.hi(), right.hi());
            m_currentBlock->setSuccessors(continuation, checkLo);
            m_currentBlock = checkLo;
            return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), right.lo(), right.lo());
#endif
        }, [=, this](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::DivisionByZero);
        });
        if (continuation) {
            append(Jump);
            m_currentBlock->setSuccessors(continuation);
            m_currentBlock = continuation;
        }
    }

    if (isSignedDiv) {
        ASSERT(std::is_signed<IntType>::value);
        IntType min = std::numeric_limits<IntType>::min();

        // FIXME: Better isel for compare with imms here.
        // https://bugs.webkit.org/show_bug.cgi?id=193999

        ExpressionType badLeft;
        ExpressionType badRight;
        {
            Type type = sizeof(IntType) == 4 ? Types::I32 : Types::I64;

            auto minTmp = addConstant(type, static_cast<uint64_t>(min));
            addCompare(sizeof(IntType) == 4 ? Types::I32 : Types::I64, MacroAssembler::Equal, left, minTmp, badLeft);
            auto negOne = addConstant(type, static_cast<uint64_t>(-1));
            addCompare(sizeof(IntType) == 4 ? Types::I32 : Types::I64, MacroAssembler::Equal, right, negOne, badRight);
        }

        emitCheck([&] {
            return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), badLeft, badRight);
        },
        [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::IntegerOverflow);
        });
    }
}

namespace {
template<typename IntType>
using BinaryFuncPtr = IntType(*)(IntType, IntType);
template<typename IntType> BinaryFuncPtr<IntType> getSoftDiv() { return nullptr; }
template<typename IntType> BinaryFuncPtr<IntType> getSoftMod() { return nullptr; }
#if USE(JSVALUE32_64)
template<> BinaryFuncPtr<int32_t> getSoftDiv<int32_t>() { return Math::i32_div_s; }
template<> BinaryFuncPtr<uint32_t> getSoftDiv<uint32_t>() { return Math::i32_div_u; }
template<> BinaryFuncPtr<int64_t> getSoftDiv<int64_t>() { return Math::i64_div_s; }
template<> BinaryFuncPtr<uint64_t> getSoftDiv<uint64_t>() { return Math::i64_div_u; }
template<> BinaryFuncPtr<int32_t> getSoftMod<int32_t>() { return Math::i32_rem_s; }
template<> BinaryFuncPtr<uint32_t> getSoftMod<uint32_t>() { return Math::i32_rem_u; }
template<> BinaryFuncPtr<int64_t> getSoftMod<int64_t>() { return Math::i64_rem_s; }
template<> BinaryFuncPtr<uint64_t> getSoftMod<uint64_t>() { return Math::i64_rem_u; }
#endif
}

template <typename IntType>
void AirIRGenerator::emitModOrDiv(bool isDiv, ExpressionType lhs, ExpressionType rhs, ExpressionType& result)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8);

    result = sizeof(IntType) == 4 ? g32() : g64();

    bool isSigned = std::is_signed<IntType>::value;

    if (isARM()) {
        // FIXME: use ARMv7 sdiv/udiv if available
        if (isDiv)
            emitCCall(getSoftDiv<IntType>(), result, lhs, rhs);
        else
            emitCCall(getSoftMod<IntType>(), result, lhs, rhs);
        return;
    }

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
    result = g64();
#if USE(JSVALUE64)
    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.countTrailingZeros64(params[1].gpr(), params[0].gpr());
    });
    emitPatchpoint(patchpoint, result, arg);
#elif USE(JSVALUE32_64)
    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
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
#endif
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
    result = f64();
#if USE(JSVALUE64)
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
    emitPatchpoint(patchpoint, result, arg);
#elif USE(JSVALUE32_64)
    emitCCall(Math::f64_convert_u_i64, result, arg);
#endif
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F32ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f32();
#if USE(JSVALUE64)
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
    emitPatchpoint(patchpoint, result, arg);
#elif USE(JSVALUE32_64)
    emitCCall(Math::f32_convert_u_i64, result, arg);
#endif
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F64Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f64();
#if USE(JSVALUE64)
    auto* patchpoint = addPatchpoint(B3::Double);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=](CCallHelpers &jit, const B3::StackmapGenerationParams &params) {
        jit.roundTowardNearestIntDouble(params[1].fpr(), params[0].fpr());
    });
    emitPatchpoint(patchpoint, result, arg);
#elif USE(JSVALUE32_64)
    emitCCall(&Math::f64_nearest, result, arg);
#endif
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F32Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f32();
#if USE(JSVALUE64)
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.roundTowardNearestIntFloat(params[1].fpr(), params[0].fpr());
    });
    emitPatchpoint(patchpoint, result, arg);
#elif USE(JSVALUE32_64)
    emitCCall(&Math::f32_nearest, result, arg);
#endif
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F64Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f64();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        auto* patchpoint = addPatchpoint(B3::Double);
        patchpoint->effects = B3::Effects::none();
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
        });
        emitPatchpoint(patchpoint, result, arg);
        return { };
    }
    emitCCall(&Math::truncDouble, result, arg);
    return { };
}

template<>
auto AirIRGenerator::addOp<OpType::F32Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = f32();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        auto* patchpoint = addPatchpoint(B3::Float);
        patchpoint->effects = B3::Effects::none();
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            jit.roundTowardZeroFloat(params[1].fpr(), params[0].fpr());
        });
        emitPatchpoint(patchpoint, result, arg);
        return { };
    }
    emitCCall(&Math::truncFloat, result, arg);
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    result = g64();
#if USE(JSVALUE64)
    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
    });

    emitPatchpoint(patchpoint, result, arg);
#elif USE(JSVALUE32_64)
    emitCCall(Math::i64_trunc_s_f64, result, arg);
#endif
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    result = g64();
#if USE(JSVALUE64)
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
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        FPRReg scratch = InvalidFPRReg;
        FPRReg constant = InvalidFPRReg;
        if (isX86()) {
            scratch = params.fpScratch(0);
            constant = params[2].fpr();
        }
        jit.truncateDoubleToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
    });

    emitPatchpoint(m_currentBlock, patchpoint, Vector<TypedTmp, 8> { result }, WTFMove(args));
#elif USE(JSVALUE32_64)
    emitCCall(Math::i64_trunc_u_f64, result, arg);
#endif
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    result = g64();
#if USE(JSVALUE64)
    auto* patchpoint = addPatchpoint(B3::Int64);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
    });
    emitPatchpoint(patchpoint, result, arg);
#elif USE(JSVALUE32_64)
    emitCCall(Math::i64_trunc_s_f32, result, arg);
#endif
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
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    result = g64();
#if USE(JSVALUE64)
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

    emitPatchpoint(m_currentBlock, patchpoint, Vector<TypedTmp, 8> { result }, WTFMove(args));
#elif USE(JSVALUE32_64)
    emitCCall(Math::i64_trunc_u_f32, result, arg);
#endif
    return { };
}

auto AirIRGenerator::addShift(Type type, B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isI64() || type.isI32());

#if USE(JSVALUE32_64)
    if (type.isI64()) {
        if (op == RotateRight64 || op == RotateLeft64)
            return addRotate64(op, value, shift, result);
        return addShift64(op, value, shift, result);
    }
#endif

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

#if USE(JSVALUE32_64)
auto AirIRGenerator::addShift64(B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
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

auto AirIRGenerator::addRotate64(B3::Air::Opcode op, ExpressionType value, ExpressionType shift, ExpressionType& result) -> PartialResult
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

    append(Shuffle, tmpArgLo, tmpArgHi, Arg::widthArg(B3::Width32), tmpArgHi, tmpArgLo, Arg::widthArg(B3::Width32));
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
#endif

auto AirIRGenerator::addCompare(Type type, MacroAssembler::RelationalCondition cond, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(type.isI32() || type.isI64());

    result = g32();

    if (type.isI32()) {
        append(Compare32, Arg::relCond(cond), lhs, rhs, result);
        return { };
    }

#if USE(JSVALUE64)
    append(Compare64, Arg::relCond(cond), lhs, rhs, result);
#elif USE(JSVALUE32_64)
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
#endif

    return { };
}

auto AirIRGenerator::addIntegerSub(B3::Air::Opcode op, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(op == Sub32 || op == Sub64);

    result = op == Sub32 ? g32() : g64();

#if USE(JSVALUE32_64)
    if (op == Sub64) {
        append(op, lhs.hi(), lhs.lo(), rhs.hi(), rhs.lo(), result.hi(), result.lo());
        return { };
    }
#endif

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
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(CeilFloat, arg0, result);
        return { };
    }
    emitCCall(&Math::ceilFloat, result, arg0);
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

    if (isARM64()) {
        if (floatType.isF32())
            append(m_currentBlock, minOrMax == MinOrMax::Max ? FloatMax : FloatMin, arg0, arg1, result);
        else
            append(m_currentBlock, minOrMax == MinOrMax::Max ? DoubleMax : DoubleMin, arg0, arg1, result);
        return { };
    }

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

template<> auto AirIRGenerator::addOp<OpType::F32Min>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F32, MinOrMax::Min, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Max>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F32, MinOrMax::Max, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Min>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F64, MinOrMax::Min, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Max>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F64, MinOrMax::Max, arg0, arg1, result);
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

    append(MoveFloatTo32, arg0, temp1);
    append(Move, Arg::bigImm(0x7fffffff), value);
    append(And32, temp1, value, value);

    append(Or32, sign, value, value);
    append(Move32ToFloat, value, result);

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F64ConvertUI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
#if USE(JSVALUE64)
    auto temp = g64();
    append(Move32, arg0, temp);
    append(ConvertInt64ToDouble, temp, result);
#elif USE(JSVALUE32_64)
    auto* patchpoint = addPatchpoint(B3::Double);
    patchpoint->effects = B3::Effects::none();
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.convertUInt32ToDouble(params[1].gpr(), params[0].fpr());
    });
    emitPatchpoint(patchpoint, result, arg0);
#endif
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
#if USE(JSVALUE64)
    append(And64, arg0, arg1, result);
#elif USE(JSVALUE32_64)
    append(And32, arg0.lo(), arg1.lo(), result.lo());
    append(And32, arg0.hi(), arg1.hi(), result.hi());
#endif
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
    return addCompare(Types::I64, MacroAssembler::GreaterThan, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64GtU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I64, MacroAssembler::Above, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64Eqz>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g32();
#if USE(JSVALUE64)
    append(Test64, Arg::resCond(MacroAssembler::Zero), arg0, arg0, result);
#elif USE(JSVALUE32_64)
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
#endif
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
#if USE(JSVALUE64)
    append(Or64, arg0, arg1, result);
#elif USE(JSVALUE32_64)
    append(Or32, arg0.lo(), arg1.lo(), result.lo());
    append(Or32, arg0.hi(), arg1.hi(), result.hi());
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32LeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I32, MacroAssembler::BelowOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32LeS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I32, MacroAssembler::LessThanOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64Ne>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I64, MacroAssembler::NotEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64Clz>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    append(CountLeadingZeros64, arg, result);
#elif USE(JSVALUE32_64)
    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
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
#endif
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
    return addCompare(Types::I32, MacroAssembler::Below, arg0, arg1, result);
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
    return addCompare(Types::I32, MacroAssembler::LessThan, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32Eq>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I32, MacroAssembler::Equal, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Copysign>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    // FIXME: We can have better codegen here for the imms and two operand forms on x86
    // https://bugs.webkit.org/show_bug.cgi?id=193999
    result = f64();
#if USE(JSVALUE64)
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
#elif USE(JSVALUE32_64)
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
#endif

    return { };
}

template<> auto AirIRGenerator::addOp<OpType::F32ConvertSI64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
#if USE(JSVALUE64)
    append(ConvertInt64ToFloat, arg0, result);
#elif USE(JSVALUE32_64)
    emitCCall(Math::f32_convert_s_i64, result, arg0);
#endif
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
#if USE(JSVALUE64)
    auto temp = g64();
    append(Move32, arg0, temp);
    append(ConvertInt64ToFloat, temp, result);
#elif USE(JSVALUE32_64)
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.convertUInt32ToFloat(params[1].gpr(), params[0].fpr());
    });
    emitPatchpoint(patchpoint, result, arg0);
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32ShrS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Types::I32, Rshift32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32GeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I32, MacroAssembler::AboveOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Ceil>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(CeilDouble, arg0, result);
        return { };
    }
    emitCCall(&Math::ceilDouble, result, arg0);
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32GeS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I32, MacroAssembler::GreaterThanOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32Shl>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Types::I32, Lshift32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64Floor>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(FloorDouble, arg0, result);
        return { };
    }
    emitCCall(&Math::floorDouble, result, arg0);
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
    return addCompare(Types::I64, MacroAssembler::Below, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64LtS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I64, MacroAssembler::LessThan, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64ConvertSI64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
#if USE(JSVALUE64)
    append(ConvertInt64ToDouble, arg0, result);
#elif USE(JSVALUE32_64)
    emitCCall(Math::f64_convert_s_i64, result, arg0);
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Xor>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    append(Xor64, arg0, arg1, result);
#elif USE(JSVALUE32_64)
    append(Xor32, arg0.lo(), arg1.lo(), result.lo());
    append(Xor32, arg0.hi(), arg1.hi(), result.hi());
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64GeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I64, MacroAssembler::AboveOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64Mul>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    append(Mul64, arg0, arg1, result);
#elif USE(JSVALUE32_64)
    auto tmpHiLo = g32();
    auto tmpLoHi = g32();
    append(Mul32, arg0.hi(), arg1.lo(), tmpHiLo);
    append(Mul32, arg0.lo(), arg1.hi(), tmpLoHi);
    append(UMull32, arg0.lo(), arg1.lo(), result.hi(),  result.lo());
    append(Add32, tmpHiLo, result.hi());
    append(Add32, tmpLoHi, result.hi());
#endif
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
    return addCompare(Types::I64, MacroAssembler::GreaterThanOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64ExtendUI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    append(Move32, arg0, result);
#elif USE(JSVALUE32_64)
    append(Move, arg0.tmp(), result.lo());
    append(Move, Arg::imm(0), result.hi());
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Ne>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    RELEASE_ASSERT(arg0 && arg1);
    return addCompare(Types::I32, MacroAssembler::NotEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F64ReinterpretI64>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f64();
#if USE(JSVALUE64)
    append(Move64ToDouble, arg0, result);
#elif USE(JSVALUE32_64)
    append(Move64ToDouble, arg0.hi(), arg0.lo(), result);
#endif
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
    return addCompare(Types::I64, MacroAssembler::Equal, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::F32Floor>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = f32();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(FloorFloat, arg0, result);
        return { };
    }
    emitCCall(&Math::floorFloat, result, arg0);
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
#if USE(JSVALUE64)
    append(MoveDoubleTo64, arg0, result);
#elif USE(JSVALUE32_64)
    append(MoveDoubleTo64, arg0, result.hi(), result.lo());
#endif
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
#if USE(JSVALUE64)
    append(Move32, arg0, result);
#elif USE(JSVALUE32_64)
    append(Move, arg0.lo(), result);
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32Rotl>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    if (isARM64() || isARM()) {
        // ARMs do not have a 'rotate left' instruction.
        auto newShift = isARM64() ? g64() : g32();
        append(Move, arg1, newShift);
        append(isARM64() ? Neg64 : Neg32, newShift);
        return addShift(Types::I32, RotateRight32, arg0, newShift, result);
    }
    return addShift(Types::I32, RotateLeft32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32Rotr>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addShift(Types::I32, RotateRight32, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I32GtU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I32, MacroAssembler::Above, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64ExtendSI32>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    append(SignExtend32ToPtr, arg0, result);
#elif USE(JSVALUE32_64)
    append(Move, arg0.tmp(), result.lo());
    append(Rshift32, arg0.tmp(), Arg::imm(31), result.hi());
#endif
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
#if USE(JSVALUE64)
    auto temp = g32();
    append(Move32, arg0, temp);
    append(SignExtend8To32, temp, temp);
    append(SignExtend32ToPtr, temp, result);
#elif USE(JSVALUE32_64)
    append(SignExtend8To32, arg0.lo(), result.lo());
    append(Rshift32, result.lo(), Arg::imm(31), result.hi());
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Extend16S>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    auto temp = g32();
    append(Move32, arg0, temp);
    append(SignExtend16To32, temp, temp);
    append(SignExtend32ToPtr, temp, result);
#elif USE(JSVALUE32_64)
    append(SignExtend16To32, arg0.lo(), result.lo());
    append(Rshift32, result.lo(), Arg::imm(31), result.hi());
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I64Extend32S>(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    auto temp = g32();
    append(Move32, arg0, temp);
    append(SignExtend32ToPtr, temp, result);
#elif USE(JSVALUE32_64)
    append(Move, arg0.lo(), result.lo());
    append(Rshift32, arg0.lo(), Arg::imm(31), result.hi());
#endif
    return { };
}

template<> auto AirIRGenerator::addOp<OpType::I32GtS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I32, MacroAssembler::GreaterThan, arg0, arg1, result);
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

template<> auto AirIRGenerator::addOp<OpType::I64LeU>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I64, MacroAssembler::BelowOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64LeS>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addCompare(Types::I64, MacroAssembler::LessThanOrEqual, arg0, arg1, result);
}

template<> auto AirIRGenerator::addOp<OpType::I64Add>(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = g64();
#if USE(JSVALUE64)
    append(Add64, arg0, arg1, result);
#elif USE(JSVALUE32_64)
    append(Add64, arg0.hi(), arg0.lo(), arg1.hi(), arg1.lo(), result.hi(), result.lo());
#endif
    return { };
}

template <size_t inlineCapacity>
PatchpointExceptionHandle AirIRGenerator::preparePatchpointForExceptions(B3::PatchpointValue* patch, Vector<ConstrainedTmp, inlineCapacity>& args)
{
    ++m_callSiteIndex;
    if (!m_tryCatchDepth)
        return { };

    unsigned numLiveValues = 0;
    forEachLiveValue([&] (TypedTmp tmp) {
        ++numLiveValues;
        if (tmp.isGPPair()) {
            ++numLiveValues;
            args.append(ConstrainedTmp(tmp.lo(), B3::ValueRep::LateColdAny));
            args.append(ConstrainedTmp(tmp.hi(), B3::ValueRep::LateColdAny));
            return;
        }
        args.append(ConstrainedTmp(tmp.tmp(), B3::ValueRep::LateColdAny));
    });

    patch->effects.exitsSideways = true;

    return PatchpointExceptionHandle { m_callSiteIndex, numLiveValues };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_B3JIT)
