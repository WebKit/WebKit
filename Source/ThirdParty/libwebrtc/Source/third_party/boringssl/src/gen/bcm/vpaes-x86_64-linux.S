// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__ELF__)
.text	

//  
// _aes_encrypt_core 
//  
// AES-encrypt %xmm0. 
//  
// Inputs: 
// %xmm0 = input 
// %xmm9-%xmm15 as in _vpaes_preheat 
// (%rdx) = scheduled keys 
//  
// Output in %xmm0 
// Clobbers  %xmm1-%xmm5, %r9, %r10, %r11, %rax 
// Preserves %xmm6 - %xmm8 so you get some local vectors 
//  
//  
.type	_vpaes_encrypt_core,@function
.align	16
_vpaes_encrypt_core:
.cfi_startproc	
	movq	%rdx,%r9
	movq	$16,%r11
	movl	240(%rdx),%eax
	movdqa	%xmm9,%xmm1
	movdqa	.Lk_ipt(%rip),%xmm2  // iptlo
	pandn	%xmm0,%xmm1
	movdqu	(%r9),%xmm5  // round0 key
	psrld	$4,%xmm1
	pand	%xmm9,%xmm0
	pshufb	%xmm0,%xmm2
	movdqa	.Lk_ipt+16(%rip),%xmm0  // ipthi
	pshufb	%xmm1,%xmm0
	pxor	%xmm5,%xmm2
	addq	$16,%r9
	pxor	%xmm2,%xmm0
	leaq	.Lk_mc_backward(%rip),%r10
	jmp	.Lenc_entry

.align	16
.Lenc_loop:
// middle of middle round
	movdqa	%xmm13,%xmm4  // 4 : sb1u
	movdqa	%xmm12,%xmm0  // 0 : sb1t
	pshufb	%xmm2,%xmm4  // 4 = sb1u
	pshufb	%xmm3,%xmm0  // 0 = sb1t
	pxor	%xmm5,%xmm4  // 4 = sb1u + k
	movdqa	%xmm15,%xmm5  // 4 : sb2u
	pxor	%xmm4,%xmm0  // 0 = A
	movdqa	-64(%r11,%r10,1),%xmm1  // .Lk_mc_forward[]
	pshufb	%xmm2,%xmm5  // 4 = sb2u
	movdqa	(%r11,%r10,1),%xmm4  // .Lk_mc_backward[]
	movdqa	%xmm14,%xmm2  // 2 : sb2t
	pshufb	%xmm3,%xmm2  // 2 = sb2t
	movdqa	%xmm0,%xmm3  // 3 = A
	pxor	%xmm5,%xmm2  // 2 = 2A
	pshufb	%xmm1,%xmm0  // 0 = B
	addq	$16,%r9  // next key
	pxor	%xmm2,%xmm0  // 0 = 2A+B
	pshufb	%xmm4,%xmm3  // 3 = D
	addq	$16,%r11  // next mc
	pxor	%xmm0,%xmm3  // 3 = 2A+B+D
	pshufb	%xmm1,%xmm0  // 0 = 2B+C
	andq	$0x30,%r11  // ... mod 4
	subq	$1,%rax  // nr--
	pxor	%xmm3,%xmm0  // 0 = 2A+3B+C+D

.Lenc_entry:
// top of round
	movdqa	%xmm9,%xmm1  // 1 : i
	movdqa	%xmm11,%xmm5  // 2 : a/k
	pandn	%xmm0,%xmm1  // 1 = i<<4
	psrld	$4,%xmm1  // 1 = i
	pand	%xmm9,%xmm0  // 0 = k
	pshufb	%xmm0,%xmm5  // 2 = a/k
	movdqa	%xmm10,%xmm3  // 3 : 1/i
	pxor	%xmm1,%xmm0  // 0 = j
	pshufb	%xmm1,%xmm3  // 3 = 1/i
	movdqa	%xmm10,%xmm4  // 4 : 1/j
	pxor	%xmm5,%xmm3  // 3 = iak = 1/i + a/k
	pshufb	%xmm0,%xmm4  // 4 = 1/j
	movdqa	%xmm10,%xmm2  // 2 : 1/iak
	pxor	%xmm5,%xmm4  // 4 = jak = 1/j + a/k
	pshufb	%xmm3,%xmm2  // 2 = 1/iak
	movdqa	%xmm10,%xmm3  // 3 : 1/jak
	pxor	%xmm0,%xmm2  // 2 = io
	pshufb	%xmm4,%xmm3  // 3 = 1/jak
	movdqu	(%r9),%xmm5
	pxor	%xmm1,%xmm3  // 3 = jo
	jnz	.Lenc_loop

// middle of last round
	movdqa	-96(%r10),%xmm4  // 3 : sbou	.Lk_sbo
	movdqa	-80(%r10),%xmm0  // 0 : sbot	.Lk_sbo+16
	pshufb	%xmm2,%xmm4  // 4 = sbou
	pxor	%xmm5,%xmm4  // 4 = sb1u + k
	pshufb	%xmm3,%xmm0  // 0 = sb1t
	movdqa	64(%r11,%r10,1),%xmm1  // .Lk_sr[]
	pxor	%xmm4,%xmm0  // 0 = A
	pshufb	%xmm1,%xmm0
	ret
.cfi_endproc	
.size	_vpaes_encrypt_core,.-_vpaes_encrypt_core

