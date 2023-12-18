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

SECTION .text

; void aom_subtract_block(int rows, int cols,
;                         int16_t *diff, ptrdiff_t diff_stride,
;                         const uint8_t *src, ptrdiff_t src_stride,
;                         const uint8_t *pred, ptrdiff_t pred_stride)

INIT_XMM sse2
cglobal subtract_block, 7, 7, 8, \
                        rows, cols, diff, diff_stride, src, src_stride, \
                        pred, pred_stride
%define pred_str colsq
  pxor                  m7, m7         ; dedicated zero register
  cmp                colsd, 4
  je .case_4
  cmp                colsd, 8
  je .case_8
  cmp                colsd, 16
  je .case_16
  cmp                colsd, 32
  je .case_32
  cmp                colsd, 64
  je .case_64

%macro loop16 6
  mova                  m0, [srcq+%1]
  mova                  m4, [srcq+%2]
  movu                  m1, [predq+%3]
  movu                  m5, [predq+%4]
  punpckhbw             m2, m0, m7
  punpckhbw             m3, m1, m7
  punpcklbw             m0, m7
  punpcklbw             m1, m7
  psubw                 m2, m3
  psubw                 m0, m1
  punpckhbw             m1, m4, m7
  punpckhbw             m3, m5, m7
  punpcklbw             m4, m7
  punpcklbw             m5, m7
  psubw                 m1, m3
  psubw                 m4, m5
  mova [diffq+mmsize*0+%5], m0
  mova [diffq+mmsize*1+%5], m2
  mova [diffq+mmsize*0+%6], m4
  mova [diffq+mmsize*1+%6], m1
%endmacro

  mov             pred_str, pred_stridemp
.loop_128:
  loop16 0*mmsize, 1*mmsize, 0*mmsize, 1*mmsize,  0*mmsize,  2*mmsize
  loop16 2*mmsize, 3*mmsize, 2*mmsize, 3*mmsize,  4*mmsize,  6*mmsize
  loop16 4*mmsize, 5*mmsize, 4*mmsize, 5*mmsize,  8*mmsize, 10*mmsize
  loop16 6*mmsize, 7*mmsize, 6*mmsize, 7*mmsize, 12*mmsize, 14*mmsize
  lea                diffq, [diffq+diff_strideq*2]
  add                predq, pred_str
  add                 srcq, src_strideq
  sub                rowsd, 1
  jnz .loop_128
  RET

.case_64:
  mov             pred_str, pred_stridemp
.loop_64:
  loop16 0*mmsize, 1*mmsize, 0*mmsize, 1*mmsize, 0*mmsize, 2*mmsize
  loop16 2*mmsize, 3*mmsize, 2*mmsize, 3*mmsize, 4*mmsize, 6*mmsize
  lea                diffq, [diffq+diff_strideq*2]
  add                predq, pred_str
  add                 srcq, src_strideq
  dec                rowsd
  jg .loop_64
  RET

.case_32:
  mov             pred_str, pred_stridemp
.loop_32:
  loop16 0, mmsize, 0, mmsize, 0, 2*mmsize
  lea                diffq, [diffq+diff_strideq*2]
  add                predq, pred_str
  add                 srcq, src_strideq
  dec                rowsd
  jg .loop_32
  RET

.case_16:
  mov             pred_str, pred_stridemp
.loop_16:
  loop16 0, src_strideq, 0, pred_str, 0, diff_strideq*2
  lea                diffq, [diffq+diff_strideq*4]
  lea                predq, [predq+pred_str*2]
  lea                 srcq, [srcq+src_strideq*2]
  sub                rowsd, 2
  jg .loop_16
  RET

%macro loop_h 0
  movh                  m0, [srcq]
  movh                  m2, [srcq+src_strideq]
  movh                  m1, [predq]
  movh                  m3, [predq+pred_str]
  punpcklbw             m0, m7
  punpcklbw             m1, m7
  punpcklbw             m2, m7
  punpcklbw             m3, m7
  psubw                 m0, m1
  psubw                 m2, m3
  mova             [diffq], m0
  mova [diffq+diff_strideq*2], m2
%endmacro

.case_8:
  mov             pred_str, pred_stridemp
.loop_8:
  loop_h
  lea                diffq, [diffq+diff_strideq*4]
  lea                 srcq, [srcq+src_strideq*2]
  lea                predq, [predq+pred_str*2]
  sub                rowsd, 2
  jg .loop_8
  RET

INIT_MMX
.case_4:
  mov             pred_str, pred_stridemp
.loop_4:
  loop_h
  lea                diffq, [diffq+diff_strideq*4]
  lea                 srcq, [srcq+src_strideq*2]
  lea                predq, [predq+pred_str*2]
  sub                rowsd, 2
  jg .loop_4
  emms
  RET
