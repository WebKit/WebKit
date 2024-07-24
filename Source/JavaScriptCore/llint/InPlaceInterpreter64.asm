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

##########
# Macros #
##########

# Callee Save

macro saveIPIntRegisters()
    subp IPIntCalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        storepairq PM, PB, -16[cfr]
        storeq wasmInstance, -24[cfr]
    elsif X86_64 or RISCV64
        storep PB, -0x8[cfr]
        storep PM, -0x10[cfr]
        storep wasmInstance, -0x18[cfr]
    end
end

macro restoreIPIntRegisters()
    if ARM64 or ARM64E
        loadpairq -16[cfr], PM, PB
        loadq -24[cfr], wasmInstance
    elsif X86_64 or RISCV64
        loadp -0x8[cfr], PB
        loadp -0x10[cfr], PM
        loadp -0x18[cfr], wasmInstance
    end
    addp IPIntCalleeSaveSpaceStackAligned, sp
end

# Tail-call dispatch

macro nextIPIntInstruction()
    # Consistency check
    # move MC, t0
    # andp 7, t0
    # bpeq t0, 0, .fine
    # break
# .fine:
    loadb [PB, PC, 1], t0
if ARM64 or ARM64E
    # x7 = IB
    # x0 = opcode
    emit "add x0, x7, x0, lsl #8"
    emit "br x0"
elsif X86_64
    lshiftq 8, t0
    leap (_ipint_unreachable), t1
    addq t1, t0
    emit "jmp *(%eax)"
else
    break
end
end

# Stack operations
# Every value on the stack is always 16 bytes! This makes life easy.

macro pushQuad(reg)
    if ARM64 or ARM64E
        push reg, reg
    elsif X86_64
        push reg
    else
        break
    end
end

macro pushQuadPair(reg1, reg2)
    push reg1, reg2
end

macro popQuad(reg, scratch)
    if ARM64 or ARM64E
        pop reg, scratch
    elsif X86_64
        pop reg
    else
        break
    end
end

macro pushVectorReg0()
    if ARM64 or ARM64E
        emit "str q0, [sp, #-16]!"
    elsif X86_64
        emit "sub $16, %esp"
        emit "movdqu %xmm0, (%esp)"
    else
        break
    end
end

macro pushVectorReg1()
    if ARM64 or ARM64E
        emit "str q1, [sp, #-16]!"
    elsif X86_64
        emit "sub $16, %esp"
        emit "movdqu %xmm1, (%esp)"
    else
        break
    end
end

macro pushVectorReg2()
    if ARM64 or ARM64E
        emit "str q2, [sp, #-16]!"
    elsif X86_64
        emit "sub $16, %esp"
        emit "movdqu %xmm2, (%esp)"
    else
        break
    end
end

macro popVectorReg0()
    if ARM64 or ARM64E
        emit "ldr q0, [sp], #16"
    elsif X86_64
        emit "movdqu (%esp), %xmm0"
        emit "add $16, %esp"
    else
        break
    end
end

macro popVectorReg1()
    if ARM64 or ARM64E
        emit "ldr q1, [sp], #16"
    elsif X86_64
        emit "movdqu (%esp), %xmm1"
        emit "add $16, %esp"
    else
        break
    end
end

macro popVectorReg2()
    if ARM64 or ARM64E
        emit "ldr q2, [sp], #16"
    elsif X86_64
        emit "movdqu (%esp), %xmm2"
        emit "add $16, %esp"
    else
        break
    end
end

# Pushes ft0 because macros
macro pushFPR()
    if ARM64 or ARM64E
        emit "str q0, [sp, #-16]!"
    elsif X86_64
        emit "sub $16, %esp"
        emit "movdqu %xmm0, (%esp)"
    else
        break
    end
end

macro pushFPR1()
    if ARM64 or ARM64E
        emit "str q1, [sp, #-16]!"
    elsif X86_64
        emit "sub $16, %esp"
        emit "movdqu %xmm1, (%esp)"
    else
        break
    end
end

macro popFPR()
    if ARM64 or ARM64E
        # We'll just drop the entire q0 register in here
        # to keep stack aligned to 16
        # We'll never actually use q0 as a whole for FP,
        # since we only work with f32 (s0) or f64 (d0)
        emit "ldr q0, [sp], #16"
    elsif X86_64
        emit "movdqu (%esp), %xmm0"
        emit "add $16, %esp"
    else
        break
    end
end

macro popFPR1()
    if ARM64 or ARM64E
        emit "ldr q1, [sp], #16"
    elsif X86_64
        emit "movdqu (%esp), %xmm1"
        emit "add $16, %esp"
    else
        break
    end
end

# Typed push/pop to make code pretty

macro pushInt32(reg)
    pushQuad(reg)
end

macro popInt32(reg, scratch)
    popQuad(reg, scratch)
end

macro pushInt64(reg)
    pushQuad(reg)
end

macro popInt64(reg, scratch)
    popQuad(reg, scratch)
end

# Entry

# PM = location in argumINT bytecode
# csr0 = tmp
# csr1 = dst
# csr2 = src
# csr3 = end
# csr4 = for dispatch

const argumINTDest = csr1
const argumINTSrc = csr2
    
macro ipintEntry()
    checkStackOverflow(ws0, csr3)

    # Allocate space for locals and rethrow values
    if ARM64 or ARM64E
        loadpairi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], csr0, csr3
    else
        loadi Wasm::IPIntCallee::m_localSizeToAlloc[ws0], csr0
        loadi Wasm::IPIntCallee::m_numRethrowSlotsToAlloc[ws0], csr3
    end
    addq csr3, csr0
    mulq LocalSize, csr0
    move sp, csr3
    subq csr0, sp
    move sp, csr4
    loadp Wasm::IPIntCallee::m_argumINTBytecodePointer[ws0], PM

    push csr0, csr1, csr2, csr3

    move csr4, argumINTDest
    leap FirstArgumentOffset[cfr], argumINTSrc

    argumINTDispatch()
end

macro argumINTDispatch()
    loadb [PM], csr0
    addq 1, PM
    lshiftq 6, csr0
if ARM64 or ARM64E
    pcrtoaddr _argumINT_begin, csr4
    addq csr0, csr4
    emit "br x23"
elsif X86_64
    leap (_argumINT_begin), csr4
    addq csr0, csr4
    emit "jmp *(%r13)"
else
    break
end
end

macro argumINTEnd()
    # zero out remaining locals
    bqeq argumINTDest, csr3, .ipint_entry_finish_zero
    storeq 0, [argumINTDest]
    addq 8, argumINTDest
end

macro argumINTFinish()
    pop csr3, csr2, csr1, csr0
end

    #############################
    # 0x00 - 0x11: control flow #
    #############################

instructionLabel(_unreachable)
    # unreachable
    ipintException(Unreachable)

instructionLabel(_nop)
    # nop
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_block)
    # block
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_loop)
    # loop
    ipintLoopOSR(1)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_if)
    # if
    popInt32(t0, t1)
    bqneq 0, t0, .ipint_if_taken
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()
.ipint_if_taken:
    # Skip LEB128
    loadb 8[PM, MC], t0
    advanceMC(9)
    advancePCByReg(t0)
    nextIPIntInstruction()

instructionLabel(_else)
    # else
    # Counterintuitively, we only run this instruction if the if
    # clause is TAKEN. This is used to branch to the end of the
    # block.
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_try)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_catch)
    # Counterintuitively, like else, we only run this instruction
    # if no exception was thrown during the preceeding try or catch block.
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_throw)
    storei PC, CallSiteIndex[cfr]

    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t0
    loadp VM::topEntryFrame[t0], t0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t0)

    move cfr, a1
    move sp, a2
    loadi [PM, MC], a3
    operationCall(macro() cCall4(_ipint_extern_throw_exception) end)
    jumpToException()

