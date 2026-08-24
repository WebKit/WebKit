// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	

// gcm_gmult_ssse3 multiplies |Xi| by |Htable| and writes the result to |Xi|.
// |Xi| is represented in GHASH's serialized byte representation. |Htable| is
// formatted as described above.
// void gcm_gmult_ssse3(uint64_t Xi[2], const u128 Htable[16]);

.globl	_gcm_gmult_ssse3
.private_extern _gcm_gmult_ssse3
.p2align	4
_gcm_gmult_ssse3:


_CET_ENDBR
	movdqu	(%rdi),%xmm0
	movdqa	L$reverse_bytes(%rip),%xmm10
	movdqa	L$low4_mask(%rip),%xmm2

// Reverse input bytes to deserialize.
	pshufb	%xmm10,%xmm0

// Split each byte into low (%xmm0) and high (%xmm1) halves.
	movdqa	%xmm2,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm2,%xmm0

// Maintain the result in %xmm2 (the value) and %xmm3 (carry bits). Note
// that, due to bit reversal, %xmm3 contains bits that fall off when
// right-shifting, not left-shifting.
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	movq	$5,%rax
L$oop_row_1:
	movdqu	(%rsi),%xmm4
	leaq	16(%rsi),%rsi

// Right-shift %xmm2 and %xmm3 by 8 bytes.
	movdqa	%xmm2,%xmm6
	palignr	$1,%xmm3,%xmm6
	movdqa	%xmm6,%xmm3
	psrldq	$1,%xmm2

// Load the next table row and index the low and high bits of the input.
// Note the low (respectively, high) half corresponds to more
// (respectively, less) significant coefficients.
	movdqa	%xmm4,%xmm5
	pshufb	%xmm0,%xmm4
	pshufb	%xmm1,%xmm5

// Add the high half (%xmm5) without shifting.
	pxor	%xmm5,%xmm2

// Add the low half (%xmm4). This must be right-shifted by 4 bits. First,
// add into the carry register (%xmm3).
	movdqa	%xmm4,%xmm5
	psllq	$60,%xmm5
	movdqa	%xmm5,%xmm6
	pslldq	$8,%xmm6
	pxor	%xmm6,%xmm3

// Next, add into %xmm2.
	psrldq	$8,%xmm5
	pxor	%xmm5,%xmm2
	psrlq	$4,%xmm4
	pxor	%xmm4,%xmm2

	subq	$1,%rax
	jnz	L$oop_row_1

// Reduce the carry register. The reduction polynomial is 1 + x + x^2 +
// x^7, so we shift and XOR four times.
	pxor	%xmm3,%xmm2  // x^0 = 0
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^1 = x
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1) = x^2
	psrlq	$5,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1+5) = x^7
	pxor	%xmm3,%xmm3
	movq	$5,%rax
L$oop_row_2:
	movdqu	(%rsi),%xmm4
	leaq	16(%rsi),%rsi

// Right-shift %xmm2 and %xmm3 by 8 bytes.
	movdqa	%xmm2,%xmm6
	palignr	$1,%xmm3,%xmm6
	movdqa	%xmm6,%xmm3
	psrldq	$1,%xmm2

// Load the next table row and index the low and high bits of the input.
// Note the low (respectively, high) half corresponds to more
// (respectively, less) significant coefficients.
	movdqa	%xmm4,%xmm5
	pshufb	%xmm0,%xmm4
	pshufb	%xmm1,%xmm5

// Add the high half (%xmm5) without shifting.
	pxor	%xmm5,%xmm2

// Add the low half (%xmm4). This must be right-shifted by 4 bits. First,
// add into the carry register (%xmm3).
	movdqa	%xmm4,%xmm5
	psllq	$60,%xmm5
	movdqa	%xmm5,%xmm6
	pslldq	$8,%xmm6
	pxor	%xmm6,%xmm3

// Next, add into %xmm2.
	psrldq	$8,%xmm5
	pxor	%xmm5,%xmm2
	psrlq	$4,%xmm4
	pxor	%xmm4,%xmm2

	subq	$1,%rax
	jnz	L$oop_row_2

// Reduce the carry register. The reduction polynomial is 1 + x + x^2 +
// x^7, so we shift and XOR four times.
	pxor	%xmm3,%xmm2  // x^0 = 0
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^1 = x
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1) = x^2
	psrlq	$5,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1+5) = x^7
	pxor	%xmm3,%xmm3
	movq	$6,%rax
L$oop_row_3:
	movdqu	(%rsi),%xmm4
	leaq	16(%rsi),%rsi

