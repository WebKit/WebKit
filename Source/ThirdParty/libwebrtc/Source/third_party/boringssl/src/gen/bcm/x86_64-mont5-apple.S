// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	

.globl	_bn_mul_mont_gather5_nohw
.private_extern _bn_mul_mont_gather5_nohw

.p2align	6
_bn_mul_mont_gather5_nohw:

_CET_ENDBR
// num is declared as an int, a 32-bit parameter, so the upper half is
// undefined. Zero the upper half to normalize it.
	movl	%r9d,%r9d
	movq	%rsp,%rax

	movd	8(%rsp),%xmm5  // load 7th argument
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	negq	%r9
	movq	%rsp,%r11
	leaq	-280(%rsp,%r9,8),%r10  // future alloca(8*(num+2)+256+8)
	negq	%r9  // restore %r9
	andq	$-1024,%r10  // minimize TLB usage

// An OS-agnostic version of __chkstk.
// 
// Some OSes (Windows) insist on stack being "wired" to
// physical memory in strictly sequential manner, i.e. if stack
// allocation spans two pages, then reference to farmost one can
// be punishable by SEGV. But page walking can do good even on
// other OSes, because it guarantees that villain thread hits
// the guard page before it can make damage to innocent one...
	subq	%r10,%r11
	andq	$-4096,%r11
	leaq	(%r10,%r11,1),%rsp
	movq	(%rsp),%r11
	cmpq	%r10,%rsp
	ja	L$mul_page_walk
	jmp	L$mul_page_walk_done

L$mul_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r11
	cmpq	%r10,%rsp
	ja	L$mul_page_walk
L$mul_page_walk_done:

	leaq	L$inc(%rip),%r10
	movq	%rax,8(%rsp,%r9,8)  // tp[num+1]=%rsp