instructionLabel(_rethrow)
    storei PC, CallSiteIndex[cfr]

    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t0
    loadp VM::topEntryFrame[t0], t0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t0)

    move cfr, a1
    move PL, a2
    loadi [PM, MC], a3
    operationCall(macro() cCall4(_ipint_extern_rethrow_exception) end)
    jumpToException()

reservedOpcode(0xa)

macro uintDispatch()
if ARM64 or ARM64E
    loadb [PM], ws2
    addq 1, PM
    bilt ws2, 5, .safe
    break
.safe:
    lshiftq 6, ws2
    pcrtoaddr _uint_begin, ws3
    addq ws2, ws3
    # ws3 = x12
    emit "br x12"
elsif X86_64
    loadb [PM], r1
    addq 1, PM
    bilt r1, 5, .safe
    break
.safe:
    lshiftq 6, r1
    leap (_uint_begin), t0
    addq r1, t0
    emit "jmp *(%rax)"
end
end

instructionLabel(_end)
    loadi Wasm::IPIntCallee::m_bytecodeLength[ws0], t0
    subq 1, t0
    bqeq PC, t0, .ipint_end_ret
    advancePC(1)
    nextIPIntInstruction()
.ipint_end_ret:
    ipintEpilogueOSR(10)
    addq MC, PM
    uintDispatch()

instructionLabel(_br)
    # br
    # number to pop
    loadh 8[PM, MC], t0
    # number to keep
    loadh 10[PM, MC], t1

    # ex. pop 3 and keep 2
    #
    # +4 +3 +2 +1 sp
    # a  b  c  d  e
    # d  e
    #
    # [sp + k + numToPop] = [sp + k] for k in numToKeep-1 -> 0
    move t0, t2
    lshiftq 4, t2
    leap [sp, t2], t2

.ipint_br_poploop:
    bqeq t1, 0, .ipint_br_popend
    subq 1, t1
    move t1, t3
    lshiftq 4, t3
    loadq [sp, t3], t0
    storeq t0, [t2, t3]
    loadq 8[sp, t3], t0
    storeq t0, 8[t2, t3]
    jmp .ipint_br_poploop
.ipint_br_popend:
    loadh 8[PM, MC], t0
    lshiftq 4, t0
    leap [sp, t0], sp
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_br_if)
    # pop i32
    popInt32(t0, t2)
    bineq t0, 0, _ipint_br
    loadb 12[PM, MC], t0
    advanceMC(13)
    advancePCByReg(t0)
    nextIPIntInstruction()

instructionLabel(_br_table)
    # br_table
    popInt32(t0, t2)
    loadi [PM, MC], t1
    advanceMC(4)
    biaeq t0, t1, .ipint_br_table_maxout
    move t0, t1
    lshiftq 3, t0
    lshiftq 2, t1
    addq t1, t0
    addq t0, MC
    jmp _ipint_br
.ipint_br_table_maxout:
    subq 1, t1
    move t1, t2
    lshiftq 3, t1
    lshiftq 2, t2
    addq t2, t1
    addq t1, MC
    jmp _ipint_br

instructionLabel(_return)
    # ret
    loadi Wasm::IPIntCallee::m_bytecodeLength[ws0], PC
    loadi Wasm::IPIntCallee::m_returnMetadata[ws0], MC
    subq 1, PC
    # This is guaranteed going to an end instruction, so skip
    # dispatch and end of program check for speed
    jmp .ipint_end_ret

instructionLabel(_call)
    storei PC, CallSiteIndex[cfr]

    # call
    jmp _ipint_call_impl

instructionLabel(_call_indirect)
    storei PC, CallSiteIndex[cfr]

    # Get ref
    # Load pre-computed values from metadata
    popInt32(t0, t1)
    push PC, MC # a4
    move t0, a2
    leap 1[PM, MC], a3
    move wasmInstance, a0
    move cfr, a1
    operationCall(macro() cCall4(_ipint_extern_call_indirect) end)
    pop MC, PC
    btpz r1, .ipint_call_indirect_throw

    loadb [PM, MC], t2
    advancePCByReg(t2)
    advanceMC(9)

    jmp .ipint_call_common
.ipint_call_indirect_throw:
    jmp _wasm_throw_from_slow_path_trampoline

reservedOpcode(0x12)
reservedOpcode(0x13)
reservedOpcode(0x14)
reservedOpcode(0x15)
reservedOpcode(0x16)
reservedOpcode(0x17)

instructionLabel(_delegate)
    # Counterintuitively, like else, we only run this instruction
    # if no exception was thrown during the preceeding try or catch block.
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_catch_all)
    # Counterintuitively, like else, we only run this instruction
    # if no exception was thrown during the preceeding try or catch block.
    loadi [PM, MC], PC
    loadi 4[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_drop)
    addq StackValueSize, sp
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_select)
    popInt32(t0, t2)
    bieq t0, 0, .ipint_select_val2
    addq 16, sp
    advancePC(1)
    advanceMC(8)
    nextIPIntInstruction()
