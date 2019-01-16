
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


# Utilities.

macro nextInstruction()
    loadb [PB, PC, 1], t0
    leap _g_opcodeMap, t1
    jmp [t1, t0, PtrSize], BytecodePtrTag
end

macro nextInstructionWide()
    loadi 1[PB, PC, 1], t0
    leap _g_opcodeMapWide, t1
    jmp [t1, t0, PtrSize], BytecodePtrTag
end

macro getuOperandNarrow(op, field, dst)
    loadb constexpr %op%_%field%_index[PB, PC, 1], dst
end

macro getOperandNarrow(op, field, dst)
    loadbsp constexpr %op%_%field%_index[PB, PC, 1], dst
end

macro getuOperandWide(op, field, dst)
    loadi constexpr %op%_%field%_index * 4 + 1[PB, PC, 1], dst
end

macro getOperandWide(op, field, dst)
    loadis constexpr %op%_%field%_index * 4 + 1[PB, PC, 1], dst
end

macro makeReturn(get, dispatch, fn)
    fn(macro (value)
        move value, t2
        get(dst, t1)
        storeq t2, [cfr, t1, 8]
        dispatch()
    end)
end

macro makeReturnProfiled(op, get, metadata, dispatch, fn)
    fn(macro (value)
        move value, t3
        metadata(t1, t2)
        valueProfile(op, t1, t3)
        get(dst, t1)
        storeq t3, [cfr, t1, 8]
        dispatch()
    end)
end

macro valueProfile(op, metadata, value)
    storeq value, %op%::Metadata::profile.m_buckets[metadata]
end

macro dispatchAfterCall(size, op, dispatch)
    loadi ArgumentCount + TagOffset[cfr], PC
    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_instructionsRawPointer[PB], PB
    unpoison(_g_CodeBlockPoison, PB, t1)
    get(size, op, dst, t1)
    storeq r0, [cfr, t1, 8]
    metadata(size, op, t2, t1)
    valueProfile(op, t2, r0)
    dispatch()
end

macro cCall2(function)
    checkStackPointerAlignment(t4, 0xbad0c002)
    if X86_64 or ARM64 or ARM64E
        call function
    elsif X86_64_WIN
        # Note: this implementation is only correct if the return type size is > 8 bytes.
        # See macro cCall2Void for an implementation when the return type <= 8 bytes.
        # On Win64, when the return type is larger than 8 bytes, we need to allocate space on the stack for the return value.
        # On entry rcx (a0), should contain a pointer to this stack space. The other parameters are shifted to the right,
        # rdx (a1) should contain the first argument, and r8 (a2) should contain the second argument.
        # On return, rax contains a pointer to this stack value, and we then need to copy the 16 byte return value into rax (r0) and rdx (r1)
        # since the return value is expected to be split between the two.
        # See http://msdn.microsoft.com/en-us/library/7572ztz4.aspx
        move a1, a2
        move a0, a1
        subp 48, sp
        move sp, a0
        addp 32, a0
        call function
        addp 48, sp
        move 8[r0], r1
        move [r0], r0
    elsif C_LOOP
        cloopCallSlowPath function, a0, a1
    else
        error
    end
end

macro cCall2Void(function)
    if C_LOOP
        cloopCallSlowPathVoid function, a0, a1
    elsif X86_64_WIN
        # Note: we cannot use the cCall2 macro for Win64 in this case,
        # as the Win64 cCall2 implemenation is only correct when the return type size is > 8 bytes.
        # On Win64, rcx and rdx are used for passing the first two parameters.
        # We also need to make room on the stack for all four parameter registers.
        # See http://msdn.microsoft.com/en-us/library/ms235286.aspx
        subp 32, sp 
        call function
        addp 32, sp
    else
        cCall2(function)
    end
end

# This barely works. arg3 and arg4 should probably be immediates.
macro cCall4(function)
    checkStackPointerAlignment(t4, 0xbad0c004)
    if X86_64 or ARM64 or ARM64E
        call function
    elsif X86_64_WIN
        # On Win64, rcx, rdx, r8, and r9 are used for passing the first four parameters.
        # We also need to make room on the stack for all four parameter registers.
        # See http://msdn.microsoft.com/en-us/library/ms235286.aspx
        subp 64, sp
        call function
        addp 64, sp
    else
        error
    end
end

macro doVMEntry(makeCall)
    functionPrologue()
    pushCalleeSaves()

    const entry = a0
    const vm = a1
    const protoCallFrame = a2

    vmEntryRecord(cfr, sp)

    checkStackPointerAlignment(t4, 0xbad0dc01)

    storep vm, VMEntryRecord::m_vm[sp]
    loadp VM::topCallFrame[vm], t4
    storep t4, VMEntryRecord::m_prevTopCallFrame[sp]
    loadp VM::topEntryFrame[vm], t4
    storep t4, VMEntryRecord::m_prevTopEntryFrame[sp]
    loadp ProtoCallFrame::calleeValue[protoCallFrame], t4
    storep t4, VMEntryRecord::m_callee[sp]

    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], t4
    addp CallFrameHeaderSlots, t4, t4
    lshiftp 3, t4
    subp sp, t4, t3
    bqbeq sp, t3, .throwStackOverflow

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
    move (constexpr ProtoCallFrame::numberOfRegisters), t3

.copyHeaderLoop:
    # Copy the CodeBlock/Callee/ArgumentCount/|this| from protoCallFrame into the callee frame.
    subi 1, t3
    loadq [protoCallFrame, t3, 8], extraTempReg
    storeq extraTempReg, CodeBlock[sp, t3, 8]
    btinz t3, .copyHeaderLoop

    loadi PayloadOffset + ProtoCallFrame::argCountAndCodeOriginValue[protoCallFrame], t4
    subi 1, t4
    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], extraTempReg
    subi 1, extraTempReg

    bieq t4, extraTempReg, .copyArgs
    move ValueUndefined, t3
.fillExtraArgsLoop:
    subi 1, extraTempReg
    storeq t3, ThisArgumentOffset + 8[sp, extraTempReg, 8]
    bineq t4, extraTempReg, .fillExtraArgsLoop

.copyArgs:
    loadp ProtoCallFrame::args[protoCallFrame], t3

.copyArgsLoop:
    btiz t4, .copyArgsDone
    subi 1, t4
    loadq [t3, t4, 8], extraTempReg
    storeq extraTempReg, ThisArgumentOffset + 8[sp, t4, 8]
    jmp .copyArgsLoop

.copyArgsDone:
    if ARM64 or ARM64E
        move sp, t4
        storep t4, VM::topCallFrame[vm]
    else
        storep sp, VM::topCallFrame[vm]
    end
    storep cfr, VM::topEntryFrame[vm]

    checkStackPointerAlignment(extraTempReg, 0xbad0dc02)

    makeCall(entry, t3, t4)

    # We may have just made a call into a JS function, so we can't rely on sp
    # for anything but the fact that our own locals (ie the VMEntryRecord) are
    # not below it. It also still has to be aligned, though.
    checkStackPointerAlignment(t2, 0xbad0dc03)

    vmEntryRecord(cfr, t4)

    loadp VMEntryRecord::m_vm[t4], vm
    loadp VMEntryRecord::m_prevTopCallFrame[t4], t2
    storep t2, VM::topCallFrame[vm]
    loadp VMEntryRecord::m_prevTopEntryFrame[t4], t2
    storep t2, VM::topEntryFrame[vm]

    subp cfr, CalleeRegisterSaveSize, sp

    popCalleeSaves()
    functionEpilogue()
    ret

.throwStackOverflow:
    move vm, a0
    move protoCallFrame, a1
    cCall2(_llint_throw_stack_overflow_error)

    vmEntryRecord(cfr, t4)

    loadp VMEntryRecord::m_vm[t4], vm
    loadp VMEntryRecord::m_prevTopCallFrame[t4], extraTempReg
    storep extraTempReg, VM::topCallFrame[vm]
    loadp VMEntryRecord::m_prevTopEntryFrame[t4], extraTempReg
    storep extraTempReg, VM::topEntryFrame[vm]

    subp cfr, CalleeRegisterSaveSize, sp

    popCalleeSaves()
    functionEpilogue()
    ret
end


macro makeJavaScriptCall(entry, temp, unused)
    addp 16, sp
    if C_LOOP
        cloopCallJSFunction entry
    else
        call entry, JSEntryPtrTag
    end
    subp 16, sp
end

macro makeHostFunctionCall(entry, temp, unused)
    move entry, temp
    storep cfr, [sp]
    move sp, a0
    if C_LOOP
        storep lr, 8[sp]
        cloopCallNative temp
    elsif X86_64_WIN
        # We need to allocate 32 bytes on the stack for the shadow space.
        subp 32, sp
        call temp, JSEntryPtrTag
        addp 32, sp
    else
        call temp, JSEntryPtrTag
    end
end

op(handleUncaughtException, macro ()
    loadp Callee[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)
    storep 0, VM::callFrameForCatch[t3]

    loadp VM::topEntryFrame[t3], cfr
    vmEntryRecord(cfr, t2)

    loadp VMEntryRecord::m_vm[t2], t3
    loadp VMEntryRecord::m_prevTopCallFrame[t2], extraTempReg
    storep extraTempReg, VM::topCallFrame[t3]
    loadp VMEntryRecord::m_prevTopEntryFrame[t2], extraTempReg
    storep extraTempReg, VM::topEntryFrame[t3]

    subp cfr, CalleeRegisterSaveSize, sp

    popCalleeSaves()
    functionEpilogue()
    ret
end)


