# Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#
# IPInt: the Wasm in-place interpreter
#
# docs by Daniel Liu <daniel_liu4@apple.com>; started as a 2023 intern project
#
# Contents:
# 0. Documentation comments
# 1. Interpreter definitions
#   1.1: Register definitions
#   1.2: Constant definitions
# 2. Core interpreter macros
# 3. Helper interpreter macros
# 4. Interpreter entrypoints
# 5. Instruction implementation
#

##############################
# 1. Interpreter definitions #
##############################

# -------------------------
# 1.1: Register definitions
# -------------------------

# IPInt uses a number of core registers which store the interpreter's state:
# - PC: (Program Counter) IPInt's program counter. This records the interpreter's position in Wasm bytecode.
# - MC: (Metadata Counter) IPInt's metadata pointer. This records the corresponding position in generated metadata.
# - WI: (Wasm Instance) pointer to the current JSWebAssemblyInstance object. This is used for accessing
#       function-specific data (callee-save).
# - PL: (Pointer to Locals) pointer to the address of local 0 in the current function. This is used for accessing
#       locals quickly.
# - MB: (Memory Base) pointer to the current Wasm memory base address (callee-save).
# - BC: (Bounds Check) the size of the current Wasm memory region, for bounds checking (callee-save).
#
# Finally, we provide four "sc" (safe for call) registers which are guaranteed to not overlap with argument
# registers (sc0, sc1, sc2, sc3)

const alignIPInt = constexpr JSC::IPInt::alignIPInt
const alignArgumInt = constexpr JSC::IPInt::alignArgumInt
const alignUInt = constexpr JSC::IPInt::alignUInt
const alignMInt = constexpr JSC::IPInt::alignMInt

if ARM64 or ARM64E
    const PC = csr7
    const MC = csr6
    const PL = t6

    # Wasm Pinned Registers
    const WI = csr0
    const MB = csr3
    const BC = csr4

    const sc0 = ws0
    const sc1 = ws1
    const sc2 = ws2
    const sc3 = ws3
elsif X86_64
    const PC = csr2
    const MC = csr1
    const PL = t5

    # Wasm Pinned Registers
    const WI = csr0
    const MB = csr3
    const BC = csr4

    const sc0 = ws0
    const sc1 = ws1
    const sc2 = csr3
    const sc3 = csr4
elsif RISCV64
    const PC = csr7
    const MC = csr6
    const PL = csr10

    # Wasm Pinned Registers
    const WI = csr0
    const MB = csr3
    const BC = csr4

    const sc0 = ws0
    const sc1 = ws1
    const sc2 = csr9
    const sc3 = csr10
elsif ARMv7
    const PC = csr1
    const MC = t6
    const PL = t7

    # Wasm Pinned Registers
    const WI = csr0
    const MB = invalidGPR
    const BC = invalidGPR

    const sc0 = t4
    const sc1 = t5
    const sc2 = csr0
    const sc3 = t7
else
    const PC = invalidGPR
    const MC = invalidGPR
    const PL = invalidGPR

    # Wasm Pinned Registers
    const WI = invalidGPR
    const MB = invalidGPR
    const BC = invalidGPR

    const sc0 = invalidGPR
    const sc1 = invalidGPR
    const sc2 = invalidGPR
    const sc3 = invalidGPR
end

# -------------------------
# 1.2: Constant definitions
# -------------------------

const PtrSize = constexpr (sizeof(void*))
const SlotSize = constexpr (sizeof(Register))

# amount of memory a local takes up on the stack (16 bytes for a v128)
const V128ISize = 16
const LocalSize = V128ISize
const StackValueSize = V128ISize

const WasmEntryPtrTag = constexpr WasmEntryPtrTag

# These must match the definition in GPRInfo.h
const wasmInstance = csr0
if X86_64 or ARM64 or ARM64E or RISCV64
    const memoryBase = csr3
    const boundsCheckingSize = csr4
elsif ARMv7
    const memoryBase = t2
    const boundsCheckingSize = t3
else
    const memoryBase = invalidGPR
    const boundsCheckingSize = invalidGPR
end

const UnboxedWasmCalleeStackSlot = CallerFrame - constexpr Wasm::numberOfIPIntCalleeSaveRegisters * SlotSize - MachineRegisterSize
const WasmToJSScratchSpaceSize = constexpr Wasm::WasmToJSScratchSpaceSize
const WasmToJSCallableFunctionSlot = constexpr Wasm::WasmToJSCallableFunctionSlot

const IPIntCalleeSaveSpaceAsVirtualRegisters = constexpr Wasm::numberOfIPIntCalleeSaveRegisters + constexpr Wasm::numberOfIPIntInternalRegisters
const IPIntCalleeSaveSpaceStackAligned = (IPIntCalleeSaveSpaceAsVirtualRegisters * SlotSize + StackAlignment - 1) & ~StackAlignmentMask

# Must match GPRInfo.h
if X86_64
    const NumberOfWasmArgumentGPRs = 6
    const NumberOfVolatileGPRs = NumberOfWasmArgumentGPRs + 2 // +2 for ws0 and ws1
elsif ARM64 or ARM64E or RISCV64
    const NumberOfWasmArgumentGPRs = 8
    const NumberOfVolatileGPRs = NumberOfWasmArgumentGPRs
elsif ARMv7
    # These 4 GPR holds only 2 JSValues in 2 pairs.
    const NumberOfWasmArgumentGPRs = 4
    const NumberOfVolatileGPRs = NumberOfWasmArgumentGPRs
else
    error
end

const NumberOfWasmArgumentFPRs = 8

##############################
# 2. Core interpreter macros #
##############################

macro ipintOp(name, impl)
    instructionLabel(name)
    impl()
end

# -----------------------------------
# 2.1: Core interpreter functionality
# -----------------------------------

# Get IPIntCallee object at startup
macro unboxWasmCallee(calleeBits, scratch)
if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), calleeBits
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, scratch
    loadp [scratch], scratch
    addp scratch, calleeBits
end

# Tail-call dispatch
macro advancePC(amount)
    addp amount, PC
end

macro advancePCByReg(amount)
    addp amount, PC
end

macro advanceMC(amount)
    addp amount, MC
end

macro advanceMCByReg(amount)
    addp amount, MC
end

macro decodeLEBVarUInt32(offset, dst, scratch1, scratch2, scratch3, scratch4)
    # if it's a single byte, fastpath it
    const tempPC = scratch4
    leap offset[PC], tempPC
    loadb [tempPC], dst

    bbb dst, 0x80, .fastpath
    # otherwise, set up for second iteration
    # next shift is 7
    move 7, scratch1
    # take off high bit
    subi 0x80, dst
    validateOpcodeConfig(scratch2)
.loop:
    addp 1, tempPC
    loadb [tempPC], scratch2
    # scratch3 = high bit 7
    # leave scratch2 with low bits 6-0
    move 0x80, scratch3
    andi scratch2, scratch3
    xori scratch3, scratch2
    lshifti scratch1, scratch2
    addi 7, scratch1
    ori scratch2, dst
    bbneq scratch3, 0, .loop
.fastpath:
end

macro checkStackOverflow(callee, scratch)
    loadi Wasm::IPIntCallee::m_maxFrameSizeInV128[callee], scratch
    mulp V128ISize, scratch
    subp cfr, scratch, scratch

if not ADDRESS64
    bpbeq scratch, cfr, .checkTrapAwareSoftStackLimit
    ipintException(StackOverflow)
.checkTrapAwareSoftStackLimit:
end
    bpbeq JSWebAssemblyInstance::m_stackMirror + StackManager::Mirror::m_trapAwareSoftStackLimit[wasmInstance], scratch, .stackHeightOK

.checkStack:
    operationCallMayThrowPreservingVolatileRegisters(macro()
        move scratch, a1
        move callee, a2
        cCall3(_ipint_extern_check_stack_and_vm_traps)
    end)

.stackHeightOK:
end

# ----------------------
# 2.2: Code organization
# ----------------------

# Instruction labels
# Important Note: If you don't use the unaligned global label from C++ (in our case we use the
# labels in InPlaceInterpreter.cpp) then some linkers will still remove the definition which
# causes all kinds of problems.

macro instructionLabel(instrname)
    aligned _ipint%instrname%_validate alignIPInt
    _ipint%instrname%_validate:
    _ipint%instrname%:
end

macro slowPathLabel(instrname)
    aligned _ipint%instrname%_slow_path_validate alignIPInt
    _ipint%instrname%_slow_path_validate:
    _ipint%instrname%_slow_path:
end

macro unimplementedInstruction(instrname)
    instructionLabel(instrname)
    validateOpcodeConfig(a0)
    break
end

macro reservedOpcode(opcode)
    unimplementedInstruction(_reserved_%opcode%)
end

# ---------------------------------------
# 2.3: Interacting with the outside world
# ---------------------------------------

# Memory
macro ipintReloadMemory()
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, t2)
    end
end

# Call site tracking

macro saveCallSiteIndex()
if X86_64
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
end
    loadp Wasm::IPIntCallee::m_bytecode[ws0], t0
    negp t0
    addp PC, t0
    storei t0, CallSiteIndex[cfr]
end

# Operation Calls

macro operationCall(fn)
    validateOpcodeConfig(a0)

    move wasmInstance, a0
    push PC, MC
    if ARM64 or ARM64E
        push PL, ws0
    elsif X86_64
        push PL
        # preserve 16 byte alignment.
        subq MachineRegisterSize, sp
    end
    fn()
    if ARM64 or ARM64E
        pop ws0, PL
    elsif X86_64
        addq MachineRegisterSize, sp
        pop PL
    end
    pop MC, PC
end

macro operationCallMayThrowImpl(fn, sizeOfExtraRegistersPreserved)
    saveCallSiteIndex()
    validateOpcodeConfig(a0)

    move wasmInstance, a0
    push PC, MC
    if ARM64 or ARM64E
        push PL, ws0
    elsif X86_64
        push PL
        # preserve 16 byte alignment.
        subq MachineRegisterSize, sp
    end
    fn()
    bpneq r1, (constexpr JSC::IPInt::SlowPathExceptionTag), .continuation

    storei r0, ArgumentCountIncludingThis + PayloadOffset[cfr]
    addp sizeOfExtraRegistersPreserved + (4 * MachineRegisterSize), sp
    jmp _wasm_throw_from_slow_path_trampoline
.continuation:
    if ARM64 or ARM64E
        pop ws0, PL
    elsif X86_64
        addq MachineRegisterSize, sp
        pop PL
    end
    pop MC, PC
end

macro operationCallMayThrow(fn)
    operationCallMayThrowImpl(fn, 0)
end

macro operationCallMayThrowPreservingVolatileRegisters(fn)
    preserveWasmVolatileRegisters()
    operationCallMayThrowImpl(fn, (NumberOfVolatileGPRs * MachineRegisterSize) + (NumberOfWasmArgumentFPRs * VectorRegisterSize))
    restoreWasmVolatileRegisters()
end

# Exception handling
macro ipintException(exception)
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
end

# OSR
macro ipintPrologueOSR(increment)
if JIT
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    baddis increment, Wasm::IPIntCallee::m_tierUpCounter + Wasm::IPIntTierUpCounter::m_counter[ws0], .continue

    preserveWasmArgumentRegisters()

if not ARMv7
    ipintReloadMemory()
    push memoryBase, boundsCheckingSize
end

    move cfr, a1
    operationCall(macro() cCall2(_ipint_extern_prologue_osr) end)
    move r0, ws0

if not ARMv7
    pop boundsCheckingSize, memoryBase
