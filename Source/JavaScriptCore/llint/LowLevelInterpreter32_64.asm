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


# Crash course on the language that this is written in (which I just call
# "assembly" even though it's more than that):
#
# - Mostly gas-style operand ordering. The last operand tends to be the
#   destination. So "a := b" is written as "mov b, a". But unlike gas,
#   comparisons are in-order, so "if (a < b)" is written as
#   "bilt a, b, ...".
#
# - "b" = byte, "h" = 16-bit word, "i" = 32-bit word, "p" = pointer.
#   Currently this is just 32-bit so "i" and "p" are interchangeable
#   except when an op supports one but not the other.
#
# - In general, valid operands for macro invocations and instructions are
#   registers (eg "t0"), addresses (eg "4[t0]"), base-index addresses
#   (eg "7[t0, t1, 2]"), absolute addresses (eg "0xa0000000[]"), or labels
#   (eg "_foo" or ".foo"). Macro invocations can also take anonymous
#   macros as operands. Instructions cannot take anonymous macros.
#
# - Labels must have names that begin with either "_" or ".".  A "." label
#   is local and gets renamed before code gen to minimize namespace
#   pollution. A "_" label is an extern symbol (i.e. ".globl"). The "_"
#   may or may not be removed during code gen depending on whether the asm
#   conventions for C name mangling on the target platform mandate a "_"
#   prefix.
#
# - A "macro" is a lambda expression, which may be either anonymous or
#   named. But this has caveats. "macro" can take zero or more arguments,
#   which may be macros or any valid operands, but it can only return
#   code. But you can do Turing-complete things via continuation passing
#   style: "macro foo (a, b) b(a) end foo(foo, foo)". Actually, don't do
#   that, since you'll just crash the assembler.
#
# - An "if" is a conditional on settings. Any identifier supplied in the
#   predicate of an "if" is assumed to be a #define that is available
#   during code gen. So you can't use "if" for computation in a macro, but
#   you can use it to select different pieces of code for different
#   platforms.
#
# - Arguments to macros follow lexical scoping rather than dynamic scoping.
#   Const's also follow lexical scoping and may override (hide) arguments
#   or other consts. All variables (arguments and constants) can be bound
#   to operands. Additionally, arguments (but not constants) can be bound
#   to macros.


# Below we have a bunch of constant declarations. Each constant must have
# a corresponding ASSERT() in LLIntData.cpp.

# Utilities
macro dispatch(advance)
    addp advance * 4, PC
    jmp [PC]
end

macro dispatchBranchWithOffset(pcOffset)
    lshifti 2, pcOffset
    addp pcOffset, PC
    jmp [PC]
end

macro dispatchBranch(pcOffset)
    loadi pcOffset, t0
    dispatchBranchWithOffset(t0)
end

macro dispatchAfterCall()
    loadi ArgumentCount + TagOffset[cfr], PC
    loadi 4[PC], t2
    storei t1, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    valueProfile(t1, t0, 4 * (CallOpCodeSize - 1), t3)
    dispatch(CallOpCodeSize)
end

macro cCall2(function, arg1, arg2)
    if ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
        move arg1, a0
        move arg2, a1
        call function
    elsif X86 or X86_WIN
        subp 8, sp
        push arg2
        push arg1
        call function
        addp 16, sp
    elsif SH4
        setargs arg1, arg2
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
    else
        cCall2(function, arg1, arg2)
    end
end

# This barely works. arg3 and arg4 should probably be immediates.
macro cCall4(function, arg1, arg2, arg3, arg4)
    if ARM or ARMv7 or ARMv7_TRADITIONAL or MIPS
        move arg1, a0
        move arg2, a1
        move arg3, a2
        move arg4, a3
        call function
    elsif X86 or X86_WIN
        push arg4
        push arg3
        push arg2
        push arg1
        call function
        addp 16, sp
    elsif SH4
        setargs arg1, arg2, arg3, arg4
        call function
    elsif C_LOOP
        error
    else
        error
    end
end

macro callSlowPath(slowPath)
    cCall2(slowPath, cfr, PC)
    move t0, PC
end

macro doCallToJavaScript(makeCall)
    if X86 or X86_WIN
        const entry = t4
        const vm = t3
        const protoCallFrame = t5

        const previousCFR = t0
        const previousPC = t1
        const temp1 = t0 # Same as previousCFR
        const temp2 = t1 # Same as previousPC
        const temp3 = t2
        const temp4 = t3 # same as vm
    elsif ARM or ARMv7 or ARMv7_TRADITIONAL or C_LOOP
        const entry = a0
        const vm = a1
        const protoCallFrame = a2

        const previousCFR = t3
        const previousPC = lr
        const temp1 = t3 # Same as previousCFR
        const temp2 = t4
        const temp3 = t5
        const temp4 = t4 # Same as temp2
    elsif MIPS
        const entry = a0
        const vmTopCallFrame = a1
        const protoCallFrame = a2
        const topOfStack = a3

        const previousCFR = t2
        const previousPC = lr
        const temp1 = t3
        const temp2 = t5
        const temp3 = t4
        const temp4 = t6
    elsif SH4
        const entry = a0
        const vm = a1
        const protoCallFrame = a2

        const previousCFR = t3
        const previousPC = lr
        const temp1 = t3 # Same as previousCFR
        const temp2 = a3
        const temp3 = t8
        const temp4 = t9
    end

    callToJavaScriptPrologue()

    if X86
        loadp 36[sp], vm
        loadp 32[sp], entry
    elsif X86_WIN
        loadp 40[sp, temp3], vm
        loadp 36[sp, temp3], entry
    else
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
    if X86
        loadp 28[sp], previousPC
        loadp 24[sp], previousCFR
    elsif X86_WIN
        loadp 32[sp, temp3], previousPC
        loadp 28[sp, temp3], previousCFR
    end
    storep previousPC, ReturnPC[cfr]
    storep previousCFR, CallerFrame[cfr]

    if X86
        loadp 40[sp], protoCallFrame
    elsif X86_WIN
        loadp 44[sp, temp3], protoCallFrame
    end

    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], temp2
    addp CallFrameHeaderSlots, temp2, temp2
    lshiftp 3, temp2
    subp cfr, temp2, temp1

    # Ensure that we have enough additional stack capacity for the incoming args,
    # and the frame for the JS code we're executing. We need to do this check
    # before we start copying the args from the protoCallFrame below.
    bpaeq temp1, VM::m_jsStackLimit[vm], .stackHeightOK

    if ARMv7
        subp cfr, 8, temp2
        move temp2, sp
    else
        subp cfr, 8, sp
    end

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
    loadi TagOffset[protoCallFrame, temp1, 8], temp3
    storei temp3, TagOffset + CodeBlock[sp, temp1, 8]
    loadi PayloadOffset[protoCallFrame, temp1, 8], temp3
    storei temp3, PayloadOffset + CodeBlock[sp, temp1, 8]
    btinz temp1, .copyHeaderLoop

    loadi PayloadOffset + ProtoCallFrame::argCountAndCodeOriginValue[protoCallFrame], temp2
    subi 1, temp2
    loadi ProtoCallFrame::paddedArgCount[protoCallFrame], temp3
    subi 1, temp3

    bieq temp2, temp3, .copyArgs
.fillExtraArgsLoop:
    subi 1, temp3
    storei UndefinedTag, ThisArgumentOffset + 8 + TagOffset[sp, temp3, 8]
    storei 0, ThisArgumentOffset + 8 + PayloadOffset[sp, temp3, 8]
    bineq temp2, temp3, .fillExtraArgsLoop

.copyArgs:
    loadp ProtoCallFrame::args[protoCallFrame], temp1