.ipint_select_val2:
    popQuad(t1, t2)
    popQuad(t0, t2)
    pushQuad(t1)
    advancePC(1)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_select_t)
    popInt32(t0, t2)
    bieq t0, 0, .ipint_select_t_val2
    addq 16, sp
    loadi [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()
.ipint_select_t_val2:
    popQuad(t1, t2)
    popQuad(t0, t3)
    pushQuadPair(t2, t1)
    loadi [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

reservedOpcode(0x1d)
reservedOpcode(0x1e)
reservedOpcode(0x1f)

    ###################################
    # 0x20 - 0x26: get and set values #
    ###################################

instructionLabel(_local_get)
    # local.get
    loadb 1[PB, PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_get_slow_path
.ipint_local_get_post_decode:
    # Index into locals
    loadq [PL, t0, LocalSize], t0
    # Push to stack
    pushQuad(t0)
    nextIPIntInstruction()

instructionLabel(_local_set)
    # local.set
    loadb 1[PB, PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_set_slow_path
.ipint_local_set_post_decode:
    # Pop from stack
    popQuad(t2, t3)
    # Store to locals
    storeq t2, [PL, t0, LocalSize]
    nextIPIntInstruction()

instructionLabel(_local_tee)
    # local.tee
    loadb 1[PB, PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_tee_slow_path
.ipint_local_tee_post_decode:
    # Load from stack
    loadq [sp], t2
    # Store to locals
    storeq t2, [PL, t0, LocalSize]
    nextIPIntInstruction()

instructionLabel(_global_get)
    # Load pre-computed index from metadata
    loadh 6[PM, MC], t2
    loadi [PM, MC], t1
    loadp JSWebAssemblyInstance::m_globals[wasmInstance], t0
    lshiftp 1, t1
    loadq [t0, t1, 8], t0
    bieq t2, 0, .ipint_global_get_embedded
    loadq [t0], t0
.ipint_global_get_embedded:
    pushQuad(t0)

    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_global_set)
    # b7 = 1 => ref, use slowpath
    loadb 7[PM, MC], t0
    bineq t0, 0, .ipint_global_set_refpath
    # b6 = 1 => portable
    loadb 6[PM, MC], t2
    # get global addr
    loadp JSWebAssemblyInstance::m_globals[wasmInstance], t0
    # get value to store
    popQuad(t3, t1)
    # get index
    loadi [PM, MC], t1
    lshiftp 1, t1
    bieq t2, 0, .ipint_global_set_embedded
    # portable: dereference then set
    loadq [t0, t1, 8], t0
    storeq t3, [t0]
    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()
.ipint_global_set_embedded:
    # embedded: set directly
    storeq t3, [t0, t1, 8]
    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

.ipint_global_set_refpath:
    loadi [PM, MC], a1
    # Pop from stack
    popQuad(a2, t3)
    operationCall(macro() cCall3(_ipint_extern_set_global_64) end)

    loadh 4[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(8)
    nextIPIntInstruction()

instructionLabel(_table_get)
    # Load pre-computed index from metadata
    loadi 1[PM, MC], a1
    popInt32(a2, t3)

    operationCallMayThrow(macro() cCall3(_ipint_extern_table_get) end)

    pushQuad(t0)

    loadb [PM, MC], t0

    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_table_set)
    # Load pre-computed index from metadata
    loadi 1[PM, MC], a1
    popQuad(a3, t0)
    popInt32(a2, t0)
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_set) end)

    loadb [PM, MC], t0

    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

reservedOpcode(0x27)

macro ipintCheckMemoryBound(mem, scratch, size)
    leap size - 1[mem], scratch
    bpb scratch, boundsCheckingSize, .continuation
    ipintException(OutOfBoundsMemoryAccess)
.continuation:
end

instructionLabel(_i32_load_mem)
    # i32.load
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    pushInt32(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_load_mem)
    # i32.load
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    loadq [memoryBase, t0], t1
    pushInt64(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_f32_load_mem)
    # f32.load
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadf [memoryBase, t0], ft0
    pushFloat32FT0()

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()
    
instructionLabel(_f64_load_mem)
    # f64.load
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    loadd [memoryBase, t0], ft0
    pushFloat64FT0()

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()
    

instructionLabel(_i32_load8s_mem)
    # i32.load8_s
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    sxb2i t1, t1
    pushInt32(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i32_load8u_mem)
    # i32.load8_u
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    pushInt32(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i32_load16s_mem)
    # i32.load16_s
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    sxh2i t1, t1
    pushInt32(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i32_load16u_mem)
    # i32.load16_u
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    pushInt32(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()


instructionLabel(_i64_load8s_mem)
    # i64.load8_s
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    sxb2q t1, t1
    pushInt64(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_load8u_mem)
    # i64.load8_u
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    pushInt64(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_load16s_mem)
    # i64.load16_s
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    sxh2q t1, t1
    pushInt64(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_load16u_mem)
    # i64.load16_u
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    pushInt64(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_load32s_mem)
    # i64.load32_s
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    sxi2q t1, t1
    pushInt64(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_load32u_mem)
    # i64.load8_s
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    pushInt64(t1)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()


instructionLabel(_i32_store_mem)
    # i32.store
    # pop data
    popInt32(t1, t2)
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    storei t1, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_store_mem)
    # i64.store
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    storeq t1, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_f32_store_mem)
    # f32.store
    # pop data
    popFloat32FT0()
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    storef ft0, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_f64_store_mem)
    # f64.store
    # pop data
    popFloat64FT0()
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 8)
    # load memory location
    stored ft0, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i32_store8_mem)
    # i32.store8
    # pop data
    popInt32(t1, t2)
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    storeb t1, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i32_store16_mem)
    # i32.store16
    # pop data
    popInt32(t1, t2)
    # pop index
    popInt32(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    storeh t1, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()
    
instructionLabel(_i64_store8_mem)
    # i64.store8
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 1)
    # load memory location
    storeb t1, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_store16_mem)
    # i64.store16
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 2)
    # load memory location
    storeh t1, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_store32_mem)
    # i64.store32
    # pop data
    popInt64(t1, t2)
    # pop index
    popInt64(t0, t2)
    loadi 1[PM, MC], t2
    addq t2, t0
    ipintCheckMemoryBound(t0, t2, 4)
    # load memory location
    storei t1, [memoryBase, t0]

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()


instructionLabel(_memory_size)
    operationCall(macro() cCall2(_ipint_extern_current_memory) end)
    pushInt32(r0)
    advancePC(2)
    nextIPIntInstruction()

instructionLabel(_memory_grow)
    popInt32(a1, t2)
    operationCall(macro() cCall2(_ipint_extern_memory_grow) end)
    pushInt32(r0)
    ipintReloadMemory()
    advancePC(2)
    nextIPIntInstruction()

    ################################
    # 0x41 - 0x44: constant values #
    ################################

instructionLabel(_i32_const)
    # i32.const
    loadb [PM, MC], t1
    bigteq t1, 2, .ipint_i32_const_slowpath
    loadb 1[PB, PC], t0
    lshiftq 7, t1
    orq t1, t0
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(2)
    advanceMC(1)
    nextIPIntInstruction()
.ipint_i32_const_slowpath:
    # Load pre-computed value from metadata
    loadi 1[PM, MC], t0
    # Push to stack
    pushInt32(t0)

    advancePCByReg(t1)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_i64_const)
    # i64.const
    # Load pre-computed value from metadata
    loadq 1[PM, MC], t0
    # Push to stack
    pushInt64(t0)
    loadb [PM, MC], t0

    advancePCByReg(t0)
    advanceMC(9)
    nextIPIntInstruction()

instructionLabel(_f32_const)
    # f32.const
    # Load pre-computed value from metadata
    loadf 1[PB, PC], ft0
    pushFloat32FT0()

    advancePC(5)
    nextIPIntInstruction()

instructionLabel(_f64_const)
    # f64.const
    # Load pre-computed value from metadata
    loadd 1[PB, PC], ft0
    pushFloat64FT0()

    advancePC(9)
    nextIPIntInstruction()

    ###############################
    # 0x45 - 0x4f: i32 comparison #
    ###############################

instructionLabel(_i32_eqz)
    # i32.eqz
    popInt32(t0, t2)
    cieq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_eq)
    # i32.eq
    popInt32(t1, t2)
    popInt32(t0, t2)
    cieq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ne)
    # i32.ne
    popInt32(t1, t2)
    popInt32(t0, t2)
    cineq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_lt_s)
    # i32.lt_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cilt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_lt_u)
    # i32.lt_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cib t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_gt_s)
    # i32.gt_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cigt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_gt_u)
    # i32.gt_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cia t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_le_s)
    # i32.le_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cilteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_le_u)
    # i32.le_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    cibeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ge_s)
    # i32.ge_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    cigteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ge_u)
    # i32.ge_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    ciaeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x50 - 0x5a: i64 comparison #
    ###############################

instructionLabel(_i64_eqz)
    # i64.eqz
    popInt64(t0, t2)
    cqeq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_eq)
    # i64.eq
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ne)
    # i64.ne
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqneq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_lt_s)
    # i64.lt_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqlt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_lt_u)
    # i64.lt_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqb t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_gt_s)
    # i64.gt_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqgt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_gt_u)
    # i64.gt_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqa t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_le_s)
    # i64.le_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqlteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_le_u)
    # i64.le_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqbeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ge_s)
    # i64.ge_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqgteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ge_u)
    # i64.ge_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    cqaeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x5b - 0x60: f32 comparison #
    ###############################

instructionLabel(_f32_eq)
    # f32.eq
    popFloat32FT1()
    popFloat32FT0()
    cfeq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_ne)
    # f32.ne
    popFloat32FT1()
    popFloat32FT0()
    cfnequn ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_lt)
    # f32.lt
    popFloat32FT1()
    popFloat32FT0()
    cflt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_gt)
    # f32.gt
    popFloat32FT1()
    popFloat32FT0()
    cfgt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_le)
    # f32.le
    popFloat32FT1()
    popFloat32FT0()
    cflteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_ge)
    # f32.ge
    popFloat32FT1()
    popFloat32FT0()
    cfgteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()


    ###############################
    # 0x61 - 0x66: f64 comparison #
    ###############################

instructionLabel(_f64_eq)
    # f64.eq
    popFloat64FT1()
    popFloat64FT0()
    cdeq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_ne)
    # f64.ne
    popFloat64FT1()
    popFloat64FT0()
    cdnequn ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_lt)
    # f64.lt
    popFloat64FT1()
    popFloat64FT0()
    cdlt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_gt)
    # f64.gt
    popFloat64FT1()
    popFloat64FT0()
    cdgt ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_le)
    # f64.le
    popFloat64FT1()
    popFloat64FT0()
    cdlteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_ge)
    # f64.ge
    popFloat64FT1()
    popFloat64FT0()
    cdgteq ft0, ft1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x67 - 0x78: i32 operations #
    ###############################

