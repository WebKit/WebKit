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

ALIGN	64
aes_gcm_avx512_constants:

; A shuffle mask that reflects the bytes of 16-byte blocks
$L$bswap_mask:
	DQ	0x08090a0b0c0d0e0f,0x0001020304050607

; This is the GHASH reducing polynomial without its constant term, i.e.
; x^128 + x^7 + x^2 + x, represented using the backwards mapping
; between bits and polynomial coefficients.
; 
; Alternatively, it can be interpreted as the naturally-ordered
; representation of the polynomial x^127 + x^126 + x^121 + 1, i.e. the
; "reversed" GHASH reducing polynomial without its x^128 term.
$L$gfpoly:
	DQ	1,0xc200000000000000

; Same as above, but with the (1 << 64) bit set.
$L$gfpoly_and_internal_carrybit:
	DQ	1,0xc200000000000001

; Values needed to prepare the initial vector of counter blocks.
$L$ctr_pattern:
	DQ	0,0
	DQ	1,0
	DQ	2,0
	DQ	3,0

; The number of AES blocks per vector, as a 128-bit value.
$L$inc_4blocks:
	DQ	4,0

section	.text code align=64

global	gcm_init_vpclmulqdq_avx512

ALIGN	32
gcm_init_vpclmulqdq_avx512:


_CET_ENDBR
; Get pointer to lowest set of key powers (located at end of array).
	lea	r8,[((256-64))+rcx]

; Load the byte-reflected hash subkey.  BoringSSL provides it in
; byte-reflected form except the two halves are in the wrong order.
	vpshufd	xmm3,XMMWORD[rdx],0x4e

; Finish preprocessing the first key power, H^1.  Since this GHASH
; implementation operates directly on values with the backwards bit
; order specified by the GCM standard, it's necessary to preprocess the
; raw key as follows.  First, reflect its bytes.  Second, multiply it
; by x^-1 mod x^128 + x^7 + x^2 + x + 1 (if using the backwards
; interpretation of polynomial coefficients), which can also be
; interpreted as multiplication by x mod x^128 + x^127 + x^126 + x^121
; + 1 using the alternative, natural interpretation of polynomial
; coefficients.  For details, see the comment above _ghash_mul.
; 
; Either way, for the multiplication the concrete operation performed
; is a left shift of the 128-bit value by 1 bit, then an XOR with (0xc2
; << 120) | 1 if a 1 bit was carried out.  However, there's no 128-bit
; wide shift instruction, so instead double each of the two 64-bit
; halves and incorporate the internal carry bit into the value XOR'd.
	vpshufd	xmm0,xmm3,0xd3
	vpsrad	xmm0,xmm0,31
	vpaddq	xmm3,xmm3,xmm3
; H_CUR_XMM ^= TMP0_XMM & gfpoly_and_internal_carrybit
	vpternlogd	xmm3,xmm0,XMMWORD[$L$gfpoly_and_internal_carrybit],0x78

; Load the gfpoly constant.
	vbroadcasti32x4	zmm5,ZMMWORD[$L$gfpoly]

; Square H^1 to get H^2.
; 
; Note that as with H^1, all higher key powers also need an extra
; factor of x^-1 (or x using the natural interpretation).  Nothing
; special needs to be done to make this happen, though: H^1 * H^1 would
; end up with two factors of x^-1, but the multiplication consumes one.
; So the product H^2 ends up with the desired one factor of x^-1.
	vpclmulqdq	xmm0,xmm3,xmm3,0x00  ; LO = a_L * a_L
	vpclmulqdq	xmm4,xmm3,xmm3,0x11  ; HI = a_H * a_H
	vpclmulqdq	xmm1,xmm5,xmm0,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpshufd	xmm0,xmm0,0x4e  ; Swap halves of LO
	vpxor	xmm1,xmm1,xmm0  ; Fold LO into MI
	vpclmulqdq	xmm0,xmm5,xmm1,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpshufd	xmm1,xmm1,0x4e  ; Swap halves of MI
	vpternlogd	xmm4,xmm1,xmm0,0x96  ; Fold MI into HI


; Create H_CUR_YMM = [H^2, H^1] and H_INC_YMM = [H^2, H^2].
	vinserti128	ymm3,ymm4,xmm3,1
	vinserti128	ymm4,ymm4,xmm4,1

; Create H_CUR = [H^4, H^3, H^2, H^1] and H_INC = [H^4, H^4, H^4, H^4].
	vpclmulqdq	ymm0,ymm3,ymm4,0x00  ; LO = a_L * b_L
	vpclmulqdq	ymm1,ymm3,ymm4,0x01  ; MI_0 = a_L * b_H
	vpclmulqdq	ymm2,ymm3,ymm4,0x10  ; MI_1 = a_H * b_L
	vpxord	ymm1,ymm1,ymm2  ; MI = MI_0 + MI_1
	vpclmulqdq	ymm2,ymm5,ymm0,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpshufd	ymm0,ymm0,0x4e  ; Swap halves of LO
	vpternlogd	ymm1,ymm0,ymm2,0x96  ; Fold LO into MI
	vpclmulqdq	ymm4,ymm3,ymm4,0x11  ; HI = a_H * b_H
	vpclmulqdq	ymm0,ymm5,ymm1,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpshufd	ymm1,ymm1,0x4e  ; Swap halves of MI
	vpternlogd	ymm4,ymm1,ymm0,0x96  ; Fold MI into HI

	vinserti64x4	zmm3,zmm4,ymm3,1
	vshufi64x2	zmm4,zmm4,zmm4,0

; Store the lowest set of key powers.
	vmovdqu8	ZMMWORD[r8],zmm3

; Compute and store the remaining key powers.
; Repeatedly multiply [H^(i+3), H^(i+2), H^(i+1), H^i] by
; [H^4, H^4, H^4, H^4] to get [H^(i+7), H^(i+6), H^(i+5), H^(i+4)].
	mov	eax,3
$L$precompute_next:
	sub	r8,64
	vpclmulqdq	zmm0,zmm3,zmm4,0x00  ; LO = a_L * b_L
	vpclmulqdq	zmm1,zmm3,zmm4,0x01  ; MI_0 = a_L * b_H
	vpclmulqdq	zmm2,zmm3,zmm4,0x10  ; MI_1 = a_H * b_L
	vpxord	zmm1,zmm1,zmm2  ; MI = MI_0 + MI_1
	vpclmulqdq	zmm2,zmm5,zmm0,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpshufd	zmm0,zmm0,0x4e  ; Swap halves of LO
	vpternlogd	zmm1,zmm0,zmm2,0x96  ; Fold LO into MI
	vpclmulqdq	zmm3,zmm3,zmm4,0x11  ; HI = a_H * b_H
	vpclmulqdq	zmm0,zmm5,zmm1,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpshufd	zmm1,zmm1,0x4e  ; Swap halves of MI
	vpternlogd	zmm3,zmm1,zmm0,0x96  ; Fold MI into HI

	vmovdqu8	ZMMWORD[r8],zmm3
	dec	eax
	jnz	NEAR $L$precompute_next

	vzeroupper  ; This is needed after using ymm or zmm registers.
	ret



global	gcm_gmult_vpclmulqdq_avx512

ALIGN	32
gcm_gmult_vpclmulqdq_avx512:

$L$SEH_begin_gcm_gmult_vpclmulqdq_avx512_1:
_CET_ENDBR
	sub	rsp,24
$L$SEH_prologue_gcm_gmult_vpclmulqdq_avx512_2:
	vmovdqa	XMMWORD[rsp],xmm6
$L$SEH_prologue_gcm_gmult_vpclmulqdq_avx512_3:

$L$SEH_endprologue_gcm_gmult_vpclmulqdq_avx512_4:

	vmovdqu	xmm0,XMMWORD[rcx]
	vmovdqu	xmm1,XMMWORD[$L$bswap_mask]
	vmovdqu	xmm2,XMMWORD[((256-16))+rdx]
	vmovdqu	xmm3,XMMWORD[$L$gfpoly]
	vpshufb	xmm0,xmm0,xmm1

	vpclmulqdq	xmm4,xmm0,xmm2,0x00  ; LO = a_L * b_L
	vpclmulqdq	xmm5,xmm0,xmm2,0x01  ; MI_0 = a_L * b_H
	vpclmulqdq	xmm6,xmm0,xmm2,0x10  ; MI_1 = a_H * b_L
	vpxord	xmm5,xmm5,xmm6  ; MI = MI_0 + MI_1
	vpclmulqdq	xmm6,xmm3,xmm4,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpshufd	xmm4,xmm4,0x4e  ; Swap halves of LO
	vpternlogd	xmm5,xmm4,xmm6,0x96  ; Fold LO into MI
	vpclmulqdq	xmm0,xmm0,xmm2,0x11  ; HI = a_H * b_H
	vpclmulqdq	xmm4,xmm3,xmm5,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpshufd	xmm5,xmm5,0x4e  ; Swap halves of MI
	vpternlogd	xmm0,xmm5,xmm4,0x96  ; Fold MI into HI


	vpshufb	xmm0,xmm0,xmm1
	vmovdqu	XMMWORD[rcx],xmm0

