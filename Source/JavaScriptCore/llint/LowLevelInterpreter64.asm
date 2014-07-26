# Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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
macro jumpToInstruction()
    jmp [PB, PC, 8]
end

macro dispatch(advance)
    addp advance, PC
    jumpToInstruction()
end

macro dispatchInt(advance)
    addi advance, PC
    jumpToInstruction()
end

macro dispatchIntIndirect(offset)
    dispatchInt(offset * 8[PB, PC, 8])
end

macro dispatchAfterCall()
    loadi ArgumentCount + TagOffset[cfr], PC
    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_instructions[PB], PB
    loadisFromInstruction(1, t1)
    storeq t0, [cfr, t1, 8]
    valueProfile(t0, (CallOpCodeSize - 1), t2)
    dispatch(CallOpCodeSize)
end

macro cCall2(function, arg1, arg2)
    checkStackPointerAlignment(t4, 0xbad0c002)
    if X86_64
        move arg1, t4
        move arg2, t5
        call function
    elsif X86_64_WIN
        # Note: this implementation is only correct if the return type size is > 8 bytes.
        # See macro cCall2Void for an implementation when the return type <= 8 bytes.
        # On Win64, when the return type is larger than 8 bytes, we need to allocate space on the stack for the return value.
        # On entry rcx (t2), should contain a pointer to this stack space. The other parameters are shifted to the right,
        # rdx (t1) should contain the first argument, and r8 (t6) should contain the second argument.
        # On return, rax contains a pointer to this stack value, and we then need to copy the 16 byte return value into rax (t0) and rdx (t1)
        # since the return value is expected to be split between the two.
        # See http://msdn.microsoft.com/en-us/library/7572ztz4.aspx
        move arg1, t1
        move arg2, t6
        subp 48, sp
        move sp, t2
        addp 32, t2
        call function
        addp 48, sp
        move 8[t0], t1
        move [t0], t0
    elsif ARM64
        move arg1, t0
        move arg2, t1
        call function
    elsif C_LOOP
        cloopCallSlowPath function, arg1, arg2
    else
        error
    end
end

macro cCall2Void(function, arg1, arg2)
    if C_LOOP
        cloopCallSlowPathVoid function, arg1, arg2
    elsif X86_64_WIN
        # Note: we cannot use the cCall2 macro for Win64 in this case,
        # as the Win64 cCall2 implemenation is only correct when the return type size is > 8 bytes.
        # On Win64, rcx and rdx are used for passing the first two parameters.
        # We also need to make room on the stack for all four parameter registers.
        # See http://msdn.microsoft.com/en-us/library/ms235286.aspx
        move arg2, t1
        move arg1, t2
        subp 32, sp 
        call function
        addp 32, sp 
    else
        cCall2(function, arg1, arg2)
    end
end

# This barely works. arg3 and arg4 should probably be immediates.
macro cCall4(function, arg1, arg2, arg3, arg4)
    checkStackPointerAlignment(t4, 0xbad0c004)
    if X86_64
        move arg1, t4
        move arg2, t5
        move arg3, t1
        move arg4, t2
        call function
    elsif X86_64_WIN
        # On Win64, rcx, rdx, r8, and r9 are used for passing the first four parameters.
        # We also need to make room on the stack for all four parameter registers.
        # See http://msdn.microsoft.com/en-us/library/ms235286.aspx
        move arg1, t2
        move arg2, t1
        move arg3, t6
        move arg4, t7
        subp 32, sp 
        call function
        addp 32, sp 
    elsif ARM64
        move arg1, t0
        move arg2, t1
        move arg3, t2
        move arg4, t3
        call function
    elsif C_LOOP
        error
    else
        error
    end
end

macro doCallToJavaScript(makeCall)
    if X86_64
        const entry = t4
        const vm = t5
        const protoCallFrame = t1

        const previousCFR = t0
        const previousPC = t6
        const temp1 = t0
        const temp2 = t3
        const temp3 = t6
    elsif X86_64_WIN
        const entry = t2
        const vm = t1
        const protoCallFrame = t6

        const previousCFR = t0
        const previousPC = t4
        const temp1 = t0
        const temp2 = t3
        const temp3 = t7
    elsif ARM64 or C_LOOP
        const entry = a0
        const vm = a1
        const protoCallFrame = a2

        const previousCFR = t5
        const previousPC = lr
        const temp1 = t3
        const temp2 = t4
        const temp3 = t6
    end

    callToJavaScriptPrologue()

    if X86_64
        loadp 7*8[sp], previousPC
        move 6*8[sp], previousCFR
    elsif X86_64_WIN
        # Win64 pushes two more registers
        loadp 9*8[sp], previousPC
        move 8*8[sp], previousCFR
    elsif ARM64
        move cfr, previousCFR
    end

    checkStackPointerAlignment(temp2, 0xbad0dc01)

    # The stack reserved zone ensures that we have adequate space for the
    # VMEntrySentinelFrame. Proceed with allocating and initializing the
    # sentinel frame.
    move sp, cfr
    subp CallFrameHeaderSlots * 8, cfr
    storep 0, ArgumentCount[cfr]
    storep vm, Callee[cfr]
    loadp VM::topCallFrame[vm], temp2
    storep temp2, ScopeChain[cfr]
    storep 1, CodeBlock[cfr]

    storep previousPC, ReturnPC[cfr]
    storep previousCFR, CallerFrame[cfr]

    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], temp2
    addp CallFrameHeaderSlots, temp2, temp2
    lshiftp 3, temp2
    subp cfr, temp2, temp1

    # Ensure that we have enough additional stack capacity for the incoming args,
    # and the frame for the JS code we're executing. We need to do this check
    # before we start copying the args from the protoCallFrame below.
    bpaeq temp1, VM::m_jsStackLimit[vm], .stackHeightOK

    move cfr, sp

    if C_LOOP
        move entry, temp2
        move vm, temp3
        cloopCallSlowPath _llint_stack_check_at_vm_entry, vm, temp1
        bpeq t0, 0, .stackCheckFailed
        move temp2, entry
        move temp3, vm
        jmp .stackHeightOK

.stackCheckFailed:
        move temp2, entry
        move temp3, vm
    end

    cCall2(_llint_throw_stack_overflow_error, vm, protoCallFrame)
    callToJavaScriptEpilogue()
    ret

.stackHeightOK:
    move temp1, sp
    move 5, temp1

.copyHeaderLoop:
    subi 1, temp1
    loadq [protoCallFrame, temp1, 8], temp3
    storeq temp3, CodeBlock[sp, temp1, 8]
    btinz temp1, .copyHeaderLoop

    loadi PayloadOffset + ProtoCallFrame::argCountAndCodeOriginValue[protoCallFrame], temp2
    subi 1, temp2
    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], temp3
    subi 1, temp3

    bieq temp2, temp3, .copyArgs
    move ValueUndefined, temp1
.fillExtraArgsLoop:
    subi 1, temp3
    storeq temp1, ThisArgumentOffset + 8[sp, temp3, 8]
    bineq temp2, temp3, .fillExtraArgsLoop

.copyArgs:
    loadp ProtoCallFrame::args[protoCallFrame], temp1

.copyArgsLoop:
    btiz temp2, .copyArgsDone
    subi 1, temp2
    loadq [temp1, temp2, 8], temp3
    storeq temp3, ThisArgumentOffset + 8[sp, temp2, 8]
    jmp .copyArgsLoop

.copyArgsDone:
    if ARM64
        move sp, temp2
        storep temp2, VM::topCallFrame[vm]
    else
        storep sp, VM::topCallFrame[vm]
    end

    move 0xffff000000000000, csr1
    addp 2, csr1, csr2

    checkStackPointerAlignment(temp3, 0xbad0dc02)

    makeCall(entry, temp1)

    checkStackPointerAlignment(temp3, 0xbad0dc03)

    bpeq CodeBlock[cfr], 1, .calleeFramePopped
    loadp CallerFrame[cfr], cfr

.calleeFramePopped:
    loadp Callee[cfr], temp2 # VM
    loadp ScopeChain[cfr], temp3 # previous topCallFrame
    storep temp3, VM::topCallFrame[temp2]

    checkStackPointerAlignment(temp3, 0xbad0dc04)

    if X86_64 or X86_64_WIN
        pop t5
    end
    callToJavaScriptEpilogue()

    ret
end


macro makeJavaScriptCall(entry, temp)
    addp 16, sp
    if C_LOOP
        cloopCallJSFunction entry
    else
        call entry
    end
    subp 16, sp
end