.copyArgsLoop:
    btiz temp2, .copyArgsDone
    subi 1, temp2
    loadi TagOffset[temp1, temp2, 8], temp3
    storei temp3, ThisArgumentOffset + 8 + TagOffset[sp, temp2, 8]
    loadi PayloadOffset[temp1, temp2, 8], temp3
    storei temp3, ThisArgumentOffset + 8 + PayloadOffset[sp, temp2, 8]
    jmp .copyArgsLoop

.copyArgsDone:
    storep sp, VM::topCallFrame[vm]

    makeCall(entry, temp1, temp2)

    bpeq CodeBlock[cfr], 1, .calleeFramePopped
    loadp CallerFrame[cfr], cfr

.calleeFramePopped:
    loadp Callee[cfr], temp3 # VM
    loadp ScopeChain[cfr], temp4 # previous topCallFrame
    storep temp4, VM::topCallFrame[temp3]

    callToJavaScriptEpilogue()
    ret
end

macro makeJavaScriptCall(entry, temp, unused)
    addp CallerFrameAndPCSize, sp
    checkStackPointerAlignment(t2, 0xbad0dc02)
    if C_LOOP
        cloopCallJSFunction entry
    else
        call entry
    end
    checkStackPointerAlignment(t2, 0xbad0dc03)
    subp CallerFrameAndPCSize, sp
end

macro makeHostFunctionCall(entry, temp1, temp2)
    move entry, temp1
    if C_LOOP
        move sp, a0
        storep cfr, [sp]
        storep lr, PtrSize[sp]
        cloopCallNative temp1
    else
        if X86 or X86_WIN
            # Put callee frame pointer on stack as arg0, also put it in ecx for "fastcall" targets
            move 0, temp2
            move temp2, 4[sp] # put 0 in ReturnPC
            move cfr, [sp] # put caller frame pointer into callee frame since callee prologue can't
            move sp, t2 # t2 is ecx
            push temp2 # Push dummy arg1
            push t2
        else
            move sp, a0
            addp CallerFrameAndPCSize, sp
        end
        call temp1
        if X86 or X86_WIN
            addp 8, sp
        else
            subp CallerFrameAndPCSize, sp
        end
    end
end

