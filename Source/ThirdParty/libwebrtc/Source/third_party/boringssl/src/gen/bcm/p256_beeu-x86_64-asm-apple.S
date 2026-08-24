// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	

.globl	_beeu_mod_inverse_vartime
.private_extern _beeu_mod_inverse_vartime

.p2align	5
_beeu_mod_inverse_vartime:

_CET_ENDBR
	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	pushq	%rbx

	pushq	%rsi


	subq	$80,%rsp

	movq	%rdi,0(%rsp)

// X=1, Y=0
	movq	$1,%r8
	xorq	%r9,%r9
	xorq	%r10,%r10
	xorq	%r11,%r11
	xorq	%rdi,%rdi

	xorq	%r12,%r12
	xorq	%r13,%r13
	xorq	%r14,%r14
	xorq	%r15,%r15
	xorq	%rbp,%rbp

// Copy a/n into B/A on the stack.
	vmovdqu	0(%rsi),%xmm0
	vmovdqu	16(%rsi),%xmm1
	vmovdqu	%xmm0,48(%rsp)
	vmovdqu	%xmm1,64(%rsp)

	vmovdqu	0(%rdx),%xmm0
	vmovdqu	16(%rdx),%xmm1
	vmovdqu	%xmm0,16(%rsp)
	vmovdqu	%xmm1,32(%rsp)

L$beeu_loop:
	xorq	%rbx,%rbx
	orq	48(%rsp),%rbx
	orq	56(%rsp),%rbx
	orq	64(%rsp),%rbx
	orq	72(%rsp),%rbx
	jz	L$beeu_loop_end


// 0 < B < |n|,
// 0 < A <= |n|,
// (1)      X*a  ==  B   (mod |n|),
// (2) (-1)*Y*a  ==  A   (mod |n|)

// Now divide B by the maximum possible power of two in the
// integers, and divide X by the same value mod |n|. When we're
// done, (1) still holds.
	movq	$1,%rcx

// Note that B > 0
L$beeu_shift_loop_XB:
	movq	%rcx,%rbx
	andq	48(%rsp),%rbx
	jnz	L$beeu_shift_loop_end_XB

// Ensure X is even and divide by two.
	movq	$1,%rbx
	andq	%r8,%rbx
	jz	L$shift1_0
	addq	0(%rdx),%r8
	adcq	8(%rdx),%r9
	adcq	16(%rdx),%r10
	adcq	24(%rdx),%r11
	adcq	$0,%rdi

L$shift1_0:
	shrdq	$1,%r9,%r8
	shrdq	$1,%r10,%r9
	shrdq	$1,%r11,%r10
	shrdq	$1,%rdi,%r11
	shrq	$1,%rdi

	shlq	$1,%rcx

// Test wraparound of the shift parameter. The probability to have 32 zeroes
// in a row is small Therefore having the value below equal $0x8000000 or
// $0x8000 does not affect the performance. We choose 0x8000000 because it
// is the maximal immediate value possible.
	cmpq	$0x8000000,%rcx
	jne	L$beeu_shift_loop_XB

L$beeu_shift_loop_end_XB:
	bsfq	%rcx,%rcx
	testq	%rcx,%rcx
	jz	L$beeu_no_shift_XB

// Copy shifted values.
// Remember not to override t3=rcx
	movq	8+48(%rsp),%rax
	movq	16+48(%rsp),%rbx
	movq	24+48(%rsp),%rsi

	shrdq	%cl,%rax,0+48(%rsp)
	shrdq	%cl,%rbx,8+48(%rsp)
	shrdq	%cl,%rsi,16+48(%rsp)

	shrq	%cl,%rsi
	movq	%rsi,24+48(%rsp)


L$beeu_no_shift_XB:
// Same for A and Y.  Afterwards, (2) still holds.
	movq	$1,%rcx

// Note that A > 0
L$beeu_shift_loop_YA:
	movq	%rcx,%rbx
	andq	16(%rsp),%rbx
	jnz	L$beeu_shift_loop_end_YA

// Ensure X is even and divide by two.
	movq	$1,%rbx
	andq	%r12,%rbx
	jz	L$shift1_1
	addq	0(%rdx),%r12
	adcq	8(%rdx),%r13
	adcq	16(%rdx),%r14
	adcq	24(%rdx),%r15
	adcq	$0,%rbp

L$shift1_1:
	shrdq	$1,%r13,%r12
	shrdq	$1,%r14,%r13
	shrdq	$1,%r15,%r14
	shrdq	$1,%rbp,%r15
	shrq	$1,%rbp

	shlq	$1,%rcx

