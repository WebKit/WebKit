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
extern "C" void ipint_entry_simd();
extern "C" void ipint_catch_entry();
extern "C" void ipint_catch_all_entry();

#define IPINT_VALIDATE_DEFINE_FUNCTION(opcode, name) \
    extern "C" void ipint_ ## name ## _validate() REFERENCED_FROM_ASM WTF_INTERNAL NO_REORDER;

#define FOR_EACH_IPINT_OPCODE(m) \
    m(0x00, unreachable) \
    m(0x01, nop) \
    m(0x02, block) \
    m(0x03, loop) \
    m(0x04, if) \
    m(0x05, else) \
    m(0x06, try) \
    m(0x07, catch) \
    m(0x08, throw) \
    m(0x09, rethrow) \
    m(0x0b, end) \
    m(0x0c, br) \
    m(0x0d, br_if) \
    m(0x0e, br_table) \
    m(0x0f, return) \
    m(0x10, call) \
    m(0x11, call_indirect) \
    m(0x18, delegate) \
    m(0x19, catch_all) \
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
    m(0xfd, simd) \
    m(0xfe, atomic)

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

#define FOR_EACH_IPINT_SIMD_OPCODE(m) \
    m(0x00, simd_v128_load_mem) \
    m(0x01, simd_v128_load_8x8s_mem) \
    m(0x02, simd_v128_load_8x8u_mem) \
    m(0x03, simd_v128_load_16x4s_mem) \
    m(0x04, simd_v128_load_16x4u_mem) \
    m(0x05, simd_v128_load_32x2s_mem) \
    m(0x06, simd_v128_load_32x2u_mem) \
    m(0x07, simd_v128_load8_splat_mem) \
    m(0x08, simd_v128_load16_splat_mem) \
    m(0x09, simd_v128_load32_splat_mem) \
    m(0x0a, simd_v128_load64_splat_mem) \
    m(0x0b, simd_v128_store_mem) \
    m(0x0c, simd_v128_const) \
    m(0x0d, simd_i8x16_shuffle) \
    m(0x0e, simd_i8x16_swizzle) \
    m(0x0f, simd_i8x16_splat) \
    m(0x10, simd_i16x8_splat) \
    m(0x11, simd_i32x4_splat) \
    m(0x12, simd_i64x2_splat) \
    m(0x13, simd_f32x4_splat) \
    m(0x14, simd_f64x2_splat) \
    m(0x15, simd_i8x16_extract_lane_s) \
    m(0x16, simd_i8x16_extract_lane_u) \
    m(0x17, simd_i8x16_replace_lane) \
    m(0x18, simd_i16x8_extract_lane_s) \
    m(0x19, simd_i16x8_extract_lane_u) \
    m(0x1a, simd_i16x8_replace_lane) \
    m(0x1b, simd_i32x4_extract_lane) \
    m(0x1c, simd_i32x4_replace_lane) \
    m(0x1d, simd_i64x2_extract_lane) \
    m(0x1e, simd_i64x2_replace_lane) \
    m(0x1f, simd_f32x4_extract_lane) \
    m(0x20, simd_f32x4_replace_lane) \
    m(0x21, simd_f64x2_extract_lane) \
    m(0x22, simd_f64x2_replace_lane) \
    m(0x23, simd_i8x16_eq) \
    m(0x24, simd_i8x16_ne) \
    m(0x25, simd_i8x16_lt_s) \
    m(0x26, simd_i8x16_lt_u) \
    m(0x27, simd_i8x16_gt_s) \
    m(0x28, simd_i8x16_gt_u) \
    m(0x29, simd_i8x16_le_s) \
    m(0x2a, simd_i8x16_le_u) \
    m(0x2b, simd_i8x16_ge_s) \
    m(0x2c, simd_i8x16_ge_u) \
    m(0x2d, simd_i16x8_eq) \
    m(0x2e, simd_i16x8_ne) \
    m(0x2f, simd_i16x8_lt_s) \
    m(0x30, simd_i16x8_lt_u) \
    m(0x31, simd_i16x8_gt_s) \
    m(0x32, simd_i16x8_gt_u) \
    m(0x33, simd_i16x8_le_s) \
    m(0x34, simd_i16x8_le_u) \
    m(0x35, simd_i16x8_ge_s) \
    m(0x36, simd_i16x8_ge_u) \
    m(0x37, simd_i32x4_eq) \
    m(0x38, simd_i32x4_ne) \
    m(0x39, simd_i32x4_lt_s) \
    m(0x3a, simd_i32x4_lt_u) \
    m(0x3b, simd_i32x4_gt_s) \
    m(0x3c, simd_i32x4_gt_u) \
    m(0x3d, simd_i32x4_le_s) \
    m(0x3e, simd_i32x4_le_u) \
    m(0x3f, simd_i32x4_ge_s) \
    m(0x40, simd_i32x4_ge_u) \
    m(0x41, simd_f32x4_eq) \
    m(0x42, simd_f32x4_ne) \
    m(0x43, simd_f32x4_lt) \
    m(0x44, simd_f32x4_gt) \
    m(0x45, simd_f32x4_le) \
    m(0x46, simd_f32x4_ge) \
    m(0x47, simd_f64x2_eq) \
    m(0x48, simd_f64x2_ne) \
    m(0x49, simd_f64x2_lt) \
    m(0x4a, simd_f64x2_gt) \
    m(0x4b, simd_f64x2_le) \
    m(0x4c, simd_f64x2_ge) \
    m(0x4d, simd_v128_not) \
    m(0x4e, simd_v128_and) \
    m(0x4f, simd_v128_andnot) \
    m(0x50, simd_v128_or) \
    m(0x51, simd_v128_xor) \
    m(0x52, simd_v128_bitselect) \
    m(0x53, simd_v128_any_true) \
    m(0x54, simd_v128_load8_lane_mem) \
    m(0x55, simd_v128_load16_lane_mem) \
    m(0x56, simd_v128_load32_lane_mem) \
    m(0x57, simd_v128_load64_lane_mem) \
    m(0x58, simd_v128_store8_lane_mem) \
    m(0x59, simd_v128_store16_lane_mem) \
    m(0x5a, simd_v128_store32_lane_mem) \
    m(0x5b, simd_v128_store64_lane_mem) \
    m(0x5c, simd_v128_load32_zero_mem) \
    m(0x5d, simd_v128_load64_zero_mem) \
    m(0x5e, simd_f32x4_demote_f64x2_zero) \
    m(0x5f, simd_f64x2_promote_low_f32x4) \
    m(0x60, simd_i8x16_abs) \
    m(0x61, simd_i8x16_neg) \
    m(0x62, simd_i8x16_popcnt) \
    m(0x63, simd_i8x16_all_true) \
    m(0x64, simd_i8x16_bitmask) \
    m(0x65, simd_i8x16_narrow_i16x8_s) \
    m(0x66, simd_i8x16_narrow_i16x8_u) \
    m(0x67, simd_f32x4_ceil) \
    m(0x68, simd_f32x4_floor) \
    m(0x69, simd_f32x4_trunc) \
    m(0x6a, simd_f32x4_nearest) \
    m(0x6b, simd_i8x16_shl) \
    m(0x6c, simd_i8x16_shr_s) \
    m(0x6d, simd_i8x16_shr_u) \
    m(0x6e, simd_i8x16_add) \
    m(0x6f, simd_i8x16_add_sat_s) \
    m(0x70, simd_i8x16_add_sat_u) \
    m(0x71, simd_i8x16_sub) \
    m(0x72, simd_i8x16_sub_sat_s) \
    m(0x73, simd_i8x16_sub_sat_u) \
    m(0x74, simd_f64x2_ceil) \
    m(0x75, simd_f64x2_floor) \
    m(0x76, simd_i8x16_min_s) \
    m(0x77, simd_i8x16_min_u) \
    m(0x78, simd_i8x16_max_s) \
    m(0x79, simd_i8x16_max_u) \
    m(0x7a, simd_f64x2_trunc) \
    m(0x7b, simd_i8x16_avgr_u) \
    m(0x7c, simd_i16x8_extadd_pairwise_i8x16_s) \
    m(0x7d, simd_i16x8_extadd_pairwise_i8x16_u) \
    m(0x7e, simd_i32x4_extadd_pairwise_i16x8_s) \
    m(0x7f, simd_i32x4_extadd_pairwise_i16x8_u) \
    m(0x80, simd_i16x8_abs) \
    m(0x81, simd_i16x8_neg) \
    m(0x82, simd_i16x8_q15mulr_sat_s) \
    m(0x83, simd_i16x8_all_true) \
    m(0x84, simd_i16x8_bitmask) \
    m(0x85, simd_i16x8_narrow_i32x4_s) \
    m(0x86, simd_i16x8_narrow_i32x4_u) \
    m(0x87, simd_i16x8_extend_low_i8x16_s) \
    m(0x88, simd_i16x8_extend_high_i8x16_s) \
    m(0x89, simd_i16x8_extend_low_i8x16_u) \
    m(0x8a, simd_i16x8_extend_high_i8x16_u) \
    m(0x8b, simd_i16x8_shl) \
    m(0x8c, simd_i16x8_shr_s) \
    m(0x8d, simd_i16x8_shr_u) \
    m(0x8e, simd_i16x8_add) \
    m(0x8f, simd_i16x8_add_sat_s) \
    m(0x90, simd_i16x8_add_sat_u) \
    m(0x91, simd_i16x8_sub) \
    m(0x92, simd_i16x8_sub_sat_s) \
    m(0x93, simd_i16x8_sub_sat_u) \
    m(0x94, simd_f64x2_nearest) \
    m(0x95, simd_i16x8_mul) \
    m(0x96, simd_i16x8_min_s) \
    m(0x97, simd_i16x8_min_u) \
    m(0x98, simd_i16x8_max_s) \
    m(0x99, simd_i16x8_max_u) \
    m(0x9b, simd_i16x8_avgr_u) \
    m(0x9c, simd_i16x8_extmul_low_i8x16_s) \
    m(0x9d, simd_i16x8_extmul_high_i8x16_s) \
    m(0x9e, simd_i16x8_extmul_low_i8x16_u) \
    m(0x9f, simd_i16x8_extmul_high_i8x16_u) \
    m(0xa0, simd_i32x4_abs) \
    m(0xa1, simd_i32x4_neg) \
    m(0xa3, simd_i32x4_all_true) \
    m(0xa4, simd_i32x4_bitmask) \
    m(0xa7, simd_i32x4_extend_low_i16x8_s) \
    m(0xa8, simd_i32x4_extend_high_i16x8_s) \
    m(0xa9, simd_i32x4_extend_low_i16x8_u) \
    m(0xaa, simd_i32x4_extend_high_i16x8_u) \
    m(0xab, simd_i32x4_shl) \
    m(0xac, simd_i32x4_shr_s) \
    m(0xad, simd_i32x4_shr_u) \
    m(0xae, simd_i32x4_add) \
    m(0xb1, simd_i32x4_sub) \
    m(0xb5, simd_i32x4_mul) \
    m(0xb6, simd_i32x4_min_s) \
    m(0xb7, simd_i32x4_min_u) \
    m(0xb8, simd_i32x4_max_s) \
    m(0xb9, simd_i32x4_max_u) \
    m(0xba, simd_i32x4_dot_i16x8_s) \
    m(0xbc, simd_i32x4_extmul_low_i16x8_s) \
    m(0xbd, simd_i32x4_extmul_high_i16x8_s) \
    m(0xbe, simd_i32x4_extmul_low_i16x8_u) \
    m(0xbf, simd_i32x4_extmul_high_i16x8_u) \
    m(0xc0, simd_i64x2_abs) \
    m(0xc1, simd_i64x2_neg) \
    m(0xc3, simd_i64x2_all_true) \
    m(0xc4, simd_i64x2_bitmask) \
    m(0xc7, simd_i64x2_extend_low_i32x4_s) \
    m(0xc8, simd_i64x2_extend_high_i32x4_s) \
    m(0xc9, simd_i64x2_extend_low_i32x4_u) \
    m(0xca, simd_i64x2_extend_high_i32x4_u) \
    m(0xcb, simd_i64x2_shl) \
    m(0xcc, simd_i64x2_shr_s) \
    m(0xcd, simd_i64x2_shr_u) \
    m(0xce, simd_i64x2_add) \
    m(0xd1, simd_i64x2_sub) \
    m(0xd5, simd_i64x2_mul) \
    m(0xd6, simd_i64x2_eq) \
    m(0xd7, simd_i64x2_ne) \
    m(0xd8, simd_i64x2_lt_s) \
    m(0xd9, simd_i64x2_gt_s) \
    m(0xda, simd_i64x2_le_s) \
    m(0xdb, simd_i64x2_ge_s) \
    m(0xdc, simd_i64x2_extmul_low_i32x4_s) \
    m(0xdd, simd_i64x2_extmul_high_i32x4_s) \
    m(0xde, simd_i64x2_extmul_low_i32x4_u) \
    m(0xdf, simd_i64x2_extmul_high_i32x4_u) \
    m(0xe0, simd_f32x4_abs) \
    m(0xe1, simd_f32x4_neg) \
    m(0xe3, simd_f32x4_sqrt) \
    m(0xe4, simd_f32x4_add) \
    m(0xe5, simd_f32x4_sub) \
    m(0xe6, simd_f32x4_mul) \
    m(0xe7, simd_f32x4_div) \
    m(0xe8, simd_f32x4_min) \
    m(0xe9, simd_f32x4_max) \
    m(0xea, simd_f32x4_pmin) \
    m(0xeb, simd_f32x4_pmax) \
    m(0xec, simd_f64x2_abs) \
    m(0xed, simd_f64x2_neg) \
    m(0xef, simd_f64x2_sqrt) \
    m(0xf0, simd_f64x2_add) \
    m(0xf1, simd_f64x2_sub) \
    m(0xf2, simd_f64x2_mul) \
    m(0xf3, simd_f64x2_div) \
    m(0xf4, simd_f64x2_min) \
    m(0xf5, simd_f64x2_max) \
    m(0xf6, simd_f64x2_pmin) \
    m(0xf7, simd_f64x2_pmax) \
    m(0xf8, simd_i32x4_trunc_sat_f32x4_s) \
    m(0xf9, simd_i32x4_trunc_sat_f32x4_u) \
    m(0xfa, simd_f32x4_convert_i32x4_s) \
    m(0xfb, simd_f32x4_convert_i32x4_u) \
    m(0xfc, simd_i32x4_trunc_sat_f64x2_s_zero) \
    m(0xfd, simd_i32x4_trunc_sat_f64x2_u_zero) \
    m(0xfe, simd_f64x2_convert_low_i32x4_s) \
    m(0xff, simd_f64x2_convert_low_i32x4_u)

