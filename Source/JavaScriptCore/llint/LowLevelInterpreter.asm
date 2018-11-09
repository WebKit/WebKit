# Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
# - "b" = byte, "h" = 16-bit word, "i" = 32-bit word, "p" = pointer.
#   For 32-bit, "i" and "p" are interchangeable except when an op supports one
#   but not the other.
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
#  - lr is defined on non-X86 architectures (ARM64, ARM64E, ARMv7, ARM,
#  ARMv7_TRADITIONAL, MIPS and CLOOP) and holds the return PC
#
#  - pc holds the (native) program counter on 32-bits ARM architectures (ARM,
#  ARMv7, ARMv7_TRADITIONAL)
#
#  - t0, t1, t2, t3, t4 and optionally t5 are temporary registers that can get trashed on
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
#  - t4 and t5 are never argument registers, t3 can only be a3, t1 can only be
#  a1; but t0 and t2 can be either a0 or a2.
#
#  - On 64 bits, there are callee-save registers named csr0, csr1, ... csrN.
#  The last three csr registers are used used to store the PC base and
#  two special tag values. Don't use them for anything else.
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

# First come the common protocols that both interpreters use. Note that each
# of these must have an ASSERT() in LLIntData.cpp

# Work-around for the fact that the toolchain's awareness of armv7k / armv7s
# results in a separate slab in the fat binary, yet the offlineasm doesn't know
# to expect it.
if ARMv7k
end
if ARMv7s
end

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

const StackAlignment = constexpr (stackAlignmentBytes())
const StackAlignmentSlots = constexpr (stackAlignmentRegisters())
const StackAlignmentMask = StackAlignment - 1

const CallerFrameAndPCSize = constexpr (sizeof(CallerFrameAndPC))

const CallerFrame = 0
const ReturnPC = CallerFrame + MachineRegisterSize
const CodeBlock = ReturnPC + MachineRegisterSize
const Callee = CodeBlock + SlotSize
const ArgumentCount = Callee + SlotSize
const ThisArgumentOffset = ArgumentCount + SlotSize
const FirstArgumentOffset = ThisArgumentOffset + SlotSize
const CallFrameHeaderSize = ThisArgumentOffset

# Some value representation constants.
if JSVALUE64
    const TagBitTypeOther = constexpr TagBitTypeOther
    const TagBitBool      = constexpr TagBitBool
    const TagBitUndefined = constexpr TagBitUndefined
    const ValueEmpty      = constexpr ValueEmpty
    const ValueFalse      = constexpr ValueFalse
    const ValueTrue       = constexpr ValueTrue
    const ValueUndefined  = constexpr ValueUndefined
    const ValueNull       = constexpr ValueNull
    const TagTypeNumber   = constexpr TagTypeNumber
    const TagMask         = constexpr TagMask
else
    const Int32Tag = constexpr JSValue::Int32Tag
    const BooleanTag = constexpr JSValue::BooleanTag
    const NullTag = constexpr JSValue::NullTag
    const UndefinedTag = constexpr JSValue::UndefinedTag
    const CellTag = constexpr JSValue::CellTag
    const EmptyValueTag = constexpr JSValue::EmptyValueTag
    const DeletedValueTag = constexpr JSValue::DeletedValueTag
    const LowestTag = constexpr JSValue::LowestTag
end

# PutByIdFlags data
const PutByIdPrimaryTypeMask = constexpr PutByIdPrimaryTypeMask
const PutByIdPrimaryTypeSecondary = constexpr PutByIdPrimaryTypeSecondary
const PutByIdPrimaryTypeObjectWithStructure = constexpr PutByIdPrimaryTypeObjectWithStructure
const PutByIdPrimaryTypeObjectWithStructureOrOther = constexpr PutByIdPrimaryTypeObjectWithStructureOrOther
const PutByIdSecondaryTypeMask = constexpr PutByIdSecondaryTypeMask
const PutByIdSecondaryTypeBottom = constexpr PutByIdSecondaryTypeBottom
const PutByIdSecondaryTypeBoolean = constexpr PutByIdSecondaryTypeBoolean
const PutByIdSecondaryTypeOther = constexpr PutByIdSecondaryTypeOther
const PutByIdSecondaryTypeInt32 = constexpr PutByIdSecondaryTypeInt32
const PutByIdSecondaryTypeNumber = constexpr PutByIdSecondaryTypeNumber
const PutByIdSecondaryTypeString = constexpr PutByIdSecondaryTypeString
const PutByIdSecondaryTypeSymbol = constexpr PutByIdSecondaryTypeSymbol
const PutByIdSecondaryTypeObject = constexpr PutByIdSecondaryTypeObject
const PutByIdSecondaryTypeObjectOrOther = constexpr PutByIdSecondaryTypeObjectOrOther
const PutByIdSecondaryTypeTop = constexpr PutByIdSecondaryTypeTop

const CallOpCodeSize = constexpr op_call_length

const maxFrameExtentForSlowPathCall = constexpr maxFrameExtentForSlowPathCall

if X86_64 or X86_64_WIN or ARM64 or ARM64E
    const CalleeSaveSpaceAsVirtualRegisters = 4
elsif C_LOOP
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

