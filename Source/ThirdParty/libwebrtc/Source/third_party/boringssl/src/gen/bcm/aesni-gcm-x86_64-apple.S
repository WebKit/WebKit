// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	


.p2align	5
_aesni_ctr32_ghash_6x:

	vmovdqu	32(%r11),%xmm2  // borrow %xmm2, .Lone_msb
	subq	$6,%rdx
	vpxor	%xmm4,%xmm4,%xmm4  // %xmm4   = 0
	vmovdqu	0-128(%rcx),%xmm15
	vpaddb	%xmm2,%xmm1,%xmm10
	vpaddb	%xmm2,%xmm10,%xmm11
	vpaddb	%xmm2,%xmm11,%xmm12
	vpaddb	%xmm2,%xmm12,%xmm13
	vpaddb	%xmm2,%xmm13,%xmm14
	vpxor	%xmm15,%xmm1,%xmm9
	vmovdqu	%xmm4,16+8(%rsp)  // "%xmm7" = 0
	jmp	L$oop6x

.p2align	5
L$oop6x:
	addl	$100663296,%ebx
	jc	L$handle_ctr32  // discard ?
	vmovdqu	0-32(%r9),%xmm3  // %xmm3^1
	vpaddb	%xmm2,%xmm14,%xmm1  // next counter value
	vpxor	%xmm15,%xmm10,%xmm10
	vpxor	%xmm15,%xmm11,%xmm11

L$resume_ctr32:
	vmovdqu	%xmm1,(%r8)  // save next counter value
	vpclmulqdq	$0x10,%xmm3,%xmm7,%xmm5
	vpxor	%xmm15,%xmm12,%xmm12
	vmovups	16-128(%rcx),%xmm2  // borrow %xmm2 for %xmm15
	vpclmulqdq	$0x01,%xmm3,%xmm7,%xmm6