instructionLabel(_i32_clz)
    # i32.clz
    popInt32(t0, t2)
    lzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ctz)
    # i32.ctz
    popInt32(t0, t2)
    tzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_popcnt)
    # i32.popcnt
    popInt32(t1, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcount) end)
    pushInt32(r1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_add)
    # i32.add
    popInt32(t1, t2)
    popInt32(t0, t2)
    addi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_sub)
    # i32.sub
    popInt32(t1, t2)
    popInt32(t0, t2)
    subi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_mul)
    # i32.mul
    popInt32(t1, t2)
    popInt32(t0, t2)
    muli t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_div_s)
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

instructionLabel(_i32_div_u)
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

instructionLabel(_i32_rem_s)
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

instructionLabel(_i32_rem_u)
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

instructionLabel(_i32_and)
    # i32.and
    popInt32(t1, t2)
    popInt32(t0, t2)
    andi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_or)
    # i32.or
    popInt32(t1, t2)
    popInt32(t0, t2)
    ori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_xor)
    # i32.xor
    popInt32(t1, t2)
    popInt32(t0, t2)
    xori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shl)
    # i32.shl
    popInt32(t1, t2)
    popInt32(t0, t2)
    lshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shr_s)
    # i32.shr_s
    popInt32(t1, t2)
    popInt32(t0, t2)
    rshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shr_u)
    # i32.shr_u
    popInt32(t1, t2)
    popInt32(t0, t2)
    urshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_rotl)
    # i32.rotl
    popInt32(t1, t2)
    popInt32(t0, t2)
    lrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_rotr)
    # i32.rotr
    popInt32(t1, t2)
    popInt32(t0, t2)
    rrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x79 - 0x8a: i64 operations #
    ###############################

instructionLabel(_i64_clz)
    # i64.clz
    popInt64(t0, t2)
    lzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ctz)
    # i64.ctz
    popInt64(t0, t2)
    tzcntq t0, t1
    pushInt64(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_popcnt)
    # i64.popcnt
    popInt64(t1, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcountll) end)
    pushInt64(r1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_add)
    # i64.add
    popInt64(t1, t2)
    popInt64(t0, t2)
    addq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_sub)
    # i64.sub
    popInt64(t1, t2)
    popInt64(t0, t2)
    subq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_mul)
    # i64.mul
    popInt64(t1, t2)
    popInt64(t0, t2)
    mulq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_div_s)
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

instructionLabel(_i64_div_u)
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

instructionLabel(_i64_rem_s)
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

instructionLabel(_i64_rem_u)
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

instructionLabel(_i64_and)
    # i64.and
    popInt64(t1, t2)
    popInt64(t0, t2)
    andq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_or)
    # i64.or
    popInt64(t1, t2)
    popInt64(t0, t2)
    orq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_xor)
    # i64.xor
    popInt64(t1, t2)
    popInt64(t0, t2)
    xorq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_shl)
    # i64.shl
    popInt64(t1, t2)
    popInt64(t0, t2)
    lshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_shr_s)
    # i64.shr_s
    popInt64(t1, t2)
    popInt64(t0, t2)
    rshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_shr_u)
    # i64.shr_u
    popInt64(t1, t2)
    popInt64(t0, t2)
    urshiftq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_rotl)
    # i64.rotl
    popInt64(t1, t2)
    popInt64(t0, t2)
    lrotateq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_rotr)
    # i64.rotr
    popInt64(t1, t2)
    popInt64(t0, t2)
    rrotateq t1, t0
    pushInt64(t0)

    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x8b - 0x98: f32 operations #
    ###############################

instructionLabel(_f32_abs)
    # f32.abs
    popFloat32FT0()
    absf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_neg)
    # f32.neg
    popFloat32FT0()
    negf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_ceil)
    # f32.ceil
    popFloat32FT0()
    ceilf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_floor)
    # f32.floor
    popFloat32FT0()
    floorf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_trunc)
    # f32.trunc
    popFloat32FT0()
    truncatef ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_nearest)
    # f32.nearest
    popFloat32FT0()
    roundf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_sqrt)
    # f32.sqrt
    popFloat32FT0()
    sqrtf ft0, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()
    
instructionLabel(_f32_add)
    # f32.add
    popFloat32FT1()
    popFloat32FT0()
    addf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_sub)
    # f32.sub
    popFloat32FT1()
    popFloat32FT0()
    subf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_mul)
    # f32.mul
    popFloat32FT1()
    popFloat32FT0()
    mulf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_div)
    # f32.div
    popFloat32FT1()
    popFloat32FT0()
    divf ft1, ft0
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_min)
    # f32.min
    popFloat32FT1()
    popFloat32FT0()
    bfeq ft0, ft1, .ipint_f32_min_equal
    bflt ft0, ft1, .ipint_f32_min_lt
    bfgt ft0, ft1, .ipint_f32_min_return

.ipint_f32_min_NaN:
    addf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_equal:
    orf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_lt:
    moved ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_min_return:
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_max)
    # f32.max
    popFloat32FT1()
    popFloat32FT0()

    bfeq ft1, ft0, .ipint_f32_max_equal
    bflt ft1, ft0, .ipint_f32_max_lt
    bfgt ft1, ft0, .ipint_f32_max_return

.ipint_f32_max_NaN:
    addf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_equal:
    andf ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_lt:
    moved ft0, ft1
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f32_max_return:
    pushFloat32FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_copysign)
    # f32.copysign
    popFloat32FT1()
    popFloat32FT0()

    ff2i ft1, t1
    move 0x80000000, t2
    andi t2, t1

    ff2i ft0, t0
    move 0x7fffffff, t2
    andi t2, t0

    ori t1, t0
    fi2f t0, ft0

    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x99 - 0xa6: f64 operations #
    ###############################

instructionLabel(_f64_abs)
    # f64.abs
    popFloat64FT0()
    absd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_neg)
    # f64.neg
    popFloat64FT0()
    negd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_ceil)
    # f64.ceil
    popFloat64FT0()
    ceild ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_floor)
    # f64.floor
    popFloat64FT0()
    floord ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_trunc)
    # f64.trunc
    popFloat64FT0()
    truncated ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_nearest)
    # f64.nearest
    popFloat64FT0()
    roundd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_sqrt)
    # f64.sqrt
    popFloat64FT0()
    sqrtd ft0, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()
    
instructionLabel(_f64_add)
    # f64.add
    popFloat64FT1()
    popFloat64FT0()
    addd ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_sub)
    # f64.sub
    popFloat64FT1()
    popFloat64FT0()
    subd ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_mul)
    # f64.mul
    popFloat64FT1()
    popFloat64FT0()
    muld ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_div)
    # f64.div
    popFloat64FT1()
    popFloat64FT0()
    divd ft1, ft0
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_min)
    # f64.min
    popFloat64FT1()
    popFloat64FT0()
    bdeq ft0, ft1, .ipint_f64_min_equal
    bdlt ft0, ft1, .ipint_f64_min_lt
    bdgt ft0, ft1, .ipint_f64_min_return

.ipint_f64_min_NaN:
    addd ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_equal:
    ord ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_lt:
    moved ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_min_return:
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_max)
    # f64.max
    popFloat64FT1()
    popFloat64FT0()

    bdeq ft1, ft0, .ipint_f64_max_equal
    bdlt ft1, ft0, .ipint_f64_max_lt
    bdgt ft1, ft0, .ipint_f64_max_return

.ipint_f64_max_NaN:
    addd ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_equal:
    andd ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_lt:
    moved ft0, ft1
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

.ipint_f64_max_return:
    pushFloat64FT1()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_copysign)
    # f64.copysign
    popFloat64FT1()
    popFloat64FT0()

    fd2q ft1, t1
    move 0x8000000000000000, t2
    andq t2, t1

    fd2q ft0, t0
    move 0x7fffffffffffffff, t2
    andq t2, t0

    orq t1, t0
    fq2d t0, ft0

    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

    ############################
    # 0xa7 - 0xc4: conversions #
    ############################