macro makeHostFunctionCall(entry, temp)
    move entry, temp
    if X86_64
        move sp, t4
    elsif X86_64_WIN
        move sp, t2
    elsif ARM64 or C_LOOP
        move sp, a0
    end
    if C_LOOP
        storep cfr, [sp]
        storep lr, 8[sp]
        cloopCallNative temp
    elsif X86_64_WIN
        # For a host function call, JIT relies on that the CallerFrame (frame pointer) is put on the stack,
        # On Win64 we need to manually copy the frame pointer to the stack, since MSVC may not maintain a frame pointer on 64-bit.
        # See http://msdn.microsoft.com/en-us/library/9z1stfyw.aspx where it's stated that rbp MAY be used as a frame pointer.
        storep cfr, [sp]

        # We need to allocate 32 bytes on the stack for the shadow space.
        subp 32, sp
        call temp
        addp 32, sp
    else
        addp 16, sp
        call temp
        subp 16, sp
    end
end


_handleUncaughtException:
    loadp ScopeChain[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    loadp VM::callFrameForThrow[t3], cfr

    # So far, we've unwound the stack to the frame just below the sentinel frame, except
    # in the case of stack overflow in the first function called from callToJavaScript.
    # Check if we need to pop to the sentinel frame and do the necessary clean up for
    # returning to the caller C frame.
    bpeq CodeBlock[cfr], 1, .handleUncaughtExceptionAlreadyIsSentinel
    loadp CallerFrame[cfr], cfr
.handleUncaughtExceptionAlreadyIsSentinel:

    loadp Callee[cfr], t3 # VM
    loadp ScopeChain[cfr], t5 # previous topCallFrame
    storep t5, VM::topCallFrame[t3]

    callToJavaScriptEpilogue()
    ret


macro prepareStateForCCall()
    leap [PB, PC, 8], PC
    move PB, t3
end

macro restoreStateAfterCCall()
    move t0, PC
    move t3, PB
    subp PB, PC
    rshiftp 3, PC
end

macro callSlowPath(slowPath)
    prepareStateForCCall()
    cCall2(slowPath, cfr, PC)
    restoreStateAfterCCall()
end

macro traceOperand(fromWhere, operand)
    prepareStateForCCall()
    cCall4(_llint_trace_operand, cfr, PC, fromWhere, operand)
    restoreStateAfterCCall()
end

macro traceValue(fromWhere, operand)
    prepareStateForCCall()
    cCall4(_llint_trace_value, cfr, PC, fromWhere, operand)
    restoreStateAfterCCall()
end

# Call a slow path for call call opcodes.
macro callCallSlowPath(slowPath, action)
    storei PC, ArgumentCount + TagOffset[cfr]
    prepareStateForCCall()
    cCall2(slowPath, cfr, PC)
    action(t0)
end

macro callWatchdogTimerHandler(throwHandler)
    storei PC, ArgumentCount + TagOffset[cfr]
    prepareStateForCCall()
    cCall2(_llint_slow_path_handle_watchdog_timer, cfr, PC)
    btpnz t0, throwHandler
    move t3, PB
    loadi ArgumentCount + TagOffset[cfr], PC
end

macro checkSwitchToJITForLoop()
    checkSwitchToJIT(
        1,
        macro()
            storei PC, ArgumentCount + TagOffset[cfr]
            prepareStateForCCall()
            cCall2(_llint_loop_osr, cfr, PC)
            btpz t0, .recover
            move t1, sp
            jmp t0
        .recover:
            move t3, PB
            loadi ArgumentCount + TagOffset[cfr], PC
        end)
end

macro loadVariable(operand, value)
    loadisFromInstruction(operand, value)
    loadq [cfr, value, 8], value
end

# Index and value must be different registers. Index may be clobbered.
macro loadConstantOrVariable(index, value)
    bpgteq index, FirstConstantRegisterIndex, .constant
    loadq [cfr, index, 8], value
    jmp .done
.constant:
    loadp CodeBlock[cfr], value
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[value], value
    subp FirstConstantRegisterIndex, index
    loadq [value, index, 8], value
.done:
end

macro loadConstantOrVariableInt32(index, value, slow)
    loadConstantOrVariable(index, value)
    bqb value, tagTypeNumber, slow
end

macro loadConstantOrVariableCell(index, value, slow)
    loadConstantOrVariable(index, value)
    btqnz value, tagMask, slow
end

macro writeBarrierOnOperand(cellOperand)
    if GGC
        loadisFromInstruction(cellOperand, t1)
        loadConstantOrVariableCell(t1, t2, .writeBarrierDone)
        checkMarkByte(t2, t1, t3, 
            macro(gcData)
                btbnz gcData, .writeBarrierDone
                push PB, PC
                cCall2Void(_llint_write_barrier_slow, cfr, t2)
                pop PC, PB
            end
        )
    .writeBarrierDone:
    end
end

macro writeBarrierOnOperands(cellOperand, valueOperand)
    if GGC
        loadisFromInstruction(valueOperand, t1)
        loadConstantOrVariableCell(t1, t0, .writeBarrierDone)
        btpz t0, .writeBarrierDone
    
        writeBarrierOnOperand(cellOperand)
    .writeBarrierDone:
    end
end

macro writeBarrierOnGlobalObject(valueOperand)
    if GGC
        loadisFromInstruction(valueOperand, t1)
        loadConstantOrVariableCell(t1, t0, .writeBarrierDone)
        btpz t0, .writeBarrierDone
    
        loadp CodeBlock[cfr], t3
        loadp CodeBlock::m_globalObject[t3], t3
        checkMarkByte(t3, t1, t2,
            macro(gcData)
                btbnz gcData, .writeBarrierDone
                push PB, PC
                cCall2Void(_llint_write_barrier_slow, cfr, t3)
                pop PC, PB
            end
        )
    .writeBarrierDone:
    end
end

macro valueProfile(value, operand, scratch)
    loadpFromInstruction(operand, scratch)
    storeq value, ValueProfile::m_buckets[scratch]
end

macro loadStructure(cell, structure)
end

macro loadStructureWithScratch(cell, structure, scratch)
    loadp CodeBlock[cfr], scratch
    loadp CodeBlock::m_vm[scratch], scratch
    loadp VM::heap + Heap::m_structureIDTable + StructureIDTable::m_table[scratch], scratch
    loadi JSCell::m_structureID[cell], structure
    loadp [scratch, structure, 8], structure
end

macro loadStructureAndClobberFirstArg(cell, structure)
    loadi JSCell::m_structureID[cell], structure
    loadp CodeBlock[cfr], cell
    loadp CodeBlock::m_vm[cell], cell
    loadp VM::heap + Heap::m_structureIDTable + StructureIDTable::m_table[cell], cell
    loadp [cell, structure, 8], structure
end

macro storeStructureWithTypeInfo(cell, structure, scratch)
    loadq Structure::m_blob + StructureIDBlob::u.doubleWord[structure], scratch
    storeq scratch, JSCell::m_structureID[cell]
end

# Entrypoints into the interpreter.

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
macro functionArityCheck(doneLabel, slowPath)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    biaeq t0, CodeBlock::m_numParameters[t1], doneLabel
    prepareStateForCCall()
    cCall2(slowPath, cfr, PC)   # This slowPath has the protocol: t0 = 0 => no error, t0 != 0 => error
    btiz t0, .noError
    move t1, cfr   # t1 contains caller frame
    jmp _llint_throw_from_slow_path_trampoline

.noError:
    # t1 points to ArityCheckData.
    loadp CommonSlowPaths::ArityCheckData::thunkToCall[t1], t2
    btpz t2, .proceedInline
    
    loadp CommonSlowPaths::ArityCheckData::returnPC[t1], t7
    loadp CommonSlowPaths::ArityCheckData::paddedStackSpace[t1], t0
    call t2
    if ASSERT_ENABLED
        loadp ReturnPC[cfr], t0
        loadp [t0], t0
    end
    jmp .continue

.proceedInline:
    loadi CommonSlowPaths::ArityCheckData::paddedStackSpace[t1], t1
    btiz t1, .continue

    // Move frame up "t1 * 2" slots
    lshiftp 1, t1
    negq t1
    move cfr, t3
    loadi PayloadOffset + ArgumentCount[cfr], t2
    addi CallFrameHeaderSlots, t2
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

    lshiftp 3, t1
    addp t1, cfr
    addp t1, sp

.continue:
    # Reload CodeBlock and reset PC, since the slow_path clobbered them.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_instructions[t1], PB
    move 0, PC
    jmp doneLabel
end

macro branchIfException(label)
    loadp ScopeChain[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    btqz VM::m_exception[t3], .noException
    jmp label
.noException:
end


# Instruction implementations

_llint_op_enter:
    traceExecution()
    checkStackPointerAlignment(t2, 0xdead00e1)
    loadp CodeBlock[cfr], t2                // t2<CodeBlock> = cfr.CodeBlock
    loadi CodeBlock::m_numVars[t2], t2      // t2<size_t> = t2<CodeBlock>.m_numVars
    btiz t2, .opEnterDone
    move ValueUndefined, t0
    negi t2
    sxi2q t2, t2
.opEnterLoop:
    storeq t0, [cfr, t2, 8]
    addq 1, t2
    btqnz t2, .opEnterLoop
.opEnterDone:
    callSlowPath(_slow_path_enter)
    dispatch(1)


_llint_op_create_activation:
    traceExecution()
    loadisFromInstruction(1, t0)
    bqneq [cfr, t0, 8], ValueEmpty, .opCreateActivationDone
    callSlowPath(_llint_slow_path_create_activation)
.opCreateActivationDone:
    dispatch(2)


_llint_op_init_lazy_reg:
    traceExecution()
    loadisFromInstruction(1, t0)
    storeq ValueEmpty, [cfr, t0, 8]
    dispatch(2)


_llint_op_create_arguments:
    traceExecution()
    loadisFromInstruction(1, t0)
    bqneq [cfr, t0, 8], ValueEmpty, .opCreateArgumentsDone
    callSlowPath(_slow_path_create_arguments)
.opCreateArgumentsDone:
    dispatch(2)


_llint_op_create_this:
    traceExecution()
    loadisFromInstruction(2, t0)
    loadp [cfr, t0, 8], t0
    loadp JSFunction::m_allocationProfile + ObjectAllocationProfile::m_allocator[t0], t1
    loadp JSFunction::m_allocationProfile + ObjectAllocationProfile::m_structure[t0], t2
    btpz t1, .opCreateThisSlow
    allocateJSObject(t1, t2, t0, t3, .opCreateThisSlow)
    loadisFromInstruction(1, t1)
    storeq t0, [cfr, t1, 8]
    dispatch(4)

.opCreateThisSlow:
    callSlowPath(_slow_path_create_this)
    dispatch(4)


_llint_op_get_callee:
    traceExecution()
    loadisFromInstruction(1, t0)
    loadp Callee[cfr], t1
    loadpFromInstruction(2, t2)
    bpneq t1, t2, .opGetCalleeSlow
    storep t1, [cfr, t0, 8]
    dispatch(3)

.opGetCalleeSlow:
    callSlowPath(_slow_path_get_callee)
    dispatch(3)

_llint_op_to_this:
    traceExecution()
    loadisFromInstruction(1, t0)
    loadq [cfr, t0, 8], t0
    btqnz t0, tagMask, .opToThisSlow
    bbneq JSCell::m_type[t0], FinalObjectType, .opToThisSlow
    loadStructureWithScratch(t0, t1, t2)
    loadpFromInstruction(2, t2)
    bpneq t1, t2, .opToThisSlow
    dispatch(3)

.opToThisSlow:
    callSlowPath(_slow_path_to_this)
    dispatch(3)


_llint_op_new_object:
    traceExecution()
    loadpFromInstruction(3, t0)
    loadp ObjectAllocationProfile::m_allocator[t0], t1
    loadp ObjectAllocationProfile::m_structure[t0], t2
    allocateJSObject(t1, t2, t0, t3, .opNewObjectSlow)
    loadisFromInstruction(1, t1)
    storeq t0, [cfr, t1, 8]
    dispatch(4)

.opNewObjectSlow:
    callSlowPath(_llint_slow_path_new_object)
    dispatch(4)


_llint_op_mov:
    traceExecution()
    loadisFromInstruction(2, t1)
    loadisFromInstruction(1, t0)
    loadConstantOrVariable(t1, t2)
    storeq t2, [cfr, t0, 8]
    dispatch(3)


macro notifyWrite(set, value, scratch, slow)
    loadb VariableWatchpointSet::m_state[set], scratch
    bieq scratch, IsInvalidated, .done
    bqneq value, VariableWatchpointSet::m_inferredValue[set], slow
.done:
end

_llint_op_captured_mov:
    traceExecution()
    loadisFromInstruction(2, t1)
    loadConstantOrVariable(t1, t2)
    loadpFromInstruction(3, t0)
    btpz t0, .opCapturedMovReady
    notifyWrite(t0, t2, t1, .opCapturedMovSlow)
.opCapturedMovReady:
    loadisFromInstruction(1, t0)
    storeq t2, [cfr, t0, 8]
    dispatch(4)

.opCapturedMovSlow:
    callSlowPath(_slow_path_captured_mov)
    dispatch(4)


_llint_op_not:
    traceExecution()
    loadisFromInstruction(2, t0)
    loadisFromInstruction(1, t1)
    loadConstantOrVariable(t0, t2)
    xorq ValueFalse, t2
    btqnz t2, ~1, .opNotSlow
    xorq ValueTrue, t2
    storeq t2, [cfr, t1, 8]
    dispatch(3)

.opNotSlow:
    callSlowPath(_slow_path_not)
    dispatch(3)


macro equalityComparison(integerComparison, slowPath)
    traceExecution()
    loadisFromInstruction(3, t0)
    loadisFromInstruction(2, t2)
    loadisFromInstruction(1, t3)
    loadConstantOrVariableInt32(t0, t1, .slow)
    loadConstantOrVariableInt32(t2, t0, .slow)
    integerComparison(t0, t1, t0)
    orq ValueFalse, t0
    storeq t0, [cfr, t3, 8]
    dispatch(4)

.slow:
    callSlowPath(slowPath)
    dispatch(4)
end

_llint_op_eq:
    equalityComparison(
        macro (left, right, result) cieq left, right, result end,
        _slow_path_eq)


_llint_op_neq:
    equalityComparison(
        macro (left, right, result) cineq left, right, result end,
        _slow_path_neq)


macro equalNullComparison()
    loadisFromInstruction(2, t0)
    loadq [cfr, t0, 8], t0
    btqnz t0, tagMask, .immediate
    btbnz JSCell::m_flags[t0], MasqueradesAsUndefined, .masqueradesAsUndefined
    move 0, t0
    jmp .done
.masqueradesAsUndefined:
    loadStructureWithScratch(t0, t2, t1)
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    cpeq Structure::m_globalObject[t2], t0, t0
    jmp .done
.immediate:
    andq ~TagBitUndefined, t0
    cqeq t0, ValueNull, t0
.done:
end

_llint_op_eq_null:
    traceExecution()
    equalNullComparison()
    loadisFromInstruction(1, t1)
    orq ValueFalse, t0
    storeq t0, [cfr, t1, 8]
    dispatch(3)


_llint_op_neq_null:
    traceExecution()
    equalNullComparison()
    loadisFromInstruction(1, t1)
    xorq ValueTrue, t0
    storeq t0, [cfr, t1, 8]
    dispatch(3)


macro strictEq(equalityOperation, slowPath)
    traceExecution()
    loadisFromInstruction(3, t0)
    loadisFromInstruction(2, t2)
    loadConstantOrVariable(t0, t1)
    loadConstantOrVariable(t2, t0)
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
    loadisFromInstruction(1, t1)
    orq ValueFalse, t0
    storeq t0, [cfr, t1, 8]
    dispatch(4)

.slow:
    callSlowPath(slowPath)
    dispatch(4)
end

_llint_op_stricteq:
    strictEq(
        macro (left, right, result) cqeq left, right, result end,
        _slow_path_stricteq)


_llint_op_nstricteq:
    strictEq(
        macro (left, right, result) cqneq left, right, result end,
        _slow_path_nstricteq)


macro preOp(arithmeticOperation, slowPath)
    traceExecution()
    loadisFromInstruction(1, t0)
    loadq [cfr, t0, 8], t1
    bqb t1, tagTypeNumber, .slow
    arithmeticOperation(t1, .slow)
    orq tagTypeNumber, t1
    storeq t1, [cfr, t0, 8]
    dispatch(2)

.slow:
    callSlowPath(slowPath)
    dispatch(2)
end

_llint_op_inc:
    preOp(
        macro (value, slow) baddio 1, value, slow end,
        _slow_path_inc)


_llint_op_dec:
    preOp(
        macro (value, slow) bsubio 1, value, slow end,
        _slow_path_dec)


_llint_op_to_number:
    traceExecution()
    loadisFromInstruction(2, t0)
    loadisFromInstruction(1, t1)
    loadConstantOrVariable(t0, t2)
    bqaeq t2, tagTypeNumber, .opToNumberIsImmediate
    btqz t2, tagTypeNumber, .opToNumberSlow
.opToNumberIsImmediate:
    storeq t2, [cfr, t1, 8]
    dispatch(3)

.opToNumberSlow:
    callSlowPath(_slow_path_to_number)
    dispatch(3)


_llint_op_negate:
    traceExecution()
    loadisFromInstruction(2, t0)
    loadisFromInstruction(1, t1)
    loadConstantOrVariable(t0, t2)
    bqb t2, tagTypeNumber, .opNegateNotInt
    btiz t2, 0x7fffffff, .opNegateSlow
    negi t2
    orq tagTypeNumber, t2
    storeq t2, [cfr, t1, 8]
    dispatch(3)
.opNegateNotInt:
    btqz t2, tagTypeNumber, .opNegateSlow
    xorq 0x8000000000000000, t2
    storeq t2, [cfr, t1, 8]
    dispatch(3)

.opNegateSlow:
    callSlowPath(_slow_path_negate)
    dispatch(3)


macro binaryOpCustomStore(integerOperationAndStore, doubleOperation, slowPath)
    loadisFromInstruction(3, t0)
    loadisFromInstruction(2, t2)
    loadConstantOrVariable(t0, t1)
    loadConstantOrVariable(t2, t0)
    bqb t0, tagTypeNumber, .op1NotInt
    bqb t1, tagTypeNumber, .op2NotInt
    loadisFromInstruction(1, t2)
    integerOperationAndStore(t1, t0, .slow, t2)
    dispatch(5)

.op1NotInt:
    # First operand is definitely not an int, the second operand could be anything.
    btqz t0, tagTypeNumber, .slow
    bqaeq t1, tagTypeNumber, .op1NotIntOp2Int
    btqz t1, tagTypeNumber, .slow
    addq tagTypeNumber, t1
    fq2d t1, ft1
    jmp .op1NotIntReady
.op1NotIntOp2Int:
    ci2d t1, ft1
.op1NotIntReady:
    loadisFromInstruction(1, t2)
    addq tagTypeNumber, t0
    fq2d t0, ft0
    doubleOperation(ft1, ft0)
    fd2q ft0, t0
    subq tagTypeNumber, t0
    storeq t0, [cfr, t2, 8]
    dispatch(5)

.op2NotInt:
    # First operand is definitely an int, the second is definitely not.
    loadisFromInstruction(1, t2)
    btqz t1, tagTypeNumber, .slow
    ci2d t0, ft0
    addq tagTypeNumber, t1
    fq2d t1, ft1
    doubleOperation(ft1, ft0)
    fd2q ft0, t0
    subq tagTypeNumber, t0
    storeq t0, [cfr, t2, 8]
    dispatch(5)

.slow:
    callSlowPath(slowPath)
    dispatch(5)
end

macro binaryOp(integerOperation, doubleOperation, slowPath)
    binaryOpCustomStore(
        macro (left, right, slow, index)
            integerOperation(left, right, slow)
            orq tagTypeNumber, right
            storeq right, [cfr, index, 8]
        end,
        doubleOperation, slowPath)
end

_llint_op_add:
    traceExecution()
    binaryOp(
        macro (left, right, slow) baddio left, right, slow end,
        macro (left, right) addd left, right end,
        _slow_path_add)


_llint_op_mul:
    traceExecution()
    binaryOpCustomStore(
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
        macro (left, right) muld left, right end,
        _slow_path_mul)


_llint_op_sub:
    traceExecution()
    binaryOp(
        macro (left, right, slow) bsubio left, right, slow end,
        macro (left, right) subd left, right end,
        _slow_path_sub)


_llint_op_div:
    traceExecution()
    if X86_64 or X86_64_WIN
        binaryOpCustomStore(
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
            macro (left, right) divd left, right end,
            _slow_path_div)
    else
        callSlowPath(_slow_path_div)
        dispatch(5)
    end


macro bitOp(operation, slowPath, advance)
    loadisFromInstruction(3, t0)
    loadisFromInstruction(2, t2)
    loadisFromInstruction(1, t3)
    loadConstantOrVariable(t0, t1)
    loadConstantOrVariable(t2, t0)
    bqb t0, tagTypeNumber, .slow
    bqb t1, tagTypeNumber, .slow
    operation(t1, t0)
    orq tagTypeNumber, t0
    storeq t0, [cfr, t3, 8]
    dispatch(advance)

.slow:
    callSlowPath(slowPath)
    dispatch(advance)
end

_llint_op_lshift:
    traceExecution()
    bitOp(
        macro (left, right) lshifti left, right end,
        _slow_path_lshift,
        4)


_llint_op_rshift:
    traceExecution()
    bitOp(
        macro (left, right) rshifti left, right end,
        _slow_path_rshift,
        4)


_llint_op_urshift:
    traceExecution()
    bitOp(
        macro (left, right) urshifti left, right end,
        _slow_path_urshift,
        4)


_llint_op_unsigned:
    traceExecution()
    loadisFromInstruction(1, t0)
    loadisFromInstruction(2, t1)
    loadConstantOrVariable(t1, t2)
    bilt t2, 0, .opUnsignedSlow
    storeq t2, [cfr, t0, 8]
    dispatch(3)
.opUnsignedSlow:
    callSlowPath(_slow_path_unsigned)
    dispatch(3)


_llint_op_bitand:
    traceExecution()
    bitOp(
        macro (left, right) andi left, right end,
        _slow_path_bitand,
        5)


_llint_op_bitxor:
    traceExecution()
    bitOp(
        macro (left, right) xori left, right end,
        _slow_path_bitxor,
        5)


_llint_op_bitor:
    traceExecution()
    bitOp(
        macro (left, right) ori left, right end,
        _slow_path_bitor,
        5)


_llint_op_check_has_instance:
    traceExecution()
    loadisFromInstruction(3, t1)
    loadConstantOrVariableCell(t1, t0, .opCheckHasInstanceSlow)
    btbz JSCell::m_flags[t0], ImplementsDefaultHasInstance, .opCheckHasInstanceSlow
    dispatch(5)

.opCheckHasInstanceSlow:
    callSlowPath(_llint_slow_path_check_has_instance)
    dispatch(0)


_llint_op_instanceof:
    traceExecution()
    # Actually do the work.
    loadisFromInstruction(3, t0)
    loadConstantOrVariableCell(t0, t1, .opInstanceofSlow)
    bbb JSCell::m_type[t1], ObjectType, .opInstanceofSlow
    loadisFromInstruction(2, t0)
    loadConstantOrVariableCell(t0, t2, .opInstanceofSlow)
    
    # Register state: t1 = prototype, t2 = value
    move 1, t0
.opInstanceofLoop:
    loadStructureAndClobberFirstArg(t2, t3)
    loadq Structure::m_prototype[t3], t2
    bqeq t2, t1, .opInstanceofDone
    btqz t2, tagMask, .opInstanceofLoop

    move 0, t0
.opInstanceofDone:
    orq ValueFalse, t0
    loadisFromInstruction(1, t3)
    storeq t0, [cfr, t3, 8]
    dispatch(4)

.opInstanceofSlow:
    callSlowPath(_llint_slow_path_instanceof)
    dispatch(4)


_llint_op_is_undefined:
    traceExecution()
    loadisFromInstruction(2, t1)
    loadisFromInstruction(1, t2)
    loadConstantOrVariable(t1, t0)
    btqz t0, tagMask, .opIsUndefinedCell
    cqeq t0, ValueUndefined, t3
    orq ValueFalse, t3
    storeq t3, [cfr, t2, 8]
    dispatch(3)
.opIsUndefinedCell:
    btbnz JSCell::m_flags[t0], MasqueradesAsUndefined, .masqueradesAsUndefined
    move ValueFalse, t1
    storeq t1, [cfr, t2, 8]
    dispatch(3)
.masqueradesAsUndefined:
    loadStructureWithScratch(t0, t3, t1)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_globalObject[t1], t1
    cpeq Structure::m_globalObject[t3], t1, t0
    orq ValueFalse, t0
    storeq t0, [cfr, t2, 8]
    dispatch(3)


_llint_op_is_boolean:
    traceExecution()
    loadisFromInstruction(2, t1)
    loadisFromInstruction(1, t2)
    loadConstantOrVariable(t1, t0)
    xorq ValueFalse, t0
    tqz t0, ~1, t0
    orq ValueFalse, t0
    storeq t0, [cfr, t2, 8]
    dispatch(3)


_llint_op_is_number:
    traceExecution()
    loadisFromInstruction(2, t1)
    loadisFromInstruction(1, t2)
    loadConstantOrVariable(t1, t0)
    tqnz t0, tagTypeNumber, t1
    orq ValueFalse, t1
    storeq t1, [cfr, t2, 8]
    dispatch(3)


_llint_op_is_string:
    traceExecution()
    loadisFromInstruction(2, t1)
    loadisFromInstruction(1, t2)
    loadConstantOrVariable(t1, t0)
    btqnz t0, tagMask, .opIsStringNotCell
    cbeq JSCell::m_type[t0], StringType, t1
    orq ValueFalse, t1
    storeq t1, [cfr, t2, 8]
    dispatch(3)
.opIsStringNotCell:
    storeq ValueFalse, [cfr, t2, 8]
    dispatch(3)


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

_llint_op_init_global_const:
    traceExecution()
    writeBarrierOnGlobalObject(2)
    loadisFromInstruction(2, t1)
    loadpFromInstruction(1, t0)
    loadConstantOrVariable(t1, t2)
    storeq t2, [t0]
    dispatch(5)


macro getById(getPropertyStorage)
    traceExecution()
    # We only do monomorphic get_by_id caching for now, and we do not modify the
    # opcode. We do, however, allow for the cache to change anytime if fails, since
    # ping-ponging is free. At best we get lucky and the get_by_id will continue
    # to take fast path on the new cache. At worst we take slow path, which is what
    # we would have been doing anyway.
    loadisFromInstruction(2, t0)
    loadConstantOrVariableCell(t0, t3, .opGetByIdSlow)
    loadStructureWithScratch(t3, t2, t1)
    loadpFromInstruction(4, t1)
    bpneq t2, t1, .opGetByIdSlow
    getPropertyStorage(
        t3,
        t0,
        macro (propertyStorage, scratch)
            loadisFromInstruction(5, t2)
            loadisFromInstruction(1, t1)
            loadq [propertyStorage, t2], scratch
            storeq scratch, [cfr, t1, 8]
            valueProfile(scratch, 8, t1)
            dispatch(9)
        end)
            
    .opGetByIdSlow:
        callSlowPath(_llint_slow_path_get_by_id)
        dispatch(9)
end

_llint_op_get_by_id:
    getById(withInlineStorage)


_llint_op_get_by_id_out_of_line:
    getById(withOutOfLineStorage)


_llint_op_get_array_length:
    traceExecution()
    loadisFromInstruction(2, t0)
    loadpFromInstruction(4, t1)
    loadConstantOrVariableCell(t0, t3, .opGetArrayLengthSlow)
    move t3, t2
    arrayProfile(t2, t1, t0)
    btiz t2, IsArray, .opGetArrayLengthSlow
    btiz t2, IndexingShapeMask, .opGetArrayLengthSlow
    loadisFromInstruction(1, t1)
    loadp JSObject::m_butterfly[t3], t0
    loadi -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], t0
    bilt t0, 0, .opGetArrayLengthSlow
    orq tagTypeNumber, t0
    valueProfile(t0, 8, t2)
    storeq t0, [cfr, t1, 8]
    dispatch(9)

.opGetArrayLengthSlow:
    callSlowPath(_llint_slow_path_get_by_id)
    dispatch(9)


_llint_op_get_arguments_length:
    traceExecution()
    loadisFromInstruction(2, t0)
    loadisFromInstruction(1, t1)
    btqnz [cfr, t0, 8], .opGetArgumentsLengthSlow
    loadi ArgumentCount + PayloadOffset[cfr], t2
    subi 1, t2
    orq tagTypeNumber, t2
    storeq t2, [cfr, t1, 8]
    dispatch(4)

.opGetArgumentsLengthSlow:
    callSlowPath(_llint_slow_path_get_arguments_length)
    dispatch(4)


macro putById(getPropertyStorage)
    traceExecution()
    writeBarrierOnOperands(1, 3)
    loadisFromInstruction(1, t3)
    loadConstantOrVariableCell(t3, t0, .opPutByIdSlow)
    loadStructureWithScratch(t0, t2, t1)
    loadpFromInstruction(4, t1)
    bpneq t2, t1, .opPutByIdSlow
    getPropertyStorage(
        t0,
        t3,
        macro (propertyStorage, scratch)
            loadisFromInstruction(5, t1)
            loadisFromInstruction(3, t2)
            loadConstantOrVariable(t2, scratch)
            storeq scratch, [propertyStorage, t1]
            dispatch(9)
        end)
end

_llint_op_put_by_id:
    putById(withInlineStorage)

.opPutByIdSlow:
    callSlowPath(_llint_slow_path_put_by_id)
    dispatch(9)


_llint_op_put_by_id_out_of_line:
    putById(withOutOfLineStorage)


macro putByIdTransition(additionalChecks, getPropertyStorage)
    traceExecution()
    writeBarrierOnOperand(1)
    loadisFromInstruction(1, t3)
    loadpFromInstruction(4, t1)
    loadConstantOrVariableCell(t3, t0, .opPutByIdSlow)
    loadStructureWithScratch(t0, t2, t3)
    bpneq t2, t1, .opPutByIdSlow
    additionalChecks(t1, t3, t2)
    loadisFromInstruction(3, t2)
    loadisFromInstruction(5, t1)
    getPropertyStorage(
        t0,
        t3,
        macro (propertyStorage, scratch)
            addp t1, propertyStorage, t3
            loadConstantOrVariable(t2, t1)
            storeq t1, [t3]
            loadpFromInstruction(6, t1)
            loadi Structure::m_blob + StructureIDBlob::u.words.word1[t1], t1
            storei t1, JSCell::m_structureID[t0]
            dispatch(9)
        end)
end

macro noAdditionalChecks(oldStructure, scratch, scratch2)
end

macro structureChainChecks(oldStructure, scratch, scratch2)
    const protoCell = oldStructure    # Reusing the oldStructure register for the proto
    loadpFromInstruction(7, scratch)
    assert(macro (ok) btpnz scratch, ok end)
    loadp StructureChain::m_vector[scratch], scratch
    assert(macro (ok) btpnz scratch, ok end)
    bqeq Structure::m_prototype[oldStructure], ValueNull, .done
.loop:
    loadq Structure::m_prototype[oldStructure], protoCell
    loadStructureAndClobberFirstArg(protoCell, scratch2)
    move scratch2, oldStructure
    bpneq oldStructure, [scratch], .opPutByIdSlow
    addp 8, scratch
    bqneq Structure::m_prototype[oldStructure], ValueNull, .loop
.done:
end

_llint_op_put_by_id_transition_direct:
    putByIdTransition(noAdditionalChecks, withInlineStorage)


_llint_op_put_by_id_transition_direct_out_of_line:
    putByIdTransition(noAdditionalChecks, withOutOfLineStorage)


_llint_op_put_by_id_transition_normal:
    putByIdTransition(structureChainChecks, withInlineStorage)


_llint_op_put_by_id_transition_normal_out_of_line:
    putByIdTransition(structureChainChecks, withOutOfLineStorage)


_llint_op_get_by_val:
    traceExecution()
    loadisFromInstruction(2, t2)
    loadConstantOrVariableCell(t2, t0, .opGetByValSlow)
    loadpFromInstruction(4, t3)
    move t0, t2
    arrayProfile(t2, t3, t1)
    loadisFromInstruction(3, t3)
    loadConstantOrVariableInt32(t3, t1, .opGetByValSlow)
    sxi2q t1, t1
    loadp JSObject::m_butterfly[t0], t3
    andi IndexingShapeMask, t2
    bieq t2, Int32Shape, .opGetByValIsContiguous
    bineq t2, ContiguousShape, .opGetByValNotContiguous
.opGetByValIsContiguous:

    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValOutOfBounds
    loadisFromInstruction(1, t0)
    loadq [t3, t1, 8], t2
    btqz t2, .opGetByValOutOfBounds
    jmp .opGetByValDone

.opGetByValNotContiguous:
    bineq t2, DoubleShape, .opGetByValNotDouble
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValOutOfBounds
    loadis 8[PB, PC, 8], t0
    loadd [t3, t1, 8], ft0
    bdnequn ft0, ft0, .opGetByValOutOfBounds
    fd2q ft0, t2
    subq tagTypeNumber, t2
    jmp .opGetByValDone
    
.opGetByValNotDouble:
    subi ArrayStorageShape, t2
    bia t2, SlowPutArrayStorageShape - ArrayStorageShape, .opGetByValSlow
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t3], .opGetByValOutOfBounds
    loadisFromInstruction(1, t0)
    loadq ArrayStorage::m_vector[t3, t1, 8], t2
    btqz t2, .opGetByValOutOfBounds

