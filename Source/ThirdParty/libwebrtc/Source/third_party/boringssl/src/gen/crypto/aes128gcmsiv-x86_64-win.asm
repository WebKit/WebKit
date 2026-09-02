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
section	.rdata rdata align=8

ALIGN	16
one:
	DQ	1,0
two:
	DQ	2,0
three:
	DQ	3,0
four:
	DQ	4,0
five:
	DQ	5,0
six:
	DQ	6,0
seven:
	DQ	7,0
eight:
	DQ	8,0

OR_MASK:
	DD	0x00000000,0x00000000,0x00000000,0x80000000
poly:
	DQ	0x1,0xc200000000000000
mask:
	DD	0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d
con1:
	DD	1,1,1,1
con2:
	DD	0x1b,0x1b,0x1b,0x1b
con3:
	DB	-1,-1,-1,-1,-1,-1,-1,-1,4,5,6,7,4,5,6,7
and_mask:
	DD	0,0xffffffff,0xffffffff,0xffffffff
section	.text code align=64


ALIGN	16
GFMUL:

	vpclmulqdq	xmm2,xmm0,xmm1,0x00
	vpclmulqdq	xmm5,xmm0,xmm1,0x11
	vpclmulqdq	xmm3,xmm0,xmm1,0x10
	vpclmulqdq	xmm4,xmm0,xmm1,0x01
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm3,8
	vpsrldq	xmm3,xmm3,8
	vpxor	xmm2,xmm2,xmm4
	vpxor	xmm5,xmm5,xmm3

	vpclmulqdq	xmm3,xmm2,XMMWORD[poly],0x10
	vpshufd	xmm4,xmm2,78
	vpxor	xmm2,xmm3,xmm4

	vpclmulqdq	xmm3,xmm2,XMMWORD[poly],0x10
	vpshufd	xmm4,xmm2,78
	vpxor	xmm2,xmm3,xmm4

	vpxor	xmm0,xmm2,xmm5
	ret


global	aesgcmsiv_htable_init

ALIGN	16
aesgcmsiv_htable_init:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesgcmsiv_htable_init:
	mov	rdi,rcx
	mov	rsi,rdx



_CET_ENDBR
	vmovdqa	xmm0,XMMWORD[rsi]
	vmovdqa	xmm1,xmm0
	vmovdqa	XMMWORD[rdi],xmm0  ; H
	call	GFMUL
	vmovdqa	XMMWORD[16+rdi],xmm0  ; H^2
	call	GFMUL
	vmovdqa	XMMWORD[32+rdi],xmm0  ; H^3
	call	GFMUL
	vmovdqa	XMMWORD[48+rdi],xmm0  ; H^4
	call	GFMUL
	vmovdqa	XMMWORD[64+rdi],xmm0  ; H^5
	call	GFMUL
	vmovdqa	XMMWORD[80+rdi],xmm0  ; H^6
	call	GFMUL
	vmovdqa	XMMWORD[96+rdi],xmm0  ; H^7
	call	GFMUL
	vmovdqa	XMMWORD[112+rdi],xmm0  ; H^8
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aesgcmsiv_htable_init:
global	aesgcmsiv_htable6_init

ALIGN	16
aesgcmsiv_htable6_init:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesgcmsiv_htable6_init:
	mov	rdi,rcx
	mov	rsi,rdx



_CET_ENDBR
	vmovdqa	xmm0,XMMWORD[rsi]
	vmovdqa	xmm1,xmm0
	vmovdqa	XMMWORD[rdi],xmm0  ; H
	call	GFMUL
	vmovdqa	XMMWORD[16+rdi],xmm0  ; H^2
	call	GFMUL
	vmovdqa	XMMWORD[32+rdi],xmm0  ; H^3
	call	GFMUL
	vmovdqa	XMMWORD[48+rdi],xmm0  ; H^4
	call	GFMUL
	vmovdqa	XMMWORD[64+rdi],xmm0  ; H^5
	call	GFMUL
	vmovdqa	XMMWORD[80+rdi],xmm0  ; H^6
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aesgcmsiv_htable6_init:
global	aesgcmsiv_htable_polyval

ALIGN	16
aesgcmsiv_htable_polyval:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesgcmsiv_htable_polyval:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9



_CET_ENDBR
	test	rdx,rdx
	jnz	NEAR $L$htable_polyval_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$htable_polyval_start:
	vzeroall

; We hash 8 blocks each iteration. If the total number of blocks is not a
; multiple of 8, we first hash the leading n%8 blocks.
	mov	r11,rdx
	and	r11,127

	jz	NEAR $L$htable_polyval_no_prefix

	vpxor	xmm9,xmm9,xmm9
	vmovdqa	xmm1,XMMWORD[rcx]
	sub	rdx,r11

	sub	r11,16

; hash first prefix block
	vmovdqu	xmm0,XMMWORD[rsi]
	vpxor	xmm0,xmm0,xmm1

	vpclmulqdq	xmm5,xmm0,XMMWORD[r11*1+rdi],0x01
	vpclmulqdq	xmm3,xmm0,XMMWORD[r11*1+rdi],0x00
	vpclmulqdq	xmm4,xmm0,XMMWORD[r11*1+rdi],0x11
	vpclmulqdq	xmm6,xmm0,XMMWORD[r11*1+rdi],0x10
	vpxor	xmm5,xmm5,xmm6

	lea	rsi,[16+rsi]
	test	r11,r11
	jnz	NEAR $L$htable_polyval_prefix_loop
	jmp	NEAR $L$htable_polyval_prefix_complete