end

    restoreWasmArgumentRegisters()

    btpz ws0, .continue

    restoreIPIntRegisters()
    restoreCallerPCAndCFR()

    if ARM64E
        leap _g_config, ws1
        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmOSREntry) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
    else
        jmp ws0, WasmEntryPtrTag
    end

.continue:
    if ARMv7
        break # FIXME: ipint support.
    end # ARMv7
end # JIT
end

macro ipintLoopOSR(increment)
if JIT and not ARMv7
    validateOpcodeConfig(ws0)
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    baddis increment, Wasm::IPIntCallee::m_tierUpCounter + Wasm::IPIntTierUpCounter::m_counter[ws0], .continue

    move cfr, a1
    move PC, a2
    # Add 1 to the index due to WTF::UncheckedKeyHashMap not supporting 0 as a key
    addq 1, a2
    move PL, a3
    operationCall(macro() cCall4(_ipint_extern_loop_osr) end)
    btpz r1, .recover
    restoreIPIntRegisters()
    restoreCallerPCAndCFR()
    move r0, a0

    if ARM64E
        move r1, ws0
        leap _g_config, ws1
        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmOSREntry) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
    else
        jmp r1, WasmEntryPtrTag
    end

.recover:
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
.continue:
end
end

macro ipintEpilogueOSR(increment)
if JIT and not ARMv7
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    baddis increment, Wasm::IPIntCallee::m_tierUpCounter + Wasm::IPIntTierUpCounter::m_counter[ws0], .continue

    move cfr, a1
    operationCall(macro() cCall2(_ipint_extern_epilogue_osr) end)
.continue:
end
end

################################
# 3. Helper interpreter macros #
################################

macro argumINTAlign(instrname)
    aligned _ipint_argumINT%instrname%_validate alignArgumInt
    _ipint_argumINT%instrname%_validate:
    _argumINT%instrname%:
end

macro mintAlign(instrname)
    aligned _ipint_mint%instrname%_validate alignMInt
    _ipint_mint%instrname%_validate:
    _mint%instrname%:
end

macro uintAlign(instrname)
    aligned _ipint_uint%instrname%_validate alignUInt
    _ipint_uint%instrname%_validate:
    _uint%instrname%:
end

# On JSVALUE64, each 64-bit argument GPR holds one whole Wasm value.
# On JSVALUE32_64, a consecutive pair of even/odd numbered GPRs hold a single
# Wasm value (even if that value is i32/f32, the odd numbered GPR holds the
# more significant word).
macro forEachWasmArgumentGPR(fn)
    if ARM64 or ARM64E
        fn(0, wa0, wa1)
        fn(2, wa2, wa3)
        fn(4, wa4, wa5)
        fn(6, wa6, wa7)
    elsif JSVALUE64
        fn(0, wa0, wa1)
        fn(2, wa2, wa3)
        fn(4, wa4, wa5)
    else
        fn(0, wa0, wa1)
        fn(2, wa2, wa3)
    end
end

macro forEachWasmArgumentFPR(fn)
    fn(0, wfa0, wfa1)
    fn(2, wfa2, wfa3)
    fn(4, wfa4, wfa5)
    fn(6, wfa6, wfa7)
end

macro preserveWasmGPRArgumentRegistersImpl()
    forEachWasmArgumentGPR(macro (index, gpr1, gpr2)
        if ARM64 or ARM64E
            storepairq gpr1, gpr2, index * MachineRegisterSize[sp]
        elsif JSVALUE64
            storeq gpr1, (index + 0) * MachineRegisterSize[sp]
            storeq gpr2, (index + 1) * MachineRegisterSize[sp]
        else
            store2ia gpr1, gpr2, index * MachineRegisterSize[sp]
        end
    end)
end

macro restoreWasmGPRArgumentRegistersImpl()
    forEachWasmArgumentGPR(macro (index, gpr1, gpr2)
        if ARM64 or ARM64E
            loadpairq index * MachineRegisterSize[sp], gpr1, gpr2
        elsif JSVALUE64
            loadq (index + 0) * MachineRegisterSize[sp], gpr1
            loadq (index + 1) * MachineRegisterSize[sp], gpr2
        else
            load2ia index * MachineRegisterSize[sp], gpr1, gpr2
        end
    end)
end

macro preserveWasmFPRArgumentRegistersImpl(fprBaseOffset)
    forEachWasmArgumentFPR(macro (index, fpr1, fpr2)
        if ARM64 or ARM64E
            storepairv fpr1, fpr2, fprBaseOffset + index * VectorRegisterSize[sp]
        elsif X86_64
            storev fpr1, fprBaseOffset + (index + 0) * VectorRegisterSize[sp]
            storev fpr2, fprBaseOffset + (index + 1) * VectorRegisterSize[sp]
        else
            stored fpr1, fprBaseOffset + (index + 0) * VectorRegisterSize[sp]
            stored fpr2, fprBaseOffset + (index + 1) * VectorRegisterSize[sp]
        end
    end)
end

macro restoreWasmFPRArgumentRegistersImpl(fprBaseOffset)
    forEachWasmArgumentFPR(macro (index, fpr1, fpr2)
        if ARM64 or ARM64E
            loadpairv fprBaseOffset + index * VectorRegisterSize[sp], fpr1, fpr2
        elsif X86_64
            loadv fprBaseOffset + (index + 0) * VectorRegisterSize[sp], fpr1
            loadv fprBaseOffset + (index + 1) * VectorRegisterSize[sp], fpr2
        else
            loadd fprBaseOffset + (index + 0) * VectorRegisterSize[sp], fpr1
            loadd fprBaseOffset + (index + 1) * VectorRegisterSize[sp], fpr2
        end
    end)
end

macro preserveWasmArgumentRegisters()
    const gprStorageSize = NumberOfWasmArgumentGPRs * MachineRegisterSize
    const fprStorageSize = NumberOfWasmArgumentFPRs * VectorRegisterSize

    subp gprStorageSize + fprStorageSize, sp
    preserveWasmGPRArgumentRegistersImpl()
    preserveWasmFPRArgumentRegistersImpl(gprStorageSize)
end

macro preserveWasmVolatileRegisters()
    const gprStorageSize = NumberOfVolatileGPRs * MachineRegisterSize
    const fprStorageSize = NumberOfWasmArgumentFPRs * VectorRegisterSize

    subp gprStorageSize + fprStorageSize, sp
    preserveWasmGPRArgumentRegistersImpl()
if X86_64
    storeq ws0, (NumberOfWasmArgumentGPRs + 0) * MachineRegisterSize[sp]
    storeq ws1, (NumberOfWasmArgumentGPRs + 1) * MachineRegisterSize[sp]
end
    preserveWasmFPRArgumentRegistersImpl(gprStorageSize)
end

macro restoreWasmArgumentRegisters()
    const gprStorageSize = NumberOfWasmArgumentGPRs * MachineRegisterSize
    const fprStorageSize = NumberOfWasmArgumentFPRs * VectorRegisterSize

    restoreWasmGPRArgumentRegistersImpl()
    restoreWasmFPRArgumentRegistersImpl(gprStorageSize)
    addp gprStorageSize + fprStorageSize, sp
end

macro restoreWasmVolatileRegisters()
    const gprStorageSize = NumberOfVolatileGPRs * MachineRegisterSize
    const fprStorageSize = NumberOfWasmArgumentFPRs * VectorRegisterSize

    restoreWasmGPRArgumentRegistersImpl()
if X86_64
    loadq (NumberOfWasmArgumentGPRs + 0) * MachineRegisterSize[sp], ws0
    loadq (NumberOfWasmArgumentGPRs + 1) * MachineRegisterSize[sp], ws1
end
    restoreWasmFPRArgumentRegistersImpl(gprStorageSize)
    addp gprStorageSize + fprStorageSize, sp
end

macro reloadMemoryRegistersFromInstance(instance, scratch1)
if not ARMv7
    loadp JSWebAssemblyInstance::m_cachedMemory[instance], memoryBase
    loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[instance], boundsCheckingSize
    cagedPrimitiveMayBeNull(memoryBase, scratch1) # If boundsCheckingSize is 0, pointer can be a nullptr.
end
end

macro throwException(exception)
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
end

if ARMv7
macro branchIfWasmException(exceptionTarget)
    loadp CodeBlock[cfr], t3
    loadp JSWebAssemblyInstance::m_vm[t3], t3
    btpz VM::m_exception[t3], .noException
    jmp exceptionTarget
.noException:
end
end

##############################
# 4. Interpreter entrypoints #
##############################

