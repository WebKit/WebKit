# Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#       function-specific data.
# - PL: (Pointer to Locals) pointer to the address of local 0 in the current function. This is used for accessing
#       locals quickly.
# - MB: (Memory Base) pointer to the current Wasm memory base address.
# - BC: (Bounds Check) the size of the current Wasm memory region, for bounds checking.
#
# Additionally, there are a few registers that are optionally supported for optimization:
# - IB: (Instruction Base) pointer to the address of the `unreachable` (0x00) label, removing the need to reload
#       this address at every dispatch.
# - HR: (Hoist Register) used for hoisting the load of the next opcode in Wasm instructions with low register
#       pressure, helping reduce the impact of load misses.
#
# Finally, we provide four "sc" (safe for call) registers which are guaranteed to not overlap with argument
# registers (sc0, sc1, sc2, sc3)

if ARM64 or ARM64E
    const PC = csr7
    const MC = csr6
    const WI = csr0
    const PL = t6
    const MB = csr3
    const BC = csr4

    const IB = t7
    const HR = t3

    const sc0 = ws0
    const sc1 = ws1
    const sc2 = ws2
    const sc3 = ws3
elsif X86_64
    const PC = csr2
    const MC = csr1
    const WI = csr0
    const PL = t6
    const MB = csr3
    const BC = csr4

    const IB = invalidGPR
    const HR = invalidGPR

    const sc0 = t0
    const sc1 = t4
    const sc2 = t7
    const sc3 = t5
elsif RISCV64
    const PC = csr7
    const MC = csr6
    const WI = csr0
    const PL = csr10
    const MB = csr3
    const BC = csr4

    const IB = invalidGPR
    const HR = invalidGPR

    const sc0 = ws0
    const sc1 = ws1
    const sc2 = csr9
    const sc3 = csr10
elsif ARMv7
    const PC = csr1
    const MC = t6
    const WI = csr0
    const PL = t7
    const MB = invalidGPR
    const BC = invalidGPR

    const IB = invalidGPR
    const HR = invalidGPR

    const sc0 = t4
    const sc1 = t5
    const sc2 = t6
    const sc3 = t7
else
    const PC = invalidGPR
    const MC = invalidGPR
    const WI = invalidGPR
    const PL = invalidGPR
    const MB = invalidGPR
    const BC = invalidGPR

    const IB = invalidGPR
    const HR = invalidGPR

    const sc0 = invalidGPR
    const sc1 = invalidGPR
    const sc2 = invalidGPR
    const sc3 = invalidGPR
end

# -------------------------
# 1.2: Constant definitions
# -------------------------

const PtrSize = constexpr (sizeof(void*))
const MachineRegisterSize = constexpr (sizeof(CPURegister))
const SlotSize = constexpr (sizeof(Register))

# amount of memory a local takes up on the stack (16 bytes for a v128)
const LocalSize = 16
const StackValueSize = 16

const wasmInstance = csr0
if X86_64 or ARM64 or ARM64E or RISCV64
    const memoryBase = csr3
    const boundsCheckingSize = csr4
else
    const memoryBase = invalidGPR
    const boundsCheckingSize = invalidGPR
end

const UnboxedWasmCalleeStackSlot = CallerFrame - constexpr Wasm::numberOfIPIntCalleeSaveRegisters * SlotSize - MachineRegisterSize

# FIXME: This happens to work because UnboxedWasmCalleeStackSlot sits in the extra space we should be more precise in case we want to use an even number of callee saves in the future.
const IPIntCalleeSaveSpaceAsVirtualRegisters = constexpr Wasm::numberOfIPIntCalleeSaveRegisters + constexpr Wasm::numberOfIPIntInternalRegisters
const IPIntCalleeSaveSpaceStackAligned = (IPIntCalleeSaveSpaceAsVirtualRegisters * SlotSize + StackAlignment - 1) & ~StackAlignmentMask
const IPIntCalleeSaveSpaceStackAligned = 2*IPIntCalleeSaveSpaceStackAligned

