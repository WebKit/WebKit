[bits 64]
[section .text]
[default rel]

%line 1+0 ./some_filename
; program tst_wyjatki
global _start
_start:
  XOR EBP, EBP
  CALL main
  XOR RAX, RAX
  MOV RDI, RAX
  MOV RAX, 60
  SYSCALL
  HLT

main:
  PUSH rbp
  MOV rbp, rsp
  SUB RSP, 0x20
    %line 2+0 some_filename
    SUB RSP, 64
