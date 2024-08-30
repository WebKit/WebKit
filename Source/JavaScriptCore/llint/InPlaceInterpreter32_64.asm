##########
# Macros #
##########

# Callee Save

macro saveIPIntRegisters()
    subp IPIntCalleeSaveSpaceStackAligned, sp
    store2ia PM, PB, -8[cfr]
end

macro restoreIPIntRegisters()
    load2ia -8[cfr], PM, PB
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
        subp 16, sp
        store2ia lo, hi, [sp]
    else
        break
    end
end

macro pushDouble(reg)
    if ARMv7
        subp 16, sp
        storei reg, [sp]
    else
        break
    end
end

macro popQuad(hi, lo)
    if ARMv7
        load2ia [sp], lo, hi
        addp 16, sp
    else
        break
    end
end

macro popDouble(reg)
    if ARMv7
        loadi [sp], reg
        addp 16, sp
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

# Pushes ft0 because macros
macro pushFPR()
    if ARMv7
        subp 16, sp
        stored ft0, [sp]
    else
        break
    end
end

macro pushFPR1()
    if ARMv7
        subp 16, sp
        stored ft1, [sp]
    else
        break
    end
end

macro popFPR()
    if ARMv7
        loadd [sp], ft0
        addp 16, sp
    else
        break
    end
end

macro popFPR1()
    if ARMv7
        loadd [sp], ft1
        addp 16, sp
    else
        break
    end
end

macro peekDouble(i, reg)
    if ARMv7
        loadi (i*16)[sp], reg
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

# Entry

# PM = location in argumINT bytecode
# csr1 = tmp
# t4 = dst
# t5 = src
# t6 = end
# t7 = for dispatch

const argumINTDest = t4
const argumINTSrc = t5

macro ipintEntry()
    checkStackOverflow(ws0, t6)

    # Allocate space for locals and rethrow values
    load2ia Wasm::IPIntCallee::m_localSizeToAlloc[ws0], t7, t6
    addp t6, t7
    mulp LocalSize, t7
    move sp, t6
    subp t7, sp
    move sp, t7
    loadp Wasm::IPIntCallee::m_argumINTBytecodePointer[ws0], PM

    push csr1, t4, t5

    move t7, argumINTDest
    leap FirstArgumentOffset[cfr], argumINTSrc

    argumINTDispatch()
end

macro argumINTDispatch()
    loadb [PM], csr1
    addp 1, PM
    lshiftp 6, csr1
    leap (_argumINT_begin + 1), t7
    addp csr1, t7
    emit "bx r9"
end

macro argumINTEnd()
    # zero out remaining locals
    bpeq argumINTDest, t6, .ipint_entry_finish_zero
    store2ia 0, 0, [argumINTDest]
    addp 8, argumINTDest
end

