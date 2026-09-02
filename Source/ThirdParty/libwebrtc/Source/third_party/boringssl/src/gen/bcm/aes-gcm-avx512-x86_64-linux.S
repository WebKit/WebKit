// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__ELF__)
.section	.rodata

.align	64
aes_gcm_avx512_constants:

// A shuffle mask that reflects the bytes of 16-byte blocks
.Lbswap_mask:
.quad	0x08090a0b0c0d0e0f, 0x0001020304050607

// This is the GHASH reducing polynomial without its constant term, i.e.
// x^128 + x^7 + x^2 + x, represented using the backwards mapping
// between bits and polynomial coefficients.
// 
// Alternatively, it can be interpreted as the naturally-ordered
// representation of the polynomial x^127 + x^126 + x^121 + 1, i.e. the
// "reversed" GHASH reducing polynomial without its x^128 term.
.Lgfpoly:
.quad	1, 0xc200000000000000

// Same as above, but with the (1 << 64) bit set.
.Lgfpoly_and_internal_carrybit:
.quad	1, 0xc200000000000001

// Values needed to prepare the initial vector of counter blocks.
.Lctr_pattern:
.quad	0, 0
.quad	1, 0
.quad	2, 0
.quad	3, 0

// The number of AES blocks per vector, as a 128-bit value.
.Linc_4blocks:
.quad	4, 0

.text	
.globl	gcm_init_vpclmulqdq_avx512
.hidden gcm_init_vpclmulqdq_avx512
.type	gcm_init_vpclmulqdq_avx512,@function
.align	32
gcm_init_vpclmulqdq_avx512:
.cfi_startproc	

_CET_ENDBR
// Get pointer to lowest set of key powers (located at end of array).
	leaq	256-64(%rdi),%r8

// Load the byte-reflected hash subkey.  BoringSSL provides it in
// byte-reflected form except the two halves are in the wrong order.
	vpshufd	$0x4e,(%rsi),%xmm3

// Finish preprocessing the first key power, H^1.  Since this GHASH
// implementation operates directly on values with the backwards bit
// order specified by the GCM standard, it's necessary to preprocess the
// raw key as follows.  First, reflect its bytes.  Second, multiply it
// by x^-1 mod x^128 + x^7 + x^2 + x + 1 (if using the backwards
// interpretation of polynomial coefficients), which can also be
// interpreted as multiplication by x mod x^128 + x^127 + x^126 + x^121
// + 1 using the alternative, natural interpretation of polynomial
// coefficients.  For details, see the comment above _ghash_mul.
// 
// Either way, for the multiplication the concrete operation performed
// is a left shift of the 128-bit value by 1 bit, then an XOR with (0xc2
// << 120) | 1 if a 1 bit was carried out.  However, there's no 128-bit
// wide shift instruction, so instead double each of the two 64-bit
// halves and incorporate the internal carry bit into the value XOR'd.
	vpshufd	$0xd3,%xmm3,%xmm0
	vpsrad	$31,%xmm0,%xmm0
	vpaddq	%xmm3,%xmm3,%xmm3
// H_CUR_XMM ^= TMP0_XMM & gfpoly_and_internal_carrybit
	vpternlogd	$0x78,.Lgfpoly_and_internal_carrybit(%rip),%xmm0,%xmm3

// Load the gfpoly constant.
	vbroadcasti32x4	.Lgfpoly(%rip),%zmm5

// Square H^1 to get H^2.
// 
// Note that as with H^1, all higher key powers also need an extra
// factor of x^-1 (or x using the natural interpretation).  Nothing
// special needs to be done to make this happen, though: H^1 * H^1 would
// end up with two factors of x^-1, but the multiplication consumes one.
// So the product H^2 ends up with the desired one factor of x^-1.
	vpclmulqdq	$0x00,%xmm3,%xmm3,%xmm0  // LO = a_L * a_L
	vpclmulqdq	$0x11,%xmm3,%xmm3,%xmm4  // HI = a_H * a_H
	vpclmulqdq	$0x01,%xmm0,%xmm5,%xmm1  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm0,%xmm0  // Swap halves of LO
	vpxor	%xmm0,%xmm1,%xmm1  // Fold LO into MI
	vpclmulqdq	$0x01,%xmm1,%xmm5,%xmm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm1,%xmm1  // Swap halves of MI
	vpternlogd	$0x96,%xmm0,%xmm1,%xmm4  // Fold MI into HI


// Create H_CUR_YMM = [H^2, H^1] and H_INC_YMM = [H^2, H^2].
	vinserti128	$1,%xmm3,%ymm4,%ymm3
	vinserti128	$1,%xmm4,%ymm4,%ymm4

// Create H_CUR = [H^4, H^3, H^2, H^1] and H_INC = [H^4, H^4, H^4, H^4].
	vpclmulqdq	$0x00,%ymm4,%ymm3,%ymm0  // LO = a_L * b_L
	vpclmulqdq	$0x01,%ymm4,%ymm3,%ymm1  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%ymm4,%ymm3,%ymm2  // MI_1 = a_H * b_L
	vpxord	%ymm2,%ymm1,%ymm1  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%ymm0,%ymm5,%ymm2  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm0,%ymm0  // Swap halves of LO
	vpternlogd	$0x96,%ymm2,%ymm0,%ymm1  // Fold LO into MI
	vpclmulqdq	$0x11,%ymm4,%ymm3,%ymm4  // HI = a_H * b_H
	vpclmulqdq	$0x01,%ymm1,%ymm5,%ymm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%ymm1,%ymm1  // Swap halves of MI
	vpternlogd	$0x96,%ymm0,%ymm1,%ymm4  // Fold MI into HI

	vinserti64x4	$1,%ymm3,%zmm4,%zmm3
	vshufi64x2	$0,%zmm4,%zmm4,%zmm4

// Store the lowest set of key powers.
	vmovdqu8	%zmm3,(%r8)

// Compute and store the remaining key powers.
// Repeatedly multiply [H^(i+3), H^(i+2), H^(i+1), H^i] by
// [H^4, H^4, H^4, H^4] to get [H^(i+7), H^(i+6), H^(i+5), H^(i+4)].
	movl	$3,%eax
.Lprecompute_next:
	subq	$64,%r8
	vpclmulqdq	$0x00,%zmm4,%zmm3,%zmm0  // LO = a_L * b_L
	vpclmulqdq	$0x01,%zmm4,%zmm3,%zmm1  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%zmm4,%zmm3,%zmm2  // MI_1 = a_H * b_L
	vpxord	%zmm2,%zmm1,%zmm1  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%zmm0,%zmm5,%zmm2  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%zmm0,%zmm0  // Swap halves of LO
	vpternlogd	$0x96,%zmm2,%zmm0,%zmm1  // Fold LO into MI
	vpclmulqdq	$0x11,%zmm4,%zmm3,%zmm3  // HI = a_H * b_H
	vpclmulqdq	$0x01,%zmm1,%zmm5,%zmm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%zmm1,%zmm1  // Swap halves of MI
	vpternlogd	$0x96,%zmm0,%zmm1,%zmm3  // Fold MI into HI

	vmovdqu8	%zmm3,(%r8)
	decl	%eax
	jnz	.Lprecompute_next

	vzeroupper  // This is needed after using ymm or zmm registers.
	ret

.cfi_endproc	
.size	gcm_init_vpclmulqdq_avx512, . - gcm_init_vpclmulqdq_avx512
.globl	gcm_gmult_vpclmulqdq_avx512
.hidden gcm_gmult_vpclmulqdq_avx512
.type	gcm_gmult_vpclmulqdq_avx512,@function
.align	32
gcm_gmult_vpclmulqdq_avx512:
.cfi_startproc	

