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

# Callee save

macro saveIPIntRegisters()
    # NOTE: We intentionally don't restore pinned wasm registers here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    subp IPIntCalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        storepairq MC, PC, -2 * SlotSize[cfr]
    elsif X86_64 or RISCV64
        storep PC, -1 * SlotSize[cfr]
        storep MC, -2 * SlotSize[cfr]
    end
end

macro restoreIPIntRegisters()
    # NOTE: We intentionally don't restore pinned wasm registers here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    if ARM64 or ARM64E
        loadpairq -2 * SlotSize[cfr], MC, PC
    elsif X86_64 or RISCV64
        loadp -1 * SlotSize[cfr], PC
        loadp -2 * SlotSize[cfr], MC
    end
    addp IPIntCalleeSaveSpaceStackAligned, sp
end

# Dispatch target bases

if ARM64 or ARM64E
const ipint_dispatch_base = _ipint_unreachable
const ipint_gc_dispatch_base = _ipint_struct_new
const ipint_conversion_dispatch_base = _ipint_i32_trunc_sat_f32_s
const ipint_simd_dispatch_base = _ipint_simd_v128_load_mem
const ipint_atomic_dispatch_base = _ipint_memory_atomic_notify
end

# Tail-call bytecode dispatch

macro nextIPIntInstruction()
    loadb [PC], t0
if ARM64 or ARM64E
    # x0 = opcode
    pcrtoaddr ipint_dispatch_base, t7
    addlshiftp t7, t0, (constexpr (WTF::fastLog2(JSC::IPInt::alignIPInt))), t0
    jmp t0
elsif X86_64
    leap _os_script_config_storage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_dispatch_base[t1], t1
    lshiftq (constexpr (WTF::fastLog2(JSC::IPInt::alignIPInt))), t0
    addq t1, t0
    jmp t0
else
    error
end
end

# Stack operations
# Every value on the stack is always 16 bytes! This makes life easy.

macro pushQuad(reg)
    if ARM64 or ARM64E
        push reg, reg
    elsif X86_64
        push reg, reg
    else
        break
    end
end

macro pushQuadPair(reg1, reg2)
    push reg1, reg2
end

macro popQuad(reg)
    # FIXME: emit post-increment in offlineasm
    if ARM64 or ARM64E
        loadqinc [sp], reg, V128ISize
    elsif X86_64
        loadq [sp], reg
        addq V128ISize, sp
    else
        break
    end
end

macro pushVec(reg)
    pushv reg
end

macro popVec(reg)
    popv reg
end

# Typed push/pop to make code pretty

macro pushInt32(reg)
    pushQuad(reg)
end

macro popInt32(reg)
    popQuad(reg)
end

macro pushFloat32(reg)
    pushv reg
end

macro popFloat32(reg)
    popv reg
end

macro pushInt64(reg)
    pushQuad(reg)
end

macro popInt64(reg)
    popQuad(reg)
end

macro pushFloat64(reg)
    pushv reg
end

macro popFloat64(reg)
    popv reg
end

# Entering IPInt

# MC = location in argumINT bytecode
# csr0 = tmp
# csr1 = dst
# csr2 = src
# csr3 = end
# csr4 = for dispatch

const argumINTTmp = csr0
const argumINTDst = sc0
const argumINTSrc = csr2
const argumINTEnd = csr3
const argumINTDsp = csr4

macro ipintEntry()
    const argumINTEndAsScratch = argumINTEnd
    checkStackOverflow(ws0, argumINTEndAsScratch)

    # Allocate space for locals and rethrow values
    if ARM64 or ARM64E
        loadpairi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], argumINTTmp, argumINTEnd
    else
        loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], argumINTTmp
        loadi Wasm::IPIntCallee::m_numRethrowSlotsToAlloc[ws0], argumINTEnd
    end
    mulp LocalSize, argumINTEnd
    mulp LocalSize, argumINTTmp
    subp argumINTEnd, sp
    move sp, argumINTEnd
    subp argumINTTmp, sp
    move sp, argumINTDsp
    loadp Wasm::IPIntCallee::m_argumINTBytecode + VectorBufferOffset[ws0], MC

    push argumINTTmp, argumINTDst, argumINTSrc, argumINTEnd

    move argumINTDsp, argumINTDst
    leap FirstArgumentOffset[cfr], argumINTSrc

    validateOpcodeConfig(argumINTTmp)
    argumINTDispatch()
end

macro argumINTDispatch()
    loadb [MC], argumINTTmp
    addp 1, MC
    bbgteq argumINTTmp, (constexpr IPInt::ArgumINTBytecode::NumOpcodes), _ipint_argument_dispatch_err
    lshiftp (constexpr (WTF::fastLog2(JSC::IPInt::alignArgumInt))), argumINTTmp
if ARM64 or ARM64E
    pcrtoaddr _argumINT_begin, argumINTDsp
    addp argumINTTmp, argumINTDsp
    jmp argumINTDsp
elsif X86_64
    leap (_argumINT_begin - _ipint_entry_relativePCBase)[PL], argumINTDsp
    addp argumINTTmp, argumINTDsp
    jmp argumINTDsp
else
    break
end
end

macro argumINTInitializeDefaultLocals()
    # zero out remaining locals
    bpeq argumINTDst, argumINTEnd, .ipint_entry_finish_zero
    loadb [MC], argumINTTmp
    addp 1, MC
    sxb2p argumINTTmp, argumINTTmp
    andp ValueNull, argumINTTmp
if ARM64 or ARM64E
    # offlineasm doesn't have xzr so emit it
    emit "stp x19, xzr, [x9]"
elsif X86_64
    storep argumINTTmp, [argumINTDst]
    storep 0, 8[argumINTDst]
end
    addp LocalSize, argumINTDst
end

macro argumINTFinish()
    pop argumINTEnd, argumINTSrc, argumINTDst, argumINTTmp
end

    #############################
    # 0x00 - 0x11: control flow #
    #############################

ipintOp(_unreachable, macro()
    # unreachable

    # Push to stack for the handler
    push PC, MC
    push PL, ws0

    move cfr, a1
    move sp, a2
    operationCall(macro() cCall3(_ipint_extern_unreachable_breakpoint_handler) end)

    # Remove pushed values
    addq 4 * SlotSize, sp

    bqeq r0, 0, .exception

.continue:
    nextIPIntInstruction()

.exception:
    ipintException(Unreachable)
end)

ipintOp(_nop, macro()
    # nop
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_block, macro()
    # block
    validateOpcodeConfig(t0)
if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    sxi2q t0, t0
    sxi2q t1, t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_loop, macro()
    # loop
    # We already validateOpcodeConfig in ipintLoopOSR.
    ipintLoopOSR(1)
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMCByReg(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_if, macro()
    # if
    validateOpcodeConfig(t1)
    popInt32(t0)
    bineq 0, t0, .ipint_if_taken
if ARM64 or ARM64E
    loadpairi IPInt::IfMetadata::elseDeltaPC[MC], t0, t1
else
    loadi IPInt::IfMetadata::elseDeltaPC[MC], t0
    loadi IPInt::IfMetadata::elseDeltaMC[MC], t1
end
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
.ipint_if_taken:
    # Skip LEB128
    loadb IPInt::IfMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::IfMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_else, macro()
    # else
    # Counterintuitively, we only run this instruction if the if
    # clause is TAKEN. This is used to branch to the end of the
    # block.
    validateOpcodeConfig(t0)
if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    # always skipping forward - no need to sign-extend t0, t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_try, macro()
    validateOpcodeConfig(t0)
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_catch, macro()
    # Counterintuitively, like else, we only run this instruction
    # if no exception was thrown during the preceeding try or catch block.
    validateOpcodeConfig(t0)
if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    # always skipping forward - no need to sign-extend t0, t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_throw, macro()
    saveCallSiteIndex()

    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t0
    loadp VM::topEntryFrame[t0], t0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t0)

    move cfr, a1
    move sp, a2
    loadi IPInt::ThrowMetadata::exceptionIndex[MC], a3
    operationCall(macro() cCall4(_ipint_extern_throw_exception) end)
    jumpToException()
end)

ipintOp(_rethrow, macro()
    saveCallSiteIndex()

    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t0
    loadp VM::topEntryFrame[t0], t0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t0)

    move cfr, a1
    move PL, a2
    loadi IPInt::RethrowMetadata::tryDepth[MC], a3
    operationCall(macro() cCall4(_ipint_extern_rethrow_exception) end)
    jumpToException()
end)

ipintOp(_throw_ref, macro()
    popQuad(a2)
    bieq a2, ValueNull, .throw_null_ref

    saveCallSiteIndex()

    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t0
    loadp VM::topEntryFrame[t0], t0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t0)

    move cfr, a1
    operationCall(macro() cCall3(_ipint_extern_throw_ref) end)
    jumpToException()

.throw_null_ref:
    throwException(NullExnrefReference)
end)

macro uintDispatch()
if ARM64 or ARM64E
    loadb [MC], sc2
    addq 1, MC
    bigteq sc2, (constexpr IPInt::UIntBytecode::NumOpcodes), _ipint_uint_dispatch_err
    lshiftq (constexpr (WTF::fastLog2(JSC::IPInt::alignUInt))), sc2
    pcrtoaddr _uint_begin, sc3
    addq sc2, ws3
    jmp ws3
elsif X86_64
    loadb [MC], sc1
    addq 1, MC
    bigteq sc1, (constexpr IPInt::UIntBytecode::NumOpcodes), _ipint_uint_dispatch_err
    lshiftq 6, sc1
    leap (_uint_begin - _mint_entry_relativePCBase)[PC, sc1], sc1
    jmp sc1
end
end

ipintOp(_end, macro()
    validateOpcodeConfig(t1)
if X86_64
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
end
    loadp Wasm::IPIntCallee::m_bytecodeEnd[ws0], t1
    bqeq PC, t1, .ipint_end_ret
    advancePC(1)
    nextIPIntInstruction()
end)

# This implementation is specially defined out of ipintOp scope to make end implementation tight.
.ipint_end_ret:
    loadp Wasm::IPIntCallee::m_uINTBytecode + VectorBufferOffset[ws0], MC
    ipintEpilogueOSR(10)
if X86_64
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
end
    loadi Wasm::IPIntCallee::m_topOfReturnStackFPOffset[ws0], sc0
    addp cfr, sc0

    initPCRelative(mint_entry, PC)

    // We've already validateOpcodeConfig() in all the places that can jump to .ipint_end_ret.
    uintDispatch()

ipintOp(_br, macro()
    # br
    validateOpcodeConfig(t0)
    loadh IPInt::BranchTargetMetadata::toPop[MC], t0
    # number to keep
    loadh IPInt::BranchTargetMetadata::toKeep[MC], t1

    # ex. pop 3 and keep 2
    #
    # +4 +3 +2 +1 sp
    # a  b  c  d  e
    # d  e
    #
    # [sp + k + numToPop] = [sp + k] for k in numToKeep-1 -> 0
    move t0, t2
    mulq StackValueSize, t2
    leap [sp, t2], t2

.ipint_br_poploop:
    bqeq t1, 0, .ipint_br_popend
    subq 1, t1
    move t1, t3
    mulq StackValueSize, t3
    loadq [sp, t3], t0
    storeq t0, [t2, t3]
    loadq 8[sp, t3], t0
    storeq t0, 8[t2, t3]
    jmp .ipint_br_poploop
.ipint_br_popend:
    loadh IPInt::BranchTargetMetadata::toPop[MC], t0
    mulq StackValueSize, t0
    leap [sp, t0], sp

if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    sxi2q t0, t0
    sxi2q t1, t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_br_if, macro()
    # pop i32
    validateOpcodeConfig(t2)
    popInt32(t0)
    bineq t0, 0, _ipint_br
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_br_table, macro()
    # br_table
    validateOpcodeConfig(t2)
    popInt32(t0)
    loadi IPInt::SwitchMetadata::size[MC], t1
    advanceMC(constexpr (sizeof(IPInt::SwitchMetadata)))
    bib t0, t1, .ipint_br_table_clamped
    subq t1, 1, t0
.ipint_br_table_clamped:
    move t0, t1
    muli (constexpr (sizeof(IPInt::BranchTargetMetadata))), t0
    addq t0, MC
    jmp _ipint_br
end)

ipintOp(_return, macro()
    validateOpcodeConfig(MC)
    # ret

if X86_64
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
end

    # This is guaranteed going to an end instruction, so skip
    # dispatch and end of program check for speed
    jmp .ipint_end_ret
end)

if ARM64 or ARM64E
    const IPIntCallCallee = sc1
    const IPIntCallFunctionSlot = sc0
elsif X86_64
    const IPIntCallCallee = t7
    const IPIntCallFunctionSlot = t6
end

ipintOp(_call, macro()
    // The operationCall below already calls validateOpcodeConfig().
    saveCallSiteIndex()

    loadb IPInt::CallMetadata::length[MC], t0
    advancePCByReg(t0)

    move cfr, a1
    move MC, a2
    advanceMC(IPInt::CallMetadata::signature)

    subq 16, sp
    move sp, a3

    # operation returns the entrypoint in r0 and the target instance in r1
    # operation stores the target callee to sp[0] and target function info to sp[1]
    operationCall(macro() cCall4(_ipint_extern_prepare_call) end)
    loadq [sp], IPIntCallCallee
    loadq 8[sp], IPIntCallFunctionSlot
    addq 16, sp

    # call
    jmp .ipint_call_common
end)

ipintOp(_call_indirect, macro()
    // The operationCall below already calls validateOpcodeConfig().
    saveCallSiteIndex()

    loadb IPInt::CallIndirectMetadata::length[MC], t2
    advancePCByReg(t2)

    # Get function index by pointer, use it as a return for callee
    move sp, a2

    # Get callIndirectMetadata
    move cfr, a1
    move MC, a3
    advanceMC(IPInt::CallIndirectMetadata::signature)

    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_indirect) end)

    loadq [sp], IPIntCallCallee
    loadq 8[sp], IPIntCallFunctionSlot
    addq 16, sp

    jmp .ipint_call_common
end)

ipintOp(_return_call, macro()
    // The operationCall below already calls validateOpcodeConfig().
    saveCallSiteIndex()

    loadb IPInt::TailCallMetadata::length[MC], t0
    advancePCByReg(t0)

    move cfr, a1
    move MC, a2
    subq 16, sp
    move sp, a3

    # operation returns the entrypoint in r0 and the target instance in r1
    # this operation stores the boxed Callee into *r2
    operationCall(macro() cCall4(_ipint_extern_prepare_call) end)

    loadq [sp], IPIntCallCallee
    loadq 8[sp], IPIntCallFunctionSlot
    addq 16, sp

    loadi IPInt::TailCallMetadata::callerStackArgSize[MC], t3
    advanceMC(IPInt::TailCallMetadata::argumentBytecode)
    jmp .ipint_tail_call_common
end)

ipintOp(_return_call_indirect, macro()
    // The operationCallMayThrow below already calls validateOpcodeConfig().
    saveCallSiteIndex()

    loadb IPInt::TailCallIndirectMetadata::length[MC], t2
    advancePCByReg(t2)

    # Get function index by pointer, use it as a return for callee
    move sp, a2

    # Get callIndirectMetadata
    move cfr, a1
    move MC, a3
    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_indirect) end)

    loadq [sp], IPIntCallCallee
    loadq 8[sp], IPIntCallFunctionSlot
    addq 16, sp

    loadi IPInt::TailCallIndirectMetadata::callerStackArgSize[MC], t3
    advanceMC(IPInt::TailCallIndirectMetadata::argumentBytecode)
    jmp .ipint_tail_call_common
end)

ipintOp(_call_ref, macro()
    // The operationCall below already calls validateOpcodeConfig().
    saveCallSiteIndex()

    move cfr, a1
    move MC, a2
    move sp, a3

    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_ref) end)
    loadq [sp], IPIntCallCallee
    loadq 8[sp], IPIntCallFunctionSlot
    addq 16, sp

    loadb IPInt::CallRefMetadata::length[MC], t3
    advanceMC(IPInt::CallRefMetadata::signature)
    advancePCByReg(t3)

    jmp .ipint_call_common
end)

ipintOp(_return_call_ref, macro()
    // The operationCallMayThrow below already calls validateOpcodeConfig().
    saveCallSiteIndex()

    loadb IPInt::TailCallRefMetadata::length[MC], t2
    advancePCByReg(t2)

    move cfr, a1
    move MC, a2
    move sp, a3
    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_ref) end)
    loadq [sp], IPIntCallCallee
    loadq 8[sp], IPIntCallFunctionSlot
    addq 16, sp

    loadi IPInt::TailCallRefMetadata::callerStackArgSize[MC], t3
    advanceMC(IPInt::TailCallRefMetadata::argumentBytecode)
    jmp .ipint_tail_call_common
end)

reservedOpcode(0x16)
reservedOpcode(0x17)

ipintOp(_delegate, macro()
    # Counterintuitively, like else, we only run this instruction
    # if no exception was thrown during the preceeding try or catch block.
    validateOpcodeConfig(t0)
if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    # always skipping forward - no need to sign-extend t0, t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_catch_all, macro()
    # Counterintuitively, like else, we only run this instruction
    # if no exception was thrown during the preceeding try or catch block.
    validateOpcodeConfig(t0)
if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    # always skipping forward - no need to sign-extend t0, t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_drop, macro()
    addq StackValueSize, sp
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_select, macro()
    popInt32(t0)
    bieq t0, 0, .ipint_select_val2
    addq StackValueSize, sp
    advancePC(1)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_select_val2:
    popVec(v1)
    popVec(v0)
    pushVec(v1)
    advancePC(1)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_select_t, macro()
    popInt32(t0)
    bieq t0, 0, .ipint_select_t_val2
    addq StackValueSize, sp
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_select_t_val2:
    popVec(v1)
    popVec(v0)
    pushVec(v1)
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

reservedOpcode(0x1d)
reservedOpcode(0x1e)

ipintOp(_try_table, macro()
    # advance MC/PC
    validateOpcodeConfig(t0)
if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    # always skipping forward - no need to sign-extend t0, t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

    ###################################
    # 0x20 - 0x26: get and set values #
    ###################################

macro localGetPostDecode()
    # Index into locals
    mulq LocalSize, t0
    loadv [PL, t0], v0
    # Push to stack
    pushVec(v0)
    nextIPIntInstruction()
end

ipintOp(_local_get, macro()
    # local.get
    loadb 1[PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_get_slow_path
    localGetPostDecode()
end)

macro localSetPostDecode()
    # Pop from stack
    popVec(v0)
    # Store to locals
    mulq LocalSize, t0
    storev v0, [PL, t0]
    nextIPIntInstruction()
end

ipintOp(_local_set, macro()
    # local.set
    loadb 1[PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_set_slow_path
    localSetPostDecode()
end)

macro localTeePostDecode()
    # Load from stack
    loadv [sp], v0
    # Store to locals
    mulq LocalSize, t0
    storev v0, [PL, t0]
    nextIPIntInstruction()
end

ipintOp(_local_tee, macro()
    # local.tee
    loadb 1[PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_tee_slow_path
    localTeePostDecode()
end)

ipintOp(_global_get, macro()
    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)

    # Load pre-computed index from metadata
    loadb IPInt::GlobalMetadata::bindingMode[MC], t2
    loadi IPInt::GlobalMetadata::index[MC], t1
    loadp JSWebAssemblyInstance::m_globals[wasmInstance], t0
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))

    lshiftp 1, t1
    bieq t2, 0, .ipint_global_get_embedded
    loadp [t0, t1, 8], t0
    loadv [t0], v0
    pushVec(v0)
    nextIPIntInstruction()

.ipint_global_get_embedded:
    loadv [t0, t1, 8], v0
    pushVec(v0)
    nextIPIntInstruction()
end)

ipintOp(_global_set, macro()
    # isRef = 1 => ref, use slowpath
    loadb IPInt::GlobalMetadata::isRef[MC], t0
    bineq t0, 0, .ipint_global_set_refpath
    # bindingMode = 1 => portable
    loadb IPInt::GlobalMetadata::bindingMode[MC], t2
    # get global addr
    loadp JSWebAssemblyInstance::m_globals[wasmInstance], t0
    # get value to store
    popVec(v0)
    # get index
    loadi IPInt::GlobalMetadata::index[MC], t1
    lshiftp 1, t1
    bieq t2, 0, .ipint_global_set_embedded
    # portable: dereference then set
    loadp [t0, t1, 8], t0
    storev v0, [t0]
    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    jmp .ipint_global_set_dispatch

.ipint_global_set_embedded:
    # embedded: set directly
    storev v0, [t0, t1, 8]
    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    jmp .ipint_global_set_dispatch

.ipint_global_set_refpath:
    loadi IPInt::GlobalMetadata::index[MC], a1
    # Pop from stack
    popQuad(a2)
    operationCall(macro() cCall3(_ipint_extern_set_global_ref) end)

    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))

.ipint_global_set_dispatch:
    nextIPIntInstruction()
end)

ipintOp(_table_get, macro()
    # Load pre-computed index from metadata
    loadi IPInt::Const32Metadata::value[MC], a1
    popInt32(a2)

    operationCallMayThrow(macro() cCall3(_ipint_extern_table_get) end)

    pushQuad(r0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_set, macro()
    # Load pre-computed index from metadata
    loadi IPInt::Const32Metadata::value[MC], a1
    popQuad(a3)
    popInt32(a2)
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_set) end)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

reservedOpcode(0x27)

macro popMemoryIndex(reg, tmp)
    popInt32(reg)
    ori 0, reg
end

macro ipintCheckMemoryBound(mem, scratch, size)
    # Memory indices are 32 bit
    leap size - 1[mem], scratch
    bpb scratch, boundsCheckingSize, .continuation
    ipintException(OutOfBoundsMemoryAccess)
.continuation:
end

