// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	

// abi_test_trampoline loads callee-saved registers from |state|, calls |func|
// with |argv|, then saves the callee-saved registers into |state|. It returns
// the result of |func|. If |unwind| is non-zero, this function triggers unwind
// instrumentation.
// uint64_t abi_test_trampoline(void (*func)(...), CallerState *state,
// const uint64_t *argv, size_t argc,
// int unwind);
.globl	_abi_test_trampoline
.private_extern _abi_test_trampoline

.p2align	4
_abi_test_trampoline:


_CET_ENDBR
// Stack layout:
// 8 bytes - align
// 48 bytes - saved caller registers
// 8 bytes - scratch space
// 8 bytes - saved copy of $unwind (SysV-only)
// 8 bytes - saved copy of $state
// 8 bytes - saved copy of $func
// 8 bytes - if needed for stack alignment
// 8*4 bytes - parameters for $func
	subq	$120,%rsp


	movq	%r8,48(%rsp)
	movq	%rbx,64(%rsp)


	movq	%rbp,72(%rsp)


	movq	%r12,80(%rsp)


	movq	%r13,88(%rsp)


	movq	%r14,96(%rsp)


	movq	%r15,104(%rsp)



	movq	0(%rsi),%rbx
	movq	8(%rsi),%rbp
	movq	16(%rsi),%r12
	movq	24(%rsi),%r13
	movq	32(%rsi),%r14
	movq	40(%rsi),%r15
// Stash $func and $state, so they are available after the call returns.
	movq	%rdi,32(%rsp)
	movq	%rsi,40(%rsp)

// Load parameters. Note this will clobber $argv and $argc, so we can
// only use non-parameter volatile registers. There are three, and they
// are the same between SysV and Win64: %rax, %r10, and %r11.
	movq	%rdx,%r10
	movq	%rcx,%r11
	decq	%r11
	js	L$args_done
	movq	(%r10),%rdi
	addq	$8,%r10
	decq	%r11
	js	L$args_done
	movq	(%r10),%rsi
	addq	$8,%r10
	decq	%r11
	js	L$args_done
	movq	(%r10),%rdx
	addq	$8,%r10
	decq	%r11
	js	L$args_done
	movq	(%r10),%rcx
	addq	$8,%r10
	decq	%r11
	js	L$args_done
	movq	(%r10),%r8
	addq	$8,%r10
	decq	%r11
	js	L$args_done
	movq	(%r10),%r9
	addq	$8,%r10
	leaq	0(%rsp),%rax
L$args_loop:
	decq	%r11
	js	L$args_done

// This block should be:
// movq (%r10), %rtmp
// movq %rtmp, (%rax)
// There are no spare registers available, so we spill into the scratch
// space.
	movq	%r11,56(%rsp)
	movq	(%r10),%r11
	movq	%r11,(%rax)
	movq	56(%rsp),%r11

	addq	$8,%r10
	addq	$8,%rax
	jmp	L$args_loop

L$args_done:
	movq	32(%rsp),%rax
	movq	48(%rsp),%r10
	testq	%r10,%r10
	jz	L$no_unwind

// Set the trap flag.
	pushfq
	orq	$0x100,0(%rsp)
	popfq

// Run an instruction to trigger a breakpoint immediately before the
// call.
	nop
.globl	_abi_test_unwind_start
.private_extern _abi_test_unwind_start
.alt_entry _abi_test_unwind_start
_abi_test_unwind_start:

	call	*%rax
.globl	_abi_test_unwind_return
.private_extern _abi_test_unwind_return
.alt_entry _abi_test_unwind_return
_abi_test_unwind_return:

// Clear the trap flag. Note this assumes the trap flag was clear on
// entry. We do not support instrumenting an unwind-instrumented
// |abi_test_trampoline|.
	pushfq
	andq	$-0x101,0(%rsp)  // -0x101 is ~0x100
	popfq
