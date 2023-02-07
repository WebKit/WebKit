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

#pragma once

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
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyStruct.h"
#include "SIMDInfo.h"
#include "ScratchRegisterAllocator.h"
#include "WasmBranchHints.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
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

namespace WasmAirIRGeneratorInternal {
    static constexpr bool verbose = false;
}

using namespace B3::Air;

/*
 * Wasm::AirIRGeneratorBase
 *
 * This is a template base class for a code generator that cooperates with the
 * wasm function parser and a suitable deriving class to produce a valid Air
 * unit for that wasm function.
 *
 * This is a _template_ base class to accomodate both 32- and 64-bit
 * platforms, which require pretty different Air units to be generated; a
 * cooperating deriving class provides the methods that depend on the word size
 * of the target platform.
 *
 * Some wasm instructions are also handled directly in the deriving classes
 * because their code generation is so different it's easier not to share code,
 * if the instruction you are looking for is not here, check either in
 * AirIRGenerator64 or AirIRGenerator32_64
 *
 * Access methods provided by the deriving class by calling `self()` (to obtain
 * `*this` downcasted as `Derived&`)
 *
 * The template parameters are:
 * - Derived: the deriving class
 * - ExpressionType: the type of wasm values--morally one or more Air::Tmp's
 *   paired with a wasm type
 */
template<typename Derived, typename ExpressionType>
struct AirIRGeneratorBase {
    ////////////////////////////////////////////////////////////////////////////////
    // Related types

    using ResultList = Vector<ExpressionType, 8>;
    using CallType = CallLinkInfo::CallType;
    using CallPatchpointData = std::pair<B3::PatchpointValue*, Box<PatchpointExceptionHandle>>;

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

        void convertTryToCatch(unsigned tryEndCallSiteIndex, ExpressionType exception)
        {
            ASSERT(blockType() == BlockType::Try);
            controlBlockType = BlockType::Catch;
            m_catchKind = CatchKind::Catch;
            m_tryEnd = tryEndCallSiteIndex;
            m_exception = exception;
        }

        void convertTryToCatchAll(unsigned tryEndCallSiteIndex, ExpressionType exception)
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

        ExpressionType exception() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_exception;
        }

    private:
        friend Derived;
        friend AirIRGeneratorBase;
        BlockType controlBlockType;
        BasicBlock* continuation;
        BasicBlock* special;
        ResultList results;
        BlockSignature returnType;
        unsigned m_tryStart;
        unsigned m_tryEnd;
        unsigned m_tryCatchDepth;
        CatchKind m_catchKind;
        ExpressionType m_exception;
    };

    using ControlType = ControlData;

    using ParserTypes = FunctionParserTypes<ControlType, ExpressionType, CallType>;

    using ControlEntry = typename ParserTypes::ControlEntry;
    using ControlStack = typename ParserTypes::ControlStack;
    using Stack = typename ParserTypes::Stack;
    using TypedExpression = typename ParserTypes::TypedExpression;

    using ErrorType = String;
    using UnexpectedResult = Unexpected<ErrorType>;
    using Result = Expected<std::unique_ptr<InternalFunction>, ErrorType>;
    using PartialResult = Expected<void, ErrorType>;

    static_assert(std::is_same_v<ResultList, typename ParserTypes::ResultList>);

    struct ConstrainedTmp {
        ConstrainedTmp() = default;
        ConstrainedTmp(ExpressionType tmp)
            : ConstrainedTmp(tmp, tmp.tmp().isReg() ? B3::ValueRep::reg(tmp.tmp().reg()) : B3::ValueRep::SomeRegister)
        {
        }

        ConstrainedTmp(ExpressionType tmp, B3::ValueRep rep)
            : tmp(tmp)
            , rep(rep)
        {
        }

        ConstrainedTmp(ExpressionType tmp, ArgumentLocation loc)
            : tmp(tmp)
            , rep(loc.location)
        {
        }

        explicit operator bool() const { return !!tmp; }

        void dump(PrintStream& out) const
        {
            out.print("ConstrainedTmp { ", tmp, ", ", rep, " }");
        }

        ExpressionType tmp;
        B3::ValueRep rep;
    };

    ////////////////////////////////////////////////////////////////////////////////
    // Get concrete instance

    Derived& self()
    {
        return *static_cast<Derived*>(this);
    }

    const Derived& self() const
    {
        return *static_cast<const Derived*>(this);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Failure reporting

    template<typename... Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module failed compiling: "_s, makeString(args)...));
    }

#define WASM_COMPILE_FAIL_IF(condition, ...) \
    do { \
        if (UNLIKELY(condition))             \
            return self().fail(__VA_ARGS__); \
    } while (0)

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor

    AirIRGeneratorBase(const ModuleInformation&, Callee&, B3::Procedure&, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode, unsigned functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount*, const TypeDefinition& originalSignature, unsigned& osrEntryScratchBufferSize);

    void finalizeEntrypoints();

    PartialResult WARN_UNUSED_RETURN addArguments(const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    //             addConstant (in derived classes)
    ExpressionType addBottom(BasicBlock*, Type);
    ExpressionType addConstantZero(BasicBlock*, Type);

    // References
    //                               addRefIsNull (in derived classes)
    PartialResult WARN_UNUSED_RETURN addRefFunc(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addRefAsNonNull(ExpressionType, ExpressionType&);

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

    // GC (in derived classes)
    PartialResult WARN_UNUSED_RETURN addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNewDefault(uint32_t index, ExpressionType size, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayGet(ExtGCOpType arrayGetKind, uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addArrayLen(ExpressionType arrayref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t index, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructNewDefault(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructGet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructSet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType value);

    // Basic operators
    //
    // Note some of these are not defined in AirIRGeneratorBase: they are expected
    // to be implemented by deriving classes. Using them will be a link error
    // ... but we don't have a convenient way to leave them out, so here is a
    // list of morally-deleted ones, as a magnet for developers searching for
    // these names.
    //
    // When we have full compiler support for C++20 concepts these shenanigans
    // can be made proper compiler errors

    /* These are morally deleted, but cannot actually be declared as such (see note above)
     *
     * // These require 64-bit-only masm instructions in patchpoints
     * PartialResult addF64ConvertUI64(ExpressionType arg, ExpressionType& result) = delete;
     * PartialResult addF32ConvertUI64(ExpressionType arg, ExpressionType& result) = delete;
     * PartialResult addI64Ctz(ExpressionType arg, ExpressionType& result) = delete;
     */

#define X(name, opcode, short, idx, ...) \
    PartialResult WARN_UNUSED_RETURN add##name(ExpressionType arg, ExpressionType& result);
    FOR_EACH_WASM_UNARY_OP(X)
#undef X
#define X(name, opcode, short, idx, ...) \
    PartialResult WARN_UNUSED_RETURN add##name(ExpressionType left, ExpressionType right, ExpressionType& result);
    FOR_EACH_WASM_BINARY_OP(X)
#undef X
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
    //                               addThrow
    //                               addRethrow (in derived classes)
    //                               addReturn (in derived classes)

    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& expressionStack = { });

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&) { return { }; }

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN emitIndirectCall(ExpressionType calleeInstance, ExpressionType calleeCode, ExpressionType jsCalleeAnchor, const TypeDefinition&, const Vector<ExpressionType>& args, ResultList&, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN addCrash();

    enum class FloatingPointTruncationKind {
        F32ToI32,
        F32ToU32,
        F32ToI64,
        F32ToU64,
        F64ToI32,
        F64ToU32,
        F64ToI64,
        F64ToU64,
    };

    struct FloatingPointRange {
        ExpressionType lowerEndpoint;
        ExpressionType upperEndpoint;
        // Some floating point truncation operations have a half-open range; others
        // are more convenient to specify as a open range.
        //
        // The top endpoint of the range is always excluded, i.e. this value chooses between:
        // closedLowerEndopint = true    =>   range === [min, max)
        // closedLowerEndopint = false   =>   range === (min, max)
        bool closedLowerEndpoint;
    };

    FloatingPointRange lookupTruncationRange(FloatingPointTruncationKind);
    PartialResult addCheckedFloatingPointTruncation(FloatingPointTruncationKind, ExpressionType arg, ExpressionType& result);

    void dump(const ControlStack&, const Stack* expressionStack);
    void setParser(FunctionParser<Derived>* parser) { m_parser = parser; };
    void didFinishParsingLocals() { }
    void didPopValueFromStack() { }

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

protected:
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

    template<typename... Arguments>
    void append(BasicBlock* block, Kind kind, Arguments&&... arguments)
    {
        // FIXME: Find a way to use origin here.
        auto& inst = block->append(kind, nullptr, Derived::extractArg(arguments)...);
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

    ExpressionType tmpForType(Type type)
    {
        switch (type.kind) {
        case TypeKind::I32:
            return self().g32();
        case TypeKind::I64:
            return self().g64();
        case TypeKind::Funcref:
            return self().gFuncref();
        case TypeKind::Ref:
        case TypeKind::RefNull:
            return self().gRef(type);
        case TypeKind::Externref:
            return self().gExternref();
        case TypeKind::F32:
            return self().f32();
        case TypeKind::F64:
            return self().f64();
        case TypeKind::V128:
            return self().v128();
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
            result[i] = self().tmpForType(signature->as<FunctionSignature>()->returnType(i));
        return result;
    }


    B3::PatchpointValue* addPatchpoint(B3::Type type)
    {
        auto* result = m_proc.add<B3::PatchpointValue>(type, B3::Origin());
        if (UNLIKELY(shouldDumpIRAtEachPhase(B3::AirMode)))
            m_patchpoints.add(result);
        return result;
    }

    template<typename... Args>
    void emitPatchpoint(B3::PatchpointValue* patch, Tmp result, Args... theArgs)
    {
        emitPatchpoint(m_currentBlock, patch, result, std::forward<Args>(theArgs)...);
    }

    template<typename... Args>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result, Args... theArgs)
    {
        emitPatchpoint(basicBlock, patch, Vector<Tmp, 8> { result }, Vector<ConstrainedTmp, sizeof...(Args)>::from(theArgs...));
    }

    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result)
    {
        emitPatchpoint(basicBlock, patch, Vector<Tmp, 8> { result }, Vector<ConstrainedTmp>());
    }

    template<size_t inlineSize>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, Tmp result, Vector<ConstrainedTmp, inlineSize>&& args)
    {
        emitPatchpoint(basicBlock, patch, Vector<Tmp, 8> { result }, WTFMove(args));
    }

    template <typename ResultTmpType, size_t inlineSize>
    void emitPatchpoint(BasicBlock* basicBlock, B3::PatchpointValue* patch, const Vector<ResultTmpType, 8>& results, Vector<ConstrainedTmp, inlineSize>&& args)
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
            // validation. We should abstract Patch enough so ValueRep's don't need to be
            // backed by Values.
            // https://bugs.webkit.org/show_bug.cgi?id=194040
            B3::Value* dummyValue = m_proc.addConstant(B3::Origin(), tmp.tmp.tmp().isGP() ? B3::pointerType() : (m_proc.usesSIMD() ? B3::V128 : B3::Double), 0);
            patch->append(dummyValue, tmp.rep);
            switch (tmp.rep.kind()) {
            // B3::Value propagates (Late)ColdAny information and later Air will allocate appropriate stack.
            case B3::ValueRep::ColdAny:
            case B3::ValueRep::LateColdAny:
            case B3::ValueRep::SomeRegister:
                inst.args.append(tmp.tmp);
                break;
            case B3::ValueRep::Register:
                patch->earlyClobbered().remove(tmp.rep.reg());
                append(basicBlock, relaxedMoveForType(toB3Type(tmp.tmp.type())), tmp.tmp.tmp(), tmp.rep.reg());
                inst.args.append(Tmp(tmp.rep.reg()));
                break;
            case B3::ValueRep::StackArgument: {
                Arg arg = Arg::callArg(tmp.rep.offsetFromSP());
                B3::Air::Opcode opcode = moveForType(toB3Type(tmp.tmp.type()));
                if (arg.isValidForm(opcode, pointerWidth()))
                    append(basicBlock, opcode, tmp.tmp, arg);
                else {
                    typename Derived::ExpressionType immTmp = self().gPtr();
                    typename Derived::ExpressionType newPtr = self().gPtr();
                    append(basicBlock, Move, Arg::bigImm(arg.offset()), immTmp);
                    append(basicBlock, Derived::AddPtr, Tmp(MacroAssembler::stackPointerRegister), immTmp, newPtr);
                    append(basicBlock, opcode, tmp.tmp, Arg::addr(newPtr));
                }
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
                patch->lateClobbered().remove(valueRep.reg());
        }
        for (unsigned i = patch->numGPScratchRegisters; i--;)
            inst.args.append(self().gPtr().tmp());
        for (unsigned i = patch->numFPScratchRegisters; i--;)
            inst.args.append(self().f64().tmp());

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
    void emitCCall(Func func, ExpressionType result, Args... args)
    {
        emitCCall(m_currentBlock, func, result, std::forward<Args>(args)...);
    }
    template <typename Func, typename ...Args>
    void emitCCall(BasicBlock* block, Func func, ExpressionType result, Args... theArgs)
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
            case TypeKind::Ref:
            case TypeKind::RefNull:
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

        auto makeDummyValue = [&] (auto tmp) {
            // FIXME: This is less than ideal to create dummy values just to satisfy Air's
            // validation. We should abstract CCall enough so we're not reliant on arguments
            // to the B3::CCallValue.
            // https://bugs.webkit.org/show_bug.cgi?id=194040
            return m_proc.addConstant(B3::Origin(), toB3Type(tmp.type()), 0);
        };

#if !USE(BUILTIN_FRAME_ADDRESS) || ASSERT_ENABLED
        // Prepare wasm operation calls.
        self().emitStore(ExpressionType { Tmp(GPRInfo::callFrameRegister), Types::IPtr }, instanceValue().tmp(), Instance::offsetOfTemporaryCallFrame());
#endif

        B3::Value* dummyFunc = m_proc.addConstant(B3::Origin(), B3::pointerType(), bitwise_cast<uintptr_t>(func));
        B3::Value* origin = m_proc.add<B3::CCallValue>(resultType, B3::Origin(), B3::Effects::none(), dummyFunc, makeDummyValue(theArgs)...);

        Inst inst(CCall, origin);

        auto callee = self().gPtr();
        append(block, Move, Arg::immPtr(tagCFunctionPtr<void*, OperationPtrTag>(func)), callee);
        inst.args.append(callee);

        if (result)
            self().appendCCallArg(inst, result);

        for (auto tmp : Vector<ExpressionType, sizeof...(Args)>::from(theArgs...))
            self().appendCCallArg(inst, tmp);

        block->append(WTFMove(inst));
    }

protected:
    void emitThrowException(CCallHelpers& jit, ExceptionType);
    void emitThrowOnNullReference(const ExpressionType& ref, ExceptionType exceptionType = ExceptionType::NullReference)
    {
        self().emitCheckForNullReference(ref, [=, this](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, exceptionType);
        });
    }

    void emitEntryTierUpCheck();
    void emitLoopTierUpCheck(uint32_t loopIndex, const Vector<ExpressionType>& liveValues);

    void emitWriteBarrierForJSWrapper();
    void emitWriteBarrier(ExpressionType cell, ExpressionType instanceCell);

    ExpressionType emitAtomicLoadOp(ExtAtomicOpType, Type, ExpressionType pointer, uint32_t offset);
    void emitAtomicStoreOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicBinaryRMWOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicCompareExchange(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t offset);

    void unifyValuesWithBlock(const Stack& resultStack, const ResultList& stack);

    template <typename IntType>
    void emitChecksForModOrDiv(bool isSignedDiv, ExpressionType left, ExpressionType right);

    enum class MinOrMax { Min, Max };
    PartialResult addFloatingPointMinOrMax(Type, MinOrMax, ExpressionType lhs, ExpressionType rhs, ExpressionType& result);

    int32_t WARN_UNUSED_RETURN fixupPointerPlusOffset(ExpressionType&, uint32_t);
    ExpressionType WARN_UNUSED_RETURN fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType, ExpressionType, uint32_t);

    void restoreWasmContextInstance(BasicBlock*, ExpressionType);
    void restoreWebAssemblyGlobalState(const MemoryInformation&, ExpressionType instance, BasicBlock*);

    B3::Origin origin();

    uint32_t outerLoopIndex() const
    {
        if (m_outerLoops.isEmpty())
            return UINT32_MAX;
        return m_outerLoops.last();
    }

    template <typename Function>
    void forEachLiveValue(Function&&);

public:
    ////////////////////////////////////////////////////////////////////////////////
    // data members

    FunctionParser<Derived>* m_parser { nullptr };
    const ModuleInformation& m_info;
    Callee& m_callee;
    const MemoryMode m_mode { MemoryMode::BoundsChecking };
    const unsigned m_functionIndex { UINT_MAX };
    TierUpCount* m_tierUp { nullptr };

    B3::Procedure& m_proc;
    Code& m_code;
    Vector<uint32_t> m_outerLoops;
    BasicBlock* m_currentBlock { nullptr };
    BasicBlock* m_rootBlock { nullptr };
    BasicBlock* m_mainEntrypointStart { nullptr };
    Vector<ExpressionType> m_locals;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.

    bool m_makesCalls { false };
    bool m_makesTailCalls { false };

    // This tracks the maximum stack offset for a tail call, to be used in the stack overflow check.
    Checked<int32_t> m_tailCallStackOffsetFromFP { 0 };

    std::optional<bool> m_hasExceptionHandlers;

    HashMap<BlockSignature, B3::Type> m_tupleMap;
    // This is only filled if we are dumping IR.
    Bag<B3::PatchpointValue*> m_patchpoints;

    ExpressionType m_instanceValue; // Always use the accessor below to ensure the instance value is materialized when used.
    bool m_usesInstanceValue { false };
    ExpressionType instanceValue()
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

    Vector<std::pair<BasicBlock*, Vector<ExpressionType>>> m_loopEntryVariableData;
    unsigned& m_osrEntryScratchBufferSize;

public:
    ////////////////////////////////////////////////////////////////////////////////
    // parameters for shared code
    static constexpr bool generatesB3OriginData = true;
    static constexpr bool supportsPinnedStateRegisters = true;
};

