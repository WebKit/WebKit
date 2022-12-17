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

# Opcodes that should eventually be shared with JS llint

wasmOp(mov, WasmMov, macro(ctx)
    mloadq(ctx, m_src, t0)
    returnq(ctx, t0)
end)

# Wasm specific bytecodes

macro emitCheckAndPreparePointer(ctx, pointer, offset, size)
    leap size - 1[pointer, offset], t5
    bpb t5, boundsCheckingSize, .continuation
    throwException(OutOfBoundsMemoryAccess)
.continuation:
    addp memoryBase, pointer
end

macro emitCheckAndPreparePointerAddingOffset(ctx, pointer, offset, size)
    leap size - 1[pointer, offset], t5
    bpb t5, boundsCheckingSize, .continuation
.throw:
    throwException(OutOfBoundsMemoryAccess)
.continuation:
    addp memoryBase, pointer
    addp offset, pointer
end

macro emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(ctx, pointer, offset, size)
    leap size - 1[pointer, offset], t5
    bpb t5, boundsCheckingSize, .continuation
.throw:
    throwException(OutOfBoundsMemoryAccess)
.continuation:
    addp memoryBase, pointer
    addp offset, pointer
    btpnz pointer, (size - 1), .throw
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

wasmOp(ref_is_null, WasmRefIsNull, macro(ctx)
    mloadp(ctx, m_ref, t0)
    cqeq t0, ValueNull, t0
    returni(ctx, t0)
end)

wasmOp(get_global, WasmGetGlobal, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    lshiftp 1, t1
    loadq [t0, t1, 8], t0
    returnq(ctx, t0)
end)

wasmOp(set_global, WasmSetGlobal, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    lshiftp 1, t1
    mloadq(ctx, m_value, t2)
    storeq t2, [t0, t1, 8]
    dispatch(ctx)
end)

wasmOp(get_global_portable_binding, WasmGetGlobalPortableBinding, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    lshiftp 1, t1
    loadq [t0, t1, 8], t0
    loadq [t0], t0
    returnq(ctx, t0)
end)

wasmOp(set_global_portable_binding, WasmSetGlobalPortableBinding, macro(ctx)
    loadp Wasm::Instance::m_globals[wasmInstance], t0
    wgetu(ctx, m_globalIndex, t1)
    lshiftp 1, t1
    mloadq(ctx, m_value, t2)
    loadq [t0, t1, 8], t0
    storeq t2, [t0]
    dispatch(ctx)
end)

# Opcodes that don't have the `b3op` entry in wasm.json. This should be kept in sync

# i32 binary ops

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
    elsif ARM64 or ARM64E or RISCV64
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
    elsif ARM64 or ARM64E or RISCV64
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
    elsif RISCV64
        remis t0, t1, t2
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
    elsif RISCV64
        remi t0, t1, t2
    else
        error
    end
    returni(ctx, t2)

.throwDivisionByZero:
    throwException(DivisionByZero)
end)

# i64 binary ops

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
    elsif ARM64 or ARM64E or RISCV64
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
    elsif ARM64 or ARM64E or RISCV64
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
    elsif RISCV64
        remqs t0, t1, t2
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
    elsif RISCV64
        remq t0, t1, t2
    else
        error
    end
    returnq(ctx, t2)

.throwDivisionByZero:
    throwException(DivisionByZero)
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

# i64 unary ops

wasmOp(i64_ctz, WasmI64Ctz, macro (ctx)
    mloadq(ctx, m_operand, t0)
    tzcntq t0, t0
    returnq(ctx, t0)
end)

wasmOp(i64_clz, WasmI64Clz, macro(ctx)
    mloadq(ctx, m_operand, t0)
    lzcntq t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_popcnt, WasmI64Popcnt, macro (ctx)
    mloadq(ctx, m_operand, a1)
    prepareStateForCCall()
    move PC, a0
    cCall2(_slow_path_wasm_popcountll)
    restoreStateAfterCCall()
    returnq(ctx, r1)
end)

wasmOp(i64_eqz, WasmI64Eqz, macro(ctx)
    mloadq(ctx, m_operand, t0)
    cqeq t0, 0, t0
    returni(ctx, t0)
end)

# f64 binary ops

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

# Type conversion ops

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