.globl	_abi_test_unwind_stop
.private_extern _abi_test_unwind_stop
.alt_entry _abi_test_unwind_stop
_abi_test_unwind_stop:

	jmp	L$call_done

L$no_unwind:
	call	*%rax

L$call_done:
// Store what $func did our state, so our caller can check.
	movq	40(%rsp),%rsi
	movq	%rbx,0(%rsi)
	movq	%rbp,8(%rsi)
	movq	%r12,16(%rsi)
	movq	%r13,24(%rsi)
	movq	%r14,32(%rsi)
	movq	%r15,40(%rsi)
	movq	64(%rsp),%rbx

	movq	72(%rsp),%rbp

	movq	80(%rsp),%r12

	movq	88(%rsp),%r13

	movq	96(%rsp),%r14

	movq	104(%rsp),%r15

	addq	$120,%rsp


// %rax already contains $func's return value, unmodified.
	ret



.globl	_abi_test_clobber_rax
.private_extern _abi_test_clobber_rax

.p2align	4
.alt_entry _abi_test_clobber_rax
_abi_test_clobber_rax:
_CET_ENDBR
	xorq	%rax,%rax
	ret

.globl	_abi_test_clobber_rbx
.private_extern _abi_test_clobber_rbx

.p2align	4
.alt_entry _abi_test_clobber_rbx
_abi_test_clobber_rbx:
_CET_ENDBR
	xorq	%rbx,%rbx
	ret

.globl	_abi_test_clobber_rcx
.private_extern _abi_test_clobber_rcx

.p2align	4
.alt_entry _abi_test_clobber_rcx
_abi_test_clobber_rcx:
_CET_ENDBR
	xorq	%rcx,%rcx
	ret

.globl	_abi_test_clobber_rdx
.private_extern _abi_test_clobber_rdx

.p2align	4
.alt_entry _abi_test_clobber_rdx
_abi_test_clobber_rdx:
_CET_ENDBR
	xorq	%rdx,%rdx
	ret

.globl	_abi_test_clobber_rdi
.private_extern _abi_test_clobber_rdi

.p2align	4
.alt_entry _abi_test_clobber_rdi
_abi_test_clobber_rdi:
_CET_ENDBR
	xorq	%rdi,%rdi
	ret

.globl	_abi_test_clobber_rsi
.private_extern _abi_test_clobber_rsi

.p2align	4
.alt_entry _abi_test_clobber_rsi
_abi_test_clobber_rsi:
_CET_ENDBR
	xorq	%rsi,%rsi
	ret

.globl	_abi_test_clobber_rbp
.private_extern _abi_test_clobber_rbp

.p2align	4
.alt_entry _abi_test_clobber_rbp
_abi_test_clobber_rbp:
_CET_ENDBR
	xorq	%rbp,%rbp
	ret

.globl	_abi_test_clobber_r8
.private_extern _abi_test_clobber_r8

.p2align	4
.alt_entry _abi_test_clobber_r8
_abi_test_clobber_r8:
_CET_ENDBR
	xorq	%r8,%r8
	ret

.globl	_abi_test_clobber_r9
.private_extern _abi_test_clobber_r9

.p2align	4
.alt_entry _abi_test_clobber_r9
_abi_test_clobber_r9:
_CET_ENDBR
	xorq	%r9,%r9
	ret

.globl	_abi_test_clobber_r10
.private_extern _abi_test_clobber_r10

.p2align	4
.alt_entry _abi_test_clobber_r10
_abi_test_clobber_r10:
_CET_ENDBR
	xorq	%r10,%r10
	ret

.globl	_abi_test_clobber_r11
.private_extern _abi_test_clobber_r11

.p2align	4
.alt_entry _abi_test_clobber_r11
_abi_test_clobber_r11:
_CET_ENDBR
	xorq	%r11,%r11
	ret

.globl	_abi_test_clobber_r12
.private_extern _abi_test_clobber_r12

