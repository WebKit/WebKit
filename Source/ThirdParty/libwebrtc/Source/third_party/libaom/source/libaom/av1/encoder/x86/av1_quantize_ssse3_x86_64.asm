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

%define private_prefix av1

%include "third_party/x86inc/x86inc.asm"

SECTION_RODATA
pw_1: times 8 dw 1

SECTION .text

%macro QUANTIZE_FP 2
cglobal quantize_%1, 0, %2, 15, coeff, ncoeff, skip, zbin, round, quant, \
                                shift, qcoeff, dqcoeff, dequant, \
                                eob, scan, iscan
  cmp                    dword skipm, 0
  jne .blank

  ; actual quantize loop - setup pointers, rounders, etc.
  movifnidn                   coeffq, coeffmp
  movifnidn                  ncoeffq, ncoeffmp
  mov                             r2, dequantmp
  movifnidn                    zbinq, zbinmp
  movifnidn                   roundq, roundmp
  movifnidn                   quantq, quantmp
  mova                            m1, [roundq]             ; m1 = round
  mova                            m2, [quantq]             ; m2 = quant
%ifidn %1, fp_32x32
  pcmpeqw                         m5, m5
  psrlw                           m5, 15
  paddw                           m1, m5
  psrlw                           m1, 1                    ; m1 = (m1 + 1) / 2
%endif
  mova                            m3, [r2q]                ; m3 = dequant
  mov                             r3, qcoeffmp
  mov                             r4, dqcoeffmp
  mov                             r5, iscanmp
%ifidn %1, fp_32x32
  psllw                           m2, 1
%endif
  pxor                            m5, m5                   ; m5 = dedicated zero

  lea                         coeffq, [  coeffq+ncoeffq*2]
  lea                            r5q, [  r5q+ncoeffq*2]
  lea                            r3q, [ r3q+ncoeffq*2]
  lea                            r4q, [r4q+ncoeffq*2]
  neg                        ncoeffq

  ; get DC and first 15 AC coeffs
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpeqw                         m7, m7

  paddsw                          m6, m1                   ; m6 += round
  punpckhqdq                      m1, m1
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                          m8, m6, m2               ; m8 = m6*q>>16
  punpckhqdq                      m2, m2
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  psignw                          m8, m9                   ; m8 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  mova            [r3q+ncoeffq*2+ 0], m8
  mova            [r3q+ncoeffq*2+16], m13
%ifidn %1, fp_32x32
  pabsw                           m8, m8
  pabsw                          m13, m13
%endif
  pmullw                          m8, m3                   ; r4[i] = r3[i] * q
  punpckhqdq                      m3, m3
  pmullw                         m13, m3                   ; r4[i] = r3[i] * q
%ifidn %1, fp_32x32
  psrlw                           m8, 1
  psrlw                          m13, 1
  psignw                          m8, m9
  psignw                         m13, m10
  psrlw                           m0, m3, 2
%else
  psrlw                           m0, m3, 1
%endif
  mova            [r4q+ncoeffq*2+ 0], m8
  mova            [r4q+ncoeffq*2+16], m13
  pcmpeqw                         m8, m5                   ; m8 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  r5q+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  r5q+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m7                   ; m11 = scan[i] + 1
  pandn                           m8, m6                   ; m8 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jz .accumulate_eob

.ac_only_loop:
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)

  pcmpgtw                         m7, m6,  m0
  pcmpgtw                        m12, m11, m0
  pmovmskb                       r6d, m7
  pmovmskb                       r2d, m12

  or                              r6, r2
  jz .skip_iter

  pcmpeqw                         m7, m7

  paddsw                          m6, m1                   ; m6 += round
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                         m14, m6, m2               ; m14 = m6*q>>16
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  psignw                         m14, m9                   ; m14 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  mova            [r3q+ncoeffq*2+ 0], m14
  mova            [r3q+ncoeffq*2+16], m13
%ifidn %1, fp_32x32
  pabsw                          m14, m14
  pabsw                          m13, m13
%endif
  pmullw                         m14, m3                   ; r4[i] = r3[i] * q
  pmullw                         m13, m3                   ; r4[i] = r3[i] * q
%ifidn %1, fp_32x32
  psrlw                          m14, 1
  psrlw                          m13, 1
  psignw                         m14, m9
  psignw                         m13, m10
%endif
  mova            [r4q+ncoeffq*2+ 0], m14
  mova            [r4q+ncoeffq*2+16], m13
  pcmpeqw                        m14, m5                   ; m14 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  r5q+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  r5q+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m7                   ; m11 = scan[i] + 1
  pandn                          m14, m6                   ; m14 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m14
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jl .ac_only_loop

  jmp .accumulate_eob
.skip_iter:
  mova            [r3q+ncoeffq*2+ 0], m5
  mova            [r3q+ncoeffq*2+16], m5
  mova            [r4q+ncoeffq*2+ 0], m5
  mova            [r4q+ncoeffq*2+16], m5
  add                        ncoeffq, mmsize
  jl .ac_only_loop

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
  mov                           [r2], r6
  RET

  ; skip-block, i.e. just write all zeroes
.blank:
  mov                             r0, dqcoeffmp
  movifnidn                  ncoeffq, ncoeffmp
  mov                             r2, qcoeffmp
  mov                             r3, eobmp

  lea                            r0q, [r0q+ncoeffq*2]
  lea                            r2q, [r2q+ncoeffq*2]
  neg                        ncoeffq
  pxor                            m7, m7
.blank_loop:
  mova            [r0q+ncoeffq*2+ 0], m7
  mova            [r0q+ncoeffq*2+16], m7
  mova            [r2q+ncoeffq*2+ 0], m7
  mova            [r2q+ncoeffq*2+16], m7
  add                        ncoeffq, mmsize
  jl .blank_loop
  mov                     word [r3q], 0
  RET
%endmacro

INIT_XMM ssse3
QUANTIZE_FP fp, 7
QUANTIZE_FP fp_32x32, 7
