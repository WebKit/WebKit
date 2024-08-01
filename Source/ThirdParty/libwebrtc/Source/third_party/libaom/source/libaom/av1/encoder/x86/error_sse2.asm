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

; Increment %1 by sizeof() tran_low_t * %2.
%macro INCREMENT_ELEMENTS_TRAN_LOW 2
  lea %1, [%1 + %2 * 4]
%endmacro

; Load %2 + %3 into m%1.
; %3 is the offset in elements, not bytes.
; If tran_low_t is 16 bits (low bit depth configuration) then load the value
; directly. If tran_low_t is 32 bits (high bit depth configuration) then pack
; the values down to 16 bits.
%macro LOAD_TRAN_LOW 3
  mova     m%1, [%2 + (%3) * 4]
  packssdw m%1, [%2 + (%3) * 4 + 16]
%endmacro

%define private_prefix av1

%include "third_party/x86inc/x86inc.asm"

SECTION .text

; int64_t av1_block_error(int16_t *coeff, int16_t *dqcoeff, intptr_t block_size,
;                         int64_t *ssz)

INIT_XMM sse2
cglobal block_error, 3, 3, 8, uqc, dqc, size, ssz
  pxor      m4, m4                 ; sse accumulator
  pxor      m6, m6                 ; ssz accumulator
  pxor      m5, m5                 ; dedicated zero register
.loop:
  LOAD_TRAN_LOW 2, uqcq, 0
  LOAD_TRAN_LOW 0, dqcq, 0
  LOAD_TRAN_LOW 3, uqcq, 8
  LOAD_TRAN_LOW 1, dqcq, 8
  INCREMENT_ELEMENTS_TRAN_LOW uqcq, 16
  INCREMENT_ELEMENTS_TRAN_LOW dqcq, 16
  sub    sizeq, 16
  psubw     m0, m2
  psubw     m1, m3
  ; individual errors are max. 15bit+sign, so squares are 30bit, and
  ; thus the sum of 2 should fit in a 31bit integer (+ unused sign bit)
  pmaddwd   m0, m0
  pmaddwd   m1, m1
  pmaddwd   m2, m2
  pmaddwd   m3, m3
  ; the sum of 2 31bit integers will fit in a 32bit unsigned integer
  paddd     m0, m1
  paddd     m2, m3
  ; accumulate in 64bit
  punpckldq m7, m0, m5
  punpckhdq m0, m5
  paddq     m4, m7
  punpckldq m7, m2, m5
  paddq     m4, m0
  punpckhdq m2, m5
  paddq     m6, m7
  paddq     m6, m2
  jg .loop

  ; accumulate horizontally and store in return value
  movhlps   m5, m4
  movhlps   m7, m6
  paddq     m4, m5
  paddq     m6, m7
%if AOM_ARCH_X86_64
  movq    rax, m4
  movq [sszq], m6
%else
  mov     eax, sszm
  pshufd   m5, m4, 0x1
  movq  [eax], m6
  movd    eax, m4
  movd    edx, m5
%endif
  RET
