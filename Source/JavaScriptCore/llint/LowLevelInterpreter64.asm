# Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
macro storePC()
    storei PC, LLIntReturnPC[cfr]
end

macro loadPC()
    loadi LLIntReturnPC[cfr], PC
end

macro getuOperandNarrow(opcodeStruct, fieldName, dst)
    loadb constexpr %opcodeStruct%_%fieldName%_index + OpcodeIDNarrowSize[PB, PC, 1], dst
end

macro getOperandNarrow(opcodeStruct, fieldName, dst)
    loadbsq constexpr %opcodeStruct%_%fieldName%_index + OpcodeIDNarrowSize[PB, PC, 1], dst
end

macro getuOperandWide16JS(opcodeStruct, fieldName, dst)
    loadh constexpr %opcodeStruct%_%fieldName%_index * 2 + OpcodeIDWide16SizeJS[PB, PC, 1], dst
end

macro getuOperandWide16Wasm(opcodeStruct, fieldName, dst)
    loadh constexpr %opcodeStruct%_%fieldName%_index * 2 + OpcodeIDWide16SizeWasm[PB, PC, 1], dst
end

macro getOperandWide16JS(opcodeStruct, fieldName, dst)
    loadhsq constexpr %opcodeStruct%_%fieldName%_index * 2 + OpcodeIDWide16SizeJS[PB, PC, 1], dst
end

macro getOperandWide16Wasm(opcodeStruct, fieldName, dst)
    loadhsq constexpr %opcodeStruct%_%fieldName%_index * 2 + OpcodeIDWide16SizeWasm[PB, PC, 1], dst
end

macro getuOperandWide32JS(opcodeStruct, fieldName, dst)
    loadi constexpr %opcodeStruct%_%fieldName%_index * 4 + OpcodeIDWide32SizeJS[PB, PC, 1], dst
end

macro getuOperandWide32Wasm(opcodeStruct, fieldName, dst)
    loadi constexpr %opcodeStruct%_%fieldName%_index * 4 + OpcodeIDWide32SizeWasm[PB, PC, 1], dst
end

macro getOperandWide32JS(opcodeStruct, fieldName, dst)
    loadis constexpr %opcodeStruct%_%fieldName%_index * 4 + OpcodeIDWide32SizeJS[PB, PC, 1], dst
end

macro getOperandWide32Wasm(opcodeStruct, fieldName, dst)
    loadis constexpr %opcodeStruct%_%fieldName%_index * 4 + OpcodeIDWide32SizeWasm[PB, PC, 1], dst
end

macro makeReturn(get, dispatch, fn)
    fn(macro (value)
        move value, t2
        get(m_dst, t1)
        storeq t2, [cfr, t1, 8]
        dispatch()
    end)
end

macro makeReturnProfiled(opcodeStruct, get, metadata, dispatch, fn)
    fn(macro (value)
        move value, t3
        metadata(t1, t2)
        valueProfile(opcodeStruct, m_profile, t1, t3)
        get(m_dst, t1)
        storeq t3, [cfr, t1, 8]
        dispatch()
    end)
end

macro valueProfile(opcodeStruct, profileName, metadata, value)
    storeq value, %opcodeStruct%::Metadata::%profileName%.m_buckets[metadata]
end

# After calling, calling bytecode is claiming input registers are not used.
macro dispatchAfterCall(size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch)
    loadPC()
    if C_LOOP or C_LOOP_WIN
        # On non C_LOOP builds, CSR restore takes care of this.
        loadp CodeBlock[cfr], PB
        loadp CodeBlock::m_instructionsRawPointer[PB], PB
    end
    get(size, opcodeStruct, dstVirtualRegister, t1)
    storeq r0, [cfr, t1, 8]
    metadata(size, opcodeStruct, t2, t1)
    valueProfile(opcodeStruct, valueProfileName, t2, r0)
    dispatch()
end

macro cCall2(function)
    checkStackPointerAlignment(t4, 0xbad0c002)
    if C_LOOP or C_LOOP_WIN
        cloopCallSlowPath function, a0, a1
    elsif X86_64 or ARM64 or ARM64E or RISCV64
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
        move 8[r0], r1
        move [r0], r0
        addp 48, sp
    else
        error
    end
end

macro cCall2Void(function)
    if C_LOOP or C_LOOP_WIN
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

macro cCall3(function)
    checkStackPointerAlignment(t4, 0xbad0c004)
    if C_LOOP or C_LOOP_WIN
        cloopCallSlowPath3 function, a0, a1, a2
    elsif X86_64 or ARM64 or ARM64E or RISCV64
        call function
    elsif X86_64_WIN
        # Note: this implementation is only correct if the return type size is > 8 bytes.
        # See macro cCall2Void for an implementation when the return type <= 8 bytes.
        # On Win64, when the return type is larger than 8 bytes, we need to allocate space on the stack for the return value.
        # On entry rcx (a0), should contain a pointer to this stack space. The other parameters are shifted to the right,
        # rdx (a1) should contain the first argument, r8 (a2) should contain the second argument, and r9 (a3) should contain the third argument.
        # On return, rax contains a pointer to this stack value, and we then need to copy the 16 byte return value into rax (r0) and rdx (r1)
        # since the return value is expected to be split between the two.
        # See http://msdn.microsoft.com/en-us/library/7572ztz4.aspx
        move a2, a3
        move a1, a2
        move a0, a1
        subp 64, sp
        move sp, a0
        addp 32, a0
        call function
        move 8[r0], r1
        move [r0], r0
        addp 64, sp
    else
        error
    end
end

# This barely works. arg3 and arg4 should probably be immediates.
macro cCall4(function)
    checkStackPointerAlignment(t4, 0xbad0c004)
    if C_LOOP or C_LOOP_WIN
        cloopCallSlowPath4 function, a0, a1, a2, a3
    elsif X86_64 or ARM64 or ARM64E or RISCV64
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

    loadi VM::disallowVMEntryCount[vm], t4
    btinz t4, .checkVMEntryPermission

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
    if C_LOOP or C_LOOP_WIN
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
    # Copy the CodeBlock/Callee/ArgumentCountIncludingThis/|this| from protoCallFrame into the callee frame.
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

    makeCall(entry, protoCallFrame, t3, t4)

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

.checkVMEntryPermission:
    move vm, a0
    move protoCallFrame, a1
    cCall2(_llint_check_vm_entry_permission)
    move ValueUndefined, r0

    subp cfr, CalleeRegisterSaveSize, sp
    popCalleeSaves()
    functionEpilogue()
    ret
end

# a0, a2, t3, t4
macro makeJavaScriptCall(entry, protoCallFrame, temp1, temp2)
    addp 16, sp
    if C_LOOP or C_LOOP_WIN
        cloopCallJSFunction entry
    elsif ARM64E
        move entry, t5
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::vmEntryToJavaScript) * PtrSize, a7
        jmp [a7], NativeToJITGatePtrTag # JSEntryPtrTag
        global _vmEntryToJavaScriptTrampoline
        _vmEntryToJavaScriptTrampoline:
        call t5, JSEntryPtrTag
    else
        call entry, JSEntryPtrTag
    end
    if ARM64E
        global _vmEntryToJavaScriptGateAfter
        _vmEntryToJavaScriptGateAfter:
    end
    subp 16, sp
end

# a0, a2, t3, t4
macro makeHostFunctionCall(entry, protoCallFrame, temp1, temp2)
    move entry, temp1
    storep cfr, [sp]
    loadp ProtoCallFrame::globalObject[protoCallFrame], a0
    move sp, a1
    if C_LOOP or C_LOOP_WIN
        storep lr, 8[sp]
        cloopCallNative temp1
    elsif X86_64_WIN
        # We need to allocate 32 bytes on the stack for the shadow space.
        subp 32, sp
        call temp1, HostFunctionPtrTag
        addp 32, sp
    else
        call temp1, HostFunctionPtrTag
    end
end

op(llint_handle_uncaught_exception, macro ()
    loadp Callee[cfr], t3
    convertCalleeToVM(t3)
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

op(llint_get_host_call_return_value, macro ()
    functionPrologue()
    pushCalleeSaves()
    loadp Callee[cfr], t0
    convertCalleeToVM(t0)
    loadq VM::encodedHostCallReturnValue[t0], t0
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

# Call a slow path for call opcodes.
macro callCallSlowPath(slowPath, action)
    storePC()
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    cCall2(slowPath)
    action(r0, r1)
end

macro callTrapHandler(throwHandler)
    storePC()
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    cCall2(_llint_slow_path_handle_traps)
    btpnz r0, throwHandler
    loadi LLIntReturnPC[cfr], PC
end

if JIT
    macro checkSwitchToJITForLoop()
        checkSwitchToJIT(
            1,
            macro()
                storePC()
                prepareStateForCCall()
                move cfr, a0
                move PC, a1
                cCall2(_llint_loop_osr)
                btpz r0, .recover
                move r1, sp

                loadBaselineJITConstantPool()

                if ARM64E
                    leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::loopOSREntry) * PtrSize, a2
                    jmp [a2], NativeToJITGatePtrTag # JSEntryPtrTag
                else
                    jmp r0, JSEntryPtrTag
                end
            .recover:
                loadPC()
            end)
    end
else
    macro checkSwitchToJITForLoop()
        checkSwitchToJIT(
            1,
            macro()
                storePC()
                prepareStateForCCall()
                move cfr, a0
                move PC, a1
                cCall2(_llint_loop_osr)
                loadPC()
            end)
    end
end

macro cage(basePtr, mask, ptr, scratch)
    if GIGACAGE_ENABLED and not (C_LOOP or C_LOOP_WIN)
        loadp basePtr, scratch
        btpz scratch, .done
        andp mask, ptr
        addp scratch, ptr
    .done:
    end
end

macro cagePrimitive(basePtr, mask, ptr, scratch)
    if GIGACAGE_ENABLED and not (C_LOOP or C_LOOP_WIN)
        loadb GigacageConfig + Gigacage::Config::disablingPrimitiveGigacageIsForbidden, scratch
        btbnz scratch, .doCaging

        loadb _disablePrimitiveGigacageRequested, scratch
        btbnz scratch, .done

    .doCaging:
        cage(basePtr, mask, ptr, scratch)
    .done:
    end
end

macro cagedPrimitive(ptr, length, scratch, scratch2)
    if ARM64E
        const source = scratch2
        move ptr, scratch2
    else
        const source = ptr
    end
    if GIGACAGE_ENABLED
        cagePrimitive(GigacageConfig + Gigacage::Config::basePtrs + GigacagePrimitiveBasePtrOffset, constexpr Gigacage::primitiveGigacageMask, source, scratch)
        if ARM64E
            const maxNumberOfAllowedPACBits = constexpr MacroAssembler::maxNumberOfAllowedPACBits
            bfiq scratch2, 0, 64 - maxNumberOfAllowedPACBits, ptr
        end
    end
    if ARM64E
        untagArrayPtr length, ptr
    end
end

macro cagedPrimitiveMayBeNull(ptr, length, scratch, scratch2)
    if ARM64E
        const source = scratch2
        move ptr, scratch2
        removeArrayPtrTag scratch2
        btpz scratch2, .nullCase
        move ptr, scratch2
    else
        # Note that we may produce non-nullptr for nullptr in non-ARM64E architecture since we add Gigacage offset.
        # But this behavior is aligned to AssemblyHelpers::{cageConditionallyAndUntag,cageWithoutUntagging}, FTL implementation of caging etc.
        const source = ptr
    end
    if GIGACAGE_ENABLED
        cagePrimitive(GigacageConfig + Gigacage::Config::basePtrs + GigacagePrimitiveBasePtrOffset, constexpr Gigacage::primitiveGigacageMask, source, scratch)
        if ARM64E
            const maxNumberOfAllowedPACBits = constexpr MacroAssembler::maxNumberOfAllowedPACBits
            bfiq scratch2, 0, 64 - maxNumberOfAllowedPACBits, ptr
        end
    end
    if ARM64E
        .nullCase:
        untagArrayPtr length, ptr
    end
end

macro loadCagedJSValue(source, dest, scratchOrLength)
    loadp source, dest
    if GIGACAGE_ENABLED
        cage(GigacageConfig + Gigacage::Config::basePtrs + GigacageJSValueBasePtrOffset, constexpr Gigacage::jsValueGigacageMask, dest, scratchOrLength)
    end
end

macro loadVariable(get, fieldName, valueReg)
    get(fieldName, valueReg)
    loadq [cfr, valueReg, 8], valueReg
end

macro storeVariable(get, fieldName, newValueReg, scratchReg)
    get(fieldName, scratchReg)
    storeq newValueReg, [cfr, scratchReg, 8]
end

# Index and value must be different registers. Index may be clobbered.
macro loadConstant(size, index, value)
    macro loadNarrow()
        loadp CodeBlock[cfr], value
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[value], value
        loadq -(FirstConstantRegisterIndexNarrow * 8)[value, index, 8], value
    end

    macro loadWide16()
        loadp CodeBlock[cfr], value
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[value], value
        loadq -(FirstConstantRegisterIndexWide16 * 8)[value, index, 8], value
    end

    macro loadWide32()
        loadp CodeBlock[cfr], value
        loadp CodeBlock::m_constantRegisters + VectorBufferOffset[value], value
        subp FirstConstantRegisterIndexWide32, index
        loadq [value, index, 8], value
    end

    size(loadNarrow, loadWide16, loadWide32, macro (load) load() end)
end

# Index and value must be different registers. Index may be clobbered.
macro loadConstantOrVariable(size, index, value)
    macro loadNarrow()
        bpgteq index, FirstConstantRegisterIndexNarrow, .constant
        loadq [cfr, index, 8], value
        jmp .done
    .constant:
        loadConstant(size, index, value)
    .done:
    end

    macro loadWide16()
        bpgteq index, FirstConstantRegisterIndexWide16, .constant
        loadq [cfr, index, 8], value
        jmp .done
    .constant:
        loadConstant(size, index, value)
    .done:
    end

    macro loadWide32()
        bpgteq index, FirstConstantRegisterIndexWide32, .constant
        loadq [cfr, index, 8], value
        jmp .done
    .constant:
        loadConstant(size, index, value)
    .done:
    end

    size(loadNarrow, loadWide16, loadWide32, macro (load) load() end)
end

macro loadConstantOrVariableInt32(size, index, value, slow)
    loadConstantOrVariable(size, index, value)
    bqb value, numberTag, slow
end

macro loadConstantOrVariableCell(size, index, value, slow)
    loadConstantOrVariable(size, index, value)
    btqnz value, notCellMask, slow
end

macro writeBarrierOnCellWithReload(cell, reloadAfterSlowPath)
    skipIfIsRememberedOrInEden(
        cell,
        macro()
            push PB, PC
            move cell, a1 # cell can be a0
            move cfr, a0
            cCall2Void(_llint_write_barrier_slow)
            pop PC, PB
            reloadAfterSlowPath()
        end)
end

macro writeBarrierOnCellAndValueWithReload(cell, value, reloadAfterSlowPath)
    btqnz value, notCellMask, .writeBarrierDone
    btqz value, .writeBarrierDone
    writeBarrierOnCellWithReload(cell, reloadAfterSlowPath)
.writeBarrierDone:
end

macro writeBarrierOnOperandWithReload(size, get, cellFieldName, reloadAfterSlowPath)
    get(cellFieldName, t1)
    loadConstantOrVariableCell(size, t1, t2, .writeBarrierDone)
    writeBarrierOnCellWithReload(t2, reloadAfterSlowPath)
.writeBarrierDone:
end

macro writeBarrierOnOperand(size, get, cellFieldName)
    writeBarrierOnOperandWithReload(size, get, cellFieldName, macro () end)
end

macro writeBarrierOnOperands(size, get, cellFieldName, valueFieldName)
    get(valueFieldName, t1)
    loadConstantOrVariableCell(size, t1, t0, .writeBarrierDone)
    btpz t0, .writeBarrierDone

    writeBarrierOnOperand(size, get, cellFieldName)
.writeBarrierDone:
end

macro writeBarrierOnGlobal(size, get, valueFieldName, loadMacro)
    get(valueFieldName, t1)
    loadConstantOrVariableCell(size, t1, t0, .writeBarrierDone)
    btpz t0, .writeBarrierDone

    loadMacro(t3)
    writeBarrierOnCellWithReload(t3, macro() end)
.writeBarrierDone:
end

macro writeBarrierOnGlobalObject(size, get, valueFieldName)
    writeBarrierOnGlobal(size, get, valueFieldName,
        macro(registerToStoreGlobal)
            loadp CodeBlock[cfr], registerToStoreGlobal
            loadp CodeBlock::m_globalObject[registerToStoreGlobal], registerToStoreGlobal
        end)
end

macro writeBarrierOnGlobalLexicalEnvironment(size, get, valueFieldName)
    writeBarrierOnGlobal(size, get, valueFieldName,
        macro(registerToStoreGlobal)
            loadp CodeBlock[cfr], registerToStoreGlobal
            loadp CodeBlock::m_globalObject[registerToStoreGlobal], registerToStoreGlobal
            loadp JSGlobalObject::m_globalLexicalEnvironment[registerToStoreGlobal], registerToStoreGlobal
        end)
end

macro structureIDToStructureWithScratch(structureIDThenStructure, scratch)
    if STRUCTURE_ID_WITH_SHIFT
        lshiftp (constexpr StructureID::encodeShiftAmount), structureIDThenStructure
    elsif ADDRESS64
        andq (constexpr StructureID::structureIDMask), structureIDThenStructure
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigStartOfStructureHeap, scratch
        loadp [scratch], scratch
        addp scratch, structureIDThenStructure
    end
end

macro loadStructureWithScratch(cell, structure, scratch)
    loadi JSCell::m_structureID[cell], structure
    structureIDToStructureWithScratch(structure, scratch)
end

# Entrypoints into the interpreter.

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
macro functionArityCheck(opcodeName, doneLabel)
    loadi PayloadOffset + ArgumentCountIncludingThis[cfr], t0
    loadi CodeBlock::m_numParameters[t1], t2
    biaeq t0, t2, doneLabel

    # t0 argumentCountIncludingThis
    # t1 CodeBlock
    # t2 numParameters

    addi CallFrameHeaderSlots, t2, t3
    btiz t3, 0x1, .arityCheck
    addi 1, t2
.arityCheck:
    subi t2, t0, t2
    addi 1, t2, t3
    andi ~1, t3
    lshiftp 3, t3
    subp cfr, t3, t5
    loadp CodeBlock::m_vm[t1], t0
    if C_LOOP or C_LOOP_WIN
        bpbeq VM::m_cloopStackLimit[t0], t5, .stackHeightOK
    else
        bpbeq VM::m_softStackLimit[t0], t5, .stackHeightOK
    end

    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    cCall2(_llint_slow_path_arityCheck)   # This slowPath has the protocol: r0 = 0 => no error, r0 != 0 => error
    btiz r0, .noError

    # We're throwing before the frame is fully set up. This frame will be
    # ignored by the unwinder. So, let's restore the callee saves before we
    # start unwinding. We need to do this before we change the cfr.
    restoreCalleeSavesUsedByLLInt()

    move r1, cfr   # r1 contains caller frame
    jmp _llint_throw_from_slow_path_trampoline

.stackHeightOK:
    move t2, r1
.noError:
    move r1, t1 # r1 contains slotsToAdd.
    btiz t1, .continue
    loadi PayloadOffset + ArgumentCountIncludingThis[cfr], t2
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
    if ARM64E
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::%opcodeName%Untag) * PtrSize, t3
        jmp [t3], NativeToJITGatePtrTag
        _js_trampoline_%opcodeName%_untag:
        loadp 8[cfr], lr
        addp 16, cfr, t3
        untagReturnAddress t3
        _%opcodeName%UntagGateAfter:
    else
        _js_trampoline_%opcodeName%_untag:
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

    if ARM64E
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::%opcodeName%Tag) * PtrSize, t3
        jmp [t3], NativeToJITGatePtrTag
        _js_trampoline_%opcodeName%_tag:
        addp 16, cfr, t1
        tagReturnAddress t1
        storep lr, 8[cfr]
        _%opcodeName%TagGateAfter:
    else
        _js_trampoline_%opcodeName%_tag:
    end