_handleUncaughtException:
    loadp ScopeChain + PayloadOffset[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    loadp VM::callFrameForThrow[t3], cfr

    # So far, we've unwound the stack to the frame just below the sentinel frame, except
    # in the case of stack overflow in the first function called from callToJavaScript.
    # Check if we need to pop to the sentinel frame and do the necessary clean up for
    # returning to the caller C frame.
    bpeq CodeBlock[cfr], 1, .handleUncaughtExceptionAlreadyIsSentinel
    loadp CallerFrame + PayloadOffset[cfr], cfr
.handleUncaughtExceptionAlreadyIsSentinel:

    loadp Callee + PayloadOffset[cfr], t3 # VM
    loadp ScopeChain + PayloadOffset[cfr], t5 # previous topCallFrame
    storep t5, VM::topCallFrame[t3]

    callToJavaScriptEpilogue()
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
    cCall4(_llint_trace_operand, cfr, PC, fromWhere, operand)
    move t0, PC
    move t1, cfr
end

# Debugging operation if you'd like to print the value of an operand in the instruction
# stream. Same as traceOperand(), but assumes that the operand is a register, and prints its
# value.
macro traceValue(fromWhere, operand)
    cCall4(_llint_trace_value, cfr, PC, fromWhere, operand)
    move t0, PC
    move t1, cfr
end

# Call a slowPath for call opcodes.
macro callCallSlowPath(slowPath, action)
    storep PC, ArgumentCount + TagOffset[cfr]
    cCall2(slowPath, cfr, PC)
    action(t0)
end

macro callWatchdogTimerHandler(throwHandler)
    storei PC, ArgumentCount + TagOffset[cfr]
    cCall2(_llint_slow_path_handle_watchdog_timer, cfr, PC)
    btpnz t0, throwHandler
    loadi ArgumentCount + TagOffset[cfr], PC
end

macro checkSwitchToJITForLoop()
    checkSwitchToJIT(
        1,
        macro ()
            storei PC, ArgumentCount + TagOffset[cfr]
            cCall2(_llint_loop_osr, cfr, PC)
            btpz t0, .recover
            move t1, sp
            jmp t0
        .recover:
            loadi ArgumentCount + TagOffset[cfr], PC
        end)
end

macro loadVariable(operand, index, tag, payload)
    loadisFromInstruction(operand, index)
    loadi TagOffset[cfr, index, 8], tag
    loadi PayloadOffset[cfr, index, 8], payload
end

# Index, tag, and payload must be different registers. Index is not
# changed.
macro loadConstantOrVariable(index, tag, payload)
    bigteq index, FirstConstantRegisterIndex, .constant
    loadi TagOffset[cfr, index, 8], tag
    loadi PayloadOffset[cfr, index, 8], payload
    jmp .done
.constant:
    loadp CodeBlock[cfr], payload
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[payload], payload
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    loadp TagOffset[payload, index, 8], tag
    loadp PayloadOffset[payload, index, 8], payload
.done:
end

macro loadConstantOrVariableTag(index, tag)
    bigteq index, FirstConstantRegisterIndex, .constant
    loadi TagOffset[cfr, index, 8], tag
    jmp .done
.constant:
    loadp CodeBlock[cfr], tag
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[tag], tag
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    loadp TagOffset[tag, index, 8], tag
.done:
end

# Index and payload may be the same register. Index may be clobbered.
macro loadConstantOrVariable2Reg(index, tag, payload)
    bigteq index, FirstConstantRegisterIndex, .constant
    loadi TagOffset[cfr, index, 8], tag
    loadi PayloadOffset[cfr, index, 8], payload
    jmp .done
.constant:
    loadp CodeBlock[cfr], tag
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[tag], tag
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    lshifti 3, index
    addp index, tag
    loadp PayloadOffset[tag], payload
    loadp TagOffset[tag], tag
.done:
end

macro loadConstantOrVariablePayloadTagCustom(index, tagCheck, payload)
    bigteq index, FirstConstantRegisterIndex, .constant
    tagCheck(TagOffset[cfr, index, 8])
    loadi PayloadOffset[cfr, index, 8], payload
    jmp .done
.constant:
    loadp CodeBlock[cfr], payload
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[payload], payload
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    tagCheck(TagOffset[payload, index, 8])
    loadp PayloadOffset[payload, index, 8], payload
.done:
end

# Index and payload must be different registers. Index is not mutated. Use
# this if you know what the tag of the variable should be. Doing the tag
# test as part of loading the variable reduces register use, but may not
# be faster than doing loadConstantOrVariable followed by a branch on the
# tag.
macro loadConstantOrVariablePayload(index, expectedTag, payload, slow)
    loadConstantOrVariablePayloadTagCustom(
        index,
        macro (actualTag) bineq actualTag, expectedTag, slow end,
        payload)
end

macro loadConstantOrVariablePayloadUnchecked(index, payload)
    loadConstantOrVariablePayloadTagCustom(
        index,
        macro (actualTag) end,
        payload)
end

macro storeStructureWithTypeInfo(cell, structure, scratch)
    storep structure, JSCell::m_structureID[cell]

    loadi Structure::m_blob + StructureIDBlob::u.words.word2[structure], scratch
    storei scratch, JSCell::m_indexingType[cell]
end

macro writeBarrierOnOperand(cellOperand)
    if GGC
        loadisFromInstruction(cellOperand, t1)
        loadConstantOrVariablePayload(t1, CellTag, t2, .writeBarrierDone)
        checkMarkByte(t2, t1, t3, 
            macro(gcData)
                btbnz gcData, .writeBarrierDone
                push cfr, PC
                # We make two extra slots because cCall2 will poke.
                subp 8, sp
                cCall2Void(_llint_write_barrier_slow, cfr, t2)
                addp 8, sp
                pop PC, cfr
            end
        )
    .writeBarrierDone:
    end
end

macro writeBarrierOnOperands(cellOperand, valueOperand)
    if GGC
        loadisFromInstruction(valueOperand, t1)
        loadConstantOrVariableTag(t1, t0)
        bineq t0, CellTag, .writeBarrierDone
    
        writeBarrierOnOperand(cellOperand)
    .writeBarrierDone:
    end
end

macro writeBarrierOnGlobalObject(valueOperand)
    if GGC
        loadisFromInstruction(valueOperand, t1)
        loadConstantOrVariableTag(t1, t0)
        bineq t0, CellTag, .writeBarrierDone
    
        loadp CodeBlock[cfr], t3
        loadp CodeBlock::m_globalObject[t3], t3
        checkMarkByte(t3, t1, t2,
            macro(gcData)
                btbnz gcData, .writeBarrierDone
                push cfr, PC
                # We make two extra slots because cCall2 will poke.
                subp 8, sp
                cCall2Void(_llint_write_barrier_slow, cfr, t3)
                addp 8, sp
                pop PC, cfr
            end
        )
    .writeBarrierDone:
    end
end

macro valueProfile(tag, payload, operand, scratch)
    loadp operand[PC], scratch
    storei tag, ValueProfile::m_buckets + TagOffset[scratch]
    storei payload, ValueProfile::m_buckets + PayloadOffset[scratch]
end


# Entrypoints into the interpreter

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
macro functionArityCheck(doneLabel, slowPath)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    biaeq t0, CodeBlock::m_numParameters[t1], doneLabel
    cCall2(slowPath, cfr, PC)   # This slowPath has a simple protocol: t0 = 0 => no error, t0 != 0 => error
    btiz t0, .noError
    move t1, cfr   # t1 contains caller frame
    jmp _llint_throw_from_slow_path_trampoline

.noError:
    # t1 points to ArityCheckData.
    loadp CommonSlowPaths::ArityCheckData::thunkToCall[t1], t2
    btpz t2, .proceedInline
    
    loadp CommonSlowPaths::ArityCheckData::returnPC[t1], t5
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
    negi t1
    move cfr, t3
    loadi PayloadOffset + ArgumentCount[cfr], t2
    addi CallFrameHeaderSlots, t2
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

    lshiftp 3, t1
    addp t1, cfr
    addp t1, sp
.continue:
    # Reload CodeBlock and PC, since the slow_path clobbered it.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_instructions[t1], PC
    jmp doneLabel
end

macro branchIfException(label)
    loadp ScopeChain[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    bieq VM::m_exception + TagOffset[t3], EmptyValueTag, .noException
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
    move UndefinedTag, t0
    move 0, t1
    negi t2
.opEnterLoop:
    storei t0, TagOffset[cfr, t2, 8]
    storei t1, PayloadOffset[cfr, t2, 8]
    addi 1, t2
    btinz t2, .opEnterLoop
.opEnterDone:
    callSlowPath(_slow_path_enter)
    dispatch(1)


_llint_op_create_activation:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opCreateActivationDone
    callSlowPath(_llint_slow_path_create_activation)
.opCreateActivationDone:
    dispatch(2)


_llint_op_init_lazy_reg:
    traceExecution()
    loadi 4[PC], t0
    storei EmptyValueTag, TagOffset[cfr, t0, 8]
    storei 0, PayloadOffset[cfr, t0, 8]
    dispatch(2)


_llint_op_create_arguments:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opCreateArgumentsDone
    callSlowPath(_slow_path_create_arguments)
.opCreateArgumentsDone:
    dispatch(2)


_llint_op_create_this:
    traceExecution()
    loadi 8[PC], t0
    loadp PayloadOffset[cfr, t0, 8], t0
    loadp JSFunction::m_allocationProfile + ObjectAllocationProfile::m_allocator[t0], t1
    loadp JSFunction::m_allocationProfile + ObjectAllocationProfile::m_structure[t0], t2
    btpz t1, .opCreateThisSlow
    allocateJSObject(t1, t2, t0, t3, .opCreateThisSlow)
    loadi 4[PC], t1
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t0, PayloadOffset[cfr, t1, 8]
    dispatch(4)

.opCreateThisSlow:
    callSlowPath(_slow_path_create_this)
    dispatch(4)


_llint_op_get_callee:
    traceExecution()
    loadi 4[PC], t0
    loadp PayloadOffset + Callee[cfr], t1
    loadpFromInstruction(2, t2)
    bpneq t1, t2, .opGetCalleeSlow
    storei CellTag, TagOffset[cfr, t0, 8]
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(3)

.opGetCalleeSlow:
    callSlowPath(_slow_path_get_callee)
    dispatch(3)

_llint_op_to_this:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], CellTag, .opToThisSlow
    loadi PayloadOffset[cfr, t0, 8], t0
    bbneq JSCell::m_type[t0], FinalObjectType, .opToThisSlow
    loadpFromInstruction(2, t2)
    bpneq JSCell::m_structureID[t0], t2, .opToThisSlow
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
    loadi 4[PC], t1
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t0, PayloadOffset[cfr, t1, 8]
    dispatch(4)

.opNewObjectSlow:
    callSlowPath(_llint_slow_path_new_object)
    dispatch(4)


_llint_op_mov:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t0
    loadConstantOrVariable(t1, t2, t3)
    storei t2, TagOffset[cfr, t0, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(3)


macro notifyWrite(set, valueTag, valuePayload, scratch, slow)
    loadb VariableWatchpointSet::m_state[set], scratch
    bieq scratch, IsInvalidated, .done
    bineq scratch, ClearWatchpoint, .overwrite
    storei valueTag, VariableWatchpointSet::m_inferredValue + TagOffset[set]
    storei valuePayload, VariableWatchpointSet::m_inferredValue + PayloadOffset[set]
    storeb IsWatched, VariableWatchpointSet::m_state[set]
    jmp .done

.overwrite:
    bineq valuePayload, VariableWatchpointSet::m_inferredValue + PayloadOffset[set], .definitelyDifferent
    bieq valueTag, VariableWatchpointSet::m_inferredValue + TagOffset[set], .done
.definitelyDifferent:
    btbnz VariableWatchpointSet::m_setIsNotEmpty[set], slow
    storei EmptyValueTag, VariableWatchpointSet::m_inferredValue + TagOffset[set]
    storei 0, VariableWatchpointSet::m_inferredValue + PayloadOffset[set]
    storeb IsInvalidated, VariableWatchpointSet::m_state[set]

.done:
end

_llint_op_captured_mov:
    traceExecution()
    loadi 8[PC], t1
    loadConstantOrVariable(t1, t2, t3)
    loadpFromInstruction(3, t0)
    btpz t0, .opCapturedMovReady
    notifyWrite(t0, t2, t3, t1, .opCapturedMovSlow)
.opCapturedMovReady:
    loadi 4[PC], t0
    storei t2, TagOffset[cfr, t0, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(4)

.opCapturedMovSlow:
    callSlowPath(_slow_path_captured_mov)
    dispatch(4)


_llint_op_not:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    loadConstantOrVariable(t0, t2, t3)
    bineq t2, BooleanTag, .opNotSlow
    xori 1, t3
    storei t2, TagOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t1, 8]
    dispatch(3)

.opNotSlow:
    callSlowPath(_slow_path_not)
    dispatch(3)


_llint_op_eq:
    traceExecution()
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, t3, .opEqSlow
    bieq t2, CellTag, .opEqSlow
    bib t2, LowestTag, .opEqSlow
    loadi 4[PC], t2
    cieq t0, t1, t0
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(4)

.opEqSlow:
    callSlowPath(_slow_path_eq)
    dispatch(4)


_llint_op_eq_null:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t3
    assertNotConstant(t0)
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
    storei BooleanTag, TagOffset[cfr, t3, 8]
    storei t1, PayloadOffset[cfr, t3, 8]
    dispatch(3)


_llint_op_neq:
    traceExecution()
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, t3, .opNeqSlow
    bieq t2, CellTag, .opNeqSlow
    bib t2, LowestTag, .opNeqSlow
    loadi 4[PC], t2
    cineq t0, t1, t0
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(4)

.opNeqSlow:
    callSlowPath(_slow_path_neq)
    dispatch(4)
    

_llint_op_neq_null:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t3
    assertNotConstant(t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
    bineq t1, CellTag, .opNeqNullImmediate
    btbnz JSCell::m_flags[t0], MasqueradesAsUndefined, .opNeqNullMasqueradesAsUndefined
    move 1, t1
    jmp .opNeqNullNotImmediate
.opNeqNullMasqueradesAsUndefined:
    loadp JSCell::m_structureID[t0], t1
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    cpneq Structure::m_globalObject[t1], t0, t1
    jmp .opNeqNullNotImmediate
.opNeqNullImmediate:
    cineq t1, NullTag, t2
    cineq t1, UndefinedTag, t1
    andi t2, t1
.opNeqNullNotImmediate:
    storei BooleanTag, TagOffset[cfr, t3, 8]
    storei t1, PayloadOffset[cfr, t3, 8]
    dispatch(3)


macro strictEq(equalityOperation, slowPath)
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, t3, .slow
    bib t2, LowestTag, .slow
    bineq t2, CellTag, .notString
    bbneq JSCell::m_type[t0], StringType, .notString
    bbeq JSCell::m_type[t1], StringType, .slow
.notString:
    loadi 4[PC], t2
    equalityOperation(t0, t1, t0)
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(4)

.slow:
    callSlowPath(slowPath)
    dispatch(4)
end

_llint_op_stricteq:
    traceExecution()
    strictEq(macro (left, right, result) cieq left, right, result end, _slow_path_stricteq)


_llint_op_nstricteq:
    traceExecution()
    strictEq(macro (left, right, result) cineq left, right, result end, _slow_path_nstricteq)


_llint_op_inc:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], Int32Tag, .opIncSlow
    loadi PayloadOffset[cfr, t0, 8], t1
    baddio 1, t1, .opIncSlow
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(2)

.opIncSlow:
    callSlowPath(_slow_path_inc)
    dispatch(2)


_llint_op_dec:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], Int32Tag, .opDecSlow
    loadi PayloadOffset[cfr, t0, 8], t1
    bsubio 1, t1, .opDecSlow
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(2)

.opDecSlow:
    callSlowPath(_slow_path_dec)
    dispatch(2)


_llint_op_to_number:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    loadConstantOrVariable(t0, t2, t3)
    bieq t2, Int32Tag, .opToNumberIsInt
    biaeq t2, LowestTag, .opToNumberSlow
.opToNumberIsInt:
    storei t2, TagOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t1, 8]
    dispatch(3)

.opToNumberSlow:
    callSlowPath(_slow_path_to_number)
    dispatch(3)


_llint_op_negate:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t3
    loadConstantOrVariable(t0, t1, t2)
    bineq t1, Int32Tag, .opNegateSrcNotInt
    btiz t2, 0x7fffffff, .opNegateSlow
    negi t2
    storei Int32Tag, TagOffset[cfr, t3, 8]
    storei t2, PayloadOffset[cfr, t3, 8]
    dispatch(3)
.opNegateSrcNotInt:
    bia t1, LowestTag, .opNegateSlow
    xori 0x80000000, t1
    storei t1, TagOffset[cfr, t3, 8]
    storei t2, PayloadOffset[cfr, t3, 8]
    dispatch(3)

.opNegateSlow:
    callSlowPath(_slow_path_negate)
    dispatch(3)


macro binaryOpCustomStore(integerOperationAndStore, doubleOperation, slowPath)
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, Int32Tag, .op1NotInt
    bineq t3, Int32Tag, .op2NotInt
    loadi 4[PC], t2
    integerOperationAndStore(t3, t1, t0, .slow, t2)
    dispatch(5)

.op1NotInt:
    # First operand is definitely not an int, the second operand could be anything.
    bia t2, LowestTag, .slow
    bib t3, LowestTag, .op1NotIntOp2Double
    bineq t3, Int32Tag, .slow
    ci2d t1, ft1
    jmp .op1NotIntReady
.op1NotIntOp2Double:
    fii2d t1, t3, ft1
.op1NotIntReady:
    loadi 4[PC], t1
    fii2d t0, t2, ft0
    doubleOperation(ft1, ft0)
    stored ft0, [cfr, t1, 8]
    dispatch(5)

.op2NotInt:
    # First operand is definitely an int, the second operand is definitely not.
    loadi 4[PC], t2
    bia t3, LowestTag, .slow
    ci2d t0, ft0
    fii2d t1, t3, ft1
    doubleOperation(ft1, ft0)
    stored ft0, [cfr, t2, 8]
    dispatch(5)

.slow:
    callSlowPath(slowPath)
    dispatch(5)
end

