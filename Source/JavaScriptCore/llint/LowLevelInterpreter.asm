# Copyright (C) 2011-2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

# Crash course on the language that this is written in (which I just call
# "assembly" even though it's more than that):
#
# - Mostly gas-style operand ordering. The last operand tends to be the
#   destination. So "a := b" is written as "mov b, a". But unlike gas,
#   comparisons are in-order, so "if (a < b)" is written as
#   "bilt a, b, ...".
#
# - "b" = byte, "h" = 16-bit word, "i" = 32-bit word, "q" = 64-bit word,
#   "f" = float, "d" = double, "p" = pointer. For 32-bit, "i" and "p" are
#   interchangeable except when an op supports one but not the other.
#
# - In general, valid operands for macro invocations and instructions are
#   registers (eg "t0"), addresses (eg "4[t0]"), base-index addresses
#   (eg "7[t0, t1, 2]"), absolute addresses (eg "0xa0000000[]"), or labels
#   (eg "_foo" or ".foo"). Macro invocations can also take anonymous
#   macros as operands. Instructions cannot take anonymous macros.
#
# - Labels must have names that begin with either "_" or ".".  A "." label
#   is local and gets renamed before code gen to minimize namespace
#   pollution. A "_" label is an extern symbol (i.e. ".globl"). The "_"
#   may or may not be removed during code gen depending on whether the asm
#   conventions for C name mangling on the target platform mandate a "_"
#   prefix.
#
# - A "macro" is a lambda expression, which may be either anonymous or
#   named. But this has caveats. "macro" can take zero or more arguments,
#   which may be macros or any valid operands, but it can only return
#   code. But you can do Turing-complete things via continuation passing
#   style: "macro foo (a, b) b(a, a) end foo(foo, foo)". Actually, don't do
#   that, since you'll just crash the assembler.
#
# - An "if" is a conditional on settings. Any identifier supplied in the
#   predicate of an "if" is assumed to be a #define that is available
#   during code gen. So you can't use "if" for computation in a macro, but
#   you can use it to select different pieces of code for different
#   platforms.
#
# - Arguments to macros follow lexical scoping rather than dynamic scoping.
#   Const's also follow lexical scoping and may override (hide) arguments
#   or other consts. All variables (arguments and constants) can be bound
#   to operands. Additionally, arguments (but not constants) can be bound
#   to macros.

# The following general-purpose registers are available:
#
#  - cfr and sp hold the call frame and (native) stack pointer respectively.
#  They are callee-save registers, and guaranteed to be distinct from all other
#  registers on all architectures.
#
#  - lr is defined on non-X86 architectures (ARM64, ARM64E, ARMv7, and CLOOP)
#  and holds the return PC
#
#  - t0, t1, t2, t3, t4, and optionally t5, t6, and t7 are temporary registers that can get trashed on
#  calls, and are pairwise distinct registers. t4 holds the JS program counter, so use
#  with caution in opcodes (actually, don't use it in opcodes at all, except as PC).
#
#  - r0 and r1 are the platform's customary return registers, and thus are
#  two distinct registers
#
#  - a0, a1, a2 and a3 are the platform's customary argument registers, and
#  thus are pairwise distinct registers. Be mindful that:
#    + On X86, there are no argument registers. a0 and a1 are edx and
#    ecx following the fastcall convention, but you should still use the stack
#    to pass your arguments. The cCall2 and cCall4 macros do this for you.
#    + On X86_64_WIN, you should allocate space on the stack for the arguments,
#    and the return convention is weird for > 8 bytes types. The only place we
#    use > 8 bytes return values is on a cCall, and cCall2 and cCall4 handle
#    this for you.
#
#  - The only registers guaranteed to be caller-saved are r0, r1, a0, a1 and a2, and
#  you should be mindful of that in functions that are called directly from C.
#  If you need more registers, you should push and pop them like a good
#  assembly citizen, because any other register will be callee-saved on X86.
#
# You can additionally assume:
#
#  - a3, t2, t3, t4 and t5 are never return registers; t0, t1, a0, a1 and a2
#  can be return registers.
#
#  - t3 can only be a3, t1 can only be a1; but t0 and t2 can be either a0 or a2.
#
#  - There are callee-save registers named csr0, csr1, ... csrN.
#  The last three csr registers are used used to store the PC base and
#  two special tag values (on 64-bits only). Don't use them for anything else.
#
# Additional platform-specific details (you shouldn't rely on this remaining
# true):
#
#  - For consistency with the baseline JIT, t0 is always r0 (and t1 is always
#  r1 on 32 bits platforms). You should use the r version when you need return
#  registers, and the t version otherwise: code using t0 (or t1) should still
#  work if swapped with e.g. t3, while code using r0 (or r1) should not. There
#  *may* be legacy code relying on this.
#
#  - On all platforms other than X86, t0 can only be a0 and t2 can only be a2.
#
#  - On all platforms other than X86 and X86_64, a2 is not a return register.
#  a2 is r0 on X86 (because we have so few registers) and r1 on X86_64 (because
#  the ABI enforces it).
#
# The following floating-point registers are available:
#
#  - ft0-ft5 are temporary floating-point registers that get trashed on calls,
#  and are pairwise distinct.
#
#  - fa0 and fa1 are the platform's customary floating-point argument
#  registers, and are both distinct. On 64-bits platforms, fa2 and fa3 are
#  additional floating-point argument registers.
#
#  - fr is the platform's customary floating-point return register
#
# You can assume that ft1-ft5 or fa1-fa3 are never fr, and that ftX is never
# faY if X != Y.

# Do not put any code before this.
global _llintPCRangeStart
_llintPCRangeStart:
    # This break instruction is needed so that the synthesized llintPCRangeStart# label
    # doesn't point to the exact same location as vmEntryToJavaScript which comes after it.
    # Otherwise, libunwind will report vmEntryToJavaScript as llintPCRangeStart in
    # stack traces.
    break

# Work-around for the fact that the toolchain's awareness of armv7k / armv7s
# results in a separate slab in the fat binary, yet the offlineasm doesn't know
# to expect it.
if ARMv7k
end
if ARMv7s
end

# First come the common protocols that both interpreters use. Note that each
# of these must have an ASSERT() in LLIntData.cpp

# These declarations must match interpreter/JSStack.h.

const PtrSize = constexpr (sizeof(void*))
const MachineRegisterSize = constexpr (sizeof(CPURegister))
const SlotSize = constexpr (sizeof(Register))

if JSVALUE64
    const CallFrameHeaderSlots = 5
else
    const CallFrameHeaderSlots = 4
    const CallFrameAlignSlots = 1
end

const JSLexicalEnvironment_variables = (sizeof JSLexicalEnvironment + SlotSize - 1) & ~(SlotSize - 1)
const DirectArguments_storage = (sizeof DirectArguments + SlotSize - 1) & ~(SlotSize - 1)
const JSInternalFieldObjectImpl_internalFields = JSInternalFieldObjectImpl::m_internalFields

const StackAlignment = constexpr (stackAlignmentBytes())
const StackAlignmentSlots = constexpr (stackAlignmentRegisters())
const StackAlignmentMask = StackAlignment - 1

const CallerFrameAndPCSize = constexpr (sizeof(CallerFrameAndPC))
const PrologueStackPointerDelta = constexpr (prologueStackPointerDelta())

const CallerFrame = 0
const ReturnPC = CallerFrame + MachineRegisterSize
const CodeBlock = ReturnPC + MachineRegisterSize
const Callee = CodeBlock + SlotSize
const ArgumentCountIncludingThis = Callee + SlotSize
const ThisArgumentOffset = ArgumentCountIncludingThis + SlotSize
const FirstArgumentOffset = ThisArgumentOffset + SlotSize
const CallFrameHeaderSize = ThisArgumentOffset

const MetadataOffsetTable16Offset = 0
const MetadataOffsetTable32Offset = constexpr UnlinkedMetadataTable::s_offset16TableSize
const NumberOfJSOpcodeIDs = constexpr numOpcodeIDs

# Some value representation constants.
if JSVALUE64
    const TagOther        = constexpr JSValue::OtherTag
    const TagBool         = constexpr JSValue::BoolTag
    const TagUndefined    = constexpr JSValue::UndefinedTag
    const ValueEmpty      = constexpr JSValue::ValueEmpty
    const ValueFalse      = constexpr JSValue::ValueFalse
    const ValueTrue       = constexpr JSValue::ValueTrue
    const ValueUndefined  = constexpr JSValue::ValueUndefined
    const ValueNull       = constexpr JSValue::ValueNull
    const TagNumber       = constexpr JSValue::NumberTag
    const NotCellMask     = constexpr JSValue::NotCellMask
    if BIGINT32
        const TagBigInt32 = constexpr JSValue::BigInt32Tag
        const MaskBigInt32 = constexpr JSValue::BigInt32Mask
    end
    const LowestOfHighBits = constexpr JSValue::LowestOfHighBits
else
    const Int32Tag = constexpr JSValue::Int32Tag
    const BooleanTag = constexpr JSValue::BooleanTag
    const NullTag = constexpr JSValue::NullTag
    const UndefinedTag = constexpr JSValue::UndefinedTag
    const CellTag = constexpr JSValue::CellTag
    const EmptyValueTag = constexpr JSValue::EmptyValueTag
    const DeletedValueTag = constexpr JSValue::DeletedValueTag
    const InvalidTag = constexpr JSValue::InvalidTag
    const LowestTag = constexpr JSValue::LowestTag
end

if LARGE_TYPED_ARRAYS
    const SmallTypedArrayMaxLength = constexpr ArrayProfile::s_smallTypedArrayMaxLength
end

const maxFrameExtentForSlowPathCall = constexpr maxFrameExtentForSlowPathCall

if X86_64 or X86_64_WIN or ARM64 or ARM64E or RISCV64
    const CalleeSaveSpaceAsVirtualRegisters = 4
elsif C_LOOP or C_LOOP_WIN
    const CalleeSaveSpaceAsVirtualRegisters = 1
elsif ARMv7
    const CalleeSaveSpaceAsVirtualRegisters = 1
else
    const CalleeSaveSpaceAsVirtualRegisters = 0
end

const CalleeSaveSpaceStackAligned = (CalleeSaveSpaceAsVirtualRegisters * SlotSize + StackAlignment - 1) & ~StackAlignmentMask


# Watchpoint states
const ClearWatchpoint = constexpr ClearWatchpoint
const IsWatched = constexpr IsWatched
const IsInvalidated = constexpr IsInvalidated

# ShadowChicken data
const ShadowChickenTailMarker = constexpr ShadowChicken::Packet::tailMarkerValue

# UnaryArithProfile data
const ArithProfileInt = constexpr (UnaryArithProfile::observedIntBits())
const ArithProfileNumber = constexpr (UnaryArithProfile::observedNumberBits())

# BinaryArithProfile data
const ArithProfileIntInt = constexpr (BinaryArithProfile::observedIntIntBits())
const ArithProfileNumberInt = constexpr (BinaryArithProfile::observedNumberIntBits())
const ArithProfileIntNumber = constexpr (BinaryArithProfile::observedIntNumberBits())
const ArithProfileNumberNumber = constexpr (BinaryArithProfile::observedNumberNumberBits())

# Pointer Tags
const AddressDiversified = 1
const BytecodePtrTag = constexpr BytecodePtrTag
const CustomAccessorPtrTag = constexpr CustomAccessorPtrTag
const JSEntryPtrTag = constexpr JSEntryPtrTag
const HostFunctionPtrTag = constexpr HostFunctionPtrTag
const JSEntrySlowPathPtrTag = constexpr JSEntrySlowPathPtrTag
const NativeToJITGatePtrTag = constexpr NativeToJITGatePtrTag
const ExceptionHandlerPtrTag = constexpr ExceptionHandlerPtrTag
const YarrEntryPtrTag = constexpr YarrEntryPtrTag
const CSSSelectorPtrTag = constexpr CSSSelectorPtrTag
const NoPtrTag = constexpr NoPtrTag
 
# VMTraps data
const VMTrapsAsyncEvents = constexpr VMTraps::AsyncEvents

# Some register conventions.
# - We use a pair of registers to represent the PC: one register for the
#   base of the bytecodes, and one register for the index.
# - The PC base (or PB for short) must be stored in a callee-save register.
# - The metadata (PM / pointer to metadata) must be stored in a callee-save register.
# - C calls are still given the Instruction* rather than the PC index.
#   This requires an add before the call, and a sub after.
if JSVALUE64
    const PC = t4 # When changing this, make sure LLIntPC is up to date in LLIntPCRanges.h
    if ARM64 or ARM64E or RISCV64
        const metadataTable = csr6
        const PB = csr7
        const numberTag = csr8
        const notCellMask = csr9
    elsif X86_64
        const metadataTable = csr1
        const PB = csr2
        const numberTag = csr3
        const notCellMask = csr4
    elsif X86_64_WIN
        const metadataTable = csr3
        const PB = csr4
        const numberTag = csr5
        const notCellMask = csr6
    elsif C_LOOP or C_LOOP_WIN
        const PB = csr0
        const numberTag = csr1
        const notCellMask = csr2
        const metadataTable = csr3
    end

