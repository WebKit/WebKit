// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__ELF__)
.text	
.globl	aes_hw_encrypt
.hidden aes_hw_encrypt
.type	aes_hw_encrypt,@function
.align	16
aes_hw_encrypt:
.cfi_startproc	
_CET_ENDBR
#ifdef BORINGSSL_DISPATCH_TEST
.extern	BORINGSSL_function_hit
.hidden BORINGSSL_function_hit
	movb	$1,BORINGSSL_function_hit+1(%rip)
#endif
	movups	(%rdi),%xmm2  // load input
	movl	240(%rdx),%eax  // key->rounds
	movups	(%rdx),%xmm0
	movups	16(%rdx),%xmm1
	leaq	32(%rdx),%rdx
	xorps	%xmm0,%xmm2
.Loop_enc1_1:
	aesenc	%xmm1,%xmm2
	decl	%eax
	movups	(%rdx),%xmm1
	leaq	16(%rdx),%rdx
	jnz	.Loop_enc1_1  // loop body is 16 bytes
	aesenclast	%xmm1,%xmm2
	pxor	%xmm0,%xmm0  // clear register bank
	pxor	%xmm1,%xmm1
	movups	%xmm2,(%rsi)  // output
	pxor	%xmm2,%xmm2
	ret
.cfi_endproc	
.size	aes_hw_encrypt,.-aes_hw_encrypt

.globl	aes_hw_decrypt
.hidden aes_hw_decrypt
.type	aes_hw_decrypt,@function
.align	16
aes_hw_decrypt:
.cfi_startproc	
_CET_ENDBR
	movups	(%rdi),%xmm2  // load input
	movl	240(%rdx),%eax  // key->rounds
	movups	(%rdx),%xmm0
	movups	16(%rdx),%xmm1
	leaq	32(%rdx),%rdx
	xorps	%xmm0,%xmm2
.Loop_dec1_2:
	aesdec	%xmm1,%xmm2
	decl	%eax
	movups	(%rdx),%xmm1
	leaq	16(%rdx),%rdx
	jnz	.Loop_dec1_2  // loop body is 16 bytes
	aesdeclast	%xmm1,%xmm2
	pxor	%xmm0,%xmm0  // clear register bank
	pxor	%xmm1,%xmm1
	movups	%xmm2,(%rsi)  // output
	pxor	%xmm2,%xmm2
	ret
.cfi_endproc	
.size	aes_hw_decrypt, .-aes_hw_decrypt
.type	_aesni_encrypt2,@function
.align	16
_aesni_encrypt2:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	movups	32(%rcx),%xmm0
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	addq	$16,%rax

.Lenc_loop2:
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Lenc_loop2

	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenclast	%xmm0,%xmm2
	aesenclast	%xmm0,%xmm3
	ret
.cfi_endproc	
.size	_aesni_encrypt2,.-_aesni_encrypt2
.type	_aesni_decrypt2,@function
.align	16
_aesni_decrypt2:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	movups	32(%rcx),%xmm0
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	addq	$16,%rax

.Ldec_loop2:
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Ldec_loop2

	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdeclast	%xmm0,%xmm2
	aesdeclast	%xmm0,%xmm3
	ret
.cfi_endproc	
.size	_aesni_decrypt2,.-_aesni_decrypt2
.type	_aesni_encrypt3,@function
.align	16
_aesni_encrypt3:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	movups	32(%rcx),%xmm0
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	addq	$16,%rax

.Lenc_loop3:
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	aesenc	%xmm0,%xmm4
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Lenc_loop3

	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenclast	%xmm0,%xmm2
	aesenclast	%xmm0,%xmm3
	aesenclast	%xmm0,%xmm4
	ret
.cfi_endproc	
.size	_aesni_encrypt3,.-_aesni_encrypt3
.type	_aesni_decrypt3,@function
.align	16
_aesni_decrypt3:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	movups	32(%rcx),%xmm0
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	addq	$16,%rax

.Ldec_loop3:
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Ldec_loop3

	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdeclast	%xmm0,%xmm2
	aesdeclast	%xmm0,%xmm3
	aesdeclast	%xmm0,%xmm4
	ret
.cfi_endproc	
.size	_aesni_decrypt3,.-_aesni_decrypt3
.type	_aesni_encrypt4,@function
.align	16
_aesni_encrypt4:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	xorps	%xmm0,%xmm5
	movups	32(%rcx),%xmm0
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
.byte	0x0f,0x1f,0x00
	addq	$16,%rax

.Lenc_loop4:
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Lenc_loop4

	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	aesenclast	%xmm0,%xmm2
	aesenclast	%xmm0,%xmm3
	aesenclast	%xmm0,%xmm4
	aesenclast	%xmm0,%xmm5
	ret
.cfi_endproc	
.size	_aesni_encrypt4,.-_aesni_encrypt4
.type	_aesni_decrypt4,@function
.align	16
_aesni_decrypt4:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	xorps	%xmm0,%xmm4
	xorps	%xmm0,%xmm5
	movups	32(%rcx),%xmm0
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
.byte	0x0f,0x1f,0x00
	addq	$16,%rax

.Ldec_loop4:
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Ldec_loop4

	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdeclast	%xmm0,%xmm2
	aesdeclast	%xmm0,%xmm3
	aesdeclast	%xmm0,%xmm4
	aesdeclast	%xmm0,%xmm5
	ret
.cfi_endproc	
.size	_aesni_decrypt4,.-_aesni_decrypt4
.type	_aesni_encrypt6,@function
.align	16
_aesni_encrypt6:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	pxor	%xmm0,%xmm3
	pxor	%xmm0,%xmm4
	aesenc	%xmm1,%xmm2
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	aesenc	%xmm1,%xmm3
	pxor	%xmm0,%xmm5
	pxor	%xmm0,%xmm6
	aesenc	%xmm1,%xmm4
	pxor	%xmm0,%xmm7
	movups	(%rcx,%rax,1),%xmm0
	addq	$16,%rax
	jmp	.Lenc_loop6_enter
.align	16
.Lenc_loop6:
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
.Lenc_loop6_enter:
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Lenc_loop6

	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenclast	%xmm0,%xmm2
	aesenclast	%xmm0,%xmm3
	aesenclast	%xmm0,%xmm4
	aesenclast	%xmm0,%xmm5
	aesenclast	%xmm0,%xmm6
	aesenclast	%xmm0,%xmm7
	ret
.cfi_endproc	
.size	_aesni_encrypt6,.-_aesni_encrypt6
.type	_aesni_decrypt6,@function
.align	16
_aesni_decrypt6:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	pxor	%xmm0,%xmm3
	pxor	%xmm0,%xmm4
	aesdec	%xmm1,%xmm2
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	aesdec	%xmm1,%xmm3
	pxor	%xmm0,%xmm5
	pxor	%xmm0,%xmm6
	aesdec	%xmm1,%xmm4
	pxor	%xmm0,%xmm7
	movups	(%rcx,%rax,1),%xmm0
	addq	$16,%rax
	jmp	.Ldec_loop6_enter
.align	16
.Ldec_loop6:
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
.Ldec_loop6_enter:
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Ldec_loop6

	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdeclast	%xmm0,%xmm2
	aesdeclast	%xmm0,%xmm3
	aesdeclast	%xmm0,%xmm4
	aesdeclast	%xmm0,%xmm5
	aesdeclast	%xmm0,%xmm6
	aesdeclast	%xmm0,%xmm7
	ret
.cfi_endproc	
.size	_aesni_decrypt6,.-_aesni_decrypt6
.type	_aesni_encrypt8,@function
.align	16
_aesni_encrypt8:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	pxor	%xmm0,%xmm4
	pxor	%xmm0,%xmm5
	pxor	%xmm0,%xmm6
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	aesenc	%xmm1,%xmm2
	pxor	%xmm0,%xmm7
	pxor	%xmm0,%xmm8
	aesenc	%xmm1,%xmm3
	pxor	%xmm0,%xmm9
	movups	(%rcx,%rax,1),%xmm0
	addq	$16,%rax
	jmp	.Lenc_loop8_inner
.align	16
.Lenc_loop8:
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
.Lenc_loop8_inner:
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
.Lenc_loop8_enter:
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	aesenc	%xmm0,%xmm8
	aesenc	%xmm0,%xmm9
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Lenc_loop8

	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	aesenclast	%xmm0,%xmm2
	aesenclast	%xmm0,%xmm3
	aesenclast	%xmm0,%xmm4
	aesenclast	%xmm0,%xmm5
	aesenclast	%xmm0,%xmm6
	aesenclast	%xmm0,%xmm7
	aesenclast	%xmm0,%xmm8
	aesenclast	%xmm0,%xmm9
	ret
