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


; CRYPTO_rdrand writes eight bytes of random data from the hardware RNG to
; |out|. It returns one on success or zero on hardware failure.
; int CRYPTO_rdrand(uint8_t out[8]);
global	CRYPTO_rdrand

ALIGN	16
CRYPTO_rdrand:

_CET_ENDBR
	xor	rax,rax
	rdrand	r8
; An add-with-carry of zero effectively sets %rax to the carry flag.
	adc	rax,rax
	mov	QWORD[rcx],r8
	ret



; CRYPTO_rdrand_multiple8_buf fills |len| bytes at |buf| with random data from
; the hardware RNG. The |len| argument must be a multiple of eight. It returns
; one on success and zero on hardware failure.
; int CRYPTO_rdrand_multiple8_buf(uint8_t *buf, size_t len);
global	CRYPTO_rdrand_multiple8_buf

ALIGN	16
CRYPTO_rdrand_multiple8_buf:

_CET_ENDBR
	test	rdx,rdx
	jz	NEAR $L$out
	mov	r8,8
$L$loop:
	rdrand	r9
	jnc	NEAR $L$err
	mov	QWORD[rcx],r9
	add	rcx,r8
	sub	rdx,r8
	jnz	NEAR $L$loop
$L$out:
	mov	rax,1
	ret
$L$err:
	xor	rax,rax
	ret


%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
