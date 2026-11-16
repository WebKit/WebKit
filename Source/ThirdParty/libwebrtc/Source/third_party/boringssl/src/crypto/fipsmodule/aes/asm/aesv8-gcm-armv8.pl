#! /usr/bin/env perl

# Copyright (c) 2022, ARM Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#========================================================================
# Written by Fangming Fang <fangming.fang@arm.com> for the OpenSSL project,
# derived from https://github.com/ARM-software/AArch64cryptolib, original
# author Samuel Lee <Samuel.Lee@arm.com>.
#========================================================================
#
# Approach - assume we don't want to reload constants, so reserve ~half of
# vector register file for constants
#
# main loop to act on 4 16B blocks per iteration, and then do modulo of the
# accumulated intermediate hashes from the 4 blocks
#
#  ____________________________________________________
# |                                                    |
# | PRE                                                |
# |____________________________________________________|
# |                |                |                  |
# | CTR block 4k+8 | AES block 4k+4 | GHASH block 4k+0 |
# |________________|________________|__________________|
# |                |                |                  |
# | CTR block 4k+9 | AES block 4k+5 | GHASH block 4k+1 |
# |________________|________________|__________________|
# |                |                |                  |
# | CTR block 4k+10| AES block 4k+6 | GHASH block 4k+2 |
# |________________|________________|__________________|
# |                |                |                  |
# | CTR block 4k+11| AES block 4k+7 | GHASH block 4k+3 |
# |________________|____(mostly)____|__________________|
# |                                                    |
# | MODULO                                             |
# |____________________________________________________|
#
# PRE: Ensure previous generated intermediate hash is aligned and merged with
# result for GHASH 4k+0
#
# EXT low_acc, low_acc, low_acc, #8
# EOR res_curr (4k+0), res_curr (4k+0), low_acc
#
# CTR block: Increment and byte reverse counter in scalar registers and transfer
# to SIMD registers
#
# REV     ctr32, rev_ctr32
# ORR     ctr64, constctr96_top32, ctr32, LSL #32
# // Keeping this in scalar registers to free up space in SIMD RF
# INS     ctr_next.d[0], constctr96_bottom64
# INS     ctr_next.d[1], ctr64X
# ADD     rev_ctr32, #1
#
# AES block:
#
# Do AES encryption/decryption on CTR block X and EOR it with input block X.
# Take 256 bytes key below for example. Doing small trick here of loading input
# in scalar registers, EORing with last key and then transferring Given we are
# very constrained in our ASIMD registers this is quite important
#
#     Encrypt:
# LDR     input_low, [ input_ptr  ], #8
# LDR     input_high, [ input_ptr  ], #8
# EOR     input_low, k14_low
# EOR     input_high, k14_high
# INS     res_curr.d[0], input_low
# INS     res_curr.d[1], input_high
# AESE    ctr_curr, k0; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k1; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k2; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k3; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k4; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k5; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k6; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k7; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k8; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k9; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k10; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k11; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k12; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k13
# EOR     res_curr, res_curr, ctr_curr
# ST1     { res_curr.16b  }, [ output_ptr  ], #16
#
#     Decrypt:
# AESE    ctr_curr, k0; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k1; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k2; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k3; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k4; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k5; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k6; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k7; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k8; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k9; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k10; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k11; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k12; AESMC ctr_curr, ctr_curr
# AESE    ctr_curr, k13
# LDR     res_curr, [ input_ptr  ], #16
# EOR     res_curr, res_curr, ctr_curr
# MOV     output_low, res_curr.d[0]
# MOV     output_high, res_curr.d[1]
# EOR     output_low, k14_low
# EOR     output_high, k14_high
# STP     output_low, output_high, [ output_ptr  ], #16
#
# GHASH block X:
#     Do 128b karatsuba polynomial multiplication on block. We only have
#     64b->128b polynomial multipliers, naively that means we need to do 4 64b
#     multiplies to generate a 128b.
#
# multiplication:
#     Pmull(A,B) == (Pmull(Ah,Bh)<<128 | Pmull(Al,Bl)) ^
#                   (Pmull(Ah,Bl) ^ Pmull(Al,Bh))<<64
#
#     The idea behind Karatsuba multiplication is that we can do just 3 64b
#     multiplies:
#     Pmull(A,B) == (Pmull(Ah,Bh)<<128 | Pmull(Al,Bl)) ^
#                   (Pmull(Ah^Al,Bh^Bl) ^ Pmull(Ah,Bh) ^
#                   Pmull(Al,Bl))<<64
#
#     There is some complication here because the bit order of GHASH's PMULL is
#     reversed compared to elsewhere, so we are multiplying with "twisted"
#     powers of H
#
# Note: We can PMULL directly into the acc_x in first GHASH of the loop
#
# Note: For scheduling big cores we want to split the processing to happen over
#       two loop iterations - otherwise the critical path latency dominates the
#       performance.
#
#       This has a knock on effect on register pressure, so we have to be a bit
#       more clever with our temporary registers than indicated here
#
# REV64   res_curr, res_curr
# INS     t_m.d[0], res_curr.d[1]
# EOR     t_m.8B, t_m.8B, res_curr.8B
# PMULL2  t_h, res_curr, HX
# PMULL   t_l, res_curr, HX
# PMULL   t_m, t_m, HX_k
# EOR     acc_h, acc_h, t_h
# EOR     acc_l, acc_l, t_l
# EOR     acc_m, acc_m, t_m
#
# MODULO: take the partial accumulators (~representing sum of 256b
#         multiplication results), from GHASH and do modulo reduction on them
#         There is some complication here because the bit order of GHASH's
#         PMULL is reversed compared to elsewhere, so we are doing modulo with
#         a reversed constant
#
# EOR     acc_m, acc_m, acc_h
# EOR     acc_m, acc_m, acc_l                // Finish off karatsuba processing
# PMULL   t_mod, acc_h, mod_constant
# EXT     acc_h, acc_h, acc_h, #8
# EOR     acc_m, acc_m, acc_h
# EOR     acc_m, acc_m, t_mod
# PMULL   acc_h, acc_m, mod_constant
# EXT     acc_m, acc_m, acc_m, #8
# EOR     acc_l, acc_l, acc_h
# EOR     acc_l, acc_l, acc_m
#
# Key Optimizations:
#
# NOTE: This implementation is heavily NEON-bound due to the large amount of
# vector computation. The original code from which this was derived often
# avoided unnecessary Load/Store operations, but at the expense of
# increased NEON pipeline pressure and more GPR <-> NEON data movements.
# The primary goal of the optimizations applied here is to reduce NEON
# pipeline pressure. This includes minimizing NEON register-to-register moves,
# GPR to NEON transfers (and vice versa), and other operations that consume
# execution slots on the NEON pipes.
#
# 1.  Merged Kernels: AES-128, AES-192, and AES-256 GCM encryption and
#     decryption are handled by single kernels each, reducing code size.
#     Conditional branching is used for the key-size specific rounds.
#
# 2.  Aggressive Pipelining: GHASH and AES operations for different blocks
#     (4k to 4k+3) are heavily interleaved within the main loop. GHASH for
#     block `n` starts while AES for block `n` is still in progress, and
#     AES for block `n+4` begins. This hides instruction latencies.
#
# 3.  Optimized Counter Handling: Instead of incrementing, reversing, and
#     moving the counter to NEON registers for each of the four blocks in
#     every loop iteration, we precompute and cache counter values on the
#     stack.
#     - The lower 96 bits of the counter are constant across the four blocks
#       within an iteration.
#     - The upper 32 bits (the byte-swapped incrementing part) are calculated
#       for all four blocks (N, N+1, N+2, N+3) and stored on the stack at
#       the start of the loop.
#     - Values are calculated one loop iteration ahead of time. That is,
#       immediately after loading values N-N+3 we calculate and store values
#       N+4-N+7. This ensures that the data is available when loaded without
#       store-load forwarding.
#     - This strategy eliminates repeated scalar-to-vector transfers (FMOV)
#       and scalar increment/reverse operations inside the tight loop.
#
# 4.  Reduced Scalar-NEON Transfers:
#     - For encryption, the final XOR with the last round key (rkN) and the
#       AES-encrypted counter is done entirely in NEON registers using
#       EOR or PLATFORM_EOR3, avoiding costly FMOV instructions to move
#       plaintext data from GPRs to NEON after partial operations. A similar
#       optimization is used for decryption.
#
# 5.  EOR3 Optimization: The PLATFORM_EOR3 macro allows using the single
#     EOR3 instruction (if available) to replace two EOR instructions,
#     i.e., D = A ^ B ^ C. This is used in both GHASH accumulator updates
#     and the final AES output XOR. Both versions of encrypt and decrypt are
#     stored in the same assembly file.
#
# 6.  Instruction scheduling was significantly modified both to directly improve
#     performance as well as to remove false dependencies when that allowed
#     additional improvements. There is still a bit of additional speedup
#     possible but would be easiest to extract with platform-specific
#     implementations.
#
use strict;
use warnings;

my $flavour = shift;
my $output  = shift;

if (!defined $flavour) {
    die "Usage: $0 <flavour> [output_filename]\n";
}

$0 =~ m/(.*[\/\\])[^\/\\]+$/; my $dir=$1;
my $xlate;
( $xlate="${dir}arm-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../../perlasm/arm-xlate.pl" and -f $xlate) or
die "can't locate arm-xlate.pl";

open OUT, "|-", $^X, $xlate, $flavour, $output;
*STDOUT=*OUT;

# Converts an EOR3 pseudo-instruction into either the raw EOR3 encoding or into
# two EOR2 instructions.
sub process_eor3_match {
    my ($vd_str, $vn_str, $vm_str, $va_str, $is_eor3) = @_;

    if ($is_eor3) {
        # Directly emit the EOR3 mnemonic.
        # Requires: .arch armv8.2-a+sha3 OR -march=armv8.2-a+sha3
        return sprintf("    eor3    %s.16b, %s.16b, %s.16b, %s.16b",
                       $vd_str, $vn_str, $vm_str, $va_str);
    } else {
        # Fallback: Two standard NEON EOR instructions
        # Result = (Vn ^ Vm) ^ Va
        # 1. Vd = Vn ^ Vm
        # 2. Vd = Vd ^ Va
        return sprintf("    eor     %s.16b, %s.16b, %s.16b\n" .
                       "    eor     %s.16b, %s.16b, %s.16b",
                       $vd_str, $vn_str, $vm_str,   # Step 1
                       $vd_str, $vd_str, $va_str);  # Step 2
    }
}

# Generates the code string for a specific version (Standard or EOR3).
# Performs regex substitutions for EOR3 instructions and symbol renaming.
sub get_transformed_code {
    my ($is_eor3, $template) = @_;
    my $temp_code = $template;

    if ($is_eor3) {
        # Suffix function names
        $temp_code =~ s/\b(aes_gcm_enc_kernel)\b/${1}_eor3/g;
        $temp_code =~ s/\b(aes_gcm_dec_kernel)\b/${1}_eor3/g;
        # Suffix local labels
        $temp_code =~ s/(\.L[a-zA-Z0-9_]+)/$1_eor3/g;
    }

    $temp_code =~ s/PLATFORM_EOR3\s*\(\s*([vV0-9]+)\s*,\s*([vV0-9]+)\s*,\s*([vV0-9]+)\s*,\s*([vV0-9]+)\s*\)/
        process_eor3_match($1, $2, $3, $4, $is_eor3)/ge;

    return $temp_code;
}

