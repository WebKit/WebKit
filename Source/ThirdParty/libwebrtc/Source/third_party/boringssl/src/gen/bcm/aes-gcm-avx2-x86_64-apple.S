// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.section	__DATA,__const

.p2align	4
aes_gcm_avx2_constants:

// A shuffle mask that reflects the bytes of 16-byte blocks
L$bswap_mask:
.quad	0x08090a0b0c0d0e0f, 0x0001020304050607

// This is the GHASH reducing polynomial without its constant term, i.e.
// x^128 + x^7 + x^2 + x, represented using the backwards mapping
// between bits and polynomial coefficients.
// 
// Alternatively, it can be interpreted as the naturally-ordered
// representation of the polynomial x^127 + x^126 + x^121 + 1, i.e. the
// "reversed" GHASH reducing polynomial without its x^128 term.
L$gfpoly:
.quad	1, 0xc200000000000000

// Same as above, but with the (1 << 64) bit set.
L$gfpoly_and_internal_carrybit:
.quad	1, 0xc200000000000001

.p2align	5
// The below constants are used for incrementing the counter blocks.
L$ctr_pattern:
.quad	0, 0
.quad	1, 0
L$inc_2blocks:
.quad	2, 0
.quad	2, 0

.text	
.globl	_gcm_init_vpclmulqdq_avx2
.private_extern _gcm_init_vpclmulqdq_avx2

.p2align	5
_gcm_init_vpclmulqdq_avx2:


_CET_ENDBR



// Load the byte-reflected hash subkey.  BoringSSL provides it in
// byte-reflected form except the two halves are in the wrong order.
	vpshufd	$0x4e,(%rsi),%xmm3

// Finish preprocessing the byte-reflected hash subkey by multiplying it by
// x^-1 ("standard" interpretation of polynomial coefficients) or
// equivalently x^1 (natural interpretation).  This gets the key into a
// format that avoids having to bit-reflect the data blocks later.
	vpshufd	$0xd3,%xmm3,%xmm0
	vpsrad	$31,%xmm0,%xmm0
	vpaddq	%xmm3,%xmm3,%xmm3
	vpand	L$gfpoly_and_internal_carrybit(%rip),%xmm0,%xmm0
	vpxor	%xmm0,%xmm3,%xmm3

	vbroadcasti128	L$gfpoly(%rip),%ymm6

// Square H^1 to get H^2.
	vpclmulqdq	$0x00,%xmm3,%xmm3,%xmm0  // LO = a_L * a_L
	vpclmulqdq	$0x11,%xmm3,%xmm3,%xmm5  // HI = a_H * a_H
	vpclmulqdq	$0x01,%xmm0,%xmm6,%xmm1  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm0,%xmm0  // Swap halves of LO
	vpxor	%xmm0,%xmm1,%xmm1  // Fold LO into MI
	vpclmulqdq	$0x01,%xmm1,%xmm6,%xmm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm1,%xmm1  // Swap halves of MI
	vpxor	%xmm1,%xmm5,%xmm5  // Fold MI into HI (part 1)
	vpxor	%xmm0,%xmm5,%xmm5  // Fold MI into HI (part 2)


// Create H_CUR = [H^2, H^1] and H_INC = [H^2, H^2].
	vinserti128	$1,%xmm3,%ymm5,%ymm3
	vinserti128	$1,%xmm5,%ymm5,%ymm5

// Compute H_CUR2 = [H^4, H^3].
	vpclmulqdq	$0x00,%ymm5,%ymm3,%ymm0  // LO = a_L * b_L
	vpclmulqdq	$0x01,%ymm5,%ymm3,%ymm1  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%ymm5,%ymm3,%ymm2  // MI_1 = a_H * b_L
	vpxor	%ymm2,%ymm1,%ymm1  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%ymm0,%ymm6,%ymm2  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm0,%ymm0  // Swap halves of LO
	vpxor	%ymm0,%ymm1,%ymm1  // Fold LO into MI (part 1)
	vpxor	%ymm2,%ymm1,%ymm1  // Fold LO into MI (part 2)
	vpclmulqdq	$0x11,%ymm5,%ymm3,%ymm4  // HI = a_H * b_H
	vpclmulqdq	$0x01,%ymm1,%ymm6,%ymm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm1,%ymm1  // Swap halves of MI
	vpxor	%ymm1,%ymm4,%ymm4  // Fold MI into HI (part 1)
	vpxor	%ymm0,%ymm4,%ymm4  // Fold MI into HI (part 2)


// Store [H^2, H^1] and [H^4, H^3].
	vmovdqu	%ymm3,96(%rdi)
	vmovdqu	%ymm4,64(%rdi)

// For Karatsuba multiplication: compute and store the two 64-bit halves of
// each key power XOR'd together.  Order is 4,2,3,1.
	vpunpcklqdq	%ymm3,%ymm4,%ymm0
	vpunpckhqdq	%ymm3,%ymm4,%ymm1
	vpxor	%ymm1,%ymm0,%ymm0
	vmovdqu	%ymm0,128+32(%rdi)