.p2align	4
.alt_entry _abi_test_clobber_r12
_abi_test_clobber_r12:
_CET_ENDBR
	xorq	%r12,%r12
	ret

.globl	_abi_test_clobber_r13
.private_extern _abi_test_clobber_r13

.p2align	4
.alt_entry _abi_test_clobber_r13
_abi_test_clobber_r13:
_CET_ENDBR
	xorq	%r13,%r13
	ret

.globl	_abi_test_clobber_r14
.private_extern _abi_test_clobber_r14

.p2align	4
.alt_entry _abi_test_clobber_r14
_abi_test_clobber_r14:
_CET_ENDBR
	xorq	%r14,%r14
	ret

.globl	_abi_test_clobber_r15
.private_extern _abi_test_clobber_r15

.p2align	4
.alt_entry _abi_test_clobber_r15
_abi_test_clobber_r15:
_CET_ENDBR
	xorq	%r15,%r15
	ret

.globl	_abi_test_clobber_xmm0
.private_extern _abi_test_clobber_xmm0

.p2align	4
.alt_entry _abi_test_clobber_xmm0
_abi_test_clobber_xmm0:
_CET_ENDBR
	pxor	%xmm0,%xmm0
	ret

.globl	_abi_test_clobber_xmm1
.private_extern _abi_test_clobber_xmm1

.p2align	4
.alt_entry _abi_test_clobber_xmm1
_abi_test_clobber_xmm1:
_CET_ENDBR
	pxor	%xmm1,%xmm1
	ret

.globl	_abi_test_clobber_xmm2
.private_extern _abi_test_clobber_xmm2

.p2align	4
.alt_entry _abi_test_clobber_xmm2
_abi_test_clobber_xmm2:
_CET_ENDBR
	pxor	%xmm2,%xmm2
	ret

.globl	_abi_test_clobber_xmm3
.private_extern _abi_test_clobber_xmm3

.p2align	4
.alt_entry _abi_test_clobber_xmm3
_abi_test_clobber_xmm3:
_CET_ENDBR
	pxor	%xmm3,%xmm3
	ret

.globl	_abi_test_clobber_xmm4
.private_extern _abi_test_clobber_xmm4

.p2align	4
.alt_entry _abi_test_clobber_xmm4
_abi_test_clobber_xmm4:
_CET_ENDBR
	pxor	%xmm4,%xmm4
	ret

.globl	_abi_test_clobber_xmm5
.private_extern _abi_test_clobber_xmm5

.p2align	4
.alt_entry _abi_test_clobber_xmm5
_abi_test_clobber_xmm5:
_CET_ENDBR
	pxor	%xmm5,%xmm5
	ret

.globl	_abi_test_clobber_xmm6
.private_extern _abi_test_clobber_xmm6

.p2align	4
.alt_entry _abi_test_clobber_xmm6
_abi_test_clobber_xmm6:
_CET_ENDBR
	pxor	%xmm6,%xmm6
	ret

.globl	_abi_test_clobber_xmm7
.private_extern _abi_test_clobber_xmm7

.p2align	4
.alt_entry _abi_test_clobber_xmm7
_abi_test_clobber_xmm7:
_CET_ENDBR
	pxor	%xmm7,%xmm7
	ret

.globl	_abi_test_clobber_xmm8
.private_extern _abi_test_clobber_xmm8

.p2align	4
.alt_entry _abi_test_clobber_xmm8
_abi_test_clobber_xmm8:
_CET_ENDBR
	pxor	%xmm8,%xmm8
	ret

.globl	_abi_test_clobber_xmm9
.private_extern _abi_test_clobber_xmm9

.p2align	4
.alt_entry _abi_test_clobber_xmm9
_abi_test_clobber_xmm9:
_CET_ENDBR
	pxor	%xmm9,%xmm9
	ret

.globl	_abi_test_clobber_xmm10
.private_extern _abi_test_clobber_xmm10

.p2align	4
.alt_entry _abi_test_clobber_xmm10
_abi_test_clobber_xmm10:
_CET_ENDBR
	pxor	%xmm10,%xmm10
	ret

