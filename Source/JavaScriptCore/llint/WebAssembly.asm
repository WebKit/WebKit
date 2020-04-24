# Copyright (C) 2019 Apple Inc. All rights reserved.
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

# Calling conventions
const CalleeSaveSpaceAsVirtualRegisters = constexpr Wasm::numberOfLLIntCalleeSaveRegisters
const CalleeSaveSpaceStackAligned = (CalleeSaveSpaceAsVirtualRegisters * SlotSize + StackAlignment - 1) & ~StackAlignmentMask
const WasmEntryPtrTag = constexpr WasmEntryPtrTag

if HAVE_FAST_TLS
    const WTF_WASM_CONTEXT_KEY = constexpr WTF_WASM_CONTEXT_KEY
end

if X86_64
    const NumberOfWasmArgumentGPRs = 6
elsif ARM64 or ARM64E
    const NumberOfWasmArgumentGPRs = 8
else
    error
end
const NumberOfWasmArgumentFPRs = 8
const NumberOfWasmArguments = NumberOfWasmArgumentGPRs + NumberOfWasmArgumentFPRs

# All callee saves must match the definition in WasmCallee.cpp

# These must match the definition in WasmMemoryInformation.cpp
const wasmInstance = csr0
const memoryBase = csr3
const memorySize = csr4

# This must match the definition in LowLevelInterpreter.asm
if X86_64
    const PB = csr2
elsif ARM64 or ARM64E
    const PB = csr7
else
    error
end

macro forEachArgumentGPR(fn)
    fn(0 * 8, wa0)
    fn(1 * 8, wa1)
    fn(2 * 8, wa2)
    fn(3 * 8, wa3)
    fn(4 * 8, wa4)
    fn(5 * 8, wa5)
    if ARM64 or ARM64E
        fn(6 * 8, wa6)
        fn(7 * 8, wa7)
    end
end

macro forEachArgumentFPR(fn)
    fn((NumberOfWasmArgumentGPRs + 0) * 8, wfa0)
    fn((NumberOfWasmArgumentGPRs + 1) * 8, wfa1)
    fn((NumberOfWasmArgumentGPRs + 2) * 8, wfa2)
    fn((NumberOfWasmArgumentGPRs + 3) * 8, wfa3)
    fn((NumberOfWasmArgumentGPRs + 4) * 8, wfa4)
    fn((NumberOfWasmArgumentGPRs + 5) * 8, wfa5)
    fn((NumberOfWasmArgumentGPRs + 6) * 8, wfa6)
    fn((NumberOfWasmArgumentGPRs + 7) * 8, wfa7)
end

# Helper macros
# FIXME: Eventually this should be unified with the JS versions
# https://bugs.webkit.org/show_bug.cgi?id=203656

macro wasmDispatch(advanceReg)
    addp advanceReg, PC
    wasmNextInstruction()
end

macro wasmDispatchIndirect(offsetReg)
    wasmDispatch(offsetReg)
end

macro wasmNextInstruction()
    loadb [PB, PC, 1], t0
    leap _g_opcodeMap, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag
end

macro wasmNextInstructionWide16()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide16, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag
end

macro wasmNextInstructionWide32()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide32, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag
end

macro checkSwitchToJIT(increment, action)
    loadp CodeBlock[cfr], ws0
    baddis increment, Wasm::FunctionCodeBlock::m_tierUpCounter + Wasm::LLIntTierUpCounter::m_counter[ws0], .continue
    action()
    .continue:
end

macro checkSwitchToJITForPrologue(codeBlockRegister)
    checkSwitchToJIT(
        5,
        macro()
            move cfr, a0
            move PC, a1
            move wasmInstance, a2
            cCall4(_slow_path_wasm_prologue_osr)
            btpz r0, .recover
            move r0, ws0

            forEachArgumentGPR(macro (offset, gpr)
                loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
            end)

            forEachArgumentFPR(macro (offset, fpr)
                loadd -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
            end)

            restoreCalleeSavesUsedByWasm()
            restoreCallerPCAndCFR()
            untagReturnAddress sp
            jmp ws0, WasmEntryPtrTag
        .recover:
            notFunctionCodeBlockGetter(codeBlockRegister)
        end)
end

macro checkSwitchToJITForLoop()
    checkSwitchToJIT(
        1,
        macro()
            storei PC, ArgumentCountIncludingThis + TagOffset[cfr]
            prepareStateForCCall()
            move cfr, a0
            move PC, a1
            move wasmInstance, a2
            cCall4(_slow_path_wasm_loop_osr)
            btpz r1, .recover
            restoreCalleeSavesUsedByWasm()
            restoreCallerPCAndCFR()
            untagReturnAddress sp
            move r0, a0
            jmp r1, WasmEntryPtrTag
        .recover:
            loadi ArgumentCountIncludingThis + TagOffset[cfr], PC
        end)
end

macro checkSwitchToJITForEpilogue()
    checkSwitchToJIT(
        10,
        macro ()
            callWasmSlowPath(_slow_path_wasm_epilogue_osr)
        end)
end

# Wasm specific helpers

macro preserveCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't save memoryBase and memorySize here. See the comment
    # in restoreCalleeSavesUsedByWasm() below for why.
    subp CalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        emit "stp x19, x26, [x29, #-16]"
    elsif X86_64
        storep PB, -0x8[cfr]
        storep wasmInstance, -0x10[cfr]
    else
        error
    end