// Memory accesses in WebAssembly have unsigned 32-bit offsets, whereas they have signed 32-bit offsets in B3.
template<typename Derived, typename ExpressionType>
int32_t AirIRGeneratorBase<Derived, ExpressionType>::fixupPointerPlusOffset(ExpressionType& ptr, uint32_t offset)
{
    if (static_cast<uint64_t>(offset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        auto previousPtr = ptr;
        ptr = self().gPtr();
        auto constant = self().gPtr();
        append(Move, Arg::bigImm(offset), constant);
        append(Derived::AddPtr, constant, previousPtr, ptr);
        return 0;
    }
    return offset;
}

template <typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::restoreWasmContextInstance(BasicBlock* block, ExpressionType instance)
{
    // FIXME: Because WasmToWasm call clobbers wasmContextInstance register and does not restore it, we need to restore it in the caller side.
    // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
    auto* patchpoint = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.writesPinned = true;
    effects.reads = B3::HeapRange::top();
    patchpoint->effects = effects;
    patchpoint->clobberLate(RegisterSetBuilder(GPRInfo::wasmContextInstancePointer));
    patchpoint->setGenerator([](CCallHelpers& jit, const B3::StackmapGenerationParams& param) {
        jit.move(param[0].gpr(), GPRInfo::wasmContextInstancePointer);
    });
    emitPatchpoint(block, patchpoint, ExpressionType(), instance);
}

template <typename Derived, typename ExpressionType>
AirIRGeneratorBase<Derived, ExpressionType>::AirIRGeneratorBase(const ModuleInformation& info, Callee& callee, B3::Procedure& procedure, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, unsigned functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount* tierUp, const TypeDefinition& originalSignature, unsigned& osrEntryScratchBufferSize)
    : m_info(info)
    , m_callee(callee)
    , m_mode(mode)
    , m_functionIndex(functionIndex)
    , m_tierUp(tierUp)
    , m_proc(procedure)
    , m_code(m_proc.code())
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_hasExceptionHandlers(hasExceptionHandlers)
    , m_numImportFunctions(info.importFunctionCount())
    , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
{
    m_currentBlock = m_code.addBlock();
    m_rootBlock = m_currentBlock;

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623
    m_code.pinRegister(GPRInfo::wasmContextInstancePointer);

    if constexpr (Derived::supportsPinnedStateRegisters) {
        m_code.pinRegister(GPRInfo::wasmBaseMemoryPointer);
        if (mode == MemoryMode::BoundsChecking)
            m_code.pinRegister(GPRInfo::wasmBoundsCheckingSizeRegister);
    } else {
        ASSERT(InvalidGPRReg == GPRInfo::wasmBaseMemoryPointer);
        ASSERT(InvalidGPRReg == GPRInfo::wasmBoundsCheckingSizeRegister);
    }

    m_prologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([=, this] (CCallHelpers& jit, B3::Air::Code& code) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        code.emitDefaultPrologue(jit);

        {
            GPRReg scratchGPR = wasmCallingConvention().prologueScratchGPRs[0];
            jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxWasm(&m_callee)), scratchGPR);
            static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
            if constexpr (is32Bit()) {
                CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
                jit.storePtr(scratchGPR, calleeSlot.withOffset(PayloadOffset));
                jit.store32(CCallHelpers::TrustedImm32(JSValue::WasmTag), calleeSlot.withOffset(TagOffset));
                jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
            } else
                jit.storePairPtr(GPRInfo::wasmContextInstancePointer, scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));
        }

        {
            const Checked<int32_t> wasmFrameSize = m_code.frameSize();
            const Checked<int32_t> wasmTailCallFrameSize = -m_tailCallStackOffsetFromFP;
            const unsigned minimumParentCheckSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), 1024);
            const unsigned extraFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), std::max<uint32_t>(
                // This allows us to elide stack checks for functions that are terminal nodes in the call
                // tree, (e.g they don't make any calls) and have a small enough frame size. This works by
                // having any such terminal node have its parent caller include some extra size in its
                // own check for it. The goal here is twofold:
                // 1. Emit less code.
                // 2. Try to speed things up by skipping stack checks.
                minimumParentCheckSize,
                // This allows us to elide stack checks in the Wasm -> JS call IC stub. Since these will
                // spill all arguments to the stack, we ensure that a stack check here covers the
                // stack that such a stub would use.
                Checked<uint32_t>(m_maxNumJSCallArguments) * sizeof(Register) + jsCallingConvention().headerSizeInBytes
            ));
            int32_t checkSize = wasmFrameSize.value();
            bool frameSizeNeedsOverflowCheck = checkSize >= static_cast<int32_t>(minimumParentCheckSize);
            bool needsOverflowCheck = frameSizeNeedsOverflowCheck;

            if (m_makesCalls) {
                needsOverflowCheck = true;
                checkSize = checkedSum<int32_t>(checkSize, extraFrameSize).value();
            } else if (m_makesTailCalls) {
                Checked<int32_t> tailCallCheckSize = std::max<Checked<int32_t>>(wasmTailCallFrameSize + extraFrameSize, 0);
                checkSize = frameSizeNeedsOverflowCheck ? std::max<Checked<int32_t>>(tailCallCheckSize, wasmFrameSize).value() : tailCallCheckSize.value();
                needsOverflowCheck = needsOverflowCheck || checkSize >= static_cast<int32_t>(minimumParentCheckSize);
            }

            bool needUnderflowCheck = static_cast<unsigned>(checkSize) > Options::reservedZoneSize();
            needsOverflowCheck = needsOverflowCheck || needUnderflowCheck;
            bool mayHaveExceptionHandlers = !m_hasExceptionHandlers || m_hasExceptionHandlers.value();

            // This allows leaf functions to not do stack checks if their frame size is within
            // certain limits since their caller would have already done the check.
            if (needsOverflowCheck) {
                if (mayHaveExceptionHandlers)
                    jit.store32(CCallHelpers::TrustedImm32(PatchpointExceptionHandle::s_invalidCallSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

                GPRReg scratch = wasmCallingConvention().prologueScratchGPRs[0];
                GPRReg scratch2 = wasmCallingConvention().prologueScratchGPRs[1];
                jit.addPtr(CCallHelpers::TrustedImm32(-checkSize), GPRInfo::callFrameRegister, scratch);
                MacroAssembler::JumpList overflow;
                if (UNLIKELY(needUnderflowCheck))
                    overflow.append(jit.branchPtr(CCallHelpers::Above, scratch, GPRInfo::callFrameRegister));
                jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfVM()), scratch2);
                overflow.append(jit.branchPtr(CCallHelpers::Below, scratch, CCallHelpers::Address(scratch2, VM::offsetOfSoftStackLimit())));
                jit.addLinkTask([overflow] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(overflow, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
                });
            }
        }
    });

    m_instanceValue = { Tmp(GPRInfo::wasmContextInstancePointer), Types::IPtr };

    append(EntrySwitch);
    m_mainEntrypointStart = m_code.addBlock();
    m_currentBlock = m_mainEntrypointStart;

    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(!m_locals.size());
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);
    m_locals.grow(wasmCallInfo.params.size());
    for (unsigned i = 0; i < wasmCallInfo.params.size(); ++i) {
        Type type = signature.as<FunctionSignature>()->argumentType(i);
        m_locals[i] = tmpForType(type);
        self().emitMove(wasmCallInfo.params[i], m_locals[i]);
    }

    emitEntryTierUpCheck();
}

template <typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::finalizeEntrypoints()
{
    unsigned numEntrypoints = Checked<unsigned>(1) + m_catchEntrypoints.size() + m_loopEntryVariableData.size();
    m_proc.setNumEntrypoints(numEntrypoints);
    m_code.setPrologueForEntrypoint(0, Ref<B3::Air::PrologueGenerator>(*m_prologueGenerator));
    for (unsigned i = 1 + m_catchEntrypoints.size(); i < numEntrypoints; ++i)
        m_code.setPrologueForEntrypoint(i, Ref<B3::Air::PrologueGenerator>(*m_prologueGenerator));

    if (m_catchEntrypoints.size()) {
        Ref<B3::Air::PrologueGenerator> catchPrologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([](CCallHelpers& jit, B3::Air::Code& code) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            emitCatchPrologueShared(code, jit);
        });

        for (unsigned i = 0; i < m_catchEntrypoints.size(); ++i)
            m_code.setPrologueForEntrypoint(1 + i, catchPrologueGenerator.copyRef());
    }

    BasicBlock::SuccessorList successors;
    successors.append(m_mainEntrypointStart);
    successors.appendVector(m_catchEntrypoints);

    ASSERT(!m_loopEntryVariableData.size() || !m_proc.usesSIMD());
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
            self().emitLoad(basePtr, offset, temps[i]);
        }

        append(Jump);
        entry->setSuccessors(loopBody);
    }

    RELEASE_ASSERT(numEntrypoints == successors.size());
    m_rootBlock->successors() = successors;
}