_CET_ENDBR



	vmovdqu	(%rdi),%xmm0
	vmovdqu	.Lbswap_mask(%rip),%xmm1
	vmovdqu	256-16(%rsi),%xmm2
	vmovdqu	.Lgfpoly(%rip),%xmm3
	vpshufb	%xmm1,%xmm0,%xmm0

	vpclmulqdq	$0x00,%xmm2,%xmm0,%xmm4  // LO = a_L * b_L
	vpclmulqdq	$0x01,%xmm2,%xmm0,%xmm5  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%xmm2,%xmm0,%xmm6  // MI_1 = a_H * b_L
	vpxord	%xmm6,%xmm5,%xmm5  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%xmm4,%xmm3,%xmm6  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm4,%xmm4  // Swap halves of LO
	vpternlogd	$0x96,%xmm6,%xmm4,%xmm5  // Fold LO into MI
	vpclmulqdq	$0x11,%xmm2,%xmm0,%xmm0  // HI = a_H * b_H
	vpclmulqdq	$0x01,%xmm5,%xmm3,%xmm4  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm5,%xmm5  // Swap halves of MI
	vpternlogd	$0x96,%xmm4,%xmm5,%xmm0  // Fold MI into HI


	vpshufb	%xmm1,%xmm0,%xmm0
	vmovdqu	%xmm0,(%rdi)

// No need for vzeroupper, since only xmm registers were used.
	ret

.cfi_endproc	
.size	gcm_gmult_vpclmulqdq_avx512, . - gcm_gmult_vpclmulqdq_avx512
.globl	gcm_ghash_vpclmulqdq_avx512
.hidden gcm_ghash_vpclmulqdq_avx512
.type	gcm_ghash_vpclmulqdq_avx512,@function
.align	32
gcm_ghash_vpclmulqdq_avx512:
.cfi_startproc	

_CET_ENDBR



// Load the bswap_mask and gfpoly constants.  Since AADLEN is usually small,
// usually only 128-bit vectors will be used.  So as an optimization, don't
// broadcast these constants to all 128-bit lanes quite yet.
	vmovdqu	.Lbswap_mask(%rip),%xmm4
	vmovdqu	.Lgfpoly(%rip),%xmm10

// Load the GHASH accumulator.
	vmovdqu	(%rdi),%xmm5
	vpshufb	%xmm4,%xmm5,%xmm5

// Optimize for AADLEN < 64 by checking for AADLEN < 64 before AADLEN < 256.
	cmpq	$64,%rcx
	jb	.Laad_blockbyblock

// AADLEN >= 64, so we'll operate on full vectors.  Broadcast bswap_mask and
// gfpoly to all 128-bit lanes.
	vshufi64x2	$0,%zmm4,%zmm4,%zmm4
	vshufi64x2	$0,%zmm10,%zmm10,%zmm10

// Load the lowest set of key powers.
	vmovdqu8	256-64(%rsi),%zmm9

	cmpq	$256,%rcx
	jb	.Laad_loop_1x

// AADLEN >= 256.  Load the higher key powers.
	vmovdqu8	256-256(%rsi),%zmm6
	vmovdqu8	256-192(%rsi),%zmm7
	vmovdqu8	256-128(%rsi),%zmm8

// Update GHASH with 256 bytes of AAD at a time.
.Laad_loop_4x:
	vmovdqu8	0(%rdx),%zmm0
	vmovdqu8	64(%rdx),%zmm1
	vmovdqu8	128(%rdx),%zmm2
	vmovdqu8	192(%rdx),%zmm3
	vpshufb	%zmm4,%zmm0,%zmm0
	vpxord	%zmm5,%zmm0,%zmm0
	vpshufb	%zmm4,%zmm1,%zmm1
	vpshufb	%zmm4,%zmm2,%zmm2
	vpshufb	%zmm4,%zmm3,%zmm3
	vpclmulqdq	$0x00,%zmm6,%zmm0,%zmm5  // LO_0
	vpclmulqdq	$0x00,%zmm7,%zmm1,%zmm11  // LO_1
	vpclmulqdq	$0x00,%zmm8,%zmm2,%zmm12  // LO_2
	vpxord	%zmm11,%zmm5,%zmm5  // sum(LO_{1,0})
	vpclmulqdq	$0x00,%zmm9,%zmm3,%zmm13  // LO_3
	vpternlogd	$0x96,%zmm13,%zmm12,%zmm5  // LO = sum(LO_{3,2,1,0})
	vpclmulqdq	$0x01,%zmm6,%zmm0,%zmm11  // MI_0
	vpclmulqdq	$0x01,%zmm7,%zmm1,%zmm12  // MI_1
	vpclmulqdq	$0x01,%zmm8,%zmm2,%zmm13  // MI_2
	vpternlogd	$0x96,%zmm13,%zmm12,%zmm11  // sum(MI_{2,1,0})
	vpclmulqdq	$0x01,%zmm9,%zmm3,%zmm12  // MI_3
	vpclmulqdq	$0x10,%zmm6,%zmm0,%zmm13  // MI_4
	vpternlogd	$0x96,%zmm13,%zmm12,%zmm11  // sum(MI_{4,3,2,1,0})
	vpclmulqdq	$0x10,%zmm7,%zmm1,%zmm12  // MI_5
	vpclmulqdq	$0x10,%zmm8,%zmm2,%zmm13  // MI_6
	vpternlogd	$0x96,%zmm13,%zmm12,%zmm11  // sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	$0x01,%zmm5,%zmm10,%zmm13  // LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	$0x10,%zmm9,%zmm3,%zmm12  // MI_7
	vpxord	%zmm12,%zmm11,%zmm11  // MI = sum(MI_{7,6,5,4,3,2,1,0})
	vpshufd	$0x4e,%zmm5,%zmm5  // Swap halves of LO
	vpclmulqdq	$0x11,%zmm6,%zmm0,%zmm0  // HI_0
	vpclmulqdq	$0x11,%zmm7,%zmm1,%zmm1  // HI_1
	vpclmulqdq	$0x11,%zmm8,%zmm2,%zmm2  // HI_2
	vpternlogd	$0x96,%zmm13,%zmm5,%zmm11  // Fold LO into MI
	vpclmulqdq	$0x11,%zmm9,%zmm3,%zmm3  // HI_3
	vpternlogd	$0x96,%zmm2,%zmm1,%zmm0  // sum(HI_{2,1,0})
	vpclmulqdq	$0x01,%zmm11,%zmm10,%zmm12  // MI_L*(x^63 + x^62 + x^57)
	vpxord	%zmm3,%zmm0,%zmm5  // HI = sum(HI_{3,2,1,0})
	vpshufd	$0x4e,%zmm11,%zmm11  // Swap halves of MI
	vpternlogd	$0x96,%zmm12,%zmm11,%zmm5  // Fold MI into HI
	vextracti32x4	$1,%zmm5,%xmm0
	vextracti32x4	$2,%zmm5,%xmm1
	vextracti32x4	$3,%zmm5,%xmm2
	vpxord	%xmm0,%xmm5,%xmm5
	vpternlogd	$0x96,%xmm1,%xmm2,%xmm5

	addq	$256,%rdx
	subq	$256,%rcx
	cmpq	$256,%rcx
	jae	.Laad_loop_4x

// Update GHASH with 64 bytes of AAD at a time.
	cmpq	$64,%rcx
	jb	.Laad_large_done
