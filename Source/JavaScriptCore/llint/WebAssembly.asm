# Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
    const NumberOfWasmArgumentJSRs = 6
elsif ARM64 or ARM64E or RISCV64
    const NumberOfWasmArgumentJSRs = 8
elsif ARMv7
    const NumberOfWasmArgumentJSRs = 2
else
    error
end

const NumberOfWasmArgumentFPRs = 8

const NumberOfWasmArguments = NumberOfWasmArgumentJSRs + NumberOfWasmArgumentFPRs

# All callee saves must match the definition in WasmCallee.cpp

# These must match the definition in WasmMemoryInformation.cpp
if X86_64 or ARM64 or ARM64E or RISCV64
    const wasmInstance = csr0
    const memoryBase = csr3
    const boundsCheckingSize = csr4
elsif ARMv7
    const wasmInstance = csr0
    const memoryBase = invalidGPR
    const boundsCheckingSize = invalidGPR
else
    error
end

# This must match the definition in LowLevelInterpreter.asm
if X86_64
    const PB = csr2
elsif ARM64 or ARM64E or RISCV64
    const PB = csr7
elsif ARMv7
    const PB = csr1
else
    error
end

# Helper macros

# On JSVALUE64, each 64-bit argument GPR holds one whole Wasm value.
# On JSVALUE32_64, a consecutive pair of even/odd numbered GPRs hold a single
# Wasm value (even if that value is i32/f32, the odd numbered GPR holds the
# more significant word).
macro forEachArgumentJSR(fn)
    if ARM64 or ARM64E
        fn(0 * 8, wa0, wa1)
        fn(2 * 8, wa2, wa3)
        fn(4 * 8, wa4, wa5)
        fn(6 * 8, wa6, wa7)
    elsif JSVALUE64
        fn(0 * 8, wa0)
        fn(1 * 8, wa1)
        fn(2 * 8, wa2)
        fn(3 * 8, wa3)
        fn(4 * 8, wa4)
        fn(5 * 8, wa5)
    else
        fn(0 * 8, wa1, wa0)
        fn(1 * 8, wa3, wa2)
    end
end

macro forEachArgumentFPR(fn)
    if ARM64 or ARM64E
        fn((NumberOfWasmArgumentJSRs + 0) * 8, wfa0, wfa1)
        fn((NumberOfWasmArgumentJSRs + 2) * 8, wfa2, wfa3)
        fn((NumberOfWasmArgumentJSRs + 4) * 8, wfa4, wfa5)
        fn((NumberOfWasmArgumentJSRs + 6) * 8, wfa6, wfa7)
    else
        fn((NumberOfWasmArgumentJSRs + 0) * 8, wfa0)
        fn((NumberOfWasmArgumentJSRs + 1) * 8, wfa1)
        fn((NumberOfWasmArgumentJSRs + 2) * 8, wfa2)
        fn((NumberOfWasmArgumentJSRs + 3) * 8, wfa3)
        fn((NumberOfWasmArgumentJSRs + 4) * 8, wfa4)
        fn((NumberOfWasmArgumentJSRs + 5) * 8, wfa5)
        fn((NumberOfWasmArgumentJSRs + 6) * 8, wfa6)
        fn((NumberOfWasmArgumentJSRs + 7) * 8, wfa7)
    end
end

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
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro wasmNextInstructionWide16()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide16, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro wasmNextInstructionWide32()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide32, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro checkSwitchToJIT(increment, action)
    loadp CodeBlock[cfr], ws0
    baddis increment, Wasm::LLIntCallee::m_tierUpCounter + Wasm::LLIntTierUpCounter::m_counter[ws0], .continue
    action()
    .continue:
end

macro checkSwitchToJITForPrologue(codeBlockRegister)
    if WEBASSEMBLY_B3JIT
    checkSwitchToJIT(
        5,
        macro()
            move cfr, a0
            move PC, a1
            move wasmInstance, a2
            cCall4(_slow_path_wasm_prologue_osr)
            btpz r0, .recover
            move r0, ws0

if ARM64 or ARM64E
            forEachArgumentJSR(macro (offset, gpr1, gpr2)
                loadpairq -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr2, gpr1
            end)
elsif JSVALUE64
            forEachArgumentJSR(macro (offset, gpr)
                loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
            end)
else
            forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
                load2ia -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpLsw, gprMsw
            end)
end
if ARM64 or ARM64E
            forEachArgumentFPR(macro (offset, fpr1, fpr2)
                loadpaird -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr2, fpr1
            end)
