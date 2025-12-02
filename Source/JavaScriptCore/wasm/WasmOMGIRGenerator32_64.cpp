/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "WasmOMGIRGenerator.h"

#if ENABLE(WEBASSEMBLY_OMGJIT)

#include "AirCode.h"
#include "AllowMacroScratchRegisterUsageIf.h"
#include "B3AbstractHeapRepository.h"
#include "B3BasicBlockInlines.h"
#include "B3CCallValue.h"
#include "B3Const128Value.h"
#include "B3ConstPtrValue.h"
#include "B3EstimateStaticExecutionCounts.h"
#include "B3FixSSA.h"
#include "B3Generate.h"
#include "B3InsertionSet.h"
#include "B3SIMDValue.h"
#include "B3StackmapGenerationParams.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3Validate.h"
#include "B3ValueInlines.h"
#include "B3ValueKey.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "B3WasmAddressValue.h"
#include "B3WasmBoundsCheckValue.h"
#include "CompilerTimingScope.h"
#include "FunctionAllowlist.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyArrayInlines.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyStruct.h"
#include "ProbeContext.h"
#include "ScratchRegisterAllocator.h"
#include "WasmBaselineData.h"
#include "WasmBranchHints.h"
#include "WasmCallProfile.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmExceptionType.h"
#include "WasmFaultSignalHandler.h"
#include "WasmFunctionParser.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmMemory.h"
#include "WasmMergedProfile.h"
#include "WasmOSREntryData.h"
#include "WasmOpcodeOrigin.h"
#include "WasmOperations.h"
#include "WasmSIMDOpcodes.h"
#include "WasmThunks.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyFunctionBase.h"
#include <limits>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if !ENABLE(WEBASSEMBLY)
#error ENABLE(WEBASSEMBLY_OMGJIT) is enabled, but ENABLE(WEBASSEMBLY) is not.
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if USE(JSVALUE32_64)

#define OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS 0
#define OMG_JSVALUE_32_64_NYI 1

namespace JSC { namespace Wasm {

using namespace B3;

namespace {
namespace WasmOMGIRGeneratorInternal {
static constexpr bool verbose = false;
static constexpr bool verboseInlining = false;
static constexpr bool traceExecution = false;
static constexpr bool traceStackValues = false;
static constexpr bool verboseTailCalls = false;
#if ASSERT_ENABLED
static constexpr bool traceExecutionIncludesConstructionSite = false;
#endif
}
}

#define TRACE_VALUE(...) do { if constexpr (WasmOMGIRGeneratorInternal::traceExecution) { traceValue(__VA_ARGS__); } } while (0)

#define TRACE_CF(...) do { if constexpr (WasmOMGIRGeneratorInternal::traceExecution) { traceCF(__VA_ARGS__); } } while (0)

class OMGIRGenerator {
    WTF_MAKE_TZONE_ALLOCATED(OMGIRGenerator);
public:
    using ExpressionType = Variable*;
    using ResultList = Vector<ExpressionType, 8>;
    using CallType = CallLinkInfo::CallType;
    using CallPatchpointData = std::tuple<B3::PatchpointValue*, RefPtr<PatchpointExceptionHandle>, RefPtr<B3::StackmapGenerator>>;
    using WasmConstRefValue = Const64Value;

    static constexpr bool shouldFuseBranchCompare = false;
    static constexpr bool tierSupportsSIMD() { return true; }
    static constexpr bool validateFunctionBodySize = true;

    struct ControlData {
        ControlData(Procedure& proc, Origin origin, BlockSignature signature, BlockType type, unsigned stackSize, BasicBlock* continuation, BasicBlock* special = nullptr)
            : controlBlockType(type)
            , m_signature(signature)
            , m_stackSize(stackSize)
            , continuation(continuation)
            , special(special)
        {
            ASSERT(type != BlockType::Try && type != BlockType::Catch);
            if (type != BlockType::TopLevel)
                m_stackSize -= signature.m_signature->argumentCount();

            if (type == BlockType::Loop) {
                for (unsigned i = 0; i < signature.m_signature->argumentCount(); ++i)
                    phis.append(proc.add<Value>(Phi, toB3Type(signature.m_signature->argumentType(i)), origin));
            } else {
                for (unsigned i = 0; i < signature.m_signature->returnCount(); ++i)
                    phis.append(proc.add<Value>(Phi, toB3Type(signature.m_signature->returnType(i)), origin));
            }
        }

        ControlData(Procedure& proc, Origin origin, BlockSignature signature, BlockType type, unsigned stackSize, BasicBlock* continuation, unsigned tryStart, unsigned tryDepth)
            : controlBlockType(type)
            , m_signature(signature)
            , m_stackSize(stackSize)
            , continuation(continuation)
            , special(nullptr)
            , m_tryStart(tryStart)
            , m_tryCatchDepth(tryDepth)
        {
            ASSERT(type == BlockType::Try || type == BlockType::TryTable);
            m_stackSize -= signature.m_signature->argumentCount();
            for (unsigned i = 0; i < signature.m_signature->returnCount(); ++i)
                phis.append(proc.add<Value>(Phi, toB3Type(signature.m_signature->returnType(i)), origin));
        }

        ControlData()
        {
        }

        static bool isIf(const ControlData& control) { return control.blockType() == BlockType::If; }
        static bool isElse(const ControlData& control) { return control.blockType() == BlockType::Else; }
        static bool isTry(const ControlData& control) { return control.blockType() == BlockType::Try; }
        static bool isTryTable(const ControlData& control) { return control.blockType() == BlockType::TryTable; }
        static bool isAnyCatch(const ControlData& control) { return control.blockType() == BlockType::Catch; }
        static bool isTopLevel(const ControlData& control) { return control.blockType() == BlockType::TopLevel; }
        static bool isLoop(const ControlData& control) { return control.blockType() == BlockType::Loop; }
        static bool isBlock(const ControlData& control) { return control.blockType() == BlockType::Block; }
        static bool isCatch(const ControlData& control)
        {
            if (control.blockType() != BlockType::Catch)
                return false;
            return control.catchKind() == CatchKind::Catch;
        }

        void dump(PrintStream& out) const
        {
            switch (blockType()) {
            case BlockType::If:
                out.print("If:       ");
                break;
            case BlockType::Else:
                out.print("Else:     ");
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
            case BlockType::TryTable:
                out.print("TryTable: ");
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
        }

        BlockType blockType() const { return controlBlockType; }

        BlockSignature signature() const { return m_signature; }

        bool hasNonVoidresult() const { return m_signature.m_signature->returnsVoid(); }

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

        void convertTryToCatch(unsigned tryEndCallSiteIndex, Variable* exception)
        {
            ASSERT(blockType() == BlockType::Try);
            controlBlockType = BlockType::Catch;
            m_catchKind = CatchKind::Catch;
            m_tryEnd = tryEndCallSiteIndex;
            m_exception = exception;
        }

        void convertTryToCatchAll(unsigned tryEndCallSiteIndex, Variable* exception)
        {
            ASSERT(blockType() == BlockType::Try);
            controlBlockType = BlockType::Catch;
            m_catchKind = CatchKind::CatchAll;
            m_tryEnd = tryEndCallSiteIndex;
            m_exception = exception;
        }

        struct TryTableTarget {
            CatchKind type;
            uint32_t tag;
            const TypeDefinition* exceptionSignature;
            ControlRef target;
        };
        using TargetList = Vector<TryTableTarget>;

        void setTryTableTargets(TargetList&& targets)
        {
            m_tryTableTargets = WTFMove(targets);
        }

        void endTryTable(unsigned tryEndCallSiteIndex)
        {
            ASSERT(blockType() == BlockType::TryTable);
            m_tryEnd = tryEndCallSiteIndex;
        }

        FunctionArgCount branchTargetArity() const
        {
            if (blockType() == BlockType::Loop)
                return m_signature.m_signature->argumentCount();
            return m_signature.m_signature->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (blockType() == BlockType::Loop)
                return m_signature.m_signature->argumentType(i);
            return m_signature.m_signature->returnType(i);
        }

        unsigned tryStart() const
        {
            ASSERT(controlBlockType == BlockType::Try || controlBlockType == BlockType::TryTable || controlBlockType == BlockType::Catch);
            return m_tryStart;
        }

        unsigned tryEnd() const
        {
            ASSERT(controlBlockType == BlockType::Catch || controlBlockType == BlockType::TryTable);
            return m_tryEnd;
        }

        unsigned tryDepth() const
        {
            ASSERT(controlBlockType == BlockType::Try || controlBlockType == BlockType::TryTable || controlBlockType == BlockType::Catch);
            return m_tryCatchDepth;
        }

        CatchKind catchKind() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_catchKind;
        }

        Variable* exception() const
        {
            ASSERT(controlBlockType == BlockType::Catch || controlBlockType == BlockType::TryTable);
            return m_exception;
        }

        unsigned stackSize() const { return m_stackSize; }

    private:
        // FIXME: Compress OMGIRGenerator::ControlData fields using an union
        // https://bugs.webkit.org/show_bug.cgi?id=231212
        friend class OMGIRGenerator;
        BlockType controlBlockType;
        BlockSignature m_signature;
        unsigned m_stackSize;
        BasicBlock* continuation;
        BasicBlock* special;
        Vector<Value*> phis;
        unsigned m_tryStart;
        unsigned m_tryEnd;
        unsigned m_tryCatchDepth;
        CatchKind m_catchKind;
        Variable* m_exception;
        TargetList m_tryTableTargets;
    };

    using ControlType = ControlData;
    using ExpressionList = Vector<ExpressionType, 1>;

    using ControlEntry = FunctionParser<OMGIRGenerator>::ControlEntry;
    using ControlStack = FunctionParser<OMGIRGenerator>::ControlStack;
    using Stack = FunctionParser<OMGIRGenerator>::Stack;
    using TypedExpression = FunctionParser<OMGIRGenerator>::TypedExpression;
    using CatchHandler = FunctionParser<OMGIRGenerator>::CatchHandler;
    using ArgumentList = FunctionParser<OMGIRGenerator>::ArgumentList;

    static_assert(std::is_same_v<ResultList, FunctionParser<OMGIRGenerator>::ResultList>);

    typedef String ErrorType;
    typedef Unexpected<ErrorType> UnexpectedResult;
    typedef Expected<std::unique_ptr<InternalFunction>, ErrorType> Result;
    typedef Expected<void, ErrorType> PartialResult;

    static ExpressionType emptyExpression() { return nullptr; };

    enum class CastKind { Cast, Test };

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module failed compiling: "_s, makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (condition) [[unlikely]]                  \
            return fail(__VA_ARGS__);             \
    } while (0)

    unsigned advanceCallSiteIndex()
    {
        if (m_inlineParent)
            return m_inlineRoot->advanceCallSiteIndex();
        return ++m_callSiteIndex;
    }

    unsigned callSiteIndex() const
    {
        if (m_inlineParent)
            return m_inlineRoot->callSiteIndex();
        return m_callSiteIndex;
    }

    OMGIRGenerator(AbstractHeapRepository&, CompilationContext&, Module&, CalleeGroup&, const ModuleInformation&, IPIntCallee&, OptimizingJITCallee&, Procedure&, Vector<UnlinkedWasmToWasmCall>&, FixedBitVector& outgoingDirectCallees, unsigned& osrEntryScratchBufferSize, MemoryMode, CompilationMode, unsigned functionIndex, unsigned loopIndexForOSREntry);
    OMGIRGenerator(AbstractHeapRepository&, CompilationContext&, OMGIRGenerator& inlineCaller, OMGIRGenerator& inlineRoot, Module&, CalleeGroup&, unsigned functionIndex, IPIntCallee&, BasicBlock* returnContinuation, Vector<Value*> args);

    void computeStackCheckSize(bool& needsOverflowCheck, int32_t& checkSize);

    Value* truncate(Value *v)
    {
        return m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), v);
    }

    Value* wasmRefOfCell(Value* cell)
    {
        return m_currentBlock->appendNew<Value>(m_proc, Stitch, origin(), cell, constant(Int32, JSValue::CellTag));
    }