// At this point, the current block of 96 (0x60) bytes has already been
// loaded into registers. Concurrently with processing it, we want to
// load the next 96 bytes of input for the next round. Obviously, we can
// only do this if there are at least 96 more bytes of input beyond the
// input we're currently processing, or else we'd read past the end of
// the input buffer. Here, we set |%r12| to 96 if there are at least 96
// bytes of input beyond the 96 bytes we're already processing, and we
// set |%r12| to 0 otherwise. In the case where we set |%r12| to 96,
// we'll read in the next block so that it is in registers for the next
// loop iteration. In the case where we set |%r12| to 0, we'll re-read
// the current block and then ignore what we re-read.
// 
// At this point, |%r14| points to the current (already read into
// registers) block, and |%r15| points to 2*96 bytes before the end of
// the input. Thus, |%r14| > |%r15| means that we do not have the next
// 96-byte block to read in, and |%r14| <= |%r15| means we do.
	xorq	%r12,%r12
	cmpq	%r14,%r15

	vaesenc	%xmm2,%xmm9,%xmm9
	vmovdqu	48+8(%rsp),%xmm0  // I[4]
	vpxor	%xmm15,%xmm13,%xmm13
	vpclmulqdq	$0x00,%xmm3,%xmm7,%xmm1
	vaesenc	%xmm2,%xmm10,%xmm10
	vpxor	%xmm15,%xmm14,%xmm14
	setnc	%r12b
	vpclmulqdq	$0x11,%xmm3,%xmm7,%xmm7
	vaesenc	%xmm2,%xmm11,%xmm11
	vmovdqu	16-32(%r9),%xmm3  // %xmm3^2
	negq	%r12
	vaesenc	%xmm2,%xmm12,%xmm12
	vpxor	%xmm5,%xmm6,%xmm6
	vpclmulqdq	$0x00,%xmm3,%xmm0,%xmm5
	vpxor	%xmm4,%xmm8,%xmm8  // modulo-scheduled
	vaesenc	%xmm2,%xmm13,%xmm13
	vpxor	%xmm5,%xmm1,%xmm4
	andq	$0x60,%r12
	vmovups	32-128(%rcx),%xmm15
	vpclmulqdq	$0x10,%xmm3,%xmm0,%xmm1
	vaesenc	%xmm2,%xmm14,%xmm14

	vpclmulqdq	$0x01,%xmm3,%xmm0,%xmm2
	leaq	(%r14,%r12,1),%r14
	vaesenc	%xmm15,%xmm9,%xmm9
	vpxor	16+8(%rsp),%xmm8,%xmm8  // modulo-scheduled [vpxor %xmm7,%xmm8,%xmm8]
	vpclmulqdq	$0x11,%xmm3,%xmm0,%xmm3
	vmovdqu	64+8(%rsp),%xmm0  // I[3]
	vaesenc	%xmm15,%xmm10,%xmm10
	movbeq	88(%r14),%r13
	vaesenc	%xmm15,%xmm11,%xmm11
	movbeq	80(%r14),%r12
	vaesenc	%xmm15,%xmm12,%xmm12
	movq	%r13,32+8(%rsp)
	vaesenc	%xmm15,%xmm13,%xmm13
	movq	%r12,40+8(%rsp)
	vmovdqu	48-32(%r9),%xmm5  // borrow %xmm5 for %xmm3^3
	vaesenc	%xmm15,%xmm14,%xmm14

	vmovups	48-128(%rcx),%xmm15
	vpxor	%xmm1,%xmm6,%xmm6
	vpclmulqdq	$0x00,%xmm5,%xmm0,%xmm1
	vaesenc	%xmm15,%xmm9,%xmm9
	vpxor	%xmm2,%xmm6,%xmm6
	vpclmulqdq	$0x10,%xmm5,%xmm0,%xmm2
	vaesenc	%xmm15,%xmm10,%xmm10
	vpxor	%xmm3,%xmm7,%xmm7
	vpclmulqdq	$0x01,%xmm5,%xmm0,%xmm3
	vaesenc	%xmm15,%xmm11,%xmm11
	vpclmulqdq	$0x11,%xmm5,%xmm0,%xmm5
	vmovdqu	80+8(%rsp),%xmm0  // I[2]
	vaesenc	%xmm15,%xmm12,%xmm12
	vaesenc	%xmm15,%xmm13,%xmm13
	vpxor	%xmm1,%xmm4,%xmm4
	vmovdqu	64-32(%r9),%xmm1  // borrow %xmm1 for %xmm3^4
	vaesenc	%xmm15,%xmm14,%xmm14

	vmovups	64-128(%rcx),%xmm15
	vpxor	%xmm2,%xmm6,%xmm6
	vpclmulqdq	$0x00,%xmm1,%xmm0,%xmm2
	vaesenc	%xmm15,%xmm9,%xmm9
	vpxor	%xmm3,%xmm6,%xmm6
	vpclmulqdq	$0x10,%xmm1,%xmm0,%xmm3
	vaesenc	%xmm15,%xmm10,%xmm10
	movbeq	72(%r14),%r13
	vpxor	%xmm5,%xmm7,%xmm7
	vpclmulqdq	$0x01,%xmm1,%xmm0,%xmm5
	vaesenc	%xmm15,%xmm11,%xmm11
	movbeq	64(%r14),%r12
	vpclmulqdq	$0x11,%xmm1,%xmm0,%xmm1
	vmovdqu	96+8(%rsp),%xmm0  // I[1]
	vaesenc	%xmm15,%xmm12,%xmm12
	movq	%r13,48+8(%rsp)
	vaesenc	%xmm15,%xmm13,%xmm13
	movq	%r12,56+8(%rsp)
	vpxor	%xmm2,%xmm4,%xmm4
	vmovdqu	96-32(%r9),%xmm2  // borrow %xmm2 for %xmm3^5
	vaesenc	%xmm15,%xmm14,%xmm14

	vmovups	80-128(%rcx),%xmm15
	vpxor	%xmm3,%xmm6,%xmm6
	vpclmulqdq	$0x00,%xmm2,%xmm0,%xmm3
	vaesenc	%xmm15,%xmm9,%xmm9
	vpxor	%xmm5,%xmm6,%xmm6
	vpclmulqdq	$0x10,%xmm2,%xmm0,%xmm5
	vaesenc	%xmm15,%xmm10,%xmm10
	movbeq	56(%r14),%r13
	vpxor	%xmm1,%xmm7,%xmm7
	vpclmulqdq	$0x01,%xmm2,%xmm0,%xmm1
	vpxor	112+8(%rsp),%xmm8,%xmm8  // accumulate I[0]
	vaesenc	%xmm15,%xmm11,%xmm11
	movbeq	48(%r14),%r12
	vpclmulqdq	$0x11,%xmm2,%xmm0,%xmm2
	vaesenc	%xmm15,%xmm12,%xmm12
	movq	%r13,64+8(%rsp)
	vaesenc	%xmm15,%xmm13,%xmm13
	movq	%r12,72+8(%rsp)
	vpxor	%xmm3,%xmm4,%xmm4
	vmovdqu	112-32(%r9),%xmm3  // %xmm3^6
	vaesenc	%xmm15,%xmm14,%xmm14

	vmovups	96-128(%rcx),%xmm15
	vpxor	%xmm5,%xmm6,%xmm6
	vpclmulqdq	$0x10,%xmm3,%xmm8,%xmm5
	vaesenc	%xmm15,%xmm9,%xmm9
	vpxor	%xmm1,%xmm6,%xmm6
	vpclmulqdq	$0x01,%xmm3,%xmm8,%xmm1
	vaesenc	%xmm15,%xmm10,%xmm10
	movbeq	40(%r14),%r13
	vpxor	%xmm2,%xmm7,%xmm7
	vpclmulqdq	$0x00,%xmm3,%xmm8,%xmm2
	vaesenc	%xmm15,%xmm11,%xmm11
	movbeq	32(%r14),%r12
	vpclmulqdq	$0x11,%xmm3,%xmm8,%xmm8
	vaesenc	%xmm15,%xmm12,%xmm12
	movq	%r13,80+8(%rsp)
	vaesenc	%xmm15,%xmm13,%xmm13
	movq	%r12,88+8(%rsp)
	vpxor	%xmm5,%xmm6,%xmm6
	vaesenc	%xmm15,%xmm14,%xmm14
	vpxor	%xmm1,%xmm6,%xmm6

	vmovups	112-128(%rcx),%xmm15
	vpslldq	$8,%xmm6,%xmm5
	vpxor	%xmm2,%xmm4,%xmm4
	vmovdqu	16(%r11),%xmm3  // .Lpoly

	vaesenc	%xmm15,%xmm9,%xmm9
	vpxor	%xmm8,%xmm7,%xmm7
	vaesenc	%xmm15,%xmm10,%xmm10
	vpxor	%xmm5,%xmm4,%xmm4
	movbeq	24(%r14),%r13
	vaesenc	%xmm15,%xmm11,%xmm11
	movbeq	16(%r14),%r12
	vpalignr	$8,%xmm4,%xmm4,%xmm0  // 1st phase
	vpclmulqdq	$0x10,%xmm3,%xmm4,%xmm4
	movq	%r13,96+8(%rsp)
	vaesenc	%xmm15,%xmm12,%xmm12
	movq	%r12,104+8(%rsp)
	vaesenc	%xmm15,%xmm13,%xmm13
	vmovups	128-128(%rcx),%xmm1  // borrow %xmm1 for %xmm15
	vaesenc	%xmm15,%xmm14,%xmm14

	vaesenc	%xmm1,%xmm9,%xmm9
	vmovups	144-128(%rcx),%xmm15
	vaesenc	%xmm1,%xmm10,%xmm10
	vpsrldq	$8,%xmm6,%xmm6
	vaesenc	%xmm1,%xmm11,%xmm11
	vpxor	%xmm6,%xmm7,%xmm7
	vaesenc	%xmm1,%xmm12,%xmm12
	vpxor	%xmm0,%xmm4,%xmm4
	movbeq	8(%r14),%r13
	vaesenc	%xmm1,%xmm13,%xmm13
	movbeq	0(%r14),%r12
	vaesenc	%xmm1,%xmm14,%xmm14
	vmovups	160-128(%rcx),%xmm1
	cmpl	$11,%r10d
	jb	L$enc_tail  // 128-bit key

	vaesenc	%xmm15,%xmm9,%xmm9
	vaesenc	%xmm15,%xmm10,%xmm10
	vaesenc	%xmm15,%xmm11,%xmm11
	vaesenc	%xmm15,%xmm12,%xmm12
	vaesenc	%xmm15,%xmm13,%xmm13
	vaesenc	%xmm15,%xmm14,%xmm14

	vaesenc	%xmm1,%xmm9,%xmm9
	vaesenc	%xmm1,%xmm10,%xmm10
	vaesenc	%xmm1,%xmm11,%xmm11
	vaesenc	%xmm1,%xmm12,%xmm12
	vaesenc	%xmm1,%xmm13,%xmm13
	vmovups	176-128(%rcx),%xmm15
	vaesenc	%xmm1,%xmm14,%xmm14
	vmovups	192-128(%rcx),%xmm1
	je	L$enc_tail  // 192-bit key

	vaesenc	%xmm15,%xmm9,%xmm9
	vaesenc	%xmm15,%xmm10,%xmm10
	vaesenc	%xmm15,%xmm11,%xmm11
	vaesenc	%xmm15,%xmm12,%xmm12
	vaesenc	%xmm15,%xmm13,%xmm13
	vaesenc	%xmm15,%xmm14,%xmm14

	vaesenc	%xmm1,%xmm9,%xmm9
	vaesenc	%xmm1,%xmm10,%xmm10
	vaesenc	%xmm1,%xmm11,%xmm11
	vaesenc	%xmm1,%xmm12,%xmm12
	vaesenc	%xmm1,%xmm13,%xmm13
	vmovups	208-128(%rcx),%xmm15
	vaesenc	%xmm1,%xmm14,%xmm14
	vmovups	224-128(%rcx),%xmm1
	jmp	L$enc_tail  // 256-bit key

