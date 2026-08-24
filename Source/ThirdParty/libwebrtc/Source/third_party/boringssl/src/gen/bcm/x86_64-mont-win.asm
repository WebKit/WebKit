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


global	bn_mul_mont_nohw

ALIGN	16
bn_mul_mont_nohw:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul_mont_nohw:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	mov	r9d,r9d
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15


	neg	r9
	mov	r11,rsp
	lea	r10,[((-16))+r9*8+rsp]  ; future alloca(8*(num+2))
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

ALIGN	16
$L$mul_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r11,QWORD[rsp]
	cmp	rsp,r10
	ja	NEAR $L$mul_page_walk
$L$mul_page_walk_done:

	mov	QWORD[8+r9*8+rsp],rax  ; tp[num+1]=%rsp

$L$mul_body:
	mov	r12,rdx  ; reassign %rdx
	mov	r8,QWORD[r8]  ; pull n0[0] value
	mov	rbx,QWORD[r12]  ; m0=bp[0]
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
	jne	NEAR $L$1st

	add	r13,rax
	mov	rax,QWORD[rsi]  ; ap[0]
	adc	rdx,0
	add	r13,r11  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],r13  ; tp[j-1]
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
	mov	rbx,QWORD[r14*8+r12]  ; m0=bp[i]
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
	jne	NEAR $L$inner

	add	r13,rax
	mov	rax,QWORD[rsi]  ; ap[0]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[i]+tp[j]
	mov	r10,QWORD[r15*8+rsp]
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],r13  ; tp[j-1]
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
	mov	r15,r9  ; j=num

ALIGN	16
$L$sub:	sbb	rax,QWORD[r14*8+rcx]
	mov	QWORD[r14*8+rdi],rax  ; rp[i]=tp[i]-np[i]
	mov	rax,QWORD[8+r14*8+rsp]  ; tp[i+1]
	lea	r14,[1+r14]  ; i++
	dec	r15  ; doesn't affect CF!
	jnz	NEAR $L$sub

	sbb	rax,0  ; handle upmost overflow bit
	mov	rbx,-1
	xor	rbx,rax  ; not %rax
	xor	r14,r14
	mov	r15,r9  ; j=num

$L$copy:  ; conditional copy
	mov	rcx,QWORD[r14*8+rdi]
	mov	rdx,QWORD[r14*8+rsp]
	and	rcx,rbx
	and	rdx,rax
	mov	QWORD[r14*8+rsp],r9  ; zap temporary vector
	or	rdx,rcx
	mov	QWORD[r14*8+rdi],rdx  ; rp[i]=tp[i]
	lea	r14,[1+r14]
	sub	r15,1
	jnz	NEAR $L$copy

	mov	rsi,QWORD[8+r9*8+rsp]  ; restore %rsp

; No return value
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

$L$SEH_end_bn_mul_mont_nohw:
global	bn_mul4x_mont

ALIGN	16
bn_mul4x_mont:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul4x_mont:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	mov	r9d,r9d
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15


	neg	r9
	mov	r11,rsp
	lea	r10,[((-32))+r9*8+rsp]  ; future alloca(8*(num+4))
	neg	r9  ; restore
	and	r10,-1024  ; minimize TLB usage

	sub	r11,r10
	and	r11,-4096
	lea	rsp,[r11*1+r10]
	mov	r11,QWORD[rsp]
	cmp	rsp,r10
	ja	NEAR $L$mul4x_page_walk
	jmp	NEAR $L$mul4x_page_walk_done

$L$mul4x_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r11,QWORD[rsp]
	cmp	rsp,r10
	ja	NEAR $L$mul4x_page_walk
$L$mul4x_page_walk_done:

	mov	QWORD[8+r9*8+rsp],rax  ; tp[num+1]=%rsp