end

macro restoreCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't restore memoryBase and memorySize here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    if ARM64 or ARM64E
        emit "ldp x19, x26, [x29, #-16]"
    elsif X86_64
        loadp -0x8[cfr], PB
        loadp -0x10[cfr], wasmInstance
    else
        error
    end
end

macro loadWasmInstanceFromTLS()
if  HAVE_FAST_TLS
    tls_loadp WTF_WASM_CONTEXT_KEY, wasmInstance
else
    crash()
end
end

macro storeWasmInstanceToTLS(instance)
if  HAVE_FAST_TLS
    tls_storep instance, WTF_WASM_CONTEXT_KEY
else
    crash()
end
end

macro reloadMemoryRegistersFromInstance(instance, scratch1, scratch2)
    loadp Wasm::Instance::m_cachedMemory[instance], memoryBase
    loadi Wasm::Instance::m_cachedMemorySize[instance], memorySize
    cagedPrimitive(memoryBase, memorySize, scratch1, scratch2)
end

macro throwException(exception)
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
end

macro callWasmSlowPath(slowPath)
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    move wasmInstance, a2
    cCall4(slowPath)
    restoreStateAfterCCall()
end

macro callWasmCallSlowPath(slowPath, action)
    storei PC, ArgumentCountIncludingThis + TagOffset[cfr]
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    move wasmInstance, a2
    cCall4(slowPath)
    action(r0, r1)
end

macro wasmPrologue(codeBlockGetter, codeBlockSetter, loadWasmInstance)
    # Set up the call frame and check if we should OSR.
    tagReturnAddress sp
    preserveCallerPCAndCFR()
    preserveCalleeSavesUsedByWasm()
    loadWasmInstance()
    reloadMemoryRegistersFromInstance(wasmInstance, ws0, ws1)

    codeBlockGetter(ws0)
    codeBlockSetter(ws0)

    # Get new sp in ws1 and check stack height.
    loadi Wasm::FunctionCodeBlock::m_numCalleeLocals[ws0], ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
    subp cfr, ws1, ws1

    bpa ws1, cfr, .stackOverflow
    bpbeq Wasm::Instance::m_cachedStackLimit[wasmInstance], ws1, .stackHeightOK

.stackOverflow:
    throwException(StackOverflow)

.stackHeightOK:
    move ws1, sp

    forEachArgumentGPR(macro (offset, gpr)
        storeq gpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)

    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)

    checkSwitchToJITForPrologue(ws0)

    # Set up the PC.
    loadp Wasm::FunctionCodeBlock::m_instructionsRawPointer[ws0], PB
    move 0, PC

    loadi Wasm::FunctionCodeBlock::m_numVars[ws0], ws1
    subi NumberOfWasmArguments + CalleeSaveSpaceAsVirtualRegisters, ws1
    btiz ws1, .zeroInitializeLocalsDone
    negi ws1
    sxi2q ws1, ws1
    leap (NumberOfWasmArguments + CalleeSaveSpaceAsVirtualRegisters + 1) * -8[cfr], ws0
.zeroInitializeLocalsLoop:
    addq 1, ws1
    storeq 0, [ws0, ws1, 8]
    btqz ws1, .zeroInitializeLocalsDone
    jmp .zeroInitializeLocalsLoop
.zeroInitializeLocalsDone:
end

macro traceExecution()
    if TRACING
        callWasmSlowPath(_slow_path_wasm_trace)
    end
end

# Less convenient, but required for opcodes that collide with reserved instructions (e.g. wasm_nop)
macro unprefixedWasmOp(opcodeName, opcodeStruct, fn)
    commonOp(opcodeName, traceExecution, macro(size)
        fn(macro(fn2)
            fn2(opcodeName, opcodeStruct, size)
        end)
    end)
end

macro wasmOp(opcodeName, opcodeStruct, fn)
    unprefixedWasmOp(wasm_%opcodeName%, opcodeStruct, fn)
end

# Same as unprefixedWasmOp, necessary for e.g. wasm_call
macro unprefixedSlowWasmOp(opcodeName)
    unprefixedWasmOp(opcodeName, unusedOpcodeStruct, macro(ctx)
        callWasmSlowPath(_slow_path_%opcodeName%)
        dispatch(ctx)
    end)
end

macro slowWasmOp(opcodeName)
    unprefixedSlowWasmOp(wasm_%opcodeName%)
end

# Macro version of load operations: mload[suffix]
# loads field from the instruction stream and performs load[suffix] to dst
macro firstConstantRegisterIndex(ctx, fn)
    ctx(macro(opcodeName, opcodeStruct, size)
        size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide16, FirstConstantRegisterIndexWide32, fn)
    end)
end

macro loadConstantOrVariable(ctx, index, loader)
    firstConstantRegisterIndex(ctx, macro (firstConstantIndex)
        bpgteq index, firstConstantIndex, .constant
        loader([cfr, index, 8])
        jmp .done
    .constant:
        loadp CodeBlock[cfr], t6
        loadp Wasm::FunctionCodeBlock::m_constants + VectorBufferOffset[t6], t6
        subp firstConstantIndex, index
        loader([t6, index, 8])
    .done:
    end)
end

macro mloadq(ctx, field, dst)
    wgets(ctx, field, dst)
    loadConstantOrVariable(ctx, dst, macro (from)
        loadq from, dst
    end)
end