wasmOp(f64_reinterpret_i64, WasmF64ReinterpretI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    fq2d t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(i64_reinterpret_f64, WasmI64ReinterpretF64, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    fd2q ft0, t0
    returnq(ctx, t0)
end)

wasmOp(i32_trunc_s_f64, WasmI32TruncSF64, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xc1e0000000200000, t0 # INT32_MIN - 1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .outOfBoundsTrunc

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTrunc

    truncated2is ft0, t0
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
    mloadf(ctx, m_operand, ft0)

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

wasmOp(i32_trunc_sat_f64_s, WasmI32TruncSatF64S, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xc1e0000000200000, t0 # INT32_MIN - 1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    move 0x41e0000000000000, t0 # -INT32_MIN
    fq2d t0, ft1
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

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .outOfBoundsTruncSatMin

    move 0x41f0000000000000, t0 # INT32_MIN * -2.0
    fq2d t0, ft1
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

wasmOp(i64_trunc_sat_f32_s, WasmI64TruncSatF32S, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xdf000000, t0 # INT64_MIN
    fi2f t0, ft1
    bfltun ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    move 0x5f000000, t0 # -INT64_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncatef2qs ft0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMin:
    move (constexpr INT64_MIN), t0
    returnq(ctx, t0)
end)

wasmOp(i64_trunc_sat_f32_u, WasmI64TruncSatF32U, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .outOfBoundsTruncSatMin

    move 0x5f800000, t0 # INT64_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncatef2q ft0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMin:
    move 0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    returnq(ctx, t0)
end)

wasmOp(i64_trunc_sat_f64_s, WasmI64TruncSatF64S, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xc3e0000000000000, t0 # INT64_MIN
    fq2d t0, ft1
    bdltun ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    move 0x43e0000000000000, t0 # -INT64_MIN
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncated2qs ft0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMinOrNaN:
    bdeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr INT64_MAX), t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMin:
    move (constexpr INT64_MIN), t0
    returnq(ctx, t0)
end)

wasmOp(i64_trunc_sat_f64_u, WasmI64TruncSatF64U, macro (ctx)
    mloadd(ctx, m_operand, ft0)

    move 0xbff0000000000000, t0 # -1.0
    fq2d t0, ft1
    bdltequn ft0, ft1, .outOfBoundsTruncSatMin

    move 0x43f0000000000000, t0 # INT64_MIN * -2.0
    fq2d t0, ft1
    bdgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncated2q ft0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMin:
    move 0, t0
    returnq(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr UINT64_MAX), t0
    returnq(ctx, t0)
end)

# Extend ops

wasmOp(i64_extend8_s, WasmI64Extend8S, macro(ctx)
    mloadq(ctx, m_operand, t0)
    sxb2q t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_extend16_s, WasmI64Extend16S, macro(ctx)
    mloadq(ctx, m_operand, t0)
    sxh2q t0, t1
    returnq(ctx, t1)
end)

wasmOp(i64_extend32_s, WasmI64Extend16S, macro(ctx)
    mloadq(ctx, m_operand, t0)
    sxi2q t0, t1
    returnq(ctx, t1)
end)

# Atomics

macro wasmAtomicBinaryRMWOps(lowerCaseOpcode, upperCaseOpcode, fnb, fnh, fni, fnq)
    wasmOp(i64_atomic_rmw8%lowerCaseOpcode%_u, WasmI64AtomicRmw8%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_value, t0)
        emitCheckAndPreparePointerAddingOffset(ctx, t3, t1, 1)
        fnb(t0, [t3], t2, t5, t1)
        andq 0xff, t0 # FIXME: ZeroExtend8To64
        assert(macro(ok) bqbeq t0, 0xff, .ok end)
        returnq(ctx, t0)
    end)
    wasmOp(i64_atomic_rmw16%lowerCaseOpcode%_u, WasmI64AtomicRmw16%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_value, t0)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(ctx, t3, t1, 2)
        fnh(t0, [t3], t2, t5, t1)
        andq 0xffff, t0 # FIXME: ZeroExtend16To64
        assert(macro(ok) bqbeq t0, 0xffff, .ok end)
        returnq(ctx, t0)
    end)
    wasmOp(i64_atomic_rmw32%lowerCaseOpcode%_u, WasmI64AtomicRmw32%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_value, t0)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(ctx, t3, t1, 4)
        fni(t0, [t3], t2, t5, t1)
        assert(macro(ok) bqbeq t0, 0xffffffff, .ok end)
        returnq(ctx, t0)
    end)
    wasmOp(i64_atomic_rmw%lowerCaseOpcode%, WasmI64AtomicRmw%upperCaseOpcode%, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_value, t0)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(ctx, t3, t1, 8)
        fnq(t0, [t3], t2, t5, t1)
        returnq(ctx, t0)
    end)