.opGetByValDone:
    storeq t2, [cfr, t0, 8]
    valueProfile(t2, 5, t0)
    dispatch(6)

.opGetByValOutOfBounds:
    loadpFromInstruction(4, t0)
    storeb 1, ArrayProfile::m_outOfBounds[t0]
.opGetByValSlow:
    callSlowPath(_llint_slow_path_get_by_val)
    dispatch(6)


_llint_op_get_argument_by_val:
    # FIXME: At some point we should array profile this. Right now it isn't necessary
    # since the DFG will never turn a get_argument_by_val into a GetByVal.
    traceExecution()
    loadisFromInstruction(2, t0)
    loadisFromInstruction(3, t1)
    btqnz [cfr, t0, 8], .opGetArgumentByValSlow
    loadConstantOrVariableInt32(t1, t2, .opGetArgumentByValSlow)
    addi 1, t2
    loadi ArgumentCount + PayloadOffset[cfr], t1
    biaeq t2, t1, .opGetArgumentByValSlow
    loadisFromInstruction(1, t3)
    loadpFromInstruction(5, t1)
    loadq ThisArgumentOffset[cfr, t2, 8], t0
    storeq t0, [cfr, t3, 8]
    valueProfile(t0, 5, t1)
    dispatch(6)

.opGetArgumentByValSlow:
    callSlowPath(_llint_slow_path_get_argument_by_val)
    dispatch(6)


