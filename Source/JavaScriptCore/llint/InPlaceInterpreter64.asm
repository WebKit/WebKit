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
    # NOTE: We intentionally don't restore memoryBase and boundsCheckingSize here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    subp IPIntCalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        storepairq MC, PC, -0x10[cfr]
        storeq wasmInstance, -0x18[cfr]
    elsif X86_64 or RISCV64
        storep PC, -0x8[cfr]
        storep MC, -0x10[cfr]
        storep wasmInstance, -0x18[cfr]
    end
end

macro restoreIPIntRegisters()
    # NOTE: We intentionally don't restore memoryBase and boundsCheckingSize here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    if ARM64 or ARM64E
        loadpairq -0x10[cfr], MC, PC
        loadq -0x18[cfr], wasmInstance
    elsif X86_64 or RISCV64
        loadp -0x8[cfr], PC
        loadp -0x10[cfr], MC
        loadp -0x18[cfr], wasmInstance
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
    emit "add x0, x7, x0, lsl #8"
    emit "br x0"
elsif X86_64
    leap _g_opcodeConfigStorage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_dispatch_base[t1], t1
    lshiftq 8, t0
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
    push reg
end

macro popVec(reg)
    pop reg
end

# Typed push/pop to make code pretty

macro pushInt32(reg)
    pushQuad(reg)
end

macro popInt32(reg, scratch)
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

macro popInt64(reg, scratch)
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
    mulq LocalSize, argumINTEnd
    mulq LocalSize, argumINTTmp
    subq argumINTEnd, sp
    move sp, argumINTEnd
    subq argumINTTmp, sp
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
    addq 1, MC
    bbgteq argumINTTmp, (constexpr IPInt::ArgumINTBytecode::NumOpcodes), _ipint_argument_dispatch_err
    lshiftq 6, argumINTTmp
if ARM64 or ARM64E
    pcrtoaddr _argumINT_begin, argumINTDsp
    addq argumINTTmp, argumINTDsp
    emit "br x23"
elsif X86_64
    leap (_argumINT_begin - _ipint_entry_relativePCBase)[PL], argumINTDsp
    addq argumINTTmp, argumINTDsp
    jmp argumINTDsp
else
    break
end
end

macro argumINTInitializeDefaultLocals()
    # zero out remaining locals
    bqeq argumINTDst, argumINTEnd, .ipint_entry_finish_zero
    loadb [MC], argumINTTmp
    addq 1, MC
    sxb2q argumINTTmp, argumINTTmp
    andq ValueNull, argumINTTmp
    storeq argumINTTmp, [argumINTDst]
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
    popInt32(t0, t1)
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
    lshiftq 6, sc2
    pcrtoaddr _uint_begin, sc3
    addq sc2, ws3
    # ws3 = x12
    emit "br x12"
elsif X86_64
    loadb [MC], sc1
    addq 1, MC
    bilt sc1, 0x12, .safe
    break
.safe:
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
    loadi Wasm::IPIntCallee::m_highestReturnStackOffset[ws0], sc0
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
    popInt32(t0, t2)
    bineq t0, 0, _ipint_br
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_br_table, macro()
    # br_table
    validateOpcodeConfig(t2)
    popInt32(t0, t2)
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

    # get function index
    loadi IPInt::CallMetadata::functionIndex[MC], a1
    advanceMC(IPInt::CallMetadata::signature)

    subq 16, sp
    move sp, a2

    # operation returns the entrypoint in r0 and the target instance in r1
    # operation stores the target callee to sp[0] and target function info to sp[1]
    operationCall(macro() cCall3(_ipint_extern_prepare_call) end)
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

    # get function index
    loadi IPInt::TailCallMetadata::functionIndex[MC], a1

    subq 16, sp
    move sp, a2

    # operation returns the entrypoint in r0 and the target instance in r1
    # this operation stores the boxed Callee into *r2
    operationCall(macro() cCall3(_ipint_extern_prepare_call) end)

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
    loadi IPInt::CallRefMetadata::typeIndex[MC], a2
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
    loadi IPInt::TailCallRefMetadata::typeIndex[MC], a2
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
    popInt32(t0, t2)
    bieq t0, 0, .ipint_select_val2
    addq StackValueSize, sp
    advancePC(1)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_select_val2:
    popQuad(t1)
    popQuad(t0)
    pushQuad(t1)
    advancePC(1)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_select_t, macro()
    popInt32(t0, t2)
    bieq t0, 0, .ipint_select_t_val2
    addq StackValueSize, sp
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_select_t_val2:
    popQuad(t1)
    popQuad(t0)
    pushQuad(t1)
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
    loadq [PL, t0], t0
    # Push to stack
    pushQuad(t0)
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
    popQuad(t2)
    # Store to locals
    mulq LocalSize, t0
    storeq t2, [PL, t0]
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
    loadq [sp], t2
    # Store to locals
    mulq LocalSize, t0
    storeq t2, [PL, t0]
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
    # Load pre-computed index from metadata
    loadb IPInt::GlobalMetadata::bindingMode[MC], t2
    loadi IPInt::GlobalMetadata::index[MC], t1
    loadp JSWebAssemblyInstance::m_globals[wasmInstance], t0
    lshiftp 1, t1
    loadq [t0, t1, 8], t0
    bieq t2, 0, .ipint_global_get_embedded
    loadq [t0], t0