// Right-shift %xmm2 and %xmm3 by 8 bytes.
	movdqa	%xmm2,%xmm6
	palignr	$1,%xmm3,%xmm6
	movdqa	%xmm6,%xmm3
	psrldq	$1,%xmm2

// Load the next table row and index the low and high bits of the input.
// Note the low (respectively, high) half corresponds to more
// (respectively, less) significant coefficients.
	movdqa	%xmm4,%xmm5
	pshufb	%xmm0,%xmm4
	pshufb	%xmm1,%xmm5

// Add the high half (%xmm5) without shifting.
	pxor	%xmm5,%xmm2

// Add the low half (%xmm4). This must be right-shifted by 4 bits. First,
// add into the carry register (%xmm3).
	movdqa	%xmm4,%xmm5
	psllq	$60,%xmm5
	movdqa	%xmm5,%xmm6
	pslldq	$8,%xmm6
	pxor	%xmm6,%xmm3

// Next, add into %xmm2.
	psrldq	$8,%xmm5
	pxor	%xmm5,%xmm2
	psrlq	$4,%xmm4
	pxor	%xmm4,%xmm2

	subq	$1,%rax
	jnz	L$oop_row_3

// Reduce the carry register. The reduction polynomial is 1 + x + x^2 +
// x^7, so we shift and XOR four times.
	pxor	%xmm3,%xmm2  // x^0 = 0
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^1 = x
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1) = x^2
	psrlq	$5,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1+5) = x^7
	pxor	%xmm3,%xmm3
// Store the result. Reverse bytes to serialize.
	pshufb	%xmm10,%xmm2
	movdqu	%xmm2,(%rdi)

// Zero any registers which contain secrets.
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	ret



// gcm_ghash_ssse3 incorporates |len| bytes from |in| to |Xi|, using |Htable| as
// the key. It writes the result back to |Xi|. |Xi| is represented in GHASH's
// serialized byte representation. |Htable| is formatted as described above.
// void gcm_ghash_ssse3(uint64_t Xi[2], const u128 Htable[16], const uint8_t *in,
// size_t len);

.globl	_gcm_ghash_ssse3
.private_extern _gcm_ghash_ssse3
.p2align	4
.alt_entry _gcm_ghash_ssse3
_gcm_ghash_ssse3:


_CET_ENDBR
	movdqu	(%rdi),%xmm0
	movdqa	L$reverse_bytes(%rip),%xmm10
	movdqa	L$low4_mask(%rip),%xmm11

// This function only processes whole blocks.
	andq	$-16,%rcx

// Reverse input bytes to deserialize. We maintain the running
// total in %xmm0.
	pshufb	%xmm10,%xmm0

// Iterate over each block. On entry to each iteration, %xmm3 is zero.
	pxor	%xmm3,%xmm3
L$oop_ghash:
// Incorporate the next block of input.
	movdqu	(%rdx),%xmm1
	pshufb	%xmm10,%xmm1  // Reverse bytes.
	pxor	%xmm1,%xmm0

// Split each byte into low (%xmm0) and high (%xmm1) halves.
	movdqa	%xmm11,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm11,%xmm0

// Maintain the result in %xmm2 (the value) and %xmm3 (carry bits). Note
// that, due to bit reversal, %xmm3 contains bits that fall off when
// right-shifting, not left-shifting.
	pxor	%xmm2,%xmm2
// %xmm3 is already zero at this point.
	movq	$5,%rax
L$oop_row_4:
	movdqu	(%rsi),%xmm4
	leaq	16(%rsi),%rsi

// Right-shift %xmm2 and %xmm3 by 8 bytes.
	movdqa	%xmm2,%xmm6
	palignr	$1,%xmm3,%xmm6
	movdqa	%xmm6,%xmm3
	psrldq	$1,%xmm2

// Load the next table row and index the low and high bits of the input.
// Note the low (respectively, high) half corresponds to more
// (respectively, less) significant coefficients.
	movdqa	%xmm4,%xmm5
	pshufb	%xmm0,%xmm4
	pshufb	%xmm1,%xmm5

// Add the high half (%xmm5) without shifting.
	pxor	%xmm5,%xmm2

// Add the low half (%xmm4). This must be right-shifted by 4 bits. First,
// add into the carry register (%xmm3).
	movdqa	%xmm4,%xmm5
	psllq	$60,%xmm5
	movdqa	%xmm5,%xmm6
	pslldq	$8,%xmm6
	pxor	%xmm6,%xmm3

// Next, add into %xmm2.
	psrldq	$8,%xmm5
	pxor	%xmm5,%xmm2
	psrlq	$4,%xmm4
	pxor	%xmm4,%xmm2

	subq	$1,%rax
	jnz	L$oop_row_4