instructionLabel(_i32_wrap_i64)
    # because of how we store values on stack, do nothing
    advancePC(1)
    nextIPIntInstruction()


instructionLabel(_i32_trunc_f32_s)
    popFloat32FT0()
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

instructionLabel(_i32_trunc_f32_u)
    popFloat32FT0()
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

instructionLabel(_i32_trunc_f64_s)
    popFloat64FT0()
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

instructionLabel(_i32_trunc_f64_u)
    popFloat64FT0()
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

instructionLabel(_i64_extend_i32_s)
    popInt32(t0, t1)
    sxi2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend_i32_u)
    popInt32(t0, t1)
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_f32_s)
    popFloat32FT0()
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

instructionLabel(_i64_trunc_f32_u)
    popFloat32FT0()
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

instructionLabel(_i64_trunc_f64_s)
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

instructionLabel(_i64_trunc_f64_u)
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

instructionLabel(_f32_convert_i32_s)
    popInt32(t0, t1)
    ci2fs t0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_convert_i32_u)
    popInt32(t0, t1)
    ci2f t0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_convert_i64_s)
    popInt64(t0, t1)
    cq2fs t0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_convert_i64_u)
    popInt64(t0, t1)
    if X86_64
        cq2f t0, t1, ft0
    else
        cq2f t0, ft0
    end
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_demote_f64)
    popFloat64FT0()
    cd2f ft0, ft0
    pushFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i32_s)
    popInt32(t0, t1)
    ci2ds t0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i32_u)
    popInt32(t0, t1)
    ci2d t0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i64_s)
    popInt64(t0, t1)
    cq2ds t0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_convert_i64_u)
    popInt64(t0, t1)
    if X86_64
        cq2d t0, t1, ft0
    else
        cq2d t0, ft0
    end
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_promote_f32)
    popFloat32FT0()
    cf2d ft0, ft0
    pushFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_reinterpret_f32)
    popFloat32FT0()
    ff2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_reinterpret_f64)
    popFloat64FT0()
    fd2q ft0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_reinterpret_i32)
    pushInt32(t0)
    fi2f t0, ft0
    popFloat32FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_reinterpret_i64)
    pushInt64(t0)
    fq2d t0, ft0
    popFloat64FT0()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_extend8_s)
    # i32.extend8_s
    popInt32(t0, t1)
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_extend16_s)
    # i32.extend8_s
    popInt32(t0, t1)
    sxh2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend8_s)
    # i64.extend8_s
    popInt64(t0, t1)
    sxb2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend16_s)
    # i64.extend8_s
    popInt64(t0, t1)
    sxh2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend32_s)
    # i64.extend8_s
    popInt64(t0, t1)
    sxi2q t0, t0
    pushInt64(t0)
    advancePC(1)
    nextIPIntInstruction()

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
    # 0xd0 - 0xd2: refs #
    #####################

instructionLabel(_ref_null_t)
    loadi 1[PM, MC], t0
    pushQuad(t0)
    loadb [PM, MC], t0
    advancePC(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_ref_is_null)
    popQuad(t0, t1)
    cqeq t0, ValueNull, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_ref_func)
    move wasmInstance, a0
    loadi 1[PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_ref_func) end)
    pushQuad(t0)
    loadb [PM, MC], t0
    advancePC(t0)
    advanceMC(5)
    nextIPIntInstruction()

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
instructionLabel(_fc_block)
    decodeLEBVarUInt32(1, t0, t1, t2, t3, ws1)
    # Security guarantee: always less than 18 (0x00 -> 0x11)
    biaeq t0, 18, .ipint_fc_nonexistent
    if ARM64 or ARM64E
        pcrtoaddr _ipint_i32_trunc_sat_f32_s, t1
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
    elsif X86_64
        lshifti 4, t0
        leap (_ipint_i32_trunc_sat_f32_s), t1
        addq t1, t0
        emit "jmp *(%eax)"
    end

.ipint_fc_nonexistent:
    break

instructionLabel(_simd)
    # TODO: for relaxed SIMD, handle parsing the value.
    # Metadata? Could just hardcode loading two bytes though
    decodeLEBVarUInt32(1, t0, t1, t2, t3, ws1)
    if ARM64 or ARM64E
        pcrtoaddr _ipint_simd_v128_load_mem, t1
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
    elsif X86_64
        lshifti 4, t0
        leap (_ipint_simd_v128_load_mem), t1
        addq t1, t0
        emit "jmp *(%eax)"
    end

instructionLabel(_atomic)
    decodeLEBVarUInt32(1, t0, t1, t2, t3, ws1)
    # Security guarantee: always less than 78 (0x00 -> 0x4e)
    biaeq t0, 0x4f, .ipint_atomic_nonexistent
    if ARM64 or ARM64E
        pcrtoaddr _ipint_memory_atomic_notify, t1
        emit "add x0, x1, x0, lsl 8"
        emit "br x0"
    elsif X86_64
        lshifti 4, t0
        leap (_ipint_memory_atomic_notify), t1
        addq t1, t0
        emit "jmp *(%eax)"
    end

.ipint_atomic_nonexistent:
    break

reservedOpcode(0xff)

    #######################
    ## 0xFC instructions ##
    #######################

instructionLabel(_i32_trunc_sat_f32_s)
    popFloat32FT0()

    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMax

    truncatef2is ft0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_i32_trunc_sat_f32_u)
    popFloat32FT0()

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMin

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMax

    truncatef2i ft0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f32_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_i32_trunc_sat_f64_s)
    popFloat64FT0()

    move 0xc1e0000000200000, t0 # INT32_MIN - 1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax

    truncated2is ft0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin
    move 0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_s_outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_i32_trunc_sat_f64_u)
    popFloat64FT0()

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin

    move 0x41f0000000000000, t0 # INT32_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax

    truncated2i ft0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i32_trunc_sat_f64_u_outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    pushInt32(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f32_s)
    popFloat32FT0()

    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax

    truncatef2qs ft0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_s_outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f32_u)
    popFloat32FT0()

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMin

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMax

    truncatef2q ft0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f32_u_outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f64_s)
    popFloat64FT0()
    move 0xc3e0000000000000, t0 # INT64_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    move 0x43e0000000000000, t0 # -INT64_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncated2qs ft0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.outOfBoundsTruncSatMin:
    move (constexpr INT64_MIN), t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_i64_trunc_sat_f64_u)
    popFloat64FT0()

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMin

    move 0x43f0000000000000, t0 # INT64_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMax

    truncated2q ft0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMin:
    move 0, t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

.ipint_i64_trunc_sat_f64_u_outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    pushInt64(t0)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_memory_init)
    # memory.init
    popQuad(t0, t3) # n
    popQuad(t1, t3) # s
    popQuad(a2, t3) # d
    lshiftq 32, t1
    orq t1, t0
    move t0, a3
    loadi 1[PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_init) end)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_data_drop)
    # data.drop
    loadi 1[PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_data_drop) end)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_memory_copy)
    # memory.copy
    popQuad(a3, t0) # n
    popQuad(a2, t0) # s
    popQuad(a1, t0) # d
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_copy) end)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_memory_fill)
    # memory.fill
    popQuad(a3, t0) # n
    popQuad(a2, t0) # val
    popQuad(a1, t0) # d
    operationCallMayThrow(macro() cCall4(_ipint_extern_memory_fill) end)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