.ipint_global_get_embedded:
    pushQuad(t0)

    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
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
    popQuad(t3)
    # get index
    loadi IPInt::GlobalMetadata::index[MC], t1
    lshiftp 1, t1
    bieq t2, 0, .ipint_global_set_embedded
    # portable: dereference then set
    loadq [t0, t1, 8], t0
    storeq t3, [t0]
    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    nextIPIntInstruction()
.ipint_global_set_embedded:
    # embedded: set directly
    storeq t3, [t0, t1, 8]
    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    nextIPIntInstruction()

.ipint_global_set_refpath:
    loadi IPInt::GlobalMetadata::index[MC], a1
    # Pop from stack
    popQuad(a2)
    operationCall(macro() cCall3(_ipint_extern_set_global_ref) end)

    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_get, macro()
    # Load pre-computed index from metadata
    loadi IPInt::Const32Metadata::value[MC], a1
    popInt32(a2, t3)

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
    popInt32(a2, t0)
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_set) end)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

reservedOpcode(0x27)

macro popMemoryIndex(reg, tmp)
    popInt32(reg, tmp)
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
    popInt32(t1, t2)
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
    popInt64(t1, t2)
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
    popInt32(t1, t2)
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
    popInt32(t1, t2)
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
    popInt64(t1, t2)
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
    popInt64(t1, t2)
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
    popInt64(t1, t2)
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
    operationCall(macro() cCall2(_ipint_extern_current_memory) end)
    pushInt32(r0)
    advancePC(2)
    nextIPIntInstruction()
end)