# ---------------------------
# 1.2: Optional optimizations
# ---------------------------

macro IfIPIntUsesIB(m)
    if ARM64 or ARM64E
        m()
    end
end

macro IfIPIntUsesHR(m, m2)
    if ARM64 or ARM64E
        m()
    else
        m2()
    end
end

macro HoistNextOpcode(offset)
    IfIPIntUsesHR(macro()
        loadb offset[PC], HR
    end, macro() end)
end

##############################
# 2. Core interpreter macros #
##############################

# -----------------------------------
# 2.1: Core interpreter functionality
# -----------------------------------

# Get IPIntCallee object at startup
macro getIPIntCallee()
    loadp Callee[cfr], ws0
if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0
    storep ws0, UnboxedWasmCalleeStackSlot[cfr]
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

# Typed push/pop to make code pretty
macro pushFloat32FT0()
    pushFPR()
end

macro pushFloat32FT1()
    pushFPR1()
end

macro popFloat32FT0()
    popFPR()
end

macro popFloat32FT1()
    popFPR1()
end

macro pushFloat64FT0()
    pushFPR()
end

macro pushFloat64FT1()
    pushFPR1()
end

macro popFloat64FT0()
    popFPR()
end

macro popFloat64FT1()
    popFPR1()
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
    lshiftp 4, scratch
    subp cfr, scratch, scratch

    bpa scratch, cfr, .stackOverflow
    bpbeq JSWebAssemblyInstance::m_softStackLimit[wasmInstance], scratch, .stackHeightOK

.stackOverflow:
    ipintException(StackOverflow)

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
    aligned _ipint%instrname%_validate 256
    _ipint%instrname%_validate:
    _ipint%instrname%:
end

macro slowPathLabel(instrname)
    aligned _ipint%instrname%_slow_path_validate 256
    _ipint%instrname%_slow_path_validate:
    _ipint%instrname%_slow_path:
end

macro unimplementedInstruction(instrname)
    instructionLabel(instrname)
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

# Operation Calls

macro operationCall(fn)
    move wasmInstance, a0
    push PC, MC
    push PL, ws0
    fn()
    pop ws0, PL
    pop MC, PC
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end
end

macro operationCallMayThrow(fn)
    storei PC, CallSiteIndex[cfr]

    move wasmInstance, a0
    push PC, MC
    push PL, ws0
    fn()
    bqneq r0, 1, .continuation
    storei r1, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
.continuation:
    pop ws0, PL
    pop MC, PC
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end
end

# Exception handling
macro ipintException(exception)
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
end

# OSR
macro ipintPrologueOSR(increment)
if JIT and not ARMv7
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    baddis increment, Wasm::IPIntCallee::m_tierUpCounter + Wasm::LLIntTierUpCounter::m_counter[ws0], .continue

    subq (NumberOfWasmArgumentJSRs + NumberOfWasmArgumentFPRs) * 8, sp
if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr2, gpr1, offset[sp]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, offset[sp]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, offset[sp]
    end)
end
if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        storepaird fpr2, fpr1, offset[sp]
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, offset[sp]
    end)
end

    ipintReloadMemory()
    push memoryBase, boundsCheckingSize

    move cfr, a1
    operationCall(macro() cCall2(_ipint_extern_prologue_osr) end)
    move r0, ws0

    pop boundsCheckingSize, memoryBase

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        loadpaird offset[sp], fpr2, fpr1
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        loadd offset[sp], fpr
    end)
end

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq offset[sp], gpr2, gpr1
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq offset[sp], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia offset[sp], gpLsw, gpMsw
    end)
end
    addq (NumberOfWasmArgumentJSRs + NumberOfWasmArgumentFPRs) * 8, sp

    btpz ws0, .recover

    restoreIPIntRegisters()
    restoreCallerPCAndCFR()

    if ARM64E
        leap _g_config, ws1
        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmOSREntry) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
    else
        jmp ws0, WasmEntryPtrTag
    end

.recover:
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
.continue:
end
end