; hash remaining prefix blocks (up to 7 total prefix blocks)
ALIGN	64
$L$htable_polyval_prefix_loop:
	sub	r11,16

	vmovdqu	xmm0,XMMWORD[rsi]  ; next data block

	vpclmulqdq	xmm6,xmm0,XMMWORD[r11*1+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[r11*1+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[r11*1+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[r11*1+rdi],0x10
	vpxor	xmm5,xmm5,xmm6

	test	r11,r11

	lea	rsi,[16+rsi]

	jnz	NEAR $L$htable_polyval_prefix_loop

$L$htable_polyval_prefix_complete:
	vpsrldq	xmm6,xmm5,8
	vpslldq	xmm5,xmm5,8

	vpxor	xmm9,xmm4,xmm6
	vpxor	xmm1,xmm3,xmm5

	jmp	NEAR $L$htable_polyval_main_loop

$L$htable_polyval_no_prefix:
; At this point we know the number of blocks is a multiple of 8. However,
; the reduction in the main loop includes a multiplication by x^(-128). In
; order to counter this, the existing tag needs to be multiplied by x^128.
; In practice, this just means that it is loaded into %xmm9, not %xmm1.
	vpxor	xmm1,xmm1,xmm1
	vmovdqa	xmm9,XMMWORD[rcx]

ALIGN	64
$L$htable_polyval_main_loop:
	sub	rdx,0x80
	jb	NEAR $L$htable_polyval_out

	vmovdqu	xmm0,XMMWORD[112+rsi]  ; Ii

	vpclmulqdq	xmm5,xmm0,XMMWORD[rdi],0x01
	vpclmulqdq	xmm3,xmm0,XMMWORD[rdi],0x00
	vpclmulqdq	xmm4,xmm0,XMMWORD[rdi],0x11
	vpclmulqdq	xmm6,xmm0,XMMWORD[rdi],0x10
	vpxor	xmm5,xmm5,xmm6

; ########################################################
	vmovdqu	xmm0,XMMWORD[96+rsi]
	vpclmulqdq	xmm6,xmm0,XMMWORD[16+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[16+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[16+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[16+rdi],0x10
	vpxor	xmm5,xmm5,xmm6


; ########################################################
	vmovdqu	xmm0,XMMWORD[80+rsi]

	vpclmulqdq	xmm7,xmm1,XMMWORD[poly],0x10  ; reduction stage 1a
	vpalignr	xmm1,xmm1,xmm1,8

	vpclmulqdq	xmm6,xmm0,XMMWORD[32+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[32+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[32+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[32+rdi],0x10
	vpxor	xmm5,xmm5,xmm6


	vpxor	xmm1,xmm1,xmm7  ; reduction stage 1b
; ########################################################
	vmovdqu	xmm0,XMMWORD[64+rsi]

	vpclmulqdq	xmm6,xmm0,XMMWORD[48+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[48+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[48+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[48+rdi],0x10
	vpxor	xmm5,xmm5,xmm6

; ########################################################
	vmovdqu	xmm0,XMMWORD[48+rsi]

	vpclmulqdq	xmm7,xmm1,XMMWORD[poly],0x10  ; reduction stage 2a
	vpalignr	xmm1,xmm1,xmm1,8

	vpclmulqdq	xmm6,xmm0,XMMWORD[64+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[64+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[64+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[64+rdi],0x10
	vpxor	xmm5,xmm5,xmm6


	vpxor	xmm1,xmm1,xmm7  ; reduction stage 2b
; ########################################################
	vmovdqu	xmm0,XMMWORD[32+rsi]

	vpclmulqdq	xmm6,xmm0,XMMWORD[80+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[80+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[80+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[80+rdi],0x10
	vpxor	xmm5,xmm5,xmm6


	vpxor	xmm1,xmm1,xmm9  ; reduction finalize
; ########################################################
	vmovdqu	xmm0,XMMWORD[16+rsi]

	vpclmulqdq	xmm6,xmm0,XMMWORD[96+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[96+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[96+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[96+rdi],0x10
	vpxor	xmm5,xmm5,xmm6

; ########################################################
	vmovdqu	xmm0,XMMWORD[rsi]
	vpxor	xmm0,xmm0,xmm1

	vpclmulqdq	xmm6,xmm0,XMMWORD[112+rdi],0x01
	vpxor	xmm5,xmm5,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[112+rdi],0x00
	vpxor	xmm3,xmm3,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[112+rdi],0x11
	vpxor	xmm4,xmm4,xmm6
	vpclmulqdq	xmm6,xmm0,XMMWORD[112+rdi],0x10
	vpxor	xmm5,xmm5,xmm6

; ########################################################
	vpsrldq	xmm6,xmm5,8
	vpslldq	xmm5,xmm5,8

	vpxor	xmm9,xmm4,xmm6
	vpxor	xmm1,xmm3,xmm5

	lea	rsi,[128+rsi]
	jmp	NEAR $L$htable_polyval_main_loop

; ########################################################

$L$htable_polyval_out:
	vpclmulqdq	xmm6,xmm1,XMMWORD[poly],0x10
	vpalignr	xmm1,xmm1,xmm1,8
	vpxor	xmm1,xmm1,xmm6

	vpclmulqdq	xmm6,xmm1,XMMWORD[poly],0x10
	vpalignr	xmm1,xmm1,xmm1,8
	vpxor	xmm1,xmm1,xmm6
	vpxor	xmm1,xmm1,xmm9

	vmovdqu	XMMWORD[rcx],xmm1
	vzeroupper
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aesgcmsiv_htable_polyval:
global	aesgcmsiv_polyval_horner

ALIGN	16
aesgcmsiv_polyval_horner:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesgcmsiv_polyval_horner:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9



_CET_ENDBR
	test	rcx,rcx
	jnz	NEAR $L$polyval_horner_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$polyval_horner_start:
; We will start with L GFMULS for POLYVAL(BIG_BUFFER)
; RES = GFMUL(RES, H)

	xor	r10,r10
	shl	rcx,4  ; L contains number of bytes to process

	vmovdqa	xmm1,XMMWORD[rsi]
	vmovdqa	xmm0,XMMWORD[rdi]

$L$polyval_horner_loop:
	vpxor	xmm0,xmm0,XMMWORD[r10*1+rdx]  ; RES = RES + Xi
	call	GFMUL  ; RES = RES * H

	add	r10,16
	cmp	rcx,r10
	jne	NEAR $L$polyval_horner_loop

; calculation of T is complete. RES=T
	vmovdqa	XMMWORD[rdi],xmm0
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aesgcmsiv_polyval_horner:
global	aes128gcmsiv_aes_ks

ALIGN	16
aes128gcmsiv_aes_ks:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes128gcmsiv_aes_ks:
	mov	rdi,rcx
	mov	rsi,rdx



_CET_ENDBR
	vmovdqu	xmm1,XMMWORD[rdi]  ; xmm1 = user key
	vmovdqa	XMMWORD[rsi],xmm1  ; rsi points to output

	vmovdqa	xmm0,XMMWORD[con1]
	vmovdqa	xmm15,XMMWORD[mask]

	mov	rax,8

$L$ks128_loop:
	add	rsi,16  ; rsi points for next key
	sub	rax,1
	vpshufb	xmm2,xmm1,xmm15  ; xmm2 = shuffled user key
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm3,xmm1,4
	vpxor	xmm1,xmm1,xmm3
	vpslldq	xmm3,xmm3,4
	vpxor	xmm1,xmm1,xmm3
	vpslldq	xmm3,xmm3,4
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2
	vmovdqa	XMMWORD[rsi],xmm1
	jne	NEAR $L$ks128_loop

	vmovdqa	xmm0,XMMWORD[con2]
	vpshufb	xmm2,xmm1,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm3,xmm1,4
	vpxor	xmm1,xmm1,xmm3
	vpslldq	xmm3,xmm3,4
	vpxor	xmm1,xmm1,xmm3
	vpslldq	xmm3,xmm3,4
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2
	vmovdqa	XMMWORD[16+rsi],xmm1

	vpshufb	xmm2,xmm1,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslldq	xmm3,xmm1,4
	vpxor	xmm1,xmm1,xmm3
	vpslldq	xmm3,xmm3,4
	vpxor	xmm1,xmm1,xmm3
	vpslldq	xmm3,xmm3,4
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2
	vmovdqa	XMMWORD[32+rsi],xmm1
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes128gcmsiv_aes_ks:
global	aes256gcmsiv_aes_ks

ALIGN	16
aes256gcmsiv_aes_ks:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes256gcmsiv_aes_ks:
	mov	rdi,rcx
	mov	rsi,rdx



_CET_ENDBR
	vmovdqu	xmm1,XMMWORD[rdi]
	vmovdqu	xmm3,XMMWORD[16+rdi]
	vmovdqa	XMMWORD[rsi],xmm1
	vmovdqa	XMMWORD[16+rsi],xmm3
	vmovdqa	xmm0,XMMWORD[con1]
	vmovdqa	xmm15,XMMWORD[mask]
	vpxor	xmm14,xmm14,xmm14
	mov	rax,6

$L$ks256_loop:
	add	rsi,32
	sub	rax,1
	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm4,xmm1,32
	vpxor	xmm1,xmm1,xmm4
	vpshufb	xmm4,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vmovdqa	XMMWORD[rsi],xmm1
	vpshufd	xmm2,xmm1,0xff
	vaesenclast	xmm2,xmm2,xmm14
	vpsllq	xmm4,xmm3,32
	vpxor	xmm3,xmm3,xmm4
	vpshufb	xmm4,xmm3,XMMWORD[con3]
	vpxor	xmm3,xmm3,xmm4
	vpxor	xmm3,xmm3,xmm2
	vmovdqa	XMMWORD[16+rsi],xmm3
	jne	NEAR $L$ks256_loop

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpsllq	xmm4,xmm1,32
	vpxor	xmm1,xmm1,xmm4
	vpshufb	xmm4,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vmovdqa	XMMWORD[32+rsi],xmm1
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

global	aes128gcmsiv_aes_ks_enc_x1

ALIGN	16
aes128gcmsiv_aes_ks_enc_x1:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes128gcmsiv_aes_ks_enc_x1:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9



_CET_ENDBR
	vmovdqa	xmm1,XMMWORD[rcx]  ; xmm1 = first 16 bytes of random key
	vmovdqa	xmm4,XMMWORD[rdi]

	vmovdqa	XMMWORD[rdx],xmm1  ; KEY[0] = first 16 bytes of random key
	vpxor	xmm4,xmm4,xmm1

	vmovdqa	xmm0,XMMWORD[con1]  ; xmm0  = 1,1,1,1
	vmovdqa	xmm15,XMMWORD[mask]  ; xmm15 = mask

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[16+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[32+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[48+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[64+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[80+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[96+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[112+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[128+rdx],xmm1


	vmovdqa	xmm0,XMMWORD[con2]

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenc	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[144+rdx],xmm1

	vpshufb	xmm2,xmm1,xmm15  ; !!saving mov instruction to xmm2
	vaesenclast	xmm2,xmm2,xmm0
	vpsllq	xmm3,xmm1,32  ; !!saving mov instruction to xmm3
	vpxor	xmm1,xmm1,xmm3
	vpshufb	xmm3,xmm1,XMMWORD[con3]
	vpxor	xmm1,xmm1,xmm3
	vpxor	xmm1,xmm1,xmm2

	vaesenclast	xmm4,xmm4,xmm1
	vmovdqa	XMMWORD[160+rdx],xmm1


	vmovdqa	XMMWORD[rsi],xmm4
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes128gcmsiv_aes_ks_enc_x1:
global	aes128gcmsiv_kdf

ALIGN	16
aes128gcmsiv_kdf:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes128gcmsiv_kdf:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
; parameter 1: %rdi                         Pointer to NONCE
; parameter 2: %rsi                         Pointer to CT
; parameter 4: %rdx                         Pointer to keys

	vmovdqa	xmm1,XMMWORD[rdx]  ; xmm1 = first 16 bytes of random key
	vmovdqa	xmm9,XMMWORD[rdi]
	vmovdqa	xmm12,XMMWORD[and_mask]
	vmovdqa	xmm13,XMMWORD[one]
	vpshufd	xmm9,xmm9,0x90
	vpand	xmm9,xmm9,xmm12
	vpaddd	xmm10,xmm9,xmm13
	vpaddd	xmm11,xmm10,xmm13
	vpaddd	xmm12,xmm11,xmm13

	vpxor	xmm9,xmm9,xmm1
	vpxor	xmm10,xmm10,xmm1
	vpxor	xmm11,xmm11,xmm1
	vpxor	xmm12,xmm12,xmm1

	vmovdqa	xmm1,XMMWORD[16+rdx]
	vaesenc	xmm9,xmm9,xmm1
	vaesenc	xmm10,xmm10,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1

	vmovdqa	xmm2,XMMWORD[32+rdx]
	vaesenc	xmm9,xmm9,xmm2
	vaesenc	xmm10,xmm10,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2

	vmovdqa	xmm1,XMMWORD[48+rdx]
	vaesenc	xmm9,xmm9,xmm1
	vaesenc	xmm10,xmm10,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1

	vmovdqa	xmm2,XMMWORD[64+rdx]
	vaesenc	xmm9,xmm9,xmm2
	vaesenc	xmm10,xmm10,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2

	vmovdqa	xmm1,XMMWORD[80+rdx]
	vaesenc	xmm9,xmm9,xmm1
	vaesenc	xmm10,xmm10,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1

	vmovdqa	xmm2,XMMWORD[96+rdx]
	vaesenc	xmm9,xmm9,xmm2
	vaesenc	xmm10,xmm10,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2

	vmovdqa	xmm1,XMMWORD[112+rdx]
	vaesenc	xmm9,xmm9,xmm1
	vaesenc	xmm10,xmm10,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1

	vmovdqa	xmm2,XMMWORD[128+rdx]
	vaesenc	xmm9,xmm9,xmm2
	vaesenc	xmm10,xmm10,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2

	vmovdqa	xmm1,XMMWORD[144+rdx]
	vaesenc	xmm9,xmm9,xmm1
	vaesenc	xmm10,xmm10,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1

	vmovdqa	xmm2,XMMWORD[160+rdx]
	vaesenclast	xmm9,xmm9,xmm2
	vaesenclast	xmm10,xmm10,xmm2
	vaesenclast	xmm11,xmm11,xmm2
	vaesenclast	xmm12,xmm12,xmm2


	vmovdqa	XMMWORD[rsi],xmm9
	vmovdqa	XMMWORD[16+rsi],xmm10
	vmovdqa	XMMWORD[32+rsi],xmm11
	vmovdqa	XMMWORD[48+rsi],xmm12
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes128gcmsiv_kdf:
global	aes128gcmsiv_enc_msg_x4

ALIGN	16
aes128gcmsiv_enc_msg_x4:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes128gcmsiv_enc_msg_x4:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]



_CET_ENDBR
	test	r8,r8
	jnz	NEAR $L$128_enc_msg_x4_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$128_enc_msg_x4_start:
	push	r12

	push	r13


	shr	r8,4  ; LEN = num of blocks
	mov	r10,r8
	shl	r10,62
	shr	r10,62

; make IV from TAG
	vmovdqa	xmm15,XMMWORD[rdx]
	vpor	xmm15,xmm15,XMMWORD[OR_MASK]  ; IV = [1]TAG[126...32][00..00]

	vmovdqu	xmm4,XMMWORD[four]  ; Register to increment counters
	vmovdqa	xmm0,xmm15  ; CTR1 = TAG[1][127...32][00..00]
	vpaddd	xmm1,xmm15,XMMWORD[one]  ; CTR2 = TAG[1][127...32][00..01]
	vpaddd	xmm2,xmm15,XMMWORD[two]  ; CTR3 = TAG[1][127...32][00..02]
	vpaddd	xmm3,xmm15,XMMWORD[three]  ; CTR4 = TAG[1][127...32][00..03]

	shr	r8,2
	je	NEAR $L$128_enc_msg_x4_check_remainder

	sub	rsi,64
	sub	rdi,64

$L$128_enc_msg_x4_loop1:
	add	rsi,64
	add	rdi,64

	vmovdqa	xmm5,xmm0
	vmovdqa	xmm6,xmm1
	vmovdqa	xmm7,xmm2
	vmovdqa	xmm8,xmm3

	vpxor	xmm5,xmm5,XMMWORD[rcx]
	vpxor	xmm6,xmm6,XMMWORD[rcx]
	vpxor	xmm7,xmm7,XMMWORD[rcx]
	vpxor	xmm8,xmm8,XMMWORD[rcx]

	vmovdqu	xmm12,XMMWORD[16+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm0,xmm0,xmm4
	vmovdqu	xmm12,XMMWORD[32+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm1,xmm1,xmm4
	vmovdqu	xmm12,XMMWORD[48+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm2,xmm2,xmm4
	vmovdqu	xmm12,XMMWORD[64+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm3,xmm3,xmm4

	vmovdqu	xmm12,XMMWORD[80+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[96+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[112+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[128+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[144+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[160+rcx]
	vaesenclast	xmm5,xmm5,xmm12
	vaesenclast	xmm6,xmm6,xmm12
	vaesenclast	xmm7,xmm7,xmm12
	vaesenclast	xmm8,xmm8,xmm12


; XOR with Plaintext
	vpxor	xmm5,xmm5,XMMWORD[rdi]
	vpxor	xmm6,xmm6,XMMWORD[16+rdi]
	vpxor	xmm7,xmm7,XMMWORD[32+rdi]
	vpxor	xmm8,xmm8,XMMWORD[48+rdi]

	sub	r8,1

	vmovdqu	XMMWORD[rsi],xmm5
	vmovdqu	XMMWORD[16+rsi],xmm6
	vmovdqu	XMMWORD[32+rsi],xmm7
	vmovdqu	XMMWORD[48+rsi],xmm8

	jne	NEAR $L$128_enc_msg_x4_loop1

	add	rsi,64
	add	rdi,64

$L$128_enc_msg_x4_check_remainder:
	cmp	r10,0
	je	NEAR $L$128_enc_msg_x4_out

$L$128_enc_msg_x4_loop2:
; enc each block separately
; CTR1 is the highest counter (even if no LOOP done)
	vmovdqa	xmm5,xmm0
	vpaddd	xmm0,xmm0,XMMWORD[one]  ; inc counter

	vpxor	xmm5,xmm5,XMMWORD[rcx]
	vaesenc	xmm5,xmm5,XMMWORD[16+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[32+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[48+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[64+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[80+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[96+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[112+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[128+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[144+rcx]
	vaesenclast	xmm5,xmm5,XMMWORD[160+rcx]

; XOR with plaintext
	vpxor	xmm5,xmm5,XMMWORD[rdi]
	vmovdqu	XMMWORD[rsi],xmm5

	add	rdi,16
	add	rsi,16

	sub	r10,1
	jne	NEAR $L$128_enc_msg_x4_loop2

$L$128_enc_msg_x4_out:
	pop	r13

	pop	r12

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes128gcmsiv_enc_msg_x4:
global	aes128gcmsiv_enc_msg_x8

ALIGN	16
aes128gcmsiv_enc_msg_x8:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes128gcmsiv_enc_msg_x8:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]



_CET_ENDBR
	test	r8,r8
	jnz	NEAR $L$128_enc_msg_x8_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$128_enc_msg_x8_start:
	push	r12

	push	r13

	push	rbp

	mov	rbp,rsp


; Place in stack
	sub	rsp,128
	and	rsp,-64

	shr	r8,4  ; LEN = num of blocks
	mov	r10,r8
	shl	r10,61
	shr	r10,61

; make IV from TAG
	vmovdqu	xmm1,XMMWORD[rdx]
	vpor	xmm1,xmm1,XMMWORD[OR_MASK]  ; TMP1= IV = [1]TAG[126...32][00..00]

; store counter8 in the stack
	vpaddd	xmm0,xmm1,XMMWORD[seven]
	vmovdqu	XMMWORD[rsp],xmm0  ; CTR8 = TAG[127...32][00..07]
	vpaddd	xmm9,xmm1,XMMWORD[one]  ; CTR2 = TAG[127...32][00..01]
	vpaddd	xmm10,xmm1,XMMWORD[two]  ; CTR3 = TAG[127...32][00..02]
	vpaddd	xmm11,xmm1,XMMWORD[three]  ; CTR4 = TAG[127...32][00..03]
	vpaddd	xmm12,xmm1,XMMWORD[four]  ; CTR5 = TAG[127...32][00..04]
	vpaddd	xmm13,xmm1,XMMWORD[five]  ; CTR6 = TAG[127...32][00..05]
	vpaddd	xmm14,xmm1,XMMWORD[six]  ; CTR7 = TAG[127...32][00..06]
	vmovdqa	xmm0,xmm1  ; CTR1 = TAG[127...32][00..00]

	shr	r8,3
	je	NEAR $L$128_enc_msg_x8_check_remainder

	sub	rsi,128
	sub	rdi,128

$L$128_enc_msg_x8_loop1:
	add	rsi,128
	add	rdi,128

	vmovdqa	xmm1,xmm0
	vmovdqa	xmm2,xmm9
	vmovdqa	xmm3,xmm10
	vmovdqa	xmm4,xmm11
	vmovdqa	xmm5,xmm12
	vmovdqa	xmm6,xmm13
	vmovdqa	xmm7,xmm14
; move from stack
	vmovdqu	xmm8,XMMWORD[rsp]

	vpxor	xmm1,xmm1,XMMWORD[rcx]
	vpxor	xmm2,xmm2,XMMWORD[rcx]
	vpxor	xmm3,xmm3,XMMWORD[rcx]
	vpxor	xmm4,xmm4,XMMWORD[rcx]
	vpxor	xmm5,xmm5,XMMWORD[rcx]
	vpxor	xmm6,xmm6,XMMWORD[rcx]
	vpxor	xmm7,xmm7,XMMWORD[rcx]
	vpxor	xmm8,xmm8,XMMWORD[rcx]

	vmovdqu	xmm15,XMMWORD[16+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqu	xmm14,XMMWORD[rsp]  ; deal with CTR8
	vpaddd	xmm14,xmm14,XMMWORD[eight]
	vmovdqu	XMMWORD[rsp],xmm14
	vmovdqu	xmm15,XMMWORD[32+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpsubd	xmm14,xmm14,XMMWORD[one]
	vmovdqu	xmm15,XMMWORD[48+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm0,xmm0,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[64+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm9,xmm9,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[80+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm10,xmm10,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[96+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm11,xmm11,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[112+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm12,xmm12,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[128+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm13,xmm13,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[144+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqu	xmm15,XMMWORD[160+rcx]
	vaesenclast	xmm1,xmm1,xmm15
	vaesenclast	xmm2,xmm2,xmm15
	vaesenclast	xmm3,xmm3,xmm15
	vaesenclast	xmm4,xmm4,xmm15
	vaesenclast	xmm5,xmm5,xmm15
	vaesenclast	xmm6,xmm6,xmm15
	vaesenclast	xmm7,xmm7,xmm15
	vaesenclast	xmm8,xmm8,xmm15


; XOR with Plaintext
	vpxor	xmm1,xmm1,XMMWORD[rdi]
	vpxor	xmm2,xmm2,XMMWORD[16+rdi]
	vpxor	xmm3,xmm3,XMMWORD[32+rdi]
	vpxor	xmm4,xmm4,XMMWORD[48+rdi]
	vpxor	xmm5,xmm5,XMMWORD[64+rdi]
	vpxor	xmm6,xmm6,XMMWORD[80+rdi]
	vpxor	xmm7,xmm7,XMMWORD[96+rdi]
	vpxor	xmm8,xmm8,XMMWORD[112+rdi]

	dec	r8

	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	vmovdqu	XMMWORD[80+rsi],xmm6
	vmovdqu	XMMWORD[96+rsi],xmm7
	vmovdqu	XMMWORD[112+rsi],xmm8

	jne	NEAR $L$128_enc_msg_x8_loop1

	add	rsi,128
	add	rdi,128

$L$128_enc_msg_x8_check_remainder:
	cmp	r10,0
	je	NEAR $L$128_enc_msg_x8_out

$L$128_enc_msg_x8_loop2:
; enc each block separately
; CTR1 is the highest counter (even if no LOOP done)
	vmovdqa	xmm1,xmm0
	vpaddd	xmm0,xmm0,XMMWORD[one]  ; inc counter

	vpxor	xmm1,xmm1,XMMWORD[rcx]
	vaesenc	xmm1,xmm1,XMMWORD[16+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[32+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[48+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[64+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[80+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[96+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[112+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[128+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[144+rcx]
	vaesenclast	xmm1,xmm1,XMMWORD[160+rcx]

; XOR with Plaintext
	vpxor	xmm1,xmm1,XMMWORD[rdi]

	vmovdqu	XMMWORD[rsi],xmm1

	add	rdi,16
	add	rsi,16

	dec	r10
	jne	NEAR $L$128_enc_msg_x8_loop2

$L$128_enc_msg_x8_out:
	mov	rsp,rbp

	pop	rbp

	pop	r13

	pop	r12

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes128gcmsiv_enc_msg_x8:
global	aes128gcmsiv_dec

ALIGN	16
aes128gcmsiv_dec:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes128gcmsiv_dec:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	test	r9,~15
	jnz	NEAR $L$128_dec_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$128_dec_start:
	vzeroupper
	vmovdqa	xmm0,XMMWORD[rdx]
; The claimed tag is provided after the current calculated tag value.
; CTRBLKs is made from it.
	vmovdqu	xmm15,XMMWORD[16+rdx]
	vpor	xmm15,xmm15,XMMWORD[OR_MASK]  ; CTR = [1]TAG[126...32][00..00]
	mov	rax,rdx

	lea	rax,[32+rax]
	lea	rcx,[32+rcx]

	and	r9,~15

; If less then 6 blocks, make singles
	cmp	r9,96
	jb	NEAR $L$128_dec_loop2

; Decrypt the first six blocks
	sub	r9,96
	vmovdqa	xmm7,xmm15
	vpaddd	xmm8,xmm7,XMMWORD[one]
	vpaddd	xmm9,xmm7,XMMWORD[two]
	vpaddd	xmm10,xmm9,XMMWORD[one]
	vpaddd	xmm11,xmm9,XMMWORD[two]
	vpaddd	xmm12,xmm11,XMMWORD[one]
	vpaddd	xmm15,xmm11,XMMWORD[two]

	vpxor	xmm7,xmm7,XMMWORD[r8]
	vpxor	xmm8,xmm8,XMMWORD[r8]
	vpxor	xmm9,xmm9,XMMWORD[r8]
	vpxor	xmm10,xmm10,XMMWORD[r8]
	vpxor	xmm11,xmm11,XMMWORD[r8]
	vpxor	xmm12,xmm12,XMMWORD[r8]

	vmovdqu	xmm4,XMMWORD[16+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[32+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[48+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[64+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[80+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[96+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[112+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[128+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[144+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[160+r8]
	vaesenclast	xmm7,xmm7,xmm4
	vaesenclast	xmm8,xmm8,xmm4
	vaesenclast	xmm9,xmm9,xmm4
	vaesenclast	xmm10,xmm10,xmm4
	vaesenclast	xmm11,xmm11,xmm4
	vaesenclast	xmm12,xmm12,xmm4

; XOR with CT
	vpxor	xmm7,xmm7,XMMWORD[rdi]
	vpxor	xmm8,xmm8,XMMWORD[16+rdi]
	vpxor	xmm9,xmm9,XMMWORD[32+rdi]
	vpxor	xmm10,xmm10,XMMWORD[48+rdi]
	vpxor	xmm11,xmm11,XMMWORD[64+rdi]
	vpxor	xmm12,xmm12,XMMWORD[80+rdi]

	vmovdqu	XMMWORD[rsi],xmm7
	vmovdqu	XMMWORD[16+rsi],xmm8
	vmovdqu	XMMWORD[32+rsi],xmm9
	vmovdqu	XMMWORD[48+rsi],xmm10
	vmovdqu	XMMWORD[64+rsi],xmm11
	vmovdqu	XMMWORD[80+rsi],xmm12

	add	rdi,96
	add	rsi,96
	jmp	NEAR $L$128_dec_loop1

; Decrypt 6 blocks each time while hashing previous 6 blocks
ALIGN	64
$L$128_dec_loop1:
	cmp	r9,96
	jb	NEAR $L$128_dec_finish_96
	sub	r9,96

	vmovdqa	xmm6,xmm12
	vmovdqa	XMMWORD[(16-32)+rax],xmm11
	vmovdqa	XMMWORD[(32-32)+rax],xmm10
	vmovdqa	XMMWORD[(48-32)+rax],xmm9
	vmovdqa	XMMWORD[(64-32)+rax],xmm8
	vmovdqa	XMMWORD[(80-32)+rax],xmm7

	vmovdqa	xmm7,xmm15
	vpaddd	xmm8,xmm7,XMMWORD[one]
	vpaddd	xmm9,xmm7,XMMWORD[two]
	vpaddd	xmm10,xmm9,XMMWORD[one]
	vpaddd	xmm11,xmm9,XMMWORD[two]
	vpaddd	xmm12,xmm11,XMMWORD[one]
	vpaddd	xmm15,xmm11,XMMWORD[two]

	vmovdqa	xmm4,XMMWORD[r8]
	vpxor	xmm7,xmm7,xmm4
	vpxor	xmm8,xmm8,xmm4
	vpxor	xmm9,xmm9,xmm4
	vpxor	xmm10,xmm10,xmm4
	vpxor	xmm11,xmm11,xmm4
	vpxor	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[((0-32))+rcx]
	vpclmulqdq	xmm2,xmm6,xmm4,0x11
	vpclmulqdq	xmm3,xmm6,xmm4,0x00
	vpclmulqdq	xmm1,xmm6,xmm4,0x01
	vpclmulqdq	xmm4,xmm6,xmm4,0x10
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm4,XMMWORD[16+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[((-16))+rax]
	vmovdqu	xmm13,XMMWORD[((-16))+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[32+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[rax]
	vmovdqu	xmm13,XMMWORD[rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[48+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[16+rax]
	vmovdqu	xmm13,XMMWORD[16+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[64+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[32+rax]
	vmovdqu	xmm13,XMMWORD[32+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[80+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[96+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[112+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4


	vmovdqa	xmm6,XMMWORD[((80-32))+rax]
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm5,XMMWORD[((80-32))+rcx]

	vpclmulqdq	xmm4,xmm6,xmm5,0x01
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x10
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm4,XMMWORD[128+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4


	vpsrldq	xmm4,xmm1,8
	vpxor	xmm5,xmm2,xmm4
	vpslldq	xmm4,xmm1,8
	vpxor	xmm0,xmm3,xmm4

	vmovdqa	xmm3,XMMWORD[poly]

	vmovdqu	xmm4,XMMWORD[144+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[160+r8]
	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vpxor	xmm4,xmm6,XMMWORD[rdi]
	vaesenclast	xmm7,xmm7,xmm4
	vpxor	xmm4,xmm6,XMMWORD[16+rdi]
	vaesenclast	xmm8,xmm8,xmm4
	vpxor	xmm4,xmm6,XMMWORD[32+rdi]
	vaesenclast	xmm9,xmm9,xmm4
	vpxor	xmm4,xmm6,XMMWORD[48+rdi]
	vaesenclast	xmm10,xmm10,xmm4
	vpxor	xmm4,xmm6,XMMWORD[64+rdi]
	vaesenclast	xmm11,xmm11,xmm4
	vpxor	xmm4,xmm6,XMMWORD[80+rdi]
	vaesenclast	xmm12,xmm12,xmm4

	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vmovdqu	XMMWORD[rsi],xmm7
	vmovdqu	XMMWORD[16+rsi],xmm8
	vmovdqu	XMMWORD[32+rsi],xmm9
	vmovdqu	XMMWORD[48+rsi],xmm10
	vmovdqu	XMMWORD[64+rsi],xmm11
	vmovdqu	XMMWORD[80+rsi],xmm12

	vpxor	xmm0,xmm0,xmm5

	lea	rdi,[96+rdi]
	lea	rsi,[96+rsi]
	jmp	NEAR $L$128_dec_loop1

$L$128_dec_finish_96:
	vmovdqa	xmm6,xmm12
	vmovdqa	XMMWORD[(16-32)+rax],xmm11
	vmovdqa	XMMWORD[(32-32)+rax],xmm10
	vmovdqa	XMMWORD[(48-32)+rax],xmm9
	vmovdqa	XMMWORD[(64-32)+rax],xmm8
	vmovdqa	XMMWORD[(80-32)+rax],xmm7

	vmovdqu	xmm4,XMMWORD[((0-32))+rcx]
	vpclmulqdq	xmm1,xmm6,xmm4,0x10
	vpclmulqdq	xmm2,xmm6,xmm4,0x11
	vpclmulqdq	xmm3,xmm6,xmm4,0x00
	vpclmulqdq	xmm4,xmm6,xmm4,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[((-16))+rax]
	vmovdqu	xmm13,XMMWORD[((-16))+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[rax]
	vmovdqu	xmm13,XMMWORD[rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[16+rax]
	vmovdqu	xmm13,XMMWORD[16+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[32+rax]
	vmovdqu	xmm13,XMMWORD[32+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm6,XMMWORD[((80-32))+rax]
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm5,XMMWORD[((80-32))+rcx]
	vpclmulqdq	xmm4,xmm6,xmm5,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x01
	vpxor	xmm1,xmm1,xmm4

	vpsrldq	xmm4,xmm1,8
	vpxor	xmm5,xmm2,xmm4
	vpslldq	xmm4,xmm1,8
	vpxor	xmm0,xmm3,xmm4

	vmovdqa	xmm3,XMMWORD[poly]

	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vpxor	xmm0,xmm0,xmm5

$L$128_dec_loop2:
; Here we encrypt any remaining whole block

; if there are no whole blocks
	cmp	r9,16
	jb	NEAR $L$128_dec_out
	sub	r9,16

	vmovdqa	xmm2,xmm15
	vpaddd	xmm15,xmm15,XMMWORD[one]

	vpxor	xmm2,xmm2,XMMWORD[r8]
	vaesenc	xmm2,xmm2,XMMWORD[16+r8]
	vaesenc	xmm2,xmm2,XMMWORD[32+r8]
	vaesenc	xmm2,xmm2,XMMWORD[48+r8]
	vaesenc	xmm2,xmm2,XMMWORD[64+r8]
	vaesenc	xmm2,xmm2,XMMWORD[80+r8]
	vaesenc	xmm2,xmm2,XMMWORD[96+r8]
	vaesenc	xmm2,xmm2,XMMWORD[112+r8]
	vaesenc	xmm2,xmm2,XMMWORD[128+r8]
	vaesenc	xmm2,xmm2,XMMWORD[144+r8]
	vaesenclast	xmm2,xmm2,XMMWORD[160+r8]
	vpxor	xmm2,xmm2,XMMWORD[rdi]
	vmovdqu	XMMWORD[rsi],xmm2
	add	rdi,16
	add	rsi,16

	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm1,XMMWORD[((-32))+rcx]
	call	GFMUL

	jmp	NEAR $L$128_dec_loop2

$L$128_dec_out:
	vmovdqu	XMMWORD[rdx],xmm0
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes128gcmsiv_dec:
global	aes128gcmsiv_ecb_enc_block

ALIGN	16
aes128gcmsiv_ecb_enc_block:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes128gcmsiv_ecb_enc_block:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
	vmovdqa	xmm1,XMMWORD[rdi]

	vpxor	xmm1,xmm1,XMMWORD[rdx]
	vaesenc	xmm1,xmm1,XMMWORD[16+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[32+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[48+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[64+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[80+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[96+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[112+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[128+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[144+rdx]
	vaesenclast	xmm1,xmm1,XMMWORD[160+rdx]  ; STATE_1 == IV

	vmovdqa	XMMWORD[rsi],xmm1

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes128gcmsiv_ecb_enc_block:
global	aes256gcmsiv_aes_ks_enc_x1

ALIGN	16
aes256gcmsiv_aes_ks_enc_x1:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes256gcmsiv_aes_ks_enc_x1:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9



_CET_ENDBR
	vmovdqa	xmm0,XMMWORD[con1]  ; CON_MASK  = 1,1,1,1
	vmovdqa	xmm15,XMMWORD[mask]  ; MASK_256
	vmovdqa	xmm8,XMMWORD[rdi]
	vmovdqa	xmm1,XMMWORD[rcx]  ; KEY_1 || KEY_2 [0..7] = user key
	vmovdqa	xmm3,XMMWORD[16+rcx]
	vpxor	xmm8,xmm8,xmm1
	vaesenc	xmm8,xmm8,xmm3
	vmovdqu	XMMWORD[rdx],xmm1  ; First round key
	vmovdqu	XMMWORD[16+rdx],xmm3
	vpxor	xmm14,xmm14,xmm14

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm4,xmm1,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vaesenc	xmm8,xmm8,xmm1
	vmovdqu	XMMWORD[32+rdx],xmm1

	vpshufd	xmm2,xmm1,0xff
	vaesenclast	xmm2,xmm2,xmm14
	vpslldq	xmm4,xmm3,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpxor	xmm3,xmm3,xmm2
	vaesenc	xmm8,xmm8,xmm3
	vmovdqu	XMMWORD[48+rdx],xmm3

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm4,xmm1,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vaesenc	xmm8,xmm8,xmm1
	vmovdqu	XMMWORD[64+rdx],xmm1

	vpshufd	xmm2,xmm1,0xff
	vaesenclast	xmm2,xmm2,xmm14
	vpslldq	xmm4,xmm3,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpxor	xmm3,xmm3,xmm2
	vaesenc	xmm8,xmm8,xmm3
	vmovdqu	XMMWORD[80+rdx],xmm3

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm4,xmm1,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vaesenc	xmm8,xmm8,xmm1
	vmovdqu	XMMWORD[96+rdx],xmm1

	vpshufd	xmm2,xmm1,0xff
	vaesenclast	xmm2,xmm2,xmm14
	vpslldq	xmm4,xmm3,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpxor	xmm3,xmm3,xmm2
	vaesenc	xmm8,xmm8,xmm3
	vmovdqu	XMMWORD[112+rdx],xmm3

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm4,xmm1,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vaesenc	xmm8,xmm8,xmm1
	vmovdqu	XMMWORD[128+rdx],xmm1

	vpshufd	xmm2,xmm1,0xff
	vaesenclast	xmm2,xmm2,xmm14
	vpslldq	xmm4,xmm3,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpxor	xmm3,xmm3,xmm2
	vaesenc	xmm8,xmm8,xmm3
	vmovdqu	XMMWORD[144+rdx],xmm3

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm4,xmm1,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vaesenc	xmm8,xmm8,xmm1
	vmovdqu	XMMWORD[160+rdx],xmm1

	vpshufd	xmm2,xmm1,0xff
	vaesenclast	xmm2,xmm2,xmm14
	vpslldq	xmm4,xmm3,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpxor	xmm3,xmm3,xmm2
	vaesenc	xmm8,xmm8,xmm3
	vmovdqu	XMMWORD[176+rdx],xmm3

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslld	xmm0,xmm0,1
	vpslldq	xmm4,xmm1,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vaesenc	xmm8,xmm8,xmm1
	vmovdqu	XMMWORD[192+rdx],xmm1

	vpshufd	xmm2,xmm1,0xff
	vaesenclast	xmm2,xmm2,xmm14
	vpslldq	xmm4,xmm3,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm3,xmm3,xmm4
	vpxor	xmm3,xmm3,xmm2
	vaesenc	xmm8,xmm8,xmm3
	vmovdqu	XMMWORD[208+rdx],xmm3

	vpshufb	xmm2,xmm3,xmm15
	vaesenclast	xmm2,xmm2,xmm0
	vpslldq	xmm4,xmm1,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpslldq	xmm4,xmm4,4
	vpxor	xmm1,xmm1,xmm4
	vpxor	xmm1,xmm1,xmm2
	vaesenclast	xmm8,xmm8,xmm1
	vmovdqu	XMMWORD[224+rdx],xmm1

	vmovdqa	XMMWORD[rsi],xmm8
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes256gcmsiv_aes_ks_enc_x1:
global	aes256gcmsiv_ecb_enc_block

ALIGN	16
aes256gcmsiv_ecb_enc_block:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes256gcmsiv_ecb_enc_block:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
	vmovdqa	xmm1,XMMWORD[rdi]
	vpxor	xmm1,xmm1,XMMWORD[rdx]
	vaesenc	xmm1,xmm1,XMMWORD[16+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[32+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[48+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[64+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[80+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[96+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[112+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[128+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[144+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[160+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[176+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[192+rdx]
	vaesenc	xmm1,xmm1,XMMWORD[208+rdx]
	vaesenclast	xmm1,xmm1,XMMWORD[224+rdx]  ; %xmm1 == IV
	vmovdqa	XMMWORD[rsi],xmm1
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes256gcmsiv_ecb_enc_block:
global	aes256gcmsiv_enc_msg_x4

ALIGN	16
aes256gcmsiv_enc_msg_x4:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes256gcmsiv_enc_msg_x4:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]



_CET_ENDBR
	test	r8,r8
	jnz	NEAR $L$256_enc_msg_x4_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$256_enc_msg_x4_start:
	mov	r10,r8
	shr	r8,4  ; LEN = num of blocks
	shl	r10,60
	jz	NEAR $L$256_enc_msg_x4_start2
	add	r8,1

$L$256_enc_msg_x4_start2:
	mov	r10,r8
	shl	r10,62
	shr	r10,62

; make IV from TAG
	vmovdqa	xmm15,XMMWORD[rdx]
	vpor	xmm15,xmm15,XMMWORD[OR_MASK]  ; IV = [1]TAG[126...32][00..00]

	vmovdqa	xmm4,XMMWORD[four]  ; Register to increment counters
	vmovdqa	xmm0,xmm15  ; CTR1 = TAG[1][127...32][00..00]
	vpaddd	xmm1,xmm15,XMMWORD[one]  ; CTR2 = TAG[1][127...32][00..01]
	vpaddd	xmm2,xmm15,XMMWORD[two]  ; CTR3 = TAG[1][127...32][00..02]
	vpaddd	xmm3,xmm15,XMMWORD[three]  ; CTR4 = TAG[1][127...32][00..03]

	shr	r8,2
	je	NEAR $L$256_enc_msg_x4_check_remainder

	sub	rsi,64
	sub	rdi,64

$L$256_enc_msg_x4_loop1:
	add	rsi,64
	add	rdi,64

	vmovdqa	xmm5,xmm0
	vmovdqa	xmm6,xmm1
	vmovdqa	xmm7,xmm2
	vmovdqa	xmm8,xmm3

	vpxor	xmm5,xmm5,XMMWORD[rcx]
	vpxor	xmm6,xmm6,XMMWORD[rcx]
	vpxor	xmm7,xmm7,XMMWORD[rcx]
	vpxor	xmm8,xmm8,XMMWORD[rcx]

	vmovdqu	xmm12,XMMWORD[16+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm0,xmm0,xmm4
	vmovdqu	xmm12,XMMWORD[32+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm1,xmm1,xmm4
	vmovdqu	xmm12,XMMWORD[48+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm2,xmm2,xmm4
	vmovdqu	xmm12,XMMWORD[64+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vpaddd	xmm3,xmm3,xmm4

	vmovdqu	xmm12,XMMWORD[80+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[96+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[112+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[128+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[144+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[160+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[176+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[192+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[208+rcx]
	vaesenc	xmm5,xmm5,xmm12
	vaesenc	xmm6,xmm6,xmm12
	vaesenc	xmm7,xmm7,xmm12
	vaesenc	xmm8,xmm8,xmm12

	vmovdqu	xmm12,XMMWORD[224+rcx]
	vaesenclast	xmm5,xmm5,xmm12
	vaesenclast	xmm6,xmm6,xmm12
	vaesenclast	xmm7,xmm7,xmm12
	vaesenclast	xmm8,xmm8,xmm12


; XOR with Plaintext
	vpxor	xmm5,xmm5,XMMWORD[rdi]
	vpxor	xmm6,xmm6,XMMWORD[16+rdi]
	vpxor	xmm7,xmm7,XMMWORD[32+rdi]
	vpxor	xmm8,xmm8,XMMWORD[48+rdi]

	sub	r8,1

	vmovdqu	XMMWORD[rsi],xmm5
	vmovdqu	XMMWORD[16+rsi],xmm6
	vmovdqu	XMMWORD[32+rsi],xmm7
	vmovdqu	XMMWORD[48+rsi],xmm8

	jne	NEAR $L$256_enc_msg_x4_loop1

	add	rsi,64
	add	rdi,64

$L$256_enc_msg_x4_check_remainder:
	cmp	r10,0
	je	NEAR $L$256_enc_msg_x4_out

$L$256_enc_msg_x4_loop2:
; encrypt each block separately
; CTR1 is the highest counter (even if no LOOP done)

	vmovdqa	xmm5,xmm0
	vpaddd	xmm0,xmm0,XMMWORD[one]  ; inc counter
	vpxor	xmm5,xmm5,XMMWORD[rcx]
	vaesenc	xmm5,xmm5,XMMWORD[16+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[32+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[48+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[64+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[80+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[96+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[112+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[128+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[144+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[160+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[176+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[192+rcx]
	vaesenc	xmm5,xmm5,XMMWORD[208+rcx]
	vaesenclast	xmm5,xmm5,XMMWORD[224+rcx]

; XOR with Plaintext
	vpxor	xmm5,xmm5,XMMWORD[rdi]

	vmovdqu	XMMWORD[rsi],xmm5

	add	rdi,16
	add	rsi,16

	sub	r10,1
	jne	NEAR $L$256_enc_msg_x4_loop2

$L$256_enc_msg_x4_out:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes256gcmsiv_enc_msg_x4:
global	aes256gcmsiv_enc_msg_x8

ALIGN	16
aes256gcmsiv_enc_msg_x8:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes256gcmsiv_enc_msg_x8:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]



_CET_ENDBR
	test	r8,r8
	jnz	NEAR $L$256_enc_msg_x8_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$256_enc_msg_x8_start:
; Place in stack
	mov	r11,rsp
	sub	r11,16
	and	r11,-64

	mov	r10,r8
	shr	r8,4  ; LEN = num of blocks
	shl	r10,60
	jz	NEAR $L$256_enc_msg_x8_start2
	add	r8,1

$L$256_enc_msg_x8_start2:
	mov	r10,r8
	shl	r10,61
	shr	r10,61

; Make IV from TAG
	vmovdqa	xmm1,XMMWORD[rdx]
	vpor	xmm1,xmm1,XMMWORD[OR_MASK]  ; TMP1= IV = [1]TAG[126...32][00..00]

; store counter8 on the stack
	vpaddd	xmm0,xmm1,XMMWORD[seven]
	vmovdqa	XMMWORD[r11],xmm0  ; CTR8 = TAG[127...32][00..07]
	vpaddd	xmm9,xmm1,XMMWORD[one]  ; CTR2 = TAG[127...32][00..01]
	vpaddd	xmm10,xmm1,XMMWORD[two]  ; CTR3 = TAG[127...32][00..02]
	vpaddd	xmm11,xmm1,XMMWORD[three]  ; CTR4 = TAG[127...32][00..03]
	vpaddd	xmm12,xmm1,XMMWORD[four]  ; CTR5 = TAG[127...32][00..04]
	vpaddd	xmm13,xmm1,XMMWORD[five]  ; CTR6 = TAG[127...32][00..05]
	vpaddd	xmm14,xmm1,XMMWORD[six]  ; CTR7 = TAG[127...32][00..06]
	vmovdqa	xmm0,xmm1  ; CTR1 = TAG[127...32][00..00]

	shr	r8,3
	jz	NEAR $L$256_enc_msg_x8_check_remainder

	sub	rsi,128
	sub	rdi,128

$L$256_enc_msg_x8_loop1:
	add	rsi,128
	add	rdi,128

	vmovdqa	xmm1,xmm0
	vmovdqa	xmm2,xmm9
	vmovdqa	xmm3,xmm10
	vmovdqa	xmm4,xmm11
	vmovdqa	xmm5,xmm12
	vmovdqa	xmm6,xmm13
	vmovdqa	xmm7,xmm14
; move from stack
	vmovdqa	xmm8,XMMWORD[r11]

	vpxor	xmm1,xmm1,XMMWORD[rcx]
	vpxor	xmm2,xmm2,XMMWORD[rcx]
	vpxor	xmm3,xmm3,XMMWORD[rcx]
	vpxor	xmm4,xmm4,XMMWORD[rcx]
	vpxor	xmm5,xmm5,XMMWORD[rcx]
	vpxor	xmm6,xmm6,XMMWORD[rcx]
	vpxor	xmm7,xmm7,XMMWORD[rcx]
	vpxor	xmm8,xmm8,XMMWORD[rcx]

	vmovdqu	xmm15,XMMWORD[16+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqa	xmm14,XMMWORD[r11]  ; deal with CTR8
	vpaddd	xmm14,xmm14,XMMWORD[eight]
	vmovdqa	XMMWORD[r11],xmm14
	vmovdqu	xmm15,XMMWORD[32+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpsubd	xmm14,xmm14,XMMWORD[one]
	vmovdqu	xmm15,XMMWORD[48+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm0,xmm0,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[64+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm9,xmm9,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[80+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm10,xmm10,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[96+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm11,xmm11,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[112+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm12,xmm12,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[128+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vpaddd	xmm13,xmm13,XMMWORD[eight]
	vmovdqu	xmm15,XMMWORD[144+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqu	xmm15,XMMWORD[160+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqu	xmm15,XMMWORD[176+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqu	xmm15,XMMWORD[192+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqu	xmm15,XMMWORD[208+rcx]
	vaesenc	xmm1,xmm1,xmm15
	vaesenc	xmm2,xmm2,xmm15
	vaesenc	xmm3,xmm3,xmm15
	vaesenc	xmm4,xmm4,xmm15
	vaesenc	xmm5,xmm5,xmm15
	vaesenc	xmm6,xmm6,xmm15
	vaesenc	xmm7,xmm7,xmm15
	vaesenc	xmm8,xmm8,xmm15

	vmovdqu	xmm15,XMMWORD[224+rcx]
	vaesenclast	xmm1,xmm1,xmm15
	vaesenclast	xmm2,xmm2,xmm15
	vaesenclast	xmm3,xmm3,xmm15
	vaesenclast	xmm4,xmm4,xmm15
	vaesenclast	xmm5,xmm5,xmm15
	vaesenclast	xmm6,xmm6,xmm15
	vaesenclast	xmm7,xmm7,xmm15
	vaesenclast	xmm8,xmm8,xmm15


; XOR with Plaintext
	vpxor	xmm1,xmm1,XMMWORD[rdi]
	vpxor	xmm2,xmm2,XMMWORD[16+rdi]
	vpxor	xmm3,xmm3,XMMWORD[32+rdi]
	vpxor	xmm4,xmm4,XMMWORD[48+rdi]
	vpxor	xmm5,xmm5,XMMWORD[64+rdi]
	vpxor	xmm6,xmm6,XMMWORD[80+rdi]
	vpxor	xmm7,xmm7,XMMWORD[96+rdi]
	vpxor	xmm8,xmm8,XMMWORD[112+rdi]

	sub	r8,1

	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	vmovdqu	XMMWORD[80+rsi],xmm6
	vmovdqu	XMMWORD[96+rsi],xmm7
	vmovdqu	XMMWORD[112+rsi],xmm8

	jne	NEAR $L$256_enc_msg_x8_loop1

	add	rsi,128
	add	rdi,128

$L$256_enc_msg_x8_check_remainder:
	cmp	r10,0
	je	NEAR $L$256_enc_msg_x8_out

$L$256_enc_msg_x8_loop2:
; encrypt each block separately
; CTR1 is the highest counter (even if no LOOP done)
	vmovdqa	xmm1,xmm0
	vpaddd	xmm0,xmm0,XMMWORD[one]

	vpxor	xmm1,xmm1,XMMWORD[rcx]
	vaesenc	xmm1,xmm1,XMMWORD[16+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[32+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[48+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[64+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[80+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[96+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[112+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[128+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[144+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[160+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[176+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[192+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[208+rcx]
	vaesenclast	xmm1,xmm1,XMMWORD[224+rcx]

; XOR with Plaintext
	vpxor	xmm1,xmm1,XMMWORD[rdi]

	vmovdqu	XMMWORD[rsi],xmm1

	add	rdi,16
	add	rsi,16
	sub	r10,1
	jnz	NEAR $L$256_enc_msg_x8_loop2

$L$256_enc_msg_x8_out:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret


$L$SEH_end_aes256gcmsiv_enc_msg_x8:
global	aes256gcmsiv_dec

ALIGN	16
aes256gcmsiv_dec:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes256gcmsiv_dec:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	test	r9,~15
	jnz	NEAR $L$256_dec_start
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$256_dec_start:
	vzeroupper
	vmovdqa	xmm0,XMMWORD[rdx]
; The claimed tag is provided after the current calculated tag value.
; CTRBLKs is made from it.
	vmovdqu	xmm15,XMMWORD[16+rdx]
	vpor	xmm15,xmm15,XMMWORD[OR_MASK]  ; CTR = [1]TAG[126...32][00..00]
	mov	rax,rdx

	lea	rax,[32+rax]
	lea	rcx,[32+rcx]

	and	r9,~15

; If less then 6 blocks, make singles
	cmp	r9,96
	jb	NEAR $L$256_dec_loop2

; Decrypt the first six blocks
	sub	r9,96
	vmovdqa	xmm7,xmm15
	vpaddd	xmm8,xmm7,XMMWORD[one]
	vpaddd	xmm9,xmm7,XMMWORD[two]
	vpaddd	xmm10,xmm9,XMMWORD[one]
	vpaddd	xmm11,xmm9,XMMWORD[two]
	vpaddd	xmm12,xmm11,XMMWORD[one]
	vpaddd	xmm15,xmm11,XMMWORD[two]

	vpxor	xmm7,xmm7,XMMWORD[r8]
	vpxor	xmm8,xmm8,XMMWORD[r8]
	vpxor	xmm9,xmm9,XMMWORD[r8]
	vpxor	xmm10,xmm10,XMMWORD[r8]
	vpxor	xmm11,xmm11,XMMWORD[r8]
	vpxor	xmm12,xmm12,XMMWORD[r8]

	vmovdqu	xmm4,XMMWORD[16+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[32+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[48+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[64+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[80+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[96+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[112+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[128+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[144+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[160+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[176+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[192+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[208+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[224+r8]
	vaesenclast	xmm7,xmm7,xmm4
	vaesenclast	xmm8,xmm8,xmm4
	vaesenclast	xmm9,xmm9,xmm4
	vaesenclast	xmm10,xmm10,xmm4
	vaesenclast	xmm11,xmm11,xmm4
	vaesenclast	xmm12,xmm12,xmm4

; XOR with CT
	vpxor	xmm7,xmm7,XMMWORD[rdi]
	vpxor	xmm8,xmm8,XMMWORD[16+rdi]
	vpxor	xmm9,xmm9,XMMWORD[32+rdi]
	vpxor	xmm10,xmm10,XMMWORD[48+rdi]
	vpxor	xmm11,xmm11,XMMWORD[64+rdi]
	vpxor	xmm12,xmm12,XMMWORD[80+rdi]

	vmovdqu	XMMWORD[rsi],xmm7
	vmovdqu	XMMWORD[16+rsi],xmm8
	vmovdqu	XMMWORD[32+rsi],xmm9
	vmovdqu	XMMWORD[48+rsi],xmm10
	vmovdqu	XMMWORD[64+rsi],xmm11
	vmovdqu	XMMWORD[80+rsi],xmm12

	add	rdi,96
	add	rsi,96
	jmp	NEAR $L$256_dec_loop1

; Decrypt 6 blocks each time while hashing previous 6 blocks
ALIGN	64
$L$256_dec_loop1:
	cmp	r9,96
	jb	NEAR $L$256_dec_finish_96
	sub	r9,96

	vmovdqa	xmm6,xmm12
	vmovdqa	XMMWORD[(16-32)+rax],xmm11
	vmovdqa	XMMWORD[(32-32)+rax],xmm10
	vmovdqa	XMMWORD[(48-32)+rax],xmm9
	vmovdqa	XMMWORD[(64-32)+rax],xmm8
	vmovdqa	XMMWORD[(80-32)+rax],xmm7

	vmovdqa	xmm7,xmm15
	vpaddd	xmm8,xmm7,XMMWORD[one]
	vpaddd	xmm9,xmm7,XMMWORD[two]
	vpaddd	xmm10,xmm9,XMMWORD[one]
	vpaddd	xmm11,xmm9,XMMWORD[two]
	vpaddd	xmm12,xmm11,XMMWORD[one]
	vpaddd	xmm15,xmm11,XMMWORD[two]

	vmovdqa	xmm4,XMMWORD[r8]
	vpxor	xmm7,xmm7,xmm4
	vpxor	xmm8,xmm8,xmm4
	vpxor	xmm9,xmm9,xmm4
	vpxor	xmm10,xmm10,xmm4
	vpxor	xmm11,xmm11,xmm4
	vpxor	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[((0-32))+rcx]
	vpclmulqdq	xmm2,xmm6,xmm4,0x11
	vpclmulqdq	xmm3,xmm6,xmm4,0x00
	vpclmulqdq	xmm1,xmm6,xmm4,0x01
	vpclmulqdq	xmm4,xmm6,xmm4,0x10
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm4,XMMWORD[16+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[((-16))+rax]
	vmovdqu	xmm13,XMMWORD[((-16))+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[32+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[rax]
	vmovdqu	xmm13,XMMWORD[rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[48+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[16+rax]
	vmovdqu	xmm13,XMMWORD[16+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[64+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[32+rax]
	vmovdqu	xmm13,XMMWORD[32+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm4,XMMWORD[80+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[96+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[112+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4


	vmovdqa	xmm6,XMMWORD[((80-32))+rax]
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm5,XMMWORD[((80-32))+rcx]

	vpclmulqdq	xmm4,xmm6,xmm5,0x01
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x10
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm4,XMMWORD[128+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4


	vpsrldq	xmm4,xmm1,8
	vpxor	xmm5,xmm2,xmm4
	vpslldq	xmm4,xmm1,8
	vpxor	xmm0,xmm3,xmm4

	vmovdqa	xmm3,XMMWORD[poly]

	vmovdqu	xmm4,XMMWORD[144+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[160+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[176+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[192+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm4,XMMWORD[208+r8]
	vaesenc	xmm7,xmm7,xmm4
	vaesenc	xmm8,xmm8,xmm4
	vaesenc	xmm9,xmm9,xmm4
	vaesenc	xmm10,xmm10,xmm4
	vaesenc	xmm11,xmm11,xmm4
	vaesenc	xmm12,xmm12,xmm4

	vmovdqu	xmm6,XMMWORD[224+r8]
	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vpxor	xmm4,xmm6,XMMWORD[rdi]
	vaesenclast	xmm7,xmm7,xmm4
	vpxor	xmm4,xmm6,XMMWORD[16+rdi]
	vaesenclast	xmm8,xmm8,xmm4
	vpxor	xmm4,xmm6,XMMWORD[32+rdi]
	vaesenclast	xmm9,xmm9,xmm4
	vpxor	xmm4,xmm6,XMMWORD[48+rdi]
	vaesenclast	xmm10,xmm10,xmm4
	vpxor	xmm4,xmm6,XMMWORD[64+rdi]
	vaesenclast	xmm11,xmm11,xmm4
	vpxor	xmm4,xmm6,XMMWORD[80+rdi]
	vaesenclast	xmm12,xmm12,xmm4

	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vmovdqu	XMMWORD[rsi],xmm7
	vmovdqu	XMMWORD[16+rsi],xmm8
	vmovdqu	XMMWORD[32+rsi],xmm9
	vmovdqu	XMMWORD[48+rsi],xmm10
	vmovdqu	XMMWORD[64+rsi],xmm11
	vmovdqu	XMMWORD[80+rsi],xmm12

	vpxor	xmm0,xmm0,xmm5

	lea	rdi,[96+rdi]
	lea	rsi,[96+rsi]
	jmp	NEAR $L$256_dec_loop1

$L$256_dec_finish_96:
	vmovdqa	xmm6,xmm12
	vmovdqa	XMMWORD[(16-32)+rax],xmm11
	vmovdqa	XMMWORD[(32-32)+rax],xmm10
	vmovdqa	XMMWORD[(48-32)+rax],xmm9
	vmovdqa	XMMWORD[(64-32)+rax],xmm8
	vmovdqa	XMMWORD[(80-32)+rax],xmm7

	vmovdqu	xmm4,XMMWORD[((0-32))+rcx]
	vpclmulqdq	xmm1,xmm6,xmm4,0x10
	vpclmulqdq	xmm2,xmm6,xmm4,0x11
	vpclmulqdq	xmm3,xmm6,xmm4,0x00
	vpclmulqdq	xmm4,xmm6,xmm4,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[((-16))+rax]
	vmovdqu	xmm13,XMMWORD[((-16))+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[rax]
	vmovdqu	xmm13,XMMWORD[rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[16+rax]
	vmovdqu	xmm13,XMMWORD[16+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4

	vmovdqu	xmm6,XMMWORD[32+rax]
	vmovdqu	xmm13,XMMWORD[32+rcx]

	vpclmulqdq	xmm4,xmm6,xmm13,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm13,0x01
	vpxor	xmm1,xmm1,xmm4


	vmovdqu	xmm6,XMMWORD[((80-32))+rax]
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm5,XMMWORD[((80-32))+rcx]
	vpclmulqdq	xmm4,xmm6,xmm5,0x11
	vpxor	xmm2,xmm2,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x00
	vpxor	xmm3,xmm3,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x10
	vpxor	xmm1,xmm1,xmm4
	vpclmulqdq	xmm4,xmm6,xmm5,0x01
	vpxor	xmm1,xmm1,xmm4

	vpsrldq	xmm4,xmm1,8
	vpxor	xmm5,xmm2,xmm4
	vpslldq	xmm4,xmm1,8
	vpxor	xmm0,xmm3,xmm4

	vmovdqa	xmm3,XMMWORD[poly]

	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vpalignr	xmm2,xmm0,xmm0,8
	vpclmulqdq	xmm0,xmm0,xmm3,0x10
	vpxor	xmm0,xmm2,xmm0

	vpxor	xmm0,xmm0,xmm5

$L$256_dec_loop2:
; Here we encrypt any remaining whole block

; if there are no whole blocks
	cmp	r9,16
	jb	NEAR $L$256_dec_out
	sub	r9,16

	vmovdqa	xmm2,xmm15
	vpaddd	xmm15,xmm15,XMMWORD[one]

	vpxor	xmm2,xmm2,XMMWORD[r8]
	vaesenc	xmm2,xmm2,XMMWORD[16+r8]
	vaesenc	xmm2,xmm2,XMMWORD[32+r8]
	vaesenc	xmm2,xmm2,XMMWORD[48+r8]
	vaesenc	xmm2,xmm2,XMMWORD[64+r8]
	vaesenc	xmm2,xmm2,XMMWORD[80+r8]
	vaesenc	xmm2,xmm2,XMMWORD[96+r8]
	vaesenc	xmm2,xmm2,XMMWORD[112+r8]
	vaesenc	xmm2,xmm2,XMMWORD[128+r8]
	vaesenc	xmm2,xmm2,XMMWORD[144+r8]
	vaesenc	xmm2,xmm2,XMMWORD[160+r8]
	vaesenc	xmm2,xmm2,XMMWORD[176+r8]
	vaesenc	xmm2,xmm2,XMMWORD[192+r8]
	vaesenc	xmm2,xmm2,XMMWORD[208+r8]
	vaesenclast	xmm2,xmm2,XMMWORD[224+r8]
	vpxor	xmm2,xmm2,XMMWORD[rdi]
	vmovdqu	XMMWORD[rsi],xmm2
	add	rdi,16
	add	rsi,16

	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm1,XMMWORD[((-32))+rcx]
	call	GFMUL

	jmp	NEAR $L$256_dec_loop2

$L$256_dec_out:
	vmovdqu	XMMWORD[rdx],xmm0
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes256gcmsiv_dec:
global	aes256gcmsiv_kdf

ALIGN	16
aes256gcmsiv_kdf:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aes256gcmsiv_kdf:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
; parameter 1: %rdi                         Pointer to NONCE
; parameter 2: %rsi                         Pointer to CT
; parameter 4: %rdx                         Pointer to keys

	vmovdqa	xmm1,XMMWORD[rdx]  ; xmm1 = first 16 bytes of random key
	vmovdqa	xmm4,XMMWORD[rdi]
	vmovdqa	xmm11,XMMWORD[and_mask]
	vmovdqa	xmm8,XMMWORD[one]
	vpshufd	xmm4,xmm4,0x90
	vpand	xmm4,xmm4,xmm11
	vpaddd	xmm6,xmm4,xmm8
	vpaddd	xmm7,xmm6,xmm8
	vpaddd	xmm11,xmm7,xmm8
	vpaddd	xmm12,xmm11,xmm8
	vpaddd	xmm13,xmm12,xmm8

	vpxor	xmm4,xmm4,xmm1
	vpxor	xmm6,xmm6,xmm1
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm11,xmm11,xmm1
	vpxor	xmm12,xmm12,xmm1
	vpxor	xmm13,xmm13,xmm1

	vmovdqa	xmm1,XMMWORD[16+rdx]
	vaesenc	xmm4,xmm4,xmm1
	vaesenc	xmm6,xmm6,xmm1
	vaesenc	xmm7,xmm7,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1

	vmovdqa	xmm2,XMMWORD[32+rdx]
	vaesenc	xmm4,xmm4,xmm2
	vaesenc	xmm6,xmm6,xmm2
	vaesenc	xmm7,xmm7,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2
	vaesenc	xmm13,xmm13,xmm2

	vmovdqa	xmm1,XMMWORD[48+rdx]
	vaesenc	xmm4,xmm4,xmm1
	vaesenc	xmm6,xmm6,xmm1
	vaesenc	xmm7,xmm7,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1

	vmovdqa	xmm2,XMMWORD[64+rdx]
	vaesenc	xmm4,xmm4,xmm2
	vaesenc	xmm6,xmm6,xmm2
	vaesenc	xmm7,xmm7,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2
	vaesenc	xmm13,xmm13,xmm2

	vmovdqa	xmm1,XMMWORD[80+rdx]
	vaesenc	xmm4,xmm4,xmm1
	vaesenc	xmm6,xmm6,xmm1
	vaesenc	xmm7,xmm7,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1

	vmovdqa	xmm2,XMMWORD[96+rdx]
	vaesenc	xmm4,xmm4,xmm2
	vaesenc	xmm6,xmm6,xmm2
	vaesenc	xmm7,xmm7,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2
	vaesenc	xmm13,xmm13,xmm2

	vmovdqa	xmm1,XMMWORD[112+rdx]
	vaesenc	xmm4,xmm4,xmm1
	vaesenc	xmm6,xmm6,xmm1
	vaesenc	xmm7,xmm7,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1

	vmovdqa	xmm2,XMMWORD[128+rdx]
	vaesenc	xmm4,xmm4,xmm2
	vaesenc	xmm6,xmm6,xmm2
	vaesenc	xmm7,xmm7,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2
	vaesenc	xmm13,xmm13,xmm2

	vmovdqa	xmm1,XMMWORD[144+rdx]
	vaesenc	xmm4,xmm4,xmm1
	vaesenc	xmm6,xmm6,xmm1
	vaesenc	xmm7,xmm7,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1

	vmovdqa	xmm2,XMMWORD[160+rdx]
	vaesenc	xmm4,xmm4,xmm2
	vaesenc	xmm6,xmm6,xmm2
	vaesenc	xmm7,xmm7,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2
	vaesenc	xmm13,xmm13,xmm2

	vmovdqa	xmm1,XMMWORD[176+rdx]
	vaesenc	xmm4,xmm4,xmm1
	vaesenc	xmm6,xmm6,xmm1
	vaesenc	xmm7,xmm7,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1

	vmovdqa	xmm2,XMMWORD[192+rdx]
	vaesenc	xmm4,xmm4,xmm2
	vaesenc	xmm6,xmm6,xmm2
	vaesenc	xmm7,xmm7,xmm2
	vaesenc	xmm11,xmm11,xmm2
	vaesenc	xmm12,xmm12,xmm2
	vaesenc	xmm13,xmm13,xmm2

	vmovdqa	xmm1,XMMWORD[208+rdx]
	vaesenc	xmm4,xmm4,xmm1
	vaesenc	xmm6,xmm6,xmm1
	vaesenc	xmm7,xmm7,xmm1
	vaesenc	xmm11,xmm11,xmm1
	vaesenc	xmm12,xmm12,xmm1
	vaesenc	xmm13,xmm13,xmm1

	vmovdqa	xmm2,XMMWORD[224+rdx]
	vaesenclast	xmm4,xmm4,xmm2
	vaesenclast	xmm6,xmm6,xmm2
	vaesenclast	xmm7,xmm7,xmm2
	vaesenclast	xmm11,xmm11,xmm2
	vaesenclast	xmm12,xmm12,xmm2
	vaesenclast	xmm13,xmm13,xmm2


	vmovdqa	XMMWORD[rsi],xmm4
	vmovdqa	XMMWORD[16+rsi],xmm6
	vmovdqa	XMMWORD[32+rsi],xmm7
	vmovdqa	XMMWORD[48+rsi],xmm11
	vmovdqa	XMMWORD[64+rsi],xmm12
	vmovdqa	XMMWORD[80+rsi],xmm13
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_aes256gcmsiv_kdf:
%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