macro prepareStateForCCall()
    addp PB, PC
end

macro restoreStateAfterCCall()
    move r0, PC
    subp PB, PC
end

macro callSlowPath(slowPath)
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    cCall2(slowPath)
    restoreStateAfterCCall()
end

macro traceOperand(fromWhere, operand)
    prepareStateForCCall()
    move fromWhere, a2
    move operand, a3
    move cfr, a0
    move PC, a1
    cCall4(_llint_trace_operand)
    restoreStateAfterCCall()
end

macro traceValue(fromWhere, operand)
    prepareStateForCCall()
    move fromWhere, a2
    move operand, a3
    move cfr, a0
    move PC, a1
    cCall4(_llint_trace_value)
    restoreStateAfterCCall()
end

# Call a slow path for call call opcodes.
macro callCallSlowPath(slowPath, action)
    storei PC, ArgumentCount + TagOffset[cfr]
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    cCall2(slowPath)
    action(r0, r1)
end

macro callTrapHandler(throwHandler)
    storei PC, ArgumentCount + TagOffset[cfr]
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    cCall2(_llint_slow_path_handle_traps)
    btpnz r0, throwHandler
    loadi ArgumentCount + TagOffset[cfr], PC
end

macro checkSwitchToJITForLoop()
    checkSwitchToJIT(
        1,
        macro()
            storei PC, ArgumentCount + TagOffset[cfr]
            prepareStateForCCall()
            move cfr, a0
            move PC, a1
            cCall2(_llint_loop_osr)
            btpz r0, .recover
            move r1, sp
            jmp r0, JSEntryPtrTag
        .recover:
            loadi ArgumentCount + TagOffset[cfr], PC
        end)
end

macro uncage(basePtr, mask, ptr, scratch)
    if GIGACAGE_ENABLED and not C_LOOP
        loadp basePtr, scratch
        btpz scratch, .done
        andp mask, ptr
        addp scratch, ptr
    .done:
    end
end

macro loadCaged(basePtr, mask, source, dest, scratch)
    loadp source, dest
    uncage(basePtr, mask, dest, scratch)
end

macro loadVariable(get, field, value)
    get(field, value)
    loadq [cfr, value, 8], value
end

# Index and value must be different registers. Index may be clobbered.
macro loadConstantOrVariable(size, index, value)
    macro loadNarrow()
        bpgteq index, FirstConstantRegisterIndexNarrow, .constant
        loadq [cfr, index, 8], value
        jmp .done
    .constant:
        loadp CodeBlock[cfr], value
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[value], value
        loadq -(FirstConstantRegisterIndexNarrow * 8)[value, index, 8], value
    .done:
    end

    macro loadWide()
        bpgteq index, FirstConstantRegisterIndexWide, .constant
        loadq [cfr, index, 8], value
        jmp .done
    .constant:
        loadp CodeBlock[cfr], value
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[value], value
        subp FirstConstantRegisterIndexWide, index
        loadq [value, index, 8], value
    .done:
    end

    size(loadNarrow, loadWide, macro (load) load() end)
end

macro loadConstantOrVariableInt32(size, index, value, slow)
    loadConstantOrVariable(size, index, value)
    bqb value, tagTypeNumber, slow
end

macro loadConstantOrVariableCell(size, index, value, slow)
    loadConstantOrVariable(size, index, value)
    btqnz value, tagMask, slow
end

macro writeBarrierOnOperandWithReload(size, get, cellOperand, reloadAfterSlowPath)
    get(cellOperand, t1)
    loadConstantOrVariableCell(size, t1, t2, .writeBarrierDone)
    skipIfIsRememberedOrInEden(
        t2,
        macro()
            push PB, PC
            move t2, a1 # t2 can be a0 (not on 64 bits, but better safe than sorry)
            move cfr, a0
            cCall2Void(_llint_write_barrier_slow)
            pop PC, PB
            reloadAfterSlowPath()
        end)
.writeBarrierDone:
end

macro writeBarrierOnOperand(size, get, cellOperand)
    writeBarrierOnOperandWithReload(size, get, cellOperand, macro () end)
end

macro writeBarrierOnOperands(size, get, cellOperand, valueOperand)
    get(valueOperand, t1)
    loadConstantOrVariableCell(size, t1, t0, .writeBarrierDone)
    btpz t0, .writeBarrierDone

    writeBarrierOnOperand(size, get, cellOperand)
.writeBarrierDone:
end

