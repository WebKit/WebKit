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


; #
; #  _aes_encrypt_core
; #
; #  AES-encrypt %xmm0.
; #
; #  Inputs:
; #     %xmm0 = input
; #     %xmm9-%xmm15 as in _vpaes_preheat
; #    (%rdx) = scheduled keys
; #
; #  Output in %xmm0
; #  Clobbers  %xmm1-%xmm5, %r9, %r10, %r11, %rax
; #  Preserves %xmm6 - %xmm8 so you get some local vectors
; #
; #

ALIGN	16
_vpaes_encrypt_core:

	mov	r9,rdx
	mov	r11,16
	mov	eax,DWORD[240+rdx]
	movdqa	xmm1,xmm9
	movdqa	xmm2,XMMWORD[$L$k_ipt]  ; iptlo
	pandn	xmm1,xmm0
	movdqu	xmm5,XMMWORD[r9]  ; round0 key
	psrld	xmm1,4
	pand	xmm0,xmm9
	pshufb	xmm2,xmm0
	movdqa	xmm0,XMMWORD[(($L$k_ipt+16))]  ; ipthi
	pshufb	xmm0,xmm1
	pxor	xmm2,xmm5
	add	r9,16
	pxor	xmm0,xmm2
	lea	r10,[$L$k_mc_backward]
	jmp	NEAR $L$enc_entry

ALIGN	16
$L$enc_loop:
; middle of middle round
	movdqa	xmm4,xmm13  ; 4 : sb1u
	movdqa	xmm0,xmm12  ; 0 : sb1t
	pshufb	xmm4,xmm2  ; 4 = sb1u
	pshufb	xmm0,xmm3  ; 0 = sb1t
	pxor	xmm4,xmm5  ; 4 = sb1u + k
	movdqa	xmm5,xmm15  ; 4 : sb2u
	pxor	xmm0,xmm4  ; 0 = A
	movdqa	xmm1,XMMWORD[((-64))+r10*1+r11]  ; .Lk_mc_forward[]
	pshufb	xmm5,xmm2  ; 4 = sb2u
	movdqa	xmm4,XMMWORD[r10*1+r11]  ; .Lk_mc_backward[]
	movdqa	xmm2,xmm14  ; 2 : sb2t
	pshufb	xmm2,xmm3  ; 2 = sb2t
	movdqa	xmm3,xmm0  ; 3 = A
	pxor	xmm2,xmm5  ; 2 = 2A
	pshufb	xmm0,xmm1  ; 0 = B
	add	r9,16  ; next key
	pxor	xmm0,xmm2  ; 0 = 2A+B
	pshufb	xmm3,xmm4  ; 3 = D
	add	r11,16  ; next mc
	pxor	xmm3,xmm0  ; 3 = 2A+B+D
	pshufb	xmm0,xmm1  ; 0 = 2B+C
	and	r11,0x30  ; ... mod 4
	sub	rax,1  ; nr--
	pxor	xmm0,xmm3  ; 0 = 2A+3B+C+D

$L$enc_entry:
; top of round
	movdqa	xmm1,xmm9  ; 1 : i
	movdqa	xmm5,xmm11  ; 2 : a/k
	pandn	xmm1,xmm0  ; 1 = i<<4
	psrld	xmm1,4  ; 1 = i
	pand	xmm0,xmm9  ; 0 = k
	pshufb	xmm5,xmm0  ; 2 = a/k
	movdqa	xmm3,xmm10  ; 3 : 1/i
	pxor	xmm0,xmm1  ; 0 = j
	pshufb	xmm3,xmm1  ; 3 = 1/i
	movdqa	xmm4,xmm10  ; 4 : 1/j
	pxor	xmm3,xmm5  ; 3 = iak = 1/i + a/k
	pshufb	xmm4,xmm0  ; 4 = 1/j
	movdqa	xmm2,xmm10  ; 2 : 1/iak
	pxor	xmm4,xmm5  ; 4 = jak = 1/j + a/k
	pshufb	xmm2,xmm3  ; 2 = 1/iak
	movdqa	xmm3,xmm10  ; 3 : 1/jak
	pxor	xmm2,xmm0  ; 2 = io
	pshufb	xmm3,xmm4  ; 3 = 1/jak
	movdqu	xmm5,XMMWORD[r9]
	pxor	xmm3,xmm1  ; 3 = jo
	jnz	NEAR $L$enc_loop

; middle of last round
	movdqa	xmm4,XMMWORD[((-96))+r10]  ; 3 : sbou	.Lk_sbo
	movdqa	xmm0,XMMWORD[((-80))+r10]  ; 0 : sbot	.Lk_sbo+16
	pshufb	xmm4,xmm2  ; 4 = sbou
	pxor	xmm4,xmm5  ; 4 = sb1u + k
	pshufb	xmm0,xmm3  ; 0 = sb1t
	movdqa	xmm1,XMMWORD[64+r10*1+r11]  ; .Lk_sr[]
	pxor	xmm0,xmm4  ; 0 = A
	pshufb	xmm0,xmm1
	ret



; #
; #  _aes_encrypt_core_2x
; #
; #  AES-encrypt %xmm0 and %xmm6 in parallel.
; #
; #  Inputs:
; #     %xmm0 and %xmm6 = input
; #     %xmm9 and %xmm10 as in _vpaes_preheat
; #    (%rdx) = scheduled keys
; #
; #  Output in %xmm0 and %xmm6
; #  Clobbers  %xmm1-%xmm5, %xmm7, %xmm8, %xmm11-%xmm13, %r9, %r10, %r11, %rax
; #  Preserves %xmm14 and %xmm15
; #
; #  This function stitches two parallel instances of _vpaes_encrypt_core. x86_64
; #  provides 16 XMM registers. _vpaes_encrypt_core computes over six registers
; #  (%xmm0-%xmm5) and additionally uses seven registers with preloaded constants
; #  from _vpaes_preheat (%xmm9-%xmm15). This does not quite fit two instances,
; #  so we spill some of %xmm9 through %xmm15 back to memory. We keep %xmm9 and
; #  %xmm10 in registers as these values are used several times in a row. The
; #  remainder are read once per round and are spilled to memory. This leaves two
; #  registers preserved for the caller.
; #
; #  Thus, of the two _vpaes_encrypt_core instances, the first uses (%xmm0-%xmm5)
; #  as before. The second uses %xmm6-%xmm8,%xmm11-%xmm13. (Add 6 to %xmm2 and
; #  below. Add 8 to %xmm3 and up.) Instructions in the second instance are
; #  indented by one space.
; #
; #

ALIGN	16
_vpaes_encrypt_core_2x:

	mov	r9,rdx
	mov	r11,16
	mov	eax,DWORD[240+rdx]
	movdqa	xmm1,xmm9
	movdqa	xmm7,xmm9
	movdqa	xmm2,XMMWORD[$L$k_ipt]  ; iptlo
	movdqa	xmm8,xmm2
	pandn	xmm1,xmm0
	pandn	xmm7,xmm6
	movdqu	xmm5,XMMWORD[r9]  ; round0 key