    Value* pointerOfWasmRef(Value* ref)
    {
        return m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), ref);
    }

    Value* pointerOfInt32(Value* value)
    {
        return value;
    }

    Value* int32OfPointer(Value* value)
    {
        return value;
    }

    // SIMD
    bool usesSIMD() { return m_info.usesSIMD(m_functionIndex); }
    void notifyFunctionUsesSIMD() { ASSERT(m_info.usesSIMD(m_functionIndex)); }
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

    ExpressionType WARN_UNUSED_RETURN addConstant(v128_t value)
    {
        return push(m_currentBlock->appendNew<Const128Value>(m_proc, origin(), value));
    }

    // SIMD generated

    #define B3_OP_CASE(OP) \
    else if (op == SIMDLaneOperation::OP) b3Op = B3::Vector##OP;

    #define B3_OP_CASES() \
    B3::Opcode b3Op = B3::Oops; \
    if (false) { }

    auto addExtractLane(SIMDInfo info, uint8_t lane, ExpressionType v, ExpressionType& result) -> PartialResult
    {
        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), B3::VectorExtractLane, toB3Type(simdScalarType(info.lane)), info,
            lane,
            get(v)));
        return { };
    }

    auto addReplaceLane(SIMDInfo info, uint8_t lane, ExpressionType v, ExpressionType s, ExpressionType& result) -> PartialResult
    {
        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), B3::VectorReplaceLane, B3::V128, info,
            lane,
            get(v),
            get(s)));
        return { };
    }

    auto addSIMDI_V(SIMDLaneOperation op, SIMDInfo info, ExpressionType v, ExpressionType& result) -> PartialResult
    {
        B3_OP_CASES()
        B3_OP_CASE(Bitmask)
        B3_OP_CASE(AnyTrue)
        B3_OP_CASE(AllTrue)
        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), b3Op, B3::Int32, info,
            get(v)));
        return { };
    }

    auto addSIMDV_V(SIMDLaneOperation op, SIMDInfo info, ExpressionType v, ExpressionType& result) -> PartialResult
    {
        B3_OP_CASES()
        B3_OP_CASE(Demote)
        B3_OP_CASE(Promote)
        B3_OP_CASE(Abs)
        B3_OP_CASE(Popcnt)
        B3_OP_CASE(Ceil)
        B3_OP_CASE(Floor)
        B3_OP_CASE(Trunc)
        B3_OP_CASE(Nearest)
        B3_OP_CASE(Sqrt)
        B3_OP_CASE(ExtaddPairwise)
        B3_OP_CASE(Convert)
        B3_OP_CASE(ConvertLow)
        B3_OP_CASE(ExtendHigh)
        B3_OP_CASE(ExtendLow)
        B3_OP_CASE(TruncSat)
        B3_OP_CASE(RelaxedTruncSat)
        B3_OP_CASE(Not)
        B3_OP_CASE(Neg)

        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), b3Op, B3::V128, info,
            get(v)));
        return { };
    }

    auto addSIMDBitwiseSelect(ExpressionType v1, ExpressionType v2, ExpressionType c, ExpressionType& result) -> PartialResult
    {
        auto b3Op = B3::VectorBitwiseSelect;
        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), b3Op, B3::V128, SIMDInfo { SIMDLane::v128, SIMDSignMode::None },
            get(v1), get(v2), get(c)));
        return { };
    }

    auto addSIMDRelOp(SIMDLaneOperation, SIMDInfo info, ExpressionType lhs, ExpressionType rhs, Air::Arg relOp, ExpressionType& result) -> PartialResult
    {
        B3::Opcode b3Op = Oops;
        if (scalarTypeIsIntegral(info.lane)) {
            switch (relOp.asRelationalCondition()) {
            case MacroAssembler::Equal:
                b3Op = VectorEqual;
                break;
            case MacroAssembler::NotEqual:
                b3Op = VectorNotEqual;
                break;
            case MacroAssembler::LessThan:
                b3Op = VectorLessThan;
                break;
            case MacroAssembler::LessThanOrEqual:
                b3Op = VectorLessThanOrEqual;
                break;
            case MacroAssembler::Below:
                b3Op = VectorBelow;
                break;
            case MacroAssembler::BelowOrEqual:
                b3Op = VectorBelowOrEqual;
                break;
            case MacroAssembler::GreaterThan:
                b3Op = VectorGreaterThan;
                break;
            case MacroAssembler::GreaterThanOrEqual:
                b3Op = VectorGreaterThanOrEqual;
                break;
            case MacroAssembler::Above:
                b3Op = VectorAbove;
                break;
            case MacroAssembler::AboveOrEqual:
                b3Op = VectorAboveOrEqual;
                break;
            }
        } else {
            switch (relOp.asDoubleCondition()) {
            case MacroAssembler::DoubleEqualAndOrdered:
                b3Op = VectorEqual;
                break;
            case MacroAssembler::DoubleNotEqualOrUnordered:
                b3Op = VectorNotEqual;
                break;
            case MacroAssembler::DoubleLessThanAndOrdered:
                b3Op = VectorLessThan;
                break;
            case MacroAssembler::DoubleLessThanOrEqualAndOrdered:
                b3Op = VectorLessThanOrEqual;
                break;
            case MacroAssembler::DoubleGreaterThanAndOrdered:
                b3Op = VectorGreaterThan;
                break;
            case MacroAssembler::DoubleGreaterThanOrEqualAndOrdered:
                b3Op = VectorGreaterThanOrEqual;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }

        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), b3Op, B3::V128, info,
            get(lhs), get(rhs)));
        return { };
    }

    Value* fixupOutOfBoundsIndicesForSwizzle(Value* input, Value* indexes)
    {
        // The intel version of the swizzle instruction does not handle OOB indices properly,
        // so we need to fix them up.
        ASSERT(isX86());
        // Let each byte mask be 112 (0x70) then after VectorAddSat
        // each index > 15 would set the saturated index's bit 7 to 1,
        // whose corresponding byte will be zero cleared in VectorSwizzle.
        // https://github.com/WebAssembly/simd/issues/93
        v128_t mask;
        mask.u64x2[0] = 0x7070707070707070;
        mask.u64x2[1] = 0x7070707070707070;
        auto saturatingMask = m_currentBlock->appendNew<Const128Value>(m_proc, origin(), mask);
        auto saturatedIndexes = m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), VectorAddSat, B3::V128, SIMDLane::i8x16, SIMDSignMode::Unsigned, saturatingMask, indexes);
        return m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, input, saturatedIndexes);
    }

    auto addSIMDV_VV(SIMDLaneOperation op, SIMDInfo info, ExpressionType a, ExpressionType b, ExpressionType& result) -> PartialResult
    {
        B3_OP_CASES()
        B3_OP_CASE(And)
        B3_OP_CASE(Andnot)
        B3_OP_CASE(AvgRound)
        B3_OP_CASE(DotProduct)
        B3_OP_CASE(Add)
        B3_OP_CASE(Mul)
        B3_OP_CASE(MulSat)
        B3_OP_CASE(Sub)
        B3_OP_CASE(Div)
        B3_OP_CASE(Pmax)
        B3_OP_CASE(Pmin)
        B3_OP_CASE(Or)
        B3_OP_CASE(Swizzle)
        B3_OP_CASE(RelaxedSwizzle)
        B3_OP_CASE(Xor)
        B3_OP_CASE(Narrow)
        B3_OP_CASE(AddSat)
        B3_OP_CASE(SubSat)
        B3_OP_CASE(Max)
        B3_OP_CASE(Min)

        if (isX86() && b3Op == B3::VectorSwizzle) {
            result = push(fixupOutOfBoundsIndicesForSwizzle(get(a), get(b)));
            return { };
        }

        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), b3Op, B3::V128, info,
            get(a), get(b)));
        return { };
    }

    auto addSIMDRelaxedFMA(SIMDLaneOperation op, SIMDInfo info, ExpressionType m1, ExpressionType m2, ExpressionType add, ExpressionType& result) -> PartialResult
    {
        B3_OP_CASES()
        B3_OP_CASE(RelaxedMAdd)
        B3_OP_CASE(RelaxedNMAdd)

        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), b3Op, B3::V128, info,
            get(m1), get(m2), get(add)));
        return { };
    }

    PartialResult WARN_UNUSED_RETURN addDrop(ExpressionType);
    PartialResult WARN_UNUSED_RETURN addInlinedArguments(const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN addArguments(const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

    // References
    PartialResult WARN_UNUSED_RETURN addRefIsNull(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addRefFunc(FunctionSpaceIndex index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addRefAsNonNull(TypedExpression, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addRefEq(ExpressionType, ExpressionType, ExpressionType&);

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
    PartialResult WARN_UNUSED_RETURN teeLocal(uint32_t, ExpressionType, ExpressionType& result);

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
    PartialResult WARN_UNUSED_RETURN addRefI31(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetS(TypedExpression ref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetU(TypedExpression ref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNew(uint32_t index, ExpressionType size, ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNewDefault(uint32_t index, ExpressionType size, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNewFixed(uint32_t typeIndex, ArgumentList& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayGet(ExtGCOpType arrayGetKind, uint32_t typeIndex, TypedExpression arrayref, ExpressionType index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNewData(uint32_t typeIndex, uint32_t dataIndex, ExpressionType size, ExpressionType offset, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNewElem(uint32_t typeIndex, uint32_t elemSegmentIndex, ExpressionType size, ExpressionType offset, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t typeIndex, TypedExpression arrayref, ExpressionType index, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addArrayLen(TypedExpression arrayref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayFill(uint32_t, TypedExpression, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayCopy(uint32_t, TypedExpression, ExpressionType, uint32_t, TypedExpression, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayInitElem(uint32_t, TypedExpression, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayInitData(uint32_t, TypedExpression, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t typeIndex, ArgumentList& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructNewDefault(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructGet(ExtGCOpType structGetKind, TypedExpression structReference, const StructType&, uint32_t fieldIndex, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructSet(TypedExpression structReference, const StructType&, uint32_t fieldIndex, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addRefTest(TypedExpression reference, bool allowNull, int32_t heapType, bool shouldNegate, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addRefCast(TypedExpression reference, bool allowNull, int32_t heapType, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addAnyConvertExtern(ExpressionType reference, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addExternConvertAny(ExpressionType reference, ExpressionType& result);

    // Basic operators
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
    PartialResult WARN_UNUSED_RETURN addTryTable(BlockSignature, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned exceptionIndex, const TypeDefinition&, Stack&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned exceptionIndex, ArgumentList& args, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrowRef(TypedExpression exception, Stack&);

    PartialResult WARN_UNUSED_RETURN addInlinedReturn(const auto& returnValues);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranchNull(ControlType&, ExpressionType, const Stack&, bool, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addBranchCast(ControlType&, TypedExpression, const Stack&, bool, int32_t, bool);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& = { });

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&) { return { }; }

    // Fused comparison stubs (B3 will do this for us later).
    PartialResult WARN_UNUSED_RETURN addFusedBranchCompare(OpType, ControlType&, ExpressionType, const Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    PartialResult WARN_UNUSED_RETURN addFusedBranchCompare(OpType, ControlType&, ExpressionType, ExpressionType, const Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    PartialResult WARN_UNUSED_RETURN addFusedIfCompare(OpType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    PartialResult WARN_UNUSED_RETURN addFusedIfCompare(OpType, ExpressionType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&) { RELEASE_ASSERT_NOT_REACHED(); }

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(unsigned, FunctionSpaceIndex functionIndexSpace, const TypeDefinition&, ArgumentList& args, ResultList& results, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned, unsigned tableIndex, const TypeDefinition&, ArgumentList& args, ResultList& results, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallRef(unsigned, const TypeDefinition&, ArgumentList& args, ResultList& results, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN addCrash();

    using ValueResults = Vector<Value*, 16>;
    void fillCallResults(Value* callResult, const TypeDefinition& signature, ValueResults&);
    PartialResult WARN_UNUSED_RETURN emitDirectCall(unsigned, FunctionSpaceIndex functionIndexSpace, const TypeDefinition&, ArgumentList& args, ValueResults&, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN emitIndirectCall(Value* calleeInstance, Value* calleeCode, Value* boxedCalleeCallee, const TypeDefinition&, const ArgumentList& args, ValueResults&, CallType = CallType::Call);

    Vector<ConstrainedValue> createCallConstrainedArgs(BasicBlock*, const CallInformation& wasmCalleeInfo, const ArgumentList&);
    auto createCallPatchpoint(BasicBlock*, const TypeDefinition&, const CallInformation&, const ArgumentList& tmpArgs) -> CallPatchpointData;
    auto createTailCallPatchpoint(BasicBlock*, const TypeDefinition&, const CallInformation& wasmCallerInfoAsCallee, const CallInformation& wasmCalleeInfoAsCallee, const ArgumentList& tmpArgSourceLocations, Vector<B3::ConstrainedValue> patchArgs) -> CallPatchpointData;

    bool canInline(FunctionSpaceIndex functionIndexSpace, unsigned callProfileIndex) const;
    PartialResult WARN_UNUSED_RETURN emitInlineDirectCall(FunctionCodeIndex calleeIndex, const TypeDefinition&, ArgumentList& args, ValueResults&);

    void dump(const ControlStack&, const Stack* expressionStack);
    void setParser(FunctionParser<OMGIRGenerator>* parser) { m_parser = parser; };
    ALWAYS_INLINE void willParseOpcode() { }
    ALWAYS_INLINE void willParseExtendedOpcode() { }
    ALWAYS_INLINE void didParseOpcode() { }
    void didFinishParsingLocals() { }
    void didPopValueFromStack(ExpressionType expr, ASCIILiteral message)
    {
        --m_stackSize;
        TRACE_VALUE(Wasm::Types::Void, get(expr), "pop at height: ", m_stackSize.value() + 1, " site: [", message, "], var ", *expr);
    }
    const Ref<TypeDefinition> getTypeDefinition(uint32_t typeIndex) { return m_info.typeSignatures[typeIndex]; }
    const ArrayType* getArrayTypeDefinition(uint32_t);
    void getArrayElementType(uint32_t, StorageType&);
    void getArrayRefType(uint32_t, Type&);

    Value* constant(B3::Type, uint64_t bits, std::optional<Origin> = std::nullopt);
    Value* constant(B3::Type, v128_t bits, std::optional<Origin> = std::nullopt);
    Value* framePointer();
    void insertEntrySwitch();
    void insertConstants();

    B3::Type toB3ResultType(const TypeDefinition*);

    void addStackMap(unsigned callSiteIndex, StackMap&& stackmap)
    {
        if (m_inlineParent) {
            m_inlineRoot->addStackMap(callSiteIndex, WTFMove(stackmap));
            return;
        }
        m_stackmaps.add(CallSiteIndex(callSiteIndex), WTFMove(stackmap));
    }

    StackMaps&& takeStackmaps()
    {
        RELEASE_ASSERT(m_inlineRoot == this);
        return WTFMove(m_stackmaps);
    }

    Vector<UnlinkedHandlerInfo>&& takeExceptionHandlers()
    {
        RELEASE_ASSERT(m_inlineRoot == this);
        return WTFMove(m_exceptionHandlers);
    }

private:
    void emitPrepareWasmOperation(BasicBlock* block)
    {
#if !USE(BUILTIN_FRAME_ADDRESS) || ASSERT_ENABLED
        // Prepare wasm operation calls.
        block->appendNew<B3::MemoryValue>(m_proc, B3::Store, origin(), framePointer(), instanceValue(), JSWebAssemblyInstance::offsetOfTemporaryCallFrame());
#else
        UNUSED_PARAM(block);
#endif
    }

    template<typename OperationType, typename ...Args>
    Value* callWasmOperation(BasicBlock* block, B3::Type resultType, OperationType operation, Args&&... args)
    {
        emitPrepareWasmOperation(block);
        Value* operationValue = block->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operation));
        return block->appendNew<CCallValue>(m_proc, resultType, origin(), operationValue, std::forward<Args>(args)...);
    }

    void emitExceptionCheck(CCallHelpers&, Origin, ExceptionType);

    void emitWriteBarrierForJSWrapper();
    void emitWriteBarrier(Value* cell, Value* instanceCell);
    Value* emitCheckAndPreparePointer(Value* pointer, uint32_t offset, uint32_t sizeOfOp);
    B3::Kind memoryKind(B3::Opcode memoryOp);
    Value* emitLoadOp(LoadOpType, Value* pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, Value* pointer, Value*, uint32_t offset);

    Value* sanitizeAtomicResult(ExtAtomicOpType, Type, Value* result);
    Value* emitAtomicLoadOp(ExtAtomicOpType, Type, Value* pointer, uint32_t offset);
    void emitAtomicStoreOp(ExtAtomicOpType, Type, Value* pointer, Value*, uint32_t offset);
    Value* emitAtomicBinaryRMWOp(ExtAtomicOpType, Type, Value* pointer, Value*, uint32_t offset);
    Value* emitAtomicCompareExchange(ExtAtomicOpType, Type, Value* pointer, Value* expected, Value*, uint32_t offset);

    Value* decodeNonNullStructure(Value* structureID);
    Value* encodeStructureID(Value* structure);

    Value* allocatorForWasmGCHeapCellSize(Value* size, BasicBlock* slowPath);
    Value* allocateWasmGCHeapCell(Value* allocator, BasicBlock* slowPath);
    Value* allocateWasmGCObject(Value* allocator, Value* structureID, Value* typeInfo, BasicBlock* slowPath);
    Value* allocateWasmGCArrayUninitialized(uint32_t typeIndex, Value* size);
    Value* allocateWasmGCStructUninitialized(uint32_t typeIndex);

    void mutatorFence();

    Value* emitGetArrayPayloadBase(Wasm::StorageType, Value*);
    Value* emitGetArraySizeWithNullCheck(Type arrayType, Value*);
    void emitNullCheck(Value*, ExceptionType);
    bool emitNullCheckBeforeAccess(Value*, ptrdiff_t offset);
    void emitArraySetUnchecked(uint32_t, Value*, Value*, Value*);
    bool emitArraySetUncheckedWithoutWriteBarrier(uint32_t, Value*, Value*, Value*);
    // Returns true if a writeBarrier/mutatorFence is needed.
    bool WARN_UNUSED_RETURN emitStructSet(bool canTrap, Value*, uint32_t, const StructType&, Value*);
    Value* WARN_UNUSED_RETURN allocateWasmGCArray(uint32_t typeIndex, Value* initValue, Value* size);
    using ArraySegmentOperation = EncodedJSValue SYSV_ABI (&)(JSC::JSWebAssemblyInstance*, uint32_t, uint32_t, uint32_t, uint32_t);
    ExpressionType WARN_UNUSED_RETURN pushArrayNewFromSegment(ArraySegmentOperation, uint32_t typeIndex, uint32_t segmentIndex, ExpressionType arraySize, ExpressionType offset, ExceptionType);
    void emitRefTestOrCast(CastKind, TypedExpression, bool, int32_t, bool, ExpressionType&);
    template <typename Generator>
    void emitCheckOrBranchForCast(CastKind, Value*, const Generator&, BasicBlock*);
    Value* emitLoadRTTFromObject(Value*);

    void unify(Value* phi, const ExpressionType source);
    void unifyValuesWithBlock(const Stack& resultStack, const ControlData& block);

    void emitChecksForModOrDiv(B3::Opcode, Value* left, Value* right);

    int32_t WARN_UNUSED_RETURN fixupPointerPlusOffset(Value*&, uint32_t);
    Value* WARN_UNUSED_RETURN fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType, Value*, uint32_t);

    void restoreWasmContextInstance(BasicBlock*, Value*);
    void restoreWebAssemblyGlobalState(const MemoryInformation&, Value* instance, BasicBlock*);
    void reloadMemoryRegistersFromInstance(const MemoryInformation&, Value* instance, BasicBlock*);

    enum OSRBufferMode {
        LoadI64, // Load I64 values directly, and let later passes lower them; Needed for interacting with lower tiers.
        SplitI64, // Split I64 values into two I32 values.
    };

    Value* loadFromScratchBuffer(OSRBufferMode, unsigned& indexInBuffer, Value* pointer, B3::Type);
    void connectControlAtEntrypoint(OSRBufferMode, unsigned& indexInBuffer, Value* pointer, ControlData&, Stack& expressionStack, ControlData& currentData, bool fillLoopPhis = false);
    Value* emitCatchImpl(CatchKind, ControlType&, unsigned exceptionIndex = 0);
    void emitCatchTableImpl(ControlData& entryData, const ControlData::TryTableTarget&);
    RefPtr<PatchpointExceptionHandle> preparePatchpointForExceptions(BasicBlock*, PatchpointValue*);
    void connectValuesForCatchEntrypoint(ControlData& catchData, Value* pointer);

    Origin origin();

    ExpressionType getPushVariable(B3::Type type)
    {
        ++m_stackSize;
        if (m_stackSize > m_maxStackSize) {
            m_maxStackSize = m_stackSize;
            Variable* var = m_proc.addVariable(type);
            if constexpr (WasmOMGIRGeneratorInternal::traceStackValues)
                set(var, constant(type, 0xBADBEEFEF));
            m_stack.append(var);
            return var;
        }

        if constexpr (WasmOMGIRGeneratorInternal::traceStackValues) {
            // When we push, everything else *should* be dead
            for (unsigned i = m_stackSize - 1; i < m_stack.size(); ++i)
                set(m_stack[i], constant(m_stack[i]->type(), 0xBADBEEFEF));
        }

        Variable* var = m_stack[m_stackSize - 1];
        if (var->type() == type)
            return var;

        var = m_proc.addVariable(type);
        m_stack[m_stackSize - 1] = var;
        return var;
    }

    ExpressionType push(Value* value)
    {
        Variable* var = getPushVariable(value->type());
        set(var, value);
        if constexpr (!WasmOMGIRGeneratorInternal::traceExecution)
            return var;
        String site;
#if ASSERT_ENABLED
        if constexpr (WasmOMGIRGeneratorInternal::traceExecutionIncludesConstructionSite)
            site = Value::generateCompilerConstructionSite();
#endif
        TRACE_VALUE(Wasm::Types::Void, get(var), "push to stack height ", m_stackSize.value(), " site: [", site, "] var ", *var);
        return var;
    }

    Value* get(BasicBlock* block, Variable* variable)
    {
        return block->appendNew<VariableValue>(m_proc, B3::Get, origin(), variable);
    }

    Value* get(Variable* variable)
    {
        return get(m_currentBlock, variable);
    }

    Value* set(BasicBlock* block, Variable* dst, Value* src)
    {
        return block->appendNew<VariableValue>(m_proc, B3::Set, origin(), dst, src);
    }

    Value* set(Variable* dst, Value* src)
    {
        return set(m_currentBlock, dst, src);
    }

    Value* set(Variable* dst, Variable* src)
    {
        return set(dst, get(src));
    }

    bool useSignalingMemory() const
    {
        return m_mode == MemoryMode::Signaling;
    }

    template<typename... Args>
    void traceValue(Type, Value*, Args&&... info);
    template<typename... Args>
    void traceCF(Args&&... info);

    FunctionParser<OMGIRGenerator>* m_parser { nullptr };
    AbstractHeapRepository& m_heaps;
    CompilationContext& m_context;
    Module& m_module;
    CalleeGroup& m_calleeGroup;
    const ModuleInformation& m_info;
    const IPIntCallee& m_profiledCallee;
    OptimizingJITCallee* m_callee;
    const MemoryMode m_mode { MemoryMode::BoundsChecking };
    const CompilationMode m_compilationMode;
    const FunctionCodeIndex m_functionIndex;
    const unsigned m_loopIndexForOSREntry { UINT_MAX };

    struct RootBlock {
        BasicBlock* block;
        bool usesSIMD;
    };

    Procedure& m_proc;
    Vector<RootBlock> m_rootBlocks;
    BasicBlock* m_topLevelBlock;
    BasicBlock* m_currentBlock { nullptr };

    // Only used when this is an inlined context
    BasicBlock* m_returnContinuation { nullptr };
    OMGIRGenerator* m_inlineRoot { nullptr };
    OMGIRGenerator* m_inlineParent { nullptr };
    Vector<Value*> m_inlinedArgs;
    Vector<Variable*> m_inlinedResults;
    unsigned m_inlineDepth { 0 };
    Checked<uint32_t> m_inlinedBytes { 0 };

    Vector<Variable*> m_locals;
    Vector<Variable*> m_stack;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    FixedBitVector& m_directCallees; // Note this includes call targets from functions we inline.
    unsigned* m_osrEntryScratchBufferSize;
    UncheckedKeyHashMap<ValueKey, Value*> m_constantPool;
    UncheckedKeyHashMap<const TypeDefinition*, B3::Type> m_tupleMap;
    InsertionSet m_constantInsertionValues;
    Value* m_framePointer { nullptr };
    bool m_makesCalls { false };
    bool m_makesTailCalls { false };

    // This tracks the maximum stack offset for a tail call, to be used in the stack overflow check.
    Checked<int32_t> m_tailCallStackOffsetFromFP { 0 };

    bool m_hasExceptionHandlers;

    Value* m_instanceValue { nullptr };
    Value* m_baseMemoryValue { nullptr };
    Value* m_boundsCheckingSizeValue { nullptr };

    Value* instanceValue()
    {
        return m_instanceValue;
    }

    Value* baseMemoryValue()
    {
        return m_baseMemoryValue;
    }

    Value* boundsCheckingSizeValue()
    {
        return m_boundsCheckingSizeValue;
    }

    uint32_t m_maxNumJSCallArguments { 0 };
    unsigned m_numImportFunctions;

    Checked<unsigned> m_tryCatchDepth { 0 };
    Checked<unsigned> m_callSiteIndex { 0 };
    Checked<unsigned> m_stackSize { 0 };
    Checked<unsigned> m_maxStackSize { 0 };
    StackMaps m_stackmaps;
    Vector<UnlinkedHandlerInfo> m_exceptionHandlers;

    RefPtr<B3::Air::PrologueGenerator> m_prologueGenerator;

    Vector<std::unique_ptr<OMGIRGenerator>> m_protectedInlineeGenerators;
    Vector<std::unique_ptr<FunctionParser<OMGIRGenerator>>> m_protectedInlineeParsers;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(OMGIRGenerator);

// Memory accesses in WebAssembly have unsigned 32-bit offsets, whereas they have signed 32-bit offsets in B3.
int32_t OMGIRGenerator::fixupPointerPlusOffset(Value*& ptr, uint32_t offset)
{
    if (static_cast<uint64_t>(offset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        uint32_t half = offset / 2;
        ptr = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), ptr, m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), half));
        return half + (offset % 2);
    }
    return offset;
}

void OMGIRGenerator::restoreWasmContextInstance(BasicBlock* block, Value* arg)
{
    // FIXME: Because WasmToWasm call clobbers wasmContextInstance register and does not restore it, we need to restore it in the caller side.
    // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
    PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(m_proc, B3::Void, Origin());
    Effects effects = Effects::none();
    effects.writesPinned = true;
    effects.reads = B3::HeapRange::top();
    patchpoint->effects = effects;
    patchpoint->clobberLate(RegisterSetBuilder(GPRInfo::wasmContextInstancePointer));
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([](CCallHelpers& jit, const StackmapGenerationParams& param) {
        jit.move(param[0].gpr(), GPRInfo::wasmContextInstancePointer);
    });
}

void OMGIRGenerator::computeStackCheckSize(bool& needsOverflowCheck, int32_t& checkSize)
{
    const Checked<int32_t> wasmFrameSize = m_proc.frameSize();
    const Checked<int32_t> wasmTailCallFrameSize = -m_tailCallStackOffsetFromFP;
    const unsigned minimumParentCheckSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(1024);
    const unsigned extraFrameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(std::max<uint32_t>(
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
        Checked<uint32_t>(m_maxNumJSCallArguments) * sizeof(Register) + JSCallingConvention::headerSizeInBytes
    ));

    checkSize = wasmFrameSize.value();
    bool frameSizeNeedsOverflowCheck = checkSize >= static_cast<int32_t>(minimumParentCheckSize);
    needsOverflowCheck = frameSizeNeedsOverflowCheck;

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
}

OMGIRGenerator::OMGIRGenerator(AbstractHeapRepository& heaps, CompilationContext& context, OMGIRGenerator& parentCaller, OMGIRGenerator& rootCaller, Module& module, CalleeGroup& calleeGroup, unsigned functionIndex, IPIntCallee& profiledCallee, BasicBlock* returnContinuation, Vector<Value*> args)
    : m_heaps(heaps)
    , m_context(context)
    , m_module(module)
    , m_calleeGroup(calleeGroup)
    , m_info(rootCaller.m_info)
    , m_profiledCallee(profiledCallee)
    , m_callee(parentCaller.m_callee)
    , m_mode(rootCaller.m_mode)
    , m_compilationMode(CompilationMode::OMGMode)
    , m_functionIndex(functionIndex)
    , m_loopIndexForOSREntry(-1)
    , m_proc(rootCaller.m_proc)
    , m_returnContinuation(returnContinuation)
    , m_inlineRoot(&rootCaller)
    , m_inlineParent(&parentCaller)
    , m_inlinedArgs(WTFMove(args))
    , m_inlineDepth(parentCaller.m_inlineDepth + 1)
    , m_unlinkedWasmToWasmCalls(rootCaller.m_unlinkedWasmToWasmCalls)
    , m_directCallees(rootCaller.m_directCallees)
    , m_osrEntryScratchBufferSize(nullptr)
    , m_constantInsertionValues(m_proc)
    , m_hasExceptionHandlers(m_profiledCallee.hasExceptionHandlers())
    , m_numImportFunctions(m_info.importFunctionCount())
    , m_tryCatchDepth(parentCaller.m_tryCatchDepth)
    , m_callSiteIndex(0)
{
    m_topLevelBlock = m_proc.addBlock();
    m_rootBlocks.append({ m_proc.addBlock(), m_info.usesSIMD(m_functionIndex) });
    m_currentBlock = m_rootBlocks[0].block;
    m_instanceValue = rootCaller.m_instanceValue;
    m_baseMemoryValue = rootCaller.m_baseMemoryValue;
    m_boundsCheckingSizeValue = rootCaller.m_boundsCheckingSizeValue;
    if (parentCaller.m_hasExceptionHandlers)
        m_hasExceptionHandlers = true;
}

OMGIRGenerator::OMGIRGenerator(AbstractHeapRepository& heaps, CompilationContext& context, Module& module, CalleeGroup& calleeGroup, const ModuleInformation& info, IPIntCallee& profiledCallee, OptimizingJITCallee& callee, Procedure& procedure, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, FixedBitVector& outgoingDirectCallees, unsigned& osrEntryScratchBufferSize, MemoryMode mode, CompilationMode compilationMode, unsigned functionIndex, unsigned loopIndexForOSREntry)
    : m_heaps(heaps)
    , m_context(context)
    , m_module(module)
    , m_calleeGroup(calleeGroup)
    , m_info(info)
    , m_profiledCallee(profiledCallee)
    , m_callee(&callee)
    , m_mode(mode)
    , m_compilationMode(compilationMode)
    , m_functionIndex(functionIndex)
    , m_loopIndexForOSREntry(loopIndexForOSREntry)
    , m_proc(procedure)
    , m_inlineRoot(this)
    , m_inlinedBytes(m_info.functionWasmSize(m_functionIndex))
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_directCallees(outgoingDirectCallees)
    , m_osrEntryScratchBufferSize(&osrEntryScratchBufferSize)
    , m_constantInsertionValues(m_proc)
    , m_hasExceptionHandlers(m_profiledCallee.hasExceptionHandlers())
    , m_numImportFunctions(info.importFunctionCount())
{
    m_topLevelBlock = m_proc.addBlock();
    m_rootBlocks.append({ m_proc.addBlock(), m_info.usesSIMD(m_functionIndex) });
    m_currentBlock = m_rootBlocks[0].block;

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623

    m_proc.pinRegister(GPRInfo::wasmContextInstancePointer);
#if OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
    m_proc.pinRegister(GPRInfo::wasmBaseMemoryPointer);
    if (mode == MemoryMode::BoundsChecking)
        m_proc.pinRegister(GPRInfo::wasmBoundsCheckingSizeRegister);
#endif
    if (info.memory) {
        m_proc.setWasmBoundsCheckGenerator([=, this](CCallHelpers& jit, WasmBoundsCheckValue* originValue, GPRReg pinnedGPR) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            switch (m_mode) {
            case MemoryMode::BoundsChecking:
                ASSERT_UNUSED(pinnedGPR, GPRInfo::wasmBoundsCheckingSizeRegister == pinnedGPR);
                break;
            case MemoryMode::Signaling:
                ASSERT_UNUSED(pinnedGPR, InvalidGPRReg == pinnedGPR);
                break;
            }
            this->emitExceptionCheck(jit, originValue->origin(), ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    {
        // FIXME: Because WasmToWasm call clobbers wasmContextInstance register and does not restore it, we need to restore it in the caller side.
        // This prevents us from using ArgumentReg to this (logically) immutable pinned register.

        B3::PatchpointValue* getInstance = m_topLevelBlock->appendNew<B3::PatchpointValue>(m_proc, pointerType(), Origin());
        getInstance->effects.writesPinned = false;
        getInstance->effects.readsPinned = true;
        getInstance->resultConstraints = { ValueRep::reg(GPRInfo::wasmContextInstancePointer) };
        getInstance->setGenerator([=] (CCallHelpers&, const B3::StackmapGenerationParams&) { });
        m_instanceValue = getInstance;

        if (!!m_info.memory) {
            if (useSignalingMemory() || m_info.memory.isShared()) {
                // Capacity and basePointer will not be changed in this case.
                if (m_mode == MemoryMode::BoundsChecking) {
                    B3::PatchpointValue* getBoundsCheckingSize = m_topLevelBlock->appendNew<B3::PatchpointValue>(m_proc, pointerType(), Origin());
                    getBoundsCheckingSize->effects.writesPinned = false;
                    getBoundsCheckingSize->effects.readsPinned = true;
#if OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
                    getBoundsCheckingSize->resultConstraints = { ValueRep::reg(GPRInfo::wasmBoundsCheckingSizeRegister) };
#else
                    getBoundsCheckingSize->resultConstraints = { ValueRep::SomeRegister };
#endif
                    getBoundsCheckingSize->setGenerator([=] (CCallHelpers&, const B3::StackmapGenerationParams&) { });
                    m_boundsCheckingSizeValue = getBoundsCheckingSize;
                }
                B3::PatchpointValue* getBaseMemory = m_topLevelBlock->appendNew<B3::PatchpointValue>(m_proc, pointerType(), Origin());
                getBaseMemory->effects.writesPinned = false;
                getBaseMemory->effects.readsPinned = true;
#if OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
                getBaseMemory->resultConstraints = { ValueRep::reg(GPRInfo::wasmBaseMemoryPointer) };
#else
                getBaseMemory->resultConstraints = { ValueRep::SomeRegister };
#endif
                getBaseMemory->setGenerator([=] (CCallHelpers&, const B3::StackmapGenerationParams&) { });
                m_baseMemoryValue = getBaseMemory;
            }
        }
    }

    m_prologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([=, this] (CCallHelpers& jit, B3::Air::Code& code) {
        RELEASE_ASSERT(m_callee);
        AllowMacroScratchRegisterUsage allowScratch(jit);

        if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
            int fi = this->m_functionIndex;
            jit.probeDebugSIMD([fi] (Probe::Context& context) {
                dataLogLn(" General Before Prologue, fucntion ", fi, " FP: ", RawHex(context.gpr<uint64_t>(GPRInfo::callFrameRegister)), " SP: ", RawHex(context.gpr<uint64_t>(MacroAssembler::stackPointerRegister)));
            });
        }

        code.emitDefaultPrologue(jit);
        GPRReg scratchGPR = wasmCallingConvention().prologueScratchGPRs[0];
        jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(m_callee)), scratchGPR);
        static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
        CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
        jit.storePtr(scratchGPR, calleeSlot.withOffset(PayloadOffset));
        jit.store32(CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag), calleeSlot.withOffset(TagOffset));
        jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
    });
    {
        B3::PatchpointValue* stackOverflowCheck = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, Void, Origin());
        stackOverflowCheck->appendSomeRegister(instanceValue());
        stackOverflowCheck->appendSomeRegister(framePointer());
        stackOverflowCheck->clobber(RegisterSetBuilder::macroClobberedGPRs());
        stackOverflowCheck->numGPScratchRegisters = 0;
        stackOverflowCheck->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            ASSERT(m_proc.frameSize() == params.proc().frameSize());
            int32_t checkSize = 0;
            bool needsOverflowCheck = false;
            computeStackCheckSize(needsOverflowCheck, checkSize);
            ASSERT(checkSize || !needsOverflowCheck);

            // This allows leaf functions to not do stack checks if their frame size is within
            // certain limits since their caller would have already done the check.
            if (needsOverflowCheck) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                GPRReg contextInstance = params[0].gpr();
                GPRReg fp = params[1].gpr();
                if (m_compilationMode == CompilationMode::OMGForOSREntryMode)
                    jit.checkWasmStackOverflow(contextInstance, CCallHelpers::TrustedImm32(checkSize), fp).linkThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(crashDueToOMGStackOverflowGenerator).code()), &jit);
                else {
                    auto failure = jit.checkWasmStackOverflow(contextInstance, CCallHelpers::TrustedImm32(checkSize), fp);
                    params.addLatePath(
                        [=](CCallHelpers& jit) {
                            failure.link(jit);
                            int32_t stackSpace = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(RegisterSetBuilder::calleeSaveRegisters().numberOfSetRegisters() * sizeof(Register));
                            ASSERT(static_cast<unsigned>(stackSpace) < Options::softReservedZoneSize());
                            jit.addPtr(CCallHelpers::TrustedImm32(-stackSpace), GPRInfo::callFrameRegister, MacroAssembler::stackPointerRegister);
                            jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ExceptionType::StackOverflow)), GPRInfo::argumentGPR1);
                            jit.nearCallThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromOMGThunkGenerator).code()));
                        });
                }
            }
        });
    }

    if (m_compilationMode == CompilationMode::OMGForOSREntryMode)
        m_currentBlock = m_proc.addBlock();
}

void OMGIRGenerator::restoreWebAssemblyGlobalState(const MemoryInformation& memory, Value* instance, BasicBlock* block)
{
    restoreWasmContextInstance(block, instance);
#if OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
    if (!!memory) {
        if (useSignalingMemory() || memory.isShared()) {
            RegisterSet clobbers;
            clobbers.add(GPRInfo::wasmBaseMemoryPointer, IgnoreVectors);
            if (m_mode == MemoryMode::BoundsChecking)
                clobbers.add(GPRInfo::wasmBoundsCheckingSizeRegister, IgnoreVectors);

            B3::PatchpointValue* patchpoint = block->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
            Effects effects = Effects::none();
            effects.writesPinned = true;
            effects.reads = B3::HeapRange::top();
            patchpoint->effects = effects;
            patchpoint->clobber(clobbers);

            patchpoint->append(baseMemoryValue(), ValueRep::SomeRegister);
            if (m_mode == MemoryMode::BoundsChecking)
                patchpoint->append(boundsCheckingSizeValue(), ValueRep::SomeRegister);
            patchpoint->setGenerator([](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                jit.move(params[0].gpr(), GPRInfo::wasmBaseMemoryPointer);
                if (params.size() == 2)
                    jit.move(params[1].gpr(), GPRInfo::wasmBoundsCheckingSizeRegister);
            });
            return;
        }

        reloadMemoryRegistersFromInstance(memory, instance, block);
    }
#else
    UNUSED_PARAM(memory);
#endif // OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
}

void OMGIRGenerator::reloadMemoryRegistersFromInstance(const MemoryInformation& memory, Value* instance, BasicBlock* block)
{
#if OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
    if (!!memory) {
        RegisterSet clobbers;
        clobbers.add(GPRInfo::wasmBaseMemoryPointer, IgnoreVectors);
        clobbers.add(GPRInfo::wasmBoundsCheckingSizeRegister, IgnoreVectors);
        clobbers.merge(RegisterSetBuilder::macroClobberedGPRs());

        B3::PatchpointValue* patchpoint = block->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        Effects effects = Effects::none();
        effects.writesPinned = true;
        effects.reads = B3::HeapRange::top();
        patchpoint->effects = effects;
        patchpoint->clobber(clobbers);
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->append(instance, ValueRep::SomeRegister);
        patchpoint->setGenerator([](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg scratch = params.gpScratch(0);
            jit.loadPairPtr(params[0].gpr(), CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
            jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, scratch);
        });
    }
#else
    UNUSED_PARAM(memory);
    UNUSED_PARAM(instance);
    UNUSED_PARAM(block);
#endif // OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
}

void OMGIRGenerator::emitExceptionCheck(CCallHelpers& jit, Origin, ExceptionType type)
{
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    jit.nearCallThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromOMGThunkGenerator).code()));
}

Value* OMGIRGenerator::constant(B3::Type type, uint64_t bits, std::optional<Origin> maybeOrigin)
{
    auto result = m_constantPool.ensure(ValueKey(opcodeForConstant(type), type, static_cast<int64_t>(bits)), [&] {
        Value* result = nullptr;
        if (type.kind() == B3::V128) {
            v128_t vector { };
            vector.u64x2[0] = bits;
            vector.u64x2[1] = 0;
            result = m_proc.addConstant(maybeOrigin ? *maybeOrigin : origin(), type, vector);
        } else
            result = m_proc.addConstant(maybeOrigin ? *maybeOrigin : origin(), type, bits);
        m_constantInsertionValues.insertValue(0, result);
        return result;
    });
    return result.iterator->value;
}

Value* OMGIRGenerator::constant(B3::Type type, v128_t bits, std::optional<Origin> maybeOrigin)
{
    Value* result = m_proc.addConstant(maybeOrigin ? *maybeOrigin : origin(), type, bits);
    m_constantInsertionValues.insertValue(0, result);
    return result;
}

Value* OMGIRGenerator::framePointer()
{
    if (!m_framePointer) {
        m_framePointer = m_proc.add<B3::Value>(B3::FramePointer, Origin());
        ASSERT(m_framePointer);
        m_constantInsertionValues.insertValue(0, m_framePointer);
    }
    return m_framePointer;
}

void OMGIRGenerator::insertEntrySwitch()
{
    m_proc.setNumEntrypoints(m_rootBlocks.size());

    Ref<B3::Air::PrologueGenerator> catchPrologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([] (CCallHelpers& jit, B3::Air::Code& code) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.addPtr(CCallHelpers::TrustedImm32(-code.frameSize()), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        jit.probe(tagCFunction<JITProbePtrTag>(code.usesSIMD() ? buildEntryBufferForCatchSIMD : buildEntryBufferForCatchNoSIMD), nullptr, code.usesSIMD() ? SavedFPWidth::SaveVectors : SavedFPWidth::DontSaveVectors);
    });

    Ref<B3::Air::PrologueGenerator> catchSIMDPrologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([] (CCallHelpers& jit, B3::Air::Code& code) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.addPtr(CCallHelpers::TrustedImm32(-code.frameSize()), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        jit.probe(tagCFunction<JITProbePtrTag>(buildEntryBufferForCatchSIMD), nullptr, SavedFPWidth::SaveVectors);
    });

    m_proc.code().setPrologueForEntrypoint(0, Ref<B3::Air::PrologueGenerator>(*m_prologueGenerator));
    for (unsigned i = 1; i < m_rootBlocks.size(); ++i)
        m_proc.code().setPrologueForEntrypoint(i, m_rootBlocks[i].usesSIMD ? catchSIMDPrologueGenerator.copyRef() : catchPrologueGenerator.copyRef());

    m_currentBlock = m_topLevelBlock;
    m_currentBlock->appendNew<Value>(m_proc, EntrySwitch, Origin());
    for (auto [block, _] : m_rootBlocks)
        m_currentBlock->appendSuccessor(FrequentedBlock(block));
}

void OMGIRGenerator::insertConstants()
{
    m_constantInsertionValues.execute(m_proc.at(0));
}

B3::Type OMGIRGenerator::toB3ResultType(const TypeDefinition* returnType)
{
    if (returnType->as<FunctionSignature>()->returnsVoid())
        return B3::Void;

#if USE(JSVALUE32_64)
    if (returnType->as<FunctionSignature>()->returnCount() == 1 && toB3Type(returnType->as<FunctionSignature>()->returnType(0)) == Int64)
        return m_proc.addTuple({ Int32, Int32 });
#endif

    if (returnType->as<FunctionSignature>()->returnCount() == 1)
        return toB3Type(returnType->as<FunctionSignature>()->returnType(0));

    auto result = m_tupleMap.ensure(returnType, [&] {
        Vector<B3::Type> result;
        Vector<B3::Type, 0> highBits;
        for (unsigned i = 0; i < returnType->as<FunctionSignature>()->returnCount(); ++i) {
            auto type = toB3Type(returnType->as<FunctionSignature>()->returnType(i));
#if USE(JSVALUE32_64)
            if (type == Int64) {
                result.append(Int32);
                highBits.append(Int32);
                continue;
            }
#endif
            result.append(type);
        }
#if USE(JSVALUE32_64)
        for (auto type : highBits)
            result.append(type);
#endif
        return m_proc.addTuple(WTFMove(result));
    });
    return result.iterator->value;
}

auto OMGIRGenerator::addLocal(Type type, uint32_t count) -> PartialResult
{
    size_t newSize = m_locals.size() + count;
    ASSERT(!(CheckedUint32(count) + m_locals.size()).hasOverflowed());
    ASSERT(newSize <= maxFunctionLocals);
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(newSize), "can't allocate memory for "_s, newSize, " locals"_s);

    m_locals.appendUsingFunctor(count, [&](size_t) {
        Variable* local = m_proc.addVariable(toB3Type(type));
        if (type.isV128())
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, constant(toB3Type(type), v128_t { }, Origin()));
        else {
            auto val = isRefType(type) ? JSValue::encode(jsNull()) : 0;
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, constant(toB3Type(type), val, Origin()));
        }
        return local;
    });
    return { };
}

auto OMGIRGenerator::addDrop(ExpressionType) -> PartialResult
{
    return { };
}

auto OMGIRGenerator::addInlinedArguments(const TypeDefinition& signature) -> PartialResult
{
    RELEASE_ASSERT(signature.as<FunctionSignature>()->argumentCount() == m_inlinedArgs.size());
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);

    for (size_t i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        B3::Type type = toB3Type(signature.as<FunctionSignature>()->argumentType(i));
        Value* value = m_inlinedArgs[i];
        RELEASE_ASSERT(value->type() == type);

        Variable* argumentVariable = m_proc.addVariable(type);
        m_locals[i] = argumentVariable;
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, value);
    }

    return { };
}

auto OMGIRGenerator::addArguments(const TypeDefinition& signature) -> PartialResult
{
    ASSERT(!m_locals.size());
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(signature.as<FunctionSignature>()->argumentCount()), "can't allocate memory for "_s, signature.as<FunctionSignature>()->argumentCount(), " arguments"_s);

    m_locals.grow(signature.as<FunctionSignature>()->argumentCount());

    if (m_inlineParent)
        return addInlinedArguments(signature);

    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);

    if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
        PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, Origin());
        patch->effects = Effects::none();
        patch->effects.controlDependent = true;
        patch->effects.fence = true;
        patch->effects.reads = HeapRange::top();
        patch->effects.writes = HeapRange::top();
        m_currentBlock->append(patch);
        patch->setGenerator([functionIndex = m_functionIndex, signature = signature.as<FunctionSignature>(), wasmCallInfo] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            jit.probeDebugSIMD([functionIndex, signature, wasmCallInfo] (Probe::Context& context) {
                dataLogLn(" General Add arguments, fucntion ", functionIndex, " FP: ", RawHex(context.gpr<uint64_t>(GPRInfo::callFrameRegister)), " SP: ", RawHex(context.gpr<uint64_t>(MacroAssembler::stackPointerRegister)));

                auto fpl = context.gpr<uint64_t*>(GPRInfo::callFrameRegister);
                auto fpi = context.gpr<uint32_t*>(GPRInfo::callFrameRegister);

                for (size_t i = 0; i < signature->argumentCount(); ++i) {
                    auto rep = wasmCallInfo.params[i];
                    auto src = rep.location;
                    auto width = rep.width;
                    dataLog("     Arg source ", i, " located at ", src, " = ");

                    if (src.isGPR()) {
                        dataLog(context.gpr(src.jsr().payloadGPR()), " / ", (int) context.gpr(src.jsr().payloadGPR()));
                        if (src.jsr().tagGPR())
                            dataLog(" Upper bits: ", context.gpr(src.jsr().tagGPR()), " / ", (int) context.gpr(src.jsr().tagGPR()));
                    } else if (src.isFPR() && width <= Width::Width64)
                        dataLog(context.fpr(src.fpr(), SavedFPWidth::DontSaveVectors));
                    else if (src.isFPR())
                        RELEASE_ASSERT_NOT_REACHED();
                    else
                        dataLog(fpl[src.offsetFromFP() / sizeof(uint64_t)], " / ", fpi[src.offsetFromFP() / sizeof(uint32_t)],  " / ", RawHex(fpi[src.offsetFromFP() / sizeof(uint32_t)]), " / ", std::bit_cast<double>(fpl[src.offsetFromFP() / sizeof(uint64_t)]), " at ", RawPointer(&fpi[src.offsetFromFP() / sizeof(uint32_t)]));
                    dataLogLn();
                }
            });
        });
    }

    for (size_t i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        B3::Type type = toB3Type(signature.as<FunctionSignature>()->argumentType(i));
        B3::Value* argument;
        auto rep = wasmCallInfo.params[i];
        if (rep.location.isGPR()) {
            if (type == Int32)
                argument = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.location.jsr().payloadGPR());
            else {
                ASSERT(type == Int64);
                ASSERT(rep.location.jsr().payloadGPR() != InvalidGPRReg);
                ASSERT(rep.location.jsr().tagGPR() != InvalidGPRReg);
                Value* argLo = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.location.jsr().payloadGPR());
                Value* argHi = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.location.jsr().tagGPR());
                argument = m_currentBlock->appendNew<Value>(m_proc, Stitch, Origin(), argLo, argHi);
            }
        } else if (rep.location.isFPR()) {
            if (type.isVector()) {
                ASSERT(rep.width == Width128);
                argument = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.location.fpr(), B3::ArgumentRegValue::UsesVectorArgs);
            } else {
                ASSERT(rep.width != Width128);
                argument = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.location.fpr());
            }
            if (type == B3::Float)
                argument = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), argument);
        } else {
            ASSERT(rep.location.isStack());
            B3::Value* address = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(),
                m_currentBlock->appendNew<B3::ConstPtrValue>(m_proc, Origin(), rep.location.offsetFromFP()));
            argument = m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Load, type, Origin(), address);
        }

        Variable* argumentVariable = m_proc.addVariable(argument->type());
        m_locals[i] = argumentVariable;
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, argument);
    }

    return { };
}

auto OMGIRGenerator::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Equal, origin(), get(value), m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), JSValue::encode(jsNull()))));
    return { };
}

auto OMGIRGenerator::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::Externref), operationGetWasmTableElement,
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), get(index));
    {
        result = push(resultValue);
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::addTableSet(unsigned tableIndex, ExpressionType index, ExpressionType value) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    auto shouldThrow = callWasmOperation(m_currentBlock, B3::Int32, operationSetWasmTableElement,
        instanceValue(), constant(Int32, tableIndex), get(index), get(value));
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), shouldThrow, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::addRefFunc(FunctionSpaceIndex index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    result = push(callWasmOperation(m_currentBlock, wasmRefType(), operationWasmRefFunc,
        instanceValue(), constant(toB3Type(Types::I32), index)));
    TRACE_VALUE(Wasm::Types::Funcref, get(result), "ref_func ", index);
    return { };
}

auto OMGIRGenerator::addRefAsNonNull(TypedExpression reference, ExpressionType& result) -> PartialResult
{
    auto value = get(reference);
    result = push(value);
    if (reference.type().isNullable())
        emitNullCheck(value, ExceptionType::NullRefAsNonNull);
    return { };
}

auto OMGIRGenerator::addRefEq(ExpressionType ref0, ExpressionType ref1, ExpressionType& result) -> PartialResult
{
    return addI64Eq(ref0, ref1, result);
}

auto OMGIRGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmTableInit,
        instanceValue(),
        constant(Int32, elementIndex),
        constant(Int32, tableIndex),
        get(dstOffset), get(srcOffset), get(length));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::addElemDrop(unsigned elementIndex) -> PartialResult
{
    callWasmOperation(m_currentBlock, B3::Void, operationWasmElemDrop,
        instanceValue(),
        constant(Int32, elementIndex));

    return { };
}

auto OMGIRGenerator::addTableSize(unsigned tableIndex, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    result = push(callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationGetWasmTableSize,
        instanceValue(), constant(Int32, tableIndex)));

    return { };
}

auto OMGIRGenerator::addTableGrow(unsigned tableIndex, ExpressionType fill, ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = push(callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmTableGrow,
        instanceValue(), constant(Int32, tableIndex), get(fill), get(delta)));

    return { };
}

auto OMGIRGenerator::addTableFill(unsigned tableIndex, ExpressionType offset, ExpressionType fill, ExpressionType count) -> PartialResult
{
    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmTableFill,
        instanceValue(), constant(Int32, tableIndex), get(offset), get(fill), get(count));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    Value* resultValue = callWasmOperation(
        m_currentBlock, toB3Type(Types::I32), operationWasmTableCopy,
        instanceValue(),
        constant(Int32, dstTableIndex),
        constant(Int32, srcTableIndex),
        get(dstOffset), get(srcOffset), get(length));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    ASSERT(m_locals[index]);
    result = push(m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, origin(), m_locals[index]));
    TRACE_VALUE(m_parser->typeOfLocal(index), get(result), "get_local ", index);
    return { };
}

auto OMGIRGenerator::addUnreachable() -> PartialResult
{
    B3::PatchpointValue* unreachable = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
    unreachable->setGenerator([this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::Unreachable);
    });
    unreachable->effects.terminal = true;
    return { };
}

auto OMGIRGenerator::addCrash() -> PartialResult
{
    B3::PatchpointValue* unreachable = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
    unreachable->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        jit.breakpoint();
    });
    unreachable->effects.terminal = true;
    return { };
}


static constexpr int tailCallPatchpointScratchOffsets[] = { CallFrameSlot::thisArgument * sizeof(Register), CallFrameSlot::argumentCountIncludingThis * sizeof(Register) };
static constexpr int tailCallPatchpointScratchCount = sizeof(tailCallPatchpointScratchOffsets) / sizeof(tailCallPatchpointScratchOffsets[0]);

void OMGIRGenerator::fillCallResults(Value* callResult, const TypeDefinition& signature, ValueResults& results)
{
    B3::Type returnType = toB3ResultType(&signature);
    switch (returnType.kind()) {
    case B3::Void: {
        break;
    }
    case B3::Tuple: {
        const Vector<B3::Type>& tuple = m_proc.tupleForType(returnType);
        auto logicalReturnCount = signature.as<FunctionSignature>()->returnCount();

        ASSERT(!isARM64() || logicalReturnCount == tuple.size());
        for (unsigned i = 0; i < logicalReturnCount; ++i) {
            if (toB3Type(signature.as<FunctionSignature>()->returnType(i)) == Int64) {
                int highBitsIndex = 0;
                for (unsigned j = 0; j < i; ++j) {
                    ASSERT(j < logicalReturnCount);
                    if (toB3Type(signature.as<FunctionSignature>()->returnType(j)) == Int64)
                        ++highBitsIndex;
                }
                auto lo = m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), Int32, callResult, i);
                auto hi = m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), Int32, callResult, logicalReturnCount + highBitsIndex);
                auto stitched = m_currentBlock->appendNew<Value>(m_proc, Stitch, origin(), lo, hi);
                results.append(stitched);
                continue;
            }
            results.append(m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), tuple[i], callResult, i));
        }
        break;
    }
    default: {
        results.append(callResult);
        break;
    }
    }
}

auto OMGIRGenerator::emitIndirectCall(Value* calleeInstance, Value* calleeCode, Value* boxedCalleeCallee, const TypeDefinition& signature, const ArgumentList& args, ValueResults& results, CallType callType) -> PartialResult
{
    const bool isTailCallRootCaller = callType == CallType::TailCall && !m_inlineParent;

    m_makesCalls = true;
    if (callType == CallType::TailCall)
        m_makesTailCalls = true;

    // Do a context switch if needed.
    {
        BasicBlock* continuation = m_proc.addBlock();
        BasicBlock* doContextSwitch = m_proc.addBlock();

        Value* isSameContextInstance = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(),
            calleeInstance, instanceValue());
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
            isSameContextInstance, FrequentedBlock(continuation), FrequentedBlock(doContextSwitch));

        PatchpointValue* patchpoint = doContextSwitch->appendNew<PatchpointValue>(m_proc, B3::Void, origin());
        patchpoint->effects.writesPinned = true;
        // We pessimistically assume we're calling something with BoundsChecking memory.
        // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
        patchpoint->clobber(RegisterSetBuilder::wasmPinnedRegisters());
        patchpoint->clobber(RegisterSetBuilder::macroClobberedGPRs());
        patchpoint->append(calleeInstance, ValueRep::SomeRegister);
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg calleeInstance = params[0].gpr();
            ASSERT(calleeInstance != GPRInfo::wasmBaseMemoryPointer);
            jit.storeWasmContextInstance(calleeInstance);