// If you change this, make sure to modify JSToWasm.cpp:createJSToWasmJITShared
op(js_to_wasm_wrapper_entry, macro ()
    if not WEBASSEMBLY or C_LOOP
        error
    end

    macro clobberVolatileRegisters()
        if ARM64 or ARM64E
            emit "movz  x9, #0xBAD"
            emit "movz x10, #0xBAD"
            emit "movz x11, #0xBAD"
            emit "movz x12, #0xBAD"
            emit "movz x13, #0xBAD"
            emit "movz x14, #0xBAD"
            emit "movz x15, #0xBAD"
            emit "movz x16, #0xBAD"
            emit "movz x17, #0xBAD"
            emit "movz x18, #0xBAD"
        elsif ARMv7
            emit "mov r4, #0xBAD"
            emit "mov r5, #0xBAD"
            emit "mov r6, #0xBAD"
            emit "mov r8, #0xBAD"
            emit "mov r9, #0xBAD"
            emit "mov r12, #0xBAD"
        end
    end

    macro repeat(scratch, f)
        move 0xBEEF, scratch
        f(0)
        f(1)
        f(2)
        f(3)
        f(4)
        f(5)
        f(6)
        f(7)
        f(8)
        f(9)
        f(10)
        f(11)
        f(12)
        f(13)
        f(14)
        f(15)
        f(16)
        f(17)
        f(18)
        f(19)
        f(20)
        f(21)
        f(22)
        f(23)
        f(24)
        f(25)
        f(26)
        f(27)
        f(28)
        f(29)
    end

    macro saveJSToWasmRegisters()
        subp constexpr Wasm::JSToWasmCallee::SpillStackSpaceAligned, sp
        if ARM64 or ARM64E
            storepairq memoryBase, boundsCheckingSize, -2 * SlotSize[cfr]
            storep wasmInstance, -3 * SlotSize[cfr]
        elsif X86_64
            # These must match the wasmToJS thunk, since the unwinder won't be able to tell who made this frame.
            storep boundsCheckingSize, -1 * SlotSize[cfr]
            storep memoryBase, -2 * SlotSize[cfr]
            storep wasmInstance, -3 * SlotSize[cfr]
        else
            storei wasmInstance, -1 * SlotSize[cfr]
        end
    end

    macro restoreJSToWasmRegisters()
        if ARM64 or ARM64E
            loadpairq -2 * SlotSize[cfr], memoryBase, boundsCheckingSize
            loadp -3 * SlotSize[cfr], wasmInstance
        elsif X86_64
            loadp -1 * SlotSize[cfr], boundsCheckingSize
            loadp -2 * SlotSize[cfr], memoryBase
            loadp -3 * SlotSize[cfr], wasmInstance
        else
            loadi -1 * SlotSize[cfr], wasmInstance
        end
        addp constexpr Wasm::JSToWasmCallee::SpillStackSpaceAligned, sp
    end

    macro getWebAssemblyFunctionAndSetNativeCalleeAndInstance(webAssemblyFunctionOut, scratch)
        # Re-load WebAssemblyFunction Callee
        loadp Callee[cfr], webAssemblyFunctionOut

        # Replace the WebAssemblyFunction Callee with our JSToWasm NativeCallee
        loadp WebAssemblyFunction::m_boxedJSToWasmCallee[webAssemblyFunctionOut], scratch
        storep scratch, Callee[cfr] # JSToWasmCallee
        if not JSVALUE64
            move constexpr JSValue::NativeCalleeTag, scratch
            storep scratch, TagOffset + Callee[cfr]
        end
        storep wasmInstance, CodeBlock[cfr]
    end

if ASSERT_ENABLED
    clobberVolatileRegisters()
end

    tagReturnAddress sp
    preserveCallerPCAndCFR()
    saveJSToWasmRegisters()

    # Load data from the entry callee
    # This was written by doVMEntry
    loadp Callee[cfr], ws0 # WebAssemblyFunction*
    loadp WebAssemblyFunction::m_importableFunction + Wasm::WasmOrJSImportableFunction::targetInstance[ws0], wasmInstance

    # Allocate stack space
    loadi WebAssemblyFunction::m_frameSize[ws0], wa0
    subp sp, wa0, wa0

if not ADDRESS64
    bpa wa0, cfr, .stackOverflow
end
    # We don't need to check m_trapAwareSoftStackLimit here because we'll end up
    # entering the Wasm function, and its prologue will handle the trap check.
    bpbeq wa0, JSWebAssemblyInstance::m_stackMirror + StackManager::Mirror::m_softStackLimit[wasmInstance], .stackOverflow

    move wa0, sp

if ASSERT_ENABLED
    repeat(wa0, macro (i)
        storep wa0, -i * SlotSize + constexpr Wasm::JSToWasmCallee::RegisterStackSpaceAligned[sp]
    end)
end

    # a0 = current stack frame position
    move sp, a0

    # Save wasmInstance and put the correct Callee into the stack for building the frame
    storep wasmInstance, CodeBlock[cfr]

if JSVALUE64
    loadp Callee[cfr], memoryBase
    transferp WebAssemblyFunction::m_boxedJSToWasmCallee[ws0], Callee[cfr]
else
    # Store old Callee to the stack temporarily
    loadp Callee[cfr], ws1
    push ws1, ws1
    loadp WebAssemblyFunction::m_boxedJSToWasmCallee[ws0], ws1
    storep ws1, Callee[cfr]
end

    # Prepare frame
    move ws0, a2
    move cfr, a1
    cCall3(_operationJSToWasmEntryWrapperBuildFrame)

    # Restore Callee slot
if JSVALUE64
    storep memoryBase, Callee[cfr]
else
    loadp [sp], ws0
    addp 2 * SlotSize, sp
    storep ws0, Callee[cfr]
end

    btpnz r1, .buildEntryFrameThrew
    move r0, ws0

    # Memory
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, wa0)
    end

    # Arguments

    forEachWasmArgumentGPR(macro (index, gpr1, gpr2)
        if ARM64 or ARM64E
            loadpairq index * MachineRegisterSize[sp], gpr1, gpr2
        elsif JSVALUE64
            loadq (index + 0) * MachineRegisterSize[sp], gpr1
            loadq (index + 1) * MachineRegisterSize[sp], gpr2
        else
            load2ia index * MachineRegisterSize[sp], gpr1, gpr2
        end
    end)
    forEachWasmArgumentFPR(macro (index, fpr1, fpr2)
        const base = NumberOfWasmArgumentGPRs * MachineRegisterSize
        if ARM64 or ARM64E
            loadpaird base + index * FPRRegisterSize[sp], fpr1, fpr2
        else
            loadd base + (index + 0) * FPRRegisterSize[sp], fpr1
            loadd base + (index + 1) * FPRRegisterSize[sp], fpr2
        end
    end)

    # Pop argument space values
    addp constexpr Wasm::JSToWasmCallee::RegisterStackSpaceAligned, sp

if ASSERT_ENABLED
    repeat(ws1, macro (i)
        storep ws1, -i * SlotSize[sp]
    end)
end

    getWebAssemblyFunctionAndSetNativeCalleeAndInstance(ws1, ws0)

    # Load callee entrypoint
    loadp WebAssemblyFunction::m_importableFunction + Wasm::WasmOrJSImportableFunction::entrypointLoadLocation[ws1], ws0
    loadp [ws0], ws0

    # Set the callee's interpreter Wasm::Callee
if JSVALUE64
    transferp WebAssemblyFunction::m_importableFunction + Wasm::WasmOrJSImportableFunction::boxedCallee[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8[sp]
else
    transferp WebAssemblyFunction::m_importableFunction + Wasm::WasmOrJSImportableFunction::boxedCallee + PayloadOffset[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + PayloadOffset[sp]
    transferp WebAssemblyFunction::m_importableFunction + Wasm::WasmOrJSImportableFunction::boxedCallee + TagOffset[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + TagOffset[sp]
end

    call ws0, WasmEntryPtrTag

if ASSERT_ENABLED
    clobberVolatileRegisters()
end

    # Restore SP
    loadp Callee[cfr], ws0 # CalleeBits(JSToWasmCallee*)
    unboxWasmCallee(ws0, ws1)

    loadi Wasm::JSToWasmCallee::m_frameSize[ws0], ws1
    subp cfr, ws1, ws1
    move ws1, sp
    subp constexpr Wasm::JSToWasmCallee::SpillStackSpaceAligned, sp

if ASSERT_ENABLED
    repeat(ws0, macro (i)
        storep ws0, -i * SlotSize + constexpr Wasm::JSToWasmCallee::RegisterStackSpaceAligned[sp]
    end)
end

    # Save return registers
    # Return register are the same as the argument registers.
    forEachWasmArgumentGPR(macro (index, gpr1, gpr2)
        if ARM64 or ARM64E
            storepairq gpr1, gpr2, index * MachineRegisterSize[sp]
        elsif JSVALUE64
            storeq gpr1, (index + 0) * MachineRegisterSize[sp]
            storeq gpr2, (index + 1) * MachineRegisterSize[sp]
        else
            store2ia gpr1, gpr2, index * MachineRegisterSize[sp]
        end
    end)
    forEachWasmArgumentFPR(macro (index, fpr1, fpr2)
        const base = NumberOfWasmArgumentGPRs * MachineRegisterSize
        if ARM64 or ARM64E
            storepaird fpr1, fpr2, base + index * FPRRegisterSize[sp]
        else
            stored fpr1, base + (index + 0) * FPRRegisterSize[sp]
            stored fpr2, base + (index + 1) * FPRRegisterSize[sp]
        end
    end)

    # Prepare frame
    move sp, a0
    move cfr, a1
    cCall2(_operationJSToWasmEntryWrapperBuildReturnFrame)

if ARMv7
    branchIfWasmException(.unwind)
else
    btpnz r1, .unwind
end

    # Clean up and return
    restoreJSToWasmRegisters()
if ASSERT_ENABLED
    clobberVolatileRegisters()
end
    restoreCallerPCAndCFR()
    ret

    # We need to set our NativeCallee/instance here since haven't done it already and wasm_throw_from_slow_path_trampoline expects them.
.stackOverflow:
    getWebAssemblyFunctionAndSetNativeCalleeAndInstance(ws1, ws0)
    throwException(StackOverflow)

.buildEntryFrameThrew:
    getWebAssemblyFunctionAndSetNativeCalleeAndInstance(ws1, ws0)

.unwind:
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], a0
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(a0, a1)

# Should be (not USE_BUILTIN_FRAME_ADDRESS) but need to keep down the size of LLIntAssembly.h
if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    call _operationWasmUnwind
    jumpToException()
end)

op(wasm_to_wasm_ipint_wrapper_entry, macro()
    # We have only pushed PC (intel) or pushed nothing(others), and we
    # are still in the caller frame.
if X86_64
    loadp (Callee - CallerFrameAndPCSize + 8)[sp], ws0
else
    loadp (Callee - CallerFrameAndPCSize)[sp], ws0
end

    unboxWasmCallee(ws0, ws1)
    loadp JSC::Wasm::IPIntCallee::m_entrypoint[ws0], ws0

    # Load the instance
if X86_64
    loadp (CodeBlock - CallerFrameAndPCSize + 8)[sp], wasmInstance
else
    loadp (CodeBlock - CallerFrameAndPCSize)[sp], wasmInstance
end

    # Memory
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, ws1)
    end

    jmp ws0, WasmEntryPtrTag
end)

# This is the interpreted analogue to WasmToJS.cpp:wasmToJS
op(wasm_to_js_wrapper_entry, macro()
    # We have only pushed PC (intel) or pushed nothing(others), and we
    # are still in the caller frame.
    # Load this before we create the stack frame, since we lose old cfr, which we wrote Callee to

    # We repurpose this slot temporarily for a WasmCallableFunction* from resolveWasmCall and friends.
    tagReturnAddress sp
    preserveCallerPCAndCFR()

    const RegisterSpaceScratchSize = 0x80
    subp (WasmToJSScratchSpaceSize + RegisterSpaceScratchSize), sp

    loadp CodeBlock[cfr], ws0
    storep ws0, WasmToJSCallableFunctionSlot[cfr]

    # Store all the registers here

    forEachWasmArgumentGPR(macro (index, gpr1, gpr2)
        if ARM64 or ARM64E
            storepairq gpr1, gpr2, index * MachineRegisterSize[sp]
        elsif JSVALUE64
            storeq gpr1, (index + 0) * MachineRegisterSize[sp]
            storeq gpr2, (index + 1) * MachineRegisterSize[sp]
        else
            store2ia gpr1, gpr2, index * MachineRegisterSize[sp]
        end
    end)
    forEachWasmArgumentFPR(macro (index, fpr1, fpr2)
        const base = NumberOfWasmArgumentGPRs * MachineRegisterSize
        if ARM64 or ARM64E
            storepaird fpr1, fpr2, base + index * FPRRegisterSize[sp]
        else
            stored fpr1, base + (index + 0) * FPRRegisterSize[sp]
            stored fpr2, base + (index + 1) * FPRRegisterSize[sp]
        end
    end)

if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    move ws0, a1
    cCall2(_operationGetWasmCalleeStackSize)

    move sp, a2
    subp r0, sp
    move sp, a0
    move cfr, a1
    move wasmInstance, a3
    cCall4(_operationWasmToJSExitMarshalArguments)
    btpnz r0, .handleException

    loadp WasmToJSCallableFunctionSlot[cfr], t2
    loadp JSC::Wasm::WasmOrJSImportableFunctionCallLinkInfo::importFunction[t2], t0
if not JSVALUE64
    move (constexpr JSValue::CellTag), t1
end
    loadp JSC::Wasm::WasmOrJSImportableFunctionCallLinkInfo::callLinkInfo[t2], t2

    # calleeGPR = t0
    # callLinkInfoGPR = t2
    # callTargetGPR = t5
    loadp CallLinkInfo::m_monomorphicCallDestination[t2], t5

    # scratch = t3
    loadp CallLinkInfo::m_callee[t2], t3
    bpeq t3, t0, .found
    btpnz t3, (constexpr CallLinkInfo::polymorphicCalleeMask), .found

.notfound:
if ARM64 or ARM64E
    pcrtoaddr _llint_default_call_trampoline, t5
else
    leap (_llint_default_call_trampoline), t5
end
    loadp CallLinkInfo::m_codeBlock[t2], t3
    storep t3, (CodeBlock - CallerFrameAndPCSize)[sp]
    call _llint_default_call_trampoline
    jmp .postcall
.found:
    # jit.transferPtr CallLinkInfo::codeBlock[t2], CodeBlock[cfr]
    loadp CallLinkInfo::m_codeBlock[t2], t3
    storep t3, (CodeBlock - CallerFrameAndPCSize)[sp]
    call t5, JSEntryPtrTag

.postcall:
    storep r0, [sp]
if not JSVALUE64
    storep r1, TagOffset[sp]
end

    move sp, a0
    move cfr, a1
    move wasmInstance, a2
    cCall3(_operationWasmToJSExitMarshalReturnValues)
    btpnz r0, .handleException

.end:
    # Retrieve return registers
    # Return register are the same as the argument registers.
    forEachWasmArgumentGPR(macro (index, gpr1, gpr2)
        if ARM64 or ARM64E
            loadpairq index * MachineRegisterSize[sp], gpr1, gpr2
        elsif JSVALUE64
            loadq (index + 0) * MachineRegisterSize[sp], gpr1
            loadq (index + 1) * MachineRegisterSize[sp], gpr2
        else
            load2ia index * MachineRegisterSize[sp], gpr1, gpr2
        end
    end)
    forEachWasmArgumentFPR(macro (index, fpr1, fpr2)
        const base = NumberOfWasmArgumentGPRs * MachineRegisterSize
        if ARM64 or ARM64E
            loadpaird base + index * FPRRegisterSize[sp], fpr1, fpr2
        else
            loadd base + (index + 0) * FPRRegisterSize[sp], fpr1
            loadd base + (index + 1) * FPRRegisterSize[sp], fpr2
        end
    end)

    loadp CodeBlock[cfr], wasmInstance
    restoreCallerPCAndCFR()
    ret

.handleException:
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], a0
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(a0, a1)