.p2align	5
L$handle_ctr32:
	vmovdqu	(%r11),%xmm0  // borrow %xmm0 for .Lbswap_mask
	vpshufb	%xmm0,%xmm1,%xmm6  // byte-swap counter
	vmovdqu	48(%r11),%xmm5  // borrow %xmm5, .Ltwo_lsb
	vpaddd	64(%r11),%xmm6,%xmm10  // .Lone_lsb
	vpaddd	%xmm5,%xmm6,%xmm11
	vmovdqu	0-32(%r9),%xmm3  // %xmm3^1
	vpaddd	%xmm5,%xmm10,%xmm12
	vpshufb	%xmm0,%xmm10,%xmm10
	vpaddd	%xmm5,%xmm11,%xmm13
	vpshufb	%xmm0,%xmm11,%xmm11
	vpxor	%xmm15,%xmm10,%xmm10
	vpaddd	%xmm5,%xmm12,%xmm14
	vpshufb	%xmm0,%xmm12,%xmm12
	vpxor	%xmm15,%xmm11,%xmm11
	vpaddd	%xmm5,%xmm13,%xmm1  // byte-swapped next counter value
	vpshufb	%xmm0,%xmm13,%xmm13
	vpshufb	%xmm0,%xmm14,%xmm14
	vpshufb	%xmm0,%xmm1,%xmm1  // next counter value
	jmp	L$resume_ctr32

.p2align	5
L$enc_tail:
	vaesenc	%xmm15,%xmm9,%xmm9
	vmovdqu	%xmm7,16+8(%rsp)  // postpone vpxor %xmm7,%xmm8,%xmm8
	vpalignr	$8,%xmm4,%xmm4,%xmm8  // 2nd phase
	vaesenc	%xmm15,%xmm10,%xmm10
	vpclmulqdq	$0x10,%xmm3,%xmm4,%xmm4
	vpxor	0(%rdi),%xmm1,%xmm2
	vaesenc	%xmm15,%xmm11,%xmm11
	vpxor	16(%rdi),%xmm1,%xmm0
	vaesenc	%xmm15,%xmm12,%xmm12
	vpxor	32(%rdi),%xmm1,%xmm5
	vaesenc	%xmm15,%xmm13,%xmm13
	vpxor	48(%rdi),%xmm1,%xmm6
	vaesenc	%xmm15,%xmm14,%xmm14
	vpxor	64(%rdi),%xmm1,%xmm7
	vpxor	80(%rdi),%xmm1,%xmm3
	vmovdqu	(%r8),%xmm1  // load next counter value

	vaesenclast	%xmm2,%xmm9,%xmm9
	vmovdqu	32(%r11),%xmm2  // borrow %xmm2, .Lone_msb
	vaesenclast	%xmm0,%xmm10,%xmm10
	vpaddb	%xmm2,%xmm1,%xmm0
	movq	%r13,112+8(%rsp)
	leaq	96(%rdi),%rdi