.continue:
    # Reload CodeBlock and reset PC, since the slow_path clobbered them.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_instructionsRawPointer[t1], PB
    move 0, PC
    jmp doneLabel
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
    get(m_index, t2)
    loadi PayloadOffset + ArgumentCountIncludingThis[cfr], t0
    bilteq t0, t2, .opGetArgumentOutOfBounds
    loadq ThisArgumentOffset[cfr, t2, 8], t0
    return(t0)

.opGetArgumentOutOfBounds:
    return(ValueUndefined)
end)


llintOpWithReturn(op_argument_count, OpArgumentCount, macro (size, get, dispatch, return)
    loadi PayloadOffset + ArgumentCountIncludingThis[cfr], t0
    subi 1, t0
    orq TagNumber, t0
    return(t0)
end)


llintOpWithReturn(op_get_scope, OpGetScope, macro (size, get, dispatch, return)
    loadp Callee[cfr], t0
    loadp JSCallee::m_scope[t0], t0
    return(t0)
end)


llintOpWithMetadata(op_to_this, OpToThis, macro (size, get, dispatch, metadata, return)
    get(m_srcDst, t0)
    loadq [cfr, t0, 8], t0
    btqnz t0, notCellMask, .opToThisSlow
    bbneq JSCell::m_type[t0], FinalObjectType, .opToThisSlow
    loadi JSCell::m_structureID[t0], t1
    metadata(t2, t3)
    loadi OpToThis::Metadata::m_cachedStructureID[t2], t2
    bineq t1, t2, .opToThisSlow
    dispatch()

.opToThisSlow:
    callSlowPath(_slow_path_to_this)
    dispatch()
end)


llintOp(op_check_tdz, OpCheckTdz, macro (size, get, dispatch)
    get(m_targetVirtualRegister, t0)
    loadConstantOrVariable(size, t0, t1)
    bqneq t1, ValueEmpty, .opNotTDZ
    callSlowPath(_slow_path_check_tdz)

.opNotTDZ:
    dispatch()
end)


llintOpWithReturn(op_mov, OpMov, macro (size, get, dispatch, return)
    get(m_src, t1)
    loadConstantOrVariable(size, t1, t2)
    return(t2)
end)


llintOpWithReturn(op_not, OpNot, macro (size, get, dispatch, return)
    get(m_operand, t0)
    loadConstantOrVariable(size, t0, t2)
    xorq ValueFalse, t2
    btqnz t2, ~1, .opNotSlow
    xorq ValueTrue, t2
    return(t2)

.opNotSlow:
    callSlowPath(_slow_path_not)
    dispatch()
end)


macro equalityComparisonOp(opcodeName, opcodeStruct, integerComparison)
    llintOpWithReturn(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, return)
        get(m_rhs, t0)
        get(m_lhs, t2)
        loadConstantOrVariableInt32(size, t0, t1, .slow)
        loadConstantOrVariableInt32(size, t2, t0, .slow)
        integerComparison(t0, t1, t0)
        orq ValueFalse, t0
        return(t0)

    .slow:
        callSlowPath(_slow_path_%opcodeName%)
        dispatch()
    end)
end


macro equalNullComparisonOp(opcodeName, opcodeStruct, fn)
    llintOpWithReturn(opcodeName, opcodeStruct, macro (size, get, dispatch, return)
        get(m_operand, t0)
        loadq [cfr, t0, 8], t0
        btqnz t0, notCellMask, .immediate
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
        andq ~TagUndefined, t0
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
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t0)
    andq ~TagUndefined, t0
    cqeq t0, ValueNull, t0
    orq ValueFalse, t0
    return(t0)
end)


macro strictEqOp(opcodeName, opcodeStruct, createBoolean)
    llintOpWithReturn(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, return)
        get(m_rhs, t0)
        get(m_lhs, t2)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)

        # At a high level we do
        # If (left is Double || right is Double)
        #     goto slowPath;
        # result = (left == right);
        # if (result)
        #     goto done;
        # if (left is Cell || right is Cell)
        #     goto slowPath;
        # done:
        # return result;

        # This fragment implements (left is Double || right is Double), with a single branch instead of the 4 that would be naively required if we used branchIfInt32/branchIfNumber
        # The trick is that if a JSValue is an Int32, then adding 1<<49 to it will make it overflow, leaving all high bits at 0
        # If it is not a number at all, then 1<<49 will be its only high bit set
        # Leaving only doubles above or equal 1<<50.
        move t0, t2
        move t1, t3
        move LowestOfHighBits, t5
        addq t5, t2
        addq t5, t3
        orq t2, t3
        lshiftq 1, t5
        bqaeq t3, t5, .slow

        cqeq t0, t1, t5
        btqnz t5, t5, .done #is there a better way of checking t5 != 0 ?

        move t0, t2
        # This andq could be an 'or' if not for BigInt32 (since it makes it possible for a Cell to be strictEqual to a non-Cell)
        andq t1, t2
        btqz t2, notCellMask, .slow

    .done:
        createBoolean(t5)
        return(t5)

    .slow:
        callSlowPath(_slow_path_%opcodeName%)
        dispatch()
    end)
end


strictEqOp(stricteq, OpStricteq,
    macro (operand) xorq ValueFalse, operand end)


strictEqOp(nstricteq, OpNstricteq,
    macro (operand) xorq ValueTrue, operand end)


macro strictEqualityJumpOp(opcodeName, opcodeStruct, jumpIfEqual, jumpIfNotEqual)
    llintOpWithJump(op_%opcodeName%, opcodeStruct, macro (size, get, jump, dispatch)
        get(m_lhs, t2)
        get(m_rhs, t3)
        loadConstantOrVariable(size, t2, t0)
        loadConstantOrVariable(size, t3, t1)

        # At a high level we do
        # If (left is Double || right is Double)
        #     goto slowPath;
        # if (left == right)
        #     goto jumpTarget;
        # if (left is Cell || right is Cell)
        #     goto slowPath;
        # goto otherJumpTarget

        # This fragment implements (left is Double || right is Double), with a single branch instead of the 4 that would be naively required if we used branchIfInt32/branchIfNumber
        # The trick is that if a JSValue is an Int32, then adding 1<<49 to it will make it overflow, leaving all high bits at 0
        # If it is not a number at all, then 1<<49 will be its only high bit set
        # Leaving only doubles above or equal 1<<50.
        move t0, t2
        move t1, t3
        move LowestOfHighBits, t5
        addq t5, t2
        addq t5, t3
        orq t2, t3
        lshiftq 1, t5
        bqaeq t3, t5, .slow

        bqeq t0, t1, .equal

        move t0, t2
        # This andq could be an 'or' if not for BigInt32 (since it makes it possible for a Cell to be strictEqual to a non-Cell)
        andq t1, t2
        btqz t2, notCellMask, .slow

        jumpIfNotEqual(jump, m_targetLabel, dispatch)

    .equal:
        jumpIfEqual(jump, m_targetLabel, dispatch)

    .slow:
        callSlowPath(_llint_slow_path_%opcodeName%)
        nextInstruction()
    end)
end


strictEqualityJumpOp(jstricteq, OpJstricteq,
    macro (jump, targetLabel, dispatch) jump(targetLabel) end,
    macro (jump, targetLabel, dispatch) dispatch() end)

strictEqualityJumpOp(jnstricteq, OpJnstricteq,
    macro (jump, targetLabel, dispatch) dispatch() end,
    macro (jump, targetLabel, dispatch) jump(targetLabel) end)

macro preOp(opcodeName, opcodeStruct, integerOperation)
    llintOpWithMetadata(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, metadata, return)
        get(m_srcDst, t0)
        loadq [cfr, t0, 8], t1
        # srcDst in t1
        # FIXME: the next line jumps to the slow path for BigInt32. We could instead have a dedicated path in here for them.
        bqb t1, numberTag, .slow
        integerOperation(t1, .slow)
        orq numberTag, t1
        storeq t1, [cfr, t0, 8]
        updateUnaryArithProfile(size, opcodeStruct, ArithProfileInt, t5, t3)
        dispatch()

    .slow:
        callSlowPath(_slow_path_%opcodeName%)
        dispatch()
    end)

