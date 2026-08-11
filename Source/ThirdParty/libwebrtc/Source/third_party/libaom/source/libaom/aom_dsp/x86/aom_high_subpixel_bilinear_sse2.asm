;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved.
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

%include "aom_ports/x86_abi_support.asm"

%macro HIGH_GET_PARAM_4 0
    mov         rdx, arg(5)                 ;filter ptr
    mov         rsi, arg(0)                 ;src_ptr
    mov         rdi, arg(2)                 ;output_ptr
    mov         rcx, 0x00000040

    movdqa      xmm3, [rdx]                 ;load filters
    pshuflw     xmm4, xmm3, 11111111b       ;k3
    psrldq      xmm3, 8
    pshuflw     xmm3, xmm3, 0b              ;k4
    punpcklwd   xmm4, xmm3                  ;k3k4

    movq        xmm3, rcx                   ;rounding
    pshufd      xmm3, xmm3, 0

    mov         rdx, 0x00010001
    movsxd      rcx, DWORD PTR arg(6)       ;bps
    movq        xmm5, rdx
    movq        xmm2, rcx
    pshufd      xmm5, xmm5, 0b
    movdqa      xmm1, xmm5
    psllw       xmm5, xmm2
    psubw       xmm5, xmm1                  ;max value (for clamping)
    pxor        xmm2, xmm2                  ;min value (for clamping)

    movsxd      rax, DWORD PTR arg(1)       ;pixels_per_line
    movsxd      rdx, DWORD PTR arg(3)       ;out_pitch
    movsxd      rcx, DWORD PTR arg(4)       ;output_height
%endm

%macro HIGH_APPLY_FILTER_4 1

    punpcklwd   xmm0, xmm1                  ;two row in one register
    pmaddwd     xmm0, xmm4                  ;multiply the filter factors

    paddd       xmm0, xmm3                  ;rounding
    psrad       xmm0, 7                     ;shift
    packssdw    xmm0, xmm0                  ;pack to word

    ;clamp the values
    pminsw      xmm0, xmm5
    pmaxsw      xmm0, xmm2

%if %1
    movq        xmm1, [rdi]
    pavgw       xmm0, xmm1
%endif

    movq        [rdi], xmm0
    lea         rsi, [rsi + 2*rax]
    lea         rdi, [rdi + 2*rdx]
    dec         rcx
%endm

%macro HIGH_GET_PARAM 0
    mov         rdx, arg(5)                 ;filter ptr
    mov         rsi, arg(0)                 ;src_ptr
    mov         rdi, arg(2)                 ;output_ptr
    mov         rcx, 0x00000040

    movdqa      xmm6, [rdx]                 ;load filters

    pshuflw     xmm7, xmm6, 11111111b       ;k3
    pshufhw     xmm6, xmm6, 0b              ;k4
    psrldq      xmm6, 8
    punpcklwd   xmm7, xmm6                  ;k3k4k3k4k3k4k3k4

    movq        xmm4, rcx                   ;rounding
    pshufd      xmm4, xmm4, 0

    mov         rdx, 0x00010001
    movsxd      rcx, DWORD PTR arg(6)       ;bps
    movq        xmm3, rdx
    movq        xmm5, rcx
    pshufd      xmm3, xmm3, 0b
    movdqa      xmm1, xmm3
    psllw       xmm3, xmm5
    psubw       xmm3, xmm1                  ;max value (for clamping)
    pxor        xmm5, xmm5                  ;min value (for clamping)

    movdqa      max, xmm3
    movdqa      min, xmm5

    movsxd      rax, DWORD PTR arg(1)       ;pixels_per_line
    movsxd      rdx, DWORD PTR arg(3)       ;out_pitch
    movsxd      rcx, DWORD PTR arg(4)       ;output_height
%endm

%macro HIGH_APPLY_FILTER_8 1
    movdqa      xmm6, xmm0
    punpckhwd   xmm6, xmm1
    punpcklwd   xmm0, xmm1
    pmaddwd     xmm6, xmm7
    pmaddwd     xmm0, xmm7

    paddd       xmm6, xmm4                  ;rounding
    paddd       xmm0, xmm4                  ;rounding
    psrad       xmm6, 7                     ;shift
    psrad       xmm0, 7                     ;shift
    packssdw    xmm0, xmm6                  ;pack back to word

    ;clamp the values
    pminsw      xmm0, max
    pmaxsw      xmm0, min

%if %1
    movdqu      xmm1, [rdi]
    pavgw       xmm0, xmm1
%endif
    movdqu      [rdi], xmm0                 ;store the result

    lea         rsi, [rsi + 2*rax]
    lea         rdi, [rdi + 2*rdx]
    dec         rcx
%endm