else
            forEachArgumentFPR(macro (offset, fpr)
                loadd -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
            end)
end

            restoreCalleeSavesUsedByWasm()
            restoreCallerPCAndCFR()
            if ARM64E
                leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::wasmOSREntry) * PtrSize, ws1
                jmp [ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
            else
                jmp ws0, WasmEntryPtrTag
            end
        .recover:
            loadp CodeBlock[cfr], codeBlockRegister
        end)
    end
end

macro checkSwitchToJITForLoop()
    if WEBASSEMBLY_B3JIT
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
            move r0, a0
            if ARM64E
                move r1, ws0
                leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::wasmOSREntry) * PtrSize, ws1
                jmp [ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
            else
                jmp r1, WasmEntryPtrTag
            end
        .recover:
            loadi ArgumentCountIncludingThis + TagOffset[cfr], PC
        end)
    end
end

macro checkSwitchToJITForEpilogue()
    if WEBASSEMBLY_B3JIT
    checkSwitchToJIT(
        10,
        macro ()
            callWasmSlowPath(_slow_path_wasm_epilogue_osr)
        end)
    end
end

# Wasm specific helpers

macro preserveCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't save memoryBase and boundsCheckingSize here. See the comment
    # in restoreCalleeSavesUsedByWasm() below for why.
    subp CalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        storepairq wasmInstance, PB, -16[cfr]
    elsif X86_64 or RISCV64
        storep PB, -0x8[cfr]
        storep wasmInstance, -0x10[cfr]
    elsif ARMv7
        storep PB, -4[cfr]
        storep wasmInstance, -8[cfr]
    else
        error
    end
end

macro restoreCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't restore memoryBase and boundsCheckingSize here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    if ARM64 or ARM64E
        loadpairq -16[cfr], wasmInstance, PB
    elsif X86_64 or RISCV64
        loadp -0x8[cfr], PB
        loadp -0x10[cfr], wasmInstance
    elsif ARMv7
        loadp -4[cfr], PB
        loadp -8[cfr], wasmInstance
    else
        error
    end
end

macro loadWasmInstanceFromTLSTo(reg)
if  HAVE_FAST_TLS
    tls_loadp WTF_WASM_CONTEXT_KEY, reg
else
    crash()
end
end

macro loadWasmInstanceFromTLS()
if  HAVE_FAST_TLS
    loadWasmInstanceFromTLSTo(wasmInstance)
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
if not ARMv7
    loadp Wasm::Instance::m_cachedMemory[instance], memoryBase
    loadp Wasm::Instance::m_cachedBoundsCheckingSize[instance], boundsCheckingSize
    cagedPrimitiveMayBeNull(memoryBase, boundsCheckingSize, scratch1, scratch2) # If boundsCheckingSize is 0, pointer can be a nullptr.
end
end

macro throwException(exception)
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
end

macro callWasmSlowPath(slowPath)
    storei PC, ArgumentCountIncludingThis + TagOffset[cfr]
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

macro restoreStackPointerAfterCall()
    loadp CodeBlock[cfr], ws1
    loadi Wasm::LLIntCallee::m_numCalleeLocals[ws1], ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
if ARMv7
    subp cfr, ws1, ws1
    move ws1, sp
else
    subp cfr, ws1, sp
end
end

macro wasmPrologue(loadWasmInstance)
    # Set up the call frame and check if we should OSR.
    preserveCallerPCAndCFR()
    preserveCalleeSavesUsedByWasm()
    loadWasmInstance()
    reloadMemoryRegistersFromInstance(wasmInstance, ws0, ws1)

    loadp Wasm::Instance::m_owner[wasmInstance], ws0
    storep ws0, ThisArgumentOffset[cfr]
if not JSVALUE64
    storei CellTag, TagOffset + ThisArgumentOffset[cfr]
end

    loadp Callee[cfr], ws0
    andp ~3, ws0
    storep ws0, CodeBlock[cfr]

    # Get new sp in ws1 and check stack height.
    loadi Wasm::LLIntCallee::m_numCalleeLocals[ws0], ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
    subp cfr, ws1, ws1

if not JSVALUE64
    subp 8, ws1 # align stack pointer
end

    bpa ws1, cfr, .stackOverflow
    bpbeq Wasm::Instance::m_cachedStackLimit[wasmInstance], ws1, .stackHeightOK

.stackOverflow:
    throwException(StackOverflow)

