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

##########
# Macros #
##########

const StackValueShift = 4

# Callee Save

const IPIntCalleeSaveSpaceMCPC = -8
const IPIntCalleeSaveSpaceWI = -12

macro saveIPIntRegisters()
    # NOTE: We intentionally don't restore memoryBase and boundsCheckingSize here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    subp IPIntCalleeSaveSpaceStackAligned, sp
    store2ia MC, PC, IPIntCalleeSaveSpaceMCPC[cfr]
    storep wasmInstance, IPIntCalleeSaveSpaceWI[cfr]
end

macro restoreIPIntRegisters()
    load2ia IPIntCalleeSaveSpaceMCPC[cfr], MC, PC
    loadp IPIntCalleeSaveSpaceWI[cfr], wasmInstance
    addp IPIntCalleeSaveSpaceStackAligned, sp
end

# Dispatch target bases

const ipint_dispatch_base = _ipint_unreachable
const ipint_gc_dispatch_base = _ipint_struct_new
const ipint_conversion_dispatch_base = _ipint_i32_trunc_sat_f32_s
const ipint_simd_dispatch_base = _ipint_simd_v128_load_mem
const ipint_atomic_dispatch_base = _ipint_memory_atomic_notify

# Tail-call dispatch

macro nextIPIntInstruction()
    # Consistency check
    # move MC, t0
    # andp 7, t0
    # bpeq t0, 0, .fine
    # break
# .fine:
    loadb [PC], t0
if ARMv7
    lshiftp 8, t0
    leap (_ipint_unreachable + 1), t1
    addp t1, t0
    emit "bx r0"
else
    break
end
end

# Stack operations
# Every value on the stack is always 16 bytes! This makes life easy.

macro pushQuad(hi, lo)
    if ARMv7
        subp LocalSize, sp
        store2ia lo, hi, [sp]
    else
        break
    end
end

macro pushDouble(reg)
    if ARMv7
        subp LocalSize, sp
        storei reg, [sp]
    else
        break
    end
end

macro popQuad(hi, lo)
    if ARMv7
        load2ia [sp], lo, hi
        addp LocalSize, sp
    else
        break
    end
end

macro popDouble(reg)
    if ARMv7
        loadi [sp], reg
        addp LocalSize, sp
    else
        break
    end
end

macro pushFloat(reg)
    if ARMv7
        subp LocalSize, sp
        stored reg, [sp]
    else
        break
    end
end

macro popFloat(reg)
    if ARMv7
        loadd [sp], reg
        addp LocalSize, sp
    else
        break
    end
end

macro pushVectorReg0()
    break
end

macro pushVectorReg1()
    break
end

macro pushVectorReg2()
    break
end

macro popVectorReg0()
    break
end

macro popVectorReg1()
    break
end

macro popVectorReg2()
    break
end

macro peekDouble(i, reg)
    if ARMv7
        loadi (i * StackValueSize)[sp], reg
    else
        break
    end
end

macro peekQuad(i, hi, lo)
    if ARMv7
        load2ia (i * StackValueSize)[sp], lo, hi
    else
        break
    end
end

macro drop()
    addp StackValueSize, sp
end

# Typed push/pop/peek to make code pretty

macro pushInt32(reg)
    pushDouble(reg)
end

macro popInt32(reg)
    popDouble(reg)
end

macro peekInt32(i, reg)
    peekDouble(i, reg)
end

macro pushInt64(hi, lo)
    pushQuad(hi, lo)
end

macro popInt64(hi, lo)
    popQuad(hi, lo)
end

macro pushFloat32(reg)
    pushFloat(reg)
end

macro popFloat32(reg)
    popFloat(reg)
end

macro pushFloat64(reg)
    pushFloat(reg)
end

macro popFloat64(reg)
    popFloat(reg)
end

# Entering IPInt

# MC = location in argumINT bytecode

const argumINTTmp = t4
const argumINTDst = csr0
const argumINTSrc = csr1
const argumINTEnd = t7
const argumINTDsp = ws0 # t5

macro ipintEntry()
    checkStackOverflow(ws0, argumINTTmp)
    move ws0, lr

    # Allocate space for locals and rethrow values
    loadi Wasm::IPIntCallee::m_localSizeToAlloc[lr], argumINTTmp
    loadi Wasm::IPIntCallee::m_numRethrowSlotsToAlloc[lr], argumINTEnd
    mulp LocalSize, argumINTEnd
    mulp LocalSize, argumINTTmp
    subp argumINTEnd, sp
    move sp, argumINTEnd
    subp argumINTTmp, sp
    move sp, argumINTDsp
    loadp Wasm::IPIntCallee::m_argumINTBytecode + VectorBufferOffset[lr], MC

    push argumINTTmp, argumINTDst, argumINTSrc, argumINTEnd

    move argumINTDsp, argumINTDst
    leap FirstArgumentOffset[cfr], argumINTSrc

    argumINTDispatch()
end

macro argumINTDispatch()
    loadb [MC], argumINTTmp
    addp 1, MC
    bbgteq argumINTTmp, (constexpr IPInt::ArgumINTBytecode::NumOpcodes), .err
    lshiftp 6, argumINTTmp
    leap (_argumINT_begin + 1), argumINTDsp
    addp argumINTTmp, argumINTDsp
    jmp argumINTDsp
.err:
    break
end

macro argumINTInitializeDefaultLocals()
    # zero out remaining locals
    bpeq argumINTDst, argumINTEnd, .ipint_entry_finish_zero
    store2ia 0, 0, [argumINTDst]
    addp 8, argumINTDst
end

macro argumINTFinish()
    pop argumINTEnd, argumINTSrc, argumINTDst, argumINTTmp
end

# FFI Calls

macro functionCall(fn)
    # Save caller-save registers used by the interpreter
    push MC, PL
    fn()
    pop PL, MC
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
if ARM64 or ARM64E
    loadpairi IPInt::BlockMetadata::deltaPC[MC], t0, t1
else
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
end
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_loop, macro()
    # loop
    ipintLoopOSR(1)
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_if, macro()
    # if
    popInt32(t0)
    bineq 0, t0, .ipint_if_taken
    loadi IPInt::IfMetadata::elseDeltaPC[MC], t0
    loadi IPInt::IfMetadata::elseDeltaMC[MC], t1
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
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

unimplementedInstruction(_try)
unimplementedInstruction(_catch)

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

unimplementedInstruction(_rethrow)

ipintOp(_throw_ref, macro()
    popQuad(a3, a2)
    bieq a3, NullTag, .throw_null_ref

    saveCallSiteIndex()

    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t0
    loadp VM::topEntryFrame[t0], t0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t0)

    move cfr, a1
    operationCall(macro() cCall4(_ipint_extern_throw_ref) end)
    jumpToException()

.throw_null_ref:
    throwException(NullExnrefReference)
end)

# MC = location in uINT bytecode
# csr1, csr0 = tmp # clobbers PC, WI
# t7 = for dispatch
# sc1 = _uint_begin + 1

macro uintDispatch()
    loadb [MC], csr1
    addp 1, MC
    bilt csr1, (constexpr IPInt::UIntBytecode::NumOpcodes), .safe
    break
.safe:
    lshiftp 6, csr1
    addp csr1, sc1, t7
    # t7 = r9
    emit "bx r9"
end

macro uintEnter()
    leap (_uint_begin + 1), sc1
    uintDispatch()
end

