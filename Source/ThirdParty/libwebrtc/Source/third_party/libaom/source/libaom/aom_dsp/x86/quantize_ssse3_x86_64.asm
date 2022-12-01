;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

%include "third_party/x86inc/x86inc.asm"

SECTION_RODATA
pw_1: times 8 dw 1

SECTION .text

%macro QUANTIZE_FN 2
cglobal quantize_%1, 0, %2, 15, coeff, ncoeff, zbin, round, quant, \
                                shift, qcoeff, dqcoeff, dequant, \
                                eob, scan, iscan

  ; actual quantize loop - setup pointers, rounders, etc.
  movifnidn                   coeffq, coeffmp
  movifnidn                  ncoeffq, ncoeffmp
  movifnidn                    zbinq, zbinmp
  movifnidn                   roundq, roundmp
  movifnidn                   quantq, quantmp
  movifnidn                 dequantq, dequantmp
  mova                            m0, [zbinq]              ; m0 = zbin
  mova                            m1, [roundq]             ; m1 = round
  mova                            m2, [quantq]             ; m2 = quant
%ifidn %1, b_32x32
  pcmpeqw                         m5, m5
  psrlw                           m5, 15
  paddw                           m0, m5
  paddw                           m1, m5
  psrlw                           m0, 1                    ; m0 = (m0 + 1) / 2
  psrlw                           m1, 1                    ; m1 = (m1 + 1) / 2