_llint_op_get_by_pname:
    traceExecution()
    loadisFromInstruction(3, t1)
    loadConstantOrVariable(t1, t0)
    loadisFromInstruction(4, t1)
    assertNotConstant(t1)
    bqneq t0, [cfr, t1, 8], .opGetByPnameSlow
    loadisFromInstruction(2, t2)
    loadisFromInstruction(5, t3)
    loadConstantOrVariableCell(t2, t0, .opGetByPnameSlow)
    assertNotConstant(t3)
    loadq [cfr, t3, 8], t1
    loadStructureWithScratch(t0, t2, t3)
    bpneq t2, JSPropertyNameIterator::m_cachedStructure[t1], .opGetByPnameSlow
    loadisFromInstruction(6, t3)
    loadi PayloadOffset[cfr, t3, 8], t3
    subi 1, t3
    biaeq t3, JSPropertyNameIterator::m_numCacheableSlots[t1], .opGetByPnameSlow
    bilt t3, JSPropertyNameIterator::m_cachedStructureInlineCapacity[t1], .opGetByPnameInlineProperty
    addi firstOutOfLineOffset, t3
    subi JSPropertyNameIterator::m_cachedStructureInlineCapacity[t1], t3
.opGetByPnameInlineProperty:
    loadPropertyAtVariableOffset(t3, t0, t0)
    loadisFromInstruction(1, t1)
    storeq t0, [cfr, t1, 8]
    dispatch(7)