ipintOp(_memory_grow, macro()
    popInt32(a1, t2)
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
    popInt32(t0, t2)
    cieq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_eq, macro()
    # i32.eq
    popInt32(t1, t2)
    popInt32(t0, t2)
    cieq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ne, macro()
    # i32.ne
    popInt32(t1, t2)
    popInt32(t0, t2)
    cineq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_lt_s, macro()
    # i32.lt_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cilt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_lt_u, macro()
    # i32.lt_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cib t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_gt_s, macro()
    # i32.gt_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cigt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_gt_u, macro()
    # i32.gt_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cia t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_le_s, macro()
    # i32.le_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cilteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_le_u, macro()
    # i32.le_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cibeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ge_s, macro()
    # i32.ge_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cigteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ge_u, macro()
    # i32.ge_u
    popInt32(t1, t2)
    popInt32(t0, t2)
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
    popInt64(t0, t2)
    cqeq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_eq, macro()
    # i64.eq
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ne, macro()
    # i64.ne
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqneq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_lt_s, macro()
    # i64.lt_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqlt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_lt_u, macro()
    # i64.lt_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqb t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_gt_s, macro()
    # i64.gt_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqgt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_gt_u, macro()
    # i64.gt_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqa t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_le_s, macro()
    # i64.le_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqlteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_le_u, macro()
    # i64.le_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqbeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ge_s, macro()
    # i64.ge_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqgteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ge_u, macro()
    # i64.ge_u
    popInt64(t1, t2)
    popInt64(t0, t2)
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
    popInt32(t0, t2)
    lzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_ctz, macro()
    # i32.ctz
    popInt32(t0, t2)
    tzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_popcnt, macro()
    # i32.popcnt
    popInt32(t1, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcount) end)
    pushInt32(r1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_add, macro()
    # i32.add
    popInt32(t1, t2)
    popInt32(t0, t2)
    addi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_sub, macro()
    # i32.sub
    popInt32(t1, t2)
    popInt32(t0, t2)
    subi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_mul, macro()
    # i32.mul
    popInt32(t1, t2)
    popInt32(t0, t2)
    muli t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_div_s, macro()
    # i32.div_s
    popInt32(t1, t2)
    popInt32(t0, t2)
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
    popInt32(t1, t2)
    popInt32(t0, t2)
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
    popInt32(t1, t2)
    popInt32(t0, t2)

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
    popInt32(t1, t2)
    popInt32(t0, t2)
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
    popInt32(t1, t2)
    popInt32(t0, t2)
    andi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_or, macro()
    # i32.or
    popInt32(t1, t2)
    popInt32(t0, t2)
    ori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_xor, macro()
    # i32.xor
    popInt32(t1, t2)
    popInt32(t0, t2)
    xori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_shl, macro()
    # i32.shl
    popInt32(t1, t2)
    popInt32(t0, t2)
    lshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_shr_s, macro()
    # i32.shr_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    rshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_shr_u, macro()
    # i32.shr_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    urshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_rotl, macro()
    # i32.rotl
    popInt32(t1, t2)
    popInt32(t0, t2)
    lrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_rotr, macro()
    # i32.rotr
    popInt32(t1, t2)
    popInt32(t0, t2)
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
    popInt64(t0, t2)
    lzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ctz, macro()
    # i64.ctz
    popInt64(t0, t2)
    tzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_popcnt, macro()
    # i64.popcnt
    popInt64(t1, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcountll) end)
    pushInt64(r1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_add, macro()
    # i64.add
    popInt64(t1, t2)
    popInt64(t0, t2)
    addq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_sub, macro()
    # i64.sub
    popInt64(t1, t2)
    popInt64(t0, t2)
    subq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_mul, macro()
    # i64.mul
    popInt64(t1, t2)
    popInt64(t0, t2)
    mulq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_div_s, macro()
    # i64.div_s
    popInt64(t1, t2)
    popInt64(t0, t2)
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
    popInt64(t1, t2)
    popInt64(t0, t2)
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
    popInt64(t1, t2)
    popInt64(t0, t2)

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
    popInt64(t1, t2)
    popInt64(t0, t2)
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
    popInt64(t1, t2)
    popInt64(t0, t2)
    andq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_or, macro()
    # i64.or
    popInt64(t1, t2)
    popInt64(t0, t2)
    orq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_xor, macro()
    # i64.xor
    popInt64(t1, t2)
    popInt64(t0, t2)
    xorq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shl, macro()
    # i64.shl
    popInt64(t1, t2)
    popInt64(t0, t2)
    lshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shr_s, macro()
    # i64.shr_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    rshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shr_u, macro()
    # i64.shr_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    urshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_rotl, macro()
    # i64.rotl
    popInt64(t1, t2)
    popInt64(t0, t2)
    lrotateq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_rotr, macro()
    # i64.rotr
    popInt64(t1, t2)
    popInt64(t0, t2)
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
    popInt32(t0, t1)
    sxi2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend_i32_u, macro()
    popInt32(t0, t1)
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
    popInt32(t0, t1)
    andq 0xffffffff, t0
    ci2fs t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i32_u, macro()
    popInt32(t0, t1)
    andq 0xffffffff, t0
    ci2f t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i64_s, macro()
    popInt64(t0, t1)
    cq2fs t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i64_u, macro()
    popInt64(t0, t1)
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
    popInt32(t0, t1)
    andq 0xffffffff, t0
    ci2ds t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i32_u, macro()
    popInt32(t0, t1)
    andq 0xffffffff, t0
    ci2d t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i64_s, macro()
    popInt64(t0, t1)
    cq2ds t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i64_u, macro()
    popInt64(t0, t1)
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
    popInt32(t0, t1)
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i32_extend16_s, macro()
    # i32.extend8_s
    popInt32(t0, t1)
    sxh2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend8_s, macro()
    # i64.extend8_s
    popInt64(t0, t1)
    sxb2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend16_s, macro()
    # i64.extend8_s
    popInt64(t0, t1)
    sxh2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend32_s, macro()
    # i64.extend8_s
    popInt64(t0, t1)
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
    move wasmInstance, a0
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
    leap _g_opcodeConfigStorage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_gc_dispatch_base[t1], t1
    if ARM64 or ARM64E
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
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
    leap _g_opcodeConfigStorage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_conversion_dispatch_base[t1], t1
    if ARM64 or ARM64E
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
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
    leap _g_opcodeConfigStorage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_simd_dispatch_base[t1], t1
    if ARM64 or ARM64E
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
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
    leap _g_opcodeConfigStorage, t1
    loadp JSC::LLInt::OpcodeConfig::ipint_atomic_dispatch_base[t1], t1
    if ARM64 or ARM64E
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
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
    loadp IPInt::StructNewMetadata::typeIndex[MC], a1  # type index
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
    loadp IPInt::StructNewDefaultMetadata::typeIndex[MC], a1  # type index
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
    operationCallMayThrow(macro() cCall3(_ipint_extern_struct_get) end)
    pushQuad(r0)

    loadb IPInt::StructGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_struct_get_s, macro()
    popQuad(a1)  # object
    loadi IPInt::StructGetSetMetadata::fieldIndex[MC], a2  # field index
    operationCallMayThrow(macro() cCall3(_ipint_extern_struct_get_s) end)
    pushQuad(r0)

    loadb IPInt::StructGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::StructGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_struct_get_u, macro()
    popQuad(a1)  # object
    loadi IPInt::StructGetSetMetadata::fieldIndex[MC], a2  # field index
    operationCallMayThrow(macro() cCall3(_ipint_extern_struct_get) end)
    pushQuad(r0)

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
    loadi IPInt::ArrayNewMetadata::typeIndex[MC], a1  # type index
    popInt32(a3, t0)  # length
    popQuad(a2)  # default value
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_new) end)

    pushQuad(r0)

    loadb IPInt::ArrayNewMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new_default, macro()
    loadi IPInt::ArrayNewMetadata::typeIndex[MC], a1  # type index
    popInt32(a2, t0)  # length
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_new_default) end)

    pushQuad(r0)

    loadb IPInt::ArrayNewMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new_fixed, macro()
    loadi IPInt::ArrayNewFixedMetadata::typeIndex[MC], a1  # type index
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
    popInt32(a3, t0)  # size
    popInt32(a2, t0)  # offset
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_new_data) end)

    pushQuad(r0)

    loadb IPInt::ArrayNewDataMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewDataMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_new_elem, macro()
    move MC, a1  # metadata
    popInt32(a3, t0)  # size
    popInt32(a2, t0)  # offset
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_new_elem) end)

    pushQuad(r0)

    loadb IPInt::ArrayNewElemMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayNewElemMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_get, macro()
    loadi IPInt::ArrayGetSetMetadata::typeIndex[MC], a1  # type index
    popInt32(a3, a0)  # index
    popQuad(a2)  # array
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_get) end)

    pushQuad(r0)

    loadb IPInt::ArrayGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_get_s, macro()
    loadi IPInt::ArrayGetSetMetadata::typeIndex[MC], a1  # type index
    popInt32(a3, a0)  # index
    popQuad(a2)  # array
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_get_s) end)

    pushQuad(r0)

    loadb IPInt::ArrayGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_get_u, macro()
    loadi IPInt::ArrayGetSetMetadata::typeIndex[MC], a1  # type index
    popInt32(a3, a0)  # index
    popQuad(a2)  # array
    operationCallMayThrow(macro() cCall4(_ipint_extern_array_get) end)

    pushQuad(r0)

    loadb IPInt::ArrayGetSetMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayGetSetMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_set, macro()
    loadi IPInt::ArrayGetSetMetadata::typeIndex[MC], a1  # type index
    move sp, a2  # stack pointer with all the arguments
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_set) end)

    addq StackValueSize*3, sp

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
    throwException(NullArrayLen)