.stackHeightOK:
    move ws1, sp

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr2, gpr1, -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
end
if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        storepaird fpr2, fpr1, -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
end

    checkSwitchToJITForPrologue(ws0)

    # Set up the PC.
    loadp Wasm::LLIntCallee::m_instructionsRawPointer[ws0], PB
    move 0, PC

    loadi Wasm::LLIntCallee::m_numVars[ws0], ws1
    subi NumberOfWasmArguments + CalleeSaveSpaceAsVirtualRegisters, ws1
    btiz ws1, .zeroInitializeLocalsDone
    lshifti 3, ws1
    negi ws1
if JSVALUE64
    sxi2q ws1, ws1
end
    leap (NumberOfWasmArguments + CalleeSaveSpaceAsVirtualRegisters + 1) * -8[cfr], ws0
.zeroInitializeLocalsLoop:
    addp PtrSize, ws1
    storep 0, [ws0, ws1]
    btpnz ws1, .zeroInitializeLocalsLoop
.zeroInitializeLocalsDone:
end

macro forEachVectorArgument(fn)
     const base = NumberOfWasmArgumentJSRs * 8
     fn(base + 0 * 16, -(base + 0 * 16 + 8), wfa0)
     fn(base + 1 * 16, -(base + 1 * 16 + 8), wfa1)
     fn(base + 2 * 16, -(base + 2 * 16 + 8), wfa2)
     fn(base + 3 * 16, -(base + 3 * 16 + 8), wfa3)
     fn(base + 4 * 16, -(base + 4 * 16 + 8), wfa4)
     fn(base + 5 * 16, -(base + 5 * 16 + 8), wfa5)
     fn(base + 6 * 16, -(base + 6 * 16 + 8), wfa6)
     fn(base + 7 * 16, -(base + 7 * 16 + 8), wfa7)
end

# Tier up immediately, while saving full vectors in argument FPRs
macro wasmPrologueSIMD(loadWasmInstance)
if WEBASSEMBLY_B3JIT
    preserveCallerPCAndCFR()
    preserveCalleeSavesUsedByWasm()
    loadWasmInstance()
    reloadMemoryRegistersFromInstance(wasmInstance, ws0, ws1)

    loadp Wasm::Instance::m_owner[wasmInstance], ws0
    storep ws0, ThisArgumentOffset[cfr]
if not JSVALUE64
    storei CellTag, TagOffset + ThisArgumentOffset[cfr]
end

    loadp Callee[cfr], ws0
    andp ~3, ws0
    storep ws0, CodeBlock[cfr]

    # Get new sp in ws1 and check stack height.
    # This should match the calculation of m_stackSize, but with double the size for fpr arg storage and no locals.
    move 8 + 8 * 2 + constexpr CallFrame::headerSizeInRegisters + 1, ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
    subp cfr, ws1, ws1

if not JSVALUE64
    subp 8, ws1 # align stack pointer
end

    bpa ws1, cfr, .stackOverflow
    bpbeq Wasm::Instance::m_cachedStackLimit[wasmInstance], ws1, .stackHeightOK

.stackOverflow:
    throwException(StackOverflow)

.stackHeightOK:
    move ws1, sp

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr2, gpr1, -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
end

    forEachVectorArgument(macro (offset, negOffset, fpr)
        storev fpr, negOffset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)

    move cfr, a0
    move PC, a1
    move wasmInstance, a2
    cCall4(_slow_path_wasm_simd_go_straight_to_bbq_osr)
    move r0, ws0

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr2, gpr1
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpLsw, gprMsw
    end)
end

    forEachVectorArgument(macro (offset, negOffset, fpr)
        loadv negOffset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
    end)

    restoreCalleeSavesUsedByWasm()
    restoreCallerPCAndCFR()
    if ARM64E
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::wasmOSREntry) * PtrSize, ws1
        jmp [ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
    else
        jmp ws0, WasmEntryPtrTag
    end
end
    break
end

macro traceExecution()
    if TRACING
        callWasmSlowPath(_slow_path_wasm_trace)
    end
end

macro commonWasmOp(opcodeName, opcodeStruct, prologue, fn)
    commonOp(opcodeName, prologue, macro(size)
        fn(macro(fn2)
            fn2(opcodeName, opcodeStruct, size)
        end)
    end)
end

# Less convenient, but required for opcodes that collide with reserved instructions (e.g. wasm_nop)
macro unprefixedWasmOp(opcodeName, opcodeStruct, fn)
    commonWasmOp(opcodeName, opcodeStruct, traceExecution, fn)
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

# Float to float rounding ops
macro wasmRoundingOp(opcodeName, opcodeStruct, fn)
if JSVALUE64 # All current 64-bit platforms have instructions for these
    wasmOp(opcodeName, opcodeStruct, fn)
else
    slowWasmOp(opcodeName)
end
end

# i64 (signed/unsigned) to f32 or f64
macro wasmI64ToFOp(opcodeName, opcodeStruct, fn)
if JSVALUE64 # All current 64-bit platforms have instructions for these
    wasmOp(opcodeName, opcodeStruct, fn)
else
    slowWasmOp(opcodeName)
end
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
        loadp Wasm::LLIntCallee::m_constants[t6], t6
        subp firstConstantIndex, index
        loader((constexpr (Int64FixedVector::Storage::offsetOfData()))[t6, index, 8])
    .done:
    end)