//  
// _aes_encrypt_core_2x 
//  
// AES-encrypt %xmm0 and %xmm6 in parallel. 
//  
// Inputs: 
// %xmm0 and %xmm6 = input 
// %xmm9 and %xmm10 as in _vpaes_preheat 
// (%rdx) = scheduled keys 
//  
// Output in %xmm0 and %xmm6 
// Clobbers  %xmm1-%xmm5, %xmm7, %xmm8, %xmm11-%xmm13, %r9, %r10, %r11, %rax 
// Preserves %xmm14 and %xmm15 
//  
// This function stitches two parallel instances of _vpaes_encrypt_core. x86_64 
// provides 16 XMM registers. _vpaes_encrypt_core computes over six registers 
// (%xmm0-%xmm5) and additionally uses seven registers with preloaded constants 
// from _vpaes_preheat (%xmm9-%xmm15). This does not quite fit two instances, 
// so we spill some of %xmm9 through %xmm15 back to memory. We keep %xmm9 and 
// %xmm10 in registers as these values are used several times in a row. The 
// remainder are read once per round and are spilled to memory. This leaves two 
// registers preserved for the caller. 
//  
// Thus, of the two _vpaes_encrypt_core instances, the first uses (%xmm0-%xmm5) 
// as before. The second uses %xmm6-%xmm8,%xmm11-%xmm13. (Add 6 to %xmm2 and 
// below. Add 8 to %xmm3 and up.) Instructions in the second instance are 
// indented by one space. 
//  
//  
.type	_vpaes_encrypt_core_2x,@function
.align	16
_vpaes_encrypt_core_2x:
.cfi_startproc	
	movq	%rdx,%r9
	movq	$16,%r11
	movl	240(%rdx),%eax
	movdqa	%xmm9,%xmm1
	movdqa	%xmm9,%xmm7
	movdqa	.Lk_ipt(%rip),%xmm2  // iptlo
	movdqa	%xmm2,%xmm8
	pandn	%xmm0,%xmm1
	pandn	%xmm6,%xmm7
	movdqu	(%r9),%xmm5  // round0 key
// Also use %xmm5 in the second instance.
	psrld	$4,%xmm1
	psrld	$4,%xmm7
	pand	%xmm9,%xmm0
	pand	%xmm9,%xmm6
	pshufb	%xmm0,%xmm2
	pshufb	%xmm6,%xmm8
	movdqa	.Lk_ipt+16(%rip),%xmm0  // ipthi
	movdqa	%xmm0,%xmm6
	pshufb	%xmm1,%xmm0
	pshufb	%xmm7,%xmm6
	pxor	%xmm5,%xmm2
	pxor	%xmm5,%xmm8
	addq	$16,%r9
	pxor	%xmm2,%xmm0
	pxor	%xmm8,%xmm6
	leaq	.Lk_mc_backward(%rip),%r10
	jmp	.Lenc2x_entry

.align	16
.Lenc2x_loop:
// middle of middle round
	movdqa	.Lk_sb1(%rip),%xmm4  // 4 : sb1u
	movdqa	.Lk_sb1+16(%rip),%xmm0  // 0 : sb1t
	movdqa	%xmm4,%xmm12
	movdqa	%xmm0,%xmm6
	pshufb	%xmm2,%xmm4  // 4 = sb1u
	pshufb	%xmm8,%xmm12
	pshufb	%xmm3,%xmm0  // 0 = sb1t
	pshufb	%xmm11,%xmm6
	pxor	%xmm5,%xmm4  // 4 = sb1u + k
	pxor	%xmm5,%xmm12
	movdqa	.Lk_sb2(%rip),%xmm5  // 4 : sb2u
	movdqa	%xmm5,%xmm13
	pxor	%xmm4,%xmm0  // 0 = A
	pxor	%xmm12,%xmm6
	movdqa	-64(%r11,%r10,1),%xmm1  // .Lk_mc_forward[]
// Also use %xmm1 in the second instance.
	pshufb	%xmm2,%xmm5  // 4 = sb2u
	pshufb	%xmm8,%xmm13
	movdqa	(%r11,%r10,1),%xmm4  // .Lk_mc_backward[]
// Also use %xmm4 in the second instance.
	movdqa	.Lk_sb2+16(%rip),%xmm2  // 2 : sb2t
	movdqa	%xmm2,%xmm8
	pshufb	%xmm3,%xmm2  // 2 = sb2t
	pshufb	%xmm11,%xmm8
	movdqa	%xmm0,%xmm3  // 3 = A
	movdqa	%xmm6,%xmm11
	pxor	%xmm5,%xmm2  // 2 = 2A
	pxor	%xmm13,%xmm8
	pshufb	%xmm1,%xmm0  // 0 = B
	pshufb	%xmm1,%xmm6
	addq	$16,%r9  // next key
	pxor	%xmm2,%xmm0  // 0 = 2A+B
	pxor	%xmm8,%xmm6
	pshufb	%xmm4,%xmm3  // 3 = D
	pshufb	%xmm4,%xmm11
	addq	$16,%r11  // next mc
	pxor	%xmm0,%xmm3  // 3 = 2A+B+D
	pxor	%xmm6,%xmm11
	pshufb	%xmm1,%xmm0  // 0 = 2B+C
	pshufb	%xmm1,%xmm6
	andq	$0x30,%r11  // ... mod 4
	subq	$1,%rax  // nr--
	pxor	%xmm3,%xmm0  // 0 = 2A+3B+C+D
	pxor	%xmm11,%xmm6

.Lenc2x_entry:
// top of round
	movdqa	%xmm9,%xmm1  // 1 : i
	movdqa	%xmm9,%xmm7
	movdqa	.Lk_inv+16(%rip),%xmm5  // 2 : a/k
	movdqa	%xmm5,%xmm13
	pandn	%xmm0,%xmm1  // 1 = i<<4
	pandn	%xmm6,%xmm7
	psrld	$4,%xmm1  // 1 = i
	psrld	$4,%xmm7
	pand	%xmm9,%xmm0  // 0 = k
	pand	%xmm9,%xmm6
	pshufb	%xmm0,%xmm5  // 2 = a/k
	pshufb	%xmm6,%xmm13
	movdqa	%xmm10,%xmm3  // 3 : 1/i
	movdqa	%xmm10,%xmm11
	pxor	%xmm1,%xmm0  // 0 = j
	pxor	%xmm7,%xmm6
	pshufb	%xmm1,%xmm3  // 3 = 1/i
	pshufb	%xmm7,%xmm11
	movdqa	%xmm10,%xmm4  // 4 : 1/j
	movdqa	%xmm10,%xmm12
	pxor	%xmm5,%xmm3  // 3 = iak = 1/i + a/k
	pxor	%xmm13,%xmm11
	pshufb	%xmm0,%xmm4  // 4 = 1/j
	pshufb	%xmm6,%xmm12
	movdqa	%xmm10,%xmm2  // 2 : 1/iak
	movdqa	%xmm10,%xmm8
	pxor	%xmm5,%xmm4  // 4 = jak = 1/j + a/k
	pxor	%xmm13,%xmm12
	pshufb	%xmm3,%xmm2  // 2 = 1/iak
	pshufb	%xmm11,%xmm8
	movdqa	%xmm10,%xmm3  // 3 : 1/jak
	movdqa	%xmm10,%xmm11
	pxor	%xmm0,%xmm2  // 2 = io
	pxor	%xmm6,%xmm8
	pshufb	%xmm4,%xmm3  // 3 = 1/jak
	pshufb	%xmm12,%xmm11
	movdqu	(%r9),%xmm5