macro binaryOp(integerOperation, doubleOperation, slowPath)
    binaryOpCustomStore(
        macro (int32Tag, left, right, slow, index)
            integerOperation(left, right, slow)
            storei int32Tag, TagOffset[cfr, index, 8]
            storei right, PayloadOffset[cfr, index, 8]
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
    binaryOpCustomStore(
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
        macro (left, right) divd left, right end,
        _slow_path_div)


macro bitOp(operation, slowPath, advance)
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t3, Int32Tag, .slow
    bineq t2, Int32Tag, .slow
    loadi 4[PC], t2
    operation(t1, t0)
    storei t3, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
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
    loadi 4[PC], t0
    loadi 8[PC], t1
    loadConstantOrVariablePayload(t1, Int32Tag, t2, .opUnsignedSlow)
    bilt t2, 0, .opUnsignedSlow
    storei t2, PayloadOffset[cfr, t0, 8]
    storei Int32Tag, TagOffset[cfr, t0, 8]
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
    loadi 12[PC], t1
    loadConstantOrVariablePayload(t1, CellTag, t0, .opCheckHasInstanceSlow)
    btbz JSCell::m_flags[t0], ImplementsDefaultHasInstance, .opCheckHasInstanceSlow
    dispatch(5)

.opCheckHasInstanceSlow:
    callSlowPath(_llint_slow_path_check_has_instance)
    dispatch(0)


_llint_op_instanceof:
    traceExecution()
    # Actually do the work.
    loadi 12[PC], t0
    loadi 4[PC], t3
    loadConstantOrVariablePayload(t0, CellTag, t1, .opInstanceofSlow)
    bbb JSCell::m_type[t1], ObjectType, .opInstanceofSlow
    loadi 8[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t2, .opInstanceofSlow)
    
    # Register state: t1 = prototype, t2 = value
    move 1, t0
.opInstanceofLoop:
    loadp JSCell::m_structureID[t2], t2
    loadi Structure::m_prototype + PayloadOffset[t2], t2
    bpeq t2, t1, .opInstanceofDone
    btinz t2, .opInstanceofLoop

    move 0, t0
.opInstanceofDone:
    storei BooleanTag, TagOffset[cfr, t3, 8]
    storei t0, PayloadOffset[cfr, t3, 8]
    dispatch(4)

.opInstanceofSlow:
    callSlowPath(_llint_slow_path_instanceof)
    dispatch(4)


_llint_op_is_undefined:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t0
    loadConstantOrVariable(t1, t2, t3)
    storei BooleanTag, TagOffset[cfr, t0, 8]
    bieq t2, CellTag, .opIsUndefinedCell
    cieq t2, UndefinedTag, t3
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(3)
.opIsUndefinedCell:
    btbnz JSCell::m_flags[t3], MasqueradesAsUndefined, .opIsUndefinedMasqueradesAsUndefined
    move 0, t1
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(3)
.opIsUndefinedMasqueradesAsUndefined:
    loadp JSCell::m_structureID[t3], t1
    loadp CodeBlock[cfr], t3
    loadp CodeBlock::m_globalObject[t3], t3
    cpeq Structure::m_globalObject[t1], t3, t1
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(3)


_llint_op_is_boolean:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t2
    loadConstantOrVariableTag(t1, t0)
    cieq t0, BooleanTag, t0
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(3)


_llint_op_is_number:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t2
    loadConstantOrVariableTag(t1, t0)
    storei BooleanTag, TagOffset[cfr, t2, 8]
    addi 1, t0
    cib t0, LowestTag + 1, t1
    storei t1, PayloadOffset[cfr, t2, 8]
    dispatch(3)


_llint_op_is_string:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t2
    loadConstantOrVariable(t1, t0, t3)
    storei BooleanTag, TagOffset[cfr, t2, 8]
    bineq t0, CellTag, .opIsStringNotCell
    cbeq JSCell::m_type[t3], StringType, t1
    storei t1, PayloadOffset[cfr, t2, 8]
    dispatch(3)
.opIsStringNotCell:
    storep 0, PayloadOffset[cfr, t2, 8]
    dispatch(3)


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


_llint_op_init_global_const:
    traceExecution()
    writeBarrierOnGlobalObject(2)
    loadi 8[PC], t1
    loadi 4[PC], t0
    loadConstantOrVariable(t1, t2, t3)
    storei t2, TagOffset[t0]
    storei t3, PayloadOffset[t0]
    dispatch(5)


# We only do monomorphic get_by_id caching for now, and we do not modify the
# opcode. We do, however, allow for the cache to change anytime if fails, since
# ping-ponging is free. At best we get lucky and the get_by_id will continue
# to take fast path on the new cache. At worst we take slow path, which is what
# we would have been doing anyway.

macro getById(getPropertyStorage)
    traceExecution()
    loadi 8[PC], t0
    loadi 16[PC], t1
    loadConstantOrVariablePayload(t0, CellTag, t3, .opGetByIdSlow)
    loadi 20[PC], t2
    getPropertyStorage(
        t3,
        t0,
        macro (propertyStorage, scratch)
            bpneq JSCell::m_structureID[t3], t1, .opGetByIdSlow
            loadi 4[PC], t1
            loadi TagOffset[propertyStorage, t2], scratch
            loadi PayloadOffset[propertyStorage, t2], t2
            storei scratch, TagOffset[cfr, t1, 8]
            storei t2, PayloadOffset[cfr, t1, 8]
            valueProfile(scratch, t2, 32, t1)
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
    loadi 8[PC], t0
    loadp 16[PC], t1
    loadConstantOrVariablePayload(t0, CellTag, t3, .opGetArrayLengthSlow)
    loadp JSCell::m_structureID[t3], t2
    arrayProfile(t2, t1, t0)
    btiz t2, IsArray, .opGetArrayLengthSlow
    btiz t2, IndexingShapeMask, .opGetArrayLengthSlow
    loadi 4[PC], t1
    loadp JSObject::m_butterfly[t3], t0
    loadi -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], t0
    bilt t0, 0, .opGetArrayLengthSlow
    valueProfile(Int32Tag, t0, 32, t2)
    storep t0, PayloadOffset[cfr, t1, 8]
    storep Int32Tag, TagOffset[cfr, t1, 8]
    dispatch(9)

.opGetArrayLengthSlow:
    callSlowPath(_llint_slow_path_get_by_id)
    dispatch(9)


_llint_op_get_arguments_length:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opGetArgumentsLengthSlow
    loadi ArgumentCount + PayloadOffset[cfr], t2
    subi 1, t2
    storei Int32Tag, TagOffset[cfr, t1, 8]
    storei t2, PayloadOffset[cfr, t1, 8]
    dispatch(4)

.opGetArgumentsLengthSlow:
    callSlowPath(_llint_slow_path_get_arguments_length)
    dispatch(4)


macro putById(getPropertyStorage)
    traceExecution()
    writeBarrierOnOperands(1, 3)
    loadi 4[PC], t3
    loadi 16[PC], t1
    loadConstantOrVariablePayload(t3, CellTag, t0, .opPutByIdSlow)
    loadi 12[PC], t2
    getPropertyStorage(
        t0,
        t3,
        macro (propertyStorage, scratch)
            bpneq JSCell::m_structureID[t0], t1, .opPutByIdSlow
            loadi 20[PC], t1
            loadConstantOrVariable2Reg(t2, scratch, t2)
            storei scratch, TagOffset[propertyStorage, t1]
            storei t2, PayloadOffset[propertyStorage, t1]
            dispatch(9)
        end)

    .opPutByIdSlow:
        callSlowPath(_llint_slow_path_put_by_id)
        dispatch(9)
end

_llint_op_put_by_id:
    putById(withInlineStorage)


_llint_op_put_by_id_out_of_line:
    putById(withOutOfLineStorage)


macro putByIdTransition(additionalChecks, getPropertyStorage)
    traceExecution()
    writeBarrierOnOperand(1)
    loadi 4[PC], t3
    loadi 16[PC], t1
    loadConstantOrVariablePayload(t3, CellTag, t0, .opPutByIdSlow)
    loadi 12[PC], t2
    bpneq JSCell::m_structureID[t0], t1, .opPutByIdSlow
    additionalChecks(t1, t3, .opPutByIdSlow)
    loadi 20[PC], t1
    getPropertyStorage(
        t0,
        t3,
        macro (propertyStorage, scratch)
            addp t1, propertyStorage, t3
            loadConstantOrVariable2Reg(t2, t1, t2)
            storei t1, TagOffset[t3]
            loadi 24[PC], t1
            storei t2, PayloadOffset[t3]
            storep t1, JSCell::m_structureID[t0]
            dispatch(9)
        end)

    .opPutByIdSlow:
        callSlowPath(_llint_slow_path_put_by_id)
        dispatch(9)
end

macro noAdditionalChecks(oldStructure, scratch, slowPath)
end

macro structureChainChecks(oldStructure, scratch, slowPath)
    const protoCell = oldStructure   # Reusing the oldStructure register for the proto

    loadp 28[PC], scratch
    assert(macro (ok) btpnz scratch, ok end)
    loadp StructureChain::m_vector[scratch], scratch
    assert(macro (ok) btpnz scratch, ok end)
    bieq Structure::m_prototype + TagOffset[oldStructure], NullTag, .done
.loop:
    loadi Structure::m_prototype + PayloadOffset[oldStructure], protoCell
    loadp JSCell::m_structureID[protoCell], oldStructure
    bpneq oldStructure, [scratch], slowPath
    addp 4, scratch
    bineq Structure::m_prototype + TagOffset[oldStructure], NullTag, .loop
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
    loadi 8[PC], t2
    loadConstantOrVariablePayload(t2, CellTag, t0, .opGetByValSlow)
    loadp JSCell::m_structureID[t0], t2
    loadp 16[PC], t3
    arrayProfile(t2, t3, t1)
    loadi 12[PC], t3
    loadConstantOrVariablePayload(t3, Int32Tag, t1, .opGetByValSlow)
    loadp JSObject::m_butterfly[t0], t3
    andi IndexingShapeMask, t2
    bieq t2, Int32Shape, .opGetByValIsContiguous
    bineq t2, ContiguousShape, .opGetByValNotContiguous
.opGetByValIsContiguous:
    
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValOutOfBounds
    loadi TagOffset[t3, t1, 8], t2
    loadi PayloadOffset[t3, t1, 8], t1
    jmp .opGetByValDone

.opGetByValNotContiguous:
    bineq t2, DoubleShape, .opGetByValNotDouble
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValOutOfBounds
    loadd [t3, t1, 8], ft0
    bdnequn ft0, ft0, .opGetByValSlow
    # FIXME: This could be massively optimized.
    fd2ii ft0, t1, t2
    loadi 4[PC], t0
    jmp .opGetByValNotEmpty

.opGetByValNotDouble:
    subi ArrayStorageShape, t2
    bia t2, SlowPutArrayStorageShape - ArrayStorageShape, .opGetByValSlow
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t3], .opGetByValOutOfBounds
    loadi ArrayStorage::m_vector + TagOffset[t3, t1, 8], t2
    loadi ArrayStorage::m_vector + PayloadOffset[t3, t1, 8], t1