; Also use %xmm5 in the second instance.
	psrld	xmm1,4
	psrld	xmm7,4
	pand	xmm0,xmm9
	pand	xmm6,xmm9
	pshufb	xmm2,xmm0
	pshufb	xmm8,xmm6
	movdqa	xmm0,XMMWORD[(($L$k_ipt+16))]  ; ipthi
	movdqa	xmm6,xmm0
	pshufb	xmm0,xmm1
	pshufb	xmm6,xmm7
	pxor	xmm2,xmm5
	pxor	xmm8,xmm5
	add	r9,16
	pxor	xmm0,xmm2
	pxor	xmm6,xmm8
	lea	r10,[$L$k_mc_backward]
	jmp	NEAR $L$enc2x_entry

ALIGN	16
$L$enc2x_loop:
; middle of middle round
	movdqa	xmm4,XMMWORD[$L$k_sb1]  ; 4 : sb1u
	movdqa	xmm0,XMMWORD[(($L$k_sb1+16))]  ; 0 : sb1t
	movdqa	xmm12,xmm4
	movdqa	xmm6,xmm0
	pshufb	xmm4,xmm2  ; 4 = sb1u
	pshufb	xmm12,xmm8
	pshufb	xmm0,xmm3  ; 0 = sb1t
	pshufb	xmm6,xmm11
	pxor	xmm4,xmm5  ; 4 = sb1u + k
	pxor	xmm12,xmm5
	movdqa	xmm5,XMMWORD[$L$k_sb2]  ; 4 : sb2u
	movdqa	xmm13,xmm5
	pxor	xmm0,xmm4  ; 0 = A
	pxor	xmm6,xmm12
	movdqa	xmm1,XMMWORD[((-64))+r10*1+r11]  ; .Lk_mc_forward[]
; Also use %xmm1 in the second instance.
	pshufb	xmm5,xmm2  ; 4 = sb2u
	pshufb	xmm13,xmm8
	movdqa	xmm4,XMMWORD[r10*1+r11]  ; .Lk_mc_backward[]
; Also use %xmm4 in the second instance.
	movdqa	xmm2,XMMWORD[(($L$k_sb2+16))]  ; 2 : sb2t
	movdqa	xmm8,xmm2
	pshufb	xmm2,xmm3  ; 2 = sb2t
	pshufb	xmm8,xmm11
	movdqa	xmm3,xmm0  ; 3 = A
	movdqa	xmm11,xmm6
	pxor	xmm2,xmm5  ; 2 = 2A
	pxor	xmm8,xmm13
	pshufb	xmm0,xmm1  ; 0 = B
	pshufb	xmm6,xmm1
	add	r9,16  ; next key
	pxor	xmm0,xmm2  ; 0 = 2A+B
	pxor	xmm6,xmm8
	pshufb	xmm3,xmm4  ; 3 = D
	pshufb	xmm11,xmm4
	add	r11,16  ; next mc
	pxor	xmm3,xmm0  ; 3 = 2A+B+D
	pxor	xmm11,xmm6
	pshufb	xmm0,xmm1  ; 0 = 2B+C
	pshufb	xmm6,xmm1
	and	r11,0x30  ; ... mod 4
	sub	rax,1  ; nr--
	pxor	xmm0,xmm3  ; 0 = 2A+3B+C+D
	pxor	xmm6,xmm11

$L$enc2x_entry:
; top of round
	movdqa	xmm1,xmm9  ; 1 : i
	movdqa	xmm7,xmm9
	movdqa	xmm5,XMMWORD[(($L$k_inv+16))]  ; 2 : a/k
	movdqa	xmm13,xmm5
	pandn	xmm1,xmm0  ; 1 = i<<4
	pandn	xmm7,xmm6
	psrld	xmm1,4  ; 1 = i
	psrld	xmm7,4
	pand	xmm0,xmm9  ; 0 = k
	pand	xmm6,xmm9
	pshufb	xmm5,xmm0  ; 2 = a/k
	pshufb	xmm13,xmm6
	movdqa	xmm3,xmm10  ; 3 : 1/i
	movdqa	xmm11,xmm10
	pxor	xmm0,xmm1  ; 0 = j
	pxor	xmm6,xmm7
	pshufb	xmm3,xmm1  ; 3 = 1/i
	pshufb	xmm11,xmm7
	movdqa	xmm4,xmm10  ; 4 : 1/j
	movdqa	xmm12,xmm10
	pxor	xmm3,xmm5  ; 3 = iak = 1/i + a/k
	pxor	xmm11,xmm13
	pshufb	xmm4,xmm0  ; 4 = 1/j
	pshufb	xmm12,xmm6
	movdqa	xmm2,xmm10  ; 2 : 1/iak
	movdqa	xmm8,xmm10
	pxor	xmm4,xmm5  ; 4 = jak = 1/j + a/k
	pxor	xmm12,xmm13
	pshufb	xmm2,xmm3  ; 2 = 1/iak
	pshufb	xmm8,xmm11
	movdqa	xmm3,xmm10  ; 3 : 1/jak
	movdqa	xmm11,xmm10
	pxor	xmm2,xmm0  ; 2 = io
	pxor	xmm8,xmm6
	pshufb	xmm3,xmm4  ; 3 = 1/jak
	pshufb	xmm11,xmm12
	movdqu	xmm5,XMMWORD[r9]
; Also use %xmm5 in the second instance.
	pxor	xmm3,xmm1  ; 3 = jo
	pxor	xmm11,xmm7
	jnz	NEAR $L$enc2x_loop

; middle of last round
	movdqa	xmm4,XMMWORD[((-96))+r10]  ; 3 : sbou	.Lk_sbo
	movdqa	xmm0,XMMWORD[((-80))+r10]  ; 0 : sbot	.Lk_sbo+16
	movdqa	xmm12,xmm4
	movdqa	xmm6,xmm0
	pshufb	xmm4,xmm2  ; 4 = sbou
	pshufb	xmm12,xmm8
	pxor	xmm4,xmm5  ; 4 = sb1u + k
	pxor	xmm12,xmm5
	pshufb	xmm0,xmm3  ; 0 = sb1t
	pshufb	xmm6,xmm11
	movdqa	xmm1,XMMWORD[64+r10*1+r11]  ; .Lk_sr[]
; Also use %xmm1 in the second instance.
	pxor	xmm0,xmm4  ; 0 = A
	pxor	xmm6,xmm12
	pshufb	xmm0,xmm1
	pshufb	xmm6,xmm1
	ret



; #
; #  Decryption core
; #
; #  Same API as encryption core.
; #

ALIGN	16
_vpaes_decrypt_core:

	mov	r9,rdx  ; load key
	mov	eax,DWORD[240+rdx]
	movdqa	xmm1,xmm9
	movdqa	xmm2,XMMWORD[$L$k_dipt]  ; iptlo
	pandn	xmm1,xmm0
	mov	r11,rax
	psrld	xmm1,4
	movdqu	xmm5,XMMWORD[r9]  ; round0 key
	shl	r11,4
	pand	xmm0,xmm9
	pshufb	xmm2,xmm0
	movdqa	xmm0,XMMWORD[(($L$k_dipt+16))]  ; ipthi
	xor	r11,0x30
	lea	r10,[$L$k_dsbd]
	pshufb	xmm0,xmm1
	and	r11,0x30
	pxor	xmm2,xmm5
	movdqa	xmm5,XMMWORD[(($L$k_mc_forward+48))]
	pxor	xmm0,xmm2
	add	r9,16
	add	r11,r10
	jmp	NEAR $L$dec_entry

