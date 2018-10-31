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


# Utilities
macro nextInstruction()
    loadb [PC], t0
    leap _g_opcodeMap, t1
    jmp [t1, t0, 4], BytecodePtrTag
end

macro nextInstructionWide()
    loadi 1[PC], t0
    leap _g_opcodeMapWide, t1
    jmp [t1, t0, 4], BytecodePtrTag
end

macro getuOperandNarrow(op, field, dst)
    loadb constexpr %op%_%field%_index[PC], dst
end

macro getOperandNarrow(op, field, dst)
    loadbsp constexpr %op%_%field%_index[PC], dst
end

macro getuOperandWide(op, field, dst)
    loadi constexpr %op%_%field%_index * 4 + 1[PC], dst
end

macro getOperandWide(op, field, dst)
    loadis constexpr %op%_%field%_index * 4 + 1[PC], dst
end

macro makeReturn(get, dispatch, fn)
    fn(macro(tag, payload)
        move tag, t5
        move payload, t3
        get(dst, t2)
        storei t5, TagOffset[cfr, t2, 8]
        storei t3, PayloadOffset[cfr, t2, 8]
        dispatch()
    end)
end

macro makeReturnProfiled(op, get, metadata, dispatch, fn)
    fn(macro (tag, payload)
        move tag, t1
        move payload, t0

        metadata(t5, t2)
        valueProfile(op, t5, t1, t0)
        get(dst, t2)
        storei t1, TagOffset[cfr, t2, 8]
        storei t0, PayloadOffset[cfr, t2, 8]
        dispatch()
    end)
end


macro dispatchAfterCall(size, op, dispatch)
    loadi ArgumentCount + TagOffset[cfr], PC
    get(size, op, dst, t3)
    storei r1, TagOffset[cfr, t3, 8]
    storei r0, PayloadOffset[cfr, t3, 8]
    metadata(size, op, t2, t3)
    valueProfile(op, t2, r1, r0)
    dispatch()
end

macro cCall2(function)
    if ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
        call function
    elsif X86 or X86_WIN
        subp 8, sp
        push a1
        push a0
        call function
        addp 16, sp
    elsif C_LOOP
        cloopCallSlowPath function, a0, a1
    else
        error
    end
end

macro cCall2Void(function)
    if C_LOOP
        cloopCallSlowPathVoid function, a0, a1
    else
        cCall2(function)
    end
end

macro cCall4(function)
    if ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
        call function
    elsif X86 or X86_WIN
        push a3
        push a2
        push a1
        push a0
        call function
        addp 16, sp
    elsif C_LOOP
        error
    else
        error
    end
end

macro callSlowPath(slowPath)
    move cfr, a0
    move PC, a1
    cCall2(slowPath)
    move r0, PC
end

macro doVMEntry(makeCall)
    functionPrologue()
    pushCalleeSaves()

    # x86 needs to load arguments from the stack
    if X86 or X86_WIN
        loadp 16[cfr], a2
        loadp 12[cfr], a1
        loadp 8[cfr], a0
    end

    const entry = a0
    const vm = a1
    const protoCallFrame = a2

    # We are using t3, t4 and t5 as temporaries through the function.
    # Since we have the guarantee that tX != aY when X != Y, we are safe from
    # aliasing problems with our arguments.

    if ARMv7
        vmEntryRecord(cfr, t3)
        move t3, sp
    else
        vmEntryRecord(cfr, sp)
    end

    storep vm, VMEntryRecord::m_vm[sp]
    loadp VM::topCallFrame[vm], t4
    storep t4, VMEntryRecord::m_prevTopCallFrame[sp]
    loadp VM::topEntryFrame[vm], t4
    storep t4, VMEntryRecord::m_prevTopEntryFrame[sp]
    loadp ProtoCallFrame::calleeValue[protoCallFrame], t4
    storep t4, VMEntryRecord::m_callee[sp]

    # Align stack pointer
    if X86_WIN or MIPS
        addp CallFrameAlignSlots * SlotSize, sp, t3
        andp ~StackAlignmentMask, t3
        subp t3, CallFrameAlignSlots * SlotSize, sp
    elsif ARM or ARMv7 or ARMv7_TRADITIONAL
        addp CallFrameAlignSlots * SlotSize, sp, t3
        clrbp t3, StackAlignmentMask, t3
        if ARMv7
            subp t3, CallFrameAlignSlots * SlotSize, t3
            move t3, sp
        else
            subp t3, CallFrameAlignSlots * SlotSize, sp
        end
    end

    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], t4
    addp CallFrameHeaderSlots, t4, t4
    lshiftp 3, t4
    subp sp, t4, t3
    bpa t3, sp, .throwStackOverflow

    # Ensure that we have enough additional stack capacity for the incoming args,
    # and the frame for the JS code we're executing. We need to do this check
    # before we start copying the args from the protoCallFrame below.
    if C_LOOP
        bpaeq t3, VM::m_cloopStackLimit[vm], .stackHeightOK
        move entry, t4
        move vm, t5
        cloopCallSlowPath _llint_stack_check_at_vm_entry, vm, t3
        bpeq t0, 0, .stackCheckFailed
        move t4, entry
        move t5, vm
        jmp .stackHeightOK

.stackCheckFailed:
        move t4, entry
        move t5, vm
        jmp .throwStackOverflow
    else
        bpb t3, VM::m_softStackLimit[vm], .throwStackOverflow
    end

.stackHeightOK:
    move t3, sp
    move 4, t3

.copyHeaderLoop:
    subi 1, t3
    loadi TagOffset[protoCallFrame, t3, 8], t5
    storei t5, TagOffset + CodeBlock[sp, t3, 8]
    loadi PayloadOffset[protoCallFrame, t3, 8], t5
    storei t5, PayloadOffset + CodeBlock[sp, t3, 8]
    btinz t3, .copyHeaderLoop

    loadi PayloadOffset + ProtoCallFrame::argCountAndCodeOriginValue[protoCallFrame], t4
    subi 1, t4
    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], t5
    subi 1, t5

    bieq t4, t5, .copyArgs
.fillExtraArgsLoop:
    subi 1, t5
    storei UndefinedTag, ThisArgumentOffset + 8 + TagOffset[sp, t5, 8]
    storei 0, ThisArgumentOffset + 8 + PayloadOffset[sp, t5, 8]
    bineq t4, t5, .fillExtraArgsLoop

.copyArgs:
    loadp ProtoCallFrame::args[protoCallFrame], t3

.copyArgsLoop:
    btiz t4, .copyArgsDone
    subi 1, t4
    loadi TagOffset[t3, t4, 8], t5
    storei t5, ThisArgumentOffset + 8 + TagOffset[sp, t4, 8]
    loadi PayloadOffset[t3, t4, 8], t5
    storei t5, ThisArgumentOffset + 8 + PayloadOffset[sp, t4, 8]
    jmp .copyArgsLoop

.copyArgsDone:
    storep sp, VM::topCallFrame[vm]
    storep cfr, VM::topEntryFrame[vm]

    makeCall(entry, t3, t4)

    if ARMv7
        vmEntryRecord(cfr, t3)
        move t3, sp
    else
        vmEntryRecord(cfr, sp)
    end

    loadp VMEntryRecord::m_vm[sp], t5
    loadp VMEntryRecord::m_prevTopCallFrame[sp], t4
    storep t4, VM::topCallFrame[t5]
    loadp VMEntryRecord::m_prevTopEntryFrame[sp], t4
    storep t4, VM::topEntryFrame[t5]

    if ARMv7
        subp cfr, CalleeRegisterSaveSize, t5
        move t5, sp
    else
        subp cfr, CalleeRegisterSaveSize, sp
    end

    popCalleeSaves()
    functionEpilogue()
    ret

.throwStackOverflow:
    subp 8, sp # Align stack for cCall2() to make a call.
    move vm, a0
    move protoCallFrame, a1
    cCall2(_llint_throw_stack_overflow_error)

    if ARMv7
        vmEntryRecord(cfr, t3)
        move t3, sp
    else
        vmEntryRecord(cfr, sp)
    end

    loadp VMEntryRecord::m_vm[sp], t5
    loadp VMEntryRecord::m_prevTopCallFrame[sp], t4
    storep t4, VM::topCallFrame[t5]
    loadp VMEntryRecord::m_prevTopEntryFrame[sp], t4
    storep t4, VM::topEntryFrame[t5]

    if ARMv7
        subp cfr, CalleeRegisterSaveSize, t5
        move t5, sp
    else
        subp cfr, CalleeRegisterSaveSize, sp
    end

    popCalleeSaves()
    functionEpilogue()
    ret
end

macro makeJavaScriptCall(entry, temp, unused)
    addp CallerFrameAndPCSize, sp
    checkStackPointerAlignment(temp, 0xbad0dc02)
    if C_LOOP
        cloopCallJSFunction entry
    else
        call entry
    end
    checkStackPointerAlignment(temp, 0xbad0dc03)
    subp CallerFrameAndPCSize, sp
end

macro makeHostFunctionCall(entry, temp1, temp2)
    move entry, temp1
    storep cfr, [sp]
    if C_LOOP
        move sp, a0
        storep lr, PtrSize[sp]
        cloopCallNative temp1
    elsif X86 or X86_WIN
        # Put callee frame pointer on stack as arg0, also put it in ecx for "fastcall" targets
        move 0, temp2
        move temp2, 4[sp] # put 0 in ReturnPC
        move sp, a0 # a0 is ecx
        push temp2 # Push dummy arg1
        push a0
        call temp1
        addp 8, sp
    else
        move sp, a0
        call temp1
    end
end

_handleUncaughtException:
    loadp Callee + PayloadOffset[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)
    storep 0, VM::callFrameForCatch[t3]

    loadp VM::topEntryFrame[t3], cfr
    if ARMv7
        vmEntryRecord(cfr, t3)
        move t3, sp
    else
        vmEntryRecord(cfr, sp)
    end

    loadp VMEntryRecord::m_vm[sp], t3
    loadp VMEntryRecord::m_prevTopCallFrame[sp], t5
    storep t5, VM::topCallFrame[t3]
    loadp VMEntryRecord::m_prevTopEntryFrame[sp], t5
    storep t5, VM::topEntryFrame[t3]

    if ARMv7
        subp cfr, CalleeRegisterSaveSize, t3
        move t3, sp
    else
        subp cfr, CalleeRegisterSaveSize, sp
    end

    popCalleeSaves()
    functionEpilogue()
    ret

macro doReturnFromHostFunction(extraStackSpace)
    functionEpilogue(extraStackSpace)
    ret
end

# Debugging operation if you'd like to print an operand in the instruction stream. fromWhere
# should be an immediate integer - any integer you like; use it to identify the place you're
# debugging from. operand should likewise be an immediate, and should identify the operand
# in the instruction stream you'd like to print out.
macro traceOperand(fromWhere, operand)
    move fromWhere, a2
    move operand, a3
    move cfr, a0
    move PC, a1
    cCall4(_llint_trace_operand)
    move r0, PC
    move r1, cfr