macro ipintLoopOSR(increment)
if JIT and not ARMv7
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    baddis increment, Wasm::IPIntCallee::m_tierUpCounter + Wasm::LLIntTierUpCounter::m_counter[ws0], .continue

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
    baddis increment, Wasm::IPIntCallee::m_tierUpCounter + Wasm::LLIntTierUpCounter::m_counter[ws0], .continue

    move cfr, a1
    operationCall(macro() cCall2(_ipint_extern_epilogue_osr) end)
.continue:
end
end

################################
# 3. Helper interpreter macros #
################################

macro argumINTAlign(instrname)
    aligned _ipint_argumINT%instrname%_validate 64
    _ipint_argumINT%instrname%_validate:
    _argumINT%instrname%:
end

macro mintAlign(instrname)
    aligned _ipint_mint%instrname%_validate 64
    _ipint_mint%instrname%_validate:
    _mint%instrname%:
end

macro uintAlign(instrname)
    aligned _ipint_uint%instrname%_validate 64
    _ipint_uint%instrname%_validate:
    _uint%instrname%:
end

##############################
# 4. Interpreter entrypoints #
##############################

op(ipint_entry, macro()
if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
    preserveCallerPCAndCFR()
    saveIPIntRegisters()
    storep wasmInstance, CodeBlock[cfr]
    getIPIntCallee()

    ipintEntry()
else
    break
end
end)

if WEBASSEMBLY and (ARM64 or ARM64E or X86_64 or ARMv7)
.ipint_entry_end_local:
    argumINTEnd()

    jmp .ipint_entry_end_local
.ipint_entry_finish_zero:
    argumINTFinish()

    loadp CodeBlock[cfr], wasmInstance
    # OSR Check
    ipintPrologueOSR(5)

    move sp, PL

    IfIPIntUsesIB(macro ()
        pcrtoaddr _ipint_unreachable, IB
    end)
    loadp Wasm::IPIntCallee::m_bytecode[ws0], PC
    loadp Wasm::IPIntCallee::m_metadata[ws0], MC
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
    getVMFromCallFrame(t3, t0)
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)

    loadp VM::callFrameForCatch[t3], cfr
    storep 0, VM::callFrameForCatch[t3]

    loadp VM::targetInterpreterPCForThrow[t3], PC
    loadp VM::targetInterpreterMetadataPCForThrow[t3], MC

    getIPIntCallee()

    loadp CodeBlock[cfr], wasmInstance
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end
    loadp Wasm::IPIntCallee::m_bytecode[ws0], t0
    loadp Wasm::IPIntCallee::m_metadata[ws0], t1
    addp t0, PC
    addp t1, MC

    # Recompute PL
    if ARM64 or ARM64E
        loadpairi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], t0, t1
    else
        loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], t0
        loadi Wasm::IPIntCallee::m_numRethrowSlotsToAlloc[ws0], t1
    end
    addq t1, t0
    mulq LocalSize, t0
    addq IPIntCalleeSaveSpaceStackAligned, t0
    subq cfr, t0, PL

    loadi [MC], t0
    # 1 << 4 == StackValueSize
    lshiftq 4, t0
    addq IPIntCalleeSaveSpaceStackAligned, t0
    subp cfr, t0, sp
end

global _ipint_catch_entry
_ipint_catch_entry:
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

global _ipint_catch_all_entry
_ipint_catch_all_entry:
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

# Trampoline entrypoints

op(ipint_trampoline, macro ()
    tagReturnAddress sp
    jmp _ipint_entry
end)

#################################
# 5. Instruction implementation #
#################################

if JSVALUE64 and (ARM64 or ARM64E or X86_64)
    include InPlaceInterpreter64
elsif ARMv7
    include InPlaceInterpreter32_64
else
# For unimplemented architectures: make sure that the assertions can still find the labels

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
reservedOpcode(0xd3)
reservedOpcode(0xd4)
reservedOpcode(0xd5)
reservedOpcode(0xd6)
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
reservedOpcode(0xfb)
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