# ArithProfile data
const ArithProfileInt = constexpr (ArithProfile::observedUnaryInt().bits())
const ArithProfileNumber = constexpr (ArithProfile::observedUnaryNumber().bits())
const ArithProfileIntInt = constexpr (ArithProfile::observedBinaryIntInt().bits())
const ArithProfileNumberInt = constexpr (ArithProfile::observedBinaryNumberInt().bits())
const ArithProfileIntNumber = constexpr (ArithProfile::observedBinaryIntNumber().bits())
const ArithProfileNumberNumber = constexpr (ArithProfile::observedBinaryNumberNumber().bits())

# Pointer Tags
const BytecodePtrTag = constexpr BytecodePtrTag
const JSEntryPtrTag = constexpr JSEntryPtrTag
const ExceptionHandlerPtrTag = constexpr ExceptionHandlerPtrTag
const NoPtrTag = constexpr NoPtrTag
const SlowPathPtrTag = constexpr SlowPathPtrTag

# Some register conventions.
if JSVALUE64
    # - Use a pair of registers to represent the PC: one register for the
    #   base of the bytecodes, and one register for the index.
    # - The PC base (or PB for short) must be stored in a callee-save register.
    # - C calls are still given the Instruction* rather than the PC index.
    #   This requires an add before the call, and a sub after.
    const PC = t4 # When changing this, make sure LLIntPC is up to date in LLIntPCRanges.h
    if ARM64 or ARM64E
        const metadataTable = csr6
        const PB = csr7
        const tagTypeNumber = csr8
        const tagMask = csr9
    elsif X86_64
        const metadataTable = csr1
        const PB = csr2
        const tagTypeNumber = csr3
        const tagMask = csr4
    elsif X86_64_WIN
        const metadataTable = csr3
        const PB = csr4
        const tagTypeNumber = csr5
        const tagMask = csr6
    elsif C_LOOP
        const PB = csr0
        const tagTypeNumber = csr1
        const tagMask = csr2
        const metadataTable = csr3
    end

else
    const PC = t4 # When changing this, make sure LLIntPC is up to date in LLIntPCRanges.h
    if C_LOOP
        const metadataTable = csr3
    elsif ARMv7
        const metadataTable = csr0
    else
        error
    end
end

macro dispatch(advance)
    addp advance, PC
    nextInstruction()
end

macro dispatchIndirect(offset)
    dispatch(offset)
end

macro dispatchOp(size, op)
    macro dispatchNarrow()
        dispatch(constexpr %op%_length)
    end

    macro dispatchWide()
        dispatch(constexpr %op%_length * 4 + 1)
    end

    size(dispatchNarrow, dispatchWide, macro (dispatch) dispatch() end)
end

macro getu(size, op, field, dst)
    size(getuOperandNarrow, getuOperandWide, macro (getu)
        getu(op, field, dst)
    end)
end

macro get(size, op, field, dst)
    size(getOperandNarrow, getOperandWide, macro (get)
        get(op, field, dst)
    end)
end

macro narrow(narrowFn, wideFn, k)
    k(narrowFn)
end

macro wide(narrowFn, wideFn, k)
    k(wideFn)
end

macro metadata(size, opcode, dst, scratch)
    loadp constexpr %opcode%::opcodeID * 4[metadataTable], dst # offset = metadataTable<unsigned*>[opcodeID]
    getu(size, opcode, metadataID, scratch) # scratch = bytecode.metadataID
    muli sizeof %opcode%::Metadata, scratch # scratch *= sizeof(Op::Metadata)
    addi scratch, dst # offset += scratch
    addp metadataTable, dst # return &metadataTable[offset]
end

macro jumpImpl(target)
    btiz target, .outOfLineJumpTarget
    dispatchIndirect(target)
.outOfLineJumpTarget:
    callSlowPath(_llint_slow_path_out_of_line_jump_target)
    nextInstruction()
end

macro commonOp(label, prologue, fn)
_%label%:
    prologue()
    fn(narrow)

_%label%_wide:
    prologue()
    fn(wide)
end

macro op(l, fn)
    commonOp(l, macro () end, macro (unused)
        fn()
    end)
end

macro llintOp(name, op, fn)
    commonOp(llint_%name%, traceExecution, macro(size)
        macro getImpl(field, dst)
            get(size, op, field, dst)
        end

        macro dispatchImpl()
            dispatchOp(size, name)
        end

        fn(size, getImpl, dispatchImpl)
    end)
end

macro llintOpWithReturn(name, op, fn)
    llintOp(name, op, macro(size, get, dispatch)
        makeReturn(get, dispatch, macro (return)
            fn(size, get, dispatch, return)
        end)
    end)
end

macro llintOpWithMetadata(name, op, fn)
    llintOpWithReturn(name, op, macro (size, get, dispatch, return)
        macro meta(dst, scratch)
            metadata(size, op, dst, scratch)
        end
        fn(size, get, dispatch, meta, return)
    end)
end

macro llintOpWithJump(name, op, impl)
    llintOpWithMetadata(name, op, macro(size, get, dispatch, metadata, return)
        macro jump(field)
            get(field, t0)
            jumpImpl(t0)
        end

        impl(size, get, jump, dispatch)
    end)
end