end)

ipintOp(_array_fill, macro()
    move sp, a1
    operationCallMayThrow(macro() cCall2(_ipint_extern_array_fill) end)

    addp 4*StackValueSize, sp

    loadb IPInt::ArrayFillMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayFillMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_copy, macro()
    move sp, a1
    operationCallMayThrow(macro() cCall2(_ipint_extern_array_copy) end)

    addp 5*StackValueSize, sp

    loadb IPInt::ArrayFillMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayCopyMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_init_data, macro()
    loadi IPInt::ArrayInitDataMetadata::dataSegmentIndex[MC], a1
    move sp, a2
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_init_data) end)

    addp 4*StackValueSize, sp

    loadb IPInt::ArrayInitDataMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayInitDataMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_array_init_elem, macro()
    loadi IPInt::ArrayInitElemMetadata::elemSegmentIndex[MC], a1
    move sp, a2
    operationCallMayThrow(macro() cCall3(_ipint_extern_array_init_elem) end)

    addp 4*StackValueSize, sp

    loadb IPInt::ArrayInitElemMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::ArrayInitElemMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_test, macro()
    loadi IPInt::RefTestCastMetadata::typeIndex[MC], a1
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
    loadi IPInt::RefTestCastMetadata::typeIndex[MC], a1
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
    loadi IPInt::RefTestCastMetadata::typeIndex[MC], a1
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
    loadi IPInt::RefTestCastMetadata::typeIndex[MC], a1
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
    loadi IPInt::RefTestCastMetadata::typeIndex[MC], a1
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
    loadi IPInt::RefTestCastMetadata::typeIndex[MC], a1
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
    popInt32(t0, t1)
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
    addp 2*StackValueSize, sp
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