%endif
  mova                            m3, [dequantq]           ; m3 = dequant
  mov                             r2, shiftmp
  psubw                           m0, [GLOBAL(pw_1)]
  mova                            m4, [r2]                 ; m4 = shift
  mov                             r3, qcoeffmp
  mov                             r4, dqcoeffmp
  mov                             r5, iscanmp
  pxor                            m5, m5                   ; m5 = dedicated zero
  DEFINE_ARGS coeff, ncoeff, d1, qcoeff, dqcoeff, iscan, d2, d3, d4, eob
  lea                         coeffq, [  coeffq+ncoeffq*4]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*4]
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*4]
  lea                         iscanq, [  iscanq+ncoeffq*2]
  neg                        ncoeffq

  ; get DC and first 15 AC coeffs
  ; coeff stored as 32bit numbers & require 16bit numbers
  mova                            m9, [  coeffq+ncoeffq*4+ 0]
  packssdw                        m9, [  coeffq+ncoeffq*4+16]
  mova                           m10, [  coeffq+ncoeffq*4+32]
  packssdw                       m10, [  coeffq+ncoeffq*4+48]
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  punpckhqdq                      m0, m0
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin
  paddsw                          m6, m1                   ; m6 += round
  punpckhqdq                      m1, m1
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                          m8, m6, m2               ; m8 = m6*q>>16
  punpckhqdq                      m2, m2
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  paddw                           m8, m6                   ; m8 += m6
  paddw                          m13, m11                  ; m13 += m11
  %ifidn %1, b_32x32
  pmullw                          m5, m8, m4               ; store the lower 16 bits of m8*qsh
  %endif
  pmulhw                          m8, m4                   ; m8 = m8*qsh>>16
  %ifidn %1, b_32x32
  psllw                           m8, 1
  psrlw                           m5, 15
  por                             m8, m5
  %endif
  punpckhqdq                      m4, m4
  %ifidn %1, b_32x32
  pmullw                          m5, m13, m4              ; store the lower 16 bits of m13*qsh
  %endif
  pmulhw                         m13, m4                   ; m13 = m13*qsh>>16
  %ifidn %1, b_32x32
  psllw                          m13, 1
  psrlw                           m5, 15
  por                            m13, m5
  pxor                            m5, m5                   ; reset m5 to zero register
  %endif
  psignw                          m8, m9                   ; m8 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  pand                            m8, m7
  pand                           m13, m12

  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  mova                           m11, m8
  mova                            m6, m8
  pcmpgtw                         m5, m8
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+ 0], m11
  mova        [qcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+32], m11
  mova        [qcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5             ; reset m5 to zero register

%ifidn %1, b_32x32
  pabsw                           m8, m8
  pabsw                          m13, m13
%endif
  pmullw                          m8, m3                   ; dqc[i] = qc[i] * q
  punpckhqdq                      m3, m3
  pmullw                         m13, m3                   ; dqc[i] = qc[i] * q
%ifidn %1, b_32x32
  psrlw                           m8, 1
  psrlw                          m13, 1
  psignw                          m8, m9
  psignw                         m13, m10
%endif
  ; store 16bit numbers as 32bit numbers in array pointed to by dqcoeff
  mova                            m11, m8
  mova                            m6, m8
  pcmpgtw                         m5, m8
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+ 0], m11
  mova       [dqcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+32], m11
  mova       [dqcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5             ; reset m5 to zero register
  pcmpeqw                         m8, m5                   ; m8 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  iscanq+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m12                  ; m11 = scan[i] + 1
  pandn                           m8, m6                   ; m8 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jz .accumulate_eob

.ac_only_loop:
  ; pack coeff from 32bit to 16bit array
  mova                            m9, [  coeffq+ncoeffq*4+ 0]
  packssdw                        m9, [  coeffq+ncoeffq*4+16]
  mova                           m10, [  coeffq+ncoeffq*4+32]
  packssdw                       m10, [  coeffq+ncoeffq*4+48]

  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin
%ifidn %1, b_32x32
  pmovmskb                       r6d, m7
  pmovmskb                       r2d, m12
  or                              r6, r2
  jz .skip_iter
%endif
  paddsw                          m6, m1                   ; m6 += round
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                         m14, m6, m2               ; m14 = m6*q>>16
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  paddw                          m14, m6                   ; m14 += m6
  paddw                          m13, m11                  ; m13 += m11
  %ifidn %1, b_32x32
  pmullw                          m5, m14, m4              ; store the lower 16 bits of m14*qsh
  %endif
  pmulhw                         m14, m4                   ; m14 = m14*qsh>>16
  %ifidn %1, b_32x32
  psllw                          m14, 1
  psrlw                           m5, 15
  por                            m14, m5
  pmullw                          m5, m13, m4              ; store the lower 16 bits of m13*qsh
  %endif
  pmulhw                         m13, m4                   ; m13 = m13*qsh>>16
  %ifidn %1, b_32x32
  psllw                          m13, 1
  psrlw                           m5, 15
  por                            m13, m5
  pxor                            m5, m5                   ; reset m5 to zero register
  %endif
  psignw                         m14, m9                   ; m14 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  pand                           m14, m7
  pand                           m13, m12

  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pxor                           m11, m11
  mova                           m11, m14
  mova                            m6, m14
  pcmpgtw                         m5, m14
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+ 0], m11
  mova        [qcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+32], m11
  mova        [qcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5             ; reset m5 to zero register

%ifidn %1, b_32x32
  pabsw                          m14, m14
  pabsw                          m13, m13
%endif
  pmullw                         m14, m3                   ; dqc[i] = qc[i] * q
  pmullw                         m13, m3                   ; dqc[i] = qc[i] * q
%ifidn %1, b_32x32
  psrlw                          m14, 1
  psrlw                          m13, 1
  psignw                         m14, m9
  psignw                         m13, m10
%endif

  ; store 16bit numbers as 32bit numbers in array pointed to by dqcoeff
  mova                           m11, m14
  mova                            m6, m14
  pcmpgtw                         m5, m14
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+ 0], m11
  mova       [dqcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+32], m11
  mova       [dqcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5

  pcmpeqw                        m14, m5                   ; m14 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  iscanq+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m12                  ; m11 = scan[i] + 1
  pandn                          m14, m6                   ; m14 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m14
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jl .ac_only_loop

%ifidn %1, b_32x32
  jmp .accumulate_eob
.skip_iter:
  mova        [qcoeffq+ncoeffq*4+ 0], m5
  mova        [qcoeffq+ncoeffq*4+16], m5
  mova        [qcoeffq+ncoeffq*4+32], m5
  mova        [qcoeffq+ncoeffq*4+48], m5
  mova       [dqcoeffq+ncoeffq*4+ 0], m5
  mova       [dqcoeffq+ncoeffq*4+16], m5
  mova       [dqcoeffq+ncoeffq*4+32], m5
  mova       [dqcoeffq+ncoeffq*4+48], m5
  add                        ncoeffq, mmsize
  jl .ac_only_loop
%endif

.accumulate_eob:
  ; horizontally accumulate/max eobs and write into [eob] memory pointer
  mov                             r2, eobmp
  pshufd                          m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0x1
  pmaxsw                          m8, m7
  pextrw                          r6, m8, 0
  mov                             [r2], r6
  RET
%endmacro

INIT_XMM ssse3
QUANTIZE_FN b, 9
QUANTIZE_FN b_32x32, 9