// Also use %xmm5 in the second instance.
	pxor	%xmm1,%xmm3  // 3 = jo
	pxor	%xmm7,%xmm11
	jnz	.Lenc2x_loop

// middle of last round
	movdqa	-96(%r10),%xmm4  // 3 : sbou	.Lk_sbo
	movdqa	-80(%r10),%xmm0  // 0 : sbot	.Lk_sbo+16
	movdqa	%xmm4,%xmm12
	movdqa	%xmm0,%xmm6
	pshufb	%xmm2,%xmm4  // 4 = sbou
	pshufb	%xmm8,%xmm12
	pxor	%xmm5,%xmm4  // 4 = sb1u + k
	pxor	%xmm5,%xmm12
	pshufb	%xmm3,%xmm0  // 0 = sb1t
	pshufb	%xmm11,%xmm6
	movdqa	64(%r11,%r10,1),%xmm1  // .Lk_sr[]
// Also use %xmm1 in the second instance.
	pxor	%xmm4,%xmm0  // 0 = A
	pxor	%xmm12,%xmm6
	pshufb	%xmm1,%xmm0
	pshufb	%xmm1,%xmm6
	ret
.cfi_endproc	
.size	_vpaes_encrypt_core_2x,.-_vpaes_encrypt_core_2x

//  
// Decryption core 
//  
// Same API as encryption core. 
//  
.type	_vpaes_decrypt_core,@function
.align	16
_vpaes_decrypt_core:
.cfi_startproc	
	movq	%rdx,%r9  // load key
	movl	240(%rdx),%eax
	movdqa	%xmm9,%xmm1
	movdqa	.Lk_dipt(%rip),%xmm2  // iptlo
	pandn	%xmm0,%xmm1
	movq	%rax,%r11
	psrld	$4,%xmm1
	movdqu	(%r9),%xmm5  // round0 key
	shlq	$4,%r11
	pand	%xmm9,%xmm0
	pshufb	%xmm0,%xmm2
	movdqa	.Lk_dipt+16(%rip),%xmm0  // ipthi
	xorq	$0x30,%r11
	leaq	.Lk_dsbd(%rip),%r10
	pshufb	%xmm1,%xmm0
	andq	$0x30,%r11
	pxor	%xmm5,%xmm2
	movdqa	.Lk_mc_forward+48(%rip),%xmm5
	pxor	%xmm2,%xmm0
	addq	$16,%r9
	addq	%r10,%r11
	jmp	.Ldec_entry

.align	16
.Ldec_loop:
//  
// Inverse mix columns 
//  
	movdqa	-32(%r10),%xmm4  // 4 : sb9u
	movdqa	-16(%r10),%xmm1  // 0 : sb9t
	pshufb	%xmm2,%xmm4  // 4 = sb9u
	pshufb	%xmm3,%xmm1  // 0 = sb9t
	pxor	%xmm4,%xmm0
	movdqa	0(%r10),%xmm4  // 4 : sbdu
	pxor	%xmm1,%xmm0  // 0 = ch
	movdqa	16(%r10),%xmm1  // 0 : sbdt

	pshufb	%xmm2,%xmm4  // 4 = sbdu
	pshufb	%xmm5,%xmm0  // MC ch
	pshufb	%xmm3,%xmm1  // 0 = sbdt
	pxor	%xmm4,%xmm0  // 4 = ch
	movdqa	32(%r10),%xmm4  // 4 : sbbu
	pxor	%xmm1,%xmm0  // 0 = ch
	movdqa	48(%r10),%xmm1  // 0 : sbbt

	pshufb	%xmm2,%xmm4  // 4 = sbbu
	pshufb	%xmm5,%xmm0  // MC ch
	pshufb	%xmm3,%xmm1  // 0 = sbbt
	pxor	%xmm4,%xmm0  // 4 = ch
	movdqa	64(%r10),%xmm4  // 4 : sbeu
	pxor	%xmm1,%xmm0  // 0 = ch
	movdqa	80(%r10),%xmm1  // 0 : sbet

	pshufb	%xmm2,%xmm4  // 4 = sbeu
	pshufb	%xmm5,%xmm0  // MC ch
	pshufb	%xmm3,%xmm1  // 0 = sbet
	pxor	%xmm4,%xmm0  // 4 = ch
	addq	$16,%r9  // next round key
	palignr	$12,%xmm5,%xmm5
	pxor	%xmm1,%xmm0  // 0 = ch
	subq	$1,%rax  // nr--

.Ldec_entry:
// top of round
	movdqa	%xmm9,%xmm1  // 1 : i
	pandn	%xmm0,%xmm1  // 1 = i<<4
	movdqa	%xmm11,%xmm2  // 2 : a/k
	psrld	$4,%xmm1  // 1 = i
	pand	%xmm9,%xmm0  // 0 = k
	pshufb	%xmm0,%xmm2  // 2 = a/k
	movdqa	%xmm10,%xmm3  // 3 : 1/i
	pxor	%xmm1,%xmm0  // 0 = j
	pshufb	%xmm1,%xmm3  // 3 = 1/i
	movdqa	%xmm10,%xmm4  // 4 : 1/j
	pxor	%xmm2,%xmm3  // 3 = iak = 1/i + a/k
	pshufb	%xmm0,%xmm4  // 4 = 1/j
	pxor	%xmm2,%xmm4  // 4 = jak = 1/j + a/k
	movdqa	%xmm10,%xmm2  // 2 : 1/iak
	pshufb	%xmm3,%xmm2  // 2 = 1/iak
	movdqa	%xmm10,%xmm3  // 3 : 1/jak
	pxor	%xmm0,%xmm2  // 2 = io
	pshufb	%xmm4,%xmm3  // 3 = 1/jak
	movdqu	(%r9),%xmm0
	pxor	%xmm1,%xmm3  // 3 = jo
	jnz	.Ldec_loop