macro mloadi(ctx, field, dst)
    wgets(ctx, field, dst)
    loadConstantOrVariable(ctx, dst, macro (from)
        loadi from, dst
    end)
end

macro mloadp(ctx, field, dst)
    wgets(ctx, field, dst)
    loadConstantOrVariable(ctx, dst, macro (from)
        loadp from, dst
    end)
end

macro mloadf(ctx, field, dst)
    wgets(ctx, field, t5)
    loadConstantOrVariable(ctx, t5, macro (from)
        loadf from, dst
    end)
end

macro mloadd(ctx, field, dst)
    wgets(ctx, field, t5)
    loadConstantOrVariable(ctx, t5, macro (from)
        loadd from, dst
    end)
end

# Typed returns

macro returnq(ctx, value)
    wgets(ctx, m_dst, t5)
    storeq value, [cfr, t5, 8]
    dispatch(ctx)
end

macro returni(ctx, value)
    wgets(ctx, m_dst, t5)
    storei value, [cfr, t5, 8]
    dispatch(ctx)
end

macro returnf(ctx, value)
    wgets(ctx, m_dst, t5)
    storef value, [cfr, t5, 8]
    dispatch(ctx)
end

macro returnd(ctx, value)
    wgets(ctx, m_dst, t5)
    stored value, [cfr, t5, 8]
    dispatch(ctx)
end

# Wasm wrapper of get/getu that operate on ctx
macro wgets(ctx, field, dst)
    ctx(macro(opcodeName, opcodeStruct, size)
        size(getOperandNarrow, getOperandWide16, getOperandWide32, macro (get)
            get(opcodeStruct, field, dst)
        end)
    end)
end

macro wgetu(ctx, field, dst)
    ctx(macro(opcodeName, opcodeStruct, size)
        size(getuOperandNarrow, getuOperandWide16, getuOperandWide32, macro (getu)
            getu(opcodeStruct, field, dst)
        end)
    end)
end

# Control flow helpers

macro dispatch(ctx)
    ctx(macro(opcodeName, opcodeStruct, size)
        genericDispatchOp(wasmDispatch, size, opcodeName)
    end)
end

macro jump(ctx, target)
    wgets(ctx, target, t0)
    btiz t0, .outOfLineJumpTarget
    wasmDispatchIndirect(t0)
.outOfLineJumpTarget:
    callWasmSlowPath(_slow_path_wasm_out_of_line_jump_target)
    wasmNextInstruction()
end

macro doReturn()
    restoreCalleeSavesUsedByWasm()
    restoreCallerPCAndCFR()
    ret
end

# Entry point

macro wasmCodeBlockGetter(targetRegister)
    loadp Callee[cfr], targetRegister
    andp ~3, targetRegister
    loadp Wasm::LLIntCallee::m_codeBlock[targetRegister], targetRegister
end

op(wasm_function_prologue, macro ()
    if not WEBASSEMBLY or not JSVALUE64 or C_LOOP or C_LOOP_WIN
        error
    end

    wasmPrologue(wasmCodeBlockGetter, functionCodeBlockSetter, loadWasmInstanceFromTLS)
    wasmNextInstruction()
end)

op(wasm_function_prologue_no_tls, macro ()
    if not WEBASSEMBLY or not JSVALUE64 or C_LOOP or C_LOOP_WIN
        error
    end

    wasmPrologue(wasmCodeBlockGetter, functionCodeBlockSetter, macro () end)
    wasmNextInstruction()
end)