#if OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
            static_assert((GPRInfo::wasmBoundsCheckingSizeRegister == GPRReg::InvalidGPRReg) || (GPRInfo::wasmBoundsCheckingSizeRegister != GPRInfo::wasmBaseMemoryPointer));
            // FIXME: We should support more than one memory size register
            //   see: https://bugs.webkit.org/show_bug.cgi?id=162952
            ASSERT(GPRInfo::wasmBoundsCheckingSizeRegister != calleeInstance);
            GPRReg scratch = params.gpScratch(0);
            jit.loadPairPtr(calleeInstance, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
            jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, scratch);
#endif // OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
        });
        doContextSwitch->appendNewControlValue(m_proc, Jump, origin(), continuation);

        m_currentBlock = continuation;
    }

    const auto& callingConvention = wasmCallingConvention();
    CallInformation wasmCalleeInfo = callingConvention.callInformationFor(signature, CallRole::Caller);
    CallInformation wasmCalleeInfoAsCallee = callingConvention.callInformationFor(signature, CallRole::Callee);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCalleeInfo.headerAndArgumentStackSizeInBytes);
    if (isTailCallRootCaller)
        calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCalleeInfo.headerAndArgumentStackSizeInBytes * 2 + sizeof(Register));

    m_proc.requestCallArgAreaSizeInBytes(calleeStackSize);

    if (isTailCallRootCaller) {
        const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
        const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex).expand();
        CallInformation wasmCallerInfoAsCallee = callingConvention.callInformationFor(callerTypeDefinition, CallRole::Callee);

        auto [patchpoint, _, prepareForCall] = createTailCallPatchpoint(m_currentBlock, signature, wasmCallerInfoAsCallee, wasmCalleeInfoAsCallee, args, { });
        unsigned patchArgsIndex = patchpoint->reps().size();
        if (is32Bit()) {
            patchpoint->append(calleeCode, ValueRep::stackArgument(0));
            patchpoint->append(boxedCalleeCallee, ValueRep::stackArgument(8));
        } else {
            patchpoint->append(calleeCode, ValueRep(GPRInfo::nonPreservedNonArgumentGPR0));
            patchpoint->append(boxedCalleeCallee, ValueRep::SomeRegister);
        }
        patchArgsIndex += m_proc.resultCount(patchpoint->type());
        patchpoint->setGenerator([prepareForCall = prepareForCall, patchArgsIndex](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            prepareForCall->run(jit, params);
            GPRReg callTarget;
            GPRReg callee;
            if (is32Bit() && params[patchArgsIndex + 1].isStack()) {
                callee = MacroAssembler::addressTempRegister;
                jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister,
                    tailCallPatchpointScratchOffsets[1] - sizeof(CallerFrameAndPC)), callee);
            } else
                callee = params[patchArgsIndex + 1].gpr();

            jit.storeWasmCalleeToCalleeCallFrame(callee, sizeof(CallerFrameAndPC) - prologueStackPointerDelta());

            // Allow scratch after the callee is stored, which could be in the scratch register.
            AllowMacroScratchRegisterUsage allowScratch(jit);

            if (is32Bit() && params[patchArgsIndex].isStack()) {
                callTarget = MacroAssembler::addressTempRegister;
                jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister,
                    tailCallPatchpointScratchOffsets[0] - sizeof(CallerFrameAndPC)), callTarget);
            } else
                callTarget = params[patchArgsIndex].gpr();

            jit.farJump(callTarget, WasmEntryPtrTag);
        });
        return { };
    }

    auto [patchpoint, handle, prepareForCall] = createCallPatchpoint(m_currentBlock, signature, wasmCalleeInfo, args);
    // We need to clobber all potential pinned registers since we might be leaving the instance.
    // We pessimistically assume we're always calling something that is bounds checking so
    // because the wasm->wasm thunk unconditionally overrides the size registers.
    // FIXME: We should not have to do this, but the wasm->wasm stub assumes it can
    // use all the pinned registers as scratch: https://bugs.webkit.org/show_bug.cgi?id=172181
    patchpoint->clobberLate(RegisterSetBuilder::wasmPinnedRegisters());

    unsigned patchArgsIndex = patchpoint->reps().size();
    patchpoint->append(calleeCode, ValueRep::SomeRegister);
    patchpoint->append(boxedCalleeCallee, ValueRep::SomeRegister);
    patchArgsIndex += m_proc.resultCount(patchpoint->type());
    patchpoint->setGenerator([this, handle = handle, prepareForCall = prepareForCall, patchArgsIndex](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        if (prepareForCall)
            prepareForCall->run(jit, params);
        if (handle)
            handle->collectStackMap(this, params);

        jit.storeWasmCalleeToCalleeCallFrame(params[patchArgsIndex + 1].gpr());
        jit.call(params[patchArgsIndex].gpr(), WasmEntryPtrTag);
    });
    fillCallResults(patchpoint, signature, results);

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_currentBlock);

    return { };
}

auto OMGIRGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = push(callWasmOperation(m_currentBlock, Int32, operationGrowMemory,
        instanceValue(), get(delta)));

    restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_currentBlock);

    return { };
}

auto OMGIRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    static_assert(sizeof(std::declval<Memory*>()->size()) == sizeof(uintptr_t), "codegen relies on this size");

    Value* size = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedMemorySize()));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_cachedMemorySize, size);

    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    Value* numPages = m_currentBlock->appendNew<Value>(m_proc, ZShr, origin(), size, constant(Int32, shiftValue));

    result = push(int32OfPointer(numPages));

    return { };
}

auto OMGIRGenerator::addMemoryFill(ExpressionType dstAddress, ExpressionType target, ExpressionType count) -> PartialResult
{
    auto* memorySize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedMemorySize()));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_cachedMemorySize, memorySize);
    auto* dstAddressValue = pointerOfInt32(get(dstAddress));
    auto* targetValue = get(target);
    auto* countValue = pointerOfInt32(get(count));

    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Above, origin(), m_currentBlock->appendNew<Value>(m_proc, Add, origin(), dstAddressValue, countValue), memorySize);
    CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    check->setGenerator([=, this, origin = this->origin()](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });

    Value* memoryBase = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedMemory()));
    dstAddressValue = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), memoryBase, dstAddressValue);
    m_currentBlock->appendNew<BulkMemoryValue>(
        m_proc, MemoryFill, origin(),
        dstAddressValue,
        targetValue,
        countValue);
    return { };
}

auto OMGIRGenerator::addMemoryInit(unsigned dataSegmentIndex, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length) -> PartialResult
{
    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmMemoryInit,
        instanceValue(),
        constant(Int32, dataSegmentIndex),
        get(dstAddress), get(srcAddress), get(length));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count) -> PartialResult
{
    auto* memorySize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedMemorySize()));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_cachedMemorySize, memorySize);
    auto* dstAddressValue = pointerOfInt32(get(dstAddress));
    auto* srcAddressValue = pointerOfInt32(get(srcAddress));
    auto* countValue = pointerOfInt32(get(count));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), m_currentBlock->appendNew<Value>(m_proc, Above, origin(), m_currentBlock->appendNew<Value>(m_proc, Add, origin(), dstAddressValue, countValue), memorySize));
        check->setGenerator([=, this, origin = this->origin()](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), m_currentBlock->appendNew<Value>(m_proc, Above, origin(), m_currentBlock->appendNew<Value>(m_proc, Add, origin(), srcAddressValue, countValue), memorySize));
        check->setGenerator([=, this, origin = this->origin()](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    Value* memoryBase = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedMemory()));
    dstAddressValue = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), memoryBase, dstAddressValue);
    srcAddressValue = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), memoryBase, srcAddressValue);
    m_currentBlock->appendNew<BulkMemoryValue>(
        m_proc, MemoryCopy, origin(),
        dstAddressValue,
        srcAddressValue,
        countValue);
    return { };
}

auto OMGIRGenerator::addDataDrop(unsigned dataSegmentIndex) -> PartialResult
{
    callWasmOperation(m_currentBlock, B3::Void, operationWasmDataDrop,
        instanceValue(),
        constant(Int32, dataSegmentIndex));

    return { };
}

template<typename... Args>
void OMGIRGenerator::traceValue(Type type, Value* value, Args&&... info)
{
    if constexpr (!WasmOMGIRGeneratorInternal::traceExecution)
        return;
    if (!type.isFuncref() && !type.isVoid())
        return;
    auto* patch = m_proc.add<PatchpointValue>(B3::Void, origin());
    patch->effects = Effects::none();
    patch->effects.controlDependent = true;
    patch->effects.fence = true;
    patch->effects.reads = HeapRange::top();
    patch->effects.writes = HeapRange::top();
    StringPrintStream sb;
    if (m_parser->unreachableBlocks())
        sb.print("(unreachable) ");
    sb.print("TRACE OMG EXECUTION fn[", m_functionIndex, "] stack height ", m_stackSize.value(), " type ", type, " ");
    sb.print(info...);
    dataLogLn("static: ", sb.toString());
    patch->setGenerator([infoString = sb.toString(), type] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        JIT_COMMENT(jit, "PROBE FOR ", infoString);
        jit.probeDebug([params, type, infoString](Probe::Context& ctx) {
            auto rep = params[0];
            uint64_t rawVal = 0;
            ASSERT(rep.isGPR() || rep.isFPR());
            if (rep.isGPR())
                rawVal = ctx.gpr(rep.gpr());
            else if (rep.isFPR())
                rawVal = ctx.fpr(rep.fpr());
            else if (rep.isConstant())
                rawVal = rep.value();

            dataLogLn(infoString, " = ", rawVal);

            if (type.isVoid() || !rawVal)
                return;

            JSValue jsValue = JSValue::decode(rawVal);
            RELEASE_ASSERT(jsValue.isCallable() || jsValue.isUndefinedOrNull());
        });
    });
    patch->append(ConstrainedValue(value, ValueRep::SomeRegister));
    m_currentBlock->append(patch);
}

template<typename... Args>
void OMGIRGenerator::traceCF(Args&&... info)
{
    if constexpr (!WasmOMGIRGeneratorInternal::traceExecution)
        return;
    auto* patch = m_proc.add<PatchpointValue>(B3::Void, origin());
    patch->effects = Effects::none();
    patch->effects.controlDependent = true;
    patch->effects.fence = true;
    patch->effects.reads = HeapRange::top();
    patch->effects.writes = HeapRange::top();
    StringPrintStream sb;
    sb.print("TRACE OMG EXECUTION fn[", m_functionIndex, " <inlined into ", m_inlineParent ? m_inlineParent->m_functionIndex : -1, ">] stack height ", m_stackSize.value(), " CF ");
    sb.print(info...);
    dataLogLn("static: ", sb.toString());
    patch->setGenerator([infoString = sb.toString()] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        JIT_COMMENT(jit, "PROBE FOR ", infoString);
        jit.probeDebug([params, infoString](Probe::Context&) {
            dataLogLn(infoString);
        });
    });
    m_currentBlock->append(patch);

    if (!WasmOMGIRGeneratorInternal::traceStackValues)
        return;
    int i = 0;
    for (auto* val : m_stack) {
        ++i;
        traceValue(Wasm::Types::Void, get(val), " wasm stack[", i, "] = ", *val);
    }

    i = 0;
    for (auto val : m_parser->expressionStack()) {
        ++i;
        traceValue(Wasm::Types::Void, get(val.value()), " parser stack[", i, "] = ", *val.value());
    }

    if (m_parser->unreachableBlocks())
        return;
    if (m_parser->expressionStack().isEmpty() && m_stackSize) {
        dataLogLn("%%%%%%%%%%%%%%%%%%%");
        return;
    }
    if (!m_parser->expressionStack().isEmpty() && !m_stackSize) {
        dataLogLn("$$$$$$$$$$$$$$$$$$$");
        return;
    }
    for (i = 0; i < (int) m_parser->expressionStack().size(); ++i) {
        if (m_parser->expressionStack()[m_parser->expressionStack().size() - i - 1] != m_stack[m_stackSize.value() - i - 1]) {
            dataLogLn("************************");
            return;
        }
    }
}

auto OMGIRGenerator::setLocal(uint32_t index, ExpressionType value) -> PartialResult
{
    ASSERT(m_locals[index]);
    m_currentBlock->appendNew<VariableValue>(m_proc, B3::Set, origin(), m_locals[index], get(value));
    TRACE_VALUE(m_parser->typeOfLocal(index), get(value), "set_local ", index);
    return { };
}

auto OMGIRGenerator::teeLocal(uint32_t index, ExpressionType value, ExpressionType& result) -> PartialResult
{
    ASSERT(m_locals[index]);
    Value* input = get(value);
    m_currentBlock->appendNew<VariableValue>(m_proc, B3::Set, origin(), m_locals[index], input);
    result = push(input);
    TRACE_VALUE(m_parser->typeOfLocal(index), input, "tee_local ", index);
    return { };
}

auto OMGIRGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(global.type), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGlobal(m_info, index)));
        switch (global.type.kind) {
        case TypeKind::I32:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_i32[index], value);
            break;
        case TypeKind::F32:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_f32[index], value);
            break;
        case TypeKind::I64:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_i64[index], value);
            break;
        case TypeKind::F64:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_f64[index], value);
            break;
        case TypeKind::V128:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_v128[index], value);
            break;
        default:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_ref[index], value);
            break;
        }
        if (global.mutability == Mutability::Immutable) {
            value->setReadsMutability(B3::Mutability::Immutable);
            value->setControlDependent(false);
        }
        result = push(value);
        break;
    }
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        auto* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGlobal(m_info, index)));
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_portableGlobals[index], pointer);
        pointer->setReadsMutability(B3::Mutability::Immutable);
        pointer->setControlDependent(false);

        auto* load = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(global.type), origin(), pointer, Wasm::Global::Value::offsetOfValue());
        m_heaps.decorateMemory(&m_heaps.WasmGlobalValue_value, load);

        result = push(load);
        break;
    }
    }
    TRACE_VALUE(global.type, get(result), "get_global ", index);

    return { };
}

auto OMGIRGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    ASSERT(toB3Type(global.type) == value->type());
    TRACE_VALUE(global.type, get(value), "set_global ", index);

    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
        auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), get(value), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGlobal(m_info, index)));
        switch (global.type.kind) {
        case TypeKind::I32:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_i32[index], store);
            break;
        case TypeKind::F32:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_f32[index], store);
            break;
        case TypeKind::I64:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_i64[index], store);
            break;
        case TypeKind::F64:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_f64[index], store);
            break;
        case TypeKind::V128:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_v128[index], store);
            break;
        default:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_embeddedGlobals_ref[index], store);
            break;
        }
        if (isRefType(global.type))
            emitWriteBarrierForJSWrapper();
        break;
    }
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        auto* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGlobal(m_info, index)));
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_portableGlobals[index], pointer);
        pointer->setReadsMutability(B3::Mutability::Immutable);
        pointer->setControlDependent(false);

        auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), get(value), static_cast<Value*>(pointer), Wasm::Global::Value::offsetOfValue());
        m_heaps.decorateMemory(&m_heaps.WasmGlobalValue_value, store);

        // We emit a write-barrier onto JSWebAssemblyGlobal, not JSWebAssemblyInstance.
        if (isRefType(global.type)) {
            auto* cell = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), pointer, Wasm::Global::Value::offsetOfOwner());
            m_heaps.decorateMemory(&m_heaps.WasmGlobalValue_owner, cell);
            cell->setControlDependent(false);

            Value* cellState = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
            m_heaps.decorateMemory(&m_heaps.JSCell_cellState, cellState);

            auto* vm = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfVM()));
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_vm, vm);
            vm->setControlDependent(false);

            Value* threshold = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapBarrierThreshold()));
            m_heaps.decorateMemory(&m_heaps.VM_heap_barrierThreshold, threshold);

            BasicBlock* fenceCheckPath = m_proc.addBlock();
            BasicBlock* fencePath = m_proc.addBlock();
            BasicBlock* doSlowPath = m_proc.addBlock();
            BasicBlock* continuation = m_proc.addBlock();

            m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellState, threshold),
                FrequentedBlock(continuation), FrequentedBlock(fenceCheckPath, FrequencyClass::Rare));
            fenceCheckPath->addPredecessor(m_currentBlock);
            continuation->addPredecessor(m_currentBlock);
            m_currentBlock = fenceCheckPath;

            Value* shouldFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapMutatorShouldBeFenced()));
            m_heaps.decorateMemory(&m_heaps.VM_heap_mutatorShouldBeFenced, shouldFence);

            m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
                shouldFence,
                FrequentedBlock(fencePath), FrequentedBlock(doSlowPath));
            fencePath->addPredecessor(m_currentBlock);
            doSlowPath->addPredecessor(m_currentBlock);
            m_currentBlock = fencePath;

            B3::PatchpointValue* doFence = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
            doFence->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                jit.memoryFence();
            });

            Value* cellStateLoadAfterFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
            m_heaps.decorateMemory(&m_heaps.JSCell_cellState, cellStateLoadAfterFence);

            m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellStateLoadAfterFence, constant(Int32, blackThreshold)),
                FrequentedBlock(continuation), FrequentedBlock(doSlowPath, FrequencyClass::Rare));
            doSlowPath->addPredecessor(m_currentBlock);
            continuation->addPredecessor(m_currentBlock);
            m_currentBlock = doSlowPath;

            callWasmOperation(m_currentBlock, B3::Void, operationWasmWriteBarrierSlowPath, cell, vm);
            m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

            continuation->addPredecessor(m_currentBlock);
            m_currentBlock = continuation;
        }
        break;
    }
    }
    return { };
}

inline void OMGIRGenerator::emitWriteBarrierForJSWrapper()
{
    emitWriteBarrier(instanceValue(), instanceValue());
}

inline void OMGIRGenerator::emitWriteBarrier(Value* cell, Value* instanceCell)
{
    Value* cellState = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
    m_heaps.decorateMemory(&m_heaps.JSCell_cellState, cellState);

    auto* vm = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceCell, safeCast<int32_t>(JSWebAssemblyInstance::offsetOfVM()));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_vm, vm);
    vm->setControlDependent(false);

    Value* threshold = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapBarrierThreshold()));
    m_heaps.decorateMemory(&m_heaps.VM_heap_barrierThreshold, threshold);

    BasicBlock* fenceCheckPath = m_proc.addBlock();
    BasicBlock* fencePath = m_proc.addBlock();
    BasicBlock* doSlowPath = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
        m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellState, threshold),
        FrequentedBlock(continuation), FrequentedBlock(fenceCheckPath, FrequencyClass::Rare));
    fenceCheckPath->addPredecessor(m_currentBlock);
    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = fenceCheckPath;

    Value* shouldFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapMutatorShouldBeFenced()));
    m_heaps.decorateMemory(&m_heaps.VM_heap_mutatorShouldBeFenced, shouldFence);

    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
        shouldFence,
        FrequentedBlock(fencePath), FrequentedBlock(doSlowPath));
    fencePath->addPredecessor(m_currentBlock);
    doSlowPath->addPredecessor(m_currentBlock);
    m_currentBlock = fencePath;

    B3::PatchpointValue* doFence = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
    doFence->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        jit.memoryFence();
    });

    Value* cellStateLoadAfterFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
    m_heaps.decorateMemory(&m_heaps.JSCell_cellState, cellStateLoadAfterFence);

    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
        m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellStateLoadAfterFence, constant(Int32, blackThreshold)),
        FrequentedBlock(continuation), FrequentedBlock(doSlowPath, FrequencyClass::Rare));
    doSlowPath->addPredecessor(m_currentBlock);
    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = doSlowPath;

    callWasmOperation(m_currentBlock, B3::Void, operationWasmWriteBarrierSlowPath, cell, vm);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = continuation;
}

inline Value* OMGIRGenerator::emitCheckAndPreparePointer(Value* pointer, uint32_t offset, uint32_t sizeOfOperation)
{
    switch (m_mode) {
    case MemoryMode::BoundsChecking: {
        // We're not using signal handling only when the memory is not shared.
        // Regardless of signaling, we must check that no memory access exceeds the current memory size.
        ASSERT(sizeOfOperation + offset > offset);
        Value* pointerPlusOffset;
        if (offset) {
            Value* fixedUpPointer = pointer;
            offset = fixupPointerPlusOffset(fixedUpPointer, offset);
            if (offset) {
                Value* offsetValue = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), offset);
                pointerPlusOffset = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), fixedUpPointer, offsetValue);
            } else
                pointerPlusOffset = pointer;
        } else
            pointerPlusOffset = pointer;
        Value* sizeValue = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), sizeOfOperation);
        Value* highestAccess = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), pointerPlusOffset, sizeValue);
        // Test that we didn't overflow.
        CheckValue* checkOverflow = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), pointer, highestAccess));
        checkOverflow->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
        // Test that we're within bounds.
        Value* boundsCheckingSize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedBoundsCheckingSize()));
        Value* isWithinBounds = m_currentBlock->appendNew<Value>(m_proc, Above, origin(), highestAccess, boundsCheckingSize);
        CheckValue* checkBounds = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), isWithinBounds);
        checkBounds->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
        break;
    }

    case MemoryMode::Signaling: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    }

    Value* memoryBase = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedMemory()));
    return m_currentBlock->appendNew<Value>(m_proc, Add, origin(), memoryBase, pointer);
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

inline B3::Kind OMGIRGenerator::memoryKind(B3::Opcode memoryOp)
{
    if (useSignalingMemory() || m_info.memory.isShared())
        return trapping(memoryOp);
    return memoryOp;
}

inline Value* OMGIRGenerator::emitLoadOp(LoadOpType op, Value* pointer, uint32_t uoffset)
{
    int32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    switch (op) {
    case LoadOpType::I32Load8S: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8S), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return value;
    }

    case LoadOpType::I64Load8S: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8S), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin(), value);
    }

    case LoadOpType::I32Load8U: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8Z), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return value;
    }

    case LoadOpType::I64Load8U: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8Z), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), value);
    }

    case LoadOpType::I32Load16S: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16S), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return value;
    }

    case LoadOpType::I64Load16S: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16S), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin(), value);
    }

    case LoadOpType::I32Load16U: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16Z), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return value;
    }

    case LoadOpType::I64Load16U: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16Z), origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), value);
    }

    case LoadOpType::I32Load: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return value;
    }

    case LoadOpType::I64Load32U: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), value);
    }

    case LoadOpType::I64Load32S: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin(), value);
    }

    case LoadOpType::I64Load: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int64, origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return value;
    }

    // This is ARMv7-specific; loading an F32/F64 from an unaligned address can
    // fault, so instead we load an Int32/Int64 (since Int loads from unaligned
    // accesses are OK) and convert it to FP.
    case LoadOpType::F32Load: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, BitwiseCast, origin(), value);
    }

    case LoadOpType::F64Load: {
        auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int64, origin(), pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
        return m_currentBlock->appendNew<Value>(m_proc, BitwiseCast, origin(), value);
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto OMGIRGenerator::load(LoadOpType op, ExpressionType pointerVar, ExpressionType& result, uint32_t offset) -> PartialResult
{
    Value* pointer = get(pointerVar);
    ASSERT(pointer->type() == Int32);

    if (sumOverflows<uint32_t>(offset, sizeOfLoadOp(op))) [[unlikely]] {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (op) {
        case LoadOpType::I32Load8S:
        case LoadOpType::I32Load16S:
        case LoadOpType::I32Load:
        case LoadOpType::I32Load16U:
        case LoadOpType::I32Load8U:
            result = push(constant(Int32, 0));
            break;
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
        case LoadOpType::I64Load16S:
        case LoadOpType::I64Load32U:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load:
        case LoadOpType::I64Load16U:
            result = push(constant(Int64, 0));
            break;
        case LoadOpType::F32Load:
            result = push(constant(Float, 0));
            break;
        case LoadOpType::F64Load:
            result = push(constant(Double, 0));
            break;
        }

    } else
        result = push(emitLoadOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfLoadOp(op)), offset));

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


inline void OMGIRGenerator::emitStoreOp(StoreOpType op, Value* pointer, Value* value, uint32_t uoffset)
{
    int32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    switch (op) {
    case StoreOpType::I64Store8:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
        [[fallthrough]];

    case StoreOpType::I32Store8: {
        auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store8), origin(), value, pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, store);
        return;
    }

    case StoreOpType::I64Store16:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
        [[fallthrough]];

    case StoreOpType::I32Store16: {
        auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store16), origin(), value, pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, store);
        return;
    }

    case StoreOpType::I64Store32:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
        [[fallthrough]];

    case StoreOpType::I64Store:
    case StoreOpType::I32Store:
    case StoreOpType::F32Store:
    case StoreOpType::F64Store: {
        auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store), origin(), value, pointer, offset);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, store);
        return;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto OMGIRGenerator::store(StoreOpType op, ExpressionType pointerVar, ExpressionType valueVar, uint32_t offset) -> PartialResult
{
    Value* pointer = get(pointerVar);
    Value* value = get(valueVar);
    ASSERT(pointer->type() == Int32);

    if (sumOverflows<uint32_t>(offset, sizeOfStoreOp(op))) [[unlikely]] {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
    } else
        emitStoreOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfStoreOp(op)), value, offset);

    return { };
}

inline Width accessWidth(ExtAtomicOpType op)
{
    return widthForBytes(1 << memoryLog2Alignment(op));
}

inline uint32_t sizeOfAtomicOpMemoryAccess(ExtAtomicOpType op)
{
    return bytesForWidth(accessWidth(op));
}

inline Value* OMGIRGenerator::sanitizeAtomicResult(ExtAtomicOpType op, Type valueType, Value* result)
{
    auto sanitize32 = [&](Value* result) {
        switch (accessWidth(op)) {
        case Width8:
            return m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), result, constant(Int32, 0xff));
        case Width16:
            return m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), result, constant(Int32, 0xffff));
        default:
            return result;
        }
    };

    switch (valueType.kind) {
    case TypeKind::I64: {
        if (accessWidth(op) == Width64)
            return result;
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), sanitize32(result));
    }
    case TypeKind::I32:
        return sanitize32(result);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

Value* OMGIRGenerator::fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType op, Value* ptr, uint32_t offset)
{
    offset = fixupPointerPlusOffset(ptr, offset);
    auto pointer = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), ptr, m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), offset));
    if (accessWidth(op) != Width8) {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), pointer, constant(pointerType(), sizeOfAtomicOpMemoryAccess(op) - 1)));
        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::UnalignedMemoryAccess);
        });
    }
    return pointer;
}

inline Value* OMGIRGenerator::emitAtomicLoadOp(ExtAtomicOpType op, Type valueType, Value* pointer, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    Value* value = nullptr;
    switch (accessWidth(op)) {
    case Width8:
    case Width16:
    case Width32:
        value = constant(Int32, 0);
        break;
    case Width64:
        value = constant(Int64, 0);
        break;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    auto* atomic = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicXchgAdd), origin(), accessWidth(op), value, pointer);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, atomic);
    m_heaps.decorateFencedAccess(&m_heaps.WebAssemblyMemory, atomic);
    return sanitizeAtomicResult(op, valueType, atomic);
}

auto OMGIRGenerator::atomicLoad(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op))) [[unlikely]] {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (valueType.kind) {
        case TypeKind::I32:
            result = push(constant(Int32, 0));
            break;
        case TypeKind::I64:
            result = push(constant(Int64, 0));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = push(emitAtomicLoadOp(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), offset));

    return { };
}

inline void OMGIRGenerator::emitAtomicStoreOp(ExtAtomicOpType op, Type valueType, Value* pointer, Value* value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    if (valueType.isI64() && accessWidth(op) != Width64)
        value = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), value);
    auto* atomic = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicXchg), origin(), accessWidth(op), value, pointer);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, atomic);
    m_heaps.decorateFencedAccess(&m_heaps.WebAssemblyMemory, atomic);
}

auto OMGIRGenerator::atomicStore(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op))) [[unlikely]] {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
    } else
        emitAtomicStoreOp(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), get(value), offset);

    return { };
}

inline Value* OMGIRGenerator::emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, Value* pointer, Value* value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    B3::Opcode opcode = B3::Nop;
    switch (op) {
    case ExtAtomicOpType::I32AtomicRmw8AddU:
    case ExtAtomicOpType::I32AtomicRmw16AddU:
    case ExtAtomicOpType::I32AtomicRmwAdd:
    case ExtAtomicOpType::I64AtomicRmw8AddU:
    case ExtAtomicOpType::I64AtomicRmw16AddU:
    case ExtAtomicOpType::I64AtomicRmw32AddU:
    case ExtAtomicOpType::I64AtomicRmwAdd:
        opcode = AtomicXchgAdd;
        break;
    case ExtAtomicOpType::I32AtomicRmw8SubU:
    case ExtAtomicOpType::I32AtomicRmw16SubU:
    case ExtAtomicOpType::I32AtomicRmwSub:
    case ExtAtomicOpType::I64AtomicRmw8SubU:
    case ExtAtomicOpType::I64AtomicRmw16SubU:
    case ExtAtomicOpType::I64AtomicRmw32SubU:
    case ExtAtomicOpType::I64AtomicRmwSub:
        opcode = AtomicXchgSub;
        break;
    case ExtAtomicOpType::I32AtomicRmw8AndU:
    case ExtAtomicOpType::I32AtomicRmw16AndU:
    case ExtAtomicOpType::I32AtomicRmwAnd:
    case ExtAtomicOpType::I64AtomicRmw8AndU:
    case ExtAtomicOpType::I64AtomicRmw16AndU:
    case ExtAtomicOpType::I64AtomicRmw32AndU:
    case ExtAtomicOpType::I64AtomicRmwAnd:
        opcode = AtomicXchgAnd;
        break;
    case ExtAtomicOpType::I32AtomicRmw8OrU:
    case ExtAtomicOpType::I32AtomicRmw16OrU:
    case ExtAtomicOpType::I32AtomicRmwOr:
    case ExtAtomicOpType::I64AtomicRmw8OrU:
    case ExtAtomicOpType::I64AtomicRmw16OrU:
    case ExtAtomicOpType::I64AtomicRmw32OrU:
    case ExtAtomicOpType::I64AtomicRmwOr:
        opcode = AtomicXchgOr;
        break;
    case ExtAtomicOpType::I32AtomicRmw8XorU:
    case ExtAtomicOpType::I32AtomicRmw16XorU:
    case ExtAtomicOpType::I32AtomicRmwXor:
    case ExtAtomicOpType::I64AtomicRmw8XorU:
    case ExtAtomicOpType::I64AtomicRmw16XorU:
    case ExtAtomicOpType::I64AtomicRmw32XorU:
    case ExtAtomicOpType::I64AtomicRmwXor:
        opcode = AtomicXchgXor;
        break;
    case ExtAtomicOpType::I32AtomicRmw8XchgU:
    case ExtAtomicOpType::I32AtomicRmw16XchgU:
    case ExtAtomicOpType::I32AtomicRmwXchg:
    case ExtAtomicOpType::I64AtomicRmw8XchgU:
    case ExtAtomicOpType::I64AtomicRmw16XchgU:
    case ExtAtomicOpType::I64AtomicRmw32XchgU:
    case ExtAtomicOpType::I64AtomicRmwXchg:
        opcode = AtomicXchg;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (valueType.isI64() && accessWidth(op) != Width64)
        value = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), value);

    auto* atomic = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(opcode), origin(), accessWidth(op), value, pointer);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, atomic);
    m_heaps.decorateFencedAccess(&m_heaps.WebAssemblyMemory, atomic);
    return sanitizeAtomicResult(op, valueType, atomic);
}

auto OMGIRGenerator::atomicBinaryRMW(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op))) [[unlikely]] {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (valueType.kind) {
        case TypeKind::I32:
            result = push(constant(Int32, 0));
            break;
        case TypeKind::I64:
            result = push(constant(Int64, 0));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = push(emitAtomicBinaryRMWOp(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), get(value), offset));

    return { };
}

Value* OMGIRGenerator::emitAtomicCompareExchange(ExtAtomicOpType op, Type valueType, Value* pointer, Value* expected, Value* value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    Width accessWidth = Wasm::accessWidth(op);

    if (widthForType(toB3Type(valueType)) == accessWidth) {
        auto* atomic = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicStrongCAS), origin(), accessWidth, expected, value, pointer);
        m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, atomic);
        m_heaps.decorateFencedAccess(&m_heaps.WebAssemblyMemory, atomic);
        return sanitizeAtomicResult(op, valueType, atomic);
    }

    Value* maximum = nullptr;
    switch (valueType.kind) {
    case TypeKind::I64: {
        switch (accessWidth) {
        case Width8:
            maximum = constant(Int64, UINT8_MAX);
            break;
        case Width16:
            maximum = constant(Int64, UINT16_MAX);
            break;
        case Width32:
            maximum = constant(Int64, UINT32_MAX);
            break;
        case Width64:
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    }
    case TypeKind::I32:
        switch (accessWidth) {
        case Width8:
            maximum = constant(Int32, UINT8_MAX);
            break;
        case Width16:
            maximum = constant(Int32, UINT16_MAX);
            break;
        case Width32:
        case Width64:
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    auto truncatedExpected = expected;
    auto truncatedValue = value;

    truncatedExpected = m_currentBlock->appendNew<B3::Value>(m_proc, B3::BitAnd, origin(), maximum, expected);

    if (valueType.isI64()) {
        truncatedExpected = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), expected);
        truncatedValue = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), value);
    }

    auto* atomic = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicStrongCAS), origin(), accessWidth, truncatedExpected, truncatedValue, pointer);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, atomic);
    m_heaps.decorateFencedAccess(&m_heaps.WebAssemblyMemory, atomic);
    return sanitizeAtomicResult(op, valueType, atomic);
}

bool WARN_UNUSED_RETURN OMGIRGenerator::emitStructSet(bool canTrap, Value* structValue, uint32_t fieldIndex, const StructType& structType, Value* argument)
{
    structValue = pointerOfWasmRef(structValue);
    auto fieldType = structType.field(fieldIndex).type;
    int32_t fieldOffset = fixupPointerPlusOffset(structValue, JSWebAssemblyStruct::offsetOfData() + structType.offsetOfFieldInPayload(fieldIndex));

    auto wrapTrapping = [&](auto input) -> B3::Kind {
        if (canTrap)
            return trapping(input);
        return input;
    };

    if (fieldType.is<PackedType>()) {
        switch (fieldType.as<PackedType>()) {
        case PackedType::I8: {
            auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Store8), origin(), argument, structValue, fieldOffset);
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i8[fieldIndex], store);
            return false;
        }
        case PackedType::I16: {
            auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Store16), origin(), argument, structValue, fieldOffset);
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i16[fieldIndex], store);
            return false;
        }
    }
    }

    ASSERT(fieldType.is<Type>());
    auto resultType = fieldType.unpacked();
    auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Store), origin(), argument, structValue, fieldOffset);
    switch (resultType.kind) {
    case TypeKind::I32:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i32[fieldIndex], store);
        break;
    case TypeKind::F32:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_f32[fieldIndex], store);
        break;
    case TypeKind::I64:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i64[fieldIndex], store);
        break;
    case TypeKind::F64:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_f64[fieldIndex], store);
        break;
    case TypeKind::V128:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_v128[fieldIndex], store);
        break;
    default:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_ref[fieldIndex], store);
        break;
    }

    // FIXME: We should be able elide this write barrier if we know we're storing jsNull();
    return isRefType(resultType);
}