template<typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::restoreWebAssemblyGlobalState(const MemoryInformation& memory, ExpressionType instance, BasicBlock* block)
{
    restoreWasmContextInstance(block, instance);

    if (!!memory && Derived::supportsPinnedStateRegisters) {
        RegisterSetBuilder clobbers;
        clobbers.add(GPRInfo::wasmBaseMemoryPointer, IgnoreVectors);
        clobbers.add(GPRInfo::wasmBoundsCheckingSizeRegister, IgnoreVectors);
        clobbers.merge(RegisterSetBuilder::macroClobberedRegisters());

        auto* patchpoint = addPatchpoint(B3::Void);
        B3::Effects effects = B3::Effects::none();
        effects.writesPinned = true;
        effects.reads = B3::HeapRange::top();
        patchpoint->effects = effects;
        patchpoint->clobber(clobbers);
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->setGenerator([](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg scratch = params.gpScratch(0);
            jit.loadPairPtr(params[0].gpr(), CCallHelpers::TrustedImm32(Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
            jit.cageConditionallyAndUntag(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, scratch, /* validateAuth */ true, /* mayBeNull */ false);
        });

        emitPatchpoint(block, patchpoint, ExpressionType(), instance);
    }
}

template <typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::emitThrowException(CCallHelpers& jit, ExceptionType type)
{
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    auto jumpToExceptionStub = jit.jump();

    jit.addLinkTask([jumpToExceptionStub] (LinkBuffer& linkBuffer) {
        linkBuffer.link(jumpToExceptionStub, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator).code()));
    });
}

template <typename Derived, typename ExpressionType>
template <typename Function>
void AirIRGeneratorBase<Derived, ExpressionType>::forEachLiveValue(Function&& function)
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

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addLocal(Type type, uint32_t count) -> PartialResult
{
    size_t newSize = m_locals.size() + count;
    ASSERT(!(CheckedUint32(count) + m_locals.size()).hasOverflowed());
    ASSERT(newSize <= maxFunctionLocals);
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(newSize), "can't allocate memory for ", newSize, " locals");

    for (uint32_t i = 0; i < count; ++i) {
        auto local = self().tmpForType(type);
        m_locals.uncheckedAppend(local);
        self().emitZeroInitialize(local);
    }
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addBottom(BasicBlock* block, Type type) -> ExpressionType
{
    append(block, B3::Air::Oops);
    return addConstantZero(block, type);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addConstantZero(BasicBlock* block, Type type) -> ExpressionType
{
    return self().addConstantZero(block, type);
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addArguments(const TypeDefinition& signature) -> PartialResult
{
    RELEASE_ASSERT(m_locals.size() == signature.as<FunctionSignature>()->argumentCount()); // We handle arguments in the prologue
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addRefFunc(uint32_t index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    if (Options::useWebAssemblyTypedFunctionReferences()) {
        TypeIndex typeIndex = m_info.typeIndexFromFunctionIndexSpace(index);
        result = tmpForType(Type { TypeKind::Ref, typeIndex });
    } else
        result = tmpForType(Types::Funcref);
    emitCCall(&operationWasmRefFunc, result, instanceValue(), self().addConstant(Types::I32, index));

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addRefAsNonNull(ExpressionType reference, ExpressionType& result) -> PartialResult
{
    emitThrowOnNullReference(reference, ExceptionType::NullRefAsNonNull);
    result = self().tmpForType(Type { TypeKind::Ref, reference.type().index });
    append(Move, reference, result);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index);

    ASSERT(index.type().isI32());
    result = tmpForType(m_info.tables[tableIndex].wasmType());
    ASSERT(widthForType(toB3Type(result.type())) == Width64);

    emitCCall(&operationGetWasmTableElement, result, instanceValue(), self().addConstant(Types::I32, tableIndex), index);

    self().emitCheckI64Zero(result, [=, this](CCallHelpers &jit, const B3::StackmapGenerationParams &) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTableSet(unsigned tableIndex, ExpressionType index, ExpressionType value) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    ASSERT(index);
    ASSERT(index.type().isI32());
    ASSERT(value);

    auto shouldThrow = self().g32();
    emitCCall(&operationSetWasmTableElement, shouldThrow, instanceValue(), self().addConstant(Types::I32, tableIndex), index, value);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), shouldThrow, shouldThrow);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
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
        self().addConstant(Types::I32, elementIndex),
        self().addConstant(Types::I32, tableIndex),
        dstOffset, srcOffset, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addElemDrop(unsigned elementIndex) -> PartialResult
{
    emitCCall(&operationWasmElemDrop, ExpressionType(), instanceValue(), self().addConstant(Types::I32, elementIndex));
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTableSize(unsigned tableIndex, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    result = tmpForType(Types::I32);

    emitCCall(&operationGetWasmTableSize, result, instanceValue(), self().addConstant(Types::I32, tableIndex));

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTableGrow(unsigned tableIndex, ExpressionType fill, ExpressionType delta, ExpressionType& result) -> PartialResult
{
    ASSERT(fill);
    ASSERT(isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()));
    ASSERT(delta);
    ASSERT(delta.type().isI32());
    result = tmpForType(Types::I32);

    emitCCall(&operationWasmTableGrow, result, instanceValue(), self().addConstant(Types::I32, tableIndex), fill, delta);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTableFill(unsigned tableIndex, ExpressionType offset, ExpressionType fill, ExpressionType count) -> PartialResult
{
    ASSERT(fill);
    ASSERT(isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()));
    ASSERT(offset);
    ASSERT(offset.type().isI32());
    ASSERT(count);
    ASSERT(count.type().isI32());

    auto result = tmpForType(Types::I32);
    emitCCall(&operationWasmTableFill, result, instanceValue(), self().addConstant(Types::I32, tableIndex), offset, fill, count);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    ASSERT(dstOffset);
    ASSERT(dstOffset.type().isI32());

    ASSERT(srcOffset);
    ASSERT(srcOffset.type().isI32());

    ASSERT(length);
    ASSERT(length.type().isI32());

    auto result = self().tmpForType(Types::I32);
    emitCCall(
        &operationWasmTableCopy, result, instanceValue(),
        self().addConstant(Types::I32, dstTableIndex),
        self().addConstant(Types::I32, srcTableIndex),
        dstOffset, srcOffset, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTableAccess);
    });

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    auto local = m_locals[index];
    ASSERT(local);
    result = self().tmpForType(local.type());
    self().emitMove(local, result);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addUnreachable() -> PartialResult
{
    B3::PatchpointValue* unreachable = addPatchpoint(B3::Void);
    unreachable->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::Unreachable);
    });
    unreachable->effects.terminal = true;
    emitPatchpoint(unreachable, ExpressionType());
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCrash() -> PartialResult
{
    B3::PatchpointValue* unreachable = addPatchpoint(B3::Void);
    unreachable->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        jit.breakpoint();
    });
    unreachable->effects.terminal = true;
    emitPatchpoint(unreachable, Tmp());
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    emitCCall(&operationGrowMemory, result, instanceValue(), delta);
    restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_currentBlock);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    auto temp1 = self().gPtr();
    auto temp2 = self().gPtr();

    RELEASE_ASSERT(Arg::isValidAddrForm(Move, Instance::offsetOfMemory(), pointerWidth()));
    RELEASE_ASSERT(Arg::isValidAddrForm(Move, Memory::offsetOfHandle(), pointerWidth()));
    RELEASE_ASSERT(Arg::isValidAddrForm(Move, BufferMemoryHandle::offsetOfSize(), pointerWidth()));
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfMemory()), temp1);
    append(Move, Arg::addr(temp1, Memory::offsetOfHandle()), temp1);
    append(Move, Arg::addr(temp1, BufferMemoryHandle::offsetOfSize()), temp1);
    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    append(Move, Arg::imm(16), temp2);
    self().addShift(Types::I32, Derived::UrshiftPtr, temp1, temp2, result);
    append(Move32, result, result);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count) -> PartialResult
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

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count) -> PartialResult
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

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addMemoryInit(unsigned dataSegmentIndex, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length) -> PartialResult
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
        self().addConstant(Types::I32, dataSegmentIndex),
        dstAddress, srcAddress, length);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), result, result);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
    });

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addDataDrop(unsigned dataSegmentIndex) -> PartialResult
{
    emitCCall(&operationWasmDataDrop, ExpressionType(), instanceValue(), self().addConstant(Types::I32, dataSegmentIndex));
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::setLocal(uint32_t index, ExpressionType value) -> PartialResult
{
    auto local = m_locals[index];
    ASSERT(local);
    self().emitMove(value, local);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    result = tmpForType(type);

    int32_t offset = Instance::offsetOfGlobalPtr(m_numImportFunctions, m_info.tableCount(), index);

    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        self().emitLoad(instanceValue().tmp(), offset, result);
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        auto temp = self().gPtr();
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        self().emitLoad(instanceValue().tmp(), offset, temp);
        self().emitLoad(temp, 0, result);
        break;
    }

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;

    int32_t offset = Instance::offsetOfGlobalPtr(m_numImportFunctions, m_info.tableCount(), index);

    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
        self().emitStore(value, instanceValue().tmp(), offset);
        if (isRefType(type))
            emitWriteBarrierForJSWrapper();
        break;
    }
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        auto temp = self().gPtr();
        self().emitLoad(instanceValue().tmp(), offset, temp);
        self().emitStore(value, temp, 0);
        if (isRefType(type)) {
            auto cell = self().gPtr();
            auto vm = self().gPtr();
            auto cellState = self().g32();
            auto threshold = self().g32();

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
            emitPatchpoint(doFence, ExpressionType());

            append(Load8, Arg::addr(cell, JSCell::cellStateOffset()), cellState);
            append(Branch32, Arg::relCond(MacroAssembler::Above), cellState, Arg::imm(blackThreshold));
            m_currentBlock->setSuccessors(continuation, doSlowPath);
            m_currentBlock = doSlowPath;

            emitCCall(&operationWasmWriteBarrierSlowPath, ExpressionType(), cell, vm);
            append(Jump);
            m_currentBlock->setSuccessors(continuation);
            m_currentBlock = continuation;
        }
        break;
    }
    }
    return { };
}

template<typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::emitWriteBarrierForJSWrapper()
{
    auto cell = self().gPtr();
    append(Move, Arg::addr(instanceValue(), Instance::offsetOfOwner()), cell);
    emitWriteBarrier(cell, cell);
}

template<typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::emitWriteBarrier(ExpressionType cell, ExpressionType instanceCell)
{
    auto vm = self().gPtr();
    auto cellState = self().g32();
    auto threshold = self().g32();

    BasicBlock* fenceCheckPath = m_code.addBlock();
    BasicBlock* fencePath = m_code.addBlock();
    BasicBlock* doSlowPath = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    append(Move, Arg::addr(instanceCell, JSWebAssemblyInstance::offsetOfVM()), vm);
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

    emitCCall(&operationWasmWriteBarrierSlowPath, ExpressionType(), cell, vm);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);
    m_currentBlock = continuation;
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

#define OPCODE_FOR_WIDTH(opcode, width) (          \
    (width) == Width8 ? B3::Air::opcode##8 :   \
    (width) == Width16 ? B3::Air::opcode##16 : \
    (width) == Width32 ? B3::Air::opcode##32 : \
    B3::Air::opcode##64)
#define OPCODE_FOR_CANONICAL_WIDTH(opcode, width) ( \
    (width) == Width64 ? B3::Air::opcode##64 : B3::Air::opcode##32)

inline Width accessWidth(ExtAtomicOpType op)
{
    return static_cast<Width>(memoryLog2Alignment(op));
}