end

# Debugging operation if you'd like to print the value of an operand in the instruction
# stream. Same as traceOperand(), but assumes that the operand is a register, and prints its
# value.
macro traceValue(fromWhere, operand)
    move fromWhere, a2
    move operand, a3
    move cfr, a0
    move PC, a1
    cCall4(_llint_trace_value)
    move r0, PC
    move r1, cfr
end

# Call a slowPath for call opcodes.
macro callCallSlowPath(slowPath, action)
    storep PC, ArgumentCount + TagOffset[cfr]
    move cfr, a0
    move PC, a1
    cCall2(slowPath)
    action(r0, r1)
end

macro callTrapHandler(throwHandler)
    storei PC, ArgumentCount + TagOffset[cfr]
    move cfr, a0
    move PC, a1
    cCall2(_llint_slow_path_handle_traps)
    btpnz r0, throwHandler
    loadi ArgumentCount + TagOffset[cfr], PC
end

macro checkSwitchToJITForLoop()
    checkSwitchToJIT(
        1,
        macro ()
            storei PC, ArgumentCount + TagOffset[cfr]
            move cfr, a0
            move PC, a1
            cCall2(_llint_loop_osr)
            btpz r0, .recover
            move r1, sp
            jmp r0
        .recover:
            loadi ArgumentCount + TagOffset[cfr], PC
        end)
end

macro loadVariable(get, operand, index, tag, payload)
    get(operand, index)
    loadi TagOffset[cfr, index, 8], tag
    loadi PayloadOffset[cfr, index, 8], payload
end

# Index, tag, and payload must be different registers. Index is not
# changed.
macro loadConstantOrVariable(size, index, tag, payload)
    size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide, macro (FirstConstantRegisterIndex)
        bigteq index, FirstConstantRegisterIndex, .constant
        loadi TagOffset[cfr, index, 8], tag
        loadi PayloadOffset[cfr, index, 8], payload
        jmp .done
    .constant:
        loadp CodeBlock[cfr], payload
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[payload], payload
        subp FirstConstantRegisterIndex, index
        loadp TagOffset[payload, index, 8], tag
        loadp PayloadOffset[payload, index, 8], payload
    .done:
    end)
end

macro loadConstantOrVariableTag(size, index, tag)
    size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide, macro (FirstConstantRegisterIndex)
        bigteq index, FirstConstantRegisterIndex, .constant
        loadi TagOffset[cfr, index, 8], tag
        jmp .done
    .constant:
        loadp CodeBlock[cfr], tag
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[tag], tag
        subp FirstConstantRegisterIndex, index
        loadp TagOffset[tag, index, 8], tag
    .done:
    end)
end

# Index and payload may be the same register. Index may be clobbered.
macro loadConstantOrVariable2Reg(size, index, tag, payload)
    size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide, macro (FirstConstantRegisterIndex)
        bigteq index, FirstConstantRegisterIndex, .constant
        loadi TagOffset[cfr, index, 8], tag
        loadi PayloadOffset[cfr, index, 8], payload
        jmp .done
    .constant:
        loadp CodeBlock[cfr], tag
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[tag], tag
        subp FirstConstantRegisterIndex, index
        lshifti 3, index
        addp index, tag
        loadp PayloadOffset[tag], payload
        loadp TagOffset[tag], tag
    .done:
    end)
end

macro loadConstantOrVariablePayloadTagCustom(size, index, tagCheck, payload)
    size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide, macro (FirstConstantRegisterIndex)
        bigteq index, FirstConstantRegisterIndex, .constant
        tagCheck(TagOffset[cfr, index, 8])
        loadi PayloadOffset[cfr, index, 8], payload
        jmp .done
    .constant:
        loadp CodeBlock[cfr], payload
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[payload], payload
        subp FirstConstantRegisterIndex, index
        tagCheck(TagOffset[payload, index, 8])
        loadp PayloadOffset[payload, index, 8], payload
    .done:
    end)
end

# Index and payload must be different registers. Index is not mutated. Use
# this if you know what the tag of the variable should be. Doing the tag
# test as part of loading the variable reduces register use, but may not
# be faster than doing loadConstantOrVariable followed by a branch on the
# tag.
macro loadConstantOrVariablePayload(size, index, expectedTag, payload, slow)
    loadConstantOrVariablePayloadTagCustom(
        size,
        index,
        macro (actualTag) bineq actualTag, expectedTag, slow end,
        payload)
end

macro loadConstantOrVariablePayloadUnchecked(size, index, payload)
    loadConstantOrVariablePayloadTagCustom(
        size,
        index,
        macro (actualTag) end,
        payload)
end

macro writeBarrierOnOperand(size, get, cellOperand)
    get(cellOperand, t1)
    loadConstantOrVariablePayload(size, t1, CellTag, t2, .writeBarrierDone)
    skipIfIsRememberedOrInEden(
        t2, 
        macro()
            push cfr, PC
            # We make two extra slots because cCall2 will poke.
            subp 8, sp
            move t2, a1 # t2 can be a0 on x86
            move cfr, a0
            cCall2Void(_llint_write_barrier_slow)
            addp 8, sp
            pop PC, cfr
        end)
.writeBarrierDone:
end

macro writeBarrierOnOperands(size, get, cellOperand, valueOperand)
    get(valueOperand, t1)
    loadConstantOrVariableTag(size, t1, t0)
    bineq t0, CellTag, .writeBarrierDone

    writeBarrierOnOperand(size, get, cellOperand)
.writeBarrierDone:
end

macro writeBarrierOnGlobal(size, get, valueOperand, loadHelper)
    get(valueOperand, t1)
    loadConstantOrVariableTag(size, t1, t0)
    bineq t0, CellTag, .writeBarrierDone

    loadHelper(t3)

    skipIfIsRememberedOrInEden(
        t3,
        macro()
            push cfr, PC
            # We make two extra slots because cCall2 will poke.
            subp 8, sp
            move cfr, a0
            move t3, a1
            cCall2Void(_llint_write_barrier_slow)
            addp 8, sp
            pop PC, cfr
        end)
.writeBarrierDone:
end

macro writeBarrierOnGlobalObject(size, get, valueOperand)
    writeBarrierOnGlobal(size, get, valueOperand,
        macro(registerToStoreGlobal)
            loadp CodeBlock[cfr], registerToStoreGlobal
            loadp CodeBlock::m_globalObject[registerToStoreGlobal], registerToStoreGlobal
        end)
end

macro writeBarrierOnGlobalLexicalEnvironment(size, get, valueOperand)
    writeBarrierOnGlobal(size, get, valueOperand,
        macro(registerToStoreGlobal)
            loadp CodeBlock[cfr], registerToStoreGlobal
            loadp CodeBlock::m_globalObject[registerToStoreGlobal], registerToStoreGlobal
            loadp JSGlobalObject::m_globalLexicalEnvironment[registerToStoreGlobal], registerToStoreGlobal
        end)
end

macro valueProfile(op, metadata, tag, payload)
    storei tag, %op%::Metadata::profile.m_buckets + TagOffset[metadata]
    storei payload, %op%::Metadata::profile.m_buckets + PayloadOffset[metadata]
end


# Entrypoints into the interpreter

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
macro functionArityCheck(doneLabel, slowPath)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    biaeq t0, CodeBlock::m_numParameters[t1], doneLabel
    move cfr, a0
    move PC, a1
    cCall2(slowPath)   # This slowPath has a simple protocol: t0 = 0 => no error, t0 != 0 => error
    btiz r0, .noError

    # We're throwing before the frame is fully set up. This frame will be
    # ignored by the unwinder. So, let's restore the callee saves before we
    # start unwinding. We need to do this before we change the cfr.
    restoreCalleeSavesUsedByLLInt()

    move r1, cfr   # r1 contains caller frame
    jmp _llint_throw_from_slow_path_trampoline

.noError:
    move r1, t1 # r1 contains slotsToAdd.
    btiz t1, .continue
    loadi PayloadOffset + ArgumentCount[cfr], t2
    addi CallFrameHeaderSlots, t2

    // Check if there are some unaligned slots we can use
    move t1, t3
    andi StackAlignmentSlots - 1, t3
    btiz t3, .noExtraSlot
.fillExtraSlots:
    move 0, t0
    storei t0, PayloadOffset[cfr, t2, 8]
    move UndefinedTag, t0
    storei t0, TagOffset[cfr, t2, 8]
    addi 1, t2
    bsubinz 1, t3, .fillExtraSlots
    andi ~(StackAlignmentSlots - 1), t1
    btiz t1, .continue

.noExtraSlot:
    // Move frame up t1 slots
    negi t1
    move cfr, t3
    subp CalleeSaveSpaceAsVirtualRegisters * SlotSize, t3
    addi CalleeSaveSpaceAsVirtualRegisters, t2
    move t1, t0
    lshiftp 3, t0
    addp t0, cfr
    addp t0, sp
.copyLoop:
    loadi PayloadOffset[t3], t0
    storei t0, PayloadOffset[t3, t1, 8]
    loadi TagOffset[t3], t0
    storei t0, TagOffset[t3, t1, 8]
    addp 8, t3
    bsubinz 1, t2, .copyLoop

    // Fill new slots with JSUndefined
    move t1, t2
.fillLoop:
    move 0, t0
    storei t0, PayloadOffset[t3, t1, 8]
    move UndefinedTag, t0
    storei t0, TagOffset[t3, t1, 8]
    addp 8, t3
    baddinz 1, t2, .fillLoop

.continue:
    # Reload CodeBlock and PC, since the slow_path clobbered it.
    loadp CodeBlock[cfr], t1
    # FIXME: cleanup double load
    # https://bugs.webkit.org/show_bug.cgi?id=190932
    loadp CodeBlock::m_instructions[t1], PC
    loadp [PC], PC
    jmp doneLabel
end