# 0xFD 0x00 - 0xFD 0x0B: memory

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

# 0xFD 0x0C: v128.const
ipintOp(_simd_v128_const, macro()
    # v128.const
    loadv 2[PC], v0
    pushv v0
    advancePC(18)
    nextIPIntInstruction()
end)

# 0xFD 0x0D - 0xFD 0x14: splat (+ shuffle/swizzle)
unimplementedInstruction(_simd_i8x16_shuffle)
unimplementedInstruction(_simd_i8x16_swizzle)
unimplementedInstruction(_simd_i8x16_splat)
unimplementedInstruction(_simd_i16x8_splat)
unimplementedInstruction(_simd_i32x4_splat)
unimplementedInstruction(_simd_i64x2_splat)
unimplementedInstruction(_simd_f32x4_splat)
unimplementedInstruction(_simd_f64x2_splat)

# 0xFD 0x15 - 0xFD 0x22: extract and replace lanes
unimplementedInstruction(_simd_i8x16_extract_lane_s)
unimplementedInstruction(_simd_i8x16_extract_lane_u)
unimplementedInstruction(_simd_i8x16_replace_lane)
unimplementedInstruction(_simd_i16x8_extract_lane_s)
unimplementedInstruction(_simd_i16x8_extract_lane_u)
unimplementedInstruction(_simd_i16x8_replace_lane)

ipintOp(_simd_i32x4_extract_lane, macro()
    # i32x4.extract_lane (lane)
    loadb 2[PC], t0  # lane index
    andi 0x3, t0
    popv v0
    if ARM64 or ARM64E
        pcrtoaddr _simd_i32x4_extract_lane_0, t1
        leap [t1, t0, 8], t0
        emit "br x0"
        _simd_i32x4_extract_lane_0:
        umovi t0, v0_i, 0
        jmp _simd_i32x4_extract_lane_end
        umovi t0, v0_i, 1
        jmp _simd_i32x4_extract_lane_end
        umovi t0, v0_i, 2
        jmp _simd_i32x4_extract_lane_end
        umovi t0, v0_i, 3
        jmp _simd_i32x4_extract_lane_end
    elsif X86_64
        # FIXME: implement SIMD instructions for x86 and finish this implementation!
    end
_simd_i32x4_extract_lane_end:
    pushInt32(t0)
    advancePC(3)
    nextIPIntInstruction()
end)

unimplementedInstruction(_simd_i32x4_replace_lane)
unimplementedInstruction(_simd_i64x2_extract_lane)
unimplementedInstruction(_simd_i64x2_replace_lane)
unimplementedInstruction(_simd_f32x4_extract_lane)
unimplementedInstruction(_simd_f32x4_replace_lane)
unimplementedInstruction(_simd_f64x2_extract_lane)
unimplementedInstruction(_simd_f64x2_replace_lane)

# 0xFD 0x23 - 0xFD 0x2C: i8x16 operations
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

# 0xFD 0x2D - 0xFD 0x36: i8x16 operations
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

# 0xFD 0x37 - 0xFD 0x40: i32x4 operations
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

# 0xFD 0x41 - 0xFD 0x46: f32x4 operations
unimplementedInstruction(_simd_f32x4_eq)
unimplementedInstruction(_simd_f32x4_ne)
unimplementedInstruction(_simd_f32x4_lt)
unimplementedInstruction(_simd_f32x4_gt)
unimplementedInstruction(_simd_f32x4_le)
unimplementedInstruction(_simd_f32x4_ge)

