; This file is generated from a similarly-named Perl script in the BoringSSL
; source tree. Do not edit by hand.

%ifidn __OUTPUT_FORMAT__, win64
default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
%define _CET_ENDBR

%ifdef BORINGSSL_PREFIX
%include "boringssl_prefix_symbols_internal_x86_64_win_asm.inc"
%endif
section	.text code align=64


global	bn_mul_mont_gather5_nohw

ALIGN	64
bn_mul_mont_gather5_nohw:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul_mont_gather5_nohw:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
; num is declared as an int, a 32-bit parameter, so the upper half is
; undefined. Zero the upper half to normalize it.
	mov	r9d,r9d
	mov	rax,rsp

	movd	xmm5,DWORD[56+rsp]  ; load 7th argument
	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15


	neg	r9
	mov	r11,rsp
	lea	r10,[((-280))+r9*8+rsp]  ; future alloca(8*(num+2)+256+8)
	neg	r9  ; restore %r9
	and	r10,-1024  ; minimize TLB usage

; An OS-agnostic version of __chkstk.
; 
; Some OSes (Windows) insist on stack being "wired" to
; physical memory in strictly sequential manner, i.e. if stack
; allocation spans two pages, then reference to farmost one can
; be punishable by SEGV. But page walking can do good even on
; other OSes, because it guarantees that villain thread hits
; the guard page before it can make damage to innocent one...
	sub	r11,r10
	and	r11,-4096
	lea	rsp,[r11*1+r10]
	mov	r11,QWORD[rsp]
	cmp	rsp,r10
	ja	NEAR $L$mul_page_walk
	jmp	NEAR $L$mul_page_walk_done

$L$mul_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r11,QWORD[rsp]
	cmp	rsp,r10
	ja	NEAR $L$mul_page_walk
$L$mul_page_walk_done:

	lea	r10,[$L$inc]
	mov	QWORD[8+r9*8+rsp],rax  ; tp[num+1]=%rsp