ALIGN	16
$L$dec_loop:
; #
; #  Inverse mix columns
; #
	movdqa	xmm4,XMMWORD[((-32))+r10]  ; 4 : sb9u
	movdqa	xmm1,XMMWORD[((-16))+r10]  ; 0 : sb9t
	pshufb	xmm4,xmm2  ; 4 = sb9u
	pshufb	xmm1,xmm3  ; 0 = sb9t
	pxor	xmm0,xmm4
	movdqa	xmm4,XMMWORD[r10]  ; 4 : sbdu
	pxor	xmm0,xmm1  ; 0 = ch
	movdqa	xmm1,XMMWORD[16+r10]  ; 0 : sbdt

	pshufb	xmm4,xmm2  ; 4 = sbdu
	pshufb	xmm0,xmm5  ; MC ch
	pshufb	xmm1,xmm3  ; 0 = sbdt
	pxor	xmm0,xmm4  ; 4 = ch
	movdqa	xmm4,XMMWORD[32+r10]  ; 4 : sbbu
	pxor	xmm0,xmm1  ; 0 = ch
	movdqa	xmm1,XMMWORD[48+r10]  ; 0 : sbbt

	pshufb	xmm4,xmm2  ; 4 = sbbu
	pshufb	xmm0,xmm5  ; MC ch
	pshufb	xmm1,xmm3  ; 0 = sbbt
	pxor	xmm0,xmm4  ; 4 = ch
	movdqa	xmm4,XMMWORD[64+r10]  ; 4 : sbeu
	pxor	xmm0,xmm1  ; 0 = ch
	movdqa	xmm1,XMMWORD[80+r10]  ; 0 : sbet

	pshufb	xmm4,xmm2  ; 4 = sbeu
	pshufb	xmm0,xmm5  ; MC ch
	pshufb	xmm1,xmm3  ; 0 = sbet
	pxor	xmm0,xmm4  ; 4 = ch
	add	r9,16  ; next round key
	palignr	xmm5,xmm5,12
	pxor	xmm0,xmm1  ; 0 = ch
	sub	rax,1  ; nr--

$L$dec_entry:
; top of round
	movdqa	xmm1,xmm9  ; 1 : i
	pandn	xmm1,xmm0  ; 1 = i<<4
	movdqa	xmm2,xmm11  ; 2 : a/k
	psrld	xmm1,4  ; 1 = i
	pand	xmm0,xmm9  ; 0 = k
	pshufb	xmm2,xmm0  ; 2 = a/k
	movdqa	xmm3,xmm10  ; 3 : 1/i
	pxor	xmm0,xmm1  ; 0 = j
	pshufb	xmm3,xmm1  ; 3 = 1/i
	movdqa	xmm4,xmm10  ; 4 : 1/j
	pxor	xmm3,xmm2  ; 3 = iak = 1/i + a/k
	pshufb	xmm4,xmm0  ; 4 = 1/j
	pxor	xmm4,xmm2  ; 4 = jak = 1/j + a/k
	movdqa	xmm2,xmm10  ; 2 : 1/iak
	pshufb	xmm2,xmm3  ; 2 = 1/iak
	movdqa	xmm3,xmm10  ; 3 : 1/jak
	pxor	xmm2,xmm0  ; 2 = io
	pshufb	xmm3,xmm4  ; 3 = 1/jak
	movdqu	xmm0,XMMWORD[r9]
	pxor	xmm3,xmm1  ; 3 = jo
	jnz	NEAR $L$dec_loop

; middle of last round
	movdqa	xmm4,XMMWORD[96+r10]  ; 3 : sbou
	pshufb	xmm4,xmm2  ; 4 = sbou
	pxor	xmm4,xmm0  ; 4 = sb1u + k
	movdqa	xmm0,XMMWORD[112+r10]  ; 0 : sbot
	movdqa	xmm2,XMMWORD[((-352))+r11]  ; .Lk_sr-.Lk_dsbd=-0x160
	pshufb	xmm0,xmm3  ; 0 = sb1t
	pxor	xmm0,xmm4  ; 0 = A
	pshufb	xmm0,xmm2
	ret



; #######################################################
; #                                                    ##
; #                  AES key schedule                  ##
; #                                                    ##
; #######################################################

ALIGN	16
_vpaes_schedule_core:

; rdi = key
; rsi = size in bits
; rdx = buffer
; rcx = direction.  0=encrypt, 1=decrypt

	call	_vpaes_preheat  ; load the tables
	movdqa	xmm8,XMMWORD[$L$k_rcon]  ; load rcon
	movdqu	xmm0,XMMWORD[rdi]  ; load key (unaligned)

; input transform
	movdqa	xmm3,xmm0
	lea	r11,[$L$k_ipt]
	call	_vpaes_schedule_transform
	movdqa	xmm7,xmm0

	lea	r10,[$L$k_sr]
	test	rcx,rcx
	jnz	NEAR $L$schedule_am_decrypting

; encrypting, output zeroth round key after transform
	movdqu	XMMWORD[rdx],xmm0
	jmp	NEAR $L$schedule_go

$L$schedule_am_decrypting:
; decrypting, output zeroth round key after shiftrows
	movdqa	xmm1,XMMWORD[r10*1+r8]
	pshufb	xmm3,xmm1
	movdqu	XMMWORD[rdx],xmm3
	xor	r8,0x30

$L$schedule_go:
	cmp	esi,192
	ja	NEAR $L$schedule_256
	je	NEAR $L$schedule_192
; 128: fall though

; #
; #  .schedule_128
; #
; #  128-bit specific part of key schedule.
; #
; #  This schedule is really simple, because all its parts
; #  are accomplished by the subroutines.
; #
$L$schedule_128:
	mov	esi,10

$L$oop_schedule_128:
	call	_vpaes_schedule_round
	dec	rsi
	jz	NEAR $L$schedule_mangle_last
	call	_vpaes_schedule_mangle  ; write output
	jmp	NEAR $L$oop_schedule_128

; #
; #  .aes_schedule_192
; #
; #  192-bit specific part of key schedule.
; #
; #  The main body of this schedule is the same as the 128-bit
; #  schedule, but with more smearing.  The long, high side is
; #  stored in %xmm7 as before, and the short, low side is in
; #  the high bits of %xmm6.
; #
; #  This schedule is somewhat nastier, however, because each
; #  round produces 192 bits of key material, or 1.5 round keys.
; #  Therefore, on each cycle we do 2 rounds and produce 3 round
; #  keys.
; #
ALIGN	16
$L$schedule_192:
	movdqu	xmm0,XMMWORD[8+rdi]  ; load key part 2 (very unaligned)
	call	_vpaes_schedule_transform  ; input transform
	movdqa	xmm6,xmm0  ; save short part
	pxor	xmm4,xmm4  ; clear 4
	movhlps	xmm6,xmm4  ; clobber low side with zeros
	mov	esi,4

