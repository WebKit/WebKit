/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

extern "C" void ipint_entry();

#define IPINT_VALIDATE_DEFINE_FUNCTION(opcode, name) \
    extern "C" void ipint_ ## name ## _validate() REFERENCED_FROM_ASM WTF_INTERNAL;

#define FOR_EACH_IPINT_OPCODE(m) \
    m(0x00, unreachable) \
    m(0x01, nop) \
    m(0x02, block) \
    m(0x03, loop) \
    m(0x04, if) \
    m(0x05, else) \
    m(0x0b, end) \
    m(0x0c, br) \
    m(0x0d, br_if) \
    m(0x0e, br_table) \
    m(0x0f, return) \
    m(0x10, call) \
    m(0x11, call_indirect) \
    m(0x1a, drop) \
    m(0x1b, select) \
    m(0x1c, select_t) \
    m(0x20, local_get) \
    m(0x21, local_set) \
    m(0x22, local_tee) \
    m(0x23, global_get) \
    m(0x24, global_set) \
    m(0x25, table_get) \
    m(0x26, table_set) \
    m(0x28, i32_load_mem) \
    m(0x29, i64_load_mem) \
    m(0x2a, f32_load_mem) \
    m(0x2b, f64_load_mem) \
    m(0x2c, i32_load8s_mem) \
    m(0x2d, i32_load8u_mem) \
    m(0x2e, i32_load16s_mem) \
    m(0x2f, i32_load16u_mem) \
    m(0x30, i64_load8s_mem) \
    m(0x31, i64_load8u_mem) \
    m(0x32, i64_load16s_mem) \
    m(0x33, i64_load16u_mem) \
    m(0x34, i64_load32s_mem) \
    m(0x35, i64_load32u_mem) \
    m(0x36, i32_store_mem) \
    m(0x37, i64_store_mem) \
    m(0x38, f32_store_mem) \
    m(0x39, f64_store_mem) \
    m(0x3a, i32_store8_mem) \
    m(0x3b, i32_store16_mem) \
    m(0x3c, i64_store8_mem) \
    m(0x3d, i64_store16_mem) \
    m(0x3e, i64_store32_mem) \
    m(0x3f, memory_size) \
    m(0x40, memory_grow) \
    m(0x41, i32_const) \
    m(0x42, i64_const) \
    m(0x43, f32_const) \
    m(0x44, f64_const) \
    m(0x45, i32_eqz) \
    m(0x46, i32_eq) \
    m(0x47, i32_ne) \
    m(0x48, i32_lt_s) \
    m(0x49, i32_lt_u) \
    m(0x4a, i32_gt_s) \
    m(0x4b, i32_gt_u) \
    m(0x4c, i32_le_s) \
    m(0x4d, i32_le_u) \
    m(0x4e, i32_ge_s) \
    m(0x4f, i32_ge_u) \
    m(0x50, i64_eqz) \
    m(0x51, i64_eq) \
    m(0x52, i64_ne) \
    m(0x53, i64_lt_s) \
    m(0x54, i64_lt_u) \
    m(0x55, i64_gt_s) \
    m(0x56, i64_gt_u) \
    m(0x57, i64_le_s) \
    m(0x58, i64_le_u) \
    m(0x59, i64_ge_s) \
    m(0x5a, i64_ge_u) \
    m(0x5b, f32_eq) \
    m(0x5c, f32_ne) \
    m(0x5d, f32_lt) \
    m(0x5e, f32_gt) \
    m(0x5f, f32_le) \
    m(0x60, f32_ge) \
    m(0x61, f64_eq) \
    m(0x62, f64_ne) \
    m(0x63, f64_lt) \
    m(0x64, f64_gt) \
    m(0x65, f64_le) \
    m(0x66, f64_ge) \
    m(0x67, i32_clz) \
    m(0x68, i32_ctz) \
    m(0x69, i32_popcnt) \
    m(0x6a, i32_add) \
    m(0x6b, i32_sub) \
    m(0x6c, i32_mul) \
    m(0x6d, i32_div_s) \
    m(0x6e, i32_div_u) \
    m(0x6f, i32_rem_s) \
    m(0x70, i32_rem_u) \
    m(0x71, i32_and) \
    m(0x72, i32_or) \
    m(0x73, i32_xor) \
    m(0x74, i32_shl) \
    m(0x75, i32_shr_s) \
    m(0x76, i32_shr_u) \
    m(0x77, i32_rotl) \
    m(0x78, i32_rotr) \
    m(0x79, i64_clz) \
    m(0x7a, i64_ctz) \
    m(0x7b, i64_popcnt) \
    m(0x7c, i64_add) \
    m(0x7d, i64_sub) \
    m(0x7e, i64_mul) \
    m(0x7f, i64_div_s) \
    m(0x80, i64_div_u) \
    m(0x81, i64_rem_s) \
    m(0x82, i64_rem_u) \
    m(0x83, i64_and) \
    m(0x84, i64_or) \
    m(0x85, i64_xor) \
    m(0x86, i64_shl) \
    m(0x87, i64_shr_s) \
    m(0x88, i64_shr_u) \
    m(0x89, i64_rotl) \
    m(0x8a, i64_rotr) \
    m(0x8b, f32_abs) \
    m(0x8c, f32_neg) \
    m(0x8d, f32_ceil) \
    m(0x8e, f32_floor) \
    m(0x8f, f32_trunc) \
    m(0x90, f32_nearest) \
    m(0x91, f32_sqrt) \
    m(0x92, f32_add) \
    m(0x93, f32_sub) \
    m(0x94, f32_mul) \
    m(0x95, f32_div) \
    m(0x96, f32_min) \
    m(0x97, f32_max) \
    m(0x98, f32_copysign) \
    m(0x99, f64_abs) \
    m(0x9a, f64_neg) \
    m(0x9b, f64_ceil) \
    m(0x9c, f64_floor) \
    m(0x9d, f64_trunc) \
    m(0x9e, f64_nearest) \
    m(0x9f, f64_sqrt) \
    m(0xa0, f64_add) \
    m(0xa1, f64_sub) \
    m(0xa2, f64_mul) \
    m(0xa3, f64_div) \
    m(0xa4, f64_min) \
    m(0xa5, f64_max) \
    m(0xa6, f64_copysign) \
    m(0xa7, i32_wrap_i64) \
    m(0xa8, i32_trunc_f32_s) \
    m(0xa9, i32_trunc_f32_u) \
    m(0xaa, i32_trunc_f64_s) \
    m(0xab, i32_trunc_f64_u) \
    m(0xac, i64_extend_i32_s) \
    m(0xad, i64_extend_i32_u) \
    m(0xae, i64_trunc_f32_s) \
    m(0xaf, i64_trunc_f32_u) \
    m(0xb0, i64_trunc_f64_s) \
    m(0xb1, i64_trunc_f64_u) \
    m(0xb2, f32_convert_i32_s) \
    m(0xb3, f32_convert_i32_u) \
    m(0xb4, f32_convert_i64_s) \
    m(0xb5, f32_convert_i64_u) \
    m(0xb6, f32_demote_f64) \
    m(0xb7, f64_convert_i32_s) \
    m(0xb8, f64_convert_i32_u) \
    m(0xb9, f64_convert_i64_s) \
    m(0xba, f64_convert_i64_u) \
    m(0xbb, f64_promote_f32) \
    m(0xbc, i32_reinterpret_f32) \
    m(0xbd, i64_reinterpret_f64) \
    m(0xbe, f32_reinterpret_i32) \
    m(0xbf, f64_reinterpret_i64) \
    m(0xc0, i32_extend8_s) \
    m(0xc1, i32_extend16_s) \
    m(0xc2, i64_extend8_s) \
    m(0xc3, i64_extend16_s) \
    m(0xc4, i64_extend32_s) \
    m(0xd0, ref_null_t) \
    m(0xd1, ref_is_null) \
    m(0xd2, ref_func) \
    m(0xfc, fc_block) \
    m(0xfd, simd)

