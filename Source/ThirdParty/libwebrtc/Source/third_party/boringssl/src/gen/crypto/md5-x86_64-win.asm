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

ALIGN	16

global	md5_block_asm_data_order

md5_block_asm_data_order:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_md5_block_asm_data_order:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
	push	rbp

	push	rbx

	push	r12

	push	r14

	push	r15

$L$prologue:

; rdi = arg #1 (ctx, MD5_CTX pointer)
; rsi = arg #2 (ptr, data pointer)
; rdx = arg #3 (nbr, number of 16-word blocks to process)
	mov	rbp,rdi  ; rbp = ctx
	shl	rdx,6  ; rdx = nbr in bytes
	lea	rdi,[rdx*1+rsi]  ; rdi = end
	mov	eax,DWORD[rbp]  ; eax = ctx->A
	mov	ebx,DWORD[4+rbp]  ; ebx = ctx->B
	mov	ecx,DWORD[8+rbp]  ; ecx = ctx->C
	mov	edx,DWORD[12+rbp]  ; edx = ctx->D
; end is 'rdi'
; ptr is 'rsi'
; A is 'eax'
; B is 'ebx'
; C is 'ecx'
; D is 'edx'

	cmp	rsi,rdi  ; cmp end with ptr
	je	NEAR $L$end  ; jmp if ptr == end