.opGetByValDone:
    loadi 4[PC], t0
    bieq t2, EmptyValueTag, .opGetByValOutOfBounds
.opGetByValNotEmpty:
    storei t2, TagOffset[cfr, t0, 8]
    storei t1, PayloadOffset[cfr, t0, 8]
    valueProfile(t2, t1, 20, t0)
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
    loadi 8[PC], t0
    loadi 12[PC], t1
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opGetArgumentByValSlow
    loadConstantOrVariablePayload(t1, Int32Tag, t2, .opGetArgumentByValSlow)
    addi 1, t2
    loadi ArgumentCount + PayloadOffset[cfr], t1
    biaeq t2, t1, .opGetArgumentByValSlow
    loadi 4[PC], t3
    loadi ThisArgumentOffset + TagOffset[cfr, t2, 8], t0
    loadi ThisArgumentOffset + PayloadOffset[cfr, t2, 8], t1
    storei t0, TagOffset[cfr, t3, 8]
    storei t1, PayloadOffset[cfr, t3, 8]
    valueProfile(t0, t1, 20, t2)
    dispatch(6)

.opGetArgumentByValSlow:
    callSlowPath(_llint_slow_path_get_argument_by_val)
    dispatch(6)


_llint_op_get_by_pname:
    traceExecution()
    loadi 12[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t1, .opGetByPnameSlow)
    loadi 16[PC], t0
    bpneq t1, PayloadOffset[cfr, t0, 8], .opGetByPnameSlow
    loadi 8[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t2, .opGetByPnameSlow)
    loadi 20[PC], t0
    loadi PayloadOffset[cfr, t0, 8], t3
    loadp JSCell::m_structureID[t2], t0
    bpneq t0, JSPropertyNameIterator::m_cachedStructure[t3], .opGetByPnameSlow
    loadi 24[PC], t0
    loadi [cfr, t0, 8], t0
    subi 1, t0
    biaeq t0, JSPropertyNameIterator::m_numCacheableSlots[t3], .opGetByPnameSlow
    bilt t0, JSPropertyNameIterator::m_cachedStructureInlineCapacity[t3], .opGetByPnameInlineProperty
    addi firstOutOfLineOffset, t0
    subi JSPropertyNameIterator::m_cachedStructureInlineCapacity[t3], t0
.opGetByPnameInlineProperty:
    loadPropertyAtVariableOffset(t0, t2, t1, t3)
    loadi 4[PC], t0
    storei t1, TagOffset[cfr, t0, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(7)

.opGetByPnameSlow:
    callSlowPath(_llint_slow_path_get_by_pname)
    dispatch(7)


macro contiguousPutByVal(storeCallback)
    biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], .outOfBounds
.storeResult:
    loadi 12[PC], t2
    storeCallback(t2, t1, t0, t3)
    dispatch(5)

.outOfBounds:
    biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
    loadp 16[PC], t2
    storeb 1, ArrayProfile::m_mayStoreToHole[t2]
    addi 1, t3, t2
    storei t2, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
    jmp .storeResult
end

macro putByVal(holeCheck, slowPath)
    traceExecution()
    writeBarrierOnOperands(1, 3)
    loadi 4[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t1, .opPutByValSlow)
    loadp JSCell::m_structureID[t1], t2
    loadp 16[PC], t3
    arrayProfile(t2, t3, t0)
    loadi 8[PC], t0
    loadConstantOrVariablePayload(t0, Int32Tag, t3, .opPutByValSlow)
    loadp JSObject::m_butterfly[t1], t0
    andi IndexingShapeMask, t2
    bineq t2, Int32Shape, .opPutByValNotInt32
    contiguousPutByVal(
        macro (operand, scratch, base, index)
            loadConstantOrVariablePayload(operand, Int32Tag, scratch, .opPutByValSlow)
            storei Int32Tag, TagOffset[base, index, 8]
            storei scratch, PayloadOffset[base, index, 8]
        end)

.opPutByValNotInt32:
    bineq t2, DoubleShape, .opPutByValNotDouble
    contiguousPutByVal(
        macro (operand, scratch, base, index)
            const tag = scratch
            const payload = operand
            loadConstantOrVariable2Reg(operand, tag, payload)
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
            loadConstantOrVariable2Reg(operand, tag, payload)
            storei tag, TagOffset[base, index, 8]
            storei payload, PayloadOffset[base, index, 8]
        end)

.opPutByValNotContiguous:
    bineq t2, ArrayStorageShape, .opPutByValSlow
    biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
    holeCheck(ArrayStorage::m_vector + TagOffset[t0, t3, 8], .opPutByValArrayStorageEmpty)
.opPutByValArrayStorageStoreResult:
    loadi 12[PC], t2
    loadConstantOrVariable2Reg(t2, t1, t2)
    storei t1, ArrayStorage::m_vector + TagOffset[t0, t3, 8]
    storei t2, ArrayStorage::m_vector + PayloadOffset[t0, t3, 8]
    dispatch(5)

.opPutByValArrayStorageEmpty:
    loadp 16[PC], t1
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
    putByVal(macro(addr, slowPath)
        bieq addr, EmptyValueTag, slowPath
    end, _llint_slow_path_put_by_val)

_llint_op_put_by_val_direct:
    putByVal(macro(addr, slowPath)
    end, _llint_slow_path_put_by_val_direct)

_llint_op_jmp:
    traceExecution()
    dispatchBranch(4[PC])


macro jumpTrueOrFalse(conditionOp, slow)
    loadi 4[PC], t1
    loadConstantOrVariablePayload(t1, BooleanTag, t0, .slow)
    conditionOp(t0, .target)
    dispatch(3)

.target:
    dispatchBranch(8[PC])

.slow:
    callSlowPath(slow)
    dispatch(0)
end