if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    call _operationWasmUnwind
    jumpToException()
end)

macro jumpToException()
    if ARM64E
        move r0, a0
        validateOpcodeConfig(a1)
        leap _g_config, a1
        jmp JSCConfigGateMapOffset + (constexpr Gate::exceptionHandler) * PtrSize[a1], NativeToJITGatePtrTag # ExceptionHandlerPtrTag
    else
        jmp r0, ExceptionHandlerPtrTag
    end
end

op(wasm_throw_from_slow_path_trampoline, macro ()
    validateOpcodeConfig(t5)
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    loadp VM::topEntryFrame[t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    move cfr, a0
    move wasmInstance, a1
    # Slow paths and the throwException macro store the exception code in the ArgumentCountIncludingThis slot
    loadi ArgumentCountIncludingThis + PayloadOffset[cfr], a2
    storei 0, CallSiteIndex[cfr]
    cCall3(_slow_path_wasm_throw_exception)
    jumpToException()
end)

# Almost the same as wasm_throw_from_slow_path_trampoline, but the exception
# has already been thrown and is now sitting in the VM.
op(wasm_unwind_from_slow_path_trampoline, macro()
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    loadp VM::topEntryFrame[t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    move cfr, a0
    move wasmInstance, a1
    storei 0, CallSiteIndex[cfr]
    cCall3(_slow_path_wasm_unwind_exception)
    jumpToException()
end)

op(wasm_throw_from_fault_handler_trampoline_reg_instance, macro ()
    move wasmInstance, a2
    loadp JSWebAssemblyInstance::m_vm[a2], a0
    loadp VM::topEntryFrame[a0], a0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(a0)

    move cfr, a0
    move a2, a1
    loadi JSWebAssemblyInstance::m_exception[a1], a2

    storei 0, CallSiteIndex[cfr]
    cCall3(_slow_path_wasm_throw_exception)
    jumpToException()
end)

op(ipint_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
    preserveCallerPCAndCFR()
    saveIPIntRegisters()
    storep wasmInstance, CodeBlock[cfr]
    loadp Callee[cfr], ws0
    unboxWasmCallee(ws0, ws1)
    storep ws0, UnboxedWasmCalleeStackSlot[cfr]

    # on x86, PL will hold the PC relative offset for argumINT, then IB will take over
    if X86_64
        initPCRelative(ipint_entry, PL)
    end
ipintEntry()
else
    break
end
end)

if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
.ipint_entry_end_local:
    argumINTInitializeDefaultLocals()
    jmp .ipint_entry_end_local

.ipint_entry_finish_zero:
    argumINTFinish()

    loadp CodeBlock[cfr], wasmInstance
    # OSR Check
if ARMv7
    ipintPrologueOSR(500000) # FIXME: support IPInt.
    break
else
    ipintPrologueOSR(5)
end
    move cfr, a1
    operationCall(macro() cCall2(_ipint_extern_prepare_function_body) end)
    move r0, ws0

    move sp, PL
    loadp Wasm::IPIntCallee::m_bytecode[ws0], PC
    loadp Wasm::IPIntCallee::m_metadata + VectorBufferOffset[ws0], MC

    # Load memory
    ipintReloadMemory()

    nextIPIntInstruction()

.ipint_exit:
    restoreIPIntRegisters()
    restoreCallerPCAndCFR()
    if ARM64E
        leap _g_config, ws0
        jmp JSCConfigGateMapOffset + (constexpr Gate::returnFromLLInt) * PtrSize[ws0], NativeToJITGatePtrTag
    else
        ret
    end
else
    break
end

macro ipintCatchCommon()
    validateOpcodeConfig(t0)
    getVMFromCallFrame(t3, t0)
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)

    loadp VM::callFrameForCatch[t3], cfr
    storep 0, VM::callFrameForCatch[t3]

    loadp VM::targetInterpreterPCForThrow[t3], PC
    loadp VM::targetInterpreterMetadataPCForThrow[t3], MC

if ARMv7
    push MC
end
    loadp Callee[cfr], ws0
    unboxWasmCallee(ws0, ws1)
    storep ws0, UnboxedWasmCalleeStackSlot[cfr]
if ARMv7
    pop MC
end

    loadp CodeBlock[cfr], wasmInstance
    loadp Wasm::IPIntCallee::m_bytecode[ws0], t1
    addp t1, PC
    loadp Wasm::IPIntCallee::m_metadata + VectorBufferOffset[ws0], t1
    addp t1, MC

    # Recompute PL
    if ARM64 or ARM64E
        loadpairi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], t0, t1
    else
        loadi Wasm::IPIntCallee::m_numRethrowSlotsToAlloc[ws0], t1
        loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], t0
    end
    addp t1, t0
    mulp LocalSize, t0
    addp IPIntCalleeSaveSpaceStackAligned, t0
    subp cfr, t0, PL

    loadi [MC], t0
    addp t1, t0
    mulp StackValueSize, t0
    addp IPIntCalleeSaveSpaceStackAligned, t0
if ARM64 or ARM64E
    subp cfr, t0, sp
else
    subp cfr, t0, t0
    move t0, sp
end

if X86_64
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
end
end

op(ipint_catch_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64)
    ipintCatchCommon()

    move cfr, a1
    move sp, a2
    move PL, a3
    operationCall(macro() cCall4(_ipint_extern_retrieve_and_clear_exception) end)

    ipintReloadMemory()
    advanceMC(4)
    nextIPIntInstruction()
else
    break
end
end)

op(ipint_catch_all_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64)
    ipintCatchCommon()

    move cfr, a1
    move 0, a2
    move PL, a3
    operationCall(macro() cCall4(_ipint_extern_retrieve_and_clear_exception) end)

    ipintReloadMemory()
    advanceMC(4)
    nextIPIntInstruction()
else
    break
end
end)

op(ipint_table_catch_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
    ipintCatchCommon()

    # push arguments but no ref: sp in a2, call normal operation

    move cfr, a1
    move sp, a2
    move PL, a3
    operationCall(macro() cCall4(_ipint_extern_retrieve_and_clear_exception) end)

    ipintReloadMemory()
    advanceMC(4)
    jmp _ipint_block
else
    break
end
end)

op(ipint_table_catch_ref_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
    ipintCatchCommon()

    # push both arguments and ref

    move cfr, a1
    move sp, a2
    move PL, a3
    operationCall(macro() cCall4(_ipint_extern_retrieve_clear_and_push_exception_and_arguments) end)

    ipintReloadMemory()
    advanceMC(4)
    jmp _ipint_block
else
    break
end
end)

op(ipint_table_catch_all_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
    ipintCatchCommon()

    # do nothing: 0 in sp for no arguments, call normal operation

    move cfr, a1
    move 0, a2
    move PL, a3
    operationCall(macro() cCall4(_ipint_extern_retrieve_and_clear_exception) end)

    ipintReloadMemory()
    advanceMC(4)
    jmp _ipint_block
else
    break
end
end)

op(ipint_table_catch_allref_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
    ipintCatchCommon()

    # push only the ref

    move cfr, a1
    move sp, a2
    move PL, a3
    operationCall(macro() cCall4(_ipint_extern_retrieve_clear_and_push_exception) end)

    ipintReloadMemory()
    advanceMC(4)
    jmp _ipint_block
else
    break
end
end)

# Trampoline entrypoints

op(ipint_trampoline, macro ()
    tagReturnAddress sp
    jmp _ipint_entry
end)

# Naming dependencies:
#
# In the following two macros, certain identifiers replicate naming conventions
# defined by C macros in wasm/js/WebAssemblyBuiltin.{h, cpp}.
# These dependencies are marked with "[!]".

# wasmInstanceArgGPR is the GPR used to pass the Wasm instance pointer.
# It must map onto the argument following the actual arguments of the builtin.
# IMPORTANT: Any changes to the trampoline logic must be replicated in its JIT counterpart in WebAssemblyBuiltinThunk.cpp.
macro wasmBuiltinCallTrampoline(setName, builtinName, wasmInstanceArgGPR)
    functionPrologue()

    # IPInt stores the callee and wasmInstance into the frame but JIT tiers don't, so we must do that here.
    leap JSWebAssemblyInstance::m_builtinCalleeBits[wasmInstance], t5
    loadp WasmBuiltinCalleeOffsets::%setName%__%builtinName%[t5], t5  # [!] BUILTIN_FULL_NAME(setName, builtinName)
    storep t5, Callee[cfr]
    storep wasmInstance, CodeBlock[cfr]

    # Set VM topCallFrame to null to not build an unnecessary stack trace if the function throws an exception.
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    storep 0, VM::topCallFrame[t5]

    # Add the extra wasmInstance arg
    move wasmInstance, wasmInstanceArgGPR
    call _wasm_builtin__%setName%__%builtinName%  # [!] BUILTIN_WASM_ENTRY_NAME(setName, builtinName)

    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    btpnz VM::m_exception[t5], .handleException

    # On x86, a0 and r0 are distinct (a0=rdi, r0=rax). The host function returns the result in r0,
    # but IPInt always expects it in a0.
if X86_64
    move r0, a0
end

    functionEpilogue()
    ret

.handleException:
    jmp _wasm_unwind_from_slow_path_trampoline
end

macro defineWasmBuiltinTrampoline(setName, builtinName, wasmInstanceArgGPR)
global _wasm_builtin_trampoline__%setName%__%builtinName%    # [!] BUILTIN_TRAMPOLINE_NAME(setName, builtinName)
_wasm_builtin_trampoline__%setName%__%builtinName%:
    wasmBuiltinCallTrampoline(setName, builtinName, wasmInstanceArgGPR)
end


#   js-string builtins, in order of appearance in the spec


# (externref, wasmInstance) -> externref
defineWasmBuiltinTrampoline(jsstring, cast, a1)

# (externref, wasmInstance) -> i32
defineWasmBuiltinTrampoline(jsstring, test, a1)

# (arrayref, i32, i32, wasmInstance) -> externref
defineWasmBuiltinTrampoline(jsstring, fromCharCodeArray, a3)

# (externref, arrayref, i32, wasmInstance) -> externref
defineWasmBuiltinTrampoline(jsstring, intoCharCodeArray, a3)