; BEGIN of loop over 16-word blocks
$L$loop:  ; save old values of A, B, C, D
	mov	r8d,eax
	mov	r9d,ebx
	mov	r14d,ecx
	mov	r15d,edx
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	xor	r11d,ecx  ; y ^ ...
	lea	eax,[((-680876936))+r10*1+rax]  ; Const + dst + ...
	and	r11d,ebx  ; x & ...
	xor	r11d,edx  ; z ^ ...
	mov	r10d,DWORD[4+rsi]  ; (NEXT STEP) X[1]
	add	eax,r11d  ; dst += ...
	rol	eax,7  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,ebx  ; dst += x
	xor	r11d,ebx  ; y ^ ...
	lea	edx,[((-389564586))+r10*1+rdx]  ; Const + dst + ...
	and	r11d,eax  ; x & ...
	xor	r11d,ecx  ; z ^ ...
	mov	r10d,DWORD[8+rsi]  ; (NEXT STEP) X[2]
	add	edx,r11d  ; dst += ...
	rol	edx,12  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,eax  ; dst += x
	xor	r11d,eax  ; y ^ ...
	lea	ecx,[606105819+r10*1+rcx]  ; Const + dst + ...
	and	r11d,edx  ; x & ...
	xor	r11d,ebx  ; z ^ ...
	mov	r10d,DWORD[12+rsi]  ; (NEXT STEP) X[3]
	add	ecx,r11d  ; dst += ...
	rol	ecx,17  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,edx  ; dst += x
	xor	r11d,edx  ; y ^ ...
	lea	ebx,[((-1044525330))+r10*1+rbx]  ; Const + dst + ...
	and	r11d,ecx  ; x & ...
	xor	r11d,eax  ; z ^ ...
	mov	r10d,DWORD[16+rsi]  ; (NEXT STEP) X[4]
	add	ebx,r11d  ; dst += ...
	rol	ebx,22  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,ecx  ; dst += x
	xor	r11d,ecx  ; y ^ ...
	lea	eax,[((-176418897))+r10*1+rax]  ; Const + dst + ...
	and	r11d,ebx  ; x & ...
	xor	r11d,edx  ; z ^ ...
	mov	r10d,DWORD[20+rsi]  ; (NEXT STEP) X[5]
	add	eax,r11d  ; dst += ...
	rol	eax,7  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,ebx  ; dst += x
	xor	r11d,ebx  ; y ^ ...
	lea	edx,[1200080426+r10*1+rdx]  ; Const + dst + ...
	and	r11d,eax  ; x & ...
	xor	r11d,ecx  ; z ^ ...
	mov	r10d,DWORD[24+rsi]  ; (NEXT STEP) X[6]
	add	edx,r11d  ; dst += ...
	rol	edx,12  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,eax  ; dst += x
	xor	r11d,eax  ; y ^ ...
	lea	ecx,[((-1473231341))+r10*1+rcx]  ; Const + dst + ...
	and	r11d,edx  ; x & ...
	xor	r11d,ebx  ; z ^ ...
	mov	r10d,DWORD[28+rsi]  ; (NEXT STEP) X[7]
	add	ecx,r11d  ; dst += ...
	rol	ecx,17  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,edx  ; dst += x
	xor	r11d,edx  ; y ^ ...
	lea	ebx,[((-45705983))+r10*1+rbx]  ; Const + dst + ...
	and	r11d,ecx  ; x & ...
	xor	r11d,eax  ; z ^ ...
	mov	r10d,DWORD[32+rsi]  ; (NEXT STEP) X[8]
	add	ebx,r11d  ; dst += ...
	rol	ebx,22  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,ecx  ; dst += x
	xor	r11d,ecx  ; y ^ ...
	lea	eax,[1770035416+r10*1+rax]  ; Const + dst + ...
	and	r11d,ebx  ; x & ...
	xor	r11d,edx  ; z ^ ...
	mov	r10d,DWORD[36+rsi]  ; (NEXT STEP) X[9]
	add	eax,r11d  ; dst += ...
	rol	eax,7  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,ebx  ; dst += x
	xor	r11d,ebx  ; y ^ ...
	lea	edx,[((-1958414417))+r10*1+rdx]  ; Const + dst + ...
	and	r11d,eax  ; x & ...
	xor	r11d,ecx  ; z ^ ...
	mov	r10d,DWORD[40+rsi]  ; (NEXT STEP) X[10]
	add	edx,r11d  ; dst += ...
	rol	edx,12  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,eax  ; dst += x
	xor	r11d,eax  ; y ^ ...
	lea	ecx,[((-42063))+r10*1+rcx]  ; Const + dst + ...
	and	r11d,edx  ; x & ...
	xor	r11d,ebx  ; z ^ ...
	mov	r10d,DWORD[44+rsi]  ; (NEXT STEP) X[11]
	add	ecx,r11d  ; dst += ...
	rol	ecx,17  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,edx  ; dst += x
	xor	r11d,edx  ; y ^ ...
	lea	ebx,[((-1990404162))+r10*1+rbx]  ; Const + dst + ...
	and	r11d,ecx  ; x & ...
	xor	r11d,eax  ; z ^ ...
	mov	r10d,DWORD[48+rsi]  ; (NEXT STEP) X[12]
	add	ebx,r11d  ; dst += ...
	rol	ebx,22  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,ecx  ; dst += x
	xor	r11d,ecx  ; y ^ ...
	lea	eax,[1804603682+r10*1+rax]  ; Const + dst + ...
	and	r11d,ebx  ; x & ...
	xor	r11d,edx  ; z ^ ...
	mov	r10d,DWORD[52+rsi]  ; (NEXT STEP) X[13]
	add	eax,r11d  ; dst += ...
	rol	eax,7  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,ebx  ; dst += x
	xor	r11d,ebx  ; y ^ ...
	lea	edx,[((-40341101))+r10*1+rdx]  ; Const + dst + ...
	and	r11d,eax  ; x & ...
	xor	r11d,ecx  ; z ^ ...
	mov	r10d,DWORD[56+rsi]  ; (NEXT STEP) X[14]
	add	edx,r11d  ; dst += ...
	rol	edx,12  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,eax  ; dst += x
	xor	r11d,eax  ; y ^ ...
	lea	ecx,[((-1502002290))+r10*1+rcx]  ; Const + dst + ...
	and	r11d,edx  ; x & ...
	xor	r11d,ebx  ; z ^ ...
	mov	r10d,DWORD[60+rsi]  ; (NEXT STEP) X[15]
	add	ecx,r11d  ; dst += ...
	rol	ecx,17  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,edx  ; dst += x
	xor	r11d,edx  ; y ^ ...
	lea	ebx,[1236535329+r10*1+rbx]  ; Const + dst + ...
	and	r11d,ecx  ; x & ...
	xor	r11d,eax  ; z ^ ...
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	add	ebx,r11d  ; dst += ...
	rol	ebx,22  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,ecx  ; dst += x
	mov	r10d,DWORD[4+rsi]  ; (NEXT STEP) X[1]
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	mov	r12d,edx  ; (NEXT STEP) z' = %edx
	not	r11d  ; not z
	lea	eax,[((-165796510))+r10*1+rax]  ; Const + dst + ...
	and	r12d,ebx  ; x & z
	and	r11d,ecx  ; y & (not z)
	mov	r10d,DWORD[24+rsi]  ; (NEXT STEP) X[6]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,r12d  ; dst += ...
	mov	r12d,ecx  ; (NEXT STEP) z' = %ecx
	rol	eax,5  ; dst <<< s
	add	eax,ebx  ; dst += x
	not	r11d  ; not z
	lea	edx,[((-1069501632))+r10*1+rdx]  ; Const + dst + ...
	and	r12d,eax  ; x & z
	and	r11d,ebx  ; y & (not z)
	mov	r10d,DWORD[44+rsi]  ; (NEXT STEP) X[11]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,r12d  ; dst += ...
	mov	r12d,ebx  ; (NEXT STEP) z' = %ebx
	rol	edx,9  ; dst <<< s
	add	edx,eax  ; dst += x
	not	r11d  ; not z
	lea	ecx,[643717713+r10*1+rcx]  ; Const + dst + ...
	and	r12d,edx  ; x & z
	and	r11d,eax  ; y & (not z)
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,r12d  ; dst += ...
	mov	r12d,eax  ; (NEXT STEP) z' = %eax
	rol	ecx,14  ; dst <<< s
	add	ecx,edx  ; dst += x
	not	r11d  ; not z
	lea	ebx,[((-373897302))+r10*1+rbx]  ; Const + dst + ...
	and	r12d,ecx  ; x & z
	and	r11d,edx  ; y & (not z)
	mov	r10d,DWORD[20+rsi]  ; (NEXT STEP) X[5]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,r12d  ; dst += ...
	mov	r12d,edx  ; (NEXT STEP) z' = %edx
	rol	ebx,20  ; dst <<< s
	add	ebx,ecx  ; dst += x
	not	r11d  ; not z
	lea	eax,[((-701558691))+r10*1+rax]  ; Const + dst + ...
	and	r12d,ebx  ; x & z
	and	r11d,ecx  ; y & (not z)
	mov	r10d,DWORD[40+rsi]  ; (NEXT STEP) X[10]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,r12d  ; dst += ...
	mov	r12d,ecx  ; (NEXT STEP) z' = %ecx
	rol	eax,5  ; dst <<< s
	add	eax,ebx  ; dst += x
	not	r11d  ; not z
	lea	edx,[38016083+r10*1+rdx]  ; Const + dst + ...
	and	r12d,eax  ; x & z
	and	r11d,ebx  ; y & (not z)
	mov	r10d,DWORD[60+rsi]  ; (NEXT STEP) X[15]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,r12d  ; dst += ...
	mov	r12d,ebx  ; (NEXT STEP) z' = %ebx
	rol	edx,9  ; dst <<< s
	add	edx,eax  ; dst += x
	not	r11d  ; not z
	lea	ecx,[((-660478335))+r10*1+rcx]  ; Const + dst + ...
	and	r12d,edx  ; x & z
	and	r11d,eax  ; y & (not z)
	mov	r10d,DWORD[16+rsi]  ; (NEXT STEP) X[4]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,r12d  ; dst += ...
	mov	r12d,eax  ; (NEXT STEP) z' = %eax
	rol	ecx,14  ; dst <<< s
	add	ecx,edx  ; dst += x
	not	r11d  ; not z
	lea	ebx,[((-405537848))+r10*1+rbx]  ; Const + dst + ...
	and	r12d,ecx  ; x & z
	and	r11d,edx  ; y & (not z)
	mov	r10d,DWORD[36+rsi]  ; (NEXT STEP) X[9]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,r12d  ; dst += ...
	mov	r12d,edx  ; (NEXT STEP) z' = %edx
	rol	ebx,20  ; dst <<< s
	add	ebx,ecx  ; dst += x
	not	r11d  ; not z
	lea	eax,[568446438+r10*1+rax]  ; Const + dst + ...
	and	r12d,ebx  ; x & z
	and	r11d,ecx  ; y & (not z)
	mov	r10d,DWORD[56+rsi]  ; (NEXT STEP) X[14]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,r12d  ; dst += ...
	mov	r12d,ecx  ; (NEXT STEP) z' = %ecx
	rol	eax,5  ; dst <<< s
	add	eax,ebx  ; dst += x
	not	r11d  ; not z
	lea	edx,[((-1019803690))+r10*1+rdx]  ; Const + dst + ...
	and	r12d,eax  ; x & z
	and	r11d,ebx  ; y & (not z)
	mov	r10d,DWORD[12+rsi]  ; (NEXT STEP) X[3]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,r12d  ; dst += ...
	mov	r12d,ebx  ; (NEXT STEP) z' = %ebx
	rol	edx,9  ; dst <<< s
	add	edx,eax  ; dst += x
	not	r11d  ; not z
	lea	ecx,[((-187363961))+r10*1+rcx]  ; Const + dst + ...
	and	r12d,edx  ; x & z
	and	r11d,eax  ; y & (not z)
	mov	r10d,DWORD[32+rsi]  ; (NEXT STEP) X[8]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,r12d  ; dst += ...
	mov	r12d,eax  ; (NEXT STEP) z' = %eax
	rol	ecx,14  ; dst <<< s
	add	ecx,edx  ; dst += x
	not	r11d  ; not z
	lea	ebx,[1163531501+r10*1+rbx]  ; Const + dst + ...
	and	r12d,ecx  ; x & z
	and	r11d,edx  ; y & (not z)
	mov	r10d,DWORD[52+rsi]  ; (NEXT STEP) X[13]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,r12d  ; dst += ...
	mov	r12d,edx  ; (NEXT STEP) z' = %edx
	rol	ebx,20  ; dst <<< s
	add	ebx,ecx  ; dst += x
	not	r11d  ; not z
	lea	eax,[((-1444681467))+r10*1+rax]  ; Const + dst + ...
	and	r12d,ebx  ; x & z
	and	r11d,ecx  ; y & (not z)
	mov	r10d,DWORD[8+rsi]  ; (NEXT STEP) X[2]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ecx  ; (NEXT STEP) z' = %ecx
	add	eax,r12d  ; dst += ...
	mov	r12d,ecx  ; (NEXT STEP) z' = %ecx
	rol	eax,5  ; dst <<< s
	add	eax,ebx  ; dst += x
	not	r11d  ; not z
	lea	edx,[((-51403784))+r10*1+rdx]  ; Const + dst + ...
	and	r12d,eax  ; x & z
	and	r11d,ebx  ; y & (not z)
	mov	r10d,DWORD[28+rsi]  ; (NEXT STEP) X[7]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,ebx  ; (NEXT STEP) z' = %ebx
	add	edx,r12d  ; dst += ...
	mov	r12d,ebx  ; (NEXT STEP) z' = %ebx
	rol	edx,9  ; dst <<< s
	add	edx,eax  ; dst += x
	not	r11d  ; not z
	lea	ecx,[1735328473+r10*1+rcx]  ; Const + dst + ...
	and	r12d,edx  ; x & z
	and	r11d,eax  ; y & (not z)
	mov	r10d,DWORD[48+rsi]  ; (NEXT STEP) X[12]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,eax  ; (NEXT STEP) z' = %eax
	add	ecx,r12d  ; dst += ...
	mov	r12d,eax  ; (NEXT STEP) z' = %eax
	rol	ecx,14  ; dst <<< s
	add	ecx,edx  ; dst += x
	not	r11d  ; not z
	lea	ebx,[((-1926607734))+r10*1+rbx]  ; Const + dst + ...
	and	r12d,ecx  ; x & z
	and	r11d,edx  ; y & (not z)
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	or	r12d,r11d  ; (y & (not z)) | (x & z)
	mov	r11d,edx  ; (NEXT STEP) z' = %edx
	add	ebx,r12d  ; dst += ...
	mov	r12d,edx  ; (NEXT STEP) z' = %edx
	rol	ebx,20  ; dst <<< s
	add	ebx,ecx  ; dst += x
	mov	r10d,DWORD[20+rsi]  ; (NEXT STEP) X[5]
	mov	r11d,ecx  ; (NEXT STEP) y' = %ecx
	lea	eax,[((-378558))+r10*1+rax]  ; Const + dst + ...
	mov	r10d,DWORD[32+rsi]  ; (NEXT STEP) X[8]
	xor	r11d,edx  ; z ^ ...
	xor	r11d,ebx  ; x ^ ...
	add	eax,r11d  ; dst += ...
	rol	eax,4  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) y' = %ebx
	add	eax,ebx  ; dst += x
	lea	edx,[((-2022574463))+r10*1+rdx]  ; Const + dst + ...
	mov	r10d,DWORD[44+rsi]  ; (NEXT STEP) X[11]
	xor	r11d,ecx  ; z ^ ...
	xor	r11d,eax  ; x ^ ...
	add	edx,r11d  ; dst += ...
	rol	edx,11  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) y' = %eax
	add	edx,eax  ; dst += x
	lea	ecx,[1839030562+r10*1+rcx]  ; Const + dst + ...
	mov	r10d,DWORD[56+rsi]  ; (NEXT STEP) X[14]
	xor	r11d,ebx  ; z ^ ...
	xor	r11d,edx  ; x ^ ...
	add	ecx,r11d  ; dst += ...
	rol	ecx,16  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) y' = %edx
	add	ecx,edx  ; dst += x
	lea	ebx,[((-35309556))+r10*1+rbx]  ; Const + dst + ...
	mov	r10d,DWORD[4+rsi]  ; (NEXT STEP) X[1]
	xor	r11d,eax  ; z ^ ...
	xor	r11d,ecx  ; x ^ ...
	add	ebx,r11d  ; dst += ...
	rol	ebx,23  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) y' = %ecx
	add	ebx,ecx  ; dst += x
	lea	eax,[((-1530992060))+r10*1+rax]  ; Const + dst + ...
	mov	r10d,DWORD[16+rsi]  ; (NEXT STEP) X[4]
	xor	r11d,edx  ; z ^ ...
	xor	r11d,ebx  ; x ^ ...
	add	eax,r11d  ; dst += ...
	rol	eax,4  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) y' = %ebx
	add	eax,ebx  ; dst += x
	lea	edx,[1272893353+r10*1+rdx]  ; Const + dst + ...
	mov	r10d,DWORD[28+rsi]  ; (NEXT STEP) X[7]
	xor	r11d,ecx  ; z ^ ...
	xor	r11d,eax  ; x ^ ...
	add	edx,r11d  ; dst += ...
	rol	edx,11  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) y' = %eax
	add	edx,eax  ; dst += x
	lea	ecx,[((-155497632))+r10*1+rcx]  ; Const + dst + ...
	mov	r10d,DWORD[40+rsi]  ; (NEXT STEP) X[10]
	xor	r11d,ebx  ; z ^ ...
	xor	r11d,edx  ; x ^ ...
	add	ecx,r11d  ; dst += ...
	rol	ecx,16  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) y' = %edx
	add	ecx,edx  ; dst += x
	lea	ebx,[((-1094730640))+r10*1+rbx]  ; Const + dst + ...
	mov	r10d,DWORD[52+rsi]  ; (NEXT STEP) X[13]
	xor	r11d,eax  ; z ^ ...
	xor	r11d,ecx  ; x ^ ...
	add	ebx,r11d  ; dst += ...
	rol	ebx,23  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) y' = %ecx
	add	ebx,ecx  ; dst += x
	lea	eax,[681279174+r10*1+rax]  ; Const + dst + ...
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	xor	r11d,edx  ; z ^ ...
	xor	r11d,ebx  ; x ^ ...
	add	eax,r11d  ; dst += ...
	rol	eax,4  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) y' = %ebx
	add	eax,ebx  ; dst += x
	lea	edx,[((-358537222))+r10*1+rdx]  ; Const + dst + ...
	mov	r10d,DWORD[12+rsi]  ; (NEXT STEP) X[3]
	xor	r11d,ecx  ; z ^ ...
	xor	r11d,eax  ; x ^ ...
	add	edx,r11d  ; dst += ...
	rol	edx,11  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) y' = %eax
	add	edx,eax  ; dst += x
	lea	ecx,[((-722521979))+r10*1+rcx]  ; Const + dst + ...
	mov	r10d,DWORD[24+rsi]  ; (NEXT STEP) X[6]
	xor	r11d,ebx  ; z ^ ...
	xor	r11d,edx  ; x ^ ...
	add	ecx,r11d  ; dst += ...
	rol	ecx,16  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) y' = %edx
	add	ecx,edx  ; dst += x
	lea	ebx,[76029189+r10*1+rbx]  ; Const + dst + ...
	mov	r10d,DWORD[36+rsi]  ; (NEXT STEP) X[9]
	xor	r11d,eax  ; z ^ ...
	xor	r11d,ecx  ; x ^ ...
	add	ebx,r11d  ; dst += ...
	rol	ebx,23  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) y' = %ecx
	add	ebx,ecx  ; dst += x
	lea	eax,[((-640364487))+r10*1+rax]  ; Const + dst + ...
	mov	r10d,DWORD[48+rsi]  ; (NEXT STEP) X[12]
	xor	r11d,edx  ; z ^ ...
	xor	r11d,ebx  ; x ^ ...
	add	eax,r11d  ; dst += ...
	rol	eax,4  ; dst <<< s
	mov	r11d,ebx  ; (NEXT STEP) y' = %ebx
	add	eax,ebx  ; dst += x
	lea	edx,[((-421815835))+r10*1+rdx]  ; Const + dst + ...
	mov	r10d,DWORD[60+rsi]  ; (NEXT STEP) X[15]
	xor	r11d,ecx  ; z ^ ...
	xor	r11d,eax  ; x ^ ...
	add	edx,r11d  ; dst += ...
	rol	edx,11  ; dst <<< s
	mov	r11d,eax  ; (NEXT STEP) y' = %eax
	add	edx,eax  ; dst += x
	lea	ecx,[530742520+r10*1+rcx]  ; Const + dst + ...
	mov	r10d,DWORD[8+rsi]  ; (NEXT STEP) X[2]
	xor	r11d,ebx  ; z ^ ...
	xor	r11d,edx  ; x ^ ...
	add	ecx,r11d  ; dst += ...
	rol	ecx,16  ; dst <<< s
	mov	r11d,edx  ; (NEXT STEP) y' = %edx
	add	ecx,edx  ; dst += x
	lea	ebx,[((-995338651))+r10*1+rbx]  ; Const + dst + ...
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	xor	r11d,eax  ; z ^ ...
	xor	r11d,ecx  ; x ^ ...
	add	ebx,r11d  ; dst += ...
	rol	ebx,23  ; dst <<< s
	mov	r11d,ecx  ; (NEXT STEP) y' = %ecx
	add	ebx,ecx  ; dst += x
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	mov	r11d,0xffffffff
	xor	r11d,edx  ; (NEXT STEP) not z' = not %edx
	lea	eax,[((-198630844))+r10*1+rax]  ; Const + dst + ...
	or	r11d,ebx  ; x | ...
	xor	r11d,ecx  ; y ^ ...
	add	eax,r11d  ; dst += ...
	mov	r10d,DWORD[28+rsi]  ; (NEXT STEP) X[7]
	mov	r11d,0xffffffff
	rol	eax,6  ; dst <<< s
	xor	r11d,ecx  ; (NEXT STEP) not z' = not %ecx
	add	eax,ebx  ; dst += x
	lea	edx,[1126891415+r10*1+rdx]  ; Const + dst + ...
	or	r11d,eax  ; x | ...
	xor	r11d,ebx  ; y ^ ...
	add	edx,r11d  ; dst += ...
	mov	r10d,DWORD[56+rsi]  ; (NEXT STEP) X[14]
	mov	r11d,0xffffffff
	rol	edx,10  ; dst <<< s
	xor	r11d,ebx  ; (NEXT STEP) not z' = not %ebx
	add	edx,eax  ; dst += x
	lea	ecx,[((-1416354905))+r10*1+rcx]  ; Const + dst + ...
	or	r11d,edx  ; x | ...
	xor	r11d,eax  ; y ^ ...
	add	ecx,r11d  ; dst += ...
	mov	r10d,DWORD[20+rsi]  ; (NEXT STEP) X[5]
	mov	r11d,0xffffffff
	rol	ecx,15  ; dst <<< s
	xor	r11d,eax  ; (NEXT STEP) not z' = not %eax
	add	ecx,edx  ; dst += x
	lea	ebx,[((-57434055))+r10*1+rbx]  ; Const + dst + ...
	or	r11d,ecx  ; x | ...
	xor	r11d,edx  ; y ^ ...
	add	ebx,r11d  ; dst += ...
	mov	r10d,DWORD[48+rsi]  ; (NEXT STEP) X[12]
	mov	r11d,0xffffffff
	rol	ebx,21  ; dst <<< s
	xor	r11d,edx  ; (NEXT STEP) not z' = not %edx
	add	ebx,ecx  ; dst += x
	lea	eax,[1700485571+r10*1+rax]  ; Const + dst + ...
	or	r11d,ebx  ; x | ...
	xor	r11d,ecx  ; y ^ ...
	add	eax,r11d  ; dst += ...
	mov	r10d,DWORD[12+rsi]  ; (NEXT STEP) X[3]
	mov	r11d,0xffffffff
	rol	eax,6  ; dst <<< s
	xor	r11d,ecx  ; (NEXT STEP) not z' = not %ecx
	add	eax,ebx  ; dst += x
	lea	edx,[((-1894986606))+r10*1+rdx]  ; Const + dst + ...
	or	r11d,eax  ; x | ...
	xor	r11d,ebx  ; y ^ ...
	add	edx,r11d  ; dst += ...
	mov	r10d,DWORD[40+rsi]  ; (NEXT STEP) X[10]
	mov	r11d,0xffffffff
	rol	edx,10  ; dst <<< s
	xor	r11d,ebx  ; (NEXT STEP) not z' = not %ebx
	add	edx,eax  ; dst += x
	lea	ecx,[((-1051523))+r10*1+rcx]  ; Const + dst + ...
	or	r11d,edx  ; x | ...
	xor	r11d,eax  ; y ^ ...
	add	ecx,r11d  ; dst += ...
	mov	r10d,DWORD[4+rsi]  ; (NEXT STEP) X[1]
	mov	r11d,0xffffffff
	rol	ecx,15  ; dst <<< s
	xor	r11d,eax  ; (NEXT STEP) not z' = not %eax
	add	ecx,edx  ; dst += x
	lea	ebx,[((-2054922799))+r10*1+rbx]  ; Const + dst + ...
	or	r11d,ecx  ; x | ...
	xor	r11d,edx  ; y ^ ...
	add	ebx,r11d  ; dst += ...
	mov	r10d,DWORD[32+rsi]  ; (NEXT STEP) X[8]
	mov	r11d,0xffffffff
	rol	ebx,21  ; dst <<< s
	xor	r11d,edx  ; (NEXT STEP) not z' = not %edx
	add	ebx,ecx  ; dst += x
	lea	eax,[1873313359+r10*1+rax]  ; Const + dst + ...
	or	r11d,ebx  ; x | ...
	xor	r11d,ecx  ; y ^ ...
	add	eax,r11d  ; dst += ...
	mov	r10d,DWORD[60+rsi]  ; (NEXT STEP) X[15]
	mov	r11d,0xffffffff
	rol	eax,6  ; dst <<< s
	xor	r11d,ecx  ; (NEXT STEP) not z' = not %ecx
	add	eax,ebx  ; dst += x
	lea	edx,[((-30611744))+r10*1+rdx]  ; Const + dst + ...
	or	r11d,eax  ; x | ...
	xor	r11d,ebx  ; y ^ ...
	add	edx,r11d  ; dst += ...
	mov	r10d,DWORD[24+rsi]  ; (NEXT STEP) X[6]
	mov	r11d,0xffffffff
	rol	edx,10  ; dst <<< s
	xor	r11d,ebx  ; (NEXT STEP) not z' = not %ebx
	add	edx,eax  ; dst += x
	lea	ecx,[((-1560198380))+r10*1+rcx]  ; Const + dst + ...
	or	r11d,edx  ; x | ...
	xor	r11d,eax  ; y ^ ...
	add	ecx,r11d  ; dst += ...
	mov	r10d,DWORD[52+rsi]  ; (NEXT STEP) X[13]
	mov	r11d,0xffffffff
	rol	ecx,15  ; dst <<< s
	xor	r11d,eax  ; (NEXT STEP) not z' = not %eax
	add	ecx,edx  ; dst += x
	lea	ebx,[1309151649+r10*1+rbx]  ; Const + dst + ...
	or	r11d,ecx  ; x | ...
	xor	r11d,edx  ; y ^ ...
	add	ebx,r11d  ; dst += ...
	mov	r10d,DWORD[16+rsi]  ; (NEXT STEP) X[4]
	mov	r11d,0xffffffff
	rol	ebx,21  ; dst <<< s
	xor	r11d,edx  ; (NEXT STEP) not z' = not %edx
	add	ebx,ecx  ; dst += x
	lea	eax,[((-145523070))+r10*1+rax]  ; Const + dst + ...
	or	r11d,ebx  ; x | ...
	xor	r11d,ecx  ; y ^ ...
	add	eax,r11d  ; dst += ...
	mov	r10d,DWORD[44+rsi]  ; (NEXT STEP) X[11]
	mov	r11d,0xffffffff
	rol	eax,6  ; dst <<< s
	xor	r11d,ecx  ; (NEXT STEP) not z' = not %ecx
	add	eax,ebx  ; dst += x
	lea	edx,[((-1120210379))+r10*1+rdx]  ; Const + dst + ...
	or	r11d,eax  ; x | ...
	xor	r11d,ebx  ; y ^ ...
	add	edx,r11d  ; dst += ...
	mov	r10d,DWORD[8+rsi]  ; (NEXT STEP) X[2]
	mov	r11d,0xffffffff
	rol	edx,10  ; dst <<< s
	xor	r11d,ebx  ; (NEXT STEP) not z' = not %ebx
	add	edx,eax  ; dst += x
	lea	ecx,[718787259+r10*1+rcx]  ; Const + dst + ...
	or	r11d,edx  ; x | ...
	xor	r11d,eax  ; y ^ ...
	add	ecx,r11d  ; dst += ...
	mov	r10d,DWORD[36+rsi]  ; (NEXT STEP) X[9]
	mov	r11d,0xffffffff
	rol	ecx,15  ; dst <<< s
	xor	r11d,eax  ; (NEXT STEP) not z' = not %eax
	add	ecx,edx  ; dst += x
	lea	ebx,[((-343485551))+r10*1+rbx]  ; Const + dst + ...
	or	r11d,ecx  ; x | ...
	xor	r11d,edx  ; y ^ ...
	add	ebx,r11d  ; dst += ...
	mov	r10d,DWORD[rsi]  ; (NEXT STEP) X[0]
	mov	r11d,0xffffffff
	rol	ebx,21  ; dst <<< s
	xor	r11d,edx  ; (NEXT STEP) not z' = not %edx
	add	ebx,ecx  ; dst += x