macro argumINTFinish()
    pop t5, t4, csr1
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
    loadi IPInt::BlockMetadata::newPC[PM, MC], PC
    loadi IPInt::BlockMetadata::newMC[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_loop)
    # loop
    ipintLoopOSR(1)
    loadb IPInt::InstructionLengthMetadata::length[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()

instructionLabel(_if)
    # if
    popInt32(t0)
    bpneq 0, t0, .ipint_if_taken
    loadi IPInt::IfMetadata::elsePC[PM, MC], PC
    loadi IPInt::IfMetadata::elseMC[PM, MC], MC
    nextIPIntInstruction()
.ipint_if_taken:
    # Skip LEB128
    loadb IPInt::IfMetadata::instructionLength[PM, MC], t0
    advanceMC(constexpr (sizeof(IPInt::IfMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()

instructionLabel(_else)
    # else
    # Counterintuitively, we only run this instruction if the if
    # clause is TAKEN. This is used to branch to the end of the
    # block.
    loadi IPInt::BlockMetadata::newPC[PM, MC], PC
    loadi IPInt::BlockMetadata::newMC[PM, MC], MC
    nextIPIntInstruction()

unimplementedInstruction(_try)
unimplementedInstruction(_catch)
unimplementedInstruction(_throw)
unimplementedInstruction(_rethrow)
reservedOpcode(0xa)

# PM = location in uINT bytecode
# t6 = tmp
# sp = src
# t7 = for dispatch

macro uintDispatch()
    loadb [PM], t6
    addp 1, PM
    bilt t6, 5, .safe
    break
.safe:
    lshiftp 6, t6
    leap (_uint_begin + 1), t7
    addp t6, t7
    # t7 = r9
    emit "bx r9"
end

instructionLabel(_end)
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    loadi Wasm::IPIntCallee::m_bytecodeLength[ws0], t0
    subp 1, t0
    bpeq PC, t0, .ipint_end_ret
    advancePC(1)
    nextIPIntInstruction()
.ipint_end_ret:
    ipintEpilogueOSR(10)
    addp MC, PM
    uintDispatch()

instructionLabel(_br)
    # br
    # number to pop
    loadh IPInt::branchTargetMetadata::toPop[PM, MC], t0
    # number to keep
    loadh IPInt::branchTargetMetadata::toKeep[PM, MC], t5

    # ex. pop 3 and keep 2
    #
    # +4 +3 +2 +1 sp
    # a  b  c  d  e
    # d  e
    #
    # [sp + k + numToPop] = [sp + k] for k in numToKeep-1 -> 0
    move t0, t2
    lshiftp 4, t2
    leap [sp, t2], t2

.ipint_br_poploop:
    bpeq t5, 0, .ipint_br_popend
    subp 1, t5
    move t5, t3
    lshiftp 4, t3
    load2ia [sp, t3], t0, t1
    store2ia t0, t1, [t2, t3]
    load2ia 8[sp, t3], t0, t1
    store2ia t0, t1, 8[t2, t3]
    jmp .ipint_br_poploop
.ipint_br_popend:
    loadh IPInt::branchTargetMetadata::toPop[PM, MC], t0
    lshiftp 4, t0
    leap [sp, t0], sp
    loadi IPInt::BlockMetadata::newPC[PM, MC], PC
    loadi IPInt::BlockMetadata::newMC[PM, MC], MC
    nextIPIntInstruction()

instructionLabel(_br_if)
    # pop i32
    popInt32(t0)
    bineq t0, 0, _ipint_br
    loadb IPInt::branchMetadata::instructionLength[PM, MC], t0
    advanceMC(constexpr (sizeof(IPInt::branchMetadata)))
    advancePCByReg(t0)
    nextIPIntInstruction()

instructionLabel(_br_table)
    # br_table
    popInt32(t0)
    loadi IPInt::switchMetadata::size[PM, MC], t1
    advanceMC(constexpr (sizeof(IPInt::switchMetadata)))
    bib t0, t1, .ipint_br_table_clamped
    subp t1, 1, t0
.ipint_br_table_clamped:
    move t0, t1
    lshiftp 3, t0
    lshiftp 2, t1
    addp t1, t0
    addp t0, MC
    jmp _ipint_br

instructionLabel(_return)
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    # ret
    loadi Wasm::IPIntCallee::m_bytecodeLength[ws0], PC
    loadi Wasm::IPIntCallee::m_returnMetadata[ws0], MC
    subp 1, PC
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
    popInt32(t0)
    storei t0, IPInt::callIndirectMetadata::functionRef[PM, MC]
    # Stash cfr
    storep cfr, IPInt::callIndirectMetadata::callFrame[PM, MC]

    # Get IPInt::callIndirectMetadata
    leap [PM, MC], a1
    operationCall(macro() cCall2(_ipint_extern_call_indirect) end)
    btpz r1, .ipint_call_indirect_throw
    # r0 = entrypoint
    # r1 = wasmInstance

    loadb IPInt::callIndirectMetadata::length[PM, MC], t2
    advancePCByReg(t2)
    advanceMC(constexpr (sizeof(IPInt::callIndirectMetadata)))

    jmp .ipint_call_common
.ipint_call_indirect_throw:
    jmp _wasm_throw_from_slow_path_trampoline

reservedOpcode(0x12)
reservedOpcode(0x13)
reservedOpcode(0x14)
reservedOpcode(0x15)
reservedOpcode(0x16)
reservedOpcode(0x17)
unimplementedInstruction(_delegate)
unimplementedInstruction(_catch_all)

instructionLabel(_drop)
    drop()
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_select)
    popInt32(t0)
    bieq t0, 0, .ipint_select_val2
    drop()
    advancePC(1)
    advanceMC(8)
    nextIPIntInstruction()
.ipint_select_val2:
    popQuad(t1, t0)
    drop()
    pushQuad(t1, t0)
    advancePC(1)
    advanceMC(8)
    nextIPIntInstruction()

unimplementedInstruction(_select_t)
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
    load2ia [PL, t0, LocalSize], t0, t1
    # Push to stack
    pushQuad(t1, t0)
    nextIPIntInstruction()

instructionLabel(_local_set)
    # local.set
    loadb 1[PB, PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_set_slow_path
.ipint_local_set_post_decode:
    # Pop from stack
    popQuad(t3, t2)
    # Store to locals
    store2ia t2, t3, [PL, t0, LocalSize]
    nextIPIntInstruction()

instructionLabel(_local_tee)
    # local.tee
    loadb 1[PB, PC], t0
    advancePC(2)
    bbaeq t0, 128, _ipint_local_tee_slow_path
.ipint_local_tee_post_decode:
    # Load from stack
    load2ia [sp], t3, t2
    # Store to locals
    store2ia t2, t3, [PL, t0, LocalSize]
    nextIPIntInstruction()

instructionLabel(_global_get)
    # Load pre-computed index from metadata
    loadb IPInt::globalMetadata::bindingMode[PM, MC], t2
    loadi IPInt::globalMetadata::index[PM, MC], t1
    loadp CodeBlock[cfr], t0
    loadp JSWebAssemblyInstance::m_globals[t0], t0
    lshiftp 1, t1
    load2ia [t0, t1, 8], t0, t1
    bieq t2, 0, .ipint_global_get_embedded
    load2ia [t0], t0, t1
.ipint_global_get_embedded:
    pushQuad(t1, t0)

    loadb IPInt::globalMetadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::globalMetadata)))
    nextIPIntInstruction()

instructionLabel(_global_set)
    # isRef = 1 => ref, use slowpath
    loadb IPInt::globalMetadata::isRef[PM, MC], t0
    bineq t0, 0, .ipint_global_set_refpath
    # bindingMode = 1 => portable
    loadb IPInt::globalMetadata::bindingMode[PM, MC], t5
    # get global addr
    loadp CodeBlock[cfr], t0
    loadp JSWebAssemblyInstance::m_globals[t0], t0
    # get value to store
    popQuad(t3, t2)
    # get index
    loadi IPInt::globalMetadata::index[PM, MC], t1
    lshiftp 1, t1
    bieq t5, 0, .ipint_global_set_embedded
    # portable: dereference then set
    loadp [t0, t1, 8], t0
    store2ia t2, t3, [t0]
    loadb IPInt::globalMetadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::globalMetadata)))
    nextIPIntInstruction()
