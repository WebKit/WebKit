# Copyright (C) 2011-2021 Apple Inc. All rights reserved.
# Copyright (C) 2021 Igalia S.L. All rights reserved.
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

# Constants

const MswOffset = 4
const LswOffset = 0

# Opcodes that should eventually be shared with JS llint

wasmOp(mov, WasmMov, macro(ctx)
    mload2i(ctx, m_src, t1, t0)
    return2i(ctx, t1, t0)
end)

# Wasm specific bytecodes

macro emitCheckAndPreparePointerAddingOffset(pointer, offset, size)
    # This macro updates 'pointer' to target address, and may thrash 'offset'
if ARMv7
    # Not enough registers on arm to keep the memory base and size in pinned
    # registers, so load them on each access instead. FIXME: improve this.
    addps offset, pointer
    bcs .throw
    addps size - 1, pointer, offset # Use offset as scratch register
    bcs .throw
    bpb offset, Wasm::Instance::m_cachedBoundsCheckingSize[wasmInstance], .continuation
.throw:
    throwException(OutOfBoundsMemoryAccess)
.continuation:
    addp Wasm::Instance::m_cachedMemory[wasmInstance], pointer
else
    crash()
end
end

macro emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(pointer, offset, size)
    # This macro updates 'pointer' to target address, and may thrash 'offset'
if ARMv7
    # Not enough registers on arm to keep the memory base and size in pinned
    # registers, so load them on each access instead. FIXME: improve this.
    addps offset, pointer
    bcs .throw
    addps size - 1, pointer, offset # Use offset as scratch register
    bcs .throw
    bpb offset, Wasm::Instance::m_cachedBoundsCheckingSize[wasmInstance], .continuation
.throw:
    throwException(OutOfBoundsMemoryAccess)
.continuation:
    addp Wasm::Instance::m_cachedMemory[wasmInstance], pointer
    btpnz pointer, (size - 1), .throw
else
    crash()
end
end

macro wasmLoadOp(name, struct, size, fn)
    wasmOp(name, struct, macro(ctx)
        mloadi(ctx, m_pointer, t0)
        wgetu(ctx, m_offset, t1)
        emitCheckAndPreparePointerAddingOffset(t0, t1, size)
        fn(t0, t3, t2)
        return2i(ctx, t3, t2)
    end)
end

wasmLoadOp(load8_u, WasmLoad8U, 1, macro(addr, dstMsw, dstLsw)
    loadb LswOffset[addr], dstLsw
    move 0, dstMsw
end)
wasmLoadOp(load16_u, WasmLoad16U, 2, macro(addr, dstMsw, dstLsw)
    loadh LswOffset[addr], dstLsw
    move 0, dstMsw
end)
wasmLoadOp(load32_u, WasmLoad32U, 4, macro(addr, dstMsw, dstLsw)
    loadi LswOffset[addr], dstLsw
    move 0, dstMsw
end)
wasmLoadOp(load64_u, WasmLoad64U, 8, macro(addr, dstMsw, dstLsw)
    # Might be unaligned, so can't use load2ia
    loadi LswOffset[addr], dstLsw
    loadi MswOffset[addr], dstMsw
end)

wasmLoadOp(i32_load8_s, WasmI32Load8S, 1, macro(addr, dstMsw, dstLsw)
    loadbsi LswOffset[addr], dstLsw
    move 0, dstMsw
end)
wasmLoadOp(i32_load16_s, WasmI32Load16S, 2, macro(addr, dstMsw, dstLsw)
    loadhsi LswOffset[addr], dstLsw
    move 0, dstMsw
end)
wasmLoadOp(i64_load8_s, WasmI64Load8S, 1, macro(addr, dstMsw, dstLsw)
    loadbsi LswOffset[addr], dstLsw
    rshifti dstLsw, 31, dstMsw
end)
wasmLoadOp(i64_load16_s, WasmI64Load16S, 2, macro(addr, dstMsw, dstLsw)
    loadhsi LswOffset[addr], dstLsw
    rshifti dstLsw, 31, dstMsw
end)
wasmLoadOp(i64_load32_s, WasmI64Load32S, 4, macro(addr, dstMsw, dstLsw)
    loadi LswOffset[addr], dstLsw
    rshifti dstLsw, 31, dstMsw
end)