$L$mul4x_body:
	mov	QWORD[16+r9*8+rsp],rdi  ; tp[num+2]=%rdi
	mov	r12,rdx  ; reassign %r12
	mov	r8,QWORD[r8]  ; pull n0[0] value
	mov	rbx,QWORD[r12]  ; m0=bp[0]
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
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD[8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD[16+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	r15,[4+r15]  ; j++
	adc	rdx,0
	mov	QWORD[rsp],rdi
	mov	r13,rdx
	jmp	NEAR $L$1st4x
ALIGN	16
$L$1st4x:
	mul	rbx  ; ap[j]*bp[0]
	add	r10,rax
	mov	rax,QWORD[((-16))+r15*8+rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-24))+r15*8+rsp],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r11,rax
	mov	rax,QWORD[((-8))+r15*8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[r15*8+rsi]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],rdi  ; tp[j-1]
	mov	r13,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r10,rax
	mov	rax,QWORD[r15*8+rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[8+r15*8+rsi]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-8))+r15*8+rsp],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r11,rax
	mov	rax,QWORD[8+r15*8+rcx]
	adc	rdx,0
	lea	r15,[4+r15]  ; j++
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[((-16))+r15*8+rsi]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-32))+r15*8+rsp],rdi  ; tp[j-1]
	mov	r13,rdx
	cmp	r15,r9
	jb	NEAR $L$1st4x

	mul	rbx  ; ap[j]*bp[0]
	add	r10,rax
	mov	rax,QWORD[((-16))+r15*8+rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-24))+r15*8+rsp],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[0]
	add	r11,rax
	mov	rax,QWORD[((-8))+r15*8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[rsi]  ; ap[0]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[0]
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],rdi  ; tp[j-1]
	mov	r13,rdx

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	mov	QWORD[((-8))+r15*8+rsp],r13
	mov	QWORD[r15*8+rsp],rdi  ; store upmost overflow bit

	lea	r14,[1+r14]  ; i++
ALIGN	4
$L$outer4x:
	mov	rbx,QWORD[r14*8+r12]  ; m0=bp[i]
	xor	r15,r15  ; j=0
	mov	r10,QWORD[rsp]
	mov	rbp,r8
	mul	rbx  ; ap[0]*bp[i]
	add	r10,rax  ; ap[0]*bp[i]+tp[0]
	mov	rax,QWORD[rcx]
	adc	rdx,0

	imul	rbp,r10  ; tp[0]*n0
	mov	r11,rdx

	mul	rbp  ; np[0]*m1
	add	r10,rax  ; "%r13", discarded
	mov	rax,QWORD[8+rsi]
	adc	rdx,0
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[8+rcx]
	adc	rdx,0
	add	r11,QWORD[8+rsp]  ; +tp[1]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[16+rsi]
	adc	rdx,0
	add	rdi,r11  ; np[j]*m1+ap[j]*bp[i]+tp[j]
	lea	r15,[4+r15]  ; j+=2
	adc	rdx,0
	mov	QWORD[rsp],rdi  ; tp[j-1]
	mov	r13,rdx
	jmp	NEAR $L$inner4x