// middle of last round
	movdqa	96(%r10),%xmm4  // 3 : sbou
	pshufb	%xmm2,%xmm4  // 4 = sbou
	pxor	%xmm0,%xmm4  // 4 = sb1u + k
	movdqa	112(%r10),%xmm0  // 0 : sbot
	movdqa	-352(%r11),%xmm2  // .Lk_sr-.Lk_dsbd=-0x160
	pshufb	%xmm3,%xmm0  // 0 = sb1t
	pxor	%xmm4,%xmm0  // 0 = A
	pshufb	%xmm2,%xmm0
	ret
.cfi_endproc	
.size	_vpaes_decrypt_core,.-_vpaes_decrypt_core

// ###################################################### 
// ## 
// AES key schedule                  ## 
// ## 
// ###################################################### 
.type	_vpaes_schedule_core,@function
.align	16
_vpaes_schedule_core:
.cfi_startproc	
// rdi = key
// rsi = size in bits
// rdx = buffer
// rcx = direction.  0=encrypt, 1=decrypt

	call	_vpaes_preheat  // load the tables
	movdqa	.Lk_rcon(%rip),%xmm8  // load rcon
	movdqu	(%rdi),%xmm0  // load key (unaligned)

// input transform
	movdqa	%xmm0,%xmm3
	leaq	.Lk_ipt(%rip),%r11
	call	_vpaes_schedule_transform
	movdqa	%xmm0,%xmm7

	leaq	.Lk_sr(%rip),%r10
	testq	%rcx,%rcx
	jnz	.Lschedule_am_decrypting

// encrypting, output zeroth round key after transform
	movdqu	%xmm0,(%rdx)
	jmp	.Lschedule_go

.Lschedule_am_decrypting:
// decrypting, output zeroth round key after shiftrows
	movdqa	(%r8,%r10,1),%xmm1
	pshufb	%xmm1,%xmm3
	movdqu	%xmm3,(%rdx)
	xorq	$0x30,%r8

.Lschedule_go:
	cmpl	$192,%esi
	ja	.Lschedule_256
	je	.Lschedule_192
// 128: fall though

//  
// .schedule_128 
//  
// 128-bit specific part of key schedule. 
//  
// This schedule is really simple, because all its parts 
// are accomplished by the subroutines. 
//  
.Lschedule_128:
	movl	$10,%esi

.Loop_schedule_128:
	call	_vpaes_schedule_round
	decq	%rsi
	jz	.Lschedule_mangle_last
	call	_vpaes_schedule_mangle  // write output
	jmp	.Loop_schedule_128

//  
// .aes_schedule_192 
//  
// 192-bit specific part of key schedule. 
//  
// The main body of this schedule is the same as the 128-bit 
// schedule, but with more smearing.  The long, high side is 
// stored in %xmm7 as before, and the short, low side is in 
// the high bits of %xmm6. 
//  
// This schedule is somewhat nastier, however, because each 
// round produces 192 bits of key material, or 1.5 round keys. 
// Therefore, on each cycle we do 2 rounds and produce 3 round 
// keys. 
//  
.align	16
.Lschedule_192:
	movdqu	8(%rdi),%xmm0  // load key part 2 (very unaligned)
	call	_vpaes_schedule_transform  // input transform
	movdqa	%xmm0,%xmm6  // save short part
	pxor	%xmm4,%xmm4  // clear 4
	movhlps	%xmm4,%xmm6  // clobber low side with zeros
	movl	$4,%esi

.Loop_schedule_192:
	call	_vpaes_schedule_round
	palignr	$8,%xmm6,%xmm0
	call	_vpaes_schedule_mangle  // save key n
	call	_vpaes_schedule_192_smear
	call	_vpaes_schedule_mangle  // save key n+1
	call	_vpaes_schedule_round
	decq	%rsi
	jz	.Lschedule_mangle_last
	call	_vpaes_schedule_mangle  // save key n+2
	call	_vpaes_schedule_192_smear
	jmp	.Loop_schedule_192

//  
// .aes_schedule_256 
//  
// 256-bit specific part of key schedule. 
//  
// The structure here is very similar to the 128-bit 
// schedule, but with an additional "low side" in 
// %xmm6.  The low side's rounds are the same as the 
// high side's, except no rcon and no rotation. 
//  
.align	16
.Lschedule_256:
	movdqu	16(%rdi),%xmm0  // load key part 2 (unaligned)
	call	_vpaes_schedule_transform  // input transform
	movl	$7,%esi

.Loop_schedule_256:
	call	_vpaes_schedule_mangle  // output low result
	movdqa	%xmm0,%xmm6  // save cur_lo in xmm6

// high round
	call	_vpaes_schedule_round
	decq	%rsi
	jz	.Lschedule_mangle_last
	call	_vpaes_schedule_mangle

// low round. swap xmm7 and xmm6
	pshufd	$0xFF,%xmm0,%xmm0
	movdqa	%xmm7,%xmm5
	movdqa	%xmm6,%xmm7
	call	_vpaes_schedule_low_round
	movdqa	%xmm5,%xmm7

	jmp	.Loop_schedule_256


//  
// .aes_schedule_mangle_last 
//  
// Mangler for last round of key schedule 
// Mangles %xmm0 
// when encrypting, outputs out(%xmm0) ^ 63 
// when decrypting, outputs unskew(%xmm0) 
//  
// Always called right before return... jumps to cleanup and exits 
//  
.align	16
.Lschedule_mangle_last:
// schedule last round key from xmm0
	leaq	.Lk_deskew(%rip),%r11  // prepare to deskew
	testq	%rcx,%rcx
	jnz	.Lschedule_mangle_last_dec