.ipint_global_set_embedded:
    # embedded: set directly
    store2ia t2, t3, [t0, t1, 8]
    loadb IPInt::globalMetadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::globalMetadata)))
    nextIPIntInstruction()

.ipint_global_set_refpath:
    loadi IPInt::globalMetadata::index[PM, MC], a1
    # Pop from stack
    popQuad(a2, a3)
    operationCall(macro() cCall4(_ipint_extern_set_global_ref) end)

    loadb IPInt::globalMetadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::globalMetadata)))
    nextIPIntInstruction()

unimplementedInstruction(_table_get)
unimplementedInstruction(_table_set)
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

instructionLabel(_i32_load_mem)
    # i32.load
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
    # load memory location
    loadi [memoryBase, t0], t1
    pushInt32(t1)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_load_mem)
    # i32.load
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 8)
    # load memory location
    load2ia [memoryBase, t0], t0, t1
    pushInt64(t1, t0)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_f32_load_mem)
    # f32.load
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
    # load memory location
    loadi [memoryBase, t0], t0 # NB: can be unaligned, hence loadi, fi2f instead of loadf (VLDR)
    fi2f t0, ft0
    pushFloat32FT0()

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()
    
instructionLabel(_f64_load_mem)
    # f64.load
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 8)
    # load memory location
    load2ia [memoryBase, t0], t0, t1 # NB: can be unaligned, hence loadi, fii2d instead of loadd
    fii2d t0, t1, ft0
    pushFloat64FT0()

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i32_load8s_mem)
    # i32.load8_s
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    sxb2i t1, t1
    pushInt32(t1)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i32_load8u_mem)
    # i32.load8_u
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadb [memoryBase, t0], t1
    pushInt32(t1)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i32_load16s_mem)
    # i32.load16_s
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    sxh2i t1, t1
    pushInt32(t1)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i32_load16u_mem)
    # i32.load16_u
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 2)
    # load memory location
    loadh [memoryBase, t0], t1
    pushInt32(t1)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_load8s_mem)
    # i64.load8_s
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadb [memoryBase, t0], t0
    sxb2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_load8u_mem)
    # i64.load8_u
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadb [memoryBase, t0], t0
    move 0, t1
    pushInt64(t1, t0)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_load16s_mem)
    # i64.load16_s
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadh [memoryBase, t0], t0
    sxh2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_load16u_mem)
    # i64.load16_u
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadh [memoryBase, t0], t0
    move 0, t1
    pushInt64(t1, t0)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_load32s_mem)
    # i64.load32_s
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadi [memoryBase, t0], t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_load32u_mem)
    # i64.load8_s
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 1)
    # load memory location
    loadi [memoryBase, t0], t0
    move 0, t1
    pushInt64(t1, t0)

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i32_store_mem)
    # i32.store
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 4)
    # store at memory location
    storei t0, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_store_mem)
    # i64.store
    # peek index
    peekInt32(1, t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 8)
    # pop data
    popInt64(t1, t0)
    # drop index
    drop()
    # store at memory location
    store2ia t0, t1, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_f32_store_mem)
    # f32.store
    # pop data
    popFloat32FT0()
    # pop index
    popInt32(t0)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t0, t1, 4)
    # store at memory location
    ff2i ft0, t1 # NB: can be unaligned, hence ff2i, storei instead of storef (VSTR)
    storei t1, [memoryBase, t0]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_f64_store_mem)
    # f64.store
    # pop data
    popFloat64FT0()
    # pop index
    popInt32(t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 8)
    # store at memory location
    fd2ii ft0, t0, t1 # NB: can be unaligned, hence fd2ii, store2ia instead of stored
    store2ia t0, t1, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i32_store8_mem)
    # i32.store8
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 1)
    # store at memory location
    storeb t0, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i32_store16_mem)
    # i32.store16
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 2)
    # store at memory location
    storeh t0, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()
    