end

if JSVALUE64
macro mloadq(ctx, field, dst)
    wgets(ctx, field, dst)
    loadConstantOrVariable(ctx, dst, macro (from)
        loadq from, dst
    end)
end
else
macro mload2i(ctx, field, dstMsw, dstLsw)
    wgets(ctx, field, dstLsw)
    loadConstantOrVariable(ctx, dstLsw, macro (from)
        load2ia from, dstLsw, dstMsw
    end)
end
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

if JSVALUE64
macro returnq(ctx, value)
    wgets(ctx, m_dst, t5)
    storeq value, [cfr, t5, 8]
    dispatch(ctx)
end
else
macro return2i(ctx, msw, lsw)
    wgets(ctx, m_dst, t5)
    store2ia lsw, msw, [cfr, t5, 8]
    dispatch(ctx)
end
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
        size(getOperandNarrow, getOperandWide16Wasm, getOperandWide32Wasm, macro (get)
            get(opcodeStruct, field, dst)
        end)
    end)
end

macro wgetu(ctx, field, dst)
    ctx(macro(opcodeName, opcodeStruct, size)
        size(getuOperandNarrow, getuOperandWide16Wasm, getuOperandWide32Wasm, macro (getu)
            getu(opcodeStruct, field, dst)
        end)
    end)
end

# Control flow helpers

macro dispatch(ctx)
    ctx(macro(opcodeName, opcodeStruct, size)
        genericDispatchOpWasm(wasmDispatch, size, opcodeName)
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
    if ARM64E
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::returnFromLLInt) * PtrSize, ws0
        jmp [ws0], NativeToJITGatePtrTag
    else
        ret
    end
end

# Entry point

macro wasmCodeBlockGetter(targetRegister)
    loadp Callee[cfr], targetRegister
    andp ~3, targetRegister
end

op(wasm_function_prologue, macro ()
    if not WEBASSEMBLY or C_LOOP or C_LOOP_WIN
        error
    end

    wasmPrologue(loadWasmInstanceFromTLS)
    wasmNextInstruction()
end)

op(wasm_function_prologue_no_tls, macro ()
    if not WEBASSEMBLY or C_LOOP or C_LOOP_WIN
        error
    end

    wasmPrologue(macro () end)
    wasmNextInstruction()
end)

op(wasm_function_prologue_simd, macro ()
    if not WEBASSEMBLY or C_LOOP or C_LOOP_WIN
        error
    end

    wasmPrologueSIMD(loadWasmInstanceFromTLS)
    break
end)

op(wasm_function_prologue_no_tls_simd, macro ()
    if not WEBASSEMBLY or C_LOOP or C_LOOP_WIN
        error
    end

    wasmPrologueSIMD(macro () end)
    break
end)

macro jumpToException()
    if ARM64E
        move r0, a0
        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::exceptionHandler) * PtrSize, a1
        jmp [a1], NativeToJITGatePtrTag # ExceptionHandlerPtrTag
    else
        jmp r0, ExceptionHandlerPtrTag
    end
end

