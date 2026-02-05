/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WasmIPIntGenerator.h"
#include <cstdint>

#if ENABLE(WEBASSEMBLY)

#include "BytecodeGeneratorBaseInlines.h"
#include "BytecodeStructs.h"
#include "InstructionStream.h"
#include "JSCJSValueInlines.h"
#include "Label.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmFunctionIPIntMetadataGenerator.h"
#include "WasmFunctionParser.h"
#include "WasmModuleDebugInfo.h"
#include <wtf/Assertions.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RefPtr.h>

/* 
 * WebAssembly in-place interpreter metadata generator
 * 
 * docs by Daniel Liu <daniel_liu4@apple.com / danlliu@umich.edu>; 2023 intern project
 * 
 * 1. Why Metadata?
 * ----------------
 * 
 * WebAssembly's bytecode format isn't always the easiest to interpret by itself: jumps would require parsing
 * through many bytes to find their target, constants are stored in LEB128, and a myriad of other reasons.
 * For IPInt, we design metadata to act as "supporting information" for the interpreter, allowing it to quickly
 * find important values such as constants, indices, and branch targets.
 *
 * FIXME: We should consider not aligning on Apple ARM64 cores since they don't typically have a penatly for unaligned loads/stores.
 * 
 * 2. Metadata Structure
 * ---------------------
 * 
 * Metadata is kept in a vector of UInt8 (bytes). We handle metadata in "metadata entries", which are groups of
 * 8 metadata bytes. We keep metadata aligned to 8B to improve access times. Sometimes, this results in higher
 * memory overhead; however, these cases are relatively sparse. Each instruction pushes a certain number of
 * entries to the metadata vector.
 *
 * 3. Metadata for Instructions
 * ----------------------------
 *
 * block (0x02):            1 entry; 8B PC of next instruction 
 * loop (0x03):             1 entry; 8B PC of next instruction 
 * if (0x04):               2 entries; 4B new PC, 4B new MC for `else`, 8B new PC for `if`
 * else (0x05):             1 entry; 4B new PC, 4B new MC for `end`
 * end (0x0b):              If exiting the function: ceil((# return values + 2) / 8) entries; 2B for total entry size, 1B / value returned
 * br (0x0c):               2 entries; 4B new PC, 4B new MC, 2B number of values to pop, 2B arity, 4B PC after br
 * br_if (0x0d):            2 entries; same as br
 * br_table (0x0e):         1 + 2n entries for n branches: 8B number of targets; n br metadata entries
 * local.get (0x20):        1 entry; 4B index of local, 4B size of instruction
 * local.set (0x21):        1 entry; 4B index of local, 4B size of instruction
 * local.tee (0x22):        2 entries because of how FunctionParser works
 * global.get (0x23):       1 entry; 4B index of global, 4B size of instruction
 * global.set (0x24):       1 entry; 4B index of global, 4B size of instruction
 * table.get (0x23):        1 entry; 4B index of table, 4B size of instruction
 * table.set (0x24):        1 entry; 4B index of table, 4B size of instruction
 * mem load (0x28 - 0x35):  1 entry; 4B memarg, 4B size of instruction
 * mem store (0x28 - 0x35): 1 entry; 4B memarg, 4B size of instruction
 * i32.const (0x41):        1 entry; 4B value, 4B size of instruction
 * i64.const (0x42):        2 entries; 8B value, 8B size of instruction
 *
 * i32, i64, f32, and f64 operations (besides the ones shown above) do not require metadata
 * 
 */

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(WEBASSEMBLY_DEBUGGER)
#define RECORD_NEXT_INSTRUCTION(fromPC, toPC)                                                            \
    do {                                                                                                 \
        if (Options::enableWasmDebugger()) [[unlikely]] {                                                \
            if (m_debugInfo) {                                                                           \
                uint32_t fromOffset = fromPC + m_metadata->m_bytecodeOffset + m_functionStartByteOffset; \
                uint32_t toOffset = toPC + m_metadata->m_bytecodeOffset + m_functionStartByteOffset;     \
                m_debugInfo->addNextInstruction(fromOffset, toOffset);                                   \
            }                                                                                            \
        }                                                                                                \
    } while (0)
#else
#define RECORD_NEXT_INSTRUCTION(fromPC, toPC) do { (void)(fromPC); (void)(toPC); } while (0)
#endif

namespace JSC { namespace Wasm {

using ErrorType = String;
using PartialResult = Expected<void, ErrorType>;
using UnexpectedResult = Unexpected<ErrorType>;
struct Value { };

// ControlBlock

struct IPIntLocation {
    uint32_t pc;
    uint32_t mc;
};

struct IPIntControlType {

    friend class IPIntGenerator;

    IPIntControlType()
    {
    }

    IPIntControlType(BlockSignature&& signature, uint32_t stackSize, BlockType blockType, CatchKind catchKind = CatchKind::Catch)
        : m_signature(WTF::move(signature))
        , m_blockType(blockType)
        , m_catchKind(catchKind)
        , m_stackSize(stackSize)
    { }

    static bool isIf(const IPIntControlType& control) { return control.blockType() == BlockType::If; }
    static bool isElse(const IPIntControlType& control) { return control.blockType() == BlockType::Else; }
    static bool isTry(const IPIntControlType& control) { return control.blockType() == BlockType::Try; }
    static bool isTryTable(const IPIntControlType& control) { return control.blockType() == BlockType::TryTable; }
    static bool isAnyCatch(const IPIntControlType& control) { return control.blockType() == BlockType::Catch; }
    static bool isTopLevel(const IPIntControlType& control) { return control.blockType() == BlockType::TopLevel; }
    static bool isLoop(const IPIntControlType& control) { return control.blockType() == BlockType::Loop; }
    static bool isBlock(const IPIntControlType& control) { return control.blockType() == BlockType::Block; }
    static bool isCatch(const IPIntControlType& control)
    {
        if (control.blockType() != BlockType::Catch)
            return false;
        return control.catchKind() == CatchKind::Catch;
    }

    void dump(PrintStream&) const
    { }

    BlockType blockType() const { return m_blockType; }
    CatchKind catchKind() const { return m_catchKind; }
    const BlockSignature& signature() const { return m_signature; }
    unsigned stackSize() const { return m_stackSize; }

    Type branchTargetType(unsigned i) const
    {
        ASSERT(i < branchTargetArity());
        if (blockType() == BlockType::Loop)
            return m_signature.argumentType(i);
        return m_signature.returnType(i);
    }

    unsigned branchTargetArity() const
    {
        return isLoop(*this)
            ? m_signature.argumentCount()
            : m_signature.returnCount();
    }

private:
    BlockSignature m_signature;
    BlockType m_blockType;
    CatchKind m_catchKind;

    int32_t m_pendingOffset { -1 };

    uint32_t m_index { 0 };
    uint32_t m_pc { 0 }; // where am i?
    uint32_t m_mc { 0 };
    uint32_t m_pcEnd { 0 };

    uint32_t m_stackSize { 0 };
    uint32_t m_tryDepth { 0 };

    Vector<IPIntLocation> m_catchesAwaitingFixup;

    struct TryTableTarget {
        CatchKind type;
        uint32_t tag;
        const TypeDefinition* exceptionSignature;
        ControlRef target;
    };
    Vector<TryTableTarget> m_tryTableTargets;
};

class IPIntGenerator {
public:
    IPIntGenerator(ModuleInformation&, FunctionCodeIndex, const TypeDefinition&, std::span<const uint8_t>, FunctionDebugInfo* = nullptr);

    static constexpr bool shouldFuseBranchCompare = false;

    using ControlType = IPIntControlType;
    using ExpressionType = Value;
    using CallType = CallLinkInfo::CallType;
    using ResultList = Vector<Value, 8>;

    using ExpressionList = Vector<Value, 1>;
    using ControlEntry = FunctionParser<IPIntGenerator>::ControlEntry;
    using ControlStack = FunctionParser<IPIntGenerator>::ControlStack;
    using Stack = FunctionParser<IPIntGenerator>::Stack;
    using TypedExpression = FunctionParser<IPIntGenerator>::TypedExpression;
    using CatchHandler = FunctionParser<IPIntGenerator>::CatchHandler;
    using ArgumentList = FunctionParser<IPIntGenerator>::ArgumentList;

    static ExpressionType emptyExpression() { return { }; };
    [[nodiscard]] PartialResult addDrop(ExpressionType);

    template <typename ...Args>
    [[nodiscard]] NEVER_INLINE UnexpectedResult fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module failed compiling: "_s, makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (condition) [[unlikely]]                  \
            return fail(__VA_ARGS__);             \
    } while (0)

    std::unique_ptr<FunctionIPIntMetadataGenerator> finalize();

    [[nodiscard]] PartialResult addArguments(const TypeDefinition&);
    [[nodiscard]] PartialResult addLocal(Type, uint32_t);
    Value addConstant(Type, uint64_t);

    // SIMD

    bool usesSIMD() { return m_usesSIMD; }
    void notifyFunctionUsesSIMD() { ASSERT(Options::useWasmSIMD()); m_usesSIMD = true; }
    [[nodiscard]] PartialResult addSIMDLoad(ExpressionType, uint32_t, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDStore(ExpressionType, ExpressionType, uint32_t);
    [[nodiscard]] PartialResult addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDLoadSplat(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t);
    [[nodiscard]] PartialResult addSIMDLoadExtend(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDLoadPad(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&);

    ExpressionType addConstant(v128_t);

    // SIMD generated

    [[nodiscard]] PartialResult addExtractLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&);
#if ENABLE(B3_JIT)
    [[nodiscard]] PartialResult addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&);
#endif
    [[nodiscard]] PartialResult addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&);

    // References