; add old values of A, B, C, D
	add	eax,r8d
	add	ebx,r9d
	add	ecx,r14d
	add	edx,r15d

; loop control
	add	rsi,64  ; ptr += 64
	cmp	rsi,rdi  ; cmp end with ptr
	jb	NEAR $L$loop  ; jmp if ptr < end
; END of loop over 16-word blocks

$L$end:
	mov	DWORD[rbp],eax  ; ctx->A = A
	mov	DWORD[4+rbp],ebx  ; ctx->B = B
	mov	DWORD[8+rbp],ecx  ; ctx->C = C
	mov	DWORD[12+rbp],edx  ; ctx->D = D

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r12,QWORD[16+rsp]

	mov	rbx,QWORD[24+rsp]

	mov	rbp,QWORD[32+rsp]

	add	rsp,40

$L$epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_md5_block_asm_data_order:
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

	lea	r10,[$L$prologue]
	cmp	rbx,r10  ; context->Rip<.Lprologue
	jb	NEAR $L$in_prologue

	mov	rax,QWORD[152+r8]  ; pull context->Rsp

	lea	r10,[$L$epilogue]
	cmp	rbx,r10  ; context->Rip>=.Lepilogue
	jae	NEAR $L$in_prologue

	lea	rax,[40+rax]

	mov	rbp,QWORD[((-8))+rax]
	mov	rbx,QWORD[((-16))+rax]
	mov	r12,QWORD[((-24))+rax]
	mov	r14,QWORD[((-32))+rax]
	mov	r15,QWORD[((-40))+rax]
	mov	QWORD[144+r8],rbx  ; restore context->Rbx
	mov	QWORD[160+r8],rbp  ; restore context->Rbp
	mov	QWORD[216+r8],r12  ; restore context->R12
	mov	QWORD[232+r8],r14  ; restore context->R14
	mov	QWORD[240+r8],r15  ; restore context->R15

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
	DD	$L$SEH_begin_md5_block_asm_data_order wrt ..imagebase
	DD	$L$SEH_end_md5_block_asm_data_order wrt ..imagebase
	DD	$L$SEH_info_md5_block_asm_data_order wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_md5_block_asm_data_order:
	DB	9,0,0,0
	DD	se_handler wrt ..imagebase
%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