end

macro wasmAtomicBinaryRMWOpsWithWeakCAS(lowerCaseOpcode, upperCaseOpcode, fni, fnq)
    wasmAtomicBinaryRMWOps(lowerCaseOpcode, upperCaseOpcode,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            if X86_64
                move t0GPR, t5GPR
                loadb mem, t0GPR
            .loop:
                move t0GPR, t2GPR
                fni(t5GPR, t2GPR)
                batomicweakcasb t0GPR, t2GPR, mem, .loop
            else
            .loop:
                loadlinkacqb mem, t1GPR
                fni(t0GPR, t1GPR, t2GPR)
                storecondrelb t5GPR, t2GPR, mem
                bineq t5GPR, 0, .loop
                move t1GPR, t0GPR
            end
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            if X86_64
                move t0GPR, t5GPR
                loadh mem, t0GPR
            .loop:
                move t0GPR, t2GPR
                fni(t5GPR, t2GPR)
                batomicweakcash t0GPR, t2GPR, mem, .loop
            else
            .loop:
                loadlinkacqh mem, t1GPR
                fni(t0GPR, t1GPR, t2GPR)
                storecondrelh t5GPR, t2GPR, mem
                bineq t5GPR, 0, .loop
                move t1GPR, t0GPR
            end
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            if X86_64
                move t0GPR, t5GPR
                loadi mem, t0GPR
            .loop:
                move t0GPR, t2GPR
                fni(t5GPR, t2GPR)
                batomicweakcasi t0GPR, t2GPR, mem, .loop
            else
            .loop:
                loadlinkacqi mem, t1GPR
                fni(t0GPR, t1GPR, t2GPR)
                storecondreli t5GPR, t2GPR, mem
                bineq t5GPR, 0, .loop
                move t1GPR, t0GPR
            end
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            if X86_64
                move t0GPR, t5GPR
                loadq mem, t0GPR
            .loop:
                move t0GPR, t2GPR
                fnq(t5GPR, t2GPR)
                batomicweakcasq t0GPR, t2GPR, mem, .loop
            else
            .loop:
                loadlinkacqq mem, t1GPR
                fnq(t0GPR, t1GPR, t2GPR)
                storecondrelq t5GPR, t2GPR, mem
                bineq t5GPR, 0, .loop
                move t1GPR, t0GPR
            end
        end)
end

if X86_64
    wasmAtomicBinaryRMWOps(_add, Add,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddb t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddh t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddi t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddq t0GPR, mem end)
    wasmAtomicBinaryRMWOps(_sub, Sub,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgsubb t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgsubh t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgsubi t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgsubq t0GPR, mem end)
    wasmAtomicBinaryRMWOps(_xchg, Xchg,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgb t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgh t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgi t0GPR, mem end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgq t0GPR, mem end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_and, And,
        macro(t5GPR, t2GPR)
            andi t5GPR, t2GPR
        end,
        macro(t5GPR, t2GPR)
            andq t5GPR, t2GPR
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_or, Or,
        macro(t5GPR, t2GPR)
            ori t5GPR, t2GPR
        end,
        macro(t5GPR, t2GPR)
            orq t5GPR, t2GPR
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_xor, Xor,
        macro(t5GPR, t2GPR)
            xori t5GPR, t2GPR
        end,
        macro(t5GPR, t2GPR)
            xorq t5GPR, t2GPR
        end)