macro llintOpWithProfile(name, op, fn)
    llintOpWithMetadata(name, op, macro(size, get, dispatch, metadata, return)
        makeReturnProfiled(op, get, metadata, dispatch, macro (returnProfiled)
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
const NoIndexingShape          = constexpr NoIndexingShape
const Int32Shape               = constexpr Int32Shape
const DoubleShape              = constexpr DoubleShape
const ContiguousShape          = constexpr ContiguousShape
const ArrayStorageShape        = constexpr ArrayStorageShape
const SlowPutArrayStorageShape = constexpr SlowPutArrayStorageShape
const CopyOnWrite              = constexpr CopyOnWrite

# Type constants.
const StringType = constexpr StringType
const SymbolType = constexpr SymbolType
const ObjectType = constexpr ObjectType
const FinalObjectType = constexpr FinalObjectType
const JSFunctionType = constexpr JSFunctionType
const ArrayType = constexpr ArrayType
const DerivedArrayType = constexpr DerivedArrayType
const ProxyObjectType = constexpr ProxyObjectType

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

# Type flags constants.
const MasqueradesAsUndefined = constexpr MasqueradesAsUndefined
const ImplementsDefaultHasInstance = constexpr ImplementsDefaultHasInstance

# Bytecode operand constants.
const FirstConstantRegisterIndexNarrow = 16
const FirstConstantRegisterIndexWide = constexpr FirstConstantRegisterIndex

# Code type constants.
const GlobalCode = constexpr GlobalCode
const EvalCode = constexpr EvalCode
const FunctionCode = constexpr FunctionCode
const ModuleCode = constexpr ModuleCode

# The interpreter steals the tag word of the argument count.
const LLIntReturnPC = ArgumentCount + TagOffset

# String flags.
const HashFlags8BitBuffer = constexpr StringImpl::s_hashFlag8BitBuffer

# Copied from PropertyOffset.h
const firstOutOfLineOffset = constexpr firstOutOfLineOffset

# ResolveType
const GlobalProperty = constexpr GlobalProperty
const GlobalVar = constexpr GlobalVar
const GlobalLexicalVar = constexpr GlobalLexicalVar
const ClosureVar = constexpr ClosureVar
const LocalClosureVar = constexpr LocalClosureVar
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
const MarkedBlockFooterOffset = constexpr MarkedBlock::offsetOfFooter

const BlackThreshold = constexpr blackThreshold

const VectorBufferOffset = Vector::m_buffer
const VectorSizeOffset = Vector::m_size

# Some common utilities.
macro crash()
    if C_LOOP
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
#             move cfr, a0 # pass the ExecState* as arg0.
#             move t0, a1 # pass the value of register t0 as arg1.
#             call _cProbeCallbackFunction # to do whatever you want.
#         end
#     )
#
if X86_64 or ARM64 or ARM64E
    macro probe(action)
        # save all the registers that the LLInt may use.
        if ARM64 or ARM64E
            push cfr, lr
        end
        push a0, a1
        push a2, a3
        push t0, t1
        push t2, t3
        push t4, t5
        if ARM64 or ARM64E
            push csr0, csr1
            push csr2, csr3
            push csr4, csr5
            push csr6, csr7
            push csr8, csr9
        end

        action()

        # restore all the registers we saved previously.
        if ARM64 or ARM64E
            pop csr9, csr8
            pop csr7, csr6
            pop csr5, csr4
            pop csr3, csr2
            pop csr1, csr0
        end
        pop t5, t4
        pop t3, t2
        pop t1, t0
        pop a3, a2
        pop a1, a0
        if ARM64 or ARM64E
            pop lr, cfr
        end
    end
else
    macro probe(action)
    end
end

macro checkStackPointerAlignment(tempReg, location)
    if ASSERT_ENABLED
        if ARM64 or ARM64E or C_LOOP
            # ARM64 and ARM64E will check for us!
            # C_LOOP does not need the alignment, and can use a little perf
            # improvement from avoiding useless work.
        else
            if ARM or ARMv7 or ARMv7_TRADITIONAL
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

if C_LOOP or ARM64 or ARM64E or X86_64 or X86_64_WIN
    const CalleeSaveRegisterCount = 0
elsif ARM or ARMv7_TRADITIONAL or ARMv7
    const CalleeSaveRegisterCount = 7
elsif MIPS
    const CalleeSaveRegisterCount = 1
elsif X86 or X86_WIN
    const CalleeSaveRegisterCount = 3
end

const CalleeRegisterSaveSize = CalleeSaveRegisterCount * MachineRegisterSize

# VMEntryTotalFrameSize includes the space for struct VMEntryRecord and the
# callee save registers rounded up to keep the stack aligned
const VMEntryTotalFrameSize = (CalleeRegisterSaveSize + sizeof VMEntryRecord + StackAlignment - 1) & ~StackAlignmentMask

macro pushCalleeSaves()
    if C_LOOP or ARM64 or ARM64E or X86_64 or X86_64_WIN
    elsif ARM or ARMv7_TRADITIONAL
        emit "push {r4-r10}"
    elsif ARMv7
        emit "push {r4-r6, r8-r11}"
    elsif MIPS
        emit "addiu $sp, $sp, -4"
        emit "sw $s4, 0($sp)"
        # save $gp to $s4 so that we can restore it after a function call
        emit "move $s4, $gp"
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
    if C_LOOP or ARM64 or ARM64E or X86_64 or X86_64_WIN
    elsif ARM or ARMv7_TRADITIONAL
        emit "pop {r4-r10}"
    elsif ARMv7
        emit "pop {r4-r6, r8-r11}"
    elsif MIPS
        emit "lw $s4, 0($sp)"
        emit "addiu $sp, $sp, 4"
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
    if C_LOOP or ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
        push lr
        push cfr
    elsif X86 or X86_WIN or X86_64 or X86_64_WIN
        push cfr
    elsif ARM64 or ARM64E
        push cfr, lr
    else
        error
    end
    move sp, cfr
end

macro restoreCallerPCAndCFR()
    move cfr, sp
    if C_LOOP or ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
        pop cfr
        pop lr
    elsif X86 or X86_WIN or X86_64 or X86_64_WIN
        pop cfr
    elsif ARM64 or ARM64E
        pop lr, cfr
    end
end

macro preserveCalleeSavesUsedByLLInt()
    subp CalleeSaveSpaceStackAligned, sp
    if C_LOOP
        storep metadataTable, -PtrSize[cfr]
    elsif ARM or ARMv7_TRADITIONAL
    elsif ARMv7
        storep metadataTable, -4[cfr]
    elsif ARM64 or ARM64E
        emit "stp x27, x28, [x29, #-16]"
        emit "stp x25, x26, [x29, #-32]"
    elsif MIPS
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
    end
end

macro restoreCalleeSavesUsedByLLInt()
    if C_LOOP
        loadp -PtrSize[cfr], metadataTable
    elsif ARM or ARMv7_TRADITIONAL
    elsif ARMv7
        loadp -4[cfr], metadataTable
    elsif ARM64 or ARM64E
        emit "ldp x25, x26, [x29, #-32]"
        emit "ldp x27, x28, [x29, #-16]"
    elsif MIPS
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
    end
end

macro copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(vm, temp)
    if ARM64 or ARM64E or X86_64 or X86_64_WIN
        loadp VM::topEntryFrame[vm], temp
        vmEntryRecord(temp, temp)
        leap VMEntryRecord::calleeSaveRegistersBuffer[temp], temp
        if ARM64 or ARM64E
            storeq csr0, [temp]
            storeq csr1, 8[temp]
            storeq csr2, 16[temp]
            storeq csr3, 24[temp]
            storeq csr4, 32[temp]
            storeq csr5, 40[temp]
            storeq csr6, 48[temp]
            storeq csr7, 56[temp]
            storeq csr8, 64[temp]
            storeq csr9, 72[temp]
            stored csfr0, 80[temp]
            stored csfr1, 88[temp]
            stored csfr2, 96[temp]
            stored csfr3, 104[temp]
            stored csfr4, 112[temp]
            stored csfr5, 120[temp]
            stored csfr6, 128[temp]
            stored csfr7, 136[temp]
        elsif X86_64
            storeq csr0, [temp]
            storeq csr1, 8[temp]
            storeq csr2, 16[temp]
            storeq csr3, 24[temp]
            storeq csr4, 32[temp]
        elsif X86_64_WIN
            storeq csr0, [temp]
            storeq csr1, 8[temp]
            storeq csr2, 16[temp]
            storeq csr3, 24[temp]
            storeq csr4, 32[temp]
            storeq csr5, 40[temp]
            storeq csr6, 48[temp]
        end
    end
end

macro restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(vm, temp)
    if ARM64 or ARM64E or X86_64 or X86_64_WIN
        loadp VM::topEntryFrame[vm], temp
        vmEntryRecord(temp, temp)
        leap VMEntryRecord::calleeSaveRegistersBuffer[temp], temp
        if ARM64 or ARM64E
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
            loadd 80[temp], csfr0
            loadd 88[temp], csfr1
            loadd 96[temp], csfr2
            loadd 104[temp], csfr3
            loadd 112[temp], csfr4
            loadd 120[temp], csfr5
            loadd 128[temp], csfr6
            loadd 136[temp], csfr7
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
        end
    end
end

macro preserveReturnAddressAfterCall(destinationRegister)
    if C_LOOP or ARM or ARMv7 or ARMv7_TRADITIONAL or ARM64 or ARM64E or MIPS
        # In C_LOOP case, we're only preserving the bytecode vPC.
        move lr, destinationRegister
    elsif X86 or X86_WIN or X86_64 or X86_64_WIN
        pop destinationRegister
    else
        error
    end
end

macro unpoison(poison, field, scratch)
    if POISON
        loadp poison, scratch
        xorp scratch, field
    end
end

macro functionPrologue()
    tagReturnAddress sp
    if X86 or X86_WIN or X86_64 or X86_64_WIN
        push cfr
    elsif ARM64 or ARM64E
        push cfr, lr
    elsif C_LOOP or ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
        push lr
        push cfr
    end
    move sp, cfr
end

macro functionEpilogue()
    if X86 or X86_WIN or X86_64 or X86_64_WIN
        pop cfr
    elsif ARM64 or ARM64E
        pop lr, cfr
    elsif C_LOOP or ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
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

macro callTargetFunction(size, op, dispatch, callee, callPtrTag)
    if C_LOOP
        cloopCallJSFunction callee
    else
        call callee, callPtrTag
    end
    restoreStackPointerAfterCall()
    dispatchAfterCall(size, op, dispatch)
end

macro prepareForRegularCall(callee, temp1, temp2, temp3, callPtrTag)
    addp CallerFrameAndPCSize, sp
end

# sp points to the new frame
macro prepareForTailCall(callee, temp1, temp2, temp3, callPtrTag)
    restoreCalleeSavesUsedByLLInt()

    loadi PayloadOffset + ArgumentCount[cfr], temp2
    loadp CodeBlock[cfr], temp1
    loadp CodeBlock::m_numParameters[temp1], temp1
    bilteq temp1, temp2, .noArityFixup
    move temp1, temp2

.noArityFixup:
    # We assume < 2^28 arguments
    muli SlotSize, temp2
    addi StackAlignment - 1 + CallFrameHeaderSize, temp2
    andi ~StackAlignmentMask, temp2

    move cfr, temp1
    addp temp2, temp1

    loadi PayloadOffset + ArgumentCount[sp], temp2
    # We assume < 2^28 arguments
    muli SlotSize, temp2
    addi StackAlignment - 1 + CallFrameHeaderSize, temp2
    andi ~StackAlignmentMask, temp2

    if ARM or ARMv7_TRADITIONAL or ARMv7 or ARM64 or ARM64E or C_LOOP or MIPS
        addp CallerFrameAndPCSize, sp
        subi CallerFrameAndPCSize, temp2
        loadp CallerFrameAndPC::returnPC[cfr], lr
    else
        addp PtrSize, sp
        subi PtrSize, temp2
        loadp PtrSize[cfr], temp3
        storep temp3, [sp]
    end

    if POINTER_PROFILING
        addp 16, cfr, temp3
        untagReturnAddress temp3
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
    jmp callee, callPtrTag
end

macro slowPathForCall(size, op, dispatch, slowPath, prepareCall)
    callCallSlowPath(
        slowPath,
        # Those are r0 and r1
        macro (callee, calleeFramePtr)
            btpz calleeFramePtr, .dontUpdateSP
            move calleeFramePtr, sp
            prepareCall(callee, t2, t3, t4, SlowPathPtrTag)
        .dontUpdateSP:
            callTargetFunction(size, op, dispatch, callee, SlowPathPtrTag)
        end)
end

macro arrayProfile(offset, cellAndIndexingType, metadata, scratch)
    const cell = cellAndIndexingType
    const indexingType = cellAndIndexingType 
    loadi JSCell::m_structureID[cell], scratch
    storei scratch, offset + ArrayProfile::m_lastSeenStructureID[metadata]
    loadb JSCell::m_indexingTypeAndMisc[cell], indexingType
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

macro checkSwitchToJIT(increment, action)
    loadp CodeBlock[cfr], t0
    baddis increment, CodeBlock::m_llintExecuteCounter + BaselineExecutionCounter::m_counter[t0], .continue
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
    size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide, macro (FirstConstantRegisterIndex)
        assert(macro (ok) bilt index, FirstConstantRegisterIndex, ok end)
    end)
end

macro functionForCallCodeBlockGetter(targetRegister, scratch)
    if JSVALUE64
        loadp Callee[cfr], targetRegister
    else
        loadp Callee + PayloadOffset[cfr], targetRegister
    end
    loadp JSFunction::m_executable[targetRegister], targetRegister
    unpoison(_g_JSFunctionPoison, targetRegister, scratch)
    loadp FunctionExecutable::m_codeBlockForCall[targetRegister], targetRegister
    loadp ExecutableToCodeBlockEdge::m_codeBlock[targetRegister], targetRegister
end

macro functionForConstructCodeBlockGetter(targetRegister, scratch)
    if JSVALUE64
        loadp Callee[cfr], targetRegister
    else
        loadp Callee + PayloadOffset[cfr], targetRegister
    end
    loadp JSFunction::m_executable[targetRegister], targetRegister
    unpoison(_g_JSFunctionPoison, targetRegister, scratch)
    loadp FunctionExecutable::m_codeBlockForConstruct[targetRegister], targetRegister
    loadp ExecutableToCodeBlockEdge::m_codeBlock[targetRegister], targetRegister
end

macro notFunctionCodeBlockGetter(targetRegister, ignored)
    loadp CodeBlock[cfr], targetRegister
end

macro functionCodeBlockSetter(sourceRegister)
    storep sourceRegister, CodeBlock[cfr]
end

macro notFunctionCodeBlockSetter(sourceRegister)
    # Nothing to do!
end

# Do the bare minimum required to execute code. Sets up the PC, leave the CodeBlock*
# in t1. May also trigger prologue entry OSR.
macro prologue(codeBlockGetter, codeBlockSetter, osrSlowPath, traceSlowPath)
    # Set up the call frame and check if we should OSR.
    tagReturnAddress sp
    preserveCallerPCAndCFR()

    if TRACING
        subp maxFrameExtentForSlowPathCall, sp
        callSlowPath(traceSlowPath)
        addp maxFrameExtentForSlowPathCall, sp
    end
    codeBlockGetter(t1, t2)
    if not C_LOOP
        baddis 5, CodeBlock::m_llintExecuteCounter + BaselineExecutionCounter::m_counter[t1], .continue
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
        if ARM64 or ARM64E
            pop lr, cfr
            untagReturnAddress sp
        elsif ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
            pop cfr
            pop lr
        else
            pop cfr
        end
        jmp r0, JSEntryPtrTag
    .recover:
        codeBlockGetter(t1, t2)
    .continue:
    end

    codeBlockSetter(t1)

    preserveCalleeSavesUsedByLLInt()

    # Set up the PC.
    if JSVALUE64
        # FIXME: cleanup double load
        # https://bugs.webkit.org/show_bug.cgi?id=190932
        loadp CodeBlock::m_instructions[t1], PB
        loadp [PB], PB
        unpoison(_g_CodeBlockPoison, PB, t3)
        move 0, PC
    else
        # FIXME: cleanup double load
        # https://bugs.webkit.org/show_bug.cgi?id=190932
        loadp CodeBlock::m_instructions[t1], PC
        loadp [PC], PC
    end

    # Get new sp in t0 and check stack height.
    getFrameRegisterSizeForCodeBlock(t1, t0)
    subp cfr, t0, t0
    bpa t0, cfr, .needStackCheck
    loadp CodeBlock::m_poisonedVM[t1], t2
    unpoison(_g_CodeBlockPoison, t2, t3)
    if C_LOOP
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
    codeBlockGetter(t1, t2)
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
        move TagTypeNumber, tagTypeNumber
        addq TagBitTypeOther, tagTypeNumber, tagMask
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
    addp -profileArgSkip, t0 # Use addi because that's what has the peephole
    assert(macro (ok) bpgteq t0, 0, ok end)
    btpz t0, .argumentProfileDone
    loadp CodeBlock::m_argumentValueProfiles + RefCountedArray::m_data[t1], t3
    btpz t3, .argumentProfileDone # When we can't JIT, we don't allocate any argument value profiles.
    mulp sizeof ValueProfile, t0, t2 # Aaaaahhhh! Need strength reduction!
    lshiftp 3, t0
    addp t2, t3
.argumentProfileLoop:
    if JSVALUE64
        loadq ThisArgumentOffset - 8 + profileArgSkip * 8[cfr, t0], t2
        subp sizeof ValueProfile, t3
        storeq t2, profileArgSkip * sizeof ValueProfile + ValueProfile::m_buckets[t3]
    else
        loadi ThisArgumentOffset + TagOffset - 8 + profileArgSkip * 8[cfr, t0], t2
        subp sizeof ValueProfile, t3
        storei t2, profileArgSkip * sizeof ValueProfile + ValueProfile::m_buckets + TagOffset[t3]
        loadi ThisArgumentOffset + PayloadOffset - 8 + profileArgSkip * 8[cfr, t0], t2
        storei t2, profileArgSkip * sizeof ValueProfile + ValueProfile::m_buckets + PayloadOffset[t3]
    end
    baddpnz -8, t0, .argumentProfileLoop
.argumentProfileDone:
end

macro doReturn()
    restoreCalleeSavesUsedByLLInt()
    restoreCallerPCAndCFR()
    ret
end

# This break instruction is needed so that the synthesized llintPCRangeStart label
# doesn't point to the exact same location as vmEntryToJavaScript which comes after it.
# Otherwise, libunwind will report vmEntryToJavaScript as llintPCRangeStart in
# stack traces.

    break

# stub to call into JavaScript or Native functions
# EncodedJSValue vmEntryToJavaScript(void* code, VM* vm, ProtoCallFrame* protoFrame)
# EncodedJSValue vmEntryToNativeFunction(void* code, VM* vm, ProtoCallFrame* protoFrame)

if C_LOOP
    _llint_vm_entry_to_javascript:
else
    global _vmEntryToJavaScript
    _vmEntryToJavaScript:
end
    doVMEntry(makeJavaScriptCall)


if C_LOOP
    _llint_vm_entry_to_native:
else
    global _vmEntryToNative
    _vmEntryToNative:
end
    doVMEntry(makeHostFunctionCall)


if not C_LOOP
    # void sanitizeStackForVMImpl(VM* vm)
    global _sanitizeStackForVMImpl
    _sanitizeStackForVMImpl:
        tagReturnAddress sp
        # We need three non-aliased caller-save registers. We are guaranteed
        # this for a0, a1 and a2 on all architectures.
        if X86 or X86_WIN
            loadp 4[sp], a0
        end
        const vm = a0
        const address = a1
        const zeroValue = a2
    
        loadp VM::m_lastStackTop[vm], address
        bpbeq sp, address, .zeroFillDone
    
        move 0, zeroValue
    .zeroFillLoop:
        storep zeroValue, [address]
        addp PtrSize, address
        bpa sp, address, .zeroFillLoop

    .zeroFillDone:
        move sp, address
        storep address, VM::m_lastStackTop[vm]
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

if C_LOOP
    # Dummy entry point the C Loop uses to initialize.
    _llint_entry:
        crash()
else
    macro initPCRelative(pcBase)
        if X86_64 or X86_64_WIN or X86 or X86_WIN
            call _relativePCBase
        _relativePCBase:
            pop pcBase
        elsif ARM64 or ARM64E
        elsif ARMv7
        _relativePCBase:
            move pc, pcBase
            subp 3, pcBase   # Need to back up the PC and set the Thumb2 bit
        elsif ARM or ARMv7_TRADITIONAL
        _relativePCBase:
            move pc, pcBase
            subp 8, pcBase
        elsif MIPS
            la _relativePCBase, pcBase
            setcallreg pcBase # needed to set $t9 to the right value for the .cpload created by the label.
        _relativePCBase:
        end
end

# The PC base is in t2, as this is what _llint_entry leaves behind through
# initPCRelative(t2)
macro setEntryAddress(index, label)
    setEntryAddressCommon(index, label, a0)
end

macro setEntryAddressWide(index, label)
     setEntryAddressCommon(index, label, a1)
end

macro setEntryAddressCommon(index, label, map)
    if X86_64 or X86_64_WIN
        leap (label - _relativePCBase)[t2], t3
        move index, t4
        storep t3, [map, t4, 8]
    elsif X86 or X86_WIN
        leap (label - _relativePCBase)[t2], t3
        move index, t4
        storep t3, [map, t4, 4]
    elsif ARM64 or ARM64E
        pcrtoaddr label, t2
        move index, t4
        storep t2, [map, t4, PtrSize]
    elsif ARM or ARMv7 or ARMv7_TRADITIONAL
        mvlbl (label - _relativePCBase), t4
        addp t4, t2, t4
        move index, t3
        storep t4, [map, t3, 4]
    elsif MIPS
        la label, t4
        la _relativePCBase, t3
        subp t3, t4
        addp t4, t2, t4
        move index, t3
        storep t4, [map, t3, 4]
    end
end

global _llint_entry
# Entry point for the llint to initialize.
_llint_entry:
    functionPrologue()
    pushCalleeSaves()
    if X86 or X86_WIN
        loadp 20[sp], a0
        loadp 24[sp], a1
    end

    initPCRelative(t2)

    # Include generated bytecode initialization file.
    include InitBytecodes

    popCalleeSaves()
    functionEpilogue()
    ret
end

_llint_op_wide:
    nextInstructionWide()

_llint_op_wide_wide:
    crash()

_llint_op_enter_wide:
    crash()

op(llint_program_prologue, macro ()
    prologue(notFunctionCodeBlockGetter, notFunctionCodeBlockSetter, _llint_entry_osr, _llint_trace_prologue)
    dispatch(0)
end)


op(llint_module_program_prologue, macro ()
    prologue(notFunctionCodeBlockGetter, notFunctionCodeBlockSetter, _llint_entry_osr, _llint_trace_prologue)
    dispatch(0)
end)


op(llint_eval_prologue, macro ()
    prologue(notFunctionCodeBlockGetter, notFunctionCodeBlockSetter, _llint_entry_osr, _llint_trace_prologue)
    dispatch(0)
end)


op(llint_function_for_call_prologue, macro ()
    prologue(functionForCallCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_call, _llint_trace_prologue_function_for_call)
    functionInitialization(0)
    dispatch(0)
end)
    

op(llint_function_for_construct_prologue, macro ()
    prologue(functionForConstructCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_construct, _llint_trace_prologue_function_for_construct)
    functionInitialization(1)
    dispatch(0)
end)
    

op(llint_function_for_call_arity_check, macro ()
    prologue(functionForCallCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_call_arityCheck, _llint_trace_arityCheck_for_call)
    functionArityCheck(.functionForCallBegin, _slow_path_call_arityCheck)
.functionForCallBegin:
    functionInitialization(0)
    dispatch(0)
end)


op(llint_function_for_construct_arity_check, macro ()
    prologue(functionForConstructCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_construct_arityCheck, _llint_trace_arityCheck_for_construct)
    functionArityCheck(.functionForConstructBegin, _slow_path_construct_arityCheck)
.functionForConstructBegin:
    functionInitialization(1)
    dispatch(0)
end)


# Value-representation-specific code.
if JSVALUE64
    include LowLevelInterpreter64
else
    include LowLevelInterpreter32_64
end


# Value-representation-agnostic code.
macro slowPathOp(op)
    llintOp(op_%op%, unused, macro (unused, unused, dispatch)
        callSlowPath(_slow_path_%op%)
        dispatch()
    end)
end

slowPathOp(create_cloned_arguments)
slowPathOp(create_direct_arguments)
slowPathOp(create_lexical_environment)
slowPathOp(create_rest)
slowPathOp(create_scoped_arguments)
slowPathOp(create_this)
slowPathOp(define_accessor_property)
slowPathOp(define_data_property)
slowPathOp(enumerator_generic_pname)
slowPathOp(enumerator_structure_pname)
slowPathOp(get_by_id_with_this)
slowPathOp(get_by_val_with_this)
slowPathOp(get_direct_pname)
slowPathOp(get_enumerable_length)
slowPathOp(get_property_enumerator)
slowPathOp(greater)
slowPathOp(greatereq)
slowPathOp(has_generic_property)
slowPathOp(has_indexed_property)
slowPathOp(has_structure_property)
slowPathOp(in_by_id)
slowPathOp(in_by_val)
slowPathOp(is_function)
slowPathOp(is_object_or_null)
slowPathOp(less)
slowPathOp(lesseq)
slowPathOp(mod)
slowPathOp(new_array_buffer)
slowPathOp(new_array_with_spread)
slowPathOp(pow)
slowPathOp(push_with_scope)
slowPathOp(put_by_id_with_this)
slowPathOp(put_by_val_with_this)
slowPathOp(resolve_scope_for_hoisting_func_decl_in_eval)
slowPathOp(spread)
slowPathOp(strcat)
slowPathOp(throw_static_error)
slowPathOp(to_index_string)
slowPathOp(typeof)
slowPathOp(unreachable)

macro llintSlowPathOp(op)
    llintOp(op_%op%, unused, macro (unused, unused, dispatch)
        callSlowPath(_llint_slow_path_%op%)
        dispatch()
    end)
end

llintSlowPathOp(del_by_id)
llintSlowPathOp(del_by_val)
llintSlowPathOp(instanceof)
llintSlowPathOp(instanceof_custom)
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
llintSlowPathOp(super_sampler_begin)
llintSlowPathOp(super_sampler_end)
llintSlowPathOp(throw)
llintSlowPathOp(try_get_by_id)

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
    jump(target)
end)