instructionLabel(_i64_store8_mem)
    # i64.store8
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 1)
    # store at memory location
    storeb t0, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_store16_mem)
    # i64.store16
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 2)
    # store at memory location
    storeh t0, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_store32_mem)
    # i64.store32
    # pop data
    popInt32(t0)
    # pop index
    popInt32(t5)
    loadi IPInt::const32Metadata::value[PM, MC], t1
    ipintWithMemory()
    ipintMaterializePtrAndCheckMemoryBound(t5, t1, 4)
    # store at memory location
    storei t0, [memoryBase, t5]

    loadb IPInt::const32Metadata::instructionLength[PM, MC], t0
    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

unimplementedInstruction(_memory_size)

instructionLabel(_memory_grow)
    popInt32(a1)
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
    loadb IPInt::InstructionLengthMetadata::length[PM, MC], t1
    bigteq t1, 2, .ipint_i32_const_slowpath
    loadb 1[PB, PC], t0
    lshifti 7, t1
    ori t1, t0
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(2)
    advanceMC(constexpr (sizeof(IPInt::InstructionLengthMetadata)))
    nextIPIntInstruction()
.ipint_i32_const_slowpath:
    # Load pre-computed value from metadata
    loadi IPInt::const32Metadata::value[PM, MC], t0
    # Push to stack
    pushInt32(t0)

    advancePCByReg(t1)
    advanceMC(constexpr (sizeof(IPInt::const32Metadata)))
    nextIPIntInstruction()

instructionLabel(_i64_const)
    # i64.const
    # Load pre-computed value from metadata
    load2ia IPInt::const64Metadata::value[PM, MC], t0, t1
    # Push to stack
    pushInt64(t1, t0)
    loadb IPInt::const64Metadata::instructionLength[PM, MC], t0

    advancePCByReg(t0)
    advanceMC(constexpr (sizeof(IPInt::const64Metadata)))
    nextIPIntInstruction()

instructionLabel(_f32_const)
    # f32.const
    # Load pre-computed value from metadata
    loadi 1[PB, PC], t0 # NB: can be unaligned, hence loadi, fi2f instead of loadf (VLDR)
    fi2f t0, ft0
    pushFloat32FT0()

    advancePC(5)
    nextIPIntInstruction()

instructionLabel(_f64_const)
    # f64.const
    # Load pre-computed value from metadata
    load2ia 1[PB, PC], t0, t1 # NB: can be unaligned, hence loadi, fii2d instead of loadd
    fii2d t0, t1, ft0
    pushFloat64FT0()

    advancePC(9)
    nextIPIntInstruction()

    ###############################
    # 0x45 - 0x4f: i32 comparison #
    ###############################