op(wasm_throw_from_slow_path_trampoline, macro ()
    loadp Wasm::Instance::m_pointerToTopEntryFrame[wasmInstance], t5
    loadp [t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    move cfr, a0
    addp PB, PC, a1
    move wasmInstance, a2
    # Slow paths and the throwException macro store the exception code in the ArgumentCountIncludingThis slot
    loadi ArgumentCountIncludingThis + PayloadOffset[cfr], a3
    storei 0, ArgumentCountIncludingThis + TagOffset[cfr]
    cCall4(_slow_path_wasm_throw_exception)
    jumpToException()
end)

macro wasm_throw_from_fault_handler(instance)
    # instance should be in a2 when we get here
    loadp Wasm::Instance::m_pointerToTopEntryFrame[instance], a0
    loadp [a0], a0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(a0)

    move constexpr Wasm::ExceptionType::OutOfBoundsMemoryAccess, a3
    move 0, a1
    move cfr, a0
    storei 0, ArgumentCountIncludingThis + TagOffset[cfr]
    cCall4(_slow_path_wasm_throw_exception)
    jumpToException()
end

op(wasm_throw_from_fault_handler_trampoline_fastTLS, macro ()
    loadWasmInstanceFromTLSTo(a2)
    wasm_throw_from_fault_handler(a2)
end)

op(wasm_throw_from_fault_handler_trampoline_reg_instance, macro ()
    move wasmInstance, a2
    wasm_throw_from_fault_handler(a2)
end)

# Disable wide version of narrow-only opcodes
noWide(wasm_enter)
noWide(wasm_wide16)
noWide(wasm_wide32)

# Opcodes that always invoke the slow path

slowWasmOp(ref_func)
slowWasmOp(table_get)
slowWasmOp(table_set)
slowWasmOp(table_init)
slowWasmOp(elem_drop)
slowWasmOp(table_size)
slowWasmOp(table_fill)
slowWasmOp(table_copy)
slowWasmOp(table_grow)
slowWasmOp(memory_fill)
slowWasmOp(memory_copy)
slowWasmOp(memory_init)
slowWasmOp(data_drop)
slowWasmOp(set_global_ref)
slowWasmOp(set_global_ref_portable_binding)
slowWasmOp(memory_atomic_wait32)
slowWasmOp(memory_atomic_wait64)
slowWasmOp(memory_atomic_notify)
slowWasmOp(array_new)
slowWasmOp(array_new_default)
slowWasmOp(array_get)
slowWasmOp(array_set)
slowWasmOp(struct_new)
slowWasmOp(struct_get)
slowWasmOp(struct_set)

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
    loadi Wasm::LLIntCallee::m_numVars[t2], t2      // t2<size_t> = t2<CodeBlock>.m_numVars
    subi CalleeSaveSpaceAsVirtualRegisters + NumberOfWasmArguments, t2
    btiz t2, .opEnterDone
    subp cfr, (CalleeSaveSpaceAsVirtualRegisters + NumberOfWasmArguments) * SlotSize, t1
    lshifti 3, t2
    negi t2
if JSVALUE64
    sxi2q t2, t2
end
    move 0, t6
.opEnterLoop:
if JSVALUE64
    storeq t6, [t1, t2]
else
    store2ia t6, t6, [t1, t2]
end
    addp 8, t2
    btpnz t2, .opEnterLoop
.opEnterDone:
    wasmDispatchIndirect(1)

unprefixedWasmOp(wasm_nop, WasmNop, macro(ctx)
    dispatch(ctx)
end)

wasmOp(loop_hint, WasmLoopHint, macro(ctx)
    checkSwitchToJITForLoop()
    dispatch(ctx)
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
    loadp Wasm::LLIntCallee::m_jumpTables[t2], t2
    muli sizeof Wasm::JumpTable, t1
    addp t1, t2

    loadp (constexpr (WasmJumpTableFixedVector::Storage::offsetOfData()))[t2], t2
    loadi Wasm::JumpTable::Storage::m_size[t2], t3
    bib t0, t3, .inBounds

.outOfBounds:
    subi t3, 1, t0

.inBounds:
    muli sizeof Wasm::JumpTableEntry, t0

    loadi (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::startOffset[t2, t0], t1
    loadi (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::dropCount[t2, t0], t3
    loadi (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::keepCount[t2, t0], t5
    dropKeep(t1, t3, t5)

    loadis (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::target[t2, t0], t3
    assert(macro(ok) btinz t3, .ok end)
    wasmDispatchIndirect(t3)
end)

unprefixedWasmOp(wasm_jmp, WasmJmp, macro(ctx)
    jump(ctx, m_targetLabel)
end)

unprefixedWasmOp(wasm_ret, WasmRet, macro(ctx)
    checkSwitchToJITForEpilogue()
if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr2, gpr1
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpLsw, gprMsw
    end)
end
if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        loadpaird -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr2, fpr1
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        loadd -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
    end)
end
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
if ARMv7
            subp cfr, ws1, ws1
            move ws1, sp
else
            subp cfr, ws1, sp
end

            wgetu(ctx, m_numberOfStackArgs, ws1)

            # Preserve the current instance
            move wasmInstance, PB

            storeWasmInstance(targetWasmInstance)
            reloadMemoryRegistersFromInstance(targetWasmInstance, wa0, wa1)

            # Load registers from stack
if ARM64 or ARM64E
            leap [sp, ws1, 8], ws1
            forEachArgumentJSR(macro (offset, gpr1, gpr2)
                loadpairq CallFrameHeaderSize + 8 + offset[ws1], gpr1, gpr2
            end)
elsif JSVALUE64
            forEachArgumentJSR(macro (offset, gpr)
                loadq CallFrameHeaderSize + 8 + offset[sp, ws1, 8], gpr
            end)
else
            forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
                load2ia CallFrameHeaderSize + 8 + offset[sp, ws1, 8], gpLsw, gprMsw
            end)
end
if ARM64 or ARM64E
            forEachArgumentFPR(macro (offset, fpr1, fpr2)
                loadpaird CallFrameHeaderSize + 8 + offset[ws1], fpr1, fpr2
            end)
else
            forEachArgumentFPR(macro (offset, fpr)
                loadd CallFrameHeaderSize + 8 + offset[sp, ws1, 8], fpr
            end)
end

            addp CallerFrameAndPCSize, sp

            ctx(macro(opcodeName, opcodeStruct, size)
                macro callNarrow()
                    if ARM64E
                        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::%opcodeName%) * PtrSize, ws1
                        jmp [ws1], NativeToJITGatePtrTag # JSEntrySlowPathPtrTag
                    end
                    _wasm_trampoline_%opcodeName%:
                    call ws0, JSEntrySlowPathPtrTag
                end

                macro callWide16()
                    if ARM64E
                        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::%opcodeName%_wide16) * PtrSize, ws1
                        jmp [ws1], NativeToJITGatePtrTag # JSEntrySlowPathPtrTag
                    end
                    _wasm_trampoline_%opcodeName%_wide16:
                    call ws0, JSEntrySlowPathPtrTag
                end

                macro callWide32()
                    if ARM64E
                        leap JSCConfig + constexpr JSC::offsetOfJSCConfigGateMap + (constexpr Gate::%opcodeName%_wide32) * PtrSize, ws1
                        jmp [ws1], NativeToJITGatePtrTag # JSEntrySlowPathPtrTag
                    end
                    _wasm_trampoline_%opcodeName%_wide32:
                    call ws0, JSEntrySlowPathPtrTag
                end

                size(callNarrow, callWide16, callWide32, macro (gen) gen() end)
                defineReturnLabel(opcodeName, size)
            end)

            restoreStackPointerAfterCall()

            # We need to set PC to load information from the instruction stream, but we
            # need to preserve its current value since it might contain a return value
if ARMv7
            push PC
else
            move PC, memoryBase
end
            move PB, wasmInstance
            loadi ArgumentCountIncludingThis + TagOffset[cfr], PC
            loadp CodeBlock[cfr], PB
            loadp Wasm::LLIntCallee::m_instructionsRawPointer[PB], PB

            wgetu(ctx, m_stackOffset, ws1)
            lshifti 3, ws1
            negi ws1
if JSVALUE64
            sxi2q ws1, ws1
end
            addp cfr, ws1

            # Argument registers are also return registers, so they must be stored to the stack
            # in case they contain return values.
            wgetu(ctx, m_numberOfStackArgs, ws0)
if ARMv7
            pop PC
else
            move memoryBase, PC
end
if ARM64 or ARM64E
            leap [ws1, ws0, 8], ws1
            forEachArgumentJSR(macro (offset, gpr1, gpr2)
                storepairq gpr1, gpr2, CallFrameHeaderSize + 8 + offset[ws1]
            end)
elsif JSVALUE64
            forEachArgumentJSR(macro (offset, gpr)
                storeq gpr, CallFrameHeaderSize + 8 + offset[ws1, ws0, 8]
            end)
else
            forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
                store2ia gpLsw, gprMsw, CallFrameHeaderSize + 8 + offset[ws1, ws0, 8]
            end)
