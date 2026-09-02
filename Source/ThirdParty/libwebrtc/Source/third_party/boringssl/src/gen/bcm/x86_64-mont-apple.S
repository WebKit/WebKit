// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	

.globl	_bn_mul_mont_nohw
.private_extern _bn_mul_mont_nohw

.p2align	4
_bn_mul_mont_nohw:

_CET_ENDBR
	movl	%r9d,%r9d
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	negq	%r9
	movq	%rsp,%r11
	leaq	-16(%rsp,%r9,8),%r10  // future alloca(8*(num+2))
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

.p2align	4
L$mul_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r11
	cmpq	%r10,%rsp
	ja	L$mul_page_walk
L$mul_page_walk_done:

	movq	%rax,8(%rsp,%r9,8)  // tp[num+1]=%rsp

L$mul_body:
	movq	%rdx,%r12  // reassign %rdx
	movq	(%r8),%r8  // pull n0[0] value
	movq	(%r12),%rbx  // m0=bp[0]
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
	jne	L$1st

	addq	%rax,%r13
	movq	(%rsi),%rax  // ap[0]
	adcq	$0,%rdx
	addq	%r11,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)  // tp[j-1]
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
	movq	(%r12,%r14,8),%rbx  // m0=bp[i]
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
	jne	L$inner

	addq	%rax,%r13
	movq	(%rsi),%rax  // ap[0]
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[i]+tp[j]
	movq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)  // tp[j-1]
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
	movq	%r9,%r15  // j=num