auto OMGIRGenerator::atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op))) [[unlikely]] {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (valueType.kind) {
        case TypeKind::I32:
            result = push(constant(Int32, 0));
            break;
        case TypeKind::I64:
            result = push(constant(Int64, 0));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = push(emitAtomicCompareExchange(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), get(expected), get(value), offset));

    return { };
}

auto OMGIRGenerator::atomicWait(ExtAtomicOpType op, ExpressionType pointerVar, ExpressionType valueVar, ExpressionType timeoutVar, ExpressionType& result, uint32_t offset) -> PartialResult
{
    Value* pointer = get(pointerVar);
    Value* value = get(valueVar);
    Value* timeout = get(timeoutVar);
    Value* resultValue = nullptr;
    if (op == ExtAtomicOpType::MemoryAtomicWait32) {
        resultValue = callWasmOperation(m_currentBlock, Int32, operationMemoryAtomicWait32,
            instanceValue(), pointer, constant(Int32, offset), value, timeout);
    } else {
        resultValue = callWasmOperation(m_currentBlock, Int32, operationMemoryAtomicWait64,
            instanceValue(), pointer, constant(Int32, offset), value, timeout);
    }

    {
        result = push(resultValue);
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::atomicNotify(ExtAtomicOpType, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t offset) -> PartialResult
{
    Value* resultValue = callWasmOperation(m_currentBlock, Int32, operationMemoryAtomicNotify,
        instanceValue(), get(pointer), constant(Int32, offset), get(count));
    {
        result = push(resultValue);
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto OMGIRGenerator::atomicFence(ExtAtomicOpType, uint8_t) -> PartialResult
{
    m_currentBlock->appendNew<FenceValue>(m_proc, origin());
    return { };
}

auto OMGIRGenerator::truncSaturated(Ext1OpType op, ExpressionType argVar, ExpressionType& result, Type returnType, Type) -> PartialResult
{
    Value* arg = get(argVar);
    Value* maxFloat = nullptr;
    Value* minFloat = nullptr;
    Value* intermediate = nullptr;
    switch (op) {
    case Ext1OpType::I32TruncSatF32S:
        maxFloat = constant(Float, std::bit_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
        minFloat = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));
        break;
    case Ext1OpType::I32TruncSatF32U:
        maxFloat = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
        minFloat = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(-1.0)));
        break;
    case Ext1OpType::I32TruncSatF64S:
        maxFloat = constant(Double, std::bit_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
        minFloat = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));
        break;
    case Ext1OpType::I32TruncSatF64U:
        maxFloat = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
        minFloat = constant(Double, std::bit_cast<uint64_t>(-1.0));
        break;
    case Ext1OpType::I64TruncSatF32S:
        maxFloat = constant(Float, std::bit_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
        minFloat = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));
        intermediate = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_s_f32)),
            arg);
        break;
    case Ext1OpType::I64TruncSatF32U:
        maxFloat = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
        minFloat = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(-1.0)));
        intermediate = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_u_f32)),
            arg);
        break;
    case Ext1OpType::I64TruncSatF64S:
        maxFloat = constant(Double, std::bit_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
        minFloat = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));
        intermediate = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_s_f64)),
            arg);
        break;
    case Ext1OpType::I64TruncSatF64U:
        maxFloat = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
        minFloat = constant(Double, std::bit_cast<uint64_t>(-1.0));
        intermediate = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_u_f64)),
            arg);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (!intermediate) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, toB3Type(returnType), origin());
        patchpoint->append(arg, ValueRep::SomeRegister);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
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
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        });
        patchpoint->effects = Effects::none();
        intermediate = patchpoint;
    }

    Value* maxResult = nullptr;
    Value* minResult = nullptr;
    Value* zero = nullptr;
    bool requiresNaNCheck = false;
    switch (op) {
    case Ext1OpType::I32TruncSatF32S:
    case Ext1OpType::I32TruncSatF64S:
        maxResult = constant(Int32, std::bit_cast<uint32_t>(INT32_MAX));
        minResult = constant(Int32, std::bit_cast<uint32_t>(INT32_MIN));
        zero = constant(Int32, 0);
        requiresNaNCheck = true;
        break;
    case Ext1OpType::I32TruncSatF32U:
    case Ext1OpType::I32TruncSatF64U:
        maxResult = constant(Int32, std::bit_cast<uint32_t>(UINT32_MAX));
        minResult = constant(Int32, std::bit_cast<uint32_t>(0U));
        break;
    case Ext1OpType::I64TruncSatF32S:
    case Ext1OpType::I64TruncSatF64S:
        maxResult = constant(Int64, std::bit_cast<uint64_t>(INT64_MAX));
        minResult = constant(Int64, std::bit_cast<uint64_t>(INT64_MIN));
        zero = constant(Int64, 0);
        requiresNaNCheck = true;
        break;
    case Ext1OpType::I64TruncSatF32U:
    case Ext1OpType::I64TruncSatF64U:
        maxResult = constant(Int64, std::bit_cast<uint64_t>(UINT64_MAX));
        minResult = constant(Int64, std::bit_cast<uint64_t>(0ULL));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, minFloat),
        m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, maxFloat),
            intermediate, maxResult),
        requiresNaNCheck ? m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), arg, arg), minResult, zero) : minResult));

    return { };
}

auto OMGIRGenerator::addRefI31(ExpressionType value, ExpressionType& result) -> PartialResult
{
    ASSERT(value->type().kind() == Int32);
    Value* shiftLeft = m_currentBlock->appendNew<Value>(m_proc, B3::Shl, origin(), get(value), constant(Int32, 0x1));
    Value* shiftRight = m_currentBlock->appendNew<Value>(m_proc, B3::SShr, origin(), shiftLeft, constant(Int32, 0x1));
    Value* extended = m_currentBlock->appendNew<Value>(m_proc, B3::ZExt32, origin(), shiftRight);
    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::BitOr, origin(), extended, constant(wasmRefType(), static_cast<int64_t>(JSValue::Int32Tag) << 32)));

    return { };
}

auto OMGIRGenerator::addI31GetS(TypedExpression reference, ExpressionType& result) -> PartialResult
{
    // Trap on null reference.
    Value* value = get(reference);
    if (reference.type().isNullable())
        emitNullCheck(value, ExceptionType::NullI31Get);

    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Trunc, origin(), value));

    return { };
}

auto OMGIRGenerator::addI31GetU(TypedExpression reference, ExpressionType& result) -> PartialResult
{
    // Trap on null reference.
    Value* value = get(reference);
    if (reference.type().isNullable())
        emitNullCheck(value, ExceptionType::NullI31Get);

    Value* masked = m_currentBlock->appendNew<Value>(m_proc, B3::BitAnd, origin(), int32OfPointer(pointerOfWasmRef(value)), constant(Int32, 0x7fffffff));
    result = push(masked);
    return { };
}

Value* OMGIRGenerator::allocateWasmGCArray(uint32_t typeIndex, Value* initValue, Value* size)
{
    StorageType elementType;
    getArrayElementType(typeIndex, elementType);

    auto* object = allocateWasmGCArrayUninitialized(typeIndex, size);

    auto* loopHeader = m_proc.addBlock();
    auto* loopBody = m_proc.addBlock();
    auto* continuation = m_proc.addBlock();

    auto* payload = emitGetArrayPayloadBase(elementType, object);
    auto* remainingUpsilon = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), pointerOfInt32(size));

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), loopHeader);
    loopHeader->addPredecessor(m_currentBlock);
    m_currentBlock = loopHeader;

    Value* remaining = m_currentBlock->appendNew<Value>(m_proc, Phi, pointerType(), origin());
    remainingUpsilon->setPhi(remaining);
    {
        Value* condition = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), remaining, constant(pointerType(), 0));
        m_currentBlock->appendNewControlValue(m_proc, Branch, origin(), condition, FrequentedBlock(continuation), FrequentedBlock(loopBody));
        continuation->addPredecessor(m_currentBlock);
        loopBody->addPredecessor(m_currentBlock);
    }

    m_currentBlock = loopBody;
    auto* updatedRemaining = m_currentBlock->appendNew<Value>(m_proc, Sub, pointerType(), origin(), remaining, constant(pointerType(), 1));
    auto* updatedRemainingUpsilon = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), updatedRemaining);
    updatedRemainingUpsilon->setPhi(remaining);

    Value* indexedAddress = m_currentBlock->appendNew<Value>(m_proc, Add, pointerType(), origin(), payload, m_currentBlock->appendNew<Value>(m_proc, Mul, pointerType(), origin(), updatedRemaining, constant(pointerType(), elementType.elementSize())));

    if (elementType.is<PackedType>()) {
        switch (elementType.as<PackedType>()) {
        case PackedType::I8: {
            auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store8, origin(), initValue, indexedAddress);
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_i8.atAnyNumber(), store);
            break;
        }
        case PackedType::I16: {
            auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store16, origin(), initValue, indexedAddress);
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_i16.atAnyNumber(), store);
            break;
        }
        }
    } else {
        ASSERT(elementType.is<Type>());
        auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), initValue, indexedAddress);
        switch (elementType.unpacked().kind) {
        case TypeKind::I32:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_i32.atAnyNumber(), store);
            break;
        case TypeKind::F32:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_f32.atAnyNumber(), store);
            break;
        case TypeKind::I64:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_i64.atAnyNumber(), store);
            break;
        case TypeKind::F64:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_f64.atAnyNumber(), store);
            break;
        case TypeKind::V128:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_v128.atAnyNumber(), store);
            break;
        default:
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_ref.atAnyNumber(), store);
            break;
        }
    }
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), FrequentedBlock(loopHeader));
    loopHeader->addPredecessor(m_currentBlock);

    m_currentBlock = continuation;
    mutatorFence();

    return object;
}

// Given a type index, verify that it's an array type and return its expansion
const ArrayType* OMGIRGenerator::getArrayTypeDefinition(uint32_t typeIndex)
{
    Ref<Wasm::TypeDefinition> typeDef = getTypeDefinition(typeIndex);
    const Wasm::TypeDefinition& arraySignature = typeDef->expand();
    ASSERT(arraySignature.is<ArrayType>());
    return arraySignature.as<ArrayType>();
}

// Given a type index for an array signature, look it up, expand it and
// return the element type
void OMGIRGenerator::getArrayElementType(uint32_t typeIndex, StorageType& result)
{
    const ArrayType* arrayType = getArrayTypeDefinition(typeIndex);
    result = arrayType->elementType().type;
}

// Given a type index, verify that it's an array type and return the type (Ref a)
void OMGIRGenerator::getArrayRefType(uint32_t typeIndex, Type& result)
{
    Ref<Wasm::TypeDefinition> typeDef = getTypeDefinition(typeIndex);
    result = Type { TypeKind::Ref, typeDef->index() };
}

auto OMGIRGenerator::addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType value, ExpressionType& result) -> PartialResult
{
#if ASSERT_ENABLED
    StorageType elementType;
    getArrayElementType(typeIndex, elementType);
    ASSERT(toB3Type(elementType.unpacked()) == value->type());
#endif

    Value* initValue = get(value);
    Value* sizeValue = get(size);
    if (value->type() == B3::Float || value->type() == B3::Double) {
        initValue = m_currentBlock->appendNew<Value>(m_proc, BitwiseCast, origin(), initValue);
        if (initValue->type() == B3::Int32)
            initValue = m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), initValue);
    }

    Value* array = allocateWasmGCArray(typeIndex, initValue, sizeValue);
    result = push(array);
    return { };
}

Variable* OMGIRGenerator::pushArrayNewFromSegment(ArraySegmentOperation operation, uint32_t typeIndex, uint32_t segmentIndex, ExpressionType arraySize, ExpressionType offset, ExceptionType exceptionType)
{
    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::Arrayref), operation,
        instanceValue(), constant(Int32, typeIndex),
        constant(Int32, segmentIndex),
        get(arraySize), get(offset));

    // Indicates out of bounds for the segment or allocation failure.
    emitNullCheck(resultValue, exceptionType);

    return push(resultValue);
}

auto OMGIRGenerator::addArrayNewDefault(uint32_t typeIndex, ExpressionType size, ExpressionType& result) -> PartialResult
{
    StorageType elementType;
    getArrayElementType(typeIndex, elementType);

    Value* initValue = nullptr;
    if (isRefType(elementType.unpacked()))
        initValue = m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), JSValue::encode(jsNull()));
    else if (elementType.elementSize() == 16)
        initValue = m_currentBlock->appendNew<Const128Value>(m_proc, origin(), v128_t { });
    else if (elementType.elementSize() <= 4)
        initValue = constant(Int32, 0);
    else
        initValue = constant(Int64, 0);

    Value* sizeValue = get(size);
    Value* array = allocateWasmGCArray(typeIndex, initValue, sizeValue);
    result = push(array);
    return { };
}

auto OMGIRGenerator::addArrayNewData(uint32_t typeIndex, uint32_t dataIndex, ExpressionType arraySize, ExpressionType offset, ExpressionType& result) -> PartialResult
{
    result = pushArrayNewFromSegment(operationWasmArrayNewData, typeIndex, dataIndex, arraySize, offset, ExceptionType::BadArrayNewInitData);

    return { };
}

auto OMGIRGenerator::addArrayNewElem(uint32_t typeIndex, uint32_t elemSegmentIndex, ExpressionType arraySize, ExpressionType offset, ExpressionType& result) -> PartialResult
{
    result = pushArrayNewFromSegment(operationWasmArrayNewElem, typeIndex, elemSegmentIndex, arraySize, offset, ExceptionType::BadArrayNewInitElem);
    return { };
}

auto OMGIRGenerator::addArrayNewFixed(uint32_t typeIndex, ArgumentList& args, ExpressionType& result) -> PartialResult
{
    StorageType elementType;
    getArrayElementType(typeIndex, elementType);

    auto* size = constant(Int32, args.size());
    auto* object = allocateWasmGCArrayUninitialized(typeIndex, size);

    for (uint32_t i = 0; i < args.size(); ++i) {
        // Emit the array set code -- note that this omits the bounds check, since
        // if operationWasmArrayNewEmpty() returned a non-null value, it's an array of the right size
        emitArraySetUncheckedWithoutWriteBarrier(typeIndex, object, constant(Int32, i), get(args[i]));
    }
    mutatorFence();
    result = push(object);
    return { };
}

Value* OMGIRGenerator::emitGetArraySizeWithNullCheck(Type arrayType, Value* array)
{
    ptrdiff_t offset = safeCast<int32_t>(JSWebAssemblyArray::offsetOfSize());
    bool canTrap = false;
    if (arrayType.isNullable())
        canTrap = emitNullCheckBeforeAccess(array, offset);
    auto wrapTrapping = [&](auto input) -> B3::Kind {
        if (canTrap)
            return trapping(input);
        return input;
    };
    auto* arraySize = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Load), Int32, origin(), pointerOfWasmRef(array), offset);
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_size, arraySize);
    return arraySize;
}

auto OMGIRGenerator::addArrayGet(ExtGCOpType arrayGetKind, uint32_t typeIndex, TypedExpression arrayref, ExpressionType index, ExpressionType& result) -> PartialResult
{
    auto arrayValue = get(arrayref);
    auto indexValue = get(index);
    StorageType elementType;
    getArrayElementType(typeIndex, elementType);
    Wasm::Type resultType = elementType.unpacked();

    // Check array bounds.
    auto* arraySize = emitGetArraySizeWithNullCheck(arrayref.type(), arrayValue);
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), indexValue, arraySize));
        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsArrayGet);
        });
    }

    Value* payloadBase = emitGetArrayPayloadBase(elementType, arrayValue);
    Value* indexedAddress = m_currentBlock->appendNew<Value>(m_proc, Add, pointerType(), origin(), payloadBase,
        m_currentBlock->appendNew<Value>(m_proc, Mul, pointerType(), origin(),
            pointerOfInt32(indexValue),
            constant(pointerType(), elementType.elementSize())));

    auto decorateWithIndex = [&](MemoryValue* load, NumberedAbstractHeap& heap, Value* indexValue) {
        // FIXME: we should do this decoration after we found that indexValue is a constant.
        // But right now, the current mechanism requires constant at the frontend level.
        // We may need to modelthis WasmArrayGet as is in B3 initially, doing optimization
        // and lower it to these sequence in the middle of the pipeline.
        // The logic is the same to what FTL is doing. But FTL already does constant folding onto FTL SSA,
        // while OMG B3 is not at this stage.
        if (indexValue->hasInt32()) {
            int32_t index = indexValue->asInt32();
            if (index >= 0) {
                m_heaps.decorateMemory(&heap[index], load);
                return;
            }
        }
        m_heaps.decorateMemory(&heap.atAnyNumber(), load);
    };

    if (elementType.is<PackedType>()) {
        MemoryValue* load;
        switch (elementType.as<PackedType>()) {
        case PackedType::I8:
            load = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), indexedAddress);
            decorateWithIndex(load, m_heaps.JSWebAssemblyArray_i8, indexValue);
            break;
        case PackedType::I16:
            load = m_currentBlock->appendNew<MemoryValue>(m_proc, Load16Z, Int32, origin(), indexedAddress);
            decorateWithIndex(load, m_heaps.JSWebAssemblyArray_i16, indexValue);
            break;
        }
        Value* postProcess = load;
        switch (arrayGetKind) {
        case ExtGCOpType::ArrayGet:
        case ExtGCOpType::ArrayGetU:
            break;
        case ExtGCOpType::ArrayGetS: {
            size_t elementSize = elementType.as<PackedType>() == PackedType::I8 ? sizeof(uint8_t) : sizeof(uint16_t);
            uint8_t bitShift = (sizeof(uint32_t) - elementSize) * 8;
            Value* shiftLeft = m_currentBlock->appendNew<Value>(m_proc, B3::Shl, origin(), postProcess, constant(Int32, bitShift));
            postProcess = m_currentBlock->appendNew<Value>(m_proc, B3::SShr, origin(), shiftLeft, constant(Int32, bitShift));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
        result = push(postProcess);
        return { };
    }

    ASSERT(elementType.is<Type>());
    auto* load = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(resultType), origin(), indexedAddress);
    switch (resultType.kind) {
    case TypeKind::I32:
        decorateWithIndex(load, m_heaps.JSWebAssemblyArray_i32, indexValue);
        break;
    case TypeKind::F32:
        decorateWithIndex(load, m_heaps.JSWebAssemblyArray_f32, indexValue);
        break;
    case TypeKind::I64:
        decorateWithIndex(load, m_heaps.JSWebAssemblyArray_i64, indexValue);
        break;
    case TypeKind::F64:
        decorateWithIndex(load, m_heaps.JSWebAssemblyArray_f64, indexValue);
        break;
    case TypeKind::V128:
        decorateWithIndex(load, m_heaps.JSWebAssemblyArray_v128, indexValue);
        break;
    default:
        decorateWithIndex(load, m_heaps.JSWebAssemblyArray_ref, indexValue);
        break;
    }

    result = push(load);

    return { };
}

void OMGIRGenerator::emitNullCheck(Value* ref, ExceptionType exceptionType)
{
    CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
        m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), ref, m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), JSValue::encode(jsNull()))));
    check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, exceptionType);
    });
}

bool OMGIRGenerator::emitNullCheckBeforeAccess(Value* ref, ptrdiff_t offset)
{
    if (Options::useWasmFaultSignalHandler()) {
        if (offset <= maxAcceptableOffsetForNullReference())
            return true;
    }

    CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
        m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), ref, m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), JSValue::encode(jsNull()))));
    check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::NullAccess);
    });
    return false;
}

Value* OMGIRGenerator::emitGetArrayPayloadBase(Wasm::StorageType fieldType, Value* arrayref)
{
    auto payloadBase = m_currentBlock->appendNew<Value>(m_proc, Add, pointerType(), origin(), pointerOfWasmRef(arrayref), constant(pointerType(), JSWebAssemblyArray::offsetOfData()));
    if (JSWebAssemblyArray::needsAlignmentCheck(fieldType)) {
        auto isPreciseAllocation = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), pointerOfWasmRef(arrayref), constant(pointerType(), PreciseAllocation::halfAlignment));
        return m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), isPreciseAllocation,
            payloadBase,
            m_currentBlock->appendNew<Value>(m_proc, Add, origin(), payloadBase, constant(pointerType(), JSWebAssemblyArray::v128AlignmentShift)));
    }
    return payloadBase;
}

// Does the array set without null check and bounds checks -- can be
// called directly by addArrayNewFixed()
bool OMGIRGenerator::emitArraySetUncheckedWithoutWriteBarrier(uint32_t typeIndex, Value* arrayref, Value* indexValue, Value* setValue)
{
    StorageType elementType;
    getArrayElementType(typeIndex, elementType);

    auto payloadBase = emitGetArrayPayloadBase(elementType, arrayref);
    auto indexedAddress = m_currentBlock->appendNew<Value>(m_proc, Add, pointerType(), origin(), payloadBase,
        m_currentBlock->appendNew<Value>(m_proc, Mul, pointerType(), origin(), pointerOfInt32(indexValue), constant(pointerType(), elementType.elementSize())));

    auto decorateWithIndex = [&](MemoryValue* store, NumberedAbstractHeap& heap, Value* indexValue) {
        // FIXME: we should do this decoration after we found that indexValue is a constant.
        // But right now, the current mechanism requires constant at the frontend level.
        // We may need to modelthis WasmArrayGet as is in B3 initially, doing optimization
        // and lower it to these sequence in the middle of the pipeline.
        // The logic is the same to what FTL is doing. But FTL already does constant folding onto FTL SSA,
        // while OMG B3 is not at this stage.
        if (indexValue->hasInt32()) {
            int32_t index = indexValue->asInt32();
            if (index >= 0) {
                m_heaps.decorateMemory(&heap[index], store);
                return;
            }
        }
        m_heaps.decorateMemory(&heap.atAnyNumber(), store);
    };

    if (elementType.is<PackedType>()) {
        switch (elementType.as<PackedType>()) {
        case PackedType::I8: {
            auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store8, origin(), setValue, indexedAddress);
            decorateWithIndex(store, m_heaps.JSWebAssemblyArray_i8, indexValue);
            break;
        }
        case PackedType::I16: {
            auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store16, origin(), setValue, indexedAddress);
            decorateWithIndex(store, m_heaps.JSWebAssemblyArray_i16, indexValue);
            break;
        }
        }
        return false;
    }

    ASSERT(elementType.is<Type>());
    auto resultType = elementType.unpacked();
    auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), setValue, indexedAddress);
    switch (resultType.kind) {
    case TypeKind::I32:
        decorateWithIndex(store, m_heaps.JSWebAssemblyArray_i32, indexValue);
        break;
    case TypeKind::F32:
        decorateWithIndex(store, m_heaps.JSWebAssemblyArray_f32, indexValue);
        break;
    case TypeKind::I64:
        decorateWithIndex(store, m_heaps.JSWebAssemblyArray_i64, indexValue);
        break;
    case TypeKind::F64:
        decorateWithIndex(store, m_heaps.JSWebAssemblyArray_f64, indexValue);
        break;
    case TypeKind::V128:
        decorateWithIndex(store, m_heaps.JSWebAssemblyArray_v128, indexValue);
        break;
    default:
        decorateWithIndex(store, m_heaps.JSWebAssemblyArray_ref, indexValue);
        break;
    }
    return isRefType(elementType.unpacked());
}

void OMGIRGenerator::emitArraySetUnchecked(uint32_t typeIndex, Value* arrayref, Value* index, Value* setValue)
{
    if (emitArraySetUncheckedWithoutWriteBarrier(typeIndex, arrayref, index, setValue))
        emitWriteBarrier(pointerOfWasmRef(arrayref), instanceValue());
}


auto OMGIRGenerator::addArraySet(uint32_t typeIndex, TypedExpression arrayref, ExpressionType index, ExpressionType value) -> PartialResult
{
#if ASSERT_ENABLED
    const ArrayType* arrayType = getArrayTypeDefinition(typeIndex);
    UNUSED_VARIABLE(arrayType);
#endif

    auto arrayValue = get(arrayref);
    auto indexValue = get(index);
    auto valueValue = get(value);

    // Check array bounds.
    auto* arraySize = emitGetArraySizeWithNullCheck(arrayref.type(), arrayValue);
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), indexValue, arraySize));
        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsArraySet);
        });
    }

    emitArraySetUnchecked(typeIndex, arrayValue, indexValue, valueValue);

    return { };
}

auto OMGIRGenerator::addArrayLen(TypedExpression arrayref, ExpressionType& result) -> PartialResult
{
    auto arrayValue = get(arrayref);

    auto* arraySize = emitGetArraySizeWithNullCheck(arrayref.type(), arrayValue);

    result = push(arraySize);

    return { };
}

auto OMGIRGenerator::addArrayFill(uint32_t typeIndex, TypedExpression arrayref, ExpressionType offset, ExpressionType value, ExpressionType size) -> PartialResult
{
    StorageType elementType;
    getArrayElementType(typeIndex, elementType);

    auto arrayValue = get(arrayref);
    auto offsetValue = get(offset);
    auto valueValue = get(value);
    auto sizeValue = get(size);

    if (arrayref.type().isNullable())
        emitNullCheck(arrayValue, ExceptionType::NullArrayFill);

    Value* resultValue;
    RELEASE_ASSERT(!elementType.unpacked().isV128());
    Value* adjustedValue = valueValue;
    if (adjustedValue->type().isFloat())
        adjustedValue = m_currentBlock->appendNew<Value>(m_proc, BitwiseCast, origin(), adjustedValue);
    if (adjustedValue->type() == Int32)
        adjustedValue = m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), adjustedValue);
    resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmArrayFill,
        instanceValue(), arrayValue, offsetValue, adjustedValue, sizeValue);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsArrayFill);
        });
    }

    return { };
}

auto OMGIRGenerator::addArrayCopy(uint32_t, TypedExpression dst, ExpressionType dstOffset, uint32_t, TypedExpression src, ExpressionType srcOffset, ExpressionType size) -> PartialResult
{
    auto dstValue = get(dst);
    auto dstOffsetValue = get(dstOffset);
    auto srcValue = get(src);
    auto srcOffsetValue = get(srcOffset);
    auto sizeValue = get(size);

    if (dst.type().isNullable())
        emitNullCheck(dstValue, ExceptionType::NullArrayCopy);
    if (src.type().isNullable())
        emitNullCheck(srcValue, ExceptionType::NullArrayCopy);

    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmArrayCopy,
        instanceValue(),
        dstValue, dstOffsetValue,
        srcValue, srcOffsetValue,
        sizeValue);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsArrayCopy);
        });
    }

    return { };
}

auto OMGIRGenerator::addArrayInitElem(uint32_t, TypedExpression dst, ExpressionType dstOffset, uint32_t srcElementIndex, ExpressionType srcOffset, ExpressionType size) -> PartialResult
{
    auto dstValue = get(dst);
    auto dstOffsetValue = get(dstOffset);
    auto srcOffsetValue = get(srcOffset);
    auto sizeValue = get(size);

    if (dst.type().isNullable())
        emitNullCheck(dstValue, ExceptionType::NullArrayInitElem);

    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmArrayInitElem,
        instanceValue(),
        dstValue, dstOffsetValue,
        constant(Int32, srcElementIndex), srcOffsetValue,
        sizeValue);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsArrayInitElem);
        });
    }

    return { };
}

auto OMGIRGenerator::addArrayInitData(uint32_t, TypedExpression dst, ExpressionType dstOffset, uint32_t srcDataIndex, ExpressionType srcOffset, ExpressionType size) -> PartialResult
{
    auto dstValue = get(dst);
    auto dstOffsetValue = get(dstOffset);
    auto srcOffsetValue = get(srcOffset);
    auto sizeValue = get(size);

    if (dst.type().isNullable())
        emitNullCheck(dstValue, ExceptionType::NullArrayInitData);

    Value* resultValue = callWasmOperation(m_currentBlock, toB3Type(Types::I32), operationWasmArrayInitData,
        instanceValue(),
        dstValue, dstOffsetValue,
        constant(Int32, srcDataIndex), srcOffsetValue,
        sizeValue);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, constant(Int32, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsArrayInitData);
        });
    }

    return { };
}

auto OMGIRGenerator::addStructNew(uint32_t typeIndex, ArgumentList& args, ExpressionType& result) -> PartialResult
{
    Value* structValue = allocateWasmGCStructUninitialized(typeIndex);
    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    for (uint32_t i = 0; i < args.size(); ++i) {
        bool needsWriteBarrier = emitStructSet(/* canTrap */ false, structValue, i, structType, get(args[i]));
        UNUSED_VARIABLE(needsWriteBarrier);
    }
    mutatorFence();
    result = push(structValue);
    return { };
}

auto OMGIRGenerator::addStructNewDefault(uint32_t typeIndex, ExpressionType& result) -> PartialResult
{
    Value* structValue = allocateWasmGCStructUninitialized(typeIndex);
    const auto& structType = *m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    for (StructFieldCount i = 0; i < structType.fieldCount(); ++i) {
        Value* initValue;
        if (Wasm::isRefType(structType.field(i).type))
            initValue = m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), JSValue::encode(jsNull()));
        else if (typeSizeInBytes(structType.field(i).type) <= 4)
            initValue = constant(Int32, 0);
        else
            initValue = constant(Int64, 0);
        // We know all the values here are not cells so we don't need a writeBarrier.
        bool needsWriteBarrier = emitStructSet(/* canTrap */ false, structValue, i, structType, initValue);
        UNUSED_VARIABLE(needsWriteBarrier);
    }
    mutatorFence();
    result = push(structValue);
    return { };
}

auto OMGIRGenerator::addStructGet(ExtGCOpType structGetKind, TypedExpression structReference, const StructType& structType, uint32_t fieldIndex, ExpressionType& result) -> PartialResult
{
    auto fieldType = structType.field(fieldIndex).type;
    auto mutability = structType.field(fieldIndex).mutability;
    auto resultType = fieldType.unpacked();

    Value* structValue = get(structReference);

    ptrdiff_t fieldOffset = fixupPointerPlusOffset(structValue, JSWebAssemblyStruct::offsetOfData() + structType.offsetOfFieldInPayload(fieldIndex));
    bool canTrap = false;
    if (structReference.type().isNullable())
        canTrap = emitNullCheckBeforeAccess(structValue, fieldOffset);
    auto wrapTrapping = [&](auto input) -> B3::Kind {
        if (canTrap)
            return trapping(input);
        return input;
    };
    structValue = pointerOfWasmRef(structValue);

    if (fieldType.is<PackedType>()) {
        MemoryValue* load;
        switch (fieldType.as<PackedType>()) {
        case PackedType::I8:
            load = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Load8Z), Int32, origin(), structValue, fieldOffset);
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i8[fieldIndex], load);
            break;
        case PackedType::I16:
            load = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Load16Z), Int32, origin(), structValue, fieldOffset);
            m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i16[fieldIndex], load);
            break;
        }
        if (mutability == Mutability::Immutable)
            load->setReadsMutability(B3::Mutability::Immutable);
        Value* postProcess = load;
        switch (structGetKind) {
        case ExtGCOpType::StructGetU:
            break;
        case ExtGCOpType::StructGetS: {
            uint8_t bitShift = (sizeof(uint32_t) - fieldType.elementSize()) * 8;
            Value* shiftLeft = m_currentBlock->appendNew<Value>(m_proc, B3::Shl, origin(), postProcess, constant(Int32, bitShift));
            postProcess = m_currentBlock->appendNew<Value>(m_proc, B3::SShr, origin(), shiftLeft, constant(Int32, bitShift));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
        result = push(postProcess);
        return { };
    }

    ASSERT(fieldType.is<Type>());
    auto* load = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Load), toB3Type(resultType), origin(), structValue, fieldOffset);
    switch (resultType.kind) {
    case TypeKind::I32:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i32[fieldIndex], load);
        break;
    case TypeKind::F32:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_f32[fieldIndex], load);
        break;
    case TypeKind::I64:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_i64[fieldIndex], load);
        break;
    case TypeKind::F64:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_f64[fieldIndex], load);
        break;
    case TypeKind::V128:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_v128[fieldIndex], load);
        break;
    default:
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_ref[fieldIndex], load);
        break;
    }
    if (mutability == Mutability::Immutable)
        load->setReadsMutability(B3::Mutability::Immutable);
    result = push(load);

    return { };
}

auto OMGIRGenerator::addStructSet(TypedExpression structReference, const StructType& structType, uint32_t fieldIndex, ExpressionType value) -> PartialResult
{
    Value* structValue = get(structReference);
    Value* valueValue = get(value);

    ptrdiff_t fieldOffset = fixupPointerPlusOffset(structValue, JSWebAssemblyStruct::offsetOfData() + structType.offsetOfFieldInPayload(fieldIndex));
    bool canTrap = false;
    if (structReference.type().isNullable())
        canTrap = emitNullCheckBeforeAccess(structValue, fieldOffset);

    bool needsWriteBarrier = emitStructSet(canTrap, structValue, fieldIndex, structType, valueValue);
    if (needsWriteBarrier)
        emitWriteBarrier(pointerOfWasmRef(structValue), instanceValue());
    return { };
}

auto OMGIRGenerator::addRefTest(TypedExpression reference, bool allowNull, int32_t heapType, bool shouldNegate, ExpressionType& result) -> PartialResult
{
    emitRefTestOrCast(CastKind::Test, reference, allowNull, heapType, shouldNegate, result);
    return { };
}

auto OMGIRGenerator::addRefCast(TypedExpression reference, bool allowNull, int32_t heapType, ExpressionType& result) -> PartialResult
{
    emitRefTestOrCast(CastKind::Cast, reference, allowNull, heapType, false, result);
    return { };
}