// Compute and store H_CUR = [H^6, H^5] and H_CUR2 = [H^8, H^7].
	vpclmulqdq	$0x00,%ymm5,%ymm4,%ymm0  // LO = a_L * b_L
	vpclmulqdq	$0x01,%ymm5,%ymm4,%ymm1  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%ymm5,%ymm4,%ymm2  // MI_1 = a_H * b_L
	vpxor	%ymm2,%ymm1,%ymm1  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%ymm0,%ymm6,%ymm2  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm0,%ymm0  // Swap halves of LO
	vpxor	%ymm0,%ymm1,%ymm1  // Fold LO into MI (part 1)
	vpxor	%ymm2,%ymm1,%ymm1  // Fold LO into MI (part 2)
	vpclmulqdq	$0x11,%ymm5,%ymm4,%ymm3  // HI = a_H * b_H
	vpclmulqdq	$0x01,%ymm1,%ymm6,%ymm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm1,%ymm1  // Swap halves of MI
	vpxor	%ymm1,%ymm3,%ymm3  // Fold MI into HI (part 1)
	vpxor	%ymm0,%ymm3,%ymm3  // Fold MI into HI (part 2)

	vpclmulqdq	$0x00,%ymm5,%ymm3,%ymm0  // LO = a_L * b_L
	vpclmulqdq	$0x01,%ymm5,%ymm3,%ymm1  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%ymm5,%ymm3,%ymm2  // MI_1 = a_H * b_L
	vpxor	%ymm2,%ymm1,%ymm1  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%ymm0,%ymm6,%ymm2  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm0,%ymm0  // Swap halves of LO
	vpxor	%ymm0,%ymm1,%ymm1  // Fold LO into MI (part 1)
	vpxor	%ymm2,%ymm1,%ymm1  // Fold LO into MI (part 2)
	vpclmulqdq	$0x11,%ymm5,%ymm3,%ymm4  // HI = a_H * b_H
	vpclmulqdq	$0x01,%ymm1,%ymm6,%ymm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm1,%ymm1  // Swap halves of MI
	vpxor	%ymm1,%ymm4,%ymm4  // Fold MI into HI (part 1)
	vpxor	%ymm0,%ymm4,%ymm4  // Fold MI into HI (part 2)

	vmovdqu	%ymm3,32(%rdi)
	vmovdqu	%ymm4,0(%rdi)

// Again, compute and store the two 64-bit halves of each key power XOR'd
// together.  Order is 8,6,7,5.
	vpunpcklqdq	%ymm3,%ymm4,%ymm0
	vpunpckhqdq	%ymm3,%ymm4,%ymm1
	vpxor	%ymm1,%ymm0,%ymm0
	vmovdqu	%ymm0,128(%rdi)

	vzeroupper
	ret



.globl	_gcm_gmult_vpclmulqdq_avx2
.private_extern _gcm_gmult_vpclmulqdq_avx2

.p2align	5
.alt_entry _gcm_gmult_vpclmulqdq_avx2
_gcm_gmult_vpclmulqdq_avx2:


_CET_ENDBR



	vmovdqu	(%rdi),%xmm0
	vmovdqu	L$bswap_mask(%rip),%xmm1
	vmovdqu	128-16(%rsi),%xmm2
	vmovdqu	L$gfpoly(%rip),%xmm3
	vpshufb	%xmm1,%xmm0,%xmm0

	vpclmulqdq	$0x00,%xmm2,%xmm0,%xmm4  // LO = a_L * b_L
	vpclmulqdq	$0x01,%xmm2,%xmm0,%xmm5  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%xmm2,%xmm0,%xmm6  // MI_1 = a_H * b_L
	vpxor	%xmm6,%xmm5,%xmm5  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%xmm4,%xmm3,%xmm6  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm4,%xmm4  // Swap halves of LO
	vpxor	%xmm4,%xmm5,%xmm5  // Fold LO into MI (part 1)
	vpxor	%xmm6,%xmm5,%xmm5  // Fold LO into MI (part 2)
	vpclmulqdq	$0x11,%xmm2,%xmm0,%xmm0  // HI = a_H * b_H
	vpclmulqdq	$0x01,%xmm5,%xmm3,%xmm4  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm5,%xmm5  // Swap halves of MI
	vpxor	%xmm5,%xmm0,%xmm0  // Fold MI into HI (part 1)
	vpxor	%xmm4,%xmm0,%xmm0  // Fold MI into HI (part 2)


	vpshufb	%xmm1,%xmm0,%xmm0
	vmovdqu	%xmm0,(%rdi)

// No need for vzeroupper, since only xmm registers were used.
	ret



.globl	_gcm_ghash_vpclmulqdq_avx2
.private_extern _gcm_ghash_vpclmulqdq_avx2

.p2align	5
.alt_entry _gcm_ghash_vpclmulqdq_avx2
_gcm_ghash_vpclmulqdq_avx2:


_CET_ENDBR



// Load the bswap_mask and gfpoly constants.  Since AADLEN is usually small,
// usually only 128-bit vectors will be used.  So as an optimization, don't
// broadcast these constants to both 128-bit lanes quite yet.
	vmovdqu	L$bswap_mask(%rip),%xmm6
	vmovdqu	L$gfpoly(%rip),%xmm7

// Load the GHASH accumulator.
	vmovdqu	(%rdi),%xmm5
	vpshufb	%xmm6,%xmm5,%xmm5

// Optimize for AADLEN < 32 by checking for AADLEN < 32 before AADLEN < 128.
	cmpq	$32,%rcx
	jb	L$ghash_lastblock

// AADLEN >= 32, so we'll operate on full vectors.  Broadcast bswap_mask and
// gfpoly to both 128-bit lanes.
	vinserti128	$1,%xmm6,%ymm6,%ymm6
	vinserti128	$1,%xmm7,%ymm7,%ymm7

	cmpq	$127,%rcx
	jbe	L$ghash_loop_1x

// Update GHASH with 128 bytes of AAD at a time.
	vmovdqu	128(%rsi),%ymm8
	vmovdqu	128+32(%rsi),%ymm9
L$ghash_loop_4x:
// First vector
	vmovdqu	0(%rdx),%ymm1
	vpshufb	%ymm6,%ymm1,%ymm1
	vmovdqu	0(%rsi),%ymm2
	vpxor	%ymm5,%ymm1,%ymm1
	vpclmulqdq	$0x00,%ymm2,%ymm1,%ymm3
	vpclmulqdq	$0x11,%ymm2,%ymm1,%ymm5
	vpunpckhqdq	%ymm1,%ymm1,%ymm0
	vpxor	%ymm1,%ymm0,%ymm0
	vpclmulqdq	$0x00,%ymm8,%ymm0,%ymm4