; No need for vzeroupper, since only xmm registers were used.
	vmovdqa	xmm6,XMMWORD[rsp]
	add	rsp,24
	ret
$L$SEH_end_gcm_gmult_vpclmulqdq_avx512_5:


global	gcm_ghash_vpclmulqdq_avx512

ALIGN	32
gcm_ghash_vpclmulqdq_avx512:

$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1:
_CET_ENDBR
	sub	rsp,136
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_2:
	vmovdqa	XMMWORD[rsp],xmm6
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_3:
	vmovdqa	XMMWORD[16+rsp],xmm7
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_4:
	vmovdqa	XMMWORD[32+rsp],xmm8
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_5:
	vmovdqa	XMMWORD[48+rsp],xmm9
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_6:
	vmovdqa	XMMWORD[64+rsp],xmm10
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_7:
	vmovdqa	XMMWORD[80+rsp],xmm11
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_8:
	vmovdqa	XMMWORD[96+rsp],xmm12
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_9:
	vmovdqa	XMMWORD[112+rsp],xmm13
$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_10:

$L$SEH_endprologue_gcm_ghash_vpclmulqdq_avx512_11:

; Load the bswap_mask and gfpoly constants.  Since AADLEN is usually small,
; usually only 128-bit vectors will be used.  So as an optimization, don't
; broadcast these constants to all 128-bit lanes quite yet.
	vmovdqu	xmm4,XMMWORD[$L$bswap_mask]
	vmovdqu	xmm10,XMMWORD[$L$gfpoly]

; Load the GHASH accumulator.
	vmovdqu	xmm5,XMMWORD[rcx]
	vpshufb	xmm5,xmm5,xmm4

; Optimize for AADLEN < 64 by checking for AADLEN < 64 before AADLEN < 256.
	cmp	r9,64
	jb	NEAR $L$aad_blockbyblock

; AADLEN >= 64, so we'll operate on full vectors.  Broadcast bswap_mask and
; gfpoly to all 128-bit lanes.
	vshufi64x2	zmm4,zmm4,zmm4,0
	vshufi64x2	zmm10,zmm10,zmm10,0

; Load the lowest set of key powers.
	vmovdqu8	zmm9,ZMMWORD[((256-64))+rdx]

	cmp	r9,256
	jb	NEAR $L$aad_loop_1x

; AADLEN >= 256.  Load the higher key powers.
	vmovdqu8	zmm6,ZMMWORD[((256-256))+rdx]
	vmovdqu8	zmm7,ZMMWORD[((256-192))+rdx]
	vmovdqu8	zmm8,ZMMWORD[((256-128))+rdx]

; Update GHASH with 256 bytes of AAD at a time.
$L$aad_loop_4x:
	vmovdqu8	zmm0,ZMMWORD[r8]
	vmovdqu8	zmm1,ZMMWORD[64+r8]
	vmovdqu8	zmm2,ZMMWORD[128+r8]
	vmovdqu8	zmm3,ZMMWORD[192+r8]
	vpshufb	zmm0,zmm0,zmm4
	vpxord	zmm0,zmm0,zmm5
	vpshufb	zmm1,zmm1,zmm4
	vpshufb	zmm2,zmm2,zmm4
	vpshufb	zmm3,zmm3,zmm4
	vpclmulqdq	zmm5,zmm0,zmm6,0x00  ; LO_0
	vpclmulqdq	zmm11,zmm1,zmm7,0x00  ; LO_1
	vpclmulqdq	zmm12,zmm2,zmm8,0x00  ; LO_2
	vpxord	zmm5,zmm5,zmm11  ; sum(LO_{1,0})
	vpclmulqdq	zmm13,zmm3,zmm9,0x00  ; LO_3
	vpternlogd	zmm5,zmm12,zmm13,0x96  ; LO = sum(LO_{3,2,1,0})
	vpclmulqdq	zmm11,zmm0,zmm6,0x01  ; MI_0
	vpclmulqdq	zmm12,zmm1,zmm7,0x01  ; MI_1
	vpclmulqdq	zmm13,zmm2,zmm8,0x01  ; MI_2
	vpternlogd	zmm11,zmm12,zmm13,0x96  ; sum(MI_{2,1,0})
	vpclmulqdq	zmm12,zmm3,zmm9,0x01  ; MI_3
	vpclmulqdq	zmm13,zmm0,zmm6,0x10  ; MI_4
	vpternlogd	zmm11,zmm12,zmm13,0x96  ; sum(MI_{4,3,2,1,0})
	vpclmulqdq	zmm12,zmm1,zmm7,0x10  ; MI_5
	vpclmulqdq	zmm13,zmm2,zmm8,0x10  ; MI_6
	vpternlogd	zmm11,zmm12,zmm13,0x96  ; sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	zmm13,zmm10,zmm5,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	zmm12,zmm3,zmm9,0x10  ; MI_7
	vpxord	zmm11,zmm11,zmm12  ; MI = sum(MI_{7,6,5,4,3,2,1,0})
	vpshufd	zmm5,zmm5,0x4e  ; Swap halves of LO
	vpclmulqdq	zmm0,zmm0,zmm6,0x11  ; HI_0
	vpclmulqdq	zmm1,zmm1,zmm7,0x11  ; HI_1
	vpclmulqdq	zmm2,zmm2,zmm8,0x11  ; HI_2
	vpternlogd	zmm11,zmm5,zmm13,0x96  ; Fold LO into MI
	vpclmulqdq	zmm3,zmm3,zmm9,0x11  ; HI_3
	vpternlogd	zmm0,zmm1,zmm2,0x96  ; sum(HI_{2,1,0})
	vpclmulqdq	zmm12,zmm10,zmm11,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpxord	zmm5,zmm0,zmm3  ; HI = sum(HI_{3,2,1,0})
	vpshufd	zmm11,zmm11,0x4e  ; Swap halves of MI
	vpternlogd	zmm5,zmm11,zmm12,0x96  ; Fold MI into HI
	vextracti32x4	xmm0,zmm5,1
	vextracti32x4	xmm1,zmm5,2
	vextracti32x4	xmm2,zmm5,3
	vpxord	xmm5,xmm5,xmm0
	vpternlogd	xmm5,xmm2,xmm1,0x96

	add	r8,256
	sub	r9,256
	cmp	r9,256
	jae	NEAR $L$aad_loop_4x

; Update GHASH with 64 bytes of AAD at a time.
	cmp	r9,64
	jb	NEAR $L$aad_large_done
$L$aad_loop_1x:
	vmovdqu8	zmm0,ZMMWORD[r8]
	vpshufb	zmm0,zmm0,zmm4
	vpxord	zmm5,zmm5,zmm0
	vpclmulqdq	zmm0,zmm5,zmm9,0x00  ; LO = a_L * b_L
	vpclmulqdq	zmm1,zmm5,zmm9,0x01  ; MI_0 = a_L * b_H
	vpclmulqdq	zmm2,zmm5,zmm9,0x10  ; MI_1 = a_H * b_L
	vpxord	zmm1,zmm1,zmm2  ; MI = MI_0 + MI_1
	vpclmulqdq	zmm2,zmm10,zmm0,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpshufd	zmm0,zmm0,0x4e  ; Swap halves of LO
	vpternlogd	zmm1,zmm0,zmm2,0x96  ; Fold LO into MI
	vpclmulqdq	zmm5,zmm5,zmm9,0x11  ; HI = a_H * b_H
	vpclmulqdq	zmm0,zmm10,zmm1,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpshufd	zmm1,zmm1,0x4e  ; Swap halves of MI
	vpternlogd	zmm5,zmm1,zmm0,0x96  ; Fold MI into HI

	vextracti32x4	xmm0,zmm5,1
	vextracti32x4	xmm1,zmm5,2
	vextracti32x4	xmm2,zmm5,3
	vpxord	xmm5,xmm5,xmm0
	vpternlogd	xmm5,xmm2,xmm1,0x96

	add	r8,64
	sub	r9,64
	cmp	r9,64
	jae	NEAR $L$aad_loop_1x

$L$aad_large_done:

; GHASH the remaining data 16 bytes at a time, using xmm registers only.
$L$aad_blockbyblock:
	test	r9,r9
	jz	NEAR $L$aad_done
	vmovdqu	xmm9,XMMWORD[((256-16))+rdx]