op(wasm_throw_from_slow_path_trampoline, macro ()
    loadp Wasm::Instance::m_pointerToTopEntryFrame[wasmInstance], t5
    loadp [t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    move cfr, a0
    addp PB, PC, a1
    move wasmInstance, a2
    # Slow paths and the throwException macro store the exception code in the ArgumentCountIncludingThis slot
    loadi ArgumentCountIncludingThis + PayloadOffset[cfr], a3
    cCall4(_slow_path_wasm_throw_exception)

    jmp r0, ExceptionHandlerPtrTag
end)

# Disable wide version of narrow-only opcodes
noWide(wasm_enter)
noWide(wasm_wide16)
noWide(wasm_wide32)

# Opcodes that always invoke the slow path

slowWasmOp(ref_func)
slowWasmOp(table_get)
slowWasmOp(table_set)
slowWasmOp(table_size)
slowWasmOp(table_fill)
slowWasmOp(table_grow)
slowWasmOp(set_global_ref)
slowWasmOp(set_global_ref_portable_binding)

wasmOp(grow_memory, WasmGrowMemory, macro(ctx)
    callWasmSlowPath(_slow_path_wasm_grow_memory)
    reloadMemoryRegistersFromInstance(wasmInstance, ws0, ws1)
    dispatch(ctx)
end)

# Opcodes that should eventually be shared with JS llint

_wasm_wide16:
    wasmNextInstructionWide16()

_wasm_wide32:
    wasmNextInstructionWide32()

_wasm_enter:
    traceExecution()
    checkStackPointerAlignment(t2, 0xdead00e1)
    loadp CodeBlock[cfr], t2                // t2<CodeBlock> = cfr.CodeBlock
    loadi Wasm::FunctionCodeBlock::m_numVars[t2], t2      // t2<size_t> = t2<CodeBlock>.m_numVars
    subq CalleeSaveSpaceAsVirtualRegisters + NumberOfWasmArguments, t2
    btiz t2, .opEnterDone
    move cfr, t1
    subq CalleeSaveSpaceAsVirtualRegisters * 8 + NumberOfWasmArguments * 8, t1
    negi t2
    sxi2q t2, t2
.opEnterLoop:
    storeq 0, [t1, t2, 8]
    addq 1, t2
    btqnz t2, .opEnterLoop
.opEnterDone:
    wasmDispatchIndirect(1)

unprefixedWasmOp(wasm_nop, WasmNop, macro(ctx)
    dispatch(ctx)
end)

wasmOp(loop_hint, WasmLoopHint, macro(ctx)
    checkSwitchToJITForLoop()
    dispatch(ctx)
end)

wasmOp(mov, WasmMov, macro(ctx)
    mloadq(ctx, m_src, t0)
    returnq(ctx, t0)
end)

wasmOp(jtrue, WasmJtrue, macro(ctx)
    mloadi(ctx, m_condition, t0)
    btiz t0, .continue
    jump(ctx, m_targetLabel)
.continue:
    dispatch(ctx)
end)

wasmOp(jfalse, WasmJfalse, macro(ctx)
    mloadi(ctx, m_condition, t0)
    btinz t0, .continue
    jump(ctx, m_targetLabel)
.continue:
    dispatch(ctx)
end)

wasmOp(switch, WasmSwitch, macro(ctx)
    mloadi(ctx, m_scrutinee, t0)
    wgetu(ctx, m_tableIndex, t1)

    loadp CodeBlock[cfr], t2
    loadp Wasm::FunctionCodeBlock::m_jumpTables + VectorBufferOffset[t2], t2
    muli sizeof Wasm::FunctionCodeBlock::JumpTable, t1
    addp t1, t2

    loadi VectorSizeOffset[t2], t3
    bib t0, t3, .inBounds

.outOfBounds:
    subi t3, 1, t0

.inBounds:
    loadp VectorBufferOffset[t2], t2
    muli sizeof Wasm::FunctionCodeBlock::JumpTableEntry, t0

    loadi Wasm::FunctionCodeBlock::JumpTableEntry::startOffset[t2, t0], t1
    loadi Wasm::FunctionCodeBlock::JumpTableEntry::dropCount[t2, t0], t3
    loadi Wasm::FunctionCodeBlock::JumpTableEntry::keepCount[t2, t0], t5
    dropKeep(t1, t3, t5)

    loadis Wasm::FunctionCodeBlock::JumpTableEntry::target[t2, t0], t3
    assert(macro(ok) btinz t3, .ok end)
    wasmDispatchIndirect(t3)
end)

unprefixedWasmOp(wasm_jmp, WasmJmp, macro(ctx)
    jump(ctx, m_targetLabel)
end)

unprefixedWasmOp(wasm_ret, WasmRet, macro(ctx)
    checkSwitchToJITForEpilogue()
    forEachArgumentGPR(macro (offset, gpr)
        loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
    end)
    forEachArgumentFPR(macro (offset, fpr)
        loadd -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
    end)
    doReturn()
end)

# Wasm specific bytecodes

wasmOp(unreachable, WasmUnreachable, macro(ctx)
    throwException(Unreachable)
end)

wasmOp(ret_void, WasmRetVoid, macro(ctx)
    checkSwitchToJITForEpilogue()
    doReturn()
end)

wasmOp(ref_is_null, WasmRefIsNull, macro(ctx)
    mloadp(ctx, m_ref, t0)
    cqeq t0, ValueNull, t0
    returni(ctx, t0)
end)

wasmOp(get_global, WasmGetGlobal, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    loadq [t0, t1, 8], t0
    returnq(ctx, t0)
end)

wasmOp(set_global, WasmSetGlobal, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    mloadq(ctx, m_value, t2)
    storeq t2, [t0, t1, 8]
    dispatch(ctx)
end)

wasmOp(get_global_portable_binding, WasmGetGlobalPortableBinding, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    loadq [t0, t1, 8], t0
    loadq [t0], t0
    returnq(ctx, t0)
end)

wasmOp(set_global_portable_binding, WasmSetGlobalPortableBinding, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    mloadq(ctx, m_value, t2)
    loadq [t0, t1, 8], t0
    storeq t2, [t0]
    dispatch(ctx)
end)

macro slowPathForWasmCall(ctx, slowPath, storeWasmInstance)
    callWasmCallSlowPath(
        slowPath,
        # callee is r0 and targetWasmInstance is r1
        macro (callee, targetWasmInstance)
            move callee, ws0

            loadi ArgumentCountIncludingThis + TagOffset[cfr], PC

            # the call might throw (e.g. indirect call with bad signature)
            btpz targetWasmInstance, .throw

            wgetu(ctx, m_stackOffset, ws1)
            lshifti 3, ws1
            subp cfr, ws1, sp

            wgetu(ctx, m_numberOfStackArgs, ws1)

            # Preserve the current instance
            move wasmInstance, PB

            storeWasmInstance(targetWasmInstance)
            reloadMemoryRegistersFromInstance(targetWasmInstance, wa0, wa1)

            # Load registers from stack
            forEachArgumentGPR(macro (offset, gpr)
                loadq CallFrameHeaderSize + offset[sp, ws1, 8], gpr
            end)

            forEachArgumentFPR(macro (offset, fpr)
                loadd CallFrameHeaderSize + offset[sp, ws1, 8], fpr
            end)

            addp CallerFrameAndPCSize, sp
            call ws0, SlowPathPtrTag

            loadp CodeBlock[cfr], ws1
            loadi Wasm::FunctionCodeBlock::m_numCalleeLocals[ws1], ws1
            lshiftp 3, ws1
            addp maxFrameExtentForSlowPathCall, ws1
            subp cfr, ws1, sp

            # We need to set PC to load information from the instruction stream, but we
            # need to preserve its current value since it might contain a return value
            move PC, memoryBase
            move PB, wasmInstance
            loadi ArgumentCountIncludingThis + TagOffset[cfr], PC
            loadp CodeBlock[cfr], PB
            loadp Wasm::FunctionCodeBlock::m_instructionsRawPointer[PB], PB

            wgetu(ctx, m_stackOffset, ws1)
            lshifti 3, ws1
            negi ws1
            sxi2q ws1, ws1
            addp cfr, ws1

            # Argument registers are also return registers, so they must be stored to the stack
            # in case they contain return values.
            wgetu(ctx, m_numberOfStackArgs, ws0)
            move memoryBase, PC
            forEachArgumentGPR(macro (offset, gpr)
                storeq gpr, CallFrameHeaderSize + offset[ws1, ws0, 8]
            end)

            forEachArgumentFPR(macro (offset, fpr)
                stored fpr, CallFrameHeaderSize + offset[ws1, ws0, 8]
            end)

            loadi ArgumentCountIncludingThis + TagOffset[cfr], PC

            storeWasmInstance(wasmInstance)
            reloadMemoryRegistersFromInstance(wasmInstance, ws0, ws1)

            # Restore stack limit
            loadp Wasm::Instance::m_pointerToActualStackLimit[wasmInstance], t5
            loadp [t5], t5
            storep t5, Wasm::Instance::m_cachedStackLimit[wasmInstance]

            dispatch(ctx)

        .throw:
            restoreStateAfterCCall()
            dispatch(ctx)
        end)
end

unprefixedWasmOp(wasm_call, WasmCall, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call, storeWasmInstanceToTLS)
end)