// encrypting
	movdqa	(%r8,%r10,1),%xmm1
	pshufb	%xmm1,%xmm0  // output permute
	leaq	.Lk_opt(%rip),%r11  // prepare to output transform
	addq	$32,%rdx

.Lschedule_mangle_last_dec:
	addq	$-16,%rdx
	pxor	.Lk_s63(%rip),%xmm0
	call	_vpaes_schedule_transform  // output transform
	movdqu	%xmm0,(%rdx)  // save last key

// cleanup
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7
	ret
.cfi_endproc	
.size	_vpaes_schedule_core,.-_vpaes_schedule_core

//  
// .aes_schedule_192_smear 
//  
// Smear the short, low side in the 192-bit key schedule. 
//  
// Inputs: 
// %xmm7: high side, b  a  x  y 
// %xmm6:  low side, d  c  0  0 
// %xmm13: 0 
//  
// Outputs: 
// %xmm6: b+c+d  b+c  0  0 
// %xmm0: b+c+d  b+c  b  a 
//  
.type	_vpaes_schedule_192_smear,@function
.align	16
_vpaes_schedule_192_smear:
.cfi_startproc	
	pshufd	$0x80,%xmm6,%xmm1  // d c 0 0 -> c 0 0 0
	pshufd	$0xFE,%xmm7,%xmm0  // b a _ _ -> b b b a
	pxor	%xmm1,%xmm6  // -> c+d c 0 0
	pxor	%xmm1,%xmm1
	pxor	%xmm0,%xmm6  // -> b+c+d b+c b a
	movdqa	%xmm6,%xmm0
	movhlps	%xmm1,%xmm6  // clobber low side with zeros
	ret
.cfi_endproc	
.size	_vpaes_schedule_192_smear,.-_vpaes_schedule_192_smear

//  
// .aes_schedule_round 
//  
// Runs one main round of the key schedule on %xmm0, %xmm7 
//  
// Specifically, runs subbytes on the high dword of %xmm0 
// then rotates it by one byte and xors into the low dword of 
// %xmm7. 
//  
// Adds rcon from low byte of %xmm8, then rotates %xmm8 for 
// next rcon. 
//  
// Smears the dwords of %xmm7 by xoring the low into the 
// second low, result into third, result into highest. 
//  
// Returns results in %xmm7 = %xmm0. 
// Clobbers %xmm1-%xmm4, %r11. 
//  
.type	_vpaes_schedule_round,@function
.align	16
_vpaes_schedule_round:
.cfi_startproc	
// extract rcon from xmm8
	pxor	%xmm1,%xmm1
	palignr	$15,%xmm8,%xmm1
	palignr	$15,%xmm8,%xmm8
	pxor	%xmm1,%xmm7

// rotate
	pshufd	$0xFF,%xmm0,%xmm0
	palignr	$1,%xmm0,%xmm0

// fall through...

// low round: same as high round, but no rotation and no rcon.
_vpaes_schedule_low_round:
// smear xmm7
	movdqa	%xmm7,%xmm1
	pslldq	$4,%xmm7
	pxor	%xmm1,%xmm7
	movdqa	%xmm7,%xmm1
	pslldq	$8,%xmm7
	pxor	%xmm1,%xmm7
	pxor	.Lk_s63(%rip),%xmm7

// subbytes
	movdqa	%xmm9,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1  // 1 = i
	pand	%xmm9,%xmm0  // 0 = k
	movdqa	%xmm11,%xmm2  // 2 : a/k
	pshufb	%xmm0,%xmm2  // 2 = a/k
	pxor	%xmm1,%xmm0  // 0 = j
	movdqa	%xmm10,%xmm3  // 3 : 1/i
	pshufb	%xmm1,%xmm3  // 3 = 1/i
	pxor	%xmm2,%xmm3  // 3 = iak = 1/i + a/k
	movdqa	%xmm10,%xmm4  // 4 : 1/j
	pshufb	%xmm0,%xmm4  // 4 = 1/j
	pxor	%xmm2,%xmm4  // 4 = jak = 1/j + a/k
	movdqa	%xmm10,%xmm2  // 2 : 1/iak
	pshufb	%xmm3,%xmm2  // 2 = 1/iak
	pxor	%xmm0,%xmm2  // 2 = io
	movdqa	%xmm10,%xmm3  // 3 : 1/jak
	pshufb	%xmm4,%xmm3  // 3 = 1/jak
	pxor	%xmm1,%xmm3  // 3 = jo
	movdqa	%xmm13,%xmm4  // 4 : sbou
	pshufb	%xmm2,%xmm4  // 4 = sbou
	movdqa	%xmm12,%xmm0  // 0 : sbot
	pshufb	%xmm3,%xmm0  // 0 = sb1t
	pxor	%xmm4,%xmm0  // 0 = sbox output

// add in smeared stuff
	pxor	%xmm7,%xmm0
	movdqa	%xmm0,%xmm7
	ret
.cfi_endproc	
.size	_vpaes_schedule_round,.-_vpaes_schedule_round

//  
// .aes_schedule_transform 
//  
// Linear-transform %xmm0 according to tables at (%r11) 
//  
// Requires that %xmm9 = 0x0F0F... as in preheat 
// Output in %xmm0 
// Clobbers %xmm1, %xmm2 
//  
.type	_vpaes_schedule_transform,@function
.align	16
_vpaes_schedule_transform:
.cfi_startproc	
	movdqa	%xmm9,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm9,%xmm0
	movdqa	(%r11),%xmm2  // lo
	pshufb	%xmm0,%xmm2
	movdqa	16(%r11),%xmm0  // hi
	pshufb	%xmm1,%xmm0
	pxor	%xmm2,%xmm0
	ret
.cfi_endproc	
.size	_vpaes_schedule_transform,.-_vpaes_schedule_transform