void OMGIRGenerator::emitRefTestOrCast(CastKind castKind, TypedExpression reference, bool allowNull, int32_t toHeapType, bool shouldNegate, ExpressionType& result)
{
    Value* value = get(reference);
    if (castKind == CastKind::Cast)
        result = push(value);

    BasicBlock* continuation = m_proc.addBlock();
    BasicBlock* trueBlock = nullptr;
    BasicBlock* falseBlock = nullptr;
    if (castKind == CastKind::Test) {
        trueBlock = m_proc.addBlock();
        falseBlock = m_proc.addBlock();
    }

    auto castFailure = [this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::CastFailure);
    };

    auto castAccessOffset = [&] -> std::optional<ptrdiff_t> {
        if (castKind == CastKind::Test)
            return std::nullopt;

        if (allowNull)
            return std::nullopt;

        if (typeIndexIsType(static_cast<Wasm::TypeIndex>(toHeapType)))
            return std::nullopt;

        Wasm::TypeDefinition& signature = m_info.typeSignatures[toHeapType];
        if (signature.expand().is<Wasm::FunctionSignature>())
            return WebAssemblyFunctionBase::offsetOfRTT();

        if (!reference.type().definitelyIsCellOrNull())
            return std::nullopt;

        if (!reference.type().definitelyIsWasmGCObjectOrNull())
            return JSCell::typeInfoTypeOffset();
        return JSCell::structureIDOffset();
    };

    bool canTrap = false;
    auto wrapTrapping = [&](auto input) -> B3::Kind {
        if (canTrap) {
            canTrap = false;
            return trapping(input);
        }
        return input;
    };
    // Ensure reference nullness agrees with heap type.
    {
        BasicBlock* nullCase = m_proc.addBlock();
        BasicBlock* nonNullCase = m_proc.addBlock();

        Value* isNull = nullptr;
        if (reference.type().isNullable()) {
            if (auto offset = castAccessOffset(); offset && offset.value() <= maxAcceptableOffsetForNullReference()) {
                isNull = constant(Int32, 0);
                canTrap = true;
            } else
            isNull = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), value, m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), JSValue::encode(jsNull())));
        } else
            isNull = constant(Int32, 0);

        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), isNull, FrequentedBlock(nullCase), FrequentedBlock(nonNullCase));
        nullCase->addPredecessor(m_currentBlock);
        nonNullCase->addPredecessor(m_currentBlock);

        m_currentBlock = nullCase;
        if (castKind == CastKind::Cast) {
            if (!allowNull) {
                B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
                throwException->setGenerator(castFailure);
            }
            m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
            continuation->addPredecessor(m_currentBlock);
        } else {
            BasicBlock* nextBlock;
            if (!allowNull)
                nextBlock = falseBlock;
            else
                nextBlock = trueBlock;
            m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), nextBlock);
            nextBlock->addPredecessor(m_currentBlock);
        }

        m_currentBlock = nonNullCase;
    }

    if (typeIndexIsType(static_cast<Wasm::TypeIndex>(toHeapType))) {
        switch (static_cast<TypeKind>(toHeapType)) {
        case Wasm::TypeKind::Funcref:
        case Wasm::TypeKind::Externref:
        case Wasm::TypeKind::Anyref:
        case Wasm::TypeKind::Exnref:
            // Casts to these types cannot fail as they are the top types of their respective hierarchies, and static type-checking does not allow cross-hierarchy casts.
            break;
        case Wasm::TypeKind::Noneref:
        case Wasm::TypeKind::Nofuncref:
        case Wasm::TypeKind::Noexternref:
        case Wasm::TypeKind::Noexnref:
            // Casts to any bottom type should always fail.
            if (castKind == CastKind::Cast) {
                B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
                throwException->setGenerator(castFailure);
            } else {
                m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), falseBlock);
                falseBlock->addPredecessor(m_currentBlock);
                m_currentBlock = m_proc.addBlock();
            }
            break;
        case Wasm::TypeKind::Eqref: {
            auto nop = [] (CCallHelpers&, const B3::StackmapGenerationParams&) { };
            BasicBlock* endBlock = castKind == CastKind::Cast ? continuation : trueBlock;
            BasicBlock* checkObject = m_proc.addBlock();

            // The eqref case chains together checks for i31, array, and struct with disjunctions so the control flow is more complicated, and requires some extra basic blocks to be created.
            Value* tag = m_currentBlock->appendNew<Value>(m_proc, TruncHigh, origin(), value);
            emitCheckOrBranchForCast(CastKind::Test, m_currentBlock->appendNew<Value>(m_proc, Below, origin(), tag, constant(pointerType(), JSValue::Int32Tag)), nop, checkObject);
            Value* untagged = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
            emitCheckOrBranchForCast(CastKind::Test, m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), untagged, constant(Int32, Wasm::maxI31ref)), nop, checkObject);
            emitCheckOrBranchForCast(CastKind::Test, m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), untagged, constant(Int32, Wasm::minI31ref)), nop, checkObject);
            m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), endBlock);
            checkObject->addPredecessor(m_currentBlock);
            endBlock->addPredecessor(m_currentBlock);

            m_currentBlock = checkObject;
            if (!reference.type().definitelyIsCellOrNull())
                emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), tag, constant(pointerType(), JSValue::CellTag)), castFailure, falseBlock);
            if (!reference.type().definitelyIsWasmGCObjectOrNull()) {
                auto* jsType = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), pointerOfWasmRef(value), safeCast<int32_t>(JSCell::typeInfoTypeOffset()));
                m_heaps.decorateMemory(&m_heaps.JSCell_typeInfoType, jsType);
                emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), jsType, constant(Int32, JSType::WebAssemblyGCObjectType)), castFailure, falseBlock);
            }
            break;
        }
        case Wasm::TypeKind::I31ref: {
            Value* tag = m_currentBlock->appendNew<Value>(m_proc, TruncHigh, origin(), value);
            emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), tag, constant(pointerType(), JSValue::Int32Tag)), castFailure, falseBlock);
            Value* untagged = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
            emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), untagged, constant(Int32, Wasm::maxI31ref)), castFailure, falseBlock);
            emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), untagged, constant(Int32, Wasm::minI31ref)), castFailure, falseBlock);
            break;
        }
        case Wasm::TypeKind::Arrayref:
        case Wasm::TypeKind::Structref: {
            if (!reference.type().definitelyIsCellOrNull()) {
                Value* tag = m_currentBlock->appendNew<Value>(m_proc, TruncHigh, origin(), value);
                emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), tag, constant(pointerType(), JSValue::CellTag)), castFailure, falseBlock);
            }
            if (!reference.type().definitelyIsWasmGCObjectOrNull()) {
                auto* jsType = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), pointerOfWasmRef(value), safeCast<int32_t>(JSCell::typeInfoTypeOffset()));
                m_heaps.decorateMemory(&m_heaps.JSCell_typeInfoType, jsType);
                emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), jsType, constant(Int32, JSType::WebAssemblyGCObjectType)), castFailure, falseBlock);
            }
            Value* rtt = emitLoadRTTFromObject(pointerOfWasmRef(value));
            auto* kind = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, origin(), rtt, safeCast<int32_t>(RTT::offsetOfKind()));
            m_heaps.decorateMemory(&m_heaps.WasmRTT_kind, kind);
            kind->setControlDependent(false);

            emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), kind, constant(Int32, static_cast<uint8_t>(static_cast<TypeKind>(toHeapType) == Wasm::TypeKind::Arrayref ? RTTKind::Array : RTTKind::Struct))), castFailure, falseBlock);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    } else {
        Wasm::TypeDefinition& signature = m_info.typeSignatures[toHeapType];
        Ref targetRTT = m_info.rtts[toHeapType];

        ([&] {
            Value* structure = nullptr;
            MemoryValue* rtt;
            if (signature.expand().is<Wasm::FunctionSignature>()) {
                rtt = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(B3::Load), pointerType(), origin(), pointerOfWasmRef(value), safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfRTT()));
                m_heaps.decorateMemory(&m_heaps.WebAssemblyFunctionBase_rtt, rtt);
            } else {
                Value* tag = m_currentBlock->appendNew<Value>(m_proc, TruncHigh, origin(), value);
                // The cell check is only needed for non-functions, as the typechecker does not allow non-Cell values for funcref casts.
                if (!reference.type().definitelyIsCellOrNull())
                    emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), tag, constant(Int32, JSValue::CellTag)), castFailure, falseBlock);

                if (!reference.type().definitelyIsWasmGCObjectOrNull()) {
                    auto* jsType = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Load8Z), Int32, origin(), pointerOfWasmRef(value), safeCast<int32_t>(JSCell::typeInfoTypeOffset()));
                    m_heaps.decorateMemory(&m_heaps.JSCell_typeInfoType, jsType);
                    emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), jsType, constant(Int32, JSType::WebAssemblyGCObjectType)), castFailure, falseBlock);
                }
                Value* structureID = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(B3::Load), Int32, origin(), pointerOfWasmRef(value), safeCast<int32_t>(JSCell::structureIDOffset()));
                structure = decodeNonNullStructure(structureID);
                if (targetRTT->displaySizeExcludingThis() < WebAssemblyGCStructure::inlinedTypeDisplaySize) {
                    auto* targetRTTPointer = constant(pointerType(), std::bit_cast<uintptr_t>(targetRTT.ptr()));
                    auto* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, pointerType(), origin(), structure, safeCast<int32_t>(WebAssemblyGCStructure::offsetOfInlinedTypeDisplay() + targetRTT->displaySizeExcludingThis() * sizeof(RefPtr<const RTT>)));
                    m_heaps.decorateMemory(&m_heaps.WebAssemblyGCStructure_inlinedTypeDisplays[targetRTT->displaySizeExcludingThis()], pointer);
                    pointer->setReadsMutability(B3::Mutability::Immutable);
                    pointer->setControlDependent(false);

                    emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), pointer, targetRTTPointer), castFailure, falseBlock);
                    return;
                }

                rtt = m_currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, pointerType(), origin(), structure, safeCast<int32_t>(WebAssemblyGCStructure::offsetOfRTT()));
                m_heaps.decorateMemory(&m_heaps.WebAssemblyGCStructure_rtt, rtt);
                rtt->setControlDependent(false);
            }

            auto* targetRTTPointer = constant(pointerType(), std::bit_cast<uintptr_t>(targetRTT.ptr()));
            BasicBlock* equalBlock;
            if (castKind == CastKind::Cast)
                equalBlock = continuation;
            else
                equalBlock = trueBlock;
            BasicBlock* slowPath = m_proc.addBlock();
            m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), rtt, targetRTTPointer), FrequentedBlock(equalBlock), FrequentedBlock(slowPath));
            equalBlock->addPredecessor(m_currentBlock);
            slowPath->addPredecessor(m_currentBlock);

            m_currentBlock = slowPath;
            if (signature.isFinalType()) {
                // If signature is final type and pointer equality failed, this value must not be a subtype.
                emitCheckOrBranchForCast(castKind, constant(Int32, 1), castFailure, falseBlock);
            } else {
                auto* displaySizeExcludingThis = m_currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, Int32, origin(), rtt, safeCast<int32_t>(RTT::offsetOfDisplaySizeExcludingThis()));
                m_heaps.decorateMemory(&m_heaps.WasmRTT_displaySizeExcludingThis, displaySizeExcludingThis);
                displaySizeExcludingThis->setControlDependent(false);

                emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, BelowEqual, origin(), displaySizeExcludingThis, constant(Int32, targetRTT->displaySizeExcludingThis())), castFailure, falseBlock);

                auto* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, pointerType(), origin(), rtt, safeCast<int32_t>(RTT::offsetOfData() + targetRTT->displaySizeExcludingThis() * sizeof(RefPtr<const RTT>)));
                m_heaps.decorateMemory(&m_heaps.WasmRTT_data[targetRTT->displaySizeExcludingThis()], pointer);
                pointer->setReadsMutability(B3::Mutability::Immutable);
                pointer->setControlDependent(false);

                emitCheckOrBranchForCast(castKind, m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), pointer, targetRTTPointer), castFailure, falseBlock);
            }
        }());
    }

    if (castKind == CastKind::Cast) {
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);
        m_currentBlock = continuation;
    } else {
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), trueBlock);
        trueBlock->addPredecessor(m_currentBlock);
        m_currentBlock = trueBlock;
        UpsilonValue* trueUpsilon = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), constant(B3::Int32, shouldNegate ? 0 : 1));
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);

        m_currentBlock = falseBlock;
        UpsilonValue* falseUpsilon = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), constant(B3::Int32, shouldNegate ? 1 : 0));
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);

        m_currentBlock = continuation;
        Value* phi = m_currentBlock->appendNew<Value>(m_proc, Phi, B3::Int32, origin());
        trueUpsilon->setPhi(phi);
        falseUpsilon->setPhi(phi);
        result = push(phi);
    }
}

template <typename Generator>
void OMGIRGenerator::emitCheckOrBranchForCast(CastKind kind, Value* condition, const Generator& generator, BasicBlock* falseBlock)
{
    if (kind == CastKind::Cast) {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), condition);
        check->setGenerator(generator);
    } else {
        ASSERT(falseBlock);
        BasicBlock* success = m_proc.addBlock();
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), condition,
            FrequentedBlock(falseBlock), FrequentedBlock(success));
        falseBlock->addPredecessor(m_currentBlock);
        success->addPredecessor(m_currentBlock);
        m_currentBlock = success;
    }
}

Value* OMGIRGenerator::decodeNonNullStructure(Value* structureID)
{
    return structureID;
}

Value* OMGIRGenerator::allocatorForWasmGCHeapCellSize(Value* sizeInBytes, BasicBlock* slowPath)
{
    static_assert(!(MarkedSpace::sizeStep & (MarkedSpace::sizeStep - 1)), "MarkedSpace::sizeStep must be a power of two.");

    ptrdiff_t allocatorBufferBaseOffset = JSWebAssemblyInstance::offsetOfAllocatorForGCObject(m_info, 0);

    unsigned stepShift = getLSBSet(MarkedSpace::sizeStep);

    auto* continuation = m_proc.addBlock();

    auto* sizeClassIndex = m_currentBlock->appendNew<Value>(m_proc, ZShr, origin(),
        m_currentBlock->appendNew<Value>(m_proc, B3::Add, origin(), sizeInBytes, constant(pointerType(), MarkedSpace::sizeStep - 1)),
        constant(Int32, stepShift));

    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
        m_currentBlock->appendNew<Value>(m_proc, Above, origin(), sizeClassIndex, constant(pointerType(), MarkedSpace::largeCutoff >> stepShift)),
        FrequentedBlock(slowPath, FrequencyClass::Rare), FrequentedBlock(continuation));
    m_currentBlock->setSuccessors(FrequentedBlock(slowPath, FrequencyClass::Rare), FrequentedBlock(continuation));
    slowPath->addPredecessor(m_currentBlock);
    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = continuation;

    Value* address = m_currentBlock->appendNew<Value>(m_proc, Add, origin(),
        m_currentBlock->appendNew<Value>(m_proc, B3::Add, origin(), instanceValue(), constant(pointerType(), allocatorBufferBaseOffset)),
        m_currentBlock->appendNew<Value>(m_proc, Mul, origin(), sizeClassIndex, constant(pointerType(), sizeof(Allocator))));

    auto* result = m_currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, pointerType(), origin(), address);
    result->setReadsMutability(B3::Mutability::Immutable);
    result->setControlDependent(false);
    return result;
}

Value* OMGIRGenerator::allocateWasmGCHeapCell(Value* allocator, BasicBlock* slowPath)
{
    auto* continuation = m_proc.addBlock();
    auto* patchpoint = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, pointerType(), origin());
    if (isARM64()) {
        // emitAllocateWithNonNullAllocator uses the scratch registers on ARM.
        patchpoint->clobber(RegisterSetBuilder::macroClobberedGPRs());
    }
    patchpoint->effects.terminal = true;
    patchpoint->appendSomeRegisterWithClobber(allocator);
    patchpoint->numGPScratchRegisters++;
    patchpoint->resultConstraints = { ValueRep::SomeEarlyRegister };

    patchpoint->setGenerator(
        [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsageIf allowScratchIf(jit, isARM64());
            CCallHelpers::JumpList jumpToSlowPath;

            GPRReg allocatorGPR = params[1].gpr();

            // We use a patchpoint to emit the allocation path because whenever we mess with
            // allocation paths, we already reason about them at the machine code level. We know
            // exactly what instruction sequence we want. We're confident that no compiler
            // optimization could make this code better. So, it's best to have the code in
            // AssemblyHelpers::emitAllocate(). That way, the same optimized path is shared by
            // all of the compiler tiers.
            jit.emitAllocateWithNonNullAllocator(
                params[0].gpr(), JITAllocator::variableNonNull(), allocatorGPR, params.gpScratch(0),
                jumpToSlowPath, CCallHelpers::SlowAllocationResult::UndefinedBehavior);

            CCallHelpers::Jump jumpToSuccess;
            if (!params.fallsThroughToSuccessor(0))
                jumpToSuccess = jit.jump();

            Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();

            params.addLatePath(
                [=] (CCallHelpers& jit) {
                    jumpToSlowPath.linkTo(*labels[1], &jit);
                    if (jumpToSuccess.isSet())
                        jumpToSuccess.linkTo(*labels[0], &jit);
                });
        });

    m_currentBlock->appendSuccessor({ continuation, FrequencyClass::Normal });
    m_currentBlock->appendSuccessor({ slowPath, FrequencyClass::Rare });

    m_currentBlock = continuation;
    return patchpoint;
}

Value* OMGIRGenerator::allocateWasmGCObject(Value* allocator, Value* structureID, Value* typeInfo, BasicBlock* slowPath)
{
    auto* cell = allocateWasmGCHeapCell(allocator, slowPath);

    auto* storeStructureID = m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, origin(), structureID, cell, safeCast<int32_t>(JSCell::structureIDOffset()));
    m_heaps.decorateMemory(&m_heaps.JSCell_structureID, storeStructureID);

    auto* storeUsefulBytes = m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, origin(), typeInfo, cell, safeCast<int32_t>(JSCell::indexingTypeAndMiscOffset()));
    m_heaps.decorateMemory(&m_heaps.JSCell_usefulBytes, storeUsefulBytes);

    auto* storeButterfly = m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, origin(), constant(pointerType(), 0), cell, safeCast<int32_t>(JSObject::butterflyOffset()));
    m_heaps.decorateMemory(&m_heaps.JSObject_butterfly, storeButterfly);
    return cell;
}

Value* OMGIRGenerator::allocateWasmGCArrayUninitialized(uint32_t typeIndex, Value* size)
{
    auto* slowPath = m_proc.addBlock();
    auto* continuation = m_proc.addBlock();

    auto* structureID = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGCObjectStructureID(m_info, typeIndex)));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_gcObjectStructureIDs[typeIndex], structureID);
    structureID->setReadsMutability(B3::Mutability::Immutable);
    structureID->setControlDependent(false);

    const ArrayType* typeDefinition = m_info.typeSignatures[typeIndex]->expand().template as<ArrayType>();
    size_t elementSize = typeDefinition->elementType().type.elementSize();
    auto* extended = pointerOfInt32(size);
    auto* shifted = m_currentBlock->appendNew<Value>(m_proc, Shl, origin(), extended, constant(Int32, getLSBSet(elementSize)));
    auto* sizeInBytes = m_currentBlock->appendNew<Value>(m_proc, Add, pointerType(), origin(), shifted, constant(pointerType(), sizeof(JSWebAssemblyArray)));
    auto* allocator = allocatorForWasmGCHeapCellSize(sizeInBytes, slowPath);
    auto* typeInfo = constant(Int32, JSWebAssemblyArray::typeInfoBlob().blob());
    auto* cell = allocateWasmGCObject(allocator, structureID, typeInfo, slowPath);
    auto* fastValue = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), wasmRefOfCell(cell));
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
    continuation->addPredecessor(m_currentBlock);

    m_currentBlock = slowPath;
    auto* slowResult = callWasmOperation(m_currentBlock, toB3Type(Types::Arrayref), operationWasmArrayNewEmpty,
        instanceValue(), constant(Int32, typeIndex), size);
    emitNullCheck(slowResult, ExceptionType::BadArrayNew);
    auto* slowValue = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), slowResult);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
    continuation->addPredecessor(m_currentBlock);

    m_currentBlock = continuation;
    auto* result = m_currentBlock->appendNew<Value>(m_proc, Phi, wasmRefType(), origin());
    fastValue->setPhi(result);
    slowValue->setPhi(result);

    auto* arraySizeStore = m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, origin(), size, pointerOfWasmRef(result), safeCast<int32_t>(JSWebAssemblyArray::offsetOfSize()));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyArray_size, arraySizeStore);
    return result;
}

Value* OMGIRGenerator::allocateWasmGCStructUninitialized(uint32_t typeIndex)
{
    auto* slowPath = m_proc.addBlock();
    auto* continuation = m_proc.addBlock();

    auto* structureID = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGCObjectStructureID(m_info, typeIndex)));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_gcObjectStructureIDs[typeIndex], structureID);
    structureID->setReadsMutability(B3::Mutability::Immutable);
    structureID->setControlDependent(false);

    const StructType* typeDefinition = m_info.typeSignatures[typeIndex]->expand().template as<StructType>();
    Value* sizeInBytes = constant(pointerType(), JSWebAssemblyStruct::allocationSize(typeDefinition->instancePayloadSize()));
    auto* allocator = allocatorForWasmGCHeapCellSize(sizeInBytes, slowPath);
    auto* typeInfo = constant(Int32, JSWebAssemblyStruct::typeInfoBlob().blob());
    auto* cell = allocateWasmGCObject(allocator, structureID, typeInfo, slowPath);
    auto* fastValue = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), wasmRefOfCell(cell));
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
    continuation->addPredecessor(m_currentBlock);

    m_currentBlock = slowPath;
    const auto type = Type { TypeKind::Ref, m_info.typeSignatures[typeIndex]->index() };
    auto* slowResult = callWasmOperation(m_currentBlock, toB3Type(type), operationWasmStructNewEmpty,
        instanceValue(), constant(Int32, typeIndex));
    emitNullCheck(slowResult, ExceptionType::BadStructNew);
    auto* slowValue = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), slowResult);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
    continuation->addPredecessor(m_currentBlock);

    m_currentBlock = continuation;
    auto* result = m_currentBlock->appendNew<Value>(m_proc, Phi, wasmRefType(), origin());
    fastValue->setPhi(result);
    slowValue->setPhi(result);

    auto* size = m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, origin(), constant(Int32, typeDefinition->instancePayloadSize()), pointerOfWasmRef(result), safeCast<int32_t>(JSWebAssemblyStruct::offsetOfSize()));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyStruct_size, size);
    return result;
}

void OMGIRGenerator::mutatorFence()
{
    if (isX86()) {
        m_currentBlock->appendNew<FenceValue>(m_proc, origin());
        return;
    }

    auto* slowPath = m_proc.addBlock();
    auto* continuation = m_proc.addBlock();

    auto* vm = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfVM()));
    m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_vm, vm);
    vm->setControlDependent(false);

    Value* shouldFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapMutatorShouldBeFenced()));
    m_heaps.decorateMemory(&m_heaps.VM_heap_mutatorShouldBeFenced, shouldFence);

    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), shouldFence, FrequentedBlock(slowPath, FrequencyClass::Rare), FrequentedBlock(continuation));
    slowPath->addPredecessor(m_currentBlock);
    continuation->addPredecessor(m_currentBlock);

    m_currentBlock = slowPath;
    m_currentBlock->appendNew<FenceValue>(m_proc, origin());
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
    continuation->addPredecessor(m_currentBlock);

    m_currentBlock = continuation;
}

Value* OMGIRGenerator::emitLoadRTTFromObject(Value* reference)
{
    Value* structureID = m_currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, Int32, origin(), reference, safeCast<int32_t>(JSCell::structureIDOffset()));
    m_heaps.decorateMemory(&m_heaps.JSCell_structureID, structureID);

    Value* structure = decodeNonNullStructure(structureID);

    auto* rtt = m_currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, pointerType(), origin(), structure, safeCast<int32_t>(WebAssemblyGCStructure::offsetOfRTT()));
    m_heaps.decorateMemory(&m_heaps.WebAssemblyGCStructure_rtt, rtt);
    rtt->setControlDependent(false);

    return rtt;
}

auto OMGIRGenerator::addAnyConvertExtern(ExpressionType reference, ExpressionType& result) -> PartialResult
{
    result = push(callWasmOperation(m_currentBlock, toB3Type(anyrefType()), operationWasmAnyConvertExtern, get(reference)));
    return { };
}

auto OMGIRGenerator::addExternConvertAny(ExpressionType reference, ExpressionType& result) -> PartialResult
{
    result = push(get(reference));
    return { };
}

auto OMGIRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), get(condition), get(nonZero), get(zero)));
    return { };
}

OMGIRGenerator::ExpressionType OMGIRGenerator::addConstant(Type type, uint64_t value)
{
    return push(constant(toB3Type(type), value));
}

auto OMGIRGenerator::addSIMDSplat(SIMDLane lane, ExpressionType scalar, ExpressionType& result) -> PartialResult
{
    Value* toSplat = get(scalar);
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), B3::VectorSplat, B3::V128, lane, SIMDSignMode::None, toSplat));
    return { };
}

auto OMGIRGenerator::addSIMDShift(SIMDLaneOperation op, SIMDInfo info, ExpressionType v, ExpressionType shift, ExpressionType& result) -> PartialResult
{
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(),
        op == SIMDLaneOperation::Shr ? B3::VectorShr : B3::VectorShl, B3::V128, info, get(v), get(shift)));
    return { };
}

auto OMGIRGenerator::addSIMDExtmul(SIMDLaneOperation op, SIMDInfo info, ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    ASSERT(info.signMode != SIMDSignMode::None);
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), op == SIMDLaneOperation::ExtmulLow ? VectorMulLow : VectorMulHigh, B3::V128, info, get(lhs), get(rhs)));
    return { };
}

auto OMGIRGenerator::addSIMDShuffle(v128_t imm, ExpressionType a, ExpressionType b, ExpressionType& result) -> PartialResult
{
    if constexpr (isX86()) {
        v128_t leftImm = imm;
        v128_t rightImm = imm;
        for (unsigned i = 0; i < 16; ++i) {
            if (leftImm.u8x16[i] > 15)
                leftImm.u8x16[i] = 0xFF; // Force OOB
            if (rightImm.u8x16[i] < 16 || rightImm.u8x16[i] > 31)
                rightImm.u8x16[i] = 0xFF; // Force OOB
        }
        // Store each byte (w/ index < 16) of `a` to result
        // and zero clear each byte (w/ index > 15) in result.
        Value* leftImmConst = m_currentBlock->appendNew<Const128Value>(m_proc, origin(), leftImm);
        Value* leftResult = m_currentBlock->appendNew<SIMDValue>(m_proc, origin(),
            VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, get(a), leftImmConst);

        // Store each byte (w/ index - 16 >= 0) of `b` to result2
        // and zero clear each byte (w/ index - 16 < 0) in result2.
        Value* rightImmConst = m_currentBlock->appendNew<Const128Value>(m_proc, origin(), rightImm);
        Value* rightResult = m_currentBlock->appendNew<SIMDValue>(m_proc, origin(),
            VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, get(b), rightImmConst);

        result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(),
            VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, leftResult, rightResult));

        return { };
    }

    if constexpr (!isARM64())
        UNREACHABLE_FOR_PLATFORM();

    Value* indexes = m_currentBlock->appendNew<Const128Value>(m_proc, origin(), imm);
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(),
        VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, get(a), get(b), indexes));

    return { };
}

auto OMGIRGenerator::addSIMDLoad(ExpressionType pointerVariable, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    Value* ptr = emitCheckAndPreparePointer(get(pointerVariable), uoffset, 16);
    int32_t offset = fixupPointerPlusOffset(ptr, uoffset);
    auto* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), B3::V128, origin(), ptr, offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, value);
    result = push(value);

    return { };
}

auto OMGIRGenerator::addSIMDStore(ExpressionType value, ExpressionType pointerVariable, uint32_t uoffset) -> PartialResult
{
    Value* ptr = emitCheckAndPreparePointer(get(pointerVariable), uoffset, 16);
    int32_t offset = fixupPointerPlusOffset(ptr, uoffset);
    auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store), origin(), get(value), ptr, offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, store);

    return { };
}

auto OMGIRGenerator::addSIMDLoadSplat(SIMDLaneOperation op, ExpressionType pointerVariable, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    size_t byteSize;

    B3::Opcode loadOp;
    B3::Type type;
    SIMDLane lane;
    switch (op) {
    case SIMDLaneOperation::LoadSplat8:
        loadOp = Load8Z;
        type = B3::Int32;
        lane = SIMDLane::i8x16;
        byteSize = 1;
        break;
    case SIMDLaneOperation::LoadSplat16:
        loadOp = Load16Z;
        type = B3::Int32;
        lane = SIMDLane::i16x8;
        byteSize = 2;
        break;
    case SIMDLaneOperation::LoadSplat32:
        loadOp = Load;
        type = B3::Int32;
        lane = SIMDLane::i32x4;
        byteSize = 4;
        break;
    case SIMDLaneOperation::LoadSplat64:
        loadOp = Load;
        type = B3::Int64;
        lane = SIMDLane::i64x2;
        byteSize = 8;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value* ptr = emitCheckAndPreparePointer(get(pointerVariable), uoffset, byteSize);
    int32_t offset = fixupPointerPlusOffset(ptr, uoffset);
    auto* memLoad = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(loadOp), type, origin(), ptr, offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, memLoad);
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), B3::VectorSplat, B3::V128, lane, SIMDSignMode::None, memLoad));

    return { };
}

auto OMGIRGenerator::addSIMDLoadLane(SIMDLaneOperation op, ExpressionType pointerVariable, ExpressionType vectorVariable, uint32_t uoffset, uint8_t laneIndex, ExpressionType& result) -> PartialResult
{
    size_t byteSize;
    B3::Opcode loadOp;
    B3::Type type;
    SIMDLane lane;
    switch (op) {
    case SIMDLaneOperation::LoadLane8:
        loadOp = Load8Z;
        type = B3::Int32;
        lane = SIMDLane::i8x16;
        byteSize = 1;
        break;
    case SIMDLaneOperation::LoadLane16:
        loadOp = Load16Z;
        type = B3::Int32;
        lane = SIMDLane::i16x8;
        byteSize = 2;
        break;
    case SIMDLaneOperation::LoadLane32:
        loadOp = Load;
        type = B3::Int32;
        lane = SIMDLane::i32x4;
        byteSize = 4;
        break;
    case SIMDLaneOperation::LoadLane64:
        loadOp = Load;
        type = B3::Int64;
        lane = SIMDLane::i64x2;
        byteSize = 8;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value* ptr = emitCheckAndPreparePointer(get(pointerVariable), uoffset, byteSize);
    int32_t offset = fixupPointerPlusOffset(ptr, uoffset);
    auto* memLoad = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(loadOp), type, origin(), ptr, offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, memLoad);
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), B3::VectorReplaceLane, B3::V128, lane, SIMDSignMode::None, laneIndex, get(vectorVariable), memLoad));

    return { };
}

auto OMGIRGenerator::addSIMDStoreLane(SIMDLaneOperation op, ExpressionType pointerVariable, ExpressionType vectorVariable, uint32_t uoffset, uint8_t laneIndex) -> PartialResult
{
    size_t byteSize;
    B3::Opcode storeOp;
    B3::Type type;
    SIMDLane lane;
    switch (op) {
    case SIMDLaneOperation::StoreLane8:
        storeOp = Store8;
        type = B3::Int32;
        lane = SIMDLane::i8x16;
        byteSize = 1;
        break;
    case SIMDLaneOperation::StoreLane16:
        storeOp = Store16;
        type = B3::Int32;
        lane = SIMDLane::i16x8;
        byteSize = 2;
        break;
    case SIMDLaneOperation::StoreLane32:
        storeOp = Store;
        type = B3::Int32;
        lane = SIMDLane::i32x4;
        byteSize = 4;
        break;
    case SIMDLaneOperation::StoreLane64:
        storeOp = Store;
        type = B3::Int64;
        lane = SIMDLane::i64x2;
        byteSize = 8;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value* ptr = emitCheckAndPreparePointer(get(pointerVariable), uoffset, byteSize);
    int32_t offset = fixupPointerPlusOffset(ptr, uoffset);
    Value* laneValue = m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), B3::VectorExtractLane, type, lane, byteSize < 4 ? SIMDSignMode::Unsigned : SIMDSignMode::None, laneIndex, get(vectorVariable));
    auto* store = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(storeOp), origin(), laneValue, ptr, offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, store);

    return { };
}

auto OMGIRGenerator::addSIMDLoadExtend(SIMDLaneOperation op, ExpressionType pointerVariable, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    B3::Opcode loadOp = Load;
    size_t byteSize = 8;
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

    Value* ptr = emitCheckAndPreparePointer(get(pointerVariable), uoffset, byteSize);
    int32_t offset = fixupPointerPlusOffset(ptr, uoffset);
    auto* memLoad = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(loadOp), B3::Double, origin(), ptr, offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, memLoad);
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), VectorExtendLow, B3::V128, SIMDInfo { lane, signMode }, memLoad));

    return { };
}

auto OMGIRGenerator::addSIMDLoadPad(SIMDLaneOperation op, ExpressionType pointerVariable, uint32_t uoffset, ExpressionType& result) -> PartialResult
{
    B3::Type loadType;
    unsigned byteSize;
    SIMDLane lane;
    uint8_t idx = 0;
    switch (op) {
    case SIMDLaneOperation::LoadPad32:
        loadType = B3::Float;
        byteSize = 4;
        lane = SIMDLane::f32x4;
        break;
    case SIMDLaneOperation::LoadPad64:
        loadType = B3::Double;
        byteSize = 8;
        lane = SIMDLane::f64x2;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value* ptr = emitCheckAndPreparePointer(get(pointerVariable), uoffset, byteSize);
    int32_t offset = fixupPointerPlusOffset(ptr, uoffset);
    auto* memLoad = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), loadType, origin(), ptr, offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyMemory, memLoad);
    result = push(m_currentBlock->appendNew<SIMDValue>(m_proc, origin(), B3::VectorReplaceLane, B3::V128, lane, SIMDSignMode::None, idx,
        m_currentBlock->appendNew<Const128Value>(m_proc, origin(), v128_t { }),
        memLoad));

    return { };
}

// OMG prefers not to see I64s as one value, but rather as two stitched I32s. BBQ prefers to see pairs
// of registers to represent I64s. These two different ways of restoring live values represent this;
// OSR directly loads I64s from the buffer, exceptions load two I32s and stitch them.
Value* OMGIRGenerator::loadFromScratchBuffer(OSRBufferMode mode, unsigned& indexInBuffer, Value* pointer, B3::Type type)
{
    unsigned valueSize = m_proc.usesSIMD() ? 2 : 1;
    ASSERT(!m_proc.usesSIMD());
    int32_t offset = valueSize * sizeof(uint64_t) * (indexInBuffer++);
    RELEASE_ASSERT(type.isNumeric());
    if (mode == SplitI64 && type.kind() == B3::TypeKind::Int64) {
        int32_t offsetHi = valueSize * sizeof(uint64_t) * (indexInBuffer++);
        auto* lo = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), pointer, offset);
        auto* hi = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), pointer, offsetHi);
        return m_currentBlock->appendNew<Value>(m_proc, Stitch, type, origin(), lo, hi);
    }
    return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, type, origin(), pointer, offset);
}