$L$mul_body:

	lea	r12,[128+rdx]  ; reassign %rdx (+size optimization)
	movdqa	xmm0,XMMWORD[r10]  ; 00000001000000010000000000000000
	movdqa	xmm1,XMMWORD[16+r10]  ; 00000002000000020000000200000002
	lea	r10,[((24-112))+r9*8+rsp]  ; place the mask after tp[num+3] (+ICache optimization)
	and	r10,-16

	pshufd	xmm5,xmm5,0  ; broadcast index
	movdqa	xmm4,xmm1
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5  ; compare to 1,0
	DB	0x67
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[112+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[128+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[144+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[160+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[176+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[192+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[208+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[224+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[240+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[256+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[272+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[288+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD[304+r10],xmm0

	paddd	xmm3,xmm2
	DB	0x67
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD[320+r10],xmm1

	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD[336+r10],xmm2
	pand	xmm0,XMMWORD[64+r12]  ; while it's still in register

	pand	xmm1,XMMWORD[80+r12]
	pand	xmm2,XMMWORD[96+r12]
	movdqa	XMMWORD[352+r10],xmm3
	pand	xmm3,XMMWORD[112+r12]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[((-128))+r12]
	movdqa	xmm5,XMMWORD[((-112))+r12]
	movdqa	xmm2,XMMWORD[((-96))+r12]
	pand	xmm4,XMMWORD[112+r10]
	movdqa	xmm3,XMMWORD[((-80))+r12]
	pand	xmm5,XMMWORD[128+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[144+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[160+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[((-64))+r12]
	movdqa	xmm5,XMMWORD[((-48))+r12]
	movdqa	xmm2,XMMWORD[((-32))+r12]
	pand	xmm4,XMMWORD[176+r10]
	movdqa	xmm3,XMMWORD[((-16))+r12]
	pand	xmm5,XMMWORD[192+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[208+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[224+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[r12]
	movdqa	xmm5,XMMWORD[16+r12]
	movdqa	xmm2,XMMWORD[32+r12]
	pand	xmm4,XMMWORD[240+r10]
	movdqa	xmm3,XMMWORD[48+r12]
	pand	xmm5,XMMWORD[256+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[272+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[288+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	por	xmm0,xmm1
; Combine the upper and lower halves of %xmm0.
	pshufd	xmm1,xmm0,0x4e  ; Swap upper and lower halves.
	por	xmm0,xmm1
	lea	r12,[256+r12]
	movq	rbx,xmm0  ; m0=bp[0]

	mov	r8,QWORD[r8]  ; pull n0[0] value
	mov	rax,QWORD[rsi]

	xor	r14,r14  ; i=0
	xor	r15,r15  ; j=0

	mov	rbp,r8
	mul	rbx  ; ap[0]*bp[0]
	mov	r10,rax
	mov	rax,QWORD[rcx]

	imul	rbp,r10  ; "tp[0]"*n0
	mov	r11,rdx

	mul	rbp  ; np[0]*m1
	add	r10,rax  ; discarded
	mov	rax,QWORD[8+rsi]
	adc	rdx,0
	mov	r13,rdx

	lea	r15,[1+r15]  ; j++
	jmp	NEAR $L$1st_enter

ALIGN	16
$L$1st:
	add	r13,rax
	mov	rax,QWORD[r15*8+rsi]
	adc	rdx,0
	add	r13,r11  ; np[j]*m1+ap[j]*bp[0]
	mov	r11,r10
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],r13  ; tp[j-1]
	mov	r13,rdx

$L$1st_enter:
	mul	rbx  ; ap[j]*bp[0]
	add	r11,rax
	mov	rax,QWORD[r15*8+rcx]
	adc	rdx,0
	lea	r15,[1+r15]  ; j++
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	cmp	r15,r9
	jne	NEAR $L$1st  ; note that upon exit %r15==%r9, so
; they can be used interchangeably

	add	r13,rax
	adc	rdx,0
	add	r13,r11  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-16))+r9*8+rsp],r13  ; tp[num-1]
	mov	r13,rdx
	mov	r11,r10

	xor	rdx,rdx
	add	r13,r11
	adc	rdx,0
	mov	QWORD[((-8))+r9*8+rsp],r13
	mov	QWORD[r9*8+rsp],rdx  ; store upmost overflow bit

	lea	r14,[1+r14]  ; i++
	jmp	NEAR $L$outer
ALIGN	16
$L$outer:
	lea	rdx,[((24+128))+r9*8+rsp]  ; where 256-byte mask is (+size optimization)
	and	rdx,-16
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movdqa	xmm0,XMMWORD[((-128))+r12]
	movdqa	xmm1,XMMWORD[((-112))+r12]
	movdqa	xmm2,XMMWORD[((-96))+r12]
	movdqa	xmm3,XMMWORD[((-80))+r12]
	pand	xmm0,XMMWORD[((-128))+rdx]
	pand	xmm1,XMMWORD[((-112))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[((-96))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[((-80))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[((-64))+r12]
	movdqa	xmm1,XMMWORD[((-48))+r12]
	movdqa	xmm2,XMMWORD[((-32))+r12]
	movdqa	xmm3,XMMWORD[((-16))+r12]
	pand	xmm0,XMMWORD[((-64))+rdx]
	pand	xmm1,XMMWORD[((-48))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[((-32))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[((-16))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[r12]
	movdqa	xmm1,XMMWORD[16+r12]
	movdqa	xmm2,XMMWORD[32+r12]
	movdqa	xmm3,XMMWORD[48+r12]
	pand	xmm0,XMMWORD[rdx]
	pand	xmm1,XMMWORD[16+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[32+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[48+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[64+r12]
	movdqa	xmm1,XMMWORD[80+r12]
	movdqa	xmm2,XMMWORD[96+r12]
	movdqa	xmm3,XMMWORD[112+r12]
	pand	xmm0,XMMWORD[64+rdx]
	pand	xmm1,XMMWORD[80+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[96+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[112+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	por	xmm4,xmm5
; Combine the upper and lower halves of %xmm4 as %xmm0.
	pshufd	xmm0,xmm4,0x4e  ; Swap upper and lower halves.
	por	xmm0,xmm4
	lea	r12,[256+r12]

	mov	rax,QWORD[rsi]  ; ap[0]
	movq	rbx,xmm0  ; m0=bp[i]

	xor	r15,r15  ; j=0
	mov	rbp,r8
	mov	r10,QWORD[rsp]

	mul	rbx  ; ap[0]*bp[i]
	add	r10,rax  ; ap[0]*bp[i]+tp[0]
	mov	rax,QWORD[rcx]
	adc	rdx,0

	imul	rbp,r10  ; tp[0]*n0
	mov	r11,rdx

	mul	rbp  ; np[0]*m1
	add	r10,rax  ; discarded
	mov	rax,QWORD[8+rsi]
	adc	rdx,0
	mov	r10,QWORD[8+rsp]  ; tp[1]
	mov	r13,rdx

	lea	r15,[1+r15]  ; j++
	jmp	NEAR $L$inner_enter

ALIGN	16
$L$inner:
	add	r13,rax
	mov	rax,QWORD[r15*8+rsi]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[i]+tp[j]
	mov	r10,QWORD[r15*8+rsp]
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],r13  ; tp[j-1]
	mov	r13,rdx

$L$inner_enter:
	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[r15*8+rcx]
	adc	rdx,0
	add	r10,r11  ; ap[j]*bp[i]+tp[j]
	mov	r11,rdx
	adc	r11,0
	lea	r15,[1+r15]  ; j++

	mul	rbp  ; np[j]*m1
	cmp	r15,r9
	jne	NEAR $L$inner  ; note that upon exit %r15==%r9, so
; they can be used interchangeably
	add	r13,rax
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[i]+tp[j]
	mov	r10,QWORD[r9*8+rsp]
	adc	rdx,0
	mov	QWORD[((-16))+r9*8+rsp],r13  ; tp[num-1]
	mov	r13,rdx

	xor	rdx,rdx
	add	r13,r11
	adc	rdx,0
	add	r13,r10  ; pull upmost overflow bit
	adc	rdx,0
	mov	QWORD[((-8))+r9*8+rsp],r13
	mov	QWORD[r9*8+rsp],rdx  ; store upmost overflow bit

	lea	r14,[1+r14]  ; i++
	cmp	r14,r9
	jb	NEAR $L$outer

	xor	r14,r14  ; i=0 and clear CF!
	mov	rax,QWORD[rsp]  ; tp[0]
	lea	rsi,[rsp]  ; borrow ap for tp
	mov	r15,r9  ; j=num
	jmp	NEAR $L$sub
ALIGN	16
$L$sub:	sbb	rax,QWORD[r14*8+rcx]
	mov	QWORD[r14*8+rdi],rax  ; rp[i]=tp[i]-np[i]
	mov	rax,QWORD[8+r14*8+rsi]  ; tp[i+1]
	lea	r14,[1+r14]  ; i++
	dec	r15  ; doesn't affect CF!
	jnz	NEAR $L$sub

	sbb	rax,0  ; handle upmost overflow bit
	mov	rbx,-1
	xor	rbx,rax
	xor	r14,r14
	mov	r15,r9  ; j=num

$L$copy:  ; conditional copy
	mov	rcx,QWORD[r14*8+rdi]
	mov	rdx,QWORD[r14*8+rsp]
	and	rcx,rbx
	and	rdx,rax
	mov	QWORD[r14*8+rsp],r14  ; zap temporary vector
	or	rdx,rcx
	mov	QWORD[r14*8+rdi],rdx  ; rp[i]=tp[i]
	lea	r14,[1+r14]
	sub	r15,1
	jnz	NEAR $L$copy

	mov	rsi,QWORD[8+r9*8+rsp]  ; restore %rsp

	mov	rax,1

	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$mul_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_bn_mul_mont_gather5_nohw:
global	bn_mul4x_mont_gather5

ALIGN	32
bn_mul4x_mont_gather5:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul4x_mont_gather5:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	DB	0x67
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$mul4x_prologue:

	DB	0x67
; num is declared as an int, a 32-bit parameter, so the upper half is
; undefined. It is important that this write to %r9, which zeros the
; upper half, predates the first access.
	shl	r9d,3  ; convert %r9 to bytes
	lea	r10,[r9*2+r9]  ; 3*%r9 in bytes
	neg	r9  ; -%r9

; #############################################################
; Ensure that stack frame doesn't alias with +3*%r9
; modulo 4096, which covers ret[num], am[num] and n[num]
; (see bn_exp.c). This is done to allow memory disambiguation
; logic do its magic. [Extra [num] is allocated in order
; to align with bn_power5's frame, which is cleansed after
; completing exponentiation. Extra 256 bytes is for power mask
; calculated from 7th argument, the index.]
; 
	lea	r11,[((-320))+r9*2+rsp]
	mov	rbp,rsp
	sub	r11,rdi
	and	r11,4095
	cmp	r10,r11
	jb	NEAR $L$mul4xsp_alt
	sub	rbp,r11  ; align with %rdi
	lea	rbp,[((-320))+r9*2+rbp]  ; future alloca(frame+2*num*8+256)
	jmp	NEAR $L$mul4xsp_done

ALIGN	32
$L$mul4xsp_alt:
	lea	r10,[((4096-320))+r9*2]
	lea	rbp,[((-320))+r9*2+rbp]  ; future alloca(frame+2*num*8+256)
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rbp,r11
$L$mul4xsp_done:
	and	rbp,-64
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,[rbp*1+r11]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$mul4x_page_walk
	jmp	NEAR $L$mul4x_page_walk_done

$L$mul4x_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$mul4x_page_walk
$L$mul4x_page_walk_done:

	neg	r9

	mov	QWORD[40+rsp],rax

$L$mul4x_body:

	call	mul4x_internal

	mov	rsi,QWORD[40+rsp]  ; restore %rsp

	mov	rax,1

	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$mul4x_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_bn_mul4x_mont_gather5:


ALIGN	32
mul4x_internal:

	shl	r9,5  ; %r9 was in bytes
	movd	xmm5,DWORD[56+rax]  ; load 7th argument, index
	lea	rax,[$L$inc]
	lea	r13,[128+r9*1+rdx]  ; end of powers table (+size optimization)
	shr	r9,5  ; restore %r9
	movdqa	xmm0,XMMWORD[rax]  ; 00000001000000010000000000000000
	movdqa	xmm1,XMMWORD[16+rax]  ; 00000002000000020000000200000002
	lea	r10,[((88-112))+r9*1+rsp]  ; place the mask after tp[num+1] (+ICache optimization)
	lea	r12,[128+rdx]  ; size optimization

	pshufd	xmm5,xmm5,0  ; broadcast index
	movdqa	xmm4,xmm1
	DB	0x67,0x67
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5  ; compare to 1,0
	DB	0x67
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[112+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[128+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[144+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[160+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[176+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[192+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[208+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[224+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[240+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[256+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[272+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[288+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD[304+r10],xmm0

	paddd	xmm3,xmm2
	DB	0x67
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD[320+r10],xmm1

	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD[336+r10],xmm2
	pand	xmm0,XMMWORD[64+r12]  ; while it's still in register

	pand	xmm1,XMMWORD[80+r12]
	pand	xmm2,XMMWORD[96+r12]
	movdqa	XMMWORD[352+r10],xmm3
	pand	xmm3,XMMWORD[112+r12]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[((-128))+r12]
	movdqa	xmm5,XMMWORD[((-112))+r12]
	movdqa	xmm2,XMMWORD[((-96))+r12]
	pand	xmm4,XMMWORD[112+r10]
	movdqa	xmm3,XMMWORD[((-80))+r12]
	pand	xmm5,XMMWORD[128+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[144+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[160+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[((-64))+r12]
	movdqa	xmm5,XMMWORD[((-48))+r12]
	movdqa	xmm2,XMMWORD[((-32))+r12]
	pand	xmm4,XMMWORD[176+r10]
	movdqa	xmm3,XMMWORD[((-16))+r12]
	pand	xmm5,XMMWORD[192+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[208+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[224+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[r12]
	movdqa	xmm5,XMMWORD[16+r12]
	movdqa	xmm2,XMMWORD[32+r12]
	pand	xmm4,XMMWORD[240+r10]
	movdqa	xmm3,XMMWORD[48+r12]
	pand	xmm5,XMMWORD[256+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[272+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[288+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	por	xmm0,xmm1
; Combine the upper and lower halves of %xmm0.
	pshufd	xmm1,xmm0,0x4e  ; Swap upper and lower halves.
	por	xmm0,xmm1
	lea	r12,[256+r12]
	movq	rbx,xmm0  ; m0=bp[0]

	mov	QWORD[((16+8))+rsp],r13  ; save end of b[num]
	mov	QWORD[((56+8))+rsp],rdi  ; save %rdi

	mov	r8,QWORD[r8]  ; pull n0[0] value
	mov	rax,QWORD[rsi]
	lea	rsi,[r9*1+rsi]  ; end of a[num]
	neg	r9

	mov	rbp,r8
	mul	rbx  ; ap[0]*bp[0]
	mov	r10,rax
	mov	rax,QWORD[rcx]

	imul	rbp,r10  ; "tp[0]"*n0
	lea	r14,[((64+8))+rsp]
	mov	r11,rdx

	mul	rbp  ; np[0]*m1
	add	r10,rax  ; discarded
	mov	rax,QWORD[8+r9*1+rsi]
	adc	rdx,0
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD[8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD[16+r9*1+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	r15,[32+r9]  ; j=4
	lea	rcx,[32+rcx]
	adc	rdx,0
	mov	QWORD[r14],rdi
	mov	r13,rdx
	jmp	NEAR $L$1st4x

ALIGN	32
$L$1st4x:
	mul	rbx  ; ap[j]*bp[0]
	add	r10,rax
	mov	rax,QWORD[((-16))+rcx]
	lea	r14,[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+r15*1+rsi]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-24))+r14],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r11,rax
	mov	rax,QWORD[((-8))+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[r15*1+rsi]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-16))+r14],rdi  ; tp[j-1]
	mov	r13,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r10,rax
	mov	rax,QWORD[rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[8+r15*1+rsi]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-8))+r14],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r11,rax
	mov	rax,QWORD[8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[16+r15*1+rsi]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[0]
	lea	rcx,[32+rcx]
	adc	rdx,0
	mov	QWORD[r14],rdi  ; tp[j-1]
	mov	r13,rdx

	add	r15,32  ; j+=4
	jnz	NEAR $L$1st4x

	mul	rbx  ; ap[j]*bp[0]
	add	r10,rax
	mov	rax,QWORD[((-16))+rcx]
	lea	r14,[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+rsi]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-24))+r14],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r11,rax
	mov	rax,QWORD[((-8))+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[r9*1+rsi]  ; ap[0]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-16))+r14],rdi  ; tp[j-1]
	mov	r13,rdx

	lea	rcx,[r9*1+rcx]  ; rewind %rcx

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	mov	QWORD[((-8))+r14],r13

	jmp	NEAR $L$outer4x

ALIGN	32
$L$outer4x:
	lea	rdx,[((16+128))+r14]  ; where 256-byte mask is (+size optimization)
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movdqa	xmm0,XMMWORD[((-128))+r12]
	movdqa	xmm1,XMMWORD[((-112))+r12]
	movdqa	xmm2,XMMWORD[((-96))+r12]
	movdqa	xmm3,XMMWORD[((-80))+r12]
	pand	xmm0,XMMWORD[((-128))+rdx]
	pand	xmm1,XMMWORD[((-112))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[((-96))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[((-80))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[((-64))+r12]
	movdqa	xmm1,XMMWORD[((-48))+r12]
	movdqa	xmm2,XMMWORD[((-32))+r12]
	movdqa	xmm3,XMMWORD[((-16))+r12]
	pand	xmm0,XMMWORD[((-64))+rdx]
	pand	xmm1,XMMWORD[((-48))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[((-32))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[((-16))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[r12]
	movdqa	xmm1,XMMWORD[16+r12]
	movdqa	xmm2,XMMWORD[32+r12]
	movdqa	xmm3,XMMWORD[48+r12]
	pand	xmm0,XMMWORD[rdx]
	pand	xmm1,XMMWORD[16+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[32+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[48+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[64+r12]
	movdqa	xmm1,XMMWORD[80+r12]
	movdqa	xmm2,XMMWORD[96+r12]
	movdqa	xmm3,XMMWORD[112+r12]
	pand	xmm0,XMMWORD[64+rdx]
	pand	xmm1,XMMWORD[80+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[96+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[112+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	por	xmm4,xmm5
; Combine the upper and lower halves of %xmm4 as %xmm0.
	pshufd	xmm0,xmm4,0x4e  ; Swap upper and lower halves.
	por	xmm0,xmm4
	lea	r12,[256+r12]
	movq	rbx,xmm0  ; m0=bp[i]

	mov	r10,QWORD[r9*1+r14]
	mov	rbp,r8
	mul	rbx  ; ap[0]*bp[i]
	add	r10,rax  ; ap[0]*bp[i]+tp[0]
	mov	rax,QWORD[rcx]
	adc	rdx,0

	imul	rbp,r10  ; tp[0]*n0
	mov	r11,rdx
	mov	QWORD[r14],rdi  ; store upmost overflow bit

	lea	r14,[r9*1+r14]  ; rewind %r14

	mul	rbp  ; np[0]*m1
	add	r10,rax  ; "%r13", discarded
	mov	rax,QWORD[8+r9*1+rsi]
	adc	rdx,0
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[8+rcx]
	adc	rdx,0
	add	r11,QWORD[8+r14]  ; +tp[1]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[16+r9*1+rsi]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[i]+tp[j]
	lea	r15,[32+r9]  ; j=4
	lea	rcx,[32+rcx]
	adc	rdx,0
	mov	r13,rdx
	jmp	NEAR $L$inner4x

ALIGN	32
$L$inner4x:
	mul	rbx  ; ap[j]*bp[i]
	add	r10,rax
	mov	rax,QWORD[((-16))+rcx]
	adc	rdx,0
	add	r10,QWORD[16+r14]  ; ap[j]*bp[i]+tp[j]
	lea	r14,[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+r15*1+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD[((-32))+r14],rdi  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[((-8))+rcx]
	adc	rdx,0
	add	r11,QWORD[((-8))+r14]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[r15*1+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD[((-24))+r14],r13  ; tp[j-1]
	mov	r13,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r10,rax
	mov	rax,QWORD[rcx]
	adc	rdx,0
	add	r10,QWORD[r14]  ; ap[j]*bp[i]+tp[j]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[8+r15*1+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD[((-16))+r14],rdi  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[8+rcx]
	adc	rdx,0
	add	r11,QWORD[8+r14]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[16+r15*1+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	rcx,[32+rcx]
	adc	rdx,0
	mov	QWORD[((-8))+r14],r13  ; tp[j-1]
	mov	r13,rdx

	add	r15,32  ; j+=4
	jnz	NEAR $L$inner4x

	mul	rbx  ; ap[j]*bp[i]
	add	r10,rax
	mov	rax,QWORD[((-16))+rcx]
	adc	rdx,0
	add	r10,QWORD[16+r14]  ; ap[j]*bp[i]+tp[j]
	lea	r14,[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD[((-32))+r14],rdi  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,rbp
	mov	rbp,QWORD[((-8))+rcx]
	adc	rdx,0
	add	r11,QWORD[((-8))+r14]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[r9*1+rsi]  ; ap[0]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD[((-24))+r14],r13  ; tp[j-1]
	mov	r13,rdx

	mov	QWORD[((-16))+r14],rdi  ; tp[j-1]
	lea	rcx,[r9*1+rcx]  ; rewind %rcx

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	add	r13,QWORD[r14]  ; pull upmost overflow bit
	adc	rdi,0  ; upmost overflow bit
	mov	QWORD[((-8))+r14],r13

	cmp	r12,QWORD[((16+8))+rsp]
	jb	NEAR $L$outer4x
	xor	rax,rax
	sub	rbp,r13  ; compare top-most words
	adc	r15,r15  ; %r15 is zero
	or	rdi,r15
	sub	rax,rdi  ; %rax=-%rdi
	lea	rbx,[r9*1+r14]  ; tptr in .sqr4x_sub
	mov	r12,QWORD[rcx]
	lea	rbp,[rcx]  ; nptr in .sqr4x_sub
	mov	rcx,r9
	sar	rcx,3+2
	mov	rdi,QWORD[((56+8))+rsp]  ; rptr in .sqr4x_sub
	dec	r12  ; so that after 'not' we get -n[0]
	xor	r10,r10
	mov	r13,QWORD[8+rbp]
	mov	r14,QWORD[16+rbp]
	mov	r15,QWORD[24+rbp]
	jmp	NEAR $L$sqr4x_sub_entry


global	bn_power5_nohw

ALIGN	32
bn_power5_nohw:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_power5_nohw:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$power5_prologue:

; num is declared as an int, a 32-bit parameter, so the upper half is
; undefined. It is important that this write to %r9, which zeros the
; upper half, come before the first access.
	shl	r9d,3  ; convert %r9 to bytes
	lea	r10d,[r9*2+r9]  ; 3*%r9
	neg	r9
	mov	r8,QWORD[r8]  ; *n0

; #############################################################
; Ensure that stack frame doesn't alias with %rdi+3*%r9
; modulo 4096, which covers ret[num], am[num] and n[num]
; (see bn_exp.c). This is done to allow memory disambiguation
; logic do its magic. [Extra 256 bytes is for power mask
; calculated from 7th argument, the index.]
; 
	lea	r11,[((-320))+r9*2+rsp]
	mov	rbp,rsp
	sub	r11,rdi
	and	r11,4095
	cmp	r10,r11
	jb	NEAR $L$pwr_sp_alt
	sub	rbp,r11  ; align with %rsi
	lea	rbp,[((-320))+r9*2+rbp]  ; future alloca(frame+2*num*8+256)
	jmp	NEAR $L$pwr_sp_done

ALIGN	32
$L$pwr_sp_alt:
	lea	r10,[((4096-320))+r9*2]
	lea	rbp,[((-320))+r9*2+rbp]  ; future alloca(frame+2*num*8+256)
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rbp,r11
$L$pwr_sp_done:
	and	rbp,-64
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,[rbp*1+r11]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$pwr_page_walk
	jmp	NEAR $L$pwr_page_walk_done

$L$pwr_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$pwr_page_walk
$L$pwr_page_walk_done:

	mov	r10,r9
	neg	r9

; #############################################################
; Stack layout
; 
; +0	saved %r9, used in reduction section
; +8	&t[2*%r9], used in reduction section
; +32	saved *n0
; +40	saved %rsp
; +48	t[2*%r9]
; 
	mov	QWORD[32+rsp],r8
	mov	QWORD[40+rsp],rax  ; save original %rsp

$L$power5_body:
	movq	xmm1,rdi  ; save %rdi, used in sqr8x
	movq	xmm2,rcx  ; save %rcx
	movq	xmm3,r10  ; -%r9, used in sqr8x
	movq	xmm4,rdx

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

	movq	rcx,xmm2
	movq	rdx,xmm4
	mov	rdi,rsi
	mov	rax,QWORD[40+rsp]
	lea	r8,[32+rsp]

	call	mul4x_internal

	mov	rsi,QWORD[40+rsp]  ; restore %rsp

	mov	rax,1
	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$power5_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_bn_power5_nohw:

global	bn_sqr8x_internal


ALIGN	32
bn_sqr8x_internal:
__bn_sqr8x_internal:

_CET_ENDBR
; #############################################################
; Squaring part:
; 
; a) multiply-n-add everything but a[i]*a[i];
; b) shift result of a) by 1 to the left and accumulate
; a[i]*a[i] products;
; 
; #############################################################
; a[1]a[0]
; a[2]a[0]
; a[3]a[0]
; a[2]a[1]
; a[4]a[0]
; a[3]a[1]
; a[5]a[0]
; a[4]a[1]
; a[3]a[2]
; a[6]a[0]
; a[5]a[1]
; a[4]a[2]
; a[7]a[0]
; a[6]a[1]
; a[5]a[2]
; a[4]a[3]
; a[7]a[1]
; a[6]a[2]
; a[5]a[3]
; a[7]a[2]
; a[6]a[3]
; a[5]a[4]
; a[7]a[3]
; a[6]a[4]
; a[7]a[4]
; a[6]a[5]
; a[7]a[5]
; a[7]a[6]
; a[1]a[0]
; a[2]a[0]
; a[3]a[0]
; a[4]a[0]
; a[5]a[0]
; a[6]a[0]
; a[7]a[0]
; a[2]a[1]
; a[3]a[1]
; a[4]a[1]
; a[5]a[1]
; a[6]a[1]
; a[7]a[1]
; a[3]a[2]
; a[4]a[2]
; a[5]a[2]
; a[6]a[2]
; a[7]a[2]
; a[4]a[3]
; a[5]a[3]
; a[6]a[3]
; a[7]a[3]
; a[5]a[4]
; a[6]a[4]
; a[7]a[4]
; a[6]a[5]
; a[7]a[5]
; a[7]a[6]
; a[0]a[0]
; a[1]a[1]
; a[2]a[2]
; a[3]a[3]
; a[4]a[4]
; a[5]a[5]
; a[6]a[6]
; a[7]a[7]

	lea	rbp,[32+r10]  ; %rbp=-(%r9-32)
	lea	rsi,[r9*1+rsi]  ; end of a[] buffer, (%rsi,%rbp)=&ap[2]

	mov	rcx,r9  ; %rcx=%r9

; comments apply to %r9==8 case
	mov	r14,QWORD[((-32))+rbp*1+rsi]  ; a[0]
	lea	rdi,[((48+8))+r9*2+rsp]  ; end of tp[] buffer, &tp[2*%r9]
	mov	rax,QWORD[((-24))+rbp*1+rsi]  ; a[1]
	lea	rdi,[((-32))+rbp*1+rdi]  ; end of tp[] window, &tp[2*%r9-"%rbp"]
	mov	rbx,QWORD[((-16))+rbp*1+rsi]  ; a[2]
	mov	r15,rax

	mul	r14  ; a[1]*a[0]
	mov	r10,rax  ; a[1]*a[0]
	mov	rax,rbx  ; a[2]
	mov	r11,rdx
	mov	QWORD[((-24))+rbp*1+rdi],r10  ; t[1]

	mul	r14  ; a[2]*a[0]
	add	r11,rax
	mov	rax,rbx
	adc	rdx,0
	mov	QWORD[((-16))+rbp*1+rdi],r11  ; t[2]
	mov	r10,rdx


	mov	rbx,QWORD[((-8))+rbp*1+rsi]  ; a[3]
	mul	r15  ; a[2]*a[1]
	mov	r12,rax  ; a[2]*a[1]+t[3]
	mov	rax,rbx
	mov	r13,rdx

	lea	rcx,[rbp]
	mul	r14  ; a[3]*a[0]
	add	r10,rax  ; a[3]*a[0]+a[2]*a[1]+t[3]
	mov	rax,rbx
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0
	mov	QWORD[((-8))+rcx*1+rdi],r10  ; t[3]
	jmp	NEAR $L$sqr4x_1st

ALIGN	32
$L$sqr4x_1st:
	mov	rbx,QWORD[rcx*1+rsi]  ; a[4]
	mul	r15  ; a[3]*a[1]
	add	r13,rax  ; a[3]*a[1]+t[4]
	mov	rax,rbx
	mov	r12,rdx
	adc	r12,0

	mul	r14  ; a[4]*a[0]
	add	r11,rax  ; a[4]*a[0]+a[3]*a[1]+t[4]
	mov	rax,rbx  ; a[3]
	mov	rbx,QWORD[8+rcx*1+rsi]  ; a[5]
	mov	r10,rdx
	adc	r10,0
	add	r11,r13
	adc	r10,0


	mul	r15  ; a[4]*a[3]
	add	r12,rax  ; a[4]*a[3]+t[5]
	mov	rax,rbx
	mov	QWORD[rcx*1+rdi],r11  ; t[4]
	mov	r13,rdx
	adc	r13,0

	mul	r14  ; a[5]*a[2]
	add	r10,rax  ; a[5]*a[2]+a[4]*a[3]+t[5]
	mov	rax,rbx
	mov	rbx,QWORD[16+rcx*1+rsi]  ; a[6]
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0

	mul	r15  ; a[5]*a[3]
	add	r13,rax  ; a[5]*a[3]+t[6]
	mov	rax,rbx
	mov	QWORD[8+rcx*1+rdi],r10  ; t[5]
	mov	r12,rdx
	adc	r12,0

	mul	r14  ; a[6]*a[2]
	add	r11,rax  ; a[6]*a[2]+a[5]*a[3]+t[6]
	mov	rax,rbx  ; a[3]
	mov	rbx,QWORD[24+rcx*1+rsi]  ; a[7]
	mov	r10,rdx
	adc	r10,0
	add	r11,r13
	adc	r10,0


	mul	r15  ; a[6]*a[5]
	add	r12,rax  ; a[6]*a[5]+t[7]
	mov	rax,rbx
	mov	QWORD[16+rcx*1+rdi],r11  ; t[6]
	mov	r13,rdx
	adc	r13,0
	lea	rcx,[32+rcx]

	mul	r14  ; a[7]*a[4]
	add	r10,rax  ; a[7]*a[4]+a[6]*a[5]+t[6]
	mov	rax,rbx
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0
	mov	QWORD[((-8))+rcx*1+rdi],r10  ; t[7]

	cmp	rcx,0
	jne	NEAR $L$sqr4x_1st

	mul	r15  ; a[7]*a[5]
	add	r13,rax
	lea	rbp,[16+rbp]
	adc	rdx,0
	add	r13,r11
	adc	rdx,0

	mov	QWORD[rdi],r13  ; t[8]
	mov	r12,rdx
	mov	QWORD[8+rdi],rdx  ; t[9]
	jmp	NEAR $L$sqr4x_outer

ALIGN	32
$L$sqr4x_outer:  ; comments apply to %r9==6 case
	mov	r14,QWORD[((-32))+rbp*1+rsi]  ; a[0]
	lea	rdi,[((48+8))+r9*2+rsp]  ; end of tp[] buffer, &tp[2*%r9]
	mov	rax,QWORD[((-24))+rbp*1+rsi]  ; a[1]
	lea	rdi,[((-32))+rbp*1+rdi]  ; end of tp[] window, &tp[2*%r9-"%rbp"]
	mov	rbx,QWORD[((-16))+rbp*1+rsi]  ; a[2]
	mov	r15,rax

	mul	r14  ; a[1]*a[0]
	mov	r10,QWORD[((-24))+rbp*1+rdi]  ; t[1]
	add	r10,rax  ; a[1]*a[0]+t[1]
	mov	rax,rbx  ; a[2]
	adc	rdx,0
	mov	QWORD[((-24))+rbp*1+rdi],r10  ; t[1]
	mov	r11,rdx

	mul	r14  ; a[2]*a[0]
	add	r11,rax
	mov	rax,rbx
	adc	rdx,0
	add	r11,QWORD[((-16))+rbp*1+rdi]  ; a[2]*a[0]+t[2]
	mov	r10,rdx
	adc	r10,0
	mov	QWORD[((-16))+rbp*1+rdi],r11  ; t[2]

	xor	r12,r12

	mov	rbx,QWORD[((-8))+rbp*1+rsi]  ; a[3]
	mul	r15  ; a[2]*a[1]
	add	r12,rax  ; a[2]*a[1]+t[3]
	mov	rax,rbx
	adc	rdx,0
	add	r12,QWORD[((-8))+rbp*1+rdi]
	mov	r13,rdx
	adc	r13,0

	mul	r14  ; a[3]*a[0]
	add	r10,rax  ; a[3]*a[0]+a[2]*a[1]+t[3]
	mov	rax,rbx
	adc	rdx,0
	add	r10,r12
	mov	r11,rdx
	adc	r11,0
	mov	QWORD[((-8))+rbp*1+rdi],r10  ; t[3]

	lea	rcx,[rbp]
	jmp	NEAR $L$sqr4x_inner

ALIGN	32
$L$sqr4x_inner:
	mov	rbx,QWORD[rcx*1+rsi]  ; a[4]
	mul	r15  ; a[3]*a[1]
	add	r13,rax  ; a[3]*a[1]+t[4]
	mov	rax,rbx
	mov	r12,rdx
	adc	r12,0
	add	r13,QWORD[rcx*1+rdi]
	adc	r12,0

	DB	0x67
	mul	r14  ; a[4]*a[0]
	add	r11,rax  ; a[4]*a[0]+a[3]*a[1]+t[4]
	mov	rax,rbx  ; a[3]
	mov	rbx,QWORD[8+rcx*1+rsi]  ; a[5]
	mov	r10,rdx
	adc	r10,0
	add	r11,r13
	adc	r10,0

	mul	r15  ; a[4]*a[3]
	add	r12,rax  ; a[4]*a[3]+t[5]
	mov	QWORD[rcx*1+rdi],r11  ; t[4]
	mov	rax,rbx
	mov	r13,rdx
	adc	r13,0
	add	r12,QWORD[8+rcx*1+rdi]
	lea	rcx,[16+rcx]  ; j++
	adc	r13,0

	mul	r14  ; a[5]*a[2]
	add	r10,rax  ; a[5]*a[2]+a[4]*a[3]+t[5]
	mov	rax,rbx
	adc	rdx,0
	add	r10,r12
	mov	r11,rdx
	adc	r11,0
	mov	QWORD[((-8))+rcx*1+rdi],r10  ; t[5], "preloaded t[1]" below

	cmp	rcx,0
	jne	NEAR $L$sqr4x_inner

	DB	0x67
	mul	r15  ; a[5]*a[3]
	add	r13,rax
	adc	rdx,0
	add	r13,r11
	adc	rdx,0

	mov	QWORD[rdi],r13  ; t[6], "preloaded t[2]" below
	mov	r12,rdx
	mov	QWORD[8+rdi],rdx  ; t[7], "preloaded t[3]" below

	add	rbp,16
	jnz	NEAR $L$sqr4x_outer

; comments apply to %r9==4 case
	mov	r14,QWORD[((-32))+rsi]  ; a[0]
	lea	rdi,[((48+8))+r9*2+rsp]  ; end of tp[] buffer, &tp[2*%r9]
	mov	rax,QWORD[((-24))+rsi]  ; a[1]
	lea	rdi,[((-32))+rbp*1+rdi]  ; end of tp[] window, &tp[2*%r9-"%rbp"]
	mov	rbx,QWORD[((-16))+rsi]  ; a[2]
	mov	r15,rax

	mul	r14  ; a[1]*a[0]
	add	r10,rax  ; a[1]*a[0]+t[1], preloaded t[1]
	mov	rax,rbx  ; a[2]
	mov	r11,rdx
	adc	r11,0

	mul	r14  ; a[2]*a[0]
	add	r11,rax
	mov	rax,rbx
	mov	QWORD[((-24))+rdi],r10  ; t[1]
	mov	r10,rdx
	adc	r10,0
	add	r11,r13  ; a[2]*a[0]+t[2], preloaded t[2]
	mov	rbx,QWORD[((-8))+rsi]  ; a[3]
	adc	r10,0

	mul	r15  ; a[2]*a[1]
	add	r12,rax  ; a[2]*a[1]+t[3], preloaded t[3]
	mov	rax,rbx
	mov	QWORD[((-16))+rdi],r11  ; t[2]
	mov	r13,rdx
	adc	r13,0

	mul	r14  ; a[3]*a[0]
	add	r10,rax  ; a[3]*a[0]+a[2]*a[1]+t[3]
	mov	rax,rbx
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0
	mov	QWORD[((-8))+rdi],r10  ; t[3]

	mul	r15  ; a[3]*a[1]
	add	r13,rax
	mov	rax,QWORD[((-16))+rsi]  ; a[2]
	adc	rdx,0
	add	r13,r11
	adc	rdx,0

	mov	QWORD[rdi],r13  ; t[4]
	mov	r12,rdx
	mov	QWORD[8+rdi],rdx  ; t[5]

	mul	rbx  ; a[2]*a[3]
	add	rbp,16
	xor	r14,r14
	sub	rbp,r9  ; %rbp=16-%r9
	xor	r15,r15

	add	rax,r12  ; t[5]
	adc	rdx,0
	mov	QWORD[8+rdi],rax  ; t[5]
	mov	QWORD[16+rdi],rdx  ; t[6]
	mov	QWORD[24+rdi],r15  ; t[7]

	mov	rax,QWORD[((-16))+rbp*1+rsi]  ; a[0]
	lea	rdi,[((48+8))+rsp]
	xor	r10,r10  ; t[0]
	mov	r11,QWORD[8+rdi]  ; t[1]

	lea	r12,[r10*2+r14]  ; t[2*i]<<1 | shift
	shr	r10,63
	lea	r13,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r13,r10  ; | t[2*i]>>63
	mov	r10,QWORD[16+rdi]  ; t[2*i+2]	# prefetch
	mov	r14,r11  ; shift=t[2*i+1]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	mov	r11,QWORD[24+rdi]  ; t[2*i+2+1]	# prefetch
	adc	r12,rax
	mov	rax,QWORD[((-8))+rbp*1+rsi]  ; a[i+1]	# prefetch
	mov	QWORD[rdi],r12
	adc	r13,rdx

	lea	rbx,[r10*2+r14]  ; t[2*i]<<1 | shift
	mov	QWORD[8+rdi],r13
	sbb	r15,r15  ; mov cf,%r15
	shr	r10,63
	lea	r8,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r8,r10  ; | t[2*i]>>63
	mov	r10,QWORD[32+rdi]  ; t[2*i+2]	# prefetch
	mov	r14,r11  ; shift=t[2*i+1]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	mov	r11,QWORD[40+rdi]  ; t[2*i+2+1]	# prefetch
	adc	rbx,rax
	mov	rax,QWORD[rbp*1+rsi]  ; a[i+1]	# prefetch
	mov	QWORD[16+rdi],rbx
	adc	r8,rdx
	lea	rbp,[16+rbp]
	mov	QWORD[24+rdi],r8
	sbb	r15,r15  ; mov cf,%r15
	lea	rdi,[64+rdi]
	jmp	NEAR $L$sqr4x_shift_n_add

ALIGN	32
$L$sqr4x_shift_n_add:
	lea	r12,[r10*2+r14]  ; t[2*i]<<1 | shift
	shr	r10,63
	lea	r13,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r13,r10  ; | t[2*i]>>63
	mov	r10,QWORD[((-16))+rdi]  ; t[2*i+2]	# prefetch
	mov	r14,r11  ; shift=t[2*i+1]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	mov	r11,QWORD[((-8))+rdi]  ; t[2*i+2+1]	# prefetch
	adc	r12,rax
	mov	rax,QWORD[((-8))+rbp*1+rsi]  ; a[i+1]	# prefetch
	mov	QWORD[((-32))+rdi],r12
	adc	r13,rdx

	lea	rbx,[r10*2+r14]  ; t[2*i]<<1 | shift
	mov	QWORD[((-24))+rdi],r13
	sbb	r15,r15  ; mov cf,%r15
	shr	r10,63
	lea	r8,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r8,r10  ; | t[2*i]>>63
	mov	r10,QWORD[rdi]  ; t[2*i+2]	# prefetch
	mov	r14,r11  ; shift=t[2*i+1]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	mov	r11,QWORD[8+rdi]  ; t[2*i+2+1]	# prefetch
	adc	rbx,rax
	mov	rax,QWORD[rbp*1+rsi]  ; a[i+1]	# prefetch
	mov	QWORD[((-16))+rdi],rbx
	adc	r8,rdx

	lea	r12,[r10*2+r14]  ; t[2*i]<<1 | shift
	mov	QWORD[((-8))+rdi],r8
	sbb	r15,r15  ; mov cf,%r15
	shr	r10,63
	lea	r13,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r13,r10  ; | t[2*i]>>63
	mov	r10,QWORD[16+rdi]  ; t[2*i+2]	# prefetch
	mov	r14,r11  ; shift=t[2*i+1]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	mov	r11,QWORD[24+rdi]  ; t[2*i+2+1]	# prefetch
	adc	r12,rax
	mov	rax,QWORD[8+rbp*1+rsi]  ; a[i+1]	# prefetch
	mov	QWORD[rdi],r12
	adc	r13,rdx

	lea	rbx,[r10*2+r14]  ; t[2*i]<<1 | shift
	mov	QWORD[8+rdi],r13
	sbb	r15,r15  ; mov cf,%r15
	shr	r10,63
	lea	r8,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r8,r10  ; | t[2*i]>>63
	mov	r10,QWORD[32+rdi]  ; t[2*i+2]	# prefetch
	mov	r14,r11  ; shift=t[2*i+1]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	mov	r11,QWORD[40+rdi]  ; t[2*i+2+1]	# prefetch
	adc	rbx,rax
	mov	rax,QWORD[16+rbp*1+rsi]  ; a[i+1]	# prefetch
	mov	QWORD[16+rdi],rbx
	adc	r8,rdx
	mov	QWORD[24+rdi],r8
	sbb	r15,r15  ; mov cf,%r15
	lea	rdi,[64+rdi]
	add	rbp,32
	jnz	NEAR $L$sqr4x_shift_n_add

	lea	r12,[r10*2+r14]  ; t[2*i]<<1 | shift
	DB	0x67
	shr	r10,63
	lea	r13,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r13,r10  ; | t[2*i]>>63
	mov	r10,QWORD[((-16))+rdi]  ; t[2*i+2]	# prefetch
	mov	r14,r11  ; shift=t[2*i+1]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	mov	r11,QWORD[((-8))+rdi]  ; t[2*i+2+1]	# prefetch
	adc	r12,rax
	mov	rax,QWORD[((-8))+rsi]  ; a[i+1]	# prefetch
	mov	QWORD[((-32))+rdi],r12
	adc	r13,rdx

	lea	rbx,[r10*2+r14]  ; t[2*i]<<1|shift
	mov	QWORD[((-24))+rdi],r13
	sbb	r15,r15  ; mov cf,%r15
	shr	r10,63
	lea	r8,[r11*2+rcx]  ; t[2*i+1]<<1 |
	shr	r11,63
	or	r8,r10  ; | t[2*i]>>63
	mul	rax  ; a[i]*a[i]
	neg	r15  ; mov %r15,cf
	adc	rbx,rax
	adc	r8,rdx
	mov	QWORD[((-16))+rdi],rbx
	mov	QWORD[((-8))+rdi],r8
	movq	rbp,xmm2
__bn_sqr8x_reduction:
	xor	rax,rax
	lea	rcx,[rbp*1+r9]  ; end of n[]
	lea	rdx,[((48+8))+r9*2+rsp]  ; end of t[] buffer
	mov	QWORD[((0+8))+rsp],rcx
	lea	rdi,[((48+8))+r9*1+rsp]  ; end of initial t[] window
	mov	QWORD[((8+8))+rsp],rdx
	neg	r9
	jmp	NEAR $L$8x_reduction_loop

ALIGN	32
$L$8x_reduction_loop:
	lea	rdi,[r9*1+rdi]  ; start of current t[] window
	DB	0x66
	mov	rbx,QWORD[rdi]
	mov	r9,QWORD[8+rdi]
	mov	r10,QWORD[16+rdi]
	mov	r11,QWORD[24+rdi]
	mov	r12,QWORD[32+rdi]
	mov	r13,QWORD[40+rdi]
	mov	r14,QWORD[48+rdi]
	mov	r15,QWORD[56+rdi]
	mov	QWORD[rdx],rax  ; store top-most carry bit
	lea	rdi,[64+rdi]

	DB	0x67
	mov	r8,rbx
	imul	rbx,QWORD[((32+8))+rsp]  ; n0*a[0]
	mov	rax,QWORD[rbp]  ; n[0]
	mov	ecx,8
	jmp	NEAR $L$8x_reduce

ALIGN	32
$L$8x_reduce:
	mul	rbx
	mov	rax,QWORD[8+rbp]  ; n[1]
	neg	r8
	mov	r8,rdx
	adc	r8,0

	mul	rbx
	add	r9,rax
	mov	rax,QWORD[16+rbp]
	adc	rdx,0
	add	r8,r9
	mov	QWORD[((48-8+8))+rcx*8+rsp],rbx  ; put aside n0*a[i]
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	add	r10,rax
	mov	rax,QWORD[24+rbp]
	adc	rdx,0
	add	r9,r10
	mov	rsi,QWORD[((32+8))+rsp]  ; pull n0, borrow %rsi
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	add	r11,rax
	mov	rax,QWORD[32+rbp]
	adc	rdx,0
	imul	rsi,r8  ; modulo-scheduled
	add	r10,r11
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD[40+rbp]
	adc	rdx,0
	add	r11,r12
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r13,rax
	mov	rax,QWORD[48+rbp]
	adc	rdx,0
	add	r12,r13
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	add	r14,rax
	mov	rax,QWORD[56+rbp]
	adc	rdx,0
	add	r13,r14
	mov	r14,rdx
	adc	r14,0

	mul	rbx
	mov	rbx,rsi  ; n0*a[i]
	add	r15,rax
	mov	rax,QWORD[rbp]  ; n[0]
	adc	rdx,0
	add	r14,r15
	mov	r15,rdx
	adc	r15,0

	dec	ecx
	jnz	NEAR $L$8x_reduce

	lea	rbp,[64+rbp]
	xor	rax,rax
	mov	rdx,QWORD[((8+8))+rsp]  ; pull end of t[]
	cmp	rbp,QWORD[((0+8))+rsp]  ; end of n[]?
	jae	NEAR $L$8x_no_tail

	DB	0x66
	add	r8,QWORD[rdi]
	adc	r9,QWORD[8+rdi]
	adc	r10,QWORD[16+rdi]
	adc	r11,QWORD[24+rdi]
	adc	r12,QWORD[32+rdi]
	adc	r13,QWORD[40+rdi]
	adc	r14,QWORD[48+rdi]
	adc	r15,QWORD[56+rdi]
	sbb	rsi,rsi  ; top carry

	mov	rbx,QWORD[((48+56+8))+rsp]  ; pull n0*a[0]
	mov	ecx,8
	mov	rax,QWORD[rbp]
	jmp	NEAR $L$8x_tail

ALIGN	32
$L$8x_tail:
	mul	rbx
	add	r8,rax
	mov	rax,QWORD[8+rbp]
	mov	QWORD[rdi],r8  ; save result
	mov	r8,rdx
	adc	r8,0

	mul	rbx
	add	r9,rax
	mov	rax,QWORD[16+rbp]
	adc	rdx,0
	add	r8,r9
	lea	rdi,[8+rdi]  ; %rdi++
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	add	r10,rax
	mov	rax,QWORD[24+rbp]
	adc	rdx,0
	add	r9,r10
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	add	r11,rax
	mov	rax,QWORD[32+rbp]
	adc	rdx,0
	add	r10,r11
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD[40+rbp]
	adc	rdx,0
	add	r11,r12
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r13,rax
	mov	rax,QWORD[48+rbp]
	adc	rdx,0
	add	r12,r13
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	add	r14,rax
	mov	rax,QWORD[56+rbp]
	adc	rdx,0
	add	r13,r14
	mov	r14,rdx
	adc	r14,0

	mul	rbx
	mov	rbx,QWORD[((48-16+8))+rcx*8+rsp]  ; pull n0*a[i]
	add	r15,rax
	adc	rdx,0
	add	r14,r15
	mov	rax,QWORD[rbp]  ; pull n[0]
	mov	r15,rdx
	adc	r15,0

	dec	ecx
	jnz	NEAR $L$8x_tail

	lea	rbp,[64+rbp]
	mov	rdx,QWORD[((8+8))+rsp]  ; pull end of t[]
	cmp	rbp,QWORD[((0+8))+rsp]  ; end of n[]?
	jae	NEAR $L$8x_tail_done  ; break out of loop

	mov	rbx,QWORD[((48+56+8))+rsp]  ; pull n0*a[0]
	neg	rsi
	mov	rax,QWORD[rbp]  ; pull n[0]
	adc	r8,QWORD[rdi]
	adc	r9,QWORD[8+rdi]
	adc	r10,QWORD[16+rdi]
	adc	r11,QWORD[24+rdi]
	adc	r12,QWORD[32+rdi]
	adc	r13,QWORD[40+rdi]
	adc	r14,QWORD[48+rdi]
	adc	r15,QWORD[56+rdi]
	sbb	rsi,rsi  ; top carry

	mov	ecx,8
	jmp	NEAR $L$8x_tail

ALIGN	32
$L$8x_tail_done:
	xor	rax,rax
	add	r8,QWORD[rdx]  ; can this overflow?
	adc	r9,0
	adc	r10,0
	adc	r11,0
	adc	r12,0
	adc	r13,0
	adc	r14,0
	adc	r15,0
	adc	rax,0

	neg	rsi
$L$8x_no_tail:
	adc	r8,QWORD[rdi]
	adc	r9,QWORD[8+rdi]
	adc	r10,QWORD[16+rdi]
	adc	r11,QWORD[24+rdi]
	adc	r12,QWORD[32+rdi]
	adc	r13,QWORD[40+rdi]
	adc	r14,QWORD[48+rdi]
	adc	r15,QWORD[56+rdi]
	adc	rax,0  ; top-most carry
	mov	rcx,QWORD[((-8))+rbp]  ; np[num-1]
	xor	rsi,rsi

	movq	rbp,xmm2  ; restore %rbp

	mov	QWORD[rdi],r8  ; store top 512 bits
	mov	QWORD[8+rdi],r9
	movq	r9,xmm3  ; %r9 is %r9, can't be moved upwards
	mov	QWORD[16+rdi],r10
	mov	QWORD[24+rdi],r11
	mov	QWORD[32+rdi],r12
	mov	QWORD[40+rdi],r13
	mov	QWORD[48+rdi],r14
	mov	QWORD[56+rdi],r15
	lea	rdi,[64+rdi]

	cmp	rdi,rdx  ; end of t[]?
	jb	NEAR $L$8x_reduction_loop
	ret



ALIGN	32
__bn_post4x_internal:

	mov	r12,QWORD[rbp]
	lea	rbx,[r9*1+rdi]  ; %rdi was %rbx above
	mov	rcx,r9
	movq	rdi,xmm1  ; restore %rdi
	neg	rax
	movq	rsi,xmm1  ; prepare for back-to-back call
	sar	rcx,3+2
	dec	r12  ; so that after 'not' we get -n[0]
	xor	r10,r10
	mov	r13,QWORD[8+rbp]
	mov	r14,QWORD[16+rbp]
	mov	r15,QWORD[24+rbp]
	jmp	NEAR $L$sqr4x_sub_entry

ALIGN	16
$L$sqr4x_sub:
	mov	r12,QWORD[rbp]
	mov	r13,QWORD[8+rbp]
	mov	r14,QWORD[16+rbp]
	mov	r15,QWORD[24+rbp]
$L$sqr4x_sub_entry:
	lea	rbp,[32+rbp]
	not	r12
	not	r13
	not	r14
	not	r15
	and	r12,rax
	and	r13,rax
	and	r14,rax
	and	r15,rax

	neg	r10  ; mov %r10,%cf
	adc	r12,QWORD[rbx]
	adc	r13,QWORD[8+rbx]
	adc	r14,QWORD[16+rbx]
	adc	r15,QWORD[24+rbx]
	mov	QWORD[rdi],r12
	lea	rbx,[32+rbx]
	mov	QWORD[8+rdi],r13
	sbb	r10,r10  ; mov %cf,%r10
	mov	QWORD[16+rdi],r14
	mov	QWORD[24+rdi],r15
	lea	rdi,[32+rdi]

	inc	rcx  ; pass %cf
	jnz	NEAR $L$sqr4x_sub

	mov	r10,r9  ; prepare for back-to-back call
	neg	r9  ; restore %r9
	ret


global	bn_mulx4x_mont_gather5

ALIGN	32
bn_mulx4x_mont_gather5:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mulx4x_mont_gather5:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$mulx4x_prologue:

; num is declared as an int, a 32-bit parameter, so the upper half is
; undefined. It is important that this write to %r9, which zeros the
; upper half, predates the first access.
	shl	r9d,3  ; convert %r9 to bytes
	lea	r10,[r9*2+r9]  ; 3*%r9 in bytes
	neg	r9  ; -%r9
	mov	r8,QWORD[r8]  ; *n0

; #############################################################
; Ensure that stack frame doesn't alias with +3*%r9
; modulo 4096, which covers ret[num], am[num] and n[num]
; (see bn_exp.c). This is done to allow memory disambiguation
; logic do its magic. [Extra [num] is allocated in order
; to align with bn_power5's frame, which is cleansed after
; completing exponentiation. Extra 256 bytes is for power mask
; calculated from 7th argument, the index.]
; 
	lea	r11,[((-320))+r9*2+rsp]
	mov	rbp,rsp
	sub	r11,rdi
	and	r11,4095
	cmp	r10,r11
	jb	NEAR $L$mulx4xsp_alt
	sub	rbp,r11  ; align with
	lea	rbp,[((-320))+r9*2+rbp]  ; future alloca(frame+2*%r9*8+256)
	jmp	NEAR $L$mulx4xsp_done

$L$mulx4xsp_alt:
	lea	r10,[((4096-320))+r9*2]
	lea	rbp,[((-320))+r9*2+rbp]  ; future alloca(frame+2*%r9*8+256)
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rbp,r11
$L$mulx4xsp_done:
	and	rbp,-64  ; ensure alignment
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,[rbp*1+r11]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$mulx4x_page_walk
	jmp	NEAR $L$mulx4x_page_walk_done

$L$mulx4x_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$mulx4x_page_walk
$L$mulx4x_page_walk_done:

; #############################################################
; Stack layout
; +0	-num
; +8	off-loaded &b[i]
; +16	end of b[num]
; +24	inner counter
; +32	saved n0
; +40	saved %rsp
; +48
; +56	saved rp
; +64	tmp[num+1]
; 
	mov	QWORD[32+rsp],r8  ; save *n0
	mov	QWORD[40+rsp],rax  ; save original %rsp

$L$mulx4x_body:
	call	mulx4x_internal

	mov	rsi,QWORD[40+rsp]  ; restore %rsp

	mov	rax,1

	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$mulx4x_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_bn_mulx4x_mont_gather5:


ALIGN	32
mulx4x_internal:

	mov	QWORD[8+rsp],r9  ; save -%r9 (it was in bytes)
	mov	r10,r9
	neg	r9  ; restore %r9
	shl	r9,5
	neg	r10  ; restore %r9
	lea	r13,[128+r9*1+rdx]  ; end of powers table (+size optimization)
	shr	r9,5+5
	movd	xmm5,DWORD[56+rax]  ; load 7th argument
	sub	r9,1
	lea	rax,[$L$inc]
	mov	QWORD[((16+8))+rsp],r13  ; end of b[num]
	mov	QWORD[((24+8))+rsp],r9  ; inner counter
	mov	QWORD[((56+8))+rsp],rdi  ; save %rdi
	movdqa	xmm0,XMMWORD[rax]  ; 00000001000000010000000000000000
	movdqa	xmm1,XMMWORD[16+rax]  ; 00000002000000020000000200000002
	lea	r10,[((88-112))+r10*1+rsp]  ; place the mask after tp[num+1] (+ICache optimization)
	lea	rdi,[128+rdx]  ; size optimization

	pshufd	xmm5,xmm5,0  ; broadcast index
	movdqa	xmm4,xmm1
	DB	0x67
	movdqa	xmm2,xmm1
	DB	0x67
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5  ; compare to 1,0
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[112+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[128+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[144+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[160+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[176+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[192+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[208+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[224+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[240+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[256+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[272+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD[288+r10],xmm3
	movdqa	xmm3,xmm4
	DB	0x67
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD[304+r10],xmm0

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD[320+r10],xmm1

	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD[336+r10],xmm2

	pand	xmm0,XMMWORD[64+rdi]  ; while it's still in register
	pand	xmm1,XMMWORD[80+rdi]
	pand	xmm2,XMMWORD[96+rdi]
	movdqa	XMMWORD[352+r10],xmm3
	pand	xmm3,XMMWORD[112+rdi]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[((-128))+rdi]
	movdqa	xmm5,XMMWORD[((-112))+rdi]
	movdqa	xmm2,XMMWORD[((-96))+rdi]
	pand	xmm4,XMMWORD[112+r10]
	movdqa	xmm3,XMMWORD[((-80))+rdi]
	pand	xmm5,XMMWORD[128+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[144+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[160+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[((-64))+rdi]
	movdqa	xmm5,XMMWORD[((-48))+rdi]
	movdqa	xmm2,XMMWORD[((-32))+rdi]
	pand	xmm4,XMMWORD[176+r10]
	movdqa	xmm3,XMMWORD[((-16))+rdi]
	pand	xmm5,XMMWORD[192+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[208+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[224+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD[rdi]
	movdqa	xmm5,XMMWORD[16+rdi]
	movdqa	xmm2,XMMWORD[32+rdi]
	pand	xmm4,XMMWORD[240+r10]
	movdqa	xmm3,XMMWORD[48+rdi]
	pand	xmm5,XMMWORD[256+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD[272+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD[288+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	pxor	xmm0,xmm1
; Combine the upper and lower halves of %xmm0.
	pshufd	xmm1,xmm0,0x4e  ; Swap upper and lower halves.
	por	xmm0,xmm1
	lea	rdi,[256+rdi]
	movq	rdx,xmm0  ; bp[0]
	lea	rbx,[((64+32+8))+rsp]

	mov	r9,rdx
	mulx	rax,r8,QWORD[rsi]  ; a[0]*b[0]
	mulx	r12,r11,QWORD[8+rsi]  ; a[1]*b[0]
	add	r11,rax
	mulx	r13,rax,QWORD[16+rsi]  ; ...
	adc	r12,rax
	adc	r13,0
	mulx	r14,rax,QWORD[24+rsi]

	mov	r15,r8
	imul	r8,QWORD[((32+8))+rsp]  ; "t[0]"*n0
	xor	rbp,rbp  ; cf=0, of=0
	mov	rdx,r8

	mov	QWORD[((8+8))+rsp],rdi  ; off-load &b[i]

	lea	rsi,[32+rsi]
	adcx	r13,rax
	adcx	r14,rbp  ; cf=0

	mulx	r10,rax,QWORD[rcx]
	adcx	r15,rax  ; discarded
	adox	r10,r11
	mulx	r11,rax,QWORD[8+rcx]
	adcx	r10,rax
	adox	r11,r12
	mulx	r12,rax,QWORD[16+rcx]
	mov	rdi,QWORD[((24+8))+rsp]  ; counter value
	mov	QWORD[((-32))+rbx],r10
	adcx	r11,rax
	adox	r12,r13
	mulx	r15,rax,QWORD[24+rcx]
	mov	rdx,r9
	mov	QWORD[((-24))+rbx],r11
	adcx	r12,rax
	adox	r15,rbp  ; of=0
	lea	rcx,[32+rcx]
	mov	QWORD[((-16))+rbx],r12
	jmp	NEAR $L$mulx4x_1st

ALIGN	32
$L$mulx4x_1st:
	adcx	r15,rbp  ; cf=0, modulo-scheduled
	mulx	rax,r10,QWORD[rsi]  ; a[4]*b[0]
	adcx	r10,r14
	mulx	r14,r11,QWORD[8+rsi]  ; a[5]*b[0]
	adcx	r11,rax
	mulx	rax,r12,QWORD[16+rsi]  ; ...
	adcx	r12,r14
	mulx	r14,r13,QWORD[24+rsi]
	DB	0x67,0x67
	mov	rdx,r8
	adcx	r13,rax
	adcx	r14,rbp  ; cf=0
	lea	rsi,[32+rsi]
	lea	rbx,[32+rbx]

	adox	r10,r15
	mulx	r15,rax,QWORD[rcx]
	adcx	r10,rax
	adox	r11,r15
	mulx	r15,rax,QWORD[8+rcx]
	adcx	r11,rax
	adox	r12,r15
	mulx	r15,rax,QWORD[16+rcx]
	mov	QWORD[((-40))+rbx],r10
	adcx	r12,rax
	mov	QWORD[((-32))+rbx],r11
	adox	r13,r15
	mulx	r15,rax,QWORD[24+rcx]
	mov	rdx,r9
	mov	QWORD[((-24))+rbx],r12
	adcx	r13,rax
	adox	r15,rbp
	lea	rcx,[32+rcx]
	mov	QWORD[((-16))+rbx],r13

	dec	rdi  ; of=0, pass cf
	jnz	NEAR $L$mulx4x_1st

	mov	rax,QWORD[8+rsp]  ; load -num
	adc	r15,rbp  ; modulo-scheduled
	lea	rsi,[rax*1+rsi]  ; rewind %rsi
	add	r14,r15
	mov	rdi,QWORD[((8+8))+rsp]  ; re-load &b[i]
	adc	rbp,rbp  ; top-most carry
	mov	QWORD[((-8))+rbx],r14
	jmp	NEAR $L$mulx4x_outer

ALIGN	32
$L$mulx4x_outer:
	lea	r10,[((16-256))+rbx]  ; where 256-byte mask is (+density control)
	pxor	xmm4,xmm4
	DB	0x67,0x67
	pxor	xmm5,xmm5
	movdqa	xmm0,XMMWORD[((-128))+rdi]
	movdqa	xmm1,XMMWORD[((-112))+rdi]
	movdqa	xmm2,XMMWORD[((-96))+rdi]
	pand	xmm0,XMMWORD[256+r10]
	movdqa	xmm3,XMMWORD[((-80))+rdi]
	pand	xmm1,XMMWORD[272+r10]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[288+r10]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[304+r10]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[((-64))+rdi]
	movdqa	xmm1,XMMWORD[((-48))+rdi]
	movdqa	xmm2,XMMWORD[((-32))+rdi]
	pand	xmm0,XMMWORD[320+r10]
	movdqa	xmm3,XMMWORD[((-16))+rdi]
	pand	xmm1,XMMWORD[336+r10]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[352+r10]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[368+r10]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[rdi]
	movdqa	xmm1,XMMWORD[16+rdi]
	movdqa	xmm2,XMMWORD[32+rdi]
	pand	xmm0,XMMWORD[384+r10]
	movdqa	xmm3,XMMWORD[48+rdi]
	pand	xmm1,XMMWORD[400+r10]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[416+r10]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[432+r10]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[64+rdi]
	movdqa	xmm1,XMMWORD[80+rdi]
	movdqa	xmm2,XMMWORD[96+rdi]
	pand	xmm0,XMMWORD[448+r10]
	movdqa	xmm3,XMMWORD[112+rdi]
	pand	xmm1,XMMWORD[464+r10]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[480+r10]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[496+r10]
	por	xmm4,xmm2
	por	xmm5,xmm3
	por	xmm4,xmm5
; Combine the upper and lower halves of %xmm4 as %xmm0.
	pshufd	xmm0,xmm4,0x4e  ; Swap upper and lower halves.
	por	xmm0,xmm4
	lea	rdi,[256+rdi]
	movq	rdx,xmm0  ; m0=bp[i]

	mov	QWORD[rbx],rbp  ; save top-most carry
	lea	rbx,[32+rax*1+rbx]  ; rewind %rbx
	mulx	r11,r8,QWORD[rsi]  ; a[0]*b[i]
	xor	rbp,rbp  ; cf=0, of=0
	mov	r9,rdx
	mulx	r12,r14,QWORD[8+rsi]  ; a[1]*b[i]
	adox	r8,QWORD[((-32))+rbx]  ; +t[0]
	adcx	r11,r14
	mulx	r13,r15,QWORD[16+rsi]  ; ...
	adox	r11,QWORD[((-24))+rbx]
	adcx	r12,r15
	mulx	r14,rdx,QWORD[24+rsi]
	adox	r12,QWORD[((-16))+rbx]
	adcx	r13,rdx
	lea	rcx,[rax*1+rcx]  ; rewind %rcx
	lea	rsi,[32+rsi]
	adox	r13,QWORD[((-8))+rbx]
	adcx	r14,rbp
	adox	r14,rbp

	mov	r15,r8
	imul	r8,QWORD[((32+8))+rsp]  ; "t[0]"*n0

	mov	rdx,r8
	xor	rbp,rbp  ; cf=0, of=0
	mov	QWORD[((8+8))+rsp],rdi  ; off-load &b[i]

	mulx	r10,rax,QWORD[rcx]
	adcx	r15,rax  ; discarded
	adox	r10,r11
	mulx	r11,rax,QWORD[8+rcx]
	adcx	r10,rax
	adox	r11,r12
	mulx	r12,rax,QWORD[16+rcx]
	adcx	r11,rax
	adox	r12,r13
	mulx	r15,rax,QWORD[24+rcx]
	mov	rdx,r9
	mov	rdi,QWORD[((24+8))+rsp]  ; counter value
	mov	QWORD[((-32))+rbx],r10
	adcx	r12,rax
	mov	QWORD[((-24))+rbx],r11
	adox	r15,rbp  ; of=0
	mov	QWORD[((-16))+rbx],r12
	lea	rcx,[32+rcx]
	jmp	NEAR $L$mulx4x_inner

ALIGN	32
$L$mulx4x_inner:
	mulx	rax,r10,QWORD[rsi]  ; a[4]*b[i]
	adcx	r15,rbp  ; cf=0, modulo-scheduled
	adox	r10,r14
	mulx	r14,r11,QWORD[8+rsi]  ; a[5]*b[i]
	adcx	r10,QWORD[rbx]
	adox	r11,rax
	mulx	rax,r12,QWORD[16+rsi]  ; ...
	adcx	r11,QWORD[8+rbx]
	adox	r12,r14
	mulx	r14,r13,QWORD[24+rsi]
	mov	rdx,r8
	adcx	r12,QWORD[16+rbx]
	adox	r13,rax
	adcx	r13,QWORD[24+rbx]
	adox	r14,rbp  ; of=0
	lea	rsi,[32+rsi]
	lea	rbx,[32+rbx]
	adcx	r14,rbp  ; cf=0

	adox	r10,r15
	mulx	r15,rax,QWORD[rcx]
	adcx	r10,rax
	adox	r11,r15
	mulx	r15,rax,QWORD[8+rcx]
	adcx	r11,rax
	adox	r12,r15
	mulx	r15,rax,QWORD[16+rcx]
	mov	QWORD[((-40))+rbx],r10
	adcx	r12,rax
	adox	r13,r15
	mov	QWORD[((-32))+rbx],r11
	mulx	r15,rax,QWORD[24+rcx]
	mov	rdx,r9
	lea	rcx,[32+rcx]
	mov	QWORD[((-24))+rbx],r12
	adcx	r13,rax
	adox	r15,rbp
	mov	QWORD[((-16))+rbx],r13

	dec	rdi  ; of=0, pass cf
	jnz	NEAR $L$mulx4x_inner

	mov	rax,QWORD[((0+8))+rsp]  ; load -num
	adc	r15,rbp  ; modulo-scheduled
	sub	rdi,QWORD[rbx]  ; pull top-most carry to %cf
	mov	rdi,QWORD[((8+8))+rsp]  ; re-load &b[i]
	mov	r10,QWORD[((16+8))+rsp]
	adc	r14,r15
	lea	rsi,[rax*1+rsi]  ; rewind %rsi
	adc	rbp,rbp  ; top-most carry
	mov	QWORD[((-8))+rbx],r14

	cmp	rdi,r10
	jb	NEAR $L$mulx4x_outer

	mov	r10,QWORD[((-8))+rcx]
	mov	r8,rbp
	mov	r12,QWORD[rax*1+rcx]
	lea	rbp,[rax*1+rcx]  ; rewind %rcx
	mov	rcx,rax
	lea	rdi,[rax*1+rbx]  ; rewind %rbx
	xor	eax,eax
	xor	r15,r15
	sub	r10,r14  ; compare top-most words
	adc	r15,r15
	or	r8,r15
	sar	rcx,3+2
	sub	rax,r8  ; %rax=-%r8
	mov	rdx,QWORD[((56+8))+rsp]  ; restore rp
	dec	r12  ; so that after 'not' we get -n[0]
	mov	r13,QWORD[8+rbp]
	xor	r8,r8
	mov	r14,QWORD[16+rbp]
	mov	r15,QWORD[24+rbp]
	jmp	NEAR $L$sqrx4x_sub_entry  ; common post-condition


global	bn_powerx5

ALIGN	32
bn_powerx5:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_powerx5:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$powerx5_prologue:

; num is declared as an int, a 32-bit parameter, so the upper half is
; undefined. It is important that this write to %r9, which zeros the
; upper half, predates the first access.
	shl	r9d,3  ; convert %r9 to bytes
	lea	r10,[r9*2+r9]  ; 3*%r9 in bytes
	neg	r9
	mov	r8,QWORD[r8]  ; *n0

; #############################################################
; Ensure that stack frame doesn't alias with %rdi+3*%r9
; modulo 4096, which covers ret[num], am[num] and n[num]
; (see bn_exp.c). This is done to allow memory disambiguation
; logic do its magic. [Extra 256 bytes is for power mask
; calculated from 7th argument, the index.]
; 
	lea	r11,[((-320))+r9*2+rsp]
	mov	rbp,rsp
	sub	r11,rdi
	and	r11,4095
	cmp	r10,r11
	jb	NEAR $L$pwrx_sp_alt
	sub	rbp,r11  ; align with %rsi
	lea	rbp,[((-320))+r9*2+rbp]  ; future alloca(frame+2*%r9*8+256)
	jmp	NEAR $L$pwrx_sp_done

ALIGN	32
$L$pwrx_sp_alt:
	lea	r10,[((4096-320))+r9*2]
	lea	rbp,[((-320))+r9*2+rbp]  ; alloca(frame+2*%r9*8+256)
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rbp,r11
$L$pwrx_sp_done:
	and	rbp,-64
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,[rbp*1+r11]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$pwrx_page_walk
	jmp	NEAR $L$pwrx_page_walk_done

$L$pwrx_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$pwrx_page_walk
$L$pwrx_page_walk_done:

	mov	r10,r9
	neg	r9

; #############################################################
; Stack layout
; 
; +0	saved %r9, used in reduction section
; +8	&t[2*%r9], used in reduction section
; +16	intermediate carry bit
; +24	top-most carry bit, used in reduction section
; +32	saved *n0
; +40	saved %rsp
; +48	t[2*%r9]
; 
	pxor	xmm0,xmm0
	movq	xmm1,rdi  ; save %rdi
	movq	xmm2,rcx  ; save %rcx
	movq	xmm3,r10  ; -%r9
	movq	xmm4,rdx
	mov	QWORD[32+rsp],r8
	mov	QWORD[40+rsp],rax  ; save original %rsp

$L$powerx5_body:

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

	mov	r9,r10  ; -num
	mov	rdi,rsi
	movq	rcx,xmm2
	movq	rdx,xmm4
	mov	rax,QWORD[40+rsp]

	call	mulx4x_internal

	mov	rsi,QWORD[40+rsp]  ; restore %rsp

	mov	rax,1

	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$powerx5_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_bn_powerx5:

global	bn_sqrx8x_internal


ALIGN	32
bn_sqrx8x_internal:
__bn_sqrx8x_internal:

_CET_ENDBR
; #################################################################
; Squaring part:
; 
; a) multiply-n-add everything but a[i]*a[i];
; b) shift result of a) by 1 to the left and accumulate
; a[i]*a[i] products;
; 
; #################################################################
; a[7]a[7]a[6]a[6]a[5]a[5]a[4]a[4]a[3]a[3]a[2]a[2]a[1]a[1]a[0]a[0]
; a[1]a[0]
; a[2]a[0]
; a[3]a[0]
; a[2]a[1]
; a[3]a[1]
; a[3]a[2]
; 
; a[4]a[0]
; a[5]a[0]
; a[6]a[0]
; a[7]a[0]
; a[4]a[1]
; a[5]a[1]
; a[6]a[1]
; a[7]a[1]
; a[4]a[2]
; a[5]a[2]
; a[6]a[2]
; a[7]a[2]
; a[4]a[3]
; a[5]a[3]
; a[6]a[3]
; a[7]a[3]
; 
; a[5]a[4]
; a[6]a[4]
; a[7]a[4]
; a[6]a[5]
; a[7]a[5]
; a[7]a[6]
; a[7]a[7]a[6]a[6]a[5]a[5]a[4]a[4]a[3]a[3]a[2]a[2]a[1]a[1]a[0]a[0]
	lea	rdi,[((48+8))+rsp]
	lea	rbp,[r9*1+rsi]
	mov	QWORD[((0+8))+rsp],r9  ; save %r9
	mov	QWORD[((8+8))+rsp],rbp  ; save end of %rsi
	jmp	NEAR $L$sqr8x_zero_start

ALIGN	32
	DB	0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00
$L$sqrx8x_zero:
	DB	0x3e
	movdqa	XMMWORD[rdi],xmm0
	movdqa	XMMWORD[16+rdi],xmm0
	movdqa	XMMWORD[32+rdi],xmm0
	movdqa	XMMWORD[48+rdi],xmm0
$L$sqr8x_zero_start:  ; aligned at 32
	movdqa	XMMWORD[64+rdi],xmm0
	movdqa	XMMWORD[80+rdi],xmm0
	movdqa	XMMWORD[96+rdi],xmm0
	movdqa	XMMWORD[112+rdi],xmm0
	lea	rdi,[128+rdi]
	sub	r9,64
	jnz	NEAR $L$sqrx8x_zero

	mov	rdx,QWORD[rsi]  ; a[0], modulo-scheduled
; xor	%r9,%r9			# t[1], ex-%r9, zero already
	xor	r10,r10
	xor	r11,r11
	xor	r12,r12
	xor	r13,r13
	xor	r14,r14
	xor	r15,r15
	lea	rdi,[((48+8))+rsp]
	xor	rbp,rbp  ; cf=0, cf=0
	jmp	NEAR $L$sqrx8x_outer_loop

ALIGN	32
$L$sqrx8x_outer_loop:
	mulx	rax,r8,QWORD[8+rsi]  ; a[1]*a[0]
	adcx	r8,r9  ; a[1]*a[0]+=t[1]
	adox	r10,rax
	mulx	rax,r9,QWORD[16+rsi]  ; a[2]*a[0]
	adcx	r9,r10
	adox	r11,rax
	DB	0xc4,0xe2,0xab,0xf6,0x86,0x18,0x00,0x00,0x00  ; mulx	3*8(%rsi),%r10,%rax	# ...
	adcx	r10,r11
	adox	r12,rax
	DB	0xc4,0xe2,0xa3,0xf6,0x86,0x20,0x00,0x00,0x00  ; mulx	4*8(%rsi),%r11,%rax
	adcx	r11,r12
	adox	r13,rax
	mulx	rax,r12,QWORD[40+rsi]
	adcx	r12,r13
	adox	r14,rax
	mulx	rax,r13,QWORD[48+rsi]
	adcx	r13,r14
	adox	rax,r15
	mulx	r15,r14,QWORD[56+rsi]
	mov	rdx,QWORD[8+rsi]  ; a[1]
	adcx	r14,rax
	adox	r15,rbp
	adc	r15,QWORD[64+rdi]
	mov	QWORD[8+rdi],r8  ; t[1]
	mov	QWORD[16+rdi],r9  ; t[2]
	sbb	rcx,rcx  ; mov %cf,%rcx
	xor	rbp,rbp  ; cf=0, of=0


	mulx	rbx,r8,QWORD[16+rsi]  ; a[2]*a[1]
	mulx	rax,r9,QWORD[24+rsi]  ; a[3]*a[1]
	adcx	r8,r10
	adox	r9,rbx
	mulx	rbx,r10,QWORD[32+rsi]  ; ...
	adcx	r9,r11
	adox	r10,rax
	DB	0xc4,0xe2,0xa3,0xf6,0x86,0x28,0x00,0x00,0x00  ; mulx	5*8(%rsi),%r11,%rax
	adcx	r10,r12
	adox	r11,rbx
	DB	0xc4,0xe2,0x9b,0xf6,0x9e,0x30,0x00,0x00,0x00  ; mulx	6*8(%rsi),%r12,%rbx
	adcx	r11,r13
	adox	r12,r14
	DB	0xc4,0x62,0x93,0xf6,0xb6,0x38,0x00,0x00,0x00  ; mulx	7*8(%rsi),%r13,%r14
	mov	rdx,QWORD[16+rsi]  ; a[2]
	adcx	r12,rax
	adox	r13,rbx
	adcx	r13,r15
	adox	r14,rbp  ; of=0
	adcx	r14,rbp  ; cf=0

	mov	QWORD[24+rdi],r8  ; t[3]
	mov	QWORD[32+rdi],r9  ; t[4]

	mulx	rbx,r8,QWORD[24+rsi]  ; a[3]*a[2]
	mulx	rax,r9,QWORD[32+rsi]  ; a[4]*a[2]
	adcx	r8,r10
	adox	r9,rbx
	mulx	rbx,r10,QWORD[40+rsi]  ; ...
	adcx	r9,r11
	adox	r10,rax
	DB	0xc4,0xe2,0xa3,0xf6,0x86,0x30,0x00,0x00,0x00  ; mulx	6*8(%rsi),%r11,%rax
	adcx	r10,r12
	adox	r11,r13
	DB	0xc4,0x62,0x9b,0xf6,0xae,0x38,0x00,0x00,0x00  ; mulx	7*8(%rsi),%r12,%r13
	DB	0x3e
	mov	rdx,QWORD[24+rsi]  ; a[3]
	adcx	r11,rbx
	adox	r12,rax
	adcx	r12,r14
	mov	QWORD[40+rdi],r8  ; t[5]
	mov	QWORD[48+rdi],r9  ; t[6]
	mulx	rax,r8,QWORD[32+rsi]  ; a[4]*a[3]
	adox	r13,rbp  ; of=0
	adcx	r13,rbp  ; cf=0

	mulx	rbx,r9,QWORD[40+rsi]  ; a[5]*a[3]
	adcx	r8,r10
	adox	r9,rax
	mulx	rax,r10,QWORD[48+rsi]  ; ...
	adcx	r9,r11
	adox	r10,r12
	mulx	r12,r11,QWORD[56+rsi]
	mov	rdx,QWORD[32+rsi]  ; a[4]
	mov	r14,QWORD[40+rsi]  ; a[5]
	adcx	r10,rbx
	adox	r11,rax
	mov	r15,QWORD[48+rsi]  ; a[6]
	adcx	r11,r13
	adox	r12,rbp  ; of=0
	adcx	r12,rbp  ; cf=0

	mov	QWORD[56+rdi],r8  ; t[7]
	mov	QWORD[64+rdi],r9  ; t[8]

	mulx	rax,r9,r14  ; a[5]*a[4]
	mov	r8,QWORD[56+rsi]  ; a[7]
	adcx	r9,r10
	mulx	rbx,r10,r15  ; a[6]*a[4]
	adox	r10,rax
	adcx	r10,r11
	mulx	rax,r11,r8  ; a[7]*a[4]
	mov	rdx,r14  ; a[5]
	adox	r11,rbx
	adcx	r11,r12
; adox	%rbp,%rax		# of=0
	adcx	rax,rbp  ; cf=0

	mulx	rbx,r14,r15  ; a[6]*a[5]
	mulx	r13,r12,r8  ; a[7]*a[5]
	mov	rdx,r15  ; a[6]
	lea	rsi,[64+rsi]
	adcx	r11,r14
	adox	r12,rbx
	adcx	r12,rax
	adox	r13,rbp

	DB	0x67,0x67
	mulx	r14,r8,r8  ; a[7]*a[6]
	adcx	r13,r8
	adcx	r14,rbp

	cmp	rsi,QWORD[((8+8))+rsp]
	je	NEAR $L$sqrx8x_outer_break

	neg	rcx  ; mov %rcx,%cf
	mov	rcx,-8
	mov	r15,rbp
	mov	r8,QWORD[64+rdi]
	adcx	r9,QWORD[72+rdi]  ; +=t[9]
	adcx	r10,QWORD[80+rdi]  ; ...
	adcx	r11,QWORD[88+rdi]
	adc	r12,QWORD[96+rdi]
	adc	r13,QWORD[104+rdi]
	adc	r14,QWORD[112+rdi]
	adc	r15,QWORD[120+rdi]
	lea	rbp,[rsi]
	lea	rdi,[128+rdi]
	sbb	rax,rax  ; mov %cf,%rcx

	mov	rdx,QWORD[((-64))+rsi]  ; a[0]
	mov	QWORD[((16+8))+rsp],rax  ; offload %rcx
	mov	QWORD[((24+8))+rsp],rdi

; lea	8*8(%rdi),%rdi	# see 2*8*8(%rdi) above
	xor	eax,eax  ; cf=0, of=0
	jmp	NEAR $L$sqrx8x_loop

ALIGN	32
$L$sqrx8x_loop:
	mov	rbx,r8
	mulx	r8,rax,QWORD[rbp]  ; a[8]*a[i]
	adcx	rbx,rax  ; +=t[8]
	adox	r8,r9

	mulx	r9,rax,QWORD[8+rbp]  ; ...
	adcx	r8,rax
	adox	r9,r10

	mulx	r10,rax,QWORD[16+rbp]
	adcx	r9,rax
	adox	r10,r11

	mulx	r11,rax,QWORD[24+rbp]
	adcx	r10,rax
	adox	r11,r12

	DB	0xc4,0x62,0xfb,0xf6,0xa5,0x20,0x00,0x00,0x00  ; mulx	4*8(%rbp),%rax,%r12
	adcx	r11,rax
	adox	r12,r13

	mulx	r13,rax,QWORD[40+rbp]
	adcx	r12,rax
	adox	r13,r14

	mulx	r14,rax,QWORD[48+rbp]
	mov	QWORD[rcx*8+rdi],rbx  ; store t[8+i]
	mov	ebx,0
	adcx	r13,rax
	adox	r14,r15

	DB	0xc4,0x62,0xfb,0xf6,0xbd,0x38,0x00,0x00,0x00  ; mulx	7*8(%rbp),%rax,%r15
	mov	rdx,QWORD[8+rcx*8+rsi]  ; a[i]
	adcx	r14,rax
	adox	r15,rbx  ; %rbx is 0, of=0
	adcx	r15,rbx  ; cf=0

	DB	0x67
	inc	rcx  ; of=0
	jnz	NEAR $L$sqrx8x_loop

	lea	rbp,[64+rbp]
	mov	rcx,-8
	cmp	rbp,QWORD[((8+8))+rsp]  ; done?
	je	NEAR $L$sqrx8x_break

	sub	rbx,QWORD[((16+8))+rsp]  ; mov 16(%rsp),%cf
	DB	0x66
	mov	rdx,QWORD[((-64))+rsi]
	adcx	r8,QWORD[rdi]
	adcx	r9,QWORD[8+rdi]
	adc	r10,QWORD[16+rdi]
	adc	r11,QWORD[24+rdi]
	adc	r12,QWORD[32+rdi]
	adc	r13,QWORD[40+rdi]
	adc	r14,QWORD[48+rdi]
	adc	r15,QWORD[56+rdi]
	lea	rdi,[64+rdi]
	DB	0x67
	sbb	rax,rax  ; mov %cf,%rax
	xor	ebx,ebx  ; cf=0, of=0
	mov	QWORD[((16+8))+rsp],rax  ; offload carry
	jmp	NEAR $L$sqrx8x_loop

ALIGN	32
$L$sqrx8x_break:
	xor	rbp,rbp
	sub	rbx,QWORD[((16+8))+rsp]  ; mov 16(%rsp),%cf
	adcx	r8,rbp
	mov	rcx,QWORD[((24+8))+rsp]  ; initial %rdi, borrow %rcx
	adcx	r9,rbp
	mov	rdx,QWORD[rsi]  ; a[8], modulo-scheduled
	adc	r10,0
	mov	QWORD[rdi],r8
	adc	r11,0
	adc	r12,0
	adc	r13,0
	adc	r14,0
	adc	r15,0
	cmp	rdi,rcx  ; cf=0, of=0
	je	NEAR $L$sqrx8x_outer_loop

	mov	QWORD[8+rdi],r9
	mov	r9,QWORD[8+rcx]
	mov	QWORD[16+rdi],r10
	mov	r10,QWORD[16+rcx]
	mov	QWORD[24+rdi],r11
	mov	r11,QWORD[24+rcx]
	mov	QWORD[32+rdi],r12
	mov	r12,QWORD[32+rcx]
	mov	QWORD[40+rdi],r13
	mov	r13,QWORD[40+rcx]
	mov	QWORD[48+rdi],r14
	mov	r14,QWORD[48+rcx]
	mov	QWORD[56+rdi],r15
	mov	r15,QWORD[56+rcx]
	mov	rdi,rcx
	jmp	NEAR $L$sqrx8x_outer_loop

ALIGN	32
$L$sqrx8x_outer_break:
	mov	QWORD[72+rdi],r9  ; t[9]
	movq	rcx,xmm3  ; -%r9
	mov	QWORD[80+rdi],r10  ; ...
	mov	QWORD[88+rdi],r11
	mov	QWORD[96+rdi],r12
	mov	QWORD[104+rdi],r13
	mov	QWORD[112+rdi],r14
	lea	rdi,[((48+8))+rsp]
	mov	rdx,QWORD[rcx*1+rsi]  ; a[0]

	mov	r11,QWORD[8+rdi]  ; t[1]
	xor	r10,r10  ; t[0], of=0, cf=0
	mov	r9,QWORD[((0+8))+rsp]  ; restore %r9
	adox	r11,r11
	mov	r12,QWORD[16+rdi]  ; t[2]	# prefetch
	mov	r13,QWORD[24+rdi]  ; t[3]	# prefetch
; jmp	.Lsqrx4x_shift_n_add	# happens to be aligned

ALIGN	32
$L$sqrx4x_shift_n_add:
	mulx	rbx,rax,rdx
	adox	r12,r12
	adcx	rax,r10
	DB	0x48,0x8b,0x94,0x0e,0x08,0x00,0x00,0x00  ; mov	8(%rsi,%rcx),%rdx	# a[i+1]	# prefetch
	DB	0x4c,0x8b,0x97,0x20,0x00,0x00,0x00  ; mov	32(%rdi),%r10	# t[2*i+4]	# prefetch
	adox	r13,r13
	adcx	rbx,r11
	mov	r11,QWORD[40+rdi]  ; t[2*i+4+1]	# prefetch
	mov	QWORD[rdi],rax
	mov	QWORD[8+rdi],rbx

	mulx	rbx,rax,rdx
	adox	r10,r10
	adcx	rax,r12
	mov	rdx,QWORD[16+rcx*1+rsi]  ; a[i+2]	# prefetch
	mov	r12,QWORD[48+rdi]  ; t[2*i+6]	# prefetch
	adox	r11,r11
	adcx	rbx,r13
	mov	r13,QWORD[56+rdi]  ; t[2*i+6+1]	# prefetch
	mov	QWORD[16+rdi],rax
	mov	QWORD[24+rdi],rbx

	mulx	rbx,rax,rdx
	adox	r12,r12
	adcx	rax,r10
	mov	rdx,QWORD[24+rcx*1+rsi]  ; a[i+3]	# prefetch
	lea	rcx,[32+rcx]
	mov	r10,QWORD[64+rdi]  ; t[2*i+8]	# prefetch
	adox	r13,r13
	adcx	rbx,r11
	mov	r11,QWORD[72+rdi]  ; t[2*i+8+1]	# prefetch
	mov	QWORD[32+rdi],rax
	mov	QWORD[40+rdi],rbx

	mulx	rbx,rax,rdx
	adox	r10,r10
	adcx	rax,r12
	jrcxz	$L$sqrx4x_shift_n_add_break
	DB	0x48,0x8b,0x94,0x0e,0x00,0x00,0x00,0x00  ; mov	0(%rsi,%rcx),%rdx	# a[i+4]	# prefetch
	adox	r11,r11
	adcx	rbx,r13
	mov	r12,QWORD[80+rdi]  ; t[2*i+10]	# prefetch
	mov	r13,QWORD[88+rdi]  ; t[2*i+10+1]	# prefetch
	mov	QWORD[48+rdi],rax
	mov	QWORD[56+rdi],rbx
	lea	rdi,[64+rdi]
	nop
	jmp	NEAR $L$sqrx4x_shift_n_add

ALIGN	32
$L$sqrx4x_shift_n_add_break:
	adcx	rbx,r13
	mov	QWORD[48+rdi],rax
	mov	QWORD[56+rdi],rbx
	lea	rdi,[64+rdi]  ; end of t[] buffer
	movq	rbp,xmm2
__bn_sqrx8x_reduction:
	xor	eax,eax  ; initial top-most carry bit
	mov	rbx,QWORD[((32+8))+rsp]  ; n0
	mov	rdx,QWORD[((48+8))+rsp]  ; "%r8", 8*0(%rdi)
	lea	rcx,[((-64))+r9*1+rbp]  ; end of n[]
; lea	48+8(%rsp,%r9,2),%rdi	# end of t[] buffer
	mov	QWORD[((0+8))+rsp],rcx  ; save end of n[]
	mov	QWORD[((8+8))+rsp],rdi  ; save end of t[]

	lea	rdi,[((48+8))+rsp]  ; initial t[] window
	jmp	NEAR $L$sqrx8x_reduction_loop

ALIGN	32
$L$sqrx8x_reduction_loop:
	mov	r9,QWORD[8+rdi]
	mov	r10,QWORD[16+rdi]
	mov	r11,QWORD[24+rdi]
	mov	r12,QWORD[32+rdi]
	mov	r8,rdx
	imul	rdx,rbx  ; n0*a[i]
	mov	r13,QWORD[40+rdi]
	mov	r14,QWORD[48+rdi]
	mov	r15,QWORD[56+rdi]
	mov	QWORD[((24+8))+rsp],rax  ; store top-most carry bit

	lea	rdi,[64+rdi]
	xor	rsi,rsi  ; cf=0,of=0
	mov	rcx,-8
	jmp	NEAR $L$sqrx8x_reduce

ALIGN	32
$L$sqrx8x_reduce:
	mov	rbx,r8
	mulx	r8,rax,QWORD[rbp]  ; n[0]
	adcx	rax,rbx  ; discarded
	adox	r8,r9

	mulx	r9,rbx,QWORD[8+rbp]  ; n[1]
	adcx	r8,rbx
	adox	r9,r10

	mulx	r10,rbx,QWORD[16+rbp]
	adcx	r9,rbx
	adox	r10,r11

	mulx	r11,rbx,QWORD[24+rbp]
	adcx	r10,rbx
	adox	r11,r12

	DB	0xc4,0x62,0xe3,0xf6,0xa5,0x20,0x00,0x00,0x00  ; mulx	8*4(%rbp),%rbx,%r12
	mov	rax,rdx
	mov	rdx,r8
	adcx	r11,rbx
	adox	r12,r13

	mulx	rdx,rbx,QWORD[((32+8))+rsp]  ; %rdx discarded
	mov	rdx,rax
	mov	QWORD[((64+48+8))+rcx*8+rsp],rax  ; put aside n0*a[i]

	mulx	r13,rax,QWORD[40+rbp]
	adcx	r12,rax
	adox	r13,r14

	mulx	r14,rax,QWORD[48+rbp]
	adcx	r13,rax
	adox	r14,r15

	mulx	r15,rax,QWORD[56+rbp]
	mov	rdx,rbx
	adcx	r14,rax
	adox	r15,rsi  ; %rsi is 0
	adcx	r15,rsi  ; cf=0

	DB	0x67,0x67,0x67
	inc	rcx  ; of=0
	jnz	NEAR $L$sqrx8x_reduce

	mov	rax,rsi  ; xor	%rax,%rax
	cmp	rbp,QWORD[((0+8))+rsp]  ; end of n[]?
	jae	NEAR $L$sqrx8x_no_tail

	mov	rdx,QWORD[((48+8))+rsp]  ; pull n0*a[0]
	add	r8,QWORD[rdi]
	lea	rbp,[64+rbp]
	mov	rcx,-8
	adcx	r9,QWORD[8+rdi]
	adcx	r10,QWORD[16+rdi]
	adc	r11,QWORD[24+rdi]
	adc	r12,QWORD[32+rdi]
	adc	r13,QWORD[40+rdi]
	adc	r14,QWORD[48+rdi]
	adc	r15,QWORD[56+rdi]
	lea	rdi,[64+rdi]
	sbb	rax,rax  ; top carry

	xor	rsi,rsi  ; of=0, cf=0
	mov	QWORD[((16+8))+rsp],rax
	jmp	NEAR $L$sqrx8x_tail

ALIGN	32
$L$sqrx8x_tail:
	mov	rbx,r8
	mulx	r8,rax,QWORD[rbp]
	adcx	rbx,rax
	adox	r8,r9

	mulx	r9,rax,QWORD[8+rbp]
	adcx	r8,rax
	adox	r9,r10

	mulx	r10,rax,QWORD[16+rbp]
	adcx	r9,rax
	adox	r10,r11

	mulx	r11,rax,QWORD[24+rbp]
	adcx	r10,rax
	adox	r11,r12

	DB	0xc4,0x62,0xfb,0xf6,0xa5,0x20,0x00,0x00,0x00  ; mulx	8*4(%rbp),%rax,%r12
	adcx	r11,rax
	adox	r12,r13

	mulx	r13,rax,QWORD[40+rbp]
	adcx	r12,rax
	adox	r13,r14

	mulx	r14,rax,QWORD[48+rbp]
	adcx	r13,rax
	adox	r14,r15

	mulx	r15,rax,QWORD[56+rbp]
	mov	rdx,QWORD[((72+48+8))+rcx*8+rsp]  ; pull n0*a[i]
	adcx	r14,rax
	adox	r15,rsi
	mov	QWORD[rcx*8+rdi],rbx  ; save result
	mov	rbx,r8
	adcx	r15,rsi  ; cf=0

	inc	rcx  ; of=0
	jnz	NEAR $L$sqrx8x_tail

	cmp	rbp,QWORD[((0+8))+rsp]  ; end of n[]?
	jae	NEAR $L$sqrx8x_tail_done  ; break out of loop

	sub	rsi,QWORD[((16+8))+rsp]  ; mov 16(%rsp),%cf
	mov	rdx,QWORD[((48+8))+rsp]  ; pull n0*a[0]
	lea	rbp,[64+rbp]
	adc	r8,QWORD[rdi]
	adc	r9,QWORD[8+rdi]
	adc	r10,QWORD[16+rdi]
	adc	r11,QWORD[24+rdi]
	adc	r12,QWORD[32+rdi]
	adc	r13,QWORD[40+rdi]
	adc	r14,QWORD[48+rdi]
	adc	r15,QWORD[56+rdi]
	lea	rdi,[64+rdi]
	sbb	rax,rax
	sub	rcx,8  ; mov	$-8,%rcx

	xor	rsi,rsi  ; of=0, cf=0
	mov	QWORD[((16+8))+rsp],rax
	jmp	NEAR $L$sqrx8x_tail

ALIGN	32
$L$sqrx8x_tail_done:
	xor	rax,rax
	add	r8,QWORD[((24+8))+rsp]  ; can this overflow?
	adc	r9,0
	adc	r10,0
	adc	r11,0
	adc	r12,0
	adc	r13,0
	adc	r14,0
	adc	r15,0
	adc	rax,0

	sub	rsi,QWORD[((16+8))+rsp]  ; mov 16(%rsp),%cf
$L$sqrx8x_no_tail:  ; %cf is 0 if jumped here
	adc	r8,QWORD[rdi]
	movq	rcx,xmm3
	adc	r9,QWORD[8+rdi]
	mov	rsi,QWORD[56+rbp]
	movq	rbp,xmm2  ; restore %rbp
	adc	r10,QWORD[16+rdi]
	adc	r11,QWORD[24+rdi]
	adc	r12,QWORD[32+rdi]
	adc	r13,QWORD[40+rdi]
	adc	r14,QWORD[48+rdi]
	adc	r15,QWORD[56+rdi]
	adc	rax,0  ; top-most carry

	mov	rbx,QWORD[((32+8))+rsp]  ; n0
	mov	rdx,QWORD[64+rcx*1+rdi]  ; modulo-scheduled "%r8"

	mov	QWORD[rdi],r8  ; store top 512 bits
	lea	r8,[64+rdi]  ; borrow %r8
	mov	QWORD[8+rdi],r9
	mov	QWORD[16+rdi],r10
	mov	QWORD[24+rdi],r11
	mov	QWORD[32+rdi],r12
	mov	QWORD[40+rdi],r13
	mov	QWORD[48+rdi],r14
	mov	QWORD[56+rdi],r15

	lea	rdi,[64+rcx*1+rdi]  ; start of current t[] window
	cmp	r8,QWORD[((8+8))+rsp]  ; end of t[]?
	jb	NEAR $L$sqrx8x_reduction_loop
	ret


ALIGN	32

__bn_postx4x_internal:

	mov	r12,QWORD[rbp]
	mov	r10,rcx  ; -%r9
	mov	r9,rcx  ; -%r9
	neg	rax
	sar	rcx,3+2
; lea	48+8(%rsp,%r9),%rdi
	movq	rdx,xmm1  ; restore %rdx
	movq	rsi,xmm1  ; prepare for back-to-back call
	dec	r12  ; so that after 'not' we get -n[0]
	mov	r13,QWORD[8+rbp]
	xor	r8,r8
	mov	r14,QWORD[16+rbp]
	mov	r15,QWORD[24+rbp]
	jmp	NEAR $L$sqrx4x_sub_entry

ALIGN	16
$L$sqrx4x_sub:
	mov	r12,QWORD[rbp]
	mov	r13,QWORD[8+rbp]
	mov	r14,QWORD[16+rbp]
	mov	r15,QWORD[24+rbp]
$L$sqrx4x_sub_entry:
	andn	r12,r12,rax
	lea	rbp,[32+rbp]
	andn	r13,r13,rax
	andn	r14,r14,rax
	andn	r15,r15,rax

	neg	r8  ; mov %r8,%cf
	adc	r12,QWORD[rdi]
	adc	r13,QWORD[8+rdi]
	adc	r14,QWORD[16+rdi]
	adc	r15,QWORD[24+rdi]
	mov	QWORD[rdx],r12
	lea	rdi,[32+rdi]
	mov	QWORD[8+rdx],r13
	sbb	r8,r8  ; mov %cf,%r8
	mov	QWORD[16+rdx],r14
	mov	QWORD[24+rdx],r15
	lea	rdx,[32+rdx]

	inc	rcx
	jnz	NEAR $L$sqrx4x_sub

	neg	r9  ; restore %r9

	ret


global	bn_scatter5

ALIGN	16
bn_scatter5:

_CET_ENDBR
	cmp	edx,0
	jz	NEAR $L$scatter_epilogue

; %r8 stores 32 entries, t0 through t31. Each entry has %edx words.
; They are interleaved in memory as follows:
; 
; t0[0]      t1[0]      t2[0]      ... t31[0]
; t0[1]      t1[1]      t2[1]      ... t31[1]
; ...
; t0[%edx-1] t1[%edx-1] t2[%edx-1] ... t31[%edx-1]

	lea	r8,[r9*8+r8]
$L$scatter:
	mov	rax,QWORD[rcx]
	lea	rcx,[8+rcx]
	mov	QWORD[r8],rax
	lea	r8,[256+r8]
	sub	edx,1
	jnz	NEAR $L$scatter
$L$scatter_epilogue:
	ret



global	bn_gather5

ALIGN	32
bn_gather5:

$L$SEH_begin_bn_gather5:  ; Win64 thing, but harmless in other cases
_CET_ENDBR
; I can't trust assembler to use specific encoding:-(
	DB	0x4c,0x8d,0x14,0x24  ; lea    (%rsp),%r10

	DB	0x48,0x81,0xec,0x08,0x01,0x00,0x00  ; sub	$0x108,%rsp
	lea	rax,[$L$inc]
	and	rsp,-16  ; shouldn't be formally required

	movd	xmm5,r9d
	movdqa	xmm0,XMMWORD[rax]  ; 00000001000000010000000000000000
	movdqa	xmm1,XMMWORD[16+rax]  ; 00000002000000020000000200000002
	lea	r11,[128+r8]  ; size optimization
	lea	rax,[128+rsp]  ; size optimization

	pshufd	xmm5,xmm5,0  ; broadcast %r9d
	movdqa	xmm4,xmm1
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5  ; compare to 1,0
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[(-128)+rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[(-112)+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[(-96)+rax],xmm2
	movdqa	xmm2,xmm4
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5  ; compare to 1,0
	movdqa	XMMWORD[(-80)+rax],xmm3
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[(-64)+rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[(-48)+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[(-32)+rax],xmm2
	movdqa	xmm2,xmm4
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5  ; compare to 1,0
	movdqa	XMMWORD[(-16)+rax],xmm3
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[16+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[32+rax],xmm2
	movdqa	xmm2,xmm4
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5  ; compare to 1,0
	movdqa	XMMWORD[48+rax],xmm3
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5  ; compare to 3,2
	movdqa	XMMWORD[64+rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5  ; compare to 5,4
	movdqa	XMMWORD[80+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5  ; compare to 7,6
	movdqa	XMMWORD[96+rax],xmm2
	movdqa	xmm2,xmm4
	movdqa	XMMWORD[112+rax],xmm3
	jmp	NEAR $L$gather

ALIGN	32
$L$gather:
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movdqa	xmm0,XMMWORD[((-128))+r11]
	movdqa	xmm1,XMMWORD[((-112))+r11]
	movdqa	xmm2,XMMWORD[((-96))+r11]
	pand	xmm0,XMMWORD[((-128))+rax]
	movdqa	xmm3,XMMWORD[((-80))+r11]
	pand	xmm1,XMMWORD[((-112))+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[((-96))+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[((-80))+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[((-64))+r11]
	movdqa	xmm1,XMMWORD[((-48))+r11]
	movdqa	xmm2,XMMWORD[((-32))+r11]
	pand	xmm0,XMMWORD[((-64))+rax]
	movdqa	xmm3,XMMWORD[((-16))+r11]
	pand	xmm1,XMMWORD[((-48))+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[((-32))+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[((-16))+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[r11]
	movdqa	xmm1,XMMWORD[16+r11]
	movdqa	xmm2,XMMWORD[32+r11]
	pand	xmm0,XMMWORD[rax]
	movdqa	xmm3,XMMWORD[48+r11]
	pand	xmm1,XMMWORD[16+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[32+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[48+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD[64+r11]
	movdqa	xmm1,XMMWORD[80+r11]
	movdqa	xmm2,XMMWORD[96+r11]
	pand	xmm0,XMMWORD[64+rax]
	movdqa	xmm3,XMMWORD[112+r11]
	pand	xmm1,XMMWORD[80+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD[96+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD[112+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	por	xmm4,xmm5
	lea	r11,[256+r11]
; Combine the upper and lower halves of %xmm0.
	pshufd	xmm0,xmm4,0x4e  ; Swap upper and lower halves.
	por	xmm0,xmm4
	movq	QWORD[rcx],xmm0  ; m0=bp[0]
	lea	rcx,[8+rcx]
	sub	edx,1
	jnz	NEAR $L$gather

	lea	rsp,[r10]

	ret
$L$SEH_end_bn_gather5:


section	.rdata rdata align=8

ALIGN	64
mont5_increments:
$L$inc:
	DD	0,0,1,1
	DD	2,2,2,2
	DB	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105
	DB	112,108,105,99,97,116,105,111,110,32,119,105,116,104,32,115
	DB	99,97,116,116,101,114,47,103,97,116,104,101,114,32,102,111
	DB	114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79
	DB	71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111
	DB	112,101,110,115,115,108,46,111,114,103,62,0
section	.text code align=64

EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
mul_handler:
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD[120+r8]  ; pull context->Rax
	mov	rbx,QWORD[248+r8]  ; pull context->Rip

	mov	rsi,QWORD[8+r9]  ; disp->ImageBase
	mov	r11,QWORD[56+r9]  ; disp->HandlerData

	mov	r10d,DWORD[r11]  ; HandlerData[0]
	lea	r10,[r10*1+rsi]  ; end of prologue label
	cmp	rbx,r10  ; context->Rip<end of prologue label
	jb	NEAR $L$common_seh_tail

	mov	r10d,DWORD[4+r11]  ; HandlerData[1]
	lea	r10,[r10*1+rsi]  ; beginning of body label
	cmp	rbx,r10  ; context->Rip<body label
	jb	NEAR $L$common_pop_regs

	mov	rax,QWORD[152+r8]  ; pull context->Rsp

	mov	r10d,DWORD[8+r11]  ; HandlerData[2]
	lea	r10,[r10*1+rsi]  ; epilogue label
	cmp	rbx,r10  ; context->Rip>=epilogue label
	jae	NEAR $L$common_seh_tail

	lea	r10,[$L$mul_epilogue]
	cmp	rbx,r10
	ja	NEAR $L$body_40

	mov	r10,QWORD[192+r8]  ; pull %r9
	mov	rax,QWORD[8+r10*8+rax]  ; pull saved stack pointer

	jmp	NEAR $L$common_pop_regs

$L$body_40:
	mov	rax,QWORD[40+rax]  ; pull saved stack pointer
$L$common_pop_regs:
	mov	rbx,QWORD[((-8))+rax]
	mov	rbp,QWORD[((-16))+rax]
	mov	r12,QWORD[((-24))+rax]
	mov	r13,QWORD[((-32))+rax]
	mov	r14,QWORD[((-40))+rax]
	mov	r15,QWORD[((-48))+rax]
	mov	QWORD[144+r8],rbx  ; restore context->Rbx
	mov	QWORD[160+r8],rbp  ; restore context->Rbp
	mov	QWORD[216+r8],r12  ; restore context->R12
	mov	QWORD[224+r8],r13  ; restore context->R13
	mov	QWORD[232+r8],r14  ; restore context->R14
	mov	QWORD[240+r8],r15  ; restore context->R15

$L$common_seh_tail:
	mov	rdi,QWORD[8+rax]
	mov	rsi,QWORD[16+rax]
	mov	QWORD[152+r8],rax  ; restore context->Rsp
	mov	QWORD[168+r8],rsi  ; restore context->Rsi
	mov	QWORD[176+r8],rdi  ; restore context->Rdi

	mov	rdi,QWORD[40+r9]  ; disp->ContextRecord
	mov	rsi,r8  ; context
	mov	ecx,154  ; sizeof(CONTEXT)
	DD	0xa548f3fc  ; cld; rep movsq

	mov	rsi,r9
	xor	rcx,rcx  ; arg1, UNW_FLAG_NHANDLER
	mov	rdx,QWORD[8+rsi]  ; arg2, disp->ImageBase
	mov	r8,QWORD[rsi]  ; arg3, disp->ControlPc
	mov	r9,QWORD[16+rsi]  ; arg4, disp->FunctionEntry
	mov	r10,QWORD[40+rsi]  ; disp->ContextRecord
	lea	r11,[56+rsi]  ; &disp->HandlerData
	lea	r12,[24+rsi]  ; &disp->EstablisherFrame
	mov	QWORD[32+rsp],r10  ; arg5
	mov	QWORD[40+rsp],r11  ; arg6
	mov	QWORD[48+rsp],r12  ; arg7
	mov	QWORD[56+rsp],rcx  ; arg8, (NULL)
	call	QWORD[__imp_RtlVirtualUnwind]

	mov	eax,1  ; ExceptionContinueSearch
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	ret


section	.pdata rdata align=4
ALIGN	4
	DD	$L$SEH_begin_bn_mul_mont_gather5_nohw wrt ..imagebase
	DD	$L$SEH_end_bn_mul_mont_gather5_nohw wrt ..imagebase
	DD	$L$SEH_info_bn_mul_mont_gather5_nohw wrt ..imagebase

	DD	$L$SEH_begin_bn_mul4x_mont_gather5 wrt ..imagebase
	DD	$L$SEH_end_bn_mul4x_mont_gather5 wrt ..imagebase
	DD	$L$SEH_info_bn_mul4x_mont_gather5 wrt ..imagebase

	DD	$L$SEH_begin_bn_power5_nohw wrt ..imagebase
	DD	$L$SEH_end_bn_power5_nohw wrt ..imagebase
	DD	$L$SEH_info_bn_power5_nohw wrt ..imagebase
	DD	$L$SEH_begin_bn_mulx4x_mont_gather5 wrt ..imagebase
	DD	$L$SEH_end_bn_mulx4x_mont_gather5 wrt ..imagebase
	DD	$L$SEH_info_bn_mulx4x_mont_gather5 wrt ..imagebase

	DD	$L$SEH_begin_bn_powerx5 wrt ..imagebase
	DD	$L$SEH_end_bn_powerx5 wrt ..imagebase
	DD	$L$SEH_info_bn_powerx5 wrt ..imagebase
	DD	$L$SEH_begin_bn_gather5 wrt ..imagebase
	DD	$L$SEH_end_bn_gather5 wrt ..imagebase
	DD	$L$SEH_info_bn_gather5 wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_bn_mul_mont_gather5_nohw:
	DB	9,0,0,0
	DD	mul_handler wrt ..imagebase
	DD	$L$mul_body wrt ..imagebase,$L$mul_body wrt ..imagebase,$L$mul_epilogue wrt ..imagebase  ; HandlerData[]
ALIGN	8
$L$SEH_info_bn_mul4x_mont_gather5:
	DB	9,0,0,0
	DD	mul_handler wrt ..imagebase
	DD	$L$mul4x_prologue wrt ..imagebase,$L$mul4x_body wrt ..imagebase,$L$mul4x_epilogue wrt ..imagebase  ; HandlerData[]
ALIGN	8
$L$SEH_info_bn_power5_nohw:
	DB	9,0,0,0
	DD	mul_handler wrt ..imagebase
	DD	$L$power5_prologue wrt ..imagebase,$L$power5_body wrt ..imagebase,$L$power5_epilogue wrt ..imagebase  ; HandlerData[]
ALIGN	8
$L$SEH_info_bn_mulx4x_mont_gather5:
	DB	9,0,0,0
	DD	mul_handler wrt ..imagebase
	DD	$L$mulx4x_prologue wrt ..imagebase,$L$mulx4x_body wrt ..imagebase,$L$mulx4x_epilogue wrt ..imagebase  ; HandlerData[]
ALIGN	8
$L$SEH_info_bn_powerx5:
	DB	9,0,0,0
	DD	mul_handler wrt ..imagebase
	DD	$L$powerx5_prologue wrt ..imagebase,$L$powerx5_body wrt ..imagebase,$L$powerx5_epilogue wrt ..imagebase  ; HandlerData[]
ALIGN	8
$L$SEH_info_bn_gather5:
	DB	0x01,0x0b,0x03,0x0a
	DB	0x0b,0x01,0x21,0x00  ; sub	rsp,0x108
	DB	0x04,0xa3,0x00,0x00  ; lea	r10,(rsp)
ALIGN	8
%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