instructionLabel(_i32_eqz)
    # i32.eqz
    popInt32(t0)
    cieq t0, 0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_eq)
    # i32.eq
    popInt32(t1)
    popInt32(t0)
    cieq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ne)
    # i32.ne
    popInt32(t1)
    popInt32(t0)
    cineq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_lt_s)
    # i32.lt_s
    popInt32(t1)
    popInt32(t0)
    cilt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_lt_u)
    # i32.lt_u
    popInt32(t1)
    popInt32(t0)
    cib t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_gt_s)
    # i32.gt_s
    popInt32(t1)
    popInt32(t0)
    cigt t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_gt_u)
    # i32.gt_u
    popInt32(t1)
    popInt32(t0)
    cia t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_le_s)
    # i32.le_s
    popInt32(t1)
    popInt32(t0)
    cilteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_le_u)
    # i32.le_u
    popInt32(t1)
    popInt32(t0)
    cibeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ge_s)
    # i32.ge_s
    popInt32(t1)
    popInt32(t0)
    cigteq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ge_u)
    # i32.ge_u
    popInt32(t1)
    popInt32(t0)
    ciaeq t0, t1, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x50 - 0x5a: i64 comparison #
    ###############################

instructionLabel(_i64_eqz)
    # i64.eqz
    popInt64(t1, t0)
    move 0, t2
    btinz t1, .ipint_i64_eqz_return
    cieq t0, 0, t2
.ipint_i64_eqz_return:
    pushInt32(t2)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_eq)
    # i64.eq
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 0, t5
    bineq t1, t3, .ipint_i64_eq_return
    cieq t0, t2, t5
.ipint_i64_eq_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ne)
    # i64.ne
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bineq t1, t3, .ipint_i64_ne_return
    cineq t0, t2, t5
.ipint_i64_ne_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_lt_s)
    # i64.lt_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bilt t1, t3, .ipint_i64_lt_s_return
    move 0, t5
    bigt t1, t3, .ipint_i64_lt_s_return
    cib t0, t2, t5
.ipint_i64_lt_s_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_lt_u)
    # i64.lt_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bib t1, t3, .ipint_i64_lt_u_return
    move 0, t5
    bia t1, t3, .ipint_i64_lt_u_return
    cib t0, t2, t5
.ipint_i64_lt_u_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_gt_s)
    # i64.gt_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bigt t1, t3, .ipint_i64_gt_s_return
    move 0, t5
    bilt t1, t3, .ipint_i64_gt_s_return
    cia t0, t2, t5
.ipint_i64_gt_s_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_gt_u)
    # i64.gt_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bia t1, t3, .ipint_i64_gt_u_return
    move 0, t5
    bib t1, t3, .ipint_i64_gt_u_return
    cia t0, t2, t5
.ipint_i64_gt_u_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_le_s)
    # i64.le_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bilt t1, t3, .ipint_i64_le_s_return
    move 0, t5
    bigt t1, t3, .ipint_i64_le_s_return
    cibeq t0, t2, t5
.ipint_i64_le_s_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_le_u)
    # i64.le_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bib t1, t3, .ipint_i64_le_u_return
    move 0, t5
    bia t1, t3, .ipint_i64_le_u_return
    cibeq t0, t2, t5
.ipint_i64_le_u_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ge_s)
    # i64.ge_s
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 1, t5
    bigt t1, t3, .ipint_i64_ge_s_return
    move 0, t5
    bilt t1, t3, .ipint_i64_ge_s_return
    ciaeq t0, t2, t5
.ipint_i64_ge_s_return:
    pushInt32(t5)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_ge_u)
    # i64.ge_u
    popInt64(t3, t2)
    popInt64(t1, t0)
    move 0, t5
    bib t1, t3, .ipint_i64_ge_u_return
    move 1, t5
    bia t1, t3, .ipint_i64_ge_u_return
    ciaeq t0, t2, t5