ipintOp(_i32_load_mem, macro()
    # i32.load
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    pushInt32(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load_mem, macro()
    # i32.load
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    loadq [memoryBase, t0], t1
    pushInt64(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f32_load_mem, macro()
    # f32.load
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadf [memoryBase, t0], ft0
    pushFloat32(ft0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f64_load_mem, macro()
    # f64.load
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    loadd [memoryBase, t0], ft0
    pushFloat64(ft0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_load8s_mem, macro()
    # i32.load8_s
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    sxb2i t1, t1
    pushInt32(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_load8u_mem, macro()
    # i32.load8_u
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    pushInt32(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_load16s_mem, macro()
    # i32.load16_s
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    sxh2i t1, t1
    pushInt32(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_load16u_mem, macro()
    # i32.load16_u
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    pushInt32(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load8s_mem, macro()
    # i64.load8_s
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    sxb2q t1, t1
    pushInt64(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load8u_mem, macro()
    # i64.load8_u
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    pushInt64(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load16s_mem, macro()
    # i64.load16_s
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    sxh2q t1, t1
    pushInt64(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load16u_mem, macro()
    # i64.load16_u
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    pushInt64(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load32s_mem, macro()
    # i64.load32_s
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    sxi2q t1, t1
    pushInt64(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load32u_mem, macro()
    # i64.load8_s
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    pushInt64(t1)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_store_mem, macro()
    # i32.store
    # pop data
    popInt32(t1)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    storei t1, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store_mem, macro()
    # i64.store
    # pop data
    popInt64(t1)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    storeq t1, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f32_store_mem, macro()
    # f32.store
    # pop data
    popFloat32(ft0)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    storef ft0, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f64_store_mem, macro()
    # f64.store
    # pop data
    popFloat64(ft0)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    stored ft0, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_store8_mem, macro()
    # i32.store8
    # pop data
    popInt32(t1)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    storeb t1, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_store16_mem, macro()
    # i32.store16
    # pop data
    popInt32(t1)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    storeh t1, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store8_mem, macro()
    # i64.store8
    # pop data
    popInt64(t1)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    storeb t1, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store16_mem, macro()
    # i64.store16
    # pop data
    popInt64(t1)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    storeh t1, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store32_mem, macro()
    # i64.store32
    # pop data
    popInt64(t1)
    # pop index
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    storei t1, [memoryBase, t0]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_memory_size, macro()
    loadp JSWebAssemblyInstance::m_cachedMemorySize[wasmInstance], t0
    urshiftp 16, t0
    zxi2q t0, t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_memory_grow, macro()
    popInt32(a1)
    operationCall(macro() cCall2(_ipint_extern_memory_grow) end)
    pushInt32(r0)
    ipintReloadMemory()
    advancePC(2)
    nextIPIntInstruction()
end)

    ################################
    # 0x41 - 0x44: constant values #
    ################################

ipintOp(_i32_const, macro()
    # i32.const
    loadb IPInt::InstructionLengthMetadata::length[MC], t1
    bigteq t1, 2, .ipint_i32_const_slowpath
    loadb 1[PC], t0
    lshiftq 7, t1
    orq t1, t0
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(2)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_i32_const_slowpath:
    # Load pre-computed value from metadata
    loadi IPInt::Const32Metadata::value[MC], t0
    # Push to stack
    pushInt32(t0)

    advancePCByReg(t1)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_const, macro()
    # i64.const
    # Load pre-computed value from metadata
    loadq IPInt::Const64Metadata::value[MC], t0
    # Push to stack
    pushInt64(t0)
    loadb IPInt::Const64Metadata::instructionLength[MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const64Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f32_const, macro()
    # f32.const
    # Load pre-computed value from metadata
    loadf 1[PC], ft0
    pushFloat32(ft0)

    advancePC(5)
    nextIPIntInstruction()
end)

ipintOp(_f64_const, macro()
    # f64.const
    # Load pre-computed value from metadata
    loadd 1[PC], ft0
    pushFloat64(ft0)

    advancePC(9)
    nextIPIntInstruction()
end)

    ###############################
    # 0x45 - 0x4f: i32 comparison #
    ###############################

ipintOp(_i32_eqz, macro()
    # i32.eqz
    popInt32(t0)
    cieq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_eq, macro()
    # i32.eq
    popInt32(t1)
    popInt32(t0)
    cieq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ne, macro()
    # i32.ne
    popInt32(t1)
    popInt32(t0)
    cineq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_lt_s, macro()
    # i32.lt_s
    popInt32(t1)
    popInt32(t0)
    cilt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_lt_u, macro()
    # i32.lt_u
    popInt32(t1)
    popInt32(t0)
    cib t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_gt_s, macro()
    # i32.gt_s
    popInt32(t1)
    popInt32(t0)
    cigt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_gt_u, macro()
    # i32.gt_u
    popInt32(t1)
    popInt32(t0)
    cia t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_le_s, macro()
    # i32.le_s
    popInt32(t1)
    popInt32(t0)
    cilteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_le_u, macro()
    # i32.le_u
    popInt32(t1)
    popInt32(t0)
    cibeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ge_s, macro()
    # i32.ge_s
    popInt32(t1)
    popInt32(t0)
    cigteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ge_u, macro()
    # i32.ge_u
    popInt32(t1)
    popInt32(t0)
    ciaeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x50 - 0x5a: i64 comparison #
    ###############################

ipintOp(_i64_eqz, macro()
    # i64.eqz
    popInt64(t0)
    cqeq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_eq, macro()
    # i64.eq
    popInt64(t1)
    popInt64(t0)
    cqeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ne, macro()
    # i64.ne
    popInt64(t1)
    popInt64(t0)
    cqneq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_lt_s, macro()
    # i64.lt_s
    popInt64(t1)
    popInt64(t0)
    cqlt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_lt_u, macro()
    # i64.lt_u
    popInt64(t1)
    popInt64(t0)
    cqb t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_gt_s, macro()
    # i64.gt_s
    popInt64(t1)
    popInt64(t0)
    cqgt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_gt_u, macro()
    # i64.gt_u
    popInt64(t1)
    popInt64(t0)
    cqa t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_le_s, macro()
    # i64.le_s
    popInt64(t1)
    popInt64(t0)
    cqlteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_le_u, macro()
    # i64.le_u
    popInt64(t1)
    popInt64(t0)
    cqbeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ge_s, macro()
    # i64.ge_s
    popInt64(t1)
    popInt64(t0)
    cqgteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ge_u, macro()
    # i64.ge_u
    popInt64(t1)
    popInt64(t0)
    cqaeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x5b - 0x60: f32 comparison #
    ###############################

ipintOp(_f32_eq, macro()
    # f32.eq
    popFloat32(ft1)
    popFloat32(ft0)
    cfeq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_ne, macro()
    # f32.ne
    popFloat32(ft1)
    popFloat32(ft0)
    cfnequn ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_lt, macro()
    # f32.lt
    popFloat32(ft1)
    popFloat32(ft0)
    cflt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_gt, macro()
    # f32.gt
    popFloat32(ft1)
    popFloat32(ft0)
    cfgt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_le, macro()
    # f32.le
    popFloat32(ft1)
    popFloat32(ft0)
    cflteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_ge, macro()
    # f32.ge
    popFloat32(ft1)
    popFloat32(ft0)
    cfgteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x61 - 0x66: f64 comparison #
    ###############################

ipintOp(_f64_eq, macro()
    # f64.eq
    popFloat64(ft1)
    popFloat64(ft0)
    cdeq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_ne, macro()
    # f64.ne
    popFloat64(ft1)
    popFloat64(ft0)
    cdnequn ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_lt, macro()
    # f64.lt
    popFloat64(ft1)
    popFloat64(ft0)
    cdlt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_gt, macro()
    # f64.gt
    popFloat64(ft1)
    popFloat64(ft0)
    cdgt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_le, macro()
    # f64.le
    popFloat64(ft1)
    popFloat64(ft0)
    cdlteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_ge, macro()
    # f64.ge
    popFloat64(ft1)
    popFloat64(ft0)
    cdgteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x67 - 0x78: i32 operations #
    ###############################

ipintOp(_i32_clz, macro()
    # i32.clz
    popInt32(t0)
    lzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ctz, macro()
    # i32.ctz
    popInt32(t0)
    tzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_popcnt, macro()
    # i32.popcnt
    popInt32(t1)
    operationCall(macro() cCall2(_slow_path_wasm_popcount) end)
    pushInt32(r1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_add, macro()
    # i32.add
    popInt32(t1)
    popInt32(t0)
    addi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_sub, macro()
    # i32.sub
    popInt32(t1)
    popInt32(t0)
    subi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_mul, macro()
    # i32.mul
    popInt32(t1)
    popInt32(t0)
    muli t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_div_s, macro()
    # i32.div_s
    popInt32(t1)
    popInt32(t0)
    btiz t1, .ipint_i32_div_s_throwDivisionByZero

    bineq t1, -1, .ipint_i32_div_s_safe
    bieq t0, constexpr INT32_MIN, .ipint_i32_div_s_throwIntegerOverflow

.ipint_i32_div_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cdqi
        idivi t1
    elsif ARM64 or ARM64E or RISCV64
        divis t1, t0
    else
        error
    end
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_div_s_throwDivisionByZero:
    ipintException(DivisionByZero)

.ipint_i32_div_s_throwIntegerOverflow:
    ipintException(IntegerOverflow)
end)

ipintOp(_i32_div_u, macro()
    # i32.div_u
    popInt32(t1)
    popInt32(t0)
    btiz t1, .ipint_i32_div_u_throwDivisionByZero

    if X86_64
        xori t2, t2
        udivi t1
    elsif ARM64 or ARM64E or RISCV64
        divi t1, t0
    else
        error
    end
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_div_u_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i32_rem_s, macro()
    # i32.rem_s
    popInt32(t1)
    popInt32(t0)

    btiz t1, .ipint_i32_rem_s_throwDivisionByZero

    bineq t1, -1, .ipint_i32_rem_s_safe
    bineq t0, constexpr INT32_MIN, .ipint_i32_rem_s_safe

    move 0, t2
    jmp .ipint_i32_rem_s_return

.ipint_i32_rem_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cdqi
        idivi t1
    elsif ARM64 or ARM64E
        divis t1, t0, t2
        muli t1, t2
        subi t0, t2, t2
    elsif RISCV64
        remis t0, t1, t2
    else
        error
    end

.ipint_i32_rem_s_return:
    pushInt32(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_rem_s_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i32_rem_u, macro()
    # i32.rem_u
    popInt32(t1)
    popInt32(t0)
    btiz t1, .ipint_i32_rem_u_throwDivisionByZero

    if X86_64
        xori t2, t2
        udivi t1
    elsif ARM64 or ARM64E
        divi t1, t0, t2
        muli t1, t2
        subi t0, t2, t2
    elsif RISCV64
        remi t0, t1, t2
    else
        error
    end
    pushInt32(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i32_rem_u_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i32_and, macro()
    # i32.and
    popInt32(t1)
    popInt32(t0)
    andi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_or, macro()
    # i32.or
    popInt32(t1)
    popInt32(t0)
    ori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_xor, macro()
    # i32.xor
    popInt32(t1)
    popInt32(t0)
    xori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_shl, macro()
    # i32.shl
    popInt32(t1)
    popInt32(t0)
    lshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_shr_s, macro()
    # i32.shr_s
    popInt32(t1)
    popInt32(t0)
    rshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_shr_u, macro()
    # i32.shr_u
    popInt32(t1)
    popInt32(t0)
    urshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_rotl, macro()
    # i32.rotl
    popInt32(t1)
    popInt32(t0)
    lrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_rotr, macro()
    # i32.rotr
    popInt32(t1)
    popInt32(t0)
    rrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x79 - 0x8a: i64 operations #
    ###############################

ipintOp(_i64_clz, macro()
    # i64.clz
    popInt64(t0)
    lzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ctz, macro()
    # i64.ctz
    popInt64(t0)
    tzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_popcnt, macro()
    # i64.popcnt
    popInt64(t1)
    operationCall(macro() cCall2(_slow_path_wasm_popcountll) end)
    pushInt64(r1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_add, macro()
    # i64.add
    popInt64(t1)
    popInt64(t0)
    addq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_sub, macro()
    # i64.sub
    popInt64(t1)
    popInt64(t0)
    subq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_mul, macro()
    # i64.mul
    popInt64(t1)
    popInt64(t0)
    mulq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_div_s, macro()
    # i64.div_s
    popInt64(t1)
    popInt64(t0)
    btqz t1, .ipint_i64_div_s_throwDivisionByZero

    bqneq t1, -1, .ipint_i64_div_s_safe
    bqeq t0, constexpr INT64_MIN, .ipint_i64_div_s_throwIntegerOverflow

.ipint_i64_div_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cqoq
        idivq t1
    elsif ARM64 or ARM64E or RISCV64
        divqs t1, t0
    else
        error
    end
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_div_s_throwDivisionByZero:
    ipintException(DivisionByZero)

.ipint_i64_div_s_throwIntegerOverflow:
    ipintException(IntegerOverflow)
end)

ipintOp(_i64_div_u, macro()
    # i64.div_u
    popInt64(t1)
    popInt64(t0)
    btqz t1, .ipint_i64_div_u_throwDivisionByZero

    if X86_64
        xorq t2, t2
        udivq t1
    elsif ARM64 or ARM64E or RISCV64
        divq t1, t0
    else
        error
    end
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_div_u_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i64_rem_s, macro()
    # i64.rem_s
    popInt64(t1)
    popInt64(t0)

    btqz t1, .ipint_i64_rem_s_throwDivisionByZero

    bqneq t1, -1, .ipint_i64_rem_s_safe
    bqneq t0, constexpr INT64_MIN, .ipint_i64_rem_s_safe

    move 0, t2
    jmp .ipint_i64_rem_s_return

.ipint_i64_rem_s_safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cqoq
        idivq t1
    elsif ARM64 or ARM64E
        divqs t1, t0, t2
        mulq t1, t2
        subq t0, t2, t2
    elsif RISCV64
        remqs t0, t1, t2
    else
        error
    end

.ipint_i64_rem_s_return:
    pushInt64(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_rem_s_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i64_rem_u, macro()
    # i64.rem_u
    popInt64(t1)
    popInt64(t0)
    btqz t1, .ipint_i64_rem_u_throwDivisionByZero

    if X86_64
        xorq t2, t2
        udivq t1
    elsif ARM64 or ARM64E
        divq t1, t0, t2
        mulq t1, t2
        subq t0, t2, t2
    elsif RISCV64
        remq t0, t1, t2
    else
        error
    end
    pushInt64(t2)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_rem_u_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i64_and, macro()
    # i64.and
    popInt64(t1)
    popInt64(t0)
    andq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_or, macro()
    # i64.or
    popInt64(t1)
    popInt64(t0)
    orq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_xor, macro()
    # i64.xor
    popInt64(t1)
    popInt64(t0)
    xorq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shl, macro()
    # i64.shl
    popInt64(t1)
    popInt64(t0)
    lshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shr_s, macro()
    # i64.shr_s
    popInt64(t1)
    popInt64(t0)
    rshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shr_u, macro()
    # i64.shr_u
    popInt64(t1)
    popInt64(t0)
    urshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_rotl, macro()
    # i64.rotl
    popInt64(t1)
    popInt64(t0)
    lrotateq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_rotr, macro()
    # i64.rotr
    popInt64(t1)
    popInt64(t0)
    rrotateq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x8b - 0x98: f32 operations #
    ###############################

ipintOp(_f32_abs, macro()
    # f32.abs
    popFloat32(ft0)
    absf ft0, ft1
    pushFloat32(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_neg, macro()
    # f32.neg
    popFloat32(ft0)
    negf ft0, ft1
    pushFloat32(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_ceil, macro()
    # f32.ceil
    popFloat32(ft0)
    ceilf ft0, ft1
    pushFloat32(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_floor, macro()
    # f32.floor
    popFloat32(ft0)
    floorf ft0, ft1
    pushFloat32(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_trunc, macro()
    # f32.trunc
    popFloat32(ft0)
    truncatef ft0, ft1
    pushFloat32(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_nearest, macro()
    # f32.nearest
    popFloat32(ft0)
    roundf ft0, ft1
    pushFloat32(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_sqrt, macro()
    # f32.sqrt
    popFloat32(ft0)
    sqrtf ft0, ft1
    pushFloat32(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_add, macro()
    # f32.add
    popFloat32(ft1)
    popFloat32(ft0)
    addf ft1, ft0
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_sub, macro()
    # f32.sub
    popFloat32(ft1)
    popFloat32(ft0)
    subf ft1, ft0
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_mul, macro()
    # f32.mul
    popFloat32(ft1)
    popFloat32(ft0)
    mulf ft1, ft0
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_div, macro()
    # f32.div
    popFloat32(ft1)
    popFloat32(ft0)
    divf ft1, ft0
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_min, macro()
    # f32.min
    popFloat32(ft1)
    popFloat32(ft0)
    bfeq ft0, ft1, .ipint_f32_min_equal
    bflt ft0, ft1, .ipint_f32_min_lt
    bfgt ft0, ft1, .ipint_f32_min_return

.ipint_f32_min_NaN:
    addf ft0, ft1
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_equal:
    orf ft0, ft1
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_lt:
    moved ft0, ft1
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_return:
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_max, macro()
    # f32.max
    popFloat32(ft1)
    popFloat32(ft0)

    bfeq ft1, ft0, .ipint_f32_max_equal
    bflt ft1, ft0, .ipint_f32_max_lt
    bfgt ft1, ft0, .ipint_f32_max_return

.ipint_f32_max_NaN:
    addf ft0, ft1
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_equal:
    andf ft0, ft1
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_lt:
    moved ft0, ft1
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_return:
    pushFloat32(ft1)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_copysign, macro()
    # f32.copysign
    popFloat32(ft1)
    popFloat32(ft0)

    ff2i ft1, t1
    move 0x80000000, t2
    andi t2, t1

    ff2i ft0, t0
    move 0x7fffffff, t2
    andi t2, t0

    ori t1, t0
    fi2f t0, ft0

    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x99 - 0xa6: f64 operations #
    ###############################

ipintOp(_f64_abs, macro()
    # f64.abs
    popFloat64(ft0)
    absd ft0, ft1
    pushFloat64(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_neg, macro()
    # f64.neg
    popFloat64(ft0)
    negd ft0, ft1
    pushFloat64(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_ceil, macro()
    # f64.ceil
    popFloat64(ft0)
    ceild ft0, ft1
    pushFloat64(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_floor, macro()
    # f64.floor
    popFloat64(ft0)
    floord ft0, ft1
    pushFloat64(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_trunc, macro()
    # f64.trunc
    popFloat64(ft0)
    truncated ft0, ft1
    pushFloat64(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_nearest, macro()
    # f64.nearest
    popFloat64(ft0)
    roundd ft0, ft1
    pushFloat64(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_sqrt, macro()
    # f64.sqrt
    popFloat64(ft0)
    sqrtd ft0, ft1
    pushFloat64(ft1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_add, macro()
    # f64.add
    popFloat64(ft1)
    popFloat64(ft0)
    addd ft1, ft0
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_sub, macro()
    # f64.sub
    popFloat64(ft1)
    popFloat64(ft0)
    subd ft1, ft0
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_mul, macro()
    # f64.mul
    popFloat64(ft1)
    popFloat64(ft0)
    muld ft1, ft0
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_div, macro()
    # f64.div
    popFloat64(ft1)
    popFloat64(ft0)
    divd ft1, ft0
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_min, macro()
    # f64.min
    popFloat64(ft1)
    popFloat64(ft0)
    bdeq ft0, ft1, .ipint_f64_min_equal
    bdlt ft0, ft1, .ipint_f64_min_lt
    bdgt ft0, ft1, .ipint_f64_min_return

.ipint_f64_min_NaN:
    addd ft0, ft1
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_equal:
    ord ft0, ft1
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_lt:
    moved ft0, ft1
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_return:
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_max, macro()
    # f64.max
    popFloat64(ft1)
    popFloat64(ft0)

    bdeq ft1, ft0, .ipint_f64_max_equal
    bdlt ft1, ft0, .ipint_f64_max_lt
    bdgt ft1, ft0, .ipint_f64_max_return

.ipint_f64_max_NaN:
    addd ft0, ft1
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_equal:
    andd ft0, ft1
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_lt:
    moved ft0, ft1
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_return:
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_copysign, macro()
    # f64.copysign
    popFloat64(ft1)
    popFloat64(ft0)

    fd2q ft1, t1
    move 0x8000000000000000, t2
    andq t2, t1

    fd2q ft0, t0
    move 0x7fffffffffffffff, t2
    andq t2, t0

    orq t1, t0
    fq2d t0, ft0

    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

    ############################
    # 0xa7 - 0xc4: conversions #
    ############################

ipintOp(_i32_wrap_i64, macro()
    # because of how we store values on stack, do nothing
    advancePC(1)
    nextIPIntInstruction()
end)


ipintOp(_i32_trunc_f32_s, macro()
    popFloat32(ft0)
    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_trunc_i32_f32_s_outOfBoundsTrunc

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_trunc_i32_f32_s_outOfBoundsTrunc

    truncatef2is ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f32_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i32_trunc_f32_u, macro()
    popFloat32(ft0)
    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_trunc_i32_f32_u_outOfBoundsTrunc

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_trunc_i32_f32_u_outOfBoundsTrunc

    truncatef2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f32_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i32_trunc_f64_s, macro()
    popFloat64(ft0)
    move 0xc1e0000000200000, t0 # INT32_MIN - 1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_trunc_i32_f64_s_outOfBoundsTrunc

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_trunc_i32_f64_s_outOfBoundsTrunc

    truncated2is ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f64_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i32_trunc_f64_u, macro()
    popFloat64(ft0)
    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_trunc_i32_f64_u_outOfBoundsTrunc

    move 0x41f0000000000000, t0 # INT32_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_trunc_i32_f64_u_outOfBoundsTrunc

    truncated2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i32_f64_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i64_extend_i32_s, macro()
    popInt32(t0)
    sxi2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend_i32_u, macro()
    popInt32(t0)
    move 0, t1
    noti t1
    andq t1, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_trunc_f32_s, macro()
    popFloat32(ft0)
    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_trunc_i64_f32_s_outOfBoundsTrunc

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_trunc_i64_f32_s_outOfBoundsTrunc

    truncatef2qs ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i64_f32_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i64_trunc_f32_u, macro()
    popFloat32(ft0)
    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i64_f32_u_outOfBoundsTrunc

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_f32_u_outOfBoundsTrunc

    truncatef2q ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f32_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i64_trunc_f64_s, macro()
    popFloat64(ft0)
    move 0xc3e0000000000000, t0 # INT64_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .ipint_i64_f64_s_outOfBoundsTrunc

    move 0x43e0000000000000, t0 # -INT64_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_f64_s_outOfBoundsTrunc

    truncated2qs ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f64_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i64_trunc_f64_u, macro()
    popFloat64(ft0)
    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i64_f64_u_outOfBoundsTrunc

    move 0x43f0000000000000, t0 # INT64_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_f64_u_outOfBoundsTrunc

    truncated2q ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f64_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_f32_convert_i32_s, macro()
    popInt32(t0)
    andq 0xffffffff, t0
    ci2fs t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i32_u, macro()
    popInt32(t0)
    andq 0xffffffff, t0
    ci2f t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i64_s, macro()
    popInt64(t0)
    cq2fs t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i64_u, macro()
    popInt64(t0)
    if X86_64
        cq2f t0, t1, ft0
    else
        cq2f t0, ft0
    end
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_demote_f64, macro()
    popFloat64(ft0)
    cd2f ft0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i32_s, macro()
    popInt32(t0)
    andq 0xffffffff, t0
    ci2ds t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i32_u, macro()
    popInt32(t0)
    andq 0xffffffff, t0
    ci2d t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i64_s, macro()
    popInt64(t0)
    cq2ds t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i64_u, macro()
    popInt64(t0)
    if X86_64
        cq2d t0, t1, ft0
    else
        cq2d t0, ft0
    end
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_promote_f32, macro()
    popFloat32(ft0)
    cf2d ft0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_reinterpret_f32, macro()
    popFloat32(ft0)
    ff2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_reinterpret_f64, macro()
    popFloat64(ft0)
    fd2q ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_reinterpret_i32, macro()
    # nop because of stack layout
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_reinterpret_i64, macro()
    # nop because of stack layout
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_extend8_s, macro()
    # i32.extend8_s
    popInt32(t0)
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_extend16_s, macro()
    # i32.extend8_s
    popInt32(t0)
    sxh2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend8_s, macro()
    # i64.extend8_s
    popInt64(t0)
    sxb2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend16_s, macro()
    # i64.extend8_s
    popInt64(t0)
    sxh2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend32_s, macro()
    # i64.extend8_s
    popInt64(t0)
    sxi2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

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

    #####################
    # 0xd0 - 0xd6: refs #
    #####################

ipintOp(_ref_null_t, macro()
    loadi IPInt::Const32Metadata::value[MC], t0
    pushQuad(t0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePC(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_is_null, macro()
    popQuad(t0)
    cqeq t0, ValueNull, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_ref_func, macro()
    loadi IPInt::Const32Metadata::value[MC], a1
    operationCall(macro() cCall2(_ipint_extern_ref_func) end)
    pushQuad(r0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePC(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_eq, macro()
    popQuad(t0)
    popQuad(t1)
    cqeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_ref_as_non_null, macro()
    loadq [sp], t0
    bqeq t0, ValueNull, .ref_as_non_null_nullRef
    advancePC(1)
    nextIPIntInstruction()
.ref_as_non_null_nullRef:
    throwException(NullRefAsNonNull)
end)

ipintOp(_br_on_null, macro()
    validateOpcodeConfig(t0)
    loadq [sp], t0
    bqneq t0, ValueNull, .br_on_null_not_null

    # pop the null
    addq StackValueSize, sp
    jmp _ipint_br
.br_on_null_not_null:
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_br_on_non_null, macro()
    validateOpcodeConfig(t0)
    loadq [sp], t0
    bqneq t0, ValueNull, _ipint_br
    addq StackValueSize, sp
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

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

# If the following four instructions are given more descriptive names,
# the changes should be matched in IPINT_INSTRUCTIONS in Tools/lldb/debug_ipint.py

ipintOp(_gc_prefix, macro()
    decodeLEBVarUInt32(1, t0, t1, t2, t3, t4)
    # Security guarantee: always less than 30 (0x00 -> 0x1e)
    biaeq t0, 0x1f, .ipint_gc_nonexistent
    leap _os_script_config_storage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_gc_dispatch_base[t1], t1
    if ARM64 or ARM64E
        addlshiftp t1, t0, 8, t0
        jmp t0
    elsif X86_64
        lshiftq 8, t0
        addq t1, t0
        jmp t0
    end

.ipint_gc_nonexistent:
    break
end)

ipintOp(_conversion_prefix, macro()
    decodeLEBVarUInt32(1, t0, t1, t2, t3, t4)
    # Security guarantee: always less than 18 (0x00 -> 0x11)
    biaeq t0, 0x12, .ipint_conversion_nonexistent
    leap _os_script_config_storage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_conversion_dispatch_base[t1], t1
    if ARM64 or ARM64E
        addlshiftp t1, t0, 8, t0
        jmp t0
    elsif X86_64
        lshiftq 8, t0
        addq t1, t0
        jmp t0
    end

.ipint_conversion_nonexistent:
    break
end)

ipintOp(_simd_prefix, macro()
    decodeLEBVarUInt32(1, t0, t1, t2, t3, t4)
    # Security guarantee: always less than 256 (0x00 -> 0xff)
    biaeq t0, 0x100, .ipint_simd_nonexistent
    leap _os_script_config_storage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_simd_dispatch_base[t1], t1
    if ARM64 or ARM64E
        addlshiftp t1, t0, 8, t0
        jmp t0
    elsif X86_64
        lshiftq 8, t0
        addq t1, t0
        jmp t0
    end

.ipint_simd_nonexistent:
    break
end)

ipintOp(_atomic_prefix, macro()
    decodeLEBVarUInt32(1, t0, t1, t2, t3, t4)
    # Security guarantee: always less than 78 (0x00 -> 0x4e)
    biaeq t0, 0x4f, .ipint_atomic_nonexistent
    leap _os_script_config_storage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_atomic_dispatch_base[t1], t1
    if ARM64 or ARM64E
        addlshiftp t1, t0, 8, t0
        jmp t0
    elsif X86_64
        lshiftq 8, t0
        addq t1, t0
        jmp t0
    end

.ipint_atomic_nonexistent:
    break
end)

reservedOpcode(0xff)
    break

    #####################
    ## GC instructions ##
    #####################

ipintOp(_struct_new, macro()
    loadi IPInt::StructNewMetadata::type[MC], a1  # type
    move sp, a2
    operationCallMayThrow(macro() cCall3(_ipint_extern_struct_new) end)
    loadh IPInt::StructNewMetadata::params[MC], t1  # number of parameters popped
    mulq StackValueSize, t1
    addq t1, sp
    pushQuad(r0)
    loadb IPInt::StructNewMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructNewMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_struct_new_default, macro()
    loadi IPInt::StructNewDefaultMetadata::type[MC], a1  # type
    operationCallMayThrow(macro() cCall2(_ipint_extern_struct_new_default) end)
    pushQuad(r0)
    loadb IPInt::StructNewDefaultMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructNewDefaultMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_struct_get, macro()
    popQuad(a1)  # object
    loadi IPInt::StructGetSetMetadata::fieldIndex[MC], a2  # field index
    subp StackValueSize, sp  # allocate space for result
    move sp, a3  # result location
    operationCallMayThrow(macro() cCall4(_ipint_extern_struct_get) end)

    loadb IPInt::StructGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_struct_get_s, macro()
    popQuad(a1)  # object
    loadi IPInt::StructGetSetMetadata::fieldIndex[MC], a2  # field index
    subp StackValueSize, sp  # allocate space for result
    move sp, a3  # result location
    operationCallMayThrow(macro() cCall4(_ipint_extern_struct_get_s) end)

    loadb IPInt::StructGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_struct_get_u, macro()
    popQuad(a1)  # object
    loadi IPInt::StructGetSetMetadata::fieldIndex[MC], a2  # field index
    subp StackValueSize, sp  # allocate space for result
    move sp, a3  # result location
    operationCallMayThrow(macro() cCall4(_ipint_extern_struct_get) end)

    loadb IPInt::StructGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_struct_set, macro()
    loadp StackValueSize[sp], a1  # object
    loadi IPInt::StructGetSetMetadata::fieldIndex[MC], a2  # field index
    move sp, a3
    operationCallMayThrow(macro() cCall4(_ipint_extern_struct_set) end)

    loadb IPInt::StructGetSetMetadata::length[MC], t0
    addp 2 * StackValueSize, sp
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new, macro()
    loadi IPInt::ArrayNewMetadata::type[MC], a1  # type
    popInt32(a2)  # length
    move sp, a3  # pointer to default value
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_new) end)
    addp StackValueSize, sp # pop default value

    pushQuad(r0)

    loadb IPInt::ArrayNewMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new_default, macro()
    loadi IPInt::ArrayNewMetadata::type[MC], a1  # type
    popInt32(a2)  # length
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_new_default) end)

    pushQuad(r0)

    loadb IPInt::ArrayNewMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new_fixed, macro()
    loadi IPInt::ArrayNewFixedMetadata::type[MC], a1  # type
    loadi IPInt::ArrayNewFixedMetadata::arraySize[MC], a2  # array length
    move sp, a3  # arguments
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_new_fixed) end)

    # pop all the arguments
    loadi IPInt::ArrayNewFixedMetadata::arraySize[MC], t3 # array length
    muli StackValueSize, t3
    addp t3, sp

    pushQuad(r0)

    loadb IPInt::ArrayNewFixedMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewFixedMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new_data, macro()
    move MC, a1  # metadata
    popInt32(a3)  # size
    popInt32(a2)  # offset
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_new_data) end)

    pushQuad(r0)

    loadb IPInt::ArrayNewDataMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewDataMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new_elem, macro()
    move MC, a1  # metadata
    popInt32(a3)  # size
    popInt32(a2)  # offset
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_new_elem) end)

    pushQuad(r0)

    loadb IPInt::ArrayNewElemMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewElemMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_get, macro()
    loadi IPInt::ArrayGetSetMetadata::type[MC], a1  # type
    move sp, a2 # all args on stack, result will be returned on stack
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_get) end)

    addp StackValueSize, sp # 2 args - 1 result

    loadb IPInt::ArrayGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_get_s, macro()
    loadi IPInt::ArrayGetSetMetadata::type[MC], a1  # type
    move sp, a2 # all args on stack, result will be returned on stack
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_get_s) end)

    addp StackValueSize, sp # 2 args - 1 result

    loadb IPInt::ArrayGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_get_u, macro()
    loadi IPInt::ArrayGetSetMetadata::type[MC], a1  # type
    move sp, a2 # all args on stack, result will be returned on stack
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_get) end)

    addp StackValueSize, sp # 2 args - 1 result

    loadb IPInt::ArrayGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_set, macro()
    loadi IPInt::ArrayGetSetMetadata::type[MC], a1  # type
    move sp, a2  # stack pointer with all the arguments
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_set) end)

    addq StackValueSize * 3, sp

    loadb IPInt::ArrayGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_len, macro()
    popQuad(t0)  # array into t0
    bqeq t0, ValueNull, .nullArray
    loadi JSWebAssemblyArray::m_size[t0], t0
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()

.nullArray:
    throwException(NullAccess)
end)

ipintOp(_array_fill, macro()
    move sp, a1
    operationCallMayThrow(macro() cCall2(_ipint_extern_array_fill) end)

    addp StackValueSize * 4, sp

    loadb IPInt::ArrayFillMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayFillMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_copy, macro()
    move sp, a1
    operationCallMayThrow(macro() cCall2(_ipint_extern_array_copy) end)

    addp StackValueSize * 5, sp

    loadb IPInt::ArrayFillMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayCopyMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_init_data, macro()
    loadi IPInt::ArrayInitDataMetadata::dataSegmentIndex[MC], a1
    move sp, a2
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_init_data) end)

    addp StackValueSize * 4, sp

    loadb IPInt::ArrayInitDataMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayInitDataMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_init_elem, macro()
    loadi IPInt::ArrayInitElemMetadata::elemSegmentIndex[MC], a1
    move sp, a2
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_init_elem) end)

    addp StackValueSize * 4, sp

    loadb IPInt::ArrayInitElemMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayInitElemMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_test, macro()
    loadi IPInt::RefTestCastMetadata::toHeapType[MC], a1
    move 0, a2  # allowNull
    popQuad(a3)
    operationCall(macro() cCall3(_ipint_extern_ref_test) end)

    pushInt32(r0)

    loadb IPInt::RefTestCastMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::RefTestCastMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_test_nullable, macro()
    loadi IPInt::RefTestCastMetadata::toHeapType[MC], a1
    move 1, a2  # allowNull
    popQuad(a3)
    operationCall(macro() cCall3(_ipint_extern_ref_test) end)

    pushInt32(r0)

    loadb IPInt::RefTestCastMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::RefTestCastMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_cast, macro()
    loadi IPInt::RefTestCastMetadata::toHeapType[MC], a1
    move 0, a2  # allowNull
    popQuad(a3)
    operationCallMayThrow(macro() cCall3(_ipint_extern_ref_cast) end)

    pushInt32(r0)

    loadb IPInt::RefTestCastMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::RefTestCastMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_cast_nullable, macro()
    loadi IPInt::RefTestCastMetadata::toHeapType[MC], a1
    move 1, a2  # allowNull
    popQuad(a3)
    operationCallMayThrow(macro() cCall3(_ipint_extern_ref_cast) end)

    pushInt32(r0)

    loadb IPInt::RefTestCastMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::RefTestCastMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_br_on_cast, macro()
    validateOpcodeConfig(a1)
    loadi IPInt::RefTestCastMetadata::toHeapType[MC], a1
    # fb 18 FLAGS
    loadb 2[PC], a2
    rshifti 1, a2  # bit 1 = null2
    loadq [sp], a3
    operationCall(macro() cCall3(_ipint_extern_ref_test) end)

    advanceMC(constexpr (sizeof(IPInt::RefTestCastMetadata)))

    bineq r0, 0, _ipint_br
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_br_on_cast_fail, macro()
    validateOpcodeConfig(a1)
    loadi IPInt::RefTestCastMetadata::toHeapType[MC], a1
    loadb 2[PC], a2
    # fb 19 FLAGS
    rshifti 1, a2  # bit 1 = null2
    loadq [sp], a3
    operationCall(macro() cCall3(_ipint_extern_ref_test) end)

    advanceMC(constexpr (sizeof(IPInt::RefTestCastMetadata)))

    bieq r0, 0, _ipint_br
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_any_convert_extern, macro()
    popQuad(a1)
    operationCall(macro() cCall2(_ipint_extern_any_convert_extern) end)
    pushQuad(r0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_extern_convert_any, macro()
    # do nothing
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_ref_i31, macro()
    popInt32(t0)
    lshifti 0x1, t0
    rshifti 0x1, t0
    orq TagNumber, t0
    pushQuad(t0)

    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_i31_get_s, macro()
    popQuad(t0)
    bqeq t0, ValueNull, .i31_get_throw
    pushInt32(t0)

    advancePC(2)
    nextIPIntInstruction()
.i31_get_throw:
    throwException(NullI31Get)
end)

ipintOp(_i31_get_u, macro()
    popQuad(t0)
    bqeq t0, ValueNull, .i31_get_throw
    andq 0x7fffffff, t0
    pushInt32(t0)

    advancePC(2)
    nextIPIntInstruction()
.i31_get_throw:
    throwException(NullI31Get)
end)

    #############################
    ## Conversion instructions ##
    #############################

ipintOp(_i32_trunc_sat_f32_s, macro()
    popFloat32(ft0)

    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMax

    truncatef2is ft0, t0
    pushInt32(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)
    jmp .end

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)
    jmp .end

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)
    jmp .end
end)

ipintOp(_i32_trunc_sat_f32_u, macro()
    popFloat32(ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMin

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMax

    truncatef2i ft0, t0
    pushInt32(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)
    jmp .end

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)
    jmp .end
end)

ipintOp(_i32_trunc_sat_f64_s, macro()
    popFloat64(ft0)

    move 0xc1e0000000200000, t0 # INT32_MIN - 1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax

    truncated2is ft0, t0
    pushInt32(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)
    jmp .end

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)
    jmp .end

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)
    jmp .end
end)

ipintOp(_i32_trunc_sat_f64_u, macro()
    popFloat64(ft0)

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin

    move 0x41f0000000000000, t0 # INT32_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax

    truncated2i ft0, t0
    pushInt32(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)
    jmp .end

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)
    jmp .end
end)

ipintOp(_i64_trunc_sat_f32_s, macro()
    popFloat32(ft0)

    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax

    truncatef2qs ft0, t0
    pushInt64(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt64(t0)
    jmp .end

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    pushInt64(t0)
    jmp .end

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMin:
    move (constexpr INT64_MIN), t0
    pushInt64(t0)
    jmp .end
end)

ipintOp(_i64_trunc_sat_f32_u, macro()
    popFloat32(ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMin

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMax

    truncatef2q ft0, t0
    pushInt64(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt64(t0)
    jmp .end

.ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    pushInt64(t0)
    jmp .end
end)

ipintOp(_i64_trunc_sat_f64_s, macro()
    popFloat64(ft0)
    move 0xc3e0000000000000, t0 # INT64_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN

    move 0x43e0000000000000, t0 # -INT64_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMax

    truncated2qs ft0, t0
    pushInt64(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt64(t0)
    jmp .end

.ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    pushInt64(t0)
    jmp .end

.ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMin:
    move (constexpr INT64_MIN), t0
    pushInt64(t0)
    jmp .end
end)

ipintOp(_i64_trunc_sat_f64_u, macro()
    popFloat64(ft0)

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMin

    move 0x43f0000000000000, t0 # INT64_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMax

    truncated2q ft0, t0
    pushInt64(t0)

.end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt64(t0)
    jmp .end

.ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    pushInt64(t0)
    jmp .end
end)

ipintOp(_memory_init, macro()
    # memory.init
    move sp, a2
    loadi 1[MC], a1
    operationCallMayThrow(macro() cCall3(_ipint_extern_memory_init) end)
    addq 3 * StackValueSize, sp
    loadb [MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata))) # xxx check
    nextIPIntInstruction()
end)

ipintOp(_data_drop, macro()
    # data.drop
    loadi 1[MC], a1
    operationCall(macro() cCall2(_ipint_extern_data_drop) end)
    loadb [MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata))) # xxx check
    nextIPIntInstruction()
end)

ipintOp(_memory_copy, macro()
    # memory.copy
    popQuad(a3) # n
    popQuad(a2) # s
    popQuad(a1) # d
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_copy) end)

    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_memory_fill, macro()
    # memory.fill
    popQuad(a3) # n
    popQuad(a2) # val
    popQuad(a1) # d
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_fill) end)

    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_init, macro()
    # table.init
    move sp, a1
    leap [MC], a2 # IPInt::tableInitMetadata
    operationCallMayThrow(macro() cCall3(_ipint_extern_table_init) end)
    addp 3 * StackValueSize, sp
    loadb IPInt::TableInitMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::TableInitMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_elem_drop, macro()
    # elem.drop
    loadi IPInt::Const32Metadata::value[MC], a1
    operationCall(macro() cCall2(_ipint_extern_elem_drop) end)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_copy, macro()
    # table.copy
    move sp, a1
    move MC, a2
    operationCallMayThrow(macro() cCall3(_ipint_extern_table_copy) end)
    addp 3 * StackValueSize, sp
    loadb IPInt::TableCopyMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::TableCopyMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_grow, macro()
    # table.grow
    move sp, a1
    move MC, a2 # IPInt::tableGrowMetadata
    operationCall(macro() cCall3(_ipint_extern_table_grow) end)
    addp StackValueSize * 2, sp
    pushQuad(r0)
    loadb IPInt::TableGrowMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::TableGrowMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_size, macro()
    # table.size
    loadi IPInt::Const32Metadata::value[MC], a1
    operationCall(macro() cCall2(_ipint_extern_table_size) end)
    pushQuad(r0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_fill, macro()
    # table.fill
    move sp, a1
    move MC, a2
    operationCallMayThrow(macro() cCall3(_ipint_extern_table_fill) end)
    addp 3 * StackValueSize, sp
    loadb IPInt::TableFillMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::TableFillMetadata)))
    nextIPIntInstruction()
end)

    #######################
    ## SIMD Instructions ##
    #######################

const ImmLaneIdxOffset = 2 # Offset in bytecode
const ImmLaneIdx16Mask = 0xf
const ImmLaneIdx8Mask = 0x7
const ImmLaneIdx4Mask = 0x3
const ImmLaneIdx2Mask = 0x1

# 0xFD 0x00 - 0xFD 0x0B: memory

# Wrapper for SIMD load/store operations. Places linear address in t0 for memOp()
macro simdMemoryOp(accessSize, memOp)
    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, accessSize)

    memOp()

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end

ipintOp(_simd_v128_load_mem, macro()
    # v128.load
    simdMemoryOp(16, macro()
        loadv [memoryBase, t0], v0
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load_8x8s_mem, macro()
    # v128.load8x8_s - load 8 8-bit values, sign-extend each to i16
    simdMemoryOp(8, macro()
        if ARM64 or ARM64E
            loadd [memoryBase, t0], ft0
            # offlineasm ft0 = ARM v0
            # offlineasm v0 = ARM v16
            emit "sxtl v16.8h, v0.8b"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "pmovsxbw (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load_8x8u_mem, macro()
    # v128.load8x8_u - load 8 8-bit values, zero-extend each to i16
    simdMemoryOp(8, macro()
        if ARM64 or ARM64E
            loadd [memoryBase, t0], ft0
            # offlineasm ft0 = ARM v0
            # offlineasm v0 = ARM v16
            emit "uxtl v16.8h, v0.8b"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "pmovzxbw (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load_16x4s_mem, macro()
    # v128.load16x4_s - load 4 16-bit values, sign-extend each to i32
    simdMemoryOp(8, macro()
        if ARM64 or ARM64E
            loadd [memoryBase, t0], ft0
            # offlineasm ft0 = ARM v0
            # offlineasm v0 = ARM v16
            emit "sxtl v16.4s, v0.4h"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "pmovsxwd (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load_16x4u_mem, macro()
    # v128.load16x4_u - load 4 16-bit values, zero-extend each to i32
    simdMemoryOp(8, macro()
        if ARM64 or ARM64E
            loadd [memoryBase, t0], ft0
            # offlineasm ft0 = ARM v0
            # offlineasm v0 = ARM v16
            emit "uxtl v16.4s, v0.4h"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "pmovzxwd (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load_32x2s_mem, macro()
    # v128.load32x2_s - load 2 32-bit values, sign-extend each to i64
    simdMemoryOp(8, macro()
        if ARM64 or ARM64E
            loadd [memoryBase, t0], ft0
            # offlineasm ft0 = ARM v0
            # offlineasm v0 = ARM v16
            emit "sxtl v16.2d, v0.2s"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "pmovsxdq (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load_32x2u_mem, macro()
    # v128.load32x2_u - load 2 32-bit values, zero-extend each to i64
    simdMemoryOp(8, macro()
        if ARM64 or ARM64E
            loadd [memoryBase, t0], ft0
            # offlineasm ft0 = ARM v0
            # offlineasm v0 = ARM v16
            emit "uxtl v16.2d, v0.2s"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "pmovzxdq (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load8_splat_mem, macro()
    # v128.load8_splat - load 1 8-bit value and splat to all 16 lanes
    simdMemoryOp(1, macro()
        if ARM64 or ARM64E
            loadb [memoryBase, t0], t1
            emit "dup v16.16b, w1"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "vpinsrb $0, (%r14,%rax), %xmm0, %xmm0"
            emit "vpxor %xmm1, %xmm1, %xmm1"
            emit "vpshufb %xmm1, %xmm0, %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load16_splat_mem, macro()
    # v128.load16_splat - load 1 16-bit value and splat to all 8 lanes
    simdMemoryOp(2, macro()
        if ARM64 or ARM64E
            loadh [memoryBase, t0], t1
            emit "dup v16.8h, w1"
        elsif X86_64
            # memoryBase is r14, t0 is eax
            emit "vpinsrw $0, (%r14,%rax), %xmm0, %xmm0"
            emit "vpshuflw $0, %xmm0, %xmm0"
            emit "vpunpcklqdq %xmm0, %xmm0, %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load32_splat_mem, macro()
    # v128.load32_splat - load 1 32-bit value and splat to all 4 lanes
    simdMemoryOp(4, macro()
        if ARM64 or ARM64E
            loadi [memoryBase, t0], t1
            emit "dup v16.4s, w1"
        elsif X86_64
            # Load and broadcast 32-bit value directly from memory to all 4 dwords
            # memoryBase is r14, t0 is eax
            emit "vbroadcastss (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_load64_splat_mem, macro()
    # v128.load64_splat - load 1 64-bit value and splat to all 2 lanes
    simdMemoryOp(8, macro()
        if ARM64 or ARM64E
            loadq [memoryBase, t0], t1
            emit "dup v16.2d, x1"
        elsif X86_64
            # Load and broadcast 64-bit value directly from memory to both qwords
            # memoryBase is r14, t0 is eax
            emit "vmovddup (%r14,%rax), %xmm0"
        else
            break # Not implemented
        end
        pushVec(v0)
    end)
end)

ipintOp(_simd_v128_store_mem, macro()
    # v128.store
    popVec(v0)
    simdMemoryOp(16, macro()
        storev v0, [memoryBase, t0]
    end)
end)

# 0xFD 0x0C: v128.const
ipintOp(_simd_v128_const, macro()
    # v128.const
    loadv 2[PC], v0
    pushVec(v0)
    advancePC(18)
    nextIPIntInstruction()
end)

# 0xFD 0x0D - 0xFD 0x14: splat (+ shuffle/swizzle)

ipintOp(_simd_i8x16_shuffle, macro()
    # i8x16.shuffle - shuffle bytes from two vectors using 16 immediate indices
    if ARM64 or ARM64E
        popVec(v1)
        popVec(v0)
        loadv ImmLaneIdxOffset[PC], v2
        emit "tbl v16.16b, {v16.16b, v17.16b}, v18.16b"
        pushVec(v0)
    else
        # X86_64 doesn't natively support shuffle so emulate it
        subp V128ISize, sp                # Allocate temp result

        # Loop through 16 output positions
        move 0, t0

    .shuffleLoop:
        loadb ImmLaneIdxOffset[PC, t0, 1], t1

        bigt t1, 31, .outOfBounds
        bigt t1, 15, .useRightVector

    .useLeftVector:
        loadb 32[sp, t1], t2
        jmp .storeByte

    .useRightVector:
        subq t1, 16, t3
        loadb 16[sp, t3], t2
        jmp .storeByte

    .outOfBounds:
        move 0, t2

    .storeByte:
        storeb t2, [sp, t0]               # Store to temp result
        addq 1, t0                        # Increment loop counter
        bilt t0, 16, .shuffleLoop

        # Copy temp result to final result location
        loadq [sp], t0
        loadq 8[sp], t1
        storeq t0, 32[sp]
        storeq t1, 40[sp]

        addp 2 * V128ISize, sp            # Pop temp result and right vector
    end

    advancePC(18)  # 2 bytes opcode + 16 bytes immediate
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_swizzle, macro()
    # i8x16.swizzle - swizzle bytes from first vector using indices from second vector
    popVec(v1)
    popVec(v0)

    if ARM64 or ARM64E
        emit "tbl v16.16b, {v16.16b}, v17.16b"
    elsif X86_64
        # vpshufb only checks bit 7 for out-of-bounds (returns 0 if bit 7 is set)
        # WebAssembly requires returning 0 for any index >= 16
        # Add 0x70 with unsigned saturation, so any index > 15 sets bit 7
        # (15 + 0x70 = 0x7F, anything > 15 saturates to 0xFF)
        # See BBQJIT::fixupOutOfBoundsIndicesForSwizzle
        emit "movabsq $0x7070707070707070, %rax"
        emit "vmovq %rax, %xmm2"
        emit "vpunpcklqdq %xmm2, %xmm2, %xmm2"   # xmm2 = [0x70, 0x70, ..., 0x70] (16 bytes)
        emit "vpaddusb %xmm2, %xmm1, %xmm1"      # Saturating add to set bit 7 for indices > 15
        emit "vpshufb %xmm1, %xmm0, %xmm0"       # Now vpshufb will return 0 for out-of-bounds
    else
        break # Not implemented
    end

    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_splat, macro()
    # i8x16.splat - splat i32 value to all 16 8-bit lanes
    popInt32(t0)

    if ARM64 or ARM64E
        emit "dup v16.16b, w0"
    elsif X86_64
        # t0 is eax on X86_64, move to xmm0 and broadcast to all 16 bytes
        emit "vmovd %eax, %xmm0"
        emit "vpinsrb $1, %eax, %xmm0, %xmm0"
        emit "vpshuflw $0, %xmm0, %xmm0"
        emit "vpunpcklqdq %xmm0, %xmm0, %xmm0"
    else
        break # Not implemented
    end

    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_splat, macro()
    # i16x8.splat - splat i32 value to all 8 16-bit lanes
    popInt32(t0)

    if ARM64 or ARM64E
        emit "dup v16.8h, w0"
    elsif X86_64
        # t0 is eax on X86_64, move to xmm0 and broadcast to all 8 words
        emit "vmovd %eax, %xmm0"
        emit "vpshuflw $0, %xmm0, %xmm0"
        emit "vpunpcklqdq %xmm0, %xmm0, %xmm0"
    else
        break # Not implemented
    end

    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_splat, macro()
    # i32x4.splat - splat i32 value to all 4 32-bit lanes
    popInt32(t0)

    if ARM64 or ARM64E
        emit "dup v16.4s, w0"
    elsif X86_64
        # t0 is eax on X86_64, move to xmm0 and broadcast to all 4 dwords
        emit "vmovd %eax, %xmm0"
        emit "vshufps $0, %xmm0, %xmm0, %xmm0"
    else
        break # Not implemented
    end

    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_splat, macro()
    # i64x2.splat - splat i64 value to all 2 64-bit lanes
    popInt64(t0)

    if ARM64 or ARM64E
        emit "dup v16.2d, x0"
    elsif X86_64
        # t0 is rax on X86_64
        emit "vmovq %rax, %xmm0"
        emit "vmovddup %xmm0, %xmm0"
    else
        break # Not implemented
    end

    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_splat, macro()
    # f32x4.splat - splat f32 value to all 4 32-bit float lanes
    popFloat32(ft0)

    if ARM64 or ARM64E
        emit "dup v16.4s, v0.s[0]"
    elsif X86_64
        # ft0 is xmm0 on X86_64, broadcast to all 4 float lanes
        emit "vshufps $0x00, %xmm0, %xmm0, %xmm0"
    else
        break # Not implemented
    end

    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_splat, macro()
    # f64x2.splat - splat f64 value to all 2 64-bit float lanes
    popFloat64(ft0)

    if ARM64 or ARM64E
        emit "dup v16.2d, v0.d[0]"
    elsif X86_64
        # ft0 is xmm0 on X86_64, duplicate lower 64-bit to both lanes
        emit "vmovddup %xmm0, %xmm0"
    else
        break # Not implemented
    end

    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x15 - 0xFD 0x22: extract and replace lanes
ipintOp(_simd_i8x16_extract_lane_s, macro()
    # i8x16.extract_lane_s (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx16Mask, t0
    loadbsi [sp, t0], t0
    addp V128ISize, sp
    pushInt32(t0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_extract_lane_u, macro()
    # i8x16.extract_lane_u (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx16Mask, t0
    loadb [sp, t0], t0
    addp V128ISize, sp
    pushInt32(t0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_replace_lane, macro()
    # i8x16.replace_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx16Mask, t0
    popInt32(t1)  # value to replace with
    storeb t1, [sp, t0]  # replace the byte at lane index
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extract_lane_s, macro()
    # i16x8.extract_lane_s (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx8Mask, t0
    loadhsi [sp, t0, 2], t0
    addp V128ISize, sp
    pushInt32(t0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extract_lane_u, macro()
    # i16x8.extract_lane_u (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx8Mask, t0
    loadh [sp, t0, 2], t0
    addp V128ISize, sp
    pushInt32(t0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_replace_lane, macro()
    # i16x8.replace_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx8Mask, t0
    popInt32(t1)  # value to replace with
    storeh t1, [sp, t0, 2]  # replace the 16-bit value at lane index
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extract_lane, macro()
    # i32x4.extract_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx4Mask, t0
    loadi [sp, t0, 4], t0
    addp V128ISize, sp
    pushInt32(t0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_replace_lane, macro()
    # i32x4.replace_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx4Mask, t0
    popInt32(t1)  # value to replace with
    storei t1, [sp, t0, 4]  # replace the 32-bit value at lane index
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extract_lane, macro()
    # i64x2.extract_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx2Mask, t0
    loadq [sp, t0, 8], t0
    addp V128ISize, sp
    pushInt64(t0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_replace_lane, macro()
    # i64x2.replace_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx2Mask, t0
    popInt64(t1)  # value to replace with
    storeq t1, [sp, t0, 8]  # replace the 64-bit value at lane index
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_extract_lane, macro()
    # f32x4.extract_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx4Mask, t0
    loadf [sp, t0, 4], ft0
    addp V128ISize, sp
    pushFloat32(ft0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_replace_lane, macro()
    # f32x4.replace_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx4Mask, t0
    popFloat32(ft0)  # value to replace with
    storef ft0, [sp, t0, 4]  # replace the 32-bit float at lane index
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_extract_lane, macro()
    # f64x2.extract_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx2Mask, t0
    loadd [sp, t0, 8], ft0
    addp V128ISize, sp
    pushFloat64(ft0)
    advancePC(3)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_replace_lane, macro()
    # f64x2.replace_lane (lane)
    loadb ImmLaneIdxOffset[PC], t0
    andi ImmLaneIdx2Mask, t0
    popFloat64(ft0)  # value to replace with
    stored ft0, [sp, t0, 8]  # replace the 64-bit float at lane index
    advancePC(3)
    nextIPIntInstruction()
end)

# 0xFD 0x23 - 0xFD 0x2C: i8x16 operations
ipintOp(_simd_i8x16_eq, macro()
    # i8x16.eq - compare 16 8-bit integers for equality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpcmpeqb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_ne, macro()
    # i8x16.ne - compare 16 8-bit integers for inequality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # Compare 16 bytes for equality, then invert the result
        emit "cmeq v16.16b, v16.16b, v17.16b"
        emit "mvn v16.16b, v16.16b"
    elsif X86_64
        # Compare for equality, then invert the result
        emit "vpcmpeqb %xmm1, %xmm0, %xmm0"
        emit "vpcmpeqb %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_lt_s, macro()
    # i8x16.lt_s - compare 16 8-bit signed integers for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmgt v17, v16 gives us v1 > v0, which is equivalent to v0 < v1
        emit "cmgt v16.16b, v17.16b, v16.16b"
    elsif X86_64
        # vpcmpgtb xmm1, xmm0 gives us xmm1 > xmm0, which is equivalent to xmm0 < xmm1
        emit "vpcmpgtb %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_lt_u, macro()
    # i8x16.lt_u - compare 16 8-bit unsigned integers for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmhi v17, v16 gives us v1 > v0 (unsigned), which is equivalent to v0 < v1
        emit "cmhi v16.16b, v17.16b, v16.16b"
    elsif X86_64
        # For unsigned comparison, we need to use min/max approach since there's no direct unsigned compare
        emit "vpminub %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqb %xmm0, %xmm2, %xmm2"  # xmm0 == min ? (xmm0 <= xmm1)
        emit "vpcmpeqb %xmm1, %xmm0, %xmm0"  # xmm0 == xmm1 ?
        emit "vpandn %xmm2, %xmm0, %xmm0"    # (xmm0 <= xmm1) && (xmm0 != xmm1) = (xmm0 < xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_gt_s, macro()
    # i8x16.gt_s - compare 16 8-bit signed integers for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmgt v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpcmpgtb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_gt_u, macro()
    # i8x16.gt_u - compare 16 8-bit unsigned integers for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmhi v16.16b, v16.16b, v17.16b"
    elsif X86_64
        # For unsigned comparison: xmm0 > xmm1 iff min(xmm0, xmm1) == xmm1 && xmm0 != xmm1
        emit "vpminub %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqb %xmm1, %xmm2, %xmm2"  # xmm1 == min ? (xmm1 <= xmm0)
        emit "vpcmpeqb %xmm1, %xmm0, %xmm0"  # xmm0 == xmm1 ?
        emit "vpandn %xmm2, %xmm0, %xmm0"    # (xmm1 <= xmm0) && (xmm0 != xmm1) = (xmm0 > xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_le_s, macro()
    # i8x16.le_s - compare 16 8-bit signed integers for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmge v17, v16 gives us v1 >= v0, which is equivalent to v0 <= v1
        emit "cmge v16.16b, v17.16b, v16.16b"
    elsif X86_64
        # xmm0 <= xmm1 iff !(xmm0 > xmm1)
        emit "vpcmpgtb %xmm1, %xmm0, %xmm0"  # xmm0 > xmm1
        emit "vpcmpeqb %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm0 > xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_le_u, macro()
    # i8x16.le_u - compare 16 8-bit unsigned integers for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmhs v17, v16 gives us v1 >= v0 (unsigned), which is equivalent to v0 <= v1
        emit "cmhs v16.16b, v17.16b, v16.16b"
    elsif X86_64
        # xmm0 <= xmm1 iff min(xmm0, xmm1) == xmm0
        emit "vpminub %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqb %xmm0, %xmm2, %xmm0"  # xmm0 == min ? (xmm0 <= xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_ge_s, macro()
    # i8x16.ge_s - compare 16 8-bit signed integers for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmge v16.16b, v16.16b, v17.16b"
    elsif X86_64
        # xmm0 >= xmm1 iff !(xmm0 < xmm1) iff !(xmm1 > xmm0)
        emit "vpcmpgtb %xmm0, %xmm1, %xmm0"  # xmm1 > xmm0
        emit "vpcmpeqb %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm1 > xmm0)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_ge_u, macro()
    # i8x16.ge_u - compare 16 8-bit unsigned integers for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmhs v16.16b, v16.16b, v17.16b"
    elsif X86_64
        # xmm0 >= xmm1 iff min(xmm0, xmm1) == xmm1
        emit "vpminub %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqb %xmm1, %xmm2, %xmm0"  # xmm1 == min ? (xmm1 <= xmm0) = (xmm0 >= xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x2D - 0xFD 0x36: i8x16 operations

ipintOp(_simd_i16x8_eq, macro()
    # i16x8.eq - compare 8 16-bit integers for equality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpcmpeqw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_ne, macro()
    # i16x8.ne - compare 8 16-bit integers for inequality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v16.8h, v16.8h, v17.8h"
        emit "mvn v16.16b, v16.16b"
    elsif X86_64
        # Compare for equality, then invert the result
        emit "vpcmpeqw %xmm1, %xmm0, %xmm0"
        emit "vpcmpeqw %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_lt_s, macro()
    # i16x8.lt_s - compare 8 16-bit signed integers for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmgt v17, v16 gives us v1 > v0, which is equivalent to v0 < v1
        emit "cmgt v16.8h, v17.8h, v16.8h"
    elsif X86_64
        # vpcmpgtw xmm1, xmm0 gives us xmm1 > xmm0, which is equivalent to xmm0 < xmm1
        emit "vpcmpgtw %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_lt_u, macro()
    # i16x8.lt_u - compare 8 16-bit unsigned integers for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmhi v17, v16 gives us v1 > v0 (unsigned), which is equivalent to v0 < v1
        emit "cmhi v16.8h, v17.8h, v16.8h"
    elsif X86_64
        # For unsigned comparison, we need to use min/max approach since there's no direct unsigned compare
        emit "vpminuw %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqw %xmm0, %xmm2, %xmm2"  # xmm0 == min ? (xmm0 <= xmm1)
        emit "vpcmpeqw %xmm1, %xmm0, %xmm0"  # xmm0 == xmm1 ?
        emit "vpandn %xmm2, %xmm0, %xmm0"    # (xmm0 <= xmm1) && (xmm0 != xmm1) = (xmm0 < xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_gt_s, macro()
    # i16x8.gt_s - compare 8 16-bit signed integers for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmgt v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpcmpgtw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_gt_u, macro()
    # i16x8.gt_u - compare 8 16-bit unsigned integers for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmhi v16.8h, v16.8h, v17.8h"
    elsif X86_64
        # For unsigned comparison: xmm0 > xmm1 iff min(xmm0, xmm1) == xmm1 && xmm0 != xmm1
        emit "vpminuw %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqw %xmm1, %xmm2, %xmm2"  # xmm1 == min ? (xmm1 <= xmm0)
        emit "vpcmpeqw %xmm1, %xmm0, %xmm0"  # xmm0 == xmm1 ?
        emit "vpandn %xmm2, %xmm0, %xmm0"    # (xmm1 <= xmm0) && (xmm0 != xmm1) = (xmm0 > xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_le_s, macro()
    # i16x8.le_s - compare 8 16-bit signed integers for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmge v17, v16 gives us v1 >= v0, which is equivalent to v0 <= v1
        emit "cmge v16.8h, v17.8h, v16.8h"
    elsif X86_64
        # xmm0 <= xmm1 iff !(xmm0 > xmm1)
        emit "vpcmpgtw %xmm1, %xmm0, %xmm0"  # xmm0 > xmm1
        emit "vpcmpeqw %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm0 > xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_le_u, macro()
    # i16x8.le_u - compare 8 16-bit unsigned integers for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmhs v17, v16 gives us v1 >= v0 (unsigned), which is equivalent to v0 <= v1
        emit "cmhs v16.8h, v17.8h, v16.8h"
    elsif X86_64
        # xmm0 <= xmm1 iff min(xmm0, xmm1) == xmm0
        emit "vpminuw %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqw %xmm0, %xmm2, %xmm0"  # xmm0 == min ? (xmm0 <= xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_ge_s, macro()
    # i16x8.ge_s - compare 8 16-bit signed integers for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmge v16.8h, v16.8h, v17.8h"
    elsif X86_64
        # xmm0 >= xmm1 iff !(xmm0 < xmm1) iff !(xmm1 > xmm0)
        emit "vpcmpgtw %xmm0, %xmm1, %xmm0"  # xmm1 > xmm0
        emit "vpcmpeqw %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm1 > xmm0)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_ge_u, macro()
    # i16x8.ge_u - compare 8 16-bit unsigned integers for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmhs v16.8h, v16.8h, v17.8h"
    elsif X86_64
        # xmm0 >= xmm1 iff min(xmm0, xmm1) == xmm1
        emit "vpminuw %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqw %xmm1, %xmm2, %xmm0"  # xmm1 == min ? (xmm1 <= xmm0) = (xmm0 >= xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x37 - 0xFD 0x40: i32x4 operations
ipintOp(_simd_i32x4_eq, macro()
    # i32x4.eq - compare 4 32-bit integers for equality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpcmpeqd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_ne, macro()
    # i32x4.ne - compare 4 32-bit integers for inequality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v16.4s, v16.4s, v17.4s"
        emit "mvn v16.16b, v16.16b"
    elsif X86_64
        # Compare for equality, then invert the result
        emit "vpcmpeqd %xmm1, %xmm0, %xmm0"
        emit "vpcmpeqd %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_lt_s, macro()
    # i32x4.lt_s - compare 4 32-bit signed integers for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmgt v17, v16 gives us v1 > v0, which is equivalent to v0 < v1
        emit "cmgt v16.4s, v17.4s, v16.4s"
    elsif X86_64
        # vpcmpgtd xmm1, xmm0 gives us xmm1 > xmm0, which is equivalent to xmm0 < xmm1
        emit "vpcmpgtd %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_lt_u, macro()
    # i32x4.lt_u - compare 4 32-bit unsigned integers for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmhi v17, v16 gives us v1 > v0 (unsigned), which is equivalent to v0 < v1
        emit "cmhi v16.4s, v17.4s, v16.4s"
    elsif X86_64
        # For unsigned comparison, we need to use min/max approach
        emit "vpminud %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqd %xmm0, %xmm2, %xmm2"  # xmm0 == min ? (xmm0 <= xmm1)
        emit "vpcmpeqd %xmm1, %xmm0, %xmm0"  # xmm0 == xmm1 ?
        emit "vpandn %xmm2, %xmm0, %xmm0"    # (xmm0 <= xmm1) && (xmm0 != xmm1) = (xmm0 < xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_gt_s, macro()
    # i32x4.gt_s - compare 4 32-bit signed integers for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmgt v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpcmpgtd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_gt_u, macro()
    # i32x4.gt_u - compare 4 32-bit unsigned integers for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmhi v16.4s, v16.4s, v17.4s"
    elsif X86_64
        # For unsigned comparison: xmm0 > xmm1 iff min(xmm0, xmm1) == xmm1 && xmm0 != xmm1
        emit "vpminud %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqd %xmm1, %xmm2, %xmm2"  # xmm1 == min ? (xmm1 <= xmm0)
        emit "vpcmpeqd %xmm1, %xmm0, %xmm0"  # xmm0 == xmm1 ?
        emit "vpandn %xmm2, %xmm0, %xmm0"    # (xmm1 <= xmm0) && (xmm0 != xmm1) = (xmm0 > xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_le_s, macro()
    # i32x4.le_s - compare 4 32-bit signed integers for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmge v17, v16 gives us v1 >= v0, which is equivalent to v0 <= v1
        emit "cmge v16.4s, v17.4s, v16.4s"
    elsif X86_64
        # xmm0 <= xmm1 iff !(xmm0 > xmm1)
        emit "vpcmpgtd %xmm1, %xmm0, %xmm0"  # xmm0 > xmm1
        emit "vpcmpeqd %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm0 > xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_le_u, macro()
    # i32x4.le_u - compare 4 32-bit unsigned integers for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmhs v17, v16 gives us v1 >= v0 (unsigned), which is equivalent to v0 <= v1
        emit "cmhs v16.4s, v17.4s, v16.4s"
    elsif X86_64
        # xmm0 <= xmm1 iff min(xmm0, xmm1) == xmm0
        emit "vpminud %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqd %xmm0, %xmm2, %xmm0"  # xmm0 == min ? (xmm0 <= xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_ge_s, macro()
    # i32x4.ge_s - compare 4 32-bit signed integers for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmge v16.4s, v16.4s, v17.4s"
    elsif X86_64
        # xmm0 >= xmm1 iff !(xmm0 < xmm1) iff !(xmm1 > xmm0)
        emit "vpcmpgtd %xmm0, %xmm1, %xmm0"  # xmm1 > xmm0
        emit "vpcmpeqd %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm1 > xmm0)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_ge_u, macro()
    # i32x4.ge_u - compare 4 32-bit unsigned integers for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmhs v16.4s, v16.4s, v17.4s"
    elsif X86_64
        # xmm0 >= xmm1 iff min(xmm0, xmm1) == xmm1
        emit "vpminud %xmm1, %xmm0, %xmm2"   # min(xmm0, xmm1) -> xmm2
        emit "vpcmpeqd %xmm1, %xmm2, %xmm0"  # xmm1 == min ? (xmm1 <= xmm0) = (xmm0 >= xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x41 - 0xFD 0x46: f32x4 operations
ipintOp(_simd_f32x4_eq, macro()
    # f32x4.eq - compare 4 32-bit floats for equality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmeq v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vcmpeqps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_ne, macro()
    # f32x4.ne - compare 4 32-bit floats for inequality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmeq v16.4s, v16.4s, v17.4s"
        emit "mvn v16.16b, v16.16b"
    elsif X86_64
        emit "vcmpneqps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_lt, macro()
    # f32x4.lt - compare 4 32-bit floats for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # fcmgt v17, v16 gives us v1 > v0, which is equivalent to v0 < v1
        emit "fcmgt v16.4s, v17.4s, v16.4s"
    elsif X86_64
        emit "vcmpltps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_gt, macro()
    # f32x4.gt - compare 4 32-bit floats for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmgt v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vcmpgtps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_le, macro()
    # f32x4.le - compare 4 32-bit floats for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # fcmge v17, v16 gives us v1 >= v0, which is equivalent to v0 <= v1
        emit "fcmge v16.4s, v17.4s, v16.4s"
    elsif X86_64
        emit "vcmpleps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_ge, macro()
    # f32x4.ge - compare 4 32-bit floats for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmge v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vcmpgeps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x47 - 0xFD 0x4c: f64x2 operations
ipintOp(_simd_f64x2_eq, macro()
    # f64x2.eq - compare 2 64-bit floats for equality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmeq v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vcmpeqpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_ne, macro()
    # f64x2.ne - compare 2 64-bit floats for inequality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmeq v16.2d, v16.2d, v17.2d"
        emit "mvn v16.16b, v16.16b"
    elsif X86_64
        emit "vcmpneqpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_lt, macro()
    # f64x2.lt - compare 2 64-bit floats for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # fcmgt v17, v16 gives us v1 > v0, which is equivalent to v0 < v1
        emit "fcmgt v16.2d, v17.2d, v16.2d"
    elsif X86_64
        emit "vcmpltpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_gt, macro()
    # f64x2.gt - compare 2 64-bit floats for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmgt v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vcmpgtpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_le, macro()
    # f64x2.le - compare 2 64-bit floats for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # fcmge v17, v16 gives us v1 >= v0, which is equivalent to v0 <= v1
        emit "fcmge v16.2d, v17.2d, v16.2d"
    elsif X86_64
        emit "vcmplepd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_ge, macro()
    # f64x2.ge - compare 2 64-bit floats for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcmge v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vcmpgepd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x4D - 0xFD 0x53: v128 operations

ipintOp(_simd_v128_not, macro()
    # v128.not - bitwise NOT of 128-bit vector
    popVec(v0)
    if ARM64 or ARM64E
        emit "mvn v16.16b, v16.16b"
    elsif X86_64
        emit "vpcmpeqb %xmm1, %xmm1, %xmm1"  # Set all bits to 1
        emit "vpxor %xmm1, %xmm0, %xmm0"     # Invert all bits
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_and, macro()
    # v128.and - bitwise AND of two 128-bit vectors
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "and v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpand %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_andnot, macro()
    # v128.andnot - bitwise AND NOT of two 128-bit vectors (v0 & ~v1)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "bic v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpandn %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_or, macro()
    # v128.or - bitwise OR of two 128-bit vectors
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "orr v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpor %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_xor, macro()
    # v128.xor - bitwise XOR of two 128-bit vectors
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "eor v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpxor %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_bitselect, macro()
    # v128.bitselect - bitwise select: (a & c) | (b & ~c)
    popVec(v2)  # selector c
    popVec(v1)  # b
    popVec(v0)  # a
    if ARM64 or ARM64E
        # Use BSL (Bit Select) instruction: bsl vd, vn, vm
        # BSL performs: vd = (vd & vn) | (~vd & vm)
        # We need: result = (a & c) | (b & ~c)
        # So we put c in the destination, then BSL with a and b
        emit "mov v18.16b, v18.16b"  # v2 -> v18 (selector)
        emit "bsl v18.16b, v16.16b, v17.16b"  # (c & a) | (~c & b)
        emit "mov v16.16b, v18.16b"  # result -> v0
    elsif X86_64
        emit "vpand %xmm2, %xmm0, %xmm3"     # xmm3 = a & c
        emit "vpandn %xmm1, %xmm2, %xmm2"    # xmm2 = b & ~c (vpandn does ~src1 & src2)
        emit "vpor %xmm2, %xmm3, %xmm0"      # xmm0 = (a & c) | (b & ~c)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_any_true, macro()
    # v128.any_true - return 1 if any bit is set, 0 otherwise
    popVec(v0)
    if ARM64 or ARM64E
        # Use UMAXV to find maximum across all bytes
        emit "umaxv b16, v16.16b"
        # Extract the result to general purpose register
        emit "fmov w0, s16"
        # Convert non-zero to 1
        emit "cmp w0, #0"
        emit "cset w0, ne"
    elsif X86_64
        emit "vptest %xmm0, %xmm0"
        emit "setne %al"                  # Set AL to 1 if ZF=0 (any bit set), 0 if ZF=1 (all zero)
        emit "movzbl %al, %eax"           # Zero-extend AL to EAX
    else
        break # Not implemented
    end
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x54 - 0xFD 0x5D: v128 load/store lane

ipintOp(_simd_v128_load8_lane_mem, macro()
    # v128.load8_lane - load 8-bit value from memory and replace lane in existing vector

    popVec(v0)
    popMemoryIndex(t0, t2)

    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    loadb [memoryBase, t0], t0

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t1
    advancePCByReg(t1)
    loadb -1[PC], t1
    andi ImmLaneIdx16Mask, t1

    # Push the result and then replace one lane of the result with the loaded value
    pushVec(v0)
    storeb t0, [sp, t1]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_load16_lane_mem, macro()
    # v128.load16_lane - load 16-bit value from memory and replace lane in existing vector

    popVec(v0)
    popMemoryIndex(t0, t2)

    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    loadh [memoryBase, t0], t0

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t1
    advancePCByReg(t1)
    loadb -1[PC], t1
    andi ImmLaneIdx8Mask, t1

    # Push the result and then replace one lane of the result with the loaded value
    pushVec(v0)
    storeh t0, [sp, t1, 2]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_load32_lane_mem, macro()
    # v128.load32_lane - load 32-bit value from memory and replace lane in existing vector

    popVec(v0)
    popMemoryIndex(t0, t2)

    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    loadi [memoryBase, t0], t0

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t1
    advancePCByReg(t1)
    loadb -1[PC], t1
    andi ImmLaneIdx4Mask, t1

    # Push the result and then replace one lane of the result with the loaded value
    pushVec(v0)
    storei t0, [sp, t1, 4]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_load64_lane_mem, macro()
    # v128.load64_lane - load 64-bit value from memory and replace lane in existing vector

    popVec(v0)
    popMemoryIndex(t0, t2)

    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    loadq [memoryBase, t0], t0

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t1
    advancePCByReg(t1)
    loadb -1[PC], t1
    andi ImmLaneIdx2Mask, t1

    # Push the result and then replace one lane of the result with the loaded value
    pushVec(v0)
    storeq t0, [sp, t1, 8]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_store8_lane_mem, macro()
    # v128.store8_lane - extract 8-bit value from lane and store to memory

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    loadb -1[PC], t1
    andi ImmLaneIdx16Mask, t1

    loadb [sp, t1], t1  # Load value from lane in vector on stack
    addp V128ISize, sp  # Pop the vector

    popMemoryIndex(t0, t2)

    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 1)

    storeb t1, [memoryBase, t0]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_store16_lane_mem, macro()
    # v128.store16_lane - extract 16-bit value from lane and store to memory

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    loadb -1[PC], t1
    andi ImmLaneIdx8Mask, t1

    loadh [sp, t1, 2], t1   # Load value from lane in vector on stack
    addp V128ISize, sp      # Pop the vector

    popMemoryIndex(t0, t2)

    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 2)

    storeh t1, [memoryBase, t0]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_store32_lane_mem, macro()
    # v128.store32_lane - extract 32-bit value from lane and store to memory

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    loadb -1[PC], t1
    andi ImmLaneIdx4Mask, t1

    loadi [sp, t1, 4], t1   # Load value from lane in vector on stack
    addp V128ISize, sp      # Pop the vector

    popMemoryIndex(t0, t2)

    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 4)

    storei t1, [memoryBase, t0]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_store64_lane_mem, macro()
    # v128.store64_lane - extract 64-bit value from lane and store to memory

    # The lane index comes after the variable length memory offset, so find it by
    # advancing the PC and loading the byte before the next instruction.
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    loadb -1[PC], t1
    andi ImmLaneIdx2Mask, t1

    loadq [sp, t1, 8], t1   # Load value from lane in vector on stack
    addp V128ISize, sp      # Pop the vector

    popMemoryIndex(t0, t2)
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    ipintCheckMemoryBound(t0, t2, 8)

    storeq t1, [memoryBase, t0]

    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_simd_v128_load32_zero_mem, macro()
    # v128.load32_zero - load 32-bit value from memory and zero-pad to 128 bits
    simdMemoryOp(4, macro()
        loadi [memoryBase, t0], t0

        subp V128ISize, sp
        storei t0, [sp]
        storei 0, 4[sp]
        storeq 0, 8[sp]
    end)
end)

ipintOp(_simd_v128_load64_zero_mem, macro()
    # v128.load64_zero - load 64-bit value from memory and zero-pad to 128 bits
    simdMemoryOp(8, macro()
        loadq [memoryBase, t0], t0

        subp V128ISize, sp
        storeq t0, [sp]
        storeq 0, 8[sp]
    end)
end)

# 0xFD 0x5E - 0xFD 0x5F: f32x4/f64x2 conversion

ipintOp(_simd_f32x4_demote_f64x2_zero, macro()
    # f32x4.demote_f64x2_zero - demote 2 f64 values to f32, zero upper 2 lanes
    popVec(v0)
    if ARM64 or ARM64E
        # Convert the two f64 values in lanes 0,1 to f32 and store in lanes 0,1
        emit "fcvtn v16.2s, v16.2d"
        # Zero the upper 64 bits (lanes 2,3)
        emit "mov v16.d[1], xzr"
    elsif X86_64
        emit "vcvtpd2ps %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_promote_low_f32x4, macro()
    # f64x2.promote_low_f32x4 - promote lower 2 f32 values to f64
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcvtl v16.2d, v16.2s"
    elsif X86_64
        emit "vcvtps2pd %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x60 - 0x66: i8x16 operations

ipintOp(_simd_i8x16_abs, macro()
    # i8x16.abs - absolute value of 16 8-bit signed integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "abs v16.16b, v16.16b"
    elsif X86_64
        emit "vpabsb %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_neg, macro()
    # i8x16.neg - negate 16 8-bit integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "neg v16.16b, v16.16b"
    elsif X86_64
        # Negate by subtracting from zero
        emit "vpxor %xmm1, %xmm1, %xmm1"
        emit "vpsubb %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_popcnt, macro()
    # i8x16.popcnt - population count (count set bits) for 16 8-bit integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "cnt v16.16b, v16.16b"
    elsif X86_64
        # x86_64 does not natively support vector lanewise popcount, so we emulate it using
        # lookup tables, similar to BBQ JIT implementation

        # Create bottom nibble mask (0x0f repeated 16 times)
        emit "movabsq $0x0f0f0f0f0f0f0f0f, %rax"
        emit "vmovq %rax, %xmm1"
        emit "vmovq %rax, %xmm4"
        emit "vpunpcklqdq %xmm4, %xmm1, %xmm1"  # xmm1 = bottom nibble mask

        # Create popcount lookup table
        emit "movabsq $0x0302020102010100, %rax"   # Low 64 bits of lookup table
        emit "vmovq %rax, %xmm2"
        emit "movabsq $0x0403030203020201, %rax"   # High 64 bits of lookup table
        emit "vmovq %rax, %xmm4"
        emit "vpunpcklqdq %xmm4, %xmm2, %xmm2"  # xmm2 = popcount lookup table

        # Split input into low and high nibbles
        emit "vmovdqa %xmm0, %xmm3"              # xmm3 = copy of input
        emit "vpand %xmm1, %xmm0, %xmm0"         # xmm0 = low nibbles (input & mask)
        emit "vpsrlw $4, %xmm3, %xmm3"           # Shift right 4 bits
        emit "vpand %xmm1, %xmm3, %xmm3"         # xmm3 = high nibbles ((input >> 4) & mask)

        # Lookup popcount for both nibbles using pshufb
        emit "vpshufb %xmm0, %xmm2, %xmm0"       # Lookup low nibbles
        emit "vpshufb %xmm3, %xmm2, %xmm3"       # Lookup high nibbles

        # Add the results
        emit "vpaddb %xmm3, %xmm0, %xmm0"        # Add popcount of low and high nibbles
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_all_true, macro()
    # i8x16.all_true - return 1 if all 16 8-bit lanes are non-zero, 0 otherwise
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v17.16b, v16.16b, #0"  # Compare each lane with 0
        emit "umaxv b17, v17.16b"         # Find maximum (any zero lane will make this non-zero)
        emit "fmov w0, s17"               # Move to general register
        emit "cmp w0, #0"                 # Compare with 0
        emit "cset w0, eq"                # Set to 1 if equal (all lanes non-zero), 0 otherwise
    elsif X86_64
        # Compare each byte with zero to create mask of zero lanes
        emit "vpxor %xmm1, %xmm1, %xmm1"      # Create zero vector
        emit "vpcmpeqb %xmm1, %xmm0, %xmm0"   # Compare each byte with 0 (0xFF if zero, 0x00 if non-zero)
        emit "vpmovmskb %xmm0, %eax"          # Extract sign bits to create 16-bit mask
        emit "test %eax, %eax"                # Test if any bit is set (any lane was zero)
        emit "sete %al"                       # Set AL to 1 if no bits set (all lanes non-zero), 0 otherwise
        emit "movzbl %al, %eax"               # Zero-extend to full 32-bit register
    else
        break # Not implemented
    end
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_bitmask, macro()
    # i8x16.bitmask - extract most significant bit from each 8-bit lane into a 16-bit integer
    # Simple loop over the 16 bytes on the stack

    move 0, t0          # Initialize result
    move 0, t3          # Byte counter

.bitmask_i8x16_loop:
    # Load byte and check sign bit
    loadb [sp, t3], t1
    andq 0x80, t1       # Extract sign bit
    btiz t1, .bitmask_i8x16_next

    # Set corresponding bit in result
    move 1, t1
    lshiftq t3, t1      # Shift to bit position
    orq t1, t0

.bitmask_i8x16_next:
    addq 1, t3          # Next byte
    bilt t3, 16, .bitmask_i8x16_loop

    addp V128ISize, sp  # Pop the vector
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_narrow_i16x8_s, macro()
    # i8x16.narrow_i16x8_s - narrow 2 i16x8 vectors to 1 i8x16 vector with signed saturation
    popVec(v1)  # Second operand
    popVec(v0)  # First operand
    if ARM64 or ARM64E
        # Signed saturating extract narrow: combine v0.8h and v1.8h into v16.16b
        emit "sqxtn v16.8b, v16.8h"    # Narrow first vector (v0) to lower 8 bytes
        emit "sqxtn2 v16.16b, v17.8h"  # Narrow second vector (v1) to upper 8 bytes
    elsif X86_64
        emit "vpacksswb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_narrow_i16x8_u, macro()
    # i8x16.narrow_i16x8_u - narrow 2 i16x8 vectors to 1 i8x16 vector with unsigned saturation
    popVec(v1)  # Second operand
    popVec(v0)  # First operand
    if ARM64 or ARM64E
        # Signed saturate extract unsigned narrow: combine v0.8h and v1.8h into v16.16b
        emit "sqxtun v16.8b, v16.8h"    # Narrow first vector (v0) to lower 8 bytes
        emit "sqxtun2 v16.16b, v17.8h"  # Narrow second vector (v1) to upper 8 bytes
    elsif X86_64
        emit "vpackuswb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x67 - 0xFD 0x6A: f32x4 operations

ipintOp(_simd_f32x4_ceil, macro()
    # f32x4.ceil - ceiling of 4 32-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintp v16.4s, v16.4s"
    elsif X86_64
        emit "vroundps $0x2, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_floor, macro()
    # f32x4.floor - floor of 4 32-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintm v16.4s, v16.4s"
    elsif X86_64
        emit "vroundps $0x1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_trunc, macro()
    # f32x4.trunc - truncate 4 32-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintz v16.4s, v16.4s"
    elsif X86_64
        emit "vroundps $0x3, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_nearest, macro()
    # f32x4.nearest - round to nearest integer (ties to even) for 4 32-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintn v16.4s, v16.4s"
    elsif X86_64
        emit "vroundps $0x0, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x6B - 0xFD 0x73: i8x16 binary operations

ipintOp(_simd_i8x16_shl, macro()
    # i8x16.shl - left shift 16 8-bit integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-7 range for 8-bit elements
        andi 7, t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.16b, w0"
        # Perform left shift
        emit "ushl v16.16b, v16.16b, v17.16b"
    elsif X86_64
        andi 7, t0
        emit "movd %eax, %xmm1"

        # See MacroAssemblerX86_64::vectorUshl8()

        # Unpack and zero-extend low input bytes to words
        emit "vxorps %xmm3, %xmm3, %xmm3"
        emit "vpunpcklbw %xmm3, %xmm0, %xmm2"

        # Word-wise shift low input bytes
        emit "vpsllw %xmm1, %xmm2, %xmm2"

        # Unpack and zero-extend high input bytes to words
        emit "vpunpckhbw %xmm3, %xmm0, %xmm3"

        # Word-wise shift high input bytes
        emit "vpsllw %xmm1, %xmm3, %xmm3"

        # Mask away higher bits of left-shifted results
        emit "vpsllw $8, %xmm2, %xmm2"
        emit "vpsllw $8, %xmm3, %xmm3"
        emit "vpsrlw $8, %xmm2, %xmm2"
        emit "vpsrlw $8, %xmm3, %xmm3"

        # Pack low and high results back to bytes
        emit "vpackuswb %xmm3, %xmm2, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_shr_s, macro()
    # i8x16.shr_s - arithmetic right shift 16 8-bit signed integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-7 range for 8-bit elements
        andi 7, t0
        # Negate for right shift
        negi t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.16b, w0"
        # Perform arithmetic right shift
        emit "sshl v16.16b, v16.16b, v17.16b"
    elsif X86_64
        andi 7, t0
        emit "movd %eax, %xmm1"

        # See MacroAssemblerX86_64::vectorSshr8()

        # Unpack and sign-extend low input bytes to words
        emit "vpmovsxbw %xmm0, %xmm2"

        # Word-wise shift low input bytes
        emit "vpsraw %xmm1, %xmm2, %xmm2"

        # Unpack and sign-extend high input bytes
        emit "vpshufd $0x0e, %xmm0, %xmm3"  # Move high 8 bytes to low position
        emit "vpmovsxbw %xmm3, %xmm3"

        # Word-wise shift high input bytes
        emit "vpsraw %xmm1, %xmm3, %xmm3"

        # Pack low and high results back to signed bytes
        emit "vpacksswb %xmm3, %xmm2, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_shr_u, macro()
    # i8x16.shr_u - logical right shift 16 8-bit unsigned integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-7 range for 8-bit elements
        andi 7, t0
        # Negate for right shift
        negi t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.16b, w0"
        # Perform logical right shift
        emit "ushl v16.16b, v16.16b, v17.16b"
    elsif X86_64
        andi 7, t0
        emit "movd %eax, %xmm1"

        # See MacroAssemblerX86_64::vectorUshr8()

        # Unpack and zero-extend low input bytes to words
        emit "vxorps %xmm3, %xmm3, %xmm3"
        emit "vpunpcklbw %xmm3, %xmm0, %xmm2"

        # Word-wise shift low input bytes
        emit "vpsrlw %xmm1, %xmm2, %xmm2"

        # Unpack and zero-extend high input bytes to words
        emit "vpunpckhbw %xmm3, %xmm0, %xmm3"

        # Word-wise shift high input bytes
        emit "vpsrlw %xmm1, %xmm3, %xmm3"

        # Pack low and high results back to unsigned bytes
        emit "vpackuswb %xmm3, %xmm2, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_add, macro()
    # i8x16.add - add 16 8-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "add v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpaddb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_add_sat_s, macro()
    # i8x16.add_sat_s - add 16 8-bit signed integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sqadd v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpaddsb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_add_sat_u, macro()
    # i8x16.add_sat_u - add 16 8-bit unsigned integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "uqadd v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpaddusb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_sub, macro()
    # i8x16.sub - subtract 16 8-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sub v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpsubb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_sub_sat_s, macro()
    # i8x16.sub_sat_s - subtract 16 8-bit signed integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sqsub v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpsubsb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_sub_sat_u, macro()
    # i8x16.sub_sat_u - subtract 16 8-bit unsigned integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "uqsub v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpsubusb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x74 - 0xFD 0x75: f64x2 operations

ipintOp(_simd_f64x2_ceil, macro()
    # f64x2.ceil - ceiling of 2 64-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintp v16.2d, v16.2d"
    elsif X86_64
        emit "vroundpd $0x2, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_floor, macro()
    # f64x2.floor - floor of 2 64-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintm v16.2d, v16.2d"
    elsif X86_64
        emit "vroundpd $0x1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x76 - 0xFD 0x79: i8x16 binary operations
ipintOp(_simd_i8x16_min_s, macro()
    # i8x16.min_s - minimum of 16 8-bit signed integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smin v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpminsb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_min_u, macro()
    # i8x16.min_u - minimum of 16 8-bit unsigned integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umin v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpminub %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_max_s, macro()
    # i8x16.max_s - maximum of 16 8-bit signed integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smax v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpmaxsb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i8x16_max_u, macro()
    # i8x16.max_u - maximum of 16 8-bit unsigned integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umax v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpmaxub %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x7A: f64x2 trunc

ipintOp(_simd_f64x2_trunc, macro()
    # f64x2.trunc - truncate 2 64-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintz v16.2d, v16.2d"
    elsif X86_64
        emit "vroundpd $0x3, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x7B: i8x16 avgr_u

ipintOp(_simd_i8x16_avgr_u, macro()
    # i8x16.avgr_u - average of 16 8-bit unsigned integers with rounding
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "urhadd v16.16b, v16.16b, v17.16b"
    elsif X86_64
        emit "vpavgb %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x7C - 0xFD 0x7F: extadd_pairwise

ipintOp(_simd_i16x8_extadd_pairwise_i8x16_s, macro()
    # i16x8.extadd_pairwise_i8x16_s - pairwise addition of signed 8-bit integers to 16-bit
    popVec(v0)
    if ARM64 or ARM64E
        emit "saddlp v16.8h, v16.16b"
    elsif X86_64
        emit "vpcmpeqd %xmm1, %xmm1, %xmm1"   # Set all bits to 1
        emit "vpsrlw $15, %xmm1, %xmm1"       # Shift to get 0x0001 in each 16-bit lane
        emit "vpackuswb %xmm1, %xmm1, %xmm1"  # Pack to get 0x01 in each 8-bit lane
        emit "vpmaddubsw %xmm0, %xmm1, %xmm0" # Pairwise multiply-add (signed)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extadd_pairwise_i8x16_u, macro()
    # i16x8.extadd_pairwise_i8x16_u - pairwise addition of unsigned 8-bit integers to 16-bit
    popVec(v0)
    if ARM64 or ARM64E
        emit "uaddlp v16.8h, v16.16b"
    elsif X86_64
        emit "vpcmpeqd %xmm1, %xmm1, %xmm1"   # Set all bits to 1
        emit "vpsrlw $15, %xmm1, %xmm1"       # Shift to get 0x0001 in each 16-bit lane
        emit "vpackuswb %xmm1, %xmm1, %xmm1"  # Pack to get 0x01 in each 8-bit lane
        emit "vpmaddubsw %xmm1, %xmm0, %xmm0" # Pairwise multiply-add (unsigned)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extadd_pairwise_i16x8_s, macro()
    # i32x4.extadd_pairwise_i16x8_s - pairwise addition of signed 16-bit integers to 32-bit
    popVec(v0)
    if ARM64 or ARM64E
        emit "saddlp v16.4s, v16.8h"
    elsif X86_64
        emit "vpcmpeqd %xmm1, %xmm1, %xmm1"   # Set all bits to 1
        emit "vpsrld $31, %xmm1, %xmm1"       # Shift to get 0x00000001 in each 32-bit lane
        emit "vpackssdw %xmm1, %xmm1, %xmm1"  # Pack to get 0x0001 in each 16-bit lane
        emit "vpmaddwd %xmm0, %xmm1, %xmm0"   # Pairwise multiply-add
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extadd_pairwise_i16x8_u, macro()
    # i32x4.extadd_pairwise_i16x8_u - pairwise addition of unsigned 16-bit integers to 32-bit
    popVec(v0)
    if ARM64 or ARM64E
        emit "uaddlp v16.4s, v16.8h"
    elsif X86_64
        emit "vpsrld $16, %xmm0, %xmm1"            # Shift right to get high 16-bits in low position
        emit "vpblendw $0xAA, %xmm1, %xmm0, %xmm0" # Blend: keep low 16-bits from src, high 16-bits from shifted
        emit "vpaddd %xmm1, %xmm0, %xmm0"          # Add the pairs
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x80 0x01 - 0xFD 0x93 0x01: i16x8 operations

ipintOp(_simd_i16x8_abs, macro()
    # i16x8.abs - absolute value of 8 16-bit signed integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "abs v16.8h, v16.8h"
    elsif X86_64
        emit "vpabsw %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_neg, macro()
    # i16x8.neg - negate 8 16-bit integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "neg v16.8h, v16.8h"
    elsif X86_64
        # Negate by subtracting from zero
        emit "vpxor %xmm1, %xmm1, %xmm1"
        emit "vpsubw %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_q15mulr_sat_s, macro()
    # i16x8.q15mulr_sat_s - Q15 multiply with rounding and saturation
    # Q15 format: multiply two 16-bit values, shift right by 15, round and saturate
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sqrdmulh v16.8h, v16.8h, v17.8h"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulSat
        emit "vpmulhrsw %xmm1, %xmm0, %xmm0"        # Q15 multiply with rounding
        emit "mov $0x8000, %eax"                    # Load -32768 (0x8000)
        emit "vmovd %eax, %xmm2"                    # Move to XMM register
        emit "vpshuflw $0x00, %xmm2, %xmm2"         # Splat to low 4 words
        emit "vpshufd $0x00, %xmm2, %xmm2"          # Splat to all 8 words
        emit "vpcmpeqw %xmm2, %xmm0, %xmm2"         # Compare result with -32768
        emit "vpxor %xmm2, %xmm0, %xmm0"            # Fix saturation: -32768 becomes 32767
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_all_true, macro()
    # i16x8.all_true - return 1 if all 8 16-bit lanes are non-zero, 0 otherwise
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v17.8h, v16.8h, #0"   # Compare each lane with 0
        emit "umaxv h17, v17.8h"         # Find maximum (any zero lane will make this non-zero)
        emit "fmov w0, s17"              # Move to general register
        emit "cmp w0, #0"                # Compare with 0
        emit "cset w0, eq"               # Set to 1 if equal (all lanes non-zero), 0 otherwise
    elsif X86_64
        # Compare each 16-bit lane with zero
        emit "vpxor %xmm1, %xmm1, %xmm1"     # Create zero vector
        emit "vpcmpeqw %xmm1, %xmm0, %xmm1"  # Compare each word with 0 (1 if zero, 0 if non-zero)

        # Test if any lane is zero
        emit "vpmovmskb %xmm1, %eax"         # Extract sign bits
        emit "testl %eax, %eax"              # Test if any bits are set
        emit "sete %al"                      # Set AL to 1 if no bits set (all lanes non-zero), 0 otherwise
        emit "movzbl %al, %eax"              # Zero-extend to 32-bit
    else
        break # Not implemented
    end
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_bitmask, macro()
    # i16x8.bitmask - extract most significant bit from each 16-bit lane into an 8-bit integer
    # Simple loop over the 8 16-bit values on the stack

    move 0, t0          # Initialize result
    move 0, t3          # Lane counter

.bitmask_i16x8_loop:
    # Load 16-bit value and check sign bit
    loadh [sp, t3, 2], t1  # Load 16-bit value at offset t1*2
    andq 0x8000, t1     # Extract sign bit (bit 15)
    btiz t1, .bitmask_i16x8_next

    # Set corresponding bit in result
    move 1, t1
    lshiftq t3, t1      # Shift to bit position
    orq t1, t0

.bitmask_i16x8_next:
    addq 1, t3          # Next lane
    bilt t3, 8, .bitmask_i16x8_loop

    addp V128ISize, sp  # Pop the vector
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_narrow_i32x4_s, macro()
    # i16x8.narrow_i32x4_s - narrow 2 i32x4 vectors to 1 i16x8 vector with signed saturation
    popVec(v1)  # Second operand
    popVec(v0)  # First operand
    if ARM64 or ARM64E
        # Signed saturating extract narrow: combine v0.4s and v1.4s into v16.8h
        emit "sqxtn v16.4h, v16.4s"    # Narrow first vector (v0) to lower 4 halfwords
        emit "sqxtn2 v16.8h, v17.4s"   # Narrow second vector (v1) to upper 4 halfwords
    elsif X86_64
        emit "vpackssdw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_narrow_i32x4_u, macro()
    # i16x8.narrow_i32x4_u - narrow 2 i32x4 vectors to 1 i16x8 vector with unsigned saturation
    popVec(v1)  # Second operand
    popVec(v0)  # First operand
    if ARM64 or ARM64E
        # Signed saturate extract unsigned narrow: combine v0.4s and v1.4s into v16.8h
        emit "sqxtun v16.4h, v16.4s"    # Narrow first vector (v0) to lower 4 halfwords
        emit "sqxtun2 v16.8h, v17.4s"   # Narrow second vector (v1) to upper 4 halfwords
    elsif X86_64
        emit "vpackusdw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extend_low_i8x16_s, macro()
    # i16x8.extend_low_i8x16_s - sign-extend lower 8 i8 values to i16
    popVec(v0)
    if ARM64 or ARM64E
        emit "sxtl v16.8h, v16.8b"
    elsif X86_64
        emit "vpmovsxbw %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extend_high_i8x16_s, macro()
    # i16x8.extend_high_i8x16_s - sign-extend upper 8 i8 values to i16
    popVec(v0)
    if ARM64 or ARM64E
        emit "sxtl2 v16.8h, v16.16b"
    elsif X86_64
        # Move high 64 bits to low, then sign extend
        emit "vpsrldq $8, %xmm0, %xmm0"   # Shift right 8 bytes to get high half
        emit "vpmovsxbw %xmm0, %xmm0"     # Sign extend
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extend_low_i8x16_u, macro()
    # i16x8.extend_low_i8x16_u - zero-extend lower 8 i8 values to i16
    popVec(v0)
    if ARM64 or ARM64E
        emit "uxtl v16.8h, v16.8b"
    elsif X86_64
        emit "vpmovzxbw %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extend_high_i8x16_u, macro()
    # i16x8.extend_high_i8x16_u - zero-extend upper 8 i8 values to i16
    popVec(v0)
    if ARM64 or ARM64E
        emit "uxtl2 v16.8h, v16.16b"
    elsif X86_64
        # Move high 64 bits to low, then zero extend
        emit "vpsrldq $8, %xmm0, %xmm0"   # Shift right 8 bytes to get high half
        emit "vpmovzxbw %xmm0, %xmm0"     # Zero extend
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_shl, macro()
    # i16x8.shl - left shift 8 16-bit integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-15 range for 16-bit elements
        andi 15, t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.8h, w0"
        # Perform left shift
        emit "ushl v16.8h, v16.8h, v17.8h"
    elsif X86_64
        # Mask shift count to 0-15 range for 16-bit elements
        andi 15, t0
        emit "movd %eax, %xmm1"
        # Perform left shift on 16-bit words
        emit "vpsllw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_shr_s, macro()
    # i16x8.shr_s - arithmetic right shift 8 16-bit signed integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-15 range for 16-bit elements
        andi 15, t0
        # Negate for right shift
        negi t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.8h, w0"
        # Perform arithmetic right shift
        emit "sshl v16.8h, v16.8h, v17.8h"
    elsif X86_64
        # Mask shift count to 0-15 range for 16-bit elements
        andi 15, t0
        emit "movd %eax, %xmm1"
        # Perform arithmetic right shift on 16-bit words
        emit "vpsraw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_shr_u, macro()
    # i16x8.shr_u - logical right shift 8 16-bit unsigned integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-15 range for 16-bit elements
        andi 15, t0
        # Negate for right shift
        negi t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.8h, w0"
        # Perform logical right shift
        emit "ushl v16.8h, v16.8h, v17.8h"
    elsif X86_64
        andi 15, t0
        emit "movd %eax, %xmm1"
        emit "vpsrlw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_add, macro()
    # i16x8.add - add 8 16-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "add v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpaddw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_add_sat_s, macro()
    # i16x8.add_sat_s - add 8 16-bit signed integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sqadd v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpaddsw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_add_sat_u, macro()
    # i16x8.add_sat_u - add 8 16-bit unsigned integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "uqadd v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpaddusw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_sub, macro()
    # i16x8.sub - subtract 8 16-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sub v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpsubw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_sub_sat_s, macro()
    # i16x8.sub_sat_s - subtract 8 16-bit signed integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sqsub v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpsubsw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_sub_sat_u, macro()
    # i16x8.sub_sat_u - subtract 8 16-bit unsigned integers with saturation
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "uqsub v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpsubusw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x94 0x01: f64x2.nearest

ipintOp(_simd_f64x2_nearest, macro()
    # f64x2.nearest - round to nearest integer (ties to even) for 2 64-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "frintn v16.2d, v16.2d"
    elsif X86_64
        emit "vroundpd $0x0, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0x95 0x01 - 0xFD 0x9F 0x01: i16x8 operations

ipintOp(_simd_i16x8_mul, macro()
    # i16x8.mul - multiply 8 16-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "mul v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpmullw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_min_s, macro()
    # i16x8.min_s - minimum of 8 16-bit signed integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smin v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpminsw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_min_u, macro()
    # i16x8.min_u - minimum of 8 16-bit unsigned integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umin v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpminuw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_max_s, macro()
    # i16x8.max_s - maximum of 8 16-bit signed integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smax v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpmaxsw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_max_u, macro()
    # i16x8.max_u - maximum of 8 16-bit unsigned integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umax v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpmaxuw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfd9a01)
ipintOp(_simd_i16x8_avgr_u, macro()
    # i16x8.avgr_u - average of 8 16-bit unsigned integers with rounding
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "urhadd v16.8h, v16.8h, v17.8h"
    elsif X86_64
        emit "vpavgw %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extmul_low_i8x16_s, macro()
    # i16x8.extmul_low_i8x16_s - multiply lower 8 i8 elements and extend to i16
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smull v16.8h, v16.8b, v17.8b"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulLow
        emit "vpmovsxbw %xmm0, %xmm2"     # Sign extend left to scratch
        emit "vpmovsxbw %xmm1, %xmm0"     # Sign extend right to dest
        emit "vpmullw %xmm2, %xmm0, %xmm0" # Multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extmul_high_i8x16_s, macro()
    # i16x8.extmul_high_i8x16_s - multiply upper 8 i8 elements and extend to i16
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smull2 v16.8h, v16.16b, v17.16b"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulHigh
        emit "vpunpckhbw %xmm0, %xmm0, %xmm2"  # Unpack high bytes of left
        emit "vpsraw $8, %xmm2, %xmm2"         # Arithmetic shift to sign extend
        emit "vpunpckhbw %xmm1, %xmm1, %xmm0"  # Unpack high bytes of right
        emit "vpsraw $8, %xmm0, %xmm0"         # Arithmetic shift to sign extend
        emit "vpmullw %xmm2, %xmm0, %xmm0"     # Multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extmul_low_i8x16_u, macro()
    # i16x8.extmul_low_i8x16_u - multiply lower 8 u8 elements and extend to i16
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umull v16.8h, v16.8b, v17.8b"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulLow
        emit "vpmovzxbw %xmm0, %xmm2"      # Zero extend left to scratch
        emit "vpmovzxbw %xmm1, %xmm0"      # Zero extend right to dest
        emit "vpmullw %xmm2, %xmm0, %xmm0" # Multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i16x8_extmul_high_i8x16_u, macro()
    # i16x8.extmul_high_i8x16_u - multiply upper 8 u8 elements and extend to i16
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umull2 v16.8h, v16.16b, v17.16b"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulHigh
        emit "vpxor %xmm2, %xmm2, %xmm2"       # Zero scratch register
        emit "vpunpckhbw %xmm2, %xmm1, %xmm1"  # Unpack high bytes of right with zeros  
        emit "vpunpckhbw %xmm2, %xmm0, %xmm0"  # Unpack high bytes of left with zeros
        emit "vpmullw %xmm1, %xmm0, %xmm0"     # Multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0xA0 0x01 - 0xFD 0xBF 0x01: i32x4 operations

ipintOp(_simd_i32x4_abs, macro()
    # i32x4.abs - absolute value of 4 32-bit signed integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "abs v16.4s, v16.4s"
    elsif X86_64
        emit "vpabsd %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_neg, macro()
    # i32x4.neg - negate 4 32-bit integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "neg v16.4s, v16.4s"
    elsif X86_64
        # Negate by subtracting from zero
        emit "vpxor %xmm1, %xmm1, %xmm1"
        emit "vpsubd %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfda201)

ipintOp(_simd_i32x4_all_true, macro()
    # i32x4.all_true - return 1 if all 4 32-bit lanes are non-zero, 0 otherwise
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v17.4s, v16.4s, #0"   # Compare each lane with 0
        emit "umaxv s17, v17.4s"         # Find maximum (any zero lane will make this non-zero)
        emit "fmov w0, s17"              # Move to general register
        emit "cmp w0, #0"                # Compare with 0
        emit "cset w0, eq"               # Set to 1 if equal (all lanes non-zero), 0 otherwise
    elsif X86_64
        # Compare each 32-bit lane with zero
        emit "vpxor %xmm1, %xmm1, %xmm1"     # Create zero vector
        emit "vpcmpeqd %xmm1, %xmm0, %xmm1"  # Compare each dword with 0 (1 if zero, 0 if non-zero)

        # Test if any lane is zero
        emit "vpmovmskb %xmm1, %eax"         # Extract sign bits
        emit "testl %eax, %eax"              # Test if any bits are set
        emit "sete %al"                      # Set AL to 1 if no bits set (all lanes non-zero), 0 otherwise
        emit "movzbl %al, %eax"              # Zero-extend to 32-bit
    else
        break # Not implemented
    end
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_bitmask, macro()
    # i32x4.bitmask - extract most significant bit from each 32-bit lane into a 4-bit integer
    # Simple loop over the 4 32-bit values on the stack

    move 0, t0          # Initialize result
    move 0, t3          # Lane counter

.bitmask_i32x4_loop:
    # Load 32-bit value and check sign bit
    loadi [sp, t3, 4], t1  # Load 32-bit value at offset t1*4
    andq 0x80000000, t1 # Extract sign bit (bit 31)
    btiz t1, .bitmask_i32x4_next

    # Set corresponding bit in result
    move 1, t1
    lshiftq t3, t1      # Shift to bit position
    orq t1, t0

.bitmask_i32x4_next:
    addq 1, t3          # Next lane
    bilt t3, 4, .bitmask_i32x4_loop

    addp V128ISize, sp  # Pop the vector
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfda501)
reservedOpcode(0xfda601)

ipintOp(_simd_i32x4_extend_low_i16x8_s, macro()
    # i32x4.extend_low_i16x8_s - sign-extend lower 4 i16 values to i32
    popVec(v0)
    if ARM64 or ARM64E
        emit "sxtl v16.4s, v16.4h"
    elsif X86_64
        emit "vpmovsxwd %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extend_high_i16x8_s, macro()
    # i32x4.extend_high_i16x8_s - sign-extend upper 4 i16 values to i32
    popVec(v0)
    if ARM64 or ARM64E
        emit "sxtl2 v16.4s, v16.8h"
    elsif X86_64
        # Move high 64 bits to low, then sign extend
        emit "vpsrldq $8, %xmm0, %xmm0"   # Shift right 8 bytes to get high half
        emit "vpmovsxwd %xmm0, %xmm0"     # Sign extend
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extend_low_i16x8_u, macro()
    # i32x4.extend_low_i16x8_u - zero-extend lower 4 i16 values to i32
    popVec(v0)
    if ARM64 or ARM64E
        emit "uxtl v16.4s, v16.4h"
    elsif X86_64
        emit "vpmovzxwd %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extend_high_i16x8_u, macro()
    # i32x4.extend_high_i16x8_u - zero-extend upper 4 i16 values to i32
    popVec(v0)
    if ARM64 or ARM64E
        emit "uxtl2 v16.4s, v16.8h"
    elsif X86_64
        # Move high 64 bits to low, then zero extend
        emit "vpsrldq $8, %xmm0, %xmm0"   # Shift right 8 bytes to get high half
        emit "vpmovzxwd %xmm0, %xmm0"     # Zero extend
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_shl, macro()
    # i32x4.shl - left shift 4 32-bit integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-31 range for 32-bit elements
        andi 31, t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.4s, w0"
        # Perform left shift
        emit "ushl v16.4s, v16.4s, v17.4s"
    elsif X86_64
        andi 31, t0
        emit "vmovd %eax, %xmm1"
        emit "vpslld %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_shr_s, macro()
    # i32x4.shr_s - arithmetic right shift 4 32-bit signed integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-31 range for 32-bit elements
        andi 31, t0
        # Negate for right shift
        negi t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.4s, w0"
        # Perform arithmetic right shift
        emit "sshl v16.4s, v16.4s, v17.4s"
    elsif X86_64
        andi 31, t0
        emit "vmovd %eax, %xmm1"
        emit "vpsrad %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_shr_u, macro()
    # i32x4.shr_u - logical right shift 4 32-bit unsigned integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-31 range for 32-bit elements
        andi 31, t0
        # Negate for right shift
        negi t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.4s, w0"
        # Perform logical right shift
        emit "ushl v16.4s, v16.4s, v17.4s"
    elsif X86_64
        andi 31, t0
        emit "vmovd %eax, %xmm1"
        emit "vpsrld %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_add, macro()
    # i32x4.add - add 4 32-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "add v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpaddd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfdaf01)
reservedOpcode(0xfdb001)

ipintOp(_simd_i32x4_sub, macro()
    # i32x4.sub - subtract 4 32-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sub v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpsubd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfdb201)
reservedOpcode(0xfdb301)
reservedOpcode(0xfdb401)

ipintOp(_simd_i32x4_mul, macro()
    # i32x4.mul - multiply 4 32-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "mul v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpmulld %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_min_s, macro()
    # i32x4.min_s - minimum of 4 32-bit signed integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smin v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpminsd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_min_u, macro()
    # i32x4.min_u - minimum of 4 32-bit unsigned integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umin v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpminud %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_max_s, macro()
    # i32x4.max_s - maximum of 4 32-bit signed integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smax v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpmaxsd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_max_u, macro()
    # i32x4.max_u - maximum of 4 32-bit unsigned integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umax v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vpmaxud %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_dot_i16x8_s, macro()
    # i32x4.dot_i16x8_s - dot product of signed 16-bit integers to 32-bit
    # Multiplies pairs of adjacent 16-bit elements and adds the results
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # Use signed multiply long to multiply adjacent pairs, then pairwise add
        emit "smull v18.4s, v16.4h, v17.4h"      # multiply low 4 pairs to v18
        emit "smull2 v16.4s, v16.8h, v17.8h"     # multiply high 4 pairs to v19
        # Now pairwise add adjacent elements within each vector to get dot products
        emit "addp v16.4s, v18.4s, v16.4s"       # pairwise add to get final dot product result
    elsif X86_64
        emit "vpmaddwd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)
reservedOpcode(0xfdbb01)

ipintOp(_simd_i32x4_extmul_low_i16x8_s, macro()
    # i32x4.extmul_low_i16x8_s - multiply lower 4 i16 elements and extend to i32
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smull v16.4s, v16.4h, v17.4h"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulLow
        emit "vpmullw %xmm1, %xmm0, %xmm2"     # Low multiply to scratch
        emit "vpmulhw %xmm1, %xmm0, %xmm0"     # High multiply (signed) to dest
        emit "vpunpcklwd %xmm0, %xmm2, %xmm0"  # Interleave low words
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extmul_high_i16x8_s, macro()
    # i32x4.extmul_high_i16x8_s - multiply upper 4 i16 elements and extend to i32
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smull2 v16.4s, v16.8h, v17.8h"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulHigh
        emit "vpmullw %xmm1, %xmm0, %xmm2"     # Low multiply to scratch
        emit "vpmulhw %xmm1, %xmm0, %xmm0"     # High multiply (signed) to dest
        emit "vpunpckhwd %xmm0, %xmm2, %xmm0"  # Interleave high words
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extmul_low_i16x8_u, macro()
    # i32x4.extmul_low_i16x8_u - multiply lower 4 u16 elements and extend to i32
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umull v16.4s, v16.4h, v17.4h"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulLow
        emit "vpmullw %xmm1, %xmm0, %xmm2"     # Low multiply to scratch
        emit "vpmulhuw %xmm1, %xmm0, %xmm0"    # High multiply (unsigned) to dest
        emit "vpunpcklwd %xmm0, %xmm2, %xmm0"  # Interleave low words
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_extmul_high_i16x8_u, macro()
    # i32x4.extmul_high_i16x8_u - multiply upper 4 u16 elements and extend to i32
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umull2 v16.4s, v16.8h, v17.8h"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulHigh
        emit "vpmullw %xmm1, %xmm0, %xmm2"     # Low multiply to scratch
        emit "vpmulhuw %xmm1, %xmm0, %xmm0"    # High multiply (unsigned) to dest
        emit "vpunpckhwd %xmm0, %xmm2, %xmm0"  # Interleave high words
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0xC0 0x01 - 0xFD 0xDF 0x01: i64x2 operations

ipintOp(_simd_i64x2_abs, macro()
    # i64x2.abs - absolute value of 2 64-bit signed integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "abs v16.2d, v16.2d"
    elsif X86_64
        # No direct vpabsq instruction, implement manually
        # For each 64-bit lane: result = (x < 0) ? -x : x
        emit "vpxor %xmm1, %xmm1, %xmm1"     # xmm1 = 0
        emit "vpcmpgtq %xmm0, %xmm1, %xmm2"  # xmm2 = mask where x < 0 (0 > x)
        emit "vpsubq %xmm0, %xmm1, %xmm1"    # xmm1 = -x
        emit "vpblendvb %xmm2, %xmm1, %xmm0, %xmm0" # blend: use -x where mask is true, x otherwise
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_neg, macro()
    # i64x2.neg - negate 2 64-bit integers
    popVec(v0)
    if ARM64 or ARM64E
        emit "neg v16.2d, v16.2d"
    elsif X86_64
        # Negate by subtracting from zero
        emit "vpxor %xmm1, %xmm1, %xmm1"
        emit "vpsubq %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfdc201)

ipintOp(_simd_i64x2_all_true, macro()
    # i64x2.all_true - return 1 if all 2 64-bit lanes are non-zero, 0 otherwise
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v17.2d, v16.2d, #0"   # Compare each lane with 0
        emit "addp d17, v17.2d"          # Add pair - if any lane was 0, result will be non-zero
        emit "fmov x0, d17"              # Move to general register
        emit "cmp x0, #0"                # Compare with 0
        emit "cset w0, eq"               # Set to 1 if equal (all lanes non-zero), 0 otherwise
    elsif X86_64
        # Compare each 64-bit lane with zero
        emit "vpxor %xmm1, %xmm1, %xmm1"     # Create zero vector
        emit "vpcmpeqq %xmm1, %xmm0, %xmm1"  # Compare each qword with 0 (1 if zero, 0 if non-zero)

        # Test if any lane is zero
        emit "vpmovmskb %xmm1, %eax"         # Extract sign bits
        emit "testl %eax, %eax"              # Test if any bits are set
        emit "sete %al"                      # Set AL to 1 if no bits set (all lanes non-zero), 0 otherwise
        emit "movzbl %al, %eax"              # Zero-extend to 32-bit
    else
        break # Not implemented
    end
    pushInt32(t0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_bitmask, macro()
    # i64x2.bitmask - extract most significant bit from each 64-bit lane into a 2-bit integer
    # Handle both 64-bit values directly

    # Load both 64-bit values
    loadq [sp], t0      # Load lane 0
    loadq 8[sp], t1     # Load lane 1
    addp V128ISize, sp  # Pop the vector

    # Initialize result
    move 0, t2

    # Check lane 0 sign bit (bit 63)
    move 0x8000000000000000, t3
    andq t3, t0
    btqz t0, .bitmask_i64x2_lane1
    orq 1, t2           # Set bit 0

.bitmask_i64x2_lane1:
    # Check lane 1 sign bit (bit 63)
    andq t3, t1
    btqz t1, .bitmask_i64x2_done
    orq 2, t2           # Set bit 1

.bitmask_i64x2_done:
    pushInt32(t2)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfdc501)
reservedOpcode(0xfdc601)

ipintOp(_simd_i64x2_extend_low_i32x4_s, macro()
    # i64x2.extend_low_i32x4_s - sign-extend lower 2 i32 values to i64
    popVec(v0)
    if ARM64 or ARM64E
        emit "sxtl v16.2d, v16.2s"
    elsif X86_64
        emit "vpmovsxdq %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extend_high_i32x4_s, macro()
    # i64x2.extend_high_i32x4_s - sign-extend upper 2 i32 values to i64
    popVec(v0)
    if ARM64 or ARM64E
        emit "sxtl2 v16.2d, v16.4s"
    elsif X86_64
        # Move high 64 bits to low, then sign extend
        emit "vpsrldq $8, %xmm0, %xmm0"   # Shift right 8 bytes to get high half
        emit "vpmovsxdq %xmm0, %xmm0"     # Sign extend
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extend_low_i32x4_u, macro()
    # i64x2.extend_low_i32x4_u - zero-extend lower 2 i32 values to i64
    popVec(v0)
    if ARM64 or ARM64E
        emit "uxtl v16.2d, v16.2s"
    elsif X86_64
        emit "vpmovzxdq %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extend_high_i32x4_u, macro()
    # i64x2.extend_high_i32x4_u - zero-extend upper 2 i32 values to i64
    popVec(v0)
    if ARM64 or ARM64E
        emit "uxtl2 v16.2d, v16.4s"
    elsif X86_64
        # Move high 64 bits to low, then zero extend
        emit "vpsrldq $8, %xmm0, %xmm0"   # Shift right 8 bytes to get high half
        emit "vpmovzxdq %xmm0, %xmm0"     # Zero extend
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_shl, macro()
    # i64x2.shl - left shift 2 64-bit integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-63 range for 64-bit elements
        andi 63, t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.2d, x0"
        # Perform left shift
        emit "ushl v16.2d, v16.2d, v17.2d"
    elsif X86_64
        andi 63, t0
        emit "movd %eax, %xmm1"
        emit "vpsllq %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_shr_s, macro()
    # i64x2.shr_s - arithmetic right shift 2 64-bit signed integers
    popInt32(t0)  # shift count
    # Mask shift count to 0-63 range for 64-bit elements
    andi 63, t0

    loadq 8[sp], t1
    rshiftq t0, t1
    storeq t1, 8[sp]

    loadq [sp], t1
    rshiftq t0, t1
    storeq t1, [sp]

    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_shr_u, macro()
    # i64x2.shr_u - logical right shift 2 64-bit unsigned integers
    popInt32(t0)  # shift count
    popVec(v0)        # vector
    if ARM64 or ARM64E
        # Mask shift count to 0-63 range for 64-bit elements
        andi 63, t0
        # Negate for right shift
        negq t0
        # Duplicate shift count to all lanes of vector register
        emit "dup v17.2d, x0"
        # Perform logical right shift
        emit "ushl v16.2d, v16.2d, v17.2d"
    elsif X86_64
        andi 63, t0
        emit "movd %eax, %xmm1"
        emit "vpsrlq %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_add, macro()
    # i64x2.add - add 2 64-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "add v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vpaddq %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfdcf01)
reservedOpcode(0xfdd001)

ipintOp(_simd_i64x2_sub, macro()
    # i64x2.sub - subtract 2 64-bit integers
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "sub v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vpsubq %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfdd201)
reservedOpcode(0xfdd301)
reservedOpcode(0xfdd401)

ipintOp(_simd_i64x2_mul, macro()
    # i64x2.mul - multiply 2 64-bit integers (low 64 bits of result)

    # Extract and multiply lane 0 (first 64-bit element)
    loadq [sp], t0            # Load lane 0 of vector1
    loadq 16[sp], t1          # Load lane 0 of vector0
    mulq t1, t0               # Multiply: t0 = t0 * t1
    storeq t0, 16[sp]         # Store result back to vector0

    # Extract and multiply lane 1 (second 64-bit element)
    loadq 8[sp], t0           # Load lane 1 of vector1
    loadq 24[sp], t1          # Load lane 1 of vector0
    mulq t1, t0               # Multiply: t0 = t0 * t1
    storeq t0, 24[sp]         # Store result back to vector0

    # Pop vector1, result in vector0
    addp V128ISize, sp        # Remove first vector from stack, leaving result
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_eq, macro()
    # i64x2.eq - compare 2 64-bit integers for equality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vpcmpeqq %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_ne, macro()
    # i64x2.ne - compare 2 64-bit integers for inequality
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmeq v16.2d, v16.2d, v17.2d"
        emit "mvn v16.16b, v16.16b"
    elsif X86_64
        # Compare for equality, then invert the result
        emit "vpcmpeqq %xmm1, %xmm0, %xmm0"
        emit "vpcmpeqq %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_lt_s, macro()
    # i64x2.lt_s - compare 2 64-bit signed integers for less than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmgt v17, v16 gives us v1 > v0, which is equivalent to v0 < v1
        emit "cmgt v16.2d, v17.2d, v16.2d"
    elsif X86_64
        # vpcmpgtq xmm1, xmm0 gives us xmm1 > xmm0, which is equivalent to xmm0 < xmm1
        emit "vpcmpgtq %xmm0, %xmm1, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_gt_s, macro()
    # i64x2.gt_s - compare 2 64-bit signed integers for greater than
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmgt v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vpcmpgtq %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_le_s, macro()
    # i64x2.le_s - compare 2 64-bit signed integers for less than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # cmge v17, v16 gives us v1 >= v0, which is equivalent to v0 <= v1
        emit "cmge v16.2d, v17.2d, v16.2d"
    elsif X86_64
        # xmm0 <= xmm1 iff !(xmm0 > xmm1)
        emit "vpcmpgtq %xmm1, %xmm0, %xmm0"  # xmm0 > xmm1
        emit "vpcmpeqq %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm0 > xmm1)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_ge_s, macro()
    # i64x2.ge_s - compare 2 64-bit signed integers for greater than or equal
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "cmge v16.2d, v16.2d, v17.2d"
    elsif X86_64
        # xmm0 >= xmm1 iff !(xmm0 < xmm1) iff !(xmm1 > xmm0)
        emit "vpcmpgtq %xmm0, %xmm1, %xmm0"  # xmm1 > xmm0
        emit "vpcmpeqq %xmm2, %xmm2, %xmm2"  # Set all bits to 1
        emit "vpxor %xmm2, %xmm0, %xmm0"     # Invert result: !(xmm1 > xmm0)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extmul_low_i32x4_s, macro()
    # i64x2.extmul_low_i32x4_s - multiply lower 2 i32 elements and extend to i64
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smull v16.2d, v16.2s, v17.2s"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulLow
        emit "vpunpckldq %xmm0, %xmm0, %xmm2"  # Duplicate low dwords of left
        emit "vpunpckldq %xmm1, %xmm1, %xmm0"  # Duplicate low dwords of right
        emit "vpmuldq %xmm2, %xmm0, %xmm0"     # Signed multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extmul_high_i32x4_s, macro()
    # i64x2.extmul_high_i32x4_s - multiply upper 2 i32 elements and extend to i64
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "smull2 v16.2d, v16.4s, v17.4s"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulHigh
        emit "vpunpckhdq %xmm0, %xmm0, %xmm2"  # Duplicate high dwords of left
        emit "vpunpckhdq %xmm1, %xmm1, %xmm0"  # Duplicate high dwords of right
        emit "vpmuldq %xmm2, %xmm0, %xmm0"     # Signed multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extmul_low_i32x4_u, macro()
    # i64x2.extmul_low_i32x4_u - multiply lower 2 u32 elements and extend to i64
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umull v16.2d, v16.2s, v17.2s"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulLow
        emit "vpunpckldq %xmm0, %xmm0, %xmm2"  # Duplicate low dwords of left
        emit "vpunpckldq %xmm1, %xmm1, %xmm0"  # Duplicate low dwords of right
        emit "vpmuludq %xmm2, %xmm0, %xmm0"    # Unsigned multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i64x2_extmul_high_i32x4_u, macro()
    # i64x2.extmul_high_i32x4_u - multiply upper 2 u32 elements and extend to i64
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "umull2 v16.2d, v16.4s, v17.4s"
    elsif X86_64
        # See MacroAssemblerX86_64::vectorMulHigh
        emit "vpunpckhdq %xmm0, %xmm0, %xmm2"  # Duplicate high dwords of left
        emit "vpunpckhdq %xmm1, %xmm1, %xmm0"  # Duplicate high dwords of right
        emit "vpmuludq %xmm2, %xmm0, %xmm0"    # Unsigned multiply
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0xE0 0x01 - 0xFD 0xEB 0x01: f32x4 operations

ipintOp(_simd_f32x4_abs, macro()
    # f32x4.abs - absolute value of 4 32-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "fabs v16.4s, v16.4s"
    elsif X86_64
        # Clear sign bit by AND with 0x7FFFFFFF mask
        emit "movabsq $0x7fffffff7fffffff, %rax"
        emit "vmovq %rax, %xmm1"
        emit "vpunpcklqdq %xmm1, %xmm1, %xmm1"
        emit "vandps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_neg, macro()
    # f32x4.neg - negate 4 32-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "fneg v16.4s, v16.4s"
    elsif X86_64
        # Flip sign bit by XOR with 0x80000000 mask
        emit "movabsq $0x8000000080000000, %rax"
        emit "vmovq %rax, %xmm1"
        emit "vpunpcklqdq %xmm1, %xmm1, %xmm1"
        emit "vxorps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfde201)

ipintOp(_simd_f32x4_sqrt, macro()
    # f32x4.sqrt - square root of 4 32-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "fsqrt v16.4s, v16.4s"
    elsif X86_64
        emit "vsqrtps %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_add, macro()
    # f32x4.add - add 4 32-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fadd v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vaddps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_sub, macro()
    # f32x4.sub - subtract 4 32-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fsub v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vsubps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_mul, macro()
    # f32x4.mul - multiply 4 32-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fmul v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vmulps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_div, macro()
    # f32x4.div - divide 4 32-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fdiv v16.4s, v16.4s, v17.4s"
    elsif X86_64
        emit "vdivps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_min, macro()
    # f32x4.min - minimum of 4 32-bit floats (IEEE 754-2008 semantics)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fmin v16.4s, v16.4s, v17.4s"
    elsif X86_64
        # Wasm differs from X86_64 in terms of signed zero values and propagating NaNs
        # so some special handling of those cases are needed.
        # Compute result in both directions to handle NaN asymmetry
        emit "vminps %xmm1, %xmm0, %xmm2"       # xmm2 = min(xmm0, xmm1)
        emit "vminps %xmm0, %xmm1, %xmm0"       # xmm0 = min(xmm1, xmm0)

        # OR results to propagate sign bits and NaN bits
        emit "vorps %xmm0, %xmm2, %xmm2"        # xmm2 = xmm0 | xmm2

        # Canonicalize NaNs by checking for unordered values and clearing mantissa
        emit "vcmpunordps %xmm2, %xmm0, %xmm0" # xmm0 = NaN mask (all 1's where NaN)
        emit "vorps %xmm0, %xmm2, %xmm2"        # xmm2 |= NaN mask
        emit "vpsrld $10, %xmm0, %xmm0"         # Shift mask to clear mantissa bits (f32 uses 10)
        emit "vpandn %xmm2, %xmm0, %xmm0"       # Clear mantissa to canonicalize NaN
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_max, macro()
    # f32x4.max - maximum of 4 32-bit floats (IEEE 754-2008 semantics)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fmax v16.4s, v16.4s, v17.4s"
    elsif X86_64
        # Wasm differs from X86_64 in terms of signed zero values and propagating NaNs
        # so some special handling of those cases are needed.
        # Compute result in both directions to handle NaN asymmetry
        emit "vmaxps %xmm1, %xmm0, %xmm2"       # xmm2 = max(xmm0, xmm1)
        emit "vmaxps %xmm0, %xmm1, %xmm0"       # xmm0 = max(xmm1, xmm0)

        # Check for discrepancies by XORing the results
        emit "vxorps %xmm0, %xmm2, %xmm0"       # xmm0 = xmm0 ^ xmm2

        # OR results to propagate sign bits and NaN bits
        emit "vorps %xmm0, %xmm2, %xmm2"        # xmm2 = xmm0 | xmm2

        # Propagate discrepancies in sign bit
        emit "vsubps %xmm0, %xmm2, %xmm2"       # xmm2 = xmm2 - xmm0

        # Canonicalize NaNs by checking for unordered values and clearing mantissa
        emit "vcmpunordps %xmm2, %xmm0, %xmm0" # xmm0 = NaN mask (all 1's where NaN)
        emit "vpsrld $10, %xmm0, %xmm0"         # Shift mask to clear mantissa bits (f32 uses 10)
        emit "vpandn %xmm2, %xmm0, %xmm0"       # Clear mantissa to canonicalize NaN
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_pmin, macro()
    # f32x4.pmin - pseudo-minimum of 4 32-bit floats (b < a ? b : a)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # Use fcmgt to compare v0 > v1, then use bsl to select
        emit "fcmgt v18.4s, v16.4s, v17.4s"
        emit "bsl v18.16b, v17.16b, v16.16b"
        emit "mov v16.16b, v18.16b"
    elsif X86_64
        emit "vcmpgtps %xmm1, %xmm0, %xmm2"          # xmm2 = (a > b) ? 0xFFFFFFFF : 0x00000000
        emit "vblendvps %xmm2, %xmm1, %xmm0, %xmm0"  # select b if mask is true, a if false
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_pmax, macro()
    # f32x4.pmax - pseudo-maximum of 4 32-bit floats (a < b ? b : a)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # Use fcmgt to compare v1 > v0, then use bsl to select
        emit "fcmgt v18.4s, v17.4s, v16.4s"
        emit "bsl v18.16b, v17.16b, v16.16b"
        emit "mov v16.16b, v18.16b"
    elsif X86_64
        emit "vcmpgtps %xmm0, %xmm1, %xmm2"          # xmm2 = (b > a) ? 0xFFFFFFFF : 0x00000000
        emit "vblendvps %xmm2, %xmm1, %xmm0, %xmm0"  # select b if mask is true, a if false
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0xEC 0x01 - 0xFD 0xF7 0x01: f64x2 operations

ipintOp(_simd_f64x2_abs, macro()
    # f64x2.abs - absolute value of 2 64-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "fabs v16.2d, v16.2d"
    elsif X86_64
        # Clear sign bit by AND with 0x7FFFFFFFFFFFFFFF mask
        emit "movabsq $0x7fffffffffffffff, %rax"
        emit "vmovq %rax, %xmm1"
        emit "vpunpcklqdq %xmm1, %xmm1, %xmm1"
        emit "vandpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_neg, macro()
    # f64x2.neg - negate 2 64-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "fneg v16.2d, v16.2d"
    elsif X86_64
        # Flip sign bit by XOR with 0x8000000000000000 mask
        emit "movabsq $0x8000000000000000, %rax"
        emit "vmovq %rax, %xmm1"
        emit "vpunpcklqdq %xmm1, %xmm1, %xmm1"
        emit "vxorpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

reservedOpcode(0xfdee01)

ipintOp(_simd_f64x2_sqrt, macro()
    # f64x2.sqrt - square root of 2 64-bit floats
    popVec(v0)
    if ARM64 or ARM64E
        emit "fsqrt v16.2d, v16.2d"
    elsif X86_64
        emit "vsqrtpd %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_add, macro()
    # f64x2.add - add 2 64-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fadd v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vaddpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_sub, macro()
    # f64x2.sub - subtract 2 64-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fsub v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vsubpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_mul, macro()
    # f64x2.mul - multiply 2 64-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fmul v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vmulpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_div, macro()
    # f64x2.div - divide 2 64-bit floats
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fdiv v16.2d, v16.2d, v17.2d"
    elsif X86_64
        emit "vdivpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_min, macro()
    # f64x2.min - minimum of 2 64-bit floats (IEEE 754-2008 semantics)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fmin v16.2d, v16.2d, v17.2d"
    elsif X86_64
        # Wasm differs from X86_64 in terms of signed zero values and propagating NaNs
        # so some special handling of those cases are needed.
        # Compute result in both directions to handle NaN asymmetry
        emit "vminpd %xmm1, %xmm0, %xmm2"       # xmm2 = min(xmm0, xmm1)
        emit "vminpd %xmm0, %xmm1, %xmm0"       # xmm0 = min(xmm1, xmm0)

        # OR results to propagate sign bits and NaN bits
        emit "vorpd %xmm0, %xmm2, %xmm2"        # xmm2 = xmm0 | xmm2

        # Canonicalize NaNs by checking for unordered values and clearing mantissa
        emit "vcmpunordpd %xmm2, %xmm0, %xmm0" # xmm0 = NaN mask (all 1's where NaN)
        emit "vorpd %xmm0, %xmm2, %xmm2"        # xmm2 |= NaN mask
        emit "vpsrlq $13, %xmm0, %xmm0"         # Shift mask to clear mantissa bits
        emit "vpandn %xmm2, %xmm0, %xmm0"       # Clear mantissa to canonicalize NaN
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_max, macro()
    # f64x2.max - maximum of 2 64-bit floats (IEEE 754-2008 semantics)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        emit "fmax v16.2d, v16.2d, v17.2d"
    elsif X86_64
        # Wasm differs from X86_64 in terms of signed zero values and propagating NaNs
        # so some special handling of those cases are needed.
        # Compute result in both directions to handle NaN asymmetry
        emit "vmaxpd %xmm1, %xmm0, %xmm2"       # xmm2 = max(xmm0, xmm1)
        emit "vmaxpd %xmm0, %xmm1, %xmm0"       # xmm0 = max(xmm1, xmm0)

        # Check for discrepancies by XORing the results
        emit "vxorpd %xmm0, %xmm2, %xmm0"       # xmm0 = xmm0 ^ xmm2

        # OR results to propagate sign bits and NaN bits
        emit "vorpd %xmm0, %xmm2, %xmm2"        # xmm2 = xmm0 | xmm2

        # Propagate discrepancies in sign bit
        emit "vsubpd %xmm0, %xmm2, %xmm2"       # xmm2 = xmm2 - xmm0

        # Canonicalize NaNs by checking for unordered values and clearing mantissa
        emit "vcmpunordpd %xmm2, %xmm0, %xmm0" # xmm0 = NaN mask (all 1's where NaN)
        emit "vpsrlq $13, %xmm0, %xmm0"         # Shift mask to clear mantissa bits
        emit "vpandn %xmm2, %xmm0, %xmm0"       # Clear mantissa to canonicalize NaN
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_pmin, macro()
    # f64x2.pmin - pseudo-minimum of 2 64-bit floats (b < a ? b : a)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # Use fcmgt to compare v0 > v1, then use bsl to select
        emit "fcmgt v18.2d, v16.2d, v17.2d"
        emit "bsl v18.16b, v17.16b, v16.16b"
        emit "mov v16.16b, v18.16b"
    elsif X86_64
        emit "vcmpgtpd %xmm1, %xmm0, %xmm2"          # xmm2 = (a > b) ? 0xFFFFFFFF : 0x00000000
        emit "vblendvpd %xmm2, %xmm1, %xmm0, %xmm0"  # select b if mask is true, a if false
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_pmax, macro()
    # f64x2.pmax - pseudo-maximum of 2 64-bit floats (a < b ? b : a)
    popVec(v1)
    popVec(v0)
    if ARM64 or ARM64E
        # Use fcmgt to compare v1 > v0, then use bsl to select
        emit "fcmgt v18.2d, v17.2d, v16.2d"
        emit "bsl v18.16b, v17.16b, v16.16b"
        emit "mov v16.16b, v18.16b"
    elsif X86_64
        emit "vcmpgtpd %xmm0, %xmm1, %xmm2"          # xmm2 = (b > a) ? 0xFFFFFFFF : 0x00000000
        emit "vblendvpd %xmm2, %xmm1, %xmm0, %xmm0"  # select b if mask is true, a if false
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

# 0xFD 0xF8 0x01 - 0xFD 0xFF 0x01: trunc/convert

ipintOp(_simd_i32x4_trunc_sat_f32x4_s, macro()
    # i32x4.trunc_sat_f32x4_s - truncate 4 f32 values to signed i32 with saturation
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcvtzs v16.4s, v16.4s"
    elsif X86_64
        # Saturation logic following MacroAssembler implementation
        emit "vmovaps %xmm0, %xmm1"                          # xmm1 = src
        emit "vcmpunordps %xmm1, %xmm1, %xmm1"               # xmm1 = NaN mask
        emit "vandnps %xmm0, %xmm1, %xmm1"                   # xmm1 = src with NaN lanes cleared
        
        # Load 0x1.0p+31f (2147483648.0f) constant
        emit "movl $0x4f000000, %eax"                        # 0x1.0p+31f
        emit "vmovd %eax, %xmm2"
        emit "vshufps $0, %xmm2, %xmm2, %xmm2"               # Broadcast to all 4 lanes
        
        emit "vcmpnltps %xmm2, %xmm1, %xmm3"                 # xmm3 = positive overflow mask (src >= 0x80000000)
        emit "vcvttps2dq %xmm1, %xmm1"                       # Convert with overflow saturated to 0x80000000
        emit "vpxor %xmm3, %xmm1, %xmm0"                     # Convert positive overflow to 0x7FFFFFFF
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_trunc_sat_f32x4_u, macro()
    # i32x4.trunc_sat_f32x4_u - truncate 4 f32 values to unsigned i32 with saturation
    popVec(v0)
    if ARM64 or ARM64E
        emit "fcvtzu v16.4s, v16.4s"
    elsif X86_64
        # Unsigned saturation logic following MacroAssembler implementation
        emit "vxorps %xmm1, %xmm1, %xmm1"                    # xmm1 = 0
        emit "vmaxps %xmm1, %xmm0, %xmm0"                    # Clear NaN and negatives
        
        # Load 2147483647.0f constant (rounds to 2147483648.0f in float32)
        emit "movl $0x4f000000, %eax"                        # 2147483647.0f
        emit "vmovd %eax, %xmm2"
        emit "vshufps $0, %xmm2, %xmm2, %xmm2"               # Broadcast to all 4 lanes
        
        emit "vmovaps %xmm0, %xmm3"                          # xmm3 = src copy
        emit "vsubps %xmm2, %xmm3, %xmm3"                    # xmm3 = src - 2147483647.0f
        emit "vcmpnltps %xmm2, %xmm3, %xmm1"                 # xmm1 = mask for overflow
        emit "vcvttps2dq %xmm3, %xmm3"                       # Convert (src - 2147483647.0f)
        emit "vpxor %xmm1, %xmm3, %xmm3"                     # Saturate positive overflow to 0x7FFFFFFF
        
        emit "vpxor %xmm4, %xmm4, %xmm4"                     # xmm4 = 0
        emit "vpmaxsd %xmm4, %xmm3, %xmm3"                   # Clear negatives
        
        emit "vcvttps2dq %xmm0, %xmm0"                       # Convert original src
        emit "vpaddd %xmm3, %xmm0, %xmm0"                    # Add correction
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_convert_i32x4_s, macro()
    # f32x4.convert_i32x4_s - convert 4 signed i32 values to f32
    popVec(v0)
    if ARM64 or ARM64E
        emit "scvtf v16.4s, v16.4s"
    elsif X86_64
        emit "vcvtdq2ps %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f32x4_convert_i32x4_u, macro()
    # f32x4.convert_i32x4_u - convert 4 unsigned i32 values to f32
    popVec(v0)
    if ARM64 or ARM64E
        emit "ucvtf v16.4s, v16.4s"
    elsif X86_64
        # See MacroAssembler::vectorConvertUnsigned
        emit "vpxor %xmm1, %xmm1, %xmm1"                 # clear scratch
        emit "vpblendw $0x55, %xmm0, %xmm1, %xmm1"       # i_low = low 16 bits of src
        emit "vpsubd %xmm1, %xmm0, %xmm0"                # i_high = high 16 bits of src
        emit "vcvtdq2ps %xmm1, %xmm1"                    # f_low = convertToF32(i_low)
        emit "vpsrld $1, %xmm0, %xmm0"                   # i_half_high = i_high / 2
        emit "vcvtdq2ps %xmm0, %xmm0"                    # f_half_high = convertToF32(i_half_high)
        emit "vaddps %xmm0, %xmm0, %xmm0"                # dst = f_half_high + f_half_high + f_low
        emit "vaddps %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_trunc_sat_f64x2_s_zero, macro()
    # i32x4.trunc_sat_f64x2_s_zero - truncate 2 f64 values to signed i32, zero upper 2 lanes
    popVec(v0)
    if ARM64 or ARM64E
        # Convert f64 to signed i64 first
        emit "fcvtzs v16.2d, v16.2d"
        # Signed saturating extract narrow from i64 to i32
        emit "sqxtn v16.2s, v16.2d"
        # Zero the upper 64 bits (lanes 2,3)
        emit "mov v16.d[1], xzr"
    elsif X86_64
        emit "vcmppd $0, %xmm0, %xmm0, %xmm1"                # xmm1 = ordered comparison mask (not NaN)
        
        # Load 2147483647.0 constant
        emit "movabsq $0x41dfffffffc00000, %rax"             # 2147483647.0 as double
        emit "vmovq %rax, %xmm2"
        emit "vpunpcklqdq %xmm2, %xmm2, %xmm2"               # Broadcast to both lanes
        
        emit "vandpd %xmm2, %xmm1, %xmm1"                    # xmm1 = 2147483647.0 where not NaN, 0 where NaN
        emit "vminpd %xmm1, %xmm0, %xmm0"                    # Clamp to max value and handle NaN
        emit "vcvttpd2dq %xmm0, %xmm0"                       # Convert to i32 (result in lower 64 bits, upper zeroed)
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_i32x4_trunc_sat_f64x2_u_zero, macro()
    # i32x4.trunc_sat_f64x2_u_zero - truncate 2 f64 values to unsigned i32, zero upper 2 lanes
    popVec(v0)
    if ARM64 or ARM64E
        # Convert f64 to unsigned i64 first
        emit "fcvtzu v16.2d, v16.2d"
        # Unsigned saturating extract narrow from i64 to i32
        emit "uqxtn v16.2s, v16.2d"
        # Zero the upper 64 bits (lanes 2,3)
        emit "mov v16.d[1], xzr"
    elsif X86_64
        # See MacroAssembler::vectorTruncSatUnsignedFloat64
        # Load constants: 4294967295.0 and 0x1.0p+52
        emit "movabsq $0x41efffffffe00000, %rax"             # 4294967295.0 as double
        emit "vmovq %rax, %xmm2"
        emit "vpunpcklqdq %xmm2, %xmm2, %xmm2"               # xmm2 = [4294967295.0, 4294967295.0]
        
        emit "movabsq $0x4330000000000000, %rax"             # 0x1.0p+52 as double
        emit "vmovq %rax, %xmm3"
        emit "vpunpcklqdq %xmm3, %xmm3, %xmm3"               # xmm3 = [0x1.0p+52, 0x1.0p+52]
        
        emit "vxorpd %xmm1, %xmm1, %xmm1"                    # xmm1 = 0.0
        emit "vmaxpd %xmm1, %xmm0, %xmm0"                    # Clear negatives
        emit "vminpd %xmm2, %xmm0, %xmm0"                    # Clamp to 4294967295.0
        emit "vroundpd $3, %xmm0, %xmm0"                     # Truncate toward zero
        emit "vaddpd %xmm3, %xmm0, %xmm0"                    # Add 0x1.0p+52 (magic number conversion)
        emit "vshufps $0x88, %xmm1, %xmm0, %xmm0"            # Pack to i32 and zero upper
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_convert_low_i32x4_s, macro()
    # f64x2.convert_low_i32x4_s - convert lower 2 signed i32 values to f64
    popVec(v0)
    if ARM64 or ARM64E
        # Sign-extend lower 2 i32 values to i64, then convert to f64
        emit "sxtl v16.2d, v16.2s"
        emit "scvtf v16.2d, v16.2d"
    elsif X86_64
        emit "vcvtdq2pd %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_simd_f64x2_convert_low_i32x4_u, macro()
    # f64x2.convert_low_i32x4_u - convert lower 2 unsigned i32 values to f64
    popVec(v0)
    if ARM64 or ARM64E
        # Zero-extend lower 2 i32 values to i64, then convert to f64
        emit "uxtl v16.2d, v16.2s"
        emit "ucvtf v16.2d, v16.2d"
    elsif X86_64
        # See MacroAssembler::vectorConvertLowUnsignedInt32
        # Load 0x43300000 (high32Bits) and splat to all lanes
        emit "movl $0x43300000, %eax"
        emit "vmovd %eax, %xmm1"
        emit "vpshufd $0, %xmm1, %xmm1"

        # Unpack lower 2 i32 with high32Bits
        emit "vunpcklps %xmm1, %xmm0, %xmm0"              # Interleave: [i32_0, 0x43300000, i32_1, 0x43300000]

        # Load 0x1.0p+52 mask
        emit "movabsq $0x4330000000000000, %rax"          # 0x1.0p+52 as double
        emit "vmovq %rax, %xmm1"
        emit "vpunpcklqdq %xmm1, %xmm1, %xmm1"            # xmm1 = [0x1.0p+52, 0x1.0p+52]

        # Subtract to get the correct unsigned values
        emit "vsubpd %xmm1, %xmm0, %xmm0"
    else
        break # Not implemented
    end
    pushVec(v0)
    advancePC(2)
    nextIPIntInstruction()
end)

    #########################
    ## Atomic instructions ##
    #########################

macro ipintCheckMemoryBoundWithAlignmentCheck(mem, scratch, size)
    leap size - 1[mem], scratch
    bpb scratch, boundsCheckingSize, .continuationInBounds
.throwOOB:
    ipintException(OutOfBoundsMemoryAccess)
.continuationInBounds:
    btpz mem, (size - 1), .continuationAligned
.throwUnaligned:
    throwException(UnalignedMemoryAccess)
.continuationAligned:
end

macro ipintCheckMemoryBoundWithAlignmentCheck1(mem, scratch)
    ipintCheckMemoryBound(mem, scratch, 1)
end

macro ipintCheckMemoryBoundWithAlignmentCheck2(mem, scratch)
    ipintCheckMemoryBoundWithAlignmentCheck(mem, scratch, 2)
end

macro ipintCheckMemoryBoundWithAlignmentCheck4(mem, scratch)
    ipintCheckMemoryBoundWithAlignmentCheck(mem, scratch, 4)
end

macro ipintCheckMemoryBoundWithAlignmentCheck8(mem, scratch)
    ipintCheckMemoryBoundWithAlignmentCheck(mem, scratch, 8)
end

ipintOp(_memory_atomic_notify, macro()
    # pop count
    popInt32(a3)
    # pop pointer
    popInt32(a1)
    # load offset
    loadi IPInt::Const32Metadata::value[MC], a2

    operationCall(macro() cCall4(_ipint_extern_memory_atomic_notify) end)
    bilt r0, 0, .atomic_notify_throw

    pushInt32(r0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()

.atomic_notify_throw:
    ipintException(OutOfBoundsMemoryAccess)
end)

ipintOp(_memory_atomic_wait32, macro()
    # pop timeout
    popInt32(a3)
    # pop value
    popInt32(a2)
    # pop pointer
    popInt32(a1)
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t0
    # merge them since the slow path takes the combined pointer + offset.
    addq t0, a1

    operationCall(macro() cCall4(_ipint_extern_memory_atomic_wait32) end)
    bilt r0, 0, .atomic_wait32_throw

    pushInt32(r0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()

.atomic_wait32_throw:
    ipintException(OutOfBoundsMemoryAccess)
end)

ipintOp(_memory_atomic_wait64, macro()
    # pop timeout
    popInt32(a3)
    # pop value
    popInt64(a2)
    # pop pointer
    popInt32(a1)
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t0
    # merge them since the slow path takes the combined pointer + offset.
    addq t0, a1

    operationCall(macro() cCall4(_ipint_extern_memory_atomic_wait64) end)
    bilt r0, 0, .atomic_wait64_throw

    pushInt32(r0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()

.atomic_wait64_throw:
    ipintException(OutOfBoundsMemoryAccess)
end)

ipintOp(_atomic_fence, macro()
    fence

    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

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

macro atomicLoadOp(boundsAndAlignmentCheck, loadAndPush)
    # pop index
    popInt32(t0)
    ori 0, t0
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t0
    boundsAndAlignmentCheck(t0,  t3)
    addq memoryBase, t0
    loadAndPush(t0, t2)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end

ipintOp(_i32_atomic_load, macro()
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadi [mem], scratch
        else
            error
        end
        pushInt32(scratch)
    end)
end)

ipintOp(_i64_atomic_load, macro()
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadq [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)
end)

ipintOp(_i32_atomic_load8_u, macro()
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadb [mem], scratch
        else
            error
        end
        pushInt32(scratch)
    end)
end)

ipintOp(_i32_atomic_load16_u, macro()
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadh [mem], scratch
        else
            error
        end
        pushInt32(scratch)
    end)
end)

ipintOp(_i64_atomic_load8_u, macro()
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadb [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)
end)

ipintOp(_i64_atomic_load16_u, macro()
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadh [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)
end)

ipintOp(_i64_atomic_load32_u, macro()
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadi [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)
end)

macro weakCASLoopByte(mem, value, scratch1AndOldValue, scratch2, fn)
    validateOpcodeConfig(scratch1AndOldValue)
    if X86_64
        loadb [mem], scratch1AndOldValue
    .loop:
        move scratch1AndOldValue, scratch2
        fn(value, scratch2)
        batomicweakcasb scratch1AndOldValue, scratch2, [mem], .loop
    else
    .loop:
        loadlinkacqb [mem], scratch1AndOldValue
        fn(value, scratch1AndOldValue, scratch2)
        storecondrelb ws2, scratch2, [mem]
        bineq ws2, 0, .loop
    end
end

macro weakCASLoopHalf(mem, value, scratch1AndOldValue, scratch2, fn)
    validateOpcodeConfig(scratch1AndOldValue)
    if X86_64
        loadh [mem], scratch1AndOldValue
    .loop:
        move scratch1AndOldValue, scratch2
        fn(value, scratch2)
        batomicweakcash scratch1AndOldValue, scratch2, [mem], .loop
    else
    .loop:
        loadlinkacqh [mem], scratch1AndOldValue
        fn(value, scratch1AndOldValue, scratch2)
        storecondrelh ws2, scratch2, [mem]
        bineq ws2, 0, .loop
    end
end

macro weakCASLoopInt(mem, value, scratch1AndOldValue, scratch2, fn)
    validateOpcodeConfig(scratch1AndOldValue)
    if X86_64
        loadi [mem], scratch1AndOldValue
    .loop:
        move scratch1AndOldValue, scratch2
        fn(value, scratch2)
        batomicweakcasi scratch1AndOldValue, scratch2, [mem], .loop
    else
    .loop:
        loadlinkacqi [mem], scratch1AndOldValue
        fn(value, scratch1AndOldValue, scratch2)
        storecondreli ws2, scratch2, [mem]
        bineq ws2, 0, .loop
    end
end

macro weakCASLoopQuad(mem, value, scratch1AndOldValue, scratch2, fn)
    validateOpcodeConfig(scratch1AndOldValue)
    if X86_64
        loadq [mem], scratch1AndOldValue
    .loop:
        move scratch1AndOldValue, scratch2
        fn(value, scratch2)
        batomicweakcasq scratch1AndOldValue, scratch2, [mem], .loop
    else
    .loop:
        loadlinkacqq [mem], scratch1AndOldValue
        fn(value, scratch1AndOldValue, scratch2)
        storecondrelq ws2, scratch2, [mem]
        bineq ws2, 0, .loop
    end
end

macro atomicStoreOp(boundsAndAlignmentCheck, popAndStore)
    # pop value
    popInt64(t1)
    # pop index
    popInt32(t2)
    ori 0, t2
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t0
    addp t0, t2
    boundsAndAlignmentCheck(t2, t3)
    addq memoryBase, t2
    popAndStore(t2, t1, t0, t3)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end

ipintOp(_i32_atomic_store, macro()
    atomicStoreOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgi value, [mem], value
        elsif X86_64
            atomicxchgi value, [mem]
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
    end)
end)

ipintOp(_i64_atomic_store, macro()
    atomicStoreOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgq value, [mem], value
        elsif X86_64
            atomicxchgq value, [mem]
        elsif ARM64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
    end)
end)

ipintOp(_i32_atomic_store8_u, macro()
    atomicStoreOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgb value, [mem], value
        elsif X86_64
            atomicxchgb value, [mem]
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
    end)
end)

ipintOp(_i32_atomic_store16_u, macro()
    atomicStoreOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgh value, [mem], value
        elsif X86_64
            atomicxchgh value, [mem]
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
    end)
end)

ipintOp(_i64_atomic_store8_u, macro()
    atomicStoreOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgb value, [mem], value
        elsif X86_64
            atomicxchgb value, [mem]
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
    end)
end)

ipintOp(_i64_atomic_store16_u, macro()
    atomicStoreOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgh value, [mem], value
        elsif X86_64
            atomicxchgh value, [mem]
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
    end)
end)

ipintOp(_i64_atomic_store32_u, macro()
    atomicStoreOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgi value, [mem], value
        elsif X86_64
            atomicxchgi value, [mem]
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
    end)
end)

macro atomicRMWOp(boundsAndAlignmentCheck, rmw)
    # pop value
    popInt64(t1)
    # pop index
    popInt32(t2)
    ori 0, t2
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t0
    addp t0, t2
    boundsAndAlignmentCheck(t2, t3)
    addq memoryBase, t2
    rmw(t2, t1, t0, t3)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end

ipintOp(_i32_atomic_rmw_add, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddi value, [mem], scratch1
        elsif X86_64
            atomicxchgaddi value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw_add, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddq value, [mem], scratch1
        elsif X86_64
            atomicxchgaddq value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addq value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw8_add_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            atomicxchgaddb value, [mem]
            move value, scratch1
            andi 0xff, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw16_add_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            atomicxchgaddh value, [mem]
            move value, scratch1
            andi 0xffff, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw8_add_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            atomicxchgaddb value, [mem]
            move value, scratch1
            andi 0xff, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw16_add_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            atomicxchgaddh value, [mem]
            move value, scratch1
            andi 0xffff, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw32_add_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddi value, [mem], scratch1
        elsif X86_64
            atomicxchgaddi value, [mem]
            move value, scratch1
            ori 0, scratch1
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw_sub, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negi value
            atomicxchgaddi value, [mem], scratch1
        elsif X86_64
            negi value
            atomicxchgaddi value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw_sub, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negq value
            atomicxchgaddq value, [mem], scratch1
        elsif X86_64
            negq value
            atomicxchgaddq value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subq oldValue, value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw8_sub_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negi value
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            negi value
            atomicxchgaddb value, [mem]
            move value, scratch1
            andi 0xff, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw16_sub_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negi value
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            negi value
            atomicxchgaddh value, [mem]
            move value, scratch1
            andi 0xffff, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw8_sub_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negq value
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            negq value
            atomicxchgaddb value, [mem]
            move value, scratch1
            andi 0xff, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw16_sub_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negq value
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            negq value
            atomicxchgaddh value, [mem]
            move value, scratch1
            andi 0xffff, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw32_sub_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negq value
            atomicxchgaddi value, [mem], scratch1
        elsif X86_64
            negq value
            atomicxchgaddi value, [mem]
            move value, scratch1
            ori 0, scratch1
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw_and, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            noti value
            atomicxchgcleari value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                andq value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                andi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw_and, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, scratch1, scratch2)
        if ARM64E
            notq value
            atomicxchgclearq value, [mem], scratch1
        elsif X86_64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro (value, dst)
                andq value, dst
            end)
        elsif ARM64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                andq value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw8_and_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            noti value
            atomicxchgclearb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                andq value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                andi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw16_and_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            noti value
            atomicxchgclearh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                andq value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                andi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw8_and_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            notq value
            atomicxchgclearb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                andq value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                andi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw16_and_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            notq value
            atomicxchgclearh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                andq value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                andi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw32_and_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            notq value
            atomicxchgcleari value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                andq value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                andi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw_or, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgori value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                ori value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                ori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw_or, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgorq value, [mem], scratch1
        elsif X86_64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro (value, dst)
                orq value, dst
            end)
        elsif ARM64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                orq value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw8_or_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgorb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                orq value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                ori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw16_or_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgorh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                orq value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                ori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw8_or_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgorb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                orq value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                ori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw16_or_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgorh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                orq value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                ori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw32_or_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgori value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                orq value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                ori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw_xor, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgxori value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                xorq value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                xori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw_xor, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgxorq value, [mem], scratch1
        elsif X86_64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro (value, dst)
                xorq value, dst
            end)
        elsif ARM64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                xorq value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw8_xor_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgxorb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                xorq value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                xori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw16_xor_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgxorh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                xorq value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                xori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw8_xor_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgxorb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                xorq value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                xori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw16_xor_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgxorh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                xorq value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                xori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw32_xor_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgxori value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                xorq value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                xori value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw_xchg, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgi value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                move value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw_xchg, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgq value, [mem], scratch1
        elsif X86_64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro (value, dst)
                move value, dst
            end)
        elsif ARM64
            weakCASLoopQuad(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw8_xchg_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                move value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i32_atomic_rmw16_xchg_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                move value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw8_xchg_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgb value, [mem], scratch1
        elsif X86_64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro (value, dst)
                move value, dst
            end)
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw16_xchg_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgh value, [mem], scratch1
        elsif X86_64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro (value, dst)
                move value, dst
            end)
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

ipintOp(_i64_atomic_rmw32_xchg_u, macro()
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgi value, [mem], scratch1
        elsif X86_64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro (value, dst)
                move value, dst
            end)
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                move value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)
end)

macro atomicCmpxchgOp(boundsAndAlignmentCheck, cmpxchg)
    # pop value
    popInt64(t1)
    # pop expected
    popInt64(t0)
    # pop index
    popInt32(t3)
    ori 0, t3
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t2
    addp t2, t3
    boundsAndAlignmentCheck(t3, t2)
    addq memoryBase, t3
    cmpxchg(t3, t1, t0, t2, t4)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end

macro weakCASExchangeByte(mem, value, expected, scratch, scratch2)
    if ARM64
    validateOpcodeConfig(scratch2)
    .loop:
        loadlinkacqb [mem], scratch2
        bqneq expected, scratch2, .fail
        storecondrelb scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        storecondrelb scratch, scratch2, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .done:
        move scratch2, expected
    else
        error
    end
end

macro weakCASExchangeHalf(mem, value, expected, scratch, scratch2)
    if ARM64
    validateOpcodeConfig(scratch2)
    .loop:
        loadlinkacqh [mem], scratch2
        bqneq expected, scratch2, .fail
        storecondrelh scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        storecondrelh scratch, scratch2, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .done:
        move scratch2, expected
    else
        error
    end
end

macro weakCASExchangeInt(mem, value, expected, scratch, scratch2)
    if ARM64
    validateOpcodeConfig(scratch2)
    .loop:
        loadlinkacqi [mem], scratch2
        bqneq expected, scratch2, .fail
        storecondreli scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        storecondreli scratch, scratch2, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .done:
        move scratch2, expected
    else
        error
    end
end

macro weakCASExchangeQuad(mem, value, expected, scratch, scratch2)
    if ARM64
    validateOpcodeConfig(scratch2)
    .loop:
        loadlinkacqq [mem], scratch2
        bqneq expected, scratch2, .fail
        storecondrelq scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        storecondrelq scratch, scratch2, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .done:
        move scratch2, expected
    else
        error
    end
end

ipintOp(_i32_atomic_rmw_cmpxchg, macro()
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, expected, scratch, scratch2)
        andq 0xffffffff, expected
        if ARM64E or X86_64
            atomicweakcasi expected, value, [mem]
        elsif ARM64
            weakCASExchangeInt(mem, value, expected, scratch, scratch2)
        else
            error
        end
        pushInt32(expected)
    end)
end)

ipintOp(_i64_atomic_rmw_cmpxchg, macro()
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, expected, scratch, scratch2)
        if ARM64E or X86_64
            atomicweakcasq expected, value, [mem]
        elsif ARM64
            weakCASExchangeQuad(mem, value, expected, scratch, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)
end)

ipintOp(_i32_atomic_rmw8_cmpxchg_u, macro()
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, expected, scratch, scratch2)
        andq 0xff, expected
        if ARM64E or X86_64
            atomicweakcasb expected, value, [mem]
        elsif ARM64
            weakCASExchangeByte(mem, value, expected, scratch, scratch2)
        else
            error
        end
        pushInt32(expected)
    end)
end)

ipintOp(_i32_atomic_rmw16_cmpxchg_u, macro()
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, expected, scratch, scratch2)
        andq 0xffff, expected
        if ARM64E or X86_64
            atomicweakcash expected, value, [mem]
        elsif ARM64
            weakCASExchangeHalf(mem, value, expected, scratch, scratch2)
        else
            error
        end
        pushInt32(expected)
    end)
end)

ipintOp(_i64_atomic_rmw8_cmpxchg_u, macro()
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, expected, scratch, scratch2)
        andq 0xff, expected
        if ARM64E or X86_64
            atomicweakcasb expected, value, [mem]
        elsif ARM64
            weakCASExchangeByte(mem, value, expected, scratch, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)
end)

ipintOp(_i64_atomic_rmw16_cmpxchg_u, macro()
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, expected, scratch, scratch2)
        andq 0xffff, expected
        if ARM64E or X86_64
            atomicweakcash expected, value, [mem]
        elsif ARM64
            weakCASExchangeHalf(mem, value, expected, scratch, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)
end)

ipintOp(_i64_atomic_rmw32_cmpxchg_u, macro()
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, expected, scratch, scratch2)
        andq 0xffffffff, expected
        if ARM64E or X86_64
            atomicweakcasi expected, value, [mem]
        elsif ARM64
            weakCASExchangeInt(mem, value, expected, scratch, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)
end)

#######################################
## ULEB128 decoding logic for locals ##
#######################################

macro decodeULEB128(result)
    # result should already be the first byte.
    andq 0x7f, result
    move 7, t2 # t1 holds the shift.
    validateOpcodeConfig(t3)
.loop:
    loadb [PC], t3
    andq t3, 0x7f, t1
    lshiftq t2, t1
    orq t1, result
    addq 7, t2
    advancePC(1)
    bbaeq t3, 128, .loop
end

slowPathLabel(_local_get)
    decodeULEB128(t0)
    localGetPostDecode()

slowPathLabel(_local_set)
    decodeULEB128(t0)
    localSetPostDecode()

slowPathLabel(_local_tee)
    decodeULEB128(t0)
    localTeePostDecode()

##################################
## "Out of line" logic for call ##
##################################

const mintSS = sc1

macro mintPop(reg)
    loadq [mintSS], reg
    addq V128ISize, mintSS
end

macro mintPopV(reg)
    loadv [mintSS], reg
    addq V128ISize, mintSS
end

macro mintArgDispatch()
    loadb [MC], sc0
    addq 1, MC
    bigteq sc0, (constexpr IPInt::CallArgumentBytecode::NumOpcodes), _ipint_mint_arg_dispatch_err
    lshiftq (constexpr (WTF::fastLog2(JSC::IPInt::alignMInt))), sc0
if ARM64 or ARM64E
    pcrtoaddr _mint_begin, csr4
    addq sc0, csr4
    jmp csr4
elsif X86_64
    leap (_mint_begin - _mint_arg_relativePCBase)[PC, sc0], sc0
    jmp sc0
end
end

macro mintRetDispatch()
    loadb [MC], sc0
    addq 1, MC
    bigteq sc0, (constexpr IPInt::CallResultBytecode::NumOpcodes), _ipint_mint_ret_dispatch_err
    lshiftq (constexpr (WTF::fastLog2(JSC::IPInt::alignMInt))), sc0
if ARM64 or ARM64E
    pcrtoaddr _mint_begin_return, csr4
    addq sc0, csr4
    jmp csr4
elsif X86_64
    leap (_mint_begin_return - _mint_ret_relativePCBase)[PC, sc0], sc0
    jmp sc0
end
end

.ipint_call_common:
    # we need to do some planning ahead to not step on our own values later
    # step 1: save all the stuff we had earlier
    # step 2: calling
    # - if we have more results than arguments, we need to move our stack pointer up in advance, or else
    #   pushing 16B values to the stack will overtake cleaning up 8B return values. we get this value from
    #   CallSignatureMetadata::numExtraResults
    # - set up the stack frame (with size CallSignatureMetadata::stackFrameSize)
    # step 2.5: saving registers:
    # - push our important data onto the stack here, after the saved space
    # step 3: jump to called function
    # - swap out instances, reload memory, and call
    # step 4: returning
    # - pop the registers from step 2.5
    # - we've left enough space for us to push our new values starting at the original stack pointer now! yay!

    # Free up r0 to be used as argument register

    const targetEntrypoint = sc2
    const targetInstance = sc3

    move r0, targetEntrypoint
    move r1, targetInstance

    const extraSpaceForReturns = t0
    const stackFrameSize = t1
    const numArguments = t2

    loadi IPInt::CallSignatureMetadata::stackFrameSize[MC], stackFrameSize
    loadh IPInt::CallSignatureMetadata::numExtraResults[MC], extraSpaceForReturns
    mulq StackValueSize, extraSpaceForReturns
    loadh IPInt::CallSignatureMetadata::numArguments[MC], numArguments
    mulq StackValueSize, numArguments
    advanceMC(constexpr (sizeof(IPInt::CallSignatureMetadata)))

    # calculate the SP after popping all arguments
    move sp, t3
    addp numArguments, t3

    # (down = decreasing address)
    # <first non-arg> <- t3 = SP after all arguments
    # arg
    # ...
    # arg
    # arg             <- initial SP (wasm stack)

    # store sp as our shadow stack for arguments later
    move sp, t4
    # make extra space if necessary
    subp extraSpaceForReturns, sp

    # <first non-arg> <- t3
    # arg
    # ...
    # arg
    # arg             <- t4 = initial SP (wasm stack)
    # reserved
    # reserved        <- sp

    # save t3 as a frame-relative value so stack data can be moved easily for JSPI
    # t3 is not used after this
    subp cfr, t3
    push t3, PC
    push PL, wasmInstance

    # set up the call frame
    move sp, t2
    subp stackFrameSize, sp

    # <first non-arg> <- first_non_arg_addr
    # arg
    # ...
    # arg
    # arg             <- t4 = initial SP (wasm stack)
    # reserved
    # reserved
    # (first_non_arg_addr - cfr), PC
    # PL, wasmInstance <- t2 = native argument stack (pushed by mINT)
    # call frame
    # call frame
    # call frame
    # call frame
    # call frame
    # call frame      <- sp

    # set up the Callee slot
    storeq IPIntCallCallee, Callee - CallerFrameAndPCSize[sp]
    storep IPIntCallFunctionSlot, CodeBlock - CallerFrameAndPCSize[sp]

    push targetEntrypoint, targetInstance

    move t2, sc3
    move t4, mintSS

    # need a common entrypoint because of x86 PC base
    jmp .ipint_mint_arg_dispatch

.ipint_tail_call_common:
    # Free up r0 to be used as argument register

    #  <caller frame>
    #  return val
    #  return val
    #  argument
    #  argument
    #  argument
    #  argument
    #  call frame
    #  call frame      <- cfr
    #  (IPInt locals)
    #  (IPInt stack)
    #  argument 0
    #  ...
    #  argument n-1
    #  argument n      <- sp

    # sc1 = target callee => wasmInstance to free up sc1
    const savedCallee = wasmInstance

    # store entrypoint and target instance on the stack for now
    push r0, r1
    push IPIntCallCallee, IPIntCallFunctionSlot

    # keep the top of IPInt stack in sc1 as shadow stack
    move sp, sc1
    # we pushed four values previously, so offset for this
    addq 32, sc1

    #  <caller frame>
    #  return val
    #  return val
    #  argument
    #  argument
    #  argument
    #  argument
    #  call frame
    #  call frame                  <- cfr
    #  (IPInt locals)
    #  (IPInt stack)
    #  argument 0
    #  ...
    #  argument n-1
    #  argument n                  <- sc1
    #  entrypoint, targetInstance
    #  callee, function info       <- sp

    # determine the location to begin copying stack arguments, starting from the last
    move cfr, sc2
    addp FirstArgumentOffset, sc2
    addp t3, sc2 # t3 = callerStackArgSize from the metadata

    #  <caller frame>              <- sc2
    #  return val
    #  return val
    #  argument
    #  argument
    #  argument
    #  argument
    #  call frame
    #  call frame                  <- cfr
    #  (IPInt locals)
    #  (IPInt stack)
    #  argument 0
    #  ...
    #  argument n-1
    #  argument n                  <- sc1
    #  entrypoint, targetInstance
    #  callee, function info       <- sp

    # get saved MC and PC

    if ARM64 or ARM64E
        loadpairq -0x10[cfr], t0, t1
    elsif X86_64 or RISCV64
        loadp -0x8[cfr], t1
        loadp -0x10[cfr], t0
    end

    push t0, t1

    # store the return address and CFR on the stack so we don't lose it
    loadp ReturnPC[cfr], t0
    loadp [cfr], t1

    push t0, t1

    #  <caller frame>              <- sc2
    #  return val
    #  return val
    #  argument
    #  argument
    #  argument
    #  argument
    #  call frame
    #  call frame                  <- cfr
    #  (IPInt locals)
    #  (IPInt stack)
    #  argument 0
    #  ...
    #  argument n-1
    #  argument n                  <- sc1
    #  entrypoint, targetInstance
    #  callee, function info
    #  saved MC/PC
    #  return address, saved CFR   <- sp

.ipint_mint_arg_dispatch:
    # on x86, we'll use PC for our PC base
    initPCRelative(mint_arg, PC)

    // We've already validateOpcodeConfig() in all the Wasm call opcodes.
    mintArgDispatch()

    # tail calls reuse most of mINT's argument logic, but exit into a different tail call stub.
    # we use sc2 to keep the new stack frame

mintAlign(_a0)
_mint_begin:
    mintPop(a0)
    mintArgDispatch()

mintAlign(_a1)
    mintPop(a1)
    mintArgDispatch()

mintAlign(_a2)
if ARM64 or ARM64E or X86_64
    mintPop(a2)
    mintArgDispatch()
else
    break
end

mintAlign(_a3)
if ARM64 or ARM64E or X86_64
    mintPop(a3)
    mintArgDispatch()
else
    break
end

mintAlign(_a4)
if ARM64 or ARM64E or X86_64
    mintPop(a4)
    mintArgDispatch()
else
    break
end

mintAlign(_a5)
if ARM64 or ARM64E or X86_64
    mintPop(a5)
    mintArgDispatch()
else
    break
end

mintAlign(_a6)
if ARM64 or ARM64E
    mintPop(a6)
    mintArgDispatch()
else
    break
end

mintAlign(_a7)
if ARM64 or ARM64E
    mintPop(a7)
    mintArgDispatch()
else
    break
end

mintAlign(_fa0)
    mintPopV(wfa0)
    mintArgDispatch()

mintAlign(_fa1)
    mintPopV(wfa1)
    mintArgDispatch()

mintAlign(_fa2)
    mintPopV(wfa2)
    mintArgDispatch()

mintAlign(_fa3)
    mintPopV(wfa3)
    mintArgDispatch()

mintAlign(_fa4)
    mintPopV(wfa4)
    mintArgDispatch()

mintAlign(_fa5)
    mintPopV(wfa5)
    mintArgDispatch()

mintAlign(_fa6)
    mintPopV(wfa6)
    mintArgDispatch()

mintAlign(_fa7)
    mintPopV(wfa7)
    mintArgDispatch()

# Note that the regular call and tail call opcodes will be implemented slightly differently.
# Regular calls have to save space for return values, while tail calls are reusing the stack frame
# and thus do not have to care.

# CallArgumentBytecode::CallArgDecSP (0x10)
mintAlign(_call_argument_dec_sp)
    subp 2 * SlotSize, sc3
    mintArgDispatch()

# CallArgumentBytecode::CallArgStore0 (0x11)
mintAlign(_call_argument_store_0)
    mintPop(sc2)
    storeq sc2, [sc3]
    mintArgDispatch()

# CallArgumentBytecode::CallArgDecSPStore8 (0x12)
mintAlign(_call_argument_dec_sp_store_8)
    mintPop(sc2)
    subp 2 * SlotSize, sc3
    storeq sc2, 8[sc3]
    mintArgDispatch()

# CallArgumentBytecode::CallArgDecSPStoreVector0 (0x13)
mintAlign(_call_argument_dec_sp_store_vector_0)
    subp 2 * SlotSize, sc3
    loadq [mintSS], sc2
    storeq sc2, [sc3]
    loadq 8[mintSS], sc2
    storeq sc2, 8[sc3]
    addq StackValueSize, mintSS
    mintArgDispatch()

# CallArgumentBytecode::TailCallArgDecSPStoreVector8 (0x14)
mintAlign(_call_argument_dec_sp_store_vector_8)
    subp 2 * SlotSize, sc3
    loadq [mintSS], sc2
    storeq sc2, 8[sc3]
    loadq 8[mintSS], sc2
    storeq sc2, 16[sc3]
    addq StackValueSize, mintSS
    mintArgDispatch()

# For tail calls, we're writing into the same frame. We're going to first push stack arguments onto the stack.
# Once we're done, we'll copy them back down into the new frame, to avoid having to deal with writing over
# arguments lower down on the stack.

# CallArgumentBytecode::TailCallArgDecSP (0x15)
mintAlign(_tail_call_argument_dec_sp)
    subp 2 * SlotSize, sp
    mintArgDispatch()

# CallArgumentBytecode::TailCallArgStore0 (0x16)
mintAlign(_tail_call_argument_store_0)
    mintPop(sc3)
    storeq sc3, [sp]
    mintArgDispatch()

# CallArgumentBytecode::TailCallArgDecSPStore8 (0x17)
mintAlign(_tail_call_argument_dec_sp_store_8)
    mintPop(sc3)
    subp 2 * SlotSize, sp
    storeq sc3, 8[sp]
    mintArgDispatch()

# CallArgumentBytecode::TailCallArgDecSPStoreVector0 (0x18)
mintAlign(_tail_call_argument_dec_sp_store_vector_0)
    subp 2 * SlotSize, sp
    loadq [mintSS], sc3
    storeq sc3, [sp]
    loadq 8[mintSS], sc3
    storeq sc3, 8[sp]
    addq StackValueSize, mintSS
    mintArgDispatch()

# CallArgumentBytecode::TailCallArgDecSPStoreVector8 (0x19)
mintAlign(_tail_call_argument_dec_sp_store_vector_8)
    subp 2 * SlotSize, sp
    loadq [mintSS], sc3
    storeq sc3, 8[sp]
    loadq 8[mintSS], sc3
    storeq sc3, 16[sp]
    addq StackValueSize, mintSS
    mintArgDispatch()

# CallArgumentBytecode::TailCall (0x1a)
mintAlign(_tail_call)
    jmp .ipint_perform_tail_call

# CallArgumentBytecode::Call (0x1b)
mintAlign(_call)
    pop wasmInstance, ws0
    # pop targetInstance, targetEntrypoint

    # Save stack pointer, if we tail call someone who changes the frame above's stack argument size.
    # Store its value relative to cfp so stack frames can be easily relocated for JSPI.
    move sp, sc1
    subp cfr, sc1
    storep sc1, ThisArgumentOffset[cfr]

    # Swap instances
    # move targetInstance, wasmInstance

    # Set up memory
    push t2, t3
    ipintReloadMemory()
    pop t3, t2

    # move targetEntrypoint, ws0

    # Make the call
if ARM64E
    leap _g_config, ws1
    jmp JSCConfigGateMapOffset + (constexpr Gate::wasm_ipint_call) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
end

_wasm_trampoline_wasm_ipint_call:
_wasm_trampoline_wasm_ipint_call_wide16:
_wasm_trampoline_wasm_ipint_call_wide32:
    call ws0, WasmEntryPtrTag

_wasm_ipint_call_return_location:
_wasm_ipint_call_return_location_wide16:
_wasm_ipint_call_return_location_wide32:
    # Restore the stack pointer
    loadp ThisArgumentOffset[cfr], sc0
    addp cfr, sc0
    move sc0, sp

    # <first non-arg>   <- first_non_arg_addr
    # arg
    # ...
    # arg
    # arg
    # reserved
    # reserved
    # (first_non_arg_addr - cfr), PC
    # PL, wasmInstance  <- sc3
    # call frame return
    # call frame return
    # call frame
    # call frame
    # call frame
    # call frame        <- sp

    loadi IPInt::CallReturnMetadata::stackFrameSize[MC], sc3
    leap [sp, sc3], sc3

    const mintRetSrc = sc1
    const mintRetDst = sc2

    loadi IPInt::CallReturnMetadata::firstStackResultSPOffset[MC], mintRetSrc
    advanceMC(IPInt::CallReturnMetadata::resultBytecode)
    leap [sp, mintRetSrc], mintRetSrc

    # load (first_non_arg_addr - cfr) from the stack and make it absolute
if ARM64 or ARM64E
    loadp (2 * SlotSize)[sc3], mintRetDst
elsif X86_64
    loadp (3 * SlotSize)[sc3], mintRetDst
end
    addp cfr, mintRetDst

    # on x86, we'll use PC again for our PC base
    initPCRelative(mint_ret, PC)

    // We've already validateOpcodeConfig() in all the Wasm call opcodes, and
    // that is the only way to get here.
    mintRetDispatch()

mintAlign(_r0)
_mint_begin_return:
    subp StackValueSize, mintRetDst
    storeq wa0, [mintRetDst]
    mintRetDispatch()

mintAlign(_r1)
    subp StackValueSize, mintRetDst
    storeq wa1, [mintRetDst]
    mintRetDispatch()

mintAlign(_r2)
if ARM64 or ARM64E or X86_64
    subp StackValueSize, mintRetDst
    storeq wa2, [mintRetDst]
    mintRetDispatch()
else
    break
end

mintAlign(_r3)
if ARM64 or ARM64E or X86_64
    subp StackValueSize, mintRetDst
    storeq wa3, [mintRetDst]
    mintRetDispatch()
else
    break
end

mintAlign(_r4)
if ARM64 or ARM64E or X86_64
    subp StackValueSize, mintRetDst
    storeq wa4, [mintRetDst]
    mintRetDispatch()
else
    break
end

mintAlign(_r5)
if ARM64 or ARM64E or X86_64
    subp StackValueSize, mintRetDst
    storeq wa5, [mintRetDst]
    mintRetDispatch()
else
    break
end

mintAlign(_r6)
if ARM64 or ARM64E
    subp StackValueSize, mintRetDst
    storeq wa6, [mintRetDst]
    mintRetDispatch()
else
    break
end

mintAlign(_r7)
if ARM64 or ARM64E
    subp StackValueSize, mintRetDst
    storeq wa7, [mintRetDst]
    mintRetDispatch()
else
    break
end

mintAlign(_fr0)
    subp StackValueSize, mintRetDst
    storev wfa0, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr1)
    subp StackValueSize, mintRetDst
    storev wfa1, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr2)
    subp StackValueSize, mintRetDst
    storev wfa2, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr3)
    subp StackValueSize, mintRetDst
    storev wfa3, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr4)
    subp StackValueSize, mintRetDst
    storev wfa4, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr5)
    subp StackValueSize, mintRetDst
    storev wfa5, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr6)
    subp StackValueSize, mintRetDst
    storev wfa6, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr7)
    subp StackValueSize, mintRetDst
    storev wfa7, [mintRetDst]
    mintRetDispatch()

# CallResultBytecode::ResultStack (0x10)
mintAlign(_result_stack)
    loadq [mintRetSrc], sc0
    addp SlotSize, mintRetSrc
    subp StackValueSize, mintRetDst
    storeq sc0, [mintRetDst]
    mintRetDispatch()

# CallResultBytecode::ResultStackVector (0x11)
mintAlign(_result_stack_vector)
    subp StackValueSize, mintRetDst
    loadq [mintRetSrc], sc0
    storeq sc0, [mintRetDst]
    loadq 8[mintRetSrc], sc0
    storeq sc0, 8[mintRetDst]
    addp 2 * SlotSize, mintRetSrc
    mintRetDispatch()

mintAlign(_end)

    # <first non-arg>   <- first_non_arg_addr
    # return result
    # ...
    # return result
    # return result
    # return result
    # return result     <- mintRetDst => new SP
    # (first_non_arg_addr - cfr), PC
    # PL, wasmInstance  <- sc3
    # call frame return <- mintRetSrc
    # call frame return
    # call frame
    # call frame
    # call frame
    # call frame        <- sp

    # note: we don't care about t3 anymore
if ARM64 or ARM64E
    loadpairq [sc3], PL, wasmInstance
else
    loadq [sc3], wasmInstance
end
    move mintRetDst, sp

if X86_64
    move wasmInstance, sc2
end

    # Restore PC / MC
    loadp Callee[cfr], ws0
    unboxWasmCallee(ws0, ws1)
    storep ws0, UnboxedWasmCalleeStackSlot[cfr]
if X86_64
    move sc2, wasmInstance
    loadq 8[sc3], PL
    loadp (2 * SlotSize)[sc3], PC
end

    # Restore memory
    ipintReloadMemory()
    nextIPIntInstruction()

.ipint_perform_tail_call:

    #  <caller frame>              <- sc2
    #  return val
    #  return val
    #  argument
    #  argument
    #  argument
    #  argument
    #  call frame
    #  call frame                  <- cfr
    #  (IPInt locals)
    #  (IPInt stack)               <- sc1 (was shadow stack, now dead and can re-use)
    #  argument 0
    #  ...
    #  argument n-1
    #  argument n
    #  entrypoint, targetInstance
    #  callee, function info
    #  saved MC/PC
    #  return address, saved CFR
    #  stack arguments
    #  stack arguments
    #  stack arguments
    #  stack arguments             <- sp

    # load the size of the arguments and results space, and subtract that from sc2
    loadi [MC], sc3
    negq sc3

    # copy args to sc2 region
    validateOpcodeConfig(sc0)
.ipint_tail_call_copy_stackargs_loop:
    bqgteq sc3, 0, .ipint_tail_call_copy_stackargs_loop_end
if ARM64 or ARM64E
    loadpairq [sp], sc0, sc1
    storepairq sc0, sc1, [sc2, sc3]
else
    loadq [sp], sc0
    loadq 8[sp], sc1
    storeq sc0, [sc2, sc3]
    storeq sc1, 8[sc2, sc3]
end

    addp 16, sc3
    addp 16, sp
    jmp .ipint_tail_call_copy_stackargs_loop

.ipint_tail_call_copy_stackargs_loop_end:

    # reload it here, which isn't optimal, but we don't really have registers
    loadi [MC], sc3
    subp sc3, sc2

    # re-setup the call frame, and load our return address in
    subp FirstArgumentOffset, sc2
if X86_64
    pop sc1, sc0
    storep sc0, ReturnPC[sc2]
elsif ARM64 or ARM64E or ARMv7 or RISCV64
    pop sc1, lr
end

    pop PC, MC

    # function info, callee
    pop sc3, sc0

    # save new Callee
    storeq sc0, Callee[sc2]
    storep sc3, CodeBlock[sc2]

    # take off the last two values we stored, and move SP down to make it look like a fresh frame
    pop targetInstance, ws0

    #  <caller frame>
    #  return val
    #  return val
    #  ...
    #  argument
    #  argument
    #  argument
    #  argument
    #  argument                    <- cfr
    #  argument
    #  argument
    #  <to be frame>
    #  <to be frame>               <- NEW SP
    #  <to be frame>               <- sc2
    #  argument 0
    #  ...
    #  argument n-1
    #  argument n

    # on ARM: lr = return address

    move sc2, sp
if ARM64E
    addp CallerFrameAndPCSize, cfr, ws2
end
    # saved cfr
    move sc1, cfr

    # swap instances
    move targetInstance, wasmInstance

    # set up memory
    push t2, t3
    ipintReloadMemory()
    pop t3, t2

    addp CallerFrameAndPCSize, sp

if X86_64
    subp 8, sp
end

    # go!
if ARM64E
    leap _g_config, ws1
    jmp JSCConfigGateMapOffset + (constexpr Gate::wasmIPIntTailCallWasmEntryPtrTag) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
end

_wasm_trampoline_wasm_ipint_tail_call:
_wasm_trampoline_wasm_ipint_tail_call_wide16:
_wasm_trampoline_wasm_ipint_tail_call_wide32:
    jmp ws0, WasmEntryPtrTag

_ipint_argument_dispatch_err:
    move 0x55, a0
    break
_ipint_uint_dispatch_err:
    move 0x66, a0
    break
_ipint_mint_arg_dispatch_err:
    move 0x77, a0
    break
_ipint_mint_ret_dispatch_err:
    move 0x88, a0
    break

###########################################
# uINT: function return value interpreter #
###########################################

uintAlign(_r0)
_uint_begin:
    popQuad(wa0)
    uintDispatch()

uintAlign(_r1)
    popQuad(wa1)
    uintDispatch()

uintAlign(_r2)
    popQuad(wa2)
    uintDispatch()

uintAlign(_r3)
    popQuad(wa3)
    uintDispatch()

uintAlign(_r4)
    popQuad(wa4)
    uintDispatch()

uintAlign(_r5)
    popQuad(wa5)
    uintDispatch()

uintAlign(_r6)
if ARM64 or ARM64E
    popQuad(wa6)
    uintDispatch()
else
    break
end

uintAlign(_r7)
if ARM64 or ARM64E
    popQuad(wa7)
    uintDispatch()
else
    break
end

uintAlign(_fr0)
    popVec(wfa0)
    uintDispatch()

uintAlign(_fr1)
    popVec(wfa1)
    uintDispatch()

uintAlign(_fr2)
    popVec(wfa2)
    uintDispatch()

uintAlign(_fr3)
    popVec(wfa3)
    uintDispatch()

uintAlign(_fr4)
    popVec(wfa4)
    uintDispatch()

uintAlign(_fr5)
    popVec(wfa5)
    uintDispatch()

uintAlign(_fr6)
    popVec(wfa6)
    uintDispatch()

uintAlign(_fr7)
    popVec(wfa7)
    uintDispatch()

# destination on stack is sc0

uintAlign(_stack)
    popInt64(sc1)
    subp SlotSize, sc0
    storeq sc1, [sc0]
    uintDispatch()

uintAlign(_stack_vector)
    subp 2 * SlotSize, sc0
    loadq [sp], sc1
    storeq sc1, [sc0]
    loadq 8[sp], sc1
    storeq sc1, 8[sc0]
    addq StackValueSize, sp
    uintDispatch()

uintAlign(_ret)
    jmp .ipint_exit

# MC = location in argumINT bytecode
# csr0 = tmp
# csr1 = dst
# csr2 = src
# csr3
# csr4 = for dispatch

# const argumINTDest = csr3
# const argumINTSrc = PB

argumINTAlign(_a0)
_argumINT_begin:
    storeq wa0, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_a1)
    storeq wa1, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_a2)
if ARM64 or ARM64E or X86_64
    storeq wa2, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()
else
    break
end


argumINTAlign(_a3)
if ARM64 or ARM64E or X86_64
    storeq wa3, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()
else
    break
end

argumINTAlign(_a4)
if ARM64 or ARM64E or X86_64
    storeq wa4, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()
else
    break
end

argumINTAlign(_a5)
if ARM64 or ARM64E or X86_64
    storeq wa5, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()
else
    break
end

argumINTAlign(_a6)
if ARM64 or ARM64E
    storeq wa6, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()
else
    break
end

argumINTAlign(_a7)
if ARM64 or ARM64E
    storeq wa7, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()
else
    break
end

argumINTAlign(_fa0)
    storev wfa0, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa1)
    storev wfa1, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa2)
    storev wfa2, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa3)
    storev wfa3, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa4)
    storev wfa4, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa5)
    storev wfa5, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa6)
    storev wfa6, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa7)
    storev wfa7, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_stack)
    loadq [argumINTSrc], csr0
    addp SlotSize, argumINTSrc
    storeq csr0, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_stack_vector)
    loadq [argumINTSrc], csr0
    storeq csr0, [argumINTDst]
    loadq 8[argumINTSrc], csr0
    storeq csr0, 8[argumINTDst]
    addp 2 * SlotSize, argumINTSrc
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_end)
    jmp .ipint_entry_end_local

if ARM64E
    global _wasmTailCallTrampoline
    _wasmTailCallTrampoline:
        untagReturnAddress ws2
        jmp ws0, WasmEntryPtrTag
end