// These two prefetches were added in BoringSSL. See change that added them.
	prefetcht0	512(%rdi)  // We use 96-byte block so prefetch 2 lines (128 bytes)
	prefetcht0	576(%rdi)
	vaesenclast	%xmm5,%xmm11,%xmm11
	vpaddb	%xmm2,%xmm0,%xmm5
	movq	%r12,120+8(%rsp)
	leaq	96(%rsi),%rsi
	vmovdqu	0-128(%rcx),%xmm15
	vaesenclast	%xmm6,%xmm12,%xmm12
	vpaddb	%xmm2,%xmm5,%xmm6
	vaesenclast	%xmm7,%xmm13,%xmm13
	vpaddb	%xmm2,%xmm6,%xmm7
	vaesenclast	%xmm3,%xmm14,%xmm14
	vpaddb	%xmm2,%xmm7,%xmm3

	addq	$0x60,%rax
	subq	$0x6,%rdx
	jc	L$6x_done

	vmovups	%xmm9,-96(%rsi)  // save output
	vpxor	%xmm15,%xmm1,%xmm9
	vmovups	%xmm10,-80(%rsi)
	vmovdqa	%xmm0,%xmm10  // 0 latency
	vmovups	%xmm11,-64(%rsi)
	vmovdqa	%xmm5,%xmm11  // 0 latency
	vmovups	%xmm12,-48(%rsi)
	vmovdqa	%xmm6,%xmm12  // 0 latency
	vmovups	%xmm13,-32(%rsi)
	vmovdqa	%xmm7,%xmm13  // 0 latency
	vmovups	%xmm14,-16(%rsi)
	vmovdqa	%xmm3,%xmm14  // 0 latency
	vmovdqu	32+8(%rsp),%xmm7  // I[5]
	jmp	L$oop6x

L$6x_done:
	vpxor	16+8(%rsp),%xmm8,%xmm8  // modulo-scheduled
	vpxor	%xmm4,%xmm8,%xmm8  // modulo-scheduled

	ret


.globl	_aesni_gcm_decrypt
.private_extern _aesni_gcm_decrypt

.p2align	5
.alt_entry _aesni_gcm_decrypt
_aesni_gcm_decrypt:


_CET_ENDBR
	xorq	%rax,%rax

// We call |_aesni_ctr32_ghash_6x|, which requires at least 96 (0x60)
// bytes of input.
	cmpq	$0x60,%rdx  // minimal accepted length
	jb	L$gcm_dec_abort

	pushq	%rbp


	movq	%rsp,%rbp  // save stack pointer

	pushq	%rbx


	pushq	%r12


	pushq	%r13


	pushq	%r14


	pushq	%r15


	vzeroupper

	movq	16(%rbp),%r12
	vmovdqu	(%r8),%xmm1  // input counter value
	addq	$-128,%rsp
	movl	12(%r8),%ebx
	leaq	L$bswap_mask(%rip),%r11
	leaq	-128(%rcx),%r14  // borrow %r14
	movq	$0xf80,%r15  // borrow %r15
	vmovdqu	(%r12),%xmm8  // load Xi
	andq	$-128,%rsp  // ensure stack alignment
	vmovdqu	(%r11),%xmm0  // borrow %xmm0 for .Lbswap_mask
	leaq	128(%rcx),%rcx  // size optimization
	leaq	32(%r9),%r9  // size optimization
	movl	240-128(%rcx),%r10d
	vpshufb	%xmm0,%xmm8,%xmm8

	andq	%r15,%r14
	andq	%rsp,%r15
	subq	%r14,%r15
	jc	L$dec_no_key_aliasing
	cmpq	$768,%r15
	jnc	L$dec_no_key_aliasing
	subq	%r15,%rsp  // avoid aliasing with key
L$dec_no_key_aliasing:

	vmovdqu	80(%rdi),%xmm7  // I[5]
	movq	%rdi,%r14
	vmovdqu	64(%rdi),%xmm4