$L$aad_loop_blockbyblock:
	vmovdqu	xmm0,XMMWORD[r8]
	vpshufb	xmm0,xmm0,xmm4
	vpxor	xmm5,xmm5,xmm0
	vpclmulqdq	xmm0,xmm5,xmm9,0x00  ; LO = a_L * b_L
	vpclmulqdq	xmm1,xmm5,xmm9,0x01  ; MI_0 = a_L * b_H
	vpclmulqdq	xmm2,xmm5,xmm9,0x10  ; MI_1 = a_H * b_L
	vpxord	xmm1,xmm1,xmm2  ; MI = MI_0 + MI_1
	vpclmulqdq	xmm2,xmm10,xmm0,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpshufd	xmm0,xmm0,0x4e  ; Swap halves of LO
	vpternlogd	xmm1,xmm0,xmm2,0x96  ; Fold LO into MI
	vpclmulqdq	xmm5,xmm5,xmm9,0x11  ; HI = a_H * b_H
	vpclmulqdq	xmm0,xmm10,xmm1,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpshufd	xmm1,xmm1,0x4e  ; Swap halves of MI
	vpternlogd	xmm5,xmm1,xmm0,0x96  ; Fold MI into HI

	add	r8,16
	sub	r9,16
	jnz	NEAR $L$aad_loop_blockbyblock

$L$aad_done:
; Store the updated GHASH accumulator back to memory.
	vpshufb	xmm5,xmm5,xmm4
	vmovdqu	XMMWORD[rcx],xmm5

	vzeroupper  ; This is needed after using ymm or zmm registers.
	vmovdqa	xmm6,XMMWORD[rsp]
	vmovdqa	xmm7,XMMWORD[16+rsp]
	vmovdqa	xmm8,XMMWORD[32+rsp]
	vmovdqa	xmm9,XMMWORD[48+rsp]
	vmovdqa	xmm10,XMMWORD[64+rsp]
	vmovdqa	xmm11,XMMWORD[80+rsp]
	vmovdqa	xmm12,XMMWORD[96+rsp]
	vmovdqa	xmm13,XMMWORD[112+rsp]
	add	rsp,136
	ret
$L$SEH_end_gcm_ghash_vpclmulqdq_avx512_12:


global	aes_gcm_enc_update_vaes_avx512

ALIGN	32
aes_gcm_enc_update_vaes_avx512:

$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1:
_CET_ENDBR
	push	rsi
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_2:
	push	rdi
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_3:
	push	r12
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_4:

	mov	rsi,QWORD[64+rsp]  ; arg5
	mov	rdi,QWORD[72+rsp]  ; arg6
	mov	r12,QWORD[80+rsp]  ; arg7
	sub	rsp,160
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_5:
	vmovdqa	XMMWORD[rsp],xmm6
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_6:
	vmovdqa	XMMWORD[16+rsp],xmm7
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_7:
	vmovdqa	XMMWORD[32+rsp],xmm8
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_8:
	vmovdqa	XMMWORD[48+rsp],xmm9
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_9:
	vmovdqa	XMMWORD[64+rsp],xmm10
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_10:
	vmovdqa	XMMWORD[80+rsp],xmm11
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_11:
	vmovdqa	XMMWORD[96+rsp],xmm12
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_12:
	vmovdqa	XMMWORD[112+rsp],xmm13
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_13:
	vmovdqa	XMMWORD[128+rsp],xmm14
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_14:
	vmovdqa	XMMWORD[144+rsp],xmm15
$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_15:

$L$SEH_endprologue_aes_gcm_enc_update_vaes_avx512_16:
%ifdef BORINGSSL_DISPATCH_TEST
EXTERN	BORINGSSL_function_hit
	mov	BYTE[((BORINGSSL_function_hit+7))],1
%endif
; Load some constants.
	vbroadcasti32x4	zmm8,ZMMWORD[$L$bswap_mask]
	vbroadcasti32x4	zmm31,ZMMWORD[$L$gfpoly]

; Load the GHASH accumulator and the starting counter.
; BoringSSL passes these values in big endian format.
	vmovdqu	xmm10,XMMWORD[r12]
	vpshufb	xmm10,xmm10,xmm8
	vbroadcasti32x4	zmm12,ZMMWORD[rsi]
	vpshufb	zmm12,zmm12,zmm8

; Load the AES key length in bytes.  BoringSSL stores number of rounds
; minus 1, so convert using: AESKEYLEN = 4 * aeskey->rounds - 20.
	mov	r10d,DWORD[240+r9]
	lea	r10d,[((-20))+r10*4]

; Make RNDKEYLAST_PTR point to the last AES round key.  This is the
; round key with index 10, 12, or 14 for AES-128, AES-192, or AES-256
; respectively.  Then load the zero-th and last round keys.
	lea	r11,[96+r10*4+r9]
	vbroadcasti32x4	zmm13,ZMMWORD[r9]
	vbroadcasti32x4	zmm14,ZMMWORD[r11]

; Finish initializing LE_CTR by adding [0, 1, 2, 3] to its low words.
	vpaddd	zmm12,zmm12,ZMMWORD[$L$ctr_pattern]

; Load 4 into all 128-bit lanes of LE_CTR_INC.
	vbroadcasti32x4	zmm11,ZMMWORD[$L$inc_4blocks]

; If there are at least 256 bytes of data, then continue into the loop
; that processes 256 bytes of data at a time.  Otherwise skip it.
	cmp	r8,256
	jb	NEAR $L$crypt_loop_4x_done__func1

; Load powers of the hash key.
	vmovdqu8	zmm27,ZMMWORD[((256-256))+rdi]
	vmovdqu8	zmm28,ZMMWORD[((256-192))+rdi]
	vmovdqu8	zmm29,ZMMWORD[((256-128))+rdi]
	vmovdqu8	zmm30,ZMMWORD[((256-64))+rdi]
; Encrypt the first 4 vectors of plaintext blocks.  Leave the resulting
; ciphertext in GHASHDATA[0-3] for GHASH.
; Increment le_ctr four times to generate four vectors of little-endian
; counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vpshufb	zmm0,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm1,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm2,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm3,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11

; AES "round zero": XOR in the zero-th round key.
	vpxord	zmm0,zmm0,zmm13
	vpxord	zmm1,zmm1,zmm13
	vpxord	zmm2,zmm2,zmm13
	vpxord	zmm3,zmm3,zmm13

	lea	rax,[16+r9]
$L$vaesenc_loop_first_4_vecs__func1:
	vbroadcasti32x4	zmm9,ZMMWORD[rax]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

	add	rax,16
	cmp	r11,rax
	jne	NEAR $L$vaesenc_loop_first_4_vecs__func1
	vpxord	zmm4,zmm14,ZMMWORD[rcx]
	vpxord	zmm5,zmm14,ZMMWORD[64+rcx]
	vpxord	zmm6,zmm14,ZMMWORD[128+rcx]
	vpxord	zmm7,zmm14,ZMMWORD[192+rcx]
	vaesenclast	zmm4,zmm0,zmm4
	vaesenclast	zmm5,zmm1,zmm5
	vaesenclast	zmm6,zmm2,zmm6
	vaesenclast	zmm7,zmm3,zmm7
	vmovdqu8	ZMMWORD[rdx],zmm4
	vmovdqu8	ZMMWORD[64+rdx],zmm5
	vmovdqu8	ZMMWORD[128+rdx],zmm6
	vmovdqu8	ZMMWORD[192+rdx],zmm7

	add	rcx,256
	add	rdx,256
	sub	r8,256
	cmp	r8,256
	jb	NEAR $L$ghash_last_ciphertext_4x__func1
; Cache as many additional AES round keys as possible.
	vbroadcasti32x4	zmm15,ZMMWORD[((-144))+r11]
	vbroadcasti32x4	zmm16,ZMMWORD[((-128))+r11]
	vbroadcasti32x4	zmm17,ZMMWORD[((-112))+r11]
	vbroadcasti32x4	zmm18,ZMMWORD[((-96))+r11]
	vbroadcasti32x4	zmm19,ZMMWORD[((-80))+r11]
	vbroadcasti32x4	zmm20,ZMMWORD[((-64))+r11]
	vbroadcasti32x4	zmm21,ZMMWORD[((-48))+r11]
	vbroadcasti32x4	zmm22,ZMMWORD[((-32))+r11]
	vbroadcasti32x4	zmm23,ZMMWORD[((-16))+r11]

$L$crypt_loop_4x__func1:
; Start the AES encryption of the counter blocks.
; Increment le_ctr four times to generate four vectors of little-endian
; counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vpshufb	zmm0,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm1,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm2,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm3,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11