macro writeBarrierOnGlobal(size, get, valueOperand, loadHelper)
    get(valueOperand, t1)
    loadConstantOrVariableCell(size, t1, t0, .writeBarrierDone)
    btpz t0, .writeBarrierDone

    loadHelper(t3)
    skipIfIsRememberedOrInEden(
        t3,
        macro()
            push PB, PC
            move cfr, a0
            move t3, a1
            cCall2Void(_llint_write_barrier_slow)
            pop PC, PB
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

macro structureIDToStructureWithScratch(structureIDThenStructure, scratch, scratch2)
    loadp CodeBlock[cfr], scratch
    loadp CodeBlock::m_poisonedVM[scratch], scratch
    unpoison(_g_CodeBlockPoison, scratch, scratch2)
    loadp VM::heap + Heap::m_structureIDTable + StructureIDTable::m_table[scratch], scratch
    loadp [scratch, structureIDThenStructure, PtrSize], structureIDThenStructure
end

macro loadStructureWithScratch(cell, structure, scratch, scratch2)
    loadi JSCell::m_structureID[cell], structure
    structureIDToStructureWithScratch(structure, scratch, scratch2)
end

# Entrypoints into the interpreter.

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
macro functionArityCheck(doneLabel, slowPath)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    biaeq t0, CodeBlock::m_numParameters[t1], doneLabel
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    cCall2(slowPath)   # This slowPath has the protocol: r0 = 0 => no error, r0 != 0 => error
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
    move ValueUndefined, t0
.fillExtraSlots:
    storeq t0, [cfr, t2, 8]
    addi 1, t2
    bsubinz 1, t3, .fillExtraSlots
    andi ~(StackAlignmentSlots - 1), t1
    btiz t1, .continue

.noExtraSlot:
    if POINTER_PROFILING
        if ARM64 or ARM64E
            loadp 8[cfr], lr
        end

        addp 16, cfr, t3
        untagReturnAddress t3
    end

    // Move frame up t1 slots
    negq t1
    move cfr, t3
    subp CalleeSaveSpaceAsVirtualRegisters * 8, t3
    addi CalleeSaveSpaceAsVirtualRegisters, t2
    move t1, t0
    # Adds to sp are always 64-bit on arm64 so we need maintain t0's high bits.
    lshiftq 3, t0
    addp t0, cfr
    addp t0, sp
.copyLoop:
    loadq [t3], t0
    storeq t0, [t3, t1, 8]
    addp 8, t3
    bsubinz 1, t2, .copyLoop

    // Fill new slots with JSUndefined
    move t1, t2
    move ValueUndefined, t0
.fillLoop:
    storeq t0, [t3, t1, 8]
    addp 8, t3
    baddinz 1, t2, .fillLoop

    if POINTER_PROFILING
        addp 16, cfr, t1
        tagReturnAddress t1

        if ARM64 or ARM64E
            storep lr, 8[cfr]
        end
    end

.continue:
    # Reload CodeBlock and reset PC, since the slow_path clobbered them.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_instructionsRawPointer[t1], PB
    unpoison(_g_CodeBlockPoison, PB, t2)
    move 0, PC
    jmp doneLabel
end

macro branchIfException(label)
    loadp Callee[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
    btpz VM::m_exception[t3], .noException
    jmp label
.noException:
end

# Instruction implementations
_llint_op_enter:
    traceExecution()
    checkStackPointerAlignment(t2, 0xdead00e1)
    loadp CodeBlock[cfr], t2                // t2<CodeBlock> = cfr.CodeBlock
    loadi CodeBlock::m_numVars[t2], t2      // t2<size_t> = t2<CodeBlock>.m_numVars
    subq CalleeSaveSpaceAsVirtualRegisters, t2
    move cfr, t1
    subq CalleeSaveSpaceAsVirtualRegisters * 8, t1
    btiz t2, .opEnterDone
    move ValueUndefined, t0
    negi t2
    sxi2q t2, t2
.opEnterLoop:
    storeq t0, [t1, t2, 8]
    addq 1, t2
    btqnz t2, .opEnterLoop
.opEnterDone:
    callSlowPath(_slow_path_enter)
    dispatchOp(narrow, op_enter)


llintOpWithProfile(op_get_argument, OpGetArgument, macro (size, get, dispatch, return)
    get(index, t2)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    bilteq t0, t2, .opGetArgumentOutOfBounds
    loadq ThisArgumentOffset[cfr, t2, 8], t0
    return(t0)

.opGetArgumentOutOfBounds:
    return(ValueUndefined)
end)


llintOpWithReturn(op_argument_count, OpArgumentCount, macro (size, get, dispatch, return)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    subi 1, t0
    orq TagTypeNumber, t0
    return(t0)
end)


llintOpWithReturn(op_get_scope, OpGetScope, macro (size, get, dispatch, return)
    loadp Callee[cfr], t0
    loadp JSCallee::m_scope[t0], t0
    return(t0)
end)


llintOpWithMetadata(op_to_this, OpToThis, macro (size, get, dispatch, metadata, return)
    get(srcDst, t0)
    loadq [cfr, t0, 8], t0
    btqnz t0, tagMask, .opToThisSlow
    bbneq JSCell::m_type[t0], FinalObjectType, .opToThisSlow
    loadStructureWithScratch(t0, t1, t2, t3)
    metadata(t2, t3)
    loadp OpToThis::Metadata::cachedStructure[t2], t2
    bpneq t1, t2, .opToThisSlow
    dispatch()

.opToThisSlow:
    callSlowPath(_slow_path_to_this)
    dispatch()
end)


llintOp(op_check_tdz, OpCheckTdz, macro (size, get, dispatch)
    get(target, t0)
    loadConstantOrVariable(size, t0, t1)
    bqneq t1, ValueEmpty, .opNotTDZ
    callSlowPath(_slow_path_throw_tdz_error)

.opNotTDZ:
    dispatch()
end)


llintOpWithReturn(op_mov, OpMov, macro (size, get, dispatch, return)
    get(src, t1)
    loadConstantOrVariable(size, t1, t2)
    return(t2)
end)


llintOpWithReturn(op_not, OpNot, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t2)
    xorq ValueFalse, t2
    btqnz t2, ~1, .opNotSlow
    xorq ValueTrue, t2
    return(t2)

.opNotSlow:
    callSlowPath(_slow_path_not)
    dispatch()
end)


macro equalityComparisonOp(name, op, integerComparison)
    llintOpWithReturn(op_%name%, op, macro (size, get, dispatch, return)
        get(rhs, t0)
        get(lhs, t2)
        loadConstantOrVariableInt32(size, t0, t1, .slow)
        loadConstantOrVariableInt32(size, t2, t0, .slow)
        integerComparison(t0, t1, t0)
        orq ValueFalse, t0
        return(t0)

    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end


macro equalNullComparisonOp(name, op, fn)
    llintOpWithReturn(name, op, macro (size, get, dispatch, return)
        get(operand, t0)
        loadq [cfr, t0, 8], t0
        btqnz t0, tagMask, .immediate
        btbnz JSCell::m_flags[t0], MasqueradesAsUndefined, .masqueradesAsUndefined
        move 0, t0
        jmp .done
    .masqueradesAsUndefined:
        loadStructureWithScratch(t0, t2, t1, t3)
        loadp CodeBlock[cfr], t0
        loadp CodeBlock::m_globalObject[t0], t0
        cpeq Structure::m_globalObject[t2], t0, t0
        jmp .done
    .immediate:
        andq ~TagBitUndefined, t0
        cqeq t0, ValueNull, t0
    .done:
        fn(t0)
        return(t0)
    end)
end

equalNullComparisonOp(op_eq_null, OpEqNull,
    macro (value) orq ValueFalse, value end)


equalNullComparisonOp(op_neq_null, OpNeqNull,
    macro (value) xorq ValueTrue, value end)


llintOpWithReturn(op_is_undefined_or_null, OpIsUndefinedOrNull, macro (size, get, dispatch, return)
    get(operand, t0)
    loadq [cfr, t0, 8], t0
    andq ~TagBitUndefined, t0
    cqeq t0, ValueNull, t0
    orq ValueFalse, t0
    return(t0)
end)


macro strictEqOp(name, op, equalityOperation)
    llintOpWithReturn(op_%name%, op, macro (size, get, dispatch, return)
        get(rhs, t0)
        get(lhs, t2)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        move t0, t2
        orq t1, t2
        btqz t2, tagMask, .slow
        bqaeq t0, tagTypeNumber, .leftOK
        btqnz t0, tagTypeNumber, .slow
    .leftOK:
        bqaeq t1, tagTypeNumber, .rightOK
        btqnz t1, tagTypeNumber, .slow
    .rightOK:
        equalityOperation(t0, t1, t0)
        orq ValueFalse, t0
        return(t0)

    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end


strictEqOp(stricteq, OpStricteq,
    macro (left, right, result) cqeq left, right, result end)


strictEqOp(nstricteq, OpNstricteq,
    macro (left, right, result) cqneq left, right, result end)


macro strictEqualityJumpOp(name, op, equalityOperation)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(lhs, t2)
        get(rhs, t3)
        loadConstantOrVariable(size, t2, t0)
        loadConstantOrVariable(size, t3, t1)
        move t0, t2
        orq t1, t2
        btqz t2, tagMask, .slow
        bqaeq t0, tagTypeNumber, .leftOK
        btqnz t0, tagTypeNumber, .slow
    .leftOK:
        bqaeq t1, tagTypeNumber, .rightOK
        btqnz t1, tagTypeNumber, .slow
    .rightOK:
        equalityOperation(t0, t1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)

    .slow:
        callSlowPath(_llint_slow_path_%name%)
        nextInstruction()
    end)
end


strictEqualityJumpOp(jstricteq, OpJstricteq,
    macro (left, right, target) bqeq left, right, target end)


strictEqualityJumpOp(jnstricteq, OpJnstricteq,
    macro (left, right, target) bqneq left, right, target end)


macro preOp(name, op, arithmeticOperation)
    llintOp(op_%name%, op, macro (size, get, dispatch)
        get(srcDst, t0)
        loadq [cfr, t0, 8], t1
        bqb t1, tagTypeNumber, .slow
        arithmeticOperation(t1, .slow)
        orq tagTypeNumber, t1
        storeq t1, [cfr, t0, 8]
        dispatch()
    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end

llintOpWithProfile(op_to_number, OpToNumber, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t2)
    bqaeq t2, tagTypeNumber, .opToNumberIsImmediate
    btqz t2, tagTypeNumber, .opToNumberSlow
.opToNumberIsImmediate:
    return(t2)

.opToNumberSlow:
    callSlowPath(_slow_path_to_number)
    dispatch()
end)


llintOpWithReturn(op_to_string, OpToString, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0)
    btqnz t0, tagMask, .opToStringSlow
    bbneq JSCell::m_type[t0], StringType, .opToStringSlow
.opToStringIsString:
    return(t0)

.opToStringSlow:
    callSlowPath(_slow_path_to_string)
    dispatch()
end)


llintOpWithProfile(op_to_object, OpToObject, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t2)
    btqnz t2, tagMask, .opToObjectSlow
    bbb JSCell::m_type[t2], ObjectType, .opToObjectSlow
    return(t2)

.opToObjectSlow:
    callSlowPath(_slow_path_to_object)
    dispatch()
end)


llintOpWithMetadata(op_negate, OpNegate, macro (size, get, dispatch, metadata, return)
    get(operand, t0)
    loadConstantOrVariable(size, t0, t3)
    metadata(t1, t2)
    loadis OpNegate::Metadata::arithProfile[t1], t2
    bqb t3, tagTypeNumber, .opNegateNotInt
    btiz t3, 0x7fffffff, .opNegateSlow
    negi t3
    orq tagTypeNumber, t3
    ori ArithProfileInt, t2
    storei t2, OpNegate::Metadata::arithProfile[t1]
    return(t3)
.opNegateNotInt:
    btqz t3, tagTypeNumber, .opNegateSlow
    xorq 0x8000000000000000, t3
    ori ArithProfileNumber, t2
    storei t2, OpNegate::Metadata::arithProfile[t1]
    return(t3)

.opNegateSlow:
    callSlowPath(_slow_path_negate)
    dispatch()
end)


macro binaryOpCustomStore(name, op, integerOperationAndStore, doubleOperation)
    llintOpWithMetadata(op_%name%, op, macro (size, get, dispatch, metadata, return)
        metadata(t5, t0)

        macro profile(type)
            ori type, %op%::Metadata::arithProfile[t5]
        end

        get(rhs, t0)
        get(lhs, t2)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        bqb t0, tagTypeNumber, .op1NotInt
        bqb t1, tagTypeNumber, .op2NotInt
        get(dst, t2)
        integerOperationAndStore(t1, t0, .slow, t2)

        profile(ArithProfileIntInt)
        dispatch()

    .op1NotInt:
        # First operand is definitely not an int, the second operand could be anything.
        btqz t0, tagTypeNumber, .slow
        bqaeq t1, tagTypeNumber, .op1NotIntOp2Int
        btqz t1, tagTypeNumber, .slow
        addq tagTypeNumber, t1
        fq2d t1, ft1
        profile(ArithProfileNumberNumber)
        jmp .op1NotIntReady
    .op1NotIntOp2Int:
        profile(ArithProfileNumberInt)
        ci2d t1, ft1
    .op1NotIntReady:
        get(dst, t2)
        addq tagTypeNumber, t0
        fq2d t0, ft0
        doubleOperation(ft1, ft0)
        fd2q ft0, t0
        subq tagTypeNumber, t0
        storeq t0, [cfr, t2, 8]
        dispatch()

    .op2NotInt:
        # First operand is definitely an int, the second is definitely not.
        get(dst, t2)
        btqz t1, tagTypeNumber, .slow
        profile(ArithProfileIntNumber)
        ci2d t0, ft0
        addq tagTypeNumber, t1
        fq2d t1, ft1
        doubleOperation(ft1, ft0)
        fd2q ft0, t0
        subq tagTypeNumber, t0
        storeq t0, [cfr, t2, 8]
        dispatch()

    .slow:
        callSlowPath(_slow_path_%name%)
        dispatch()
    end)