.cfi_endproc	
.size	_aesni_encrypt8,.-_aesni_encrypt8
.type	_aesni_decrypt8,@function
.align	16
_aesni_decrypt8:
.cfi_startproc	
	movups	(%rcx),%xmm0
	shll	$4,%eax
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm2
	xorps	%xmm0,%xmm3
	pxor	%xmm0,%xmm4
	pxor	%xmm0,%xmm5
	pxor	%xmm0,%xmm6
	leaq	32(%rcx,%rax,1),%rcx
	negq	%rax  // %eax
	aesdec	%xmm1,%xmm2
	pxor	%xmm0,%xmm7
	pxor	%xmm0,%xmm8
	aesdec	%xmm1,%xmm3
	pxor	%xmm0,%xmm9
	movups	(%rcx,%rax,1),%xmm0
	addq	$16,%rax
	jmp	.Ldec_loop8_inner
.align	16
.Ldec_loop8:
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
.Ldec_loop8_inner:
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
.Ldec_loop8_enter:
	movups	(%rcx,%rax,1),%xmm1
	addq	$32,%rax
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	aesdec	%xmm0,%xmm8
	aesdec	%xmm0,%xmm9
	movups	-16(%rcx,%rax,1),%xmm0
	jnz	.Ldec_loop8

	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
	aesdeclast	%xmm0,%xmm2
	aesdeclast	%xmm0,%xmm3
	aesdeclast	%xmm0,%xmm4
	aesdeclast	%xmm0,%xmm5
	aesdeclast	%xmm0,%xmm6
	aesdeclast	%xmm0,%xmm7
	aesdeclast	%xmm0,%xmm8
	aesdeclast	%xmm0,%xmm9
	ret
.cfi_endproc	
.size	_aesni_decrypt8,.-_aesni_decrypt8
.globl	aes_hw_ecb_encrypt
.hidden aes_hw_ecb_encrypt
.type	aes_hw_ecb_encrypt,@function
.align	16
aes_hw_ecb_encrypt:
.cfi_startproc	
_CET_ENDBR
	andq	$-16,%rdx  // if (%rdx<16)
	jz	.Lecb_ret  // return

	movl	240(%rcx),%eax  // key->rounds
	movups	(%rcx),%xmm0
	movq	%rcx,%r11  // backup %rcx
	movl	%eax,%r10d  // backup %eax
	testl	%r8d,%r8d  // 5th argument
	jz	.Lecb_decrypt
//  --------------------------- ECB ENCRYPT ------------------------------
	cmpq	$0x80,%rdx  // if (%rdx<8*16)
	jb	.Lecb_enc_tail  // short input

	movdqu	(%rdi),%xmm2  // load 8 input blocks
	movdqu	16(%rdi),%xmm3
	movdqu	32(%rdi),%xmm4
	movdqu	48(%rdi),%xmm5
	movdqu	64(%rdi),%xmm6
	movdqu	80(%rdi),%xmm7
	movdqu	96(%rdi),%xmm8
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi  // %rdi+=8*16
	subq	$0x80,%rdx  // %rdx-=8*16 (can be zero)
	jmp	.Lecb_enc_loop8_enter
.align	16
.Lecb_enc_loop8:
	movups	%xmm2,(%rsi)  // store 8 output blocks
	movq	%r11,%rcx  // restore %rcx
	movdqu	(%rdi),%xmm2  // load 8 input blocks
	movl	%r10d,%eax  // restore %eax
	movups	%xmm3,16(%rsi)
	movdqu	16(%rdi),%xmm3
	movups	%xmm4,32(%rsi)
	movdqu	32(%rdi),%xmm4
	movups	%xmm5,48(%rsi)
	movdqu	48(%rdi),%xmm5
	movups	%xmm6,64(%rsi)
	movdqu	64(%rdi),%xmm6
	movups	%xmm7,80(%rsi)
	movdqu	80(%rdi),%xmm7
	movups	%xmm8,96(%rsi)
	movdqu	96(%rdi),%xmm8
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi  // %rsi+=8*16
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi  // %rdi+=8*16
.Lecb_enc_loop8_enter:

	call	_aesni_encrypt8

	subq	$0x80,%rdx
	jnc	.Lecb_enc_loop8  // loop if %rdx-=8*16 didn't borrow

	movups	%xmm2,(%rsi)  // store 8 output blocks
	movq	%r11,%rcx  // restore %rcx
	movups	%xmm3,16(%rsi)
	movl	%r10d,%eax  // restore %eax
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	movups	%xmm8,96(%rsi)
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi  // %rsi+=8*16
	addq	$0x80,%rdx  // restore real remaining %rdx
	jz	.Lecb_ret  // done if (%rdx==0)

