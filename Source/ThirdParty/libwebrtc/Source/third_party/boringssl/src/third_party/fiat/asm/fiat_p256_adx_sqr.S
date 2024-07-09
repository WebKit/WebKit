#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && \
    (defined(__APPLE__) || defined(__ELF__))

.intel_syntax noprefix
.text
#if defined(__APPLE__)
.private_extern _fiat_p256_adx_sqr
.global _fiat_p256_adx_sqr
_fiat_p256_adx_sqr:
#else
.type fiat_p256_adx_sqr, @function
.hidden fiat_p256_adx_sqr
.global fiat_p256_adx_sqr
fiat_p256_adx_sqr:
#endif

.cfi_startproc
_CET_ENDBR
push rbp
.cfi_adjust_cfa_offset 8
.cfi_offset rbp, -16
mov rbp, rsp
mov rdx, [ rsi + 0x0 ]
mulx r10, rax, [ rsi + 0x18 ]
mulx rcx, r11, rdx
mulx r9, r8, [ rsi + 0x8 ]
mov [ rsp - 0x80 ], rbx
.cfi_offset rbx, -16-0x80
xor rbx, rbx
adox r8, r8
mov [ rsp - 0x78 ], r12
.cfi_offset r12, -16-0x78
mulx r12, rbx, [ rsi + 0x10 ]
mov rdx, [ rsi + 0x8 ]
mov [ rsp - 0x70 ], r13
.cfi_offset r13, -16-0x70
mov [ rsp - 0x68 ], r14
.cfi_offset r14, -16-0x68
mulx r14, r13, rdx
mov [ rsp - 0x60 ], r15
.cfi_offset r15, -16-0x60
mov [ rsp - 0x58 ], rdi
mulx rdi, r15, [ rsi + 0x10 ]
adcx r12, r15
mov [ rsp - 0x50 ], r11
mulx r11, r15, [ rsi + 0x18 ]
adcx r10, rdi
mov rdi, 0x0
adcx r11, rdi
clc
adcx rbx, r9
adox rbx, rbx
adcx rax, r12
adox rax, rax
adcx r15, r10
adox r15, r15
mov rdx, [ rsi + 0x10 ]
mulx r12, r9, [ rsi + 0x18 ]
adcx r9, r11
adcx r12, rdi
mulx r11, r10, rdx
clc
adcx rcx, r8
adcx r13, rbx
adcx r14, rax
adox r9, r9
adcx r10, r15
mov rdx, [ rsi + 0x18 ]
mulx rbx, r8, rdx
adox r12, r12
adcx r11, r9
mov rsi, [ rsp - 0x50 ]
adcx r8, r12
mov rax, 0x100000000
mov rdx, rax
mulx r15, rax, rsi
adcx rbx, rdi
adox rbx, rdi
xor r9, r9
adox rax, rcx
adox r15, r13
mulx rcx, rdi, rax
adcx rdi, r15
adox rcx, r14
mov rdx, 0xffffffff00000001
mulx r14, r13, rsi
adox r14, r10
adcx r13, rcx
mulx r12, r10, rax
adox r12, r11
mov r11, r9
adox r11, r8
adcx r10, r14
mov r8, r9
adcx r8, r12
mov rax, r9
adcx rax, r11
mov r15, r9
adox r15, rbx
mov rdx, 0x100000000
mulx rcx, rbx, rdi
mov r14, r9
adcx r14, r15
mov r12, r9
adox r12, r12
adcx r12, r9
adox rbx, r13
mulx r11, r13, rbx
mov r15, 0xffffffff00000001
mov rdx, r15
mulx rsi, r15, rbx
adox rcx, r10
adox r11, r8
mulx r8, r10, rdi
adcx r13, rcx
adox r8, rax
adcx r10, r11
adox rsi, r14
mov rdi, r12
mov rax, r9
adox rdi, rax
adcx r15, r8
mov r14, rax
adcx r14, rsi
adcx rdi, r9
dec r9
mov rbx, r13
sub rbx, r9
mov rcx, 0xffffffff
mov r11, r10
sbb r11, rcx
mov r8, r15
sbb r8, rax
mov rsi, r14
sbb rsi, rdx
sbb rdi, rax
cmovc rbx, r13
cmovc r8, r15
cmovc r11, r10
cmovc rsi, r14
mov rdi, [ rsp - 0x58 ]
mov [ rdi + 0x18 ], rsi
mov [ rdi + 0x0 ], rbx
mov [ rdi + 0x8 ], r11
mov [ rdi + 0x10 ], r8
mov rbx, [ rsp - 0x80 ]
.cfi_restore rbx
mov r12, [ rsp - 0x78 ]
.cfi_restore r12
mov r13, [ rsp - 0x70 ]
.cfi_restore r13
mov r14, [ rsp - 0x68 ]
.cfi_restore r14
mov r15, [ rsp - 0x60 ]
.cfi_restore r15
pop rbp
.cfi_restore rbp
.cfi_adjust_cfa_offset -8
ret
.cfi_endproc
#if defined(__ELF__)
.size fiat_p256_adx_sqr, .-fiat_p256_adx_sqr
#endif

#endif