llintJumpTrueOrFalseOp(
    jtrue, OpJtrue,
    macro (value, target) btinz value, 1, target end)


llintJumpTrueOrFalseOp(
    jfalse, OpJfalse,
    macro (value, target) btiz value, 1, target end)


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


llintOp(op_check_traps, OpCheckTraps, macro (unused, unused, dispatch)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_poisonedVM[t1], t1
    unpoison(_g_CodeBlockPoison, t1, t2)
    loadb VM::m_traps+VMTraps::m_needTrapHandling[t1], t0
    btpnz t0, .handleTraps
.afterHandlingTraps:
    dispatch()
.handleTraps:
    callTrapHandler(.throwHandler)
    jmp .afterHandlingTraps
.throwHandler:
    jmp _llint_throw_from_slow_path_trampoline
end)


# Returns the packet pointer in t0.
macro acquireShadowChickenPacket(slow)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_poisonedVM[t1], t1
    unpoison(_g_CodeBlockPoison, t1, t2)
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
commonCallOp(op_call, _llint_slow_path_call, OpCall, prepareForRegularCall, macro (getu, metadata)
    arrayProfileForCall(OpCall, getu)
end)


macro callOp(name, op, prepareCall, fn)
    commonCallOp(op_%name%, _llint_slow_path_%name%, op, prepareCall, fn)