    [[nodiscard]] PartialResult addRefIsNull(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addRefFunc(FunctionSpaceIndex, ExpressionType&);
    [[nodiscard]] PartialResult addRefAsNonNull(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addRefEq(ExpressionType, ExpressionType, ExpressionType&);

    // Tables

    [[nodiscard]] PartialResult addTableGet(unsigned, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addTableSet(unsigned, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addTableInit(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addElemDrop(unsigned);
    [[nodiscard]] PartialResult addTableSize(unsigned, ExpressionType&);
    [[nodiscard]] PartialResult addTableGrow(unsigned, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addTableFill(unsigned, ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addTableCopy(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType);

    // Locals

    [[nodiscard]] PartialResult getLocal(uint32_t index, ExpressionType&);
    [[nodiscard]] PartialResult setLocal(uint32_t, ExpressionType);
    [[nodiscard]] PartialResult teeLocal(uint32_t, ExpressionType, ExpressionType& result);

    // Globals

    [[nodiscard]] PartialResult getGlobal(uint32_t, ExpressionType&);
    [[nodiscard]] PartialResult setGlobal(uint32_t, ExpressionType);

    // Memory

    [[nodiscard]] PartialResult load(LoadOpType, ExpressionType, ExpressionType&, uint64_t);
    [[nodiscard]] PartialResult store(StoreOpType, ExpressionType, ExpressionType, uint64_t);
    [[nodiscard]] PartialResult addGrowMemory(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addCurrentMemory(ExpressionType&);
    [[nodiscard]] PartialResult addMemoryFill(ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addMemoryCopy(ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addMemoryInit(unsigned, ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addDataDrop(unsigned);

    // Atomics

    [[nodiscard]] PartialResult atomicLoad(ExtAtomicOpType, Type, ExpressionType, ExpressionType&, uint32_t);
    [[nodiscard]] PartialResult atomicStore(ExtAtomicOpType, Type, ExpressionType, ExpressionType, uint32_t);
    [[nodiscard]] PartialResult atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType&, uint32_t);
    [[nodiscard]] PartialResult atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t);

    [[nodiscard]] PartialResult atomicWait(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t);
    [[nodiscard]] PartialResult atomicNotify(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType&, uint32_t);
    [[nodiscard]] PartialResult atomicFence(ExtAtomicOpType, uint8_t);

    // Saturated truncation

    [[nodiscard]] PartialResult truncSaturated(Ext1OpType, ExpressionType, ExpressionType&, Type, Type);

    // GC

    [[nodiscard]] PartialResult addRefI31(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI31GetS(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI31GetU(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addArrayNew(uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addArrayNewDefault(uint32_t, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addArrayNewData(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addArrayNewElem(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addArrayNewFixed(uint32_t, ArgumentList&, ExpressionType&);
    [[nodiscard]] PartialResult addArrayGet(ExtGCOpType, uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addArraySet(uint32_t, ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addArrayLen(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addArrayFill(uint32_t, ExpressionType, ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addArrayCopy(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addArrayInitElem(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addArrayInitData(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    [[nodiscard]] PartialResult addStructNew(uint32_t, ArgumentList&, ExpressionType&);
    [[nodiscard]] PartialResult addStructNewDefault(uint32_t, ExpressionType&);
    [[nodiscard]] PartialResult addStructGet(ExtGCOpType, ExpressionType, const StructType&, const RTT&, uint32_t, ExpressionType&);
    [[nodiscard]] PartialResult addStructSet(ExpressionType, const StructType&, const RTT&, uint32_t, ExpressionType);
    [[nodiscard]] PartialResult addRefTest(ExpressionType, bool, int32_t, bool, ExpressionType&);
    [[nodiscard]] PartialResult addRefCast(ExpressionType, bool, int32_t, ExpressionType&);
    [[nodiscard]] PartialResult addAnyConvertExtern(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addExternConvertAny(ExpressionType, ExpressionType&);

    // Basic operators

    [[nodiscard]] PartialResult addI32DivS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32RemS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32DivU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32RemU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64DivS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64RemS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64DivU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64RemU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Ctz(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Popcnt(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Popcnt(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Nearest(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Nearest(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Trunc(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Trunc(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32TruncSF64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32TruncSF32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32TruncUF64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32TruncUF32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64TruncSF64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64TruncSF32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64TruncUF64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64TruncUF32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Ceil(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Mul(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Sub(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Le(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32DemoteF64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Ne(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Lt(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Min(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Max(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Min(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Max(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Mul(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Div(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Clz(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Copysign(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32ReinterpretI32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Ne(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Gt(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Sqrt(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Ge(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64GtS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64GtU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Div(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Add(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32LeU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32LeS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Ne(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Clz(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Neg(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32And(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32LtU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Rotr(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Abs(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32LtS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Eq(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Copysign(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32ConvertSI64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Rotl(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Lt(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64ConvertSI32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Eq(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Le(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Ge(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32ShrU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32ConvertUI32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32ShrS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32GeU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Ceil(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32GeS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Shl(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Floor(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Xor(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Abs(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Mul(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Sub(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32ReinterpretF32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Add(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Sub(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Or(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64LtU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64LtS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64ConvertSI64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Xor(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64GeU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Mul(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Sub(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64PromoteF32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Add(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64GeS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64ExtendUI32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Ne(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64ReinterpretI64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Eq(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Eq(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Floor(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32ConvertSI32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64And(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Or(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Ctz(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Eqz(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Eqz(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64ReinterpretF64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64ConvertUI32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32ConvertUI64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64ConvertUI64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64ShrS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64ShrU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Sqrt(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Shl(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF32Gt(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32WrapI64(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Rotl(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Rotr(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32GtU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64ExtendSI32(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Extend8S(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32Extend16S(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Extend8S(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Extend16S(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Extend32S(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI32GtS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addF64Neg(ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64LeU(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64LeS(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addI64Add(ExpressionType, ExpressionType, ExpressionType&);
    [[nodiscard]] PartialResult addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&);

    // Control flow

    [[nodiscard]] ControlType addTopLevel(BlockSignature&&);
    [[nodiscard]] PartialResult addBlock(BlockSignature&&, Stack&, ControlType&, Stack&);
    [[nodiscard]] PartialResult addLoop(BlockSignature&&, Stack&, ControlType&, Stack&, uint32_t);
    [[nodiscard]] PartialResult addIf(ExpressionType, BlockSignature&&, Stack&, ControlType&, Stack&);
    [[nodiscard]] PartialResult addElse(ControlType&, Stack&);
    [[nodiscard]] PartialResult addElseToUnreachable(ControlType&);

    [[nodiscard]] PartialResult addTry(BlockSignature&&, Stack&, ControlType&, Stack&);
    [[nodiscard]] PartialResult addTryTable(BlockSignature&&, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack);
    [[nodiscard]] PartialResult addCatch(unsigned, const TypeDefinition&, Stack&, ControlType&, ResultList&);
    [[nodiscard]] PartialResult addCatchToUnreachable(unsigned, const TypeDefinition&, ControlType&, ResultList&);
    [[nodiscard]] PartialResult addCatchAll(Stack&, ControlType&);
    [[nodiscard]] PartialResult addCatchAllToUnreachable(ControlType&);
    [[nodiscard]] PartialResult addDelegate(ControlType&, ControlType&);
    [[nodiscard]] PartialResult addDelegateToUnreachable(ControlType&, ControlType&);
    [[nodiscard]] PartialResult addThrow(unsigned, ArgumentList&, Stack&);
    [[nodiscard]] PartialResult addRethrow(unsigned, ControlType&);
    [[nodiscard]] PartialResult addThrowRef(ExpressionType, Stack&);

    [[nodiscard]] PartialResult addReturn(const ControlType&, const Stack&);
    [[nodiscard]] PartialResult addBranch(ControlType&, ExpressionType, const Stack&);
    [[nodiscard]] PartialResult addBranchNull(ControlType&, ExpressionType, Stack&, bool, ExpressionType&);
    [[nodiscard]] PartialResult addBranchCast(ControlType&, ExpressionType, Stack&, bool, int32_t, bool);
    [[nodiscard]] PartialResult addSwitch(ExpressionType, const Vector<ControlType*>&, ControlType&, const Stack&);
    [[nodiscard]] PartialResult endBlock(ControlEntry&, Stack&);
    void endTryTable(const ControlType& data);
    [[nodiscard]] PartialResult addEndToUnreachable(ControlEntry&, Stack&);

    [[nodiscard]] PartialResult endTopLevel(const Stack&);

    // Fused comparison stubs (TODO: make use of these for better codegen)
    [[nodiscard]] PartialResult addFusedBranchCompare(OpType, ControlType&, ExpressionType, const Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    [[nodiscard]] PartialResult addFusedBranchCompare(OpType, ControlType&, ExpressionType, ExpressionType, const Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    [[nodiscard]] PartialResult addFusedIfCompare(OpType, ExpressionType, BlockSignature&&, Stack&, ControlType&, Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    [[nodiscard]] PartialResult addFusedIfCompare(OpType, ExpressionType, ExpressionType, BlockSignature&&, Stack&, ControlType&, Stack&) { RELEASE_ASSERT_NOT_REACHED(); }

    // Calls

    [[nodiscard]] PartialResult addCall(unsigned, FunctionSpaceIndex, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call);
    [[nodiscard]] PartialResult addCallIndirect(unsigned, unsigned, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call);
    [[nodiscard]] PartialResult addCallRef(unsigned, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call);
    [[nodiscard]] PartialResult addUnreachable();
    [[nodiscard]] PartialResult addCrash();

    void setParser(FunctionParser<IPIntGenerator>* parser) { m_parser = parser; };
    size_t getCurrentInstructionLength()
    {
        return m_parser->offset() - m_parser->currentOpcodeStartingOffset();
    }
    void addCallCommonData(const FunctionSignature&, const CallInformation&);
    void addTailCallCommonData(const FunctionSignature&, const CallInformation&);
    void didFinishParsingLocals()
    {
        m_metadata->m_bytecodeOffset = m_parser->offset();
    }
    void didPopValueFromStack(ExpressionType, ASCIILiteral) { }
    void willParseOpcode() { }
    void willParseExtendedOpcode() { }
    void didParseOpcode()
    {
        if (!m_parser->unreachableBlocks()) {
            ASSERT(m_parser->getStackHeightInValues() == m_stackSize.value());
            if (Options::enableWasmDebugger()) [[unlikely]] {
                if (m_debugInfo) {
                    OpType currentOpcode = m_parser->currentOpcode();
                    bool isControlFlowInstruction = Wasm::isControlFlowInstructionWithExtGC(currentOpcode, [this]() {
                        return m_parser->currentExtendedOpcode();
                    });
                    if (!isControlFlowInstruction || currentOpcode == AnnotatedSelect)
                        RECORD_NEXT_INSTRUCTION(curPC(), nextPC());
                }
            }
        }
    }

    void dump(const ControlStack&, const Stack*);

    void convertTryToCatch(ControlType& tryBlock, CatchKind);

    ALWAYS_INLINE void changeStackSize(int delta)
    {
        m_stackSize += delta;
        if (delta > 0)
            m_maxStackSize = std::max(m_maxStackSize, m_stackSize.value());
    }

    void coalesceControlFlow(bool force = false);
    void resolveEntryTarget(unsigned, IPIntLocation);
    void resolveExitTarget(unsigned, IPIntLocation);

    void tryToResolveEntryTarget(uint32_t index, IPIntLocation loc, uint8_t*)
    {
        m_controlStructuresAwaitingCoalescing[index].m_awaitingEntryTarget.append(loc);
    }

    void tryToResolveExitTarget(uint32_t index, IPIntLocation loc, uint8_t*)
    {
        m_controlStructuresAwaitingCoalescing[index].m_awaitingExitTarget.append(loc);
    }

    void tryToResolveBranchTarget(ControlType& targetBlock, IPIntLocation loc, uint8_t* metadata)
    {
        if (ControlType::isTopLevel(targetBlock)) {
            m_jumpLocationsAwaitingEnd.append(loc);
            return;
        }
        auto index = targetBlock.m_index;
        auto& target = m_controlStructuresAwaitingCoalescing[index];
        if (target.isLoop) {
            ASSERT(target.m_entryResolved);
            IPInt::BlockMetadata md = { static_cast<int32_t>(target.m_entryTarget.pc - loc.pc), static_cast<int32_t>(target.m_entryTarget.mc - loc.mc) };
            WRITE_TO_METADATA(metadata + loc.mc, md, IPInt::BlockMetadata);
            RECORD_NEXT_INSTRUCTION(loc.pc, target.m_entryTarget.pc);
        } else {
            ASSERT(!target.m_exitResolved);
            target.m_awaitingBranchTarget.append(loc);
        }
    }

    ALWAYS_INLINE const CallInformation& cachedCallInformationFor(const FunctionSignature& signature)
    {
        if (m_cachedSignature != &signature) {
            m_cachedSignature = &signature;
            m_cachedCallBytecode.shrink(0);
            m_cachedCallInformation = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
        }
        return m_cachedCallInformation;
    }

    static constexpr bool tierSupportsSIMD() { return true; }
    static constexpr bool validateFunctionBodySize = true;

private:
    Checked<uint32_t> m_stackSize { 0 };
    uint32_t m_maxStackSize { 0 };
    Checked<uint32_t> m_tryDepth { 0 };
    uint32_t m_maxTryDepth { 0 };
    FunctionParser<IPIntGenerator>* m_parser { nullptr };
    ModuleInformation& m_info;
    const FunctionCodeIndex m_functionIndex;
    std::unique_ptr<FunctionIPIntMetadataGenerator> m_metadata;

    struct ControlStructureAwaitingCoalescing {
        Vector<IPIntLocation, 16> m_awaitingEntryTarget { };
        Vector<IPIntLocation, 16> m_awaitingBranchTarget { };
        Vector<IPIntLocation, 16> m_awaitingExitTarget { };

        IPIntLocation m_entryTarget { 0, 0 }; // where do we go when entering normally?
        IPIntLocation m_exitTarget { 0, 0 }; // where do we go when leaving?

        uint32_t startPC { 0 };
        bool isLoop { false };
        bool m_entryResolved { false };
        bool m_exitResolved { false };
    };
    Vector<ControlStructureAwaitingCoalescing, 16> m_controlStructuresAwaitingCoalescing;

    struct QueuedCoalesceRequest {
        size_t index;
        bool isEntry;
    };
    Vector<QueuedCoalesceRequest, 16> m_coalesceQueue;

    // if this is 0, all our control structures have been coalesced and we can clean up the vector
    unsigned m_coalesceDebt { 0 };

    // exit loations can still be unresolved when the ControlType* dies, so we put them here
    Vector<IPIntLocation> m_exitHandlersAwaitingCoalescing;
    // all jumps that go to the top level and return
    Vector<IPIntLocation> m_jumpLocationsAwaitingEnd;

    inline uint32_t curPC() { return m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset; }
    inline uint32_t nextPC() { return m_parser->offset() - m_metadata->m_bytecodeOffset; }
    inline uint32_t curMC() { return m_metadata->m_metadata.size(); }

    CallInformation m_cachedCallInformation { };
    const FunctionSignature* m_cachedSignature { nullptr };
    Vector<uint8_t, 16> m_cachedCallBytecode;

    Checked<int32_t> m_argumentAndResultsStackSize;

    bool m_usesRethrow { false };
    bool m_usesSIMD { false };

    size_t m_functionStartByteOffset { 0 };
    FunctionDebugInfo* m_debugInfo { nullptr };
};

// use if (true) to avoid warnings.
#define IPINT_UNIMPLEMENTED { if (true) { CRASH(); } return { }; }

IPIntGenerator::IPIntGenerator(ModuleInformation& info, FunctionCodeIndex functionIndex, const TypeDefinition&, std::span<const uint8_t> bytecode, FunctionDebugInfo* debugInfo)
    : m_info(info)
    , m_functionIndex(functionIndex)
    , m_metadata(WTF::makeUnique<FunctionIPIntMetadataGenerator>(functionIndex, bytecode))
    , m_functionStartByteOffset(info.functions[functionIndex].start)
    , m_debugInfo(debugInfo)
{
}

[[nodiscard]] PartialResult IPIntGenerator::addDrop(ExpressionType)
{
    changeStackSize(-1);
    return { };
}

Value IPIntGenerator::addConstant(Type type, uint64_t value)
{
    changeStackSize(1);
    m_metadata->addLEB128ConstantAndLengthForType(type, value, getCurrentInstructionLength());
    return { };
}

// SIMD

[[nodiscard]] PartialResult IPIntGenerator::addSIMDLoad(ExpressionType, uint32_t offset, ExpressionType&)
{
    changeStackSize(0); // Pop address, push v128 value (net change = 0)
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDStore(ExpressionType, ExpressionType, uint32_t offset)
{
    changeStackSize(-2); // Pop address and v128 value
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDLoadSplat(SIMDLaneOperation, ExpressionType pointer, uint32_t offset, ExpressionType& result)
{
    return addSIMDLoad(pointer, offset, result);
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t offset, uint8_t, ExpressionType&)
{
    changeStackSize(-1);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t offset, uint8_t)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDLoadExtend(SIMDLaneOperation, ExpressionType pointer, uint32_t offset, ExpressionType& result)
{
    return addSIMDLoad(pointer, offset, result);
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDLoadPad(SIMDLaneOperation, ExpressionType pointer, uint32_t offset, ExpressionType& result)
{
    return addSIMDLoad(pointer, offset, result);
}

IPIntGenerator::ExpressionType IPIntGenerator::addConstant(v128_t)
{
    changeStackSize(1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addExtractLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-2); // 3 operands, 1 result
    return { };
}

#if ENABLE(B3_JIT)
[[nodiscard]] PartialResult IPIntGenerator::addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}
#endif

[[nodiscard]] PartialResult IPIntGenerator::addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1); // Pop two v128 values, push one v128 value
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-2); // Pop three v128 values, push one v128 value
    return { };
}

// References

[[nodiscard]] PartialResult IPIntGenerator::addRefIsNull(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addRefFunc(FunctionSpaceIndex index, ExpressionType&)
{
    changeStackSize(1);
    m_metadata->addLEB128ConstantInt32AndLength(index, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addRefAsNonNull(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addRefEq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Tables

[[nodiscard]] PartialResult IPIntGenerator::addTableGet(unsigned index, ExpressionType, ExpressionType&)
{
    m_metadata->addLEB128ConstantInt32AndLength(index, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addTableSet(unsigned index, ExpressionType, ExpressionType)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(index, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    IPInt::TableInitMetadata table {
        .elementIndex = safeCast<uint32_t>(elementIndex),
        .tableIndex = safeCast<uint32_t>(tableIndex),
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(table);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addElemDrop(unsigned elementIndex)
{
    m_metadata->addLEB128ConstantInt32AndLength(elementIndex, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addTableSize(unsigned tableIndex, ExpressionType&)
{
    changeStackSize(1);
    m_metadata->addLEB128ConstantInt32AndLength(tableIndex, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addTableGrow(unsigned tableIndex, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    IPInt::TableGrowMetadata table {
        .tableIndex = safeCast<uint32_t>(tableIndex),
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(table);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addTableFill(unsigned tableIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    IPInt::TableFillMetadata table {
        .tableIndex = safeCast<uint32_t>(tableIndex),
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(table);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    IPInt::TableCopyMetadata table {
        .dstTableIndex = safeCast<uint32_t>(dstTableIndex),
        .srcTableIndex = safeCast<uint32_t>(srcTableIndex),
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(table);
    return { };
}

// Locals and Globals

[[nodiscard]] PartialResult IPIntGenerator::addArguments(const TypeDefinition &signature)
{
    auto sig = signature.as<FunctionSignature>();
    const CallInformation callCC = wasmCallingConvention().callInformationFor(*sig, CallRole::Callee);

    ASSERT(callCC.headerAndArgumentStackSizeInBytes >= callCC.headerIncludingThisSizeInBytes);
    m_argumentAndResultsStackSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callCC.headerAndArgumentStackSizeInBytes) - callCC.headerIncludingThisSizeInBytes;
    ASSERT(!Options::useWasmIPInt() || !(m_argumentAndResultsStackSize % 16)); // mINT requires this

    auto numArgs = sig->argumentCount();
    m_metadata->m_numLocals += numArgs;
    m_metadata->m_numArguments = numArgs;

    m_metadata->m_argumINTBytecode.reserveInitialCapacity(sig->argumentCount() + 1);

    constexpr static int NUM_ARGUMINT_GPRS = 8;
    constexpr static int NUM_ARGUMINT_FPRS = 8;

    ASSERT_UNUSED(NUM_ARGUMINT_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_ARGUMINT_GPRS);
    ASSERT_UNUSED(NUM_ARGUMINT_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_ARGUMINT_FPRS);

    // 0x00 - 0x07: GPR 0-7
    // 0x08 - 0x0f: FPR 0-3
    // 0x10: stack
    // 0x11: end

    for (size_t i = 0; i < numArgs; ++i) {
        const ArgumentLocation& argLoc = callCC.params[i];
        const ValueLocation& loc = argLoc.location;

        if (loc.isGPR()) {
#if USE(JSVALUE64)
            ASSERT_UNUSED(NUM_ARGUMINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().gpr()) < NUM_ARGUMINT_GPRS);
            m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::ArgGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr()));
#elif USE(JSVALUE32_64)
            ASSERT_UNUSED(NUM_ARGUMINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_ARGUMINT_GPRS);
            ASSERT_UNUSED(NUM_ARGUMINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().tagGPR()) < NUM_ARGUMINT_GPRS);
            m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::ArgGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord)) / 2);
#endif
        } else if (loc.isFPR()) {
            ASSERT_UNUSED(NUM_ARGUMINT_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_ARGUMINT_FPRS);
            m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::ArgFPR) + FPRInfo::toArgumentIndex(loc.fpr()));
        } else {
            RELEASE_ASSERT(loc.isStack());
            switch (argLoc.width) {
            case Width::Width64:
                m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::Stack));
                break;
            case Width::Width128:
                m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::StackVector));
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED("No argumINT bytecode for result width");
            }
        }
    }
    m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::End));

#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]] {
        if (m_debugInfo) {
            auto* localTypes = &m_debugInfo->locals;
            for (size_t i = 0; i < numArgs; ++i)
                localTypes->append(sig->argumentType(i));
        }
    }
#endif

    m_metadata->addReturnData(*sig, callCC);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addLocal(Type localType, uint32_t count)
{
    // push 0x00 or 0xff (for bit hacks) to the metadata depending on if we have a primitive or a reference
    if (isRefType(localType)) {
        for (unsigned i = 0; i < count; ++i)
            m_metadata->m_argumINTBytecode.append(0xff);
    } else {
        for (unsigned i = 0; i < count; ++i)
            m_metadata->m_argumINTBytecode.append(0);
    }
    m_metadata->m_numLocals += count;

#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]] {
        if (m_debugInfo) {
            auto* localTypes = &m_debugInfo->locals;
            for (unsigned i = 0; i < count; ++i)
                localTypes->append(localType);
        }
    }
#endif

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::getLocal(uint32_t, ExpressionType&)
{
    // Local indices are usually very small, so we decode them on the fly
    // instead of generating metadata.
    changeStackSize(1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::setLocal(uint32_t, ExpressionType)
{
    // Local indices are usually very small, so we decode them on the fly
    // instead of generating metadata.
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::teeLocal(uint32_t, ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::getGlobal(uint32_t index, ExpressionType&)
{
    changeStackSize(1);
    const Wasm::GlobalInformation& global = m_info.globals[index];
    IPInt::GlobalMetadata mdGlobal {
        .index = index,
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) },
        .bindingMode = safeCast<uint8_t>(global.bindingMode),
        .isRef = safeCast<uint8_t>(isRefType(m_info.globals[index].type))
    };
    m_metadata->appendMetadata(mdGlobal);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::setGlobal(uint32_t index, ExpressionType)
{
    changeStackSize(-1);
    const Wasm::GlobalInformation& global = m_info.globals[index];
    IPInt::GlobalMetadata mdGlobal {
        .index = index,
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) },
        .bindingMode = safeCast<uint8_t>(global.bindingMode),
        .isRef = safeCast<uint8_t>(isRefType(m_info.globals[index].type))
    };
    m_metadata->appendMetadata(mdGlobal);
    return { };
}

// Loads and Stores

[[nodiscard]] PartialResult IPIntGenerator::load(LoadOpType, ExpressionType, ExpressionType&, uint64_t offset)
{
    if (m_info.memory.isMemory64())
        m_metadata->addLEB128ConstantInt64AndLength(offset, getCurrentInstructionLength());
    else
        m_metadata->addLEB128ConstantInt32AndLength(static_cast<uint32_t>(offset), getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::store(StoreOpType, ExpressionType, ExpressionType, uint64_t offset)
{
    changeStackSize(-2);
    if (m_info.memory.isMemory64())
        m_metadata->addLEB128ConstantInt64AndLength(offset, getCurrentInstructionLength());
    else
        m_metadata->addLEB128ConstantInt32AndLength(static_cast<uint32_t>(offset), getCurrentInstructionLength());
    return { };
}

// Memories

[[nodiscard]] PartialResult IPIntGenerator::addGrowMemory(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addCurrentMemory(ExpressionType&)
{
    changeStackSize(1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addMemoryFill(ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addMemoryCopy(ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addMemoryInit(unsigned dataIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    m_metadata->addLEB128ConstantInt32AndLength(dataIndex, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addDataDrop(unsigned dataIndex)
{
    m_metadata->addLEB128ConstantInt32AndLength(dataIndex, getCurrentInstructionLength());
    return { };
}

// Atomics

[[nodiscard]] PartialResult IPIntGenerator::atomicLoad(ExtAtomicOpType, Type, ExpressionType, ExpressionType&, uint32_t offset)
{
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::atomicStore(ExtAtomicOpType, Type, ExpressionType, ExpressionType, uint32_t offset)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-1);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::atomicWait(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::atomicNotify(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-1);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::atomicFence(ExtAtomicOpType, uint8_t)
{
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

// GC

[[nodiscard]] PartialResult IPIntGenerator::addRefI31(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI31GetS(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI31GetU(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayNew(uint32_t index, ExpressionType, ExpressionType, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::ArrayNewMetadata>({
        index,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayNewData(uint32_t index, uint32_t dataSegmentIndex, ExpressionType, ExpressionType, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::ArrayNewDataMetadata>({
        index,
        dataSegmentIndex,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayNewElem(uint32_t index, uint32_t elemSegmentIndex, ExpressionType, ExpressionType, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::ArrayNewElemMetadata>({
        index,
        elemSegmentIndex,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayNewFixed(uint32_t index, ArgumentList& args, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::ArrayNewFixedMetadata>({
        index,
        static_cast<uint32_t>(args.size()),
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-args.size() + 1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayNewDefault(uint32_t index, ExpressionType, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::ArrayNewMetadata>({
        index,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayGet(ExtGCOpType, uint32_t index, ExpressionType, ExpressionType, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::ArrayGetSetMetadata>({
        index,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArraySet(uint32_t index, ExpressionType, ExpressionType, ExpressionType)
{
    m_metadata->appendMetadata<IPInt::ArrayGetSetMetadata>({
        index,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-3);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayLen(ExpressionType, ExpressionType&)
{
    // no metadata
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayFill(uint32_t, ExpressionType, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-4);
    m_metadata->appendMetadata<IPInt::ArrayFillMetadata>({
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayCopy(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-5);
    m_metadata->appendMetadata<IPInt::ArrayCopyMetadata>({
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayInitElem(uint32_t, ExpressionType, ExpressionType, uint32_t elemSegmentIndex, ExpressionType, ExpressionType)
{
    changeStackSize(-4);
    m_metadata->appendMetadata<IPInt::ArrayInitDataMetadata>({
        elemSegmentIndex,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addArrayInitData(uint32_t, ExpressionType, ExpressionType, uint32_t dataSegmentIndex, ExpressionType, ExpressionType)
{
    changeStackSize(-4);
    m_metadata->appendMetadata<IPInt::ArrayInitDataMetadata>({
        dataSegmentIndex,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addStructNew(uint32_t index, ArgumentList&, ExpressionType&)
{
    const StructType& type = *m_info.typeSignatures[index]->expand().as<StructType>();
    m_metadata->appendMetadata<IPInt::StructNewMetadata>({
        index,
        static_cast<uint16_t>(type.fieldCount()),
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-type.fieldCount() + 1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addStructNewDefault(uint32_t index, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::StructNewDefaultMetadata>({
        index,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addStructGet(ExtGCOpType, ExpressionType, const StructType&, const RTT&, uint32_t fieldIndex, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::StructGetSetMetadata>({
        fieldIndex,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addStructSet(ExpressionType, const StructType&, const RTT&, uint32_t fieldIndex, ExpressionType)
{
    m_metadata->appendMetadata<IPInt::StructGetSetMetadata>({
        fieldIndex,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    changeStackSize(-2);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addRefTest(ExpressionType, bool, int32_t heapType, bool, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::RefTestCastMetadata>({
        heapType,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addRefCast(ExpressionType, bool, int32_t heapType, ExpressionType&)
{
    m_metadata->appendMetadata<IPInt::RefTestCastMetadata>({
        heapType,
        static_cast<uint8_t>(getCurrentInstructionLength())
    });
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addAnyConvertExtern(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addExternConvertAny(ExpressionType, ExpressionType&)
{
    return { };
}

// Integer Arithmetic

[[nodiscard]] PartialResult IPIntGenerator::addI32Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32DivS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32DivU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64DivS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64DivU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32RemS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32RemU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64RemS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64RemU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Bitwise Operations

[[nodiscard]] PartialResult IPIntGenerator::addI32And(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64And(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Xor(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Xor(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Or(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Or(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Shl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32ShrU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32ShrS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Shl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64ShrU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64ShrS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Rotl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Rotl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Rotr(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Rotr(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Popcnt(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Popcnt(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Clz(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Clz(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Ctz(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Ctz(ExpressionType, ExpressionType&)
{
    return { };
}

// Floating-Point Arithmetic

[[nodiscard]] PartialResult IPIntGenerator::addF32Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Div(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Div(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Other Floating-Point Instructions

[[nodiscard]] PartialResult IPIntGenerator::addF32Min(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Max(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Min(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Max(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Nearest(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Nearest(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Floor(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Floor(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Ceil(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Ceil(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Copysign(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Copysign(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Sqrt(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Sqrt(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Neg(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Neg(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Abs(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Abs(ExpressionType, ExpressionType&)
{
    return { };
}

// Integer Comparisons

[[nodiscard]] PartialResult IPIntGenerator::addI32Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32LtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32LtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32LeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32LeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32GtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32GtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32GeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32GeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Eqz(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64GtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64GtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64GeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64GeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64LtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64LtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64LeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64LeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Eqz(ExpressionType, ExpressionType&)
{
    return { };
}

// Floating-Point Comparisons

[[nodiscard]] PartialResult IPIntGenerator::addF32Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Lt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Le(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Gt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Ge(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Lt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Le(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Gt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64Ge(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Integer Extension

[[nodiscard]] PartialResult IPIntGenerator::addI64ExtendSI32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64ExtendUI32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Extend8S(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32Extend16S(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Extend8S(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Extend16S(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64Extend32S(ExpressionType, ExpressionType&)
{
    return { };
}

// Truncation

[[nodiscard]] PartialResult IPIntGenerator::addF64Trunc(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32Trunc(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32TruncSF64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32TruncSF32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32TruncUF64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32TruncUF32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64TruncSF64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64TruncSF32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64TruncUF64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64TruncUF32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::truncSaturated(Ext1OpType, ExpressionType, ExpressionType&, Type, Type)
{
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

// Conversions

[[nodiscard]] PartialResult IPIntGenerator::addI32WrapI64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32DemoteF64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64PromoteF32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32ReinterpretI32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI32ReinterpretF32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64ReinterpretI64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addI64ReinterpretF64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32ConvertSI32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32ConvertUI32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32ConvertSI64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF32ConvertUI64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64ConvertSI32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64ConvertUI32(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64ConvertSI64(ExpressionType, ExpressionType&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addF64ConvertUI64(ExpressionType, ExpressionType&)
{
    return { };
}

// Control Flow Blocks

void IPIntGenerator::coalesceControlFlow(bool force)
{
    // Peek at the next opcode

    IPIntLocation here = { nextPC(), curMC() };
    if (!force) {
        if (m_parser->offset() >= m_parser->source().size())
            return;
        uint8_t nextOpcode = m_parser->source()[m_parser->offset()];
        if (nextOpcode == Block || nextOpcode == End)
            return;
    } else
        here = { curPC(), curMC() };

    // There's something useful after us. Resolve everything here.
    for (auto& entry : m_coalesceQueue) {
        if (entry.isEntry)
            resolveEntryTarget(entry.index, here);
        else
            resolveExitTarget(entry.index, here);
    }
    m_coalesceQueue.shrink(0);

    if (!m_coalesceDebt)
        m_controlStructuresAwaitingCoalescing.shrink(0);

    for (auto& src : m_exitHandlersAwaitingCoalescing) {
        IPInt::BlockMetadata md = { static_cast<int32_t>(here.pc - src.pc), static_cast<int32_t>(here.mc - src.mc) };
        WRITE_TO_METADATA(m_metadata->m_metadata.mutableSpan().data() + src.mc, md, IPInt::BlockMetadata);
        RECORD_NEXT_INSTRUCTION(src.pc, here.pc);
    }
    m_exitHandlersAwaitingCoalescing.shrink(0);
}

void IPIntGenerator::resolveEntryTarget(unsigned index, IPIntLocation loc)
{
    auto& control = m_controlStructuresAwaitingCoalescing[index];
    ASSERT(!control.m_entryResolved);
    for (auto& src : control.m_awaitingEntryTarget) {
        // write delta PC and delta MC
        IPInt::BlockMetadata md = { static_cast<int32_t>(loc.pc - src.pc), static_cast<int32_t>(loc.mc - src.mc) };
        WRITE_TO_METADATA(m_metadata->m_metadata.mutableSpan().data() + src.mc, md, IPInt::BlockMetadata);
        RECORD_NEXT_INSTRUCTION(src.pc, loc.pc); // FIXME: coalescing sequential blocks - should update instead of adding
    }
    if (control.isLoop) {
        for (auto& src : control.m_awaitingBranchTarget) {
            IPInt::BlockMetadata md = { static_cast<int32_t>(loc.pc - src.pc), static_cast<int32_t>(loc.mc - src.mc) };
            WRITE_TO_METADATA(m_metadata->m_metadata.mutableSpan().data() + src.mc, md, IPInt::BlockMetadata);
            RECORD_NEXT_INSTRUCTION(src.pc, loc.pc);
        }
        control.m_awaitingBranchTarget.clear();
    }
    control.m_awaitingEntryTarget.clear();
    control.m_entryResolved = true;
    control.m_entryTarget = loc;
}

void IPIntGenerator::resolveExitTarget(unsigned index, IPIntLocation loc)
{
    auto& control = m_controlStructuresAwaitingCoalescing[index];
    ASSERT(!control.m_exitResolved);
    for (auto& src : control.m_awaitingExitTarget) {
        // write delta PC and delta MC
        IPInt::BlockMetadata md = { static_cast<int32_t>(loc.pc - src.pc), static_cast<int32_t>(loc.mc - src.mc) };
        WRITE_TO_METADATA(m_metadata->m_metadata.mutableSpan().data() + src.mc, md, IPInt::BlockMetadata);
        RECORD_NEXT_INSTRUCTION(src.pc, loc.pc);
    }
    if (!control.isLoop) {
        for (auto& src : control.m_awaitingBranchTarget) {
            IPInt::BlockMetadata md = { static_cast<int32_t>(loc.pc - src.pc), static_cast<int32_t>(loc.mc - src.mc) };
            WRITE_TO_METADATA(m_metadata->m_metadata.mutableSpan().data() + src.mc, md, IPInt::BlockMetadata);
            RECORD_NEXT_INSTRUCTION(src.pc, loc.pc);
        }
        control.m_awaitingBranchTarget.clear();
    }
    control.m_awaitingExitTarget.clear();
    control.m_exitResolved = true;
    control.m_exitTarget = loc;
}

[[nodiscard]] IPIntGenerator::ControlType IPIntGenerator::addTopLevel(BlockSignature&& signature)
{
    ControlType topLevel = ControlType(WTF::move(signature), 0, BlockType::TopLevel);
    return topLevel;
}

[[nodiscard]] PartialResult IPIntGenerator::addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-2);
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addBlock(BlockSignature&& signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    splitStack(signature, oldStack, newStack);
    block = ControlType(WTF::move(signature), m_stackSize.value() - newStack.size(), BlockType::Block);
    block.m_index = m_controlStructuresAwaitingCoalescing.size();
    block.m_pc = curPC();
    block.m_mc = curMC();
    block.m_pendingOffset = curMC();
    RECORD_NEXT_INSTRUCTION(block.m_pc, nextPC());

    // Register to be coalesced if possible!
    m_coalesceQueue.append(QueuedCoalesceRequest { m_controlStructuresAwaitingCoalescing.size(), true });
    m_controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = block.m_pc,
        .isLoop = false
    });
    ++m_coalesceDebt;

    IPIntLocation here = { curPC(), curMC() };
    m_metadata->addBlankSpace<IPInt::BlockMetadata>();
    tryToResolveEntryTarget(block.m_index, here, m_metadata->m_metadata.mutableSpan().data());

    coalesceControlFlow();

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addLoop(BlockSignature&& signature, Stack& oldStack, ControlType& block, Stack& newStack, uint32_t loopIndex)
{
    splitStack(signature, oldStack, newStack);
    block = ControlType(WTF::move(signature), m_stackSize.value() - newStack.size(), BlockType::Loop);
    block.m_index = m_controlStructuresAwaitingCoalescing.size();
    block.m_pendingOffset = -1; // no need to update!
    block.m_pc = curPC();
    RECORD_NEXT_INSTRUCTION(block.m_pc, nextPC());

    // Register to be coalesced if possible!
    m_controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .m_entryTarget = { curPC(), curMC() },
        .startPC = block.m_pc,
        .isLoop = true,
        .m_entryResolved = true,
    });
    ++m_coalesceDebt;

    IPInt::InstructionLengthMetadata md { static_cast<uint8_t>(getCurrentInstructionLength()) };
    m_metadata->appendMetadata(md);

    // Loop OSR
    ASSERT(m_parser->getStackHeightInValues() + newStack.size() == m_stackSize.value());
    unsigned numOSREntryDataValues = m_stackSize.value();

    // Note the +1: we do this to avoid having 0 as a key in the map, since the current map can't handle 0 as a key
    m_metadata->tierUpCounter().add(m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset + 1, IPIntTierUpCounter::OSREntryData { loopIndex, numOSREntryDataValues, m_tryDepth });

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addIf(ExpressionType, BlockSignature&& signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    splitStack(signature, oldStack, newStack);
    changeStackSize(-1);
    block = ControlType(WTF::move(signature), m_stackSize.value() - newStack.size(), BlockType::If);
    block.m_index = m_controlStructuresAwaitingCoalescing.size();
    block.m_pc = curPC();
    block.m_mc = curMC();
    block.m_pendingOffset = m_metadata->m_metadata.size();
    RECORD_NEXT_INSTRUCTION(block.m_pc, nextPC());

    m_coalesceQueue.append(QueuedCoalesceRequest { m_controlStructuresAwaitingCoalescing.size(), true });
    m_controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = block.m_pc,
        .isLoop = false
    });
    ++m_coalesceDebt;

    IPInt::IfMetadata mdIf {
        .elseDeltaPC = 0xbeef,
        .elseDeltaMC = 0xbeef,
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(mdIf);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addElse(ControlType& block, Stack&)
{
    return addElseToUnreachable(block);
}

[[nodiscard]] PartialResult IPIntGenerator::addElseToUnreachable(ControlType& block)
{
    auto blockSignature = block.signature();
    m_stackSize = block.stackSize();
    changeStackSize(blockSignature.argumentCount());
    auto ifIndex = block.m_index;

    auto mdIf = reinterpret_cast<IPInt::IfMetadata*>(m_metadata->m_metadata.mutableSpan().data() + block.m_pendingOffset);

    // delta PC
    mdIf->elseDeltaPC = nextPC() - block.m_pc;
    RECORD_NEXT_INSTRUCTION(block.m_pc, block.m_pc + mdIf->elseDeltaPC);

    // delta MC
    if (m_parser->currentOpcode() == OpType::End) {
        // Edge case: if ... end with no else
        mdIf->elseDeltaMC = curMC() - block.m_mc;
        block = ControlType(WTF::move(blockSignature), block.stackSize(), BlockType::Else);
        block.m_index = ifIndex;
        block.m_pendingOffset = -1;
        return { };
    }

    // New MC, normal case
    mdIf->elseDeltaMC = safeCast<uint32_t>(curMC() + sizeof(IPInt::BlockMetadata)) - block.m_mc;
    block = ControlType(WTF::move(blockSignature), block.stackSize(), BlockType::Else);
    block.m_index = ifIndex;
    block.m_pc = curPC();
    block.m_mc = curMC();
    block.m_pendingOffset = curMC();

    m_metadata->addBlankSpace<IPInt::BlockMetadata>();
    return { };
}

// Exception Handling

[[nodiscard]] PartialResult IPIntGenerator::addTry(BlockSignature&& signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    m_tryDepth++;
    m_maxTryDepth = std::max(m_maxTryDepth, m_tryDepth.value());

    splitStack(signature, oldStack, newStack);
    block = ControlType(WTF::move(signature), m_stackSize.value() - newStack.size(), BlockType::Try);
    block.m_index = m_controlStructuresAwaitingCoalescing.size();
    block.m_tryDepth = m_tryDepth;
    block.m_pc = curPC();
    block.m_mc = curMC();
    RECORD_NEXT_INSTRUCTION(block.m_pc, nextPC());

    m_coalesceQueue.append(QueuedCoalesceRequest { m_controlStructuresAwaitingCoalescing.size(), true });
    m_controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = block.m_pc,
        .isLoop = false
    });
    ++m_coalesceDebt;

    // FIXME: Should this participate the same skipping that block does?
    // The upside is that we skip a bunch of sequential try/block instructions.
    // The downside is that try needs more metadata.
    // It's not clear that code would want to have many nested try blocks
    // though.
    m_metadata->addLength(getCurrentInstructionLength());

    coalesceControlFlow();
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addTryTable(BlockSignature&& signature, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack)
{
    splitStack(signature, enclosingStack, newStack);
    result = ControlType(WTF::move(signature), m_stackSize.value() - newStack.size(), BlockType::TryTable);
    result.m_tryTableTargets.reserveInitialCapacity(targets.size());
    result.m_index = m_controlStructuresAwaitingCoalescing.size();
    result.m_pc = curPC();
    result.m_mc = curMC();
    result.m_pendingOffset = curMC();
    RECORD_NEXT_INSTRUCTION(result.m_pc, nextPC());

    m_coalesceQueue.append(QueuedCoalesceRequest { m_controlStructuresAwaitingCoalescing.size(), true });
    m_controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = result.m_pc,
        .isLoop = false
    });
    ++m_coalesceDebt;

    IPIntLocation here = { curPC(), curMC() };
    m_metadata->addBlankSpace<IPInt::BlockMetadata>();
    tryToResolveEntryTarget(result.m_index, here, m_metadata->m_metadata.mutableSpan().data());

    result.m_tryTableTargets.appendUsingFunctor(targets.size(),
        [&](unsigned i) -> ControlType::TryTableTarget {
            auto& target = targets[i];
            return {
                target.type,
                target.tag,
                target.exceptionSignature,
                target.target
            };
        }
    );

    // append all the branch data first
    for (auto& target : targets) {
        auto entry = m_parser->resolveControlRef(target.target).controlData;
        // stack size at destination is (locals) + (everything below target) + (things we push)
        m_metadata->appendMetadata<IPInt::CatchMetadata>({
            static_cast<uint32_t>(entry.stackSize() + entry.branchTargetArity() + roundUpToMultipleOf<2>(m_metadata->m_numLocals))
        });

        IPIntLocation here = { curPC(), curMC() };
        m_metadata->appendMetadata<IPInt::BlockMetadata>({
            .deltaPC = 0xbeef, .deltaMC = 0xbeef
        });

        tryToResolveBranchTarget(entry, here, m_metadata->m_metadata.mutableSpan().data());
    }

    coalesceControlFlow();
    return { };
}

void IPIntGenerator::convertTryToCatch(ControlType& tryBlock, CatchKind catchKind)
{
    ASSERT(ControlType::isTry(tryBlock));
    ControlType catchBlock = ControlType(BlockSignature { tryBlock.signature() }, tryBlock.stackSize(), BlockType::Catch, catchKind);
    catchBlock.m_pc = tryBlock.m_pc;
    catchBlock.m_pcEnd = m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset;
    catchBlock.m_tryDepth = tryBlock.m_tryDepth;

    catchBlock.m_index = tryBlock.m_index;
    catchBlock.m_mc = tryBlock.m_mc;

    tryBlock = WTF::move(catchBlock);
}

[[nodiscard]] PartialResult IPIntGenerator::addCatch(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, Stack&, ControlType& block, ResultList& results)
{

    return addCatchToUnreachable(exceptionIndex, exceptionSignature, block, results);
}

[[nodiscard]] PartialResult IPIntGenerator::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, ControlType& block, ResultList& results)
{
    if (ControlType::isTry(block))
        convertTryToCatch(block, CatchKind::Catch);

    const FunctionSignature& signature = *exceptionSignature.as<FunctionSignature>();
    for (unsigned i = 0; i < signature.argumentCount(); i++)
        results.append(Value { });

    ASSERT(block.stackSize() == m_parser->getControlEntryStackHeightInValues());
    m_stackSize = block.stackSize();
    changeStackSize(signature.argumentCount());

    // FIXME: If this is actually unreachable we shouldn't need metadata.
    block.m_catchesAwaitingFixup.append({ curPC(), curMC() });
    m_metadata->addBlankSpace<IPInt::BlockMetadata>();

    m_metadata->m_exceptionHandlers.append({
        HandlerType::Catch,
        static_cast<uint32_t>(block.m_pc),
        static_cast<uint32_t>(block.m_pcEnd + 1), // + 1 since m_pcEnd is the PC of the catch bytecode, which should be included in the range
        static_cast<uint32_t>(m_parser->offset() - m_metadata->m_bytecodeOffset),
        static_cast<uint32_t>(m_metadata->m_metadata.size()),
        m_tryDepth,
        exceptionIndex
    });

    uint32_t stackSizeInV128 = m_stackSize.value() + roundUpToMultipleOf<2>(m_metadata->m_numLocals);
    IPInt::CatchMetadata mdCatch {
        .stackSizeInV128 = stackSizeInV128
    };
    m_metadata->appendMetadata(mdCatch);

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addCatchAll(Stack&, ControlType& block)
{
    return addCatchAllToUnreachable(block);
}

[[nodiscard]] PartialResult IPIntGenerator::addCatchAllToUnreachable(ControlType& block)
{
    UNUSED_PARAM(block);
    if (ControlType::isTry(block))
        convertTryToCatch(block, CatchKind::CatchAll);
    else
        block.m_catchKind = CatchKind::CatchAll;

    ASSERT(block.stackSize() == m_parser->getControlEntryStackHeightInValues());
    m_stackSize = block.stackSize();

    // FIXME: If this is actually unreachable we shouldn't need metadata.
    block.m_catchesAwaitingFixup.append({ curPC(), curMC() });
    m_metadata->addBlankSpace(sizeof(IPInt::BlockMetadata));

    m_metadata->m_exceptionHandlers.append({
        HandlerType::CatchAll,
        static_cast<uint32_t>(block.m_pc),
        static_cast<uint32_t>(block.m_pcEnd + 1), // + 1 since m_pcEnd is the PC of the catch bytecode, which should be included in the range
        static_cast<uint32_t>(m_parser->offset() - m_metadata->m_bytecodeOffset),
        static_cast<uint32_t>(m_metadata->m_metadata.size()),
        m_tryDepth,
        0
    });

    uint32_t stackSizeInV128 = m_stackSize.value() + roundUpToMultipleOf<2>(m_metadata->m_numLocals);
    IPInt::CatchMetadata mdCatch {
        .stackSizeInV128 = stackSizeInV128
    };
    m_metadata->appendMetadata(mdCatch);

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addDelegate(ControlType& target, ControlType& data)
{
    return addDelegateToUnreachable(target, data);
}

[[nodiscard]] PartialResult IPIntGenerator::addDelegateToUnreachable(ControlType& target, ControlType& data)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(data);

    data.m_pcEnd = curPC();

    // FIXME: If this is actually unreachable we shouldn't need metadata.
    data.m_catchesAwaitingFixup.append({ curPC(), curMC() });
    m_metadata->addBlankSpace<IPInt::BlockMetadata>();

    ASSERT(ControlType::isTry(target) || ControlType::isTopLevel(target));
    unsigned targetDepth = ControlType::isTry(target) ? target.m_tryDepth : 0;

    m_metadata->m_exceptionHandlers.append({
        HandlerType::Delegate,
        static_cast<uint32_t>(data.m_pc),
        static_cast<uint32_t>(data.m_pcEnd + 1), // + 1 since m_pcEnd is the PC of the delegate bytecode, which should be included in the range
        static_cast<uint32_t>(curPC()),
        static_cast<uint32_t>(curMC()),
        m_tryDepth,
        targetDepth
    });

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addThrow(unsigned exceptionIndex, ArgumentList&, Stack&)
{
    IPInt::ThrowMetadata mdThrow {
        .exceptionIndex = safeCast<uint32_t>(exceptionIndex)
    };
    m_metadata->appendMetadata(mdThrow);

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addRethrow(unsigned, ControlType& catchBlock)
{
    m_usesRethrow = true;

    IPInt::RethrowMetadata mdRethrow {
        .tryDepth = catchBlock.m_tryDepth
    };
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(sizeof(mdRethrow));
    WRITE_TO_METADATA(m_metadata->m_metadata.mutableSpan().data() + size, mdRethrow, IPInt::RethrowMetadata);

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addThrowRef(ExpressionType, Stack&)
{
    changeStackSize(-1);
    return { };
}

// Control Flow Branches

[[nodiscard]] PartialResult IPIntGenerator::addReturn(const ControlType&, const Stack&)
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addBranch(ControlType& block, ExpressionType, const Stack&)
{
    bool isBrIf = (m_parser->currentOpcode() == OpType::BrIf);
    if (isBrIf)
        changeStackSize(-1);

    IPIntLocation here = { curPC(), curMC() };
    if (isBrIf)
        RECORD_NEXT_INSTRUCTION(here.pc, nextPC());

    IPInt::BranchMetadata branch {
        .target = {
            .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
            .toPop = safeCast<uint16_t>(m_stackSize - block.stackSize() - block.branchTargetArity()),
            .toKeep = safeCast<uint16_t>(block.branchTargetArity()),
        },
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(branch);

    tryToResolveBranchTarget(block, here, m_metadata->m_metadata.mutableSpan().data());

    return { };
}
[[nodiscard]] PartialResult IPIntGenerator::addBranchNull(ControlType& block, ExpressionType, Stack&, bool shouldNegate, ExpressionType&)
{
    // We don't need shouldNegate in the metadata since it's in the opcode

    IPIntLocation here = { curPC(), curMC() };
    RECORD_NEXT_INSTRUCTION(here.pc, nextPC());

    unsigned toPop = m_stackSize - block.stackSize() - block.branchTargetArity();

    // if we branch_on_null, we'll pop the null first
    if (!shouldNegate)
        toPop -= 1;

    IPInt::BranchMetadata branch {
        .target = {
            .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
            .toPop = safeCast<uint16_t>(toPop),
            .toKeep = safeCast<uint16_t>(block.branchTargetArity()),
        },
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(branch);

    tryToResolveBranchTarget(block, here, m_metadata->m_metadata.mutableSpan().data());

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addBranchCast(ControlType& block, ExpressionType, Stack&, bool, int32_t heapType, bool)
{
    m_metadata->appendMetadata<IPInt::RefTestCastMetadata>({
        heapType,
        0
    });

    IPIntLocation here = { curPC(), curMC() };
    RECORD_NEXT_INSTRUCTION(here.pc, nextPC());

    m_metadata->appendMetadata<IPInt::BranchMetadata>({
        .target = {
            .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
            .toPop = safeCast<uint16_t>(m_stackSize - block.stackSize() - block.branchTargetArity()),
            .toKeep = safeCast<uint16_t>(block.branchTargetArity()),
        },
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    });

    tryToResolveBranchTarget(block, here, m_metadata->m_metadata.mutableSpan().data());
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addSwitch(ExpressionType, const Vector<ControlType*>& jumps, ControlType& defaultJump, const Stack&)
{
    changeStackSize(-1);
    IPInt::SwitchMetadata mdSwitch {
        .size = safeCast<uint32_t>(jumps.size() + 1),
        .target = { }
    };
    m_metadata->appendMetadata(mdSwitch);

    for (auto* block : jumps) {
        IPInt::BranchTargetMetadata target {
            .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
            .toPop = safeCast<uint16_t>(m_stackSize - block->stackSize() - block->branchTargetArity()),
            .toKeep = safeCast<uint16_t>(block->branchTargetArity())
        };
        IPIntLocation here = { curPC(), curMC() };
        m_metadata->appendMetadata(target);
        tryToResolveBranchTarget(*block, here, m_metadata->m_metadata.mutableSpan().data());
    }
    IPInt::BranchTargetMetadata defaultTarget {
        .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
        .toPop = safeCast<uint16_t>(m_stackSize - defaultJump.stackSize() - defaultJump.branchTargetArity()),
        .toKeep = safeCast<uint16_t>(defaultJump.branchTargetArity())
    };
    IPIntLocation here = { curPC(), curMC() };
    m_metadata->appendMetadata(defaultTarget);
    tryToResolveBranchTarget(defaultJump, here, m_metadata->m_metadata.mutableSpan().data());

    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::endBlock(ControlEntry& entry, Stack& stack)
{
    return addEndToUnreachable(entry, stack);
}

void IPIntGenerator::endTryTable(const ControlType& data)
{
    auto targets = data.m_tryTableTargets;

    unsigned i = 0;
    for (auto& target : targets) {
        HandlerType targetType;
        switch (target.type) {
        case CatchKind::Catch:
            targetType = HandlerType::TryTableCatch;
            break;
        case CatchKind::CatchRef:
            targetType = HandlerType::TryTableCatchRef;
            break;
        case CatchKind::CatchAll:
            targetType = HandlerType::TryTableCatchAll;
            break;
        case CatchKind::CatchAllRef:
            targetType = HandlerType::TryTableCatchAllRef;
            break;
        }
        auto entry = m_parser->resolveControlRef(target.target).controlData;
        m_metadata->m_exceptionHandlers.append({
            targetType,
            data.m_pc,
            curPC() + 1, // + 1 since the end bytecode should be included

            // index into the array of try_table targets
            data.m_pc, // PC will be fixed up relative to the try_table's PC
            static_cast<unsigned>(data.m_mc
                + sizeof(IPInt::BlockMetadata)
                + i * (sizeof(IPInt::CatchMetadata) + sizeof(IPInt::BlockMetadata))),
            m_tryDepth,
            target.tag
        });
        ++i;
    }
}

[[nodiscard]] PartialResult IPIntGenerator::addEndToUnreachable(ControlEntry& entry, Stack&)
{
    const auto& block = entry.controlData;
    for (unsigned i = 0; i < block.signature().returnCount(); i ++)
        entry.enclosedExpressionStack.constructAndAppend(block.signature().returnType(i), Value { });
    m_stackSize = block.stackSize();
    changeStackSize(block.signature().returnCount());

    if (ControlType::isTry(block) || ControlType::isAnyCatch(block)) {
        --m_tryDepth;
        m_exitHandlersAwaitingCoalescing.appendVector(block.m_catchesAwaitingFixup);
    }

    if (ControlType::isTryTable(block))
        endTryTable(block);

    if (ControlType::isTopLevel(block)) {
        // Hit the end
        m_exitHandlersAwaitingCoalescing.appendVector(m_jumpLocationsAwaitingEnd);
        coalesceControlFlow(true);

        // Metadata = round up 8 bytes, one for each
        m_metadata->m_bytecode = m_metadata->m_bytecode.first(m_parser->offset());
        return { };
    }

    if (ControlType::isIf(block)) {
        m_exitHandlersAwaitingCoalescing.append({ block.m_pc, block.m_mc });
    } else if (ControlType::isElse(block)) {
        // if it's not an if ... end, coalesce
        if (block.m_pendingOffset != -1)
            m_exitHandlersAwaitingCoalescing.append({ block.m_pc, block.m_mc });
        m_coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
        --m_coalesceDebt;
    } else if (ControlType::isBlock(block)) {
        m_coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
        --m_coalesceDebt;
    } else if (ControlType::isLoop(block)) {
        m_coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
        --m_coalesceDebt;
    } else if (ControlType::isTryTable(block)) {
        m_coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
        --m_coalesceDebt;
    } else if (ControlType::isTry(block) || ControlType::isAnyCatch(block)) {
        m_coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
        --m_coalesceDebt;
    }

    // mark pending exit targets to be resolved
    // any pending branch targets must be blocks because a loop would've been resolved. if it's loop, end then there's nobody
    // asking for the target

    coalesceControlFlow();
    return { };
}

auto IPIntGenerator::endTopLevel(const Stack&) -> PartialResult
{
    bool isNotDebugMode = !m_debugInfo;
    if (m_usesSIMD && isNotDebugMode)
        m_info.markUsesSIMD(m_metadata->functionIndex());
    if (isNotDebugMode)
        m_info.doneSeeingFunction(m_metadata->m_functionIndex);
    return { };
}

// Calls

// Appends the bytecode to setup the arguments and perform a call / tail-call. Note that the resulting bytecode is backwards.
template<bool isTailCall>
static void addCallArgumentBytecode(Vector<uint8_t, 16>& results, const CallInformation& callConvention)
{
    constexpr static int NUM_MINT_CALL_GPRS = 8;
    constexpr static int NUM_MINT_CALL_FPRS = 8;
    ASSERT_UNUSED(NUM_MINT_CALL_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_MINT_CALL_GPRS);
    ASSERT_UNUSED(NUM_MINT_CALL_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_MINT_CALL_FPRS);

    // Translate Call bytecodes to TailCall bytecodes when isTailCall.
    auto toBytecodeUint8 = [](IPInt::CallArgumentBytecode bytecode) {
        constexpr uint8_t tailBytecodeOffset = static_cast<uint8_t>(IPInt::CallArgumentBytecode::TailCallArgDecSP) - static_cast<uint8_t>(IPInt::CallArgumentBytecode::CallArgDecSP);
        uint8_t bytecodeUint8 = static_cast<uint8_t>(bytecode);
        ASSERT(static_cast<uint8_t>(IPInt::CallArgumentBytecode::CallArgDecSP) <= bytecodeUint8
            && bytecodeUint8 <= static_cast<uint8_t>(IPInt::CallArgumentBytecode::CallArgDecSPStoreVector8));

        if constexpr (isTailCall)
            bytecodeUint8 += tailBytecodeOffset;
        return bytecodeUint8;
    };

    results.append(static_cast<uint8_t>(isTailCall ? IPInt::CallArgumentBytecode::TailCall : IPInt::CallArgumentBytecode::Call));

    intptr_t spOffset = callConvention.headerIncludingThisSizeInBytes;

    auto isAligned16 = [&spOffset]() ALWAYS_INLINE_LAMBDA {
        return !(spOffset & 0xf);
    };

    ASSERT(isAligned16());
    results.appendUsingFunctor(callConvention.params.size(),
        [&](unsigned index) -> uint8_t {
            const ArgumentLocation& argLoc = callConvention.params[index];
            const ValueLocation& loc = argLoc.location;

            if (loc.isGPR()) {
#if USE(JSVALUE64)
                ASSERT_UNUSED(NUM_MINT_CALL_GPRS, GPRInfo::toArgumentIndex(loc.jsr().gpr()) < NUM_MINT_CALL_GPRS);
                return static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr());
#elif USE(JSVALUE32_64)
                ASSERT_UNUSED(NUM_MINT_CALL_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_MINT_CALL_GPRS);
                ASSERT_UNUSED(NUM_MINT_CALL_GPRS, GPRInfo::toArgumentIndex(loc.jsr().tagGPR()) < NUM_MINT_CALL_GPRS);
                return static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord));
#endif
            }

            if (loc.isFPR()) {
                ASSERT_UNUSED(NUM_MINT_CALL_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_MINT_CALL_FPRS);
                return static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentFPR) + FPRInfo::toArgumentIndex(loc.fpr());
            }
            RELEASE_ASSERT(loc.isStackArgument());
            // mINT bytecode handlers assume this; if it fails, mINT needs updating
            ASSERT(loc.offsetFromSP() == spOffset);
            IPInt::CallArgumentBytecode bytecode;
            switch (argLoc.width) {
            case Width64:
                bytecode = isAligned16() ? IPInt::CallArgumentBytecode::CallArgStore0 : IPInt::CallArgumentBytecode::CallArgDecSPStore8;
                spOffset += 8; // These bytecodes store 8-bytes
                break;
            case Width128:
                bytecode = isAligned16() ? IPInt::CallArgumentBytecode::CallArgDecSPStoreVector0 : IPInt::CallArgumentBytecode::CallArgDecSPStoreVector8;
                spOffset += 16; // These bytecodes store 16-bytes
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED("No bytecode for stack argument location width");
            }
            return toBytecodeUint8(bytecode);
        });

    if (!isAligned16()) {
        // In this case, the final argument ended up unaligned w.r.t. 16-byte stack alignment,
        // so this allocates that top pair of stack slots. The lower 8-bytes have already been
        // counted by spOffset.
        spOffset += 8;
        results.append(toBytecodeUint8(IPInt::CallArgumentBytecode::CallArgDecSP));
    }
    intptr_t frameSize = roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes);
    ASSERT(frameSize >= spOffset);

    ASSERT(isAligned16());
    // Pad out the argument / result stack space not occupied by the pushed arguments
    for (; spOffset < frameSize; spOffset += 16) // This bytecode pads by 16-bytes
        results.append(toBytecodeUint8(IPInt::CallArgumentBytecode::CallArgDecSP));
    ASSERT(spOffset == frameSize);
}

static intptr_t addCallResultBytecode(Vector<uint8_t, 16>& results, const CallInformation& callConvention)
{
    constexpr static int NUM_MINT_RET_GPRS = 8;
    constexpr static int NUM_MINT_RET_FPRS = 8;
    ASSERT_UNUSED(NUM_MINT_RET_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_MINT_RET_GPRS);
    ASSERT_UNUSED(NUM_MINT_RET_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_MINT_RET_FPRS);

    intptr_t firstStackResultSPOffset = 0;
    bool hasSeenStackResult = false;
    intptr_t spOffset = 0;

    results.appendUsingFunctor(callConvention.results.size(),
        [&](unsigned index) -> uint8_t {
            const ArgumentLocation& argLoc = callConvention.results[index];
            const ValueLocation& loc = argLoc.location;

            if (loc.isGPR()) {
                ASSERT_UNUSED(NUM_MINT_RET_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_MINT_RET_GPRS);
#if USE(JSVALUE64)
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr());
#elif USE(JSVALUE32_64)
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord));
#endif
            }

            if (loc.isFPR()) {
                ASSERT_UNUSED(NUM_MINT_RET_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_MINT_RET_FPRS);
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultFPR) + FPRInfo::toArgumentIndex(loc.fpr());
            }
            RELEASE_ASSERT(loc.isStackArgument());

            if (!hasSeenStackResult) {
                hasSeenStackResult = true;
                // mINT needs to be able to locate the first stack result
                spOffset = loc.offsetFromSP();
                firstStackResultSPOffset = spOffset;
            }
            // mINT bytecode handlers assume this; if it fails, mINT needs updating
            ASSERT(loc.offsetFromSP() == spOffset);
            switch (argLoc.width) {
            case Width::Width64:
                spOffset += 8; // This bytecode pops 8-bytes
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultStack);
            case Width::Width128:
                spOffset += 16; // This bytecode pops 16-bytes
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultStackVector);
            default:
                ASSERT_NOT_REACHED("No bytecode for stack result location width");
                return 0;
            }
        });

    results.append(static_cast<uint8_t>(IPInt::CallResultBytecode::End));
    return firstStackResultSPOffset;
}

void IPIntGenerator::addCallCommonData(const FunctionSignature&, const CallInformation& callConvention)
{
    // cachedCallInformationFor() invalidates this cache on a miss, so if the cache is populated,
    // it was a cache hit and we can use the previously generated payload.
    if (!m_cachedCallBytecode.isEmpty()) {
        size_t size = m_metadata->m_metadata.size();
        m_metadata->addBlankSpace(m_cachedCallBytecode.size());
        memcpy(m_metadata->m_metadata.mutableSpan().data() + size, m_cachedCallBytecode.span().data(), m_cachedCallBytecode.size());
        return;
    }

    addCallArgumentBytecode<false>(m_cachedCallBytecode, callConvention);
    m_cachedCallBytecode.reverse();

    Checked<uint32_t> frameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes);
    IPInt::CallReturnMetadata commonReturn {
        .stackFrameSize = frameSize,
        .firstStackResultSPOffset = 0, // TBD
        .resultBytecode = { }
    };

    Vector<uint8_t, 16> returnBytecode;
    Checked<uint32_t> firstStackResultSPOffset = addCallResultBytecode(returnBytecode, callConvention);

    commonReturn.firstStackResultSPOffset = firstStackResultSPOffset;

    auto toSpan = [&](auto& metadata) {
        auto start = std::bit_cast<const uint8_t*>(&metadata);
        return std::span { start, start + sizeof(metadata) };
    };
    m_cachedCallBytecode.append(toSpan(commonReturn));
    m_cachedCallBytecode.append(returnBytecode.span());

    size_t size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(m_cachedCallBytecode.size());
    memcpy(m_metadata->m_metadata.mutableSpan().data() + size, m_cachedCallBytecode.mutableSpan().data(), m_cachedCallBytecode.size());
}

void IPIntGenerator::addTailCallCommonData(const FunctionSignature&, const CallInformation& callConvention)
{
    Vector<uint8_t, 16> mINTBytecode;
    addCallArgumentBytecode<true>(mINTBytecode, callConvention);

    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(mINTBytecode.size());
    std::ranges::reverse_copy(mINTBytecode, m_metadata->m_metadata.mutableSpan().data() + size);

    size_t stackArgumentsAndResultsInBytes = roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes) - callConvention.headerIncludingThisSizeInBytes;
    // The WASM stack slots are always 16-bytes.
    size_t extraWasmStackInBytes = roundUpToMultipleOf<16>(stackArgumentsAndResultsInBytes);
    if (m_stackSize + extraWasmStackInBytes / 16 > m_maxStackSize)
        m_maxStackSize = m_stackSize + extraWasmStackInBytes / 16;

    ASSERT(!(stackArgumentsAndResultsInBytes % 16)); // mINT requires this for 16-bytes at a time tail-call arguments copy
    m_metadata->appendMetadata(stackArgumentsAndResultsInBytes);
}

[[nodiscard]] PartialResult IPIntGenerator::addCall(unsigned callProfileIndex, FunctionSpaceIndex index, const TypeDefinition& type, ArgumentList&, ResultList& results, CallType callType)
{
    const FunctionSignature& signature = *type.as<FunctionSignature>();
    const CallInformation& callConvention = cachedCallInformationFor(signature);
    m_metadata->addCallTarget(callProfileIndex, index);

    if (callType == CallType::TailCall) {
        // on a tail call, we need to:
        // roll back to old SP, shift SP to accommodate arguments
        // put arguments into registers / sp (reutilize mINT)
        // jump to entrypoint
        changeStackSize(-signature.argumentCount());
        m_metadata->setTailCall(index, m_info.isImportedFunctionFromFunctionIndexSpace(index));

        IPInt::TailCallMetadata functionIndexMetadata {
            .length = safeCast<uint8_t>(getCurrentInstructionLength()),
            .callProfileIndex = callProfileIndex,
            .functionIndex = index,
            .callerStackArgSize = static_cast<int32_t>(m_argumentAndResultsStackSize),
            .argumentBytecode = { }
        };
        m_metadata->appendMetadata(functionIndexMetadata);
        addTailCallCommonData(signature, callConvention);
        return { };
    }

    results.appendUsingFunctor(signature.returnCount(), [](unsigned) { return Value { }; });
    changeStackSize(signature.returnCount() - signature.argumentCount());

    Checked<uint32_t> frameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes);
    IPInt::CallMetadata functionIndexMetadata {
        .length = safeCast<uint8_t>(getCurrentInstructionLength()),
        .callProfileIndex = callProfileIndex,
        .functionIndex = index,
        .signature = {
            frameSize,
            static_cast<uint16_t>(signature.returnCount() > signature.argumentCount() ? signature.returnCount() - signature.argumentCount() : 0),
            static_cast<uint16_t>(signature.argumentCount())
        },
        .argumentBytecode = { }
    };
    m_metadata->appendMetadata(functionIndexMetadata);
    addCallCommonData(signature, callConvention);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addCallIndirect(unsigned callProfileIndex, unsigned tableIndex, const TypeDefinition& originalSignature, ArgumentList&, ResultList& results, CallType callType)
{
    const FunctionSignature& signature = *originalSignature.expand().as<FunctionSignature>();
    const CallInformation& callConvention = cachedCallInformationFor(signature);
    m_metadata->addCallTarget(callProfileIndex, { });

    if (callType == CallType::TailCall) {
        const unsigned callIndex = 1;
        changeStackSize(-signature.argumentCount() - callIndex);
        m_metadata->setTailCallClobbersInstance();

        // on a tail call, we need to:
        // roll back to old SP, shift SP to accommodate arguments
        // put arguments into registers / sp (reutilize mINT)
        // jump to entrypoint
        IPInt::TailCallIndirectMetadata functionIndexMetadata {
            .length = safeCast<uint8_t>(getCurrentInstructionLength()),
            .callProfileIndex = callProfileIndex,
            .tableIndex = tableIndex,
            .rtt = m_metadata->addSignature(originalSignature),
            .callerStackArgSize = static_cast<int32_t>(m_argumentAndResultsStackSize),
            .argumentBytecode = { }
        };
        m_metadata->appendMetadata(functionIndexMetadata);
        addTailCallCommonData(signature, callConvention);
        return { };
    }

    results.appendUsingFunctor(signature.returnCount(), [](unsigned) { return Value { }; });
    const unsigned callIndex = 1;
    changeStackSize(signature.returnCount() - signature.argumentCount() - callIndex);

    Checked<uint32_t> frameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes);
    IPInt::CallIndirectMetadata functionIndexMetadata {
        .length = safeCast<uint8_t>(getCurrentInstructionLength()),
        .callProfileIndex = callProfileIndex,
        .tableIndex = tableIndex,
        .rtt = m_metadata->addSignature(originalSignature),
        .signature = {
            frameSize,
            static_cast<uint16_t>(signature.returnCount() > signature.argumentCount() ? signature.returnCount() - signature.argumentCount() : 0),
            static_cast<uint16_t>(signature.argumentCount())
        },
        .argumentBytecode = { }
    };
    m_metadata->appendMetadata(functionIndexMetadata);

    addCallCommonData(signature, callConvention);
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addCallRef(unsigned callProfileIndex, const TypeDefinition& originalSignature, ArgumentList&, ResultList& results, CallType callType)
{
    const FunctionSignature& signature = *originalSignature.expand().as<FunctionSignature>();
    const CallInformation& callConvention = cachedCallInformationFor(signature);
    m_metadata->addCallTarget(callProfileIndex, { });

    if (callType == CallType::TailCall) {
        const unsigned callIndex = 1;
        changeStackSize(-signature.argumentCount() - callIndex);
        m_metadata->setTailCallClobbersInstance();

        // on a tail call, we need to:
        // roll back to old SP, shift SP to accommodate arguments
        // put arguments into registers / sp (reutilize mINT)
        // jump to entrypoint
        IPInt::TailCallRefMetadata callMetadata {
            .length = safeCast<uint8_t>(getCurrentInstructionLength()),
            .callProfileIndex = callProfileIndex,
            .callerStackArgSize = static_cast<int32_t>(m_argumentAndResultsStackSize),
            .argumentBytecode = { }
        };
        m_metadata->appendMetadata(callMetadata);
        addTailCallCommonData(signature, callConvention);
        return { };
    }

    results.appendUsingFunctor(signature.returnCount(), [](unsigned) { return Value { }; });
    const unsigned callRef = 1;
    changeStackSize(signature.returnCount() - signature.argumentCount() - callRef);

    Checked<uint32_t> frameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes);
    IPInt::CallRefMetadata callMetadata {
        .length = safeCast<uint8_t>(getCurrentInstructionLength()),
        .callProfileIndex = callProfileIndex,
        .signature = {
            frameSize,
            static_cast<uint16_t>(signature.returnCount() > signature.argumentCount() ? signature.returnCount() - signature.argumentCount() : 0),
            static_cast<uint16_t>(signature.argumentCount())
        },
        .argumentBytecode = { }
    };
    m_metadata->appendMetadata(callMetadata);

    addCallCommonData(signature, callConvention);
    return { };
}

// Traps

[[nodiscard]] PartialResult IPIntGenerator::addUnreachable()
{
    return { };
}

[[nodiscard]] PartialResult IPIntGenerator::addCrash()
{
    return { };
}

// Finalize

std::unique_ptr<FunctionIPIntMetadataGenerator> IPIntGenerator::finalize()
{
    if (m_usesRethrow)
        m_metadata->m_numAlignedRethrowSlots = roundUpToMultipleOf<2>(m_maxTryDepth);

    // Pad the metadata to an even number since we will allocate the rounded up size
    if (m_metadata->m_numLocals % 2)
        m_metadata->m_argumINTBytecode.append(0);

    m_metadata->m_maxFrameSizeInV128 = roundUpToMultipleOf<2>(m_metadata->m_numLocals) / 2;
    m_metadata->m_maxFrameSizeInV128 += m_metadata->m_numAlignedRethrowSlots / 2;
    m_metadata->m_maxFrameSizeInV128 += m_maxStackSize;
    if (m_metadata->m_callTargets.size() < m_parser->numCallProfiles())
        m_metadata->m_callTargets.insertFill(m_metadata->m_callTargets.size(), FunctionSpaceIndex { }, m_parser->numCallProfiles() - m_metadata->m_callTargets.size());

    return WTF::move(m_metadata);
}

Expected<std::unique_ptr<FunctionIPIntMetadataGenerator>, String> parseAndCompileMetadata(std::span<const uint8_t> function, const TypeDefinition& signature, ModuleInformation& info, FunctionCodeIndex functionIndex)
{
    IPIntGenerator generator(info, functionIndex, signature, function);
    FunctionParser<IPIntGenerator> parser(generator, function, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());
    return generator.finalize();
}

void parseForDebugInfo(std::span<const uint8_t> function, const TypeDefinition& signature, ModuleInformation& info, FunctionCodeIndex functionIndex, FunctionDebugInfo& debugInfo)
{
    IPIntGenerator generator(info, functionIndex, signature, function, &debugInfo);
    FunctionParser<IPIntGenerator> parser(generator, function, signature, info);
    auto result = parser.parse();
    if (!result) {
        WTF::dataLogLn("Failed to parse for debug info:", result.error());
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void IPIntGenerator::dump(const ControlStack&, const Stack*)
{
    dataLogLn("PC: ", m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset, " MC: ", m_metadata->m_metadata.size());
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