end

if X86_64 or X86_64_WIN
    binaryOpCustomStore(div, OpDiv,
        macro (left, right, slow, index)
            # Assume t3 is scratchable.
            btiz left, slow
            bineq left, -1, .notNeg2TwoThe31DivByNeg1
            bieq right, -2147483648, .slow
        .notNeg2TwoThe31DivByNeg1:
            btinz right, .intOK
            bilt left, 0, slow
        .intOK:
            move left, t3
            move right, t0
            cdqi
            idivi t3
            btinz t1, slow
            orq tagTypeNumber, t0
            storeq t0, [cfr, index, 8]
        end,
        macro (left, right) divd left, right end)
else
    slowPathOp(div)
end


binaryOpCustomStore(mul, OpMul,
    macro (left, right, slow, index)
        # Assume t3 is scratchable.
        move right, t3
        bmulio left, t3, slow
        btinz t3, .done
        bilt left, 0, slow
        bilt right, 0, slow
    .done:
        orq tagTypeNumber, t3
        storeq t3, [cfr, index, 8]
    end,
    macro (left, right) muld left, right end)


macro binaryOp(name, op, integerOperation, doubleOperation)
    binaryOpCustomStore(name, op,
        macro (left, right, slow, index)
            integerOperation(left, right, slow)
            orq tagTypeNumber, right
            storeq right, [cfr, index, 8]
        end,
        doubleOperation)
end

binaryOp(add, OpAdd,
    macro (left, right, slow) baddio left, right, slow end,
    macro (left, right) addd left, right end)


binaryOp(sub, OpSub,
    macro (left, right, slow) bsubio left, right, slow end,
    macro (left, right) subd left, right end)


llintOpWithReturn(op_unsigned, OpUnsigned, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t2)
    bilt t2, 0, .opUnsignedSlow
    return(t2)
.opUnsignedSlow:
    callSlowPath(_slow_path_unsigned)
    dispatch()
end)


macro commonBitOp(opKind, name, op, operation)
    opKind(op_%name%, op, macro (size, get, dispatch, return)
        get(rhs, t0)
        get(lhs, t2)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        bqb t0, tagTypeNumber, .slow
        bqb t1, tagTypeNumber, .slow
        operation(t1, t0)
        orq tagTypeNumber, t0
        return(t0)

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

bitOpProfiled(bitand, OpBitand,
    macro (left, right) andi left, right end)

bitOpProfiled(bitor, OpBitor,
    macro (left, right) ori left, right end)

bitOpProfiled(bitxor, OpBitxor,
    macro (left, right) xori left, right end)

llintOpWithProfile(op_bitnot, OpBitnot, macro (size, get, dispatch, return)
    get(operand, t0)
    loadConstantOrVariableInt32(size, t0, t3, .opBitNotSlow)
    noti t3
    orq tagTypeNumber, t3
    return(t3)
.opBitNotSlow:
    callSlowPath(_slow_path_bitnot)
    dispatch()
end)


llintOp(op_overrides_has_instance, OpOverridesHasInstance, macro (size, get, dispatch)
    get(dst, t3)

    get(hasInstanceValue, t1)
    loadConstantOrVariable(size, t1, t0)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_globalObject[t2], t2
    loadp JSGlobalObject::m_functionProtoHasInstanceSymbolFunction[t2], t2
    bqneq t0, t2, .opOverridesHasInstanceNotDefaultSymbol

    get(constructor, t1)
    loadConstantOrVariable(size, t1, t0)
    tbz JSCell::m_flags[t0], ImplementsDefaultHasInstance, t1
    orq ValueFalse, t1
    storeq t1, [cfr, t3, 8]
    dispatch()

.opOverridesHasInstanceNotDefaultSymbol:
    storeq ValueTrue, [cfr, t3, 8]
    dispatch()
end)


llintOpWithReturn(op_is_empty, OpIsEmpty, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0)
    cqeq t0, ValueEmpty, t3
    orq ValueFalse, t3
    return(t3)
end)


llintOpWithReturn(op_is_undefined, OpIsUndefined, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0)
    btqz t0, tagMask, .opIsUndefinedCell
    cqeq t0, ValueUndefined, t3
    orq ValueFalse, t3
    return(t3)
.opIsUndefinedCell:
    btbnz JSCell::m_flags[t0], MasqueradesAsUndefined, .masqueradesAsUndefined
    move ValueFalse, t1
    return(t1)
.masqueradesAsUndefined:
    loadStructureWithScratch(t0, t3, t1, t5)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_globalObject[t1], t1
    cpeq Structure::m_globalObject[t3], t1, t0
    orq ValueFalse, t0
    return(t0)
end)


llintOpWithReturn(op_is_boolean, OpIsBoolean, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0)
    xorq ValueFalse, t0
    tqz t0, ~1, t0
    orq ValueFalse, t0
    return(t0)
end)


llintOpWithReturn(op_is_number, OpIsNumber, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0)
    tqnz t0, tagTypeNumber, t1
    orq ValueFalse, t1
    return(t1)
end)


llintOpWithReturn(op_is_cell_with_type, OpIsCellWithType, macro (size, get, dispatch, return)
    getu(size, OpIsCellWithType, type, t0)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t3)
    btqnz t3, tagMask, .notCellCase
    cbeq JSCell::m_type[t3], t0, t1
    orq ValueFalse, t1
    return(t1)
.notCellCase:
    return(ValueFalse)
end)


llintOpWithReturn(op_is_object, OpIsObject, macro (size, get, dispatch, return)
    get(operand, t1)
    loadConstantOrVariable(size, t1, t0)
    btqnz t0, tagMask, .opIsObjectNotCell
    cbaeq JSCell::m_type[t0], ObjectType, t1
    orq ValueFalse, t1
    return(t1)
.opIsObjectNotCell:
    return(ValueFalse)
end)


macro loadPropertyAtVariableOffset(propertyOffsetAsInt, objectAndStorage, value)
    bilt propertyOffsetAsInt, firstOutOfLineOffset, .isInline
    loadp JSObject::m_butterfly[objectAndStorage], objectAndStorage
    negi propertyOffsetAsInt
    sxi2q propertyOffsetAsInt, propertyOffsetAsInt
    jmp .ready
.isInline:
    addp sizeof JSObject - (firstOutOfLineOffset - 2) * 8, objectAndStorage
.ready:
    loadq (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffsetAsInt, 8], value
end


macro storePropertyAtVariableOffset(propertyOffsetAsInt, objectAndStorage, value)
    bilt propertyOffsetAsInt, firstOutOfLineOffset, .isInline
    loadp JSObject::m_butterfly[objectAndStorage], objectAndStorage
    negi propertyOffsetAsInt
    sxi2q propertyOffsetAsInt, propertyOffsetAsInt
    jmp .ready
.isInline:
    addp sizeof JSObject - (firstOutOfLineOffset - 2) * 8, objectAndStorage
.ready:
    storeq value, (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffsetAsInt, 8]
end


llintOpWithMetadata(op_get_by_id_direct, OpGetByIdDirect, macro (size, get, dispatch, metadata, return)
    metadata(t2, t0)
    get(base, t0)
    loadConstantOrVariableCell(size, t0, t3, .opGetByIdDirectSlow)
    loadi JSCell::m_structureID[t3], t1
    loadi OpGetByIdDirect::Metadata::structure[t2], t0
    bineq t0, t1, .opGetByIdDirectSlow
    loadi OpGetByIdDirect::Metadata::offset[t2], t1
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(OpGetByIdDirect, t2, t0)
    return(t0)

.opGetByIdDirectSlow:
    callSlowPath(_llint_slow_path_get_by_id_direct)
    dispatch()
end)


llintOpWithMetadata(op_get_by_id, OpGetById, macro (size, get, dispatch, metadata, return)
    metadata(t2, t1)
    loadb OpGetById::Metadata::mode[t2], t1
    get(base, t0)
    loadConstantOrVariableCell(size, t0, t3, .opGetByIdSlow)

.opGetByIdDefault:
    bbneq t1, constexpr GetByIdMode::Default, .opGetByIdProtoLoad
    loadi JSCell::m_structureID[t3], t1
    loadi OpGetById::Metadata::modeMetadata.defaultMode.structure[t2], t0
    bineq t0, t1, .opGetByIdSlow
    loadis OpGetById::Metadata::modeMetadata.defaultMode.cachedOffset[t2], t1
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(OpGetById, t2, t0)
    return(t0)

.opGetByIdProtoLoad:
    bbneq t1, constexpr GetByIdMode::ProtoLoad, .opGetByIdArrayLength
    loadi JSCell::m_structureID[t3], t1
    loadi OpGetById::Metadata::modeMetadata.protoLoadMode.structure[t2], t3
    bineq t3, t1, .opGetByIdSlow
    loadis OpGetById::Metadata::modeMetadata.protoLoadMode.cachedOffset[t2], t1
    loadp OpGetById::Metadata::modeMetadata.protoLoadMode.cachedSlot[t2], t3
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(OpGetById, t2, t0)
    return(t0)

.opGetByIdArrayLength:
    bbneq t1, constexpr GetByIdMode::ArrayLength, .opGetByIdUnset
    move t3, t0
    arrayProfile(OpGetById::Metadata::modeMetadata.arrayLengthMode.arrayProfile, t0, t2, t5)
    btiz t0, IsArray, .opGetByIdSlow
    btiz t0, IndexingShapeMask, .opGetByIdSlow
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::jsValue, constexpr JSVALUE_GIGACAGE_MASK, JSObject::m_butterfly[t3], t0, t1)
    loadi -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], t0
    bilt t0, 0, .opGetByIdSlow
    orq tagTypeNumber, t0
    valueProfile(OpGetById, t2, t0)
    return(t0)