elsif ARM64E
    wasmAtomicBinaryRMWOps(_add, Add,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddb t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddh t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddi t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgaddq t0GPR, mem, t0GPR end)
    wasmAtomicBinaryRMWOps(_sub, Sub,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            negq t0GPR
            atomicxchgaddb t0GPR, mem, t0GPR
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            negq t0GPR
            atomicxchgaddh t0GPR, mem, t0GPR
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            negq t0GPR
            atomicxchgaddi t0GPR, mem, t0GPR
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            negq t0GPR
            atomicxchgaddq t0GPR, mem, t0GPR
        end)
    wasmAtomicBinaryRMWOps(_and, And,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            notq t0GPR
            atomicxchgclearb t0GPR, mem, t0GPR
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            notq t0GPR
            atomicxchgclearh t0GPR, mem, t0GPR
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            notq t0GPR
            atomicxchgcleari t0GPR, mem, t0GPR
        end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR)
            notq t0GPR
            atomicxchgclearq t0GPR, mem, t0GPR
        end)
    wasmAtomicBinaryRMWOps(_or, Or,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgorb t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgorh t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgori t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgorq t0GPR, mem, t0GPR end)
    wasmAtomicBinaryRMWOps(_xor, Xor,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgxorb t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgxorh t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgxori t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgxorq t0GPR, mem, t0GPR end)
    wasmAtomicBinaryRMWOps(_xchg, Xchg,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgb t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgh t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgi t0GPR, mem, t0GPR end,
        macro(t0GPR, mem, t2GPR, t5GPR, t1GPR) atomicxchgq t0GPR, mem, t0GPR end)
elsif ARM64 or RISCV64
    wasmAtomicBinaryRMWOpsWithWeakCAS(_add, Add,
        macro(t0GPR, t1GPR, t2GPR)
            addi t0GPR, t1GPR, t2GPR
        end,
        macro(t0GPR, t1GPR, t2GPR)
            addq t0GPR, t1GPR, t2GPR
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_sub, Sub,
        macro(t0GPR, t1GPR, t2GPR)
            subi t1GPR, t0GPR, t2GPR
        end,
        macro(t0GPR, t1GPR, t2GPR)
            subq t1GPR, t0GPR, t2GPR
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_xchg, Xchg,
        macro(t0GPR, t1GPR, t2GPR)
            move t0GPR, t2GPR
        end,
        macro(t0GPR, t1GPR, t2GPR)
            move t0GPR, t2GPR
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_and, And,
        macro(t0GPR, t1GPR, t2GPR)
            andi t0GPR, t1GPR, t2GPR
        end,
        macro(t0GPR, t1GPR, t2GPR)
            andq t0GPR, t1GPR, t2GPR
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_or, Or,
        macro(t0GPR, t1GPR, t2GPR)
            ori t0GPR, t1GPR, t2GPR
        end,
        macro(t0GPR, t1GPR, t2GPR)
            orq t0GPR, t1GPR, t2GPR
        end)
    wasmAtomicBinaryRMWOpsWithWeakCAS(_xor, Xor,
        macro(t0GPR, t1GPR, t2GPR)
            xori t0GPR, t1GPR, t2GPR
        end,
        macro(t0GPR, t1GPR, t2GPR)
            xorq t0GPR, t1GPR, t2GPR
        end)
end

macro wasmAtomicCompareExchangeOps(lowerCaseOpcode, upperCaseOpcode, fnb, fnh, fni, fnq)
    wasmOp(i64_atomic_rmw8%lowerCaseOpcode%_u, WasmI64AtomicRmw8%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_expected, t0)
        mloadq(ctx, m_value, t2)
        emitCheckAndPreparePointerAddingOffset(ctx, t3, t1, 1)
        fnb(t0, t2, [t3], t5, t1)
        andq 0xff, t0 # FIXME: ZeroExtend8To64
        assert(macro(ok) bqbeq t0, 0xff, .ok end)
        returnq(ctx, t0)
    end)
    wasmOp(i64_atomic_rmw16%lowerCaseOpcode%_u, WasmI64AtomicRmw16%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_expected, t0)
        mloadq(ctx, m_value, t2)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(ctx, t3, t1, 2)
        fnh(t0, t2, [t3], t5, t1)
        andq 0xffff, t0 # FIXME: ZeroExtend16To64
        assert(macro(ok) bqbeq t0, 0xffff, .ok end)
        returnq(ctx, t0)
    end)
    wasmOp(i64_atomic_rmw32%lowerCaseOpcode%_u, WasmI64AtomicRmw32%upperCaseOpcode%U, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_expected, t0)
        mloadq(ctx, m_value, t2)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(ctx, t3, t1, 4)
        fni(t0, t2, [t3], t5, t1)
        assert(macro(ok) bqbeq t0, 0xffffffff, .ok end)
        returnq(ctx, t0)
    end)
    wasmOp(i64_atomic_rmw%lowerCaseOpcode%, WasmI64AtomicRmw%upperCaseOpcode%, macro(ctx)
        mloadi(ctx, m_pointer, t3)
        wgetu(ctx, m_offset, t1)
        mloadq(ctx, m_expected, t0)
        mloadq(ctx, m_value, t2)
        emitCheckAndPreparePointerAddingOffsetWithAlignmentCheck(ctx, t3, t1, 8)
        fnq(t0, t2, [t3], t5, t1)
        returnq(ctx, t0)
    end)