end


callOp(tail_call, OpTailCall, prepareForTailCall, macro (getu, metadata)
    arrayProfileForCall(OpTailCall, getu)
    checkSwitchToJITForEpilogue()
    # reload metadata since checkSwitchToJITForEpilogue() might have trashed t5
    metadata(t5, t0)
end)


callOp(construct, OpConstruct, prepareForRegularCall, macro (getu, metadata) end)


macro doCallVarargs(size, op, dispatch, frameSlowPath, slowPath, prepareCall)
    callSlowPath(frameSlowPath)
    branchIfException(_llint_throw_from_slow_path_trampoline)
    # calleeFrame in r1
    if JSVALUE64
        move r1, sp
    else
        # The calleeFrame is not stack aligned, move down by CallerFrameAndPCSize to align
        if ARMv7
            subp r1, CallerFrameAndPCSize, t2
            move t2, sp
        else
            subp r1, CallerFrameAndPCSize, sp
        end
    end
    slowPathForCall(size, op, dispatch, slowPath, prepareCall)
end


llintOp(op_call_varargs, OpCallVarargs, macro (size, get, dispatch)
    doCallVarargs(size, OpCallVarargs, dispatch, _llint_slow_path_size_frame_for_varargs, _llint_slow_path_call_varargs, prepareForRegularCall)
end)