.opGetByPnameSlow:
    callSlowPath(_llint_slow_path_get_by_pname)
    dispatch(7)


macro contiguousPutByVal(storeCallback)
    biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], .outOfBounds
.storeResult:
    loadisFromInstruction(3, t2)
    storeCallback(t2, t1, [t0, t3, 8])
    dispatch(5)

.outOfBounds:
    biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
    loadp 32[PB, PC, 8], t2
    storeb 1, ArrayProfile::m_mayStoreToHole[t2]
    addi 1, t3, t2
    storei t2, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
    jmp .storeResult
end

macro putByVal(slowPath)
    traceExecution()
    writeBarrierOnOperands(1, 3)
    loadisFromInstruction(1, t0)
    loadConstantOrVariableCell(t0, t1, .opPutByValSlow)
    loadpFromInstruction(4, t3)
    move t1, t2
    arrayProfile(t2, t3, t0)
    loadisFromInstruction(2, t0)
    loadConstantOrVariableInt32(t0, t3, .opPutByValSlow)
    sxi2q t3, t3
    loadp JSObject::m_butterfly[t1], t0
    andi IndexingShapeMask, t2
    bineq t2, Int32Shape, .opPutByValNotInt32
    contiguousPutByVal(
        macro (operand, scratch, address)
            loadConstantOrVariable(operand, scratch)
            bpb scratch, tagTypeNumber, .opPutByValSlow
            storep scratch, address
        end)