$L$oop_schedule_192:
	call	_vpaes_schedule_round
	palignr	xmm0,xmm6,8
	call	_vpaes_schedule_mangle  ; save key n
	call	_vpaes_schedule_192_smear
	call	_vpaes_schedule_mangle  ; save key n+1
	call	_vpaes_schedule_round
	dec	rsi
	jz	NEAR $L$schedule_mangle_last
	call	_vpaes_schedule_mangle  ; save key n+2
	call	_vpaes_schedule_192_smear
	jmp	NEAR $L$oop_schedule_192

; #
; #  .aes_schedule_256
; #
; #  256-bit specific part of key schedule.
; #
; #  The structure here is very similar to the 128-bit
; #  schedule, but with an additional "low side" in
; #  %xmm6.  The low side's rounds are the same as the
; #  high side's, except no rcon and no rotation.
; #
ALIGN	16
$L$schedule_256:
	movdqu	xmm0,XMMWORD[16+rdi]  ; load key part 2 (unaligned)
	call	_vpaes_schedule_transform  ; input transform
	mov	esi,7

$L$oop_schedule_256:
	call	_vpaes_schedule_mangle  ; output low result
	movdqa	xmm6,xmm0  ; save cur_lo in xmm6

; high round
	call	_vpaes_schedule_round
	dec	rsi
	jz	NEAR $L$schedule_mangle_last
	call	_vpaes_schedule_mangle

; low round. swap xmm7 and xmm6
	pshufd	xmm0,xmm0,0xFF
	movdqa	xmm5,xmm7
	movdqa	xmm7,xmm6
	call	_vpaes_schedule_low_round
	movdqa	xmm7,xmm5

	jmp	NEAR $L$oop_schedule_256


; #
; #  .aes_schedule_mangle_last
; #
; #  Mangler for last round of key schedule
; #  Mangles %xmm0
; #    when encrypting, outputs out(%xmm0) ^ 63
; #    when decrypting, outputs unskew(%xmm0)
; #
; #  Always called right before return... jumps to cleanup and exits
; #
ALIGN	16
$L$schedule_mangle_last:
; schedule last round key from xmm0
	lea	r11,[$L$k_deskew]  ; prepare to deskew
	test	rcx,rcx
	jnz	NEAR $L$schedule_mangle_last_dec

; encrypting
	movdqa	xmm1,XMMWORD[r10*1+r8]
	pshufb	xmm0,xmm1  ; output permute
	lea	r11,[$L$k_opt]  ; prepare to output transform
	add	rdx,32

$L$schedule_mangle_last_dec:
	add	rdx,-16
	pxor	xmm0,XMMWORD[$L$k_s63]
	call	_vpaes_schedule_transform  ; output transform
	movdqu	XMMWORD[rdx],xmm0  ; save last key

; cleanup
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	ret



; #
; #  .aes_schedule_192_smear
; #
; #  Smear the short, low side in the 192-bit key schedule.
; #
; #  Inputs:
; #    %xmm7: high side, b  a  x  y
; #    %xmm6:  low side, d  c  0  0
; #    %xmm13: 0
; #
; #  Outputs:
; #    %xmm6: b+c+d  b+c  0  0
; #    %xmm0: b+c+d  b+c  b  a
; #

ALIGN	16
_vpaes_schedule_192_smear:

	pshufd	xmm1,xmm6,0x80  ; d c 0 0 -> c 0 0 0
	pshufd	xmm0,xmm7,0xFE  ; b a _ _ -> b b b a
	pxor	xmm6,xmm1  ; -> c+d c 0 0
	pxor	xmm1,xmm1
	pxor	xmm6,xmm0  ; -> b+c+d b+c b a
	movdqa	xmm0,xmm6
	movhlps	xmm6,xmm1  ; clobber low side with zeros
	ret



; #
; #  .aes_schedule_round
; #
; #  Runs one main round of the key schedule on %xmm0, %xmm7
; #
; #  Specifically, runs subbytes on the high dword of %xmm0
; #  then rotates it by one byte and xors into the low dword of
; #  %xmm7.
; #
; #  Adds rcon from low byte of %xmm8, then rotates %xmm8 for
; #  next rcon.
; #
; #  Smears the dwords of %xmm7 by xoring the low into the
; #  second low, result into third, result into highest.
; #
; #  Returns results in %xmm7 = %xmm0.
; #  Clobbers %xmm1-%xmm4, %r11.
; #

ALIGN	16
_vpaes_schedule_round:

; extract rcon from xmm8
	pxor	xmm1,xmm1
	palignr	xmm1,xmm8,15
	palignr	xmm8,xmm8,15
	pxor	xmm7,xmm1

; rotate
	pshufd	xmm0,xmm0,0xFF
	palignr	xmm0,xmm0,1

; fall through...

; low round: same as high round, but no rotation and no rcon.
_vpaes_schedule_low_round:
; smear xmm7
	movdqa	xmm1,xmm7
	pslldq	xmm7,4
	pxor	xmm7,xmm1
	movdqa	xmm1,xmm7
	pslldq	xmm7,8
	pxor	xmm7,xmm1
	pxor	xmm7,XMMWORD[$L$k_s63]

; subbytes
	movdqa	xmm1,xmm9
	pandn	xmm1,xmm0
	psrld	xmm1,4  ; 1 = i
	pand	xmm0,xmm9  ; 0 = k
	movdqa	xmm2,xmm11  ; 2 : a/k
	pshufb	xmm2,xmm0  ; 2 = a/k
	pxor	xmm0,xmm1  ; 0 = j
	movdqa	xmm3,xmm10  ; 3 : 1/i
	pshufb	xmm3,xmm1  ; 3 = 1/i
	pxor	xmm3,xmm2  ; 3 = iak = 1/i + a/k
	movdqa	xmm4,xmm10  ; 4 : 1/j
	pshufb	xmm4,xmm0  ; 4 = 1/j
	pxor	xmm4,xmm2  ; 4 = jak = 1/j + a/k
	movdqa	xmm2,xmm10  ; 2 : 1/iak
	pshufb	xmm2,xmm3  ; 2 = 1/iak
	pxor	xmm2,xmm0  ; 2 = io
	movdqa	xmm3,xmm10  ; 3 : 1/jak
	pshufb	xmm3,xmm4  ; 3 = 1/jak
	pxor	xmm3,xmm1  ; 3 = jo
	movdqa	xmm4,xmm13  ; 4 : sbou
	pshufb	xmm4,xmm2  ; 4 = sbou
	movdqa	xmm0,xmm12  ; 0 : sbot
	pshufb	xmm0,xmm3  ; 0 = sb1t
	pxor	xmm0,xmm4  ; 0 = sbox output

; add in smeared stuff
	pxor	xmm0,xmm7
	movdqa	xmm7,xmm0
	ret



; #
; #  .aes_schedule_transform
; #
; #  Linear-transform %xmm0 according to tables at (%r11)
; #
; #  Requires that %xmm9 = 0x0F0F... as in preheat
; #  Output in %xmm0
; #  Clobbers %xmm1, %xmm2
; #