end

llintOpWithProfile(op_to_number, OpToNumber, macro (size, get, dispatch, return)
    get(m_operand, t0)
    loadConstantOrVariable(size, t0, t2)
    bqaeq t2, numberTag, .opToNumberIsImmediate
    btqz t2, numberTag, .opToNumberSlow
.opToNumberIsImmediate:
    return(t2)

.opToNumberSlow:
    callSlowPath(_slow_path_to_number)
    dispatch()
end)

llintOpWithProfile(op_to_numeric, OpToNumeric, macro (size, get, dispatch, return)
    get(m_operand, t0)
    loadConstantOrVariable(size, t0, t2)
    bqaeq t2, numberTag, .opToNumericIsImmediate
    btqz t2, numberTag, .opToNumericSlow
.opToNumericIsImmediate:
    return(t2)

.opToNumericSlow:
    callSlowPath(_slow_path_to_numeric)
    dispatch()
end)


llintOpWithReturn(op_to_string, OpToString, macro (size, get, dispatch, return)
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t0)
    btqnz t0, notCellMask, .opToStringSlow
    bbneq JSCell::m_type[t0], StringType, .opToStringSlow
.opToStringIsString:
    return(t0)

.opToStringSlow:
    callSlowPath(_slow_path_to_string)
    dispatch()
end)


llintOpWithProfile(op_to_object, OpToObject, macro (size, get, dispatch, return)
    get(m_operand, t0)
    loadConstantOrVariable(size, t0, t2)
    btqnz t2, notCellMask, .opToObjectSlow
    bbb JSCell::m_type[t2], ObjectType, .opToObjectSlow
    return(t2)

.opToObjectSlow:
    callSlowPath(_slow_path_to_object)
    dispatch()
end)


llintOpWithMetadata(op_negate, OpNegate, macro (size, get, dispatch, metadata, return)
    get(m_operand, t0)
    loadConstantOrVariable(size, t0, t3)
    bqb t3, numberTag, .opNegateNotInt
    btiz t3, 0x7fffffff, .opNegateSlow
    negi t3
    orq numberTag, t3
    updateUnaryArithProfile(size, OpNegate, ArithProfileInt, t1, t2)
    return(t3)
.opNegateNotInt:
    btqz t3, numberTag, .opNegateSlow
    xorq 0x8000000000000000, t3
    updateUnaryArithProfile(size, OpNegate, ArithProfileNumber, t1, t2)
    return(t3)

.opNegateSlow:
    callSlowPath(_slow_path_negate)
    dispatch()
end)


macro binaryOpCustomStore(opcodeName, opcodeStruct, integerOperationAndStore, doubleOperation)
    llintOpWithMetadata(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, metadata, return)
        get(m_rhs, t0)
        get(m_lhs, t2)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        bqb t0, numberTag, .op1NotInt
        bqb t1, numberTag, .op2NotInt
        get(m_dst, t2)
        integerOperationAndStore(t0, t1, .slow, t2)

        updateBinaryArithProfile(size, opcodeStruct, ArithProfileIntInt, t5, t2)
        dispatch()

    .op1NotInt:
        # First operand is definitely not an int, the second operand could be anything.
        btqz t0, numberTag, .slow
        bqaeq t1, numberTag, .op1NotIntOp2Int
        btqz t1, numberTag, .slow
        addq numberTag, t1
        fq2d t1, ft1
        updateBinaryArithProfile(size, opcodeStruct, ArithProfileNumberNumber, t5, t2)
        jmp .op1NotIntReady
    .op1NotIntOp2Int:
        updateBinaryArithProfile(size, opcodeStruct, ArithProfileNumberInt, t5, t2)
        ci2ds t1, ft1
    .op1NotIntReady:
        get(m_dst, t2)
        addq numberTag, t0
        fq2d t0, ft0
        doubleOperation(ft0, ft1)
        fd2q ft0, t0
        subq numberTag, t0
        storeq t0, [cfr, t2, 8]
        dispatch()

    .op2NotInt:
        # First operand is definitely an int, the second is definitely not.
        btqz t1, numberTag, .slow
        updateBinaryArithProfile(size, opcodeStruct, ArithProfileIntNumber, t5, t2)
        get(m_dst, t2)
        ci2ds t0, ft0
        addq numberTag, t1
        fq2d t1, ft1
        doubleOperation(ft0, ft1)
        fd2q ft0, t0
        subq numberTag, t0
        storeq t0, [cfr, t2, 8]
        dispatch()

    .slow:
        callSlowPath(_slow_path_%opcodeName%)
        dispatch()
    end)
end

if X86_64 or X86_64_WIN
    binaryOpCustomStore(div, OpDiv,
        macro (lhs, rhs, slow, index)
            # Assume t3 is scratchable.
            btiz rhs, slow
            bineq rhs, -1, .notNeg2ToThe31DivByNeg1
            bieq lhs, -2147483648, slow
        .notNeg2ToThe31DivByNeg1:
            btinz lhs, .intOK
            bilt rhs, 0, slow
        .intOK:
            move rhs, t3
            cdqi
            idivi t3
            btinz t1, slow
            orq numberTag, t0
            storeq t0, [cfr, index, 8]
        end,
        macro (lhs, rhs) divd rhs, lhs end)
else
    slowPathOp(div)
end


binaryOpCustomStore(mul, OpMul,
    macro (lhs, rhs, slow, index)
        # Assume t3 is scratchable.
        move lhs, t3
        bmulio rhs, t3, slow
        btinz t3, .done
        bilt rhs, 0, slow
        bilt lhs, 0, slow
    .done:
        orq numberTag, t3
        storeq t3, [cfr, index, 8]
    end,
    macro (lhs, rhs) muld rhs, lhs end)


macro binaryOp(opcodeName, opcodeStruct, integerOperation, doubleOperation)
    binaryOpCustomStore(opcodeName, opcodeStruct,
        macro (lhs, rhs, slow, index)
            integerOperation(lhs, rhs, slow)
            orq numberTag, lhs
            storeq lhs, [cfr, index, 8]
        end,
        doubleOperation)
end

binaryOp(add, OpAdd,
    macro (lhs, rhs, slow) baddio rhs, lhs, slow end,
    macro (lhs, rhs) addd rhs, lhs end)


binaryOp(sub, OpSub,
    macro (lhs, rhs, slow) bsubio rhs, lhs, slow end,
    macro (lhs, rhs) subd rhs, lhs end)

if X86_64 or X86_64_WIN
    llintOpWithReturn(op_mod, OpMod, macro (size, get, dispatch, return)
        get(m_rhs, t0)
        get(m_lhs, t2)
        loadConstantOrVariableInt32(size, t0, t1, .slow)
        loadConstantOrVariableInt32(size, t2, t0, .slow)

        # Assume t3 is scratchable.
        # r1 is always edx (even on Windows).
        btiz t1, .slow
        bineq t1, -1, .notNeg2ToThe31ModByNeg1
        bieq t0, -2147483648, .slow
    .notNeg2ToThe31ModByNeg1:
        move t1, t3
        bilt t0, 0, .needsNegZeroCheck
        cdqi
        idivi t3
        orq numberTag, r1
        return(r1)
    .needsNegZeroCheck:
        cdqi
        idivi t3
        btiz r1, .slow
        orq numberTag, r1
        return(r1)

    .slow:
        callSlowPath(_slow_path_mod)
        dispatch()
    end)
else
    slowPathOp(mod)
end

llintOpWithReturn(op_pow, OpPow, macro (size, get, dispatch, return)
    get(m_rhs, t0)
    get(m_lhs, t2)
    loadConstantOrVariableInt32(size, t0, t1, .slow)
    loadConstantOrVariable(size, t2, t0)

    bilt t1, 0, .slow
    bigt t1, (constexpr maxExponentForIntegerMathPow), .slow

    bqb t0, numberTag, .lhsNotInt
    ci2ds t0, ft0
    jmp .lhsReady
.lhsNotInt:
    btqz t0, numberTag, .slow
    addq numberTag, t0
    fq2d t0, ft0
.lhsReady:
    get(m_dst, t2)
    move 1, t0
    ci2ds t0, ft1

.loop:
    btiz t1, 0x1, .exponentIsEven
    muld ft0, ft1
.exponentIsEven:
    muld ft0, ft0
    rshifti 1, t1
    btinz t1, .loop

    fd2q ft1, t0
    subq numberTag, t0
    storeq t0, [cfr, t2, 8]
    dispatch()

.slow:
    callSlowPath(_slow_path_pow)
    dispatch()
end)

llintOpWithReturn(op_unsigned, OpUnsigned, macro (size, get, dispatch, return)
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t2)
    bilt t2, 0, .opUnsignedSlow
    return(t2)
.opUnsignedSlow:
    callSlowPath(_slow_path_unsigned)
    dispatch()
end)


macro commonBitOp(opKind, opcodeName, opcodeStruct, operation)
    opKind(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, return)
        get(m_rhs, t0)
        get(m_lhs, t2)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        bqb t0, numberTag, .slow
        bqb t1, numberTag, .slow
        operation(t0, t1)
        orq numberTag, t0
        return(t0)

    .slow:
        callSlowPath(_slow_path_%opcodeName%)
        dispatch()
    end)
end

macro bitOp(opcodeName, opcodeStruct, operation)
    commonBitOp(llintOpWithReturn, opcodeName, opcodeStruct, operation)
end

macro bitOpProfiled(opcodeName, opcodeStruct, operation)
    commonBitOp(llintOpWithProfile, opcodeName, opcodeStruct, operation)
end

bitOpProfiled(lshift, OpLshift,
    macro (lhs, rhs) lshifti rhs, lhs end)


bitOpProfiled(rshift, OpRshift,
    macro (lhs, rhs) rshifti rhs, lhs end)


bitOp(urshift, OpUrshift,
    macro (lhs, rhs) urshifti rhs, lhs end)

bitOpProfiled(bitand, OpBitand,
    macro (lhs, rhs) andi rhs, lhs end)

bitOpProfiled(bitor, OpBitor,
    macro (lhs, rhs) ori rhs, lhs end)

bitOpProfiled(bitxor, OpBitxor,
    macro (lhs, rhs) xori rhs, lhs end)

llintOpWithProfile(op_bitnot, OpBitnot, macro (size, get, dispatch, return)
    get(m_operand, t0)
    loadConstantOrVariableInt32(size, t0, t3, .opBitNotSlow)
    noti t3
    orq numberTag, t3
    return(t3)
.opBitNotSlow:
    callSlowPath(_slow_path_bitnot)
    dispatch()
end)


llintOp(op_overrides_has_instance, OpOverridesHasInstance, macro (size, get, dispatch)
    get(m_dst, t3)

    get(m_hasInstanceValue, t1)
    loadConstantOrVariable(size, t1, t0)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_globalObject[t2], t2
    loadp JSGlobalObject::m_functionProtoHasInstanceSymbolFunction[t2], t2
    bqneq t0, t2, .opOverridesHasInstanceNotDefaultSymbol

    get(m_constructor, t1)
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
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t0)
    cqeq t0, ValueEmpty, t3
    orq ValueFalse, t3
    return(t3)
end)


llintOpWithReturn(op_typeof_is_undefined, OpTypeofIsUndefined, macro (size, get, dispatch, return)
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t0)
    btqz t0, notCellMask, .opIsUndefinedCell
    cqeq t0, ValueUndefined, t3
    orq ValueFalse, t3
    return(t3)
.opIsUndefinedCell:
    btbnz JSCell::m_flags[t0], MasqueradesAsUndefined, .masqueradesAsUndefined
    move ValueFalse, t1
    return(t1)
.masqueradesAsUndefined:
    loadStructureWithScratch(t0, t3, t1)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_globalObject[t1], t1
    cpeq Structure::m_globalObject[t3], t1, t0
    orq ValueFalse, t0
    return(t0)
end)


llintOpWithReturn(op_is_boolean, OpIsBoolean, macro (size, get, dispatch, return)
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t0)
    xorq ValueFalse, t0
    tqz t0, ~1, t0
    orq ValueFalse, t0
    return(t0)
end)


llintOpWithReturn(op_is_number, OpIsNumber, macro (size, get, dispatch, return)
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t0)
    tqnz t0, numberTag, t1
    orq ValueFalse, t1
    return(t1)
end)

if BIGINT32
    llintOpWithReturn(op_is_big_int, OpIsBigInt, macro(size, get, dispatch, return)
        get(m_operand, t1)
        loadConstantOrVariable(size, t1, t0)
        btqnz t0, notCellMask, .notCellCase
        cbeq JSCell::m_type[t0], HeapBigIntType, t1
        orq ValueFalse, t1
        return(t1)
    .notCellCase:
        andq MaskBigInt32, t0
        cqeq t0, TagBigInt32, t0
        orq ValueFalse, t0
        return(t0)
    end)
else
# if BIGINT32 is not supported we generate op_is_cell_with_type instead of op_is_big_int
    llintOp(op_is_big_int, OpIsBigInt, macro(unused, unused, unused)
        notSupported()
    end)
end

llintOpWithReturn(op_is_cell_with_type, OpIsCellWithType, macro (size, get, dispatch, return)
    getu(size, OpIsCellWithType, m_type, t0)
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t3)
    btqnz t3, notCellMask, .notCellCase
    cbeq JSCell::m_type[t3], t0, t1
    orq ValueFalse, t1
    return(t1)
.notCellCase:
    return(ValueFalse)
end)


llintOpWithReturn(op_is_object, OpIsObject, macro (size, get, dispatch, return)
    get(m_operand, t1)
    loadConstantOrVariable(size, t1, t0)
    btqnz t0, notCellMask, .opIsObjectNotCell
    cbaeq JSCell::m_type[t0], ObjectType, t1
    orq ValueFalse, t1
    return(t1)
.opIsObjectNotCell:
    return(ValueFalse)
end)