// Second vector
	vmovdqu	32(%rdx),%ymm1
	vpshufb	%ymm6,%ymm1,%ymm1
	vmovdqu	32(%rsi),%ymm2
	vpclmulqdq	$0x00,%ymm2,%ymm1,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vpclmulqdq	$0x11,%ymm2,%ymm1,%ymm0
	vpxor	%ymm0,%ymm5,%ymm5
	vpunpckhqdq	%ymm1,%ymm1,%ymm0
	vpxor	%ymm1,%ymm0,%ymm0
	vpclmulqdq	$0x10,%ymm8,%ymm0,%ymm0
	vpxor	%ymm0,%ymm4,%ymm4
// Third vector
	vmovdqu	64(%rdx),%ymm1
	vpshufb	%ymm6,%ymm1,%ymm1
	vmovdqu	64(%rsi),%ymm2
	vpclmulqdq	$0x00,%ymm2,%ymm1,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vpclmulqdq	$0x11,%ymm2,%ymm1,%ymm0
	vpxor	%ymm0,%ymm5,%ymm5
	vpunpckhqdq	%ymm1,%ymm1,%ymm0
	vpxor	%ymm1,%ymm0,%ymm0
	vpclmulqdq	$0x00,%ymm9,%ymm0,%ymm0
	vpxor	%ymm0,%ymm4,%ymm4

// Fourth vector
	vmovdqu	96(%rdx),%ymm1
	vpshufb	%ymm6,%ymm1,%ymm1
	vmovdqu	96(%rsi),%ymm2
	vpclmulqdq	$0x00,%ymm2,%ymm1,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vpclmulqdq	$0x11,%ymm2,%ymm1,%ymm0
	vpxor	%ymm0,%ymm5,%ymm5
	vpunpckhqdq	%ymm1,%ymm1,%ymm0
	vpxor	%ymm1,%ymm0,%ymm0
	vpclmulqdq	$0x10,%ymm9,%ymm0,%ymm0
	vpxor	%ymm0,%ymm4,%ymm4
// Finalize 'mi' following Karatsuba multiplication.
	vpxor	%ymm3,%ymm4,%ymm4
	vpxor	%ymm5,%ymm4,%ymm4

// Fold lo into mi.
	vbroadcasti128	L$gfpoly(%rip),%ymm2
	vpclmulqdq	$0x01,%ymm3,%ymm2,%ymm0
	vpshufd	$0x4e,%ymm3,%ymm3
	vpxor	%ymm3,%ymm4,%ymm4
	vpxor	%ymm0,%ymm4,%ymm4
// Fold mi into hi.
	vpclmulqdq	$0x01,%ymm4,%ymm2,%ymm0
	vpshufd	$0x4e,%ymm4,%ymm4
	vpxor	%ymm4,%ymm5,%ymm5
	vpxor	%ymm0,%ymm5,%ymm5
	vextracti128	$1,%ymm5,%xmm0
	vpxor	%xmm0,%xmm5,%xmm5

	subq	$-128,%rdx  // 128 is 4 bytes, -128 is 1 byte
	addq	$-128,%rcx
	cmpq	$127,%rcx
	ja	L$ghash_loop_4x

// Update GHASH with 32 bytes of AAD at a time.
	cmpq	$32,%rcx
	jb	L$ghash_loop_1x_done
L$ghash_loop_1x:
	vmovdqu	(%rdx),%ymm0
	vpshufb	%ymm6,%ymm0,%ymm0
	vpxor	%ymm0,%ymm5,%ymm5
	vmovdqu	128-32(%rsi),%ymm0
	vpclmulqdq	$0x00,%ymm0,%ymm5,%ymm1  // LO = a_L * b_L
	vpclmulqdq	$0x01,%ymm0,%ymm5,%ymm2  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%ymm0,%ymm5,%ymm3  // MI_1 = a_H * b_L
	vpxor	%ymm3,%ymm2,%ymm2  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%ymm1,%ymm7,%ymm3  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm1,%ymm1  // Swap halves of LO
	vpxor	%ymm1,%ymm2,%ymm2  // Fold LO into MI (part 1)
	vpxor	%ymm3,%ymm2,%ymm2  // Fold LO into MI (part 2)
	vpclmulqdq	$0x11,%ymm0,%ymm5,%ymm5  // HI = a_H * b_H
	vpclmulqdq	$0x01,%ymm2,%ymm7,%ymm1  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm2,%ymm2  // Swap halves of MI
	vpxor	%ymm2,%ymm5,%ymm5  // Fold MI into HI (part 1)
	vpxor	%ymm1,%ymm5,%ymm5  // Fold MI into HI (part 2)

	vextracti128	$1,%ymm5,%xmm0
	vpxor	%xmm0,%xmm5,%xmm5
	addq	$32,%rdx
	subq	$32,%rcx
	cmpq	$32,%rcx
	jae	L$ghash_loop_1x
L$ghash_loop_1x_done:

// Update GHASH with the remaining 16-byte block if any.
L$ghash_lastblock:
	testq	%rcx,%rcx
	jz	L$ghash_done
	vmovdqu	(%rdx),%xmm0
	vpshufb	%xmm6,%xmm0,%xmm0
	vpxor	%xmm0,%xmm5,%xmm5
	vmovdqu	128-16(%rsi),%xmm0
	vpclmulqdq	$0x00,%xmm0,%xmm5,%xmm1  // LO = a_L * b_L
	vpclmulqdq	$0x01,%xmm0,%xmm5,%xmm2  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%xmm0,%xmm5,%xmm3  // MI_1 = a_H * b_L
	vpxor	%xmm3,%xmm2,%xmm2  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%xmm1,%xmm7,%xmm3  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm1,%xmm1  // Swap halves of LO
	vpxor	%xmm1,%xmm2,%xmm2  // Fold LO into MI (part 1)
	vpxor	%xmm3,%xmm2,%xmm2  // Fold LO into MI (part 2)
	vpclmulqdq	$0x11,%xmm0,%xmm5,%xmm5  // HI = a_H * b_H
	vpclmulqdq	$0x01,%xmm2,%xmm7,%xmm1  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm2,%xmm2  // Swap halves of MI
	vpxor	%xmm2,%xmm5,%xmm5  // Fold MI into HI (part 1)
	vpxor	%xmm1,%xmm5,%xmm5  // Fold MI into HI (part 2)