ipintOp(_end, macro()
    loadi Wasm::IPIntCallee::m_bytecodeEnd[ws0], t0
    bpeq PC, t0, .ipint_end_ret
    advancePC(1)
    nextIPIntInstruction()
end)

# This implementation is specially defined out of ipintOp scope to make end implementation tight.
.ipint_end_ret:
    loadp Wasm::IPIntCallee::m_uINTBytecode + VectorBufferOffset[ws0], MC
    ipintEpilogueOSR(10)
    loadp Wasm::IPIntCallee::m_highestReturnStackOffset[ws0], sc0
    addp cfr, sc0
    uintEnter()

ipintOp(_br, macro()
    # br
    # number to pop
    loadh IPInt::BranchTargetMetadata::toPop[MC], t0
    # number to keep
    loadh IPInt::BranchTargetMetadata::toKeep[MC], t4

    # ex. pop 3 and keep 2
    #
    # +4 +3 +2 +1 sp
    # a  b  c  d  e
    # d  e
    #
    # [sp + k + numToPop] = [sp + k] for k in numToKeep-1 -> 0
    move t0, t2
    lshiftp StackValueShift, t2
    leap [sp, t2], t2

.ipint_br_poploop:
    bpeq t4, 0, .ipint_br_popend
    subp 1, t4
    move t4, t3
    lshiftp StackValueShift, t3
    load2ia [sp, t3], t0, t1
    store2ia t0, t1, [t2, t3]
    load2ia 8[sp, t3], t0, t1
    store2ia t0, t1, 8[t2, t3]
    jmp .ipint_br_poploop
.ipint_br_popend:
    loadh IPInt::BranchTargetMetadata::toPop[MC], t0
    lshiftp StackValueShift, t0
    leap [sp, t0], sp
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

ipintOp(_br_if, macro()
    # pop i32
    popInt32(t0)
    bineq t0, 0, _ipint_br
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_br_table, macro()
    # br_table
    popInt32(t0)
    loadi IPInt::SwitchMetadata::size[MC], t1
    advanceMC(constexpr (sizeof(IPInt::SwitchMetadata)))
    bib t0, t1, .ipint_br_table_clamped
    subp t1, 1, t0
.ipint_br_table_clamped:
    move t0, t1
    lshiftp 3, t0
    lshiftp 2, t1
    addp t1, t0
    addp t0, MC
    jmp _ipint_br
end)

ipintOp(_return, macro()
    # ret
    loadi Wasm::IPIntCallee::m_bytecodeEnd[ws0], PC
    loadp Wasm::IPIntCallee::m_uINTBytecode + VectorBufferOffset[ws0], MC
    # This is guaranteed going to an end instruction, so skip
    # dispatch and end of program check for speed
    jmp .ipint_end_ret
end)

const IPIntCallCallee = sc1
const IPIntCallFunctionSlot = sc0

ipintOp(_call, macro()
    loadp Wasm::IPIntCallee::m_bytecode[ws0], t0
    move PC, t1
    subp t0, t1
    storei t1, CallSiteIndex[cfr]

    loadb IPInt::CallMetadata::length[MC], t0
    advancePCByReg(t0)

    # get function index
    loadb IPInt::CallMetadata::functionIndex[MC], a1
    advanceMC(IPInt::CallMetadata::signature)

    subp 16, sp
    move sp, a2

    # operation returns the entrypoint in r0 and the target instance in r1
    # operation stores the target callee to sp[0] and target function info to sp[1]
    operationCall(macro() cCall3(_ipint_extern_prepare_call) end)
    loadp [sp], IPIntCallCallee
    loadp 8[sp], IPIntCallFunctionSlot
    addp 16, sp

    # call
    jmp .ipint_call_common
end)

ipintOp(_call_indirect, macro()
    loadp Wasm::IPIntCallee::m_bytecode[ws0], t0
    move PC, t1
    subp t0, t1
    storei t1, CallSiteIndex[cfr]

    loadb IPInt::CallIndirectMetadata::length[MC], t2
    advancePCByReg(t2)

    # Get function index by pointer, use it as a return for callee
    move sp, a2

    # Get callIndirectMetadata
    move cfr, a1
    move MC, a3
    advanceMC(IPInt::CallIndirectMetadata::signature)

    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_indirect) end)

    loadp [sp], IPIntCallCallee
    loadp 8[sp], IPIntCallFunctionSlot
    addp 16, sp

    jmp .ipint_call_common
end)

ipintOp(_return_call, macro()
    saveCallSiteIndex()

    loadb IPInt::TailCallMetadata::length[MC], t0
    advancePCByReg(t0)

    # get function index
    loadi IPInt::TailCallMetadata::functionIndex[MC], a1

    subp 16, sp
    move sp, a2

    # operation returns the entrypoint in r0 and the target instance in r1
    # this operation stores the boxed Callee into *r2
    operationCall(macro() cCall3(_ipint_extern_prepare_call) end)

    loadp [sp], IPIntCallCallee
    loadp 8[sp], IPIntCallFunctionSlot
    addp 16, sp

    loadi IPInt::TailCallMetadata::callerStackArgSize[MC], t3
    advanceMC(IPInt::TailCallMetadata::argumentBytecode)
    jmp .ipint_tail_call_common
end)

ipintOp(_return_call_indirect, macro()
    saveCallSiteIndex()

    loadb IPInt::TailCallIndirectMetadata::length[MC], t0
    advancePCByReg(t0)

    # Get function index by pointer, use it as a return for callee
    move sp, a2

    # Get callIndirectMetadata
    move cfr, a1
    move MC, a3
    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_indirect) end)

    loadp [sp], IPIntCallCallee
    loadp 8[sp], IPIntCallFunctionSlot
    addp 16, sp

    loadi IPInt::TailCallIndirectMetadata::callerStackArgSize[MC], t3
    advanceMC(IPInt::TailCallIndirectMetadata::argumentBytecode)
    jmp .ipint_tail_call_common
end)

ipintOp(_call_ref, macro()
    saveCallSiteIndex()

    move cfr, a1
    loadi IPInt::CallRefMetadata::typeIndex[MC], a2
    move sp, a3

    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_ref) end)
    loadp [sp], IPIntCallCallee
    loadp 8[sp], IPIntCallFunctionSlot
    addp 16, sp
    
    loadb IPInt::CallRefMetadata::length[MC], t3
    advanceMC(IPInt::CallRefMetadata::signature)
    advancePCByReg(t3)

    jmp .ipint_call_common
end)

ipintOp(_return_call_ref, macro()
    saveCallSiteIndex()

    loadb IPInt::TailCallRefMetadata::length[MC], t2
    advancePCByReg(t2)

    move cfr, a1
    loadi IPInt::TailCallRefMetadata::typeIndex[MC], a2
    move sp, a3
    operationCallMayThrow(macro() cCall4(_ipint_extern_prepare_call_ref) end)
    loadp [sp], IPIntCallCallee
    loadp 8[sp], IPIntCallFunctionSlot
    addp 16, sp

    loadi IPInt::TailCallRefMetadata::callerStackArgSize[MC], t3
    advanceMC(IPInt::TailCallRefMetadata::argumentBytecode)
    jmp .ipint_tail_call_common
end)

reservedOpcode(0x16)
reservedOpcode(0x17)
unimplementedInstruction(_delegate)
unimplementedInstruction(_catch_all)