# (i32, wasmInstance) -> externref
defineWasmBuiltinTrampoline(jsstring, fromCharCode, a1)

# (i32, wasmInstance) -> externref
defineWasmBuiltinTrampoline(jsstring, fromCodePoint, a1)

# (externref, i32, wasmInstance) -> i32
defineWasmBuiltinTrampoline(jsstring, charCodeAt, a2)

# (externref, i32, wasmInstance) -> i32
defineWasmBuiltinTrampoline(jsstring, codePointAt, a2)

# (externref, wasmInstance) -> i32
defineWasmBuiltinTrampoline(jsstring, length, a1)

# (externref, externref, wasmInstance) -> externref
defineWasmBuiltinTrampoline(jsstring, concat, a2)

# (externref, i32, i32, wasmInstance) -> externref
defineWasmBuiltinTrampoline(jsstring, substring, a3)

# (externref, externref, wasmInstance) -> i32
defineWasmBuiltinTrampoline(jsstring, equals, a2)

# (externref, externref, wasmInstance) -> i32
defineWasmBuiltinTrampoline(jsstring, compare, a2)

# Low-level logic for the handling of JSPI futures

# void captureABICalleeSaves(CPURegister* buffer)
global _captureABICalleeSaves
_captureABICalleeSaves:
    functionPrologue()
    copyCalleeSavesToBuffer(a0)
    functionEpilogue()
    ret

# void teleportForJSPI(CPURegister* calleeSaves, CallFrame* returnOutOfFrame, EncodedJSValue result)
global _teleportForJSPI
_teleportForJSPI:
    restoreCalleeSavesFromBuffer(a0)
    move a1, sp
    move a2, r0
    functionEpilogue()
    ret

# Move $sp down enough to contain an instance of the given struct, ensuring proper alignment.
macro allocaStruct(structName, temp)
    move constexpr(sizeof(PinballFulfillContext)), temp
    addp StackAlignmentMask, temp
    andp ~StackAlignmentMask, temp
    subp temp, sp
end

# a0 = globalObject, a1 = callFrame
global _enterWebAssemblySuspendingFunction
_enterWebAssemblySuspendingFunction:
    functionPrologue()
    # allocate the callee saves buffer and save them
    subp 40 * 8, sp # FIXME: use the actual platform number of callee saves
    copyCalleeSavesToBuffer(sp)
    move sp, a2

    call _runWebAssemblySuspendingFunction

    restoreCalleeSavesFromBuffer(sp)
    addp 40 * 8, sp
    functionEpilogue()
    ret

# a0 = globalObject, a1 = callFrame
global  _pinballHandlerFulfillFunction
_pinballHandlerFulfillFunction:
    # This is the host function implementing the fulfilling PinballHandler.
    # Because it needs to engage in invasive stack surgery and register juggling, the core logic is implemented
    # here in assembly, with calls to C++ functions implementing higher-level "normal" logic.
    # Execution state is kept on the stack as a struct PinballFulfillContext, and SP always points at it.
    # Key invariant: SP is stable and not perturbed by any calls we make.
    functionPrologue()
    allocaStruct(PinballFulfillContext, t2)
    # Save all callee saves - wasm will trample over them all
    leap PinballFulfillContext::cppCalleeSaves[sp], t2
    copyCalleeSavesToBuffer(t2)
    move sp, a2 # additional arg to the init call: the context pointer
    call _pinballHandlerFulfillFunctionInit
    # Restore callee saves expected by Wasm
    loadp PinballFulfillContext::evacuatedCalleeSaves[sp], t0
    restoreCalleeSavesFromBuffer(t0)

.execute:
    move sp, a0
    # Push two nulls above the future sentinel frame record in place of callee/codeBlock,
    # so StackVisitor can enter the sentinel without choking. This avoids extra complexity in StackSlicer.
    subp 16, sp
    storep 0, [sp]
    storep 0, 8[sp]
    call .execute_evacuated_slice
    addp 16, sp
    # Capture all return registers to later pass them on to the next slice. For now we just save all argument registers.
    forEachWasmArgumentGPR(macro(index, reg1, reg2)
        if ARM64 or ARM64E
            storepairq reg1, reg2, index * MachineRegisterSize + PinballFulfillContext::arguments[sp]
        elsif JSVALUE64
            storeq reg1, (index + 0) * MachineRegisterSize + PinballFulfillContext::arguments[sp]
            storeq reg2, (index + 1) * MachineRegisterSize + PinballFulfillContext::arguments[sp]
        else
            store2ia reg1, reg2, index * MachineRegisterSize + PinballFulfillContext::arguments[sp]
        end
    end)
    forEachWasmArgumentFPR(macro(index, reg1, reg2)
        if ARM64 or ARM64E
            storepaird reg1, reg2, NumberOfWasmArgumentGPRs * MachineRegisterSize + index * FPRRegisterSize + PinballFulfillContext::arguments[sp]
        else
            stored reg1, NumberOfWasmArgumentGPRs * MachineRegisterSize + (index + 0) * FPRRegisterSize + PinballFulfillContext::arguments[sp]
            stored reg2, NumberOfWasmArgumentGPRs * MachineRegisterSize + (index + 1) * FPRRegisterSize + PinballFulfillContext::arguments[sp]
        end
    end)
    # Examine the outcome of last slice execution and proceed accordingly
    move sp, a0
    call _pinballHandlerFulfillFunctionContinue
    btinz r0, .execute

    # Done. At this point the context destructor already executed, but it's still on the stack
    # and plain old data in its callee saves array is okay to use.
    leap PinballFulfillContext::cppCalleeSaves[sp], t5
    restoreCalleeSavesFromBuffer(t5)
    loadp PinballFulfillContext::arguments[sp], r0
    move cfr, sp
    functionEpilogue()
    ret

.execute_evacuated_slice:
    # FIXME: sentinel frame should be enhanced to become a proper VM entry frame, registered with the VM as the top entry frame.
    #
    # Perform stack surgery to implant the frames from the slice currently in the context onto the execution stack
    # and kick off their execution. The function sets up multiple frames on the stack (shown in square brackets):
    #
    #      ( caller )
    #   [ sentinel     ]
    #   [ Wasm Frame N ]
    #         ...
    #   [ Wasm Frame 0 ]
    #   [ return frame ]
    #
    # The sentinel frame ensures that SP is reset to its original value when returning from the implanted Wasm frames.
    # That is essential because the caller expects SP to be stable. Wasm slices include additional "headroom" slots
    # above the topmost frame record and returning from them does not by itself reset SP to the bottom of the caller.
    #
    # After all the surgery, the 'ret' at the end of this function does not return to the caller, it returns to Wasm Frame 0.
    # Prior to that return we must load argument registers with values expected by the slice.
    # After all Wasm frame have executed, Wasm Frame N returns to the sentinel which then returns to
    # the caller (pinballHandlerFulfillFunction).

    # construct the sentinel frame
    functionPrologue()
    # Complete the initialization of the active JSPIContext by making the sentinel its limit.
    leap PinballFulfillContext::jspiContext[a0], t5
    storep cfr, JSPIContext::limitFrame[t5]
    # Allocate stack space for the frames to implant and prepare the 4 arguments of the implant call.
    # a0 = context, already in the register
    loadp PinballFulfillContext::sliceByteSize[a0], t5
    subp t5, sp # sliceByteSize is always stack-aligned by construction
    move sp, a1 # a1 = implantation base
    move cfr, a2 # a2 = returnFP (the sentinel frame)
    if ARM64 or ARM64E
        pcrtoaddr _exit_implanted_slice, a3 # a3 = returnPC
    else
        leap _exit_implanted_slice, a3
    end
    # Preserve on the stack the arguments buffer ptr (plus another value to pad it to 16 bytes)
    leap PinballFulfillContext::arguments[a0], t5
    push t5, a0
    # Create the return frame
    functionPrologue()
    # Create the implanted Wasm frames in the allocated space.
    call _pinballHandlerFulfillFunctionImplant
    # The implant function returns the FP and the return PC of Wasm Frame 0 as r0 and r1.
    # Use these values to link the return frame to return into Wasm Frame 0.
    # r1 should be signed with the address of the slot just above the current frame record,
    # which incidentally also holds the pointer to load argument registers from.
    move cfr, t5
    addp 2 * MachineRegisterSize, t5
    tagCodePtr r1, t5
    if ARM64 or ARM64E
        storepairq r0, r1, [cfr]
    else
        storep r0, [cfr]
        storep r1, MachineRegisterSize[cfr]
    end
    # Restore the arguments buffer ptr, load the return/argument registers and return into Wasm Frame 0.
    loadp [t5], sc0 # sc0 = arguments buffer
    forEachWasmArgumentGPR(macro(index, gpr1, gpr2)
        if ARM64 or ARM64E
            loadpairq index * MachineRegisterSize[sc0], gpr1, gpr2
        elsif JSVALUE64
            loadq (index + 0) * MachineRegisterSize[sc0], gpr1
            loadq (index + 1) * MachineRegisterSize[sc0], gpr2
        else
            load2ia index * MachineRegisterSize[sc0], gpr1, gpr2
        end
    end)
    forEachWasmArgumentFPR(macro(index, fpr1, fpr2)
        if ARM64 or ARM64E
            loadpaird NumberOfWasmArgumentGPRs * MachineRegisterSize + index * FPRRegisterSize[sc0], fpr1, fpr2
        else
            loadd NumberOfWasmArgumentGPRs * MachineRegisterSize + (index + 0) * FPRRegisterSize[sc0], fpr1
            loadd NumberOfWasmArgumentGPRs * MachineRegisterSize + (index + 1) * FPRRegisterSize[sc0], fpr2
        end
    end)
    functionEpilogue()
    ret

op(exit_implanted_slice, macro()
    move cfr, sp
    functionEpilogue()
    ret
end)

# EncodedJSValue pinballHandlerPropagateException(JSGlobalObject*, PinballHandler*, JSWebAssemblyException*);
global _pinballHandlerPropagateException
_pinballHandlerPropagateException:
    functionPrologue()
    allocaStruct(PinballUnwindContext, t5)
    # Save all callee saves - wasm will trample over them all
    leap PinballUnwindContext::cppCalleeSaves[sp], t5
    copyCalleeSavesToBuffer(t5)
    move sp, a3 # additional arg to the init call: the context pointer
    call _pinballHandlerUnwindInit
    # Restore callee saves expected by Wasm
    loadp PinballUnwindContext::evacuatedCalleeSaves[sp], t0
    restoreCalleeSavesFromBuffer(t0)
    # from this point on, wasmInstance is valid
    move sp, a0
    move wasmInstance, a1
    call _pinballHandlerUnwindInitWasm

    move sp, a0
    loadp PinballUnwindContext::zombieFrameCallee[sp], sc0
    subp 32, sp
    storep 0, [sp]
    storep sc0, 8[sp]
    # preserve sc0 past this point until another store in .unwind_evacuated_slice
    call .unwind_evacuated_slice
    addp 32, sp

    move cfr, sp
    functionEpilogue()
    ret

.unwind_evacuated_slice:
    functionPrologue()
    # Allocate stack space for the frames to implant and prepare the 4 arguments of the implant call.
    # a0 = context, already in the register
    loadp PinballUnwindContext::sliceByteSize[a0], t5
    subp t5, sp # sliceByteSize is always stack-aligned by construction
    move sp, a1 # a1 = implantation base
    move cfr, a2 # a2 = returnFP (the sentinel frame)
    if ARM64 or ARM64E
        pcrtoaddr _exit_implanted_slice, a3 # a3 = returnPC
    else
        leap _exit_implanted_slice, a3
    end
    # Create a fake throwing frame, with a zombie callee and a null codeblock
    subp 32, sp
    storep 0, [sp]
    storep sc0, 8[sp]
    functionPrologue()
    push a0, a0 # we'll need the context again later
    # Create the implanted Wasm frames in the allocated space.
    call _pinballHandlerUnwindImplant

    move cfr, t5
    addp 2 * MachineRegisterSize, t5
    tagCodePtr r1, t5
    if ARM64 or ARM64E
        storepairq r0, r1, [cfr]
    else
        storep r0, [cfr]
        storep r1, MachineRegisterSize[cfr]
    end

    # CLARIFY: this makes it not collect the stack trace, but should we?
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    storep 0, VM::topCallFrame[t5]

    pop a0, a1 # a0 = context
    # loadi PinballUnwindContext::exceptionIndex[a0], a3
    # move wasmInstance, a0
    # move cfr, a1
    # move sp, a2

    loadp PinballUnwindContext::exception[a0], t4
    storep t4, VM::m_exception[t5]

    jmp _wasm_unwind_from_slow_path_trampoline
    # operationCall(macro() cCall4(_ipint_extern_throw_exception) end)
    # jumpToException()


#################################
# 5. Instruction implementation #
#################################

if JSVALUE64 and (ARM64 or ARM64E or X86_64)
    include InPlaceInterpreter64
elsif ARMv7
    include InPlaceInterpreter32_64
else
# For unimplemented architectures: make sure that the assertions can still find the labels
# See https://webassembly.github.io/spec/core/appendix/index-instructions.html for the list of instructions.

unimplementedInstruction(_unreachable)
unimplementedInstruction(_nop)
unimplementedInstruction(_block)
unimplementedInstruction(_loop)
unimplementedInstruction(_if)
unimplementedInstruction(_else)
unimplementedInstruction(_try)
unimplementedInstruction(_catch)
unimplementedInstruction(_throw)
unimplementedInstruction(_rethrow)
reservedOpcode(0xa)
unimplementedInstruction(_end)
unimplementedInstruction(_br)
unimplementedInstruction(_br_if)
unimplementedInstruction(_br_table)
unimplementedInstruction(_return)
unimplementedInstruction(_call)
unimplementedInstruction(_call_indirect)
reservedOpcode(0x12)
reservedOpcode(0x13)
reservedOpcode(0x14)
reservedOpcode(0x15)
reservedOpcode(0x16)
reservedOpcode(0x17)
unimplementedInstruction(_delegate)
unimplementedInstruction(_catch_all)
unimplementedInstruction(_drop)
unimplementedInstruction(_select)
unimplementedInstruction(_select_t)
reservedOpcode(0x1d)
reservedOpcode(0x1e)
reservedOpcode(0x1f)
unimplementedInstruction(_local_get)
unimplementedInstruction(_local_set)
unimplementedInstruction(_local_tee)
unimplementedInstruction(_global_get)
unimplementedInstruction(_global_set)
unimplementedInstruction(_table_get)
unimplementedInstruction(_table_set)
reservedOpcode(0x27)
unimplementedInstruction(_i32_load_mem)
unimplementedInstruction(_i64_load_mem)
unimplementedInstruction(_f32_load_mem)
unimplementedInstruction(_f64_load_mem)
unimplementedInstruction(_i32_load8s_mem)
unimplementedInstruction(_i32_load8u_mem)
unimplementedInstruction(_i32_load16s_mem)
unimplementedInstruction(_i32_load16u_mem)
unimplementedInstruction(_i64_load8s_mem)
unimplementedInstruction(_i64_load8u_mem)
unimplementedInstruction(_i64_load16s_mem)
unimplementedInstruction(_i64_load16u_mem)
unimplementedInstruction(_i64_load32s_mem)
unimplementedInstruction(_i64_load32u_mem)
unimplementedInstruction(_i32_store_mem)
unimplementedInstruction(_i64_store_mem)
unimplementedInstruction(_f32_store_mem)
unimplementedInstruction(_f64_store_mem)
unimplementedInstruction(_i32_store8_mem)
unimplementedInstruction(_i32_store16_mem)
unimplementedInstruction(_i64_store8_mem)
unimplementedInstruction(_i64_store16_mem)
unimplementedInstruction(_i64_store32_mem)
unimplementedInstruction(_memory_size)
unimplementedInstruction(_memory_grow)
unimplementedInstruction(_i32_const)
unimplementedInstruction(_i64_const)
unimplementedInstruction(_f32_const)
unimplementedInstruction(_f64_const)
unimplementedInstruction(_i32_eqz)
unimplementedInstruction(_i32_eq)
unimplementedInstruction(_i32_ne)
unimplementedInstruction(_i32_lt_s)
unimplementedInstruction(_i32_lt_u)
unimplementedInstruction(_i32_gt_s)
unimplementedInstruction(_i32_gt_u)
unimplementedInstruction(_i32_le_s)
unimplementedInstruction(_i32_le_u)
unimplementedInstruction(_i32_ge_s)
unimplementedInstruction(_i32_ge_u)
unimplementedInstruction(_i64_eqz)
unimplementedInstruction(_i64_eq)
unimplementedInstruction(_i64_ne)
unimplementedInstruction(_i64_lt_s)
unimplementedInstruction(_i64_lt_u)
unimplementedInstruction(_i64_gt_s)
unimplementedInstruction(_i64_gt_u)
unimplementedInstruction(_i64_le_s)
unimplementedInstruction(_i64_le_u)
unimplementedInstruction(_i64_ge_s)
unimplementedInstruction(_i64_ge_u)
unimplementedInstruction(_f32_eq)
unimplementedInstruction(_f32_ne)
unimplementedInstruction(_f32_lt)
unimplementedInstruction(_f32_gt)
unimplementedInstruction(_f32_le)
unimplementedInstruction(_f32_ge)
unimplementedInstruction(_f64_eq)
unimplementedInstruction(_f64_ne)
unimplementedInstruction(_f64_lt)
unimplementedInstruction(_f64_gt)
unimplementedInstruction(_f64_le)
unimplementedInstruction(_f64_ge)
unimplementedInstruction(_i32_clz)
unimplementedInstruction(_i32_ctz)
unimplementedInstruction(_i32_popcnt)
unimplementedInstruction(_i32_add)
unimplementedInstruction(_i32_sub)
unimplementedInstruction(_i32_mul)
unimplementedInstruction(_i32_div_s)
unimplementedInstruction(_i32_div_u)
unimplementedInstruction(_i32_rem_s)
unimplementedInstruction(_i32_rem_u)
unimplementedInstruction(_i32_and)
unimplementedInstruction(_i32_or)
unimplementedInstruction(_i32_xor)
unimplementedInstruction(_i32_shl)
unimplementedInstruction(_i32_shr_s)
unimplementedInstruction(_i32_shr_u)
unimplementedInstruction(_i32_rotl)
unimplementedInstruction(_i32_rotr)
unimplementedInstruction(_i64_clz)
unimplementedInstruction(_i64_ctz)
unimplementedInstruction(_i64_popcnt)
unimplementedInstruction(_i64_add)
unimplementedInstruction(_i64_sub)
unimplementedInstruction(_i64_mul)
unimplementedInstruction(_i64_div_s)
unimplementedInstruction(_i64_div_u)
unimplementedInstruction(_i64_rem_s)
unimplementedInstruction(_i64_rem_u)
unimplementedInstruction(_i64_and)
unimplementedInstruction(_i64_or)
unimplementedInstruction(_i64_xor)
unimplementedInstruction(_i64_shl)
unimplementedInstruction(_i64_shr_s)
unimplementedInstruction(_i64_shr_u)
unimplementedInstruction(_i64_rotl)
unimplementedInstruction(_i64_rotr)
unimplementedInstruction(_f32_abs)
unimplementedInstruction(_f32_neg)
unimplementedInstruction(_f32_ceil)
unimplementedInstruction(_f32_floor)
unimplementedInstruction(_f32_trunc)
unimplementedInstruction(_f32_nearest)
unimplementedInstruction(_f32_sqrt)
unimplementedInstruction(_f32_add)
unimplementedInstruction(_f32_sub)
unimplementedInstruction(_f32_mul)
unimplementedInstruction(_f32_div)
unimplementedInstruction(_f32_min)
unimplementedInstruction(_f32_max)
unimplementedInstruction(_f32_copysign)
unimplementedInstruction(_f64_abs)
unimplementedInstruction(_f64_neg)
unimplementedInstruction(_f64_ceil)
unimplementedInstruction(_f64_floor)
unimplementedInstruction(_f64_trunc)
unimplementedInstruction(_f64_nearest)
unimplementedInstruction(_f64_sqrt)
unimplementedInstruction(_f64_add)
unimplementedInstruction(_f64_sub)
unimplementedInstruction(_f64_mul)
unimplementedInstruction(_f64_div)
unimplementedInstruction(_f64_min)
unimplementedInstruction(_f64_max)
unimplementedInstruction(_f64_copysign)
unimplementedInstruction(_i32_wrap_i64)
unimplementedInstruction(_i32_trunc_f32_s)
unimplementedInstruction(_i32_trunc_f32_u)
unimplementedInstruction(_i32_trunc_f64_s)
unimplementedInstruction(_i32_trunc_f64_u)
unimplementedInstruction(_i64_extend_i32_s)
unimplementedInstruction(_i64_extend_i32_u)
unimplementedInstruction(_i64_trunc_f32_s)
unimplementedInstruction(_i64_trunc_f32_u)
unimplementedInstruction(_i64_trunc_f64_s)
unimplementedInstruction(_i64_trunc_f64_u)
unimplementedInstruction(_f32_convert_i32_s)
unimplementedInstruction(_f32_convert_i32_u)
unimplementedInstruction(_f32_convert_i64_s)
unimplementedInstruction(_f32_convert_i64_u)
unimplementedInstruction(_f32_demote_f64)
unimplementedInstruction(_f64_convert_i32_s)
unimplementedInstruction(_f64_convert_i32_u)
unimplementedInstruction(_f64_convert_i64_s)
unimplementedInstruction(_f64_convert_i64_u)
unimplementedInstruction(_f64_promote_f32)
unimplementedInstruction(_i32_reinterpret_f32)
unimplementedInstruction(_i64_reinterpret_f64)
unimplementedInstruction(_f32_reinterpret_i32)
unimplementedInstruction(_f64_reinterpret_i64)
unimplementedInstruction(_i32_extend8_s)
unimplementedInstruction(_i32_extend16_s)
unimplementedInstruction(_i64_extend8_s)
unimplementedInstruction(_i64_extend16_s)
unimplementedInstruction(_i64_extend32_s)
reservedOpcode(0xc5)
reservedOpcode(0xc6)
reservedOpcode(0xc7)
reservedOpcode(0xc8)
reservedOpcode(0xc9)
reservedOpcode(0xca)
reservedOpcode(0xcb)
reservedOpcode(0xcc)
reservedOpcode(0xcd)
reservedOpcode(0xce)
reservedOpcode(0xcf)
unimplementedInstruction(_ref_null_t)
unimplementedInstruction(_ref_is_null)
unimplementedInstruction(_ref_func)
unimplementedInstruction(_ref_eq)
unimplementedInstruction(_ref_as_non_null)
unimplementedInstruction(_br_on_null)
unimplementedInstruction(_br_on_non_null)
reservedOpcode(0xd7)
reservedOpcode(0xd8)
reservedOpcode(0xd9)
reservedOpcode(0xda)
reservedOpcode(0xdb)
reservedOpcode(0xdc)
reservedOpcode(0xdd)
reservedOpcode(0xde)
reservedOpcode(0xdf)
reservedOpcode(0xe0)
reservedOpcode(0xe1)
reservedOpcode(0xe2)
reservedOpcode(0xe3)
reservedOpcode(0xe4)
reservedOpcode(0xe5)
reservedOpcode(0xe6)
reservedOpcode(0xe7)
reservedOpcode(0xe8)
reservedOpcode(0xe9)
reservedOpcode(0xea)
reservedOpcode(0xeb)
reservedOpcode(0xec)
reservedOpcode(0xed)
reservedOpcode(0xee)
reservedOpcode(0xef)
reservedOpcode(0xf0)
reservedOpcode(0xf1)
reservedOpcode(0xf2)
reservedOpcode(0xf3)
reservedOpcode(0xf4)
reservedOpcode(0xf5)
reservedOpcode(0xf6)
reservedOpcode(0xf7)
reservedOpcode(0xf8)
reservedOpcode(0xf9)
reservedOpcode(0xfa)
unimplementedInstruction(_fb_block)
unimplementedInstruction(_fc_block)
unimplementedInstruction(_simd)
unimplementedInstruction(_atomic)
reservedOpcode(0xff)

    #######################
    ## 0xfc instructions ##
    #######################