macro branchIfException(label)
    loadp Callee + PayloadOffset[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
    btiz VM::m_exception[t3], .noException
    jmp label
.noException:
end


# Instruction implementations

_llint_op_enter:
    traceExecution()
    checkStackPointerAlignment(t2, 0xdead00e1)
    loadp CodeBlock[cfr], t2                // t2<CodeBlock> = cfr.CodeBlock
    loadi CodeBlock::m_numVars[t2], t2      // t2<size_t> = t2<CodeBlock>.m_numVars
    subi CalleeSaveSpaceAsVirtualRegisters, t2
    move cfr, t3
    subp CalleeSaveSpaceAsVirtualRegisters * SlotSize, t3
    btiz t2, .opEnterDone
    move UndefinedTag, t0
    move 0, t1
    negi t2
.opEnterLoop:
    storei t0, TagOffset[t3, t2, 8]
    storei t1, PayloadOffset[t3, t2, 8]
    addi 1, t2
    btinz t2, .opEnterLoop
.opEnterDone:
    callSlowPath(_slow_path_enter)
    dispatchOp(narrow, op_enter)


llintOpWithProfile(op_get_argument, OpGetArgument, macro (size, get, dispatch, return)
    get(index, t2)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    bilteq t0, t2, .opGetArgumentOutOfBounds
    loadi ThisArgumentOffset + TagOffset[cfr, t2, 8], t0
    loadi ThisArgumentOffset + PayloadOffset[cfr, t2, 8], t3
    return (t0, t3)

.opGetArgumentOutOfBounds:
    return (UndefinedTag, 0)
end)


llintOpWithReturn(op_argument_count, OpArgumentCount, macro (size, get, dispatch, return)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    subi 1, t0
    return(Int32Tag, t0)
end)


llintOpWithReturn(op_get_scope, OpGetScope, macro (size, get, dispatch, return)
    loadi Callee + PayloadOffset[cfr], t0
    loadi JSCallee::m_scope[t0], t0
    return (CellTag, t0)
end)


llintOpWithMetadata(op_to_this, OpToThis, macro (size, get, dispatch, metadata, return)
    get(srcDst, t0)
    bineq TagOffset[cfr, t0, 8], CellTag, .opToThisSlow
    loadi PayloadOffset[cfr, t0, 8], t0
    bbneq JSCell::m_type[t0], FinalObjectType, .opToThisSlow
    metadata(t2, t3)
    loadp OpToThis::Metadata::cachedStructure[t2], t2
    bpneq JSCell::m_structureID[t0], t2, .opToThisSlow
    dispatch()

.opToThisSlow:
    callSlowPath(_slow_path_to_this)
    dispatch()
end)


llintOp(op_check_tdz, OpCheckTdz, macro (size, get, dispatch)
    get(target, t0)
    loadConstantOrVariableTag(size, t0, t1)
    bineq t1, EmptyValueTag, .opNotTDZ
    callSlowPath(_slow_path_throw_tdz_error)

.opNotTDZ:
    dispatch()
end)


llintOpWithReturn(op_mov, OpMov, macro (size, get, dispatch, return)
    get(src, t1)
    loadConstantOrVariable(size, t1, t2, t3)
    return(t2, t3)
end)


llintOpWithReturn(op_not, OpNot, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t2, t3)
    bineq t2, BooleanTag, .opNotSlow
    xori 1, t3
    return(t2, t3)

.opNotSlow:
    callSlowPath(_slow_path_not)
    dispatch()
end)


macro equalityComparisonOp(name, op, integerComparison)
    llintOpWithReturn(op_%name%, op, macro (size, get, dispatch, return)
        get(rhs, t2)
        get(lhs, t0)
        loadConstantOrVariable(size, t2, t3, t1)
        loadConstantOrVariable2Reg(size, t0, t2, t0)
        bineq t2, t3, .opEqSlow
        bieq t2, CellTag, .opEqSlow
        bib t2, LowestTag, .opEqSlow
        integerComparison(t0, t1, t0)
        return(BooleanTag, t0)

    .opEqSlow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end


macro equalityJumpOp(name, op, integerComparison)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(rhs, t2)
        get(lhs, t0)
        loadConstantOrVariable(size, t2, t3, t1)
        loadConstantOrVariable2Reg(size, t0, t2, t0)
        bineq t2, t3, .slow
        bieq t2, CellTag, .slow
        bib t2, LowestTag, .slow
        integerComparison(t0, t1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)

    .slow:
        callSlowPath(_llint_slow_path_%name%)
        nextInstruction()
    end)
end


macro equalNullComparisonOp(name, op, fn)
    llintOpWithReturn(name, op, macro (size, get, dispatch, return)
        get(operand, t0)
        assertNotConstant(size, t0)
        loadi TagOffset[cfr, t0, 8], t1
        loadi PayloadOffset[cfr, t0, 8], t0
        bineq t1, CellTag, .opEqNullImmediate
        btbnz JSCell::m_flags[t0], MasqueradesAsUndefined, .opEqNullMasqueradesAsUndefined
        move 0, t1
        jmp .opEqNullNotImmediate
    .opEqNullMasqueradesAsUndefined:
        loadp JSCell::m_structureID[t0], t1
        loadp CodeBlock[cfr], t0
        loadp CodeBlock::m_globalObject[t0], t0
        cpeq Structure::m_globalObject[t1], t0, t1
        jmp .opEqNullNotImmediate
    .opEqNullImmediate:
        cieq t1, NullTag, t2
        cieq t1, UndefinedTag, t1
        ori t2, t1
    .opEqNullNotImmediate:
        fn(t1)
        return(BooleanTag, t1)
    end)
end

equalNullComparisonOp(op_eq_null, OpEqNull, macro (value) end)

equalNullComparisonOp(op_neq_null, OpNeqNull,
    macro (value) xori 1, value end)


macro strictEqOp(name, op, equalityOperation)
    llintOpWithReturn(op_%name%, op, macro (size, get, dispatch, return)
        get(rhs, t2)
        get(lhs, t0)
        loadConstantOrVariable(size, t2, t3, t1)
        loadConstantOrVariable2Reg(size, t0, t2, t0)
        bineq t2, t3, .slow
        bib t2, LowestTag, .slow
        bineq t2, CellTag, .notStringOrSymbol
        bbaeq JSCell::m_type[t0], ObjectType, .notStringOrSymbol
        bbb JSCell::m_type[t1], ObjectType, .slow
    .notStringOrSymbol:
        equalityOperation(t0, t1, t0)
        return(BooleanTag, t0)

    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end


macro strictEqualityJumpOp(name, op, equalityOperation)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(rhs, t2)
        get(lhs, t0)
        loadConstantOrVariable(size, t2, t3, t1)
        loadConstantOrVariable2Reg(size, t0, t2, t0)
        bineq t2, t3, .slow
        bib t2, LowestTag, .slow
        bineq t2, CellTag, .notStringOrSymbol
        bbaeq JSCell::m_type[t0], ObjectType, .notStringOrSymbol
        bbb JSCell::m_type[t1], ObjectType, .slow
    .notStringOrSymbol:
        equalityOperation(t0, t1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)

    .slow:
        callSlowPath(_llint_slow_path_%name%)
        nextInstruction()
    end)
end


strictEqOp(stricteq, OpStricteq,
    macro (left, right, result) cieq left, right, result end)


strictEqOp(nstricteq, OpNstricteq,
    macro (left, right, result) cineq left, right, result end)


strictEqualityJumpOp(jstricteq, OpJstricteq,
    macro (left, right, target) bieq left, right, target end)


strictEqualityJumpOp(jnstricteq, OpJnstricteq,
    macro (left, right, target) bineq left, right, target end)


macro preOp(name, op, operation)
    llintOp(op_%name%, op, macro (size, get, dispatch)
        get(srcDst, t0)
        bineq TagOffset[cfr, t0, 8], Int32Tag, .slow
        loadi PayloadOffset[cfr, t0, 8], t1
        operation(t1, .slow)
        storei t1, PayloadOffset[cfr, t0, 8]
        dispatch()

    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end


llintOpWithProfile(op_to_number, OpToNumber, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t2, t3)
    bieq t2, Int32Tag, .opToNumberIsInt
    biaeq t2, LowestTag, .opToNumberSlow
.opToNumberIsInt:
    return(t2, t3)

.opToNumberSlow:
    callSlowPath(_slow_path_to_number)
    dispatch()
end)


llintOpWithReturn(op_to_string, OpToString, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t2, t3)
    bineq t2, CellTag, .opToStringSlow
    bbneq JSCell::m_type[t3], StringType, .opToStringSlow
.opToStringIsString:
    return(t2, t3)

.opToStringSlow:
    callSlowPath(_slow_path_to_string)
    dispatch()
end)


llintOpWithProfile(op_to_object, OpToObject, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t2, t3)
    bineq t2, CellTag, .opToObjectSlow
    bbb JSCell::m_type[t3], ObjectType, .opToObjectSlow
    return(t2, t3)

.opToObjectSlow:
    callSlowPath(_slow_path_to_object)
    dispatch()
end)


llintOpWithMetadata(op_negate, OpNegate, macro (size, get, dispatch, metadata, return)

    macro arithProfile(type)
        ori type, OpNegate::Metadata::arithProfile[t5]
    end

    metadata(t5, t0)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t1, t2)
    bineq t1, Int32Tag, .opNegateSrcNotInt
    btiz t2, 0x7fffffff, .opNegateSlow
    negi t2
    arithProfile(ArithProfileInt)
    return (Int32Tag, t2)
.opNegateSrcNotInt:
    bia t1, LowestTag, .opNegateSlow
    xori 0x80000000, t1
    arithProfile(ArithProfileNumber)
    return(t1, t2)

.opNegateSlow:
    callSlowPath(_slow_path_negate)
    dispatch()
end)