.Laad_loop_1x:
	vmovdqu8	(%rdx),%zmm0
	vpshufb	%zmm4,%zmm0,%zmm0
	vpxord	%zmm0,%zmm5,%zmm5
	vpclmulqdq	$0x00,%zmm9,%zmm5,%zmm0  // LO = a_L * b_L
	vpclmulqdq	$0x01,%zmm9,%zmm5,%zmm1  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%zmm9,%zmm5,%zmm2  // MI_1 = a_H * b_L
	vpxord	%zmm2,%zmm1,%zmm1  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%zmm0,%zmm10,%zmm2  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%zmm0,%zmm0  // Swap halves of LO
	vpternlogd	$0x96,%zmm2,%zmm0,%zmm1  // Fold LO into MI
	vpclmulqdq	$0x11,%zmm9,%zmm5,%zmm5  // HI = a_H * b_H
	vpclmulqdq	$0x01,%zmm1,%zmm10,%zmm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%zmm1,%zmm1  // Swap halves of MI
	vpternlogd	$0x96,%zmm0,%zmm1,%zmm5  // Fold MI into HI

	vextracti32x4	$1,%zmm5,%xmm0
	vextracti32x4	$2,%zmm5,%xmm1
	vextracti32x4	$3,%zmm5,%xmm2
	vpxord	%xmm0,%xmm5,%xmm5
	vpternlogd	$0x96,%xmm1,%xmm2,%xmm5

	addq	$64,%rdx
	subq	$64,%rcx
	cmpq	$64,%rcx
	jae	.Laad_loop_1x

.Laad_large_done:

// GHASH the remaining data 16 bytes at a time, using xmm registers only.
.Laad_blockbyblock:
	testq	%rcx,%rcx
	jz	.Laad_done
	vmovdqu	256-16(%rsi),%xmm9
.Laad_loop_blockbyblock:
	vmovdqu	(%rdx),%xmm0
	vpshufb	%xmm4,%xmm0,%xmm0
	vpxor	%xmm0,%xmm5,%xmm5
	vpclmulqdq	$0x00,%xmm9,%xmm5,%xmm0  // LO = a_L * b_L
	vpclmulqdq	$0x01,%xmm9,%xmm5,%xmm1  // MI_0 = a_L * b_H
	vpclmulqdq	$0x10,%xmm9,%xmm5,%xmm2  // MI_1 = a_H * b_L
	vpxord	%xmm2,%xmm1,%xmm1  // MI = MI_0 + MI_1
	vpclmulqdq	$0x01,%xmm0,%xmm10,%xmm2  // LO_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm0,%xmm0  // Swap halves of LO
	vpternlogd	$0x96,%xmm2,%xmm0,%xmm1  // Fold LO into MI
	vpclmulqdq	$0x11,%xmm9,%xmm5,%xmm5  // HI = a_H * b_H
	vpclmulqdq	$0x01,%xmm1,%xmm10,%xmm0  // MI_L*(x^63 + x^62 + x^57)
	vpshufd	$0x4e,%xmm1,%xmm1  // Swap halves of MI
	vpternlogd	$0x96,%xmm0,%xmm1,%xmm5  // Fold MI into HI

	addq	$16,%rdx
	subq	$16,%rcx
	jnz	.Laad_loop_blockbyblock

.Laad_done:
// Store the updated GHASH accumulator back to memory.
	vpshufb	%xmm4,%xmm5,%xmm5
	vmovdqu	%xmm5,(%rdi)

	vzeroupper  // This is needed after using ymm or zmm registers.
	ret

.cfi_endproc	
.size	gcm_ghash_vpclmulqdq_avx512, . - gcm_ghash_vpclmulqdq_avx512
.globl	aes_gcm_enc_update_vaes_avx512
.hidden aes_gcm_enc_update_vaes_avx512
.type	aes_gcm_enc_update_vaes_avx512,@function
.align	32
aes_gcm_enc_update_vaes_avx512:
.cfi_startproc	

_CET_ENDBR
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-16

	movq	16(%rsp),%r12  // arg7
#ifdef BORINGSSL_DISPATCH_TEST
.extern	BORINGSSL_function_hit
.hidden BORINGSSL_function_hit
	movb	$1,BORINGSSL_function_hit+7(%rip)
#endif
// Load some constants.
	vbroadcasti32x4	.Lbswap_mask(%rip),%zmm8
	vbroadcasti32x4	.Lgfpoly(%rip),%zmm31

// Load the GHASH accumulator and the starting counter.
// BoringSSL passes these values in big endian format.
	vmovdqu	(%r12),%xmm10
	vpshufb	%xmm8,%xmm10,%xmm10
	vbroadcasti32x4	(%r8),%zmm12
	vpshufb	%zmm8,%zmm12,%zmm12

// Load the AES key length in bytes.  BoringSSL stores number of rounds
// minus 1, so convert using: AESKEYLEN = 4 * aeskey->rounds - 20.
	movl	240(%rcx),%r10d
	leal	-20(,%r10,4),%r10d

// Make RNDKEYLAST_PTR point to the last AES round key.  This is the
// round key with index 10, 12, or 14 for AES-128, AES-192, or AES-256
// respectively.  Then load the zero-th and last round keys.
	leaq	96(%rcx,%r10,4),%r11
	vbroadcasti32x4	(%rcx),%zmm13
	vbroadcasti32x4	(%r11),%zmm14

// Finish initializing LE_CTR by adding [0, 1, 2, 3] to its low words.
	vpaddd	.Lctr_pattern(%rip),%zmm12,%zmm12

// Load 4 into all 128-bit lanes of LE_CTR_INC.
	vbroadcasti32x4	.Linc_4blocks(%rip),%zmm11

// If there are at least 256 bytes of data, then continue into the loop
// that processes 256 bytes of data at a time.  Otherwise skip it.
	cmpq	$256,%rdx
	jb	.Lcrypt_loop_4x_done__func1

// Load powers of the hash key.
	vmovdqu8	256-256(%r9),%zmm27
	vmovdqu8	256-192(%r9),%zmm28
	vmovdqu8	256-128(%r9),%zmm29
	vmovdqu8	256-64(%r9),%zmm30
// Encrypt the first 4 vectors of plaintext blocks.  Leave the resulting
// ciphertext in GHASHDATA[0-3] for GHASH.
// Increment le_ctr four times to generate four vectors of little-endian
// counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vpshufb	%zmm8,%zmm12,%zmm0
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm1
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm2
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm3
	vpaddd	%zmm11,%zmm12,%zmm12

// AES "round zero": XOR in the zero-th round key.
	vpxord	%zmm13,%zmm0,%zmm0
	vpxord	%zmm13,%zmm1,%zmm1
	vpxord	%zmm13,%zmm2,%zmm2
	vpxord	%zmm13,%zmm3,%zmm3

	leaq	16(%rcx),%rax
.Lvaesenc_loop_first_4_vecs__func1:
	vbroadcasti32x4	(%rax),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

	addq	$16,%rax
	cmpq	%rax,%r11
	jne	.Lvaesenc_loop_first_4_vecs__func1
	vpxord	0(%rdi),%zmm14,%zmm4
	vpxord	64(%rdi),%zmm14,%zmm5
	vpxord	128(%rdi),%zmm14,%zmm6
	vpxord	192(%rdi),%zmm14,%zmm7
	vaesenclast	%zmm4,%zmm0,%zmm4
	vaesenclast	%zmm5,%zmm1,%zmm5
	vaesenclast	%zmm6,%zmm2,%zmm6
	vaesenclast	%zmm7,%zmm3,%zmm7
	vmovdqu8	%zmm4,0(%rsi)
	vmovdqu8	%zmm5,64(%rsi)
	vmovdqu8	%zmm6,128(%rsi)
	vmovdqu8	%zmm7,192(%rsi)

	addq	$256,%rdi
	addq	$256,%rsi
	subq	$256,%rdx
	cmpq	$256,%rdx
	jb	.Lghash_last_ciphertext_4x__func1