//  
// .aes_schedule_mangle 
//  
// Mangle xmm0 from (basis-transformed) standard version 
// to our version. 
//  
// On encrypt, 
// xor with 0x63 
// multiply by circulant 0,1,1,1 
// apply shiftrows transform 
//  
// On decrypt, 
// xor with 0x63 
// multiply by "inverse mixcolumns" circulant E,B,D,9 
// deskew 
// apply shiftrows transform 
//  
//  
// Writes out to (%rdx), and increments or decrements it 
// Keeps track of round number mod 4 in %r8 
// Preserves xmm0 
// Clobbers xmm1-xmm5 
//  
.type	_vpaes_schedule_mangle,@function
.align	16
_vpaes_schedule_mangle:
.cfi_startproc	
	movdqa	%xmm0,%xmm4  // save xmm0 for later
	movdqa	.Lk_mc_forward(%rip),%xmm5
	testq	%rcx,%rcx
	jnz	.Lschedule_mangle_dec

// encrypting
	addq	$16,%rdx
	pxor	.Lk_s63(%rip),%xmm4
	pshufb	%xmm5,%xmm4
	movdqa	%xmm4,%xmm3
	pshufb	%xmm5,%xmm4
	pxor	%xmm4,%xmm3
	pshufb	%xmm5,%xmm4
	pxor	%xmm4,%xmm3

	jmp	.Lschedule_mangle_both
.align	16
.Lschedule_mangle_dec:
// inverse mix columns
	leaq	.Lk_dksd(%rip),%r11
	movdqa	%xmm9,%xmm1
	pandn	%xmm4,%xmm1
	psrld	$4,%xmm1  // 1 = hi
	pand	%xmm9,%xmm4  // 4 = lo

	movdqa	0(%r11),%xmm2
	pshufb	%xmm4,%xmm2
	movdqa	16(%r11),%xmm3
	pshufb	%xmm1,%xmm3
	pxor	%xmm2,%xmm3
	pshufb	%xmm5,%xmm3

	movdqa	32(%r11),%xmm2
	pshufb	%xmm4,%xmm2
	pxor	%xmm3,%xmm2
	movdqa	48(%r11),%xmm3
	pshufb	%xmm1,%xmm3
	pxor	%xmm2,%xmm3
	pshufb	%xmm5,%xmm3

	movdqa	64(%r11),%xmm2
	pshufb	%xmm4,%xmm2
	pxor	%xmm3,%xmm2
	movdqa	80(%r11),%xmm3
	pshufb	%xmm1,%xmm3
	pxor	%xmm2,%xmm3
	pshufb	%xmm5,%xmm3

	movdqa	96(%r11),%xmm2
	pshufb	%xmm4,%xmm2
	pxor	%xmm3,%xmm2
	movdqa	112(%r11),%xmm3
	pshufb	%xmm1,%xmm3
	pxor	%xmm2,%xmm3

	addq	$-16,%rdx

.Lschedule_mangle_both:
	movdqa	(%r8,%r10,1),%xmm1
	pshufb	%xmm1,%xmm3
	addq	$-16,%r8
	andq	$0x30,%r8
	movdqu	%xmm3,(%rdx)
	ret
.cfi_endproc	
.size	_vpaes_schedule_mangle,.-_vpaes_schedule_mangle

// 
// Interface to OpenSSL
// 
.globl	vpaes_set_encrypt_key
.hidden vpaes_set_encrypt_key
.type	vpaes_set_encrypt_key,@function
.align	16
vpaes_set_encrypt_key:
.cfi_startproc	
_CET_ENDBR
#ifdef BORINGSSL_DISPATCH_TEST
.extern	BORINGSSL_function_hit
.hidden BORINGSSL_function_hit
	movb	$1,BORINGSSL_function_hit+5(%rip)
#endif

	movl	%esi,%eax
	shrl	$5,%eax
	addl	$5,%eax
	movl	%eax,240(%rdx)  // AES_KEY->rounds = nbits/32+5;

	movl	$0,%ecx
	movl	$0x30,%r8d
	call	_vpaes_schedule_core
	xorl	%eax,%eax
	ret
.cfi_endproc	
.size	vpaes_set_encrypt_key,.-vpaes_set_encrypt_key

.globl	vpaes_set_decrypt_key
.hidden vpaes_set_decrypt_key
.type	vpaes_set_decrypt_key,@function
.align	16
vpaes_set_decrypt_key:
.cfi_startproc	
_CET_ENDBR
	movl	%esi,%eax
	shrl	$5,%eax
	addl	$5,%eax
	movl	%eax,240(%rdx)  // AES_KEY->rounds = nbits/32+5;
	shll	$4,%eax
	leaq	16(%rdx,%rax,1),%rdx

	movl	$1,%ecx
	movl	%esi,%r8d
	shrl	$1,%r8d
	andl	$32,%r8d
	xorl	$32,%r8d  // nbits==192?0:32
	call	_vpaes_schedule_core
	xorl	%eax,%eax
	ret
.cfi_endproc	
.size	vpaes_set_decrypt_key,.-vpaes_set_decrypt_key

.globl	vpaes_encrypt
.hidden vpaes_encrypt
.type	vpaes_encrypt,@function
.align	16
vpaes_encrypt:
.cfi_startproc	
_CET_ENDBR
#ifdef BORINGSSL_DISPATCH_TEST
.extern	BORINGSSL_function_hit
.hidden BORINGSSL_function_hit
	movb	$1,BORINGSSL_function_hit+4(%rip)
#endif
	movdqu	(%rdi),%xmm0
	call	_vpaes_preheat
	call	_vpaes_encrypt_core
	movdqu	%xmm0,(%rsi)
	ret
.cfi_endproc	
.size	vpaes_encrypt,.-vpaes_encrypt

.globl	vpaes_decrypt
.hidden vpaes_decrypt
.type	vpaes_decrypt,@function
.align	16
vpaes_decrypt:
.cfi_startproc	
_CET_ENDBR
	movdqu	(%rdi),%xmm0
	call	_vpaes_preheat
	call	_vpaes_decrypt_core
	movdqu	%xmm0,(%rsi)
	ret