macro wasmStoreOp(name, struct, size, fn)
    wasmOp(name, struct, macro(ctx)
        mloadi(ctx, m_pointer, t0)
        wgetu(ctx, m_offset, t1)
        emitCheckAndPreparePointerAddingOffset(t0, t1, size)
        mload2i(ctx, m_value, t3, t2)
        fn(t3, t2, t0)
        dispatch(ctx)
    end)
end

wasmStoreOp(store8, WasmStore8, 1, macro(srcMsw, srcLsw, addr)
    storeb srcLsw, LswOffset[addr]
end)
wasmStoreOp(store16, WasmStore16, 2, macro(srcMsw, srcLsw, addr)
    storeh srcLsw, LswOffset[addr]
end)
wasmStoreOp(store32, WasmStore32, 4, macro(srcMsw, srcLsw, addr)
    storei srcLsw, LswOffset[addr]
end)
wasmStoreOp(store64, WasmStore64, 8, macro(srcMsw, srcLsw, addr)
    # Might be unaligned, so can't use store2ia
    storei srcLsw, LswOffset[addr]
    storei srcMsw, MswOffset[addr]
end)

wasmOp(ref_is_null, WasmRefIsNull, macro(ctx)
    mload2i(ctx, m_ref, t1, t0) # t0 is unused
    cieq t1, NullTag, t0
    returni(ctx, t0)
end)

wasmOp(get_global, WasmGetGlobal, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    load2ia [t0, t1, 8], t0, t1
    return2i(ctx, t1, t0)
end)

wasmOp(set_global, WasmSetGlobal, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    mload2i(ctx, m_value, t3, t2)
    store2ia t2, t3, [t0, t1, 8]
    dispatch(ctx)
end)

wasmOp(get_global_portable_binding, WasmGetGlobalPortableBinding, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    loadp [t0, t1, 8], t0
    load2ia [t0], t0, t1
    return2i(ctx, t1, t0)
end)

wasmOp(set_global_portable_binding, WasmSetGlobalPortableBinding, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    mload2i(ctx, m_value, t3, t2)
    loadp [t0, t1, 8], t0
    store2ia t2, t3, [t0]
    dispatch(ctx)
end)

# Opcodes that don't have the `b3op` entry in wasm.json. This should be kept in sync

macro callDivRem(fn)
if ARMv7
    subp StackAlignment, sp
    storep PC, [sp]
    cCall4(fn)
    loadp [sp], PC
    addp StackAlignment, sp
else
    error
end
end

# i32 binary ops

wasmOp(i32_div_s, WasmI32DivS, macro (ctx)
    mloadi(ctx, m_lhs, a0)
    mloadi(ctx, m_rhs, a1)

    btiz a1, .throwDivisionByZero

    bineq a1, -1, .safe
    bieq a0, constexpr INT32_MIN, .throwIntegerOverflow

.safe:
    callDivRem(_slow_path_wasm_i32_div_s)
    returni(ctx, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)

.throwIntegerOverflow:
    throwException(IntegerOverflow)
end)