%macro HIGH_APPLY_FILTER_16 1
    movdqa      xmm5, xmm0
    movdqa      xmm6, xmm2
    punpckhwd   xmm5, xmm1
    punpckhwd   xmm6, xmm3
    punpcklwd   xmm0, xmm1
    punpcklwd   xmm2, xmm3

    pmaddwd     xmm5, xmm7
    pmaddwd     xmm6, xmm7
    pmaddwd     xmm0, xmm7
    pmaddwd     xmm2, xmm7

    paddd       xmm5, xmm4                  ;rounding
    paddd       xmm6, xmm4
    paddd       xmm0, xmm4
    paddd       xmm2, xmm4

    psrad       xmm5, 7                     ;shift
    psrad       xmm6, 7
    psrad       xmm0, 7
    psrad       xmm2, 7

    packssdw    xmm0, xmm5                  ;pack back to word
    packssdw    xmm2, xmm6                  ;pack back to word

    ;clamp the values
    pminsw      xmm0, max
    pmaxsw      xmm0, min
    pminsw      xmm2, max
    pmaxsw      xmm2, min

%if %1
    movdqu      xmm1, [rdi]
    movdqu      xmm3, [rdi + 16]
    pavgw       xmm0, xmm1
    pavgw       xmm2, xmm3
%endif
    movdqu      [rdi], xmm0               ;store the result
    movdqu      [rdi + 16], xmm2          ;store the result

    lea         rsi, [rsi + 2*rax]
    lea         rdi, [rdi + 2*rdx]
    dec         rcx
%endm

SECTION .text

globalsym(aom_highbd_filter_block1d4_v2_sse2)
sym(aom_highbd_filter_block1d4_v2_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    push        rsi
    push        rdi
    ; end prolog

    HIGH_GET_PARAM_4
.loop:
    movq        xmm0, [rsi]                 ;load src
    movq        xmm1, [rsi + 2*rax]

    HIGH_APPLY_FILTER_4 0
    jnz         .loop

    ; begin epilog
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

globalsym(aom_highbd_filter_block1d8_v2_sse2)
sym(aom_highbd_filter_block1d8_v2_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 8
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 16 * 2
    %define max [rsp + 16 * 0]
    %define min [rsp + 16 * 1]

    HIGH_GET_PARAM
.loop:
    movdqu      xmm0, [rsi]                 ;0
    movdqu      xmm1, [rsi + 2*rax]         ;1

    HIGH_APPLY_FILTER_8 0
    jnz         .loop

    add rsp, 16 * 2
    pop rsp

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

globalsym(aom_highbd_filter_block1d16_v2_sse2)
sym(aom_highbd_filter_block1d16_v2_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 9
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 16 * 2
    %define max [rsp + 16 * 0]
    %define min [rsp + 16 * 1]

    HIGH_GET_PARAM
.loop:
    movdqu        xmm0, [rsi]               ;0
    movdqu        xmm2, [rsi + 16]
    movdqu        xmm1, [rsi + 2*rax]       ;1
    movdqu        xmm3, [rsi + 2*rax + 16]

    HIGH_APPLY_FILTER_16 0
    jnz         .loop

    add rsp, 16 * 2
    pop rsp

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

globalsym(aom_highbd_filter_block1d4_h2_sse2)
sym(aom_highbd_filter_block1d4_h2_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    push        rsi
    push        rdi
    ; end prolog

    HIGH_GET_PARAM_4
.loop:
    movdqu      xmm0, [rsi]                 ;load src
    movdqa      xmm1, xmm0
    psrldq      xmm1, 2

    HIGH_APPLY_FILTER_4 0
    jnz         .loop

    ; begin epilog
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

globalsym(aom_highbd_filter_block1d8_h2_sse2)
sym(aom_highbd_filter_block1d8_h2_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 8
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 16 * 2
    %define max [rsp + 16 * 0]
    %define min [rsp + 16 * 1]

    HIGH_GET_PARAM
.loop:
    movdqu      xmm0, [rsi]                 ;load src
    movdqu      xmm1, [rsi + 2]

    HIGH_APPLY_FILTER_8 0
    jnz         .loop

    add rsp, 16 * 2
    pop rsp

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

globalsym(aom_highbd_filter_block1d16_h2_sse2)
sym(aom_highbd_filter_block1d16_h2_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 9
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 16 * 2
    %define max [rsp + 16 * 0]
    %define min [rsp + 16 * 1]

    HIGH_GET_PARAM
.loop:
    movdqu      xmm0,   [rsi]               ;load src
    movdqu      xmm1,   [rsi + 2]
    movdqu      xmm2,   [rsi + 16]
    movdqu      xmm3,   [rsi + 18]

    HIGH_APPLY_FILTER_16 0
    jnz         .loop

    add rsp, 16 * 2
    pop rsp

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret
