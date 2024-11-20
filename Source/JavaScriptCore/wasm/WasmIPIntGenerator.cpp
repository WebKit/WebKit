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
#include "WasmGeneratorTraits.h"
#include <variant>
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

    IPIntControlType(BlockSignature signature, uint32_t stackSize, BlockType blockType, CatchKind catchKind = CatchKind::Catch)
        : m_signature(signature)
        , m_blockType(blockType)
        , m_catchKind(catchKind)
        , m_stackSize(stackSize)
    { }

    static bool isIf(const IPIntControlType& control) { return control.blockType() == BlockType::If; }
    static bool isTry(const IPIntControlType& control) { return control.blockType() == BlockType::Try; }
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
    BlockSignature signature() const { return m_signature; }
    unsigned stackSize() const { return m_stackSize; }

    Type branchTargetType(unsigned i) const
    {
        ASSERT(i < branchTargetArity());
        if (blockType() == BlockType::Loop)
            return m_signature.m_signature->argumentType(i);
        return m_signature.m_signature->returnType(i);
    }

    unsigned branchTargetArity() const
    {
        return isLoop(*this)
            ? m_signature.m_signature->argumentCount()
            : m_signature.m_signature->returnCount();
    }

private:
    BlockSignature m_signature;
    BlockType m_blockType;
    CatchKind m_catchKind;

    bool isElse = false;

    int32_t m_pendingOffset { -1 };

    uint32_t m_index = 0;
    uint32_t m_pc = 0; // where am i?
    uint32_t m_mc = 0;
    uint32_t m_pcEnd = 0;

    uint32_t m_stackSize { 0 };
    uint32_t m_tryDepth { 0 };

    Vector<IPIntLocation> m_catchesAwaitingFixup;
};

class IPIntGenerator {
public:
    IPIntGenerator(ModuleInformation&, FunctionCodeIndex, const TypeDefinition&, std::span<const uint8_t>);

    static constexpr bool shouldFuseBranchCompare = false;

    using ControlType = IPIntControlType;
    using ExpressionType = Value;
    using CallType = CallLinkInfo::CallType;
    using ResultList = Vector<Value, 8>;
    using ArgumentList = Vector<Value, 8>;

    using ExpressionList = Vector<Value, 1>;
    using ControlEntry = FunctionParser<IPIntGenerator>::ControlEntry;
    using ControlStack = FunctionParser<IPIntGenerator>::ControlStack;
    using Stack = FunctionParser<IPIntGenerator>::Stack;
    using TypedExpression = FunctionParser<IPIntGenerator>::TypedExpression;
    using CatchHandler = FunctionParser<IPIntGenerator>::CatchHandler;

    static ExpressionType emptyExpression() { return { }; };
    PartialResult WARN_UNUSED_RETURN addDrop(ExpressionType);

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

    std::unique_ptr<FunctionIPIntMetadataGenerator> finalize();