ALIGN	16
_vpaes_schedule_transform:

	movdqa	xmm1,xmm9
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm9
	movdqa	xmm2,XMMWORD[r11]  ; lo
	pshufb	xmm2,xmm0
	movdqa	xmm0,XMMWORD[16+r11]  ; hi
	pshufb	xmm0,xmm1
	pxor	xmm0,xmm2
	ret



; #
; #  .aes_schedule_mangle
; #
; #  Mangle xmm0 from (basis-transformed) standard version
; #  to our version.
; #
; #  On encrypt,
; #    xor with 0x63
; #    multiply by circulant 0,1,1,1
; #    apply shiftrows transform
; #
; #  On decrypt,
; #    xor with 0x63
; #    multiply by "inverse mixcolumns" circulant E,B,D,9
; #    deskew
; #    apply shiftrows transform
; #
; #
; #  Writes out to (%rdx), and increments or decrements it
; #  Keeps track of round number mod 4 in %r8
; #  Preserves xmm0
; #  Clobbers xmm1-xmm5
; #

ALIGN	16
_vpaes_schedule_mangle:

	movdqa	xmm4,xmm0  ; save xmm0 for later
	movdqa	xmm5,XMMWORD[$L$k_mc_forward]
	test	rcx,rcx
	jnz	NEAR $L$schedule_mangle_dec

; encrypting
	add	rdx,16
	pxor	xmm4,XMMWORD[$L$k_s63]
	pshufb	xmm4,xmm5
	movdqa	xmm3,xmm4
	pshufb	xmm4,xmm5
	pxor	xmm3,xmm4
	pshufb	xmm4,xmm5
	pxor	xmm3,xmm4

	jmp	NEAR $L$schedule_mangle_both
ALIGN	16
$L$schedule_mangle_dec:
; inverse mix columns
	lea	r11,[$L$k_dksd]
	movdqa	xmm1,xmm9
	pandn	xmm1,xmm4
	psrld	xmm1,4  ; 1 = hi
	pand	xmm4,xmm9  ; 4 = lo

	movdqa	xmm2,XMMWORD[r11]
	pshufb	xmm2,xmm4
	movdqa	xmm3,XMMWORD[16+r11]
	pshufb	xmm3,xmm1
	pxor	xmm3,xmm2
	pshufb	xmm3,xmm5

	movdqa	xmm2,XMMWORD[32+r11]
	pshufb	xmm2,xmm4
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD[48+r11]
	pshufb	xmm3,xmm1
	pxor	xmm3,xmm2
	pshufb	xmm3,xmm5

	movdqa	xmm2,XMMWORD[64+r11]
	pshufb	xmm2,xmm4
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD[80+r11]
	pshufb	xmm3,xmm1
	pxor	xmm3,xmm2
	pshufb	xmm3,xmm5

	movdqa	xmm2,XMMWORD[96+r11]
	pshufb	xmm2,xmm4
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD[112+r11]
	pshufb	xmm3,xmm1
	pxor	xmm3,xmm2

	add	rdx,-16

$L$schedule_mangle_both:
	movdqa	xmm1,XMMWORD[r10*1+r8]
	pshufb	xmm3,xmm1
	add	r8,-16
	and	r8,0x30
	movdqu	XMMWORD[rdx],xmm3
	ret



; 
; Interface to OpenSSL
; 
global	vpaes_set_encrypt_key

ALIGN	16
vpaes_set_encrypt_key:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_set_encrypt_key:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
%ifdef BORINGSSL_DISPATCH_TEST
EXTERN	BORINGSSL_function_hit
	mov	BYTE[((BORINGSSL_function_hit+5))],1
%endif

	lea	rsp,[((-184))+rsp]
	movaps	XMMWORD[16+rsp],xmm6
	movaps	XMMWORD[32+rsp],xmm7
	movaps	XMMWORD[48+rsp],xmm8
	movaps	XMMWORD[64+rsp],xmm9
	movaps	XMMWORD[80+rsp],xmm10
	movaps	XMMWORD[96+rsp],xmm11
	movaps	XMMWORD[112+rsp],xmm12
	movaps	XMMWORD[128+rsp],xmm13
	movaps	XMMWORD[144+rsp],xmm14
	movaps	XMMWORD[160+rsp],xmm15
$L$enc_key_body:
	mov	eax,esi
	shr	eax,5
	add	eax,5
	mov	DWORD[240+rdx],eax  ; AES_KEY->rounds = nbits/32+5;

	mov	ecx,0
	mov	r8d,0x30
	call	_vpaes_schedule_core
	movaps	xmm6,XMMWORD[16+rsp]
	movaps	xmm7,XMMWORD[32+rsp]
	movaps	xmm8,XMMWORD[48+rsp]
	movaps	xmm9,XMMWORD[64+rsp]
	movaps	xmm10,XMMWORD[80+rsp]
	movaps	xmm11,XMMWORD[96+rsp]
	movaps	xmm12,XMMWORD[112+rsp]
	movaps	xmm13,XMMWORD[128+rsp]
	movaps	xmm14,XMMWORD[144+rsp]
	movaps	xmm15,XMMWORD[160+rsp]
	lea	rsp,[184+rsp]
$L$enc_key_epilogue:
	xor	eax,eax
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_vpaes_set_encrypt_key:

global	vpaes_set_decrypt_key

ALIGN	16
vpaes_set_decrypt_key:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_set_decrypt_key:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
	lea	rsp,[((-184))+rsp]
	movaps	XMMWORD[16+rsp],xmm6
	movaps	XMMWORD[32+rsp],xmm7
	movaps	XMMWORD[48+rsp],xmm8
	movaps	XMMWORD[64+rsp],xmm9
	movaps	XMMWORD[80+rsp],xmm10
	movaps	XMMWORD[96+rsp],xmm11
	movaps	XMMWORD[112+rsp],xmm12
	movaps	XMMWORD[128+rsp],xmm13
	movaps	XMMWORD[144+rsp],xmm14
	movaps	XMMWORD[160+rsp],xmm15
$L$dec_key_body:
	mov	eax,esi
	shr	eax,5
	add	eax,5
	mov	DWORD[240+rdx],eax  ; AES_KEY->rounds = nbits/32+5;
	shl	eax,4
	lea	rdx,[16+rax*1+rdx]

	mov	ecx,1
	mov	r8d,esi
	shr	r8d,1
	and	r8d,32
	xor	r8d,32  ; nbits==192?0:32
	call	_vpaes_schedule_core
	movaps	xmm6,XMMWORD[16+rsp]
	movaps	xmm7,XMMWORD[32+rsp]
	movaps	xmm8,XMMWORD[48+rsp]
	movaps	xmm9,XMMWORD[64+rsp]
	movaps	xmm10,XMMWORD[80+rsp]
	movaps	xmm11,XMMWORD[96+rsp]
	movaps	xmm12,XMMWORD[112+rsp]
	movaps	xmm13,XMMWORD[128+rsp]
	movaps	xmm14,XMMWORD[144+rsp]
	movaps	xmm15,XMMWORD[160+rsp]
	lea	rsp,[184+rsp]