end

if X86_64 or ARM64E or ARM64 or RISCV64
# t0GPR => expected, t2GPR => value, mem => memory reference
wasmAtomicCompareExchangeOps(_cmpxchg, Cmpxchg,
    macro(t0GPR, t2GPR, mem, t5GPR, t1GPR)
        if X86_64 or ARM64E
            bqa t0GPR , 0xff, .fail
            atomicweakcasb t0GPR, t2GPR, mem
            jmp .done
        .fail:
            atomicloadb mem, t0GPR
        .done:
        else
        .loop:
            loadlinkacqb mem, t1GPR
            bqneq t0GPR, t1GPR, .fail
            storecondrelb t5GPR, t2GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .fail:
            storecondrelb t5GPR, t1GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .done:
            move t1GPR, t0GPR
        end
    end,
    macro(t0GPR, t2GPR, mem, t5GPR, t1GPR)
        if X86_64 or ARM64E
            bqa t0GPR, 0xffff, .fail
            atomicweakcash t0GPR, t2GPR, mem
            jmp .done
        .fail:
            atomicloadh mem, t0GPR
        .done:
        else
        .loop:
            loadlinkacqh mem, t1GPR
            bqneq t0GPR, t1GPR, .fail
            storecondrelh t5GPR, t2GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .fail:
            storecondrelh t5GPR, t1GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .done:
            move t1GPR, t0GPR
        end
    end,
    macro(t0GPR, t2GPR, mem, t5GPR, t1GPR)
        if X86_64 or ARM64E
            bqa t0GPR, 0xffffffff, .fail
            atomicweakcasi t0GPR, t2GPR, mem
            jmp .done
        .fail:
            atomicloadi mem, t0GPR
        .done:
        else
        .loop:
            loadlinkacqi mem, t1GPR
            bqneq t0GPR, t1GPR, .fail
            storecondreli t5GPR, t2GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .fail:
            storecondreli t5GPR, t1GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .done:
            move t1GPR, t0GPR
        end
    end,
    macro(t0GPR, t2GPR, mem, t5GPR, t1GPR)
        if X86_64 or ARM64E
            atomicweakcasq t0GPR, t2GPR, mem
        else
        .loop:
            loadlinkacqq mem, t1GPR
            bqneq t0GPR, t1GPR, .fail
            storecondrelq t5GPR, t2GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .fail:
            storecondrelq t5GPR, t1GPR, mem
            bieq t5GPR, 0, .done
            jmp .loop
        .done:
            move t1GPR, t0GPR
        end
    end)
end

# GC ops

wasmOp(i31_new, WasmI31New, macro(ctx)
    mloadi(ctx, m_value, t0)
    andq 0x7fffffff, t0
    orq TagNumber, t0
    returnq(ctx, t0)
end)

wasmOp(i31_get, WasmI31Get, macro(ctx)
    mloadp(ctx, m_ref, t0)
    bqeq t0, ValueNull, .throw
    wgetu(ctx, m_isSigned, t1)
    btiz t1, .unsigned
    lshifti 0x1, t0
    rshifti 0x1, t0
.unsigned:
    returni(ctx, t0)

.throw:
    throwException(NullI31Get)
end)

wasmOp(array_len, WasmArrayLen, macro(ctx)
    mloadp(ctx, m_arrayref, t0)
    bqeq t0, ValueNull, .nullArray
    loadi JSWebAssemblyArray::m_size[t0], t0
    returni(ctx, t0)

.nullArray:
    throwException(NullArrayLen)
end)