unprefixedWasmOp(wasm_call_no_tls, WasmCallNoTls, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call_no_tls, macro(targetInstance) move targetInstance, wasmInstance end)
end)

wasmOp(call_indirect, WasmCallIndirect, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call_indirect, storeWasmInstanceToTLS)
end)

wasmOp(call_indirect_no_tls, WasmCallIndirectNoTls, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call_indirect_no_tls, macro(targetInstance) move targetInstance, wasmInstance end)
end)

wasmOp(current_memory, WasmCurrentMemory, macro(ctx)
    loadp Wasm::Instance::m_cachedMemorySize[wasmInstance], t0
    urshiftq 16, t0
    returnq(ctx, t0)
end)

wasmOp(select, WasmSelect, macro(ctx)
    mloadi(ctx, m_condition, t0)
    btiz t0, .isZero
    mloadq(ctx, m_nonZero, t0)
    returnq(ctx, t0)
.isZero:
    mloadq(ctx, m_zero, t0)
    returnq(ctx, t0)
end)

# uses offset as scratch and returns result on pointer
macro emitCheckAndPreparePointer(ctx, pointer, offset, size)
    leap size - 1[pointer, offset], t5
    bpb t5, memorySize, .continuation
    throwException(OutOfBoundsMemoryAccess)
.continuation:
    addp memoryBase, pointer
end

macro wasmLoadOp(name, struct, size, fn)
    wasmOp(name, struct, macro(ctx)
        mloadi(ctx, m_pointer, t0)
        wgetu(ctx, m_offset, t1)
        emitCheckAndPreparePointer(ctx, t0, t1, size)
        fn([t0, t1], t2)
        returnq(ctx, t2)
    end)
end

wasmLoadOp(load8_u, WasmLoad8U, 1, macro(mem, dst) loadb mem, dst end)
wasmLoadOp(load16_u, WasmLoad16U, 2, macro(mem, dst) loadh mem, dst end)
wasmLoadOp(load32_u, WasmLoad32U, 4, macro(mem, dst) loadi mem, dst end)
wasmLoadOp(load64_u, WasmLoad64U, 8, macro(mem, dst) loadq mem, dst end)

wasmLoadOp(i32_load8_s, WasmI32Load8S, 1, macro(mem, dst) loadbsi mem, dst end)
wasmLoadOp(i64_load8_s, WasmI64Load8S, 1, macro(mem, dst) loadbsq mem, dst end)
wasmLoadOp(i32_load16_s, WasmI32Load16S, 2, macro(mem, dst) loadhsi mem, dst end)
wasmLoadOp(i64_load16_s, WasmI64Load16S, 2, macro(mem, dst) loadhsq mem, dst end)
wasmLoadOp(i64_load32_s, WasmI64Load32S, 4, macro(mem, dst) loadis mem, dst end)

macro wasmStoreOp(name, struct, size, fn)
    wasmOp(name, struct, macro(ctx)
        mloadi(ctx, m_pointer, t0)
        wgetu(ctx, m_offset, t1)
        emitCheckAndPreparePointer(ctx, t0, t1, size)
        mloadq(ctx, m_value, t2)
        fn(t2, [t0, t1])
        dispatch(ctx)
    end)
end