#define FOR_EACH_IPINT_0xFC_TRUNC_OPCODE(m) \
    m(0x00, i32_trunc_sat_f32_s) \
    m(0x01, i32_trunc_sat_f32_u) \
    m(0x02, i32_trunc_sat_f64_s) \
    m(0x03, i32_trunc_sat_f64_u) \
    m(0x04, i64_trunc_sat_f32_s) \
    m(0x05, i64_trunc_sat_f32_u) \
    m(0x06, i64_trunc_sat_f64_s) \
    m(0x07, i64_trunc_sat_f64_u) \
    m(0x08, memory_init) \
    m(0x09, data_drop) \
    m(0x0a, memory_copy) \
    m(0x0b, memory_fill) \
    m(0x0c, table_init) \
    m(0x0d, elem_drop) \
    m(0x0e, table_copy) \
    m(0x0f, table_grow) \
    m(0x10, table_size) \
    m(0x11, table_fill)

#if !ENABLE(C_LOOP) && CPU(ADDRESS64) && (CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS)))
FOR_EACH_IPINT_OPCODE(IPINT_VALIDATE_DEFINE_FUNCTION);
FOR_EACH_IPINT_0xFC_TRUNC_OPCODE(IPINT_VALIDATE_DEFINE_FUNCTION);
#endif

namespace JSC { namespace IPInt {

void initialize();

} }

#endif // ENABLE(WEBASSEMBLY)