#define FOR_EACH_IPINT_ATOMIC_OPCODE(m) \
    m(0x00, memory_atomic_notify) \
    m(0x01, memory_atomic_wait32) \
    m(0x02, memory_atomic_wait64) \
    m(0x03, atomic_fence) \
    m(0x10, i32_atomic_load) \
    m(0x11, i64_atomic_load) \
    m(0x12, i32_atomic_load8_u) \
    m(0x13, i32_atomic_load16_u) \
    m(0x14, i64_atomic_load8_u) \
    m(0x15, i64_atomic_load16_u) \
    m(0x16, i64_atomic_load32_u) \
    m(0x17, i32_atomic_store) \
    m(0x18, i64_atomic_store) \
    m(0x19, i32_atomic_store8_u) \
    m(0x1a, i32_atomic_store16_u) \
    m(0x1b, i64_atomic_store8_u) \
    m(0x1c, i64_atomic_store16_u) \
    m(0x1d, i64_atomic_store32_u) \
    m(0x1e, i32_atomic_rmw_add) \
    m(0x1f, i64_atomic_rmw_add) \
    m(0x20, i32_atomic_rmw8_add_u) \
    m(0x21, i32_atomic_rmw16_add_u) \
    m(0x22, i64_atomic_rmw8_add_u) \
    m(0x23, i64_atomic_rmw16_add_u) \
    m(0x24, i64_atomic_rmw32_add_u) \
    m(0x25, i32_atomic_rmw_sub) \
    m(0x26, i64_atomic_rmw_sub) \
    m(0x27, i32_atomic_rmw8_sub_u) \
    m(0x28, i32_atomic_rmw16_sub_u) \
    m(0x29, i64_atomic_rmw8_sub_u) \
    m(0x2a, i64_atomic_rmw16_sub_u) \
    m(0x2b, i64_atomic_rmw32_sub_u) \
    m(0x2c, i32_atomic_rmw_and) \
    m(0x2d, i64_atomic_rmw_and) \
    m(0x2e, i32_atomic_rmw8_and_u) \
    m(0x2f, i32_atomic_rmw16_and_u) \
    m(0x30, i64_atomic_rmw8_and_u) \
    m(0x31, i64_atomic_rmw16_and_u) \
    m(0x32, i64_atomic_rmw32_and_u) \
    m(0x33, i32_atomic_rmw_or) \
    m(0x34, i64_atomic_rmw_or) \
    m(0x35, i32_atomic_rmw8_or_u) \
    m(0x36, i32_atomic_rmw16_or_u) \
    m(0x37, i64_atomic_rmw8_or_u) \
    m(0x38, i64_atomic_rmw16_or_u) \
    m(0x39, i64_atomic_rmw32_or_u) \
    m(0x3a, i32_atomic_rmw_xor) \
    m(0x3b, i64_atomic_rmw_xor) \
    m(0x3c, i32_atomic_rmw8_xor_u) \
    m(0x3d, i32_atomic_rmw16_xor_u) \
    m(0x3e, i64_atomic_rmw8_xor_u) \
    m(0x3f, i64_atomic_rmw16_xor_u) \
    m(0x40, i64_atomic_rmw32_xor_u) \
    m(0x41, i32_atomic_rmw_xchg) \
    m(0x42, i64_atomic_rmw_xchg) \
    m(0x43, i32_atomic_rmw8_xchg_u) \
    m(0x44, i32_atomic_rmw16_xchg_u) \
    m(0x45, i64_atomic_rmw8_xchg_u) \
    m(0x46, i64_atomic_rmw16_xchg_u) \
    m(0x47, i64_atomic_rmw32_xchg_u) \
    m(0x48, i32_atomic_rmw_cmpxchg) \
    m(0x49, i64_atomic_rmw_cmpxchg) \
    m(0x4a, i32_atomic_rmw8_cmpxchg_u) \
    m(0x4b, i32_atomic_rmw16_cmpxchg_u) \
    m(0x4c, i64_atomic_rmw8_cmpxchg_u) \
    m(0x4d, i64_atomic_rmw16_cmpxchg_u) \
    m(0x4e, i64_atomic_rmw32_cmpxchg_u) \


#if !ENABLE(C_LOOP) && CPU(ADDRESS64) && (CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS)))
FOR_EACH_IPINT_OPCODE(IPINT_VALIDATE_DEFINE_FUNCTION);
FOR_EACH_IPINT_0xFC_TRUNC_OPCODE(IPINT_VALIDATE_DEFINE_FUNCTION);
FOR_EACH_IPINT_SIMD_OPCODE(IPINT_VALIDATE_DEFINE_FUNCTION);
FOR_EACH_IPINT_ATOMIC_OPCODE(IPINT_VALIDATE_DEFINE_FUNCTION);
#endif

namespace JSC { namespace IPInt {

void initialize();

} }

#endif // ENABLE(WEBASSEMBLY)
