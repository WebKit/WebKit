// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64) && defined(__APPLE__)
#if __ARM_MAX_ARCH__ >= 8
.arch_extension crypto
.arch_extension sha3

.text
.globl	_aes_gcm_enc_kernel
.private_extern	_aes_gcm_enc_kernel

.align	4
_aes_gcm_enc_kernel:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29, x30, [sp, #-224]!
	mov	x29, sp
	ld1	{ v0.16b}, [x4]                                                 // Load initial counter block
	stp	x19, x20, [sp, #16]
	mov	v1.16b, v0.16b                                               // Initialize ctr1-3 from ctr0
	mov	v2.16b, v0.16b
	mov	v3.16b, v0.16b
	mov	x16, x4                                                       // Pointer to counter block in memory
	mov	x8, x5                                                            // Pointer to AES key schedule context
	stp	x21, x22, [sp, #32]
    // [sp, #48] is unused but allocated to align the stack layout with aes_gcm_dec_kernel
	stp	d8, d9, [sp, #64]                                                  // Save Neon registers
	stp	d10, d11, [sp, #80]
	stp	d12, d13, [sp, #96]
	stp	d14, d15, [sp, #112]
	ldr	w17, [x8, #240]                                             // Load number of AES rounds
	add	x7, x8, x17, lsl #4                                     // Calculate pointer to the last round key
	ldp	x13, x14, [x7]                                         // load round N key (for final XOR)
	ldr	q31, [x7, #-16]                                          // load round N-1 key
	add	x4, x0, x1, lsr #3                    // Calculate end of input
	lsr	x5, x1, #3                               // Total byte length
	mov	x15, x5
	ldr	w12, [x16, #12]                                            // Load counter's low 32 bits
	sub	x5, x5, #1                       // byte_len - 1
	ldr	q18, [x8, #0]                                                   // load rk0
	and	x5, x5, #0xffffffffffffffc0      // Align main loop end to a multiple of 64 bytes
	add	x5, x5, x0
	rev	w12, w12                                                     // Reverse for big-endian increment
	uxtw	x10, w12                                                       // Zero extend reversed w12 into x10 for final counter update
    // Pre-compute this value instead of using two instructions to reconstruct it every iteration
	mov	x21, #0xc200000000000000                                // GHASH reduction constant
	str	x21, [sp, #128]
    // We maintain four copies of ctr values on the stack. Each loop iteration we
    // store the updated ctr value to the last four bytes (e.g., 160 + 12).
    // We then load the four values. This avoids a singificant number of
    // expensive GPR->NEON and NEON->NEON moves. To avoid LDST forwarding we
    // calculate and store the values one iteration ahead so they have time to
    // drain before we load them.
	str	q0,     [sp, #160]                                  // Store base counter for block 0-3
	str	q0,     [sp, #176]
	str	q0,     [sp, #192]
	str	q0,     [sp, #208]
    // Since we need the values right away don't go through the stack this first
    // time. Manually insert the incremented big-endian counter values.
	rev	w20, w12
	mov	v0.s[3], w20                                             // ctr0 + 0
	add	w20, w12, #1
	rev	w20, w20
	mov	v1.s[3], w20                                             // ctr0 + 1
	add	w20, w12, #2
	rev	w20, w20
	mov	v2.s[3], w20                                             // ctr0 + 2
	add	w20, w12, #3
	rev	w20, w20
	mov	v3.s[3], w20                                             // ctr0 + 3
    // Calculate the ctr values for the *next* (not current) group of four
    // blocks. Store the incremented parts to the stack.
	add	w20, w12, #4
	rev	w20, w20
	str	w20, [sp, #172]                             // ctr0 + 4 for next iter
	add	w20, w12, #5
	rev	w20, w20
	str	w20, [sp, #188]                             // ctr0 + 5 for next iter
	add	w20, w12, #6
	rev	w20, w20
	str	w20, [sp, #204]                             // ctr0 + 6 for next iter
	add	w20, w12, #7
	rev	w20, w20
	str	w20, [sp, #220]                             // ctr0 + 7 for next iter
	add	w12, w12, #8                                                 // Advance counter past these two sets
    // --- Start AES for first 4 blocks ---
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 0
	ldp	q19, q20, [x8, #16]                                           // load rk1, rk2
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 0
	ldp	q21, q22, [x8, #48]                                           // load rk3, rk4
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 0
	ldp	q23, q24, [x8, #80]                                           // load rk5, rk6
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 0
	ldp	q13, q14, [x6, #32]                                         // load H2, H3 (GHASH keys)
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 1
	ldp	q25, q26, [x8, #112]                                          // load rk7, rk8
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 1
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 2
	ext	v14.16b, v14.16b, v14.16b, #8                                      // Byte swap H3 for GHASH
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 2
	ext	v13.16b, v13.16b, v13.16b, #8                                      // Byte swap H2 for GHASH
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 2
	ldr	q15, [x6, #80]                                               // load H4
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 2
	ext	v15.16b, v15.16b, v15.16b, #8                                      // Byte swap H4 for GHASH
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 3
	ld1	{ v11.16b}, [x3]                                      // Load initial GHASH accumulator (T)
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 3
	ext	v11.16b, v11.16b, v11.16b, #8                             // Byte swap T for GHASH
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 3
	rev64	v11.16b, v11.16b                                           // Correct byte order within 64-bit lanes
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 3
	trn2	v17.2d,  v14.2d,    v15.2d                                      // Karatsuba key: H4_low | H3_low
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 4
	ldr	q12, [x6]                                                    // load H1
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 4
	ext	v12.16b, v12.16b, v12.16b, #8                                      // Byte swap H1 for GHASH
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 4
	trn1	v9.2d, v14.2d,    v15.2d                                      // Karatsuba key: H4_high | H3_high
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 4
	trn2	v16.2d,  v12.2d,    v13.2d                                      // Karatsuba key: H2_low | H1_low
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 5
	ldr	q30, [x7]                                                // Preload round N key for final EOR
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 5
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 5
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 5
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 6
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 6
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 6
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 6
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 7
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 7
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 7
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 7
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 8
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 8
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 8
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	b.lt	Lenc_finish_first_blocks                                         // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	b.eq	Lenc_finish_first_blocks                                         // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
Lenc_finish_first_blocks:
	cmp	x0, x5                                    // check if we have <= 4 blocks to process in the tail
	eor	v17.16b, v17.16b, v9.16b                                   // Karatsuba key: H3^H4
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	trn1	v8.2d,  v12.2d, v13.2d                                 // Karatsuba key: H2_high | H1_high
	eor	v16.16b, v16.16b, v8.16b                            // Karatsuba key: H1^H2
	b.ge	Lenc_tail                                                        // handle tail if no more full 4-block sets
	ldp	q6, q7, [x0, #32]                                  // AES blocks 2,3 load plaintext
	ldp	q4, q5, [x0], #64                                  // AES blocks 0,1 load plaintext
    // Compute and store first 4 ciphertext blocks
	eor	v4.16b, v4.16b, v30.16b
	eor	v4.16b, v4.16b, v0.16b                                // AES block 0 - result = PT ^ AES(ctr0)
	eor	v5.16b, v5.16b, v30.16b
	eor	v5.16b, v5.16b, v1.16b                                // AES block 1 - result = PT ^ AES(ctr1)
	eor	v6.16b, v6.16b, v30.16b
	eor	v6.16b, v6.16b, v2.16b                                // AES block 2 - result = PT ^ AES(ctr2)
	eor	v7.16b, v7.16b, v30.16b
	eor	v7.16b, v7.16b, v3.16b                                // AES block 3 - result = PT ^ AES(ctr3)
	st1	{ v4.16b, v5.16b, v6.16b, v7.16b}, [x2], #64  // AES blocks 0-3 - store result
    // Load counter values for the second iteration from the stack
	ldp	q0, q1, [sp, #160]
	ldp	q2, q3, [sp, #192]
    // Prepare and store counter values for the third iteration
	rev	w20, w12
	str	w20, [sp, #172]                             // ctr + 8
	add	w20, w12, #1
	rev	w20, w20
	str	w20, [sp, #188]                             // ctr + 9
	add	w20, w12, #2
	rev	w20, w20
	str	w20, [sp, #204]                             // ctr + 10
	add	w20, w12, #3
	rev	w20, w20
	str	w20, [sp, #220]                             // ctr + 11
	add	w12, w12, #4                                                 // Advance counter base
	cmp	x0, x5                                    // check if we have <= 4 blocks remaining
	b.ge	Lenc_prepretail                                                  // go to prepretail if < 2 full loops left
Lenc_main_loop:	//	main loop start (processes 4 blocks per iteration)
    // --- AES Pipeline for blocks 4k+4 to 4k+7 ---
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 0
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 0
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 0
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 0
	ldr	d8, [sp, #128]                      // Load GHASH reduction constant
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 1
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 1
    // --- GHASH Pipeline (interleaved with AES) for blocks 4k to 4k+3 ---
	rev64	v4.16b, v4.16b                                             // GHASH block 4k - Byte swap CT
	rev64	v5.16b, v5.16b                                             // GHASH block 4k+1 - Byte swap CT
	rev64	v6.16b, v6.16b                                             // GHASH block 4k+2 - Byte swap CT
	rev64	v7.16b, v7.16b                                             // GHASH block 4k+3 - Byte swap CT
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 2
	ext	v11.16b, v11.16b, v11.16b, #8                             // GHASH - prepare acc for XOR
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 2
	eor	v4.16b, v4.16b, v11.16b                                   // GHASH block 4k - Y_i = CT_i ^ Y_{i-1}
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 2
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 4k - low
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 2
	pmull2	v9.1q, v6.2d, v13.2d                                     // GHASH block 4k+2 - high
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 3
	mov	d10, v17.d[1]                                                // GHASH block 4k - mid Karatsuba key
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 3
	mov	d20, v4.d[1]                                             // GHASH block 4k - mid
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 3
	eor	v20.8b, v20.8b, v4.8b                               // GHASH block 4k - mid
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 3
	mov	d21, v5.d[1]                                             // GHASH block 4k+1 - mid
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 4
	eor	v21.8b, v21.8b, v5.8b                               // GHASH block 4k+1 - mid
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 4
	pmull	v10.1q, v20.1d, v10.1d                               // GHASH block 4k - mid
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 4
	pmull	v21.1q, v21.1d, v17.1d                             // GHASH block 4k+1 - mid
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 4
	eor	v10.16b, v10.16b, v21.16b                              // GHASH block 4k+1 - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 5
	ext	v22.16b, v22.16b, v6.16b, #8                        // GHASH block 4k+2 - mid
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 5
	eor	v22.16b, v22.16b, v6.16b                            // GHASH block 4k+2 - mid
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 5
	pmull2	v22.1q, v22.2d, v16.2d                            // GHASH block 4k+2 - mid
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 5
	mov	d23, v7.d[1]                                             // GHASH block 4k+3 - mid
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 6
	eor	v23.8b, v23.8b, v7.8b                               // GHASH block 4k+3 - mid
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 6
	pmull	v23.1q, v23.1d, v16.1d                             // GHASH block 4k+3 - mid
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 6
	eor	v10.16b, v10.16b, v22.16b
	eor	v10.16b, v10.16b, v23.16b                       // GHASH block 4k+2/3 - mid
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 6
	pmull2	v22.1q, v4.2d, v15.2d                                  // GHASH block 4k - high
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 7
	pmull2	v21.1q, v7.2d, v12.2d                                  // GHASH block 4k+3 - high
	eor	v22.16b, v22.16b, v21.16b                        // GHASH block 4k+3 - high
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 7
	pmull2	v23.1q, v5.2d, v14.2d                                  // GHASH block 4k+1 - high
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 7
	pmull	v21.1q, v6.1d, v13.1d                                   // GHASH block 4k+2 - low
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 7
	pmull	v20.1q, v5.1d, v14.1d                                   // GHASH block 4k+1 - low
	eor	v9.16b, v9.16b, v22.16b
	eor	v9.16b, v9.16b, v23.16b                       // GHASH block 4k/1/2/3 - high
	pmull	v22.1q, v7.1d, v12.1d                                   // GHASH block 4k+3 - low
	ldp	q6, q7, [x0, #32]
	ldp	q4, q5, [x0], #64
	eor	v20.16b, v20.16b, v21.16b                        // GHASH block 4k+1 - low
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 8
	eor	v11.16b, v11.16b, v22.16b
	eor	v11.16b, v11.16b, v20.16b                       // GHASH block 4k/1/2/3 - low
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 8
	eor	v10.16b, v10.16b, v9.16b
	eor	v10.16b, v10.16b, v11.16b                             // MODULO - karatsuba tidy up
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 8
	pmull	v20.1q, v9.1d, v8.1d                        // MODULO - top 64b align with mid
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	b.lt	Lenc_main_loop_continue                                          // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 10
	b.eq	Lenc_main_loop_continue                                          // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 12
Lenc_main_loop_continue:
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor	v10.16b, v10.16b, v20.16b
	eor	v10.16b, v10.16b, v9.16b                          // MODULO - fold into mid
	pmull	v9.1q, v10.1d, v8.1d                           // MODULO - mid 64b align with low
	ext	v20.16b, v10.16b, v10.16b, #8                          // MODULO - other mid alignment
	eor	v11.16b, v9.16b, v11.16b
	eor	v11.16b, v11.16b, v20.16b                          // MODULO - fold into low
	aese	v0.16b, v31.16b                                             // AES block 4k+4 - round N-1
	eor	v4.16b, v4.16b, v30.16b
	eor	v4.16b, v4.16b, v0.16b                                // AES block 4k+4 - result
	aese	v1.16b, v31.16b                                             // AES block 4k+5 - round N-1
	eor	v5.16b, v5.16b, v30.16b
	eor	v5.16b, v5.16b, v1.16b                                // AES block 4k+5 - result
	aese	v2.16b, v31.16b                                             // AES block 4k+6 - round N-1
	eor	v6.16b, v6.16b, v30.16b
	eor	v6.16b, v6.16b, v2.16b                                // AES block 4k+6 - result
	aese	v3.16b, v31.16b                                             // AES block 4k+7 - round N-1
	eor	v7.16b, v7.16b, v30.16b
	eor	v7.16b, v7.16b, v3.16b                                // AES block 4k+7 - result
	ldp	q0, q1, [sp, #160]
	ldp	q2, q3, [sp, #192]
    // We used these registers as temporaries above so reload the RKs.
	ldp	q20, q21, [x8, #32]                                           // load rk2, rk3
	ldp	q22, q23, [x8, #64]                                           // load rk4, rk5
	st1	{ v4.16b, v5.16b, v6.16b, v7.16b}, [x2], #64  // AES blocks 4k+4-7 - store result
	rev	w20, w12
	str	w20, [sp, #172]
	add	w20, w12, #1
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w12, #2
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w12, #3
	rev	w20, w20
	str	w20, [sp, #220]
	add	w12, w12, #4
	cmp	x0, x5                                    // LOOP CONTROL
	b.lt	Lenc_main_loop
Lenc_prepretail:	//	PREPRETAIL
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 0
	rev64	v6.16b, v6.16b                                             // GHASH block 2
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 0
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 0
	rev64	v4.16b, v4.16b                                             // GHASH block 0
	ext	v11.16b, v11.16b, v11.16b, #8                             // PRE 0
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 1
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 1
	eor	v4.16b, v4.16b, v11.16b                                   // PRE 1
	rev64	v5.16b, v5.16b                                             // GHASH block 1
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 2
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 0
	mov	d10, v17.d[1]                                                // GHASH block 0 - mid Karatsuba key
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 1
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 0 - low
	mov	d8, v4.d[1]                                         // GHASH block 0 - mid
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH block 0 - high
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 3
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 2
	eor	v8.8b, v8.8b, v4.8b                       // GHASH block 0 - mid
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 2
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 1
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 3
	pmull	v10.1q, v8.1d, v10.1d                           // GHASH block 0 - mid
	pmull2	v4.1q, v5.2d, v14.2d                                      // GHASH block 1 - high
	pmull	v8.1q, v5.1d, v14.1d                               // GHASH block 1 - low
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 2
	eor	v9.16b, v9.16b, v4.16b                                  // GHASH block 1 - high
	mov	d4, v5.d[1]                                                 // GHASH block 1 - mid
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 3
	eor	v11.16b, v11.16b, v8.16b                          // GHASH block 1 - low
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 3
	eor	v4.8b, v4.8b, v5.8b                                       // GHASH block 1 - mid
	mov	d8, v6.d[1]                                         // GHASH block 2 - mid
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 4
	rev64	v7.16b, v7.16b                                             // GHASH block 3
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 4
	pmull	v4.1q, v4.1d, v17.1d                                     // GHASH block 1 - mid
	eor	v8.8b, v8.8b, v6.8b                       // GHASH block 2 - mid
	pmull	v5.1q, v6.1d, v13.1d                                       // GHASH block 2 - low
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 5
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 4
	eor	v10.16b, v10.16b, v4.16b                                  // GHASH block 1 - mid
	pmull2	v4.1q, v6.2d, v13.2d                                      // GHASH block 2 - high
	eor	v11.16b, v11.16b, v5.16b                                  // GHASH block 2 - low
	ins	v8.d[1], v8.d[0]                             // GHASH block 2 - mid
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 5
	eor	v9.16b, v9.16b, v4.16b                                  // GHASH block 2 - high
	mov	d4, v7.d[1]                                                 // GHASH block 3 - mid
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 4
	pmull2	v8.1q, v8.2d, v16.2d                    // GHASH block 2 - mid
	eor	v4.8b, v4.8b, v7.8b                                       // GHASH block 3 - mid
	pmull2	v5.1q, v7.2d, v12.2d                                      // GHASH block 3 - high
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 5
	pmull	v4.1q, v4.1d, v16.1d                                     // GHASH block 3 - mid
	eor	v10.16b, v10.16b, v8.16b                          // GHASH block 2 - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 5
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 6
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 6
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 6
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 6
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 7
	eor	v9.16b, v9.16b, v5.16b                                  // GHASH block 3 - high
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 7
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 7
	ldr	d8, [sp, #128]
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 8
	eor	v10.16b, v10.16b, v4.16b                                  // GHASH block 3 - mid
	pmull	v6.1q, v7.1d, v12.1d                                       // GHASH block 3 - low
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 8
	eor	v11.16b, v11.16b, v6.16b                                  // GHASH block 3 - low
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 7
	eor	v10.16b, v10.16b, v9.16b                                 // karatsuba tidy up
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 8
	pmull	v4.1q, v9.1d, v8.1d
	ext	v9.16b, v9.16b, v9.16b, #8
	eor	v10.16b, v10.16b, v11.16b
	b.lt	Lenc_finish_prepretail                                           // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	b.eq	Lenc_finish_prepretail                                           // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
Lenc_finish_prepretail:
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	eor	v10.16b, v10.16b, v4.16b
	eor	v10.16b, v10.16b, v9.16b
	pmull	v4.1q, v10.1d, v8.1d
	ext	v10.16b, v10.16b, v10.16b, #8
	eor	v11.16b, v11.16b, v4.16b
	eor	v11.16b, v11.16b, v10.16b
Lenc_tail:	//	TAIL: Process remaining 0 to 3 blocks
	ext	v8.16b, v11.16b, v11.16b, #8                      // Save current GHASH state for partial tag feed-in
	sub	x5, x4, x0                    // main_end_input_ptr is number of bytes left to process
	ldp	x6, x7, [x0], #16                            // AES block 0 - load plaintext
	eor	x6, x6, x13                                       // AES block 0 - round N low
	eor	x7, x7, x14                                       // AES block 0 - round N high
	cmp	x5, #48
	fmov	d4, x6                                                 // AES block 0 - mov low
	fmov	v4.d[1], x7                                             // AES block 0 - mov high
	eor	v5.16b, v4.16b, v0.16b                                    // AES block 0 - result
	b.gt	Lenc_blocks_more_than_3
	cmp	x5, #32
	mov	v3.16b, v2.16b
	movi	v11.8b, #0
	movi	v9.8b, #0
	mov	v2.16b, v1.16b
	movi	v10.8b, #0
	b.gt	Lenc_blocks_more_than_2
	mov	v3.16b, v1.16b
	cmp	x5, #16
	b.gt	Lenc_blocks_more_than_1
	b	Lenc_blocks_less_than_1
Lenc_blocks_more_than_3:	//	blocks left >  3
	st1	{ v5.16b}, [x2], #16                                   // AES final-2 block - store result
	ldp	x6, x7, [x0], #16                            // AES final-2 block - load input low & high
	rev64	v4.16b, v5.16b                                             // GHASH final-3 block
	eor	x6, x6, x13                                       // AES final-2 block - round N low
	eor	v4.16b, v4.16b, v8.16b                            // feed in partial tag
	eor	x7, x7, x14                                       // AES final-2 block - round N high
	mov	d22, v4.d[1]                                             // GHASH final-3 block - mid
	fmov	d5, x6                                                 // AES final-2 block - mov low
	fmov	v5.d[1], x7                                             // AES final-2 block - mov high
	eor	v22.8b, v22.8b, v4.8b                               // GHASH final-3 block - mid
	movi	v8.8b, #0                                              // suppress further partial tag feed in
	mov	d10, v17.d[1]                                                // GHASH final-3 block - mid
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH final-3 block - low
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH final-3 block - high
	pmull	v10.1q, v22.1d, v10.1d                               // GHASH final-3 block - mid
	eor	v5.16b, v5.16b, v1.16b                                    // AES final-2 block - result
Lenc_blocks_more_than_2:	//	blocks left >  2
	st1	{ v5.16b}, [x2], #16                                   // AES final-2 block - store result
	ldp	x6, x7, [x0], #16                            // AES final-1 block - load input low & high
	rev64	v4.16b, v5.16b                                             // GHASH final-2 block
	eor	x6, x6, x13                                       // AES final-1 block - round N low
	eor	v4.16b, v4.16b, v8.16b                            // feed in partial tag
	fmov	d5, x6                                                 // AES final-1 block - mov low
	eor	x7, x7, x14                                       // AES final-1 block - round N high
	fmov	v5.d[1], x7                                             // AES final-1 block - mov high
	movi	v8.8b, #0                                              // suppress further partial tag feed in
	pmull2	v20.1q, v4.2d, v14.2d                                  // GHASH final-2 block - high
	mov	d22, v4.d[1]                                             // GHASH final-2 block - mid
	pmull	v21.1q, v4.1d, v14.1d                                   // GHASH final-2 block - low
	eor	v22.8b, v22.8b, v4.8b                               // GHASH final-2 block - mid
	eor	v5.16b, v5.16b, v2.16b                                    // AES final-1 block - result
	eor	v9.16b, v9.16b, v20.16b                              // GHASH final-2 block - high
	pmull	v22.1q, v22.1d, v17.1d                             // GHASH final-2 block - mid
	eor	v11.16b, v11.16b, v21.16b                              // GHASH final-2 block - low
	eor	v10.16b, v10.16b, v22.16b                              // GHASH final-2 block - mid
Lenc_blocks_more_than_1:	//	blocks left >  1
	st1	{ v5.16b}, [x2], #16                                   // AES final-1 block - store result
	rev64	v4.16b, v5.16b                                             // GHASH final-1 block: Byte Swap CT
	ldp	x6, x7, [x0], #16                            // AES final block - load plaintext
	eor	v4.16b, v4.16b, v8.16b                            // Feed in partial tag
	movi	v8.8b, #0                                              // Clear for next block
	eor	x6, x6, x13                                       // AES final block - round N low
	mov	d22, v4.d[1]                                             // GHASH final-1 block - mid
	pmull2	v20.1q, v4.2d, v13.2d                                  // GHASH final-1 block - high
	eor	x7, x7, x14                                       // AES final block - round N high
	eor	v22.8b, v22.8b, v4.8b                               // GHASH final-1 block - mid
	eor	v9.16b, v9.16b, v20.16b                              // GHASH final-1 block - high
	ins	v22.d[1], v22.d[0]                                     // GHASH final-1 block - mid
	fmov	d5, x6                                                 // AES final block - mov low
	fmov	v5.d[1], x7                                             // AES final block - mov high
	pmull2	v22.1q, v22.2d, v16.2d                            // GHASH final-1 block - mid
	pmull	v21.1q, v4.1d, v13.1d                                   // GHASH final-1 block - low
	eor	v5.16b, v5.16b, v3.16b                                    // AES final block - result
	eor	v10.16b, v10.16b, v22.16b                              // GHASH final-1 block - mid
	eor	v11.16b, v11.16b, v21.16b                              // GHASH final-1 block - low
Lenc_blocks_less_than_1:	//	Last partial block handling
	add	x10, x10, x1, lsr #7                                    // Calculate the updated counter based on the number of 16B chunks we processed
	rev	w10, w10
	str	w10, [x16, #12]                                              // store the updated counter
	and	x1, x1, #127                                     // bit_length %= 128
	mvn	x13, xzr                                                        // Mask for low 64 bits
	sub	x1, x1, #128                                     //
	neg	x1, x1                                           // Valid bits in the last block (1-128)
	ldr	q18, [x2]                                  // Load destination for merging
	mvn	x14, xzr                                                        // Mask for high 64 bits
	and	x1, x1, #127                                     // bit_length %= 128
	lsr	x14, x14, x1                                        // rkN_h is mask for top 64b of last block
	cmp	x1, #64
	csel	x6, x13, x14, lt
	csel	x7, x14, xzr, lt
	fmov	d0, x6                                                 // ctr0d is mask for last block
	fmov	v0.d[1], x7
	and	v5.16b, v5.16b, v0.16b                                    // Mask out unused bits of the last CT block
	rev64	v4.16b, v5.16b                                             // GHASH final block - byte swap
	eor	v4.16b, v4.16b, v8.16b                            // Feed in partial tag
	bif	v5.16b, v18.16b, v0.16b                        // Bitwise Insert: merge with existing data at output_ptr
	pmull2	v20.1q, v4.2d, v12.2d                                  // GHASH final block - high
	mov	d8, v4.d[1]                                         // GHASH final block - mid
	pmull	v21.1q, v4.1d, v12.1d                                   // GHASH final block - low
	eor	v9.16b, v9.16b, v20.16b                              // GHASH final block - high
	eor	v8.8b, v8.8b, v4.8b                       // GHASH final block - mid
	pmull	v8.1q, v8.1d, v16.1d                     // GHASH final block - mid
	eor	v11.16b, v11.16b, v21.16b                              // GHASH final block - low
	eor	v10.16b, v10.16b, v8.16b                          // GHASH final block - mid
	eor	v4.16b, v11.16b, v9.16b                                  // MODULO - karatsuba tidy up
	fmov	d8, x21
	eor	v10.16b, v10.16b, v4.16b                                  // MODULO - karatsuba tidy up
	pmull	v7.1q, v9.1d, v8.1d                            // MODULO - top 64b align with mid
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor	v10.16b, v10.16b, v7.16b
	eor	v10.16b, v10.16b, v9.16b                              // MODULO - fold into mid
	pmull	v9.1q, v10.1d, v8.1d                           // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	st1	{ v5.16b}, [x2]                                        // store all 16B
	eor	v11.16b, v11.16b, v9.16b
	eor	v11.16b, v11.16b, v10.16b                             // MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8                             // Byte swap GHASH result
	rev64	v11.16b, v11.16b                                           // Final Tag
	mov	x0, x15
	st1	{ v11.16b }, [x3]                                     // Store final tag
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	d8, d9, [sp, #64]
	ldp	d10, d11, [sp, #80]
	ldp	d12, d13, [sp, #96]
	ldp	d14, d15, [sp, #112]
	ldp	x29, x30, [sp], #224
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.globl	_aes_gcm_dec_kernel
.private_extern	_aes_gcm_dec_kernel

.align	4
_aes_gcm_dec_kernel:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29, x30, [sp, #-224]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	ld1	{ v0.16b}, [x4]
	mov	v1.16b, v0.16b
	mov	v2.16b, v0.16b
	mov	v3.16b, v0.16b
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]
	stp	d8, d9, [sp, #64]
	stp	d10, d11, [sp, #80]
	stp	d12, d13, [sp, #96]
	stp	d14, d15, [sp, #112]
	ldr	w17, [x8, #240]                                             // Load number of AES rounds
	add	x19, x8, x17, lsl #4                                    // borrow input_l1 for last key
	ldp	x13, x14, [x19]                                        // load round N keys
	ldr	q31, [x19, #-16]                                         // load round N-1 keys
	add	x4, x0, x1, lsr #3                    // end_input_ptr
	lsr	x5, x1, #3                               // byte_len
	mov	x15, x5
	ldr	w9, [x16, #12]                                           // Load scalar 32-bit counter (CTR)
	sub	x5, x5, #1                       // byte_len - 1
	ldr	q18, [x8, #0]                                                   // load rk0
	and	x5, x5, #0xffffffffffffffc0      // number of bytes to be processed in main loop (at least 1 byte must be handled by tail)
	add	x5, x5, x0
	rev	w9, w9                                                   // Reverse it once for big-endian incrementing
	uxtw	x10, w9                                                      // Zero extend reversed w9 into x10
	str	q0,     [sp, #160]
	str	q0,     [sp, #176]
	str	q0,     [sp, #192]
	str	q0,     [sp, #208]
	rev	w20, w9
	mov	v0.s[3], w20
	add	w20, w9, #1
	rev	w20, w20
	mov	v1.s[3], w20
	add	w20, w9, #2
	rev	w20, w20
	mov	v2.s[3], w20
	add	w20, w9, #3
	rev	w20, w20
	mov	v3.s[3], w20
	add	w20, w9, #4
	rev	w20, w20
	str	w20, [sp, #172]
	add	w20, w9, #5
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w9, #6
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w9, #7
	rev	w20, w20
	str	w20, [sp, #220]
	add	w9, w9, #8
    // Pre-compute this value instead of using two instructions for moving and then shifting in the main loop
	mov	x21, #0xc200000000000000
	str	x21, [sp, #128]
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 0
	ldp	q19, q20, [x8, #16]                                           // load rk1, rk2
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 0
	ldp	q21, q22, [x8, #48]                                           // load rk3, rk4
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 0
	ldp	q23, q24, [x8, #80]                                           // load rk5, rk6
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 0
	ldp	q13, q14, [x6, #32]                                         // load h2, h3
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 1
	ldp	q25, q26, [x8, #112]                                          // load rk7, rk8
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 1
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 2
	ext	v14.16b, v14.16b, v14.16b, #8
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 2
	ext	v13.16b, v13.16b, v13.16b, #8
	ldr	q15, [x6, #80]                                               // load h4
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 2
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 2
	ext	v15.16b, v15.16b, v15.16b, #8
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 3
	ld1	{ v11.16b}, [x3]
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 3
	ext	v11.16b, v11.16b, v11.16b, #8
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 3
	rev64	v11.16b, v11.16b
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 3
	trn2	v17.2d,  v14.2d,    v15.2d                                      // h4l | h3l
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 4
	ldr	q12, [x6]                                                    // load h1
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 4
	ext	v12.16b, v12.16b, v12.16b, #8
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 4
	trn1	v9.2d, v14.2d,    v15.2d                                      // h4h | h3h
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 4
	trn2	v16.2d,  v12.2d,    v13.2d                                      // h2l | h1l
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 5
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 5
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 5
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 5
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 6
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 6
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 6
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 6
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 7
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 7
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 7
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 7
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 8
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 8
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 8
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	b.lt	Ldec_finish_first_blocks                                         // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	b.eq	Ldec_finish_first_blocks                                         // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
Ldec_finish_first_blocks:
	ldr	q27, [x19]                                          // load rkN
	cmp	x0, x5                                    // check if we have <= 4 blocks
	eor	v17.16b, v17.16b, v9.16b                                   // h4k | h3k
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	trn1	v8.2d,    v12.2d,    v13.2d                                      // h2h | h1h
	eor	v16.16b, v16.16b, v8.16b                                      // h2k | h1k
	b.ge	Ldec_tail                                                        // handle tail
    // Setup for AES blocks 0-3 is done purely on NEON side instead of mixing NEON and scalar instructions.
    // This is because the final result of the AES block needs to be EORd with the final round key
    // value (v30). This avoids several fmovs.
	ldp	q6, q7, [x0, #32]                                  // AES blocks 2,3 load ciphertext
	ldp	q4, q5, [x0], #64                                  // AES blocks 0,1 load ciphertext
	eor	v0.16b, v4.16b, v0.16b
	eor	v0.16b, v0.16b, v27.16b                           // AES block 0 - result
	eor	v1.16b, v5.16b, v1.16b
	eor	v1.16b, v1.16b, v27.16b                           // AES block 1 - result
	eor	v2.16b, v6.16b, v2.16b
	eor	v2.16b, v2.16b, v27.16b                           // AES block 2 - result
	eor	v3.16b, v7.16b, v3.16b
	eor	v3.16b, v3.16b, v27.16b                           // AES block 3 - result
	st1	{ v0.16b, v1.16b, v2.16b, v3.16b}, [x2], #64  // AES blocks 0-3 - store result
	ldr	q0,     [sp, #160]
	ldr	q1,     [sp, #176]
	ldr	q2,     [sp, #192]
	ldr	q3,     [sp, #208]
	rev	w20, w9
	str	w20, [sp, #172]
	add	w20, w9, #1
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w9, #2
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w9, #3
	rev	w20, w20
	str	w20, [sp, #220]
	add	w9, w9, #4
	cmp	x0, x5                                    // check if we have <= 4 blocks
	b.ge	Ldec_prepretail                                                  // do prepretail
Ldec_main_loop:	//	main loop start
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 0
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 0
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 0
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 0
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 1
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 1
	rev64	v4.16b, v4.16b                                             // GHASH block 4k
	rev64	v5.16b, v5.16b                                             // GHASH block 4k+1
	rev64	v6.16b, v6.16b                                             // GHASH block 4k+2
	rev64	v7.16b, v7.16b                                             // GHASH block 4k+3
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 2
	ext	v11.16b, v11.16b, v11.16b, #8                             // PRE 0
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 2
	eor	v4.16b, v4.16b, v11.16b                                   // PRE 1
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 2
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 4k - low
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 2
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH block 4k - high
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 3
	mov	d10, v17.d[1]                                                // GHASH block 4k - mid
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 3
	mov	d8, v4.d[1]                                                   // GHASH block 4k - mid
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 3
	eor	v8.8b, v8.8b, v4.8b                                           // GHASH block 4k - mid
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 3
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 4
	pmull2	v4.1q, v5.2d, v14.2d                                        // GHASH block 4k+1 - high
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 4
	eor	v9.16b, v9.16b, v4.16b                                    // GHASH block 4k+1 - high
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 4
	mov	d4, v5.d[1]                                                   // GHASH block 4k+1 - mid
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 4
	pmull	v10.1q, v8.1d, v10.1d                                     // GHASH block 4k - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 5
	pmull	v8.1q, v5.1d, v14.1d                                         // GHASH block 4k+1 - low
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 5
	eor	v4.8b, v4.8b, v5.8b                                           // GHASH block 4k+1 - mid
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 5
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 5
	pmull	v5.1q, v6.1d, v13.1d                                         // GHASH block 4k+2 - low
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 6
	eor	v11.16b, v11.16b, v8.16b
	eor	v11.16b, v11.16b, v5.16b                                   // GHASH block 4k+1 - low & GHASH block 4k+2 - low
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 6
	mov	d8, v6.d[1]                                                   // GHASH block 4k+2 - mid
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 6
	eor	v8.8b, v8.8b, v6.8b                                           // GHASH block 4k+2 - mid
	pmull	v4.1q, v4.1d, v17.1d                                         // GHASH block 4k+1 - mid
	ins	v8.d[1], v8.d[0]                                                 // GHASH block 4k+2 - mid
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 6
	pmull2	v28.1q, v6.2d, v13.2d                                   // GHASH block 4k+2 - high
	mov	d6, v7.d[1]                                                   // GHASH block 4k+3 - mid
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 7
	pmull2	v8.1q, v8.2d, v16.2d                                        // GHASH block 4k+2 - mid
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 7
	pmull	v27.1q, v7.1d, v12.1d                                 // GHASH block 4k+3 - low
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 7
	eor	v10.16b, v10.16b, v4.16b
	eor	v10.16b, v10.16b, v8.16b                                   // GHASH block 4k+1 - mid & GHASH block 4k+2 - mid
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 7
	pmull2	v5.1q, v7.2d, v12.2d                                        // GHASH block 4k+3 - high
	eor	v6.8b, v6.8b, v7.8b                                           // GHASH block 4k+3 - mid
	eor	v9.16b, v9.16b, v28.16b
	eor	v9.16b, v9.16b, v5.16b                              // GHASH block 4k+2 - high & GHASH block 4k+3 - high
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 8
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 8
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 8
	pmull	v6.1q, v6.1d, v16.1d                                         // GHASH block 4k+3 - mid
	ldr	d8, [sp, #128]
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 8
	pmull	v7.1q, v9.1d, v8.1d                           // MODULO - top 64b align with mid
	eor	v10.16b, v10.16b, v6.16b
	eor	v10.16b, v10.16b, v7.16b                                // GHASH block 4k+3 - mid & MODULO - fold into mid
	eor	v11.16b, v11.16b, v27.16b                            // GHASH block 4k+3 - low
	eor	v6.16b, v11.16b, v9.16b                                    // MODULO - karatsuba tidy up
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor	v10.16b, v10.16b, v6.16b
	eor	v10.16b, v10.16b, v9.16b                                // MODULO - karatsuba tidy up & MODULO - fold into mid
	pmull	v8.1q, v10.1d, v8.1d                    // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	eor	v11.16b, v11.16b, v8.16b
	eor	v11.16b, v11.16b, v10.16b                      // MODULO - fold into low
	cmp	w17, #12                                                     // setup flags for AES-128/192/256 check
	b.lt	Ldec_main_loop_continue                                          // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 10
	b.eq	Ldec_main_loop_continue                                          // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 12
Ldec_main_loop_continue:
	ldr	q27, [x19]                                          // load rkN
	ldp	q6, q7, [x0, #32]                                  // AES blocks 2,3 load ciphertext
	ldp	q4, q5, [x0], #64                                  // AES blocks 0,1 load ciphertext
	aese	v0.16b, v31.16b                                             // AES block 4k+4 - round N-1
	eor	v0.16b, v4.16b, v0.16b
	eor	v0.16b, v0.16b, v27.16b                           // AES block 4k+4 - result
	aese	v1.16b, v31.16b                                             // AES block 4k+5 - round N-1
	eor	v1.16b, v5.16b, v1.16b
	eor	v1.16b, v1.16b, v27.16b                           // AES block 4k+5 - result
	aese	v2.16b, v31.16b                                             // AES block 4k+6 - round N-1
	eor	v2.16b, v6.16b, v2.16b
	eor	v2.16b, v2.16b, v27.16b                           // AES block 4k+6 - result
	aese	v3.16b, v31.16b                                             // AES block 4k+7 - round N-1
	eor	v3.16b, v7.16b, v3.16b
	eor	v3.16b, v3.16b, v27.16b                           // AES block 4k+7 - result
	st1	{ v0.16b, v1.16b, v2.16b, v3.16b}, [x2], #64  // AES blocks 4k+4-7 - store result
	ldr	q0,     [sp, #160]
	ldr	q1,     [sp, #176]
	ldr	q2,     [sp, #192]
	ldr	q3,     [sp, #208]
	rev	w20, w9
	str	w20, [sp, #172]
	add	w20, w9, #1
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w9, #2
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w9, #3
	rev	w20, w20
	str	w20, [sp, #220]
	add	w9, w9, #4
	cmp	x0, x5
	b.lt	Ldec_main_loop
Ldec_prepretail:	//	PREPRETAIL
	rev64	v4.16b, v4.16b                                             // GHASH block 0
	rev64	v5.16b, v5.16b                                             // GHASH block 1
	ext	v11.16b, v11.16b, v11.16b, #8                             // PRE 0
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 0
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 0
	eor	v4.16b, v4.16b, v11.16b                                   // PRE 1
	rev64	v6.16b, v6.16b                                             // GHASH block 2
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 1
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 0 - low
	mov	d8, v4.d[1]                                                   // GHASH block 0 - mid
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH block 0 - high
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 0
	mov	d10, v17.d[1]                                                // GHASH block 0 - mid
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 1
	eor	v8.8b, v8.8b, v4.8b                                           // GHASH block 0 - mid
	pmull2	v4.1q, v5.2d, v14.2d                                        // GHASH block 1 - high
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 1
	rev64	v7.16b, v7.16b                                             // GHASH block 3
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 0
	pmull	v10.1q, v8.1d, v10.1d                                     // GHASH block 0 - mid
	eor	v9.16b, v9.16b, v4.16b                                    // GHASH block 1 - high
	pmull	v8.1q, v5.1d, v14.1d                                         // GHASH block 1 - low
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 1
	mov	d4, v5.d[1]                                                   // GHASH block 1 - mid
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 2
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 2
	eor	v11.16b, v11.16b, v8.16b                                    // GHASH block 1 - low
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 2
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 3
	mov	d8, v6.d[1]                                                   // GHASH block 2 - mid
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 2
	eor	v4.8b, v4.8b, v5.8b                                           // GHASH block 1 - mid
	pmull	v5.1q, v6.1d, v13.1d                                         // GHASH block 2 - low
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 4
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 3
	eor	v8.8b, v8.8b, v6.8b                                           // GHASH block 2 - mid
	pmull	v4.1q, v4.1d, v17.1d                                         // GHASH block 1 - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 5
	eor	v11.16b, v11.16b, v5.16b                                    // GHASH block 2 - low
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 4
	pmull2	v5.1q, v7.2d, v12.2d                                        // GHASH block 3 - high
	eor	v10.16b, v10.16b, v4.16b                                    // GHASH block 1 - mid
	pmull2	v4.1q, v6.2d, v13.2d                                        // GHASH block 2 - high
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 5
	ins	v8.d[1], v8.d[0]                                                 // GHASH block 2 - mid
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 3
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 3
	eor	v9.16b, v9.16b, v4.16b
	eor	v9.16b, v9.16b, v5.16b                                   // GHASH block 2 - high & GHASH block 3 - high
	pmull	v4.1q, v7.1d, v12.1d                                         // GHASH block 3 - low
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 4
	mov	d6, v7.d[1]                                                   // GHASH block 3 - mid
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 4
	pmull2	v8.1q, v8.2d, v16.2d                                        // GHASH block 2 - mid
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 5
	eor	v6.8b, v6.8b, v7.8b                                           // GHASH block 3 - mid
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 5
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 6
	eor	v10.16b, v10.16b, v8.16b                                    // GHASH block 2 - mid
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 6
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 6
	movi	v8.8b, #0xc2
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 6
	eor	v11.16b, v11.16b, v4.16b                                    // GHASH block 3 - low
	pmull	v6.1q, v6.1d, v16.1d                                         // GHASH block 3 - mid
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 7
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 7
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 7
	eor	v10.16b, v10.16b, v6.16b                                    // GHASH block 3 - mid
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 8
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 7
	eor	v6.16b, v11.16b, v9.16b                                    // MODULO - karatsuba tidy up
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 8
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 8
	shl	d8, d8, #56                                // mod_constant
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 8
	cmp	w17, #12                                                     // setup flags for AES-128/192/256 check
	b.lt	Ldec_finish_prepretail                                           // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	b.eq	Ldec_finish_prepretail                                           // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
Ldec_finish_prepretail:
	eor	v10.16b, v10.16b, v6.16b                                    // MODULO - karatsuba tidy up
	pmull	v7.1q, v9.1d, v8.1d                           // MODULO - top 64b align with mid
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor	v10.16b, v10.16b, v7.16b
	eor	v10.16b, v10.16b, v9.16b                             // MODULO - fold into mid
	pmull	v8.1q, v10.1d, v8.1d                    // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	eor	v11.16b, v11.16b, v8.16b
	eor	v11.16b, v11.16b, v10.16b                      // MODULO - fold into low
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
Ldec_tail:	//	TAIL
	sub	x5, x4, x0                    // main_end_input_ptr is number of bytes left to process
	ld1	{ v5.16b}, [x0], #16                                    // AES block 0 - load ciphertext
	eor	v0.16b, v5.16b, v0.16b                                    // AES block 0 - result
	mov	x6, v0.d[0]                                             // AES block 0 - mov low
	mov	x7, v0.d[1]                                             // AES block 0 - mov high
	ext	v8.16b, v11.16b, v11.16b, #8                                // prepare final partial tag
	eor	x6, x6, x13                                     // AES block 0 - round N low
	eor	x7, x7, x14                                     // AES block 0 - round N high
	cmp	x5, #48
	b.gt	Ldec_blocks_more_than_3
	mov	v3.16b, v2.16b
	movi	v10.8b, #0
	movi	v11.8b, #0
	movi	v9.8b, #0
	mov	v2.16b, v1.16b
	cmp	x5, #32
	b.gt	Ldec_blocks_more_than_2
	mov	v3.16b, v1.16b
	cmp	x5, #16
	b.gt	Ldec_blocks_more_than_1
	b	Ldec_blocks_less_than_1
Ldec_blocks_more_than_3:	//	blocks left >  3
	rev64	v4.16b, v5.16b                                             // GHASH final-3 block
	ld1	{ v5.16b}, [x0], #16                                    // AES final-2 block - load ciphertext
	stp	x6, x7, [x2], #16                         // AES final-3 block  - store result
	mov	d10, v17.d[1]                                                // GHASH final-3 block - mid
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	eor	v0.16b, v5.16b, v1.16b                                    // AES final-2 block - result
	mov	d22, v4.d[1]                                                  // GHASH final-3 block - mid
	mov	x6, v0.d[0]                                             // AES final-2 block - mov low
	mov	x7, v0.d[1]                                             // AES final-2 block - mov high
	eor	v22.8b, v22.8b, v4.8b                                       // GHASH final-3 block - mid
	movi	v8.8b, #0                                                        // suppress further partial tag feed in
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH final-3 block - high
	pmull	v10.1q, v22.1d, v10.1d                                   // GHASH final-3 block - mid
	eor	x6, x6, x13                                     // AES final-2 block - round N low
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH final-3 block - low
	eor	x7, x7, x14                                     // AES final-2 block - round N high
Ldec_blocks_more_than_2:	//	blocks left >  2
	rev64	v4.16b, v5.16b                                             // GHASH final-2 block
	ld1	{ v5.16b}, [x0], #16                                    // AES final-1 block - load ciphertext
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	stp	x6, x7, [x2], #16                         // AES final-2 block  - store result
	eor	v0.16b, v5.16b, v2.16b                                    // AES final-1 block - result
	mov	d22, v4.d[1]                                                  // GHASH final-2 block - mid
	pmull	v21.1q, v4.1d, v14.1d                                         // GHASH final-2 block - low
	pmull2	v20.1q, v4.2d, v14.2d                                       // GHASH final-2 block - high
	eor	v22.8b, v22.8b, v4.8b                                       // GHASH final-2 block - mid
	mov	x6, v0.d[0]                                             // AES final-1 block - mov low
	mov	x7, v0.d[1]                                             // AES final-1 block - mov high
	eor	v11.16b, v11.16b, v21.16b                                   // GHASH final-2 block - low
	movi	v8.8b, #0                                                        // suppress further partial tag feed in
	pmull	v22.1q, v22.1d, v17.1d                                     // GHASH final-2 block - mid
	eor	v9.16b, v9.16b, v20.16b                                   // GHASH final-2 block - high
	eor	x6, x6, x13                                     // AES final-1 block - round N low
	eor	v10.16b, v10.16b, v22.16b                                  // GHASH final-2 block - mid
	eor	x7, x7, x14                                     // AES final-1 block - round N high
Ldec_blocks_more_than_1:	//	blocks left >  1
	stp	x6, x7, [x2], #16                         // AES final-1 block  - store result
	rev64	v4.16b, v5.16b                                             // GHASH final-1 block
	ld1	{ v5.16b}, [x0], #16                                    // AES final block - load ciphertext
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	movi	v8.8b, #0                                                        // suppress further partial tag feed in
	mov	d22, v4.d[1]                                                  // GHASH final-1 block - mid
	eor	v0.16b, v5.16b, v3.16b                                    // AES final block - result
	pmull2	v20.1q, v4.2d, v13.2d                                        // GHASH final-1 block - high
	eor	v22.8b, v22.8b, v4.8b                                       // GHASH final-1 block - mid
	pmull	v21.1q, v4.1d, v13.1d                                         // GHASH final-1 block - low
	mov	x6, v0.d[0]                                             // AES final block - mov low
	ins	v22.d[1], v22.d[0]                                             // GHASH final-1 block - mid
	mov	x7, v0.d[1]                                             // AES final block - mov high
	pmull2	v22.1q, v22.2d, v16.2d                                    // GHASH final-1 block - mid
	eor	x6, x6, x13                                     // AES final block - round N low
	eor	v11.16b, v11.16b, v21.16b                                   // GHASH final-1 block - low
	eor	v9.16b, v9.16b, v20.16b                                   // GHASH final-1 block - high
	eor	v10.16b, v10.16b, v22.16b                                  // GHASH final-1 block - mid
	eor	x7, x7, x14                                     // AES final block - round N high
Ldec_blocks_less_than_1:	//	blocks left <= 1
	add	x10, x10, x1, lsr #7                                    // Calculate the updated counter based on the number of 16B chunks we processed
	rev	w10, w10
	str	w10, [x16, #12]                                              // store the updated counter
	and	x1, x1, #127                                     // bit_length %= 128
	mvn	x14, xzr                                                        // rkN_h = 0xffffffffffffffff
	sub	x1, x1, #128                                     // bit_length -= 128
	mvn	x13, xzr                                                        // rkN_l = 0xffffffffffffffff
	ldp	x4, x5, [x2]                 // load existing bytes we need to not overwrite
	neg	x1, x1                                           // bit_length = 128 - #bits in input (in range [1,128])
	and	x1, x1, #127                                     // bit_length %= 128
	lsr	x14, x14, x1                                        // rkN_h is mask for top 64b of last block
	cmp	x1, #64
	csel	x9, x13, x14, lt
	csel	x10, x14, xzr, lt
	fmov	d0, x9                                                   // ctr0b is mask for last block
	and	x6, x6, x9
	mov	v0.d[1], x10
	bic	x4, x4, x9                            // mask out low existing bytes
	bic	x5, x5, x10              // mask out high existing bytes
	orr	x6, x6, x4
	and	x7, x7, x10
	orr	x7, x7, x5
	and	v5.16b, v5.16b, v0.16b                                    // possibly partial last block has zeroes in highest bits
	rev64	v4.16b, v5.16b                                             // GHASH final block
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	pmull	v21.1q, v4.1d, v12.1d                                         // GHASH final block - low
	mov	d8, v4.d[1]                                                   // GHASH final block - mid
	eor	v8.8b, v8.8b, v4.8b                                           // GHASH final block - mid
	pmull2	v20.1q, v4.2d, v12.2d                                        // GHASH final block - high
	pmull	v8.1q, v8.1d, v16.1d                                         // GHASH final block - mid
	eor	v9.16b, v9.16b, v20.16b                                   // GHASH final block - high
	eor	v11.16b, v11.16b, v21.16b                                   // GHASH final block - low
	eor	v10.16b, v10.16b, v8.16b                                    // GHASH final block - mid
	ldr	d8, [sp, #128]
	eor	v6.16b, v11.16b, v9.16b                                    // MODULO - karatsuba tidy up
	eor	v10.16b, v10.16b, v6.16b                                    // MODULO - karatsuba tidy up
	pmull	v7.1q, v9.1d, v8.1d                           // MODULO - top 64b align with mid
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor	v10.16b, v10.16b, v7.16b                                 // MODULO - fold into mid
	eor	v10.16b, v10.16b, v9.16b                                 // MODULO - fold into mid
	pmull	v8.1q, v10.1d, v8.1d                    // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	eor	v11.16b, v11.16b, v8.16b                          // MODULO - fold into low
	stp	x6, x7, [x2]
	eor	v11.16b, v11.16b, v10.16b                                 // MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b                                           // Final Tag
	mov	x0, x15
	st1	{ v11.16b }, [x3]                                     // Store final tag
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	d8, d9, [sp, #64]
	ldp	d10, d11, [sp, #80]
	ldp	d12, d13, [sp, #96]
	ldp	d14, d15, [sp, #112]
	ldp	x29, x30, [sp], #224
	AARCH64_VALIDATE_LINK_REGISTER
	ret


.globl	_aes_gcm_enc_kernel_eor3
.private_extern	_aes_gcm_enc_kernel_eor3

.align	4
_aes_gcm_enc_kernel_eor3:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29, x30, [sp, #-224]!
	mov	x29, sp
	ld1	{ v0.16b}, [x4]                                                 // Load initial counter block
	stp	x19, x20, [sp, #16]
	mov	v1.16b, v0.16b                                               // Initialize ctr1-3 from ctr0
	mov	v2.16b, v0.16b
	mov	v3.16b, v0.16b
	mov	x16, x4                                                       // Pointer to counter block in memory
	mov	x8, x5                                                            // Pointer to AES key schedule context
	stp	x21, x22, [sp, #32]
    // [sp, #48] is unused but allocated to align the stack layout with aes_gcm_dec_kernel_eor3
	stp	d8, d9, [sp, #64]                                                  // Save Neon registers
	stp	d10, d11, [sp, #80]
	stp	d12, d13, [sp, #96]
	stp	d14, d15, [sp, #112]
	ldr	w17, [x8, #240]                                             // Load number of AES rounds
	add	x7, x8, x17, lsl #4                                     // Calculate pointer to the last round key
	ldp	x13, x14, [x7]                                         // load round N key (for final XOR)
	ldr	q31, [x7, #-16]                                          // load round N-1 key
	add	x4, x0, x1, lsr #3                    // Calculate end of input
	lsr	x5, x1, #3                               // Total byte length
	mov	x15, x5
	ldr	w12, [x16, #12]                                            // Load counter's low 32 bits
	sub	x5, x5, #1                       // byte_len - 1
	ldr	q18, [x8, #0]                                                   // load rk0
	and	x5, x5, #0xffffffffffffffc0      // Align main loop end to a multiple of 64 bytes
	add	x5, x5, x0
	rev	w12, w12                                                     // Reverse for big-endian increment
	uxtw	x10, w12                                                       // Zero extend reversed w12 into x10 for final counter update
    // Pre-compute this value instead of using two instructions to reconstruct it every iteration
	mov	x21, #0xc200000000000000                                // GHASH reduction constant
	str	x21, [sp, #128]
    // We maintain four copies of ctr values on the stack. Each loop iteration we
    // store the updated ctr value to the last four bytes (e.g., 160 + 12).
    // We then load the four values. This avoids a singificant number of
    // expensive GPR->NEON and NEON->NEON moves. To avoid LDST forwarding we
    // calculate and store the values one iteration ahead so they have time to
    // drain before we load them.
	str	q0,     [sp, #160]                                  // Store base counter for block 0-3
	str	q0,     [sp, #176]
	str	q0,     [sp, #192]
	str	q0,     [sp, #208]
    // Since we need the values right away don't go through the stack this first
    // time. Manually insert the incremented big-endian counter values.
	rev	w20, w12
	mov	v0.s[3], w20                                             // ctr0 + 0
	add	w20, w12, #1
	rev	w20, w20
	mov	v1.s[3], w20                                             // ctr0 + 1
	add	w20, w12, #2
	rev	w20, w20
	mov	v2.s[3], w20                                             // ctr0 + 2
	add	w20, w12, #3
	rev	w20, w20
	mov	v3.s[3], w20                                             // ctr0 + 3
    // Calculate the ctr values for the *next* (not current) group of four
    // blocks. Store the incremented parts to the stack.
	add	w20, w12, #4
	rev	w20, w20
	str	w20, [sp, #172]                             // ctr0 + 4 for next iter
	add	w20, w12, #5
	rev	w20, w20
	str	w20, [sp, #188]                             // ctr0 + 5 for next iter
	add	w20, w12, #6
	rev	w20, w20
	str	w20, [sp, #204]                             // ctr0 + 6 for next iter
	add	w20, w12, #7
	rev	w20, w20
	str	w20, [sp, #220]                             // ctr0 + 7 for next iter
	add	w12, w12, #8                                                 // Advance counter past these two sets
    // --- Start AES for first 4 blocks ---
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 0
	ldp	q19, q20, [x8, #16]                                           // load rk1, rk2
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 0
	ldp	q21, q22, [x8, #48]                                           // load rk3, rk4
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 0
	ldp	q23, q24, [x8, #80]                                           // load rk5, rk6
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 0
	ldp	q13, q14, [x6, #32]                                         // load H2, H3 (GHASH keys)
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 1
	ldp	q25, q26, [x8, #112]                                          // load rk7, rk8
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 1
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 2
	ext	v14.16b, v14.16b, v14.16b, #8                                      // Byte swap H3 for GHASH
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 2
	ext	v13.16b, v13.16b, v13.16b, #8                                      // Byte swap H2 for GHASH
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 2
	ldr	q15, [x6, #80]                                               // load H4
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 2
	ext	v15.16b, v15.16b, v15.16b, #8                                      // Byte swap H4 for GHASH
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 3
	ld1	{ v11.16b}, [x3]                                      // Load initial GHASH accumulator (T)
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 3
	ext	v11.16b, v11.16b, v11.16b, #8                             // Byte swap T for GHASH
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 3
	rev64	v11.16b, v11.16b                                           // Correct byte order within 64-bit lanes
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 3
	trn2	v17.2d,  v14.2d,    v15.2d                                      // Karatsuba key: H4_low | H3_low
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 4
	ldr	q12, [x6]                                                    // load H1
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 4
	ext	v12.16b, v12.16b, v12.16b, #8                                      // Byte swap H1 for GHASH
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 4
	trn1	v9.2d, v14.2d,    v15.2d                                      // Karatsuba key: H4_high | H3_high
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 4
	trn2	v16.2d,  v12.2d,    v13.2d                                      // Karatsuba key: H2_low | H1_low
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 5
	ldr	q30, [x7]                                                // Preload round N key for final EOR
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 5
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 5
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 5
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 6
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 6
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 6
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 6
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 7
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 7
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 7
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 7
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 8
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 8
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 8
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	b.lt	Lenc_finish_first_blocks_eor3                                         // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	b.eq	Lenc_finish_first_blocks_eor3                                         // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
Lenc_finish_first_blocks_eor3:
	cmp	x0, x5                                    // check if we have <= 4 blocks to process in the tail
	eor	v17.16b, v17.16b, v9.16b                                   // Karatsuba key: H3^H4
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	trn1	v8.2d,  v12.2d, v13.2d                                 // Karatsuba key: H2_high | H1_high
	eor	v16.16b, v16.16b, v8.16b                            // Karatsuba key: H1^H2
	b.ge	Lenc_tail_eor3                                                        // handle tail if no more full 4-block sets
	ldp	q6, q7, [x0, #32]                                  // AES blocks 2,3 load plaintext
	ldp	q4, q5, [x0], #64                                  // AES blocks 0,1 load plaintext
    // Compute and store first 4 ciphertext blocks
	eor3	v4.16b, v4.16b, v30.16b, v0.16b                                // AES block 0 - result = PT ^ AES(ctr0)
	eor3	v5.16b, v5.16b, v30.16b, v1.16b                                // AES block 1 - result = PT ^ AES(ctr1)
	eor3	v6.16b, v6.16b, v30.16b, v2.16b                                // AES block 2 - result = PT ^ AES(ctr2)
	eor3	v7.16b, v7.16b, v30.16b, v3.16b                                // AES block 3 - result = PT ^ AES(ctr3)
	st1	{ v4.16b, v5.16b, v6.16b, v7.16b}, [x2], #64  // AES blocks 0-3 - store result
    // Load counter values for the second iteration from the stack
	ldp	q0, q1, [sp, #160]
	ldp	q2, q3, [sp, #192]
    // Prepare and store counter values for the third iteration
	rev	w20, w12
	str	w20, [sp, #172]                             // ctr + 8
	add	w20, w12, #1
	rev	w20, w20
	str	w20, [sp, #188]                             // ctr + 9
	add	w20, w12, #2
	rev	w20, w20
	str	w20, [sp, #204]                             // ctr + 10
	add	w20, w12, #3
	rev	w20, w20
	str	w20, [sp, #220]                             // ctr + 11
	add	w12, w12, #4                                                 // Advance counter base
	cmp	x0, x5                                    // check if we have <= 4 blocks remaining
	b.ge	Lenc_prepretail_eor3                                                  // go to prepretail if < 2 full loops left
Lenc_main_loop_eor3:	//	main loop start (processes 4 blocks per iteration)
    // --- AES Pipeline for blocks 4k+4 to 4k+7 ---
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 0
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 0
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 0
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 0
	ldr	d8, [sp, #128]                      // Load GHASH reduction constant
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 1
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 1
    // --- GHASH Pipeline (interleaved with AES) for blocks 4k to 4k+3 ---
	rev64	v4.16b, v4.16b                                             // GHASH block 4k - Byte swap CT
	rev64	v5.16b, v5.16b                                             // GHASH block 4k+1 - Byte swap CT
	rev64	v6.16b, v6.16b                                             // GHASH block 4k+2 - Byte swap CT
	rev64	v7.16b, v7.16b                                             // GHASH block 4k+3 - Byte swap CT
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 2
	ext	v11.16b, v11.16b, v11.16b, #8                             // GHASH - prepare acc for XOR
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 2
	eor	v4.16b, v4.16b, v11.16b                                   // GHASH block 4k - Y_i = CT_i ^ Y_{i-1}
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 2
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 4k - low
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 2
	pmull2	v9.1q, v6.2d, v13.2d                                     // GHASH block 4k+2 - high
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 3
	mov	d10, v17.d[1]                                                // GHASH block 4k - mid Karatsuba key
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 3
	mov	d20, v4.d[1]                                             // GHASH block 4k - mid
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 3
	eor	v20.8b, v20.8b, v4.8b                               // GHASH block 4k - mid
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 3
	mov	d21, v5.d[1]                                             // GHASH block 4k+1 - mid
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 4
	eor	v21.8b, v21.8b, v5.8b                               // GHASH block 4k+1 - mid
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 4
	pmull	v10.1q, v20.1d, v10.1d                               // GHASH block 4k - mid
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 4
	pmull	v21.1q, v21.1d, v17.1d                             // GHASH block 4k+1 - mid
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 4
	eor	v10.16b, v10.16b, v21.16b                              // GHASH block 4k+1 - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 5
	ext	v22.16b, v22.16b, v6.16b, #8                        // GHASH block 4k+2 - mid
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 5
	eor	v22.16b, v22.16b, v6.16b                            // GHASH block 4k+2 - mid
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 5
	pmull2	v22.1q, v22.2d, v16.2d                            // GHASH block 4k+2 - mid
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 5
	mov	d23, v7.d[1]                                             // GHASH block 4k+3 - mid
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 6
	eor	v23.8b, v23.8b, v7.8b                               // GHASH block 4k+3 - mid
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 6
	pmull	v23.1q, v23.1d, v16.1d                             // GHASH block 4k+3 - mid
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 6
	eor3	v10.16b, v10.16b, v22.16b, v23.16b                       // GHASH block 4k+2/3 - mid
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 6
	pmull2	v22.1q, v4.2d, v15.2d                                  // GHASH block 4k - high
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 7
	pmull2	v21.1q, v7.2d, v12.2d                                  // GHASH block 4k+3 - high
	eor	v22.16b, v22.16b, v21.16b                        // GHASH block 4k+3 - high
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 7
	pmull2	v23.1q, v5.2d, v14.2d                                  // GHASH block 4k+1 - high
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 7
	pmull	v21.1q, v6.1d, v13.1d                                   // GHASH block 4k+2 - low
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 7
	pmull	v20.1q, v5.1d, v14.1d                                   // GHASH block 4k+1 - low
	eor3	v9.16b, v9.16b, v22.16b, v23.16b                       // GHASH block 4k/1/2/3 - high
	pmull	v22.1q, v7.1d, v12.1d                                   // GHASH block 4k+3 - low
	ldp	q6, q7, [x0, #32]
	ldp	q4, q5, [x0], #64
	eor	v20.16b, v20.16b, v21.16b                        // GHASH block 4k+1 - low
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 4k+4 - round 8
	eor3	v11.16b, v11.16b, v22.16b, v20.16b                       // GHASH block 4k/1/2/3 - low
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 4k+5 - round 8
	eor3	v10.16b, v10.16b, v9.16b, v11.16b                             // MODULO - karatsuba tidy up
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 4k+6 - round 8
	pmull	v20.1q, v9.1d, v8.1d                        // MODULO - top 64b align with mid
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 4k+7 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	b.lt	Lenc_main_loop_continue_eor3                                          // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 10
	b.eq	Lenc_main_loop_continue_eor3                                          // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 12
Lenc_main_loop_continue_eor3:
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor3	v10.16b, v10.16b, v20.16b, v9.16b                          // MODULO - fold into mid
	pmull	v9.1q, v10.1d, v8.1d                           // MODULO - mid 64b align with low
	ext	v20.16b, v10.16b, v10.16b, #8                          // MODULO - other mid alignment
	eor3	v11.16b, v9.16b, v11.16b, v20.16b                          // MODULO - fold into low
	aese	v0.16b, v31.16b                                             // AES block 4k+4 - round N-1
	eor3	v4.16b, v4.16b, v30.16b, v0.16b                                // AES block 4k+4 - result
	aese	v1.16b, v31.16b                                             // AES block 4k+5 - round N-1
	eor3	v5.16b, v5.16b, v30.16b, v1.16b                                // AES block 4k+5 - result
	aese	v2.16b, v31.16b                                             // AES block 4k+6 - round N-1
	eor3	v6.16b, v6.16b, v30.16b, v2.16b                                // AES block 4k+6 - result
	aese	v3.16b, v31.16b                                             // AES block 4k+7 - round N-1
	eor3	v7.16b, v7.16b, v30.16b, v3.16b                                // AES block 4k+7 - result
	ldp	q0, q1, [sp, #160]
	ldp	q2, q3, [sp, #192]
    // We used these registers as temporaries above so reload the RKs.
	ldp	q20, q21, [x8, #32]                                           // load rk2, rk3
	ldp	q22, q23, [x8, #64]                                           // load rk4, rk5
	st1	{ v4.16b, v5.16b, v6.16b, v7.16b}, [x2], #64  // AES blocks 4k+4-7 - store result
	rev	w20, w12
	str	w20, [sp, #172]
	add	w20, w12, #1
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w12, #2
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w12, #3
	rev	w20, w20
	str	w20, [sp, #220]
	add	w12, w12, #4
	cmp	x0, x5                                    // LOOP_eor3 CONTROL
	b.lt	Lenc_main_loop_eor3
Lenc_prepretail_eor3:	//	PREPRETAIL
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 0
	rev64	v6.16b, v6.16b                                             // GHASH block 2
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 0
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 0
	rev64	v4.16b, v4.16b                                             // GHASH block 0
	ext	v11.16b, v11.16b, v11.16b, #8                             // PRE 0
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 1
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 1
	eor	v4.16b, v4.16b, v11.16b                                   // PRE 1
	rev64	v5.16b, v5.16b                                             // GHASH block 1
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 2
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 0
	mov	d10, v17.d[1]                                                // GHASH block 0 - mid Karatsuba key
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 1
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 0 - low
	mov	d8, v4.d[1]                                         // GHASH block 0 - mid
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH block 0 - high
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 3
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 2
	eor	v8.8b, v8.8b, v4.8b                       // GHASH block 0 - mid
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 2
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 1
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 3
	pmull	v10.1q, v8.1d, v10.1d                           // GHASH block 0 - mid
	pmull2	v4.1q, v5.2d, v14.2d                                      // GHASH block 1 - high
	pmull	v8.1q, v5.1d, v14.1d                               // GHASH block 1 - low
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 2
	eor	v9.16b, v9.16b, v4.16b                                  // GHASH block 1 - high
	mov	d4, v5.d[1]                                                 // GHASH block 1 - mid
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 3
	eor	v11.16b, v11.16b, v8.16b                          // GHASH block 1 - low
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 3
	eor	v4.8b, v4.8b, v5.8b                                       // GHASH block 1 - mid
	mov	d8, v6.d[1]                                         // GHASH block 2 - mid
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 4
	rev64	v7.16b, v7.16b                                             // GHASH block 3
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 4
	pmull	v4.1q, v4.1d, v17.1d                                     // GHASH block 1 - mid
	eor	v8.8b, v8.8b, v6.8b                       // GHASH block 2 - mid
	pmull	v5.1q, v6.1d, v13.1d                                       // GHASH block 2 - low
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 5
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 4
	eor	v10.16b, v10.16b, v4.16b                                  // GHASH block 1 - mid
	pmull2	v4.1q, v6.2d, v13.2d                                      // GHASH block 2 - high
	eor	v11.16b, v11.16b, v5.16b                                  // GHASH block 2 - low
	ins	v8.d[1], v8.d[0]                             // GHASH block 2 - mid
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 5
	eor	v9.16b, v9.16b, v4.16b                                  // GHASH block 2 - high
	mov	d4, v7.d[1]                                                 // GHASH block 3 - mid
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 4
	pmull2	v8.1q, v8.2d, v16.2d                    // GHASH block 2 - mid
	eor	v4.8b, v4.8b, v7.8b                                       // GHASH block 3 - mid
	pmull2	v5.1q, v7.2d, v12.2d                                      // GHASH block 3 - high
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 5
	pmull	v4.1q, v4.1d, v16.1d                                     // GHASH block 3 - mid
	eor	v10.16b, v10.16b, v8.16b                          // GHASH block 2 - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 5
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 6
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 6
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 6
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 6
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 7
	eor	v9.16b, v9.16b, v5.16b                                  // GHASH block 3 - high
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 7
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 7
	ldr	d8, [sp, #128]
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 8
	eor	v10.16b, v10.16b, v4.16b                                  // GHASH block 3 - mid
	pmull	v6.1q, v7.1d, v12.1d                                       // GHASH block 3 - low
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 8
	eor	v11.16b, v11.16b, v6.16b                                  // GHASH block 3 - low
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 7
	eor	v10.16b, v10.16b, v9.16b                                 // karatsuba tidy up
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 8
	pmull	v4.1q, v9.1d, v8.1d
	ext	v9.16b, v9.16b, v9.16b, #8
	eor	v10.16b, v10.16b, v11.16b
	b.lt	Lenc_finish_prepretail_eor3                                           // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	b.eq	Lenc_finish_prepretail_eor3                                           // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
Lenc_finish_prepretail_eor3:
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	eor3	v10.16b, v10.16b, v4.16b, v9.16b
	pmull	v4.1q, v10.1d, v8.1d
	ext	v10.16b, v10.16b, v10.16b, #8
	eor3	v11.16b, v11.16b, v4.16b, v10.16b
Lenc_tail_eor3:	//	TAIL: Process remaining 0 to 3 blocks
	ext	v8.16b, v11.16b, v11.16b, #8                      // Save current GHASH state for partial tag feed-in
	sub	x5, x4, x0                    // main_end_input_ptr is number of bytes left to process
	ldp	x6, x7, [x0], #16                            // AES block 0 - load plaintext
	eor	x6, x6, x13                                       // AES block 0 - round N low
	eor	x7, x7, x14                                       // AES block 0 - round N high
	cmp	x5, #48
	fmov	d4, x6                                                 // AES block 0 - mov low
	fmov	v4.d[1], x7                                             // AES block 0 - mov high
	eor	v5.16b, v4.16b, v0.16b                                    // AES block 0 - result
	b.gt	Lenc_blocks_more_than_3_eor3
	cmp	x5, #32
	mov	v3.16b, v2.16b
	movi	v11.8b, #0
	movi	v9.8b, #0
	mov	v2.16b, v1.16b
	movi	v10.8b, #0
	b.gt	Lenc_blocks_more_than_2_eor3
	mov	v3.16b, v1.16b
	cmp	x5, #16
	b.gt	Lenc_blocks_more_than_1_eor3
	b	Lenc_blocks_less_than_1_eor3
Lenc_blocks_more_than_3_eor3:	//	blocks left >  3
	st1	{ v5.16b}, [x2], #16                                   // AES final-2 block - store result
	ldp	x6, x7, [x0], #16                            // AES final-2 block - load input low & high
	rev64	v4.16b, v5.16b                                             // GHASH final-3 block
	eor	x6, x6, x13                                       // AES final-2 block - round N low
	eor	v4.16b, v4.16b, v8.16b                            // feed in partial tag
	eor	x7, x7, x14                                       // AES final-2 block - round N high
	mov	d22, v4.d[1]                                             // GHASH final-3 block - mid
	fmov	d5, x6                                                 // AES final-2 block - mov low
	fmov	v5.d[1], x7                                             // AES final-2 block - mov high
	eor	v22.8b, v22.8b, v4.8b                               // GHASH final-3 block - mid
	movi	v8.8b, #0                                              // suppress further partial tag feed in
	mov	d10, v17.d[1]                                                // GHASH final-3 block - mid
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH final-3 block - low
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH final-3 block - high
	pmull	v10.1q, v22.1d, v10.1d                               // GHASH final-3 block - mid
	eor	v5.16b, v5.16b, v1.16b                                    // AES final-2 block - result
Lenc_blocks_more_than_2_eor3:	//	blocks left >  2
	st1	{ v5.16b}, [x2], #16                                   // AES final-2 block - store result
	ldp	x6, x7, [x0], #16                            // AES final-1 block - load input low & high
	rev64	v4.16b, v5.16b                                             // GHASH final-2 block
	eor	x6, x6, x13                                       // AES final-1 block - round N low
	eor	v4.16b, v4.16b, v8.16b                            // feed in partial tag
	fmov	d5, x6                                                 // AES final-1 block - mov low
	eor	x7, x7, x14                                       // AES final-1 block - round N high
	fmov	v5.d[1], x7                                             // AES final-1 block - mov high
	movi	v8.8b, #0                                              // suppress further partial tag feed in
	pmull2	v20.1q, v4.2d, v14.2d                                  // GHASH final-2 block - high
	mov	d22, v4.d[1]                                             // GHASH final-2 block - mid
	pmull	v21.1q, v4.1d, v14.1d                                   // GHASH final-2 block - low
	eor	v22.8b, v22.8b, v4.8b                               // GHASH final-2 block - mid
	eor	v5.16b, v5.16b, v2.16b                                    // AES final-1 block - result
	eor	v9.16b, v9.16b, v20.16b                              // GHASH final-2 block - high
	pmull	v22.1q, v22.1d, v17.1d                             // GHASH final-2 block - mid
	eor	v11.16b, v11.16b, v21.16b                              // GHASH final-2 block - low
	eor	v10.16b, v10.16b, v22.16b                              // GHASH final-2 block - mid
Lenc_blocks_more_than_1_eor3:	//	blocks left >  1
	st1	{ v5.16b}, [x2], #16                                   // AES final-1 block - store result
	rev64	v4.16b, v5.16b                                             // GHASH final-1 block: Byte Swap CT
	ldp	x6, x7, [x0], #16                            // AES final block - load plaintext
	eor	v4.16b, v4.16b, v8.16b                            // Feed in partial tag
	movi	v8.8b, #0                                              // Clear for next block
	eor	x6, x6, x13                                       // AES final block - round N low
	mov	d22, v4.d[1]                                             // GHASH final-1 block - mid
	pmull2	v20.1q, v4.2d, v13.2d                                  // GHASH final-1 block - high
	eor	x7, x7, x14                                       // AES final block - round N high
	eor	v22.8b, v22.8b, v4.8b                               // GHASH final-1 block - mid
	eor	v9.16b, v9.16b, v20.16b                              // GHASH final-1 block - high
	ins	v22.d[1], v22.d[0]                                     // GHASH final-1 block - mid
	fmov	d5, x6                                                 // AES final block - mov low
	fmov	v5.d[1], x7                                             // AES final block - mov high
	pmull2	v22.1q, v22.2d, v16.2d                            // GHASH final-1 block - mid
	pmull	v21.1q, v4.1d, v13.1d                                   // GHASH final-1 block - low
	eor	v5.16b, v5.16b, v3.16b                                    // AES final block - result
	eor	v10.16b, v10.16b, v22.16b                              // GHASH final-1 block - mid
	eor	v11.16b, v11.16b, v21.16b                              // GHASH final-1 block - low
Lenc_blocks_less_than_1_eor3:	//	Last partial block handling
	add	x10, x10, x1, lsr #7                                    // Calculate the updated counter based on the number of 16B chunks we processed
	rev	w10, w10
	str	w10, [x16, #12]                                              // store the updated counter
	and	x1, x1, #127                                     // bit_length %= 128
	mvn	x13, xzr                                                        // Mask for low 64 bits
	sub	x1, x1, #128                                     //
	neg	x1, x1                                           // Valid bits in the last block (1-128)
	ldr	q18, [x2]                                  // Load destination for merging
	mvn	x14, xzr                                                        // Mask for high 64 bits
	and	x1, x1, #127                                     // bit_length %= 128
	lsr	x14, x14, x1                                        // rkN_h is mask for top 64b of last block
	cmp	x1, #64
	csel	x6, x13, x14, lt
	csel	x7, x14, xzr, lt
	fmov	d0, x6                                                 // ctr0d is mask for last block
	fmov	v0.d[1], x7
	and	v5.16b, v5.16b, v0.16b                                    // Mask out unused bits of the last CT block
	rev64	v4.16b, v5.16b                                             // GHASH final block - byte swap
	eor	v4.16b, v4.16b, v8.16b                            // Feed in partial tag
	bif	v5.16b, v18.16b, v0.16b                        // Bitwise Insert: merge with existing data at output_ptr
	pmull2	v20.1q, v4.2d, v12.2d                                  // GHASH final block - high
	mov	d8, v4.d[1]                                         // GHASH final block - mid
	pmull	v21.1q, v4.1d, v12.1d                                   // GHASH final block - low
	eor	v9.16b, v9.16b, v20.16b                              // GHASH final block - high
	eor	v8.8b, v8.8b, v4.8b                       // GHASH final block - mid
	pmull	v8.1q, v8.1d, v16.1d                     // GHASH final block - mid
	eor	v11.16b, v11.16b, v21.16b                              // GHASH final block - low
	eor	v10.16b, v10.16b, v8.16b                          // GHASH final block - mid
	eor	v4.16b, v11.16b, v9.16b                                  // MODULO - karatsuba tidy up
	fmov	d8, x21
	eor	v10.16b, v10.16b, v4.16b                                  // MODULO - karatsuba tidy up
	pmull	v7.1q, v9.1d, v8.1d                            // MODULO - top 64b align with mid
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor3	v10.16b, v10.16b, v7.16b, v9.16b                              // MODULO - fold into mid
	pmull	v9.1q, v10.1d, v8.1d                           // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	st1	{ v5.16b}, [x2]                                        // store all 16B
	eor3	v11.16b, v11.16b, v9.16b, v10.16b                             // MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8                             // Byte swap GHASH result
	rev64	v11.16b, v11.16b                                           // Final Tag
	mov	x0, x15
	st1	{ v11.16b }, [x3]                                     // Store final tag
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	d8, d9, [sp, #64]
	ldp	d10, d11, [sp, #80]
	ldp	d12, d13, [sp, #96]
	ldp	d14, d15, [sp, #112]
	ldp	x29, x30, [sp], #224
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.globl	_aes_gcm_dec_kernel_eor3
.private_extern	_aes_gcm_dec_kernel_eor3

.align	4
_aes_gcm_dec_kernel_eor3:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29, x30, [sp, #-224]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	ld1	{ v0.16b}, [x4]
	mov	v1.16b, v0.16b
	mov	v2.16b, v0.16b
	mov	v3.16b, v0.16b
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]
	stp	d8, d9, [sp, #64]
	stp	d10, d11, [sp, #80]
	stp	d12, d13, [sp, #96]
	stp	d14, d15, [sp, #112]
	ldr	w17, [x8, #240]                                             // Load number of AES rounds
	add	x19, x8, x17, lsl #4                                    // borrow input_l1 for last key
	ldp	x13, x14, [x19]                                        // load round N keys
	ldr	q31, [x19, #-16]                                         // load round N-1 keys
	add	x4, x0, x1, lsr #3                    // end_input_ptr
	lsr	x5, x1, #3                               // byte_len
	mov	x15, x5
	ldr	w9, [x16, #12]                                           // Load scalar 32-bit counter (CTR)
	sub	x5, x5, #1                       // byte_len - 1
	ldr	q18, [x8, #0]                                                   // load rk0
	and	x5, x5, #0xffffffffffffffc0      // number of bytes to be processed in main loop (at least 1 byte must be handled by tail)
	add	x5, x5, x0
	rev	w9, w9                                                   // Reverse it once for big-endian incrementing
	uxtw	x10, w9                                                      // Zero extend reversed w9 into x10
	str	q0,     [sp, #160]
	str	q0,     [sp, #176]
	str	q0,     [sp, #192]
	str	q0,     [sp, #208]
	rev	w20, w9
	mov	v0.s[3], w20
	add	w20, w9, #1
	rev	w20, w20
	mov	v1.s[3], w20
	add	w20, w9, #2
	rev	w20, w20
	mov	v2.s[3], w20
	add	w20, w9, #3
	rev	w20, w20
	mov	v3.s[3], w20
	add	w20, w9, #4
	rev	w20, w20
	str	w20, [sp, #172]
	add	w20, w9, #5
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w9, #6
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w9, #7
	rev	w20, w20
	str	w20, [sp, #220]
	add	w9, w9, #8
    // Pre-compute this value instead of using two instructions for moving and then shifting in the main loop
	mov	x21, #0xc200000000000000
	str	x21, [sp, #128]
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 0
	ldp	q19, q20, [x8, #16]                                           // load rk1, rk2
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 0
	ldp	q21, q22, [x8, #48]                                           // load rk3, rk4
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 0
	ldp	q23, q24, [x8, #80]                                           // load rk5, rk6
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 0
	ldp	q13, q14, [x6, #32]                                         // load h2, h3
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 1
	ldp	q25, q26, [x8, #112]                                          // load rk7, rk8
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 1
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 2
	ext	v14.16b, v14.16b, v14.16b, #8
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 2
	ext	v13.16b, v13.16b, v13.16b, #8
	ldr	q15, [x6, #80]                                               // load h4
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 2
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 2
	ext	v15.16b, v15.16b, v15.16b, #8
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 3
	ld1	{ v11.16b}, [x3]
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 3
	ext	v11.16b, v11.16b, v11.16b, #8
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 3
	rev64	v11.16b, v11.16b
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 3
	trn2	v17.2d,  v14.2d,    v15.2d                                      // h4l | h3l
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 4
	ldr	q12, [x6]                                                    // load h1
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 4
	ext	v12.16b, v12.16b, v12.16b, #8
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 4
	trn1	v9.2d, v14.2d,    v15.2d                                      // h4h | h3h
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 4
	trn2	v16.2d,  v12.2d,    v13.2d                                      // h2l | h1l
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 5
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 5
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 5
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 5
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 6
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 6
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 6
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 6
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 7
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 7
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 7
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 7
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b           // AES block 0 - round 8
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b           // AES block 1 - round 8
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b           // AES block 2 - round 8
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b           // AES block 3 - round 8
	cmp	x17, #12                                                       // setup flags for AES-128/192/256 check
	b.lt	Ldec_finish_first_blocks_eor3                                         // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	b.eq	Ldec_finish_first_blocks_eor3                                         // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
Ldec_finish_first_blocks_eor3:
	ldr	q27, [x19]                                          // load rkN
	cmp	x0, x5                                    // check if we have <= 4 blocks
	eor	v17.16b, v17.16b, v9.16b                                   // h4k | h3k
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	trn1	v8.2d,    v12.2d,    v13.2d                                      // h2h | h1h
	eor	v16.16b, v16.16b, v8.16b                                      // h2k | h1k
	b.ge	Ldec_tail_eor3                                                        // handle tail
    // Setup for AES blocks 0-3 is done purely on NEON side instead of mixing NEON and scalar instructions.
    // This is because the final result of the AES block needs to be EORd with the final round key
    // value (v30). This avoids several fmovs.
	ldp	q6, q7, [x0, #32]                                  // AES blocks 2,3 load ciphertext
	ldp	q4, q5, [x0], #64                                  // AES blocks 0,1 load ciphertext
	eor3	v0.16b, v4.16b, v0.16b, v27.16b                           // AES block 0 - result
	eor3	v1.16b, v5.16b, v1.16b, v27.16b                           // AES block 1 - result
	eor3	v2.16b, v6.16b, v2.16b, v27.16b                           // AES block 2 - result
	eor3	v3.16b, v7.16b, v3.16b, v27.16b                           // AES block 3 - result
	st1	{ v0.16b, v1.16b, v2.16b, v3.16b}, [x2], #64  // AES blocks 0-3 - store result
	ldr	q0,     [sp, #160]
	ldr	q1,     [sp, #176]
	ldr	q2,     [sp, #192]
	ldr	q3,     [sp, #208]
	rev	w20, w9
	str	w20, [sp, #172]
	add	w20, w9, #1
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w9, #2
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w9, #3
	rev	w20, w20
	str	w20, [sp, #220]
	add	w9, w9, #4
	cmp	x0, x5                                    // check if we have <= 4 blocks
	b.ge	Ldec_prepretail_eor3                                                  // do prepretail
Ldec_main_loop_eor3:	//	main loop start
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 0
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 0
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 0
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 0
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 1
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 1
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 1
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 1
	rev64	v4.16b, v4.16b                                             // GHASH block 4k
	rev64	v5.16b, v5.16b                                             // GHASH block 4k+1
	rev64	v6.16b, v6.16b                                             // GHASH block 4k+2
	rev64	v7.16b, v7.16b                                             // GHASH block 4k+3
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 2
	ext	v11.16b, v11.16b, v11.16b, #8                             // PRE 0
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 2
	eor	v4.16b, v4.16b, v11.16b                                   // PRE 1
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 2
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 4k - low
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 2
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH block 4k - high
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 3
	mov	d10, v17.d[1]                                                // GHASH block 4k - mid
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 3
	mov	d8, v4.d[1]                                                   // GHASH block 4k - mid
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 3
	eor	v8.8b, v8.8b, v4.8b                                           // GHASH block 4k - mid
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 3
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 4
	pmull2	v4.1q, v5.2d, v14.2d                                        // GHASH block 4k+1 - high
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 4
	eor	v9.16b, v9.16b, v4.16b                                    // GHASH block 4k+1 - high
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 4
	mov	d4, v5.d[1]                                                   // GHASH block 4k+1 - mid
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 4
	pmull	v10.1q, v8.1d, v10.1d                                     // GHASH block 4k - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 5
	pmull	v8.1q, v5.1d, v14.1d                                         // GHASH block 4k+1 - low
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 5
	eor	v4.8b, v4.8b, v5.8b                                           // GHASH block 4k+1 - mid
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 5
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 5
	pmull	v5.1q, v6.1d, v13.1d                                         // GHASH block 4k+2 - low
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 6
	eor3	v11.16b, v11.16b, v8.16b, v5.16b                                   // GHASH block 4k+1 - low & GHASH block 4k+2 - low
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 6
	mov	d8, v6.d[1]                                                   // GHASH block 4k+2 - mid
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 6
	eor	v8.8b, v8.8b, v6.8b                                           // GHASH block 4k+2 - mid
	pmull	v4.1q, v4.1d, v17.1d                                         // GHASH block 4k+1 - mid
	ins	v8.d[1], v8.d[0]                                                 // GHASH block 4k+2 - mid
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 6
	pmull2	v28.1q, v6.2d, v13.2d                                   // GHASH block 4k+2 - high
	mov	d6, v7.d[1]                                                   // GHASH block 4k+3 - mid
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 7
	pmull2	v8.1q, v8.2d, v16.2d                                        // GHASH block 4k+2 - mid
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 7
	pmull	v27.1q, v7.1d, v12.1d                                 // GHASH block 4k+3 - low
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 7
	eor3	v10.16b, v10.16b, v4.16b, v8.16b                                   // GHASH block 4k+1 - mid & GHASH block 4k+2 - mid
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 7
	pmull2	v5.1q, v7.2d, v12.2d                                        // GHASH block 4k+3 - high
	eor	v6.8b, v6.8b, v7.8b                                           // GHASH block 4k+3 - mid
	eor3	v9.16b, v9.16b, v28.16b, v5.16b                              // GHASH block 4k+2 - high & GHASH block 4k+3 - high
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          // AES block 4k+4 - round 8
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          // AES block 4k+5 - round 8
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          // AES block 4k+6 - round 8
	pmull	v6.1q, v6.1d, v16.1d                                         // GHASH block 4k+3 - mid
	ldr	d8, [sp, #128]
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          // AES block 4k+7 - round 8
	pmull	v7.1q, v9.1d, v8.1d                           // MODULO - top 64b align with mid
	eor3	v10.16b, v10.16b, v6.16b, v7.16b                                // GHASH block 4k+3 - mid & MODULO - fold into mid
	eor	v11.16b, v11.16b, v27.16b                            // GHASH block 4k+3 - low
	eor	v6.16b, v11.16b, v9.16b                                    // MODULO - karatsuba tidy up
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor3	v10.16b, v10.16b, v6.16b, v9.16b                                // MODULO - karatsuba tidy up & MODULO - fold into mid
	pmull	v8.1q, v10.1d, v8.1d                    // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	eor3	v11.16b, v11.16b, v8.16b, v10.16b                      // MODULO - fold into low
	cmp	w17, #12                                                     // setup flags for AES-128/192/256 check
	b.lt	Ldec_main_loop_continue_eor3                                          // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 10
	b.eq	Ldec_main_loop_continue_eor3                                          // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 4k+4 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 4k+5 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 4k+6 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 4k+7 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 4k+4 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 4k+5 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 4k+6 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 4k+7 - round 12
Ldec_main_loop_continue_eor3:
	ldr	q27, [x19]                                          // load rkN
	ldp	q6, q7, [x0, #32]                                  // AES blocks 2,3 load ciphertext
	ldp	q4, q5, [x0], #64                                  // AES blocks 0,1 load ciphertext
	aese	v0.16b, v31.16b                                             // AES block 4k+4 - round N-1
	eor3	v0.16b, v4.16b, v0.16b, v27.16b                           // AES block 4k+4 - result
	aese	v1.16b, v31.16b                                             // AES block 4k+5 - round N-1
	eor3	v1.16b, v5.16b, v1.16b, v27.16b                           // AES block 4k+5 - result
	aese	v2.16b, v31.16b                                             // AES block 4k+6 - round N-1
	eor3	v2.16b, v6.16b, v2.16b, v27.16b                           // AES block 4k+6 - result
	aese	v3.16b, v31.16b                                             // AES block 4k+7 - round N-1
	eor3	v3.16b, v7.16b, v3.16b, v27.16b                           // AES block 4k+7 - result
	st1	{ v0.16b, v1.16b, v2.16b, v3.16b}, [x2], #64  // AES blocks 4k+4-7 - store result
	ldr	q0,     [sp, #160]
	ldr	q1,     [sp, #176]
	ldr	q2,     [sp, #192]
	ldr	q3,     [sp, #208]
	rev	w20, w9
	str	w20, [sp, #172]
	add	w20, w9, #1
	rev	w20, w20
	str	w20, [sp, #188]
	add	w20, w9, #2
	rev	w20, w20
	str	w20, [sp, #204]
	add	w20, w9, #3
	rev	w20, w20
	str	w20, [sp, #220]
	add	w9, w9, #4
	cmp	x0, x5
	b.lt	Ldec_main_loop_eor3
Ldec_prepretail_eor3:	//	PREPRETAIL
	rev64	v4.16b, v4.16b                                             // GHASH block 0
	rev64	v5.16b, v5.16b                                             // GHASH block 1
	ext	v11.16b, v11.16b, v11.16b, #8                             // PRE 0
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 0
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 0
	eor	v4.16b, v4.16b, v11.16b                                   // PRE 1
	rev64	v6.16b, v6.16b                                             // GHASH block 2
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 1
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH block 0 - low
	mov	d8, v4.d[1]                                                   // GHASH block 0 - mid
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH block 0 - high
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 0
	mov	d10, v17.d[1]                                                // GHASH block 0 - mid
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 1
	eor	v8.8b, v8.8b, v4.8b                                           // GHASH block 0 - mid
	pmull2	v4.1q, v5.2d, v14.2d                                        // GHASH block 1 - high
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 1
	rev64	v7.16b, v7.16b                                             // GHASH block 3
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 0
	pmull	v10.1q, v8.1d, v10.1d                                     // GHASH block 0 - mid
	eor	v9.16b, v9.16b, v4.16b                                    // GHASH block 1 - high
	pmull	v8.1q, v5.1d, v14.1d                                         // GHASH block 1 - low
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 1
	mov	d4, v5.d[1]                                                   // GHASH block 1 - mid
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 2
	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 2
	eor	v11.16b, v11.16b, v8.16b                                    // GHASH block 1 - low
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 2
	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 3
	mov	d8, v6.d[1]                                                   // GHASH block 2 - mid
	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 2
	eor	v4.8b, v4.8b, v5.8b                                           // GHASH block 1 - mid
	pmull	v5.1q, v6.1d, v13.1d                                         // GHASH block 2 - low
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 4
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 3
	eor	v8.8b, v8.8b, v6.8b                                           // GHASH block 2 - mid
	pmull	v4.1q, v4.1d, v17.1d                                         // GHASH block 1 - mid
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 5
	eor	v11.16b, v11.16b, v5.16b                                    // GHASH block 2 - low
	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 4
	pmull2	v5.1q, v7.2d, v12.2d                                        // GHASH block 3 - high
	eor	v10.16b, v10.16b, v4.16b                                    // GHASH block 1 - mid
	pmull2	v4.1q, v6.2d, v13.2d                                        // GHASH block 2 - high
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 5
	ins	v8.d[1], v8.d[0]                                                 // GHASH block 2 - mid
	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 3
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 3
	eor3	v9.16b, v9.16b, v4.16b, v5.16b                                   // GHASH block 2 - high & GHASH block 3 - high
	pmull	v4.1q, v7.1d, v12.1d                                         // GHASH block 3 - low
	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 4
	mov	d6, v7.d[1]                                                   // GHASH block 3 - mid
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 4
	pmull2	v8.1q, v8.2d, v16.2d                                        // GHASH block 2 - mid
	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 5
	eor	v6.8b, v6.8b, v7.8b                                           // GHASH block 3 - mid
	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 5
	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 6
	eor	v10.16b, v10.16b, v8.16b                                    // GHASH block 2 - mid
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 6
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 6
	movi	v8.8b, #0xc2
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 6
	eor	v11.16b, v11.16b, v4.16b                                    // GHASH block 3 - low
	pmull	v6.1q, v6.1d, v16.1d                                         // GHASH block 3 - mid
	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 7
	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 7
	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 7
	eor	v10.16b, v10.16b, v6.16b                                    // GHASH block 3 - mid
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          // AES block 3 - round 8
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 7
	eor	v6.16b, v11.16b, v9.16b                                    // MODULO - karatsuba tidy up
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          // AES block 1 - round 8
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          // AES block 0 - round 8
	shl	d8, d8, #56                                // mod_constant
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          // AES block 2 - round 8
	cmp	w17, #12                                                     // setup flags for AES-128/192/256 check
	b.lt	Ldec_finish_prepretail_eor3                                           // branch if AES-128
	ldp	q27, q28, [x8, #144]                               // load rk9, rk10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 9
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 9
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 10
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 10
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 10
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 10
	b.eq	Ldec_finish_prepretail_eor3                                           // branch if AES-192
	ldp	q27, q28, [x8, #176]                               // load rk11, rk12
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b    // AES block 0 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b    // AES block 1 - round 11
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b    // AES block 2 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b    // AES block 3 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b       // AES block 0 - round 12
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b       // AES block 1 - round 12
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b       // AES block 2 - round 12
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b       // AES block 3 - round 12
Ldec_finish_prepretail_eor3:
	eor	v10.16b, v10.16b, v6.16b                                    // MODULO - karatsuba tidy up
	pmull	v7.1q, v9.1d, v8.1d                           // MODULO - top 64b align with mid
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor3	v10.16b, v10.16b, v7.16b, v9.16b                             // MODULO - fold into mid
	pmull	v8.1q, v10.1d, v8.1d                    // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	eor3	v11.16b, v11.16b, v8.16b, v10.16b                      // MODULO - fold into low
	aese	v1.16b, v31.16b                                             // AES block 1 - round N-1
	aese	v0.16b, v31.16b                                             // AES block 0 - round N-1
	aese	v3.16b, v31.16b                                             // AES block 3 - round N-1
	aese	v2.16b, v31.16b                                             // AES block 2 - round N-1
Ldec_tail_eor3:	//	TAIL
	sub	x5, x4, x0                    // main_end_input_ptr is number of bytes left to process
	ld1	{ v5.16b}, [x0], #16                                    // AES block 0 - load ciphertext
	eor	v0.16b, v5.16b, v0.16b                                    // AES block 0 - result
	mov	x6, v0.d[0]                                             // AES block 0 - mov low
	mov	x7, v0.d[1]                                             // AES block 0 - mov high
	ext	v8.16b, v11.16b, v11.16b, #8                                // prepare final partial tag
	eor	x6, x6, x13                                     // AES block 0 - round N low
	eor	x7, x7, x14                                     // AES block 0 - round N high
	cmp	x5, #48
	b.gt	Ldec_blocks_more_than_3_eor3
	mov	v3.16b, v2.16b
	movi	v10.8b, #0
	movi	v11.8b, #0
	movi	v9.8b, #0
	mov	v2.16b, v1.16b
	cmp	x5, #32
	b.gt	Ldec_blocks_more_than_2_eor3
	mov	v3.16b, v1.16b
	cmp	x5, #16
	b.gt	Ldec_blocks_more_than_1_eor3
	b	Ldec_blocks_less_than_1_eor3
Ldec_blocks_more_than_3_eor3:	//	blocks left >  3
	rev64	v4.16b, v5.16b                                             // GHASH final-3 block
	ld1	{ v5.16b}, [x0], #16                                    // AES final-2 block - load ciphertext
	stp	x6, x7, [x2], #16                         // AES final-3 block  - store result
	mov	d10, v17.d[1]                                                // GHASH final-3 block - mid
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	eor	v0.16b, v5.16b, v1.16b                                    // AES final-2 block - result
	mov	d22, v4.d[1]                                                  // GHASH final-3 block - mid
	mov	x6, v0.d[0]                                             // AES final-2 block - mov low
	mov	x7, v0.d[1]                                             // AES final-2 block - mov high
	eor	v22.8b, v22.8b, v4.8b                                       // GHASH final-3 block - mid
	movi	v8.8b, #0                                                        // suppress further partial tag feed in
	pmull2	v9.1q, v4.2d, v15.2d                                     // GHASH final-3 block - high
	pmull	v10.1q, v22.1d, v10.1d                                   // GHASH final-3 block - mid
	eor	x6, x6, x13                                     // AES final-2 block - round N low
	pmull	v11.1q, v4.1d, v15.1d                                      // GHASH final-3 block - low
	eor	x7, x7, x14                                     // AES final-2 block - round N high
Ldec_blocks_more_than_2_eor3:	//	blocks left >  2
	rev64	v4.16b, v5.16b                                             // GHASH final-2 block
	ld1	{ v5.16b}, [x0], #16                                    // AES final-1 block - load ciphertext
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	stp	x6, x7, [x2], #16                         // AES final-2 block  - store result
	eor	v0.16b, v5.16b, v2.16b                                    // AES final-1 block - result
	mov	d22, v4.d[1]                                                  // GHASH final-2 block - mid
	pmull	v21.1q, v4.1d, v14.1d                                         // GHASH final-2 block - low
	pmull2	v20.1q, v4.2d, v14.2d                                       // GHASH final-2 block - high
	eor	v22.8b, v22.8b, v4.8b                                       // GHASH final-2 block - mid
	mov	x6, v0.d[0]                                             // AES final-1 block - mov low
	mov	x7, v0.d[1]                                             // AES final-1 block - mov high
	eor	v11.16b, v11.16b, v21.16b                                   // GHASH final-2 block - low
	movi	v8.8b, #0                                                        // suppress further partial tag feed in
	pmull	v22.1q, v22.1d, v17.1d                                     // GHASH final-2 block - mid
	eor	v9.16b, v9.16b, v20.16b                                   // GHASH final-2 block - high
	eor	x6, x6, x13                                     // AES final-1 block - round N low
	eor	v10.16b, v10.16b, v22.16b                                  // GHASH final-2 block - mid
	eor	x7, x7, x14                                     // AES final-1 block - round N high
Ldec_blocks_more_than_1_eor3:	//	blocks left >  1
	stp	x6, x7, [x2], #16                         // AES final-1 block  - store result
	rev64	v4.16b, v5.16b                                             // GHASH final-1 block
	ld1	{ v5.16b}, [x0], #16                                    // AES final block - load ciphertext
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	movi	v8.8b, #0                                                        // suppress further partial tag feed in
	mov	d22, v4.d[1]                                                  // GHASH final-1 block - mid
	eor	v0.16b, v5.16b, v3.16b                                    // AES final block - result
	pmull2	v20.1q, v4.2d, v13.2d                                        // GHASH final-1 block - high
	eor	v22.8b, v22.8b, v4.8b                                       // GHASH final-1 block - mid
	pmull	v21.1q, v4.1d, v13.1d                                         // GHASH final-1 block - low
	mov	x6, v0.d[0]                                             // AES final block - mov low
	ins	v22.d[1], v22.d[0]                                             // GHASH final-1 block - mid
	mov	x7, v0.d[1]                                             // AES final block - mov high
	pmull2	v22.1q, v22.2d, v16.2d                                    // GHASH final-1 block - mid
	eor	x6, x6, x13                                     // AES final block - round N low
	eor	v11.16b, v11.16b, v21.16b                                   // GHASH final-1 block - low
	eor	v9.16b, v9.16b, v20.16b                                   // GHASH final-1 block - high
	eor	v10.16b, v10.16b, v22.16b                                  // GHASH final-1 block - mid
	eor	x7, x7, x14                                     // AES final block - round N high
Ldec_blocks_less_than_1_eor3:	//	blocks left <= 1
	add	x10, x10, x1, lsr #7                                    // Calculate the updated counter based on the number of 16B chunks we processed
	rev	w10, w10
	str	w10, [x16, #12]                                              // store the updated counter
	and	x1, x1, #127                                     // bit_length %= 128
	mvn	x14, xzr                                                        // rkN_h = 0xffffffffffffffff
	sub	x1, x1, #128                                     // bit_length -= 128
	mvn	x13, xzr                                                        // rkN_l = 0xffffffffffffffff
	ldp	x4, x5, [x2]                 // load existing bytes we need to not overwrite
	neg	x1, x1                                           // bit_length = 128 - #bits in input (in range [1,128])
	and	x1, x1, #127                                     // bit_length %= 128
	lsr	x14, x14, x1                                        // rkN_h is mask for top 64b of last block
	cmp	x1, #64
	csel	x9, x13, x14, lt
	csel	x10, x14, xzr, lt
	fmov	d0, x9                                                   // ctr0b is mask for last block
	and	x6, x6, x9
	mov	v0.d[1], x10
	bic	x4, x4, x9                            // mask out low existing bytes
	bic	x5, x5, x10              // mask out high existing bytes
	orr	x6, x6, x4
	and	x7, x7, x10
	orr	x7, x7, x5
	and	v5.16b, v5.16b, v0.16b                                    // possibly partial last block has zeroes in highest bits
	rev64	v4.16b, v5.16b                                             // GHASH final block
	eor	v4.16b, v4.16b, v8.16b                                      // feed in partial tag
	pmull	v21.1q, v4.1d, v12.1d                                         // GHASH final block - low
	mov	d8, v4.d[1]                                                   // GHASH final block - mid
	eor	v8.8b, v8.8b, v4.8b                                           // GHASH final block - mid
	pmull2	v20.1q, v4.2d, v12.2d                                        // GHASH final block - high
	pmull	v8.1q, v8.1d, v16.1d                                         // GHASH final block - mid
	eor	v9.16b, v9.16b, v20.16b                                   // GHASH final block - high
	eor	v11.16b, v11.16b, v21.16b                                   // GHASH final block - low
	eor	v10.16b, v10.16b, v8.16b                                    // GHASH final block - mid
	ldr	d8, [sp, #128]
	eor	v6.16b, v11.16b, v9.16b                                    // MODULO - karatsuba tidy up
	eor	v10.16b, v10.16b, v6.16b                                    // MODULO - karatsuba tidy up
	pmull	v7.1q, v9.1d, v8.1d                           // MODULO - top 64b align with mid
	ext	v9.16b, v9.16b, v9.16b, #8                             // MODULO - other top alignment
	eor	v10.16b, v10.16b, v7.16b                                 // MODULO - fold into mid
	eor	v10.16b, v10.16b, v9.16b                                 // MODULO - fold into mid
	pmull	v8.1q, v10.1d, v8.1d                    // MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                             // MODULO - other mid alignment
	eor	v11.16b, v11.16b, v8.16b                          // MODULO - fold into low
	stp	x6, x7, [x2]
	eor	v11.16b, v11.16b, v10.16b                                 // MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b                                           // Final Tag
	mov	x0, x15
	st1	{ v11.16b }, [x3]                                     // Store final tag
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	d8, d9, [sp, #64]
	ldp	d10, d11, [sp, #80]
	ldp	d12, d13, [sp, #96]
	ldp	d14, d15, [sp, #112]
	ldp	x29, x30, [sp], #224
	AARCH64_VALIDATE_LINK_REGISTER
	ret

#endif  // __ARM_MAX_ARCH__ >= 8
#endif  // !OPENSSL_NO_ASM && defined(OPENSSL_AARCH64) && defined(__APPLE__)
