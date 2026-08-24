// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	

// CRYPTO_rdrand writes eight bytes of random data from the hardware RNG to
// |out|. It returns one on success or zero on hardware failure.
// int CRYPTO_rdrand(uint8_t out[8]);
.globl	_CRYPTO_rdrand
.private_extern _CRYPTO_rdrand

.p2align	4
_CRYPTO_rdrand:

_CET_ENDBR
	xorq	%rax,%rax
	rdrand	%rdx
// An add-with-carry of zero effectively sets %rax to the carry flag.
	adcq	%rax,%rax
	movq	%rdx,0(%rdi)
	ret



// CRYPTO_rdrand_multiple8_buf fills |len| bytes at |buf| with random data from
// the hardware RNG. The |len| argument must be a multiple of eight. It returns
// one on success and zero on hardware failure.
// int CRYPTO_rdrand_multiple8_buf(uint8_t *buf, size_t len);
.globl	_CRYPTO_rdrand_multiple8_buf
.private_extern _CRYPTO_rdrand_multiple8_buf

.p2align	4
.alt_entry _CRYPTO_rdrand_multiple8_buf
_CRYPTO_rdrand_multiple8_buf:

_CET_ENDBR
	testq	%rsi,%rsi
	jz	L$out
	movq	$8,%rdx
L$loop:
	rdrand	%rcx
	jnc	L$err
	movq	%rcx,0(%rdi)
	addq	%rdx,%rdi
	subq	%rdx,%rsi
	jnz	L$loop
L$out:
	movq	$1,%rax
	ret
L$err:
	xorq	%rax,%rax
	ret


#endif