// Cache as many additional AES round keys as possible.
	vbroadcasti32x4	-144(%r11),%zmm15
	vbroadcasti32x4	-128(%r11),%zmm16
	vbroadcasti32x4	-112(%r11),%zmm17
	vbroadcasti32x4	-96(%r11),%zmm18
	vbroadcasti32x4	-80(%r11),%zmm19
	vbroadcasti32x4	-64(%r11),%zmm20
	vbroadcasti32x4	-48(%r11),%zmm21
	vbroadcasti32x4	-32(%r11),%zmm22
	vbroadcasti32x4	-16(%r11),%zmm23

.Lcrypt_loop_4x__func1:
// Start the AES encryption of the counter blocks.
// Increment le_ctr four times to generate four vectors of little-endian
// counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vpshufb	%zmm8,%zmm12,%zmm0
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm1
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm2
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm3
	vpaddd	%zmm11,%zmm12,%zmm12

// AES "round zero": XOR in the zero-th round key.
	vpxord	%zmm13,%zmm0,%zmm0
	vpxord	%zmm13,%zmm1,%zmm1
	vpxord	%zmm13,%zmm2,%zmm2
	vpxord	%zmm13,%zmm3,%zmm3

	cmpl	$24,%r10d
	jl	.Laes128__func1
	je	.Laes192__func1
// AES-256
	vbroadcasti32x4	-208(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

	vbroadcasti32x4	-192(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

.Laes192__func1:
	vbroadcasti32x4	-176(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

	vbroadcasti32x4	-160(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

.Laes128__func1:

// Prefetch the source data 512 bytes ahead into the L1 data cache, to
// improve performance when the hardware prefetcher is disabled.  Assumes the
// L1 data cache line size is 64 bytes (de facto standard on x86_64).
	prefetcht0	512+0(%rdi)
	prefetcht0	512+64(%rdi)
	prefetcht0	512+128(%rdi)
	prefetcht0	512+192(%rdi)

// Finish the AES encryption of the counter blocks in AESDATA[0-3],
// interleaved with the GHASH update of the ciphertext blocks in
// GHASHDATA[0-3].
	vpshufb	%zmm8,%zmm4,%zmm4
	vpxord	%zmm10,%zmm4,%zmm4
	vpshufb	%zmm8,%zmm5,%zmm5
	vpshufb	%zmm8,%zmm6,%zmm6

	vaesenc	%zmm15,%zmm0,%zmm0
	vaesenc	%zmm15,%zmm1,%zmm1
	vaesenc	%zmm15,%zmm2,%zmm2
	vaesenc	%zmm15,%zmm3,%zmm3

	vpshufb	%zmm8,%zmm7,%zmm7
	vpclmulqdq	$0x00,%zmm27,%zmm4,%zmm10  // LO_0
	vpclmulqdq	$0x00,%zmm28,%zmm5,%zmm24  // LO_1
	vpclmulqdq	$0x00,%zmm29,%zmm6,%zmm25  // LO_2

	vaesenc	%zmm16,%zmm0,%zmm0
	vaesenc	%zmm16,%zmm1,%zmm1
	vaesenc	%zmm16,%zmm2,%zmm2
	vaesenc	%zmm16,%zmm3,%zmm3

	vpxord	%zmm24,%zmm10,%zmm10  // sum(LO_{1,0})
	vpclmulqdq	$0x00,%zmm30,%zmm7,%zmm26  // LO_3
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm10  // LO = sum(LO_{3,2,1,0})
	vpclmulqdq	$0x01,%zmm27,%zmm4,%zmm24  // MI_0

	vaesenc	%zmm17,%zmm0,%zmm0
	vaesenc	%zmm17,%zmm1,%zmm1
	vaesenc	%zmm17,%zmm2,%zmm2
	vaesenc	%zmm17,%zmm3,%zmm3

	vpclmulqdq	$0x01,%zmm28,%zmm5,%zmm25  // MI_1
	vpclmulqdq	$0x01,%zmm29,%zmm6,%zmm26  // MI_2
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{2,1,0})
	vpclmulqdq	$0x01,%zmm30,%zmm7,%zmm25  // MI_3

	vaesenc	%zmm18,%zmm0,%zmm0
	vaesenc	%zmm18,%zmm1,%zmm1
	vaesenc	%zmm18,%zmm2,%zmm2
	vaesenc	%zmm18,%zmm3,%zmm3

	vpclmulqdq	$0x10,%zmm27,%zmm4,%zmm26  // MI_4
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{4,3,2,1,0})
	vpclmulqdq	$0x10,%zmm28,%zmm5,%zmm25  // MI_5
	vpclmulqdq	$0x10,%zmm29,%zmm6,%zmm26  // MI_6

	vaesenc	%zmm19,%zmm0,%zmm0
	vaesenc	%zmm19,%zmm1,%zmm1
	vaesenc	%zmm19,%zmm2,%zmm2
	vaesenc	%zmm19,%zmm3,%zmm3

	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	$0x01,%zmm10,%zmm31,%zmm26  // LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	$0x10,%zmm30,%zmm7,%zmm25  // MI_7
	vpxord	%zmm25,%zmm24,%zmm24  // MI = sum(MI_{7,6,5,4,3,2,1,0})

	vaesenc	%zmm20,%zmm0,%zmm0
	vaesenc	%zmm20,%zmm1,%zmm1
	vaesenc	%zmm20,%zmm2,%zmm2
	vaesenc	%zmm20,%zmm3,%zmm3

	vpshufd	$0x4e,%zmm10,%zmm10  // Swap halves of LO
	vpclmulqdq	$0x11,%zmm27,%zmm4,%zmm4  // HI_0
	vpclmulqdq	$0x11,%zmm28,%zmm5,%zmm5  // HI_1
	vpclmulqdq	$0x11,%zmm29,%zmm6,%zmm6  // HI_2

	vaesenc	%zmm21,%zmm0,%zmm0
	vaesenc	%zmm21,%zmm1,%zmm1
	vaesenc	%zmm21,%zmm2,%zmm2
	vaesenc	%zmm21,%zmm3,%zmm3

	vpternlogd	$0x96,%zmm26,%zmm10,%zmm24  // Fold LO into MI
	vpclmulqdq	$0x11,%zmm30,%zmm7,%zmm7  // HI_3
	vpternlogd	$0x96,%zmm6,%zmm5,%zmm4  // sum(HI_{2,1,0})
	vpclmulqdq	$0x01,%zmm24,%zmm31,%zmm25  // MI_L*(x^63 + x^62 + x^57)

	vaesenc	%zmm22,%zmm0,%zmm0
	vaesenc	%zmm22,%zmm1,%zmm1
	vaesenc	%zmm22,%zmm2,%zmm2
	vaesenc	%zmm22,%zmm3,%zmm3

	vpxord	%zmm7,%zmm4,%zmm10  // HI = sum(HI_{3,2,1,0})
	vpshufd	$0x4e,%zmm24,%zmm24  // Swap halves of MI
	vpternlogd	$0x96,%zmm25,%zmm24,%zmm10  // Fold MI into HI

	vaesenc	%zmm23,%zmm0,%zmm0
	vaesenc	%zmm23,%zmm1,%zmm1
	vaesenc	%zmm23,%zmm2,%zmm2
	vaesenc	%zmm23,%zmm3,%zmm3


	vextracti32x4	$1,%zmm10,%xmm4
	vextracti32x4	$2,%zmm10,%xmm5
	vextracti32x4	$3,%zmm10,%xmm6
	vpxord	%xmm4,%xmm10,%xmm10
	vpternlogd	$0x96,%xmm5,%xmm6,%xmm10

	vpxord	0(%rdi),%zmm14,%zmm4
	vpxord	64(%rdi),%zmm14,%zmm5
	vpxord	128(%rdi),%zmm14,%zmm6
	vpxord	192(%rdi),%zmm14,%zmm7
	vaesenclast	%zmm4,%zmm0,%zmm4
	vaesenclast	%zmm5,%zmm1,%zmm5
	vaesenclast	%zmm6,%zmm2,%zmm6
	vaesenclast	%zmm7,%zmm3,%zmm7
	vmovdqu8	%zmm4,0(%rsi)
	vmovdqu8	%zmm5,64(%rsi)
	vmovdqu8	%zmm6,128(%rsi)
	vmovdqu8	%zmm7,192(%rsi)

	addq	$256,%rdi
	addq	$256,%rsi
	subq	$256,%rdx
	cmpq	$256,%rdx
	jae	.Lcrypt_loop_4x__func1
.Lghash_last_ciphertext_4x__func1:
	vpshufb	%zmm8,%zmm4,%zmm4
	vpxord	%zmm10,%zmm4,%zmm4
	vpshufb	%zmm8,%zmm5,%zmm5
	vpshufb	%zmm8,%zmm6,%zmm6
	vpshufb	%zmm8,%zmm7,%zmm7
	vpclmulqdq	$0x00,%zmm27,%zmm4,%zmm10  // LO_0
	vpclmulqdq	$0x00,%zmm28,%zmm5,%zmm24  // LO_1
	vpclmulqdq	$0x00,%zmm29,%zmm6,%zmm25  // LO_2
	vpxord	%zmm24,%zmm10,%zmm10  // sum(LO_{1,0})
	vpclmulqdq	$0x00,%zmm30,%zmm7,%zmm26  // LO_3
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm10  // LO = sum(LO_{3,2,1,0})
	vpclmulqdq	$0x01,%zmm27,%zmm4,%zmm24  // MI_0
	vpclmulqdq	$0x01,%zmm28,%zmm5,%zmm25  // MI_1
	vpclmulqdq	$0x01,%zmm29,%zmm6,%zmm26  // MI_2
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{2,1,0})
	vpclmulqdq	$0x01,%zmm30,%zmm7,%zmm25  // MI_3
	vpclmulqdq	$0x10,%zmm27,%zmm4,%zmm26  // MI_4
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{4,3,2,1,0})
	vpclmulqdq	$0x10,%zmm28,%zmm5,%zmm25  // MI_5
	vpclmulqdq	$0x10,%zmm29,%zmm6,%zmm26  // MI_6
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	$0x01,%zmm10,%zmm31,%zmm26  // LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	$0x10,%zmm30,%zmm7,%zmm25  // MI_7
	vpxord	%zmm25,%zmm24,%zmm24  // MI = sum(MI_{7,6,5,4,3,2,1,0})
	vpshufd	$0x4e,%zmm10,%zmm10  // Swap halves of LO
	vpclmulqdq	$0x11,%zmm27,%zmm4,%zmm4  // HI_0
	vpclmulqdq	$0x11,%zmm28,%zmm5,%zmm5  // HI_1
	vpclmulqdq	$0x11,%zmm29,%zmm6,%zmm6  // HI_2
	vpternlogd	$0x96,%zmm26,%zmm10,%zmm24  // Fold LO into MI
	vpclmulqdq	$0x11,%zmm30,%zmm7,%zmm7  // HI_3
	vpternlogd	$0x96,%zmm6,%zmm5,%zmm4  // sum(HI_{2,1,0})
	vpclmulqdq	$0x01,%zmm24,%zmm31,%zmm25  // MI_L*(x^63 + x^62 + x^57)
	vpxord	%zmm7,%zmm4,%zmm10  // HI = sum(HI_{3,2,1,0})
	vpshufd	$0x4e,%zmm24,%zmm24  // Swap halves of MI
	vpternlogd	$0x96,%zmm25,%zmm24,%zmm10  // Fold MI into HI
	vextracti32x4	$1,%zmm10,%xmm4
	vextracti32x4	$2,%zmm10,%xmm5
	vextracti32x4	$3,%zmm10,%xmm6
	vpxord	%xmm4,%xmm10,%xmm10
	vpternlogd	$0x96,%xmm5,%xmm6,%xmm10