ipintOp(_drop, macro()
    drop()
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_select, macro()
    popInt32(t0)
    bieq t0, 0, .ipint_select_val2
    drop()
    advancePC(1)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_select_val2:
    popQuad(t3, t2)
    popQuad(t1, t0)
    pushQuad(t3, t2)
    advancePC(1)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_select_t, macro()
    popInt32(t0)
    bieq t0, 0, .ipint_select_t_val2
    drop()
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_select_t_val2:
    popQuad(t3, t2)
    popQuad(t1, t0)
    pushQuad(t3, t2)
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

reservedOpcode(0x1d)
reservedOpcode(0x1e)

ipintOp(_try_table, macro()
    # advance MC/PC
    loadi IPInt::BlockMetadata::deltaPC[MC], t0
    loadi IPInt::BlockMetadata::deltaMC[MC], t1
    advancePCByReg(t0)
    advanceMCByReg(t1)
    nextIPIntInstruction()
end)

    ###################################
    # 0x20 - 0x26: get and set values #
    ###################################

ipintOp(_local_get, macro()
    # local.get
    loadb 1[PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_get_slow_path
.ipint_local_get_post_decode:
    # Index into locals
    mulp LocalSize, t0
    load2ia [PL, t0], t0, t1
    # Push to stack
    pushQuad(t1, t0)
    nextIPIntInstruction()
end)

ipintOp(_local_set, macro()
    # local.set
    loadb 1[PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_set_slow_path
.ipint_local_set_post_decode:
    # Pop from stack
    popQuad(t3, t2)
    # Store to locals
    mulp LocalSize, t0
    store2ia t2, t3, [PL, t0]
    nextIPIntInstruction()
end)

ipintOp(_local_tee, macro()
    # local.tee
    loadb 1[PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_tee_slow_path
.ipint_local_tee_post_decode:
    # Load from stack
    load2ia [sp], t2, t3
    # Store to locals
    mulp LocalSize, t0
    store2ia t2, t3, [PL, t0]
    nextIPIntInstruction()
end)

ipintOp(_global_get, macro()
    # Load pre-computed index from metadata
    loadb IPInt::GlobalMetadata::bindingMode[MC], t2
    loadi IPInt::GlobalMetadata::index[MC], t1
    loadp CodeBlock[cfr], t0
    loadp JSWebAssemblyInstance::m_globals[t0], t0
    lshiftp 1, t1
    load2ia [t0, t1, 8], t0, t1
    bieq t2, 0, .ipint_global_get_embedded
    load2ia [t0], t0, t1
.ipint_global_get_embedded:
    pushQuad(t1, t0)

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
    loadb IPInt::GlobalMetadata::bindingMode[MC], t4
    # get global addr
    loadp CodeBlock[cfr], t0
    loadp JSWebAssemblyInstance::m_globals[t0], t0
    # get value to store
    popQuad(t3, t2)
    # get index
    loadi IPInt::GlobalMetadata::index[MC], t1
    lshiftp 1, t1
    bieq t4, 0, .ipint_global_set_embedded
    # portable: dereference then set
    loadp [t0, t1, 8], t0
    store2ia t2, t3, [t0]
    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    nextIPIntInstruction()
.ipint_global_set_embedded:
    # embedded: set directly
    store2ia t2, t3, [t0, t1, 8]
    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    nextIPIntInstruction()

.ipint_global_set_refpath:
    loadi IPInt::GlobalMetadata::index[MC], a1
    # Pop from stack
    popQuad(a3, a2)
    operationCall(macro() cCall4(_ipint_extern_set_global_ref) end)

    loadb IPInt::GlobalMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::GlobalMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_get, macro()
    # Load pre-computed index from metadata
    loadi IPInt::Const32Metadata::value[MC], a1
    popInt32(a2)
    operationCallMayThrow(macro() cCall3(_ipint_extern_table_get) end)
    pushQuad(r1, r0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_set, macro()
    # Load pre-computed index from metadata
    loadi IPInt::Const32Metadata::value[MC], a1 # table index
    popQuad(t4, t3) # value
    popInt32(a2) # index
    operationCallMayThrow(macro()
        push t4, t3
        cCall3(_ipint_extern_table_set)
        addp 2*MachineRegisterSize, sp
    end)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

reservedOpcode(0x27)

macro ipintWithMemory()
    loadp CodeBlock[cfr], t3
    load2ia JSWebAssemblyInstance::m_cachedMemory[t3], memoryBase, boundsCheckingSize
end

# NB: mutates mem to be mem + offset, clobbers offset
macro ipintMaterializePtrAndCheckMemoryBound(mem, offset, size)
    addps offset, mem
    bcs .outOfBounds
    addps size - 1, mem, offset
    bcs .outOfBounds
    bpb offset, boundsCheckingSize, .continuation
.outOfBounds:
    ipintException(OutOfBoundsMemoryAccess)
.continuation:
end

ipintOp(_i32_load_mem, macro()
    # i32.load
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
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
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 8)
    # load memory location
    load2ia [memoryBase, t0], t0, t1
    pushInt64(t1, t0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f32_load_mem, macro()
    # f32.load
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
    # load memory location
    loadi [memoryBase, t0], t0 # NB: can be unaligned, hence loadi, fi2f instead of loadf (VLDR)
    fi2f t0, ft0
    pushFloat32(ft0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f64_load_mem, macro()
    # f64.load
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 8)
    # load memory location
    load2ia [memoryBase, t0], t0, t1 # NB: can be unaligned, hence loadi, fii2d instead of loadd
    fii2d t0, t1, ft0
    pushFloat64(ft0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_load8s_mem, macro()
    # i32.load8_s
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
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
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
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
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 2)
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
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 2)
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
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadb [memoryBase, t0], t0
    sxb2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load8u_mem, macro()
    # i64.load8_u
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadb [memoryBase, t0], t0
    move 0, t1
    pushInt64(t1, t0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load16s_mem, macro()
    # i64.load16_s
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 2)
    # load memory location
    loadh [memoryBase, t0], t0
    sxh2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load16u_mem, macro()
    # i64.load16_u
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 2)
    # load memory location
    loadh [memoryBase, t0], t0
    move 0, t1
    pushInt64(t1, t0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load32s_mem, macro()
    # i64.load32_s
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
    # load memory location
    loadi [memoryBase, t0], t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_load32u_mem, macro()
    # i64.load8_s
    # pop index
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
    # load memory location
    loadi [memoryBase, t0], t0
    move 0, t1
    pushInt64(t1, t0)

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_store_mem, macro()
    # i32.store
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 4)
    # store at memory location
    storei t0, [memoryBase, t4]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store_mem, macro()
    # i64.store
    # peek index
    peekInt32(1, t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 8)
    # pop data
    popInt64(t1, t0)
    # drop index
    drop()
    # store at memory location
    store2ia t0, t1, [memoryBase, t4]

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
    popInt32(t0)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
    # store at memory location
    ff2i ft0, t1 # NB: can be unaligned, hence ff2i, storei instead of storef (VSTR)
    storei t1, [memoryBase, t0]

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
    popInt32(t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 8)
    # store at memory location
    fd2ii ft0, t0, t1 # NB: can be unaligned, hence fd2ii, store2ia instead of stored
    store2ia t0, t1, [memoryBase, t4]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_store8_mem, macro()
    # i32.store8
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 1)
    # store at memory location
    storeb t0, [memoryBase, t4]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_store16_mem, macro()
    # i32.store16
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 2)
    # store at memory location
    storeh t0, [memoryBase, t4]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store8_mem, macro()
    # i64.store8
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 1)
    # store at memory location
    storeb t0, [memoryBase, t4]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store16_mem, macro()
    # i64.store16
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 2)
    # store at memory location
    storeh t0, [memoryBase, t4]

    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_store32_mem, macro()
    # i64.store32
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t4)
    loadi IPInt::Const32Metadata::value[MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t4, t1, 4)
    # store at memory location
    storei t0, [memoryBase, t4]

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
    lshifti 7, t1
    ori t1, t0
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
    load2ia IPInt::Const64Metadata::value[MC], t0, t1
    # Push to stack
    pushInt64(t1, t0)
    loadb IPInt::Const64Metadata::instructionLength[MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::Const64Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_f32_const, macro()
    # f32.const
    # Load pre-computed value from metadata
    loadi 1[PC], t0 # NB: can be unaligned, hence loadi, fi2f instead of loadf (VLDR)
    fi2f t0, ft0
    pushFloat32(ft0)

    advancePC(5)
    nextIPIntInstruction()
end)