.opGetByIdUnset:
    loadi JSCell::m_structureID[t3], t1
    loadi OpGetById::Metadata::modeMetadata.unsetMode.structure[t2], t0
    bineq t0, t1, .opGetByIdSlow
    valueProfile(OpGetById, t2, ValueUndefined)
    return(ValueUndefined)

.opGetByIdSlow:
    callSlowPath(_llint_slow_path_get_by_id)
    dispatch()
end)


llintOpWithMetadata(op_put_by_id, OpPutById, macro (size, get, dispatch, metadata, return)
    get(base, t3)
    loadConstantOrVariableCell(size, t3, t0, .opPutByIdSlow)
    metadata(t5, t2)
    loadis OpPutById::Metadata::oldStructure[t5], t2
    bineq t2, JSCell::m_structureID[t0], .opPutByIdSlow

    # At this point, we have:
    # t0 -> object base
    # t2 -> current structure ID
    # t5 -> metadata

    loadi OpPutById::Metadata::newStructure[t5], t1
    btiz t1, .opPutByIdNotTransition

    # This is the transition case. t1 holds the new structureID. t2 holds the old structure ID.
    # If we have a chain, we need to check it. t0 is the base. We may clobber t1 to use it as
    # scratch.
    loadp OpPutById::Metadata::structureChain[t5], t3
    btpz t3, .opPutByIdTransitionDirect

    structureIDToStructureWithScratch(t2, t1, t3)

    # reload the StructureChain since we used t3 as a scratch above
    loadp OpPutById::Metadata::structureChain[t5], t3

    loadp StructureChain::m_vector[t3], t3
    assert(macro (ok) btpnz t3, ok end)

    loadq Structure::m_prototype[t2], t2
    bqeq t2, ValueNull, .opPutByIdTransitionChainDone
.opPutByIdTransitionChainLoop:
    # At this point, t2 contains a prototye, and [t3] contains the Structure* that we want that
    # prototype to have. We don't want to have to load the Structure* for t2. Instead, we load
    # the Structure* from [t3], and then we compare its id to the id in the header of t2.
    loadp [t3], t1
    loadi JSCell::m_structureID[t2], t2
    # Now, t1 has the Structure* and t2 has the StructureID that we want that Structure* to have.
    bineq t2, Structure::m_blob + StructureIDBlob::u.fields.structureID[t1], .opPutByIdSlow
    addp PtrSize, t3
    loadq Structure::m_prototype[t1], t2
    bqneq t2, ValueNull, .opPutByIdTransitionChainLoop

.opPutByIdTransitionChainDone:
    # Reload the new structure, since we clobbered it above.
    loadi OpPutById::Metadata::newStructure[t5], t1

.opPutByIdTransitionDirect:
    storei t1, JSCell::m_structureID[t0]
    writeBarrierOnOperandWithReload(size, get, base, macro ()
        # Reload metadata into t5
        metadata(t5, t1)
        # Reload base into t0
        get(base, t1)
        loadConstantOrVariable(size, t1, t0)
    end)

.opPutByIdNotTransition:
    # The only thing live right now is t0, which holds the base.
    get(value, t1)
    loadConstantOrVariable(size, t1, t2)
    loadi OpPutById::Metadata::offset[t5], t1
    storePropertyAtVariableOffset(t1, t0, t2)
    writeBarrierOnOperands(size, get, base, value)
    dispatch()

.opPutByIdSlow:
    callSlowPath(_llint_slow_path_put_by_id)
    dispatch()
end)


llintOpWithMetadata(op_get_by_val, OpGetByVal, macro (size, get, dispatch, metadata, return)
    macro finishGetByVal(result, scratch)
        get(dst, scratch)
        storeq result, [cfr, scratch, 8]
        valueProfile(OpGetByVal, t5, result)
        dispatch()
    end

    macro finishIntGetByVal(result, scratch)
        orq tagTypeNumber, result
        finishGetByVal(result, scratch)
    end

    macro finishDoubleGetByVal(result, scratch1, scratch2)
        fd2q result, scratch1
        subq tagTypeNumber, scratch1
        finishGetByVal(scratch1, scratch2)
    end

    metadata(t5, t2)

    get(base, t2)
    loadConstantOrVariableCell(size, t2, t0, .opGetByValSlow)

    move t0, t2
    arrayProfile(OpGetByVal::Metadata::arrayProfile, t2, t5, t1)

    get(property, t3)
    loadConstantOrVariableInt32(size, t3, t1, .opGetByValSlow)
    sxi2q t1, t1

    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::jsValue, constexpr JSVALUE_GIGACAGE_MASK, JSObject::m_butterfly[t0], t3, tagTypeNumber)
    move TagTypeNumber, tagTypeNumber

    andi IndexingShapeMask, t2
    bieq t2, Int32Shape, .opGetByValIsContiguous
    bineq t2, ContiguousShape, .opGetByValNotContiguous

.opGetByValIsContiguous:
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValSlow
    get(dst, t0)
    loadq [t3, t1, 8], t2
    btqz t2, .opGetByValSlow
    jmp .opGetByValDone

.opGetByValNotContiguous:
    bineq t2, DoubleShape, .opGetByValNotDouble
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValSlow
    get(dst, t0)
    loadd [t3, t1, 8], ft0
    bdnequn ft0, ft0, .opGetByValSlow
    fd2q ft0, t2
    subq tagTypeNumber, t2
    jmp .opGetByValDone
    
.opGetByValNotDouble:
    subi ArrayStorageShape, t2
    bia t2, SlowPutArrayStorageShape - ArrayStorageShape, .opGetByValNotIndexedStorage
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t3], .opGetByValSlow
    get(dst, t0)
    loadq ArrayStorage::m_vector[t3, t1, 8], t2
    btqz t2, .opGetByValSlow

.opGetByValDone:
    storeq t2, [cfr, t0, 8]
    valueProfile(OpGetByVal, t5, t2)
    dispatch()

.opGetByValNotIndexedStorage:
    # First lets check if we even have a typed array. This lets us do some boilerplate up front.
    loadb JSCell::m_type[t0], t2
    subi FirstTypedArrayType, t2
    biaeq t2, NumberOfTypedArrayTypesExcludingDataView, .opGetByValSlow
    
    # Sweet, now we know that we have a typed array. Do some basic things now.
    biaeq t1, JSArrayBufferView::m_length[t0], .opGetByValSlow

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

    bia t2, Uint16ArrayType - FirstTypedArrayType, .opGetByValAboveUint16Array

    # We have one of Int8ArrayType .. Uint16ArrayType.
    bia t2, Uint8ClampedArrayType - FirstTypedArrayType, .opGetByValInt16ArrayOrUint16Array

    # We have one of Int8ArrayType ... Uint8ClampedArrayType
    bia t2, Int8ArrayType - FirstTypedArrayType, .opGetByValUint8ArrayOrUint8ClampedArray

    # We have Int8ArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    loadbs [t3, t1], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint8ArrayOrUint8ClampedArray:
    bia t2, Uint8ArrayType - FirstTypedArrayType, .opGetByValUint8ClampedArray

    # We have Uint8ArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    loadb [t3, t1], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint8ClampedArray:
    # We have Uint8ClampedArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    loadb [t3, t1], t0
    finishIntGetByVal(t0, t1)

.opGetByValInt16ArrayOrUint16Array:
    # We have either Int16ArrayType or Uint16ClampedArrayType.
    bia t2, Int16ArrayType - FirstTypedArrayType, .opGetByValUint16Array

    # We have Int16ArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    loadhs [t3, t1, 2], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint16Array:
    # We have Uint16ArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    loadh [t3, t1, 2], t0
    finishIntGetByVal(t0, t1)

.opGetByValAboveUint16Array:
    # We have one of Int32ArrayType .. Float64ArrayType.
    bia t2, Uint32ArrayType - FirstTypedArrayType, .opGetByValFloat32ArrayOrFloat64Array

    # We have either Int32ArrayType or Uint32ArrayType
    bia t2, Int32ArrayType - FirstTypedArrayType, .opGetByValUint32Array

    # We have Int32ArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    loadi [t3, t1, 4], t0
    finishIntGetByVal(t0, t1)

.opGetByValUint32Array:
    # We have Uint32ArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    # This is the hardest part because of large unsigned values.
    loadi [t3, t1, 4], t0
    bilt t0, 0, .opGetByValSlow # This case is still awkward to implement in LLInt.
    finishIntGetByVal(t0, t1)