.p2align	4
L$sub:	sbbq	(%rcx,%r14,8),%rax
	movq	%rax,(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	movq	8(%rsp,%r14,8),%rax  // tp[i+1]
	leaq	1(%r14),%r14  // i++
	decq	%r15  // doesn't affect CF
	jnz	L$sub

	sbbq	$0,%rax  // handle upmost overflow bit
	movq	$-1,%rbx
	xorq	%rax,%rbx  // not %rax
	xorq	%r14,%r14
	movq	%r9,%r15  // j=num

L$copy:  // conditional copy
	movq	(%rdi,%r14,8),%rcx
	movq	(%rsp,%r14,8),%rdx
	andq	%rbx,%rcx
	andq	%rax,%rdx
	movq	%r9,(%rsp,%r14,8)  // zap temporary vector
	orq	%rcx,%rdx
	movq	%rdx,(%rdi,%r14,8)  // rp[i]=tp[i]
	leaq	1(%r14),%r14
	subq	$1,%r15
	jnz	L$copy

	movq	8(%rsp,%r9,8),%rsi  // restore %rsp

// No return value
	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$mul_epilogue:
	ret


.globl	_bn_mul4x_mont
.private_extern _bn_mul4x_mont

.p2align	4
.alt_entry _bn_mul4x_mont
_bn_mul4x_mont:

_CET_ENDBR
	movl	%r9d,%r9d
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	negq	%r9
	movq	%rsp,%r11
	leaq	-32(%rsp,%r9,8),%r10  // future alloca(8*(num+4))
	negq	%r9  // restore
	andq	$-1024,%r10  // minimize TLB usage

	subq	%r10,%r11
	andq	$-4096,%r11
	leaq	(%r10,%r11,1),%rsp
	movq	(%rsp),%r11
	cmpq	%r10,%rsp
	ja	L$mul4x_page_walk
	jmp	L$mul4x_page_walk_done

L$mul4x_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r11
	cmpq	%r10,%rsp
	ja	L$mul4x_page_walk
L$mul4x_page_walk_done:

	movq	%rax,8(%rsp,%r9,8)  // tp[num+1]=%rsp

L$mul4x_body:
	movq	%rdi,16(%rsp,%r9,8)  // tp[num+2]=%rdi
	movq	%rdx,%r12  // reassign %r12
	movq	(%r8),%r8  // pull n0[0] value
	movq	(%r12),%rbx  // m0=bp[0]
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
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	leaq	4(%r15),%r15  // j++
	adcq	$0,%rdx
	movq	%rdi,(%rsp)
	movq	%rdx,%r13
	jmp	L$1st4x
.p2align	4
L$1st4x:
	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r11
	movq	8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	leaq	4(%r15),%r15  // j++
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	-16(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13
	cmpq	%r9,%r15
	jb	L$1st4x

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[0]
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi),%rax  // ap[0]
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[0]
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdi,(%rsp,%r15,8)  // store upmost overflow bit

	leaq	1(%r14),%r14  // i++
.p2align	2
L$outer4x:
	movq	(%r12,%r14,8),%rbx  // m0=bp[i]
	xorq	%r15,%r15  // j=0
	movq	(%rsp),%r10
	movq	%r8,%rbp
	mulq	%rbx  // ap[0]*bp[i]
	addq	%rax,%r10  // ap[0]*bp[i]+tp[0]
	movq	(%rcx),%rax
	adcq	$0,%rdx

	imulq	%r10,%rbp  // tp[0]*n0
	movq	%rdx,%r11

	mulq	%rbp  // np[0]*m1
	addq	%rax,%r10  // "%r13", discarded
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	addq	8(%rsp),%r11  // +tp[1]
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi  // np[j]*m1+ap[j]*bp[i]+tp[j]
	leaq	4(%r15),%r15  // j+=2
	adcq	$0,%rdx
	movq	%rdi,(%rsp)  // tp[j-1]
	movq	%rdx,%r13
	jmp	L$inner4x
.p2align	4
L$inner4x:
	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-16(%rsp,%r15,8),%r10  // ap[j]*bp[i]+tp[j]
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	(%rsp,%r15,8),%r10  // ap[j]*bp[i]+tp[j]
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	leaq	4(%r15),%r15  // j++
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	-16(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13
	cmpq	%r9,%r15
	jb	L$inner4x

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-16(%rsp,%r15,8),%r10  // ap[j]*bp[i]+tp[j]
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp  // np[j]*m1
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%rdi

	mulq	%rbx  // ap[j]*bp[i]
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	leaq	1(%r14),%r14  // i++
	movq	%rdx,%r10

	mulq	%rbp  // np[j]*m1
	addq	%rax,%rdi
	movq	(%rsi),%rax  // ap[0]
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)  // tp[j-1]
	movq	%rdx,%r13

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	addq	(%rsp,%r9,8),%r13  // pull upmost overflow bit
	adcq	$0,%rdi
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdi,(%rsp,%r15,8)  // store upmost overflow bit

	cmpq	%r9,%r14
	jb	L$outer4x
	movq	16(%rsp,%r9,8),%rdi  // restore %rdi
	leaq	-4(%r9),%r15
	movq	0(%rsp),%rax  // tp[0]
	movq	8(%rsp),%rdx  // tp[1]
	shrq	$2,%r15  // j=num/4-1
	leaq	(%rsp),%rsi  // borrow ap for tp
	xorq	%r14,%r14  // i=0 and clear CF

	subq	0(%rcx),%rax
	movq	16(%rsi),%rbx  // tp[2]
	movq	24(%rsi),%rbp  // tp[3]
	sbbq	8(%rcx),%rdx

L$sub4x:
	movq	%rax,0(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	movq	%rdx,8(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	sbbq	16(%rcx,%r14,8),%rbx
	movq	32(%rsi,%r14,8),%rax  // tp[i+1]
	movq	40(%rsi,%r14,8),%rdx
	sbbq	24(%rcx,%r14,8),%rbp
	movq	%rbx,16(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	movq	%rbp,24(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	sbbq	32(%rcx,%r14,8),%rax
	movq	48(%rsi,%r14,8),%rbx
	movq	56(%rsi,%r14,8),%rbp
	sbbq	40(%rcx,%r14,8),%rdx
	leaq	4(%r14),%r14  // i++
	decq	%r15  // doesn't affect CF
	jnz	L$sub4x

	movq	%rax,0(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	movq	32(%rsi,%r14,8),%rax  // load overflow bit
	sbbq	16(%rcx,%r14,8),%rbx
	movq	%rdx,8(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	sbbq	24(%rcx,%r14,8),%rbp
	movq	%rbx,16(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]

	sbbq	$0,%rax  // handle upmost overflow bit
	movq	%rbp,24(%rdi,%r14,8)  // rp[i]=tp[i]-np[i]
	pxor	%xmm0,%xmm0
	movq	%rax,%xmm4
	pcmpeqd	%xmm5,%xmm5
	pshufd	$0,%xmm4,%xmm4
	movq	%r9,%r15
	pxor	%xmm4,%xmm5
	shrq	$2,%r15  // j=num/4
	xorl	%eax,%eax  // i=0

	jmp	L$copy4x
.p2align	4
L$copy4x:  // conditional copy
	movdqa	(%rsp,%rax,1),%xmm1
	movdqu	(%rdi,%rax,1),%xmm2
	pand	%xmm4,%xmm1
	pand	%xmm5,%xmm2
	movdqa	16(%rsp,%rax,1),%xmm3
	movdqa	%xmm0,(%rsp,%rax,1)
	por	%xmm2,%xmm1
	movdqu	16(%rdi,%rax,1),%xmm2
	movdqu	%xmm1,(%rdi,%rax,1)
	pand	%xmm4,%xmm3
	pand	%xmm5,%xmm2
	movdqa	%xmm0,16(%rsp,%rax,1)
	por	%xmm2,%xmm3
	movdqu	%xmm3,16(%rdi,%rax,1)
	leaq	32(%rax),%rax
	decq	%r15
	jnz	L$copy4x
	movq	8(%rsp,%r9,8),%rsi  // restore %rsp

// No return value
	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$mul4x_epilogue:
	ret


// see x86_64-mont5 module
// see x86_64-mont5 module

.globl	_bn_sqr8x_mont
.private_extern _bn_sqr8x_mont

.p2align	5
.alt_entry _bn_sqr8x_mont
_bn_sqr8x_mont:

_CET_ENDBR
	movl	%r9d,%r9d
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$sqr8x_prologue:

	movl	%r9d,%r10d
	shll	$3,%r9d  // convert %r9 to bytes
	shlq	$3+2,%r10  // 4*%r9
	negq	%r9

// #############################################################
// ensure that stack frame doesn't alias with %rsi modulo
// 4096. this is done to allow memory disambiguation logic
// do its job.
// 
	leaq	-64(%rsp,%r9,2),%r11
	movq	%rsp,%rbp
	movq	(%r8),%r8  // *n0
	subq	%rsi,%r11
	andq	$4095,%r11
	cmpq	%r11,%r10
	jb	L$sqr8x_sp_alt
	subq	%r11,%rbp  // align with %rsi
	leaq	-64(%rbp,%r9,2),%rbp  // future alloca(frame+2*%r9)
	jmp	L$sqr8x_sp_done

.p2align	5
L$sqr8x_sp_alt:
	leaq	4096-64(,%r9,2),%r10  // 4096-frame-2*%r9
	leaq	-64(%rbp,%r9,2),%rbp  // future alloca(frame+2*%r9)
	subq	%r10,%r11
	movq	$0,%r10
	cmovcq	%r10,%r11
	subq	%r11,%rbp
L$sqr8x_sp_done:
	andq	$-64,%rbp
	movq	%rsp,%r11
	subq	%rbp,%r11
	andq	$-4096,%r11
	leaq	(%r11,%rbp,1),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$sqr8x_page_walk
	jmp	L$sqr8x_page_walk_done

.p2align	4
L$sqr8x_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$sqr8x_page_walk
L$sqr8x_page_walk_done:

	movq	%r9,%r10
	negq	%r9

	movq	%r8,32(%rsp)
	movq	%rax,40(%rsp)  // save original %rsp

L$sqr8x_body:

	movq	%rcx,%xmm2  // save pointer to modulus
	pxor	%xmm0,%xmm0
	movq	%rdi,%xmm1  // save %rdi
	movq	%r10,%xmm3  // -%r9
	testq	%rdx,%rdx
	jz	L$sqr8x_nox

	call	_bn_sqrx8x_internal  // see x86_64-mont5 module
// %rax	top-most carry
// %rbp	nptr
// %rcx	-8*num
// %r8	end of tp[2*num]
	leaq	(%r8,%rcx,1),%rbx
	movq	%rcx,%r9
	movq	%rcx,%rdx
	movq	%xmm1,%rdi
	sarq	$3+2,%rcx  // %cf=0
	jmp	L$sqr8x_sub

.p2align	5
L$sqr8x_nox:
	call	_bn_sqr8x_internal  // see x86_64-mont5 module
// %rax	top-most carry
// %rbp	nptr
// %r8	-8*num
// %rdi	end of tp[2*num]
	leaq	(%rdi,%r9,1),%rbx
	movq	%r9,%rcx
	movq	%r9,%rdx
	movq	%xmm1,%rdi
	sarq	$3+2,%rcx  // %cf=0
	jmp	L$sqr8x_sub

.p2align	5
L$sqr8x_sub:
	movq	0(%rbx),%r12
	movq	8(%rbx),%r13
	movq	16(%rbx),%r14
	movq	24(%rbx),%r15
	leaq	32(%rbx),%rbx
	sbbq	0(%rbp),%r12
	sbbq	8(%rbp),%r13
	sbbq	16(%rbp),%r14
	sbbq	24(%rbp),%r15
	leaq	32(%rbp),%rbp
	movq	%r12,0(%rdi)
	movq	%r13,8(%rdi)
	movq	%r14,16(%rdi)
	movq	%r15,24(%rdi)
	leaq	32(%rdi),%rdi
	incq	%rcx  // preserves %cf
	jnz	L$sqr8x_sub

	sbbq	$0,%rax  // top-most carry
	leaq	(%rbx,%r9,1),%rbx  // rewind
	leaq	(%rdi,%r9,1),%rdi  // rewind

	movq	%rax,%xmm1
	pxor	%xmm0,%xmm0
	pshufd	$0,%xmm1,%xmm1
	movq	40(%rsp),%rsi  // restore %rsp

	jmp	L$sqr8x_cond_copy

.p2align	5
L$sqr8x_cond_copy:
	movdqa	0(%rbx),%xmm2
	movdqa	16(%rbx),%xmm3
	leaq	32(%rbx),%rbx
	movdqu	0(%rdi),%xmm4
	movdqu	16(%rdi),%xmm5
	leaq	32(%rdi),%rdi
	movdqa	%xmm0,-32(%rbx)  // zero tp
	movdqa	%xmm0,-16(%rbx)
	movdqa	%xmm0,-32(%rbx,%rdx,1)
	movdqa	%xmm0,-16(%rbx,%rdx,1)
	pcmpeqd	%xmm1,%xmm0
	pand	%xmm1,%xmm2
	pand	%xmm1,%xmm3
	pand	%xmm0,%xmm4
	pand	%xmm0,%xmm5
	pxor	%xmm0,%xmm0
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqu	%xmm4,-32(%rdi)
	movdqu	%xmm5,-16(%rdi)
	addq	$32,%r9
	jnz	L$sqr8x_cond_copy

// No return value
	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$sqr8x_epilogue:
	ret


.globl	_bn_mulx4x_mont
.private_extern _bn_mulx4x_mont

.p2align	5
.alt_entry _bn_mulx4x_mont
_bn_mulx4x_mont:

_CET_ENDBR
	movq	%rsp,%rax

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$mulx4x_prologue:

	shll	$3,%r9d  // convert %r9 to bytes
	xorq	%r10,%r10
	subq	%r9,%r10  // -%r9
	movq	(%r8),%r8  // *n0
	leaq	-72(%rsp,%r10,1),%rbp  // future alloca(frame+%r9+8)
	andq	$-128,%rbp
	movq	%rsp,%r11
	subq	%rbp,%r11
	andq	$-4096,%r11
	leaq	(%r11,%rbp,1),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$mulx4x_page_walk
	jmp	L$mulx4x_page_walk_done

.p2align	4
L$mulx4x_page_walk:
	leaq	-4096(%rsp),%rsp
	movq	(%rsp),%r10
	cmpq	%rbp,%rsp
	ja	L$mulx4x_page_walk
L$mulx4x_page_walk_done:

	leaq	(%rdx,%r9,1),%r10
// #############################################################
// Stack layout
// +0	num
// +8	off-loaded &b[i]
// +16	end of b[num]
// +24	saved n0
// +32	saved rp
// +40	saved %rsp
// +48	inner counter
// +56
// +64	tmp[num+1]
// 
	movq	%r9,0(%rsp)  // save %r9
	shrq	$5,%r9
	movq	%r10,16(%rsp)  // end of b[num]
	subq	$1,%r9
	movq	%r8,24(%rsp)  // save *n0
	movq	%rdi,32(%rsp)  // save %rdi
	movq	%rax,40(%rsp)  // save original %rsp

	movq	%r9,48(%rsp)  // inner counter
	jmp	L$mulx4x_body

.p2align	5
L$mulx4x_body:
	leaq	8(%rdx),%rdi
	movq	(%rdx),%rdx  // b[0], %rdx==%rdx actually
	leaq	64+32(%rsp),%rbx
	movq	%rdx,%r9

	mulxq	0(%rsi),%r8,%rax  // a[0]*b[0]
	mulxq	8(%rsi),%r11,%r14  // a[1]*b[0]
	addq	%rax,%r11
	movq	%rdi,8(%rsp)  // off-load &b[i]
	mulxq	16(%rsi),%r12,%r13  // ...
	adcq	%r14,%r12
	adcq	$0,%r13

	movq	%r8,%rdi  // borrow %rdi
	imulq	24(%rsp),%r8  // "t[0]"*n0
	xorq	%rbp,%rbp  // cf=0, of=0

	mulxq	24(%rsi),%rax,%r14
	movq	%r8,%rdx
	leaq	32(%rsi),%rsi
	adcxq	%rax,%r13
	adcxq	%rbp,%r14  // cf=0

	mulxq	0(%rcx),%rax,%r10
	adcxq	%rax,%rdi  // discarded
	adoxq	%r11,%r10
	mulxq	8(%rcx),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11
.byte	0xc4,0x62,0xfb,0xf6,0xa1,0x10,0x00,0x00,0x00  // mulx	2*8(%rcx),%rax,%r12
	movq	48(%rsp),%rdi  // counter value
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

	movq	0(%rsp),%rax  // load num
	movq	8(%rsp),%rdi  // re-load &b[i]
	adcq	%rbp,%r15  // modulo-scheduled
	addq	%r15,%r14
	sbbq	%r15,%r15  // top-most carry
	movq	%r14,-8(%rbx)
	jmp	L$mulx4x_outer

.p2align	5
L$mulx4x_outer:
	movq	(%rdi),%rdx  // b[i]
	leaq	8(%rdi),%rdi  // b++
	subq	%rax,%rsi  // rewind %rsi
	movq	%r15,(%rbx)  // save top-most carry
	leaq	64+32(%rsp),%rbx
	subq	%rax,%rcx  // rewind %rcx

	mulxq	0(%rsi),%r8,%r11  // a[0]*b[i]
	xorl	%ebp,%ebp  // xor	%rbp,%rbp	# cf=0, of=0
	movq	%rdx,%r9
	mulxq	8(%rsi),%r14,%r12  // a[1]*b[i]
	adoxq	-32(%rbx),%r8
	adcxq	%r14,%r11
	mulxq	16(%rsi),%r15,%r13  // ...
	adoxq	-24(%rbx),%r11
	adcxq	%r15,%r12
	adoxq	-16(%rbx),%r12
	adcxq	%rbp,%r13
	adoxq	%rbp,%r13

	movq	%rdi,8(%rsp)  // off-load &b[i]
	movq	%r8,%r15
	imulq	24(%rsp),%r8  // "t[0]"*n0
	xorl	%ebp,%ebp  // xor	%rbp,%rbp	# cf=0, of=0

	mulxq	24(%rsi),%rax,%r14
	movq	%r8,%rdx
	adcxq	%rax,%r13
	adoxq	-8(%rbx),%r13
	adcxq	%rbp,%r14
	leaq	32(%rsi),%rsi
	adoxq	%rbp,%r14

	mulxq	0(%rcx),%rax,%r10
	adcxq	%rax,%r15  // discarded
	adoxq	%r11,%r10
	mulxq	8(%rcx),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11
	mulxq	16(%rcx),%rax,%r12
	movq	%r10,-32(%rbx)
	adcxq	%rax,%r11
	adoxq	%r13,%r12
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r11,-24(%rbx)
	leaq	32(%rcx),%rcx
	adcxq	%rax,%r12
	adoxq	%rbp,%r15  // of=0
	movq	48(%rsp),%rdi  // counter value
	movq	%r12,-16(%rbx)

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
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r11,-32(%rbx)
	movq	%r12,-24(%rbx)
	adcxq	%rax,%r13
	adoxq	%rbp,%r15
	leaq	32(%rcx),%rcx
	movq	%r13,-16(%rbx)

	decq	%rdi  // of=0, pass cf
	jnz	L$mulx4x_inner

	movq	0(%rsp),%rax  // load num
	movq	8(%rsp),%rdi  // re-load &b[i]
	adcq	%rbp,%r15  // modulo-scheduled
	subq	0(%rbx),%rbp  // pull top-most carry
	adcq	%r15,%r14
	sbbq	%r15,%r15  // top-most carry
	movq	%r14,-8(%rbx)

	cmpq	16(%rsp),%rdi
	jne	L$mulx4x_outer

	leaq	64(%rsp),%rbx
	subq	%rax,%rcx  // rewind %rcx
	negq	%r15
	movq	%rax,%rdx
	shrq	$3+2,%rax  // %cf=0
	movq	32(%rsp),%rdi  // restore rp
	jmp	L$mulx4x_sub

.p2align	5
L$mulx4x_sub:
	movq	0(%rbx),%r11
	movq	8(%rbx),%r12
	movq	16(%rbx),%r13
	movq	24(%rbx),%r14
	leaq	32(%rbx),%rbx
	sbbq	0(%rcx),%r11
	sbbq	8(%rcx),%r12
	sbbq	16(%rcx),%r13
	sbbq	24(%rcx),%r14
	leaq	32(%rcx),%rcx
	movq	%r11,0(%rdi)
	movq	%r12,8(%rdi)
	movq	%r13,16(%rdi)
	movq	%r14,24(%rdi)
	leaq	32(%rdi),%rdi
	decq	%rax  // preserves %cf
	jnz	L$mulx4x_sub

	sbbq	$0,%r15  // top-most carry
	leaq	64(%rsp),%rbx
	subq	%rdx,%rdi  // rewind

	movq	%r15,%xmm1
	pxor	%xmm0,%xmm0
	pshufd	$0,%xmm1,%xmm1
	movq	40(%rsp),%rsi  // restore %rsp

	jmp	L$mulx4x_cond_copy

.p2align	5
L$mulx4x_cond_copy:
	movdqa	0(%rbx),%xmm2
	movdqa	16(%rbx),%xmm3
	leaq	32(%rbx),%rbx
	movdqu	0(%rdi),%xmm4
	movdqu	16(%rdi),%xmm5
	leaq	32(%rdi),%rdi
	movdqa	%xmm0,-32(%rbx)  // zero tp
	movdqa	%xmm0,-16(%rbx)
	pcmpeqd	%xmm1,%xmm0
	pand	%xmm1,%xmm2
	pand	%xmm1,%xmm3
	pand	%xmm0,%xmm4
	pand	%xmm0,%xmm5
	pxor	%xmm0,%xmm0
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqu	%xmm4,-32(%rdi)
	movdqu	%xmm5,-16(%rdi)
	subq	$32,%rdx
	jnz	L$mulx4x_cond_copy

	movq	%rdx,(%rbx)

// No return value
	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$mulx4x_epilogue:
	ret


.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	4
#endif