// |_aesni_ctr32_ghash_6x| requires |%r15| to point to 2*96 (0xc0)
// bytes before the end of the input. Note, in particular, that this is
// correct even if |%rdx| is not an even multiple of 96 or 16. XXX: This
// seems to require that |%rdi| + |%rdx| >= 2*96 (0xc0); i.e. |%rdi| must
// not be near the very beginning of the address space when |%rdx| < 2*96
// (0xc0).
	leaq	-192(%rdi,%rdx,1),%r15

	vmovdqu	48(%rdi),%xmm5
	shrq	$4,%rdx
	xorq	%rax,%rax
	vmovdqu	32(%rdi),%xmm6
	vpshufb	%xmm0,%xmm7,%xmm7  // passed to _aesni_ctr32_ghash_6x
	vmovdqu	16(%rdi),%xmm2
	vpshufb	%xmm0,%xmm4,%xmm4
	vmovdqu	(%rdi),%xmm3
	vpshufb	%xmm0,%xmm5,%xmm5
	vmovdqu	%xmm4,48(%rsp)
	vpshufb	%xmm0,%xmm6,%xmm6
	vmovdqu	%xmm5,64(%rsp)
	vpshufb	%xmm0,%xmm2,%xmm2
	vmovdqu	%xmm6,80(%rsp)
	vpshufb	%xmm0,%xmm3,%xmm3
	vmovdqu	%xmm2,96(%rsp)
	vmovdqu	%xmm3,112(%rsp)

	call	_aesni_ctr32_ghash_6x

	movq	16(%rbp),%r12
	vmovups	%xmm9,-96(%rsi)  // save output
	vmovups	%xmm10,-80(%rsi)
	vmovups	%xmm11,-64(%rsi)
	vmovups	%xmm12,-48(%rsi)
	vmovups	%xmm13,-32(%rsi)
	vmovups	%xmm14,-16(%rsi)

	vpshufb	(%r11),%xmm8,%xmm8  // .Lbswap_mask
	vmovdqu	%xmm8,(%r12)  // output Xi

	vzeroupper
	leaq	-40(%rbp),%rsp  // restore %rsp to fixed allocation

	popq	%r15

	popq	%r14

	popq	%r13

	popq	%r12

	popq	%rbx

	popq	%rbp

L$gcm_dec_abort:
	ret




.p2align	5
.alt_entry _aesni_ctr32_6x
_aesni_ctr32_6x:

	vmovdqu	0-128(%rcx),%xmm4  // borrow %xmm4 for %xmm15
	vmovdqu	32(%r11),%xmm2  // borrow %xmm2, .Lone_msb
	leaq	-1(%r10),%r13
	vmovups	16-128(%rcx),%xmm15
	leaq	32-128(%rcx),%r12
	vpxor	%xmm4,%xmm1,%xmm9
	addl	$100663296,%ebx
	jc	L$handle_ctr32_2
	vpaddb	%xmm2,%xmm1,%xmm10
	vpaddb	%xmm2,%xmm10,%xmm11
	vpxor	%xmm4,%xmm10,%xmm10
	vpaddb	%xmm2,%xmm11,%xmm12
	vpxor	%xmm4,%xmm11,%xmm11
	vpaddb	%xmm2,%xmm12,%xmm13
	vpxor	%xmm4,%xmm12,%xmm12
	vpaddb	%xmm2,%xmm13,%xmm14
	vpxor	%xmm4,%xmm13,%xmm13
	vpaddb	%xmm2,%xmm14,%xmm1
	vpxor	%xmm4,%xmm14,%xmm14
	jmp	L$oop_ctr32

.p2align	4
L$oop_ctr32:
	vaesenc	%xmm15,%xmm9,%xmm9
	vaesenc	%xmm15,%xmm10,%xmm10
	vaesenc	%xmm15,%xmm11,%xmm11
	vaesenc	%xmm15,%xmm12,%xmm12
	vaesenc	%xmm15,%xmm13,%xmm13
	vaesenc	%xmm15,%xmm14,%xmm14
	vmovups	(%r12),%xmm15
	leaq	16(%r12),%r12
	decl	%r13d
	jnz	L$oop_ctr32

	vmovdqu	(%r12),%xmm3  // last round key
	vaesenc	%xmm15,%xmm9,%xmm9
	vpxor	0(%rdi),%xmm3,%xmm4
	vaesenc	%xmm15,%xmm10,%xmm10
	vpxor	16(%rdi),%xmm3,%xmm5
	vaesenc	%xmm15,%xmm11,%xmm11
	vpxor	32(%rdi),%xmm3,%xmm6
	vaesenc	%xmm15,%xmm12,%xmm12
	vpxor	48(%rdi),%xmm3,%xmm8
	vaesenc	%xmm15,%xmm13,%xmm13
	vpxor	64(%rdi),%xmm3,%xmm2
	vaesenc	%xmm15,%xmm14,%xmm14
	vpxor	80(%rdi),%xmm3,%xmm3
	leaq	96(%rdi),%rdi

	vaesenclast	%xmm4,%xmm9,%xmm9
	vaesenclast	%xmm5,%xmm10,%xmm10
	vaesenclast	%xmm6,%xmm11,%xmm11
	vaesenclast	%xmm8,%xmm12,%xmm12
	vaesenclast	%xmm2,%xmm13,%xmm13
	vaesenclast	%xmm3,%xmm14,%xmm14
	vmovups	%xmm9,0(%rsi)
	vmovups	%xmm10,16(%rsi)
	vmovups	%xmm11,32(%rsi)
	vmovups	%xmm12,48(%rsi)
	vmovups	%xmm13,64(%rsi)
	vmovups	%xmm14,80(%rsi)
	leaq	96(%rsi),%rsi

	ret