else
    const PC = t4 # When changing this, make sure LLIntPC is up to date in LLIntPCRanges.h
    if C_LOOP or C_LOOP_WIN
        const PB = csr0
        const metadataTable = csr3
    elsif ARMv7
        const metadataTable = csr0
        const PB = csr1
    else
        error
    end
end

if GIGACAGE_ENABLED
    const GigacagePrimitiveBasePtrOffset = constexpr Gigacage::offsetOfPrimitiveGigacageBasePtr
    const GigacageJSValueBasePtrOffset = constexpr Gigacage::offsetOfJSValueGigacageBasePtr
end

# Opcode offsets
const OpcodeIDNarrowSize = 1 # OpcodeID
const OpcodeIDWide16SizeJS = 2 # Wide16 Prefix + OpcodeID
const OpcodeIDWide32SizeJS = 2 # Wide32 Prefix + OpcodeID
const OpcodeIDWide16SizeWasm = 2 # Wide16 Prefix + OpcodeID(1 byte)
const OpcodeIDWide32SizeWasm = 2 # Wide32 Prefix + OpcodeID(1 byte)

if X86_64_WIN or C_LOOP_WIN
    const WTFConfig = _g_wtfConfigForLLInt
    const GigacageConfig = _g_gigacageConfig
    const JSCConfig = _g_jscConfig
else
    const WTFConfig = _g_config + constexpr WTF::startOffsetOfWTFConfig
    const GigacageConfig = _g_config + constexpr Gigacage::startOffsetOfGigacageConfig
    const JSCConfigOffset = constexpr WTF::offsetOfWTFConfigExtension
    const JSCConfigGateMapOffset = JSCConfigOffset + constexpr JSC::offsetOfJSCConfigGateMap
end

