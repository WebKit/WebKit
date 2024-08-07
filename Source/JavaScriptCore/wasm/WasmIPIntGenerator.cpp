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
#include <wtf/text/MakeString.h>

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

namespace JSC { namespace Wasm {

using ErrorType = String;
using PartialResult = Expected<void, ErrorType>;
using UnexpectedResult = Unexpected<ErrorType>;
struct Value { };

// ControlBlock

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
            return m_signature->argumentType(i);
        return m_signature->returnType(i);
    }

    unsigned branchTargetArity() const
    {
        return isLoop(*this)
            ? m_signature->argumentCount()
            : m_signature->returnCount();
    }

private:
    BlockSignature m_signature;
    BlockType m_blockType;
    CatchKind m_catchKind;

    Vector<uint32_t> m_awaitingUpdate;
    int32_t m_pendingOffset { -1 };
    int32_t m_pc { -1 };
    int32_t m_mc { -1 };
    int32_t m_pcEnd { -1 };
    uint32_t m_stackSize { 0 };
    uint32_t m_tryDepth { 0 };
};

class IPIntGenerator {
public:
    IPIntGenerator(ModuleInformation&, unsigned, const TypeDefinition&, std::span<const uint8_t>);

    using ControlType = IPIntControlType;
    using ExpressionType = Value;
    using CallType = CallLinkInfo::CallType;
    using ResultList = Vector<Value, 8>;