macro binaryOpCustomStore(name, op, integerOperationAndStore, doubleOperation)
    llintOpWithMetadata(op_%name%, op, macro (size, get, dispatch, metadata, return)
        macro arithProfile(type)
            ori type, %op%::Metadata::arithProfile[t5]
        end

        metadata(t5, t2)
        get(rhs, t2)
        get(lhs, t0)
        loadConstantOrVariable(size, t2, t3, t1)
        loadConstantOrVariable2Reg(size, t0, t2, t0)
        bineq t2, Int32Tag, .op1NotInt
        bineq t3, Int32Tag, .op2NotInt
        arithProfile(ArithProfileIntInt)
        get(dst, t2)
        integerOperationAndStore(t3, t1, t0, .slow, t2)
        dispatch()

    .op1NotInt:
        # First operand is definitely not an int, the second operand could be anything.
        bia t2, LowestTag, .slow
        bib t3, LowestTag, .op1NotIntOp2Double
        bineq t3, Int32Tag, .slow
        arithProfile(ArithProfileNumberInt)
        ci2d t1, ft1
        jmp .op1NotIntReady
    .op1NotIntOp2Double:
        fii2d t1, t3, ft1
        arithProfile(ArithProfileNumberNumber)
    .op1NotIntReady:
        get(dst, t1)
        fii2d t0, t2, ft0
        doubleOperation(ft1, ft0)
        stored ft0, [cfr, t1, 8]
        dispatch()

    .op2NotInt:
        # First operand is definitely an int, the second operand is definitely not.
        get(dst, t2)
        bia t3, LowestTag, .slow
        arithProfile(ArithProfileIntNumber)
        ci2d t0, ft0
        fii2d t1, t3, ft1
        doubleOperation(ft1, ft0)
        stored ft0, [cfr, t2, 8]
        dispatch()

    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end

macro binaryOp(name, op, integerOperation, doubleOperation)
    binaryOpCustomStore(name, op,
        macro (int32Tag, left, right, slow, index)
            integerOperation(left, right, slow)
            storei int32Tag, TagOffset[cfr, index, 8]
            storei right, PayloadOffset[cfr, index, 8]
        end,
        doubleOperation)
end

binaryOp(add, OpAdd,
    macro (left, right, slow) baddio left, right, slow end,
    macro (left, right) addd left, right end)


binaryOpCustomStore(mul, OpMul,
    macro (int32Tag, left, right, slow, index)
        const scratch = int32Tag   # We know that we can reuse the int32Tag register since it has a constant.
        move right, scratch
        bmulio left, scratch, slow
        btinz scratch, .done
        bilt left, 0, slow
        bilt right, 0, slow
    .done:
        storei Int32Tag, TagOffset[cfr, index, 8]
        storei scratch, PayloadOffset[cfr, index, 8]
    end,
    macro (left, right) muld left, right end)


binaryOp(sub, OpSub,
    macro (left, right, slow) bsubio left, right, slow end,
    macro (left, right) subd left, right end)


binaryOpCustomStore(div, OpDiv,
    macro (int32Tag, left, right, slow, index)
        ci2d left, ft0
        ci2d right, ft1
        divd ft0, ft1
        bcd2i ft1, right, .notInt
        storei int32Tag, TagOffset[cfr, index, 8]
        storei right, PayloadOffset[cfr, index, 8]
        jmp .done
    .notInt:
        stored ft1, [cfr, index, 8]
    .done:
    end,
    macro (left, right) divd left, right end)


llintOpWithReturn(op_unsigned, OpUnsigned, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariablePayload(size, t1, Int32Tag, t2, .opUnsignedSlow)
    bilt t2, 0, .opUnsignedSlow
    return (Int32Tag, t2)
.opUnsignedSlow:
    callSlowPath(_slow_path_unsigned)
    dispatch()
end)


macro commonBitOp(opKind, name, op, operation)
    opKind(op_%name%, op, macro (size, get, dispatch, return)
        get(rhs, t2)
        get(lhs, t0)
        loadConstantOrVariable(size, t2, t3, t1)
        loadConstantOrVariable2Reg(size, t0, t2, t0)
        bineq t3, Int32Tag, .slow
        bineq t2, Int32Tag, .slow
        operation(t1, t0)
        return (t3, t0)

    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end

macro bitOp(name, op, operation)
    commonBitOp(llintOpWithReturn, name, op, operation)
end

macro bitOpProfiled(name, op, operation)
    commonBitOp(llintOpWithProfile, name, op, operation)
end


bitOp(lshift, OpLshift,
    macro (left, right) lshifti left, right end)


bitOp(rshift, OpRshift,
    macro (left, right) rshifti left, right end)


bitOp(urshift, OpUrshift,
    macro (left, right) urshifti left, right end)

bitOp(bitxor, OpBitxor,
    macro (left, right) xori left, right end)

bitOpProfiled(bitand, OpBitand,
    macro (left, right) andi left, right end)

bitOpProfiled(bitor, OpBitor,
        macro (left, right) ori left, right end)


llintOp(op_overrides_has_instance, OpOverridesHasInstance, macro (size, get, dispatch)
    get(dst, t3)
    storei BooleanTag, TagOffset[cfr, t3, 8]

    # First check if hasInstanceValue is the one on Function.prototype[Symbol.hasInstance]
    get(hasInstanceValue, t0)
    loadConstantOrVariablePayload(size, t0, CellTag, t2, .opOverrideshasInstanceValueNotCell)
    loadConstantOrVariable(size, t0, t1, t2)
    bineq t1, CellTag, .opOverrideshasInstanceValueNotCell

    # We don't need hasInstanceValue's tag register anymore.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_globalObject[t1], t1
    loadp JSGlobalObject::m_functionProtoHasInstanceSymbolFunction[t1], t1
    bineq t1, t2, .opOverrideshasInstanceValueNotDefault

    # We know the constructor is a cell.
    get(constructor, t0)
    loadConstantOrVariablePayloadUnchecked(size, t0, t1)
    tbz JSCell::m_flags[t1], ImplementsDefaultHasInstance, t0
    storei t0, PayloadOffset[cfr, t3, 8]
    dispatch()

.opOverrideshasInstanceValueNotCell:
.opOverrideshasInstanceValueNotDefault:
    storei 1, PayloadOffset[cfr, t3, 8]
    dispatch()
end)


llintOpWithReturn(op_is_empty, OpIsEmpty, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t2, t3)
    cieq t2, EmptyValueTag, t3
    return(BooleanTag, t3)
end)


llintOpWithReturn(op_is_undefined, OpIsUndefined, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t2, t3)
    bieq t2, CellTag, .opIsUndefinedCell
    cieq t2, UndefinedTag, t3
    return(BooleanTag, t3)
.opIsUndefinedCell:
    btbnz JSCell::m_flags[t3], MasqueradesAsUndefined, .opIsUndefinedMasqueradesAsUndefined
    return(BooleanTag, 0)
.opIsUndefinedMasqueradesAsUndefined:
    loadp JSCell::m_structureID[t3], t1
    loadp CodeBlock[cfr], t3
    loadp CodeBlock::m_globalObject[t3], t3
    cpeq Structure::m_globalObject[t1], t3, t1
    return(BooleanTag, t1)
end)


llintOpWithReturn(op_is_boolean, OpIsBoolean, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariableTag(size, t1, t0)
    cieq t0, BooleanTag, t0
    return(BooleanTag, t0)
end)


llintOpWithReturn(op_is_number, OpIsNumber, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariableTag(size, t1, t0)
    addi 1, t0
    cib t0, LowestTag + 1, t1
    return(BooleanTag, t1)
end)


llintOpWithReturn(op_is_cell_with_type, OpIsCellWithType, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0, t3)
    bineq t0, CellTag, .notCellCase
    get(type, t0)
    cbeq JSCell::m_type[t3], t0, t1
    return(BooleanTag, t1)
.notCellCase:
    return(BooleanTag, 0)
end)


llintOpWithReturn(op_is_object, OpIsObject, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0, t3)
    bineq t0, CellTag, .opIsObjectNotCell
    cbaeq JSCell::m_type[t3], ObjectType, t1
    return(BooleanTag, t1)
.opIsObjectNotCell:
    return(BooleanTag, 0)
end)


macro loadPropertyAtVariableOffsetKnownNotInline(propertyOffset, objectAndStorage, tag, payload)
    assert(macro (ok) bigteq propertyOffset, firstOutOfLineOffset, ok end)
    negi propertyOffset
    loadp JSObject::m_butterfly[objectAndStorage], objectAndStorage
    loadi TagOffset + (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffset, 8], tag
    loadi PayloadOffset + (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffset, 8], payload
end

macro loadPropertyAtVariableOffset(propertyOffset, objectAndStorage, tag, payload)
    bilt propertyOffset, firstOutOfLineOffset, .isInline
    loadp JSObject::m_butterfly[objectAndStorage], objectAndStorage
    negi propertyOffset
    jmp .ready
.isInline:
    addp sizeof JSObject - (firstOutOfLineOffset - 2) * 8, objectAndStorage
.ready:
    loadi TagOffset + (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffset, 8], tag
    loadi PayloadOffset + (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffset, 8], payload
end

macro storePropertyAtVariableOffset(propertyOffsetAsInt, objectAndStorage, tag, payload)
    bilt propertyOffsetAsInt, firstOutOfLineOffset, .isInline
    loadp JSObject::m_butterfly[objectAndStorage], objectAndStorage
    negi propertyOffsetAsInt
    jmp .ready
.isInline:
    addp sizeof JSObject - (firstOutOfLineOffset - 2) * 8, objectAndStorage
.ready:
    storei tag, TagOffset + (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffsetAsInt, 8]
    storei payload, PayloadOffset + (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffsetAsInt, 8]
end


# We only do monomorphic get_by_id caching for now, and we do not modify the
# opcode for own properties. We also allow for the cache to change anytime it fails,
# since ping-ponging is free. At best we get lucky and the get_by_id will continue
# to take fast path on the new cache. At worst we take slow path, which is what
# we would have been doing anyway. For prototype/unset properties, we will attempt to
# convert opcode into a get_by_id_proto_load/get_by_id_unset, respectively, after an
# execution counter hits zero.

llintOpWithMetadata(op_get_by_id_direct, OpGetByIdDirect, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)
    get(base, t0)
    loadi OpGetByIdDirect::Metadata::structure[t5], t1
    loadConstantOrVariablePayload(size, t0, CellTag, t3, .opGetByIdDirectSlow)
    loadi OpGetByIdDirect::Metadata::offset[t5], t2
    bineq JSCell::m_structureID[t3], t1, .opGetByIdDirectSlow
    loadPropertyAtVariableOffset(t2, t3, t0, t1)
    valueProfile(OpGetByIdDirect, t5, t0, t1)
    return(t0, t1)

.opGetByIdDirectSlow:
    callSlowPath(_llint_slow_path_get_by_id_direct)
    dispatch()
end)


llintOpWithMetadata(op_get_by_id, OpGetById, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)
    loadb OpGetById::Metadata::mode[t5], t1
    get(base, t0)

.opGetByIdProtoLoad:
    bbneq t1, constexpr GetByIdMode::ProtoLoad, .opGetByIdArrayLength
    loadi OpGetById::Metadata::modeMetadata.protoLoadMode.structure[t5], t1
    loadConstantOrVariablePayload(size, t0, CellTag, t3, .opGetByIdSlow)
    loadi OpGetById::Metadata::modeMetadata.protoLoadMode.cachedOffset[t5], t2
    bineq JSCell::m_structureID[t3], t1, .opGetByIdSlow
    loadp OpGetById::Metadata::modeMetadata.protoLoadMode.cachedSlot[t5], t3
    loadPropertyAtVariableOffset(t2, t3, t0, t1)
    valueProfile(OpGetById, t5, t0, t1)
    return(t0, t1)

.opGetByIdArrayLength:
    bbneq t1, constexpr GetByIdMode::ArrayLength, .opGetByIdUnset
    loadConstantOrVariablePayload(size, t0, CellTag, t3, .opGetByIdSlow)
    move t3, t2
    arrayProfile(OpGetById::Metadata::modeMetadata.arrayLengthMode.arrayProfile, t2, t5, t0)
    btiz t2, IsArray, .opGetByIdSlow
    btiz t2, IndexingShapeMask, .opGetByIdSlow
    loadp JSObject::m_butterfly[t3], t0
    loadi -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], t0
    bilt t0, 0, .opGetByIdSlow
    valueProfile(OpGetById, t5, Int32Tag, t0)
    return(Int32Tag, t0)

.opGetByIdUnset:
    bbneq t1, constexpr GetByIdMode::Unset, .opGetByIdDefault
    loadi OpGetById::Metadata::modeMetadata.unsetMode.structure[t5], t1
    loadConstantOrVariablePayload(size, t0, CellTag, t3, .opGetByIdSlow)
    bineq JSCell::m_structureID[t3], t1, .opGetByIdSlow
    valueProfile(OpGetById, t5, UndefinedTag, 0)
    return(UndefinedTag, 0)