unimplementedInstruction(_i32_trunc_sat_f32_s)
unimplementedInstruction(_i32_trunc_sat_f32_u)
unimplementedInstruction(_i32_trunc_sat_f64_s)
unimplementedInstruction(_i32_trunc_sat_f64_u)
unimplementedInstruction(_i64_trunc_sat_f32_s)
unimplementedInstruction(_i64_trunc_sat_f32_u)
unimplementedInstruction(_i64_trunc_sat_f64_s)
unimplementedInstruction(_i64_trunc_sat_f64_u)
unimplementedInstruction(_memory_init)
unimplementedInstruction(_data_drop)
unimplementedInstruction(_memory_copy)
unimplementedInstruction(_memory_fill)
unimplementedInstruction(_table_init)
unimplementedInstruction(_elem_drop)
unimplementedInstruction(_table_copy)
unimplementedInstruction(_table_grow)
unimplementedInstruction(_table_size)
unimplementedInstruction(_table_fill)

    #######################
    ## SIMD Instructions ##
    #######################

unimplementedInstruction(_simd_v128_load_mem)
unimplementedInstruction(_simd_v128_load_8x8s_mem)
unimplementedInstruction(_simd_v128_load_8x8u_mem)
unimplementedInstruction(_simd_v128_load_16x4s_mem)
unimplementedInstruction(_simd_v128_load_16x4u_mem)
unimplementedInstruction(_simd_v128_load_32x2s_mem)
unimplementedInstruction(_simd_v128_load_32x2u_mem)
unimplementedInstruction(_simd_v128_load8_splat_mem)
unimplementedInstruction(_simd_v128_load16_splat_mem)
unimplementedInstruction(_simd_v128_load32_splat_mem)
unimplementedInstruction(_simd_v128_load64_splat_mem)
unimplementedInstruction(_simd_v128_store_mem)
unimplementedInstruction(_simd_v128_const)
unimplementedInstruction(_simd_i8x16_shuffle)
unimplementedInstruction(_simd_i8x16_swizzle)
unimplementedInstruction(_simd_i8x16_splat)
unimplementedInstruction(_simd_i16x8_splat)
unimplementedInstruction(_simd_i32x4_splat)
unimplementedInstruction(_simd_i64x2_splat)
unimplementedInstruction(_simd_f32x4_splat)
unimplementedInstruction(_simd_f64x2_splat)
unimplementedInstruction(_simd_i8x16_extract_lane_s)
unimplementedInstruction(_simd_i8x16_extract_lane_u)
unimplementedInstruction(_simd_i8x16_replace_lane)
unimplementedInstruction(_simd_i16x8_extract_lane_s)
unimplementedInstruction(_simd_i16x8_extract_lane_u)
unimplementedInstruction(_simd_i16x8_replace_lane)
unimplementedInstruction(_simd_i32x4_extract_lane)
unimplementedInstruction(_simd_i32x4_replace_lane)
unimplementedInstruction(_simd_i64x2_extract_lane)
unimplementedInstruction(_simd_i64x2_replace_lane)
unimplementedInstruction(_simd_f32x4_extract_lane)
unimplementedInstruction(_simd_f32x4_replace_lane)
unimplementedInstruction(_simd_f64x2_extract_lane)
unimplementedInstruction(_simd_f64x2_replace_lane)
unimplementedInstruction(_simd_i8x16_eq)
unimplementedInstruction(_simd_i8x16_ne)
unimplementedInstruction(_simd_i8x16_lt_s)
unimplementedInstruction(_simd_i8x16_lt_u)
unimplementedInstruction(_simd_i8x16_gt_s)
unimplementedInstruction(_simd_i8x16_gt_u)
unimplementedInstruction(_simd_i8x16_le_s)
unimplementedInstruction(_simd_i8x16_le_u)
unimplementedInstruction(_simd_i8x16_ge_s)
unimplementedInstruction(_simd_i8x16_ge_u)
unimplementedInstruction(_simd_i16x8_eq)
unimplementedInstruction(_simd_i16x8_ne)
unimplementedInstruction(_simd_i16x8_lt_s)
unimplementedInstruction(_simd_i16x8_lt_u)
unimplementedInstruction(_simd_i16x8_gt_s)
unimplementedInstruction(_simd_i16x8_gt_u)
unimplementedInstruction(_simd_i16x8_le_s)
unimplementedInstruction(_simd_i16x8_le_u)
unimplementedInstruction(_simd_i16x8_ge_s)
unimplementedInstruction(_simd_i16x8_ge_u)
unimplementedInstruction(_simd_i32x4_eq)
unimplementedInstruction(_simd_i32x4_ne)
unimplementedInstruction(_simd_i32x4_lt_s)
unimplementedInstruction(_simd_i32x4_lt_u)
unimplementedInstruction(_simd_i32x4_gt_s)
unimplementedInstruction(_simd_i32x4_gt_u)
unimplementedInstruction(_simd_i32x4_le_s)
unimplementedInstruction(_simd_i32x4_le_u)
unimplementedInstruction(_simd_i32x4_ge_s)
unimplementedInstruction(_simd_i32x4_ge_u)
unimplementedInstruction(_simd_f32x4_eq)
unimplementedInstruction(_simd_f32x4_ne)
unimplementedInstruction(_simd_f32x4_lt)
unimplementedInstruction(_simd_f32x4_gt)
unimplementedInstruction(_simd_f32x4_le)
unimplementedInstruction(_simd_f32x4_ge)
unimplementedInstruction(_simd_f64x2_eq)
unimplementedInstruction(_simd_f64x2_ne)
unimplementedInstruction(_simd_f64x2_lt)
unimplementedInstruction(_simd_f64x2_gt)
unimplementedInstruction(_simd_f64x2_le)
unimplementedInstruction(_simd_f64x2_ge)
unimplementedInstruction(_simd_v128_not)
unimplementedInstruction(_simd_v128_and)
unimplementedInstruction(_simd_v128_andnot)
unimplementedInstruction(_simd_v128_or)
unimplementedInstruction(_simd_v128_xor)
unimplementedInstruction(_simd_v128_bitselect)
unimplementedInstruction(_simd_v128_any_true)
unimplementedInstruction(_simd_v128_load8_lane_mem)
unimplementedInstruction(_simd_v128_load16_lane_mem)
unimplementedInstruction(_simd_v128_load32_lane_mem)
unimplementedInstruction(_simd_v128_load64_lane_mem)
unimplementedInstruction(_simd_v128_store8_lane_mem)
unimplementedInstruction(_simd_v128_store16_lane_mem)
unimplementedInstruction(_simd_v128_store32_lane_mem)
unimplementedInstruction(_simd_v128_store64_lane_mem)
unimplementedInstruction(_simd_v128_load32_zero_mem)
unimplementedInstruction(_simd_v128_load64_zero_mem)
unimplementedInstruction(_simd_f32x4_demote_f64x2_zero)
unimplementedInstruction(_simd_f64x2_promote_low_f32x4)
unimplementedInstruction(_simd_i8x16_abs)
unimplementedInstruction(_simd_i8x16_neg)
unimplementedInstruction(_simd_i8x16_popcnt)
unimplementedInstruction(_simd_i8x16_all_true)
unimplementedInstruction(_simd_i8x16_bitmask)
unimplementedInstruction(_simd_i8x16_narrow_i16x8_s)
unimplementedInstruction(_simd_i8x16_narrow_i16x8_u)
unimplementedInstruction(_simd_f32x4_ceil)
unimplementedInstruction(_simd_f32x4_floor)
unimplementedInstruction(_simd_f32x4_trunc)
unimplementedInstruction(_simd_f32x4_nearest)
unimplementedInstruction(_simd_i8x16_shl)
unimplementedInstruction(_simd_i8x16_shr_s)
unimplementedInstruction(_simd_i8x16_shr_u)
unimplementedInstruction(_simd_i8x16_add)
unimplementedInstruction(_simd_i8x16_add_sat_s)
unimplementedInstruction(_simd_i8x16_add_sat_u)
unimplementedInstruction(_simd_i8x16_sub)
unimplementedInstruction(_simd_i8x16_sub_sat_s)
unimplementedInstruction(_simd_i8x16_sub_sat_u)
unimplementedInstruction(_simd_f64x2_ceil)
unimplementedInstruction(_simd_f64x2_floor)
unimplementedInstruction(_simd_i8x16_min_s)
unimplementedInstruction(_simd_i8x16_min_u)
unimplementedInstruction(_simd_i8x16_max_s)
unimplementedInstruction(_simd_i8x16_max_u)
unimplementedInstruction(_simd_f64x2_trunc)
unimplementedInstruction(_simd_i8x16_avgr_u)
unimplementedInstruction(_simd_i16x8_extadd_pairwise_i8x16_s)
unimplementedInstruction(_simd_i16x8_extadd_pairwise_i8x16_u)
unimplementedInstruction(_simd_i32x4_extadd_pairwise_i16x8_s)
unimplementedInstruction(_simd_i32x4_extadd_pairwise_i16x8_u)
unimplementedInstruction(_simd_i16x8_abs)
unimplementedInstruction(_simd_i16x8_neg)
unimplementedInstruction(_simd_i16x8_q15mulr_sat_s)
unimplementedInstruction(_simd_i16x8_all_true)
unimplementedInstruction(_simd_i16x8_bitmask)
unimplementedInstruction(_simd_i16x8_narrow_i32x4_s)
unimplementedInstruction(_simd_i16x8_narrow_i32x4_u)
unimplementedInstruction(_simd_i16x8_extend_low_i8x16_s)
unimplementedInstruction(_simd_i16x8_extend_high_i8x16_s)
unimplementedInstruction(_simd_i16x8_extend_low_i8x16_u)
unimplementedInstruction(_simd_i16x8_extend_high_i8x16_u)
unimplementedInstruction(_simd_i16x8_shl)
unimplementedInstruction(_simd_i16x8_shr_s)
unimplementedInstruction(_simd_i16x8_shr_u)
unimplementedInstruction(_simd_i16x8_add)
unimplementedInstruction(_simd_i16x8_add_sat_s)
unimplementedInstruction(_simd_i16x8_add_sat_u)
unimplementedInstruction(_simd_i16x8_sub)
unimplementedInstruction(_simd_i16x8_sub_sat_s)
unimplementedInstruction(_simd_i16x8_sub_sat_u)
unimplementedInstruction(_simd_f64x2_nearest)
unimplementedInstruction(_simd_i16x8_mul)
unimplementedInstruction(_simd_i16x8_min_s)
unimplementedInstruction(_simd_i16x8_min_u)
unimplementedInstruction(_simd_i16x8_max_s)
unimplementedInstruction(_simd_i16x8_max_u)
reservedOpcode(0xfd9a01)
unimplementedInstruction(_simd_i16x8_avgr_u)
unimplementedInstruction(_simd_i16x8_extmul_low_i8x16_s)
unimplementedInstruction(_simd_i16x8_extmul_high_i8x16_s)
unimplementedInstruction(_simd_i16x8_extmul_low_i8x16_u)
unimplementedInstruction(_simd_i16x8_extmul_high_i8x16_u)
unimplementedInstruction(_simd_i32x4_abs)
unimplementedInstruction(_simd_i32x4_neg)
reservedOpcode(0xfda201)
unimplementedInstruction(_simd_i32x4_all_true)
unimplementedInstruction(_simd_i32x4_bitmask)
reservedOpcode(0xfda501)
reservedOpcode(0xfda601)
unimplementedInstruction(_simd_i32x4_extend_low_i16x8_s)
unimplementedInstruction(_simd_i32x4_extend_high_i16x8_s)
unimplementedInstruction(_simd_i32x4_extend_low_i16x8_u)
unimplementedInstruction(_simd_i32x4_extend_high_i16x8_u)
unimplementedInstruction(_simd_i32x4_shl)
unimplementedInstruction(_simd_i32x4_shr_s)
unimplementedInstruction(_simd_i32x4_shr_u)
unimplementedInstruction(_simd_i32x4_add)
reservedOpcode(0xfdaf01)
reservedOpcode(0xfdb001)
unimplementedInstruction(_simd_i32x4_sub)
reservedOpcode(0xfdb201)
reservedOpcode(0xfdb301)
reservedOpcode(0xfdb401)
unimplementedInstruction(_simd_i32x4_mul)
unimplementedInstruction(_simd_i32x4_min_s)
unimplementedInstruction(_simd_i32x4_min_u)
unimplementedInstruction(_simd_i32x4_max_s)
unimplementedInstruction(_simd_i32x4_max_u)
unimplementedInstruction(_simd_i32x4_dot_i16x8_s)
reservedOpcode(0xfdbb01)
unimplementedInstruction(_simd_i32x4_extmul_low_i16x8_s)
unimplementedInstruction(_simd_i32x4_extmul_high_i16x8_s)
unimplementedInstruction(_simd_i32x4_extmul_low_i16x8_u)
unimplementedInstruction(_simd_i32x4_extmul_high_i16x8_u)
unimplementedInstruction(_simd_i64x2_abs)
unimplementedInstruction(_simd_i64x2_neg)
reservedOpcode(0xfdc201)
unimplementedInstruction(_simd_i64x2_all_true)
unimplementedInstruction(_simd_i64x2_bitmask)
reservedOpcode(0xfdc501)
reservedOpcode(0xfdc601)
unimplementedInstruction(_simd_i64x2_extend_low_i32x4_s)
unimplementedInstruction(_simd_i64x2_extend_high_i32x4_s)
unimplementedInstruction(_simd_i64x2_extend_low_i32x4_u)
unimplementedInstruction(_simd_i64x2_extend_high_i32x4_u)
unimplementedInstruction(_simd_i64x2_shl)
unimplementedInstruction(_simd_i64x2_shr_s)
unimplementedInstruction(_simd_i64x2_shr_u)
unimplementedInstruction(_simd_i64x2_add)
reservedOpcode(0xfdcf01)
reservedOpcode(0xfdd001)
unimplementedInstruction(_simd_i64x2_sub)
reservedOpcode(0xfdd201)
reservedOpcode(0xfdd301)
reservedOpcode(0xfdd401)
unimplementedInstruction(_simd_i64x2_mul)
unimplementedInstruction(_simd_i64x2_eq)
unimplementedInstruction(_simd_i64x2_ne)
unimplementedInstruction(_simd_i64x2_lt_s)
unimplementedInstruction(_simd_i64x2_gt_s)
unimplementedInstruction(_simd_i64x2_le_s)
unimplementedInstruction(_simd_i64x2_ge_s)
unimplementedInstruction(_simd_i64x2_extmul_low_i32x4_s)
unimplementedInstruction(_simd_i64x2_extmul_high_i32x4_s)
unimplementedInstruction(_simd_i64x2_extmul_low_i32x4_u)
unimplementedInstruction(_simd_i64x2_extmul_high_i32x4_u)
unimplementedInstruction(_simd_f32x4_abs)
unimplementedInstruction(_simd_f32x4_neg)
reservedOpcode(0xfde201)
unimplementedInstruction(_simd_f32x4_sqrt)
unimplementedInstruction(_simd_f32x4_add)
unimplementedInstruction(_simd_f32x4_sub)
unimplementedInstruction(_simd_f32x4_mul)
unimplementedInstruction(_simd_f32x4_div)
unimplementedInstruction(_simd_f32x4_min)
unimplementedInstruction(_simd_f32x4_max)
unimplementedInstruction(_simd_f32x4_pmin)
unimplementedInstruction(_simd_f32x4_pmax)
unimplementedInstruction(_simd_f64x2_abs)
unimplementedInstruction(_simd_f64x2_neg)
reservedOpcode(0xfdee01)
unimplementedInstruction(_simd_f64x2_sqrt)
unimplementedInstruction(_simd_f64x2_add)
unimplementedInstruction(_simd_f64x2_sub)
unimplementedInstruction(_simd_f64x2_mul)
unimplementedInstruction(_simd_f64x2_div)
unimplementedInstruction(_simd_f64x2_min)
unimplementedInstruction(_simd_f64x2_max)
unimplementedInstruction(_simd_f64x2_pmin)
unimplementedInstruction(_simd_f64x2_pmax)
unimplementedInstruction(_simd_i32x4_trunc_sat_f32x4_s)
unimplementedInstruction(_simd_i32x4_trunc_sat_f32x4_u)
unimplementedInstruction(_simd_f32x4_convert_i32x4_s)
unimplementedInstruction(_simd_f32x4_convert_i32x4_u)
unimplementedInstruction(_simd_i32x4_trunc_sat_f64x2_s_zero)
unimplementedInstruction(_simd_i32x4_trunc_sat_f64x2_u_zero)
unimplementedInstruction(_simd_f64x2_convert_low_i32x4_s)
unimplementedInstruction(_simd_f64x2_convert_low_i32x4_u)

    #########################
    ## Atomic instructions ##
    #########################