.Lcrypt_loop_4x_done__func1:
// Check whether any data remains.
	testq	%rdx,%rdx
	jz	.Ldone__func1

// The data length isn't a multiple of 256 bytes.  Process the remaining
// data of length 1 <= DATALEN < 256, up to one 64-byte vector at a time.
// Going one vector at a time may seem inefficient compared to having
// separate code paths for each possible number of vectors remaining.
// However, using a loop keeps the code size down, and it performs
// surprising well; modern CPUs will start executing the next iteration
// before the previous one finishes and also predict the number of loop
// iterations.  For a similar reason, we roll up the AES rounds.
// 
// On the last iteration, the remaining length may be less than 64 bytes.
// Handle this using masking.
// 
// Since there are enough key powers available for all remaining data,
// there is no need to do a GHASH reduction after each iteration.
// Instead, multiply each remaining block by its own key power, and only
// do a GHASH reduction at the very end.

// Make POWERS_PTR point to the key powers [H^N, H^(N-1), ...] where N
// is the number of blocks that remain.
	movq	%rdx,%rax
	negq	%rax
	andq	$-16,%rax  // -round_up(DATALEN, 16)
	leaq	256(%r9,%rax,1),%r8
	vpxor	%xmm4,%xmm4,%xmm4
	vpxor	%xmm5,%xmm5,%xmm5
	vpxor	%xmm6,%xmm6,%xmm6

	cmpq	$64,%rdx
	jb	.Lpartial_vec__func1

.Lcrypt_loop_1x__func1:
// Process a full 64-byte vector.

// Encrypt a vector of counter blocks.
	vpshufb	%zmm8,%zmm12,%zmm0
	vpaddd	%zmm11,%zmm12,%zmm12
	vpxord	%zmm13,%zmm0,%zmm0
	leaq	16(%rcx),%rax
.Lvaesenc_loop_tail_full_vec__func1:
	vbroadcasti32x4	(%rax),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	.Lvaesenc_loop_tail_full_vec__func1
	vaesenclast	%zmm14,%zmm0,%zmm0

// XOR the data with the vector of keystream blocks.
	vmovdqu8	(%rdi),%zmm1
	vpxord	%zmm1,%zmm0,%zmm0
	vmovdqu8	%zmm0,(%rsi)

// Update GHASH with the ciphertext blocks, without reducing.
	vmovdqu8	(%r8),%zmm30
	vpshufb	%zmm8,%zmm0,%zmm0
	vpxord	%zmm10,%zmm0,%zmm0
	vpclmulqdq	$0x00,%zmm30,%zmm0,%zmm7  // a_L * b_L
	vpclmulqdq	$0x01,%zmm30,%zmm0,%zmm1  // a_L * b_H
	vpclmulqdq	$0x10,%zmm30,%zmm0,%zmm2  // a_H * b_L
	vpclmulqdq	$0x11,%zmm30,%zmm0,%zmm3  // a_H * b_H
	vpxord	%zmm7,%zmm4,%zmm4
	vpternlogd	$0x96,%zmm2,%zmm1,%zmm5
	vpxord	%zmm3,%zmm6,%zmm6

	vpxor	%xmm10,%xmm10,%xmm10

	addq	$64,%r8
	addq	$64,%rdi
	addq	$64,%rsi
	subq	$64,%rdx
	cmpq	$64,%rdx
	jae	.Lcrypt_loop_1x__func1

	testq	%rdx,%rdx
	jz	.Lreduce__func1

.Lpartial_vec__func1:
// Process a partial vector of length 1 <= DATALEN < 64.