; AES "round zero": XOR in the zero-th round key.
	vpxord	zmm0,zmm0,zmm13
	vpxord	zmm1,zmm1,zmm13
	vpxord	zmm2,zmm2,zmm13
	vpxord	zmm3,zmm3,zmm13

	cmp	r10d,24
	jl	NEAR $L$aes128__func1
	je	NEAR $L$aes192__func1
; AES-256
	vbroadcasti32x4	zmm9,ZMMWORD[((-208))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

	vbroadcasti32x4	zmm9,ZMMWORD[((-192))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

$L$aes192__func1:
	vbroadcasti32x4	zmm9,ZMMWORD[((-176))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

	vbroadcasti32x4	zmm9,ZMMWORD[((-160))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

$L$aes128__func1:

; Prefetch the source data 512 bytes ahead into the L1 data cache, to
; improve performance when the hardware prefetcher is disabled.  Assumes the
; L1 data cache line size is 64 bytes (de facto standard on x86_64).
	prefetcht0	[((512+0))+rcx]
	prefetcht0	[((512+64))+rcx]
	prefetcht0	[((512+128))+rcx]
	prefetcht0	[((512+192))+rcx]

; Finish the AES encryption of the counter blocks in AESDATA[0-3],
; interleaved with the GHASH update of the ciphertext blocks in
; GHASHDATA[0-3].
	vpshufb	zmm4,zmm4,zmm8
	vpxord	zmm4,zmm4,zmm10
	vpshufb	zmm5,zmm5,zmm8
	vpshufb	zmm6,zmm6,zmm8

	vaesenc	zmm0,zmm0,zmm15
	vaesenc	zmm1,zmm1,zmm15
	vaesenc	zmm2,zmm2,zmm15
	vaesenc	zmm3,zmm3,zmm15

	vpshufb	zmm7,zmm7,zmm8
	vpclmulqdq	zmm10,zmm4,zmm27,0x00  ; LO_0
	vpclmulqdq	zmm24,zmm5,zmm28,0x00  ; LO_1
	vpclmulqdq	zmm25,zmm6,zmm29,0x00  ; LO_2

	vaesenc	zmm0,zmm0,zmm16
	vaesenc	zmm1,zmm1,zmm16
	vaesenc	zmm2,zmm2,zmm16
	vaesenc	zmm3,zmm3,zmm16

	vpxord	zmm10,zmm10,zmm24  ; sum(LO_{1,0})
	vpclmulqdq	zmm26,zmm7,zmm30,0x00  ; LO_3
	vpternlogd	zmm10,zmm25,zmm26,0x96  ; LO = sum(LO_{3,2,1,0})
	vpclmulqdq	zmm24,zmm4,zmm27,0x01  ; MI_0

	vaesenc	zmm0,zmm0,zmm17
	vaesenc	zmm1,zmm1,zmm17
	vaesenc	zmm2,zmm2,zmm17
	vaesenc	zmm3,zmm3,zmm17

	vpclmulqdq	zmm25,zmm5,zmm28,0x01  ; MI_1
	vpclmulqdq	zmm26,zmm6,zmm29,0x01  ; MI_2
	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{2,1,0})
	vpclmulqdq	zmm25,zmm7,zmm30,0x01  ; MI_3

	vaesenc	zmm0,zmm0,zmm18
	vaesenc	zmm1,zmm1,zmm18
	vaesenc	zmm2,zmm2,zmm18
	vaesenc	zmm3,zmm3,zmm18

	vpclmulqdq	zmm26,zmm4,zmm27,0x10  ; MI_4
	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{4,3,2,1,0})
	vpclmulqdq	zmm25,zmm5,zmm28,0x10  ; MI_5
	vpclmulqdq	zmm26,zmm6,zmm29,0x10  ; MI_6

	vaesenc	zmm0,zmm0,zmm19
	vaesenc	zmm1,zmm1,zmm19
	vaesenc	zmm2,zmm2,zmm19
	vaesenc	zmm3,zmm3,zmm19

	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	zmm26,zmm31,zmm10,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	zmm25,zmm7,zmm30,0x10  ; MI_7
	vpxord	zmm24,zmm24,zmm25  ; MI = sum(MI_{7,6,5,4,3,2,1,0})

	vaesenc	zmm0,zmm0,zmm20
	vaesenc	zmm1,zmm1,zmm20
	vaesenc	zmm2,zmm2,zmm20
	vaesenc	zmm3,zmm3,zmm20

	vpshufd	zmm10,zmm10,0x4e  ; Swap halves of LO
	vpclmulqdq	zmm4,zmm4,zmm27,0x11  ; HI_0
	vpclmulqdq	zmm5,zmm5,zmm28,0x11  ; HI_1
	vpclmulqdq	zmm6,zmm6,zmm29,0x11  ; HI_2

	vaesenc	zmm0,zmm0,zmm21
	vaesenc	zmm1,zmm1,zmm21
	vaesenc	zmm2,zmm2,zmm21
	vaesenc	zmm3,zmm3,zmm21

	vpternlogd	zmm24,zmm10,zmm26,0x96  ; Fold LO into MI
	vpclmulqdq	zmm7,zmm7,zmm30,0x11  ; HI_3
	vpternlogd	zmm4,zmm5,zmm6,0x96  ; sum(HI_{2,1,0})
	vpclmulqdq	zmm25,zmm31,zmm24,0x01  ; MI_L*(x^63 + x^62 + x^57)

	vaesenc	zmm0,zmm0,zmm22
	vaesenc	zmm1,zmm1,zmm22
	vaesenc	zmm2,zmm2,zmm22
	vaesenc	zmm3,zmm3,zmm22

	vpxord	zmm10,zmm4,zmm7  ; HI = sum(HI_{3,2,1,0})
	vpshufd	zmm24,zmm24,0x4e  ; Swap halves of MI
	vpternlogd	zmm10,zmm24,zmm25,0x96  ; Fold MI into HI

	vaesenc	zmm0,zmm0,zmm23
	vaesenc	zmm1,zmm1,zmm23
	vaesenc	zmm2,zmm2,zmm23
	vaesenc	zmm3,zmm3,zmm23


	vextracti32x4	xmm4,zmm10,1
	vextracti32x4	xmm5,zmm10,2
	vextracti32x4	xmm6,zmm10,3
	vpxord	xmm10,xmm10,xmm4
	vpternlogd	xmm10,xmm6,xmm5,0x96

	vpxord	zmm4,zmm14,ZMMWORD[rcx]
	vpxord	zmm5,zmm14,ZMMWORD[64+rcx]
	vpxord	zmm6,zmm14,ZMMWORD[128+rcx]
	vpxord	zmm7,zmm14,ZMMWORD[192+rcx]
	vaesenclast	zmm4,zmm0,zmm4
	vaesenclast	zmm5,zmm1,zmm5
	vaesenclast	zmm6,zmm2,zmm6
	vaesenclast	zmm7,zmm3,zmm7
	vmovdqu8	ZMMWORD[rdx],zmm4
	vmovdqu8	ZMMWORD[64+rdx],zmm5
	vmovdqu8	ZMMWORD[128+rdx],zmm6
	vmovdqu8	ZMMWORD[192+rdx],zmm7

	add	rcx,256
	add	rdx,256
	sub	r8,256
	cmp	r8,256
	jae	NEAR $L$crypt_loop_4x__func1
$L$ghash_last_ciphertext_4x__func1:
	vpshufb	zmm4,zmm4,zmm8
	vpxord	zmm4,zmm4,zmm10
	vpshufb	zmm5,zmm5,zmm8
	vpshufb	zmm6,zmm6,zmm8
	vpshufb	zmm7,zmm7,zmm8
	vpclmulqdq	zmm10,zmm4,zmm27,0x00  ; LO_0
	vpclmulqdq	zmm24,zmm5,zmm28,0x00  ; LO_1
	vpclmulqdq	zmm25,zmm6,zmm29,0x00  ; LO_2
	vpxord	zmm10,zmm10,zmm24  ; sum(LO_{1,0})
	vpclmulqdq	zmm26,zmm7,zmm30,0x00  ; LO_3
	vpternlogd	zmm10,zmm25,zmm26,0x96  ; LO = sum(LO_{3,2,1,0})
	vpclmulqdq	zmm24,zmm4,zmm27,0x01  ; MI_0
	vpclmulqdq	zmm25,zmm5,zmm28,0x01  ; MI_1
	vpclmulqdq	zmm26,zmm6,zmm29,0x01  ; MI_2
	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{2,1,0})
	vpclmulqdq	zmm25,zmm7,zmm30,0x01  ; MI_3
	vpclmulqdq	zmm26,zmm4,zmm27,0x10  ; MI_4
	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{4,3,2,1,0})
	vpclmulqdq	zmm25,zmm5,zmm28,0x10  ; MI_5
	vpclmulqdq	zmm26,zmm6,zmm29,0x10  ; MI_6
	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	zmm26,zmm31,zmm10,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	zmm25,zmm7,zmm30,0x10  ; MI_7
	vpxord	zmm24,zmm24,zmm25  ; MI = sum(MI_{7,6,5,4,3,2,1,0})
	vpshufd	zmm10,zmm10,0x4e  ; Swap halves of LO
	vpclmulqdq	zmm4,zmm4,zmm27,0x11  ; HI_0
	vpclmulqdq	zmm5,zmm5,zmm28,0x11  ; HI_1
	vpclmulqdq	zmm6,zmm6,zmm29,0x11  ; HI_2
	vpternlogd	zmm24,zmm10,zmm26,0x96  ; Fold LO into MI
	vpclmulqdq	zmm7,zmm7,zmm30,0x11  ; HI_3
	vpternlogd	zmm4,zmm5,zmm6,0x96  ; sum(HI_{2,1,0})
	vpclmulqdq	zmm25,zmm31,zmm24,0x01  ; MI_L*(x^63 + x^62 + x^57)
	vpxord	zmm10,zmm4,zmm7  ; HI = sum(HI_{3,2,1,0})
	vpshufd	zmm24,zmm24,0x4e  ; Swap halves of MI
	vpternlogd	zmm10,zmm24,zmm25,0x96  ; Fold MI into HI
	vextracti32x4	xmm4,zmm10,1
	vextracti32x4	xmm5,zmm10,2
	vextracti32x4	xmm6,zmm10,3
	vpxord	xmm10,xmm10,xmm4
	vpternlogd	xmm10,xmm6,xmm5,0x96