unimplementedInstruction(_memory_atomic_notify)
unimplementedInstruction(_memory_atomic_wait32)
unimplementedInstruction(_memory_atomic_wait64)
unimplementedInstruction(_atomic_fence)

reservedOpcode(atomic_0x4)
reservedOpcode(atomic_0x5)
reservedOpcode(atomic_0x6)
reservedOpcode(atomic_0x7)
reservedOpcode(atomic_0x8)
reservedOpcode(atomic_0x9)
reservedOpcode(atomic_0xa)
reservedOpcode(atomic_0xb)
reservedOpcode(atomic_0xc)
reservedOpcode(atomic_0xd)
reservedOpcode(atomic_0xe)
reservedOpcode(atomic_0xf)

unimplementedInstruction(_i32_atomic_load)
unimplementedInstruction(_i64_atomic_load)
unimplementedInstruction(_i32_atomic_load8_u)
unimplementedInstruction(_i32_atomic_load16_u)
unimplementedInstruction(_i64_atomic_load8_u)
unimplementedInstruction(_i64_atomic_load16_u)
unimplementedInstruction(_i64_atomic_load32_u)
unimplementedInstruction(_i32_atomic_store)
unimplementedInstruction(_i64_atomic_store)
unimplementedInstruction(_i32_atomic_store8_u)
unimplementedInstruction(_i32_atomic_store16_u)
unimplementedInstruction(_i64_atomic_store8_u)
unimplementedInstruction(_i64_atomic_store16_u)
unimplementedInstruction(_i64_atomic_store32_u)
unimplementedInstruction(_i32_atomic_rmw_add)
unimplementedInstruction(_i64_atomic_rmw_add)
unimplementedInstruction(_i32_atomic_rmw8_add_u)
unimplementedInstruction(_i32_atomic_rmw16_add_u)
unimplementedInstruction(_i64_atomic_rmw8_add_u)
unimplementedInstruction(_i64_atomic_rmw16_add_u)
unimplementedInstruction(_i64_atomic_rmw32_add_u)
unimplementedInstruction(_i32_atomic_rmw_sub)
unimplementedInstruction(_i64_atomic_rmw_sub)
unimplementedInstruction(_i32_atomic_rmw8_sub_u)
unimplementedInstruction(_i32_atomic_rmw16_sub_u)
unimplementedInstruction(_i64_atomic_rmw8_sub_u)
unimplementedInstruction(_i64_atomic_rmw16_sub_u)
unimplementedInstruction(_i64_atomic_rmw32_sub_u)
unimplementedInstruction(_i32_atomic_rmw_and)
unimplementedInstruction(_i64_atomic_rmw_and)
unimplementedInstruction(_i32_atomic_rmw8_and_u)
unimplementedInstruction(_i32_atomic_rmw16_and_u)
unimplementedInstruction(_i64_atomic_rmw8_and_u)
unimplementedInstruction(_i64_atomic_rmw16_and_u)
unimplementedInstruction(_i64_atomic_rmw32_and_u)
unimplementedInstruction(_i32_atomic_rmw_or)
unimplementedInstruction(_i64_atomic_rmw_or)
unimplementedInstruction(_i32_atomic_rmw8_or_u)
unimplementedInstruction(_i32_atomic_rmw16_or_u)
unimplementedInstruction(_i64_atomic_rmw8_or_u)
unimplementedInstruction(_i64_atomic_rmw16_or_u)
unimplementedInstruction(_i64_atomic_rmw32_or_u)
unimplementedInstruction(_i32_atomic_rmw_xor)
unimplementedInstruction(_i64_atomic_rmw_xor)
unimplementedInstruction(_i32_atomic_rmw8_xor_u)
unimplementedInstruction(_i32_atomic_rmw16_xor_u)
unimplementedInstruction(_i64_atomic_rmw8_xor_u)
unimplementedInstruction(_i64_atomic_rmw16_xor_u)
unimplementedInstruction(_i64_atomic_rmw32_xor_u)
unimplementedInstruction(_i32_atomic_rmw_xchg)
unimplementedInstruction(_i64_atomic_rmw_xchg)
unimplementedInstruction(_i32_atomic_rmw8_xchg_u)
unimplementedInstruction(_i32_atomic_rmw16_xchg_u)
unimplementedInstruction(_i64_atomic_rmw8_xchg_u)
unimplementedInstruction(_i64_atomic_rmw16_xchg_u)
unimplementedInstruction(_i64_atomic_rmw32_xchg_u)
unimplementedInstruction(_i32_atomic_rmw_cmpxchg)
unimplementedInstruction(_i64_atomic_rmw_cmpxchg)
unimplementedInstruction(_i32_atomic_rmw8_cmpxchg_u)
unimplementedInstruction(_i32_atomic_rmw16_cmpxchg_u)
unimplementedInstruction(_i64_atomic_rmw8_cmpxchg_u)
unimplementedInstruction(_i64_atomic_rmw16_cmpxchg_u)
unimplementedInstruction(_i64_atomic_rmw32_cmpxchg_u)
end