inline uint32_t sizeOfAtomicOpMemoryAccess(ExtAtomicOpType op)
{
    return bytesForWidth(accessWidth(op));
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType op, ExpressionType pointer, uint32_t uoffset) -> ExpressionType
{
    ASSERT(pointer);
    uint32_t offset = fixupPointerPlusOffset(pointer, uoffset);
    if (Arg::isValidAddrForm(Derived::LeaPtr, offset, widthForBytes(sizeOfAtomicOpMemoryAccess(op)))) {
        if (!offset)
            return pointer;
        auto newPtr = self().gPtr();
        append(Derived::LeaPtr, Arg::addr(pointer, offset), newPtr);
        return newPtr;
    }
    auto newPtr = self().g64();
    append(Move, Arg::bigImm(offset), newPtr);
    append(Derived::AddPtr, pointer, newPtr);
    return newPtr;
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::emitAtomicLoadOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, uint32_t uoffset) -> ExpressionType
{
    auto newPtr = self().fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);

    if (accessWidth(op) != Width8) {
        emitCheck([&] {
            auto const mask = isX86() || is32Bit() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1);
            return Inst(Derived::BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, mask);
        }, [=, this](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    std::optional<B3::Air::Opcode> opcode;
    if (isX86() || isARM64_LSE())
        opcode = OPCODE_FOR_WIDTH(AtomicXchgAdd, accessWidth(op));
    B3::Air::Opcode nonAtomicOpcode = OPCODE_FOR_CANONICAL_WIDTH(Add, accessWidth(op));

    auto result = valueType.isI64() ? self().g64() : self().g32();

    if (opcode) {
        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind(), Arg::Tmp)) {
            append(Move, Arg::imm(0), result);
            appendEffectful(opcode.value(), result, addrArg, result);
            self().sanitizeAtomicResult(op, result);
            return result;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            append(Move, Arg::imm(0), result);
            appendEffectful(opcode.value(), result, addrArg);
            self().sanitizeAtomicResult(op, result);
            return result;
        }
    }

    self().appendGeneralAtomic(op, nonAtomicOpcode, B3::Commutative, Arg::imm(0), addrArg, result);
    self().sanitizeAtomicResult(op, result);
    return result;
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::atomicLoad(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* patch = addPatchpoint(B3::Void);
        patch->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(patch, ExpressionType());

        // We won't reach here, so we just pick a random reg.
        switch (valueType.kind) {
        case TypeKind::I32:
            result = self().g32();
            break;
        case TypeKind::I64:
            result = self().g64();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = emitAtomicLoadOp(op, valueType, self().emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), offset);

    return { };
}