.cfi_endproc	
.size	vpaes_decrypt,.-vpaes_decrypt
.globl	vpaes_cbc_encrypt
.hidden vpaes_cbc_encrypt
.type	vpaes_cbc_encrypt,@function
.align	16
vpaes_cbc_encrypt:
.cfi_startproc	
_CET_ENDBR
	xchgq	%rcx,%rdx
	subq	$16,%rcx
	jc	.Lcbc_abort
	movdqu	(%r8),%xmm6  // load IV
	subq	%rdi,%rsi
	call	_vpaes_preheat
	cmpl	$0,%r9d
	je	.Lcbc_dec_loop
	jmp	.Lcbc_enc_loop
.align	16
.Lcbc_enc_loop:
	movdqu	(%rdi),%xmm0
	pxor	%xmm6,%xmm0
	call	_vpaes_encrypt_core
	movdqa	%xmm0,%xmm6
	movdqu	%xmm0,(%rsi,%rdi,1)
	leaq	16(%rdi),%rdi
	subq	$16,%rcx
	jnc	.Lcbc_enc_loop
	jmp	.Lcbc_done
.align	16
.Lcbc_dec_loop:
	movdqu	(%rdi),%xmm0
	movdqa	%xmm0,%xmm7
	call	_vpaes_decrypt_core
	pxor	%xmm6,%xmm0
	movdqa	%xmm7,%xmm6
	movdqu	%xmm0,(%rsi,%rdi,1)
	leaq	16(%rdi),%rdi
	subq	$16,%rcx
	jnc	.Lcbc_dec_loop
.Lcbc_done:
	movdqu	%xmm6,(%r8)  // save IV
.Lcbc_abort:
	ret
.cfi_endproc	
.size	vpaes_cbc_encrypt,.-vpaes_cbc_encrypt
.globl	vpaes_ctr32_encrypt_blocks
.hidden vpaes_ctr32_encrypt_blocks
.type	vpaes_ctr32_encrypt_blocks,@function
.align	16
vpaes_ctr32_encrypt_blocks:
.cfi_startproc	
_CET_ENDBR
// _vpaes_encrypt_core and _vpaes_encrypt_core_2x expect the key in %rdx.
	xchgq	%rcx,%rdx
	testq	%rcx,%rcx
	jz	.Lctr32_abort
	movdqu	(%r8),%xmm0  // Load IV.
	movdqa	.Lctr_add_one(%rip),%xmm8
	subq	%rdi,%rsi  // This allows only incrementing %rdi.
	call	_vpaes_preheat
	movdqa	%xmm0,%xmm6
	pshufb	.Lrev_ctr(%rip),%xmm6

	testq	$1,%rcx
	jz	.Lctr32_prep_loop

// Handle one block so the remaining block count is even for
// _vpaes_encrypt_core_2x.
	movdqu	(%rdi),%xmm7  // Load input.
	call	_vpaes_encrypt_core
	pxor	%xmm7,%xmm0
	paddd	%xmm8,%xmm6
	movdqu	%xmm0,(%rsi,%rdi,1)
	subq	$1,%rcx
	leaq	16(%rdi),%rdi
	jz	.Lctr32_done

.Lctr32_prep_loop:
// _vpaes_encrypt_core_2x leaves only %xmm14 and %xmm15 as spare
// registers. We maintain two byte-swapped counters in them.
	movdqa	%xmm6,%xmm14
	movdqa	%xmm6,%xmm15
	paddd	%xmm8,%xmm15

.Lctr32_loop:
	movdqa	.Lrev_ctr(%rip),%xmm1  // Set up counters.
	movdqa	%xmm14,%xmm0
	movdqa	%xmm15,%xmm6
	pshufb	%xmm1,%xmm0
	pshufb	%xmm1,%xmm6
	call	_vpaes_encrypt_core_2x
	movdqu	(%rdi),%xmm1  // Load input.
	movdqu	16(%rdi),%xmm2
	movdqa	.Lctr_add_two(%rip),%xmm3
	pxor	%xmm1,%xmm0  // XOR input.
	pxor	%xmm2,%xmm6
	paddd	%xmm3,%xmm14  // Increment counters.
	paddd	%xmm3,%xmm15
	movdqu	%xmm0,(%rsi,%rdi,1)  // Write output.
	movdqu	%xmm6,16(%rsi,%rdi,1)
	subq	$2,%rcx  // Advance loop.
	leaq	32(%rdi),%rdi
	jnz	.Lctr32_loop

.Lctr32_done:
.Lctr32_abort:
	ret
.cfi_endproc	
.size	vpaes_ctr32_encrypt_blocks,.-vpaes_ctr32_encrypt_blocks
//  
// _aes_preheat 
//  
// Fills register %r10 -> .aes_consts (so you can -fPIC) 
// and %xmm9-%xmm15 as specified below. 
//  
.type	_vpaes_preheat,@function
.align	16
_vpaes_preheat:
.cfi_startproc	
	leaq	.Lk_s0F(%rip),%r10
	movdqa	-32(%r10),%xmm10  // .Lk_inv
	movdqa	-16(%r10),%xmm11  // .Lk_inv+16
	movdqa	0(%r10),%xmm9  // .Lk_s0F
	movdqa	48(%r10),%xmm13  // .Lk_sb1
	movdqa	64(%r10),%xmm12  // .Lk_sb1+16
	movdqa	80(%r10),%xmm15  // .Lk_sb2
	movdqa	96(%r10),%xmm14  // .Lk_sb2+16
	ret
.cfi_endproc	
.size	_vpaes_preheat,.-_vpaes_preheat
// ###################################################### 
// ## 
// Constants                      ## 
// ## 
// ###################################################### 
.type	_vpaes_consts,@object
.section	.rodata
.align	64
_vpaes_consts:
.Lk_inv:  // inv, inva
.quad	0x0E05060F0D080180, 0x040703090A0B0C02
.quad	0x01040A060F0B0780, 0x030D0E0C02050809