end
if ARM64 or ARM64E
            forEachArgumentFPR(macro (offset, fpr1, fpr2)
                storepaird fpr1, fpr2, CallFrameHeaderSize + 8 + offset[ws1]
            end)
else
            forEachArgumentFPR(macro (offset, fpr)
                stored fpr, CallFrameHeaderSize + 8 + offset[ws1, ws0, 8]
            end)
end

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

wasmOp(call_ref, WasmCallRef, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call_ref, storeWasmInstanceToTLS)
end)

wasmOp(call_ref_no_tls, WasmCallRefNoTls, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call_ref_no_tls, macro(targetInstance) move targetInstance, wasmInstance end)
end)

wasmOp(current_memory, WasmCurrentMemory, macro(ctx)
    loadp Wasm::Instance::m_memory[wasmInstance], t0
    loadp Wasm::Memory::m_handle[t0], t0
    loadp BufferMemoryHandle::m_size[t0], t0
    urshiftp 16, t0
if JSVALUE64
    returnq(ctx, t0)
else
    return2i(ctx, 0, t0)
end
end)

wasmOp(select, WasmSelect, macro(ctx)
    mloadi(ctx, m_condition, t0)
    btiz t0, .isZero
if JSVALUE64
    mloadq(ctx, m_nonZero, t0)
    returnq(ctx, t0)
.isZero:
    mloadq(ctx, m_zero, t0)
    returnq(ctx, t0)
else
    mload2i(ctx, m_nonZero, t1, t0)
    return2i(ctx, t1, t0)
.isZero:
    mload2i(ctx, m_zero, t1, t0)
    return2i(ctx, t1, t0)
end
end)