macro loadInlineOffset(propertyOffsetAsInt, objectAndStorage, value)
    addp sizeof JSObject - (firstOutOfLineOffset - 2) * 8, objectAndStorage
    loadq (firstOutOfLineOffset - 2) * 8[objectAndStorage, propertyOffsetAsInt, 8], value
end


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


llintOpWithMetadata(op_try_get_by_id, OpTryGetById, macro (size, get, dispatch, metadata, return)
    metadata(t2, t0)
    get(m_base, t0)
    loadConstantOrVariableCell(size, t0, t3, .opTryGetByIdSlow)
    loadi JSCell::m_structureID[t3], t1
    loadi OpTryGetById::Metadata::m_structureID[t2], t0
    bineq t0, t1, .opTryGetByIdSlow
    loadi OpTryGetById::Metadata::m_offset[t2], t1
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(OpTryGetById, m_profile, t2, t0)
    return(t0)

.opTryGetByIdSlow:
    callSlowPath(_llint_slow_path_try_get_by_id)
    dispatch()
end)

llintOpWithMetadata(op_get_by_id_direct, OpGetByIdDirect, macro (size, get, dispatch, metadata, return)
    metadata(t2, t0)
    get(m_base, t0)
    loadConstantOrVariableCell(size, t0, t3, .opGetByIdDirectSlow)
    loadi JSCell::m_structureID[t3], t1
    loadi OpGetByIdDirect::Metadata::m_structureID[t2], t0
    bineq t0, t1, .opGetByIdDirectSlow
    loadi OpGetByIdDirect::Metadata::m_offset[t2], t1
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(OpGetByIdDirect, m_profile, t2, t0)
    return(t0)

.opGetByIdDirectSlow:
    callSlowPath(_llint_slow_path_get_by_id_direct)
    dispatch()
end)

# The base object is expected in t3
macro performGetByIDHelper(opcodeStruct, modeMetadataName, valueProfileName, slowLabel, size, metadata, return)
    metadata(t2, t1)
    loadb %opcodeStruct%::Metadata::%modeMetadataName%.mode[t2], t1

.opGetByIdDefault:
    bbneq t1, constexpr GetByIdMode::Default, .opGetByIdProtoLoad
    loadi JSCell::m_structureID[t3], t1
    loadi %opcodeStruct%::Metadata::%modeMetadataName%.defaultMode.structureID[t2], t0
    bineq t0, t1, slowLabel
    loadis %opcodeStruct%::Metadata::%modeMetadataName%.defaultMode.cachedOffset[t2], t1
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(opcodeStruct, valueProfileName, t2, t0)
    return(t0)

.opGetByIdProtoLoad:
    bbneq t1, constexpr GetByIdMode::ProtoLoad, .opGetByIdArrayLength
    loadi JSCell::m_structureID[t3], t1
    loadi %opcodeStruct%::Metadata::%modeMetadataName%.protoLoadMode.structureID[t2], t3
    bineq t3, t1, slowLabel
    loadis %opcodeStruct%::Metadata::%modeMetadataName%.protoLoadMode.cachedOffset[t2], t1
    loadp %opcodeStruct%::Metadata::%modeMetadataName%.protoLoadMode.cachedSlot[t2], t3
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(opcodeStruct, valueProfileName, t2, t0)
    return(t0)

.opGetByIdArrayLength:
    bbneq t1, constexpr GetByIdMode::ArrayLength, .opGetByIdUnset
    move t3, t0
    arrayProfile(%opcodeStruct%::Metadata::%modeMetadataName%.arrayLengthMode.arrayProfile, t0, t2, t5)
    btiz t0, IsArray, slowLabel
    btiz t0, IndexingShapeMask, slowLabel
    loadCagedJSValue(JSObject::m_butterfly[t3], t0, t1)
    loadi -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], t0
    bilt t0, 0, slowLabel
    orq numberTag, t0
    valueProfile(opcodeStruct, valueProfileName, t2, t0)
    return(t0)

.opGetByIdUnset:
    loadi JSCell::m_structureID[t3], t1
    loadi %opcodeStruct%::Metadata::%modeMetadataName%.unsetMode.structureID[t2], t0
    bineq t0, t1, slowLabel
    valueProfile(opcodeStruct, valueProfileName, t2, ValueUndefined)
    return(ValueUndefined)

end

llintOpWithMetadata(op_get_by_id, OpGetById, macro (size, get, dispatch, metadata, return)
    get(m_base, t0)
    loadConstantOrVariableCell(size, t0, t3, .opGetByIdSlow)
    performGetByIDHelper(OpGetById, m_modeMetadata, m_profile, .opGetByIdSlow, size, metadata, return)

.opGetByIdSlow:
    callSlowPath(_llint_slow_path_get_by_id)
    dispatch()

.osrReturnPoint:
    getterSetterOSRExitReturnPoint(op_get_by_id, size)
    metadata(t2, t3)
    valueProfile(OpGetById, m_profile, t2, r0)
    return(r0)

end)


llintOpWithProfile(op_get_prototype_of, OpGetPrototypeOf, macro (size, get, dispatch, return)
    get(m_value, t1)
    loadConstantOrVariable(size, t1, t0)

    btqnz t0, notCellMask, .opGetPrototypeOfSlow
    bbb JSCell::m_type[t0], ObjectType, .opGetPrototypeOfSlow

    loadStructureWithScratch(t0, t2, t1)
    loadh Structure::m_outOfLineTypeFlags[t2], t3
    btinz t3, OverridesGetPrototypeOutOfLine, .opGetPrototypeOfSlow

    loadq Structure::m_prototype[t2], t2
    btqz t2, .opGetPrototypeOfPolyProto
    return(t2)

.opGetPrototypeOfSlow:
    callSlowPath(_slow_path_get_prototype_of)
    dispatch()

.opGetPrototypeOfPolyProto:
    move knownPolyProtoOffset, t1
    loadInlineOffset(t1, t0, t3)
    return(t3)
end)


llintOpWithMetadata(op_put_by_id, OpPutById, macro (size, get, dispatch, metadata, return)
    get(m_base, t3)
    loadConstantOrVariableCell(size, t3, t0, .opPutByIdSlow)
    metadata(t5, t2)
    loadi OpPutById::Metadata::m_oldStructureID[t5], t2
    bineq t2, JSCell::m_structureID[t0], .opPutByIdSlow

    # At this point, we have:
    # t0 -> object base
    # t2 -> current structure ID
    # t5 -> metadata

    loadi OpPutById::Metadata::m_newStructureID[t5], t1
    btiz t1, .opPutByIdNotTransition

    # This is the transition case. t1 holds the new structureID. t2 holds the old structure ID.
    # If we have a chain, we need to check it. t0 is the base. We may clobber t1 to use it as
    # scratch.
    loadp OpPutById::Metadata::m_structureChain[t5], t3
    btpz t3, .opPutByIdTransitionDirect

    structureIDToStructureWithScratch(t2, t1)

    loadp StructureChain::m_vector[t3], t3
    assert(macro (ok) btpnz t3, ok end)

    loadq Structure::m_prototype[t2], t2
    bqeq t2, ValueNull, .opPutByIdTransitionChainDone
.opPutByIdTransitionChainLoop:
    loadi JSCell::m_structureID[t2], t2
    bineq t2, [t3], .opPutByIdSlow
    addp 4, t3
    structureIDToStructureWithScratch(t2, t1)
    loadq Structure::m_prototype[t2], t2
    bqneq t2, ValueNull, .opPutByIdTransitionChainLoop

.opPutByIdTransitionChainDone:
    # Reload the new structure, since we clobbered it above.
    loadi OpPutById::Metadata::m_newStructureID[t5], t1
    # Reload base into t0
    get(m_base, t3)
    loadConstantOrVariable(size, t3, t0)

.opPutByIdTransitionDirect:
    storei t1, JSCell::m_structureID[t0]
    writeBarrierOnOperandWithReload(size, get, m_base, macro ()
        # Reload metadata into t5
        metadata(t5, t1)
        # Reload base into t0
        get(m_base, t1)
        loadConstantOrVariable(size, t1, t0)
    end)

.opPutByIdNotTransition:
    # The only thing live right now is t0, which holds the base.
    get(m_value, t1)
    loadConstantOrVariable(size, t1, t2)
    loadi OpPutById::Metadata::m_offset[t5], t1
    storePropertyAtVariableOffset(t1, t0, t2)
    writeBarrierOnOperands(size, get, m_base, m_value)
    dispatch()

.opPutByIdSlow:
    callSlowPath(_llint_slow_path_put_by_id)
    dispatch()

.osrReturnPoint:
    getterSetterOSRExitReturnPoint(op_put_by_id, size)
    dispatch()

end)


llintOpWithMetadata(op_get_by_val, OpGetByVal, macro (size, get, dispatch, metadata, return)
    macro finishGetByVal(result, scratch)
        get(m_dst, scratch)
        storeq result, [cfr, scratch, 8]
        valueProfile(OpGetByVal, m_profile, t5, result)
        dispatch()
    end

    macro finishIntGetByVal(result, scratch)
        orq numberTag, result
        finishGetByVal(result, scratch)
    end

    macro finishDoubleGetByVal(result, scratch1, scratch2, unused)
        fd2q result, scratch1
        subq numberTag, scratch1
        finishGetByVal(scratch1, scratch2)
    end

    macro setLargeTypedArray()
        if LARGE_TYPED_ARRAYS
            storeb 1, OpGetByVal::Metadata::m_arrayProfile + ArrayProfile::m_mayBeLargeTypedArray[t5]
        else
            crash()
        end
    end

    metadata(t5, t2)

    get(m_base, t2)
    loadConstantOrVariableCell(size, t2, t0, .opGetByValSlow)

    move t0, t2
    arrayProfile(OpGetByVal::Metadata::m_arrayProfile, t2, t5, t1)

    get(m_property, t3)
    loadConstantOrVariableInt32(size, t3, t1, .opGetByValSlow)
    # This sign-extension makes the bounds-checking in getByValTypedArray work even on 4GB TypedArray.
    sxi2q t1, t1

    loadCagedJSValue(JSObject::m_butterfly[t0], t3, numberTag)
    move TagNumber, numberTag

    andi IndexingShapeMask, t2
    bieq t2, Int32Shape, .opGetByValIsContiguous
    bineq t2, ContiguousShape, .opGetByValNotContiguous

.opGetByValIsContiguous:
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValSlow
    get(m_dst, t0)
    loadq [t3, t1, 8], t2
    btqz t2, .opGetByValSlow
    jmp .opGetByValDone

.opGetByValNotContiguous:
    bineq t2, DoubleShape, .opGetByValNotDouble
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t3], .opGetByValSlow
    get(m_dst, t0)
    loadd [t3, t1, 8], ft0
    bdnequn ft0, ft0, .opGetByValSlow
    fd2q ft0, t2
    subq numberTag, t2
    jmp .opGetByValDone
    
.opGetByValNotDouble:
    subi ArrayStorageShape, t2
    bia t2, SlowPutArrayStorageShape - ArrayStorageShape, .opGetByValNotIndexedStorage
    biaeq t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t3], .opGetByValSlow
    get(m_dst, t0)
    loadq ArrayStorage::m_vector[t3, t1, 8], t2
    btqz t2, .opGetByValSlow

.opGetByValDone:
    storeq t2, [cfr, t0, 8]
    valueProfile(OpGetByVal, m_profile, t5, t2)
    dispatch()

.opGetByValNotIndexedStorage:
    getByValTypedArray(t0, t1, finishIntGetByVal, finishDoubleGetByVal, setLargeTypedArray, .opGetByValSlow)

.opGetByValSlow:
    callSlowPath(_llint_slow_path_get_by_val)
    dispatch()

.osrReturnPoint:
    getterSetterOSRExitReturnPoint(op_get_by_val, size)
    metadata(t5, t2)
    valueProfile(OpGetByVal, m_profile, t5, r0)
    return(r0)

end)

llintOpWithMetadata(op_get_private_name, OpGetPrivateName, macro (size, get, dispatch, metadata, return)
    metadata(t2, t0)

    # Slow path if the private field is stale
    get(m_property, t1)
    loadConstantOrVariable(size, t1, t0)
    loadp OpGetPrivateName::Metadata::m_property[t2], t1
    bpneq t1, t0, .opGetPrivateNameSlow

    get(m_base, t0)
    loadConstantOrVariableCell(size, t0, t3, .opGetPrivateNameSlow)
    loadi JSCell::m_structureID[t3], t1
    loadi OpGetPrivateName::Metadata::m_structureID[t2], t0
    bineq t0, t1, .opGetPrivateNameSlow

    loadi OpGetPrivateName::Metadata::m_offset[t2], t1
    loadPropertyAtVariableOffset(t1, t3, t0)
    valueProfile(OpGetPrivateName, m_profile, t2, t0)
    return(t0)

.opGetPrivateNameSlow:
    callSlowPath(_llint_slow_path_get_private_name)
    dispatch()
end)

llintOpWithMetadata(op_put_private_name, OpPutPrivateName, macro (size, get, dispatch, metadata, return)
    get(m_base, t3)
    loadConstantOrVariableCell(size, t3, t0, .opPutPrivateNameSlow)
    get(m_property, t3)
    loadConstantOrVariableCell(size, t3, t1, .opPutPrivateNameSlow)
    metadata(t5, t2)
    loadi OpPutPrivateName::Metadata::m_oldStructureID[t5], t2
    bineq t2, JSCell::m_structureID[t0], .opPutPrivateNameSlow

    loadp OpPutPrivateName::Metadata::m_property[t5], t3
    bpneq t3, t1, .opPutPrivateNameSlow

    # At this point, we have:
    # t0 -> object base
    # t2 -> current structure ID
    # t5 -> metadata

    loadi OpPutPrivateName::Metadata::m_newStructureID[t5], t1
    btiz t1, .opPutNotTransition

    storei t1, JSCell::m_structureID[t0]
    writeBarrierOnOperandWithReload(size, get, m_base, macro ()
        # Reload metadata into t5
        metadata(t5, t1)
        # Reload base into t0
        get(m_base, t1)
        loadConstantOrVariable(size, t1, t0)
    end)

.opPutNotTransition:
    # The only thing live right now is t0, which holds the base.
    get(m_value, t1)
    loadConstantOrVariable(size, t1, t2)
    loadi OpPutPrivateName::Metadata::m_offset[t5], t1
    storePropertyAtVariableOffset(t1, t0, t2)
    writeBarrierOnOperands(size, get, m_base, m_value)
    dispatch()

.opPutPrivateNameSlow:
    callSlowPath(_llint_slow_path_put_private_name)
    dispatch()
end)