.globl	_abi_test_clobber_xmm11
.private_extern _abi_test_clobber_xmm11

.p2align	4
.alt_entry _abi_test_clobber_xmm11
_abi_test_clobber_xmm11:
_CET_ENDBR
	pxor	%xmm11,%xmm11
	ret

.globl	_abi_test_clobber_xmm12
.private_extern _abi_test_clobber_xmm12

.p2align	4
.alt_entry _abi_test_clobber_xmm12
_abi_test_clobber_xmm12:
_CET_ENDBR
	pxor	%xmm12,%xmm12
	ret

.globl	_abi_test_clobber_xmm13
.private_extern _abi_test_clobber_xmm13

.p2align	4
.alt_entry _abi_test_clobber_xmm13
_abi_test_clobber_xmm13:
_CET_ENDBR
	pxor	%xmm13,%xmm13
	ret

.globl	_abi_test_clobber_xmm14
.private_extern _abi_test_clobber_xmm14

.p2align	4
.alt_entry _abi_test_clobber_xmm14
_abi_test_clobber_xmm14:
_CET_ENDBR
	pxor	%xmm14,%xmm14
	ret

.globl	_abi_test_clobber_xmm15
.private_extern _abi_test_clobber_xmm15

.p2align	4
.alt_entry _abi_test_clobber_xmm15
_abi_test_clobber_xmm15:
_CET_ENDBR
	pxor	%xmm15,%xmm15
	ret

// abi_test_bad_unwind_wrong_register preserves the ABI, but annotates the wrong
// register in unwind metadata.
// void abi_test_bad_unwind_wrong_register(void);
.globl	_abi_test_bad_unwind_wrong_register
.private_extern _abi_test_bad_unwind_wrong_register

.p2align	4
.alt_entry _abi_test_bad_unwind_wrong_register
_abi_test_bad_unwind_wrong_register:


_CET_ENDBR
	pushq	%r12
// This should be %r13
// This should be %r13

// Windows evaluates epilogs directly in the unwinder, rather than using
// unwind codes. Add a nop so there is one non-epilog point (immediately
// before the nop) where the unwinder can observe the mistake.
	nop
	popq	%r12

	ret




// abi_test_bad_unwind_temporary preserves the ABI, but temporarily corrupts the
// storage space for a saved register, breaking unwind.
// void abi_test_bad_unwind_temporary(void);
.globl	_abi_test_bad_unwind_temporary
.private_extern _abi_test_bad_unwind_temporary

.p2align	4
.alt_entry _abi_test_bad_unwind_temporary
_abi_test_bad_unwind_temporary:


_CET_ENDBR
	pushq	%r12




	movq	%r12,%rax
	incq	%rax
	movq	%rax,(%rsp)
// Unwinding from here is incorrect. Although %r12 itself has not been
// changed, the unwind codes say to look in (%rsp) instead.

	movq	%r12,(%rsp)
// Unwinding is now fixed.

	popq	%r12

	ret




// abi_test_get_and_clear_direction_flag clears the direction flag. If the flag
// was previously set, it returns one. Otherwise, it returns zero.
// int abi_test_get_and_clear_direction_flag(void);
.globl	_abi_test_get_and_clear_direction_flag
.private_extern _abi_test_get_and_clear_direction_flag

.alt_entry _abi_test_get_and_clear_direction_flag
_abi_test_get_and_clear_direction_flag:
_CET_ENDBR
	pushfq
	popq	%rax
	andq	$0x400,%rax
	shrq	$10,%rax
	cld
	ret


// abi_test_set_direction_flag sets the direction flag.
// void abi_test_set_direction_flag(void);
.globl	_abi_test_set_direction_flag
.private_extern _abi_test_set_direction_flag

.alt_entry _abi_test_set_direction_flag
_abi_test_set_direction_flag:
_CET_ENDBR
	std
	ret

#endif