wasmStoreOp(store8, WasmStore8, 1, macro(value, mem) storeb value, mem end)
wasmStoreOp(store16, WasmStore16, 2, macro(value, mem) storeh value, mem end)
wasmStoreOp(store32, WasmStore32, 4, macro(value, mem) storei value, mem end)
wasmStoreOp(store64, WasmStore64, 8, macro(value, mem) storeq value, mem end)

# Opcodes that don't have the `b3op` entry in wasm.json. This should be kept in sync

wasmOp(i32_div_s, WasmI32DivS, macro (ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)

    btiz t1, .throwDivisionByZero

    bineq t1, -1, .safe
    bieq t0, constexpr INT32_MIN, .throwIntegerOverflow

.safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cdqi
        idivi t1
    elsif ARM64 or ARM64E
        divis t1, t0
    else
        error
    end
    returni(ctx, t0)

.throwDivisionByZero:
    throwException(DivisionByZero)

.throwIntegerOverflow:
    throwException(IntegerOverflow)
end)

wasmOp(i32_div_u, WasmI32DivU, macro (ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)

    btiz t1, .throwDivisionByZero

    if X86_64
        xori t2, t2
        udivi t1
    elsif ARM64 or ARM64E
        divi t1, t0
    else
        error
    end
    returni(ctx, t0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i32_rem_s, WasmI32RemS, macro (ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)

    btiz t1, .throwDivisionByZero

    bineq t1, -1, .safe
    bineq t0, constexpr INT32_MIN, .safe

    move 0, t2
    jmp .return

.safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cdqi
        idivi t1
    elsif ARM64 or ARM64E
        divis t1, t0, t2
        muli t1, t2
        subi t0, t2, t2
    else
        error
    end

.return:
    returni(ctx, t2)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i32_rem_u, WasmI32RemU, macro (ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)

    btiz t1, .throwDivisionByZero

    if X86_64
        xori t2, t2
        udivi t1
    elsif ARM64 or ARM64E
        divi t1, t0, t2
        muli t1, t2
        subi t0, t2, t2
    else
        error
    end
    returni(ctx, t2)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i32_ctz, WasmI32Ctz, macro (ctx)
    mloadq(ctx, m_operand, t0)
    tzcnti t0, t0
    returni(ctx, t0)
end)

wasmOp(i32_popcnt, WasmI32Popcnt, macro (ctx)
    mloadi(ctx, m_operand, a1)
    prepareStateForCCall()
    move PC, a0
    cCall2(_slow_path_wasm_popcount)
    restoreStateAfterCCall()
    returni(ctx, r1)
end)

wasmOp(i64_div_s, WasmI64DivS, macro (ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)

    btqz t1, .throwDivisionByZero

    bqneq t1, -1, .safe
    bqeq t0, constexpr INT64_MIN, .throwIntegerOverflow

.safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cqoq
        idivq t1
    elsif ARM64 or ARM64E
        divqs t1, t0
    else
        error
    end
    returnq(ctx, t0)

.throwDivisionByZero:
    throwException(DivisionByZero)

.throwIntegerOverflow:
    throwException(IntegerOverflow)
end)

wasmOp(i64_div_u, WasmI64DivU, macro (ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)

    btqz t1, .throwDivisionByZero

    if X86_64
        xorq t2, t2
        udivq t1
    elsif ARM64 or ARM64E
        divq t1, t0
    else
        error
    end
    returnq(ctx, t0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i64_rem_s, WasmI64RemS, macro (ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)

    btqz t1, .throwDivisionByZero

    bqneq t1, -1, .safe
    bqneq t0, constexpr INT64_MIN, .safe

    move 0, t2
    jmp .return

.safe:
    if X86_64
        # FIXME: Add a way to static_asset that t0 is rax and t2 is rdx
        # https://bugs.webkit.org/show_bug.cgi?id=203692
        cqoq
        idivq t1
    elsif ARM64 or ARM64E
        divqs t1, t0, t2
        mulq t1, t2
        subq t0, t2, t2
    else
        error
    end

.return:
    returnq(ctx, t2) # rdx has the remainder

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i64_rem_u, WasmI64RemU, macro (ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)

    btqz t1, .throwDivisionByZero

    if X86_64
        xorq t2, t2
        udivq t1
    elsif ARM64 or ARM64E
        divq t1, t0, t2
        mulq t1, t2
        subq t0, t2, t2
    else
        error
    end
    returnq(ctx, t2)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i64_ctz, WasmI64Ctz, macro (ctx)
    mloadq(ctx, m_operand, t0)
    tzcntq t0, t0
    returnq(ctx, t0)
end)

wasmOp(i64_popcnt, WasmI64Popcnt, macro (ctx)
    mloadq(ctx, m_operand, a1)
    prepareStateForCCall()
    move PC, a0
    cCall2(_slow_path_wasm_popcountll)
    restoreStateAfterCCall()
    returnq(ctx, r1)
end)

wasmOp(f32_trunc, WasmF32Trunc, macro (ctx)
    mloadf(ctx, m_operand, ft0)
    truncatef ft0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_nearest, WasmF32Nearest, macro (ctx)
    mloadf(ctx, m_operand, ft0)
    roundf ft0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f64_trunc, WasmF64Trunc, macro (ctx)
    mloadd(ctx, m_operand, ft0)
    truncated ft0, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_nearest, WasmF64Nearest, macro (ctx)
    mloadd(ctx, m_operand, ft0)
    roundd ft0, ft0
    returnd(ctx, ft0)
end)

wasmOp(i32_trunc_s_f32, WasmI32TruncSF32, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xcf000000, t0 # INT32_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .outOfBoundsTrunc

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTrunc

    truncatef2is ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i32_trunc_s_f64, WasmI32TruncSF64, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xc1e0000000000000, t0 # INT32_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .outOfBoundsTrunc

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTrunc

    truncated2is ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i32_trunc_u_f32, WasmI32TruncUF32, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .outOfBoundsTrunc

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTrunc

    truncatef2i ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i32_trunc_u_f64, WasmI32TruncUF64, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .outOfBoundsTrunc

    move 0x41f0000000000000, t0 # INT32_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTrunc

    truncated2i ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i64_trunc_s_f32, WasmI64TruncSF32, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .outOfBoundsTrunc

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTrunc

    truncatef2qs ft0, t0
    returnq(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i64_trunc_s_f64, WasmI64TruncSF64, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xc3e0000000000000, t0 # INT64_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .outOfBoundsTrunc

    move 0x43e0000000000000, t0 # -INT64_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTrunc

    truncated2qs ft0, t0
    returnq(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i64_trunc_u_f32, WasmI64TruncUF32, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .outOfBoundsTrunc

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTrunc

    truncatef2q ft0, t0
    returnq(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i64_trunc_u_f64, WasmI64TruncUF64, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .outOfBoundsTrunc

    move 0x43f0000000000000, t0 # INT64_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTrunc

    truncated2q ft0, t0
    returnq(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(f32_convert_u_i64, WasmF32ConvertUI64, macro (ctx)
    mloadq(ctx, m_operand, t0)
    if X86_64
        cq2f t0, t1, ft0
    elsif ARM64 or ARM64E
        cq2f t0, ft0
    else
        error
    end
    returnf(ctx, ft0)
end)

wasmOp(f64_convert_u_i64, WasmF64ConvertUI64, macro (ctx)
    mloadq(ctx, m_operand, t0)
    if X86_64
        cq2d t0, t1, ft0
    elsif ARM64 or ARM64E
        cq2d t0, ft0
    else
        error
    end
    returnd(ctx, ft0)
end)

wasmOp(i32_eqz, WasmI32Eqz, macro(ctx)
    mloadi(ctx, m_operand, t0)
    cieq t0, 0, t0
    returni(ctx, t0)
end)

wasmOp(i64_shl, WasmI64Shl, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    lshiftq t1, t0
    returnq(ctx, t0)
end)

wasmOp(i64_shr_u, WasmI64ShrU, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    urshiftq t1, t0
    returnq(ctx, t0)
end)

wasmOp(i64_shr_s, WasmI64ShrS, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    rshiftq t1, t0
    returnq(ctx, t0)
end)

wasmOp(i64_rotr, WasmI64Rotr, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    rrotateq t1, t0
    returnq(ctx, t0)
end)

wasmOp(i64_rotl, WasmI64Rotl, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    lrotateq t1, t0
    returnq(ctx, t0)
end)

wasmOp(i64_eqz, WasmI64Eqz, macro(ctx)
    mloadq(ctx, m_operand, t0)
    cqeq t0, 0, t0
    returni(ctx, t0)
end)

wasmOp(f32_min, WasmF32Min, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)

    bfeq ft0, ft1, .equal
    bflt ft0, ft1, .lt
    bfgt ft0, ft1, .return

.NaN:
    addf ft0, ft1
    jmp .return

.equal:
    orf ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnf(ctx, ft1)
end)

wasmOp(f32_max, WasmF32Max, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)

    bfeq ft1, ft0, .equal
    bflt ft1, ft0, .lt
    bfgt ft1, ft0, .return

.NaN:
    addf ft0, ft1
    jmp .return

.equal:
    andf ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnf(ctx, ft1)
end)

wasmOp(f32_copysign, WasmF32Copysign, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)

    ff2i ft1, t1
    move 0x80000000, t2
    andi t2, t1

    ff2i ft0, t0
    move 0x7fffffff, t2
    andi t2, t0

    ori t1, t0
    fi2f t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f64_min, WasmF64Min, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)

    bdeq ft0, ft1, .equal
    bdlt ft0, ft1, .lt
    bdgt ft0, ft1, .return

.NaN:
    addd ft0, ft1
    jmp .return

.equal:
    ord ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnd(ctx, ft1)
end)

wasmOp(f64_max, WasmF64Max, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)

    bdeq ft1, ft0, .equal
    bdlt ft1, ft0, .lt
    bdgt ft1, ft0, .return

.NaN:
    addd ft0, ft1
    jmp .return

.equal:
    andd ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnd(ctx, ft1)
end)

wasmOp(f64_copysign, WasmF64Copysign, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)

    fd2q ft1, t1
    move 0x8000000000000000, t2
    andq t2, t1

    fd2q ft0, t0
    move 0x7fffffffffffffff, t2
    andq t2, t0

    orq t1, t0
    fq2d t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(f32_convert_u_i32, WasmF32ConvertUI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2f t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f64_convert_u_i32, WasmF64ConvertUI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2d t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(i32_add, WasmI32Add, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    addi t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_sub, WasmI32Sub, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    subi t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_mul, WasmI32Mul, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    muli t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_and, WasmI32And, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    andi t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_or, WasmI32Or, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    ori t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_xor, WasmI32Xor, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    xori t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_shl, WasmI32Shl, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    lshifti t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_shr_u, WasmI32ShrU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    urshifti t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_shr_s, WasmI32ShrS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    rshifti t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_rotr, WasmI32Rotr, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    rrotatei t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_rotl, WasmI32Rotl, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    lrotatei t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_eq, WasmI32Eq, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cieq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_ne, WasmI32Ne, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cineq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_lt_s, WasmI32LtS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cilt t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_le_s, WasmI32LeS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cilteq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_lt_u, WasmI32LtU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cib t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_le_u, WasmI32LeU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cibeq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_gt_s, WasmI32GtS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cigt t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_ge_s, WasmI32GeS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cigteq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_gt_u, WasmI32GtU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cia t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_ge_u, WasmI32GeU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    ciaeq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i32_clz, WasmI32Clz, macro(ctx)
    mloadi(ctx, m_operand, t0)
    lzcnti t0, t1
    returni(ctx, t1)
end)

wasmOp(i64_add, WasmI64Add, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    addq t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_sub, WasmI64Sub, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    subq t1, t0
    returnq(ctx, t0)
end)

wasmOp(i64_mul, WasmI64Mul, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    mulq t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_and, WasmI64And, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    andq t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_or, WasmI64Or, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    orq t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_xor, WasmI64Xor, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    xorq t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_eq, WasmI64Eq, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqeq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_ne, WasmI64Ne, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqneq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_lt_s, WasmI64LtS, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqlt t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_le_s, WasmI64LeS, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqlteq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_lt_u, WasmI64LtU, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqb t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_le_u, WasmI64LeU, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqbeq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_gt_s, WasmI64GtS, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqgt t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_ge_s, WasmI64GeS, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqgteq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_gt_u, WasmI64GtU, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqa t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_ge_u, WasmI64GeU, macro(ctx)
    mloadq(ctx, m_lhs, t0)
    mloadq(ctx, m_rhs, t1)
    cqaeq t0, t1, t2
    andi 1, t2
    returni(ctx, t2)
end)

wasmOp(i64_clz, WasmI64Clz, macro(ctx)
    mloadq(ctx, m_operand, t0)
    lzcntq t0, t1
    returnq(ctx, t1)
end)

wasmOp(f32_add, WasmF32Add, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    addf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_sub, WasmF32Sub, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    subf ft1, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_mul, WasmF32Mul, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    mulf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_div, WasmF32Div, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    divf ft1, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_abs, WasmF32Abs, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    absf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_neg, WasmF32Neg, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    negf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_ceil, WasmF32Ceil, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    ceilf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_floor, WasmF32Floor, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    floorf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_sqrt, WasmF32Sqrt, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    sqrtf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_eq, WasmF32Eq, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfeq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_ne, WasmF32Ne, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfnequn ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_lt, WasmF32Lt, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cflt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_le, WasmF32Le, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cflteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_gt, WasmF32Gt, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfgt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_ge, WasmF32Ge, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfgteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_add, WasmF64Add, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    addd ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_sub, WasmF64Sub, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    subd ft1, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_mul, WasmF64Mul, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    muld ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_div, WasmF64Div, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    divd ft1, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_abs, WasmF64Abs, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    absd ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_neg, WasmF64Neg, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    negd ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_ceil, WasmF64Ceil, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    ceild ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_floor, WasmF64Floor, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    floord ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_sqrt, WasmF64Sqrt, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    sqrtd ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_eq, WasmF64Eq, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdeq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_ne, WasmF64Ne, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdnequn ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_lt, WasmF64Lt, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdlt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_le, WasmF64Le, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdlteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_gt, WasmF64Gt, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdgt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_ge, WasmF64Ge, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdgteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(i32_wrap_i64, WasmI32WrapI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    returni(ctx, t0)
end)

wasmOp(i64_extend_s_i32, WasmI64ExtendSI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    sxi2q t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_extend_u_i32, WasmI64ExtendUI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    zxi2q t0, t1
    returnq(ctx, t1)
end)

wasmOp(f32_convert_s_i32, WasmF32ConvertSI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2fs t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_convert_s_i64, WasmF32ConvertSI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    cq2fs t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_demote_f64, WasmF32DemoteF64, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    cd2f ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_reinterpret_i32, WasmF32ReinterpretI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    fi2f t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f64_convert_s_i32, WasmF64ConvertSI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2ds t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_convert_s_i64, WasmF64ConvertSI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    cq2ds t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_promote_f32, WasmF64PromoteF32, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    cf2d ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_reinterpret_i64, WasmF64ReinterpretI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    fq2d t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(i32_reinterpret_f32, WasmI32ReinterpretF32, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    ff2i ft0, t0
    returni(ctx, t0)
end)

wasmOp(i64_reinterpret_f64, WasmI64ReinterpretF64, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    fd2q ft0, t0
    returnq(ctx, t0)
end)

macro dropKeep(startOffset, drop, keep)
    lshifti 3, startOffset
    subp cfr, startOffset, startOffset
    negi drop
    sxi2q drop, drop

.copyLoop:
    btiz keep, .done
    loadq [startOffset, drop, 8], t6
    storeq t6, [startOffset]
    subi 1, keep
    subp 8, startOffset
    jmp .copyLoop

.done:
end

wasmOp(drop_keep, WasmDropKeep, macro(ctx)
    wgetu(ctx, m_startOffset, t0)
    wgetu(ctx, m_dropCount, t1)
    wgetu(ctx, m_keepCount, t2)

    dropKeep(t0, t1, t2)

    dispatch(ctx)
end)