ALIGN	16
$L$inner4x:
	mul	rbx  ; ap[j]*bp[i]
	add	r10,rax
	mov	rax,QWORD[((-16))+r15*8+rcx]
	adc	rdx,0
	add	r10,QWORD[((-16))+r15*8+rsp]  ; ap[j]*bp[i]+tp[j]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD[((-24))+r15*8+rsp],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[((-8))+r15*8+rcx]
	adc	rdx,0
	add	r11,QWORD[((-8))+r15*8+rsp]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[r15*8+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],rdi  ; tp[j-1]
	mov	r13,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r10,rax
	mov	rax,QWORD[r15*8+rcx]
	adc	rdx,0
	add	r10,QWORD[r15*8+rsp]  ; ap[j]*bp[i]+tp[j]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[8+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD[((-8))+r15*8+rsp],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[8+r15*8+rcx]
	adc	rdx,0
	add	r11,QWORD[8+r15*8+rsp]
	adc	rdx,0
	lea	r15,[4+r15]  ; j++
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[((-16))+r15*8+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD[((-32))+r15*8+rsp],rdi  ; tp[j-1]
	mov	r13,rdx
	cmp	r15,r9
	jb	NEAR $L$inner4x

	mul	rbx  ; ap[j]*bp[i]
	add	r10,rax
	mov	rax,QWORD[((-16))+r15*8+rcx]
	adc	rdx,0
	add	r10,QWORD[((-16))+r15*8+rsp]  ; ap[j]*bp[i]+tp[j]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp  ; np[j]*m1
	add	r13,rax
	mov	rax,QWORD[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD[((-24))+r15*8+rsp],r13  ; tp[j-1]
	mov	rdi,rdx

	mul	rbx  ; ap[j]*bp[i]
	add	r11,rax
	mov	rax,QWORD[((-8))+r15*8+rcx]
	adc	rdx,0
	add	r11,QWORD[((-8))+r15*8+rsp]
	adc	rdx,0
	lea	r14,[1+r14]  ; i++
	mov	r10,rdx

	mul	rbp  ; np[j]*m1
	add	rdi,rax
	mov	rax,QWORD[rsi]  ; ap[0]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD[((-16))+r15*8+rsp],rdi  ; tp[j-1]
	mov	r13,rdx

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	add	r13,QWORD[r9*8+rsp]  ; pull upmost overflow bit
	adc	rdi,0
	mov	QWORD[((-8))+r15*8+rsp],r13
	mov	QWORD[r15*8+rsp],rdi  ; store upmost overflow bit

	cmp	r14,r9
	jb	NEAR $L$outer4x
	mov	rdi,QWORD[16+r9*8+rsp]  ; restore %rdi
	lea	r15,[((-4))+r9]
	mov	rax,QWORD[rsp]  ; tp[0]
	mov	rdx,QWORD[8+rsp]  ; tp[1]
	shr	r15,2  ; j=num/4-1
	lea	rsi,[rsp]  ; borrow ap for tp
	xor	r14,r14  ; i=0 and clear CF!

	sub	rax,QWORD[rcx]
	mov	rbx,QWORD[16+rsi]  ; tp[2]
	mov	rbp,QWORD[24+rsi]  ; tp[3]
	sbb	rdx,QWORD[8+rcx]

$L$sub4x:
	mov	QWORD[r14*8+rdi],rax  ; rp[i]=tp[i]-np[i]
	mov	QWORD[8+r14*8+rdi],rdx  ; rp[i]=tp[i]-np[i]
	sbb	rbx,QWORD[16+r14*8+rcx]
	mov	rax,QWORD[32+r14*8+rsi]  ; tp[i+1]
	mov	rdx,QWORD[40+r14*8+rsi]
	sbb	rbp,QWORD[24+r14*8+rcx]
	mov	QWORD[16+r14*8+rdi],rbx  ; rp[i]=tp[i]-np[i]
	mov	QWORD[24+r14*8+rdi],rbp  ; rp[i]=tp[i]-np[i]
	sbb	rax,QWORD[32+r14*8+rcx]
	mov	rbx,QWORD[48+r14*8+rsi]
	mov	rbp,QWORD[56+r14*8+rsi]
	sbb	rdx,QWORD[40+r14*8+rcx]
	lea	r14,[4+r14]  ; i++
	dec	r15  ; doesn't affect CF!
	jnz	NEAR $L$sub4x

	mov	QWORD[r14*8+rdi],rax  ; rp[i]=tp[i]-np[i]
	mov	rax,QWORD[32+r14*8+rsi]  ; load overflow bit
	sbb	rbx,QWORD[16+r14*8+rcx]
	mov	QWORD[8+r14*8+rdi],rdx  ; rp[i]=tp[i]-np[i]
	sbb	rbp,QWORD[24+r14*8+rcx]
	mov	QWORD[16+r14*8+rdi],rbx  ; rp[i]=tp[i]-np[i]

	sbb	rax,0  ; handle upmost overflow bit
	mov	QWORD[24+r14*8+rdi],rbp  ; rp[i]=tp[i]-np[i]
	pxor	xmm0,xmm0
	movq	xmm4,rax
	pcmpeqd	xmm5,xmm5
	pshufd	xmm4,xmm4,0
	mov	r15,r9
	pxor	xmm5,xmm4
	shr	r15,2  ; j=num/4
	xor	eax,eax  ; i=0

	jmp	NEAR $L$copy4x
ALIGN	16
$L$copy4x:  ; conditional copy
	movdqa	xmm1,XMMWORD[rax*1+rsp]
	movdqu	xmm2,XMMWORD[rax*1+rdi]
	pand	xmm1,xmm4
	pand	xmm2,xmm5
	movdqa	xmm3,XMMWORD[16+rax*1+rsp]
	movdqa	XMMWORD[rax*1+rsp],xmm0
	por	xmm1,xmm2
	movdqu	xmm2,XMMWORD[16+rax*1+rdi]
	movdqu	XMMWORD[rax*1+rdi],xmm1
	pand	xmm3,xmm4
	pand	xmm2,xmm5
	movdqa	XMMWORD[16+rax*1+rsp],xmm0
	por	xmm3,xmm2
	movdqu	XMMWORD[16+rax*1+rdi],xmm3
	lea	rax,[32+rax]
	dec	r15
	jnz	NEAR $L$copy4x
	mov	rsi,QWORD[8+r9*8+rsp]  ; restore %rsp

; No return value
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

$L$SEH_end_bn_mul4x_mont:
EXTERN	bn_sqrx8x_internal  ; see x86_64-mont5 module
EXTERN	bn_sqr8x_internal  ; see x86_64-mont5 module

global	bn_sqr8x_mont

ALIGN	32
bn_sqr8x_mont:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_sqr8x_mont:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	mov	r9d,r9d
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$sqr8x_prologue:

	mov	r10d,r9d
	shl	r9d,3  ; convert %r9 to bytes
	shl	r10,3+2  ; 4*%r9
	neg	r9

; #############################################################
; ensure that stack frame doesn't alias with %rsi modulo
; 4096. this is done to allow memory disambiguation logic
; do its job.
; 
	lea	r11,[((-64))+r9*2+rsp]
	mov	rbp,rsp
	mov	r8,QWORD[r8]  ; *n0
	sub	r11,rsi
	and	r11,4095
	cmp	r10,r11
	jb	NEAR $L$sqr8x_sp_alt
	sub	rbp,r11  ; align with %rsi
	lea	rbp,[((-64))+r9*2+rbp]  ; future alloca(frame+2*%r9)
	jmp	NEAR $L$sqr8x_sp_done

ALIGN	32
$L$sqr8x_sp_alt:
	lea	r10,[((4096-64))+r9*2]  ; 4096-frame-2*%r9
	lea	rbp,[((-64))+r9*2+rbp]  ; future alloca(frame+2*%r9)
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rbp,r11
$L$sqr8x_sp_done:
	and	rbp,-64
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,[rbp*1+r11]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$sqr8x_page_walk
	jmp	NEAR $L$sqr8x_page_walk_done

ALIGN	16
$L$sqr8x_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$sqr8x_page_walk
$L$sqr8x_page_walk_done:

	mov	r10,r9
	neg	r9

	mov	QWORD[32+rsp],r8
	mov	QWORD[40+rsp],rax  ; save original %rsp

$L$sqr8x_body:

	movq	xmm2,rcx  ; save pointer to modulus
	pxor	xmm0,xmm0
	movq	xmm1,rdi  ; save %rdi
	movq	xmm3,r10  ; -%r9
	test	rdx,rdx
	jz	NEAR $L$sqr8x_nox

	call	bn_sqrx8x_internal  ; see x86_64-mont5 module
; %rax	top-most carry
; %rbp	nptr
; %rcx	-8*num
; %r8	end of tp[2*num]
	lea	rbx,[rcx*1+r8]
	mov	r9,rcx
	mov	rdx,rcx
	movq	rdi,xmm1
	sar	rcx,3+2  ; %cf=0
	jmp	NEAR $L$sqr8x_sub

ALIGN	32
$L$sqr8x_nox:
	call	bn_sqr8x_internal  ; see x86_64-mont5 module
; %rax	top-most carry
; %rbp	nptr
; %r8	-8*num
; %rdi	end of tp[2*num]
	lea	rbx,[r9*1+rdi]
	mov	rcx,r9
	mov	rdx,r9
	movq	rdi,xmm1
	sar	rcx,3+2  ; %cf=0
	jmp	NEAR $L$sqr8x_sub

ALIGN	32
$L$sqr8x_sub:
	mov	r12,QWORD[rbx]
	mov	r13,QWORD[8+rbx]
	mov	r14,QWORD[16+rbx]
	mov	r15,QWORD[24+rbx]
	lea	rbx,[32+rbx]
	sbb	r12,QWORD[rbp]
	sbb	r13,QWORD[8+rbp]
	sbb	r14,QWORD[16+rbp]
	sbb	r15,QWORD[24+rbp]
	lea	rbp,[32+rbp]
	mov	QWORD[rdi],r12
	mov	QWORD[8+rdi],r13
	mov	QWORD[16+rdi],r14
	mov	QWORD[24+rdi],r15
	lea	rdi,[32+rdi]
	inc	rcx  ; preserves %cf
	jnz	NEAR $L$sqr8x_sub

	sbb	rax,0  ; top-most carry
	lea	rbx,[r9*1+rbx]  ; rewind
	lea	rdi,[r9*1+rdi]  ; rewind

	movq	xmm1,rax
	pxor	xmm0,xmm0
	pshufd	xmm1,xmm1,0
	mov	rsi,QWORD[40+rsp]  ; restore %rsp

	jmp	NEAR $L$sqr8x_cond_copy

ALIGN	32
$L$sqr8x_cond_copy:
	movdqa	xmm2,XMMWORD[rbx]
	movdqa	xmm3,XMMWORD[16+rbx]
	lea	rbx,[32+rbx]
	movdqu	xmm4,XMMWORD[rdi]
	movdqu	xmm5,XMMWORD[16+rdi]
	lea	rdi,[32+rdi]
	movdqa	XMMWORD[(-32)+rbx],xmm0  ; zero tp
	movdqa	XMMWORD[(-16)+rbx],xmm0
	movdqa	XMMWORD[(-32)+rdx*1+rbx],xmm0
	movdqa	XMMWORD[(-16)+rdx*1+rbx],xmm0
	pcmpeqd	xmm0,xmm1
	pand	xmm2,xmm1
	pand	xmm3,xmm1
	pand	xmm4,xmm0
	pand	xmm5,xmm0
	pxor	xmm0,xmm0
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqu	XMMWORD[(-32)+rdi],xmm4
	movdqu	XMMWORD[(-16)+rdi],xmm5
	add	r9,32
	jnz	NEAR $L$sqr8x_cond_copy

; No return value
	mov	r15,QWORD[((-48))+rsi]

	mov	r14,QWORD[((-40))+rsi]

	mov	r13,QWORD[((-32))+rsi]

	mov	r12,QWORD[((-24))+rsi]

	mov	rbp,QWORD[((-16))+rsi]

	mov	rbx,QWORD[((-8))+rsi]

	lea	rsp,[rsi]

$L$sqr8x_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_bn_sqr8x_mont:
global	bn_mulx4x_mont

ALIGN	32
bn_mulx4x_mont:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mulx4x_mont:
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

	shl	r9d,3  ; convert %r9 to bytes
	xor	r10,r10
	sub	r10,r9  ; -%r9
	mov	r8,QWORD[r8]  ; *n0
	lea	rbp,[((-72))+r10*1+rsp]  ; future alloca(frame+%r9+8)
	and	rbp,-128
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,[rbp*1+r11]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$mulx4x_page_walk
	jmp	NEAR $L$mulx4x_page_walk_done

ALIGN	16
$L$mulx4x_page_walk:
	lea	rsp,[((-4096))+rsp]
	mov	r10,QWORD[rsp]
	cmp	rsp,rbp
	ja	NEAR $L$mulx4x_page_walk
$L$mulx4x_page_walk_done:

	lea	r10,[r9*1+rdx]
; #############################################################
; Stack layout
; +0	num
; +8	off-loaded &b[i]
; +16	end of b[num]
; +24	saved n0
; +32	saved rp
; +40	saved %rsp
; +48	inner counter
; +56
; +64	tmp[num+1]
; 
	mov	QWORD[rsp],r9  ; save %r9
	shr	r9,5
	mov	QWORD[16+rsp],r10  ; end of b[num]
	sub	r9,1
	mov	QWORD[24+rsp],r8  ; save *n0
	mov	QWORD[32+rsp],rdi  ; save %rdi
	mov	QWORD[40+rsp],rax  ; save original %rsp

	mov	QWORD[48+rsp],r9  ; inner counter
	jmp	NEAR $L$mulx4x_body

ALIGN	32
$L$mulx4x_body:
	lea	rdi,[8+rdx]
	mov	rdx,QWORD[rdx]  ; b[0], %rdx==%rdx actually
	lea	rbx,[((64+32))+rsp]
	mov	r9,rdx

	mulx	rax,r8,QWORD[rsi]  ; a[0]*b[0]
	mulx	r14,r11,QWORD[8+rsi]  ; a[1]*b[0]
	add	r11,rax
	mov	QWORD[8+rsp],rdi  ; off-load &b[i]
	mulx	r13,r12,QWORD[16+rsi]  ; ...
	adc	r12,r14
	adc	r13,0

	mov	rdi,r8  ; borrow %rdi
	imul	r8,QWORD[24+rsp]  ; "t[0]"*n0
	xor	rbp,rbp  ; cf=0, of=0

	mulx	r14,rax,QWORD[24+rsi]
	mov	rdx,r8
	lea	rsi,[32+rsi]
	adcx	r13,rax
	adcx	r14,rbp  ; cf=0

	mulx	r10,rax,QWORD[rcx]
	adcx	rdi,rax  ; discarded
	adox	r10,r11
	mulx	r11,rax,QWORD[8+rcx]
	adcx	r10,rax
	adox	r11,r12
	DB	0xc4,0x62,0xfb,0xf6,0xa1,0x10,0x00,0x00,0x00  ; mulx	2*8(%rcx),%rax,%r12
	mov	rdi,QWORD[48+rsp]  ; counter value
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

	mov	rax,QWORD[rsp]  ; load num
	mov	rdi,QWORD[8+rsp]  ; re-load &b[i]
	adc	r15,rbp  ; modulo-scheduled
	add	r14,r15
	sbb	r15,r15  ; top-most carry
	mov	QWORD[((-8))+rbx],r14
	jmp	NEAR $L$mulx4x_outer

ALIGN	32
$L$mulx4x_outer:
	mov	rdx,QWORD[rdi]  ; b[i]
	lea	rdi,[8+rdi]  ; b++
	sub	rsi,rax  ; rewind %rsi
	mov	QWORD[rbx],r15  ; save top-most carry
	lea	rbx,[((64+32))+rsp]
	sub	rcx,rax  ; rewind %rcx

	mulx	r11,r8,QWORD[rsi]  ; a[0]*b[i]
	xor	ebp,ebp  ; xor	%rbp,%rbp	# cf=0, of=0
	mov	r9,rdx
	mulx	r12,r14,QWORD[8+rsi]  ; a[1]*b[i]
	adox	r8,QWORD[((-32))+rbx]
	adcx	r11,r14
	mulx	r13,r15,QWORD[16+rsi]  ; ...
	adox	r11,QWORD[((-24))+rbx]
	adcx	r12,r15
	adox	r12,QWORD[((-16))+rbx]
	adcx	r13,rbp
	adox	r13,rbp

	mov	QWORD[8+rsp],rdi  ; off-load &b[i]
	mov	r15,r8
	imul	r8,QWORD[24+rsp]  ; "t[0]"*n0
	xor	ebp,ebp  ; xor	%rbp,%rbp	# cf=0, of=0

	mulx	r14,rax,QWORD[24+rsi]
	mov	rdx,r8
	adcx	r13,rax
	adox	r13,QWORD[((-8))+rbx]
	adcx	r14,rbp
	lea	rsi,[32+rsi]
	adox	r14,rbp

	mulx	r10,rax,QWORD[rcx]
	adcx	r15,rax  ; discarded
	adox	r10,r11
	mulx	r11,rax,QWORD[8+rcx]
	adcx	r10,rax
	adox	r11,r12
	mulx	r12,rax,QWORD[16+rcx]
	mov	QWORD[((-32))+rbx],r10
	adcx	r11,rax
	adox	r12,r13
	mulx	r15,rax,QWORD[24+rcx]
	mov	rdx,r9
	mov	QWORD[((-24))+rbx],r11
	lea	rcx,[32+rcx]
	adcx	r12,rax
	adox	r15,rbp  ; of=0
	mov	rdi,QWORD[48+rsp]  ; counter value
	mov	QWORD[((-16))+rbx],r12

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
	mulx	r15,rax,QWORD[24+rcx]
	mov	rdx,r9
	mov	QWORD[((-32))+rbx],r11
	mov	QWORD[((-24))+rbx],r12
	adcx	r13,rax
	adox	r15,rbp
	lea	rcx,[32+rcx]
	mov	QWORD[((-16))+rbx],r13

	dec	rdi  ; of=0, pass cf
	jnz	NEAR $L$mulx4x_inner

	mov	rax,QWORD[rsp]  ; load num
	mov	rdi,QWORD[8+rsp]  ; re-load &b[i]
	adc	r15,rbp  ; modulo-scheduled
	sub	rbp,QWORD[rbx]  ; pull top-most carry
	adc	r14,r15
	sbb	r15,r15  ; top-most carry
	mov	QWORD[((-8))+rbx],r14

	cmp	rdi,QWORD[16+rsp]
	jne	NEAR $L$mulx4x_outer

	lea	rbx,[64+rsp]
	sub	rcx,rax  ; rewind %rcx
	neg	r15
	mov	rdx,rax
	shr	rax,3+2  ; %cf=0
	mov	rdi,QWORD[32+rsp]  ; restore rp
	jmp	NEAR $L$mulx4x_sub

ALIGN	32
$L$mulx4x_sub:
	mov	r11,QWORD[rbx]
	mov	r12,QWORD[8+rbx]
	mov	r13,QWORD[16+rbx]
	mov	r14,QWORD[24+rbx]
	lea	rbx,[32+rbx]
	sbb	r11,QWORD[rcx]
	sbb	r12,QWORD[8+rcx]
	sbb	r13,QWORD[16+rcx]
	sbb	r14,QWORD[24+rcx]
	lea	rcx,[32+rcx]
	mov	QWORD[rdi],r11
	mov	QWORD[8+rdi],r12
	mov	QWORD[16+rdi],r13
	mov	QWORD[24+rdi],r14
	lea	rdi,[32+rdi]
	dec	rax  ; preserves %cf
	jnz	NEAR $L$mulx4x_sub

	sbb	r15,0  ; top-most carry
	lea	rbx,[64+rsp]
	sub	rdi,rdx  ; rewind

	movq	xmm1,r15
	pxor	xmm0,xmm0
	pshufd	xmm1,xmm1,0
	mov	rsi,QWORD[40+rsp]  ; restore %rsp

	jmp	NEAR $L$mulx4x_cond_copy

ALIGN	32
$L$mulx4x_cond_copy:
	movdqa	xmm2,XMMWORD[rbx]
	movdqa	xmm3,XMMWORD[16+rbx]
	lea	rbx,[32+rbx]
	movdqu	xmm4,XMMWORD[rdi]
	movdqu	xmm5,XMMWORD[16+rdi]
	lea	rdi,[32+rdi]
	movdqa	XMMWORD[(-32)+rbx],xmm0  ; zero tp
	movdqa	XMMWORD[(-16)+rbx],xmm0
	pcmpeqd	xmm0,xmm1
	pand	xmm2,xmm1
	pand	xmm3,xmm1
	pand	xmm4,xmm0
	pand	xmm5,xmm0
	pxor	xmm0,xmm0
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqu	XMMWORD[(-32)+rdi],xmm4
	movdqu	XMMWORD[(-16)+rdi],xmm5
	sub	rdx,32
	jnz	NEAR $L$mulx4x_cond_copy

	mov	QWORD[rbx],rdx

; No return value
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

$L$SEH_end_bn_mulx4x_mont:
	DB	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105
	DB	112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56
	DB	54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83
	DB	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
	DB	115,108,46,111,114,103,62,0
ALIGN	16
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

	mov	rax,QWORD[152+r8]  ; pull context->Rsp

	mov	r10d,DWORD[4+r11]  ; HandlerData[1]
	lea	r10,[r10*1+rsi]  ; epilogue label
	cmp	rbx,r10  ; context->Rip>=epilogue label
	jae	NEAR $L$common_seh_tail

	mov	r10,QWORD[192+r8]  ; pull %r9
	mov	rax,QWORD[8+r10*8+rax]  ; pull saved stack pointer

	jmp	NEAR $L$common_pop_regs



ALIGN	16
sqr_handler:
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
	cmp	rbx,r10  ; context->Rip<.Lsqr_prologue
	jb	NEAR $L$common_seh_tail

	mov	r10d,DWORD[4+r11]  ; HandlerData[1]
	lea	r10,[r10*1+rsi]  ; body label
	cmp	rbx,r10  ; context->Rip<.Lsqr_body
	jb	NEAR $L$common_pop_regs

	mov	rax,QWORD[152+r8]  ; pull context->Rsp

	mov	r10d,DWORD[8+r11]  ; HandlerData[2]
	lea	r10,[r10*1+rsi]  ; epilogue label
	cmp	rbx,r10  ; context->Rip>=.Lsqr_epilogue
	jae	NEAR $L$common_seh_tail

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
	DD	$L$SEH_begin_bn_mul_mont_nohw wrt ..imagebase
	DD	$L$SEH_end_bn_mul_mont_nohw wrt ..imagebase
	DD	$L$SEH_info_bn_mul_mont_nohw wrt ..imagebase

	DD	$L$SEH_begin_bn_mul4x_mont wrt ..imagebase
	DD	$L$SEH_end_bn_mul4x_mont wrt ..imagebase
	DD	$L$SEH_info_bn_mul4x_mont wrt ..imagebase

	DD	$L$SEH_begin_bn_sqr8x_mont wrt ..imagebase
	DD	$L$SEH_end_bn_sqr8x_mont wrt ..imagebase
	DD	$L$SEH_info_bn_sqr8x_mont wrt ..imagebase
	DD	$L$SEH_begin_bn_mulx4x_mont wrt ..imagebase
	DD	$L$SEH_end_bn_mulx4x_mont wrt ..imagebase
	DD	$L$SEH_info_bn_mulx4x_mont wrt ..imagebase
section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_bn_mul_mont_nohw:
	DB	9,0,0,0
	DD	mul_handler wrt ..imagebase
	DD	$L$mul_body wrt ..imagebase,$L$mul_epilogue wrt ..imagebase  ; HandlerData[]
$L$SEH_info_bn_mul4x_mont:
	DB	9,0,0,0
	DD	mul_handler wrt ..imagebase
	DD	$L$mul4x_body wrt ..imagebase,$L$mul4x_epilogue wrt ..imagebase  ; HandlerData[]
$L$SEH_info_bn_sqr8x_mont:
	DB	9,0,0,0
	DD	sqr_handler wrt ..imagebase
	DD	$L$sqr8x_prologue wrt ..imagebase,$L$sqr8x_body wrt ..imagebase,$L$sqr8x_epilogue wrt ..imagebase  ; HandlerData[]
ALIGN	8
$L$SEH_info_bn_mulx4x_mont:
	DB	9,0,0,0
	DD	sqr_handler wrt ..imagebase
	DD	$L$mulx4x_prologue wrt ..imagebase,$L$mulx4x_body wrt ..imagebase,$L$mulx4x_epilogue wrt ..imagebase  ; HandlerData[]
ALIGN	8
%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