$L$dec_key_epilogue:
	xor	eax,eax
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_vpaes_set_decrypt_key:

global	vpaes_encrypt

ALIGN	16
vpaes_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
%ifdef BORINGSSL_DISPATCH_TEST
EXTERN	BORINGSSL_function_hit
	mov	BYTE[((BORINGSSL_function_hit+4))],1
%endif
	lea	rsp,[((-184))+rsp]
	movaps	XMMWORD[16+rsp],xmm6
	movaps	XMMWORD[32+rsp],xmm7
	movaps	XMMWORD[48+rsp],xmm8
	movaps	XMMWORD[64+rsp],xmm9
	movaps	XMMWORD[80+rsp],xmm10
	movaps	XMMWORD[96+rsp],xmm11
	movaps	XMMWORD[112+rsp],xmm12
	movaps	XMMWORD[128+rsp],xmm13
	movaps	XMMWORD[144+rsp],xmm14
	movaps	XMMWORD[160+rsp],xmm15
$L$enc_body:
	movdqu	xmm0,XMMWORD[rdi]
	call	_vpaes_preheat
	call	_vpaes_encrypt_core
	movdqu	XMMWORD[rsi],xmm0
	movaps	xmm6,XMMWORD[16+rsp]
	movaps	xmm7,XMMWORD[32+rsp]
	movaps	xmm8,XMMWORD[48+rsp]
	movaps	xmm9,XMMWORD[64+rsp]
	movaps	xmm10,XMMWORD[80+rsp]
	movaps	xmm11,XMMWORD[96+rsp]
	movaps	xmm12,XMMWORD[112+rsp]
	movaps	xmm13,XMMWORD[128+rsp]
	movaps	xmm14,XMMWORD[144+rsp]
	movaps	xmm15,XMMWORD[160+rsp]
	lea	rsp,[184+rsp]
$L$enc_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_vpaes_encrypt:

global	vpaes_decrypt

ALIGN	16
vpaes_decrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_decrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
	lea	rsp,[((-184))+rsp]
	movaps	XMMWORD[16+rsp],xmm6
	movaps	XMMWORD[32+rsp],xmm7
	movaps	XMMWORD[48+rsp],xmm8
	movaps	XMMWORD[64+rsp],xmm9
	movaps	XMMWORD[80+rsp],xmm10
	movaps	XMMWORD[96+rsp],xmm11
	movaps	XMMWORD[112+rsp],xmm12
	movaps	XMMWORD[128+rsp],xmm13
	movaps	XMMWORD[144+rsp],xmm14
	movaps	XMMWORD[160+rsp],xmm15
$L$dec_body:
	movdqu	xmm0,XMMWORD[rdi]
	call	_vpaes_preheat
	call	_vpaes_decrypt_core
	movdqu	XMMWORD[rsi],xmm0
	movaps	xmm6,XMMWORD[16+rsp]
	movaps	xmm7,XMMWORD[32+rsp]
	movaps	xmm8,XMMWORD[48+rsp]
	movaps	xmm9,XMMWORD[64+rsp]
	movaps	xmm10,XMMWORD[80+rsp]
	movaps	xmm11,XMMWORD[96+rsp]
	movaps	xmm12,XMMWORD[112+rsp]
	movaps	xmm13,XMMWORD[128+rsp]
	movaps	xmm14,XMMWORD[144+rsp]
	movaps	xmm15,XMMWORD[160+rsp]
	lea	rsp,[184+rsp]
$L$dec_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_vpaes_decrypt:
global	vpaes_cbc_encrypt

ALIGN	16
vpaes_cbc_encrypt:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_cbc_encrypt:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



_CET_ENDBR
	xchg	rdx,rcx
	sub	rcx,16
	jc	NEAR $L$cbc_abort
	lea	rsp,[((-184))+rsp]
	movaps	XMMWORD[16+rsp],xmm6
	movaps	XMMWORD[32+rsp],xmm7
	movaps	XMMWORD[48+rsp],xmm8
	movaps	XMMWORD[64+rsp],xmm9
	movaps	XMMWORD[80+rsp],xmm10
	movaps	XMMWORD[96+rsp],xmm11
	movaps	XMMWORD[112+rsp],xmm12
	movaps	XMMWORD[128+rsp],xmm13
	movaps	XMMWORD[144+rsp],xmm14
	movaps	XMMWORD[160+rsp],xmm15
$L$cbc_body:
	movdqu	xmm6,XMMWORD[r8]  ; load IV
	sub	rsi,rdi
	call	_vpaes_preheat
	cmp	r9d,0
	je	NEAR $L$cbc_dec_loop
	jmp	NEAR $L$cbc_enc_loop
ALIGN	16
$L$cbc_enc_loop:
	movdqu	xmm0,XMMWORD[rdi]
	pxor	xmm0,xmm6
	call	_vpaes_encrypt_core
	movdqa	xmm6,xmm0
	movdqu	XMMWORD[rdi*1+rsi],xmm0
	lea	rdi,[16+rdi]
	sub	rcx,16
	jnc	NEAR $L$cbc_enc_loop
	jmp	NEAR $L$cbc_done
ALIGN	16
$L$cbc_dec_loop:
	movdqu	xmm0,XMMWORD[rdi]
	movdqa	xmm7,xmm0
	call	_vpaes_decrypt_core
	pxor	xmm0,xmm6
	movdqa	xmm6,xmm7
	movdqu	XMMWORD[rdi*1+rsi],xmm0
	lea	rdi,[16+rdi]
	sub	rcx,16
	jnc	NEAR $L$cbc_dec_loop
$L$cbc_done:
	movdqu	XMMWORD[r8],xmm6  ; save IV
	movaps	xmm6,XMMWORD[16+rsp]
	movaps	xmm7,XMMWORD[32+rsp]
	movaps	xmm8,XMMWORD[48+rsp]
	movaps	xmm9,XMMWORD[64+rsp]
	movaps	xmm10,XMMWORD[80+rsp]
	movaps	xmm11,XMMWORD[96+rsp]
	movaps	xmm12,XMMWORD[112+rsp]
	movaps	xmm13,XMMWORD[128+rsp]
	movaps	xmm14,XMMWORD[144+rsp]
	movaps	xmm15,XMMWORD[160+rsp]
	lea	rsp,[184+rsp]
$L$cbc_epilogue:
$L$cbc_abort:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_vpaes_cbc_encrypt:
global	vpaes_ctr32_encrypt_blocks

ALIGN	16
vpaes_ctr32_encrypt_blocks:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_ctr32_encrypt_blocks:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]



_CET_ENDBR
; _vpaes_encrypt_core and _vpaes_encrypt_core_2x expect the key in %rdx.
	xchg	rdx,rcx
	test	rcx,rcx
	jz	NEAR $L$ctr32_abort
	lea	rsp,[((-184))+rsp]
	movaps	XMMWORD[16+rsp],xmm6
	movaps	XMMWORD[32+rsp],xmm7
	movaps	XMMWORD[48+rsp],xmm8
	movaps	XMMWORD[64+rsp],xmm9
	movaps	XMMWORD[80+rsp],xmm10
	movaps	XMMWORD[96+rsp],xmm11
	movaps	XMMWORD[112+rsp],xmm12
	movaps	XMMWORD[128+rsp],xmm13
	movaps	XMMWORD[144+rsp],xmm14
	movaps	XMMWORD[160+rsp],xmm15
