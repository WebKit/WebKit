#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && \
    (defined(__APPLE__) || defined(__ELF__))

.intel_syntax noprefix
.text
#if defined(__APPLE__)
.private_extern _fiat_p256_adx_mul
.global _fiat_p256_adx_mul
_fiat_p256_adx_mul:
#else
.type fiat_p256_adx_mul, @function
.hidden fiat_p256_adx_mul
.global fiat_p256_adx_mul
fiat_p256_adx_mul:
#endif

.cfi_startproc
_CET_ENDBR
push rbp
.cfi_adjust_cfa_offset 8
.cfi_offset rbp, -16
mov rbp, rsp
mov rax, rdx
mov rdx, [ rsi + 0x0 ]
test al, al
mulx r8, rcx, [ rax + 0x0 ]
mov [ rsp - 0x80 ], rbx
.cfi_offset rbx, -16-0x80
mulx rbx, r9, [ rax + 0x8 ]
mov [ rsp - 0x68 ], r14
.cfi_offset r14, -16-0x68
adc r9, r8
mov [ rsp - 0x60 ], r15
.cfi_offset r15, -16-0x60
mulx r15, r14, [ rax + 0x10 ]
mov [ rsp - 0x78 ], r12
.cfi_offset r12, -16-0x78
adc r14, rbx
mulx r11, r10, [ rax + 0x18 ]
mov [ rsp - 0x70 ], r13
.cfi_offset r13, -16-0x70
adc r10, r15
mov rdx, [ rsi + 0x8 ]
mulx rbx, r8, [ rax + 0x0 ]
adc r11, 0x0
xor r15, r15
adcx r8, r9
adox rbx, r14
mov [ rsp - 0x58 ], rdi
mulx rdi, r9, [ rax + 0x8 ]
adcx r9, rbx
adox rdi, r10
mulx rbx, r14, [ rax + 0x10 ]
adcx r14, rdi
adox rbx, r11
mulx r13, r12, [ rax + 0x18 ]
adcx r12, rbx
mov rdx, 0x100000000
mulx r11, r10, rcx
adox r13, r15
adcx r13, r15
xor rdi, rdi
adox r10, r8
mulx r8, rbx, r10
adox r11, r9
adcx rbx, r11
adox r8, r14
mov rdx, 0xffffffff00000001
mulx r9, r15, rcx
adcx r15, r8
adox r9, r12
mulx r14, rcx, r10
mov rdx, [ rsi + 0x10 ]
mulx r10, r12, [ rax + 0x8 ]
adcx rcx, r9
adox r14, r13
mulx r11, r13, [ rax + 0x0 ]
mov r9, rdi
adcx r14, r9
adox rdi, rdi
adc rdi, 0x0
xor r9, r9
adcx r13, rbx
adox r11, r15
mov rdx, [ rsi + 0x10 ]
mulx r15, r8, [ rax + 0x10 ]
adox r10, rcx
mulx rcx, rbx, [ rax + 0x18 ]
mov rdx, [ rsi + 0x18 ]
adcx r12, r11
mulx rsi, r11, [ rax + 0x8 ]
adcx r8, r10
adox r15, r14
adcx rbx, r15
adox rcx, r9
adcx rcx, r9
mulx r15, r10, [ rax + 0x0 ]
add rcx, rdi
mov r14, r9
adc r14, 0
xor r9, r9
adcx r10, r12
adox r15, r8
adcx r11, r15
adox rsi, rbx
mulx r8, r12, [ rax + 0x10 ]
adox r8, rcx
mulx rcx, rbx, [ rax + 0x18 ]
adcx r12, rsi
adox rcx, r9
mov rdx, 0x100000000
adcx rbx, r8
adc rcx, 0
mulx rdi, r15, r13
xor rax, rax
adcx rcx, r14
adc rax, 0
xor r9, r9
adox r15, r10
mulx r14, r10, r15
adox rdi, r11
mov rdx, 0xffffffff00000001
adox r14, r12
adcx r10, rdi
mulx r12, r11, r13
adcx r11, r14
adox r12, rbx
mulx rbx, r13, r15
adcx r13, r12
adox rbx, rcx
mov r8, r9
adox rax, r9
adcx r8, rbx
adc rax, 0x0
mov rcx, rax
mov r15, 0xffffffffffffffff
mov rdi, r10
sub rdi, r15
mov r14, 0xffffffff
mov r12, r11
sbb r12, r14
mov rbx, r13
sbb rbx, r9
mov rax, rax
mov rax, r8
sbb rax, rdx
sbb rcx, r9
cmovc rdi, r10
mov r10, [ rsp - 0x58 ]
cmovc rbx, r13
mov r13, [ rsp - 0x70 ]
.cfi_restore r13
cmovc r12, r11
cmovc rax, r8
mov [ r10 + 0x10 ], rbx
mov rbx, [ rsp - 0x80 ]
.cfi_restore rbx
mov [ r10 + 0x0 ], rdi
mov [ r10 + 0x8 ], r12
mov [ r10 + 0x18 ], rax
mov r12, [ rsp - 0x78 ]
.cfi_restore r12
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
.size fiat_p256_adx_mul, .-fiat_p256_adx_mul
#endif

#endif