.opGetByValFloat32ArrayOrFloat64Array:
    # We have one of Float32ArrayType or Float64ArrayType. Sadly, we cannot handle Float32Array
    # inline yet. That would require some offlineasm changes.
    bieq t2, Float32ArrayType - FirstTypedArrayType, .opGetByValSlow

    # We have Float64ArrayType.
    loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::primitive, constexpr PRIMITIVE_GIGACAGE_MASK, JSArrayBufferView::m_vector[t0], t3, t2)
    loadd [t3, t1, 8], ft0
    bdnequn ft0, ft0, .opGetByValSlow
    finishDoubleGetByVal(ft0, t0, t1)

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
            storeCallback(t2, t1, [t0, t3, 8])
            dispatch()

        .outOfBounds:
            biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
            storeb 1, %op%::Metadata::arrayProfile.m_mayStoreToHole[t5]
            addi 1, t3, t2
            storei t2, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
            jmp .storeResult
        end

        get(base, t0)
        loadConstantOrVariableCell(size, t0, t1, .opPutByValSlow)
        move t1, t2
        metadata(t5, t0)
        arrayProfile(%op%::Metadata::arrayProfile, t2, t5, t0)
        get(property, t0)
        loadConstantOrVariableInt32(size, t0, t3, .opPutByValSlow)
        sxi2q t3, t3
        loadCaged(_g_gigacageBasePtrs + Gigacage::BasePtrs::jsValue, constexpr JSVALUE_GIGACAGE_MASK, JSObject::m_butterfly[t1], t0, tagTypeNumber)
        move TagTypeNumber, tagTypeNumber
        btinz t2, CopyOnWrite, .opPutByValSlow
        andi IndexingShapeMask, t2
        bineq t2, Int32Shape, .opPutByValNotInt32
        contiguousPutByVal(
            macro (operand, scratch, address)
                loadConstantOrVariable(size, operand, scratch)
                bqb scratch, tagTypeNumber, .opPutByValSlow
                storeq scratch, address
                writeBarrierOnOperands(size, get, base, value)
            end)

    .opPutByValNotInt32:
        bineq t2, DoubleShape, .opPutByValNotDouble
        contiguousPutByVal(
            macro (operand, scratch, address)
                loadConstantOrVariable(size, operand, scratch)
                bqb scratch, tagTypeNumber, .notInt
                ci2d scratch, ft0
                jmp .ready
            .notInt:
                addq tagTypeNumber, scratch
                fq2d scratch, ft0
                bdnequn ft0, ft0, .opPutByValSlow
            .ready:
                stored ft0, address
                writeBarrierOnOperands(size, get, base, value)
            end)

    .opPutByValNotDouble:
        bineq t2, ContiguousShape, .opPutByValNotContiguous
        contiguousPutByVal(
            macro (operand, scratch, address)
                loadConstantOrVariable(size, operand, scratch)
                storeq scratch, address
                writeBarrierOnOperands(size, get, base, value)
            end)

    .opPutByValNotContiguous:
        bineq t2, ArrayStorageShape, .opPutByValSlow
        biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
        btqz ArrayStorage::m_vector[t0, t3, 8], .opPutByValArrayStorageEmpty
    .opPutByValArrayStorageStoreResult:
        get(value, t2)
        loadConstantOrVariable(size, t2, t1)
        storeq t1, ArrayStorage::m_vector[t0, t3, 8]
        writeBarrierOnOperands(size, get, base, value)
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
        loadConstantOrVariable(size, t1, t0)
        btqnz t0, ~0xf, .slow
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
        loadq [cfr, t0, 8], t0
        btqnz t0, tagMask, .immediate
        loadStructureWithScratch(t0, t2, t1, t3)
        cellHandler(t2, JSCell::m_flags[t0], .target)
        dispatch()

    .target:
        jump(target)

    .immediate:
        andq ~TagBitUndefined, t0
        immediateHandler(t0, .target)
        dispatch()
    end)
end

equalNullJumpOp(jeq_null, OpJeqNull,
    macro (structure, value, target) 
        btbz value, MasqueradesAsUndefined, .notMasqueradesAsUndefined
        loadp CodeBlock[cfr], t0
        loadp CodeBlock::m_globalObject[t0], t0
        bpeq Structure::m_globalObject[structure], t0, target
.notMasqueradesAsUndefined:
    end,
    macro (value, target) bqeq value, ValueNull, target end)


equalNullJumpOp(jneq_null, OpJneqNull,
    macro (structure, value, target) 
        btbz value, MasqueradesAsUndefined, target
        loadp CodeBlock[cfr], t0
        loadp CodeBlock::m_globalObject[t0], t0
        bpneq Structure::m_globalObject[structure], t0, target
    end,
    macro (value, target) bqneq value, ValueNull, target end)


llintOpWithMetadata(op_jneq_ptr, OpJneqPtr, macro (size, get, dispatch, metadata, return)
    get(value, t0)
    get(specialPointer, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_globalObject[t2], t2
    loadp JSGlobalObject::m_specialPointers[t2, t1, PtrSize], t1
    bpneq t1, [cfr, t0, 8], .opJneqPtrTarget
    dispatch()

.opJneqPtrTarget:
    metadata(t5, t0)
    storeb 1, OpJneqPtr::Metadata::hasJumped[t5]
    get(target, t0)
    jumpImpl(t0)
end)


macro compareJumpOp(name, op, integerCompare, doubleCompare)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(lhs, t2)
        get(rhs, t3)
        loadConstantOrVariable(size, t2, t0)
        loadConstantOrVariable(size, t3, t1)
        bqb t0, tagTypeNumber, .op1NotInt
        bqb t1, tagTypeNumber, .op2NotInt
        integerCompare(t0, t1, .jumpTarget)
        dispatch()

    .op1NotInt:
        btqz t0, tagTypeNumber, .slow
        bqb t1, tagTypeNumber, .op1NotIntOp2NotInt
        ci2d t1, ft1
        jmp .op1NotIntReady
    .op1NotIntOp2NotInt:
        btqz t1, tagTypeNumber, .slow
        addq tagTypeNumber, t1
        fq2d t1, ft1
    .op1NotIntReady:
        addq tagTypeNumber, t0
        fq2d t0, ft0
        doubleCompare(ft0, ft1, .jumpTarget)
        dispatch()

    .op2NotInt:
        ci2d t0, ft0
        btqz t1, tagTypeNumber, .slow
        addq tagTypeNumber, t1
        fq2d t1, ft1
        doubleCompare(ft0, ft1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)

    .slow:
        callSlowPath(_llint_slow_path_%name%)
        nextInstruction()
    end)
end


macro equalityJumpOp(name, op, integerComparison)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(lhs, t2)
        get(rhs, t3)
        loadConstantOrVariableInt32(size, t2, t0, .slow)
        loadConstantOrVariableInt32(size, t3, t1, .slow)
        integerComparison(t0, t1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)

    .slow:
        callSlowPath(_llint_slow_path_%name%)
        nextInstruction()
    end)
end