    PartialResult WARN_UNUSED_RETURN addArguments(const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    Value addConstant(Type, uint64_t);

    // SIMD

    bool usesSIMD() { return m_usesSIMD; }
    void notifyFunctionUsesSIMD() { ASSERT(Options::useWasmSIMD()); m_usesSIMD = true; }
    PartialResult WARN_UNUSED_RETURN addSIMDLoad(ExpressionType, uint32_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDStore(ExpressionType, ExpressionType, uint32_t);
    PartialResult WARN_UNUSED_RETURN addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadSplat(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadExtend(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDLoadPad(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&);

    ExpressionType addConstant(v128_t);

    // SIMD generated

    PartialResult WARN_UNUSED_RETURN addExtractLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&);
#if ENABLE(B3_JIT)
    PartialResult WARN_UNUSED_RETURN addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&);
#endif
    PartialResult WARN_UNUSED_RETURN addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&);

    // References

    PartialResult WARN_UNUSED_RETURN addRefIsNull(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addRefFunc(FunctionSpaceIndex, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addRefAsNonNull(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addRefEq(ExpressionType, ExpressionType, ExpressionType&);

    // Tables

    PartialResult WARN_UNUSED_RETURN addTableGet(unsigned, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addTableSet(unsigned, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addTableInit(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addElemDrop(unsigned);
    PartialResult WARN_UNUSED_RETURN addTableSize(unsigned, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addTableGrow(unsigned, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addTableFill(unsigned, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addTableCopy(unsigned, unsigned, ExpressionType, ExpressionType, ExpressionType);

    // Locals

    PartialResult WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN setLocal(uint32_t, ExpressionType);
    PartialResult WARN_UNUSED_RETURN teeLocal(uint32_t, ExpressionType, ExpressionType& result);

    // Globals

    PartialResult WARN_UNUSED_RETURN getGlobal(uint32_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN setGlobal(uint32_t, ExpressionType);

    // Memory

    PartialResult WARN_UNUSED_RETURN load(LoadOpType, ExpressionType, ExpressionType&, uint32_t);
    PartialResult WARN_UNUSED_RETURN store(StoreOpType, ExpressionType, ExpressionType, uint32_t);
    PartialResult WARN_UNUSED_RETURN addGrowMemory(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addCurrentMemory(ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addMemoryFill(ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addMemoryCopy(ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addMemoryInit(unsigned, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addDataDrop(unsigned);

    // Atomics

    PartialResult WARN_UNUSED_RETURN atomicLoad(ExtAtomicOpType, Type, ExpressionType, ExpressionType&, uint32_t);
    PartialResult WARN_UNUSED_RETURN atomicStore(ExtAtomicOpType, Type, ExpressionType, ExpressionType, uint32_t);
    PartialResult WARN_UNUSED_RETURN atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType&, uint32_t);
    PartialResult WARN_UNUSED_RETURN atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t);

    PartialResult WARN_UNUSED_RETURN atomicWait(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t);
    PartialResult WARN_UNUSED_RETURN atomicNotify(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType&, uint32_t);
    PartialResult WARN_UNUSED_RETURN atomicFence(ExtAtomicOpType, uint8_t);

    // Saturated truncation

    PartialResult WARN_UNUSED_RETURN truncSaturated(Ext1OpType, ExpressionType, ExpressionType&, Type, Type);

    // GC

    PartialResult WARN_UNUSED_RETURN addRefI31(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI31GetS(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI31GetU(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayNew(uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayNewDefault(uint32_t, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayNewData(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayNewElem(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayNewFixed(uint32_t, ArgumentList&, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayGet(ExtGCOpType, uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayLen(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayFill(uint32_t, ExpressionType, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayCopy(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayInitElem(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayInitData(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t, ArgumentList&, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addStructNewDefault(uint32_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addStructGet(ExtGCOpType, ExpressionType, const StructType&, uint32_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addStructSet(ExpressionType, const StructType&, uint32_t, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addRefTest(ExpressionType, bool, int32_t, bool, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addRefCast(ExpressionType, bool, int32_t, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addAnyConvertExtern(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addExternConvertAny(ExpressionType, ExpressionType&);

    // Basic operators

    PartialResult WARN_UNUSED_RETURN addI32DivS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32RemS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32DivU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32RemU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64DivS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64RemS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64DivU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64RemU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Ctz(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Popcnt(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Popcnt(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Nearest(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Nearest(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Trunc(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Trunc(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32TruncSF64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32TruncSF32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32TruncUF64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32TruncUF32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64TruncSF64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64TruncSF32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64TruncUF64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64TruncUF32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Ceil(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Mul(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Sub(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Le(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32DemoteF64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Ne(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Lt(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Min(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Max(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Min(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Max(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Mul(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Div(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Clz(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Copysign(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32ReinterpretI32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Ne(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Gt(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Sqrt(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Ge(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64GtS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64GtU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Div(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Add(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32LeU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32LeS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Ne(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Clz(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Neg(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32And(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32LtU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Rotr(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Abs(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32LtS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Eq(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Copysign(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32ConvertSI64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Rotl(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Lt(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64ConvertSI32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Eq(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Le(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Ge(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32ShrU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32ConvertUI32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32ShrS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32GeU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Ceil(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32GeS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Shl(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Floor(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Xor(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Abs(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Mul(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Sub(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32ReinterpretF32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Add(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Sub(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Or(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64LtU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64LtS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64ConvertSI64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Xor(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64GeU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Mul(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Sub(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64PromoteF32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Add(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64GeS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64ExtendUI32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Ne(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64ReinterpretI64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Eq(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Eq(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Floor(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32ConvertSI32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64And(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Or(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Ctz(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Eqz(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Eqz(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64ReinterpretF64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64ConvertUI32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32ConvertUI64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64ConvertUI64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64ShrS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64ShrU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Sqrt(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Shl(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF32Gt(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32WrapI64(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Rotl(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Rotr(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32GtU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64ExtendSI32(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Extend8S(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32Extend16S(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Extend8S(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Extend16S(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Extend32S(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI32GtS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addF64Neg(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64LeU(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64LeS(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addI64Add(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&);

    // Control flow

    ControlType WARN_UNUSED_RETURN addTopLevel(BlockSignature);
    PartialResult WARN_UNUSED_RETURN addBlock(BlockSignature, Stack&, ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addLoop(BlockSignature, Stack&, ControlType&, Stack&, uint32_t);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType, BlockSignature, Stack&, ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addElse(ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlType&);

    PartialResult WARN_UNUSED_RETURN addTry(BlockSignature, Stack&, ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addTryTable(BlockSignature, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned, const TypeDefinition&, Stack&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned, const TypeDefinition&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned, ArgumentList&, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrowRef(ExpressionType, Stack&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlType&, const Stack&);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlType&, ExpressionType, const Stack&);
    PartialResult WARN_UNUSED_RETURN addBranchNull(ControlType&, ExpressionType, Stack&, bool, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addBranchCast(ControlType&, ExpressionType, Stack&, bool, int32_t, bool);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType, const Vector<ControlType*>&, ControlType&, const Stack&);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack&);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, Stack&);

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&);

    // Fused comparison stubs (TODO: make use of these for better codegen)
    PartialResult WARN_UNUSED_RETURN addFusedBranchCompare(OpType, ControlType&, ExpressionType, const Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    PartialResult WARN_UNUSED_RETURN addFusedBranchCompare(OpType, ControlType&, ExpressionType, ExpressionType, const Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    PartialResult WARN_UNUSED_RETURN addFusedIfCompare(OpType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&) { RELEASE_ASSERT_NOT_REACHED(); }
    PartialResult WARN_UNUSED_RETURN addFusedIfCompare(OpType, ExpressionType, ExpressionType, BlockSignature, Stack&, ControlType&, Stack&) { RELEASE_ASSERT_NOT_REACHED(); }

    // Calls

    PartialResult WARN_UNUSED_RETURN addCall(FunctionSpaceIndex, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned, const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, ArgumentList&, ResultList&, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN addCrash();

    void setParser(FunctionParser<IPIntGenerator>* parser) { m_parser = parser; };
    size_t getCurrentInstructionLength()
    {
        return m_parser->offset() - m_parser->currentOpcodeStartingOffset();
    }
    void addCallCommonData(const FunctionSignature&);
    void addTailCallCommonData(const FunctionSignature&);
    void didFinishParsingLocals()
    {
        m_metadata->m_bytecodeOffset = m_parser->offset();
    }
    void didPopValueFromStack(ExpressionType, ASCIILiteral) { }
    void willParseOpcode() { }
    void willParseExtendedOpcode() { }
    void didParseOpcode()
    {
        if (!m_parser->unreachableBlocks())
            ASSERT(m_parser->getStackHeightInValues() == m_stackSize.value());
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
        controlStructuresAwaitingCoalescing[index].m_awaitingEntryTarget.append(loc);
    }

    void tryToResolveExitTarget(uint32_t index, IPIntLocation loc, uint8_t*)
    {
        controlStructuresAwaitingCoalescing[index].m_awaitingExitTarget.append(loc);
    }

    void tryToResolveBranchTarget(ControlType& targetBlock, IPIntLocation loc, uint8_t* metadata)
    {
        if (ControlType::isTopLevel(targetBlock)) {
            jumpLocationsAwaitingEnd.append(loc);
            return;
        }
        auto index = targetBlock.m_index;
        auto& target = controlStructuresAwaitingCoalescing[index];
        if (target.isLoop) {
            ASSERT(target.m_entryResolved);
            IPInt::BlockMetadata md = { target.m_entryTarget.pc - loc.pc, target.m_entryTarget.mc - loc.mc };
            WRITE_TO_METADATA(metadata + loc.mc, md, IPInt::BlockMetadata);
        } else {
            ASSERT(!target.m_exitResolved);
            target.m_awaitingBranchTarget.append(loc);
        }
    }

    static constexpr bool tierSupportsSIMD = true;
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
        uint32_t startPC;
        bool isLoop = false;

        Vector<IPIntLocation, 16> m_awaitingEntryTarget = { };
        Vector<IPIntLocation, 16> m_awaitingBranchTarget = { };
        Vector<IPIntLocation, 16> m_awaitingExitTarget = { };

        bool m_entryResolved = false;
        IPIntLocation m_entryTarget = { 0, 0 }; // where do we go when entering normally?
        bool m_exitResolved = false;
        IPIntLocation m_exitTarget = { 0, 0 }; // where do we go when leaving?
    };
    Vector<ControlStructureAwaitingCoalescing, 16> controlStructuresAwaitingCoalescing;

    struct QueuedCoalesceRequest {
        size_t index;
        bool isEntry;
    };
    Vector<QueuedCoalesceRequest, 16> coalesceQueue;

    // if this is 0, all our control structures have been coalesced and we can clean up the vector
    unsigned coalesceDebt = 0;

    // exit loations can still be unresolved when the ControlType* dies, so we put them here
    Vector<IPIntLocation> exitHandlersAwaitingCoalescing;
    // all jumps that go to the top level and return
    Vector<IPIntLocation> jumpLocationsAwaitingEnd;

    inline uint32_t curPC() { return m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset; }
    inline uint32_t nextPC() { return m_parser->offset() - m_metadata->m_bytecodeOffset; }
    inline uint32_t curMC() { return m_metadata->m_metadata.size(); }

    // FIXME: If rethrow is not used in practice we should consider just reparsing the function to update the SP offsets.
    Vector<uint32_t> m_catchSPMetadataOffsets;

    bool m_usesRethrow { false };
    bool m_usesSIMD { false };
};

// use if (true) to avoid warnings.
#define IPINT_UNIMPLEMENTED { if (true) { CRASH(); } return { }; }

IPIntGenerator::IPIntGenerator(ModuleInformation& info, FunctionCodeIndex functionIndex, const TypeDefinition&, std::span<const uint8_t> bytecode)
    : m_info(info)
    , m_functionIndex(functionIndex)
    , m_metadata(WTF::makeUnique<FunctionIPIntMetadataGenerator>(functionIndex, bytecode))
{
    m_metadata->m_callees = FixedBitVector(m_info.internalFunctionCount());
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addDrop(ExpressionType)
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDLoad(ExpressionType, uint32_t, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDStore(ExpressionType, ExpressionType, uint32_t) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDSplat(SIMDLane, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDShuffle(v128_t, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDShift(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDExtmul(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDLoadSplat(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDLoadLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDStoreLane(SIMDLaneOperation, ExpressionType, ExpressionType, uint32_t, uint8_t) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDLoadExtend(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDLoadPad(SIMDLaneOperation, ExpressionType, uint32_t, ExpressionType&) IPINT_UNIMPLEMENTED

IPIntGenerator::ExpressionType IPIntGenerator::addConstant(v128_t)
{
    changeStackSize(1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addExtractLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addReplaceLane(SIMDInfo, uint8_t, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDI_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDV_V(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDBitwiseSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
#if ENABLE(B3_JIT)
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDRelOp(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, B3::Air::Arg, ExpressionType&) IPINT_UNIMPLEMENTED
#endif
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDV_VV(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSIMDRelaxedFMA(SIMDLaneOperation, SIMDInfo, ExpressionType, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED

// References

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefIsNull(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefFunc(FunctionSpaceIndex index, ExpressionType&)
{
    changeStackSize(1);
    m_metadata->addLEB128ConstantInt32AndLength(index, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefAsNonNull(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefEq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Tables

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableGet(unsigned index, ExpressionType, ExpressionType&)
{
    m_metadata->addLEB128ConstantInt32AndLength(index, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableSet(unsigned index, ExpressionType, ExpressionType)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(index, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType, ExpressionType, ExpressionType)
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addElemDrop(unsigned elementIndex)
{
    m_metadata->addLEB128ConstantInt32AndLength(elementIndex, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableSize(unsigned tableIndex, ExpressionType&)
{
    changeStackSize(1);
    m_metadata->addLEB128ConstantInt32AndLength(tableIndex, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableGrow(unsigned tableIndex, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    IPInt::TableGrowMetadata table {
        .tableIndex = safeCast<uint32_t>(tableIndex),
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(table);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableFill(unsigned tableIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    IPInt::TableFillMetadata table {
        .tableIndex = safeCast<uint32_t>(tableIndex),
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(table);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType, ExpressionType, ExpressionType)
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArguments(const TypeDefinition &signature)
{
    auto fprToIndex = [&](FPRReg r) -> unsigned {
        for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i) {
            if (FPRInfo::toArgumentRegister(i) == r)
                return i;
        }
        RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return 0;
    };

    auto sig = signature.as<FunctionSignature>();
    CallInformation callCC = wasmCallingConvention().callInformationFor(*sig, CallRole::Callee);

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
        auto loc = callCC.params[i].location;
        if (loc.isGPR()) {
            ASSERT_UNUSED(NUM_ARGUMINT_GPRS, loc.jsr().payloadGPR() < NUM_ARGUMINT_GPRS);
            m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::ArgGPR) + loc.jsr().payloadGPR());
        } else if (loc.isFPR()) {
            ASSERT_UNUSED(NUM_ARGUMINT_FPRS, fprToIndex(loc.fpr()) < NUM_ARGUMINT_FPRS);
            m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::RegFPR) + fprToIndex(loc.fpr()));
        } else if (loc.isStack()) {
            m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::Stack));
        }
    }
    m_metadata->m_argumINTBytecode.append(static_cast<uint8_t>(IPInt::ArgumINTBytecode::End));

    m_metadata->addReturnData(*sig);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addLocal(Type, uint32_t count)
{
    m_metadata->m_numLocals += count;
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::getLocal(uint32_t, ExpressionType&)
{
    // Local indices are usually very small, so we decode them on the fly
    // instead of generating metadata.
    changeStackSize(1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::setLocal(uint32_t, ExpressionType)
{
    // Local indices are usually very small, so we decode them on the fly
    // instead of generating metadata.
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::teeLocal(uint32_t, ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::getGlobal(uint32_t index, ExpressionType&)
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::setGlobal(uint32_t index, ExpressionType)
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::load(LoadOpType, ExpressionType, ExpressionType&, uint32_t offset)
{
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::store(StoreOpType, ExpressionType, ExpressionType, uint32_t offset)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

// Memories

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addGrowMemory(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCurrentMemory(ExpressionType&)
{
    changeStackSize(1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addMemoryFill(ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addMemoryCopy(ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addMemoryInit(unsigned dataIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    m_metadata->addLEB128ConstantInt32AndLength(dataIndex, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addDataDrop(unsigned dataIndex)
{
    m_metadata->addLEB128ConstantInt32AndLength(dataIndex, getCurrentInstructionLength());
    return { };
}

// Atomics

PartialResult WARN_UNUSED_RETURN IPIntGenerator::atomicLoad(ExtAtomicOpType, Type, ExpressionType, ExpressionType&, uint32_t offset)
{
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::atomicStore(ExtAtomicOpType, Type, ExpressionType, ExpressionType, uint32_t offset)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-1);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::atomicWait(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-2);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::atomicNotify(ExtAtomicOpType, ExpressionType, ExpressionType, ExpressionType&, uint32_t offset)
{
    changeStackSize(-1);
    m_metadata->addLEB128ConstantInt32AndLength(offset, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::atomicFence(ExtAtomicOpType, uint8_t)
{
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

// GC

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefI31(ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI31GetS(ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI31GetU(ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayNew(uint32_t, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayNewData(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayNewElem(uint32_t, uint32_t, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayNewFixed(uint32_t, ArgumentList&, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayNewDefault(uint32_t, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayGet(ExtGCOpType, uint32_t, ExpressionType, ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArraySet(uint32_t, ExpressionType, ExpressionType, ExpressionType) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayLen(ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayFill(uint32_t typeIndex, ExpressionType, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-4);
    m_metadata->addLEB128ConstantInt32AndLength(typeIndex, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayCopy(uint32_t dstArrayIndex, ExpressionType, ExpressionType, uint32_t srcArrayIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-5);
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(9);
    auto arrayCopyData = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(arrayCopyData, dstArrayIndex, uint32_t);
    WRITE_TO_METADATA(arrayCopyData + 4, srcArrayIndex, uint32_t);
    WRITE_TO_METADATA(arrayCopyData + 8, getCurrentInstructionLength(), uint8_t);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayInitElem(uint32_t dstArrayIndex, ExpressionType, ExpressionType, uint32_t srcElementIndex, ExpressionType, ExpressionType)
{
    changeStackSize(-4);
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(9);
    auto arrayInitElemData = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(arrayInitElemData, dstArrayIndex, uint32_t);
    WRITE_TO_METADATA(arrayInitElemData + 4, srcElementIndex, uint32_t);
    WRITE_TO_METADATA(arrayInitElemData + 8, getCurrentInstructionLength(), uint8_t);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayInitData(uint32_t dstArrayIndex, ExpressionType, ExpressionType, uint32_t srcDataIndex, ExpressionType, ExpressionType)
{
    changeStackSize(-4);
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(9);
    auto arrayInitDataData = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(arrayInitDataData, dstArrayIndex, uint32_t);
    WRITE_TO_METADATA(arrayInitDataData + 4, srcDataIndex, uint32_t);
    WRITE_TO_METADATA(arrayInitDataData + 8, getCurrentInstructionLength(), uint8_t);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addStructNew(uint32_t, ArgumentList&, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addStructNewDefault(uint32_t, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addStructGet(ExtGCOpType, ExpressionType, const StructType&, uint32_t, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addStructSet(ExpressionType, const StructType&, uint32_t, ExpressionType) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefTest(ExpressionType, bool, int32_t, bool, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefCast(ExpressionType, bool, int32_t, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addAnyConvertExtern(ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addExternConvertAny(ExpressionType, ExpressionType&) IPINT_UNIMPLEMENTED

// Integer Arithmetic

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32DivS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32DivU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64DivS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64DivU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32RemS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32RemU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64RemS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64RemU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Bitwise Operations

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32And(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64And(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Xor(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Xor(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Or(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Or(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Shl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32ShrU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32ShrS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Shl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64ShrU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64ShrS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Rotl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Rotl(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Rotr(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Rotr(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Popcnt(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Popcnt(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Clz(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Clz(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Ctz(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Ctz(ExpressionType, ExpressionType&)
{
    return { };
}

// Floating-Point Arithmetic

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Add(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Sub(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Mul(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Div(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Div(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Other Floating-Point Instructions

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Min(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Max(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Min(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Max(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Nearest(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Nearest(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Floor(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Floor(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Ceil(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Ceil(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Copysign(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Copysign(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Sqrt(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Sqrt(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Neg(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Neg(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Abs(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Abs(ExpressionType, ExpressionType&)
{
    return { };
}

// Integer Comparisons

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32LtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32LtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32LeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32LeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32GtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32GtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32GeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32GeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Eqz(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64GtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64GtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64GeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64GeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64LtS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64LtU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64LeS(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64LeU(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Eqz(ExpressionType, ExpressionType&)
{
    return { };
}

// Floating-Point Comparisons

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Lt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Le(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Gt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Ge(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Eq(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Ne(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Lt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Le(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Gt(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Ge(ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-1);
    return { };
}

// Integer Extension

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64ExtendSI32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64ExtendUI32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Extend8S(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32Extend16S(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Extend8S(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Extend16S(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64Extend32S(ExpressionType, ExpressionType&)
{
    return { };
}

// Truncation

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64Trunc(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32Trunc(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32TruncSF64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32TruncSF32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32TruncUF64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32TruncUF32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64TruncSF64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64TruncSF32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64TruncUF64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64TruncUF32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::truncSaturated(Ext1OpType, ExpressionType, ExpressionType&, Type, Type)
{
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

// Conversions

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32WrapI64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32DemoteF64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64PromoteF32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32ReinterpretI32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI32ReinterpretF32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64ReinterpretI64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addI64ReinterpretF64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32ConvertSI32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32ConvertUI32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32ConvertSI64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF32ConvertUI64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64ConvertSI32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64ConvertUI32(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64ConvertSI64(ExpressionType, ExpressionType&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addF64ConvertUI64(ExpressionType, ExpressionType&)
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
    for (auto& entry : coalesceQueue) {
        if (entry.isEntry)
            resolveEntryTarget(entry.index, here);
        else
            resolveExitTarget(entry.index, here);
    }
    coalesceQueue.clear();

    if (!coalesceDebt)
        controlStructuresAwaitingCoalescing.clear();

    for (auto& src : exitHandlersAwaitingCoalescing) {
        IPInt::BlockMetadata md = { here.pc - src.pc, here.mc - src.mc };
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + src.mc, md, IPInt::BlockMetadata);
    }
    exitHandlersAwaitingCoalescing.clear();
}

void IPIntGenerator::resolveEntryTarget(unsigned index, IPIntLocation loc)
{
    auto& control = controlStructuresAwaitingCoalescing[index];
    ASSERT(!control.m_entryResolved);
    for (auto& src : control.m_awaitingEntryTarget) {
        // write delta PC and delta MC
        IPInt::BlockMetadata md = { loc.pc - src.pc, loc.mc - src.mc };
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + src.mc, md, IPInt::BlockMetadata);
    }
    if (control.isLoop) {
        for (auto& src : control.m_awaitingBranchTarget) {
            IPInt::BlockMetadata md = { loc.pc - src.pc, loc.mc - src.mc };
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + src.mc, md, IPInt::BlockMetadata);
        }
        control.m_awaitingBranchTarget.clear();
    }
    control.m_awaitingEntryTarget.clear();
    control.m_entryResolved = true;
    control.m_entryTarget = loc;
}

void IPIntGenerator::resolveExitTarget(unsigned index, IPIntLocation loc)
{
    auto& control = controlStructuresAwaitingCoalescing[index];
    ASSERT(!control.m_exitResolved);
    for (auto& src : control.m_awaitingExitTarget) {
        // write delta PC and delta MC
        IPInt::BlockMetadata md = { loc.pc - src.pc, loc.mc - src.mc };
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + src.mc, md, IPInt::BlockMetadata);
    }
    if (!control.isLoop) {
        for (auto& src : control.m_awaitingBranchTarget) {
            IPInt::BlockMetadata md = { loc.pc - src.pc, loc.mc - src.mc };
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + src.mc, md, IPInt::BlockMetadata);
        }
        control.m_awaitingBranchTarget.clear();
    }
    control.m_awaitingExitTarget.clear();
    control.m_exitResolved = true;
    control.m_exitTarget = loc;
}

IPIntGenerator::ControlType WARN_UNUSED_RETURN IPIntGenerator::addTopLevel(BlockSignature signature)
{
    return ControlType(signature, 0, BlockType::TopLevel);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-2);
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBlock(BlockSignature signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    splitStack(signature, oldStack, newStack);
    block = ControlType(signature, m_stackSize.value() - newStack.size(), BlockType::Block);
    block.m_index = controlStructuresAwaitingCoalescing.size();
    block.m_pc = curPC();
    block.m_mc = curMC();
    block.m_pendingOffset = curMC();


    // Register to be coalesced if possible!
    coalesceQueue.append(QueuedCoalesceRequest { controlStructuresAwaitingCoalescing.size(), true });
    controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = block.m_pc,
        .isLoop = false
    });
    ++coalesceDebt;

    tryToResolveEntryTarget(block.m_index, { curPC(), curMC() }, m_metadata->m_metadata.data());
    m_metadata->addBlankSpace<IPInt::BlockMetadata>();

    coalesceControlFlow();

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addLoop(BlockSignature signature, Stack& oldStack, ControlType& block, Stack& newStack, uint32_t loopIndex)
{
    splitStack(signature, oldStack, newStack);
    block = ControlType(signature, m_stackSize.value() - newStack.size(), BlockType::Loop);
    block.m_index = controlStructuresAwaitingCoalescing.size();
    block.m_pendingOffset = -1; // no need to update!
    block.m_pc = curPC();

    // Register to be coalesced if possible!
    controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = block.m_pc,
        .isLoop = true,
        .m_entryResolved = true,
        .m_entryTarget = { curPC(), curMC() }
    });
    ++coalesceDebt;

    IPInt::InstructionLengthMetadata md { static_cast<uint8_t>(getCurrentInstructionLength()) };
    m_metadata->appendMetadata(md);

    // Loop OSR
    ASSERT(m_parser->getStackHeightInValues() + newStack.size() == m_stackSize.value());
    unsigned numOSREntryDataValues = m_stackSize.value();

    // Note the +1: we do this to avoid having 0 as a key in the map, since the current map can't handle 0 as a key
    m_metadata->tierUpCounter().add(m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset + 1, IPIntTierUpCounter::OSREntryData { loopIndex, numOSREntryDataValues, m_tryDepth });

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addIf(ExpressionType, BlockSignature signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    splitStack(signature, oldStack, newStack);
    changeStackSize(-1);
    block = ControlType(signature, m_stackSize.value() - newStack.size(), BlockType::If);
    block.m_index = controlStructuresAwaitingCoalescing.size();
    block.m_pc = curPC();
    block.m_mc = curMC();
    block.m_pendingOffset = m_metadata->m_metadata.size();

    coalesceQueue.append(QueuedCoalesceRequest { controlStructuresAwaitingCoalescing.size(), true });
    controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = block.m_pc,
        .isLoop = false
    });
    ++coalesceDebt;

    IPInt::IfMetadata mdIf {
        .elseDeltaPC = 0xbeef,
        .elseDeltaMC = 0xbeef,
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(mdIf);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addElse(ControlType& block, Stack&)
{
    return addElseToUnreachable(block);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addElseToUnreachable(ControlType& block)
{
    auto blockSignature = block.signature();
    const FunctionSignature& signature = *blockSignature.m_signature;
    m_stackSize = block.stackSize();
    changeStackSize(signature.argumentCount());
    auto ifIndex = block.m_index;

    auto mdIf = reinterpret_cast<IPInt::IfMetadata*>(m_metadata->m_metadata.data() + block.m_pendingOffset);

    // delta PC
    mdIf->elseDeltaPC = nextPC() - block.m_pc;

    // delta MC
    if (m_parser->currentOpcode() == OpType::End) {
        // Edge case: if ... end with no else
        mdIf->elseDeltaMC = curMC() - block.m_mc;
        block = ControlType(block.signature(), block.stackSize(), BlockType::Block);
        block.m_index = ifIndex;
        block.m_pendingOffset = -1;
        block.isElse = true;
        return { };
    }

    // New MC, normal case
    mdIf->elseDeltaMC = safeCast<uint32_t>(curMC() + sizeof(IPInt::BlockMetadata)) - block.m_mc;
    block = ControlType(block.signature(), block.stackSize(), BlockType::Block);
    block.m_index = ifIndex;
    block.m_pc = curPC();
    block.m_mc = curMC();
    block.m_pendingOffset = curMC();
    block.isElse = true;

    m_metadata->addBlankSpace<IPInt::BlockMetadata>();
    return { };
}

// Exception Handling

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTry(BlockSignature signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    m_tryDepth++;
    m_maxTryDepth = std::max(m_maxTryDepth, m_tryDepth.value());

    splitStack(signature, oldStack, newStack);
    block = ControlType(signature, m_stackSize.value() - newStack.size(), BlockType::Try);
    block.m_index = controlStructuresAwaitingCoalescing.size();
    block.m_tryDepth = m_tryDepth;
    block.m_pc = curPC();
    block.m_mc = curMC();

    coalesceQueue.append(QueuedCoalesceRequest { controlStructuresAwaitingCoalescing.size(), true });
    controlStructuresAwaitingCoalescing.append(ControlStructureAwaitingCoalescing {
        .startPC = block.m_pc,
        .isLoop = false
    });
    ++coalesceDebt;

    // FIXME: Should this participate the same skipping that block does?
    // The upside is that we skip a bunch of sequential try/block instructions.
    // The downside is that try needs more metadata.
    // It's not clear that code would want to have many nested try blocks
    // though.
    m_metadata->addLength(getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTryTable(BlockSignature, Stack& enclosingStack, const Vector<CatchHandler>& targets, ControlType& result, Stack& newStack)
{
    UNUSED_PARAM(enclosingStack);
    UNUSED_PARAM(targets);
    UNUSED_PARAM(result);
    UNUSED_PARAM(newStack);
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

void IPIntGenerator::convertTryToCatch(ControlType& tryBlock, CatchKind catchKind)
{
    ASSERT(ControlType::isTry(tryBlock));
    ControlType catchBlock = ControlType(tryBlock.signature(), tryBlock.stackSize(), BlockType::Catch, catchKind);
    catchBlock.m_pc = tryBlock.m_pc;
    catchBlock.m_pcEnd = m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset;
    catchBlock.m_tryDepth = tryBlock.m_tryDepth;

    tryBlock = WTFMove(catchBlock);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCatch(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, Stack&, ControlType& block, ResultList& results)
{

    return addCatchToUnreachable(exceptionIndex, exceptionSignature, block, results);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, ControlType& block, ResultList& results)
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
        static_cast<uint32_t>(block.m_pcEnd),
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCatchAll(Stack&, ControlType& block)
{
    return addCatchAllToUnreachable(block);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCatchAllToUnreachable(ControlType& block)
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
        static_cast<uint32_t>(block.m_pcEnd),
        static_cast<uint32_t>(m_parser->offset() - m_metadata->m_bytecodeOffset),
        static_cast<uint32_t>(m_metadata->m_metadata.size()),
        m_tryDepth,
        0
    });

    // IPInt stack entries are 16 bytes to keep the stack aligned. With the exception of locals, which are only 8 bytes.
    uint32_t stackSizeInV128 = m_stackSize.value() + roundUpToMultipleOf<2>(m_metadata->m_numLocals) / 2;
    IPInt::CatchMetadata mdCatch {
        .stackSizeInV128 = stackSizeInV128
    };
    m_metadata->appendMetadata(mdCatch);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addDelegate(ControlType& target, ControlType& data)
{
    return addDelegateToUnreachable(target, data);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addDelegateToUnreachable(ControlType& target, ControlType& data)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(data);
    // FIXME: If this is actually unreachable we shouldn't need metadata.
    data.m_catchesAwaitingFixup.append({ curPC(), curMC() });
    m_metadata->addBlankSpace<IPInt::BlockMetadata>();

    ASSERT(ControlType::isTry(target) || ControlType::isTopLevel(target));
    unsigned targetDepth = ControlType::isTry(target) ? target.m_tryDepth : 0;

    m_metadata->m_exceptionHandlers.append({
        HandlerType::Delegate,
        static_cast<uint32_t>(data.m_pc),
        static_cast<uint32_t>(data.m_pcEnd),
        static_cast<uint32_t>(m_parser->offset() - m_metadata->m_bytecodeOffset),
        static_cast<uint32_t>(m_metadata->m_metadata.size()),
        m_tryDepth,
        targetDepth
    });

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addThrow(unsigned exceptionIndex, ArgumentList&, Stack&)
{
    IPInt::ThrowMetadata mdThrow {
        .exceptionIndex = safeCast<uint32_t>(exceptionIndex)
    };
    m_metadata->appendMetadata(mdThrow);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRethrow(unsigned, ControlType& catchBlock)
{
    m_usesRethrow = true;

    IPInt::RethrowMetadata mdRethrow {
        .tryDepth = catchBlock.m_tryDepth
    };
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(sizeof(mdRethrow));
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, mdRethrow, IPInt::RethrowMetadata);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addThrowRef(ExpressionType, Stack&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

// Control Flow Branches

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addReturn(const ControlType&, const Stack&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBranch(ControlType& block, ExpressionType, const Stack&)
{
    if (m_parser->currentOpcode() == OpType::BrIf)
        changeStackSize(-1);

    IPIntLocation here = { curPC(), curMC() };

    IPInt::BranchMetadata branch {
        .target = {
            .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
            .toPop = safeCast<uint16_t>(m_stackSize - block.stackSize() - block.branchTargetArity()),
            .toKeep = safeCast<uint16_t>(block.branchTargetArity()),
        },
        .instructionLength = { .length = safeCast<uint8_t>(getCurrentInstructionLength()) }
    };
    m_metadata->appendMetadata(branch);

    tryToResolveBranchTarget(block, here, m_metadata->m_metadata.data());

    return { };
}
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBranchNull(ControlType&, ExpressionType, Stack&, bool, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBranchCast(ControlType&, ExpressionType, Stack&, bool, int32_t, bool) IPINT_UNIMPLEMENTED

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSwitch(ExpressionType, const Vector<ControlType*>& jumps, ControlType& defaultJump, const Stack&)
{
    changeStackSize(-1);
    IPInt::SwitchMetadata mdSwitch {
        .size = safeCast<uint32_t>(jumps.size() + 1),
        .target = { }
    };
    m_metadata->appendMetadata(mdSwitch);

    for (auto block : jumps) {
        IPInt::BranchTargetMetadata target {
            .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
            .toPop = safeCast<uint16_t>(m_stackSize - block->stackSize() - block->branchTargetArity()),
            .toKeep = safeCast<uint16_t>(block->branchTargetArity())
        };
        IPIntLocation here = { curPC(), curMC() };
        tryToResolveBranchTarget(*block, here, m_metadata->m_metadata.data());
        m_metadata->appendMetadata(target);
    }
    IPInt::BranchTargetMetadata defaultTarget {
        .block = { .deltaPC = 0xbeef, .deltaMC = 0xbeef },
        .toPop = safeCast<uint16_t>(m_stackSize - defaultJump.stackSize() - defaultJump.branchTargetArity()),
        .toKeep = safeCast<uint16_t>(defaultJump.branchTargetArity())
    };
    IPIntLocation here = { curPC(), curMC() };
    tryToResolveBranchTarget(defaultJump, here, m_metadata->m_metadata.data());
    m_metadata->appendMetadata(defaultTarget);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::endBlock(ControlEntry& entry, Stack& stack)
{
    return addEndToUnreachable(entry, stack);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addEndToUnreachable(ControlEntry& entry, Stack&)
{
    auto blockSignature = entry.controlData.signature();
    const auto& signature = *blockSignature.m_signature;
    for (unsigned i = 0; i < signature.returnCount(); i ++)
        entry.enclosedExpressionStack.constructAndAppend(signature.returnType(i), Value { });
    auto block = entry.controlData;
    m_stackSize = block.stackSize();
    changeStackSize(signature.returnCount());

    if (ControlType::isTry(block) || ControlType::isAnyCatch(block)) {
        --m_tryDepth;
        exitHandlersAwaitingCoalescing.appendVector(block.m_catchesAwaitingFixup);
    }

    if (ControlType::isTopLevel(block)) {
        // Hit the end
        exitHandlersAwaitingCoalescing.appendVector(jumpLocationsAwaitingEnd);
        coalesceControlFlow(true);

        // Metadata = round up 8 bytes, one for each
        m_metadata->m_bytecode = m_metadata->m_bytecode.first(m_parser->offset());
        return { };
    }

    if (ControlType::isIf(block)) {
        exitHandlersAwaitingCoalescing.append({ block.m_pc, block.m_mc });
    } else if (ControlType::isBlock(block)) {
        if (block.isElse) {
            // if it's not an if ... end, coalesce
            if (block.m_pendingOffset != -1)
                exitHandlersAwaitingCoalescing.append({ block.m_pc, block.m_mc });
            coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
            --coalesceDebt;
        } else {
            // block
            coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
            --coalesceDebt;
        }
    } else if (ControlType::isLoop(block)) {
        coalesceQueue.append({ static_cast<unsigned>(block.m_index), false });
        --coalesceDebt;
    }

    // mark pending exit targets to be resolved
    // any pending branch targets must be blocks because a loop would've been resolved. if it's loop, end then there's nobody
    // asking for the target

    coalesceControlFlow();
    return { };
}

auto IPIntGenerator::endTopLevel(BlockSignature signature, const Stack& expressionStack) -> PartialResult
{
    if (m_usesSIMD)
        m_info.markUsesSIMD(m_metadata->functionIndex());
    RELEASE_ASSERT(expressionStack.size() == signature.m_signature->returnCount());
    m_info.doneSeeingFunction(m_metadata->m_functionIndex);
    return { };
}

// Calls
static constexpr unsigned fprToIndex(FPRReg r)
{
    for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i) {
        if (FPRInfo::toArgumentRegister(i) == r)
            return i;
    }
    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return 0;
}

void IPIntGenerator::addCallCommonData(const FunctionSignature& signature)
{
    CallInformation callConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    uint16_t stackArgs = 0;

    constexpr static int NUM_MINT_CALL_GPRS = 8;
    constexpr static int NUM_MINT_CALL_FPRS = 8;
    ASSERT_UNUSED(NUM_MINT_CALL_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_MINT_CALL_GPRS);
    ASSERT_UNUSED(NUM_MINT_CALL_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_MINT_CALL_FPRS);

    Vector<uint8_t, 16> mINTBytecode;
    mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::Call));
    for (size_t i = 0; i < signature.argumentCount(); ++i) {
        auto loc = callConvention.params[i].location;
        if (loc.isGPR()) {
            ASSERT_UNUSED(NUM_MINT_CALL_GPRS, loc.jsr().payloadGPR() < NUM_MINT_CALL_GPRS);
            mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentGPR) + loc.jsr().payloadGPR());
        } else if (loc.isFPR()) {
            ASSERT_UNUSED(NUM_MINT_CALL_FPRS, fprToIndex(loc.fpr()) < NUM_MINT_CALL_FPRS);
            mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentFPR) + fprToIndex(loc.fpr()));
        } else if (loc.isStackArgument()) {
            if (stackArgs++ & 1)
                mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentStackUnaligned));
            else
                mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentStackAligned));
        } else
            RELEASE_ASSERT_NOT_REACHED();
    }
    if (stackArgs & 1) {
        ++stackArgs;
        mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::StackAlign));
    }

    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(mINTBytecode.size());
    auto data = m_metadata->m_metadata.data() + size;
    while (!mINTBytecode.isEmpty()) {
        WRITE_TO_METADATA(data, mINTBytecode.last(), uint8_t);
        data += 1;
        mINTBytecode.removeLast();
    }

    IPInt::CallReturnMetadata commonReturn {
        .stackSlots = safeCast<uint16_t>(stackArgs), // stackArgs is already padded
        .argumentCount = safeCast<uint16_t>(signature.argumentCount()),
        .resultBytecode = { }
    };
    m_metadata->appendMetadata(commonReturn);

    mINTBytecode.clear();

    CallInformation returnConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);

    constexpr static int NUM_MINT_RET_GPRS = 8;
    constexpr static int NUM_MINT_RET_FPRS = 8;
    ASSERT_UNUSED(NUM_MINT_RET_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_MINT_RET_GPRS);
    ASSERT_UNUSED(NUM_MINT_RET_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_MINT_RET_FPRS);

    for (size_t i = 0; i < signature.returnCount(); ++i) {
        auto loc = returnConvention.results[i].location;
        if (loc.isGPR()) {
            ASSERT_UNUSED(NUM_MINT_RET_GPRS, loc.jsr().payloadGPR() < NUM_MINT_RET_GPRS);
            mINTBytecode.append(static_cast<uint8_t>(IPInt::CallResultBytecode::ResultGPR) + loc.jsr().payloadGPR());
        } else if (loc.isFPR()) {
            ASSERT_UNUSED(NUM_MINT_RET_FPRS, fprToIndex(loc.fpr()) < NUM_MINT_RET_FPRS);
            mINTBytecode.append(static_cast<uint8_t>(IPInt::CallResultBytecode::ResultFPR) + fprToIndex(loc.fpr()));
        } else if (loc.isStackArgument())
            mINTBytecode.append(static_cast<uint8_t>(IPInt::CallResultBytecode::ResultStack));
        else
            RELEASE_ASSERT_NOT_REACHED();
    }
    mINTBytecode.append(static_cast<uint8_t>(IPInt::CallResultBytecode::End));

    size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(mINTBytecode.size());
    data = m_metadata->m_metadata.data() + size;
    memcpy(data, mINTBytecode.data(), mINTBytecode.size());
}

void IPIntGenerator::addTailCallCommonData(const FunctionSignature& signature)
{
    CallInformation callConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    uint16_t stackArgs = 0;

    constexpr static int NUM_MINT_CALL_GPRS = 8;
    constexpr static int NUM_MINT_CALL_FPRS = 8;
    ASSERT_UNUSED(NUM_MINT_CALL_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_MINT_CALL_GPRS);
    ASSERT_UNUSED(NUM_MINT_CALL_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_MINT_CALL_FPRS);

    Vector<uint8_t, 16> mINTBytecode;
    mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::TailCall));
    for (size_t i = 0; i < signature.argumentCount(); ++i) {
        auto loc = callConvention.params[i].location;
        if (loc.isGPR()) {
            ASSERT_UNUSED(NUM_MINT_CALL_GPRS, loc.jsr().payloadGPR() < NUM_MINT_CALL_GPRS);
            mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentGPR) + loc.jsr().payloadGPR());
        } else if (loc.isFPR()) {
            ASSERT_UNUSED(NUM_MINT_CALL_FPRS, fprToIndex(loc.fpr()) < NUM_MINT_CALL_FPRS);
            mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentFPR) + fprToIndex(loc.fpr()));
        } else if (loc.isStackArgument()) {
            if (stackArgs++ & 1)
                mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::TailArgumentStackUnaligned));
            else
                mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::TailArgumentStackAligned));
        } else
            RELEASE_ASSERT_NOT_REACHED();
    }
    if (stackArgs & 1) {
        ++stackArgs;
        mINTBytecode.append(static_cast<uint8_t>(IPInt::CallArgumentBytecode::TailStackAlign));
    }

    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(mINTBytecode.size());
    auto data = m_metadata->m_metadata.data() + size;
    while (!mINTBytecode.isEmpty()) {
        WRITE_TO_METADATA(data, mINTBytecode.last(), uint8_t);
        data += 1;
        mINTBytecode.removeLast();
    }
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCall(FunctionSpaceIndex index, const TypeDefinition& type, ArgumentList&, ResultList& results, CallType callType)
{
    const FunctionSignature& signature = *type.as<FunctionSignature>();
    if (callType == CallType::TailCall) {
        // on a tail call, we need to:
        // roll back to old SP, shift SP to accommodate arguments
        // put arguments into registers / sp (reutilize mINT)
        // jump to entrypoint
        changeStackSize(-signature.argumentCount());
        const auto& callingConvention = wasmCallingConvention();

        const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
        const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex).expand();
        uint32_t callerStackArgs = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), callingConvention.numberOfStackValues(*callerTypeDefinition.as<FunctionSignature>()));

        IPInt::TailCallMetadata functionIndexMetadata {
            .length = safeCast<uint8_t>(getCurrentInstructionLength()),
            .functionIndex = index,
            .callerStackArgSize = static_cast<int32_t>(callerStackArgs * sizeof(Register)),
            .argumentBytecode = { }
        };
        m_metadata->appendMetadata(functionIndexMetadata);
        addTailCallCommonData(signature);
    } else {
        for (unsigned i = 0; i < signature.returnCount(); i ++)
            results.append(Value { });
        changeStackSize(signature.returnCount() - signature.argumentCount());
        if (!m_info.isImportedFunctionFromFunctionIndexSpace(index))
            m_metadata->m_callees.testAndSet(index - m_info.importFunctionCount());

        IPInt::CallMetadata functionIndexMetadata {
            .length = safeCast<uint8_t>(getCurrentInstructionLength()),
            .functionIndex = index,
            .argumentBytecode = { }
        };
        m_metadata->appendMetadata(functionIndexMetadata);
        addCallCommonData(signature);
    }
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCallIndirect(unsigned tableIndex, const TypeDefinition& type, ArgumentList&, ResultList& results, CallType callType)
{
    const FunctionSignature& signature = *type.as<FunctionSignature>();
    if (callType == CallType::TailCall) {
        const unsigned callIndex = 1;
        changeStackSize(-signature.argumentCount() - callIndex);
        // on a tail call, we need to:
        // roll back to old SP, shift SP to accommodate arguments
        // put arguments into registers / sp (reutilize mINT)
        // jump to entrypoint
        const auto& callingConvention = wasmCallingConvention();

        const TypeIndex callerTypeIndex = m_info.internalFunctionTypeIndices[m_functionIndex];
        const TypeDefinition& callerTypeDefinition = TypeInformation::get(callerTypeIndex).expand();
        uint32_t callerStackArgs = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), callingConvention.numberOfStackValues(*callerTypeDefinition.as<FunctionSignature>()));

        IPInt::TailCallIndirectMetadata functionIndexMetadata {
            .length = safeCast<uint8_t>(getCurrentInstructionLength()),
            .tableIndex = tableIndex,
            .typeIndex = m_metadata->addSignature(type),
            .callerStackArgSize = static_cast<int32_t>(callerStackArgs * sizeof(Register)),
            .argumentBytecode = { }
        };
        m_metadata->appendMetadata(functionIndexMetadata);
        addTailCallCommonData(signature);
    } else {
        const FunctionSignature& signature = *type.as<FunctionSignature>();
        for (unsigned i = 0; i < signature.returnCount(); i ++)
            results.append(Value { });
        const unsigned callIndex = 1;
        changeStackSize(signature.returnCount() - signature.argumentCount() - callIndex);

        IPInt::CallIndirectMetadata functionIndexMetadata {
            .length = safeCast<uint8_t>(getCurrentInstructionLength()),
            .tableIndex = tableIndex,
            .typeIndex = m_metadata->addSignature(type),
            .argumentBytecode = { }
        };
        m_metadata->appendMetadata(functionIndexMetadata);

        addCallCommonData(signature);
    }
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCallRef(const TypeDefinition& type, ArgumentList&, ResultList& results, CallType)
{
    const FunctionSignature& signature = *type.as<FunctionSignature>();
    for (unsigned i = 0; i < signature.returnCount(); i ++)
        results.append(Value { });
    const unsigned callRef = 1;
    changeStackSize(signature.returnCount() - signature.argumentCount() - callRef);
    return { };
}

// Traps

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addUnreachable()
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCrash()
{
    return { };
}

// Finalize

std::unique_ptr<FunctionIPIntMetadataGenerator> IPIntGenerator::finalize()
{
    if (m_usesRethrow) {
        m_metadata->m_numAlignedRethrowSlots = roundUpToMultipleOf<2>(m_maxTryDepth);
        for (uint32_t catchSPOffset : m_catchSPMetadataOffsets)
            *reinterpret_cast_ptr<uint32_t*>(m_metadata->m_metadata.data() + catchSPOffset) += m_metadata->m_numAlignedRethrowSlots;
    }

    m_metadata->m_maxFrameSizeInV128 = roundUpToMultipleOf<2>(m_metadata->m_numLocals) / 2;
    m_metadata->m_maxFrameSizeInV128 += m_metadata->m_numAlignedRethrowSlots / 2;
    m_metadata->m_maxFrameSizeInV128 += m_maxStackSize;

    return WTFMove(m_metadata);
}

Expected<std::unique_ptr<FunctionIPIntMetadataGenerator>, String> parseAndCompileMetadata(std::span<const uint8_t> function, const TypeDefinition& signature, ModuleInformation& info, FunctionCodeIndex functionIndex)
{
    IPIntGenerator generator(info, functionIndex, signature, function);
    FunctionParser<IPIntGenerator> parser(generator, function, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());
    return generator.finalize();
}

void IPIntGenerator::dump(const ControlStack&, const Stack*)
{
    dataLogLn("PC: ", m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset, " MC: ", m_metadata->m_metadata.size());
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