ipintOp(_f64_const, macro()
    # f64.const
    # Load pre-computed value from metadata
    load2ia 1[PC], t0, t1 # NB: can be unaligned, hence loadi, fii2d instead of loadd
    fii2d t0, t1, ft0
    pushFloat64(ft0)

    advancePC(9)
    nextIPIntInstruction()

    ###############################
    # 0x45 - 0x4f: i32 comparison #
    ###############################
end)

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
    popInt64(t1, t0)
    move 0, t2
    btinz t1, .ipint_i64_eqz_return
    cieq t0, 0, t2
.ipint_i64_eqz_return:
    pushInt32(t2)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_eq, macro()
    # i64.eq
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 0, t4
    bineq t1, t3, .ipint_i64_eq_return
    cieq t0, t2, t4
.ipint_i64_eq_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ne, macro()
    # i64.ne
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bineq t1, t3, .ipint_i64_ne_return
    cineq t0, t2, t4
.ipint_i64_ne_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_lt_s, macro()
    # i64.lt_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bilt t1, t3, .ipint_i64_lt_s_return
    move 0, t4
    bigt t1, t3, .ipint_i64_lt_s_return
    cib t0, t2, t4
.ipint_i64_lt_s_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_lt_u, macro()
    # i64.lt_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bib t1, t3, .ipint_i64_lt_u_return
    move 0, t4
    bia t1, t3, .ipint_i64_lt_u_return
    cib t0, t2, t4
.ipint_i64_lt_u_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_gt_s, macro()
    # i64.gt_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bigt t1, t3, .ipint_i64_gt_s_return
    move 0, t4
    bilt t1, t3, .ipint_i64_gt_s_return
    cia t0, t2, t4
.ipint_i64_gt_s_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_gt_u, macro()
    # i64.gt_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bia t1, t3, .ipint_i64_gt_u_return
    move 0, t4
    bib t1, t3, .ipint_i64_gt_u_return
    cia t0, t2, t4
.ipint_i64_gt_u_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_le_s, macro()
    # i64.le_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bilt t1, t3, .ipint_i64_le_s_return
    move 0, t4
    bigt t1, t3, .ipint_i64_le_s_return
    cibeq t0, t2, t4
.ipint_i64_le_s_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_le_u, macro()
    # i64.le_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bib t1, t3, .ipint_i64_le_u_return
    move 0, t4
    bia t1, t3, .ipint_i64_le_u_return
    cibeq t0, t2, t4
.ipint_i64_le_u_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ge_s, macro()
    # i64.ge_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t4
    bigt t1, t3, .ipint_i64_ge_s_return
    move 0, t4
    bilt t1, t3, .ipint_i64_ge_s_return
    ciaeq t0, t2, t4
.ipint_i64_ge_s_return:
    pushInt32(t4)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ge_u, macro()
    # i64.ge_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 0, t4
    bib t1, t3, .ipint_i64_ge_u_return
    move 1, t4
    bia t1, t3, .ipint_i64_ge_u_return
    ciaeq t0, t2, t4
.ipint_i64_ge_u_return:
    pushInt32(t4)
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
    functionCall(macro () cCall2(_i32_div_s) end)
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

    functionCall(macro () cCall2(_i32_div_u) end)
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

    move 0, t0
    jmp .ipint_i32_rem_s_return

.ipint_i32_rem_s_safe:
    functionCall(macro () cCall2(_i32_rem_s) end)

.ipint_i32_rem_s_return:
    pushInt32(t0)
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

    functionCall(macro () cCall2(_i32_rem_u) end)
    pushInt32(t0)
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
    popInt64(t1, t0)
    btiz t1, .ipint_i64_clz_bottom

    lzcnti t1, t0
    jmp .ipint_i64_clz_return

.ipint_i64_clz_bottom:
    lzcnti t0, t0
    addi 32, t0

.ipint_i64_clz_return:
    move 0, t1
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_ctz, macro()
    # i64.ctz
    popInt64(t1, t0)
    btiz t0, .ipint_i64_ctz_top

    tzcnti t0, t0
    jmp .ipint_i64_ctz_return

.ipint_i64_ctz_top:
    tzcnti t1, t0
    addi 32, t0

.ipint_i64_ctz_return:
    move 0, t1
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_popcnt, macro()
    # i64.popcnt
    popInt64(t3, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcountll) end)
    move 0, t0
    pushInt64(t0, r1)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_add, macro()
    # i64.add
    popInt64(t3, t2)
    popInt64(t1, t0)
    addis t2, t0
    adci  t3, t1
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_sub, macro()
    # i64.sub
    popInt64(t3, t2)
    popInt64(t1, t0)
    subis t2, t0
    sbci  t3, t1
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_mul, macro()
    # i64.mul
    popInt64(t3, t2)
    popInt64(t1, t0)
    muli t2, t1
    muli t0, t3
    umulli t0, t2, t0, t2
    addi t1, t2
    addi t3, t2
    pushInt64(t2, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_div_s, macro()
    # i64.div_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    btinz t3, .ipint_i64_div_s_nonZero
    btiz t2, .ipint_i64_div_s_throwDivisionByZero

.ipint_i64_div_s_nonZero:
    bineq t3, -1, .ipint_i64_div_s_safe
    bineq t2, -1, .ipint_i64_div_s_safe
    bineq t1, constexpr INT32_MIN, .ipint_i64_div_s_safe
    btiz t0, .ipint_i64_div_s_throwIntegerOverflow

.ipint_i64_div_s_safe:
    functionCall(macro () cCall4(_i64_div_s) end)
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_div_s_throwDivisionByZero:
    ipintException(DivisionByZero)

.ipint_i64_div_s_throwIntegerOverflow:
    ipintException(IntegerOverflow)
end)

ipintOp(_i64_div_u, macro()
    # i64.div_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    btinz t3, .ipint_i64_div_u_nonZero
    btiz t2, .ipint_i64_div_u_throwDivisionByZero

.ipint_i64_div_u_nonZero:
    functionCall(macro () cCall4(_i64_div_u) end)
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_div_u_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i64_rem_s, macro()
    # i64.rem_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    btinz t3, .ipint_i64_rem_s_nonZero
    btiz t2, .ipint_i64_rem_s_throwDivisionByZero

.ipint_i64_rem_s_nonZero:
    bineq t3, -1, .ipint_i64_rem_s_safe
    bineq t2, -1, .ipint_i64_rem_s_safe
    bineq t1, constexpr INT32_MIN, .ipint_i64_rem_s_safe

    move 0, t1
    move 0, t0
    jmp .ipint_i64_rem_s_return

.ipint_i64_rem_s_safe:
    functionCall(macro () cCall4(_i64_rem_s) end)

.ipint_i64_rem_s_return:
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_rem_s_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i64_rem_u, macro()
    # i64.rem_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    btinz t3, .ipint_i64_rem_u_nonZero
    btiz t2, .ipint_i64_rem_u_throwDivisionByZero

.ipint_i64_rem_u_nonZero:
    functionCall(macro () cCall4(_i64_rem_u) end)
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_rem_u_throwDivisionByZero:
    ipintException(DivisionByZero)
end)