.Lk_s0F:  // s0F
.quad	0x0F0F0F0F0F0F0F0F, 0x0F0F0F0F0F0F0F0F

.Lk_ipt:  // input transform (lo, hi)
.quad	0xC2B2E8985A2A7000, 0xCABAE09052227808
.quad	0x4C01307D317C4D00, 0xCD80B1FCB0FDCC81

.Lk_sb1:  // sb1u, sb1t
.quad	0xB19BE18FCB503E00, 0xA5DF7A6E142AF544
.quad	0x3618D415FAE22300, 0x3BF7CCC10D2ED9EF
.Lk_sb2:  // sb2u, sb2t
.quad	0xE27A93C60B712400, 0x5EB7E955BC982FCD
.quad	0x69EB88400AE12900, 0xC2A163C8AB82234A
.Lk_sbo:  // sbou, sbot
.quad	0xD0D26D176FBDC700, 0x15AABF7AC502A878
.quad	0xCFE474A55FBB6A00, 0x8E1E90D1412B35FA

.Lk_mc_forward:  // mc_forward
.quad	0x0407060500030201, 0x0C0F0E0D080B0A09
.quad	0x080B0A0904070605, 0x000302010C0F0E0D
.quad	0x0C0F0E0D080B0A09, 0x0407060500030201
.quad	0x000302010C0F0E0D, 0x080B0A0904070605

.Lk_mc_backward:  // mc_backward
.quad	0x0605040702010003, 0x0E0D0C0F0A09080B
.quad	0x020100030E0D0C0F, 0x0A09080B06050407
.quad	0x0E0D0C0F0A09080B, 0x0605040702010003
.quad	0x0A09080B06050407, 0x020100030E0D0C0F

.Lk_sr:  // sr
.quad	0x0706050403020100, 0x0F0E0D0C0B0A0908
.quad	0x030E09040F0A0500, 0x0B06010C07020D08
.quad	0x0F060D040B020900, 0x070E050C030A0108
.quad	0x0B0E0104070A0D00, 0x0306090C0F020508

.Lk_rcon:  // rcon
.quad	0x1F8391B9AF9DEEB6, 0x702A98084D7C7D81

.Lk_s63:  // s63: all equal to 0x63 transformed
.quad	0x5B5B5B5B5B5B5B5B, 0x5B5B5B5B5B5B5B5B

.Lk_opt:  // output transform
.quad	0xFF9F4929D6B66000, 0xF7974121DEBE6808
.quad	0x01EDBD5150BCEC00, 0xE10D5DB1B05C0CE0

.Lk_deskew:  // deskew tables: inverts the sbox's "skew"
.quad	0x07E4A34047A4E300, 0x1DFEB95A5DBEF91A
.quad	0x5F36B5DC83EA6900, 0x2841C2ABF49D1E77

//  
// Decryption stuff 
// Key schedule constants 
//  
.Lk_dksd:  // decryption key schedule: invskew x*D
.quad	0xFEB91A5DA3E44700, 0x0740E3A45A1DBEF9
.quad	0x41C277F4B5368300, 0x5FDC69EAAB289D1E
.Lk_dksb:  // decryption key schedule: invskew x*B
.quad	0x9A4FCA1F8550D500, 0x03D653861CC94C99
.quad	0x115BEDA7B6FC4A00, 0xD993256F7E3482C8
.Lk_dkse:  // decryption key schedule: invskew x*E + 0x63
.quad	0xD5031CCA1FC9D600, 0x53859A4C994F5086
.quad	0xA23196054FDC7BE8, 0xCD5EF96A20B31487
.Lk_dks9:  // decryption key schedule: invskew x*9
.quad	0xB6116FC87ED9A700, 0x4AED933482255BFC
.quad	0x4576516227143300, 0x8BB89FACE9DAFDCE

//  
// Decryption stuff 
// Round function constants 
//  
.Lk_dipt:  // decryption input transform
.quad	0x0F505B040B545F00, 0x154A411E114E451A
.quad	0x86E383E660056500, 0x12771772F491F194

.Lk_dsb9:  // decryption sbox output *9*u, *9*t
.quad	0x851C03539A86D600, 0xCAD51F504F994CC9
.quad	0xC03B1789ECD74900, 0x725E2C9EB2FBA565
.Lk_dsbd:  // decryption sbox output *D*u, *D*t
.quad	0x7D57CCDFE6B1A200, 0xF56E9B13882A4439
.quad	0x3CE2FAF724C6CB00, 0x2931180D15DEEFD3
.Lk_dsbb:  // decryption sbox output *B*u, *B*t
.quad	0xD022649296B44200, 0x602646F6B0F2D404
.quad	0xC19498A6CD596700, 0xF3FF0C3E3255AA6B
.Lk_dsbe:  // decryption sbox output *E*u, *E*t
.quad	0x46F2929626D4D000, 0x2242600464B4F6B0
.quad	0x0C55A6CDFFAAC100, 0x9467F36B98593E32
.Lk_dsbo:  // decryption sbox final output
.quad	0x1387EA537EF94000, 0xC7AA6DB9D4943E2D
.quad	0x12D7560F93441D00, 0xCA4B8159D8C58E9C

// .Lrev_ctr is a permutation which byte-swaps the counter portion of the IV.
.Lrev_ctr:
.quad	0x0706050403020100, 0x0c0d0e0f0b0a0908
// .Lctr_add_* may be added to a byte-swapped xmm register to increment the
// counter. The register must be byte-swapped again to form the actual input.
.Lctr_add_one:
.quad	0x0000000000000000, 0x0000000100000000
.Lctr_add_two:
.quad	0x0000000000000000, 0x0000000200000000

.byte	86,101,99,116,111,114,32,80,101,114,109,117,116,97,116,105,111,110,32,65,69,83,32,102,111,114,32,120,56,54,95,54,52,47,83,83,83,69,51,44,32,77,105,107,101,32,72,97,109,98,117,114,103,32,40,83,116,97,110,102,111,114,100,32,85,110,105,118,101,114,115,105,116,121,41,0
.align	64
.size	_vpaes_consts,.-_vpaes_consts
.text	
#endif