template<typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::emitAtomicStoreOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    auto newPtr = self().fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);

    if (accessWidth(op) != Width8) {
        emitCheck([&] {
            return Inst(Derived::BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() || is32Bit() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    std::optional<B3::Air::Opcode> opcode;
    if (isX86() || isARM64_LSE())
        opcode = OPCODE_FOR_WIDTH(AtomicXchg, accessWidth(op));
    B3::Air::Opcode nonAtomicOpcode = B3::Air::Nop;

    if (opcode) {
        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind(), Arg::Tmp)) {
            auto result = valueType.isI64() ? self().g64() : self().g32();
            appendEffectful(opcode.value(), value, addrArg, result);
            return;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            auto result = valueType.isI64() ? self().g64() : self().g32();
            append(Move, value, result);
            appendEffectful(opcode.value(), result, addrArg);
            return;
        }
    }

    self().appendGeneralAtomic(op, nonAtomicOpcode, B3::Commutative, value, addrArg, valueType.isI64() ? self().g64() : self().g32());
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::atomicStore(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* throwException = addPatchpoint(B3::Void);
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(throwException, ExpressionType());
    } else
        emitAtomicStoreOp(op, valueType, self().emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), value, offset);

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset) -> ExpressionType
{
    auto newPtr = self().fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);

    if (accessWidth(op) != Width8) {
        emitCheck([&] {
            return Inst(Derived::BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() || is32Bit() ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
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
        if (isX86() || isARM64_LSE())
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
        if (isX86() || isARM64_LSE()) {
            ExpressionType newValue;
            if (valueType.isI64()) {
                newValue = self().g64();
                append(Move, value, newValue);
                append(Neg64, newValue);
            } else {
                newValue = self().g32();
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
        if (isARM64_LSE()) {
            ExpressionType newValue;
            if (valueType.isI64()) {
                newValue = self().g64();
                append(Not64, value, newValue);
            } else {
                newValue = self().g32();
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
        if (isARM64_LSE())
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
        if (isARM64_LSE())
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
        if (isX86() || isARM64_LSE())
            opcode = OPCODE_FOR_WIDTH(AtomicXchg, accessWidth(op));
        nonAtomicOpcode = B3::Air::Nop;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    auto result = valueType.isI64() ? self().g64() : self().g32();

    if (opcode) {
        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind(), Arg::Tmp)) {
            appendEffectful(opcode.value(), value, addrArg, result);
            self().sanitizeAtomicResult(op, result);
            return result;
        }

        if (isValidForm(opcode.value(), Arg::Tmp, addrArg.kind())) {
            append(Move, value, result);
            appendEffectful(opcode.value(), result, addrArg);
            self().sanitizeAtomicResult(op, result);
            return result;
        }
    }

    self().appendGeneralAtomic(op, nonAtomicOpcode, commutativity, value, addrArg, result);
    self().sanitizeAtomicResult(op, result);
    return result;
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::atomicBinaryRMW(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer.tmp().isGP());

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        auto* patch = addPatchpoint(B3::Void);
        patch->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
        emitPatchpoint(patch, ExpressionType());

        switch (valueType.kind) {
        case TypeKind::I32:
            result = self().g32();
            break;
        case TypeKind::I64:
            result = self().g64();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = self().emitAtomicBinaryRMWOp(op, valueType, self().emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), value, offset);

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::emitAtomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t uoffset) -> ExpressionType
{
    auto newPtr = self().fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);
    Arg addrArg = isX86() ? Arg::addr(newPtr) : Arg::simpleAddr(newPtr);
    Width valueWidth = widthForType(toB3Type(valueType));
    Width accessWidth = Wasm::accessWidth(op);

    if (accessWidth != Width8) {
        emitCheck([&] {
            return Inst(Derived::BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::NonZero), newPtr, isX86() || is32Bit()  ? Arg::bitImm(sizeOfAtomicOpMemoryAccess(op) - 1) : Arg::bitImm64(sizeOfAtomicOpMemoryAccess(op) - 1));
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    auto result = expected.type().isI64() ? self().g64() : self().g32();

    if (valueWidth == accessWidth) {
        self().appendStrongCAS(op, expected, value, addrArg, result);
        self().sanitizeAtomicResult(op, result);
        return result;
    }

    BasicBlock* failureCase = m_code.addBlock();
    BasicBlock* successCase = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    auto truncatedExpected = expected.type().isI64() ? self().g64() : self().g32();
    self().sanitizeAtomicResult(op, expected, truncatedExpected);

    auto const appendBranchOnUnexpected = [&] {
        append(OPCODE_FOR_CANONICAL_WIDTH(Branch, valueWidth), Arg::relCond(MacroAssembler::NotEqual), expected, truncatedExpected);
    };

    if constexpr (is32Bit()) {
        if (valueWidth == Width64) {
            append(Branch32, Arg::relCond(MacroAssembler::NotEqual), expected.lo(), truncatedExpected.lo());
            auto* checkHiBlock = m_code.addBlock();
            m_currentBlock->setSuccessors(B3::Air::FrequentedBlock(failureCase), checkHiBlock);
            m_currentBlock = checkHiBlock;
            append(Branch32, Arg::relCond(MacroAssembler::NotEqual), expected.hi(), truncatedExpected.hi());
        } else
            appendBranchOnUnexpected();
    } else
        appendBranchOnUnexpected();

    m_currentBlock->setSuccessors(B3::Air::FrequentedBlock(failureCase, B3::FrequencyClass::Rare), successCase);

    m_currentBlock = successCase;
    self().appendStrongCAS(op, expected, value, addrArg, result);
    append(Jump);
    m_currentBlock->setSuccessors(continuation);

    m_currentBlock = failureCase;
    ([&] {
        std::optional<B3::Air::Opcode> opcode;
        if (isX86() || isARM64_LSE())
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
        self().appendGeneralAtomic(op, nonAtomicOpcode, B3::Commutative, Arg::imm(0), addrArg, result);
    })();
    append(Jump);
    m_currentBlock->setSuccessors(continuation);

    m_currentBlock = continuation;
    self().sanitizeAtomicResult(op, result);
    return result;
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
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
        emitPatchpoint(patch, ExpressionType());

        // We won't reach here, so we just pick a random reg.
        switch (valueType.kind) {
        case TypeKind::I32:
            result = self().g32();
            break;
        case TypeKind::I64:
            result = self().g64();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    } else
        result = self().emitAtomicCompareExchange(op, valueType, self().emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), expected, value, offset);

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::atomicWait(ExtAtomicOpType op, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = self().g32();

    if (op == ExtAtomicOpType::MemoryAtomicWait32)
        emitCCall(&operationMemoryAtomicWait32, result, instanceValue(), pointer, self().addConstant(Types::I32, offset), value, timeout);
    else
        emitCCall(&operationMemoryAtomicWait64, result, instanceValue(), pointer, self().addConstant(Types::I32, offset), value, timeout);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::LessThan), result, Arg::imm(0));
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
    });

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::atomicNotify(ExtAtomicOpType, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = self().g32();

    emitCCall(&operationMemoryAtomicNotify, result, instanceValue(), pointer, self().addConstant(Types::I32, offset), count);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::LessThan), result, Arg::imm(0));
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsMemoryAccess);
    });

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::atomicFence(ExtAtomicOpType, uint8_t) -> PartialResult
{
    append(MemoryFence);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::lookupTruncationRange(FloatingPointTruncationKind kind) -> FloatingPointRange
{
    ExpressionType min;
    ExpressionType max;
    bool closedLowerEndpoint = false;

    switch (kind) {
    case FloatingPointTruncationKind::F32ToI32:
        closedLowerEndpoint = true;
        max = self().addConstant(Types::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
        min = self().addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));
        break;
    case FloatingPointTruncationKind::F32ToU32:
        max = self().addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
        min = self().addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
        break;
    case FloatingPointTruncationKind::F64ToI32:
        max = self().addConstant(Types::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
        min = self().addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));
        break;
    case FloatingPointTruncationKind::F64ToU32:
        max = self().addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
        min = self().addConstant(Types::F64, bitwise_cast<uint64_t>(-1.0));
        break;
    case FloatingPointTruncationKind::F32ToI64:
        closedLowerEndpoint = true;
        max = self().addConstant(Types::F32, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
        min = self().addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));
        break;
    case FloatingPointTruncationKind::F32ToU64:
        max = self().addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
        min = self().addConstant(Types::F32, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
        break;
    case FloatingPointTruncationKind::F64ToI64:
        closedLowerEndpoint = true;
        max = self().addConstant(Types::F64, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
        min = self().addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));
        break;
    case FloatingPointTruncationKind::F64ToU64:
        max = self().addConstant(Types::F64, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
        min = self().addConstant(Types::F64, bitwise_cast<uint64_t>(-1.0));
        break;
    }

    return FloatingPointRange { min, max, closedLowerEndpoint };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::truncSaturated(Ext1OpType op, ExpressionType arg, ExpressionType& result, Type returnType, Type operandType) -> PartialResult
{
    auto truncationKind = ([&] {
        switch (op) {
        case Ext1OpType::I32TruncSatF32S:
            return FloatingPointTruncationKind::F32ToI32;
        case Ext1OpType::I32TruncSatF32U:
            return FloatingPointTruncationKind::F32ToU32;
        case Ext1OpType::I32TruncSatF64S:
            return FloatingPointTruncationKind::F64ToI32;
        case Ext1OpType::I32TruncSatF64U:
            return FloatingPointTruncationKind::F64ToU32;
        case Ext1OpType::I64TruncSatF32S:
            return FloatingPointTruncationKind::F32ToI64;
        case Ext1OpType::I64TruncSatF32U:
            return FloatingPointTruncationKind::F32ToU64;
        case Ext1OpType::I64TruncSatF64S:
            return FloatingPointTruncationKind::F64ToI64;
        case Ext1OpType::I64TruncSatF64U:
            return FloatingPointTruncationKind::F64ToU64;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    })();

    auto const range = lookupTruncationRange(truncationKind);
    auto const minFloat = range.lowerEndpoint;
    auto const maxFloat = range.upperEndpoint;
    // We deliberately ignore `range.closedLowerEndpoint` here because this is a saturating
    // operation so we literally can't treat the endpoints wrong (everything in
    // the closed ranges outside the valid range results in the corresponding
    // endpoint's value)

    uint64_t minResult = 0;
    uint64_t maxResult = 0;
    switch (truncationKind) {
    case FloatingPointTruncationKind::F32ToI32:
    case FloatingPointTruncationKind::F64ToI32:
        maxResult = bitwise_cast<uint32_t>(INT32_MAX);
        minResult = bitwise_cast<uint32_t>(INT32_MIN);
        break;
    case FloatingPointTruncationKind::F32ToU32:
    case FloatingPointTruncationKind::F64ToU32:
        maxResult = bitwise_cast<uint32_t>(UINT32_MAX);
        minResult = bitwise_cast<uint32_t>(0U);
        break;
    case FloatingPointTruncationKind::F32ToI64:
    case FloatingPointTruncationKind::F64ToI64:
        maxResult = bitwise_cast<uint64_t>(INT64_MAX);
        minResult = bitwise_cast<uint64_t>(INT64_MIN);
        break;
    case FloatingPointTruncationKind::F32ToU64:
    case FloatingPointTruncationKind::F64ToU64:
        maxResult = bitwise_cast<uint64_t>(UINT64_MAX);
        minResult = bitwise_cast<uint64_t>(0ULL);
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

    // As an optimization, if the min result is 0; we can unconditionally return
    // that if the above-minimum-range check fails; otherwise, we need to check
    // for NaN since it also will fail the above-minimum-range-check
    if (!minResult) {
        self().emitMaterializeConstant(minCase, result.type(), minResult, result);
        append(minCase, Jump);
        minCase->setSuccessors(continuation);
    } else {
        BasicBlock* minMaterializeCase = m_code.addBlock();
        BasicBlock* nanCase = m_code.addBlock();
        append(minCase, branchOp, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg, arg);
        minCase->setSuccessors(minMaterializeCase, nanCase);

        self().emitMaterializeConstant(minMaterializeCase, result.type(), minResult, result);
        append(minMaterializeCase, Jump);
        minMaterializeCase->setSuccessors(continuation);

        self().emitMaterializeConstant(nanCase, result.type(), 0, result);
        append(nanCase, Jump);
        nanCase->setSuccessors(continuation);
    }

    self().emitMaterializeConstant(maxCase, result.type(), maxResult, result);
    append(maxCase, Jump);
    maxCase->setSuccessors(continuation);

    m_currentBlock = inBoundsCase;
    self().addUncheckedFloatingPointTruncation(truncationKind, arg, result);
    append(inBoundsCase, Jump);
    inBoundsCase->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCheckedFloatingPointTruncation(FloatingPointTruncationKind kind, ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto const range = lookupTruncationRange(kind);

    auto compareOpcode = ([&] {
        switch (kind) {
        case FloatingPointTruncationKind::F32ToI32:
        case FloatingPointTruncationKind::F32ToU32:
        case FloatingPointTruncationKind::F32ToI64:
        case FloatingPointTruncationKind::F32ToU64:
            return CompareFloat;
        case FloatingPointTruncationKind::F64ToI32:
        case FloatingPointTruncationKind::F64ToU32:
        case FloatingPointTruncationKind::F64ToI64:
        case FloatingPointTruncationKind::F64ToU64:
            return CompareDouble;
        }
        RELEASE_ASSERT_NOT_REACHED();
    })();

    auto temp1 = self().g32();
    auto temp2 = self().g32();

    auto const lowerCompareCondition = range.closedLowerEndpoint ?
        MacroAssembler::DoubleLessThanOrUnordered : MacroAssembler::DoubleLessThanOrEqualOrUnordered;
    auto const upperCompareCondition = MacroAssembler::DoubleGreaterThanOrEqualOrUnordered;

    append(compareOpcode, Arg::doubleCond(lowerCompareCondition), arg, range.lowerEndpoint, temp1);
    append(compareOpcode, Arg::doubleCond(upperCompareCondition), arg, range.upperEndpoint, temp2);
    append(Or32, temp1, temp2);

    emitCheck([&] {
        return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), temp2, temp2);
    }, [=, this](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsTrunc);
    });

    switch (kind) {
    case FloatingPointTruncationKind::F32ToI32:
    case FloatingPointTruncationKind::F32ToU32:
    case FloatingPointTruncationKind::F64ToI32:
    case FloatingPointTruncationKind::F64ToU32:
        result = self().g32();
        break;
    case FloatingPointTruncationKind::F32ToI64:
    case FloatingPointTruncationKind::F32ToU64:
    case FloatingPointTruncationKind::F64ToI64:
    case FloatingPointTruncationKind::F64ToU64:
        result = self().g64();
        break;
    }

    self().addUncheckedFloatingPointTruncation(kind, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType value, ExpressionType& result) -> PartialResult
{
    const Wasm::TypeDefinition& arraySignature = m_info.typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());

    ExpressionType tmpForValue;
    self().emitCoerceToI64(value, tmpForValue);
    result = self().tmpForType(Wasm::Type { Wasm::TypeKind::Ref, Wasm::TypeInformation::get(arraySignature) });
    // FIXME: Emit this inline.
    // https://bugs.webkit.org/show_bug.cgi?id=245405
    emitCCall(&operationWasmArrayNew, result, instanceValue(), self().addConstant(Types::I32, typeIndex), size, tmpForValue);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addArrayNewDefault(uint32_t typeIndex, ExpressionType size, ExpressionType& result) -> PartialResult
{
    const Wasm::TypeDefinition& arraySignature = m_info.typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());
    const StorageType& elementType = arraySignature.as<ArrayType>()->elementType().type;

    ExpressionType tmpForValue;
    if (Wasm::isRefType(elementType))
        tmpForValue = self().addConstant(elementType.as<Type>(), JSValue::encode(jsNull()));
    else {
        tmpForValue = self().g64();
        self().emitZeroInitialize(tmpForValue);
    }

    result = self().tmpForType(Wasm::Type { Wasm::TypeKind::Ref, Wasm::TypeInformation::get(arraySignature) });
    // FIXME: Emit this inline.
    // https://bugs.webkit.org/show_bug.cgi?id=245405
    emitCCall(&operationWasmArrayNew, result, instanceValue(), self().addConstant(Types::I32, typeIndex), size, tmpForValue);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addArrayGet(ExtGCOpType arrayGetKind, uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType& result) -> PartialResult
{
    ASSERT(arrayGetKind == ExtGCOpType::ArrayGet || arrayGetKind == ExtGCOpType::ArrayGetS || arrayGetKind == ExtGCOpType::ArrayGetU);

    const Wasm::TypeDefinition& arraySignature = m_info.typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::StorageType elementType = arraySignature.as<ArrayType>()->elementType().type;
    Wasm::Type resultType = elementType.unpacked();

    // Ensure arrayref is non-null.
    emitThrowOnNullReference(arrayref, ExceptionType::NullArrayGet);

    // Check array bounds.
    auto arraySize = self().g32();
    self().emitLoad(self().extractJSValuePointer(arrayref), JSWebAssemblyArray::offsetOfSize(), arraySize);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), index, arraySize);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsArrayGet);
    });

    auto getValue = self().g64();
    // FIXME: Emit this inline.
    // https://bugs.webkit.org/show_bug.cgi?id=245405
    emitCCall(&operationWasmArrayGet, getValue, instanceValue(), self().addConstant(Types::I32, typeIndex), arrayref, index);

    self().emitCoerceFromI64(resultType, getValue, result);

    if (elementType.is<PackedType>()) {
        switch (arrayGetKind) {
        case ExtGCOpType::ArrayGetU:
            break;
        case ExtGCOpType::ArrayGetS: {
            size_t elementSize = elementType.as<PackedType>() == PackedType::I8 ? sizeof(uint8_t) : sizeof(uint16_t);
            uint8_t bitShift = (sizeof(uint32_t) - elementSize) * 8;
            auto tmpForShift = self().g32();
            append(Move, Arg::imm(bitShift), tmpForShift);
            self().addShift(Types::I32, Lshift32, result, tmpForShift, result);
            self().addShift(Types::I32, Rshift32, result, tmpForShift, result);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
    }

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addArraySet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType value) -> PartialResult
{
    // Ensure arrayref is non-null.
    emitThrowOnNullReference(arrayref, ExceptionType::NullArraySet);

    // Check array bounds.
    auto arraySize = self().g32();
    self().emitLoad(self().extractJSValuePointer(arrayref), JSWebAssemblyArray::offsetOfSize(), arraySize);
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), index, arraySize);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsArraySet);
    });

    ExpressionType tmpForValue;
    self().emitCoerceToI64(value, tmpForValue);
    emitCCall(&operationWasmArraySet, ExpressionType(), instanceValue(), self().addConstant(Types::I32, typeIndex), arrayref, index, tmpForValue);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addArrayLen(ExpressionType arrayref, ExpressionType& result) -> PartialResult
{
    // Ensure arrayref is non-null.
    emitThrowOnNullReference(arrayref, ExceptionType::NullArrayLen);

    result = self().g32();
    self().emitLoad(self().extractJSValuePointer(arrayref), JSWebAssemblyArray::offsetOfSize(), result);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addStructNew(uint32_t typeIndex, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    ASSERT(typeIndex < m_info.typeCount());
    result = self().tmpForType(Type { TypeKind::Ref, m_info.typeSignatures[typeIndex]->index() });
    // FIXME: inline the allocation.
    // https://bugs.webkit.org/show_bug.cgi?id=244388
    self().emitCCall(&operationWasmStructNewEmpty, result, instanceValue(), self().addConstant(Types::I32, typeIndex));

    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    for (unsigned i = 0; i < args.size(); ++i) {
        auto status = self().addStructSet(result, structType, i, args[i]);
        if (!status)
            return status;
    }
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addStructNewDefault(uint32_t typeIndex, ExpressionType& result) -> PartialResult
{
    ASSERT(typeIndex < m_info.typeCount());
    result = self().tmpForType(Type { TypeKind::Ref, m_info.typeSignatures[typeIndex]->index() });
    // FIXME: inline the allocation.
    // https://bugs.webkit.org/show_bug.cgi?id=244388
    self().emitCCall(&operationWasmStructNewEmpty, result, instanceValue(), self().addConstant(Types::I32, typeIndex));

    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    for (StructFieldCount i = 0; i < structType.fieldCount(); ++i) {
        ExpressionType tmpForValue;
        if (Wasm::isRefType(structType.field(i).type))
            tmpForValue = self().addConstant(structType.field(i).type.template as<Type>(), JSValue::encode(jsNull()));
        else {
            tmpForValue = self().g64();
            self().emitZeroInitialize(tmpForValue);
        }

        auto status = self().addStructSet(result, structType, i, tmpForValue);
        if (!status)
            return status;
    }

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addStructGet(ExpressionType structReference, const StructType& structType, uint32_t fieldIndex, ExpressionType& result) -> PartialResult
{
    emitThrowOnNullReference(structReference, ExceptionType::NullStructGet);

    auto payload = self().gPtr();
    auto structBase = self().extractJSValuePointer(structReference);
    self().emitLoad(structBase, JSWebAssemblyStruct::offsetOfPayload(), payload);

    uint32_t fieldOffset = fixupPointerPlusOffset(payload, *structType.getFieldOffset(fieldIndex));
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=246981
    ASSERT(structType.field(fieldIndex).type.is<Type>());
    Type fieldType = structType.field(fieldIndex).type.as<Type>();
    result = tmpForType(fieldType);
    self().emitLoad(payload, fieldOffset, result);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addStructSet(ExpressionType structReference, const StructType& structType, uint32_t fieldIndex, ExpressionType value) -> PartialResult
{
    emitThrowOnNullReference(structReference, ExceptionType::NullStructSet);

    auto payload = self().gPtr();
    auto structBase = self().extractJSValuePointer(structReference);
    self().emitLoad(structBase, JSWebAssemblyStruct::offsetOfPayload(), payload);

    uint32_t fieldOffset = fixupPointerPlusOffset(payload, *structType.getFieldOffset(fieldIndex));
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=246981
    ASSERT(structType.field(fieldIndex).type.is<Type>());
    Type fieldType = structType.field(fieldIndex).type.as<Type>();

    if (isRefType(fieldType)) {
        auto instanceCell = self().gPtr();
        append(Move, Arg::addr(instanceValue(), Instance::offsetOfOwner()), instanceCell);
        emitWriteBarrier(ExpressionType(structBase, Types::I32), instanceCell);
    }
    self().emitStore(value, payload, fieldOffset);
    return { };
}


template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    ASSERT(nonZero.type() == zero.type());
    result = self().tmpForType(nonZero.type());

    BasicBlock* isZero = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    self().emitMove(nonZero, result);
    append(BranchTest32, Arg::resCond(MacroAssembler::Zero), condition, condition);
    m_currentBlock->setSuccessors(isZero, continuation);

    m_currentBlock = isZero;
    self().emitMove(zero, result);
    append(Jump);
    isZero->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template <typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::emitEntryTierUpCheck()
{
    if (!m_tierUp)
        return;

    auto countdownPtr = self().gPtr();
    append(Move, Arg::bigImm(bitwise_cast<uintptr_t>(&m_tierUp->m_counter)), countdownPtr);

    auto* patch = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    patch->effects = effects;
    patch->clobber(RegisterSetBuilder::macroClobberedRegisters());

    patch->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);

        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::functionEntryIncrement()), CCallHelpers::Address(params[0].gpr()));
        CCallHelpers::Label tierUpResume = jit.label();

        params.addLatePath([=, this] (CCallHelpers& jit) {
            tierUp.link(&jit);

            const unsigned extraPaddingBytes = 0;
            RegisterSet registersToSpill = { };
            registersToSpill.add(GPRInfo::argumentGPR1, IgnoreVectors);
            unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

            jit.move(MacroAssembler::TrustedImm32(m_functionIndex), GPRInfo::argumentGPR1);
            MacroAssembler::Call call = jit.nearCall();

            ASSERT(!registersToSpill.numberOfSetFPRs());
            ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToSpill, { }, numberOfStackBytesUsedForRegisterPreservation, extraPaddingBytes);
            jit.jump(tierUpResume);

            bool isSIMD = m_proc.usesSIMD();
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                MacroAssembler::repatchNearCall(linkBuffer.locationOfNearCall<NoPtrTag>(call), CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(triggerOMGEntryTierUpThunkGenerator(isSIMD)).code()));
            });
        });
    });

    self().emitPatchpoint(patch, Tmp(), countdownPtr);
}