    using ExpressionList = Vector<Value, 1>;
    using ControlEntry = FunctionParser<IPIntGenerator>::ControlEntry;
    using ControlStack = FunctionParser<IPIntGenerator>::ControlStack;
    using Stack = FunctionParser<IPIntGenerator>::Stack;
    using TypedExpression = FunctionParser<IPIntGenerator>::TypedExpression;

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
    PartialResult WARN_UNUSED_RETURN addRefFunc(uint32_t, ExpressionType&);
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
    PartialResult WARN_UNUSED_RETURN addArrayNewFixed(uint32_t, Vector<ExpressionType>&, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayGet(ExtGCOpType, uint32_t, ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayLen(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addArrayFill(uint32_t, ExpressionType, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayCopy(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayInitElem(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addArrayInitData(uint32_t, ExpressionType, ExpressionType, uint32_t, ExpressionType, ExpressionType);
    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t, Vector<ExpressionType>&, ExpressionType&);
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

    void condenseControlFlowInstructions();

    ControlType WARN_UNUSED_RETURN addTopLevel(BlockSignature);
    PartialResult WARN_UNUSED_RETURN addBlock(BlockSignature, Stack&, ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addLoop(BlockSignature, Stack&, ControlType&, Stack&, uint32_t);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType, BlockSignature, Stack&, ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addElse(ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlType&);

    PartialResult WARN_UNUSED_RETURN addTry(BlockSignature, Stack&, ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned, const TypeDefinition&, Stack&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned, const TypeDefinition&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned, Vector<ExpressionType>&, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlType&, const Stack&);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlType&, ExpressionType, const Stack&);
    PartialResult WARN_UNUSED_RETURN addBranchNull(ControlType&, ExpressionType, Stack&, bool, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN addBranchCast(ControlType&, ExpressionType, Stack&, bool, int32_t, bool);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType, const Vector<ControlType*>&, ControlType&, const Stack&);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack&);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, Stack&);

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&);

    // Calls

    PartialResult WARN_UNUSED_RETURN addCall(uint32_t, const TypeDefinition&, Vector<ExpressionType>&, ResultList&, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned, const TypeDefinition&, Vector<ExpressionType>&, ResultList&, CallType = CallType::Call);
    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, Vector<ExpressionType>&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN addCrash();

    void setParser(FunctionParser<IPIntGenerator>* parser) { m_parser = parser; };
    size_t getCurrentInstructionLength()
    {
        return m_parser->offset() - m_parser->currentOpcodeStartingOffset();
    }
    void addCallCommonData(const FunctionSignature&);
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

    static constexpr bool tierSupportsSIMD = true;
private:
    Checked<uint32_t> m_stackSize { 0 };
    uint32_t m_maxStackSize { 0 };
    Checked<uint32_t> m_tryDepth { 0 };
    uint32_t m_maxTryDepth { 0 };
    FunctionParser<IPIntGenerator>* m_parser { nullptr };
    ModuleInformation& m_info;
    std::unique_ptr<FunctionIPIntMetadataGenerator> m_metadata;

    // FIXME: If rethrow is not used in practice we should consider just reparsing the function to update the SP offsets.
    Vector<uint32_t> m_catchSPMetadataOffsets;

    bool m_usesRethrow { false };
    bool m_usesSIMD { false };
};

// use if (true) to avoid warnings.
#define IPINT_UNIMPLEMENTED { if (true) { CRASH(); } return { }; }

IPIntGenerator::IPIntGenerator(ModuleInformation& info, unsigned functionIndex, const TypeDefinition&, std::span<const uint8_t> bytecode)
    : m_info(info)
    , m_metadata(WTF::makeUnique<FunctionIPIntMetadataGenerator>(functionIndex, bytecode))
{
    UNUSED_PARAM(info);
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

IPIntGenerator::ExpressionType IPIntGenerator::addConstant(v128_t value)
{
    changeStackSize(1);
    m_metadata->addLEB128V128Constant(value, getCurrentInstructionLength());
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRefFunc(uint32_t index, ExpressionType&)
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
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(9);
    auto tableInitData = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(tableInitData, elementIndex, uint32_t);
    WRITE_TO_METADATA(tableInitData + 4, tableIndex, uint32_t);
    WRITE_TO_METADATA(tableInitData + 8, getCurrentInstructionLength(), uint8_t);
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
    m_metadata->addLEB128ConstantInt32AndLength(tableIndex, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableFill(unsigned tableIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    m_metadata->addLEB128ConstantInt32AndLength(tableIndex, getCurrentInstructionLength());
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType, ExpressionType, ExpressionType)
{
    changeStackSize(-3);
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(9);
    auto tableInitData = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(tableInitData, dstTableIndex, uint32_t);
    WRITE_TO_METADATA(tableInitData + 4, srcTableIndex, uint32_t);
    WRITE_TO_METADATA(tableInitData + 8, getCurrentInstructionLength(), uint8_t);
    return { };
}

// Locals and Globals

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArguments(const TypeDefinition &signature)
{
    auto sig = signature.as<FunctionSignature>();
    auto numArgs = sig->argumentCount();
    m_metadata->m_numLocals += numArgs;
    m_metadata->m_numArguments = numArgs;
    m_metadata->m_argumINTBytecode.resize(numArgs + 1);

    int numGPR = 0;
    int numFPR = 0;

    // 0x00 - 0x07: GPR 0-7
    // 0x08 - 0x0b: FPR 0-3
    // 0x0c: stack
    // 0x0d: end

    for (size_t i = 0; i < numArgs; ++i) {
        auto arg = sig->argumentType(i);
        if (arg.isI32() || arg.isI64()) {
            if (numGPR < 8)
                m_metadata->m_argumINTBytecode[i] = numGPR++;
            else {
                m_metadata->m_numArgumentsOnStack++;
                m_metadata->m_argumINTBytecode[i] = 0x0c;
            }
        } else {
            if (numFPR < 8)
                m_metadata->m_argumINTBytecode[i] = 8 + numFPR++;
            else {
                m_metadata->m_numArgumentsOnStack++;
                m_metadata->m_argumINTBytecode[i] = 0x0c;
            }
        }
    }
    m_metadata->m_argumINTBytecode.last() = 0x0d;
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
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(8);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, index, uint32_t);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 4, getCurrentInstructionLength(), uint16_t);
    const Wasm::GlobalInformation& global = m_info.globals[index];
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 6, 0, uint16_t);
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 6, 1, uint16_t);
        break;
    }
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::setGlobal(uint32_t index, ExpressionType)
{
    changeStackSize(-1);
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(8);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, index, uint32_t);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 4, getCurrentInstructionLength(), uint16_t);
    const Wasm::GlobalInformation& global = m_info.globals[index];
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 6, 0, uint8_t);
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 6, 1, uint8_t);
        break;
    }
    if (isRefType(m_info.globals[index].type))
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 7, 1, uint8_t);
    else
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 7, 0, uint8_t);
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
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addArrayNewFixed(uint32_t, Vector<ExpressionType>&, ExpressionType&) IPINT_UNIMPLEMENTED
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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addStructNew(uint32_t, Vector<ExpressionType>&, ExpressionType&) IPINT_UNIMPLEMENTED
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