ipintOp(_i64_and, macro()
    # i64.and
    popInt64(t3, t2)
    popInt64(t1, t0)
    andi t3, t1
    andi t2, t0
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_or, macro()
    # i64.or
    popInt64(t3, t2)
    popInt64(t1, t0)
    ori t3, t1
    ori t2, t0
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_xor, macro()
    # i64.xor
    popInt64(t3, t2)
    popInt64(t1, t0)
    xori t3, t1
    xori t2, t0
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shl, macro()
    # i64.shl
    popInt32(t2)
    popInt64(t1, t0)
    andi 0x3f, t2
    btiz t2, .ipint_i64_shl_return
    bib t2, 32, .ipint_i64_lessThan32

    subi 32, t2
    lshifti t0, t2, t1
    move 0, t0
    jmp .ipint_i64_shl_return

.ipint_i64_lessThan32:
    lshifti t2, t1
    move 32, t3
    subi t2, t3
    urshifti t0, t3, t3
    ori t3, t1
    lshifti t2, t0

.ipint_i64_shl_return:
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shr_s, macro()
    # i64.shr_s
    popInt32(t2)
    popInt64(t1, t0)
    andi 0x3f, t2
    btiz t2, .ipint_i64_shr_s_return
    bib t2, 32, .ipint_i64_shr_s_lessThan32

    subi 32, t2
    rshifti t1, t2, t0
    rshifti 31, t1
    jmp .ipint_i64_shr_s_return

.ipint_i64_shr_s_lessThan32:
    urshifti t2, t0
    move 32, t3
    subi t2, t3
    lshifti t1, t3, t3
    ori t3, t0
    rshifti t2, t1

.ipint_i64_shr_s_return:
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_shr_u, macro()
    # i64.shr_u
    popInt32(t2)
    popInt64(t1, t0)
    andi 0x3f, t2
    btiz t2, .ipint_i64_shr_u_return
    bib t2, 32, .ipint_i64_shr_u_lessThan32

    subi 32, t2
    urshifti t1, t2, t0
    move 0, t1
    jmp .ipint_i64_shr_u_return

.ipint_i64_shr_u_lessThan32:
    urshifti t2, t0
    move 32, t3
    subi t2, t3
    lshifti t1, t3, t3
    ori t3, t0
    urshifti t2, t1

.ipint_i64_shr_u_return:
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_rotl, macro()
    # i64.rotl
    popInt32(t2)
    popInt64(t1, t0)
    andi t2, 0x20, t3
    btiz t3, .ipint_i64_rotl_noSwap

    move t0, t3
    move t1, t0
    move t3, t1

.ipint_i64_rotl_noSwap:
    andi 0x1f, t2
    btiz t2, .ipint_i64_rotl_return

    move 32, t4
    subi t2, t4
    urshifti t0, t4, t3
    urshifti t1, t4, t4
    lshifti t2, t0
    lshifti t2, t1
    ori t4, t0
    ori t3, t1

.ipint_i64_rotl_return:
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_rotr, macro()
    # i64.rotr
    popInt32(t2)
    popInt64(t1, t0)
    andi t2, 0x20, t3
    btiz t3, .ipint_i64_rotr_noSwap

    move t0, t3
    move t1, t0
    move t3, t1

.ipint_i64_rotr_noSwap:
    andi 0x1f, t2
    btiz t2, .ipint_i64_rotr_return

    move 32, t4
    subi t2, t4
    lshifti t0, t4, t3
    lshifti t1, t4, t4
    urshifti t2, t0
    urshifti t2, t1
    ori t4, t0
    ori t3, t1

.ipint_i64_rotr_return:
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x8b - 0x98: f32 operations #
    ###############################


ipintOp(_f32_abs, macro()
    # f32.abs
    popFloat32(ft0)
    absf ft0, ft0
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_neg, macro()
    # f32.neg
    popFloat32(ft0)
    negf ft0, ft0
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_ceil, macro()
    # f32.ceil
    popFloat32(ft0)
    functionCall(macro () cCall2(_ceilFloat) end)
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_floor, macro()
    # f32.floor
    popFloat32(ft0)
    functionCall(macro () cCall2(_floorFloat) end)
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_trunc, macro()
    # f32.trunc
    popFloat32(ft0)
    functionCall(macro () cCall2(_truncFloat) end)
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_nearest, macro()
    # f32.nearest
    popFloat32(ft0)
    functionCall(macro () cCall2(_f32_nearest) end)
    pushFloat32(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_sqrt, macro()
    # f32.sqrt
    popFloat32(ft0)
    sqrtf ft0, ft0
    pushFloat32(ft0)

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
    popDouble(t1)
    popDouble(t0)

    andi 0x80000000, t1
    andi 0x7fffffff, t0
    ori t1, t0

    pushDouble(t0)

    advancePC(1)
    nextIPIntInstruction()
end)

    ###############################
    # 0x99 - 0xa6: f64 operations #
    ###############################

ipintOp(_f64_abs, macro()
    # f64.abs
    popFloat64(ft0)
    absd ft0, ft0
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_neg, macro()
    # f64.neg
    popFloat64(ft0)
    negd ft0, ft0
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_ceil, macro()
    # f64.ceil
    popFloat64(ft0)
    functionCall(macro () cCall2(_ceilDouble) end)
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_floor, macro()
    # f64.floor
    popFloat64(ft0)
    functionCall(macro () cCall2(_floorDouble) end)
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_trunc, macro()
    # f64.trunc
    popFloat64(ft0)
    functionCall(macro () cCall2(_truncDouble) end)
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_nearest, macro()
    # f64.nearest
    popFloat64(ft0)
    functionCall(macro () cCall2(_f64_nearest) end)
    pushFloat64(ft0)

    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_sqrt, macro()
    # f64.sqrt
    popFloat64(ft0)
    sqrtd ft0, ft0
    pushFloat64(ft0)

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
    jmp .ipint_f64_min_return

.ipint_f64_min_equal:
    ord ft0, ft1
    jmp .ipint_f64_min_return

.ipint_f64_min_lt:
    moved ft0, ft1
    # continue with .ipint_f64_min_return

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
    jmp .ipint_f64_max_return

.ipint_f64_max_equal:
    andd ft0, ft1
    jmp .ipint_f64_max_return

.ipint_f64_max_lt:
    moved ft0, ft1
    # continue with .ipint_f64_max_return

.ipint_f64_max_return:
    pushFloat64(ft1)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_copysign, macro()
    # f64.copysign
    popQuad(t3, t2)
    popQuad(t1, t0)

    andi 0x7fffffff, t1
    andi 0x80000000, t3
    ori t3, t1

    pushQuad(t1, t0)

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
    # INT32_MIN - 1.0
    move 0xc1e00000, t1
    move 0x00200000, t0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .ipint_trunc_i32_f64_s_outOfBoundsTrunc

    # -INT32_MIN
    move 0x41e00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
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
    # -1.0
    move 0xbff00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .ipint_trunc_i32_f64_u_outOfBoundsTrunc

    # INT32_MIN * -2.0
    move 0x41f00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
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
    rshifti t0, 31, t1
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend_i32_u, macro()
    popInt32(t0)
    move 0, t1
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_trunc_f32_s, macro()
    popFloat64(ft0)
    
    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_trunc_i64_f32_s_outOfBoundsTrunc
    
    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_trunc_i64_f32_s_outOfBoundsTrunc

    functionCall(macro () cCall2(_i64_trunc_s_f32) end)
    pushInt64(r1, r0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_trunc_i64_f32_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i64_trunc_f32_u, macro()
    popFloat64(ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i64_f32_u_outOfBoundsTrunc

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_f32_u_outOfBoundsTrunc

    functionCall(macro () cCall2(_i64_trunc_u_f32) end)
    pushInt64(r1, r0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f32_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i64_trunc_f64_s, macro()
    popFloat64(ft0)

    # INT64_MIN
    move 0xc3e00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
    bdltun ft0, ft1, .ipint_i64_f64_s_outOfBoundsTrunc

    # -INT64_MIN
    move 0x43e00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .ipint_i64_f64_s_outOfBoundsTrunc

    functionCall(macro () cCall2(_i64_trunc_s_f64) end)
    pushInt64(r1, r0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f64_s_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_i64_trunc_f64_u, macro()
    popFloat64(ft0)

    # -1.0
    move 0xbff00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .ipint_i64_f64_u_outOfBoundsTrunc

    # INT64_MIN * -2.0
    move 0x43f00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .ipint_i64_f64_u_outOfBoundsTrunc

    functionCall(macro () cCall2(_i64_trunc_u_f64) end)
    pushInt64(r1, r0)
    advancePC(1)
    nextIPIntInstruction()

.ipint_i64_f64_u_outOfBoundsTrunc:
    ipintException(OutOfBoundsTrunc)
end)

ipintOp(_f32_convert_i32_s, macro()
    popInt32(t0)
    ci2fs t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i32_u, macro()
    popInt32(t0)
    ci2f t0, ft0
    pushFloat32(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i64_s, macro()
    popInt64(t1, t0)
    functionCall(macro () cCall2(_f32_convert_s_i64) end)
    pushFloat32(fr)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f32_convert_i64_u, macro()
    popInt64(t1, t0)
    functionCall(macro () cCall2(_f32_convert_u_i64) end)
    pushFloat32(fr)
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
    ci2ds t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i32_u, macro()
    popInt32(t0)
    ci2d t0, ft0
    pushFloat64(ft0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i64_s, macro()
    popInt64(t1, t0)
    functionCall(macro () cCall2(_f64_convert_s_i64) end)
    pushFloat64(fr)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_f64_convert_i64_u, macro()
    popInt64(t1, t0)
    functionCall(macro () cCall2(_f64_convert_u_i64) end)
    pushFloat64(fr)
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
    fd2ii ft0, t0, t1
    pushInt64(t1, t0)
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
    popInt32(t0)
    sxb2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend16_s, macro()
    # i64.extend8_s
    popInt32(t0)
    sxh2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_i64_extend32_s, macro()
    # i64.extend8_s
    popInt32(t0)
    rshifti t0, 31, t1
    pushInt64(t1, t0)
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
    move NullTag, t1
    pushQuad(t1, t0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePC(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_is_null, macro()
    popQuad(t1, t0)
    cieq t1, NullTag, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_ref_func, macro()
    loadi IPInt::Const32Metadata::value[MC], a1
    operationCall(macro() cCall2(_ipint_extern_ref_func) end)
    pushQuad(r1, r0)
    loadb IPInt::Const32Metadata::instructionLength[MC], t0
    advancePC(t0)
    advanceMC(constexpr (sizeof(IPInt::Const32Metadata)))
    nextIPIntInstruction()
end)

ipintOp(_ref_eq, macro()
    popQuad(t1, t0)
    popQuad(t3, t2)
    cieq t0, t2, t0
    cieq t1, t3, t1
    andi t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()
end)

ipintOp(_ref_as_non_null, macro()
    peekQuad(0, t1, t0)
    bpeq t1, NullTag, .ref_as_non_null_nullRef
    advancePC(1)
    nextIPIntInstruction()
.ref_as_non_null_nullRef:
    throwException(NullRefAsNonNull)
end)

ipintOp(_br_on_null, macro()
    peekQuad(0, t1, t0)
    bineq t1, NullTag, .br_on_null_not_null

    # pop the null
    drop()
    jmp _ipint_br
.br_on_null_not_null:
    loadb IPInt::BranchMetadata::instructionLength[MC], t0
    advanceMC(constexpr (sizeof(IPInt::BranchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()
end)

ipintOp(_br_on_non_null, macro()
    peekQuad(0, t1, t0)
    bineq t1, NullTag, _ipint_br
    drop()
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
unimplementedInstruction(_gc_prefix)

ipintOp(_conversion_prefix, macro()
    decodeLEBVarUInt32(1, t0, t1, t2, t3, t4)
    # Security guarantee: always less than 18 (0x00 -> 0x11)
    biaeq t0, 0x12, .ipint_conversion_nonexistent
    lshiftp 8, t0
    leap (ipint_conversion_dispatch_base + 1), t1
    addp t1, t0
    emit "bx r0"

.ipint_conversion_nonexistent:
    break
end)

unimplementedInstruction(_simd_prefix)
unimplementedInstruction(_atomic_prefix)
reservedOpcode(0xff)

    #######################
    ## 0xFB instructions ##
    #######################

unimplementedInstruction(_struct_new)
unimplementedInstruction(_struct_new_default)
unimplementedInstruction(_struct_get)
unimplementedInstruction(_struct_get_s)
unimplementedInstruction(_struct_get_u)
unimplementedInstruction(_struct_set)
unimplementedInstruction(_array_new)
unimplementedInstruction(_array_new_default)
unimplementedInstruction(_array_new_fixed)
unimplementedInstruction(_array_new_data)
unimplementedInstruction(_array_new_elem)
unimplementedInstruction(_array_get)
unimplementedInstruction(_array_get_s)
unimplementedInstruction(_array_get_u)
unimplementedInstruction(_array_set)
unimplementedInstruction(_array_len)
unimplementedInstruction(_array_fill)
unimplementedInstruction(_array_copy)
unimplementedInstruction(_array_init_data)
unimplementedInstruction(_array_init_elem)
unimplementedInstruction(_ref_test)
unimplementedInstruction(_ref_test_nullable)
unimplementedInstruction(_ref_cast)
unimplementedInstruction(_ref_cast_nullable)
unimplementedInstruction(_br_on_cast)
unimplementedInstruction(_br_on_cast_fail)
unimplementedInstruction(_any_convert_extern)
unimplementedInstruction(_extern_convert_any)
unimplementedInstruction(_ref_i31)
unimplementedInstruction(_i31_get_s)
unimplementedInstruction(_i31_get_u)

    #######################
    ## 0xFC instructions ##
    #######################

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
    jmp .ipint_i32_trunc_sat_f32_s_end

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f32_s_end

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f32_s_end

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)
    # jmp .ipint_i32_trunc_sat_f32_s_end

.ipint_i32_trunc_sat_f32_s_end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
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
    jmp .ipint_i32_trunc_sat_f32_u_end

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f32_u_end

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)
    # jmp .ipint_i32_trunc_sat_f32_u_end
    
.ipint_i32_trunc_sat_f32_u_end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_trunc_sat_f64_s, macro()
    popFloat64(ft0)

    # INT32_MIN - 1.0
    move 0xc1e00000, t1
    move 0x00200000, t0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN

    # -INT32_MIN
    move 0x41e00000, t1
    move 0x00000000, t0 
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax

    truncated2is ft0, t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f64_s_end

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f64_s_end

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f64_s_end

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)
    # jmp .ipint_i32_trunc_sat_f64_s_end

.ipint_i32_trunc_sat_f64_s_end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_i32_trunc_sat_f64_u, macro()
    popFloat64(ft0)

    # -1.0
    move 0xbff00000, t1
    move 0x00000000, t0 
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin

    # INT32_MIN * -2.0
    move 0x41f00000, t1
    move 0x00000000, t0 
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax

    truncated2i ft0, t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f64_u_end

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)
    jmp .ipint_i32_trunc_sat_f64_u_end

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)
    # jmp .ipint_i32_trunc_sat_f64_u_end

.ipint_i32_trunc_sat_f64_u_end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_trunc_sat_f32_s, macro()
    popFloat32(ft0)

    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax

    functionCall(macro () cCall2(_i64_trunc_s_f32) end)

    pushInt64(r1, r0)
    jmp .ipint_i64_trunc_sat_f32_s_end

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMin
    pushInt64(0, 0)
    jmp .ipint_i64_trunc_sat_f32_s_end

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    # INT64_MAX
    move 0x7fffffff, t1
    move 0xffffffff, t0
    pushInt64(t1, t0)
    jmp .ipint_i64_trunc_sat_f32_s_end

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMin:
    # INT64_MIN
    move 0x80000000, t1
    move 0x00000000, t0
    pushInt64(t1, t0)
    # jmp .ipint_i64_trunc_sat_f32_s_end

.ipint_i64_trunc_sat_f32_s_end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_trunc_sat_f32_u, macro()
    popFloat32(ft0)

    functionCall(macro () cCall2(_i64_trunc_u_f32) end)

    pushInt64(r1, r0)

    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_trunc_sat_f64_s, macro()
    popFloat64(ft0)

    # INT64_MIN
    move 0xc3e00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
    bdltun ft0, ft1, .ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN

    # -INT64_MIN
    move 0x43e00000, t1
    move 0x00000000, t0
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMax

    functionCall(macro () cCall2(_i64_trunc_s_f64) end)

    pushInt64(r1, r0)
    jmp .ipint_i64_trunc_sat_f64_s_end

.ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMin
    pushInt64(0, 0)
    jmp .ipint_i64_trunc_sat_f64_s_end

.ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMax:
    # INT64_MAX
    move 0x7fffffff, t1
    move 0xffffffff, t0
    pushInt64(t1, t0)
    jmp .ipint_i64_trunc_sat_f64_s_end

.ipint_i64_trunc_sat_f64_s_outOfBoundsTruncSatMin:
    # INT64_MIN
    move 0x80000000, t1
    move 0x00000000, t0
    pushInt64(t1, t0)
    # jmp .ipint_i64_trunc_sat_f64_s_end

.ipint_i64_trunc_sat_f64_s_end:
    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_i64_trunc_sat_f64_u, macro()
    popFloat64(ft0)

    functionCall(macro () cCall2(_i64_trunc_u_f64) end)

    pushInt64(r1, r0)

    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_memory_init, macro()
    # memory.init
    move sp, a2
    loadi 1[MC], a1
    operationCallMayThrow(macro() cCall3(_ipint_extern_memory_init) end)
    addp 3 * StackValueSize, sp
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
    popInt32(a3) # n
    popInt32(a2) # s
    popInt32(a1) # d
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_copy) end)

    loadb IPInt::InstructionLengthMetadata::length[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_memory_fill, macro()
    # memory.fill
    popInt32(a3) # n
    popInt32(a2) # val
    popInt32(a1) # d
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
    pushInt32(r0)
    loadb IPInt::TableGrowMetadata::instructionLength[MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::TableGrowMetadata)))
    nextIPIntInstruction()
end)

ipintOp(_table_size, macro()
    # table.size
    loadi IPInt::Const32Metadata::value[MC], a1
    operationCall(macro() cCall2(_ipint_extern_table_size) end)
    pushInt32(r0)
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
unimplementedInstruction(_simd_v128_const)

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

unimplementedInstruction(_simd_i32x4_extract_lane)
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

#######################################
## ULEB128 decoding logic for locals ##
#######################################

macro decodeULEB128(result)
    # result should already be the first byte.
    andq 0x7f, result
    move 7, t2 # t1 holds the shift.
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
    break

slowPathLabel(_local_set)
    break

slowPathLabel(_local_tee)
    break

##################################
## "Out of line" logic for call ##
##################################

# time to use the safe for call registers!
# sc0 = mINT shadow stack pointer (tracks the Wasm stack)

const mintSS = sc1
const mintDst = sc2

macro mintPop(hi, lo)
    load2ia [mintSS], lo, hi
    addp LocalSize, mintSS
end

macro mintPopF(reg)
    loadd [mintSS], reg
    addp LocalSize, mintSS
end

macro mintArgDispatch()
    loadb [MC], sc0
    addp 1, MC
    bilt sc0, (constexpr IPInt::CallArgumentBytecode::NumOpcodes), .safe
    break
.safe:
    lshiftp 6, sc0
    leap (_mint_begin + 1), t7
    addp sc0, t7
    # t7 = r9
    emit "bx r9"
end

macro mintRetDispatch()
    loadb [MC], sc0
    addp 1, MC
    bilt sc0, (constexpr IPInt::CallResultBytecode::NumOpcodes), .safe
    break
.safe:
    lshiftp 6, sc0
    leap (_mint_begin_return + 1), t7
    addp sc0, t7
    # t7 = r9
    emit "bx r9"
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

    const targetEntrypoint = r0
    const targetInstance = r1

    const argSP = t3
    const shadowSP = t4

    # calculate the SP after popping all arguments
    move sp, argSP
    loadh IPInt::CallSignatureMetadata::numArguments[MC], t2
    lshiftp StackValueShift, t2
    addp t2, argSP

    # (down = decreasing address)
    # <first non-arg> <- argSP = SP after all arguments
    # arg
    # ...
    # arg
    # arg             <- initial SP

    # store sp as our shadow stack for arguments later
    move sp, t4
    # make extra space if necessary
    move sp, shadowSP
    loadh IPInt::CallSignatureMetadata::numExtraResults[MC], t2
    lshiftp StackValueShift, t2
    subp t2, sp

    # <first non-arg> <- argSP
    # arg
    # ...
    # arg
    # arg             <- shadowSP = initial sp
    # reserved
    # reserved        <- sp

    # align sp
    subp 8, sp

    push argSP, PC
    push PL, wasmInstance

    # set up the call frame
    move sp, t3
    loadi IPInt::CallSignatureMetadata::stackFrameSize[MC], t2
    subp t2, sp

    advanceMC(constexpr (sizeof(IPInt::CallSignatureMetadata)))

    # <first non-arg> <- argSP
    # arg
    # ...
    # arg
    # arg             <- shadowSP = initial sp
    # reserved
    # reserved
    # argSP, PC
    # PL, wasmInstance
    # call frame
    # call frame
    # call frame
    # call frame
    # call frame
    # call frame      <- sp

    # set up the Callee slot
    store2ia IPIntCallCallee, (constexpr JSValue::NativeCalleeTag), Callee - CallerFrameAndPCSize[sp]
    storep IPIntCallFunctionSlot, CodeBlock - CallerFrameAndPCSize[sp]

    push targetEntrypoint, targetInstance
    move t3, mintDst

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

    # keep the top of IPInt stack in mintSS as shadow stack
    move sp, mintSS
    # we pushed four values previously, so offset for this
    addp 16, mintSS

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
    #  argument n                  <- mintSS
    #  entrypoint, targetInstance
    #  callee, function info       <- sp

    # determine the location to begin copying stack arguments, starting from the last
    move cfr, mintDst
    addp FirstArgumentOffset, mintDst
    addp t3, mintDst

    #  <caller frame>
    #  return val                  <- mintDst
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
    #  argument n                  <- mintSS
    #  entrypoint, targetInstance
    #  callee, function info       <- sp

    # get saved MC and PC
    load2ia IPIntCalleeSaveSpaceMCPC[cfr], t0, t1
    push t0, t1

    # store the return address and CFR on the stack so we don't lose it
    loadp ReturnPC[cfr], t0
    loadp [cfr], t1

    push t0, t1

    #  <caller frame>
    #  return val                  <- mintDst
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
    #  argument n                  <- mintSS
    #  entrypoint, targetInstance
    #  callee, function info
    #  saved MC/PC
    #  return address, saved CFR   <- sp

.ipint_mint_arg_dispatch:
    # on x86, we'll use PC for our PC base
    initPCRelative(mint_arg, PC)

    mintArgDispatch()

    # tail calls reuse most of mINT's argument logic, but exit into a different tail call stub.
    # we use sc2 to keep the new stack frame

mintAlign(_a0)
_mint_begin:
    mintPop(a1, a0)
    mintArgDispatch()

mintAlign(_a1)
    mintPop(a3, a2)
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
    mintPop(argumINTTmp, lr)
    store2ia argumINTTmp, lr, [sc3]
    mintArgDispatch()

mintAlign(_stackeight)
    mintPop(argumINTTmp, lr)
    subp 16, sc3
    store2ia argumINTTmp, lr, 8[sc3]
    mintArgDispatch()

# Since we're writing into the same frame, we're going to first push stack arguments onto the stack.
# Once we're done, we'll copy them back down into the new frame, to avoid having to deal with writing over
# arguments lower down on the stack.

mintAlign(_tail_stackzero)
    break

mintAlign(_tail_stackeight)
    break

mintAlign(_gap)
    break

mintAlign(_tail_gap)
    break

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

    mintRetDispatch()

mintAlign(_r0)
_mint_begin_return:
    subp StackValueSize, mintRetDst
    store2ia r0, r1, [mintRetDst]
    mintRetDispatch()

mintAlign(_r1)
    subp StackValueSize, mintRetDst
    store2ia a2, a3, [mintRetDst]
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
    stored fr, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr1)
    subp StackValueSize, mintRetDst
    stored fa1, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr2)
    subp StackValueSize, mintRetDst
    stored wfa2, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr3)
    subp StackValueSize, mintRetDst
    stored wfa3, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr4)
    subp StackValueSize, mintRetDst
    stored wfa4, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr5)
    subp StackValueSize, mintRetDst
    stored wfa5, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr6)
    subp StackValueSize, mintRetDst
    stored wfa6, [mintRetDst]
    mintRetDispatch()