macro equalNull(cellHandler, immediateHandler)
    loadi 4[PC], t0
    assertNotConstant(t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
    bineq t1, CellTag, .immediate
    loadp JSCell::m_structureID[t0], t2
    cellHandler(t2, JSCell::m_flags[t0], .target)
    dispatch(3)

.target:
    dispatchBranch(8[PC])

.immediate:
    ori 1, t1
    immediateHandler(t1, .target)
    dispatch(3)
end

_llint_op_jeq_null:
    traceExecution()
    equalNull(
        macro (structure, value, target) 
            btbz value, MasqueradesAsUndefined, .opJeqNullNotMasqueradesAsUndefined
            loadp CodeBlock[cfr], t0
            loadp CodeBlock::m_globalObject[t0], t0
            bpeq Structure::m_globalObject[structure], t0, target
.opJeqNullNotMasqueradesAsUndefined:
        end,
        macro (value, target) bieq value, NullTag, target end)
    

_llint_op_jneq_null:
    traceExecution()
    equalNull(
        macro (structure, value, target) 
            btbz value, MasqueradesAsUndefined, target 
            loadp CodeBlock[cfr], t0
            loadp CodeBlock::m_globalObject[t0], t0
            bpneq Structure::m_globalObject[structure], t0, target
        end,
        macro (value, target) bineq value, NullTag, target end)


_llint_op_jneq_ptr:
    traceExecution()
    loadi 4[PC], t0
    loadi 8[PC], t1
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_globalObject[t2], t2
    bineq TagOffset[cfr, t0, 8], CellTag, .opJneqPtrBranch
    loadp JSGlobalObject::m_specialPointers[t2, t1, 4], t1
    bpeq PayloadOffset[cfr, t0, 8], t1, .opJneqPtrFallThrough
.opJneqPtrBranch:
    dispatchBranch(12[PC])
.opJneqPtrFallThrough:
    dispatch(4)


macro compare(integerCompare, doubleCompare, slowPath)
    loadi 4[PC], t2
    loadi 8[PC], t3
    loadConstantOrVariable(t2, t0, t1)
    loadConstantOrVariable2Reg(t3, t2, t3)
    bineq t0, Int32Tag, .op1NotInt
    bineq t2, Int32Tag, .op2NotInt
    integerCompare(t1, t3, .jumpTarget)
    dispatch(4)

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
    dispatch(4)

.op2NotInt:
    ci2d t1, ft0
    bia t2, LowestTag, .slow
    fii2d t3, t2, ft1
    doubleCompare(ft0, ft1, .jumpTarget)
    dispatch(4)

.jumpTarget:
    dispatchBranch(12[PC])

.slow:
    callSlowPath(slowPath)
    dispatch(0)
end


_llint_op_switch_imm:
    traceExecution()
    loadi 12[PC], t2
    loadi 4[PC], t3
    loadConstantOrVariable(t2, t1, t0)
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
    dispatchBranchWithOffset(t1)

.opSwitchImmNotInt:
    bib t1, LowestTag, .opSwitchImmSlow  # Go to slow path if it's a double.
.opSwitchImmFallThrough:
    dispatchBranch(8[PC])

.opSwitchImmSlow:
    callSlowPath(_llint_slow_path_switch_imm)
    dispatch(0)


_llint_op_switch_char:
    traceExecution()
    loadi 12[PC], t2
    loadi 4[PC], t3
    loadConstantOrVariable(t2, t1, t0)
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
    dispatchBranchWithOffset(t1)

.opSwitchCharFallThrough:
    dispatchBranch(8[PC])

.opSwitchOnRope:
    callSlowPath(_llint_slow_path_switch_char)
    dispatch(0)


_llint_op_new_func:
    traceExecution()
    btiz 12[PC], .opNewFuncUnchecked
    loadi 4[PC], t1
    bineq TagOffset[cfr, t1, 8], EmptyValueTag, .opNewFuncDone
.opNewFuncUnchecked:
    callSlowPath(_llint_slow_path_new_func)
.opNewFuncDone:
    dispatch(4)


_llint_op_new_captured_func:
    traceExecution()
    callSlowPath(_slow_path_new_captured_func)
    dispatch(4)


macro arrayProfileForCall()
    loadi 16[PC], t3
    negi t3
    bineq ThisArgumentOffset + TagOffset[cfr, t3, 8], CellTag, .done
    loadi ThisArgumentOffset + PayloadOffset[cfr, t3, 8], t0
    loadp JSCell::m_structureID[t0], t0
    loadpFromInstruction(CallOpCodeSize - 2, t1)
    storep t0, ArrayProfile::m_lastSeenStructureID[t1]
.done:
end

macro doCall(slowPath)
    loadi 8[PC], t0
    loadi 20[PC], t1
    loadp LLIntCallLinkInfo::callee[t1], t2
    loadConstantOrVariablePayload(t0, CellTag, t3, .opCallSlow)
    bineq t3, t2, .opCallSlow
    loadi 16[PC], t3
    lshifti 3, t3
    negi t3
    addp cfr, t3  # t3 contains the new value of cfr
    loadp JSFunction::m_scope[t2], t0
    storei t2, Callee + PayloadOffset[t3]
    storei t0, ScopeChain + PayloadOffset[t3]
    loadi 12[PC], t2
    storei PC, ArgumentCount + TagOffset[cfr]
    storei t2, ArgumentCount + PayloadOffset[t3]
    storei CellTag, Callee + TagOffset[t3]
    storei CellTag, ScopeChain + TagOffset[t3]
    addp CallerFrameAndPCSize, t3
    callTargetFunction(t1, t3)

.opCallSlow:
    slowPathForCall(slowPath)
end


_llint_op_tear_off_activation:
    traceExecution()
    loadi 4[PC], t0
    bieq TagOffset[cfr, t0, 8], EmptyValueTag, .opTearOffActivationNotCreated
    callSlowPath(_llint_slow_path_tear_off_activation)
.opTearOffActivationNotCreated:
    dispatch(2)


_llint_op_tear_off_arguments:
    traceExecution()
    loadi 4[PC], t0
    addi 1, t0   # Get the unmodifiedArgumentsRegister
    bieq TagOffset[cfr, t0, 8], EmptyValueTag, .opTearOffArgumentsNotCreated
    callSlowPath(_llint_slow_path_tear_off_arguments)
.opTearOffArgumentsNotCreated:
    dispatch(3)


_llint_op_ret:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadi 4[PC], t2
    loadConstantOrVariable(t2, t1, t0)
    doReturn()


_llint_op_ret_object_or_this:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadi 4[PC], t2
    loadConstantOrVariable(t2, t1, t0)
    bineq t1, CellTag, .opRetObjectOrThisNotObject
    bbb JSCell::m_type[t0], ObjectType, .opRetObjectOrThisNotObject
    doReturn()

.opRetObjectOrThisNotObject:
    loadi 8[PC], t2
    loadConstantOrVariable(t2, t1, t0)
    doReturn()


_llint_op_to_primitive:
    traceExecution()
    loadi 8[PC], t2
    loadi 4[PC], t3
    loadConstantOrVariable(t2, t1, t0)
    bineq t1, CellTag, .opToPrimitiveIsImm
    bbneq JSCell::m_type[t0], StringType, .opToPrimitiveSlowCase
.opToPrimitiveIsImm:
    storei t1, TagOffset[cfr, t3, 8]
    storei t0, PayloadOffset[cfr, t3, 8]
    dispatch(3)

.opToPrimitiveSlowCase:
    callSlowPath(_slow_path_to_primitive)
    dispatch(3)


_llint_op_next_pname:
    traceExecution()
    loadi 12[PC], t1
    loadi 16[PC], t2
    loadi PayloadOffset[cfr, t1, 8], t0
    bieq t0, PayloadOffset[cfr, t2, 8], .opNextPnameEnd
    loadi 20[PC], t2
    loadi PayloadOffset[cfr, t2, 8], t2
    loadp JSPropertyNameIterator::m_jsStrings[t2], t3
    loadi [t3, t0, 8], t3
    addi 1, t0
    storei t0, PayloadOffset[cfr, t1, 8]
    loadi 4[PC], t1
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t1, 8]
    loadi 8[PC], t3
    loadi PayloadOffset[cfr, t3, 8], t3
    loadp JSCell::m_structureID[t3], t1
    bpneq t1, JSPropertyNameIterator::m_cachedStructure[t2], .opNextPnameSlow
    loadp JSPropertyNameIterator::m_cachedPrototypeChain[t2], t0
    loadp StructureChain::m_vector[t0], t0
    btpz [t0], .opNextPnameTarget