IPIntGenerator::ControlType WARN_UNUSED_RETURN IPIntGenerator::addTopLevel(BlockSignature signature)
{
    return ControlType(signature, 0, BlockType::TopLevel);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&)
{
    changeStackSize(-2);
    m_metadata->addRawValue(getCurrentInstructionLength());
    return { };
}

inline void IPIntGenerator::condenseControlFlowInstructions()
{
    // Peek at the next instruction: if it's not a block, go through and resolve all the metadata entries
    auto nextOpcode = m_metadata->m_bytecode[m_parser->offset()];
    if (nextOpcode != OpType::Block) {
        // next PC (to skip type signature)
        for (auto offset : m_metadata->m_repeatedControlFlowInstructionMetadataOffsets) {
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + offset, m_parser->offset() - m_metadata->m_bytecodeOffset, uint32_t);
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + offset + 4, m_metadata->m_metadata.size(), uint32_t);
        }
        m_metadata->m_repeatedControlFlowInstructionMetadataOffsets.clear();
    }
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBlock(BlockSignature signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    splitStack(signature, oldStack, newStack);
    block = ControlType(signature, m_stackSize.value() - newStack.size(), BlockType::Block);

    // Allocate space in metadata
    m_metadata->m_repeatedControlFlowInstructionMetadataOffsets.append(m_metadata->m_metadata.size());
    m_metadata->addBlankSpace(8);
    condenseControlFlowInstructions();
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addLoop(BlockSignature signature, Stack& oldStack, ControlType& block, Stack& newStack, uint32_t loopIndex)
{
    splitStack(signature, oldStack, newStack);
    block = ControlType(signature, m_stackSize.value() - newStack.size(), BlockType::Loop);
    block.m_pendingOffset = -1; // no need to update!

    // Allocate space in metadata
    auto size = m_metadata->m_metadata.size();
    block.m_pc = m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset;
    block.m_mc = size;
    m_metadata->addBlankSpace(1);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, getCurrentInstructionLength(), uint8_t);

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
    block.m_pendingOffset = m_metadata->m_metadata.size();
    // 4B PC of else
    // 4B MC of else
    auto length = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(9);
    // 1B instruction length
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + length + 8, getCurrentInstructionLength(), uint8_t);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addElse(ControlType& block, Stack& stack)
{
    const FunctionSignature& signature = *block.signature();
    stack.clear();
    for (unsigned i = 0; i < signature.argumentCount(); i ++)
        stack.constructAndAppend(signature.argumentType(i), Value { });
    return addElseToUnreachable(block);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addElseToUnreachable(ControlType& block)
{
    const FunctionSignature& signature = *block.signature();
    ASSERT(block.stackSize() == m_parser->getControlEntryStackHeightInValues());
    m_stackSize = block.stackSize();
    changeStackSize(signature.argumentCount());

    // New PC
    // size - 1 for index of last element
    // - bytecodeOffset since we index starting there in IPInt
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + block.m_pendingOffset, m_parser->offset() - m_metadata->m_bytecodeOffset, uint32_t);
    // New MC
    if (m_parser->currentOpcode() == OpType::End) {
        // Edge case: if ... end with no else: don't actually add in this metadata or else IPInt tries to read the else
        // New MC
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + block.m_pendingOffset + 4, m_metadata->m_metadata.size(), uint32_t);
        block = ControlType(block.signature(), block.stackSize(), BlockType::Block);
        block.m_pendingOffset = -1;
        return { };
    }
    // New MC
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + block.m_pendingOffset + 4, m_metadata->m_metadata.size() + 8, uint32_t);
    block = ControlType(block.signature(), block.stackSize(), BlockType::Block);
    block.m_pendingOffset = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(8);
    return { };
}