void OMGIRGenerator::connectControlAtEntrypoint(OSRBufferMode mode, unsigned& indexInBuffer, Value* pointer, ControlData& data, Stack& expressionStack, ControlData& currentData, bool fillLoopPhis)
{
    TRACE_CF("Connect control at entrypoint");
    for (unsigned i = 0; i < expressionStack.size(); i++) {
        TypedExpression value = expressionStack[i];
        auto* load = loadFromScratchBuffer(mode, indexInBuffer, pointer, value->type());
        if (fillLoopPhis)
            m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), load, data.phis[i]);
        else
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, origin(), value.value(), load);
    }
    if (ControlType::isAnyCatch(data) && &data != &currentData) {
        auto* load = loadFromScratchBuffer(mode, indexInBuffer, pointer, pointerType());
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, origin(), data.exception(), load);
    }
};

auto OMGIRGenerator::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex) -> PartialResult
{
    TRACE_CF("LOOP: entering loop index: ", loopIndex, " signature: ", *signature.m_signature);
    BasicBlock* body = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    block = ControlData(m_proc, origin(), signature, BlockType::Loop, m_stackSize, continuation, body);

    unsigned offset = enclosingStack.size() - signature.m_signature->argumentCount();
    for (unsigned i = 0; i < signature.m_signature->argumentCount(); ++i) {
        TypedExpression value = enclosingStack.at(offset + i);
        Value* phi = block.phis[i];
        m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), get(value), phi);
        body->append(phi);
        set(body, value, phi);
        newStack.append(value);
    }
    enclosingStack.shrink(offset);

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), body);
    if (loopIndex == m_loopIndexForOSREntry) {
        dataLogLnIf(WasmOMGIRGeneratorInternal::verbose, "Setting up for OSR entry");

        m_currentBlock = m_rootBlocks[0].block;
        Value* pointer = m_rootBlocks[0].block->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR0);

        unsigned indexInBuffer = 0;

        for (auto& local : m_locals)
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, loadFromScratchBuffer(LoadI64, indexInBuffer, pointer, local->type()));

        for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
            auto& data = m_parser->controlStack()[controlIndex].controlData;
            auto& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
            connectControlAtEntrypoint(LoadI64, indexInBuffer, pointer, data, expressionStack, block);
            if (ControlType::isTry(data))
                ++indexInBuffer;
        }
        connectControlAtEntrypoint(LoadI64, indexInBuffer, pointer, block, enclosingStack, block);
        connectControlAtEntrypoint(LoadI64, indexInBuffer, pointer, block, newStack, block, true);

        ASSERT(!m_proc.usesSIMD() || m_compilationMode == CompilationMode::OMGForOSREntryMode);
        unsigned valueSize = m_proc.usesSIMD() ? 2 : 1;
        *m_osrEntryScratchBufferSize = valueSize * indexInBuffer;
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), body);
        body->addPredecessor(m_currentBlock);
    }

    m_currentBlock = body;
    return { };
}

OMGIRGenerator::ControlData OMGIRGenerator::addTopLevel(BlockSignature signature)
{
    TRACE_CF("TopLevel: ", *signature.m_signature);
    return ControlData(m_proc, Origin(), signature, BlockType::TopLevel, m_stackSize, m_proc.addBlock());
}

auto OMGIRGenerator::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> PartialResult
{
    TRACE_CF("Block: ", *signature.m_signature);
    BasicBlock* continuation = m_proc.addBlock();

    splitStack(signature, enclosingStack, newStack);
    newBlock = ControlData(m_proc, origin(), signature, BlockType::Block, m_stackSize, continuation);
    return { };
}

auto OMGIRGenerator::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    // FIXME: This needs to do some kind of stack passing.

    BasicBlock* taken = m_proc.addBlock();
    BasicBlock* notTaken = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();
    FrequencyClass takenFrequency = FrequencyClass::Normal;
    FrequencyClass notTakenFrequency = FrequencyClass::Normal;

    BranchHint hint = m_info.getBranchHint(m_functionIndex, m_parser->currentOpcodeStartingOffset());
    switch (hint) {
    case BranchHint::Unlikely:
        takenFrequency = FrequencyClass::Rare;
        break;
    case BranchHint::Likely:
        notTakenFrequency = FrequencyClass::Rare;
        break;
    case BranchHint::Invalid:
        break;
    }

    m_currentBlock->appendNew<Value>(m_proc, B3::Branch, origin(), get(condition));
    m_currentBlock->setSuccessors(FrequentedBlock(taken, takenFrequency), FrequentedBlock(notTaken, notTakenFrequency));
    taken->addPredecessor(m_currentBlock);
    notTaken->addPredecessor(m_currentBlock);

    m_currentBlock = taken;
    TRACE_CF("IF");
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(m_proc, origin(), signature, BlockType::If, m_stackSize, continuation, notTaken);
    return { };
}

auto OMGIRGenerator::addElse(ControlData& data, const Stack& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addElseToUnreachable(data);
}

auto OMGIRGenerator::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.blockType() == BlockType::If);
    m_stackSize = data.stackSize() + data.m_signature.m_signature->argumentCount();
    m_currentBlock = data.special;
    data.convertIfToBlock();
    TRACE_CF("ELSE");
    return { };
}

auto OMGIRGenerator::addTry(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    ++m_tryCatchDepth;
    TRACE_CF("TRY");

    BasicBlock* continuation = m_proc.addBlock();
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(m_proc, origin(), signature, BlockType::Try, m_stackSize, continuation, advanceCallSiteIndex(), m_tryCatchDepth);
    return { };
}

auto OMGIRGenerator::addTryTable(BlockSignature signature, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack) -> PartialResult
{
    ++m_tryCatchDepth;
    TRACE_CF("TRY");

    auto targetList = targets.map(
        [&](const auto& target) -> ControlData::TryTableTarget {
            return {
                target.type,
                target.tag,
                target.exceptionSignature,
                target.target
            };
        }
    );

    BasicBlock* continuation = m_proc.addBlock();
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(m_proc, origin(), signature, BlockType::TryTable, m_stackSize, continuation, advanceCallSiteIndex(), m_tryCatchDepth);
    result.setTryTableTargets(WTFMove(targetList));

    return { };
}

auto OMGIRGenerator::addCatch(unsigned exceptionIndex, const TypeDefinition& signature, Stack& currentStack, ControlType& data, ResultList& results) -> PartialResult
{
    TRACE_CF("CATCH: ", signature);
    unifyValuesWithBlock(currentStack, data);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addCatchToUnreachable(exceptionIndex, signature, data, results);
}

RefPtr<PatchpointExceptionHandle> OMGIRGenerator::preparePatchpointForExceptions(BasicBlock* block, PatchpointValue* patch)
{
    bool mustSaveState = m_tryCatchDepth;

    if (!mustSaveState)
        return nullptr;

    unsigned firstStackmapChildOffset = patch->numChildren();
    unsigned firstStackmapParamOffset = firstStackmapChildOffset + m_proc.resultCount(patch->type());

    Vector<Value*> liveValues;
    Origin origin = this->origin();

    Vector<OMGIRGenerator*> frames;
    for (auto* currentFrame = this; currentFrame; currentFrame = currentFrame->m_inlineParent)
        frames.append(currentFrame);
    frames.reverse();

    auto addLiveValue = [&] (Value* v) {
        if (is32Bit() && v->type().kind() == Int64) {
            liveValues.append(block->appendNew<ExtractValue>(m_proc, origin, Int32, v, ExtractValue::s_int64LowBits));
            liveValues.append(block->appendNew<ExtractValue>(m_proc, origin, Int32, v, ExtractValue::s_int64HighBits));
        } else
            liveValues.append(v);
    };

    for (auto* currentFrame : frames) {
        for (Variable* local : currentFrame->m_locals) {
            Value* result = block->appendNew<VariableValue>(m_proc, B3::Get, origin, local);
            addLiveValue(result);
        }
        for (unsigned controlIndex = 0; controlIndex < currentFrame->m_parser->controlStack().size(); ++controlIndex) {
            ControlData& data = currentFrame->m_parser->controlStack()[controlIndex].controlData;
            Stack& expressionStack = currentFrame->m_parser->controlStack()[controlIndex].enclosedExpressionStack;
            for (Variable* value : expressionStack)
                addLiveValue(get(block, value));
            if (ControlType::isAnyCatch(data))
                addLiveValue(get(block, data.exception()));
        }
        // inlineParent frames only
        if (currentFrame != this) {
        for (Variable* value : currentFrame->m_parser->expressionStack())
            addLiveValue(get(block, value));
        }
    }

    patch->effects.exitsSideways = true;
    patch->appendVectorWithRep(liveValues, ValueRep::LateColdAny);

    return PatchpointExceptionHandle::create(m_hasExceptionHandlers, callSiteIndex(), static_cast<unsigned>(liveValues.size()), firstStackmapParamOffset, firstStackmapChildOffset);
}

auto OMGIRGenerator::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& signature, ControlType& data, ResultList& results) -> PartialResult
{
    Value* payload = emitCatchImpl(CatchKind::Catch, data, exceptionIndex);
    unsigned offset = 0;
    for (unsigned i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        Type type = signature.as<FunctionSignature>()->argumentType(i);
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(type), origin(), payload, safeCast<int32_t>(offset * sizeof(uint64_t)));
        results.append(push(value));
        offset += type.kind == TypeKind::V128 ? 2 : 1;
    }
    TRACE_CF("CATCH");
    return { };
}

auto OMGIRGenerator::addCatchAll(Stack& currentStack, ControlType& data) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data);
    TRACE_CF("CATCH_ALL");
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addCatchAllToUnreachable(data);
}

auto OMGIRGenerator::addCatchAllToUnreachable(ControlType& data) -> PartialResult
{
    emitCatchImpl(CatchKind::CatchAll, data);
    return { };
}

Value* OMGIRGenerator::emitCatchImpl(CatchKind kind, ControlType& data, unsigned exceptionIndex)
{
    m_currentBlock = m_proc.addBlock();
    m_rootBlocks.append({ m_currentBlock, usesSIMD() });
    m_stackSize = data.stackSize();

    if (ControlType::isTry(data)) {
        if (kind == CatchKind::Catch)
            data.convertTryToCatch(advanceCallSiteIndex(), m_proc.addVariable(pointerType()));
        else
            data.convertTryToCatchAll(advanceCallSiteIndex(), m_proc.addVariable(pointerType()));
    }
    // We convert from "try" to "catch" ControlType above. This doesn't
    // happen if ControlType is already a "catch". This can happen when
    // we have multiple catches like "try {} catch(A){} catch(B){}...CatchAll(E){}"
    ASSERT(ControlType::isAnyCatch(data));

    HandlerType handlerType = kind == CatchKind::Catch ? HandlerType::Catch : HandlerType::CatchAll;
    m_exceptionHandlers.append({ handlerType, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, exceptionIndex });

    reloadMemoryRegistersFromInstance(m_info.memory, instanceValue(), m_currentBlock);

    Value* pointer = m_currentBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR0);
    Value* exception = m_currentBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR1);
    Value* buffer = m_currentBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR2);

    unsigned indexInBuffer = 0;

    Vector<OMGIRGenerator*> frames;
    for (auto* currentFrame = this; currentFrame; currentFrame = currentFrame->m_inlineParent)
        frames.append(currentFrame);
    frames.reverse();

    for (auto* currentFrame : frames) {
        for (auto& local : currentFrame->m_locals)
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, loadFromScratchBuffer(SplitI64, indexInBuffer, pointer, local->type()));

        for (unsigned controlIndex = 0; controlIndex < currentFrame->m_parser->controlStack().size(); ++controlIndex) {
            auto& controlData = currentFrame->m_parser->controlStack()[controlIndex].controlData;
            auto& expressionStack = currentFrame->m_parser->controlStack()[controlIndex].enclosedExpressionStack;
            connectControlAtEntrypoint(SplitI64, indexInBuffer, pointer, controlData, expressionStack, data);
        }

        auto& topControlData = currentFrame->m_parser->controlStack().last().controlData;
        auto& topExpressionStack = currentFrame->m_parser->expressionStack();
        connectControlAtEntrypoint(SplitI64, indexInBuffer, pointer, topControlData, topExpressionStack, data);
    }

    set(data.exception(), exception);
    TRACE_CF("CATCH");

    return buffer;
}

auto OMGIRGenerator::emitCatchTableImpl(ControlData& data, const ControlData::TryTableTarget& target) -> void
{
    auto block = m_proc.addBlock();
    m_rootBlocks.append({ block, usesSIMD() });
    auto oldBlock = m_currentBlock;
    m_currentBlock = block;

    HandlerType handlerType;
    switch (target.type) {
    case CatchKind::Catch:
        handlerType = HandlerType::TryTableCatch;
        break;
    case CatchKind::CatchRef:
        handlerType = HandlerType::TryTableCatchRef;
        break;
    case CatchKind::CatchAll:
        handlerType = HandlerType::TryTableCatchAll;
        break;
    case CatchKind::CatchAllRef:
        handlerType = HandlerType::TryTableCatchAllRef;
        break;
    }
    m_exceptionHandlers.append({ handlerType, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, target.tag });

    auto signature = target.exceptionSignature;

    reloadMemoryRegistersFromInstance(m_info.memory, instanceValue(), m_currentBlock);

    Value* pointer = m_currentBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR0);
    Value* exception = m_currentBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR1);
    Value* buffer = m_currentBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR2);

    unsigned indexInBuffer = 0;

    Vector<OMGIRGenerator*> frames;
    for (auto* currentFrame = this; currentFrame; currentFrame = currentFrame->m_inlineParent)
        frames.append(currentFrame);
    frames.reverse();

    for (auto* currentFrame : frames) {
        for (auto& local : currentFrame->m_locals)
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, loadFromScratchBuffer(SplitI64, indexInBuffer, pointer, local->type()));

        for (unsigned controlIndex = 0; controlIndex < currentFrame->m_parser->controlStack().size(); ++controlIndex) {
            auto& controlData = currentFrame->m_parser->controlStack()[controlIndex].controlData;
            auto& expressionStack = currentFrame->m_parser->controlStack()[controlIndex].enclosedExpressionStack;
            connectControlAtEntrypoint(SplitI64, indexInBuffer, pointer, controlData, expressionStack, data);
        }

        auto& topControlData = currentFrame->m_parser->controlStack().last().controlData;
        auto& topExpressionStack = currentFrame->m_parser->expressionStack();
        connectControlAtEntrypoint(SplitI64, indexInBuffer, pointer, topControlData, topExpressionStack, data);
    }


    Stack resultStack;
    if (target.type == CatchKind::Catch || target.type == CatchKind::CatchRef) {
        unsigned offset = 0;
        for (unsigned i = 0; i < signature->template as<FunctionSignature>()->argumentCount(); ++i) {
            Type type = signature->as<FunctionSignature>()->argumentType(i);
            Variable* var = m_proc.addVariable(toB3Type(type));
            Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(type), origin(), buffer, safeCast<int32_t>(offset * sizeof(uint64_t)));
            set(var, value);
            resultStack.constructAndAppend(type, var);
            offset += type.kind == TypeKind::V128 ? 2 : 1;
        }
    }

    if (target.type == CatchKind::CatchRef || target.type == CatchKind::CatchAllRef) {
        Variable* var = m_proc.addVariable(wasmRefType());
        exception = wasmRefOfCell(exception);
        set(var, exception);
        push(exception);
        resultStack.constructAndAppend(Type { TypeKind::RefNull, static_cast<TypeIndex>(TypeKind::Exnref) }, var);
    }

    auto& targetControl = m_parser->resolveControlRef(target.target).controlData;
    unifyValuesWithBlock(resultStack, targetControl);

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), FrequentedBlock(targetControl.targetBlockForBranch(), FrequencyClass::Normal));
    targetControl.targetBlockForBranch()->addPredecessor(block);

    m_currentBlock = oldBlock;
}

auto OMGIRGenerator::addDelegate(ControlType& target, ControlType& data) -> PartialResult
{
    return addDelegateToUnreachable(target, data);
}

auto OMGIRGenerator::addDelegateToUnreachable(ControlType& target, ControlType& data) -> PartialResult
{
    TRACE_CF("DELEGATE");
    unsigned targetDepth = 0;
    if (m_inlineParent)
        targetDepth += m_inlineParent->m_tryCatchDepth;

    if (ControlType::isTry(target))
        targetDepth = target.tryDepth();

    m_exceptionHandlers.append({ HandlerType::Delegate, data.tryStart(), advanceCallSiteIndex(), 0, m_tryCatchDepth, targetDepth });
    return { };
}

auto OMGIRGenerator::addThrow(unsigned exceptionIndex, ArgumentList& args, Stack&) -> PartialResult
{
    TRACE_CF("THROW");

    advanceCallSiteIndex();
    PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, origin(), cloningForbidden(Patchpoint));
    patch->effects.terminal = true;
    patch->append(instanceValue(), ValueRep::reg(GPRInfo::argumentGPR0));
    unsigned offset = 0;
    for (auto arg : args) {
        patch->append(get(arg), ValueRep::stackArgument(safeCast<intptr_t>(offset * sizeof(EncodedJSValue))));
        offset += arg->type().isVector() ? 2 : 1;
    }
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, offset);
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()));
    auto handle = preparePatchpointForExceptions(m_currentBlock, patch);
    patch->setGenerator([this, exceptionIndex, handle = WTFMove(handle), origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        if (handle)
            handle->collectStackMap(this, params);
        RELEASE_ASSERT(origin.wasmOrigin());
        emitThrowImpl(jit, exceptionIndex);
    });
    m_currentBlock->append(patch);

    return { };
}

auto WARN_UNUSED_RETURN OMGIRGenerator::addThrowRef(TypedExpression exnref, Stack&) -> PartialResult
{
    TRACE_CF("THROW_REF");

    Value* exception = get(exnref);
    Value* exceptionLo = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), exception);
    Value* exceptionHi = m_currentBlock->appendNew<Value>(m_proc, TruncHigh, origin(), exception);
    if (exnref.type().isNullable())
        emitNullCheck(exception, ExceptionType::NullExnrefReference);

    advanceCallSiteIndex();
    PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, origin(), cloningForbidden(Patchpoint));
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()));
    patch->effects.terminal = true;
    patch->append(instanceValue(), ValueRep::reg(GPRInfo::argumentGPR0));
    patch->append(exceptionLo, ValueRep::reg(GPRInfo::argumentGPR2));
    patch->append(exceptionHi, ValueRep::reg(GPRInfo::argumentGPR3));
    auto handle = preparePatchpointForExceptions(m_currentBlock, patch);
    patch->setGenerator([this, handle = WTFMove(handle), origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        if (handle)
            handle->collectStackMap(this, params);
        RELEASE_ASSERT(origin.wasmOrigin());
        emitThrowRefImpl(jit);
    });
    m_currentBlock->append(patch);
    return { };
}

auto OMGIRGenerator::addRethrow(unsigned, ControlType& data) -> PartialResult
{
    TRACE_CF("RETHROW");

    Value* exception = get(data.exception());

    advanceCallSiteIndex();
    PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, origin(), cloningForbidden(Patchpoint));
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()));
    patch->effects.terminal = true;
    patch->append(instanceValue(), ValueRep::reg(GPRInfo::argumentGPR0));
    patch->append(exception, ValueRep::reg(GPRInfo::argumentGPR2));
    patch->append(constant(Int32, JSValue::CellTag), ValueRep::reg(GPRInfo::argumentGPR3));
    auto handle = preparePatchpointForExceptions(m_currentBlock, patch);
    patch->setGenerator([this, handle = WTFMove(handle), origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        if (handle)
            handle->collectStackMap(this, params);
        RELEASE_ASSERT(origin.wasmOrigin());
        emitThrowRefImpl(jit);
    });
    m_currentBlock->append(patch);

    return { };
}

auto OMGIRGenerator::addInlinedReturn(const auto& returnValues) -> PartialResult
{
    dataLogLnIf(WasmOMGIRGeneratorInternal::verboseInlining, "Returning inline to BB ", *m_returnContinuation);

    auto* signature = m_parser->signature().as<FunctionSignature>();
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(m_parser->signature(), CallRole::Callee);
    RELEASE_ASSERT(returnValues.size() >= wasmCallInfo.results.size());
    RELEASE_ASSERT(signature->returnCount() == wasmCallInfo.results.size());

    if (!m_inlinedResults.size() && wasmCallInfo.results.size()) {
        for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i)
            m_inlinedResults.append(m_proc.addVariable(toB3Type(signature->returnType(i))));
    }
    RELEASE_ASSERT(m_inlinedResults.size() == wasmCallInfo.results.size());

    unsigned offset = returnValues.size() - wasmCallInfo.results.size();
    for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i)
        m_currentBlock->appendNew<B3::VariableValue>(m_proc, B3::Set, origin(), m_inlinedResults[i], get(returnValues[offset + i]));

    m_currentBlock->appendNewControlValue(m_proc, B3::Jump, origin(), FrequentedBlock(m_returnContinuation));
    return { };
}

auto OMGIRGenerator::addReturn(const ControlData&, const Stack& returnValues) -> PartialResult
{
    TRACE_CF("RETURN");
    if (m_returnContinuation)
        return addInlinedReturn(returnValues);

    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(m_parser->signature(), CallRole::Callee);
    PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, origin());
    patch->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        params.code().emitEpilogue(jit);
    });
    patch->effects.terminal = true;

    RELEASE_ASSERT(returnValues.size() >= wasmCallInfo.results.size());
    unsigned offset = returnValues.size() - wasmCallInfo.results.size();
    for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i) {
        B3::ValueRep rep = wasmCallInfo.results[i].location;
        if (rep.isStack()) {
            B3::Value* address = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(), constant(pointerType(), rep.offsetFromFP()));
            m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, Origin(), get(returnValues[offset + i]), address);
        } else {
            ASSERT(rep.isReg());
            auto result = get(returnValues[offset + i]);
            if (rep.isFPR())
                patch->append(result, B3::ValueRep(wasmCallInfo.results[i].location.fpr()));
            else if (result->type() != Int64)
                patch->append(result, B3::ValueRep(wasmCallInfo.results[i].location.jsr().payloadGPR()));
            else {
                auto* hi = m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), Int32, result, ExtractValue::s_int64HighBits);
                auto* lo = m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), Int32, result, ExtractValue::s_int64LowBits);
                patch->append(hi, B3::ValueRep(wasmCallInfo.results[i].location.jsr().tagGPR()));
                patch->append(lo, B3::ValueRep(wasmCallInfo.results[i].location.jsr().payloadGPR()));
            }
        }

        TRACE_VALUE(m_parser->signature().as<FunctionSignature>()->returnType(i), get(returnValues[offset + i]), "put to return value ", i);
    }

    m_currentBlock->append(patch);
    return { };
}

auto OMGIRGenerator::addBranch(ControlData& data, ExpressionType condition, const Stack& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data);

    BasicBlock* target = data.targetBlockForBranch();
    FrequencyClass targetFrequency = FrequencyClass::Normal;
    FrequencyClass continuationFrequency = FrequencyClass::Normal;

    BranchHint hint = m_info.getBranchHint(m_functionIndex, m_parser->currentOpcodeStartingOffset());
    switch (hint) {
    case BranchHint::Unlikely:
        targetFrequency = FrequencyClass::Rare;
        break;
    case BranchHint::Likely:
        continuationFrequency = FrequencyClass::Rare;
        break;
    case BranchHint::Invalid:
        break;
    }

    TRACE_CF("BRANCH to ", *target);

    if (condition) {
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNew<Value>(m_proc, B3::Branch, origin(), get(condition));
        m_currentBlock->setSuccessors(FrequentedBlock(target, targetFrequency), FrequentedBlock(continuation, continuationFrequency));
        target->addPredecessor(m_currentBlock);
        continuation->addPredecessor(m_currentBlock);
        m_currentBlock = continuation;
    } else {
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), FrequentedBlock(target, targetFrequency));
        target->addPredecessor(m_currentBlock);
    }

    return { };
}

auto OMGIRGenerator::addBranchNull(ControlData& data, ExpressionType reference, const Stack& returnValues, bool shouldNegate, ExpressionType& result) -> PartialResult
{
    auto condition = push(m_currentBlock->appendNew<Value>(m_proc, shouldNegate ? B3::NotEqual : B3::Equal, origin(), get(reference), m_currentBlock->appendNew<WasmConstRefValue>(m_proc, origin(), JSValue::encode(jsNull()))));
    // We should pop the condition here to keep stack size consistent.
    --m_stackSize;

    WASM_FAIL_IF_HELPER_FAILS(addBranch(data, condition, returnValues));

    if (!shouldNegate)
        result = push(get(reference));

    return { };
}

auto OMGIRGenerator::addBranchCast(ControlData& data, TypedExpression reference, const Stack& returnValues, bool allowNull, int32_t heapType, bool shouldNegate) -> PartialResult
{
    ExpressionType condition;
    emitRefTestOrCast(CastKind::Test, reference, allowNull, heapType, shouldNegate, condition);
    // We should pop the condition here to keep stack size consistent.
    --m_stackSize;

    WASM_FAIL_IF_HELPER_FAILS(addBranch(data, condition, returnValues));

    return { };
}

auto OMGIRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const Stack& expressionStack) -> PartialResult
{
    TRACE_CF("SWITCH");
    UNUSED_PARAM(expressionStack);
    for (size_t i = 0; i < targets.size(); ++i)
        unifyValuesWithBlock(expressionStack, *targets[i]);
    unifyValuesWithBlock(expressionStack, defaultTarget);

    SwitchValue* switchValue = m_currentBlock->appendNew<SwitchValue>(m_proc, origin(), get(condition));
    switchValue->setFallThrough(FrequentedBlock(defaultTarget.targetBlockForBranch()));
    for (size_t i = 0; i < targets.size(); ++i)
        switchValue->appendCase(SwitchCase(i, FrequentedBlock(targets[i]->targetBlockForBranch())));

    return { };
}

auto OMGIRGenerator::endBlock(ControlEntry& entry, Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    ASSERT(expressionStack.size() == data.signature().m_signature->returnCount());
    if (data.blockType() != BlockType::Loop)
        unifyValuesWithBlock(expressionStack, data);

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    data.continuation->addPredecessor(m_currentBlock);

    return addEndToUnreachable(entry, expressionStack);
}

auto OMGIRGenerator::addEndToUnreachable(ControlEntry& entry, const Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;
    m_stackSize = data.stackSize();

    if (data.blockType() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    } else if (data.blockType() == BlockType::Try || data.blockType() == BlockType::Catch)
        --m_tryCatchDepth;
    else if (data.blockType() == BlockType::TryTable) {
        // emit each handler as a new basic block
        data.endTryTable(advanceCallSiteIndex());
        auto targets = data.m_tryTableTargets;
        for (auto& target : targets)
            emitCatchTableImpl(data, target);
        --m_tryCatchDepth;
    }

    auto blockSignature = data.signature();
    if (data.blockType() != BlockType::Loop) {
        for (unsigned i = 0; i < blockSignature.m_signature->returnCount(); ++i) {
            Value* result = data.phis[i];
            m_currentBlock->append(result);
            entry.enclosedExpressionStack.constructAndAppend(blockSignature.m_signature->returnType(i), push(result));
        }
    } else {
        for (unsigned i = 0; i < blockSignature.m_signature->returnCount(); ++i) {
            if (i < expressionStack.size()) {
                ++m_stackSize;
                entry.enclosedExpressionStack.append(expressionStack[i]);
            } else {
                Type returnType = blockSignature.m_signature->returnType(i);
                entry.enclosedExpressionStack.constructAndAppend(returnType, push(constant(toB3Type(returnType), 0xbbadbeef)));
            }
        }
    }

    if constexpr (WasmOMGIRGeneratorInternal::traceStackValues) {
        m_parser->expressionStack().swap(entry.enclosedExpressionStack);
        TRACE_CF("END: ", *blockSignature.m_signature, " block type ", (int) data.blockType());
        m_parser->expressionStack().swap(entry.enclosedExpressionStack);
    }

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.blockType() == BlockType::TopLevel)
        return addReturn(entry.controlData, entry.enclosedExpressionStack);

    return { };
}

Vector<ConstrainedValue> OMGIRGenerator::createCallConstrainedArgs(BasicBlock* block, const CallInformation& wasmCalleeInfo, const ArgumentList& tmpArgs)
{
    Vector<ConstrainedValue> constrainedPatchArgs;
    for (unsigned i = 0; i < tmpArgs.size(); ++i) {
        auto dstLocation = wasmCalleeInfo.params[i];
        if (tmpArgs[i]->type() == Int64 && dstLocation.location.isGPR()) {
            auto int64 = get(block, tmpArgs[i]);
            auto hi = block->appendNew<ExtractValue>(m_proc, origin(), Int32, int64, ExtractValue::s_int64HighBits);
            auto lo = block->appendNew<ExtractValue>(m_proc, origin(), Int32, int64, ExtractValue::s_int64LowBits);
            constrainedPatchArgs.append(B3::ConstrainedValue(lo, ValueRep::reg(dstLocation.location.jsr().payloadGPR())));
            constrainedPatchArgs.append(B3::ConstrainedValue(hi, ValueRep::reg(dstLocation.location.jsr().tagGPR())));
            continue;
        }

        if (tmpArgs[i]->type() == Int64) {
            auto int64 = get(block, tmpArgs[i]);
            auto hi = block->appendNew<ExtractValue>(m_proc, origin(), Int32, int64, ExtractValue::s_int64HighBits);
            auto lo = block->appendNew<ExtractValue>(m_proc, origin(), Int32, int64, ExtractValue::s_int64LowBits);

            if (dstLocation.location.isStack()) {
                constrainedPatchArgs.append(B3::ConstrainedValue(lo, ValueRep::stack(dstLocation.location.offsetFromFP())));
                constrainedPatchArgs.append(B3::ConstrainedValue(hi, ValueRep::stack(dstLocation.location.offsetFromFP() + static_cast<intptr_t>(sizeof(int)))));
            } else {
                ASSERT(dstLocation.location.isStackArgument());
                constrainedPatchArgs.append(B3::ConstrainedValue(lo, ValueRep::stackArgument(dstLocation.location.offsetFromSP())));
                constrainedPatchArgs.append(B3::ConstrainedValue(hi, ValueRep::stackArgument(dstLocation.location.offsetFromSP() + static_cast<intptr_t>(sizeof(int)))));
            }
            continue;
        }
        constrainedPatchArgs.append(B3::ConstrainedValue(get(block, tmpArgs[i]), dstLocation));
    }

    return constrainedPatchArgs;
}


auto OMGIRGenerator::createCallPatchpoint(BasicBlock* block, const TypeDefinition& signature, const CallInformation& wasmCalleeInfo, const ArgumentList& tmpArgs) -> CallPatchpointData
{
    auto& functionSignature = *signature.as<FunctionSignature>();
    auto returnType = toB3ResultType(&signature);

    auto constrainedPatchArgs = createCallConstrainedArgs(block, wasmCalleeInfo, tmpArgs);

    advanceCallSiteIndex();
    PatchpointValue* patchpoint = m_proc.add<PatchpointValue>(returnType, origin());
    patchpoint->effects.writesPinned = true;
    patchpoint->effects.readsPinned = true;
    patchpoint->clobberEarly(RegisterSetBuilder::macroClobberedGPRs());
    patchpoint->clobberLate(RegisterSetBuilder::registersToSaveForJSCall(m_proc.usesSIMD() ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()));
    patchpoint->appendVector(constrainedPatchArgs);
    auto exceptionHandle = preparePatchpointForExceptions(block, patchpoint);

    const Vector<ArgumentLocation, 1>& constrainedResultLocations = wasmCalleeInfo.results;
    if (returnType.isTuple() || returnType == Int64) {
        Vector<B3::ValueRep, 1> resultConstraints;
        Vector<B3::ValueRep, 1> resultConstraintsHigh;

        for (auto valueLocation : constrainedResultLocations) {
            if (valueLocation.location.isGPR())
                resultConstraints.append(B3::ValueRep(valueLocation.location.jsr().payloadGPR()));
            else
                resultConstraints.append(B3::ValueRep(valueLocation.location));
        }

        for (size_t index = 0; index < functionSignature.returnCount(); ++index) {
            auto valueLocation = constrainedResultLocations[index];

            if (toB3Type(functionSignature.returnType(index)) == Int64) {
                ASSERT(returnType == Int64 || m_proc.tupleForType(returnType)[index] == Int32);
                if (valueLocation.location.isGPR())
                    resultConstraintsHigh.append(B3::ValueRep(valueLocation.location.jsr().tagGPR()));
                else if (valueLocation.location.isStack())
                    resultConstraintsHigh.append(B3::ValueRep::stack(checkedSum<intptr_t>(valueLocation.location.offsetFromFP(), static_cast<intptr_t>(bytesForWidth(Width32))).value()));
                else {
                    ASSERT(valueLocation.location.isStackArgument());
                    resultConstraintsHigh.append(B3::ValueRep::stackArgument(checkedSum<intptr_t>(valueLocation.location.offsetFromSP(), static_cast<intptr_t>(bytesForWidth(Width32))).value()));
                }
            }
        }

        ASSERT(resultConstraints.size() + resultConstraintsHigh.size() == (returnType == Int64 ? 2 : m_proc.tupleForType(returnType).size()));
        for (auto c : resultConstraintsHigh)
            resultConstraints.append(c);
        patchpoint->resultConstraints = WTFMove(resultConstraints);
    } else if (returnType != B3::Void) {
        Vector<B3::ValueRep, 1> resultConstraints;
        for (auto valueLocation : constrainedResultLocations) {
            if (valueLocation.location.isGPR())
                resultConstraints.append(B3::ValueRep(valueLocation.location.jsr().payloadGPR()));
            else
                resultConstraints.append(B3::ValueRep(valueLocation.location));
        }
        patchpoint->resultConstraints = WTFMove(resultConstraints);
    }
    block->append(patchpoint);
    return { patchpoint, WTFMove(exceptionHandle), nullptr };
}

