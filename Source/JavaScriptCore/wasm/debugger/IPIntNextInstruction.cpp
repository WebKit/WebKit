/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IPIntNextInstruction.h"

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "InPlaceInterpreter.h"
#include <wtf/StdLibExtras.h>

namespace JSC {
namespace IPInt {

// Forward declarations for opcode handlers using the macro
#define DECLARE_OPCODE_HANDLER(opcode, name) \
    static NextInstructionResult handle_##name(const uint8_t* pc, const uint8_t* mc);

FOR_EACH_IPINT_OPCODE(DECLARE_OPCODE_HANDLER)

// Now define each handler individually - this ensures we cover all opcodes from the macro

// Control flow instructions
static NextInstructionResult handle_unreachable(const uint8_t* pc, const uint8_t* mc) // TODO: weird?
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_nop(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_block(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withBlockMetadata(pc, mc);
}

static NextInstructionResult handle_loop(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::variableLength(pc, mc);
}

static NextInstructionResult handle_if(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleIf(pc, mc);
}

static NextInstructionResult handle_else(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withBlockMetadata(pc, mc);
}

static NextInstructionResult handle_try(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::variableLength(pc, mc);
}

static NextInstructionResult handle_catch(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withBlockMetadata(pc, mc);
}

static NextInstructionResult handle_throw(const uint8_t* pc, const uint8_t* mc) // TODO: wrong
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_rethrow(const uint8_t* pc, const uint8_t* mc) // TODO: wrong
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_throw_ref(const uint8_t* pc, const uint8_t* mc) // TODO: wrong
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_end(const uint8_t* pc, const uint8_t* mc) // TODO: what happens next?
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_br(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleBr(pc, mc);
}

static NextInstructionResult handle_br_if(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleBrIf(pc, mc);
}

static NextInstructionResult handle_br_table(const uint8_t* pc, const uint8_t* mc) // TODO: fixme
{
    return InstructionAdvance::handleBrTable(pc, mc);
}

static NextInstructionResult handle_return(const uint8_t*, const uint8_t*)
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

static NextInstructionResult handle_call(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleCall(pc, mc);
}

static NextInstructionResult handle_call_indirect(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleCallIndirect(pc, mc);
}

static NextInstructionResult handle_return_call(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleCall(pc, mc);
}

static NextInstructionResult handle_return_call_indirect(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleCallIndirect(pc, mc);
}

static NextInstructionResult handle_call_ref(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleCall(pc, mc);
}

static NextInstructionResult handle_return_call_ref(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleCall(pc, mc);
}

// Reserved opcodes
static NextInstructionResult handle_reserved_0x16(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_reserved_0x17(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_delegate(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withBlockMetadata(pc, mc);
}

static NextInstructionResult handle_catch_all(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withBlockMetadata(pc, mc);
}

static NextInstructionResult handle_drop(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_select(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::variableLength(pc, mc);
}

static NextInstructionResult handle_select_t(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::variableLength(pc, mc);
}

static NextInstructionResult handle_reserved_0x1d(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_reserved_0x1e(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_try_table(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withBlockMetadata(pc, mc);
}

// Variable instructions
static NextInstructionResult handle_local_get(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleLocalGet(pc, mc);
}

static NextInstructionResult handle_local_set(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleLocalSet(pc, mc);
}

static NextInstructionResult handle_local_tee(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleLocalTee(pc, mc);
}

static NextInstructionResult handle_global_get(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withGlobalMetadata(pc, mc);
}

static NextInstructionResult handle_global_set(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withGlobalMetadata(pc, mc);
}

static NextInstructionResult handle_table_get(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_table_set(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_reserved_0x27(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

// Memory instructions - all use Const32Metadata for memory offset
static NextInstructionResult handle_i32_load_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_load_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_f32_load_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_f64_load_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i32_load8s_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i32_load8u_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i32_load16s_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i32_load16u_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_load8s_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_load8u_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_load16s_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_load16u_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_load32s_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_load32u_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i32_store_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_store_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_f32_store_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_f64_store_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i32_store8_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i32_store16_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_store8_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_store16_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_i64_store32_mem(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_memory_size(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 2);
}

static NextInstructionResult handle_memory_grow(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 2);
}

// Constant instructions
static NextInstructionResult handle_i32_const(const uint8_t* pc, const uint8_t* mc)
{
    const InstructionLengthMetadata* metadata = reinterpret_cast<const InstructionLengthMetadata*>(mc);
    if (metadata->length >= 2)
        return InstructionAdvance::withConst32Metadata(pc, mc);
    return InstructionAdvance::fixedLength(pc, mc, 2);
}

static NextInstructionResult handle_i64_const(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst64Metadata(pc, mc);
}

static NextInstructionResult handle_f32_const(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 5);
}

static NextInstructionResult handle_f64_const(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 9);
}

// All arithmetic/comparison instructions - fixed length 1
#define DEFINE_ARITHMETIC_HANDLER(name)                                              \
    static NextInstructionResult handle_##name(const uint8_t* pc, const uint8_t* mc) \
    {                                                                                \
        return InstructionAdvance::fixedLength(pc, mc, 1);                           \
    }

DEFINE_ARITHMETIC_HANDLER(i32_eqz)
DEFINE_ARITHMETIC_HANDLER(i32_eq)
DEFINE_ARITHMETIC_HANDLER(i32_ne)
DEFINE_ARITHMETIC_HANDLER(i32_lt_s)
DEFINE_ARITHMETIC_HANDLER(i32_lt_u)
DEFINE_ARITHMETIC_HANDLER(i32_gt_s)
DEFINE_ARITHMETIC_HANDLER(i32_gt_u)
DEFINE_ARITHMETIC_HANDLER(i32_le_s)
DEFINE_ARITHMETIC_HANDLER(i32_le_u)
DEFINE_ARITHMETIC_HANDLER(i32_ge_s)
DEFINE_ARITHMETIC_HANDLER(i32_ge_u)
DEFINE_ARITHMETIC_HANDLER(i64_eqz)
DEFINE_ARITHMETIC_HANDLER(i64_eq)
DEFINE_ARITHMETIC_HANDLER(i64_ne)
DEFINE_ARITHMETIC_HANDLER(i64_lt_s)
DEFINE_ARITHMETIC_HANDLER(i64_lt_u)
DEFINE_ARITHMETIC_HANDLER(i64_gt_s)
DEFINE_ARITHMETIC_HANDLER(i64_gt_u)
DEFINE_ARITHMETIC_HANDLER(i64_le_s)
DEFINE_ARITHMETIC_HANDLER(i64_le_u)
DEFINE_ARITHMETIC_HANDLER(i64_ge_s)
DEFINE_ARITHMETIC_HANDLER(i64_ge_u)
DEFINE_ARITHMETIC_HANDLER(f32_eq)
DEFINE_ARITHMETIC_HANDLER(f32_ne)
DEFINE_ARITHMETIC_HANDLER(f32_lt)
DEFINE_ARITHMETIC_HANDLER(f32_gt)
DEFINE_ARITHMETIC_HANDLER(f32_le)
DEFINE_ARITHMETIC_HANDLER(f32_ge)
DEFINE_ARITHMETIC_HANDLER(f64_eq)
DEFINE_ARITHMETIC_HANDLER(f64_ne)
DEFINE_ARITHMETIC_HANDLER(f64_lt)
DEFINE_ARITHMETIC_HANDLER(f64_gt)
DEFINE_ARITHMETIC_HANDLER(f64_le)
DEFINE_ARITHMETIC_HANDLER(f64_ge)
DEFINE_ARITHMETIC_HANDLER(i32_clz)
DEFINE_ARITHMETIC_HANDLER(i32_ctz)
DEFINE_ARITHMETIC_HANDLER(i32_popcnt)
DEFINE_ARITHMETIC_HANDLER(i32_add)
DEFINE_ARITHMETIC_HANDLER(i32_sub)
DEFINE_ARITHMETIC_HANDLER(i32_mul)
DEFINE_ARITHMETIC_HANDLER(i32_div_s)
DEFINE_ARITHMETIC_HANDLER(i32_div_u)
DEFINE_ARITHMETIC_HANDLER(i32_rem_s)
DEFINE_ARITHMETIC_HANDLER(i32_rem_u)
DEFINE_ARITHMETIC_HANDLER(i32_and)
DEFINE_ARITHMETIC_HANDLER(i32_or)
DEFINE_ARITHMETIC_HANDLER(i32_xor)
DEFINE_ARITHMETIC_HANDLER(i32_shl)
DEFINE_ARITHMETIC_HANDLER(i32_shr_s)
DEFINE_ARITHMETIC_HANDLER(i32_shr_u)
DEFINE_ARITHMETIC_HANDLER(i32_rotl)
DEFINE_ARITHMETIC_HANDLER(i32_rotr)
DEFINE_ARITHMETIC_HANDLER(i64_clz)
DEFINE_ARITHMETIC_HANDLER(i64_ctz)
DEFINE_ARITHMETIC_HANDLER(i64_popcnt)
DEFINE_ARITHMETIC_HANDLER(i64_add)
DEFINE_ARITHMETIC_HANDLER(i64_sub)
DEFINE_ARITHMETIC_HANDLER(i64_mul)
DEFINE_ARITHMETIC_HANDLER(i64_div_s)
DEFINE_ARITHMETIC_HANDLER(i64_div_u)
DEFINE_ARITHMETIC_HANDLER(i64_rem_s)
DEFINE_ARITHMETIC_HANDLER(i64_rem_u)
DEFINE_ARITHMETIC_HANDLER(i64_and)
DEFINE_ARITHMETIC_HANDLER(i64_or)
DEFINE_ARITHMETIC_HANDLER(i64_xor)
DEFINE_ARITHMETIC_HANDLER(i64_shl)
DEFINE_ARITHMETIC_HANDLER(i64_shr_s)
DEFINE_ARITHMETIC_HANDLER(i64_shr_u)
DEFINE_ARITHMETIC_HANDLER(i64_rotl)
DEFINE_ARITHMETIC_HANDLER(i64_rotr)
DEFINE_ARITHMETIC_HANDLER(f32_abs)
DEFINE_ARITHMETIC_HANDLER(f32_neg)
DEFINE_ARITHMETIC_HANDLER(f32_ceil)
DEFINE_ARITHMETIC_HANDLER(f32_floor)
DEFINE_ARITHMETIC_HANDLER(f32_trunc)
DEFINE_ARITHMETIC_HANDLER(f32_nearest)
DEFINE_ARITHMETIC_HANDLER(f32_sqrt)
DEFINE_ARITHMETIC_HANDLER(f32_add)
DEFINE_ARITHMETIC_HANDLER(f32_sub)
DEFINE_ARITHMETIC_HANDLER(f32_mul)
DEFINE_ARITHMETIC_HANDLER(f32_div)
DEFINE_ARITHMETIC_HANDLER(f32_min)
DEFINE_ARITHMETIC_HANDLER(f32_max)
DEFINE_ARITHMETIC_HANDLER(f32_copysign)
DEFINE_ARITHMETIC_HANDLER(f64_abs)
DEFINE_ARITHMETIC_HANDLER(f64_neg)
DEFINE_ARITHMETIC_HANDLER(f64_ceil)
DEFINE_ARITHMETIC_HANDLER(f64_floor)
DEFINE_ARITHMETIC_HANDLER(f64_trunc)
DEFINE_ARITHMETIC_HANDLER(f64_nearest)
DEFINE_ARITHMETIC_HANDLER(f64_sqrt)
DEFINE_ARITHMETIC_HANDLER(f64_add)
DEFINE_ARITHMETIC_HANDLER(f64_sub)
DEFINE_ARITHMETIC_HANDLER(f64_mul)
DEFINE_ARITHMETIC_HANDLER(f64_div)
DEFINE_ARITHMETIC_HANDLER(f64_min)
DEFINE_ARITHMETIC_HANDLER(f64_max)
DEFINE_ARITHMETIC_HANDLER(f64_copysign)
DEFINE_ARITHMETIC_HANDLER(i32_wrap_i64)
DEFINE_ARITHMETIC_HANDLER(i32_trunc_f32_s)
DEFINE_ARITHMETIC_HANDLER(i32_trunc_f32_u)
DEFINE_ARITHMETIC_HANDLER(i32_trunc_f64_s)
DEFINE_ARITHMETIC_HANDLER(i32_trunc_f64_u)
DEFINE_ARITHMETIC_HANDLER(i64_extend_i32_s)
DEFINE_ARITHMETIC_HANDLER(i64_extend_i32_u)
DEFINE_ARITHMETIC_HANDLER(i64_trunc_f32_s)
DEFINE_ARITHMETIC_HANDLER(i64_trunc_f32_u)
DEFINE_ARITHMETIC_HANDLER(i64_trunc_f64_s)
DEFINE_ARITHMETIC_HANDLER(i64_trunc_f64_u)
DEFINE_ARITHMETIC_HANDLER(f32_convert_i32_s)
DEFINE_ARITHMETIC_HANDLER(f32_convert_i32_u)
DEFINE_ARITHMETIC_HANDLER(f32_convert_i64_s)
DEFINE_ARITHMETIC_HANDLER(f32_convert_i64_u)
DEFINE_ARITHMETIC_HANDLER(f32_demote_f64)
DEFINE_ARITHMETIC_HANDLER(f64_convert_i32_s)
DEFINE_ARITHMETIC_HANDLER(f64_convert_i32_u)
DEFINE_ARITHMETIC_HANDLER(f64_convert_i64_s)
DEFINE_ARITHMETIC_HANDLER(f64_convert_i64_u)
DEFINE_ARITHMETIC_HANDLER(f64_promote_f32)
DEFINE_ARITHMETIC_HANDLER(i32_reinterpret_f32)
DEFINE_ARITHMETIC_HANDLER(i64_reinterpret_f64)
DEFINE_ARITHMETIC_HANDLER(f32_reinterpret_i32)
DEFINE_ARITHMETIC_HANDLER(f64_reinterpret_i64)
DEFINE_ARITHMETIC_HANDLER(i32_extend8_s)
DEFINE_ARITHMETIC_HANDLER(i32_extend16_s)
DEFINE_ARITHMETIC_HANDLER(i64_extend8_s)
DEFINE_ARITHMETIC_HANDLER(i64_extend16_s)
DEFINE_ARITHMETIC_HANDLER(i64_extend32_s)

// Reserved opcodes
#define DEFINE_RESERVED_HANDLER(name)                                                \
    static NextInstructionResult handle_##name(const uint8_t* pc, const uint8_t* mc) \
    {                                                                                \
        return InstructionAdvance::fixedLength(pc, mc, 1);                           \
    }

DEFINE_RESERVED_HANDLER(reserved_0xc5)
DEFINE_RESERVED_HANDLER(reserved_0xc6)
DEFINE_RESERVED_HANDLER(reserved_0xc7)
DEFINE_RESERVED_HANDLER(reserved_0xc8)
DEFINE_RESERVED_HANDLER(reserved_0xc9)
DEFINE_RESERVED_HANDLER(reserved_0xca)
DEFINE_RESERVED_HANDLER(reserved_0xcb)
DEFINE_RESERVED_HANDLER(reserved_0xcc)
DEFINE_RESERVED_HANDLER(reserved_0xcd)
DEFINE_RESERVED_HANDLER(reserved_0xce)
DEFINE_RESERVED_HANDLER(reserved_0xcf)

// Reference instructions
static NextInstructionResult handle_ref_null_t(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_ref_is_null(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_ref_func(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::withConst32Metadata(pc, mc);
}

static NextInstructionResult handle_ref_eq(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_ref_as_non_null(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::fixedLength(pc, mc, 1);
}

static NextInstructionResult handle_br_on_null(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleBrIf(pc, mc);
}

static NextInstructionResult handle_br_on_non_null(const uint8_t* pc, const uint8_t* mc)
{
    return InstructionAdvance::handleBrIf(pc, mc);
}

// More reserved opcodes
DEFINE_RESERVED_HANDLER(reserved_0xd7)
DEFINE_RESERVED_HANDLER(reserved_0xd8)
DEFINE_RESERVED_HANDLER(reserved_0xd9)
DEFINE_RESERVED_HANDLER(reserved_0xda)
DEFINE_RESERVED_HANDLER(reserved_0xdb)
DEFINE_RESERVED_HANDLER(reserved_0xdc)
DEFINE_RESERVED_HANDLER(reserved_0xdd)
DEFINE_RESERVED_HANDLER(reserved_0xde)
DEFINE_RESERVED_HANDLER(reserved_0xdf)
DEFINE_RESERVED_HANDLER(reserved_0xe0)
DEFINE_RESERVED_HANDLER(reserved_0xe1)
DEFINE_RESERVED_HANDLER(reserved_0xe2)
DEFINE_RESERVED_HANDLER(reserved_0xe3)
DEFINE_RESERVED_HANDLER(reserved_0xe4)
DEFINE_RESERVED_HANDLER(reserved_0xe5)
DEFINE_RESERVED_HANDLER(reserved_0xe6)
DEFINE_RESERVED_HANDLER(reserved_0xe7)
DEFINE_RESERVED_HANDLER(reserved_0xe8)
DEFINE_RESERVED_HANDLER(reserved_0xe9)
DEFINE_RESERVED_HANDLER(reserved_0xea)
DEFINE_RESERVED_HANDLER(reserved_0xeb)
DEFINE_RESERVED_HANDLER(reserved_0xec)
DEFINE_RESERVED_HANDLER(reserved_0xed)
DEFINE_RESERVED_HANDLER(reserved_0xee)
DEFINE_RESERVED_HANDLER(reserved_0xef)
DEFINE_RESERVED_HANDLER(reserved_0xf0)
DEFINE_RESERVED_HANDLER(reserved_0xf1)
DEFINE_RESERVED_HANDLER(reserved_0xf2)
DEFINE_RESERVED_HANDLER(reserved_0xf3)
DEFINE_RESERVED_HANDLER(reserved_0xf4)
DEFINE_RESERVED_HANDLER(reserved_0xf5)
DEFINE_RESERVED_HANDLER(reserved_0xf6)
DEFINE_RESERVED_HANDLER(reserved_0xf7)
DEFINE_RESERVED_HANDLER(reserved_0xf8)
DEFINE_RESERVED_HANDLER(reserved_0xf9)
DEFINE_RESERVED_HANDLER(reserved_0xfa)

// Forward declarations for prefix sub-opcode handlers
#define DECLARE_GC_HANDLER(opcode, name) \
    static NextInstructionResult handle_gc_##name(const uint8_t* pc, const uint8_t* mc);

#define DECLARE_CONVERSION_HANDLER(opcode, name) \
    static NextInstructionResult handle_conversion_##name(const uint8_t* pc, const uint8_t* mc);

#define DECLARE_SIMD_HANDLER(opcode, name) \
    static NextInstructionResult handle_simd_##name(const uint8_t* pc, const uint8_t* mc);

#define DECLARE_ATOMIC_HANDLER(opcode, name) \
    static NextInstructionResult handle_atomic_##name(const uint8_t* pc, const uint8_t* mc);

FOR_EACH_IPINT_GC_OPCODE(DECLARE_GC_HANDLER)
FOR_EACH_IPINT_CONVERSION_OPCODE(DECLARE_CONVERSION_HANDLER)
FOR_EACH_IPINT_SIMD_OPCODE(DECLARE_SIMD_HANDLER)
FOR_EACH_IPINT_ATOMIC_OPCODE(DECLARE_ATOMIC_HANDLER)

// Define handlers for prefix sub-opcodes using macros
#define DEFINE_GC_HANDLER(opcode, name)                                                 \
    static NextInstructionResult handle_gc_##name(const uint8_t* pc, const uint8_t* mc) \
    {                                                                                   \
        return InstructionAdvance::variableLength(pc, mc);                              \
    }

#define DEFINE_CONVERSION_HANDLER(opcode, name)                                                 \
    static NextInstructionResult handle_conversion_##name(const uint8_t* pc, const uint8_t* mc) \
    {                                                                                           \
        return InstructionAdvance::variableLength(pc, mc);                                      \
    }

#define DEFINE_SIMD_HANDLER(opcode, name)                                                 \
    static NextInstructionResult handle_simd_##name(const uint8_t* pc, const uint8_t* mc) \
    {                                                                                     \
        return InstructionAdvance::variableLength(pc, mc);                                \
    }

#define DEFINE_ATOMIC_HANDLER(opcode, name)                                                 \
    static NextInstructionResult handle_atomic_##name(const uint8_t* pc, const uint8_t* mc) \
    {                                                                                       \
        return InstructionAdvance::withConst32Metadata(pc, mc);                             \
    }

FOR_EACH_IPINT_GC_OPCODE(DEFINE_GC_HANDLER)
FOR_EACH_IPINT_CONVERSION_OPCODE(DEFINE_CONVERSION_HANDLER)
FOR_EACH_IPINT_SIMD_OPCODE(DEFINE_SIMD_HANDLER)
FOR_EACH_IPINT_ATOMIC_OPCODE(DEFINE_ATOMIC_HANDLER)

// Prefix instructions that dispatch to sub-opcodes
static NextInstructionResult handle_gc_prefix(const uint8_t* pc, const uint8_t* mc)
{
    uint8_t subOpcode = pc[1];

    switch (subOpcode) {
#define GC_OPCODE_CASE(opcode, name) \
    case opcode:                     \
        return handle_gc_##name(pc, mc);

        FOR_EACH_IPINT_GC_OPCODE(GC_OPCODE_CASE)

    default:
        return InstructionAdvance::variableLength(pc, mc);
    }
}

static NextInstructionResult handle_conversion_prefix(const uint8_t* pc, const uint8_t* mc)
{
    uint8_t subOpcode = pc[1];

    switch (subOpcode) {
#define CONVERSION_OPCODE_CASE(opcode, name) \
    case opcode:                             \
        return handle_conversion_##name(pc, mc);

        FOR_EACH_IPINT_CONVERSION_OPCODE(CONVERSION_OPCODE_CASE)

    default:
        return InstructionAdvance::variableLength(pc, mc);
    }
}

static NextInstructionResult handle_simd_prefix(const uint8_t* pc, const uint8_t* mc)
{
    uint8_t subOpcode = pc[1];

    switch (subOpcode) {
#define SIMD_OPCODE_CASE(opcode, name) \
    case opcode:                       \
        return handle_simd_##name(pc, mc);

        FOR_EACH_IPINT_SIMD_OPCODE(SIMD_OPCODE_CASE)

    default:
        return InstructionAdvance::variableLength(pc, mc);
    }
}

static NextInstructionResult handle_atomic_prefix(const uint8_t* pc, const uint8_t* mc)
{
    uint8_t subOpcode = pc[1];

    switch (subOpcode) {
#define ATOMIC_OPCODE_CASE(opcode, name) \
    case opcode:                         \
        return handle_atomic_##name(pc, mc);

        FOR_EACH_IPINT_ATOMIC_OPCODE(ATOMIC_OPCODE_CASE)

    default:
        return InstructionAdvance::withConst32Metadata(pc, mc);
    }
}

DEFINE_RESERVED_HANDLER(reserved_0xff)

// Main dispatch function using the macro to generate switch cases
NextInstructionResult calculateNextInstruction(uint8_t opcode, const uint8_t* pc, const uint8_t* mc)
{
    switch (opcode) {
#define OPCODE_CASE(opcode, name) \
    case opcode:                  \
        return handle_##name(pc, mc);

        FOR_EACH_IPINT_OPCODE(OPCODE_CASE)

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

namespace InstructionAdvance {

NextInstructionResult fixedLength(const uint8_t* pc, const uint8_t*, uint32_t pcAdvance)
{
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + pcAdvance);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult variableLength(const uint8_t* pc, const uint8_t* mc)
{
    const InstructionLengthMetadata* metadata = reinterpret_cast<const InstructionLengthMetadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->length);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult withConst32Metadata(const uint8_t* pc, const uint8_t* mc)
{
    const Const32Metadata* metadata = reinterpret_cast<const Const32Metadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->instructionLength.length);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult withConst64Metadata(const uint8_t* pc, const uint8_t* mc)
{
    const Const64Metadata* metadata = reinterpret_cast<const Const64Metadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->instructionLength.length);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult withGlobalMetadata(const uint8_t* pc, const uint8_t* mc)
{
    const GlobalMetadata* metadata = reinterpret_cast<const GlobalMetadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->instructionLength.length);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult withBlockMetadata(const uint8_t* pc, const uint8_t* mc)
{
    const BlockMetadata* metadata = reinterpret_cast<const BlockMetadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->deltaPC);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult handleIf(const uint8_t* pc, const uint8_t* mc)
{
    const IfMetadata* metadata = reinterpret_cast<const IfMetadata*>(mc);

    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->instructionLength.length);
    result.isConditionalBranch = true;
    result.elsePC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->elseDeltaPC);
    return result;
}

NextInstructionResult handleBr(const uint8_t* pc, const uint8_t* mc)
{
    const BlockMetadata* metadata = reinterpret_cast<const BlockMetadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + static_cast<int32_t>(metadata->deltaPC));
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult handleBrIf(const uint8_t* pc, const uint8_t* mc)
{
    NextInstructionResult result = handleBr(pc, mc);
    const BranchMetadata* metadata = reinterpret_cast<const BranchMetadata*>(mc);
    result.isConditionalBranch = true;
    result.elsePC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->instructionLength.length);
    return result;
}

NextInstructionResult handleBrTable(const uint8_t* pc, const uint8_t* mc)
{
    // br_table is complex - for simplicity, assume it uses variable length
    return variableLength(pc, mc);
}

NextInstructionResult handleLocalGet(const uint8_t* pc, const uint8_t* mc)
{
    // For simplicity, assume all local instructions use 2 bytes (opcode + index)
    // In reality, this would need ULEB128 decoding for indices >= 128
    return fixedLength(pc, mc, 2);
}

NextInstructionResult handleLocalSet(const uint8_t* pc, const uint8_t* mc)
{
    // For simplicity, assume all local instructions use 2 bytes
    return fixedLength(pc, mc, 2);
}

NextInstructionResult handleLocalTee(const uint8_t* pc, const uint8_t* mc)
{
    // For simplicity, assume all local instructions use 2 bytes
    return fixedLength(pc, mc, 2);
}

NextInstructionResult handleCall(const uint8_t* pc, const uint8_t* mc)
{
    const CallMetadata* metadata = reinterpret_cast<const CallMetadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->length);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult handleCallIndirect(const uint8_t* pc, const uint8_t* mc)
{
    const CallIndirectMetadata* metadata = reinterpret_cast<const CallIndirectMetadata*>(mc);
    NextInstructionResult result;
    result.nextPC = reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(pc) + metadata->length);
    result.isConditionalBranch = false;
    result.elsePC = nullptr;
    return result;
}

NextInstructionResult handleGCPrefix(const uint8_t* pc, const uint8_t* mc)
{
    // GC prefix instructions - use variable length
    return variableLength(pc, mc);
}

NextInstructionResult handleConversionPrefix(const uint8_t* pc, const uint8_t* mc)
{
    // Conversion prefix instructions - most are fixed length 2
    return fixedLength(pc, mc, 2);
}

NextInstructionResult handleSIMDPrefix(const uint8_t* pc, const uint8_t* mc)
{
    // SIMD prefix instructions - use variable length
    return variableLength(pc, mc);
}

NextInstructionResult handleAtomicPrefix(const uint8_t* pc, const uint8_t* mc)
{
    // Atomic prefix instructions - use const32 metadata
    return withConst32Metadata(pc, mc);
}

} // namespace InstructionAdvance

}
} // namespace JSC::IPInt

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