.opGetByIdDefault:
    loadi OpGetById::Metadata::modeMetadata.defaultMode.structure[t5], t1
    loadConstantOrVariablePayload(size, t0, CellTag, t3, .opGetByIdSlow)
    loadis OpGetById::Metadata::modeMetadata.defaultMode.cachedOffset[t5], t2
    bineq JSCell::m_structureID[t3], t1, .opGetByIdSlow
    loadPropertyAtVariableOffset(t2, t3, t0, t1)
    valueProfile(OpGetById, t5, t0, t1)
    return(t0, t1)

.opGetByIdSlow:
    callSlowPath(_llint_slow_path_get_by_id)
    dispatch()
end)


llintOpWithMetadata(op_put_by_id, OpPutById, macro (size, get, dispatch, metadata, return)
    writeBarrierOnOperands(size, get, base, value)
    metadata(t5, t3)
    get(base, t3)
    loadConstantOrVariablePayload(size, t3, CellTag, t0, .opPutByIdSlow)
    loadi JSCell::m_structureID[t0], t2
    bineq t2, OpPutById::Metadata::oldStructure[t5], .opPutByIdSlow

    # At this point, we have:
    # t5 -> metadata
    # t2 -> currentStructureID
    # t0 -> object base
    # We will lose currentStructureID in the shenanigans below.

    get(value, t1)
    loadConstantOrVariable(size, t1, t2, t3)
    loadi OpPutById::Metadata::flags[t5], t1

    # At this point, we have:
    # t0 -> object base
    # t1 -> put by id flags
    # t2 -> value tag
    # t3 -> value payload

    btinz t1, PutByIdPrimaryTypeMask, .opPutByIdTypeCheckObjectWithStructureOrOther

    # We have one of the non-structure type checks. Find out which one.
    andi PutByIdSecondaryTypeMask, t1
    bilt t1, PutByIdSecondaryTypeString, .opPutByIdTypeCheckLessThanString

    # We are one of the following: String, Symbol, Object, ObjectOrOther, Top
    bilt t1, PutByIdSecondaryTypeObjectOrOther, .opPutByIdTypeCheckLessThanObjectOrOther

    # We are either ObjectOrOther or Top.
    bieq t1, PutByIdSecondaryTypeTop, .opPutByIdDoneCheckingTypes

    # Check if we are ObjectOrOther.
    bieq t2, CellTag, .opPutByIdTypeCheckObject
.opPutByIdTypeCheckOther:
    bieq t2, NullTag, .opPutByIdDoneCheckingTypes
    bieq t2, UndefinedTag, .opPutByIdDoneCheckingTypes
    jmp .opPutByIdSlow

.opPutByIdTypeCheckLessThanObjectOrOther:
    # We are either String, Symbol or Object.
    bineq t2, CellTag, .opPutByIdSlow
    bieq t1, PutByIdSecondaryTypeObject, .opPutByIdTypeCheckObject
    bieq t1, PutByIdSecondaryTypeSymbol, .opPutByIdTypeCheckSymbol
    bbeq JSCell::m_type[t3], StringType, .opPutByIdDoneCheckingTypes
    jmp .opPutByIdSlow
.opPutByIdTypeCheckObject:
    bbaeq JSCell::m_type[t3], ObjectType, .opPutByIdDoneCheckingTypes
    jmp .opPutByIdSlow
.opPutByIdTypeCheckSymbol:
    bbeq JSCell::m_type[t3], SymbolType, .opPutByIdDoneCheckingTypes
    jmp .opPutByIdSlow

.opPutByIdTypeCheckLessThanString:
    # We are one of the following: Bottom, Boolean, Other, Int32, Number.
    bilt t1, PutByIdSecondaryTypeInt32, .opPutByIdTypeCheckLessThanInt32

    # We are either Int32 or Number.
    bieq t1, PutByIdSecondaryTypeNumber, .opPutByIdTypeCheckNumber

    bieq t2, Int32Tag, .opPutByIdDoneCheckingTypes
    jmp .opPutByIdSlow

.opPutByIdTypeCheckNumber:
    bib t2, LowestTag + 1, .opPutByIdDoneCheckingTypes
    jmp .opPutByIdSlow

.opPutByIdTypeCheckLessThanInt32:
    # We are one of the following: Bottom, Boolean, Other
    bineq t1, PutByIdSecondaryTypeBoolean, .opPutByIdTypeCheckBottomOrOther
    bieq t2, BooleanTag, .opPutByIdDoneCheckingTypes
    jmp .opPutByIdSlow

.opPutByIdTypeCheckBottomOrOther:
    bieq t1, PutByIdSecondaryTypeOther, .opPutByIdTypeCheckOther
    jmp .opPutByIdSlow

.opPutByIdTypeCheckObjectWithStructureOrOther:
    bieq t2, CellTag, .opPutByIdTypeCheckObjectWithStructure
    btinz t1, PutByIdPrimaryTypeObjectWithStructureOrOther, .opPutByIdTypeCheckOther
    jmp .opPutByIdSlow

.opPutByIdTypeCheckObjectWithStructure:
    andi PutByIdSecondaryTypeMask, t1
    bineq t1, JSCell::m_structureID[t3], .opPutByIdSlow

.opPutByIdDoneCheckingTypes:
    loadi OpPutById::Metadata::newStructure[t5], t1

    btiz t1, .opPutByIdNotTransition

    # This is the transition case. t1 holds the new Structure*. If we have a chain, we need to
    # check it. t0 is the base. We may clobber t1 to use it as scratch.
    loadp OpPutById::Metadata::structureChain[t5], t3
    btpz t3, .opPutByIdTransitionDirect

    loadi OpPutById::Metadata::oldStructure[t5], t2 # Need old structure again.
    loadp StructureChain::m_vector[t3], t3
    assert(macro (ok) btpnz t3, ok end)

    loadp Structure::m_prototype[t2], t2
    btpz t2, .opPutByIdTransitionChainDone
.opPutByIdTransitionChainLoop:
    loadp [t3], t1
    bpneq t1, JSCell::m_structureID[t2], .opPutByIdSlow
    addp 4, t3
    loadp Structure::m_prototype[t1], t2
    btpnz t2, .opPutByIdTransitionChainLoop

.opPutByIdTransitionChainDone:
    loadi OpPutById::Metadata::newStructure[t5], t1

.opPutByIdTransitionDirect:
    storei t1, JSCell::m_structureID[t0]
    get(value, t1)
    loadConstantOrVariable(size, t1, t2, t3)
    loadi OpPutById::Metadata::offset[t5], t1
    storePropertyAtVariableOffset(t1, t0, t2, t3)
    writeBarrierOnOperand(size, get, base)
    dispatch()

.opPutByIdNotTransition:
    # The only thing live right now is t0, which holds the base.
    get(value, t1)
    loadConstantOrVariable(size, t1, t2, t3)
    loadi OpPutById::Metadata::offset[t5], t1
    storePropertyAtVariableOffset(t1, t0, t2, t3)
    dispatch()

.opPutByIdSlow:
    callSlowPath(_llint_slow_path_put_by_id)
    dispatch()
end)


llintOpWithMetadata(op_get_by_val, OpGetByVal, macro (size, get, dispatch, metadata, return)
    metadata(t5, t2)
    get(base, t2)
    loadConstantOrVariablePayload(size, t2, CellTag, t0, .opGetByValSlow)
    move t0, t2
    arrayProfile(OpGetByVal::Metadata::arrayProfile, t2, t5, t1)
    get(property, t3)
    loadConstantOrVariablePayload(size, t3, Int32Tag, t1, .opGetByValSlow)
    loadp JSObject::m_butterfly[t0], t3
    andi IndexingShapeMask, t2
    bieq t2, Int32Shape, .opGetByValIsContiguous
    bineq t2, ContiguousShape, .opGetByValNotContiguous

.opGetByValIsContiguous:
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValSlow
    loadi TagOffset[t3, t1, 8], t2
    loadi PayloadOffset[t3, t1, 8], t1
    jmp .opGetByValDone

.opGetByValNotContiguous:
    bineq t2, DoubleShape, .opGetByValNotDouble
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValSlow
    loadd [t3, t1, 8], ft0
    bdnequn ft0, ft0, .opGetByValSlow
    # FIXME: This could be massively optimized.
    fd2ii ft0, t1, t2
    get(dst, t0)
    jmp .opGetByValNotEmpty

.opGetByValNotDouble:
    subi ArrayStorageShape, t2
    bia t2, SlowPutArrayStorageShape - ArrayStorageShape, .opGetByValSlow
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t3], .opGetByValSlow
    loadi ArrayStorage::m_vector + TagOffset[t3, t1, 8], t2
    loadi ArrayStorage::m_vector + PayloadOffset[t3, t1, 8], t1

.opGetByValDone:
    get(dst, t0)
    bieq t2, EmptyValueTag, .opGetByValSlow
.opGetByValNotEmpty:
    storei t2, TagOffset[cfr, t0, 8]
    storei t1, PayloadOffset[cfr, t0, 8]
    valueProfile(OpGetByVal, t5, t2, t1)
    dispatch()

.opGetByValSlow:
    callSlowPath(_llint_slow_path_get_by_val)
    dispatch()
end)