.p2align	5
L$handle_ctr32_2:
	vpshufb	%xmm0,%xmm1,%xmm6  // byte-swap counter
	vmovdqu	48(%r11),%xmm5  // borrow %xmm5, .Ltwo_lsb
	vpaddd	64(%r11),%xmm6,%xmm10  // .Lone_lsb
	vpaddd	%xmm5,%xmm6,%xmm11
	vpaddd	%xmm5,%xmm10,%xmm12
	vpshufb	%xmm0,%xmm10,%xmm10
	vpaddd	%xmm5,%xmm11,%xmm13
	vpshufb	%xmm0,%xmm11,%xmm11
	vpxor	%xmm4,%xmm10,%xmm10
	vpaddd	%xmm5,%xmm12,%xmm14
	vpshufb	%xmm0,%xmm12,%xmm12
	vpxor	%xmm4,%xmm11,%xmm11
	vpaddd	%xmm5,%xmm13,%xmm1  // byte-swapped next counter value
	vpshufb	%xmm0,%xmm13,%xmm13
	vpxor	%xmm4,%xmm12,%xmm12
	vpshufb	%xmm0,%xmm14,%xmm14
	vpxor	%xmm4,%xmm13,%xmm13
	vpshufb	%xmm0,%xmm1,%xmm1  // next counter value
	vpxor	%xmm4,%xmm14,%xmm14
	jmp	L$oop_ctr32



.globl	_aesni_gcm_encrypt
.private_extern _aesni_gcm_encrypt

.p2align	5
.alt_entry _aesni_gcm_encrypt
_aesni_gcm_encrypt:


_CET_ENDBR
#ifdef BORINGSSL_DISPATCH_TEST

	movb	$1,_BORINGSSL_function_hit+2(%rip)
#endif
	xorq	%rax,%rax

// We call |_aesni_ctr32_6x| twice, each call consuming 96 bytes of
// input. Then we call |_aesni_ctr32_ghash_6x|, which requires at
// least 96 more bytes of input.
	cmpq	$288,%rdx  // minimal accepted length
	jb	L$gcm_enc_abort

	pushq	%rbp


	movq	%rsp,%rbp  // save stack pointer

	pushq	%rbx


	pushq	%r12


	pushq	%r13


	pushq	%r14


	pushq	%r15


	vzeroupper

	vmovdqu	(%r8),%xmm1  // input counter value
	addq	$-128,%rsp
	movl	12(%r8),%ebx
	leaq	L$bswap_mask(%rip),%r11
	leaq	-128(%rcx),%r14  // borrow %r14
	movq	$0xf80,%r15  // borrow %r15
	leaq	128(%rcx),%rcx  // size optimization
	vmovdqu	(%r11),%xmm0  // borrow %xmm0 for .Lbswap_mask
	andq	$-128,%rsp  // ensure stack alignment
	movl	240-128(%rcx),%r10d

	andq	%r15,%r14
	andq	%rsp,%r15
	subq	%r14,%r15
	jc	L$enc_no_key_aliasing
	cmpq	$768,%r15
	jnc	L$enc_no_key_aliasing
	subq	%r15,%rsp  // avoid aliasing with key
L$enc_no_key_aliasing:

	movq	%rsi,%r14