.opPutByValNotInt32:
    bineq t2, DoubleShape, .opPutByValNotDouble
    contiguousPutByVal(
        macro (operand, scratch, address)
            loadConstantOrVariable(operand, scratch)
            bqb scratch, tagTypeNumber, .notInt
            ci2d scratch, ft0
            jmp .ready
        .notInt:
            addp tagTypeNumber, scratch
            fq2d scratch, ft0
            bdnequn ft0, ft0, .opPutByValSlow
        .ready:
            stored ft0, address
        end)

.opPutByValNotDouble:
    bineq t2, ContiguousShape, .opPutByValNotContiguous
    contiguousPutByVal(
        macro (operand, scratch, address)
            loadConstantOrVariable(operand, scratch)
            storep scratch, address
        end)

.opPutByValNotContiguous:
    bineq t2, ArrayStorageShape, .opPutByValSlow
    biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
    btqz ArrayStorage::m_vector[t0, t3, 8], .opPutByValArrayStorageEmpty
.opPutByValArrayStorageStoreResult:
    loadisFromInstruction(3, t2)
    loadConstantOrVariable(t2, t1)
    storeq t1, ArrayStorage::m_vector[t0, t3, 8]
    dispatch(5)

.opPutByValArrayStorageEmpty:
    loadpFromInstruction(4, t1)
    storeb 1, ArrayProfile::m_mayStoreToHole[t1]
    addi 1, ArrayStorage::m_numValuesInVector[t0]
    bib t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], .opPutByValArrayStorageStoreResult
    addi 1, t3, t1
    storei t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
    jmp .opPutByValArrayStorageStoreResult

.opPutByValOutOfBounds:
    loadpFromInstruction(4, t0)
    storeb 1, ArrayProfile::m_outOfBounds[t0]
.opPutByValSlow:
    callSlowPath(slowPath)
    dispatch(5)
end

_llint_op_put_by_val:
    putByVal(_llint_slow_path_put_by_val)

_llint_op_put_by_val_direct:
    putByVal(_llint_slow_path_put_by_val_direct)


_llint_op_jmp:
    traceExecution()
    dispatchIntIndirect(1)


macro jumpTrueOrFalse(conditionOp, slow)
    loadisFromInstruction(1, t1)
    loadConstantOrVariable(t1, t0)
    xorq ValueFalse, t0
    btqnz t0, -1, .slow
    conditionOp(t0, .target)
    dispatch(3)

.target:
    dispatchIntIndirect(2)

.slow:
    callSlowPath(slow)
    dispatch(0)
end


macro equalNull(cellHandler, immediateHandler)
    loadisFromInstruction(1, t0)
    assertNotConstant(t0)
    loadq [cfr, t0, 8], t0
    btqnz t0, tagMask, .immediate
    loadStructureWithScratch(t0, t2, t1)
    cellHandler(t2, JSCell::m_flags[t0], .target)
    dispatch(3)

.target:
    dispatchIntIndirect(2)

.immediate:
    andq ~TagBitUndefined, t0
    immediateHandler(t0, .target)
    dispatch(3)
end

_llint_op_jeq_null:
    traceExecution()
    equalNull(
        macro (structure, value, target) 
            btbz value, MasqueradesAsUndefined, .notMasqueradesAsUndefined
            loadp CodeBlock[cfr], t0
            loadp CodeBlock::m_globalObject[t0], t0
            bpeq Structure::m_globalObject[structure], t0, target
.notMasqueradesAsUndefined:
        end,
        macro (value, target) bqeq value, ValueNull, target end)


_llint_op_jneq_null:
    traceExecution()
    equalNull(
        macro (structure, value, target) 
            btbz value, MasqueradesAsUndefined, target
            loadp CodeBlock[cfr], t0
            loadp CodeBlock::m_globalObject[t0], t0
            bpneq Structure::m_globalObject[structure], t0, target
        end,
        macro (value, target) bqneq value, ValueNull, target end)