template <typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::emitLoopTierUpCheck(uint32_t loopIndex, const Vector<ExpressionType>& liveValues)
{
    uint32_t outerLoopIndex = this->outerLoopIndex();
    m_outerLoops.append(loopIndex);

    if (!m_tierUp)
        return;

    ASSERT(m_tierUp->osrEntryTriggers().size() == loopIndex);
    m_tierUp->osrEntryTriggers().append(TierUpCount::TriggerReason::DontTrigger);
    m_tierUp->outerLoops().append(outerLoopIndex);

    auto countdownPtr = self().gPtr();
    append(Move, Arg::bigImm(bitwise_cast<uintptr_t>(&m_tierUp->m_counter)), countdownPtr);

    auto* patch = addPatchpoint(B3::Void);
    B3::Effects effects = B3::Effects::none();
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    effects.exitsSideways = true;
    patch->effects = effects;

    patch->clobber(RegisterSetBuilder::macroClobberedRegisters());
    RegisterSetBuilder clobberLate;
    clobberLate.add(GPRInfo::argumentGPR0, IgnoreVectors);
    patch->clobberLate(clobberLate);

    Vector<ConstrainedTmp> patchArgs;
    patchArgs.append(countdownPtr);
    for (const auto& tmp : liveValues) {
        dataLogLnIf(WasmAirIRGeneratorInternal::verbose, "OSR loop patch param before allocation: ", tmp);
        patchArgs.append(ConstrainedTmp(tmp, B3::ValueRep::ColdAny));
    }

    TierUpCount::TriggerReason* forceEntryTrigger = &(m_tierUp->osrEntryTriggers().last());
    static_assert(!static_cast<uint8_t>(TierUpCount::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
    static_assert(sizeof(TierUpCount::TriggerReason) == 1, "branchTest8 assumes this size");
    SavedFPWidth savedFPWidth = m_proc.usesSIMD() ? SavedFPWidth::SaveVectors : SavedFPWidth::DontSaveVectors;
    patch->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        CCallHelpers::Jump forceOSREntry = jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::AbsoluteAddress(forceEntryTrigger));
        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::loopIncrement()), CCallHelpers::Address(params[0].gpr()));
        MacroAssembler::Label tierUpResume = jit.label();

        // First argument is the countdown location.
        ASSERT(params.value()->numChildren() >= 1);
        StackMap values(params.value()->numChildren() - 1);
        for (unsigned i = 1; i < params.value()->numChildren(); ++i) {
            dataLogLnIf(WasmAirIRGeneratorInternal::verbose, "OSR loop patchpoint param[", i, "] = ", params[i]);
            values[i - 1] = OSREntryValue(params[i], params.value()->child(i)->type());
        }

        OSREntryData& osrEntryData = m_tierUp->addOSREntryData(m_functionIndex, loopIndex, WTFMove(values));
        OSREntryData* osrEntryDataPtr = &osrEntryData;

        params.addLatePath([=] (CCallHelpers& jit) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            forceOSREntry.link(&jit);
            tierUp.link(&jit);

            jit.probe(tagCFunction<JITProbePtrTag>(operationWasmTriggerOSREntryNow), osrEntryDataPtr, savedFPWidth);
            jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::argumentGPR0).linkTo(tierUpResume, &jit);
            jit.farJump(GPRInfo::argumentGPR1, WasmEntryPtrTag);
        });
    });

    emitPatchpoint(m_currentBlock, patch, ResultList { }, WTFMove(patchArgs));
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTopLevel(BlockSignature signature) -> ControlData
{
    return ControlData(B3::Origin(), signature, tmpsForSignature(signature), BlockType::TopLevel, m_code.addBlock());
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex) -> PartialResult
{
    RELEASE_ASSERT(loopIndex == m_loopEntryVariableData.size() || (m_proc.usesSIMD() && !m_loopEntryVariableData.size()));

    BasicBlock* body = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();

    splitStack(signature, enclosingStack, newStack);

    Vector<ExpressionType> liveValues;
    forEachLiveValue([&] (auto tmp) {
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

    if (!m_proc.usesSIMD())
        m_loopEntryVariableData.append(std::pair<BasicBlock*, Vector<ExpressionType>>(body, WTFMove(liveValues)));

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> PartialResult
{
    splitStack(signature, enclosingStack, newStack);
    newBlock = ControlData(origin(), signature, tmpsForSignature(signature), BlockType::Block, m_code.addBlock());
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    BasicBlock* taken = m_code.addBlock();
    BasicBlock* notTaken = m_code.addBlock();
    BasicBlock* continuation = m_code.addBlock();
    B3::FrequencyClass takenFrequency = B3::FrequencyClass::Normal;
    B3::FrequencyClass notTakenFrequency= B3::FrequencyClass::Normal;

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

    // Wasm bools are i32.
    append(BranchTest32, Arg::resCond(MacroAssembler::NonZero), condition, condition);
    m_currentBlock->setSuccessors(FrequentedBlock(taken, takenFrequency), FrequentedBlock(notTaken, notTakenFrequency));

    m_currentBlock = taken;
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(origin(), signature, tmpsForSignature(signature), BlockType::If, continuation, notTaken);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addElse(ControlData& data, const Stack& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);
    return addElseToUnreachable(data);
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.blockType() == BlockType::If);
    m_currentBlock = data.special;
    data.convertIfToBlock();
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addTry(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    ++m_tryCatchDepth;

    BasicBlock* continuation = m_code.addBlock();
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(origin(), signature, tmpsForSignature(signature), BlockType::Try, continuation, ++m_callSiteIndex, m_tryCatchDepth);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCatch(unsigned exceptionIndex, const TypeDefinition& signature, Stack& currentStack, ControlType& data, ResultList& results) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);
    return addCatchToUnreachable(exceptionIndex, signature, data, results);
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCatchAll(Stack& currentStack, ControlType& data) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);
    return addCatchAllToUnreachable(data);
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& signature, ControlType& data, ResultList& results) -> PartialResult
{
    Tmp buffer = self().emitCatchImpl(CatchKind::Catch, data, exceptionIndex);
    for (unsigned i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        Type type = signature.as<FunctionSignature>()->argumentType(i);
        auto tmp = tmpForType(type);
        if (type.isV128())
            append(MoveZeroToVector, tmp);
        else
            self().emitLoad(buffer, i * sizeof(uint64_t), tmp);
        results.append(tmp);
    }
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCatchAllToUnreachable(ControlType& data) -> PartialResult
{
    self().emitCatchImpl(CatchKind::CatchAll, data);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addDelegate(ControlType& target, ControlType& data) -> PartialResult
{
    return addDelegateToUnreachable(target, data);
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addDelegateToUnreachable(ControlType& target, ControlType& data) -> PartialResult
{
    unsigned targetDepth = 0;
    if (ControlType::isTry(target))
        targetDepth = target.tryDepth();

    m_exceptionHandlers.append({ HandlerType::Delegate, data.tryStart(), ++m_callSiteIndex, 0, m_tryCatchDepth, targetDepth });
    return { };
}

// NOTE: All branches in Wasm are on 32-bit ints

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addBranch(ControlData& data, ExpressionType condition, const Stack& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data.results);

    BasicBlock* target = data.targetBlockForBranch();
    B3::FrequencyClass targetFrequency = B3::FrequencyClass::Normal;
    B3::FrequencyClass continuationFrequency = B3::FrequencyClass::Normal;

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

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const Stack& expressionStack) -> PartialResult
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
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());

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

    emitPatchpoint(patchpoint, ExpressionType(), condition);

    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::endBlock(ControlEntry& entry, Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    if (data.blockType() != BlockType::Loop)
        unifyValuesWithBlock(expressionStack, data.results);
    append(Jump);
    m_currentBlock->setSuccessors(data.continuation);

    return addEndToUnreachable(entry, expressionStack);
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addEndToUnreachable(ControlEntry& entry, const Stack& expressionStack) -> PartialResult
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
        for (unsigned i = 0; i < data.signature()->template as<FunctionSignature>()->returnCount(); ++i) {
            if (i < expressionStack.size())
                entry.enclosedExpressionStack.append(expressionStack[i]);
            else {
                Type type = data.signature()->template as<FunctionSignature>()->returnType(i);
                entry.enclosedExpressionStack.constructAndAppend(type, addBottom(m_currentBlock, type));
            }
        }
    } else {
        for (unsigned i = 0; i < data.signature()->template as<FunctionSignature>()->returnCount(); ++i)
            entry.enclosedExpressionStack.constructAndAppend(data.signature()->template as<FunctionSignature>()->returnType(i), data.results[i]);
    }

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.blockType() == BlockType::TopLevel)
        return self().addReturn(data, entry.enclosedExpressionStack);

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCall(uint32_t functionIndex, const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results, CallType callType) -> PartialResult
{
    bool isTailCall = callType == CallType::TailCall;
    ASSERT(callType == CallType::Call || isTailCall);
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    const auto& callingConvention = wasmCallingConvention();
    CallInformation wasmCalleeInfo = callingConvention.callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCalleeInfo.headerAndArgumentStackSizeInBytes);
    Checked<int32_t> tailCallStackOffsetFromFP;

    if (isTailCall) {
        m_makesTailCalls = true;

        const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
        const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex);
        CallInformation wasmCallerInfo = callingConvention.callInformationFor(callerTypeDefinition, CallRole::Callee);
        Checked<int32_t> callerStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCallerInfo.headerAndArgumentStackSizeInBytes);
        tailCallStackOffsetFromFP = callerStackSize - calleeStackSize;

        m_tailCallStackOffsetFromFP = std::min(m_tailCallStackOffsetFromFP, tailCallStackOffsetFromFP);
    } else {
        m_makesCalls = true;
        for (unsigned i = 0; i < signature.as<FunctionSignature>()->returnCount(); ++i)
            results.append(tmpForType(signature.as<FunctionSignature>()->returnType(i)));
    }

    m_proc.requestCallArgAreaSizeInBytes(calleeStackSize);

    auto currentInstance = self().gPtr();
    append(Move, instanceValue(), currentInstance);

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {

        auto emitCallToImport = [&, this](CallPatchpointData data) -> void {
            CallPatchpointData::first_type patchpoint = data.first;
            CallPatchpointData::second_type handle = data.second;
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(RegisterSetBuilder::wasmPinnedRegisters(MemoryMode::BoundsChecking));
            patchpoint->setGenerator([this, handle, isTailCall, tailCallStackOffsetFromFP](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                if (isTailCall)
                    prepareForTailCall(jit, params, tailCallStackOffsetFromFP);
                if (handle)
                    handle->generate(jit, params, this);
                JIT_COMMENT(jit, "Wasm to imported function call patchpoint");
                if (isTailCall) {
                    // In tail-call, we always configure JSWebAssemblyInstance* in |this| to anchor it from conservative GC roots.
                    CCallHelpers::Address thisSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register)) - prologueStackPointerDelta());
                    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfOwner()), GPRInfo::nonPreservedNonArgumentGPR1);
                    jit.storePtr(GPRInfo::nonPreservedNonArgumentGPR1, thisSlot.withOffset(PayloadOffset));
                    jit.farJump(params[0].gpr(), WasmEntryPtrTag);
                } else
                    jit.call(params[params.proc().resultCount(params.value()->type())].gpr(), WasmEntryPtrTag);
            });
        };

        m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

        auto jumpDestination = self().gPtr();
        append(Move, Arg::bigImm(Instance::offsetOfImportFunctionStub(functionIndex)), jumpDestination);
        append(Derived::AddPtr, instanceValue(), jumpDestination);
        append(Move, Arg::addr(jumpDestination), jumpDestination);

        if (isTailCall) {
            emitCallToImport(self().emitTailCallPatchpoint(m_currentBlock, tailCallStackOffsetFromFP, wasmCalleeInfo.params, args, { { jumpDestination, B3::ValueRep(GPRInfo::nonPreservedNonArgumentGPR0) } }));
            return { };
        }

        emitCallToImport(self().emitCallPatchpoint(m_currentBlock, self().toB3ResultType(&signature), results, args, wasmCalleeInfo, { { jumpDestination, B3::ValueRep(GPRInfo::nonPreservedNonArgumentGPR0) } }));

        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(m_info.memory, currentInstance, m_currentBlock);

        return { };
    } // isImportedFunctionFromFunctionIndexSpace

    Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;

    auto emitUnlinkedWasmToWasmCall = [&, this](CallPatchpointData data) -> void {
        CallPatchpointData::first_type patchpoint = data.first;
        CallPatchpointData::second_type handle = data.second;
        patchpoint->setGenerator([this, handle, unlinkedWasmToWasmCalls, functionIndex, isTailCall, tailCallStackOffsetFromFP](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            if (isTailCall)
                prepareForTailCall(jit, params, tailCallStackOffsetFromFP);
            if (handle)
                handle->generate(jit, params, this);
            JIT_COMMENT(jit, "Wasm to wasm unlinked function call patchpoint");
            if (isTailCall) {
                // In tail-call, we always configure JSWebAssemblyInstance* in |this| to anchor it from conservative GC roots.
                CCallHelpers::Address thisSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register)) - prologueStackPointerDelta());
                jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfOwner()), GPRInfo::nonPreservedNonArgumentGPR1);
                jit.storePtr(GPRInfo::nonPreservedNonArgumentGPR1, thisSlot.withOffset(PayloadOffset));
            }
            CCallHelpers::Call call = isTailCall ? jit.threadSafePatchableNearTailCall() : jit.threadSafePatchableNearCall();
            jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex](LinkBuffer& linkBuffer) {
                unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
            });
        });
    };

    if (isTailCall) {
        emitUnlinkedWasmToWasmCall(self().emitTailCallPatchpoint(m_currentBlock, tailCallStackOffsetFromFP, wasmCalleeInfo.params, args));
        return { };
    }

    // We do not need to store the JS instance in |this|
    // 1. It is not tail-call. So this does not clobber the arguments of this function.
    // 2. We are not changing instance. Thus, |this| of this function's arguments are the same and will continue to provide a GC root for the JS instance.

    auto data = self().emitCallPatchpoint(m_currentBlock, self().toB3ResultType(&signature), results, args, wasmCalleeInfo);
    auto* patchpoint = data.first;

    // We need to clobber the size register since the LLInt always bounds checks
    if (self().useSignalingMemory() || m_info.memory.isShared())
        patchpoint->clobberLate(RegisterSetBuilder { GPRInfo::wasmBoundsCheckingSizeRegister });

    emitUnlinkedWasmToWasmCall(WTFMove(data));

    if (m_info.callCanClobberInstance(functionIndex))
        self().restoreWebAssemblyGlobalState(m_info.memory, currentInstance, m_currentBlock);

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCallIndirect(unsigned tableIndex, const TypeDefinition& originalSignature, Vector<ExpressionType>& args, ResultList& results, CallType callType) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();
    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the js, we conservatively assume all call indirects
    // can be to the js for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    ExpressionType callableFunctionBuffer = self().gPtr();
    ExpressionType callableFunctionBufferLength = self().gPtr();
    {
        RELEASE_ASSERT(Arg::isValidAddrForm(Move, FuncRefTable::offsetOfFunctions(), pointerWidth()));
        RELEASE_ASSERT(Arg::isValidAddrForm(Move32, FuncRefTable::offsetOfLength(), pointerWidth()));

        self().emitLoad(instanceValue().tmp(), Instance::offsetOfTablePtr(m_numImportFunctions, tableIndex), callableFunctionBufferLength);
        ASSERT(tableIndex < m_info.tableCount());
        auto& tableInformation = m_info.table(tableIndex);
        if (tableInformation.maximum() && tableInformation.maximum().value() == tableInformation.initial()) {
            if (!tableInformation.isImport())
                append(Derived::AddPtr, Arg::imm(FuncRefTable::offsetOfFunctionsForFixedSizedTable()), callableFunctionBufferLength, callableFunctionBuffer);
            else
                append(Move, Arg::addr(callableFunctionBufferLength, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
            callableFunctionBufferLength = self().addConstant(Types::I32, tableInformation.initial());
        } else {
            append(Move, Arg::addr(callableFunctionBufferLength, FuncRefTable::offsetOfFunctions()), callableFunctionBuffer);
            append(Move32, Arg::addr(callableFunctionBufferLength, Table::offsetOfLength()), callableFunctionBufferLength);
        }
    }

    append(Move32, calleeIndex, calleeIndex);

    // Check the index we are looking for is valid.
    emitCheck([&] {
        return Inst(Branch32, nullptr, Arg::relCond(MacroAssembler::AboveOrEqual), calleeIndex, callableFunctionBufferLength);
    }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::OutOfBoundsCallIndirect);
    });

    ExpressionType calleeCode = self().gPtr();
    ExpressionType calleeInstance = self().gPtr();
    ExpressionType jsCalleeAnchor = self().gPtr();
    {
        static_assert(sizeof(TypeIndex) == sizeof(void*));
        ExpressionType calleeSignatureIndex = self().gPtr();
        // Compute the offset in the table index space we are looking for.
        append(Move, Arg::imm(sizeof(FuncRefTable::Function)), calleeSignatureIndex);
        append(Derived::MulPtr, calleeIndex, calleeSignatureIndex);
        append(Derived::AddPtr, callableFunctionBuffer, calleeSignatureIndex);

        append(Move, Arg::addr(calleeSignatureIndex, FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation()), calleeCode); // Pointer to callee code.
        append(Move, Arg::addr(calleeSignatureIndex, FuncRefTable::Function::offsetOfInstance()), calleeInstance);
        append(Move, Arg::addr(calleeSignatureIndex, FuncRefTable::Function::offsetOfValue()), jsCalleeAnchor);

        // FIXME: This seems wasteful to do two checks just for a nicer error message.
        // We should move just to use a single branch and then figure out what
        // error to use in the exception handler.

        append(Move, Arg::addr(calleeSignatureIndex, FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfSignatureIndex()), calleeSignatureIndex);

        emitCheck([&] {
            static_assert(!TypeDefinition::invalidIndex, "");
            return Inst(Derived::BranchTestPtr, nullptr, Arg::resCond(MacroAssembler::Zero), calleeSignatureIndex, calleeSignatureIndex);
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::NullTableEntry);
        });

        ExpressionType expectedSignatureIndex = self().gPtr();
        append(Move, Arg::bigImm(TypeInformation::get(originalSignature)), expectedSignatureIndex);
        emitCheck([&] {
            return Inst(Derived::BranchPtr, nullptr, Arg::relCond(MacroAssembler::NotEqual), calleeSignatureIndex, expectedSignatureIndex);
        }, [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::BadSignature);
        });
    }

    return self().emitIndirectCall(calleeInstance, calleeCode, jsCalleeAnchor, signature, args, results, callType);
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addCallRef(const TypeDefinition& originalSignature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    m_makesCalls = true;
    // Note: call ref can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the js, we conservatively assume all call indirects
    // can be to the js for our stack check calculation.
    ExpressionType calleeFunction = args.takeLast();
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));
    const TypeDefinition& signature = originalSignature.expand();

    emitThrowOnNullReference(calleeFunction);

    ExpressionType calleeCode = self().gPtr();
    append(Move, Arg::addr(self().extractJSValuePointer(calleeFunction), WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation()), calleeCode); // Pointer to callee code.

    auto jsCalleeInstance = self().gPtr();
    auto calleeInstance = self().gPtr();
    append(Move, Arg::addr(self().extractJSValuePointer(calleeFunction), WebAssemblyFunctionBase::offsetOfInstance()), jsCalleeInstance);
    append(Move, Arg::addr(jsCalleeInstance, JSWebAssemblyInstance::offsetOfInstance()), calleeInstance);

    return emitIndirectCall(calleeInstance, calleeCode, jsCalleeInstance, signature, args, results);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::emitIndirectCall(ExpressionType calleeInstance, ExpressionType calleeCode, ExpressionType jsCalleeAnchor, const TypeDefinition& signature, const Vector<ExpressionType>& args, ResultList& results, CallType callType) -> PartialResult
{
    bool isTailCall = callType == CallType::TailCall;
    ASSERT(callType == CallType::Call || isTailCall);

    const auto& callingConvention = wasmCallingConvention();
    CallInformation wasmCalleeInfo = callingConvention.callInformationFor(signature, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCalleeInfo.headerAndArgumentStackSizeInBytes);

    m_proc.requestCallArgAreaSizeInBytes(calleeStackSize);

    auto currentInstance = self().gPtr();
    append(Move, instanceValue(), currentInstance);

    // Do a context switch if needed.
    {
        BasicBlock* doContextSwitch = m_code.addBlock();
        BasicBlock* continuation = m_code.addBlock();

        append(Derived::BranchPtr, Arg::relCond(MacroAssembler::Equal), calleeInstance, currentInstance);
        m_currentBlock->setSuccessors(continuation, doContextSwitch);

        auto* patchpoint = addPatchpoint(B3::Void);
        patchpoint->effects.writesPinned = true;
        // We pessimistically assume we're calling something with BoundsChecking memory.
        // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
        patchpoint->clobber(RegisterSetBuilder::wasmPinnedRegisters(MemoryMode::BoundsChecking));
        patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg calleeInstance = params[0].gpr();
            GPRReg scratch = params.gpScratch(0);
            ASSERT(scratch != calleeInstance);
            UNUSED_PARAM(scratch);
            jit.storeWasmContextInstance(calleeInstance);

            if constexpr (Derived::supportsPinnedStateRegisters) {
                // FIXME: We should support more than one memory size register
                //   see: https://bugs.webkit.org/show_bug.cgi?id=162952
                ASSERT(GPRInfo::wasmBoundsCheckingSizeRegister != calleeInstance);
                ASSERT(GPRInfo::wasmBaseMemoryPointer != calleeInstance);
                jit.loadPairPtr(calleeInstance, CCallHelpers::TrustedImm32(Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
                jit.cageConditionallyAndUntag(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, scratch, /* validateAuth */ true, /* mayBeNull */ false);
            }
        });

        emitPatchpoint(doContextSwitch, patchpoint, ExpressionType(), calleeInstance);
        append(doContextSwitch, Jump);
        doContextSwitch->setSuccessors(continuation);

        m_currentBlock = continuation;
    }

    append(Move, Arg::addr(calleeCode), calleeCode);

    if (isTailCall) {
        m_makesTailCalls = true;

        const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
        const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex);
        CallInformation wasmCallerInfo = callingConvention.callInformationFor(callerTypeDefinition, CallRole::Callee);
        Checked<int32_t> callerStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCallerInfo.headerAndArgumentStackSizeInBytes);
        Checked<int32_t> tailCallStackOffsetFromFP = callerStackSize - calleeStackSize;
        m_tailCallStackOffsetFromFP = std::min(m_tailCallStackOffsetFromFP, tailCallStackOffsetFromFP);

        auto data = self().emitTailCallPatchpoint(m_currentBlock, tailCallStackOffsetFromFP, wasmCalleeInfo.params, args, { { calleeCode, B3::ValueRep(GPRInfo::nonPreservedNonArgumentGPR0) } });
        auto patchpoint = data.first;
        auto exceptionHandle = data.second;
        patchpoint->setGenerator([=, this](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            prepareForTailCall(jit, params, tailCallStackOffsetFromFP);
            if (exceptionHandle)
                exceptionHandle->generate(jit, params, this);

            // In tail-call, we always configure JSWebAssemblyInstance* in |this| to anchor it from conservative GC roots.
            CCallHelpers::Address thisSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register)) - prologueStackPointerDelta());
            jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfOwner()), GPRInfo::nonPreservedNonArgumentGPR1);
            jit.storePtr(GPRInfo::nonPreservedNonArgumentGPR1, thisSlot.withOffset(PayloadOffset));
            jit.farJump(params[0].gpr(), WasmEntryPtrTag);
        });
        return { };
    }

    m_makesCalls = true;

    for (unsigned i = 0; i < signature.as<FunctionSignature>()->returnCount(); ++i)
        results.append(tmpForType(signature.as<FunctionSignature>()->returnType(i)));

    // Since this can switch instance, we need to keep JSWebAssemblyInstance anchored in the stack.

    auto data = self().emitCallPatchpoint(m_currentBlock, self().toB3ResultType(&signature), results, args, wasmCalleeInfo, { { calleeCode, B3::ValueRep::SomeRegister }, { jsCalleeAnchor, wasmCalleeInfo.thisArgument } });
    auto* patchpoint = data.first;
    auto exceptionHandle = data.second;

    // We need to clobber all potential pinned registers since we might be leaving the instance.
    // We pessimistically assume we're always calling something that is bounds checking so
    // because the wasm->wasm thunk unconditionally overrides the size registers.
    // FIXME: We should not have to do this, but the wasm->wasm stub assumes it can
    // use all the pinned registers as scratch: https://bugs.webkit.org/show_bug.cgi?id=172181

    patchpoint->clobberLate(RegisterSetBuilder::wasmPinnedRegisters(MemoryMode::BoundsChecking));

    patchpoint->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        if (exceptionHandle)
            exceptionHandle->generate(jit, params, this);
        jit.call(params[params.proc().resultCount(params.value()->type())].gpr(), WasmEntryPtrTag);
    });

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(m_info.memory, currentInstance, m_currentBlock);

    return { };
}