.ipint_i64_ge_u_return:
    pushInt32(t5)
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
    popInt32(t0)
    lzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_ctz)
    # i32.ctz
    popInt32(t0)
    tzcnti t0, t1
    pushInt32(t1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_popcnt)
    # i32.popcnt
    popInt32(t1)
    operationCall(macro() cCall2(_slow_path_wasm_popcount) end)
    pushInt32(r1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_add)
    # i32.add
    popInt32(t1)
    popInt32(t0)
    addi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_sub)
    # i32.sub
    popInt32(t1)
    popInt32(t0)
    subi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_mul)
    # i32.mul
    popInt32(t1)
    popInt32(t0)
    muli t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_div_s)
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

instructionLabel(_i32_div_u)
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

instructionLabel(_i32_rem_s)
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

instructionLabel(_i32_rem_u)
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

instructionLabel(_i32_and)
    # i32.and
    popInt32(t1)
    popInt32(t0)
    andi t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_or)
    # i32.or
    popInt32(t1)
    popInt32(t0)
    ori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_xor)
    # i32.xor
    popInt32(t1)
    popInt32(t0)
    xori t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shl)
    # i32.shl
    popInt32(t1)
    popInt32(t0)
    lshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shr_s)
    # i32.shr_s
    popInt32(t1)
    popInt32(t0)
    rshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_shr_u)
    # i32.shr_u
    popInt32(t1)
    popInt32(t0)
    urshifti t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_rotl)
    # i32.rotl
    popInt32(t1)
    popInt32(t0)
    lrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_rotr)
    # i32.rotr
    popInt32(t1)
    popInt32(t0)
    rrotatei t1, t0
    pushInt32(t0)

    advancePC(1)
    nextIPIntInstruction()

    ###############################
    # 0x79 - 0x8a: i64 operations #
    ###############################

instructionLabel(_i64_clz)
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

instructionLabel(_i64_ctz)
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

instructionLabel(_i64_popcnt)
    # i64.popcnt
    popInt64(t3, t2)
    operationCall(macro() cCall2(_slow_path_wasm_popcountll) end)
    move 0, t0
    pushInt64(t0, r1)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_add)
    # i64.add
    popInt64(t3, t2)
    popInt64(t1, t0)
    addis t2, t0
    adci  t3, t1
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_sub)
    # i64.sub
    popInt64(t3, t2)
    popInt64(t1, t0)
    subis t2, t0
    sbci  t3, t1
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_mul)
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

instructionLabel(_i64_div_s)
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

instructionLabel(_i64_div_u)
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

instructionLabel(_i64_rem_s)
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

instructionLabel(_i64_rem_u)
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

instructionLabel(_i64_and)
    # i64.and
    popInt64(t3, t2)
    popInt64(t1, t0)
    andi t3, t1
    andi t2, t0
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_or)
    # i64.or
    popInt64(t3, t2)
    popInt64(t1, t0)
    ori t3, t1
    ori t2, t0
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_xor)
    # i64.xor
    popInt64(t3, t2)
    popInt64(t1, t0)
    xori t3, t1
    xori t2, t0
    pushInt64(t1, t0)

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_shl)
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

instructionLabel(_i64_shr_s)
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

instructionLabel(_i64_shr_u)
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

instructionLabel(_i64_rotl)
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

    move 32, t5
    subi t2, t5
    urshifti t0, t5, t3
    urshifti t1, t5, t5
    lshifti t2, t0
    lshifti t2, t1
    ori t5, t0
    ori t3, t1

.ipint_i64_rotl_return:
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_rotr)
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

    move 32, t5
    subi t2, t5
    lshifti t0, t5, t3
    lshifti t1, t5, t5
    urshifti t2, t0
    urshifti t2, t1
    ori t5, t0
    ori t3, t1

.ipint_i64_rotr_return:
    pushInt64(t1, t0)
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
    functionCall(macro () cCall2(_ceilFloat) end)
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_floor)
    # f32.floor
    popFloat32FT0()
    functionCall(macro () cCall2(_floorFloat) end)
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_trunc)
    # f32.trunc
    popFloat32FT0()
    functionCall(macro () cCall2(_truncFloat) end)
    pushFloat32FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f32_nearest)
    # f32.nearest
    popFloat32FT0()
    functionCall(macro () cCall2(_f32_nearest) end)
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
    functionCall(macro () cCall2(_ceilDouble) end)
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_floor)
    # f64.floor
    popFloat64FT0()
    functionCall(macro () cCall2(_floorDouble) end)
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_trunc)
    # f64.trunc
    popFloat64FT0()
    functionCall(macro () cCall2(_truncDouble) end)
    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_f64_nearest)
    # f64.nearest
    popFloat64FT0()
    functionCall(macro () cCall2(_f64_nearest) end)
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

    fd2ii ft1, t2, t3
    fd2ii ft0, t0, t1
    andi 0x7fffffff, t1
    andi 0x80000000, t3
    ori t3, t1

    pushFloat64FT0()

    advancePC(1)
    nextIPIntInstruction()

    ############################
    # 0xa7 - 0xc4: conversions #
    ############################

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