// Exception Handling

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addTry(BlockSignature signature, Stack& oldStack, ControlType& block, Stack& newStack)
{
    m_tryDepth++;
    m_maxTryDepth = std::max(m_maxTryDepth, m_tryDepth.value());

    splitStack(signature, oldStack, newStack);
    block = ControlType(signature, m_stackSize.value() - newStack.size(), BlockType::Try);
    block.m_pc = m_parser->currentOpcodeStartingOffset() - m_metadata->m_bytecodeOffset;
    block.m_tryDepth = m_tryDepth;

    // FIXME: Should this participate the same skipping that block does?
    // The upside is that we skip a bunch of sequential try/block instructions.
    // The downside is that try needs more metadata.
    // It's not clear that code would want to have many nested try blocks
    // though.
    m_metadata->addLength(getCurrentInstructionLength());
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
    block.m_awaitingUpdate.append(m_metadata->m_metadata.size());
    m_metadata->addBlankSpace(8);

    m_metadata->m_exceptionHandlers.append({
        HandlerType::Catch,
        static_cast<uint32_t>(block.m_pc),
        static_cast<uint32_t>(block.m_pcEnd),
        static_cast<uint32_t>(m_parser->offset() - m_metadata->m_bytecodeOffset),
        static_cast<uint32_t>(m_metadata->m_metadata.size()),
        m_tryDepth,
        exceptionIndex
    });

    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(4);
    // IPInt stack entries are 16 bytes to keep the stack aligned. With the exception of locals, which are only 8 bytes.
    uint32_t stackSizeInV128 = m_stackSize.value() + roundUpToMultipleOf<2>(m_metadata->m_numLocals) / 2;
    m_catchSPMetadataOffsets.append(size);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, stackSizeInV128, uint32_t);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCatchAll(Stack&, ControlType& block)
{
    return addCatchAllToUnreachable(block);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCatchAllToUnreachable(ControlType& block)
{
    if (ControlType::isTry(block))
        convertTryToCatch(block, CatchKind::CatchAll);
    else
        block.m_catchKind = CatchKind::CatchAll;

    ASSERT(block.stackSize() == m_parser->getControlEntryStackHeightInValues());
    m_stackSize = block.stackSize();

    // FIXME: If this is actually unreachable we shouldn't need metadata.
    block.m_awaitingUpdate.append(m_metadata->m_metadata.size());
    m_metadata->addBlankSpace(8);

    m_metadata->m_exceptionHandlers.append({
        HandlerType::CatchAll,
        static_cast<uint32_t>(block.m_pc),
        static_cast<uint32_t>(block.m_pcEnd),
        static_cast<uint32_t>(m_parser->offset() - m_metadata->m_bytecodeOffset),
        static_cast<uint32_t>(m_metadata->m_metadata.size()),
        m_tryDepth,
        0
    });

    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(4);
    // IPInt stack entries are 16 bytes to keep the stack aligned. With the exception of locals, which are only 8 bytes.
    uint32_t stackSizeInV128 = m_stackSize.value() + roundUpToMultipleOf<2>(m_metadata->m_numLocals) / 2;
    m_catchSPMetadataOffsets.append(size);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, stackSizeInV128, uint32_t);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addDelegate(ControlType& target, ControlType& data)
{
    return addDelegateToUnreachable(target, data);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addDelegateToUnreachable(ControlType& target, ControlType& data)
{
    // FIXME: If this is actually unreachable we shouldn't need metadata.
    auto size = m_metadata->m_metadata.size();
    data.m_awaitingUpdate.append(size);
    m_metadata->addBlankSpace(8);

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

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addThrow(unsigned exceptionIndex, Vector<ExpressionType>&, Stack&)
{
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(4);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, exceptionIndex, uint32_t);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addRethrow(unsigned, ControlType& catchBlock)
{
    m_usesRethrow = true;

    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(4);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, catchBlock.m_tryDepth, uint32_t);

    return { };
}

// Control Flow Branches

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addReturn(const ControlType&, const Stack&)
{
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBranch(ControlType& block, ExpressionType, const Stack& stack)
{
    if (m_parser->currentOpcode() == OpType::BrIf)
        changeStackSize(-1);

    auto size = m_metadata->m_metadata.size();
    block.m_awaitingUpdate.append(size);
    m_metadata->addBlankSpace(13);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 8, stack.size() - block.branchTargetArity(), uint16_t);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 10, block.branchTargetArity(), uint16_t);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size + 12, getCurrentInstructionLength(), uint8_t);
    return { };
}
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBranchNull(ControlType&, ExpressionType, Stack&, bool, ExpressionType&) IPINT_UNIMPLEMENTED
PartialResult WARN_UNUSED_RETURN IPIntGenerator::addBranchCast(ControlType&, ExpressionType, Stack&, bool, int32_t, bool) IPINT_UNIMPLEMENTED

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addSwitch(ExpressionType, const Vector<ControlType*>& jumps, ControlType& defaultJump, const Stack& stack)
{
    auto size = m_metadata->m_metadata.size();
    // Metadata layout
    // 0 - 3     number of jump targets (including end)
    // 8 - 15    4B PC for t0, 4B MC for t0
    // 16 - 19   2B pop, 2B keep
    // 20 and on repeat for each branch target
    m_metadata->addBlankSpace(4);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + size, jumps.size() + 1, uint32_t);

    for (auto block : jumps) {
        auto jumpBase = m_metadata->m_metadata.size();
        m_metadata->addBlankSpace(12);
        block->m_awaitingUpdate.append(jumpBase);
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + jumpBase + 8, stack.size() - block->branchTargetArity(), uint16_t);
        WRITE_TO_METADATA(m_metadata->m_metadata.data() + jumpBase + 10, block->branchTargetArity(), uint16_t);
    }
    auto defaultJumpBase = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(12);
    defaultJump.m_awaitingUpdate.append(defaultJumpBase);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + defaultJumpBase + 8, stack.size() - defaultJump.branchTargetArity(), uint16_t);
    WRITE_TO_METADATA(m_metadata->m_metadata.data() + defaultJumpBase + 10, defaultJump.branchTargetArity(), uint16_t);

    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::endBlock(ControlEntry& entry, Stack& stack)
{
    return addEndToUnreachable(entry, stack);
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addEndToUnreachable(ControlEntry& entry, Stack&)
{
    const FunctionSignature& signature = *entry.controlData.signature();
    for (unsigned i = 0; i < signature.returnCount(); i ++)
        entry.enclosedExpressionStack.constructAndAppend(signature.returnType(i), Value { });
    auto block = entry.controlData;
    m_stackSize = block.stackSize();
    changeStackSize(signature.returnCount());

    if (ControlType::isTry(block) || ControlType::isAnyCatch(block))
        --m_tryDepth;

    if (ControlType::isTopLevel(block)) {
        // Hit the end
        // Resolve all condensing ends to jump here
        for (auto x : m_metadata->m_repeatedControlFlowInstructionMetadataOffsets) {
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x, m_parser->offset() - m_metadata->m_bytecodeOffset - 1, uint32_t);
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x + 4, m_metadata->m_metadata.size(), uint32_t);
        }
        m_metadata->m_repeatedControlFlowInstructionMetadataOffsets.clear();

        // Final end
        for (auto x : block.m_awaitingUpdate) {
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x, m_parser->offset() - m_metadata->m_bytecodeOffset - 1, uint32_t);
            // New MC
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x + 4, m_metadata->m_metadata.size(), uint32_t);
        }

        // Metadata = round up 8 bytes, one for each
        m_metadata->m_bytecode = m_metadata->m_bytecode.first(m_parser->offset());
        Vector<Type, 16> types(entry.controlData.branchTargetArity());
        for (size_t i = 0; i < entry.controlData.branchTargetArity(); ++i)
            types[i] = entry.controlData.branchTargetType(i);
        m_metadata->addReturnData(types);
    }

    // if, else, block: set metadata of prior instruction to current location
    // if: jump forward for not taken
    // else: jump forward for if taken
    // block: jump forward for br inside
    if (ControlType::isIf(block)) {
        // if .. end
        m_metadata->m_repeatedControlFlowInstructionMetadataOffsets.append(entry.controlData.m_pendingOffset);
    } else if (ControlType::isBlock(block) || ControlType::isAnyCatch(block) || ControlType::isTry(block)) {
        if (block.m_pendingOffset != -1) {
            // (if..) else .. end
            m_metadata->m_repeatedControlFlowInstructionMetadataOffsets.append(entry.controlData.m_pendingOffset);
        } else {
            // If it's a block or catch or try (delegate), resolve all the jumps
            for (auto x : block.m_awaitingUpdate) {
                m_metadata->m_repeatedControlFlowInstructionMetadataOffsets.append(x);
            }
        }
    } else if (ControlType::isLoop(block)) {
        for (auto x : block.m_awaitingUpdate) {
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x, block.m_pc, uint32_t);
            // New MC
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x + 4, block.m_mc, uint32_t);
        }
    }

    if (m_metadata->m_bytecode[m_parser->offset()] != OpType::End) {
        for (auto x : m_metadata->m_repeatedControlFlowInstructionMetadataOffsets) {
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x, m_parser->offset() - m_metadata->m_bytecodeOffset - 1, uint32_t);
            WRITE_TO_METADATA(m_metadata->m_metadata.data() + x + 4, m_metadata->m_metadata.size(), uint32_t);
        }
        m_metadata->m_repeatedControlFlowInstructionMetadataOffsets.clear();
    }

    return { };
}