// Set the data mask %k1 to DATALEN 1's.
// Set the key powers mask %k2 to round_up(DATALEN, 16) 1's.
	movq	$-1,%rax
	bzhiq	%rdx,%rax,%rax
	kmovq	%rax,%k1
	addq	$15,%rdx
	andq	$-16,%rdx
	movq	$-1,%rax
	bzhiq	%rdx,%rax,%rax
	kmovq	%rax,%k2

// Encrypt one last vector of counter blocks.  This does not need to be
// masked.  The counter does not need to be incremented here.
	vpshufb	%zmm8,%zmm12,%zmm0
	vpxord	%zmm13,%zmm0,%zmm0
	leaq	16(%rcx),%rax
.Lvaesenc_loop_tail_partialvec__func1:
	vbroadcasti32x4	(%rax),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	.Lvaesenc_loop_tail_partialvec__func1
	vaesenclast	%zmm14,%zmm0,%zmm0

// XOR the data with the appropriate number of keystream bytes.
	vmovdqu8	(%rdi),%zmm1{%k1}{z}
	vpxord	%zmm1,%zmm0,%zmm0
	vmovdqu8	%zmm0,(%rsi){%k1}

// Update GHASH with the ciphertext block(s), without reducing.
// 
// In the case of DATALEN < 64, the ciphertext is zero-padded to 64
// bytes.  (If decrypting, it's done by the above masked load.  If
// encrypting, it's done by the below masked register-to-register move.)
// Note that if DATALEN <= 48, there will be additional padding beyond
// the padding of the last block specified by GHASH itself; i.e., there
// may be whole block(s) that get processed by the GHASH multiplication
// and reduction instructions but should not actually be included in the
// GHASH.  However, any such blocks are all-zeroes, and the values that
// they're multiplied with are also all-zeroes.  Therefore they just add
// 0 * 0 = 0 to the final GHASH result, which makes no difference.
	vmovdqu8	(%r8),%zmm30{%k2}{z}
	vmovdqu8	%zmm0,%zmm1{%k1}{z}
	vpshufb	%zmm8,%zmm1,%zmm0
	vpxord	%zmm10,%zmm0,%zmm0
	vpclmulqdq	$0x00,%zmm30,%zmm0,%zmm7  // a_L * b_L
	vpclmulqdq	$0x01,%zmm30,%zmm0,%zmm1  // a_L * b_H
	vpclmulqdq	$0x10,%zmm30,%zmm0,%zmm2  // a_H * b_L
	vpclmulqdq	$0x11,%zmm30,%zmm0,%zmm3  // a_H * b_H
	vpxord	%zmm7,%zmm4,%zmm4
	vpternlogd	$0x96,%zmm2,%zmm1,%zmm5
	vpxord	%zmm3,%zmm6,%zmm6


.Lreduce__func1:
// Finally, do the GHASH reduction.
	vpclmulqdq	$0x01,%zmm4,%zmm31,%zmm0
	vpshufd	$0x4e,%zmm4,%zmm4
	vpternlogd	$0x96,%zmm0,%zmm4,%zmm5
	vpclmulqdq	$0x01,%zmm5,%zmm31,%zmm0
	vpshufd	$0x4e,%zmm5,%zmm5
	vpternlogd	$0x96,%zmm0,%zmm5,%zmm6

	vextracti32x4	$1,%zmm6,%xmm0
	vextracti32x4	$2,%zmm6,%xmm1
	vextracti32x4	$3,%zmm6,%xmm2
	vpxord	%xmm0,%xmm6,%xmm10
	vpternlogd	$0x96,%xmm1,%xmm2,%xmm10


.Ldone__func1:
// Store the updated GHASH accumulator back to memory.
	vpshufb	%xmm8,%xmm10,%xmm10
	vmovdqu	%xmm10,(%r12)

	vzeroupper  // This is needed after using ymm or zmm registers.
	popq	%r12
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r12
	ret

.cfi_endproc	
.size	aes_gcm_enc_update_vaes_avx512, . - aes_gcm_enc_update_vaes_avx512
.globl	aes_gcm_dec_update_vaes_avx512
.hidden aes_gcm_dec_update_vaes_avx512
.type	aes_gcm_dec_update_vaes_avx512,@function
.align	32
aes_gcm_dec_update_vaes_avx512:
.cfi_startproc	

_CET_ENDBR
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-16

	movq	16(%rsp),%r12  // arg7
// Load some constants.
	vbroadcasti32x4	.Lbswap_mask(%rip),%zmm8
	vbroadcasti32x4	.Lgfpoly(%rip),%zmm31

// Load the GHASH accumulator and the starting counter.
// BoringSSL passes these values in big endian format.
	vmovdqu	(%r12),%xmm10
	vpshufb	%xmm8,%xmm10,%xmm10
	vbroadcasti32x4	(%r8),%zmm12
	vpshufb	%zmm8,%zmm12,%zmm12

// Load the AES key length in bytes.  BoringSSL stores number of rounds
// minus 1, so convert using: AESKEYLEN = 4 * aeskey->rounds - 20.
	movl	240(%rcx),%r10d
	leal	-20(,%r10,4),%r10d

// Make RNDKEYLAST_PTR point to the last AES round key.  This is the
// round key with index 10, 12, or 14 for AES-128, AES-192, or AES-256
// respectively.  Then load the zero-th and last round keys.
	leaq	96(%rcx,%r10,4),%r11
	vbroadcasti32x4	(%rcx),%zmm13
	vbroadcasti32x4	(%r11),%zmm14

// Finish initializing LE_CTR by adding [0, 1, 2, 3] to its low words.
	vpaddd	.Lctr_pattern(%rip),%zmm12,%zmm12

// Load 4 into all 128-bit lanes of LE_CTR_INC.
	vbroadcasti32x4	.Linc_4blocks(%rip),%zmm11

// If there are at least 256 bytes of data, then continue into the loop
// that processes 256 bytes of data at a time.  Otherwise skip it.
	cmpq	$256,%rdx
	jb	.Lcrypt_loop_4x_done__func2

// Load powers of the hash key.
	vmovdqu8	256-256(%r9),%zmm27
	vmovdqu8	256-192(%r9),%zmm28
	vmovdqu8	256-128(%r9),%zmm29
	vmovdqu8	256-64(%r9),%zmm30
// Cache as many additional AES round keys as possible.
	vbroadcasti32x4	-144(%r11),%zmm15
	vbroadcasti32x4	-128(%r11),%zmm16
	vbroadcasti32x4	-112(%r11),%zmm17
	vbroadcasti32x4	-96(%r11),%zmm18
	vbroadcasti32x4	-80(%r11),%zmm19
	vbroadcasti32x4	-64(%r11),%zmm20
	vbroadcasti32x4	-48(%r11),%zmm21
	vbroadcasti32x4	-32(%r11),%zmm22
	vbroadcasti32x4	-16(%r11),%zmm23

.Lcrypt_loop_4x__func2:
	vmovdqu8	0(%rdi),%zmm4
	vmovdqu8	64(%rdi),%zmm5
	vmovdqu8	128(%rdi),%zmm6
	vmovdqu8	192(%rdi),%zmm7
// Start the AES encryption of the counter blocks.
// Increment le_ctr four times to generate four vectors of little-endian
// counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vpshufb	%zmm8,%zmm12,%zmm0
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm1
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm2
	vpaddd	%zmm11,%zmm12,%zmm12
	vpshufb	%zmm8,%zmm12,%zmm3
	vpaddd	%zmm11,%zmm12,%zmm12