llintOp(op_tail_call_varargs, OpTailCallVarargs, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    # We lie and perform the tail call instead of preparing it since we can't
    # prepare the frame for a call opcode
    doCallVarargs(size, OpTailCallVarargs, dispatch, _llint_slow_path_size_frame_for_varargs, _llint_slow_path_tail_call_varargs, prepareForTailCall)
end)


llintOp(op_tail_call_forward_arguments, OpTailCallForwardArguments, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    # We lie and perform the tail call instead of preparing it since we can't
    # prepare the frame for a call opcode
    doCallVarargs(size, OpTailCallForwardArguments, dispatch, _llint_slow_path_size_frame_for_forward_arguments, _llint_slow_path_tail_call_forward_arguments, prepareForTailCall)
end)


llintOp(op_construct_varargs, OpConstructVarargs, macro (size, get, dispatch)
    doCallVarargs(size, OpConstructVarargs, dispatch, _llint_slow_path_size_frame_for_varargs, _llint_slow_path_construct_varargs, prepareForRegularCall)
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

_llint_op_call_eval:
    slowPathForCall(
        narrow,
        OpCallEval,
        macro () dispatchOp(narrow, op_call_eval) end,
        _llint_slow_path_call_eval,
        prepareForRegularCall)

_llint_op_call_eval_wide:
    slowPathForCall(
        wide,
        OpCallEval,
        macro () dispatchOp(wide, op_call_eval) end,
        _llint_slow_path_call_eval_wide,
        prepareForRegularCall)

_llint_generic_return_point:
    dispatchAfterCall(narrow, OpCallEval, macro ()
        dispatchOp(narrow, op_call_eval)
    end)

_llint_generic_return_point_wide:
    dispatchAfterCall(wide, OpCallEval, macro()
        dispatchOp(wide, op_call_eval)
    end)

llintOp(op_identity_with_profile, OpIdentityWithProfile, macro (unused, unused, dispatch)
    dispatch()
end)


llintOp(op_yield, OpYield, macro (unused, unused, unused)
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
        # on Intel, which is 1 byte, and bkpt on ARMv7, which is 2 bytes.)
        break
    end
end