instructionLabel(_i32_reinterpret_f32)
    popFloat32FT0()
    ff2i ft0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_reinterpret_f64)
    popFloat64FT0()
    fd2ii ft0, t0, t1
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

unimplementedInstruction(_f32_reinterpret_i32)
unimplementedInstruction(_f64_reinterpret_i64)

instructionLabel(_i32_extend8_s)
    # i32.extend8_s
    popInt32(t0)
    sxb2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i32_extend16_s)
    # i32.extend8_s
    popInt32(t0)
    sxh2i t0, t0
    pushInt32(t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend8_s)
    # i64.extend8_s
    popInt32(t0)
    sxb2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend16_s)
    # i64.extend8_s
    popInt32(t0)
    sxh2i t0, t0
    rshifti t0, 31, t1
    pushInt64(t1, t0)
    advancePC(1)
    nextIPIntInstruction()

instructionLabel(_i64_extend32_s)
    # i64.extend8_s
    popInt32(t0)
    rshifti t0, 31, t1
    pushInt64(t1, t0)
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
    ## 0xFC instructions ##
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

slowPathLabel(_local_get)
    break

slowPathLabel(_local_set)
    break

slowPathLabel(_local_tee)
    break

##################################
## "Out of line" logic for call ##
##################################

# PM = location in mintINT bytecode
# csr1 = tmp
# sp = dst
# t5 = src
# t7 = for dispatch

macro mintPop(hi, lo)
    load2ia [t5], lo, hi
    addp 16, t5
end

macro mintPopF(reg)
    loadd [t5], reg
    addp 16, t5
end

macro mintArgDispatch()
    loadb [PM], csr1
    addp 1, PM
    andp 15, csr1
    lshiftp 6, csr1
    leap (_mint_begin + 1), t7
    addp csr1, t7
    # t7 = r9
    emit "bx r9"
end

macro mintRetDispatch()
    loadb [PM], csr1
    addp 1, PM
    bilt csr1, 14, .safe
    break
.safe:
    lshiftp 6, csr1
    leap (_mint_begin_return + 1), t7
    addp csr1, t7
    # t7 = r9
    emit "bx r9"
end

_ipint_call_impl:
    loadb IPInt::callMetadata::length[PM, MC], t0
    advancePCByReg(t0)

    # Get IPInt::callMetadata
    leap [PM, MC], a1
    advanceMC(constexpr (sizeof(IPInt::callMetadata)))
    operationCall(macro() cCall2(_ipint_extern_call) end)
    # r0 = IPInt::calleeMetadata*

.ipint_call_common:
    # CANNOT throw away: entrypoint, PM
    # CAN throw away immediately: PB
    # for call: MUST preserve MC
    # for call: PB/PM (load from callee)

    const ipintCallSavedCallee = MC

    # shadow stack pointer
    const ipintCallShadowSP = t5

    move sp, ipintCallShadowSP

    # We'll update PM to be the value that the return metadata starts at
    addp MC, PM
    
    # Save PL
    push PL

    # Free up r0 to be used as argument register
    move r0, ipintCallSavedCallee

    # Align stack (not needed because we pushed PL)
    # const ipintCallStackStackAlign = CallFrameAlignSlots * SlotSize
    # subp ipintCallStackStackAlign, sp

    mintArgDispatch()

mintAlign(_a0)
_mint_begin:
    mintPop(a1, a0)
    mintArgDispatch()

mintAlign(_a1)
    break

mintAlign(_a2)
    mintPop(a3, a2)
    mintArgDispatch()

mintAlign(_a3)
    break

mintAlign(_a4)
    break

mintAlign(_a5)
    break

mintAlign(_a6)
    break

mintAlign(_a7)
    break

mintAlign(_fa0)
    mintPopF(ft0)
    mintArgDispatch()

mintAlign(_fa1)
    mintPopF(ft1)
    mintArgDispatch()

mintAlign(_fa2)
    break

mintAlign(_fa3)
    break

mintAlign(_stackzero)
    break