// Reduce the carry register. The reduction polynomial is 1 + x + x^2 +
// x^7, so we shift and XOR four times.
	pxor	%xmm3,%xmm2  // x^0 = 0
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^1 = x
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1) = x^2
	psrlq	$5,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1+5) = x^7
	pxor	%xmm3,%xmm3
	movq	$5,%rax
L$oop_row_5:
	movdqu	(%rsi),%xmm4
	leaq	16(%rsi),%rsi

// Right-shift %xmm2 and %xmm3 by 8 bytes.
	movdqa	%xmm2,%xmm6
	palignr	$1,%xmm3,%xmm6
	movdqa	%xmm6,%xmm3
	psrldq	$1,%xmm2

// Load the next table row and index the low and high bits of the input.
// Note the low (respectively, high) half corresponds to more
// (respectively, less) significant coefficients.
	movdqa	%xmm4,%xmm5
	pshufb	%xmm0,%xmm4
	pshufb	%xmm1,%xmm5

// Add the high half (%xmm5) without shifting.
	pxor	%xmm5,%xmm2

// Add the low half (%xmm4). This must be right-shifted by 4 bits. First,
// add into the carry register (%xmm3).
	movdqa	%xmm4,%xmm5
	psllq	$60,%xmm5
	movdqa	%xmm5,%xmm6
	pslldq	$8,%xmm6
	pxor	%xmm6,%xmm3

// Next, add into %xmm2.
	psrldq	$8,%xmm5
	pxor	%xmm5,%xmm2
	psrlq	$4,%xmm4
	pxor	%xmm4,%xmm2

	subq	$1,%rax
	jnz	L$oop_row_5

// Reduce the carry register. The reduction polynomial is 1 + x + x^2 +
// x^7, so we shift and XOR four times.
	pxor	%xmm3,%xmm2  // x^0 = 0
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^1 = x
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1) = x^2
	psrlq	$5,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1+5) = x^7
	pxor	%xmm3,%xmm3
	movq	$6,%rax
L$oop_row_6:
	movdqu	(%rsi),%xmm4
	leaq	16(%rsi),%rsi

// Right-shift %xmm2 and %xmm3 by 8 bytes.
	movdqa	%xmm2,%xmm6
	palignr	$1,%xmm3,%xmm6
	movdqa	%xmm6,%xmm3
	psrldq	$1,%xmm2

// Load the next table row and index the low and high bits of the input.
// Note the low (respectively, high) half corresponds to more
// (respectively, less) significant coefficients.
	movdqa	%xmm4,%xmm5
	pshufb	%xmm0,%xmm4
	pshufb	%xmm1,%xmm5

// Add the high half (%xmm5) without shifting.
	pxor	%xmm5,%xmm2

// Add the low half (%xmm4). This must be right-shifted by 4 bits. First,
// add into the carry register (%xmm3).
	movdqa	%xmm4,%xmm5
	psllq	$60,%xmm5
	movdqa	%xmm5,%xmm6
	pslldq	$8,%xmm6
	pxor	%xmm6,%xmm3

// Next, add into %xmm2.
	psrldq	$8,%xmm5
	pxor	%xmm5,%xmm2
	psrlq	$4,%xmm4
	pxor	%xmm4,%xmm2

	subq	$1,%rax
	jnz	L$oop_row_6

// Reduce the carry register. The reduction polynomial is 1 + x + x^2 +
// x^7, so we shift and XOR four times.
	pxor	%xmm3,%xmm2  // x^0 = 0
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^1 = x
	psrlq	$1,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1) = x^2
	psrlq	$5,%xmm3
	pxor	%xmm3,%xmm2  // x^(1+1+5) = x^7
	pxor	%xmm3,%xmm3
	movdqa	%xmm2,%xmm0

// Rewind %rsi for the next iteration.
	leaq	-256(%rsi),%rsi

// Advance input and continue.
	leaq	16(%rdx),%rdx
	subq	$16,%rcx
	jnz	L$oop_ghash

// Reverse bytes and store the result.
	pshufb	%xmm10,%xmm0
	movdqu	%xmm0,(%rdi)

// Zero any registers which contain secrets.
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	ret




.section	__DATA,__const

.p2align	4
ghash_ssse3_constants:
// .Lreverse_bytes is a permutation which, if applied with pshufb, reverses the
// bytes in an XMM register.
L$reverse_bytes:
.byte	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
// .Llow4_mask is an XMM mask which selects the low four bits of each byte.
L$low4_mask:
.quad	0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f
.text	
#endif