$L$crypt_loop_4x_done__func1:
; Check whether any data remains.
	test	r8,r8
	jz	NEAR $L$done__func1

; The data length isn't a multiple of 256 bytes.  Process the remaining
; data of length 1 <= DATALEN < 256, up to one 64-byte vector at a time.
; Going one vector at a time may seem inefficient compared to having
; separate code paths for each possible number of vectors remaining.
; However, using a loop keeps the code size down, and it performs
; surprising well; modern CPUs will start executing the next iteration
; before the previous one finishes and also predict the number of loop
; iterations.  For a similar reason, we roll up the AES rounds.
; 
; On the last iteration, the remaining length may be less than 64 bytes.
; Handle this using masking.
; 
; Since there are enough key powers available for all remaining data,
; there is no need to do a GHASH reduction after each iteration.
; Instead, multiply each remaining block by its own key power, and only
; do a GHASH reduction at the very end.

; Make POWERS_PTR point to the key powers [H^N, H^(N-1), ...] where N
; is the number of blocks that remain.
	mov	rax,r8
	neg	rax
	and	rax,-16  ; -round_up(DATALEN, 16)
	lea	rsi,[256+rax*1+rdi]
	vpxor	xmm4,xmm4,xmm4
	vpxor	xmm5,xmm5,xmm5
	vpxor	xmm6,xmm6,xmm6

	cmp	r8,64
	jb	NEAR $L$partial_vec__func1

$L$crypt_loop_1x__func1:
; Process a full 64-byte vector.

; Encrypt a vector of counter blocks.
	vpshufb	zmm0,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpxord	zmm0,zmm0,zmm13
	lea	rax,[16+r9]
$L$vaesenc_loop_tail_full_vec__func1:
	vbroadcasti32x4	zmm9,ZMMWORD[rax]
	vaesenc	zmm0,zmm0,zmm9
	add	rax,16
	cmp	r11,rax
	jne	NEAR $L$vaesenc_loop_tail_full_vec__func1
	vaesenclast	zmm0,zmm0,zmm14

; XOR the data with the vector of keystream blocks.
	vmovdqu8	zmm1,ZMMWORD[rcx]
	vpxord	zmm0,zmm0,zmm1
	vmovdqu8	ZMMWORD[rdx],zmm0

; Update GHASH with the ciphertext blocks, without reducing.
	vmovdqu8	zmm30,ZMMWORD[rsi]
	vpshufb	zmm0,zmm0,zmm8
	vpxord	zmm0,zmm0,zmm10
	vpclmulqdq	zmm7,zmm0,zmm30,0x00  ; a_L * b_L
	vpclmulqdq	zmm1,zmm0,zmm30,0x01  ; a_L * b_H
	vpclmulqdq	zmm2,zmm0,zmm30,0x10  ; a_H * b_L
	vpclmulqdq	zmm3,zmm0,zmm30,0x11  ; a_H * b_H
	vpxord	zmm4,zmm4,zmm7
	vpternlogd	zmm5,zmm1,zmm2,0x96
	vpxord	zmm6,zmm6,zmm3

	vpxor	xmm10,xmm10,xmm10

	add	rsi,64
	add	rcx,64
	add	rdx,64
	sub	r8,64
	cmp	r8,64
	jae	NEAR $L$crypt_loop_1x__func1

	test	r8,r8
	jz	NEAR $L$reduce__func1

$L$partial_vec__func1:
; Process a partial vector of length 1 <= DATALEN < 64.

; Set the data mask %k1 to DATALEN 1's.
; Set the key powers mask %k2 to round_up(DATALEN, 16) 1's.
	mov	rax,-1
	bzhi	rax,rax,r8
	kmovq	k1,rax
	add	r8,15
	and	r8,-16
	mov	rax,-1
	bzhi	rax,rax,r8
	kmovq	k2,rax

; Encrypt one last vector of counter blocks.  This does not need to be
; masked.  The counter does not need to be incremented here.
	vpshufb	zmm0,zmm12,zmm8
	vpxord	zmm0,zmm0,zmm13
	lea	rax,[16+r9]
$L$vaesenc_loop_tail_partialvec__func1:
	vbroadcasti32x4	zmm9,ZMMWORD[rax]
	vaesenc	zmm0,zmm0,zmm9
	add	rax,16
	cmp	r11,rax
	jne	NEAR $L$vaesenc_loop_tail_partialvec__func1
	vaesenclast	zmm0,zmm0,zmm14

; XOR the data with the appropriate number of keystream bytes.
	vmovdqu8	zmm1{k1}{z},[rcx]
	vpxord	zmm0,zmm0,zmm1
	vmovdqu8	ZMMWORD[rdx]{k1},zmm0