# 0xFD 0x47 - 0xFD 0x4c: f64x2 operations
unimplementedInstruction(_simd_f64x2_eq)
unimplementedInstruction(_simd_f64x2_ne)
unimplementedInstruction(_simd_f64x2_lt)
unimplementedInstruction(_simd_f64x2_gt)
unimplementedInstruction(_simd_f64x2_le)
unimplementedInstruction(_simd_f64x2_ge)

# 0xFD 0x4D - 0xFD 0x53: v128 operations
unimplementedInstruction(_simd_v128_not)
unimplementedInstruction(_simd_v128_and)
unimplementedInstruction(_simd_v128_andnot)
unimplementedInstruction(_simd_v128_or)
unimplementedInstruction(_simd_v128_xor)
unimplementedInstruction(_simd_v128_bitselect)
unimplementedInstruction(_simd_v128_any_true)

# 0xFD 0x54 - 0xFD 0x5D: v128 load/store lane
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

# 0xFD 0x5E - 0xFD 0x5F: f32x4/f64x2 conversion
unimplementedInstruction(_simd_f32x4_demote_f64x2_zero)
unimplementedInstruction(_simd_f64x2_promote_low_f32x4)

# 0xFD 0x60 - 0x66: i8x16 operations
unimplementedInstruction(_simd_i8x16_abs)
unimplementedInstruction(_simd_i8x16_neg)
unimplementedInstruction(_simd_i8x16_popcnt)
unimplementedInstruction(_simd_i8x16_all_true)
unimplementedInstruction(_simd_i8x16_bitmask)
unimplementedInstruction(_simd_i8x16_narrow_i16x8_s)
unimplementedInstruction(_simd_i8x16_narrow_i16x8_u)

# 0xFD 0x67 - 0xFD 0x6A: f32x4 operations
unimplementedInstruction(_simd_f32x4_ceil)
unimplementedInstruction(_simd_f32x4_floor)
unimplementedInstruction(_simd_f32x4_trunc)
unimplementedInstruction(_simd_f32x4_nearest)

# 0xFD 0x6B - 0xFD 0x73: i8x16 binary operations
unimplementedInstruction(_simd_i8x16_shl)
unimplementedInstruction(_simd_i8x16_shr_s)
unimplementedInstruction(_simd_i8x16_shr_u)
unimplementedInstruction(_simd_i8x16_add)
unimplementedInstruction(_simd_i8x16_add_sat_s)
unimplementedInstruction(_simd_i8x16_add_sat_u)
unimplementedInstruction(_simd_i8x16_sub)
unimplementedInstruction(_simd_i8x16_sub_sat_s)
unimplementedInstruction(_simd_i8x16_sub_sat_u)

# 0xFD 0x74 - 0xFD 0x75: f64x2 operations
unimplementedInstruction(_simd_f64x2_ceil)
unimplementedInstruction(_simd_f64x2_floor)

# 0xFD 0x76 - 0xFD 0x79: i8x16 binary operations
unimplementedInstruction(_simd_i8x16_min_s)
unimplementedInstruction(_simd_i8x16_min_u)
unimplementedInstruction(_simd_i8x16_max_s)
unimplementedInstruction(_simd_i8x16_max_u)

# 0xFD 0x7A: f64x2 trunc
unimplementedInstruction(_simd_f64x2_trunc)

# 0xFD 0x7B: i8x16 avgr_u
unimplementedInstruction(_simd_i8x16_avgr_u)

# 0xFD 0x7C - 0xFD 0x7F: extadd_pairwise
unimplementedInstruction(_simd_i16x8_extadd_pairwise_i8x16_s)
unimplementedInstruction(_simd_i16x8_extadd_pairwise_i8x16_u)
unimplementedInstruction(_simd_i32x4_extadd_pairwise_i16x8_s)
unimplementedInstruction(_simd_i32x4_extadd_pairwise_i16x8_u)

# 0xFD 0x80 0x01 - 0xFD 0x93 0x01: i16x8 operations

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

# 0xFD 0x94 0x01: f64x2.nearest

unimplementedInstruction(_simd_f64x2_nearest)

# 0xFD 0x95 0x01 - 0xFD 0x9F 0x01: i16x8 operations

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

# 0xFD 0xA0 0x01 - 0xFD 0xBF 0x01: i32x4 operations

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

# 0xFD 0xC0 0x01 - 0xFD 0xDF 0x01: i64x2 operations

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