_llint_op_jneq_ptr:
    traceExecution()
    loadisFromInstruction(1, t0)
    loadisFromInstruction(2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_globalObject[t2], t2
    loadp JSGlobalObject::m_specialPointers[t2, t1, 8], t1
    bpneq t1, [cfr, t0, 8], .opJneqPtrTarget
    dispatch(4)

.opJneqPtrTarget:
    dispatchIntIndirect(3)


macro compare(integerCompare, doubleCompare, slowPath)
    loadisFromInstruction(1, t2)
    loadisFromInstruction(2, t3)
    loadConstantOrVariable(t2, t0)
    loadConstantOrVariable(t3, t1)
    bqb t0, tagTypeNumber, .op1NotInt
    bqb t1, tagTypeNumber, .op2NotInt
    integerCompare(t0, t1, .jumpTarget)
    dispatch(4)

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
    dispatch(4)

.op2NotInt:
    ci2d t0, ft0
    btqz t1, tagTypeNumber, .slow
    addq tagTypeNumber, t1
    fq2d t1, ft1
    doubleCompare(ft0, ft1, .jumpTarget)
    dispatch(4)

.jumpTarget:
    dispatchIntIndirect(3)

.slow:
    callSlowPath(slowPath)
    dispatch(0)
end


_llint_op_switch_imm:
    traceExecution()
    loadisFromInstruction(3, t2)
    loadisFromInstruction(1, t3)
    loadConstantOrVariable(t2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3    # FIXME: would be nice to peephole this!
    loadp CodeBlock::RareData::m_switchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    bqb t1, tagTypeNumber, .opSwitchImmNotInt
    subi SimpleJumpTable::min[t2], t1
    biaeq t1, SimpleJumpTable::branchOffsets + VectorSizeOffset[t2], .opSwitchImmFallThrough
    loadp SimpleJumpTable::branchOffsets + VectorBufferOffset[t2], t3
    loadis [t3, t1, 4], t1
    btiz t1, .opSwitchImmFallThrough
    dispatch(t1)

.opSwitchImmNotInt:
    btqnz t1, tagTypeNumber, .opSwitchImmSlow   # Go slow if it's a double.
.opSwitchImmFallThrough:
    dispatchIntIndirect(2)

.opSwitchImmSlow:
    callSlowPath(_llint_slow_path_switch_imm)
    dispatch(0)


_llint_op_switch_char:
    traceExecution()
    loadisFromInstruction(3, t2)
    loadisFromInstruction(1, t3)
    loadConstantOrVariable(t2, t1)
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
    dispatch(t1)

.opSwitchCharFallThrough:
    dispatchIntIndirect(2)

.opSwitchOnRope:
    callSlowPath(_llint_slow_path_switch_char)
    dispatch(0)


_llint_op_new_func:
    traceExecution()
    loadisFromInstruction(3, t2)
    btiz t2, .opNewFuncUnchecked
    loadisFromInstruction(1, t1)
    btqnz [cfr, t1, 8], .opNewFuncDone
.opNewFuncUnchecked:
    callSlowPath(_llint_slow_path_new_func)
.opNewFuncDone:
    dispatch(4)


_llint_op_new_captured_func:
    traceExecution()
    callSlowPath(_slow_path_new_captured_func)
    dispatch(4)


macro arrayProfileForCall()
    loadisFromInstruction(4, t3)
    negp t3
    loadq ThisArgumentOffset[cfr, t3, 8], t0
    btqnz t0, tagMask, .done
    loadpFromInstruction((CallOpCodeSize - 2), t1)
    loadi JSCell::m_structureID[t0], t3
    storei t3, ArrayProfile::m_lastSeenStructureID[t1]
.done:
end

macro doCall(slowPath)
    loadisFromInstruction(2, t0)
    loadpFromInstruction(5, t1)
    loadp LLIntCallLinkInfo::callee[t1], t2
    loadConstantOrVariable(t0, t3)
    bqneq t3, t2, .opCallSlow
    loadisFromInstruction(4, t3)
    lshifti 3, t3
    negp t3
    addp cfr, t3
    loadp JSFunction::m_scope[t2], t0
    storeq t2, Callee[t3]
    storeq t0, ScopeChain[t3]
    loadisFromInstruction(3, t2)
    storei PC, ArgumentCount + TagOffset[cfr]
    storei t2, ArgumentCount + PayloadOffset[t3]
    addp CallerFrameAndPCSize, t3
    callTargetFunction(t1, t3)

.opCallSlow:
    slowPathForCall(slowPath)
end


_llint_op_tear_off_activation:
    traceExecution()
    loadisFromInstruction(1, t0)
    btqz [cfr, t0, 8], .opTearOffActivationNotCreated
    callSlowPath(_llint_slow_path_tear_off_activation)
.opTearOffActivationNotCreated:
    dispatch(2)


_llint_op_tear_off_arguments:
    traceExecution()
    loadisFromInstruction(1, t0)
    addq 1, t0   # Get the unmodifiedArgumentsRegister
    btqz [cfr, t0, 8], .opTearOffArgumentsNotCreated
    callSlowPath(_llint_slow_path_tear_off_arguments)
.opTearOffArgumentsNotCreated:
    dispatch(3)


_llint_op_ret:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadisFromInstruction(1, t2)
    loadConstantOrVariable(t2, t0)
    doReturn()


_llint_op_ret_object_or_this:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadisFromInstruction(1, t2)
    loadConstantOrVariable(t2, t0)
    btqnz t0, tagMask, .opRetObjectOrThisNotObject
    bbb JSCell::m_type[t0], ObjectType, .opRetObjectOrThisNotObject
    doReturn()

.opRetObjectOrThisNotObject:
    loadisFromInstruction(2, t2)
    loadConstantOrVariable(t2, t0)
    doReturn()


_llint_op_to_primitive:
    traceExecution()
    loadisFromInstruction(2, t2)
    loadisFromInstruction(1, t3)
    loadConstantOrVariable(t2, t0)
    btqnz t0, tagMask, .opToPrimitiveIsImm
    bbneq JSCell::m_type[t0], StringType, .opToPrimitiveSlowCase
.opToPrimitiveIsImm:
    storeq t0, [cfr, t3, 8]
    dispatch(3)

.opToPrimitiveSlowCase:
    callSlowPath(_slow_path_to_primitive)
    dispatch(3)


_llint_op_next_pname:
    traceExecution()
    loadisFromInstruction(3, t1)
    loadisFromInstruction(4, t2)
    assertNotConstant(t1)
    assertNotConstant(t2)
    loadi PayloadOffset[cfr, t1, 8], t0
    bieq t0, PayloadOffset[cfr, t2, 8], .opNextPnameEnd
    loadisFromInstruction(5, t2)
    assertNotConstant(t2)
    loadp [cfr, t2, 8], t2
    loadp JSPropertyNameIterator::m_jsStrings[t2], t3
    loadq [t3, t0, 8], t3
    addi 1, t0
    storei t0, PayloadOffset[cfr, t1, 8]
    loadisFromInstruction(1, t1)
    storeq t3, [cfr, t1, 8]
    loadisFromInstruction(2, t3)
    assertNotConstant(t3)
    loadq [cfr, t3, 8], t3
    loadStructureWithScratch(t3, t1, t0)
    bpneq t1, JSPropertyNameIterator::m_cachedStructure[t2], .opNextPnameSlow
    loadp JSPropertyNameIterator::m_cachedPrototypeChain[t2], t0
    loadp StructureChain::m_vector[t0], t0
    btpz [t0], .opNextPnameTarget
.opNextPnameCheckPrototypeLoop:
    bqeq Structure::m_prototype[t1], ValueNull, .opNextPnameSlow
    loadq Structure::m_prototype[t1], t2
    loadStructureWithScratch(t2, t1, t3)
    bpneq t1, [t0], .opNextPnameSlow
    addp 8, t0
    btpnz [t0], .opNextPnameCheckPrototypeLoop
.opNextPnameTarget:
    dispatchIntIndirect(6)

.opNextPnameEnd:
    dispatch(7)

.opNextPnameSlow:
    callSlowPath(_llint_slow_path_next_pname) # This either keeps the PC where it was (causing us to loop) or sets it to target.
    dispatch(0)


_llint_op_catch:
    # This is where we end up from the JIT's throw trampoline (because the
    # machine code return address will be set to _llint_op_catch), and from
    # the interpreter's throw trampoline (see _llint_throw_trampoline).
    # The throwing code must have known that we were throwing to the interpreter,
    # and have set VM::targetInterpreterPCForThrow.
    loadp ScopeChain[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    loadp VM::callFrameForThrow[t3], cfr
    restoreStackPointerAfterCall()

    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_instructions[PB], PB
    loadp VM::targetInterpreterPCForThrow[t3], PC
    subp PB, PC
    rshiftp 3, PC
    loadq VM::m_exception[t3], t0
    storeq 0, VM::m_exception[t3]
    loadisFromInstruction(1, t2)
    storeq t0, [cfr, t2, 8]
    traceExecution()
    dispatch(2)


_llint_op_end:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadisFromInstruction(1, t0)
    assertNotConstant(t0)
    loadq [cfr, t0, 8], t0
    doReturn()


_llint_throw_from_slow_path_trampoline:
    callSlowPath(_llint_slow_path_handle_exception)

    # When throwing from the interpreter (i.e. throwing from LLIntSlowPaths), so
    # the throw target is not necessarily interpreted code, we come to here.
    # This essentially emulates the JIT's throwing protocol.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_vm[t1], t1
    jmp VM::targetMachinePCForThrow[t1]


_llint_throw_during_call_trampoline:
    preserveReturnAddressAfterCall(t2)
    jmp _llint_throw_from_slow_path_trampoline


macro nativeCallTrampoline(executableOffsetToFunction)

    functionPrologue()
    storep 0, CodeBlock[cfr]
    if X86_64 or X86_64_WIN
        if X86_64
            const arg1 = t4  # t4 = rdi
            const arg2 = t5  # t5 = rsi
            const temp = t1
        elsif X86_64_WIN
            const arg1 = t2  # t2 = rcx
            const arg2 = t1  # t1 = rdx
            const temp = t0
        end
        loadp ScopeChain[cfr], t0
        andp MarkedBlockMask, t0
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t0], t0
        storep cfr, VM::topCallFrame[t0]
        loadp CallerFrame[cfr], t0
        loadq ScopeChain[t0], t1
        storeq t1, ScopeChain[cfr]
        move cfr, arg1
        loadp Callee[cfr], arg2
        loadp JSFunction::m_executable[arg2], temp
        checkStackPointerAlignment(t3, 0xdead0001)
        if X86_64_WIN
            subp 32, sp
        end
        call executableOffsetToFunction[temp]
        if X86_64_WIN
            addp 32, sp
        end
        loadp ScopeChain[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    elsif ARM64 or C_LOOP
        loadp ScopeChain[cfr], t0
        andp MarkedBlockMask, t0
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t0], t0
        storep cfr, VM::topCallFrame[t0]
        loadp CallerFrame[cfr], t2
        loadp ScopeChain[t2], t1
        storep t1, ScopeChain[cfr]
        preserveReturnAddressAfterCall(t3)
        storep t3, ReturnPC[cfr]
        move cfr, t0
        loadp Callee[cfr], t1
        loadp JSFunction::m_executable[t1], t1
        move t2, cfr # Restore cfr to avoid loading from stack
        if C_LOOP
            cloopCallNative executableOffsetToFunction[t1]
        else
            call executableOffsetToFunction[t1]
        end
        restoreReturnAddressBeforeReturn(t3)
        loadp ScopeChain[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    else
        error
    end

    functionEpilogue()

    btqnz VM::m_exception[t3], .handleException
    ret

.handleException:
    storep cfr, VM::topCallFrame[t3]
    restoreStackPointerAfterCall()
    jmp _llint_throw_from_slow_path_trampoline
end


macro getGlobalObject(dst)
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadisFromInstruction(dst, t1)
    storeq t0, [cfr, t1, 8]
end

macro varInjectionCheck(slowPath)
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_varInjectionWatchpoint[t0], t0
    bbeq WatchpointSet::m_state[t0], IsInvalidated, slowPath
end

macro resolveScope()
    loadp CodeBlock[cfr], t0
    loadisFromInstruction(4, t2)
    btbz CodeBlock::m_needsActivation[t0], .resolveScopeAfterActivationCheck
    loadis CodeBlock::m_activationRegister[t0], t1
    btpz [cfr, t1, 8], .resolveScopeAfterActivationCheck
    addi 1, t2

.resolveScopeAfterActivationCheck:
    loadp ScopeChain[cfr], t0
    btiz t2, .resolveScopeLoopEnd

.resolveScopeLoop:
    loadp JSScope::m_next[t0], t0
    subi 1, t2
    btinz t2, .resolveScopeLoop

.resolveScopeLoopEnd:
    loadisFromInstruction(1, t1)
    storeq t0, [cfr, t1, 8]
end


_llint_op_resolve_scope:
    traceExecution()
    loadisFromInstruction(3, t0)

#rGlobalProperty:
    bineq t0, GlobalProperty, .rGlobalVar
    getGlobalObject(1)
    dispatch(6)

.rGlobalVar:
    bineq t0, GlobalVar, .rClosureVar
    getGlobalObject(1)
    dispatch(6)

.rClosureVar:
    bineq t0, ClosureVar, .rGlobalPropertyWithVarInjectionChecks
    resolveScope()
    dispatch(6)

.rGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .rGlobalVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic)
    getGlobalObject(1)
    dispatch(6)

.rGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .rClosureVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic)
    getGlobalObject(1)
    dispatch(6)

.rClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .rDynamic
    varInjectionCheck(.rDynamic)
    resolveScope()
    dispatch(6)

.rDynamic:
    callSlowPath(_llint_slow_path_resolve_scope)
    dispatch(6)


macro loadWithStructureCheck(operand, slowPath)
    loadisFromInstruction(operand, t0)
    loadq [cfr, t0, 8], t0
    loadStructureWithScratch(t0, t2, t1)
    loadpFromInstruction(5, t1)
    bpneq t2, t1, slowPath
end

macro getProperty()
    loadisFromInstruction(6, t1)
    loadPropertyAtVariableOffset(t1, t0, t2)
    valueProfile(t2, 7, t0)
    loadisFromInstruction(1, t0)
    storeq t2, [cfr, t0, 8]
end

macro getGlobalVar()
    loadpFromInstruction(6, t0)
    loadq [t0], t0
    valueProfile(t0, 7, t1)
    loadisFromInstruction(1, t1)
    storeq t0, [cfr, t1, 8]
end

macro getClosureVar()
    loadp JSVariableObject::m_registers[t0], t0
    loadisFromInstruction(6, t1)
    loadq [t0, t1, 8], t0
    valueProfile(t0, 7, t1)
    loadisFromInstruction(1, t1)
    storeq t0, [cfr, t1, 8]
end

_llint_op_get_from_scope:
    traceExecution()
    loadisFromInstruction(4, t0)
    andi ResolveModeMask, t0

#gGlobalProperty:
    bineq t0, GlobalProperty, .gGlobalVar
    loadWithStructureCheck(2, .gDynamic)
    getProperty()
    dispatch(8)

.gGlobalVar:
    bineq t0, GlobalVar, .gClosureVar
    getGlobalVar()
    dispatch(8)

.gClosureVar:
    bineq t0, ClosureVar, .gGlobalPropertyWithVarInjectionChecks
    loadVariable(2, t0)
    getClosureVar()
    dispatch(8)

.gGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .gGlobalVarWithVarInjectionChecks
    loadWithStructureCheck(2, .gDynamic)
    getProperty()
    dispatch(8)

.gGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .gClosureVarWithVarInjectionChecks
    varInjectionCheck(.gDynamic)
    loadVariable(2, t0)
    getGlobalVar()
    dispatch(8)

.gClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .gDynamic
    varInjectionCheck(.gDynamic)
    loadVariable(2, t0)
    getClosureVar()
    dispatch(8)

.gDynamic:
    callSlowPath(_llint_slow_path_get_from_scope)
    dispatch(8)


macro putProperty()
    loadisFromInstruction(3, t1)
    loadConstantOrVariable(t1, t2)
    loadisFromInstruction(6, t1)
    storePropertyAtVariableOffset(t1, t0, t2)
end

macro putGlobalVar()
    loadisFromInstruction(3, t0)
    loadConstantOrVariable(t0, t1)
    loadpFromInstruction(5, t2)
    notifyWrite(t2, t1, t0, .pDynamic)
    loadpFromInstruction(6, t0)
    storeq t1, [t0]
end

macro putClosureVar()
    loadisFromInstruction(3, t1)
    loadConstantOrVariable(t1, t2)
    loadp JSVariableObject::m_registers[t0], t0
    loadisFromInstruction(6, t1)
    storeq t2, [t0, t1, 8]
end


_llint_op_put_to_scope:
    traceExecution()
    loadisFromInstruction(4, t0)
    andi ResolveModeMask, t0

#pGlobalProperty:
    bineq t0, GlobalProperty, .pGlobalVar
    writeBarrierOnOperands(1, 3)
    loadWithStructureCheck(1, .pDynamic)
    putProperty()
    dispatch(7)

.pGlobalVar:
    bineq t0, GlobalVar, .pClosureVar
    writeBarrierOnGlobalObject(3)
    putGlobalVar()
    dispatch(7)

.pClosureVar:
    bineq t0, ClosureVar, .pGlobalPropertyWithVarInjectionChecks
    writeBarrierOnOperands(1, 3)
    loadVariable(1, t0)
    putClosureVar()
    dispatch(7)

.pGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .pGlobalVarWithVarInjectionChecks
    writeBarrierOnOperands(1, 3)
    loadWithStructureCheck(1, .pDynamic)
    putProperty()
    dispatch(7)

.pGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .pClosureVarWithVarInjectionChecks
    writeBarrierOnGlobalObject(3)
    varInjectionCheck(.pDynamic)
    putGlobalVar()
    dispatch(7)

.pClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .pDynamic
    writeBarrierOnOperands(1, 3)
    varInjectionCheck(.pDynamic)
    loadVariable(1, t0)
    putClosureVar()
    dispatch(7)

.pDynamic:
    callSlowPath(_llint_slow_path_put_to_scope)
    dispatch(7)