mintAlign(_stackeight)
    break

mintAlign(_gap)
    break

mintAlign(_call)
    # Save PM (conflicts with wasmInstance)
    const ipintCallSavedPM = PB
    move PM, ipintCallSavedPM

    # Set up the rest of the stack frame
    subp FirstArgumentOffset - CallerFrameAndPCSize, sp

    storep PC, ThisArgumentOffset - CallerFrameAndPCSize[sp]

    # Set up callee slot
    loadp IPInt::calleeMetadata::boxedCallee[ipintCallSavedCallee], PC
    storep PC, Callee - CallerFrameAndPCSize[sp]

    # Swap instances
    loadp IPInt::calleeMetadata::instance[ipintCallSavedCallee], wasmInstance

    # Make the call
    loadp IPInt::calleeMetadata::entrypoint[ipintCallSavedCallee], ipintCallSavedCallee
    call ipintCallSavedCallee, JSEntrySlowPathPtrTag

    loadp ThisArgumentOffset - CallerFrameAndPCSize[sp], PC
    # Restore the stack pointer
    addp FirstArgumentOffset - CallerFrameAndPCSize, sp
    #addp ipintCallStackStackAlign, sp

    move ipintCallSavedPM, PM
    # Hey, look. PM hasn't been used to store anything.
    # No need to compute anything, just directly load stuff we need.
    loadh IPInt::callReturnMetadata::stackSlots[PM], t5  # number of stack args
    leap [sp, t5, 8], sp

    const ipintCallSavedPL = MC

    # Grab PL
    pop ipintCallSavedPL

    # Adjust sp to pop off arguments consumed
    # (mint popped off ipintCallShadowSP)
    loadh IPInt::callReturnMetadata::argumentCount[PM], t5
    lshiftp 4, t5
    addp t5, sp
    addp constexpr (sizeof(IPInt::callReturnMetadata)), PM
    mintRetDispatch()

mintAlign(_r0)
_mint_begin_return:
    pushQuad(r1, r0)
    mintRetDispatch()

mintAlign(_r1)
    break

mintAlign(_r2)
    break

mintAlign(_r3)
    break

mintAlign(_r4)
    break

mintAlign(_r5)
    break

mintAlign(_r6)
    break

mintAlign(_r7)
    break

mintAlign(_fr0)
    pushFPR()
    mintRetDispatch()

mintAlign(_fr1)
    break

mintAlign(_fr2)
    break

mintAlign(_fr3)
    break

mintAlign(_stack)
    break

mintAlign(_end)
    # Stash away PM for later MC calculation
    # (MC gets clobbered by getIPIntCallee())
    push PM
    # Restore PL
    move ipintCallSavedPL, PL
    # Restore PB/PM
    getIPIntCallee()
    loadp Wasm::IPIntCallee::m_bytecode[ws0], PB
    loadp Wasm::IPIntCallee::m_metadata[ws0], PM
    pop MC
    subp PM, MC
    # Restore memory
    ipintReloadMemory()
    nextIPIntInstruction()

uintAlign(_r0)
_uint_begin:
    popQuad(r1, r0)
    uintDispatch()

uintAlign(_r1)
    break

uintAlign(_fr1)
    popFPR()
    uintDispatch()

uintAlign(_stack)
    break

uintAlign(_ret)
    jmp .ipint_exit

# PM = location in argumINT bytecode
# t5 = tmp
# t6 = dst
# t7 = src
# cfr
# r12 = for dispatch

# const argumINTDest = t6
# const argumINTSrc = t7

argumINTAlign(_a0)
_argumINT_begin:
    storei a0, [argumINTDest]
    storei a1, 4[argumINTDest]
    addp 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_a1)
    break

argumINTAlign(_a2)
    storei a2, [argumINTDest]
    storei a3, 4[argumINTDest]
    addp 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_a3)
    break

argumINTAlign(_a4)
    break

argumINTAlign(_a5)
    break

argumINTAlign(_a6)
    break

argumINTAlign(_a7)
    break

argumINTAlign(_fa0)
    stored fa0, [argumINTDest]
    addp 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_fa1)
    stored fa1, [argumINTDest]
    addp 8, argumINTDest
    argumINTDispatch()

argumINTAlign(_fa2)
    break

argumINTAlign(_fa3)
    break

argumINTAlign(_stack)
    break

argumINTAlign(_end)
    jmp .ipint_entry_end_local