// Test wraparound of the shift parameter. The probability to have 32 zeroes
// in a row is small therefore having the value below equal $0x8000000 or
// $0x8000 Does not affect the performance. We choose 0x8000000 because it
// is the maximal immediate value possible.
	cmpq	$0x8000000,%rcx
	jne	L$beeu_shift_loop_YA

L$beeu_shift_loop_end_YA:
	bsfq	%rcx,%rcx
	testq	%rcx,%rcx
	jz	L$beeu_no_shift_YA

// Copy shifted values.
// Remember not to override t3=rcx
	movq	8+16(%rsp),%rax
	movq	16+16(%rsp),%rbx
	movq	24+16(%rsp),%rsi

	shrdq	%cl,%rax,0+16(%rsp)
	shrdq	%cl,%rbx,8+16(%rsp)
	shrdq	%cl,%rsi,16+16(%rsp)

	shrq	%cl,%rsi
	movq	%rsi,24+16(%rsp)


L$beeu_no_shift_YA:
// T = B-A (A,B < 2^256)
	movq	48(%rsp),%rax
	movq	56(%rsp),%rbx
	movq	64(%rsp),%rsi
	movq	72(%rsp),%rcx
	subq	16(%rsp),%rax
	sbbq	24(%rsp),%rbx
	sbbq	32(%rsp),%rsi
	sbbq	40(%rsp),%rcx  // borrow from shift
	jnc	L$beeu_B_bigger_than_A

// A = A - B
	movq	16(%rsp),%rax
	movq	24(%rsp),%rbx
	movq	32(%rsp),%rsi
	movq	40(%rsp),%rcx
	subq	48(%rsp),%rax
	sbbq	56(%rsp),%rbx
	sbbq	64(%rsp),%rsi
	sbbq	72(%rsp),%rcx
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	movq	%rsi,32(%rsp)
	movq	%rcx,40(%rsp)

// Y = Y + X
	addq	%r8,%r12
	adcq	%r9,%r13
	adcq	%r10,%r14
	adcq	%r11,%r15
	adcq	%rdi,%rbp
	jmp	L$beeu_loop

L$beeu_B_bigger_than_A:
// B = T = B - A
	movq	%rax,48(%rsp)
	movq	%rbx,56(%rsp)
	movq	%rsi,64(%rsp)
	movq	%rcx,72(%rsp)

// X = Y + X
	addq	%r12,%r8
	adcq	%r13,%r9
	adcq	%r14,%r10
	adcq	%r15,%r11
	adcq	%rbp,%rdi

	jmp	L$beeu_loop

L$beeu_loop_end:
// The Euclid's algorithm loop ends when A == beeu(a,n);
// Therefore (-1)*Y*a == A (mod |n|), Y>0

// Verify that A = 1 ==> (-1)*Y*a = A = 1  (mod |n|)
	movq	16(%rsp),%rbx
	subq	$1,%rbx
	orq	24(%rsp),%rbx
	orq	32(%rsp),%rbx
	orq	40(%rsp),%rbx
// If not, fail.
	jnz	L$beeu_err

// From this point on, we no longer need X
// Therefore we use it as a temporary storage.
// X = n
	movq	0(%rdx),%r8
	movq	8(%rdx),%r9
	movq	16(%rdx),%r10
	movq	24(%rdx),%r11
	xorq	%rdi,%rdi

L$beeu_reduction_loop:
	movq	%r12,16(%rsp)
	movq	%r13,24(%rsp)
	movq	%r14,32(%rsp)
	movq	%r15,40(%rsp)
	movq	%rbp,48(%rsp)

// If Y>n ==> Y=Y-n
	subq	%r8,%r12
	sbbq	%r9,%r13
	sbbq	%r10,%r14
	sbbq	%r11,%r15
	sbbq	$0,%rbp

// Choose old Y or new Y
	cmovcq	16(%rsp),%r12
	cmovcq	24(%rsp),%r13
	cmovcq	32(%rsp),%r14
	cmovcq	40(%rsp),%r15
	jnc	L$beeu_reduction_loop

// X = n - Y (n, Y < 2^256), (Cancel the (-1))
	subq	%r12,%r8
	sbbq	%r13,%r9
	sbbq	%r14,%r10
	sbbq	%r15,%r11

L$beeu_save:
// Save the inverse(<2^256) to out.
	movq	0(%rsp),%rdi

	movq	%r8,0(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

// Return 1.
	movq	$1,%rax
	jmp	L$beeu_finish

L$beeu_err:
// Return 0.
	xorq	%rax,%rax

L$beeu_finish:
	addq	$80,%rsp

	popq	%rsi

	popq	%rbx

	popq	%r15

	popq	%r14

	popq	%r13

	popq	%r12

	popq	%rbp

	ret



#endif