instructionLabel(_table_init)
    # memory.init
    popQuad(t0, t3) # n
    popQuad(t1, t3) # s
    popQuad(a2, t3) # d
    lshiftq 32, t1
    orq t1, t0
    move t0, a3
    leap [PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_init) end)
    loadb 8[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(9)
    nextIPIntInstruction()

instructionLabel(_elem_drop)
    # elem.drop
    loadi 1[PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_elem_drop) end)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_table_copy)
    # table.copy
    popQuad(t0, t3) # n
    popQuad(t1, t3) # s
    popQuad(a2, t3) # d
    lshiftq 32, t1
    orq t1, t0
    move t0, a3
    leap [PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_copy) end)
    loadb 8[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(9)
    nextIPIntInstruction()

instructionLabel(_table_grow)
    # table.grow
    loadi 1[PM, MC], a1
    popQuad(a3, t0) # n
    popQuad(a2, t0) # fill
    operationCall(macro() cCall4(_ipint_extern_table_grow) end)
    pushQuad(t0)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_table_size)
    # table.size
    loadi 1[PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_table_size) end)
    pushQuad(t0)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

instructionLabel(_table_fill)
    # table.fill
    popQuad(t0, t3) # n
    popQuad(a2, t3) # val
    popQuad(t3, t1) # i
    lshiftq 32, t3
    orq t0, t3
    loadi 1[PM, MC], a1
    operationCallMayThrow(macro() cCall4(_ipint_extern_table_fill) end)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

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
instructionLabel(_simd_v128_const)
    # v128.const
    leap [PM, MC], t0
    loadv 1[t0], v0
    loadb [t0], t0
    pushv v0
    advancePCByReg(t0)
    advanceMC(17)
    nextIPIntInstruction()

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

instructionLabel(_simd_i32x4_extract_lane)
    # i32x4.extract_lane (lane)
    loadb 2[PB, PC], t0  # lane index
    popv v0
    if ARM64 or ARM64E
        pcrtoaddr _simd_i32x4_extract_lane_0, t1
        leap [t1, t0, 8], t0
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
    bpb scratch, boundsCheckingSize, .continuation
.throw:
    ipintException(OutOfBoundsMemoryAccess)
.continuation:
    btpnz mem, (size - 1), .throw
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

instructionLabel(_memory_atomic_notify)
    # pop count
    popInt32(a3, t0)
    # pop pointer
    popInt32(a1, t0)
    # load offset
    loadi 1[PM, MC], a2

    move wasmInstance, a0
    operationCall(macro() cCall4(_ipint_extern_memory_atomic_notify) end)
    bilt r0, 0, .atomic_notify_throw

    pushInt32(r0)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

.atomic_notify_throw:
    ipintException(OutOfBoundsMemoryAccess)

instructionLabel(_memory_atomic_wait32)
    # pop timeout
    popInt32(a3, t0)
    # pop value
    popInt32(a2, t0)
    # pop pointer
    popInt32(a1, t0)
    # load offset
    loadi 1[PM, MC], t0
    # merge them since the slow path takes the combined pointer + offset.
    addq t0, a1

    move wasmInstance, a0
    operationCall(macro() cCall4(_ipint_extern_memory_atomic_wait32) end)
    bilt r0, 0, .atomic_wait32_throw

    pushInt32(r0)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

.atomic_wait32_throw:
    ipintException(OutOfBoundsMemoryAccess)

instructionLabel(_memory_atomic_wait64)
    # pop timeout
    popInt32(a3, t0)
    # pop value
    popInt64(a2, t0)
    # pop pointer
    popInt32(a1, t0)
    # load offset
    loadi 1[PM, MC], t0
    # merge them since the slow path takes the combined pointer + offset.
    addq t0, a1

    move wasmInstance, a0
    operationCall(macro() cCall4(_ipint_extern_memory_atomic_wait64) end)
    bilt r0, 0, .atomic_wait64_throw

    pushInt32(r0)
    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()

.atomic_wait64_throw:
    ipintException(OutOfBoundsMemoryAccess)

instructionLabel(_atomic_fence)
    fence

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(1)
    nextIPIntInstruction()

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
    # load offset
    loadi 1[PM, MC], t2
    addq t2, t0
    boundsAndAlignmentCheck(t0,  t3)
    addq memoryBase, t0
    loadAndPush(t0, t2)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()
end

instructionLabel(_i32_atomic_load)
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadi [mem], scratch
        else
            error
        end
        pushInt32(scratch)
    end)

instructionLabel(_i64_atomic_load)
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadq [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)

instructionLabel(_i32_atomic_load8_u)
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadb [mem], scratch
        else
            error
        end
        pushInt32(scratch)
    end)

instructionLabel(_i32_atomic_load16_u)
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadh [mem], scratch
        else
            error
        end
        pushInt32(scratch)
    end)

instructionLabel(_i64_atomic_load8_u)
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadb [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)

instructionLabel(_i64_atomic_load16_u)
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadh [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)

instructionLabel(_i64_atomic_load32_u)
    atomicLoadOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, scratch)
        if ARM64 or ARM64E or X86_64
            atomicloadi [mem], scratch
        else
            error
        end
        pushInt64(scratch)
    end)

macro weakCASLoopByte(mem, value, scratch1AndOldValue, scratch2, fn)
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
    # load offset
    loadi 1[PM, MC], t0
    addq t0, t2
    boundsAndAlignmentCheck(t2, t3)
    addq memoryBase, t2
    popAndStore(t2, t1, t0, t3)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()
end

instructionLabel(_i32_atomic_store)
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

instructionLabel(_i64_atomic_store)
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

instructionLabel(_i32_atomic_store8_u)
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

instructionLabel(_i32_atomic_store16_u)
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

instructionLabel(_i64_atomic_store8_u)
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

instructionLabel(_i64_atomic_store16_u)
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

instructionLabel(_i64_atomic_store32_u)
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


macro atomicRMWOp(boundsAndAlignmentCheck, rmw)
    # pop value
    popInt64(t1, t0)
    # pop index
    popInt32(t2, t0)
    # load offset
    loadi 1[PM, MC], t0
    addq t0, t2
    boundsAndAlignmentCheck(t2, t3)
    addq memoryBase, t2
    rmw(t2, t1, t0, t3)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()
end

instructionLabel(_i32_atomic_rmw_add)
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

instructionLabel(_i64_atomic_rmw_add)
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

instructionLabel(_i32_atomic_rmw8_add_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            atomicxchgaddb value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)

instructionLabel(_i32_atomic_rmw16_add_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            atomicxchgaddh value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)

instructionLabel(_i64_atomic_rmw8_add_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            atomicxchgaddb value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)

instructionLabel(_i64_atomic_rmw16_add_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            atomicxchgaddh value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                addi value, oldValue, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)

instructionLabel(_i64_atomic_rmw32_add_u)
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
        pushInt64(scratch1)
    end)

instructionLabel(_i32_atomic_rmw_sub)
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


instructionLabel(_i64_atomic_rmw_sub)
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

instructionLabel(_i32_atomic_rmw8_sub_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negi value
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            negi value
            atomicxchgaddb value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)

instructionLabel(_i32_atomic_rmw16_sub_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negi value
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            negi value
            atomicxchgaddh value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt32(scratch1)
    end)

instructionLabel(_i64_atomic_rmw8_sub_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negq value
            atomicxchgaddb value, [mem], scratch1
        elsif X86_64
            negq value
            atomicxchgaddb value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopByte(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)

instructionLabel(_i64_atomic_rmw16_sub_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negq value
            atomicxchgaddh value, [mem], scratch1
        elsif X86_64
            negq value
            atomicxchgaddh value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopHalf(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)

instructionLabel(_i64_atomic_rmw32_sub_u)
    atomicRMWOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, scratch1, scratch2)
        if ARM64E
            negq value
            atomicxchgaddi value, [mem], scratch1
        elsif X86_64
            negq value
            atomicxchgaddi value, [mem]
            move value, scratch1
        elsif ARM64
            weakCASLoopInt(mem, value, scratch1, scratch2, macro(value, oldValue, newValue)
                subi oldValue, value, newValue
            end)
        else
            error
        end
        pushInt64(scratch1)
    end)

instructionLabel(_i32_atomic_rmw_and)
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