wasmOp(i32_div_u, WasmI32DivU, macro (ctx)
    mloadi(ctx, m_lhs, a0)
    mloadi(ctx, m_rhs, a1)

    btiz a1, .throwDivisionByZero

    callDivRem(_slow_path_wasm_i32_div_u)
    returni(ctx, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i32_rem_s, WasmI32RemS, macro (ctx)
    mloadi(ctx, m_lhs, a0)
    mloadi(ctx, m_rhs, a1)

    btiz a1, .throwDivisionByZero

    bineq a1, -1, .safe
    bineq a0, constexpr INT32_MIN, .safe

    move 0, r0
    jmp .return

.safe:
    callDivRem(_slow_path_wasm_i32_rem_s)

.return:
    returni(ctx, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i32_rem_u, WasmI32RemU, macro (ctx)
    mloadi(ctx, m_lhs, a0)
    mloadi(ctx, m_rhs, a1)

    btiz a1, .throwDivisionByZero

    callDivRem(_slow_path_wasm_i32_rem_u)
    returni(ctx, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

# i64 binary ops

wasmOp(i64_add, WasmI64Add, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    addis t2, t0
    adci  t3, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_sub, WasmI64Sub, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    subis t2, t0
    sbci  t3, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_mul, WasmI64Mul, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    muli t2, t1
    muli t0, t3
    umulli t0, t2, t0, t2
    addi t1, t2
    addi t3, t2
    return2i(ctx, t2, t0)
end)

wasmOp(i64_div_s, WasmI64DivS, macro (ctx)
    mload2i(ctx, m_lhs, a1, a0)
    mload2i(ctx, m_rhs, a3, a2)

    btinz a3, .nonZeroDivisor
    btiz a2, .throwDivisionByZero

.nonZeroDivisor:
    bineq a3, -1, .safe
    bineq a2, -1, .safe
    bineq a1, constexpr INT32_MIN, .safe
    btiz a0, .throwIntegerOverflow

.safe:
    callDivRem(_slow_path_wasm_i64_div_s)
    return2i(ctx, r1, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)

.throwIntegerOverflow:
    throwException(IntegerOverflow)
end)

wasmOp(i64_div_u, WasmI64DivU, macro (ctx)
    mload2i(ctx, m_lhs, a1, a0)
    mload2i(ctx, m_rhs, a3, a2)

    btinz a3, .nonZeroDivisor
    btiz a2, .throwDivisionByZero

.nonZeroDivisor:
    callDivRem(_slow_path_wasm_i64_div_u)
    return2i(ctx, r1, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i64_rem_s, WasmI64RemS, macro (ctx)
    mload2i(ctx, m_lhs, a1, a0)
    mload2i(ctx, m_rhs, a3, a2)

    btinz a3, .nonZeroDivisor
    btiz a2, .throwDivisionByZero

.nonZeroDivisor:
    bineq a3, -1, .safe
    bineq a2, -1, .safe
    bineq a1, constexpr INT32_MIN, .safe
    btinz a0, .safe

    move 0, r1
    move 0, r0
    jmp .return

.safe:
    callDivRem(_slow_path_wasm_i64_rem_s)

.return:
    return2i(ctx, r1, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i64_rem_u, WasmI64RemU, macro (ctx)
    mload2i(ctx, m_lhs, a1, a0)
    mload2i(ctx, m_rhs, a3, a2)

    btinz a3, .nonZeroDivisor
    btiz a2, .throwDivisionByZero

.nonZeroDivisor:
    callDivRem(_slow_path_wasm_i64_rem_u)
    return2i(ctx, r1, r0)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

wasmOp(i64_and, WasmI64And, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    andi t2, t0
    andi t3, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_or, WasmI64Or, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    ori t2, t0
    ori t3, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_xor, WasmI64Xor, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    xori t2, t0
    xori t3, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_shl, WasmI64Shl, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mloadi(ctx, m_rhs, t2)

    andi 0x3f, t2
    btiz t2, .return
    bib t2, 32, .lessThan32

    subi 32, t2
    lshifti t0, t2, t1
    move 0, t0
    jmp .return

.lessThan32:
    lshifti t2, t1
    move 32, t3
    subi t2, t3
    urshifti t0, t3, t3
    ori t3, t1
    lshifti t2, t0

.return:
    return2i(ctx, t1, t0)
end)

wasmOp(i64_shr_u, WasmI64ShrU, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mloadi(ctx, m_rhs, t2)

    andi 0x3f, t2
    btiz t2, .return
    bib t2, 32, .lessThan32

    subi 32, t2
    urshifti t1, t2, t0
    move 0, t1
    jmp .return

.lessThan32:
    urshifti t2, t0
    move 32, t3
    subi t2, t3
    lshifti t1, t3, t3
    ori t3, t0
    urshifti t2, t1

.return:
    return2i(ctx, t1, t0)
end)

wasmOp(i64_shr_s, WasmI64ShrS, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mloadi(ctx, m_rhs, t2)

    andi 0x3f, t2
    btiz t2, .return
    bib t2, 32, .lessThan32

    subi 32, t2
    rshifti t1, t2, t0
    rshifti 31, t1
    jmp .return

.lessThan32:
    urshifti t2, t0
    move 32, t3
    subi t2, t3
    lshifti t1, t3, t3
    ori t3, t0
    rshifti t2, t1

.return:
    return2i(ctx, t1, t0)
end)

wasmOp(i64_rotr, WasmI64Rotr, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mloadi(ctx, m_rhs, t2)

    andi t2, 0x20, t3
    btiz t3, .noSwap

    move t0, t3
    move t1, t0
    move t3, t1

.noSwap:
    andi 0x1f, t2
    btiz t2, .return

    move 32, t5
    subi t2, t5
    lshifti t0, t5, t3
    lshifti t1, t5, t5
    urshifti t2, t0
    urshifti t2, t1
    ori t5, t0
    ori t3, t1

.return:
    return2i(ctx, t1, t0)
end)

wasmOp(i64_rotl, WasmI64Rotl, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mloadi(ctx, m_rhs, t2)

    andi t2, 0x20, t3
    btiz t3, .noSwap

    move t0, t3
    move t1, t0
    move t3, t1

.noSwap:
    andi 0x1f, t2
    btiz t2, .return

    move 32, t5
    subi t2, t5
    urshifti t0, t5, t3
    urshifti t1, t5, t5
    lshifti t2, t0
    lshifti t2, t1
    ori t5, t0
    ori t3, t1

.return:
    return2i(ctx, t1, t0)
end)

wasmOp(i64_eq, WasmI64Eq, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 0, t6
    bineq t1, t3, .return
    cieq t0, t2, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_ne, WasmI64Ne, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bineq t1, t3, .return
    cineq t0, t2, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_lt_s, WasmI64LtS, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bilt t1, t3, .return
    move 0, t6
    bigt t1, t3, .return
    cib t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_le_s, WasmI64LeS, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bilt t1, t3, .return
    move 0, t6
    bigt t1, t3, .return
    cibeq t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_lt_u, WasmI64LtU, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bib t1, t3, .return
    move 0, t6
    bia t1, t3, .return
    cib t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_le_u, WasmI64LeU, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bib t1, t3, .return
    move 0, t6
    bia t1, t3, .return
    cibeq t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_gt_s, WasmI64GtS, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bigt t1, t3, .return
    move 0, t6
    bilt t1, t3, .return
    cia t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_ge_s, WasmI64GeS, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bigt t1, t3, .return
    move 0, t6
    bilt t1, t3, .return
    ciaeq t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_gt_u, WasmI64GtU, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 1, t6
    bia t1, t3, .return
    move 0, t6
    bib t1, t3, .return
    cia t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

wasmOp(i64_ge_u, WasmI64GeU, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2)
    move 0, t6
    bib t1, t3, .return
    move 1, t6
    bia t1, t3, .return
    ciaeq t0, t2, t6
    andi 1, t6
.return:
    returni(ctx, t6)
end)

# i64 unary ops

wasmOp(i64_ctz, WasmI64Ctz, macro (ctx)
    mload2i(ctx, m_operand, t1, t0)
    btiz t0, .top

    tzcnti t0, t0
    jmp .return

.top:
    tzcnti t1, t0
    addi 32, t0

.return:
    return2i(ctx, 0, t0)
end)

wasmOp(i64_clz, WasmI64Clz, macro(ctx)
    mload2i(ctx, m_operand, t1, t0)
    btiz t1, .bottom

    lzcnti t1, t0
    jmp .return

.bottom:
    lzcnti t0, t0
    addi 32, t0

.return:
    return2i(ctx, 0, t0)
end)

wasmOp(i64_popcnt, WasmI64Popcnt, macro (ctx)
    mload2i(ctx, m_operand, a3, a2)
    prepareStateForCCall()
    move PC, a0
    cCall2(_slow_path_wasm_popcountll)
    restoreStateAfterCCall()
    return2i(ctx, 0, r1)
end)

wasmOp(i64_eqz, WasmI64Eqz, macro(ctx)
    mload2i(ctx, m_operand, t1, t0)
    move 0, t2
    btinz t1, .return
    cieq t0, 0, t2
.return:
    returni(ctx, t2)
end)

# f64 binary ops

wasmOp(f64_copysign, WasmF64Copysign, macro(ctx)
    mload2i(ctx, m_lhs, t1, t0)
    mload2i(ctx, m_rhs, t3, t2) # t2 unused

    andi 0x7fffffff, t1
    andi 0x80000000, t3
    ori t3, t1

    return2i(ctx, t1, t0)
end)

# Type conversion ops

wasmOp(i64_extend_s_i32, WasmI64ExtendSI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    rshifti t0, 31, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_extend_u_i32, WasmI64ExtendUI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    return2i(ctx, 0, t0)
end)

wasmOp(f64_reinterpret_i64, WasmF64ReinterpretI64, macro(ctx)
    mload2i(ctx, m_operand, t1, t0)
    fii2d t0, t1, ft0
    returnd(ctx, ft0)
end)

wasmOp(i64_reinterpret_f64, WasmI64ReinterpretF64, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    fd2ii ft0, t0, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i32_trunc_s_f64, WasmI32TruncSF64, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    moveii 0xc1e0000000200000, t1, t0 # INT32_MIN - 1.0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .outOfBoundsTrunc

    moveii 0x41e0000000000000, t1, t0 # -INT32_MIN
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .outOfBoundsTrunc

    truncated2is ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i32_trunc_u_f64, WasmI32TruncUF64, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    moveii 0xbff0000000000000, t1, t0 # -1.0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .outOfBoundsTrunc

    moveii 0x41f0000000000000, t1, t0 # INT32_MIN * -2.0
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .outOfBoundsTrunc

    truncated2i ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

slowWasmOp(i64_trunc_s_f32)
slowWasmOp(i64_trunc_u_f32)
slowWasmOp(i64_trunc_s_f64)
slowWasmOp(i64_trunc_u_f64)

wasmOp(i32_trunc_sat_f64_s, WasmI32TruncSatF64S, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    moveii 0xc1e0000000200000, t1, t0 # INT32_MIN - 1.0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    moveii 0x41e0000000000000, t1, t0 # -INT32_MIN
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncated2is ft0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    returni(ctx, t0)

.outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    returni(ctx, t0)
end)

wasmOp(i32_trunc_sat_f64_u, WasmI32TruncSatF64U, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    moveii 0xbff0000000000000, t1, t0 # -1.0
    fii2d t0, t1, ft1
    bdltequn ft0, ft1, .outOfBoundsTruncSatMin

    moveii 0x41f0000000000000, t1, t0 # INT32_MIN * -2.0
    fii2d t0, t1, ft1
    bdgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncated2i ft0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMin:
    move 0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    returni(ctx, t0)
end)

slowWasmOp(i64_trunc_sat_f32_s)
slowWasmOp(i64_trunc_sat_f32_u)
slowWasmOp(i64_trunc_sat_f64_s)
slowWasmOp(i64_trunc_sat_f64_u)

# Extend ops

wasmOp(i64_extend8_s, WasmI64Extend8S, macro(ctx)
    mloadi(ctx, m_operand, t0)
    sxb2i t0, t0
    rshifti t0, 31, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_extend16_s, WasmI64Extend16S, macro(ctx)
    mloadi(ctx, m_operand, t0)
    sxh2i t0, t0
    rshifti t0, 31, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i64_extend32_s, WasmI64Extend16S, macro(ctx)
    mloadi(ctx, m_operand, t0)
    rshifti t0, 31, t1
    return2i(ctx, t1, t0)
end)

# Atomics

macro wasmAtomicBinaryRMWOps(lowerCaseOpcode, upperCaseOpcode, fnb, fnh, fni, fn2i)
    wasmOp(i64_atomic_rmw8%lowerCaseOpcode%_u, WasmI64AtomicRmw8%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mloadi(ctx, m_value, t0)
        emitCheckAndPreparePointerAddingOffset(t3, t5, 1)
        fnb(t0, [t3], t2, t5, t6)
        andi 0xff, t6 # FIXME: ZeroExtend8To64
        assert(macro(ok) bibeq t6, 0xff, .ok end)
        return2i(ctx, 0, t6)
    end)
    wasmOp(i64_atomic_rmw16%lowerCaseOpcode%_u, WasmI64AtomicRmw16%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mloadi(ctx, m_value, t0)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(t3, t5, 2)
        fnh(t0, [t3], t2, t5, t6)
        andi 0xffff, t6 # FIXME: ZeroExtend16To64
        assert(macro(ok) bibeq t6, 0xffff, .ok end)
        return2i(ctx, 0, t6)
    end)
    wasmOp(i64_atomic_rmw32%lowerCaseOpcode%_u, WasmI64AtomicRmw32%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mloadi(ctx, m_value, t0)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(t3, t5, 4)
        fni(t0, [t3], t2, t5, t6)
        return2i(ctx, 0, t6)
    end)
    wasmOp(i64_atomic_rmw%lowerCaseOpcode%, WasmI64AtomicRmw%upperCaseOpcode%, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mload2i(ctx, m_value, t1, t0)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(t3, t5, 8)
        push t4 # This is the PB, so need to be saved and restored
        fn2i(t1, t0, [t3], t4, t5, t7, t2, t6)
        pop t4
        return2i(ctx, t2, t6)
    end)
end

macro wasmAtomicBinaryRMWOpsWithWeakCAS(lowerCaseOpcode, upperCaseOpcode, fni, fn2i)
    wasmAtomicBinaryRMWOps(lowerCaseOpcode, upperCaseOpcode,
        macro(t0GPR, mem, t2GPR, t5GPR, t6GPR)
                fence
            .loop:
                loadlinkb mem, t6GPR
                fni(t0GPR, t6GPR, t2GPR)
                storecondb t5GPR, t2GPR, mem
                bineq t5GPR, 0, .loop
                fence
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t6GPR)
                fence
            .loop:
                loadlinkh mem, t6GPR
                fni(t0GPR, t6GPR, t2GPR)
                storecondh t5GPR, t2GPR, mem
                bineq t5GPR, 0, .loop
                fence
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t6GPR)
                fence
            .loop:
                loadlinki mem, t6GPR
                fni(t0GPR, t6GPR, t2GPR)
                storecondi t5GPR, t2GPR, mem
                bineq t5GPR, 0, .loop
                fence
        end,
        macro(t1GPR, t0GPR, mem, t4GPR, t5GPR, t7GPR, t2GPR, t6GPR)
                fence
            .loop:
                loadlink2i mem, t6GPR, t2GPR
                fn2i(t1GPR, t0GPR, t2GPR, t6GPR, t4GPR, t5GPR)
                storecond2i t7GPR, t5GPR, t4GPR, mem
                bineq t7GPR, 0, .loop
                fence
        end)
end

if ARMv7
    wasmAtomicBinaryRMWOpsWithWeakCAS(_add, Add,
        macro(src0, src1, dst)
            addi src0, src1, dst
        end,
        macro(src0Msw, src0Lsw, src1Msw, src1Lsw, dstMsw, dstLsw)
            addis src0Lsw, src1Lsw, dstLsw
            adci  src0Msw, src1Msw, dstMsw
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_sub, Sub,
        macro(src0, src1, dst)
            subi src1, src0, dst
        end,
        macro(src0Msw, src0Lsw, src1Msw, src1Lsw, dstMsw, dstLsw)
            subis src1Lsw, src0Lsw, dstLsw
            sbci  src1Msw, src0Msw, dstMsw
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_xchg, Xchg,
        macro(src0, src1, dst)
            move src0, dst
        end,
        macro(src0Msw, src0Lsw, src1Msw, src1Lsw, dstMsw, dstLsw)
            move src0Lsw, dstLsw
            move src0Msw, dstMsw
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_and, And,
        macro(src0, src1, dst)
            andi src0, src1, dst
        end,
        macro(src0Msw, src0Lsw, src1Msw, src1Lsw, dstMsw, dstLsw)
            andi src0Lsw, src1Lsw, dstLsw
            andi src0Msw, src1Msw, dstMsw
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_or, Or,
        macro(src0, src1, dst)
            ori src0, src1, dst
        end,
        macro(src0Msw, src0Lsw, src1Msw, src1Lsw, dstMsw, dstLsw)
            ori src0Lsw, src1Lsw, dstLsw
            ori src0Msw, src1Msw, dstMsw
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_xor, Xor,
        macro(src0, src1, dst)
            xori src0, src1, dst
        end,
        macro(src0Msw, src0Lsw, src1Msw, src1Lsw, dstMsw, dstLsw)
            xori src0Lsw, src1Lsw, dstLsw
            xori src0Msw, src1Msw, dstMsw
        end)
end

macro wasmAtomicCompareExchangeOps(lowerCaseOpcode, upperCaseOpcode, fnb, fnh, fni, fn2i)
    wasmOp(i64_atomic_rmw8%lowerCaseOpcode%_u, WasmI64AtomicRmw8%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mload2i(ctx, m_expected, t1, t0)
        mload2i(ctx, m_value, t6, t2)
        emitCheckAndPreparePointerAddingOffset(t3, t5, 1)
        fnb(t1, t0, t6, t2, [t3], t7, t5)
        andi 0xff, t0 # FIXME: ZeroExtend8To64
        assert(macro(ok) bibeq t0, 0xff, .ok end)
        assert(macro(ok) bieq t1, 0, .ok end)
        return2i(ctx, t1, t0)
    end)
    wasmOp(i64_atomic_rmw16%lowerCaseOpcode%_u, WasmI64AtomicRmw16%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mload2i(ctx, m_expected, t1, t0)
        mload2i(ctx, m_value, t6, t2)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(t3, t5, 2)
        fnh(t1, t0, t6, t2, [t3], t7, t5)
        andi 0xffff, t0 # FIXME: ZeroExtend16To64
        assert(macro(ok) bibeq t0, 0xffff, .ok end)
        assert(macro(ok) bieq t1, 0, .ok end)
        return2i(ctx, t1, t0)
    end)
    wasmOp(i64_atomic_rmw32%lowerCaseOpcode%_u, WasmI64AtomicRmw32%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mload2i(ctx, m_expected, t1, t0)
        mload2i(ctx, m_value, t6, t2)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(t3, t5, 8)
        fni(t1, t0, t6, t2, [t3], t7, t5)
        assert(macro(ok) bieq t1, 0, .ok end)
        return2i(ctx, t1, t0)
    end)
    wasmOp(i64_atomic_rmw%lowerCaseOpcode%, WasmI64AtomicRmw%upperCaseOpcode%, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t5)
        mload2i(ctx, m_expected, t1, t0)
        mload2i(ctx, m_value, t6, t2)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(t3, t5, 8)
        push t4 # This is the PB, so need to be saved and restored
        fn2i(t1, t0, t6, t2, [t3], t7, t5, t4)
        pop t4
        return2i(ctx, t1, t0)
    end)
end

if ARMv7
// exp: "expected", val: "value", res: "result"
wasmAtomicCompareExchangeOps(_cmpxchg, Cmpxchg,
    macro(expMsw, expLsw, valMsw, valLsw, mem, scratch, resLsw)
            fence
        .loop:
            loadlinkb mem, resLsw
            bineq expLsw, resLsw, .fail
            bineq expMsw, 0, .fail
            storecondb scratch, valLsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .fail:
            storecondb scratch, resLsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .done:
            fence
            move resLsw, expLsw
            move 0, expMsw
    end,
    macro(expMsw, expLsw, valMsw, valLsw, mem, scratch, resLsw)
            fence
        .loop:
            loadlinkh mem, resLsw
            bineq expLsw, resLsw, .fail
            bineq expMsw, 0, .fail
            storecondh scratch, valLsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .fail:
            storecondh scratch, resLsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .done:
            fence
            move resLsw, expLsw
            move 0, expMsw
    end,
    macro(expMsw, expLsw, valMsw, valLsw, mem, scratch, resLsw)
            fence
        .loop:
            loadlinki mem, resLsw
            bineq expLsw, resLsw, .fail
            bineq expMsw, 0, .fail
            storecondi scratch, valLsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .fail:
            storecondi scratch, resLsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .done:
            fence
            move resLsw, expLsw
            move 0, expMsw
    end,
    macro(expMsw, expLsw, valMsw, valLsw, mem, scratch, resMsw, resLsw)
            fence
        .loop:
            loadlink2i mem, resLsw, resMsw
            bineq expLsw, resLsw, .fail
            bineq expMsw, resMsw, .fail
            storecond2i scratch, valLsw, valMsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .fail:
            storecond2i scratch, resLsw, resMsw, mem
            bieq scratch, 0, .done
            jmp .loop
        .done:
            fence
            move resLsw, expLsw
            move resMsw, expMsw
    end)
end

# GC ops

wasmOp(i31_new, WasmI31New, macro(ctx)
    mloadi(ctx, m_value, t0)
    andi 0x7fffffff, t0
    move Int32Tag, t1
    return2i(ctx, t1, t0)
end)

wasmOp(i31_get_s, WasmI31GetS, macro(ctx)
    mload2i(ctx, m_ref, t1, t0)
    bieq t1, NullTag, .throw
    lshifti 0x1, t0
    rshifti 0x1, t0
    returni(ctx, t0)

.throw:
    throwException(NullI31Get)
end)

wasmOp(i31_get_u, WasmI31GetU, macro(ctx)
    mload2i(ctx, m_ref, t1, t0)
    bieq t1, NullTag, .throw
    returni(ctx, t0)

.throw:
    throwException(NullI31Get)
end)

wasmOp(array_len, WasmArrayLen, macro(ctx)
    mload2i(ctx, m_arrayref, t1, t0)
    bieq t1, NullTag, .nullArray
    loadi JSWebAssemblyArray::m_size[t0], t0
    returni(ctx, t0)

.nullArray:
    throwException(NullArrayLen)
end)