llintOpWithMetadata(op_set_private_brand, OpSetPrivateBrand, macro (size, get, dispatch, metadata, return)
    get(m_base, t3)
    loadConstantOrVariableCell(size, t3, t0, .opSetPrivateBrandSlow)
    get(m_brand, t3)
    loadConstantOrVariableCell(size, t3, t1, .opSetPrivateBrandSlow)
    metadata(t5, t2)
    loadi OpSetPrivateBrand::Metadata::m_oldStructureID[t5], t2
    bineq t2, JSCell::m_structureID[t0], .opSetPrivateBrandSlow

    loadp OpSetPrivateBrand::Metadata::m_brand[t5], t3
    bpneq t3, t1, .opSetPrivateBrandSlow

    loadi OpSetPrivateBrand::Metadata::m_newStructureID[t5], t1
    storei t1, JSCell::m_structureID[t0]
    writeBarrierOnOperand(size, get, m_base)
    dispatch()

.opSetPrivateBrandSlow:
    callSlowPath(_llint_slow_path_set_private_brand)
    dispatch()
end)

llintOpWithMetadata(op_check_private_brand, OpCheckPrivateBrand, macro (size, get, dispatch, metadata, return)
    metadata(t5, t2)
    get(m_base, t3)
    loadConstantOrVariableCell(size, t3, t0, .opCheckPrivateBrandSlow)
    get(m_brand, t3)
    loadConstantOrVariableCell(size, t3, t1, .opCheckPrivateBrandSlow)

    loadp OpCheckPrivateBrand::Metadata::m_brand[t5], t3
    bqneq t3, t1, .opCheckPrivateBrandSlow

    loadi OpCheckPrivateBrand::Metadata::m_structureID[t5], t2
    bineq t2, JSCell::m_structureID[t0], .opCheckPrivateBrandSlow
    dispatch()

.opCheckPrivateBrandSlow:
    callSlowPath(_llint_slow_path_check_private_brand)
    dispatch()
end)

macro putByValOp(opcodeName, opcodeStruct, osrExitPoint)
    llintOpWithMetadata(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, metadata, return)
        macro contiguousPutByVal(storeCallback)
            biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], .outOfBounds
        .storeResult:
            get(m_value, t2)
            storeCallback(t2, t1, [t0, t3, 8])
            dispatch()

        .outOfBounds:
            biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
            storeb 1, %opcodeStruct%::Metadata::m_arrayProfile.m_mayStoreToHole[t5]
            addi 1, t3, t2
            storei t2, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
            jmp .storeResult
        end

        get(m_base, t0)
        loadConstantOrVariableCell(size, t0, t1, .opPutByValSlow)
        move t1, t2
        metadata(t5, t0)
        arrayProfile(%opcodeStruct%::Metadata::m_arrayProfile, t2, t5, t0)
        get(m_property, t0)
        loadConstantOrVariableInt32(size, t0, t3, .opPutByValSlow)
        sxi2q t3, t3
        loadCagedJSValue(JSObject::m_butterfly[t1], t0, numberTag)
        move TagNumber, numberTag
        btinz t2, CopyOnWrite, .opPutByValSlow
        andi IndexingShapeMask, t2
        bineq t2, Int32Shape, .opPutByValNotInt32
        contiguousPutByVal(
            macro (operand, scratch, address)
                loadConstantOrVariable(size, operand, scratch)
                bqb scratch, numberTag, .opPutByValSlow
                storeq scratch, address
                writeBarrierOnOperands(size, get, m_base, m_value)
            end)

    .opPutByValNotInt32:
        bineq t2, DoubleShape, .opPutByValNotDouble
        contiguousPutByVal(
            macro (operand, scratch, address)
                loadConstantOrVariable(size, operand, scratch)
                bqb scratch, numberTag, .notInt
                ci2ds scratch, ft0
                jmp .ready
            .notInt:
                addq numberTag, scratch
                fq2d scratch, ft0
                bdnequn ft0, ft0, .opPutByValSlow
            .ready:
                stored ft0, address
                writeBarrierOnOperands(size, get, m_base, m_value)
            end)

    .opPutByValNotDouble:
        bineq t2, ContiguousShape, .opPutByValNotContiguous
        contiguousPutByVal(
            macro (operand, scratch, address)
                loadConstantOrVariable(size, operand, scratch)
                storeq scratch, address
                writeBarrierOnOperands(size, get, m_base, m_value)
            end)

    .opPutByValNotContiguous:
        bineq t2, ArrayStorageShape, .opPutByValSlow
        biaeq t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.vectorLength[t0], .opPutByValOutOfBounds
        btqz ArrayStorage::m_vector[t0, t3, 8], .opPutByValArrayStorageEmpty
    .opPutByValArrayStorageStoreResult:
        get(m_value, t2)
        loadConstantOrVariable(size, t2, t1)
        storeq t1, ArrayStorage::m_vector[t0, t3, 8]
        writeBarrierOnOperands(size, get, m_base, m_value)
        dispatch()

    .opPutByValArrayStorageEmpty:
        storeb 1, %opcodeStruct%::Metadata::m_arrayProfile.m_mayStoreToHole[t5]
        addi 1, ArrayStorage::m_numValuesInVector[t0]
        bib t3, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0], .opPutByValArrayStorageStoreResult
        addi 1, t3, t1
        storei t1, -sizeof IndexingHeader + IndexingHeader::u.lengths.publicLength[t0]
        jmp .opPutByValArrayStorageStoreResult

    .opPutByValOutOfBounds:
        storeb 1, %opcodeStruct%::Metadata::m_arrayProfile.m_outOfBounds[t5]
    .opPutByValSlow:
        callSlowPath(_llint_slow_path_%opcodeName%)
        dispatch()

        osrExitPoint(size, dispatch)
        
    end)
end

putByValOp(put_by_val, OpPutByVal, macro (size, dispatch)
.osrReturnPoint:
    getterSetterOSRExitReturnPoint(op_put_by_val, size)
    dispatch()
end)

putByValOp(put_by_val_direct, OpPutByValDirect, macro (a, b) end)


macro llintJumpTrueOrFalseOp(opcodeName, opcodeStruct, miscConditionOp, truthyCellConditionOp)
    llintOpWithJump(op_%opcodeName%, opcodeStruct, macro (size, get, jump, dispatch)
        get(m_condition, t1)
        loadConstantOrVariable(size, t1, t0)
        btqnz t0, ~0xf, .maybeCell
        miscConditionOp(t0, .target)
        dispatch()

    .maybeCell:
        btqnz t0, notCellMask, .slow
        bbbeq JSCell::m_type[t0], constexpr JSType::LastMaybeFalsyCellPrimitive, .slow
        btbnz JSCell::m_flags[t0], constexpr MasqueradesAsUndefined, .slow
        truthyCellConditionOp(dispatch)

    .target:
        jump(m_targetLabel)

    .slow:
        callSlowPath(_llint_slow_path_%opcodeName%)
        nextInstruction()
    end)
end