template <typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::unifyValuesWithBlock(const Stack& resultStack, const ResultList& result)
{
    ASSERT(result.size() <= resultStack.size());

    for (size_t i = 0; i < result.size(); ++i)
        self().emitMove(resultStack[resultStack.size() - 1 - i], result[result.size() - 1 - i]);
}

template <typename Stack>
static void dumpExpressionStack(const CommaPrinter& comma, const Stack& expressionStack)
{
    dataLog(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, expression.value());
}

template <typename Derived, typename ExpressionType>
void AirIRGeneratorBase<Derived, ExpressionType>::dump(const ControlStack& controlStack, const Stack* stack)
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

template <typename Derived, typename ExpressionType>
B3::Origin AirIRGeneratorBase<Derived, ExpressionType>::origin()
{
    // FIXME: We should implement a way to give Inst's an origin, and pipe that
    // information into the sampling profiler: https://bugs.webkit.org/show_bug.cgi?id=234182
    return B3::Origin();
}

template<typename Generator>
Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileAirImpl(CompilationContext& compilationContext, Callee& callee, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, TierUpCount* tierUp)
{
    auto result = makeUnique<InternalFunction>();

    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();

    compilationContext.procedure = makeUnique<B3::Procedure>(info.isSIMDFunction(functionIndex));
    auto& procedure = *compilationContext.procedure;
    Code& code = procedure.code();

    if constexpr (Generator::generatesB3OriginData) {
        procedure.setOriginPrinter([](PrintStream& out, B3::Origin origin) {
            if (origin.data())
                out.print("Wasm: ", OpcodeOrigin(origin));
        });
    }
    procedure.code().setDisassembler(makeUnique<B3::Air::Disassembler>());

    // This means we cannot use either StackmapGenerationParams::usedRegisters() or
    // StackmapGenerationParams::unavailableRegisters(). In exchange for this concession, we
    // don't strictly need to run Air::reportUsedRegisters(), which saves a bit of CPU time at
    // optLevel=1.
    procedure.setNeedsUsedRegisters(false);

    procedure.setOptLevel(Options::webAssemblyBBQAirOptimizationLevel());

    Generator irGenerator(info, callee, procedure, unlinkedWasmToWasmCalls, mode, functionIndex, hasExceptionHandlers, tierUp, signature, result->osrEntryScratchBufferSize);
    FunctionParser<Generator> parser(irGenerator, function.data.data(), function.data.size(), signature, info);
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

template <typename Derived, typename ExpressionType>
template <typename IntType>
void AirIRGeneratorBase<Derived, ExpressionType>::emitChecksForModOrDiv(bool isSignedDiv, ExpressionType left, ExpressionType right)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8);

    auto const throwDivisionByZeroExceptionPatch = [=, this](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitThrowException(jit, ExceptionType::DivisionByZero);
    };

    if constexpr (sizeof(IntType) == 4) {
        emitCheck([&] {
            return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::Zero), right, right);
        }, throwDivisionByZeroExceptionPatch);
    } else
        self().emitCheckI64Zero(right, throwDivisionByZeroExceptionPatch);

    if (isSignedDiv) {
        ASSERT(std::is_signed<IntType>::value);
        IntType min = std::numeric_limits<IntType>::min();

        // FIXME: Better isel for compare with imms here.
        // https://bugs.webkit.org/show_bug.cgi?id=193999

        ExpressionType badLeft;
        ExpressionType badRight;
        {
            Type type = sizeof(IntType) == 4 ? Types::I32 : Types::I64;

            auto minTmp = self().addConstant(type, static_cast<uint64_t>(min));
            self().addCompare(type, MacroAssembler::Equal, left, minTmp, badLeft);
            auto negOne = self().addConstant(type, static_cast<uint64_t>(-1));
            self().addCompare(type, MacroAssembler::Equal, right, negOne, badRight);
        }

        emitCheck([&] {
            return Inst(BranchTest32, nullptr, Arg::resCond(MacroAssembler::NonZero), badLeft, badRight);
        },
        [=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitThrowException(jit, ExceptionType::IntegerOverflow);
        });
    }
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32DivS(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<int32_t>(true, left, right);
    self().template emitModOrDiv<int32_t>(true, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32RemS(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<int32_t>(false, left, right);
    self().template emitModOrDiv<int32_t>(false, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32DivU(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<uint32_t>(false, left, right);
    self().template emitModOrDiv<uint32_t>(true, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32RemU(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<uint32_t>(false, left, right);
    self().template emitModOrDiv<uint32_t>(false, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64DivS(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<int64_t>(true, left, right);
    self().template emitModOrDiv<int64_t>(true, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64RemS(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<int64_t>(false, left, right);
    self().template emitModOrDiv<int64_t>(false, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64DivU(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<uint64_t>(false, left, right);
    self().template emitModOrDiv<uint64_t>(true, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64RemU(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    self().template emitChecksForModOrDiv<uint64_t>(false, left, right);
    self().template emitModOrDiv<uint64_t>(false, left, right, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Ctz(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Int32);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    result = self().g32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Popcnt(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = self().g32();

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

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Popcnt(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = self().g64();

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

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Nearest(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Double);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.roundTowardNearestIntDouble(params[1].fpr(), params[0].fpr());
    });
    result = self().f64();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Nearest(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    auto* patchpoint = addPatchpoint(B3::Float);
    patchpoint->effects = B3::Effects::none();
    patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        jit.roundTowardNearestIntFloat(params[1].fpr(), params[0].fpr());
    });
    result = self().f32();
    emitPatchpoint(patchpoint, result, arg);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Trunc(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        auto* patchpoint = addPatchpoint(B3::Double);
        patchpoint->effects = B3::Effects::none();
        patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
        });
        emitPatchpoint(patchpoint, result, arg);
        return { };
    }
    emitCCall(&Math::truncDouble, result, arg);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Trunc(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        auto* patchpoint = addPatchpoint(B3::Float);
        patchpoint->effects = B3::Effects::none();
        patchpoint->setGenerator([=](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            jit.roundTowardZeroFloat(params[1].fpr(), params[0].fpr());
        });
        emitPatchpoint(patchpoint, result, arg);
        return { };
    }
    emitCCall(&Math::truncFloat, result, arg);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32TruncSF64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F64ToI32, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32TruncSF32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F32ToI32, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32TruncUF64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F64ToU32, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32TruncUF32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F32ToU32, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64TruncSF64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F64ToI64, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64TruncSF32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F32ToI64, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64TruncUF64(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F64ToU64, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64TruncUF32(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    addCheckedFloatingPointTruncation(FloatingPointTruncationKind::F32ToU64, arg, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Ceil(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(CeilFloat, arg0, result);
        return { };
    }
    emitCCall(&Math::ceilFloat, result, arg0);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Mul(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(Mul32, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Sub(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addIntegerSub(Sub32, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Le(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32DemoteF64(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    append(ConvertDoubleToFloat, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Ne(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleNotEqualOrUnordered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Lt(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleLessThanAndOrdered), arg0, arg1, result);
    return { };
}

template <typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addFloatingPointMinOrMax(Type floatType, MinOrMax minOrMax, ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
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
    append(isLessThan, Derived::moveOpForValueType(floatType), isLessThanResult, result);
    append(isLessThan, Jump);
    isLessThan->setSuccessors(continuation);

    auto isGreaterThanResult = minOrMax == MinOrMax::Max ? arg0 : arg1;
    append(isGreaterThan, Derived::moveOpForValueType(floatType), isGreaterThanResult, result);
    append(isGreaterThan, Jump);
    isGreaterThan->setSuccessors(continuation);

    auto addOp = floatType.isF32() ? AddFloat : AddDouble;
    append(isNaN, addOp, arg0, arg1, result);
    append(isNaN, Jump);
    isNaN->setSuccessors(continuation);

    m_currentBlock = continuation;

    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Min(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F32, MinOrMax::Min, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Max(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F32, MinOrMax::Max, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Min(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F64, MinOrMax::Min, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Max(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return addFloatingPointMinOrMax(Types::F64, MinOrMax::Max, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Mul(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addFloatingPointBinOp(Types::F64, MulDouble, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Div(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addFloatingPointBinOp(Types::F32, DivFloat, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Clz(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CountLeadingZeros32, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Copysign(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    // FIXME: We can have better codegen here for the imms and two operand forms on x86
    // https://bugs.webkit.org/show_bug.cgi?id=193999
    result = self().f32();
    auto temp1 = self().g32();
    auto sign = self().g32();
    auto value = self().g32();

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

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32ReinterpretI32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    append(Move32ToFloat, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Ne(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleNotEqualOrUnordered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Gt(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Sqrt(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    append(SqrtFloat, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Ge(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64GtS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::GreaterThan, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64GtU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::Above, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Div(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addFloatingPointBinOp(Types::F64, DivDouble, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Add(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    append(AddFloat, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32LeU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::BelowOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32LeS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::LessThanOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Ne(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::NotEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Clz(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    append(CountLeadingZeros64, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Neg(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    if (isValidForm(NegateFloat, Arg::Tmp, Arg::Tmp))
        append(NegateFloat, arg0, result);
    else {
        auto constant = self().addConstant(Types::I32, bitwise_cast<uint32_t>(static_cast<float>(-0.0)));
        auto temp = self().g32();
        append(MoveFloatTo32, arg0, temp);
        append(Xor32, constant, temp);
        append(Move32ToFloat, temp, result);
    }
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32And(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(And32, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32LtU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::Below, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Rotr(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I64, RotateRight64, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Abs(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    return self().addFloatingPointAbs(AbsDouble, arg0, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32LtS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::LessThan, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Eq(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    return self().addCompare(Types::I32, MacroAssembler::Equal, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Copysign(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    // FIXME: We can have better codegen here for the imms and two operand forms on x86
    // https://bugs.webkit.org/show_bug.cgi?id=193999
    result = self().f64();
    auto temp1 = self().g64();
    auto sign = self().g64();
    auto value = self().g64();

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

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32ConvertSI64(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    append(ConvertInt64ToFloat, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Rotl(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    if (isARM64()) {
        // ARM64 doesn't have a rotate left. (ARMv7 doesn't either but it's handled in the 32-bit code)
        auto newShift = self().g64();
        append(Move, arg1, newShift);
        append(Neg64, newShift);
        return self().addShift(Types::I64, RotateRight64, arg0, newShift, result);
    }
    return self().addShift(Types::I64, RotateLeft64, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Lt(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64ConvertSI32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    append(ConvertInt32ToDouble, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Eq(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareDouble, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg0, arg1, result);
    return { };
}


template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Le(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Ge(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32ShrU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I32, Urshift32, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32ConvertUI32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    auto temp = self().g64();
    append(Move32, arg0, temp);
    append(ConvertInt64ToFloat, temp, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32ShrS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I32, Rshift32, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32GeU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::AboveOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Ceil(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(CeilDouble, arg0, result);
        return { };
    }
    emitCCall(&Math::ceilDouble, result, arg0);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32GeS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::GreaterThanOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Shl(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I32, Lshift32, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Floor(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(FloorDouble, arg0, result);
        return { };
    }
    emitCCall(&Math::floorDouble, result, arg0);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Xor(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(Xor32, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Abs(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    return self().addFloatingPointAbs(AbsFloat, arg0, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Mul(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    append(MulFloat, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Sub(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addIntegerSub(Sub64, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32ReinterpretF32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(MoveFloatTo32, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Add(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(Add32, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Sub(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addFloatingPointBinOp(Types::F64, SubDouble, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Or(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(Or32, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64LtU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::Below, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64LtS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::LessThan, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64ConvertSI64(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    append(ConvertInt64ToDouble, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Xor(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    append(Xor64, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64GeU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::AboveOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Mul(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    append(Mul64, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Sub(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    if (isValidForm(SubFloat, Arg::Tmp, Arg::Tmp, Arg::Tmp))
        append(SubFloat, arg0, arg1, result);
    else {
        RELEASE_ASSERT(isX86());
        append(MoveFloat, arg0, result);
        append(SubFloat, arg1, result);
    }
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64PromoteF32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    append(ConvertFloatToDouble, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Add(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    append(AddDouble, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64GeS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::GreaterThanOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64ExtendUI32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    append(Move32, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Ne(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::NotEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64ReinterpretI64(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    append(Move64ToDouble, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Eq(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Eq(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::Equal, arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Floor(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    if (MacroAssembler::supportsFloatingPointRounding()) {
        append(FloorFloat, arg0, result);
        return { };
    }
    emitCCall(&Math::floorFloat, result, arg0);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32ConvertSI32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f32();
    append(ConvertInt32ToFloat, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Eqz(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(Test32, Arg::resCond(MacroAssembler::Zero), arg0, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64ReinterpretF64(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    append(MoveDoubleTo64, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64ShrS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I64, Rshift64, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64ShrU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I64, Urshift64, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Sqrt(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    append(SqrtDouble, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Shl(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I64, Lshift64, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF32Gt(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(CompareFloat, Arg::doubleCond(MacroAssembler::DoubleGreaterThanAndOrdered), arg0, arg1, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32WrapI64(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(Move32, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Rotl(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    if (isARM64() || isARM_THUMB2()) {
        // ARMs do not have a 'rotate left' instruction.
        auto newShift = isARM64() ? self().g64() : self().g32();
        append(Move, arg1, newShift);
        append(isARM64() ? Neg64 : Neg32, newShift);
        return self().addShift(Types::I32, RotateRight32, arg0, newShift, result);
    }
    return self().addShift(Types::I32, RotateLeft32, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Rotr(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addShift(Types::I32, RotateRight32, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32GtU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::Above, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64ExtendSI32(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    append(SignExtend32ToPtr, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Extend8S(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(SignExtend8To32, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32Extend16S(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g32();
    append(SignExtend16To32, arg0, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Extend8S(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    auto temp = self().g32();
    append(Move32, arg0, temp);
    append(SignExtend8To32, temp, temp);
    append(SignExtend32ToPtr, temp, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Extend16S(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    auto temp = self().g32();
    append(Move32, arg0, temp);
    append(SignExtend16To32, temp, temp);
    append(SignExtend32ToPtr, temp, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Extend32S(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    auto temp = self().g32();
    append(Move32, arg0, temp);
    append(SignExtend32ToPtr, temp, result);
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI32GtS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I32, MacroAssembler::GreaterThan, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addF64Neg(ExpressionType arg0, ExpressionType& result) -> PartialResult
{
    result = self().f64();
    if (isValidForm(NegateDouble, Arg::Tmp, Arg::Tmp))
        append(NegateDouble, arg0, result);
    else {
        auto constant = self().addConstant(Types::I64, bitwise_cast<uint64_t>(static_cast<double>(-0.0)));
        auto temp = self().g64();
        append(MoveDoubleTo64, arg0, temp);
        append(Xor64, constant, temp);
        append(Move64ToDouble, temp, result);
    }
    return { };
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64LeU(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::BelowOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64LeS(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    return self().addCompare(Types::I64, MacroAssembler::LessThanOrEqual, arg0, arg1, result);
}

template<typename Derived, typename ExpressionType>
auto AirIRGeneratorBase<Derived, ExpressionType>::addI64Add(ExpressionType arg0, ExpressionType arg1, ExpressionType& result) -> PartialResult
{
    result = self().g64();
    append(Add64, arg0, arg1, result);
    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_B3JIT)