L$ghash_done:
// Store the updated GHASH accumulator back to memory.
	vpshufb	%xmm6,%xmm5,%xmm5
	vmovdqu	%xmm5,(%rdi)

	vzeroupper
	ret



.globl	_aes_gcm_enc_update_vaes_avx2
.private_extern _aes_gcm_enc_update_vaes_avx2

.p2align	5
.alt_entry _aes_gcm_enc_update_vaes_avx2
_aes_gcm_enc_update_vaes_avx2:


_CET_ENDBR
	pushq	%r12


	movq	16(%rsp),%r12  // arg7
#ifdef BORINGSSL_DISPATCH_TEST

	movb	$1,_BORINGSSL_function_hit+6(%rip)
#endif
	vbroadcasti128	L$bswap_mask(%rip),%ymm0

// Load the GHASH accumulator and the starting counter.
// BoringSSL passes these values in big endian format.
	vmovdqu	(%r12),%xmm1
	vpshufb	%xmm0,%xmm1,%xmm1
	vbroadcasti128	(%r8),%ymm11
	vpshufb	%ymm0,%ymm11,%ymm11

// Load the AES key length in bytes.  BoringSSL stores number of rounds
// minus 1, so convert using: AESKEYLEN = 4 * aeskey->rounds - 20.
	movl	240(%rcx),%r10d
	leal	-20(,%r10,4),%r10d

// Make RNDKEYLAST_PTR point to the last AES round key.  This is the
// round key with index 10, 12, or 14 for AES-128, AES-192, or AES-256
// respectively.  Then load the zero-th and last round keys.
	leaq	96(%rcx,%r10,4),%r11
	vbroadcasti128	(%rcx),%ymm9
	vbroadcasti128	(%r11),%ymm10

// Finish initializing LE_CTR by adding 1 to the second block.
	vpaddd	L$ctr_pattern(%rip),%ymm11,%ymm11

// If there are at least 128 bytes of data, then continue into the loop that
// processes 128 bytes of data at a time.  Otherwise skip it.
	cmpq	$127,%rdx
	jbe	L$crypt_loop_4x_done__func1

	vmovdqu	128(%r9),%ymm7
	vmovdqu	128+32(%r9),%ymm8
// Encrypt the first 4 vectors of plaintext blocks.
// Increment le_ctr four times to generate four vectors of little-endian
// counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vmovdqu	L$inc_2blocks(%rip),%ymm2
	vpshufb	%ymm0,%ymm11,%ymm12
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm13
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm14
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm15
	vpaddd	%ymm2,%ymm11,%ymm11

// AES "round zero": XOR in the zero-th round key.
	vpxor	%ymm9,%ymm12,%ymm12
	vpxor	%ymm9,%ymm13,%ymm13
	vpxor	%ymm9,%ymm14,%ymm14
	vpxor	%ymm9,%ymm15,%ymm15

	leaq	16(%rcx),%rax
L$vaesenc_loop_first_4_vecs__func1:
	vbroadcasti128	(%rax),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	addq	$16,%rax
	cmpq	%rax,%r11
	jne	L$vaesenc_loop_first_4_vecs__func1
	vpxor	0(%rdi),%ymm10,%ymm2
	vpxor	32(%rdi),%ymm10,%ymm3
	vpxor	64(%rdi),%ymm10,%ymm5
	vpxor	96(%rdi),%ymm10,%ymm6
	vaesenclast	%ymm2,%ymm12,%ymm12
	vaesenclast	%ymm3,%ymm13,%ymm13
	vaesenclast	%ymm5,%ymm14,%ymm14
	vaesenclast	%ymm6,%ymm15,%ymm15
	vmovdqu	%ymm12,0(%rsi)
	vmovdqu	%ymm13,32(%rsi)
	vmovdqu	%ymm14,64(%rsi)
	vmovdqu	%ymm15,96(%rsi)

	subq	$-128,%rdi  // 128 is 4 bytes, -128 is 1 byte
	addq	$-128,%rdx
	cmpq	$127,%rdx
	jbe	L$ghash_last_ciphertext_4x__func1
.p2align	4
L$crypt_loop_4x__func1:

// Start the AES encryption of the counter blocks.
// Increment le_ctr four times to generate four vectors of little-endian
// counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vmovdqu	L$inc_2blocks(%rip),%ymm2
	vpshufb	%ymm0,%ymm11,%ymm12
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm13
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm14
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm15
	vpaddd	%ymm2,%ymm11,%ymm11

// AES "round zero": XOR in the zero-th round key.
	vpxor	%ymm9,%ymm12,%ymm12
	vpxor	%ymm9,%ymm13,%ymm13
	vpxor	%ymm9,%ymm14,%ymm14
	vpxor	%ymm9,%ymm15,%ymm15

	cmpl	$24,%r10d
	jl	L$aes128__func1
	je	L$aes192__func1