// See createTailCallPatchpoint for the setup before this.
static inline void prepareForTailCallImpl(unsigned functionIndex, CCallHelpers& jit, const B3::StackmapGenerationParams& params, const TypeDefinition& signature, const CallInformation& wasmCallerInfoAsCallee, const CallInformation& wasmCalleeInfoAsCallee, unsigned firstPatchArg, unsigned lastPatchArg, int32_t newFPOffsetFromFP)
{
    auto& functionSignature = *signature.as<FunctionSignature>();
    const Checked<int32_t> offsetOfFirstSlotFromFP = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCallerInfoAsCallee.headerAndArgumentStackSizeInBytes);
    JIT_COMMENT(jit, "Set up tail call, new FP offset from FP: ", newFPOffsetFromFP);

    const unsigned frameSize = params.code().frameSize();
    ASSERT(WTF::roundUpToMultipleOf<stackAlignmentBytes()>(frameSize + sizeof(CallerFrameAndPC)) == frameSize + sizeof(CallerFrameAndPC));

    auto fpOffsetToSPOffset = [frameSize](int32_t offset) {
        return checkedSum<int>(safeCast<int>(frameSize), offset).value();
    };

    auto newReturnPCOffset = fpOffsetToSPOffset(checkedSum<intptr_t>(CallFrame::returnPCOffset(), newFPOffsetFromFP).value());

    // We requested some extra stack space below via requestCallArgAreaSize
    // ... FP [initial safe area][caller stack space ] [callArgSpace                    ] SP ...
    // becomes
    // ... FP [safe area growing ->    ] [danger           ] [ scratch                  ] SP ...
    // This scratch space sits at the very bottom of the stack, near sp.
    // AirLowerStackArgs takes care of adding callArgSpace to our total caller frame size.
    // BUT, even though we have this extra space, the new frame might be bigger, so we can't
    // use the new frame as scratch. The new return pc represents the lowest offset from SP we can use.
    int spillPointer = 0;
    const int scratchAreaUpperBound = std::min(
        safeCast<int>(WTF::roundUpToMultipleOf<stackAlignmentBytes()>(static_cast<int>(wasmCalleeInfoAsCallee.headerAndArgumentStackSizeInBytes))),
        newReturnPCOffset);
    auto allocateSpill = [&] (Width width) -> int {
        int offset = spillPointer;
        spillPointer += bytesForWidth(width);
        ASSERT(spillPointer <= scratchAreaUpperBound);
        ASSERT(offset < scratchAreaUpperBound);
        return offset;
    };

    RegisterAtOffsetList calleeSaves = params.code().calleeSaveRegisterAtOffsetList();

    // Be careful not to clobber this below.
    // We also need to make sure that we preserve this if it is used by the patchpoint body.
    AllowMacroScratchRegisterUsage allowScratch(jit);
    auto tmp = jit.scratchRegister();
    bool tmpNeedsSaving = false;
    int tmpSpillOffsetRelativeToOriginalSP = 0;

    // If we pass a stack location to the patchpoint in arugmentCountIncludingThis, preserve it here.
    bool stackPatchArg[tailCallPatchpointScratchCount] = { false, false };
    int stackPatchArgSpill[tailCallPatchpointScratchCount] = { 0, 0 };

    // Nothing before saving tmp can use the scratch register since it might clobber an input.
    {
        DisallowMacroScratchRegisterUsage disallowScratch(jit);

        // Set up a valid frame so that we can clobber this one.
        jit.emitRestore(calleeSaves);

        for (unsigned i = 0; i < params.size(); ++i) {
            auto arg = params[i];
            if (arg.isGPR()) {
                ASSERT(!calleeSaves.find(arg.gpr()));
                if (arg.gpr() == tmp)
                    tmpNeedsSaving = true;
                continue;
            }
            if (arg.isFPR()) {
                ASSERT(!calleeSaves.find(arg.fpr()));
                continue;
            }
        }

        for (unsigned i = lastPatchArg; i < params.size(); ++i) {
            auto arg = params[i];
            if (arg.isStack()) {
                unsigned scratch = -1;
                for (unsigned i = 0; i < tailCallPatchpointScratchCount; ++i) {
                    if (!stackPatchArg[i]) {
                        scratch = i;
                        break;
                    }
                }
                ASSERT(scratch >= 0 && !stackPatchArg[scratch]);
                stackPatchArg[scratch] = true;
                if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
                    jit.probeDebug([arg] (Probe::Context& context) {
                        dataLogLn("patch arg spill: ", RawHex(context.gpr<uintptr_t*>(MacroAssembler::framePointerRegister)[arg.offsetFromFP() / sizeof(uintptr_t)]));
                    });
                }
                // A convinent and save place to stash it.
                jit.transferPtr(CCallHelpers::Address(MacroAssembler::framePointerRegister, arg.offsetFromFP()),
                    CCallHelpers::Address(MacroAssembler::framePointerRegister, tailCallPatchpointScratchOffsets[scratch]));
            } else
                ASSERT(arg.isGPR() || arg.isFPR());
        }

        ASSERT(!calleeSaves.find(tmp) || !tmpNeedsSaving);
    }

#if ASSERT_ENABLED
    // Let's make sure we never rely on these slots, since they may be used for scratch.
    for (unsigned i = 0; i < tailCallPatchpointScratchCount; ++i) {
        if (!stackPatchArg[i]) {
            jit.storePtr(MacroAssembler::TrustedImmPtr(0xBEEFAAAA),
                CCallHelpers::Address(MacroAssembler::framePointerRegister, tailCallPatchpointScratchOffsets[i]));
        }
    }
#endif

    JIT_COMMENT(jit, "Let's use the caller's frame, so that we always have a valid frame.");
    if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
        jit.probeDebugSIMD([frameSize, fpOffsetToSPOffset, newFPOffsetFromFP, signature = Ref<const TypeDefinition>(signature), wasmCalleeInfoAsCallee, firstPatchArg, lastPatchArg, params, functionIndex] (Probe::Context& context) {
            auto& functionSignature = *signature->as<FunctionSignature>();
            auto sp = context.gpr<uintptr_t>(MacroAssembler::stackPointerRegister);
            auto fp = context.gpr<uintptr_t>(GPRInfo::callFrameRegister);
            dataLogLn("Before tail call in function ", functionIndex, " before changing anything: FP: ", RawHex(fp), " SP: ", RawHex(sp));
            dataLogLn("New FP will be at ", RawHex(sp + fpOffsetToSPOffset(newFPOffsetFromFP)));
            CallFrame* fpp = context.gpr<CallFrame*>(GPRInfo::callFrameRegister);
            dataLogLn("callee original: ", RawPointer(fpp->callee().rawPtr()));
            auto& wasmCallee = context.gpr<uintptr_t*>(GPRInfo::callFrameRegister)[CallFrameSlot::callee * 1];
            dataLogLn("callee original: ", RawHex(wasmCallee), " at ", RawPointer(&wasmCallee));
            dataLogLn("retPC original: ", RawPointer(fpp->rawReturnPC()));
            auto& retPC = context.gpr<uintptr_t*>(GPRInfo::callFrameRegister)[CallFrame::returnPCOffset() / sizeof(uint64_t)];
            dataLogLn("retPC original: ", RawHex(retPC), " at ", RawPointer(&retPC));
            dataLogLn("callerFrame original: ", RawPointer(fpp->callerFrame()));
            ASSERT_UNUSED(frameSize, sp + frameSize == fp);

            auto fpl = context.gpr<uint64_t*>(GPRInfo::callFrameRegister);
            auto fpi = context.gpr<uint32_t*>(GPRInfo::callFrameRegister);

            unsigned currentPatchArg = firstPatchArg;
            auto dumpSrc = [&context, fpl, fpi, fpp] (ValueRep src, Width width) {
                dataLog(src, " = ");
                if (src.isGPR())
                    dataLog(context.gpr(src.gpr()), " / ", (int) context.gpr(src.gpr()));
                else if (src.isFPR() && width <= Width64)
                    dataLog(context.fpr(src.fpr(), SavedFPWidth::DontSaveVectors));
                else if (src.isFPR())
                    RELEASE_ASSERT_NOT_REACHED();
                else if (src.isConstant())
                    dataLog(src.value(), " / ", src.doubleValue());
                else
                    dataLog(fpl[src.offsetFromFP() / sizeof(*fpl)], " / ", fpi[src.offsetFromFP() / sizeof(*fpi)], " / ", std::bit_cast<double>(fpl[src.offsetFromFP() / sizeof(*fpl)]), " at ", RawPointer(&fpp[src.offsetFromFP() / sizeof(*fpp)]));
            };

            for (unsigned i = 0; i < functionSignature.argumentCount(); ++i) {
                auto width = functionSignature.argumentType(i).width();
                ASSERT(wasmCalleeInfoAsCallee.params[i].width >= width);
                dataLog("Arg source ", i, " located at ");
                ASSERT_UNUSED(lastPatchArg, currentPatchArg < lastPatchArg);
                dumpSrc(params[currentPatchArg++], width);
                if (is32Bit() && functionSignature.argumentType(i).isI64()) {
                    dataLog(" (Upper bits: ");
                    ASSERT_UNUSED(lastPatchArg, currentPatchArg < lastPatchArg);
                    dumpSrc(params[currentPatchArg++], width);
                    dataLog(")");
                }

                dataLogLn(" ->(final) ", wasmCalleeInfoAsCallee.params[i].location);
            }
        });
    }
    jit.loadPtr(CCallHelpers::Address(MacroAssembler::framePointerRegister, CallFrame::callerFrameOffset()), MacroAssembler::framePointerRegister);
    if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
        jit.probeDebugSIMD([] (Probe::Context& context) {
            auto sp = context.gpr<uintptr_t>(MacroAssembler::stackPointerRegister);
            auto fp = context.gpr<uintptr_t>(GPRInfo::callFrameRegister);
            dataLogLn("In the new expanded frame, including F's caller: FP: ", RawHex(fp), " SP: ", RawHex(sp));
        });
    }

    JIT_COMMENT(jit, "Copy over args if needed into their final position, clobbering everything.");
    // This code has a bunch of overlap with CallFrameShuffler and Shuffle in Air/BBQ

    auto doMove = [&jit, tmp] (int srcOffset, int dstOffset, Width width) {
        JIT_COMMENT(jit, "Do move ", srcOffset, " -> ", dstOffset);
        auto src = CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset);
        auto dst = CCallHelpers::Address(MacroAssembler::stackPointerRegister, dstOffset);
        if (width <= Width32)
            jit.transfer32(src, dst);
        else if (width <= Width64)
            jit.transfer64(src, dst);
        else {
            jit.transfer64(src, dst);
            jit.transfer64(src.withOffset(bytesForWidth(Width::Width64)), dst.withOffset(bytesForWidth(Width::Width64)));
        }
        if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
            jit.loadPtr(dst, tmp);
            jit.probeDebugSIMD([tmp, srcOffset, dstOffset, width] (Probe::Context& context) {
                auto val = context.gpr<uintptr_t>(tmp);
                auto sp = context.gpr<uintptr_t>(MacroAssembler::stackPointerRegister);
                dataLogLn("Move value ", val, " / ", RawHex(val), " at ", RawHex(sp + srcOffset), " -> ", RawHex(sp + dstOffset), " width ", width);
            });
        }
    };

    // This should grow down towards SP (towards 0) as we move stuff out of the way.
    int safeAreaLowerBound = fpOffsetToSPOffset(CallFrameSlot::firstArgument * sizeof(Register));
    const int stackUpperBound = fpOffsetToSPOffset(offsetOfFirstSlotFromFP); // ArgN in the stack diagram
    ASSERT(safeAreaLowerBound > 0);
    ASSERT(safeAreaLowerBound < stackUpperBound);

    JIT_COMMENT(jit, "SP[", safeAreaLowerBound, "] to SP[", stackUpperBound, "] form the safe portion of the stack to clobber; Scratches go from SP[0] to SP[", scratchAreaUpperBound, "].");

    if (tmpNeedsSaving) {
        tmpSpillOffsetRelativeToOriginalSP = allocateSpill(WidthPtr);
        jit.storePtr(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister, tmpSpillOffsetRelativeToOriginalSP));
    }

    for (unsigned i = 0; i < tailCallPatchpointScratchCount; ++i) {
        if (stackPatchArg[i]) {
            stackPatchArgSpill[i] = allocateSpill(WidthPtr);
            jit.transferPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister, fpOffsetToSPOffset(tailCallPatchpointScratchOffsets[i])), CCallHelpers::Address(MacroAssembler::stackPointerRegister, stackPatchArgSpill[i]));
        }
    }

#if ASSERT_ENABLED
    // Clobber all safe values to make debugging easier.
    for (int i = safeAreaLowerBound; i < stackUpperBound; i += sizeof(Register)) {
        jit.storePtr(MacroAssembler::TrustedImmPtr(0xBEEF),
            CCallHelpers::Address(MacroAssembler::stackPointerRegister, i));
    }
#endif

    // srcOffset, dstOffset
    Vector<std::tuple<int, int, Width>> argsToMove;
    Vector<std::tuple<int, int, Width>> spillsToMove;
    argsToMove.reserveInitialCapacity(wasmCalleeInfoAsCallee.params.size() + 1);

    // We will complete those moves who's source is closest to the danger frontier first.
    // That will move the danger frontier.
    unsigned currentPatchArg = firstPatchArg;
    for (unsigned i = 0; i < functionSignature.argumentCount(); ++i) {
        auto dst = wasmCalleeInfoAsCallee.params[i];
        auto dstType = functionSignature.argumentType(i);
        ASSERT(dst.width <= Width::Width64);
        ASSERT(dst.width >= dstType.width());
        if (dst.location.isGPR()) {
            ASSERT(!calleeSaves.find(dst.location.jsr().payloadGPR()));
            currentPatchArg += dstType.width() == Width64 ? 2 : 1;
            continue;
        }
        if (dst.location.isFPR()) {
            ASSERT(!calleeSaves.find(dst.location.fpr()));
            currentPatchArg++;
            continue;
        }

        auto saveSrc = [tmp, tmpNeedsSaving, tmpSpillOffsetRelativeToOriginalSP, dstType, &allocateSpill, &jit, &fpOffsetToSPOffset](ValueRep src) -> std::tuple<int, Width> {
            int srcOffset = 0;
            if (tmpNeedsSaving && src.isGPR() && src.gpr() == tmp) {
                // Before tmp may have been clobbered, it was spilled to tmpSpillOffsetRelativeToOriginalSP.
                srcOffset = tmpSpillOffsetRelativeToOriginalSP;
            } else if (src.isGPR()) {
                srcOffset = allocateSpill(WidthPtr);
                jit.storePtr(src.gpr(), CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset));
                return { srcOffset, WidthPtr };
            }
            if (src.isFPR()) {
                srcOffset = allocateSpill(dstType.width());
                if (dstType.width() <= Width::Width64)
                    jit.storeDouble(src.fpr(), CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset));
                else
                    jit.storeVector(src.fpr(), CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset));
            } else if (src.isConstant()) {
                if (toB3Type(dstType).kind() == Float) {
                    srcOffset = allocateSpill(Width32);
                    jit.move(MacroAssembler::TrustedImm32(src.value()), tmp);
                    jit.store32(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset));
                } else if (toB3Type(dstType).kind() == Double) {
                    srcOffset = allocateSpill(Width64);
                    jit.move(MacroAssembler::TrustedImmPtr(src.value()), tmp);
                    jit.storePtr(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset));
                    jit.move(MacroAssembler::TrustedImmPtr(src.value() >> 32), tmp);
                    jit.storePtr(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset + sizeof(int)));
                } else {
                    srcOffset = allocateSpill(WidthPtr);
                    jit.move(MacroAssembler::TrustedImmPtr(src.value()), tmp);
                    jit.storePtr(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset));
                    return { srcOffset, WidthPtr };
                }
            } else {
                ASSERT(src.isStack());
                srcOffset = fpOffsetToSPOffset(src.offsetFromFP());

                if (is32Bit() && toB3Type(dstType).kind() == Int64)
                    return { srcOffset, WidthPtr };
            }

            return { srcOffset, dstType.width() };
        };

        ASSERT_UNUSED(lastPatchArg, currentPatchArg < lastPatchArg);
        auto [srcOffset, srcWidth] = saveSrc(params[currentPatchArg++]);
        intptr_t dstOffset = fpOffsetToSPOffset(checkedSum<int32_t>(dst.location.offsetFromFP(), newFPOffsetFromFP).value());
        ASSERT(srcOffset >= 0);
        ASSERT(dstOffset >= 0);
        JIT_COMMENT(jit, "Arg ", i, " has srcOffset ", srcOffset, " dstOffset ", dstOffset);
        argsToMove.append({ srcOffset, dstOffset, srcWidth });

        if (is32Bit() && srcWidth < dstType.width()) {
            ASSERT_UNUSED(lastPatchArg, currentPatchArg < lastPatchArg);
            auto [srcOffsetUpperBits, srcWidthUpperBits] = saveSrc(params[currentPatchArg++]);
            ASSERT(srcWidthUpperBits == WidthPtr);
            argsToMove.append({ srcOffsetUpperBits, dstOffset + sizeof(int), srcWidthUpperBits });
        }
    }

    argsToMove.append({
        fpOffsetToSPOffset(CallFrame::returnPCOffset()),
        newReturnPCOffset,
        WidthPtr
    });
    JIT_COMMENT(jit, "ReturnPC has srcOffset ", fpOffsetToSPOffset(CallFrame::returnPCOffset()), " dstOffset ", newReturnPCOffset);

    for (unsigned i = 0; i < tailCallPatchpointScratchCount; ++i) {
        if (stackPatchArg[i])
            argsToMove.append({ stackPatchArgSpill[i], fpOffsetToSPOffset(tailCallPatchpointScratchOffsets[i] + newFPOffsetFromFP), WidthPtr });
    }

    std::ranges::sort(argsToMove, [](const auto& left, const auto& right) {
        return std::get<0>(left) > std::get<0>(right);
    });

    for (unsigned i = 0; i < argsToMove.size(); ++i) {
        auto [srcOffset, dstOffset, width] = argsToMove[i];
        // The first arg is the highest-offset arg, and we expect that moving it should
        // make progress on moving the safe area down.
        ASSERT_UNUSED(safeAreaLowerBound, srcOffset <= safeAreaLowerBound);

        safeAreaLowerBound = srcOffset;
        ASSERT(srcOffset < stackUpperBound);
        ASSERT(dstOffset < stackUpperBound);
        ASSERT(dstOffset >= scratchAreaUpperBound);
        ASSERT(srcOffset >= 0);
        ASSERT(dstOffset >= 0);

        JIT_COMMENT(jit, "SP[", safeAreaLowerBound, "] to SP[", stackUpperBound, "] form the safe portion of the stack to clobber.");

        if (dstOffset >= safeAreaLowerBound)
            doMove(srcOffset, dstOffset, width);
        else {
            JIT_COMMENT(jit, "Must spill.");
            auto scratch = allocateSpill(width);
            doMove(srcOffset, scratch, width);
            spillsToMove.append({ scratch, dstOffset, width });
        }
    }

    JIT_COMMENT(jit, "Move spills");

    for (unsigned i = 0; i < spillsToMove.size(); ++i) {
        auto [srcOffset, dstOffset, width] = spillsToMove[i];
        ASSERT(srcOffset < stackUpperBound);
        ASSERT(dstOffset < stackUpperBound);
        ASSERT(dstOffset >= scratchAreaUpperBound);
        ASSERT(srcOffset >= 0);
        ASSERT(dstOffset >= 0);

        doMove(srcOffset, dstOffset, width);
    }

    JIT_COMMENT(jit, "Now we can restore / resign lr.");

    // Pop our locals, leaving only the new frame behind as though our original caller had called the callee.
    // Also pop callee.
    auto newFPOffsetFromSP = fpOffsetToSPOffset(newFPOffsetFromFP);
    ASSERT(newFPOffsetFromSP > 0);
    ASSERT(WTF::roundUpToMultipleOf<stackAlignmentBytes()>(std::abs(newFPOffsetFromSP) + sizeof(CallerFrameAndPC)) == static_cast<size_t>(std::abs(newFPOffsetFromSP) + sizeof(CallerFrameAndPC)));

    auto newSPAtPrologueOffsetFromSP = newFPOffsetFromSP + prologueStackPointerDelta();

    // The return PC should be at the top of the new stack.
    // On ARM64E, we load it before changing SP to avoid needing an extra temp register.

#if CPU(ARM) || CPU(ARM64) || CPU(RISCV64)
    JIT_COMMENT(jit, "Load the return pointer from its saved location.");
    jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister, newFPOffsetFromSP + OBJECT_OFFSETOF(CallerFrameAndPC, returnPC)), tmp);
    jit.move(tmp, MacroAssembler::linkRegister);
    if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
        jit.probeDebugSIMD([] (Probe::Context& context) {
            dataLogLn("tagged return pc: ", RawHex(context.gpr<uintptr_t>(MacroAssembler::linkRegister)));
        });
    }
#if CPU(ARM64E)
    JIT_COMMENT(jit, "The return pointer was signed with the stack height before we pushed lr, fp, see emitFunctionPrologue. newFPOffsetFromSP: ", newFPOffsetFromSP, " newFPOffsetFromFP ", newFPOffsetFromFP);
    jit.addPtr(MacroAssembler::TrustedImm32(params.code().frameSize() + sizeof(CallerFrameAndPC)), MacroAssembler::stackPointerRegister, tmp);
    jit.untagPtr(tmp, MacroAssembler::linkRegister);
    if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
        jit.probeDebugSIMD([] (Probe::Context& context) {
            dataLogLn("untagged return pc: ", RawHex(context.gpr<uintptr_t>(MacroAssembler::linkRegister)));
        });
    }
    jit.validateUntaggedPtr(MacroAssembler::linkRegister);
#endif
#endif

    if (tmpNeedsSaving)
        jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister, tmpSpillOffsetRelativeToOriginalSP), tmp);

    // Nothing after restoring tmp can use the scratch register since it might clobber an input.
    {
        DisallowMacroScratchRegisterUsage disallowScratch(jit);

        jit.addPtr(MacroAssembler::TrustedImm32(newSPAtPrologueOffsetFromSP), MacroAssembler::stackPointerRegister);

#if CPU(X86_64)
        if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
            jit.probeDebugSIMD([] (Probe::Context& context) {
                dataLogLn("return pc on the top of the stack: ", RawHex(*context.gpr<uintptr_t*>(MacroAssembler::stackPointerRegister)), " at ", RawHex(context.gpr<uintptr_t>(MacroAssembler::stackPointerRegister)));
            });
        }
#endif

        JIT_COMMENT(jit, "OK, now we can jump.");
        if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
            jit.probeDebugSIMD([wasmCalleeInfoAsCallee] (Probe::Context& context) {
                dataLogLn("Can now jump: FP: ", RawHex(context.gpr<uintptr_t>(GPRInfo::callFrameRegister)), " SP: ", RawHex(context.gpr<uintptr_t>(MacroAssembler::stackPointerRegister)));
                auto* newFP = context.gpr<uintptr_t*>(MacroAssembler::stackPointerRegister) - prologueStackPointerDelta() / sizeof(uintptr_t);
                dataLogLn("New (callee) FP at prologue will be at ", RawPointer(newFP));
                auto fpl = std::bit_cast<uint64_t*>(newFP);
                auto fpi = std::bit_cast<uint32_t*>(newFP);

                for (unsigned i = 0; i < wasmCalleeInfoAsCallee.params.size(); ++i) {
                    auto arg = wasmCalleeInfoAsCallee.params[i];
                    auto src = arg.location;
                    dataLog("Arg ", i, " located at ", arg.location, " = ");
                    if (arg.location.isGPR()) {
                        dataLog(context.gpr(arg.location.jsr().payloadGPR()), " / ", (int) context.gpr(arg.location.jsr().payloadGPR()));
                        if (src.jsr().tagGPR())
                            dataLog(" Upper bits: ", context.gpr(src.jsr().tagGPR()), " / ", (int) context.gpr(src.jsr().tagGPR()));
                    } else if (arg.location.isFPR() && arg.width <= Width::Width64)
                        dataLog(context.fpr(arg.location.fpr(), SavedFPWidth::DontSaveVectors));
                    else if (arg.location.isFPR())
                        RELEASE_ASSERT_NOT_REACHED();
                    else
                        dataLog(fpl[src.offsetFromFP() / sizeof(*fpl)], " / ", fpi[src.offsetFromFP() / sizeof(*fpi)],  " / ", RawHex(fpi[src.offsetFromFP() / sizeof(*fpi)]), " / ", std::bit_cast<double>(fpl[src.offsetFromFP() / sizeof(*fpl)]), " at ", RawPointer(&fpi[src.offsetFromFP() / sizeof(*fpi)]));
                    dataLogLn();
                }
            });
        }

#if ASSERT_ENABLED
        // Everything in the old stack might be overwritten anyway. Clobber for easier debugging.
        if (tmpNeedsSaving)
            jit.pushPair(tmp, tmp);
        jit.move(MacroAssembler::TrustedImm32(0xBFFF), tmp);
        constexpr int stackSlotsToClobber = 3 * stackAlignmentBytes();
        constexpr int stackBytesToClobber = stackSlotsToClobber * registerSize();
        static_assert(!(stackBytesToClobber & (stackAlignmentBytes() - 1)), "Size in bytes to clobber on stack is aligned");
        for (int i = 0; i < stackSlotsToClobber / 2; ++i)
            jit.pushPair(tmp, tmp);
        jit.addPtr(MacroAssembler::TrustedImm32(stackBytesToClobber), MacroAssembler::stackPointerRegister);
        if (tmpNeedsSaving)
            jit.popPair(tmp, tmp);
#endif
    }
}

// See also: https://leaningtech.com/fantastic-tail-calls-and-how-to-implement-them/, a blog post about contributing this feature.
auto OMGIRGenerator::createTailCallPatchpoint(BasicBlock* block, const TypeDefinition& signature, const CallInformation& wasmCallerInfoAsCallee, const CallInformation& wasmCalleeInfoAsCallee, const ArgumentList& tmpArgSourceLocations, Vector<B3::ConstrainedValue> patchArgs) -> CallPatchpointData
{
    m_makesTailCalls = true;
    // Our args are placed in argument registers or locals.
    // We must:
    // - Restore callee saves
    // - Restore and re-sign lr
    // - Restore our caller's FP so that the stack area we write to is always valid
    // - Move stack args from our stack to their final resting spots. Note that they might overlap.
    // Layout of stack right now, and after this patchpoint.
    //
    //
    //    |          Original Caller   |                                                                      |          ......            |
    //    +----------------------------+ <--                                                                  +----------------------------+ <--
    //    |           F.argN           |    |                                    +-------------------->       |           G.argM           |    |                            Safe to clobber
    //    +----------------------------+    | lower address                      |                            +----------------------------+    | lower address
    //    |           F.arg1           |    v                                    |                            |           arg1             |    v
    //    +----------------------------+                                         |                            +----------------------------+
    //    |           F.arg0           |                                         |                            |           arg0             |                                   .......... < Danger froniter, grows down as args get moved out of the way
    //    +----------------------------+                                         |                            +----------------------------+
    //    |           F.this           |                                         |                            |           this'            |                                Dangerous to clobber
    //    +----------------------------+                                         |                            +----------------------------+
    //    | argumentCountIncludingThis |                                         |                            |          A.C.I.T.'         |
    //    +----------------------------+                                         |                            +----------------------------+
    //    |          F.callee          |                                         |                            |        G.callee            |
    //    +----------------------------+                                         |                            +----------------------------+
    //    |        F.codeBlock         |                               (shuffleStackArgs...)                  |        G.codeBlock         |
    //    +----------------------------+                                         |                     (arm) >+----------------------------+
    //    | return-address after F     |                                         |                            |   return-address after F   |
    //    +----------------------------+                                         | SP at G prologue (intel) ->+----------------------------+
    //    |          F.caller.FP       |                                         |                            |          F.caller.FP       |
    //    +----------------------------+  <- F.FP                                |    G.FP after G prologue-> +----------------------------+
    //    |          callee saves      |                                         |                            |          callee saves      |
    //    +----------------------------+   <----+   argM to G  ------------------+                            +----------------------------+
    //    |          F.local0          |        |   ....                                                      |          G.local0          |
    //    +----------------------------+        |   arg0 to G                                                 +----------------------------+
    //    |          F.local1          |        |                                                             |          G.local1          |
    //    +----------------------------+        |                                                             +----------------------------+
    //    |          F.localN          |        |                                                             |          G.localM          |
    //    +----------------------------|        |                                                             +----------------------------+
    //    |          ......            |        |                                                             |          ......            |
    //    +----------------------------|  <- SP |                                       SP after G prologue-> +----------------------------+
    //                                          |
    //    Note that F.FP is not the same as G.FP because the number of args may differ.
    // We must not clobber any local because source args may be located anywhere.
    // The final resting place of G.argM (F.argN) up to the return address after F is fair game to clobber; we do not permit StackArgument value reps.

    // First slot here is the last argument to F, a.k.a the first stack slot that belongs to F.
    const Checked<int32_t> offsetOfFirstSlotFromFP = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCallerInfoAsCallee.headerAndArgumentStackSizeInBytes);
    ASSERT(offsetOfFirstSlotFromFP > 0);
    const Checked<int32_t> offsetOfNewFPFromFirstSlot = checkedProduct<int32_t>(-1, WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCalleeInfoAsCallee.headerAndArgumentStackSizeInBytes));
    const Checked<int32_t> newFPOffsetFromFP = offsetOfFirstSlotFromFP + offsetOfNewFPFromFirstSlot;
    m_tailCallStackOffsetFromFP = std::min(m_tailCallStackOffsetFromFP, newFPOffsetFromFP);

    RegisterSet scratchRegisters = RegisterSetBuilder::macroClobberedGPRs();
    RegisterSet forbiddenArgumentRegisters = RegisterSetBuilder::calleeSaveRegisters().merge(scratchRegisters);

    ASSERT(wasmCalleeInfoAsCallee.params.size() == tmpArgSourceLocations.size());
#if ASSERT_ENABLED
    for (unsigned i = 0; i < patchArgs.size(); ++i) {
        // We will clobber our stack, so we shouldn't be reading any special extra patch args from it after this point.
        // If we do need a stack arg, we can only save one.
        ASSERT(patchArgs[i].rep().isReg()
            || patchArgs[i].rep().isConstant()
            || (patchArgs[i].rep().isStackArgument() && patchArgs.size() == 1));
        ASSERT(!patchArgs[i].rep().isReg() || !scratchRegisters.contains(patchArgs[i].rep().reg(), IgnoreVectors));
    }
#endif

    ASSERT(wasmCalleeInfoAsCallee.params.size() == tmpArgSourceLocations.size());
    unsigned firstPatchArg = patchArgs.size();

    auto constrainedArgPatchArgs = createCallConstrainedArgs(block, wasmCalleeInfoAsCallee, tmpArgSourceLocations);

    for (unsigned i = 0; i < constrainedArgPatchArgs.size(); ++i) {
        auto src = constrainedArgPatchArgs[i].value();
        auto dst = constrainedArgPatchArgs[i].rep();
        ASSERT(dst.isStack() || dst.isFPR() || dst.isGPR());
        if (!dst.isStack()) {
            // We will restore callee saves before jumping to the callee.
            // The calling convention should guarantee this anyway, but let's document it just in case.
            ASSERT_UNUSED(forbiddenArgumentRegisters, !forbiddenArgumentRegisters.contains(dst.isGPR() ? Reg(dst.gpr()) : Reg(dst.fpr()), IgnoreVectors));
            patchArgs.append(constrainedArgPatchArgs[i]);
            continue;
        }
        patchArgs.append(ConstrainedValue(src, ValueRep::LateColdAny));
    }
    unsigned lastPatchArg = patchArgs.size();

    PatchpointValue* patchpoint = m_proc.add<PatchpointValue>(B3::Void, origin());
    patchpoint->effects.terminal = true;
    patchpoint->effects.readsPinned = true;
    patchpoint->effects.writesPinned = true;

    RegisterSetBuilder clobbers;
    clobbers.merge(RegisterSetBuilder::calleeSaveRegisters());
    clobbers.exclude(RegisterSetBuilder::stackRegisters());
    patchpoint->clobberEarly(WTFMove(clobbers));
    patchpoint->clobberLate(RegisterSetBuilder::macroClobberedGPRs());
    patchpoint->appendVector(WTFMove(patchArgs));
    // See prepareForTailCallImpl for the heart of this patchpoint.
    block->append(patchpoint);

    firstPatchArg += m_proc.resultCount(patchpoint->type());
    lastPatchArg += m_proc.resultCount(patchpoint->type());

    auto prepareForCall = createSharedTask<B3::StackmapGeneratorFunction>([signature = Ref<const TypeDefinition>(signature), wasmCalleeInfoAsCallee, wasmCallerInfoAsCallee, newFPOffsetFromFP, firstPatchArg, lastPatchArg, functionIndex = m_functionIndex](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        ASSERT(newFPOffsetFromFP >= 0 || params.code().frameSize() >= static_cast<uint32_t>(-newFPOffsetFromFP));
        prepareForTailCallImpl(functionIndex, jit, params, signature, wasmCallerInfoAsCallee, wasmCalleeInfoAsCallee, firstPatchArg, lastPatchArg, newFPOffsetFromFP);
    });

    return { patchpoint, nullptr, WTFMove(prepareForCall) };
}

bool OMGIRGenerator::canInline(FunctionSpaceIndex, unsigned) const
{
    ASSERT(!m_inlinedBytes || !m_inlineParent);
    return false;
}

auto OMGIRGenerator::emitInlineDirectCall(FunctionCodeIndex calleeFunctionIndex, const TypeDefinition& calleeSignature, ArgumentList& args, ValueResults& results) -> PartialResult
{
    Vector<Value*> getArgs;

    for (auto& arg : args)
        getArgs.append(m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, origin(), arg.value()));

    BasicBlock* continuation = m_proc.addBlock();
    // Not all inine frames need to save state, but we still need to make sure that there is at least
    // one unique CallSiteIndex per inline frame for stack traces to work.
    advanceCallSiteIndex();
    auto firstInlineCallSiteIndex = advanceCallSiteIndex();

    const FunctionData& function = m_info.functions[calleeFunctionIndex];
    Ref<IPIntCallee> profiledCallee = m_calleeGroup.ipintCalleeFromFunctionIndexSpace(m_calleeGroup.toSpaceIndex(calleeFunctionIndex));
    m_protectedInlineeGenerators.append(makeUnique<OMGIRGenerator>(m_heaps, m_context, *this, *m_inlineRoot, m_module, m_calleeGroup, calleeFunctionIndex, profiledCallee.get(), continuation, WTFMove(getArgs)));
    auto& irGenerator = *m_protectedInlineeGenerators.last();
    m_protectedInlineeParsers.append(makeUnique<FunctionParser<OMGIRGenerator>>(irGenerator, function.data, calleeSignature, m_info));
    auto& parser = *m_protectedInlineeParsers.last();
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    irGenerator.insertConstants();
    for (unsigned i = 1; i < irGenerator.m_rootBlocks.size(); ++i) {
        auto block = irGenerator.m_rootBlocks[i];
        dataLogLnIf(WasmOMGIRGeneratorInternal::verboseInlining, "Block (", i, ")", block.block, " is an inline catch handler");
        m_rootBlocks.append({ block.block, block.usesSIMD || irGenerator.usesSIMD() });
    }
    m_exceptionHandlers.appendVector(WTFMove(irGenerator.m_exceptionHandlers));
    if (irGenerator.m_exceptionHandlers.size())
        m_hasExceptionHandlers = true;
    RELEASE_ASSERT(!irGenerator.m_callSiteIndex);

    irGenerator.m_topLevelBlock->appendNewControlValue(m_proc, B3::Jump, origin(), FrequentedBlock(irGenerator.m_rootBlocks[0].block));
    m_makesCalls |= irGenerator.m_makesCalls;
    ASSERT(&irGenerator.m_proc == &m_proc);

    dataLogLnIf(WasmOMGIRGeneratorInternal::verboseInlining, "Block ", *m_currentBlock, " is going to do an inline call to block ", *irGenerator.m_topLevelBlock, " then continue at ", *continuation);
    m_currentBlock->appendNewControlValue(m_proc, B3::Jump, origin(), FrequentedBlock(irGenerator.m_topLevelBlock));
    m_currentBlock = continuation;

    for (unsigned i = 0; i < calleeSignature.as<FunctionSignature>()->returnCount(); ++i)
        results.append(m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, origin(), irGenerator.m_inlinedResults[i]));

    auto lastInlineCallSiteIndex = advanceCallSiteIndex();
    advanceCallSiteIndex();
    m_callee->addCodeOrigin(firstInlineCallSiteIndex, lastInlineCallSiteIndex, m_info, calleeFunctionIndex + m_numImportFunctions);

    dataLogLnIf(WasmOMGIRGeneratorInternal::verboseInlining, "Inlining CallSiteIndex range: ", firstInlineCallSiteIndex, " -> ", lastInlineCallSiteIndex, " [", m_inlineDepth, "]");

    return { };
}