.Lecb_enc_tail:  // %rdx is less than 8*16
	movups	(%rdi),%xmm2
	cmpq	$0x20,%rdx
	jb	.Lecb_enc_one
	movups	16(%rdi),%xmm3
	je	.Lecb_enc_two
	movups	32(%rdi),%xmm4
	cmpq	$0x40,%rdx
	jb	.Lecb_enc_three
	movups	48(%rdi),%xmm5
	je	.Lecb_enc_four
	movups	64(%rdi),%xmm6
	cmpq	$0x60,%rdx
	jb	.Lecb_enc_five
	movups	80(%rdi),%xmm7
	je	.Lecb_enc_six
	movdqu	96(%rdi),%xmm8
	xorps	%xmm9,%xmm9
	call	_aesni_encrypt8
	movups	%xmm2,(%rsi)  // store 7 output blocks
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	movups	%xmm8,96(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_one:
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_enc1_3:
	aesenc	%xmm1,%xmm2
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_3  // loop body is 16 bytes
	aesenclast	%xmm1,%xmm2
	movups	%xmm2,(%rsi)  // store one output block
	jmp	.Lecb_ret
.align	16
.Lecb_enc_two:
	call	_aesni_encrypt2
	movups	%xmm2,(%rsi)  // store 2 output blocks
	movups	%xmm3,16(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_three:
	call	_aesni_encrypt3
	movups	%xmm2,(%rsi)  // store 3 output blocks
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_four:
	call	_aesni_encrypt4
	movups	%xmm2,(%rsi)  // store 4 output blocks
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_five:
	xorps	%xmm7,%xmm7
	call	_aesni_encrypt6
	movups	%xmm2,(%rsi)  // store 5 output blocks
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	jmp	.Lecb_ret
.align	16
.Lecb_enc_six:
	call	_aesni_encrypt6
	movups	%xmm2,(%rsi)  // store 6 output blocks
	movups	%xmm3,16(%rsi)
	movups	%xmm4,32(%rsi)
	movups	%xmm5,48(%rsi)
	movups	%xmm6,64(%rsi)
	movups	%xmm7,80(%rsi)
	jmp	.Lecb_ret
// --------------------------- ECB DECRYPT ------------------------------#
.align	16
.Lecb_decrypt:
	cmpq	$0x80,%rdx  // if (%rdx<8*16)
	jb	.Lecb_dec_tail  // short input

	movdqu	(%rdi),%xmm2  // load 8 input blocks
	movdqu	16(%rdi),%xmm3
	movdqu	32(%rdi),%xmm4
	movdqu	48(%rdi),%xmm5
	movdqu	64(%rdi),%xmm6
	movdqu	80(%rdi),%xmm7
	movdqu	96(%rdi),%xmm8
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi  // %rdi+=8*16
	subq	$0x80,%rdx  // %rdx-=8*16 (can be zero)
	jmp	.Lecb_dec_loop8_enter
.align	16
.Lecb_dec_loop8:
	movups	%xmm2,(%rsi)  // store 8 output blocks
	movq	%r11,%rcx  // restore %rcx
	movdqu	(%rdi),%xmm2  // load 8 input blocks
	movl	%r10d,%eax  // restore %eax
	movups	%xmm3,16(%rsi)
	movdqu	16(%rdi),%xmm3
	movups	%xmm4,32(%rsi)
	movdqu	32(%rdi),%xmm4
	movups	%xmm5,48(%rsi)
	movdqu	48(%rdi),%xmm5
	movups	%xmm6,64(%rsi)
	movdqu	64(%rdi),%xmm6
	movups	%xmm7,80(%rsi)
	movdqu	80(%rdi),%xmm7
	movups	%xmm8,96(%rsi)
	movdqu	96(%rdi),%xmm8
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi  // %rsi+=8*16
	movdqu	112(%rdi),%xmm9
	leaq	128(%rdi),%rdi  // %rdi+=8*16
.Lecb_dec_loop8_enter:

	call	_aesni_decrypt8

	movups	(%r11),%xmm0
	subq	$0x80,%rdx
	jnc	.Lecb_dec_loop8  // loop if %rdx-=8*16 didn't borrow

	movups	%xmm2,(%rsi)  // store 8 output blocks
	pxor	%xmm2,%xmm2  // clear register bank
	movq	%r11,%rcx  // restore %rcx
	movups	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3
	movl	%r10d,%eax  // restore %eax
	movups	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	movups	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	movups	%xmm6,64(%rsi)
	pxor	%xmm6,%xmm6
	movups	%xmm7,80(%rsi)
	pxor	%xmm7,%xmm7
	movups	%xmm8,96(%rsi)
	pxor	%xmm8,%xmm8
	movups	%xmm9,112(%rsi)
	pxor	%xmm9,%xmm9
	leaq	128(%rsi),%rsi  // %rsi+=8*16
	addq	$0x80,%rdx  // restore real remaining %rdx
	jz	.Lecb_ret  // done if (%rdx==0)

.Lecb_dec_tail:
	movups	(%rdi),%xmm2
	cmpq	$0x20,%rdx
	jb	.Lecb_dec_one
	movups	16(%rdi),%xmm3
	je	.Lecb_dec_two
	movups	32(%rdi),%xmm4
	cmpq	$0x40,%rdx
	jb	.Lecb_dec_three
	movups	48(%rdi),%xmm5
	je	.Lecb_dec_four
	movups	64(%rdi),%xmm6
	cmpq	$0x60,%rdx
	jb	.Lecb_dec_five
	movups	80(%rdi),%xmm7
	je	.Lecb_dec_six
	movups	96(%rdi),%xmm8
	movups	(%rcx),%xmm0
	xorps	%xmm9,%xmm9
	call	_aesni_decrypt8
	movups	%xmm2,(%rsi)  // store 7 output blocks
	pxor	%xmm2,%xmm2  // clear register bank
	movups	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3
	movups	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	movups	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	movups	%xmm6,64(%rsi)
	pxor	%xmm6,%xmm6
	movups	%xmm7,80(%rsi)
	pxor	%xmm7,%xmm7
	movups	%xmm8,96(%rsi)
	pxor	%xmm8,%xmm8
	pxor	%xmm9,%xmm9
	jmp	.Lecb_ret
.align	16
.Lecb_dec_one:
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_4:
	aesdec	%xmm1,%xmm2
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_4  // loop body is 16 bytes
	aesdeclast	%xmm1,%xmm2
	movups	%xmm2,(%rsi)  // store one output block
	pxor	%xmm2,%xmm2  // clear register bank
	jmp	.Lecb_ret
.align	16
.Lecb_dec_two:
	call	_aesni_decrypt2
	movups	%xmm2,(%rsi)  // store 2 output blocks
	pxor	%xmm2,%xmm2  // clear register bank
	movups	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3
	jmp	.Lecb_ret
.align	16
.Lecb_dec_three:
	call	_aesni_decrypt3
	movups	%xmm2,(%rsi)  // store 3 output blocks
	pxor	%xmm2,%xmm2  // clear register bank
	movups	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3
	movups	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	jmp	.Lecb_ret
.align	16
.Lecb_dec_four:
	call	_aesni_decrypt4
	movups	%xmm2,(%rsi)  // store 4 output blocks
	pxor	%xmm2,%xmm2  // clear register bank
	movups	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3
	movups	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	movups	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	jmp	.Lecb_ret
.align	16
.Lecb_dec_five:
	xorps	%xmm7,%xmm7
	call	_aesni_decrypt6
	movups	%xmm2,(%rsi)  // store 5 output blocks
	pxor	%xmm2,%xmm2  // clear register bank
	movups	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3
	movups	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	movups	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	movups	%xmm6,64(%rsi)
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7
	jmp	.Lecb_ret
.align	16
.Lecb_dec_six:
	call	_aesni_decrypt6
	movups	%xmm2,(%rsi)  // store 6 output blocks
	pxor	%xmm2,%xmm2  // clear register bank
	movups	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3
	movups	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	movups	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	movups	%xmm6,64(%rsi)
	pxor	%xmm6,%xmm6
	movups	%xmm7,80(%rsi)
	pxor	%xmm7,%xmm7

.Lecb_ret:
	xorps	%xmm0,%xmm0  // %xmm0
	pxor	%xmm1,%xmm1
	ret
.cfi_endproc	
.size	aes_hw_ecb_encrypt,.-aes_hw_ecb_encrypt
.globl	aes_hw_ctr32_encrypt_blocks
.hidden aes_hw_ctr32_encrypt_blocks
.type	aes_hw_ctr32_encrypt_blocks,@function
.align	16
aes_hw_ctr32_encrypt_blocks:
.cfi_startproc	
_CET_ENDBR
#ifdef BORINGSSL_DISPATCH_TEST
	movb	$1,BORINGSSL_function_hit(%rip)
#endif
	cmpq	$1,%rdx
	jne	.Lctr32_bulk

// handle single block without allocating stack frame,
// useful when handling edges
	movups	(%r8),%xmm2
	movups	(%rdi),%xmm3
	movl	240(%rcx),%edx  // key->rounds
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_enc1_5:
	aesenc	%xmm1,%xmm2
	decl	%edx
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_5  // loop body is 16 bytes
	aesenclast	%xmm1,%xmm2
	pxor	%xmm0,%xmm0  // clear register bank
	pxor	%xmm1,%xmm1
	xorps	%xmm3,%xmm2
	pxor	%xmm3,%xmm3
	movups	%xmm2,(%rsi)
	xorps	%xmm2,%xmm2
	jmp	.Lctr32_epilogue

.align	16
.Lctr32_bulk:
	leaq	(%rsp),%r11  // use %r11 as frame pointer
.cfi_def_cfa_register	%r11
	pushq	%rbp
.cfi_offset	%rbp,-16
	subq	$128,%rsp
	andq	$-16,%rsp  // Linux kernel stack can be incorrectly seeded

// 8 16-byte words on top of stack are counter values
// xor-ed with zero-round key

	movdqu	(%r8),%xmm2
	movdqu	(%rcx),%xmm0
	movl	12(%r8),%r8d  // counter LSB
	pxor	%xmm0,%xmm2
	movl	12(%rcx),%ebp  // 0-round key LSB
	movdqa	%xmm2,0(%rsp)  // populate counter block
	bswapl	%r8d
	movdqa	%xmm2,%xmm3
	movdqa	%xmm2,%xmm4
	movdqa	%xmm2,%xmm5
	movdqa	%xmm2,64(%rsp)
	movdqa	%xmm2,80(%rsp)
	movdqa	%xmm2,96(%rsp)
	movq	%rdx,%r10  // about to borrow %rdx
	movdqa	%xmm2,112(%rsp)

	leaq	1(%r8),%rax
	leaq	2(%r8),%rdx
	bswapl	%eax
	bswapl	%edx
	xorl	%ebp,%eax
	xorl	%ebp,%edx
	pinsrd	$3,%eax,%xmm3
	leaq	3(%r8),%rax
	movdqa	%xmm3,16(%rsp)
	pinsrd	$3,%edx,%xmm4
	bswapl	%eax
	movq	%r10,%rdx  // restore %rdx
	leaq	4(%r8),%r10
	movdqa	%xmm4,32(%rsp)
	xorl	%ebp,%eax
	bswapl	%r10d
	pinsrd	$3,%eax,%xmm5
	xorl	%ebp,%r10d
	movdqa	%xmm5,48(%rsp)
	leaq	5(%r8),%r9
	movl	%r10d,64+12(%rsp)
	bswapl	%r9d
	leaq	6(%r8),%r10
	movl	240(%rcx),%eax  // key->rounds
	xorl	%ebp,%r9d
	bswapl	%r10d
	movl	%r9d,80+12(%rsp)
	xorl	%ebp,%r10d
	leaq	7(%r8),%r9
	movl	%r10d,96+12(%rsp)
	bswapl	%r9d
	xorl	%ebp,%r9d
	movl	%r9d,112+12(%rsp)

	movups	16(%rcx),%xmm1

	movdqa	64(%rsp),%xmm6
	movdqa	80(%rsp),%xmm7

	cmpq	$8,%rdx  // %rdx is in blocks
	jb	.Lctr32_tail  // short input if (%rdx<8)

	leaq	128(%rcx),%rcx  // size optimization
	subq	$8,%rdx  // %rdx is biased by -8
	jmp	.Lctr32_loop8

.align	32
.Lctr32_loop8:
	addl	$8,%r8d  // next counter value
	movdqa	96(%rsp),%xmm8
	aesenc	%xmm1,%xmm2
	movl	%r8d,%r9d
	movdqa	112(%rsp),%xmm9
	aesenc	%xmm1,%xmm3
	bswapl	%r9d
	movups	32-128(%rcx),%xmm0
	aesenc	%xmm1,%xmm4
	xorl	%ebp,%r9d
	nop
	aesenc	%xmm1,%xmm5
	movl	%r9d,0+12(%rsp)  // store next counter value
	leaq	1(%r8),%r9
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	movups	48-128(%rcx),%xmm1
	bswapl	%r9d
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	xorl	%ebp,%r9d
.byte	0x66,0x90
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	movl	%r9d,16+12(%rsp)
	leaq	2(%r8),%r9
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	aesenc	%xmm0,%xmm8
	aesenc	%xmm0,%xmm9
	movups	64-128(%rcx),%xmm0
	bswapl	%r9d
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	xorl	%ebp,%r9d
.byte	0x66,0x90
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	movl	%r9d,32+12(%rsp)
	leaq	3(%r8),%r9
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	movups	80-128(%rcx),%xmm1
	bswapl	%r9d
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	xorl	%ebp,%r9d
.byte	0x66,0x90
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	movl	%r9d,48+12(%rsp)
	leaq	4(%r8),%r9
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	aesenc	%xmm0,%xmm8
	aesenc	%xmm0,%xmm9
	movups	96-128(%rcx),%xmm0
	bswapl	%r9d
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	xorl	%ebp,%r9d
.byte	0x66,0x90
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	movl	%r9d,64+12(%rsp)
	leaq	5(%r8),%r9
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	movups	112-128(%rcx),%xmm1
	bswapl	%r9d
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	xorl	%ebp,%r9d
.byte	0x66,0x90
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	movl	%r9d,80+12(%rsp)
	leaq	6(%r8),%r9
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	aesenc	%xmm0,%xmm8
	aesenc	%xmm0,%xmm9
	movups	128-128(%rcx),%xmm0
	bswapl	%r9d
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	xorl	%ebp,%r9d
.byte	0x66,0x90
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	movl	%r9d,96+12(%rsp)
	leaq	7(%r8),%r9
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	movups	144-128(%rcx),%xmm1
	bswapl	%r9d
	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	aesenc	%xmm0,%xmm4
	xorl	%ebp,%r9d
	movdqu	0(%rdi),%xmm10  // start loading input
	aesenc	%xmm0,%xmm5
	movl	%r9d,112+12(%rsp)
	cmpl	$11,%eax
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	aesenc	%xmm0,%xmm8
	aesenc	%xmm0,%xmm9
	movups	160-128(%rcx),%xmm0

	jb	.Lctr32_enc_done

	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	movups	176-128(%rcx),%xmm1

	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	aesenc	%xmm0,%xmm8
	aesenc	%xmm0,%xmm9
	movups	192-128(%rcx),%xmm0
	je	.Lctr32_enc_done

	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	movups	208-128(%rcx),%xmm1

	aesenc	%xmm0,%xmm2
	aesenc	%xmm0,%xmm3
	aesenc	%xmm0,%xmm4
	aesenc	%xmm0,%xmm5
	aesenc	%xmm0,%xmm6
	aesenc	%xmm0,%xmm7
	aesenc	%xmm0,%xmm8
	aesenc	%xmm0,%xmm9
	movups	224-128(%rcx),%xmm0
	jmp	.Lctr32_enc_done

.align	16
.Lctr32_enc_done:
	movdqu	16(%rdi),%xmm11
	pxor	%xmm0,%xmm10  // input^=round[last]
	movdqu	32(%rdi),%xmm12
	pxor	%xmm0,%xmm11
	movdqu	48(%rdi),%xmm13
	pxor	%xmm0,%xmm12
	movdqu	64(%rdi),%xmm14
	pxor	%xmm0,%xmm13
	movdqu	80(%rdi),%xmm15
	pxor	%xmm0,%xmm14
	prefetcht0	448(%rdi)  // We process 128 bytes (8*16), so to prefetch 1 iteration
	prefetcht0	512(%rdi)  // We need to prefetch 2 64 byte lines
	pxor	%xmm0,%xmm15
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8
	aesenc	%xmm1,%xmm9
	movdqu	96(%rdi),%xmm1  // borrow %xmm1 for inp[6]
	leaq	128(%rdi),%rdi  // %rdi+=8*16

	aesenclast	%xmm10,%xmm2  // is inp[N]^round[last]
	pxor	%xmm0,%xmm1  // borrowed
	movdqu	112-128(%rdi),%xmm10
	aesenclast	%xmm11,%xmm3
	pxor	%xmm0,%xmm10
	movdqa	0(%rsp),%xmm11  // load next counter block
	aesenclast	%xmm12,%xmm4
	aesenclast	%xmm13,%xmm5
	movdqa	16(%rsp),%xmm12
	movdqa	32(%rsp),%xmm13
	aesenclast	%xmm14,%xmm6
	aesenclast	%xmm15,%xmm7
	movdqa	48(%rsp),%xmm14
	movdqa	64(%rsp),%xmm15
	aesenclast	%xmm1,%xmm8
	movdqa	80(%rsp),%xmm0
	movups	16-128(%rcx),%xmm1  // real 1st-round key
	aesenclast	%xmm10,%xmm9

	movups	%xmm2,(%rsi)  // store 8 output blocks
	movdqa	%xmm11,%xmm2
	movups	%xmm3,16(%rsi)
	movdqa	%xmm12,%xmm3
	movups	%xmm4,32(%rsi)
	movdqa	%xmm13,%xmm4
	movups	%xmm5,48(%rsi)
	movdqa	%xmm14,%xmm5
	movups	%xmm6,64(%rsi)
	movdqa	%xmm15,%xmm6
	movups	%xmm7,80(%rsi)
	movdqa	%xmm0,%xmm7
	movups	%xmm8,96(%rsi)
	movups	%xmm9,112(%rsi)
	leaq	128(%rsi),%rsi  // %rsi+=8*16

	subq	$8,%rdx
	jnc	.Lctr32_loop8  // loop if %rdx-=8 didn't borrow

	addq	$8,%rdx  // restore real remaining %rdx
	jz	.Lctr32_done  // done if (%rdx==0)
	leaq	-128(%rcx),%rcx

.Lctr32_tail:
// note that at this point %xmm2..5 are populated with
// counter values xor-ed with 0-round key
	leaq	16(%rcx),%rcx
	cmpq	$4,%rdx
	jb	.Lctr32_loop3
	je	.Lctr32_loop4

// if (%rdx>4) compute 7 E(counter)
	shll	$4,%eax
	movdqa	96(%rsp),%xmm8
	pxor	%xmm9,%xmm9

	movups	16(%rcx),%xmm0
	aesenc	%xmm1,%xmm2
	aesenc	%xmm1,%xmm3
	leaq	32-16(%rcx,%rax,1),%rcx  // prepare for .Lenc_loop8_enter
	negq	%rax
	aesenc	%xmm1,%xmm4
	addq	$16,%rax  // prepare for .Lenc_loop8_enter
	movups	(%rdi),%xmm10
	aesenc	%xmm1,%xmm5
	aesenc	%xmm1,%xmm6
	movups	16(%rdi),%xmm11  // pre-load input
	movups	32(%rdi),%xmm12
	aesenc	%xmm1,%xmm7
	aesenc	%xmm1,%xmm8

	call	.Lenc_loop8_enter

	movdqu	48(%rdi),%xmm13
	pxor	%xmm10,%xmm2
	movdqu	64(%rdi),%xmm10
	pxor	%xmm11,%xmm3
	movdqu	%xmm2,(%rsi)  // store output
	pxor	%xmm12,%xmm4
	movdqu	%xmm3,16(%rsi)
	pxor	%xmm13,%xmm5
	movdqu	%xmm4,32(%rsi)
	pxor	%xmm10,%xmm6
	movdqu	%xmm5,48(%rsi)
	movdqu	%xmm6,64(%rsi)
	cmpq	$6,%rdx
	jb	.Lctr32_done  // %rdx was 5, stop store

	movups	80(%rdi),%xmm11
	xorps	%xmm11,%xmm7
	movups	%xmm7,80(%rsi)
	je	.Lctr32_done  // %rdx was 6, stop store

	movups	96(%rdi),%xmm12
	xorps	%xmm12,%xmm8
	movups	%xmm8,96(%rsi)
	jmp	.Lctr32_done  // %rdx was 7, stop store

.align	32
.Lctr32_loop4:
	aesenc	%xmm1,%xmm2
	leaq	16(%rcx),%rcx
	decl	%eax
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	aesenc	%xmm1,%xmm5
	movups	(%rcx),%xmm1
	jnz	.Lctr32_loop4
	aesenclast	%xmm1,%xmm2
	aesenclast	%xmm1,%xmm3
	movups	(%rdi),%xmm10  // load input
	movups	16(%rdi),%xmm11
	aesenclast	%xmm1,%xmm4
	aesenclast	%xmm1,%xmm5
	movups	32(%rdi),%xmm12
	movups	48(%rdi),%xmm13

	xorps	%xmm10,%xmm2
	movups	%xmm2,(%rsi)  // store output
	xorps	%xmm11,%xmm3
	movups	%xmm3,16(%rsi)
	pxor	%xmm12,%xmm4
	movdqu	%xmm4,32(%rsi)
	pxor	%xmm13,%xmm5
	movdqu	%xmm5,48(%rsi)
	jmp	.Lctr32_done  // %rdx was 4, stop store

.align	32
.Lctr32_loop3:
	aesenc	%xmm1,%xmm2
	leaq	16(%rcx),%rcx
	decl	%eax
	aesenc	%xmm1,%xmm3
	aesenc	%xmm1,%xmm4
	movups	(%rcx),%xmm1
	jnz	.Lctr32_loop3
	aesenclast	%xmm1,%xmm2
	aesenclast	%xmm1,%xmm3
	aesenclast	%xmm1,%xmm4

	movups	(%rdi),%xmm10  // load input
	xorps	%xmm10,%xmm2
	movups	%xmm2,(%rsi)  // store output
	cmpq	$2,%rdx
	jb	.Lctr32_done  // %rdx was 1, stop store

	movups	16(%rdi),%xmm11
	xorps	%xmm11,%xmm3
	movups	%xmm3,16(%rsi)
	je	.Lctr32_done  // %rdx was 2, stop store

	movups	32(%rdi),%xmm12
	xorps	%xmm12,%xmm4
	movups	%xmm4,32(%rsi)  // %rdx was 3, stop store

.Lctr32_done:
	xorps	%xmm0,%xmm0  // clear register bank
	xorl	%ebp,%ebp
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7
	movaps	%xmm0,0(%rsp)  // clear stack
	pxor	%xmm8,%xmm8
	movaps	%xmm0,16(%rsp)
	pxor	%xmm9,%xmm9
	movaps	%xmm0,32(%rsp)
	pxor	%xmm10,%xmm10
	movaps	%xmm0,48(%rsp)
	pxor	%xmm11,%xmm11
	movaps	%xmm0,64(%rsp)
	pxor	%xmm12,%xmm12
	movaps	%xmm0,80(%rsp)
	pxor	%xmm13,%xmm13
	movaps	%xmm0,96(%rsp)
	pxor	%xmm14,%xmm14
	movaps	%xmm0,112(%rsp)
	pxor	%xmm15,%xmm15
	movq	-8(%r11),%rbp
.cfi_restore	%rbp
	leaq	(%r11),%rsp
.cfi_def_cfa_register	%rsp
.Lctr32_epilogue:
	ret
.cfi_endproc	
.size	aes_hw_ctr32_encrypt_blocks,.-aes_hw_ctr32_encrypt_blocks
.globl	aes_hw_cbc_encrypt
.hidden aes_hw_cbc_encrypt
.type	aes_hw_cbc_encrypt,@function
.align	16
aes_hw_cbc_encrypt:
.cfi_startproc	
_CET_ENDBR
	testq	%rdx,%rdx  // check length
	jz	.Lcbc_ret

	movl	240(%rcx),%r10d  // key->rounds
	movq	%rcx,%r11  // backup %rcx
	testl	%r9d,%r9d  // 6th argument
	jz	.Lcbc_decrypt
//  --------------------------- CBC ENCRYPT ------------------------------
	movups	(%r8),%xmm2  // load iv as initial state
	movl	%r10d,%eax
	cmpq	$16,%rdx
	jb	.Lcbc_enc_tail
	subq	$16,%rdx
	jmp	.Lcbc_enc_loop
.align	16
.Lcbc_enc_loop:
	movups	(%rdi),%xmm3  // load input
	leaq	16(%rdi),%rdi
// xorps	%xmm3,%xmm2
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	xorps	%xmm0,%xmm3
	leaq	32(%rcx),%rcx
	xorps	%xmm3,%xmm2
.Loop_enc1_6:
	aesenc	%xmm1,%xmm2
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_enc1_6  // loop body is 16 bytes
	aesenclast	%xmm1,%xmm2
	movl	%r10d,%eax  // restore %eax
	movq	%r11,%rcx  // restore %rcx
	movups	%xmm2,0(%rsi)  // store output
	leaq	16(%rsi),%rsi
	subq	$16,%rdx
	jnc	.Lcbc_enc_loop
	addq	$16,%rdx
	jnz	.Lcbc_enc_tail
	pxor	%xmm0,%xmm0  // clear register bank
	pxor	%xmm1,%xmm1
	movups	%xmm2,(%r8)
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	jmp	.Lcbc_ret

.Lcbc_enc_tail:
	movq	%rdx,%rcx  // zaps %rcx
	xchgq	%rdi,%rsi  // %rdi is %rsi and %rsi is %rdi now
.long	0x9066A4F3  // rep movsb
	movl	$16,%ecx  // zero tail
	subq	%rdx,%rcx
	xorl	%eax,%eax
.long	0x9066AAF3  // rep stosb
	leaq	-16(%rdi),%rdi  // rewind %rsi by 1 block
	movl	%r10d,%eax  // restore %eax
	movq	%rdi,%rsi  // %rdi and %rsi are the same
	movq	%r11,%rcx  // restore %rcx
	xorq	%rdx,%rdx  // len=16
	jmp	.Lcbc_enc_loop  // one more spin
// --------------------------- CBC DECRYPT ------------------------------#
.align	16
.Lcbc_decrypt:
	cmpq	$16,%rdx
	jne	.Lcbc_decrypt_bulk

// handle single block without allocating stack frame,
// useful in ciphertext stealing mode
	movdqu	(%rdi),%xmm2  // load input
	movdqu	(%r8),%xmm3  // load iv
	movdqa	%xmm2,%xmm4  // future iv
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_7:
	aesdec	%xmm1,%xmm2
	decl	%r10d
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_7  // loop body is 16 bytes
	aesdeclast	%xmm1,%xmm2
	pxor	%xmm0,%xmm0  // clear register bank
	pxor	%xmm1,%xmm1
	movdqu	%xmm4,(%r8)  // store iv
	xorps	%xmm3,%xmm2  // ^=iv
	pxor	%xmm3,%xmm3
	movups	%xmm2,(%rsi)  // store output
	pxor	%xmm2,%xmm2
	jmp	.Lcbc_ret
.align	16
.Lcbc_decrypt_bulk:
	leaq	(%rsp),%r11  // frame pointer
.cfi_def_cfa_register	%r11
	pushq	%rbp
.cfi_offset	%rbp,-16
	subq	$16,%rsp
	andq	$-16,%rsp  // Linux kernel stack can be incorrectly seeded
	movq	%rcx,%rbp  // [re-]backup %rcx [after reassignment]
	movups	(%r8),%xmm10
	movl	%r10d,%eax
	cmpq	$0x50,%rdx
	jbe	.Lcbc_dec_tail

	movups	(%rcx),%xmm0
	movdqu	0(%rdi),%xmm2  // load input
	movdqu	16(%rdi),%xmm3
	movdqa	%xmm2,%xmm11
	movdqu	32(%rdi),%xmm4
	movdqa	%xmm3,%xmm12
	movdqu	48(%rdi),%xmm5
	movdqa	%xmm4,%xmm13
	movdqu	64(%rdi),%xmm6
	movdqa	%xmm5,%xmm14
	movdqu	80(%rdi),%xmm7
	movdqa	%xmm6,%xmm15
	cmpq	$0x70,%rdx
	jbe	.Lcbc_dec_six_or_seven

	subq	$0x70,%rdx  // %rdx is biased by -7*16
	leaq	112(%rcx),%rcx  // size optimization
	jmp	.Lcbc_dec_loop8_enter
.align	16
.Lcbc_dec_loop8:
	movups	%xmm9,(%rsi)
	leaq	16(%rsi),%rsi
.Lcbc_dec_loop8_enter:
	movdqu	96(%rdi),%xmm8
	pxor	%xmm0,%xmm2
	movdqu	112(%rdi),%xmm9
	pxor	%xmm0,%xmm3
	movups	16-112(%rcx),%xmm1
	pxor	%xmm0,%xmm4
	movq	$-1,%rbp
	cmpq	$0x70,%rdx  // is there at least 0x60 bytes ahead?
	pxor	%xmm0,%xmm5
	pxor	%xmm0,%xmm6
	pxor	%xmm0,%xmm7
	pxor	%xmm0,%xmm8

	aesdec	%xmm1,%xmm2
	pxor	%xmm0,%xmm9
	movups	32-112(%rcx),%xmm0
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	adcq	$0,%rbp
	andq	$128,%rbp
	aesdec	%xmm1,%xmm9
	addq	%rdi,%rbp
	movups	48-112(%rcx),%xmm1
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	aesdec	%xmm0,%xmm8
	aesdec	%xmm0,%xmm9
	movups	64-112(%rcx),%xmm0
	nop
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
	movups	80-112(%rcx),%xmm1
	nop
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	aesdec	%xmm0,%xmm8
	aesdec	%xmm0,%xmm9
	movups	96-112(%rcx),%xmm0
	nop
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
	movups	112-112(%rcx),%xmm1
	nop
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	aesdec	%xmm0,%xmm8
	aesdec	%xmm0,%xmm9
	movups	128-112(%rcx),%xmm0
	nop
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
	movups	144-112(%rcx),%xmm1
	cmpl	$11,%eax
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	aesdec	%xmm0,%xmm8
	aesdec	%xmm0,%xmm9
	movups	160-112(%rcx),%xmm0
	jb	.Lcbc_dec_done
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
	movups	176-112(%rcx),%xmm1
	nop
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	aesdec	%xmm0,%xmm8
	aesdec	%xmm0,%xmm9
	movups	192-112(%rcx),%xmm0
	je	.Lcbc_dec_done
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
	movups	208-112(%rcx),%xmm1
	nop
	aesdec	%xmm0,%xmm2
	aesdec	%xmm0,%xmm3
	aesdec	%xmm0,%xmm4
	aesdec	%xmm0,%xmm5
	aesdec	%xmm0,%xmm6
	aesdec	%xmm0,%xmm7
	aesdec	%xmm0,%xmm8
	aesdec	%xmm0,%xmm9
	movups	224-112(%rcx),%xmm0
	jmp	.Lcbc_dec_done
.align	16
.Lcbc_dec_done:
	aesdec	%xmm1,%xmm2
	aesdec	%xmm1,%xmm3
	pxor	%xmm0,%xmm10
	pxor	%xmm0,%xmm11
	aesdec	%xmm1,%xmm4
	aesdec	%xmm1,%xmm5
	pxor	%xmm0,%xmm12
	pxor	%xmm0,%xmm13
	aesdec	%xmm1,%xmm6
	aesdec	%xmm1,%xmm7
	pxor	%xmm0,%xmm14
	pxor	%xmm0,%xmm15
	aesdec	%xmm1,%xmm8
	aesdec	%xmm1,%xmm9
	movdqu	80(%rdi),%xmm1

	aesdeclast	%xmm10,%xmm2
	movdqu	96(%rdi),%xmm10  // borrow %xmm10
	pxor	%xmm0,%xmm1
	aesdeclast	%xmm11,%xmm3
	pxor	%xmm0,%xmm10
	movdqu	112(%rdi),%xmm0  // next IV
	aesdeclast	%xmm12,%xmm4
	leaq	128(%rdi),%rdi
	movdqu	0(%rbp),%xmm11
	aesdeclast	%xmm13,%xmm5
	aesdeclast	%xmm14,%xmm6
	movdqu	16(%rbp),%xmm12
	movdqu	32(%rbp),%xmm13
	aesdeclast	%xmm15,%xmm7
	aesdeclast	%xmm1,%xmm8
	movdqu	48(%rbp),%xmm14
	movdqu	64(%rbp),%xmm15
	aesdeclast	%xmm10,%xmm9
	movdqa	%xmm0,%xmm10  // return %xmm10
	movdqu	80(%rbp),%xmm1
	movups	-112(%rcx),%xmm0

	movups	%xmm2,(%rsi)  // store output
	movdqa	%xmm11,%xmm2
	movups	%xmm3,16(%rsi)
	movdqa	%xmm12,%xmm3
	movups	%xmm4,32(%rsi)
	movdqa	%xmm13,%xmm4
	movups	%xmm5,48(%rsi)
	movdqa	%xmm14,%xmm5
	movups	%xmm6,64(%rsi)
	movdqa	%xmm15,%xmm6
	movups	%xmm7,80(%rsi)
	movdqa	%xmm1,%xmm7
	movups	%xmm8,96(%rsi)
	leaq	112(%rsi),%rsi

	subq	$0x80,%rdx
	ja	.Lcbc_dec_loop8

	movaps	%xmm9,%xmm2
	leaq	-112(%rcx),%rcx
	addq	$0x70,%rdx
	jle	.Lcbc_dec_clear_tail_collected
	movups	%xmm9,(%rsi)
	leaq	16(%rsi),%rsi
	cmpq	$0x50,%rdx
	jbe	.Lcbc_dec_tail

	movaps	%xmm11,%xmm2
.Lcbc_dec_six_or_seven:
	cmpq	$0x60,%rdx
	ja	.Lcbc_dec_seven

	movaps	%xmm7,%xmm8
	call	_aesni_decrypt6
	pxor	%xmm10,%xmm2  // ^= IV
	movaps	%xmm8,%xmm10
	pxor	%xmm11,%xmm3
	movdqu	%xmm2,(%rsi)
	pxor	%xmm12,%xmm4
	movdqu	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3  // clear register bank
	pxor	%xmm13,%xmm5
	movdqu	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	pxor	%xmm14,%xmm6
	movdqu	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	pxor	%xmm15,%xmm7
	movdqu	%xmm6,64(%rsi)
	pxor	%xmm6,%xmm6
	leaq	80(%rsi),%rsi
	movdqa	%xmm7,%xmm2
	pxor	%xmm7,%xmm7
	jmp	.Lcbc_dec_tail_collected

.align	16
.Lcbc_dec_seven:
	movups	96(%rdi),%xmm8
	xorps	%xmm9,%xmm9
	call	_aesni_decrypt8
	movups	80(%rdi),%xmm9
	pxor	%xmm10,%xmm2  // ^= IV
	movups	96(%rdi),%xmm10
	pxor	%xmm11,%xmm3
	movdqu	%xmm2,(%rsi)
	pxor	%xmm12,%xmm4
	movdqu	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3  // clear register bank
	pxor	%xmm13,%xmm5
	movdqu	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	pxor	%xmm14,%xmm6
	movdqu	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	pxor	%xmm15,%xmm7
	movdqu	%xmm6,64(%rsi)
	pxor	%xmm6,%xmm6
	pxor	%xmm9,%xmm8
	movdqu	%xmm7,80(%rsi)
	pxor	%xmm7,%xmm7
	leaq	96(%rsi),%rsi
	movdqa	%xmm8,%xmm2
	pxor	%xmm8,%xmm8
	pxor	%xmm9,%xmm9
	jmp	.Lcbc_dec_tail_collected

.Lcbc_dec_tail:
	movups	(%rdi),%xmm2
	subq	$0x10,%rdx
	jbe	.Lcbc_dec_one  // %rdx is 1*16 or less

	movups	16(%rdi),%xmm3
	movaps	%xmm2,%xmm11
	subq	$0x10,%rdx
	jbe	.Lcbc_dec_two  // %rdx is 2*16 or less

	movups	32(%rdi),%xmm4
	movaps	%xmm3,%xmm12
	subq	$0x10,%rdx
	jbe	.Lcbc_dec_three  // %rdx is 3*16 or less

	movups	48(%rdi),%xmm5
	movaps	%xmm4,%xmm13
	subq	$0x10,%rdx
	jbe	.Lcbc_dec_four  // %rdx is 4*16 or less

	movups	64(%rdi),%xmm6  // %rdx is 5*16 or less
	movaps	%xmm5,%xmm14
	movaps	%xmm6,%xmm15
	xorps	%xmm7,%xmm7
	call	_aesni_decrypt6
	pxor	%xmm10,%xmm2
	movaps	%xmm15,%xmm10
	pxor	%xmm11,%xmm3
	movdqu	%xmm2,(%rsi)
	pxor	%xmm12,%xmm4
	movdqu	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3  // clear register bank
	pxor	%xmm13,%xmm5
	movdqu	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	pxor	%xmm14,%xmm6
	movdqu	%xmm5,48(%rsi)
	pxor	%xmm5,%xmm5
	leaq	64(%rsi),%rsi
	movdqa	%xmm6,%xmm2
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7
	subq	$0x10,%rdx
	jmp	.Lcbc_dec_tail_collected

.align	16
.Lcbc_dec_one:
	movaps	%xmm2,%xmm11
	movups	(%rcx),%xmm0
	movups	16(%rcx),%xmm1
	leaq	32(%rcx),%rcx
	xorps	%xmm0,%xmm2
.Loop_dec1_8:
	aesdec	%xmm1,%xmm2
	decl	%eax
	movups	(%rcx),%xmm1
	leaq	16(%rcx),%rcx
	jnz	.Loop_dec1_8  // loop body is 16 bytes
	aesdeclast	%xmm1,%xmm2
	xorps	%xmm10,%xmm2
	movaps	%xmm11,%xmm10
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_two:
	movaps	%xmm3,%xmm12
	call	_aesni_decrypt2
	pxor	%xmm10,%xmm2
	movaps	%xmm12,%xmm10
	pxor	%xmm11,%xmm3
	movdqu	%xmm2,(%rsi)
	movdqa	%xmm3,%xmm2
	pxor	%xmm3,%xmm3  // clear register bank
	leaq	16(%rsi),%rsi
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_three:
	movaps	%xmm4,%xmm13
	call	_aesni_decrypt3
	pxor	%xmm10,%xmm2
	movaps	%xmm13,%xmm10
	pxor	%xmm11,%xmm3
	movdqu	%xmm2,(%rsi)
	pxor	%xmm12,%xmm4
	movdqu	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3  // clear register bank
	movdqa	%xmm4,%xmm2
	pxor	%xmm4,%xmm4
	leaq	32(%rsi),%rsi
	jmp	.Lcbc_dec_tail_collected
.align	16
.Lcbc_dec_four:
	movaps	%xmm5,%xmm14
	call	_aesni_decrypt4
	pxor	%xmm10,%xmm2
	movaps	%xmm14,%xmm10
	pxor	%xmm11,%xmm3
	movdqu	%xmm2,(%rsi)
	pxor	%xmm12,%xmm4
	movdqu	%xmm3,16(%rsi)
	pxor	%xmm3,%xmm3  // clear register bank
	pxor	%xmm13,%xmm5
	movdqu	%xmm4,32(%rsi)
	pxor	%xmm4,%xmm4
	movdqa	%xmm5,%xmm2
	pxor	%xmm5,%xmm5
	leaq	48(%rsi),%rsi
	jmp	.Lcbc_dec_tail_collected

.align	16
.Lcbc_dec_clear_tail_collected:
	pxor	%xmm3,%xmm3  // clear register bank
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6  // %xmm6..9
	pxor	%xmm7,%xmm7
	pxor	%xmm8,%xmm8
	pxor	%xmm9,%xmm9
.Lcbc_dec_tail_collected:
	movups	%xmm10,(%r8)
	andq	$15,%rdx
	jnz	.Lcbc_dec_tail_partial
	movups	%xmm2,(%rsi)
	pxor	%xmm2,%xmm2
	jmp	.Lcbc_dec_ret
.align	16
.Lcbc_dec_tail_partial:
	movaps	%xmm2,(%rsp)
	pxor	%xmm2,%xmm2
	movq	$16,%rcx
	movq	%rsi,%rdi
	subq	%rdx,%rcx
	leaq	(%rsp),%rsi
.long	0x9066A4F3  // rep movsb
	movdqa	%xmm2,(%rsp)

.Lcbc_dec_ret:
	xorps	%xmm0,%xmm0  // %xmm0
	pxor	%xmm1,%xmm1
	movq	-8(%r11),%rbp
.cfi_restore	%rbp
	leaq	(%r11),%rsp
.cfi_def_cfa_register	%rsp
.Lcbc_ret:
	ret
.cfi_endproc	
.size	aes_hw_cbc_encrypt,.-aes_hw_cbc_encrypt
.globl	aes_hw_encrypt_key_to_decrypt_key
.hidden aes_hw_encrypt_key_to_decrypt_key
.type	aes_hw_encrypt_key_to_decrypt_key,@function
.align	16
aes_hw_encrypt_key_to_decrypt_key:
.cfi_startproc	
_CET_ENDBR

	movl	240(%rdi),%esi
	shll	$4,%esi

	leaq	16(%rdi,%rsi,1),%rdx  // points at the end of key schedule

	movups	(%rdi),%xmm0  // just swap
	movups	(%rdx),%xmm1
	movups	%xmm0,(%rdx)
	movups	%xmm1,(%rdi)
	leaq	16(%rdi),%rdi
	leaq	-16(%rdx),%rdx

.Ldec_key_inverse:
	movups	(%rdi),%xmm0  // swap and inverse
	movups	(%rdx),%xmm1
	aesimc	%xmm0,%xmm0
	aesimc	%xmm1,%xmm1
	leaq	16(%rdi),%rdi
	leaq	-16(%rdx),%rdx
	movups	%xmm0,16(%rdx)
	movups	%xmm1,-16(%rdi)
	cmpq	%rdi,%rdx
	ja	.Ldec_key_inverse

	movups	(%rdi),%xmm0  // inverse middle
	aesimc	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	movups	%xmm0,(%rdx)
	pxor	%xmm0,%xmm0
	ret
.cfi_endproc	
.size	aes_hw_encrypt_key_to_decrypt_key,.-aes_hw_encrypt_key_to_decrypt_key
.globl	aes_hw_set_encrypt_key_base
.hidden aes_hw_set_encrypt_key_base
.type	aes_hw_set_encrypt_key_base,@function
.align	16
aes_hw_set_encrypt_key_base:
.cfi_startproc	

_CET_ENDBR
#ifdef BORINGSSL_DISPATCH_TEST
	movb	$1,BORINGSSL_function_hit+3(%rip)
#endif
	subq	$8,%rsp
.cfi_adjust_cfa_offset	8


	movups	(%rdi),%xmm0  // pull first 128 bits of *userKey
	xorps	%xmm4,%xmm4  // low dword of xmm4 is assumed 0
	leaq	16(%rdx),%rax  // %rax is used as modifiable copy of %rdx
	cmpl	$256,%esi
	je	.L14rounds
	cmpl	$192,%esi
	je	.L12rounds
	cmpl	$128,%esi
	jne	.Lbad_keybits

.L10rounds:
	movl	$9,%esi  // 10 rounds for 128-bit key

	movups	%xmm0,(%rdx)  // round 0
	aeskeygenassist	$0x1,%xmm0,%xmm1  // round 1
	call	.Lkey_expansion_128_cold
	aeskeygenassist	$0x2,%xmm0,%xmm1  // round 2
	call	.Lkey_expansion_128
	aeskeygenassist	$0x4,%xmm0,%xmm1  // round 3
	call	.Lkey_expansion_128
	aeskeygenassist	$0x8,%xmm0,%xmm1  // round 4
	call	.Lkey_expansion_128
	aeskeygenassist	$0x10,%xmm0,%xmm1  // round 5
	call	.Lkey_expansion_128
	aeskeygenassist	$0x20,%xmm0,%xmm1  // round 6
	call	.Lkey_expansion_128
	aeskeygenassist	$0x40,%xmm0,%xmm1  // round 7
	call	.Lkey_expansion_128
	aeskeygenassist	$0x80,%xmm0,%xmm1  // round 8
	call	.Lkey_expansion_128
	aeskeygenassist	$0x1b,%xmm0,%xmm1  // round 9
	call	.Lkey_expansion_128
	aeskeygenassist	$0x36,%xmm0,%xmm1  // round 10
	call	.Lkey_expansion_128
	movups	%xmm0,(%rax)
	movl	%esi,80(%rax)  // 240(%rdx)
	xorl	%eax,%eax
	jmp	.Lenc_key_ret

.align	16
.L12rounds:
	movq	16(%rdi),%xmm2  // remaining 1/3 of *userKey
	movl	$11,%esi  // 12 rounds for 192

	movups	%xmm0,(%rdx)  // round 0
	aeskeygenassist	$0x1,%xmm2,%xmm1  // round 1,2
	call	.Lkey_expansion_192a_cold
	aeskeygenassist	$0x2,%xmm2,%xmm1  // round 2,3
	call	.Lkey_expansion_192b
	aeskeygenassist	$0x4,%xmm2,%xmm1  // round 4,5
	call	.Lkey_expansion_192a
	aeskeygenassist	$0x8,%xmm2,%xmm1  // round 5,6
	call	.Lkey_expansion_192b
	aeskeygenassist	$0x10,%xmm2,%xmm1  // round 7,8
	call	.Lkey_expansion_192a
	aeskeygenassist	$0x20,%xmm2,%xmm1  // round 8,9
	call	.Lkey_expansion_192b
	aeskeygenassist	$0x40,%xmm2,%xmm1  // round 10,11
	call	.Lkey_expansion_192a
	aeskeygenassist	$0x80,%xmm2,%xmm1  // round 11,12
	call	.Lkey_expansion_192b
	movups	%xmm0,(%rax)
	movl	%esi,48(%rax)  // 240(%rdx)
	xorq	%rax,%rax
	jmp	.Lenc_key_ret

.align	16
.L14rounds:
	movups	16(%rdi),%xmm2  // remaining half of *userKey
	movl	$13,%esi  // 14 rounds for 256
	leaq	16(%rax),%rax

	movups	%xmm0,(%rdx)  // round 0
	movups	%xmm2,16(%rdx)  // round 1
	aeskeygenassist	$0x1,%xmm2,%xmm1  // round 2
	call	.Lkey_expansion_256a_cold
	aeskeygenassist	$0x1,%xmm0,%xmm1  // round 3
	call	.Lkey_expansion_256b
	aeskeygenassist	$0x2,%xmm2,%xmm1  // round 4
	call	.Lkey_expansion_256a
	aeskeygenassist	$0x2,%xmm0,%xmm1  // round 5
	call	.Lkey_expansion_256b
	aeskeygenassist	$0x4,%xmm2,%xmm1  // round 6
	call	.Lkey_expansion_256a
	aeskeygenassist	$0x4,%xmm0,%xmm1  // round 7
	call	.Lkey_expansion_256b
	aeskeygenassist	$0x8,%xmm2,%xmm1  // round 8
	call	.Lkey_expansion_256a
	aeskeygenassist	$0x8,%xmm0,%xmm1  // round 9
	call	.Lkey_expansion_256b
	aeskeygenassist	$0x10,%xmm2,%xmm1  // round 10
	call	.Lkey_expansion_256a
	aeskeygenassist	$0x10,%xmm0,%xmm1  // round 11
	call	.Lkey_expansion_256b
	aeskeygenassist	$0x20,%xmm2,%xmm1  // round 12
	call	.Lkey_expansion_256a
	aeskeygenassist	$0x20,%xmm0,%xmm1  // round 13
	call	.Lkey_expansion_256b
	aeskeygenassist	$0x40,%xmm2,%xmm1  // round 14
	call	.Lkey_expansion_256a
	movups	%xmm0,(%rax)
	movl	%esi,16(%rax)  // 240(%rdx)
	xorq	%rax,%rax
	jmp	.Lenc_key_ret

.align	16
.Lbad_keybits:
	movq	$-2,%rax
.Lenc_key_ret:
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	addq	$8,%rsp
.cfi_adjust_cfa_offset	-8
	ret
.cfi_endproc	

.size	aes_hw_set_encrypt_key_base,.-aes_hw_set_encrypt_key_base

.align	16
.Lkey_expansion_128:
.cfi_startproc	
	movups	%xmm0,(%rax)
	leaq	16(%rax),%rax
.Lkey_expansion_128_cold:
	shufps	$16,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$255,%xmm1,%xmm1  // critical path
	xorps	%xmm1,%xmm0
	ret
.cfi_endproc	

.align	16
.Lkey_expansion_192a:
.cfi_startproc	
	movups	%xmm0,(%rax)
	leaq	16(%rax),%rax
.Lkey_expansion_192a_cold:
	movaps	%xmm2,%xmm5
.Lkey_expansion_192b_warm:
	shufps	$16,%xmm0,%xmm4
	movdqa	%xmm2,%xmm3
	xorps	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	pslldq	$4,%xmm3
	xorps	%xmm4,%xmm0
	pshufd	$85,%xmm1,%xmm1  // critical path
	pxor	%xmm3,%xmm2
	pxor	%xmm1,%xmm0
	pshufd	$255,%xmm0,%xmm3
	pxor	%xmm3,%xmm2
	ret
.cfi_endproc	

.align	16
.Lkey_expansion_192b:
.cfi_startproc	
	movaps	%xmm0,%xmm3
	shufps	$68,%xmm0,%xmm5
	movups	%xmm5,(%rax)
	shufps	$78,%xmm2,%xmm3
	movups	%xmm3,16(%rax)
	leaq	32(%rax),%rax
	jmp	.Lkey_expansion_192b_warm
.cfi_endproc	

.align	16
.Lkey_expansion_256a:
.cfi_startproc	
	movups	%xmm2,(%rax)
	leaq	16(%rax),%rax
.Lkey_expansion_256a_cold:
	shufps	$16,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$140,%xmm0,%xmm4
	xorps	%xmm4,%xmm0
	shufps	$255,%xmm1,%xmm1  // critical path
	xorps	%xmm1,%xmm0
	ret
.cfi_endproc	

.align	16
.Lkey_expansion_256b:
.cfi_startproc	
	movups	%xmm0,(%rax)
	leaq	16(%rax),%rax

	shufps	$16,%xmm2,%xmm4
	xorps	%xmm4,%xmm2
	shufps	$140,%xmm2,%xmm4
	xorps	%xmm4,%xmm2
	shufps	$170,%xmm1,%xmm1  // critical path
	xorps	%xmm1,%xmm2
	ret
.cfi_endproc	

.globl	aes_hw_set_encrypt_key_alt
.hidden aes_hw_set_encrypt_key_alt
.type	aes_hw_set_encrypt_key_alt,@function
.align	16
aes_hw_set_encrypt_key_alt:
.cfi_startproc	

_CET_ENDBR
#ifdef BORINGSSL_DISPATCH_TEST
	movb	$1,BORINGSSL_function_hit+3(%rip)
#endif
	subq	$8,%rsp
.cfi_adjust_cfa_offset	8


	movups	(%rdi),%xmm0  // pull first 128 bits of *userKey
	xorps	%xmm4,%xmm4  // low dword of xmm4 is assumed 0
	leaq	16(%rdx),%rax  // %rax is used as modifiable copy of %rdx
	cmpl	$256,%esi
	je	.L14rounds_alt
	cmpl	$192,%esi
	je	.L12rounds_alt
	cmpl	$128,%esi
	jne	.Lbad_keybits_alt

	movl	$9,%esi  // 10 rounds for 128-bit key
	movdqa	.Lkey_rotate(%rip),%xmm5
	movl	$8,%r10d
	movdqa	.Lkey_rcon1(%rip),%xmm4
	movdqa	%xmm0,%xmm2
	movdqu	%xmm0,(%rdx)
	jmp	.Loop_key128

.align	16
.Loop_key128:
	pshufb	%xmm5,%xmm0
	aesenclast	%xmm4,%xmm0
	pslld	$1,%xmm4
	leaq	16(%rax),%rax

	movdqa	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm3,%xmm2

	pxor	%xmm2,%xmm0
	movdqu	%xmm0,-16(%rax)
	movdqa	%xmm0,%xmm2

	decl	%r10d
	jnz	.Loop_key128

	movdqa	.Lkey_rcon1b(%rip),%xmm4

	pshufb	%xmm5,%xmm0
	aesenclast	%xmm4,%xmm0
	pslld	$1,%xmm4

	movdqa	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm3,%xmm2

	pxor	%xmm2,%xmm0
	movdqu	%xmm0,(%rax)

	movdqa	%xmm0,%xmm2
	pshufb	%xmm5,%xmm0
	aesenclast	%xmm4,%xmm0

	movdqa	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm2,%xmm3
	pslldq	$4,%xmm2
	pxor	%xmm3,%xmm2

	pxor	%xmm2,%xmm0
	movdqu	%xmm0,16(%rax)

	movl	%esi,96(%rax)  // 240(%rdx)
	xorl	%eax,%eax
	jmp	.Lenc_key_ret_alt

.align	16
.L12rounds_alt:
	movq	16(%rdi),%xmm2  // remaining 1/3 of *userKey
	movl	$11,%esi  // 12 rounds for 192
	movdqa	.Lkey_rotate192(%rip),%xmm5
	movdqa	.Lkey_rcon1(%rip),%xmm4
	movl	$8,%r10d
	movdqu	%xmm0,(%rdx)
	jmp	.Loop_key192

.align	16
.Loop_key192:
	movq	%xmm2,0(%rax)
	movdqa	%xmm2,%xmm1
	pshufb	%xmm5,%xmm2
	aesenclast	%xmm4,%xmm2
	pslld	$1,%xmm4
	leaq	24(%rax),%rax

	movdqa	%xmm0,%xmm3
	pslldq	$4,%xmm0
	pxor	%xmm0,%xmm3
	pslldq	$4,%xmm0
	pxor	%xmm0,%xmm3
	pslldq	$4,%xmm0
	pxor	%xmm3,%xmm0

	pshufd	$0xff,%xmm0,%xmm3
	pxor	%xmm1,%xmm3
	pslldq	$4,%xmm1
	pxor	%xmm1,%xmm3

	pxor	%xmm2,%xmm0
	pxor	%xmm3,%xmm2
	movdqu	%xmm0,-16(%rax)

	decl	%r10d
	jnz	.Loop_key192

	movl	%esi,32(%rax)  // 240(%rdx)
	xorl	%eax,%eax
	jmp	.Lenc_key_ret_alt

.align	16
.L14rounds_alt:
	movups	16(%rdi),%xmm2  // remaining half of *userKey
	movl	$13,%esi  // 14 rounds for 256
	leaq	16(%rax),%rax
	movdqa	.Lkey_rotate(%rip),%xmm5
	movdqa	.Lkey_rcon1(%rip),%xmm4
	movl	$7,%r10d
	movdqu	%xmm0,0(%rdx)
	movdqa	%xmm2,%xmm1
	movdqu	%xmm2,16(%rdx)
	jmp	.Loop_key256

.align	16
.Loop_key256:
	pshufb	%xmm5,%xmm2
	aesenclast	%xmm4,%xmm2

	movdqa	%xmm0,%xmm3
	pslldq	$4,%xmm0
	pxor	%xmm0,%xmm3
	pslldq	$4,%xmm0
	pxor	%xmm0,%xmm3
	pslldq	$4,%xmm0
	pxor	%xmm3,%xmm0
	pslld	$1,%xmm4

	pxor	%xmm2,%xmm0
	movdqu	%xmm0,(%rax)

	decl	%r10d
	jz	.Ldone_key256

	pshufd	$0xff,%xmm0,%xmm2
	pxor	%xmm3,%xmm3
	aesenclast	%xmm3,%xmm2

	movdqa	%xmm1,%xmm3
	pslldq	$4,%xmm1
	pxor	%xmm1,%xmm3
	pslldq	$4,%xmm1
	pxor	%xmm1,%xmm3
	pslldq	$4,%xmm1
	pxor	%xmm3,%xmm1

	pxor	%xmm1,%xmm2
	movdqu	%xmm2,16(%rax)
	leaq	32(%rax),%rax
	movdqa	%xmm2,%xmm1

	jmp	.Loop_key256

.Ldone_key256:
	movl	%esi,16(%rax)  // 240(%rdx)
	xorl	%eax,%eax
	jmp	.Lenc_key_ret_alt

.align	16
.Lbad_keybits_alt:
	movq	$-2,%rax
.Lenc_key_ret_alt:
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	addq	$8,%rsp
.cfi_adjust_cfa_offset	-8
	ret
.cfi_endproc	

.size	aes_hw_set_encrypt_key_alt,.-aes_hw_set_encrypt_key_alt
.section	.rodata

.align	64
aesni_constants:
.Lbswap_mask:
.byte	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
.Lincrement32:
.long	6,6,6,0
.Lincrement64:
.long	1,0,0,0
.Lxts_magic:
.long	0x87,0,1,0
.Lincrement1:
.byte	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
.Lkey_rotate:
.long	0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d
.Lkey_rotate192:
.long	0x04070605,0x04070605,0x04070605,0x04070605
.Lkey_rcon1:
.long	1,1,1,1
.Lkey_rcon1b:
.long	0x1b,0x1b,0x1b,0x1b

.byte	65,69,83,32,102,111,114,32,73,110,116,101,108,32,65,69,83,45,78,73,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	64
.text	
#endif