macro putByValOp(name, op)
    llintOpWithMetadata(op_%name%, op, macro (size, get, dispatch, metadata, return)
        macro contiguousPutByVal(storeCallback)
            biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], .outOfBounds
        .storeResult:
            get(value, t2)
            storeCallback(t2, t1, t0, t3)
            dispatch()

        .outOfBounds:
            biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
            storeb 1, %op%::Metadata::arrayProfile.m_mayStoreToHole[t5]
            addi 1, t3, t2
            storei t2, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
            jmp .storeResult
        end

        writeBarrierOnOperands(size, get, base, value)
        metadata(t5, t0)
        get(base, t0)
        loadConstantOrVariablePayload(size, t0, CellTag, t1, .opPutByValSlow)
        move t1, t2
        arrayProfile(%op%::Metadata::arrayProfile, t2, t5, t0)
        get(property, t0)
        loadConstantOrVariablePayload(size, t0, Int32Tag, t3, .opPutByValSlow)
        loadp JSObject::m_butterfly[t1], t0
        btinz t2, CopyOnWrite, .opPutByValSlow
        andi IndexingShapeMask, t2
        bineq t2, Int32Shape, .opPutByValNotInt32
        contiguousPutByVal(
            macro (operand, scratch, base, index)
                loadConstantOrVariablePayload(size, operand, Int32Tag, scratch, .opPutByValSlow)
                storei Int32Tag, TagOffset[base, index, 8]
                storei scratch, PayloadOffset[base, index, 8]
            end)

    .opPutByValNotInt32:
        bineq t2, DoubleShape, .opPutByValNotDouble
        contiguousPutByVal(
            macro (operand, scratch, base, index)
                const tag = scratch
                const payload = operand
                loadConstantOrVariable2Reg(size, operand, tag, payload)
                bineq tag, Int32Tag, .notInt
                ci2d payload, ft0
                jmp .ready
            .notInt:
                fii2d payload, tag, ft0
                bdnequn ft0, ft0, .opPutByValSlow
            .ready:
                stored ft0, [base, index, 8]
            end)

    .opPutByValNotDouble:
        bineq t2, ContiguousShape, .opPutByValNotContiguous
        contiguousPutByVal(
            macro (operand, scratch, base, index)
                const tag = scratch
                const payload = operand
                loadConstantOrVariable2Reg(size, operand, tag, payload)
                storei tag, TagOffset[base, index, 8]
                storei payload, PayloadOffset[base, index, 8]
            end)

    .opPutByValNotContiguous:
        bineq t2, ArrayStorageShape, .opPutByValSlow
        biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
        bieq ArrayStorage::m_vector + TagOffset[t0, t3, 8], EmptyValueTag, .opPutByValArrayStorageEmpty
    .opPutByValArrayStorageStoreResult:
        get(value, t2)
        loadConstantOrVariable2Reg(size, t2, t1, t2)
        storei t1, ArrayStorage::m_vector + TagOffset[t0, t3, 8]
        storei t2, ArrayStorage::m_vector + PayloadOffset[t0, t3, 8]
        dispatch()

    .opPutByValArrayStorageEmpty:
        storeb 1, %op%::Metadata::arrayProfile.m_mayStoreToHole[t5]
        addi 1, ArrayStorage::m_numValuesInVector[t0]
        bib t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], .opPutByValArrayStorageStoreResult
        addi 1, t3, t1
        storei t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
        jmp .opPutByValArrayStorageStoreResult

    .opPutByValOutOfBounds:
        storeb 1, %op%::Metadata::arrayProfile.m_outOfBounds[t5]
    .opPutByValSlow:
        callSlowPath(_llint_slow_path_%name%)
        dispatch()
    end)
end


putByValOp(put_by_val, OpPutByVal)

putByValOp(put_by_val_direct, OpPutByValDirect)


macro llintJumpTrueOrFalseOp(name, op, conditionOp)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(condition, t1)
        loadConstantOrVariablePayload(size, t1, BooleanTag, t0, .slow)
        conditionOp(t0, .target)
        dispatch()

    .target:
        jump(target)

    .slow:
        callSlowPath(_llint_slow_path_%name%)
        nextInstruction()
    end)
end


macro equalNullJumpOp(name, op, cellHandler, immediateHandler)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(value, t0)
        assertNotConstant(size, t0)
        loadi TagOffset[cfr, t0, 8], t1
        loadi PayloadOffset[cfr, t0, 8], t0
        bineq t1, CellTag, .immediate
        loadp JSCell::m_structureID[t0], t2
        cellHandler(t2, JSCell::m_flags[t0], .target)
        dispatch()

    .target:
        jump(target)

    .immediate:
        ori 1, t1
        immediateHandler(t1, .target)
        dispatch()
    end)
end

equalNullJumpOp(jeq_null, OpJeqNull,
    macro (structure, value, target)
        btbz value, MasqueradesAsUndefined, .opJeqNullNotMasqueradesAsUndefined
        loadp CodeBlock[cfr], t0
        loadp CodeBlock::m_globalObject[t0], t0
        bpeq Structure::m_globalObject[structure], t0, target
    .opJeqNullNotMasqueradesAsUndefined:
    end,
    macro (value, target) bieq value, NullTag, target end)
    

equalNullJumpOp(jneq_null, OpJneqNull,
    macro (structure, value, target)
        btbz value, MasqueradesAsUndefined, target
        loadp CodeBlock[cfr], t0
        loadp CodeBlock::m_globalObject[t0], t0
        bpneq Structure::m_globalObject[structure], t0, target
    end,
    macro (value, target) bineq value, NullTag, target end)


llintOpWithMetadata(op_jneq_ptr, OpJneqPtr, macro (size, get, dispatch, metadata, return)
    get(value, t0)
    get(specialPointer, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_globalObject[t2], t2
    bineq TagOffset[cfr, t0, 8], CellTag, .opJneqPtrBranch
    loadp JSGlobalObject::m_specialPointers[t2, t1, 4], t1
    bpeq PayloadOffset[cfr, t0, 8], t1, .opJneqPtrFallThrough
.opJneqPtrBranch:
    metadata(t5, t2)
    storeb 1, OpJneqPtr::Metadata::hasJumped[t5]
    get(target, t0)
    jumpImpl(t0)
.opJneqPtrFallThrough:
    dispatch()
end)


macro compareUnsignedJumpOp(name, op, integerCompare)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(lhs, t2)
        get(rhs, t3)
        loadConstantOrVariable(size, t2, t0, t1)
        loadConstantOrVariable2Reg(size, t3, t2, t3)
        integerCompare(t1, t3, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)
    end)
end


macro compareUnsignedOp(name, op, integerCompareAndSet)
    llintOpWithReturn(op_%name%, op, macro (size, get, dispatch, return)
        get(rhs, t2)
        get(lhs, t0)
        loadConstantOrVariable(size, t2, t3, t1)
        loadConstantOrVariable2Reg(size, t0, t2, t0)
        integerCompareAndSet(t0, t1, t0)
        return(BooleanTag, t0)
    end)
end


macro compareJumpOp(name, op, integerCompare, doubleCompare)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(lhs, t2)
        get(rhs, t3)
        loadConstantOrVariable(size, t2, t0, t1)
        loadConstantOrVariable2Reg(size, t3, t2, t3)
        bineq t0, Int32Tag, .op1NotInt
        bineq t2, Int32Tag, .op2NotInt
        integerCompare(t1, t3, .jumpTarget)
        dispatch()

    .op1NotInt:
        bia t0, LowestTag, .slow
        bib t2, LowestTag, .op1NotIntOp2Double
        bineq t2, Int32Tag, .slow
        ci2d t3, ft1
        jmp .op1NotIntReady
    .op1NotIntOp2Double:
        fii2d t3, t2, ft1
    .op1NotIntReady:
        fii2d t1, t0, ft0
        doubleCompare(ft0, ft1, .jumpTarget)
        dispatch()

    .op2NotInt:
        ci2d t1, ft0
        bia t2, LowestTag, .slow
        fii2d t3, t2, ft1
        doubleCompare(ft0, ft1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)

    .slow:
        callSlowPath(_llint_slow_path_%name%)
        nextInstruction()
    end)
end


llintOpWithJump(op_switch_imm, OpSwitchImm, macro (size, get, jump, dispatch)
    get(scrutinee, t2)
    get(tableIndex, t3)
    loadConstantOrVariable(size, t2, t1, t0)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3   # FIXME: would be nice to peephole this!
    loadp CodeBlock::RareData::m_switchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    bineq t1, Int32Tag, .opSwitchImmNotInt
    subi SimpleJumpTable::min[t2], t0
    biaeq t0, SimpleJumpTable::branchOffsets + VectorSizeOffset[t2], .opSwitchImmFallThrough
    loadp SimpleJumpTable::branchOffsets + VectorBufferOffset[t2], t3
    loadi [t3, t0, 4], t1
    btiz t1, .opSwitchImmFallThrough
    dispatchIndirect(t1)

.opSwitchImmNotInt:
    bib t1, LowestTag, .opSwitchImmSlow  # Go to slow path if it's a double.
.opSwitchImmFallThrough:
    jump(defaultOffset)

.opSwitchImmSlow:
    callSlowPath(_llint_slow_path_switch_imm)
    nextInstruction()
end)


llintOpWithJump(op_switch_char, OpSwitchChar, macro (size, get, jump, dispatch)
    get(scrutinee, t2)
    get(tableIndex, t3)
    loadConstantOrVariable(size, t2, t1, t0)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3
    loadp CodeBlock::RareData::m_switchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    bineq t1, CellTag, .opSwitchCharFallThrough
    bbneq JSCell::m_type[t0], StringType, .opSwitchCharFallThrough
    bineq JSString::m_length[t0], 1, .opSwitchCharFallThrough
    loadp JSString::m_value[t0], t0
    btpz  t0, .opSwitchOnRope
    loadp StringImpl::m_data8[t0], t1
    btinz StringImpl::m_hashAndFlags[t0], HashFlags8BitBuffer, .opSwitchChar8Bit
    loadh [t1], t0
    jmp .opSwitchCharReady
.opSwitchChar8Bit:
    loadb [t1], t0
.opSwitchCharReady:
    subi SimpleJumpTable::min[t2], t0
    biaeq t0, SimpleJumpTable::branchOffsets + VectorSizeOffset[t2], .opSwitchCharFallThrough
    loadp SimpleJumpTable::branchOffsets + VectorBufferOffset[t2], t2
    loadi [t2, t0, 4], t1
    btiz t1, .opSwitchCharFallThrough
    dispatchIndirect(t1)

.opSwitchCharFallThrough:
    jump(defaultOffset)

.opSwitchOnRope:
    callSlowPath(_llint_slow_path_switch_char)
    nextInstruction()
end)


macro arrayProfileForCall(op, getu)
    getu(argv, t3)
    negi t3
    bineq ThisArgumentOffset + TagOffset[cfr, t3, 8], CellTag, .done
    loadi ThisArgumentOffset + PayloadOffset[cfr, t3, 8], t0
    loadp JSCell::m_structureID[t0], t0
    storep t0, %op%::Metadata::arrayProfile.m_lastSeenStructureID[t5]
.done:
end

macro commonCallOp(name, slowPath, op, prepareCall, prologue)
    llintOpWithMetadata(name, op, macro (size, get, dispatch, metadata, return)
        metadata(t5, t0)

        prologue(macro (field, dst)
            getu(size, op, field, dst)
        end, metadata)

        get(callee, t0)
        loadp %op%::Metadata::callLinkInfo.callee[t5], t2
        loadConstantOrVariablePayload(size, t0, CellTag, t3, .opCallSlow)
        bineq t3, t2, .opCallSlow
        get(argv, t3)
        lshifti 3, t3
        negi t3
        addp cfr, t3  # t3 contains the new value of cfr
        storei t2, Callee + PayloadOffset[t3]
        get(argc, t2)
        storei PC, ArgumentCount + TagOffset[cfr]
        storei t2, ArgumentCount + PayloadOffset[t3]
        storei CellTag, Callee + TagOffset[t3]
        move t3, sp
        prepareCall(%op%::Metadata::callLinkInfo.machineCodeTarget[t5], t2, t3, t4, JSEntryPtrTag)
        callTargetFunction(size, op, dispatch, %op%::Metadata::callLinkInfo.machineCodeTarget[t5], JSEntryPtrTag)

    .opCallSlow:
        slowPathForCall(size, op, dispatch, slowPath, prepareCall)
    end)