auto OMGIRGenerator::addCall(unsigned callProfileIndex, FunctionSpaceIndex functionIndexSpace, const TypeDefinition& signature, ArgumentList& args, ResultList& results, CallType callType) -> PartialResult
{
    TRACE_CF("Call: entered with ", signature);

    ValueResults values;
    auto result = emitDirectCall(callProfileIndex, functionIndexSpace, signature, args, values, callType);
    if (!result.has_value()) [[unlikely]]
        return result;

    const bool isTailCallRootCaller = callType == CallType::TailCall && !m_inlineParent;
    const bool isTailCallInlineCaller = callType == CallType::TailCall && m_inlineParent;

    if (!isTailCallRootCaller) {
        for (Value* value : values)
            results.append(push(value));
        if (isTailCallInlineCaller)
            return addInlinedReturn(results);
    }

    return { };
}

auto OMGIRGenerator::emitDirectCall(unsigned callProfileIndex, FunctionSpaceIndex functionIndexSpace, const TypeDefinition& signature, ArgumentList& args, ValueResults& results, CallType callType) -> PartialResult
{
    if (!m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace)) {
        // Record the callee so the callee knows to look for it in updateCallsitesToCallUs.
        // FIXME: This could only record the callees from inlined functions since BBQ should have reported any direct callees before so we don't do the extra
        // bookkeeping for edges we already know about.
        m_directCallees.testAndSet(m_info.toCodeIndex(functionIndexSpace));
    }

    const bool isTailCallRootCaller = callType == CallType::TailCall && !m_inlineParent;
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    const auto& callingConvention = wasmCallingConvention();
    Checked<int32_t> tailCallStackOffsetFromFP;
    CallInformation wasmCalleeInfo = callingConvention.callInformationFor(signature, CallRole::Caller);
    CallInformation wasmCalleeInfoAsCallee = callingConvention.callInformationFor(signature, CallRole::Callee);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCalleeInfo.headerAndArgumentStackSizeInBytes);
    if (isTailCallRootCaller)
        calleeStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(wasmCalleeInfo.headerAndArgumentStackSizeInBytes * 2 + sizeof(Register));
    const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex).expand();
    CallInformation wasmCallerInfoAsCallee = callingConvention.callInformationFor(callerTypeDefinition, CallRole::Callee);

    B3::Type returnType = toB3ResultType(&signature);
    Value* jumpDestination = nullptr;

    m_makesCalls = true;
    if (callType == CallType::TailCall)
        m_makesTailCalls = true;

    m_proc.requestCallArgAreaSizeInBytes(calleeStackSize + sizeof(Register));

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace)) {
        auto emitCallToImport = [&, this](PatchpointValue* patchpoint, RefPtr<PatchpointExceptionHandle> handle, RefPtr<B3::StackmapGenerator> prepareForCall) -> void {
            unsigned patchArgsIndex = patchpoint->reps().size();
            if (is32Bit() && isTailCallRootCaller)
                patchpoint->append(jumpDestination, ValueRep::stackArgument(0));
            else
                patchpoint->append(jumpDestination, ValueRep(GPRInfo::nonPreservedNonArgumentGPR0));
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we could be calling to something that is bounds checking.
            // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(RegisterSetBuilder::wasmPinnedRegisters());
            patchArgsIndex += m_proc.resultCount(patchpoint->type());
            patchpoint->setGenerator([this, patchArgsIndex, handle, isTailCallRootCaller, tailCallStackOffsetFromFP, prepareForCall](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                if (prepareForCall)
                    prepareForCall->run(jit, params);
                ASSERT(!isTailCallRootCaller || !handle);
                if (handle)
                    handle->collectStackMap(this, params);
                if (isTailCallRootCaller) {
                    GPRReg callTarget;
                    if (is32Bit() && params[patchArgsIndex].isStack()) {
                        callTarget = MacroAssembler::addressTempRegister;
                        jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister,
                            tailCallPatchpointScratchOffsets[0] - sizeof(CallerFrameAndPC)), callTarget);
                    } else
                        callTarget = params[patchArgsIndex].gpr();
                    if (WasmOMGIRGeneratorInternal::verboseTailCalls) {
                        jit.probeDebug([callTarget] (Probe::Context& context) {
                            dataLogLn("Can now jump: ", RawHex(context.gpr<uintptr_t>(callTarget)));
                        });
                    }
                    jit.farJump(callTarget, WasmEntryPtrTag);
                }
                else {
                    jit.call(params[patchArgsIndex].gpr(), WasmEntryPtrTag);
                    // Restore the stack pointer since it may have been lowered if our callee did a tail call.
                    jit.addPtr(CCallHelpers::TrustedImm32(-params.code().frameSize()), GPRInfo::callFrameRegister, MacroAssembler::stackPointerRegister);
                }
            });
        };

        m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

        // FIXME: Let's remove this indirection by creating a PIC friendly IC
        // for calls out to the js. This shouldn't be that hard to do. We could probably
        // implement the IC to be over Context*.
        // https://bugs.webkit.org/show_bug.cgi?id=170375
        jumpDestination = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfImportFunctionStub(functionIndexSpace)));
        m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_importFunctionStubs[functionIndexSpace], jumpDestination);

        if (isTailCallRootCaller) {
            auto [patchpoint, handle, prepareForCall] = createTailCallPatchpoint(m_currentBlock, signature, wasmCallerInfoAsCallee, wasmCalleeInfoAsCallee, args, { });
            emitCallToImport(patchpoint, handle, prepareForCall);
            return { };
        }

        auto [patchpoint, handle, prepareForCall] = createCallPatchpoint(m_currentBlock, signature, wasmCalleeInfo, args);
        emitCallToImport(patchpoint, handle, prepareForCall);

        if (returnType != B3::Void)
            fillCallResults(patchpoint, signature, results);

        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_currentBlock);
        return { };

    } // isImportedFunctionFromFunctionIndexSpace

    Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;

    auto emitUnlinkedWasmToWasmCall = [&, this](PatchpointValue* patchpoint, RefPtr<PatchpointExceptionHandle> handle, RefPtr<B3::StackmapGenerator> prepareForCall) -> void {
        patchpoint->setGenerator([this, handle, unlinkedWasmToWasmCalls, functionIndexSpace, isTailCallRootCaller, tailCallStackOffsetFromFP, prepareForCall](CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            if (prepareForCall)
                prepareForCall->run(jit, params);
            if (handle)
                handle->collectStackMap(this, params);

            ASSERT(!m_info.isImportedFunctionFromFunctionIndexSpace(functionIndexSpace));

            // If the call is self-recursion, it is guaranteed that callee will be OMG as well since it ends up calling itself.
            // Since the OMG function prologue will put callee, the caller does not need to place it.
            bool selfRecursion = &m_inlineRoot->m_info == &m_info && m_info.toCodeIndex(functionIndexSpace) == m_inlineRoot->m_functionIndex;
            if (!(selfRecursion && !isTailCallRootCaller)) {
                Ref<IPIntCallee> callee = m_calleeGroup.ipintCalleeFromFunctionIndexSpace(functionIndexSpace);
                jit.storeWasmCalleeToCalleeCallFrame(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(callee.ptr())), isTailCallRootCaller ? sizeof(CallerFrameAndPC) - prologueStackPointerDelta() : 0);
            }
            auto call = isTailCallRootCaller ? jit.threadSafePatchableNearTailCall() : jit.threadSafePatchableNearCall();

            jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace](LinkBuffer& linkBuffer) {
                unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndexSpace });
            });
            jit.addPtr(CCallHelpers::TrustedImm32(-params.code().frameSize()), GPRInfo::callFrameRegister, MacroAssembler::stackPointerRegister);
        });
    };

    if (isTailCallRootCaller) {
        auto [patchpoint, handle, prepareForCall] = createTailCallPatchpoint(m_currentBlock, signature, wasmCallerInfoAsCallee, wasmCalleeInfoAsCallee, args, { });
        emitUnlinkedWasmToWasmCall(patchpoint, handle, prepareForCall);
        return { };
    }

    if (callType == CallType::Call && canInline(functionIndexSpace, callProfileIndex)) {
        auto functionIndex = m_info.toCodeIndex(functionIndexSpace);
        dataLogLnIf(WasmOMGIRGeneratorInternal::verboseInlining, " inlining call to ", functionIndex, " from ", m_functionIndex, " depth ", m_inlineDepth);
        m_inlineRoot->m_inlinedBytes += m_info.functionWasmSizeImportSpace(functionIndexSpace);
        return emitInlineDirectCall(functionIndex, signature, args, results);
    }

    // We do not need to store |this| with JS instance since,
    // 1. It is not tail-call. So this does not clobber the arguments of this function.
    // 2. We are not changing instance. Thus, |this| of this function's arguments are the same and OK.

    auto [patchpoint, handle, prepareForCall] = createCallPatchpoint(m_currentBlock, signature, wasmCalleeInfo, args);
    emitUnlinkedWasmToWasmCall(patchpoint, handle, prepareForCall);
    // We need to clobber the size register since the IPInt always bounds checks
#if OMG_JSVALUE_32_64_PINNED_MEMORY_REGISTERS
    if (useSignalingMemory() || m_info.memory.isShared())
        patchpoint->clobberLate(RegisterSetBuilder { GPRInfo::wasmBoundsCheckingSizeRegister });
#endif


    fillCallResults(patchpoint, signature, results);

    if (m_info.callCanClobberInstance(functionIndexSpace)) {
        patchpoint->clobberLate(RegisterSetBuilder::wasmPinnedRegisters());
        restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_currentBlock);
    }

    return { };
}

auto OMGIRGenerator::addCallIndirect(unsigned callProfileIndex, unsigned tableIndex, const TypeDefinition& originalSignature, ArgumentList& args, ResultList& results, CallType callType) -> PartialResult
{
    UNUSED_PARAM(callProfileIndex);
    Value* calleeIndex = get(args.takeLast());
    const TypeDefinition& signature = originalSignature.expand();
    const bool isTailCallRootCaller = callType == CallType::TailCall && !m_inlineParent;
    const bool isTailCallInlineCaller = callType == CallType::TailCall && m_inlineParent;
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    TRACE_CF("Call_indirect: entered with table index: ", tableIndex, " ", originalSignature);

    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the js, we conservatively assume all call indirects
    // can be to the js for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    Value* callableFunctionBuffer = nullptr;
    Value* callableFunctionBufferLength;
    {
        ASSERT(tableIndex < m_info.tableCount());
        auto& tableInformation = m_info.table(tableIndex);

        if (tableInformation.maximum() && tableInformation.maximum().value() == tableInformation.initial()) {
            // The buffer is immutable & non-control-dependent since this table is not resizable / reaching to the maximum size.
            callableFunctionBufferLength = constant(B3::Int32, tableInformation.initial(), origin());
            if (!tableIndex) {
                auto* result = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedTable0Buffer()));
                m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_cachedTable0Buffer, result);
                result->setReadsMutability(B3::Mutability::Immutable);
                result->setControlDependent(false);
                callableFunctionBuffer = result;
            } else {
                auto* table = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfTable(m_info, tableIndex)));
                m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_tables[tableIndex], table);
                table->setReadsMutability(B3::Mutability::Immutable);
                table->setControlDependent(false);

            if (!tableInformation.isImport()) {
                // Table is fixed-sized and it is not imported one. Thus this is definitely fixed-sized FuncRefTable.
                callableFunctionBuffer = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), table, constant(pointerType(), safeCast<int32_t>(FuncRefTable::offsetOfFunctionsForFixedSizedTable())));
                } else {
                    auto* result = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), table, safeCast<int32_t>(FuncRefTable::offsetOfFunctions()));
                    m_heaps.decorateMemory(&m_heaps.WasmFuncRefTable_functions, result);
                    result->setReadsMutability(B3::Mutability::Immutable);
                    result->setControlDependent(false);
                    callableFunctionBuffer = result;
                }
            }
        } else {
            if (!tableIndex) {
                callableFunctionBufferLength = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedTable0Length()));
                m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_cachedTable0Length, callableFunctionBufferLength);

                callableFunctionBuffer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCachedTable0Buffer()));
                m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_cachedTable0Buffer, callableFunctionBuffer);
            } else {
                auto* table = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfTable(m_info, tableIndex)));
                m_heaps.decorateMemory(&m_heaps.JSWebAssemblyInstance_tables[tableIndex], table);
                table->setReadsMutability(B3::Mutability::Immutable);
                table->setControlDependent(false);

            callableFunctionBufferLength = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), table, safeCast<int32_t>(Table::offsetOfLength()));
                m_heaps.decorateMemory(&m_heaps.WasmTable_length, callableFunctionBufferLength);

            callableFunctionBuffer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), table, safeCast<int32_t>(FuncRefTable::offsetOfFunctions()));
                m_heaps.decorateMemory(&m_heaps.WasmFuncRefTable_functions, callableFunctionBuffer);
    }
        }
    }

    // Check the index we are looking for is valid.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), calleeIndex, callableFunctionBufferLength));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsCallIndirect);
        });
    }

    calleeIndex = pointerOfInt32(calleeIndex);

    BasicBlock* continuation = m_proc.addBlock();

    Value* callableFunction = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), callableFunctionBuffer, m_currentBlock->appendNew<Value>(m_proc, Mul, origin(), calleeIndex, constant(pointerType(), sizeof(FuncRefTable::Function))));

    // Check that the WasmToWasmImportableFunction is initialized. We trap if it isn't. An "invalid" SignatureIndex indicates it's not initialized.
    // FIXME: when we have trap handlers, we can just let the call fail because Signature::invalidIndex is 0. https://bugs.webkit.org/show_bug.cgi?id=177210
    static_assert(sizeof(WasmToWasmImportableFunction::typeIndex) == sizeof(uintptr_t), "Load codegen assumes ptr");
    Value* calleeCallee = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callableFunction, safeCast<int32_t>(FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfBoxedCallee()));
    m_heaps.decorateMemory(&m_heaps.WasmFuncRefTableFunction_boxedCallee, calleeCallee);

    Value* calleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callableFunction, safeCast<int32_t>(FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfTargetInstance()));
    m_heaps.decorateMemory(&m_heaps.WasmFuncRefTableFunction_targetInstance, calleeInstance);

    ValueResults fastValues;
    Value* calleeCodeLocation = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callableFunction, safeCast<int32_t>(FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation()));
    m_heaps.decorateMemory(&m_heaps.WasmFuncRefTableFunction_entrypointLoadLocation, calleeCodeLocation);

    Value* calleeRTT = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callableFunction, safeCast<int32_t>(FuncRefTable::Function::offsetOfFunction() + WasmToWasmImportableFunction::offsetOfRTT()));
    m_heaps.decorateMemory(&m_heaps.WasmFuncRefTableFunction_rtt, calleeRTT);

    auto signatureRTT = TypeInformation::getCanonicalRTT(originalSignature.index());

    Value* expectedRTT = constant(pointerType(), std::bit_cast<uintptr_t>(signatureRTT.ptr()));

    if (originalSignature.isFinalType()) {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), calleeRTT, expectedRTT));
        check->setGenerator([=, this, origin = this->origin()](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::BadSignature);
        });
    } else {
        BasicBlock* checkDone = m_proc.addBlock();
        BasicBlock* moreChecks = m_proc.addBlock();

        Value* hasEqualSignatures = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), calleeRTT, expectedRTT);
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), hasEqualSignatures, FrequentedBlock(checkDone), FrequentedBlock(moreChecks, FrequencyClass::Rare));
        moreChecks->addPredecessor(m_currentBlock);
        checkDone->addPredecessor(m_currentBlock);

        m_currentBlock = moreChecks;
        CheckValue* checkNull = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), calleeRTT, constant(pointerType(), 0)));
        checkNull->setGenerator([=, this, origin = this->origin()](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::BadSignature);
        });

        auto* rttSize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), calleeRTT, safeCast<int32_t>(RTT::offsetOfDisplaySizeExcludingThis()));
        m_heaps.decorateMemory(&m_heaps.WasmRTT_displaySizeExcludingThis, rttSize);

        CheckValue* checkRTTSize = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), m_currentBlock->appendNew<Value>(m_proc, BelowEqual, origin(), rttSize, constant(Int32, signatureRTT->displaySizeExcludingThis())));
        checkRTTSize->setGenerator([=, this, origin = this->origin()](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::BadSignature);
        });

        auto* displayEntry = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), calleeRTT, safeCast<int32_t>(RTT::offsetOfData() + signatureRTT->displaySizeExcludingThis() * sizeof(RefPtr<const RTT>)));
        m_heaps.decorateMemory(&m_heaps.WasmRTT_data[signatureRTT->displaySizeExcludingThis()], displayEntry);
        displayEntry->setReadsMutability(B3::Mutability::Immutable);
        displayEntry->setControlDependent(false);

        CheckValue* checkRTTDisplayEntry = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), displayEntry, constant(pointerType(), std::bit_cast<uintptr_t>(signatureRTT.ptr()))));
        checkRTTDisplayEntry->setGenerator([=, this, origin = this->origin()](CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitExceptionCheck(jit, origin, ExceptionType::BadSignature);
            });

        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), checkDone);
        checkDone->addPredecessor(m_currentBlock);

        m_currentBlock = checkDone;
    }

    Value* calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), calleeCodeLocation);
    m_heaps.decorateMemory(&m_heaps.WasmWasmCallableFunctionLocation_value, calleeCode);

    ValueResults slowValues;
    auto result = emitIndirectCall(calleeInstance, calleeCode, calleeCallee, signature, args, slowValues, callType);
    if (!result.has_value()) [[unlikely]]
        return result;

    if (!isTailCallRootCaller) {
        for (Value*& value : slowValues) {
            auto* input = value;
            value = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), input);
        }
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);

        m_currentBlock = continuation;
        for (unsigned i = 0; i < slowValues.size(); ++i) {
            auto* slowValue = slowValues[i];
            auto* phi = m_currentBlock->appendNew<Value>(m_proc, Phi, slowValue->child(0)->type(), origin());
            slowValue->as<UpsilonValue>()->setPhi(phi);
            if (!fastValues.isEmpty()) {
                auto* fastValue = fastValues[i];
                fastValue->as<UpsilonValue>()->setPhi(phi);
            }
            results.append(push(phi));
    }

        if (isTailCallInlineCaller) {
            ASSERT(m_returnContinuation);
            return addInlinedReturn(results);
        }
    }

    return { };
}

auto OMGIRGenerator::addCallRef(unsigned callProfileIndex, const TypeDefinition& originalSignature, ArgumentList& args, ResultList& results, CallType callType) -> PartialResult
{
    UNUSED_PARAM(callProfileIndex);
    TypedExpression calleeArg = args.takeLast();
    Value* callee = get(calleeArg);
    TRACE_VALUE(Wasm::Types::Void, callee, "call_ref: ", originalSignature);
    const TypeDefinition& signature = originalSignature.expand();
    const bool isTailCallRootCaller = callType == CallType::TailCall && !m_inlineParent;
    const bool isTailCallInlineCaller = callType == CallType::TailCall && m_inlineParent;
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
    m_makesCalls = true;

    TRACE_CF("CallRef: entered with ", signature);

    // Note: call ref can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the js, we conservatively assume all call indirects
    // can be to the js for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    ptrdiff_t offset = safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfTargetInstance());
    bool canTrap = false;
    if (calleeArg.type().isNullable())
        canTrap = emitNullCheckBeforeAccess(callee, offset);
    auto wrapTrapping = [&](auto input) -> B3::Kind {
        if (canTrap)
            return trapping(input);
        return input;
    };

    Value* calleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Load), pointerType(), origin(), pointerOfWasmRef(callee), offset);
    m_heaps.decorateMemory(&m_heaps.WebAssemblyFunctionBase_targetInstance, calleeInstance);

    Value* calleeCallee = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), pointerOfWasmRef(callee), safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfBoxedCallee()));
    m_heaps.decorateMemory(&m_heaps.WebAssemblyFunctionBase_boxedCallee, calleeCallee);

    BasicBlock* continuation = m_proc.addBlock();

    ValueResults fastValues;
    Value* calleeCodeLocation = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), pointerOfWasmRef(callee), safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation()));
    m_heaps.decorateMemory(&m_heaps.WebAssemblyFunctionBase_entrypointLoadLocation, calleeCodeLocation);

    Value* calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), calleeCodeLocation);
    m_heaps.decorateMemory(&m_heaps.WasmWasmCallableFunctionLocation_value, calleeCode);

    ValueResults slowValues;
    auto result = emitIndirectCall(calleeInstance, calleeCode, calleeCallee, signature, args, slowValues, callType);
    if (!result.has_value()) [[unlikely]]
        return result;

    if (!isTailCallRootCaller) {
        for (Value*& value : slowValues) {
            auto* input = value;
            value = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), input);
        }
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);

        m_currentBlock = continuation;
        for (unsigned i = 0; i < slowValues.size(); ++i) {
            auto* slowValue = slowValues[i];
            auto* phi = m_currentBlock->appendNew<Value>(m_proc, Phi, slowValue->child(0)->type(), origin());
            slowValue->as<B3::UpsilonValue>()->setPhi(phi);
            if (!fastValues.isEmpty()) {
                auto* fastValue = fastValues[i];
                fastValue->as<B3::UpsilonValue>()->setPhi(phi);
            }
            results.append(push(phi));
        }

        if (isTailCallInlineCaller) {
            ASSERT(m_returnContinuation);
            return addInlinedReturn(results);
        }
    }
    return { };
}

void OMGIRGenerator::unify(Value* phi, const ExpressionType source)
{
    m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), get(source), phi);
}

void OMGIRGenerator::unifyValuesWithBlock(const Stack& resultStack, const ControlData& block)
{
    const Vector<Value*>& phis = block.phis;
    size_t resultSize = phis.size();

    ASSERT(resultSize <= resultStack.size());

    for (size_t i = 0; i < resultSize; ++i)
        unify(phis[resultSize - 1 - i], resultStack.at(resultStack.size() - 1 - i));
}

static void dumpExpressionStack(const CommaPrinter& comma, const OMGIRGenerator::Stack& expressionStack)
{
    dataLog(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, *expression);
}

void OMGIRGenerator::dump(const ControlStack& controlStack, const Stack* expressionStack)
{
    dataLogLn("Constants:");
    for (const auto& constant : m_constantPool)
        dataLogLn(deepDump(m_proc, constant.value));

    dataLogLn("Processing Graph:");
    dataLog(m_proc);
    dataLogLn("With current block:", *m_currentBlock);
    dataLogLn("Control stack:");
    ASSERT(controlStack.size());
    for (size_t i = controlStack.size(); i--;) {
        dataLog("  ", controlStack[i].controlData, ": ");
        CommaPrinter comma(", "_s, ""_s);
        dumpExpressionStack(comma, *expressionStack);
        expressionStack = &controlStack[i].enclosedExpressionStack;
        dataLogLn();
    }
    dataLogLn();
}

auto OMGIRGenerator::origin() -> Origin
{
    if (!m_parser)
        return Origin();

    OpcodeOrigin opcodeOrigin = OpcodeOrigin(m_parser->currentOpcode(), m_parser->currentOpcodeStartingOffset());
    switch (m_parser->currentOpcode()) {
    case OpType::Ext1:
    case OpType::ExtGC:
    case OpType::ExtAtomic:
    case OpType::ExtSIMD:
        opcodeOrigin = OpcodeOrigin(m_parser->currentOpcode(), m_parser->currentExtendedOpcode(), m_parser->currentOpcodeStartingOffset());
        break;
    default:
        break;
    }
    ASSERT(isValidOpType(static_cast<uint8_t>(opcodeOrigin.opcode())));
    WasmOrigin result { CallSiteIndex(callSiteIndex()), opcodeOrigin };
    if (m_context.origins.isEmpty() || m_context.origins.last() != result)
        m_context.origins.append(result);
    return Origin(&m_context.origins.last());
}

static bool shouldDumpIRFor(uint32_t functionIndex)
{
    static LazyNeverDestroyed<FunctionAllowlist> dumpAllowlist;
    static std::once_flag initializeAllowlistFlag;
    std::call_once(initializeAllowlistFlag, [] {
        const char* functionAllowlistFile = Options::wasmOMGFunctionsToDump();
        dumpAllowlist.construct(functionAllowlistFile);
    });
    return dumpAllowlist->shouldDumpWasmFunction(functionIndex);
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileOMG(CompilationContext& compilationContext, IPIntCallee& profiledCallee, OptimizingJITCallee& callee, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, Module& module, CalleeGroup& calleeGroup, const ModuleInformation& info, MemoryMode mode, CompilationMode compilationMode, FunctionCodeIndex functionIndex, uint32_t loopIndexForOSREntry)
{
    CompilerTimingScope totalScope("B3"_s, "Total OMG compilation"_s);

    Wasm::Thunks::singleton().stub(Wasm::catchInWasmThunkGenerator);

    auto result = makeUnique<InternalFunction>();

    AbstractHeapRepository heaps;
    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();
    compilationContext.procedure = makeUniqueWithoutFastMallocCheck<Procedure>(info.usesSIMD(functionIndex));

    Procedure& procedure = *compilationContext.procedure;
    if (shouldDumpIRFor(functionIndex + info.importFunctionCount()))
        procedure.setShouldDumpIR();

    procedure.setNeedsPCToOriginMap();
    procedure.setOriginPrinter([](PrintStream& out, Origin origin) {
        if (auto* impl = origin.maybeWasmOrigin())
            out.print("Wasm: ", impl->m_opcodeOrigin, " CallSiteIndex: ", impl->m_callSiteIndex.bits());
    });

    // This means we cannot use either StackmapGenerationParams::usedRegisters() or
    // StackmapGenerationParams::unavailableRegisters(). In exchange for this concession, we
    // don't strictly need to run Air::reportUsedRegisters(), which saves a bit of CPU time at
    // optLevel=1.
    procedure.setNeedsUsedRegisters(false);

    procedure.setOptLevel(Options::wasmOMGOptimizationLevel());

    procedure.code().setForceIRCRegisterAllocation();

    result->outgoingJITDirectCallees = FixedBitVector(info.internalFunctionCount());
    OMGIRGenerator irGenerator(heaps, compilationContext, module, calleeGroup, info, profiledCallee, callee, procedure, unlinkedWasmToWasmCalls, result->outgoingJITDirectCallees, result->osrEntryScratchBufferSize, mode, compilationMode, functionIndex, loopIndexForOSREntry);
    FunctionParser<OMGIRGenerator> parser(irGenerator, function.data, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    irGenerator.insertEntrySwitch();
    irGenerator.insertConstants();

    // Make sure everything is decorated. This does a bunch of deferred decorating. This has
    // to happen last because our abstract heaps are generated lazily. They have to be
    // generated lazily because we have an infinite number of numbered, indexed, and
    // absolute heaps. We only become aware of the ones we actually mention while lowering.
    heaps.computeRangesAndDecorateInstructions();

    procedure.resetReachability();
    if (ASSERT_ENABLED)
        validate(procedure, "After parsing:\n");

    estimateStaticExecutionCounts(procedure);

    dataLogIf(WasmOMGIRGeneratorInternal::verbose, "Pre SSA: ", procedure);
    fixSSA(procedure);
    dataLogIf(WasmOMGIRGeneratorInternal::verbose, "Post SSA: ", procedure);

    {
        if (shouldDumpDisassemblyFor(compilationMode))
            procedure.code().setDisassembler(makeUniqueWithoutFastMallocCheck<B3::Air::Disassembler>());
        B3::prepareForGeneration(procedure);
        B3::generate(procedure, *compilationContext.wasmEntrypointJIT);
        compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
        result->entrypoint.calleeSaveRegisters = procedure.calleeSaveRegisterAtOffsetList();
    }

    result->stackmaps = irGenerator.takeStackmaps();
    result->exceptionHandlers = irGenerator.takeExceptionHandlers();

    if (compilationMode == CompilationMode::OMGForOSREntryMode) {
        int32_t checkSize = 0;
        bool needsOverflowCheck = false;
        irGenerator.computeStackCheckSize(needsOverflowCheck, checkSize);
        ASSERT(checkSize || !needsOverflowCheck);
        if (!needsOverflowCheck)
            checkSize = stackCheckNotNeeded;
        uncheckedDowncast<OMGOSREntryCallee>(callee).setStackCheckSize(checkSize);
    }

    return result;
}

// Custom wasm ops. These are the ones too messy to do in wasm.json.

void OMGIRGenerator::emitChecksForModOrDiv(B3::Opcode operation, Value* left, Value* right)
{
    ASSERT(operation == Div || operation == Mod || operation == UDiv || operation == UMod);
    const B3::Type type = left->type();

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), right, constant(type, 0)));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::DivisionByZero);
        });
    }

    if (operation == Div) {
        int64_t min = type == Int32 ? std::numeric_limits<int32_t>::min() : std::numeric_limits<int64_t>::min();

        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), left, constant(type, min)),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), right, constant(type, -1))));

        check->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, origin, ExceptionType::IntegerOverflow);
        });
    }
}

auto OMGIRGenerator::addI32DivS(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI32RemS(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, chill(op), origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI32DivU(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI32RemU(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI64DivS(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI64RemS(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, chill(op), origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI64DivU(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI64RemU(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

auto OMGIRGenerator::addI32Ctz(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

auto OMGIRGenerator::addI64Ctz(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* argLo = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), arg);
    Value* argHi = m_currentBlock->appendNew<Value>(m_proc, TruncHigh, origin(), arg);
    PatchpointValue* ctzLo = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    ctzLo->append(argLo, ValueRep::SomeRegister);
    ctzLo->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    PatchpointValue* ctzHi = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    ctzHi->append(argHi, ValueRep::SomeRegister);
    ctzHi->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    ctzHi->effects = Effects::none();
    Value* thirtyTwo = m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 32);
    Value* useLo = m_currentBlock->appendNew<Value>(m_proc, Below, origin(), ctzLo, thirtyTwo);
    Value* ctzIfHi = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), ctzHi, thirtyTwo);
    Value* select = m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), useLo, ctzLo, ctzIfHi);
    result = push(m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), select));
    return { };
}

auto OMGIRGenerator::addI32Popcnt(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    if (MacroAssembler::supportsCountPopulation()) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
        patchpoint->append(arg, ValueRep::SomeRegister);
#if CPU(X86_64)
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation32(params[1].gpr(), params[0].gpr());
        });
#else
        patchpoint->numFPScratchRegisters = 1;
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation32(params[1].gpr(), params[0].gpr(), params.fpScratch(0));
        });
#endif
        patchpoint->effects = Effects::none();
        result = push(patchpoint);
        return { };
    }

    // Pure math function does not need to call emitPrepareWasmOperation.
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationPopcount32));
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(), Effects::none(), funcAddress, arg));
    return { };
}

auto OMGIRGenerator::addI64Popcnt(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    if (MacroAssembler::supportsCountPopulation()) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
        patchpoint->append(arg, ValueRep::SomeRegister);
#if CPU(X86_64)
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation64(params[1].gpr(), params[0].gpr());
        });
#else
        patchpoint->numFPScratchRegisters = 1;
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation64(params[1].gpr(), params[0].gpr(), params.fpScratch(0));
        });
#endif
        patchpoint->effects = Effects::none();
        result = push(patchpoint);
        return { };
    }

    // Pure math function does not need to call emitPrepareWasmOperation.
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationPopcount64));
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, Int64, origin(), Effects::none(), funcAddress, arg));
    return { };
}

auto OMGIRGenerator::addF64ConvertUI64(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Double, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::f64_convert_u_i64)),
        arg);

    result = push(call);
    return { };
}

auto OMGIRGenerator::addF32ConvertUI64(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Float, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::f32_convert_u_i64)),
        arg);
    result = push(call);
    return { };
}

auto OMGIRGenerator::addF64Nearest(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* callee = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::f64_roundeven));
    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Double, origin(), callee, arg);
    result = push(call);
    return { };
}

auto OMGIRGenerator::addF32Nearest(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* callee = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::f32_roundeven));
    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Float, origin(), callee, arg);
    result = push(call);
    return { };
}

auto OMGIRGenerator::addI32TruncSF64(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, std::bit_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
    Value* min = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

auto OMGIRGenerator::addI32TruncSF32(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, std::bit_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
    Value* min = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}


auto OMGIRGenerator::addI32TruncUF64(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
    Value* min = constant(Double, std::bit_cast<uint64_t>(-1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

auto OMGIRGenerator::addI32TruncUF32(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
    Value* min = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(-1.0)));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

auto OMGIRGenerator::addI64TruncSF64(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, std::bit_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
    Value* min = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });
    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_s_f64)),
        arg);
    result = push(call);
    return { };
}

auto OMGIRGenerator::addI64TruncUF64(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, std::bit_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
    Value* min = constant(Double, std::bit_cast<uint64_t>(-1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });

    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_u_f64)),
        arg);
    result = push(call);
    return { };
}

auto OMGIRGenerator::addI64TruncSF32(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, std::bit_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
    Value* min = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });
    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_s_f32)),
        arg);
    result = push(call);
    return { };
}

auto OMGIRGenerator::addI64TruncUF32(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
    Value* min = constant(Float, std::bit_cast<uint32_t>(static_cast<float>(-1.0)));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this, origin = this->origin()] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, origin, ExceptionType::OutOfBoundsTrunc);
    });

    Value* call = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(Math::i64_trunc_u_f32)),
        arg);
    result = push(call);
    return { };
}

} } // namespace JSC::Wasm

#include "WasmOMGIRGeneratorInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // USE(JSVALUE32_64)
#endif // ENABLE(WEBASSEMBLY_OMGJIT)