// |_aesni_ctr32_ghash_6x| requires |%r15| to point to 2*96 (0xc0)
// bytes before the end of the input. Note, in particular, that this is
// correct even if |%rdx| is not an even multiple of 96 or 16. Unlike in
// the decryption case, there's no caveat that |%rsi| must not be near
// the very beginning of the address space, because we know that
// |%rdx| >= 3*96 from the check above, and so we know
// |%rsi| + |%rdx| >= 2*96 (0xc0).
	leaq	-192(%rsi,%rdx,1),%r15

	shrq	$4,%rdx

	call	_aesni_ctr32_6x
	vpshufb	%xmm0,%xmm9,%xmm8  // save bswapped output on stack
	vpshufb	%xmm0,%xmm10,%xmm2
	vmovdqu	%xmm8,112(%rsp)
	vpshufb	%xmm0,%xmm11,%xmm4
	vmovdqu	%xmm2,96(%rsp)
	vpshufb	%xmm0,%xmm12,%xmm5
	vmovdqu	%xmm4,80(%rsp)
	vpshufb	%xmm0,%xmm13,%xmm6
	vmovdqu	%xmm5,64(%rsp)
	vpshufb	%xmm0,%xmm14,%xmm7  // passed to _aesni_ctr32_ghash_6x
	vmovdqu	%xmm6,48(%rsp)

	call	_aesni_ctr32_6x

	movq	16(%rbp),%r12
	leaq	32(%r9),%r9  // size optimization
	vmovdqu	(%r12),%xmm8  // load Xi
	subq	$12,%rdx
	movq	$192,%rax
	vpshufb	%xmm0,%xmm8,%xmm8

	call	_aesni_ctr32_ghash_6x
	vmovdqu	32(%rsp),%xmm7  // I[5]
	vmovdqu	(%r11),%xmm0  // borrow %xmm0 for .Lbswap_mask
	vmovdqu	0-32(%r9),%xmm3  // %xmm3^1
	vpunpckhqdq	%xmm7,%xmm7,%xmm1
	vmovdqu	32-32(%r9),%xmm15  // borrow %xmm15 for
	vmovups	%xmm9,-96(%rsi)  // save output
	vpshufb	%xmm0,%xmm9,%xmm9  // but keep bswapped copy
	vpxor	%xmm7,%xmm1,%xmm1
	vmovups	%xmm10,-80(%rsi)
	vpshufb	%xmm0,%xmm10,%xmm10
	vmovups	%xmm11,-64(%rsi)
	vpshufb	%xmm0,%xmm11,%xmm11
	vmovups	%xmm12,-48(%rsi)
	vpshufb	%xmm0,%xmm12,%xmm12
	vmovups	%xmm13,-32(%rsi)
	vpshufb	%xmm0,%xmm13,%xmm13
	vmovups	%xmm14,-16(%rsi)
	vpshufb	%xmm0,%xmm14,%xmm14
	vmovdqu	%xmm9,16(%rsp)  // free %xmm9
	vmovdqu	48(%rsp),%xmm6  // I[4]
	vmovdqu	16-32(%r9),%xmm0  // borrow %xmm0 for %xmm3^2
	vpunpckhqdq	%xmm6,%xmm6,%xmm2
	vpclmulqdq	$0x00,%xmm3,%xmm7,%xmm5
	vpxor	%xmm6,%xmm2,%xmm2
	vpclmulqdq	$0x11,%xmm3,%xmm7,%xmm7
	vpclmulqdq	$0x00,%xmm15,%xmm1,%xmm1

	vmovdqu	64(%rsp),%xmm9  // I[3]
	vpclmulqdq	$0x00,%xmm0,%xmm6,%xmm4
	vmovdqu	48-32(%r9),%xmm3  // %xmm3^3
	vpxor	%xmm5,%xmm4,%xmm4
	vpunpckhqdq	%xmm9,%xmm9,%xmm5
	vpclmulqdq	$0x11,%xmm0,%xmm6,%xmm6
	vpxor	%xmm9,%xmm5,%xmm5
	vpxor	%xmm7,%xmm6,%xmm6
	vpclmulqdq	$0x10,%xmm15,%xmm2,%xmm2
	vmovdqu	80-32(%r9),%xmm15
	vpxor	%xmm1,%xmm2,%xmm2

	vmovdqu	80(%rsp),%xmm1  // I[2]
	vpclmulqdq	$0x00,%xmm3,%xmm9,%xmm7
	vmovdqu	64-32(%r9),%xmm0  // borrow %xmm0 for %xmm3^4
	vpxor	%xmm4,%xmm7,%xmm7
	vpunpckhqdq	%xmm1,%xmm1,%xmm4
	vpclmulqdq	$0x11,%xmm3,%xmm9,%xmm9
	vpxor	%xmm1,%xmm4,%xmm4
	vpxor	%xmm6,%xmm9,%xmm9
	vpclmulqdq	$0x00,%xmm15,%xmm5,%xmm5
	vpxor	%xmm2,%xmm5,%xmm5

	vmovdqu	96(%rsp),%xmm2  // I[1]
	vpclmulqdq	$0x00,%xmm0,%xmm1,%xmm6
	vmovdqu	96-32(%r9),%xmm3  // %xmm3^5
	vpxor	%xmm7,%xmm6,%xmm6
	vpunpckhqdq	%xmm2,%xmm2,%xmm7
	vpclmulqdq	$0x11,%xmm0,%xmm1,%xmm1
	vpxor	%xmm2,%xmm7,%xmm7
	vpxor	%xmm9,%xmm1,%xmm1
	vpclmulqdq	$0x10,%xmm15,%xmm4,%xmm4
	vmovdqu	128-32(%r9),%xmm15
	vpxor	%xmm5,%xmm4,%xmm4

	vpxor	112(%rsp),%xmm8,%xmm8  // accumulate I[0]
	vpclmulqdq	$0x00,%xmm3,%xmm2,%xmm5
	vmovdqu	112-32(%r9),%xmm0  // borrow %xmm0 for %xmm3^6
	vpunpckhqdq	%xmm8,%xmm8,%xmm9
	vpxor	%xmm6,%xmm5,%xmm5
	vpclmulqdq	$0x11,%xmm3,%xmm2,%xmm2
	vpxor	%xmm8,%xmm9,%xmm9
	vpxor	%xmm1,%xmm2,%xmm2
	vpclmulqdq	$0x00,%xmm15,%xmm7,%xmm7
	vpxor	%xmm4,%xmm7,%xmm4

	vpclmulqdq	$0x00,%xmm0,%xmm8,%xmm6
	vmovdqu	0-32(%r9),%xmm3  // %xmm3^1
	vpunpckhqdq	%xmm14,%xmm14,%xmm1
	vpclmulqdq	$0x11,%xmm0,%xmm8,%xmm8
	vpxor	%xmm14,%xmm1,%xmm1
	vpxor	%xmm5,%xmm6,%xmm5
	vpclmulqdq	$0x10,%xmm15,%xmm9,%xmm9
	vmovdqu	32-32(%r9),%xmm15
	vpxor	%xmm2,%xmm8,%xmm7
	vpxor	%xmm4,%xmm9,%xmm6

	vmovdqu	16-32(%r9),%xmm0  // borrow %xmm0 for %xmm3^2
	vpxor	%xmm5,%xmm7,%xmm9  // aggregated Karatsuba post-processing
	vpclmulqdq	$0x00,%xmm3,%xmm14,%xmm4
	vpxor	%xmm9,%xmm6,%xmm6
	vpunpckhqdq	%xmm13,%xmm13,%xmm2
	vpclmulqdq	$0x11,%xmm3,%xmm14,%xmm14
	vpxor	%xmm13,%xmm2,%xmm2
	vpslldq	$8,%xmm6,%xmm9
	vpclmulqdq	$0x00,%xmm15,%xmm1,%xmm1
	vpxor	%xmm9,%xmm5,%xmm8
	vpsrldq	$8,%xmm6,%xmm6
	vpxor	%xmm6,%xmm7,%xmm7

	vpclmulqdq	$0x00,%xmm0,%xmm13,%xmm5
	vmovdqu	48-32(%r9),%xmm3  // %xmm3^3
	vpxor	%xmm4,%xmm5,%xmm5
	vpunpckhqdq	%xmm12,%xmm12,%xmm9
	vpclmulqdq	$0x11,%xmm0,%xmm13,%xmm13
	vpxor	%xmm12,%xmm9,%xmm9
	vpxor	%xmm14,%xmm13,%xmm13
	vpalignr	$8,%xmm8,%xmm8,%xmm14  // 1st phase
	vpclmulqdq	$0x10,%xmm15,%xmm2,%xmm2
	vmovdqu	80-32(%r9),%xmm15
	vpxor	%xmm1,%xmm2,%xmm2

	vpclmulqdq	$0x00,%xmm3,%xmm12,%xmm4
	vmovdqu	64-32(%r9),%xmm0  // borrow %xmm0 for %xmm3^4
	vpxor	%xmm5,%xmm4,%xmm4
	vpunpckhqdq	%xmm11,%xmm11,%xmm1
	vpclmulqdq	$0x11,%xmm3,%xmm12,%xmm12
	vpxor	%xmm11,%xmm1,%xmm1
	vpxor	%xmm13,%xmm12,%xmm12
	vxorps	16(%rsp),%xmm7,%xmm7  // accumulate %xmm9
	vpclmulqdq	$0x00,%xmm15,%xmm9,%xmm9
	vpxor	%xmm2,%xmm9,%xmm9

	vpclmulqdq	$0x10,16(%r11),%xmm8,%xmm8
	vxorps	%xmm14,%xmm8,%xmm8

	vpclmulqdq	$0x00,%xmm0,%xmm11,%xmm5
	vmovdqu	96-32(%r9),%xmm3  // %xmm3^5
	vpxor	%xmm4,%xmm5,%xmm5
	vpunpckhqdq	%xmm10,%xmm10,%xmm2
	vpclmulqdq	$0x11,%xmm0,%xmm11,%xmm11
	vpxor	%xmm10,%xmm2,%xmm2
	vpalignr	$8,%xmm8,%xmm8,%xmm14  // 2nd phase
	vpxor	%xmm12,%xmm11,%xmm11
	vpclmulqdq	$0x10,%xmm15,%xmm1,%xmm1
	vmovdqu	128-32(%r9),%xmm15
	vpxor	%xmm9,%xmm1,%xmm1

	vxorps	%xmm7,%xmm14,%xmm14
	vpclmulqdq	$0x10,16(%r11),%xmm8,%xmm8
	vxorps	%xmm14,%xmm8,%xmm8

	vpclmulqdq	$0x00,%xmm3,%xmm10,%xmm4
	vmovdqu	112-32(%r9),%xmm0  // borrow %xmm0 for %xmm3^6
	vpxor	%xmm5,%xmm4,%xmm4
	vpunpckhqdq	%xmm8,%xmm8,%xmm9
	vpclmulqdq	$0x11,%xmm3,%xmm10,%xmm10
	vpxor	%xmm8,%xmm9,%xmm9
	vpxor	%xmm11,%xmm10,%xmm10
	vpclmulqdq	$0x00,%xmm15,%xmm2,%xmm2
	vpxor	%xmm1,%xmm2,%xmm2

	vpclmulqdq	$0x00,%xmm0,%xmm8,%xmm5
	vpclmulqdq	$0x11,%xmm0,%xmm8,%xmm7
	vpxor	%xmm4,%xmm5,%xmm5
	vpclmulqdq	$0x10,%xmm15,%xmm9,%xmm6
	vpxor	%xmm10,%xmm7,%xmm7
	vpxor	%xmm2,%xmm6,%xmm6

	vpxor	%xmm5,%xmm7,%xmm4  // aggregated Karatsuba post-processing
	vpxor	%xmm4,%xmm6,%xmm6
	vpslldq	$8,%xmm6,%xmm1
	vmovdqu	16(%r11),%xmm3  // .Lpoly
	vpsrldq	$8,%xmm6,%xmm6
	vpxor	%xmm1,%xmm5,%xmm8
	vpxor	%xmm6,%xmm7,%xmm7

	vpalignr	$8,%xmm8,%xmm8,%xmm2  // 1st phase
	vpclmulqdq	$0x10,%xmm3,%xmm8,%xmm8
	vpxor	%xmm2,%xmm8,%xmm8

	vpalignr	$8,%xmm8,%xmm8,%xmm2  // 2nd phase
	vpclmulqdq	$0x10,%xmm3,%xmm8,%xmm8
	vpxor	%xmm7,%xmm2,%xmm2
	vpxor	%xmm2,%xmm8,%xmm8
	movq	16(%rbp),%r12
	vpshufb	(%r11),%xmm8,%xmm8  // .Lbswap_mask
	vmovdqu	%xmm8,(%r12)  // output Xi

	vzeroupper
	leaq	-40(%rbp),%rsp  // restore %rsp to fixed allocation

	popq	%r15

	popq	%r14

	popq	%r13

	popq	%r12

	popq	%rbx

	popq	%rbp

L$gcm_enc_abort:
	ret



.section	__DATA,__const

.p2align	6
aesni_gcm_constants:
L$bswap_mask:
.byte	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
L$poly:
.byte	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xc2
L$one_msb:
.byte	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
L$two_lsb:
.byte	2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
L$one_lsb:
.byte	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
.byte	65,69,83,45,78,73,32,71,67,77,32,109,111,100,117,108,101,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	6
.text	
#endif