macro compareUnsignedJumpOp(name, op, integerCompare)
    llintOpWithJump(op_%name%, op, macro (size, get, jump, dispatch)
        get(lhs, t2)
        get(rhs, t3)
        loadConstantOrVariable(size, t2, t0)
        loadConstantOrVariable(size, t3, t1)
        integerCompare(t0, t1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(target)
    end)
end


macro compareUnsignedOp(name, op, integerCompareAndSet)
    llintOpWithReturn(op_%name%, op, macro (size, get, dispatch, return)
        get(lhs, t2)
        get(rhs,  t0)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        integerCompareAndSet(t0, t1, t0)
        orq ValueFalse, t0
        return(t0)
    end)
end


llintOpWithJump(op_switch_imm, OpSwitchImm, macro (size, get, jump, dispatch)
    get(scrutinee, t2)
    get(tableIndex, t3)
    loadConstantOrVariable(size, t2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3
    loadp CodeBlock::RareData::m_switchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    bqb t1, tagTypeNumber, .opSwitchImmNotInt
    subi SimpleJumpTable::min[t2], t1
    biaeq t1, SimpleJumpTable::branchOffsets + VectorSizeOffset[t2], .opSwitchImmFallThrough
    loadp SimpleJumpTable::branchOffsets + VectorBufferOffset[t2], t3
    loadis [t3, t1, 4], t1
    btiz t1, .opSwitchImmFallThrough
    dispatchIndirect(t1)

.opSwitchImmNotInt:
    btqnz t1, tagTypeNumber, .opSwitchImmSlow   # Go slow if it's a double.
.opSwitchImmFallThrough:
    jump(defaultOffset)

.opSwitchImmSlow:
    callSlowPath(_llint_slow_path_switch_imm)
    nextInstruction()
end)


llintOpWithJump(op_switch_char, OpSwitchChar, macro (size, get, jump, dispatch)
    get(scrutinee, t2)
    get(tableIndex, t3)
    loadConstantOrVariable(size, t2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3
    loadp CodeBlock::RareData::m_switchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    btqnz t1, tagMask, .opSwitchCharFallThrough
    bbneq JSCell::m_type[t1], StringType, .opSwitchCharFallThrough
    bineq JSString::m_length[t1], 1, .opSwitchCharFallThrough
    loadp JSString::m_value[t1], t0
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
    loadis [t2, t0, 4], t1
    btiz t1, .opSwitchCharFallThrough
    dispatchIndirect(t1)

.opSwitchCharFallThrough:
    jump(defaultOffset)

.opSwitchOnRope:
    callSlowPath(_llint_slow_path_switch_char)
    nextInstruction()
end)


# we assume t5 contains the metadata, and we should not scratch that
macro arrayProfileForCall(op, getu)
    getu(argv, t3)
    negp t3
    loadq ThisArgumentOffset[cfr, t3, 8], t0
    btqnz t0, tagMask, .done
    loadi JSCell::m_structureID[t0], t3
    storei t3, %op%::Metadata::arrayProfile.m_lastSeenStructureID[t5]
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
        loadConstantOrVariable(size, t0, t3)
        bqneq t3, t2, .opCallSlow
        getu(size, op, argv, t3)
        lshifti 3, t3
        negp t3
        addp cfr, t3
        storeq t2, Callee[t3]
        getu(size, op, argc, t2)
        storei PC, ArgumentCount + TagOffset[cfr]
        storei t2, ArgumentCount + PayloadOffset[t3]
        move t3, sp
        if POISON
            loadp _g_JITCodePoison, t2
            xorp %op%::Metadata::callLinkInfo.machineCodeTarget[t5], t2
            prepareCall(t2, t1, t3, t4, JSEntryPtrTag)
            callTargetFunction(size, op, dispatch, t2, JSEntryPtrTag)
        else
            prepareCall(%op%::Metadata::callLinkInfo.machineCodeTarget[t5], t2, t3, t4, JSEntryPtrTag)
            callTargetFunction(size, op, dispatch, %op%::Metadata::callLinkInfo.machineCodeTarget[t5], JSEntryPtrTag)
        end

    .opCallSlow:
        slowPathForCall(size, op, dispatch, slowPath, prepareCall)
    end)
end

llintOp(op_ret, OpRet, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    get(value, t2)
    loadConstantOrVariable(size, t2, r0)
    doReturn()
end)


llintOpWithReturn(op_to_primitive, OpToPrimitive, macro (size, get, dispatch, return)
    get(src, t2)
    loadConstantOrVariable(size, t2, t0)
    btqnz t0, tagMask, .opToPrimitiveIsImm
    bbaeq JSCell::m_type[t0], ObjectType, .opToPrimitiveSlowCase
.opToPrimitiveIsImm:
    return(t0)

.opToPrimitiveSlowCase:
    callSlowPath(_slow_path_to_primitive)
    dispatch()
end)


commonOp(llint_op_catch, macro() end, macro (size)
    # This is where we end up from the JIT's throw trampoline (because the
    # machine code return address will be set to _llint_op_catch), and from
    # the interpreter's throw trampoline (see _llint_throw_trampoline).
    # The throwing code must have known that we were throwing to the interpreter,
    # and have set VM::targetInterpreterPCForThrow.
    loadp Callee[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)
    loadp VM::callFrameForCatch[t3], cfr
    storep 0, VM::callFrameForCatch[t3]
    restoreStackPointerAfterCall()

    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_metadata[PB], metadataTable
    loadp CodeBlock::m_instructionsRawPointer[PB], PB
    unpoison(_g_CodeBlockPoison, PB, t2)
    loadp VM::targetInterpreterPCForThrow[t3], PC
    subp PB, PC

    callSlowPath(_llint_slow_path_check_if_exception_is_uncatchable_and_notify_profiler)
    bpeq r1, 0, .isCatchableException
    jmp _llint_throw_from_slow_path_trampoline

.isCatchableException:
    loadp Callee[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3

    loadp VM::m_exception[t3], t0
    storep 0, VM::m_exception[t3]
    get(size, OpCatch, exception, t2)
    storeq t0, [cfr, t2, 8]

    loadq Exception::m_value[t0], t3
    get(size, OpCatch, thrownValue, t2)
    storeq t3, [cfr, t2, 8]

    traceExecution()

    callSlowPath(_llint_slow_path_profile_catch)

    dispatchOp(size, op_catch)
end)


llintOp(op_end, OpEnd, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    get(value, t0)
    assertNotConstant(size, t0)
    loadq [cfr, t0, 8], r0
    doReturn()
end)


op(llint_throw_from_slow_path_trampoline, macro ()
    loadp Callee[cfr], t1
    andp MarkedBlockMask, t1
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t1
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(t1, t2)

    callSlowPath(_llint_slow_path_handle_exception)

    # When throwing from the interpreter (i.e. throwing from LLIntSlowPaths), so
    # the throw target is not necessarily interpreted code, we come to here.
    # This essentially emulates the JIT's throwing protocol.
    loadp Callee[cfr], t1
    andp MarkedBlockMask, t1
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t1
    jmp VM::targetMachinePCForThrow[t1], ExceptionHandlerPtrTag
end)


op(llint_throw_during_call_trampoline, macro ()
    preserveReturnAddressAfterCall(t2)
    jmp _llint_throw_from_slow_path_trampoline
end)


macro nativeCallTrampoline(executableOffsetToFunction)

    functionPrologue()
    storep 0, CodeBlock[cfr]
    loadp Callee[cfr], t0
    andp MarkedBlockMask, t0, t1
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t1
    storep cfr, VM::topCallFrame[t1]
    if ARM64 or ARM64E or C_LOOP
        storep lr, ReturnPC[cfr]
    end
    move cfr, a0
    loadp Callee[cfr], t1
    loadp JSFunction::m_executable[t1], t1
    unpoison(_g_JSFunctionPoison, t1, t2)
    checkStackPointerAlignment(t3, 0xdead0001)
    if C_LOOP
        loadp _g_NativeCodePoison, t2
        xorp executableOffsetToFunction[t1], t2
        cloopCallNative t2
    else
        if X86_64_WIN
            subp 32, sp
            call executableOffsetToFunction[t1], JSEntryPtrTag
            addp 32, sp
        else
            loadp _g_NativeCodePoison, t2
            xorp executableOffsetToFunction[t1], t2
            call t2, JSEntryPtrTag
        end
    end

    loadp Callee[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3

    btpnz VM::m_exception[t3], .handleException

    functionEpilogue()
    ret

.handleException:
    storep cfr, VM::topCallFrame[t3]
    jmp _llint_throw_from_slow_path_trampoline
end

macro internalFunctionCallTrampoline(offsetOfFunction)
    functionPrologue()
    storep 0, CodeBlock[cfr]
    loadp Callee[cfr], t0
    andp MarkedBlockMask, t0, t1
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t1], t1
    storep cfr, VM::topCallFrame[t1]
    if ARM64 or ARM64E or C_LOOP
        storep lr, ReturnPC[cfr]
    end
    move cfr, a0
    loadp Callee[cfr], t1
    checkStackPointerAlignment(t3, 0xdead0001)
    if C_LOOP
        loadp _g_NativeCodePoison, t2
        xorp offsetOfFunction[t1], t2
        cloopCallNative t2
    else
        if X86_64_WIN
            subp 32, sp
            call offsetOfFunction[t1], JSEntryPtrTag
            addp 32, sp
        else
            loadp _g_NativeCodePoison, t2
            xorp offsetOfFunction[t1], t2
            call t2, JSEntryPtrTag
        end
    end

    loadp Callee[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlockFooterOffset + MarkedBlock::Footer::m_vm[t3], t3

    btpnz VM::m_exception[t3], .handleException

    functionEpilogue()
    ret

.handleException:
    storep cfr, VM::topCallFrame[t3]
    jmp _llint_throw_from_slow_path_trampoline
end

macro varInjectionCheck(slowPath, scratch)
    loadp CodeBlock[cfr], scratch
    loadp CodeBlock::m_globalObject[scratch], scratch
    loadp JSGlobalObject::m_varInjectionWatchpoint[scratch], scratch
    bbeq WatchpointSet::m_state[scratch], IsInvalidated, slowPath
end

llintOpWithMetadata(op_resolve_scope, OpResolveScope, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)

    macro getConstantScope()
        loadp OpResolveScope::Metadata::constantScope[t5],  t0
        return(t0)
    end

    macro resolveScope()
        loadi OpResolveScope::Metadata::localScopeDepth[t5], t2
        get(scope, t0)
        loadq [cfr, t0, 8], t0
        btiz t2, .resolveScopeLoopEnd

    .resolveScopeLoop:
        loadp JSScope::m_next[t0], t0
        subi 1, t2
        btinz t2, .resolveScopeLoop

    .resolveScopeLoopEnd:
        return(t0)
    end

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
    varInjectionCheck(.rDynamic, t2)
    getConstantScope()

.rGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .rGlobalLexicalVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic, t2)
    getConstantScope()

.rGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .rClosureVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic, t2)
    getConstantScope()

.rClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .rDynamic
    varInjectionCheck(.rDynamic, t2)
    resolveScope()

.rDynamic:
    callSlowPath(_slow_path_resolve_scope)
    dispatch()
end)


macro loadWithStructureCheck(op, get, slowPath)
    get(scope, t0)
    loadq [cfr, t0, 8], t0
    loadStructureWithScratch(t0, t2, t1, t3)
    loadp %op%::Metadata::structure[t5], t1
    bpneq t2, t1, slowPath
end

llintOpWithMetadata(op_get_from_scope, OpGetFromScope, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)

    macro getProperty()
        loadp OpGetFromScope::Metadata::operand[t5], t1
        loadPropertyAtVariableOffset(t1, t0, t2)
        valueProfile(OpGetFromScope, t5, t2)
        return(t2)
    end

    macro getGlobalVar(tdzCheckIfNecessary)
        loadp OpGetFromScope::Metadata::operand[t5], t0
        loadq [t0], t0
        tdzCheckIfNecessary(t0)
        valueProfile(OpGetFromScope, t5, t0)
        return(t0)
    end

    macro getClosureVar()
        loadp OpGetFromScope::Metadata::operand[t5], t1
        loadq JSLexicalEnvironment_variables[t0, t1, 8], t0
        valueProfile(OpGetFromScope, t5, t0)
        return(t0)
    end

    loadi OpGetFromScope::Metadata::getPutInfo[t5], t0
    andi ResolveTypeMask, t0

#gGlobalProperty:
    bineq t0, GlobalProperty, .gGlobalVar
    loadWithStructureCheck(OpGetFromScope, get, .gDynamic)
    getProperty()

.gGlobalVar:
    bineq t0, GlobalVar, .gGlobalLexicalVar
    getGlobalVar(macro(v) end)

.gGlobalLexicalVar:
    bineq t0, GlobalLexicalVar, .gClosureVar
    getGlobalVar(
        macro (value)
            bqeq value, ValueEmpty, .gDynamic
        end)

.gClosureVar:
    bineq t0, ClosureVar, .gGlobalPropertyWithVarInjectionChecks
    loadVariable(get, scope, t0)
    getClosureVar()

.gGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .gGlobalVarWithVarInjectionChecks
    loadWithStructureCheck(OpGetFromScope, get, .gDynamic)
    getProperty()

.gGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .gGlobalLexicalVarWithVarInjectionChecks
    varInjectionCheck(.gDynamic, t2)
    getGlobalVar(macro(v) end)

.gGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .gClosureVarWithVarInjectionChecks
    varInjectionCheck(.gDynamic, t2)
    getGlobalVar(
        macro (value)
            bqeq value, ValueEmpty, .gDynamic
        end)

.gClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .gDynamic
    varInjectionCheck(.gDynamic, t2)
    loadVariable(get, scope, t0)
    getClosureVar()

.gDynamic:
    callSlowPath(_llint_slow_path_get_from_scope)
    dispatch()
end)