mintAlign(_fr7)
    subp StackValueSize, mintRetDst
    stored wfa7, [mintRetDst]
    mintRetDispatch()

mintAlign(_stack)
    load2ia [mintRetSrc], sc0, sc1
    addp SlotSize, mintRetSrc
    subp StackValueSize, mintRetDst
    store2ia sc0, sc1, [mintRetDst]
    mintRetDispatch()

mintAlign(_stack_gap)
    addp SlotSize, mintRetSrc
    mintRetDispatch()

mintAlign(_end)

    # <first non-arg>   <- argSP
    # return result
    # ...
    # return result
    # return result
    # return result
    # return result     <- mintRetDst => new SP
    # argSP, PC
    # PL, wasmInstance  <- sc2
    # call frame return <- sp
    # call frame return
    # call frame
    # call frame
    # call frame
    # call frame

    # Restore PC, WI, PL
    loadp 2*MachineRegisterSize[sc2], PC
    # note: we don't care about argSP anymore
    load2ia [sc2], wasmInstance, PL
    move mintRetDst, sp

    push MC
    getIPIntCallee()
    pop MC

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
    #  argument n                  <- mintSS
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
.ipint_tail_call_copy_stackargs_loop:
    btiz sc3, .ipint_tail_call_copy_stackargs_loop_end
    load2ia [sp], sc0, sc1
    store2ia sc0, sc1, [sc2, sc3]
    load2ia 8[sp], sc0, sc1
    store2ia sc0, sc1, 8[sc2, sc3]

    addp 16, sc3
    addp 16, sp
    jmp .ipint_tail_call_copy_stackargs_loop