{
{
    # We attempt to minimize the number of aliases for the same register but
    # unfortunately sometimes you have to specify a Q or D register.

    ## AArch64 General-Purpose Registers (GPRs)
    my $input_ptr          = "x0";     # IN: Pointer to input plaintext
    my $bit_length         = "x1";     # IN: Length of input in bits
    my $output_ptr         = "x2";     # IN: Pointer to output ciphertext
    my $current_tag        = "x3";     # IN: Pointer to 16-byte GCM tag (T)
    my $end_input_ptr      = "x4";     # Calculated end of all input
    my $main_end_input_ptr = "x5";     # Calculated end of the main 4-block loop
    my $Htable             = "x6";     # Pointer to GHASH key table (H)
    my $key_ptr            = "x7";     # Pointer to end of AES key schedule
    my $cc                 = "x8";     # Pointer to AES key schedule (context)

    # Re-purposed registers for tail/scalar handling
    my $input_l0           = "x6";
    my $input_h0           = "x7";

    my $T32                = "x10";    # Holds scalar 64-bit counter value
    my $W32                = "w10";    # Holds scalar 32-bit counter value
    my $W_T32              = "w12";    # Holds reversed scalar 32-bit counter
    my $rkN_l              = "x13";    # Holds low 64 bits of final round key
    my $rkN_h              = "x14";    # Holds high 64 bits of final round key
    my $len                = "x15";    # Byte length of input
    my $counter            = "x16";    # Holds pointer to the 16-byte counter block (CTR)
    my $rounds             = "x17";    # Number of AES rounds (scalar)
    my $rounds_w           = "w17";    # Number of AES rounds (scalar)
    my $tmp_gpr_w          = "w20";    # Scratch register (word)
    my $mod_constantx      = "x21";    # Holds GHASH modulus 0xc2... (scalar)

    ## NEON/SIMD Registers (v0-v31)
    my ($ctr0, $ctr1, $ctr2, $ctr3)     = map("v$_", (0..3));
    my ($ctr0q, $ctr1q, $ctr2q, $ctr3q) = map("q$_", (0..3));
    my $ctr0d                           = "d0";

    my ($res0, $res1, $res2, $res3)     = map("v$_", (4..7));
    my ($res0q, $res1q, $res2q, $res3q) = map("q$_", (4..7));
    my ($res0d, $res1d, $res2d, $res3d) = map("d$_", (4..7));

    my ($acc_h, $acc_m, $acc_l)         = map("v$_", (9..11));

    my ($h1, $h2, $h3, $h4)             = map("v$_", (12..15));
    my ($h1q, $h2q, $h3q, $h4q)         = map("q$_", (12..15));

    my ($h12k, $h34k)                   = map("v$_", (16..17)); # H[1]^H[2] and H[3]^H[4]

    my ($rk0, $rk1, $rk2, $rk3, $rk4, $rk5, $rk6, $rk7, $rk8) = map("v$_", (18..26));
    my ($rk0q, $rk1q, $rk2q, $rk3q, $rk4q, $rk5q, $rk6q, $rk7q, $rk8q) = map("q$_", (18..26));

    my $rk9_11_tmp         = "v27";    # Used for rk9 (if AES-192) and rk11 (if AES-256)
    my $rk10_12            = "v28";    # Used for rk10 (if AES-192) and rk12 (if AES-256)
    my $rk9_11_tmpq        = "q27";
    my $rk10_12q           = "q28";

    my $rkNm1              = "v31";    # Round N-1 key
    my $rkNm1q             = "q31";

    my $mod_constant       = "v8";
    my $mod_constantd      = "d8";

    my $final_block_dest   = "v18";
    my $final_block_destq  = "q18";

    my ($ghash_t0, $ghash_t1, $ghash_t2, $ghash_t3)     = map("v$_", (20..23));
    my ($ghash_t0d, $ghash_t1d, $ghash_t2d, $ghash_t3d) = map("d$_", (20..23));

    my $v_rkN              = "v30";
    my $v_rkNq             = "q30";

    # Locations on the stack
    my $mod_constant_sp_offset = 128;
    my $ctr0_sp_offset         = 160;
    my $ctr1_sp_offset         = 176;
    my $ctr2_sp_offset         = 192;
    my $ctr3_sp_offset         = 208;

    # Registers specific to dec_kernel
    my ($input_l1, $input_h1)  = map("x$_", (19..20));
    my $ctr32w                 = "w9";
    my ($ctr32x, $ctr96_b64x)  = map("x$_", (9..10));
    my $acc_md                 = "d10";

    # Decryption temporary NEON registers
    my $t0  = "v8";   my $t0d  = "d8";
    my $t1  = "v4";   my $t1d  = "d4";
    my $t2  = "v8";   my $t2d  = "d8";
    my $t3  = "v4";   my $t3d  = "d4";
    my $t4  = "v4";   my $t4d  = "d4";
    my $t5  = "v5";   my $t5d  = "d5";
    my $t6  = "v8";   my $t6d  = "d8";
    my $t7  = "v5";   my $t7d  = "d5";
    my $t8  = "v4";   my $t8d  = "d8";
    my $t9  = "v6";   my $t9d  = "d6";

    my ($ctr_t0, $ctr_t1, $ctr_t2, $ctr_t3) = map("v$_", (4..7));
    my $mod_t = "v7";

    # Decryption key registers
    my $rk2q1 = "v20.1q";
    my $rk3q1 = "v21.1q";
    my $rk4v  = "v22";
    my $rk4d  = "d22";

    my ($output_l1, $output_h1, $output_l2, $output_h2, $output_l3, $output_h3) = map("x$_", (19..24));
    my ($output_l0, $output_h0) = map("x$_", (6..7));

    # --- Assemble the entire template ---
    my $code_template = "";
    my $header_directives = <<'___';
#if __ARM_MAX_ARCH__ >= 8
.arch armv8.2-a+crypto+sha3
.text
___

    $code_template .= <<"___";
.global aes_gcm_enc_kernel
.type   aes_gcm_enc_kernel,%function
.align  4
aes_gcm_enc_kernel:
    AARCH64_SIGN_LINK_REGISTER
    stp    x29, x30, [sp, #-224]!
    mov    x29, sp
    ld1    { $ctr0.16b}, [x4]                                                 // Load initial counter block
    stp    x19, x20, [sp, #16]
    mov    $ctr1.16b, $ctr0.16b                                               // Initialize ctr1-3 from ctr0
    mov    $ctr2.16b, $ctr0.16b
    mov    $ctr3.16b, $ctr0.16b
    mov    $counter, x4                                                       // Pointer to counter block in memory
    mov    $cc, x5                                                            // Pointer to AES key schedule context
    stp    $mod_constantx, x22, [sp, #32]
    // [sp, #48] is unused but allocated to align the stack layout with aes_gcm_dec_kernel
    stp    d8, d9, [sp, #64]                                                  // Save Neon registers
    stp    d10, d11, [sp, #80]
    stp    d12, d13, [sp, #96]
    stp    d14, d15, [sp, #112]
    ldr    $rounds_w, [$cc, #240]                                             // Load number of AES rounds
    add    $key_ptr, $cc, $rounds, lsl #4                                     // Calculate pointer to the last round key
    ldp    $rkN_l, $rkN_h, [$key_ptr]                                         // load round N key (for final XOR)
    ldr    $rkNm1q, [$key_ptr, #-16]                                          // load round N-1 key
    add    $end_input_ptr, $input_ptr, $bit_length, lsr #3                    // Calculate end of input
    lsr    $main_end_input_ptr, $bit_length, #3                               // Total byte length
    mov    $len, $main_end_input_ptr
    ldr    $W_T32, [$counter, #12]                                            // Load counter's low 32 bits
    sub    $main_end_input_ptr, $main_end_input_ptr, #1                       // byte_len - 1
    ldr    $rk0q, [$cc, #0]                                                   // load rk0
    and    $main_end_input_ptr, $main_end_input_ptr, #0xffffffffffffffc0      // Align main loop end to a multiple of 64 bytes
    add    $main_end_input_ptr, $main_end_input_ptr, $input_ptr
    rev    $W_T32, $W_T32                                                     // Reverse for big-endian increment
    uxtw   $T32, $W_T32                                                       // Zero extend reversed w12 into x10 for final counter update
    // Pre-compute this value instead of using two instructions to reconstruct it every iteration
    mov    $mod_constantx, #0xc200000000000000                                // GHASH reduction constant
    str    $mod_constantx, [sp, #$mod_constant_sp_offset]
    // We maintain four copies of ctr values on the stack. Each loop iteration we
    // store the updated ctr value to the last four bytes (e.g., $ctr0_sp_offset + 12).
    // We then load the four values. This avoids a singificant number of
    // expensive GPR->NEON and NEON->NEON moves. To avoid LDST forwarding we
    // calculate and store the values one iteration ahead so they have time to
    // drain before we load them.
    str    $ctr0q,     [sp, #$ctr0_sp_offset]                                  // Store base counter for block 0-3
    str    $ctr0q,     [sp, #$ctr1_sp_offset]
    str    $ctr0q,     [sp, #$ctr2_sp_offset]
    str    $ctr0q,     [sp, #$ctr3_sp_offset]
    // Since we need the values right away don't go through the stack this first
    // time. Manually insert the incremented big-endian counter values.
    rev    $tmp_gpr_w, $W_T32
    mov    $ctr0.s[3], $tmp_gpr_w                                             // ctr0 + 0
    add    $tmp_gpr_w, $W_T32, #1
    rev    $tmp_gpr_w, $tmp_gpr_w
    mov    $ctr1.s[3], $tmp_gpr_w                                             // ctr0 + 1
    add    $tmp_gpr_w, $W_T32, #2
    rev    $tmp_gpr_w, $tmp_gpr_w
    mov    $ctr2.s[3], $tmp_gpr_w                                             // ctr0 + 2
    add    $tmp_gpr_w, $W_T32, #3
    rev    $tmp_gpr_w, $tmp_gpr_w
    mov    $ctr3.s[3], $tmp_gpr_w                                             // ctr0 + 3
    // Calculate the ctr values for the *next* (not current) group of four
    // blocks. Store the incremented parts to the stack.
    add    $tmp_gpr_w, $W_T32, #4
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr0_sp_offset + 12 ]}]                             // ctr0 + 4 for next iter
    add    $tmp_gpr_w, $W_T32, #5
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr1_sp_offset + 12 ]}]                             // ctr0 + 5 for next iter
    add    $tmp_gpr_w, $W_T32, #6
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr2_sp_offset + 12 ]}]                             // ctr0 + 6 for next iter
    add    $tmp_gpr_w, $W_T32, #7
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr3_sp_offset + 12 ]}]                             // ctr0 + 7 for next iter
    add    $W_T32, $W_T32, #8                                                 // Advance counter past these two sets
    // --- Start AES for first 4 blocks ---
    aese    $ctr0.16b, $rk0.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 0
    ldp    $rk1q, $rk2q, [$cc, #16]                                           // load rk1, rk2
    aese    $ctr1.16b, $rk0.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 0
    ldp    $rk3q, $rk4q, [$cc, #48]                                           // load rk3, rk4
    aese    $ctr2.16b, $rk0.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 0
    ldp    $rk5q, $rk6q, [$cc, #80]                                           // load rk5, rk6
    aese    $ctr3.16b, $rk0.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 0
    ldp    $h2q, $h3q, [$Htable, #32]                                         // load H2, H3 (GHASH keys)
    aese    $ctr0.16b, $rk1.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 1
    ldp    $rk7q, $rk8q, [$cc, #112]                                          // load rk7, rk8
    aese    $ctr1.16b, $rk1.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 1
    aese    $ctr2.16b, $rk1.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 1
    aese    $ctr3.16b, $rk1.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 1
    aese    $ctr0.16b, $rk2.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 2
    ext    $h3.16b, $h3.16b, $h3.16b, #8                                      // Byte swap H3 for GHASH
    aese    $ctr1.16b, $rk2.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 2
    ext    $h2.16b, $h2.16b, $h2.16b, #8                                      // Byte swap H2 for GHASH
    aese    $ctr2.16b, $rk2.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 2
    ldr    $h4q, [$Htable, #80]                                               // load H4
    aese    $ctr3.16b, $rk2.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 2
    ext    $h4.16b, $h4.16b, $h4.16b, #8                                      // Byte swap H4 for GHASH
    aese    $ctr0.16b, $rk3.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 3
    ld1    { $acc_l.16b}, [$current_tag]                                      // Load initial GHASH accumulator (T)
    aese    $ctr1.16b, $rk3.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 3
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8                             // Byte swap T for GHASH
    aese    $ctr2.16b, $rk3.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 3
    rev64    $acc_l.16b, $acc_l.16b                                           // Correct byte order within 64-bit lanes
    aese    $ctr3.16b, $rk3.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 3
    trn2    $h34k.2d,  $h3.2d,    $h4.2d                                      // Karatsuba key: H4_low | H3_low
    aese    $ctr0.16b, $rk4.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 4
    ldr    $h1q, [$Htable]                                                    // load H1
    aese    $ctr1.16b, $rk4.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 4
    ext    $h1.16b, $h1.16b, $h1.16b, #8                                      // Byte swap H1 for GHASH
    aese    $ctr2.16b, $rk4.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 4
    trn1    $acc_h.2d, $h3.2d,    $h4.2d                                      // Karatsuba key: H4_high | H3_high
    aese    $ctr3.16b, $rk4.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 4
    trn2    $h12k.2d,  $h1.2d,    $h2.2d                                      // Karatsuba key: H2_low | H1_low
    aese    $ctr0.16b, $rk5.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 5
    ldr    $v_rkNq, [$key_ptr]                                                // Preload round N key for final EOR
    aese    $ctr1.16b, $rk5.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 5
    aese    $ctr3.16b, $rk5.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 5
    aese    $ctr2.16b, $rk5.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 5
    aese    $ctr0.16b, $rk6.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 6
    aese    $ctr1.16b, $rk6.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 6
    aese    $ctr2.16b, $rk6.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 6
    aese    $ctr3.16b, $rk6.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 6
    aese    $ctr0.16b, $rk7.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 7
    aese    $ctr1.16b, $rk7.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 7
    aese    $ctr2.16b, $rk7.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 7
    aese    $ctr3.16b, $rk7.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 7
    aese    $ctr0.16b, $rk8.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 8
    aese    $ctr1.16b, $rk8.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 8
    aese    $ctr2.16b, $rk8.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 8
    aese    $ctr3.16b, $rk8.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 8
    cmp    $rounds, #12                                                       // setup flags for AES-128/192/256 check
    b.lt    .Lenc_finish_first_blocks                                         // branch if AES-128
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #144]                               // load rk9, rk10
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 9
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 9
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 9
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 9
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 10
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 10
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 10
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 10
    b.eq    .Lenc_finish_first_blocks                                         // branch if AES-192
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #176]                               // load rk11, rk12
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 11
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 11
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 11
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 11
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 12
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 12
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 12
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 12
.Lenc_finish_first_blocks:
    cmp    $input_ptr, $main_end_input_ptr                                    // check if we have <= 4 blocks to process in the tail
    eor    $h34k.16b, $h34k.16b, $acc_h.16b                                   // Karatsuba key: H3^H4
    aese    $ctr0.16b, $rkNm1.16b                                             // AES block 0 - round N-1
    aese    $ctr1.16b, $rkNm1.16b                                             // AES block 1 - round N-1
    aese    $ctr2.16b, $rkNm1.16b                                             // AES block 2 - round N-1
    aese    $ctr3.16b, $rkNm1.16b                                             // AES block 3 - round N-1
    trn1    $mod_constant.2d,  $h1.2d, $h2.2d                                 // Karatsuba key: H2_high | H1_high
    eor    $h12k.16b, $h12k.16b, $mod_constant.16b                            // Karatsuba key: H1^H2
    b.ge    .Lenc_tail                                                        // handle tail if no more full 4-block sets
    ldp    $res2q, $res3q, [$input_ptr, #32]                                  // AES blocks 2,3 load plaintext
    ldp    $res0q, $res1q, [$input_ptr], #64                                  // AES blocks 0,1 load plaintext
    // Compute and store first 4 ciphertext blocks
    PLATFORM_EOR3($res0, $res0, $v_rkN, $ctr0)                                // AES block 0 - result = PT ^ AES(ctr0)
    PLATFORM_EOR3($res1, $res1, $v_rkN, $ctr1)                                // AES block 1 - result = PT ^ AES(ctr1)
    PLATFORM_EOR3($res2, $res2, $v_rkN, $ctr2)                                // AES block 2 - result = PT ^ AES(ctr2)
    PLATFORM_EOR3($res3, $res3, $v_rkN, $ctr3)                                // AES block 3 - result = PT ^ AES(ctr3)
    st1    { $res0.16b, $res1.16b, $res2.16b, $res3.16b}, [$output_ptr], #64  // AES blocks 0-3 - store result
    // Load counter values for the second iteration from the stack
    ldp    $ctr0q, $ctr1q, [sp, #$ctr0_sp_offset]
    ldp    $ctr2q, $ctr3q, [sp, #$ctr2_sp_offset]
    // Prepare and store counter values for the third iteration
    rev    $tmp_gpr_w, $W_T32
    str    $tmp_gpr_w, [sp, #@{[ $ctr0_sp_offset + 12 ]}]                             // ctr + 8
    add    $tmp_gpr_w, $W_T32, #1
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr1_sp_offset + 12 ]}]                             // ctr + 9
    add    $tmp_gpr_w, $W_T32, #2
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr2_sp_offset + 12 ]}]                             // ctr + 10
    add    $tmp_gpr_w, $W_T32, #3
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr3_sp_offset + 12 ]}]                             // ctr + 11
    add    $W_T32, $W_T32, #4                                                 // Advance counter base
    cmp    $input_ptr, $main_end_input_ptr                                    // check if we have <= 4 blocks remaining
    b.ge    .Lenc_prepretail                                                  // go to prepretail if < 2 full loops left
.Lenc_main_loop:    //    main loop start (processes 4 blocks per iteration)
    // --- AES Pipeline for blocks 4k+4 to 4k+7 ---
    aese    $ctr0.16b, $rk0.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 0
    aese    $ctr1.16b, $rk0.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 0
    aese    $ctr2.16b, $rk0.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 0
    aese    $ctr3.16b, $rk0.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 0
    ldr    $mod_constantd, [sp, #$mod_constant_sp_offset]                      // Load GHASH reduction constant
    aese    $ctr0.16b, $rk1.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 1
    aese    $ctr1.16b, $rk1.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 1
    aese    $ctr2.16b, $rk1.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 1
    aese    $ctr3.16b, $rk1.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 1
    // --- GHASH Pipeline (interleaved with AES) for blocks 4k to 4k+3 ---
    rev64    $res0.16b, $res0.16b                                             // GHASH block 4k - Byte swap CT
    rev64    $res1.16b, $res1.16b                                             // GHASH block 4k+1 - Byte swap CT
    rev64    $res2.16b, $res2.16b                                             // GHASH block 4k+2 - Byte swap CT
    rev64    $res3.16b, $res3.16b                                             // GHASH block 4k+3 - Byte swap CT
    aese    $ctr0.16b, $rk2.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 2
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8                             // GHASH - prepare acc for XOR
    aese    $ctr1.16b, $rk2.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 2
    eor    $res0.16b, $res0.16b, $acc_l.16b                                   // GHASH block 4k - Y_i = CT_i ^ Y_{i-1}
    aese    $ctr2.16b, $rk2.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 2
    pmull    $acc_l.1q, $res0.1d, $h4.1d                                      // GHASH block 4k - low
    aese    $ctr3.16b, $rk2.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 2
    pmull2    $acc_h.1q, $res2.2d, $h2.2d                                     // GHASH block 4k+2 - high
    aese    $ctr0.16b, $rk3.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 3
    mov    $acc_md, $h34k.d[1]                                                // GHASH block 4k - mid Karatsuba key
    aese    $ctr1.16b, $rk3.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 3
    mov    $ghash_t0d, $res0.d[1]                                             // GHASH block 4k - mid
    aese    $ctr2.16b, $rk3.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 3
    eor    $ghash_t0.8b, $ghash_t0.8b, $res0.8b                               // GHASH block 4k - mid
    aese    $ctr3.16b, $rk3.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 3
    mov    $ghash_t1d, $res1.d[1]                                             // GHASH block 4k+1 - mid
    aese    $ctr0.16b, $rk4.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 4
    eor    $ghash_t1.8b, $ghash_t1.8b, $res1.8b                               // GHASH block 4k+1 - mid
    aese    $ctr1.16b, $rk4.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 4
    pmull    $acc_m.1q, $ghash_t0.1d, $acc_m.1d                               // GHASH block 4k - mid
    aese    $ctr2.16b, $rk4.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 4
    pmull    $ghash_t1.1q, $ghash_t1.1d, $h34k.1d                             // GHASH block 4k+1 - mid
    aese    $ctr3.16b, $rk4.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 4
    eor    $acc_m.16b, $acc_m.16b, $ghash_t1.16b                              // GHASH block 4k+1 - mid
    aese    $ctr0.16b, $rk5.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 5
    ext    $ghash_t2.16b, $ghash_t2.16b, $res2.16b, #8                        // GHASH block 4k+2 - mid
    aese    $ctr1.16b, $rk5.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 5
    eor    $ghash_t2.16b, $ghash_t2.16b, $res2.16b                            // GHASH block 4k+2 - mid
    aese    $ctr2.16b, $rk5.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 5
    pmull2    $ghash_t2.1q, $ghash_t2.2d, $h12k.2d                            // GHASH block 4k+2 - mid
    aese    $ctr3.16b, $rk5.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 5
    mov    $ghash_t3d, $res3.d[1]                                             // GHASH block 4k+3 - mid
    aese    $ctr0.16b, $rk6.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 6
    eor    $ghash_t3.8b, $ghash_t3.8b, $res3.8b                               // GHASH block 4k+3 - mid
    aese    $ctr1.16b, $rk6.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 6
    pmull    $ghash_t3.1q, $ghash_t3.1d, $h12k.1d                             // GHASH block 4k+3 - mid
    aese    $ctr2.16b, $rk6.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 6
    PLATFORM_EOR3($acc_m, $acc_m, $ghash_t2, $ghash_t3)                       // GHASH block 4k+2/3 - mid
    aese    $ctr3.16b, $rk6.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 6
    pmull2    $ghash_t2.1q, $res0.2d, $h4.2d                                  // GHASH block 4k - high
    aese    $ctr0.16b, $rk7.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 7
    pmull2    $ghash_t1.1q, $res3.2d, $h1.2d                                  // GHASH block 4k+3 - high
    eor    $ghash_t2.16b, $ghash_t2.16b, $ghash_t1.16b                        // GHASH block 4k+3 - high
    aese    $ctr1.16b, $rk7.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 7
    pmull2    $ghash_t3.1q, $res1.2d, $h3.2d                                  // GHASH block 4k+1 - high
    aese    $ctr2.16b, $rk7.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 7
    pmull    $ghash_t1.1q, $res2.1d, $h2.1d                                   // GHASH block 4k+2 - low
    aese    $ctr3.16b, $rk7.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 7
    pmull    $ghash_t0.1q, $res1.1d, $h3.1d                                   // GHASH block 4k+1 - low
    PLATFORM_EOR3($acc_h, $acc_h, $ghash_t2, $ghash_t3)                       // GHASH block 4k/1/2/3 - high
    pmull    $ghash_t2.1q, $res3.1d, $h1.1d                                   // GHASH block 4k+3 - low
    ldp    $res2q, $res3q, [$input_ptr, #32]
    ldp    $res0q, $res1q, [$input_ptr], #64
    eor    $ghash_t0.16b, $ghash_t0.16b, $ghash_t1.16b                        // GHASH block 4k+1 - low
    aese    $ctr0.16b, $rk8.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 4k+4 - round 8
    PLATFORM_EOR3($acc_l, $acc_l, $ghash_t2, $ghash_t0)                       // GHASH block 4k/1/2/3 - low
    aese    $ctr1.16b, $rk8.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 4k+5 - round 8
    PLATFORM_EOR3($acc_m, $acc_m, $acc_h, $acc_l)                             // MODULO - karatsuba tidy up
    aese    $ctr2.16b, $rk8.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 4k+6 - round 8
    pmull    $ghash_t0.1q, $acc_h.1d, $mod_constant.1d                        // MODULO - top 64b align with mid
    aese    $ctr3.16b, $rk8.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 4k+7 - round 8
    cmp    $rounds, #12                                                       // setup flags for AES-128/192/256 check
    b.lt    .Lenc_main_loop_continue                                          // branch if AES-128
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #144]                               // load rk9, rk10
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 4k+4 - round 9
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 4k+5 - round 9
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 4k+6 - round 9
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 4k+7 - round 9
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 4k+4 - round 10
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 4k+5 - round 10
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 4k+6 - round 10
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 4k+7 - round 10
    b.eq    .Lenc_main_loop_continue                                          // branch if AES-192
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #176]                               // load rk11, rk12
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 4k+4 - round 11
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 4k+5 - round 11
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 4k+6 - round 11
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 4k+7 - round 11
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 4k+4 - round 12
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 4k+5 - round 12
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 4k+6 - round 12
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 4k+7 - round 12
.Lenc_main_loop_continue:
    ext    $acc_h.16b, $acc_h.16b, $acc_h.16b, #8                             // MODULO - other top alignment
    PLATFORM_EOR3($acc_m, $acc_m, $ghash_t0, $acc_h)                          // MODULO - fold into mid
    pmull    $acc_h.1q, $acc_m.1d, $mod_constant.1d                           // MODULO - mid 64b align with low
    ext    $ghash_t0.16b, $acc_m.16b, $acc_m.16b, #8                          // MODULO - other mid alignment
    PLATFORM_EOR3($acc_l, $acc_h, $acc_l, $ghash_t0)                          // MODULO - fold into low
    aese    $ctr0.16b, $rkNm1.16b                                             // AES block 4k+4 - round N-1
    PLATFORM_EOR3($res0, $res0, $v_rkN, $ctr0)                                // AES block 4k+4 - result
    aese    $ctr1.16b, $rkNm1.16b                                             // AES block 4k+5 - round N-1
    PLATFORM_EOR3($res1, $res1, $v_rkN, $ctr1)                                // AES block 4k+5 - result
    aese    $ctr2.16b, $rkNm1.16b                                             // AES block 4k+6 - round N-1
    PLATFORM_EOR3($res2, $res2, $v_rkN, $ctr2)                                // AES block 4k+6 - result
    aese    $ctr3.16b, $rkNm1.16b                                             // AES block 4k+7 - round N-1
    PLATFORM_EOR3($res3, $res3, $v_rkN, $ctr3)                                // AES block 4k+7 - result
    ldp    $ctr0q, $ctr1q, [sp, #$ctr0_sp_offset]
    ldp    $ctr2q, $ctr3q, [sp, #$ctr2_sp_offset]
    // We used these registers as temporaries above so reload the RKs.
    ldp    $rk2q, $rk3q, [$cc, #32]                                           // load rk2, rk3
    ldp    $rk4q, $rk5q, [$cc, #64]                                           // load rk4, rk5
    st1    { $res0.16b, $res1.16b, $res2.16b, $res3.16b}, [$output_ptr], #64  // AES blocks 4k+4-7 - store result
    rev    $tmp_gpr_w, $W_T32
    str    $tmp_gpr_w, [sp, #@{[ $ctr0_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $W_T32, #1
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr1_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $W_T32, #2
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr2_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $W_T32, #3
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr3_sp_offset + 12 ]}]
    add    $W_T32, $W_T32, #4
    cmp    $input_ptr, $main_end_input_ptr                                    // .LOOP CONTROL
    b.lt    .Lenc_main_loop
.Lenc_prepretail:                                                             //    PREPRETAIL
    aese    $ctr1.16b, $rk0.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 0
    rev64    $res2.16b, $res2.16b                                             // GHASH block 2
    aese    $ctr2.16b, $rk0.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 0
    aese    $ctr0.16b, $rk0.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 0
    rev64    $res0.16b, $res0.16b                                             // GHASH block 0
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8                             // PRE 0
    aese    $ctr2.16b, $rk1.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 1
    aese    $ctr0.16b, $rk1.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 1
    eor    $res0.16b, $res0.16b, $acc_l.16b                                   // PRE 1
    rev64    $res1.16b, $res1.16b                                             // GHASH block 1
    aese    $ctr2.16b, $rk2.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 2
    aese    $ctr3.16b, $rk0.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 0
    mov    $acc_md, $h34k.d[1]                                                // GHASH block 0 - mid Karatsuba key
    aese    $ctr1.16b, $rk1.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 1
    pmull    $acc_l.1q, $res0.1d, $h4.1d                                      // GHASH block 0 - low
    mov    $mod_constantd, $res0.d[1]                                         // GHASH block 0 - mid
    pmull2    $acc_h.1q, $res0.2d, $h4.2d                                     // GHASH block 0 - high
    aese    $ctr2.16b, $rk3.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 3
    aese    $ctr1.16b, $rk2.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 2
    eor    $mod_constant.8b, $mod_constant.8b, $res0.8b                       // GHASH block 0 - mid
    aese    $ctr0.16b, $rk2.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 2
    aese    $ctr3.16b, $rk1.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 1
    aese    $ctr1.16b, $rk3.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 3
    pmull    $acc_m.1q, $mod_constant.1d, $acc_m.1d                           // GHASH block 0 - mid
    pmull2    $res0.1q, $res1.2d, $h3.2d                                      // GHASH block 1 - high
    pmull    $mod_constant.1q, $res1.1d, $h3.1d                               // GHASH block 1 - low
    aese    $ctr3.16b, $rk2.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 2
    eor    $acc_h.16b, $acc_h.16b, $res0.16b                                  // GHASH block 1 - high
    mov    $res0d, $res1.d[1]                                                 // GHASH block 1 - mid
    aese    $ctr0.16b, $rk3.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 3
    eor    $acc_l.16b, $acc_l.16b, $mod_constant.16b                          // GHASH block 1 - low
    aese    $ctr3.16b, $rk3.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 3
    eor    $res0.8b, $res0.8b, $res1.8b                                       // GHASH block 1 - mid
    mov    $mod_constantd, $res2.d[1]                                         // GHASH block 2 - mid
    aese    $ctr0.16b, $rk4.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 4
    rev64    $res3.16b, $res3.16b                                             // GHASH block 3
    aese    $ctr3.16b, $rk4.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 4
    pmull    $res0.1q, $res0.1d, $h34k.1d                                     // GHASH block 1 - mid
    eor    $mod_constant.8b, $mod_constant.8b, $res2.8b                       // GHASH block 2 - mid
    pmull    $res1.1q, $res2.1d, $h2.1d                                       // GHASH block 2 - low
    aese    $ctr3.16b, $rk5.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 5
    aese    $ctr2.16b, $rk4.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 4
    eor    $acc_m.16b, $acc_m.16b, $res0.16b                                  // GHASH block 1 - mid
    pmull2    $res0.1q, $res2.2d, $h2.2d                                      // GHASH block 2 - high
    eor    $acc_l.16b, $acc_l.16b, $res1.16b                                  // GHASH block 2 - low
    ins    $mod_constant.d[1], $mod_constant.d[0]                             // GHASH block 2 - mid
    aese    $ctr2.16b, $rk5.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 5
    eor    $acc_h.16b, $acc_h.16b, $res0.16b                                  // GHASH block 2 - high
    mov    $res0d, $res3.d[1]                                                 // GHASH block 3 - mid
    aese    $ctr1.16b, $rk4.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 4
    pmull2    $mod_constant.1q, $mod_constant.2d, $h12k.2d                    // GHASH block 2 - mid
    eor    $res0.8b, $res0.8b, $res3.8b                                       // GHASH block 3 - mid
    pmull2    $res1.1q, $res3.2d, $h1.2d                                      // GHASH block 3 - high
    aese    $ctr1.16b, $rk5.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 5
    pmull    $res0.1q, $res0.1d, $h12k.1d                                     // GHASH block 3 - mid
    eor    $acc_m.16b, $acc_m.16b, $mod_constant.16b                          // GHASH block 2 - mid
    aese    $ctr0.16b, $rk5.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 5
    aese    $ctr1.16b, $rk6.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 6
    aese    $ctr2.16b, $rk6.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 6
    aese    $ctr0.16b, $rk6.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 6
    aese    $ctr3.16b, $rk6.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 6
    aese    $ctr1.16b, $rk7.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 7
    eor    $acc_h.16b, $acc_h.16b, $res1.16b                                  // GHASH block 3 - high
    aese    $ctr0.16b, $rk7.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 7
    aese    $ctr3.16b, $rk7.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 7
    ldr    $mod_constantd, [sp, #$mod_constant_sp_offset]
    aese    $ctr1.16b, $rk8.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 8
    eor    $acc_m.16b, $acc_m.16b, $res0.16b                                  // GHASH block 3 - mid
    pmull    $res2.1q, $res3.1d, $h1.1d                                       // GHASH block 3 - low
    aese    $ctr3.16b, $rk8.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 8
    cmp    $rounds, #12                                                       // setup flags for AES-128/192/256 check
    aese    $ctr0.16b, $rk8.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 8
    eor    $acc_l.16b, $acc_l.16b, $res2.16b                                  // GHASH block 3 - low
    aese    $ctr2.16b, $rk7.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 7
    eor    $acc_m.16b, $acc_m.16b, $acc_h.16b                                 // karatsuba tidy up
    aese    $ctr2.16b, $rk8.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 8
    pmull    $res0.1q, $acc_h.1d, $mod_constant.1d
    ext    $acc_h.16b, $acc_h.16b, $acc_h.16b, #8
    eor    $acc_m.16b, $acc_m.16b, $acc_l.16b
    b.lt    .Lenc_finish_prepretail                                           // branch if AES-128
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #144]                               // load rk9, rk10
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 9
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 9
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 9
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 9
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 10
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 10
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 10
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 10
    b.eq    .Lenc_finish_prepretail                                           // branch if AES-192
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #176]                               // load rk11, rk12
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 11
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 11
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 11
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 11
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 12
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 12
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 12
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 12
.Lenc_finish_prepretail:
    aese    $ctr0.16b, $rkNm1.16b                                             // AES block 0 - round N-1
    aese    $ctr1.16b, $rkNm1.16b                                             // AES block 1 - round N-1
    aese    $ctr2.16b, $rkNm1.16b                                             // AES block 2 - round N-1
    aese    $ctr3.16b, $rkNm1.16b                                             // AES block 3 - round N-1
    PLATFORM_EOR3($acc_m, $acc_m, $res0, $acc_h)
    pmull    $res0.1q, $acc_m.1d, $mod_constant.1d
    ext    $acc_m.16b, $acc_m.16b, $acc_m.16b, #8
    PLATFORM_EOR3($acc_l, $acc_l, $res0, $acc_m)
.Lenc_tail:                                                                   // TAIL: Process remaining 0 to 3 blocks
    ext    $mod_constant.16b, $acc_l.16b, $acc_l.16b, #8                      // Save current GHASH state for partial tag feed-in
    sub    $main_end_input_ptr, $end_input_ptr, $input_ptr                    // main_end_input_ptr is number of bytes left to process
    ldp    $input_l0, $input_h0, [$input_ptr], #16                            // AES block 0 - load plaintext
    eor    $input_l0, $input_l0, $rkN_l                                       // AES block 0 - round N low
    eor    $input_h0, $input_h0, $rkN_h                                       // AES block 0 - round N high
    cmp    $main_end_input_ptr, #48
    fmov    $res0d, $input_l0                                                 // AES block 0 - mov low
    fmov    $res0.d[1], $input_h0                                             // AES block 0 - mov high
    eor    $res1.16b, $res0.16b, $ctr0.16b                                    // AES block 0 - result
    b.gt    .Lenc_blocks_more_than_3
    cmp    $main_end_input_ptr, #32
    mov    $ctr3.16b, $ctr2.16b
    movi    $acc_l.8b, #0
    movi    $acc_h.8b, #0
    mov    $ctr2.16b, $ctr1.16b
    movi    $acc_m.8b, #0
    b.gt    .Lenc_blocks_more_than_2
    mov    $ctr3.16b, $ctr1.16b
    cmp    $main_end_input_ptr, #16
    b.gt    .Lenc_blocks_more_than_1
    b    .Lenc_blocks_less_than_1
.Lenc_blocks_more_than_3:                                                     // blocks left >  3
    st1    { $res1.16b}, [$output_ptr], #16                                   // AES final-2 block - store result
    ldp    $input_l0, $input_h0, [$input_ptr], #16                            // AES final-2 block - load input low & high
    rev64    $res0.16b, $res1.16b                                             // GHASH final-3 block
    eor    $input_l0, $input_l0, $rkN_l                                       // AES final-2 block - round N low
    eor    $res0.16b, $res0.16b, $mod_constant.16b                            // feed in partial tag
    eor    $input_h0, $input_h0, $rkN_h                                       // AES final-2 block - round N high
    mov    $ghash_t2d, $res0.d[1]                                             // GHASH final-3 block - mid
    fmov    $res1d, $input_l0                                                 // AES final-2 block - mov low
    fmov    $res1.d[1], $input_h0                                             // AES final-2 block - mov high
    eor    $ghash_t2.8b, $ghash_t2.8b, $res0.8b                               // GHASH final-3 block - mid
    movi    $mod_constant.8b, #0                                              // suppress further partial tag feed in
    mov    $acc_md, $h34k.d[1]                                                // GHASH final-3 block - mid
    pmull    $acc_l.1q, $res0.1d, $h4.1d                                      // GHASH final-3 block - low
    pmull2    $acc_h.1q, $res0.2d, $h4.2d                                     // GHASH final-3 block - high
    pmull    $acc_m.1q, $ghash_t2.1d, $acc_m.1d                               // GHASH final-3 block - mid
    eor    $res1.16b, $res1.16b, $ctr1.16b                                    // AES final-2 block - result
.Lenc_blocks_more_than_2:                                                     // blocks left >  2
    st1    { $res1.16b}, [$output_ptr], #16                                   // AES final-2 block - store result
    ldp    $input_l0, $input_h0, [$input_ptr], #16                            // AES final-1 block - load input low & high
    rev64    $res0.16b, $res1.16b                                             // GHASH final-2 block
    eor    $input_l0, $input_l0, $rkN_l                                       // AES final-1 block - round N low
    eor    $res0.16b, $res0.16b, $mod_constant.16b                            // feed in partial tag
    fmov    $res1d, $input_l0                                                 // AES final-1 block - mov low
    eor    $input_h0, $input_h0, $rkN_h                                       // AES final-1 block - round N high
    fmov    $res1.d[1], $input_h0                                             // AES final-1 block - mov high
    movi    $mod_constant.8b, #0                                              // suppress further partial tag feed in
    pmull2    $ghash_t0.1q, $res0.2d, $h3.2d                                  // GHASH final-2 block - high
    mov    $ghash_t2d, $res0.d[1]                                             // GHASH final-2 block - mid
    pmull    $ghash_t1.1q, $res0.1d, $h3.1d                                   // GHASH final-2 block - low
    eor    $ghash_t2.8b, $ghash_t2.8b, $res0.8b                               // GHASH final-2 block - mid
    eor    $res1.16b, $res1.16b, $ctr2.16b                                    // AES final-1 block - result
    eor    $acc_h.16b, $acc_h.16b, $ghash_t0.16b                              // GHASH final-2 block - high
    pmull    $ghash_t2.1q, $ghash_t2.1d, $h34k.1d                             // GHASH final-2 block - mid
    eor    $acc_l.16b, $acc_l.16b, $ghash_t1.16b                              // GHASH final-2 block - low
    eor    $acc_m.16b, $acc_m.16b, $ghash_t2.16b                              // GHASH final-2 block - mid
.Lenc_blocks_more_than_1:                                                     // blocks left >  1
    st1    { $res1.16b}, [$output_ptr], #16                                   // AES final-1 block - store result
    rev64    $res0.16b, $res1.16b                                             // GHASH final-1 block: Byte Swap CT
    ldp    $input_l0, $input_h0, [$input_ptr], #16                            // AES final block - load plaintext
    eor    $res0.16b, $res0.16b, $mod_constant.16b                            // Feed in partial tag
    movi    $mod_constant.8b, #0                                              // Clear for next block
    eor    $input_l0, $input_l0, $rkN_l                                       // AES final block - round N low
    mov    $ghash_t2d, $res0.d[1]                                             // GHASH final-1 block - mid
    pmull2    $ghash_t0.1q, $res0.2d, $h2.2d                                  // GHASH final-1 block - high
    eor    $input_h0, $input_h0, $rkN_h                                       // AES final block - round N high
    eor    $ghash_t2.8b, $ghash_t2.8b, $res0.8b                               // GHASH final-1 block - mid
    eor    $acc_h.16b, $acc_h.16b, $ghash_t0.16b                              // GHASH final-1 block - high
    ins    $ghash_t2.d[1], $ghash_t2.d[0]                                     // GHASH final-1 block - mid
    fmov    $res1d, $input_l0                                                 // AES final block - mov low
    fmov    $res1.d[1], $input_h0                                             // AES final block - mov high
    pmull2    $ghash_t2.1q, $ghash_t2.2d, $h12k.2d                            // GHASH final-1 block - mid
    pmull    $ghash_t1.1q, $res0.1d, $h2.1d                                   // GHASH final-1 block - low
    eor    $res1.16b, $res1.16b, $ctr3.16b                                    // AES final block - result
    eor    $acc_m.16b, $acc_m.16b, $ghash_t2.16b                              // GHASH final-1 block - mid
    eor    $acc_l.16b, $acc_l.16b, $ghash_t1.16b                              // GHASH final-1 block - low
.Lenc_blocks_less_than_1:                                                     // Last partial block handling
    add    $T32, $T32, $bit_length, lsr #7                                    // Calculate the updated counter based on the number of 16B chunks we processed
    rev    $W32, $W32
    str    $W32, [$counter, #12]                                              // store the updated counter
    and    $bit_length, $bit_length, #127                                     // bit_length %= 128
    mvn    $rkN_l, xzr                                                        // Mask for low 64 bits
    sub    $bit_length, $bit_length, #128                                     //
    neg    $bit_length, $bit_length                                           // Valid bits in the last block (1-128)
    ldr    $final_block_destq, [$output_ptr]                                  // Load destination for merging
    mvn    $rkN_h, xzr                                                        // Mask for high 64 bits
    and    $bit_length, $bit_length, #127                                     // bit_length %= 128
    lsr    $rkN_h, $rkN_h, $bit_length                                        // rkN_h is mask for top 64b of last block
    cmp    $bit_length, #64
    csel    $input_l0, $rkN_l, $rkN_h, lt
    csel    $input_h0, $rkN_h, xzr, lt
    fmov    $ctr0d, $input_l0                                                 // ctr0d is mask for last block
    fmov    $ctr0.d[1], $input_h0
    and    $res1.16b, $res1.16b, $ctr0.16b                                    // Mask out unused bits of the last CT block
    rev64    $res0.16b, $res1.16b                                             // GHASH final block - byte swap
    eor    $res0.16b, $res0.16b, $mod_constant.16b                            // Feed in partial tag
    bif    $res1.16b, $final_block_dest.16b, $ctr0.16b                        // Bitwise Insert: merge with existing data at output_ptr
    pmull2    $ghash_t0.1q, $res0.2d, $h1.2d                                  // GHASH final block - high
    mov    $mod_constantd, $res0.d[1]                                         // GHASH final block - mid
    pmull    $ghash_t1.1q, $res0.1d, $h1.1d                                   // GHASH final block - low
    eor    $acc_h.16b, $acc_h.16b, $ghash_t0.16b                              // GHASH final block - high
    eor    $mod_constant.8b, $mod_constant.8b, $res0.8b                       // GHASH final block - mid
    pmull    $mod_constant.1q, $mod_constant.1d, $h12k.1d                     // GHASH final block - mid
    eor    $acc_l.16b, $acc_l.16b, $ghash_t1.16b                              // GHASH final block - low
    eor    $acc_m.16b, $acc_m.16b, $mod_constant.16b                          // GHASH final block - mid
    eor    $res0.16b, $acc_l.16b, $acc_h.16b                                  // MODULO - karatsuba tidy up
    fmov    $mod_constantd, $mod_constantx
    eor    $acc_m.16b, $acc_m.16b, $res0.16b                                  // MODULO - karatsuba tidy up
    pmull    $res3.1q, $acc_h.1d, $mod_constant.1d                            // MODULO - top 64b align with mid
    ext    $acc_h.16b, $acc_h.16b, $acc_h.16b, #8                             // MODULO - other top alignment
    PLATFORM_EOR3($acc_m, $acc_m, $res3, $acc_h)                              // MODULO - fold into mid
    pmull    $acc_h.1q, $acc_m.1d, $mod_constant.1d                           // MODULO - mid 64b align with low
    ext    $acc_m.16b, $acc_m.16b, $acc_m.16b, #8                             // MODULO - other mid alignment
    st1    { $res1.16b}, [$output_ptr]                                        // store all 16B
    PLATFORM_EOR3($acc_l, $acc_l, $acc_h, $acc_m)                             // MODULO - fold into low
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8                             // Byte swap GHASH result
    rev64    $acc_l.16b, $acc_l.16b                                           // Final Tag
    mov    $input_ptr, $len
    st1    { $acc_l.16b }, [$current_tag]                                     // Store final tag
    ldp    x19, x20, [sp, #16]
    ldp    $mod_constantx, x22, [sp, #32]
    ldp    d8, d9, [sp, #64]
    ldp    d10, d11, [sp, #80]
    ldp    d12, d13, [sp, #96]
    ldp    d14, d15, [sp, #112]
    ldp    x29, x30, [sp], #224
    AARCH64_VALIDATE_LINK_REGISTER
    ret
.size aes_gcm_enc_kernel,.-aes_gcm_enc_kernel
___
    ################################################################################
    # aes_gcm_dec_kernel
    ################################################################################
    $code_template .= <<"___";
.global aes_gcm_dec_kernel
.type   aes_gcm_dec_kernel,%function
.align  4
aes_gcm_dec_kernel:
    AARCH64_SIGN_LINK_REGISTER
    stp    x29, x30, [sp, #-224]!
    mov    x29, sp
    stp    x19, x20, [sp, #16]
    ld1    { $ctr0.16b}, [x4]
    mov    $ctr1.16b, $ctr0.16b
    mov    $ctr2.16b, $ctr0.16b
    mov    $ctr3.16b, $ctr0.16b
    mov    $counter, x4
    mov    $cc, x5
    stp    $mod_constantx, x22, [sp, #32]
    stp    x23, x24, [sp, #48]
    stp    d8, d9, [sp, #64]
    stp    d10, d11, [sp, #80]
    stp    d12, d13, [sp, #96]
    stp    d14, d15, [sp, #112]
    ldr    $rounds_w, [$cc, #240]                                             // Load number of AES rounds
    add    $input_l1, $cc, $rounds, lsl #4                                    // borrow input_l1 for last key
    ldp    $rkN_l, $rkN_h, [$input_l1]                                        // load round N keys
    ldr    $rkNm1q, [$input_l1, #-16]                                         // load round N-1 keys
    add    $end_input_ptr, $input_ptr, $bit_length, lsr #3                    // end_input_ptr
    lsr    $main_end_input_ptr, $bit_length, #3                               // byte_len
    mov    $len, $main_end_input_ptr
    ldr    $ctr32w, [$counter, #12]                                           // Load scalar 32-bit counter (CTR)
    sub    $main_end_input_ptr, $main_end_input_ptr, #1                       // byte_len - 1
    ldr    $rk0q, [$cc, #0]                                                   // load rk0
    and    $main_end_input_ptr, $main_end_input_ptr, #0xffffffffffffffc0      // number of bytes to be processed in main loop (at least 1 byte must be handled by tail)
    add    $main_end_input_ptr, $main_end_input_ptr, $input_ptr
    rev    $ctr32w, $ctr32w                                                   // Reverse it once for big-endian incrementing
    uxtw   $T32, $ctr32w                                                      // Zero extend reversed w9 into x10
    str    $ctr0q,     [sp, #$ctr0_sp_offset]
    str    $ctr0q,     [sp, #$ctr1_sp_offset]
    str    $ctr0q,     [sp, #$ctr2_sp_offset]
    str    $ctr0q,     [sp, #$ctr3_sp_offset]
    rev    $tmp_gpr_w, $ctr32w
    mov    $ctr0.s[3], $tmp_gpr_w
    add    $tmp_gpr_w, $ctr32w, #1
    rev    $tmp_gpr_w, $tmp_gpr_w
    mov    $ctr1.s[3], $tmp_gpr_w
    add    $tmp_gpr_w, $ctr32w, #2
    rev    $tmp_gpr_w, $tmp_gpr_w
    mov    $ctr2.s[3], $tmp_gpr_w
    add    $tmp_gpr_w, $ctr32w, #3
    rev    $tmp_gpr_w, $tmp_gpr_w
    mov    $ctr3.s[3], $tmp_gpr_w
    add    $tmp_gpr_w, $ctr32w, #4
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr0_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #5
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr1_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #6
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr2_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #7
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr3_sp_offset + 12 ]}]
    add    $ctr32w, $ctr32w, #8
    // Pre-compute this value instead of using two instructions for moving and then shifting in the main loop
    mov    $mod_constantx, #0xc200000000000000
    str    $mod_constantx, [sp, #$mod_constant_sp_offset]
    aese    $ctr0.16b, $rk0.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 0
    ldp    $rk1q, $rk2q, [$cc, #16]                                           // load rk1, rk2
    aese    $ctr1.16b, $rk0.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 0
    ldp    $rk3q, $rk4q, [$cc, #48]                                           // load rk3, rk4
    aese    $ctr2.16b, $rk0.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 0
    ldp    $rk5q, $rk6q, [$cc, #80]                                           // load rk5, rk6
    aese    $ctr3.16b, $rk0.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 0
    ldp    $h2q, $h3q, [$Htable, #32]                                         // load h2, h3
    aese    $ctr0.16b, $rk1.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 1
    ldp    $rk7q, $rk8q, [$cc, #112]                                          // load rk7, rk8
    aese    $ctr1.16b, $rk1.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 1
    aese    $ctr2.16b, $rk1.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 1
    aese    $ctr3.16b, $rk1.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 1
    aese    $ctr0.16b, $rk2.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 2
    ext    $h3.16b, $h3.16b, $h3.16b, #8
    aese    $ctr1.16b, $rk2.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 2
    ext    $h2.16b, $h2.16b, $h2.16b, #8
    ldr    $h4q, [$Htable, #80]                                               // load h4
    aese    $ctr2.16b, $rk2.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 2
    aese    $ctr3.16b, $rk2.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 2
    ext    $h4.16b, $h4.16b, $h4.16b, #8
    aese    $ctr0.16b, $rk3.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 3
    ld1    { $acc_l.16b}, [$current_tag]
    aese    $ctr1.16b, $rk3.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 3
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8
    aese    $ctr2.16b, $rk3.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 3
    rev64    $acc_l.16b, $acc_l.16b
    aese    $ctr3.16b, $rk3.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 3
    trn2    $h34k.2d,  $h3.2d,    $h4.2d                                      // h4l | h3l
    aese    $ctr0.16b, $rk4.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 4
    ldr    $h1q, [$Htable]                                                    // load h1
    aese    $ctr1.16b, $rk4.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 4
    ext    $h1.16b, $h1.16b, $h1.16b, #8
    aese    $ctr2.16b, $rk4.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 4
    trn1    $acc_h.2d, $h3.2d,    $h4.2d                                      // h4h | h3h
    aese    $ctr3.16b, $rk4.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 4
    trn2    $h12k.2d,  $h1.2d,    $h2.2d                                      // h2l | h1l
    aese    $ctr0.16b, $rk5.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 5
    aese    $ctr1.16b, $rk5.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 5
    aese    $ctr3.16b, $rk5.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 5
    aese    $ctr2.16b, $rk5.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 5
    aese    $ctr0.16b, $rk6.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 6
    aese    $ctr1.16b, $rk6.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 6
    aese    $ctr2.16b, $rk6.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 6
    aese    $ctr3.16b, $rk6.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 6
    aese    $ctr0.16b, $rk7.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 7
    aese    $ctr1.16b, $rk7.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 7
    aese    $ctr2.16b, $rk7.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 7
    aese    $ctr3.16b, $rk7.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 7
    aese    $ctr0.16b, $rk8.16b \n    aesmc    $ctr0.16b, $ctr0.16b           // AES block 0 - round 8
    aese    $ctr1.16b, $rk8.16b \n    aesmc    $ctr1.16b, $ctr1.16b           // AES block 1 - round 8
    aese    $ctr2.16b, $rk8.16b \n    aesmc    $ctr2.16b, $ctr2.16b           // AES block 2 - round 8
    aese    $ctr3.16b, $rk8.16b \n    aesmc    $ctr3.16b, $ctr3.16b           // AES block 3 - round 8
    cmp    $rounds, #12                                                       // setup flags for AES-128/192/256 check
    b.lt    .Ldec_finish_first_blocks                                         // branch if AES-128
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #144]                               // load rk9, rk10
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 9
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 9
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 9
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 9
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 10
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 10
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 10
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 10
    b.eq    .Ldec_finish_first_blocks                                         // branch if AES-192
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #176]                               // load rk11, rk12
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 11
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 11
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 11
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 11
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 12
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 12
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 12
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 12
.Ldec_finish_first_blocks:
    ldr    $rk9_11_tmpq, [$input_l1]                                          // load rkN
    cmp    $input_ptr, $main_end_input_ptr                                    // check if we have <= 4 blocks
    eor    $h34k.16b, $h34k.16b, $acc_h.16b                                   // h4k | h3k
    aese    $ctr1.16b, $rkNm1.16b                                             // AES block 1 - round N-1
    aese    $ctr2.16b, $rkNm1.16b                                             // AES block 2 - round N-1
    aese    $ctr3.16b, $rkNm1.16b                                             // AES block 3 - round N-1
    aese    $ctr0.16b, $rkNm1.16b                                             // AES block 0 - round N-1
    trn1    $t0.2d,    $h1.2d,    $h2.2d                                      // h2h | h1h
    eor    $h12k.16b, $h12k.16b, $t0.16b                                      // h2k | h1k
    b.ge    .Ldec_tail                                                        // handle tail
    // Setup for AES blocks 0-3 is done purely on NEON side instead of mixing NEON and scalar instructions.
    // This is because the final result of the AES block needs to be EORd with the final round key
    // value ($v_rkN). This avoids several fmovs.
    ldp    $res2q, $res3q, [$input_ptr, #32]                                  // AES blocks 2,3 load ciphertext
    ldp    $res0q, $res1q, [$input_ptr], #64                                  // AES blocks 0,1 load ciphertext
    PLATFORM_EOR3($ctr0, $res0, $ctr0, $rk9_11_tmp)                           // AES block 0 - result
    PLATFORM_EOR3($ctr1, $res1, $ctr1, $rk9_11_tmp)                           // AES block 1 - result
    PLATFORM_EOR3($ctr2, $res2, $ctr2, $rk9_11_tmp)                           // AES block 2 - result
    PLATFORM_EOR3($ctr3, $res3, $ctr3, $rk9_11_tmp)                           // AES block 3 - result
    st1    { $ctr0.16b, $ctr1.16b, $ctr2.16b, $ctr3.16b}, [$output_ptr], #64  // AES blocks 0-3 - store result
    ldr    $ctr0q,     [sp, #$ctr0_sp_offset]
    ldr    $ctr1q,     [sp, #$ctr1_sp_offset]
    ldr    $ctr2q,     [sp, #$ctr2_sp_offset]
    ldr    $ctr3q,     [sp, #$ctr3_sp_offset]
    rev    $tmp_gpr_w, $ctr32w
    str    $tmp_gpr_w, [sp, #@{[ $ctr0_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #1
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr1_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #2
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr2_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #3
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr3_sp_offset + 12 ]}]
    add    $ctr32w, $ctr32w, #4
    cmp    $input_ptr, $main_end_input_ptr                                    // check if we have <= 4 blocks
    b.ge    .Ldec_prepretail                                                  // do prepretail
.Ldec_main_loop:    // main loop start
    aese    $ctr0.16b, $rk0.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 0
    aese    $ctr1.16b, $rk0.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 0
    aese    $ctr2.16b, $rk0.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 0
    aese    $ctr3.16b, $rk0.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 0
    aese    $ctr0.16b, $rk1.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 1
    aese    $ctr1.16b, $rk1.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 1
    aese    $ctr2.16b, $rk1.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 1
    aese    $ctr3.16b, $rk1.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 1
    rev64    $res0.16b, $res0.16b                                             // GHASH block 4k
    rev64    $res1.16b, $res1.16b                                             // GHASH block 4k+1
    rev64    $res2.16b, $res2.16b                                             // GHASH block 4k+2
    rev64    $res3.16b, $res3.16b                                             // GHASH block 4k+3
    aese    $ctr0.16b, $rk2.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 2
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8                             // PRE 0
    aese    $ctr1.16b, $rk2.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 2
    eor    $res0.16b, $res0.16b, $acc_l.16b                                   // PRE 1
    aese    $ctr2.16b, $rk2.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 2
    pmull    $acc_l.1q, $res0.1d, $h4.1d                                      // GHASH block 4k - low
    aese    $ctr3.16b, $rk2.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 2
    pmull2    $acc_h.1q, $res0.2d, $h4.2d                                     // GHASH block 4k - high
    aese    $ctr0.16b, $rk3.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 3
    mov    $acc_md, $h34k.d[1]                                                // GHASH block 4k - mid
    aese    $ctr1.16b, $rk3.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 3
    mov    $t0d, $res0.d[1]                                                   // GHASH block 4k - mid
    aese    $ctr2.16b, $rk3.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 3
    eor    $t0.8b, $t0.8b, $res0.8b                                           // GHASH block 4k - mid
    aese    $ctr3.16b, $rk3.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 3
    aese    $ctr0.16b, $rk4.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 4
    pmull2    $t1.1q, $res1.2d, $h3.2d                                        // GHASH block 4k+1 - high
    aese    $ctr1.16b, $rk4.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 4
    eor    $acc_h.16b, $acc_h.16b, $t1.16b                                    // GHASH block 4k+1 - high
    aese    $ctr2.16b, $rk4.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 4
    mov    $t3d, $res1.d[1]                                                   // GHASH block 4k+1 - mid
    aese    $ctr3.16b, $rk4.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 4
    pmull    $acc_m.1q, $t0.1d, $acc_m.1d                                     // GHASH block 4k - mid
    aese    $ctr0.16b, $rk5.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 5
    pmull    $t2.1q, $res1.1d, $h3.1d                                         // GHASH block 4k+1 - low
    aese    $ctr1.16b, $rk5.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 5
    eor    $t3.8b, $t3.8b, $res1.8b                                           // GHASH block 4k+1 - mid
    aese    $ctr3.16b, $rk5.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 5
    aese    $ctr2.16b, $rk5.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 5
    pmull    $t5.1q, $res2.1d, $h2.1d                                         // GHASH block 4k+2 - low
    aese    $ctr0.16b, $rk6.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 6
    PLATFORM_EOR3($acc_l, $acc_l, $t2, $t5)                                   // GHASH block 4k+1 - low & GHASH block 4k+2 - low
    aese    $ctr1.16b, $rk6.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 6
    mov    $t6d, $res2.d[1]                                                   // GHASH block 4k+2 - mid
    aese    $ctr2.16b, $rk6.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 6
    eor    $t6.8b, $t6.8b, $res2.8b                                           // GHASH block 4k+2 - mid
    pmull    $t3.1q, $t3.1d, $h34k.1d                                         // GHASH block 4k+1 - mid
    ins    $t6.d[1], $t6.d[0]                                                 // GHASH block 4k+2 - mid
    aese    $ctr3.16b, $rk6.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 6
    pmull2    $rk10_12.1q, $res2.2d, $h2.2d                                   // GHASH block 4k+2 - high
    mov    $t9d, $res3.d[1]                                                   // GHASH block 4k+3 - mid
    aese    $ctr0.16b, $rk7.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 7
    pmull2    $t6.1q, $t6.2d, $h12k.2d                                        // GHASH block 4k+2 - mid
    aese    $ctr1.16b, $rk7.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 7
    pmull    $rk9_11_tmp.1q, $res3.1d, $h1.1d                                 // GHASH block 4k+3 - low
    aese    $ctr2.16b, $rk7.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 7
    PLATFORM_EOR3($acc_m, $acc_m, $t3, $t6)                                   // GHASH block 4k+1 - mid & GHASH block 4k+2 - mid
    aese    $ctr3.16b, $rk7.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 7
    pmull2    $t7.1q, $res3.2d, $h1.2d                                        // GHASH block 4k+3 - high
    eor    $t9.8b, $t9.8b, $res3.8b                                           // GHASH block 4k+3 - mid
    PLATFORM_EOR3($acc_h, $acc_h, $rk10_12, $t7)                              // GHASH block 4k+2 - high & GHASH block 4k+3 - high
    aese    $ctr0.16b, $rk8.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 4k+4 - round 8
    aese    $ctr1.16b, $rk8.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 4k+5 - round 8
    aese    $ctr2.16b, $rk8.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 4k+6 - round 8
    pmull    $t9.1q, $t9.1d, $h12k.1d                                         // GHASH block 4k+3 - mid
    ldr    $mod_constantd, [sp, #$mod_constant_sp_offset]
    aese    $ctr3.16b, $rk8.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 4k+7 - round 8
    pmull    $mod_t.1q, $acc_h.1d, $mod_constant.1d                           // MODULO - top 64b align with mid
    PLATFORM_EOR3($acc_m, $acc_m, $t9, $mod_t)                                // GHASH block 4k+3 - mid & MODULO - fold into mid
    eor    $acc_l.16b, $acc_l.16b, $rk9_11_tmp.16b                            // GHASH block 4k+3 - low
    eor    $t9.16b, $acc_l.16b, $acc_h.16b                                    // MODULO - karatsuba tidy up
    ext    $acc_h.16b, $acc_h.16b, $acc_h.16b, #8                             // MODULO - other top alignment
    PLATFORM_EOR3($acc_m, $acc_m, $t9, $acc_h)                                // MODULO - karatsuba tidy up & MODULO - fold into mid
    pmull    $mod_constant.1q, $acc_m.1d, $mod_constant.1d                    // MODULO - mid 64b align with low
    ext    $acc_m.16b, $acc_m.16b, $acc_m.16b, #8                             // MODULO - other mid alignment
    PLATFORM_EOR3($acc_l, $acc_l, $mod_constant, $acc_m)                      // MODULO - fold into low
    cmp    $rounds_w, #12                                                     // setup flags for AES-128/192/256 check
    b.lt    .Ldec_main_loop_continue                                          // branch if AES-128
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #144]                               // load rk9, rk10
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 4k+4 - round 9
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 4k+5 - round 9
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 4k+6 - round 9
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 4k+7 - round 9
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 4k+4 - round 10
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 4k+5 - round 10
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 4k+6 - round 10
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 4k+7 - round 10
    b.eq    .Ldec_main_loop_continue                                          // branch if AES-192
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #176]                               // load rk11, rk12
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 4k+4 - round 11
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 4k+5 - round 11
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 4k+6 - round 11
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 4k+7 - round 11
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 4k+4 - round 12
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 4k+5 - round 12
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 4k+6 - round 12
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 4k+7 - round 12
.Ldec_main_loop_continue:
    ldr    $rk9_11_tmpq, [$input_l1]                                          // load rkN
    ldp    $res2q, $res3q, [$input_ptr, #32]                                  // AES blocks 2,3 load ciphertext
    ldp    $res0q, $res1q, [$input_ptr], #64                                  // AES blocks 0,1 load ciphertext
    aese    $ctr0.16b, $rkNm1.16b                                             // AES block 4k+4 - round N-1
    PLATFORM_EOR3($ctr0, $res0, $ctr0, $rk9_11_tmp)                           // AES block 4k+4 - result
    aese    $ctr1.16b, $rkNm1.16b                                             // AES block 4k+5 - round N-1
    PLATFORM_EOR3($ctr1, $res1, $ctr1, $rk9_11_tmp)                           // AES block 4k+5 - result
    aese    $ctr2.16b, $rkNm1.16b                                             // AES block 4k+6 - round N-1
    PLATFORM_EOR3($ctr2, $res2, $ctr2, $rk9_11_tmp)                           // AES block 4k+6 - result
    aese    $ctr3.16b, $rkNm1.16b                                             // AES block 4k+7 - round N-1
    PLATFORM_EOR3($ctr3, $res3, $ctr3, $rk9_11_tmp)                           // AES block 4k+7 - result
    st1    { $ctr0.16b, $ctr1.16b, $ctr2.16b, $ctr3.16b}, [$output_ptr], #64  // AES blocks 4k+4-7 - store result
    ldr    $ctr0q,     [sp, #$ctr0_sp_offset]
    ldr    $ctr1q,     [sp, #$ctr1_sp_offset]
    ldr    $ctr2q,     [sp, #$ctr2_sp_offset]
    ldr    $ctr3q,     [sp, #$ctr3_sp_offset]
    rev    $tmp_gpr_w, $ctr32w
    str    $tmp_gpr_w, [sp, #@{[ $ctr0_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #1
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr1_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #2
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr2_sp_offset + 12 ]}]
    add    $tmp_gpr_w, $ctr32w, #3
    rev    $tmp_gpr_w, $tmp_gpr_w
    str    $tmp_gpr_w, [sp, #@{[ $ctr3_sp_offset + 12 ]}]
    add    $ctr32w, $ctr32w, #4
    cmp    $input_ptr, $main_end_input_ptr
    b.lt    .Ldec_main_loop
.Ldec_prepretail:                                                             // PREPRETAIL
    rev64    $res0.16b, $res0.16b                                             // GHASH block 0
    rev64    $res1.16b, $res1.16b                                             // GHASH block 1
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8                             // PRE 0
    aese    $ctr0.16b, $rk0.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 0
    aese    $ctr1.16b, $rk0.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 0
    eor    $res0.16b, $res0.16b, $acc_l.16b                                   // PRE 1
    rev64    $res2.16b, $res2.16b                                             // GHASH block 2
    aese    $ctr1.16b, $rk1.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 1
    pmull    $acc_l.1q, $res0.1d, $h4.1d                                      // GHASH block 0 - low
    mov    $t0d, $res0.d[1]                                                   // GHASH block 0 - mid
    pmull2    $acc_h.1q, $res0.2d, $h4.2d                                     // GHASH block 0 - high
    aese    $ctr2.16b, $rk0.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 0
    mov    $acc_md, $h34k.d[1]                                                // GHASH block 0 - mid
    aese    $ctr0.16b, $rk1.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 1
    eor    $t0.8b, $t0.8b, $res0.8b                                           // GHASH block 0 - mid
    pmull2    $t1.1q, $res1.2d, $h3.2d                                        // GHASH block 1 - high
    aese    $ctr2.16b, $rk1.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 1
    rev64    $res3.16b, $res3.16b                                             // GHASH block 3
    aese    $ctr3.16b, $rk0.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 0
    pmull    $acc_m.1q, $t0.1d, $acc_m.1d                                     // GHASH block 0 - mid
    eor    $acc_h.16b, $acc_h.16b, $t1.16b                                    // GHASH block 1 - high
    pmull    $t2.1q, $res1.1d, $h3.1d                                         // GHASH block 1 - low
    aese    $ctr3.16b, $rk1.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 1
    mov    $t3d, $res1.d[1]                                                   // GHASH block 1 - mid
    aese    $ctr0.16b, $rk2.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 2
    aese    $ctr1.16b, $rk2.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 2
    eor    $acc_l.16b, $acc_l.16b, $t2.16b                                    // GHASH block 1 - low
    aese    $ctr2.16b, $rk2.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 2
    aese    $ctr0.16b, $rk3.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 3
    mov    $t6d, $res2.d[1]                                                   // GHASH block 2 - mid
    aese    $ctr3.16b, $rk2.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 2
    eor    $t3.8b, $t3.8b, $res1.8b                                           // GHASH block 1 - mid
    pmull    $t5.1q, $res2.1d, $h2.1d                                         // GHASH block 2 - low
    aese    $ctr0.16b, $rk4.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 4
    aese    $ctr3.16b, $rk3.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 3
    eor    $t6.8b, $t6.8b, $res2.8b                                           // GHASH block 2 - mid
    pmull    $t3.1q, $t3.1d, $h34k.1d                                         // GHASH block 1 - mid
    aese    $ctr0.16b, $rk5.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 5
    eor    $acc_l.16b, $acc_l.16b, $t5.16b                                    // GHASH block 2 - low
    aese    $ctr3.16b, $rk4.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 4
    pmull2    $t7.1q, $res3.2d, $h1.2d                                        // GHASH block 3 - high
    eor    $acc_m.16b, $acc_m.16b, $t3.16b                                    // GHASH block 1 - mid
    pmull2    $t4.1q, $res2.2d, $h2.2d                                        // GHASH block 2 - high
    aese    $ctr3.16b, $rk5.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 5
    ins    $t6.d[1], $t6.d[0]                                                 // GHASH block 2 - mid
    aese    $ctr2.16b, $rk3.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 3
    aese    $ctr1.16b, $rk3.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 3
    PLATFORM_EOR3($acc_h, $acc_h, $t4, $t7)                                   // GHASH block 2 - high & GHASH block 3 - high
    pmull    $t8.1q, $res3.1d, $h1.1d                                         // GHASH block 3 - low
    aese    $ctr2.16b, $rk4.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 4
    mov    $t9d, $res3.d[1]                                                   // GHASH block 3 - mid
    aese    $ctr1.16b, $rk4.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 4
    pmull2    $t6.1q, $t6.2d, $h12k.2d                                        // GHASH block 2 - mid
    aese    $ctr2.16b, $rk5.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 5
    eor    $t9.8b, $t9.8b, $res3.8b                                           // GHASH block 3 - mid
    aese    $ctr1.16b, $rk5.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 5
    aese    $ctr3.16b, $rk6.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 6
    eor    $acc_m.16b, $acc_m.16b, $t6.16b                                    // GHASH block 2 - mid
    aese    $ctr2.16b, $rk6.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 6
    aese    $ctr0.16b, $rk6.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 6
    movi    $mod_constant.8b, #0xc2
    aese    $ctr1.16b, $rk6.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 6
    eor    $acc_l.16b, $acc_l.16b, $t8.16b                                    // GHASH block 3 - low
    pmull    $t9.1q, $t9.1d, $h12k.1d                                         // GHASH block 3 - mid
    aese    $ctr3.16b, $rk7.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 7
    aese    $ctr1.16b, $rk7.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 7
    aese    $ctr0.16b, $rk7.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 7
    eor    $acc_m.16b, $acc_m.16b, $t9.16b                                    // GHASH block 3 - mid
    aese    $ctr3.16b, $rk8.16b  \n    aesmc    $ctr3.16b, $ctr3.16b          // AES block 3 - round 8
    aese    $ctr2.16b, $rk7.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 7
    eor    $t9.16b, $acc_l.16b, $acc_h.16b                                    // MODULO - karatsuba tidy up
    aese    $ctr1.16b, $rk8.16b  \n    aesmc    $ctr1.16b, $ctr1.16b          // AES block 1 - round 8
    aese    $ctr0.16b, $rk8.16b  \n    aesmc    $ctr0.16b, $ctr0.16b          // AES block 0 - round 8
    shl    $mod_constantd, $mod_constantd, #56                                // mod_constant
    aese    $ctr2.16b, $rk8.16b  \n    aesmc    $ctr2.16b, $ctr2.16b          // AES block 2 - round 8
    cmp    $rounds_w, #12                                                     // setup flags for AES-128/192/256 check
    b.lt    .Ldec_finish_prepretail                                           // branch if AES-128
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #144]                               // load rk9, rk10
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 9
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 9
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 9
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 9
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 10
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 10
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 10
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 10
    b.eq    .Ldec_finish_prepretail                                           // branch if AES-192
    ldp    $rk9_11_tmpq, $rk10_12q, [$cc, #176]                               // load rk11, rk12
    aese    $ctr0.16b, $rk9_11_tmp.16b \n    aesmc    $ctr0.16b, $ctr0.16b    // AES block 0 - round 11
    aese    $ctr1.16b, $rk9_11_tmp.16b \n    aesmc    $ctr1.16b, $ctr1.16b    // AES block 1 - round 11
    aese    $ctr2.16b, $rk9_11_tmp.16b \n    aesmc    $ctr2.16b, $ctr2.16b    // AES block 2 - round 11
    aese    $ctr3.16b, $rk9_11_tmp.16b \n    aesmc    $ctr3.16b, $ctr3.16b    // AES block 3 - round 11
    aese    $ctr0.16b, $rk10_12.16b \n    aesmc    $ctr0.16b, $ctr0.16b       // AES block 0 - round 12
    aese    $ctr1.16b, $rk10_12.16b \n    aesmc    $ctr1.16b, $ctr1.16b       // AES block 1 - round 12
    aese    $ctr2.16b, $rk10_12.16b \n    aesmc    $ctr2.16b, $ctr2.16b       // AES block 2 - round 12
    aese    $ctr3.16b, $rk10_12.16b \n    aesmc    $ctr3.16b, $ctr3.16b       // AES block 3 - round 12
.Ldec_finish_prepretail:
    eor    $acc_m.16b, $acc_m.16b, $t9.16b                                    // MODULO - karatsuba tidy up
    pmull    $mod_t.1q, $acc_h.1d, $mod_constant.1d                           // MODULO - top 64b align with mid
    ext    $acc_h.16b, $acc_h.16b, $acc_h.16b, #8                             // MODULO - other top alignment
    PLATFORM_EOR3($acc_m, $acc_m, $mod_t, $acc_h)                             // MODULO - fold into mid
    pmull    $mod_constant.1q, $acc_m.1d, $mod_constant.1d                    // MODULO - mid 64b align with low
    ext    $acc_m.16b, $acc_m.16b, $acc_m.16b, #8                             // MODULO - other mid alignment
    PLATFORM_EOR3($acc_l, $acc_l, $mod_constant, $acc_m)                      // MODULO - fold into low
    aese    $ctr1.16b, $rkNm1.16b                                             // AES block 1 - round N-1
    aese    $ctr0.16b, $rkNm1.16b                                             // AES block 0 - round N-1
    aese    $ctr3.16b, $rkNm1.16b                                             // AES block 3 - round N-1
    aese    $ctr2.16b, $rkNm1.16b                                             // AES block 2 - round N-1
.Ldec_tail:                                                                   // TAIL
    sub    $main_end_input_ptr, $end_input_ptr, $input_ptr                    // main_end_input_ptr is number of bytes left to process
    ld1    { $res1.16b}, [$input_ptr], #16                                    // AES block 0 - load ciphertext
    eor    $ctr0.16b, $res1.16b, $ctr0.16b                                    // AES block 0 - result
    mov    $output_l0, $ctr0.d[0]                                             // AES block 0 - mov low
    mov    $output_h0, $ctr0.d[1]                                             // AES block 0 - mov high
    ext    $t0.16b, $acc_l.16b, $acc_l.16b, #8                                // prepare final partial tag
    eor    $output_l0, $output_l0, $rkN_l                                     // AES block 0 - round N low
    eor    $output_h0, $output_h0, $rkN_h                                     // AES block 0 - round N high
    cmp    $main_end_input_ptr, #48
    b.gt    .Ldec_blocks_more_than_3
    mov    $ctr3.16b, $ctr2.16b
    movi    $acc_m.8b, #0
    movi    $acc_l.8b, #0
    movi    $acc_h.8b, #0
    mov    $ctr2.16b, $ctr1.16b
    cmp    $main_end_input_ptr, #32
    b.gt    .Ldec_blocks_more_than_2
    mov    $ctr3.16b, $ctr1.16b
    cmp    $main_end_input_ptr, #16
    b.gt    .Ldec_blocks_more_than_1
    b    .Ldec_blocks_less_than_1
.Ldec_blocks_more_than_3:                                                     // blocks left >  3
    rev64    $res0.16b, $res1.16b                                             // GHASH final-3 block
    ld1    { $res1.16b}, [$input_ptr], #16                                    // AES final-2 block - load ciphertext
    stp    $output_l0, $output_h0, [$output_ptr], #16                         // AES final-3 block  - store result
    mov    $acc_md, $h34k.d[1]                                                // GHASH final-3 block - mid
    eor    $res0.16b, $res0.16b, $t0.16b                                      // feed in partial tag
    eor    $ctr0.16b, $res1.16b, $ctr1.16b                                    // AES final-2 block - result
    mov    $rk4d, $res0.d[1]                                                  // GHASH final-3 block - mid
    mov    $output_l0, $ctr0.d[0]                                             // AES final-2 block - mov low
    mov    $output_h0, $ctr0.d[1]                                             // AES final-2 block - mov high
    eor    $rk4v.8b, $rk4v.8b, $res0.8b                                       // GHASH final-3 block - mid
    movi    $t0.8b, #0                                                        // suppress further partial tag feed in
    pmull2    $acc_h.1q, $res0.2d, $h4.2d                                     // GHASH final-3 block - high
    pmull    $acc_m.1q, $rk4v.1d, $acc_m.1d                                   // GHASH final-3 block - mid
    eor    $output_l0, $output_l0, $rkN_l                                     // AES final-2 block - round N low
    pmull    $acc_l.1q, $res0.1d, $h4.1d                                      // GHASH final-3 block - low
    eor    $output_h0, $output_h0, $rkN_h                                     // AES final-2 block - round N high
.Ldec_blocks_more_than_2:                                                     // blocks left >  2
    rev64    $res0.16b, $res1.16b                                             // GHASH final-2 block
    ld1    { $res1.16b}, [$input_ptr], #16                                    // AES final-1 block - load ciphertext
    eor    $res0.16b, $res0.16b, $t0.16b                                      // feed in partial tag
    stp    $output_l0, $output_h0, [$output_ptr], #16                         // AES final-2 block  - store result
    eor    $ctr0.16b, $res1.16b, $ctr2.16b                                    // AES final-1 block - result
    mov    $rk4d, $res0.d[1]                                                  // GHASH final-2 block - mid
    pmull    $rk3q1, $res0.1d, $h3.1d                                         // GHASH final-2 block - low
    pmull2    $rk2.1q, $res0.2d, $h3.2d                                       // GHASH final-2 block - high
    eor    $rk4v.8b, $rk4v.8b, $res0.8b                                       // GHASH final-2 block - mid
    mov    $output_l0, $ctr0.d[0]                                             // AES final-1 block - mov low
    mov    $output_h0, $ctr0.d[1]                                             // AES final-1 block - mov high
    eor    $acc_l.16b, $acc_l.16b, $rk3.16b                                   // GHASH final-2 block - low
    movi    $t0.8b, #0                                                        // suppress further partial tag feed in
    pmull    $rk4v.1q, $rk4v.1d, $h34k.1d                                     // GHASH final-2 block - mid
    eor    $acc_h.16b, $acc_h.16b, $rk2.16b                                   // GHASH final-2 block - high
    eor    $output_l0, $output_l0, $rkN_l                                     // AES final-1 block - round N low
    eor    $acc_m.16b, $acc_m.16b, $rk4v.16b                                  // GHASH final-2 block - mid
    eor    $output_h0, $output_h0, $rkN_h                                     // AES final-1 block - round N high
.Ldec_blocks_more_than_1:                                                     // blocks left >  1
    stp    $output_l0, $output_h0, [$output_ptr], #16                         // AES final-1 block  - store result
    rev64    $res0.16b, $res1.16b                                             // GHASH final-1 block
    ld1    { $res1.16b}, [$input_ptr], #16                                    // AES final block - load ciphertext
    eor    $res0.16b, $res0.16b, $t0.16b                                      // feed in partial tag
    movi    $t0.8b, #0                                                        // suppress further partial tag feed in
    mov    $rk4d, $res0.d[1]                                                  // GHASH final-1 block - mid
    eor    $ctr0.16b, $res1.16b, $ctr3.16b                                    // AES final block - result
    pmull2    $rk2q1, $res0.2d, $h2.2d                                        // GHASH final-1 block - high
    eor    $rk4v.8b, $rk4v.8b, $res0.8b                                       // GHASH final-1 block - mid
    pmull    $rk3q1, $res0.1d, $h2.1d                                         // GHASH final-1 block - low
    mov    $output_l0, $ctr0.d[0]                                             // AES final block - mov low
    ins    $rk4v.d[1], $rk4v.d[0]                                             // GHASH final-1 block - mid
    mov    $output_h0, $ctr0.d[1]                                             // AES final block - mov high
    pmull2    $rk4v.1q, $rk4v.2d, $h12k.2d                                    // GHASH final-1 block - mid
    eor    $output_l0, $output_l0, $rkN_l                                     // AES final block - round N low
    eor    $acc_l.16b, $acc_l.16b, $rk3.16b                                   // GHASH final-1 block - low
    eor    $acc_h.16b, $acc_h.16b, $rk2.16b                                   // GHASH final-1 block - high
    eor    $acc_m.16b, $acc_m.16b, $rk4v.16b                                  // GHASH final-1 block - mid
    eor    $output_h0, $output_h0, $rkN_h                                     // AES final block - round N high
.Ldec_blocks_less_than_1:                                                     // blocks left <= 1
    add    $T32, $T32, $bit_length, lsr #7                                    // Calculate the updated counter based on the number of 16B chunks we processed
    rev    $W32, $W32
    str    $W32, [$counter, #12]                                              // store the updated counter
    and    $bit_length, $bit_length, #127                                     // bit_length %= 128
    mvn    $rkN_h, xzr                                                        // rkN_h = 0xffffffffffffffff
    sub    $bit_length, $bit_length, #128                                     // bit_length -= 128
    mvn    $rkN_l, xzr                                                        // rkN_l = 0xffffffffffffffff
    ldp    $end_input_ptr, $main_end_input_ptr, [$output_ptr]                 // load existing bytes we need to not overwrite
    neg    $bit_length, $bit_length                                           // bit_length = 128 - #bits in input (in range [1,128])
    and    $bit_length, $bit_length, #127                                     // bit_length %= 128
    lsr    $rkN_h, $rkN_h, $bit_length                                        // rkN_h is mask for top 64b of last block
    cmp    $bit_length, #64
    csel    $ctr32x, $rkN_l, $rkN_h, lt
    csel    $ctr96_b64x, $rkN_h, xzr, lt
    fmov    $ctr0d, $ctr32x                                                   // ctr0b is mask for last block
    and    $output_l0, $output_l0, $ctr32x
    mov    $ctr0.d[1], $ctr96_b64x
    bic    $end_input_ptr, $end_input_ptr, $ctr32x                            // mask out low existing bytes
    bic    $main_end_input_ptr, $main_end_input_ptr, $ctr96_b64x              // mask out high existing bytes
    orr    $output_l0, $output_l0, $end_input_ptr
    and    $output_h0, $output_h0, $ctr96_b64x
    orr    $output_h0, $output_h0, $main_end_input_ptr
    and    $res1.16b, $res1.16b, $ctr0.16b                                    // possibly partial last block has zeroes in highest bits
    rev64    $res0.16b, $res1.16b                                             // GHASH final block
    eor    $res0.16b, $res0.16b, $t0.16b                                      // feed in partial tag
    pmull    $rk3q1, $res0.1d, $h1.1d                                         // GHASH final block - low
    mov    $t0d, $res0.d[1]                                                   // GHASH final block - mid
    eor    $t0.8b, $t0.8b, $res0.8b                                           // GHASH final block - mid
    pmull2    $rk2q1, $res0.2d, $h1.2d                                        // GHASH final block - high
    pmull    $t0.1q, $t0.1d, $h12k.1d                                         // GHASH final block - mid
    eor    $acc_h.16b, $acc_h.16b, $rk2.16b                                   // GHASH final block - high
    eor    $acc_l.16b, $acc_l.16b, $rk3.16b                                   // GHASH final block - low
    eor    $acc_m.16b, $acc_m.16b, $t0.16b                                    // GHASH final block - mid
    ldr    $mod_constantd, [sp, #$mod_constant_sp_offset]
    eor    $t9.16b, $acc_l.16b, $acc_h.16b                                    // MODULO - karatsuba tidy up
    eor    $acc_m.16b, $acc_m.16b, $t9.16b                                    // MODULO - karatsuba tidy up
    pmull    $mod_t.1q, $acc_h.1d, $mod_constant.1d                           // MODULO - top 64b align with mid
    ext    $acc_h.16b, $acc_h.16b, $acc_h.16b, #8                             // MODULO - other top alignment
    eor    $acc_m.16b, $acc_m.16b, $mod_t.16b                                 // MODULO - fold into mid
    eor    $acc_m.16b, $acc_m.16b, $acc_h.16b                                 // MODULO - fold into mid
    pmull    $mod_constant.1q, $acc_m.1d, $mod_constant.1d                    // MODULO - mid 64b align with low
    ext    $acc_m.16b, $acc_m.16b, $acc_m.16b, #8                             // MODULO - other mid alignment
    eor    $acc_l.16b, $acc_l.16b, $mod_constant.16b                          // MODULO - fold into low
    stp    $output_l0, $output_h0, [$output_ptr]
    eor    $acc_l.16b, $acc_l.16b, $acc_m.16b                                 // MODULO - fold into low
    ext    $acc_l.16b, $acc_l.16b, $acc_l.16b, #8
    rev64    $acc_l.16b, $acc_l.16b                                           // Final Tag
    mov    x0, $len
    st1    { $acc_l.16b }, [$current_tag]                                     // Store final tag
    ldp    x19, x20, [sp, #16]
    ldp    x21, x22, [sp, #32]
    ldp    x23, x24, [sp, #48]
    ldp    d8, d9, [sp, #64]
    ldp    d10, d11, [sp, #80]
    ldp    d12, d13, [sp, #96]
    ldp    d14, d15, [sp, #112]
    ldp    x29, x30, [sp], #224
    AARCH64_VALIDATE_LINK_REGISTER
    ret
.size aes_gcm_dec_kernel,.-aes_gcm_dec_kernel
___
    # 1. Print directives
    print $header_directives;

    # 2. Print Legacy implementation
    print get_transformed_code(0, $code_template);

    print "\n";

    # 3. Print EOR3 implementation
    print get_transformed_code(1, $code_template);

    print <<'___';
#endif  // __ARM_MAX_ARCH__ >= 8
___

    # Close the handle (which flushes to arm-xlate.pl)
    close STDOUT or die "error closing STDOUT: $!";
}
}