L$mul_body:

	leaq	128(%rdx),%r12  // reassign %rdx (+size optimization)
	movdqa	0(%r10),%xmm0  // 00000001000000010000000000000000
	movdqa	16(%r10),%xmm1  // 00000002000000020000000200000002
	leaq	24-112(%rsp,%r9,8),%r10  // place the mask after tp[num+3] (+ICache optimization)
	andq	$-16,%r10

	pshufd	$0,%xmm5,%xmm5  // broadcast index
	movdqa	%xmm1,%xmm4
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0  // compare to 1,0
.byte	0x67
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,112(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,128(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,144(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,160(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,176(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,192(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,208(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,224(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,240(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,256(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,272(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,288(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,304(%r10)

	paddd	%xmm2,%xmm3
.byte	0x67
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,320(%r10)

	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,336(%r10)
	pand	64(%r12),%xmm0  // while it's still in register

	pand	80(%r12),%xmm1
	pand	96(%r12),%xmm2
	movdqa	%xmm3,352(%r10)
	pand	112(%r12),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-128(%r12),%xmm4
	movdqa	-112(%r12),%xmm5
	movdqa	-96(%r12),%xmm2
	pand	112(%r10),%xmm4
	movdqa	-80(%r12),%xmm3
	pand	128(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	144(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	160(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-64(%r12),%xmm4
	movdqa	-48(%r12),%xmm5
	movdqa	-32(%r12),%xmm2
	pand	176(%r10),%xmm4
	movdqa	-16(%r12),%xmm3
	pand	192(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	208(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	224(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	0(%r12),%xmm4
	movdqa	16(%r12),%xmm5
	movdqa	32(%r12),%xmm2
	pand	240(%r10),%xmm4
	movdqa	48(%r12),%xmm3
	pand	256(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	272(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	288(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	por	%xmm1,%xmm0
// Combine the upper and lower halves of %xmm0.
	pshufd	$0x4e,%xmm0,%xmm1  // Swap upper and lower halves.
	por	%xmm1,%xmm0
	leaq	256(%r12),%r12
	movq	%xmm0,%rbx  // m0=bp[0]

	movq	(%r8),%r8  // pull n0[0] value
	movq	(%rsi),%rax

	xorq	%r14,%r14  // i=0
	xorq	%r15,%r15  // j=0

	movq	%r8,%rbp
	mulq	%rbx  // ap[0]*bp[0]
	movq	%rax,%r10
	movq	(%rcx),%rax

	imulq	%r10,%rbp  // "tp[0]"*n0
	movq	%rdx,%r11

	mulq	%rbp  // np[0]*m1
	addq	%rax,%r10  // discarded
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%r13

	leaq	1(%r15),%r15  // j++
	jmp	L$1st_enter

.p2align	4
L$1st:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%r13  // np[j]*m1+ap[j]*bp[0]
	movq	%r10,%r11
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13

L$1st_enter:
	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r11
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	leaq	1(%r15),%r15  // j++
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	cmpq	%r9,%r15
	jne	L$1st  // note that upon exit %r15==%r9, so
// they can be used interchangeably

	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%r11,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r9,8)  // tp[num-1]
	movq	%rdx,%r13
	movq	%r10,%r11

	xorq	%rdx,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r9,8)
	movq	%rdx,(%rsp,%r9,8)  // store upmost overflow bit

	leaq	1(%r14),%r14  // i++
	jmp	L$outer
.p2align	4
L$outer:
	leaq	24+128(%rsp,%r9,8),%rdx  // where 256-byte mask is (+size optimization)
	andq	$-16,%rdx
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r12),%xmm0
	movdqa	-112(%r12),%xmm1
	movdqa	-96(%r12),%xmm2
	movdqa	-80(%r12),%xmm3
	pand	-128(%rdx),%xmm0
	pand	-112(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r12),%xmm0
	movdqa	-48(%r12),%xmm1
	movdqa	-32(%r12),%xmm2
	movdqa	-16(%r12),%xmm3
	pand	-64(%rdx),%xmm0
	pand	-48(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r12),%xmm0
	movdqa	16(%r12),%xmm1
	movdqa	32(%r12),%xmm2
	movdqa	48(%r12),%xmm3
	pand	0(%rdx),%xmm0
	pand	16(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r12),%xmm0
	movdqa	80(%r12),%xmm1
	movdqa	96(%r12),%xmm2
	movdqa	112(%r12),%xmm3
	pand	64(%rdx),%xmm0
	pand	80(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
// Combine the upper and lower halves of %xmm4 as %xmm0.
	pshufd	$0x4e,%xmm4,%xmm0  // Swap upper and lower halves.
	por	%xmm4,%xmm0
	leaq	256(%r12),%r12

	movq	(%rsi),%rax  // ap[0]
	movq	%xmm0,%rbx  // m0=bp[i]

	xorq	%r15,%r15  // j=0
	movq	%r8,%rbp
	movq	(%rsp),%r10

	mulq	%rbx  // ap[0]*bp[i]
	addq	%rax,%r10  // ap[0]*bp[i]+tp[0]
	movq	(%rcx),%rax
	adcq	$0,%rdx

	imulq	%r10,%rbp  // tp[0]*n0
	movq	%rdx,%r11

	mulq	%rbp  // np[0]*m1
	addq	%rax,%r10  // discarded
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	8(%rsp),%r10  // tp[1]
	movq	%rdx,%r13

	leaq	1(%r15),%r15  // j++
	jmp	L$inner_enter

.p2align	4
L$inner:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[i]+tp[j]
	movq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13

L$inner_enter:
	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%r10  // ap[j]*bp[i]+tp[j]
	movq	%rdx,%r11
	adcq	$0,%r11
	leaq	1(%r15),%r15  // j++

	mulq	%rbp  // np[j]*m1
	cmpq	%r9,%r15
	jne	L$inner  // note that upon exit %r15==%r9, so
// they can be used interchangeably
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[i]+tp[j]
	movq	(%rsp,%r9,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r9,8)  // tp[num-1]
	movq	%rdx,%r13

	xorq	%rdx,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	addq	%r10,%r13  // pull upmost overflow bit
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r9,8)
	movq	%rdx,(%rsp,%r9,8)  // store upmost overflow bit

	leaq	1(%r14),%r14  // i++
	cmpq	%r9,%r14
	jb	L$outer

	xorq	%r14,%r14  // i=0 and clear CF
	movq	(%rsp),%rax  // tp[0]
	leaq	(%rsp),%rsi  // borrow ap for tp
	movq	%r9,%r15  // j=num
	jmp	L$sub
.p2align	4
L$sub:	sbbq	(%rcx,%r14,8),%rax
	movq	%rax,(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	movq	8(%rsi,%r14,8),%rax  // tp[i+1]
	leaq	1(%r14),%r14  // i++
	decq	%r15  // doesn't affect CF
	jnz	L$sub

	sbbq	$0,%rax  // handle upmost overflow bit
	movq	$-1,%rbx
	xorq	%rax,%rbx
	xorq	%r14,%r14
	movq	%r9,%r15  // j=num

L$copy:  // conditional copy
	movq	(%rdi,%r14,8),%rcx
	movq	(%rsp,%r14,8),%rdx
	andq	%rbx,%rcx
	andq	%rax,%rdx
	movq	%r14,(%rsp,%r14,8)  // zap temporary vector
	orq	%rcx,%rdx
	movq	%rdx,(%rdi,%r14,8)  // rp[i]=tp[i]
	leaq	1(%r14),%r14
	subq	$1,%r15
	jnz	L$copy

	movq	8(%rsp,%r9,8),%rsi  // restore %rsp

	movq	$1,%rax

	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$mul_epilogue:
	ret


.globl	_bn_mul4x_mont_gather5
.private_extern _bn_mul4x_mont_gather5

.p2align	5
.alt_entry _bn_mul4x_mont_gather5
_bn_mul4x_mont_gather5:

_CET_ENDBR
.byte	0x67
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$mul4x_prologue:

.byte	0x67
// num is declared as an int, a 32-bit parameter, so the upper half is
// undefined. It is important that this write to %r9, which zeros the
// upper half, predates the first access.
	shll	$3,%r9d  // convert %r9 to bytes
	leaq	(%r9,%r9,2),%r10  // 3*%r9 in bytes
	negq	%r9  // -%r9

// #############################################################
// Ensure that stack frame doesn't alias with +3*%r9
// modulo 4096, which covers ret[num], am[num] and n[num]
// (see bn_exp.c). This is done to allow memory disambiguation
// logic do its magic. [Extra [num] is allocated in order
// to align with bn_power5's frame, which is cleansed after
// completing exponentiation. Extra 256 bytes is for power mask
// calculated from 7th argument, the index.]
// 
	leaq	-320(%rsp,%r9,2),%r11
	movq	%rsp,%rbp
	subq	%rdi,%r11
	andq	$4095,%r11
	cmpq	%r11,%r10
	jb	L$mul4xsp_alt
	subq	%r11,%rbp  // align with %rdi
	leaq	-320(%rbp,%r9,2),%rbp  // future alloca(frame+2*num*8+256)
	jmp	L$mul4xsp_done

.p2align	5
L$mul4xsp_alt:
	leaq	4096-320(,%r9,2),%r10
	leaq	-320(%rbp,%r9,2),%rbp  // future alloca(frame+2*num*8+256)
	subq	%r10,%r11
	movq	$0,%r10
	cmovcq	%r10,%r11
	subq	%r11,%rbp
L$mul4xsp_done:
	andq	$-64,%rbp
	movq	%rsp,%r11
	subq	%rbp,%r11
	andq	$-4096,%r11
	leaq	(%r11,%rbp,1),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$mul4x_page_walk
	jmp	L$mul4x_page_walk_done

L$mul4x_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$mul4x_page_walk
L$mul4x_page_walk_done:

	negq	%r9

	movq	%rax,40(%rsp)

L$mul4x_body:

	call	mul4x_internal

	movq	40(%rsp),%rsi  // restore %rsp

	movq	$1,%rax

	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$mul4x_epilogue:
	ret




.p2align	5
.alt_entry mul4x_internal
mul4x_internal:

	shlq	$5,%r9  // %r9 was in bytes
	movd	8(%rax),%xmm5  // load 7th argument, index
	leaq	L$inc(%rip),%rax
	leaq	128(%rdx,%r9,1),%r13  // end of powers table (+size optimization)
	shrq	$5,%r9  // restore %r9
	movdqa	0(%rax),%xmm0  // 00000001000000010000000000000000
	movdqa	16(%rax),%xmm1  // 00000002000000020000000200000002
	leaq	88-112(%rsp,%r9,1),%r10  // place the mask after tp[num+1] (+ICache optimization)
	leaq	128(%rdx),%r12  // size optimization

	pshufd	$0,%xmm5,%xmm5  // broadcast index
	movdqa	%xmm1,%xmm4
.byte	0x67,0x67
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0  // compare to 1,0
.byte	0x67
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,112(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,128(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,144(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,160(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,176(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,192(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,208(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,224(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,240(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,256(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,272(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,288(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,304(%r10)

	paddd	%xmm2,%xmm3
.byte	0x67
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,320(%r10)

	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,336(%r10)
	pand	64(%r12),%xmm0  // while it's still in register

	pand	80(%r12),%xmm1
	pand	96(%r12),%xmm2
	movdqa	%xmm3,352(%r10)
	pand	112(%r12),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-128(%r12),%xmm4
	movdqa	-112(%r12),%xmm5
	movdqa	-96(%r12),%xmm2
	pand	112(%r10),%xmm4
	movdqa	-80(%r12),%xmm3
	pand	128(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	144(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	160(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-64(%r12),%xmm4
	movdqa	-48(%r12),%xmm5
	movdqa	-32(%r12),%xmm2
	pand	176(%r10),%xmm4
	movdqa	-16(%r12),%xmm3
	pand	192(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	208(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	224(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	0(%r12),%xmm4
	movdqa	16(%r12),%xmm5
	movdqa	32(%r12),%xmm2
	pand	240(%r10),%xmm4
	movdqa	48(%r12),%xmm3
	pand	256(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	272(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	288(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	por	%xmm1,%xmm0
// Combine the upper and lower halves of %xmm0.
	pshufd	$0x4e,%xmm0,%xmm1  // Swap upper and lower halves.
	por	%xmm1,%xmm0
	leaq	256(%r12),%r12
	movq	%xmm0,%rbx  // m0=bp[0]

	movq	%r13,16+8(%rsp)  // save end of b[num]
	movq	%rdi,56+8(%rsp)  // save %rdi

	movq	(%r8),%r8  // pull n0[0] value
	movq	(%rsi),%rax
	leaq	(%rsi,%r9,1),%rsi  // end of a[num]
	negq	%r9

	movq	%r8,%rbp
	mulq	%rbx  // ap[0]*bp[0]
	movq	%rax,%r10
	movq	(%rcx),%rax

	imulq	%r10,%rbp  // "tp[0]"*n0
	leaq	64+8(%rsp),%r14
	movq	%rdx,%r11

	mulq	%rbp  // np[0]*m1
	addq	%rax,%r10  // discarded
	movq	8(%rsi,%r9,1),%rax
	adcq	$0,%rdx
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	16(%rsi,%r9,1),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	leaq	32(%r9),%r15  // j=4
	leaq	32(%rcx),%rcx
	adcq	$0,%rdx
	movq	%rdi,(%r14)
	movq	%rdx,%r13
	jmp	L$1st4x

.p2align	5
L$1st4x:
	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r10
	movq	-16(%rcx),%rax
	leaq	32(%r14),%r14
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-24(%r14)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r11
	movq	-8(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%rdi,-16(%r14)  // tp[j-1]
	movq	%rdx,%r13

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r10
	movq	0(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	8(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-8(%r14)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	16(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[0]
	leaq	32(%rcx),%rcx
	adcq	$0,%rdx
	movq	%rdi,(%r14)  // tp[j-1]
	movq	%rdx,%r13

	addq	$32,%r15  // j+=4
	jnz	L$1st4x

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r10
	movq	-16(%rcx),%rax
	leaq	32(%r14),%r14
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-24(%r14)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r11
	movq	-8(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi,%r9,1),%rax  // ap[0]
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%rdi,-16(%r14)  // tp[j-1]
	movq	%rdx,%r13

	leaq	(%rcx,%r9,1),%rcx  // rewind %rcx

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	movq	%r13,-8(%r14)

	jmp	L$outer4x

.p2align	5
L$outer4x:
	leaq	16+128(%r14),%rdx  // where 256-byte mask is (+size optimization)
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r12),%xmm0
	movdqa	-112(%r12),%xmm1
	movdqa	-96(%r12),%xmm2
	movdqa	-80(%r12),%xmm3
	pand	-128(%rdx),%xmm0
	pand	-112(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r12),%xmm0
	movdqa	-48(%r12),%xmm1
	movdqa	-32(%r12),%xmm2
	movdqa	-16(%r12),%xmm3
	pand	-64(%rdx),%xmm0
	pand	-48(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r12),%xmm0
	movdqa	16(%r12),%xmm1
	movdqa	32(%r12),%xmm2
	movdqa	48(%r12),%xmm3
	pand	0(%rdx),%xmm0
	pand	16(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r12),%xmm0
	movdqa	80(%r12),%xmm1
	movdqa	96(%r12),%xmm2
	movdqa	112(%r12),%xmm3
	pand	64(%rdx),%xmm0
	pand	80(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
// Combine the upper and lower halves of %xmm4 as %xmm0.
	pshufd	$0x4e,%xmm4,%xmm0  // Swap upper and lower halves.
	por	%xmm4,%xmm0
	leaq	256(%r12),%r12
	movq	%xmm0,%rbx  // m0=bp[i]

	movq	(%r14,%r9,1),%r10
	movq	%r8,%rbp
	mulq	%rbx  // ap[0]*bp[i]
	addq	%rax,%r10  // ap[0]*bp[i]+tp[0]
	movq	(%rcx),%rax
	adcq	$0,%rdx

	imulq	%r10,%rbp  // tp[0]*n0
	movq	%rdx,%r11
	movq	%rdi,(%r14)  // store upmost overflow bit

	leaq	(%r14,%r9,1),%r14  // rewind %r14

	mulq	%rbp  // np[0]*m1
	addq	%rax,%r10  // "%r13", discarded
	movq	8(%rsi,%r9,1),%rax
	adcq	$0,%rdx
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	addq	8(%r14),%r11  // +tp[1]
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	16(%rsi,%r9,1),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[i]+tp[j]
	leaq	32(%r9),%r15  // j=4
	leaq	32(%rcx),%rcx
	adcq	$0,%rdx
	movq	%rdx,%r13
	jmp	L$inner4x

.p2align	5
L$inner4x:
	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r10
	movq	-16(%rcx),%rax
	adcq	$0,%rdx
	addq	16(%r14),%r10  // ap[j]*bp[i]+tp[j]
	leaq	32(%r14),%r14
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-32(%r14)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	-8(%rcx),%rax
	adcq	$0,%rdx
	addq	-8(%r14),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-24(%r14)  // tp[j-1]
	movq	%rdx,%r13

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r10
	movq	0(%rcx),%rax
	adcq	$0,%rdx
	addq	(%r14),%r10  // ap[j]*bp[i]+tp[j]
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	8(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-16(%r14)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	addq	8(%r14),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	16(%rsi,%r15,1),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	leaq	32(%rcx),%rcx
	adcq	$0,%rdx
	movq	%r13,-8(%r14)  // tp[j-1]
	movq	%rdx,%r13

	addq	$32,%r15  // j+=4
	jnz	L$inner4x

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r10
	movq	-16(%rcx),%rax
	adcq	$0,%rdx
	addq	16(%r14),%r10  // ap[j]*bp[i]+tp[j]
	leaq	32(%r14),%r14
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-32(%r14)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	%rbp,%rax
	movq	-8(%rcx),%rbp
	adcq	$0,%rdx
	addq	-8(%r14),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi,%r9,1),%rax  // ap[0]
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-24(%r14)  // tp[j-1]
	movq	%rdx,%r13

	movq	%rdi,-16(%r14)  // tp[j-1]
	leaq	(%rcx,%r9,1),%rcx  // rewind %rcx

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	addq	(%r14),%r13  // pull upmost overflow bit
	adcq	$0,%rdi  // upmost overflow bit
	movq	%r13,-8(%r14)

	cmpq	16+8(%rsp),%r12
	jb	L$outer4x
	xorq	%rax,%rax
	subq	%r13,%rbp  // compare top-most words
	adcq	%r15,%r15  // %r15 is zero
	orq	%r15,%rdi
	subq	%rdi,%rax  // %rax=-%rdi
	leaq	(%r14,%r9,1),%rbx  // tptr in .sqr4x_sub
	movq	(%rcx),%r12
	leaq	(%rcx),%rbp  // nptr in .sqr4x_sub
	movq	%r9,%rcx
	sarq	$3+2,%rcx
	movq	56+8(%rsp),%rdi  // rptr in .sqr4x_sub
	decq	%r12  // so that after 'not' we get -n[0]
	xorq	%r10,%r10
	movq	8(%rbp),%r13
	movq	16(%rbp),%r14
	movq	24(%rbp),%r15
	jmp	L$sqr4x_sub_entry


.globl	_bn_power5_nohw
.private_extern _bn_power5_nohw

.p2align	5
.alt_entry _bn_power5_nohw
_bn_power5_nohw:

_CET_ENDBR
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$power5_prologue:

// num is declared as an int, a 32-bit parameter, so the upper half is
// undefined. It is important that this write to %r9, which zeros the
// upper half, come before the first access.
	shll	$3,%r9d  // convert %r9 to bytes
	leal	(%r9,%r9,2),%r10d  // 3*%r9
	negq	%r9
	movq	(%r8),%r8  // *n0

// #############################################################
// Ensure that stack frame doesn't alias with %rdi+3*%r9
// modulo 4096, which covers ret[num], am[num] and n[num]
// (see bn_exp.c). This is done to allow memory disambiguation
// logic do its magic. [Extra 256 bytes is for power mask
// calculated from 7th argument, the index.]
// 
	leaq	-320(%rsp,%r9,2),%r11
	movq	%rsp,%rbp
	subq	%rdi,%r11
	andq	$4095,%r11
	cmpq	%r11,%r10
	jb	L$pwr_sp_alt
	subq	%r11,%rbp  // align with %rsi
	leaq	-320(%rbp,%r9,2),%rbp  // future alloca(frame+2*num*8+256)
	jmp	L$pwr_sp_done

.p2align	5
L$pwr_sp_alt:
	leaq	4096-320(,%r9,2),%r10
	leaq	-320(%rbp,%r9,2),%rbp  // future alloca(frame+2*num*8+256)
	subq	%r10,%r11
	movq	$0,%r10
	cmovcq	%r10,%r11
	subq	%r11,%rbp
L$pwr_sp_done:
	andq	$-64,%rbp
	movq	%rsp,%r11
	subq	%rbp,%r11
	andq	$-4096,%r11
	leaq	(%r11,%rbp,1),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$pwr_page_walk
	jmp	L$pwr_page_walk_done

L$pwr_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$pwr_page_walk
L$pwr_page_walk_done:

	movq	%r9,%r10
	negq	%r9

// #############################################################
// Stack layout
// 
// +0	saved %r9, used in reduction section
// +8	&t[2*%r9], used in reduction section
// +32	saved *n0
// +40	saved %rsp
// +48	t[2*%r9]
// 
	movq	%r8,32(%rsp)
	movq	%rax,40(%rsp)  // save original %rsp

L$power5_body:
	movq	%rdi,%xmm1  // save %rdi, used in sqr8x
	movq	%rcx,%xmm2  // save %rcx
	movq	%r10,%xmm3  // -%r9, used in sqr8x
	movq	%rdx,%xmm4

	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal

	movq	%xmm2,%rcx
	movq	%xmm4,%rdx
	movq	%rsi,%rdi
	movq	40(%rsp),%rax
	leaq	32(%rsp),%r8

	call	mul4x_internal

	movq	40(%rsp),%rsi  // restore %rsp

	movq	$1,%rax
	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$power5_epilogue:
	ret



.globl	_bn_sqr8x_internal
.private_extern _bn_sqr8x_internal
.private_extern	_bn_sqr8x_internal

.p2align	5
.alt_entry _bn_sqr8x_internal
_bn_sqr8x_internal:
.alt_entry __bn_sqr8x_internal
__bn_sqr8x_internal:

_CET_ENDBR
// #############################################################
// Squaring part:
// 
// a) multiply-n-add everything but a[i]*a[i];
// b) shift result of a) by 1 to the left and accumulate
// a[i]*a[i] products;
// 
// #############################################################
// a[1]a[0]
// a[2]a[0]
// a[3]a[0]
// a[2]a[1]
// a[4]a[0]
// a[3]a[1]
// a[5]a[0]
// a[4]a[1]
// a[3]a[2]
// a[6]a[0]
// a[5]a[1]
// a[4]a[2]
// a[7]a[0]
// a[6]a[1]
// a[5]a[2]
// a[4]a[3]
// a[7]a[1]
// a[6]a[2]
// a[5]a[3]
// a[7]a[2]
// a[6]a[3]
// a[5]a[4]
// a[7]a[3]
// a[6]a[4]
// a[7]a[4]
// a[6]a[5]
// a[7]a[5]
// a[7]a[6]
// a[1]a[0]
// a[2]a[0]
// a[3]a[0]
// a[4]a[0]
// a[5]a[0]
// a[6]a[0]
// a[7]a[0]
// a[2]a[1]
// a[3]a[1]
// a[4]a[1]
// a[5]a[1]
// a[6]a[1]
// a[7]a[1]
// a[3]a[2]
// a[4]a[2]
// a[5]a[2]
// a[6]a[2]
// a[7]a[2]
// a[4]a[3]
// a[5]a[3]
// a[6]a[3]
// a[7]a[3]
// a[5]a[4]
// a[6]a[4]
// a[7]a[4]
// a[6]a[5]
// a[7]a[5]
// a[7]a[6]
// a[0]a[0]
// a[1]a[1]
// a[2]a[2]
// a[3]a[3]
// a[4]a[4]
// a[5]a[5]
// a[6]a[6]
// a[7]a[7]

	leaq	32(%r10),%rbp  // %rbp=-(%r9-32)
	leaq	(%rsi,%r9,1),%rsi  // end of a[] buffer, (%rsi,%rbp)=&ap[2]

	movq	%r9,%rcx  // %rcx=%r9

// comments apply to %r9==8 case
	movq	-32(%rsi,%rbp,1),%r14  // a[0]
	leaq	48+8(%rsp,%r9,2),%rdi  // end of tp[] buffer, &tp[2*%r9]
	movq	-24(%rsi,%rbp,1),%rax  // a[1]
	leaq	-32(%rdi,%rbp,1),%rdi  // end of tp[] window, &tp[2*%r9-"%rbp"]
	movq	-16(%rsi,%rbp,1),%rbx  // a[2]
	movq	%rax,%r15

	mulq	%r14  // a[1]*a[0]
	movq	%rax,%r10  // a[1]*a[0]
	movq	%rbx,%rax  // a[2]
	movq	%rdx,%r11
	movq	%r10,-24(%rdi,%rbp,1)  // t[1]

	mulq	%r14  // a[2]*a[0]
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	$0,%rdx
	movq	%r11,-16(%rdi,%rbp,1)  // t[2]
	movq	%rdx,%r10


	movq	-8(%rsi,%rbp,1),%rbx  // a[3]
	mulq	%r15  // a[2]*a[1]
	movq	%rax,%r12  // a[2]*a[1]+t[3]
	movq	%rbx,%rax
	movq	%rdx,%r13

	leaq	(%rbp),%rcx
	mulq	%r14  // a[3]*a[0]
	addq	%rax,%r10  // a[3]*a[0]+a[2]*a[1]+t[3]
	movq	%rbx,%rax
	movq	%rdx,%r11
	adcq	$0,%r11
	addq	%r12,%r10
	adcq	$0,%r11
	movq	%r10,-8(%rdi,%rcx,1)  // t[3]
	jmp	L$sqr4x_1st

.p2align	5
L$sqr4x_1st:
	movq	(%rsi,%rcx,1),%rbx  // a[4]
	mulq	%r15  // a[3]*a[1]
	addq	%rax,%r13  // a[3]*a[1]+t[4]
	movq	%rbx,%rax
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%r14  // a[4]*a[0]
	addq	%rax,%r11  // a[4]*a[0]+a[3]*a[1]+t[4]
	movq	%rbx,%rax  // a[3]
	movq	8(%rsi,%rcx,1),%rbx  // a[5]
	movq	%rdx,%r10
	adcq	$0,%r10
	addq	%r13,%r11
	adcq	$0,%r10


	mulq	%r15  // a[4]*a[3]
	addq	%rax,%r12  // a[4]*a[3]+t[5]
	movq	%rbx,%rax
	movq	%r11,(%rdi,%rcx,1)  // t[4]
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%r14  // a[5]*a[2]
	addq	%rax,%r10  // a[5]*a[2]+a[4]*a[3]+t[5]
	movq	%rbx,%rax
	movq	16(%rsi,%rcx,1),%rbx  // a[6]
	movq	%rdx,%r11
	adcq	$0,%r11
	addq	%r12,%r10
	adcq	$0,%r11

	mulq	%r15  // a[5]*a[3]
	addq	%rax,%r13  // a[5]*a[3]+t[6]
	movq	%rbx,%rax
	movq	%r10,8(%rdi,%rcx,1)  // t[5]
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%r14  // a[6]*a[2]
	addq	%rax,%r11  // a[6]*a[2]+a[5]*a[3]+t[6]
	movq	%rbx,%rax  // a[3]
	movq	24(%rsi,%rcx,1),%rbx  // a[7]
	movq	%rdx,%r10
	adcq	$0,%r10
	addq	%r13,%r11
	adcq	$0,%r10


	mulq	%r15  // a[6]*a[5]
	addq	%rax,%r12  // a[6]*a[5]+t[7]
	movq	%rbx,%rax
	movq	%r11,16(%rdi,%rcx,1)  // t[6]
	movq	%rdx,%r13
	adcq	$0,%r13
	leaq	32(%rcx),%rcx

	mulq	%r14  // a[7]*a[4]
	addq	%rax,%r10  // a[7]*a[4]+a[6]*a[5]+t[6]
	movq	%rbx,%rax
	movq	%rdx,%r11
	adcq	$0,%r11
	addq	%r12,%r10
	adcq	$0,%r11
	movq	%r10,-8(%rdi,%rcx,1)  // t[7]

	cmpq	$0,%rcx
	jne	L$sqr4x_1st

	mulq	%r15  // a[7]*a[5]
	addq	%rax,%r13
	leaq	16(%rbp),%rbp
	adcq	$0,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx

	movq	%r13,(%rdi)  // t[8]
	movq	%rdx,%r12
	movq	%rdx,8(%rdi)  // t[9]
	jmp	L$sqr4x_outer

.p2align	5
L$sqr4x_outer:  // comments apply to %r9==6 case
	movq	-32(%rsi,%rbp,1),%r14  // a[0]
	leaq	48+8(%rsp,%r9,2),%rdi  // end of tp[] buffer, &tp[2*%r9]
	movq	-24(%rsi,%rbp,1),%rax  // a[1]
	leaq	-32(%rdi,%rbp,1),%rdi  // end of tp[] window, &tp[2*%r9-"%rbp"]
	movq	-16(%rsi,%rbp,1),%rbx  // a[2]
	movq	%rax,%r15

	mulq	%r14  // a[1]*a[0]
	movq	-24(%rdi,%rbp,1),%r10  // t[1]
	addq	%rax,%r10  // a[1]*a[0]+t[1]
	movq	%rbx,%rax  // a[2]
	adcq	$0,%rdx
	movq	%r10,-24(%rdi,%rbp,1)  // t[1]
	movq	%rdx,%r11

	mulq	%r14  // a[2]*a[0]
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	$0,%rdx
	addq	-16(%rdi,%rbp,1),%r11  // a[2]*a[0]+t[2]
	movq	%rdx,%r10
	adcq	$0,%r10
	movq	%r11,-16(%rdi,%rbp,1)  // t[2]

	xorq	%r12,%r12

	movq	-8(%rsi,%rbp,1),%rbx  // a[3]
	mulq	%r15  // a[2]*a[1]
	addq	%rax,%r12  // a[2]*a[1]+t[3]
	movq	%rbx,%rax
	adcq	$0,%rdx
	addq	-8(%rdi,%rbp,1),%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%r14  // a[3]*a[0]
	addq	%rax,%r10  // a[3]*a[0]+a[2]*a[1]+t[3]
	movq	%rbx,%rax
	adcq	$0,%rdx
	addq	%r12,%r10
	movq	%rdx,%r11
	adcq	$0,%r11
	movq	%r10,-8(%rdi,%rbp,1)  // t[3]

	leaq	(%rbp),%rcx
	jmp	L$sqr4x_inner

.p2align	5
L$sqr4x_inner:
	movq	(%rsi,%rcx,1),%rbx  // a[4]
	mulq	%r15  // a[3]*a[1]
	addq	%rax,%r13  // a[3]*a[1]+t[4]
	movq	%rbx,%rax
	movq	%rdx,%r12
	adcq	$0,%r12
	addq	(%rdi,%rcx,1),%r13
	adcq	$0,%r12

.byte	0x67
	mulq	%r14  // a[4]*a[0]
	addq	%rax,%r11  // a[4]*a[0]+a[3]*a[1]+t[4]
	movq	%rbx,%rax  // a[3]
	movq	8(%rsi,%rcx,1),%rbx  // a[5]
	movq	%rdx,%r10
	adcq	$0,%r10
	addq	%r13,%r11
	adcq	$0,%r10

	mulq	%r15  // a[4]*a[3]
	addq	%rax,%r12  // a[4]*a[3]+t[5]
	movq	%r11,(%rdi,%rcx,1)  // t[4]
	movq	%rbx,%rax
	movq	%rdx,%r13
	adcq	$0,%r13
	addq	8(%rdi,%rcx,1),%r12
	leaq	16(%rcx),%rcx  // j++
	adcq	$0,%r13

	mulq	%r14  // a[5]*a[2]
	addq	%rax,%r10  // a[5]*a[2]+a[4]*a[3]+t[5]
	movq	%rbx,%rax
	adcq	$0,%rdx
	addq	%r12,%r10
	movq	%rdx,%r11
	adcq	$0,%r11
	movq	%r10,-8(%rdi,%rcx,1)  // t[5], "preloaded t[1]" below

	cmpq	$0,%rcx
	jne	L$sqr4x_inner

.byte	0x67
	mulq	%r15  // a[5]*a[3]
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx

	movq	%r13,(%rdi)  // t[6], "preloaded t[2]" below
	movq	%rdx,%r12
	movq	%rdx,8(%rdi)  // t[7], "preloaded t[3]" below

	addq	$16,%rbp
	jnz	L$sqr4x_outer

// comments apply to %r9==4 case
	movq	-32(%rsi),%r14  // a[0]
	leaq	48+8(%rsp,%r9,2),%rdi  // end of tp[] buffer, &tp[2*%r9]
	movq	-24(%rsi),%rax  // a[1]
	leaq	-32(%rdi,%rbp,1),%rdi  // end of tp[] window, &tp[2*%r9-"%rbp"]
	movq	-16(%rsi),%rbx  // a[2]
	movq	%rax,%r15

	mulq	%r14  // a[1]*a[0]
	addq	%rax,%r10  // a[1]*a[0]+t[1], preloaded t[1]
	movq	%rbx,%rax  // a[2]
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%r14  // a[2]*a[0]
	addq	%rax,%r11
	movq	%rbx,%rax
	movq	%r10,-24(%rdi)  // t[1]
	movq	%rdx,%r10
	adcq	$0,%r10
	addq	%r13,%r11  // a[2]*a[0]+t[2], preloaded t[2]
	movq	-8(%rsi),%rbx  // a[3]
	adcq	$0,%r10

	mulq	%r15  // a[2]*a[1]
	addq	%rax,%r12  // a[2]*a[1]+t[3], preloaded t[3]
	movq	%rbx,%rax
	movq	%r11,-16(%rdi)  // t[2]
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%r14  // a[3]*a[0]
	addq	%rax,%r10  // a[3]*a[0]+a[2]*a[1]+t[3]
	movq	%rbx,%rax
	movq	%rdx,%r11
	adcq	$0,%r11
	addq	%r12,%r10
	adcq	$0,%r11
	movq	%r10,-8(%rdi)  // t[3]

	mulq	%r15  // a[3]*a[1]
	addq	%rax,%r13
	movq	-16(%rsi),%rax  // a[2]
	adcq	$0,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx

	movq	%r13,(%rdi)  // t[4]
	movq	%rdx,%r12
	movq	%rdx,8(%rdi)  // t[5]

	mulq	%rbx  // a[2]*a[3]
	addq	$16,%rbp
	xorq	%r14,%r14
	subq	%r9,%rbp  // %rbp=16-%r9
	xorq	%r15,%r15

	addq	%r12,%rax  // t[5]
	adcq	$0,%rdx
	movq	%rax,8(%rdi)  // t[5]
	movq	%rdx,16(%rdi)  // t[6]
	movq	%r15,24(%rdi)  // t[7]

	movq	-16(%rsi,%rbp,1),%rax  // a[0]
	leaq	48+8(%rsp),%rdi
	xorq	%r10,%r10  // t[0]
	movq	8(%rdi),%r11  // t[1]

	leaq	(%r14,%r10,2),%r12  // t[2*i]<<1 | shift
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r13  // | t[2*i]>>63
	movq	16(%rdi),%r10  // t[2*i+2]	# prefetch
	movq	%r11,%r14  // shift=t[2*i+1]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	movq	24(%rdi),%r11  // t[2*i+2+1]	# prefetch
	adcq	%rax,%r12
	movq	-8(%rsi,%rbp,1),%rax  // a[i+1]	# prefetch
	movq	%r12,(%rdi)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx  // t[2*i]<<1 | shift
	movq	%r13,8(%rdi)
	sbbq	%r15,%r15  // mov cf,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r8  // | t[2*i]>>63
	movq	32(%rdi),%r10  // t[2*i+2]	# prefetch
	movq	%r11,%r14  // shift=t[2*i+1]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	movq	40(%rdi),%r11  // t[2*i+2+1]	# prefetch
	adcq	%rax,%rbx
	movq	0(%rsi,%rbp,1),%rax  // a[i+1]	# prefetch
	movq	%rbx,16(%rdi)
	adcq	%rdx,%r8
	leaq	16(%rbp),%rbp
	movq	%r8,24(%rdi)
	sbbq	%r15,%r15  // mov cf,%r15
	leaq	64(%rdi),%rdi
	jmp	L$sqr4x_shift_n_add

.p2align	5
L$sqr4x_shift_n_add:
	leaq	(%r14,%r10,2),%r12  // t[2*i]<<1 | shift
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r13  // | t[2*i]>>63
	movq	-16(%rdi),%r10  // t[2*i+2]	# prefetch
	movq	%r11,%r14  // shift=t[2*i+1]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	movq	-8(%rdi),%r11  // t[2*i+2+1]	# prefetch
	adcq	%rax,%r12
	movq	-8(%rsi,%rbp,1),%rax  // a[i+1]	# prefetch
	movq	%r12,-32(%rdi)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx  // t[2*i]<<1 | shift
	movq	%r13,-24(%rdi)
	sbbq	%r15,%r15  // mov cf,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r8  // | t[2*i]>>63
	movq	0(%rdi),%r10  // t[2*i+2]	# prefetch
	movq	%r11,%r14  // shift=t[2*i+1]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	movq	8(%rdi),%r11  // t[2*i+2+1]	# prefetch
	adcq	%rax,%rbx
	movq	0(%rsi,%rbp,1),%rax  // a[i+1]	# prefetch
	movq	%rbx,-16(%rdi)
	adcq	%rdx,%r8

	leaq	(%r14,%r10,2),%r12  // t[2*i]<<1 | shift
	movq	%r8,-8(%rdi)
	sbbq	%r15,%r15  // mov cf,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r13  // | t[2*i]>>63
	movq	16(%rdi),%r10  // t[2*i+2]	# prefetch
	movq	%r11,%r14  // shift=t[2*i+1]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	movq	24(%rdi),%r11  // t[2*i+2+1]	# prefetch
	adcq	%rax,%r12
	movq	8(%rsi,%rbp,1),%rax  // a[i+1]	# prefetch
	movq	%r12,0(%rdi)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx  // t[2*i]<<1 | shift
	movq	%r13,8(%rdi)
	sbbq	%r15,%r15  // mov cf,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r8  // | t[2*i]>>63
	movq	32(%rdi),%r10  // t[2*i+2]	# prefetch
	movq	%r11,%r14  // shift=t[2*i+1]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	movq	40(%rdi),%r11  // t[2*i+2+1]	# prefetch
	adcq	%rax,%rbx
	movq	16(%rsi,%rbp,1),%rax  // a[i+1]	# prefetch
	movq	%rbx,16(%rdi)
	adcq	%rdx,%r8
	movq	%r8,24(%rdi)
	sbbq	%r15,%r15  // mov cf,%r15
	leaq	64(%rdi),%rdi
	addq	$32,%rbp
	jnz	L$sqr4x_shift_n_add

	leaq	(%r14,%r10,2),%r12  // t[2*i]<<1 | shift
.byte	0x67
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r13  // | t[2*i]>>63
	movq	-16(%rdi),%r10  // t[2*i+2]	# prefetch
	movq	%r11,%r14  // shift=t[2*i+1]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	movq	-8(%rdi),%r11  // t[2*i+2+1]	# prefetch
	adcq	%rax,%r12
	movq	-8(%rsi),%rax  // a[i+1]	# prefetch
	movq	%r12,-32(%rdi)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx  // t[2*i]<<1|shift
	movq	%r13,-24(%rdi)
	sbbq	%r15,%r15  // mov cf,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8  // t[2*i+1]<<1 |
	shrq	$63,%r11
	orq	%r10,%r8  // | t[2*i]>>63
	mulq	%rax  // a[i]*a[i]
	negq	%r15  // mov %r15,cf
	adcq	%rax,%rbx
	adcq	%rdx,%r8
	movq	%rbx,-16(%rdi)
	movq	%r8,-8(%rdi)
	movq	%xmm2,%rbp
.alt_entry __bn_sqr8x_reduction
__bn_sqr8x_reduction:
	xorq	%rax,%rax
	leaq	(%r9,%rbp,1),%rcx  // end of n[]
	leaq	48+8(%rsp,%r9,2),%rdx  // end of t[] buffer
	movq	%rcx,0+8(%rsp)
	leaq	48+8(%rsp,%r9,1),%rdi  // end of initial t[] window
	movq	%rdx,8+8(%rsp)
	negq	%r9
	jmp	L$8x_reduction_loop

.p2align	5
L$8x_reduction_loop:
	leaq	(%rdi,%r9,1),%rdi  // start of current t[] window
.byte	0x66
	movq	0(%rdi),%rbx
	movq	8(%rdi),%r9
	movq	16(%rdi),%r10
	movq	24(%rdi),%r11
	movq	32(%rdi),%r12
	movq	40(%rdi),%r13
	movq	48(%rdi),%r14
	movq	56(%rdi),%r15
	movq	%rax,(%rdx)  // store top-most carry bit
	leaq	64(%rdi),%rdi

.byte	0x67
	movq	%rbx,%r8
	imulq	32+8(%rsp),%rbx  // n0*a[0]
	movq	0(%rbp),%rax  // n[0]
	movl	$8,%ecx
	jmp	L$8x_reduce

.p2align	5
L$8x_reduce:
	mulq	%rbx
	movq	8(%rbp),%rax  // n[1]
	negq	%r8
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rbp),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	movq	%rbx,48-8+8(%rsp,%rcx,8)  // put aside n0*a[i]
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rbp),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	32+8(%rsp),%rsi  // pull n0, borrow %rsi
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rbp),%rax
	adcq	$0,%rdx
	imulq	%r8,%rsi  // modulo-scheduled
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rbp),%rax
	adcq	$0,%rdx
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rbp),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rbp),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	movq	%rsi,%rbx  // n0*a[i]
	addq	%rax,%r15
	movq	0(%rbp),%rax  // n[0]
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	%rdx,%r15
	adcq	$0,%r15

	decl	%ecx
	jnz	L$8x_reduce

	leaq	64(%rbp),%rbp
	xorq	%rax,%rax
	movq	8+8(%rsp),%rdx  // pull end of t[]
	cmpq	0+8(%rsp),%rbp  // end of n[]?
	jae	L$8x_no_tail

.byte	0x66
	addq	0(%rdi),%r8
	adcq	8(%rdi),%r9
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15
	sbbq	%rsi,%rsi  // top carry

	movq	48+56+8(%rsp),%rbx  // pull n0*a[0]
	movl	$8,%ecx
	movq	0(%rbp),%rax
	jmp	L$8x_tail

.p2align	5
L$8x_tail:
	mulq	%rbx
	addq	%rax,%r8
	movq	8(%rbp),%rax
	movq	%r8,(%rdi)  // save result
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rbp),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	leaq	8(%rdi),%rdi  // %rdi++
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rbp),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rbp),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rbp),%rax
	adcq	$0,%rdx
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rbp),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rbp),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	movq	48-16+8(%rsp,%rcx,8),%rbx  // pull n0*a[i]
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	0(%rbp),%rax  // pull n[0]
	movq	%rdx,%r15
	adcq	$0,%r15

	decl	%ecx
	jnz	L$8x_tail

	leaq	64(%rbp),%rbp
	movq	8+8(%rsp),%rdx  // pull end of t[]
	cmpq	0+8(%rsp),%rbp  // end of n[]?
	jae	L$8x_tail_done  // break out of loop

	movq	48+56+8(%rsp),%rbx  // pull n0*a[0]
	negq	%rsi
	movq	0(%rbp),%rax  // pull n[0]
	adcq	0(%rdi),%r8
	adcq	8(%rdi),%r9
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15
	sbbq	%rsi,%rsi  // top carry

	movl	$8,%ecx
	jmp	L$8x_tail

.p2align	5
L$8x_tail_done:
	xorq	%rax,%rax
	addq	(%rdx),%r8  // can this overflow?
	adcq	$0,%r9
	adcq	$0,%r10
	adcq	$0,%r11
	adcq	$0,%r12
	adcq	$0,%r13
	adcq	$0,%r14
	adcq	$0,%r15
	adcq	$0,%rax

	negq	%rsi
L$8x_no_tail:
	adcq	0(%rdi),%r8
	adcq	8(%rdi),%r9
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15
	adcq	$0,%rax  // top-most carry
	movq	-8(%rbp),%rcx  // np[num-1]
	xorq	%rsi,%rsi

	movq	%xmm2,%rbp  // restore %rbp

	movq	%r8,0(%rdi)  // store top 512 bits
	movq	%r9,8(%rdi)
	movq	%xmm3,%r9  // %r9 is %r9, can't be moved upwards
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)
	leaq	64(%rdi),%rdi

	cmpq	%rdx,%rdi  // end of t[]?
	jb	L$8x_reduction_loop
	ret



.p2align	5
.alt_entry __bn_post4x_internal
__bn_post4x_internal:

	movq	0(%rbp),%r12
	leaq	(%rdi,%r9,1),%rbx  // %rdi was %rbx above
	movq	%r9,%rcx
	movq	%xmm1,%rdi  // restore %rdi
	negq	%rax
	movq	%xmm1,%rsi  // prepare for back-to-back call
	sarq	$3+2,%rcx
	decq	%r12  // so that after 'not' we get -n[0]
	xorq	%r10,%r10
	movq	8(%rbp),%r13
	movq	16(%rbp),%r14
	movq	24(%rbp),%r15
	jmp	L$sqr4x_sub_entry

.p2align	4
L$sqr4x_sub:
	movq	0(%rbp),%r12
	movq	8(%rbp),%r13
	movq	16(%rbp),%r14
	movq	24(%rbp),%r15
L$sqr4x_sub_entry:
	leaq	32(%rbp),%rbp
	notq	%r12
	notq	%r13
	notq	%r14
	notq	%r15
	andq	%rax,%r12
	andq	%rax,%r13
	andq	%rax,%r14
	andq	%rax,%r15

	negq	%r10  // mov %r10,%cf
	adcq	0(%rbx),%r12
	adcq	8(%rbx),%r13
	adcq	16(%rbx),%r14
	adcq	24(%rbx),%r15
	movq	%r12,0(%rdi)
	leaq	32(%rbx),%rbx
	movq	%r13,8(%rdi)
	sbbq	%r10,%r10  // mov %cf,%r10
	movq	%r14,16(%rdi)
	movq	%r15,24(%rdi)
	leaq	32(%rdi),%rdi

	incq	%rcx  // pass %cf
	jnz	L$sqr4x_sub

	movq	%r9,%r10  // prepare for back-to-back call
	negq	%r9  // restore %r9
	ret


.globl	_bn_mulx4x_mont_gather5
.private_extern _bn_mulx4x_mont_gather5

.p2align	5
.alt_entry _bn_mulx4x_mont_gather5
_bn_mulx4x_mont_gather5:

_CET_ENDBR
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$mulx4x_prologue:

// num is declared as an int, a 32-bit parameter, so the upper half is
// undefined. It is important that this write to %r9, which zeros the
// upper half, predates the first access.
	shll	$3,%r9d  // convert %r9 to bytes
	leaq	(%r9,%r9,2),%r10  // 3*%r9 in bytes
	negq	%r9  // -%r9
	movq	(%r8),%r8  // *n0

// #############################################################
// Ensure that stack frame doesn't alias with +3*%r9
// modulo 4096, which covers ret[num], am[num] and n[num]
// (see bn_exp.c). This is done to allow memory disambiguation
// logic do its magic. [Extra [num] is allocated in order
// to align with bn_power5's frame, which is cleansed after
// completing exponentiation. Extra 256 bytes is for power mask
// calculated from 7th argument, the index.]
// 
	leaq	-320(%rsp,%r9,2),%r11
	movq	%rsp,%rbp
	subq	%rdi,%r11
	andq	$4095,%r11
	cmpq	%r11,%r10
	jb	L$mulx4xsp_alt
	subq	%r11,%rbp  // align with
	leaq	-320(%rbp,%r9,2),%rbp  // future alloca(frame+2*%r9*8+256)
	jmp	L$mulx4xsp_done

L$mulx4xsp_alt:
	leaq	4096-320(,%r9,2),%r10
	leaq	-320(%rbp,%r9,2),%rbp  // future alloca(frame+2*%r9*8+256)
	subq	%r10,%r11
	movq	$0,%r10
	cmovcq	%r10,%r11
	subq	%r11,%rbp
L$mulx4xsp_done:
	andq	$-64,%rbp  // ensure alignment
	movq	%rsp,%r11
	subq	%rbp,%r11
	andq	$-4096,%r11
	leaq	(%r11,%rbp,1),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$mulx4x_page_walk
	jmp	L$mulx4x_page_walk_done

L$mulx4x_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$mulx4x_page_walk
L$mulx4x_page_walk_done:

// #############################################################
// Stack layout
// +0	-num
// +8	off-loaded &b[i]
// +16	end of b[num]
// +24	inner counter
// +32	saved n0
// +40	saved %rsp
// +48
// +56	saved rp
// +64	tmp[num+1]
// 
	movq	%r8,32(%rsp)  // save *n0
	movq	%rax,40(%rsp)  // save original %rsp

L$mulx4x_body:
	call	mulx4x_internal

	movq	40(%rsp),%rsi  // restore %rsp

	movq	$1,%rax

	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$mulx4x_epilogue:
	ret




.p2align	5
.alt_entry mulx4x_internal
mulx4x_internal:

	movq	%r9,8(%rsp)  // save -%r9 (it was in bytes)
	movq	%r9,%r10
	negq	%r9  // restore %r9
	shlq	$5,%r9
	negq	%r10  // restore %r9
	leaq	128(%rdx,%r9,1),%r13  // end of powers table (+size optimization)
	shrq	$5+5,%r9
	movd	8(%rax),%xmm5  // load 7th argument
	subq	$1,%r9
	leaq	L$inc(%rip),%rax
	movq	%r13,16+8(%rsp)  // end of b[num]
	movq	%r9,24+8(%rsp)  // inner counter
	movq	%rdi,56+8(%rsp)  // save %rdi
	movdqa	0(%rax),%xmm0  // 00000001000000010000000000000000
	movdqa	16(%rax),%xmm1  // 00000002000000020000000200000002
	leaq	88-112(%rsp,%r10,1),%r10  // place the mask after tp[num+1] (+ICache optimization)
	leaq	128(%rdx),%rdi  // size optimization

	pshufd	$0,%xmm5,%xmm5  // broadcast index
	movdqa	%xmm1,%xmm4
.byte	0x67
	movdqa	%xmm1,%xmm2
.byte	0x67
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0  // compare to 1,0
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,112(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,128(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,144(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,160(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,176(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,192(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,208(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,224(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,240(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,256(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,272(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,288(%r10)
	movdqa	%xmm4,%xmm3
.byte	0x67
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,304(%r10)

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,320(%r10)

	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,336(%r10)

	pand	64(%rdi),%xmm0  // while it's still in register
	pand	80(%rdi),%xmm1
	pand	96(%rdi),%xmm2
	movdqa	%xmm3,352(%r10)
	pand	112(%rdi),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-128(%rdi),%xmm4
	movdqa	-112(%rdi),%xmm5
	movdqa	-96(%rdi),%xmm2
	pand	112(%r10),%xmm4
	movdqa	-80(%rdi),%xmm3
	pand	128(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	144(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	160(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-64(%rdi),%xmm4
	movdqa	-48(%rdi),%xmm5
	movdqa	-32(%rdi),%xmm2
	pand	176(%r10),%xmm4
	movdqa	-16(%rdi),%xmm3
	pand	192(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	208(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	224(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	0(%rdi),%xmm4
	movdqa	16(%rdi),%xmm5
	movdqa	32(%rdi),%xmm2
	pand	240(%r10),%xmm4
	movdqa	48(%rdi),%xmm3
	pand	256(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	272(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	288(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	pxor	%xmm1,%xmm0
// Combine the upper and lower halves of %xmm0.
	pshufd	$0x4e,%xmm0,%xmm1  // Swap upper and lower halves.
	por	%xmm1,%xmm0
	leaq	256(%rdi),%rdi
	movq	%xmm0,%rdx  // bp[0]
	leaq	64+32+8(%rsp),%rbx

	movq	%rdx,%r9
	mulxq	0(%rsi),%r8,%rax  // a[0]*b[0]
	mulxq	8(%rsi),%r11,%r12  // a[1]*b[0]
	addq	%rax,%r11
	mulxq	16(%rsi),%rax,%r13  // ...
	adcq	%rax,%r12
	adcq	$0,%r13
	mulxq	24(%rsi),%rax,%r14

	movq	%r8,%r15
	imulq	32+8(%rsp),%r8  // "t[0]"*n0
	xorq	%rbp,%rbp  // cf=0, of=0
	movq	%r8,%rdx

	movq	%rdi,8+8(%rsp)  // off-load &b[i]

	leaq	32(%rsi),%rsi
	adcxq	%rax,%r13
	adcxq	%rbp,%r14  // cf=0

	mulxq	0(%rcx),%rax,%r10
	adcxq	%rax,%r15  // discarded
	adoxq	%r11,%r10
	mulxq	8(%rcx),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11
	mulxq	16(%rcx),%rax,%r12
	movq	24+8(%rsp),%rdi  // counter value
	movq	%r10,-32(%rbx)
	adcxq	%rax,%r11
	adoxq	%r13,%r12
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r11,-24(%rbx)
	adcxq	%rax,%r12
	adoxq	%rbp,%r15  // of=0
	leaq	32(%rcx),%rcx
	movq	%r12,-16(%rbx)
	jmp	L$mulx4x_1st

.p2align	5
L$mulx4x_1st:
	adcxq	%rbp,%r15  // cf=0, modulo-scheduled
	mulxq	0(%rsi),%r10,%rax  // a[4]*b[0]
	adcxq	%r14,%r10
	mulxq	8(%rsi),%r11,%r14  // a[5]*b[0]
	adcxq	%rax,%r11
	mulxq	16(%rsi),%r12,%rax  // ...
	adcxq	%r14,%r12
	mulxq	24(%rsi),%r13,%r14
.byte	0x67,0x67
	movq	%r8,%rdx
	adcxq	%rax,%r13
	adcxq	%rbp,%r14  // cf=0
	leaq	32(%rsi),%rsi
	leaq	32(%rbx),%rbx

	adoxq	%r15,%r10
	mulxq	0(%rcx),%rax,%r15
	adcxq	%rax,%r10
	adoxq	%r15,%r11
	mulxq	8(%rcx),%rax,%r15
	adcxq	%rax,%r11
	adoxq	%r15,%r12
	mulxq	16(%rcx),%rax,%r15
	movq	%r10,-40(%rbx)
	adcxq	%rax,%r12
	movq	%r11,-32(%rbx)
	adoxq	%r15,%r13
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r12,-24(%rbx)
	adcxq	%rax,%r13
	adoxq	%rbp,%r15
	leaq	32(%rcx),%rcx
	movq	%r13,-16(%rbx)

	decq	%rdi  // of=0, pass cf
	jnz	L$mulx4x_1st

	movq	8(%rsp),%rax  // load -num
	adcq	%rbp,%r15  // modulo-scheduled
	leaq	(%rsi,%rax,1),%rsi  // rewind %rsi
	addq	%r15,%r14
	movq	8+8(%rsp),%rdi  // re-load &b[i]
	adcq	%rbp,%rbp  // top-most carry
	movq	%r14,-8(%rbx)
	jmp	L$mulx4x_outer

.p2align	5
L$mulx4x_outer:
	leaq	16-256(%rbx),%r10  // where 256-byte mask is (+density control)
	pxor	%xmm4,%xmm4
.byte	0x67,0x67
	pxor	%xmm5,%xmm5
	movdqa	-128(%rdi),%xmm0
	movdqa	-112(%rdi),%xmm1
	movdqa	-96(%rdi),%xmm2
	pand	256(%r10),%xmm0
	movdqa	-80(%rdi),%xmm3
	pand	272(%r10),%xmm1
	por	%xmm0,%xmm4
	pand	288(%r10),%xmm2
	por	%xmm1,%xmm5
	pand	304(%r10),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%rdi),%xmm0
	movdqa	-48(%rdi),%xmm1
	movdqa	-32(%rdi),%xmm2
	pand	320(%r10),%xmm0
	movdqa	-16(%rdi),%xmm3
	pand	336(%r10),%xmm1
	por	%xmm0,%xmm4
	pand	352(%r10),%xmm2
	por	%xmm1,%xmm5
	pand	368(%r10),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%rdi),%xmm0
	movdqa	16(%rdi),%xmm1
	movdqa	32(%rdi),%xmm2
	pand	384(%r10),%xmm0
	movdqa	48(%rdi),%xmm3
	pand	400(%r10),%xmm1
	por	%xmm0,%xmm4
	pand	416(%r10),%xmm2
	por	%xmm1,%xmm5
	pand	432(%r10),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%rdi),%xmm0
	movdqa	80(%rdi),%xmm1
	movdqa	96(%rdi),%xmm2
	pand	448(%r10),%xmm0
	movdqa	112(%rdi),%xmm3
	pand	464(%r10),%xmm1
	por	%xmm0,%xmm4
	pand	480(%r10),%xmm2
	por	%xmm1,%xmm5
	pand	496(%r10),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
// Combine the upper and lower halves of %xmm4 as %xmm0.
	pshufd	$0x4e,%xmm4,%xmm0  // Swap upper and lower halves.
	por	%xmm4,%xmm0
	leaq	256(%rdi),%rdi
	movq	%xmm0,%rdx  // m0=bp[i]

	movq	%rbp,(%rbx)  // save top-most carry
	leaq	32(%rbx,%rax,1),%rbx  // rewind %rbx
	mulxq	0(%rsi),%r8,%r11  // a[0]*b[i]
	xorq	%rbp,%rbp  // cf=0, of=0
	movq	%rdx,%r9
	mulxq	8(%rsi),%r14,%r12  // a[1]*b[i]
	adoxq	-32(%rbx),%r8  // +t[0]
	adcxq	%r14,%r11
	mulxq	16(%rsi),%r15,%r13  // ...
	adoxq	-24(%rbx),%r11
	adcxq	%r15,%r12
	mulxq	24(%rsi),%rdx,%r14
	adoxq	-16(%rbx),%r12
	adcxq	%rdx,%r13
	leaq	(%rcx,%rax,1),%rcx  // rewind %rcx
	leaq	32(%rsi),%rsi
	adoxq	-8(%rbx),%r13
	adcxq	%rbp,%r14
	adoxq	%rbp,%r14

	movq	%r8,%r15
	imulq	32+8(%rsp),%r8  // "t[0]"*n0

	movq	%r8,%rdx
	xorq	%rbp,%rbp  // cf=0, of=0
	movq	%rdi,8+8(%rsp)  // off-load &b[i]

	mulxq	0(%rcx),%rax,%r10
	adcxq	%rax,%r15  // discarded
	adoxq	%r11,%r10
	mulxq	8(%rcx),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11
	mulxq	16(%rcx),%rax,%r12
	adcxq	%rax,%r11
	adoxq	%r13,%r12
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	24+8(%rsp),%rdi  // counter value
	movq	%r10,-32(%rbx)
	adcxq	%rax,%r12
	movq	%r11,-24(%rbx)
	adoxq	%rbp,%r15  // of=0
	movq	%r12,-16(%rbx)
	leaq	32(%rcx),%rcx
	jmp	L$mulx4x_inner

.p2align	5
L$mulx4x_inner:
	mulxq	0(%rsi),%r10,%rax  // a[4]*b[i]
	adcxq	%rbp,%r15  // cf=0, modulo-scheduled
	adoxq	%r14,%r10
	mulxq	8(%rsi),%r11,%r14  // a[5]*b[i]
	adcxq	0(%rbx),%r10
	adoxq	%rax,%r11
	mulxq	16(%rsi),%r12,%rax  // ...
	adcxq	8(%rbx),%r11
	adoxq	%r14,%r12
	mulxq	24(%rsi),%r13,%r14
	movq	%r8,%rdx
	adcxq	16(%rbx),%r12
	adoxq	%rax,%r13
	adcxq	24(%rbx),%r13
	adoxq	%rbp,%r14  // of=0
	leaq	32(%rsi),%rsi
	leaq	32(%rbx),%rbx
	adcxq	%rbp,%r14  // cf=0

	adoxq	%r15,%r10
	mulxq	0(%rcx),%rax,%r15
	adcxq	%rax,%r10
	adoxq	%r15,%r11
	mulxq	8(%rcx),%rax,%r15
	adcxq	%rax,%r11
	adoxq	%r15,%r12
	mulxq	16(%rcx),%rax,%r15
	movq	%r10,-40(%rbx)
	adcxq	%rax,%r12
	adoxq	%r15,%r13
	movq	%r11,-32(%rbx)
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	leaq	32(%rcx),%rcx
	movq	%r12,-24(%rbx)
	adcxq	%rax,%r13
	adoxq	%rbp,%r15
	movq	%r13,-16(%rbx)

	decq	%rdi  // of=0, pass cf
	jnz	L$mulx4x_inner

	movq	0+8(%rsp),%rax  // load -num
	adcq	%rbp,%r15  // modulo-scheduled
	subq	0(%rbx),%rdi  // pull top-most carry to %cf
	movq	8+8(%rsp),%rdi  // re-load &b[i]
	movq	16+8(%rsp),%r10
	adcq	%r15,%r14
	leaq	(%rsi,%rax,1),%rsi  // rewind %rsi
	adcq	%rbp,%rbp  // top-most carry
	movq	%r14,-8(%rbx)

	cmpq	%r10,%rdi
	jb	L$mulx4x_outer

	movq	-8(%rcx),%r10
	movq	%rbp,%r8
	movq	(%rcx,%rax,1),%r12
	leaq	(%rcx,%rax,1),%rbp  // rewind %rcx
	movq	%rax,%rcx
	leaq	(%rbx,%rax,1),%rdi  // rewind %rbx
	xorl	%eax,%eax
	xorq	%r15,%r15
	subq	%r14,%r10  // compare top-most words
	adcq	%r15,%r15
	orq	%r15,%r8
	sarq	$3+2,%rcx
	subq	%r8,%rax  // %rax=-%r8
	movq	56+8(%rsp),%rdx  // restore rp
	decq	%r12  // so that after 'not' we get -n[0]
	movq	8(%rbp),%r13
	xorq	%r8,%r8
	movq	16(%rbp),%r14
	movq	24(%rbp),%r15
	jmp	L$sqrx4x_sub_entry  // common post-condition


.globl	_bn_powerx5
.private_extern _bn_powerx5

.p2align	5
.alt_entry _bn_powerx5
_bn_powerx5:

_CET_ENDBR
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$powerx5_prologue:

// num is declared as an int, a 32-bit parameter, so the upper half is
// undefined. It is important that this write to %r9, which zeros the
// upper half, predates the first access.
	shll	$3,%r9d  // convert %r9 to bytes
	leaq	(%r9,%r9,2),%r10  // 3*%r9 in bytes
	negq	%r9
	movq	(%r8),%r8  // *n0

// #############################################################
// Ensure that stack frame doesn't alias with %rdi+3*%r9
// modulo 4096, which covers ret[num], am[num] and n[num]
// (see bn_exp.c). This is done to allow memory disambiguation
// logic do its magic. [Extra 256 bytes is for power mask
// calculated from 7th argument, the index.]
// 
	leaq	-320(%rsp,%r9,2),%r11
	movq	%rsp,%rbp
	subq	%rdi,%r11
	andq	$4095,%r11
	cmpq	%r11,%r10
	jb	L$pwrx_sp_alt
	subq	%r11,%rbp  // align with %rsi
	leaq	-320(%rbp,%r9,2),%rbp  // future alloca(frame+2*%r9*8+256)
	jmp	L$pwrx_sp_done

.p2align	5
L$pwrx_sp_alt:
	leaq	4096-320(,%r9,2),%r10
	leaq	-320(%rbp,%r9,2),%rbp  // alloca(frame+2*%r9*8+256)
	subq	%r10,%r11
	movq	$0,%r10
	cmovcq	%r10,%r11
	subq	%r11,%rbp
L$pwrx_sp_done:
	andq	$-64,%rbp
	movq	%rsp,%r11
	subq	%rbp,%r11
	andq	$-4096,%r11
	leaq	(%r11,%rbp,1),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$pwrx_page_walk
	jmp	L$pwrx_page_walk_done

L$pwrx_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$pwrx_page_walk
L$pwrx_page_walk_done:

	movq	%r9,%r10
	negq	%r9

// #############################################################
// Stack layout
// 
// +0	saved %r9, used in reduction section
// +8	&t[2*%r9], used in reduction section
// +16	intermediate carry bit
// +24	top-most carry bit, used in reduction section
// +32	saved *n0
// +40	saved %rsp
// +48	t[2*%r9]
// 
	pxor	%xmm0,%xmm0
	movq	%rdi,%xmm1  // save %rdi
	movq	%rcx,%xmm2  // save %rcx
	movq	%r10,%xmm3  // -%r9
	movq	%rdx,%xmm4
	movq	%r8,32(%rsp)
	movq	%rax,40(%rsp)  // save original %rsp

L$powerx5_body:

	call	__bn_sqrx8x_internal
	call	__bn_postx4x_internal
	call	__bn_sqrx8x_internal
	call	__bn_postx4x_internal
	call	__bn_sqrx8x_internal
	call	__bn_postx4x_internal
	call	__bn_sqrx8x_internal
	call	__bn_postx4x_internal
	call	__bn_sqrx8x_internal
	call	__bn_postx4x_internal

	movq	%r10,%r9  // -num
	movq	%rsi,%rdi
	movq	%xmm2,%rcx
	movq	%xmm4,%rdx
	movq	40(%rsp),%rax

	call	mulx4x_internal

	movq	40(%rsp),%rsi  // restore %rsp

	movq	$1,%rax

	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$powerx5_epilogue:
	ret



.globl	_bn_sqrx8x_internal
.private_extern _bn_sqrx8x_internal
.private_extern	_bn_sqrx8x_internal

.p2align	5
.alt_entry _bn_sqrx8x_internal
_bn_sqrx8x_internal:
.alt_entry __bn_sqrx8x_internal
__bn_sqrx8x_internal:

_CET_ENDBR
// #################################################################
// Squaring part:
// 
// a) multiply-n-add everything but a[i]*a[i];
// b) shift result of a) by 1 to the left and accumulate
// a[i]*a[i] products;
// 
// #################################################################
// a[7]a[7]a[6]a[6]a[5]a[5]a[4]a[4]a[3]a[3]a[2]a[2]a[1]a[1]a[0]a[0]
// a[1]a[0]
// a[2]a[0]
// a[3]a[0]
// a[2]a[1]
// a[3]a[1]
// a[3]a[2]
// 
// a[4]a[0]
// a[5]a[0]
// a[6]a[0]
// a[7]a[0]
// a[4]a[1]
// a[5]a[1]
// a[6]a[1]
// a[7]a[1]
// a[4]a[2]
// a[5]a[2]
// a[6]a[2]
// a[7]a[2]
// a[4]a[3]
// a[5]a[3]
// a[6]a[3]
// a[7]a[3]
// 
// a[5]a[4]
// a[6]a[4]
// a[7]a[4]
// a[6]a[5]
// a[7]a[5]
// a[7]a[6]
// a[7]a[7]a[6]a[6]a[5]a[5]a[4]a[4]a[3]a[3]a[2]a[2]a[1]a[1]a[0]a[0]
	leaq	48+8(%rsp),%rdi
	leaq	(%rsi,%r9,1),%rbp
	movq	%r9,0+8(%rsp)  // save %r9
	movq	%rbp,8+8(%rsp)  // save end of %rsi
	jmp	L$sqr8x_zero_start

.p2align	5
.byte	0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00
L$sqrx8x_zero:
.byte	0x3e
	movdqa	%xmm0,0(%rdi)
	movdqa	%xmm0,16(%rdi)
	movdqa	%xmm0,32(%rdi)
	movdqa	%xmm0,48(%rdi)
L$sqr8x_zero_start:  // aligned at 32
	movdqa	%xmm0,64(%rdi)
	movdqa	%xmm0,80(%rdi)
	movdqa	%xmm0,96(%rdi)
	movdqa	%xmm0,112(%rdi)
	leaq	128(%rdi),%rdi
	subq	$64,%r9
	jnz	L$sqrx8x_zero

	movq	0(%rsi),%rdx  // a[0], modulo-scheduled
// xor	%r9,%r9			# t[1], ex-%r9, zero already
	xorq	%r10,%r10
	xorq	%r11,%r11
	xorq	%r12,%r12
	xorq	%r13,%r13
	xorq	%r14,%r14
	xorq	%r15,%r15
	leaq	48+8(%rsp),%rdi
	xorq	%rbp,%rbp  // cf=0, cf=0
	jmp	L$sqrx8x_outer_loop

.p2align	5
L$sqrx8x_outer_loop:
	mulxq	8(%rsi),%r8,%rax  // a[1]*a[0]
	adcxq	%r9,%r8  // a[1]*a[0]+=t[1]
	adoxq	%rax,%r10
	mulxq	16(%rsi),%r9,%rax  // a[2]*a[0]
	adcxq	%r10,%r9
	adoxq	%rax,%r11
.byte	0xc4,0xe2,0xab,0xf6,0x86,0x18,0x00,0x00,0x00  // mulx	3*8(%rsi),%r10,%rax	# ...
	adcxq	%r11,%r10
	adoxq	%rax,%r12
.byte	0xc4,0xe2,0xa3,0xf6,0x86,0x20,0x00,0x00,0x00  // mulx	4*8(%rsi),%r11,%rax
	adcxq	%r12,%r11
	adoxq	%rax,%r13
	mulxq	40(%rsi),%r12,%rax
	adcxq	%r13,%r12
	adoxq	%rax,%r14
	mulxq	48(%rsi),%r13,%rax
	adcxq	%r14,%r13
	adoxq	%r15,%rax
	mulxq	56(%rsi),%r14,%r15
	movq	8(%rsi),%rdx  // a[1]
	adcxq	%rax,%r14
	adoxq	%rbp,%r15
	adcq	64(%rdi),%r15
	movq	%r8,8(%rdi)  // t[1]
	movq	%r9,16(%rdi)  // t[2]
	sbbq	%rcx,%rcx  // mov %cf,%rcx
	xorq	%rbp,%rbp  // cf=0, of=0


	mulxq	16(%rsi),%r8,%rbx  // a[2]*a[1]
	mulxq	24(%rsi),%r9,%rax  // a[3]*a[1]
	adcxq	%r10,%r8
	adoxq	%rbx,%r9
	mulxq	32(%rsi),%r10,%rbx  // ...
	adcxq	%r11,%r9
	adoxq	%rax,%r10
.byte	0xc4,0xe2,0xa3,0xf6,0x86,0x28,0x00,0x00,0x00  // mulx	5*8(%rsi),%r11,%rax
	adcxq	%r12,%r10
	adoxq	%rbx,%r11
.byte	0xc4,0xe2,0x9b,0xf6,0x9e,0x30,0x00,0x00,0x00  // mulx	6*8(%rsi),%r12,%rbx
	adcxq	%r13,%r11
	adoxq	%r14,%r12
.byte	0xc4,0x62,0x93,0xf6,0xb6,0x38,0x00,0x00,0x00  // mulx	7*8(%rsi),%r13,%r14
	movq	16(%rsi),%rdx  // a[2]
	adcxq	%rax,%r12
	adoxq	%rbx,%r13
	adcxq	%r15,%r13
	adoxq	%rbp,%r14  // of=0
	adcxq	%rbp,%r14  // cf=0

	movq	%r8,24(%rdi)  // t[3]
	movq	%r9,32(%rdi)  // t[4]

	mulxq	24(%rsi),%r8,%rbx  // a[3]*a[2]
	mulxq	32(%rsi),%r9,%rax  // a[4]*a[2]
	adcxq	%r10,%r8
	adoxq	%rbx,%r9
	mulxq	40(%rsi),%r10,%rbx  // ...
	adcxq	%r11,%r9
	adoxq	%rax,%r10
.byte	0xc4,0xe2,0xa3,0xf6,0x86,0x30,0x00,0x00,0x00  // mulx	6*8(%rsi),%r11,%rax
	adcxq	%r12,%r10
	adoxq	%r13,%r11
.byte	0xc4,0x62,0x9b,0xf6,0xae,0x38,0x00,0x00,0x00  // mulx	7*8(%rsi),%r12,%r13
.byte	0x3e
	movq	24(%rsi),%rdx  // a[3]
	adcxq	%rbx,%r11
	adoxq	%rax,%r12
	adcxq	%r14,%r12
	movq	%r8,40(%rdi)  // t[5]
	movq	%r9,48(%rdi)  // t[6]
	mulxq	32(%rsi),%r8,%rax  // a[4]*a[3]
	adoxq	%rbp,%r13  // of=0
	adcxq	%rbp,%r13  // cf=0

	mulxq	40(%rsi),%r9,%rbx  // a[5]*a[3]
	adcxq	%r10,%r8
	adoxq	%rax,%r9
	mulxq	48(%rsi),%r10,%rax  // ...
	adcxq	%r11,%r9
	adoxq	%r12,%r10
	mulxq	56(%rsi),%r11,%r12
	movq	32(%rsi),%rdx  // a[4]
	movq	40(%rsi),%r14  // a[5]
	adcxq	%rbx,%r10
	adoxq	%rax,%r11
	movq	48(%rsi),%r15  // a[6]
	adcxq	%r13,%r11
	adoxq	%rbp,%r12  // of=0
	adcxq	%rbp,%r12  // cf=0

	movq	%r8,56(%rdi)  // t[7]
	movq	%r9,64(%rdi)  // t[8]

	mulxq	%r14,%r9,%rax  // a[5]*a[4]
	movq	56(%rsi),%r8  // a[7]
	adcxq	%r10,%r9
	mulxq	%r15,%r10,%rbx  // a[6]*a[4]
	adoxq	%rax,%r10
	adcxq	%r11,%r10
	mulxq	%r8,%r11,%rax  // a[7]*a[4]
	movq	%r14,%rdx  // a[5]
	adoxq	%rbx,%r11
	adcxq	%r12,%r11
// adox	%rbp,%rax		# of=0
	adcxq	%rbp,%rax  // cf=0

	mulxq	%r15,%r14,%rbx  // a[6]*a[5]
	mulxq	%r8,%r12,%r13  // a[7]*a[5]
	movq	%r15,%rdx  // a[6]
	leaq	64(%rsi),%rsi
	adcxq	%r14,%r11
	adoxq	%rbx,%r12
	adcxq	%rax,%r12
	adoxq	%rbp,%r13

.byte	0x67,0x67
	mulxq	%r8,%r8,%r14  // a[7]*a[6]
	adcxq	%r8,%r13
	adcxq	%rbp,%r14

	cmpq	8+8(%rsp),%rsi
	je	L$sqrx8x_outer_break

	negq	%rcx  // mov %rcx,%cf
	movq	$-8,%rcx
	movq	%rbp,%r15
	movq	64(%rdi),%r8
	adcxq	72(%rdi),%r9  // +=t[9]
	adcxq	80(%rdi),%r10  // ...
	adcxq	88(%rdi),%r11
	adcq	96(%rdi),%r12
	adcq	104(%rdi),%r13
	adcq	112(%rdi),%r14
	adcq	120(%rdi),%r15
	leaq	(%rsi),%rbp
	leaq	128(%rdi),%rdi
	sbbq	%rax,%rax  // mov %cf,%rcx

	movq	-64(%rsi),%rdx  // a[0]
	movq	%rax,16+8(%rsp)  // offload %rcx
	movq	%rdi,24+8(%rsp)

// lea	8*8(%rdi),%rdi	# see 2*8*8(%rdi) above
	xorl	%eax,%eax  // cf=0, of=0
	jmp	L$sqrx8x_loop

.p2align	5
L$sqrx8x_loop:
	movq	%r8,%rbx
	mulxq	0(%rbp),%rax,%r8  // a[8]*a[i]
	adcxq	%rax,%rbx  // +=t[8]
	adoxq	%r9,%r8

	mulxq	8(%rbp),%rax,%r9  // ...
	adcxq	%rax,%r8
	adoxq	%r10,%r9

	mulxq	16(%rbp),%rax,%r10
	adcxq	%rax,%r9
	adoxq	%r11,%r10

	mulxq	24(%rbp),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11

.byte	0xc4,0x62,0xfb,0xf6,0xa5,0x20,0x00,0x00,0x00  // mulx	4*8(%rbp),%rax,%r12
	adcxq	%rax,%r11
	adoxq	%r13,%r12

	mulxq	40(%rbp),%rax,%r13
	adcxq	%rax,%r12
	adoxq	%r14,%r13

	mulxq	48(%rbp),%rax,%r14
	movq	%rbx,(%rdi,%rcx,8)  // store t[8+i]
	movl	$0,%ebx
	adcxq	%rax,%r13
	adoxq	%r15,%r14

.byte	0xc4,0x62,0xfb,0xf6,0xbd,0x38,0x00,0x00,0x00  // mulx	7*8(%rbp),%rax,%r15
	movq	8(%rsi,%rcx,8),%rdx  // a[i]
	adcxq	%rax,%r14
	adoxq	%rbx,%r15  // %rbx is 0, of=0
	adcxq	%rbx,%r15  // cf=0

.byte	0x67
	incq	%rcx  // of=0
	jnz	L$sqrx8x_loop

	leaq	64(%rbp),%rbp
	movq	$-8,%rcx
	cmpq	8+8(%rsp),%rbp  // done?
	je	L$sqrx8x_break

	subq	16+8(%rsp),%rbx  // mov 16(%rsp),%cf
.byte	0x66
	movq	-64(%rsi),%rdx
	adcxq	0(%rdi),%r8
	adcxq	8(%rdi),%r9
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15
	leaq	64(%rdi),%rdi
.byte	0x67
	sbbq	%rax,%rax  // mov %cf,%rax
	xorl	%ebx,%ebx  // cf=0, of=0
	movq	%rax,16+8(%rsp)  // offload carry
	jmp	L$sqrx8x_loop

.p2align	5
L$sqrx8x_break:
	xorq	%rbp,%rbp
	subq	16+8(%rsp),%rbx  // mov 16(%rsp),%cf
	adcxq	%rbp,%r8
	movq	24+8(%rsp),%rcx  // initial %rdi, borrow %rcx
	adcxq	%rbp,%r9
	movq	0(%rsi),%rdx  // a[8], modulo-scheduled
	adcq	$0,%r10
	movq	%r8,0(%rdi)
	adcq	$0,%r11
	adcq	$0,%r12
	adcq	$0,%r13
	adcq	$0,%r14
	adcq	$0,%r15
	cmpq	%rcx,%rdi  // cf=0, of=0
	je	L$sqrx8x_outer_loop

	movq	%r9,8(%rdi)
	movq	8(%rcx),%r9
	movq	%r10,16(%rdi)
	movq	16(%rcx),%r10
	movq	%r11,24(%rdi)
	movq	24(%rcx),%r11
	movq	%r12,32(%rdi)
	movq	32(%rcx),%r12
	movq	%r13,40(%rdi)
	movq	40(%rcx),%r13
	movq	%r14,48(%rdi)
	movq	48(%rcx),%r14
	movq	%r15,56(%rdi)
	movq	56(%rcx),%r15
	movq	%rcx,%rdi
	jmp	L$sqrx8x_outer_loop

.p2align	5
L$sqrx8x_outer_break:
	movq	%r9,72(%rdi)  // t[9]
	movq	%xmm3,%rcx  // -%r9
	movq	%r10,80(%rdi)  // ...
	movq	%r11,88(%rdi)
	movq	%r12,96(%rdi)
	movq	%r13,104(%rdi)
	movq	%r14,112(%rdi)
	leaq	48+8(%rsp),%rdi
	movq	(%rsi,%rcx,1),%rdx  // a[0]

	movq	8(%rdi),%r11  // t[1]
	xorq	%r10,%r10  // t[0], of=0, cf=0
	movq	0+8(%rsp),%r9  // restore %r9
	adoxq	%r11,%r11
	movq	16(%rdi),%r12  // t[2]	# prefetch
	movq	24(%rdi),%r13  // t[3]	# prefetch
// jmp	.Lsqrx4x_shift_n_add	# happens to be aligned

.p2align	5
L$sqrx4x_shift_n_add:
	mulxq	%rdx,%rax,%rbx
	adoxq	%r12,%r12
	adcxq	%r10,%rax
.byte	0x48,0x8b,0x94,0x0e,0x08,0x00,0x00,0x00  // mov	8(%rsi,%rcx),%rdx	# a[i+1]	# prefetch
.byte	0x4c,0x8b,0x97,0x20,0x00,0x00,0x00  // mov	32(%rdi),%r10	# t[2*i+4]	# prefetch
	adoxq	%r13,%r13
	adcxq	%r11,%rbx
	movq	40(%rdi),%r11  // t[2*i+4+1]	# prefetch
	movq	%rax,0(%rdi)
	movq	%rbx,8(%rdi)

	mulxq	%rdx,%rax,%rbx
	adoxq	%r10,%r10
	adcxq	%r12,%rax
	movq	16(%rsi,%rcx,1),%rdx  // a[i+2]	# prefetch
	movq	48(%rdi),%r12  // t[2*i+6]	# prefetch
	adoxq	%r11,%r11
	adcxq	%r13,%rbx
	movq	56(%rdi),%r13  // t[2*i+6+1]	# prefetch
	movq	%rax,16(%rdi)
	movq	%rbx,24(%rdi)

	mulxq	%rdx,%rax,%rbx
	adoxq	%r12,%r12
	adcxq	%r10,%rax
	movq	24(%rsi,%rcx,1),%rdx  // a[i+3]	# prefetch
	leaq	32(%rcx),%rcx
	movq	64(%rdi),%r10  // t[2*i+8]	# prefetch
	adoxq	%r13,%r13
	adcxq	%r11,%rbx
	movq	72(%rdi),%r11  // t[2*i+8+1]	# prefetch
	movq	%rax,32(%rdi)
	movq	%rbx,40(%rdi)

	mulxq	%rdx,%rax,%rbx
	adoxq	%r10,%r10
	adcxq	%r12,%rax
	jrcxz	L$sqrx4x_shift_n_add_break
.byte	0x48,0x8b,0x94,0x0e,0x00,0x00,0x00,0x00  // mov	0(%rsi,%rcx),%rdx	# a[i+4]	# prefetch
	adoxq	%r11,%r11
	adcxq	%r13,%rbx
	movq	80(%rdi),%r12  // t[2*i+10]	# prefetch
	movq	88(%rdi),%r13  // t[2*i+10+1]	# prefetch
	movq	%rax,48(%rdi)
	movq	%rbx,56(%rdi)
	leaq	64(%rdi),%rdi
	nop
	jmp	L$sqrx4x_shift_n_add

.p2align	5
L$sqrx4x_shift_n_add_break:
	adcxq	%r13,%rbx
	movq	%rax,48(%rdi)
	movq	%rbx,56(%rdi)
	leaq	64(%rdi),%rdi  // end of t[] buffer
	movq	%xmm2,%rbp
.alt_entry __bn_sqrx8x_reduction
__bn_sqrx8x_reduction:
	xorl	%eax,%eax  // initial top-most carry bit
	movq	32+8(%rsp),%rbx  // n0
	movq	48+8(%rsp),%rdx  // "%r8", 8*0(%rdi)
	leaq	-64(%rbp,%r9,1),%rcx  // end of n[]
// lea	48+8(%rsp,%r9,2),%rdi	# end of t[] buffer
	movq	%rcx,0+8(%rsp)  // save end of n[]
	movq	%rdi,8+8(%rsp)  // save end of t[]

	leaq	48+8(%rsp),%rdi  // initial t[] window
	jmp	L$sqrx8x_reduction_loop

.p2align	5
L$sqrx8x_reduction_loop:
	movq	8(%rdi),%r9
	movq	16(%rdi),%r10
	movq	24(%rdi),%r11
	movq	32(%rdi),%r12
	movq	%rdx,%r8
	imulq	%rbx,%rdx  // n0*a[i]
	movq	40(%rdi),%r13
	movq	48(%rdi),%r14
	movq	56(%rdi),%r15
	movq	%rax,24+8(%rsp)  // store top-most carry bit

	leaq	64(%rdi),%rdi
	xorq	%rsi,%rsi  // cf=0,of=0
	movq	$-8,%rcx
	jmp	L$sqrx8x_reduce

.p2align	5
L$sqrx8x_reduce:
	movq	%r8,%rbx
	mulxq	0(%rbp),%rax,%r8  // n[0]
	adcxq	%rbx,%rax  // discarded
	adoxq	%r9,%r8

	mulxq	8(%rbp),%rbx,%r9  // n[1]
	adcxq	%rbx,%r8
	adoxq	%r10,%r9

	mulxq	16(%rbp),%rbx,%r10
	adcxq	%rbx,%r9
	adoxq	%r11,%r10

	mulxq	24(%rbp),%rbx,%r11
	adcxq	%rbx,%r10
	adoxq	%r12,%r11

.byte	0xc4,0x62,0xe3,0xf6,0xa5,0x20,0x00,0x00,0x00  // mulx	8*4(%rbp),%rbx,%r12
	movq	%rdx,%rax
	movq	%r8,%rdx
	adcxq	%rbx,%r11
	adoxq	%r13,%r12

	mulxq	32+8(%rsp),%rbx,%rdx  // %rdx discarded
	movq	%rax,%rdx
	movq	%rax,64+48+8(%rsp,%rcx,8)  // put aside n0*a[i]

	mulxq	40(%rbp),%rax,%r13
	adcxq	%rax,%r12
	adoxq	%r14,%r13

	mulxq	48(%rbp),%rax,%r14
	adcxq	%rax,%r13
	adoxq	%r15,%r14

	mulxq	56(%rbp),%rax,%r15
	movq	%rbx,%rdx
	adcxq	%rax,%r14
	adoxq	%rsi,%r15  // %rsi is 0
	adcxq	%rsi,%r15  // cf=0

.byte	0x67,0x67,0x67
	incq	%rcx  // of=0
	jnz	L$sqrx8x_reduce

	movq	%rsi,%rax  // xor	%rax,%rax
	cmpq	0+8(%rsp),%rbp  // end of n[]?
	jae	L$sqrx8x_no_tail

	movq	48+8(%rsp),%rdx  // pull n0*a[0]
	addq	0(%rdi),%r8
	leaq	64(%rbp),%rbp
	movq	$-8,%rcx
	adcxq	8(%rdi),%r9
	adcxq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15
	leaq	64(%rdi),%rdi
	sbbq	%rax,%rax  // top carry

	xorq	%rsi,%rsi  // of=0, cf=0
	movq	%rax,16+8(%rsp)
	jmp	L$sqrx8x_tail

.p2align	5
L$sqrx8x_tail:
	movq	%r8,%rbx
	mulxq	0(%rbp),%rax,%r8
	adcxq	%rax,%rbx
	adoxq	%r9,%r8

	mulxq	8(%rbp),%rax,%r9
	adcxq	%rax,%r8
	adoxq	%r10,%r9

	mulxq	16(%rbp),%rax,%r10
	adcxq	%rax,%r9
	adoxq	%r11,%r10

	mulxq	24(%rbp),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11

.byte	0xc4,0x62,0xfb,0xf6,0xa5,0x20,0x00,0x00,0x00  // mulx	8*4(%rbp),%rax,%r12
	adcxq	%rax,%r11
	adoxq	%r13,%r12

	mulxq	40(%rbp),%rax,%r13
	adcxq	%rax,%r12
	adoxq	%r14,%r13

	mulxq	48(%rbp),%rax,%r14
	adcxq	%rax,%r13
	adoxq	%r15,%r14

	mulxq	56(%rbp),%rax,%r15
	movq	72+48+8(%rsp,%rcx,8),%rdx  // pull n0*a[i]
	adcxq	%rax,%r14
	adoxq	%rsi,%r15
	movq	%rbx,(%rdi,%rcx,8)  // save result
	movq	%r8,%rbx
	adcxq	%rsi,%r15  // cf=0

	incq	%rcx  // of=0
	jnz	L$sqrx8x_tail

	cmpq	0+8(%rsp),%rbp  // end of n[]?
	jae	L$sqrx8x_tail_done  // break out of loop

	subq	16+8(%rsp),%rsi  // mov 16(%rsp),%cf
	movq	48+8(%rsp),%rdx  // pull n0*a[0]
	leaq	64(%rbp),%rbp
	adcq	0(%rdi),%r8
	adcq	8(%rdi),%r9
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15
	leaq	64(%rdi),%rdi
	sbbq	%rax,%rax
	subq	$8,%rcx  // mov	$-8,%rcx

	xorq	%rsi,%rsi  // of=0, cf=0
	movq	%rax,16+8(%rsp)
	jmp	L$sqrx8x_tail

.p2align	5
L$sqrx8x_tail_done:
	xorq	%rax,%rax
	addq	24+8(%rsp),%r8  // can this overflow?
	adcq	$0,%r9
	adcq	$0,%r10
	adcq	$0,%r11
	adcq	$0,%r12
	adcq	$0,%r13
	adcq	$0,%r14
	adcq	$0,%r15
	adcq	$0,%rax

	subq	16+8(%rsp),%rsi  // mov 16(%rsp),%cf
L$sqrx8x_no_tail:  // %cf is 0 if jumped here
	adcq	0(%rdi),%r8
	movq	%xmm3,%rcx
	adcq	8(%rdi),%r9
	movq	56(%rbp),%rsi
	movq	%xmm2,%rbp  // restore %rbp
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15
	adcq	$0,%rax  // top-most carry

	movq	32+8(%rsp),%rbx  // n0
	movq	64(%rdi,%rcx,1),%rdx  // modulo-scheduled "%r8"

	movq	%r8,0(%rdi)  // store top 512 bits
	leaq	64(%rdi),%r8  // borrow %r8
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	leaq	64(%rdi,%rcx,1),%rdi  // start of current t[] window
	cmpq	8+8(%rsp),%r8  // end of t[]?
	jb	L$sqrx8x_reduction_loop
	ret


.p2align	5

.alt_entry __bn_postx4x_internal
__bn_postx4x_internal:

	movq	0(%rbp),%r12
	movq	%rcx,%r10  // -%r9
	movq	%rcx,%r9  // -%r9
	negq	%rax
	sarq	$3+2,%rcx
// lea	48+8(%rsp,%r9),%rdi
	movq	%xmm1,%rdx  // restore %rdx
	movq	%xmm1,%rsi  // prepare for back-to-back call
	decq	%r12  // so that after 'not' we get -n[0]
	movq	8(%rbp),%r13
	xorq	%r8,%r8
	movq	16(%rbp),%r14
	movq	24(%rbp),%r15
	jmp	L$sqrx4x_sub_entry

.p2align	4
L$sqrx4x_sub:
	movq	0(%rbp),%r12
	movq	8(%rbp),%r13
	movq	16(%rbp),%r14
	movq	24(%rbp),%r15
L$sqrx4x_sub_entry:
	andnq	%rax,%r12,%r12
	leaq	32(%rbp),%rbp
	andnq	%rax,%r13,%r13
	andnq	%rax,%r14,%r14
	andnq	%rax,%r15,%r15

	negq	%r8  // mov %r8,%cf
	adcq	0(%rdi),%r12
	adcq	8(%rdi),%r13
	adcq	16(%rdi),%r14
	adcq	24(%rdi),%r15
	movq	%r12,0(%rdx)
	leaq	32(%rdi),%rdi
	movq	%r13,8(%rdx)
	sbbq	%r8,%r8  // mov %cf,%r8
	movq	%r14,16(%rdx)
	movq	%r15,24(%rdx)
	leaq	32(%rdx),%rdx

	incq	%rcx
	jnz	L$sqrx4x_sub

	negq	%r9  // restore %r9

	ret


.globl	_bn_scatter5
.private_extern _bn_scatter5

.p2align	4
.alt_entry _bn_scatter5
_bn_scatter5:

_CET_ENDBR
	cmpl	$0,%esi
	jz	L$scatter_epilogue

// %rdx stores 32 entries, t0 through t31. Each entry has %esi words.
// They are interleaved in memory as follows:
// 
// t0[0]      t1[0]      t2[0]      ... t31[0]
// t0[1]      t1[1]      t2[1]      ... t31[1]
// ...
// t0[%esi-1] t1[%esi-1] t2[%esi-1] ... t31[%esi-1]

	leaq	(%rdx,%rcx,8),%rdx
L$scatter:
	movq	(%rdi),%rax
	leaq	8(%rdi),%rdi
	movq	%rax,(%rdx)
	leaq	256(%rdx),%rdx
	subl	$1,%esi
	jnz	L$scatter
L$scatter_epilogue:
	ret



.globl	_bn_gather5
.private_extern _bn_gather5

.p2align	5
.alt_entry _bn_gather5
_bn_gather5:

L$SEH_begin_bn_gather5:  // Win64 thing, but harmless in other cases
_CET_ENDBR
// I can't trust assembler to use specific encoding:-(
.byte	0x4c,0x8d,0x14,0x24  // lea    (%rsp),%r10

.byte	0x48,0x81,0xec,0x08,0x01,0x00,0x00  // sub	$0x108,%rsp
	leaq	L$inc(%rip),%rax
	andq	$-16,%rsp  // shouldn't be formally required

	movd	%ecx,%xmm5
	movdqa	0(%rax),%xmm0  // 00000001000000010000000000000000
	movdqa	16(%rax),%xmm1  // 00000002000000020000000200000002
	leaq	128(%rdx),%r11  // size optimization
	leaq	128(%rsp),%rax  // size optimization

	pshufd	$0,%xmm5,%xmm5  // broadcast %ecx
	movdqa	%xmm1,%xmm4
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0  // compare to 1,0
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,-128(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,-112(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,-96(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0  // compare to 1,0
	movdqa	%xmm3,-80(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,-64(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,-48(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,-32(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0  // compare to 1,0
	movdqa	%xmm3,-16(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,0(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,16(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,32(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0  // compare to 1,0
	movdqa	%xmm3,48(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1  // compare to 3,2
	movdqa	%xmm0,64(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2  // compare to 5,4
	movdqa	%xmm1,80(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3  // compare to 7,6
	movdqa	%xmm2,96(%rax)
	movdqa	%xmm4,%xmm2
	movdqa	%xmm3,112(%rax)
	jmp	L$gather

.p2align	5
L$gather:
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r11),%xmm0
	movdqa	-112(%r11),%xmm1
	movdqa	-96(%r11),%xmm2
	pand	-128(%rax),%xmm0
	movdqa	-80(%r11),%xmm3
	pand	-112(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r11),%xmm0
	movdqa	-48(%r11),%xmm1
	movdqa	-32(%r11),%xmm2
	pand	-64(%rax),%xmm0
	movdqa	-16(%r11),%xmm3
	pand	-48(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r11),%xmm0
	movdqa	16(%r11),%xmm1
	movdqa	32(%r11),%xmm2
	pand	0(%rax),%xmm0
	movdqa	48(%r11),%xmm3
	pand	16(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r11),%xmm0
	movdqa	80(%r11),%xmm1
	movdqa	96(%r11),%xmm2
	pand	64(%rax),%xmm0
	movdqa	112(%r11),%xmm3
	pand	80(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
	leaq	256(%r11),%r11
// Combine the upper and lower halves of %xmm0.
	pshufd	$0x4e,%xmm4,%xmm0  // Swap upper and lower halves.
	por	%xmm4,%xmm0
	movq	%xmm0,(%rdi)  // m0=bp[0]
	leaq	8(%rdi),%rdi
	subl	$1,%esi
	jnz	L$gather

	leaq	(%r10),%rsp

	ret
L$SEH_end_bn_gather5:


.section	__DATA,__const

.p2align	6
mont5_increments:
L$inc:
.long	0,0, 1,1
.long	2,2, 2,2
.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,119,105,116,104,32,115,99,97,116,116,101,114,47,103,97,116,104,101,114,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.text	
#endif