// AES "round zero": XOR in the zero-th round key.
	vpxord	%zmm13,%zmm0,%zmm0
	vpxord	%zmm13,%zmm1,%zmm1
	vpxord	%zmm13,%zmm2,%zmm2
	vpxord	%zmm13,%zmm3,%zmm3

	cmpl	$24,%r10d
	jl	.Laes128__func2
	je	.Laes192__func2
// AES-256
	vbroadcasti32x4	-208(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

	vbroadcasti32x4	-192(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

.Laes192__func2:
	vbroadcasti32x4	-176(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

	vbroadcasti32x4	-160(%r11),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	vaesenc	%zmm9,%zmm1,%zmm1
	vaesenc	%zmm9,%zmm2,%zmm2
	vaesenc	%zmm9,%zmm3,%zmm3

.Laes128__func2:

// Prefetch the source data 512 bytes ahead into the L1 data cache, to
// improve performance when the hardware prefetcher is disabled.  Assumes the
// L1 data cache line size is 64 bytes (de facto standard on x86_64).
	prefetcht0	512+0(%rdi)
	prefetcht0	512+64(%rdi)
	prefetcht0	512+128(%rdi)
	prefetcht0	512+192(%rdi)

// Finish the AES encryption of the counter blocks in AESDATA[0-3],
// interleaved with the GHASH update of the ciphertext blocks in
// GHASHDATA[0-3].
	vpshufb	%zmm8,%zmm4,%zmm4
	vpxord	%zmm10,%zmm4,%zmm4
	vpshufb	%zmm8,%zmm5,%zmm5
	vpshufb	%zmm8,%zmm6,%zmm6

	vaesenc	%zmm15,%zmm0,%zmm0
	vaesenc	%zmm15,%zmm1,%zmm1
	vaesenc	%zmm15,%zmm2,%zmm2
	vaesenc	%zmm15,%zmm3,%zmm3

	vpshufb	%zmm8,%zmm7,%zmm7
	vpclmulqdq	$0x00,%zmm27,%zmm4,%zmm10  // LO_0
	vpclmulqdq	$0x00,%zmm28,%zmm5,%zmm24  // LO_1
	vpclmulqdq	$0x00,%zmm29,%zmm6,%zmm25  // LO_2

	vaesenc	%zmm16,%zmm0,%zmm0
	vaesenc	%zmm16,%zmm1,%zmm1
	vaesenc	%zmm16,%zmm2,%zmm2
	vaesenc	%zmm16,%zmm3,%zmm3

	vpxord	%zmm24,%zmm10,%zmm10  // sum(LO_{1,0})
	vpclmulqdq	$0x00,%zmm30,%zmm7,%zmm26  // LO_3
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm10  // LO = sum(LO_{3,2,1,0})
	vpclmulqdq	$0x01,%zmm27,%zmm4,%zmm24  // MI_0

	vaesenc	%zmm17,%zmm0,%zmm0
	vaesenc	%zmm17,%zmm1,%zmm1
	vaesenc	%zmm17,%zmm2,%zmm2
	vaesenc	%zmm17,%zmm3,%zmm3

	vpclmulqdq	$0x01,%zmm28,%zmm5,%zmm25  // MI_1
	vpclmulqdq	$0x01,%zmm29,%zmm6,%zmm26  // MI_2
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{2,1,0})
	vpclmulqdq	$0x01,%zmm30,%zmm7,%zmm25  // MI_3

	vaesenc	%zmm18,%zmm0,%zmm0
	vaesenc	%zmm18,%zmm1,%zmm1
	vaesenc	%zmm18,%zmm2,%zmm2
	vaesenc	%zmm18,%zmm3,%zmm3

	vpclmulqdq	$0x10,%zmm27,%zmm4,%zmm26  // MI_4
	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{4,3,2,1,0})
	vpclmulqdq	$0x10,%zmm28,%zmm5,%zmm25  // MI_5
	vpclmulqdq	$0x10,%zmm29,%zmm6,%zmm26  // MI_6

	vaesenc	%zmm19,%zmm0,%zmm0
	vaesenc	%zmm19,%zmm1,%zmm1
	vaesenc	%zmm19,%zmm2,%zmm2
	vaesenc	%zmm19,%zmm3,%zmm3

	vpternlogd	$0x96,%zmm26,%zmm25,%zmm24  // sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	$0x01,%zmm10,%zmm31,%zmm26  // LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	$0x10,%zmm30,%zmm7,%zmm25  // MI_7
	vpxord	%zmm25,%zmm24,%zmm24  // MI = sum(MI_{7,6,5,4,3,2,1,0})

	vaesenc	%zmm20,%zmm0,%zmm0
	vaesenc	%zmm20,%zmm1,%zmm1
	vaesenc	%zmm20,%zmm2,%zmm2
	vaesenc	%zmm20,%zmm3,%zmm3

	vpshufd	$0x4e,%zmm10,%zmm10  // Swap halves of LO
	vpclmulqdq	$0x11,%zmm27,%zmm4,%zmm4  // HI_0
	vpclmulqdq	$0x11,%zmm28,%zmm5,%zmm5  // HI_1
	vpclmulqdq	$0x11,%zmm29,%zmm6,%zmm6  // HI_2

	vaesenc	%zmm21,%zmm0,%zmm0
	vaesenc	%zmm21,%zmm1,%zmm1
	vaesenc	%zmm21,%zmm2,%zmm2
	vaesenc	%zmm21,%zmm3,%zmm3

	vpternlogd	$0x96,%zmm26,%zmm10,%zmm24  // Fold LO into MI
	vpclmulqdq	$0x11,%zmm30,%zmm7,%zmm7  // HI_3
	vpternlogd	$0x96,%zmm6,%zmm5,%zmm4  // sum(HI_{2,1,0})
	vpclmulqdq	$0x01,%zmm24,%zmm31,%zmm25  // MI_L*(x^63 + x^62 + x^57)

	vaesenc	%zmm22,%zmm0,%zmm0
	vaesenc	%zmm22,%zmm1,%zmm1
	vaesenc	%zmm22,%zmm2,%zmm2
	vaesenc	%zmm22,%zmm3,%zmm3

	vpxord	%zmm7,%zmm4,%zmm10  // HI = sum(HI_{3,2,1,0})
	vpshufd	$0x4e,%zmm24,%zmm24  // Swap halves of MI
	vpternlogd	$0x96,%zmm25,%zmm24,%zmm10  // Fold MI into HI

	vaesenc	%zmm23,%zmm0,%zmm0
	vaesenc	%zmm23,%zmm1,%zmm1
	vaesenc	%zmm23,%zmm2,%zmm2
	vaesenc	%zmm23,%zmm3,%zmm3


	vextracti32x4	$1,%zmm10,%xmm4
	vextracti32x4	$2,%zmm10,%xmm5
	vextracti32x4	$3,%zmm10,%xmm6
	vpxord	%xmm4,%xmm10,%xmm10
	vpternlogd	$0x96,%xmm5,%xmm6,%xmm10

	vpxord	0(%rdi),%zmm14,%zmm4
	vpxord	64(%rdi),%zmm14,%zmm5
	vpxord	128(%rdi),%zmm14,%zmm6
	vpxord	192(%rdi),%zmm14,%zmm7
	vaesenclast	%zmm4,%zmm0,%zmm4
	vaesenclast	%zmm5,%zmm1,%zmm5
	vaesenclast	%zmm6,%zmm2,%zmm6
	vaesenclast	%zmm7,%zmm3,%zmm7
	vmovdqu8	%zmm4,0(%rsi)
	vmovdqu8	%zmm5,64(%rsi)
	vmovdqu8	%zmm6,128(%rsi)
	vmovdqu8	%zmm7,192(%rsi)

	addq	$256,%rdi
	addq	$256,%rsi
	subq	$256,%rdx
	cmpq	$256,%rdx
	jae	.Lcrypt_loop_4x__func2
.Lcrypt_loop_4x_done__func2:
// Check whether any data remains.
	testq	%rdx,%rdx
	jz	.Ldone__func2

// The data length isn't a multiple of 256 bytes.  Process the remaining
// data of length 1 <= DATALEN < 256, up to one 64-byte vector at a time.
// Going one vector at a time may seem inefficient compared to having
// separate code paths for each possible number of vectors remaining.
// However, using a loop keeps the code size down, and it performs
// surprising well; modern CPUs will start executing the next iteration
// before the previous one finishes and also predict the number of loop
// iterations.  For a similar reason, we roll up the AES rounds.
// 
// On the last iteration, the remaining length may be less than 64 bytes.
// Handle this using masking.
// 
// Since there are enough key powers available for all remaining data,
// there is no need to do a GHASH reduction after each iteration.
// Instead, multiply each remaining block by its own key power, and only
// do a GHASH reduction at the very end.

// Make POWERS_PTR point to the key powers [H^N, H^(N-1), ...] where N
// is the number of blocks that remain.
	movq	%rdx,%rax
	negq	%rax
	andq	$-16,%rax  // -round_up(DATALEN, 16)
	leaq	256(%r9,%rax,1),%r8
	vpxor	%xmm4,%xmm4,%xmm4
	vpxor	%xmm5,%xmm5,%xmm5
	vpxor	%xmm6,%xmm6,%xmm6

	cmpq	$64,%rdx
	jb	.Lpartial_vec__func2

.Lcrypt_loop_1x__func2:
// Process a full 64-byte vector.

// Encrypt a vector of counter blocks.
	vpshufb	%zmm8,%zmm12,%zmm0
	vpaddd	%zmm11,%zmm12,%zmm12
	vpxord	%zmm13,%zmm0,%zmm0
	leaq	16(%rcx),%rax
.Lvaesenc_loop_tail_full_vec__func2:
	vbroadcasti32x4	(%rax),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	.Lvaesenc_loop_tail_full_vec__func2
	vaesenclast	%zmm14,%zmm0,%zmm0

// XOR the data with the vector of keystream blocks.
	vmovdqu8	(%rdi),%zmm1
	vpxord	%zmm1,%zmm0,%zmm0
	vmovdqu8	%zmm0,(%rsi)

// Update GHASH with the ciphertext blocks, without reducing.
	vmovdqu8	(%r8),%zmm30
	vpshufb	%zmm8,%zmm1,%zmm0
	vpxord	%zmm10,%zmm0,%zmm0
	vpclmulqdq	$0x00,%zmm30,%zmm0,%zmm7  // a_L * b_L
	vpclmulqdq	$0x01,%zmm30,%zmm0,%zmm1  // a_L * b_H
	vpclmulqdq	$0x10,%zmm30,%zmm0,%zmm2  // a_H * b_L
	vpclmulqdq	$0x11,%zmm30,%zmm0,%zmm3  // a_H * b_H
	vpxord	%zmm7,%zmm4,%zmm4
	vpternlogd	$0x96,%zmm2,%zmm1,%zmm5
	vpxord	%zmm3,%zmm6,%zmm6

	vpxor	%xmm10,%xmm10,%xmm10

	addq	$64,%r8
	addq	$64,%rdi
	addq	$64,%rsi
	subq	$64,%rdx
	cmpq	$64,%rdx
	jae	.Lcrypt_loop_1x__func2

	testq	%rdx,%rdx
	jz	.Lreduce__func2

.Lpartial_vec__func2:
// Process a partial vector of length 1 <= DATALEN < 64.

// Set the data mask %k1 to DATALEN 1's.
// Set the key powers mask %k2 to round_up(DATALEN, 16) 1's.
	movq	$-1,%rax
	bzhiq	%rdx,%rax,%rax
	kmovq	%rax,%k1
	addq	$15,%rdx
	andq	$-16,%rdx
	movq	$-1,%rax
	bzhiq	%rdx,%rax,%rax
	kmovq	%rax,%k2

// Encrypt one last vector of counter blocks.  This does not need to be
// masked.  The counter does not need to be incremented here.
	vpshufb	%zmm8,%zmm12,%zmm0
	vpxord	%zmm13,%zmm0,%zmm0
	leaq	16(%rcx),%rax
.Lvaesenc_loop_tail_partialvec__func2:
	vbroadcasti32x4	(%rax),%zmm9
	vaesenc	%zmm9,%zmm0,%zmm0
	addq	$16,%rax
	cmpq	%rax,%r11
	jne	.Lvaesenc_loop_tail_partialvec__func2
	vaesenclast	%zmm14,%zmm0,%zmm0

// XOR the data with the appropriate number of keystream bytes.
	vmovdqu8	(%rdi),%zmm1{%k1}{z}
	vpxord	%zmm1,%zmm0,%zmm0
	vmovdqu8	%zmm0,(%rsi){%k1}

// Update GHASH with the ciphertext block(s), without reducing.
// 
// In the case of DATALEN < 64, the ciphertext is zero-padded to 64
// bytes.  (If decrypting, it's done by the above masked load.  If
// encrypting, it's done by the below masked register-to-register move.)
// Note that if DATALEN <= 48, there will be additional padding beyond
// the padding of the last block specified by GHASH itself; i.e., there
// may be whole block(s) that get processed by the GHASH multiplication
// and reduction instructions but should not actually be included in the
// GHASH.  However, any such blocks are all-zeroes, and the values that
// they're multiplied with are also all-zeroes.  Therefore they just add
// 0 * 0 = 0 to the final GHASH result, which makes no difference.
	vmovdqu8	(%r8),%zmm30{%k2}{z}

	vpshufb	%zmm8,%zmm1,%zmm0
	vpxord	%zmm10,%zmm0,%zmm0
	vpclmulqdq	$0x00,%zmm30,%zmm0,%zmm7  // a_L * b_L
	vpclmulqdq	$0x01,%zmm30,%zmm0,%zmm1  // a_L * b_H
	vpclmulqdq	$0x10,%zmm30,%zmm0,%zmm2  // a_H * b_L
	vpclmulqdq	$0x11,%zmm30,%zmm0,%zmm3  // a_H * b_H
	vpxord	%zmm7,%zmm4,%zmm4
	vpternlogd	$0x96,%zmm2,%zmm1,%zmm5
	vpxord	%zmm3,%zmm6,%zmm6


.Lreduce__func2:
// Finally, do the GHASH reduction.
	vpclmulqdq	$0x01,%zmm4,%zmm31,%zmm0
	vpshufd	$0x4e,%zmm4,%zmm4
	vpternlogd	$0x96,%zmm0,%zmm4,%zmm5
	vpclmulqdq	$0x01,%zmm5,%zmm31,%zmm0
	vpshufd	$0x4e,%zmm5,%zmm5
	vpternlogd	$0x96,%zmm0,%zmm5,%zmm6

	vextracti32x4	$1,%zmm6,%xmm0
	vextracti32x4	$2,%zmm6,%xmm1
	vextracti32x4	$3,%zmm6,%xmm2
	vpxord	%xmm0,%xmm6,%xmm10
	vpternlogd	$0x96,%xmm1,%xmm2,%xmm10


.Ldone__func2:
// Store the updated GHASH accumulator back to memory.
	vpshufb	%xmm8,%xmm10,%xmm10
	vmovdqu	%xmm10,(%r12)

	vzeroupper  // This is needed after using ymm or zmm registers.
	popq	%r12
.cfi_adjust_cfa_offset	-8
.cfi_restore	%r12
	ret

.cfi_endproc	
.size	aes_gcm_dec_update_vaes_avx512, . - aes_gcm_dec_update_vaes_avx512
#endif