.ipint_tail_call_copy_stackargs_loop_end:

    # reload it here, which isn't optimal, but we don't really have registers
    loadi [MC], sc3
    mulp SlotSize, sc3
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
    store2ia sc0, (constexpr JSValue::NativeCalleeTag), Callee[sc2]
    storep sc3, CodeBlock[sc2]

    # take off the last two values we stored, and move SP down to make it look like a fresh frame
    pop sc0, sc3  # sc0 = targetInstance

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
    move sc0, wasmInstance

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
    jmp sc3, WasmEntryPtrTag

###########################################
# uINT: function return value interpreter #
###########################################

uintAlign(_r0)
_uint_begin:
    popQuad(r1, r0)
    uintDispatch()

uintAlign(_r1)
    break

uintAlign(_r2)
    popQuad(a3, a2)
    uintDispatch()

uintAlign(_r3)
    break

uintAlign(_r4)
    break

uintAlign(_r5)
    break

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
    popFloat(fr)
    uintDispatch()

uintAlign(_fr1)
    popFloat(fa1)
    uintDispatch()

uintAlign(_fr2)
    popFloat(wfa2)
    uintDispatch()

uintAlign(_fr3)
    popFloat(wfa3)
    uintDispatch()

uintAlign(_fr4)
    popFloat(wfa4)
    uintDispatch()

uintAlign(_fr5)
    popFloat(wfa5)
    uintDispatch()

uintAlign(_fr6)
    popFloat(wfa6)
    uintDispatch()

uintAlign(_fr7)
    popFloat(wfa7)
    uintDispatch()

# destination on stack is sc0

uintAlign(_stack)
    popInt64(argumINTTmp, lr)
    store2ia argumINTTmp, lr, [sc0]
    subp 8, sc0
    uintDispatch()

uintAlign(_ret)
    jmp .ipint_exit

# MC = location in argumINT bytecode
# csr1 = tmp
# t4 = dst
# t5 = src
# t7 = for dispatch

argumINTAlign(_a0)
_argumINT_begin:
    storei a0, [argumINTDst]
    storei a1, 4[argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_a1)
    storei a2, [argumINTDst]
    storei a3, 4[argumINTDst]
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
    stored fa0, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_fa1)
    stored fa1, [argumINTDst]
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
    load2ia [argumINTSrc], argumINTTmp, lr
    addp 8, argumINTSrc
    store2ia argumINTTmp, lr, [argumINTDst]
    addp LocalSize, argumINTDst
    argumINTDispatch()

argumINTAlign(_end)
    jmp .ipint_entry_end_local