llintOpWithMetadata(op_put_to_scope, OpPutToScope, macro (size, get, dispatch, metadata, return)
    macro putProperty()
        get(value, t1)
        loadConstantOrVariable(size, t1, t2)
        loadis OpPutToScope::Metadata::operand[t5], t1
        storePropertyAtVariableOffset(t1, t0, t2)
    end

    macro putGlobalVariable()
        get(value, t0)
        loadConstantOrVariable(size, t0, t1)
        loadp OpPutToScope::Metadata::watchpointSet[t5], t2
        loadp OpPutToScope::Metadata::operand[t5], t0
        notifyWrite(t2, .pDynamic)
        storeq t1, [t0]
    end

    macro putClosureVar()
        get(value, t1)
        loadConstantOrVariable(size, t1, t2)
        loadis OpPutToScope::Metadata::operand[t5], t1
        storeq t2, JSLexicalEnvironment_variables[t0, t1, 8]
    end

    macro putLocalClosureVar()
        get(value, t1)
        loadConstantOrVariable(size, t1, t2)
        loadp OpPutToScope::Metadata::watchpointSet[t5], t3
        btpz t3, .noVariableWatchpointSet
        notifyWrite(t3, .pDynamic)
    .noVariableWatchpointSet:
        loadis OpPutToScope::Metadata::operand[t5], t1
        storeq t2, JSLexicalEnvironment_variables[t0, t1, 8]
    end

    macro checkTDZInGlobalPutToScopeIfNecessary()
        loadis OpPutToScope::Metadata::getPutInfo[t5], t0
        andi InitializationModeMask, t0
        rshifti InitializationModeShift, t0
        bineq t0, NotInitialization, .noNeedForTDZCheck
        loadp OpPutToScope::Metadata::operand[t5], t0
        loadq [t0], t0
        bqeq t0, ValueEmpty, .pDynamic
    .noNeedForTDZCheck:
    end

    metadata(t5, t0)
    loadi OpPutToScope::Metadata::getPutInfo[t5], t0
    andi ResolveTypeMask, t0

#pLocalClosureVar:
    bineq t0, LocalClosureVar, .pGlobalProperty
    loadVariable(get, scope, t0)
    putLocalClosureVar()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pGlobalProperty:
    bineq t0, GlobalProperty, .pGlobalVar
    loadWithStructureCheck(OpPutToScope, get, .pDynamic)
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
    checkTDZInGlobalPutToScopeIfNecessary()
    putGlobalVariable()
    writeBarrierOnGlobalLexicalEnvironment(size, get, value)
    dispatch()

.pClosureVar:
    bineq t0, ClosureVar, .pGlobalPropertyWithVarInjectionChecks
    loadVariable(get, scope, t0)
    putClosureVar()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .pGlobalVarWithVarInjectionChecks
    loadWithStructureCheck(OpPutToScope, get, .pDynamic)
    putProperty()
    writeBarrierOnOperands(size, get, scope, value)
    dispatch()

.pGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .pGlobalLexicalVarWithVarInjectionChecks
    varInjectionCheck(.pDynamic, t2)
    putGlobalVariable()
    writeBarrierOnGlobalObject(size, get, value)
    dispatch()

.pGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .pClosureVarWithVarInjectionChecks
    varInjectionCheck(.pDynamic, t2)
    checkTDZInGlobalPutToScopeIfNecessary()
    putGlobalVariable()
    writeBarrierOnGlobalLexicalEnvironment(size, get, value)
    dispatch()

.pClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .pModuleVar
    varInjectionCheck(.pDynamic, t2)
    loadVariable(get, scope, t0)
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
    loadVariable(get, arguments, t0)
    getu(size, OpGetFromArguments, index, t1)
    loadq DirectArguments_storage[t0, t1, 8], t0
    return(t0)
end)


llintOp(op_put_to_arguments, OpPutToArguments, macro (size, get, dispatch)
    loadVariable(get, arguments, t0)
    getu(size, OpPutToArguments, index, t1)
    get(value, t3)
    loadConstantOrVariable(size, t3, t2)
    storeq t2, DirectArguments_storage[t0, t1, 8]
    writeBarrierOnOperands(size, get, arguments, value)
    dispatch()
end)


llintOpWithReturn(op_get_parent_scope, OpGetParentScope, macro (size, get, dispatch, return)
    loadVariable(get, scope, t0)
    loadp JSScope::m_next[t0], t0
    return(t0)
end)


llintOpWithMetadata(op_profile_type, OpProfileType, macro (size, get, dispatch, metadata, return)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_poisonedVM[t1], t1
    unpoison(_g_CodeBlockPoison, t1, t3)
    # t1 is holding the pointer to the typeProfilerLog.
    loadp VM::m_typeProfilerLog[t1], t1
    # t2 is holding the pointer to the current log entry.
    loadp TypeProfilerLog::m_currentLogEntryPtr[t1], t2

    # t0 is holding the JSValue argument.
    get(target, t3)
    loadConstantOrVariable(size, t3, t0)

    bqeq t0, ValueEmpty, .opProfileTypeDone
    # Store the JSValue onto the log entry.
    storeq t0, TypeProfilerLog::LogEntry::value[t2]
    
    # Store the TypeLocation onto the log entry.
    metadata(t5, t3)
    loadp OpProfileType::Metadata::typeLocation[t5], t3
    storep t3, TypeProfilerLog::LogEntry::location[t2]

    btqz t0, tagMask, .opProfileTypeIsCell
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
    addq 1, BasicBlockLocation::m_executionCount[t0]
    dispatch()
end)


llintOpWithReturn(op_get_rest_length, OpGetRestLength, macro (size, get, dispatch, return)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    subi 1, t0
    getu(size, OpGetRestLength, numParametersToSkip, t1)
    bilteq t0, t1, .storeZero
    subi t1, t0
    jmp .boxUp
.storeZero:
    move 0, t0
.boxUp:
    orq tagTypeNumber, t0
    return(t0)
end)


llintOp(op_log_shadow_chicken_prologue, OpLogShadowChickenPrologue, macro (size, get, dispatch)
    acquireShadowChickenPacket(.opLogShadowChickenPrologueSlow)
    storep cfr, ShadowChicken::Packet::frame[t0]
    loadp CallerFrame[cfr], t1
    storep t1, ShadowChicken::Packet::callerFrame[t0]
    loadp Callee[cfr], t1
    storep t1, ShadowChicken::Packet::callee[t0]
    loadVariable(get, scope, t1)
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
    loadVariable(get, thisValue, t1)
    storep t1, ShadowChicken::Packet::thisValue[t0]
    loadVariable(get, scope, t1)
    storep t1, ShadowChicken::Packet::scope[t0]
    loadp CodeBlock[cfr], t1
    storep t1, ShadowChicken::Packet::codeBlock[t0]
    storei PC, ShadowChicken::Packet::callSiteIndex[t0]
    dispatch()
.opLogShadowChickenTailSlow:
    callSlowPath(_llint_slow_path_log_shadow_chicken_tail)
    dispatch()
end)