$L$ctr32_body:
	movdqu	xmm0,XMMWORD[r8]  ; Load IV.
	movdqa	xmm8,XMMWORD[$L$ctr_add_one]
	sub	rsi,rdi  ; This allows only incrementing %rdi.
	call	_vpaes_preheat
	movdqa	xmm6,xmm0
	pshufb	xmm6,XMMWORD[$L$rev_ctr]

	test	rcx,1
	jz	NEAR $L$ctr32_prep_loop

; Handle one block so the remaining block count is even for
; _vpaes_encrypt_core_2x.
	movdqu	xmm7,XMMWORD[rdi]  ; Load input.
	call	_vpaes_encrypt_core
	pxor	xmm0,xmm7
	paddd	xmm6,xmm8
	movdqu	XMMWORD[rdi*1+rsi],xmm0
	sub	rcx,1
	lea	rdi,[16+rdi]
	jz	NEAR $L$ctr32_done

$L$ctr32_prep_loop:
; _vpaes_encrypt_core_2x leaves only %xmm14 and %xmm15 as spare
; registers. We maintain two byte-swapped counters in them.
	movdqa	xmm14,xmm6
	movdqa	xmm15,xmm6
	paddd	xmm15,xmm8

$L$ctr32_loop:
	movdqa	xmm1,XMMWORD[$L$rev_ctr]  ; Set up counters.
	movdqa	xmm0,xmm14
	movdqa	xmm6,xmm15
	pshufb	xmm0,xmm1
	pshufb	xmm6,xmm1
	call	_vpaes_encrypt_core_2x
	movdqu	xmm1,XMMWORD[rdi]  ; Load input.
	movdqu	xmm2,XMMWORD[16+rdi]
	movdqa	xmm3,XMMWORD[$L$ctr_add_two]
	pxor	xmm0,xmm1  ; XOR input.
	pxor	xmm6,xmm2
	paddd	xmm14,xmm3  ; Increment counters.
	paddd	xmm15,xmm3
	movdqu	XMMWORD[rdi*1+rsi],xmm0  ; Write output.
	movdqu	XMMWORD[16+rdi*1+rsi],xmm6
	sub	rcx,2  ; Advance loop.
	lea	rdi,[32+rdi]
	jnz	NEAR $L$ctr32_loop

$L$ctr32_done:
	movaps	xmm6,XMMWORD[16+rsp]
	movaps	xmm7,XMMWORD[32+rsp]
	movaps	xmm8,XMMWORD[48+rsp]
	movaps	xmm9,XMMWORD[64+rsp]
	movaps	xmm10,XMMWORD[80+rsp]
	movaps	xmm11,XMMWORD[96+rsp]
	movaps	xmm12,XMMWORD[112+rsp]
	movaps	xmm13,XMMWORD[128+rsp]
	movaps	xmm14,XMMWORD[144+rsp]
	movaps	xmm15,XMMWORD[160+rsp]
	lea	rsp,[184+rsp]
$L$ctr32_epilogue:
$L$ctr32_abort:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_vpaes_ctr32_encrypt_blocks:
; #
; #  _aes_preheat
; #
; #  Fills register %r10 -> .aes_consts (so you can -fPIC)
; #  and %xmm9-%xmm15 as specified below.
; #

ALIGN	16
_vpaes_preheat:

	lea	r10,[$L$k_s0F]
	movdqa	xmm10,XMMWORD[((-32))+r10]  ; .Lk_inv
	movdqa	xmm11,XMMWORD[((-16))+r10]  ; .Lk_inv+16
	movdqa	xmm9,XMMWORD[r10]  ; .Lk_s0F
	movdqa	xmm13,XMMWORD[48+r10]  ; .Lk_sb1
	movdqa	xmm12,XMMWORD[64+r10]  ; .Lk_sb1+16
	movdqa	xmm15,XMMWORD[80+r10]  ; .Lk_sb2
	movdqa	xmm14,XMMWORD[96+r10]  ; .Lk_sb2+16
	ret


; #######################################################
; #                                                    ##
; #                     Constants                      ##
; #                                                    ##
; #######################################################

section	.rdata rdata align=8
ALIGN	64
_vpaes_consts:
$L$k_inv:  ; inv, inva
	DQ	0x0E05060F0D080180,0x040703090A0B0C02
	DQ	0x01040A060F0B0780,0x030D0E0C02050809

$L$k_s0F:  ; s0F
	DQ	0x0F0F0F0F0F0F0F0F,0x0F0F0F0F0F0F0F0F

$L$k_ipt:  ; input transform (lo, hi)
	DQ	0xC2B2E8985A2A7000,0xCABAE09052227808
	DQ	0x4C01307D317C4D00,0xCD80B1FCB0FDCC81

$L$k_sb1:  ; sb1u, sb1t
	DQ	0xB19BE18FCB503E00,0xA5DF7A6E142AF544
	DQ	0x3618D415FAE22300,0x3BF7CCC10D2ED9EF
$L$k_sb2:  ; sb2u, sb2t
	DQ	0xE27A93C60B712400,0x5EB7E955BC982FCD
	DQ	0x69EB88400AE12900,0xC2A163C8AB82234A
$L$k_sbo:  ; sbou, sbot
	DQ	0xD0D26D176FBDC700,0x15AABF7AC502A878
	DQ	0xCFE474A55FBB6A00,0x8E1E90D1412B35FA

$L$k_mc_forward:  ; mc_forward
	DQ	0x0407060500030201,0x0C0F0E0D080B0A09
	DQ	0x080B0A0904070605,0x000302010C0F0E0D
	DQ	0x0C0F0E0D080B0A09,0x0407060500030201
	DQ	0x000302010C0F0E0D,0x080B0A0904070605

$L$k_mc_backward:  ; mc_backward
	DQ	0x0605040702010003,0x0E0D0C0F0A09080B
	DQ	0x020100030E0D0C0F,0x0A09080B06050407
	DQ	0x0E0D0C0F0A09080B,0x0605040702010003
	DQ	0x0A09080B06050407,0x020100030E0D0C0F

$L$k_sr:  ; sr
	DQ	0x0706050403020100,0x0F0E0D0C0B0A0908
	DQ	0x030E09040F0A0500,0x0B06010C07020D08
	DQ	0x0F060D040B020900,0x070E050C030A0108
	DQ	0x0B0E0104070A0D00,0x0306090C0F020508

$L$k_rcon:  ; rcon
	DQ	0x1F8391B9AF9DEEB6,0x702A98084D7C7D81

$L$k_s63:  ; s63: all equal to 0x63 transformed
	DQ	0x5B5B5B5B5B5B5B5B,0x5B5B5B5B5B5B5B5B

$L$k_opt:  ; output transform
	DQ	0xFF9F4929D6B66000,0xF7974121DEBE6808
	DQ	0x01EDBD5150BCEC00,0xE10D5DB1B05C0CE0