# Opcodes that don't have the `b3op` entry in wasm.json. This should be kept in sync

wasmOp(i32_ctz, WasmI32Ctz, macro (ctx)
    mloadi(ctx, m_operand, t0)
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

wasmRoundingOp(f32_trunc, WasmF32Trunc, macro (ctx)
    mloadf(ctx, m_operand, ft0)
    truncatef ft0, ft0
    returnf(ctx, ft0)
end)

wasmRoundingOp(f32_nearest, WasmF32Nearest, macro (ctx)
    mloadf(ctx, m_operand, ft0)
    roundf ft0, ft0
    returnf(ctx, ft0)
end)

wasmRoundingOp(f64_trunc, WasmF64Trunc, macro (ctx)
    mloadd(ctx, m_operand, ft0)
    truncated ft0, ft0
    returnd(ctx, ft0)
end)

wasmRoundingOp(f64_nearest, WasmF64Nearest, macro (ctx)
    mloadd(ctx, m_operand, ft0)
    roundd ft0, ft0
    returnd(ctx, ft0)
end)

wasmOp(i32_trunc_s_f32, WasmI32TruncSF32, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
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

wasmOp(i32_trunc_sat_f32_s, WasmI32TruncSatF32S, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncatef2is ft0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    returni(ctx, t0)

.outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    returni(ctx, t0)
end)

wasmOp(i32_trunc_sat_f32_u, WasmI32TruncSatF32U, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .outOfBoundsTruncSatMin

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncatef2i ft0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMin:
    move 0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    returni(ctx, t0)
end)

wasmI64ToFOp(f32_convert_u_i64, WasmF32ConvertUI64, macro (ctx)
    mloadq(ctx, m_operand, t0)
    if X86_64
        cq2f t0, t1, ft0
    else
        cq2f t0, ft0
    end
    returnf(ctx, ft0)
end)

wasmI64ToFOp(f64_convert_u_i64, WasmF64ConvertUI64, macro (ctx)
    mloadq(ctx, m_operand, t0)
    if X86_64
        cq2d t0, t1, ft0
    else
        cq2d t0, ft0
    end
    returnd(ctx, ft0)
end)

wasmOp(i32_eqz, WasmI32Eqz, macro(ctx)
    mloadi(ctx, m_operand, t0)
    cieq t0, 0, t0
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

wasmRoundingOp(f32_ceil, WasmF32Ceil, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    ceilf ft0, ft1
    returnf(ctx, ft1)
end)

wasmRoundingOp(f32_floor, WasmF32Floor, macro(ctx)
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

wasmRoundingOp(f64_ceil, WasmF64Ceil, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    ceild ft0, ft1
    returnd(ctx, ft1)
end)

wasmRoundingOp(f64_floor, WasmF64Floor, macro(ctx)
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
    mloadi(ctx, m_operand, t0)
    returni(ctx, t0)
end)

wasmOp(i32_extend8_s, WasmI32Extend8S, macro(ctx)
    mloadi(ctx, m_operand, t0)
    sxb2i t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_extend16_s, WasmI32Extend16S, macro(ctx)
    mloadi(ctx, m_operand, t0)
    sxh2i t0, t1
    returni(ctx, t1)
end)

wasmOp(f32_convert_s_i32, WasmF32ConvertSI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2fs t0, ft0
    returnf(ctx, ft0)
end)

wasmI64ToFOp(f32_convert_s_i64, WasmF32ConvertSI64, macro(ctx)
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

wasmI64ToFOp(f64_convert_s_i64, WasmF64ConvertSI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    cq2ds t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_promote_f32, WasmF64PromoteF32, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    cf2d ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(i32_reinterpret_f32, WasmI32ReinterpretF32, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    ff2i ft0, t0
    returni(ctx, t0)
end)

macro dropKeep(startOffset, drop, keep)
    lshifti 3, startOffset
    subp cfr, startOffset, startOffset
    negi drop
if JSVALUE64
    sxi2q drop, drop
end

.copyLoop:
    btiz keep, .done
if JSVALUE64
    loadq [startOffset, drop, 8], t6
    storeq t6, [startOffset]
else
    load2ia [startOffset, drop, 8], t5, t6
    store2ia t5, t6, [startOffset]
end
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

wasmOp(atomic_fence, WasmDropKeep, macro(ctx)
    fence
    dispatch(ctx)
end)

wasmOp(throw, WasmThrow, macro(ctx)
    loadp Wasm::Instance::m_pointerToTopEntryFrame[wasmInstance], t5
    loadp [t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    callWasmSlowPath(_slow_path_wasm_throw)
    jumpToException()
end)

wasmOp(rethrow, WasmRethrow, macro(ctx)
    loadp Wasm::Instance::m_pointerToTopEntryFrame[wasmInstance], t5
    loadp [t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    callWasmSlowPath(_slow_path_wasm_rethrow)
    jumpToException()
end)

macro commonCatchImpl(ctx, storeWasmInstance)
    loadp Callee[cfr], t3
    convertCalleeToVM(t3)
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)

    loadp VM::calleeForWasmCatch + PayloadOffset[t3], ws1
    storep 0, VM::calleeForWasmCatch + PayloadOffset[t3]
    storep ws1, Callee + PayloadOffset[cfr]
if not JSVALUE64
    loadi VM::calleeForWasmCatch + TagOffset[t3], ws1
    storei EmptyValueTag, VM::calleeForWasmCatch + TagOffset[t3]
    storei ws1, Callee + TagOffset[cfr]
end

    loadp VM::callFrameForCatch[t3], cfr
    storep 0, VM::callFrameForCatch[t3]

    restoreStackPointerAfterCall()

    loadp ThisArgumentOffset[cfr], wasmInstance
    loadp JSWebAssemblyInstance::m_instance[wasmInstance], wasmInstance
    storeWasmInstance(wasmInstance)
    reloadMemoryRegistersFromInstance(wasmInstance, ws0, ws1)

    loadp CodeBlock[cfr], PB
    loadp Wasm::LLIntCallee::m_instructionsRawPointer[PB], PB
    loadp VM::targetInterpreterPCForThrow[t3], PC
    subp PB, PC

    callWasmSlowPath(_slow_path_wasm_retrieve_and_clear_exception)
end

macro catchAllImpl(ctx, storeWasmInstance)
    commonCatchImpl(ctx, storeWasmInstance)
    traceExecution()
    dispatch(ctx)
end

macro catchImpl(ctx, storeWasmInstance)
    commonCatchImpl(ctx, storeWasmInstance)

    move r1, t1

    wgetu(ctx, m_startOffset, t2)
    wgetu(ctx, m_argumentCount, t3)

    lshifti 3, t2
    subp cfr, t2, t2

.copyLoop:
    btiz t3, .done
if JSVALUE64
    loadq [t1], t6
    storeq t6, [t2]
else
    load2ia [t1], t5, t6
    store2ia t5, t6, [t2]
end
    subi 1, t3
    # FIXME: Use arm store-add/sub instructions in wasm LLInt catch
    # https://bugs.webkit.org/show_bug.cgi?id=231210
    subp 8, t2
    addp 8, t1
    jmp .copyLoop

.done:
    traceExecution()
    dispatch(ctx)
end

commonWasmOp(wasm_catch, WasmCatch, macro() end, macro(ctx)
    catchImpl(ctx, storeWasmInstanceToTLS)
end)

commonWasmOp(wasm_catch_no_tls, WasmCatch, macro() end, macro(ctx)
    catchImpl(ctx, macro(instance) end)
end)

commonWasmOp(wasm_catch_all, WasmCatchAll, macro() end, macro(ctx)
    catchAllImpl(ctx, storeWasmInstanceToTLS)
end)

commonWasmOp(wasm_catch_all_no_tls, WasmCatchAll, macro() end, macro(ctx)
    catchAllImpl(ctx, macro(instance) end)
end)

# Value-representation-specific code.
if JSVALUE64
    include WebAssembly64
else
    include WebAssembly32_64
end