# 0xFD 0xE0 0x01 - 0xFD 0xEB 0x01: f32x4 operations

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

# 0xFD 0xEC 0x01 - 0xFD 0xF7 0x01: f64x2 operations

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

# 0xFD 0xF8 0x01 - 0xFD 0xFF 0x01: trunc/convert

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
    popInt32(a3, t0)
    # pop pointer
    popInt32(a1, t0)
    # load offset
    loadi IPInt::Const32Metadata::value[MC], a2

    move wasmInstance, a0
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
    popInt32(a3, t0)
    # pop value
    popInt32(a2, t0)
    # pop pointer
    popInt32(a1, t0)
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t0
    # merge them since the slow path takes the combined pointer + offset.
    addq t0, a1

    move wasmInstance, a0
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
    popInt32(a3, t0)
    # pop value
    popInt64(a2, t0)
    # pop pointer
    popInt32(a1, t0)
    # load offset
    loadi IPInt::Const32Metadata::value[MC], t0
    # merge them since the slow path takes the combined pointer + offset.
    addq t0, a1

    move wasmInstance, a0
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
    popInt32(t0, t2)
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
    popInt64(t1, t0)
    # pop index
    popInt32(t2, t0)
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
    popInt64(t1, t0)
    # pop index
    popInt32(t2, t0)
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
    popInt64(t1, t2)
    # pop expected
    popInt64(t0, t2)
    # pop index
    popInt32(t3, t2)
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

macro mintPopF(reg)
    loadd [mintSS], reg
    addq V128ISize, mintSS
end

macro mintArgDispatch()
    loadb [MC], sc0
    addq 1, MC
    bigteq sc0, (constexpr IPInt::CallArgumentBytecode::NumOpcodes), _ipint_mint_arg_dispatch_err
    lshiftq 6, sc0
if ARM64 or ARM64E
    pcrtoaddr _mint_begin, csr4
    addq sc0, csr4
    # csr4 = x23
    emit "br x23"
elsif X86_64
    leap (_mint_begin - _mint_arg_relativePCBase)[PC, sc0], sc0
    jmp sc0
end
end

macro mintRetDispatch()
    loadb [MC], sc0
    addq 1, MC
    bigteq sc0, (constexpr IPInt::CallResultBytecode::NumOpcodes), _ipint_mint_ret_dispatch_err
    lshiftq 6, sc0
if ARM64 or ARM64E
    pcrtoaddr _mint_begin_return, csr4
    addq sc0, csr4
    # csr4 = x23
    emit "br x23"
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

    # sc2 = target entrypoint
    # sc3 = target instance

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
    # arg             <- initial SP

    # store sp as our shadow stack for arguments later
    move sp, t4
    # make extra space if necessary
    subp extraSpaceForReturns, sp

    # <first non-arg> <- t3
    # arg
    # ...
    # arg
    # arg             <- sc0 = initial sp
    # reserved
    # reserved        <- sp

    push t3, PC
    push PL, wasmInstance

    # set up the call frame
    move sp, t2
    subp stackFrameSize, sp

    # <first non-arg> <- t3
    # arg
    # ...
    # arg
    # arg             <- sc0 = initial sp
    # reserved
    # reserved
    # t3, PC
    # PL, wasmInstance
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
    addp t3, sc2

    #  <caller frame>
    #  return val                  <- sc2
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

    #  <caller frame>
    #  return val                  <- sc2
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
    mintPopF(wfa0)
    mintArgDispatch()

mintAlign(_fa1)
    mintPopF(wfa1)
    mintArgDispatch()

mintAlign(_fa2)
    mintPopF(wfa2)
    mintArgDispatch()

mintAlign(_fa3)
    mintPopF(wfa3)
    mintArgDispatch()

mintAlign(_fa4)
    mintPopF(wfa4)
    mintArgDispatch()

mintAlign(_fa5)
    mintPopF(wfa5)
    mintArgDispatch()

mintAlign(_fa6)
    mintPopF(wfa6)
    mintArgDispatch()

mintAlign(_fa7)
    mintPopF(wfa7)
    mintArgDispatch()

# Note that the regular call and tail call opcodes will be implemented slightly differently.
# Regular calls have to save space for return values, while tail calls are reusing the stack frame
# and thus do not have to care.

mintAlign(_stackzero)
    mintPop(sc2)
    storeq sc2, [sc3]
    mintArgDispatch()