$L$k_deskew:  ; deskew tables: inverts the sbox's "skew"
	DQ	0x07E4A34047A4E300,0x1DFEB95A5DBEF91A
	DQ	0x5F36B5DC83EA6900,0x2841C2ABF49D1E77

; #
; #  Decryption stuff
; #  Key schedule constants
; #
$L$k_dksd:  ; decryption key schedule: invskew x*D
	DQ	0xFEB91A5DA3E44700,0x0740E3A45A1DBEF9
	DQ	0x41C277F4B5368300,0x5FDC69EAAB289D1E
$L$k_dksb:  ; decryption key schedule: invskew x*B
	DQ	0x9A4FCA1F8550D500,0x03D653861CC94C99
	DQ	0x115BEDA7B6FC4A00,0xD993256F7E3482C8
$L$k_dkse:  ; decryption key schedule: invskew x*E + 0x63
	DQ	0xD5031CCA1FC9D600,0x53859A4C994F5086
	DQ	0xA23196054FDC7BE8,0xCD5EF96A20B31487
$L$k_dks9:  ; decryption key schedule: invskew x*9
	DQ	0xB6116FC87ED9A700,0x4AED933482255BFC
	DQ	0x4576516227143300,0x8BB89FACE9DAFDCE

; #
; #  Decryption stuff
; #  Round function constants
; #
$L$k_dipt:  ; decryption input transform
	DQ	0x0F505B040B545F00,0x154A411E114E451A
	DQ	0x86E383E660056500,0x12771772F491F194

$L$k_dsb9:  ; decryption sbox output *9*u, *9*t
	DQ	0x851C03539A86D600,0xCAD51F504F994CC9
	DQ	0xC03B1789ECD74900,0x725E2C9EB2FBA565
$L$k_dsbd:  ; decryption sbox output *D*u, *D*t
	DQ	0x7D57CCDFE6B1A200,0xF56E9B13882A4439
	DQ	0x3CE2FAF724C6CB00,0x2931180D15DEEFD3
$L$k_dsbb:  ; decryption sbox output *B*u, *B*t
	DQ	0xD022649296B44200,0x602646F6B0F2D404
	DQ	0xC19498A6CD596700,0xF3FF0C3E3255AA6B
$L$k_dsbe:  ; decryption sbox output *E*u, *E*t
	DQ	0x46F2929626D4D000,0x2242600464B4F6B0
	DQ	0x0C55A6CDFFAAC100,0x9467F36B98593E32
$L$k_dsbo:  ; decryption sbox final output
	DQ	0x1387EA537EF94000,0xC7AA6DB9D4943E2D
	DQ	0x12D7560F93441D00,0xCA4B8159D8C58E9C

; .Lrev_ctr is a permutation which byte-swaps the counter portion of the IV.
$L$rev_ctr:
	DQ	0x0706050403020100,0x0c0d0e0f0b0a0908
; .Lctr_add_* may be added to a byte-swapped xmm register to increment the
; counter. The register must be byte-swapped again to form the actual input.
$L$ctr_add_one:
	DQ	0x0000000000000000,0x0000000100000000
$L$ctr_add_two:
	DQ	0x0000000000000000,0x0000000200000000

	DB	86,101,99,116,111,114,32,80,101,114,109,117,116,97,116,105
	DB	111,110,32,65,69,83,32,102,111,114,32,120,56,54,95,54
	DB	52,47,83,83,83,69,51,44,32,77,105,107,101,32,72,97
	DB	109,98,117,114,103,32,40,83,116,97,110,102,111,114,100,32
	DB	85,110,105,118,101,114,115,105,116,121,41,0
ALIGN	64

section	.text code align=64

EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
se_handler:
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
	lea	r10,[r10*1+rsi]  ; prologue label
	cmp	rbx,r10  ; context->Rip<prologue label
	jb	NEAR $L$in_prologue

	mov	rax,QWORD[152+r8]  ; pull context->Rsp

	mov	r10d,DWORD[4+r11]  ; HandlerData[1]
	lea	r10,[r10*1+rsi]  ; epilogue label
	cmp	rbx,r10  ; context->Rip>=epilogue label
	jae	NEAR $L$in_prologue

	lea	rsi,[16+rax]  ; %xmm save area
	lea	rdi,[512+r8]  ; &context.Xmm6
	mov	ecx,20  ; 10*sizeof(%xmm0)/sizeof(%rax)
	DD	0xa548f3fc  ; cld; rep movsq
	lea	rax,[184+rax]  ; adjust stack pointer

$L$in_prologue:
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
	DD	$L$SEH_begin_vpaes_set_encrypt_key wrt ..imagebase
	DD	$L$SEH_end_vpaes_set_encrypt_key wrt ..imagebase
	DD	$L$SEH_info_vpaes_set_encrypt_key wrt ..imagebase

	DD	$L$SEH_begin_vpaes_set_decrypt_key wrt ..imagebase
	DD	$L$SEH_end_vpaes_set_decrypt_key wrt ..imagebase
	DD	$L$SEH_info_vpaes_set_decrypt_key wrt ..imagebase

	DD	$L$SEH_begin_vpaes_encrypt wrt ..imagebase
	DD	$L$SEH_end_vpaes_encrypt wrt ..imagebase
	DD	$L$SEH_info_vpaes_encrypt wrt ..imagebase

	DD	$L$SEH_begin_vpaes_decrypt wrt ..imagebase
	DD	$L$SEH_end_vpaes_decrypt wrt ..imagebase
	DD	$L$SEH_info_vpaes_decrypt wrt ..imagebase

	DD	$L$SEH_begin_vpaes_cbc_encrypt wrt ..imagebase
	DD	$L$SEH_end_vpaes_cbc_encrypt wrt ..imagebase
	DD	$L$SEH_info_vpaes_cbc_encrypt wrt ..imagebase

	DD	$L$SEH_begin_vpaes_ctr32_encrypt_blocks wrt ..imagebase
	DD	$L$SEH_end_vpaes_ctr32_encrypt_blocks wrt ..imagebase
	DD	$L$SEH_info_vpaes_ctr32_encrypt_blocks wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_vpaes_set_encrypt_key:
	DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$enc_key_body wrt ..imagebase,$L$enc_key_epilogue wrt ..imagebase  ; HandlerData[]
$L$SEH_info_vpaes_set_decrypt_key:
	DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$dec_key_body wrt ..imagebase,$L$dec_key_epilogue wrt ..imagebase  ; HandlerData[]
$L$SEH_info_vpaes_encrypt:
	DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$enc_body wrt ..imagebase,$L$enc_epilogue wrt ..imagebase  ; HandlerData[]
$L$SEH_info_vpaes_decrypt:
	DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$dec_body wrt ..imagebase,$L$dec_epilogue wrt ..imagebase  ; HandlerData[]
$L$SEH_info_vpaes_cbc_encrypt:
	DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$cbc_body wrt ..imagebase,$L$cbc_epilogue wrt ..imagebase  ; HandlerData[]
$L$SEH_info_vpaes_ctr32_encrypt_blocks:
	DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$ctr32_body wrt ..imagebase,$L$ctr32_epilogue wrt ..imagebase  ; HandlerData[]
%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