; Update GHASH with the ciphertext block(s), without reducing.
; 
; In the case of DATALEN < 64, the ciphertext is zero-padded to 64
; bytes.  (If decrypting, it's done by the above masked load.  If
; encrypting, it's done by the below masked register-to-register move.)
; Note that if DATALEN <= 48, there will be additional padding beyond
; the padding of the last block specified by GHASH itself; i.e., there
; may be whole block(s) that get processed by the GHASH multiplication
; and reduction instructions but should not actually be included in the
; GHASH.  However, any such blocks are all-zeroes, and the values that
; they're multiplied with are also all-zeroes.  Therefore they just add
; 0 * 0 = 0 to the final GHASH result, which makes no difference.
	vmovdqu8	zmm30{k2}{z},[rsi]
	vmovdqu8	zmm1{k1}{z},zmm0
	vpshufb	zmm0,zmm1,zmm8
	vpxord	zmm0,zmm0,zmm10
	vpclmulqdq	zmm7,zmm0,zmm30,0x00  ; a_L * b_L
	vpclmulqdq	zmm1,zmm0,zmm30,0x01  ; a_L * b_H
	vpclmulqdq	zmm2,zmm0,zmm30,0x10  ; a_H * b_L
	vpclmulqdq	zmm3,zmm0,zmm30,0x11  ; a_H * b_H
	vpxord	zmm4,zmm4,zmm7
	vpternlogd	zmm5,zmm1,zmm2,0x96
	vpxord	zmm6,zmm6,zmm3


$L$reduce__func1:
; Finally, do the GHASH reduction.
	vpclmulqdq	zmm0,zmm31,zmm4,0x01
	vpshufd	zmm4,zmm4,0x4e
	vpternlogd	zmm5,zmm4,zmm0,0x96
	vpclmulqdq	zmm0,zmm31,zmm5,0x01
	vpshufd	zmm5,zmm5,0x4e
	vpternlogd	zmm6,zmm5,zmm0,0x96

	vextracti32x4	xmm0,zmm6,1
	vextracti32x4	xmm1,zmm6,2
	vextracti32x4	xmm2,zmm6,3
	vpxord	xmm10,xmm6,xmm0
	vpternlogd	xmm10,xmm2,xmm1,0x96


$L$done__func1:
; Store the updated GHASH accumulator back to memory.
	vpshufb	xmm10,xmm10,xmm8
	vmovdqu	XMMWORD[r12],xmm10

	vzeroupper  ; This is needed after using ymm or zmm registers.
	vmovdqa	xmm6,XMMWORD[rsp]
	vmovdqa	xmm7,XMMWORD[16+rsp]
	vmovdqa	xmm8,XMMWORD[32+rsp]
	vmovdqa	xmm9,XMMWORD[48+rsp]
	vmovdqa	xmm10,XMMWORD[64+rsp]
	vmovdqa	xmm11,XMMWORD[80+rsp]
	vmovdqa	xmm12,XMMWORD[96+rsp]
	vmovdqa	xmm13,XMMWORD[112+rsp]
	vmovdqa	xmm14,XMMWORD[128+rsp]
	vmovdqa	xmm15,XMMWORD[144+rsp]
	add	rsp,160
	pop	r12
	pop	rdi
	pop	rsi
	ret
$L$SEH_end_aes_gcm_enc_update_vaes_avx512_17:


global	aes_gcm_dec_update_vaes_avx512

ALIGN	32
aes_gcm_dec_update_vaes_avx512:

$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1:
_CET_ENDBR
	push	rsi
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_2:
	push	rdi
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_3:
	push	r12
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_4:

	mov	rsi,QWORD[64+rsp]  ; arg5
	mov	rdi,QWORD[72+rsp]  ; arg6
	mov	r12,QWORD[80+rsp]  ; arg7
	sub	rsp,160
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_5:
	vmovdqa	XMMWORD[rsp],xmm6
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_6:
	vmovdqa	XMMWORD[16+rsp],xmm7
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_7:
	vmovdqa	XMMWORD[32+rsp],xmm8
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_8:
	vmovdqa	XMMWORD[48+rsp],xmm9
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_9:
	vmovdqa	XMMWORD[64+rsp],xmm10
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_10:
	vmovdqa	XMMWORD[80+rsp],xmm11
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_11:
	vmovdqa	XMMWORD[96+rsp],xmm12
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_12:
	vmovdqa	XMMWORD[112+rsp],xmm13
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_13:
	vmovdqa	XMMWORD[128+rsp],xmm14
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_14:
	vmovdqa	XMMWORD[144+rsp],xmm15
$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_15:

$L$SEH_endprologue_aes_gcm_dec_update_vaes_avx512_16:
; Load some constants.
	vbroadcasti32x4	zmm8,ZMMWORD[$L$bswap_mask]
	vbroadcasti32x4	zmm31,ZMMWORD[$L$gfpoly]

; Load the GHASH accumulator and the starting counter.
; BoringSSL passes these values in big endian format.
	vmovdqu	xmm10,XMMWORD[r12]
	vpshufb	xmm10,xmm10,xmm8
	vbroadcasti32x4	zmm12,ZMMWORD[rsi]
	vpshufb	zmm12,zmm12,zmm8

; Load the AES key length in bytes.  BoringSSL stores number of rounds
; minus 1, so convert using: AESKEYLEN = 4 * aeskey->rounds - 20.
	mov	r10d,DWORD[240+r9]
	lea	r10d,[((-20))+r10*4]

; Make RNDKEYLAST_PTR point to the last AES round key.  This is the
; round key with index 10, 12, or 14 for AES-128, AES-192, or AES-256
; respectively.  Then load the zero-th and last round keys.
	lea	r11,[96+r10*4+r9]
	vbroadcasti32x4	zmm13,ZMMWORD[r9]
	vbroadcasti32x4	zmm14,ZMMWORD[r11]

; Finish initializing LE_CTR by adding [0, 1, 2, 3] to its low words.
	vpaddd	zmm12,zmm12,ZMMWORD[$L$ctr_pattern]

; Load 4 into all 128-bit lanes of LE_CTR_INC.
	vbroadcasti32x4	zmm11,ZMMWORD[$L$inc_4blocks]

; If there are at least 256 bytes of data, then continue into the loop
; that processes 256 bytes of data at a time.  Otherwise skip it.
	cmp	r8,256
	jb	NEAR $L$crypt_loop_4x_done__func2

; Load powers of the hash key.
	vmovdqu8	zmm27,ZMMWORD[((256-256))+rdi]
	vmovdqu8	zmm28,ZMMWORD[((256-192))+rdi]
	vmovdqu8	zmm29,ZMMWORD[((256-128))+rdi]
	vmovdqu8	zmm30,ZMMWORD[((256-64))+rdi]
; Cache as many additional AES round keys as possible.
	vbroadcasti32x4	zmm15,ZMMWORD[((-144))+r11]
	vbroadcasti32x4	zmm16,ZMMWORD[((-128))+r11]
	vbroadcasti32x4	zmm17,ZMMWORD[((-112))+r11]
	vbroadcasti32x4	zmm18,ZMMWORD[((-96))+r11]
	vbroadcasti32x4	zmm19,ZMMWORD[((-80))+r11]
	vbroadcasti32x4	zmm20,ZMMWORD[((-64))+r11]
	vbroadcasti32x4	zmm21,ZMMWORD[((-48))+r11]
	vbroadcasti32x4	zmm22,ZMMWORD[((-32))+r11]
	vbroadcasti32x4	zmm23,ZMMWORD[((-16))+r11]

$L$crypt_loop_4x__func2:
	vmovdqu8	zmm4,ZMMWORD[rcx]
	vmovdqu8	zmm5,ZMMWORD[64+rcx]
	vmovdqu8	zmm6,ZMMWORD[128+rcx]
	vmovdqu8	zmm7,ZMMWORD[192+rcx]
; Start the AES encryption of the counter blocks.
; Increment le_ctr four times to generate four vectors of little-endian
; counter blocks, swap each to big-endian, and store them in aesdata[0-3].
	vpshufb	zmm0,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm1,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm2,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpshufb	zmm3,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11

; AES "round zero": XOR in the zero-th round key.
	vpxord	zmm0,zmm0,zmm13
	vpxord	zmm1,zmm1,zmm13
	vpxord	zmm2,zmm2,zmm13
	vpxord	zmm3,zmm3,zmm13

	cmp	r10d,24
	jl	NEAR $L$aes128__func2
	je	NEAR $L$aes192__func2
; AES-256
	vbroadcasti32x4	zmm9,ZMMWORD[((-208))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

	vbroadcasti32x4	zmm9,ZMMWORD[((-192))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

$L$aes192__func2:
	vbroadcasti32x4	zmm9,ZMMWORD[((-176))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

	vbroadcasti32x4	zmm9,ZMMWORD[((-160))+r11]
	vaesenc	zmm0,zmm0,zmm9
	vaesenc	zmm1,zmm1,zmm9
	vaesenc	zmm2,zmm2,zmm9
	vaesenc	zmm3,zmm3,zmm9

$L$aes128__func2:

; Prefetch the source data 512 bytes ahead into the L1 data cache, to
; improve performance when the hardware prefetcher is disabled.  Assumes the
; L1 data cache line size is 64 bytes (de facto standard on x86_64).
	prefetcht0	[((512+0))+rcx]
	prefetcht0	[((512+64))+rcx]
	prefetcht0	[((512+128))+rcx]
	prefetcht0	[((512+192))+rcx]

; Finish the AES encryption of the counter blocks in AESDATA[0-3],
; interleaved with the GHASH update of the ciphertext blocks in
; GHASHDATA[0-3].
	vpshufb	zmm4,zmm4,zmm8
	vpxord	zmm4,zmm4,zmm10
	vpshufb	zmm5,zmm5,zmm8
	vpshufb	zmm6,zmm6,zmm8

	vaesenc	zmm0,zmm0,zmm15
	vaesenc	zmm1,zmm1,zmm15
	vaesenc	zmm2,zmm2,zmm15
	vaesenc	zmm3,zmm3,zmm15

	vpshufb	zmm7,zmm7,zmm8
	vpclmulqdq	zmm10,zmm4,zmm27,0x00  ; LO_0
	vpclmulqdq	zmm24,zmm5,zmm28,0x00  ; LO_1
	vpclmulqdq	zmm25,zmm6,zmm29,0x00  ; LO_2

	vaesenc	zmm0,zmm0,zmm16
	vaesenc	zmm1,zmm1,zmm16
	vaesenc	zmm2,zmm2,zmm16
	vaesenc	zmm3,zmm3,zmm16

	vpxord	zmm10,zmm10,zmm24  ; sum(LO_{1,0})
	vpclmulqdq	zmm26,zmm7,zmm30,0x00  ; LO_3
	vpternlogd	zmm10,zmm25,zmm26,0x96  ; LO = sum(LO_{3,2,1,0})
	vpclmulqdq	zmm24,zmm4,zmm27,0x01  ; MI_0

	vaesenc	zmm0,zmm0,zmm17
	vaesenc	zmm1,zmm1,zmm17
	vaesenc	zmm2,zmm2,zmm17
	vaesenc	zmm3,zmm3,zmm17

	vpclmulqdq	zmm25,zmm5,zmm28,0x01  ; MI_1
	vpclmulqdq	zmm26,zmm6,zmm29,0x01  ; MI_2
	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{2,1,0})
	vpclmulqdq	zmm25,zmm7,zmm30,0x01  ; MI_3

	vaesenc	zmm0,zmm0,zmm18
	vaesenc	zmm1,zmm1,zmm18
	vaesenc	zmm2,zmm2,zmm18
	vaesenc	zmm3,zmm3,zmm18

	vpclmulqdq	zmm26,zmm4,zmm27,0x10  ; MI_4
	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{4,3,2,1,0})
	vpclmulqdq	zmm25,zmm5,zmm28,0x10  ; MI_5
	vpclmulqdq	zmm26,zmm6,zmm29,0x10  ; MI_6

	vaesenc	zmm0,zmm0,zmm19
	vaesenc	zmm1,zmm1,zmm19
	vaesenc	zmm2,zmm2,zmm19
	vaesenc	zmm3,zmm3,zmm19

	vpternlogd	zmm24,zmm25,zmm26,0x96  ; sum(MI_{6,5,4,3,2,1,0})
	vpclmulqdq	zmm26,zmm31,zmm10,0x01  ; LO_L*(x^63 + x^62 + x^57)
	vpclmulqdq	zmm25,zmm7,zmm30,0x10  ; MI_7
	vpxord	zmm24,zmm24,zmm25  ; MI = sum(MI_{7,6,5,4,3,2,1,0})

	vaesenc	zmm0,zmm0,zmm20
	vaesenc	zmm1,zmm1,zmm20
	vaesenc	zmm2,zmm2,zmm20
	vaesenc	zmm3,zmm3,zmm20

	vpshufd	zmm10,zmm10,0x4e  ; Swap halves of LO
	vpclmulqdq	zmm4,zmm4,zmm27,0x11  ; HI_0
	vpclmulqdq	zmm5,zmm5,zmm28,0x11  ; HI_1
	vpclmulqdq	zmm6,zmm6,zmm29,0x11  ; HI_2

	vaesenc	zmm0,zmm0,zmm21
	vaesenc	zmm1,zmm1,zmm21
	vaesenc	zmm2,zmm2,zmm21
	vaesenc	zmm3,zmm3,zmm21

	vpternlogd	zmm24,zmm10,zmm26,0x96  ; Fold LO into MI
	vpclmulqdq	zmm7,zmm7,zmm30,0x11  ; HI_3
	vpternlogd	zmm4,zmm5,zmm6,0x96  ; sum(HI_{2,1,0})
	vpclmulqdq	zmm25,zmm31,zmm24,0x01  ; MI_L*(x^63 + x^62 + x^57)

	vaesenc	zmm0,zmm0,zmm22
	vaesenc	zmm1,zmm1,zmm22
	vaesenc	zmm2,zmm2,zmm22
	vaesenc	zmm3,zmm3,zmm22

	vpxord	zmm10,zmm4,zmm7  ; HI = sum(HI_{3,2,1,0})
	vpshufd	zmm24,zmm24,0x4e  ; Swap halves of MI
	vpternlogd	zmm10,zmm24,zmm25,0x96  ; Fold MI into HI

	vaesenc	zmm0,zmm0,zmm23
	vaesenc	zmm1,zmm1,zmm23
	vaesenc	zmm2,zmm2,zmm23
	vaesenc	zmm3,zmm3,zmm23


	vextracti32x4	xmm4,zmm10,1
	vextracti32x4	xmm5,zmm10,2
	vextracti32x4	xmm6,zmm10,3
	vpxord	xmm10,xmm10,xmm4
	vpternlogd	xmm10,xmm6,xmm5,0x96

	vpxord	zmm4,zmm14,ZMMWORD[rcx]
	vpxord	zmm5,zmm14,ZMMWORD[64+rcx]
	vpxord	zmm6,zmm14,ZMMWORD[128+rcx]
	vpxord	zmm7,zmm14,ZMMWORD[192+rcx]
	vaesenclast	zmm4,zmm0,zmm4
	vaesenclast	zmm5,zmm1,zmm5
	vaesenclast	zmm6,zmm2,zmm6
	vaesenclast	zmm7,zmm3,zmm7
	vmovdqu8	ZMMWORD[rdx],zmm4
	vmovdqu8	ZMMWORD[64+rdx],zmm5
	vmovdqu8	ZMMWORD[128+rdx],zmm6
	vmovdqu8	ZMMWORD[192+rdx],zmm7

	add	rcx,256
	add	rdx,256
	sub	r8,256
	cmp	r8,256
	jae	NEAR $L$crypt_loop_4x__func2
$L$crypt_loop_4x_done__func2:
; Check whether any data remains.
	test	r8,r8
	jz	NEAR $L$done__func2

; The data length isn't a multiple of 256 bytes.  Process the remaining
; data of length 1 <= DATALEN < 256, up to one 64-byte vector at a time.
; Going one vector at a time may seem inefficient compared to having
; separate code paths for each possible number of vectors remaining.
; However, using a loop keeps the code size down, and it performs
; surprising well; modern CPUs will start executing the next iteration
; before the previous one finishes and also predict the number of loop
; iterations.  For a similar reason, we roll up the AES rounds.
; 
; On the last iteration, the remaining length may be less than 64 bytes.
; Handle this using masking.
; 
; Since there are enough key powers available for all remaining data,
; there is no need to do a GHASH reduction after each iteration.
; Instead, multiply each remaining block by its own key power, and only
; do a GHASH reduction at the very end.

; Make POWERS_PTR point to the key powers [H^N, H^(N-1), ...] where N
; is the number of blocks that remain.
	mov	rax,r8
	neg	rax
	and	rax,-16  ; -round_up(DATALEN, 16)
	lea	rsi,[256+rax*1+rdi]
	vpxor	xmm4,xmm4,xmm4
	vpxor	xmm5,xmm5,xmm5
	vpxor	xmm6,xmm6,xmm6

	cmp	r8,64
	jb	NEAR $L$partial_vec__func2

$L$crypt_loop_1x__func2:
; Process a full 64-byte vector.

; Encrypt a vector of counter blocks.
	vpshufb	zmm0,zmm12,zmm8
	vpaddd	zmm12,zmm12,zmm11
	vpxord	zmm0,zmm0,zmm13
	lea	rax,[16+r9]
$L$vaesenc_loop_tail_full_vec__func2:
	vbroadcasti32x4	zmm9,ZMMWORD[rax]
	vaesenc	zmm0,zmm0,zmm9
	add	rax,16
	cmp	r11,rax
	jne	NEAR $L$vaesenc_loop_tail_full_vec__func2
	vaesenclast	zmm0,zmm0,zmm14

; XOR the data with the vector of keystream blocks.
	vmovdqu8	zmm1,ZMMWORD[rcx]
	vpxord	zmm0,zmm0,zmm1
	vmovdqu8	ZMMWORD[rdx],zmm0

; Update GHASH with the ciphertext blocks, without reducing.
	vmovdqu8	zmm30,ZMMWORD[rsi]
	vpshufb	zmm0,zmm1,zmm8
	vpxord	zmm0,zmm0,zmm10
	vpclmulqdq	zmm7,zmm0,zmm30,0x00  ; a_L * b_L
	vpclmulqdq	zmm1,zmm0,zmm30,0x01  ; a_L * b_H
	vpclmulqdq	zmm2,zmm0,zmm30,0x10  ; a_H * b_L
	vpclmulqdq	zmm3,zmm0,zmm30,0x11  ; a_H * b_H
	vpxord	zmm4,zmm4,zmm7
	vpternlogd	zmm5,zmm1,zmm2,0x96
	vpxord	zmm6,zmm6,zmm3

	vpxor	xmm10,xmm10,xmm10

	add	rsi,64
	add	rcx,64
	add	rdx,64
	sub	r8,64
	cmp	r8,64
	jae	NEAR $L$crypt_loop_1x__func2

	test	r8,r8
	jz	NEAR $L$reduce__func2

$L$partial_vec__func2:
; Process a partial vector of length 1 <= DATALEN < 64.

; Set the data mask %k1 to DATALEN 1's.
; Set the key powers mask %k2 to round_up(DATALEN, 16) 1's.
	mov	rax,-1
	bzhi	rax,rax,r8
	kmovq	k1,rax
	add	r8,15
	and	r8,-16
	mov	rax,-1
	bzhi	rax,rax,r8
	kmovq	k2,rax

; Encrypt one last vector of counter blocks.  This does not need to be
; masked.  The counter does not need to be incremented here.
	vpshufb	zmm0,zmm12,zmm8
	vpxord	zmm0,zmm0,zmm13
	lea	rax,[16+r9]
$L$vaesenc_loop_tail_partialvec__func2:
	vbroadcasti32x4	zmm9,ZMMWORD[rax]
	vaesenc	zmm0,zmm0,zmm9
	add	rax,16
	cmp	r11,rax
	jne	NEAR $L$vaesenc_loop_tail_partialvec__func2
	vaesenclast	zmm0,zmm0,zmm14

; XOR the data with the appropriate number of keystream bytes.
	vmovdqu8	zmm1{k1}{z},[rcx]
	vpxord	zmm0,zmm0,zmm1
	vmovdqu8	ZMMWORD[rdx]{k1},zmm0

; Update GHASH with the ciphertext block(s), without reducing.
; 
; In the case of DATALEN < 64, the ciphertext is zero-padded to 64
; bytes.  (If decrypting, it's done by the above masked load.  If
; encrypting, it's done by the below masked register-to-register move.)
; Note that if DATALEN <= 48, there will be additional padding beyond
; the padding of the last block specified by GHASH itself; i.e., there
; may be whole block(s) that get processed by the GHASH multiplication
; and reduction instructions but should not actually be included in the
; GHASH.  However, any such blocks are all-zeroes, and the values that
; they're multiplied with are also all-zeroes.  Therefore they just add
; 0 * 0 = 0 to the final GHASH result, which makes no difference.
	vmovdqu8	zmm30{k2}{z},[rsi]

	vpshufb	zmm0,zmm1,zmm8
	vpxord	zmm0,zmm0,zmm10
	vpclmulqdq	zmm7,zmm0,zmm30,0x00  ; a_L * b_L
	vpclmulqdq	zmm1,zmm0,zmm30,0x01  ; a_L * b_H
	vpclmulqdq	zmm2,zmm0,zmm30,0x10  ; a_H * b_L
	vpclmulqdq	zmm3,zmm0,zmm30,0x11  ; a_H * b_H
	vpxord	zmm4,zmm4,zmm7
	vpternlogd	zmm5,zmm1,zmm2,0x96
	vpxord	zmm6,zmm6,zmm3


$L$reduce__func2:
; Finally, do the GHASH reduction.
	vpclmulqdq	zmm0,zmm31,zmm4,0x01
	vpshufd	zmm4,zmm4,0x4e
	vpternlogd	zmm5,zmm4,zmm0,0x96
	vpclmulqdq	zmm0,zmm31,zmm5,0x01
	vpshufd	zmm5,zmm5,0x4e
	vpternlogd	zmm6,zmm5,zmm0,0x96

	vextracti32x4	xmm0,zmm6,1
	vextracti32x4	xmm1,zmm6,2
	vextracti32x4	xmm2,zmm6,3
	vpxord	xmm10,xmm6,xmm0
	vpternlogd	xmm10,xmm2,xmm1,0x96


$L$done__func2:
; Store the updated GHASH accumulator back to memory.
	vpshufb	xmm10,xmm10,xmm8
	vmovdqu	XMMWORD[r12],xmm10

	vzeroupper  ; This is needed after using ymm or zmm registers.
	vmovdqa	xmm6,XMMWORD[rsp]
	vmovdqa	xmm7,XMMWORD[16+rsp]
	vmovdqa	xmm8,XMMWORD[32+rsp]
	vmovdqa	xmm9,XMMWORD[48+rsp]
	vmovdqa	xmm10,XMMWORD[64+rsp]
	vmovdqa	xmm11,XMMWORD[80+rsp]
	vmovdqa	xmm12,XMMWORD[96+rsp]
	vmovdqa	xmm13,XMMWORD[112+rsp]
	vmovdqa	xmm14,XMMWORD[128+rsp]
	vmovdqa	xmm15,XMMWORD[144+rsp]
	add	rsp,160
	pop	r12
	pop	rdi
	pop	rsi
	ret
$L$SEH_end_aes_gcm_dec_update_vaes_avx512_17:


section	.pdata rdata align=4
ALIGN	4
	DD	$L$SEH_begin_gcm_gmult_vpclmulqdq_avx512_1 wrt ..imagebase
	DD	$L$SEH_end_gcm_gmult_vpclmulqdq_avx512_5 wrt ..imagebase
	DD	$L$SEH_info_gcm_gmult_vpclmulqdq_avx512_0 wrt ..imagebase

	DD	$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1 wrt ..imagebase
	DD	$L$SEH_end_gcm_ghash_vpclmulqdq_avx512_12 wrt ..imagebase
	DD	$L$SEH_info_gcm_ghash_vpclmulqdq_avx512_0 wrt ..imagebase

	DD	$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1 wrt ..imagebase
	DD	$L$SEH_end_aes_gcm_enc_update_vaes_avx512_17 wrt ..imagebase
	DD	$L$SEH_info_aes_gcm_enc_update_vaes_avx512_0 wrt ..imagebase

	DD	$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1 wrt ..imagebase
	DD	$L$SEH_end_aes_gcm_dec_update_vaes_avx512_17 wrt ..imagebase
	DD	$L$SEH_info_aes_gcm_dec_update_vaes_avx512_0 wrt ..imagebase


section	.xdata rdata align=8
ALIGN	4
$L$SEH_info_gcm_gmult_vpclmulqdq_avx512_0:
	DB	1  ; version 1, no flags
	DB	$L$SEH_endprologue_gcm_gmult_vpclmulqdq_avx512_4-$L$SEH_begin_gcm_gmult_vpclmulqdq_avx512_1
	DB	3
	DB	0
	DB	$L$SEH_prologue_gcm_gmult_vpclmulqdq_avx512_3-$L$SEH_begin_gcm_gmult_vpclmulqdq_avx512_1
	DB	104
	DW	0
	DB	$L$SEH_prologue_gcm_gmult_vpclmulqdq_avx512_2-$L$SEH_begin_gcm_gmult_vpclmulqdq_avx512_1
	DB	34

	DW	0
$L$SEH_info_gcm_ghash_vpclmulqdq_avx512_0:
	DB	1  ; version 1, no flags
	DB	$L$SEH_endprologue_gcm_ghash_vpclmulqdq_avx512_11-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	18
	DB	0
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_10-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	216
	DW	7
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_9-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	200
	DW	6
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_8-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	184
	DW	5
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_7-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	168
	DW	4
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_6-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	152
	DW	3
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_5-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	136
	DW	2
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_4-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	120
	DW	1
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_3-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	104
	DW	0
	DB	$L$SEH_prologue_gcm_ghash_vpclmulqdq_avx512_2-$L$SEH_begin_gcm_ghash_vpclmulqdq_avx512_1
	DB	1
	DW	17

$L$SEH_info_aes_gcm_enc_update_vaes_avx512_0:
	DB	1  ; version 1, no flags
	DB	$L$SEH_endprologue_aes_gcm_enc_update_vaes_avx512_16-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	25
	DB	0
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_15-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	248
	DW	9
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_14-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	232
	DW	8
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_13-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	216
	DW	7
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_12-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	200
	DW	6
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_11-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	184
	DW	5
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_10-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	168
	DW	4
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_9-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	152
	DW	3
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_8-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	136
	DW	2
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_7-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	120
	DW	1
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_6-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	104
	DW	0
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_5-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	1
	DW	20
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_4-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	192
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_3-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	112
	DB	$L$SEH_prologue_aes_gcm_enc_update_vaes_avx512_2-$L$SEH_begin_aes_gcm_enc_update_vaes_avx512_1
	DB	96

	DW	0
$L$SEH_info_aes_gcm_dec_update_vaes_avx512_0:
	DB	1  ; version 1, no flags
	DB	$L$SEH_endprologue_aes_gcm_dec_update_vaes_avx512_16-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	25
	DB	0
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_15-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	248
	DW	9
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_14-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	232
	DW	8
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_13-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	216
	DW	7
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_12-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	200
	DW	6
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_11-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	184
	DW	5
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_10-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	168
	DW	4
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_9-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	152
	DW	3
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_8-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	136
	DW	2
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_7-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	120
	DW	1
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_6-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	104
	DW	0
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_5-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	1
	DW	20
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_4-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	192
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_3-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	112
	DB	$L$SEH_prologue_aes_gcm_dec_update_vaes_avx512_2-$L$SEH_begin_aes_gcm_dec_update_vaes_avx512_1
	DB	96

	DW	0
%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