macro nextInstruction()
    loadb [PB, PC, 1], t0
    leap _g_opcodeMap, t1
    jmp [t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro nextInstructionWide16()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide16, t1
    jmp [t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro nextInstructionWide32()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide32, t1
    jmp [t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro dispatch(advanceReg)
    addp advanceReg, PC
    nextInstruction()
end

macro dispatchIndirect(offsetReg)
    dispatch(offsetReg)
end

macro genericDispatchOpJS(dispatch, size, opcodeName)
    macro dispatchNarrow()
        dispatch((constexpr %opcodeName%_length) * 1 + OpcodeIDNarrowSize)
    end

    macro dispatchWide16()
        dispatch((constexpr %opcodeName%_length) * 2 + OpcodeIDWide16SizeJS)
    end

    macro dispatchWide32()
        dispatch((constexpr %opcodeName%_length) * 4 + OpcodeIDWide32SizeJS)
    end

    size(dispatchNarrow, dispatchWide16, dispatchWide32, macro (dispatch) dispatch() end)
end

macro genericDispatchOpWasm(dispatch, size, opcodeName)
    macro dispatchNarrow()
        dispatch((constexpr %opcodeName%_length) * 1 + OpcodeIDNarrowSize)
    end

    macro dispatchWide16()
        dispatch((constexpr %opcodeName%_length) * 2 + OpcodeIDWide16SizeWasm)
    end

    macro dispatchWide32()
        dispatch((constexpr %opcodeName%_length) * 4 + OpcodeIDWide32SizeWasm)
    end

    size(dispatchNarrow, dispatchWide16, dispatchWide32, macro (dispatch) dispatch() end)
end

macro dispatchOp(size, opcodeName)
    genericDispatchOpJS(dispatch, size, opcodeName)
end

macro superSamplerBegin(scratch)
    leap _g_superSamplerCount, scratch
    addi 1, [scratch]
end

macro superSamplerEnd(scratch)
    leap _g_superSamplerCount, scratch
    subi 1, [scratch]
end

macro getu(size, opcodeStruct, fieldName, dst)
    size(getuOperandNarrow, getuOperandWide16JS, getuOperandWide32JS, macro (getu)
        getu(opcodeStruct, fieldName, dst)
    end)
end

macro get(size, opcodeStruct, fieldName, dst)
    size(getOperandNarrow, getOperandWide16JS, getOperandWide32JS, macro (get)
        get(opcodeStruct, fieldName, dst)
    end)
end

macro narrow(narrowFn, wide16Fn, wide32Fn, k)
    k(narrowFn)
end

macro wide16(narrowFn, wide16Fn, wide32Fn, k)
    k(wide16Fn)
end

macro wide32(narrowFn, wide16Fn, wide32Fn, k)
    k(wide32Fn)
end

macro metadata(size, opcode, dst, scratch)
    loadh (constexpr %opcode%::opcodeID * 2 + MetadataOffsetTable16Offset)[metadataTable], dst # offset = metadataTable<uint16_t*>[opcodeID]
    btinz dst, .setUpOffset
    loadi (constexpr %opcode%::opcodeID * 4 + MetadataOffsetTable32Offset)[metadataTable], dst # offset = metadataTable<uint32_t*>[opcodeID]
.setUpOffset:
    getu(size, opcode, m_metadataID, scratch) # scratch = bytecode.m_metadataID
    muli sizeof %opcode%::Metadata, scratch # scratch *= sizeof(Op::Metadata)
    addi scratch, dst # offset += scratch
    addp metadataTable, dst # return &metadataTable[offset]
    # roundUpToMultipleOf(alignof(Metadata), dst)
    const adder = (constexpr (alignof(%opcode%::Metadata))) - 1
    const mask = ~adder
    addp adder, dst
    andp mask, dst
end

macro jumpImpl(dispatchIndirect, targetOffsetReg)
    btiz targetOffsetReg, .outOfLineJumpTarget
    dispatchIndirect(targetOffsetReg)
.outOfLineJumpTarget:
    callSlowPath(_llint_slow_path_out_of_line_jump_target)
    nextInstruction()
end

macro commonOp(label, prologue, fn)
_%label%:
    prologue()
    fn(narrow)
    if ASSERT_ENABLED
        break
        break
    end

# FIXME: We cannot enable wide16 bytecode in Windows CLoop. With MSVC, as CLoop::execute gets larger code
# size, CLoop::execute gets higher stack height requirement. This makes CLoop::execute takes 160KB stack
# per call, causes stack overflow error easily. For now, we disable wide16 optimization for Windows CLoop.
# https://bugs.webkit.org/show_bug.cgi?id=198283
if not C_LOOP_WIN
_%label%_wide16:
    prologue()
    fn(wide16)
    if ASSERT_ENABLED
        break
        break
    end
end

_%label%_wide32:
    prologue()
    fn(wide32)
    if ASSERT_ENABLED
        break
        break
    end
end

macro op(l, fn)
    commonOp(l, macro () end, macro (size)
        size(fn, macro() break end, macro() break end, macro(gen) gen() end)
    end)
end

macro llintOp(opcodeName, opcodeStruct, fn)
    commonOp(llint_%opcodeName%, traceExecution, macro(size)
        macro getImpl(fieldName, dst)
            get(size, opcodeStruct, fieldName, dst)
        end

        macro dispatchImpl()
            dispatchOp(size, opcodeName)
        end

        fn(size, getImpl, dispatchImpl)
    end)
end

macro llintOpWithReturn(opcodeName, opcodeStruct, fn)
    llintOp(opcodeName, opcodeStruct, macro(size, get, dispatch)
        makeReturn(get, dispatch, macro (return)
            fn(size, get, dispatch, return)
        end)
    end)
end

macro llintOpWithMetadata(opcodeName, opcodeStruct, fn)
    llintOpWithReturn(opcodeName, opcodeStruct, macro (size, get, dispatch, return)
        macro meta(dst, scratch)
            metadata(size, opcodeStruct, dst, scratch)
        end
        fn(size, get, dispatch, meta, return)
    end)
end

macro llintOpWithJump(opcodeName, opcodeStruct, impl)
    llintOpWithMetadata(opcodeName, opcodeStruct, macro(size, get, dispatch, metadata, return)
        macro jump(fieldName)
            get(fieldName, t0)
            jumpImpl(dispatchIndirect, t0)
        end

        impl(size, get, jump, dispatch)
    end)
end

macro llintOpWithProfile(opcodeName, opcodeStruct, fn)
    llintOpWithMetadata(opcodeName, opcodeStruct, macro(size, get, dispatch, metadata, return)
        makeReturnProfiled(size, opcodeStruct, get, metadata, dispatch, macro (returnProfiled)
            fn(size, get, dispatch, returnProfiled)
        end)
    end)
end


if X86_64_WIN
    const extraTempReg = t0
else
    const extraTempReg = t5
end

# Constants for reasoning about value representation.
const TagOffset = constexpr TagOffset
const PayloadOffset = constexpr PayloadOffset

# Constant for reasoning about butterflies.
const IsArray                  = constexpr IsArray
const IndexingShapeMask        = constexpr IndexingShapeMask
const IndexingTypeMask         = constexpr IndexingTypeMask
const NoIndexingShape          = constexpr NoIndexingShape
const Int32Shape               = constexpr Int32Shape
const DoubleShape              = constexpr DoubleShape
const ContiguousShape          = constexpr ContiguousShape
const ArrayStorageShape        = constexpr ArrayStorageShape
const SlowPutArrayStorageShape = constexpr SlowPutArrayStorageShape
const CopyOnWrite              = constexpr CopyOnWrite
const ArrayWithUndecided       = constexpr ArrayWithUndecided

# Type constants.
const StructureType = constexpr StructureType
const StringType = constexpr StringType
const SymbolType = constexpr SymbolType
const ObjectType = constexpr ObjectType
const FinalObjectType = constexpr FinalObjectType
const JSFunctionType = constexpr JSFunctionType
const InternalFunctionType = constexpr InternalFunctionType
const ArrayType = constexpr ArrayType
const DerivedArrayType = constexpr DerivedArrayType
const ProxyObjectType = constexpr ProxyObjectType
const HeapBigIntType = constexpr HeapBigIntType
const FunctionExecutableType = constexpr FunctionExecutableType

# The typed array types need to be numbered in a particular order because of the manually written
# switch statement in get_by_val and put_by_val.
const Int8ArrayType = constexpr Int8ArrayType
const Uint8ArrayType = constexpr Uint8ArrayType
const Uint8ClampedArrayType = constexpr Uint8ClampedArrayType
const Int16ArrayType = constexpr Int16ArrayType
const Uint16ArrayType = constexpr Uint16ArrayType
const Int32ArrayType = constexpr Int32ArrayType
const Uint32ArrayType = constexpr Uint32ArrayType
const Float32ArrayType = constexpr Float32ArrayType
const Float64ArrayType = constexpr Float64ArrayType

const FirstTypedArrayType = constexpr FirstTypedArrayType
const NumberOfTypedArrayTypesExcludingDataView = constexpr NumberOfTypedArrayTypesExcludingDataView
const NumberOfTypedArrayTypesExcludingBigIntArraysAndDataView = constexpr NumberOfTypedArrayTypesExcludingBigIntArraysAndDataView

# Type flags constants.
const MasqueradesAsUndefined = constexpr MasqueradesAsUndefined
const ImplementsDefaultHasInstance = constexpr ImplementsDefaultHasInstance
const OverridesGetPrototypeOutOfLine = constexpr OverridesGetPrototypeOutOfLine

# Bytecode operand constants.
const FirstConstantRegisterIndexNarrow = constexpr FirstConstantRegisterIndex8
const FirstConstantRegisterIndexWide16 = constexpr FirstConstantRegisterIndex16
const FirstConstantRegisterIndexWide32 = constexpr FirstConstantRegisterIndex

# Code type constants.
const GlobalCode = constexpr GlobalCode
const EvalCode = constexpr EvalCode
const FunctionCode = constexpr FunctionCode
const ModuleCode = constexpr ModuleCode

# The interpreter steals the tag word of the argument count.
const CallSiteIndex = ArgumentCountIncludingThis + TagOffset

# String flags.
const isRopeInPointer = constexpr JSString::isRopeInPointer
const HashFlags8BitBuffer = constexpr StringImpl::s_hashFlag8BitBuffer

# Copied from PropertyOffset.h
const firstOutOfLineOffset = constexpr firstOutOfLineOffset
const knownPolyProtoOffset = constexpr knownPolyProtoOffset

# ResolveType
const GlobalProperty = constexpr GlobalProperty
const GlobalVar = constexpr GlobalVar
const GlobalLexicalVar = constexpr GlobalLexicalVar
const ClosureVar = constexpr ClosureVar
const ResolvedClosureVar = constexpr ResolvedClosureVar
const ModuleVar = constexpr ModuleVar
const GlobalPropertyWithVarInjectionChecks = constexpr GlobalPropertyWithVarInjectionChecks
const GlobalVarWithVarInjectionChecks = constexpr GlobalVarWithVarInjectionChecks
const GlobalLexicalVarWithVarInjectionChecks = constexpr GlobalLexicalVarWithVarInjectionChecks
const ClosureVarWithVarInjectionChecks = constexpr ClosureVarWithVarInjectionChecks

const ResolveTypeMask = constexpr GetPutInfo::typeBits
const InitializationModeMask = constexpr GetPutInfo::initializationBits
const InitializationModeShift = constexpr GetPutInfo::initializationShift
const NotInitialization = constexpr InitializationMode::NotInitialization

const MarkedBlockSize = constexpr MarkedBlock::blockSize
const MarkedBlockMask = ~(MarkedBlockSize - 1)
const MarkedBlockHeaderOffset = constexpr MarkedBlock::offsetOfHeader
const PreciseAllocationHeaderSize = constexpr (PreciseAllocation::headerSize())
const PreciseAllocationVMOffset = (PreciseAllocation::m_weakSet + WeakSet::m_vm - PreciseAllocationHeaderSize)

const BlackThreshold = constexpr blackThreshold

const VectorBufferOffset = Vector::m_buffer
const VectorSizeOffset = Vector::m_size

# Some common utilities.
macro crash()
    if C_LOOP or C_LOOP_WIN
        cloopCrash
    else
        call _llint_crash
    end
end

macro assert(assertion)
    if ASSERT_ENABLED
        assertion(.ok)
        crash()
    .ok:
    end
end

macro assert_with(assertion, crash)
    if ASSERT_ENABLED
        assertion(.ok)
        crash()
    .ok:
    end
end

# The probe macro can be used to insert some debugging code without perturbing scalar
# registers. Presently, the probe macro only preserves scalar registers. Hence, the
# C probe callback function should not trash floating point registers.
#
# The macro you pass to probe() can pass whatever registers you like to your probe
# callback function. However, you need to be mindful of which of the registers are
# also used as argument registers, and ensure that you don't trash the register value
# before storing it in the probe callback argument register that you desire.
#
# Here's an example of how it's used:
#
#     probe(
#         macro()
#             move cfr, a0 # pass the CallFrame* as arg0.
#             move t0, a1 # pass the value of register t0 as arg1.
#             call _cProbeCallbackFunction # to do whatever you want.
#         end
#     )
#
if X86_64 or ARM64 or ARM64E or ARMv7
    macro probe(action)
        # save all the registers that the LLInt may use.
        if ARM64 or ARM64E or ARMv7
            push cfr, lr
        end
        push a0, a1
        push a2, a3
        push t0, t1
        push t2, t3
        push t4, t5
        push t6, t7
        push ws0, ws1
        if ARM64 or ARM64E
            push csr0, csr1
            push csr2, csr3
            push csr4, csr5
            push csr6, csr7
            push csr8, csr9
        elsif ARMv7
            push csr0, csr1
        end

        action()

        # restore all the registers we saved previously.
        if ARM64 or ARM64E
            pop csr9, csr8
            pop csr7, csr6
            pop csr5, csr4
            pop csr3, csr2
            pop csr1, csr0
        elsif ARMv7
            pop csr1, csr0
        end
        pop ws1, ws0
        pop t7, t6
        pop t5, t4
        pop t3, t2
        pop t1, t0
        pop a3, a2
        pop a1, a0
        if ARM64 or ARM64E or ARMv7
            pop lr, cfr
        end
    end
else
    macro probe(action)
    end
end

macro checkStackPointerAlignment(tempReg, location)
    if ASSERT_ENABLED
        if ARM64 or ARM64E or C_LOOP or C_LOOP_WIN
            # ARM64 and ARM64E will check for us!
            # C_LOOP or C_LOOP_WIN does not need the alignment, and can use a little perf
            # improvement from avoiding useless work.
        else
            if ARMv7
                # ARM can't do logical ops with the sp as a source
                move sp, tempReg
                andp StackAlignmentMask, tempReg
            else
                andp sp, StackAlignmentMask, tempReg
            end
            btpz tempReg, .stackPointerOkay
            move location, tempReg
            break
        .stackPointerOkay:
        end
    end
end

if C_LOOP or C_LOOP_WIN or ARM64 or ARM64E or X86_64 or X86_64_WIN or RISCV64
    const CalleeSaveRegisterCount = 0
elsif ARMv7
    const CalleeSaveRegisterCount = 5 + 2 * 2 // 5 32-bit GPRs + 2 64-bit FPRs
elsif X86 or X86_WIN
    const CalleeSaveRegisterCount = 3
end

const CalleeRegisterSaveSize = CalleeSaveRegisterCount * MachineRegisterSize

# VMEntryTotalFrameSize includes the space for struct VMEntryRecord and the
# callee save registers rounded up to keep the stack aligned
const VMEntryTotalFrameSize = (CalleeRegisterSaveSize + sizeof VMEntryRecord + StackAlignment - 1) & ~StackAlignmentMask

macro pushCalleeSaves()
    # Note: Only registers that are in RegisterSetBuilder::calleeSaveRegisters(),
    # but are not in RegisterSetBuilder::vmCalleeSaveRegisters() need to be saved here,
    # i.e.: only those registers that are callee save in the C ABI, but are not
    # callee save in the JIT ABI.
    if C_LOOP or C_LOOP_WIN or ARM64 or ARM64E or X86_64 or X86_64_WIN or RISCV64
    elsif ARMv7
        emit "vpush.64 {d14, d15}"
        emit "push {r4-r6, r8-r9}"
    elsif X86
        emit "push %esi"
        emit "push %edi"
        emit "push %ebx"
    elsif X86_WIN
        emit "push esi"
        emit "push edi"
        emit "push ebx"
    end
end

macro popCalleeSaves()
    if C_LOOP or C_LOOP_WIN or ARM64 or ARM64E or X86_64 or X86_64_WIN or RISCV64
    elsif ARMv7
        emit "pop {r4-r6, r8-r9}"
        emit "vpop.64 {d14, d15}"
    elsif X86
        emit "pop %ebx"
        emit "pop %edi"
        emit "pop %esi"
    elsif X86_WIN
        emit "pop ebx"
        emit "pop edi"
        emit "pop esi"
    end
end

macro preserveCallerPCAndCFR()
    if C_LOOP or C_LOOP_WIN or ARMv7
        push lr
        push cfr
    elsif X86 or X86_WIN or X86_64 or X86_64_WIN
        push cfr
    elsif ARM64 or ARM64E or RISCV64
        push cfr, lr
    else
        error
    end
    move sp, cfr
end

macro restoreCallerPCAndCFR()
    move cfr, sp
    if C_LOOP or C_LOOP_WIN or ARMv7
        pop cfr
        pop lr
    elsif X86 or X86_WIN or X86_64 or X86_64_WIN
        pop cfr
    elsif ARM64 or ARM64E or RISCV64
        pop lr, cfr
    end
end

macro preserveCalleeSavesUsedByLLInt()
    subp CalleeSaveSpaceStackAligned, sp
    if C_LOOP or C_LOOP_WIN
        storep metadataTable, -PtrSize[cfr]
    elsif ARMv7
        storep PB, -4[cfr]
        storep metadataTable, -8[cfr]
    elsif ARM64 or ARM64E
        storepairq csr8, csr9, -16[cfr]
        storepairq csr6, csr7, -32[cfr]
    elsif X86
    elsif X86_WIN
    elsif X86_64
        storep csr4, -8[cfr]
        storep csr3, -16[cfr]
        storep csr2, -24[cfr]
        storep csr1, -32[cfr]
    elsif X86_64_WIN
        storep csr6, -8[cfr]
        storep csr5, -16[cfr]
        storep csr4, -24[cfr]
        storep csr3, -32[cfr]
    elsif RISCV64
        storep csr9, -8[cfr]
        storep csr8, -16[cfr]
        storep csr7, -24[cfr]
        storep csr6, -32[cfr]
    end
end

macro restoreCalleeSavesUsedByLLInt()
    if C_LOOP or C_LOOP_WIN
        loadp -PtrSize[cfr], metadataTable
    elsif ARMv7
        loadp -4[cfr], PB
        loadp -8[cfr], metadataTable
    elsif ARM64 or ARM64E
        loadpairq -32[cfr], csr6, csr7
        loadpairq -16[cfr], csr8, csr9
    elsif X86
    elsif X86_WIN
    elsif X86_64
        loadp -32[cfr], csr1
        loadp -24[cfr], csr2
        loadp -16[cfr], csr3
        loadp -8[cfr], csr4
    elsif X86_64_WIN
        loadp -32[cfr], csr3
        loadp -24[cfr], csr4
        loadp -16[cfr], csr5
        loadp -8[cfr], csr6
    elsif RISCV64
        loadp -32[cfr], csr6
        loadp -24[cfr], csr7
        loadp -16[cfr], csr8
        loadp -8[cfr], csr9
    end
end

macro copyCalleeSavesToEntryFrameCalleeSavesBuffer(entryFrame)
    if ARM64 or ARM64E or X86_64 or X86_64_WIN or ARMv7 or RISCV64
        vmEntryRecord(entryFrame, entryFrame)
        leap VMEntryRecord::calleeSaveRegistersBuffer[entryFrame], entryFrame
        if ARM64 or ARM64E
            storepairq csr0, csr1, [entryFrame]
            storepairq csr2, csr3, 16[entryFrame]
            storepairq csr4, csr5, 32[entryFrame]
            storepairq csr6, csr7, 48[entryFrame]
            storepairq csr8, csr9, 64[entryFrame]
            storepaird csfr0, csfr1, 80[entryFrame]
            storepaird csfr2, csfr3, 96[entryFrame]
            storepaird csfr4, csfr5, 112[entryFrame]
            storepaird csfr6, csfr7, 128[entryFrame]
        elsif X86_64
            storeq csr0, [entryFrame]
            storeq csr1, 8[entryFrame]
            storeq csr2, 16[entryFrame]
            storeq csr3, 24[entryFrame]
            storeq csr4, 32[entryFrame]
        elsif X86_64_WIN
            storeq csr0, [entryFrame]
            storeq csr1, 8[entryFrame]
            storeq csr2, 16[entryFrame]
            storeq csr3, 24[entryFrame]
            storeq csr4, 32[entryFrame]
            storeq csr5, 40[entryFrame]
            storeq csr6, 48[entryFrame]
        elsif ARMv7
            storep csr0, [entryFrame]
            storep csr1, 4[entryFrame]
            stored csfr0, 8[entryFrame]
            stored csfr1, 16[entryFrame]
            stored csfr2, 24[entryFrame]
            stored csfr3, 32[entryFrame]
            stored csfr4, 40[entryFrame]
            stored csfr5, 48[entryFrame]
        elsif RISCV64
            storep csr0, [entryFrame]
            storep csr1, 8[entryFrame]
            storep csr2, 16[entryFrame]
            storep csr3, 24[entryFrame]
            storep csr4, 32[entryFrame]
            storep csr5, 40[entryFrame]
            storep csr6, 48[entryFrame]
            storep csr7, 56[entryFrame]
            storep csr8, 64[entryFrame]
            storep csr9, 72[entryFrame]
            storep csr10, 80[entryFrame]
            stored csfr0, 88[entryFrame]
            stored csfr1, 96[entryFrame]
            stored csfr2, 104[entryFrame]
            stored csfr3, 112[entryFrame]
            stored csfr4, 120[entryFrame]
            stored csfr5, 128[entryFrame]
            stored csfr6, 136[entryFrame]
            stored csfr7, 144[entryFrame]
            stored csfr8, 152[entryFrame]
            stored csfr9, 160[entryFrame]
            stored csfr10, 168[entryFrame]
            stored csfr11, 176[entryFrame]
        end
    end
end

macro copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(vm, temp)
    if ARM64 or ARM64E or X86_64 or X86_64_WIN or ARMv7 or RISCV64
        loadp VM::topEntryFrame[vm], temp
        copyCalleeSavesToEntryFrameCalleeSavesBuffer(temp)
    end
end

macro restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(vm, temp)
    if ARM64 or ARM64E or X86_64 or X86_64_WIN or ARMv7 or RISCV64
        loadp VM::topEntryFrame[vm], temp
        vmEntryRecord(temp, temp)
        leap VMEntryRecord::calleeSaveRegistersBuffer[temp], temp
        if ARM64 or ARM64E
            loadpairq [temp], csr0, csr1
            loadpairq 16[temp], csr2, csr3
            loadpairq 32[temp], csr4, csr5
            loadpairq 48[temp], csr6, csr7
            loadpairq 64[temp], csr8, csr9
            loadpaird 80[temp], csfr0, csfr1
            loadpaird 96[temp], csfr2, csfr3
            loadpaird 112[temp], csfr4, csfr5
            loadpaird 128[temp], csfr6, csfr7
        elsif X86_64
            loadq [temp], csr0
            loadq 8[temp], csr1
            loadq 16[temp], csr2
            loadq 24[temp], csr3
            loadq 32[temp], csr4
        elsif X86_64_WIN
            loadq [temp], csr0
            loadq 8[temp], csr1
            loadq 16[temp], csr2
            loadq 24[temp], csr3
            loadq 32[temp], csr4
            loadq 40[temp], csr5
            loadq 48[temp], csr6
        elsif ARMv7
            loadp [temp], csr0
            loadp 4[temp], csr1
            loadd 8[temp], csfr0
            loadd 16[temp], csfr1
            loadd 24[temp], csfr2
            loadd 32[temp], csfr3
            loadd 40[temp], csfr4
            loadd 48[temp], csfr5
        elsif RISCV64
            loadq [temp], csr0
            loadq 8[temp], csr1
            loadq 16[temp], csr2
            loadq 24[temp], csr3
            loadq 32[temp], csr4
            loadq 40[temp], csr5
            loadq 48[temp], csr6
            loadq 56[temp], csr7
            loadq 64[temp], csr8
            loadq 72[temp], csr9
            loadq 80[temp], csr10
            loadd 88[temp], csfr0
            loadd 96[temp], csfr1
            loadd 104[temp], csfr2
            loadd 112[temp], csfr3
            loadd 120[temp], csfr4
            loadd 128[temp], csfr5
            loadd 136[temp], csfr6
            loadd 144[temp], csfr7
            loadd 152[temp], csfr8
            loadd 160[temp], csfr9
            loadd 168[temp], csfr10
            loadd 176[temp], csfr11
        end
    end
end

macro preserveReturnAddressAfterCall(destinationRegister)
    if C_LOOP or C_LOOP_WIN or ARMv7 or ARM64 or ARM64E or RISCV64
        # In C_LOOP or C_LOOP_WIN case, we're only preserving the bytecode vPC.
        move lr, destinationRegister
    elsif X86 or X86_WIN or X86_64 or X86_64_WIN
        pop destinationRegister
    else
        error
    end
end

macro functionPrologue()
    tagReturnAddress sp
    if X86 or X86_WIN or X86_64 or X86_64_WIN
        push cfr
    elsif ARM64 or ARM64E or RISCV64
        push cfr, lr
    elsif C_LOOP or C_LOOP_WIN or ARMv7 
        push lr
        push cfr
    end
    move sp, cfr
end

macro functionEpilogue()
    if X86 or X86_WIN or X86_64 or X86_64_WIN
        pop cfr
    elsif ARM64 or ARM64E or RISCV64
        pop lr, cfr
    elsif C_LOOP or C_LOOP_WIN or ARMv7
        pop cfr
        pop lr
    end
end

macro vmEntryRecord(entryFramePointer, resultReg)
    subp entryFramePointer, VMEntryTotalFrameSize, resultReg
end

macro getFrameRegisterSizeForCodeBlock(codeBlock, size)
    loadi CodeBlock::m_numCalleeLocals[codeBlock], size
    lshiftp 3, size
    addp maxFrameExtentForSlowPathCall, size
end

macro restoreStackPointerAfterCall()
    loadp CodeBlock[cfr], t2
    getFrameRegisterSizeForCodeBlock(t2, t2)
    if ARMv7
        subp cfr, t2, t2
        move t2, sp
    else
        subp cfr, t2, sp
    end
end

macro traceExecution()
    if TRACING
        callSlowPath(_llint_trace)
    end
end

macro defineReturnLabel(opcodeName, size)
    macro defineNarrow()
        if not C_LOOP_WIN
            _%opcodeName%_return_location:
        end
    end

    macro defineWide16()
        if not C_LOOP_WIN
            _%opcodeName%_return_location_wide16:
        end
    end

    macro defineWide32()
        if not C_LOOP_WIN
            _%opcodeName%_return_location_wide32:
        end
    end

    size(defineNarrow, defineWide16, defineWide32, macro (f) f() end)
end

if ARM64E
    global _llint_function_for_call_arity_checkUntagGateAfter
    global _llint_function_for_call_arity_checkTagGateAfter
    global _llint_function_for_construct_arity_checkUntagGateAfter
    global _llint_function_for_construct_arity_checkTagGateAfter
end

macro callTargetFunction(opcodeName, size, opcodeStruct, dispatchAfterCall, valueProfileName, dstVirtualRegister, dispatch, callee, callPtrTag)
    if C_LOOP or C_LOOP_WIN
        cloopCallJSFunction callee
    elsif ARM64E
        macro callNarrow()
            leap _g_config, a7
            jmp JSCConfigGateMapOffset + (constexpr Gate::%opcodeName%) * PtrSize[a7], NativeToJITGatePtrTag # callPtrTag
            _js_trampoline_%opcodeName%:
            call t5, callPtrTag
        end

        macro callWide16()
            leap _g_config, a7
            jmp JSCConfigGateMapOffset + (constexpr Gate::%opcodeName%_wide16) * PtrSize[a7], NativeToJITGatePtrTag # callPtrTag
            _js_trampoline_%opcodeName%_wide16:
            call t5, callPtrTag
        end

        macro callWide32()
            leap _g_config, a7
            jmp JSCConfigGateMapOffset + (constexpr Gate::%opcodeName%_wide32) * PtrSize[a7], NativeToJITGatePtrTag # callPtrTag
            _js_trampoline_%opcodeName%_wide32:
            call t5, callPtrTag
        end

        move callee, t5
        size(callNarrow, callWide16, callWide32, macro (gen) gen() end)
    else
        call callee, callPtrTag
        if ARMv7
            # It is required in ARMv7 because global label definitions
            # for those architectures generates a set of instructions
            # that can clobber LLInt execution, resulting in unexpected
            # crashes.
            restoreStackPointerAfterCall()
            dispatchAfterCall(size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch)
        end
    end
    defineReturnLabel(opcodeName, size)
    restoreStackPointerAfterCall()
    dispatchAfterCall(size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch)

    if not ARM64E
        # It is required in ARMv7 because global label definitions
        # for those architectures generates a set of instructions
        # that can clobber LLInt execution, resulting in unexpected
        # crashes.
        macro labelNarrow()
            _js_trampoline_%opcodeName%:
        end

        macro labelWide16()
            _js_trampoline_%opcodeName%_wide16:
        end

        macro labelWide32()
            _js_trampoline_%opcodeName%_wide32:
        end
        size(labelNarrow, labelWide16, labelWide32, macro (gen) gen() end)
        crash()
    end
end

macro prepareForRegularCall(temp1, temp2, temp3, temp4, storeCodeBlock)
    storeCodeBlock(CodeBlock - CallerFrameAndPCSize[sp])
end

macro invokeForRegularCall(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, callee, maybeOldCFR, callPtrTag)
    callTargetFunction(opcodeName, size, opcodeStruct, dispatchAfterRegularCall, valueProfileName, dstVirtualRegister, dispatch, callee, callPtrTag)
end

macro invokeForRegularCallIgnoreResult(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, callee, maybeOldCFR, callPtrTag)
    callTargetFunction(opcodeName, size, opcodeStruct, dispatchAfterRegularCallIgnoreResult, valueProfileName, dstVirtualRegister, dispatch, callee, callPtrTag)
end

# t5 is metadata
macro prepareForSlowRegularCall()
end

# sp points to the new frame + CallerFrameAndPCSize
# We leave cfr temp4 to use it for untagging.
macro prepareForTailCall(temp1, temp2, temp3, temp4, storeCodeBlock)
    restoreCalleeSavesUsedByLLInt()

    loadi PayloadOffset + ArgumentCountIncludingThis[cfr], temp2
    loadp CodeBlock[cfr], temp1
    loadi CodeBlock::m_numParameters[temp1], temp1
    bilteq temp1, temp2, .noArityFixup
    move temp1, temp2

.noArityFixup:
    # We assume < 2^28 arguments
    muli SlotSize, temp2
    addi StackAlignment - 1 + CallFrameHeaderSize, temp2
    andi ~StackAlignmentMask, temp2

    move cfr, temp1
    addp temp2, temp1

    loadi PayloadOffset + ArgumentCountIncludingThis - CallerFrameAndPCSize[sp], temp2
    # We assume < 2^28 arguments
    muli SlotSize, temp2
    addi StackAlignment - 1 + CallFrameHeaderSize, temp2
    andi ~StackAlignmentMask, temp2

    if ARMv7 or ARM64 or ARM64E or C_LOOP or C_LOOP_WIN or RISCV64
        subi CallerFrameAndPCSize, temp2
        loadp CallerFrameAndPC::returnPC[cfr], lr
    else
        subp PtrSize, sp
        subi PtrSize, temp2
        loadp PtrSize[cfr], temp3
        storep temp3, [sp]
    end

    if ARM64E
        addp 16, cfr, temp4
    end

    subp temp2, temp1
    loadp [cfr], cfr

.copyLoop:
    if ARM64 and not ADDRESS64
        subi MachineRegisterSize, temp2
        loadq [sp, temp2, 1], temp3
        storeq temp3, [temp1, temp2, 1]
        btinz temp2, .copyLoop
    else
        subi PtrSize, temp2
        loadp [sp, temp2, 1], temp3
        storep temp3, [temp1, temp2, 1]
        btinz temp2, .copyLoop
    end

    move temp1, sp

    storeCodeBlock(CodeBlock - PrologueStackPointerDelta[sp])
end

macro invokeForTailCall(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, callee, maybeOldCFR, callPtrTag)
    if ARM64E
        move maybeOldCFR, a6
        move callee, a7
        leap _g_config, a5
        jmp JSCConfigGateMapOffset + (constexpr Gate::tailCall%callPtrTag%) * PtrSize[a5], NativeToJITGatePtrTag # %callPtrTag%
    else
        jmp callee, callPtrTag
    end
end

macro prepareForSlowTailCall()
    restoreCalleeSavesUsedByLLInt()
end

macro slowPathForCommonCall(opcodeName, size, opcodeStruct, dispatch, slowPath, prepareCall)
    slowPathForCall(opcodeName, size, opcodeStruct, m_valueProfile, m_dst, dispatch, slowPath, prepareCall)
end

macro slowPathForCall(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, slowPath, prepareCall)
    callCallSlowPath(
        slowPath,
        # Those parameters are r0 and r1
        macro (callee, calleeFramePtr)
            btpz calleeFramePtr, .dontUpdateSP
            move calleeFramePtr, sp
            prepareCall(t2, t3, t4, t1, macro(address) end)
        .dontUpdateSP:
            callTargetFunction(%opcodeName%_slow, size, opcodeStruct, dispatchAfterRegularCall, valueProfileName, dstVirtualRegister, dispatch, callee, JSEntrySlowPathPtrTag)
        end)
end

macro getterSetterOSRExitReturnPoint(opName, size)
    crash() # We don't reach this in straight line code. We only reach it via returning to the code below when reconstructing stack frames during OSR exit.

    defineReturnLabel(opName, size)

    restoreStackPointerAfterCall()
    loadi CallSiteIndex[cfr], PC
end

macro arrayProfile(offset, cellAndIndexingType, metadata, scratch)
    const cell = cellAndIndexingType
    const indexingType = cellAndIndexingType 
    loadi JSCell::m_structureID[cell], scratch
    storei scratch, offset + ArrayProfile::m_lastSeenStructureID[metadata]
    loadb JSCell::m_indexingTypeAndMisc[cell], indexingType
end

# Note that index is already sign-extended to be a register width.
macro getByValTypedArray(base, index, finishIntGetByVal, finishDoubleGetByVal, setLargeTypedArray, slowPath)
    # First lets check if we even have a typed array. This lets us do some boilerplate up front.
    loadb JSCell::m_type[base], t2
    subi FirstTypedArrayType, t2
    biaeq t2, NumberOfTypedArrayTypesExcludingBigIntArraysAndDataView, slowPath
    
    # Sweet, now we know that we have a typed array. Do some basic things now.

    btbnz JSArrayBufferView::m_mode[base], (constexpr isResizableOrGrowableSharedMode), slowPath
    if LARGE_TYPED_ARRAYS
        bqaeq index, JSArrayBufferView::m_length[base], slowPath
        bqbeq index, SmallTypedArrayMaxLength, .smallTypedArray
        setLargeTypedArray(t3)
.smallTypedArray:
    else
        biaeq index, JSArrayBufferView::m_length[base], slowPath
    end

    loadp JSArrayBufferView::m_vector[base], t3
    # length and scratch are intentionally undefined on this branch because they are not used on other platforms.
    if ARM64E
        const length = t6
        const scratch = t7
        loadq JSArrayBufferView::m_length[base], length
    end
    cagedPrimitive(t3, length, base, scratch)

    # Now bisect through the various types:
    #    Int8ArrayType,
    #    Uint8ArrayType,
    #    Uint8ClampedArrayType,
    #    Int16ArrayType,
    #    Uint16ArrayType,
    #    Int32ArrayType,
    #    Uint32ArrayType,
    #    Float32ArrayType,
    #    Float64ArrayType,
    #
    # Yet, we are not supporting BitInt64Array and BigUint64Array.

    bia t2, Uint16ArrayType - FirstTypedArrayType, .opGetByValAboveUint16Array

    # We have one of Int8ArrayType .. Uint16ArrayType.
    bia t2, Uint8ClampedArrayType - FirstTypedArrayType, .opGetByValInt16ArrayOrUint16Array

    # We have one of Int8ArrayType ... Uint8ClampedArrayType
    bia t2, Int8ArrayType - FirstTypedArrayType, .opGetByValUint8ArrayOrUint8ClampedArray

    # We have Int8ArrayType.
    loadbsi [t3, index], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint8ArrayOrUint8ClampedArray:
    bia t2, Uint8ArrayType - FirstTypedArrayType, .opGetByValUint8ClampedArray

    # We have Uint8ArrayType.
    loadb [t3, index], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint8ClampedArray:
    # We have Uint8ClampedArrayType.
    loadb [t3, index], t0
    finishIntGetByVal(t0, t1)

.opGetByValInt16ArrayOrUint16Array:
    # We have either Int16ArrayType or Uint16ClampedArrayType.
    bia t2, Int16ArrayType - FirstTypedArrayType, .opGetByValUint16Array

    # We have Int16ArrayType.
    loadhsi [t3, index, 2], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint16Array:
    # We have Uint16ArrayType.
    loadh [t3, index, 2], t0
    finishIntGetByVal(t0, t1)

.opGetByValAboveUint16Array:
    # We have one of Int32ArrayType .. Float64ArrayType.
    bia t2, Uint32ArrayType - FirstTypedArrayType, .opGetByValFloat32ArrayOrFloat64Array

    # We have either Int32ArrayType or Uint32ArrayType
    bia t2, Int32ArrayType - FirstTypedArrayType, .opGetByValUint32Array

    # We have Int32ArrayType.
    loadi [t3, index, 4], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint32Array:
    # We have Uint32ArrayType.
    # This is the hardest part because of large unsigned values.
    loadi [t3, index, 4], t0
    bilt t0, 0, slowPath # This case is still awkward to implement in LLInt.
    finishIntGetByVal(t0, t1)

.opGetByValFloat32ArrayOrFloat64Array:
    # We have one of Float32ArrayType or Float64ArrayType. Sadly, we cannot handle Float32Array
    # inline yet. That would require some offlineasm changes.
    bieq t2, Float32ArrayType - FirstTypedArrayType, slowPath

    # We have Float64ArrayType.
    loadd [t3, index, 8], ft0
    bdnequn ft0, ft0, slowPath
    finishDoubleGetByVal(ft0, t0, t1, t2)
end

macro skipIfIsRememberedOrInEden(cell, slowPath)
    memfence
    bba JSCell::m_cellState[cell], BlackThreshold, .done
    slowPath()
.done:
end

macro notifyWrite(set, slow)
    bbneq WatchpointSet::m_state[set], IsInvalidated, slow
end

macro varReadOnlyCheck(slowPath, scratch)
    loadp CodeBlock[cfr], scratch
    loadp CodeBlock::m_globalObject[scratch], scratch
    loadp JSGlobalObject::m_varReadOnlyWatchpointSet[scratch], scratch
    bbeq WatchpointSet::m_state[scratch], IsInvalidated, slowPath
end

macro checkSwitchToJIT(increment, action)
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_unlinkedCode[t0], t0
    baddis increment, (UnlinkedCodeBlock::m_llintExecuteCounter + BaselineExecutionCounter::m_counter)[t0], .continue
    action()
    .continue:
end

macro checkSwitchToJITForEpilogue()
    checkSwitchToJIT(
        10,
        macro ()
            callSlowPath(_llint_replace)
        end)
end

macro assertNotConstant(size, index)
    size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide16, FirstConstantRegisterIndexWide32, macro (FirstConstantRegisterIndex)
        assert(macro (ok) bilt index, FirstConstantRegisterIndex, ok end)
    end)
end

macro convertJSCalleeToVM(callee)
    btpnz callee, (constexpr PreciseAllocation::halfAlignment), .preciseAllocation
    andp MarkedBlockMask, callee
    loadp MarkedBlockHeaderOffset + MarkedBlock::Header::m_vm[callee], callee
    jmp .done
.preciseAllocation:
    loadp PreciseAllocationVMOffset[callee], callee
.done:
end

macro getVMFromCallFrame(vm, scratch)
if WEBASSEMBLY
        if JSVALUE64
            loadq Callee[cfr], vm
            move vm, scratch
            andq (constexpr JSValue::NativeCalleeMask), scratch
            bqeq scratch, (constexpr JSValue::NativeCalleeTag), .isWasmCallee
        else
            loadi Callee + TagOffset[cfr], scratch
            bieq scratch, (constexpr JSValue::NativeCalleeTag), .isWasmCallee
            loadp Callee + PayloadOffset[cfr], vm
        end
        convertJSCalleeToVM(vm)
        jmp .loaded
    .isWasmCallee:
        loadp CodeBlock + PayloadOffset[cfr], vm
        loadp Wasm::Instance::m_vm[vm], vm
    .loaded:
else
    loadp Callee + PayloadOffset[cfr], vm
    convertJSCalleeToVM(vm)
end
end

# Do the bare minimum required to execute code. Sets up the PC, leave the CodeBlock*
# in t1. May also trigger prologue entry OSR.
macro prologue(osrSlowPath, traceSlowPath)
    # Set up the call frame and check if we should OSR.
    preserveCallerPCAndCFR()

    if TRACING
        subp maxFrameExtentForSlowPathCall, sp
        callSlowPath(traceSlowPath)
        addp maxFrameExtentForSlowPathCall, sp
    end
    loadp CodeBlock[cfr], t1
    if not (C_LOOP or C_LOOP_WIN)
        loadp CodeBlock::m_unlinkedCode[t1], t0
        baddis 5, (UnlinkedCodeBlock::m_llintExecuteCounter + BaselineExecutionCounter::m_counter)[t0], .continue
        if JSVALUE64
            move cfr, a0
            move PC, a1
            cCall2(osrSlowPath)
        else
            # We are after the function prologue, but before we have set up sp from the CodeBlock.
            # Temporarily align stack pointer for this call.
            subp 8, sp
            move cfr, a0
            move PC, a1
            cCall2(osrSlowPath)
            addp 8, sp
        end
        btpz r0, .recover
        move cfr, sp # restore the previous sp
        # pop the callerFrame since we will jump to a function that wants to save it
        if ARM64 or RISCV64
            pop lr, cfr
        elsif ARM64E
            # untagReturnAddress will be performed in Gate::entryOSREntry.
            pop lr, cfr
        elsif ARMv7
            pop cfr
            pop lr
        else
            pop cfr
        end

        if ARM64E
            move r0, a0
            leap _g_config, a2
            jmp JSCConfigGateMapOffset + (constexpr Gate::entryOSREntry) * PtrSize[a2], NativeToJITGatePtrTag # JSEntryPtrTag
        else
            jmp r0, JSEntryPtrTag
        end
    .recover:
        loadp CodeBlock[cfr], t1
    .continue:
    end

    preserveCalleeSavesUsedByLLInt()

    # Set up the PC.
    loadp CodeBlock::m_instructionsRawPointer[t1], PB
    move 0, PC

    # Get new sp in t0 and check stack height.
    getFrameRegisterSizeForCodeBlock(t1, t0)
    subp cfr, t0, t0
    bpa t0, cfr, .needStackCheck
    loadp CodeBlock::m_vm[t1], t2
    if C_LOOP or C_LOOP_WIN
        bpbeq VM::m_cloopStackLimit[t2], t0, .stackHeightOK
    else
        bpbeq VM::m_softStackLimit[t2], t0, .stackHeightOK
    end

.needStackCheck:
    # Stack height check failed - need to call a slow_path.
    # Set up temporary stack pointer for call including callee saves
    subp maxFrameExtentForSlowPathCall, sp
    callSlowPath(_llint_stack_check)
    bpeq r1, 0, .stackHeightOKGetCodeBlock

    # We're throwing before the frame is fully set up. This frame will be
    # ignored by the unwinder. So, let's restore the callee saves before we
    # start unwinding. We need to do this before we change the cfr.
    restoreCalleeSavesUsedByLLInt()

    move r1, cfr
    jmp _llint_throw_from_slow_path_trampoline

.stackHeightOKGetCodeBlock:
    # Stack check slow path returned that the stack was ok.
    # Since they were clobbered, need to get CodeBlock and new sp
    loadp CodeBlock[cfr], t1
    getFrameRegisterSizeForCodeBlock(t1, t0)
    subp cfr, t0, t0

.stackHeightOK:
    if X86_64 or ARM64
        # We need to start zeroing from sp as it has been adjusted after saving callee saves.
        move sp, t2
        move t0, sp
.zeroStackLoop:
        bpeq sp, t2, .zeroStackDone
        subp PtrSize, t2
        storep 0, [t2]
        jmp .zeroStackLoop
.zeroStackDone:
    else
        move t0, sp
    end

    loadp CodeBlock::m_metadata[t1], metadataTable

    if JSVALUE64
        move TagNumber, numberTag
        addq TagOther, numberTag, notCellMask
    end
end

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
# Must call dispatch(0) after calling this.
macro functionInitialization(profileArgSkip)
    # Profile the arguments. Unfortunately, we have no choice but to do this. This
    # code is pretty horrendous because of the difference in ordering between
    # arguments and value profiles, the desire to have a simple loop-down-to-zero
    # loop, and the desire to use only three registers so as to preserve the PC and
    # the code block. It is likely that this code should be rewritten in a more
    # optimal way for architectures that have more than five registers available
    # for arbitrary use in the interpreter.
    loadi CodeBlock::m_numParameters[t1], t0
    addp -profileArgSkip, t0
    assert(macro (ok) bpgteq t0, 0, ok end)
    btpz t0, .argumentProfileDone
    loadp CodeBlock::m_argumentValueProfiles + ArgumentValueProfileFixedVector::m_storage[t1], t3
    btpz t3, .argumentProfileDone # When we can't JIT, we don't allocate any argument value profiles.
    mulp sizeof ArgumentValueProfile, t0, t2 # Aaaaahhhh! Need strength reduction!
    lshiftp 3, t0 # offset of last JSValue arguments on the stack.
    addp (constexpr (ArgumentValueProfileFixedVector::Storage::offsetOfData())), t3
    addp t2, t3 # pointer to end of ValueProfile array in the value profile array.
.argumentProfileLoop:
    if JSVALUE64
        loadq ThisArgumentOffset - 8 + profileArgSkip * 8[cfr, t0], t2
        subp sizeof ArgumentValueProfile, t3
        storeq t2, profileArgSkip * sizeof ArgumentValueProfile + ValueProfile::m_buckets[t3]
    else
        subp sizeof ArgumentValueProfile, t3
        loadi ThisArgumentOffset + TagOffset - 8 + profileArgSkip * 8[cfr, t0], t1
        loadi ThisArgumentOffset + PayloadOffset - 8 + profileArgSkip * 8[cfr, t0], t2
        storeJSValueConcurrent(
            macro (val, offset)
                storei val, profileArgSkip * sizeof ArgumentValueProfile + ValueProfile::m_buckets + offset[t3]
            end,
            t1,
            t2
        )
    end
    baddpnz -8, t0, .argumentProfileLoop
.argumentProfileDone:
end

macro doReturn()
    restoreCalleeSavesUsedByLLInt()
    restoreCallerPCAndCFR()
    if ARM64E
        leap _g_config, a2
        jmp JSCConfigGateMapOffset + (constexpr Gate::returnFromLLInt) * PtrSize[a2], NativeToJITGatePtrTag
    else
        ret
    end
end

# stub to call into JavaScript or Native functions
# EncodedJSValue vmEntryToJavaScript(void* code, VM* vm, ProtoCallFrame* protoFrame)
# EncodedJSValue vmEntryToNativeFunction(void* code, VM* vm, ProtoCallFrame* protoFrame)

if C_LOOP or C_LOOP_WIN
    _llint_vm_entry_to_javascript:
else
    global _vmEntryToJavaScript
    _vmEntryToJavaScript:
end
    doVMEntry(makeJavaScriptCall)


if C_LOOP or C_LOOP_WIN
    _llint_vm_entry_to_native:
else
    global _vmEntryToNative
    _vmEntryToNative:
end
    doVMEntry(makeHostFunctionCall)

if ARM64E
    global _vmEntryToYarrJITAfter
end
global _vmEntryToYarrJIT
_vmEntryToYarrJIT:
    functionPrologue()
if ARM64E
    jmp t5, YarrEntryPtrTag
    _vmEntryToYarrJITAfter:
end
    functionEpilogue()
    ret

# a0, a1, a2 are used. a3 contains function address.
# EncodedJSValue(JIT_OPERATION_ATTRIBUTES*)(JSGlobalObject*, EncodedJSValue, PropertyName, void*);
global _vmEntryCustomGetter
_vmEntryCustomGetter:
if ARM64E
    jmp a3, CustomAccessorPtrTag
else
    crash()
end

# a0, a1, a2, a3 are used. a4 contains function address.
# bool (JIT_OPERATION_ATTRIBUTES*)(JSGlobalObject*, EncodedJSValue, EncodedJSValue, PropertyName, void*);
global _vmEntryCustomSetter
_vmEntryCustomSetter:
if ARM64E
    jmp a4, CustomAccessorPtrTag
else
    crash()
end

# a0 and a1 are used. a2 contains function address.
global _vmEntryHostFunction
_vmEntryHostFunction:
    jmp a2, HostFunctionPtrTag

# unsigned vmEntryToCSSJIT(uintptr_t, uintptr_t, uintptr_t, const void* codePtr);
if ARM64E
    global _vmEntryToCSSJITAfter
end
global _vmEntryToCSSJIT
_vmEntryToCSSJIT:
    functionPrologue()
    jmp t3, CSSSelectorPtrTag
if ARM64E
    _vmEntryToCSSJITAfter:
end
    functionEpilogue()
    ret

if not (C_LOOP or C_LOOP_WIN)
    # void sanitizeStackForVMImpl(VM* vm)
    global _sanitizeStackForVMImpl
    _sanitizeStackForVMImpl:
        tagReturnAddress sp
        # We need three non-aliased caller-save registers. We are guaranteed
        # this for a0, a1 and a2 on all architectures. Beware also that
        # offlineasm might use temporary registers when lowering complex
        # instructions on some platforms, which might be callee-save. To avoid
        # this, we use the simplest instructions so we never need a temporary
        # and hence don't clobber any callee-save registers.
        if X86 or X86_WIN
            loadp 4[sp], a0
        end
        const address = a1
        const scratch = a2

        move VM::m_lastStackTop, scratch
        addp scratch, a0
        loadp [a0], address
        move sp, scratch
        storep scratch, [a0]
        move sp, a0

        bpbeq sp, address, .zeroFillDone
        move address, sp

        # Filling stack space from |address| to sp (stored in |a0| now).
        if (ARM64 or ARM64E) and ADDRESS64
            # Because of ARM64 calling convention, stack-pointer is already 16-byte aligned.
            # Let's check address is aligned or not to use 16-byte zero-fill.
            assert(macro (ok)  btpz a0, (PtrSize * 2 - 1), ok end)
            btpz address, (PtrSize * 2 - 1), .zeroFillLoop
            # If it is not aligned, then store pointer-size and increment.
            emit "str xzr, [x1], #8" # address is a1, thus x1
            bpbeq a0, address, .zeroFillDone
            assert(macro (ok)  btpz address, (PtrSize * 2 - 1), ok end)
        .zeroFillLoop:
            # Use non-temporal store-pair (stnp) since these stack values are meaningless to the execution.
            # Avoid polluting CPU cache by using stnp.
            emit "stnp xzr, xzr, [x1]" # address is a1, thus x1
            addp PtrSize * 2, address
            bpa a0, address, .zeroFillLoop
        else
            move 0, scratch
        .zeroFillLoop:
            storep scratch, [address]
            addp PtrSize, address
            bpa a0, address, .zeroFillLoop
        end

    .zeroFillDone:
        move a0, sp
        ret

    # VMEntryRecord* vmEntryRecord(const EntryFrame* entryFrame)
    global _vmEntryRecord
    _vmEntryRecord:
        tagReturnAddress sp
        if X86 or X86_WIN
            loadp 4[sp], a0
        end

        vmEntryRecord(a0, r0)
        ret
end

if ARM64E
    if JIT_CAGE
        # void* jitCagePtr(void* pointer, uintptr_t tag)
        emit ".globl _jitCagePtr"
        emit "_jitCagePtr:"
            tagReturnAddress sp
            leap _g_config, t2
            jmp JSCConfigGateMapOffset + (constexpr Gate::jitCagePtr) * PtrSize[t2], NativeToJITGatePtrTag
    end
    global _jitCagePtrGateAfter
    _jitCagePtrGateAfter:
        ret

    global _tailCallJSEntryTrampoline
    _tailCallJSEntryTrampoline:
        untagReturnAddress a6
        jmp a7, JSEntryPtrTag

    global _tailCallJSEntrySlowPathTrampoline
    _tailCallJSEntrySlowPathTrampoline:
        untagReturnAddress a6
        jmp a7, JSEntryPtrTag

    global _tailCallWithoutUntagJSEntryTrampoline
    _tailCallWithoutUntagJSEntryTrampoline:
        jmp a7, JSEntryPtrTag

    global _exceptionHandlerTrampoline
    _exceptionHandlerTrampoline:
        jmp a0, ExceptionHandlerPtrTag

    global _returnFromLLIntTrampoline
    _returnFromLLIntTrampoline:
        ret

    global _jsTrampolineProgramPrologue
    _jsTrampolineProgramPrologue:
        tagReturnAddress sp
        jmp _llint_program_prologue

    global _jsTrampolineModuleProgramPrologue
    _jsTrampolineModuleProgramPrologue:
        tagReturnAddress sp
        jmp _llint_module_program_prologue

    global _jsTrampolineEvalPrologue
    _jsTrampolineEvalPrologue:
        tagReturnAddress sp
        jmp _llint_eval_prologue

    global _jsTrampolineFunctionForCallPrologue
    _jsTrampolineFunctionForCallPrologue:
        tagReturnAddress sp
        jmp _llint_function_for_call_prologue

    global _jsTrampolineFunctionForConstructPrologue
    _jsTrampolineFunctionForConstructPrologue:
        tagReturnAddress sp
        jmp _llint_function_for_construct_prologue

    global _jsTrampolineFunctionForCallArityCheckPrologue
    _jsTrampolineFunctionForCallArityCheckPrologue:
        tagReturnAddress sp
        jmp _llint_function_for_call_arity_check

    global _jsTrampolineFunctionForConstructArityCheckPrologue
    _jsTrampolineFunctionForConstructArityCheckPrologue:
        tagReturnAddress sp
        jmp _llint_function_for_construct_arity_check
end

if C_LOOP or C_LOOP_WIN
    # Dummy entry point the C Loop uses to initialize.
    _llint_entry:
        crash()
else
    macro initPCRelative(kind, pcBase)
        if X86_64 or X86_64_WIN or X86 or X86_WIN
            call _%kind%_relativePCBase
        _%kind%_relativePCBase:
            pop pcBase
        elsif ARM64 or ARM64E
        elsif ARMv7
        _%kind%_relativePCBase:
            move pc, pcBase
            subp 3, pcBase   # Need to back up the PC and set the Thumb2 bit
        end
    end

    # The PC base is in t3, as this is what _llint_entry leaves behind through
    # initPCRelative(t3)
    macro setEntryAddressCommon(kind, index, label, map)
        if X86_64
            leap (label - _%kind%_relativePCBase)[t3], t4
            move index, t5
            storep t4, [map, t5, 8]
        elsif X86_64_WIN
            leap (label - _%kind%_relativePCBase)[t3], t4
            move index, t0
            storep t4, [map, t0, 8]
        elsif X86 or X86_WIN
            leap (label - _%kind%_relativePCBase)[t3], t4
            move index, t5
            storep t4, [map, t5, 4]
        elsif ARM64 or RISCV64
            pcrtoaddr label, t3
            move index, t4
            storep t3, [map, t4, PtrSize]
        elsif ARM64E
            pcrtoaddr label, t3
            move index, t4
            leap [map, t4, PtrSize], t4
            tagCodePtr t3, BytecodePtrTag, AddressDiversified, t4
            storep t3, [t4]
        elsif ARMv7
            mvlbl (label - _%kind%_relativePCBase), t4
            addp t4, t3, t4
            move index, t5
            storep t4, [map, t5, 4]
        end
    end



    macro includeEntriesAtOffset(kind, fn)
        macro setEntryAddress(index, label)
            setEntryAddressCommon(kind, index, label, a0)
        end

        macro setEntryAddressWide16(index, label)
             setEntryAddressCommon(kind, index, label, a1)
        end

        macro setEntryAddressWide32(index, label)
             setEntryAddressCommon(kind, index, label, a2)
        end

        fn()
    end


macro entry(kind, initialize)
    global _%kind%_entry
    _%kind%_entry:
        functionPrologue()
        pushCalleeSaves()
        if X86 or X86_WIN
            loadp 20[sp], a0
            loadp 24[sp], a1
            loadp 28[sp], a2
        end

        initPCRelative(kind, t3)

        # Include generated bytecode initialization file.
        includeEntriesAtOffset(kind, initialize)

        if X86_64_WIN or C_LOOP_WIN
            leap JSCConfig + constexpr JSC::offsetOfJSCConfigInitializeHasBeenCalled, t3
            bbeq [t3], 0, .notFrozen
        else
            leap _g_config, t3
            bbeq JSCConfigOffset + constexpr JSC::offsetOfJSCConfigInitializeHasBeenCalled[t3], 0, .notFrozen
        end
        crash()
    .notFrozen:

        popCalleeSaves()
        functionEpilogue()
        ret
end

# Entry point for the llint to initialize.
entry(llint, macro()
    include InitBytecodes
end)

end // not (C_LOOP or C_LOOP_WIN)

_llint_op_wide16:
    nextInstructionWide16()

_llint_op_wide32:
    nextInstructionWide32()

macro noWide(label)
_%label%_wide16:
    crash()

_%label%_wide32:
    crash()
end

noWide(llint_op_wide16)
noWide(llint_op_wide32)
noWide(llint_op_enter)

op(llint_program_prologue, macro ()
    prologue(_llint_entry_osr, _llint_trace_prologue)
    dispatch(0)
end)


op(llint_module_program_prologue, macro ()
    prologue(_llint_entry_osr, _llint_trace_prologue)
    dispatch(0)
end)


op(llint_eval_prologue, macro ()
    prologue(_llint_entry_osr, _llint_trace_prologue)
    dispatch(0)
end)


op(llint_function_for_call_prologue, macro ()
    prologue(_llint_entry_osr_function_for_call, _llint_trace_prologue_function_for_call)
    functionInitialization(0)
    dispatch(0)
end)
    

op(llint_function_for_construct_prologue, macro ()
    prologue(_llint_entry_osr_function_for_construct, _llint_trace_prologue_function_for_construct)
    functionInitialization(1)
    dispatch(0)
end)
    

op(llint_function_for_call_arity_check, macro ()
    prologue(_llint_entry_osr_function_for_call_arityCheck, _llint_trace_arityCheck_for_call)
    functionArityCheck(llint_function_for_call_arity_check, .functionForCallBegin)
.functionForCallBegin:
    functionInitialization(0)
    dispatch(0)
end)

op(llint_function_for_construct_arity_check, macro ()
    prologue(_llint_entry_osr_function_for_construct_arityCheck, _llint_trace_arityCheck_for_construct)
    functionArityCheck(llint_function_for_construct_arity_check, .functionForConstructBegin)
.functionForConstructBegin:
    functionInitialization(1)
    dispatch(0)
end)

# Need these stub labels to make js_trampoline_llint_function_for_call_arity_check_untag etc. LLInt helper opcodes.
_js_trampoline_llint_function_for_call_arity_check_untag_wide16:
_js_trampoline_llint_function_for_call_arity_check_untag_wide32:
_js_trampoline_llint_function_for_call_arity_check_tag_wide16:
_js_trampoline_llint_function_for_call_arity_check_tag_wide32:
_js_trampoline_llint_function_for_construct_arity_check_untag_wide16:
_js_trampoline_llint_function_for_construct_arity_check_untag_wide32:
_js_trampoline_llint_function_for_construct_arity_check_tag_wide16:
_js_trampoline_llint_function_for_construct_arity_check_tag_wide32:
    crash()

# Value-representation-specific code.
if JSVALUE64
    include LowLevelInterpreter64
else
    include LowLevelInterpreter32_64
end


# Value-representation-agnostic code.
macro slowPathOp(opcodeName)
    llintOp(op_%opcodeName%, unused, macro (unused, unused, dispatch)
        callSlowPath(_slow_path_%opcodeName%)
        dispatch()
    end)
end

slowPathOp(create_cloned_arguments)
slowPathOp(create_direct_arguments)
slowPathOp(create_lexical_environment)
slowPathOp(create_rest)
slowPathOp(create_scoped_arguments)
slowPathOp(create_this)
slowPathOp(create_promise)
slowPathOp(create_generator)
slowPathOp(create_async_generator)
slowPathOp(define_accessor_property)
slowPathOp(define_data_property)
slowPathOp(get_by_val_with_this)

if not JSVALUE64
    slowPathOp(get_prototype_of)
end

slowPathOp(instanceof_custom)
slowPathOp(is_callable)
slowPathOp(is_constructor)
slowPathOp(new_array_buffer)
slowPathOp(new_array_with_spread)
slowPathOp(new_array_with_species)
slowPathOp(push_with_scope)
slowPathOp(put_by_id_with_this)
slowPathOp(put_by_val_with_this)
slowPathOp(resolve_scope_for_hoisting_func_decl_in_eval)
slowPathOp(spread)
slowPathOp(strcat)
slowPathOp(throw_static_error)
slowPathOp(typeof)
slowPathOp(typeof_is_object)
slowPathOp(unreachable)
slowPathOp(new_promise)
slowPathOp(new_generator)

macro llintSlowPathOp(opcodeName)
    llintOp(op_%opcodeName%, unused, macro (unused, unused, dispatch)
        callSlowPath(_llint_slow_path_%opcodeName%)
        dispatch()
    end)
end

llintSlowPathOp(in_by_id)
llintSlowPathOp(in_by_val)
llintSlowPathOp(has_private_name)
llintSlowPathOp(has_private_brand)
llintSlowPathOp(del_by_id)
llintSlowPathOp(del_by_val)
llintSlowPathOp(instanceof)
llintSlowPathOp(new_array)
llintSlowPathOp(new_array_with_size)
llintSlowPathOp(new_async_func)
llintSlowPathOp(new_async_func_exp)
llintSlowPathOp(new_async_generator_func)
llintSlowPathOp(new_async_generator_func_exp)
llintSlowPathOp(new_func)
llintSlowPathOp(new_func_exp)
llintSlowPathOp(new_generator_func)
llintSlowPathOp(new_generator_func_exp)
llintSlowPathOp(new_object)
llintSlowPathOp(new_regexp)
llintSlowPathOp(put_getter_by_id)
llintSlowPathOp(put_getter_by_val)
llintSlowPathOp(put_getter_setter_by_id)
llintSlowPathOp(put_setter_by_id)
llintSlowPathOp(put_setter_by_val)
llintSlowPathOp(set_function_name)
llintSlowPathOp(throw)
llintSlowPathOp(get_by_id_with_this)

llintOp(op_switch_string, unused, macro (unused, unused, unused)
    callSlowPath(_llint_slow_path_switch_string)
    nextInstruction()
end)


equalityComparisonOp(eq, OpEq,
    macro (left, right, result) cieq left, right, result end)


equalityComparisonOp(neq, OpNeq,
    macro (left, right, result) cineq left, right, result end)


compareUnsignedOp(below, OpBelow,
    macro (left, right, result) cib left, right, result end)


compareUnsignedOp(beloweq, OpBeloweq,
    macro (left, right, result) cibeq left, right, result end)


llintOpWithJump(op_jmp, OpJmp, macro (size, get, jump, dispatch)
    jump(m_targetLabel)
end)


llintJumpTrueOrFalseOp(jtrue, OpJtrue, 
    # Misc primitive
    macro (value, target) btinz value, 1, target end,
    # Truthy Cell
    macro (dispatch) end)


llintJumpTrueOrFalseOp(jfalse, OpJfalse,
    # Misc primitive
    macro (value, target) btiz value, 1, target end,
    # Truthy Cell
    macro (dispatch) dispatch() end)

compareOp(greater, OpGreater,
    macro (left, right, result) cigt left, right, result end,
    macro (left, right, result) cdgt left, right, result end)

compareOp(greatereq, OpGreatereq,
    macro (left, right, result) cigteq left, right, result end,
    macro (left, right, result) cdgteq left, right, result end)

compareOp(less, OpLess,
    macro (left, right, result) cilt left, right, result end,
    macro (left, right, result) cdlt left, right, result end)

compareOp(lesseq, OpLesseq,
    macro (left, right, result) cilteq left, right, result end,
    macro (left, right, result) cdlteq left, right, result end)

compareJumpOp(
    jless, OpJless,
    macro (left, right, target) bilt left, right, target end,
    macro (left, right, target) bdlt left, right, target end)


compareJumpOp(
    jnless, OpJnless,
    macro (left, right, target) bigteq left, right, target end,
    macro (left, right, target) bdgtequn left, right, target end)


compareJumpOp(
    jgreater, OpJgreater,
    macro (left, right, target) bigt left, right, target end,
    macro (left, right, target) bdgt left, right, target end)


compareJumpOp(
    jngreater, OpJngreater,
    macro (left, right, target) bilteq left, right, target end,
    macro (left, right, target) bdltequn left, right, target end)


compareJumpOp(
    jlesseq, OpJlesseq,
    macro (left, right, target) bilteq left, right, target end,
    macro (left, right, target) bdlteq left, right, target end)


compareJumpOp(
    jnlesseq, OpJnlesseq,
    macro (left, right, target) bigt left, right, target end,
    macro (left, right, target) bdgtun left, right, target end)


compareJumpOp(
    jgreatereq, OpJgreatereq,
    macro (left, right, target) bigteq left, right, target end,
    macro (left, right, target) bdgteq left, right, target end)


compareJumpOp(
    jngreatereq, OpJngreatereq,
    macro (left, right, target) bilt left, right, target end,
    macro (left, right, target) bdltun left, right, target end)


equalityJumpOp(
    jeq, OpJeq,
    macro (left, right, target) bieq left, right, target end)


equalityJumpOp(
    jneq, OpJneq,
    macro (left, right, target) bineq left, right, target end)


compareUnsignedJumpOp(
    jbelow, OpJbelow,
    macro (left, right, target) bib left, right, target end)


compareUnsignedJumpOp(
    jbeloweq, OpJbeloweq,
    macro (left, right, target) bibeq left, right, target end)


preOp(inc, OpInc,
    macro (value, slow) baddio 1, value, slow end)

preOp(dec, OpDec,
    macro (value, slow) bsubio 1, value, slow end)


llintOp(op_loop_hint, OpLoopHint, macro (unused, unused, dispatch)
    checkSwitchToJITForLoop()
    dispatch()
end)


macro checkTraps(dispatch)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_vm[t1], t1
    loadi VM::m_traps+VMTraps::m_trapBits[t1], t0
    andi VMTrapsAsyncEvents, t0
    btpnz t0, .handleTraps
.afterHandlingTraps:
    dispatch()
.handleTraps:
    callTrapHandler(.throwHandler)
    jmp .afterHandlingTraps
.throwHandler:
    jmp _llint_throw_from_slow_path_trampoline
end

llintOp(op_check_traps, OpCheckTraps, macro (unused, unused, dispatch)
    checkTraps(dispatch)
end)


# Returns the packet pointer in t0.
macro acquireShadowChickenPacket(slow)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_vm[t1], t1
    loadp VM::m_shadowChicken[t1], t2
    loadp ShadowChicken::m_logCursor[t2], t0
    bpaeq t0, ShadowChicken::m_logEnd[t2], slow
    addp sizeof ShadowChicken::Packet, t0, t1
    storep t1, ShadowChicken::m_logCursor[t2]
end


llintOp(op_nop, OpNop, macro (unused, unused, dispatch)
    dispatch()
end)


# we can't use callOp because we can't pass `call` as the opcode name, since it's an instruction name
commonCallOp(op_call, OpCall, prepareForRegularCall, invokeForRegularCall, prepareForSlowRegularCall, macro (getu, metadata)
    arrayProfileForCall(OpCall, getu)
end, dispatchAfterRegularCall)

commonCallOp(op_construct, OpConstruct, prepareForRegularCall, invokeForRegularCall, prepareForSlowRegularCall, macro (getu, metadata)
end, dispatchAfterRegularCall)

commonCallOp(op_tail_call, OpTailCall, prepareForTailCall, invokeForTailCall, prepareForSlowTailCall, macro (getu, metadata)
    arrayProfileForCall(OpTailCall, getu)
    checkSwitchToJITForEpilogue()
    # reload metadata since checkSwitchToJITForEpilogue() might have trashed t5
    metadata(t5, t0)
end, dispatchAfterTailCall)

commonCallOp(op_call_ignore_result, OpCallIgnoreResult, prepareForRegularCall, invokeForRegularCallIgnoreResult, prepareForSlowRegularCall, macro (getu, metadata)
    arrayProfileForCall(OpCallIgnoreResult, getu)
end, dispatchAfterRegularCallIgnoreResult)

macro branchIfException(exceptionTarget)
    loadp CodeBlock[cfr], t3
    loadp CodeBlock::m_vm[t3], t3
    btpz VM::m_exception[t3], .noException
    jmp exceptionTarget
.noException:    
end


llintOpWithMetadata(op_call_varargs, OpCallVarargs, macro (size, get, dispatch, metadata, return)
    doCallVarargs(op_call_varargs, size, get, OpCallVarargs, m_valueProfile, m_dst, dispatch, metadata, _llint_slow_path_size_frame_for_varargs, _llint_slow_path_call_varargs, prepareForRegularCall, invokeForRegularCall, prepareForSlowRegularCall, dispatchAfterRegularCall)
end)

llintOpWithMetadata(op_tail_call_varargs, OpTailCallVarargs, macro (size, get, dispatch, metadata, return)
    checkSwitchToJITForEpilogue()
    # We lie and perform the tail call instead of preparing it since we can't
    # prepare the frame for a call opcode
    doCallVarargs(op_tail_call_varargs, size, get, OpTailCallVarargs, m_valueProfile, m_dst, dispatch, metadata, _llint_slow_path_size_frame_for_varargs, _llint_slow_path_tail_call_varargs, prepareForTailCall, invokeForTailCall, prepareForSlowTailCall, dispatchAfterTailCall)
end)


llintOpWithMetadata(op_tail_call_forward_arguments, OpTailCallForwardArguments, macro (size, get, dispatch, metadata, return)
    checkSwitchToJITForEpilogue()
    # We lie and perform the tail call instead of preparing it since we can't
    # prepare the frame for a call opcode
    doCallVarargs(op_tail_call_forward_arguments, size, get, OpTailCallForwardArguments, m_valueProfile, m_dst, dispatch, metadata, _llint_slow_path_size_frame_for_forward_arguments, _llint_slow_path_tail_call_forward_arguments, prepareForTailCall, invokeForTailCall, prepareForSlowTailCall, dispatchAfterTailCall)
end)


llintOpWithMetadata(op_construct_varargs, OpConstructVarargs, macro (size, get, dispatch, metadata, return)
    doCallVarargs(op_construct_varargs, size, get, OpConstructVarargs, m_valueProfile, m_dst, dispatch, metadata, _llint_slow_path_size_frame_for_varargs, _llint_slow_path_construct_varargs, prepareForRegularCall, invokeForRegularCall, prepareForSlowRegularCall, dispatchAfterRegularCall)
end)


# Eval is executed in one of two modes:
#
# 1) We find that we're really invoking eval() in which case the
#    execution is perfomed entirely inside the slow_path, and it
#    returns the PC of a function that just returns the return value
#    that the eval returned.
#
# 2) We find that we're invoking something called eval() that is not
#    the real eval. Then the slow_path returns the PC of the thing to
#    call, and we call it.
#
# This allows us to handle two cases, which would require a total of
# up to four pieces of state that cannot be easily packed into two
# registers (C functions can return up to two registers, easily):
#
# - The call frame register. This may or may not have been modified
#   by the slow_path, but the convention is that it returns it. It's not
#   totally clear if that's necessary, since the cfr is callee save.
#   But that's our style in this here interpreter so we stick with it.
#
# - A bit to say if the slow_path successfully executed the eval and has
#   the return value, or did not execute the eval but has a PC for us
#   to call.
#
# - Either:
#   - The JS return value (two registers), or
#
#   - The PC to call.
#
# It turns out to be easier to just always have this return the cfr
# and a PC to call, and that PC may be a dummy thunk that just
# returns the JS value that the eval returned.

_llint_op_call_direct_eval:
    slowPathForCommonCall(
        op_call_direct_eval,
        narrow,
        OpCallDirectEval,
        macro () dispatchOp(narrow, op_call_direct_eval) end,
        _llint_slow_path_call_direct_eval,
        prepareForRegularCall)

_llint_op_call_direct_eval_wide16:
    slowPathForCommonCall(
        op_call_direct_eval,
        wide16,
        OpCallDirectEval,
        macro () dispatchOp(wide16, op_call_direct_eval) end,
        _llint_slow_path_call_direct_eval_wide16,
        prepareForRegularCall)

_llint_op_call_direct_eval_wide32:
    slowPathForCommonCall(
        op_call_direct_eval,
        wide32,
        OpCallDirectEval,
        macro () dispatchOp(wide32, op_call_direct_eval) end,
        _llint_slow_path_call_direct_eval_wide32,
        prepareForRegularCall)


commonOp(llint_generic_return_point, macro () end, macro (size)
    dispatchAfterRegularCall(size, OpCallDirectEval, m_valueProfile, m_dst, macro ()
        dispatchOp(size, op_call_direct_eval)
    end)
end)


llintOp(op_identity_with_profile, OpIdentityWithProfile, macro (unused, unused, dispatch)
    dispatch()
end)


llintOp(op_yield, OpYield, macro (unused, unused, unused)
    notSupported()
end)


llintOp(op_create_generator_frame_environment, OpYield, macro (unused, unused, unused)
    notSupported()
end)


llintOp(op_debug, OpDebug, macro (unused, unused, dispatch)
    loadp CodeBlock[cfr], t0
    loadi CodeBlock::m_debuggerRequests[t0], t0
    btiz t0, .opDebugDone
    callSlowPath(_llint_slow_path_debug)
.opDebugDone:                    
    dispatch()
end)


llintOp(op_super_sampler_begin, OpSuperSamplerBegin, macro (unused, unused, dispatch)
    superSamplerBegin(t1)
    dispatch()
end)

llintOp(op_super_sampler_end, OpSuperSamplerEnd, macro (unused, unused, dispatch)
    superSamplerEnd(t1)
    dispatch()
end)

op(llint_native_call_trampoline, macro ()
    nativeCallTrampoline(NativeExecutable::m_function)
end)


op(llint_native_construct_trampoline, macro ()
    nativeCallTrampoline(NativeExecutable::m_constructor)
end)


op(llint_internal_function_call_trampoline, macro ()
    internalFunctionCallTrampoline(InternalFunction::m_functionForCall)
end)


op(llint_internal_function_construct_trampoline, macro ()
    internalFunctionCallTrampoline(InternalFunction::m_functionForConstruct)
end)

# 64bit:t0 32bit(t0,t1) is callee
# t2 is CallLinkInfo*
macro linkFor(function)
    functionPrologue()
    move t2, a1
    move cfr, a0
    cCall2(function)
    functionEpilogue()
    untagReturnAddress sp
    btpnz r1, .throw
    jmp r0, JSEntryPtrTag
.throw:
    functionPrologue()
    jmp _llint_throw_from_slow_path_trampoline
end

# 64bit:t0 32bit(t0,t1) is callee
# t2 is CallLinkInfo*
macro virtualThunkFor(offsetOfJITCodeWithArityCheck, offsetOfCodeBlock, internalFunctionTrampoline, slowCase)
    addi 1, CallLinkInfo::m_slowPathCount[t2]
    if JSVALUE64
        btqnz t0, NotCellMask, slowCase
    else
        bineq t1, CellTag, slowCase
    end
    bbneq JSCell::m_type[t0], JSFunctionType, .notJSFunction
    loadp JSFunction::m_executableOrRareData[t0], t5
    btpz t5, (constexpr JSFunction::rareDataTag), .isExecutable
    loadp (FunctionRareData::m_executable - (constexpr JSFunction::rareDataTag))[t5], t5
.isExecutable:
    loadp offsetOfJITCodeWithArityCheck[t5], t4
    btpz t4, slowCase # When jumping to slowCase, t0, t1, t2, needs to be unmodified.
    move t4, t1
    move 0, t0
    bbneq JSCell::m_type[t5], FunctionExecutableType, .callCode
    loadp offsetOfCodeBlock[t5], t0
.callCode:
    storep t0, CodeBlock - PrologueStackPointerDelta[sp]
    jmp t1, JSEntryPtrTag
.notJSFunction:
    bbneq JSCell::m_type[t0], InternalFunctionType, slowCase
    jmp internalFunctionTrampoline
end

# 64bit:t0 32bit(t0,t1) is callee
# t2 is CallLinkInfo*
op(llint_default_call_trampoline, macro ()
    linkFor(_llint_default_call)
end)

# 64bit:t0 32bit(t0,t1) is callee
# t2 is CallLinkInfo*
op(llint_virtual_call_trampoline, macro ()
    virtualThunkFor(ExecutableBase::m_jitCodeForCallWithArityCheck, FunctionExecutable::m_codeBlockForCall, _llint_internal_function_call_trampoline, .slowCase)
.slowCase:
    linkFor(_llint_virtual_call)
end)

# 64bit:t0 32bit(t0,t1) is callee
# t2 is CallLinkInfo*
op(llint_virtual_construct_trampoline, macro ()
    virtualThunkFor(ExecutableBase::m_jitCodeForConstructWithArityCheck, FunctionExecutable::m_codeBlockForConstruct, _llint_internal_function_construct_trampoline, .slowCase)
.slowCase:
    linkFor(_llint_virtual_call)
end)

# 64bit:t0 32bit(t0,t1) is callee
# t2 is CallLinkInfo*
op(llint_virtual_tail_call_trampoline, macro ()
    virtualThunkFor(ExecutableBase::m_jitCodeForCallWithArityCheck, FunctionExecutable::m_codeBlockForCall, _llint_internal_function_call_trampoline, .slowCase)
.slowCase:
    linkFor(_llint_virtual_call)
end)

if JIT
    macro loadBaselineJITConstantPool()
        # Baseline uses LLInt's PB register for its JIT constant pool.
        loadp CodeBlock[cfr], PB
        loadp CodeBlock::m_jitData[PB], PB
    end

    macro setupReturnToBaselineAfterCheckpointExitIfNeeded()
        # DFG or FTL OSR exit could have compiled an OSR exit to LLInt code.
        # That means it set up registers as if execution would happen in the
        # LLInt. However, during OSR exit for checkpoints, we might return to
        # JIT code if it's already compiled. After the OSR exit gets compiled,
        # we can tier up to JIT code. And checkpoint exit will jump to it.
        # That means we always need to set up our constant pool GPR, because the OSR
        # exit code might not have done it.
        bpneq r0, 1, .notBaselineJIT
        loadBaselineJITConstantPool()
    .notBaselineJIT:

    end
else
    macro loadBaselineJITConstantPool()
    end

    macro setupReturnToBaselineAfterCheckpointExitIfNeeded()
    end
end

op(checkpoint_osr_exit_from_inlined_call_trampoline, macro ()
    if (JSVALUE64 and not (C_LOOP or C_LOOP_WIN)) or ARMv7
        restoreStackPointerAfterCall()

        # Make sure we move r0 to a1 first since r0 might be the same as a0, for instance, on arm.
        if ARMv7
            # Given _llint_slow_path_checkpoint_osr_exit_from_inlined_call has
            # parameters as CallFrame* and EncodedJSValue,
            # we need to store call result on a2, a3 and call frame on a0,
            # leaving a1 as dummy value (this calling convention is considered only
            # for little-endian architectures).
            move r1, a3
            move r0, a2
            move cfr, a0
            # We don't call saveStateForCCall() because we are going to use the bytecodeIndex from our side state.
            cCall4(_llint_slow_path_checkpoint_osr_exit_from_inlined_call)
        else
            move r0, a1
            move cfr, a0
            # We don't call saveStateForCCall() because we are going to use the bytecodeIndex from our side state.
            cCall2(_llint_slow_path_checkpoint_osr_exit_from_inlined_call)
        end

        setupReturnToBaselineAfterCheckpointExitIfNeeded()
        restoreStateAfterCCall()
        branchIfException(_llint_throw_from_slow_path_trampoline)

        if ARM64E
            move r1, a0
            leap _g_config, a2
            jmp JSCConfigGateMapOffset + (constexpr Gate::loopOSREntry) * PtrSize[a2], NativeToJITGatePtrTag # JSEntryPtrTag
        else
            jmp r1, JSEntryPtrTag
        end
    else
        notSupported()
    end
end)

op(checkpoint_osr_exit_trampoline, macro ()
    # FIXME: We can probably dispatch to the checkpoint handler directly but this was easier 
    # and probably doesn't matter for performance.
    if (JSVALUE64 and not (C_LOOP or C_LOOP_WIN)) or ARMv7
        restoreStackPointerAfterCall()

        move cfr, a0
        # We don't call saveStateForCCall() because we are going to use the bytecodeIndex from our side state.
        cCall2(_llint_slow_path_checkpoint_osr_exit)
        setupReturnToBaselineAfterCheckpointExitIfNeeded()
        restoreStateAfterCCall()
        branchIfException(_llint_throw_from_slow_path_trampoline)
        if ARM64E
            move r1, a0
            leap _g_config, a2
            jmp JSCConfigGateMapOffset + (constexpr Gate::loopOSREntry) * PtrSize[a2], NativeToJITGatePtrTag # JSEntryPtrTag
        else
            jmp r1, JSEntryPtrTag
        end
    else
        notSupported()
    end
end)

op(normal_osr_exit_trampoline, macro ()
    dispatch(0)
end)


# Lastly, make sure that we can link even though we don't support all opcodes.
# These opcodes should never arise when using LLInt or either JIT. We assert
# as much.

macro notSupported()
    if ASSERT_ENABLED
        crash()
    else
        # We should use whatever the smallest possible instruction is, just to
        # ensure that there is a gap between instruction labels. If multiple
        # smallest instructions exist, we should pick the one that is most
        # likely result in execution being halted. Currently that is the break
        # instruction on all architectures we're interested in. (Break is int3
        # on Intel, which is 1 byte, and udf on ARMv7, which is 2 bytes.)
        break
    end
end


macro updateUnaryArithProfile(size, opcodeStruct, type, scratch1, scratch2)
    getu(size, opcodeStruct, m_profileIndex, scratch1)
    loadp CodeBlock[cfr], scratch2
    loadp CodeBlock::m_unlinkedCode[scratch2], scratch2
    loadp UnlinkedCodeBlock::m_unaryArithProfiles + UnaryArithProfileFixedVector::m_storage[scratch2], scratch2
    orh type, (constexpr (UnaryArithProfileFixedVector::Storage::offsetOfData())) + UnaryArithProfile::m_bits[scratch2, scratch1, 2]
end

macro updateBinaryArithProfile(size, opcodeStruct, type, scratch1, scratch2)
    getu(size, opcodeStruct, m_profileIndex, scratch1)
    loadp CodeBlock[cfr], scratch2
    loadp CodeBlock::m_unlinkedCode[scratch2], scratch2
    loadp UnlinkedCodeBlock::m_binaryArithProfiles + BinaryArithProfileFixedVector::m_storage[scratch2], scratch2
    orh type, (constexpr (BinaryArithProfileFixedVector::Storage::offsetOfData())) + BinaryArithProfile::m_bits[scratch2, scratch1, 2]
end

if WEBASSEMBLY

entry(wasm, macro()
    include InitWasm
end)

macro wasmScope()
    # Wrap the script in a macro since it overwrites some of the LLInt macros,
    # but we don't want to interfere with the LLInt opcodes
    include WebAssembly
    include InPlaceInterpreter
end

global _wasmLLIntPCRangeStart
_wasmLLIntPCRangeStart:
    break # FIXME: rdar://96556827
wasmScope()
global _wasmLLIntPCRangeEnd
_wasmLLIntPCRangeEnd:
    break # FIXME: rdar://96556827

else

# These need to be defined even when WebAssembly is disabled
op(wasm_function_prologue, macro ()
    crash()
end)

op(wasm_function_prologue_simd, macro ()
    crash()
end)

_wasm_trampoline_wasm_call:
_wasm_trampoline_wasm_call_indirect:
_wasm_trampoline_wasm_call_ref:
_wasm_trampoline_wasm_call_wide16:
_wasm_trampoline_wasm_call_indirect_wide16:
_wasm_trampoline_wasm_call_ref_wide16:
_wasm_trampoline_wasm_call_wide32:
_wasm_trampoline_wasm_call_indirect_wide32:
_wasm_trampoline_wasm_call_ref_wide32:
_wasm_trampoline_wasm_tail_call:
_wasm_trampoline_wasm_tail_call_indirect:
_wasm_trampoline_wasm_tail_call_wide16:
_wasm_trampoline_wasm_tail_call_indirect_wide16:
_wasm_trampoline_wasm_tail_call_wide32:
_wasm_trampoline_wasm_tail_call_indirect_wide32:
    crash()

end # WEBASSEMBLY

include? LowLevelInterpreterAdditions

global _llintPCRangeEnd
_llintPCRangeEnd:
    break # FIXME: rdar://96556827
# Do not put any code after this.