instructionLabel(_i64_atomic_rmw_and)
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

instructionLabel(_i32_atomic_rmw8_and_u)
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

instructionLabel(_i32_atomic_rmw16_and_u)
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

instructionLabel(_i64_atomic_rmw8_and_u)
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

instructionLabel(_i64_atomic_rmw16_and_u)
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

instructionLabel(_i64_atomic_rmw32_and_u)
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

instructionLabel(_i32_atomic_rmw_or)
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

instructionLabel(_i64_atomic_rmw_or)
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

instructionLabel(_i32_atomic_rmw8_or_u)
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

instructionLabel(_i32_atomic_rmw16_or_u)
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

instructionLabel(_i64_atomic_rmw8_or_u)
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

instructionLabel(_i64_atomic_rmw16_or_u)
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

instructionLabel(_i64_atomic_rmw32_or_u)
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

instructionLabel(_i32_atomic_rmw_xor)
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

instructionLabel(_i64_atomic_rmw_xor)
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


instructionLabel(_i32_atomic_rmw8_xor_u)
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

instructionLabel(_i32_atomic_rmw16_xor_u)
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

instructionLabel(_i64_atomic_rmw8_xor_u)
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

instructionLabel(_i64_atomic_rmw16_xor_u)
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

instructionLabel(_i64_atomic_rmw32_xor_u)
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

instructionLabel(_i32_atomic_rmw_xchg)
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

instructionLabel(_i64_atomic_rmw_xchg)
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

instructionLabel(_i32_atomic_rmw8_xchg_u)
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

instructionLabel(_i32_atomic_rmw16_xchg_u)
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

instructionLabel(_i64_atomic_rmw8_xchg_u)
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

instructionLabel(_i64_atomic_rmw16_xchg_u)
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

instructionLabel(_i64_atomic_rmw32_xchg_u)
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

macro atomicCmpxchgOp(boundsAndAlignmentCheck, cmpxchg)
    # pop value
    popInt64(t1, t2)
    # pop expected
    popInt64(t0, t2)
    # pop index
    popInt32(t3, t2)
    # load offset
    loadi 1[PM, MC], t2
    addq t2, t3
    boundsAndAlignmentCheck(t3, t2)
    addq memoryBase, t3
    cmpxchg(t3, t1, t0, t2)

    loadb [PM, MC], t0
    advancePCByReg(t0)
    advanceMC(5)
    nextIPIntInstruction()
end

macro weakCASExchangeByte(mem, value, expected, scratch)
    if ARM64
    .loop:
        loadlinkacqb [mem], scratch
        bqneq expected, scratch, .fail
        storecondrelb scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        emit "clrex"
        move scratch, expected
    .done:
    else
        error
    end
end

macro weakCASExchangeHalf(mem, value, expected, scratch)
    if ARM64
    .loop:
        loadlinkacqh [mem], scratch
        bqneq expected, scratch, .fail
        storecondrelh scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        emit "clrex"
        move scratch, expected
    .done:
    else
        error
    end
end

macro weakCASExchangeInt(mem, value, expected, scratch)
    if ARM64
    .loop:
        loadlinkacqi [mem], scratch
        bqneq expected, scratch, .fail
        storecondreli scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        emit "clrex"
        move scratch, expected
    .done:
    else
        error
    end
end

macro weakCASExchangeQuad(mem, value, expected, scratch)
    if ARM64
    .loop:
        loadlinkacqq [mem], scratch
        bqneq expected, scratch, .fail
        storecondrelq scratch, value, [mem]
        bieq scratch, 0, .done
        jmp .loop
    .fail:
        emit "clrex"
        move scratch, expected
    .done:
    else
        error
    end
end

instructionLabel(_i32_atomic_rmw_cmpxchg)
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, expected, scratch2)
        if ARM64E or X86_64
            atomicweakcasi expected, value, [mem]
        elsif ARM64
            weakCASExchangeInt(mem, value, expected, scratch2)
        else
            error
        end
        pushInt32(expected)
    end)

instructionLabel(_i64_atomic_rmw_cmpxchg)
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck8, macro(mem, value, expected, scratch2)
        if ARM64E or X86_64
            atomicweakcasq expected, value, [mem]
        elsif ARM64
            weakCASExchangeQuad(mem, value, expected, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)

instructionLabel(_i32_atomic_rmw8_cmpxchg_u)
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, expected, scratch2)
        andq 0xff, expected
        if ARM64E or X86_64
            atomicweakcasb expected, value, [mem]
        elsif ARM64
            weakCASExchangeByte(mem, value, expected, scratch2)
        else
            error
        end
        pushInt32(expected)
    end)

instructionLabel(_i32_atomic_rmw16_cmpxchg_u)
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, expected, scratch2)
        andq 0xffff, expected
        if ARM64E or X86_64
            atomicweakcash expected, value, [mem]
        elsif ARM64
            weakCASExchangeHalf(mem, value, expected, scratch2)
        else
            error
        end
        pushInt32(expected)
    end)

instructionLabel(_i64_atomic_rmw8_cmpxchg_u)
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck1, macro(mem, value, expected, scratch2)
        andq 0xff, expected
        if ARM64E or X86_64
            atomicweakcasb expected, value, [mem]
        elsif ARM64
            weakCASExchangeByte(mem, value, expected, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)

instructionLabel(_i64_atomic_rmw16_cmpxchg_u)
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck2, macro(mem, value, expected, scratch2)
        andq 0xffff, expected
        if ARM64E or X86_64
            atomicweakcash expected, value, [mem]
        elsif ARM64
            weakCASExchangeHalf(mem, value, expected, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)

instructionLabel(_i64_atomic_rmw32_cmpxchg_u)
    atomicCmpxchgOp(ipintCheckMemoryBoundWithAlignmentCheck4, macro(mem, value, expected, scratch2)
        andq 0xffffffff, expected
        if ARM64E or X86_64
            atomicweakcasi expected, value, [mem]
        elsif ARM64
            weakCASExchangeInt(mem, value, expected, scratch2)
        else
            error
        end
        pushInt64(expected)
    end)

#######################################
## ULEB128 decoding logic for locals ##
#######################################

macro decodeULEB128(exitLabel, result)
    # result should already be the first byte.
    andq 0x7f, result
    move 7, t2 # t1 holds the shift.
.loop:
    loadb [PB, PC], t3
    andq t3, 0x7f, t1
    lshiftq t2, t1
    orq t1, result
    addq 7, t2
    advancePC(1)
    bbaeq t3, 128, .loop
    jmp exitLabel
end

slowPathLabel(_local_get)
    decodeULEB128(.ipint_local_get_post_decode, t0)

slowPathLabel(_local_set)
    decodeULEB128(.ipint_local_set_post_decode, t0)

slowPathLabel(_local_tee)
    decodeULEB128(.ipint_local_tee_post_decode, t0)

##################################
## "Out of line" logic for call ##
##################################

macro mintPop(reg)
    loadq [ws1], reg
    addq 16, ws1
end

macro mintPopF(reg)
    loadd [ws1], reg
    addq 16, ws1
end

macro mintArgDispatch()
    loadb [PM], ws0
    addq 1, PM
    andq 15, ws0
    lshiftq 6, ws0
if ARM64 or ARM64E
    pcrtoaddr _mint_begin, csr4
    addq ws0, csr4
    # csr4 = x23
    emit "br x23"
elsif X86_64
    leap (_mint_begin), csr4
    addq ws0, csr4
    # csr4 = r13
    emit "jmp *(%r13)"
end
end

macro mintRetDispatch()
    loadb [PM], ws0
    addq 1, PM
    bilt ws0, 14, .safe
    break
.safe:
    lshiftq 6, ws0
if ARM64 or ARM64E
    pcrtoaddr _mint_begin_return, csr4
    addq ws0, csr4
    # csr4 = x23
    emit "br x23"