macro equalNullJumpOp(opcodeName, opcodeStruct, cellHandler, immediateHandler)
    llintOpWithJump(op_%opcodeName%, opcodeStruct, macro (size, get, jump, dispatch)
        get(m_value, t0)
        assertNotConstant(size, t0)
        loadq [cfr, t0, 8], t0
        btqnz t0, notCellMask, .immediate
        loadStructureWithScratch(t0, t2, t1)
        cellHandler(t2, JSCell::m_flags[t0], .target)
        dispatch()

    .target:
        jump(m_targetLabel)

    .immediate:
        andq ~TagUndefined, t0
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

macro undefinedOrNullJumpOp(opcodeName, opcodeStruct, fn)
    llintOpWithJump(op_%opcodeName%, opcodeStruct, macro (size, get, jump, dispatch)
        get(m_value, t1)
        loadConstantOrVariable(size, t1, t0)
        andq ~TagUndefined, t0
        fn(t0, .target)
        dispatch()

    .target:
        jump(m_targetLabel)
    end)
end

undefinedOrNullJumpOp(jundefined_or_null, OpJundefinedOrNull,
    macro (value, target) bqeq value, ValueNull, target end)

undefinedOrNullJumpOp(jnundefined_or_null, OpJnundefinedOrNull,
    macro (value, target) bqneq value, ValueNull, target end)

llintOpWithReturn(op_jeq_ptr, OpJeqPtr, macro (size, get, dispatch, return)
    get(m_value, t0)
    get(m_specialPointer, t1)
    loadConstant(size, t1, t2)
    bpeq t2, [cfr, t0, 8], .opJeqPtrTarget
    dispatch()

.opJeqPtrTarget:
    get(m_targetLabel, t0)
    jumpImpl(dispatchIndirect, t0)
end)


llintOpWithMetadata(op_jneq_ptr, OpJneqPtr, macro (size, get, dispatch, metadata, return)
    get(m_value, t0)
    get(m_specialPointer, t1)
    loadConstant(size, t1, t2)
    bpneq t2, [cfr, t0, 8], .opJneqPtrTarget
    dispatch()

.opJneqPtrTarget:
    metadata(t5, t0)
    storeb 1, OpJneqPtr::Metadata::m_hasJumped[t5]
    get(m_targetLabel, t0)
    jumpImpl(dispatchIndirect, t0)
end)


macro compareJumpOp(opcodeName, opcodeStruct, integerCompare, doubleCompare)
    llintOpWithJump(op_%opcodeName%, opcodeStruct, macro (size, get, jump, dispatch)
        get(m_lhs, t2)
        get(m_rhs, t3)
        loadConstantOrVariable(size, t2, t0)
        loadConstantOrVariable(size, t3, t1)
        bqb t0, numberTag, .op1NotInt
        bqb t1, numberTag, .op2NotInt
        integerCompare(t0, t1, .jumpTarget)
        dispatch()

    .op1NotInt:
        btqz t0, numberTag, .slow
        bqb t1, numberTag, .op1NotIntOp2NotInt
        ci2ds t1, ft1
        jmp .op1NotIntReady
    .op1NotIntOp2NotInt:
        btqz t1, numberTag, .slow
        addq numberTag, t1
        fq2d t1, ft1
    .op1NotIntReady:
        addq numberTag, t0
        fq2d t0, ft0
        doubleCompare(ft0, ft1, .jumpTarget)
        dispatch()

    .op2NotInt:
        ci2ds t0, ft0
        btqz t1, numberTag, .slow
        addq numberTag, t1
        fq2d t1, ft1
        doubleCompare(ft0, ft1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(m_targetLabel)

    .slow:
        callSlowPath(_llint_slow_path_%opcodeName%)
        nextInstruction()
    end)
end


macro equalityJumpOp(opcodeName, opcodeStruct, integerComparison)
    llintOpWithJump(op_%opcodeName%, opcodeStruct, macro (size, get, jump, dispatch)
        get(m_lhs, t2)
        get(m_rhs, t3)
        loadConstantOrVariableInt32(size, t2, t0, .slow)
        loadConstantOrVariableInt32(size, t3, t1, .slow)
        integerComparison(t0, t1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(m_targetLabel)

    .slow:
        callSlowPath(_llint_slow_path_%opcodeName%)
        nextInstruction()
    end)
end


macro compareUnsignedJumpOp(opcodeName, opcodeStruct, integerCompareMacro)
    llintOpWithJump(op_%opcodeName%, opcodeStruct, macro (size, get, jump, dispatch)
        get(m_lhs, t2)
        get(m_rhs, t3)
        loadConstantOrVariable(size, t2, t0)
        loadConstantOrVariable(size, t3, t1)
        integerCompareMacro(t0, t1, .jumpTarget)
        dispatch()

    .jumpTarget:
        jump(m_targetLabel)
    end)
end


macro compareOp(opcodeName, opcodeStruct, integerCompareAndSet, doubleCompareAndSet)
    llintOpWithReturn(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, return)
        get(m_lhs, t2)
        get(m_rhs, t0)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        bqb t0, numberTag, .op1NotInt
        bqb t1, numberTag, .op2NotInt
        integerCompareAndSet(t0, t1, t0)
        orq ValueFalse, t0
        return(t0)

    .op1NotInt:
        btqz t0, numberTag, .slow
        bqb t1, numberTag, .op1NotIntOp2NotInt
        ci2ds t1, ft1
        jmp .op1NotIntReady

    .op1NotIntOp2NotInt:
        btqz t1, numberTag, .slow
        addq numberTag, t1
        fq2d t1, ft1

    .op1NotIntReady:
        addq numberTag, t0
        fq2d t0, ft0
        doubleCompareAndSet(ft0, ft1, t0)
        orq ValueFalse, t0
        return(t0)

    .op2NotInt:
        ci2ds t0, ft0
        btqz t1, numberTag, .slow
        addq numberTag, t1
        fq2d t1, ft1
        doubleCompareAndSet(ft0, ft1, t0)
        orq ValueFalse, t0
        return(t0)

    .slow:
        callSlowPath(_llint_slow_path_%opcodeName%)
        dispatch()
    end)
end


macro compareUnsignedOp(opcodeName, opcodeStruct, integerCompareAndSet)
    llintOpWithReturn(op_%opcodeName%, opcodeStruct, macro (size, get, dispatch, return)
        get(m_lhs, t2)
        get(m_rhs,  t0)
        loadConstantOrVariable(size, t0, t1)
        loadConstantOrVariable(size, t2, t0)
        integerCompareAndSet(t0, t1, t0)
        orq ValueFalse, t0
        return(t0)
    end)
end


llintOpWithJump(op_switch_imm, OpSwitchImm, macro (size, get, jump, dispatch)
    get(m_scrutinee, t2)
    getu(size, OpSwitchImm, m_tableIndex, t3)
    loadConstantOrVariable(size, t2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_unlinkedCode[t2], t2
    loadp UnlinkedCodeBlock::m_rareData[t2], t2
    muli sizeof UnlinkedSimpleJumpTable, t3
    loadp UnlinkedCodeBlock::RareData::m_unlinkedSwitchJumpTables + UnlinkedSimpleJumpTableFixedVector::m_storage[t2], t2
    addp (constexpr (UnlinkedSimpleJumpTableFixedVector::Storage::offsetOfData())), t2
    addp t3, t2
    bqb t1, numberTag, .opSwitchImmNotInt
    subi UnlinkedSimpleJumpTable::m_min[t2], t1
    loadp UnlinkedSimpleJumpTable::m_branchOffsets + Int32FixedVector::m_storage[t2], t2
    btpz t2, .opSwitchImmFallThrough
    biaeq t1, Int32FixedVector::Storage::m_size[t2], .opSwitchImmFallThrough
    loadis (constexpr (Int32FixedVector::Storage::offsetOfData()))[t2, t1, 4], t1
    btiz t1, .opSwitchImmFallThrough
    dispatchIndirect(t1)

.opSwitchImmNotInt:
    btqnz t1, numberTag, .opSwitchImmSlow   # Go slow if it's a double.
.opSwitchImmFallThrough:
    jump(m_defaultOffset)

.opSwitchImmSlow:
    callSlowPath(_llint_slow_path_switch_imm)
    nextInstruction()
end)


llintOpWithJump(op_switch_char, OpSwitchChar, macro (size, get, jump, dispatch)
    get(m_scrutinee, t2)
    getu(size, OpSwitchChar, m_tableIndex, t3)
    loadConstantOrVariable(size, t2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_unlinkedCode[t2], t2
    loadp UnlinkedCodeBlock::m_rareData[t2], t2
    muli sizeof UnlinkedSimpleJumpTable, t3
    loadp UnlinkedCodeBlock::RareData::m_unlinkedSwitchJumpTables + UnlinkedSimpleJumpTableFixedVector::m_storage[t2], t2
    addp (constexpr (UnlinkedSimpleJumpTableFixedVector::Storage::offsetOfData())), t2
    addp t3, t2
    btqnz t1, notCellMask, .opSwitchCharFallThrough
    bbneq JSCell::m_type[t1], StringType, .opSwitchCharFallThrough
    loadp JSString::m_fiber[t1], t0
    btpnz t0, isRopeInPointer, .opSwitchOnRope
    bineq StringImpl::m_length[t0], 1, .opSwitchCharFallThrough
    loadp StringImpl::m_data8[t0], t1
    btinz StringImpl::m_hashAndFlags[t0], HashFlags8BitBuffer, .opSwitchChar8Bit
    loadh [t1], t0
    jmp .opSwitchCharReady
.opSwitchChar8Bit:
    loadb [t1], t0
.opSwitchCharReady:
    subi UnlinkedSimpleJumpTable::m_min[t2], t0
    loadp UnlinkedSimpleJumpTable::m_branchOffsets + Int32FixedVector::m_storage[t2], t2
    btpz t2, .opSwitchCharFallThrough
    biaeq t0, Int32FixedVector::Storage::m_size[t2], .opSwitchCharFallThrough
    loadis (constexpr (Int32FixedVector::Storage::offsetOfData()))[t2, t0, 4], t1
    btiz t1, .opSwitchCharFallThrough
    dispatchIndirect(t1)

.opSwitchCharFallThrough:
    jump(m_defaultOffset)

.opSwitchOnRope:
    bineq JSRopeString::m_compactFibers + JSRopeString::CompactFibers::m_length[t1], 1, .opSwitchCharFallThrough

.opSwitchOnRopeChar:
    callSlowPath(_llint_slow_path_switch_char)
    nextInstruction()
end)


# we assume t5 contains the metadata, and we should not scratch that
macro arrayProfileForCall(opcodeStruct, getu)
    getu(m_argv, t3)
    negp t3
    loadq ThisArgumentOffset[cfr, t3, 8], t0
    btqnz t0, notCellMask, .done
    loadi JSCell::m_structureID[t0], t3
    storei t3, %opcodeStruct%::Metadata::m_arrayProfile.m_lastSeenStructureID[t5]
.done:
end

# t5 holds metadata.
macro callHelper(opcodeName, slowPath, opcodeStruct, valueProfileName, dstVirtualRegister, prepareCall, invokeCall, preparePolymorphic, prepareSlowCall, size, dispatch, metadata, getCallee, getArgumentStart, getArgumentCountIncludingThis)
    getCallee(t1)

    loadConstantOrVariable(size, t1, t0)

    # Aligned to JIT::compileSetupFrame
    getArgumentStart(t3)
    lshifti 3, t3
    negp t3
    addp cfr, t3
    getArgumentCountIncludingThis(t2)
    storei t2, ArgumentCountIncludingThis + PayloadOffset[t3]

    # Store location bits and |callee|, and configure sp.
    storePC()
    storeq t0, Callee[t3]
    move t3, sp
    addp CallerFrameAndPCSize, sp

    loadp %opcodeStruct%::Metadata::m_callLinkInfo.m_calleeOrCodeBlock[t5], t1
    btpz t1, (constexpr CallLinkInfo::polymorphicCalleeMask), .notPolymorphic
    # prepareCall in LLInt does not untag return address. So we need to untag that in the trampoline separately.
    # But we should not untag that for polymorphic call case since it should be done in the polymorphic thunk side.
    preparePolymorphic(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, JSEntryPtrTag)
    jmp .goPolymorphic

.notPolymorphic:
    bqneq t0, t1, .opCallSlow
    prepareCall(t2, t3, t4, t1, macro(address)
        loadp %opcodeStruct%::Metadata::m_callLinkInfo.u.dataIC.m_codeBlock[t5], t2
        storep t2, address
    end)

.goPolymorphic:
    loadp %opcodeStruct%::Metadata::m_callLinkInfo.u.dataIC.m_monomorphicCallDestination[t5], t5
    invokeCall(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, t5, t1, JSEntryPtrTag)

.opCallSlow:
    # 64bit:t0 32bit(t0,t1) is callee
    # t2 is CallLinkInfo*
    # t3 is caller's JSGlobalObject
    addp %opcodeStruct%::Metadata::m_callLinkInfo, t5, t2 # CallLinkInfo* in t2
    loadp %opcodeStruct%::Metadata::m_callLinkInfo.m_slowPathCallDestination[t5], t5
    loadp CodeBlock[cfr], t3
    loadp CodeBlock::m_globalObject[t3], t3
    prepareSlowCall()
    callTargetFunction(%opcodeName%_slow, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, t5, JSEntryPtrTag)
end

macro commonCallOp(opcodeName, slowPath, opcodeStruct, prepareCall, invokeCall, preparePolymorphic, prepareSlowCall, prologue)
    llintOpWithMetadata(opcodeName, opcodeStruct, macro (size, get, dispatch, metadata, return)
        metadata(t5, t0)

        prologue(macro (fieldName, dst)
            getu(size, opcodeStruct, fieldName, dst)
        end, metadata)

        macro getCallee(dst)
            get(m_callee, dst)
        end

        macro getArgumentStart(dst)
            getu(size, opcodeStruct, m_argv, dst)
        end

        macro getArgumentCount(dst)
            getu(size, opcodeStruct, m_argc, dst)
        end

        # t5 holds metadata
        callHelper(opcodeName, slowPath, opcodeStruct, m_profile, m_dst, prepareCall, invokeCall, preparePolymorphic, prepareSlowCall, size, dispatch, metadata, getCallee, getArgumentStart, getArgumentCount)
    end)
end

macro doCallVarargs(opcodeName, size, get, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, metadata, frameSlowPath, slowPath, prepareCall, invokeCall, preparePolymorphic, prepareSlowCall)
    callSlowPath(frameSlowPath)
    branchIfException(_llint_throw_from_slow_path_trampoline)
    # calleeFrame in r1
    if JSVALUE64
        move r1, sp
    else
        # The calleeFrame is not stack aligned, move down by CallerFrameAndPCSize to align
        if ARMv7
            subp r1, CallerFrameAndPCSize, t2
            move t2, sp
        else
            subp r1, CallerFrameAndPCSize, sp
        end
    end
    callCallSlowPath(
        slowPath,
        # Those parameters are r0 and r1
        macro (restoredPCOrThrow, calleeFramePtr)
            btpz calleeFramePtr, .dontUpdateSP
            restoreStateAfterCCall()
            move calleeFramePtr, sp
            get(m_callee, t1)
            loadConstantOrVariable(size, t1, t0)
            metadata(t5, t2)

            loadp %opcodeStruct%::Metadata::m_callLinkInfo.m_calleeOrCodeBlock[t5], t1
            btpz t1, (constexpr CallLinkInfo::polymorphicCalleeMask), .notPolymorphic
            # prepareCall in LLInt does not untag return address. So we need to untag that in the trampoline separately.
            # But we should not untag that for polymorphic call case since it should be done in the polymorphic thunk side.
            preparePolymorphic(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, JSEntryPtrTag)
            jmp .goPolymorphic

        .notPolymorphic:
            bqneq t0, t1, .opCallSlow
            prepareCall(t2, t3, t4, t1, macro(address)
                loadp %opcodeStruct%::Metadata::m_callLinkInfo.u.dataIC.m_codeBlock[t5], t2
                storep t2, address
            end)

        .goPolymorphic:
            loadp %opcodeStruct%::Metadata::m_callLinkInfo.u.dataIC.m_monomorphicCallDestination[t5], t5
            invokeCall(opcodeName, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, t5, t1, JSEntryPtrTag)

        .opCallSlow:
            # 64bit:t0 32bit(t0,t1) is callee
            # t2 is CallLinkInfo*
            # t3 is caller's JSGlobalObject
            addp %opcodeStruct%::Metadata::m_callLinkInfo, t5, t2 # CallLinkInfo* in t2
            loadp %opcodeStruct%::Metadata::m_callLinkInfo.m_slowPathCallDestination[t5], t5
            loadp CodeBlock[cfr], t3
            loadp CodeBlock::m_globalObject[t3], t3
            prepareSlowCall()
            callTargetFunction(%opcodeName%_slow, size, opcodeStruct, valueProfileName, dstVirtualRegister, dispatch, t5, JSEntryPtrTag)
        .dontUpdateSP:
            jmp _llint_throw_from_slow_path_trampoline
        end)
end

llintOp(op_ret, OpRet, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    get(m_value, t2)
    loadConstantOrVariable(size, t2, r0)
    doReturn()
end)


llintOpWithReturn(op_to_primitive, OpToPrimitive, macro (size, get, dispatch, return)
    get(m_src, t2)
    loadConstantOrVariable(size, t2, t0)
    btqnz t0, notCellMask, .opToPrimitiveIsImm
    bbaeq JSCell::m_type[t0], ObjectType, .opToPrimitiveSlowCase
.opToPrimitiveIsImm:
    return(t0)

.opToPrimitiveSlowCase:
    callSlowPath(_slow_path_to_primitive)
    dispatch()
end)


llintOpWithReturn(op_to_property_key, OpToPropertyKey, macro (size, get, dispatch, return)
    get(m_src, t2)
    loadConstantOrVariable(size, t2, t0)

    btqnz t0, notCellMask, .opToPropertyKeySlow
    bbeq JSCell::m_type[t0], SymbolType, .done
    bbneq JSCell::m_type[t0], StringType, .opToPropertyKeySlow

.done:
    return(t0)

.opToPropertyKeySlow:
    callSlowPath(_slow_path_to_property_key)
    dispatch()
end)


commonOp(llint_op_catch, macro () end, macro (size)
    # This is where we end up from the JIT's throw trampoline (because the
    # machine code return address will be set to _llint_op_catch), and from
    # the interpreter's throw trampoline (see _llint_throw_trampoline).
    # The throwing code must have known that we were throwing to the interpreter,
    # and have set VM::targetInterpreterPCForThrow.
    loadp Callee[cfr], t3
    convertCalleeToVM(t3)
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)
    loadp VM::callFrameForCatch[t3], cfr
    storep 0, VM::callFrameForCatch[t3]
    restoreStackPointerAfterCall()

    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_metadata[PB], metadataTable
    loadp CodeBlock::m_instructionsRawPointer[PB], PB
    loadp VM::targetInterpreterPCForThrow[t3], PC
    subp PB, PC

    callSlowPath(_llint_slow_path_retrieve_and_clear_exception_if_catchable)
    bpneq r1, 0, .isCatchableException
    jmp _llint_throw_from_slow_path_trampoline

.isCatchableException:
    move r1, t0
    get(size, OpCatch, m_exception, t2)
    storeq t0, [cfr, t2, 8]

    loadq Exception::m_value[t0], t3
    get(size, OpCatch, m_thrownValue, t2)
    storeq t3, [cfr, t2, 8]

    traceExecution()

    callSlowPath(_llint_slow_path_profile_catch)

    dispatchOp(size, op_catch)
end)


llintOp(op_end, OpEnd, macro (size, get, dispatch)
    checkSwitchToJITForEpilogue()
    get(m_value, t0)
    assertNotConstant(size, t0)
    loadq [cfr, t0, 8], r0
    doReturn()
end)


op(llint_throw_from_slow_path_trampoline, macro ()
    loadp Callee[cfr], t1
    convertCalleeToVM(t1)
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(t1, t2)

    callSlowPath(_llint_slow_path_handle_exception)

    # When throwing from the interpreter (i.e. throwing from LLIntSlowPaths), so
    # the throw target is not necessarily interpreted code, we come to here.
    # This essentially emulates the JIT's throwing protocol.
    loadp Callee[cfr], t1
    convertCalleeToVM(t1)
    if ARM64E
        loadp VM::targetMachinePCForThrow[t1], a0
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::exceptionHandler) * PtrSize, a1
        jmp [a1], NativeToJITGatePtrTag # ExceptionHandlerPtrTag
    else
        jmp VM::targetMachinePCForThrow[t1], ExceptionHandlerPtrTag
    end
end)


op(llint_throw_during_call_trampoline, macro ()
    preserveReturnAddressAfterCall(t2)
    jmp _llint_throw_from_slow_path_trampoline
end)


macro nativeCallTrampoline(executableOffsetToFunction)
    functionPrologue()
    storep 0, CodeBlock[cfr]
    loadp Callee[cfr], a0
    loadp JSFunction::m_executableOrRareData[a0], a2
    btpz a2, (constexpr JSFunction::rareDataTag), .isExecutable
    loadp (FunctionRareData::m_executable - (constexpr JSFunction::rareDataTag))[a2], a2
.isExecutable:
    loadp JSFunction::m_scope[a0], a0
    loadp JSGlobalObject::m_vm[a0], a1
    storep cfr, VM::topCallFrame[a1]
    if ARM64 or ARM64E or C_LOOP or C_LOOP_WIN
        storep lr, ReturnPC[cfr]
    end
    move cfr, a1
    checkStackPointerAlignment(t3, 0xdead0001)
    if C_LOOP or C_LOOP_WIN
        cloopCallNative executableOffsetToFunction[a2]
    else
        if X86_64_WIN
            subp 32, sp
            call executableOffsetToFunction[a2], HostFunctionPtrTag
            addp 32, sp
        else
            call executableOffsetToFunction[a2], HostFunctionPtrTag
        end
    end

    loadp Callee[cfr], t3
    loadp JSFunction::m_scope[t3], t3
    loadp JSGlobalObject::m_vm[t3], t3

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
    loadp Callee[cfr], a2
    loadp InternalFunction::m_globalObject[a2], a0
    loadp JSGlobalObject::m_vm[a0], a1
    storep cfr, VM::topCallFrame[a1]
    if ARM64 or ARM64E or C_LOOP or C_LOOP_WIN
        storep lr, ReturnPC[cfr]
    end
    move cfr, a1
    checkStackPointerAlignment(t3, 0xdead0001)
    if C_LOOP or C_LOOP_WIN
        cloopCallNative offsetOfFunction[a2]
    else
        if X86_64_WIN
            subp 32, sp
            call offsetOfFunction[a2], HostFunctionPtrTag
            addp 32, sp
        else
            call offsetOfFunction[a2], HostFunctionPtrTag
        end
    end

    loadp Callee[cfr], t3
    loadp InternalFunction::m_globalObject[t3], t3
    loadp JSGlobalObject::m_vm[t3], t3

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
    loadp JSGlobalObject::m_varInjectionWatchpointSet[scratch], scratch
    bbeq WatchpointSet::m_state[scratch], IsInvalidated, slowPath
end

llintOpWithMetadata(op_resolve_scope, OpResolveScope, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)

    macro getConstantScope(dst)
        loadp OpResolveScope::Metadata::m_constantScope[t5], dst
    end

    macro returnConstantScope()
        getConstantScope(t0)
        return(t0)
    end

    macro globalLexicalBindingEpochCheck(slowPath, globalObject, scratch)
        loadi OpResolveScope::Metadata::m_globalLexicalBindingEpoch[t5], scratch
        bineq JSGlobalObject::m_globalLexicalBindingEpoch[globalObject], scratch, slowPath
    end

    macro resolveScope()
        loadi OpResolveScope::Metadata::m_localScopeDepth[t5], t2
        get(m_scope, t0)
        loadq [cfr, t0, 8], t0
        btiz t2, .resolveScopeLoopEnd

    .resolveScopeLoop:
        loadp JSScope::m_next[t0], t0
        subi 1, t2
        btinz t2, .resolveScopeLoop

    .resolveScopeLoopEnd:
        return(t0)
    end

    loadi OpResolveScope::Metadata::m_resolveType[t5], t0

#rGlobalProperty:
    bineq t0, GlobalProperty, .rGlobalVar
    getConstantScope(t0)
    globalLexicalBindingEpochCheck(.rDynamic, t0, t2)
    return(t0)

.rGlobalVar:
    bineq t0, GlobalVar, .rGlobalLexicalVar
    returnConstantScope()

.rGlobalLexicalVar:
    bineq t0, GlobalLexicalVar, .rClosureVar
    returnConstantScope()

.rClosureVar:
    bineq t0, ClosureVar, .rModuleVar
    resolveScope()

.rModuleVar:
    bineq t0, ModuleVar, .rGlobalPropertyWithVarInjectionChecks
    returnConstantScope()

.rGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .rGlobalVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic, t2)
    getConstantScope(t0)
    globalLexicalBindingEpochCheck(.rDynamic, t0, t2)
    return(t0)

.rGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .rGlobalLexicalVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic, t2)
    returnConstantScope()

.rGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .rClosureVarWithVarInjectionChecks
    varInjectionCheck(.rDynamic, t2)
    returnConstantScope()

.rClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .rDynamic
    varInjectionCheck(.rDynamic, t2)
    resolveScope()

.rDynamic:
    callSlowPath(_slow_path_resolve_scope)
    dispatch()
end)


macro loadWithStructureCheck(opcodeStruct, get, slowPath)
    get(m_scope, t0)
    loadq [cfr, t0, 8], t0
    loadStructureWithScratch(t0, t2, t1)
    loadp %opcodeStruct%::Metadata::m_structure[t5], t1
    bpneq t2, t1, slowPath
end

llintOpWithMetadata(op_get_from_scope, OpGetFromScope, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)

    macro getProperty()
        loadp OpGetFromScope::Metadata::m_operand[t5], t1
        loadPropertyAtVariableOffset(t1, t0, t2)
        valueProfile(OpGetFromScope, m_profile, t5, t2)
        return(t2)
    end

    macro getGlobalVar(tdzCheckIfNecessary)
        loadp OpGetFromScope::Metadata::m_operand[t5], t0
        loadq [t0], t0
        tdzCheckIfNecessary(t0)
        valueProfile(OpGetFromScope, m_profile, t5, t0)
        return(t0)
    end

    macro getClosureVar()
        loadp OpGetFromScope::Metadata::m_operand[t5], t1
        loadq JSLexicalEnvironment_variables[t0, t1, 8], t0
        valueProfile(OpGetFromScope, m_profile, t5, t0)
        return(t0)
    end

    loadi OpGetFromScope::Metadata::m_getPutInfo + GetPutInfo::m_operand[t5], t0
    andi ResolveTypeMask, t0

#gGlobalProperty:
    bineq t0, GlobalProperty, .gGlobalVar
    loadWithStructureCheck(OpGetFromScope, get, .gDynamic) # This structure check includes lexical binding epoch check since when the epoch is changed, scope will be changed too.
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
    loadVariable(get, m_scope, t0)
    getClosureVar()

.gGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .gGlobalVarWithVarInjectionChecks
    loadWithStructureCheck(OpGetFromScope, get, .gDynamic) # This structure check includes lexical binding epoch check since when the epoch is changed, scope will be changed too.
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
    loadVariable(get, m_scope, t0)
    getClosureVar()

.gDynamic:
    callSlowPath(_llint_slow_path_get_from_scope)
    dispatch()
end)


llintOpWithMetadata(op_put_to_scope, OpPutToScope, macro (size, get, dispatch, metadata, return)
    macro putProperty()
        get(m_value, t1)
        loadConstantOrVariable(size, t1, t2)
        loadp OpPutToScope::Metadata::m_operand[t5], t1
        storePropertyAtVariableOffset(t1, t0, t2)
    end

    macro putGlobalVariable()
        get(m_value, t0)
        loadConstantOrVariable(size, t0, t1)
        loadp OpPutToScope::Metadata::m_watchpointSet[t5], t2
        btpz t2, .noVariableWatchpointSet
        notifyWrite(t2, .pDynamic)
    .noVariableWatchpointSet:
        loadp OpPutToScope::Metadata::m_operand[t5], t0
        storeq t1, [t0]
    end

    macro putClosureVar()
        get(m_value, t1)
        loadConstantOrVariable(size, t1, t2)
        loadp OpPutToScope::Metadata::m_operand[t5], t1
        storeq t2, JSLexicalEnvironment_variables[t0, t1, 8]
    end

    macro putResolvedClosureVar()
        get(m_value, t1)
        loadConstantOrVariable(size, t1, t2)
        loadp OpPutToScope::Metadata::m_watchpointSet[t5], t3
        btpz t3, .noVariableWatchpointSet
        notifyWrite(t3, .pDynamic)
    .noVariableWatchpointSet:
        loadp OpPutToScope::Metadata::m_operand[t5], t1
        storeq t2, JSLexicalEnvironment_variables[t0, t1, 8]
    end

    macro checkTDZInGlobalPutToScopeIfNecessary()
        loadi OpPutToScope::Metadata::m_getPutInfo + GetPutInfo::m_operand[t5], t0
        andi InitializationModeMask, t0
        rshifti InitializationModeShift, t0
        bineq t0, NotInitialization, .noNeedForTDZCheck
        loadp OpPutToScope::Metadata::m_operand[t5], t0
        loadq [t0], t0
        bqeq t0, ValueEmpty, .pDynamic
    .noNeedForTDZCheck:
    end

    metadata(t5, t0)
    loadi OpPutToScope::Metadata::m_getPutInfo + GetPutInfo::m_operand[t5], t0
    andi ResolveTypeMask, t0

#pResolvedClosureVar:
    bineq t0, ResolvedClosureVar, .pGlobalProperty
    loadVariable(get, m_scope, t0)
    putResolvedClosureVar()
    writeBarrierOnOperands(size, get, m_scope, m_value)
    dispatch()

.pGlobalProperty:
    bineq t0, GlobalProperty, .pGlobalVar
    loadWithStructureCheck(OpPutToScope, get, .pDynamic) # This structure check includes lexical binding epoch check since when the epoch is changed, scope will be changed too.
    putProperty()
    writeBarrierOnOperands(size, get, m_scope, m_value)
    dispatch()

.pGlobalVar:
    bineq t0, GlobalVar, .pGlobalLexicalVar
    varReadOnlyCheck(.pDynamic, t2)
    putGlobalVariable()
    writeBarrierOnGlobalObject(size, get, m_value)
    dispatch()

.pGlobalLexicalVar:
    bineq t0, GlobalLexicalVar, .pClosureVar
    checkTDZInGlobalPutToScopeIfNecessary()
    putGlobalVariable()
    writeBarrierOnGlobalLexicalEnvironment(size, get, m_value)
    dispatch()

.pClosureVar:
    bineq t0, ClosureVar, .pGlobalPropertyWithVarInjectionChecks
    loadVariable(get, m_scope, t0)
    putClosureVar()
    writeBarrierOnOperands(size, get, m_scope, m_value)
    dispatch()

.pGlobalPropertyWithVarInjectionChecks:
    bineq t0, GlobalPropertyWithVarInjectionChecks, .pGlobalVarWithVarInjectionChecks
    loadWithStructureCheck(OpPutToScope, get, .pDynamic) # This structure check includes lexical binding epoch check since when the epoch is changed, scope will be changed too.
    putProperty()
    writeBarrierOnOperands(size, get, m_scope, m_value)
    dispatch()

.pGlobalVarWithVarInjectionChecks:
    bineq t0, GlobalVarWithVarInjectionChecks, .pGlobalLexicalVarWithVarInjectionChecks
    # FIXME: Avoid loading m_globalObject twice
    # https://bugs.webkit.org/show_bug.cgi?id=223097
    varInjectionCheck(.pDynamic, t2)
    varReadOnlyCheck(.pDynamic, t2)
    putGlobalVariable()
    writeBarrierOnGlobalObject(size, get, m_value)
    dispatch()

.pGlobalLexicalVarWithVarInjectionChecks:
    bineq t0, GlobalLexicalVarWithVarInjectionChecks, .pClosureVarWithVarInjectionChecks
    varInjectionCheck(.pDynamic, t2)
    checkTDZInGlobalPutToScopeIfNecessary()
    putGlobalVariable()
    writeBarrierOnGlobalLexicalEnvironment(size, get, m_value)
    dispatch()

.pClosureVarWithVarInjectionChecks:
    bineq t0, ClosureVarWithVarInjectionChecks, .pModuleVar
    varInjectionCheck(.pDynamic, t2)
    loadVariable(get, m_scope, t0)
    putClosureVar()
    writeBarrierOnOperands(size, get, m_scope, m_value)
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
    loadVariable(get, m_arguments, t0)
    getu(size, OpGetFromArguments, m_index, t1)
    loadq DirectArguments_storage[t0, t1, 8], t0
    return(t0)
end)


llintOp(op_put_to_arguments, OpPutToArguments, macro (size, get, dispatch)
    loadVariable(get, m_arguments, t0)
    getu(size, OpPutToArguments, m_index, t1)
    get(m_value, t3)
    loadConstantOrVariable(size, t3, t2)
    storeq t2, DirectArguments_storage[t0, t1, 8]
    writeBarrierOnOperands(size, get, m_arguments, m_value)
    dispatch()
end)


llintOpWithReturn(op_get_parent_scope, OpGetParentScope, macro (size, get, dispatch, return)
    loadVariable(get, m_scope, t0)
    loadp JSScope::m_next[t0], t0
    return(t0)
end)


llintOpWithMetadata(op_profile_type, OpProfileType, macro (size, get, dispatch, metadata, return)
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_vm[t1], t1
    # t1 is holding the pointer to the typeProfilerLog.
    loadp VM::m_typeProfilerLog[t1], t1
    # t2 is holding the pointer to the current log entry.
    loadp TypeProfilerLog::m_currentLogEntryPtr[t1], t2

    # t0 is holding the JSValue argument.
    get(m_targetVirtualRegister, t3)
    loadConstantOrVariable(size, t3, t0)

    bqeq t0, ValueEmpty, .opProfileTypeDone
    # Store the JSValue onto the log entry.
    storeq t0, TypeProfilerLog::LogEntry::value[t2]
    
    # Store the TypeLocation onto the log entry.
    metadata(t5, t3)
    loadp OpProfileType::Metadata::m_typeLocation[t5], t3
    storep t3, TypeProfilerLog::LogEntry::location[t2]

    btqz t0, notCellMask, .opProfileTypeIsCell
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
    loadp OpProfileControlFlow::Metadata::m_basicBlockLocation[t5], t0
    addq 1, BasicBlockLocation::m_executionCount[t0]
    dispatch()
end)