// AES-256
	vbroadcasti128	-208(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vbroadcasti128	-192(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

L$aes192__func1:
	vbroadcasti128	-176(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vbroadcasti128	-160(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

L$aes128__func1:
	prefetcht0	512(%rdi)
	prefetcht0	512+64(%rdi)
// First vector
	vmovdqu	0(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	0(%r9),%ymm4
	vpxor	%ymm1,%ymm3,%ymm3
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x00,%ymm7,%ymm2,%ymm6

	vbroadcasti128	-144(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15


	vbroadcasti128	-128(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Second vector
	vmovdqu	32(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	32(%r9),%ymm4
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x10,%ymm7,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6

	vbroadcasti128	-112(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Third vector
	vmovdqu	64(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	64(%r9),%ymm4

	vbroadcasti128	-96(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1

	vbroadcasti128	-80(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x00,%ymm8,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6

// Fourth vector
	vmovdqu	96(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3

	vbroadcasti128	-64(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vmovdqu	96(%r9),%ymm4
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x10,%ymm8,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6

	vbroadcasti128	-48(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Finalize 'mi' following Karatsuba multiplication.
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm1,%ymm6,%ymm6

// Fold lo into mi.
	vbroadcasti128	L$gfpoly(%rip),%ymm4
	vpclmulqdq	$0x01,%ymm5,%ymm4,%ymm2
	vpshufd	$0x4e,%ymm5,%ymm5
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm2,%ymm6,%ymm6

	vbroadcasti128	-32(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Fold mi into hi.
	vpclmulqdq	$0x01,%ymm6,%ymm4,%ymm2
	vpshufd	$0x4e,%ymm6,%ymm6
	vpxor	%ymm6,%ymm1,%ymm1
	vpxor	%ymm2,%ymm1,%ymm1

	vbroadcasti128	-16(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vextracti128	$1,%ymm1,%xmm2
	vpxor	%xmm2,%xmm1,%xmm1


	subq	$-128,%rsi  // 128 is 4 bytes, -128 is 1 byte
	vpxor	0(%rdi),%ymm10,%ymm2
	vpxor	32(%rdi),%ymm10,%ymm3
	vpxor	64(%rdi),%ymm10,%ymm5
	vpxor	96(%rdi),%ymm10,%ymm6
	vaesenclast	%ymm2,%ymm12,%ymm12
	vaesenclast	%ymm3,%ymm13,%ymm13
	vaesenclast	%ymm5,%ymm14,%ymm14
	vaesenclast	%ymm6,%ymm15,%ymm15
	vmovdqu	%ymm12,0(%rsi)
	vmovdqu	%ymm13,32(%rsi)
	vmovdqu	%ymm14,64(%rsi)
	vmovdqu	%ymm15,96(%rsi)

	subq	$-128,%rdi

	addq	$-128,%rdx
	cmpq	$127,%rdx
	ja	L$crypt_loop_4x__func1
L$ghash_last_ciphertext_4x__func1:
// First vector
	vmovdqu	0(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	0(%r9),%ymm4
	vpxor	%ymm1,%ymm3,%ymm3
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x00,%ymm7,%ymm2,%ymm6
// Second vector
	vmovdqu	32(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	32(%r9),%ymm4
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x10,%ymm7,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6
// Third vector
	vmovdqu	64(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	64(%r9),%ymm4
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x00,%ymm8,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6

// Fourth vector
	vmovdqu	96(%rsi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	96(%r9),%ymm4
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x10,%ymm8,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6
// Finalize 'mi' following Karatsuba multiplication.
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm1,%ymm6,%ymm6

// Fold lo into mi.
	vbroadcasti128	L$gfpoly(%rip),%ymm4
	vpclmulqdq	$0x01,%ymm5,%ymm4,%ymm2
	vpshufd	$0x4e,%ymm5,%ymm5
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm2,%ymm6,%ymm6
// Fold mi into hi.
	vpclmulqdq	$0x01,%ymm6,%ymm4,%ymm2
	vpshufd	$0x4e,%ymm6,%ymm6
	vpxor	%ymm6,%ymm1,%ymm1
	vpxor	%ymm2,%ymm1,%ymm1
	vextracti128	$1,%ymm1,%xmm2
	vpxor	%xmm2,%xmm1,%xmm1

	subq	$-128,%rsi
L$crypt_loop_4x_done__func1:
// Check whether any data remains.
	testq	%rdx,%rdx
	jz	L$done__func1

// DATALEN is in [16, 32, 48, 64, 80, 96, 112].

// Make POWERS_PTR point to the key powers [H^N, H^(N-1), ...] where N
// is the number of blocks that remain.
	leaq	128(%r9),%r8
	subq	%rdx,%r8

// Start collecting the unreduced GHASH intermediate value LO, MI, HI.
	vpxor	%xmm5,%xmm5,%xmm5
	vpxor	%xmm6,%xmm6,%xmm6
	vpxor	%xmm7,%xmm7,%xmm7

	cmpq	$64,%rdx
	jb	L$lessthan64bytes__func1

// DATALEN is in [64, 80, 96, 112].  Encrypt two vectors of counter blocks.
	vpshufb	%ymm0,%ymm11,%ymm12
	vpaddd	L$inc_2blocks(%rip),%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm13
	vpaddd	L$inc_2blocks(%rip),%ymm11,%ymm11
	vpxor	%ymm9,%ymm12,%ymm12
	vpxor	%ymm9,%ymm13,%ymm13
	leaq	16(%rcx),%rax
L$vaesenc_loop_tail_1__func1:
	vbroadcasti128	(%rax),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	L$vaesenc_loop_tail_1__func1
	vaesenclast	%ymm10,%ymm12,%ymm12
	vaesenclast	%ymm10,%ymm13,%ymm13

// XOR the data with the two vectors of keystream blocks.
	vmovdqu	0(%rdi),%ymm2
	vmovdqu	32(%rdi),%ymm3
	vpxor	%ymm2,%ymm12,%ymm12
	vpxor	%ymm3,%ymm13,%ymm13
	vmovdqu	%ymm12,0(%rsi)
	vmovdqu	%ymm13,32(%rsi)

// Update GHASH with two vectors of ciphertext blocks, without reducing.
	vpshufb	%ymm0,%ymm12,%ymm12
	vpshufb	%ymm0,%ymm13,%ymm13
	vpxor	%ymm1,%ymm12,%ymm12
	vmovdqu	(%r8),%ymm2
	vmovdqu	32(%r8),%ymm3
	vpclmulqdq	$0x00,%ymm2,%ymm12,%ymm5
	vpclmulqdq	$0x01,%ymm2,%ymm12,%ymm6
	vpclmulqdq	$0x10,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%ymm2,%ymm12,%ymm7
	vpclmulqdq	$0x00,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm5,%ymm5
	vpclmulqdq	$0x01,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x10,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm7,%ymm7

	addq	$64,%r8
	addq	$64,%rdi
	addq	$64,%rsi
	subq	$64,%rdx
	jz	L$reduce__func1

	vpxor	%xmm1,%xmm1,%xmm1

// DATALEN is in [16, 32, 48].  Encrypt two last vectors of counter blocks.
L$lessthan64bytes__func1:
	vpshufb	%ymm0,%ymm11,%ymm12
	vpaddd	L$inc_2blocks(%rip),%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm13
	vpxor	%ymm9,%ymm12,%ymm12
	vpxor	%ymm9,%ymm13,%ymm13
	leaq	16(%rcx),%rax
L$vaesenc_loop_tail_2__func1:
	vbroadcasti128	(%rax),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	L$vaesenc_loop_tail_2__func1
	vaesenclast	%ymm10,%ymm12,%ymm12
	vaesenclast	%ymm10,%ymm13,%ymm13

// XOR the remaining data with the keystream blocks, and update GHASH with
// the remaining ciphertext blocks without reducing.

	cmpq	$32,%rdx
	jb	L$xor_one_block__func1
	je	L$xor_two_blocks__func1

L$xor_three_blocks__func1:
	vmovdqu	0(%rdi),%ymm2
	vmovdqu	32(%rdi),%xmm3
	vpxor	%ymm2,%ymm12,%ymm12
	vpxor	%xmm3,%xmm13,%xmm13
	vmovdqu	%ymm12,0(%rsi)
	vmovdqu	%xmm13,32(%rsi)

	vpshufb	%ymm0,%ymm12,%ymm12
	vpshufb	%xmm0,%xmm13,%xmm13
	vpxor	%ymm1,%ymm12,%ymm12
	vmovdqu	(%r8),%ymm2
	vmovdqu	32(%r8),%xmm3
	vpclmulqdq	$0x00,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm5,%ymm5
	vpclmulqdq	$0x01,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x10,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm7,%ymm7
	jmp	L$ghash_mul_one_vec_unreduced__func1

L$xor_two_blocks__func1:
	vmovdqu	(%rdi),%ymm2
	vpxor	%ymm2,%ymm12,%ymm12
	vmovdqu	%ymm12,(%rsi)
	vpshufb	%ymm0,%ymm12,%ymm12
	vpxor	%ymm1,%ymm12,%ymm12
	vmovdqu	(%r8),%ymm2
	jmp	L$ghash_mul_one_vec_unreduced__func1

L$xor_one_block__func1:
	vmovdqu	(%rdi),%xmm2
	vpxor	%xmm2,%xmm12,%xmm12
	vmovdqu	%xmm12,(%rsi)
	vpshufb	%xmm0,%xmm12,%xmm12
	vpxor	%xmm1,%xmm12,%xmm12
	vmovdqu	(%r8),%xmm2

L$ghash_mul_one_vec_unreduced__func1:
	vpclmulqdq	$0x00,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm5,%ymm5
	vpclmulqdq	$0x01,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x10,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm7,%ymm7

L$reduce__func1:
// Finally, do the GHASH reduction.
	vbroadcasti128	L$gfpoly(%rip),%ymm2
	vpclmulqdq	$0x01,%ymm5,%ymm2,%ymm3
	vpshufd	$0x4e,%ymm5,%ymm5
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm3,%ymm6,%ymm6
	vpclmulqdq	$0x01,%ymm6,%ymm2,%ymm3
	vpshufd	$0x4e,%ymm6,%ymm6
	vpxor	%ymm6,%ymm7,%ymm7
	vpxor	%ymm3,%ymm7,%ymm7
	vextracti128	$1,%ymm7,%xmm1
	vpxor	%xmm7,%xmm1,%xmm1

L$done__func1:
// Store the updated GHASH accumulator back to memory.
	vpshufb	%xmm0,%xmm1,%xmm1
	vmovdqu	%xmm1,(%r12)

	vzeroupper
	popq	%r12

	ret



.globl	_aes_gcm_dec_update_vaes_avx2
.private_extern _aes_gcm_dec_update_vaes_avx2

.p2align	5
.alt_entry _aes_gcm_dec_update_vaes_avx2
_aes_gcm_dec_update_vaes_avx2:


_CET_ENDBR
	pushq	%r12


	movq	16(%rsp),%r12  // arg7
	vbroadcasti128	L$bswap_mask(%rip),%ymm0

// Load the GHASH accumulator and the starting counter.
// BoringSSL passes these values in big endian format.
	vmovdqu	(%r12),%xmm1
	vpshufb	%xmm0,%xmm1,%xmm1
	vbroadcasti128	(%r8),%ymm11
	vpshufb	%ymm0,%ymm11,%ymm11

// Load the AES key length in bytes.  BoringSSL stores number of rounds
// minus 1, so convert using: AESKEYLEN = 4 * aeskey->rounds - 20.
	movl	240(%rcx),%r10d
	leal	-20(,%r10,4),%r10d

// Make RNDKEYLAST_PTR point to the last AES round key.  This is the
// round key with index 10, 12, or 14 for AES-128, AES-192, or AES-256
// respectively.  Then load the zero-th and last round keys.
	leaq	96(%rcx,%r10,4),%r11
	vbroadcasti128	(%rcx),%ymm9
	vbroadcasti128	(%r11),%ymm10

// Finish initializing LE_CTR by adding 1 to the second block.
	vpaddd	L$ctr_pattern(%rip),%ymm11,%ymm11

// If there are at least 128 bytes of data, then continue into the loop that
// processes 128 bytes of data at a time.  Otherwise skip it.
	cmpq	$127,%rdx
	jbe	L$crypt_loop_4x_done__func2

	vmovdqu	128(%r9),%ymm7
	vmovdqu	128+32(%r9),%ymm8
.p2align	4
L$crypt_loop_4x__func2:

// Start the AES encryption of the counter blocks.
// Increment le_ctr four times to generate four vectors of little-endian
// counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vmovdqu	L$inc_2blocks(%rip),%ymm2
	vpshufb	%ymm0,%ymm11,%ymm12
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm13
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm14
	vpaddd	%ymm2,%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm15
	vpaddd	%ymm2,%ymm11,%ymm11

// AES "round zero": XOR in the zero-th round key.
	vpxor	%ymm9,%ymm12,%ymm12
	vpxor	%ymm9,%ymm13,%ymm13
	vpxor	%ymm9,%ymm14,%ymm14
	vpxor	%ymm9,%ymm15,%ymm15

	cmpl	$24,%r10d
	jl	L$aes128__func2
	je	L$aes192__func2
// AES-256
	vbroadcasti128	-208(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vbroadcasti128	-192(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

L$aes192__func2:
	vbroadcasti128	-176(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vbroadcasti128	-160(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

L$aes128__func2:
	prefetcht0	512(%rdi)
	prefetcht0	512+64(%rdi)
// First vector
	vmovdqu	0(%rdi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	0(%r9),%ymm4
	vpxor	%ymm1,%ymm3,%ymm3
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x00,%ymm7,%ymm2,%ymm6

	vbroadcasti128	-144(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15


	vbroadcasti128	-128(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Second vector
	vmovdqu	32(%rdi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	32(%r9),%ymm4
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x10,%ymm7,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6

	vbroadcasti128	-112(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Third vector
	vmovdqu	64(%rdi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3
	vmovdqu	64(%r9),%ymm4

	vbroadcasti128	-96(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1

	vbroadcasti128	-80(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x00,%ymm8,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6

// Fourth vector
	vmovdqu	96(%rdi),%ymm3
	vpshufb	%ymm0,%ymm3,%ymm3

	vbroadcasti128	-64(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vmovdqu	96(%r9),%ymm4
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm5,%ymm5
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vpunpckhqdq	%ymm3,%ymm3,%ymm2
	vpxor	%ymm3,%ymm2,%ymm2
	vpclmulqdq	$0x10,%ymm8,%ymm2,%ymm2
	vpxor	%ymm2,%ymm6,%ymm6

	vbroadcasti128	-48(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Finalize 'mi' following Karatsuba multiplication.
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm1,%ymm6,%ymm6

// Fold lo into mi.
	vbroadcasti128	L$gfpoly(%rip),%ymm4
	vpclmulqdq	$0x01,%ymm5,%ymm4,%ymm2
	vpshufd	$0x4e,%ymm5,%ymm5
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm2,%ymm6,%ymm6

	vbroadcasti128	-32(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

// Fold mi into hi.
	vpclmulqdq	$0x01,%ymm6,%ymm4,%ymm2
	vpshufd	$0x4e,%ymm6,%ymm6
	vpxor	%ymm6,%ymm1,%ymm1
	vpxor	%ymm2,%ymm1,%ymm1

	vbroadcasti128	-16(%r11),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	vaesenc	%ymm2,%ymm14,%ymm14
	vaesenc	%ymm2,%ymm15,%ymm15

	vextracti128	$1,%ymm1,%xmm2
	vpxor	%xmm2,%xmm1,%xmm1


// 128 is 4 bytes, -128 is 1 byte
	vpxor	0(%rdi),%ymm10,%ymm2
	vpxor	32(%rdi),%ymm10,%ymm3
	vpxor	64(%rdi),%ymm10,%ymm5
	vpxor	96(%rdi),%ymm10,%ymm6
	vaesenclast	%ymm2,%ymm12,%ymm12
	vaesenclast	%ymm3,%ymm13,%ymm13
	vaesenclast	%ymm5,%ymm14,%ymm14
	vaesenclast	%ymm6,%ymm15,%ymm15
	vmovdqu	%ymm12,0(%rsi)
	vmovdqu	%ymm13,32(%rsi)
	vmovdqu	%ymm14,64(%rsi)
	vmovdqu	%ymm15,96(%rsi)

	subq	$-128,%rdi
	subq	$-128,%rsi
	addq	$-128,%rdx
	cmpq	$127,%rdx
	ja	L$crypt_loop_4x__func2
L$crypt_loop_4x_done__func2:
// Check whether any data remains.
	testq	%rdx,%rdx
	jz	L$done__func2

// DATALEN is in [16, 32, 48, 64, 80, 96, 112].

// Make POWERS_PTR point to the key powers [H^N, H^(N-1), ...] where N
// is the number of blocks that remain.
	leaq	128(%r9),%r8
	subq	%rdx,%r8

// Start collecting the unreduced GHASH intermediate value LO, MI, HI.
	vpxor	%xmm5,%xmm5,%xmm5
	vpxor	%xmm6,%xmm6,%xmm6
	vpxor	%xmm7,%xmm7,%xmm7

	cmpq	$64,%rdx
	jb	L$lessthan64bytes__func2

// DATALEN is in [64, 80, 96, 112].  Encrypt two vectors of counter blocks.
	vpshufb	%ymm0,%ymm11,%ymm12
	vpaddd	L$inc_2blocks(%rip),%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm13
	vpaddd	L$inc_2blocks(%rip),%ymm11,%ymm11
	vpxor	%ymm9,%ymm12,%ymm12
	vpxor	%ymm9,%ymm13,%ymm13
	leaq	16(%rcx),%rax
L$vaesenc_loop_tail_1__func2:
	vbroadcasti128	(%rax),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	L$vaesenc_loop_tail_1__func2
	vaesenclast	%ymm10,%ymm12,%ymm12
	vaesenclast	%ymm10,%ymm13,%ymm13

// XOR the data with the two vectors of keystream blocks.
	vmovdqu	0(%rdi),%ymm2
	vmovdqu	32(%rdi),%ymm3
	vpxor	%ymm2,%ymm12,%ymm12
	vpxor	%ymm3,%ymm13,%ymm13
	vmovdqu	%ymm12,0(%rsi)
	vmovdqu	%ymm13,32(%rsi)

// Update GHASH with two vectors of ciphertext blocks, without reducing.
	vpshufb	%ymm0,%ymm2,%ymm12
	vpshufb	%ymm0,%ymm3,%ymm13
	vpxor	%ymm1,%ymm12,%ymm12
	vmovdqu	(%r8),%ymm2
	vmovdqu	32(%r8),%ymm3
	vpclmulqdq	$0x00,%ymm2,%ymm12,%ymm5
	vpclmulqdq	$0x01,%ymm2,%ymm12,%ymm6
	vpclmulqdq	$0x10,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%ymm2,%ymm12,%ymm7
	vpclmulqdq	$0x00,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm5,%ymm5
	vpclmulqdq	$0x01,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x10,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%ymm3,%ymm13,%ymm4
	vpxor	%ymm4,%ymm7,%ymm7

	addq	$64,%r8
	addq	$64,%rdi
	addq	$64,%rsi
	subq	$64,%rdx
	jz	L$reduce__func2

	vpxor	%xmm1,%xmm1,%xmm1

// DATALEN is in [16, 32, 48].  Encrypt two last vectors of counter blocks.
L$lessthan64bytes__func2:
	vpshufb	%ymm0,%ymm11,%ymm12
	vpaddd	L$inc_2blocks(%rip),%ymm11,%ymm11
	vpshufb	%ymm0,%ymm11,%ymm13
	vpxor	%ymm9,%ymm12,%ymm12
	vpxor	%ymm9,%ymm13,%ymm13
	leaq	16(%rcx),%rax
L$vaesenc_loop_tail_2__func2:
	vbroadcasti128	(%rax),%ymm2
	vaesenc	%ymm2,%ymm12,%ymm12
	vaesenc	%ymm2,%ymm13,%ymm13
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	L$vaesenc_loop_tail_2__func2
	vaesenclast	%ymm10,%ymm12,%ymm12
	vaesenclast	%ymm10,%ymm13,%ymm13

// XOR the remaining data with the keystream blocks, and update GHASH with
// the remaining ciphertext blocks without reducing.

	cmpq	$32,%rdx
	jb	L$xor_one_block__func2
	je	L$xor_two_blocks__func2

L$xor_three_blocks__func2:
	vmovdqu	0(%rdi),%ymm2
	vmovdqu	32(%rdi),%xmm3
	vpxor	%ymm2,%ymm12,%ymm12
	vpxor	%xmm3,%xmm13,%xmm13
	vmovdqu	%ymm12,0(%rsi)
	vmovdqu	%xmm13,32(%rsi)

	vpshufb	%ymm0,%ymm2,%ymm12
	vpshufb	%xmm0,%xmm3,%xmm13
	vpxor	%ymm1,%ymm12,%ymm12
	vmovdqu	(%r8),%ymm2
	vmovdqu	32(%r8),%xmm3
	vpclmulqdq	$0x00,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm5,%ymm5
	vpclmulqdq	$0x01,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x10,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%xmm3,%xmm13,%xmm4
	vpxor	%ymm4,%ymm7,%ymm7
	jmp	L$ghash_mul_one_vec_unreduced__func2

L$xor_two_blocks__func2:
	vmovdqu	(%rdi),%ymm2
	vpxor	%ymm2,%ymm12,%ymm12
	vmovdqu	%ymm12,(%rsi)
	vpshufb	%ymm0,%ymm2,%ymm12
	vpxor	%ymm1,%ymm12,%ymm12
	vmovdqu	(%r8),%ymm2
	jmp	L$ghash_mul_one_vec_unreduced__func2

L$xor_one_block__func2:
	vmovdqu	(%rdi),%xmm2
	vpxor	%xmm2,%xmm12,%xmm12
	vmovdqu	%xmm12,(%rsi)
	vpshufb	%xmm0,%xmm2,%xmm12
	vpxor	%xmm1,%xmm12,%xmm12
	vmovdqu	(%r8),%xmm2

L$ghash_mul_one_vec_unreduced__func2:
	vpclmulqdq	$0x00,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm5,%ymm5
	vpclmulqdq	$0x01,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x10,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm6,%ymm6
	vpclmulqdq	$0x11,%ymm2,%ymm12,%ymm4
	vpxor	%ymm4,%ymm7,%ymm7

L$reduce__func2:
// Finally, do the GHASH reduction.
	vbroadcasti128	L$gfpoly(%rip),%ymm2
	vpclmulqdq	$0x01,%ymm5,%ymm2,%ymm3
	vpshufd	$0x4e,%ymm5,%ymm5
	vpxor	%ymm5,%ymm6,%ymm6
	vpxor	%ymm3,%ymm6,%ymm6
	vpclmulqdq	$0x01,%ymm6,%ymm2,%ymm3
	vpshufd	$0x4e,%ymm6,%ymm6
	vpxor	%ymm6,%ymm7,%ymm7
	vpxor	%ymm3,%ymm7,%ymm7
	vextracti128	$1,%ymm7,%xmm1
	vpxor	%xmm7,%xmm1,%xmm1

L$done__func2:
// Store the updated GHASH accumulator back to memory.
	vpshufb	%xmm0,%xmm1,%xmm1
	vmovdqu	%xmm1,(%r12)

	vzeroupper
	popq	%r12

	ret



#endif