auto IPIntGenerator::endTopLevel(BlockSignature signature, const Stack& expressionStack) -> PartialResult
{
    if (m_usesSIMD)
        m_info.markUsesSIMD(m_metadata->functionIndex());
    RELEASE_ASSERT(expressionStack.size() == signature->returnCount());
    m_info.doneSeeingFunction(m_metadata->m_functionIndex);
    return { };
}

// Calls

void IPIntGenerator::addCallCommonData(const FunctionSignature& signature)
{
    // Metadata structure for calls:
    // mINT: mini-interpreter / minimalist interpreter (whichever floats your boat)
    //
    // 0x00 - 0x07: push into a0, a1, ...
    // 0x08 - 0x0b: push into fa0, fa1, ...
    // 0x0c: pop stack value, push onto stack[0]
    // 0x0d: pop stack value, add another 16B for params, push onto stack[8]
    // 0x0e: add another 16B for params
    // 0x0f: stop

#if CPU(ARM64)
    const uint8_t gprs = 8;
    const uint8_t fprs = 4;
#elif CPU(X86_64)
    const uint8_t gprs = 4;
    const uint8_t fprs = 4;
#elif CPU(ARM)
    const uint8_t gprs = 4;
    const uint8_t fprs = 2;
#else
    const uint8_t gprs = 0;
    const uint8_t fprs = 0;
    UNUSED_PARAM(signature);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif

    uint8_t gprsUsed = 0;
    uint8_t fprsUsed = 0;
    uint16_t stackArgs = 0;

    Vector<uint8_t, 16> minINTBytecode;
    minINTBytecode.append(0x0f);
    for (size_t i = 0; i < signature.argumentCount(); ++i) {
        auto type = signature.argumentType(i);
        if ((type.isI32() || type.isI64()) && gprsUsed != gprs)
            minINTBytecode.append(gprsUsed++);
        else if ((type.isF32() || type.isF64()) && fprsUsed != fprs)
            minINTBytecode.append(8 + fprsUsed++);
        else {
            minINTBytecode.append(0x0c + (stackArgs & 1));
            ++stackArgs;
        }
    }

    if (stackArgs & 1)
        minINTBytecode.append(0x0e);
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(minINTBytecode.size());
    auto data = m_metadata->m_metadata.data() + size;
    while (!minINTBytecode.isEmpty()) {
        WRITE_TO_METADATA(data, minINTBytecode.last(), uint8_t);
        data += 1;
        minINTBytecode.removeLast();
    }

    // Returns:
    // 2B for number of arguments on stack (to clean up current call frame)
    // 2B for number of arguments (to take off arguments)
    // 0x00 - 0x07: r0 - r7
    // 0x08 - 0x0b: fr0 - fr3
    // 0x0c: stack
    // 0x0d: end
    size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(4);
    data = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(data, (stackArgs + 1) & (-2), uint16_t);
    WRITE_TO_METADATA(data + 2, signature.argumentCount(), uint16_t);

    minINTBytecode.clear();

    gprsUsed = 0;
    fprsUsed = 0;

    for (size_t i = 0; i < signature.returnCount(); ++i) {
        auto type = signature.returnType(i);
        if ((type.isI32() || type.isI64()) && gprsUsed != gprs)
            minINTBytecode.append(gprsUsed++);
        else if ((type.isF32() || type.isF64()) && fprsUsed != fprs)
            minINTBytecode.append(8 + fprsUsed++);
        else
            minINTBytecode.append(0x0c);
    }
    minINTBytecode.append(0x0d);

    size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(minINTBytecode.size());
    data = m_metadata->m_metadata.data() + size;
    for (auto i : minINTBytecode) {
        WRITE_TO_METADATA(data, i, uint8_t);
        ++data;
    }
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCall(uint32_t index, const TypeDefinition& type, Vector<ExpressionType>&, ResultList& results, CallType)
{
    const FunctionSignature& signature = *type.as<FunctionSignature>();
    for (unsigned i = 0; i < signature.returnCount(); i ++)
        results.append(Value { });
    changeStackSize(signature.returnCount() - signature.argumentCount());

    // Function index:
    // 1B for instruction length
    // 4B for decoded index
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(5);
    auto functionIndexMetadata = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(functionIndexMetadata, getCurrentInstructionLength(), uint8_t);
    WRITE_TO_METADATA(functionIndexMetadata + 1, index, uint32_t);
    addCallCommonData(signature);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCallIndirect(unsigned tableIndex, const TypeDefinition& type, Vector<ExpressionType>&, ResultList& results, CallType)
{
    const FunctionSignature& signature = *type.as<FunctionSignature>();
    for (unsigned i = 0; i < signature.returnCount(); i ++)
        results.append(Value { });
    const unsigned callIndex = 1;
    changeStackSize(signature.returnCount() - signature.argumentCount() - callIndex);

    // Function index:
    // 1B for length
    // 4B for table index
    // 4B for type index
    auto size = m_metadata->m_metadata.size();
    m_metadata->addBlankSpace(9);
    auto functionIndexMetadata = m_metadata->m_metadata.data() + size;
    WRITE_TO_METADATA(functionIndexMetadata, getCurrentInstructionLength(), uint8_t);
    WRITE_TO_METADATA(functionIndexMetadata + 1, tableIndex, uint32_t);
    WRITE_TO_METADATA(functionIndexMetadata + 5, m_metadata->addSignature(type), uint32_t);

    addCallCommonData(signature);
    return { };
}

PartialResult WARN_UNUSED_RETURN IPIntGenerator::addCallRef(const TypeDefinition& type, Vector<ExpressionType>&, ResultList& results)
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

Expected<std::unique_ptr<FunctionIPIntMetadataGenerator>, String> parseAndCompileMetadata(std::span<const uint8_t> function, const TypeDefinition& signature, ModuleInformation& info, uint32_t functionIndex)
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

#endif // ENABLE(WEBASSEMBLY)