llintOpWithReturn(op_get_rest_length, OpGetRestLength, macro (size, get, dispatch, return)
    loadi PayloadOffset + ArgumentCountIncludingThis[cfr], t0
    subi 1, t0
    getu(size, OpGetRestLength, m_numParametersToSkip, t1)
    bilteq t0, t1, .storeZero
    subi t1, t0
    jmp .boxUp
.storeZero:
    move 0, t0
.boxUp:
    orq numberTag, t0
    return(t0)
end)


llintOpWithMetadata(op_iterator_open, OpIteratorOpen, macro (size, get, dispatch, metadata, return)
    macro fastNarrow()
        callSlowPath(_iterator_open_try_fast_narrow)
    end
    macro fastWide16()
        callSlowPath(_iterator_open_try_fast_wide16)
    end
    macro fastWide32()
        callSlowPath(_iterator_open_try_fast_wide32)
    end
    size(fastNarrow, fastWide16, fastWide32, macro (callOp) callOp() end)

    # FIXME: We should do this with inline assembly since it's the "fast" case.
    bbeq r1, constexpr IterationMode::Generic, .iteratorOpenGeneric
    dispatch()

.iteratorOpenGeneric:
    macro gotoGetByIdCheckpoint()
        jmp .getByIdStart
    end

    macro getCallee(dst)
        get(m_symbolIterator, dst)
    end

    macro getArgumentIncludingThisStart(dst)
        getu(size, OpIteratorOpen, m_stackOffset, dst)
    end

    macro getArgumentIncludingThisCount(dst)
        move 1, dst
    end

    metadata(t5, t0)
    callHelper(op_iterator_open, _llint_slow_path_iterator_open_call, OpIteratorOpen, m_iteratorProfile, m_iterator, prepareForRegularCall, invokeForRegularCall, prepareForPolymorphicRegularCall, prepareForSlowRegularCall, size, gotoGetByIdCheckpoint, metadata, getCallee, getArgumentIncludingThisStart, getArgumentIncludingThisCount)

.getByIdStart:
    macro storeNextAndDispatch(value)
        move value, t2
        get(m_next, t1)
        storeq t2, [cfr, t1, 8]
        dispatch()
    end

    loadVariable(get, m_iterator, t3)
    btqnz t3, notCellMask, .iteratorOpenGenericGetNextSlow
    performGetByIDHelper(OpIteratorOpen, m_modeMetadata, m_nextProfile, .iteratorOpenGenericGetNextSlow, size, metadata, storeNextAndDispatch)

.iteratorOpenGenericGetNextSlow:
    callSlowPath(_llint_slow_path_iterator_open_get_next)
    dispatch()

end)

llintOpWithMetadata(op_iterator_next, OpIteratorNext, macro (size, get, dispatch, metadata, return)

    loadVariable(get, m_next, t0)
    btqnz t0, t0, .iteratorNextGeneric
    macro fastNarrow()
        callSlowPath(_iterator_next_try_fast_narrow)
    end
    macro fastWide16()
        callSlowPath(_iterator_next_try_fast_wide16)
    end
    macro fastWide32()
        callSlowPath(_iterator_next_try_fast_wide32)
    end
    size(fastNarrow, fastWide16, fastWide32, macro (callOp) callOp() end)

    # FIXME: We should do this with inline assembly since it's the "fast" case.
    bbeq r1, constexpr IterationMode::Generic, .iteratorNextGeneric
    dispatch()

.iteratorNextGeneric:
    macro gotoGetDoneCheckpoint()
        jmp .getDoneStart
    end

    macro getCallee(dst)
        get(m_next, dst)
    end

    macro getArgumentIncludingThisStart(dst)
        getu(size, OpIteratorNext, m_stackOffset, dst)
    end

    macro getArgumentIncludingThisCount(dst)
        move 1, dst
    end

    # Use m_value slot as a tmp since we are going to write to it later.
    metadata(t5, t0)
    callHelper(op_iterator_next, _llint_slow_path_iterator_next_call, OpIteratorNext, m_nextResultProfile, m_value, prepareForRegularCall, invokeForRegularCall, prepareForPolymorphicRegularCall, prepareForSlowRegularCall, size, gotoGetDoneCheckpoint, metadata, getCallee, getArgumentIncludingThisStart, getArgumentIncludingThisCount)

.getDoneStart:
    macro storeDoneAndJmpToGetValue(doneValue)
        # use t0 because performGetByIDHelper usually puts the doneValue there and offlineasm will elide the self move.
        move doneValue, t0
        get(m_done, t1)
        storeq t0, [cfr, t1, 8]
        jmp .getValueStart
    end


    loadVariable(get, m_value, t3)
    btqnz t3, notCellMask, .getDoneSlow
    performGetByIDHelper(OpIteratorNext, m_doneModeMetadata, m_doneProfile, .getDoneSlow, size, metadata, storeDoneAndJmpToGetValue)

.getDoneSlow:
    callSlowPath(_llint_slow_path_iterator_next_get_done)
    branchIfException(_llint_throw_from_slow_path_trampoline)
    loadVariable(get, m_done, t0)

    # storeDoneAndJmpToGetValue puts the doneValue into t0
.getValueStart:
    # Branch to slow if not misc primitive.
    btqnz t0, ~0xf, .getValueSlow
    btiz t0, 0x1, .notDone
    dispatch()

.notDone:
    macro storeValueAndDispatch(v)
        move v, t2
        storeVariable(get, m_value, t2, t1)
        checkStackPointerAlignment(t0, 0xbaddb01e)
        dispatch()
    end

    # Reload the next result tmp since the get_by_id above may have clobbered t3.
    loadVariable(get, m_value, t3)
    # We don't need to check if the iterator result is a cell here since we will have thrown an error before.
    performGetByIDHelper(OpIteratorNext, m_valueModeMetadata, m_valueProfile, .getValueSlow, size, metadata, storeValueAndDispatch)

.getValueSlow:
    callSlowPath(_llint_slow_path_iterator_next_get_value)
    dispatch()
end)

llintOpWithReturn(op_get_property_enumerator, OpGetPropertyEnumerator, macro (size, get, dispatch, return)
    get(m_base, t1)
    loadConstantOrVariableCell(size, t1, t0, .slowPath)

    loadb JSCell::m_indexingTypeAndMisc[t0], t1
    andi IndexingTypeMask, t1
    bia t1, ArrayWithUndecided, .slowPath

    loadStructureWithScratch(t0, t1, t2)
    loadp Structure::m_previousOrRareData[t1], t1
    btpz t1, .slowPath
    bbeq JSCell::m_type[t1], StructureType, .slowPath

    loadp StructureRareData::m_cachedPropertyNameEnumeratorAndFlag[t1], t1
    btpz t1, .slowPath
    btpnz t1, (constexpr StructureRareData::cachedPropertyNameEnumeratorIsValidatedViaTraversingFlag), .slowPath

    return(t1)

.slowPath:
    callSlowPath(_slow_path_get_property_enumerator)
    dispatch()
end)

llintOp(op_enumerator_next, OpEnumeratorNext, macro (size, get, dispatch)
    # Note: this will always call the slow path on at least the first/last execution of EnumeratorNext for any given loop.
    # The upside this is that we don't have to record any metadata or mode information here as the slow path will do it for us when transitioning from InitMode/IndexedMode to OwnStructureMode, or from OwnStructureMode to GenericMode.
    loadVariable(get, m_mode, t0)
    bbneq t0, constexpr JSPropertyNameEnumerator::OwnStructureMode, .nextSlowPath

    get(m_base, t1)
    loadConstantOrVariableCell(size, t1, t0, .nextSlowPath)

    loadVariable(get, m_enumerator, t1)
    loadi JSPropertyNameEnumerator::m_cachedStructureID[t1], t2
    bineq t2, JSCell::m_structureID[t0], .nextSlowPath

    loadVariable(get, m_index, t2)
    addq 1, t2
    loadi JSPropertyNameEnumerator::m_endStructurePropertyIndex[t1], t3
    biaeq t2, t3, .nextSlowPath

    storeVariable(get, m_index, t2, t3)
    loadp JSPropertyNameEnumerator::m_propertyNames[t1], t3
    zxi2q t2, t2
    loadp [t3, t2, PtrSize], t3

    storeVariable(get, m_propertyName, t3, t2)
    dispatch()

.nextSlowPath:
    callSlowPath(_slow_path_enumerator_next)
    dispatch()
end)

llintOpWithMetadata(op_enumerator_get_by_val, OpEnumeratorGetByVal, macro (size, get, dispatch, metadata, return)
    metadata(t5, t0)

    loadVariable(get, m_mode, t0)

    # FIXME: This should be orb but that doesn't exist for some reason... https://bugs.webkit.org/show_bug.cgi?id=229445
    loadb OpEnumeratorGetByVal::Metadata::m_enumeratorMetadata[t5], t1
    ori t0, t1
    storeb t1, OpEnumeratorGetByVal::Metadata::m_enumeratorMetadata[t5]

    bbneq t0, constexpr JSPropertyNameEnumerator::OwnStructureMode, .getSlowPath

    get(m_base, t1)
    loadConstantOrVariableCell(size, t1, t0, .getSlowPath)

    loadVariable(get, m_enumerator, t1)
    loadi JSPropertyNameEnumerator::m_cachedStructureID[t1], t2
    bineq t2, JSCell::m_structureID[t0], .getSlowPath

    loadVariable(get, m_index, t2)
    loadi JSPropertyNameEnumerator::m_cachedInlineCapacity[t1], t1
    biaeq t2, t1, .outOfLine

    zxi2q t2, t2
    loadq sizeof JSObject[t0, t2, 8], t2
    jmp .done

.outOfLine:
    loadp JSObject::m_butterfly[t0], t0
    subi t1, t2
    negi t2
    sxi2q t2, t2
    loadq constexpr ((offsetInButterfly(firstOutOfLineOffset)) * sizeof(EncodedJSValue))[t0, t2, 8], t2

.done:
    valueProfile(OpEnumeratorGetByVal, m_profile, t5, t2)
    return(t2)

.getSlowPath:
    callSlowPath(_slow_path_enumerator_get_by_val)
    dispatch()
end)

macro hasPropertyImpl(opcodeStruct, size, get, dispatch, metadata, return, slowPath)
    metadata(t5, t0)

    loadVariable(get, m_mode, t0)
    # FIXME: This should be orb but that doesn't exist for some reason... https://bugs.webkit.org/show_bug.cgi?id=229445
    loadb %opcodeStruct%::Metadata::m_enumeratorMetadata[t5], t1
    ori t0, t1
    storeb t1, %opcodeStruct%::Metadata::m_enumeratorMetadata[t5]

    bbneq t0, constexpr JSPropertyNameEnumerator::OwnStructureMode, .callSlowPath

    get(m_base, t1)
    loadConstantOrVariableCell(size, t1, t0, .callSlowPath)

    loadVariable(get, m_enumerator, t1)
    loadi JSPropertyNameEnumerator::m_cachedStructureID[t1], t2
    bineq t2, JSCell::m_structureID[t0], .callSlowPath

    move ValueTrue, t2
    return(t2)

.callSlowPath:
    callSlowPath(slowPath)
    dispatch()
end

llintOpWithMetadata(op_enumerator_in_by_val, OpEnumeratorInByVal, macro (size, get, dispatch, metadata, return)
    hasPropertyImpl(OpEnumeratorInByVal, size, get, dispatch, metadata, return, _slow_path_enumerator_in_by_val)
end)

llintOpWithMetadata(op_enumerator_has_own_property, OpEnumeratorHasOwnProperty, macro (size, get, dispatch, metadata, return)
    hasPropertyImpl(OpEnumeratorHasOwnProperty, size, get, dispatch, metadata, return, _slow_path_enumerator_has_own_property)
end)

llintOpWithProfile(op_get_internal_field, OpGetInternalField, macro (size, get, dispatch, return)
    loadVariable(get, m_base, t1)
    getu(size, OpGetInternalField, m_index, t2)
    loadq JSInternalFieldObjectImpl_internalFields[t1, t2, SlotSize], t0
    return(t0)
end)

llintOp(op_put_internal_field, OpPutInternalField, macro (size, get, dispatch)
    loadVariable(get, m_base, t0)
    get(m_value, t1)
    loadConstantOrVariable(size, t1, t2)
    getu(size, OpPutInternalField, m_index, t1)
    storeq t2, JSInternalFieldObjectImpl_internalFields[t0, t1, SlotSize]
    writeBarrierOnCellAndValueWithReload(t0, t2, macro() end)
    dispatch()
end)


llintOp(op_log_shadow_chicken_prologue, OpLogShadowChickenPrologue, macro (size, get, dispatch)
    acquireShadowChickenPacket(.opLogShadowChickenPrologueSlow)
    storep cfr, ShadowChicken::Packet::frame[t0]
    loadp CallerFrame[cfr], t1
    storep t1, ShadowChicken::Packet::callerFrame[t0]
    loadp Callee[cfr], t1
    storep t1, ShadowChicken::Packet::callee[t0]
    loadVariable(get, m_scope, t1)
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
    loadVariable(get, m_thisValue, t1)
    storep t1, ShadowChicken::Packet::thisValue[t0]
    loadVariable(get, m_scope, t1)
    storep t1, ShadowChicken::Packet::scope[t0]
    loadp CodeBlock[cfr], t1
    storep t1, ShadowChicken::Packet::codeBlock[t0]
    storei PC, ShadowChicken::Packet::callSiteIndex[t0]
    dispatch()
.opLogShadowChickenTailSlow:
    callSlowPath(_llint_slow_path_log_shadow_chicken_tail)
    dispatch()
end)

op(fuzzer_return_early_from_loop_hint, macro ()
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_globalThis[t0], t0
    doReturn()
end)