mintAlign(_stackeight)
    mintPop(sc2)
    subp 16, sc3
    storeq sc2, 8[sc3]
    mintArgDispatch()

# Since we're writing into the same frame, we're going to first push stack arguments onto the stack.
# Once we're done, we'll copy them back down into the new frame, to avoid having to deal with writing over
# arguments lower down on the stack.

mintAlign(_tail_stackzero)
    mintPop(sc3)
    storeq sc3, [sp]
    mintArgDispatch()

mintAlign(_tail_stackeight)
    mintPop(sc3)
    subp 16, sp
    storeq sc3, 8[sp]
    mintArgDispatch()

mintAlign(_gap)
    subp 16, sc3
    mintArgDispatch()

mintAlign(_tail_gap)
    subp 16, sp
    mintArgDispatch()

mintAlign(_tail_call)
    jmp .ipint_perform_tail_call

mintAlign(_call)
    pop wasmInstance, ws0
    # pop targetInstance, targetEntrypoint

    # Save stack pointer, if we tail call someone who changes the frame above's stack argument size
    move sp, sc1
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
    move sc0, sp

    # <first non-arg>   <- t3
    # arg
    # ...
    # arg
    # arg
    # reserved
    # reserved
    # t3, PC
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

    loadi IPInt::CallReturnMetadata::firstStackArgumentSPOffset[MC], mintRetSrc
    advanceMC(IPInt::CallReturnMetadata::resultBytecode)
    leap [sp, mintRetSrc], mintRetSrc

if ARM64 or ARM64E
    loadp 2*SlotSize[sc3], mintRetDst
elsif X86_64
    loadp 3*SlotSize[sc3], mintRetDst
end

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

mintAlign(_stack)
    loadq [mintRetSrc], sc0
    addp SlotSize, mintRetSrc
    subp StackValueSize, mintRetDst
    storeq sc0, [mintRetDst]
    mintRetDispatch()

mintAlign(_stack_gap)
    addp SlotSize, mintRetSrc
    mintRetDispatch()

mintAlign(_end)

    # <first non-arg>   <- t3
    # return result
    # ...
    # return result
    # return result
    # return result
    # return result     <- mintRetDst => new SP
    # t3, PC
    # PL, wasmInstance  <- sc3
    # call frame return <- sp
    # call frame return
    # call frame
    # call frame
    # call frame
    # call frame

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
    getIPIntCallee()
if X86_64
    move sc2, wasmInstance
    loadq 8[sc3], PL
    loadp 2*SlotSize[sc3], PC
end

    # Restore memory
    ipintReloadMemory()
    nextIPIntInstruction()

.ipint_perform_tail_call:

    #  <caller frame>
    #  return val                  <- sc2
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
    #  return address, saved CFR
    #  stack arguments
    #  stack arguments
    #  stack arguments
    #  stack arguments             <- sp

    # load the size of stack values in, and subtract that from sc2
    loadi [MC], sc3
    mulp -SlotSize, sc3

    # copy from sc2 downwards
    validateOpcodeConfig(sc0)
.ipint_tail_call_copy_stackargs_loop:
    btiz sc3, .ipint_tail_call_copy_stackargs_loop_end
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
    mulq SlotSize, sc3
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
    #  argument n                  <- sc1

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
    popFloat64(wfa0)
    uintDispatch()

uintAlign(_fr1)
    popFloat64(wfa1)
    uintDispatch()

uintAlign(_fr2)
    popFloat64(wfa2)
    uintDispatch()

uintAlign(_fr3)
    popFloat64(wfa3)
    uintDispatch()

uintAlign(_fr4)
    popFloat64(wfa4)
    uintDispatch()

uintAlign(_fr5)
    popFloat64(wfa5)
    uintDispatch()

uintAlign(_fr6)
    popFloat64(wfa6)
    uintDispatch()

uintAlign(_fr7)
    popFloat64(wfa7)
    uintDispatch()

# destination on stack is sc0

uintAlign(_stack)
    popInt64(sc1, sc2)
    storeq sc1, [sc0]
    subp 8, sc0
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
    stored wfa0, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa1)
    stored wfa1, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa2)
    stored wfa2, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa3)
    stored wfa3, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa4)
    stored wfa4, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa5)
    stored wfa5, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa6)
    stored wfa6, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa7)
    stored wfa7, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_stack)
    loadq [argumINTSrc], csr0
    addp 8, argumINTSrc
    storeq csr0, [argumINTDst]
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