.opNextPnameCheckPrototypeLoop:
    bieq Structure::m_prototype + TagOffset[t1], NullTag, .opNextPnameSlow
    loadp Structure::m_prototype + PayloadOffset[t1], t2
    loadp JSCell::m_structureID[t2], t1
    bpneq t1, [t0], .opNextPnameSlow
    addp 4, t0
    btpnz [t0], .opNextPnameCheckPrototypeLoop
.opNextPnameTarget:
    dispatchBranch(24[PC])

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
    loadp ScopeChain + PayloadOffset[cfr], t3
    andp MarkedBlockMask, t3
    loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
    loadp VM::callFrameForThrow[t3], cfr
    restoreStackPointerAfterCall()

    loadi VM::targetInterpreterPCForThrow[t3], PC
    loadi VM::m_exception + PayloadOffset[t3], t0
    loadi VM::m_exception + TagOffset[t3], t1
    storei 0, VM::m_exception + PayloadOffset[t3]
    storei EmptyValueTag, VM::m_exception + TagOffset[t3]
    loadi 4[PC], t2
    storei t0, PayloadOffset[cfr, t2, 8]
    storei t1, TagOffset[cfr, t2, 8]
    traceExecution()  # This needs to be here because we don't want to clobber t0, t1, t2, t3 above.
    dispatch(2)


# Gives you the scope in t0, while allowing you to optionally perform additional checks on the
# scopes as they are traversed. scopeCheck() is called with two arguments: the register
# holding the scope, and a register that can be used for scratch. Note that this does not
# use t3, so you can hold stuff in t3 if need be.
macro getDeBruijnScope(deBruijinIndexOperand, scopeCheck)
    loadp ScopeChain + PayloadOffset[cfr], t0
    loadi deBruijinIndexOperand, t2

    btiz t2, .done

    loadp CodeBlock[cfr], t1
    bineq CodeBlock::m_codeType[t1], FunctionCode, .loop
    btbz CodeBlock::m_needsActivation[t1], .loop

    loadi CodeBlock::m_activationRegister[t1], t1

    # Need to conditionally skip over one scope.
    bieq TagOffset[cfr, t1, 8], EmptyValueTag, .noActivation
    scopeCheck(t0, t1)
    loadp JSScope::m_next[t0], t0
.noActivation:
    subi 1, t2

    btiz t2, .done
.loop:
    scopeCheck(t0, t1)
    loadp JSScope::m_next[t0], t0
    subi 1, t2
    btinz t2, .loop

.done:

end

_llint_op_end:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadi 4[PC], t0
    assertNotConstant(t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
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
    loadp CallerFrame[cfr], t0
    loadi ScopeChain + PayloadOffset[t0], t1
    storei CellTag, ScopeChain + TagOffset[cfr]
    storei t1, ScopeChain + PayloadOffset[cfr]
    if X86 or X86_WIN
        subp 8, sp # align stack pointer
        andp MarkedBlockMask, t1
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t1], t3
        storep cfr, VM::topCallFrame[t3]
        move cfr, t2  # t2 = ecx
        storep t2, [sp]
        loadi Callee + PayloadOffset[cfr], t1
        loadp JSFunction::m_executable[t1], t1
        checkStackPointerAlignment(t3, 0xdead0001)
        call executableOffsetToFunction[t1]
        loadp ScopeChain[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
        addp 8, sp
    elsif ARM or ARMv7 or ARMv7_TRADITIONAL or C_LOOP or MIPS or SH4
        subp 8, sp # align stack pointer
        # t1 already contains the ScopeChain.
        andp MarkedBlockMask, t1
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t1], t1
        storep cfr, VM::topCallFrame[t1]
        if MIPS or SH4
            move cfr, a0
        else
            move cfr, t0
        end
        loadi Callee + PayloadOffset[cfr], t1
        loadp JSFunction::m_executable[t1], t1
        checkStackPointerAlignment(t3, 0xdead0001)
        if C_LOOP
            cloopCallNative executableOffsetToFunction[t1]
        else
            call executableOffsetToFunction[t1]
        end
        loadp ScopeChain[cfr], t3
        andp MarkedBlockMask, t3
        loadp MarkedBlock::m_weakSet + WeakSet::m_vm[t3], t3
        addp 8, sp
    else
        error
    end
    
    functionEpilogue()
    bineq VM::m_exception + TagOffset[t3], EmptyValueTag, .handleException
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
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t0, PayloadOffset[cfr, t1, 8]
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
    btpz PayloadOffset[cfr, t1, 8], .resolveScopeAfterActivationCheck
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
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t0, PayloadOffset[cfr, t1, 8]
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
    loadp [cfr, t0, 8], t0
    loadpFromInstruction(5, t1)
    bpneq JSCell::m_structureID[t0], t1, slowPath
end

macro getProperty()
    loadisFromInstruction(6, t3)
    loadPropertyAtVariableOffset(t3, t0, t1, t2)
    valueProfile(t1, t2, 28, t0)
    loadisFromInstruction(1, t0)
    storei t1, TagOffset[cfr, t0, 8]
    storei t2, PayloadOffset[cfr, t0, 8]
end

macro getGlobalVar()
    loadpFromInstruction(6, t0)
    loadp TagOffset[t0], t1
    loadp PayloadOffset[t0], t2
    valueProfile(t1, t2, 28, t0)
    loadisFromInstruction(1, t0)
    storei t1, TagOffset[cfr, t0, 8]
    storei t2, PayloadOffset[cfr, t0, 8]
end

macro getClosureVar()
    loadp JSVariableObject::m_registers[t0], t0
    loadisFromInstruction(6, t3)
    loadp TagOffset[t0, t3, 8], t1
    loadp PayloadOffset[t0, t3, 8], t2
    valueProfile(t1, t2, 28, t0)
    loadisFromInstruction(1, t0)
    storei t1, TagOffset[cfr, t0, 8]
    storei t2, PayloadOffset[cfr, t0, 8]
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
    loadVariable(2, t2, t1, t0)
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
    loadVariable(2, t2, t1, t0)
    getGlobalVar()
    dispatch(8)

.gClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .gDynamic
    varInjectionCheck(.gDynamic)
    loadVariable(2, t2, t1, t0)
    getClosureVar()
    dispatch(8)

.gDynamic:
    callSlowPath(_llint_slow_path_get_from_scope)
    dispatch(8)


macro putProperty()
    loadisFromInstruction(3, t1)
    loadConstantOrVariable(t1, t2, t3)
    loadisFromInstruction(6, t1)
    storePropertyAtVariableOffset(t1, t0, t2, t3)
end

macro putGlobalVar()
    loadisFromInstruction(3, t0)
    loadConstantOrVariable(t0, t1, t2)
    loadpFromInstruction(5, t3)
    notifyWrite(t3, t1, t2, t0, .pDynamic)
    loadpFromInstruction(6, t0)
    storei t1, TagOffset[t0]
    storei t2, PayloadOffset[t0]
end

macro putClosureVar()
    loadisFromInstruction(3, t1)
    loadConstantOrVariable(t1, t2, t3)
    loadp JSVariableObject::m_registers[t0], t0
    loadisFromInstruction(6, t1)
    storei t2, TagOffset[t0, t1, 8]
    storei t3, PayloadOffset[t0, t1, 8]
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
    loadVariable(1, t2, t1, t0)
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
    loadVariable(1, t2, t1, t0)
    putClosureVar()
    dispatch(7)

.pDynamic:
    callSlowPath(_llint_slow_path_put_to_scope)
    dispatch(7)