elsif X86_64
    leap (_mint_begin_return), csr4
    addq ws0, csr4
    # csr4 = r13
    emit "jmp *(%r13)"
end
end

_ipint_call_impl:
    # 0 - 3: function index
    # 4 - 7: PC post call
    # 8 - 9: length of mint bytecode
    # 10 - : mint bytecode

    # function index
    loadi 1[PM, MC], t0

    loadb [PM, MC], t1
    advancePCByReg(t1)
    advanceMC(5)

    # Get function data
    move t0, a1
    move wasmInstance, a0
    operationCall(macro() cCall2(_ipint_extern_call) end)

.ipint_call_common:
    # wasmInstance = csr0
    # PM = csr1
    # PB = csr2
    # memoryBase = csr3
    # boundsCheckingSize = csr4

    # CANNOT throw away: entrypoint, new instance, PM
    # CAN throw away immediately: memoryBase, boundsCheckingSize, PB
    # for call: MUST preserve MC
    # for call: PB/PM (load from callee)

    # csr0 = wasmInstance, then PC
    # csr1 = PM (later PM + PB)
    # csr2 = new entrypoint
    # csr3 = new instance, then old instance
    # csr4 = temp
    # ws0 = temp

    const ipintCallSavedEntrypoint = PB
    const ipintCallNewInstance = memoryBase

    # shadow stack pointer
    const ipintCallShadowSP = ws1

    push PL, wasmInstance
    move sp, ipintCallShadowSP
    addq 16, ipintCallShadowSP
    move PC, wasmInstance

    # Free up r0, r1 to be used as argument registers
    move r0, ipintCallSavedEntrypoint
    move r1, ipintCallNewInstance

    # We'll update PM to be the value that the return metadata starts at
    addq MC, PM
    mintArgDispatch()

mintAlign(_a0)
_mint_begin:
    mintPop(a0)
    mintArgDispatch()

mintAlign(_a1)
    mintPop(a1)
    mintArgDispatch()

mintAlign(_a2)
    mintPop(a2)
    mintArgDispatch()

mintAlign(_a3)
    mintPop(a3)
    mintArgDispatch()

mintAlign(_a4)
if ARM64 or ARM64E
    mintPop(a4)
    mintArgDispatch()
else
    break
end

mintAlign(_a5)
if ARM64 or ARM64E
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
    mintPopF(fa0)
    mintArgDispatch()

mintAlign(_fa1)
    mintPopF(fa1)
    mintArgDispatch()

mintAlign(_fa2)
    mintPopF(fa2)
    mintArgDispatch()

mintAlign(_fa3)
    mintPopF(fa3)
    mintArgDispatch()

mintAlign(_stackzero)
    mintPop(ws0)
    storeq ws0, [sp]
    mintArgDispatch()

mintAlign(_stackeight)
    mintPop(ws0)
    pushQuad(ws0)
    mintArgDispatch()

mintAlign(_gap)
    subq 16, sp
    mintArgDispatch()

mintAlign(_call)
    # Set up the rest of the stack frame
    subp FirstArgumentOffset - 16, sp

    # wasmInstance = PC
    storeq wasmInstance, ThisArgumentOffset - 16[sp]

    # Swap instances
    move ipintCallNewInstance, wasmInstance

    # Set up memory
    push t2, t3
    ipintReloadMemory()
    pop t3, t2

    # Make the call
    call ipintCallSavedEntrypoint, JSEntrySlowPathPtrTag

    loadq ThisArgumentOffset - 16[sp], PB
    # Restore the stack pointer
    addp FirstArgumentOffset - 16, sp

    # Hey, look. PM hasn't been used to store anything.
    # No need to compute anything, just directly load stuff we need.
    loadh [PM], ws0  # number of stack args
    leap [sp, ws0, 8], sp

    const ipintCallSavedPL = memoryBase

    # Grab PL
    pop wasmInstance, ipintCallSavedPL

    loadh 2[PM], ws0
    lshiftq 4, ws0
    addq ws0, sp
    addq 4, PM
    mintRetDispatch()

mintAlign(_r0)
_mint_begin_return:
    pushQuad(r0)
    mintRetDispatch()

mintAlign(_r1)
    pushQuad(r1)
    mintRetDispatch()

mintAlign(_r2)
    pushQuad(t2)
    mintRetDispatch()

mintAlign(_r3)
    pushQuad(t3)
    mintRetDispatch()

mintAlign(_r4)
if ARM64 or ARM64E
    pushQuad(t4)
    mintRetDispatch()
else
    break
end

mintAlign(_r5)
if ARM64 or ARM64E
    pushQuad(t5)
    mintRetDispatch()
else
    break
end

mintAlign(_r6)
if ARM64 or ARM64E
    pushQuad(t6)
    mintRetDispatch()
else
    break
end

mintAlign(_r7)
if ARM64 or ARM64E
    pushQuad(t7)
    mintRetDispatch()
else
    break
end

mintAlign(_fr0)
    if ARM64 or ARM64E
        emit "str q0, [sp, #-16]!"
    else
        emit "sub $16, %esp"
        emit "movdqu %xmm0, (%esp)"
    end
    mintRetDispatch()

mintAlign(_fr1)
    if ARM64 or ARM64E
        emit "str q1, [sp, #-16]!"
    else
        emit "sub $16, %esp"
        emit "movdqu %xmm1, (%esp)"
    end
    mintRetDispatch()

mintAlign(_fr2)
    if ARM64 or ARM64E
        emit "str q2, [sp, #-16]!"
    else
        emit "sub $16, %esp"
        emit "movdqu %xmm2, (%esp)"
    end
    mintRetDispatch()

mintAlign(_fr3)
    if ARM64 or ARM64E
        emit "str q3, [sp, #-16]!"
    else
        emit "sub $16, %esp"
        emit "movdqu %xmm3, (%esp)"
    end
    mintRetDispatch()

mintAlign(_stack)
    # TODO
    break

mintAlign(_end)

    move PM, MC
    move PB, PC
    # Restore PL
    move ipintCallSavedPL, PL
    # Restore PB/PM
    getIPIntCallee()
    loadp Wasm::IPIntCallee::m_bytecode[ws0], PB
    loadp Wasm::IPIntCallee::m_metadata[ws0], PM
    subq PM, MC
    # Restore IB
    if ARM64 or ARM64E
        pcrtoaddr _ipint_unreachable, IB
    end
    # Restore memory
    ipintReloadMemory()
    nextIPIntInstruction()

uintAlign(_r0)
_uint_begin:
    popQuad(r0, t3)
    uintDispatch()

uintAlign(_r1)
    popQuad(r1, t3)
    uintDispatch()

uintAlign(_fr1)
    popFPR()
    uintDispatch()

uintAlign(_stack)
    break

uintAlign(_ret)
    jmp .ipint_exit

# PM = location in argumINT bytecode
# csr0 = tmp
# csr1 = dst
# csr2 = src
# csr3
# csr4 = for dispatch

# const argumINTDest = csr3
# const argumINTSrc = PB

argumINTAlign(_a0)
_argumINT_begin:
    storeq a0, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_a1)
    storeq a1, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_a2)
    storeq a2, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_a3)
    storeq a3, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_a4)
if ARM64 or ARM64E
    storeq a4, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()
else
    break
end

argumINTAlign(_a5)
if ARM64 or ARM64E
    storeq a5, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()
else
    break
end

argumINTAlign(_a6)
if ARM64 or ARM64E
    storeq a6, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()
else
    break
end

argumINTAlign(_a7)
if ARM64 or ARM64E
    storeq a7, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()
else
    break
end

argumINTAlign(_fa0)
    stored fa0, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_fa1)
    stored fa1, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_fa2)
    stored fa2, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_fa3)
    stored fa3, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_stack)
    loadq [argumINTSrc], csr0
    addq 8, argumINTSrc
    storeq csr0, [argumINTDest]
    addq 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_end)
    jmp .ipint_entry_end_local