end

llintOp(op_ret, OpRet, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    get(value, t2)
    loadConstantOrVariable(size, t2, t1, t0)
    doReturn()
end)


llintOpWithReturn(op_to_primitive, OpToPrimitive, macro (size, get, dispatch, return)
    get(src, t2)
    loadConstantOrVariable(size, t2, t1, t0)
    bineq t1, CellTag, .opToPrimitiveIsImm
    bbaeq JSCell::m_type[t0], ObjectType, .opToPrimitiveSlowCase
.opToPrimitiveIsImm:
    return(t1, t0)

.opToPrimitiveSlowCase:
    callSlowPath(_slow_path_to_primitive)
    dispatch()
end)


commonOp(op_catch, macro() end, macro (size)
    # This is where we end up from the JIT's throw trampoline (because the
    # machine code return address will be set to _llint_op_catch), and from
    # the interpreter's throw trampoline (see _llint_throw_trampoline).
    # The throwing code must have known that we were throwing to the interpreter,
    # and have set VM::targetInterpreterPCForThrow.
    loadp Callee + PayloadOffset[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)
    loadp VM::callFrameForCatch[t3], cfr
    storep 0, VM::callFrameForCatch[t3]
    restoreStackPointerAfterCall()

    if C_LOOP
        # restore metadataTable since we don't restore callee saves for CLoop during unwinding
        loadp CodeBlock[cfr], t1
        # FIXME: cleanup double load
        # https://bugs.webkit.org/show_bug.cgi?id=190933
        loadp CodeBlock::m_metadata[t1], metadataTable
        loadp MetadataTable::m_buffer[metadataTable], metadataTable
    end

    loadi VM::targetInterpreterPCForThrow[t3], PC

    callSlowPath(_llint_slow_path_check_if_exception_is_uncatchable_and_notify_profiler)
    bpeq r1, 0, .isCatchableException
    jmp _llint_throw_from_slow_path_trampoline

.isCatchableException:
    loadp Callee + PayloadOffset[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3

    loadi VM::m_exception[t3], t0
    storei 0, VM::m_exception[t3]
    get(size, OpCatch, exception, t2)
    storei t0, PayloadOffset[cfr, t2, 8]
    storei CellTag, TagOffset[cfr, t2, 8]

    loadi Exception::m_value + TagOffset[t0], t1
    loadi Exception::m_value + PayloadOffset[t0], t0
    get(size, OpCatch, thrownValue, t2)
    storei t0, PayloadOffset[cfr, t2, 8]
    storei t1, TagOffset[cfr, t2, 8]

    traceExecution()  # This needs to be here because we don't want to clobber t0, t1, t2, t3 above.

    callSlowPath(_llint_slow_path_profile_catch)

    dispatchOp(size, op_catch)
end)

llintOp(op_end, OpEnd, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    get(value, t0)
    assertNotConstant(size, t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
    doReturn()
end)


op(llint_throw_from_slow_path_trampoline, macro()
    callSlowPath(_llint_slow_path_handle_exception)

    # When throwing from the interpreter (i.e. throwing from LLIntSlowPaths), so
    # the throw target is not necessarily interpreted code, we come to here.
    # This essentially emulates the JIT's throwing protocol.
    loadp Callee[cfr], t1
    andp MarkedBlockMask, t1
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t1
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(t1, t2)
    jmp VM::targetMachinePCForThrow[t1]
end)


op(llint_throw_during_call_trampoline, macro()
    preserveReturnAddressAfterCall(t2)
    jmp _llint_throw_from_slow_path_trampoline
end)


macro nativeCallTrampoline(executableOffsetToFunction)

    functionPrologue()
    storep 0, CodeBlock[cfr]
    loadi Callee + PayloadOffset[cfr], t1
    // Callee is still in t1 for code below
    if X86 or X86_WIN
        subp 8, sp # align stack pointer
        andp MarkedBlockMask, t1
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t3
        storep cfr, VM::topCallFrame[t3]
        move cfr, a0  # a0 = ecx
        storep a0, [sp]
        loadi Callee + PayloadOffset[cfr], t1
        loadp JSFunction::m_executable[t1], t1
        checkStackPointerAlignment(t3, 0xdead0001)
        call executableOffsetToFunction[t1]
        loadp Callee + PayloadOffset[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
        addp 8, sp
    elsif ARM or ARMv7 or ARMv7_TRADITIONAL or C_LOOP or MIPS
        if MIPS
        # calling convention says to save stack space for 4 first registers in
        # all cases. To match our 16-byte alignment, that means we need to
        # take 24 bytes
            subp 24, sp
        else
            subp 8, sp # align stack pointer
        end
        # t1 already contains the Callee.
        andp MarkedBlockMask, t1
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t1
        storep cfr, VM::topCallFrame[t1]
        move cfr, a0
        loadi Callee + PayloadOffset[cfr], t1
        loadp JSFunction::m_executable[t1], t1
        checkStackPointerAlignment(t3, 0xdead0001)
        if C_LOOP
            cloopCallNative executableOffsetToFunction[t1]
        else
            call executableOffsetToFunction[t1]
        end
        loadp Callee + PayloadOffset[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
        if MIPS
            addp 24, sp
        else
            addp 8, sp
        end
    else
        error
    end
    
    btinz VM::m_exception[t3], .handleException

    functionEpilogue()
    ret

.handleException:
if X86 or X86_WIN
    subp 8, sp # align stack pointer
end
    storep cfr, VM::topCallFrame[t3]
    jmp _llint_throw_from_slow_path_trampoline
end


macro internalFunctionCallTrampoline(offsetOfFunction)
    functionPrologue()
    storep 0, CodeBlock[cfr]
    loadi Callee + PayloadOffset[cfr], t1
    // Callee is still in t1 for code below
    if X86 or X86_WIN
        subp 8, sp # align stack pointer
        andp MarkedBlockMask, t1
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t3
        storep cfr, VM::topCallFrame[t3]
        move cfr, a0  # a0 = ecx
        storep a0, [sp]
        loadi Callee + PayloadOffset[cfr], t1
        checkStackPointerAlignment(t3, 0xdead0001)
        call offsetOfFunction[t1]
        loadp Callee + PayloadOffset[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
        addp 8, sp
    elsif ARM or ARMv7 or ARMv7_TRADITIONAL or C_LOOP or MIPS
        subp 8, sp # align stack pointer
        # t1 already contains the Callee.
        andp MarkedBlockMask, t1
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t1
        storep cfr, VM::topCallFrame[t1]
        move cfr, a0
        loadi Callee + PayloadOffset[cfr], t1
        checkStackPointerAlignment(t3, 0xdead0001)
        if C_LOOP
            cloopCallNative offsetOfFunction[t1]
        else
            call offsetOfFunction[t1]
        end
        loadp Callee + PayloadOffset[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
        addp 8, sp
    else
        error
    end

    btinz VM::m_exception[t3], .handleException

    functionEpilogue()
    ret

.handleException:
if X86 or X86_WIN
    subp 8, sp # align stack pointer
end
    storep cfr, VM::topCallFrame[t3]
    jmp _llint_throw_from_slow_path_trampoline
end


macro varInjectionCheck(slowPath)
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_varInjectionWatchpoint[t0], t0
    bbeq WatchpointSet::m_state[t0], IsInvalidated, slowPath
end


llintOpWithMetadata(op_resolve_scope, OpResolveScope, macro (size, get, dispatch, metadata, return)

    macro getConstantScope()
        loadp OpResolveScope::Metadata::constantScope[t5],  t0
        return(CellTag, t0)
    end

    macro resolveScope()
        loadi OpResolveScope::Metadata::localScopeDepth[t5], t2
        get(scope, t0)
        loadp PayloadOffset[cfr, t0, 8], t0
        btiz t2, .resolveScopeLoopEnd

    .resolveScopeLoop:
        loadp JSScope::m_next[t0], t0
        subi 1, t2
        btinz t2, .resolveScopeLoop

    .resolveScopeLoopEnd:
        return(CellTag, t0)
    end

    metadata(t5, t0)
    loadp OpResolveScope::Metadata::resolveType[t5], t0

#rGlobalProperty:
    bineq t0, GlobalProperty, .rGlobalVar
    getConstantScope()

.rGlobalVar:
    bineq t0, GlobalVar, .rGlobalLexicalVar
    getConstantScope()

.rGlobalLexicalVar:
    bineq t0, GlobalLexicalVar, .rClosureVar
    getConstantScope()

.rClosureVar:
    bineq t0, ClosureVar, .rModuleVar
    resolveScope()

.rModuleVar:
    bineq t0, ModuleVar, .rGlobalPropertyWithVarInjectionChecks
    getConstantScope()

.rGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .rGlobalVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic)
    getConstantScope()

.rGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .rGlobalLexicalVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic)
    getConstantScope()

.rGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .rClosureVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic)
    getConstantScope()

.rClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .rDynamic
    varInjectionCheck(.rDynamic)
    resolveScope()

.rDynamic:
    callSlowPath(_slow_path_resolve_scope)
    dispatch()
end)


macro loadWithStructureCheck(op, get, operand, slowPath)
    get(scope, t0)
    loadp PayloadOffset[cfr, t0, 8], t0
    loadp %op%::Metadata::structure[t5], t1
    bpneq JSCell::m_structureID[t0], t1, slowPath
end


llintOpWithMetadata(op_get_from_scope, OpGetFromScope, macro (size, get, dispatch, metadata, return)
    macro getProperty()
        loadis OpGetFromScope::Metadata::operand[t5], t3
        loadPropertyAtVariableOffset(t3, t0, t1, t2)
        valueProfile(OpGetFromScope, t5, t1, t2)
        return(t1, t2)
    end

    macro getGlobalVar(tdzCheckIfNecessary)
        loadp OpGetFromScope::Metadata::operand[t5], t0
        loadp TagOffset[t0], t1
        loadp PayloadOffset[t0], t2
        tdzCheckIfNecessary(t1)
        valueProfile(OpGetFromScope, t5, t1, t2)
        return(t1, t2)
    end

    macro getClosureVar()
        loadis OpGetFromScope::Metadata::operand[t5], t3
        loadp JSLexicalEnvironment_variables + TagOffset[t0, t3, 8], t1
        loadp JSLexicalEnvironment_variables + PayloadOffset[t0, t3, 8], t2
        valueProfile(OpGetFromScope, t5, t1, t2)
        return(t1, t2)
    end

    metadata(t5, t0)
    loadi OpGetFromScope::Metadata::getPutInfo[t5], t0
    andi ResolveTypeMask, t0

#gGlobalProperty:
    bineq t0, GlobalProperty, .gGlobalVar
    loadWithStructureCheck(OpGetFromScope, get, scope, .gDynamic)
    getProperty()

.gGlobalVar:
    bineq t0, GlobalVar, .gGlobalLexicalVar
    getGlobalVar(macro(t) end)

.gGlobalLexicalVar:
    bineq t0, GlobalLexicalVar, .gClosureVar
    getGlobalVar(
        macro(tag)
            bieq tag, EmptyValueTag, .gDynamic
        end)

.gClosureVar:
    bineq t0, ClosureVar, .gGlobalPropertyWithVarInjectionChecks
    loadVariable(get, scope, t2, t1, t0)
    getClosureVar()

.gGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .gGlobalVarWithVarInjectionChecks
    loadWithStructureCheck(OpGetFromScope, get, scope, .gDynamic)
    getProperty()

.gGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .gGlobalLexicalVarWithVarInjectionChecks
    varInjectionCheck(.gDynamic)
    getGlobalVar(macro(t) end)

.gGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .gClosureVarWithVarInjectionChecks
    varInjectionCheck(.gDynamic)
    getGlobalVar(
        macro(tag)
            bieq tag, EmptyValueTag, .gDynamic
        end)

.gClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .gDynamic
    varInjectionCheck(.gDynamic)
    loadVariable(get, scope, t2, t1, t0)
    getClosureVar()

.gDynamic:
    callSlowPath(_llint_slow_path_get_from_scope)
    dispatch()
end)


llintOpWithMetadata(op_put_to_scope, OpPutToScope, macro (size, get, dispatch, metadata, return)
    macro putProperty()
        get(value, t1)
        loadConstantOrVariable(size, t1, t2, t3)
        loadis OpPutToScope::Metadata::operand[t5], t1
        storePropertyAtVariableOffset(t1, t0, t2, t3)
    end

    macro putGlobalVariable()
        get(value, t0)
        loadConstantOrVariable(size, t0, t1, t2)
        loadp OpPutToScope::Metadata::watchpointSet[t5], t3
        notifyWrite(t3, .pDynamic)
        loadp OpPutToScope::Metadata::operand[t5], t0
        storei t1, TagOffset[t0]
        storei t2, PayloadOffset[t0]
    end

    macro putClosureVar()
        get(value, t1)
        loadConstantOrVariable(size, t1, t2, t3)
        loadis OpPutToScope::Metadata::operand[t5], t1
        storei t2, JSLexicalEnvironment_variables + TagOffset[t0, t1, 8]
        storei t3, JSLexicalEnvironment_variables + PayloadOffset[t0, t1, 8]
    end

    macro putLocalClosureVar()
        get(value, t1)
        loadConstantOrVariable(size, t1, t2, t3)
        loadp OpPutToScope::Metadata::watchpointSet[t5], t1
        btpz t1, .noVariableWatchpointSet
        notifyWrite(t1, .pDynamic)
    .noVariableWatchpointSet:
        loadis OpPutToScope::Metadata::operand[t5], t1
        storei t2, JSLexicalEnvironment_variables + TagOffset[t0, t1, 8]
        storei t3, JSLexicalEnvironment_variables + PayloadOffset[t0, t1, 8]
    end


    metadata(t5, t0)
    loadi OpPutToScope::Metadata::getPutInfo[t5], t0
    andi ResolveTypeMask, t0

#pLocalClosureVar:
    bineq t0, LocalClosureVar, .pGlobalProperty
    loadVariable(get, scope, t2, t1, t0)
    putLocalClosureVar()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pGlobalProperty:
    bineq t0, GlobalProperty, .pGlobalVar
    loadWithStructureCheck(OpPutToScope, get, scope, .pDynamic)
    putProperty()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pGlobalVar:
    bineq t0, GlobalVar, .pGlobalLexicalVar
    putGlobalVariable()
    writeBarrierOnGlobalObject(size, get, value)
    dispatch()

.pGlobalLexicalVar:
    bineq t0, GlobalLexicalVar, .pClosureVar
    putGlobalVariable()
    writeBarrierOnGlobalLexicalEnvironment(size, get, value)
    dispatch()

.pClosureVar:
    bineq t0, ClosureVar, .pGlobalPropertyWithVarInjectionChecks
    loadVariable(get, scope, t2, t1, t0)
    putClosureVar()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .pGlobalVarWithVarInjectionChecks
    loadWithStructureCheck(OpPutToScope, get, scope, .pDynamic)
    putProperty()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .pGlobalLexicalVarWithVarInjectionChecks
    varInjectionCheck(.pDynamic)
    putGlobalVariable()
    writeBarrierOnGlobalObject(size, get, value)
    dispatch()

.pGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .pClosureVarWithVarInjectionChecks
    varInjectionCheck(.pDynamic)
    putGlobalVariable()
    writeBarrierOnGlobalLexicalEnvironment(size, get, value)
    dispatch()

.pClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .pModuleVar
    varInjectionCheck(.pDynamic)
    loadVariable(get, scope, t2, t1, t0)
    putClosureVar()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pModuleVar:
    bineq t0, ModuleVar, .pDynamic
    callSlowPath(_slow_path_throw_strict_mode_readonly_property_write_error)
    dispatch()

.pDynamic:
    callSlowPath(_llint_slow_path_put_to_scope)
    dispatch()
end)


llintOpWithProfile(op_get_from_arguments, OpGetFromArguments, macro (size, get, dispatch, return)
    get(arguments, t0)
    loadi PayloadOffset[cfr, t0, 8], t0
    get(index, t1)
    loadi DirectArguments_storage + TagOffset[t0, t1, 8], t2
    loadi DirectArguments_storage + PayloadOffset[t0, t1, 8], t3
    return(t2, t3)
end)


llintOp(op_put_to_arguments, OpPutToArguments, macro (size, get, dispatch)
    writeBarrierOnOperands(size, get, arguments, value)
    get(arguments, t0)
    loadi PayloadOffset[cfr, t0, 8], t0
    get(value, t1)
    loadConstantOrVariable(size, t1, t2, t3)
    get(index, t1)
    storei t2, DirectArguments_storage + TagOffset[t0, t1, 8]
    storei t3, DirectArguments_storage + PayloadOffset[t0, t1, 8]
    dispatch()
end)


llintOpWithReturn(op_get_parent_scope, OpGetParentScope, macro (size, get, dispatch, return)
    get(scope, t0)
    loadp PayloadOffset[cfr, t0, 8], t0
    loadp JSScope::m_next[t0], t0
    return(CellTag, t0)
end)


llintOpWithMetadata(op_profile_type, OpProfileType, macro (size, get, dispatch, metadata, return)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_poisonedVM[t1], t1
    unpoison(_g_CodeBlockPoison, t1, t2)
    # t1 is holding the pointer to the typeProfilerLog.
    loadp VM::m_typeProfilerLog[t1], t1

    # t0 is holding the payload, t5 is holding the tag.
    get(target, t2)
    loadConstantOrVariable(size, t2, t5, t0)

    bieq t5, EmptyValueTag, .opProfileTypeDone

    metadata(t3, t2)
    # t2 is holding the pointer to the current log entry.
    loadp TypeProfilerLog::m_currentLogEntryPtr[t1], t2

    # Store the JSValue onto the log entry.
    storei t5, TypeProfilerLog::LogEntry::value + TagOffset[t2]
    storei t0, TypeProfilerLog::LogEntry::value + PayloadOffset[t2]

    # Store the TypeLocation onto the log entry.
    loadp OpProfileType::Metadata::typeLocation[t3], t3
    storep t3, TypeProfilerLog::LogEntry::location[t2]

    bieq t5, CellTag, .opProfileTypeIsCell
    storei 0, TypeProfilerLog::LogEntry::structureID[t2]
    jmp .opProfileTypeSkipIsCell
.opProfileTypeIsCell:
    loadi JSCell::m_structureID[t0], t3
    storei t3, TypeProfilerLog::LogEntry::structureID[t2]
.opProfileTypeSkipIsCell:
    
    # Increment the current log entry.
    addp sizeof TypeProfilerLog::LogEntry, t2
    storep t2, TypeProfilerLog::m_currentLogEntryPtr[t1]

    loadp TypeProfilerLog::m_logEndPtr[t1], t1
    bpneq t2, t1, .opProfileTypeDone
    callSlowPath(_slow_path_profile_type_clear_log)

.opProfileTypeDone:
    dispatch()
end)


llintOpWithMetadata(op_profile_control_flow, OpProfileControlFlow, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)
    loadp OpProfileControlFlow::Metadata::basicBlockLocation[t5], t0
    loadi BasicBlockLocation::m_executionCount[t0], t1
    baddio 1, t1, .done
    storei t1, BasicBlockLocation::m_executionCount[t0]
.done:
    dispatch()
end)


llintOpWithReturn(op_get_rest_length, OpGetRestLength, macro (size, get, dispatch, return)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    subi 1, t0
    get(numParametersToSkip, t1)
    bilteq t0, t1, .storeZero
    subi t1, t0
    jmp .finish
.storeZero:
    move 0, t0
.finish:
    return(Int32Tag, t0)
end)


llintOp(op_log_shadow_chicken_prologue, OpLogShadowChickenPrologue, macro (size, get, dispatch)
    acquireShadowChickenPacket(.opLogShadowChickenPrologueSlow)
    storep cfr, ShadowChicken::Packet::frame[t0]
    loadp CallerFrame[cfr], t1
    storep t1, ShadowChicken::Packet::callerFrame[t0]
    loadp Callee + PayloadOffset[cfr], t1
    storep t1, ShadowChicken::Packet::callee[t0]
    get(scope, t1)
    loadi PayloadOffset[cfr, t1, 8], t1
    storep t1, ShadowChicken::Packet::scope[t0]
    dispatch()
.opLogShadowChickenPrologueSlow:
    callSlowPath(_llint_slow_path_log_shadow_chicken_prologue)
    dispatch()
end)


llintOp(op_log_shadow_chicken_tail, OpLogShadowChickenTail, macro (size, get, dispatch)
    acquireShadowChickenPacket(.opLogShadowChickenTailSlow)
    storep cfr, ShadowChicken::Packet::frame[t0]
    storep ShadowChickenTailMarker, ShadowChicken::Packet::callee[t0]
    loadVariable(get, thisValue, t3, t2, t1)
    storei t2, TagOffset + ShadowChicken::Packet::thisValue[t0]
    storei t1, PayloadOffset + ShadowChicken::Packet::thisValue[t0]
    get(scope, t1)
    loadi PayloadOffset[cfr, t1, 8], t1
    storep t1, ShadowChicken::Packet::scope[t0]
    loadp CodeBlock[cfr], t1
    storep t1, ShadowChicken::Packet::codeBlock[t0]
    storei PC, ShadowChicken::Packet::callSiteIndex[t0]
    dispatch()
.opLogShadowChickenTailSlow:
    callSlowPath(_llint_slow_path_log_shadow_chicken_tail)
    dispatch()
end)
